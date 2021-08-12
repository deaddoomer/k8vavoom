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
// Ax+By+Cz=D (ABC is normal, D is distance)
// this the same as in Quake engine, so i can... borrow code from it ;-)
class /*__attribute__((packed))*/ TPlane {
public:
  TVec normal;
  float dist;

public:
  // put here because i canot find a better place for it yet
  static VVA_OKUNUSED inline void CreateBBox (float bbox[6], const TVec &v0, const TVec &v1) noexcept {
    if (v0.x < v1.x) {
      bbox[BOX3D_MINX] = v0.x;
      bbox[BOX3D_MAXX] = v1.x;
    } else {
      bbox[BOX3D_MINX] = v1.x;
      bbox[BOX3D_MAXX] = v0.x;
    }
    if (v0.y < v1.y) {
      bbox[BOX3D_MINY] = v0.y;
      bbox[BOX3D_MAXY] = v1.y;
    } else {
      bbox[BOX3D_MINY] = v1.y;
      bbox[BOX3D_MAXY] = v0.y;
    }
    if (v0.z < v1.z) {
      bbox[BOX3D_MINZ] = v0.z;
      bbox[BOX3D_MAXZ] = v1.z;
    } else {
      bbox[BOX3D_MINZ] = v1.z;
      bbox[BOX3D_MAXZ] = v0.z;
    }
  }

public:
  // fuck you, shitpp! i have to define all ctors when i want only one or two
  VVA_ALWAYS_INLINE TPlane () noexcept {}
  VVA_ALWAYS_INLINE TPlane (ENoInit) noexcept {}
  VVA_ALWAYS_INLINE TPlane (const TVec &anormal, const float adist) noexcept : normal(anormal), dist(adist) {}
  VVA_ALWAYS_INLINE TPlane (const TPlane &src) noexcept : normal(src.normal), dist(src.dist) {}

  VVA_ALWAYS_INLINE TPlane &operator = (const TPlane &src) noexcept = default;

  VVA_ALWAYS_INLINE VVA_CHECKRESULT bool isValid () const noexcept { return (normal.isValid() && !normal.isZero() && isFiniteF(dist)); }
  VVA_ALWAYS_INLINE VVA_CHECKRESULT bool isVertical () const noexcept { return (normal.z == 0.0f); }

  VVA_ALWAYS_INLINE void Set (const TVec &Anormal, const float Adist) noexcept {
    normal = Anormal;
    dist = Adist;
  }

  inline void SetAndNormalise (const TVec &Anormal, const float Adist) noexcept {
    normal = Anormal;
    dist = Adist;
    NormaliseInPlace();
    if (!normal.isValid() || normal.isZero()) {
      //k8: what to do here?!
      normal = TVec(1.0f, 0.0f, 0.0f);
      dist = 0.0f;
      return;
    }
  }

  // initialises vertical plane from point and direction (direction need not to be normalized)
  inline void SetPointDirXY (const TVec &point, const TVec &dir) noexcept {
    normal = TVec(dir.y, -dir.x, 0.0f);
    // use some checks to avoid floating point inexactness on axial planes
    if (!normal.x) {
      if (!normal.y) {
        //k8: what to do here?!
        normal = TVec(1.0f, 0.0f, 0.0f);
        dist = 0.0f;
        return;
      }
      // vertical
      normal.y = (normal.y < 0 ? -1.0f : 1.0f);
    } else if (!normal.y) {
      // horizontal
      normal.x = (normal.x < 0 ? -1.0f : 1.0f);
    } else {
      // sloped
      normal.normaliseInPlace();
    }
    dist = point.dot(normal);
  }

  // initialises "full" plane from point and direction
  // `norm` must be normalized, both vectors must be valid
  VVA_ALWAYS_INLINE void SetPointNormal3D (const TVec &point, const TVec &norm) noexcept {
    normal = norm;
    dist = point.dot(normal);
  }

