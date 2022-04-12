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
#include "vc_object_rtti.cpp"
#define CORE_CHACHAPRNG_C
// it must be disabled, so we can use it in VavoomC
#define CHACHA_C_DISABLE_SSE
#include "../core/crypto/chachaprng_c.h"
static_assert(sizeof(ChaChaR) == 105, "invalid `ChaChaR` size");


//**************************************************************************
//
//  Basic functions
//
//**************************************************************************

IMPLEMENT_FUNCTION(VObject, get_GC_AliveObjects) { RET_INT(gcLastStats.alive); }
IMPLEMENT_FUNCTION(VObject, get_GC_LastCollectedObjects) { RET_INT(gcLastStats.lastCollected); }
IMPLEMENT_FUNCTION(VObject, get_GC_LastCollectDuration) { RET_FLOAT((float)gcLastStats.lastCollectDuration); }
IMPLEMENT_FUNCTION(VObject, get_GC_LastCollectTime) { RET_FLOAT((float)gcLastStats.lastCollectTime); }

IMPLEMENT_FUNCTION(VObject, get_GC_MessagesAllowed) { RET_BOOL(GGCMessagesAllowed); }
IMPLEMENT_FUNCTION(VObject, set_GC_MessagesAllowed) { bool val; vobjGetParam(val); GGCMessagesAllowed = val; }

//WARNING! must be defined by all api users separately!
/*
IMPLEMENT_FUNCTION(VObject, CvarUnlatchAll) {
  if (GGameInfo && GGameInfo->NetMode < NM_DedicatedServer) {
    VCvar::Unlatch();
  }
}
*/


static TMap<VStr, bool> setOfNameSets;


//==========================================================================
//
//  setNamePut
//
//==========================================================================
static bool setNamePut (VName setName, VName value) {
  VStr realName = VStr(va("%s \x01\x01\x01 %s", *setName, *value));
  return setOfNameSets.put(realName, true);
}


//==========================================================================
//
//  setNameCheck
//
//==========================================================================
static bool setNameCheck (VName setName, VName value) {
  VStr realName = VStr(va("%s \x01\x01\x01 %s", *setName, *value));
  return setOfNameSets.has(realName);
}


//==========================================================================
//
//  Object.Destroy
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, Destroy) {
  vobjGetParamSelf();
  if (Self) {
    if (VObject::standaloneExecutor && GImmediadeDelete) {
      delete Self;
    } else {
      Self->ConditionalDestroy();
    }
  }
}


//==========================================================================
//
//  Object.IsA
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, IsA) {
  VName SomeName;
  vobjGetParamSelf(SomeName);
  bool Ret = false;
  if (Self) {
    for (const VClass *c = Self->Class; c; c = c->GetSuperClass()) {
      if (c->GetVName() == SomeName) { Ret = true; break; }
    }
  }
  RET_BOOL(Ret);
}


//==========================================================================
//
//  Object.IsDestroyed
//
//==========================================================================
/*
IMPLEMENT_FUNCTION(VObject, IsDestroyed) {
  vobjGetParamSelf();
  RET_BOOL(Self ? Self->IsGoingToDie() : 1);
}
*/


//==========================================================================
//
//  Object.CollectGarbage
//
//==========================================================================
// static final void CollectGarbage (optional bool destroyDelayed);
IMPLEMENT_FUNCTION(VObject, GC_CollectGarbage) {
  VOptParamBool destroyDelayed(false);
  vobjGetParam(destroyDelayed);
  // meh, `CollectGarbage()` will take care of it
  //if (!VObject::standaloneExecutor) destroyDelayed = false; // anyway
  CollectGarbage(destroyDelayed);
}


//**************************************************************************
//
//  Error functions
//
//**************************************************************************
IMPLEMENT_FUNCTION(VObject, Error) {
  VObject::VMDumpCallStack();
  if (VObject::cliAllErrorsAreFatal) {
    VPackage::SysErrorBuiltin(PF_FormatString());
  } else {
    VPackage::HostErrorBuiltin(PF_FormatString());
  }
}

IMPLEMENT_FUNCTION(VObject, FatalError) {
  VObject::VMDumpCallStack();
  VPackage::SysErrorBuiltin(PF_FormatString());
}

IMPLEMENT_FUNCTION(VObject, AssertError) {
  VStr msg;
  vobjGetParam(msg);
  VObject::VMDumpCallStack();
  VPackage::AssertErrorBuiltin(msg);
}


//**************************************************************************
//
//  Math functions
//
//**************************************************************************
/*
IMPLEMENT_FUNCTION(VObject, AngleMod360) {
  float an;
  vobjGetParam(an);
  if (!isFiniteF(an)) { VObject::VMDumpCallStack(); if (isNaNF(an)) VPackage::InternalFatalError("got NAN"); else VPackage::InternalFatalError("got INF"); }
  RET_FLOAT(AngleMod(an));
}

IMPLEMENT_FUNCTION(VObject, AngleMod180) {
  float an;
  vobjGetParam(an);
  if (!isFiniteF(an)) { VObject::VMDumpCallStack(); if (isNaNF(an)) VPackage::InternalFatalError("got NAN"); else VPackage::InternalFatalError("got INF"); }
  RET_FLOAT(AngleMod180(an));
}
*/

//native static final TVec Project2D (const TVec v1, const TVec v2, const TVec p);
IMPLEMENT_FUNCTION(VObject, Project2D) {
  TVec v1, v2, p;
  vobjGetParam(v1, v2, p);
  RET_VEC(Project2D(v1, v2, p));
}

//native static final float PerpDot2D (const TVec a, const TVec b, const TVec c);
IMPLEMENT_FUNCTION(VObject, PerpDot2D) {
  TVec a, b, c;
  vobjGetParam(a, b, c);
  RET_FLOAT(PerpDot2D(a, b, c));
}

//native static final bool IsPointOnLine2D (const TVec v1, const TVec v2, const TVec p);
IMPLEMENT_FUNCTION(VObject, IsPointOnLine2D) {
  TVec v1, v2, p;
  vobjGetParam(v1, v2, p);
  RET_BOOL(IsPointOnLine2D(v1, v2, p));
}

//native static final bool IsPointOnSeg2D (const TVec v1, const TVec v2, const TVec p);
IMPLEMENT_FUNCTION(VObject, IsPointOnSeg2D) {
  TVec v1, v2, p;
  vobjGetParam(v1, v2, p);
  RET_BOOL(IsPointOnSeg2D(v1, v2, p));
}

//native static final bool IsProjectedPointOnSeg2D (const TVec v1, const TVec v2, const TVec p);
IMPLEMENT_FUNCTION(VObject, IsProjectedPointOnSeg2D) {
  TVec v1, v2, p;
  vobjGetParam(v1, v2, p);
  RET_BOOL(IsProjectedPointOnSeg2D(v1, v2, p));
}

// native static final void AngleVectors (const TAVec angles, out TVec forward, optional out TVec right, optional out TVec up);
IMPLEMENT_FUNCTION(VObject, AngleVectors) {
  // note that optional out will still get a temporary storage
  TVec dummyright, dummyup;
  TVec *vforward;
  VOptParamPtr<TVec> vright(&dummyright), vup(&dummyup);
  TAVec angles;
  vobjGetParam(angles, vforward, vright, vup);
  if (vup.specified || vright.specified) {
    if (!vright) vright.value = &dummyright;
    if (!vup) vup.value = &dummyup;
    AngleVectors(angles, *vforward, *vright, *vup);
  } else {
    *vforward = AngleVector(angles);
  }
}

// native static final void AngleVector (const TAVec angles, out TVec forward);
IMPLEMENT_FUNCTION(VObject, AngleVector) {
  TAVec angles;
  TVec *vforward;
  vobjGetParam(angles, vforward);
  *vforward = AngleVector(angles);
}

// native static final void VectorAngles (const TVec vec, out TAVec angles);
IMPLEMENT_FUNCTION(VObject, VectorAngles) {
  TVec vec;
  TAVec *angles;
  vobjGetParam(vec, angles);
  *angles = VectorAngles(vec);
}

//native static final void DirVectorsAngles (const TVec forward, const TVec right, const TVec up, out TAVec angles);
IMPLEMENT_FUNCTION(VObject, DirVectorsAngles) {
  TVec fwd, right, up;
  TAVec *angles;
  vobjGetParam(fwd, right, up, angles);
  VectorsAngles(fwd, right, up, *angles);
}

// native static final float VectorAngleYaw (const TVec vec);
IMPLEMENT_FUNCTION(VObject, VectorAngleYaw) {
  TVec vec;
  vobjGetParam(vec);
  RET_FLOAT(VectorAngleYaw(vec));
}

// native static final float VectorAnglePitch (const TVec vec);
IMPLEMENT_FUNCTION(VObject, VectorAnglePitch) {
  TVec vec;
  vobjGetParam(vec);
  RET_FLOAT(VectorAnglePitch(vec));
}

// native static final TVec AngleYawVector (const float yaw);
IMPLEMENT_FUNCTION(VObject, AngleYawVector) {
  float yaw;
  vobjGetParam(yaw);
  RET_VEC(AngleVectorYaw(yaw));
}

// native static final TVec AnglePitchVector (const float pitch);
IMPLEMENT_FUNCTION(VObject, AnglePitchVector) {
  float yaw;
  vobjGetParam(yaw);
  RET_VEC(AngleVectorPitch(yaw));
}

// native static final TVec AnglesRightVector (const TAVec angles);
IMPLEMENT_FUNCTION(VObject, AnglesRightVector) {
  TAVec angles;
  vobjGetParam(angles);
  RET_VEC(AnglesRightVector(angles));
}

// native static final TVec YawVectorRight (float yaw);
IMPLEMENT_FUNCTION(VObject, YawVectorRight) {
  float yaw;
  vobjGetParam(yaw);
  RET_VEC(YawVectorRight(yaw));
}

// native static final TVec RotateDirectionVector (const TVec vec, const TAVec rot);
IMPLEMENT_FUNCTION(VObject, RotateDirectionVector) {
  TVec vec;
  TAVec rot;
  vobjGetParam(vec, rot);
  TAVec angles = VectorAngles(vec);
  angles.pitch += rot.pitch;
  angles.yaw += rot.yaw;
  angles.roll += rot.roll;
  RET_VEC(AngleVector(angles));
}

// native static final void VectorRotateAroundZ (ref TVec vec, float angle);
IMPLEMENT_FUNCTION(VObject, VectorRotateAroundZ) {
  TVec *vec;
  float angle;
  vobjGetParam(vec, angle);

  angle = AngleMod(angle);
  const float dstx = vec->x*mcos(angle)-vec->y*msin(angle);
  const float dsty = vec->x*msin(angle)+vec->y*mcos(angle);

  vec->x = dstx;
  vec->y = dsty;
}

// native static final TVec RotateVectorAroundVector (const TVec Vector, const TVec Axis, float angle);
IMPLEMENT_FUNCTION(VObject, RotateVectorAroundVector) {
  TVec vec, axis;
  float angle;
  vobjGetParam(vec, axis, angle);
  RET_VEC(RotateVectorAroundVector(vec, axis, angle));
}

// native static final TVec RotatePointAroundVector (const TVec dir, const TVec point, float degrees);
IMPLEMENT_FUNCTION(VObject, RotatePointAroundVector) {
  TVec dir, point, res;
  float degrees;
  vobjGetParam(dir, point, degrees);
  RotatePointAroundVector(res, dir, point, degrees);
  RET_VEC(res);
}


// native static final float GetPlanePointZ (const ref TPlane plane, const TVec point);
IMPLEMENT_FUNCTION(VObject, GetPlanePointZ) {
  TPlane *plane;
  TVec point;
  vobjGetParam(plane, point);
  const float res = plane->GetPointZ(point);
  if (!isFiniteF(res)) { VObject::VMDumpCallStack(); VPackage::InternalFatalError("invalid call to `GetPlanePointZ()` (probably called with vertical plane)"); }
  RET_FLOAT(res);
}

// native static final float GetPlanePointZRev (const ref TPlane plane, const TVec point);
IMPLEMENT_FUNCTION(VObject, GetPlanePointZRev) {
  TPlane *plane;
  TVec point;
  vobjGetParam(plane, point);
  const float res = plane->GetPointZRev(point);
  if (!isFiniteF(res)) { VObject::VMDumpCallStack(); VPackage::InternalFatalError("invalid call to `GetPlanePointZ()` (probably called with vertical plane)"); }
  RET_FLOAT(res);
}

// native static final int PointOnPlaneSide (const TVec point, const ref TPlane plane);
IMPLEMENT_FUNCTION(VObject, PointOnPlaneSide) {
  TVec point;
  TPlane *plane;
  vobjGetParam(point, plane);
  RET_INT(plane->PointOnSide(point));
}

// native static final int PointOnPlaneSide2 (const TVec point, const ref TPlane plane);
IMPLEMENT_FUNCTION(VObject, PointOnPlaneSide2) {
  TVec point;
  TPlane *plane;
  vobjGetParam(point, plane);
  RET_INT(plane->PointOnSide2(point));
}

// native static final int BoxOnLineSide2DV (const TVec bmin, const TVec bmax, const TVec v1, const TVec v2);
IMPLEMENT_FUNCTION(VObject, BoxOnLineSide2DV) {
  TVec bmin, bmax, v1, v2;
  vobjGetParam(bmin, bmax, v1, v2);
  float tmbox[4];
  if (bmin.y < bmax.y) {
    tmbox[BOX2D_TOP] = bmax.y;
    tmbox[BOX2D_BOTTOM] = bmin.y;
  } else {
    tmbox[BOX2D_TOP] = bmin.y;
    tmbox[BOX2D_BOTTOM] = bmax.y;
  }
  if (bmin.x < bmax.x) {
    tmbox[BOX2D_LEFT] = bmin.x;
    tmbox[BOX2D_RIGHT] = bmax.x;
  } else {
    tmbox[BOX2D_LEFT] = bmax.x;
    tmbox[BOX2D_RIGHT] = bmin.x;
  }
  RET_INT(BoxOnLineSide2D(tmbox, v1, v2));
}

//native static final bool IsPlainFloor (const ref TPlane plane); // valid only for floors
IMPLEMENT_FUNCTION(VObject, IsPlainFloor) {
  TPlane *plane;
  vobjGetParam(plane);
  RET_BOOL(plane->normal.z == 1.0f);
}

//native static final bool IsPlainCeiling (const ref TPlane plane); // valid only for ceilings
IMPLEMENT_FUNCTION(VObject, IsPlainCeiling) {
  TPlane *plane;
  vobjGetParam(plane);
  RET_BOOL(plane->normal.z == -1.0f);
}

//native static final bool IsSlopedFlat (const ref TPlane plane);
IMPLEMENT_FUNCTION(VObject, IsSlopedFlat) {
  TPlane *plane;
  vobjGetParam(plane);
  RET_BOOL(fabsf(plane->normal.z) != 1.0f);
}

//native static final bool IsVerticalPlane (const ref TPlane plane);
IMPLEMENT_FUNCTION(VObject, IsVerticalPlane) {
  TPlane *plane;
  vobjGetParam(plane);
  RET_BOOL(plane->normal.z == 0.0f);
}


// native static final TVec PlaneProjectPoint (const ref TPlane plane, TVec v);
IMPLEMENT_FUNCTION(VObject, PlaneProjectPoint) {
  TPlane *plane;
  TVec v;
  vobjGetParam(plane, v);
  //RET_VEC(v-(v-plane->normal*plane->dist).dot(plane->normal)*plane->normal);
  RET_VEC(plane->Project(v));
}

//native static final float PlanePointDistance (const ref TPlane plane, TVec v);
IMPLEMENT_FUNCTION(VObject, PlanePointDistance) {
  TPlane *plane;
  TVec v;
  vobjGetParam(plane, v);
  RET_FLOAT(plane->PointDistance(v));
}

//native static final float PlaneLineIntersectTime (const ref TPlane plane, TVec v0, TVec v1);
IMPLEMENT_FUNCTION(VObject, PlaneLineIntersectTime) {
  TPlane *plane;
  TVec v0, v1;
  vobjGetParam(plane, v0, v1);
  if (v0 == v1) {
    RET_FLOAT(-1.0f);
  } else {
    RET_FLOAT(plane->LineIntersectTime(v0, v1));
  }
}

//native static final bool PlaneLineIntersect (const ref TPlane plane, TVec v0, TVec v1, out TVec vint);
IMPLEMENT_FUNCTION(VObject, PlaneLineIntersect) {
  TPlane *plane;
  TVec v0, v1;
  TVec *res;
  vobjGetParam(plane, v0, v1, res);
  RET_BOOL(plane->LineIntersectEx(*res, v0, v1));
}

