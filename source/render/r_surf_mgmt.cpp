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

//#define VV_ENSURE_POINTS_DEBUG
#define VV_USE_PROPER_CENTROID


//==========================================================================
//
//  surface_t::FreeLightmaps
//
//==========================================================================
void surface_t::FreeLightmaps () noexcept {
  if (lightmap) {
    light_mem -= lmsize;
    Z_Free(lightmap);
    lightmap = nullptr;
    lmsize = 0;
  }
  if (lightmap_rgb) {
    light_mem -= lmrgbsize;
    Z_Free(lightmap_rgb);
    lightmap_rgb = nullptr;
    lmrgbsize = 0;
  }
}


//==========================================================================
//
//  surface_t::FreeRGBLightmap
//
//==========================================================================
void surface_t::FreeRGBLightmap () noexcept {
  if (lightmap_rgb) {
    light_mem -= lmrgbsize;
    Z_Free(lightmap_rgb);
    lightmap_rgb = nullptr;
    lmrgbsize = 0;
  }
}


//==========================================================================
//
//  surface_t::ReserveMonoLightmap
//
//==========================================================================
void surface_t::ReserveMonoLightmap (int sz) noexcept {
  vassert(sz > 0);
  if (lmsize < sz) {
    light_mem -= lmsize;
    light_mem += sz;
    lightmap = (vuint8 *)Z_Realloc(lightmap, sz);
    lmsize = sz;
  }
}


//==========================================================================
//
//  surface_t::ReserveRGBLightmap
//
//==========================================================================
void surface_t::ReserveRGBLightmap (int sz) noexcept {
  vassert(sz > 0);
  if (lmrgbsize < sz) {
    light_mem -= lmrgbsize;
    light_mem += sz;
    lmrgbsize = sz;
    lightmap_rgb = (rgb_t *)Z_Realloc(lightmap_rgb, sz);
  }
}


//==========================================================================
//
//  surface_t::CalculateFastCentroid
//
//==========================================================================
TVec surface_t::CalculateFastCentroid () const noexcept {
  TVec cp(0.0f, 0.0f, 0.0f);
  if (count > 0) {
    const SurfVertex *sf = &verts[0];
    if (plane.normal.z != 0.0f) {
      // flat
      for (int f = count; f--; ++sf) {
        cp.x += sf->x;
        cp.y += sf->y;
      }
      cp.x /= (float)count;
      cp.y /= (float)count;
      cp.z = plane.GetPointZ(cp);
    } else {
      // wall
      for (int f = count; f--; ++sf) {
        cp.x += sf->x;
        cp.y += sf->y;
        cp.z += sf->z;
      }
      cp.x /= (float)count;
      cp.y /= (float)count;
      cp.z /= (float)count;
      // just in case, project it on the plane
      if (plane.PointDistance(cp) != 0.0f) cp = plane.Project(cp);
    }
  }
  return cp;
}


//==========================================================================
//
//  surface_t::CalculateRealCentroid
//
//  surfaces can contain split points on the same line
//  if i'll simply sum all vertices, our center could be not right
//  so i will use only "turning points" (as it should be)
//
//==========================================================================
TVec surface_t::CalculateRealCentroid () const noexcept {
  // surfaces with 3 vertices are usually proper triangles,
  // don't bother with complex calculations in this case
  if (count <= 3) return CalculateFastCentroid();
  TVec cp(0.0f, 0.0f, 0.0f); // accumulator, and final result
  TVec pdir(0.0f, 0.0f, 0.0f); // last direction
  const SurfVertex *sf = &verts[0];
  TVec lastv1(sf->x, sf->y, sf->z);
  ++sf;
  TVec prevpt(lastv1); // previous vertex
  int ccnt = 0;
  for (int f = count+1; f--; ++sf) {
    const TVec cv = sf->vec();
    if (!ccnt) {
      // first one
      pdir = cv-lastv1;
      if (pdir.lengthSquared() >= 0.2f*0.2f) {
        pdir = pdir.normalised();
        cp += lastv1;
        ccnt = 1;
      }
    } else {
      // are we making a turn here?
      TVec xdir = cv-lastv1;
      if (xdir.lengthSquared() >= 0.2f*0.2f) {
        xdir = xdir.normalised();
        // dot product for two normalized vectors is cosine between them
        // cos(0) is 1, so if it isn't, we have a turn
        // cos(180) is -1, but it doesn't matter here
        if (fabsf(xdir.dot(pdir)) < 1.0f-0.0001f) {
          // looks like we did made a turn
          // we have a new point
          // remember it
          lastv1 = prevpt;
          // add it to the sum
          cp += lastv1;
          ++ccnt;
          // and remember new direction
          pdir = (cv-lastv1).normalised();
        }
      }
    }
    prevpt = cv;
  }
  // if we have less than three points, something's gone wrong...
  if (ccnt < 3) return CalculateFastCentroid();
  cp = cp/(float)ccnt;
  if (plane.normal.z != 0.0f) {
    cp.z = plane.GetPointZ(cp);
  } else {
    // just in case, project it on the plane
    if (plane.PointDistance(cp) != 0.0f) cp = plane.Project(cp);
  }
  return cp;
}


