//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**  Copyright (C) 2018-2021 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, version 3 of the License ONLY.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**  You should have received a copy of the GNU General Public License
//**  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//**
//**************************************************************************
#include "../gamedefs.h"
#include "r_local.h"


// ////////////////////////////////////////////////////////////////////////// //
#define ON_EPSILON      (0.1f)
#define SUBDIVIDE_SIZE  (240)

#define EXTMAX  (32767)
//#define EXTMAX  (65536)
// float mantissa is 24 bits, but let's play safe, and use 20 bits
//#define EXTMAX  (0x100000)


// ////////////////////////////////////////////////////////////////////////// //
VCvarB r_precalc_static_lights("r_precalc_static_lights", true, "Precalculate static lights?", CVAR_Archive);
int r_precalc_static_lights_override = -1; // <0: not set

extern VCvarB r_lmap_recalc_static;
extern VCvarB r_lmap_bsp_trace_static;


// ////////////////////////////////////////////////////////////////////////// //
extern int light_mem;


// ////////////////////////////////////////////////////////////////////////// //
// pool allocator for split vertices
// ////////////////////////////////////////////////////////////////////////// //
static float *spvPoolDots = nullptr;
static int *spvPoolSides = nullptr;
static SurfVertex *spvPoolV1 = nullptr;
static SurfVertex *spvPoolV2 = nullptr;
static int spvPoolSize = 0;


static inline void spvReserve (int size) {
  if (size < 1) size = 1;
  size = (size|0xfff)+1;
  if (spvPoolSize < size) {
    spvPoolSize = size;
    spvPoolDots = (float *)Z_Realloc(spvPoolDots, spvPoolSize*sizeof(spvPoolDots[0])); if (!spvPoolDots) Sys_Error("OOM!");
    spvPoolSides = (int *)Z_Realloc(spvPoolSides, spvPoolSize*sizeof(spvPoolSides[0])); if (!spvPoolSides) Sys_Error("OOM!");
    spvPoolV1 = (SurfVertex *)Z_Realloc(spvPoolV1, spvPoolSize*sizeof(spvPoolV1[0])); if (!spvPoolV1) Sys_Error("OOM!");
    spvPoolV2 = (SurfVertex *)Z_Realloc(spvPoolV2, spvPoolSize*sizeof(spvPoolV2[0])); if (!spvPoolV2) Sys_Error("OOM!");
  }
}


//==========================================================================
//
//  CalcSurfMinMax
//
//  surface must be valid
//
//==========================================================================
static bool CalcSurfMinMax (surface_t *surf, float &outmins, float &outmaxs, const TVec axis/*, const float offs=0.0f*/) {
  float mins = +99999.0f;
  float maxs = -99999.0f;
  const SurfVertex *vt = surf->verts;
  for (int i = surf->count; i--; ++vt) {
    if (!vt->vec().isValid()) {
      GCon->Log(NAME_Warning, "ERROR(SF): invalid surface vertex; THIS IS INTERNAL K8VAVOOM BUG!");
      surf->count = 0;
      outmins = outmaxs = 0.0f;
      return false;
    }
    // ignore offset, we don't need it anymore
    //const float dot = DotProduct(*vt, axis)+offs;
    const float dot = DotProduct(vt->vec(), axis);
    if (dot < mins) mins = dot;
    if (dot > maxs) maxs = dot;
  }
  // always zero-based (why?)
  //maxs -= mins;
  //mins = 0;
  outmins = clampval(mins, -32767.0f, 32767.0f);
  outmaxs = clampval(maxs, -32767.0f, 32767.0f);
  return true;
}