// initialises vertical plane from point and direction
//native static final void PlaneForPointDir (out TPlane plane, TVec point, TVec dir);
IMPLEMENT_FUNCTION(VObject, PlaneForPointDir) {
  TPlane *plane;
  TVec point, dir;
  vobjGetParam(plane, point, dir);
  plane->SetPointDirXY(point, dir);
}

// initialises vertical plane from point and direction
// native static final void PlaneForPointNormal (out TPlane plane, TVec point, TVec norm, optional bool normNormalised);
IMPLEMENT_FUNCTION(VObject, PlaneForPointNormal) {
  TPlane *plane;
  TVec point, norm;
  VOptParamBool normalNormalised(false);
  vobjGetParam(plane, point, norm, normalNormalised);
  if (normalNormalised) {
    plane->SetPointNormal3D(point, norm);
  } else {
    plane->SetPointNormal3DSafe(point, norm);
  }
}

// initialises vertical plane from two points
// native static final void PlaneForLine (out TPlane plane, TVec pointA, TVec pointB);
IMPLEMENT_FUNCTION(VObject, PlaneForLine) {
  TPlane *plane;
  TVec v1, v2;
  vobjGetParam(plane, v1, v2);
  plane->Set2Points(v1, v2);
}


//**************************************************************************
//
//  Name functions
//
//**************************************************************************
// native static final int nameicmp (name s1, name s2);
IMPLEMENT_FUNCTION(VObject, nameicmp) {
  VName s1, s2;
  vobjGetParam(s1, s2);
  RET_INT(VStr::ICmp(*s1, *s2));
}

// native static final int namestricmp (name s1, string s2);
IMPLEMENT_FUNCTION(VObject, namestricmp) {
  VName s1;
  VStr s2;
  vobjGetParam(s1, s2);
  RET_INT(s2.ICmp(*s1));
}

// native static final bool nameEquCI (name s1, name s2);
IMPLEMENT_FUNCTION(VObject, nameEquCI) {
  VName n1, n2;
  vobjGetParam(n1, n2);
  RET_BOOL(VStr::strEquCI(*n1, *n2));
}

// native static final bool nameStrEqu (name s1, string s2) [property(name) strEqu];
IMPLEMENT_FUNCTION(VObject, nameStrEqu) {
  VName n1;
  VStr s2;
  vobjGetParam(n1, s2);
  RET_BOOL(s2.strEqu(*n1));
}

// native static final bool nameStrEquCI (name s1, string s2) [property(name) strEquCI];
IMPLEMENT_FUNCTION(VObject, nameStrEquCI) {
  VName n1;
  VStr s2;
  vobjGetParam(n1, s2);
  RET_BOOL(s2.strEquCI(*n1));
}

// native static final bool nameStartsWith (name s1, name pfx) [property(name) startsWith];
IMPLEMENT_FUNCTION(VObject, nameStartsWith) {
  VName n1, pfx;
  vobjGetParam(n1, pfx);
  if (n1 != NAME_None && pfx != NAME_None) {
    RET_BOOL(VStr::startsWith(*n1, *pfx));
  } else {
    RET_BOOL(false);
  }
}

// native static final bool nameStartsWithCI (name s1, name pfx) [property(name) startsWithCI];
IMPLEMENT_FUNCTION(VObject, nameStartsWithCI) {
  VName n1, pfx;
  vobjGetParam(n1, pfx);
  if (n1 != NAME_None && pfx != NAME_None) {
    RET_BOOL(VStr::startsWithCI(*n1, *pfx));
  } else {
    RET_BOOL(false);
  }
}

// native static final bool nameStrStartsWith (name s1, name pfx) [property(name) startsWithStr];
IMPLEMENT_FUNCTION(VObject, nameStrStartsWith) {
  VName n1;
  VStr pfx;
  vobjGetParam(n1, pfx);
  if (n1 != NAME_None && !pfx.isEmpty()) {
    RET_BOOL(VStr::startsWith(*n1, *pfx));
  } else {
    RET_BOOL(false);
  }
}

// native static final bool nameStrStartsWithCI (name s1, name pfx) [property(name) startsWithStrCI];
IMPLEMENT_FUNCTION(VObject, nameStrStartsWithCI) {
  VName n1;
  VStr pfx;
  vobjGetParam(n1, pfx);
  if (n1 != NAME_None && !pfx.isEmpty()) {
    RET_BOOL(VStr::startsWithCI(*n1, *pfx));
  } else {
    RET_BOOL(false);
  }
}

// native static final bool nameEndsWith (name s1, name sfx) [property(name) endsWith];
IMPLEMENT_FUNCTION(VObject, nameEndsWith) {
  VName n1, sfx;
  vobjGetParam(n1, sfx);
  if (n1 != NAME_None && sfx != NAME_None) {
    RET_BOOL(VStr::endsWith(*n1, *sfx));
  } else {
    RET_BOOL(false);
  }
}

// native static final bool nameEndsWithCI (name s1, name sfx) [property(name) endsWithCI];
IMPLEMENT_FUNCTION(VObject, nameEndsWithCI) {
  VName n1, sfx;
  vobjGetParam(n1, sfx);
  if (n1 != NAME_None && sfx != NAME_None) {
    RET_BOOL(VStr::endsWithCI(*n1, *sfx));
  } else {
    RET_BOOL(false);
  }
}

// native static final bool nameStrEndsWith (name s1, name sfx) [property(name) endsWithStr];
IMPLEMENT_FUNCTION(VObject, nameStrEndsWith) {
  VName n1;
  VStr sfx;
  vobjGetParam(n1, sfx);
  if (n1 != NAME_None && !sfx.isEmpty()) {
    RET_BOOL(VStr::endsWith(*n1, *sfx));
  } else {
    RET_BOOL(false);
  }
}

// native static final bool nameStrEndsWithCI (name s1, name sfx) [property(name) endsWithStrCI];
IMPLEMENT_FUNCTION(VObject, nameStrEndsWithCI) {
  VName n1;
  VStr sfx;
  vobjGetParam(n1, sfx);
  if (n1 != NAME_None && !sfx.isEmpty()) {
    RET_BOOL(VStr::endsWithCI(*n1, *sfx));
  } else {
    RET_BOOL(false);
  }
}


//**************************************************************************
//
//  String functions
//
//**************************************************************************
// native static final bool strEqu (string s1, string s2);
IMPLEMENT_FUNCTION(VObject, strEqu) {
  VStr s1, s2;
  vobjGetParam(s1, s2);
  RET_BOOL(s1.strEqu(s2));
}

// native static final bool strEquCI (string s1, string s2);
IMPLEMENT_FUNCTION(VObject, strEquCI) {
  VStr s1, s2;
  vobjGetParam(s1, s2);
  RET_BOOL(s1.strEquCI(s2));
}

// native static final bool strNameEqu (string s1, name s2) [property(string) nameEqu];
IMPLEMENT_FUNCTION(VObject, strNameEqu) {
  VStr s1;
  VName n2;
  vobjGetParam(s1, n2);
  RET_BOOL(s1.strEqu(*n2));
}

// native static final bool strNameEquCI (string s1, name s2) [property(string) nameEquCI];
IMPLEMENT_FUNCTION(VObject, strNameEquCI) {
  VStr s1;
  VName n2;
  vobjGetParam(s1, n2);
  RET_BOOL(s1.strEquCI(*n2));
}

// native static final int strcmp (string s1, string s2);
IMPLEMENT_FUNCTION(VObject, strcmp) {
  VStr s1, s2;
  vobjGetParam(s1, s2);
  RET_INT(s1.Cmp(s2));
}

// native static final int stricmp (string s1, string s2);
IMPLEMENT_FUNCTION(VObject, stricmp) {
  VStr s1, s2;
  vobjGetParam(s1, s2);
  RET_INT(s1.ICmp(s2));
}

// native static final string strlwr (string s);
IMPLEMENT_FUNCTION(VObject, strlwr) {
  VStr s;
  vobjGetParam(s);
  RET_STR(s.toLowerCase());
}

// native static final string strupr (string s);
IMPLEMENT_FUNCTION(VObject, strupr) {
  VStr s;
  vobjGetParam(s);
  RET_STR(s.toUpperCase());
}

// native static final int strlenutf8 (string s);
IMPLEMENT_FUNCTION(VObject, strlenutf8) {
  VStr s;
  vobjGetParam(s);
  RET_INT(s.Utf8Length());
}

// native static final string substrutf8 (string Str, int Start, int Len);
IMPLEMENT_FUNCTION(VObject, substrutf8) {
  VStr str;
  int start, len;
  vobjGetParam(str, start, len);
  RET_STR(str.Utf8Substring(start, len));
}

//native static final string strmid (string Str, int Start, optional int Len);
IMPLEMENT_FUNCTION(VObject, strmid) {
  VStr s;
  int start;
  VOptParamInt len(0);
  vobjGetParam(s, start, len);
  if (!len.specified) { if (start < 0) start = 0; len = s.length(); }
  RET_STR(s.mid(start, len));
}

//native static final string strleft (string Str, int len);
IMPLEMENT_FUNCTION(VObject, strleft) {
  VStr s;
  int len;
  vobjGetParam(s, len);
  RET_STR(s.left(len));
}

//native static final string strright (string Str, int len);
IMPLEMENT_FUNCTION(VObject, strright) {
  VStr s;
  int len;
  vobjGetParam(s, len);
  RET_STR(s.right(len));
}

//native static final string strrepeat (int len, optinal int ch);
IMPLEMENT_FUNCTION(VObject, strrepeat) {
  int len;
  VOptParamInt ch(32);
  vobjGetParam(len, ch);
  VStr s;
  s.setLength(len, ch);
  RET_STR(s);
}

// Creates one-char non-utf8 string from the given char code&0xff; 0 is allowed
//native static final string strFromChar (int ch);
IMPLEMENT_FUNCTION(VObject, strFromChar) {
  int ch;
  vobjGetParam(ch);
  ch &= 0xff;
  VStr s((char)ch);
  RET_STR(s);
}

// Creates one-char utf8 string from the given char code (or empty string if char code is invalid); 0 is allowed
//native static final string strFromCharUtf8 (int ch);
IMPLEMENT_FUNCTION(VObject, strFromCharUtf8) {
  int ch;
  vobjGetParam(ch);
  VStr s;
  if (ch >= 0 && ch <= 0x10FFFF) s.utf8Append((vuint32)ch);
  RET_STR(s);
}

//native static final string strFromInt (int v);
IMPLEMENT_FUNCTION(VObject, strFromInt) {
  int v;
  vobjGetParam(v);
  VStr s(v);
  RET_STR(s);
}

//native static final string strFromFloat (float v);
IMPLEMENT_FUNCTION(VObject, strFromFloat) {
  float v;
  vobjGetParam(v);
  VStr s(v);
  RET_STR(s);
}

//native static final bool globmatch (string str, string pat, optional bool caseSensitive/*=true*/);
IMPLEMENT_FUNCTION(VObject, globmatch) {
  VStr str, pat;
  VOptParamBool caseSens(true);
  vobjGetParam(str, pat, caseSens);
  RET_BOOL(VStr::globmatch(*str, *pat, caseSens));
}

//native static final float strApproxMatch (string stra, string strb);
IMPLEMENT_FUNCTION(VObject, strApproxMatch) {
  VStr stra, strb;
  vobjGetParam(stra, strb);
  RET_FLOAT(stra.approxMatch(strb));
}

// native static final string va (string format, ...) [printf,1];
IMPLEMENT_FUNCTION(VObject, va) {
  RET_STR(PF_FormatString());
}

// native static final int atoi (string str, optional out bool err);
IMPLEMENT_FUNCTION(VObject, atoi) {
  VStr str;
  VOptParamPtr<int> err(nullptr); // this is actually bool
  vobjGetParam(str, err);
  int res = 0;
  if (str.convertInt(&res)) {
    if (err) *err = 0;
    RET_INT(res);
  } else {
    if (err) *err = 1;
    RET_INT(0);
  }
}

// native static final float atof (string str, optional out bool err);
IMPLEMENT_FUNCTION(VObject, atof) {
  VStr str;
  VOptParamPtr<int> err(nullptr); // this is actually bool
  vobjGetParam(str, err);
  float res = 0;
  if (str.convertFloat(&res)) {
    if (err) *err = 0;
    RET_FLOAT(res);
  } else {
    if (err) *err = 1;
    RET_FLOAT(0);
  }
}

//native static final bool StrStartsWith (string Str, string Check, optional bool caseSensitive/*=true*/);
IMPLEMENT_FUNCTION(VObject, StrStartsWith) {
  VStr Str, Check;
  VOptParamBool caseSens(true);
  vobjGetParam(Str, Check, caseSens);
  if (caseSens) {
    RET_BOOL(Str.StartsWith(Check));
  } else {
    RET_BOOL(Str.startsWithNoCase(Check));
  }
}

//native static final bool StrEndsWith (string Str, string Check, optional bool caseSensitive/*=true*/);
IMPLEMENT_FUNCTION(VObject, StrEndsWith) {
  VStr Str, Check;
  VOptParamBool caseSens(true);
  vobjGetParam(Str, Check, caseSens);
  if (caseSens) {
    RET_BOOL(Str.EndsWith(Check));
  } else {
    RET_BOOL(Str.endsWithNoCase(Check));
  }
}

//native static final bool StrStartsWithCI (string Str, string Check);
IMPLEMENT_FUNCTION(VObject, StrStartsWithCI) {
  VStr Str, Check;
  vobjGetParam(Str, Check);
  RET_BOOL(Str.startsWithNoCase(Check));
}

//native static final bool StrEndsWithCI (string Str, string Check);
IMPLEMENT_FUNCTION(VObject, StrEndsWithCI) {
  VStr Str, Check;
  vobjGetParam(Str, Check);
  RET_BOOL(Str.endsWithNoCase(Check));
}

// native static final string StrReplace (string Str, string Search, string Replacement);
IMPLEMENT_FUNCTION(VObject, StrReplace) {
  VStr Str, Search, Replacement;
  vobjGetParam(Str, Search, Replacement);
  RET_STR(Str.Replace(Search, Replacement));
}

//native static final int strIndexOf (const string str, const string pat, optional int startpos, optional bool caseSensitive/*=true*/);
IMPLEMENT_FUNCTION(VObject, strIndexOf) {
  VStr str, pat;
  VOptParamInt startpos(0);
  VOptParamBool caseSens(true);
  vobjGetParam(str, pat, startpos, caseSens);
  if (!caseSens) { pat = pat.toLowerCase(); str = str.toLowerCase(); }
  RET_INT(str.indexOf(pat, startpos));
}

//native static final int strLastIndexOf (const string str, const string pat, optional int startpos, optional bool caseSensitive/*=true*/);
IMPLEMENT_FUNCTION(VObject, strLastIndexOf) {
  VStr str, pat;
  VOptParamInt startpos(0);
  VOptParamBool caseSens(true);
  vobjGetParam(str, pat, startpos, caseSens);
  if (!caseSens) { pat = pat.toLowerCase(); str = str.toLowerCase(); }
  RET_INT(str.lastIndexOf(pat, startpos));
}

// returns existing name, or ''
//native static final name findExistingName (string s, optional bool locase);
IMPLEMENT_FUNCTION(VObject, findExistingName) {
  VStr str;
  VOptParamBool locase(false);
  vobjGetParam(str, locase);
  VName res;
  if (!str.isEmpty()) res = VName(*str, (locase ? VName::FindLower : VName::Find));
  RET_NAME(res);
}

// native static final string strRemoveColors (string s) [property(string) removeColors];
IMPLEMENT_FUNCTION(VObject, strRemoveColors) {
  VStr str;
  vobjGetParam(str);
  RET_STR(str.RemoveColors());
}

// native static final string strXStrip (string s) [property(string) xstrip];
IMPLEMENT_FUNCTION(VObject, strXStrip) {
  VStr str;
  vobjGetParam(str);
  RET_STR(str.xstrip());
}

// native static final string strTrimAll (string s) [property(string) stripLeft];
IMPLEMENT_FUNCTION(VObject, strTrimAll) {
  VStr str;
  vobjGetParam(str);
  RET_STR(str.xstrip());
}

// native static final string strTrimLeft (string s) [property(string) stripLeft];
IMPLEMENT_FUNCTION(VObject, strTrimLeft) {
  VStr str;
  vobjGetParam(str);
  RET_STR(str.trimLeft());
}