  // initialises "full" plane from point and direction (direction will be normalised)
  inline void SetPointNormal3DSafe (const TVec &point, const TVec &dir) noexcept {
    if (dir.isValid() && point.isValid() && !dir.isZero()) {
      normal = dir.normalised();
      if (normal.isValid() && !normal.isZero()) {
        dist = point.dot(normal);
      } else {
        //k8: what to do here?!
        normal = TVec(1.0f, 0.0f, 0.0f);
        dist = 0.0f;
      }
    } else {
      //k8: what to do here?!
      normal = TVec(1.0f, 0.0f, 0.0f);
      dist = 0.0f;
    }
  }

  // initialises vertical plane from 2 points
  VVA_ALWAYS_INLINE void Set2Points (const TVec &v1, const TVec &v2) noexcept {
    SetPointDirXY(v1, v2-v1);
  }

  // the normal will point out of the clock for clockwise ordered points
  VVA_ALWAYS_INLINE void SetFromTriangle (const TVec &a, const TVec &b, const TVec &c) noexcept {
    normal = (c-a).cross(b-a).normalised();
    dist = a.dot(normal);
  }

  VVA_ALWAYS_INLINE bool isFloor () const noexcept { return (normal.z > 0.0f); }
  VVA_ALWAYS_INLINE bool isCeiling () const noexcept { return (normal.z < 0.0f); }
  VVA_ALWAYS_INLINE bool isSlope () const noexcept { return (fabsf(normal.z) != 1.0f); }
  VVA_ALWAYS_INLINE bool isWall () const noexcept { return (normal.z == 0.0f); }

  // valid only for horizontal planes!
  VVA_ALWAYS_INLINE float GetRealDist () const noexcept { return dist*normal.z; }

  // WARNING! do not call this repeatedly, or on normalized plane!
  //          due to floating math inexactness, you will accumulate errors.
  VVA_ALWAYS_INLINE void NormaliseInPlace () noexcept {
    const float mag = normal.invlength();
    normal *= mag;
    // multiply by mag too, because we're doing "dot-dist", so
    // if our vector becomes smaller, our dist should become smaller too
    dist *= mag;
  }

  VVA_ALWAYS_INLINE void FlipInPlace () noexcept {
    normal = -normal;
    dist = -dist;
  }

  // *signed* distance from point to plane
  // plane must be normalized
  VVA_ALWAYS_INLINE VVA_CHECKRESULT float PointDistance (const TVec &p) const noexcept {
    return p.dot(normal)-dist;
  }

  // returns intersection time
  // negative means "no intersection" (and `*currhit` is not modified)
  // yeah, we have too many of those
  // this one is used in various plane checks in the engine (it happened so historically)
  inline float IntersectionTime (const TVec &linestart, const TVec &lineend, TVec *currhit=nullptr) const noexcept {
    const float d1 = PointDistance(linestart);
    if (d1 < 0.0f) return -1.0f; // don't shoot back side

    const float d2 = PointDistance(lineend);
    if (d2 >= 0.0f) return -1.0f; // didn't hit plane

    //if (d2 > 0.0f) return true; // didn't hit plane (was >=)
    //if (fabsf(d2-d1) < 0.0001f) return true; // too close to zero

    const float frac = d1/(d1-d2); // [0..1], from start
    if (!isFiniteF(frac) || frac < 0.0f || frac > 1.0f) return -1.0f; // just in case

    if (currhit) *currhit = linestart+(lineend-linestart)*frac;
    return frac;
  }

  // get z of point with given x and y coords
  // don't try to use it on a vertical plane
  VVA_ALWAYS_INLINE VVA_CHECKRESULT float GetPointZ (const float x, const float y) const noexcept {
    return (VSUM3(dist, -(normal.x*x), -(normal.y*y))/normal.z);
  }

