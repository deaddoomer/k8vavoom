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

// debug checks
//#define FRUSTUM_BBOX_CHECKS
#define PLANE_BOX_USE_REJECT_ACCEPT


const TVec TVec::ZeroVector = TVec(0.0f, 0.0f, 0.0f);


//==========================================================================
//
//  AngleVectors
//
//==========================================================================
void AngleVectors (const TAVec &angles, TVec &forward, TVec &right, TVec &up) noexcept {
  /*
  const float ay = DEG2RADF(angles.yaw);
  const float ap = DEG2RADF(angles.pitch);
  const float ar = DEG2RADF(angles.roll);

  const float sy = sinf(ay);
  const float cy = cosf(ay);
  const float sp = sinf(ap);
  const float cp = cosf(ap);
  const float sr = sinf(ar);
  const float cr = cosf(ar);
  */

  float sy, cy;
  float sp, cp;
  float sr, cr;

  msincos(angles.yaw, &sy, &cy);
  if (angles.pitch || angles.roll) {
    msincos(angles.pitch, &sp, &cp);
    if (angles.roll) {
      msincos(angles.roll, &sr, &cr);

      forward.x = cp*cy;
      forward.y = cp*sy;
      forward.z = -sp;
      right.x = VSUM2(-sr*sp*cy, cr*sy);
      right.y = VSUM2(-sr*sp*sy, -(cr*cy));
      right.z = -sr*cp;
      up.x = VSUM2(cr*sp*cy, sr*sy);
      up.y = VSUM2(cr*sp*sy, -(sr*cy));
      up.z = cr*cp;
    } else {
      // no roll
      forward.x = cp*cy;
      forward.y = cp*sy;
      forward.z = -sp;
      right.x = sy;
      right.y = -cy;
      right.z = 0.0f;
      up.x = sp*cy;
      up.y = sp*sy;
      up.z = cp;
    }
  } else {
    // no pitch, no roll
    forward.x = cy;
    forward.y = sy;
    forward.z = 0.0f;
    right.x = sy;
    right.y = -cy;
    right.z = 0.0f;
    up.x = 0.0f;
    up.y = 0.0f;
    up.z = 1.0f;
  }
}


//==========================================================================
//
//  AngleRightVector
//
//==========================================================================
void AngleRightVector (const TAVec &angles, TVec &right) noexcept {
  float sy, cy;
  msincos(angles.yaw, &sy, &cy);
  if (angles.pitch) {
    if (angles.roll) {
      float sp, cp;
      float sr, cr;
      msincos(angles.pitch, &sp, &cp);
      msincos(angles.roll, &sr, &cr);

      right.x = VSUM2(-sr*sp*cy, cr*sy);
      right.y = VSUM2(-sr*sp*sy, -(cr*cy));
      right.z = -sr*cp;
    } else {
      // no roll
      right.x = sy;
      right.y = -cy;
      right.z = 0.0f;
    }
  } else {
    // no pitch, no roll
    right.x = sy;
    right.y = -cy;
    right.z = 0.0f;
  }
}


//==========================================================================
//
//  YawVectorRight
//
//==========================================================================
void YawVectorRight (float yaw, TVec &right) noexcept {
  /*
  float sy, cy;
  msincos(yaw, &sy, &cy);
  right.x = sy;
  right.y = -cy;
  */
  msincos(yaw, &right.x, &right.y);
  right.y = -right.y;
  right.z = 0.0f;
}


//==========================================================================
//
//  AngleVector
//
//==========================================================================
void AngleVector (const TAVec &angles, TVec &forward) noexcept {
  /*
  const float ay = DEG2RADF(angles.yaw);
  const float ap = DEG2RADF(angles.pitch);

  const float sy = sinf(ay);
  const float cy = cosf(ay);
  const float sp = sinf(ap);
  const float cp = cosf(ap);
  */
  if (angles.pitch) {
    float sy, cy;
    float sp, cp;
    msincos(angles.yaw, &sy, &cy);
    msincos(angles.pitch, &sp, &cp);

    forward.x = cp*cy;
    forward.y = cp*sy;
    forward.z = -sp;
  } else {
    // no pitch
    msincos(angles.yaw, &forward.y, &forward.x);
    forward.z = 0.0f;
  }
}