// native static final string strTrimRight (string s) [property(string) stripRight];
IMPLEMENT_FUNCTION(VObject, strTrimRight) {
  VStr str;
  vobjGetParam(str);
  RET_STR(str.trimRight());
}


//**************************************************************************
//
//  Random numbers
//
//**************************************************************************
IMPLEMENT_FUNCTION(VObject, FRandom) {
  RET_FLOAT(FRandom());
}

IMPLEMENT_FUNCTION(VObject, FRandomFull) {
  RET_FLOAT(FRandomFull());
}

IMPLEMENT_FUNCTION(VObject, FRandomFullSlow) {
  RET_FLOAT(FRandomFullSlow());
}

// native static final float FRandomBetweenSlow (float minv, float maxv);
IMPLEMENT_FUNCTION(VObject, FRandomBetweenSlow) {
  float minv, maxv;
  vobjGetParam(minv, maxv);
  RET_FLOAT(FRandomBetweenSlow(minv, maxv));
}

// native static final float FRandomBetween (float minv, float maxv);
IMPLEMENT_FUNCTION(VObject, FRandomBetween) {
  float minv, maxv;
  vobjGetParam(minv, maxv);
  RET_FLOAT(FRandomBetween(minv, maxv));
}

// native static final int IRandomBetweenSlow (int minv, int maxv);
IMPLEMENT_FUNCTION(VObject, IRandomBetweenSlow) {
  int minv, maxv;
  vobjGetParam(minv, maxv);
  RET_INT(IRandomBetweenSlow(minv, maxv));
}

// native static final int IRandomBetween (int minv, int maxv);
IMPLEMENT_FUNCTION(VObject, IRandomBetween) {
  int minv, maxv;
  vobjGetParam(minv, maxv);
  RET_INT(IRandomBetween(minv, maxv));
}

IMPLEMENT_FUNCTION(VObject, GenRandomSeedU32) {
  vint32 rn;
  do { ed25519_randombytes(&rn, sizeof(rn)); } while (!rn);
  RET_INT(rn);
}

IMPLEMENT_FUNCTION(VObject, GenRandomU31) {
  RET_INT(GenRandomU31());
}

IMPLEMENT_FUNCTION(VObject, P_Random) {
  RET_INT(P_Random());
}


// native static final void bjprngSeed (out BJPRNGCtx ctx, int aseed);
IMPLEMENT_FUNCTION(VObject, bjprngSeed) {
  BJPRNGCtx *ctx;
  vuint32 aseed;
  vobjGetParam(ctx, aseed);
  if (ctx) bjprng_raninit(ctx, aseed);
}

// native static final void bjprngSeedRandom (out BJPRNGCtx ctx);
IMPLEMENT_FUNCTION(VObject, bjprngSeedRandom) {
  BJPRNGCtx *ctx;
  vobjGetParam(ctx);
  if (ctx) {
    vuint32 rn;
    do { ed25519_randombytes(&rn, sizeof(rn)); } while (!rn);
    bjprng_raninit(ctx, (vuint32)rn);
  }
}

// full 32-bit value (so it can be negative)
//native static final int bjprngNext (ref BJPRNGCtx ctx);
IMPLEMENT_FUNCTION(VObject, bjprngNext) {
  BJPRNGCtx *ctx;
  vobjGetParam(ctx);
  RET_INT(ctx ? bjprng_ranval(ctx) : 0);
}

// only positives, 31-bit
//native static final int bjprngNextU31 (ref BJPRNGCtx ctx);
IMPLEMENT_FUNCTION(VObject, bjprngNextU31) {
  BJPRNGCtx *ctx;
  vobjGetParam(ctx);
  RET_INT(ctx ? bjprng_ranval(ctx)&0x7fffffffu : 0);
}

// [0..1) (WARNING! not really uniform!)
//native static final float bjprngNextFloat (ref BJPRNGCtx ctx);
IMPLEMENT_FUNCTION(VObject, bjprngNextFloat) {
  BJPRNGCtx *ctx;
  vobjGetParam(ctx);
  if (ctx) {
    for (;;) {
      float v = ((double)bjprng_ranval(ctx))/((double)0xffffffffu);
      if (v < 1.0f) { RET_FLOAT(v); return; }
    }
  } else {
    RET_FLOAT(0);
  }
}

// [0..1] (WARNING! not really uniform!)
//native static final float bjprngNextFloatFull (ref BJPRNGCtx ctx);
IMPLEMENT_FUNCTION(VObject, bjprngNextFloatFull) {
  BJPRNGCtx *ctx;
  vobjGetParam(ctx);
  if (ctx) {
    const float v = ((double)bjprng_ranval(ctx))/((double)0xffffffffu);
    RET_FLOAT(v);
  } else {
    RET_FLOAT(0);
  }
}


// native static final void pcg3264Seed (out PCG3264_Ctx ctx, int aseed);
IMPLEMENT_FUNCTION(VObject, pcg3264Seed) {
  PCG3264_Ctx *ctx;
  vuint32 aseed;
  vobjGetParam(ctx, aseed);
  if (ctx) pcg3264_seedU32(ctx, aseed);
}

// native static final void pcg3264SeedRandom (out PCG3264_Ctx ctx);
IMPLEMENT_FUNCTION(VObject, pcg3264SeedRandom) {
  PCG3264_Ctx *ctx;
  vobjGetParam(ctx);
  if (ctx) ed25519_randombytes(ctx, sizeof(PCG3264_Ctx));
}

// full 32-bit value (so it can be negative)
//native static final int pcg3264Next (ref PCG3264_Ctx ctx);
IMPLEMENT_FUNCTION(VObject, pcg3264Next) {
  PCG3264_Ctx *ctx;
  vobjGetParam(ctx);
  RET_INT(ctx ? pcg3264_next(ctx) : 0);
}

// only positives, 31-bit
//native static final int pcg3264NextU31 (ref PCG3264_Ctx ctx);
IMPLEMENT_FUNCTION(VObject, pcg3264NextU31) {
  PCG3264_Ctx *ctx;
  vobjGetParam(ctx);
  RET_INT(ctx ? pcg3264_next(ctx)&0x7fffffffu : 0);
}

// [0..1) (WARNING! not really uniform!)
//native static final float pcg3264NextFloat (ref PCG3264_Ctx ctx);
IMPLEMENT_FUNCTION(VObject, pcg3264NextFloat) {
  PCG3264_Ctx *ctx;
  vobjGetParam(ctx);
  if (ctx) {
    for (;;) {
      float v = ((double)pcg3264_next(ctx))/((double)0xffffffffu);
      if (v < 1.0f) { RET_FLOAT(v); return; }
    }
  } else {
    RET_FLOAT(0);
  }
}

// [0..1] (WARNING! not really uniform!)
//native static final float pcg3264NextFloatFull (ref PCG3264_Ctx ctx);
IMPLEMENT_FUNCTION(VObject, pcg3264NextFloatFull) {
  PCG3264_Ctx *ctx;
  vobjGetParam(ctx);
  if (ctx) {
    const float v = ((double)pcg3264_next(ctx))/((double)0xffffffffu);
    RET_FLOAT(v);
  } else {
    RET_FLOAT(0);
  }
}


// native static final bool chachaIsValid (const ref ChaChaCtx ctx);
IMPLEMENT_FUNCTION(VObject, chachaIsValid) {
  ChaChaR *ctx;
  vobjGetParam(ctx);
  RET_BOOL(ctx ? (ctx->rounds > 0 && ctx->rounds <= 64 && (ctx->rounds&1) == 0) : false);
}

// native static final int chachaGetRounds (const ref ChaChaCtx ctx);
IMPLEMENT_FUNCTION(VObject, chachaGetRounds) {
  ChaChaR *ctx;
  vobjGetParam(ctx);
  RET_INT(ctx ? ctx->rounds : 0);
}

// seed with a random seed (chacha20)
// native static final void chachaSeedRandom (out ChaChaCtx ctx);
IMPLEMENT_FUNCTION(VObject, chachaSeedRandom) {
  ChaChaR *ctx;
  vobjGetParam(ctx);
  if (ctx) {
    vuint64 seedval, stream;
    ed25519_randombytes(&seedval, sizeof(seedval));
    ed25519_randombytes(&stream, sizeof(stream));
    if (chacha_init_ex(ctx, seedval, stream, CHACHA_DEFAULT_ROUNDS) != 0) ctx->rounds = 0;
  }
}

// seed with the given seed (chacha20)
// native static final void chachaSeed (out ChaChaCtx ctx, int aseed);
IMPLEMENT_FUNCTION(VObject, chachaSeed) {
  ChaChaR *ctx;
  vuint32 aseed;
  vobjGetParam(ctx, aseed);
  if (ctx) {
    if (chacha_init(ctx, (vuint32)aseed) != 0) ctx->rounds = 0;
  }
}

// seed with the given seeds and rounds
// native static final void chachaSeedEx (out ChaChaCtx ctx, int aseed0, int aseed1, int astream0, int astream1, int rounds);
IMPLEMENT_FUNCTION(VObject, chachaSeedEx) {
  ChaChaR *ctx;
  vuint32 aseed0, aseed1;
  vuint32 astream0, astream1;
  int rounds;
  vobjGetParam(ctx, aseed0, aseed1, astream0, astream1, rounds);
  if (ctx) {
    vuint64 seedval = (vuint32)aseed1;
    seedval <<= 32;
    seedval |= (vuint32)aseed0;
    vuint64 stream = (vuint32)astream1;
    stream <<= 32;
    stream |= (vuint32)astream0;
    if (chacha_init_ex(ctx, seedval, stream, rounds) != 0) ctx->rounds = 0;
  }
}

// full 32-bit value (so it can be negative)
// native static final int chachaNext (ref ChaChaCtx ctx);
IMPLEMENT_FUNCTION(VObject, chachaNext) {
  ChaChaR *ctx;
  vobjGetParam(ctx);
  RET_INT(ctx ? chacha_next(ctx) : 0);
}

// only positives, 31-bit
// native static final int chachaNextU31 (ref ChaChaCtx ctx);
IMPLEMENT_FUNCTION(VObject, chachaNextU31) {
  ChaChaR *ctx;
  vobjGetParam(ctx);
  RET_INT(ctx ? chacha_next(ctx)&0x7fffffffu : 0);
}

// [0..1) (WARNING! not really uniform!)
// native static final float chachaNextFloat (ref ChaChaCtx ctx);
IMPLEMENT_FUNCTION(VObject, chachaNextFloat) {
  ChaChaR *ctx;
  vobjGetParam(ctx);
  if (ctx) {
    for (;;) {
      float v = ((double)chacha_next(ctx))/((double)0xffffffffu);
      if (v < 1.0f) { RET_FLOAT(v); return; }
    }
  } else {
    RET_FLOAT(0);
  }
}

// [0..1] (WARNING! not really uniform!)
// native static final float chachaNextFloatFull (ref ChaChaCtx ctx);
IMPLEMENT_FUNCTION(VObject, chachaNextFloatFull) {
  ChaChaR *ctx;
  vobjGetParam(ctx);
  if (ctx) {
    const float v = ((double)chacha_next(ctx))/((double)0xffffffffu);
    RET_FLOAT(v);
  } else {
    RET_FLOAT(0);
  }
}


//==========================================================================
//
//  Printing in console
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, print) {
  VStr s = PF_FormatString();
  PR_DoWriteBuf(*s);
  PR_DoWriteBuf(nullptr);
}

IMPLEMENT_FUNCTION(VObject, dprint) {
  VStr s = PF_FormatString();
  PR_DoWriteBuf(*s, true); // debug
  PR_DoWriteBuf(nullptr, true); // debug
}

IMPLEMENT_FUNCTION(VObject, printwarn) {
  VStr s = PF_FormatString();
  PR_DoWriteBuf(*s, false, NAME_Warning);
  PR_DoWriteBuf(nullptr, false, NAME_Warning);
}

IMPLEMENT_FUNCTION(VObject, printerror) {
  VStr s = PF_FormatString();
  PR_DoWriteBuf(*s, false, NAME_Error);
  PR_DoWriteBuf(nullptr, false, NAME_Error);
}

IMPLEMENT_FUNCTION(VObject, printdebug) {
  VStr s = PF_FormatString();
  PR_DoWriteBuf(*s, false, NAME_Debug);
  PR_DoWriteBuf(nullptr, false, NAME_Debug);
}

// WARNING! keep in sync with C++ code!
enum PrintMsg {
  PMSG_Log,
  PMSG_Warning,
  PMSG_Error,
  PMSG_Debug,
  PMSG_Init,
  PMSG_DevNet,
  //
  PMSG_Bot,
  PMSG_BotDev,
  PMSG_BotDevAI,
  PMSG_BotDevRoam,
  PMSG_BotDevCheckPos,
  PMSG_BotDevItems,
  PMSG_BotDevAttack,
  PMSG_BotDevPath,
  PMSG_BotDevCrumbs,
  PMSG_BotDevPlanPath,
};

IMPLEMENT_FUNCTION(VObject, printmsg) {
  int type;
  VStr s = PF_FormatString();
  vobjGetParam(type);
  EName nn = NAME_Log;
  switch (type) {
    case PMSG_Log: nn = NAME_Log; break;
    case PMSG_Warning: nn = NAME_Warning; break;
    case PMSG_Error: nn = NAME_Error; break;
    case PMSG_Debug: nn = NAME_Debug; break;
    case PMSG_Init: nn = NAME_Init; break;
    case PMSG_DevNet: nn = NAME_DevNet; break;
    //
    case PMSG_Bot: nn = NAME_Bot; break;
    case PMSG_BotDev: nn = NAME_BotDev; break;
    case PMSG_BotDevAI: nn = NAME_BotDevAI; break;
    case PMSG_BotDevRoam: nn = NAME_BotDevRoam; break;
    case PMSG_BotDevCheckPos: nn = NAME_BotDevCheckPos; break;
    case PMSG_BotDevItems: nn = NAME_BotDevItems; break;
    case PMSG_BotDevAttack: nn = NAME_BotDevAttack; break;
    case PMSG_BotDevPath: nn = NAME_BotDevPath; break;
    case PMSG_BotDevCrumbs: nn = NAME_BotDevCrumbs; break;
    case PMSG_BotDevPlanPath: nn = NAME_BotDevPlanPath; break;
  }
  PR_DoWriteBuf(*s, false, nn);
  PR_DoWriteBuf(nullptr, false, nn);
}


//==========================================================================
//
//  Class methods
//
//==========================================================================
IMPLEMENT_FUNCTION(VObject, FindClass) {
  VName Name;
  vobjGetParam(Name);
  RET_PTR(VClass::FindClass(*Name));
}

IMPLEMENT_FUNCTION(VObject, FindClassNoCase) {
  VName Name;
  vobjGetParam(Name);
  RET_PTR(VClass::FindClassNoCase(*Name));
}

IMPLEMENT_FUNCTION(VObject, FindClassNoCaseStr) {
  VStr Name;
  vobjGetParam(Name);
  RET_PTR(VClass::FindClassNoCase(*Name));
}

//native static final class FindClassNoCaseEx (class BaseClass, name Name);
IMPLEMENT_FUNCTION(VObject, FindClassNoCaseEx) {
  VClass *BaseClass;
  VName Name;
  vobjGetParam(BaseClass, Name);
  VClass *cls = VClass::FindClassNoCase(*Name);
  if (cls && BaseClass && !cls->IsChildOf(BaseClass)) {
    GLog.Logf(NAME_Warning, "FindClassNoCaseEx: class `%s` is not a child of `%s`", cls->GetName(), BaseClass->GetName());
    cls = nullptr;
  }
  RET_PTR(cls);
}

//native static final class FindClassNoCaseExStr (class BaseClass, string Name);
IMPLEMENT_FUNCTION(VObject, FindClassNoCaseExStr) {
  VClass *BaseClass;
  VStr Name;
  vobjGetParam(BaseClass, Name);
  VClass *cls = VClass::FindClassNoCase(*Name);
  if (cls && BaseClass && !cls->IsChildOf(BaseClass)) {
    GLog.Logf(NAME_Warning, "FindClassNoCaseExStr: class `%s` is not a child of `%s`", cls->GetName(), BaseClass->GetName());
    cls = nullptr;
  }
  RET_PTR(cls);
}

// native static final bool ClassIsChildOf (class SomeClass, class BaseClass);
IMPLEMENT_FUNCTION(VObject, ClassIsChildOf) {
  VClass *SomeClass, *BaseClass;
  vobjGetParam(SomeClass, BaseClass);
  RET_BOOL(SomeClass && BaseClass ? SomeClass->IsChildOf(BaseClass) : false);
}

