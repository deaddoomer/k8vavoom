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


const TVec TVec::ZeroVector = TVec(0.0f, 0.0f, 0.0f);
const TVec TVec::InvalidVector = TVec(NAN, NAN, NAN);


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

  forward.fixDenormalsInPlace();
  right.fixDenormalsInPlace();
  up.fixDenormalsInPlace();
}


//==========================================================================
//
//  AnglesRightVector
//
//==========================================================================
TVec AnglesRightVector (const TAVec &angles) noexcept {
  TVec right;
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
  right.fixDenormalsInPlace();
  return right;
}


//==========================================================================
//
//  YawVectorRight
//
//==========================================================================
TVec YawVectorRight (float yaw) noexcept {
  /*
  float sy, cy;
  msincos(yaw, &sy, &cy);
  right.x = sy;
  right.y = -cy;
  */
  TVec right;
  msincos(yaw, &right.x, &right.y);
  right.y = -right.y;
  right.z = 0.0f;
  right.fixDenormalsInPlace();
  return right;
}


//==========================================================================
//
//  AngleVector
//
//==========================================================================
TVec AngleVector (const TAVec &angles) noexcept {
  /*
  const float ay = DEG2RADF(angles.yaw);
  const float ap = DEG2RADF(angles.pitch);

  const float sy = sinf(ay);
  const float cy = cosf(ay);
  const float sp = sinf(ap);
  const float cp = cosf(ap);
  */
  TVec forward;
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
  forward.fixDenormalsInPlace();
  return forward;
}


//==========================================================================
//
//  VectorAngles
//
//==========================================================================
TAVec VectorAngles (const TVec &vec) noexcept {
  TAVec angles;
  const float fx = vec.x;
  const float fy = vec.y;
  const float len2d = VSUM2(fx*fx, fy*fy);
  if (len2d < 0.0001f) {
    angles.pitch = (vec.z < 0.0f ? 90 : 270);
    angles.yaw = 0.0f;
  } else {
    angles.pitch = -matan(vec.z, sqrtf(len2d));
    angles.yaw = matan(fy, fx);
  }
  angles.roll = 0.0f;
  return angles;
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
      angles.pitch = 270;
      angles.roll = matan(-up.y, -up.x);
    } else {
      angles.pitch = 90;
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
//  AngleVectorPitch
//
//==========================================================================
VVA_CHECKRESULT TVec AngleVectorPitch (const float pitch) noexcept {
  TVec forward;
  msincos(pitch, &forward.z, &forward.x);
  forward.y = 0.0f;
  forward.z = -forward.z;
  /*
  forward.x = mcos(pitch);
  forward.y = 0.0f;
  forward.z = -msin(pitch);
  */
  forward.fixDenormalsInPlace();
  return forward;
}


//==========================================================================
//
//  ProjectPointOnPlane
//
//  returns `false` on error (and zero `dst`)
//
//==========================================================================
bool ProjectPointOnPlane (TVec &dst, const TVec &p, const TVec &normal) noexcept {
  const float inv_denom = 1.0f/normal.dot(normal);
  if (!isFiniteF(inv_denom)) { dst = TVec(0.0f, 0.0f, 0.0f); return false; } //k8: what to do here?
  const float d = normal.dot(p)*inv_denom;
  dst = p-d*(normal*inv_denom);
  dst.fixDenormalsInPlace();
  return true;
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
  return (Vector*M).noDenormals();
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
  vup = vr.cross(vf);

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

  for (unsigned i = 0; i < 3; ++i) dst[i] = zeroDenormalsF(rot[i][0]*point[0]+rot[i][1]*point[1]+rot[i][2]*point[2]);
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
  axis[2] = axis[0].cross(axis[1]).noDenormals();
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
  float d = right.dot(forward);
  //VectorMA(right, -d, forward, right);
  // (const vec3_t veca, float scale, const vec3_t vecb, vec3_t vecc): vecc = veca+scal*vecb
  right -= forward*d;
  right.normaliseInPlace();
  up = right.cross(forward);
  up.fixDenormalsInPlace();
}



//**************************************************************************
//
// ray-box intersection
//
//**************************************************************************

#define RBI_FIRST()  do { \
  /* we know `tmin` and `tmax` here for sure */ \
  /*tmin = max2(tmin, min2(t1, t2));*/ \
  /*tmax = min2(tmax, max2(t1, t2));*/ \
  if (t1 < t2) { tmin = t1; tmax = t2; } else { tmin = t2; tmax = t1; } \
  wasTM = true; \
} while (0)


