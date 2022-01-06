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
#include "../gamedefs.h"
#include "r_local.h"


static VCvarF r_dbg_wtrotate_tmult("r_dbg_wtrotate_tmult", "0.0", "Wall texture rotation time multiplier (for debug)", CVAR_PreInit|CVAR_NoShadow/*|CVAR_Archive*/);


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
  texinfo->ColorMap = CM_Default;
  texinfo->saxis = texinfo->saxisLM = TVec(1.0f, 0.0f, 0.0f);
  texinfo->taxis = texinfo->taxisLM = TVec(0.0f, 0.0f, -1.0f);
  texinfo->soffs = texinfo->toffs = 0.0f;
}


static double prevtime = -1.0;


//==========================================================================
//
//  VRenderLevelShared::SetupTextureAxesOffsetNew
//
//  used only for normal wall textures: top, mid, bottom
//
//  it seems that `segsidedef` offset is in effect, but scaling is not
//
//==========================================================================
void VRenderLevelShared::SetupTextureAxesOffsetEx (seg_t *seg, texinfo_t *texinfo, VTexture *tex, const side_tex_params_t *tparam, float &TexZ, const side_tex_params_t *segparam, TAxType type) {
  texinfo->Tex = tex;
  texinfo->noDecals = tex->noDecals;
  // can be fixed later
  // for 3d floor sides this is not required (alpha already set by the caller)
  if (!segparam) {
    texinfo->Alpha = 1.1f;
    texinfo->Additive = false;
  }
  texinfo->ColorMap = CM_Default;

  const bool xflip = ((tparam->Flags^(segparam ? segparam->Flags : 0u))&STP_FLIP_X);
  const bool yflip = ((tparam->Flags^(segparam ? segparam->Flags : 0u))&STP_FLIP_Y);

  const bool xbroken = ((tparam->Flags^(segparam ? segparam->Flags : 0u))&STP_BROKEN_FLIP_X);
  const bool ybroken = ((tparam->Flags^(segparam ? segparam->Flags : 0u))&STP_BROKEN_FLIP_Y);

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

  // textures with broken flipping cannot be rotated
  if ((xflip || yflip) && (xbroken || ybroken)) angle = 0.0f;

  // there is no reason to rotate or flip lightmap texture
  texinfo->saxisLM = seg->ndir;
  texinfo->taxisLM = TVec(0.0f, 0.0f, -1.0f);

  // calculate texture axes (this also does scaling)
  // Doom "up" is positive `z`
  // texture origin is left bottom corner (don't even ask me why)
  // positive `toffs` moves texture origin up
  if (angle == 0.0f) {
    // most common case
    texinfo->saxis = seg->ndir;
    texinfo->taxis = TVec(0.0f, 0.0f, -1.0f);
  } else {
    float s, c;
    msincos(angle, &s, &c);
    // rotate seg direction
    texinfo->taxis = TVec(s*seg->ndir.x, s*seg->ndir.y, -c);
    texinfo->saxis = seg->normal.cross(texinfo->taxis).normalise();
  }
  texinfo->saxis *= TextureSScale(tex)*tparam->ScaleX;
  texinfo->taxis *= TextureTScale(tex)*tparam->ScaleY;

  if (xflip) texinfo->saxis = -texinfo->saxis;
  if (yflip) texinfo->taxis = -texinfo->taxis;

  // basic texture offset, so it will start at `v1`
  //texinfo->soffs = -texinfo->saxis.dot(*seg->linedef->v1); // horizontal
  //texinfo->toffs = -texinfo->taxis.dot(TVec(seg->linedef->v1->x, seg->linedef->v1->y, TexZ)); // vertical
  //for long memories... texinfo->soffs += c*seg->offset-seg->offset;

  // x offset need not to be modified (i hope)
  float xofs = tparam->TextureOffset+(segparam ? segparam->TextureOffset : 0.0f);
  if (xflip) xofs = -xofs;

  // y offset should be modified, because non-wrapping textures physically moves
  float yofs = tparam->RowOffset+(segparam ? segparam->RowOffset : 0.0f);
  if (yflip) yofs = -yofs;

  // non-wrapping middle texture?
  if (IsTAxWrapFlagActive(type)) {
    if (((seg->linedef->flags&ML_WRAP_MIDTEX)|(seg->sidedef->Flags&SDF_WRAPMIDTEX)) == 0) {
      // yeah, move TexZ
      //TexZ += yofs*(TextureOffsetTScale(tex)/tparam->ScaleY);
      // this seems to be what GZDoom does
      #if 1
      TexZ += yofs/(TextureTScale(tex)/TextureOffsetTScale(tex)*tparam->ScaleY);
      #else
      TexZ += yofs/(TextureTScale(tex)/TextureOffsetTScale(tex));
      #endif
      yofs = 0.0f;
    }
  }

  //texinfo->soffs = 0.0f;
  const TVec *v;
  TVec ndir;
  if (!xflip) {
    v = (seg->side == 0 ? seg->linedef->v1 : seg->linedef->v2);
    ndir = seg->ndir;
  } else if (xflip && !xbroken) {
    // flipped
    v = (seg->side == 0 ? seg->linedef->v2 : seg->linedef->v1);
    ndir = -seg->ndir;
  } else {
    // flipped, broken gzdoom version
    // this starts texture mapping from the same vertex as normal version, but mirrors the texture (stupid bug promoted to feature, in typical ZDoom style)
    v = (seg->side == 0 ? seg->linedef->v1 : seg->linedef->v2);
    ndir = -seg->ndir;
  }
  texinfo->soffs = -ndir.dot(*v)*TextureSScale(tex)*tparam->ScaleX; // horizontal

  if (!yflip) {
    texinfo->toffs = TexZ*TextureTScale(tex)*tparam->ScaleY; // vertical
  } else {
    const float texh = tex->GetScaledHeightF()/tparam->ScaleY;
    texinfo->toffs = (TexZ+texh)*TextureTScale(tex)*tparam->ScaleY; // vertical
  }

  /*
  if (xofs && (ptrdiff_t)(seg->linedef-&Level->Lines[0]) == 29678) {
    GCon->Logf(NAME_Debug, "*** xofs=%g; xscale=%g; sscaleofs=%g; realscale=%g; realofs=%g", xofs, tparam->ScaleX, TextureOffsetSScale(tex), DivByScale(TextureOffsetSScale(tex), tparam->ScaleX), xofs*(TextureOffsetSScale(tex)/tparam->ScaleX));
  }
  */

  // apply texture offsets from texture params
  //texinfo->soffs += sofssign*xofs*(TextureOffsetSScale(tex)/tparam->ScaleX); // horizontal
  //texinfo->toffs += tofssign*yofs*(TextureOffsetTScale(tex)/tparam->ScaleY); // vertical

  // k8: wtf? seems that offsets need not to be scaled (thanks, Remilia!)
  #if 0
  texinfo->soffs += xofs*TextureOffsetSScale(tex)/tparam->ScaleX; // horizontal
  texinfo->toffs += yofs*TextureOffsetTScale(tex)/tparam->ScaleY; // vertical
  #else
    #if 1
  texinfo->soffs += xofs*TextureOffsetSScale(tex); // horizontal
  texinfo->toffs += yofs*TextureOffsetTScale(tex); // vertical
    #else
  {
    TVec p(xofs*TextureOffsetSScale(tex)/tparam->ScaleX, yofs*TextureOffsetTScale(tex)/tparam->ScaleY);
    texinfo->soffs -= p.dot(texinfo->saxis);
    texinfo->toffs -= p.dot(texinfo->taxis);
  }
    #endif
  #endif

  #if 0
  // rotate around bottom left corner (doesn't work)
  if (angle) {
    const float cx = v->x; //+seg->ndir.x*seg->length*0.5f; //tex->GetWidth()*0.5f;
    const float cy = v->y; //+seg->ndir.y*seg->length*0.5f; //tex->GetHeight()*0.5f;
    TVec p(cx, cy);
    texinfo->soffs -= p.dot(texinfo->saxis);
    texinfo->toffs -= p.dot(texinfo->taxis);
  }
  #endif
}
