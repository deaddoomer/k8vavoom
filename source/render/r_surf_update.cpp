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
#include "r_surf_utils.cpp"
#include "r_surf_utils_check.cpp"


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
    //if (seg->pobj) GCon->Logf(NAME_Debug, "pobj #%d seg; OFFSETING", seg->pobj->index);
    InitSurfs(false, sp->surfs, &sp->texinfo, (plane ? plane : seg), sub);
  }
}


//==========================================================================
//
//  VRenderLevelShared::UpdateTextureOffsetsEx
//
//==========================================================================
void VRenderLevelShared::UpdateTextureOffsetsEx (subsector_t *sub, seg_t *seg, segpart_t *sp, const side_tex_params_t *tparam, const side_tex_params_t *tparam2) {
  bool reinitSurfs = false;

  const float ctofs = tparam->TextureOffset+tparam2->TextureOffset;
  if (FASI(sp->TextureOffset) != FASI(ctofs)) {
    reinitSurfs = true;
    sp->texinfo.soffs += (ctofs-sp->TextureOffset)*TextureOffsetSScale(sp->texinfo.Tex);
    sp->TextureOffset = ctofs;
  }

  const float rwofs = tparam->RowOffset+tparam2->RowOffset;
  if (FASI(sp->RowOffset) != FASI(rwofs)) {
    reinitSurfs = true;
    sp->texinfo.toffs += (rwofs-sp->RowOffset)*TextureOffsetTScale(sp->texinfo.Tex);
    sp->RowOffset = rwofs;
  }

  if (reinitSurfs) {
    // do not recalculate static lightmaps
    InitSurfs(false, sp->surfs, &sp->texinfo, seg, sub);
  }
}