//==========================================================================
//
//  VRenderLevelLightmap::InitSurfs
//
//==========================================================================
void VRenderLevelLightmap::InitSurfs (bool recalcStaticLightmaps, surface_t *ASurfs, texinfo_t *texinfo, const TPlane *plane, subsector_t *sub) {
  bool doPrecalc = (r_precalc_static_lights_override >= 0 ? !!r_precalc_static_lights_override : r_precalc_static_lights);

  for (surface_t *surf = ASurfs; surf; surf = surf->next) {
    if (texinfo) surf->texinfo = texinfo;
    if (plane) surf->plane = *plane;

    if (surf->count == 0) {
      GCon->Logf(NAME_Warning, "empty surface at subsector #%d", (int)(ptrdiff_t)(sub-Level->Subsectors));
      surf->texturemins[0] = 16;
      surf->extents[0] = 16;
      surf->texturemins[1] = 16;
      surf->extents[1] = 16;
      surf->subsector = sub;
      surf->drawflags &= ~surface_t::DF_CALC_LMAP; // just in case
    } else if (surf->count < 3) {
      GCon->Logf(NAME_Warning, "degenerate surface with #%d vertices at subsector #%d", surf->count, (int)(ptrdiff_t)(sub-Level->Subsectors));
      surf->texturemins[0] = 16;
      surf->extents[0] = 16;
      surf->texturemins[1] = 16;
      surf->extents[1] = 16;
      surf->subsector = sub;
      surf->drawflags &= ~surface_t::DF_CALC_LMAP; // just in case
    } else {
      /*short*/int old_texturemins[2];
      /*short*/int old_extents[2];

      // to do checking later
      old_texturemins[0] = surf->texturemins[0];
      old_texturemins[1] = surf->texturemins[1];
      old_extents[0] = surf->extents[0];
      old_extents[1] = surf->extents[1];

      float mins, maxs;

      if (!CalcSurfMinMax(surf, mins, maxs, texinfo->saxisLM/*, texinfo->soffs*/)) {
        // bad surface
        surf->drawflags &= ~surface_t::DF_CALC_LMAP; // just in case
        continue;
      }
      int bmins = (int)floorf(mins/16.0f);
      int bmaxs = (int)ceilf(maxs/16.0f);

      if (bmins < -EXTMAX/16 || bmins > EXTMAX/16 ||
          bmaxs < -EXTMAX/16 || bmaxs > EXTMAX/16 ||
          (bmaxs-bmins) < -EXTMAX/16 ||
          (bmaxs-bmins) > EXTMAX/16)
      {
        GCon->Logf(NAME_Warning, "Subsector %d got too big S surface extents: (%d,%d)", (int)(ptrdiff_t)(sub-Level->Subsectors), bmins, bmaxs);
        surf->texturemins[0] = 0;
        surf->extents[0] = 256;
      } else {
        surf->texturemins[0] = bmins*16;
        surf->extents[0] = (bmaxs-bmins)*16;
      }

      if (!CalcSurfMinMax(surf, mins, maxs, texinfo->taxisLM/*, texinfo->toffs*/)) {
        // bad surface
        surf->drawflags &= ~surface_t::DF_CALC_LMAP; // just in case
        continue;
      }
      bmins = (int)floorf(mins/16.0f);
      bmaxs = (int)ceilf(maxs/16.0f);

      if (bmins < -EXTMAX/16 || bmins > EXTMAX/16 ||
          bmaxs < -EXTMAX/16 || bmaxs > EXTMAX/16 ||
          (bmaxs-bmins) < -EXTMAX/16 ||
          (bmaxs-bmins) > EXTMAX/16)
      {
        GCon->Logf(NAME_Warning, "Subsector %d got too big T surface extents: (%d,%d)", (int)(ptrdiff_t)(sub-Level->Subsectors), bmins, bmaxs);
        surf->texturemins[1] = 0;
        surf->extents[1] = 256;
        //GCon->Logf("AXIS=(%g,%g,%g)", texinfo->taxis.x, texinfo->taxis.y, texinfo->taxis.z);
      } else {
        surf->texturemins[1] = bmins*16;
        surf->extents[1] = (bmaxs-bmins)*16;
        //GCon->Logf("AXIS=(%g,%g,%g)", texinfo->taxis.x, texinfo->taxis.y, texinfo->taxis.z);
      }

      // reset surface cache only if something was changed
      bool minMaxChanged =
        recalcStaticLightmaps ||
        old_texturemins[0] != surf->texturemins[0] ||
        old_texturemins[1] != surf->texturemins[1] ||
        old_extents[0] != surf->extents[0] ||
        old_extents[1] != surf->extents[1];

      if (minMaxChanged) FlushSurfCaches(surf);

      vassert(surf->subsector == sub);
      if (inWorldCreation && doPrecalc) {
        //LightFace(surf); // we'll do it later
        surf->drawflags |= surface_t::DF_CALC_LMAP;
      } else {
        // mark this surface for static lightmap recalc
        if ((recalcStaticLightmaps && r_lmap_recalc_static) || inWorldCreation) {
          surf->drawflags |= surface_t::DF_CALC_LMAP;
          InvalidateStaticLightmapsSubs(surf->subsector);
        }
      }
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
struct SClipInfo {
  int vcount[2];
  SurfVertex *verts[2];
};


//==========================================================================
//
//  SplitSurface
//
//  returns `false` if surface cannot be split
//  axis must be valid
//
//==========================================================================
static bool SplitSurface (SClipInfo &clip, surface_t *surf, const TVec &axis) {
  clip.vcount[0] = clip.vcount[1] = 0;
  if (!surf || surf->count < 3) return false; // cannot split

  float mins, maxs;
  if (!CalcSurfMinMax(surf, mins, maxs, axis)) {
    // invalid surface
    surf->count = 0;
    return false;
  }

  if (maxs-mins <= SUBDIVIDE_SIZE) return false;

  TPlane plane;
  plane.normal = axis;
  const float dot0 = Length(plane.normal);
  plane.normal.normaliseInPlace();
  plane.dist = (mins+SUBDIVIDE_SIZE-16)/dot0;

  enum {
    PlaneBack = -1,
    PlaneCoplanar = 0,
    PlaneFront = 1,
  };

  const int surfcount = surf->count;
  spvReserve(surfcount*2+2); //k8: `surf->count+1` is enough, but...

  float *dots = spvPoolDots;
  int *sides = spvPoolSides;
  SurfVertex *verts1 = spvPoolV1;
  SurfVertex *verts2 = spvPoolV2;

  const SurfVertex *vt = surf->verts;

  int backSideCount = 0, frontSideCount = 0;
  for (int i = 0; i < surfcount; ++i, ++vt) {
    //const float dot = DotProduct(vt->vec(), plane.normal)-plane.dist;
    const float dot = plane.PointDistance(vt->vec());
    dots[i] = dot;
         if (dot < -ON_EPSILON) { ++backSideCount; sides[i] = PlaneBack; }
    else if (dot > ON_EPSILON) { ++frontSideCount; sides[i] = PlaneFront; }
    else sides[i] = PlaneCoplanar;
  }
  dots[surfcount] = dots[0];
  sides[surfcount] = sides[0];

  // completely on one side?
  if (!backSideCount || !frontSideCount) return false;

  TVec mid(0, 0, 0);
  clip.verts[0] = verts1;
  clip.verts[1] = verts2;

  vt = surf->verts;
  for (int i = 0; i < surfcount; ++i) {
    if (sides[i] == PlaneCoplanar) {
      clip.verts[0][clip.vcount[0]++] = vt[i];
      clip.verts[1][clip.vcount[1]++] = vt[i];
      continue;
    }

    unsigned cvidx = (sides[i] == PlaneFront ? 0 : 1);
    clip.verts[cvidx][clip.vcount[cvidx]++] = vt[i];

    if (sides[i+1] == PlaneCoplanar || sides[i] == sides[i+1]) continue;

    // generate a split point
    const TVec &p1 = vt[i].vec();
    const TVec &p2 = vt[(i+1)%surfcount].vec();

    const float dot = dots[i]/(dots[i]-dots[i+1]);
    for (int j = 0; j < 3; ++j) {
      // avoid round off error when possible
           if (plane.normal[j] == 1.0f) mid[j] = plane.dist;
      else if (plane.normal[j] == -1.0f) mid[j] = -plane.dist;
      else mid[j] = p1[j]+dot*(p2[j]-p1[j]);
    }

    clip.verts[0][clip.vcount[0]++].setVec(mid);
    clip.verts[1][clip.vcount[1]++].setVec(mid);
  }

  return (clip.vcount[0] >= 3 && clip.vcount[1] >= 3);
}


//==========================================================================
//
//  VRenderLevelLightmap::SubdivideFaceInternal
//
//==========================================================================
surface_t *VRenderLevelLightmap::SubdivideFaceInternal (surface_t *surf, const TVec &axis, const TVec *nextaxis, const TPlane *plane) {
  subsector_t *sub = surf->subsector;
  vassert(sub);

  if (plane) surf->plane = *plane;

  if (surf->count < 2) {
    //Sys_Error("surface with less than three (%d) vertices)", f->count);
    GCon->Logf(NAME_Warning, "surface with less than two (%d) vertices (divface) (sub=%d; sector=%d)", surf->count, (int)(ptrdiff_t)(sub-Level->Subsectors), (int)(ptrdiff_t)(sub->sector-Level->Sectors));
    return surf;
  }

  // this can happen for wall without texture
  if (!axis.isValid() || axis.isZero()) {
    GCon->Logf(NAME_Warning, "ERROR(SF): invalid axis (%f,%f,%f); THIS IS MAP BUG! (sub=%d; sector=%d)", axis.x, axis.y, axis.z, (int)(ptrdiff_t)(sub-Level->Subsectors), (int)(ptrdiff_t)(sub->sector-Level->Sectors));
    return (nextaxis ? SubdivideFaceInternal(surf, *nextaxis, nullptr, plane) : surf);
  }

  SClipInfo clip;
  if (!SplitSurface(clip, surf, axis)) {
    // cannot subdivide, try next axis
    return (nextaxis ? SubdivideFaceInternal(surf, *nextaxis, nullptr, plane) : surf);
  }

  vassert(clip.vcount[0] > 2);
  vassert(clip.vcount[1] > 2);

  ++c_subdivides;

  surface_t *back = NewWSurf(clip.vcount[1]);
  back->copyRequiredFrom(*surf);
  back->count = clip.vcount[1];
  if (back->count) memcpy((void *)back->verts, clip.verts[1], back->count*sizeof(SurfVertex));
  if (plane) back->plane = *plane;

  surface_t *front = NewWSurf(clip.vcount[0]);
  front->copyRequiredFrom(*surf);
  front->count = clip.vcount[0];
  if (front->count) memcpy((void *)front->verts, clip.verts[0], front->count*sizeof(SurfVertex));
  if (plane) front->plane = *plane;

  front->next = surf->next;

  surf->next = nullptr;
  FreeWSurfs(surf);

  back->next = SubdivideFaceInternal(front, axis, nextaxis, plane);
  return (nextaxis ? SubdivideFaceInternal(back, *nextaxis, nullptr, plane) : back);
}


/*
  subdivision t-junctions fixer

  loop over all surfaces; for each surface point, check if it touches
  any surfaces from the adjacent subsectors. if it is, insert that point there

  currently we're doing this by brute force
*/


//==========================================================================
//
//  IsPointOnLine
//
//==========================================================================
static inline bool IsPointOnLine (const TVec &v0, const TVec &v1, const TVec &p) noexcept {
  if (v0.x == v1.x) {
    // vertical line
    if (fabsf(p.x-v0.x) > 0.001f) return false;
    return (v0.y < v1.y ? (p.y >= v0.y && p.y <= v1.y) : (p.y >= v1.y && p.y <= v0.y));
  } else if (v0.y == v1.y) {
    // horizontal line
    if (fabsf(p.y-v0.y) > 0.001f) return false;
    return (v0.x < v1.x ? (p.x >= v0.x && p.x <= v1.x) : (p.x >= v1.x && p.x <= v0.x));
  } else {
    // bounding box check
    if (v0.x < v1.x) {
      if (p.x < v0.x || p.x > v1.x) return false;
    } else {
      if (p.x < v1.x || p.x > v0.x) return false;
    }

    if (v0.y < v1.y) {
      if (p.y < v0.y || p.y > v1.y) return false;
    } else {
      if (p.y < v1.y || p.y > v0.y) return false;
    }

    // diagonal line with valid bounding box
    const float dx = v1.x-v0.x;
    const float dy = v1.y-v0.y;

    const float slope = dy/dx;

    // y = mx + c
    // intercept c = y - mx
    const float intercept = v0.y-slope*v0.x; // which is same as (v1.y-slope*v1.x)

    const float spy = slope*p.x+intercept;
    return (fabsf(spy-p.y) <= 0.001f); // y position should match
  }
}


//==========================================================================
//
//  surfDrop
//
//==========================================================================
static VVA_OKUNUSED surface_t *surfDrop (surface_t *surfs, int idx) noexcept {
  surface_t *prev = nullptr;
  surface_t *curr = surfs;
  while (curr) {
    if (idx-- == 0) {
      if (prev) prev->next = curr->next; else surfs = curr->next;
      break;
    }
    prev = curr;
    curr = curr->next;
  }
  return surfs;
}


//==========================================================================
//
//  surfIndex
//
//==========================================================================
static VVA_OKUNUSED int surfIndex (const surface_t *surfs, const surface_t *curr) noexcept {
  if (!curr) return -1;
  int res = 0;
  while (surfs && surfs != curr) { ++res; surfs = surfs->next; }
  return (surfs == curr ? res : -666);
}


//==========================================================================
//
//  FlatSurfaceInsertPoint
//
//  do not use reference to `p` here!
//  it may be a vector from some source that can be modified
//
//==========================================================================
static surface_t *FlatSurfaceInsertPoint (VRenderLevelShared *RLev, subsector_t *ownsub, surface_t *surf, surface_t *&surfhead, surface_t *prev, const TVec p) {
  if (!surf || surf->count < 3 || fabsf(surf->plane.PointDistance(p)) >= 0.01f) return surf;
  //surface_t *prev = nullptr;
  #if 0
  VLevel *Level = RLev->GetLevel();
  const int sfidx = surfIndex(surfhead, surf);
  #endif
  // check each surface line
  for (int pn0 = (int)surf->isCentroidCreated(); pn0 < surf->count-(int)surf->isCentroidCreated(); ++pn0) {
    int pn1 = (pn0+1)%surf->count;
    if (surf->isCentroidCreated()) vassert(pn1 != 0);
    TVec v0 = TVec(surf->verts[pn0].x, surf->verts[pn0].y);
    TVec v1 = TVec(surf->verts[pn1].x, surf->verts[pn1].y);
    if ((v1-v0).length2DSquared() < 1.0f) continue; // ignore, it is too small
    // check corners
    if ((v0-p).length2DSquared() < 1.0f || (v1-p).length2DSquared() < 1.0f) continue;
    #if 0
    GCon->Logf(NAME_Debug, "surface #%d : %p for subsector #%d: checking point; line=(%g,%g,%g)-(%g,%g,%g); plane=(%g,%g,%g):%g; point=(%g,%g,%g); online=%d",
      sfidx, surf, (int)(ptrdiff_t)(surf->subsector-&Level->Subsectors[0]),
      v0.x, v0.y, v0.z, v1.x, v1.y, v1.z,
      surf->plane.normal.x, surf->plane.normal.y, surf->plane.normal.z, surf->plane.dist,
      p.x, p.y, p.z, (int)IsPointOnLine(v0, v1, p));
    #endif
    if (!IsPointOnLine(v0, v1, p)) continue;
    // check height (just in case)
    const float nz = surf->plane.GetPointZ(p);
    if (fabsf(nz-p.z) > 0.001f) continue;
    #if 0
    GCon->Logf(NAME_Debug, "surface #%d : %p for subsector #%d need a new point; line=(%g,%g,%g)-(%g,%g,%g); plane=(%g,%g,%g):%g; orgpoint=(%g,%g,%g)",
      sfidx, surf, (int)(ptrdiff_t)(surf->subsector-&Level->Subsectors[0]),
      v0.x, v0.y, v0.z, v1.x, v1.y, v1.z,
      surf->plane.normal.x, surf->plane.normal.y, surf->plane.normal.z, surf->plane.dist,
      p.x, p.y, p.z);
    #endif
    // insert a new point
    if (!prev && surf != surfhead) {
      prev = surfhead;
      while (prev->next != surf) prev = prev->next;
    }
    surf = RLev->EnsureSurfacePoints(surf, surf->count+(surf->isCentroidCreated() ? 1 : 3), surfhead, prev);
    // create centroid
    if (!surf->isCentroidCreated()) {
      surf->AddCentroidFlat();
      ++pn0;
    }
    // insert point
    surf->InsertVertexAt(pn0+1, ownsub, p);
    // the point cannot be inserted into several lines,
    // so we're finished with this surface
    break;
  }

  return surf;
}


//==========================================================================
//
//  FlatSurfacesInsertPoint
//
//  do not pass surface vertices as `p`!
//  it may be a vertex from some source that can be modified
//
//==========================================================================
static void FlatSurfacesInsertPoint (VRenderLevelShared *RLev, subsector_t *ownsub, surface_t *&surfhead, const TVec &p) {
  surface_t *surf = surfhead;
  surface_t *prev = nullptr;
  while (surf) {
    surf = FlatSurfaceInsertPoint(RLev, ownsub, surf, surfhead, prev, p);
    prev = surf;
    surf = surf->next;
  }
}


//==========================================================================
//
//  FixSecSurface
//
//  do not pass surface vertices as `p`!
//  it may be a vertex from some source that can be modified
//
//==========================================================================
static inline void FixSecSurface (VRenderLevelShared *RLev, subsector_t *ownsub, sec_surface_t *secsurf, const TVec &p) {
  if (secsurf && secsurf->surfs) FlatSurfacesInsertPoint(RLev, ownsub, secsurf->surfs, p);
}


//==========================================================================
//
//  FixOwnSecSurface
//
//  fixes one surface
//  must be called before subdivisions
//
//==========================================================================
static surface_t *FixOwnSecSurface (VRenderLevelShared *RLev, subsector_t *ownsub, surface_t *surf, sec_surface_t *secsfc) {
  for (const surface_t *srcsurf = (secsfc ? secsfc->surfs : nullptr); srcsurf; srcsurf = srcsurf->next) {
    for (int pn0 = (int)srcsurf->isCentroidCreated(); pn0 < srcsurf->count-(int)srcsurf->isCentroidCreated(); ++pn0) {
      surf = FlatSurfaceInsertPoint(RLev, ownsub, surf, surf, nullptr, srcsurf->verts[pn0].vec());
    }
  }
  return surf;
}


//==========================================================================
//
//  FlatInsertPointsFromAllSurfaces
//
//  used to insert points from wall surfaces
//
//==========================================================================
static void FlatInsertPointsFromAllSurfaces (VRenderLevelShared *RLev, subsector_t *ownsub, surface_t *slist, surface_t *&surfhead) {
  for (; slist; slist = slist->next) {
    for (int spn = (int)slist->isCentroidCreated(); spn < slist->count-(int)slist->isCentroidCreated(); ++spn) {
      FlatSurfacesInsertPoint(RLev, ownsub, surfhead, slist->verts[spn].vec());
    }
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::SubdivideFace
//
//==========================================================================
surface_t *VRenderLevelLightmap::SubdivideFace (surface_t *surf, const TVec &axis, const TVec *nextaxis, const TPlane *plane, bool doSubdivisions) {
  if (!surf) return surf;

  subsector_t *sub = surf->subsector;
  vassert(sub);

  if (surf->count < 3 || sub->isOriginalPObj()) return surf; // nobody cares

  vassert(!surf->isCentroidCreated());

  // add points from the adjacent subsectors to this one
  if (lastRenderQuality) {
    seg_t *seg = &Level->Segs[sub->firstline];
    for (int f = sub->numlines; f--; ++seg) {
      vassert(seg->frontsub == sub);
      seg_t *partner = seg->partner;
      if (!partner) continue; // one-sided seg
      subsector_t *psub = partner->frontsub;
      if (psub == sub) continue; // just in case
      // check floor and ceiling
      for (subregion_t *region = psub->regions; region; region = region->next) {
        surf = FixOwnSecSurface(this, psub, surf, region->realfloor);
        surf = FixOwnSecSurface(this, psub, surf, region->realceil);
        surf = FixOwnSecSurface(this, psub, surf, region->fakefloor);
        surf = FixOwnSecSurface(this, psub, surf, region->fakeceil);
      }
    }
  }

  if (doSubdivisions) {
    //GCon->Logf(NAME_Debug, "removing centroid from default %s surface of subsector #%d (vcount=%d; hasc=%d)", (plane->isFloor() ? "floor" : "ceiling"), (int)(ptrdiff_t)(sub-&Level->Subsectors[0]), surf->count, surf->isCentroidCreated());
    // we'll re-add it later
    surf->RemoveCentroid();

    surf = SubdivideFaceInternal(surf, axis, nextaxis, plane);
    vassert(surf);

    // always create centroids for complex surfaces
    // this is required to avoid omiting some triangles in renderer (which can cause t-junctions)
    {
      surface_t *prev = nullptr;
      for (surface_t *ss = surf; ss; prev = ss, ss = ss->next) {
        if (ss->count > 4 && !ss->isCentroidCreated()) {
          // make room
          ss = EnsureSurfacePoints(ss, surf->count+2, surf, prev);
          // add insert centroid
          ss->AddCentroidFlat();
        }
      }
    }

    if (!lastRenderQuality /*|| !surf->next*/) return surf; // no subdivisions --> nothing to do

    // fix splitted surfaces
    // this is stupid brute-force approach, but i may do something with it later
    if (surf->next) {
      // surface to check
      surface_t *prev = nullptr;
      for (surface_t *sfcheck = surf; sfcheck; prev = sfcheck, sfcheck = sfcheck->next) {
        #if 0
        GCon->Logf(NAME_Debug, "==== FIXING SURFACE #%d : %p (%d) ===", surfIndex(surf, sfcheck), sfcheck, sfcheck->isCentroidCreated());
        for (int pn0 = (int)sfcheck->isCentroidCreated(); pn0 < sfcheck->count-(int)sfcheck->isCentroidCreated(); ++pn0) {
          GCon->Logf("  pt #%2d/%2d: (%g,%g,%g)", pn0, sfcheck->count, sfcheck->verts[pn0].x, sfcheck->verts[pn0].y, sfcheck->verts[pn0].z);
        }
        #endif
        // check with other surfaces' points
        for (surface_t *sfother = surf; sfother; sfother = sfother->next) {
          if (sfcheck == sfother) continue;
          for (int spn = (int)sfother->isCentroidCreated(); spn < sfother->count-(int)sfother->isCentroidCreated(); ++spn) {
            #if 0
            GCon->Logf(NAME_Debug, "  checker surface #%d : %p, point #%d: (%g,%g,%g)", surfIndex(surf, sfother), sfother, spn, sfother->verts[spn].x, sfother->verts[spn].y, sfother->verts[spn].z);
            #endif
            sfcheck = FlatSurfaceInsertPoint(this, nullptr, sfcheck, surf, prev, sfother->verts[spn].vec());
          }
        }
      }
    }

    // append points from wall surfaces
    seg_t *seg = &Level->Segs[sub->firstline];
    for (int f = sub->numlines; f--; ++seg) {
      drawseg_t *ds = seg->drawsegs;
      if (!ds) continue;
      if (ds->top) FlatInsertPointsFromAllSurfaces(this, sub, ds->top->surfs, surf);
      if (ds->mid) FlatInsertPointsFromAllSurfaces(this, sub, ds->mid->surfs, surf);
      if (ds->bot) FlatInsertPointsFromAllSurfaces(this, sub, ds->bot->surfs, surf);
      if (ds->topsky) FlatInsertPointsFromAllSurfaces(this, sub, ds->topsky->surfs, surf);
      //if (ds->extra) FlatInsertPointsFromAllSurfaces(this, sub, ds->extra->surfs, surf);
      FlatInsertPointsFromAllSurfaces(this, sub, ds->HorizonTop, surf);
      FlatInsertPointsFromAllSurfaces(this, sub, ds->HorizonBot, surf);
    }
  } else {
    // always create centroids for complex surfaces
    // this is required to avoid omiting some triangles in renderer (which can cause t-junctions)
    {
      surface_t *prev = nullptr;
      for (surface_t *ss = surf; ss; prev = ss, ss = ss->next) {
        if (ss->count > 4 && !ss->isCentroidCreated()) {
          // make room
          ss = EnsureSurfacePoints(ss, surf->count+2, surf, prev);
          // add insert centroid
          ss->AddCentroidFlat();
          vassert(ss->isCentroidCreated());
        }
      }
    }
    if (!lastRenderQuality) return surf;
  }

  // do not fix polyobjects, or invalid subsectors (yet)
  if (sub->isAnyPObj() || sub->numlines < 1) return surf;

  /*
  if ((ptrdiff_t)(sub-&Level->Subsectors[0]) != 2) return surf;
  #if 1
  //surf = surf->next;
  surf->next = nullptr;
  return surf;
  #endif
  */

  // fix adjacent subsectors
  //GCon->Logf(NAME_Debug, "=== checking subdivided flats for source subsector #%d ===", (int)(ptrdiff_t)(sub-&Level->Subsectors[0]));
  for (surface_t *ss = surf; ss; ss = ss->next) {
    for (int spn = (int)ss->isCentroidCreated(); spn < ss->count-(int)ss->isCentroidCreated(); ++spn) {
      TVec p(ss->verts[spn].vec());
      seg_t *seg = &Level->Segs[sub->firstline];
      for (int f = sub->numlines; f--; ++seg) {
        vassert(seg->frontsub == sub);
        seg_t *partner = seg->partner;
        if (!partner) continue; // one-sided seg
        subsector_t *psub = partner->frontsub;
        if (psub == sub) continue; // just in case
        //if ((ptrdiff_t)(psub-&Level->Subsectors[0]) != 2) continue;
        // check floor and ceiling
        for (subregion_t *region = psub->regions; region; region = region->next) {
          FixSecSurface(this, sub, region->realfloor, p);
          FixSecSurface(this, sub, region->realceil, p);
          FixSecSurface(this, sub, region->fakefloor, p);
          FixSecSurface(this, sub, region->fakeceil, p);
        }
      }
    }
  }
  //GCon->Logf(NAME_Debug, "=== DONE checking subdivided flats for source subsector #%d ===", (int)(ptrdiff_t)(sub-&Level->Subsectors[0]));

  return surf;
}


//==========================================================================
//
//  VRenderLevelLightmap::SubdivideSegInternal
//
//==========================================================================
surface_t *VRenderLevelLightmap::SubdivideSegInternal (surface_t *surf, const TVec &axis, const TVec *nextaxis, seg_t *seg) {
  subsector_t *sub = surf->subsector;
  //vassert(surf->seg == seg);
  vassert(sub);

  if (seg) surf->plane = *seg;

  if (surf->count < 2) {
    //Sys_Error("surface with less than three (%d) vertices)", surf->count);
    GCon->Logf(NAME_Warning, "surface with less than two (%d) vertices (divseg) (sub=%d; sector=%d)", surf->count, (int)(ptrdiff_t)(sub-Level->Subsectors), (int)(ptrdiff_t)(sub->sector-Level->Sectors));
    return surf;
  }

  // this can happen for wall without texture
  if (!axis.isValid() || axis.isZero()) {
    GCon->Logf(NAME_Warning, "ERROR(SS): invalid axis (%f,%f,%f); THIS IS MAP BUG! (sub=%d; sector=%d)", axis.x, axis.y, axis.z, (int)(ptrdiff_t)(sub-Level->Subsectors), (int)(ptrdiff_t)(sub->sector-Level->Sectors));
    return (nextaxis ? SubdivideSegInternal(surf, *nextaxis, nullptr, seg) : surf);
  }

  SClipInfo clip;
  if (!SplitSurface(clip, surf, axis)) {
    // cannot subdivide, try next axis
    return (nextaxis ? SubdivideSegInternal(surf, *nextaxis, nullptr, seg) : surf);
  }

  vassert(clip.vcount[0] > 2);
  vassert(clip.vcount[1] > 2);

  ++c_seg_div;

  vassert(clip.vcount[1] <= surface_t::MAXWVERTS);
  surf->count = clip.vcount[1];
  if (surf->count) memcpy((void *)surf->verts, clip.verts[1], surf->count*sizeof(SurfVertex));

  surface_t *news = NewWSurf(clip.vcount[0]);
  news->copyRequiredFrom(*surf);
  //news->drawflags = surf->drawflags;
  //news->typeFlags = surf->typeFlags;
  //news->subsector = sub;
  //news->seg = seg;
  news->count = clip.vcount[0];
  if (news->count) memcpy((void *)news->verts, clip.verts[0], news->count*sizeof(SurfVertex));
  if (seg) news->plane = *seg;

  news->next = surf->next;
  surf->next = SubdivideSegInternal(news, axis, nextaxis, seg);
  if (nextaxis) return SubdivideSegInternal(surf, *nextaxis, nullptr, seg);
  return surf;
}


//==========================================================================
//
//  VRenderLevelLightmap::SubdivideSeg
//
//==========================================================================
surface_t *VRenderLevelLightmap::SubdivideSeg (surface_t *surf, const TVec &axis, const TVec *nextaxis, seg_t *seg) {
  return SubdivideSegInternal(surf, axis, nextaxis, seg);
}


//==========================================================================
//
//  VRenderLevelLightmap::FixSegSurfaceTJunctions
//
//==========================================================================
surface_t *VRenderLevelLightmap::FixSegSurfaceTJunctions (surface_t *surf, seg_t *seg) {
  return surf;
}
