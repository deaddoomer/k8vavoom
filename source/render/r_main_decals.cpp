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
//**  Copyright (C) 2018-2023 Ketmar Dark
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
#include "../psim/p_decal.h"
#include "r_local.h"


//==========================================================================
//
//  VRenderLevelShared::AppendFlatDecal
//
//  this will NOT take ownership, nor create any clones!
//  will include decal into list of decals for the given subregion
//
//==========================================================================
void VRenderLevelShared::AppendFlatDecal (decal_t *dc) {
  vassert(dc);
  vassert(dc->sub);
  vassert(!dc->sreg);
  vassert(dc->isFloor() || dc->isCeiling());
  vassert(dc->eregindex >= 0);
  vassert(!dc->sregnext);
  vassert(!dc->sregprev);

  // find sector region
  const sec_region_t *myreg = nullptr;
  int ridx = dc->eregindex;
  for (const sec_region_t *sr = dc->sub->sector->eregions; sr; sr = sr->next, --ridx) {
    if (ridx == 0) {
      myreg = sr;
      break;
    }
  }
  vassert(myreg);

  // find subsector subregion
  subregion_t *mysreg = nullptr;
  for (subregion_t *reg = dc->sub->regions; reg; reg = reg->next) {
    if (reg->secregion == myreg) {
      mysreg = reg;
      break;
    }
  }
  vassert(mysreg);

  dc->sreg = mysreg;

  // append to the corresponding list
  subregion_info_t *nfo = &SubRegionInfo[mysreg->rdindex];
  if (dc->isFloor()) {
    DLListAppendEx(dc, nfo->floordecalhead, nfo->floordecaltail, sregprev, sregnext);
  } else {
    DLListAppendEx(dc, nfo->ceildecalhead, nfo->ceildecaltail, sregprev, sregnext);
  }
}


//==========================================================================
//
//  VRenderLevelShared::RemoveFlatDecal
//
//  call this before destroying the decal
//
//==========================================================================
void VRenderLevelShared::RemoveFlatDecal (decal_t *dc) {
  vassert(dc);
  vassert(dc->sub);
  vassert(dc->sreg);
  vassert(dc->isFloor() || dc->isCeiling());

  // remove from the corresponding list
  subregion_info_t *nfo = &SubRegionInfo[dc->sreg->rdindex];
  if (dc->isFloor()) {
    DLListRemoveEx(dc, nfo->floordecalhead, nfo->floordecaltail, sregprev, sregnext);
  } else {
    DLListRemoveEx(dc, nfo->ceildecalhead, nfo->ceildecaltail, sregprev, sregnext);
  }

  dc->sreg = nullptr;
  dc->sregprev = dc->sregnext = nullptr;
}


//==========================================================================
//
//  VRenderLevelShared::GetSRegFloorDecalHead
//
//==========================================================================
decal_t *VRenderLevelShared::GetSRegFloorDecalHead (const subregion_t *sreg) noexcept {
  if (!sreg || !SubRegionInfo) return nullptr;
  return SubRegionInfo[sreg->rdindex].floordecalhead;
}


//==========================================================================
//
//  VRenderLevelShared::GetSRegCeilingDecalHead
//
//==========================================================================
decal_t *VRenderLevelShared::GetSRegCeilingDecalHead (const subregion_t *sreg) noexcept {
  if (!sreg || !SubRegionInfo) return nullptr;
  return SubRegionInfo[sreg->rdindex].ceildecalhead;
}