IMPLEMENT_FUNCTION(VObject, GetClassName) {
  VClass *SomeClass;
  vobjGetParam(SomeClass);
  RET_NAME(SomeClass ? SomeClass->Name : NAME_None);
}

IMPLEMENT_FUNCTION(VObject, GetClassLocationStr) {
  VClass *SomeClass;
  vobjGetParam(SomeClass);
  RET_STR(SomeClass ? SomeClass->Loc.toStringNoCol() : VStr::EmptyString);
}

IMPLEMENT_FUNCTION(VObject, GetFullClassName) {
  VClass *SomeClass;
  vobjGetParam(SomeClass);
  RET_STR(SomeClass ? SomeClass->GetFullName() : VStr::EmptyString);
}

IMPLEMENT_FUNCTION(VObject, IsAbstractClass) {
  VClass *SomeClass;
  vobjGetParam(SomeClass);
  if (SomeClass) {
    RET_BOOL(!!(SomeClass->ClassFlags&CLASS_Abstract));
  } else {
    RET_BOOL(false);
  }
}

IMPLEMENT_FUNCTION(VObject, IsNativeClass) {
  VClass *SomeClass;
  vobjGetParam(SomeClass);
  if (SomeClass) {
    RET_BOOL(!!(SomeClass->ClassFlags&CLASS_Native));
  } else {
    RET_BOOL(false);
  }
}

IMPLEMENT_FUNCTION(VObject, GetClassParent) {
  VClass *SomeClass;
  vobjGetParam(SomeClass);
  RET_PTR(SomeClass ? SomeClass->ParentClass : nullptr);
}

IMPLEMENT_FUNCTION(VObject, GetClassReplacement) {
  VClass *SomeClass;
  vobjGetParam(SomeClass);
  RET_PTR(SomeClass ? SomeClass->GetReplacement() : nullptr);
}

// returns `none` if replacement is not compatible with `CType`
// native static final spawner class GetCompatibleClassReplacement (class CType, class C);
IMPLEMENT_FUNCTION(VObject, GetCompatibleClassReplacement) {
  VClass *CType, *SomeClass;
  vobjGetParam(CType, SomeClass);
  VClass *nc = (SomeClass ? SomeClass->GetReplacement() : nullptr);
  if (nc && CType && !nc->IsChildOf(CType)) nc = nullptr;
  RET_PTR(nc);
}

IMPLEMENT_FUNCTION(VObject, GetClassReplacee) {
  VClass *SomeClass;
  vobjGetParam(SomeClass);
  RET_PTR(SomeClass ? SomeClass->GetReplacee() : nullptr);
}

// native static final state FindClassState (class C, name StateName, optional name SubLabel, optional bool exact);
IMPLEMENT_FUNCTION(VObject, FindClassState) {
  VClass *Cls;
  VName StateName;
  VOptParamName SubLabel(NAME_None);
  VOptParamBool Exact(false);
  vobjGetParam(Cls, StateName, SubLabel, Exact);
  VStateLabel *Lbl = (Cls ? Cls->FindStateLabel(StateName, SubLabel, Exact) : nullptr);
  RET_PTR(Lbl ? Lbl->State : nullptr);
}

// native static final int GetClassNumOwnedStates (class C);
IMPLEMENT_FUNCTION(VObject, GetClassNumOwnedStates) {
  VClass *Cls;
  vobjGetParam(Cls);
  int Ret = 0;
  if (Cls) for (VState *S = Cls->States; S; S = S->Next) ++Ret;
  RET_INT(Ret);
}

// native static final state GetClassFirstState (class C);
IMPLEMENT_FUNCTION(VObject, GetClassFirstState) {
  VClass *Cls;
  vobjGetParam(Cls);
  RET_PTR(Cls ? Cls->States : nullptr);
}

// native static final state GetNoJumpState ();
IMPLEMENT_FUNCTION(VObject, GetNoJumpState) {
  RET_PTR(VState::GetNoJumpState());
}

// native static final name GetClassGameObjName (class C);
IMPLEMENT_FUNCTION(VObject, GetClassGameObjName) {
  VClass *SomeClass;
  vobjGetParam(SomeClass);
  RET_NAME(SomeClass ? SomeClass->ClassGameObjName : NAME_None);
}

// native static final int GetClassInstanceCount (class C);
IMPLEMENT_FUNCTION(VObject, GetClassInstanceCount) {
  VClass *SomeClass;
  vobjGetParam(SomeClass);
  RET_INT(SomeClass ? SomeClass->InstanceCount : 0);
}

// native static final int GetClassInstanceCountWithSub (class C);
IMPLEMENT_FUNCTION(VObject, GetClassInstanceCountWithSub) {
  VClass *SomeClass;
  vobjGetParam(SomeClass);
  RET_INT(SomeClass ? SomeClass->InstanceCountWithSub : 0);
}

// native static final int GetClassInstanceSize (class C);
IMPLEMENT_FUNCTION(VObject, GetClassInstanceSize) {
  VClass *SomeClass;
  vobjGetParam(SomeClass);
  RET_INT(SomeClass ? SomeClass->ClassSize : 0);
}


//native final static class FindMObjId (int id, optional int GameFilter);
IMPLEMENT_FUNCTION(VObject, FindMObjId) {
  int id;
  VOptParamInt gf(-1);
  vobjGetParam(id, gf);
  auto nfo = VClass::FindMObjId(id, gf);
  RET_REF(nfo ? nfo->Class : nullptr);
}

//native final static class FindScriptId (int id, optional int GameFilter);
IMPLEMENT_FUNCTION(VObject, FindScriptId) {
  int id;
  VOptParamInt gf(-1);
  vobjGetParam(id, gf);
  auto nfo = VClass::FindScriptId(id, gf);
  RET_REF(nfo ? nfo->Class : nullptr);
}

// native final static class FindClassByGameObjName (name aname, optional name pkgname);
IMPLEMENT_FUNCTION(VObject, FindClassByGameObjName) {
  VName aname;
  VOptParamName pkgname(NAME_None);
  vobjGetParam(aname, pkgname);
  RET_REF(VMemberBase::StaticFindClassByGameObjName(aname, pkgname));
}


//==========================================================================
//
//  State methods
//
//==========================================================================
// native static final string GetStateLocationStr (state State);
IMPLEMENT_FUNCTION(VObject, GetStateLocationStr) {
  VState *State;
  vobjGetParam(State);
  RET_STR(State ? State->Loc.toStringNoCol() : VStr::EmptyString);
}

// native static final string GetFullStateName (state State);
IMPLEMENT_FUNCTION(VObject, GetFullStateName) {
  VState *State;
  vobjGetParam(State);
  RET_STR(State ? State->GetFullName() : VStr::EmptyString);
}

// native static final bool StateIsInRange (state State, state Start, state End, int MaxDepth);
IMPLEMENT_FUNCTION(VObject, StateIsInRange) {
  VState *State, *Start, *End;
  int MaxDepth;
  vobjGetParam(State, Start, End, MaxDepth);
  RET_BOOL(State ? State->IsInRange(Start, End, MaxDepth) : false);
}

// native static final bool StateIsInSequence (state State, state Start);
IMPLEMENT_FUNCTION(VObject, StateIsInSequence) {
  VState *State, *Start;
  vobjGetParam(State, Start);
  RET_BOOL(State ? State->IsInSequence(Start) : false);
}

// native static final name GetStateSpriteName (state State);
IMPLEMENT_FUNCTION(VObject, GetStateSpriteName) {
  VState *State;
  vobjGetParam(State);
  RET_NAME(State ? State->SpriteName : NAME_None);
}

// native static final int GetStateSpriteFrame (state State);
IMPLEMENT_FUNCTION(VObject, GetStateSpriteFrame) {
  VState *State;
  vobjGetParam(State);
  RET_INT(State ? State->Frame : 0);
}

// native static final int GetStateSpriteFrameWidth (state State);
IMPLEMENT_FUNCTION(VObject, GetStateSpriteFrameWidth) {
  VState *State;
  vobjGetParam(State);
  RET_INT(State ? State->frameWidth : 0);
}

// native static final int GetStateSpriteFrameHeight (state State);
IMPLEMENT_FUNCTION(VObject, GetStateSpriteFrameHeight) {
  VState *State;
  vobjGetParam(State);
  RET_INT(State ? State->frameHeight : 0);
}

// native static final void GetStateSpriteFrameSize (state State, out int w, out int h);
IMPLEMENT_FUNCTION(VObject, GetStateSpriteFrameSize) {
  VState *State;
  int *w, *h;
  vobjGetParam(State, w, h);
  if (State) {
    if (w) *w = State->frameWidth;
    if (h) *h = State->frameHeight;
  } else {
    if (w) *w = 0;
    if (h) *h = 0;
  }
}

// native static final float GetStateDuration (state State);
IMPLEMENT_FUNCTION(VObject, GetStateDuration) {
  VState *State;
  vobjGetParam(State);
  RET_FLOAT(State ? State->Time : 0.0f);
}

// native static final state GetStatePlus (state S, int Offset, optional bool IgnoreJump/*=true*/);
IMPLEMENT_FUNCTION(VObject, GetStatePlus) {
  VState *State;
  int Offset;
  VOptParamBool IgnoreJump(true);
  vobjGetParam(State, Offset, IgnoreJump);
  RET_PTR(State ? State->GetPlus(Offset, IgnoreJump) : nullptr);
}

// native static final bool StateHasAction (state State);
IMPLEMENT_FUNCTION(VObject, StateHasAction) {
  VState *State;
  vobjGetParam(State);
  RET_BOOL(State ? !!State->Function : false);
}

// native static final void CallStateAction (Object actobj, state State);
IMPLEMENT_FUNCTION(VObject, CallStateAction) {
  VObject *obj;
  VState *State;
  vobjGetParam(obj, State);
  if (State && State->Function) {
    P_PASS_REF(obj);
    (void)ExecuteFunction(State->Function);
  }
}

// native static final state GetNextState (state State);
// by execution
IMPLEMENT_FUNCTION(VObject, GetNextState) {
  VState *State;
  vobjGetParam(State);
  RET_PTR(State ? State->NextState : nullptr);
}

// native static final state GetNextStateInProg (state State);
// by declaration
IMPLEMENT_FUNCTION(VObject, GetNextStateInProg) {
  VState *State;
  vobjGetParam(State);
  RET_PTR(State ? State->Next : nullptr);
}

// native static final int GetStateSpriteFrameOfsX (state State);
IMPLEMENT_FUNCTION(VObject, GetStateSpriteFrameOfsX) {
  VState *State;
  vobjGetParam(State);
  RET_INT(State ? State->frameOfsX : 0);
}

// native static final int GetStateSpriteFrameOfsY (state State);
IMPLEMENT_FUNCTION(VObject, GetStateSpriteFrameOfsY) {
  VState *State;
  vobjGetParam(State);
  RET_INT(State ? State->frameOfsY : 0);
}

// native static final void GetStateSpriteFrameOffset (state State, out int dx, out int dy);
IMPLEMENT_FUNCTION(VObject, GetStateSpriteFrameOffset) {
  VState *State;
  int *dx, *dy;
  vobjGetParam(State, dx, dy);
  if (State) {
    if (dx) *dx = State->frameOfsX;
    if (dy) *dy = State->frameOfsY;
  } else {
    if (dx) *dx = 0;
    if (dy) *dy = 0;
  }
}

// native static final int GetStateMisc1 (state State);
IMPLEMENT_FUNCTION(VObject, GetStateMisc1) {
  VState *State;
  vobjGetParam(State);
  RET_INT(State ? State->Misc1 : 0);
}

// native static final int GetStateMisc2 (state State);
IMPLEMENT_FUNCTION(VObject, GetStateMisc2) {
  VState *State;
  vobjGetParam(State);
  RET_INT(State ? State->Misc2 : 0);
}

// native static final void SetStateMisc1 (state State, int v);
IMPLEMENT_FUNCTION(VObject, SetStateMisc1) {
  VState *State;
  int v;
  vobjGetParam(State, v);
  if (State) State->Misc1 = v;
}

// native static final void SetStateMisc2 (state State, int v);
IMPLEMENT_FUNCTION(VObject, SetStateMisc2) {
  VState *State;
  int v;
  vobjGetParam(State, v);
  if (State) State->Misc2 = v;
}

// native static final StateFrameType GetStateTicKind (state State);
IMPLEMENT_FUNCTION(VObject, GetStateTicKind) {
  VState *State;
  vobjGetParam(State);
  RET_INT(State ? State->TicType : 0);
}

// native static final int GetStateArgN (state State, int argn);
IMPLEMENT_FUNCTION(VObject, GetStateArgN) {
  VState *State;
  int argn;
  vobjGetParam(State, argn);
  if (State && argn >= 0 && argn <= 1) {
    RET_INT(argn == 0 ? State->Arg1 : State->Arg2);
  } else {
    RET_INT(0);
  }
}

// native static final void SetStateArgN (state State, int argn, int v);
IMPLEMENT_FUNCTION(VObject, SetStateArgN) {
  VState *State;
  int argn, v;
  vobjGetParam(State, argn, v);
  if (State && argn >= 0 && argn <= 1) {
    if (argn == 0) State->Arg1 = v; else State->Arg2 = v;
  }
}

// native static final int GetStateFRN (state State);
IMPLEMENT_FUNCTION(VObject, GetStateFRN) {
  VState *State;
  vobjGetParam(State);
  RET_INT(State ? State->frameAction : 0);
}

// native static final void SetStateFRN (state State, int v);
IMPLEMENT_FUNCTION(VObject, SetStateFRN) {
  VState *State;
  int v;
  vobjGetParam(State, v);
  if (State) State->frameAction = v;
}


//==========================================================================
//
//  iterators
//
//==========================================================================
// native static final iterator AllClasses (class BaseClass, out class Class);
IMPLEMENT_FUNCTION(VObject, AllClasses) {
  VClass *BaseClass;
  VClass **Class;
  vobjGetParam(BaseClass, Class);
  RET_PTR(new VClassesIterator(BaseClass, Class));
}

// native static final iterator AllObjects (class BaseClass, out Object Obj);
IMPLEMENT_FUNCTION(VObject, AllObjects) {
  VClass *BaseClass;
  VObject **Obj;
  vobjGetParam(BaseClass, Obj);
  RET_PTR(new VObjectsIterator(BaseClass, Obj));
}

// native static final iterator AllClassStates (class BaseClass, out state State);
IMPLEMENT_FUNCTION(VObject, AllClassStates) {
  VClass *BaseClass;
  VState **aout;
  vobjGetParam(BaseClass, aout);
  RET_PTR(new VClassStatesIterator(BaseClass, aout));
}


//==========================================================================
//
//  Misc
//
//==========================================================================
// default `skipReplacement` is:
//   `false` for k8vavoom and VCC
//   `false` for vccrun
// native static final spawner Object SpawnObject (class cid, optional bool skipReplacement);
IMPLEMENT_FUNCTION(VObject, SpawnObject) {
  VClass *Class;
  VOptParamBool skipReplacement(false); //FIXME: `true` for k8vavoom?
  vobjGetParam(Class, skipReplacement);
  if (!Class) { VMDumpCallStack(); VPackage::InternalFatalError("Cannot spawn `none`"); }
  if (skipReplacement) {
    if (Class->ClassFlags&CLASS_Abstract) { VMDumpCallStack(); VPackage::InternalFatalError("Cannot spawn abstract object"); }
  } else {
    if (Class->GetReplacement()->ClassFlags&CLASS_Abstract) { VMDumpCallStack(); VPackage::InternalFatalError("Cannot spawn abstract object"); }
  }
  RET_REF(VObject::StaticSpawnObject(Class, skipReplacement));
}


#include <time.h>
#include <sys/time.h>

#ifdef _WIN32
//# if __GNUC__ < 6
static inline struct tm *localtime_r (const time_t *_Time, struct tm *_Tm) {
  return (localtime_s(_Tm, _Time) ? NULL : _Tm);
}
//# endif
#endif


struct TTimeVal {
  vint32 secs; // actually, unsigned
  vint32 usecs;
  // for 2030+
  vint32 secshi;
};


struct TDateTime {
  vint32 sec; // [0..60] (yes, *sometimes* it can be 60)
  vint32 min; // [0..59]
  vint32 hour; // [0..23]
  vint32 month; // [0..11]
  vint32 year; // normal value, i.e. 2042 for 2042
  vint32 mday; // [1..31] -- day of the month
  //
  vint32 wday; // [0..6] -- day of the week (0 is sunday)
  vint32 yday; // [0..365] -- day of the year
  vint32 isdst; // is daylight saving time?
};