#define RBI_NEXT()  do { \
  /*tmin = max2(tmin, min2(t1, t2));*/ \
  /*tmax = min2(tmax, max2(t1, t2));*/ \
  if (wasTM) { \
    if (t1 < t2) { \
      if (tmin < t1) tmin = t1; \
      if (tmax > t2) tmax = t2; \
    } else { \
      if (tmin < t2) tmin = t2; \
      if (tmax > t1) tmax = t1; \
    } \
  } else { \
    /* we know `tmin` and `tmax` here for sure */ \
    if (t1 < t2) { tmin = t1; tmax = t2; } else { tmin = t2; tmax = t1; } \
  } \
} while (0)


//==========================================================================
//
//  RayBoxIntersection2D
//
//  this is slow, but we won't test billions of things anyway
//  returns intersection time, `0.0f` for "inside", or `-1.0f` for outside
//
//  `org` is ray origin
//  `dir` is ray direction (not normalized! used to get ray endpoint)
//
//  `rad` should not be negative
//
//==========================================================================
float RayBoxIntersection2D (const TVec &c, const float rad, const TVec &org, const TVec &dir) noexcept {
  float tmin = -INFINITY, tmax = INFINITY;

  const float xc = c.x-org.x;
  const float yc = c.y-org.y;

  bool wasTM = false;

  // x
  if (dir.x != 0.0f) {
    const float inv = 1.0f/dir.x;
    const float t1 = (xc-rad)*inv;
    const float t2 = (xc+rad)*inv;
    RBI_FIRST();
  } else {
    if (fabsf(xc) > rad) return -1.0f;
  }

  // y
  if (dir.y != 0.0f) {
    const float inv = 1.0f/dir.y;
    const float t1 = (yc-rad)*inv;
    const float t2 = (yc+rad)*inv;
    RBI_NEXT();
  } else {
    if (fabsf(yc) > rad) return -1.0f;
  }

  // `tmin` is "enter time", `tmax` is "exit time"
  // if `tmin` is negative, we're started inside the box

  if (tmax < 0.0f || tmin > 1.0f || tmax <= tmin) return -1.0f; // didn't hit
  return zeroDenormalsF(max2(0.0f, tmin));
}


//==========================================================================
//
//  RayBoxIntersection3D
//
//  this is slow, but we won't test billions of things anyway
//  returns intersection time, `0.0f` for "inside", or `-1.0f` for outside
//
//  `org` is ray origin
//  `dir` is ray direction (not normalized! used to get ray endpoint)
//  `c.z` is box bottom; box top is `c.z+hgt`
//
//  `rad` should not be negative
//  `hgt` should not be negative
//
//==========================================================================
float RayBoxIntersection3D (const TVec &c, const float rad, const float hgt, const TVec &org, const TVec &dir) noexcept {
  float tmin = -INFINITY, tmax = INFINITY;

  const float xc = c.x-org.x;
  const float yc = c.y-org.y;
  const float zbot = c.z-org.z;

  bool wasTM = false;

  // x
  if (dir.x != 0.0f) {
    const float inv = 1.0f/dir.x;
    const float t1 = (xc-rad)*inv;
    const float t2 = (xc+rad)*inv;
    RBI_FIRST();
  } else {
    if (fabsf(xc) > rad) return -1.0f;
  }

  // y
  if (dir.y != 0.0f) {
    const float inv = 1.0f/dir.y;
    const float t1 = (yc-rad)*inv;
    const float t2 = (yc+rad)*inv;
    RBI_NEXT();
    wasTM = true;
  } else {
    if (fabsf(yc) > rad) return -1.0f;
  }

  // z
  if (dir.z != 0.0f) {
    const float inv = 1.0f/dir.z;
    const float t1 = zbot*inv;
    const float t2 = (zbot+hgt)*inv;
    RBI_NEXT();
  } else {
    if (org.z < c.z || org.z > c.z+hgt) return -1.0f;
  }

  // `tmin` is "enter time", `tmax` is "exit time"
  // if `tmin` is negative, we're started inside the box

  if (tmax < 0.0f || tmin > 1.0f || tmax <= tmin) return -1.0f; // didn't hit
  return zeroDenormalsF(max2(0.0f, tmin));
}