//==========================================================================
//
//  VectorAngles
//
//==========================================================================
void VectorAngles (const TVec &vec, TAVec &angles) noexcept {
  const float fx = vec.x;
  const float fy = vec.y;
  const float len2d = VSUM2(fx*fx, fy*fy);
  if (len2d < 0.0001f) {
    angles.pitch = (vec.z > 0.0f ? 90 : 270);
    angles.yaw = 0.0f;
  } else {
    angles.pitch = -matan(vec.z, sqrtf(len2d));
    angles.yaw = matan(fy, fx);
  }
  angles.roll = 0.0f;
}


//==========================================================================
//
//  VectorsAngles
//
//==========================================================================
void VectorsAngles (const TVec &forward, const TVec &right, const TVec &up, TAVec &angles) noexcept {
  const float fx = forward.x;
  const float fy = forward.y;
  float len2d = VSUM2(fx*fx, fy*fy);
  if (len2d < 0.0001f) {
    angles.yaw = 0.0f;
    if (forward.z > 0.0f) {
      angles.pitch = 90;
      angles.roll = matan(-up.y, -up.x);
    } else {
      angles.pitch = 270;
      angles.roll = matan(-up.y, up.x);
    }
  } else {
    len2d = sqrtf(len2d);
    angles.pitch = matan(-forward.z, len2d); // up/down
    angles.yaw = matan(fy, fx); // left/right
    angles.roll = (right.z || up.z ? matan(-right.z/len2d, up.z/len2d) : 0.0f);
  }
}


//==========================================================================
//
//  PerpendicularVector
//
//  assumes "src" is normalised
//
//==========================================================================
void PerpendicularVector (TVec &dst, const TVec &src) noexcept {
  unsigned pos;
  unsigned i;
  float minelem = 1.0f;
  TVec tempvec;

  // find the smallest magnitude axially aligned vector
  for (pos = 0, i = 0; i < 3; ++i) {
    if (fabsf(src[i]) < minelem) {
      pos = i;
      minelem = fabsf(src[i]);
    }
  }
  tempvec[0] = tempvec[1] = tempvec[2] = 0.0f;
  tempvec[pos] = 1.0f;

  // project the point onto the plane defined by src
  if (ProjectPointOnPlane(dst, tempvec, src)) {
    // normalise the result
    //dst = NormaliseSafe(dst);
    dst.normaliseInPlace();
  }
}


//==========================================================================
//
//  RotateVectorAroundVector
//
//==========================================================================
VVA_CHECKRESULT TVec RotateVectorAroundVector (const TVec &Vector, const TVec &Axis, float Angle) noexcept {
  VRotMatrix M(Axis, Angle);
  return Vector*M;
}


//==========================================================================
//
//  MatrixMultiply
//
//==========================================================================
static void MatrixMultiply (const float in1[3][3], const float in2[3][3], float out[3][3]) noexcept {
  out[0][0] = in1[0][0]*in2[0][0]+in1[0][1]*in2[1][0]+in1[0][2]*in2[2][0];
  out[0][1] = in1[0][0]*in2[0][1]+in1[0][1]*in2[1][1]+in1[0][2]*in2[2][1];
  out[0][2] = in1[0][0]*in2[0][2]+in1[0][1]*in2[1][2]+in1[0][2]*in2[2][2];
  out[1][0] = in1[1][0]*in2[0][0]+in1[1][1]*in2[1][0]+in1[1][2]*in2[2][0];
  out[1][1] = in1[1][0]*in2[0][1]+in1[1][1]*in2[1][1]+in1[1][2]*in2[2][1];
  out[1][2] = in1[1][0]*in2[0][2]+in1[1][1]*in2[1][2]+in1[1][2]*in2[2][2];
  out[2][0] = in1[2][0]*in2[0][0]+in1[2][1]*in2[1][0]+in1[2][2]*in2[2][0];
  out[2][1] = in1[2][0]*in2[0][1]+in1[2][1]*in2[1][1]+in1[2][2]*in2[2][1];
  out[2][2] = in1[2][0]*in2[0][2]+in1[2][1]*in2[1][2]+in1[2][2]*in2[2][2];
}


