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
  seg_t *seg = dseg->seg;

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
      TSecPlaneRef po_floor = region->floorplane;
      TSecPlaneRef po_ceiling = region->ceilplane;
      sector_t *posec = pobj->originalSector;
      if (posec) {
        po_floor.set(&posec->floor, false);
        po_ceiling.set(&posec->ceiling, false);
      } else {
        po_floor = region->floorplane;
        po_ceiling = region->ceilplane;
      }
      for (int polyCount = pobj->numsegs; polyCount--; ++polySeg) {
        UpdateDrawSeg(sub, (*polySeg)->drawsegs, po_floor, po_ceiling);
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
  if (!r_viewleaf) return; // just in case

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
    //(viewhs && Drawer->vieworg.z <= viewhs->floor.GetPointZClamped(Drawer->vieworg));
    (hs && Drawer->vieworg.z <= hs->floor.GetPointZClamped(Drawer->vieworg));
  */
  //bool underwater = (viewhs && Drawer->vieworg.z <= viewhs->floor.GetPointZClamped(Drawer->vieworg));
  bool underwater = (hs && Drawer->vieworg.z <= hs->floor.GetPointZClamped(Drawer->vieworg));
  bool underwaterView = (viewhs && Drawer->vieworg.z <= viewhs->floor.GetPointZClamped(Drawer->vieworg));
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
  float refceilz = (hs ? hs->ceiling.GetPointZClamped(Drawer->vieworg) : 0); // k8: was `nullptr` -- wtf?!
  //float orgflorz = sector->floor.GetPointZClamped(viewx, viewy);
  float orgceilz = sector->ceiling.GetPointZClamped(Drawer->vieworg);

  if (underwater /*||(viewhs && Drawer->vieworg.z <= viewhs->floor.GetPointZClamped(Drawer->vieworg))*/) {
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
  } else if (((hs && Drawer->vieworg.z > hs->ceiling.GetPointZClamped(Drawer->vieworg)) || //k8: dunno, it was `floor` there, and it seems to be a typo
              (viewhs && Drawer->vieworg.z > viewhs->ceiling.GetPointZClamped(Drawer->vieworg))) &&
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
      if (Drawer->vieworg.z < hs->floor.GetPointZClamped(Drawer->vieworg)) {
        // fake floor is actually a ceiling now
        ff->floorplane.flipInPlace();
      }
      if (Drawer->vieworg.z > hs->ceiling.GetPointZClamped(Drawer->vieworg)) {
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