//==========================================================================
//
//  surface_t::AddCentroid
//
//  there should be enough room for two new points
//
//==========================================================================
void surface_t::AddCentroid () noexcept {
  vassert(!isCentroidCreated());
  if (count >= 3) {
    #ifdef VV_USE_PROPER_CENTROID
    TVec cp(CalculateRealCentroid());
    #else
    TVec cp(CalculateFastCentroid());
    #endif
    InsertVertexAt(0, cp, nullptr, nullptr);
    setCentroidCreated();
    // and re-add the previous first point as the final one
    // (so the final triangle will be rendered too)
    // this is not required for quad, but required for "real" triangle fan
    // need to copy the point first, because we're passing a reference to it
    cp = verts[1].vec();
    InsertVertexAt(count, cp, nullptr, nullptr);
    vassert(verts[1].vec() == verts[count-1].vec());
  }
}


//==========================================================================
//
//  surface_t::InsertVertexAt
//
//  there should be enough room for vertex
//
//==========================================================================
void surface_t::InsertVertexAt (int idx, const TVec &p, sec_surface_t *ownssf, seg_t *ownseg) noexcept {
  vassert(idx >= 0 && idx <= count);
  if (idx < count) memmove((void *)(&verts[idx+1]), (void *)(&verts[idx]), (count-idx)*sizeof(verts[0]));
  ++count;
  SurfVertex *dv = &verts[idx];
  memset((void *)dv, 0, sizeof(*dv)); // just in case
  dv->x = p.x;
  dv->y = p.y;
  dv->z = p.z;
  dv->ownerssf = ownssf;
  dv->ownerseg = ownseg;
}


//==========================================================================
//
//  surface_t::RemoveVertexAt
//
//==========================================================================
void surface_t::RemoveVertexAt (int idx) noexcept {
  vassert(idx >= 0 && idx < count);
  if (idx < count-1) memmove((void *)(&verts[idx]), (void *)(&verts[idx+1]), (count-idx-1)*sizeof(verts[0]));
  --count;
}


//==========================================================================
//
//  surface_t::RemoveCentroid
//
//==========================================================================
void surface_t::RemoveCentroid () noexcept {
  if (isCentroidCreated()) {
    vassert(count > 3);
    vassert(verts[1].vec() == verts[count-1].vec());
    resetCentroidCreated();
    // `-2`, because we don't need the last point
    memmove((void *)&verts[0], (void *)&verts[1], (count-2)*sizeof(verts[0]));
    count -= 2;
  }
}


//==========================================================================
//
//  surface_t::RemoveSsfOwnVertices
//
//  remove all vertices with this owning subsector
//
//==========================================================================
void surface_t::RemoveSsfOwnVertices (const sec_surface_t *ssf) noexcept {
  if (!ssf) return;
  int idx = 0;
  while (idx < count) if (verts[idx].ownerssf == ssf) RemoveVertexAt(idx); else ++idx;
}