//==========================================================================
//
//  RotatePointAroundVector
//
//  This is not implemented very well...
//
//==========================================================================
void RotatePointAroundVector (TVec &dst, const TVec &dir, const TVec &point, float degrees) noexcept {
  float m[3][3];
  float im[3][3];
  float zrot[3][3];
  float tmpmat[3][3];
  float rot[3][3];
  TVec vr, vup, vf;

  vf[0] = dir[0];
  vf[1] = dir[1];
  vf[2] = dir[2];

  PerpendicularVector(vr, dir);
  vup = CrossProduct(vr, vf);

  m[0][0] = vr[0];
  m[1][0] = vr[1];
  m[2][0] = vr[2];

  m[0][1] = vup[0];
  m[1][1] = vup[1];
  m[2][1] = vup[2];

  m[0][2] = vf[0];
  m[1][2] = vf[1];
  m[2][2] = vf[2];

  memcpy(im, m, sizeof(im));

  im[0][1] = m[1][0];
  im[0][2] = m[2][0];
  im[1][0] = m[0][1];
  im[1][2] = m[2][1];
  im[2][0] = m[0][2];
  im[2][1] = m[1][2];

  memset(zrot, 0, sizeof(zrot));
  zrot[0][0] = zrot[1][1] = zrot[2][2] = 1.0f;

  float s, c;
  msincos(degrees, &s, &c);
  zrot[0][0] = c;
  zrot[0][1] = s;
  zrot[1][0] = -s;
  zrot[1][1] = c;

  MatrixMultiply(m, zrot, tmpmat);
  MatrixMultiply(tmpmat, im, rot);

  for (unsigned i = 0; i < 3; ++i) dst[i] = rot[i][0]*point[0]+rot[i][1]*point[1]+rot[i][2]*point[2];
}


//==========================================================================
//
//  RotateAroundDirection
//
//==========================================================================
void RotateAroundDirection (TVec axis[3], float yaw) noexcept {
  // create an arbitrary axis[1]
  PerpendicularVector(axis[1], axis[0]);
  // rotate it around axis[0] by yaw
  if (yaw) {
    TVec temp = axis[1];
    RotatePointAroundVector(axis[1], axis[0], temp, yaw);
  }
  // cross to get axis[2]
  axis[2] = CrossProduct(axis[0], axis[1]);
}


//==========================================================================
//
//  MakeNormalVectors
//
//  given a normalized forward vector, create two
//  other perpendicular vectors
//
//==========================================================================
void MakeNormalVectors (const TVec &forward, TVec &right, TVec &up) noexcept {
  // this rotate and negate guarantees a vector not colinear with the original
  right[1] = -forward[0];
  right[2] = forward[1];
  right[0] = forward[2];
  float d = DotProduct(right, forward);
  //VectorMA(right, -d, forward, right);
  // (const vec3_t veca, float scale, const vec3_t vecb, vec3_t vecc): vecc = veca+scal*vecb
  right -= forward*d;
  right.normaliseInPlace();
  up = CrossProduct(right, forward);
}