  VVA_ALWAYS_INLINE VVA_CHECKRESULT float GetPointZRev (const float x, const float y) const noexcept {
    return (VSUM3(-dist, -(-normal.x*x), -(-normal.y*y))/(-normal.z));
  }

  VVA_ALWAYS_INLINE VVA_CHECKRESULT float GetPointZ (const TVec &v) const noexcept {
    return GetPointZ(v.x, v.y);
  }

  VVA_ALWAYS_INLINE VVA_CHECKRESULT float GetPointZRev (const TVec &v) const noexcept {
    return GetPointZRev(v.x, v.y);
  }


  // "land" point onto the plane
  // plane must be normalized
  VVA_ALWAYS_INLINE VVA_CHECKRESULT TVec landAlongNormal (const TVec &point) const noexcept {
    const float pdist = PointDistance(point);
    return (fabsf(pdist) > 0.0001f ? point-normal*pdist : point);
  }

  // plane must be normalized
  VVA_ALWAYS_INLINE VVA_CHECKRESULT TVec Project (const TVec &v) const noexcept {
    return v-(v-normal*dist).dot(normal)*normal;
  }


  // returns the point where the line p0-p1 intersects this plane
  // `p0` and `p1` must not be the same
  VVA_ALWAYS_INLINE VVA_CHECKRESULT float LineIntersectTime (const TVec &p0, const TVec &p1, const float eps=0.0001f) const noexcept {
    const float dv = normal.dot(p1-p0);
    return (fabsf(dv) > eps ? (dist-normal.dot(p0))/dv : INFINITY);
  }

  // returns the point where the line p0-p1 intersects this plane
  // `p0` and `p1` must not be the same
  inline VVA_CHECKRESULT TVec LineIntersect (const TVec &p0, const TVec &p1, const float eps=0.0001f) const noexcept {
    const TVec dif = p1-p0;
    const float dv = normal.dot(dif);
    const float t = (fabsf(dv) > eps ? (dist-normal.dot(p0))/dv : 0.0f);
    return p0+(dif*t);
  }

  // returns the point where the line p0-p1 intersects this plane
  // `p0` and `p1` must not be the same
  inline VVA_CHECKRESULT bool LineIntersectEx (TVec &res, const TVec &p0, const TVec &p1, const float eps=0.0001f) const noexcept {
    const TVec dif = p1-p0;
    const float dv = normal.dot(dif);
    if (fabsf(dv) > eps) {
      const float t = (dist-normal.dot(p0))/normal.dot(dif);
      if (t < 0.0f) { res = p0; return false; }
      if (t > 1.0f) { res = p1; return false; }
      res = p0+(dif*t);
      return true;
    } else {
      res = p0;
      return false;
    }
  }

  // returns the point where the line p0-p1 intersects this plane
  // `p0` and `p1` must not be the same
  VVA_ALWAYS_INLINE VVA_CHECKRESULT float LineIntersectTimeWithShift (const TVec &p0, const TVec &p1, const float shift, const float eps=0.0001f) const noexcept {
    const float dv = normal.dot(p1-p0);
    return (fabsf(dv) > eps ? (dist+shift-normal.dot(p0))/dv : INFINITY);
  }


  // intersection of 3 planes, Graphics Gems 1 pg 305
  // not sure if it should be `dist` or `-dist` here for vavoom planes
  inline VVA_CHECKRESULT TVec IntersectionPoint (const TPlane &plane2, const TPlane &plane3) const noexcept {
    const float det = normal.cross(plane2.normal).dot(plane3.normal);
    // if the determinant is 0, that means parallel planes, no intersection
    if (fabsf(det) < 0.001f) return TVec::InvalidVector;
    return
      (plane2.normal.cross(plane3.normal)*(-dist)+
       plane3.normal.cross(normal)*(-plane2.dist)+
       normal.cross(plane2.normal)*(-plane3.dist))/det;
  }


