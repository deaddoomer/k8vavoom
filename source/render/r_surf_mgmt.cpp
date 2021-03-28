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
static void AppendSurfaces (segpart_t *sp, surface_t *newsurfs) noexcept {
  vassert(sp);
  if (!newsurfs) return; // nothing to do
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
  if (vcount > surface_t::MAXWVERTS) {
    const int vcnt = (vcount|3)+1;
    surface_t *res = (surface_t *)Z_Calloc(sizeof(surface_t)+(vcnt-1)*sizeof(SurfVertex));
    res->count = vcount;
    return res;
  }
  // fits into "standard" surface
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

  surf->count = vcount;
  return surf;
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
    const int maxcount = (surf->isWorldAllocated() ? surface_t::MAXWVERTS : surf->count);
    if (vcount > maxcount) {
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
  surf = FixSegTJunctions(SubdivideSeg(surf, texinfo->saxisLM, &texinfo->taxisLM, seg), seg);
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
void VRenderLevelShared::CreateWorldSurfFromWV (subsector_t *sub, seg_t *seg, segpart_t *sp, TVec quad[4], vuint32 typeFlags, bool doOffset) noexcept {
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

  //k8: HACK! HACK! HACK!
  //    move middle wall backwards a little, so it will be hidden behind up/down surfaces
  //    this is required for sectors with 3d floors, until i wrote a proper texture clipping math
  if (doOffset) for (unsigned f = 0; f < (unsigned)wcount; ++f) wstart[f] -= seg->normal*0.01f;

  AppendSurfaces(sp, CreateWSurf(wstart, &sp->texinfo, seg, sub, wcount, typeFlags));
}


//#define VV_QUAD_SPLIT_DEBUG

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
void VRenderLevelShared::CreateWorldSurfFromWVSplit (sector_t *clipsec, subsector_t *sub, seg_t *seg, segpart_t *sp, TVec quad[4], vuint32 typeFlags, bool doOffset) noexcept {
  if (!isValidQuad(quad)) return;

  if (!seg->linedef || !lastQuadSplit || !clipsec || !seg || seg->pobj || clipsec->isAnyPObj() || !clipsec->Has3DFloors()) {
    return CreateWorldSurfFromWV(sub, seg, sp, quad, typeFlags, doOffset);
  }

  TVec orig[4];
  memcpy(orig, quad, sizeof(orig));

  #ifdef VV_QUAD_SPLIT_DEBUG
  GCon->Logf(NAME_Debug, "***CreateWorldSurfFromWVSplit: seg #%d, line #%d, clipsec #%d; quad=(%g,%g,%g):(%g,%g,%g):(%g,%g,%g):(%g,%g,%g)",
    (int)(ptrdiff_t)(seg-&Level->Segs[0]),
    (int)(ptrdiff_t)(seg->linedef-&Level->Lines[0]),
    (int)(ptrdiff_t)(clipsec-&Level->Sectors[0]),
    quad[0].x, quad[0].y, quad[0].z,
    quad[1].x, quad[1].y, quad[1].z,
    quad[2].x, quad[2].y, quad[2].z,
    quad[3].x, quad[3].y, quad[3].z);
  #endif

  for (;;) {
    #ifdef VV_QUAD_SPLIT_DEBUG
    GCon->Logf(NAME_Debug, "--- original: (%g,%g,%g):(%g,%g,%g):(%g,%g,%g):(%g,%g,%g)",
      quad[0].x, quad[0].y, quad[0].z,
      quad[1].x, quad[1].y, quad[1].z,
      quad[2].x, quad[2].y, quad[2].z,
      quad[3].x, quad[3].y, quad[3].z);
    #endif
    sec_region_t *creg = ClipQuadWithRegions(quad, clipsec);
    #ifdef VV_QUAD_SPLIT_DEBUG
    GCon->Logf(NAME_Debug, "--- split to: (%g,%g,%g):(%g,%g,%g):(%g,%g,%g):(%g,%g,%g)",
      quad[0].x, quad[0].y, quad[0].z,
      quad[1].x, quad[1].y, quad[1].z,
      quad[2].x, quad[2].y, quad[2].z,
      quad[3].x, quad[3].y, quad[3].z);
    VLevel::DumpRegion(creg, true);
    #endif
    CreateWorldSurfFromWV(sub, seg, sp, quad, typeFlags, doOffset);
    if (!creg) return; // done with it
    // start with clip region top
    const float tzv1 = creg->eceiling.GetPointZ(*seg->v1);
    const float tzv2 = creg->eceiling.GetPointZ(*seg->v2);
    if (tzv1 >= orig[1].z && tzv2 >= orig[2].z) return; // out of quad
    orig[0].z = tzv1;
    orig[3].z = tzv2;
    if (orig[0].z > orig[1].z || orig[3].z > orig[2].z || !isValidQuad(orig)) return;
    memcpy(quad, orig, sizeof(orig));
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
