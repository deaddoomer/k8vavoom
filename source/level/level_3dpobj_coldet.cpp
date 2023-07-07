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


//==========================================================================
//
//  VLevel::CanHave3DPolyObjAt2DPoint
//
//  check polyobj blockmap: if it doesn't have any 3d pobs,
//  then there is no reason to look for starting subsector/pobj
//
//==========================================================================
bool VLevel::CanHave3DPolyObjAt2DPoint (const float x, float y) const noexcept {
  if (!Has3DPolyObjects() || !PolyBlockMap) return false;
  const int bmx = MapBlock(x-BlockMapOrgX);
  const int bmy = MapBlock(y-BlockMapOrgY);
  if (bmx < 0 || bmx >= BlockMapWidth || bmy < 0 || bmy >= BlockMapHeight) return false;
  for (polyblock_t *PolyLink = PolyBlockMap[bmy*BlockMapWidth+bmx]; PolyLink; PolyLink = PolyLink->next) {
    polyobj_t *po = PolyLink->polyobj;
    if (po && po->Is3D()) {
      if (po->pofloor.minz < po->poceiling.maxz) return true;
    }
  }
  return false;
}


//==========================================================================
//
//  VLevel::CanHave3DPolyObjAt3DPoint
//
//  check polyobj blockmap: if it doesn't have any 3d pobs,
//  then there is no reason to look for starting subsector/pobj
//
//==========================================================================
bool VLevel::CanHave3DPolyObjAt3DPoint (const TVec &p) const noexcept {
  if (!Has3DPolyObjects() || !PolyBlockMap) return false;
  const int bmx = MapBlock(p.x-BlockMapOrgX);
  const int bmy = MapBlock(p.y-BlockMapOrgY);
  if (bmx < 0 || bmx >= BlockMapWidth || bmy < 0 || bmy >= BlockMapHeight) return false;
  for (polyblock_t *PolyLink = PolyBlockMap[bmy*BlockMapWidth+bmx]; PolyLink; PolyLink = PolyLink->next) {
    polyobj_t *po = PolyLink->polyobj;
    if (po && po->Is3D()) {
      if (po->pofloor.minz < po->poceiling.maxz) {
        if (p.z >= po->pofloor.minz && p.z <= po->poceiling.maxz) return true;
      }
    }
  }
  return false;
}


//==========================================================================
//
//  VLevel::CanHave3DPolyObjAt2DBBox
//
//==========================================================================
bool VLevel::CanHave3DPolyObjAt2DBBox (const float bb2d[4]) const noexcept {
  if (!Has3DPolyObjects() || !PolyBlockMap) return false;
  int x0 = MapBlock(bb2d[BOX2D_MINX]-BlockMapOrgX);
  int y0 = MapBlock(bb2d[BOX2D_MINY]-BlockMapOrgY);
  int x1 = MapBlock(bb2d[BOX2D_MAXX]-BlockMapOrgX);
  int y1 = MapBlock(bb2d[BOX2D_MAXY]-BlockMapOrgY);
  if (x1 < x0) { const int tmp = x0; x0 = x1; x1 = tmp; } // just in case
  if (y1 < y0) { const int tmp = y0; y0 = y1; y1 = tmp; } // just in case
  if (x1 < 0 || x0 >= BlockMapWidth || y1 < 0 || y0 >= BlockMapHeight) return false;
  x0 = max2(0, x0);
  y0 = max2(0, y0);
  x1 = min2(BlockMapWidth-1, x1);
  y1 = min2(BlockMapHeight-1, y1);
  //TODO: use validcount here?
  for (int by = y0; by <= y1; ++by) {
    for (int bx = x0; bx <= x1; ++bx) {
      for (polyblock_t *PolyLink = PolyBlockMap[by*BlockMapWidth+bx]; PolyLink; PolyLink = PolyLink->next) {
        polyobj_t *po = PolyLink->polyobj;
        if (po && po->Is3D()) {
          if (po->pofloor.minz < po->poceiling.maxz) {
            if (Are2DBBoxesOverlap(po->bbox2d, bb2d)) return true;
          }
        }
      }
    }
  }
  return false;
}


