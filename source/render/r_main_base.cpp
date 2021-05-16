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
//  VRenderLevelPublic::VRenderLevelPublic
//
//==========================================================================
VRenderLevelPublic::VRenderLevelPublic () noexcept
  : staticLightsFiltered(false)
  //, clip_base()
  //, refdef()
{
  Drawer->MirrorFlip = false;
  Drawer->MirrorClip = false;
}


//==========================================================================
//
//  VRenderLevelPublic::SpreadDecal
//
//  will create new decals for all touching subsectors
//  `origdc` must be valid, with proper flags and region number
//  `origdc` won't be modified, owned, or destroyed
//
//==========================================================================
void VRenderLevelPublic::SpreadDecal (const decal_t *origdc) {
  if (!origdc) return;
  vassert(origdc->slidesec);
  vassert((origdc->dcsurf&decal_t::SurfTypeMask) == decal_t::Floor || (origdc->dcsurf&decal_t::SurfTypeMask) == decal_t::Ceiling);
  vassert(origdc->eregindex >= 0);
  int ridx = origdc->eregindex;
  for (const sec_region_t *sr = origdc->slidesec->eregions; sr; sr = sr->next) {
    --ridx;
    vassert(ridx >= 0);
  }
}
