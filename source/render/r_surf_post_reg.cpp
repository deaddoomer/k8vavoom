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


//==========================================================================
//
//  spvReserve
//
//==========================================================================
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
    //const float dot = axis.dot(*vt)+offs;
    const float dot = axis.dot(vt->vec());
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


// ////////////////////////////////////////////////////////////////////////// //
struct SClipInfo {
  int vcount[2];
  SurfVertex *verts[2];

  inline void appendVertex (const int pidx, const SurfVertex &v) noexcept {
    verts[pidx][vcount[pidx]++] = v;
  }

  inline void appendPoint (const int pidx, const TVec p) noexcept {
    verts[pidx][vcount[pidx]++].clearSetVec(p);
  }
};


//==========================================================================
//
//  SplitSurface
//
//  returns `false` if surface cannot be split
//  axis must be valid
//
//  WARNING! thread-unsafe!
//  WARNING! returned vertices memory in `clip` will be reused!
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
  const float dot0 = plane.normal.length();
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

  bool hasFrontSomething = false;
  bool hasBackSomething = false;
  for (int i = 0; i < surfcount; ++i, ++vt) {
    //const float dot = plane.normal.dot(vt->vec())-plane.dist;
    const float dot = plane.PointDistance(vt->vec());
    dots[i] = dot;
         if (dot < -ON_EPSILON) { sides[i] = PlaneBack; hasBackSomething = true; }
    else if (dot > +ON_EPSILON) { sides[i] = PlaneFront; hasFrontSomething = true; }
    else sides[i] = PlaneCoplanar;
  }

  if (!hasBackSomething || !hasFrontSomething) {
    // totally coplanar, or completely on one side
    return false;
  }

  dots[surfcount] = dots[0];
  sides[surfcount] = sides[0];

  TVec mid(0, 0, 0);
  clip.verts[0] = verts1;
  clip.verts[1] = verts2;

  vt = surf->verts;
  for (int i = 0; i < surfcount; ++i) {
    if (sides[i] == PlaneCoplanar) {
      clip.appendVertex(0, vt[i]);
      clip.appendVertex(1, vt[i]);
      continue;
    }

    clip.appendVertex((sides[i] == PlaneBack), vt[i]);

    if (sides[i+1] == PlaneCoplanar || sides[i] == sides[i+1]) continue;

    // generate a split point
    const TVec &p1 = vt[i].vec();
    const TVec &p2 = vt[(i+1)%surfcount].vec();

    const float dot = dots[i]/(dots[i]-dots[i+1]);
    for (int j = 0; j < 3; ++j) {
      // avoid round off error when possible
           if (plane.normal[j] == +1.0f) mid[j] = plane.dist;
      else if (plane.normal[j] == -1.0f) mid[j] = -plane.dist;
      else mid[j] = p1[j]+dot*(p2[j]-p1[j]);
    }

    clip.appendPoint(0, mid);
    clip.appendPoint(1, mid);
  }

  return (clip.vcount[0] >= 3 && clip.vcount[1] >= 3);
}


//==========================================================================
//
//  surfDrop
//
//==========================================================================
/*
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
*/


//==========================================================================
//
//  surfIndex
//
//==========================================================================
/*
static VVA_OKUNUSED int surfIndex (const surface_t *surfs, const surface_t *curr) noexcept {
  if (!curr) return -1;
  int res = 0;
  while (surfs && surfs != curr) { ++res; surfs = surfs->next; }
  return (surfs == curr ? res : -666);
}
*/


//==========================================================================
//
//  RemoveCentroids
//
//  always create centroids for complex surfaces
//  this is required to avoid omiting some triangles
//  in renderer (which can cause t-junctions)
//
//==========================================================================
static surface_t *RemoveCentroids (surface_t *surf) {
  for (surface_t *ss = surf; ss; ss = ss->next) {
    if (ss->isCentroidCreated()) ss->RemoveCentroid();
  }
  return surf;
}