//==========================================================================
//
//  surface_t::RemoveSegOwnVertices
//
//  remove all vertices with this owning seg
//
//==========================================================================
void surface_t::RemoveSegOwnVertices (const seg_t *seg) noexcept {
  if (!seg) return;
  int idx = 0;
  while (idx < count) if (verts[idx].ownerseg == seg) RemoveVertexAt(idx); else ++idx;
}



//**************************************************************************
//**
//**  Sector surfaces
//**
//**************************************************************************

//==========================================================================
//
//  AppendSurfaces
//
//==========================================================================
static inline void AppendSurfaces (segpart_t *sp, surface_t *newsurfs) noexcept {
  vassert(sp);
  if (!newsurfs) return; // nothing to d
  // new list will start with `newsurfs`
  surface_t *ss = sp->surfs;
  if (ss) {
    // append
    while (ss->next) ss = ss->next;
    ss->next = newsurfs;
  } else {
    sp->surfs = newsurfs;
  }
}


//**************************************************************************
//**
//**  Seg surfaces
//**
//**************************************************************************

//==========================================================================
//
//  VRenderLevelShared::NewWSurf
//
//==========================================================================
surface_t *VRenderLevelShared::NewWSurf (int vcount) noexcept {
  vassert(vcount >= 0);
  enum { WSURFSIZE = sizeof(surface_t)+sizeof(SurfVertex)*(surface_t::MAXWVERTS-1) };

  // do we need to dynamically allocate a surface?
  if (vcount > surface_t::MAXWVERTS) {
    const int vcnt = (vcount|0x0f)+1;
    surface_t *res = (surface_t *)Z_Calloc(sizeof(surface_t)+(vcnt-1)*sizeof(SurfVertex));
    res->alloted = vcnt;
    res->count = vcount;
    return res;
  }

  // fits into "standard" world surface
  if (!free_wsurfs) {
    // allocate some more surfs
    vuint8 *tmp = (vuint8 *)Z_Calloc(WSURFSIZE*4096+sizeof(void *));
    *(void **)tmp = AllocatedWSurfBlocks;
    AllocatedWSurfBlocks = tmp;
    tmp += sizeof(void *);
    for (int i = 0; i < 4096; ++i, tmp += WSURFSIZE) {
      ((surface_t *)tmp)->next = free_wsurfs;
      free_wsurfs = (surface_t *)tmp;
    }
  }

  surface_t *surf = free_wsurfs;
  free_wsurfs = surf->next;

  memset((void *)surf, 0, WSURFSIZE);
  surf->allocflags = surface_t::ALLOC_WORLD;

  surf->alloted = surface_t::MAXWVERTS;
  surf->count = vcount;
  return surf;
}


