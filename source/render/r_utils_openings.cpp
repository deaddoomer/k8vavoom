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
// this is directly included where necessary
//**************************************************************************
#include "../gamedefs.h"
#include "r_local.h"


//==========================================================================
//
//  VRenderLevelShared::GetHigherRegionAtZ
//
//  the one that is higher
//  valid only if `srcreg` is solid and insane
//
//==========================================================================
sec_region_t *VRenderLevelShared::GetHigherRegionAtZ (sector_t *sector, const float srcrtopz, const sec_region_t *reg2skip) noexcept {
  vassert(sector);
  if (!sector->eregions->next) return sector->eregions;
  // get distance to ceiling
  // we want the nearest ceiling
  float bestdist = FLT_MAX;
  sec_region_t *bestreg = sector->eregions;
  for (sec_region_t *reg = sector->eregions->next; reg; reg = reg->next) {
    // ignore sane regions here, because they don't change lighting underneath
    if (reg == reg2skip || (reg->regflags&(sec_region_t::RF_NonSolid|sec_region_t::RF_OnlyVisual|sec_region_t::RF_SaneRegion|sec_region_t::RF_BaseRegion)) != 0) continue;
    const float rtopz = reg->eceiling.GetRealDist();
    if (srcrtopz >= rtopz) continue; // source region is higher
    const float rbotz = reg->efloor.GetRealDist();
    // ignore paper-thin regions (nope, it sill may influence lighting)
    if (rtopz < rbotz) continue; // invalid, ignore
    const float dist = srcrtopz-rbotz;
    //if (dist <= 0.0f) continue; // too low
    if (dist < bestdist) {
      bestdist = dist;
      bestreg = reg;
    }
  }
  return bestreg;
}


//==========================================================================
//
//  VRenderLevelShared::GetHigherRegion
//
//  the one that is higher
//  valid only if `srcreg` is solid and insane
//
//==========================================================================
sec_region_t *VRenderLevelShared::GetHigherRegion (sector_t *sector, sec_region_t *srcreg) noexcept {
  vassert(sector);
  if (!srcreg || !sector->eregions->next) return sector->eregions;
  // get distance to ceiling
  // we want the nearest ceiling
  const float srcrtopz = (srcreg->regflags&sec_region_t::RF_BaseRegion ? srcreg->efloor.GetRealDist() : srcreg->eceiling.GetRealDist());
  return GetHigherRegionAtZ(sector, srcrtopz, srcreg);
}
