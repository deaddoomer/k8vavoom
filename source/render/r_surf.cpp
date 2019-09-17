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
//**  Copyright (C) 2018-2019 Ketmar Dark
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
#include "gamedefs.h"
#include "r_local.h"
#include "sv_local.h"


// ////////////////////////////////////////////////////////////////////////// //
static VCvarB r_hack_transtop("r_hack_transtop", true, "Allow \"Transparent Top Texture\" hack?", CVAR_PreInit|CVAR_Archive);


//**************************************************************************
//
//  Scaling
//
//**************************************************************************
static inline __attribute__((const)) float TextureSScale (const VTexture *pic) { return pic->SScale; }
static inline __attribute__((const)) float TextureTScale (const VTexture *pic) { return pic->TScale; }
static inline __attribute__((const)) float TextureOffsetSScale (const VTexture *pic) { return (pic->bWorldPanning ? pic->SScale : 1.0f); }
static inline __attribute__((const)) float TextureOffsetTScale (const VTexture *pic) { return (pic->bWorldPanning ? pic->TScale : 1.0f); }


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
static void AppendSurfaces (segpart_t *sp, surface_t *newsurfs) {
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


//==========================================================================
//
//  VRenderLevelShared::SetupSky
//
//==========================================================================
void VRenderLevelShared::SetupSky () {
  skyheight = -99999.0f;
  for (int i = 0; i < Level->NumSectors; ++i) {
    if (Level->Sectors[i].ceiling.pic == skyflatnum &&
        Level->Sectors[i].ceiling.maxz > skyheight)
    {
      skyheight = Level->Sectors[i].ceiling.maxz;
    }
  }
  // make it a bit higher to avoid clipping of the sprites
  skyheight += 8*1024;
  memset((void *)&sky_plane, 0, sizeof(sky_plane));
  sky_plane.Set(TVec(0, 0, -1), -skyheight);
  sky_plane.pic = skyflatnum;
  sky_plane.Alpha = 1.1f;
  sky_plane.LightSourceSector = -1;
  sky_plane.MirrorAlpha = 1.0f;
  sky_plane.XScale = 1.0f;
  sky_plane.YScale = 1.0f;
}


//==========================================================================
//
//  VRenderLevelShared::FlushSurfCaches
//
//==========================================================================
void VRenderLevelShared::FlushSurfCaches (surface_t *InSurfs) {
  surface_t *surfs = InSurfs;
  while (surfs) {
    if (surfs->CacheSurf) FreeSurfCache(surfs->CacheSurf);
    surfs = surfs->next;
  }
}


//==========================================================================
//
//  VRenderLevelShared::CreateSecSurface
//
//  this is used to create floor and ceiling surfaces
//
//  `ssurf` can be `nullptr`, and it will be allocated, otherwise changed
//
//==========================================================================
sec_surface_t *VRenderLevelShared::CreateSecSurface (sec_surface_t *ssurf, subsector_t *sub, TSecPlaneRef InSplane) {
  int vcount = sub->numlines;

  if (vcount < 3) {
    GCon->Logf(NAME_Warning, "CreateSecSurface: subsector #%d has only #%d vertices", (int)(ptrdiff_t)(sub-Level->Subsectors), vcount);
    if (vcount < 1) Sys_Error("ONE VERTEX. WTF?!");
    if (ssurf) return ssurf;
  }
  //vassert(vcount >= 3);

  // if we're simply changing sky, and already have surface created, do not recreate it, it is pointless
  bool isSkyFlat = (InSplane.splane->pic == skyflatnum);
  bool recreateSurface = true;
  bool updateZ = false;

  // fix plane
  TSecPlaneRef spl(InSplane);
  if (isSkyFlat && spl.GetNormalZ() < 0.0f) spl.set(&sky_plane, false);

  surface_t *surf = nullptr;
  if (!ssurf) {
    // new sector surface
    ssurf = new sec_surface_t;
    memset((void *)ssurf, 0, sizeof(sec_surface_t));
    surf = (surface_t *)Z_Calloc(sizeof(surface_t)+(vcount-1)*sizeof(TVec));
  } else {
    // change sector surface
    // we still may have to recreate it if it was a "sky <-> non-sky" change, so check for it
    recreateSurface = !isSkyFlat || ((ssurf->esecplane.splane->pic == skyflatnum) != isSkyFlat);
    if (recreateSurface) {
      //GCon->Logf("***  RECREATING!");
      surf = ReallocSurface(ssurf->surfs, vcount);
    } else {
      updateZ = (FASI(ssurf->edist) != FASI(spl.splane->dist));
      surf = ssurf->surfs;
    }
    ssurf->surfs = nullptr; // just in case
  }

  vuint32 typeFlags = (spl.GetNormalZ() > 0.0f ? surface_t::TF_FLOOR : surface_t::TF_CEILING);

  // this is required to calculate static lightmaps, and for other business
  for (surface_t *ss = surf; ss; ss = ss->next) {
    ss->subsector = sub;
    ss->typeFlags = typeFlags;
  }

  ssurf->esecplane = spl;
  ssurf->edist = spl.splane->dist;

  // setup texture
  VTexture *Tex = GTextureManager(spl.splane->pic);
  if (!Tex) Tex = GTextureManager[GTextureManager.DefaultTexture];
  if (fabsf(spl.splane->normal.z) > 0.1f) {
    float s, c;
    msincos(spl.splane->BaseAngle-spl.splane->Angle, &s, &c);
    ssurf->texinfo.saxis = TVec(c,  s, 0)*(TextureSScale(Tex)*spl.splane->XScale);
    ssurf->texinfo.taxis = TVec(s, -c, 0)*(TextureTScale(Tex)*spl.splane->YScale);
  } else {
    ssurf->texinfo.taxis = TVec(0, 0, -1)*(TextureTScale(Tex)*spl.splane->YScale);
    ssurf->texinfo.saxis = Normalise(CrossProduct(spl.GetNormal(), ssurf->texinfo.taxis))*(TextureSScale(Tex)*spl.splane->XScale);
  }

  const float newsoffs = spl.splane->xoffs;
  const float newtoffs = spl.splane->yoffs+spl.splane->BaseYOffs;
  bool offsChanged = (FASI(ssurf->texinfo.soffs) != FASI(newsoffs) ||
                      FASI(ssurf->texinfo.toffs) != FASI(newtoffs));
  ssurf->texinfo.soffs = newsoffs;
  ssurf->texinfo.toffs = newtoffs;
  //ssurf->texinfo.soffs = spl.splane->xoffs;
  //ssurf->texinfo.toffs = spl.splane->yoffs+spl.splane->BaseYOffs;

  ssurf->texinfo.Tex = Tex;
  ssurf->texinfo.noDecals = (Tex ? Tex->noDecals : true);
  ssurf->texinfo.Alpha = (spl.splane->Alpha < 1.0f ? spl.splane->Alpha : 1.1f);
  ssurf->texinfo.Additive = !!(spl.splane->flags&SPF_ADDITIVE);
  ssurf->texinfo.ColorMap = 0;
  ssurf->XScale = spl.splane->XScale;
  ssurf->YScale = spl.splane->YScale;
  ssurf->Angle = spl.splane->BaseAngle-spl.splane->Angle;

  TPlane plane = *(TPlane *)spl.splane;
  if (spl.flipped) plane.flipInPlace();

  if (recreateSurface) {
    surf->plane = plane;
    surf->count = vcount;
    const seg_t *seg = &Level->Segs[sub->firstline];
    TVec *dptr = surf->verts;
    if (spl.GetNormalZ() < 0.0f) {
      // backward
      for (seg += (vcount-1); vcount--; ++dptr, --seg) {
        const TVec &v = *seg->v1;
        *dptr = TVec(v.x, v.y, spl.GetPointZ(v.x, v.y));
      }
    } else {
      // forward
      for (; vcount--; ++dptr, ++seg) {
        const TVec &v = *seg->v1;
        *dptr = TVec(v.x, v.y, spl.GetPointZ(v.x, v.y));
      }
    }

    if (isSkyFlat) {
      // don't subdivide sky, as it cannot have lightmap
      ssurf->surfs = surf;
      surf->texinfo = &ssurf->texinfo;
    } else {
      ssurf->surfs = SubdivideFace(surf, ssurf->texinfo.saxis, &ssurf->texinfo.taxis);
      InitSurfs(true, ssurf->surfs, &ssurf->texinfo, &plane, sub);
    }
  } else if (updateZ) {
    // update z coords
    bool changed = false;
    for (; surf; surf = surf->next) {
      TVec *svert = surf->verts;
      for (int i = surf->count; i--; ++svert) {
        const float oldZ = svert->z;
        svert->z = spl.GetPointZ(svert->x, svert->y);
        if (!changed && FASI(oldZ) != FASI(svert->z)) changed = true;
      }
      if (changed) InitSurfs(true, ssurf->surfs, &ssurf->texinfo, &plane, sub);
    }
  } else if (offsChanged) {
    // still have to force it, because texture is scrolled, and lightmap s/t are invalid
    InitSurfs(true, ssurf->surfs, &ssurf->texinfo, &plane, sub);
  }

  return ssurf;
}


//==========================================================================
//
//  VRenderLevelShared::UpdateSecSurface
//
//  this is used to update floor and ceiling surfaces
//
//==========================================================================
void VRenderLevelShared::UpdateSecSurface (sec_surface_t *ssurf, TSecPlaneRef RealPlane, subsector_t *sub, bool ignoreColorMap) {
  if (!ssurf->esecplane.splane->pic) return; // no texture? nothing to do

  TSecPlaneRef splane(ssurf->esecplane);

  if (splane.splane != RealPlane.splane) {
    // check for sky changes
    if ((splane.splane->pic == skyflatnum) != (RealPlane.splane->pic == skyflatnum)) {
      // sky <-> non-sky, simply recreate it
      sec_surface_t *newsurf = CreateSecSurface(ssurf, sub, RealPlane);
      vassert(newsurf == ssurf); // sanity check
      ssurf->texinfo.ColorMap = ColorMap; // just in case
      // nothing more to do
      return;
    }
    // substitute real plane with sky plane if necessary
    if (RealPlane.splane->pic == skyflatnum && RealPlane.GetNormalZ() < 0.0f) {
      if (splane.splane != &sky_plane) {
        // recreate it, just in case
        sec_surface_t *newsurf = CreateSecSurface(ssurf, sub, RealPlane);
        vassert(newsurf == ssurf); // sanity check
        ssurf->texinfo.ColorMap = ColorMap; // just in case
        // nothing more to do
        return;
      }
      splane.set(&sky_plane, false);
    }
  }

  enum { USS_Normal, USS_Force, USS_IgnoreCMap, USS_ForceIgnoreCMap };

  // if scale/angle was changed, we should update everything, and possibly rebuild the surface
  // our general surface creation function will take care of everything
  if (FASI(ssurf->XScale) != FASI(splane.splane->XScale) ||
      FASI(ssurf->YScale) != FASI(splane.splane->YScale) ||
      ssurf->Angle != splane.splane->BaseAngle-splane.splane->Angle)
  {
    // this will update texture, offsets, and everything
    /*
    GCon->Logf("*** SSF RECREATION! xscale=(%g:%g), yscale=(%g,%g); angle=(%g,%g)",
      ssurf->XScale, splane.splane->XScale,
      ssurf->YScale, splane.splane->YScale,
      ssurf->Angle, splane.splane->BaseAngle-splane.splane->Angle);
    */
    sec_surface_t *newsurf = CreateSecSurface(ssurf, sub, RealPlane);
    vassert(newsurf == ssurf); // sanity check
    ssurf->texinfo.ColorMap = (!ignoreColorMap ? ColorMap : 0); // just in case
    // nothing more to do
    return;
  }

  if (!ignoreColorMap) ssurf->texinfo.ColorMap = ColorMap; // just in case
  const float newsoffs = splane.splane->xoffs;
  const float newtoffs = splane.splane->yoffs+splane.splane->BaseYOffs;
  bool offsChanged = (FASI(ssurf->texinfo.soffs) != FASI(newsoffs) ||
                      FASI(ssurf->texinfo.toffs) != FASI(newtoffs));
  ssurf->texinfo.soffs = newsoffs;
  ssurf->texinfo.toffs = newtoffs;

  // ok, we still may need to update texture or z coords
  // update texture?
  VTexture *Tex = GTextureManager(splane.splane->pic);
  if (!Tex) Tex = GTextureManager[GTextureManager.DefaultTexture];
  ssurf->texinfo.Tex = Tex;
  ssurf->texinfo.noDecals = Tex->noDecals;

  // update z coords?
  if (FASI(ssurf->edist) != FASI(splane.splane->dist)) {
    TPlane plane = *(TPlane *)splane.splane;
    if (splane.flipped) plane.flipInPlace();
    bool changed = false;
    ssurf->edist = splane.splane->dist;
    for (surface_t *surf = ssurf->surfs; surf; surf = surf->next) {
      surf->plane = plane;
      TVec *svert = surf->verts;
      for (int i = surf->count; i--; ++svert) {
        const float oldZ = svert->z;
        svert->z = splane.GetPointZ(svert->x, svert->y);
        if (!changed && FASI(oldZ) != FASI(svert->z)) changed = true;
      }
    }
    // force lightmap recalculation
    if (changed || splane.splane->pic != skyflatnum) {
      InitSurfs(true, ssurf->surfs, &ssurf->texinfo, &plane, sub);
    }
  } else if (offsChanged && splane.splane->pic != skyflatnum) {
    // still have to force it, because texture is scrolled, and lightmap s/t are invalid
    TPlane plane = *(TPlane *)splane.splane;
    if (splane.flipped) plane.flipInPlace();
    InitSurfs(true, ssurf->surfs, &ssurf->texinfo, &plane, sub);
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
surface_t *VRenderLevelShared::NewWSurf () {
  enum { WSURFSIZE = sizeof(surface_t)+sizeof(TVec)*(surface_t::MAXWVERTS-1) };
#if 1
  if (!free_wsurfs) {
    // allocate some more surfs
    vuint8 *tmp = (vuint8 *)Z_Calloc(WSURFSIZE*128+sizeof(void *));
    *(void **)tmp = AllocatedWSurfBlocks;
    AllocatedWSurfBlocks = tmp;
    tmp += sizeof(void *);
    for (int i = 0; i < 128; ++i) {
      ((surface_t *)tmp)->next = free_wsurfs;
      free_wsurfs = (surface_t *)tmp;
      tmp += WSURFSIZE;
    }
  }
  surface_t *surf = free_wsurfs;
  free_wsurfs = surf->next;

  memset((void *)surf, 0, WSURFSIZE);
#else
  surface_t *surf = (surface_t *)Z_Calloc(WSURFSIZE);
#endif

  return surf;
}


//==========================================================================
//
//  VRenderLevelShared::FreeWSurfs
//
//==========================================================================
void VRenderLevelShared::FreeWSurfs (surface_t *&InSurfs) {
#if 1
  surface_t *surfs = InSurfs;
  FlushSurfCaches(surfs);
  while (surfs) {
    if (surfs->lightmap) Z_Free(surfs->lightmap);
    if (surfs->lightmap_rgb) Z_Free(surfs->lightmap_rgb);
    surface_t *next = surfs->next;
    surfs->next = free_wsurfs;
    free_wsurfs = surfs;
    surfs = next;
  }
  InSurfs = nullptr;
#else
  while (InSurfs) {
    surface_t *surf = InSurfs;
    InSurfs = InSurfs->next;
    if (surf->CacheSurf) FreeSurfCache(surf->CacheSurf);
    if (surf->lightmap) Z_Free(surf->lightmap);
    if (surf->lightmap_rgb) Z_Free(surf->lightmap_rgb);
    Z_Free(surf);
  }
#endif
}


//==========================================================================
//
//  VRenderLevelShared::CreateWSurf
//
//  this is used to create world/wall surface
//
//==========================================================================
surface_t *VRenderLevelShared::CreateWSurf (TVec *wv, texinfo_t *texinfo, seg_t *seg, subsector_t *sub, int wvcount, vuint32 typeFlags) {
  if (wvcount < 3) return nullptr;
  if (wvcount == 4 && (wv[1].z <= wv[0].z && wv[2].z <= wv[3].z)) return nullptr;
  if (wvcount > surface_t::MAXWVERTS) Sys_Error("cannot create huge world surface (the thing that should not be)");

  if (!texinfo->Tex || texinfo->Tex->Type == TEXTYPE_Null) return nullptr;

  surface_t *surf = NewWSurf();
  surf->subsector = sub;
  surf->seg = seg;
  surf->next = nullptr;
  surf->count = wvcount;
  surf->typeFlags = typeFlags;
  memcpy(surf->verts, wv, wvcount*sizeof(TVec));

  if (texinfo->Tex == GTextureManager[skyflatnum]) {
    // never split sky surfaces
    surf->texinfo = texinfo;
    surf->plane = *(TPlane *)seg;
    return surf;
  }

  surf = SubdivideSeg(surf, texinfo->saxis, &texinfo->taxis, seg);
  InitSurfs(true, surf, texinfo, seg, sub);
  return surf;
}


//==========================================================================
//
//  VRenderLevelShared::CountSegParts
//
//==========================================================================
int VRenderLevelShared::CountSegParts (const seg_t *seg) {
  if (!seg->linedef) return 0; // miniseg
  if (!seg->backsector) return 2;
  //k8: each backsector 3d floor can require segpart
  int count = 4;
  for (const sec_region_t *reg = seg->backsector->eregions; reg; reg = reg->next) count += 2; // just in case, reserve two
  return count;
}


//==========================================================================
//
//  VRenderLevelShared::CreateWorldSurfFromWV
//
//==========================================================================
void VRenderLevelShared::CreateWorldSurfFromWV (subsector_t *sub, seg_t *seg, segpart_t *sp, TVec wv[4], vuint32 typeFlags, bool doOffset) {
  if (wv[0].z == wv[1].z && wv[1].z == wv[2].z && wv[2].z == wv[3].z) {
    // degenerate surface, no need to create it
    return;
  }

  if (wv[0].z == wv[1].z && wv[2].z == wv[3].z) {
    // degenerate surface (thin line), cannot create it (no render support)
    return;
  }

  if (wv[0].z == wv[2].z && wv[1].z == wv[3].z) {
    // degenerate surface (thin line), cannot create it (no render support)
    return;
  }

  TVec *wstart = wv;
  int wcount = 4;
  if (wv[0].z == wv[1].z) {
    // can reduce to triangle
    wstart = wv+1;
    wcount = 3;
  } else if (wv[2].z == wv[3].z) {
    // can reduce to triangle
    wstart = wv;
    wcount = 3;
  }

  //k8: HACK! HACK! HACK!
  //    move middle wall backwards a little, so it will be hidden behind up/down surfaces
  //    this is required for sectors with 3d floors, until i wrote a proper texture clipping math
  if (doOffset) for (unsigned f = 0; f < (unsigned)wcount; ++f) wstart[f] -= seg->normal*0.01f;

  AppendSurfaces(sp, CreateWSurf(wstart, &sp->texinfo, seg, sub, wcount, typeFlags));
}


//==========================================================================
//
//  VRenderLevelShared::SetupOneSidedSkyWSurf
//
//==========================================================================
void VRenderLevelShared::SetupOneSidedSkyWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  FreeWSurfs(sp->surfs);

  sp->texinfo.Tex = GTextureManager[skyflatnum];
  sp->texinfo.noDecals = sp->texinfo.Tex->noDecals;
  sp->texinfo.Alpha = 1.1f;
  sp->texinfo.Additive = false;
  sp->texinfo.ColorMap = 0;

  if (sp->texinfo.Tex->Type != TEXTYPE_Null) {
    TVec wv[4];

    const float topz1 = r_ceiling.GetPointZ(*seg->v1);
    const float topz2 = r_ceiling.GetPointZ(*seg->v2);

    wv[0].x = wv[1].x = seg->v1->x;
    wv[0].y = wv[1].y = seg->v1->y;
    wv[2].x = wv[3].x = seg->v2->x;
    wv[2].y = wv[3].y = seg->v2->y;

    wv[0].z = topz1;
    wv[1].z = wv[2].z = skyheight;
    wv[3].z = topz2;

    CreateWorldSurfFromWV(sub, seg, sp, wv, 0); // sky texture, no type flags
  }

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backTopDist = 0.0f;
  sp->backBotDist = 0.0f;
}


//==========================================================================
//
//  VRenderLevelShared::SetupTwoSidedSkyWSurf
//
//==========================================================================
void VRenderLevelShared::SetupTwoSidedSkyWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  FreeWSurfs(sp->surfs);

  sp->texinfo.Tex = GTextureManager[skyflatnum];
  sp->texinfo.noDecals = sp->texinfo.Tex->noDecals;
  sp->texinfo.Alpha = 1.1f;
  sp->texinfo.Additive = false;
  sp->texinfo.ColorMap = 0;

  if (sp->texinfo.Tex->Type != TEXTYPE_Null) {
    TVec wv[4];

    const float topz1 = r_ceiling.GetPointZ(*seg->v1);
    const float topz2 = r_ceiling.GetPointZ(*seg->v2);

    wv[0].x = wv[1].x = seg->v1->x;
    wv[0].y = wv[1].y = seg->v1->y;
    wv[2].x = wv[3].x = seg->v2->x;
    wv[2].y = wv[3].y = seg->v2->y;

    wv[0].z = topz1;
    wv[1].z = wv[2].z = skyheight;
    wv[3].z = topz2;

    CreateWorldSurfFromWV(sub, seg, sp, wv, 0); // sky texture, no type flags
  }

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backTopDist = 0.0f;
  sp->backBotDist = 0.0f;
}


//==========================================================================
//
//  SetupTextureAxesOffset
//
//==========================================================================
static inline void SetupTextureAxesOffset (seg_t *seg, texinfo_t *texinfo, VTexture *tex, const side_tex_params_t *tparam) {
  texinfo->Tex = tex;
  texinfo->noDecals = tex->noDecals;
  // can be fixed later
  texinfo->Alpha = 1.1f;
  texinfo->Additive = false;
  texinfo->ColorMap = 0;

  texinfo->saxis = seg->dir*(TextureSScale(tex)*tparam->ScaleX);
  texinfo->taxis = TVec(0, 0, -1)*(TextureTScale(tex)*tparam->ScaleY);

  texinfo->soffs = -DotProduct(*seg->v1, texinfo->saxis)+
                      seg->offset*(TextureSScale(tex)*tparam->ScaleX)+ //k8: maybe we need to *divide* to scale here?
                      tparam->TextureOffset*TextureOffsetSScale(tex);

  texinfo->toffs = 0.0f;
  // toffs is not calculated here, as its calculation depends of texture position and pegging
}


//==========================================================================
//
//  IsTransDoorHack
//
//  HACK: sector with height of 1, and only middle
//  masked texture is "transparent door"
//
//==========================================================================
static inline bool IsTransDoorHack (const seg_t *seg) {
  const sector_t *secs[2] = { seg->frontsector, seg->backsector };
  // check for slopes
  if (secs[0]->floor.normal.z != 1.0f || secs[0]->ceiling.normal.z != -1.0f) return false;
  if (secs[1]->floor.normal.z != 1.0f || secs[1]->ceiling.normal.z != -1.0f) return false;
  // check for door
  if (secs[0]->floor.minz != secs[1]->floor.minz) return false;
  // check for middle texture
  const side_t *sidedef = seg->sidedef;
  VTexture *mt = GTextureManager(sidedef->MidTexture);
  if (!mt || mt->Type == TEXTYPE_Null || !mt->isTransparent()) return false;
  // ok, looks like it
  return true;
}


//==========================================================================
//
//  DivByScale
//
//==========================================================================
static inline float DivByScale (float v, float scale) {
  return (scale > 0 ? v/scale : v);
}


//==========================================================================
//
//  VRenderLevelShared::SetupTwoSidedTopWSurf
//
//==========================================================================
void VRenderLevelShared::SetupTwoSidedTopWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  FreeWSurfs(sp->surfs);

  const line_t *linedef = seg->linedef;
  const side_t *sidedef = seg->sidedef;

  sec_plane_t *back_floor = &seg->backsector->floor;
  sec_plane_t *back_ceiling = &seg->backsector->ceiling;

  VTexture *TTex = GTextureManager(sidedef->TopTexture);
  if (!TTex) TTex = GTextureManager[GTextureManager.DefaultTexture];
  if (IsSky(r_ceiling.splane) && IsSky(back_ceiling) && r_ceiling.splane->SkyBox != back_ceiling->SkyBox) {
    TTex = GTextureManager[skyflatnum];
  }

  // HACK: sector with height of 1, and only middle masked texture is "transparent door"
  //       also, invert "upper unpegged" flag for this case
  int peghack = 0;
  if (r_hack_transtop && TTex->Type == TEXTYPE_Null && IsTransDoorHack(seg)) {
    TTex = GTextureManager(sidedef->MidTexture);
    peghack = ML_DONTPEGTOP;
  }

  SetupTextureAxesOffset(seg, &sp->texinfo, TTex, &sidedef->Top);

  if (TTex->Type != TEXTYPE_Null) {
    TVec wv[4];

    const float topz1 = r_ceiling.GetPointZ(*seg->v1);
    const float topz2 = r_ceiling.GetPointZ(*seg->v2);
    const float botz1 = r_floor.GetPointZ(*seg->v1);
    const float botz2 = r_floor.GetPointZ(*seg->v2);

    const float back_topz1 = back_ceiling->GetPointZ(*seg->v1);
    const float back_topz2 = back_ceiling->GetPointZ(*seg->v2);

    // hack to allow height changes in outdoor areas
    float top_topz1 = topz1;
    float top_topz2 = topz2;
    float top_TexZ = r_ceiling.splane->TexZ;
    if (IsSky(r_ceiling.splane) && IsSky(back_ceiling) && r_ceiling.splane->SkyBox == back_ceiling->SkyBox) {
      top_topz1 = back_topz1;
      top_topz2 = back_topz2;
      top_TexZ = back_ceiling->TexZ;
    }

    if ((linedef->flags&ML_DONTPEGTOP)^peghack) {
      // top of texture at top
      sp->texinfo.toffs = top_TexZ;
    } else {
      // bottom of texture
      sp->texinfo.toffs = back_ceiling->TexZ+(TTex->GetScaledHeight()*sidedef->Top.ScaleY);
    }
    sp->texinfo.toffs *= TextureTScale(TTex)*sidedef->Top.ScaleY;
    sp->texinfo.toffs += sidedef->Top.RowOffset*TextureOffsetTScale(TTex);

    wv[0].x = wv[1].x = seg->v1->x;
    wv[0].y = wv[1].y = seg->v1->y;
    wv[2].x = wv[3].x = seg->v2->x;
    wv[2].y = wv[3].y = seg->v2->y;

    wv[0].z = max2(back_topz1, botz1);
    wv[1].z = top_topz1;
    wv[2].z = top_topz2;
    wv[3].z = max2(back_topz2, botz2);

    CreateWorldSurfFromWV(sub, seg, sp, wv, surface_t::TF_TOP|(peghack ? surface_t::TF_TOPHACK : 0));
  }

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backTopDist = back_ceiling->dist;
  sp->backBotDist = back_floor->dist;
  sp->TextureOffset = sidedef->Top.TextureOffset;
  sp->RowOffset = sidedef->Top.RowOffset;
}


