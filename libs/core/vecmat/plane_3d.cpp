//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
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
#include "../core.h"


//==========================================================================
//
//  TPlane::SweepSphere
//
//  sphere sweep test
//  `hitpos` will be sphere position when it hits this plane
//  `time` will be normalized collision time (negative if initially stuck)
//  returns `true` if stuck/hit
//
//==========================================================================
bool TPlane::SweepSphere (const TVec &origin, const float radius, const TVec &vdelta, TVec *hitpos, float *time) const noexcept {
  const float rad = fabsf(radius);
  const float d0 = PointDistance(origin);
  // check if the sphere is already touching the plane
  if (fabsf(d0) <= rad) {
    if (hitpos) *hitpos = origin;
    if (time) *time = 0.0f;
    return true;
  }
  const float denom = normal.dot(vdelta);
  // check if the sphere penetrated during movement
  if (denom*dist >= 0.0f) {
    // parallel or too far away
    if (hitpos) *hitpos = origin+vdelta;
    if (time) *time = 1.0f;
    return false;
  }
  // possibly penetrated
  const float r = (dist > 0.0f ? rad : -rad);
  const float t = (r-dist)/denom;
  if (t < 0.0f || t >= 1.0f) {
    if (hitpos) *hitpos = origin+vdelta;
    if (time) *time = 1.0f;
    return false;
  }
  if (time) *time = t; // time
  if (hitpos) *hitpos = origin+t*vdelta-r*normal;
  return true;
}


//==========================================================================
//
//  TPlane::SweepBox3DEx
//
//  box must be valid
//  `time` will be set only if our box hits/penetrates the plane
//  negative time means "stuck" (and it will be exit time)
//  `bbox` are bounding box *half-sizes*, min are negative, max are positive
//  `vstart` and `vend` are for bounding box center point
//  returns `true` on hit/stuck
//
//==========================================================================
bool TPlane::SweepBox3DEx (const float radius, const float height, const TVec &vstart, const TVec &vend, float *time, TVec *hitpoint) const noexcept {
  const float rad = fabsf(radius);
  const float halfheight = fabsf(height)*0.5f;
  const float bboxext[6] = { -rad, -rad, -halfheight, rad, rad, halfheight };
  const TVec offset = get3DBBoxAcceptPoint(bboxext);
  // adjust the plane distance apropriately
  const float cdist = dist-offset.dot(normal);
  const float idist = vstart.dot(normal)-cdist;
  const float odist = vend.dot(normal)-cdist;
  //GLog.Logf(NAME_Debug, "000: idist=%f; odist=%f", idist, odist);
  if ((idist <= 0.0f && odist <= 0.0f) || // doesn't cross this plane, don't bother
      (idist >= 0.0f && odist >= 0.0f)) // touches, and leaving
  {
    if (time) *time = 1.0f;
    if (hitpoint) *hitpoint = vend;
    return false;
  }
  // negative `idist` means "stuck"
  if (idist < 0.0f) {
    if (time) *time = -1.0f;
    if (hitpoint) *hitpoint = vstart;
    return true;
  }
  // hit time
  const float tt = idist/(idist-odist);
  //GLog.Logf(NAME_Debug, "001: idist=%f; odist=%f; tt=%f", idist, odist, tt);
  // `tt` must not be negative here, but...
  if (tt < 0.0f || tt >= 1.0f) {
    if (time) *time = 1.0f;
    if (hitpoint) *hitpoint = vend;
    return false;
  }
  if (time) *time = tt;
  if (hitpoint) *hitpoint = vstart+(vend-vstart)*tt; //+offset;//k8: offset is not necessary, i believe
  return true;
}