//native static final bool GetTimeOfDay (out TTimeVal tv);
IMPLEMENT_FUNCTION(VObject, GetTimeOfDay) {
  TTimeVal *tvres;
  vobjGetParam(tvres);
  tvres->secshi = 0;
  timeval tv;
  if (gettimeofday(&tv, nullptr)) {
    tvres->secs = 0;
    tvres->usecs = 0;
    RET_BOOL(false);
  } else {
    tvres->secs = (int)(tv.tv_sec&0xffffffff);
    tvres->usecs = (int)tv.tv_usec;
    tvres->secshi = (int)(((uint64_t)tv.tv_sec)>>32);
    RET_BOOL(true);
  }
}


//native static final bool DecodeTimeVal (out TDateTime tm, const ref TTimeVal tv);
IMPLEMENT_FUNCTION(VObject, DecodeTimeVal) {
  TDateTime *tmres;
  TTimeVal *tvin;
  vobjGetParam(tmres, tvin);
  timeval tv;
  tv.tv_sec = (((uint64_t)tvin->secs)&0xffffffff)|(((uint64_t)tvin->secshi)<<32);
  //tv.tv_usec = tvin->usecs;
  tm ctm;
#ifndef STK_TIMET_FIX
  if (localtime_r(&tv.tv_sec, &ctm)) {
#else
  time_t tsec = tv.tv_sec;
  if (localtime_r(&tsec, &ctm)) {
#endif
    tmres->sec = ctm.tm_sec;
    tmres->min = ctm.tm_min;
    tmres->hour = ctm.tm_hour;
    tmres->month = ctm.tm_mon;
    tmres->year = ctm.tm_year+1900;
    tmres->mday = ctm.tm_mday;
    tmres->wday = ctm.tm_wday;
    tmres->yday = ctm.tm_yday;
    tmres->isdst = ctm.tm_isdst;
    RET_BOOL(true);
  } else {
    memset(tmres, 0, sizeof(*tmres));
    RET_BOOL(false);
  }
}


//native static final bool EncodeTimeVal (out TTimeVal tv, ref TDateTime tm, optional bool usedst);
IMPLEMENT_FUNCTION(VObject, EncodeTimeVal) {
  TTimeVal *tvres;
  TDateTime *tmin;
  VOptParamBool usedst(false);
  vobjGetParam(tvres, tmin, usedst);
  tm ctm;
  memset(&ctm, 0, sizeof(ctm));
  ctm.tm_sec = tmin->sec;
  ctm.tm_min = tmin->min;
  ctm.tm_hour = tmin->hour;
  ctm.tm_mon = tmin->month;
  ctm.tm_year = tmin->year-1900;
  ctm.tm_mday = tmin->mday;
  //ctm.tm_wday = tmin->wday;
  //ctm.tm_yday = tmin->yday;
  ctm.tm_isdst = tmin->isdst;
  if (!usedst) ctm.tm_isdst = -1;
  auto tt = mktime(&ctm);
  if (tt == (time_t)-1) {
    // oops
    memset(tvres, 0, sizeof(*tvres));
    RET_BOOL(false);
  } else {
    // update it
    tmin->sec = ctm.tm_sec;
    tmin->min = ctm.tm_min;
    tmin->hour = ctm.tm_hour;
    tmin->month = ctm.tm_mon;
    tmin->year = ctm.tm_year+1900;
    tmin->mday = ctm.tm_mday;
    tmin->wday = ctm.tm_wday;
    tmin->yday = ctm.tm_yday;
    tmin->isdst = ctm.tm_isdst;
    // setup tvres
    tvres->secs = (int)(tt&0xffffffff);
    tvres->usecs = 0;
    tvres->secshi = (int)(((uint64_t)tt)>>32);
    RET_BOOL(true);
  }
}


// ////////////////////////////////////////////////////////////////////////// //
//static native final string GetInputKeyName (int kcode);
IMPLEMENT_FUNCTION(VObject, GetInputKeyStrName) {
  int kcode;
  vobjGetParam(kcode);
  RET_STR(VObject::NameFromVKey(kcode));
}


//static native final int GetInputKeyCode (string kname);
IMPLEMENT_FUNCTION(VObject, GetInputKeyCode) {
  VStr kname;
  vobjGetParam(kname);
  RET_INT(VObject::VKeyFromName(kname));
}


//**************************************************************************
//
//  Cvar functions
//
//**************************************************************************
// native static final bool CvarExists (name Name);
/*
IMPLEMENT_FUNCTION(VObject, CvarExists) {
  VName name;
  vobjGetParam(name);
  RET_BOOL(VCvar::HasVar(*name));
}
*/

// native static final int CvarGetFlags (name Name);
IMPLEMENT_FUNCTION(VObject, CvarGetFlags) {
  VName name;
  vobjGetParam(name);
  RET_INT(VCvar::GetVarFlags(*name));
}

// native static final string GetCvarHelp (name Name);
IMPLEMENT_FUNCTION(VObject, GetCvarHelp) {
  VName name;
  vobjGetParam(name);
  RET_STR(VStr(VCvar::GetHelp(*name)));
}

// native static final string GetCvarDefault (name Name);
IMPLEMENT_FUNCTION(VObject, GetCvarDefault) {
  VName name;
  vobjGetParam(name);
  VCvar *cvar = VCvar::FindVariable(*name);
  if (!cvar) {
    RET_STR(VStr::EmptyString);
  } else {
    RET_STR(VStr(cvar->GetDefault()));
  }
}

// native static final void CreateCvar (name Name, string defaultValue, string helpText, optional int flags);
IMPLEMENT_FUNCTION(VObject, CreateCvar) {
  VName name;
  VStr def, help;
  VOptParamInt flags(0);
  vobjGetParam(name, def, help, flags);
  VCvar::CreateNew(name, def, help, flags);
}

// native static final void CreateCvarInt (name Name, int defaultValue, string helpText, optional int flags);
IMPLEMENT_FUNCTION(VObject, CreateCvarInt) {
  VName name;
  int def;
  VStr help;
  VOptParamInt flags(0);
  vobjGetParam(name, def, help, flags);
  VCvar::CreateNewInt(name, def, help, flags);
}

// native static final void CreateCvarFloat (name Name, float defaultValue, string helpText, optional int flags);
IMPLEMENT_FUNCTION(VObject, CreateCvarFloat) {
  VName name;
  float def;
  VStr help;
  VOptParamInt flags(0);
  vobjGetParam(name, def, help, flags);
  VCvar::CreateNewFloat(name, def, help, flags);
}

// native static final void CreateCvarBool (name Name, bool defaultValue, string helpText, optional int flags);
IMPLEMENT_FUNCTION(VObject, CreateCvarBool) {
  VName name;
  bool def;
  VStr help;
  VOptParamInt flags(0);
  vobjGetParam(name, def, help, flags);
  VCvar::CreateNewBool(name, def, help, flags);
}

// native static final void CreateCvarStr (name Name, string defaultValue, string helpText, optional int flags);
IMPLEMENT_FUNCTION(VObject, CreateCvarStr) {
  VName name;
  VStr def;
  VStr help;
  VOptParamInt flags(0);
  vobjGetParam(name, def, help, flags);
  VCvar::CreateNewStr(name, def, help, flags);
}

/*
// native static final int GetCvar (name Name);
IMPLEMENT_FUNCTION(VObject, GetCvar) {
  VName name;
  vobjGetParam(name);
  RET_INT(VCvar::GetInt(*name));
}

// native static final void SetCvar (name Name, int value);
IMPLEMENT_FUNCTION(VObject, SetCvar) {
  VName name;
  int value;
  vobjGetParam(name, value);
  VCvar::Set(*name, value);
}

// native static final float GetCvarF (name Name);
IMPLEMENT_FUNCTION(VObject, GetCvarF) {
  VName name;
  vobjGetParam(name);
  RET_FLOAT(VCvar::GetFloat(*name));
}

// native static final void SetCvarF (name Name, float value);
IMPLEMENT_FUNCTION(VObject, SetCvarF) {
  VName name;
  float value;
  vobjGetParam(name, value);
  VCvar::Set(*name, value);
}

// native static final string GetCvarS (name Name);
IMPLEMENT_FUNCTION(VObject, GetCvarS) {
  VName name;
  vobjGetParam(name);
  RET_STR(VCvar::GetString(*name));
}

// native static final void SetCvarS (name Name, string value);
IMPLEMENT_FUNCTION(VObject, SetCvarS) {
  VName name;
  VStr value;
  vobjGetParam(name, value);
  VCvar::Set(*name, value);
}

// native static final bool GetCvarB (name Name);
IMPLEMENT_FUNCTION(VObject, GetCvarB) {
  VName name;
  vobjGetParam(name);
  RET_BOOL(VCvar::GetBool(*name));
}

// native static final void SetCvarB (name Name, bool value);
IMPLEMENT_FUNCTION(VObject, SetCvarB) {
  VName name;
  bool value;
  vobjGetParam(name, value);
  VCvar::Set(*name, value ? 1 : 0);
}
*/


IMPLEMENT_FUNCTION(VObject, SetShadowCvar) {
  VName name;
  int value;
  vobjGetParam(name, value);
  VCvar *cvar = VCvar::FindVariable(*name);
  if (cvar) cvar->SetShadow(value);
}

IMPLEMENT_FUNCTION(VObject, SetShadowCvarF) {
  VName name;
  float value;
  vobjGetParam(name, value);
  VCvar *cvar = VCvar::FindVariable(*name);
  if (cvar) cvar->SetShadow(value);
}

IMPLEMENT_FUNCTION(VObject, SetShadowCvarS) {
  VName name;
  VStr value;
  vobjGetParam(name, value);
  VCvar *cvar = VCvar::FindVariable(*name);
  if (cvar) cvar->SetShadow(value);
}

IMPLEMENT_FUNCTION(VObject, SetShadowCvarB) {
  VName name;
  bool value;
  vobjGetParam(name, value);
  VCvar *cvar = VCvar::FindVariable(*name);
  if (cvar) cvar->SetShadow(value ? 1 : 0);
}

IMPLEMENT_FUNCTION(VObject, CvarIsReadOnly) {
  VName name;
  vobjGetParam(name);
  VCvar *cvar = VCvar::FindVariable(*name);
  if (cvar) RET_BOOL(cvar->IsShadowed() || cvar->IsReadOnly()); else RET_BOOL(true);
}


// ////////////////////////////////////////////////////////////////////////// //
// temporary name set functions
// used to prevent warning spam
// ////////////////////////////////////////////////////////////////////////// //

// native static final bool SetNamePutElement (name setName, name value);
IMPLEMENT_FUNCTION(VObject, SetNamePutElement) {
  VName setName, value;
  vobjGetParam(setName, value);
  RET_BOOL(setNamePut(setName, value));
}


// native static final bool SetNameCheckElement (name setName, name value);
IMPLEMENT_FUNCTION(VObject, SetNameCheckElement) {
  VName setName, value;
  vobjGetParam(setName, value);
  RET_BOOL(setNameCheck(setName, value));
}


// ////////////////////////////////////////////////////////////////////////// //
static TVec RayLineIntersection2D (TVec rayO, TVec rayDir, TVec vv1, TVec vv2) {
  rayDir.z = 0; // not interested
  //rayDir = Normalise(rayDir);
  const float den = (vv2.y-vv1.y)*rayDir.x-(vv2.x-vv1.x)*rayDir.y;
  //if (fabsf(den) < 0.00001f) return rayO;
  const float e = rayO.y-vv1.y;
  const float f = rayO.x-vv1.x;
  const float invden = 1.0f/den;
  if (!isFiniteF(invden)) return rayO;
  const float ua = ((vv2.x-vv1.x)*e-(vv2.y-vv1.y)*f)*invden;
  if (ua >= 0 && ua <= 1) {
    const float ub = (rayDir.x*e-rayDir.y*f)*invden;
    if (ua != 0 || ub != 0) {
      TVec res;
      res.x = rayO.x+ua*rayDir.x;
      res.y = rayO.y+ua*rayDir.y;
      res.z = rayO.z;
      return res;
    }
  }
  return rayO;
}


// native static final TVec RayLineIntersection2D (TVec rayO, TVec rayE, TVec vv1, TVec vv2);
IMPLEMENT_FUNCTION(VObject, RayLineIntersection2D) {
  TVec rayO, rayE, vv1, vv2;
  vobjGetParam(rayO, rayE, vv1, vv2);
  const TVec rayDir = rayE-rayO;
  RET_VEC(RayLineIntersection2D(rayO, rayDir, vv1, vv2));
}

// native static final TVec RayLineIntersection2DDir (TVec rayO, TVec rayDir, TVec vv1, TVec vv2);
IMPLEMENT_FUNCTION(VObject, RayLineIntersection2DDir) {
  TVec rayO, rayDir, vv1, vv2;
  vobjGetParam(rayO, rayDir, vv1, vv2);
  RET_VEC(RayLineIntersection2D(rayO, rayDir, vv1, vv2));
}


//==========================================================================
//
//  Events
//
//==========================================================================
// native static final bool PostEvent (const ref event_t ev);
IMPLEMENT_FUNCTION(VObject, PostEvent) {
  event_t *ev;
  vobjGetParam(ev);
  RET_BOOL(ev ? VObject::PostEvent(*ev) : false);
}

// native static final bool InsertEvent (const ref event_t ev);
IMPLEMENT_FUNCTION(VObject, InsertEvent) {
  event_t *ev;
  vobjGetParam(ev);
  RET_BOOL(ev ? VObject::InsertEvent(*ev) : false);
}

// native static final int CountQueuedEvents ();
IMPLEMENT_FUNCTION(VObject, CountQueuedEvents) {
  RET_INT(VObject::CountQueuedEvents());
}

// native static final bool PeekEvent (int idx, out event_t ev);
IMPLEMENT_FUNCTION(VObject, PeekEvent) {
  int idx;
  event_t *ev;
  vobjGetParam(idx, ev);
  RET_BOOL(ev ? VObject::PeekEvent(idx, ev) : false);
}

// native static final bool GetEvent (out event_t ev);
IMPLEMENT_FUNCTION(VObject, GetEvent) {
  event_t *ev;
  vobjGetParam(ev);
  RET_BOOL(ev ? VObject::GetEvent(ev) : false);
}

// native static final int GetEventQueueSize ();
IMPLEMENT_FUNCTION(VObject, GetEventQueueSize) {
  RET_INT(VObject::GetEventQueueSize());
}


//**************************************************************************
//
//  event_t
//
//**************************************************************************
// native void clear ();
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, event_t, clear) {
  event_t *ev;
  vobjGetParam(ev);
  if (ev) ev->clear();
}

//native bool isAnyMouse () const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, event_t, isAnyMouse) {
  event_t *ev;
  vobjGetParam(ev);
  RET_BOOL(ev ? ev->isAnyMouseEvent() : false);
}

// default is both press and release
//native bool isMouseButton (ref event_t evt, optional bool down/*=undefined*/) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, event_t, isMouseButton) {
  event_t *ev;
  VOptParamBool down(true);
  vobjGetParam(ev, down);
  if (!ev) { RET_BOOL(false); return; }
  if (down.specified) {
    RET_BOOL(down ? ev->isAnyMouseButtonDownEvent() : ev->isAnyMouseButtonUpEvent());
  } else {
    RET_BOOL(ev->isAnyMouseButtonEvent());
  }
}



// ////////////////////////////////////////////////////////////////////////// //
// fltconv
// ugly names are intentional
//native static final int fltconv_getsign (float v); // -1 or +1
IMPLEMENT_FUNCTION(VObject, fltconv_getsign) {
  float f;
  vobjGetParam(f);
  RET_INT(fltconv_getsign(f));
}

//native static final int fltconv_getexponent (float v); // always positive, [0..255]
IMPLEMENT_FUNCTION(VObject, fltconv_getexponent) {
  float f;
  vobjGetParam(f);
  RET_INT(fltconv_getexponent(f));
}

//native static final int fltconv_getmantissa (float v); // always positive, [0..0x7f_ffff] (or [0..8388607])
IMPLEMENT_FUNCTION(VObject, fltconv_getmantissa) {
  float f;
  vobjGetParam(f);
  RET_INT(fltconv_getmantissa(f));
}

// invalid values leads to NaN with undefined payload and sign
//native static final float fltconv_constructfloat (int sign, int exponent, int mantissa);
IMPLEMENT_FUNCTION(VObject, fltconv_constructfloat) {
  int sign, exponent, mantissa;
  vobjGetParam(sign, exponent, mantissa);
  RET_FLOAT(fltconv_constructfloat(sign, exponent, mantissa));
}