//==========================================================================
//
//  VRenderLevelShared::SetupTwoSidedBotWSurf
//
//==========================================================================
void VRenderLevelShared::SetupTwoSidedBotWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  FreeWSurfs(sp->surfs);

  const line_t *linedef = seg->linedef;
  const side_t *sidedef = seg->sidedef;

  sec_plane_t *back_floor = &seg->backsector->floor;
  sec_plane_t *back_ceiling = &seg->backsector->ceiling;

  VTexture *BTex = GTextureManager(sidedef->BottomTexture);
  if (!BTex) BTex = GTextureManager[GTextureManager.DefaultTexture];

  SetupTextureAxesOffset(seg, &sp->texinfo, BTex, &sidedef->Bot);

  if (BTex->Type != TEXTYPE_Null) {
    TVec wv[4];

    float topz1 = r_ceiling.GetPointZ(*seg->v1);
    float topz2 = r_ceiling.GetPointZ(*seg->v2);
    float botz1 = r_floor.GetPointZ(*seg->v1);
    float botz2 = r_floor.GetPointZ(*seg->v2);
    float top_TexZ = r_ceiling.splane->TexZ;

    const float back_botz1 = back_floor->GetPointZ(*seg->v1);
    const float back_botz2 = back_floor->GetPointZ(*seg->v2);

    // hack to allow height changes in outdoor areas
    if (IsSky(r_ceiling.splane) && IsSky(back_ceiling)) {
      topz1 = back_ceiling->GetPointZ(*seg->v1);
      topz2 = back_ceiling->GetPointZ(*seg->v2);
      top_TexZ = back_ceiling->TexZ;
    }

    if (linedef->flags&ML_DONTPEGBOTTOM) {
      // bottom of texture at bottom
      // top of texture at top
      sp->texinfo.toffs = top_TexZ;
    } else {
      // top of texture at top
      sp->texinfo.toffs = back_floor->TexZ;
    }
    sp->texinfo.toffs *= TextureTScale(BTex)*sidedef->Bot.ScaleY;
    sp->texinfo.toffs += sidedef->Bot.RowOffset*TextureOffsetTScale(BTex);

    wv[0].x = wv[1].x = seg->v1->x;
    wv[0].y = wv[1].y = seg->v1->y;
    wv[2].x = wv[3].x = seg->v2->x;
    wv[2].y = wv[3].y = seg->v2->y;

    wv[0].z = botz1;
    wv[1].z = min2(back_botz1, topz1);
    wv[2].z = min2(back_botz2, topz2);
    wv[3].z = botz2;

    CreateWorldSurfFromWV(sub, seg, sp, wv, surface_t::TF_BOTTOM);
  }

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backBotDist = back_floor->dist;
  sp->backTopDist = back_ceiling->dist;
  sp->TextureOffset = sidedef->Bot.TextureOffset;
  sp->RowOffset = sidedef->Bot.RowOffset;
}