//==========================================================================
//
//  TPlane::SweepBox3D
//
//  box must be valid
//  `time` will be set only if our box hits/penetrates the plane
//  if the box is initially stuck, time will be negative
//  `bbox` are bounding box *coordinates*
//  returns `true` on hit/stuck
//
//==========================================================================
bool TPlane::SweepBox3D (const float bbox[6], const TVec &vdelta, float *time, TVec *hitpoint) const noexcept {
  // calculate center point
  const TVec origin(
    (bbox[BOX3D_MAXX]+bbox[BOX3D_MINX])*0.5f,
    (bbox[BOX3D_MAXY]+bbox[BOX3D_MINY])*0.5f,
    (bbox[BOX3D_MAXZ]+bbox[BOX3D_MINZ])*0.5f);
  // calculate extents
  const TVec ext(
    (bbox[BOX3D_MAXX]-bbox[BOX3D_MINX])*0.5f,
    (bbox[BOX3D_MAXY]-bbox[BOX3D_MINY])*0.5f,
    (bbox[BOX3D_MAXZ]-bbox[BOX3D_MINZ])*0.5f);
  // calculate projection radius
  const float r = fabsf(ext.x*normal.x)+fabsf(ext.y*normal.y)+fabsf(ext.z*normal.z);
  // we're basically doing swept-shere-vs-plane here
  const float odot = normal.dot(origin);
  // check if the sphere is already touching the plane
  if (fabsf(odot-dist) <= r) {
    if (hitpoint) *hitpoint = origin; // arbitrary
    if (time) *time = -1.0f;
    return true;
  }
  const float denom = normal.dot(vdelta);
  if (denom >= 0.0f) {
    // parallel, or moving away
    if (hitpoint) *hitpoint = get3DBBoxAcceptPoint(bbox)+vdelta; // arbitrary
    if (time) *time = 1.0f;
    return false;
  }
  // possibly penetrated
  const float t = (r+dist-odot)/denom;
  if (t < 0.0f || t >= 1.0f) {
    if (hitpoint) *hitpoint = get3DBBoxAcceptPoint(bbox)+vdelta; // arbitrary
    if (time) *time = 1.0f;
    return false;
  }
  if (time) *time = t; // time
  if (hitpoint) *hitpoint = origin+t*vdelta;
  return true;
}


//==========================================================================
//
//  TPlane::checkBox3D
//
//  returns `false` is box is on the back of the plane
//  bbox:
//    [0] is minx
//    [1] is miny
//    [2] is minz
//    [3] is maxx
//    [4] is maxy
//    [5] is maxz
//
//==========================================================================
/*
bool TPlane::checkBox3D (const float bbox[6]) const noexcept {
#ifdef FRUSTUM_BBOX_CHECKS
  vassert(bbox[0] <= bbox[3+0]);
  vassert(bbox[1] <= bbox[3+1]);
  vassert(bbox[2] <= bbox[3+2]);
#endif
#ifdef PLANE_BOX_USE_REJECT_ACCEPT
  // check reject point
  return (DotProduct(normal, get3DBBoxRejectPoint(bbox))-dist > 0.0f); // entire box on a back side?
#else
  CONST_BBoxVertexIndex;
  for (unsigned j = 0; j < 8; ++j) {
    if (!PointOnSide(TVec(bbox[BBoxVertexIndex[j][0]], bbox[BBoxVertexIndex[j][1]], bbox[BBoxVertexIndex[j][2]]))) {
      return true;
    }
  }
  return false;
#endif
}
*/


//==========================================================================
//
//  TPlane::checkBox3DEx
//
//==========================================================================
/*
int TPlane::checkBox3DEx (const float bbox[6]) const noexcept {
#ifdef FRUSTUM_BBOX_CHECKS
  vassert(bbox[0] <= bbox[3+0]);
  vassert(bbox[1] <= bbox[3+1]);
  vassert(bbox[2] <= bbox[3+2]);
#endif
#ifdef PLANE_BOX_USE_REJECT_ACCEPT
  // check reject point
  float d = DotProduct(normal, get3DBBoxRejectPoint(bbox))-dist;
  if (d <= 0.0f) return TFrustum::OUTSIDE; // entire box on a back side
  // check accept point
  d = DotProduct(normal, get3DBBoxAcceptPoint(bbox))-dist;
  return (d < 0.0f ? TFrustum::PARTIALLY : TFrustum::INSIDE); // if accept point on another side (or on plane), assume intersection
#else
  CONST_BBoxVertexIndex;
  unsigned passed = 0;
  for (unsigned j = 0; j < 8; ++j) {
    if (!PointOnSide(TVec(bbox[BBoxVertexIndex[j][0]], bbox[BBoxVertexIndex[j][1]], bbox[BBoxVertexIndex[j][2]]))) {
      ++passed;
      break;
    }
  }
  return (passed ? (passed == 8 ? TFrustum::INSIDE : TFrustum::PARTIALLY) : TFrustum::OUTSIDE);
#endif
}
*/