  // sphere sweep test
  // `hitpos` will be sphere position when it hits this plane
  // `time` will be normalized collision time (negative if initially stuck)
  // returns `true` if stuck/hit
  bool SweepSphere (const TVec &origin, const float radius, const TVec &vdelta, TVec *hitpos=nullptr, float *time=nullptr) const noexcept;

  // box must be valid
  // `time` will be set only if our box hits/penetrates the plane
  // negative time means "stuck" (and it will be exit time)
  // `vstart` and `vend` are for bounding box center point
  // returns `true` on hit/stuck
  bool SweepBox3DEx (const float radius, const float height, const TVec &vstart, const TVec &vend, float *time=nullptr, TVec *hitpoint=nullptr) const noexcept;

  // box must be valid
  // `time` will be set only if our box hits/penetrates the plane
  // if the box is initially stuck, time will be negative
  // `bbox` are bounding box *coordinates*
  // returns `true` on hit/stuck
  bool SweepBox3D (const float bbox[6], const TVec &vdelta, float *time=nullptr, TVec *hitpoint=nullptr) const noexcept;


  // returns side 0 (front) or 1 (back, or on plane)
  VVA_ALWAYS_INLINE VVA_CHECKRESULT int PointOnSide (const TVec &point) const noexcept {
    return (point.dot(normal)-dist <= 0.0f);
  }

  // returns side 0 (front) or 1 (back, or on plane)
  VVA_ALWAYS_INLINE VVA_CHECKRESULT int PointOnSideThreshold (const TVec &point) const noexcept {
    return (point.dot(normal)-dist < 0.1f);
  }

  // returns side 0 (front) or 1 (back, or on plane)
  VVA_ALWAYS_INLINE VVA_CHECKRESULT int PointOnSideThreshold (const TVec &point, const float thre) const noexcept {
    return (point.dot(normal)-dist < thre);
  }

  // returns side 0 (front, or on plane) or 1 (back)
  // "fri" means "front inclusive"
  VVA_ALWAYS_INLINE VVA_CHECKRESULT int PointOnSideFri (const TVec &point) const noexcept {
    return (point.dot(normal)-dist < 0.0f);
  }

  // returns side 0 (front), 1 (back), or 2 (on)
  // used in line tracing (only)
  VVA_ALWAYS_INLINE VVA_CHECKRESULT int PointOnSide2 (const TVec &point) const noexcept {
    const float dot = point.dot(normal)-dist;
    return (dot < -0.1f ? 1 : dot > 0.1f ? 0 : 2);
  }

  // returns side 0 (front), 1 (back)
  // if at least some part of the sphere is on a front side, it means "front"
  VVA_ALWAYS_INLINE VVA_CHECKRESULT int SphereOnSide (const TVec &center, const float radius) const noexcept {
    return (PointDistance(center) <= -radius);
  }

  VVA_ALWAYS_INLINE VVA_CHECKRESULT bool SphereTouches (const TVec &center, const float radius) const noexcept {
    return (fabsf(PointDistance(center)) < radius);
  }

  // returns side 0 (front), 1 (back), or 2 (collides)
  VVA_ALWAYS_INLINE VVA_CHECKRESULT int SphereOnSide2 (const TVec &center, const float radius) const noexcept {
    const float d = PointDistance(center);
    return (d < -radius ? 1 : d > radius ? 0 : 2);
  }

  // returns "AABB reject point"
  // i.e. box point that is furthest from the plane
  VVA_ALWAYS_INLINE VVA_CHECKRESULT TVec get3DBBoxRejectPoint (const float bbox[6]) const noexcept {
    return TVec(
      bbox[BOX3D_X+(normal.x < 0.0f ? BOX3D_MINIDX : BOX3D_MAXIDX)],
      bbox[BOX3D_Y+(normal.y < 0.0f ? BOX3D_MINIDX : BOX3D_MAXIDX)],
      bbox[BOX3D_Z+(normal.z < 0.0f ? BOX3D_MINIDX : BOX3D_MAXIDX)]);
  }