//==========================================================================
//
//  VLevel::CanHave3DPolyObjAt3DBBox
//
//==========================================================================
bool VLevel::CanHave3DPolyObjAt3DBBox (const float bb3d[6]) const noexcept {
  if (!Has3DPolyObjects() || !PolyBlockMap) return false;
  int x0 = MapBlock(bb3d[BOX3D_MINX]-BlockMapOrgX);
  int y0 = MapBlock(bb3d[BOX3D_MINY]-BlockMapOrgY);
  int x1 = MapBlock(bb3d[BOX3D_MAXX]-BlockMapOrgX);
  int y1 = MapBlock(bb3d[BOX3D_MAXY]-BlockMapOrgY);
  if (x1 < x0) { const int tmp = x0; x0 = x1; x1 = tmp; } // just in case
  if (y1 < y0) { const int tmp = y0; y0 = y1; y1 = tmp; } // just in case
  if (x1 < 0 || x0 >= BlockMapWidth || y1 < 0 || y0 >= BlockMapHeight) return false;
  x0 = max2(0, x0);
  y0 = max2(0, y0);
  x1 = min2(BlockMapWidth-1, x1);
  y1 = min2(BlockMapHeight-1, y1);
  const float z0 = min2(bb3d[BOX3D_MINZ], bb3d[BOX3D_MAXZ]); // just in case
  const float z1 = max2(bb3d[BOX3D_MINZ], bb3d[BOX3D_MAXZ]); // just in case
  Create2DBBoxFrom3DBBox(bb2d, bb3d);
  //TODO: use validcount here?
  for (int by = y0; by <= y1; ++by) {
    for (int bx = x0; bx <= x1; ++bx) {
      for (polyblock_t *PolyLink = PolyBlockMap[by*BlockMapWidth+bx]; PolyLink; PolyLink = PolyLink->next) {
        polyobj_t *po = PolyLink->polyobj;
        if (po && po->Is3D()) {
          if (po->pofloor.minz < po->poceiling.maxz) {
            if (z0 < po->pofloor.minz || z1 > po->poceiling.maxz) continue;
            if (Are2DBBoxesOverlap(po->bbox2d, bb2d)) return true;
          }
        }
      }
    }
  }
  return false;
}


//==========================================================================
//
//  VLevel::CheckPObjPassPlanes
//
//  returns hit time
//  negative means "no hit"
//
//==========================================================================
float VLevel::CheckPObjPassPlanes (const polyobj_t *po, const TVec &linestart, const TVec &lineend,
                                   TVec *outHitPoint, TVec *outHitNormal, TPlane *outHitPlane)
{
  if (!po) return -1.0f;

  // check polyobject planes
  float bestfrac = FLT_MAX;
  const sec_plane_t *hpl = nullptr;

  for (unsigned f = 0; f < 2; ++f) {
    const sec_plane_t *spl = (f ? &po->poceiling : &po->pofloor);

    const float d1 = spl->PointDistance(linestart);
    if (d1 < 0.0f) continue; // don't shoot back side

    const float d2 = spl->PointDistance(lineend);
    if (d2 >= 0.0f) continue; // didn't hit plane

    //if (d2 > 0.0f) return true; // didn't hit plane (was >=)
    //if (fabsf(d2-d1) < 0.0001f) return true; // too close to zero

    const float frac = d1/(d1-d2); // [0..1], from start
    if (!isFiniteF(frac) || frac < 0.0f || frac > 1.0f) continue; // just in case

    if (!hpl || frac < bestfrac) {
      bestfrac = frac;
      hpl = spl;
    }
  }

  if (!hpl) return -1.0f; // no hit

  if (outHitPoint) *outHitPoint = linestart+(lineend-linestart)*bestfrac;
  if (outHitNormal) *outHitNormal = hpl->normal;
  if (outHitPlane) *outHitPlane = *hpl;

  return bestfrac; // hit detected
}