//==========================================================================
//
//  TPlane::BoxOnPlaneSide
//
//  this is the slow, general version
//
//  0: Quake source says that this can't happen
//  1: in front
//  2: in back
//  3: in both
//
//==========================================================================
unsigned TPlane::BoxOnPlaneSide (const TVec &emins, const TVec &emaxs) const noexcept {
  TVec corners[2];

  // create the proper leading and trailing verts for the box
  for (int i = 0; i < 3; ++i) {
    if (normal[i] < 0) {
      corners[0][i] = emins[i];
      corners[1][i] = emaxs[i];
    } else {
      corners[1][i] = emins[i];
      corners[0][i] = emaxs[i];
    }
  }

  const float dist1 = normal.dot(corners[0])-dist;
  const float dist2 = normal.dot(corners[1])-dist;
  unsigned sides = (unsigned)(dist1 >= 0.0f);
  //if (dist1 >= 0.0f) sides = 1;
  if (dist2 < 0.0f) sides |= 2u;

  return sides;
}


//==========================================================================
//
//  TPlane::get3DBBoxSupportPoint
//
//  box point that is furthest *into* the plane (or closest to it)
//
//==========================================================================
VVA_CHECKRESULT TVec TPlane::get3DBBoxSupportPoint (const float bbox[6], float *pdist) const noexcept {
  float bestDist = +INFINITY;
  TVec bestVec(0.0f, 0.0f, 0.0f);
  //CONST_BBoxVertexIndexFlat;
  //const unsigned *bbp = BBoxVertexIndexFlat;
  for (unsigned bidx = 0; bidx < 8; ++bidx/*, bbp += 3*/) {
    //const TVec bv = TVec(bbox[bbp[0]], bbox[bbp[1]], bbox[bbp[2]]);
    const TVec bv(GetBBox3DCorner(bidx, bbox));
    const float dist = PointDistance(bv);
    if (dist < bestDist) {
      bestDist = dist;
      bestVec = bv;
    }
  }
  if (pdist) *pdist = bestDist;
  return bestVec;
}


//==========================================================================
//
//  TPlane::get2DBBoxSupportPoint
//
//  box point that is furthest *into* the plane (or closest to it)
//
//==========================================================================
VVA_CHECKRESULT TVec TPlane::get2DBBoxSupportPoint (const float bbox2d[4], float *pdist, const float z) const noexcept {
  float bestDist = +INFINITY;
  TVec bestVec(0.0f, 0.0f, 0.0f);
  //CONST_BBox2DVertexIndexFlat;
  //const unsigned *bbp = BBox2DVertexIndexFlat;
  for (unsigned bidx = 0; bidx < 4; ++bidx/*, bbp += 2*/) {
    //const TVec bv = TVec(bbox2d[bbp[0]], bbox2d[bbp[1]], z);
    const TVec bv(GetBBox2DCornerEx(bidx, bbox2d, z));
    const float dist = PointDistance(bv);
    if (dist < bestDist) {
      bestDist = dist;
      bestVec = bv;
    }
  }
  if (pdist) *pdist = bestDist;
  return bestVec;
}


