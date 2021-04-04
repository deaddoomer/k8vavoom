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


enum {
  QInvalid = -1, // invalid region
  QIntersect = 0,
  QTop = 1,
  QBottom = 2,
  QInside = 3,
};


//==========================================================================
//
//  CheckQuadLine
//
//==========================================================================
static int CheckQuadLine (const TVec &v0, const TVec &v1, const TSecPlaneRef &plane, const bool isFloor) {
  TVec normal = plane.GetNormal();
  float dist = plane.GetDist();
  if (normal.z < 0.0f) {
    // plane points down, invert the plane
    normal = -normal;
    dist = -dist;
  }
  const float v1distbot = DotProduct(v0, normal)-dist;
  const float v1disttop = DotProduct(v1, normal)-dist;
  /*
  float v1distbot = plane.PointDistance(v0);
  float v1disttop = plane.PointDistance(v1);
  if (plane.GetNormalZ() < 0.0f) {
    // plane points down, invert the distances
    v1distbot = -v1distbot;
    v1disttop = -v1disttop;
  }
  */
  // positive distance means "above"
  if (isFloor) {
    if (v1disttop >= 0.0f && v1distbot >= 0.0f) return QTop;
    if (v1disttop < 0.0f && v1distbot < 0.0f) return QBottom;
  } else {
    if (v1disttop > 0.0f && v1distbot > 0.0f) return QTop;
    if (v1disttop <= 0.0f && v1distbot <= 0.0f) return QBottom;
  }
  return QIntersect;
}


//==========================================================================
//
//  VRenderLevelShared::CheckQuadVsRegion
//
//  quad must be valid
//
//==========================================================================
int VRenderLevelShared::CheckQuadVsRegion (const TVec quad[4], const sec_region_t *reg) noexcept {
  const float fz1 = reg->efloor.GetPointZClamped(quad[QUAD_V1_TOP]);
  const float cz1 = reg->eceiling.GetPointZClamped(quad[QUAD_V1_TOP]);
  if (fz1 > cz1) return QInvalid; // invalid region

  const float fz2 = reg->efloor.GetPointZClamped(quad[QUAD_V2_TOP]);
  const float cz2 = reg->eceiling.GetPointZClamped(quad[QUAD_V2_TOP]);
  if (fz2 > cz2) return QInvalid; // invalid region

  // check the floor
  const int v1floor = CheckQuadLine(quad[QUAD_V1_BOTTOM], quad[QUAD_V1_TOP], reg->efloor, true);
  if (v1floor == QIntersect) return QIntersect;
  const int v2floor = CheckQuadLine(quad[QUAD_V2_BOTTOM], quad[QUAD_V2_TOP], reg->efloor, true);
  if (v2floor == QIntersect) return QIntersect;
  // if on the different sides, it definitely intersects
  if (v1floor != v2floor) return QIntersect;
  // on the same side
  // if both lines are below the floor, the whole quad is below
  if (v1floor == QBottom) return QBottom;

  // the quad is above the floor, check the ceiling
  const int v1ceiling = CheckQuadLine(quad[QUAD_V1_BOTTOM], quad[QUAD_V1_TOP], reg->eceiling, false);
  if (v1ceiling == QIntersect) return QIntersect;
  const int v2ceiling = CheckQuadLine(quad[QUAD_V2_BOTTOM], quad[QUAD_V2_TOP], reg->eceiling, false);
  if (v2ceiling == QIntersect) return QIntersect;
  // if on the different sides, it definitely intersects
  if (v1ceiling != v2ceiling) return QIntersect;
  // on the same side
  // if both lines are above the ceiling, the whole quad is above
  if (v1ceiling == QTop) return QTop;

  // we are completely inside
  return QInside;
}


