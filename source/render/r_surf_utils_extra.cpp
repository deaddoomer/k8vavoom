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
//  DivByScale
//
//==========================================================================
static inline float DivByScale (float v, float scale) {
  return (scale > 0 ? v/scale : v);
}


//==========================================================================
//
//  DivByScale2
//
//==========================================================================
static inline float DivByScale2 (float v, float scale, float scale2) {
  if (scale2 <= 0.0f) return DivByScale(v, scale);
  if (scale <= 0.0f) return DivByScale(v, scale2);
  return v/(scale*scale2);
}


//==========================================================================
//
//  SetupTextureAxesOffsetDummyEx
//
//==========================================================================
static inline void SetupTextureAxesOffsetDummyEx (texinfo_t *texinfo, VTexture *tex) {
  texinfo->Tex = tex;
  texinfo->noDecals = tex->noDecals;
  texinfo->ColorMap = 0;
  texinfo->saxis = texinfo->saxisLM = TVec(1.0f, 0.0f, 0.0f);
  texinfo->taxis = texinfo->taxisLM = TVec(0.0f, 0.0f, -1.0f);
  texinfo->soffs = texinfo->toffs = 0.0f;
}


#if 0
//==========================================================================
//
//  SetupTextureAxesOffsetEx
//
//  used for 3d floor extra midtextures
//
//==========================================================================
static inline void SetupTextureAxesOffsetEx (seg_t *seg, texinfo_t *texinfo, VTexture *tex, const side_tex_params_t *tparam, const side_tex_params_t *segparam) {
  texinfo->Tex = tex;
  texinfo->noDecals = tex->noDecals;
  // can be fixed later
  texinfo->Alpha = 1.1f;
  texinfo->Additive = false;
  texinfo->ColorMap = 0;

  #ifdef VV_SURFCTOR_3D_USE_SEGSIDEDEF_SCALE
  const float scale2X = segparam->ScaleY;
  const float scale2Y = segparam->ScaleY;
  #else
  // it seems that `segsidedef` offset is in effect, but scaling is not
  const float scale2X = 1.0f;
  const float scale2Y = 1.0f;
  #endif

  const float sofssign = (tparam->Flags&STP_FLIP_X ? -1.0f : +1.0f);
  const float sofssign2 = (segparam->Flags&STP_FLIP_X ? -1.0f : +1.0f);

  texinfo->saxisLM = seg->dir;
  texinfo->saxis = seg->dir*(VRenderLevelShared::TextureSScale(tex)*tparam->ScaleX*scale2X);
  texinfo->taxisLM = TVec(0, 0, -1);
  texinfo->taxis = TVec(0, 0, -1)*(VRenderLevelShared::TextureTScale(tex)*tparam->ScaleY*scale2Y);

  texinfo->soffs = -DotProduct(*seg->v1, texinfo->saxis)+
                   seg->offset*(VRenderLevelShared::TextureSScale(tex)*tparam->ScaleX*scale2X)+
                   (sofssign*tparam->TextureOffset+sofssign2*segparam->TextureOffset)*VRenderLevelShared::TextureOffsetSScale(tex);

  texinfo->toffs = 0.0f;
  // toffs is not calculated here, as its calculation depends of texture position and pegging

  if (tparam->Flags&STP_FLIP_X) texinfo->saxis = -texinfo->saxis;
  if (tparam->Flags&STP_FLIP_Y) texinfo->taxis = -texinfo->taxis;
}


//==========================================================================
//
//  FixMidTextureOffsetAndOriginEx
//
//==========================================================================
static inline void FixMidTextureOffsetAndOriginEx (float &zOrg, const line_t *linedef, const side_t *sidedef, texinfo_t *texinfo, VTexture *MTex, const side_tex_params_t *tparam, const side_tex_params_t *segparam) {
  const float tofssign = (tparam->Flags&STP_FLIP_Y ? -1.0f : +1.0f);
  const float tofssign2 = (segparam->Flags&STP_FLIP_Y ? -1.0f : +1.0f);
  #ifdef VV_SURFCTOR_3D_USE_SEGSIDEDEF_SCALE
  const float scale2Y = segparam->ScaleY;
  #else
  // it seems that `segsidedef` offset is in effect, but scaling is not
  const float scale2Y = 1.0f;
  #endif
  // it is always wrapped, so just slide it
  texinfo->toffs = (tofssign*tparam->RowOffset+tofssign2*segparam->RowOffset)*VRenderLevelShared::TextureOffsetTScale(MTex);
  texinfo->toffs += zOrg*(VRenderLevelShared::TextureTScale(MTex)*tparam->ScaleY*scale2Y);
}
#endif


