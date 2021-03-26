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
// this is directly included where necessary
//**************************************************************************

// it seems that `segsidedef` offset is in effect, but scaling is not
//#define VV_SURFCTOR_3D_USE_SEGSIDEDEF_SCALE


//==========================================================================
//
//  SetupTextureAxesOffset
//
//  used only for normal wall textures: top, mid, bottom
//
//==========================================================================
static VVA_OKUNUSED inline void SetupTextureAxesOffset (seg_t *seg, texinfo_t *texinfo, VTexture *tex, const side_tex_params_t *tparam) {
  texinfo->Tex = tex;
  texinfo->noDecals = tex->noDecals;
  // can be fixed later
  texinfo->Alpha = 1.1f;
  texinfo->Additive = false;
  texinfo->ColorMap = 0;

  texinfo->saxisLM = seg->dir;
  texinfo->saxis = seg->dir*(VRenderLevelShared::TextureSScale(tex)*tparam->ScaleX);
  texinfo->taxisLM = TVec(0, 0, -1);
  texinfo->taxis = TVec(0, 0, -1)*(VRenderLevelShared::TextureTScale(tex)*tparam->ScaleY);

  texinfo->soffs = -DotProduct(*seg->v1, texinfo->saxis)+
                   seg->offset*(VRenderLevelShared::TextureSScale(tex)*tparam->ScaleX)+
                   tparam->TextureOffset*VRenderLevelShared::TextureOffsetSScale(tex);
  texinfo->toffs = 0.0f;
  // toffs is not calculated here, as its calculation depends of texture position and pegging
}