  // returns "AABB accept point"
  // i.e. box point that is closest to the plane
  VVA_ALWAYS_INLINE VVA_CHECKRESULT TVec get3DBBoxAcceptPoint (const float bbox[6]) const noexcept {
    return TVec(
      bbox[BOX3D_X+(normal.x < 0.0f ? BOX3D_MAXIDX : BOX3D_MINIDX)],
      bbox[BOX3D_Y+(normal.y < 0.0f ? BOX3D_MAXIDX : BOX3D_MINIDX)],
      bbox[BOX3D_Z+(normal.z < 0.0f ? BOX3D_MAXIDX : BOX3D_MINIDX)]);
  }

  VVA_ALWAYS_INLINE VVA_CHECKRESULT TVec get2DBBoxRejectPoint (const float bbox2d[4], const float minz=0.0f, const float maxz=0.0f) const noexcept {
    return TVec(
      bbox2d[normal.x < 0.0f ? BOX2D_LEFT : BOX2D_RIGHT],
      bbox2d[normal.y < 0.0f ? BOX2D_BOTTOM : BOX2D_TOP],
      (normal.z < 0.0f ? minz : maxz));
  }

  VVA_ALWAYS_INLINE VVA_CHECKRESULT TVec get2DBBoxAcceptPoint (const float bbox2d[4], const float minz=0.0f, const float maxz=0.0f) const noexcept {
    return TVec(
      bbox2d[normal.x < 0.0f ? BOX2D_RIGHT : BOX2D_LEFT],
      bbox2d[normal.y < 0.0f ? BOX2D_TOP : BOX2D_BOTTOM],
      (normal.z < 0.0f ? maxz : minz));
  }

  // returns `false` if the box fully is on the back side of the plane
  VVA_ALWAYS_INLINE VVA_CHECKRESULT bool checkBox3D (const float bbox[6]) const noexcept {
    // check reject point
    return (PointDistance(get3DBBoxRejectPoint(bbox)) > 0.0f); // at least partially on a front side?
  }

  // returns `false` if the box fully is on the back side of the plane
  VVA_ALWAYS_INLINE VVA_CHECKRESULT bool checkBox3DInclusive (const float bbox[6]) const noexcept {
    // check reject point
    return (PointDistance(get3DBBoxRejectPoint(bbox)) >= 0.0f); // at least partially on a front side?
  }

  // returns `false` if the box fully is on the back side of the plane
  // returns `true` for non-vertical planes (because our box is 2d)
  VVA_ALWAYS_INLINE VVA_CHECKRESULT bool checkBox2D (const float bbox2d[4]) const noexcept {
    if (normal.z == 0.0f) {
      // vertical plane
      // check reject point
      return (PointDistance(get2DBBoxRejectPoint(bbox2d)) > 0.0f); // at least partially on a front side?
    } else {
      return true;
    }
  }

  // returns `false` if the box fully is on the back side of the plane
  // returns `true` for non-vertical planes (because our box is 2d)
  VVA_ALWAYS_INLINE VVA_CHECKRESULT bool checkBox2DInclusive (const float bbox2d[4]) const noexcept {
    if (normal.z == 0.0f) {
      // vertical plane
      // check reject point
      return (PointDistance(get2DBBoxRejectPoint(bbox2d)) >= 0.0f); // at least partially on a front side?
    } else {
      return true;
    }
  }

  // WARNING! make sure that the following constants are in sync with `TFrustum` ones!
  enum { OUTSIDE = 0, INSIDE = 1, PARTIALLY = -1 };