//==========================================================================
//
//  RecreateCentroids
//
//  always create centroids for complex surfaces
//  this is required to avoid omiting some triangles
//  in renderer (which can cause t-junctions)
//
//==========================================================================
static surface_t *RecreateCentroids (VRenderLevelShared *RLev, surface_t *surf) {
  surface_t *prev = nullptr;
  for (surface_t *ss = surf; ss; prev = ss, ss = ss->next) {
    if (ss->count > 4 && !ss->isCentroidCreated()) {
      // make room
      ss = RLev->EnsureSurfacePoints(ss, ss->count+2, surf, prev);
      // add insert centroid
      ss->AddCentroid();
    }
  }
  return surf;
}


//==========================================================================
//
//  VRenderLevelLightmap::InitSurfs
//
//==========================================================================
void VRenderLevelLightmap::InitSurfs (bool recalcStaticLightmaps, surface_t *ASurfs, texinfo_t *texinfo, const TPlane *plane, subsector_t *sub, seg_t *seg, subregion_t *sreg) {
  bool doPrecalc = (r_precalc_static_lights_override >= 0 ? !!r_precalc_static_lights_override : r_precalc_static_lights);

  for (surface_t *surf = ASurfs; surf; surf = surf->next) {
    if (texinfo) surf->texinfo = texinfo;
    if (plane) surf->plane = *plane;
    surf->subsector = sub;
    surf->seg = seg;
    surf->sreg = sreg;

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
//  IsPointOnLine3D
//
//==========================================================================
static inline bool IsPointOnLine3D (const TVec &v0, const TVec &v1, const TVec &p) noexcept {
  const TVec ldir = v1-v0;
  const float llensq = ldir.lengthSquared();
  if (llensq < 1.0f) return false; // dot
  // check for corners
  const TVec p0 = p-v0;
  if (p0.lengthSquared() < 1.0f) return false;
  const TVec p1 = p-v1;
  if (p1.lengthSquared() < 1.0f) return false;
  const float llen = sqrtf(llensq);
  // check projection
  const TVec ndir = ldir/llen;
  const float prj = ndir.dot(p0);
  if (prj < 1.0f || prj > llen-1.0f) return false; // projection is too big
  // check point distance
  const float dist = p0.cross(p1).length()/llen;
  //const float dist = p0.cross(p1).lengthSquared()/llensq;
  return (dist < 0.001f);
}


//==========================================================================
//
//  SurfaceInsertPointIntoEdge
//
//  do not use reference to `p` here!
//  it may be a vector from some source that can be modified
//
//==========================================================================
static surface_t *SurfaceInsertPointIntoEdge (VRenderLevelShared *RLev, sec_surface_t *ownssf, seg_t *ownseg, surface_t *surf, surface_t *&surfhead, surface_t *prev, const TVec p) {
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
    const TVec v0 = surf->verts[pn0].vec();
    const TVec v1 = surf->verts[pn1].vec();
    #if 0
    GCon->Logf(NAME_Debug, "surface #%d : %p for subsector #%d: checking point; line=(%g,%g,%g)-(%g,%g,%g); plane=(%g,%g,%g):%g; point=(%g,%g,%g); online=%d",
      sfidx, surf, (int)(ptrdiff_t)(surf->subsector-&Level->Subsectors[0]),
      v0.x, v0.y, v0.z, v1.x, v1.y, v1.z,
      surf->plane.normal.x, surf->plane.normal.y, surf->plane.normal.z, surf->plane.dist,
      p.x, p.y, p.z, (int)IsPointOnLine2D(v0, v1, p));
    #endif
    // this also checks for corners, and for very short lines
    if (!IsPointOnLine3D(v0, v1, p)) continue;
    #if 0
    GCon->Logf(NAME_Debug, "surface #%d : %p for subsector #%d need a new point before %d; line=(%g,%g,%g)-(%g,%g,%g); plane=(%g,%g,%g):%g; orgpoint=(%g,%g,%g) (cp=%d)",
      sfidx, surf, (int)(ptrdiff_t)(surf->subsector-&Level->Subsectors[0]), pn0+1,
      v0.x, v0.y, v0.z, v1.x, v1.y, v1.z,
      surf->plane.normal.x, surf->plane.normal.y, surf->plane.normal.z, surf->plane.dist,
      p.x, p.y, p.z, (int)surf->isCentroidCreated());
    GCon->Logf(NAME_Debug, "=== BEFORE (%d) ===", surf->count);
    for (int f = 0; f < surf->count; ++f) {
      GCon->Logf(NAME_Debug, "  %2d: (%g,%g,%g); ownssf=%p, ownseg=%d", f, surf->verts[f].x, surf->verts[f].y, surf->verts[f].z,
        surf->verts[f].ownerssf,
        (surf->verts[f].ownerseg ? (int)(ptrdiff_t)(surf->verts[f].ownerseg-&Level->Segs[0]) : -1));
    }
    #endif
    // insert a new point
    if (!prev && surf != surfhead) {
      prev = surfhead;
      while (prev->next != surf) prev = prev->next;
    }
    surf = RLev->EnsureSurfacePoints(surf, surf->count+(surf->isCentroidCreated() ? 1 : 3), surfhead, prev);
    // create centroid
    if (!surf->isCentroidCreated()) {
      surf->AddCentroid();
      ++pn0;
    }
    // insert point
    surf->InsertVertexAt(pn0+1, p, ownssf, ownseg);
    #if 0
    GCon->Logf(NAME_Debug, "=== AFTER (%d) ===", surf->count);
    for (int f = 0; f < surf->count; ++f) {
      GCon->Logf(NAME_Debug, "  %2d: (%g,%g,%g); ownssf=%d, ownseg=%d", f, surf->verts[f].x, surf->verts[f].y, surf->verts[f].z,
        surf->verts[f].ownerssf,
        (surf->verts[f].ownerseg ? (int)(ptrdiff_t)(surf->verts[f].ownerseg-&Level->Segs[0]) : -1));
    }
    #endif
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
static void FlatSurfacesInsertPoint (VRenderLevelShared *RLev, sec_surface_t *ownssf, seg_t *ownseg, surface_t *&surfhead, const TVec p) {
  surface_t *surf = surfhead;
  surface_t *prev = nullptr;
  while (surf) {
    surf = SurfaceInsertPointIntoEdge(RLev, ownssf, ownseg, surf, surfhead, prev, p);
    prev = surf;
    surf = surf->next;
  }
}


//==========================================================================
//
//  WallSurfacesInsertPoint
//
//  do not pass surface vertices as `p`!
//  it may be a vertex from some source that can be modified
//
//==========================================================================
static void WallSurfacesInsertPoint (VRenderLevelShared *RLev, sec_surface_t *ownssf, seg_t *ownseg, surface_t *&surfhead, const TVec p) {
  surface_t *surf = surfhead;
  surface_t *prev = nullptr;
  while (surf) {
    surf = SurfaceInsertPointIntoEdge(RLev, ownssf, ownseg, surf, surfhead, prev, p);
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
static inline void FixSecSurface (VRenderLevelShared *RLev, sec_surface_t *ownssf, seg_t *ownseg, sec_surface_t *secsurf, const TVec &p) {
  if (secsurf && secsurf->surfs) FlatSurfacesInsertPoint(RLev, ownssf, ownseg, secsurf->surfs, p);
}


//==========================================================================
//
//  FixOwnSecSurface
//
//  fixes one surface
//  must be called before subdivisions
//
//==========================================================================
static surface_t *FixOwnSecSurface (VRenderLevelShared *RLev, sec_surface_t *ownssf, seg_t *ownseg, surface_t *surf, sec_surface_t *secsfc) {
  for (const surface_t *srcsurf = (secsfc ? secsfc->surfs : nullptr); srcsurf; srcsurf = srcsurf->next) {
    for (int pn0 = (int)srcsurf->isCentroidCreated(); pn0 < srcsurf->count-(int)srcsurf->isCentroidCreated(); ++pn0) {
      surf = SurfaceInsertPointIntoEdge(RLev, ownssf, ownseg, surf, surf, nullptr, srcsurf->verts[pn0].vec());
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
static void FlatInsertPointsFromAllSurfaces (VRenderLevelShared *RLev, sec_surface_t *ownssf, seg_t *ownseg, surface_t *slist, surface_t *&surfhead) {
  for (; slist; slist = slist->next) {
    for (int spn = (int)slist->isCentroidCreated(); spn < slist->count-(int)slist->isCentroidCreated(); ++spn) {
      FlatSurfacesInsertPoint(RLev, ownssf, ownseg, surfhead, slist->verts[spn].vec());
    }
  }
}


//==========================================================================
//
//  CleanupSurfaceList
//
//==========================================================================
static void CleanupSurfaceList (surface_t *surf, const sec_surface_t *ownssf, const seg_t *ownseg) {
  for (; surf; surf = surf->next) {
    if (ownssf) surf->RemoveSsfOwnVertices(ownssf);
    if (ownseg) surf->RemoveSegOwnVertices(ownseg);
  }
}


//==========================================================================
//
//  CleanupSurfaceLists
//
//==========================================================================
static void CleanupSurfaceLists (drawseg_t *ds, const sec_surface_t *ownssf, const seg_t *ownseg) {
  if (ds) {
    if (ds->top) CleanupSurfaceList(ds->top->surfs, ownssf, ownseg);
    if (ds->mid) CleanupSurfaceList(ds->mid->surfs, ownssf, ownseg);
    if (ds->bot) CleanupSurfaceList(ds->bot->surfs, ownssf, ownseg);
    if (ds->topsky) CleanupSurfaceList(ds->topsky->surfs, ownssf, ownseg);
    //if (ds->extra) CleanupSurfaceList(ds->extra->surfs, ownssf, ownseg);
    //CleanupSurfaceList(ds->HorizonTop, ownssf, ownseg);
    //CleanupSurfaceList(ds->HorizonBot, ownssf, ownseg);
  }
}


//==========================================================================
//
//  AddWallPointsFromSurfaceList
//
//==========================================================================
static void AddWallPointsFromSurfaceList (VRenderLevelShared *RLev, surface_t *surf, seg_t *seg, surface_t *&surfhead) {
  for (; surf; surf = surf->next) {
    for (int spn = (int)surf->isCentroidCreated(); spn < surf->count-(int)surf->isCentroidCreated(); ++spn) {
      const SurfVertex *sv = &surf->verts[spn];
      if (sv->ownerseg == seg) continue; // fast reject, just in case
      WallSurfacesInsertPoint(RLev, nullptr, seg, surfhead, sv->vec());
    }
  }
}


//==========================================================================
//
//  AddWallPointsFromSegSurfaces
//
//==========================================================================
static void AddWallPointsFromSegSurfaces (VRenderLevelShared *RLev, seg_t *seg, surface_t *&surfhead) {
  if (!seg) return;
  drawseg_t *ds = seg->drawsegs;
  if (ds) {
    if (ds->top) AddWallPointsFromSurfaceList(RLev, ds->top->surfs, seg, surfhead);
    if (ds->mid) AddWallPointsFromSurfaceList(RLev, ds->mid->surfs, seg, surfhead);
    if (ds->bot) AddWallPointsFromSurfaceList(RLev, ds->bot->surfs, seg, surfhead);
    if (ds->topsky) AddWallPointsFromSurfaceList(RLev, ds->topsky->surfs, seg, surfhead);
    //if (ds->extra) AddWallPointsFromSurfaceList(RLev, ds->extra->surfs, seg, surfhead);
    //AddWallPointsFromSurfaceList(RLev, ds->HorizonTop, seg, surfhead);
    //AddWallPointsFromSurfaceList(RLev, ds->HorizonBot, seg, surfhead);
  }
}


//==========================================================================
//
//  AddFloorPointToSegWalls
//
//==========================================================================
static void AddFloorPointToSegWalls (VRenderLevelShared *RLev, seg_t *seg, sec_surface_t *ownssf, const TVec &p) {
  if (!seg) return;
  drawseg_t *ds = seg->drawsegs;
  if (!ds) return;
  if (ds->top) WallSurfacesInsertPoint(RLev, ownssf, nullptr, ds->top->surfs, p);
  if (ds->mid) WallSurfacesInsertPoint(RLev, ownssf, nullptr, ds->mid->surfs, p);
  if (ds->bot) WallSurfacesInsertPoint(RLev, ownssf, nullptr, ds->bot->surfs, p);
  if (ds->topsky) WallSurfacesInsertPoint(RLev, ownssf, nullptr, ds->topsky->surfs, p);
  //if (ds->extra) WallSurfacesInsertPoint(RLev, ownssf, nullptr, ds->extra->surfs, p);
  //WallSurfacesInsertPoint(RLev, ownssf, nullptr, ds->HorizonTop, p);
  //WallSurfacesInsertPoint(RLev, ownssf, nullptr, ds->HorizonBot, p);
}



//==========================================================================
//
//  VRenderLevelLightmap::SubdivideFace
//
//==========================================================================
surface_t *VRenderLevelLightmap::SubdivideFace (surface_t *surf, subregion_t *sreg, sec_surface_t *ssf, const TVec &axis, const TVec *nextaxis, const TPlane *plane, bool doSubdivisions) {
  (void)sreg;
  if (!surf) return surf;

  subsector_t *sub = surf->subsector;
  vassert(sub);

  if (surf->count < 3 || sub->isOriginalPObj()) return surf; // nobody cares

  vassert(!surf->isCentroidCreated());

  adjSubs.resetNoDtor(); // adjacent subsectors will be collected here
  adjSegs.resetNoDtor(); // partner segs will be collected here

  {
    seg_t *seg = &Level->Segs[sub->firstline];
    for (int f = sub->numlines; f--; ++seg) {
      vassert(seg->frontsub == sub);
      CleanupSurfaceLists(seg->drawsegs, ssf, nullptr);
      seg_t *partner = seg->partner;
      if (!partner) continue; // one-sided seg
      subsector_t *psub = partner->frontsub;
      if (psub == sub) continue; // just in case
      bool found = false;
      for (auto &&v : adjSegs) if (v == partner) { found = true; break; }
      if (!found) {
        adjSegs.append(partner);
        CleanupSurfaceLists(partner->drawsegs, ssf, nullptr);
      }
      found = false;
      for (auto &&v : adjSubs) if (v == psub) { found = true; break; }
      if (!found) {
        adjSubs.append(psub);
        // clear regions
        for (subregion_t *region = psub->regions; region; region = region->next) {
          if (region->realfloor) CleanupSurfaceList(region->realfloor->surfs, ssf, nullptr);
          if (region->realceil) CleanupSurfaceList(region->realceil->surfs, ssf, nullptr);
          if (region->fakefloor) CleanupSurfaceList(region->fakefloor->surfs, ssf, nullptr);
          if (region->fakeceil) CleanupSurfaceList(region->fakeceil->surfs, ssf, nullptr);
        }
      }
    }
  }

  // add points from the adjacent subsectors to this one
  if (lastRenderQuality) {
    for (auto &&psub : adjSubs) {
      for (subregion_t *region = psub->regions; region; region = region->next) {
        surf = FixOwnSecSurface(this, region->realfloor, nullptr, surf, region->realfloor);
        surf = FixOwnSecSurface(this, region->realceil, nullptr, surf, region->realceil);
        surf = FixOwnSecSurface(this, region->fakefloor, nullptr, surf, region->fakefloor);
        surf = FixOwnSecSurface(this, region->fakeceil, nullptr, surf, region->fakeceil);
      }
    }
  }

  if (doSubdivisions) {
    //GCon->Logf(NAME_Debug, "removing centroid from default %s surface of subsector #%d (vcount=%d; hasc=%d)", (plane->isFloor() ? "floor" : "ceiling"), (int)(ptrdiff_t)(sub-&Level->Subsectors[0]), surf->count, surf->isCentroidCreated());
    // we'll re-add it later; subdivision works better without it
    surf = RemoveCentroids(surf);
    surf = SubdivideFaceInternal(surf, axis, nextaxis, plane);
    vassert(surf);
  }

  // always create centroids for complex surfaces
  // this is required to avoid omiting some triangles in renderer (which can cause t-junctions)
  surf = RecreateCentroids(this, surf);

  if (!lastRenderQuality) return surf; // no "quality fixes" required

  // fix t-junctions //

  // fix splitted surfaces with each other
  // this is stupid brute-force approach, but i may do something with it later
  if (surf->next) {
    // surface to check
    surface_t *prev = nullptr;
    for (surface_t *sfcheck = surf; sfcheck; prev = sfcheck, sfcheck = sfcheck->next) {
      // check with other surfaces' points
      for (surface_t *sfother = surf; sfother; sfother = sfother->next) {
        if (sfcheck == sfother) continue;
        for (int spn = (int)sfother->isCentroidCreated(); spn < sfother->count-(int)sfother->isCentroidCreated(); ++spn) {
          sfcheck = SurfaceInsertPointIntoEdge(this, nullptr, nullptr, sfcheck, surf, prev, sfother->verts[spn].vec());
        }
      }
    }
  }

  // append points from our wall surfaces
  {
    seg_t *seg = &Level->Segs[sub->firstline];
    for (int f = sub->numlines; f--; ++seg) {
      drawseg_t *ds = seg->drawsegs;
      if (ds) {
        if (ds->top) FlatInsertPointsFromAllSurfaces(this, nullptr, seg, ds->top->surfs, surf);
        if (ds->mid) FlatInsertPointsFromAllSurfaces(this, nullptr, seg, ds->mid->surfs, surf);
        if (ds->bot) FlatInsertPointsFromAllSurfaces(this, nullptr, seg, ds->bot->surfs, surf);
        if (ds->topsky) FlatInsertPointsFromAllSurfaces(this, nullptr, seg, ds->topsky->surfs, surf);
        //if (ds->extra) FlatInsertPointsFromAllSurfaces(this, nullptr, seg, ds->extra->surfs, surf);
        //FlatInsertPointsFromAllSurfaces(this, nullptr, seg, ds->HorizonTop, surf);
        //FlatInsertPointsFromAllSurfaces(this, nullptr, seg, ds->HorizonBot, surf);
      }
    }
  }

  // append points from adjacent wall surfaces
  for (auto &&partner : adjSegs) {
    drawseg_t *ds = partner->drawsegs;
    if (ds) {
      if (ds->top) FlatInsertPointsFromAllSurfaces(this, nullptr, partner, ds->top->surfs, surf);
      if (ds->mid) FlatInsertPointsFromAllSurfaces(this, nullptr, partner, ds->mid->surfs, surf);
      if (ds->bot) FlatInsertPointsFromAllSurfaces(this, nullptr, partner, ds->bot->surfs, surf);
      if (ds->topsky) FlatInsertPointsFromAllSurfaces(this, nullptr, partner, ds->topsky->surfs, surf);
      //if (ds->extra) FlatInsertPointsFromAllSurfaces(this, nullptr, partner, ds->extra->surfs, surf);
      //FlatInsertPointsFromAllSurfaces(this, nullptr, partner, ds->HorizonTop, surf);
      //FlatInsertPointsFromAllSurfaces(this, nullptr, partner, ds->HorizonBot, surf);
    }
  }

  // do not fix polyobjects, or invalid subsectors (yet)
  if (sub->isAnyPObj() || sub->numlines < 1) return surf;

  // fix adjacent subsector segs and flats
  for (surface_t *ss = surf; ss; ss = ss->next) {
    for (int spn = (int)ss->isCentroidCreated(); spn < ss->count-(int)ss->isCentroidCreated(); ++spn) {
      TVec p(ss->verts[spn].vec());
      // our segs
      {
        seg_t *seg = &Level->Segs[sub->firstline];
        for (int f = sub->numlines; f--; ++seg) {
          AddFloorPointToSegWalls(this, seg, ssf, p);
        }
      }
      // adjacent segs
      for (auto &&partner : adjSegs) {
        AddFloorPointToSegWalls(this, partner, ssf, p);
      }
      // add point to adjacent flats
      for (auto &&psub : adjSubs) {
        for (subregion_t *region = psub->regions; region; region = region->next) {
          FixSecSurface(this, ssf, nullptr, region->realfloor, p);
          FixSecSurface(this, ssf, nullptr, region->realceil, p);
          FixSecSurface(this, ssf, nullptr, region->fakefloor, p);
          FixSecSurface(this, ssf, nullptr, region->fakeceil, p);
        }
      }
    }
  }

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
  vassert(!surf->next);
  // remove centroid (it could be added by wall t-junction fixer)
  surf = RemoveCentroids(surf);
  surf = SubdivideSegInternal(surf, axis, nextaxis, seg);
  // always create centroids for complex surfaces
  // this is required to avoid omiting some triangles in renderer (which can cause t-junctions)
  surf = RecreateCentroids(this, surf);
  return surf;
}


//==========================================================================
//
//  VRenderLevelLightmap::FixSegSurfaceTJunctions
//
//  append points from adjacent line (seg) surfaces
//  append our new points to adjacent line (seg) surfaces
//
//==========================================================================
surface_t *VRenderLevelLightmap::FixSegSurfaceTJunctions (surface_t *surf, seg_t *myseg) {
  if (!surf || !lastRenderQuality || !myseg->linedef) return surf;

  subsector_t *sub = surf->subsector;
  vassert(sub);

  if (sub->isOriginalPObj()) return surf; // nobody cares

  surf = RemoveCentroids(surf);

  // collect all segs we may modify
  adjSegs.resetNoDtor();
  {
    const line_t *ld = myseg->linedef;
    // own segs
    for (seg_t *lseg = ld->firstseg; lseg; lseg = lseg->lsnext) {
      if (!lseg->drawsegs || lseg == myseg) continue;
      if (lseg->v1 == myseg->v1 || lseg->v2 == myseg->v1 ||
          lseg->v1 == myseg->v2 || lseg->v2 == myseg->v2)
      {
        // good seg
        bool found = false;
        for (int c = 0; c < adjSegs.length(); ++c) if (adjSegs.ptr()[c] == lseg) { found = true; break; }
        if (!found) adjSegs.append(lseg);
      }
    }
    // adjacent segs
    for (int lvidx = 0; lvidx < 2; ++lvidx) {
      // don't do this for "inner" segs
      if (lvidx == 0) {
        if (myseg->v1 != ld->v1 && myseg->v2 != ld->v1) continue;
      } else {
        if (myseg->v1 != ld->v2 && myseg->v2 != ld->v2) continue;
      }
      const int len = ld->vxCount(lvidx);
      for (int f = 0; f < len; ++f) {
        const line_t *ll = ld->vxLine(lvidx, f);
        // we need the first and the last segs
        for (seg_t *lseg = ll->firstseg; lseg; lseg = lseg->lsnext) {
          if (!lseg->drawsegs || lseg == myseg) continue;
          if (lseg->v1 == ld->v1 || lseg->v2 == ld->v1 ||
              lseg->v1 == ld->v2 || lseg->v2 == ld->v2)
          {
            // good seg
            bool found = false;
            for (int c = 0; c < adjSegs.length(); ++c) if (adjSegs.ptr()[c] == lseg) { found = true; break; }
            if (!found) adjSegs.append(lseg);
          }
        }
      }
    }
  }

  // remove all vertices we may add to adjacent surfaces
  for (auto &&seg : adjSegs) CleanupSurfaceLists(seg->drawsegs, nullptr, myseg);

  // add points to adjacent surfaces
  if (surf->next) {
    // had a split, need to do it
    for (auto &&seg : adjSegs) {
      drawseg_t *ds = seg->drawsegs;
      //AddWallPointsFromSegSurfaces(this, myseg, surf);
      if (ds->top) AddWallPointsFromSegSurfaces(this, myseg, ds->top->surfs);
      if (ds->mid) AddWallPointsFromSegSurfaces(this, myseg, ds->mid->surfs);
      if (ds->bot) AddWallPointsFromSegSurfaces(this, myseg, ds->bot->surfs);
      if (ds->topsky) AddWallPointsFromSegSurfaces(this, myseg, ds->topsky->surfs);
      //if (ds->extra) AddWallPointsFromSegSurfaces(this, myseg, ds->extra->surfs);
      //AddWallPointsFromSegSurfaces(this, myseg, ds->HorizonTop);
      //AddWallPointsFromSegSurfaces(this, myseg, ds->HorizonBot);
    }
  }

  {
    // cleanup adjacent subsector flats
    seg_t *seg = &Level->Segs[sub->firstline];
    for (int f = sub->numlines; f--; ++seg) {
      if (!seg->linedef) continue; // minisegs cannot change anything
      vassert(seg->frontsub == sub);
      seg_t *partner = seg->partner;
      if (!partner) continue; // one-sided seg
      subsector_t *psub = partner->frontsub;
      if (psub == sub) continue; // just in case
      // check floor and ceiling
      for (subregion_t *region = psub->regions; region; region = region->next) {
        if (region->realfloor) CleanupSurfaceList(region->realfloor->surfs, nullptr, myseg);
        if (region->realceil) CleanupSurfaceList(region->realceil->surfs, nullptr, myseg);
        if (region->fakefloor) CleanupSurfaceList(region->fakefloor->surfs, nullptr, myseg);
        if (region->fakeceil) CleanupSurfaceList(region->fakeceil->surfs, nullptr, myseg);
      }
    }
  }

  {
    // process adjacent subsector flats too
    seg_t *seg = &Level->Segs[sub->firstline];
    for (int f = sub->numlines; f--; ++seg) {
      if (!seg->linedef) continue; // minisegs cannot change anything
      vassert(seg->frontsub == sub);
      seg_t *partner = seg->partner;
      if (!partner) continue; // one-sided seg
      subsector_t *psub = partner->frontsub;
      if (psub == sub) continue; // just in case
      // check floor and ceiling
      for (subregion_t *region = psub->regions; region; region = region->next) {
        for (surface_t *ss = surf; ss; ss = ss->next) {
          for (int spn = (int)ss->isCentroidCreated(); spn < ss->count-(int)ss->isCentroidCreated(); ++spn) {
            const TVec &p = ss->verts[spn].vec();
            FixSecSurface(this, nullptr, myseg, region->realfloor, p);
            FixSecSurface(this, nullptr, myseg, region->realceil, p);
            FixSecSurface(this, nullptr, myseg, region->fakefloor, p);
            FixSecSurface(this, nullptr, myseg, region->fakeceil, p);
          }
        }
      }
    }
  }
  // and own too, why not?
  {
    // check floor and ceiling
    for (subregion_t *region = sub->regions; region; region = region->next) {
      for (surface_t *ss = surf; ss; ss = ss->next) {
        for (int spn = (int)ss->isCentroidCreated(); spn < ss->count-(int)ss->isCentroidCreated(); ++spn) {
          const TVec &p = ss->verts[spn].vec();
          FixSecSurface(this, nullptr, nullptr, region->realfloor, p);
          FixSecSurface(this, nullptr, nullptr, region->realceil, p);
          FixSecSurface(this, nullptr, nullptr, region->fakefloor, p);
          FixSecSurface(this, nullptr, nullptr, region->fakeceil, p);
        }
      }
    }
  }

  //if (surf->count < 3) return surf; // nothing to do here

  // add all points from adjacent surfaces
  for (auto &&seg : adjSegs) AddWallPointsFromSegSurfaces(this, seg, surf);

  // always create centroids for complex surfaces
  // this is required to avoid omiting some triangles in renderer (which can cause t-junctions)
  surf = RecreateCentroids(this, surf);

  return surf;
}
