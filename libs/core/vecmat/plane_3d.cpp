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
//  TPlane::BoxOnPlaneSide
//
//  this is the slow, general version
//
//  1: in front
//  2: in back
//  3: in both
//
//==========================================================================
unsigned TPlane::BoxOnPlaneSide (const TVec &emins, const TVec &emaxs) const noexcept {
  TVec corners[2];

  // create the proper leading and trailing verts for the box
  for (int i = 0; i < 3; ++i) {
    if (normal[i] < 0.0f) {
      corners[0][i] = emins[i];
      corners[1][i] = emaxs[i];
    } else {
      corners[1][i] = emins[i];
      corners[0][i] = emaxs[i];
    }
  }

  const float dist1 = PointDistance(corners[0]);
  const float dist2 = PointDistance(corners[1]);

  unsigned sides = (unsigned)(dist1 >= 0.0f);
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
  for (unsigned bidx = 0; bidx < MAX_BBOX3D_CORNERS; ++bidx) {
    const TVec bv(GetBBox3DCorner(bidx, bbox));
    const float dist = PointDistance(bv);
    if (dist < bestDist) {
      bestDist = dist;
      bestVec = bv;
    }
  }
  if (pdist) *pdist = zeroDenormalsF(bestDist);
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
  for (unsigned bidx = 0; bidx < MAX_BBOX2D_CORNERS; ++bidx) {
    const TVec bv(GetBBox2DCornerEx(bidx, bbox2d, z));
    const float dist = PointDistance(bv);
    if (dist < bestDist) {
      bestDist = dist;
      bestVec = bv;
    }
  }
  if (pdist) *pdist = zeroDenormalsF(bestDist);
  return bestVec;
}


//==========================================================================
//
//  TPlane::get3DBBoxAntiSupportPoint
//
//  box point that is furthest *out* *of* the plane (or closest to it)
//
//==========================================================================
VVA_CHECKRESULT TVec TPlane::get3DBBoxAntiSupportPoint (const float bbox[6], float *pdist) const noexcept {
  float bestDist = -INFINITY;
  TVec bestVec(0.0f, 0.0f, 0.0f);
  for (unsigned bidx = 0; bidx < MAX_BBOX3D_CORNERS; ++bidx) {
    const TVec bv(GetBBox3DCorner(bidx, bbox));
    const float dist = PointDistance(bv);
    if (dist > bestDist) {
      bestDist = dist;
      bestVec = bv;
    }
  }
  if (pdist) *pdist = zeroDenormalsF(bestDist);
  return bestVec;
}


//==========================================================================
//
//  TPlane::get2DBBoxAntiSupportPoint
//
//  box point that is furthest *out* *of* the plane (or closest to it)
//
//==========================================================================
VVA_CHECKRESULT TVec TPlane::get2DBBoxAntiSupportPoint (const float bbox2d[4], float *pdist, const float z) const noexcept {
  float bestDist = -INFINITY;
  TVec bestVec(0.0f, 0.0f, 0.0f);
  for (unsigned bidx = 0; bidx < MAX_BBOX2D_CORNERS; ++bidx) {
    const TVec bv(GetBBox2DCornerEx(bidx, bbox2d, z));
    const float dist = PointDistance(bv);
    if (dist > bestDist) {
      bestDist = dist;
      bestVec = bv;
    }
  }
  if (pdist) *pdist = zeroDenormalsF(bestDist);
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

  wdata.ensure(vcount+1);

  int *sides = wdata.sides;
  float *dots = wdata.dots;
  TVec *points = wdata.points;

  // for better memory access locality
  memcpy((void *)points, (void *)src, vcount*sizeof(points[0]));

  // cache values
  const TVec norm = normal;
  const float pdist = dist;

  // classify points
  bool hasFrontSomething = false;
  bool hasBackSomething = false;
  bool hasCoplanarSomething = false;
  for (unsigned i = 0; i < (unsigned)vcount; ++i) {
    const float dot = (dots[i] = norm.dot(points[i])-pdist);
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
    } else {
      if (sides[i] == PlaneFront) {
        if (destfront) destfront[fcount] = points[i];
        /*fuckyougcc*/ ++fcount; // not a bug
      } else {
        if (destback) destback[bcount] = points[i];
        /*fuckyougcc*/ ++bcount; // not a bug
      }

      if (!(sides[i+1] == PlaneCoplanar || sides[i] == sides[i+1])) {
        // generate a split point
        TVec mid;
        const TVec &p1 = points[i];
        const TVec &p2 = points[i+1];
        float idist = dots[i] - dots[i+1];
        if (idist == 0.0f) {
          mid = p1;
        } else {
          idist = dots[i] / idist;

          // avoid roundoff error when possible
          if (norm.x == +1.0f) mid.x = +pdist;
          else if (norm.x == -1.0f) mid.x = -pdist;
          else mid.x = p1.x+idist*(p2.x-p1.x);

          if (norm.y == +1.0f) mid.y = +pdist;
          else if (norm.y == -1.0f) mid.y = -pdist;
          else mid.y = p1.y+idist*(p2.y-p1.y);

          if (norm.z == +1.0f) mid.z = +pdist;
          else if (norm.z == -1.0f) mid.z = -pdist;
          else mid.z = p1.z+idist*(p2.z-p1.z);
        }
        mid.fixDenormalsInPlace();

        if (destfront) destfront[fcount] = mid;
        /*fuckyougcc*/ ++fcount; // not a bug
        if (destback) destback[bcount] = mid;
        /*fuckyougcc*/ ++bcount; // not a bug
      }
    }
  }

  if (frontcount) *frontcount = (int)fcount;
  if (backcount) *backcount = (int)bcount;
}