//==========================================================================
//
//  VRenderLevelShared::ClassifyRegionsVsQuad
//
//  sets region flags
//  quad must be valid
//  returns `true` if the quad is completely inside some region
//
//==========================================================================
bool VRenderLevelShared::ClassifyRegionsVsQuad (const TVec quad[4], sec_region_t *reg, bool onlySolid, const sec_region_t *ignorereg) noexcept {
  const vuint32 mask = (onlySolid ? sec_region_t::RF_NonSolid : 0u)|sec_region_t::RF_OnlyVisual|sec_region_t::RF_BaseRegion;
  for (; reg; reg = reg->next) {
    reg->regflags &= ~sec_region_t::RF_SplitMask;
    if (reg == ignorereg || (reg->regflags&mask) || (onlySolid && !reg->isBlockingExtraLine())) {
      reg->regflags |= sec_region_t::RF_Invalid;
      /*
      if (reg == ignorereg) reg->regflags |= sec_region_t::RF_QuadIntersects;
      if (reg->regflags&mask) reg->regflags |= sec_region_t::RF_QuadAbove;
      if (onlySolid && !reg->isBlockingExtraLine()) reg->regflags |= sec_region_t::RF_QuadBelow;
      */
      continue;
    }
    const int rclass = CheckQuadVsRegion(quad, reg);
    switch (rclass) {
      case QInvalid: reg->regflags |= sec_region_t::RF_Invalid; break;
      case QIntersect: reg->regflags |= sec_region_t::RF_QuadIntersects; break;
      case QTop: reg->regflags |= sec_region_t::RF_QuadAbove; break;
      case QBottom: reg->regflags |= sec_region_t::RF_QuadBelow; break;
      case QInside: return true; // completely inside
      default: Sys_Error("invalid quad-vs-region class");
    }
  }
  return false;
}


//==========================================================================
//
//  ConvertTriToQuad
//
//==========================================================================
static inline void ConvertTriToQuad (TVec quad[4]) noexcept {
  if (quad[QUAD_V1_BOTTOM].z == quad[QUAD_V1_TOP].z) {
    // the last point must be doubled
    quad[QUAD_V2_BOTTOM] = quad[QUAD_V2_TOP];
  } else {
    // the first point must be doubled
    quad[QUAD_V2_BOTTOM] = quad[QUAD_V2_TOP];
    quad[QUAD_V2_TOP] = quad[QUAD_V1_TOP];
    quad[QUAD_V1_TOP] = quad[QUAD_V1_BOTTOM];
  }
}