// ////////////////////////////////////////////////////////////////////////// //
//#include "vc_zastar.h"
#include "vc_zastar.cpp"


// ////////////////////////////////////////////////////////////////////////// //
// colors
IMPLEMENT_FUNCTION(VObject, RGB) {
  int r, g, b;
  vobjGetParam(r, g, b);
  RET_INT((vint32)(0xff000000u|(((vuint32)clampToByte(r))<<16)|(((vuint32)clampToByte(g))<<8)|((vuint32)clampToByte(b))));
}

IMPLEMENT_FUNCTION(VObject, RGBA) {
  int r, g, b, a;
  vobjGetParam(r, g, b, a);
  RET_INT((vint32)((((vuint32)clampToByte(a))<<24)|(((vuint32)clampToByte(r))<<16)|(((vuint32)clampToByte(g))<<8)|((vuint32)clampToByte(b))));
}

IMPLEMENT_FUNCTION(VObject, RGBf) {
  float fr, fg, fb;
  vobjGetParam(fr, fg, fb);
  const vuint32 r = clampToByte((int)(clampval(fr, 0.0f, 1.0f)*255.0f));
  const vuint32 g = clampToByte((int)(clampval(fg, 0.0f, 1.0f)*255.0f));
  const vuint32 b = clampToByte((int)(clampval(fb, 0.0f, 1.0f)*255.0f));
  RET_INT((vint32)(0xff000000u|(r<<16)|(g<<8)|b));
}

IMPLEMENT_FUNCTION(VObject, RGBAf) {
  float fr, fg, fb, fa;
  vobjGetParam(fr, fg, fb, fa);
  const vuint32 r = clampToByte((int)(clampval(fr, 0.0f, 1.0f)*255.0f));
  const vuint32 g = clampToByte((int)(clampval(fg, 0.0f, 1.0f)*255.0f));
  const vuint32 b = clampToByte((int)(clampval(fb, 0.0f, 1.0f)*255.0f));
  const vuint32 a = clampToByte((int)(clampval(fa, 0.0f, 1.0f)*255.0f));
  RET_INT((vint32)((a<<24)|(r<<16)|(g<<8)|b));
}


#define CREATE_RGBA_ELEMENT_GETTERS_SETTERS(name_,shift_)  \
  /* ubyte gettter */ \
  IMPLEMENT_FUNCTION(VObject, RGBGet##name_) { \
    vuint32 clr; \
    vobjGetParam(clr); \
    RET_BYTE((clr>>(shift_))&0xffu); \
  }\
  /* int setter */ \
  IMPLEMENT_FUNCTION(VObject, RGBSet##name_) { \
    vuint32 clr; \
    int v; \
    vobjGetParam(clr, v); \
    clr &= ~(0xffu<<(shift_)); \
    clr |= ((vuint32)(clampToByte(v)))<<(shift_); \
    RET_INT(clr); \
  } \
  /* float getter */ \
  IMPLEMENT_FUNCTION(VObject, RGBGet##name_##f) { \
    vuint32 clr; \
    vobjGetParam(clr); \
    RET_FLOAT(((clr>>(shift_))&0xffu)/255.0f); \
  } \
  /* float setter */ \
  IMPLEMENT_FUNCTION(VObject, RGBSet##name_##f) { \
    vuint32 clr; \
    float v; \
    vobjGetParam(clr, v); \
    clr &= ~(0xffu<<(shift_)); \
    clr |= ((vuint32)(clampToByte((int)(clampval(v, 0.0f, 1.0f)*255.0f))))<<(shift_); \
    RET_INT(clr); \
  } \

CREATE_RGBA_ELEMENT_GETTERS_SETTERS(A, 24)
CREATE_RGBA_ELEMENT_GETTERS_SETTERS(R, 16)
CREATE_RGBA_ELEMENT_GETTERS_SETTERS(G, 8)
CREATE_RGBA_ELEMENT_GETTERS_SETTERS(B, 0)

#undef CREATE_RGBA_ELEMENT_GETTERS_SETTERS


// native static final void RGB2HSV (int rgb, out float h, out float s, out float v);
IMPLEMENT_FUNCTION(VObject, RGB2HSV) {
  vuint32 clr;
  float r, g, b;
  float *h;
  float *s;
  float *v;
  float tmph, tmps, tmpv;
  vobjGetParam(clr, h, s, v);
  if (!h) h = &tmph;
  if (!s) s = &tmps;
  if (!v) v = &tmpv;
  UnpackRGBf(clr, r, g, b);
  M_RgbToHsv(r, g, b, *h, *s, *v);
}

// native static final int HSV2RGB (float h, float s, float v);
IMPLEMENT_FUNCTION(VObject, HSV2RGB) {
  float r, g, b;
  float h, s, v;
  vobjGetParam(h, s, v);
  M_HsvToRgb(h, s, v, r, g, b);
  RET_INT((vint32)PackRGBf(r, g, b));
}

// native static final void RGB2HSL (int rgb, out float h, out float s, out float l);
IMPLEMENT_FUNCTION(VObject, RGB2HSL) {
  vuint32 clr;
  float r, g, b;
  float *h;
  float *s;
  float *l;
  float tmph, tmps, tmpv;
  vobjGetParam(clr, h, s, l);
  if (!h) h = &tmph;
  if (!s) s = &tmps;
  if (!l) l = &tmpv;
  UnpackRGBf(clr, r, g, b);
  M_RgbToHsl(r, g, b, *h, *s, *l);
}

// native static final int HSL2RGB (float h, float s, float l);
IMPLEMENT_FUNCTION(VObject, HSL2RGB) {
  float r, g, b;
  float h, s, l;
  vobjGetParam(h, s, l);
  M_HslToRgb(h, s, l, r, g, b);
  RET_INT((vint32)PackRGBf(r, g, b));
}


// native static final int RGBDistanceSquared (int c0, int c1);
IMPLEMENT_FUNCTION(VObject, RGBDistanceSquared) {
  vuint32 c0, c1;
  vobjGetParam(c0, c1);
  vuint8 r0, g0, b0;
  vuint8 r1, g1, b1;
  UnpackRGB(c0, r0, g0, b0);
  UnpackRGB(c1, r1, g1, b1);
  RET_INT(rgbDistanceSquared(r0, g0, b0, r1, g1, b1));
}

// native static final ubyte sRGBGamma (float v);
IMPLEMENT_FUNCTION(VObject, sRGBGamma) {
  float v;
  vobjGetParam(v);
  RET_BYTE(clampToByte(sRGBgamma(v)));
}

// native static final float sRGBUngamma (int v);
IMPLEMENT_FUNCTION(VObject, sRGBUngamma) {
  int v;
  vobjGetParam(v);
  RET_FLOAT((float)sRGBungamma(clampToByte(v)));
}

// native static final ubyte sRGBIntensity (int clr);
IMPLEMENT_FUNCTION(VObject, sRGBIntensity) {
  vuint32 clr;
  vuint8 r, g, b;
  vobjGetParam(clr);
  UnpackRGB(clr, r, g, b);
  RET_BYTE(colorIntensity(r, g, b));
}


//**************************************************************************
//
//  bounding box creation
//
//**************************************************************************
//native static final void CreateDoomBox2D (out TVec bmin, out TVec bmax, const TVec origin, const float radius);
IMPLEMENT_FREE_FUNCTION(VObject, CreateDoomBox2D) {
  TVec *bmin;
  TVec *bmax;
  TVec origin;
  float radius;
  vobjGetParam(bmin, bmax, origin, radius);
  radius = fabsf(radius);
  *bmin = TVec(origin.x-radius, origin.y-radius);
  *bmax = TVec(origin.x+radius, origin.y+radius);
}

//native static final void CreateDoomBox3D (out TVec bmin, out TVec bmax, const TVec origin, const float radius, const float height);
IMPLEMENT_FREE_FUNCTION(VObject, CreateDoomBox3D) {
  TVec *bmin;
  TVec *bmax;
  TVec origin;
  float radius, height;
  vobjGetParam(bmin, bmax, origin, radius, height);
  if (height < 0.0f) { origin.z -= height; height = -height; }
  radius = fabsf(radius);
  *bmin = TVec(origin.x-radius, origin.y-radius, origin.z);
  *bmax = TVec(origin.x+radius, origin.y+radius, origin.z+height);
}


//**************************************************************************
//
//  TVec
//
//**************************************************************************
//native bool isZero () const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TVec, isZero) {
  TVec *Self;
  vobjGetParam(Self);
  RET_BOOL(Self->isZero());
}

//native bool isZero2D () const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TVec, isZero2D) {
  TVec *Self;
  vobjGetParam(Self);
  RET_BOOL(Self->isZero2D());
}

//native TVec closestPointOnBox3D (const TVec bmin, const TVec bmax) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TVec, closestPointOnBox3D) {
  TVec *Self;
  TVec bmin, bmax;
  vobjGetParam(Self, bmin, bmax);
  Create3DBBoxFromVectors(bbox, bmin, bmax);
  RET_VEC(Self->closestPointOnBBox3D(bbox));
}

//native TVec closestPointOnDoomBox3D (const TVec origin, const float radius, const float height) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TVec, closestPointOnDoomBox3D) {
  TVec *Self;
  TVec origin;
  float radius, height;
  vobjGetParam(Self, origin, radius, height);
  if (height < 0.0f) { origin.z -= height; height = -height; }
  radius = fabsf(radius);
  CreateDoom3DBBox(bbox, origin, radius, height);
  RET_VEC(Self->closestPointOnBBox3D(bbox));
}

//native TVec closestPointOnBox2D (const TVec bmin, const TVec bmax) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TVec, closestPointOnBox2D) {
  TVec *Self;
  TVec bmin, bmax;
  vobjGetParam(Self, bmin, bmax);
  Create2DBBoxFromVectors(bbox, bmin, bmax);
  RET_VEC(Self->closestPointOnBBox2D(bbox));
}

//native TVec closestPointOnDoomBox2D (const TVec origin, const float radius) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TVec, closestPointOnDoomBox2D) {
  TVec *Self;
  TVec origin;
  float radius;
  vobjGetParam(Self, origin, radius);
  radius = fabsf(radius);
  CreateDoom2DBBox(bbox, origin, radius);
  RET_VEC(Self->closestPointOnBBox2D(bbox));
}

//native float box3DDistanceSquared (const TVec bmin, const TVec bmax) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TVec, box3DDistanceSquared) {
  TVec *Self;
  TVec bmin, bmax;
  vobjGetParam(Self, bmin, bmax);
  Create3DBBoxFromVectors(bbox, bmin, bmax);
  RET_FLOAT(Self->bbox3DDistanceSquared(bbox));
}

//native float doomBox3DDistanceSquared (const TVec origin, const float radius, const float height) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TVec, doomBox3DDistanceSquared) {
  TVec *Self;
  TVec origin;
  float radius, height;
  vobjGetParam(Self, origin, radius, height);
  if (height < 0.0f) { origin.z -= height; height = -height; }
  radius = fabsf(radius);
  CreateDoom3DBBox(bbox, origin, radius, height);
  RET_FLOAT(Self->bbox3DDistanceSquared(bbox));
}

//native float box2DDistanceSquared (const TVec bmin, const TVec bmax) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TVec, box2DDistanceSquared) {
  TVec *Self;
  TVec bmin, bmax;
  vobjGetParam(Self, bmin, bmax);
  Create2DBBoxFromVectors(bbox, bmin, bmax);
  RET_FLOAT(Self->bbox2DDistanceSquared(bbox));
}

//native float doomBox2DDistanceSquared (const TVec origin, const float radius) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TVec, doomBox2DDistanceSquared) {
  TVec *Self;
  TVec origin;
  float radius;
  vobjGetParam(Self, origin, radius);
  radius = fabsf(radius);
  CreateDoom2DBBox(bbox, origin, radius);
  RET_FLOAT(Self->bbox2DDistanceSquared(bbox));
}

//native TVec box3DSupportPoint (const TVec bmin, const TVec bmax) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TVec, box3DSupportPoint) {
  TVec *Self;
  TVec bmin, bmax;
  vobjGetParam(Self, bmin, bmax);
  Create3DBBoxFromVectors(bbox, bmin, bmax);
  RET_VEC(Self->get3DBBoxSupportPoint(bbox));
}

//native TVec doomBox3DSupportPoint (const TVec origin, const float radius, const float height) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TVec, doomBox3DSupportPoint) {
  TVec *Self;
  TVec origin;
  float radius, height;
  vobjGetParam(Self, origin, radius, height);
  if (height < 0.0f) { origin.z -= height; height = -height; }
  radius = fabsf(radius);
  CreateDoom3DBBox(bbox, origin, radius, height);
  RET_VEC(Self->get3DBBoxSupportPoint(bbox));
}

//native TVec box2DSupportPoint (const TVec bmin, const TVec bmax) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TVec, box2DSupportPoint) {
  TVec *Self;
  TVec bmin, bmax;
  vobjGetParam(Self, bmin, bmax);
  Create2DBBoxFromVectors(bbox, bmin, bmax);
  RET_VEC(Self->get2DBBoxSupportPoint(bbox));
}

//native TVec doomBox2DSupportPoint (const TVec origin, const float radius) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TVec, doomBox2DSupportPoint) {
  TVec *Self;
  TVec origin;
  float radius;
  vobjGetParam(Self, origin, radius);
  radius = fabsf(radius);
  CreateDoom2DBBox(bbox, origin, radius);
  RET_VEC(Self->get2DBBoxSupportPoint(bbox));
}



//**************************************************************************
//
//  TPlane
//
//**************************************************************************
//native float PointDistance (const TVec p) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, PointDistance) {
  TPlane *Self;
  TVec p;
  vobjGetParam(Self, p);
  RET_FLOAT(Self->PointDistance(p));
}

//native void SetPointDirXY (const TVec point, const TVec dir);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, SetPointDirXY) {
  TPlane *Self;
  TVec p, d;
  vobjGetParam(Self, p, d);
  RET_VOID(Self->SetPointDirXY(p, d));
}

//native void Set2Points (const TVec v1, const TVec v2);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, Set2Points) {
  TPlane *Self;
  TVec v1, v2;
  vobjGetParam(Self, v1, v2);
  RET_VOID(Self->Set2Points(v1, v2));
}

//native void SetPointNormal3D (const TVec point, const TVec norm);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, SetPointNormal3D) {
  TPlane *Self;
  TVec p, n;
  vobjGetParam(Self, p, n);
  RET_VOID(Self->SetPointNormal3D(p, n));
}

//native void SetPointNormal3DSafe (const TVec point, const TVec dir);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, SetPointNormal3DSafe) {
  TPlane *Self;
  TVec p, d;
  vobjGetParam(Self, p, d);
  RET_VOID(Self->SetPointNormal3DSafe(p, d));
}

//native void SetFromTriangle (const TVec a, const TVec b, const TVec c);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, SetFromTriangle) {
  TPlane *Self;
  TVec a, b, c;
  vobjGetParam(Self, a, b, c);
  RET_VOID(Self->SetFromTriangle(a, b, c));
}

//native bool isFloor () const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, isFloor) {
  TPlane *Self;
  vobjGetParam(Self);
  RET_BOOL(Self->isFloor());
}

//native bool isCeiling () const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, isCeiling) {
  TPlane *Self;
  vobjGetParam(Self);
  RET_BOOL(Self->isCeiling());
}

//native bool isSlope () const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, isSlope) {
  TPlane *Self;
  vobjGetParam(Self);
  RET_BOOL(Self->isSlope());
}

//native bool isWall () const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, isWall) {
  TPlane *Self;
  vobjGetParam(Self);
  RET_BOOL(Self->isWall());
}

//native float GetRealDist () const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, GetRealDist) {
  TPlane *Self;
  vobjGetParam(Self);
  RET_FLOAT(Self->GetRealDist());
}

//native void NormaliseInPlace ();
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, NormaliseInPlace) {
  TPlane *Self;
  vobjGetParam(Self);
  RET_VOID(Self->NormaliseInPlace());
}

//native void FlipInPlace ();
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, FlipInPlace) {
  TPlane *Self;
  vobjGetParam(Self);
  RET_VOID(Self->FlipInPlace());
}

//native float IntersectionTime (const TVec linestart, const TVec lineend, optional out TVec currhit) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, IntersectionTime) {
  TPlane *Self;
  TVec lstart, lend;
  VOptParamPtr<TVec> hitpoint;
  vobjGetParam(Self, lstart, lend, hitpoint);
  RET_FLOAT(Self->IntersectionTime(lstart, lend, hitpoint.value));
}