//==========================================================================
//
//  TPlane::checkBox
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
bool TPlane::checkBox (const float bbox[6]) const noexcept {
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
//  TPlane::checkBoxEx
//
//==========================================================================
/*
int TPlane::checkBoxEx (const float bbox[6]) const noexcept {
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

  const float dist1 = DotProduct(normal, corners[0])-dist;
  const float dist2 = DotProduct(normal, corners[1])-dist;
  unsigned sides = (unsigned)(dist1 >= 0);
  //if (dist1 >= 0) sides = 1;
  if (dist2 < 0) sides |= 2u;

  return sides;
}


//==========================================================================
//
//  TPlane::ClipPoly
//
//  clip convex polygon to this plane
//  returns number of new vertices (it can be 0 if the poly is completely clipped away)
//  `dest` should have room for at least `vcount+1` vertices, and should not be equal to `src`
//  precondition: vcount >= 3
//
//==========================================================================
int TPlane::ClipPoly (ClipWorkData &wdata, TVec *dest, const TVec *src, int vcount, const float eps) const noexcept {
  enum {
    PlaneBack = -1,
    PlaneCoplanar = 0,
    PlaneFront = 1,
  };

  if (vcount < 3) return 0; // why not?
  vassert(dest);
  vassert(src);
  vassert(dest != src);
  //vassert(vcount >= 3);

  wdata.ensure(vcount+1);

  int *sides = wdata.sides;
  float *dots = wdata.dots;

  // cache values
  const TVec norm = this->normal;
  const float pdst = this->dist;

  // determine sides for each point
  bool hasFrontSomething = false;
  for (unsigned i = 0; i < (unsigned)vcount; ++i) {
    const float dot = DotProduct(src[i], norm)-pdst;
    dots[i] = dot;
         if (dot < -eps) sides[i] = PlaneBack;
    else if (dot > eps) { sides[i] = PlaneFront; hasFrontSomething = true; }
    else sides[i] = PlaneCoplanar;
  }

  if (!hasFrontSomething) return 0; // completely clipped away

  dots[vcount] = dots[0];
  sides[vcount] = sides[0];

  unsigned dcount = 0;

  for (unsigned i = 0; i < (unsigned)vcount; ++i) {
    if (sides[i] == PlaneCoplanar) {
      dest[dcount++] = src[i];
      continue;
    }
    if (sides[i] == PlaneFront) dest[dcount++] = src[i];
    if (sides[i+1] == PlaneCoplanar || sides[i] == sides[i+1]) continue;
    // generate a split point
    const TVec &p1 = src[i];
    const TVec &p2 = src[(i+1u)%(unsigned)vcount];
    const float idist = dots[i]/(dots[i]-dots[i+1]);
    TVec &mid = dest[dcount++];
    for (unsigned j = 0; j < 3; ++j) {
      // avoid roundoff error when possible
           if (norm[j] == 1) mid[j] = pdst;
      else if (norm[j] == -1) mid[j] = -pdst;
      else mid[j] = p1[j]+idist*(p2[j]-p1[j]);
    }
  }

  return (int)dcount;
}



//==========================================================================
//
//  TClipBase::setupFromFOVs
//
//==========================================================================
void TClipBase::setupFromFOVs (const float afovx, const float afovy) noexcept {
  if (afovx == 0.0f || afovy == 0.0f || !isFiniteF(afovx) || !isFiniteF(afovy)) {
    clear();
  } else {
    fovx = afovx;
    fovy = afovy;
    const float invfovx = 1.0f/afovx;
    const float invfovy = 1.0f/afovy;
    clipbase[0] = TVec(invfovx, 0.0f, 1.0f); // left side clip
    clipbase[1] = TVec(-invfovx, 0.0f, 1.0f); // right side clip
    clipbase[2] = TVec(0.0f, -invfovy, 1.0f); // top side clip
    clipbase[3] = TVec(0.0f, invfovy, 1.0f); // bottom side clip
  }
}


//==========================================================================
//
//  TClipBase::setupViewport
//
//==========================================================================
void TClipBase::setupViewport (int awidth, int aheight, float afov, float apixelAspect) noexcept {
  if (awidth < 1 || aheight < 1 || afov <= 0.01f || apixelAspect <= 0) {
    clear();
    return;
  }
  float afovx, afovy;
  CalcFovXY(&afovx, &afovy, awidth, aheight, afov, apixelAspect);
  setupFromFOVs(afovx, afovy);
}


//==========================================================================
//
//  TClipBase::setupViewport
//
//==========================================================================
void TClipBase::setupViewport (const TClipParam &cp) noexcept {
  if (!cp.isValid()) {
    clear();
    return;
  }
  float afovx, afovy;
  CalcFovXY(&afovx, &afovy, cp);
  setupFromFOVs(afovx, afovy);
}



//==========================================================================
//
//  TFrustum::setup
//
//  `clip_base` is from engine's `SetupFrame()` or `SetupCameraFrame()`
//
//==========================================================================
void TFrustum::setup (const TClipBase &clipbase, const TFrustumParam &fp, bool createbackplane, const float farplanez) noexcept {
  clear();
  if (!clipbase.isValid() || !fp.isValid()) return;
  planeCount = 4; // anyway
  // create side planes
  for (unsigned i = 0; i < 4; ++i) {
    const TVec &v = clipbase.clipbase[i];
    // v.z is always 1.0f
    const TVec v2(
      VSUM3(v.x*fp.vright.x, v.y*fp.vup.x, /*v.z* */fp.vforward.x),
      VSUM3(v.x*fp.vright.y, v.y*fp.vup.y, /*v.z* */fp.vforward.y),
      VSUM3(v.x*fp.vright.z, v.y*fp.vup.z, /*v.z* */fp.vforward.z));
    planes[i].SetPointNormal3D(fp.origin, v2.normalise());
    planes[i].clipflag = 1U<<i;
  }
  // create back plane
  if (createbackplane) {
    // move back plane forward a little
    // it should be moved to match projection, but our line tracing code
    // is using `0.1f` tolerance, and it should be lower than that
    //planes[4].SetPointNormal3D(fp.origin/*+fp.vforward*0.02f*/, fp.vforward);
    // disregard that; our `zNear` is `1.0f`, so just use it
    // k8: better be safe than sorry
    planes[4].SetPointNormal3D(fp.origin+fp.vforward*0.125f, fp.vforward);
    // sanity check: camera shouldn't be in frustum
    //vassert(planes[4].PointOnSide(fp.origin));
    planes[4].clipflag = 1U<<4;
    planeCount = 5;
  } else {
    planes[4].clipflag = 0;
  }
  // create far plane
  if (isFiniteF(farplanez) && farplanez > 0) {
    planes[5].SetPointNormal3D(fp.origin+fp.vforward*farplanez, -fp.vforward);
    planes[5].clipflag = 1U<<5;
    planeCount = 6;
  } else {
    planes[5].clipflag = 0;
  }
}


