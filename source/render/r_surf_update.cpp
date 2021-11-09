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
#include "r_surf_update_utils_inc.cpp"


VCvarB r_dbg_force_world_update("r_dbg_force_world_update", false, "Force world updates on each frame (for debugging).", CVAR_PreInit|CVAR_NoShadow/*|CVAR_Archive*/);



//==========================================================================
//
//  VRenderLevelShared::UpdateTextureOffsets
//
//==========================================================================
void VRenderLevelShared::UpdateTextureOffsets (subsector_t *sub, seg_t *seg, segpart_t *sp, const side_tex_params_t *tparam, const TPlane *plane) {
  bool reinitSurfs = false;

  if (FASI(sp->TextureOffset) != FASI(tparam->TextureOffset)) {
    reinitSurfs = true;
    sp->texinfo.soffs += (tparam->TextureOffset-sp->TextureOffset)*TextureOffsetSScale(sp->texinfo.Tex)/tparam->ScaleX;
    sp->TextureOffset = tparam->TextureOffset;
  }

  if (FASI(sp->RowOffset) != FASI(tparam->RowOffset)) {
    reinitSurfs = true;
    sp->texinfo.toffs += (tparam->RowOffset-sp->RowOffset)*TextureOffsetTScale(sp->texinfo.Tex)/tparam->ScaleY;
    sp->RowOffset = tparam->RowOffset;
  }

  if (reinitSurfs) {
    // do not recalculate static lightmaps
    //if (seg->pobj) GCon->Logf(NAME_Debug, "pobj #%d seg; OFFSETING", seg->pobj->index);
    InitSurfs(false, sp->surfs, &sp->texinfo, (plane ? plane : seg), sub, seg, nullptr);
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
    sp->texinfo.soffs += (ctofs-sp->TextureOffset)*TextureOffsetSScale(sp->texinfo.Tex)/tparam->ScaleX;
    sp->TextureOffset = ctofs;
  }

  const float rwofs = tparam->RowOffset+tparam2->RowOffset;
  if (FASI(sp->RowOffset) != FASI(rwofs)) {
    reinitSurfs = true;
    sp->texinfo.toffs += (rwofs-sp->RowOffset)*TextureOffsetTScale(sp->texinfo.Tex)/tparam->ScaleY;
    sp->RowOffset = rwofs;
  }

  if (reinitSurfs) {
    // do not recalculate static lightmaps
    InitSurfs(false, sp->surfs, &sp->texinfo, seg, sub, seg, nullptr);
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

  line_t *ld = seg->linedef;
  if (!ld) return; // miniseg
  ld->exFlags |= ML_EX_NON_TRANSLUCENT;

  const bool forceUpdate = r_dbg_force_world_update.asBool();

  bool needTJ = false;

  // note that we need to check for "any flat height changed" in recreation code path
  // this is to avoid constantly recreating the whole map when we only need to fix t-junctions

  if (!seg->backsector) {
    // one-sided seg
    // top sky
    segpart_t *sp = dseg->topsky;
    if (sp) {
      if (forceUpdate || (FASI(sp->frontTopDist) != FASI(r_ceiling.splane->dist) && R_IsStrictlySkyFlatPlane(r_ceiling.splane))) {
        SetupOneSidedSkyWSurf(sub, seg, sp, r_floor, r_ceiling);
      }
      sp->texinfo.ColorMap = ColorMap;
    }

    // midtexture
    sp = dseg->mid;
    if (sp) {
      //if (seg->pobj) GCon->Logf(NAME_Debug, "pobj #%d seg; UPDATING", seg->pobj->index);
      if (forceUpdate || CheckMidRecreate1S(seg, sp, r_floor.splane, r_ceiling.splane)) {
        if (!seg->pobj && CheckFlatsChanged(seg, sp, r_floor.splane, r_ceiling.splane)) needTJ = true;
        //if (seg->pobj) GCon->Logf(NAME_Debug, "pobj #%d seg; RECREATING; needTJ=%d", seg->pobj->index, (int)needTJ);
        sp->ResetFixTJunk();
        SetupOneSidedMidWSurf(sub, seg, sp, r_floor, r_ceiling);
      } else if (sp->surfs) {
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
      if (forceUpdate ||
          (FASI(sp->frontTopDist) != FASI(r_ceiling.splane->dist) &&
           R_IsStrictlySkyFlatPlane(r_ceiling.splane) && !R_IsStrictlySkyFlatPlane(back_ceiling)))
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
      if (forceUpdate || CheckTopRecreate2S(seg, sp, r_floor.splane, r_ceiling.splane)) {
        if (!seg->pobj && CheckFlatsChanged(seg, sp, r_floor.splane, r_ceiling.splane)) needTJ = true;
        sp->ResetFixTJunk();
        SetupTwoSidedTopWSurf(sub, seg, sp, r_floor, r_ceiling);
        //if (CheckTopRecreate2S(seg, sp, r_floor.splane, r_ceiling.splane)) GCon->Logf(NAME_Debug, "FUCK! line #%d", (int)(ptrdiff_t)(ld-&Level->Lines[0]));
      } else if (sp->surfs) {
        UpdateTextureOffsets(sub, seg, sp, &seg->sidedef->Top);
      }
      sp->texinfo.ColorMap = ColorMap;
    }

    // bottom wall
    sp = dseg->bot;
    if (sp) {
      if (forceUpdate || CheckBotRecreate2S(seg, sp, r_floor.splane, r_ceiling.splane)) {
        if (!seg->pobj && CheckFlatsChanged(seg, sp, r_floor.splane, r_ceiling.splane)) needTJ = true;
        sp->ResetFixTJunk();
        SetupTwoSidedBotWSurf(sub, seg, sp, r_floor, r_ceiling);
      } else if (sp->surfs) {
        UpdateTextureOffsets(sub, seg, sp, &seg->sidedef->Bot);
      }
      sp->texinfo.ColorMap = ColorMap;
    }

    // masked MidTexture
    sp = dseg->mid;
    if (sp) {
      if (forceUpdate || CheckMidRecreate2S(seg, sp, r_floor.splane, r_ceiling.splane)) {
        if (!seg->pobj && CheckFlatsChanged(seg, sp, r_floor.splane, r_ceiling.splane)) needTJ = true;
        sp->ResetFixTJunk();
        SetupTwoSidedMidWSurf(sub, seg, sp, r_floor, r_ceiling);
        if (sp->surfs && sp->texinfo.Tex->Type != TEXTYPE_Null) {
          if (ld->alpha < 1.0f || sp->texinfo.Tex->isTranslucent()) ld->exFlags &= ~ML_EX_NON_TRANSLUCENT;
        }
      } else if (sp->surfs) {
        UpdateTextureOffsets(sub, seg, sp, &seg->sidedef->Mid);
        if (sp->texinfo.Tex->Type != TEXTYPE_Null) {
          sp->texinfo.Alpha = ld->alpha;
          sp->texinfo.Additive = !!(ld->flags&ML_ADDITIVE);
          if (sp->texinfo.Alpha < 1.0f || sp->texinfo.Additive || sp->texinfo.Tex->isTranslucent()) ld->exFlags &= ~ML_EX_NON_TRANSLUCENT;
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

      if (forceUpdate || CheckCommonRecreateEx(sp, MTex, r_floor.splane, r_ceiling.splane, reg->efloor.splane, reg->eceiling.splane)) {
        if (CheckFlatsChanged(seg, sp, r_floor.splane, r_ceiling.splane)) needTJ = true;
        sp->ResetFixTJunk();
        SetupTwoSidedMidExtraWSurf(reg, sub, seg, sp, r_floor, r_ceiling);
        if (sp->surfs && sp->texinfo.Tex->Type != TEXTYPE_Null) {
          if (sp->texinfo.Alpha < 1.0f || sp->texinfo.Additive || sp->texinfo.Tex->isTranslucent()) ld->exFlags &= ~ML_EX_NON_TRANSLUCENT;
        }
      } else if (sp->surfs) {
        const bool translucentTex = MTex->isTranslucent();
        const bool oldTranslucent = (translucentTex || sp->texinfo.Additive || sp->texinfo.Alpha < 1.0f);
        UpdateTextureOffsetsEx(sub, seg, sp, &extraside->Mid, &seg->sidedef->Mid);
        if (sp->texinfo.Tex->Type != TEXTYPE_Null) {
          // not from a line!
          sp->texinfo.Alpha = (reg->efloor.splane->Alpha < 1.0f ? reg->efloor.splane->Alpha : 1.1f);
          sp->texinfo.Additive = !!(reg->efloor.splane->flags&SPF_ADDITIVE);
          const bool newTranslucent = (translucentTex || sp->texinfo.Additive || sp->texinfo.Alpha < 1.0f);
          if (newTranslucent) ld->exFlags &= ~ML_EX_NON_TRANSLUCENT;
          if (oldTranslucent != newTranslucent) {
            if (newTranslucent) {
              for (surface_t *sf = sp->surfs; sf; sf = sf->next) sf->drawflags |= surface_t::DF_NO_FACE_CULL;
            } else {
              for (surface_t *sf = sp->surfs; sf; sf = sf->next) sf->drawflags &= ~surface_t::DF_NO_FACE_CULL;
            }
          }
        } else {
          sp->texinfo.Alpha = 1.1f;
          sp->texinfo.Additive = false;
        }
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
//  there is no need to update polyobjects here,
//  bsp renderer will do it explicitly
//
//==========================================================================
void VRenderLevelShared::UpdateSubRegions (subsector_t *sub) {
  if (!sub) return;

  //if (sub->isInnerPObj()) GCon->Logf(NAME_Debug, "updating subsector for pobj #%d", sub->ownpobj->tag);

  for (subregion_t *region = sub->regions; region; region = region->next) {
    TSecPlaneRef r_floor = region->floorplane;
    TSecPlaneRef r_ceiling = region->ceilplane;

    sec_region_t *secregion = region->secregion;

    if ((secregion->regflags&sec_region_t::RF_BaseRegion) && sub->numlines > 0 && !sub->isInnerPObj()) {
      const seg_t *seg = &Level->Segs[sub->firstline];
      for (int j = sub->numlines; j--; ++seg) {
        line_t *ld = seg->linedef;
        if (ld) {
          // miniseg has no drawsegs/segparts
          if (seg->drawsegs) UpdateDrawSeg(sub, seg->drawsegs, r_floor, r_ceiling);
        }
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

    region->ResetForceRecreation();

    // polyobj cannot be in 3d floor
  }
}


//==========================================================================
//
//  VRenderLevelShared::UpdatePObj
//
//==========================================================================
void VRenderLevelShared::UpdatePObj (polyobj_t *po) {
  if (!po) return;

  // check refsector for normal pobj
  if (!po->Is3D() && po->refsector) {
    const float rfh = po->refsector->floor.GetRealDist();
    const float dz = rfh-po->refsectorOldFloorHeight;
    if (dz != 0.0f) {
      // fix height
      po->pofloor.dist -= dz; // floor dist is negative
      po->pofloor.TexZ += dz;
      po->pofloor.minz += dz;
      po->pofloor.maxz += dz;
      po->pofloor.TexZ = po->pofloor.minz;
      po->poceiling.dist += dz; // ceiling dist is positive
      po->poceiling.TexZ += dz;
      po->poceiling.minz += dz;
      po->poceiling.maxz += dz;
      po->poceiling.TexZ = po->poceiling.minz;
      po->refsectorOldFloorHeight = rfh;
    }
  }

  TSecPlaneRef po_floor, po_ceiling;
  po_floor.set(&po->pofloor, false);
  po_ceiling.set(&po->poceiling, false);

  // update polyobject segs
  for (auto &&sit : po->SegFirst()) {
    const seg_t *seg = sit.seg();
    line_t *ld = seg->linedef;
    if (ld) {
      subsector_t *sub = seg->frontsub;
      //FIXME: this should be done in pobj loader!
      if (po->Is3D() && !sub->isInnerPObj()) {
        seg_t *partner = seg->partner;
        if (partner && partner->frontsub && partner->frontsub->isInnerPObj()) sub = partner->frontsub;
      }
      UpdateDrawSeg(sub, seg->drawsegs, po_floor, po_ceiling);
    }
  }

  if (!po->Is3D()) return;

  // update polyobject flats
  for (subsector_t *sub = po->GetSector()->subsectors; sub; sub = sub->seclink) {
    vassert(sub->isInnerPObj());

    // polyobject have only one region (this is invariant)
    subregion_t *region = sub->regions;
    vassert(!region->next);

    //TSecPlaneRef r_floor = region->floorplane;
    //TSecPlaneRef r_ceiling = region->ceilplane;

    // update flags
    if (region->realfloor) {
      // check if we have to remove zerosky flag
      // "zerosky" is set when the sector has zero height, and sky ceiling
      // this is what removes extra floors on Doom II MAP01, for example
      region->flags &= ~subregion_t::SRF_ZEROSKY_FLOOR_HACK;
      UpdateSecSurface(region->realfloor, region->floorplane, sub, region);
    }

    if (region->fakefloor) {
      TSecPlaneRef fakefloor;
      fakefloor.set(&sub->sector->fakefloors->floorplane, false);
      if (!fakefloor.isFloor()) fakefloor.Flip();
      if (!region->fakefloor->esecplane.isFloor()) region->fakefloor->esecplane.Flip();
      UpdateSecSurface(region->fakefloor, fakefloor, sub, region, false/*allow cmap*/, true/*fake*/);
    }

    if (region->realceil) UpdateSecSurface(region->realceil, region->ceilplane, sub, region);

    if (region->fakeceil) {
      TSecPlaneRef fakeceil;
      fakeceil.set(&sub->sector->fakefloors->ceilplane, false);
      if (!fakeceil.isCeiling()) fakeceil.Flip();
      if (!region->fakeceil->esecplane.isCeiling()) region->fakeceil->esecplane.Flip();
      UpdateSecSurface(region->fakeceil, fakeceil, sub, region, false/*allow cmap*/, true/*fake*/);
    }

    region->ResetForceRecreation();
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

    region->ResetForceRecreation();
  }
}