//native float LineIntersectTime (const TVec p0, const TVec p1, optional const float eps/*=0*/) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, LineIntersectTime) {
  TPlane *Self;
  TVec p0, p1;
  VOptParamFloat eps(0.0f);
  vobjGetParam(Self, p0, p1, eps);
  RET_FLOAT(Self->LineIntersectTime(p0, p1, eps));
}

//native TVec LineIntersect (const TVec p0, const TVec p1, optional const float eps/*=0*/) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, LineIntersect) {
  TPlane *Self;
  TVec p0, p1;
  VOptParamFloat eps(0.0f);
  vobjGetParam(Self, p0, p1, eps);
  RET_VEC(Self->LineIntersect(p0, p1, eps));
}

//native float LineIntersectTimeWithShift (const TVec p0, const TVec p1, const float shift, optional const float eps/*=0*/) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, LineIntersectTimeWithShift) {
  TPlane *Self;
  TVec p0, p1;
  float shift;
  VOptParamFloat eps(0.0f);
  vobjGetParam(Self, p0, p1, shift, eps);
  RET_FLOAT(Self->LineIntersectTimeWithShift(p0, p1, shift, eps));
}

//native float GetPointZ (const TVec v) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, GetPointZ) {
  TPlane *Self;
  TVec v;
  vobjGetParam(Self, v);
  RET_FLOAT(Self->GetPointZ(v));
}

//native float GetPointZRev (const TVec v) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, GetPointZRev) {
  TPlane *Self;
  TVec v;
  vobjGetParam(Self, v);
  RET_FLOAT(Self->GetPointZRev(v));
}

//native TVec Project (const TVec v) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, Project) {
  TPlane *Self;
  TVec v;
  vobjGetParam(Self, v);
  RET_VEC(Self->Project(v));
}

//native TVec IntersectionPoint (const ref TPlane plane2, const ref TPlane plane3) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, IntersectionPoint) {
  TPlane *Self;
  TPlane *p2;
  TPlane *p3;
  vobjGetParam(Self, p2, p3);
  RET_VEC(Self->IntersectionPoint(*p2, *p3));
}

//native bool SweepSphere (const TVec origin, const float radius, const TVec amove, optional out TVec hitpos, optional out float u) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, SweepSphere) {
  TPlane *Self;
  TVec origin;
  float radius;
  TVec amove;
  VOptParamPtr<TVec> hitpos;
  VOptParamPtr<float> u;
  vobjGetParam(Self, origin, radius, amove, hitpos, u);
  radius = fabsf(radius);
  RET_BOOL(Self->SweepSphere(origin, radius, amove, hitpos.value, u.value));
}

//native bool SweepBox3D (const TVec bmin, const TVec bmax, const TVec vdelta, optional out float time, optional out TVec hitpoint) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, SweepBox3D) {
  TPlane *Self;
  TVec bmin, bmax, vdelta;
  VOptParamPtr<float> time;
  VOptParamPtr<TVec> hitpoint;
  vobjGetParam(Self, bmin, bmax, vdelta, time, hitpoint);
  Create3DBBoxFromVectors(bbox, bmin, bmax);
  RET_BOOL(Self->SweepBox3D(bbox, vdelta, time.value, hitpoint.value));
}

//native bool SweepDoomBox3D (const TVec origin, const float radius, const float height, const TVec vdelta, optional out float time, optional out TVec hitpoint) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, SweepDoomBox3D) {
  TPlane *Self;
  TVec origin;
  float radius, height;
  TVec vdelta;
  VOptParamPtr<float> time;
  VOptParamPtr<TVec> hitpoint;
  vobjGetParam(Self, origin, radius, height, vdelta, time, hitpoint);
  if (height < 0.0f) { origin.z -= height; height = -height; }
  radius = fabsf(radius);
  CreateDoom3DBBox(bbox, origin, radius, height);
  RET_BOOL(Self->SweepBox3D(bbox, vdelta, time.value, hitpoint.value));
}

//native bool SweepBox3DEx (const float radius, const float height, const TVec vstart, const TVec vend, optional out float time, optional out TVec hitpoint) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, SweepBox3DEx) {
  TPlane *Self;
  float radius, height;
  TVec vstart, vend;
  VOptParamPtr<float> time;
  VOptParamPtr<TVec> hitpoint;
  vobjGetParam(Self, radius, height, vstart, vend, time, hitpoint);
  RET_BOOL(Self->SweepBox3DEx(radius, height, vstart, vend, time.value, hitpoint.value));
}

//native int PointOnSide (const TVec point) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, PointOnSide) {
  TPlane *Self;
  TVec p;
  vobjGetParam(Self, p);
  RET_INT(Self->PointOnSide(p));
}

//native int PointOnSide2 (const TVec point) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, PointOnSide2) {
  TPlane *Self;
  TVec p;
  vobjGetParam(Self, p);
  RET_INT(Self->PointOnSide2(p));
}

//native int SphereOnSide (const TVec center, float radius) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, SphereOnSide) {
  TPlane *Self;
  TVec center;
  float radius;
  vobjGetParam(Self, center, radius);
  radius = fabsf(radius);
  RET_INT(Self->SphereOnSide(center, radius));
}

//native bool SphereTouches (const TVec center, float radius) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, SphereTouches) {
  TPlane *Self;
  TVec center;
  float radius;
  vobjGetParam(Self, center, radius);
  radius = fabsf(radius);
  RET_BOOL(Self->SphereTouches(center, radius));
}

//native int SphereOnSide2 (const TVec center, float radius) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, SphereOnSide2) {
  TPlane *Self;
  TVec center;
  float radius;
  vobjGetParam(Self, center, radius);
  radius = fabsf(radius);
  RET_INT(Self->SphereOnSide2(center, radius));
}

//native bool checkBox3D (const TVec bmin, const TVec bmax) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, checkBox3D) {
  TPlane *Self;
  TVec bmin, bmax;
  vobjGetParam(Self, bmin, bmax);
  Create3DBBoxFromVectors(bbox, bmin, bmax);
  RET_BOOL(Self->checkBox3D(bbox));
}

//native bool checkBox2D (const TVec bmin, const TVec bmax) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, checkBox2D) {
  TPlane *Self;
  TVec bmin, bmax;
  vobjGetParam(Self, bmin, bmax);
  Create2DBBoxFromVectors(bbox, bmin, bmax);
  RET_BOOL(Self->checkBox2D(bbox));
}

//native bool checkDoomBox3D (const TVec origin, const float radius, const float height) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, checkDoomBox3D) {
  TPlane *Self;
  TVec origin;
  float radius, height;
  vobjGetParam(Self, origin, radius, height);
  if (height < 0.0f) { origin.z -= height; height = -height; }
  radius = fabsf(radius);
  CreateDoom3DBBox(bbox, origin, radius, height);
  RET_BOOL(Self->checkBox3D(bbox));
}

//native bool checkDoomBox2D (const TVec origin, const float radius) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, checkDoomBox2D) {
  TPlane *Self;
  TVec origin;
  float radius;
  vobjGetParam(Self, origin, radius);
  radius = fabsf(radius);
  CreateDoom2DBBox(bbox, origin, radius);
  RET_BOOL(Self->checkBox2D(bbox));
}

//native int Box3DOnSide2 (const TVec bmin, const TVec bmax) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, Box3DOnSide2) {
  TPlane *Self;
  TVec bmin, bmax;
  vobjGetParam(Self, bmin, bmax);
  Create3DBBoxFromVectors(bbox, bmin, bmax);
  RET_INT(Self->Box3DOnSide2(bbox));
}

//native int DoomBox3DOnSide2 (const TVec origin, const float radius, const float height) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, DoomBox3DOnSide2) {
  TPlane *Self;
  TVec origin;
  float radius, height;
  vobjGetParam(Self, origin, radius, height);
  if (height < 0.0f) { origin.z -= height; height = -height; }
  radius = fabsf(radius);
  CreateDoom3DBBox(bbox, origin, radius, height);
  RET_INT(Self->Box3DOnSide2(bbox));
}

//native int Box2DOnSide2 (const TVec bmin, const TVec bmax) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, Box2DOnSide2) {
  TPlane *Self;
  TVec bmin, bmax;
  vobjGetParam(Self, bmin, bmax);
  Create2DBBoxFromVectors(bbox, bmin, bmax);
  RET_INT(Self->Box2DOnSide2(bbox));
}

//native int DoomBox2DOnSide2 (const TVec origin, const float radius) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, DoomBox2DOnSide2) {
  TPlane *Self;
  TVec origin;
  float radius;
  vobjGetParam(Self, origin, radius);
  radius = fabsf(radius);
  CreateDoom2DBBox(bbox, origin, radius);
  RET_INT(Self->Box2DOnSide2(bbox));
}

//native TVec GetBox2DAcceptPoint (const TVec bmin, const TVec bmax);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, GetBox2DAcceptPoint) {
  TPlane *Self;
  TVec bmin, bmax;
  vobjGetParam(Self, bmin, bmax);
  Create2DBBoxFromVectors(bbox, bmin, bmax);
  RET_VEC(Self->get2DBBoxAcceptPoint(bbox));
}

//native TVec GetBox3DAcceptPoint (const TVec bmin, const TVec bmax);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, GetBox3DAcceptPoint) {
  TPlane *Self;
  TVec bmin, bmax;
  vobjGetParam(Self, bmin, bmax);
  Create3DBBoxFromVectors(bbox, bmin, bmax);
  RET_VEC(Self->get3DBBoxAcceptPoint(bbox));
}

//native TVec GetDoomBox2DAcceptPoint (const TVec origin, const float radius);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, GetDoomBox2DAcceptPoint) {
  TPlane *Self;
  TVec origin;
  float radius;
  vobjGetParam(Self, origin, radius);
  radius = fabsf(radius);
  CreateDoom2DBBox(bbox, origin, radius);
  RET_VEC(Self->get2DBBoxAcceptPoint(bbox));
}

//native TVec GetDoomBox3DAcceptPoint (const TVec origin, const float radius, const float height);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, GetDoomBox3DAcceptPoint) {
  TPlane *Self;
  TVec origin;
  float radius, height;
  vobjGetParam(Self, origin, radius, height);
  if (height < 0.0f) { origin.z -= height; height = -height; }
  radius = fabsf(radius);
  CreateDoom3DBBox(bbox, origin, radius, height);
  RET_VEC(Self->get3DBBoxAcceptPoint(bbox));
}

//native TVec GetBox2DRejectPoint (const TVec bmin, const TVec bmax);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, GetBox2DRejectPoint) {
  TPlane *Self;
  TVec bmin, bmax;
  vobjGetParam(Self, bmin, bmax);
  Create2DBBoxFromVectors(bbox, bmin, bmax);
  RET_VEC(Self->get2DBBoxRejectPoint(bbox));
}

//native TVec GetBox3DRejectPoint (const TVec bmin, const TVec bmax);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, GetBox3DRejectPoint) {
  TPlane *Self;
  TVec bmin, bmax;
  vobjGetParam(Self, bmin, bmax);
  Create3DBBoxFromVectors(bbox, bmin, bmax);
  RET_VEC(Self->get3DBBoxRejectPoint(bbox));
}

//native TVec GetDoomBox2DRejectPoint (const TVec origin, const float radius);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, GetDoomBox2DRejectPoint) {
  TPlane *Self;
  TVec origin;
  float radius;
  vobjGetParam(Self, origin, radius);
  radius = fabsf(radius);
  CreateDoom2DBBox(bbox, origin, radius);
  RET_VEC(Self->get2DBBoxRejectPoint(bbox));
}

//native TVec GetDoomBox3DRejectPoint (const TVec origin, const float radius, const float height);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, GetDoomBox3DRejectPoint) {
  TPlane *Self;
  TVec origin;
  float radius, height;
  vobjGetParam(Self, origin, radius, height);
  if (height < 0.0f) { origin.z -= height; height = -height; }
  radius = fabsf(radius);
  CreateDoom3DBBox(bbox, origin, radius, height);
  RET_VEC(Self->get3DBBoxRejectPoint(bbox));
}

//native TVec GetBox3DSupportPoint (const TVec bmin, const TVec bmax, optional out float pdist) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, GetBox3DSupportPoint) {
  TPlane *Self;
  TVec bmin, bmax;
  VOptParamPtr<float> pdist(nullptr);
  vobjGetParam(Self, bmin, bmax, pdist);
  Create3DBBoxFromVectors(bbox, bmin, bmax);
  RET_VEC(Self->get3DBBoxSupportPoint(bbox, pdist.value));
}

//native TVec GetBox2DSupportPoint (const TVec bmin, const TVec bmax, optional out float pdist, optional float z/*=0.0f*/) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, GetBox2DSupportPoint) {
  TPlane *Self;
  TVec bmin, bmax;
  VOptParamPtr<float> pdist(nullptr);
  VOptParamFloat z(0.0f);
  vobjGetParam(Self, bmin, bmax, pdist, z);
  Create3DBBoxFromVectors(bbox, bmin, bmax);
  RET_VEC(Self->get2DBBoxSupportPoint(bbox, pdist.value, z.value));
}

//native TVec GetDoomBox3DSupportPoint (const TVec origin, const float radius, const float height, optional out float pdist) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, GetDoomBox3DSupportPoint) {
  TPlane *Self;
  TVec origin;
  float radius, height;
  VOptParamPtr<float> pdist(nullptr);
  vobjGetParam(Self, origin, radius, height, pdist);
  if (height < 0.0f) { origin.z -= height; height = -height; }
  radius = fabsf(radius);
  CreateDoom3DBBox(bbox, origin, radius, height);
  RET_VEC(Self->get3DBBoxSupportPoint(bbox, pdist.value));
}

//native TVec GetDoomBox2DSupportPoint (const TVec origin, const float radius, optional out float pdist, optional float z/*=0.0f*/) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, GetDoomBox2DSupportPoint) {
  TPlane *Self;
  TVec origin;
  float radius;
  VOptParamPtr<float> pdist(nullptr);
  VOptParamFloat z(0.0f);
  vobjGetParam(Self, origin, radius, pdist, z);
  radius = fabsf(radius);
  CreateDoom2DBBox(bbox, origin, radius);
  RET_VEC(Self->get2DBBoxSupportPoint(bbox, pdist.value, z.value));
}

//native TVec GetBox3DAntiSupportPoint (const TVec bmin, const TVec bmax, optional out float pdist) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, GetBox3DAntiSupportPoint) {
  TPlane *Self;
  TVec bmin, bmax;
  VOptParamPtr<float> pdist(nullptr);
  vobjGetParam(Self, bmin, bmax, pdist);
  Create3DBBoxFromVectors(bbox, bmin, bmax);
  RET_VEC(Self->get3DBBoxAntiSupportPoint(bbox, pdist.value));
}

//native TVec GetBox2DAntiSupportPoint (const TVec bmin, const TVec bmax, optional out float pdist, optional float z/*=0.0f*/) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, GetBox2DAntiSupportPoint) {
  TPlane *Self;
  TVec bmin, bmax;
  VOptParamPtr<float> pdist(nullptr);
  VOptParamFloat z(0.0f);
  vobjGetParam(Self, bmin, bmax, pdist, z);
  Create3DBBoxFromVectors(bbox, bmin, bmax);
  RET_VEC(Self->get2DBBoxAntiSupportPoint(bbox, pdist.value, z.value));
}

//native TVec GetDoomBox3DAntiSupportPoint (const TVec origin, const float radius, const float height, optional out float pdist) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, GetDoomBox3DAntiSupportPoint) {
  TPlane *Self;
  TVec origin;
  float radius, height;
  VOptParamPtr<float> pdist(nullptr);
  vobjGetParam(Self, origin, radius, height, pdist);
  if (height < 0.0f) { origin.z -= height; height = -height; }
  radius = fabsf(radius);
  CreateDoom3DBBox(bbox, origin, radius, height);
  RET_VEC(Self->get3DBBoxAntiSupportPoint(bbox, pdist.value));
}

//native TVec GetDoomBox2DAntiSupportPoint (const TVec origin, const float radius, optional out float pdist, optional float z/*=0.0f*/) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, GetDoomBox2DAntiSupportPoint) {
  TPlane *Self;
  TVec origin;
  float radius;
  VOptParamPtr<float> pdist(nullptr);
  VOptParamFloat z(0.0f);
  vobjGetParam(Self, origin, radius, pdist, z);
  radius = fabsf(radius);
  CreateDoom2DBBox(bbox, origin, radius);
  RET_VEC(Self->get2DBBoxAntiSupportPoint(bbox, pdist.value, z.value));
}