//==========================================================================
//
//  VRenderLevelShared::EnsureSurfacePoints
//
//  used to grow surface in lightmap t-junction fixer
//  returns new surface, and may modify `listhead` (and `pref->next`)
//  if `prev` is `nullptr`, will try to find the proper `prev`
//  vertex counter is not changed
//
//==========================================================================
surface_t *VRenderLevelShared::EnsureSurfacePoints (surface_t *surf, int vcount, surface_t *&listhead, surface_t *prev) noexcept {
  if (vcount <= surf->alloted) return surf; // nothing to do
  #ifdef VV_ENSURE_POINTS_DEBUG
  if (surf->isCentroidCreated()) {
    vassert(surf->verts[1].vec() == surf->verts[surf->count-1].vec());
  }
  // need new surface
  GCon->Logf(NAME_Debug, "allocating new surface; old count=%d; old alloted=%d; new count=%d; oldflags=0x%04x", surf->count, surf->alloted, vcount, surf->allocflags);
  #endif
  const size_t surfsize = sizeof(*surf)-sizeof(surf->verts[0])+surf->count*sizeof(surf->verts[0]);
  surface_t *snew = NewWSurf(vcount);
  if (vcount > surface_t::MAXWVERTS) {
    vassert(!snew->isWorldAllocated());
  } else {
    vassert(snew->isWorldAllocated());
  }
  const bool iswalloc = snew->isWorldAllocated();
  const int newalloted = snew->alloted;
  vassert(newalloted >= vcount);
  // copy old surface data
  memcpy((void *)snew, (void *)surf, surfsize);
  // fix new fields
  #ifdef VV_ENSURE_POINTS_DEBUG
  GCon->Logf(NAME_Debug, "allocated new surface; old count=%d; old alloted=%d; new count=%d; new alloted=%d; newflags=0x%04x", surf->count, surf->alloted, snew->count, snew->alloted, snew->allocflags);
  #endif
  if (iswalloc) {
    snew->allocflags |= surface_t::ALLOC_WORLD;
  } else {
    snew->allocflags &= ~surface_t::ALLOC_WORLD;
  }
  snew->alloted = newalloted;
  #ifdef VV_ENSURE_POINTS_DEBUG
  GCon->Logf(NAME_Debug, "allocated new surface; old count=%d; old alloted=%d; new count=%d; new alloted=%d; newflags=0x%04x", surf->count, surf->alloted, snew->count, snew->alloted, snew->allocflags);
  #endif
  // fix `prev` if necessary
  if (!prev && surf != listhead) {
    prev = listhead;
    while (prev->next != surf) prev = prev->next;
  }
  // reinsert into surface list
  if (prev) {
    vassert(prev->next == surf);
    prev->next = snew;
  } else {
    vassert(listhead == surf);
    listhead = snew;
  }
  // fix various caches
  for (surfcache_t *sc = snew->CacheSurf; sc; sc = sc->chain) {
    if (sc->surf == surf) sc->surf = snew;
  }
  // free old surface
  if (surf->isWorldAllocated()) {
    surf->next = free_wsurfs;
    free_wsurfs = surf;
  } else {
    Z_Free(surf);
  }
  // done
  #ifdef VV_ENSURE_POINTS_DEBUG
  if (snew->isCentroidCreated()) {
    vassert(snew->verts[1].vec() == snew->verts[snew->count-1].vec());
  }
  #endif
  return snew;
}


//==========================================================================
//
//  VRenderLevelShared::FreeWSurfs
//
//==========================================================================
void VRenderLevelShared::FreeWSurfs (surface_t *&InSurfs) noexcept {
  surface_t *surfs = InSurfs;
  FlushSurfCaches(surfs);
  while (surfs) {
    surfs->FreeLightmaps();
    surface_t *next = surfs->next;
    if (surfs->isWorldAllocated()) {
      surfs->next = free_wsurfs;
      free_wsurfs = surfs;
    } else {
      Z_Free(surfs);
    }
    surfs = next;
  }
  InSurfs = nullptr;
}


//==========================================================================
//
//  VRenderLevelShared::ReallocSurface
//
//  free all surfaces except the first one, clear first, set
//  number of vertices to vcount
//
//==========================================================================
surface_t *VRenderLevelShared::ReallocSurface (surface_t *surfs, int vcount) noexcept {
  vassert(vcount >= 0); // just in case
  surface_t *surf = surfs;
  if (surf) {
    if (vcount > surf->alloted) {
      FreeWSurfs(surf);
      return NewWSurf(vcount);
    }
    // free surface chain
    if (surf->next) { FreeWSurfs(surf->next); surf->next = nullptr; }
    if (surf->CacheSurf) FreeSurfCache(surf->CacheSurf);
    surf->FreeLightmaps();
    const unsigned oldaf = (surf->allocflags&surface_t::ALLOC_WORLD);
    memset((void *)surf, 0, sizeof(surface_t)+(vcount-1)*sizeof(SurfVertex));
    surf->allocflags = oldaf;
    surf->count = vcount;
    return surf;
  } else {
    return NewWSurf(vcount);
  }
}


