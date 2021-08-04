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
//#include "p_entity.h"
//#include "p_levelinfo.h"


//==========================================================================
//
//  Script polyobject methods
//
//==========================================================================
//native final void SpawnPolyobj (mthing_t *thing, float x, float y, float height, int tag, bool crush, bool hurt);
IMPLEMENT_FUNCTION(VLevel, SpawnPolyobj) {
  mthing_t *thing;
  float x, y, height;
  int tag;
  bool crush, hurt;
  vobjGetParamSelf(thing, x, y, height, tag, crush, hurt);
  Self->SpawnPolyobj(thing, x, y, height, tag, crush, hurt);
}

//native final void AddPolyAnchorPoint (mthing_t *thing, float x, float y, float height, int tag);
IMPLEMENT_FUNCTION(VLevel, AddPolyAnchorPoint) {
  mthing_t *thing;
  float x, y, height;
  int tag;
  vobjGetParamSelf(thing, x, y, height, tag);
  Self->AddPolyAnchorPoint(thing, x, y, height, tag);
}

//native final void Add3DPolyobjLink (mthing_t *thing, int srcpid, int destpid);
IMPLEMENT_FUNCTION(VLevel, Add3DPolyobjLink) {
  mthing_t *thing;
  int srcpid, destpid;
  vobjGetParamSelf(thing, srcpid, destpid);
  Self->Add3DPolyobjLink(thing, srcpid, destpid);
}

//native final polyobj_t *GetPolyobj (int polyNum);
IMPLEMENT_FUNCTION(VLevel, GetPolyobj) {
  int tag;
  vobjGetParamSelf(tag);
  RET_PTR(Self->GetPolyobj(tag));
}

//native final int GetPolyobjMirror (int poly);
IMPLEMENT_FUNCTION(VLevel, GetPolyobjMirror) {
  int tag;
  vobjGetParamSelf(tag);
  RET_INT(Self->GetPolyobjMirror(tag));
}

//native final bool MovePolyobj (int num, float x, float y, optional float z, optional int flags);
IMPLEMENT_FUNCTION(VLevel, MovePolyobj) {
  int tag;
  float x, y;
  VOptParamFloat z(0);
  VOptParamInt flags(0);
  vobjGetParamSelf(tag, x, y, z, flags);
  RET_BOOL(Self->MovePolyobj(tag, x, y, z, flags));
}

//native final bool RotatePolyobj (int num, float angle, optional int flags);
IMPLEMENT_FUNCTION(VLevel, RotatePolyobj) {
  int tag;
  float angle;
  VOptParamInt flags(0);
  vobjGetParamSelf(tag, angle, flags);
  RET_BOOL(Self->RotatePolyobj(tag, angle, flags));
}