//==========================================================================
//
//  TFrustum::checkPoint
//
//==========================================================================
void TFrustum::setFarPlane (const TFrustumParam &fp, float farplanez) noexcept {
  // create far plane
  if (isFiniteF(farplanez) && farplanez > 0) {
    planes[5].SetPointNormal3D(fp.origin+fp.vforward*farplanez, -fp.vforward);
    planes[5].clipflag = 1U<<5;
    planeCount = 6;
  } else {
    planes[5].clipflag = 0;
    if (planeCount >= 6) planeCount = 5;
  }
}


//==========================================================================
//
//  TFrustum::checkPoint
//
//  returns `false` is sphere is out of frustum (or frustum is not valid)
//
//==========================================================================
bool TFrustum::checkPoint (const TVec &point, const unsigned mask) const noexcept {
  if (!planeCount || !mask) return true;
  const TClipPlane *cp = &planes[0];
  for (unsigned i = planeCount; i--; ++cp) {
    if (!(cp->clipflag&mask)) continue; // don't need to clip against it
    if (cp->PointOnSide(point)) return false; // viewer is in back side or on plane
  }
  return true;
}


//==========================================================================
//
//  TFrustum::checkSphere
//
//  returns `false` is sphere is out of frustum (or frustum is not valid)
//  note that this can give us false positives, see
//  https://stackoverflow.com/questions/37512308/
//
//==========================================================================
bool TFrustum::checkSphere (const TVec &center, const float radius, const unsigned mask) const noexcept {
  if (!planeCount || !mask) return true;
  if (radius <= 0) return checkPoint(center, mask);
  const TClipPlane *cp = &planes[0];
  for (unsigned i = planeCount; i--; ++cp) {
    if (!(cp->clipflag&mask)) continue; // don't need to clip against it
    if (cp->SphereOnSide(center, radius)) {
      // on a back side (or on a plane)
      return false;
    }
  }
  return true;
}