//==========================================================================
//
//  VRenderLevelShared::SetupOneSidedMidWSurf
//
//==========================================================================
void VRenderLevelShared::SetupOneSidedMidWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  FreeWSurfs(sp->surfs);

  const line_t *linedef = seg->linedef;
  const side_t *sidedef = seg->sidedef;

  VTexture *MTex = GTextureManager(sidedef->MidTexture);
  // k8: one-sided line should have a midtex
  if (!MTex) {
    GCon->Logf(NAME_Warning, "Sidedef #%d should have midtex, but it hasn't (%d)", (int)(ptrdiff_t)(sidedef-Level->Sides), sidedef->MidTexture.id);
    MTex = GTextureManager[GTextureManager.DefaultTexture];
  }

  SetupTextureAxesOffset(seg, &sp->texinfo, MTex, &sidedef->Mid);

  if (linedef->flags&ML_DONTPEGBOTTOM) {
    // bottom of texture at bottom
    sp->texinfo.toffs = r_floor.splane->TexZ+(MTex->GetScaledHeight()*sidedef->Mid.ScaleY);
  } else if (linedef->flags&ML_DONTPEGTOP) {
    // top of texture at top of top region
    sp->texinfo.toffs = seg->frontsub->sector->ceiling.TexZ;
  } else {
    // top of texture at top
    sp->texinfo.toffs = r_ceiling.splane->TexZ;
  }
  sp->texinfo.toffs *= TextureTScale(MTex)*sidedef->Mid.ScaleY;
  sp->texinfo.toffs += sidedef->Mid.RowOffset*TextureOffsetTScale(MTex);

  if (MTex->Type != TEXTYPE_Null) {
    TVec wv[4];

    wv[0].x = wv[1].x = seg->v1->x;
    wv[0].y = wv[1].y = seg->v1->y;
    wv[2].x = wv[3].x = seg->v2->x;
    wv[2].y = wv[3].y = seg->v2->y;

    const float topz1 = r_ceiling.GetPointZ(*seg->v1);
    const float topz2 = r_ceiling.GetPointZ(*seg->v2);
    const float botz1 = r_floor.GetPointZ(*seg->v1);
    const float botz2 = r_floor.GetPointZ(*seg->v2);

    wv[0].z = botz1;
    wv[1].z = topz1;
    wv[2].z = topz2;
    wv[3].z = botz2;

    CreateWorldSurfFromWV(sub, seg, sp, wv, surface_t::TF_MIDDLE);
  }

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backTopDist = 0.0f;
  sp->backBotDist = 0.0f;
  sp->TextureOffset = sidedef->Mid.TextureOffset;
  sp->RowOffset = sidedef->Mid.RowOffset;
}


//==========================================================================
//
//  DumpOpening
//
//==========================================================================
static __attribute__((unused)) void DumpOpening (const opening_t *op) {
  GCon->Logf("  %p: floor=%g (%g,%g,%g:%g); ceil=%g (%g,%g,%g:%g); lowfloor=%g; range=%g",
    op,
    op->bottom, op->efloor.GetNormal().x, op->efloor.GetNormal().y, op->efloor.GetNormal().z, op->efloor.GetDist(),
    op->top, op->eceiling.GetNormal().x, op->eceiling.GetNormal().y, op->eceiling.GetNormal().z, op->eceiling.GetDist(),
    op->lowfloor, op->range);
}


//==========================================================================
//
//  FixMidTextureOffsetAndOrigin
//
//==========================================================================
static inline void FixMidTextureOffsetAndOrigin (float &z_org, const line_t *linedef, const side_t *sidedef, texinfo_t *texinfo, VTexture *MTex, const side_tex_params_t *tparam, bool forceWrapped=false) {
  if (forceWrapped || ((linedef->flags&ML_WRAP_MIDTEX)|(sidedef->Flags&SDF_WRAPMIDTEX))) {
    // it is wrapped, so just slide it
    texinfo->toffs = tparam->RowOffset*TextureOffsetTScale(MTex);
  } else {
    // move origin up/down, as this texture is not wrapped
    z_org += tparam->RowOffset*DivByScale(TextureOffsetTScale(MTex), tparam->ScaleY);
    // offset is done by origin, so we don't need to offset texture
    texinfo->toffs = 0.0f;
  }
  texinfo->toffs += z_org*(TextureTScale(MTex)*tparam->ScaleY);
}