//native float Angle2DToPlane (const ref TPlane to) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, Angle2DToPlane) {
  TPlane *Self;
  TPlane *To;
  vobjGetParam(Self, To);
  if (!Self || !To || Self == To) {
    RET_FLOAT(0.0f);
  } else {
    RET_FLOAT(PlanesAngle2D(Self, To));
  }
}

//native float Angle2DToPlaneFlipTo (const ref TPlane to) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, Angle2DToPlaneFlipTo) {
  TPlane *Self;
  TPlane *To;
  vobjGetParam(Self, To);
  if (!Self || !To || Self == To) {
    RET_FLOAT(0.0f);
  } else {
    RET_FLOAT(PlanesAngle2DFlipTo(Self, To));
  }
}

//native float Angle2DSignedToPlane (const ref TPlane to) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, Angle2DSignedToPlane) {
  TPlane *Self;
  TPlane *To;
  vobjGetParam(Self, To);
  if (!Self || !To || Self == To) {
    RET_FLOAT(0.0f);
  } else {
    RET_FLOAT(PlanesAngle2DSigned(Self, To));
  }
}

//native float Angle2DSignedToPlaneFlipTo (const ref TPlane to) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TPlane, Angle2DSignedToPlaneFlipTo) {
  TPlane *Self;
  TPlane *To;
  vobjGetParam(Self, To);
  if (!Self || !To || Self == To) {
    RET_FLOAT(0.0f);
  } else {
    RET_FLOAT(PlanesAngle2DSignedFlipTo(Self, To));
  }
}


//**************************************************************************
//
//  TMatrix4
//
//**************************************************************************
//native void SetIdentity ();
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, SetIdentity) {
  VMatrix4 *Self;
  vobjGetParam(Self);
  Self->SetIdentity();
}

//native void SetZero ();
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, SetZero) {
  VMatrix4 *Self;
  vobjGetParam(Self);
  Self->SetZero();
}

//native void copyFrom (const ref TMatrix4 m2);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, copyFrom) {
  VMatrix4 *Self;
  VMatrix4 *m2;
  vobjGetParam(Self, m2);
  if (Self != m2) {
    if (m2) *Self = *m2; else Self->SetIdentity();
  }
}

//native bool equal (const ref TMatrix4 m2) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, equal) {
  VMatrix4 *Self;
  VMatrix4 *m2;
  vobjGetParam(Self, m2);
  if (Self != m2) {
    RET_BOOL(*Self == *m2);
  } else {
    RET_BOOL(true);
  }
}

//native void mul (const ref TMatrix4 mt);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, mul) {
  VMatrix4 *Self;
  VMatrix4 *m2;
  vobjGetParam(Self, m2);
  *Self *= *m2;
}

//native void mulrev (const ref TMatrix4 mt);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, mulrev) {
  VMatrix4 *Self;
  VMatrix4 *m2;
  vobjGetParam(Self, m2);
  VMatrix4 xm = *m2;
  xm *= *Self;
  *Self = xm;
}

//native TVec vmul (const TVec v) const; // matrix by vector
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, vmul) {
  VMatrix4 *Self;
  TVec v;
  vobjGetParam(Self, v);
  RET_VEC((*Self)*v);
}

//native TVec vmulrev (const TVec v) const; // vector by matrix
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, vmulrev) {
  VMatrix4 *Self;
  TVec v;
  vobjGetParam(Self, v);
  RET_VEC(v*(*Self));
}

// unary minus, i.e. negate all matrix elements
//native void negate ();
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, negate) {
  VMatrix4 *Self;
  vobjGetParam(Self);
  for (unsigned f = 0; f < 4; ++f) {
    for (unsigned c = 0; f < 4; ++f) {
      Self->m[f][c] = -Self->m[f][c];
    }
  }
}

// this is for camera matrices
//native TVec getUpVector () const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, getUpVector) {
  VMatrix4 *Self;
  vobjGetParam(Self);
  RET_VEC(Self->getUpVector());
}

//native TVec getRightVector () const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, getRightVector) {
  VMatrix4 *Self;
  vobjGetParam(Self);
  RET_VEC(Self->getRightVector());
}

//native TVec getForwardVector () const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, getForwardVector) {
  VMatrix4 *Self;
  vobjGetParam(Self);
  RET_VEC(Self->getForwardVector());
}

//native float getDeterminant () const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, getDeterminant) {
  VMatrix4 *Self;
  vobjGetParam(Self);
  RET_FLOAT(Self->Determinant());
}

//native void Transpose () const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, Transpose) {
  VMatrix4 *Self;
  vobjGetParam(Self);
  *Self = Self->Transpose();
}

//native void RotateX (float angle);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, RotateX) {
  VMatrix4 *Self;
  float angle;
  vobjGetParam(Self, angle);
  *Self = VMatrix4::RotateX(angle);
}

//native void RotateY (float angle);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, RotateY) {
  VMatrix4 *Self;
  float angle;
  vobjGetParam(Self, angle);
  *Self = VMatrix4::RotateY(angle);
}

//native void RotateZ (float angle);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, RotateZ) {
  VMatrix4 *Self;
  float angle;
  vobjGetParam(Self, angle);
  *Self = VMatrix4::RotateZ(angle);
}

//native void Translate (const TVec v);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, Translate) {
  VMatrix4 *Self;
  TVec v;
  vobjGetParam(Self, v);
  *Self = VMatrix4::Translate(v);
}

//native void TranslateNeg (const TVec v);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, TranslateNeg) {
  VMatrix4 *Self;
  TVec v;
  vobjGetParam(Self, v);
  *Self = VMatrix4::TranslateNeg(v);
}

//native void Scale (const TVec v);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, Scale) {
  VMatrix4 *Self;
  TVec v;
  vobjGetParam(Self, v);
  *Self = VMatrix4::Scale(v);
}

//native void Rotate (const TVec v); // x, y and z are angles; does x, then y, then z
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, Rotate) {
  VMatrix4 *Self;
  TVec v;
  vobjGetParam(Self, v);
  *Self = VMatrix4::Rotate(v);
}

//native void RotateZXYv (const TVec v);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, RotateZXYv) {
  VMatrix4 *Self;
  TVec v;
  vobjGetParam(Self, v);
  *Self = VMatrix4::RotateZXY(v);
}

//native void RotateZXYa (const TAVec v);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, RotateZXYa) {
  VMatrix4 *Self;
  TAVec v;
  vobjGetParam(Self, v);
  *Self = VMatrix4::RotateZXY(v);
}

//native void Invert ();
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, Invert) {
  VMatrix4 *Self;
  vobjGetParam(Self);
  Self->invert();
}

//native TVec rotateVector (const TVec V) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, rotateVector) {
  VMatrix4 *Self;
  TVec v;
  vobjGetParam(Self, v);
  RET_VEC(Self->RotateVector(v));
}

// ignore rotation part
//native TVec translateVector (const TVec V) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, translateVector) {
  VMatrix4 *Self;
  TVec v;
  vobjGetParam(Self, v);
  RET_VEC(Self->TranslateVector(v));
}

//native TVec transform (const TVec V) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, transform) {
  VMatrix4 *Self;
  TVec v;
  vobjGetParam(Self, v);
  RET_VEC(Self->Transform(v));
}

//native TVec transformW (const TVec V, const float w) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, transformW) {
  VMatrix4 *Self;
  TVec v;
  float w;
  vobjGetParam(Self, v, w);
  RET_VEC(Self->Transform(v, w));
}

//native TVec transform2 (const TVec V) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, transform2) {
  VMatrix4 *Self;
  TVec v;
  vobjGetParam(Self, v);
  RET_VEC(Self->Transform2(v));
}

//native TVec transform2OnlyXY (const TVec V) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, transform2OnlyXY) {
  VMatrix4 *Self;
  TVec v;
  vobjGetParam(Self, v);
  RET_VEC(Self->Transform2OnlyXY(v));
}

//native float transform2OnlyZ (const TVec V) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, transform2OnlyZ) {
  VMatrix4 *Self;
  TVec v;
  vobjGetParam(Self, v);
  RET_FLOAT(Self->Transform2OnlyZ(v));
}

//native TVec transform2W (const TVec V, const float w) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, transform2W) {
  VMatrix4 *Self;
  TVec v;
  float w;
  vobjGetParam(Self, v, w);
  RET_VEC(Self->Transform2(v, w));
}

//native float transformInPlace (ref TVec V) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, transformInPlace) {
  VMatrix4 *Self;
  TVec *v;
  vobjGetParam(Self, v);
  RET_FLOAT(Self->TransformInPlace(*v));
}

//native float transformInPlaceW (ref TVec V, const float w) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, transformInPlaceW) {
  VMatrix4 *Self;
  TVec *v;
  float w;
  vobjGetParam(Self, v, w);
  RET_FLOAT(Self->TransformInPlace(*v, w));
}

//native float transform2InPlace (ref TVec V) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, transform2InPlace) {
  VMatrix4 *Self;
  TVec *v;
  vobjGetParam(Self, v);
  RET_FLOAT(Self->Transform2InPlace(*v));
}

//native float transform2InPlaceW (ref TVec V, const float w) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, transform2InPlaceW) {
  VMatrix4 *Self;
  TVec *v;
  float w;
  vobjGetParam(Self, v, w);
  RET_FLOAT(Self->Transform2InPlace(*v, w));
}

//native void ModelProjectCombine (const ref TMatrix4 model, const ref VMatrix4 proj);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, ModelProjectCombine) {
  VMatrix4 *Self;
  VMatrix4 *model;
  VMatrix4 *proj;
  vobjGetParam(Self, model, proj);
  Self->ModelProjectCombine(*model, *proj);
}

//native void extractFrustumLeft (ref TPlane plane) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, extractFrustumLeft) {
  VMatrix4 *Self;
  TPlane *plane;
  vobjGetParam(Self, plane);
  Self->ExtractFrustumLeft(*plane);
}

//native void extractFrustumRight (ref TPlane plane) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, extractFrustumRight) {
  VMatrix4 *Self;
  TPlane *plane;
  vobjGetParam(Self, plane);
  Self->ExtractFrustumRight(*plane);
}

//native void extractFrustumTop (ref TPlane plane) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, extractFrustumTop) {
  VMatrix4 *Self;
  TPlane *plane;
  vobjGetParam(Self, plane);
  Self->ExtractFrustumTop(*plane);
}

//native void extractFrustumBottom (ref TPlane plane) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, extractFrustumBottom) {
  VMatrix4 *Self;
  TPlane *plane;
  vobjGetParam(Self, plane);
  Self->ExtractFrustumBottom(*plane);
}

//native void extractFrustumFar (ref TPlane plane) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, extractFrustumFar) {
  VMatrix4 *Self;
  TPlane *plane;
  vobjGetParam(Self, plane);
  Self->ExtractFrustumFar(*plane);
}

//native void extractFrustumNear (ref TPlane plane) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, extractFrustumNear) {
  VMatrix4 *Self;
  TPlane *plane;
  vobjGetParam(Self, plane);
  Self->ExtractFrustumNear(*plane);
}

//native void Frustum (float left, float right, float bottom, float top, float nearVal, float farVal);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, Frustum) {
  VMatrix4 *Self;
  float left, right, bottom, top, nearVal, farVal;
  vobjGetParam(Self, left, right, bottom, top, nearVal, farVal);
  *Self = VMatrix4::Frustum(left, right, bottom, top, nearVal, farVal);
}

//native void Ortho (float left, float right, float bottom, float top, float nearVal, float farVal);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, Ortho) {
  VMatrix4 *Self;
  float left, right, bottom, top, nearVal, farVal;
  vobjGetParam(Self, left, right, bottom, top, nearVal, farVal);
  *Self = VMatrix4::Ortho(left, right, bottom, top, nearVal, farVal);
}

//native void Perspective (float fovY, float aspect, float zNear, float zFar);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, Perspective) {
  VMatrix4 *Self;
  float fovY, aspect, zNear, zFar;
  vobjGetParam(Self, fovY, aspect, zNear, zFar);
  *Self = VMatrix4::Perspective(fovY, aspect, zNear, zFar);
}

//native void LookAtFucked (const TVec eye, const TVec center, const TVec up);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, LookAtFucked) {
  VMatrix4 *Self;
  TVec eye, center, up;
  vobjGetParam(Self, eye, center, up);
  *Self = VMatrix4::LookAtFucked(eye, center, up);
}

//native void ProjectionNegOne (float fovY, float aspect, float zNear, float zFar);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, ProjectionNegOne) {
  VMatrix4 *Self;
  float fovY, aspect, zNear, zFar;
  vobjGetParam(Self, fovY, aspect, zNear, zFar);
  *Self = VMatrix4::ProjectionNegOne(fovY, aspect, zNear, zFar);
}

//native void ProjectionZeroOne (float fovY, float aspect, float zNear, float zFar);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, ProjectionZeroOne) {
  VMatrix4 *Self;
  float fovY, aspect, zNear, zFar;
  vobjGetParam(Self, fovY, aspect, zNear, zFar);
  *Self = VMatrix4::ProjectionZeroOne(fovY, aspect, zNear, zFar);
}

//native void LookAtGLM (const TVec eye, const TVec center, const TVec up);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, LookAtGLM) {
  VMatrix4 *Self;
  TVec eye, center, up;
  vobjGetParam(Self, eye, center, up);
  *Self = VMatrix4::LookAtGLM(eye, center, up);
}

//native void LookAt (const TVec eye, const TVec center, const TVec up);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, LookAt) {
  VMatrix4 *Self;
  TVec eye, center, up;
  vobjGetParam(Self, eye, center, up);
  *Self = VMatrix4::LookAt(eye, center, up);
}

//native void lookingAt (const TVec target);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, lookingAt) {
  VMatrix4 *Self;
  TVec target;
  vobjGetParam(Self, target);
  Self->lookingAt(target);
}

//native void lookingAtUp (const TVec target, const TVec upVec);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, lookingAtUp) {
  VMatrix4 *Self;
  TVec target, upVec;
  vobjGetParam(Self, target, upVec);
  Self->lookingAt(target, upVec);
}

//native void rotate (float angle, const TVec axis);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, rotate) {
  VMatrix4 *Self;
  float angle;
  TVec axis;
  vobjGetParam(Self, angle, axis);
  Self->rotate(angle, axis);
}

//native void rotateX (float angle);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, rotateX) {
  VMatrix4 *Self;
  float angle;
  vobjGetParam(Self, angle);
  Self->rotateX(angle);
}

//native void rotateY (float angle);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, rotateY) {
  VMatrix4 *Self;
  float angle;
  vobjGetParam(Self, angle);
  Self->rotateY(angle);
}

//native void rotateZ (float angle);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, rotateZ) {
  VMatrix4 *Self;
  float angle;
  vobjGetParam(Self, angle);
  Self->rotateZ(angle);
}

//native TAVec getAngles () const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, getAngles) {
  VMatrix4 *Self;
  vobjGetParam(Self);
  RET_AVEC(Self->getAngles());
}

//native void blend (const ref VMatrix4 m2, float blend) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, blend) {
  VMatrix4 *Self;
  VMatrix4 *m2;
  float blend;
  vobjGetParam(Self, m2, blend);
  *Self = Self->blended(*m2, blend);
}

//void toQuaternion (ref TQuaternion quat) const;
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, toQuaternion) {
  VMatrix4 *Self;
  float *q;
  vobjGetParam(Self, q);
  Self->toQuaternion(q);
}

//void FromQuaternion (const ref TQuaternion quat);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, FromQuaternion) {
  VMatrix4 *Self;
  float *q;
  vobjGetParam(Self, q);
  Self->fromQuaternion(q);
}

//native void BuildScale (const TVec vv);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, BuildScale) {
  VMatrix4 *Self;
  TVec v;
  vobjGetParam(Self, v);
  *Self = VMatrix4::BuildScale(v);
}

//native void BuildOffset (const TVec vv);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, BuildOffset) {
  VMatrix4 *Self;
  TVec v;
  vobjGetParam(Self, v);
  *Self = VMatrix4::BuildOffset(v);
}

//native void BuildRotate (const TAVec aa);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, BuildRotate) {
  VMatrix4 *Self;
  TAVec a;
  vobjGetParam(Self, a);
  *Self = VMatrix4::BuildRotate(a);
}

//native void scaleXY (const float ScaleX, const float ScaleY);
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, TMatrix4, scaleXY) {
  VMatrix4 *Self;
  float scaleX, scaleY;
  vobjGetParam(Self, scaleX, scaleY);
  Self->scaleXY(scaleX, scaleY);
}
