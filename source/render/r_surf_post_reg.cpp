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
//**  Copyright (C) 2018-2022 Ketmar Dark
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
VCvarB r_precalc_static_lights("r_precalc_static_lights", true, "Precalculate static lights?", CVAR_Archive|CVAR_NoShadow);
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
//  surfIndex
//
//==========================================================================
static VVA_OKUNUSED int surfIndex (const surface_t *surfs, const surface_t *curr) noexcept {
  int res;
  if (!curr) {
    res = -1;
  } else {
    res = 0;
    while (surfs && surfs != curr) { ++res; surfs = surfs->next; }
    if (!surfs) res = -666;
  }
  return res;
}


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
  // 0: front; 1: back
  int vcount[2];
  SurfVertex *verts[2];

  inline void appendVertex (const int pidx, const SurfVertex &v) noexcept {
    verts[pidx][vcount[pidx]++] = v;
  }

  inline void appendPoint (const int pidx, const TVec p) noexcept {
    verts[pidx][vcount[pidx]++].Set(p);
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
    } else {
      clip.appendVertex((sides[i] == PlaneBack), vt[i]);
      if (sides[i+1] != PlaneCoplanar && sides[i] != sides[i+1]) {
        // generate a split point
        const TVec &p1 = vt[i].vec();
        const TVec &p2 = vt[(i+1)%surfcount].vec();

        const float dot = dots[i]/(dots[i]-dots[i+1]);
        for (int j = 0; j < 3; ++j) {
          // avoid round off error when possible
          if (fabsf(plane.normal[j]) == 1.0f) {
            mid[j] = (plane.normal[j] > 0.0f ? plane.dist : -plane.dist);
          } else {
            mid[j] = p1[j]+dot*(p2[j]-p1[j]);
          }
        }

        clip.appendPoint(0, mid);
        clip.appendPoint(1, mid);
      }
    }
  }

  return (clip.vcount[0] >= 3 && clip.vcount[1] >= 3);
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
static void RecreateCentroids (VRenderLevelShared *RLev, surface_t *&surf) {
  surface_t *prev = nullptr;
  for (surface_t *ss = surf; ss; prev = ss, ss = ss->next) {
    if (ss->count > 4 && !ss->isCentroidCreated()) {
      // make room
      ss = RLev->EnsureSurfacePoints(ss, ss->count+2, surf, prev);
      if (!prev) surf = ss; // first item, fix list head
      // insert centroid
      ss->AddCentroid();
    }
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::InitSurfs
//
//==========================================================================
void VRenderLevelLightmap::InitSurfs (bool recalcStaticLightmaps, surface_t *ASurfs,
                                      texinfo_t *texinfo, const TPlane *plane,
                                      subsector_t *sub, seg_t *seg, subregion_t *sreg)
{
  bool doPrecalc = (r_precalc_static_lights_override >= 0
                      ? !!r_precalc_static_lights_override
                      : r_precalc_static_lights);

  for (surface_t *surf = ASurfs; surf; surf = surf->next) {
    if (texinfo) surf->texinfo = texinfo;
    if (plane) surf->plane = *plane;
    surf->subsector = sub;
    surf->seg = seg;
    surf->sreg = sreg;

    if (surf->count == 0) {
      GCon->Logf(NAME_Warning, "empty surface at subsector #%d",
                 (int)(ptrdiff_t)(sub-Level->Subsectors));
      surf->texturemins[0] = 16;
      surf->extents[0] = 16;
      surf->texturemins[1] = 16;
      surf->extents[1] = 16;
      surf->subsector = sub;
      surf->drawflags &= ~surface_t::DF_CALC_LMAP; // just in case
    } else if (surf->count < 3) {
      GCon->Logf(NAME_Warning, "degenerate surface with #%d vertices at subsector #%d",
                 surf->count, (int)(ptrdiff_t)(sub-Level->Subsectors));
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
        GCon->Logf(NAME_Warning, "Subsector %d got too big S surface extents: (%d,%d)",
                   (int)(ptrdiff_t)(sub-Level->Subsectors), bmins, bmaxs);
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
        GCon->Logf(NAME_Warning, "Subsector %d got too big T surface extents: (%d,%d)",
                   (int)(ptrdiff_t)(sub-Level->Subsectors), bmins, bmaxs);
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
surface_t *VRenderLevelLightmap::SubdivideFaceInternal (surface_t *surf, const TVec &axis,
                                                        const TVec *nextaxis, const TPlane *plane)
{
  subsector_t *sub = surf->subsector;
  vassert(sub);

  // just in case
  //if (surf->isCentroidCreated()) surf->RemoveCentroid();
  vassert(!surf->isCentroidCreated());

  if (plane) surf->plane = *plane;

  if (surf->count < 2) {
    //Sys_Error("surface with less than three (%d) vertices)", f->count);
    GCon->Logf(NAME_Warning, "surface with less than two (%d) vertices (divface) (sub=%d; sector=%d)",
               surf->count, (int)(ptrdiff_t)(sub-Level->Subsectors),
               (int)(ptrdiff_t)(sub->sector-Level->Sectors));
    return surf;
  }

  // this can happen for wall without texture
  if (!axis.isValid() || axis.isZero()) {
    GCon->Logf(NAME_Warning, "ERROR(SF): invalid axis (%f,%f,%f); THIS IS MAP BUG! (sub=%d; sector=%d)",
               axis.x, axis.y, axis.z, (int)(ptrdiff_t)(sub-Level->Subsectors),
               (int)(ptrdiff_t)(sub->sector-Level->Sectors));
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
  memcpy((void *)back->verts, clip.verts[1], back->count*sizeof(SurfVertex));
  vassert(back->verts[0].tjflags >= 0);
  if (plane) back->plane = *plane;

  surface_t *front = NewWSurf(clip.vcount[0]);
  front->copyRequiredFrom(*surf);
  front->count = clip.vcount[0];
  memcpy((void *)front->verts, clip.verts[0], front->count*sizeof(SurfVertex));
  vassert(front->verts[0].tjflags >= 0);
  if (plane) front->plane = *plane;

  front->next = surf->next;

  surf->next = nullptr;
  FreeWSurfs(surf);

  back->next = SubdivideFaceInternal(front, axis, nextaxis, plane);
  return (nextaxis ? SubdivideFaceInternal(back, *nextaxis, nullptr, plane) : back);
}


//==========================================================================
//
//  VRenderLevelLightmap::SubdivideFace
//
//==========================================================================
surface_t *VRenderLevelLightmap::SubdivideFace (surface_t *surf,
                                                subregion_t *sreg, sec_surface_t *ssf,
                                                const TVec &axis, const TVec *nextaxis,
                                                const TPlane *plane, bool doSubdivisions)
{
  (void)sreg;
  if (!surf) return surf;

  subsector_t *sub = surf->subsector;
  vassert(sub);

  if (surf->count < 3 || sub->isOriginalPObj()) return surf; // nobody cares

  vassert(!surf->isCentroidCreated());

  if (doSubdivisions) {
    //GCon->Logf(NAME_Debug, "removing centroid from default %s surface of subsector #%d (vcount=%d; hasc=%d)", (plane->isFloor() ? "floor" : "ceiling"), (int)(ptrdiff_t)(sub-&Level->Subsectors[0]), surf->count, surf->isCentroidCreated());
    // we'll re-add it later; subdivision works better without it
    surf = SubdivideFaceInternal(surf, axis, nextaxis, plane);
    vassert(surf);
  }

  return surf;
}


//==========================================================================
//
//  VRenderLevelLightmap::SubdivideSegInternal
//
//==========================================================================
surface_t *VRenderLevelLightmap::SubdivideSegInternal (surface_t *surf, const TVec &axis,
                                                       const TVec *nextaxis, seg_t *seg)
{
  subsector_t *sub = surf->subsector;
  //vassert(surf->seg == seg);
  vassert(sub);

  vassert(!surf->isCentroidCreated());

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

  if (clip.vcount[1] > surface_t::MAXWVERTS && clip.vcount[1] > surf->count) {
    GCon->Logf(NAME_Error, "clipped surface has %d vertices (orig: %d) (and max is %d)", clip.vcount[1], surf->count, surface_t::MAXWVERTS);
    vassert(clip.vcount[1] <= surface_t::MAXWVERTS || clip.vcount[1] <= surf->count);
  }

  surf->count = clip.vcount[1];
  if (surf->count) memcpy((void *)surf->verts, clip.verts[1], surf->count*sizeof(SurfVertex));

  surface_t *news = NewWSurf(clip.vcount[0]);
  news->copyRequiredFrom(*surf);
  news->count = clip.vcount[0];
  if (news->count) memcpy((void *)news->verts, clip.verts[0], news->count*sizeof(SurfVertex));
  if (seg) news->plane = *seg;

  news->next = surf->next;
  surf->next = SubdivideSegInternal(news, axis, nextaxis, seg);
  return (nextaxis ? SubdivideSegInternal(surf, *nextaxis, nullptr, seg) : surf);
}


//==========================================================================
//
//  VRenderLevelLightmap::SubdivideSeg
//
//==========================================================================
surface_t *VRenderLevelLightmap::SubdivideSeg (surface_t *surf, const TVec &axis,
                                               const TVec *nextaxis, seg_t *seg)
{
  vassert(!surf->next);
  return SubdivideSegInternal(surf, axis, nextaxis, seg);
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
static surface_t *SurfaceInsertPointIntoEdge (VRenderLevelShared *RLev,
                                              surface_t *surf, surface_t *&surfhead,
                                              surface_t *prev, const TVec p)
{
  if (!surf || surf->count < 3+(int)surf->isCentroidCreated() || fabsf(surf->plane.PointDistance(p)) >= 0.01f) return surf;
  //surface_t *prev = nullptr;
  #if 0
  VLevel *Level = RLev->GetLevel();
  const int sfidx = surfIndex(surfhead, surf);
  #endif
  // check each surface line
  const int cc = (int)surf->isCentroidCreated();
  const int end = surf->count-cc; // with centroid, last vertex dups first non-centroid one
  for (int pn0 = cc; pn0 < end; ++pn0) {
    const int pn1 = (pn0+1)%surf->count;
    vassert((cc == 0) || (pn1 != 0));
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
    #if 0
      surf = RLev->EnsureSurfacePoints(surf, surf->count+(surf->isCentroidCreated() ? 1 : 3), surfhead, prev);
      // create centroid
      if (!surf->isCentroidCreated()) {
        surf->AddCentroid();
        ++pn0;
      }
    #else
      // remove centroid, it will be recreated by the caller
      if (surf->isCentroidCreated()) {
        surf->RemoveCentroid();
        --pn0;
        vassert(pn0 >= 0);
      }
      surf = RLev->EnsureSurfacePoints(surf, surf->count+1, surfhead, prev);
    #endif
    // insert point
    surf->InsertVertexAt(pn0+1, p, 1);
    //vassert(surf->count > 4);
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
//  CleanupSurfaceList
//
//==========================================================================
static void CleanupSurfaceList (surface_t *surf) {
  for (; surf; surf = surf->next) {
    surf->RemoveTJVerts();
    // don't bother, we don't need to do it
    //if (surf->isCentroidCreated()) surf->RemoveCentroid();
  }
}


//==========================================================================
//
//  CleanupSurfaceLists
//
//==========================================================================
static void CleanupSurfaceLists (drawseg_t *ds) {
  if (ds) {
    if (ds->top) CleanupSurfaceList(ds->top->surfs);
    if (ds->mid) CleanupSurfaceList(ds->mid->surfs);
    if (ds->bot) CleanupSurfaceList(ds->bot->surfs);
    if (ds->topsky) CleanupSurfaceList(ds->topsky->surfs);
    if (ds->extra) CleanupSurfaceList(ds->extra->surfs);
    //CleanupSurfaceList(ds->HorizonTop, ownssf, ownseg);
    //CleanupSurfaceList(ds->HorizonBot, ownssf, ownseg);
  }
}


//==========================================================================
//
//  AddPointsFromSurfaceList
//
//==========================================================================
static void AddPointsFromSurfaceList (VRenderLevelShared *RLev,
                                      surface_t *list, /*from*/
                                      surface_t *&surfhead/*to*/)
{
  for (surface_t *curr = list; curr; curr = curr->next) {
    for (int spn = (int)curr->isCentroidCreated(); spn < curr->count-(int)curr->isCentroidCreated(); ++spn) {
      const SurfVertex *sv = &curr->verts[spn];
      if (sv->tjflags > 0) continue; // fast reject, just in case (we just added this point)

      surface_t *surf = surfhead;
      surface_t *prev = nullptr;
      while (surf) {
        if (surf != curr) {
          surf = SurfaceInsertPointIntoEdge(RLev, surf, surfhead, prev, sv->vec());
        }
        prev = surf;
        surf = surf->next;
      }
    }
  }
}


//==========================================================================
//
//  AddPointsFromDrawseg
//
//==========================================================================
static void AddPointsFromDrawseg (VRenderLevelShared *RLev,
                                  drawseg_t *ds,
                                  surface_t *&surfhead/*to*/)
{
  if (ds) {
    if (ds->top) AddPointsFromSurfaceList(RLev, ds->top->surfs, surfhead);
    if (ds->mid) AddPointsFromSurfaceList(RLev, ds->mid->surfs, surfhead);
    if (ds->bot) AddPointsFromSurfaceList(RLev, ds->bot->surfs, surfhead);
    if (ds->topsky) AddPointsFromSurfaceList(RLev, ds->topsky->surfs, surfhead);
    if (ds->extra) AddPointsFromSurfaceList(RLev, ds->extra->surfs, surfhead);
  }
}


//==========================================================================
//
//  SurfSubRestoreCentroids
//
//==========================================================================
static void SurfSubRestoreCentroids (VRenderLevelShared *RLev, surface_t *&surf,
                                     subsector_t *sub)
{
  // always create centroids for complex surfaces
  // this is required to avoid omiting some triangles in renderer (which can cause t-junctions)
  RecreateCentroids(RLev, surf);

  // also, recreate centroids for floors
  if (sub) {
    for (subregion_t *region = sub->regions; region; region = region->next) {
      if (region->realfloor) RecreateCentroids(RLev, region->realfloor->surfs);
      if (region->realceil) RecreateCentroids(RLev, region->realceil->surfs);
      if (region->fakefloor) RecreateCentroids(RLev, region->fakefloor->surfs);
      if (region->fakeceil) RecreateCentroids(RLev, region->fakeceil->surfs);
    }
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::FixSegSurfaceTJunctions
//
//==========================================================================
surface_t *VRenderLevelLightmap::FixSegSurfaceTJunctions (surface_t *surf, seg_t *myseg) {
  if (!surf || !myseg || !myseg->linedef || inWorldCreation) return surf;

  subsector_t *sub = surf->subsector;
  vassert(sub);

  if (sub->isOriginalPObj()) return surf; // nobody cares

  if (lastRenderQuality) {
    //if ((int)(ptrdiff_t)(sub-&Level->Subsectors[0]) != 40) return surf;

    if (true) {
      // no need to do it
      //surf = RemoveCentroids(surf);

      // collect all segs we want to inspect
      adjSegs.resetNoDtor();
      {
        const line_t *ld = myseg->linedef;

        // own segs
        for (seg_t *lseg = ld->firstseg; lseg; lseg = lseg->lsnext) {
          if (lseg != myseg && lseg->drawsegs) {
            // good seg
            /*
            bool found = false;
            for (seg_t *ss : adjSegs) if (ss == lseg) { found = true; break; }
            if (!found) adjSegs.append(lseg);
            */
            adjSegs.append(lseg);
          }
        }

        // segs from adjacent lines
        for (int lvidx = 0; lvidx < 2; ++lvidx) {
          const int len = ld->vxCount(lvidx);
          for (int f = 0; f < len; ++f) {
            const line_t *ll = ld->vxLine(lvidx, f);
            for (seg_t *lseg = ll->firstseg; lseg; lseg = lseg->lsnext) {
              if (lseg != myseg && lseg->drawsegs) {
                // good seg
                /*
                bool found = false;
                for (seg_t *ss : adjSegs) if (ss == lseg) { found = true; break; }
                if (!found) adjSegs.append(lseg);
                */
                adjSegs.append(lseg);
              }
            }
          }
        }
      }

      // collect adjacent subsectors
      adjSubs.resetNoDtor(); // adjacent subsectors will be collected here
      adjSubs.append(sub);
      for (seg_t *xseg : adjSegs) {
        subsector_t *xsub = xseg->frontsub;
        if (xsub && xsub != sub) {
          bool found = false;
          for (subsector_t *ss : adjSubs) if (ss == xsub) { found = true; break; }
          if (!found) adjSubs.append(xsub);
        }
        if (xseg->partner) {
          subsector_t *xsub = xseg->partner->frontsub;
          if (xsub && xsub != sub) {
            bool found = false;
            for (subsector_t *ss : adjSubs) if (ss == xsub) { found = true; break; }
            if (!found) adjSubs.append(xsub);
          }
        }
      }

      #if 1
      // remove fixes for our seg surfaces
      CleanupSurfaceLists(myseg->drawsegs);
      // also, clean subsector flats, we will refix them
      // check floor and ceiling
      for (subregion_t *region = sub->regions; region; region = region->next) {
        for (surface_t *ss = surf; ss; ss = ss->next) {
          if (region->realfloor) CleanupSurfaceList(region->realfloor->surfs);
          if (region->realceil) CleanupSurfaceList(region->realceil->surfs);
          if (region->fakefloor) CleanupSurfaceList(region->fakefloor->surfs);
          if (region->fakeceil) CleanupSurfaceList(region->fakeceil->surfs);
        }
      }
      #endif

      // add all points from adjacent surfaces

      // insert wall surfaces
      for (seg_t *xseg : adjSegs) {
        drawseg_t *ds = xseg->drawsegs;
        if (xseg != myseg && ds) {
          #if 1
          AddPointsFromDrawseg(this, ds, surf);
          #endif
        }
      }

      #if 1
      // insert floor surface points from adjacent subsectors into walls
      for (subsector_t *xsub : adjSubs) {
        //seg_t *xseg = &Level->Segs[sub->firstline];
        //for (int f = sub->numlines; f--; ++xseg) {}
        for (subregion_t *region = xsub->regions; region; region = region->next) {
          if (region->realfloor) AddPointsFromSurfaceList(this, region->realfloor->surfs, surf);
          if (region->realceil) AddPointsFromSurfaceList(this, region->realceil->surfs, surf);
          if (region->fakefloor) AddPointsFromSurfaceList(this, region->fakefloor->surfs, surf);
          if (region->fakeceil) AddPointsFromSurfaceList(this, region->fakeceil->surfs, surf);
        }
      }
      #endif

      // lines from adjacent subsectors should affect floors
      // collect adjacent subsector lines
      adjSegs.resetNoDtor(); // we don't need old segs, they cannot affect floors anyway
      for (subsector_t *xsub : adjSubs) {
        seg_t *lseg = &Level->Segs[xsub->firstline];
        for (int f = sub->numlines; f--; ++lseg) {
          if (lseg != myseg && lseg->drawsegs) {
            // good seg
            bool found = false;
            for (seg_t *ss : adjSegs) if (ss == lseg) { found = true; break; }
            if (!found) adjSegs.append(lseg);
          }
        }
      }

      // fix floors
      for (seg_t *xseg : adjSegs) {
        drawseg_t *ds = xseg->drawsegs;
        if (xseg != myseg && ds) {
          #if 1
          // insert wall surface points into floors of `sub`
          for (subregion_t *region = sub->regions; region; region = region->next) {
            if (region->realfloor) AddPointsFromDrawseg(this, ds, region->realfloor->surfs);
            if (region->realceil) AddPointsFromDrawseg(this, ds, region->realceil->surfs);
            if (region->fakefloor) AddPointsFromDrawseg(this, ds, region->fakefloor->surfs);
            if (region->fakeceil) AddPointsFromDrawseg(this, ds, region->fakeceil->surfs);
          }
          #endif
        }
      }
    }
  }

  // always create centroids for complex surfaces
  // this is required to avoid omiting some triangles in renderer (which can cause t-junctions)
  SurfSubRestoreCentroids(this, surf, sub);

  return surf;
}


//==========================================================================
//
//  VRenderLevelLightmap::DoneInitialWordSurfCreation
//
//  t-junctions should be processed after all subdivisions are done,
//  otherwise there will be some unprocessed quads.
//
//==========================================================================
void VRenderLevelLightmap::DoneInitialWordSurfCreation () {
  /*
  for (auto &&sec : Level->allSectors()) SectorModified(&sec);
  */
  const bool oldIWC = inWorldCreation;
  inWorldCreation = false; // otherwise `FixSegSurfaceTJunctions()` will do nothing

  for (auto &&ssx : Level->allSegs()) {
    if (ssx.linedef && ssx.drawsegs) {
      for (int dsidx = 0; dsidx < 4; ++dsidx) {
        segpart_t *xsegpart =
          dsidx == 0 ? ssx.drawsegs->top :
          dsidx == 1 ? ssx.drawsegs->mid :
          dsidx == 2 ? ssx.drawsegs->bot :
          dsidx == 3 ? ssx.drawsegs->extra :
          nullptr;
        while (xsegpart) {
          xsegpart->surfs = FixSegSurfaceTJunctions(xsegpart->surfs, &ssx);
          xsegpart = xsegpart->next;
        }
      }
    }
  }

  inWorldCreation = oldIWC;
}