//==========================================================================
//
//  VRenderLevelShared::SetupTwoSidedMidWSurf
//
//  create normal midtexture surface
//
//==========================================================================
void VRenderLevelShared::SetupTwoSidedMidWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  FreeWSurfs(sp->surfs);

  const line_t *linedef = seg->linedef;
  const side_t *sidedef = seg->sidedef;

  VTexture *MTex = GTextureManager(sidedef->MidTexture);
  if (!MTex) MTex = GTextureManager[GTextureManager.DefaultTexture];

  SetupTextureAxesOffset(seg, &sp->texinfo, MTex, &sidedef->Mid);

  sec_plane_t *back_floor = &seg->backsector->floor;
  sec_plane_t *back_ceiling = &seg->backsector->ceiling;

  if (MTex->Type != TEXTYPE_Null) {
    TVec wv[4];

    const float back_topz1 = back_ceiling->GetPointZ(*seg->v1);
    const float back_topz2 = back_ceiling->GetPointZ(*seg->v2);
    const float back_botz1 = back_floor->GetPointZ(*seg->v1);
    const float back_botz2 = back_floor->GetPointZ(*seg->v2);

    const float exbotz = min2(back_botz1, back_botz2);
    const float extopz = max2(back_topz1, back_topz2);

    const float texh = DivByScale(MTex->GetScaledHeight(), sidedef->Mid.ScaleY);
    float z_org; // texture top
    if (linedef->flags&ML_DONTPEGBOTTOM) {
      // bottom of texture at bottom
      z_org = max2(seg->frontsector->floor.TexZ, seg->backsector->floor.TexZ)+texh;
    } else {
      // top of texture at top
      z_org = min2(seg->frontsector->ceiling.TexZ, seg->backsector->ceiling.TexZ);
    }
    FixMidTextureOffsetAndOrigin(z_org, linedef, sidedef, &sp->texinfo, MTex, &sidedef->Mid);

    sp->texinfo.Alpha = linedef->alpha;
    sp->texinfo.Additive = !!(linedef->flags&ML_ADDITIVE);

    //bool doDump = ((ptrdiff_t)(linedef-Level->Lines) == 7956);
    enum { doDump = 0 };
    if (doDump) { GCon->Logf("=== MIDSURF FOR LINE #%d (fs=%d; bs=%d; side=%d) ===", (int)(ptrdiff_t)(linedef-Level->Lines), (int)(ptrdiff_t)(seg->frontsector-Level->Sectors), (int)(ptrdiff_t)(seg->backsector-Level->Sectors), (int)(ptrdiff_t)(sidedef-Level->Sides)); }
    //if (linedef->alpha < 1.0f) GCon->Logf("=== MIDSURF FOR LINE #%d (fs=%d; bs=%d) ===", (int)(ptrdiff_t)(linedef-Level->Lines), (int)(ptrdiff_t)(seg->frontsector-Level->Sectors), (int)(ptrdiff_t)(seg->backsector-Level->Sectors));
    if (doDump) { GCon->Logf("   LINEWRAP=%u; SIDEWRAP=%u; ADDITIVE=%u; Alpha=%g; botpeg=%u; z_org=%g; texh=%g", (linedef->flags&ML_WRAP_MIDTEX), (sidedef->Flags&SDF_WRAPMIDTEX), (linedef->flags&ML_ADDITIVE), linedef->alpha, linedef->flags&ML_DONTPEGBOTTOM, z_org, texh); }
    if (doDump) { GCon->Logf("   tx is '%s'; size=(%d,%d); scale=(%g,%g)", *MTex->Name, MTex->GetWidth(), MTex->GetHeight(), MTex->SScale, MTex->TScale); }

    //k8: HACK! HACK! HACK!
    //    move middle wall backwards a little, so it will be hidden behind up/down surfaces
    //    this is required for sectors with 3d floors, until i wrote a proper texture clipping math
    bool doOffset = seg->backsector->Has3DFloors();

    for (opening_t *cop = SV_SectorOpenings(seg->frontsector, true); cop; cop = cop->next) {
      if (extopz <= cop->bottom || exbotz >= cop->top) {
        if (doDump) { GCon->Log(" SKIP opening"); DumpOpening(cop); }
        //continue;
      }
      if (doDump) { GCon->Logf(" ACCEPT opening"); DumpOpening(cop); }
      // ok, we are at least partially in this opening

      wv[0].x = wv[1].x = seg->v1->x;
      wv[0].y = wv[1].y = seg->v1->y;
      wv[2].x = wv[3].x = seg->v2->x;
      wv[2].y = wv[3].y = seg->v2->y;

      const float topz1 = min2(back_topz1, cop->eceiling.GetPointZ(*seg->v1));
      const float topz2 = min2(back_topz2, cop->eceiling.GetPointZ(*seg->v2));
      const float botz1 = max2(back_botz1, cop->efloor.GetPointZ(*seg->v1));
      const float botz2 = max2(back_botz2, cop->efloor.GetPointZ(*seg->v2));

      float midtopz1 = topz1;
      float midtopz2 = topz2;
      float midbotz1 = botz1;
      float midbotz2 = botz2;

      if (doDump) { GCon->Logf(" zorg=(%g,%g); botz=(%g,%g); topz=(%g,%g)", z_org-texh, z_org, midbotz1, midbotz2, midtopz1, midtopz2); }

      if (sidedef->TopTexture > 0) {
        midtopz1 = min2(midtopz1, cop->eceiling.GetPointZ(*seg->v1));
        midtopz2 = min2(midtopz2, cop->eceiling.GetPointZ(*seg->v2));
      }

      if (sidedef->BottomTexture > 0) {
        midbotz1 = max2(midbotz1, cop->efloor.GetPointZ(*seg->v1));
        midbotz2 = max2(midbotz2, cop->efloor.GetPointZ(*seg->v2));
      }

      if (midbotz1 >= midtopz1 || midbotz2 >= midtopz2) continue;

      if (doDump) { GCon->Logf(" zorg=(%g,%g); botz=(%g,%g); topz=(%g,%g); backbotz=(%g,%g); backtopz=(%g,%g)", z_org-texh, z_org, midbotz1, midbotz2, midtopz1, midtopz2, back_botz1, back_botz2, back_topz1, back_topz2); }

      float hgts[4];

      // linedef->flags&ML_CLIP_MIDTEX, sidedef->Flags&SDF_CLIPMIDTEX
      // this clips texture to a floor, otherwise it goes beyound it
      // it seems that all modern OpenGL renderers just ignores clip flag, and
      // renders all midtextures as always clipped.
      if ((linedef->flags&ML_WRAP_MIDTEX)|(sidedef->Flags&SDF_WRAPMIDTEX)) {
        hgts[0] = midbotz1;
        hgts[1] = midtopz1;
        hgts[2] = midtopz2;
        hgts[3] = midbotz2;
      } else {
        if (z_org <= max2(midbotz1, midbotz2)) continue;
        if (z_org-texh >= max2(midtopz1, midtopz2)) continue;
        if (doDump) {
          GCon->Log(" === front regions ===");
          VLevel::dumpSectorRegions(seg->frontsector);
          GCon->Log(" === front openings ===");
          for (opening_t *bop = SV_SectorOpenings2(seg->frontsector, true); bop; bop = bop->next) DumpOpening(bop);
          GCon->Log(" === back regions ===");
          VLevel::dumpSectorRegions(seg->backsector);
          GCon->Log(" === back openings ===");
          for (opening_t *bop = SV_SectorOpenings2(seg->backsector, true); bop; bop = bop->next) DumpOpening(bop);
        }
        hgts[0] = max2(midbotz1, z_org-texh);
        hgts[1] = min2(midtopz1, z_org);
        hgts[2] = min2(midtopz2, z_org);
        hgts[3] = max2(midbotz2, z_org-texh);
      }

      wv[0].z = hgts[0];
      wv[1].z = hgts[1];
      wv[2].z = hgts[2];
      wv[3].z = hgts[3];

      if (doDump) {
        GCon->Logf("  z:(%g,%g,%g,%g)", hgts[0], hgts[1], hgts[2], hgts[3]);
        for (int wc = 0; wc < 4; ++wc) GCon->Logf("  wc #%d: (%g,%g,%g)", wc, wv[wc].x, wv[wc].y, wv[wc].z);
      }

      CreateWorldSurfFromWV(sub, seg, sp, wv, surface_t::TF_MIDDLE, doOffset);
    }
  } else {
    // empty midtexture
    sp->texinfo.Alpha = 1.1f;
    sp->texinfo.Additive = false;
  }

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backTopDist = back_ceiling->dist;
  sp->backBotDist = back_floor->dist;
  sp->TextureOffset = sidedef->Mid.TextureOffset;
  sp->RowOffset = sidedef->Mid.RowOffset;
}


