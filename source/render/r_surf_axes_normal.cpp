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


//==========================================================================
//
//  VRenderLevelShared::SetupTextureAxesOffsetDummy
//
//==========================================================================
void VRenderLevelShared::SetupTextureAxesOffsetDummy (texinfo_t *texinfo, VTexture *tex, const line_t *line) {
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
//  VRenderLevelShared::SetupTextureAxesOffset
//
//  used only for normal wall textures: top, mid, bottom
//
//==========================================================================
void VRenderLevelShared::SetupTextureAxesOffset (seg_t *seg, texinfo_t *texinfo, VTexture *tex, const side_tex_params_t *tparam) {
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
//  VRenderLevelShared::FixMidTextureOffsetAndOrigin
//
//==========================================================================
void VRenderLevelShared::FixMidTextureOffsetAndOrigin (float &zOrg, const line_t *linedef, const side_t *sidedef, texinfo_t *texinfo, VTexture *MTex, const side_tex_params_t *tparam,bool forceWrapped=false) {
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
//  VRenderLevelShared::SetupTextureAxesOffsetNew
//
//  used only for normal wall textures: top, mid, bottom
//
//==========================================================================
void VRenderLevelShared::SetupTextureAxesOffsetNew (seg_t *seg, texinfo_t *texinfo, VTexture *tex, const side_tex_params_t *tparam, float &TexZ, bool wrapped) {
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
    texinfo->taxisLM = TVec(0.0f, 0.0f, -1.0f);
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
    texinfo->toffs = -DotProduct(TVec(0.0f, 0.0f, TexZ), texinfo->taxis)*
                     VRenderLevelShared::TextureTScale(tex)*tparam->ScaleY+
                     zofs;

    //texinfo->toffs -= DotProduct(texinfo->taxis, TVec(seg->offset, seg->offset, 0.0f));

    texinfo->soffs += 12.0f;
    texinfo->toffs += 12.0f;
  }

  if (tparam->Flags&STP_FLIP_X) texinfo->saxis = -texinfo->saxis;
  if (tparam->Flags&STP_FLIP_Y) texinfo->taxis = -texinfo->taxis;
}
