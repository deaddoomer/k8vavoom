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


//#define VV_TJUNCTION_VERBOSE

#ifdef VV_TJUNCTION_VERBOSE
static bool tjlog_verbose = false;
# define TJLOG(...)  if (tjlog_verbose && dbg_fix_tjunctions) GCon->Logf(__VA_ARGS__)
#else
# define TJLOG(...)  do {} while (0)
#endif


/*
  subdivision t-junctions fixer

  loop over all surfaces; for each surface point, check if it touches
  any surfaces from the adjacent subsectors. if it is, insert that point there

  currently we're doing this by brute force
*/


//==========================================================================
//
//  FindSurf
//
//==========================================================================
static inline bool FindSurf (surface_t *surf, surface_t *list, surface_t **pprev) {
  surface_t *prev = nullptr;
  while (list && list != surf) { prev = list; list = list->next; }
  if (list && pprev) *pprev = prev;
  return !!list;
}


//==========================================================================
//
//  VRenderLevelShared::FindWallSurfHead
//
//==========================================================================
surface_t **VRenderLevelShared::FindWallSurfHead (surface_t *surf, seg_t *myseg, surface_t **pprev) {
  vassert(surf);
  vassert(myseg);
  drawseg_t *ds = myseg->drawsegs;
  if (ds) {
    if (ds->top && FindSurf(surf, ds->top->surfs, pprev)) return &ds->top->surfs;
    if (ds->mid && FindSurf(surf, ds->mid->surfs, pprev)) return &ds->mid->surfs;
    if (ds->bot && FindSurf(surf, ds->bot->surfs, pprev)) return &ds->bot->surfs;
    if (ds->topsky && FindSurf(surf, ds->topsky->surfs, pprev)) return &ds->topsky->surfs;
    if (ds->extra && FindSurf(surf, ds->extra->surfs, pprev)) return &ds->extra->surfs;
  }
  if (pprev) *pprev = nullptr;
  return nullptr;
}


//==========================================================================
//
//  VRenderLevelShared::FindSubSurfHead
//
//==========================================================================
surface_t **VRenderLevelShared::FindSubSurfHead (surface_t *surf, subsector_t *sub, surface_t **pprev) {
  vassert(surf);
  vassert(sub);
  for (subregion_t *region = sub->regions; region; region = region->next) {
    if (region->realfloor && FindSurf(surf, region->realfloor->surfs, pprev)) return &region->realfloor->surfs;
    if (region->realceil && FindSurf(surf, region->realceil->surfs, pprev)) return &region->realceil->surfs;
    if (region->fakefloor && FindSurf(surf, region->fakefloor->surfs, pprev)) return &region->fakefloor->surfs;
    if (region->fakeceil && FindSurf(surf, region->fakeceil->surfs, pprev)) return &region->fakeceil->surfs;
  }
  if (pprev) *pprev = nullptr;
  return nullptr;
}


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
//  CheckLineBBox
//
//==========================================================================
static inline bool CheckLineBBox (const TVec &v0, const TVec &v1, const TVec &p) noexcept {
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
  if (v0.z < v1.z) {
    if (p.z < v0.z || p.z > v1.z) return false;
  } else {
    if (p.z < v1.z || p.z > v0.z) return false;
  }
  return true;
}


//==========================================================================
//
//  IsPointOnLine3D
//
//==========================================================================
static inline bool IsPointOnLine3D (const TVec &v0, const TVec &v1, const TVec &p) noexcept {
  if (!CheckLineBBox(v0, v1, p)) return false;
  const TVec ldir = v1-v0;
  const float llensq = ldir.lengthSquared();
  if (llensq < 1.0f) return false; // dot
  // check for corners
  const TVec p0 = p-v0; if (p0.lengthSquared() < 1.0f) return false;
  const TVec p1 = p-v1; if (p1.lengthSquared() < 1.0f) return false;
  const float llen = sqrtf(llensq);
  // check projection
  const TVec ndir = ldir/llen;
  const float prj = ndir.dot(p0);
  if (prj < 1.0f || prj > llen-1.0f) return false; // projection is too big
  // check point distance
  const float dist = p0.cross(p1).length()/llen;
  return (dist < 0.001f);
}


