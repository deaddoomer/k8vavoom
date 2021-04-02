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
  if (wvcount < 3) return nullptr;
  if (wvcount == 4 && (wv[1].z <= wv[0].z && wv[2].z <= wv[3].z)) return nullptr;
  if (wvcount > surface_t::MAXWVERTS) Sys_Error("cannot create huge world surface (the thing that should not be)");

  if (!texinfo->Tex || texinfo->Tex->Type == TEXTYPE_Null) return nullptr;

  surface_t *surf = NewWSurf(wvcount);
  surf->subsector = sub;
  surf->seg = seg;
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
  //surf = FixSegTJunctions(SubdivideSeg(surf, texinfo->saxisLM, &texinfo->taxisLM, seg), seg);
  surf = SubdivideSeg(FixSegTJunctions(surf, seg), texinfo->saxisLM, &texinfo->taxisLM, seg);
  surf = FixSegSurfaceTJunctions(surf, seg);
  InitSurfs(true, surf, texinfo, seg, sub); // recalc static lightmaps
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


//#define VV_QUAD_SPLIT_DEBUG

//surface_t::TF_TOP
//surface_t::TF_BOTTOM
//surface_t::TF_MIDDLE

//==========================================================================
//
//  VRenderLevelShared::CreateWorldSurfFromWVSplit
//
//  split quad to parts unobscured by 3d walls
//
//  quad points (usually) are:
//   [0]: left bottom point
//   [1]: left top point
//   [2]: right top point
//   [3]: right bottom point
//
//==========================================================================
void VRenderLevelShared::CreateWorldSurfFromWVSplit (sector_t *clipsec, subsector_t *sub, seg_t *seg, segpart_t *sp, TVec quad[4], vuint32 typeFlags) noexcept {
  if (!isValidNormalQuad(quad)) return;

  if (!seg->linedef || !clipsec || !seg || seg->pobj || clipsec->isAnyPObj() || !clipsec->Has3DFloors()) {
    return CreateWorldSurfFromWV(sub, seg, sp, quad, typeFlags);
  }

  TVec orig[4];
  memcpy((void *)orig, quad, sizeof(orig));

  #ifdef VV_QUAD_SPLIT_DEBUG
  const int lidx = (int)(ptrdiff_t)(seg->linedef-&Level->Lines[0]);
  const bool doDump =
    lidx == 2662 /*|| lidx == 1050 || lidx == 1051*/ ||
    false;
  #endif

  #ifdef VV_QUAD_SPLIT_DEBUG
  if (doDump) {
    GCon->Logf(NAME_Debug, "***CreateWorldSurfFromWVSplit: seg #%d, line #%d, clipsec #%d; quad=(%g,%g,%g):(%g,%g,%g):(%g,%g,%g):(%g,%g,%g)",
      (int)(ptrdiff_t)(seg-&Level->Segs[0]),
      (int)(ptrdiff_t)(seg->linedef-&Level->Lines[0]),
      (int)(ptrdiff_t)(clipsec-&Level->Sectors[0]),
      quad[0].x, quad[0].y, quad[0].z,
      quad[1].x, quad[1].y, quad[1].z,
      quad[2].x, quad[2].y, quad[2].z,
      quad[3].x, quad[3].y, quad[3].z);
    GCon->Log(NAME_Debug, "======== REGIONS ========");
    for (const sec_region_t *rr = clipsec->eregions; rr; rr = rr->next) {
      VLevel::DumpRegion(rr, true);
      const line_t *ld = rr->extraline;
      if (!ld) continue;
      GCon->Logf(NAME_Debug, "  eline #%d; fsec=%d; bsec=%d; alpha=%g; 2s=%d",
        (int)(ptrdiff_t)(ld-&Level->Lines[0]),
        (ld->frontsector ? (int)(ptrdiff_t)(ld->frontsector-&Level->Sectors[0]) : -1),
        (ld->backsector ? (int)(ptrdiff_t)(ld->backsector-&Level->Sectors[0]) : -1),
        ld->alpha, (ld->flags&ML_TWOSIDED ? 1 : 0));
      const side_t *sd = (ld->sidenum[0] >= 0 ? &Level->Sides[ld->sidenum[0]] : nullptr);
      if (sd) {
        VTexture *tt = GTextureManager(sd->TopTexture);
        VTexture *bt = GTextureManager(sd->BottomTexture);
        VTexture *mt = GTextureManager(sd->MidTexture);
        GCon->Logf(NAME_Debug, "    front side: toptex=%s (%u); bottex=%s (%u); midtex=%s (%u)",
          (tt ? *tt->Name : "<none>"), sd->TopTexture.id,
          (bt ? *bt->Name : "<none>"), sd->BottomTexture.id,
          (mt ? *mt->Name : "<none>"), sd->MidTexture.id);
      }
      sd = (ld->sidenum[1] >= 0 ? &Level->Sides[ld->sidenum[1]] : nullptr);
      if (sd) {
        VTexture *tt = GTextureManager(sd->TopTexture);
        VTexture *bt = GTextureManager(sd->BottomTexture);
        VTexture *mt = GTextureManager(sd->MidTexture);
        GCon->Logf(NAME_Debug, "    back side: toptex=%s (%u); bottex=%s (%u); midtex=%s (%u)",
          (tt ? *tt->Name : "<none>"), sd->TopTexture.id,
          (bt ? *bt->Name : "<none>"), sd->BottomTexture.id,
          (mt ? *mt->Name : "<none>"), sd->MidTexture.id);
      }
    }
    GCon->Log(NAME_Debug, "=========================");
  }
  #endif

  for (;;) {
    #ifdef VV_QUAD_SPLIT_DEBUG
    if (doDump) {
      GCon->Logf(NAME_Debug, "--- original: (%g,%g,%g):(%g,%g,%g):(%g,%g,%g):(%g,%g,%g) (ok=%d)",
        quad[0].x, quad[0].y, quad[0].z,
        quad[1].x, quad[1].y, quad[1].z,
        quad[2].x, quad[2].y, quad[2].z,
        quad[3].x, quad[3].y, quad[3].z,
        (int)isValidNormalQuad(quad));
    }
    #endif
    sec_region_t *creg = SplitQuadWithRegionsBottom(quad, clipsec);
    #ifdef VV_QUAD_SPLIT_DEBUG
    if (doDump) {
      GCon->Logf(NAME_Debug, "--- split to: (%g,%g,%g):(%g,%g,%g):(%g,%g,%g):(%g,%g,%g) (ok=%d)",
        quad[0].x, quad[0].y, quad[0].z,
        quad[1].x, quad[1].y, quad[1].z,
        quad[2].x, quad[2].y, quad[2].z,
        quad[3].x, quad[3].y, quad[3].z,
        (int)isValidNormalQuad(quad));
      VLevel::DumpRegion(creg, true);
    }
    #endif
    CreateWorldSurfFromWV(sub, seg, sp, quad, typeFlags);
    if (!creg) return; // done with it
    // start with clip region top
    const float tzv1 = creg->eceiling.GetPointZ(*seg->v1);
    const float tzv2 = creg->eceiling.GetPointZ(*seg->v2);
    #ifdef VV_QUAD_SPLIT_DEBUG
    if (doDump) {
      GCon->Logf(NAME_Debug, "---  tzv1=%g (v1top=%g); tzv2=%g (v2top=%g); exit=%d", tzv1, orig[QUAD_V1_TOP].z, tzv2, orig[QUAD_V2_TOP].z, (int)(tzv1 >= orig[QUAD_V1_TOP].z && tzv2 >= orig[QUAD_V2_TOP].z));
    }
    #endif
    if (tzv1 >= orig[QUAD_V1_TOP].z && tzv2 >= orig[QUAD_V2_TOP].z) return; // out of quad
    orig[QUAD_V1_BOTTOM].z = tzv1;
    orig[QUAD_V2_BOTTOM].z = tzv2;
    #ifdef VV_QUAD_SPLIT_DEBUG
    if (doDump) {
      GCon->Logf(NAME_Debug, "--- new quad: (%g,%g,%g):(%g,%g,%g):(%g,%g,%g):(%g,%g,%g) (ok=%d)",
        orig[0].x, orig[0].y, orig[0].z,
        orig[1].x, orig[1].y, orig[1].z,
        orig[2].x, orig[2].y, orig[2].z,
        orig[3].x, orig[3].y, orig[3].z,
        (int)isValidNormalQuad(orig));
    }
    #endif
    if (!isValidNormalQuad(orig)) return;
    memcpy((void *)quad, orig, sizeof(orig));
  }
}


