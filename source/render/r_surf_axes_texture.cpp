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

// it seems that `segsidedef` offset is in effect, but scaling is not
//#define VV_SURFCTOR_3D_USE_SEGSIDEDEF_SCALE


static VCvarF r_dbg_wtrotate_tmult("r_dbg_wtrotate_tmult", "0.0", "Wall texture rotation time multiplier (for debug)", CVAR_PreInit/*|CVAR_Archive*/);


//==========================================================================
//
//  VRenderLevelShared::SetupTextureAxesOffsetDummy
//
//==========================================================================
void VRenderLevelShared::SetupTextureAxesOffsetDummy (texinfo_t *texinfo, VTexture *tex, const bool resetAlpha) {
  texinfo->Tex = tex;
  texinfo->noDecals = tex->noDecals;
  // can be fixed later
  if (resetAlpha) {
    texinfo->Alpha = 1.1f;
    texinfo->Additive = false;
  }
  texinfo->ColorMap = 0;
  texinfo->saxis = texinfo->saxisLM = TVec(1.0f, 0.0f, 0.0f);
  texinfo->taxis = texinfo->taxisLM = TVec(0.0f, 0.0f, -1.0f);
  texinfo->soffs = texinfo->toffs = 0.0f;
}


static double prevtime = -1.0f;


//==========================================================================
//
//  VRenderLevelShared::SetupTextureAxesOffsetNew
//
//  used only for normal wall textures: top, mid, bottom
//
//==========================================================================
void VRenderLevelShared::SetupTextureAxesOffset (seg_t *seg, texinfo_t *texinfo, VTexture *tex, const side_tex_params_t *tparam, float TexZ, const side_tex_params_t *segparam) {
  texinfo->Tex = tex;
  texinfo->noDecals = tex->noDecals;
  // can be fixed later
  // for 3d floor sides this is not required (alpha already set by the caller)
  if (!segparam) {
    texinfo->Alpha = 1.1f;
    texinfo->Additive = false;
  }
  texinfo->ColorMap = 0;

  #ifdef VV_SURFCTOR_3D_USE_SEGSIDEDEF_SCALE
  const float scale2X = (segparam ? segparam->ScaleY : 1.0f);
  const float scale2Y = (segparam ? segparam->ScaleY : 1.0f);
  #else
    // it seems that `segsidedef` offset is in effect, but scaling is not
    #define scale2X  1.0f
    #define scale2Y  1.0f
  #endif

  //const float sofssign = ((tparam->Flags^(segparam ? segparam->Flags : 0u))&STP_FLIP_X ? -1.0f : +1.0f);
  //const float tofssign = ((tparam->Flags^(segparam ? segparam->Flags : 0u))&STP_FLIP_Y ? -1.0f : +1.0f);
  #define sofssign  1.0f
  #define tofssign  1.0f

  float angle = 0.0f; //AngleMod(tparam->BaseAngle-tparam->Angle)
  const float rtt = r_dbg_wtrotate_tmult.asFloat();
  if (rtt != 0.0f) {
    if (prevtime < 0) prevtime = Sys_Time();
    const double ctime = Sys_Time();
    angle = AngleMod((ctime-prevtime)*rtt);
  }

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

  // Doom "up" is positive `z`
  // texture origin is left bottom corner (don't even ask me why)
  // positive `toffs` moves texture origin up

  // texture axes (this also does scaling)
  texinfo->saxis = texinfo->saxisLM*(TextureSScale(tex)*tparam->ScaleX*scale2X);
  texinfo->taxis = texinfo->taxisLM*(TextureTScale(tex)*tparam->ScaleY*scale2Y);

  if ((tparam->Flags^(segparam ? segparam->Flags : 0u))&STP_FLIP_X) texinfo->saxis = -texinfo->saxis;
  if ((tparam->Flags^(segparam ? segparam->Flags : 0u))&STP_FLIP_Y) texinfo->taxis = -texinfo->taxis;

  // basic texture offset, so it will start at `v1`
  //texinfo->soffs = -DotProduct(*seg->linedef->v1, texinfo->saxis); // horizontal
  //texinfo->toffs = -DotProduct(TVec(seg->linedef->v1->x, seg->linedef->v1->y, TexZ), texinfo->taxis); // vertical
  //for long memories... texinfo->soffs += c*seg->offset-seg->offset;

  texinfo->soffs = -DotProduct(*seg->linedef->v1, seg->dir)*TextureSScale(tex)*tparam->ScaleX*scale2X; // horizontal
  texinfo->toffs = TexZ*TextureTScale(tex)*tparam->ScaleY*scale2Y; // vertical

  // apply texture offsets from texture params
  texinfo->soffs += sofssign*(tparam->TextureOffset+(segparam ? segparam->TextureOffset : 0.0f))*DivByScale(TextureOffsetSScale(tex), tparam->ScaleX*scale2X); // horizontal
  texinfo->toffs += tofssign*(tparam->RowOffset+(segparam ? segparam->RowOffset : 0.0f))*DivByScale(TextureOffsetTScale(tex), tparam->ScaleY*scale2Y); // vertical
}