//==========================================================================
//
//  VRenderLevelShared::SurfaceInsertPointIntoEdge
//
//  do not use reference to `p` here!
//  it may be a vector from some source that can be modified
//
//==========================================================================
surface_t *VRenderLevelShared::SurfaceInsertPointIntoEdge (surface_t *surf, surface_t *&surfhead,
                                                           surface_t *prev, const TVec p,
                                                           bool *modified)
{
  enum { DebugDump = false };

  if (!surf || surf->count < 3+(int)surf->isCentroidCreated() ||  // bad surface?
      fabsf(surf->plane.PointDistance(p)) >= 0.01f)  // the point should be on the surface plane
  {
    return surf;
  }

  int sfidx, subidx;
  if (DebugDump) {
    sfidx = surfIndex(surfhead, surf);
    subidx = (int)(ptrdiff_t)(surf->subsector-&Level->Subsectors[0]);
  }

  // check each surface line
  const int count = surf->count;
  const int cc = (int)surf->isCentroidCreated();
  const int end = surf->count-cc; // with centroid, last vertex dups first non-centroid one

  for (int pn0 = cc; pn0 < end; ++pn0) {
    const int pn1 = (pn0+1 == count ? 0 : pn0+1);
    vassert((cc == 0) || (pn1 != 0));
    const TVec v0 = surf->verts[pn0].vec();
    const TVec v1 = surf->verts[pn1].vec();
    if (DebugDump) {
      GCon->Logf(NAME_Debug, "surface #%d : %p for subsector #%d: checking point;"
                 " line=(%g,%g,%g)-(%g,%g,%g); plane=(%g,%g,%g):%g; point=(%g,%g,%g); online=%d",
                 sfidx, surf, subidx,
                 v0.x, v0.y, v0.z, v1.x, v1.y, v1.z,
                 surf->plane.normal.x, surf->plane.normal.y, surf->plane.normal.z, surf->plane.dist,
                 p.x, p.y, p.z, (int)IsPointOnLine3D(v0, v1, p));
    }
    // `IsPointOnLine3D()` also checks for corners, and for very short lines
    if (IsPointOnLine3D(v0, v1, p)) {
      if (DebugDump) {
        GCon->Logf(NAME_Debug, "surface #%d : %p for subsector #%d need a new point before %d; line=(%g,%g,%g)-(%g,%g,%g); plane=(%g,%g,%g):%g; orgpoint=(%g,%g,%g) (cp=%d)",
          sfidx, surf, subidx, pn0+1,
          v0.x, v0.y, v0.z, v1.x, v1.y, v1.z,
          surf->plane.normal.x, surf->plane.normal.y, surf->plane.normal.z, surf->plane.dist,
          p.x, p.y, p.z, (int)surf->isCentroidCreated());
        GCon->Logf(NAME_Debug, "=== BEFORE (%d) ===", surf->count);
        for (int f = 0; f < surf->count; ++f) {
          GCon->Logf(NAME_Debug, "  %2d: (%g,%g,%g); tjflags=%d",
                     f, surf->verts[f].x, surf->verts[f].y, surf->verts[f].z,
                     surf->verts[f].tjflags);
        }
      }
      // insert a new point
      if (!prev && surf != surfhead) {
        prev = surfhead;
        while (prev->next != surf) prev = prev->next;
      }
      // remove centroid, it will be recreated by the caller
      if (surf->isCentroidCreated()) {
        surf->RemoveCentroid();
        --pn0;
        vassert(pn0 >= 0);
      }
      surf = EnsureSurfacePoints(surf, surf->count+1, surfhead, prev);
      // insert point
      surf->InsertVertexAt(pn0+1, p, 1);
      //vassert(surf->count > 4);
      if (DebugDump) {
        GCon->Logf(NAME_Debug, "=== AFTER (%d) ===", surf->count);
        for (int f = 0; f < surf->count; ++f) {
          GCon->Logf(NAME_Debug, "  %2d: (%g,%g,%g); tjflags=%d",
                     f, surf->verts[f].x, surf->verts[f].y, surf->verts[f].z,
                     surf->verts[f].tjflags);
        }
      }
      // the point cannot be inserted into several lines,
      // so we're finished with this surface
      if (modified) *modified = true;
      break;
    }
  }

  return surf;
}


//==========================================================================
//
//  AddPointsFromSurfaceList
//
//==========================================================================
void VRenderLevelShared::AddPointsFromSurfaceList (surface_t *list, /*from*/
                                                   surface_t *&dest,
                                                   surface_t *&surfhead/*to*/,
                                                   surface_t *prev, bool *modified)
{
  vassert(dest);
  vassert(surfhead);
  while (list) {
    if (list != dest) {
      const SurfVertex *sv = &list->verts[0];
      for (int spn = list->count; spn--; ++sv) {
        // do not process fix points (it is useless)
        if (sv->tjflags == 0) {
          dest = SurfaceInsertPointIntoEdge(dest, surfhead, prev, sv->vec(), modified);
        }
      }
    }
    list = list->next;
  }
}