  // returns one of OUTSIDE, INSIDE, PARTIALLY
  // if the box is touching the plane from inside, it is still assumed to be inside
  VVA_ALWAYS_INLINE VVA_CHECKRESULT int checkBox3DEx (const float bbox[6]) const noexcept {
    // check reject point
    if (PointDistance(get3DBBoxRejectPoint(bbox)) <= 0.0f) return OUTSIDE; // entire box on a back side
    // check accept point
    // if accept point on another side (or on plane), assume intersection
    return (PointDistance(get3DBBoxAcceptPoint(bbox)) < 0.0f ? PARTIALLY : INSIDE);
  }

  // returns one of OUTSIDE, INSIDE, PARTIALLY
  // if the box is touching the plane from inside, it is still assumed to be inside
  // returns PARTIALLY for non-vertical planes (because our box is 2d)
  VVA_ALWAYS_INLINE VVA_CHECKRESULT int checkBox2DEx (const float bbox2d[4]) const noexcept {
    if (normal.z == 0.0f) {
      // check reject point
      if (PointDistance(get2DBBoxRejectPoint(bbox2d)) <= 0.0f) return OUTSIDE; // entire box on a back side
      // check accept point
      // if accept point on another side (or on plane), assume intersection
      return (PointDistance(get2DBBoxAcceptPoint(bbox2d)) < 0.0f ? PARTIALLY : INSIDE);
    } else {
      return PARTIALLY;
    }
  }

  // returns side 0 (front), 1 (back), or 2 (collides)
  // if the box is touching the plane from the front, it is still assumed to be in front
  VVA_ALWAYS_INLINE VVA_CHECKRESULT int Box3DOnSide2 (const float bbox[6]) const noexcept {
    // check reject point
    if (PointDistance(get3DBBoxRejectPoint(bbox)) <= 0.0f) return 1; // entire box is on a back side
    // check accept point
    // if accept point on another side (or on plane), assume intersection
    return (PointDistance(get3DBBoxAcceptPoint(bbox)) < 0.0f ? 2 : 0);
  }

  // returns side 0 (front), 1 (back), or 2 (collides)
  // if the box is touching the plane from the front, it is still assumed to be in front
  // returns 2 for non-vertical planes (because our box is 2d)
  VVA_ALWAYS_INLINE VVA_CHECKRESULT int Box2DOnSide2 (const float bbox2d[4]) const noexcept {
    if (normal.z == 0.0f) {
      // check reject point
      if (PointDistance(get2DBBoxRejectPoint(bbox2d)) <= 0.0f) return 1; // entire box is on a back side
      // check accept point
      // if accept point on another side (or on plane), assume intersection
      return (PointDistance(get2DBBoxAcceptPoint(bbox2d)) < 0.0f ? 2 : 0);
    } else {
      return 2;
    }
  }

  // it does the same accept/reject check, but returns this:
  //   0: Quake source says that this can't happen
  //   1: in front
  //   2: in back
  //   3: in both
  // i.e.
  //   bit 0 is set if some part of the cube is in front, and
  //   bit 1 is set if some part of the cube is in back
  VVA_CHECKRESULT unsigned BoxOnPlaneSide (const TVec &emins, const TVec &emaxs) const noexcept;

  // box point that is furthest *into* the plane (or closest to it)
  VVA_CHECKRESULT TVec get3DBBoxSupportPoint (const float bbox[6], float *pdist=nullptr) const noexcept;
  VVA_CHECKRESULT TVec get2DBBoxSupportPoint (const float bbox2d[4], float *pdist=nullptr, const float z=0.0f) const noexcept;

  // this is used in `ClipPoly`
  // all data is malloced, so you'd better keep this between calls to avoid excessive allocations
  struct ClipWorkData {
  private:
    enum { INLINE_SIZE = 42 };
    int inlsides[INLINE_SIZE];
    float inldots[INLINE_SIZE];
    TVec inlpoints[INLINE_SIZE];

  public:
    int *sides;
    float *dots;
    TVec *points;
    int tbsize;

  public:
    VV_DISABLE_COPY(ClipWorkData)