//==========================================================================
//
//  VRenderLevelShared::CreateWSurf
//
//  this is used to create world/wall surface
//
//  quad points (usually) are:
//   [0]: left bottom point
//   [1]: left top point
//   [2]: right top point
//   [3]: right bottom point
//
//==========================================================================
surface_t *VRenderLevelShared::CreateWSurf (TVec *wv, texinfo_t *texinfo, seg_t *seg, subsector_t *sub, int wvcount, vuint32 typeFlags) noexcept {
  vassert(seg);
  if (wvcount < 3) return nullptr;
  if (wvcount == 4 && (wv[1].z <= wv[0].z && wv[2].z <= wv[3].z)) return nullptr;
  if (wvcount > surface_t::MAXWVERTS) Sys_Error("cannot create huge world surface (the thing that should not be)");

  if (!texinfo->Tex || texinfo->Tex->Type == TEXTYPE_Null) return nullptr;

  surface_t *surf = NewWSurf(wvcount);
  surf->subsector = sub;
  surf->seg = seg;
  surf->sreg = nullptr;
  surf->next = nullptr;
  surf->count = wvcount;
  surf->typeFlags = typeFlags;
  //memcpy(surf->verts, wv, wvcount*sizeof(SurfVertex));
  //memset((void *)surf->verts, 0, wvcount*sizeof(SurfVertex));
  for (int f = 0; f < wvcount; ++f) surf->verts[f].setVec(wv[f]);

  if (texinfo->Tex == GTextureManager[skyflatnum]) {
    // never split sky surfaces
    surf->texinfo = texinfo;
    surf->plane = *(TPlane *)seg;
    return surf;
  }

  //!GCon->Logf(NAME_Debug, "sfcS:%p: saxis=(%g,%g,%g); taxis=(%g,%g,%g); saxisLM=(%g,%g,%g); taxisLM=(%g,%g,%g)", surf, texinfo->saxis.x, texinfo->saxis.y, texinfo->saxis.z, texinfo->taxis.x, texinfo->taxis.y, texinfo->taxis.z, texinfo->saxisLM.x, texinfo->saxisLM.y, texinfo->saxisLM.z, texinfo->taxisLM.x, texinfo->taxisLM.y, texinfo->taxisLM.z);
  // fix wall t-junctions with common firxer first
  surf = FixSegTJunctions(surf, seg);
  // subdivide surface
  surf = SubdivideSeg(surf, texinfo->saxisLM, &texinfo->taxisLM, seg);
  // and fix t-junctions from subdivision
  surf = FixSegSurfaceTJunctions(surf, seg);
  InitSurfs(true, surf, texinfo, seg, sub, seg, nullptr); // recalc static lightmaps
  return surf;
}


//==========================================================================
//
//  VRenderLevelShared::CreateWorldSurfFromWV
//
//  quad points (usually) are:
//   [0]: left bottom point
//   [1]: left top point
//   [2]: right top point
//   [3]: right bottom point
//
//==========================================================================
void VRenderLevelShared::CreateWorldSurfFromWV (subsector_t *sub, seg_t *seg, segpart_t *sp, TVec quad[4], vuint32 typeFlags) noexcept {
  if (!isValidQuad(quad)) return;

  TVec *wstart = quad;
  int wcount = 4;
  if (quad[0].z == quad[1].z) {
    // can reduce to triangle
    wstart = quad+1;
    wcount = 3;
  } else if (quad[2].z == quad[3].z) {
    // can reduce to triangle
    wstart = quad;
    wcount = 3;
  }

  AppendSurfaces(sp, CreateWSurf(wstart, &sp->texinfo, seg, sub, wcount, typeFlags));
}


