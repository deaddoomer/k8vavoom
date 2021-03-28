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
//  VRenderLevelShared::GetOpeningAbove
//
//  clip quad with non-vertical plane
//  quad itself must be vertical
//
//  as our quad is vertical, planes can only cut it in half
//  ignore planes above or below the quad
//
//  returns `true` if was cut; `quad` contains new "bottom" quad
//  if returned `false`, `quad` is unchanged
//
//  quad points are:
//   [0]: left bottom point
//   [1]: left top point
//   [2]: right top point
//   [3]: right bottom point
//
//==========================================================================
bool VRenderLevelShared::ClipQuadWithPlane (TVec quad[4], const TVec normal, const float dist) noexcept {
  vassert(isFiniteF(normal.z) && normal.z != 0.0f);
  vassert(isFiniteF(dist));
  TPlane pl;
  pl.normal = normal;
  pl.dist = dist;
  //if (pl.normal.z < 0.0f) pl.flipInPlace(); // now our plane points up -- doesn't matter

  bool wasHit = false;

  // left hitpoint
  do {
    const float dot1 = pl.PointDistance(quad[0]);
    const float dot2 = pl.PointDistance(quad[1]);
    // do not use multiplication to check: zero speedup, lost accuracy
    //if (dot1*dot2 >= 0.0f) break; // plane isn't crossed
    if (dot1 <= 0.0f && dot2 <= 0.0f) break; // didn't reached back side (or on the plane)
    if (dot1 >= 0.0f && dot2 >= 0.0f) break; // didn't reached front side (or on the plane)
    // find the fractional intersection time
    const TVec dir = quad[1]-quad[0];
    const float den = DotProduct(pl.normal, dir);
    if (den == 0.0f) break;
    const float num = pl.dist-DotProduct(quad[0], pl.normal);
    const float frac = num/den;
    if (frac < 0.0f || frac > 1.0f) break; // behind source or beyond end point
    // now we have a hitpoint
    wasHit = true;
    // split it: cut original quad top
    quad[1] = quad[0]+dir*frac;
  } while (0);

  // right hitpoint
  do {
    const float dot1 = pl.PointDistance(quad[3]);
    const float dot2 = pl.PointDistance(quad[2]);
    // do not use multiplication to check: zero speedup, lost accuracy
    //if (dot1*dot2 >= 0.0f) break; // plane isn't crossed
    if (dot1 <= 0.0f && dot2 <= 0.0f) break; // didn't reached back side (or on the plane)
    if (dot1 >= 0.0f && dot2 >= 0.0f) break; // didn't reached front side (or on the plane)
    // find the fractional intersection time
    const TVec dir = quad[2]-quad[3];
    const float den = DotProduct(pl.normal, dir);
    if (den == 0.0f) break;
    const float num = pl.dist-DotProduct(quad[3], pl.normal);
    const float frac = num/den;
    if (frac < 0.0f || frac > 1.0f) break; // behind source or beyond end point
    // now we have a hitpoint
    wasHit = true;
    // split it: cut original quad top
    quad[2] = quad[3]+dir*frac;
  } while (0);

  return wasHit;
}


//==========================================================================
//
//  VRenderLevelShared::ClipQuadWithRegions
//
//  this is used to split the wall with 3d surfaces
//  returns the lowest clipping region
//
//==========================================================================
sec_region_t *VRenderLevelShared::ClipQuadWithRegions (TVec quad[4], sector_t *sec) noexcept {
  // no need to split with base region
  sec_region_t *clipreg = nullptr;
  for (sec_region_t *reg = sec->eregions; reg; reg = reg->next) {
    if (reg->regflags&(sec_region_t::RF_NonSolid|sec_region_t::RF_OnlyVisual|sec_region_t::RF_BaseRegion/*|sec_region_t::RF_SaneRegion*/)) continue;
    const float z1 = reg->eceiling.GetRealDist();
    const float z0 = reg->efloor.GetRealDist();
    if (z1 < z0) continue; // invalid region
    // clip only with floor, ceiling is never lower than it
    if (ClipQuadWithPlane(quad, reg->efloor.GetNormal(), reg->efloor.GetDist())) {
      clipreg = reg;
    }
  }
  return clipreg;
}
