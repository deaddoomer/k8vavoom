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


//==========================================================================
//
//  VLevel::PointRegionLight
//
//  this is used to get lighting for the given point
//  returns region to use as light param source,
//  and additionally glow flags (`glowFlags` can be nullptr)
//
//  glowFlags: bit 0: floor glow allowed; bit 1: ceiling glow allowed
//  `sector` can be `nullptr`
//
//==========================================================================
sec_region_t *VLevel::PointRegionLight (const subsector_t *sub, const TVec &p, unsigned *glowFlags) {
  if (!sub) sub = PointInSubsector(p);
  const sector_t *sector = sub->sector;

  // polyobject sector?
  if (sector->isAnyPObj()) {
    if (glowFlags) *glowFlags = 3u; // both glows allowed
    return sector->eregions;
  }

  sec_region_t *res = sector->eregions;
  bool wasInsideSwimmable = false;
  float bestheight = FLT_MAX; // for swimmable
  float bestceil = FLT_MAX; // for solid
  unsigned gflg = 3u; // both glows allowed

  if (sector->Has3DFloors()) {
    // check regions
    for (sec_region_t *reg = sector->eregions->next; reg; reg = reg->next) {
      if (reg->regflags&(sec_region_t::RF_BaseRegion|sec_region_t::RF_OnlyVisual)) continue;
      // get opening points
      const float fz = reg->efloor.GetPointZClamped(p);
      const float cz = reg->eceiling.GetPointZClamped(p);
      // ignore paper-thin regions
      if (fz >= cz) continue; // invalid, or paper-thin, ignore
      if (reg->regflags&sec_region_t::RF_NonSolid) {
        // swimmable, should be inside
        if (p.z >= fz && p.z <= cz) {
          const float hgt = cz-fz;
          if (hgt < bestheight) {
            bestheight = hgt;
            res = reg;
            wasInsideSwimmable = true;
          }
        }
      } else {
        // calc glow flags
        if (p.z <= cz) gflg &= ~2u; // we have solid region above us, no ceiling glow
        if (p.z >= fz) gflg &= ~1u; // we have solid region below us, no floor glow
        // solid, should be below
        if (!wasInsideSwimmable && p.z < fz && fz < bestceil) {
          bestceil = fz;
          res = reg;
        }
      }
    }
  }

  // check polyobjects
  if (!wasInsideSwimmable && CanHave3DPolyObjAt2DPoint(p.x, p.y)) {
    for (auto &&it : sub->PObjFirst()) {
      polyobj_t *po = it.pobj();
      if (!po->Is3D()) continue;
      if (!IsPointInside2DBBox(p.x, p.y, po->bbox2d)) continue;
      const float pz0 = po->pofloor.minz;
      const float pz1 = po->poceiling.maxz;
      if (pz0 >= pz1) continue; // paper-thin
      if (p.z < pz1) continue; // not above
      // above pobj, check best ceiling
      if (p.z != pz1) {
        if (pz1 >= bestceil) continue; // this pobj is higher than 3d floor (the thing that should not be)
        if (IsPointInsideSector2D(po->GetSector(), p.x, p.y)) {
          res = po->GetSector()->eregions;
        }
      } else {
        if (IsPointInsideSector2D(po->GetSector(), p.x, p.y)) {
          res = po->GetSector()->eregions;
          break;
        }
      }
    }
  }

  if (glowFlags) *glowFlags = gflg;
  return res;

#if 0
  // early exit if we have no 3d floors
  if (!sector->Has3DFloors()) {
    if (glowFlags) *glowFlags = 3u; // both glows allowed
    return sector->eregions;
  }

  unsigned gflg = 0;

  /*
   (?) if the point is in a water (swimmable), use swimmable region
   otherwise, check distance to region floor (region is from ceiling down to floor), and use the smallest one
   also, ignore paper-thin regions

   additionally, calculate glow flags:
   floor glow is allowed if we are under the bottom-most solid region
   ceiling glow is allowed if we are above the top-most solid region
  */

  sec_region_t *best = nullptr;
  float bestDist = 99999.0f; // minimum distance to region floor
  gflg = 3u; // will be reset when necessary
  for (sec_region_t *reg = sector->eregions->next; reg; reg = reg->next) {
    // ignore base (just in case) and visual-only regions
    if (reg->regflags&(sec_region_t::RF_OnlyVisual|sec_region_t::RF_BaseRegion)) continue;
    const float rtopz = reg->eceiling.GetPointZ(p);
    const float rbotz = reg->efloor.GetPointZ(p);
    // ignore paper-thin regions
    if (rtopz <= rbotz) continue; // invalid, or paper-thin, ignore
    float botDist;
    if (reg->regflags&sec_region_t::RF_NonSolid) {
      // swimmable, use region ceiling
      botDist = rtopz-p.z;
    } else {
      // solid, use region floor
      // calc glow flags
      if (p.z < rtopz) gflg &= ~2u; // we have solid region above us, no ceiling glow
      if (p.z > rbotz) gflg &= ~1u; // we have solid region below us, no floor glow
      // check best distance
      botDist = rbotz-p.z;
    }
    // if we're not in bounds, ignore
    // if further than the current best, ignore
    if (botDist <= 0.0f || botDist > bestDist) continue;
    // found new good region
    best = reg;
    bestDist = botDist;
  }

  if (!best) {
    // use base region
    best = sector->eregions;
  }

  if (glowFlags) *glowFlags = gflg;
  return best;
#endif
}