//==========================================================================
//
//  VRenderLevelShared::CreateWorldSurfFromWVSplit
//
//  split quad to parts unobscured by 3d walls
//
//==========================================================================
void VRenderLevelShared::CreateWorldSurfFromWVSplit (subsector_t *sub, seg_t *seg, segpart_t *sp, TVec quad[4], vuint32 typeFlags,
                                                     bool onlySolid, const sec_region_t *ignorereg) noexcept
{
  if (!isValidQuad(quad)) return;

  if (!seg->linedef || seg->pobj || sub->isAnyPObj()) {
    return CreateWorldSurfFromWV(sub, seg, sp, quad, typeFlags);
  }

  vassert(seg->frontsub == sub);
  if (seg->frontsector != sub->sector) {
    GCon->Logf(NAME_Error, "seg of line #%d: frontsector=%d; sub->sector=%d",
      (int)(ptrdiff_t)(seg->linedef-&Level->Lines[0]),
      (seg->frontsector ? (int)(ptrdiff_t)(seg->frontsector-&Level->Sectors[0]) : -1),
      (sub->sector ? (int)(ptrdiff_t)(sub->sector-&Level->Sectors[0]) : -1));
  }
  //vassert(seg->frontsector == sub->sector);

  sector_t *clipsec = seg->backsector;
  if (clipsec && (clipsec->isAnyPObj() || !clipsec->Has3DFloors())) clipsec = nullptr;

  sector_t *frontsec = seg->frontsector;
  if (frontsec && (frontsec->isAnyPObj() || !frontsec->Has3DFloors())) frontsec = nullptr;

  if (!clipsec && !frontsec) {
    return CreateWorldSurfFromWV(sub, seg, sp, quad, typeFlags);
  }

  //const bool dump = ((ptrdiff_t)(seg->linedef-&Level->Lines[0]) == 20883);
  enum { dump = false };

  if (dump) {
    GCon->Log(NAME_Debug, "========================================");

    GCon->Logf(NAME_Debug, "  frontsec=%d; clipsec=%d; onlySolid=%d",
      (frontsec ? (int)(ptrdiff_t)(frontsec-&Level->Sectors[0]) : -1),
      (clipsec ? (int)(ptrdiff_t)(clipsec-&Level->Sectors[0]) : -1),
      (int)onlySolid);

    GCon->Logf(NAME_Debug, "*** seg of line #%d: side=%d; frontsub=%d; partner=%d; frontsec=%d; backsec=%d",
      (int)(ptrdiff_t)(seg->linedef-&Level->Lines[0]),
      seg->side, (int)(ptrdiff_t)(seg->frontsub-&Level->Subsectors[0]),
      (seg->partner ? 1 : 0),
      (int)(ptrdiff_t)(seg->frontsector-&Level->Sectors[0]),
      (seg->backsector ? (int)(ptrdiff_t)(seg->backsector-&Level->Sectors[0]) : -1));

    DumpQuad("quad", quad);

    if (ignorereg) {
      GCon->Log(NAME_Debug, "--- IGNORE REGION ---");
      VLevel::DumpRegion(ignorereg, true);
      GCon->Log(NAME_Debug, "---------------------");
    }

    GCon->Logf(NAME_Debug, "--- FRONT REGIONS (sector #%d) ---", (int)(ptrdiff_t)(seg->frontsector-&Level->Sectors[0]));
    for (const sec_region_t *reg = seg->frontsector->eregions; reg; reg = reg->next) VLevel::DumpRegion(reg, true);

    if (seg->backsector) {
      GCon->Logf(NAME_Debug, "--- BACK REGIONS (sector #%d) ---", (int)(ptrdiff_t)(seg->backsector-&Level->Sectors[0]));
      for (const sec_region_t *reg = seg->backsector->eregions; reg; reg = reg->next) VLevel::DumpRegion(reg, true);
    }

    GCon->Log(NAME_Debug, "========================================");
  }

  return CreateWorldSurfFromWVSplitFromReg((clipsec ? clipsec->eregions->next : nullptr), (frontsec ? frontsec->eregions->next : nullptr), sub, seg, sp, quad, typeFlags, onlySolid, ignorereg);
}


//==========================================================================
//
//  VRenderLevelShared::RecursiveQuadSplit
//
//==========================================================================
void VRenderLevelShared::RecursiveQuadSplit (const QSplitInfo &nfo, sec_region_t *frontreg, sec_region_t *backreg, TVec quad[4]) {
  if (!isValidQuad(quad)) return; // nothing to do anymore

  do {
    if (!frontreg && !backreg) return CreateWorldSurfFromWV(nfo.sub, nfo.seg, nfo.sp, quad, nfo.typeFlags); // we're done
    if (!frontreg) { frontreg = backreg; backreg = nullptr; } // just in case
    // skip regions we cannot process
    for (; frontreg; frontreg = frontreg->next) {
      if (frontreg == nfo.ignorereg) continue;
      if (frontreg->regflags&nfo.mask) continue;
      if (nfo.onlySolid && !frontreg->isBlockingExtraLine()) continue;
      if (frontreg->efloor.GetRealDist() >= frontreg->eceiling.GetRealDist()) continue;
      break;
    }
  } while (!frontreg);

  TVec newquad[4];

  // process bottom part
  if (SplitQuadWithPlane(quad, frontreg->efloor, newquad, nullptr)) {
    RecursiveQuadSplit(nfo, frontreg->next, backreg, newquad);
  }

  // process top part
  if (!SplitQuadWithPlane(quad, frontreg->eceiling, nullptr, quad)) return;
  return RecursiveQuadSplit(nfo, frontreg->next, backreg, quad);
}