//==========================================================================
//
//  VRenderLevelShared::SetupTwoSidedMidExtraWSurf
//
//  create 3d-floors midtexture (side) surfaces
//  this creates surfaces not in 3d-floor sector, but in neighbouring one
//
//  do not create side surfaces if they aren't in openings
//
//==========================================================================
void VRenderLevelShared::SetupTwoSidedMidExtraWSurf (sec_region_t *reg, subsector_t *sub, seg_t *seg, segpart_t *sp,
                                                     TSecPlaneRef r_floor, TSecPlaneRef r_ceiling, opening_t *ops)
{
  FreeWSurfs(sp->surfs);

  const line_t *linedef = reg->extraline;
  const side_t *sidedef = &Level->Sides[linedef->sidenum[0]];

  VTexture *MTex = GTextureManager(sidedef->MidTexture);
  if (!MTex) MTex = GTextureManager[GTextureManager.DefaultTexture];

  //bool doDump = false;
  enum { doDump = 0 };

  /*
  if (seg->frontsector-&Level->Sectors[0] == 70) {
    doDump = true;
    GCon->Logf("::: SECTOR #70 (back #%d) EF: texture='%s'", (seg->backsector ? (int)(ptrdiff_t)(seg->backsector-&Level->Sectors[0]) : -1), *MTex->Name);
    GCon->Log(" === front regions ===");
    VLevel::dumpSectorRegions(seg->frontsector);
    GCon->Log(" === front openings ===");
    for (opening_t *bop = SV_SectorOpenings2(seg->frontsector, true); bop; bop = bop->next) DumpOpening(bop);
    GCon->Log(" === real openings ===");
    for (opening_t *bop = ops; bop; bop = bop->next) DumpOpening(bop);
  }

  if (seg->backsector && seg->backsector-&Level->Sectors[0] == 70) {
    doDump = true;
    GCon->Logf("::: BACK-SECTOR #70 (front #%d) EF: texture='%s'", (seg->frontsector ? (int)(ptrdiff_t)(seg->frontsector-&Level->Sectors[0]) : -1), *MTex->Name);
    GCon->Log(" === front regions ===");
    VLevel::dumpSectorRegions(seg->frontsector);
    GCon->Log(" === front openings ===");
    for (opening_t *bop = SV_SectorOpenings2(seg->frontsector, true); bop; bop = bop->next) DumpOpening(bop);
    GCon->Log(" === real openings ===");
    for (opening_t *bop = ops; bop; bop = bop->next) DumpOpening(bop);
  }
  */

  SetupTextureAxesOffset(seg, &sp->texinfo, MTex, &sidedef->Mid);

  const float texh = DivByScale(MTex->GetScaledHeight(), sidedef->Mid.ScaleY);
  float z_org; // texture top
  if (linedef->flags&ML_DONTPEGBOTTOM) {
    // bottom of texture at bottom
    z_org = reg->efloor.splane->TexZ+texh;
  } else {
    // top of texture at top
    z_org = reg->eceiling.splane->TexZ;
  }
  FixMidTextureOffsetAndOrigin(z_org, linedef, sidedef, &sp->texinfo, MTex, &sidedef->Mid, true);

  sp->texinfo.Alpha = (reg->efloor.splane->Alpha < 1.0f ? reg->efloor.splane->Alpha : 1.1f);
  sp->texinfo.Additive = !!(reg->efloor.splane->flags&SPF_ADDITIVE);

  if (MTex->Type != TEXTYPE_Null) {
    TVec wv[4];

    //const bool wrapped = !!((linedef->flags&ML_WRAP_MIDTEX)|(sidedef->Flags&SDF_WRAPMIDTEX));
    // side 3d floor midtex should always be wrapped
    enum { wrapped = 1 };

    const float extratopz1 = reg->eceiling.GetPointZ(*seg->v1);
    const float extratopz2 = reg->eceiling.GetPointZ(*seg->v2);
    const float extrabotz1 = reg->efloor.GetPointZ(*seg->v1);
    const float extrabotz2 = reg->efloor.GetPointZ(*seg->v2);

    // find opening for this side
    const float exbotz = min2(extrabotz1, extrabotz2);
    const float extopz = max2(extratopz1, extratopz2);

    for (opening_t *cop = ops; cop; cop = cop->next) {
      if (extopz <= cop->bottom || exbotz >= cop->top) {
        if (doDump) GCon->Logf("  (%g,%g): skip opening (%g,%g)", exbotz, extopz, cop->bottom, cop->top);
        continue;
      }
      // ok, we are at least partially in this opening

      float topz1 = cop->eceiling.GetPointZ(*seg->v1);
      float topz2 = cop->eceiling.GetPointZ(*seg->v2);
      float botz1 = cop->efloor.GetPointZ(*seg->v1);
      float botz2 = cop->efloor.GetPointZ(*seg->v2);

      if (doDump) GCon->Logf("  (%g,%g): HIT opening (%g,%g) (%g:%g,%g:%g); ex=(%g:%g,%g:%g)", exbotz, extopz, cop->bottom, cop->top, botz1, botz2, topz1, topz2, extrabotz1, extrabotz2, extratopz1, extratopz2);

      // check texture limits
      if (!wrapped) {
        if (max2(topz1, topz2) <= z_org-texh) continue;
        if (min2(botz1, botz2) >= z_org) continue;
      }

      wv[0].x = wv[1].x = seg->v1->x;
      wv[0].y = wv[1].y = seg->v1->y;
      wv[2].x = wv[3].x = seg->v2->x;
      wv[2].y = wv[3].y = seg->v2->y;

      if (wrapped) {
        wv[0].z = max2(extrabotz1, botz1);
        wv[1].z = min2(extratopz1, topz1);
        wv[2].z = min2(extratopz2, topz2);
        wv[3].z = max2(extrabotz2, botz2);
      } else {
        wv[0].z = max3(extrabotz1, botz1, z_org-texh);
        wv[1].z = min3(extratopz1, topz1, z_org);
        wv[2].z = min3(extratopz2, topz2, z_org);
        wv[3].z = max3(extrabotz2, botz2, z_org-texh);
      }

      if (doDump) for (int wf = 0; wf < 4; ++wf) GCon->Logf("   wf #%d: (%g,%g,%g)", wf, wv[wf].x, wv[wf].y, wv[wf].z);

      CreateWorldSurfFromWV(sub, seg, sp, wv, surface_t::TF_MIDDLE);
    }

    if (sp->surfs && (sp->texinfo.Alpha < 1.0f || sp->texinfo.Additive || MTex->isTranslucent())) {
      for (surface_t *sf = sp->surfs; sf; sf = sf->next) sf->drawflags |= surface_t::DF_NO_FACE_CULL;
    }
  } else {
    sp->texinfo.Alpha = 1.1f;
    sp->texinfo.Additive = false;
  }

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backTopDist = reg->eceiling.splane->dist;
  sp->backBotDist = reg->efloor.splane->dist;
  sp->TextureOffset = sidedef->Mid.TextureOffset;
  sp->RowOffset = sidedef->Mid.RowOffset;
}


//==========================================================================
//
//  VRenderLevelShared::CreateSegParts
//
//  create world/wall surfaces
//
//==========================================================================
void VRenderLevelShared::CreateSegParts (subsector_t *sub, drawseg_t *dseg, seg_t *seg, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling, sec_region_t *curreg, bool isMainRegion) {
  segpart_t *sp;

  dseg->seg = seg;
  dseg->next = seg->drawsegs;
  seg->drawsegs = dseg;

  if (!seg->linedef) return; // miniseg

  if (!isMainRegion) return;

  if (!seg->backsector) {
    // one sided line
    // middle wall
    dseg->mid = SurfCreatorGetPSPart();
    sp = dseg->mid;
    sp->basereg = curreg;
    SetupOneSidedMidWSurf(sub, seg, sp, r_floor, r_ceiling);

    // sky above line
    dseg->topsky = SurfCreatorGetPSPart();
    sp = dseg->topsky;
    sp->basereg = curreg;
    if (IsSky(r_ceiling.splane)) SetupOneSidedSkyWSurf(sub, seg, sp, r_floor, r_ceiling);
  } else {
    // two sided line
    // top wall
    dseg->top = SurfCreatorGetPSPart();
    sp = dseg->top;
    sp->basereg = curreg;
    SetupTwoSidedTopWSurf(sub, seg, sp, r_floor, r_ceiling);

    // sky above top
    dseg->topsky = SurfCreatorGetPSPart();
    dseg->topsky->basereg = curreg;
    if (IsSky(r_ceiling.splane) && !IsSky(&seg->backsector->ceiling)) {
      sp = dseg->topsky;
      SetupTwoSidedSkyWSurf(sub, seg, sp, r_floor, r_ceiling);
    }

    // bottom wall
    dseg->bot = SurfCreatorGetPSPart();
    sp = dseg->bot;
    sp->basereg = curreg;
    SetupTwoSidedBotWSurf(sub, seg, sp, r_floor, r_ceiling);

    // middle wall
    dseg->mid = SurfCreatorGetPSPart();
    sp = dseg->mid;
    sp->basereg = curreg;
    SetupTwoSidedMidWSurf(sub, seg, sp, r_floor, r_ceiling);

    // create region sides
    // this creates sides for neightbour 3d floors
    opening_t *ops = nullptr;
    bool opsCreated = false;
    for (sec_region_t *reg = seg->backsector->eregions->next; reg; reg = reg->next) {
      if (!reg->extraline) continue; // no need to create extra side

      sp = SurfCreatorGetPSPart();
      sp->basereg = reg;
      sp->next = dseg->extra;
      dseg->extra = sp;

      if (!opsCreated) {
        opsCreated = true;
        ops = SV_SectorOpenings(seg->frontsector);
      }
      SetupTwoSidedMidExtraWSurf(reg, sub, seg, sp, r_floor, r_ceiling, ops);
    }
  }
}


//==========================================================================
//
//  CheckCommonRecreateEx
//
//==========================================================================
static inline bool CheckCommonRecreateEx (segpart_t *sp, VTexture *NTex, const TPlane *floor, const TPlane *ceiling,
                                          const TPlane *backfloor, const TPlane *backceiling)
{
  if (!NTex) NTex = GTextureManager[GTextureManager.DefaultTexture];
  bool res =
    (ceiling ? FASI(sp->frontTopDist) != FASI(ceiling->dist) : false) ||
    (floor ? FASI(sp->frontBotDist) != FASI(floor->dist) : false) ||
    (backceiling ? FASI(sp->backTopDist) != FASI(backceiling->dist) : false) ||
    (backfloor ? FASI(sp->backBotDist) != FASI(backfloor->dist) : false) ||
    FASI(sp->texinfo.Tex->SScale) != FASI(NTex->SScale) ||
    FASI(sp->texinfo.Tex->TScale) != FASI(NTex->TScale) ||
    (sp->texinfo.Tex->Type == TEXTYPE_Null) != (NTex->Type == TEXTYPE_Null) ||
    sp->texinfo.Tex->GetHeight() != NTex->GetHeight() ||
    sp->texinfo.Tex->GetWidth() != NTex->GetWidth();
  // update texture, why not
  sp->texinfo.Tex = NTex;
  sp->texinfo.noDecals = NTex->noDecals;
  return res;
}


//==========================================================================
//
//  CheckCommonRecreate
//
//==========================================================================
static inline bool CheckCommonRecreate (seg_t *seg, segpart_t *sp, VTexture *NTex, const TPlane *floor, const TPlane *ceiling) {
  if (seg->backsector) {
    return CheckCommonRecreateEx(sp, NTex, floor, ceiling, &seg->backsector->floor, &seg->backsector->ceiling);
  } else {
    return CheckCommonRecreateEx(sp, NTex, floor, ceiling, nullptr, nullptr);
  }
}


//==========================================================================
//
//  CheckMidRecreate
//
//==========================================================================
static inline bool CheckMidRecreate (seg_t *seg, segpart_t *sp, const TPlane *floor, const TPlane *ceiling) {
  return CheckCommonRecreate(seg, sp, GTextureManager(seg->sidedef->MidTexture), floor, ceiling);
}


//==========================================================================
//
//  CheckTopRecreate
//
//==========================================================================
static inline bool CheckTopRecreate (seg_t *seg, segpart_t *sp, sec_plane_t *floor, sec_plane_t *ceiling) {
  sec_plane_t *back_ceiling = &seg->backsector->ceiling;
  VTexture *TTex = GTextureManager(seg->sidedef->TopTexture);
  if (IsSky(ceiling) && IsSky(back_ceiling) && ceiling->SkyBox != back_ceiling->SkyBox) {
    TTex = GTextureManager[skyflatnum];
  }
  return CheckCommonRecreate(seg, sp, TTex, floor, ceiling);
}


//==========================================================================
//
//  CheckBopRecreate
//
//==========================================================================
static inline bool CheckBopRecreate (seg_t *seg, segpart_t *sp, const TPlane *floor, const TPlane *ceiling) {
  return CheckCommonRecreate(seg, sp, GTextureManager(seg->sidedef->BottomTexture), floor, ceiling);
}


//==========================================================================
//
//  VRenderLevelShared::UpdateTextureOffsets
//
//==========================================================================
void VRenderLevelShared::UpdateTextureOffsets (subsector_t *sub, seg_t *seg, segpart_t *sp, const side_tex_params_t *tparam, const TPlane *plane) {
  bool reinitSurfs = false;

  if (FASI(sp->TextureOffset) != FASI(tparam->TextureOffset)) {
    reinitSurfs = true;
    sp->texinfo.soffs += (tparam->TextureOffset-sp->TextureOffset)*TextureOffsetSScale(sp->texinfo.Tex);
    sp->TextureOffset = tparam->TextureOffset;
  }

  if (FASI(sp->RowOffset) != FASI(tparam->RowOffset)) {
    reinitSurfs = true;
    sp->texinfo.toffs += (tparam->RowOffset-sp->RowOffset)*TextureOffsetTScale(sp->texinfo.Tex);
    sp->RowOffset = tparam->RowOffset;
  }

  if (reinitSurfs) {
    // do not recalculate static lightmaps
    InitSurfs(false, sp->surfs, &sp->texinfo, (plane ? plane : seg), sub);
  }
}