//==========================================================================
//
//  VRenderLevelShared::AddPointsFromDrawseg
//
//==========================================================================
void VRenderLevelShared::AddPointsFromDrawseg (drawseg_t *ds,
                                               surface_t *&dest,
                                               surface_t *&surfhead/*to*/,
                                               surface_t *prev, bool *modified)
{
  if (ds) {
    if (ds->top) AddPointsFromSurfaceList(ds->top->surfs, dest, surfhead, prev, modified);
    if (ds->mid) AddPointsFromSurfaceList(ds->mid->surfs, dest, surfhead, prev, modified);
    if (ds->bot) AddPointsFromSurfaceList(ds->bot->surfs, dest, surfhead, prev, modified);
    if (ds->topsky) AddPointsFromSurfaceList(ds->topsky->surfs, dest, surfhead, prev, modified);
    if (ds->extra) AddPointsFromSurfaceList(ds->extra->surfs, dest, surfhead, prev, modified);
  }
}


//==========================================================================
//
//  VRenderLevelShared::AddPointsFromRegion
//
//==========================================================================
void VRenderLevelShared::AddPointsFromRegion (subregion_t *region,
                                              surface_t *&dest,
                                              surface_t *&surfhead/*to*/,
                                              surface_t *prev, bool *modified)
{
  if (region) {
    if (region->realfloor) AddPointsFromSurfaceList(region->realfloor->surfs, dest, surfhead, prev, modified);
    if (region->realceil) AddPointsFromSurfaceList(region->realceil->surfs, dest, surfhead, prev, modified);
    if (region->fakefloor) AddPointsFromSurfaceList(region->fakefloor->surfs, dest, surfhead, prev, modified);
    if (region->fakeceil) AddPointsFromSurfaceList(region->fakeceil->surfs, dest, surfhead, prev, modified);
  }
}