//==========================================================================
//
//  VRenderLevelShared::UpdateDrawSeg
//
//==========================================================================
void VRenderLevelShared::UpdateDrawSeg (subsector_t *sub, drawseg_t *dseg, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  if (!dseg) return; // just in case
  seg_t *seg = dseg->seg;
  if (!seg) return; // just in case

  if (!seg->linedef) return; // miniseg
  bool needTJ = false;

  if (!seg->backsector) {
    // one-sided seg
    // top sky
    segpart_t *sp = dseg->topsky;
    if (sp) {
      if (FASI(sp->frontTopDist) != FASI(r_ceiling.splane->dist) && R_IsStrictlySkyFlatPlane(r_ceiling.splane)) {
        SetupOneSidedSkyWSurf(sub, seg, sp, r_floor, r_ceiling);
      }
      sp->texinfo.ColorMap = ColorMap;
    }

    // midtexture
    sp = dseg->mid;
    if (sp) {
      //if (seg->pobj) GCon->Logf(NAME_Debug, "pobj #%d seg; UPDATING", seg->pobj->index);
      if (CheckMidRecreate1S(seg, sp, r_floor.splane, r_ceiling.splane)) {
        if (!seg->pobj && CheckFlatsChanged(seg, sp, r_floor.splane, r_ceiling.splane)) needTJ = true;
        //if (seg->pobj) GCon->Logf(NAME_Debug, "pobj #%d seg; RECREATING; needTJ=%d", seg->pobj->index, (int)needTJ);
        sp->ResetFixTJunk();
        SetupOneSidedMidWSurf(sub, seg, sp, r_floor, r_ceiling);
      } else {
        //if (seg->pobj) GCon->Logf(NAME_Debug, "pobj #%d seg; OFFSETING", seg->pobj->index);
        UpdateTextureOffsets(sub, seg, sp, &seg->sidedef->Mid);
      }
      sp->texinfo.ColorMap = ColorMap;
    }
  } else {
    const bool transDoorHack = !!(sub->sector->SectorFlags&sector_t::SF_IsTransDoor);
    //sub->sector->SectorFlags &= ~sector_t::SF_IsTransDoor; //nope, we need this flag to create proper doortracks

    // two-sided seg
    sec_plane_t *back_ceiling = &seg->backsector->ceiling;

    // sky above top
    segpart_t *sp = dseg->topsky;
    if (sp) {
      if (FASI(sp->frontTopDist) != FASI(r_ceiling.splane->dist) &&
          R_IsStrictlySkyFlatPlane(r_ceiling.splane) && !R_IsStrictlySkyFlatPlane(back_ceiling))
      {
        if (!seg->pobj && CheckFlatsChanged(seg, sp, r_floor.splane, r_ceiling.splane)) needTJ = true;
        sp->ResetFixTJunk();
        SetupTwoSidedSkyWSurf(sub, seg, sp, r_floor, r_ceiling);
      }
      sp->texinfo.ColorMap = ColorMap;
    }

    //TODO: properly implement 2s transparent door hack (TNT MAP02)

    // top wall
    sp = dseg->top;
    if (sp) {
      if (CheckTopRecreate2S(seg, sp, r_floor.splane, r_ceiling.splane)) {
        if (!seg->pobj && CheckFlatsChanged(seg, sp, r_floor.splane, r_ceiling.splane)) needTJ = true;
        sp->ResetFixTJunk();
        SetupTwoSidedTopWSurf(sub, seg, sp, r_floor, r_ceiling);
        //if (CheckTopRecreate2S(seg, sp, r_floor.splane, r_ceiling.splane)) GCon->Logf(NAME_Debug, "FUCK! line #%d", (int)(ptrdiff_t)(seg->linedef-&Level->Lines[0]));
      } else {
        UpdateTextureOffsets(sub, seg, sp, &seg->sidedef->Top);
      }
      sp->texinfo.ColorMap = ColorMap;
    }

    // bottom wall
    sp = dseg->bot;
    if (sp) {
      if (CheckBotRecreate2S(seg, sp, r_floor.splane, r_ceiling.splane)) {
        if (!seg->pobj && CheckFlatsChanged(seg, sp, r_floor.splane, r_ceiling.splane)) needTJ = true;
        sp->ResetFixTJunk();
        SetupTwoSidedBotWSurf(sub, seg, sp, r_floor, r_ceiling);
      } else {
        UpdateTextureOffsets(sub, seg, sp, &seg->sidedef->Bot);
      }
      sp->texinfo.ColorMap = ColorMap;
    }

    // masked MidTexture
    sp = dseg->mid;
    if (sp) {
      if (CheckMidRecreate2S(seg, sp, r_floor.splane, r_ceiling.splane)) {
        if (!seg->pobj && CheckFlatsChanged(seg, sp, r_floor.splane, r_ceiling.splane)) needTJ = true;
        sp->ResetFixTJunk();
        SetupTwoSidedMidWSurf(sub, seg, sp, r_floor, r_ceiling);
      } else {
        UpdateTextureOffsets(sub, seg, sp, &seg->sidedef->Mid);
        if (sp->texinfo.Tex->Type != TEXTYPE_Null) {
          sp->texinfo.Alpha = seg->linedef->alpha;
          sp->texinfo.Additive = !!(seg->linedef->flags&ML_ADDITIVE);
        } else {
          sp->texinfo.Alpha = 1.1f;
          sp->texinfo.Additive = false;
        }
      }
      sp->texinfo.ColorMap = ColorMap;
    }

    // update 3d floors
    for (sp = dseg->extra; sp; sp = sp->next) {
      sec_region_t *reg = sp->basereg;
      vassert(reg->extraline);
      const side_t *extraside = &Level->Sides[reg->extraline->sidenum[0]];

      // hack: 3d floor with sky texture seems to be transparent in renderer
      // it should not end here, though, so skip the check
      //if (extraside->MidTexture == skyflatnum) continue;

      VTexture *MTex = GTextureManager(extraside->MidTexture);
      if (!MTex) MTex = GTextureManager[GTextureManager.DefaultTexture];

      if (CheckCommonRecreateEx(sp, MTex, r_floor.splane, r_ceiling.splane, reg->efloor.splane, reg->eceiling.splane)) {
        if (CheckFlatsChanged(seg, sp, r_floor.splane, r_ceiling.splane)) needTJ = true;
        sp->ResetFixTJunk();
        SetupTwoSidedMidExtraWSurf(reg, sub, seg, sp, r_floor, r_ceiling);
      } else {
        UpdateTextureOffsetsEx(sub, seg, sp, &extraside->Mid, &seg->sidedef->Mid);
      }
      sp->texinfo.ColorMap = ColorMap;
    }

    if (!seg->pobj && transDoorHack != !!(sub->sector->SectorFlags&sector_t::SF_IsTransDoor)) InvalidateWholeSeg(seg);
  }

  if (needTJ && lastRenderQuality) MarkTJunctions(seg);
}