//==========================================================================
//
//  VLevel::CheckPObjPlanesPoint
//
//  returns hit time
//  negative means "no hit"
//
//==========================================================================
float VLevel::CheckPObjPlanesPoint (const TVec &linestart, const TVec &lineend, const subsector_t *stsub,
                                    TVec *outHitPoint, TVec *outHitNormal, TPlane *outHitPlane, polyobj_t **outpo)
{
  /*
  const float bbox3d[6] = {
    min2(linestart.x, lineend.x),
    min2(linestart.y, lineend.y),
    min2(linestart.z, lineend.z),
    max2(linestart.x, lineend.x),
    max2(linestart.y, lineend.y),
    max2(linestart.z, lineend.z),
  };
  if (!CanHave3DPolyObjAt3DBBox(bbox3d)) return -1.0f;
  */
  if (!CanHave3DPolyObjAt2DPoint(linestart.x, linestart.y)) return -1.0f;

  const subsector_t *sub = (stsub ?: PointInSubsector(linestart));

  float besttime = FLT_MAX;
  bool didHit = false;
  TVec besthp = TVec::ZeroVector, bestnorm = TVec::ZeroVector;
  TPlane bestplane;
  polyobj_t *bestpo = nullptr;

  TVec curhp, curnorm;
  TPlane curplane;

  // if starting point is inside a 3d pobj, check pobj plane hits
  for (auto &&it : sub->PObjFirst()) {
    polyobj_t *po = it.pobj();
    if (!po->Is3D()) continue;
    if (!IsPointInside2DBBox(linestart.x, linestart.y, po->bbox2d)) continue;
    if (!IsPointInsideSector2D(po->GetSector(), linestart.x, linestart.y)) continue;
    // starting point is inside 3d pobj inner sector, check pobj planes

    const float frc = CheckPObjPassPlanes(po, linestart, lineend, &curhp, &curnorm, &curplane);
    if (frc < 0.0f || frc > besttime) continue;

    // check if hitpoint is still inside this pobj
    if (!IsPointInside2DBBox(curhp.x, curhp.y, po->bbox2d)) continue;
    if (!IsPointInsideSector2D(po->GetSector(), curhp.x, curhp.y)) continue;

    // yep, got it
    didHit = true;
    besttime = frc;
    besthp = curhp;
    bestnorm = curnorm;
    bestplane = curplane;
    bestpo = po;
  }

  if (!didHit) return -1.0f;

  if (outHitPoint) *outHitPoint = besthp;
  if (outHitNormal) *outHitNormal = bestnorm;
  if (outHitPlane) *outHitPlane = bestplane;
  if (outpo) *outpo = bestpo;
  return besttime;
}


//==========================================================================
//
//  VLevel::IsPointInsideSubsector2D
//
//==========================================================================
bool VLevel::IsPointInsideSubsector2D (const subsector_t *sub, const float x, const float y) const noexcept {
  if (!sub) return false;
  // subsector must be closed (it is guaranteed if it have enough segs)
  if (sub->numlines < 3) return false;
  if (!IsPointInside2DBBox(x, y, sub->bbox2d)) return false;
  const TVec p(x, y, 0.0f);
  const seg_t *seg = &Segs[sub->firstline];
  for (int f = sub->numlines; f--; ++seg) {
    //if (seg->PointDistance(p) < 0.0f) return false;
    if (seg->normal.dot2D(p) < seg->dist) return false; // slightly faster
  }
  return true;
}


//==========================================================================
//
//  VLevel::IsBBox2DTouchingSector
//
//==========================================================================
bool VLevel::IsBBox2DTouchingSubsector (const subsector_t *sub, const float bb2d[4]) const noexcept {
  if (!sub) return false;
  // subsector must be closed (it is guaranteed if it have enough segs)
  if (sub->numlines < 3) return false;
  // if rectangles aren't intersecting... you got it
  if (!Are2DBBoxesOverlap(sub->bbox2d, bb2d)) return false;
  // if subsector is fully inside our rect, we're done
  // strictly speaking, this check is not necessary, because
  // for each seg `checkBox2DInclusive()` will select the point that is
  // "on the right side", but why not?
  if (sub->bbox2d[BOX2D_MINX] >= bb2d[BOX2D_MINX] &&
      sub->bbox2d[BOX2D_MINY] >= bb2d[BOX2D_MINY] &&
      sub->bbox2d[BOX2D_MAXX] <= bb2d[BOX2D_MAXX] &&
      sub->bbox2d[BOX2D_MAXY] <= bb2d[BOX2D_MAXY])
  {
    return true;
  }
  // check subsector segs
  // if our rect is fully outside of at least one seg, it is outside of subsector
  // otherwise we are inside, and can return success
  const seg_t *seg = &Segs[sub->firstline];
  for (int f = sub->numlines; f--; ++seg) {
    //if (!seg->checkBox2DInclusive(bb2d)) return false;
    if (seg->normal.dot2D(seg->get2DBBoxRejectPoint(bb2d)) < seg->dist) return false; // slightly faster
  }
  return true;
}