//==========================================================================
//
//  VRenderLevelShared::CreateWorldSurfFromWVSplitFromReg
//
//==========================================================================
void VRenderLevelShared::CreateWorldSurfFromWVSplitFromReg (sec_region_t *frontreg, sec_region_t *backreg, subsector_t *sub, seg_t *seg, segpart_t *sp,
                                                            TVec quad[4], vuint32 typeFlags,
                                                            bool onlySolid, const sec_region_t *ignorereg) noexcept
{
  if (!isValidQuad(quad)) return;

  if (!frontreg && !backreg) return CreateWorldSurfFromWV(sub, seg, sp, quad, typeFlags);

  // just in case
  if (!seg || !seg->linedef || seg->pobj) return CreateWorldSurfFromWV(sub, seg, sp, quad, typeFlags);

  QSplitInfo nfo;
  nfo.sub = sub;
  nfo.seg = seg;
  nfo.sp = sp;
  nfo.typeFlags = typeFlags;
  nfo.onlySolid = onlySolid;
  nfo.ignorereg = ignorereg;
  nfo.mask = ((onlySolid ? sec_region_t::RF_NonSolid : 0u)|sec_region_t::RF_OnlyVisual|sec_region_t::RF_BaseRegion);

  return RecursiveQuadSplit(nfo, frontreg, backreg, quad);
}


//==========================================================================
//
//  VRenderLevelShared::FlushSurfCaches
//
//==========================================================================
void VRenderLevelShared::FlushSurfCaches (surface_t *InSurfs) noexcept {
  surface_t *surfs = InSurfs;
  while (surfs) {
    if (surfs->CacheSurf) FreeSurfCache(surfs->CacheSurf);
    surfs = surfs->next;
  }
}


//==========================================================================
//
//  VRenderLevelShared::FreeSurfaces
//
//==========================================================================
void VRenderLevelShared::FreeSurfaces (surface_t *InSurf) noexcept {
  /*
  surface_t *next;
  for (surface_t *s = InSurf; s; s = next) {
    if (s->CacheSurf) FreeSurfCache(s->CacheSurf);
    s->FreeLightmaps();
    next = s->next;
    Z_Free(s);
  }
  */
  FreeWSurfs(InSurf);
}


//==========================================================================
//
//  VRenderLevelShared::FreeSegParts
//
//==========================================================================
void VRenderLevelShared::FreeSegParts (segpart_t *ASP) noexcept {
  for (segpart_t *sp = ASP; sp; sp = sp->next) FreeWSurfs(sp->surfs);
}


//==========================================================================
//
//  InvalidateSegPart
//
//==========================================================================
void VRenderLevelShared::InvalidateSegPart (segpart_t *sp) noexcept {
  for (; sp; sp = sp->next) sp->SetFixTJunk();
}


//==========================================================================
//
//  InvalidateWholeSeg
//
//==========================================================================
void VRenderLevelShared::InvalidateWholeSeg (seg_t *seg) noexcept {
  //GCon->Logf(NAME_Debug, "*TRANSDOOR INVALIDATION; seg=%p; line %p", seg, seg->linedef);
  drawseg_t *ds = seg->drawsegs;
  if (ds) {
    InvalidateSegPart(ds->top);
    InvalidateSegPart(ds->mid);
    InvalidateSegPart(ds->bot);
    InvalidateSegPart(ds->topsky);
    InvalidateSegPart(ds->extra);
  }
}