//==========================================================================
//
//  VRenderLevelShared::UpdateSubRegions
//
//==========================================================================
void VRenderLevelShared::UpdateSubRegions (subsector_t *sub) {
  if (!sub) return;

  // polyobj cannot be in subsector with 3d floors, so update it once
  //FIXME: this does excessive updates; it is safe, but slow
  if (sub->HasPObjs()) {
    // update the polyobj
    for (auto &&it : sub->PObjFirst()) {
      polyobj_t *pobj = it.pobj();
      if (pobj->pofloor.TexZ <= -90000.0f) continue; // untranslated
      seg_t **polySeg = pobj->segs;
      TSecPlaneRef po_floor, po_ceiling;
      po_floor.set(&pobj->pofloor, false);
      po_ceiling.set(&pobj->poceiling, false);
      for (auto &&sit : pobj->SegFirst()) {
        seg_t *seg = sit.seg();
        if (seg->drawsegs) UpdateDrawSeg(sub, seg->drawsegs, po_floor, po_ceiling);
      }
    }
  }

  for (subregion_t *region = sub->regions; region; region = region->next) {
    TSecPlaneRef r_floor = region->floorplane;
    TSecPlaneRef r_ceiling = region->ceilplane;

    sec_region_t *secregion = region->secregion;

    if ((secregion->regflags&sec_region_t::RF_BaseRegion) && sub->numlines > 0) {
      const seg_t *seg = &Level->Segs[sub->firstline];
      for (int j = sub->numlines; j--; ++seg) {
        if (!seg->linedef || !seg->drawsegs) continue; // miniseg has no drawsegs/segparts
        UpdateDrawSeg(sub, seg->drawsegs, r_floor, r_ceiling);
      }
    }

    if (region->realfloor) {
      // check if we have to remove zerosky flag
      // "zerosky" is set when the sector has zero height, and sky ceiling
      // this is what removes extra floors on Doom II MAP01, for example
      if (region->flags&subregion_t::SRF_ZEROSKY_FLOOR_HACK) {
        if (secregion->eceiling.splane->pic != skyflatnum ||
            secregion->efloor.splane->pic == skyflatnum ||
            sub->sector->floor.normal.z != 1.0f || sub->sector->ceiling.normal.z != -1.0f ||
            sub->sector->floor.minz != sub->sector->ceiling.minz)
        {
          // no more zerofloor
          //GCon->Logf(NAME_Debug, "deactivate ZEROSKY HACK: sub=%d; region=%p", (int)(ptrdiff_t)(sub-&Level->Subsectors[0]), region);
          region->flags &= ~subregion_t::SRF_ZEROSKY_FLOOR_HACK;
        }
      }
      UpdateSecSurface(region->realfloor, region->floorplane, sub, region);
    }
    if (region->fakefloor) {
      TSecPlaneRef fakefloor;
      fakefloor.set(&sub->sector->fakefloors->floorplane, false);
      if (!fakefloor.isFloor()) fakefloor.Flip();
      if (!region->fakefloor->esecplane.isFloor()) region->fakefloor->esecplane.Flip();
      UpdateSecSurface(region->fakefloor, fakefloor, sub, region, false/*allow cmap*/, true/*fake*/);
      //region->fakefloor->texinfo.Tex = GTextureManager[GTextureManager.DefaultTexture];
    }

    if (region->realceil) UpdateSecSurface(region->realceil, region->ceilplane, sub, region);
    if (region->fakeceil) {
      TSecPlaneRef fakeceil;
      fakeceil.set(&sub->sector->fakefloors->ceilplane, false);
      if (!fakeceil.isCeiling()) fakeceil.Flip();
      if (!region->fakeceil->esecplane.isCeiling()) region->fakeceil->esecplane.Flip();
      UpdateSecSurface(region->fakeceil, fakeceil, sub, region, false/*allow cmap*/, true/*fake*/);
      //region->fakeceil->texinfo.Tex = GTextureManager[GTextureManager.DefaultTexture];
    }

    // polyobj cannot be in 3d floor
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
      if (region->realfloor) {
        // check if we have to remove zerosky flag
        // "zerosky" is set when the sector has zero height, and sky ceiling
        // this is what removes extra floors on Doom II MAP01, for example
        if (region->flags&subregion_t::SRF_ZEROSKY_FLOOR_HACK) {
          if (region->secregion->eceiling.splane->pic != skyflatnum ||
              region->secregion->efloor.splane->pic == skyflatnum ||
              sub->sector->floor.normal.z != 1.0f || sub->sector->ceiling.normal.z != -1.0f ||
              sub->sector->floor.minz != sub->sector->ceiling.minz)
          {
            // no more zerofloor
            //GCon->Logf(NAME_Debug, "deactivate ZEROSKY HACK: sub=%d; region=%p", (int)(ptrdiff_t)(sub-&Level->Subsectors[0]), region);
            region->flags &= ~subregion_t::SRF_ZEROSKY_FLOOR_HACK;
          }
        }
        UpdateSecSurface(region->realfloor, region->floorplane, sub, region, true/*no cmap*/); // ignore colormap
      }
      if (region->fakefloor) {
        TSecPlaneRef fakefloor;
        fakefloor.set(&sub->sector->fakefloors->floorplane, false);
        if (!fakefloor.isFloor()) fakefloor.Flip();
        if (!region->fakefloor->esecplane.isFloor()) region->fakefloor->esecplane.Flip();
        UpdateSecSurface(region->fakefloor, fakefloor, sub, region, true/*no cmap*/, true/*fake*/); // ignore colormap
      }
    }
    if (doceils) {
      if (region->realceil) UpdateSecSurface(region->realceil, region->ceilplane, sub, region);
      if (region->fakeceil) {
        TSecPlaneRef fakeceil;
        fakeceil.set(&sub->sector->fakefloors->ceilplane, false);
        if (!fakeceil.isCeiling()) fakeceil.Flip();
        if (!region->fakeceil->esecplane.isCeiling()) region->fakeceil->esecplane.Flip();
        UpdateSecSurface(region->fakeceil, fakeceil, sub, region, false/*allow cmap*/, true/*fake*/);
      }
    }
  }
}