//==========================================================================
//
//  TFrustum::checkBox
//
//  returns `false` is box is out of frustum (or frustum is not valid)
//  bbox:
//    [0] is minx
//    [1] is miny
//    [2] is minz
//    [3] is maxx
//    [4] is maxy
//    [5] is maxz
//
//==========================================================================
bool TFrustum::checkBox (const float bbox[6], const unsigned mask) const noexcept {
  if (!planeCount || !mask) return true;
#ifdef FRUSTUM_BBOX_CHECKS
  vassert(bbox[0] <= bbox[3+0]);
  vassert(bbox[1] <= bbox[3+1]);
  vassert(bbox[2] <= bbox[3+2]);
#endif
#ifndef PLANE_BOX_USE_REJECT_ACCEPT
  CONST_BBoxVertexIndex;
#endif
  const TClipPlane *cp = &planes[0];
  for (unsigned i = planeCount; i--; ++cp) {
    if (!(cp->clipflag&mask)) continue; // don't need to clip against it
#ifdef PLANE_BOX_USE_REJECT_ACCEPT
    if (DotProduct(cp->normal, cp->get3DBBoxRejectPoint(bbox))-cp->dist <= 0.0f) return false; // on a back side?
#else
    bool passed = false;
    for (unsigned j = 0; j < 8; ++j) {
      if (!cp->PointOnSide(TVec(bbox[BBoxVertexIndex[j][0]], bbox[BBoxVertexIndex[j][1]], bbox[BBoxVertexIndex[j][2]]))) {
        passed = true;
        break;
      }
    }
    if (!passed) return false;
#endif
  }
  return true;
}


//==========================================================================
//
//  TFrustum::checkBoxEx
//
//  0: completely outside; >0: completely inside; <0: partially inside
//  note that this won't work for big boxes: we need to do more checks, see
//  http://iquilezles.org/www/articles/frustumcorrect/frustumcorrect.htm
//
//==========================================================================
int TFrustum::checkBoxEx (const float bbox[6], const unsigned mask) const noexcept {
  if (!planeCount || !mask) return INSIDE;
#ifdef FRUSTUM_BBOX_CHECKS
  vassert(bbox[0] <= bbox[3+0]);
  vassert(bbox[1] <= bbox[3+1]);
  vassert(bbox[2] <= bbox[3+2]);
#endif
#ifndef PLANE_BOX_USE_REJECT_ACCEPT
  CONST_BBoxVertexIndex;
#endif
  int res = INSIDE; // assume that the aabb will be inside the frustum
  const TClipPlane *cp = &planes[0];
  for (unsigned i = planeCount; i--; ++cp) {
    if (!(cp->clipflag&mask)) continue; // don't need to clip against it
#ifdef PLANE_BOX_USE_REJECT_ACCEPT
    // check reject point
    float d = DotProduct(cp->normal, cp->get3DBBoxRejectPoint(bbox))-cp->dist;
    if (d <= 0.0f) return OUTSIDE; // entire box on a back side
    // if the box is already determined to be partially inside, there is no need to check accept point anymore
    if (res == INSIDE) {
      // check accept point
      d = DotProduct(cp->normal, cp->get3DBBoxAcceptPoint(bbox))-cp->dist;
      if (d < 0.0f) res = PARTIALLY;
    }
#else
    if (res == INSIDE) {
      unsigned passed = 0;
      for (unsigned j = 0; j < 8; ++j) {
        if (!cp->PointOnSide(TVec(bbox[BBoxVertexIndex[j][0]], bbox[BBoxVertexIndex[j][1]], bbox[BBoxVertexIndex[j][2]]))) {
          ++passed;
        }
      }
      if (!passed) return OUTSIDE;
      if (passed != 8) res = PARTIALLY;
    } else {
      // partially
      bool passed = false;
      for (unsigned j = 0; j < 8; ++j) {
        if (!cp->PointOnSide(TVec(bbox[BBoxVertexIndex[j][0]], bbox[BBoxVertexIndex[j][1]], bbox[BBoxVertexIndex[j][2]]))) {
          passed = true;
          break;
        }
      }
      if (!passed) return OUTSIDE;
    }
#endif
  }
  return res;
}


//==========================================================================
//
//  TFrustum::checkVerts
//
//==========================================================================
bool TFrustum::checkVerts (const TVec *verts, const unsigned vcount, const unsigned mask) const noexcept {
  if (!planeCount || !mask || !vcount) return true;
  const TClipPlane *cp = &planes[0];
  for (unsigned i = planeCount; i--; ++cp) {
    if (!(cp->clipflag&mask)) continue; // don't need to clip against it
    bool passed = false;
    for (unsigned j = 0; j < vcount; ++j) {
      if (!cp->PointOnSide(verts[j])) {
        passed = true;
        break;
      }
    }
    if (!passed) return false;
  }
  return true;
}


