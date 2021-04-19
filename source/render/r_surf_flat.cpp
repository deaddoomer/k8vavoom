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
static VCvarB r_hack_zero_sky("r_hack_zero_sky", true, "ZeroSky hack (Doom II MAP01 extra floor fix).", CVAR_Archive);


//**************************************************************************
//**
//**  Flat surfaces
//**
//**************************************************************************

//==========================================================================
//
//  VRenderLevelShared::SetupSky
//
//==========================================================================
void VRenderLevelShared::SetupSky () {
  skyheight = -99999.0f;
  for (auto &&sec : Level->allSectors()) {
    if (sec.ceiling.pic == skyflatnum && sec.ceiling.maxz > skyheight) {
      skyheight = sec.ceiling.maxz;
    }
  }
  // make it a bit higher to avoid clipping of the sprites
  if (skyheight < -32768.0f) skyheight = -32768.0f;
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
//  IsZeroSkyFloorHack
//
//==========================================================================
static inline bool IsZeroSkyFloorHack (const subsector_t *sub, const sec_region_t *reg) {
  // if current sector has zero height, and its ceiling is sky, and its floor is not sky, skip floor creation
  return
    (reg->regflags&sec_region_t::RF_BaseRegion) &&
    reg->eceiling.splane->pic == skyflatnum &&
    reg->efloor.splane->pic != skyflatnum &&
    sub->sector->floor.normal.z == 1.0f && sub->sector->ceiling.normal.z == -1.0f &&
    sub->sector->floor.minz == sub->sector->ceiling.minz;
}


//==========================================================================
//
//  SurfRecalcFlatOffset
//
//  returns `true` if any offset was changed
//
//==========================================================================
static inline void SurfRecalcFlatOffset (sec_surface_t *surf, const TSecPlaneRef &spl, VTexture *Tex) {
  float newsoffs = spl.splane->xoffs*(VRenderLevelShared::TextureSScale(Tex)*spl.splane->XScale);
  float newtoffs = (spl.splane->yoffs+spl.splane->BaseYOffs)*(VRenderLevelShared::TextureTScale(Tex)*spl.splane->YScale);

  const float cx = spl.splane->PObjCX;
  const float cy = spl.splane->PObjCY;
  if (cx || cy) {
    TVec p(cx, cy);
    newsoffs -= DotProduct(surf->texinfo.saxis, p);
    newtoffs -= DotProduct(surf->texinfo.taxis, p);
  }

  surf->texinfo.soffs = newsoffs;
  surf->texinfo.toffs = newtoffs;
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
sec_surface_t *VRenderLevelShared::CreateSecSurface (sec_surface_t *ssurf, subsector_t *sub, TSecPlaneRef InSplane, subregion_t *sreg, bool fake) {
  int vcount = sub->numlines;

  if (vcount < 3) {
    GCon->Logf(NAME_Warning, "CreateSecSurface: subsector #%d of sector #%d has only #%d seg%s", (int)(ptrdiff_t)(sub-Level->Subsectors), (int)(ptrdiff_t)(sub->sector-Level->Sectors), vcount, (vcount != 1 ? "s" : ""));
    if (vcount < 1) Sys_Error("ZERO SEGS. WTF?!");
    if (!ssurf) {
      // new sector surface
      ssurf = new sec_surface_t;
      memset((void *)ssurf, 0, sizeof(sec_surface_t));
    } else {
      vassert(!ssurf->surfs);
    }
    TSecPlaneRef spl(InSplane);
    ssurf->esecplane = spl;
    ssurf->edist = spl.splane->dist;
    ssurf->XScale = 1.0f;
    ssurf->YScale = 1.0f;
    return ssurf;
  }
  //vassert(vcount >= 3);

  // if current sector has zero height, and its ceiling is sky, and its floor is not sky, skip floor creation
  // this is what removes extra floors on Doom II MAP01, for example
  if (!fake && r_hack_zero_sky && IsZeroSkyFloorHack(sub, sreg->secregion)) {
    sreg->flags |= subregion_t::SRF_ZEROSKY_FLOOR_HACK;
    // we still need to create this floor, because it may be reactivated later
  } else {
    sreg->flags &= ~subregion_t::SRF_ZEROSKY_FLOOR_HACK;
  }

  // if we're simply changing sky, and already have surface created, do not recreate it, it is pointless
  bool isSkyFlat = (InSplane.splane->pic == skyflatnum);
  bool recreateSurface = true;
  bool updateZ = false;

  //if (R_IsStackedSectorPlane(InSplane.splane)) isSkyFlat = true;

  // fix plane
  TSecPlaneRef spl(InSplane);
  if (isSkyFlat && spl.GetNormalZ() < 0.0f) spl.set(&sky_plane, false);

  surface_t *surf = nullptr;
  if (!ssurf) {
    // new sector surface
    ssurf = new sec_surface_t;
    memset((void *)ssurf, 0, sizeof(sec_surface_t));
    surf = NewWSurf(vcount);
  } else {
    // change sector surface
    // we still may have to recreate it if it was a "sky <-> non-sky" change, so check for it
    recreateSurface = !isSkyFlat || ((ssurf->esecplane.splane->pic == skyflatnum) != isSkyFlat) || sreg->IsForcedRecreation();
    if (recreateSurface) {
      //if (sub->isInnerPObj()) GCon->Logf("***  RECREATING!");
      surf = ReallocSurface(ssurf->surfs, vcount);
    } else {
      updateZ = (FASI(ssurf->edist) != FASI(spl.splane->dist));
      surf = ssurf->surfs;
    }
    ssurf->surfs = nullptr; // just in case
  }

  vuint32 typeFlags = (spl.GetNormalZ() > 0.0f ? surface_t::TF_FLOOR : surface_t::TF_CEILING);
  const sec_region_t *reg = sreg->secregion;
  if ((reg->regflags&(sec_region_t::RF_BaseRegion|sec_region_t::RF_SaneRegion)) == 0) {
    if (reg->regflags&(sec_region_t::RF_NonSolid|sec_region_t::RF_OnlyVisual)) typeFlags |= surface_t::TF_3DFLOOR;
  }

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
    //k8: do we really need to rotate lightmap axes? i don't think so
    #if 0
    ssurf->texinfo.saxisLM = TVec(c,  s);
    ssurf->texinfo.taxisLM = TVec(s, -c);
    #else
    ssurf->texinfo.saxisLM = TVec(+1.0f,  0.0f);
    ssurf->texinfo.taxisLM = TVec( 0.0f, -1.0f);
    #endif
    ssurf->texinfo.saxis = TVec(c,  s)*(TextureSScale(Tex)*spl.splane->XScale);
    ssurf->texinfo.taxis = TVec(s, -c)*(TextureTScale(Tex)*spl.splane->YScale);
  } else {
    ssurf->texinfo.taxisLM = TVec(0.0f, 0.0f, -1.0f);
    ssurf->texinfo.saxisLM = Normalise(CrossProduct(spl.GetNormal(), ssurf->texinfo.taxisLM));
    ssurf->texinfo.taxis = ssurf->texinfo.taxisLM*(TextureTScale(Tex)*spl.splane->YScale);
    ssurf->texinfo.saxis = ssurf->texinfo.saxisLM*(TextureSScale(Tex)*spl.splane->XScale);
  }

  /*bool offsChanged = */SurfRecalcFlatOffset(ssurf, spl, Tex);

  ssurf->texinfo.Tex = Tex;
  ssurf->texinfo.noDecals = (Tex ? Tex->noDecals : true);
  ssurf->texinfo.Alpha = (spl.splane->Alpha < 1.0f ? spl.splane->Alpha : 1.1f);
  ssurf->texinfo.Additive = !!(spl.splane->flags&SPF_ADDITIVE);
  ssurf->texinfo.ColorMap = 0;
  ssurf->XScale = spl.splane->XScale;
  ssurf->YScale = spl.splane->YScale;
  ssurf->Angle = spl.splane->BaseAngle-spl.splane->Angle;

  TPlane plane = *(TPlane *)spl.splane;
  if (spl.flipped) plane.FlipInPlace();

  if (recreateSurface) {
    surf->plane = plane;
    surf->count = vcount;
    const seg_t *seg = &Level->Segs[sub->firstline];
    SurfVertex *dptr = surf->verts;
    if (spl.GetNormalZ() < 0.0f) {
      // backward
      for (seg += (vcount-1); vcount--; ++dptr, --seg) {
        const TVec &v = *seg->v1;
        dptr->setVec(v.x, v.y, spl.GetPointZ(v.x, v.y));
      }
    } else {
      // forward
      for (; vcount--; ++dptr, ++seg) {
        const TVec &v = *seg->v1;
        dptr->setVec(v.x, v.y, spl.GetPointZ(v.x, v.y));
      }
    }

    if (isSkyFlat) {
      // don't subdivide sky, as it cannot have lightmap
      // but we still need to fix possible t-junctions
      //ssurf->surfs = surf;
      ssurf->surfs = SubdivideFace(surf, sreg, ssurf, ssurf->texinfo.saxisLM, &ssurf->texinfo.taxisLM, &plane, false);
      vassert(!ssurf->surfs->next);
      surf = ssurf->surfs; // if may be changed
      surf->texinfo = &ssurf->texinfo;
      surf->plane = plane;
    } else {
      //!GCon->Logf(NAME_Debug, "sfcF:%p: saxis=(%g,%g,%g); taxis=(%g,%g,%g); saxisLM=(%g,%g,%g); taxisLM=(%g,%g,%g)", ssurf, ssurf->texinfo.saxis.x, ssurf->texinfo.saxis.y, ssurf->texinfo.saxis.z, ssurf->texinfo.taxis.x, ssurf->texinfo.taxis.y, ssurf->texinfo.taxis.z, ssurf->texinfo.saxisLM.x, ssurf->texinfo.saxisLM.y, ssurf->texinfo.saxisLM.z, ssurf->texinfo.taxisLM.x, ssurf->texinfo.taxisLM.y, ssurf->texinfo.taxisLM.z);
      ssurf->surfs = SubdivideFace(surf, sreg, ssurf, ssurf->texinfo.saxisLM, &ssurf->texinfo.taxisLM, &plane);
      surf = ssurf->surfs; // if may be changed
      InitSurfs(true, ssurf->surfs, &ssurf->texinfo, &plane, sub, nullptr); // recalc static lightmaps
    }
  } else if (updateZ) {
    // update z coords
    bool changed = false;
    for (; surf; surf = surf->next) {
      SurfVertex *svert = surf->verts;
      for (int i = surf->count; i--; ++svert) {
        const float oldZ = svert->z;
        svert->z = spl.GetPointZ(svert->x, svert->y);
        if (!changed && FASI(oldZ) != FASI(svert->z)) changed = true;
      }
      if (changed) InitSurfs(true, ssurf->surfs, &ssurf->texinfo, &plane, sub, nullptr); // recalc static lightmaps
    }
  }
  /*k8: no, lightmap doesn't depend of texture axes anymore
  else if (offsChanged) {
    // still have to force it, because texture is scrolled, and lightmap s/t are invalid
    InitSurfs(true, ssurf->surfs, &ssurf->texinfo, &plane, sub, nullptr);
  }
  */

  return ssurf;
}


//==========================================================================
//
//  VRenderLevelShared::UpdateSecSurface
//
//  this is used to update floor and ceiling surfaces
//
//==========================================================================
void VRenderLevelShared::UpdateSecSurface (sec_surface_t *ssurf, TSecPlaneRef RealPlane, subsector_t *sub, subregion_t *sreg, bool ignoreColorMap, bool fake) {
  if (!ssurf->esecplane.splane->pic) return; // no texture? nothing to do

  TSecPlaneRef splane(ssurf->esecplane);

  if (sreg->IsForcedRecreation()) {
    sec_surface_t *newsurf = CreateSecSurface(ssurf, sub, RealPlane, sreg, fake);
    vassert(newsurf == ssurf); // sanity check
    ssurf->texinfo.ColorMap = ColorMap; // just in case
    // nothing more to do
    return;
  }

  if (splane.splane != RealPlane.splane) {
    // check for sky changes
    if ((splane.splane->pic == skyflatnum) != (RealPlane.splane->pic == skyflatnum)) {
      // sky <-> non-sky, simply recreate it
      sec_surface_t *newsurf = CreateSecSurface(ssurf, sub, RealPlane, sreg, fake);
      vassert(newsurf == ssurf); // sanity check
      ssurf->texinfo.ColorMap = ColorMap; // just in case
      // nothing more to do
      return;
    }
    // substitute real plane with sky plane if necessary
    if (RealPlane.splane->pic == skyflatnum && RealPlane.GetNormalZ() < 0.0f) {
      if (splane.splane != &sky_plane) {
        // recreate it, just in case
        sec_surface_t *newsurf = CreateSecSurface(ssurf, sub, RealPlane, sreg, fake);
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
    sec_surface_t *newsurf = CreateSecSurface(ssurf, sub, RealPlane, sreg, fake);
    vassert(newsurf == ssurf); // sanity check
    ssurf->texinfo.ColorMap = (!ignoreColorMap ? ColorMap : 0); // just in case
    // nothing more to do
    return;
  }

  if (!ignoreColorMap) ssurf->texinfo.ColorMap = ColorMap; // just in case
  // ok, we still may need to update texture or z coords
  // update texture?
  VTexture *Tex = GTextureManager(splane.splane->pic);
  if (!Tex) Tex = GTextureManager[GTextureManager.DefaultTexture];
  /*bool offsChanged = */SurfRecalcFlatOffset(ssurf, splane, Tex);
  ssurf->texinfo.Tex = Tex;
  ssurf->texinfo.noDecals = Tex->noDecals;

  // update z coords?
  if (FASI(ssurf->edist) != FASI(splane.splane->dist)) {
    TPlane plane = *(TPlane *)splane.splane;
    if (splane.flipped) plane.FlipInPlace();
    bool changed = false;
    ssurf->edist = splane.splane->dist;
    for (surface_t *surf = ssurf->surfs; surf; surf = surf->next) {
      surf->plane = plane;
      SurfVertex *svert = surf->verts;
      for (int i = surf->count; i--; ++svert) {
        const float oldZ = svert->z;
        svert->z = splane.GetPointZ(svert->x, svert->y);
        if (!changed && FASI(oldZ) != FASI(svert->z)) changed = true;
      }
    }
    // force lightmap recalculation
    if (changed || splane.splane->pic != skyflatnum) {
      InitSurfs(true, ssurf->surfs, &ssurf->texinfo, &plane, sub, nullptr); // recalc static lightmaps
    }
  }
  /*k8: no, lightmap doesn't depend of texture axes anymore
  else if (offsChanged && splane.splane->pic != skyflatnum) {
    // still have to force it, because texture is scrolled, and lightmap s/t are invalid
    TPlane plane = *(TPlane *)splane.splane;
    if (splane.flipped) plane.FlipInPlace();
    InitSurfs(true, ssurf->surfs, &ssurf->texinfo, &plane, sub, nullptr);
  }
  */
}