//==========================================================================
//
//  VRenderLevelShared::AddPointsFromAllRegions
//
//==========================================================================
void VRenderLevelShared::AddPointsFromAllRegions (subsector_t *sub,
                                                  surface_t *&dest,
                                                  surface_t *&surfhead/*to*/,
                                                  surface_t *prev, bool *modified)
{
  if (sub) {
    for (subregion_t *region = sub->regions; region; region = region->next) {
      AddPointsFromRegion(region, dest, surfhead, prev, modified);
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::FixSurfCentroid
//
//  always create centroids for complex surfaces
//  this is required to avoid omiting some triangles in renderer
//  (which can cause t-junctions)
//
//==========================================================================
surface_t *VRenderLevelShared::FixSurfCentroid (surface_t *surf, surface_t *&surfhead,
                                                surface_t *prev)
{
  vassert(surf);
  vassert(surfhead);
  bool needtj = false;
  const SurfVertex *sv = &surf->verts[0];
  for (int spn = surf->count; spn-- && !needtj; ++sv) {
    needtj = (sv->tjflags > 0);
  }
  if (surf->isCentroidCreated()) surf->RemoveCentroid();
  if (needtj || surf->count > 4) {
    // make room
    surf = EnsureSurfacePoints(surf, surf->count+2, surfhead, prev);
    // insert centroid
    surf->AddCentroid();
  }
  return surf;
}


//==========================================================================
//
//  VRenderLevelShared::FixSegSurfaceTJunctions
//
//==========================================================================
surface_t *VRenderLevelShared::FixSegSurfaceTJunctions (surface_t *surf, seg_t *myseg) {
  if (!surf) return nullptr;

  surf->RemoveTJVerts();
  surf->ResetFixCracks();
  if (!myseg) return surf;

  subsector_t *sub = surf->subsector;
  vassert(sub);
  vassert(sub == myseg->frontsub);

  if (sub->isOriginalPObj()) return surf; // nobody cares

  surface_t *prev;
  surface_t **surfhead = FindWallSurfHead(surf, myseg, &prev);
  if (!surfhead) {
    GCon->Log(NAME_Error, "FixSegSurfaceTJunctions: orphaned surface!");
    return surf;
  }

  if (lastRenderQuality) {
    //if ((int)(ptrdiff_t)(sub-&Level->Subsectors[0]) != 40) return surf;

    // mark adjacent subsector flats for fixing
    // nope, `MarkAdjacentTJunctions()` should take care of it

    const bool isLM = IsLightmapRenderer();

    const line_t *line = myseg->linedef;
    if (line) {
      /*
        we need adjacent segs to process.

        for starting/ending seg of a line, we need to collect the corresponding segs
        from adjacent lines (but we need only segs which faces the same subsector).

        for middle seg, it is enough to process left and right line segs.
      */

      //FIXME: we should process only segs we are interesting in...
      //       it is safe to process segs that will not contribute,
      //       but it's just a waste of time

      // proces segs from our own line
      for (seg_t *lseg = line->firstseg; lseg; lseg = lseg->lsnext) {
        // do not ignore our seg, it may contain some interesting surfaces too
        if (lseg->frontsub == sub && lseg->drawsegs) {
          AddPointsFromDrawseg(lseg->drawsegs, surf, *surfhead, prev, nullptr);
        }
      }

      // proces segs from adjacent lines
      for (int lvidx = 0; lvidx < 2; ++lvidx) {
        const int len = line->vxCount(lvidx);
        for (int f = 0; f < len; ++f) {
          const line_t *ll = line->vxLine(lvidx, f);
          for (seg_t *lseg = ll->firstseg; lseg; lseg = lseg->lsnext) {
            // all subs, why not
            AddPointsFromDrawseg(lseg->drawsegs, surf, *surfhead, prev, nullptr);
            // no need to process partners, as they are belong to the same linedef anyway
          }
        }
      }
    }

    // insert floor surface points into walls
    if (isLM) {
      // our subsector flats
      AddPointsFromAllRegions(sub, surf, *surfhead, prev, nullptr);
    }

    // and adjacent subsectors flats/segs
    seg_t *xseg = &Level->Segs[sub->firstline];
    for (int f = sub->numlines; f--; ++xseg) {
      // seg
      AddPointsFromDrawseg(xseg->drawsegs, surf, *surfhead, prev, nullptr);
      // front subsector
      if (isLM && xseg->frontsub != sub) {
        AddPointsFromAllRegions(xseg->frontsub, surf, *surfhead, prev, nullptr);
      }
      // back subsector
      if (xseg->partner) {
        AddPointsFromDrawseg(xseg->partner->drawsegs, surf, *surfhead, prev, nullptr);
        if (isLM && xseg->partner->frontsub != sub) {
          AddPointsFromAllRegions(xseg->partner->frontsub, surf, *surfhead, prev, nullptr);
        }
      }
    }
  } else {
    surf->RemoveTJVerts();
    surf->ResetFixCracks();
  }

  return FixSurfCentroid(surf, *surfhead, prev);
}


//==========================================================================
//
//  VRenderLevelShared::FixSubFlatSurfaceTJunctions
//
//==========================================================================
surface_t *VRenderLevelShared::FixSubFlatSurfaceTJunctions (surface_t *surf, subsector_t *sub) {
  if (!surf) return nullptr;

  surf->RemoveTJVerts();
  surf->ResetFixCracks();

  if (!sub) return surf;

  vassert(sub == surf->subsector);

  if (sub->isOriginalPObj()) return surf; // nobody cares

  surface_t *prev;
  surface_t **surfhead = FindSubSurfHead(surf, sub, &prev);
  if (!surfhead) {
    GCon->Log(NAME_Error, "FixSubFlatSurfaceTJunctions: orphaned surface!");
    return surf;
  }

  if (lastRenderQuality && IsLightmapRenderer()) {
    // other subsector surfaces
    AddPointsFromAllRegions(sub, surf, *surfhead, prev, nullptr);

    // fix flat surface using subsector segs.
    // this is slightly more work than needed, but meh
    // we also need to check adjacent subsectors, and process
    // their flats too, but only if they have the same height.
    // currently, we will simply process everything, but this should be optimized.
    seg_t *xseg = &Level->Segs[sub->firstline];
    for (int f = sub->numlines; f--; ++xseg) {
      if (xseg->drawsegs) AddPointsFromDrawseg(xseg->drawsegs, surf, *surfhead, prev, nullptr);
      // neighbour subsectors surfaces
      // this is not required if the other sector has different floor or
      // ceiling height, but meh... let's play safe here
      if (xseg->frontsub != sub) {
        AddPointsFromAllRegions(xseg->frontsub, surf, *surfhead, prev, nullptr);
      }
      if (xseg->partner && xseg->partner->frontsub != sub) {
        AddPointsFromDrawseg(xseg->partner->drawsegs, surf, *surfhead, prev, nullptr);
        AddPointsFromAllRegions(xseg->partner->frontsub, surf, *surfhead, prev, nullptr);
      }
    }
  }

  return FixSurfCentroid(surf, *surfhead, prev);
}


//==========================================================================
//
//  VRenderLevelShared::SurfaceFixTJunctions
//
//==========================================================================
void VRenderLevelShared::SurfaceFixTJunctions (surface_t *&surf) {
  if (surf) {
    if (surf->IsFixCracks() || (!lastRenderQuality && surf->isCentroidCreated())) {
      surf->RemoveTJVerts();
      surf->ResetFixCracks();
      if (surf->seg) surf = FixSegSurfaceTJunctions(surf, surf->seg);
      if (surf->sreg) surf = FixSubFlatSurfaceTJunctions(surf, surf->subsector);
    }
  }
}
