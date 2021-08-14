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

// ////////////////////////////////////////////////////////////////////////// //
#if 0

#define CONST_BBoxVertexIndex  \
  const unsigned BBoxVertexIndex[8][3] = { \
    {BOX3D_MINX, BOX3D_MINY, BOX3D_MINZ}, \
    {BOX3D_MAXX, BOX3D_MINY, BOX3D_MINZ}, \
    {BOX3D_MINX, BOX3D_MAXY, BOX3D_MINZ}, \
    {BOX3D_MAXX, BOX3D_MAXY, BOX3D_MINZ}, \
    {BOX3D_MINX, BOX3D_MINY, BOX3D_MAXZ}, \
    {BOX3D_MAXX, BOX3D_MINY, BOX3D_MAXZ}, \
    {BOX3D_MINX, BOX3D_MAXY, BOX3D_MAXZ}, \
    {BOX3D_MAXX, BOX3D_MAXY, BOX3D_MAXZ}, \
  }

#define CONST_BBoxVertexIndexFlat  \
  const unsigned BBoxVertexIndexFlat[8*3] = { \
    BOX3D_MINX, BOX3D_MINY, BOX3D_MINZ, \
    BOX3D_MAXX, BOX3D_MINY, BOX3D_MINZ, \
    BOX3D_MINX, BOX3D_MAXY, BOX3D_MINZ, \
    BOX3D_MAXX, BOX3D_MAXY, BOX3D_MINZ, \
    BOX3D_MINX, BOX3D_MINY, BOX3D_MAXZ, \
    BOX3D_MAXX, BOX3D_MINY, BOX3D_MAXZ, \
    BOX3D_MINX, BOX3D_MAXY, BOX3D_MAXZ, \
    BOX3D_MAXX, BOX3D_MAXY, BOX3D_MAXZ, \
  }

#define CONST_BBox2DVertexIndex  \
  const unsigned BBox2DVertexIndex[4][2] = { \
    {BOX2D_MINX, BOX2D_MINY}, \
    {BOX2D_MAXX, BOX2D_MINY}, \
    {BOX2D_MINX, BOX2D_MAXY}, \
    {BOX2D_MAXX, BOX2D_MAXY}, \
  }

#define CONST_BBox2DVertexIndexFlat  \
  const unsigned BBox2DVertexIndexFlat[4*2] = { \
    BOX2D_MINX, BOX2D_MINY, \
    BOX2D_MAXX, BOX2D_MINY, \
    BOX2D_MINX, BOX2D_MAXY, \
    BOX2D_MAXX, BOX2D_MAXY, \
  }

#endif

#define HUGE_BBOX2D(bbname_)  float bbname_[4] = { /*maxy*/+FLT_MAX, /*miny*/-FLT_MAX, -FLT_MAX/*minx*/, +FLT_MAX/*maxx*/ }


// this is for 2d line/node bboxes
// bounding box
enum {
  BOX2D_TOP, // the top is greater than the bottom
  BOX2D_BOTTOM, // the bottom is lesser than the top
  BOX2D_LEFT,
  BOX2D_RIGHT,
  // or this
  BOX2D_MAXY = 0,
  BOX2D_MINY = 1,
  BOX2D_MINX = 2,
  BOX2D_MAXX = 3,
};

// this is for 3d bboxes
// bounding box
enum {
  BOX3D_MINX = 0,
  BOX3D_MINY = 1,
  BOX3D_MINZ = 2,
  BOX3D_MAXX = 3,
  BOX3D_MAXY = 4,
  BOX3D_MAXZ = 5,
  // various constants
  BOX3D_MINIDX = 0,
  BOX3D_MAXIDX = 3,
  // add those to MINIDX/MAXIDX to get the corresponding element
  BOX3D_X = 0,
  BOX3D_Y = 1,
  BOX3D_Z = 2,
};


// ////////////////////////////////////////////////////////////////////////// //
// 3d point/vector
class /*__attribute__((packed))*/ TVec {
public:
  float x, y, z;

public:
  static const TVec ZeroVector;
  static const TVec InvalidVector;

public:
  VVA_ALWAYS_INLINE TVec () noexcept {}
  VVA_ALWAYS_INLINE TVec (ENoInit) noexcept {}
  //nope;TVec () : x(0.0f), y(0.0f), z(0.0f) {}
  VVA_ALWAYS_INLINE TVec (float Ax, float Ay, float Az=0.0f) noexcept : x(Ax), y(Ay), z(Az) {}
  VVA_ALWAYS_INLINE TVec (const float f[3]) noexcept { x = f[0]; y = f[1]; z = f[2]; }
  VVA_ALWAYS_INLINE TVec (const TVec &src) noexcept : x(src.x), y(src.y), z(src.z) {}