//==========================================================================
//
//  SetupTextureAxesOffsetEx
//
//  used for 3d floor extra midtextures
//
//==========================================================================
static VVA_OKUNUSED inline void SetupTextureAxesOffsetEx (seg_t *seg, texinfo_t *texinfo, VTexture *tex, const side_tex_params_t *tparam, const side_tex_params_t *tparam2) {
  texinfo->Tex = tex;
  texinfo->noDecals = tex->noDecals;
  // can be fixed later
  texinfo->Alpha = 1.1f;
  texinfo->Additive = false;
  texinfo->ColorMap = 0;

  #ifdef VV_SURFCTOR_3D_USE_SEGSIDEDEF_SCALE
  const float scale2X = tparam2->ScaleY;
  const float scale2Y = tparam2->ScaleY;
  #else
  // it seems that `segsidedef` offset is in effect, but scaling is not
  const float scale2X = 1.0f;
  const float scale2Y = 1.0f;
  #endif

  texinfo->saxisLM = seg->dir;
  texinfo->saxis = seg->dir*(VRenderLevelShared::TextureSScale(tex)*tparam->ScaleX*scale2X);
  texinfo->taxisLM = TVec(0, 0, -1);
  texinfo->taxis = TVec(0, 0, -1)*(VRenderLevelShared::TextureTScale(tex)*tparam->ScaleY*scale2Y);

  texinfo->soffs = -DotProduct(*seg->v1, texinfo->saxis)+
                   seg->offset*(VRenderLevelShared::TextureSScale(tex)*tparam->ScaleX*scale2X)+
                   (tparam->TextureOffset+tparam2->TextureOffset)*VRenderLevelShared::TextureOffsetSScale(tex);

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
//  actually, 2s "door" wall without top/bottom textures, and with masked
//  midtex is "transparent door"
//
//==========================================================================
static VVA_OKUNUSED bool IsTransDoorHack (const seg_t *seg, bool fortop) {
  const sector_t *secs[2] = { seg->frontsector, seg->backsector };
  if (!secs[0] || !secs[1]) return false;
  const side_t *sidedef = seg->sidedef;
  if (!GTextureManager.IsEmptyTexture(fortop ? sidedef->TopTexture : sidedef->BottomTexture)) return false;
  // if we have don't have a midtex, it is not a door hack
  //if (GTextureManager.IsEmptyTexture(sidedef->MidTexture)) return false;
  VTexture *tex = GTextureManager[sidedef->MidTexture];
  if (!tex || tex->Type == TEXTYPE_Null) return false;
  // should be "see through", or line should have alpha
  if (seg->linedef->alpha >= 1.0f && !tex->isSeeThrough()) return false;
  // check for slopes
  if (secs[0]->floor.normal.z != 1.0f || secs[0]->ceiling.normal.z != -1.0f) return false;
  if (secs[1]->floor.normal.z != 1.0f || secs[1]->ceiling.normal.z != -1.0f) return false;
  // ok, looks like it
  return true;
}


//==========================================================================
//
//  IsTransDoorHackTop
//
//==========================================================================
static inline bool IsTransDoorHackTop (const seg_t *seg) {
  return IsTransDoorHack(seg, true);
}


//==========================================================================
//
//  IsTransDoorHackBot
//
//==========================================================================
static inline VVA_OKUNUSED bool IsTransDoorHackBot (const seg_t *seg) {
  return IsTransDoorHack(seg, false);
}


//==========================================================================
//
//  DivByScale
//
//==========================================================================
static inline VVA_OKUNUSED float DivByScale (float v, float scale) {
  return (scale > 0 ? v/scale : v);
}


//==========================================================================
//
//  DivByScale2
//
//==========================================================================
static inline VVA_OKUNUSED float DivByScale2 (float v, float scale, float scale2) {
  if (scale2 <= 0.0f) return DivByScale(v, scale);
  if (scale <= 0.0f) return DivByScale(v, scale2);
  return v/(scale*scale2);
}


//==========================================================================
//
//  GetMinFloorZWithFake
//
//  get max floor height for given floor and fake floor
//
//==========================================================================
// i haet shitpp templates!
#define GetFixedZWithFake(func_,plname_,v_,sec_,r_plane_)  \
  ((sec_) && (sec_)->heightsec ? func_((r_plane_).GetPointZ(v_), (sec_)->heightsec->plname_.GetPointZ(v_)) : (r_plane_).GetPointZ(v_))


//==========================================================================
//
//  SetupFakeDistances
//
//==========================================================================
static inline VVA_OKUNUSED void SetupFakeDistances (const seg_t *seg, segpart_t *sp) {
  if (seg->frontsector->heightsec) {
    sp->frontFakeFloorDist = seg->frontsector->heightsec->floor.dist;
    sp->frontFakeCeilDist = seg->frontsector->heightsec->ceiling.dist;
  } else {
    sp->frontFakeFloorDist = 0.0f;
    sp->frontFakeCeilDist = 0.0f;
  }

  if (seg->backsector && seg->backsector->heightsec) {
    sp->backFakeFloorDist = seg->backsector->heightsec->floor.dist;
    sp->backFakeCeilDist = seg->backsector->heightsec->ceiling.dist;
  } else {
    sp->backFakeFloorDist = 0.0f;
    sp->backFakeCeilDist = 0.0f;
  }
}


//==========================================================================
//
//  FixMidTextureOffsetAndOrigin
//
//==========================================================================
static inline VVA_OKUNUSED void FixMidTextureOffsetAndOrigin (float &z_org, const line_t *linedef, const side_t *sidedef, texinfo_t *texinfo, VTexture *MTex, const side_tex_params_t *tparam, bool forceWrapped=false) {
  if (forceWrapped || ((linedef->flags&ML_WRAP_MIDTEX)|(sidedef->Flags&SDF_WRAPMIDTEX))) {
    // it is wrapped, so just slide it
    texinfo->toffs = tparam->RowOffset*VRenderLevelShared::TextureOffsetTScale(MTex);
  } else {
    // move origin up/down, as this texture is not wrapped
    z_org += tparam->RowOffset*DivByScale(VRenderLevelShared::TextureOffsetTScale(MTex), tparam->ScaleY);
    // offset is done by origin, so we don't need to offset texture
    texinfo->toffs = 0.0f;
  }
  texinfo->toffs += z_org*(VRenderLevelShared::TextureTScale(MTex)*tparam->ScaleY);
}


//==========================================================================
//
//  FixMidTextureOffsetAndOriginEx
//
//==========================================================================
static inline VVA_OKUNUSED void FixMidTextureOffsetAndOriginEx (float &z_org, const line_t *linedef, const side_t *sidedef, texinfo_t *texinfo, VTexture *MTex, const side_tex_params_t *tparam, const side_tex_params_t *tparam2) {
  #ifdef VV_SURFCTOR_3D_USE_SEGSIDEDEF_SCALE
  const float scale2Y = tparam2->ScaleY;
  #else
  // it seems that `segsidedef` offset is in effect, but scaling is not
  const float scale2Y = 1.0f;
  #endif
  // it is always wrapped, so just slide it
  texinfo->toffs = (tparam->RowOffset+tparam2->RowOffset)*VRenderLevelShared::TextureOffsetTScale(MTex);
  texinfo->toffs += z_org*(VRenderLevelShared::TextureTScale(MTex)*tparam->ScaleY*scale2Y);
}