//==========================================================================
//
//  TPlane::ClipPoly
//
//  clip convex polygon to this plane
//  returns number of new vertices (it can be 0 if the poly is completely clipped away)
//  `dest` should have room for at least `vcount+1` vertices, and should not be equal to `src`
//  precondition: vcount >= 3
//  coplanarSide: 0 is front, 1 is back, >1 is both, negative is none
//
//==========================================================================
void TPlane::ClipPoly (ClipWorkData &wdata, const TVec *src, const int vcount,
                       TVec *destfront, int *frontcount, TVec *destback, int *backcount,
                       const int coplanarSide, const float eps) const noexcept
{
  enum {
    PlaneBack = -1,
    PlaneCoplanar = 0,
    PlaneFront = 1,
  };

  if (frontcount) *frontcount = 0;
  if (backcount) *backcount = 0;

  if (vcount < 3) return; // why not?
  vassert(src);
  //vassert(vcount >= 3);

  wdata.ensure(vcount+1);

  int *sides = wdata.sides;
  float *dots = wdata.dots;
  TVec *points = wdata.points;

  // cache values
  const TVec norm = normal;
  const float pdist = dist;

  // classify points
  bool hasFrontSomething = false;
  bool hasBackSomething = false;
  bool hasCoplanarSomething = false;
  for (unsigned i = 0; i < (unsigned)vcount; ++i) {
    points[i] = src[i];
    const float dot = norm.dot(src[i])-pdist;
    dots[i] = dot;
         if (dot < -eps) { sides[i] = PlaneBack; hasBackSomething = true; }
    else if (dot > +eps) { sides[i] = PlaneFront; hasFrontSomething = true; }
    else { sides[i] = PlaneCoplanar; hasCoplanarSomething = true; }
  }

  if (!hasBackSomething && !hasFrontSomething) {
    // totally coplanar
    vassert(hasCoplanarSomething);
    if (coplanarSide == CoplanarFront || coplanarSide == CoplanarBoth) {
      if (frontcount) *frontcount = vcount;
      if (destfront) memcpy((void *)destfront, (void *)points, vcount*sizeof(points[0]));
    }
    if (coplanarSide == CoplanarBack || coplanarSide == CoplanarBoth) {
      if (backcount) *backcount = vcount;
      if (destback) memcpy((void *)destback, (void *)points, vcount*sizeof(points[0]));
    }
    return;
  }

  if (!hasBackSomething) {
    // totally on the front side (possibly touching the plane), copy it
    vassert(hasFrontSomething);
    if (frontcount) *frontcount = vcount;
    if (destfront) memcpy((void *)destfront, (void *)points, vcount*sizeof(points[0]));
    return;
  }

  if (!hasFrontSomething) {
    // totally on the back side (possibly touching the plane), copy it
    vassert(hasBackSomething);
    if (backcount) *backcount = vcount;
    if (destback) memcpy((void *)destback, (void *)points, vcount*sizeof(points[0]));
    return;
  }

  // must be split

  // copy, so we can avoid modulus
  dots[vcount] = dots[0];
  sides[vcount] = sides[0];
  points[vcount] = points[0];

  unsigned fcount = 0;
  unsigned bcount = 0;

  for (unsigned i = 0; i < (unsigned)vcount; ++i) {
    if (sides[i] == PlaneCoplanar) {
      if (destfront) destfront[fcount] = points[i];
      /*fuckyougcc*/ ++fcount; // not a bug
      if (destback) destback[bcount] = points[i];
      /*fuckyougcc*/ ++bcount; // not a bug
      continue;
    }

    if (sides[i] == PlaneFront) {
      if (destfront) destfront[fcount] = points[i];
      /*fuckyougcc*/ ++fcount; // not a bug
    } else {
      if (destback) destback[bcount] = points[i];
      /*fuckyougcc*/ ++bcount; // not a bug
    }

    if (sides[i+1] == PlaneCoplanar || sides[i] == sides[i+1]) continue;

    // generate a split point
    const TVec &p1 = points[i];
    const TVec &p2 = points[i+1];
    const float idist = dots[i]/(dots[i]-dots[i+1]);
    TVec mid;
    for (unsigned j = 0; j < 3; ++j) {
      // avoid roundoff error when possible
           if (norm[j] == +1.0f) mid[j] = +pdist;
      else if (norm[j] == -1.0f) mid[j] = -pdist;
      else mid[j] = p1[j]+idist*(p2[j]-p1[j]);
    }
    if (destfront) destfront[fcount] = mid;
    /*fuckyougcc*/ ++fcount; // not a bug
    if (destback) destback[bcount] = mid;
    /*fuckyougcc*/ ++bcount; // not a bug
  }

  if (frontcount) *frontcount = (int)fcount;
  if (backcount) *backcount = (int)bcount;
}