//==========================================================================
//
//  TFrustum::checkVertsEx
//
//==========================================================================
int TFrustum::checkVertsEx (const TVec *verts, const unsigned vcount, const unsigned mask) const noexcept {
  if (!planeCount || !mask || !vcount) return true;
  int res = INSIDE; // assume that the aabb will be inside the frustum
  const TClipPlane *cp = &planes[0];
  for (unsigned i = planeCount; i--; ++cp) {
    if (!(cp->clipflag&mask)) continue; // don't need to clip against it
    if (res == INSIDE) {
      unsigned passed = 0;
      for (unsigned j = 0; j < vcount; ++j) {
        if (!cp->PointOnSide(verts[j])) {
          ++passed;
        }
      }
      if (!passed) return OUTSIDE;
      if (passed != vcount) res = PARTIALLY;
    } else {
      // partially
      bool passed = false;
      for (unsigned j = 0; j < vcount; ++j) {
        if (!cp->PointOnSide(verts[j])) {
          passed = true;
          break;
        }
      }
      if (!passed) return OUTSIDE;
    }
  }
  return res;
}


//==========================================================================
//
//  TFrustum::checkQuadEx
//
//==========================================================================
int TFrustum::checkQuadEx (const TVec &v1, const TVec &v2, const TVec &v3, const TVec &v4, const unsigned mask) const noexcept {
  if (!planeCount || !mask) return true;
  int res = INSIDE;
  const TClipPlane *cp = &planes[0];
  for (unsigned i = planeCount; i--; ++cp) {
    if (!(cp->clipflag&mask)) continue; // don't need to clip against it
    float d1 = DotProduct(cp->normal, v1)-cp->dist;
    float d2 = DotProduct(cp->normal, v2)-cp->dist;
    float d3 = DotProduct(cp->normal, v3)-cp->dist;
    float d4 = DotProduct(cp->normal, v4)-cp->dist;
    // if everything is on the back side, we're done here
    if (d1 < 0 && d2 < 0 && d3 < 0 && d4 < 0) return OUTSIDE;
    // if we're already hit "partial" case, no need to perform further checks
    if (res == INSIDE) {
      // if everything on the front side, go on
      if (d1 >= 0 && d2 >= 0 && d3 >= 0 && d4 >= 0) continue;
      // both sides touched
      res = PARTIALLY;
    }
  }
  return res;
}



//==========================================================================
//
//  P_BoxOnLineSide
//
//  check the relationship between the given box and the partition
//  line.  Returns -1 if box is on left side, +1 if box is on right
//  size, or 0 if the line intersects the box.
//
//==========================================================================
int BoxOnLineSide2D (const float tmbox[4], TVec v1, TVec v2) noexcept {
  v1.z = v2.z = 0;
  TVec dir = v2-v1;

  int p1, p2;

  if (!dir.y) {
    // horizontal
    p1 = (tmbox[BOX2D_TOP] > v1.y);
    p2 = (tmbox[BOX2D_BOTTOM] > v1.y);
    if (dir.x < 0) {
      p1 ^= 1;
      p2 ^= 1;
    }
  } else if (!dir.x) {
    // vertical
    p1 = (tmbox[BOX2D_RIGHT] < v1.x);
    p2 = (tmbox[BOX2D_LEFT] < v1.x);
    if (dir.y < 0) {
      p1 ^= 1;
      p2 ^= 1;
    }
  } else if (dir.y/dir.x > 0) {
    TPlane lpl;
    lpl.SetPointDirXY(v1, dir);
    // positive
    p1 = lpl.PointOnSide(TVec(tmbox[BOX2D_LEFT], tmbox[BOX2D_TOP], 0));
    p2 = lpl.PointOnSide(TVec(tmbox[BOX2D_RIGHT], tmbox[BOX2D_BOTTOM], 0));
  } else {
    // negative
    TPlane lpl;
    lpl.SetPointDirXY(v1, dir);
    p1 = lpl.PointOnSide(TVec(tmbox[BOX2D_RIGHT], tmbox[BOX2D_TOP], 0));
    p2 = lpl.PointOnSide(TVec(tmbox[BOX2D_LEFT], tmbox[BOX2D_BOTTOM], 0));
  }

  if (p1 == p2) return p1;
  return -1;
}
