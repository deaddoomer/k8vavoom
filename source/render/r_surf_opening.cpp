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
//  VRenderLevelShared::isPointInsideSolidReg
//
//  exactly on the floor/ceiling is "inside" too
//
//==========================================================================
bool VRenderLevelShared::isPointInsideSolidReg (const TVec lv, const float pz, const sec_region_t *streg, const sec_region_t *ignorereg) noexcept {
  for (const sec_region_t *reg = streg; reg; reg = reg->next) {
    if (reg == ignorereg || (reg->regflags&(sec_region_t::RF_NonSolid|sec_region_t::RF_OnlyVisual|sec_region_t::RF_BaseRegion)) != 0) continue;
    const float cz = reg->eceiling.GetPointZ(lv);
    const float fz = reg->efloor.GetPointZ(lv);
    if (cz <= fz) continue; // invisible region
    // paper-thin regions will split planes too
    if (pz >= fz && pz <= cz) return true;
  }
  return false;
}


//==========================================================================
//
//  VRenderLevelShared::SplitQuadWithPlane
//
//  splits quad with a non-vertical plane
//  returns `false` if there was no split
//  (both `qbottom` and `qtop` will contain the original quad in this case)
//  `qbottom` and/or `qtop` can be `nullptr`
//  plane orientation doesn't matter
//  `qbottom` or `qtop` can be the same as `quad`
//
//==========================================================================
bool VRenderLevelShared::SplitQuadWithPlane (const TVec quad[4], const TPlane &pl, TVec qbottom[4], TVec qtop[4]) noexcept {
  vassert(quad);
  vassert(pl.isValid());
  vassert(!pl.isVertical());

  bool wasHit = false;
  if (qbottom && qbottom != quad) memcpy((void *)qbottom, quad, sizeof(quad[0])*4);
  if (qtop && qtop != quad) memcpy((void *)qtop, quad, sizeof(quad[0])*4);

  // left hitpoint
  do {
    const float dot1 = pl.PointDistance(quad[QUAD_V1_BOTTOM]);
    const float dot2 = pl.PointDistance(quad[QUAD_V1_TOP]);
    // do not use multiplication to check: zero speedup, lost accuracy
    //if (dot1*dot2 >= 0.0f) break; // plane isn't crossed
    if (dot1 <= 0.0f && dot2 <= 0.0f) break; // didn't reached back side (or on the plane)
    if (dot1 >= 0.0f && dot2 >= 0.0f) break; // didn't reached front side (or on the plane)
    // find the fractional intersection time
    const TVec dir = quad[QUAD_V1_TOP]-quad[QUAD_V1_BOTTOM];
    const float den = DotProduct(pl.normal, dir);
    if (den == 0.0f) break;
    const float num = pl.dist-DotProduct(quad[QUAD_V1_BOTTOM], pl.normal);
    const float frac = num/den;
    if (frac < 0.0f || frac > 1.0f) break; // behind source or beyond end point
    // now we have a hitpoint
    wasHit = true;
    // split it: cut original quad top
    const TVec hp = quad[QUAD_V1_BOTTOM]+dir*frac;
    if (qbottom) qbottom[QUAD_V1_TOP] = hp;
    if (qtop) qtop[QUAD_V1_BOTTOM] = hp;
  } while (0);

  // right hitpoint
  do {
    const float dot1 = pl.PointDistance(quad[QUAD_V2_BOTTOM]);
    const float dot2 = pl.PointDistance(quad[QUAD_V2_TOP]);
    // do not use multiplication to check: zero speedup, lost accuracy
    //if (dot1*dot2 >= 0.0f) break; // plane isn't crossed
    if (dot1 <= 0.0f && dot2 <= 0.0f) break; // didn't reached back side (or on the plane)
    if (dot1 >= 0.0f && dot2 >= 0.0f) break; // didn't reached front side (or on the plane)
    // find the fractional intersection time
    const TVec dir = quad[QUAD_V2_TOP]-quad[QUAD_V2_BOTTOM];
    const float den = DotProduct(pl.normal, dir);
    if (den == 0.0f) break;
    const float num = pl.dist-DotProduct(quad[QUAD_V2_BOTTOM], pl.normal);
    const float frac = num/den;
    if (frac < 0.0f || frac > 1.0f) break; // behind source or beyond end point
    // now we have a hitpoint
    wasHit = true;
    // split it: cut original quad top
    const TVec hp = quad[QUAD_V2_BOTTOM]+dir*frac;
    if (qbottom) qbottom[QUAD_V2_TOP] = hp;
    if (qtop) qtop[QUAD_V2_BOTTOM] = hp;
  } while (0);

  return wasHit;
}


  // starts with the given region
  // modifies `quad`
  enum {
    QSPLIT_BOTTOM = 0, // leaves bottom part
    QSPLIT_TOP = 1, // leaves top part
  };


//==========================================================================
//
//  VRenderLevelShared::SplitQuadWithRegions
//
//  this is used to split the wall with 3d surfaces
//  returns the lowest clipping region
//
//  note that *partially* overlapping 3d floors with slopes is UB
//  the clipper will shit itself with such configuration
//
//  i.e. if you have one sloped 3d floor that penetrates another 3d floor,
//  the clipper WILL glitch badly
//
//  non-sloped 3d floor penetration should be ok, tho (yet it is still UB)
//
//==========================================================================
sec_region_t *VRenderLevelShared::SplitQuadWithRegions (const int mode, TVec quad[4], sec_region_t *reg, const sec_region_t *ignorereg) noexcept {
  sec_region_t *clipreg = nullptr;
  for (; reg; reg = reg->next) {
    if (reg->regflags&(sec_region_t::RF_NonSolid|sec_region_t::RF_OnlyVisual|sec_region_t::RF_BaseRegion)) continue;
    const line_t *ld = reg->extraline;
    if (ld) {
      if (ld->sidenum[0] < 0 || ld->alpha < 1.0f || (ld->flags&ML_ADDITIVE)) continue;
      const side_t *sd = &Level->Sides[ld->sidenum[0]];
      VTexture *MTex = GTextureManager(sd->MidTexture);
      if (!MTex || MTex->Type == TEXTYPE_Null || MTex->isSeeThrough()) continue;
    }
    // check for invalid region (and ignore it)
    const float fz1 = reg->efloor.GetPointZClamped(quad[QUAD_V1_TOP]);
    const float cz1 = reg->eceiling.GetPointZClamped(quad[QUAD_V1_TOP]);
    if (fz1 > cz1) continue; // invalid region
    const float fz2 = reg->efloor.GetPointZClamped(quad[QUAD_V2_TOP]);
    const float cz2 = reg->eceiling.GetPointZClamped(quad[QUAD_V2_TOP]);
    if (fz2 > cz2) continue; // invalid region
    if (mode == QSPLIT_BOTTOM) {
      // clip only with floor, ceiling is never lower than it
      if (SplitQuadWithPlane(quad, reg->efloor, quad, nullptr)) clipreg = reg; // leave bottom
    } else {
      // clip only with ceiling, floor is never lower than it
      if (SplitQuadWithPlane(quad, reg->eceiling, nullptr, quad)) clipreg = reg; // leave top
    }
  }
  return clipreg;
}
