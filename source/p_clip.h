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
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
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
#define VAVOOM_CLIPPER_USE_FLOAT
//#define VAVOOM_CLIPPER_USE_REAL_ANGLES

#ifdef VAVOOM_CLIPPER_USE_FLOAT
# define  VVC_matan  matan
# define  VVC_AngleMod  AngleMod
# define  VVC_AngleMod180  AngleMod180
#else
# define  VVC_matan  matand
# define  VVC_AngleMod  AngleModD
# define  VVC_AngleMod180  AngleMod180D
#endif

#ifdef VAVOOM_CLIPPER_USE_REAL_ANGLES
# define PointToClipAngle  PointToRealAngle
# define PointToClipAngleZeroOrigin  PointToRealAngleZeroOrigin
# define VV_CLIPPER_FULL_CHECK
#else
# define PointToClipAngle  PointToPseudoAngle
# define PointToClipAngleZeroOrigin  PointToPseudoAngleZeroOrigin
# ifdef VV_CLIPPER_FULL_CHECK
#  error "oops"
# endif
#endif


class VViewClipper {
public:
#ifdef VAVOOM_CLIPPER_USE_FLOAT
  typedef float VFloat;
#else
  typedef double VFloat;
#endif

private:
  //struct VClipNode;
  struct VClipNode {
    VFloat From;
    VFloat To;
    VClipNode *Prev;
    VClipNode *Next;
  };

  VClipNode *FreeClipNodes;
  VClipNode *ClipHead;
  VClipNode *ClipTail;
  TVec Origin;
  float Radius; // for light clipper
  VLevel *Level;
  TFrustum Frustum; // why not?
  bool ClearClipNodesCalled;

  VClipNode *NewClipNode ();
  void RemoveClipNode (VClipNode *Node);
  void DoAddClipRange (VFloat From, VFloat To);

  bool DoIsRangeVisible (const VFloat From, const VFloat To) const;

  bool IsRangeVisibleAngle (const VFloat From, const VFloat To) const;

  void AddClipRangeAngle (const VFloat From, const VFloat To);

  void DoRemoveClipRange (VFloat From, VFloat To);
  void RemoveClipRangeAngle (VFloat From, VFloat To);

public:
  static inline VFloat PointToRealAngleZeroOrigin (const float dx, const float dy) {
    VFloat Ret = VVC_matan(dy, dx);
    if (Ret < (VFloat)0) Ret += (VFloat)360;
    return Ret;
  }

  inline VFloat PointToRealAngle (const float x, const float y) const { return PointToRealAngleZeroOrigin(y-Origin.y, x-Origin.x); }
  inline VFloat PointToRealAngle (const TVec &p) const { return PointToRealAngle(p.x, p.y); }

  // returns the pseudoangle between the line p1 to (infinity, p1.y) and the
  // line from p1 to p2. the pseudoangle has the property that the ordering of
  // points by true angle around p1 and ordering of points by pseudoangle are the
  // same. for clipping exact angles are not needed, only the ordering matters.
  // k8: i found this code in GZDoom.
  static inline VFloat PointToPseudoAngleZeroOrigin (float dx, float dy) {
    if (dx == 0 && dy == 0) return 1;
    VFloat res = dy/(fabsf(dx)+fabsf(dy));
    #if PSEUDOANGLE_FROM_ZERO
    res = (dx < 0 ? 2-res : res)+1;
    return res+(res < 1 ? 3 : -1);
    #else
    return (dx < 0 ? 2-res : res)+1;
    #endif
  }

  inline VFloat PointToPseudoAngle (float x, float y) const { return PointToPseudoAngleZeroOrigin(x-Origin.x, y-Origin.y); }
  inline VFloat PointToPseudoAngle (const TVec &p) const { return PointToPseudoAngle(p.x, p.y); }

public:
  VViewClipper ();
  ~VViewClipper ();