//==========================================================================
//
//  SetupTextureAxesOffsetEx
//
//  used for 3d floor extra midtextures
//
//  `tparam` is from control line
//  `segparam` is from seg
//
//==========================================================================
static inline void SetupTextureAxesOffsetExNew (seg_t *seg, texinfo_t *texinfo, VTexture *tex, const side_tex_params_t *tparam, const side_tex_params_t *segparam, const float TexZ) {
  texinfo->Tex = tex;
  texinfo->noDecals = tex->noDecals;
  // can be fixed later
  texinfo->Alpha = 1.1f;
  texinfo->Additive = false;
  texinfo->ColorMap = 0;

  #ifdef VV_SURFCTOR_3D_USE_SEGSIDEDEF_SCALE
  const float scale2X = segparam->ScaleY;
  const float scale2Y = segparam->ScaleY;
  #else
  // it seems that `segsidedef` offset is in effect, but scaling is not
    #define scale2X  1.0f
    #define scale2Y  1.0f
  #endif

  const float sofssign = ((tparam->Flags^segparam->Flags)&STP_FLIP_X ? -1.0f : +1.0f);
  const float tofssign = ((tparam->Flags^segparam->Flags)&STP_FLIP_Y ? -1.0f : +1.0f);

  /*
  texinfo->saxisLM = seg->dir;
  texinfo->taxisLM = TVec(0, 0, -1);

  texinfo->saxis = texinfo->saxisLM*(VRenderLevelShared::TextureSScale(tex)*tparam->ScaleX*scale2X);
  texinfo->taxis = texinfo->taxisLM*(VRenderLevelShared::TextureTScale(tex)*tparam->ScaleY*scale2Y);

  texinfo->soffs = -DotProduct(*seg->v1, texinfo->saxis)+
                   seg->offset*(VRenderLevelShared::TextureSScale(tex)*tparam->ScaleX*scale2X)+
                   (sofssign*tparam->TextureOffset+sofssign2*segparam->TextureOffset)*VRenderLevelShared::TextureOffsetSScale(tex);


  // it is always wrapped, so just slide it
  texinfo->toffs = (tofssign*tparam->RowOffset+tofssign2*segparam->RowOffset)*VRenderLevelShared::TextureOffsetTScale(MTex);
  texinfo->toffs += zOrg*(VRenderLevelShared::TextureTScale(MTex)*tparam->ScaleY*scale2Y);
  */

  const float angle = 0.0f; //AngleMod(tparam->BaseAngle-tparam->Angle);
  if (angle == 0.0f) {
    texinfo->saxisLM = seg->dir;
    texinfo->taxisLM = TVec(0.0f, 0.0f, -1.0f);
  } else {
    float s, c;
    msincos(angle, &s, &c);
    // rotate seg direction
    texinfo->taxisLM = TVec(s*seg->dir.x, s*seg->dir.y, -c);
    texinfo->saxisLM = Normalise(CrossProduct(seg->normal, texinfo->taxisLM));
  }

  texinfo->saxis = texinfo->saxisLM*(VRenderLevelShared::TextureSScale(tex)*tparam->ScaleX*scale2X);
  texinfo->taxis = texinfo->taxisLM*(VRenderLevelShared::TextureTScale(tex)*tparam->ScaleY*scale2Y);

  texinfo->soffs = -DotProduct(*seg->v1, texinfo->saxis)+
                   seg->offset*(VRenderLevelShared::TextureSScale(tex)*tparam->ScaleX*scale2X)+
                   (sofssign*(tparam->TextureOffset+segparam->TextureOffset))*VRenderLevelShared::TextureOffsetSScale(tex);

  // it is always wrapped, so just slide it
  const float zofs = (tofssign*tparam->RowOffset)*VRenderLevelShared::TextureOffsetTScale(tex);

  if (angle == 0.0f) {
    texinfo->toffs = TexZ*(VRenderLevelShared::TextureTScale(tex)*tparam->ScaleY*scale2Y)+zofs;
  } else {
    texinfo->toffs = -DotProduct(TVec(0, 0, TexZ), texinfo->taxis)*
                     (VRenderLevelShared::TextureTScale(tex)*tparam->ScaleY*scale2Y)+
                     zofs;
  }

  if (tparam->Flags&STP_FLIP_X) texinfo->saxis = -texinfo->saxis;
  if (tparam->Flags&STP_FLIP_Y) texinfo->taxis = -texinfo->taxis;

  #ifdef scale2X
    #undef scale2X
  #endif
  #ifdef scale2Y
    #undef scale2Y
  #endif
}
