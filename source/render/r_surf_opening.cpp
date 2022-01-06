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
  const float v1distbot = v0.dot(normal)-dist;
  const float v1disttop = v1.dot(normal)-dist;
  // positive distance means "above"
  if (isFloor) {
    if (v1disttop >= 0.0f && v1distbot >= 0.0f) return VRenderLevelShared::QTop;
    if (v1disttop < 0.0f && v1distbot < 0.0f) return VRenderLevelShared::QBottom;
  } else {
    if (v1disttop > 0.0f && v1distbot > 0.0f) return VRenderLevelShared::QTop;
    if (v1disttop <= 0.0f && v1distbot <= 0.0f) return VRenderLevelShared::QBottom;
  }
  return VRenderLevelShared::QIntersect;
}


//==========================================================================
//
//  VRenderLevelShared::ClassifyQuadVsRegion
//
//  quad must be valid
//
//==========================================================================
int VRenderLevelShared::ClassifyQuadVsRegion (const TVec quad[4], const sec_region_t *reg) noexcept {
  if (reg->efloor.GetRealDist() >= reg->eceiling.GetRealDist()) return QInvalid; // invalid region

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
//  VRenderLevelShared::ClassifyQuadVsPlane
//
//  quad must be valid
//
//==========================================================================
int VRenderLevelShared::ClassifyQuadVsPlane (const TVec quad[4], const TSecPlaneRef &regplane, bool isFloor) noexcept {
  const int v1floor = CheckQuadLine(quad[QUAD_V1_BOTTOM], quad[QUAD_V1_TOP], regplane, isFloor);
  if (v1floor == QIntersect) return QIntersect;
  const int v2floor = CheckQuadLine(quad[QUAD_V2_BOTTOM], quad[QUAD_V2_TOP], regplane, isFloor);
  if (v2floor == QIntersect) return QIntersect;
  // if on the different sides, it definitely intersects
  if (v1floor != v2floor) return QIntersect;
  // on the same side
  return v1floor;
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
bool VRenderLevelShared::SplitQuadWithPlane (const TVec quad[4], TPlane pl, TVec qbottom[4], TVec qtop[4]) noexcept {
  vassert(quad);
  vassert(pl.isValid());
  vassert(!pl.isVertical());

  // make the plane points up, so negative distance will mean "below"
  if (pl.normal.z < 0.0f) pl.FlipInPlace();

  // `front` is "above", `back` is "below"
  TPlane::ClipWorkData cd;
  int topcount = 0, botcount = 0;

  // 5 is enough, but let's play safe ;-)
  TVec qtoptemp[6];
  TVec qbottemp[6];

  pl.ClipPoly(cd, quad, 4, qtoptemp, &topcount, qbottemp, &botcount, TPlane::CoplanarNone, 0.0f);

  // check invariants (nope, we can get 5 vertices)
  vassert(topcount == 0 || (topcount >= 3 && topcount <= 5));
  vassert(botcount == 0 || (botcount >= 3 && botcount <= 5));
  #if 0
  if (!(topcount == 0 || topcount == 3 || topcount == 4)) {
    GCon->Logf(NAME_Error, "FUCK! topcount=%d", topcount);
    DumpQuad("original quad", quad);
    GCon->Logf(NAME_Debug, "  plane: normal=(%g,%g,%g); dist=%g", pl.normal.x, pl.normal.y, pl.normal.z, pl.dist);
    VStr s; for (int f = 0; f < topcount; ++f) { if (f) s += ", "; s += va("(%g,%g,%g)", qtoptemp[f].x, qtoptemp[f].y, qtoptemp[f].z); }
    GCon->Logf(NAME_Debug, "  resulting top poly: %s", *s);
  }
  if (!(botcount == 0 || botcount == 3 || botcount == 4)) {
    GCon->Logf(NAME_Error, "FUCK! botcount=%d", botcount);
    DumpQuad("original quad", quad);
    GCon->Logf(NAME_Debug, "  plane: normal=(%g,%g,%g); dist=%g", pl.normal.x, pl.normal.y, pl.normal.z, pl.dist);
    VStr s; for (int f = 0; f < botcount; ++f) { if (f) s += ", "; s += va("(%g,%g,%g)", qbottemp[f].x, qbottemp[f].y, qbottemp[f].z); }
    GCon->Logf(NAME_Debug, "  resulting bottom poly: %s", *s);
  }
  #endif

  // don't bother with 5-gons, we cannot process them
  // this can happen with some idiotic/weird sloped 3d floor configurations
  //
  //  a---b
  // +|   |
  //  +   |
  //  |+  |
  //  c-+-d
  //     +
  //
  // as you can see, we'll get 5 vertices here.
  // as our other code can process only valid quads with strictly vertical edges,
  // let's use the heiristic here: if `a` and `b` are both "above", send the original
  // quad to the top, otherwise send it to the bottom.
  // this way we may get some overdraw, but it is acceptable for now.

  if (topcount == 5) {
    vassert(botcount != 5);
    // send the whole quad to the top
    memcpy((void *)qtoptemp, (void *)quad, 4*sizeof(qtoptemp[0]));
    topcount = 4;
  }

  if (botcount == 5) {
    // send the whole quad to the bottom
    vassert(topcount != 5);
    memcpy((void *)qbottemp, (void *)quad, 4*sizeof(qbottemp[0]));
    botcount = 4;
  }

  if (qtop) {
    if (topcount == 0) {
      // no top quad
      memset((void *)qtop, 0, 4*sizeof(qtop[0]));
    } else {
      vassert(topcount == 3 || topcount == 4);
      memcpy((void *)qtop, (void *)qtoptemp, topcount*sizeof(qtop[0]));
      if (topcount == 3) ConvertTriToQuad(qtop);
    }
  }

  if (qbottom) {
    if (botcount == 0) {
      // no top quad
      memset((void *)qbottom, 0, 4*sizeof(qbottom[0]));
    } else {
      vassert(botcount == 3 || botcount == 4);
      memcpy((void *)qbottom, (void *)qbottemp, botcount*sizeof(qbottom[0]));
      if (botcount == 3) ConvertTriToQuad(qbottom);
    }
  }

  if (qtop && qbottom) return ((botcount|topcount) != 0);
  if (qtop) return (topcount != 0);
  if (qbottom) return (botcount != 0);
  return ((botcount|topcount) != 0);
}