//==========================================================================
//
//  VLevel::IsPointInsideSector
//
//==========================================================================
bool VLevel::IsPointInsideSector2D (const sector_t *sec, const float x, const float y) const noexcept {
  if (!sec) return false;
  for (const subsector_t *sub = sec->subsectors; sub; sub = sub->seclink) {
    if (IsPointInsideSubsector2D(sub, x, y)) return true;
  }
  return false;
}


//==========================================================================
//
//  VLevel::IsBBox2DTouchingSector
//
//==========================================================================
bool VLevel::IsBBox2DTouchingSector (const sector_t *sec, const float bb2d[4]) const noexcept {
  if (!sec) return false;
  for (const subsector_t *sub = sec->subsectors; sub; sub = sub->seclink) {
    if (IsBBox2DTouchingSubsector(sub, bb2d)) return true;
  }
  return false;
}


//==========================================================================
//
//  VLevel::Is2DPointInside3DPolyObj
//
//==========================================================================
bool VLevel::Is2DPointInside3DPolyObj (const polyobj_t *po, const float x, const float y) const noexcept {
  if (!po || !po->Is3D()) return false;
  if (!IsPointInside2DBBox(x, y, po->bbox2d)) return false;
  return IsPointInsideSector2D(po->GetSector(), x, y);
}


//==========================================================================
//
//  VLevel::Is3DPointInside3DPolyObj
//
//==========================================================================
bool VLevel::Is3DPointInside3DPolyObj (const polyobj_t *po, const TVec &pos) const noexcept {
  if (!po || !po->Is3D()) return false;
  if (pos.z < po->pofloor.minz || pos.z > po->poceiling.maxz) return false;
  if (!IsPointInside2DBBox(pos.x, pos.y, po->bbox2d)) return false;
  return IsPointInsideSector2D(po->GetSector(), pos.x, pos.y);
}


//==========================================================================
//
//  VLevel::IsBBox2DTouching3DPolyObj
//
//==========================================================================
bool VLevel::IsBBox2DTouching3DPolyObj (const polyobj_t *po, const float bb2d[4]) const noexcept {
  if (!po || !po->Is3D()) return false;
  if (!Are2DBBoxesOverlap(po->bbox2d, bb2d)) return false;
  return IsBBox2DTouchingSector(po->GetSector(), bb2d);
}


//==========================================================================
//
//  VLevel::IsBBox3DTouching3DPolyObj
//
//==========================================================================
bool VLevel::IsBBox3DTouching3DPolyObj (const polyobj_t *po, const float bb3d[6]) const noexcept {
  if (!po || !po->Is3D()) return false;
  if (bb3d[BOX3D_MAXZ] < po->pofloor.minz || bb3d[BOX3D_MINZ] > po->poceiling.maxz) return false;
  Create2DBBoxFrom3DBBox(bb2d, bb3d);
  if (!Are2DBBoxesOverlap(po->bbox2d, bb2d)) return false;
  return IsBBox2DTouchingSector(po->GetSector(), bb2d);
}



//**************************************************************************
//
// VavoomC API
//
//**************************************************************************

//native finalt float CheckPObjPlanesPoint (const TVec linestart, const TVec lineend,
//                                          optional out TVec outHitPoint, optional out TVec outHitNormal);
IMPLEMENT_FUNCTION(VLevel, CheckPObjPlanesPoint) {
  TVec linestart, lineend;
  VOptParamPtr<TVec> outHitPoint;
  VOptParamPtr<TVec> outHitNormal;
  vobjGetParamSelf(linestart, lineend, outHitPoint, outHitNormal);
  RET_FLOAT(Self->CheckPObjPlanesPoint(linestart, lineend, nullptr, outHitPoint, outHitNormal, nullptr, nullptr));
}