//==========================================================================
//
//  TPlane::ClipPolyFront
//
//  clip convex polygon to this plane
//  returns number of new vertices (it can be 0 if the poly is completely clipped away)
//  `dest` should have room for at least `vcount+1` vertices, and should not be equal to `src`
//  precondition: vcount >= 3
//
//  leaves only front part; coplanar polys goes to front
//
//==========================================================================
void TPlane::ClipPolyFront (ClipWorkData &wdata, const TVec *src, const int vcount,
                            TVec *dest, int *destcount, const float eps) const noexcept
{
  enum {
    PlaneBack = -1,
    PlaneCoplanar = 0,
    PlaneFront = 1,
  };

  vassert(destcount);

  if (vcount < 3) {
    *destcount = 0;
    return;
  }

  vassert(src);
  vassert(dest);

  wdata.ensure(vcount+1);

  int *sides = wdata.sides;
  float *dots = wdata.dots;
  TVec *points = wdata.points;

  // for better memory access locality
  memcpy((void *)points, (void *)src, vcount*sizeof(points[0]));

  // cache values
  const TVec norm = normal;
  const float pdist = dist;

  // classify points
  bool hasFrontSomething = false;
  bool hasBackSomething = false;
  bool hasCoplanarSomething = false;
  for (unsigned i = 0; i < (unsigned)vcount; ++i) {
    const float dot = (dots[i] = norm.dot(points[i])-pdist);
         if (dot < -eps) { sides[i] = PlaneBack; hasBackSomething = true; }
    else if (dot > +eps) { sides[i] = PlaneFront; hasFrontSomething = true; }
    else { sides[i] = PlaneCoplanar; hasCoplanarSomething = true; }
  }

  if (!hasFrontSomething) {
    // nothing at the front, but may be totally coplanar
    if (!hasBackSomething) {
      // totally coplanar
      vassert(hasCoplanarSomething);
      *destcount = vcount;
      if ((void *)dest != (void *)src) {
        memcpy((void *)dest, (void *)points, vcount*sizeof(points[0]));
      }
    } else {
      // nothing at the front
      *destcount = 0;
    }
    return;
  } else if (!hasBackSomething) {
    // totally on the front side (possibly with coplanars), copy it
    *destcount = vcount;
    if ((void *)dest != (void *)src) {
      memcpy((void *)dest, (void *)points, vcount*sizeof(points[0]));
    }
    return;
  }

  // must be split

  // copy, so we can avoid modulus
  dots[vcount] = dots[0];
  sides[vcount] = sides[0];
  points[vcount] = points[0];

  int fcount = 0;

  for (int i = 0; i < vcount; ++i) {
    if (sides[i] == PlaneCoplanar) {
      dest[fcount++] = points[i];
    } else {
      if (sides[i] == PlaneFront) {
        dest[fcount++] = points[i];
      }

      if (!(sides[i + 1] == PlaneCoplanar || sides[i] == sides[i + 1])) {
        // generate a split point
        TVec mid;
        const TVec &p1 = points[i];
        const TVec &p2 = points[i+1];
        float idist = dots[i] - dots[i+1];
        if (idist == 0.0f) {
          mid = p1;
        } else {
          idist = dots[i] / idist;

          // avoid roundoff error when possible
          if (norm.x == +1.0f) mid.x = +pdist;
          else if (norm.x == -1.0f) mid.x = -pdist;
          else mid.x = p1.x+idist*(p2.x-p1.x);

          if (norm.y == +1.0f) mid.y = +pdist;
          else if (norm.y == -1.0f) mid.y = -pdist;
          else mid.y = p1.y+idist*(p2.y-p1.y);

          if (p1.z != p2.z) {
            if (norm.z == +1.0f) mid.z = +pdist;
            else if (norm.z == -1.0f) mid.z = -pdist;
            else mid.z = p1.z+idist*(p2.z-p1.z);
          } else {
            mid.z = p1.z;
          }
        }

        mid.fixDenormalsInPlace();
        dest[fcount++] = mid;
      }
    }
  }

  vassert(fcount <= vcount+1);

  *destcount = fcount;
}