//==========================================================================
//
//  VRenderLevelShared::CreateWorldSurfFromWVSplitFromReg
//
//  split quad to parts unobscured by 3d walls
//
//  quad points (usually) are:
//   [0]: left bottom point
//   [1]: left top point
//   [2]: right top point
//   [3]: right bottom point
//
//==========================================================================
void VRenderLevelShared::CreateWorldSurfFromWVSplitFromReg (sec_region_t *reg, sector_t *bsec, subsector_t *sub, seg_t *seg, segpart_t *sp, TVec quad[4], vuint32 typeFlags) noexcept {
  if (!isValidNormalQuad(quad)) return;

  if (bsec && bsec->isAnyPObj()) bsec = nullptr;

  if (!reg && bsec) return CreateWorldSurfFromWVSplit(bsec, sub, seg, sp, quad, typeFlags);

  if (!reg || !seg->linedef || !seg || seg->pobj) {
    return CreateWorldSurfFromWV(sub, seg, sp, quad, typeFlags);
  }

  TVec orig[4];
  memcpy((void *)orig, quad, sizeof(orig));

  #ifdef VV_QUAD_SPLIT_DEBUG
  const int lidx = (int)(ptrdiff_t)(seg->linedef-&Level->Lines[0]);
  const bool doDump =
    lidx == 2662 /*|| lidx == 1050 || lidx == 1051*/ ||
    false;
  #endif

  #ifdef VV_QUAD_SPLIT_DEBUG
  if (doDump) {
    GCon->Logf(NAME_Debug, "***CreateWorldSurfFromWVSplitFromReg: seg #%d, line #%d, quad=(%g,%g,%g):(%g,%g,%g):(%g,%g,%g):(%g,%g,%g)",
      (int)(ptrdiff_t)(seg-&Level->Segs[0]),
      (int)(ptrdiff_t)(seg->linedef-&Level->Lines[0]),
      quad[0].x, quad[0].y, quad[0].z,
      quad[1].x, quad[1].y, quad[1].z,
      quad[2].x, quad[2].y, quad[2].z,
      quad[3].x, quad[3].y, quad[3].z);
  }
  #endif

  for (;;) {
    #ifdef VV_QUAD_SPLIT_DEBUG
    if (doDump) {
      GCon->Logf(NAME_Debug, "--- original: (%g,%g,%g):(%g,%g,%g):(%g,%g,%g):(%g,%g,%g)",
        quad[0].x, quad[0].y, quad[0].z,
        quad[1].x, quad[1].y, quad[1].z,
        quad[2].x, quad[2].y, quad[2].z,
        quad[3].x, quad[3].y, quad[3].z);
    }
    #endif
    sec_region_t *creg = SplitQuadWithRegionsBottom(quad, reg);
    #ifdef VV_QUAD_SPLIT_DEBUG
    if (doDump) {
      GCon->Logf(NAME_Debug, "--- split to: (%g,%g,%g):(%g,%g,%g):(%g,%g,%g):(%g,%g,%g)",
        quad[0].x, quad[0].y, quad[0].z,
        quad[1].x, quad[1].y, quad[1].z,
        quad[2].x, quad[2].y, quad[2].z,
        quad[3].x, quad[3].y, quad[3].z);
      VLevel::DumpRegion(creg, true);
    }
    #endif
    if (bsec) {
      CreateWorldSurfFromWVSplit(bsec, sub, seg, sp, quad, typeFlags);
    } else {
      CreateWorldSurfFromWV(sub, seg, sp, quad, typeFlags);
    }
    if (!creg) return; // done with it
    // start with clip region top
    const float tzv1 = creg->eceiling.GetPointZ(*seg->v1);
    const float tzv2 = creg->eceiling.GetPointZ(*seg->v2);
    if (tzv1 >= orig[QUAD_V1_TOP].z && tzv2 >= orig[QUAD_V2_TOP].z) return; // out of quad
    orig[QUAD_V1_BOTTOM].z = tzv1;
    orig[QUAD_V2_BOTTOM].z = tzv2;
    if (!isValidNormalQuad(orig)) return;
    memcpy((void *)quad, orig, sizeof(orig));
  }
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
