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
static bool IsTransDoorHack (const seg_t *seg, bool fortop) {
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
static inline  bool IsTransDoorHackBot (const seg_t *seg) {
  return IsTransDoorHack(seg, false);
}