  //VVA_ALWAYS_INLINE TVec &operator = (const TVec &other) noexcept { if (&other != this) memcpy((void *)this, (const void *)&other, sizeof(TVec)); return *this; }
  //VVA_ALWAYS_INLINE TVec &operator = (const TVec &other) noexcept { x = other.x; y = other.y; z = other.z; return *this; }
  VVA_ALWAYS_INLINE TVec &operator = (const TVec &src) noexcept = default;

  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool operator == (const TVec &v2) const noexcept { return (x == v2.x && y == v2.y && z == v2.z); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool operator != (const TVec &v2) const noexcept { return (x != v2.x || y != v2.y || z != v2.z); }


  VVA_ALWAYS_INLINE VVA_CHECKRESULT const float &operator [] (const size_t i) const noexcept { vassert(i < 3); return (&x)[i]; }
  VVA_ALWAYS_INLINE VVA_CHECKRESULT float &operator [] (const size_t i) noexcept { vassert(i < 3); return (&x)[i]; }

  VVA_ALWAYS_INLINE VVA_CHECKRESULT bool isValid () const noexcept { return (isFiniteF(x) && isFiniteF(y) && isFiniteF(z)); }
  VVA_ALWAYS_INLINE VVA_CHECKRESULT bool isZero () const noexcept { return (x == 0.0f && y == 0.0f && z == 0.0f); }
  VVA_ALWAYS_INLINE VVA_CHECKRESULT bool isZero2D () const noexcept { return (x == 0.0f && y == 0.0f); }

  // this is what VavoomC wants: false is either zero, or invalid vector
  VVA_ALWAYS_INLINE VVA_CHECKRESULT bool toBool () const noexcept {
    return
      isFiniteF(x) && isFiniteF(y) && isFiniteF(z) &&
      (x != 0.0f || y != 0.0f || z != 0.0f);
  }

  VVA_ALWAYS_INLINE TVec &operator += (const TVec &v) noexcept { x += v.x; y += v.y; z += v.z; return *this; }
  VVA_ALWAYS_INLINE TVec &operator -= (const TVec &v) noexcept { x -= v.x; y -= v.y; z -= v.z; return *this; }
  VVA_ALWAYS_INLINE TVec &operator *= (const float scale) noexcept { x *= scale; y *= scale; z *= scale; return *this; }
  VVA_ALWAYS_INLINE TVec &operator /= (float scale) noexcept {
    scale = 1.0f/scale;
    if (!isFiniteF(scale)) scale = 0.0f;
    x *= scale;
    y *= scale;
    z *= scale;
    return *this;
  }

  VVA_CHECKRESULT VVA_ALWAYS_INLINE TVec operator + (void) const noexcept { return *this; }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE TVec operator - (void) const noexcept { return TVec(-x, -y, -z); }

  VVA_CHECKRESULT VVA_ALWAYS_INLINE TVec operator + (const TVec &v2) const noexcept { return TVec(VSUM2(x, v2.x), VSUM2(y, v2.y), VSUM2(z, v2.z)); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE TVec operator - (const TVec &v2) const noexcept { return TVec(VSUM2(x, (-v2.x)), VSUM2(y, (-v2.y)), VSUM2(z, (-v2.z))); }

  VVA_CHECKRESULT VVA_ALWAYS_INLINE TVec operator * (const float s) const noexcept { return TVec(s*x, s*y, s*z); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE TVec operator / (float s) const noexcept { s = 1.0f/s; if (!isFiniteF(s)) s = 0.0f; return TVec(x*s, y*s, z*s); }

  //VVA_CHECKRESULT VVA_ALWAYS_INLINE float operator * (const TVec &b) const noexcept { return dot(b); }
  //VVA_CHECKRESULT VVA_ALWAYS_INLINE TVec operator ^ (const TVec &b) const noexcept { return cross(b); }
  //VVA_CHECKRESULT VVA_ALWAYS_INLINE TVec operator % (const TVec &b) const noexcept { return cross(b); }

#ifdef USE_FAST_INVSQRT
  VVA_ALWAYS_INLINE VVA_CHECKRESULT float invlength () const noexcept { return fastInvSqrtf(VSUM3(x*x, y*y, z*z)); }
  VVA_ALWAYS_INLINE VVA_CHECKRESULT float invlength2D () const noexcept { return fastInvSqrtf(VSUM2(x*x, y*y)); }
#else
  VVA_ALWAYS_INLINE VVA_CHECKRESULT float invlength () const noexcept { return 1.0f/sqrtf(VSUM3(x*x, y*y, z*z)); }
  VVA_ALWAYS_INLINE VVA_CHECKRESULT float invlength2D () const noexcept { return 1.0f/sqrtf(VSUM2(x*x, y*y)); }
#endif

  VVA_ALWAYS_INLINE VVA_CHECKRESULT TVec abs () const noexcept { return TVec(fabsf(x), fabsf(y), fabsf(z)); }

  //VVA_ALWAYS_INLINE VVA_CHECKRESULT float LengthSquared () const noexcept { return VSUM3(x*x, y*y, z*z); }
  VVA_ALWAYS_INLINE VVA_CHECKRESULT float lengthSquared () const noexcept { return VSUM3(x*x, y*y, z*z); }

  //VVA_ALWAYS_INLINE VVA_CHECKRESULT float Length2DSquared () const noexcept { return VSUM2(x*x, y*y); }
  VVA_ALWAYS_INLINE VVA_CHECKRESULT float length2DSquared () const noexcept { return VSUM2(x*x, y*y); }


  // this is slightly slower, but better for axis-aligned vectors

  //VVA_ALWAYS_INLINE VVA_CHECKRESULT float Length2D () const noexcept { return (x && y ? sqrtf(VSUM2(x*x, y*y)) : fabsf(x+y)); }
  VVA_ALWAYS_INLINE VVA_CHECKRESULT float length2D () const noexcept { return (x && y ? sqrtf(VSUM2(x*x, y*y)) : fabsf(x+y)); }

  VVA_ALWAYS_INLINE VVA_CHECKRESULT float length () const noexcept {
         if (z) { if (x || y) return sqrtf(VSUM3(x*x, y*y, z*z)); else return fabsf(z); }
    else if (x && y) return sqrtf(VSUM2(x*x, y*y));
    else return fabsf(x+y);
  }

  //VVA_ALWAYS_INLINE VVA_CHECKRESULT float DistanceTo (const TVec &v) const noexcept { return TVec(x-v.x, y-v.y, z-v.z).Length(); }
  //VVA_ALWAYS_INLINE VVA_CHECKRESULT float DistanceTo2D (const TVec &v) const noexcept { return TVec(x-v.x, y-v.y, 0.0f).Length2D(); }

  VVA_ALWAYS_INLINE VVA_CHECKRESULT float distanceTo (const TVec &v) const noexcept { return TVec(x-v.x, y-v.y, z-v.z).length(); }
  VVA_ALWAYS_INLINE VVA_CHECKRESULT float distanceTo2D (const TVec &v) const noexcept { return TVec(x-v.x, y-v.y, 0.0f).length2D(); }

  /*
  VVA_ALWAYS_INLINE void normaliseInPlace () noexcept { if (z) { const float invlen = invlength(); x *= invlen; y *= invlen; z *= invlen; } else normalise2DInPlace(); }
  VVA_ALWAYS_INLINE void normalise2DInPlace () noexcept { if (x && y) { const float invlen = invlength2D(); x *= invlen; y *= invlen; } else { x = signval(x); y = signval(y); } }

  VVA_ALWAYS_INLINE VVA_CHECKRESULT TVec normalised () const noexcept { if (z) { const float invlen = invlength(); return TVec(x*invlen, y*invlen, z*invlen); } else return normalised2D(); }
  VVA_ALWAYS_INLINE VVA_CHECKRESULT TVec normalised2D () const noexcept { if (x && y) { const float invlen = invlength2D(); return TVec(x*invlen, y*invlen, 0.0f); } else return TVec(signval(x), signval(y), 0.0f); }
  */

  // this is slightly slower, but better for axis-aligned vectors

  VVA_ALWAYS_INLINE void normalise2DInPlace () noexcept {
    if (x && y) {
      const float invlen = invlength2D();
      x *= invlen; y *= invlen;
    } else {
      // at least one of `x`/`y` is zero here (but both can be zeroes, hence the checks)
           if (x) x = (x < 0.0f ? -1.0f : 1.0f);
      else if (y) y = (y < 0.0f ? -1.0f : 1.0f);
    }
    z = 0.0f;
  }

  VVA_ALWAYS_INLINE void normaliseInPlace () noexcept {
    if (z) {
      if (x || y) {
        const float invlen = invlength();
        x *= invlen; y *= invlen; z *= invlen;
      } else {
        // `z` is never zero here
        z = (z < 0.0f ? -1.0f : 1.0f);
      }
    } else {
      normalise2DInPlace();
    }
  }

  VVA_ALWAYS_INLINE VVA_CHECKRESULT TVec normalised2D () const noexcept {
         if (x && y) { const float invlen = invlength2D(); return TVec(x*invlen, y*invlen, 0.0f); }
    else if (x) return TVec((x < 0.0f ? -1.0f : +1.0f), 0.0f, 0.0f);
    else if (y) return TVec(0.0f, (y < 0.0f ? -1.0f : +1.0f), 0.0f);
    else return TVec(0.0f, 0.0f, 0.0f);
  }

  VVA_ALWAYS_INLINE VVA_CHECKRESULT TVec normalised () const noexcept {
    if (z) {
      if (x || y) {
        const float invlen = invlength();
        return TVec(x*invlen, y*invlen, z*invlen);
      } else {
        // `z` is never zero here
        return TVec(0.0f, 0.0f, (z < 0.0f ? -1.0f : 1.0f));
      }
    } else {
      return normalised2D();
    }
  }

  VVA_ALWAYS_INLINE VVA_CHECKRESULT float dot (const TVec &v2) const noexcept { return VSUM3(x*v2.x, y*v2.y, z*v2.z); }

  VVA_ALWAYS_INLINE VVA_CHECKRESULT float dotv2neg (const TVec &v2) const noexcept { return VSUM3(x*(-v2.x), y*(-v2.y), z*(-v2.z)); }

  VVA_ALWAYS_INLINE VVA_CHECKRESULT float dot2D (const TVec &v2) const noexcept { return VSUM2(x*v2.x, y*v2.y); }

  VVA_ALWAYS_INLINE VVA_CHECKRESULT TVec cross (const TVec &v2) const noexcept { return TVec(VSUM2(y*v2.z, -(z*v2.y)), VSUM2(z*v2.x, -(x*v2.z)), VSUM2(x*v2.y, -(y*v2.x))); }

  // 2d cross product (z, as x and y are effectively zero in 2d)
  VVA_ALWAYS_INLINE VVA_CHECKRESULT float cross2D (const TVec &v2) const noexcept { return VSUM2(x*v2.y, -(y*v2.x)); }

  // z is zero
  VVA_ALWAYS_INLINE VVA_CHECKRESULT TVec mul2 (const float s) const noexcept { return TVec(x*s, y*s, 0.0f); }
  VVA_ALWAYS_INLINE VVA_CHECKRESULT TVec mul3 (const float s) const noexcept { return TVec(x*s, y*s, z*s); }

  // returns projection of this vector onto `v`
  VVA_ALWAYS_INLINE VVA_CHECKRESULT TVec projectTo (const TVec &v) const noexcept { return v.mul3(dot(v)/v.lengthSquared()); }
  VVA_ALWAYS_INLINE VVA_CHECKRESULT TVec projectTo2D (const TVec &v) const noexcept { return v.mul2(dot2D(v)/v.length2DSquared()); }

  VVA_ALWAYS_INLINE VVA_CHECKRESULT TVec sub2D (const TVec &v) const noexcept { return TVec(x-v.x, y-v.y, 0.0f); }

  // dir must be normalised, angle must be valid
  VVA_ALWAYS_INLINE VVA_CHECKRESULT bool isInSpotlight (const TVec &origin, const TVec &dir, const float angle) const noexcept {
    TVec surfaceToLight = TVec(-(origin.x-x), -(origin.y-y), -(origin.z-z));
    if (surfaceToLight.lengthSquared() <= 8.0f) return true;
    surfaceToLight.normaliseInPlace();
    const float ltangle = macos(surfaceToLight.dot(dir));
    return (ltangle < angle);
  }

  // dir must be normalised, angle must be valid
  // returns cone light attenuation multiplier in range [0..1]
  VVA_ALWAYS_INLINE VVA_CHECKRESULT float calcSpotlightAttMult (const TVec &origin, const TVec &dir, const float angle) const noexcept {
    TVec surfaceToLight = TVec(-(origin.x-x), -(origin.y-y), -(origin.z-z));
    if (surfaceToLight.lengthSquared() <= 8.0f) { return 1.0f; }
    surfaceToLight.normaliseInPlace();
    const float ltangle = macos(surfaceToLight.dot(dir));
    return (ltangle < angle ? sinf(midval(0.0f, (angle-ltangle)/angle, 1.0f)*((float)M_PI/2.0f)) : 0.0f);
  }

  // range must be valid
  VVA_ALWAYS_INLINE void clampScaleInPlace (const float fabsmax) noexcept {
    if (isValid()) {
      if (fabsmax > 0.0f && (fabsf(x) > fabsmax || fabsf(y) > fabsmax || fabsf(z) > fabsmax)) {
        // need to rescale
        // find abs of the longest axis
        float vv;
        float absmax = fabsf(x);
        // y
        vv = fabsf(y);
        if (vv > absmax) absmax = vv;
        // z
        vv = fabsf(z);
        if (vv > absmax) absmax = vv;
        // now rescale to range size
        const float rngscale = fabsmax/absmax;
        x *= rngscale;
        y *= rngscale;
        z *= rngscale;
      }
    } else {
      x = y = z = 0.0f;
    }
  }

  // return the point on or in AABB b that is closest to p
  VVA_ALWAYS_INLINE VVA_CHECKRESULT TVec closestPointOnBBox3D (const float bbox3d[6]) const noexcept {
    // for each coordinate axis, if the point coordinate value is
    // outside box, clamp it to the box, else keep it as is
    return TVec(
      min2(max2(x, bbox3d[BOX3D_MINX]), bbox3d[BOX3D_MAXX]),
      min2(max2(y, bbox3d[BOX3D_MINY]), bbox3d[BOX3D_MAXY]),
      min2(max2(z, bbox3d[BOX3D_MINZ]), bbox3d[BOX3D_MAXZ]));
  }

  // return the point on or in AABB b that is closest to p
  VVA_ALWAYS_INLINE VVA_CHECKRESULT TVec closestPointOnBBox2D (const float bbox2d[4]) const noexcept {
    // for each coordinate axis, if the point coordinate value is
    // outside box, clamp it to the box, else keep it as is
    return TVec(
      min2(max2(x, bbox2d[BOX2D_MINX]), bbox2d[BOX2D_MAXX]),
      min2(max2(y, bbox2d[BOX2D_MINY]), bbox2d[BOX2D_MAXY]),
      z);
  }

  // computes the square distance between this point and an AABB
  VVA_ALWAYS_INLINE VVA_CHECKRESULT float bbox3DDistanceSquared (const float bbox3d[6]) const noexcept {
    float sqDist = 0.0f;
    // for each axis count any excess distance outside box extents
    { // x
      float v = x;
      if (v < bbox3d[BOX3D_MINX]) sqDist += (bbox3d[BOX3D_MINX]-v)*(bbox3d[BOX3D_MINX]-v);
      if (v > bbox3d[BOX3D_MAXX]) sqDist += (v-bbox3d[BOX3D_MAXX])*(v-bbox3d[BOX3D_MAXX]);
    }
    { // y
      float v = y;
      if (v < bbox3d[BOX3D_MINY]) sqDist += (bbox3d[BOX3D_MINY]-v)*(bbox3d[BOX3D_MINY]-v);
      if (v > bbox3d[BOX3D_MAXY]) sqDist += (v-bbox3d[BOX3D_MAXY])*(v-bbox3d[BOX3D_MAXY]);
    }
    { // z
      float v = z;
      if (v < bbox3d[BOX3D_MINZ]) sqDist += (bbox3d[BOX3D_MINZ]-v)*(bbox3d[BOX3D_MINZ]-v);
      if (v > bbox3d[BOX3D_MAXZ]) sqDist += (v-bbox3d[BOX3D_MAXZ])*(v-bbox3d[BOX3D_MAXZ]);
    }
    return sqDist;
  }

  // computes the square distance between this point and an AABB
  VVA_ALWAYS_INLINE VVA_CHECKRESULT float bbox2DDistanceSquared (const float bbox2d[4]) const noexcept {
    float sqDist = 0.0f;
    // for each axis count any excess distance outside box extents
    { // x
      float v = x;
      if (v < bbox2d[BOX2D_MINX]) sqDist += (bbox2d[BOX2D_MINX]-v)*(bbox2d[BOX2D_MINX]-v);
      if (v > bbox2d[BOX2D_MAXX]) sqDist += (v-bbox2d[BOX2D_MAXX])*(v-bbox2d[BOX2D_MAXX]);
    }
    { // y
      float v = y;
      if (v < bbox2d[BOX2D_MINY]) sqDist += (bbox2d[BOX2D_MINY]-v)*(bbox2d[BOX2D_MINY]-v);
      if (v > bbox2d[BOX2D_MAXY]) sqDist += (v-bbox2d[BOX2D_MAXY])*(v-bbox2d[BOX2D_MAXY]);
    }
    return sqDist;
  }

  // http://www.randygaul.net/2014/07/23/distance-point-to-line-segment/
  VVA_ALWAYS_INLINE VVA_CHECKRESULT float line2DDistanceSquared (const TVec &a, const TVec &b) const noexcept {
    const TVec n = b-a;
    const TVec pa = a-(*this);
    const TVec c = n*(pa.dot2D(n)/n.dot2D(n));
    const TVec d = pa-c;
    return d.dot2D(d);
  }

  // http://www.randygaul.net/2014/07/23/distance-point-to-line-segment/
  VVA_ALWAYS_INLINE VVA_CHECKRESULT float segment2DDistanceSquared (const TVec &a, const TVec &b) const noexcept {
    const TVec n = b-a;
    const TVec pa = a-(*this);

    const float c = n.dot2D(pa);

    if (c > 0.0f) return pa.dot2D(pa); // closest point is a

    const TVec bp = (*this)-b;

    if (n.dot2D(bp) > 0.0f) return bp.dot2D(bp); // closest point is b

    // closest point is between a and b
    const TVec e = pa-n*(c/n.dot2D(n));

    return e.dot2D(e);
  }

  // box point that is furthest in the given direction
  VVA_ALWAYS_INLINE VVA_CHECKRESULT TVec get3DBBoxSupportPoint (const float bbox[6]) const noexcept {
    return TVec(
      bbox[BOX3D_X+(x < 0.0f ? BOX3D_MINIDX : BOX3D_MAXIDX)],
      bbox[BOX3D_Y+(y < 0.0f ? BOX3D_MINIDX : BOX3D_MAXIDX)],
      bbox[BOX3D_Z+(z < 0.0f ? BOX3D_MINIDX : BOX3D_MAXIDX)]);
  }

  VVA_ALWAYS_INLINE VVA_CHECKRESULT TVec get2DBBoxSupportPoint (const float bbox2d[4], const float z=0.0f) const noexcept {
    return TVec(
      bbox2d[x < 0.0f ? BOX2D_LEFT : BOX2D_RIGHT],
      bbox2d[y < 0.0f ? BOX2D_BOTTOM : BOX2D_TOP],
      z);
  }
};

static_assert(__builtin_offsetof(TVec, y) == __builtin_offsetof(TVec, x)+sizeof(float), "TVec layout fail (0)");
static_assert(__builtin_offsetof(TVec, z) == __builtin_offsetof(TVec, y)+sizeof(float), "TVec layout fail (1)");
static_assert(sizeof(TVec) == sizeof(float)*3, "TVec layout fail (2)");

VVA_ALWAYS_INLINE VVA_PURE vuint32 GetTypeHash (const TVec &v) noexcept { return joaatHashBuf(&v, 3*sizeof(float)); }


static VVA_OKUNUSED VVA_CHECKRESULT VVA_ALWAYS_INLINE TVec operator * (const float s, const TVec &v) noexcept { return TVec(s*v.x, s*v.y, s*v.z); }

//static VVA_OKUNUSED VVA_CHECKRESULT VVA_ALWAYS_INLINE TVec abs (const TVec &v1) noexcept { return v1.abs(); }

/*
static VVA_OKUNUSED VVA_CHECKRESULT VVA_ALWAYS_INLINE float Length (const TVec &v) noexcept { return v.length(); }
static VVA_OKUNUSED VVA_CHECKRESULT VVA_ALWAYS_INLINE float length (const TVec &v) noexcept { return v.length(); }
static VVA_OKUNUSED VVA_CHECKRESULT VVA_ALWAYS_INLINE float Length2D (const TVec &v) noexcept { return v.length2D(); }
static VVA_OKUNUSED VVA_CHECKRESULT VVA_ALWAYS_INLINE float length2D (const TVec &v) noexcept { return v.length2D(); }

static VVA_OKUNUSED VVA_CHECKRESULT VVA_ALWAYS_INLINE float LengthSquared (const TVec &v) noexcept { return v.lengthSquared(); }
static VVA_OKUNUSED VVA_CHECKRESULT VVA_ALWAYS_INLINE float lengthSquared (const TVec &v) noexcept { return v.lengthSquared(); }
static VVA_OKUNUSED VVA_CHECKRESULT VVA_ALWAYS_INLINE float Length2DSquared (const TVec &v) noexcept { return v.length2DSquared(); }
static VVA_OKUNUSED VVA_CHECKRESULT VVA_ALWAYS_INLINE float length2DSquared (const TVec &v) noexcept { return v.length2DSquared(); }
*/

//static VVA_OKUNUSED VVA_CHECKRESULT VVA_ALWAYS_INLINE TVec Normalise (const TVec &v) noexcept { return v.normalised(); }
//static VVA_OKUNUSED VVA_CHECKRESULT VVA_ALWAYS_INLINE TVec normalise (const TVec &v) noexcept { return v.normalised(); }

//static VVA_OKUNUSED VVA_CHECKRESULT VVA_ALWAYS_INLINE TVec normalise2D (const TVec &v) noexcept { return v.normalised2D(); }

//static VVA_OKUNUSED VVA_CHECKRESULT VVA_ALWAYS_INLINE float DotProduct (const TVec &v1, const TVec &v2) noexcept { return v1.dot(v2); }
//static VVA_OKUNUSED VVA_CHECKRESULT VVA_ALWAYS_INLINE float dot (const TVec &v1, const TVec &v2) noexcept { return v1.dot(v2); }

//static VVA_OKUNUSED VVA_CHECKRESULT VVA_ALWAYS_INLINE float DotProductV2Neg (const TVec &v1, const TVec &v2) noexcept { return v1.dotv2neg(v2); }
//static VVA_OKUNUSED VVA_CHECKRESULT VVA_ALWAYS_INLINE float dotv2neg (const TVec &v1, const TVec &v2) noexcept { return v1.dotv2neg(v2); }

//static VVA_OKUNUSED VVA_CHECKRESULT VVA_ALWAYS_INLINE float DotProduct2D (const TVec &v1, const TVec &v2) noexcept { return v1.dot2D(v2); }
//static VVA_OKUNUSED VVA_CHECKRESULT VVA_ALWAYS_INLINE float dot2D (const TVec &v1, const TVec &v2) noexcept { return v1.dot2D(v2); }

//static VVA_OKUNUSED VVA_CHECKRESULT VVA_ALWAYS_INLINE TVec CrossProduct (const TVec &v1, const TVec &v2) noexcept { return v1.cross(v2); }
//static VVA_OKUNUSED VVA_CHECKRESULT VVA_ALWAYS_INLINE TVec cross (const TVec &v1, const TVec &v2) noexcept { return v1.cross(v2); }

// returns signed magnitude of cross-product (z, as x and y are effectively zero in 2d)
//static VVA_OKUNUSED VVA_CHECKRESULT VVA_ALWAYS_INLINE float CrossProduct2D (const TVec &v1, const TVec &v2) noexcept { return v1.cross2D(v2); }
//static VVA_OKUNUSED VVA_CHECKRESULT VVA_ALWAYS_INLINE float cross2D (const TVec &v1, const TVec &v2) noexcept { return v1.cross2D(v2); }

static VVA_OKUNUSED VVA_ALWAYS_INLINE VStream &operator << (VStream &Strm, TVec &v) { return Strm << v.x << v.y << v.z; }


#define MAX_BBOX3D_CORNERS  (8u)
#define MAX_BBOX2D_CORNERS  (4u)

// idx: [0..7]
static VVA_OKUNUSED VVA_CHECKRESULT VVA_ALWAYS_INLINE TVec GetBBox3DCorner (const unsigned index, const float bbox3d[6]) noexcept {
  return TVec(
    bbox3d[(index&0x01u ? BOX3D_MAXX : BOX3D_MINX)],
    bbox3d[(index&0x02u ? BOX3D_MAXY : BOX3D_MINY)],
    bbox3d[(index&0x04u ? BOX3D_MAXZ : BOX3D_MINZ)]);
}

// idx: [0..3]
static VVA_OKUNUSED VVA_CHECKRESULT VVA_ALWAYS_INLINE TVec GetBBox2DCorner (const unsigned index, const float bbox2d[4]) noexcept {
  return TVec(
    bbox2d[(index&0x01u ? BOX2D_MINX : BOX2D_MAXX)],
    bbox2d[(index&0x02u ? BOX2D_MINY : BOX2D_MAXY)],
    0.0f);
}

// idx: [0..3]
static VVA_OKUNUSED VVA_CHECKRESULT VVA_ALWAYS_INLINE TVec GetBBox2DCornerEx (const unsigned index, const float bbox2d[4], const float z) noexcept {
  return TVec(
    bbox2d[(index&0x01u ? BOX2D_MINX : BOX2D_MAXX)],
    bbox2d[(index&0x02u ? BOX2D_MINY : BOX2D_MAXY)],
    z);
}


void AngleVectors (const TAVec &angles, TVec &forward, TVec &right, TVec &up) noexcept;
void VectorsAngles (const TVec &forward, const TVec &right, const TVec &up, TAVec &angles) noexcept;
VVA_CHECKRESULT TVec AnglesRightVector (const TAVec &angles) noexcept;
VVA_CHECKRESULT TVec AngleVector (const TAVec &angles) noexcept; // forward vector
VVA_CHECKRESULT TVec AngleVectorPitch (const float pitch) noexcept;
VVA_CHECKRESULT TVec YawVectorRight (float yaw) noexcept;
VVA_CHECKRESULT TAVec VectorAngles (const TVec &vec) noexcept; // from forward

VVA_CHECKRESULT TVec RotateVectorAroundVector (const TVec &Vector, const TVec &Axis, float Angle) noexcept;

void RotatePointAroundVector (TVec &dst, const TVec &dir, const TVec &point, float degrees) noexcept;
// sets axis[1] and axis[2]
void RotateAroundDirection (TVec axis[3], float yaw) noexcept;

// given a normalized forward vector, create two other perpendicular vectors
void MakeNormalVectors (const TVec &forward, TVec &right, TVec &up) noexcept;


static VVA_ALWAYS_INLINE VVA_OKUNUSED VVA_CHECKRESULT TVec AngleVectorYaw (const float yaw) noexcept {
  float sy, cy;
  msincos(yaw, &sy, &cy);
  return TVec(cy, sy, 0.0f);
}

static VVA_ALWAYS_INLINE VVA_OKUNUSED VVA_CHECKRESULT float VectorAngleYaw (const TVec &vec) noexcept {
  const float fx = vec.x;
  const float fy = vec.y;
  const float len2d = VSUM2(fx*fx, fy*fy);
  return (len2d < 0.0001f ? 0.0f : matan(fy, fx));
}

static VVA_ALWAYS_INLINE VVA_OKUNUSED VVA_CHECKRESULT float VectorAnglePitch (const TVec &vec) noexcept {
  const float fx = vec.x;
  const float fy = vec.y;
  const float len2d = VSUM2(fx*fx, fy*fy);
  return (len2d < 0.0001f ? (vec.z < 0.0f ? 90 : 270) : -matan(vec.z, sqrtf(len2d)));
}

// returns `false` on error (and zero `dst`)
bool ProjectPointOnPlane (TVec &dst, const TVec &p, const TVec &normal) noexcept;

void PerpendicularVector (TVec &dst, const TVec &src) noexcept; // assumes "src" is normalised


// origin is center
static VVA_ALWAYS_INLINE VVA_OKUNUSED void Create2DBBox (float box[4], const TVec &origin, const float radius) noexcept {
  box[BOX2D_MAXY] = origin.y+radius;
  box[BOX2D_MINY] = origin.y-radius;
  box[BOX2D_MINX] = origin.x-radius;
  box[BOX2D_MAXX] = origin.x+radius;
}

// origin is center, bottom
static VVA_ALWAYS_INLINE VVA_OKUNUSED void Create3DBBox (float box[6], const TVec &origin, const float radius, const float height) noexcept {
  box[BOX3D_MINX] = origin.x-radius;
  box[BOX3D_MINY] = origin.y-radius;
  box[BOX3D_MINZ] = origin.z;
  box[BOX3D_MAXX] = origin.x+radius;
  box[BOX3D_MAXY] = origin.y+radius;
  box[BOX3D_MAXZ] = origin.z+height;
}


static VVA_ALWAYS_INLINE VVA_OKUNUSED VVA_CHECKRESULT bool IsPointInsideBBox2D (const float x, const float y, const float bbox2d[4]) noexcept {
  return
    x >= bbox2d[BOX2D_MINX] && x <= bbox2d[BOX2D_MAXX] &&
    y >= bbox2d[BOX2D_MINY] && y <= bbox2d[BOX2D_MAXY];
}


#define Create2DBBoxFrom3DBBox(destb_,srcb_)  \
  const float destb_[4] = { (srcb_)[BOX3D_MAXY], (srcb_)[BOX3D_MINY], (srcb_)[BOX3D_MINX], (srcb_)[BOX3D_MAXX] }

#define Create3DBBoxFrom2DBBox(destb_,srcb_,zmin_,zmax_)  \
  const float destb_[6] = { (srcb_)[BOX2D_MINX], (srcb_)[BOX2D_MINY], (zmin_), (srcb_)[BOX2D_MAXX], (srcb_)[BOX2D_MAXY], (zmax_) }

#define Create2DBBoxFromVectors(destb_,minb_,maxb_)  \
  const float destb_[4] = { (maxb_).y, (minb_).y, (minb_).x, (maxb_).x }

#define Create3DBBoxFromVectors(destb_,minb_,maxb_)  \
  const float destb_[6] = { (minb_).x, (minb_).y, (minb_).z, (maxb_).x, (maxb_).y, (maxb_).z }

#define CreateDoom2DBBox(destb_,org_,rad_)  \
  const float destb_[4] = { (org_).y+(rad_), (org_).y-(rad_), (org_).x-(rad_), (org_).x+(rad_) }

#define CreateDoom3DBBox(destb_,org_,rad_,hgt_)  \
  const float destb_[6] = { (org_).x-(rad_), (org_).y-(rad_), (org_).z, (org_).x+(rad_), (org_).y+(rad_), (org_).z+(hgt_) }


static VVA_ALWAYS_INLINE VVA_OKUNUSED VVA_CHECKRESULT bool isCircleTouchingLine (const TVec &corg, const float radiusSq, const TVec &v0, const TVec &v1) noexcept {
  const TVec s0qp = corg-v0;
  if (s0qp.length2DSquared() <= radiusSq) return true;
  if ((corg-v1).length2DSquared() <= radiusSq) return true;
  const TVec s0s1 = v1-v0;
  const float a = s0s1.dot2D(s0s1);
  if (a == 0.0f) return false;
  const float b = s0s1.dot2D(s0qp);
  const float t = b/a; // length of projection of s0qp onto s0s1
  if (t < 0.0f || t > 1.0f) return false;
  const float c = s0qp.dot2D(s0qp);
  const float r2 = c-a*t*t;
  return (r2 < radiusSq); // true if collides
}


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
float RayBoxIntersection2D (const TVec &c, const float rad, const TVec &org, const TVec &dir) noexcept;


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
float RayBoxIntersection3D (const TVec &c, const float rad, const float hgt, const TVec &org, const TVec &dir) noexcept;