//==========================================================================
//
//  VRenderLevelShared::isPointInsideSolidReg
//
//  exactly on the floor/ceiling is "inside" too
//  used only in t-junction fixer
//
//==========================================================================
bool VRenderLevelShared::isPointInsideSolidReg (const TVec lv, const float pz, const sec_region_t *streg, const sec_region_t *ignorereg) noexcept {
  for (const sec_region_t *reg = streg; reg; reg = reg->next) {
    if (reg == ignorereg || (reg->regflags&(sec_region_t::RF_NonSolid|sec_region_t::RF_OnlyVisual|sec_region_t::RF_BaseRegion)) != 0) continue;
    if (!reg->isBlockingExtraLine()) continue;
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
//  returns `false` if there is no bottom quad
//  plane orientation doesn't matter, "top" is always on top
//  input quad must be valid
//  either `qbottom` or `qtop` will contain invalid quad
//  (depending if the quad is above or below the plane)
//  `qbottom` and/or `qtop` can be `nullptr`
//  plane orientation doesn't matter (but it must not be vertical)
//  `qbottom` or `qtop` can be the same as `quad`
//
//==========================================================================
bool VRenderLevelShared::SplitQuadWithPlane (const TVec quad[4], TPlane pl, TVec qbottom[4], TVec qtop[4], bool isFloor) noexcept {
  vassert(quad);
  vassert(pl.isValid());
  vassert(!pl.isVertical());

  // make the plane points up, so negative distance will mean "below"
  if (pl.normal.z < 0.0f) pl.flipInPlace();

  // `front` is "above", `back` is "below"
  TPlane::ClipWorkData cd;
  int topcount = 0, botcount = 0;

  #if 1
  GCon->Logf(NAME_Debug, "quad: v1bot=(%g,%g,%g); v1top=(%g,%g,%g); v2top=(%g,%g,%g); v2bot=(%g,%g,%g)",
    quad[QUAD_V1_BOTTOM].x, quad[QUAD_V1_BOTTOM].y, quad[QUAD_V1_BOTTOM].z,
    quad[QUAD_V1_TOP].x, quad[QUAD_V1_TOP].y, quad[QUAD_V1_TOP].z,
    quad[QUAD_V2_TOP].x, quad[QUAD_V2_TOP].y, quad[QUAD_V2_TOP].z,
    quad[QUAD_V2_BOTTOM].x, quad[QUAD_V2_BOTTOM].y, quad[QUAD_V2_BOTTOM].z);
  GCon->Logf(NAME_Debug, "plane: normal=(%g,%g,%g); dist=%g", pl.normal.x, pl.normal.y, pl.normal.z, pl.dist);
  #endif
  pl.ClipPoly(cd, quad, 4, qtop, &topcount, qbottom, &botcount, /*(isFloor ? TPlane::CoplanarFront : TPlane::CoplanarBack)*/TPlane::CoplanarNone, 0.0f);
  // check invariants
  #if 1
  GCon->Logf(NAME_Debug, "topcount=%d; botcount=%d; valid=%d", topcount, botcount, (int)isValidQuad(quad));
  #endif
  vassert(topcount == 0 || topcount == 3 || topcount == 4);
  vassert(botcount == 0 || botcount == 3 || botcount == 4);

  if (qtop) {
    if (topcount == 0) {
      // no top quad
      memset((void *)qtop, 0, 4*sizeof(qtop[0]));
    } else if (topcount == 3) {
      ConvertTriToQuad(qtop);
    }
    #if 1
    if (topcount) {
      GCon->Logf(NAME_Debug, "*** top quad: v1bot=(%g,%g,%g); v1top=(%g,%g,%g); v2top=(%g,%g,%g); v2bot=(%g,%g,%g)",
        qtop[QUAD_V1_BOTTOM].x, qtop[QUAD_V1_BOTTOM].y, qtop[QUAD_V1_BOTTOM].z,
        qtop[QUAD_V1_TOP].x, qtop[QUAD_V1_TOP].y, qtop[QUAD_V1_TOP].z,
        qtop[QUAD_V2_TOP].x, qtop[QUAD_V2_TOP].y, qtop[QUAD_V2_TOP].z,
        qtop[QUAD_V2_BOTTOM].x, qtop[QUAD_V2_BOTTOM].y, qtop[QUAD_V2_BOTTOM].z);
    }
    #endif
  }

  if (qbottom) {
    if (botcount == 0) {
      // no top quad
      memset((void *)qbottom, 0, 4*sizeof(qbottom[0]));
    } else if (botcount == 3) {
      ConvertTriToQuad(qbottom);
    }
    #if 1
    if (botcount) {
      GCon->Logf(NAME_Debug, "*** bottom quad: v1bot=(%g,%g,%g); v1top=(%g,%g,%g); v2top=(%g,%g,%g); v2bot=(%g,%g,%g)",
        qbottom[QUAD_V1_BOTTOM].x, qbottom[QUAD_V1_BOTTOM].y, qbottom[QUAD_V1_BOTTOM].z,
        qbottom[QUAD_V1_TOP].x, qbottom[QUAD_V1_TOP].y, qbottom[QUAD_V1_TOP].z,
        qbottom[QUAD_V2_TOP].x, qbottom[QUAD_V2_TOP].y, qbottom[QUAD_V2_TOP].z,
        qbottom[QUAD_V2_BOTTOM].x, qbottom[QUAD_V2_BOTTOM].y, qbottom[QUAD_V2_BOTTOM].z);
    }
    #endif
  }

  if (qtop && qbottom) return ((botcount|topcount) != 0);
  if (qtop) return (topcount != 0);
  if (qbottom) return (botcount != 0);
  return ((botcount|topcount) != 0);
}


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
sec_region_t *VRenderLevelShared::SplitQuadWithRegions (TVec quad[4], sec_region_t *reg, bool onlySolid, const sec_region_t *ignorereg) noexcept {
  const vuint32 mask = (onlySolid ? sec_region_t::RF_NonSolid : 0u)|sec_region_t::RF_OnlyVisual|sec_region_t::RF_BaseRegion;
  for (; reg; reg = reg->next) {
    if (reg == ignorereg) continue;
    if (reg->regflags&mask) continue;
    if (onlySolid && !reg->isBlockingExtraLine()) continue;
    const int rtype = CheckQuadVsRegion(quad, reg);
    // invalid or at the top? ignore such regions
    if (rtype == QInvalid || rtype == QTop) continue;
    // inside?
    if (rtype == QInside) {
      // nothing to do anymore
      memset((void *)quad, 0, 4*sizeof(quad[0])); // this creates invalid quad
      return reg;
    }
    // intersects?
    if (rtype == QIntersect) {
      if (!SplitQuadWithPlane(quad, reg->efloor, quad, nullptr, true/*isFloor*/)) return reg; // completely splitted away
      continue;
    }
    // completely at the bottom, nothing to do
    vassert(rtype == QBottom);
  }
  // no splits were found
  return nullptr;
}
