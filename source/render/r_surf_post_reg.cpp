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
    surf->SetFixCracks();
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
  /*
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
  */
}
