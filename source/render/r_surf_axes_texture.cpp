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
void VRenderLevelShared::SetupTextureAxesOffset (seg_t *seg, texinfo_t *texinfo, VTexture *tex, const side_tex_params_t *tparam, float &TexZ, const side_tex_params_t *segparam) {
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

  const bool xflip = ((tparam->Flags^(segparam ? segparam->Flags : 0u))&STP_FLIP_X);
  const bool yflip = ((tparam->Flags^(segparam ? segparam->Flags : 0u))&STP_FLIP_Y);

  // still need to adjust offset signs, so offsets will work the same way regardless of flipping
  //const float sofssign = (xflip ? -1.0f : +1.0f);
  //const float tofssign = (yflip ? -1.0f : +1.0f);

  #if 0
  float angle = 0.0f; //AngleMod(tparam->BaseAngle-tparam->Angle)
  #else
  float angle = (segparam ? segparam->BaseAngle-segparam->Angle : tparam->BaseAngle-tparam->Angle);
  #endif
  const float rtt = r_dbg_wtrotate_tmult.asFloat();
  if (rtt != 0.0f) {
    if (prevtime < 0) prevtime = Sys_Time();
    const double ctime = Sys_Time();
    angle = AngleMod((ctime-prevtime)*rtt);
  }

  // there is no reason to rotate lightmap texture
  texinfo->saxisLM = seg->dir;
  texinfo->taxisLM = TVec(0.0f, 0.0f, -1.0f);

  // calculate texture axes (this also does scaling)
  // Doom "up" is positive `z`
  // texture origin is left bottom corner (don't even ask me why)
  // positive `toffs` moves texture origin up

  if (angle == 0.0f) {
    // most common case
    texinfo->saxis = seg->dir;
    texinfo->taxis = TVec(0.0f, 0.0f, -1.0f);
  } else {
    float s, c;
    msincos(angle, &s, &c);
    // rotate seg direction
    texinfo->taxis = TVec(s*seg->dir.x, s*seg->dir.y, -c);
    texinfo->saxis = Normalise(CrossProduct(seg->normal, texinfo->taxis));
  }
  texinfo->saxis *= TextureSScale(tex)*tparam->ScaleX*scale2X;
  texinfo->taxis *= TextureTScale(tex)*tparam->ScaleY*scale2Y;

  if (xflip) texinfo->saxis = -texinfo->saxis;
  if (yflip) texinfo->taxis = -texinfo->taxis;

  // basic texture offset, so it will start at `v1`
  //texinfo->soffs = -DotProduct(*seg->linedef->v1, texinfo->saxis); // horizontal
  //texinfo->toffs = -DotProduct(TVec(seg->linedef->v1->x, seg->linedef->v1->y, TexZ), texinfo->taxis); // vertical
  //for long memories... texinfo->soffs += c*seg->offset-seg->offset;

  // x offset need not to be modified (i hope)
  float xofs = tparam->TextureOffset+(segparam ? segparam->TextureOffset : 0.0f);
  if (xflip) xofs = -xofs;

  // y offset should be modified, because non-wrapping textures physically moves
  float yofs = tparam->RowOffset+(segparam ? segparam->RowOffset : 0.0f);
  if (yflip) yofs = -yofs;

  // non-wrapping?
  if (((seg->linedef->flags&ML_WRAP_MIDTEX)|(seg->sidedef->Flags&SDF_WRAPMIDTEX)) == 0) {
    // yeah, move TexZ
    TexZ += yofs*(TextureOffsetTScale(tex)/tparam->ScaleY);
    yofs = 0.0f;
  }

  if (!xflip) {
    texinfo->soffs = -DotProduct(*seg->linedef->v1, seg->dir)*TextureSScale(tex)*tparam->ScaleX*scale2X; // horizontal
  } else {
    // flipped
    texinfo->soffs = -DotProduct(*seg->linedef->v2, -seg->dir)*TextureSScale(tex)*tparam->ScaleX*scale2X; // horizontal
  }

  if (!yflip) {
    texinfo->toffs = TexZ*TextureTScale(tex)*tparam->ScaleY*scale2Y; // vertical
  } else {
    const float texh = tex->GetScaledHeight()/tparam->ScaleY;
    texinfo->toffs = (TexZ+texh)*TextureTScale(tex)*tparam->ScaleY*scale2Y; // vertical
  }

  /*
  if (xofs && (ptrdiff_t)(seg->linedef-&Level->Lines[0]) == 29678) {
    GCon->Logf(NAME_Debug, "*** xofs=%g; xscale=%g; sscaleofs=%g; realscale=%g; realofs=%g", xofs, tparam->ScaleX, TextureOffsetSScale(tex), DivByScale(TextureOffsetSScale(tex), tparam->ScaleX*scale2X), xofs*(TextureOffsetSScale(tex)/tparam->ScaleX));
  }
  */

  // apply texture offsets from texture params
  //texinfo->soffs += sofssign*xofs*(TextureOffsetSScale(tex)/tparam->ScaleX); // horizontal
  //texinfo->toffs += tofssign*yofs*(TextureOffsetTScale(tex)/tparam->ScaleY); // vertical
  texinfo->soffs += xofs*TextureOffsetSScale(tex); // horizontal
  texinfo->toffs += yofs*TextureOffsetTScale(tex); // vertical

  #if 0
  // rotate around bottom left corner (doesn't work)
  if (angle) {
    const float cx = seg->linedef->v1->x; //+seg->dir.x*seg->length/2.0f; //tex->GetWidth()/2.0f;
    const float cy = seg->linedef->v1->y; //+seg->dir.y*seg->length/2.0f; //tex->GetHeight()/2.0f;
    TVec p(cx, cy);
    texinfo->soffs -= DotProduct(texinfo->saxis, p);
    texinfo->toffs -= DotProduct(texinfo->taxis, p);
  }
  #endif
}