//==========================================================================
//
//  VRenderLevelShared::InvaldateAllSegParts
//
//==========================================================================
void VRenderLevelShared::InvaldateAllSegParts () noexcept {
  segpart_t *sp = AllocatedSegParts;
  for (int f = NumSegParts; f--; ++sp) sp->SetFixTJunk(); // this forces recreation
}


//==========================================================================
//
//  VRenderLevelShared::MarkAdjacentTJunctions
//
//  (do not mark lines belongs only to `fsec`)
//  this doesn't seem to be required, and should not give a huge speedup
//  so i removed this check
//
//==========================================================================
void VRenderLevelShared::MarkAdjacentTJunctions (const sector_t *fsec, const line_t *line) noexcept {
  if (!line || line->pobj()) return; // no line, or polyobject line
  //vassert(fsec);
  const unsigned lineidx = (unsigned)(ptrdiff_t)(line-&Level->Lines[0]);
  if (tjLineMarkCheck[lineidx] == updateWorldFrame) return; // already processed at this frame
  tjLineMarkCheck[lineidx] = updateWorldFrame;
  //GCon->Logf(NAME_Debug, "mark tjunctions for line #%d", (int)(ptrdiff_t)(line-&Level->Lines[0]));
  // simply mark all adjacents for recreation
  for (int lvidx = 0; lvidx < 2; ++lvidx) {
    for (int f = 0; f < line->vxCount(lvidx); ++f) {
      const line_t *ln = line->vxLine(lvidx, f);
      if (ln == line) continue;
      // do not perform front sector checks, i'm not sure that i will not miss anything with them
      /*
      if (ln->frontsector) {
        if (ln->backsector) {
          // 2-sided
          if (ln->frontsector == fsec && ln->backsector == fsec) continue;
        } else {
          // 1-sided
          if (ln->frontsector == fsec) continue;
        }
      } else if (ln->backsector) {
        // very strange 1-sided
        if (ln->backsector == fsec) continue;
      }
      */
      const unsigned lnidx = (unsigned)(ptrdiff_t)(ln-&Level->Lines[0]);
      if (tjLineMarkFix[lnidx] == updateWorldFrame) continue; // already marked (and maybe processed) at this frame
      tjLineMarkFix[lnidx] = updateWorldFrame;
      //GCon->Logf(NAME_Debug, "  ...marking line #%d", (int)(ptrdiff_t)(ln-&Level->Lines[0]));
      // for each seg
      for (seg_t *ns = ln->firstseg; ns; ns = ns->lsnext) {
        // ignore "inner" seg (i.e. that one which doesn't start or end on line vertex)
        // this is to avoid introducing cracks in the middle of the wall that was splitted by BSP
        // we can be absolutely sure that vertices are reused, because we're creating segs by our own nodes builder
        if (ns->v1 != ln->v1 && ns->v1 != ln->v2 &&
            ns->v2 != ln->v1 && ns->v2 != ln->v2)
        {
          continue;
        }
        drawseg_t *ds = ns->drawsegs;
        if (ds) {
          // for each segpart
          InvalidateSegPart(ds->top);
          InvalidateSegPart(ds->mid);
          InvalidateSegPart(ds->bot);
          InvalidateSegPart(ds->topsky);
          // i'm pretty sure that we don't have to fix 3d floors
          //InvalidateSegPart(ds->extra);
        }
      }
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::MarkTJunctions
//
//==========================================================================
void VRenderLevelShared::MarkTJunctions (seg_t *seg) noexcept {
  if (seg->pobj) return; // don't do anything for polyobjects (for now)
  const line_t *line = seg->linedef;
  if (!line || line->pobj()) return; // miniseg, or polyobject line
  const sector_t *mysec = seg->frontsector;
  // just in case; also, more polyobject checks (skip sectors containing original polyobjs)
  if (!mysec || mysec->isAnyPObj()) return;
  const unsigned lineidx = (unsigned)(ptrdiff_t)(line-&Level->Lines[0]);
  if (tjLineMarkCheck[lineidx] == updateWorldFrame) return; // already processed at this frame
  return MarkAdjacentTJunctions(mysec, line);
}