  inline VLevel *GetLevel () { return Level; }
  inline VLevel *GetLevel () const { return Level; }

  inline const TVec &GetOrigin () const { return Origin; }
  inline const TFrustum &GetFrustum () const { return Frustum; }

  // 0: completely outside; >0: completely inside; <0: partially inside
  int CheckSubsectorFrustum (const subsector_t *sub, const unsigned mask=~0u) const;
  bool CheckSegFrustum (const seg_t *seg, const unsigned mask=~0u) const;

  void ClearClipNodes (const TVec &AOrigin, VLevel *ALevel, float aradius=0.0f);


  void ClipResetFrustumPlanes (); // call this after setting up frustum to disable height clipping

  // this is for the case when you already have direction vectors, to speed up things a little
  void ClipInitFrustumPlanes (const TAVec &viewangles, const TVec &viewforward, const TVec &viewright, const TVec &viewup,
                              const float fovx, const float fovy);

  void ClipInitFrustumPlanes (const TAVec &viewangles, const float fovx, const float fovy);

  // call this only on empty clipper (i.e. either new, or after calling `ClearClipNodes()`)
  // this is for the case when you already have direction vectors, to speed up things a little
  void ClipInitFrustumRange (const TAVec &viewangles, const TVec &viewforward,
                             const TVec &viewright, const TVec &viewup,
                             const float fovx, const float fovy);

  void ClipInitFrustumRange (const TAVec &viewangles, const float fovx, const float fovy);


#ifdef VV_CLIPPER_FULL_CHECK
  inline bool ClipIsFull () const {
    return (ClipHead && ClipHead->From == (VFloat)0 && ClipHead->To == (VFloat)360);
  }
#endif

  inline bool ClipIsEmpty () const {
    return !ClipHead;
  }

  inline bool IsRangeVisible (const TVec &vfrom, const TVec &vto) const {
    return IsRangeVisibleAngle(PointToClipAngle(vfrom), PointToClipAngle(vto));
  }

  void ClipToRanges (const VViewClipper &Range);

  inline void AddClipRange (const TVec &vfrom, const TVec &vto) {
    AddClipRangeAngle(PointToClipAngle(vfrom), PointToClipAngle(vto));
  }

  bool ClipIsBBoxVisible (const float BBox[6]) const;
  bool ClipCheckRegion (const subregion_t *region, const subsector_t *sub) const;
  bool ClipCheckSubsector (const subsector_t *sub);

#ifdef CLIENT
  bool ClipLightIsBBoxVisible (const float BBox[6]) const;
  bool ClipLightCheckRegion (const subregion_t *region, const subsector_t *sub, bool asShadow) const;
  bool ClipLightCheckSubsector (const subsector_t *sub, bool asShadow) const;
  // this doesn't do raduis and subsector checks: this is done in `BuildLightVis()`
  bool ClipLightCheckSeg (const seg_t *seg, bool asShadow) const;
#endif

  void ClipAddLine (const TVec v1, const TVec v2);
  void ClipAddBBox (const float BBox[6]);
  void ClipAddSubsectorSegs (const subsector_t *sub, const TPlane *Mirror=nullptr, bool clipAll=false);

#ifdef CLIENT
  // this doesn't check for radius
  void ClipLightAddSubsectorSegs (const subsector_t *sub, bool asShadow, const TPlane *Mirror=nullptr);
#endif

  // debug
  void Dump () const;

private:
  void CheckAddClipSeg (const seg_t *line, const TPlane *Mirror=nullptr, bool clipAll=false);
#ifdef CLIENT
  void CheckLightAddClipSeg (const seg_t *line, const TPlane *Mirror, bool asShadow);
  // light radius should be valid
  int CheckSubsectorLight (const subsector_t *sub) const;
#endif

public:
  static bool IsSegAClosedSomething (const TFrustum *Frustum, const seg_t *seg, const TVec *lorg=nullptr, const float *lrad=nullptr);
};
