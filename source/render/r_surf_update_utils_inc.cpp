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
// this is directly included where necessary
//**************************************************************************


//==========================================================================
//
//  CheckFakeDistances
//
//  returns `true` if something was changed
//
//==========================================================================
static inline bool CheckFakeDistances (const seg_t *seg, const segpart_t *sp) {
  if (seg->frontsector->heightsec) {
    if (FASI(sp->frontFakeFloorDist) != FASI(seg->frontsector->heightsec->floor.dist) ||
        FASI(sp->frontFakeCeilDist) != FASI(seg->frontsector->heightsec->ceiling.dist))
    {
      return true;
    }
  }

  if (seg->backsector && seg->backsector->heightsec) {
    return
      FASI(sp->backFakeFloorDist) != FASI(seg->backsector->heightsec->floor.dist) ||
      FASI(sp->backFakeCeilDist) != FASI(seg->backsector->heightsec->ceiling.dist);
  }

  return false;
}


//==========================================================================
//
//  CheckFlatsChangedEx
//
//==========================================================================
static inline bool CheckFlatsChangedEx (const segpart_t *sp, const TPlane *floor, const TPlane *ceiling,
                                        const TPlane *backfloor, const TPlane *backceiling)
{
  return
    (ceiling ? FASI(sp->frontTopDist) != FASI(ceiling->dist) : false) ||
    (floor ? FASI(sp->frontBotDist) != FASI(floor->dist) : false) ||
    (backceiling ? FASI(sp->backTopDist) != FASI(backceiling->dist) : false) ||
    (backfloor ? FASI(sp->backBotDist) != FASI(backfloor->dist) : false);
}


//==========================================================================
//
//  CheckFlatsChanged
//
//==========================================================================
static inline bool CheckFlatsChanged (const seg_t *seg, const segpart_t *sp, const TPlane *floor, const TPlane *ceiling) {
  if (seg->backsector) {
    return CheckFlatsChangedEx(sp, floor, ceiling, &seg->backsector->floor, &seg->backsector->ceiling);
  } else {
    return CheckFlatsChangedEx(sp, floor, ceiling, nullptr, nullptr);
  }
}


//==========================================================================
//
//  CheckCommonRecreateEx
//
//==========================================================================
static inline bool CheckCommonRecreateEx (segpart_t *sp, VTexture *NTex,
                                          const TPlane *floor, const TPlane *ceiling,
                                          const TPlane *backfloor, const TPlane *backceiling)
{
  if (!NTex) NTex = GTextureManager[GTextureManager.DefaultTexture];
  const bool res =
    /*(sp->NeedFixTJunk()) ||*/
    /*(sp->drawseg && sp->drawseg->NeedRecreate()) ||*/
    (CheckFlatsChangedEx(sp, floor, ceiling, backfloor, backceiling)) ||
    FASI(sp->texinfo.Tex->SScale) != FASI(NTex->SScale) ||
    FASI(sp->texinfo.Tex->TScale) != FASI(NTex->TScale) ||
    (sp->texinfo.Tex->Type == TEXTYPE_Null) != (NTex->Type == TEXTYPE_Null) ||
    sp->texinfo.Tex->GetHeight() != NTex->GetHeight() ||
    sp->texinfo.Tex->GetWidth() != NTex->GetWidth();
  // update texture, why not
  // without this, various animations (like switches) won't work
  sp->texinfo.Tex = NTex;
  sp->texinfo.noDecals = NTex->noDecals;
  //sp->texinfo.ColorMap = ColorMap; // cannot do it here, alas
  return res;
}


//==========================================================================
//
//  CheckCommonRecreate
//
//==========================================================================
static inline bool CheckCommonRecreate (const seg_t *seg, segpart_t *sp, VTexture *NTex, const TPlane *floor, const TPlane *ceiling, bool ismid=false) {
  // for middle textures without wrapping, changing vertical offset means "recreate"
  if (ismid && ((seg->linedef->flags&ML_WRAP_MIDTEX)|(seg->sidedef->Flags&SDF_WRAPMIDTEX)) == 0 &&
      FASI(sp->RowOffset) != FASI(seg->sidedef->Mid.RowOffset)) return true;
  if (seg->backsector) {
    // check for fake floors
    return
      CheckFakeDistances(seg, sp) ||
      CheckCommonRecreateEx(sp, NTex, floor, ceiling, &seg->backsector->floor, &seg->backsector->ceiling);
  } else {
    return CheckCommonRecreateEx(sp, NTex, floor, ceiling, nullptr, nullptr);
  }
}


//==========================================================================
//
//  CheckMidRecreate1S
//
//==========================================================================
static inline bool CheckMidRecreate1S (const seg_t *seg, segpart_t *sp, const TPlane *floor, const TPlane *ceiling) {
  return CheckCommonRecreate(seg, sp, GTextureManager(seg->sidedef->MidTexture), floor, ceiling, true);
}


//==========================================================================
//
//  CheckMidRecreate2S
//
//==========================================================================
static inline bool CheckMidRecreate2S (const seg_t *seg, segpart_t *sp, const TPlane *floor, const TPlane *ceiling) {
  return CheckCommonRecreate(seg, sp, GTextureManager(seg->sidedef->MidTexture), floor, ceiling, true);
}


//==========================================================================
//
//  CheckTopRecreate2S
//
//==========================================================================
static inline bool CheckTopRecreate2S (const seg_t *seg, segpart_t *sp, const sec_plane_t *floor, const sec_plane_t *ceiling) {
  const sec_plane_t *back_ceiling = &seg->backsector->ceiling;
  //VTexture *TTex = (seg->frontsector->SectorFlags&sector_t::SF_IsTransDoorTop ? GTextureManager(seg->sidedef->MidTexture) : GTextureManager(seg->sidedef->TopTexture));

  VTexture *TTex = GTextureManager(seg->sidedef->TopTexture);
  if (!seg->pobj &&
      ((seg->frontsub->sector->SectorFlags&sector_t::SF_IsTransDoorTop) ||
       (r_hack_transparent_doors && (!TTex || TTex->Type == TEXTYPE_Null) && VRenderLevelShared::IsTransDoorHackTop(seg))))
  {
    TTex = GTextureManager(seg->sidedef->MidTexture);
  }

  if (ceiling->SkyBox != back_ceiling->SkyBox && R_IsStrictlySkyFlatPlane(ceiling) && R_IsStrictlySkyFlatPlane(back_ceiling)) {
    TTex = GTextureManager[skyflatnum];
  }
  return CheckCommonRecreate(seg, sp, TTex, floor, ceiling);
}


//==========================================================================
//
//  CheckBotRecreate2S
//
//==========================================================================
static inline bool CheckBotRecreate2S (seg_t *seg, segpart_t *sp, const TPlane *floor, const TPlane *ceiling) {
  //VTexture *BTex = (seg->frontsector->SectorFlags&sector_t::SF_IsTransDoorBot ? GTextureManager(seg->sidedef->MidTexture) : GTextureManager(seg->sidedef->BottomTexture));

  VTexture *BTex = GTextureManager(seg->sidedef->BottomTexture);
  if (!seg->pobj &&
      ((seg->frontsub->sector->SectorFlags&sector_t::SF_IsTransDoorBot) ||
       (r_hack_transparent_doors && (!BTex || BTex->Type == TEXTYPE_Null) && VRenderLevelShared::IsTransDoorHackBot(seg))))
  {
    BTex = GTextureManager(seg->sidedef->MidTexture);
  }

  return CheckCommonRecreate(seg, sp, BTex, floor, ceiling);
}