    inline ClipWorkData () noexcept : sides(&inlsides[0]), dots(&inldots[0]), points(&inlpoints[0]), tbsize(INLINE_SIZE) {}
    inline ~ClipWorkData () noexcept { clear(); }

    inline void clear () noexcept {
      if (sides && sides != &inlsides[0]) Z_Free(sides);
      sides = &inlsides[0];
      if (dots && dots != &inldots[0]) Z_Free(dots);
      dots = &inldots[0];
      if (points && points != &inlpoints[0]) Z_Free(points);
      tbsize = INLINE_SIZE;
    }

    inline void ensure (int newsize) noexcept {
      if (tbsize < newsize) {
        tbsize = (newsize|0x7f)+1;
        if (sides == &inlsides[0]) sides = nullptr;
        sides = (int *)Z_Realloc(sides, tbsize*sizeof(sides[0]));
        if (dots == &inldots[0]) dots = nullptr;
        dots = (float *)Z_Realloc(dots, tbsize*sizeof(dots[0]));
        if (points == &inlpoints[0]) points = nullptr;
        points = (TVec *)Z_Realloc(points, tbsize*sizeof(points[0]));
      }
    }
  };

  // if the poly is *FULLY* coplanar, where `ClipPoly()` should send it?
  enum {
    CoplanarNone = -1,
    CoplanarFront = 0,
    CoplanarBack = 1,
    CoplanarBoth = 2,
  };

  // clip convex polygon to this plane
  // `dest` should have room for at least `vcount+1` vertices (and can be equad to `src`, or `nullptr`)
  // precondition: vcount >= 3
  void ClipPoly (ClipWorkData &wdata, const TVec *src, const int vcount, TVec *destfront, int *frontcount, TVec *destback, int *backcount,
                 const int coplanarSide=CoplanarBoth, const float eps=0.1f) const noexcept;
};

static_assert(__builtin_offsetof(TPlane, dist) == __builtin_offsetof(TPlane, normal.z)+sizeof(float), "TPlane layout fail (0)");
static_assert(sizeof(TPlane) == sizeof(float)*4, "TPlane layout fail (1)");

VVA_ALWAYS_INLINE VVA_PURE vuint32 GetTypeHash (const TPlane &v) noexcept { return joaatHashBuf(&v, 4*sizeof(float)); }

// should be used only for wall planes!
static VVA_ALWAYS_INLINE VVA_OKUNUSED VVA_CHECKRESULT
float PlanesAngle2D (const TPlane *from, const TPlane *to) noexcept {
  const float afrom = VectorAngleYaw(from->normal);
  const float ato = VectorAngleYaw(to->normal);
  return AngleMod(AngleMod(ato-afrom+180.0f)-180.0f);
}

// should be used only for wall planes!
static VVA_ALWAYS_INLINE VVA_OKUNUSED VVA_CHECKRESULT
float PlanesAngle2DFlipTo (const TPlane *from, const TPlane *to) noexcept {
  const float afrom = VectorAngleYaw(from->normal);
  const float ato = VectorAngleYaw(-to->normal);
  return AngleMod(AngleMod(ato-afrom+180.0f)-180.0f);
}


// should be used only for wall planes!
static VVA_ALWAYS_INLINE VVA_OKUNUSED VVA_CHECKRESULT
float PlanesAngle2DSigned (const TPlane *from, const TPlane *to) noexcept {
  const float afrom = VectorAngleYaw(from->normal);
  const float ato = VectorAngleYaw(to->normal);
  return AngleMod(ato-afrom+180.0f)-180.0f;
}

// should be used only for wall planes!
static VVA_ALWAYS_INLINE VVA_OKUNUSED VVA_CHECKRESULT
float PlanesAngle2DSignedFlipTo (const TPlane *from, const TPlane *to) noexcept {
  const float afrom = VectorAngleYaw(from->normal);
  const float ato = VectorAngleYaw(-to->normal);
  return AngleMod(ato-afrom+180.0f)-180.0f;
}
