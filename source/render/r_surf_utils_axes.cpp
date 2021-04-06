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
//  SetupTextureAxesOffsetDummy
//
//==========================================================================
static inline void SetupTextureAxesOffsetDummy (texinfo_t *texinfo, VTexture *tex, const line_t *line=nullptr) {
  texinfo->Tex = tex;
  texinfo->noDecals = tex->noDecals;
  // can be fixed later
  if (line) {
    texinfo->Alpha = line->alpha;
    texinfo->Additive = !!(line->flags&ML_ADDITIVE);
  } else {
    texinfo->Alpha = 1.1f;
    texinfo->Additive = false;
  }
  texinfo->ColorMap = 0;
  texinfo->saxis = texinfo->saxisLM = TVec(1.0f, 0.0f, 0.0f);
  texinfo->taxis = texinfo->taxisLM = TVec(0.0f, 0.0f, -1.0f);
  texinfo->soffs = texinfo->toffs = 0.0f;
}


#if 0
//==========================================================================
//
//  SetupTextureAxesOffset
//
//  used only for normal wall textures: top, mid, bottom
//
//==========================================================================
static inline void SetupTextureAxesOffset (seg_t *seg, texinfo_t *texinfo, VTexture *tex, const side_tex_params_t *tparam) {
  texinfo->Tex = tex;
  texinfo->noDecals = tex->noDecals;
  // can be fixed later
  texinfo->Alpha = 1.1f;
  texinfo->Additive = false;
  texinfo->ColorMap = 0;

  const float sofssign = (tparam->Flags&STP_FLIP_X ? -1.0f : +1.0f);

  const float angle = 0.0f; //AngleMod(tparam->BaseAngle-tparam->Angle);
  /*
  if (angle == 0.0f) {
    texinfo->saxisLM = seg->dir;
    texinfo->taxisLM = TVec(0, 0, -1);
  } else
  */
  {
    float s, c;
    msincos(angle, &s, &c);
    // rotate seg direction
    texinfo->taxisLM = TVec(s*seg->dir.x, s*seg->dir.y, -c);
    texinfo->saxisLM = Normalise(CrossProduct(seg->normal, texinfo->taxisLM));
  }

  texinfo->saxis = texinfo->saxisLM*(VRenderLevelShared::TextureSScale(tex)*tparam->ScaleX);
  texinfo->taxis = texinfo->taxisLM*(VRenderLevelShared::TextureTScale(tex)*tparam->ScaleY);

  texinfo->soffs = -DotProduct(*seg->v1, texinfo->saxis)+
                   seg->offset*(VRenderLevelShared::TextureSScale(tex)*tparam->ScaleX)+
                   (sofssign*tparam->TextureOffset)*VRenderLevelShared::TextureOffsetSScale(tex);
  texinfo->toffs = 0.0f;
  // toffs is not calculated here, as its calculation depends of texture position and pegging

  if (tparam->Flags&STP_FLIP_X) texinfo->saxis = -texinfo->saxis;
  if (tparam->Flags&STP_FLIP_Y) texinfo->taxis = -texinfo->taxis;
}


//==========================================================================
//
//  FixMidTextureOffsetAndOrigin
//
//==========================================================================
static inline void FixMidTextureOffsetAndOrigin (float &zOrg, const line_t *linedef, const side_t *sidedef, texinfo_t *texinfo, VTexture *MTex, const side_tex_params_t *tparam,bool forceWrapped=false) {
  const float tofssign = (tparam->Flags&STP_FLIP_Y ? -1.0f : +1.0f);
  if (forceWrapped || ((linedef->flags&ML_WRAP_MIDTEX)|(sidedef->Flags&SDF_WRAPMIDTEX))) {
    // it is wrapped, so just slide it
    texinfo->toffs = (tofssign*tparam->RowOffset)*VRenderLevelShared::TextureOffsetTScale(MTex);
  } else {
    // move origin up/down, as this texture is not wrapped
    zOrg += (tofssign*tparam->RowOffset)*DivByScale(VRenderLevelShared::TextureOffsetTScale(MTex), tparam->ScaleY);
    // offset is done by origin, so we don't need to offset texture
    texinfo->toffs = 0.0f;
  }
  texinfo->toffs += zOrg*(VRenderLevelShared::TextureTScale(MTex)*tparam->ScaleY);
}
#endif


//==========================================================================
//
//  SetupTextureAxesOffsetNew
//
//  used only for normal wall textures: top, mid, bottom
//
//==========================================================================
static inline void SetupTextureAxesOffsetNew (seg_t *seg, texinfo_t *texinfo, VTexture *tex, const side_tex_params_t *tparam, float &TexZ, bool wrapped) {
  texinfo->Tex = tex;
  texinfo->noDecals = tex->noDecals;
  // can be fixed later
  texinfo->Alpha = 1.1f;
  texinfo->Additive = false;
  texinfo->ColorMap = 0;

  const float sofssign = (tparam->Flags&STP_FLIP_X ? -1.0f : +1.0f);
  const float tofssign = (tparam->Flags&STP_FLIP_Y ? -1.0f : +1.0f);

  const float angle = 0.0f; //AngleMod(tparam->BaseAngle-tparam->Angle);
  if (angle == 0.0f) {
    texinfo->saxisLM = seg->dir;
    texinfo->taxisLM = TVec(0, 0, -1);
  } else {
    float s, c;
    msincos(angle, &s, &c);
    // rotate seg direction
    texinfo->taxisLM = TVec(s*seg->dir.x, s*seg->dir.y, -c);
    texinfo->saxisLM = Normalise(CrossProduct(seg->normal, texinfo->taxisLM));
  }

  texinfo->saxis = texinfo->saxisLM*(VRenderLevelShared::TextureSScale(tex)*tparam->ScaleX);
  texinfo->taxis = texinfo->taxisLM*(VRenderLevelShared::TextureTScale(tex)*tparam->ScaleY);

  texinfo->soffs = -DotProduct(*seg->v1, texinfo->saxis)+
                   seg->offset*(VRenderLevelShared::TextureSScale(tex)*tparam->ScaleX)+
                   (sofssign*tparam->TextureOffset)*VRenderLevelShared::TextureOffsetSScale(tex);

  float zofs;
  if (wrapped) {
    zofs = (tofssign*tparam->RowOffset)*VRenderLevelShared::TextureOffsetTScale(tex);
  } else {
    // move origin up/down, as this texture is not wrapped
    TexZ += (tofssign*tparam->RowOffset)*DivByScale(VRenderLevelShared::TextureOffsetTScale(tex), tparam->ScaleY);
    // offset is done by origin, so we don't need to offset texture
    zofs = 0.0f;
  }

  if (angle == 0.0f) {
    texinfo->toffs = TexZ*
                     VRenderLevelShared::TextureTScale(tex)*tparam->ScaleY+
                     zofs;
  } else {
    texinfo->toffs = -DotProduct(TVec(0, 0, TexZ), texinfo->taxis)*
                     VRenderLevelShared::TextureTScale(tex)*tparam->ScaleY+
                     zofs;
  }

  if (tparam->Flags&STP_FLIP_X) texinfo->saxis = -texinfo->saxis;
  if (tparam->Flags&STP_FLIP_Y) texinfo->taxis = -texinfo->taxis;
}