//==========================================================================
//
//  VRenderLevelShared::UpdateDrawSeg
//
//==========================================================================
void VRenderLevelShared::UpdateDrawSeg (subsector_t *sub, drawseg_t *dseg, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  seg_t *seg = dseg->seg;

  if (!seg->linedef) return; // miniseg

  if (!seg->backsector) {
    // one-sided seg
    // top sky
    segpart_t *sp = dseg->topsky;
    if (sp) {
      if (IsSky(r_ceiling.splane) && FASI(sp->frontTopDist) != FASI(r_ceiling.splane->dist)) {
        SetupOneSidedSkyWSurf(sub, seg, sp, r_floor, r_ceiling);
      }
      sp->texinfo.ColorMap = ColorMap;
    }

    // midtexture
    sp = dseg->mid;
    if (sp) {
      if (CheckMidRecreate(seg, sp, r_floor.splane, r_ceiling.splane)) {
        SetupOneSidedMidWSurf(sub, seg, sp, r_floor, r_ceiling);
      } else {
        UpdateTextureOffsets(sub, seg, sp, &seg->sidedef->Mid);
      }
      sp->texinfo.ColorMap = ColorMap;
    }
  } else {
    // two-sided seg
    sec_plane_t *back_ceiling = &seg->backsector->ceiling;

    // sky above top
    segpart_t *sp = dseg->topsky;
    if (sp) {
      if (IsSky(r_ceiling.splane) && !IsSky(back_ceiling) && FASI(sp->frontTopDist) != FASI(r_ceiling.splane->dist)) {
        SetupTwoSidedSkyWSurf(sub, seg, sp, r_floor, r_ceiling);
      }
      sp->texinfo.ColorMap = ColorMap;
    }

    // top wall
    sp = dseg->top;
    if (sp) {
      if (CheckTopRecreate(seg, sp, r_floor.splane, r_ceiling.splane)) {
        SetupTwoSidedTopWSurf(sub, seg, sp, r_floor, r_ceiling);
      } else {
        UpdateTextureOffsets(sub, seg, sp, &seg->sidedef->Top);
      }
      sp->texinfo.ColorMap = ColorMap;
    }

    // bottom wall
    sp = dseg->bot;
    if (sp) {
      if (CheckBopRecreate(seg, sp, r_floor.splane, r_ceiling.splane)) {
        SetupTwoSidedBotWSurf(sub, seg, sp, r_floor, r_ceiling);
      } else {
        UpdateTextureOffsets(sub, seg, sp, &seg->sidedef->Bot);
      }
      sp->texinfo.ColorMap = ColorMap;
    }

    // masked MidTexture
    sp = dseg->mid;
    if (sp) {
      if (CheckMidRecreate(seg, sp, r_floor.splane, r_ceiling.splane)) {
        SetupTwoSidedMidWSurf(sub, seg, sp, r_floor, r_ceiling);
      } else {
        UpdateTextureOffsets(sub, seg, sp, &seg->sidedef->Mid);
      }
      sp->texinfo.ColorMap = ColorMap;
      if (sp->texinfo.Tex->Type != TEXTYPE_Null) {
        sp->texinfo.Alpha = seg->linedef->alpha;
        sp->texinfo.Additive = !!(seg->linedef->flags&ML_ADDITIVE);
      } else {
        sp->texinfo.Alpha = 1.1f;
        sp->texinfo.Additive = false;
      }
    }

    opening_t *ops = nullptr;
    bool opsCreated = false;
    // update 3d floors
    for (sp = dseg->extra; sp; sp = sp->next) {
      sec_region_t *reg = sp->basereg;
      vassert(reg->extraline);
      side_t *extraside = &Level->Sides[reg->extraline->sidenum[0]];

      VTexture *MTex = GTextureManager(extraside->MidTexture);
      if (!MTex) MTex = GTextureManager[GTextureManager.DefaultTexture];

      if (CheckCommonRecreateEx(sp, MTex, r_floor.splane, r_ceiling.splane, reg->efloor.splane, reg->eceiling.splane)) {
        if (!opsCreated) {
          opsCreated = true;
          ops = SV_SectorOpenings(seg->frontsector);
        }
        SetupTwoSidedMidExtraWSurf(reg, sub, seg, sp, r_floor, r_ceiling, ops);
      } else {
        UpdateTextureOffsets(sub, seg, sp, &extraside->Mid);
      }
      sp->texinfo.ColorMap = ColorMap;
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::SegMoved
//
//  called when polyobject moved
//
//==========================================================================
void VRenderLevelShared::SegMoved (seg_t *seg) {
  if (!seg->drawsegs) return; // drawsegs not created yet
  if (!seg->linedef) Sys_Error("R_SegMoved: miniseg");
  if (!seg->drawsegs->mid) return; // no midsurf

  //k8: just in case
  if (seg->length <= 0.0f) {
    seg->dir = TVec(1, 0, 0); // arbitrary
  } else {
    seg->dir = ((*seg->v2)-(*seg->v1)).normalised2D();
    if (!seg->dir.isValid() || seg->dir.isZero()) seg->dir = TVec(1, 0, 0); // arbitrary
  }

  const side_t *sidedef = seg->sidedef;

  VTexture *MTex = seg->drawsegs->mid->texinfo.Tex;
  seg->drawsegs->mid->texinfo.saxis = seg->dir*(TextureSScale(MTex)*sidedef->Mid.ScaleX);
  seg->drawsegs->mid->texinfo.soffs = -DotProduct(*seg->v1, seg->drawsegs->mid->texinfo.saxis)+
                                      seg->offset*(TextureSScale(MTex)*sidedef->Mid.ScaleX)+
                                      sidedef->Mid.TextureOffset*TextureOffsetSScale(MTex);

  // force update
  seg->drawsegs->mid->frontTopDist += 0.346f;
}


//==========================================================================
//
//  VRenderLevelShared::CreateWorldSurfaces
//
//==========================================================================
void VRenderLevelShared::CreateWorldSurfaces () {
  inWorldCreation = true;

  vassert(!free_wsurfs);
  SetupSky();

  // set up fake floors
  for (int i = 0; i < Level->NumSectors; ++i) {
    if (Level->Sectors[i].heightsec || Level->Sectors[i].deepref) {
      SetupFakeFloors(&Level->Sectors[i]);
    }
  }

  if (inWorldCreation) {
    if (IsAdvancedRenderer()) {
      R_LdrMsgShowSecondary("CREATING WORLD SURFACES...");
    } else {
      R_LdrMsgShowSecondary("CALCULATING LIGHTMAPS...");
    }
    R_PBarReset();
  }

  // count regions in all subsectors
  int count = 0;
  int dscount = 0;
  int spcount = 0;
  subsector_t *sub = &Level->Subsectors[0];
  for (int i = Level->NumSubsectors; i--; ++sub) {
    if (!sub->sector->linecount) continue; // skip sectors containing original polyobjs
    count += 4*2; //k8: dunno
    for (sec_region_t *reg = sub->sector->eregions; reg; reg = reg->next) {
      ++count;
      dscount += sub->numlines;
      if (sub->HasPObjs()) {
        for (auto &&it : sub->PObjFirst()) dscount += it.value()->numsegs; // polyobj
      }
      for (int j = 0; j < sub->numlines; ++j) spcount += CountSegParts(&Level->Segs[sub->firstline+j]);
      if (sub->HasPObjs()) {
        for (auto &&it : sub->PObjFirst()) {
          polyobj_t *pobj = it.value();
          seg_t *const *polySeg = pobj->segs;
          for (int polyCount = pobj->numsegs; polyCount--; ++polySeg) spcount += CountSegParts(*polySeg);
        }
      }
    }
  }

  // get some memory
  subregion_t *sreg = new subregion_t[count+1];
  drawseg_t *pds = new drawseg_t[dscount+1];
  pspart = new segpart_t[spcount+1];

  pspartsLeft = spcount+1;
  int sregLeft = count+1;
  int pdsLeft = dscount+1;

  memset((void *)sreg, 0, sizeof(subregion_t)*sregLeft);
  memset((void *)pds, 0, sizeof(drawseg_t)*pdsLeft);
  memset((void *)pspart, 0, sizeof(segpart_t)*pspartsLeft);

  AllocatedSubRegions = sreg;
  AllocatedDrawSegs = pds;
  AllocatedSegParts = pspart;

  // create sector surfaces
  sub = &Level->Subsectors[0];
  for (int i = Level->NumSubsectors; i--; ++sub) {
    if (!sub->sector->linecount) continue; // skip sectors containing original polyobjs

    TSecPlaneRef main_floor = sub->sector->eregions->efloor;
    TSecPlaneRef main_ceiling = sub->sector->eregions->eceiling;

    subregion_t *lastsreg = sub->regions;

    int ridx = 0;
    for (sec_region_t *reg = sub->sector->eregions; reg; reg = reg->next, ++ridx) {
      if (sregLeft == 0) Sys_Error("out of subregions in surface creator");

      TSecPlaneRef r_floor, r_ceiling;
      r_floor = reg->efloor;
      r_ceiling = reg->eceiling;

      sreg->secregion = reg;
      sreg->floorplane = r_floor;
      sreg->ceilplane = r_ceiling;
      sreg->realfloor = (reg->regflags&sec_region_t::RF_SkipFloorSurf ? nullptr : CreateSecSurface(nullptr, sub, r_floor));
      sreg->realceil = (reg->regflags&sec_region_t::RF_SkipCeilSurf ? nullptr : CreateSecSurface(nullptr, sub, r_ceiling));

      // create fake floor and ceiling
      if (ridx == 0 && sub->sector->fakefloors) {
        TSecPlaneRef fakefloor, fakeceil;
        fakefloor.set(&sub->sector->fakefloors->floorplane, false);
        fakeceil.set(&sub->sector->fakefloors->ceilplane, false);
        if (!fakefloor.isFloor()) fakefloor.Flip();
        if (!fakeceil.isCeiling()) fakeceil.Flip();
        sreg->fakefloor = (reg->regflags&sec_region_t::RF_SkipFloorSurf ? nullptr : CreateSecSurface(nullptr, sub, fakefloor));
        sreg->fakeceil = (reg->regflags&sec_region_t::RF_SkipCeilSurf ? nullptr : CreateSecSurface(nullptr, sub, fakeceil));
      }

      sreg->count = sub->numlines;
      if (ridx == 0 && sub->HasPObjs()) {
        for (auto &&it : sub->PObjFirst()) sreg->count += it.value()->numsegs; // polyobj
      }
      if (pdsLeft < sreg->count) Sys_Error("out of drawsegs in surface creator");
      sreg->lines = pds;
      pds += sreg->count;
      pdsLeft -= sreg->count;
      for (int j = 0; j < sub->numlines; ++j) CreateSegParts(sub, &sreg->lines[j], &Level->Segs[sub->firstline+j], main_floor, main_ceiling, reg, (ridx == 0));

      if (ridx == 0 && sub->HasPObjs()) {
        // polyobj
        int j = sub->numlines;
        for (auto &&it : sub->PObjFirst()) {
          polyobj_t *pobj = it.value();
          seg_t **polySeg = pobj->segs;
          for (int polyCount = pobj->numsegs; polyCount--; ++polySeg, ++j) {
            CreateSegParts(sub, &sreg->lines[j], *polySeg, main_floor, main_ceiling, nullptr, true);
          }
        }
      }

      /*
      sreg->next = sub->regions;
      sub->regions = sreg;
      */
      // proper append
      sreg->next = nullptr;
      if (lastsreg) lastsreg->next = sreg; else sub->regions = sreg;
      lastsreg = sreg;

      ++sreg;
      --sregLeft;
    }

    if (inWorldCreation) R_PBarUpdate("Lighting", Level->NumSubsectors-i, Level->NumSubsectors);
  }

  InitialWorldUpdate();

  if (inWorldCreation) R_PBarUpdate("Lighting", Level->NumSubsectors, Level->NumSubsectors, true);

  inWorldCreation = false;
}


//==========================================================================
//
//  VRenderLevelShared::UpdateSubRegion
//
//==========================================================================
void VRenderLevelShared::UpdateSubRegion (subsector_t *sub, subregion_t *region) {
  if (!region || !sub) return;

  // polyobj cannot be in subsector with 3d floors, so update it once
  if (sub->HasPObjs()) {
    // update the polyobj
    for (auto &&it : sub->PObjFirst()) {
      polyobj_t *pobj = it.value();
      seg_t **polySeg = pobj->segs;
      for (int polyCount = pobj->numsegs; polyCount--; ++polySeg) {
        UpdateDrawSeg(sub, (*polySeg)->drawsegs, region->floorplane, region->ceilplane);
      }
    }
  }

  for (; region; region = region->next) {
    TSecPlaneRef r_floor = region->floorplane;
    TSecPlaneRef r_ceiling = region->ceilplane;

    drawseg_t *ds = region->lines;
    for (int count = sub->numlines; count--; ++ds) {
      UpdateDrawSeg(sub, ds, r_floor, r_ceiling/*, ClipSegs*/);
    }

    if (region->realfloor) UpdateSecSurface(region->realfloor, region->floorplane, sub);
    if (region->fakefloor) {
      TSecPlaneRef fakefloor;
      fakefloor.set(&sub->sector->fakefloors->floorplane, false);
      if (!fakefloor.isFloor()) fakefloor.Flip();
      if (!region->fakefloor->esecplane.isFloor()) region->fakefloor->esecplane.Flip();
      UpdateSecSurface(region->fakefloor, fakefloor, sub);
      //region->fakefloor->texinfo.Tex = GTextureManager[GTextureManager.DefaultTexture];
    }

    if (region->realceil) UpdateSecSurface(region->realceil, region->ceilplane, sub);
    if (region->fakeceil) {
      TSecPlaneRef fakeceil;
      fakeceil.set(&sub->sector->fakefloors->ceilplane, false);
      if (!fakeceil.isCeiling()) fakeceil.Flip();
      if (!region->fakeceil->esecplane.isCeiling()) region->fakeceil->esecplane.Flip();
      UpdateSecSurface(region->fakeceil, fakeceil, sub);
      //region->fakeceil->texinfo.Tex = GTextureManager[GTextureManager.DefaultTexture];
    }

    /* polyobj cannot be in 3d floor
    if (updatePoly && sub->HasPObjs()) {
      // update the polyobj
      updatePoly = false;
      for (auto &&it : sub->PObjFirst()) {
        polyobj_t *pobj = it.value();
        seg_t **polySeg = pobj->segs;
        for (int polyCount = pobj->numsegs; polyCount--; ++polySeg) {
          UpdateDrawSeg(sub, (*polySeg)->drawsegs, r_floor, r_ceiling);
        }
      }
    }
    */
  }
}


//==========================================================================
//
//  VRenderLevelShared::UpdateSubsectorFloorSurfaces
//
//==========================================================================
void VRenderLevelShared::UpdateSubsectorFlatSurfaces (subsector_t *sub, bool dofloors, bool doceils, bool forced) {
  if (!sub || (!dofloors && !doceils)) return;
  if (!forced && sub->updateWorldFrame == updateWorldFrame) return;
  for (subregion_t *region = sub->regions; region; region = region->next) {
    if (dofloors) {
      if (region->realfloor) UpdateSecSurface(region->realfloor, region->floorplane, sub, true); // ignore colormap
      if (region->fakefloor) {
        TSecPlaneRef fakefloor;
        fakefloor.set(&sub->sector->fakefloors->floorplane, false);
        if (!fakefloor.isFloor()) fakefloor.Flip();
        if (!region->fakefloor->esecplane.isFloor()) region->fakefloor->esecplane.Flip();
        UpdateSecSurface(region->fakefloor, fakefloor, sub, true); // ignore colormap
      }
    }
    if (doceils) {
      if (region->realceil) UpdateSecSurface(region->realceil, region->ceilplane, sub);
      if (region->fakeceil) {
        TSecPlaneRef fakeceil;
        fakeceil.set(&sub->sector->fakefloors->ceilplane, false);
        if (!fakeceil.isCeiling()) fakeceil.Flip();
        if (!region->fakeceil->esecplane.isCeiling()) region->fakeceil->esecplane.Flip();
        UpdateSecSurface(region->fakeceil, fakeceil, sub);
      }
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::CopyPlaneIfValid
//
//==========================================================================
bool VRenderLevelShared::CopyPlaneIfValid (sec_plane_t *dest, const sec_plane_t *source, const sec_plane_t *opp) {
  bool copy = false;

  // if the planes do not have matching slopes, then always copy them
  // because clipping would require creating new sectors
  if (source->normal != dest->normal) {
    copy = true;
  } else if (opp->normal != -dest->normal) {
    if (source->dist < dest->dist) copy = true;
  } else if (source->dist < dest->dist && source->dist > -opp->dist) {
    copy = true;
  }

  if (copy) *(TPlane *)dest = *(TPlane *)source;

  return copy;
}


//==========================================================================
//
//  VRenderLevelShared::UpdateFakeFlats
//
//  killough 3/7/98: Hack floor/ceiling heights for deep water etc.
//
//  If player's view height is underneath fake floor, lower the
//  drawn ceiling to be just under the floor height, and replace
//  the drawn floor and ceiling textures, and light level, with
//  the control sector's.
//
//  Similar for ceiling, only reflected.
//
//  killough 4/11/98, 4/13/98: fix bugs, add 'back' parameter
//
//  k8: this whole thing is a fuckin' mess. it should be rewritten.
//  we can simply create fakes, and let the renderer to the rest (i think).
//
//==========================================================================
void VRenderLevelShared::UpdateFakeFlats (sector_t *sector) {
  // replace sector being drawn with a copy to be hacked
  fakefloor_t *ff = sector->fakefloors;
  if (!ff) return; //k8:just in case

  // sector_t::SF_ClipFakePlanes: improved texture control
  //   the real floor and ceiling will be drawn with the real sector's flats
  //   the fake floor and ceiling (from either side) will be drawn with the control sector's flats
  //   the real floor and ceiling will be drawn even when in the middle part, allowing lifts into and out of deep water to render correctly (not possible in Boom)
  //   this flag does not work properly with sloped floors, and, if flag 2 is not set, with sloped ceilings either

  const sector_t *hs = sector->heightsec;
  sector_t *viewhs = r_viewleaf->sector->heightsec;
  /*
  bool underwater = / *r_fakingunderwater ||* /
    //(viewhs && vieworg.z <= viewhs->floor.GetPointZClamped(vieworg));
    (hs && vieworg.z <= hs->floor.GetPointZClamped(vieworg));
  */
  //bool underwater = (viewhs && vieworg.z <= viewhs->floor.GetPointZClamped(vieworg));
  bool underwater = (hs && vieworg.z <= hs->floor.GetPointZClamped(vieworg));
  bool underwaterView = (viewhs && vieworg.z <= viewhs->floor.GetPointZClamped(vieworg));
  bool diffTex = !!(hs && hs->SectorFlags&sector_t::SF_ClipFakePlanes);

  ff->floorplane = sector->floor;
  ff->ceilplane = sector->ceiling;
  ff->params = sector->params;
  /*
  if (!underwater && diffTex && (hs->SectorFlags&sector_t::SF_FakeFloorOnly)) {
    ff->floorplane = hs->floor;
    ff->floorplane.pic = GTextureManager.DefaultTexture;
    return;
  }
  */
  /*
    ff->ceilplane.normal = -hs->floor.normal;
    ff->ceilplane.dist = -hs->floor.dist;
    ff->ceilplane.pic = GTextureManager.DefaultTexture;
    return;
  */
  //if (!underwater && diffTex) ff->floorplane = hs->floor;
  //return;

  //GCon->Logf("sector=%d; hs=%d", (int)(ptrdiff_t)(sector-&Level->Sectors[0]), (int)(ptrdiff_t)(hs-&Level->Sectors[0]));

  // replace floor and ceiling height with control sector's heights
  if (diffTex && !(hs->SectorFlags&sector_t::SF_FakeCeilingOnly)) {
    if (CopyPlaneIfValid(&ff->floorplane, &hs->floor, &sector->ceiling)) {
      ff->floorplane.pic = hs->floor.pic;
      //GCon->Logf("opic=%d; fpic=%d", sector->floor.pic.id, hs->floor.pic.id);
    } else if (hs && (hs->SectorFlags&sector_t::SF_FakeFloorOnly)) {
      if (underwater) {
        //GCon->Logf("viewhs=%s", (viewhs ? "tan" : "ona"));
        //tempsec->ColorMap = hs->ColorMap;
        ff->params.Fade = hs->params.Fade;
        if (!(hs->SectorFlags&sector_t::SF_NoFakeLight)) {
          ff->params.lightlevel = hs->params.lightlevel;
          ff->params.LightColor = hs->params.LightColor;
          /*
          if (floorlightlevel != nullptr) *floorlightlevel = GetFloorLight(hs);
          if (ceilinglightlevel != nullptr) *ceilinglightlevel = GetFloorLight(hs);
          */
          //ff->floorplane = (viewhs ? viewhs->floor : sector->floor);
        }
        ff->ceilplane = hs->floor;
        ff->ceilplane.flipInPlace();
        //ff->ceilplane.normal = -hs->floor.normal;
        //ff->ceilplane.dist = -hs->floor.dist;
        //ff->ceilplane.pic = GTextureManager.DefaultTexture;
        //ff->ceilplane.pic = hs->floor.pic;
      } else {
        ff->floorplane = hs->floor;
        //ff->floorplane.pic = hs->floor.pic;
        //ff->floorplane.pic = GTextureManager.DefaultTexture;
      }
      return;
    }
  } else {
    if (hs && !(hs->SectorFlags&sector_t::SF_FakeCeilingOnly)) {
      //ff->floorplane.normal = hs->floor.normal;
      //ff->floorplane.dist = hs->floor.dist;
      //GCon->Logf("  000");
      if (!underwater) *(TPlane *)&ff->floorplane = *(TPlane *)&hs->floor;
      //*(TPlane *)&ff->floorplane = *(TPlane *)&sector->floor;
      //CopyPlaneIfValid(&ff->floorplane, &hs->floor, &sector->ceiling);
    }
  }


  if (hs && !(hs->SectorFlags&sector_t::SF_FakeFloorOnly)) {
    if (diffTex) {
      if (CopyPlaneIfValid(&ff->ceilplane, &hs->ceiling, &sector->floor)) {
        ff->ceilplane.pic = hs->ceiling.pic;
      }
    } else {
      //ff->ceilplane.normal = hs->ceiling.normal;
      //ff->ceilplane.dist = hs->ceiling.dist;
      //GCon->Logf("  001");
      *(TPlane *)&ff->ceilplane = *(TPlane *)&hs->ceiling;
    }
  }

  //float refflorz = hs->floor.GetPointZClamped(viewx, viewy);
  float refceilz = (hs ? hs->ceiling.GetPointZClamped(vieworg) : 0); // k8: was `nullptr` -- wtf?!
  //float orgflorz = sector->floor.GetPointZClamped(viewx, viewy);
  float orgceilz = sector->ceiling.GetPointZClamped(vieworg);

  if (underwater /*||(viewhs && vieworg.z <= viewhs->floor.GetPointZClamped(vieworg))*/) {
    //!ff->floorplane.normal = sector->floor.normal;
    //!ff->floorplane.dist = sector->floor.dist;
    //!ff->ceilplane.normal = -hs->floor.normal;
    //!ff->ceilplane.dist = -hs->floor.dist/* - -hs->floor.normal.z*/;
    *(TPlane *)&ff->floorplane = *(TPlane *)&sector->floor;
    *(TPlane *)&ff->ceilplane = *(TPlane *)&hs->ceiling;
    //ff->ColorMap = hs->ColorMap;
    if (underwaterView) ff->params.Fade = hs->params.Fade;
  }

  // killough 11/98: prevent sudden light changes from non-water sectors:
  if ((underwater /*&& !back*/) || underwaterView) {
    // head-below-floor hack
    ff->floorplane.pic = (diffTex ? sector->floor.pic : hs->floor.pic);
    ff->floorplane.xoffs = hs->floor.xoffs;
    ff->floorplane.yoffs = hs->floor.yoffs;
    ff->floorplane.XScale = hs->floor.XScale;
    ff->floorplane.YScale = hs->floor.YScale;
    ff->floorplane.Angle = hs->floor.Angle;
    ff->floorplane.BaseAngle = hs->floor.BaseAngle;
    ff->floorplane.BaseYOffs = hs->floor.BaseYOffs;
    //ff->floorplane = hs->floor;
    //*(TPlane *)&ff->floorplane = *(TPlane *)&sector->floor;
    //ff->floorplane.dist -= 42;
    //ff->floorplane.dist += 9;

    ff->ceilplane.normal = -hs->floor.normal;
    ff->ceilplane.dist = -hs->floor.dist/* - -hs->floor.normal.z*/;
    //ff->ceilplane.pic = GTextureManager.DefaultTexture;
    //GCon->Logf("!!!");
    if (hs->ceiling.pic == skyflatnum) {
      ff->floorplane.normal = -ff->ceilplane.normal;
      ff->floorplane.dist = -ff->ceilplane.dist/* - ff->ceilplane.normal.z*/;
      ff->ceilplane.pic = ff->floorplane.pic;
      ff->ceilplane.xoffs = ff->floorplane.xoffs;
      ff->ceilplane.yoffs = ff->floorplane.yoffs;
      ff->ceilplane.XScale = ff->floorplane.XScale;
      ff->ceilplane.YScale = ff->floorplane.YScale;
      ff->ceilplane.Angle = ff->floorplane.Angle;
      ff->ceilplane.BaseAngle = ff->floorplane.BaseAngle;
      ff->ceilplane.BaseYOffs = ff->floorplane.BaseYOffs;
    } else {
      ff->ceilplane.pic = (diffTex ? sector->floor.pic : hs->ceiling.pic);
      ff->ceilplane.xoffs = hs->ceiling.xoffs;
      ff->ceilplane.yoffs = hs->ceiling.yoffs;
      ff->ceilplane.XScale = hs->ceiling.XScale;
      ff->ceilplane.YScale = hs->ceiling.YScale;
      ff->ceilplane.Angle = hs->ceiling.Angle;
      ff->ceilplane.BaseAngle = hs->ceiling.BaseAngle;
      ff->ceilplane.BaseYOffs = hs->ceiling.BaseYOffs;
    }

    // k8: why underwaterView? because of kdizd bugs
    //     this seems to be totally wrong, though
    if (!(hs->SectorFlags&sector_t::SF_NoFakeLight) && /*underwaterView*/viewhs) {
      ff->params.lightlevel = hs->params.lightlevel;
      ff->params.LightColor = hs->params.LightColor;
      /*
      if (floorlightlevel != nullptr) *floorlightlevel = GetFloorLight(hs);
      if (ceilinglightlevel != nullptr) *ceilinglightlevel = GetFloorLight(hs);
      */
    }
  } else if (((hs && vieworg.z > hs->ceiling.GetPointZClamped(vieworg)) || //k8: dunno, it was `floor` there, and it seems to be a typo
              (viewhs && vieworg.z > viewhs->ceiling.GetPointZClamped(vieworg))) &&
             orgceilz > refceilz && !(hs->SectorFlags&sector_t::SF_FakeFloorOnly))
  {
    // above-ceiling hack
    ff->ceilplane.normal = hs->ceiling.normal;
    ff->ceilplane.dist = hs->ceiling.dist;
    ff->floorplane.normal = -hs->ceiling.normal;
    ff->floorplane.dist = -hs->ceiling.dist;
    ff->params.Fade = hs->params.Fade;
    //ff->params.ColorMap = hs->params.ColorMap;

    ff->ceilplane.pic = diffTex ? sector->ceiling.pic : hs->ceiling.pic;
    ff->floorplane.pic = hs->ceiling.pic;
    ff->floorplane.xoffs = ff->ceilplane.xoffs = hs->ceiling.xoffs;
    ff->floorplane.yoffs = ff->ceilplane.yoffs = hs->ceiling.yoffs;
    ff->floorplane.XScale = ff->ceilplane.XScale = hs->ceiling.XScale;
    ff->floorplane.YScale = ff->ceilplane.YScale = hs->ceiling.YScale;
    ff->floorplane.Angle = ff->ceilplane.Angle = hs->ceiling.Angle;
    ff->floorplane.BaseAngle = ff->ceilplane.BaseAngle = hs->ceiling.BaseAngle;
    ff->floorplane.BaseYOffs = ff->ceilplane.BaseYOffs = hs->ceiling.BaseYOffs;

    if (hs->floor.pic != skyflatnum) {
      ff->ceilplane.normal = sector->ceiling.normal;
      ff->ceilplane.dist = sector->ceiling.dist;
      ff->floorplane.pic = hs->floor.pic;
      ff->floorplane.xoffs = hs->floor.xoffs;
      ff->floorplane.yoffs = hs->floor.yoffs;
      ff->floorplane.XScale = hs->floor.XScale;
      ff->floorplane.YScale = hs->floor.YScale;
      ff->floorplane.Angle = hs->floor.Angle;
    }

    // k8: why underwaterView? because of kdizd bugs
    //     this seems to be totally wrong, though
    if (!(hs->SectorFlags&sector_t::SF_NoFakeLight) && viewhs) {
      ff->params.lightlevel = hs->params.lightlevel;
      ff->params.LightColor = hs->params.LightColor;
    }
  } else {
    if (diffTex) {
      ff->floorplane = hs->floor;
      ff->ceilplane = hs->floor;
      if (vieworg.z < hs->floor.GetPointZClamped(vieworg)) {
        // fake floor is actually a ceiling now
        ff->floorplane.flipInPlace();
      }
      if (vieworg.z > hs->ceiling.GetPointZClamped(vieworg)) {
        // fake ceiling is actually a floor now
        ff->ceilplane.flipInPlace();
      }
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::UpdateDeepWater
//
//==========================================================================
void VRenderLevelShared::UpdateDeepWater (sector_t *sector) {
  if (!sector) return; // just in case
  const sector_t *ds = sector->deepref;

  if (!ds) return; // just in case

  // replace sector being drawn with a copy to be hacked
  fakefloor_t *ff = sector->fakefloors;
  if (!ff) return; //k8:just in case
  ff->floorplane = sector->floor;
  ff->ceilplane = sector->ceiling;
  ff->params = sector->params;

  ff->floorplane.normal = ds->floor.normal;
  ff->floorplane.dist = ds->floor.dist;
}


//==========================================================================
//
//  VRenderLevelShared::UpdateFloodBug
//
//  emulate floodfill bug
//
//==========================================================================
void VRenderLevelShared::UpdateFloodBug (sector_t *sector) {
  if (!sector) return; // just in case
  fakefloor_t *ff = sector->fakefloors;
  if (!ff) return; // just in case
  //GCon->Logf("UpdateFloodBug: sector #%d (bridge: %s)", (int)(ptrdiff_t)(sector-&Level->Sectors[0]), (sector->SectorFlags&sector_t::SF_HangingBridge ? "tan" : "ona"));
  if (sector->SectorFlags&sector_t::SF_HangingBridge) {
    sector_t *sursec = sector->othersecFloor;
    ff->floorplane = sursec->floor;
    // ceiling must be current sector's floor, flipped
    ff->ceilplane = sector->floor;
    ff->ceilplane.flipInPlace();
    ff->params = sursec->params;
    //GCon->Logf("  floor: (%g,%g,%g : %g)", ff->floorplane.normal.x, ff->floorplane.normal.y, ff->floorplane.normal.z, ff->floorplane.dist);
    return;
  }
  // replace sector being drawn with a copy to be hacked
  ff->floorplane = sector->floor;
  ff->ceilplane = sector->ceiling;
  ff->params = sector->params;
  // floor
  if (sector->othersecFloor && sector->floor.minz < sector->othersecFloor->floor.minz) {
    ff->floorplane = sector->othersecFloor->floor;
    ff->params = sector->othersecFloor->params;
    ff->floorplane.LightSourceSector = (int)(ptrdiff_t)(sector->othersecFloor-Level->Sectors);
  }
  if (sector->othersecCeiling && sector->ceiling.minz > sector->othersecCeiling->ceiling.minz) {
    ff->ceilplane = sector->othersecCeiling->ceiling;
    ff->params = sector->othersecCeiling->params;
    ff->ceilplane.LightSourceSector = (int)(ptrdiff_t)(sector->othersecCeiling-Level->Sectors);
  }
}


//==========================================================================
//
//  VRenderLevelShared::SetupFakeFloors
//
//==========================================================================
void VRenderLevelShared::SetupFakeFloors (sector_t *sector) {
  if (!sector->deepref) {
    if (sector->heightsec->SectorFlags&sector_t::SF_IgnoreHeightSec) return;
  }

  if (!sector->fakefloors) sector->fakefloors = new fakefloor_t;
  memset((void *)sector->fakefloors, 0, sizeof(fakefloor_t));
  sector->fakefloors->floorplane = sector->floor;
  sector->fakefloors->ceilplane = sector->ceiling;
  sector->fakefloors->params = sector->params;

  sector->eregions->params = &sector->fakefloors->params;
}


//==========================================================================
//
//  VRenderLevelShared::ReallocSurface
//
//  free all surfaces except the first one, clear first, set
//  number of vertices to vcount
//
//==========================================================================
surface_t *VRenderLevelShared::ReallocSurface (surface_t *surfs, int vcount) {
  vassert(vcount > 2); // just in case
  surface_t *surf = surfs;
  if (surf) {
    // clear first surface
    if (surf->CacheSurf) FreeSurfCache(surf->CacheSurf);
    if (surf->lightmap) Z_Free(surf->lightmap);
    if (surf->lightmap_rgb) Z_Free(surf->lightmap_rgb);
    // free extra surfaces
    surface_t *next;
    for (surface_t *s = surfs->next; s; s = next) {
      if (s->CacheSurf) FreeSurfCache(s->CacheSurf);
      if (s->lightmap) Z_Free(s->lightmap);
      if (s->lightmap_rgb) Z_Free(s->lightmap_rgb);
      next = s->next;
      Z_Free(s);
    }
    surf->next = nullptr;
    // realloc first surface (if necessary)
    if (surf->count != vcount) {
      const size_t msize = sizeof(surface_t)+(vcount-1)*sizeof(TVec);
      surf = (surface_t *)Z_Realloc(surf, msize);
      memset((void *)surf, 0, msize);
    } else {
      memset((void *)surf, 0, sizeof(surface_t)+(vcount-1)*sizeof(TVec));
    }
    surf->count = vcount;
  } else {
    surf = (surface_t *)Z_Calloc(sizeof(surface_t)+(vcount-1)*sizeof(TVec));
    surf->count = vcount;
  }
  return surf;
}


//==========================================================================
//
//  VRenderLevelShared::FreeSurfaces
//
//==========================================================================
void VRenderLevelShared::FreeSurfaces (surface_t *InSurf) {
  surface_t *next;
  for (surface_t *s = InSurf; s; s = next) {
    if (s->CacheSurf) FreeSurfCache(s->CacheSurf);
    if (s->lightmap) Z_Free(s->lightmap);
    if (s->lightmap_rgb) Z_Free(s->lightmap_rgb);
    next = s->next;
    Z_Free(s);
  }
}


//==========================================================================
//
//  VRenderLevelShared::FreeSegParts
//
//==========================================================================
void VRenderLevelShared::FreeSegParts (segpart_t *ASP) {
  for (segpart_t *sp = ASP; sp; sp = sp->next) FreeWSurfs(sp->surfs);
}
