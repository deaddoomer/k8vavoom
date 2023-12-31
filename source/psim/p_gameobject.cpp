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
#include "p_decal.h"


IMPLEMENT_CLASS(V, GameObject)


#define TAG_HASH_MAX_BUCKETS  (4096)

struct TagHashBucket {
  int tag;
  void *ptr;
  int next;
};


struct TagHash {
  TArray<TagHashBucket> buckets;
  int first[TAG_HASH_MAX_BUCKETS];
};


//==========================================================================
//
//  tagHashAlloc
//
//==========================================================================
TagHash *tagHashAlloc () {
  TagHash *th = (TagHash *)Z_Calloc(sizeof(TagHash));
  for (int f = 0; f < TAG_HASH_MAX_BUCKETS; ++f) th->first[f] = -1;
  return th;
}


//==========================================================================
//
//  tagHashClear
//
//==========================================================================
void tagHashClear (TagHash *th) {
  if (!th) return;
  for (int f = 0; f < TAG_HASH_MAX_BUCKETS; ++f) th->first[f] = -1;
  th->buckets.reset();
}


//==========================================================================
//
//  tagHashFree
//
//==========================================================================
void tagHashFree (TagHash *&th) {
  if (th) {
    th->buckets.clear();
    Z_Free(th);
    th = nullptr;
  }
}


//==========================================================================
//
//  tagHashPut
//
//==========================================================================
void tagHashPut (TagHash *th, int tag, void *ptr) {
  if (!th || !ptr || !tag || tag == -1) return;
  // check for existing ptr
  int lastBIdx = -1;
  const int hash = ((vuint32)tag)%TAG_HASH_MAX_BUCKETS;
  for (int hidx = th->first[hash]; hidx >= 0; hidx = th->buckets[hidx].next) {
    TagHashBucket &bk = th->buckets[hidx];
    if (bk.tag == tag && bk.ptr == ptr) return;
    lastBIdx = hidx;
  }
  // append new bucket
  TagHashBucket &bk = th->buckets.alloc();
  bk.tag = tag;
  bk.ptr = ptr;
  bk.next = -1;
  if (lastBIdx == -1) {
    vassert(th->first[hash] == -1);
    th->first[hash] = th->buckets.length()-1;
  } else {
    vassert(th->first[hash] != -1);
    vassert(th->buckets[lastBIdx].next == -1);
    th->buckets[lastBIdx].next = th->buckets.length()-1;
  }
}


//==========================================================================
//
//  tagHashCheckTag
//
//==========================================================================
bool tagHashCheckTag (TagHash *th, int tag, const void *ptr) {
  if (!th || !ptr || !tag || tag == -1) return false;
  const int hash = ((vuint32)tag)%TAG_HASH_MAX_BUCKETS;
  for (int hidx = th->first[hash]; hidx >= 0; hidx = th->buckets[hidx].next) {
    TagHashBucket &bk = th->buckets[hidx];
    if (bk.tag == tag && bk.ptr == ptr) return true;
  }
  return false;
}


/*
//==========================================================================
//
//  tagHashFirst
//
//==========================================================================
bool tagHashFirst (TagHashIter *it, TagHash *th, int tag) {
  if (!it || !th || tag == 0) {
    if (it) memset((void *)it, 0, sizeof(*it));
    return false;
  }
  const int hash = ((vuint32)tag)%TAG_HASH_MAX_BUCKETS;
  int hidx = th->first[hash];
  while (hidx >= 0 && th->buckets[hidx].tag != tag) hidx = th->buckets[hidx].next;
  if (hidx < 0) {
    memset((void *)it, 0, sizeof(*it));
    return false;
  }
  it->th = th;
  it->tag = tag;
  it->index = hidx;
  return true;
}


//==========================================================================
//
//  tagHashNext
//
//==========================================================================
bool tagHashNext (TagHashIter *it) {
  if (!it || !it->th) return false;
  const tag = th->tag;
  int hidx = th->index;
  if (hidx >= 0) {
    hidx = th->buckets[hidx].next;
    while (hidx >= 0 && th->buckets[hidx].tag != tag) hidx = th->buckets[hidx].next;
  }
  if (hidx < 0) {
    memset((void *)it, 0, sizeof(*it));
    return false;
  }
  it->index = hidx;
  return true;
}


//==========================================================================
//
//  tagHashCurrent
//
//==========================================================================
void *tagHashCurrent (TagHashIter *it) {
  return (it && it->th ? it->th->buckets[it->index].ptr : nullptr);
}
*/


//==========================================================================
//
//  tagHashFirst
//
//==========================================================================
int tagHashFirst (const TagHash *th, int tag) {
  if (!th || !tag || tag == -1) return -1;
  const int hash = ((vuint32)tag)%TAG_HASH_MAX_BUCKETS;
  int hidx = th->first[hash];
  while (hidx >= 0 && th->buckets[hidx].tag != tag) hidx = th->buckets[hidx].next;
  return hidx;
}


//==========================================================================
//
//  tagHashNext
//
//==========================================================================
int tagHashNext (const TagHash *th, int index, int tag) {
  if (!th || index < 0 || !tag) return -1;
  index = th->buckets[index].next;
  while (index >= 0 && th->buckets[index].tag != tag) index = th->buckets[index].next;
  return index;
}


//==========================================================================
//
//  tagHashPtr
//
//==========================================================================
void *tagHashPtr (const TagHash *th, int index) {
  if (!th || index < 0 || index >= th->buckets.length()) return nullptr;
  return (void *)(th->buckets[index].ptr);
}


//==========================================================================
//
//  tagHashTag
//
//==========================================================================
int tagHashTag (const TagHash *th, int index) {
  if (!th || index < 0 || index >= th->buckets.length()) return -1;
  return th->buckets[index].tag;
}



//==========================================================================
//
//  sec_region_t::isBlockingExtraLine
//
//==========================================================================
bool sec_region_t::isBlockingExtraLine () const noexcept {
  if (!extraline) return false;
  if (!extraline->frontside || extraline->alpha < 1.0f || (extraline->flags&ML_ADDITIVE)) return false;
  if (extraline->frontside->MidTexture.id <= 0) return false; // texture 0 is "none" (null)
  if (efloor.splane->Alpha < 1.0f || (efloor.splane->flags&SPF_ADDITIVE)) return false;
  VTexture *tex = GTextureManager[extraline->frontside->MidTexture.id]; // unanimated, nobody cares here
  return (tex && tex->Type != TEXTYPE_Null && !tex->isSeeThrough());
}



//==========================================================================
//
//  sector_t::CreateBaseRegion
//
//==========================================================================
void sector_t::CreateBaseRegion () {
  vassert(!eregions);
  sec_region_t *reg = (sec_region_t *)Z_Calloc(sizeof(sec_region_t));
  reg->efloor.set(&floor, false);
  reg->eceiling.set(&ceiling, false);
  reg->params = &params;
  reg->extraline = nullptr;
  reg->regflags = sec_region_t::RF_BaseRegion|sec_region_t::RF_NonSolid;
  eregions = reg;
}


//==========================================================================
//
//  sector_t::DeleteAllRegions
//
//==========================================================================
void sector_t::DeleteAllRegions () {
  while (eregions) {
    sec_region_t *r = eregions;
    eregions = r->next;
    Z_Free(r);
  }
}


//==========================================================================
//
//  sector_t::AllocRegion
//
//==========================================================================
sec_region_t *sector_t::AllocRegion () {
  sec_region_t *reg = (sec_region_t *)Z_Calloc(sizeof(sec_region_t));
  sec_region_t *last = eregions;
  if (last) {
    while (last->next) last = last->next;
    last->next = reg;
  } else {
    eregions = reg;
  }
  return reg;
}



//==========================================================================
//
//  seg_t::appendDecal
//
//==========================================================================
void seg_t::appendDecal (decal_t *dc) noexcept {
  if (!dc) return;
  vassert(dc->dcsurf == 0u);
  vassert(!dc->prev);
  vassert(!dc->next);
  vassert(!dc->seg);
  vassert(!dc->sreg);
  dc->seg = this;
  DLListAppend(dc, decalhead, decaltail);
}


//==========================================================================
//
//  seg_t::removeDecal
//
//  will not delete it
//
//==========================================================================
void seg_t::removeDecal (decal_t *dc) noexcept {
  if (!dc) return;
  vassert(dc->dcsurf == 0u);
  vassert(dc->seg == this);
  vassert(!dc->sreg);
  DLListRemove(dc, decalhead, decaltail);
  dc->prev = dc->next = nullptr;
  dc->seg = nullptr;
}


//==========================================================================
//
//  seg_t::killAllDecals
//
//==========================================================================
void seg_t::killAllDecals (VLevel *Level) noexcept {
  vassert(Level);
  while (decalhead) Level->DestroyDecal(decalhead); // this calls `removeDecal()`
  vassert(!decalhead);
  vassert(!decaltail);
}



//==========================================================================
//
//  line_t::Box2DSide
//
//  considers the line to be infinite
//  returns side 0 or 1, -1 if box crosses the line
//
//==========================================================================
int line_t::Box2DSide (const float tmbox[4]) const noexcept {
  unsigned p1 = 0, p2 = 0;
  switch (slopetype) {
    case ST_HORIZONTAL:
      p1 = (tmbox[BOX2D_TOP] > v1->y);
      p2 = (tmbox[BOX2D_BOTTOM] > v1->y);
      if (dir.x < 0.0f) { p1 ^= 1; p2 ^= 1; }
      break;
    case ST_VERTICAL:
      p1 = (tmbox[BOX2D_RIGHT] < v1->x);
      p2 = (tmbox[BOX2D_LEFT] < v1->x);
      if (dir.y < 0.0f) { p1 ^= 1; p2 ^= 1; }
      break;
    case ST_POSITIVE:
      p1 = PointOnSide(TVec(tmbox[BOX2D_LEFT], tmbox[BOX2D_TOP], 0.0f));
      p2 = PointOnSide(TVec(tmbox[BOX2D_RIGHT], tmbox[BOX2D_BOTTOM], 0.0f));
      break;
    case ST_NEGATIVE:
      p1 = PointOnSide(TVec(tmbox[BOX2D_RIGHT], tmbox[BOX2D_TOP], 0.0f));
      p2 = PointOnSide(TVec(tmbox[BOX2D_LEFT], tmbox[BOX2D_BOTTOM], 0.0f));
      break;
  }
  return (p1 == p2 ? (int)p1 : -1);
}



//==========================================================================
//
//  getFieldPtr
//
//==========================================================================
static vuint8 *getFieldPtr (VFieldType *fldtype, VObject *obj, VName fldname, int index, VObject *Self) {
  if (!obj) {
    VObject::VMDumpCallStack();
    if (Self) {
      GCon->Logf(NAME_Error, "cannot find field '%s' in null object, redirected from `%s`", *fldname, *Self->GetClass()->GetFullName());
    } else {
      GCon->Logf(NAME_Error, "cannot find field '%s' in null object", *fldname);
    }
    return nullptr;
  }
  VField *fld = obj->GetClass()->FindField(fldname);
  if (!fld) {
    VObject::VMDumpCallStack();
    if (Self == obj) {
      GCon->Logf(NAME_Error, "uservar '%s' not found in object of class `%s`", *fldname, *obj->GetClass()->GetFullName());
    } else {
      GCon->Logf(NAME_Error, "uservar '%s' not found in object of class `%s`, redirected from `%s`", *fldname, *obj->GetClass()->GetFullName(), *Self->GetClass()->GetFullName());
    }
    return nullptr;
  }
  if (fld->Type.Type == TYPE_Array) {
    if (index < 0 || fld->Type.ArrayDimInternal < 0 || index >= fld->Type.ArrayDimInternal) {
      VObject::VMDumpCallStack();
      if (Self == obj) {
        GCon->Logf(NAME_Error, "uservar '%s' array index out of bounds (%d) in object of class `%s`", *fldname, index, *obj->GetClass()->GetFullName());
      } else {
        GCon->Logf(NAME_Error, "uservar '%s' array index out of bounds (%d) in object of class `%s`, redirected from `%s`", *fldname, index, *obj->GetClass()->GetFullName(), *Self->GetClass()->GetFullName());
      }
      return nullptr;
    }
    VFieldType itt = fld->Type.GetArrayInnerType();
    if (fldtype) *fldtype = itt;
    return ((vuint8 *)obj)+fld->Ofs+itt.GetSize()*index;
  } else {
    if (index != 0) {
      VObject::VMDumpCallStack();
      if (Self == obj) {
        GCon->Logf(NAME_Error, "cannot index non-array uservar '%s' in object of class `%s` (index is %d)", *fldname, *obj->GetClass()->GetFullName(), index);
      } else {
        GCon->Logf(NAME_Error, "cannot index non-array uservar '%s' in object of class `%s` (index is %d), redirected from `%s`", *fldname, *obj->GetClass()->GetFullName(), index, *Self->GetClass()->GetFullName());
      }
      return nullptr;
    }
    if (fldtype) *fldtype = fld->Type;
    return ((vuint8 *)obj)+fld->Ofs;
  }
}


//==========================================================================
//
//  VGameObject::getRedirection
//
//==========================================================================
static VObject *getRedirection (VName fldname, VGameObject *gobj) {
  if (!gobj) {
    VObject::VMDumpCallStack();
    GCon->Logf(NAME_Error, "cannot redirect field '%s' in none object", *fldname);
    return nullptr;
  }
  if (gobj->IsDestroyed()) {
    VObject::VMDumpCallStack();
    //Host_Error("cannot redirect field '%s' in dead object", *fldname);
    GCon->Logf(NAME_Warning, "cannot redirect field '%s' in dead object", *fldname);
    return nullptr;
  }
  if (!gobj->_stateRouteSelf) return gobj;
  if (gobj->_stateRouteSelf->IsDestroyed()) {
    VObject::VMDumpCallStack();
    //Host_Error("cannot redirect field '%s' in dead object, from '%s'", *fldname, *gobj->GetClass()->GetFullName());
    GCon->Logf(NAME_Warning, "cannot redirect field '%s' in dead object, from '%s'", *fldname, *gobj->GetClass()->GetFullName());
    return nullptr;
  }
  return gobj->_stateRouteSelf;
}


//==========================================================================
//
//  VGameObject::_get_user_var_int
//
//==========================================================================
int VGameObject::_get_user_var_int (VName fldname, int index) {
  VObject *xobj = getRedirection(fldname, this);
  if (!xobj) return 0;
  VFieldType type;
  vuint8 *dptr = getFieldPtr(&type, xobj, fldname, index, this);
  if (!dptr) return 0;
  switch (type.Type) {
    case TYPE_Int: return *(const vint32 *)dptr;
    case TYPE_Float: return *(const float *)dptr;
  }
  GCon->Logf(NAME_Error, "cannot get non-int uservar '%s'", *fldname);
  return 0;
}


//==========================================================================
//
//  VGameObject::_get_user_var_float
//
//==========================================================================
float VGameObject::_get_user_var_float (VName fldname, int index) {
  VObject *xobj = getRedirection(fldname, this);
  if (!xobj) return 0;
  VFieldType type;
  vuint8 *dptr = getFieldPtr(&type, xobj, fldname, index, this);
  if (!dptr) return 0;
  switch (type.Type) {
    case TYPE_Int: return *(const vint32 *)dptr;
    case TYPE_Float: return *(const float *)dptr;
  }
  GCon->Logf(NAME_Error, "cannot get non-float uservar '%s'", *fldname);
  return 0;
}


//==========================================================================
//
//  VGameObject::_set_user_var_int
//
//==========================================================================
void VGameObject::_set_user_var_int (VName fldname, int value, int index) {
  VObject *xobj = getRedirection(fldname, this);
  #if 0
  if (VStr::strEquCI(*fldname, "user_trailstep")) {
    GCon->Logf(NAME_Debug, "_set_user_var_int: fld='%s'; value=%d; self=%s:%u; redir=%s:%u", *fldname, value,
      GetClass()->GetName(), GetUniqueId(), (xobj ? xobj->GetClass()->GetName() : "<none>"), (xobj ? xobj->GetUniqueId() : 0));
  }
  #endif
  if (!xobj) return;
  VFieldType type;
  vuint8 *dptr = getFieldPtr(&type, xobj, fldname, index, this);
  if (!dptr) return;
  switch (type.Type) {
    case TYPE_Int: *(vint32 *)dptr = value; return;
    case TYPE_Float: *(float *)dptr = value; return;
  }
  VObject::VMDumpCallStack();
  GCon->Logf(NAME_Error, "cannot set non-int uservar '%s'", *fldname);
}


//==========================================================================
//
//  VGameObject::_set_user_var_float
//
//==========================================================================
void VGameObject::_set_user_var_float (VName fldname, float value, int index) {
  VObject *xobj = getRedirection(fldname, this);
  if (!xobj) return;
  VFieldType type;
  vuint8 *dptr = getFieldPtr(&type, xobj, fldname, index, this);
  if (!dptr) return;
  switch (type.Type) {
    case TYPE_Int: *(vint32 *)dptr = value; return;
    case TYPE_Float: *(float *)dptr = value; return;
  }
  GCon->Logf(NAME_Error, "cannot set non-float uservar '%s'", *fldname);
}


//==========================================================================
//
//  VGameObject::_get_user_var_type
//
//==========================================================================
VGameObject::UserVarFieldType VGameObject::_get_user_var_type (VName fldname) {
  VObject *xobj = getRedirection(fldname, this);
  if (!xobj) return UserVarFieldType::None; // dunno
  VField *fld = xobj->GetClass()->FindField(fldname);
  if (!fld) return UserVarFieldType::None;
  if (fld->Type.Type == TYPE_Array) {
    if (fld->Type.IsArray2D()) return UserVarFieldType::None; // invalid
    switch (fld->Type.GetArrayInnerType().Type) {
      case TYPE_Int: return UserVarFieldType::IntArray;
      case TYPE_Float: return UserVarFieldType::FloatArray;
    }
  } else {
    switch (fld->Type.Type) {
      case TYPE_Int: return UserVarFieldType::Int;
      case TYPE_Float: return UserVarFieldType::Float;
    }
  }
  return UserVarFieldType::None; // invalid
}


//==========================================================================
//
//  VGameObject::_get_user_var_dim
//
//  array dimension; -1: not an array, or absent
//
//==========================================================================
int VGameObject::_get_user_var_dim (VName fldname) {
  VObject *xobj = getRedirection(fldname, this);
  if (!xobj) return -1;
  VField *fld = xobj->GetClass()->FindField(fldname);
  if (!fld) return -1;
  if (fld->Type.Type == TYPE_Array) {
    if (fld->Type.IsArray2D()) return -1; // invalid
    int dim = fld->Type.ArrayDimInternal;
    if (dim < 0) return -1;
    return dim;
  }
  return -1; // invalid
}


// ////////////////////////////////////////////////////////////////////////// //
//native final int _get_user_var_int (name fldname, optional int index);
IMPLEMENT_FUNCTION(VGameObject, _get_user_var_int) {
  VName fldname;
  VOptParamInt index(0);
  vobjGetParamSelf(fldname, index);
  if (!Self) { VObject::VMDumpCallStack(); Host_Error("cannot get field '%s' from null object", *fldname); }
  RET_INT(Self->_get_user_var_int(fldname, index));
}

//native final float _get_user_var_float (name fldname, optional int index);
IMPLEMENT_FUNCTION(VGameObject, _get_user_var_float) {
  VName fldname;
  VOptParamInt index(0);
  vobjGetParamSelf(fldname, index);
  if (!Self) { VObject::VMDumpCallStack(); Host_Error("cannot get field '%s' from null object", *fldname); }
  RET_FLOAT(Self->_get_user_var_float(fldname, index));
}

//native final void _set_user_var_int (name fldname, int value, optional int index);
IMPLEMENT_FUNCTION(VGameObject, _set_user_var_int) {
  VName fldname;
  int value;
  VOptParamInt index(0);
  vobjGetParamSelf(fldname, value, index);
  if (!Self) { VObject::VMDumpCallStack(); Host_Error("cannot set field '%s' in null object", *fldname); }
  Self->_set_user_var_int(fldname, value, index);
}

//native final void _set_user_var_float (name fldname, float value, optional int index);
IMPLEMENT_FUNCTION(VGameObject, _set_user_var_float) {
  VName fldname;
  float value;
  VOptParamInt index(0);
  vobjGetParamSelf(fldname, value, index);
  if (!Self) { VObject::VMDumpCallStack(); Host_Error("cannot set field '%s' in null object", *fldname); }
  Self->_set_user_var_float(fldname, value, index);
}

// native final UserVarFieldType _get_user_var_type (name fldname);
IMPLEMENT_FUNCTION(VGameObject, _get_user_var_type) {
  VName fldname;
  vobjGetParamSelf(fldname);
  if (!Self) { VObject::VMDumpCallStack(); Host_Error("cannot check field '%s' in null object", *fldname); }
  RET_INT(Self->_get_user_var_type(fldname));
}

// native final int _get_user_var_dim (name fldname); // array dimension; -1: not an array, or absent
IMPLEMENT_FUNCTION(VGameObject, _get_user_var_dim) {
  VName fldname;
  vobjGetParamSelf(fldname);
  if (!Self) { VObject::VMDumpCallStack(); Host_Error("cannot check field '%s' in null object", *fldname); }
  RET_INT(Self->_get_user_var_dim(fldname));
}


//native static final TVec spGetNormal (const ref TSecPlaneRef sp);
IMPLEMENT_FUNCTION(VGameObject, spGetNormal) {
  TSecPlaneRef *sp;
  vobjGetParam(sp);
  //P_GET_PTR(TSecPlaneRef, sp);
  RET_VEC(sp->GetNormal());
}

//native static final float spGetNormalZ (const ref TSecPlaneRef sp);
IMPLEMENT_FUNCTION(VGameObject, spGetNormalZ) {
  TSecPlaneRef *sp;
  vobjGetParam(sp);
  //P_GET_PTR(TSecPlaneRef, sp);
  RET_FLOAT(sp->GetNormalZ());
}

//native static final float spGetDist (const ref TSecPlaneRef sp);
IMPLEMENT_FUNCTION(VGameObject, spGetDist) {
  TSecPlaneRef *sp;
  vobjGetParam(sp);
  //P_GET_PTR(TSecPlaneRef, sp);
  RET_FLOAT(sp->GetDist());
}

//native static final float spGetPointZ (const ref TSecPlaneRef sp, const ref TVec p);
IMPLEMENT_FUNCTION(VGameObject, spGetPointZ) {
  TSecPlaneRef *sp;
  TVec *point;
  vobjGetParam(sp, point);
  //P_GET_PTR(TVec, point);
  //P_GET_PTR(TSecPlaneRef, sp);
  RET_FLOAT(sp->GetPointZClamped(point->x, point->y));
}

//native static final float spDotPoint (const ref TSecPlaneRef sp, const ref TVec point);
IMPLEMENT_FUNCTION(VGameObject, spDotPoint) {
  TSecPlaneRef *sp;
  TVec *point;
  vobjGetParam(sp, point);
  //P_GET_PTR(TVec, point);
  //P_GET_PTR(TSecPlaneRef, sp);
  RET_FLOAT(sp->DotPoint(*point));
}

//native static final float spPointDistance (const ref TSecPlaneRef sp, const ref TVec point);
IMPLEMENT_FUNCTION(VGameObject, spPointDistance) {
  TSecPlaneRef *sp;
  TVec *point;
  vobjGetParam(sp, point);
  //P_GET_PTR(TVec, point);
  //P_GET_PTR(TSecPlaneRef, sp);
  RET_FLOAT(sp->PointDistance(*point));
}

//native static final int spPointOnSide (const ref TSecPlaneRef sp, const ref TVec point);
IMPLEMENT_FUNCTION(VGameObject, spPointOnSide) {
  TSecPlaneRef *sp;
  TVec *point;
  vobjGetParam(sp, point);
  //P_GET_PTR(TVec, point);
  //P_GET_PTR(TSecPlaneRef, sp);
  RET_INT(sp->PointOnSide(*point));
}

//native static final int spPointOnSideThreshold (const ref TSecPlaneRef sp, const ref TVec point);
IMPLEMENT_FUNCTION(VGameObject, spPointOnSideThreshold) {
  TSecPlaneRef *sp;
  TVec *point;
  vobjGetParam(sp, point);
  //P_GET_PTR(TVec, point);
  //P_GET_PTR(TSecPlaneRef, sp);
  RET_INT(sp->PointOnSideThreshold(*point));
}

//native static final int spPointOnSideFri (const ref TSecPlaneRef sp, const ref TVec point);
IMPLEMENT_FUNCTION(VGameObject, spPointOnSideFri) {
  TSecPlaneRef *sp;
  TVec *point;
  vobjGetParam(sp, point);
  //P_GET_PTR(TVec, point);
  //P_GET_PTR(TSecPlaneRef, sp);
  RET_INT(sp->PointOnSideFri(*point));
}

//native static final int spPointOnSide2 (const ref TSecPlaneRef sp, const ref TVec point);
IMPLEMENT_FUNCTION(VGameObject, spPointOnSide2) {
  TSecPlaneRef *sp;
  TVec *point;
  vobjGetParam(sp, point);
  //P_GET_PTR(TVec, point);
  //P_GET_PTR(TSecPlaneRef, sp);
  RET_INT(sp->PointOnSide2(*point));
}

//native static final bool spSphereTouches (const ref TSecPlaneRef sp, const ref TVec center, float radius);
IMPLEMENT_FUNCTION(VGameObject, spSphereTouches) {
  TSecPlaneRef *sp;
  TVec *center;
  float radius;
  vobjGetParam(sp, center, radius);
  //P_GET_FLOAT(radius);
  //P_GET_PTR(TVec, center);
  //P_GET_PTR(TSecPlaneRef, sp);
  RET_BOOL(sp->SphereTouches(*center, radius));
}

//native static final int spSphereOnSide (const ref TSecPlaneRef sp, const ref TVec center, float radius);
IMPLEMENT_FUNCTION(VGameObject, spSphereOnSide) {
  TSecPlaneRef *sp;
  TVec *center;
  float radius;
  vobjGetParam(sp, center, radius);
  //P_GET_FLOAT(radius);
  //P_GET_PTR(TVec, center);
  //P_GET_PTR(TSecPlaneRef, sp);
  RET_INT(sp->SphereOnSide(*center, radius));
}

//native static final int spSphereOnSide2 (const ref TSecPlaneRef sp, const ref TVec center, float radius);
IMPLEMENT_FUNCTION(VGameObject, spSphereOnSide2) {
  TSecPlaneRef *sp;
  TVec *center;
  float radius;
  vobjGetParam(sp, center, radius);
  //P_GET_FLOAT(radius);
  //P_GET_PTR(TVec, center);
  //P_GET_PTR(TSecPlaneRef, sp);
  RET_INT(sp->SphereOnSide2(*center, radius));
}

//native static final float GetPointZClamped (const ref sec_plane_t plane, const TVec point);
IMPLEMENT_FUNCTION(VGameObject, GetPointZClamped) {
  sec_plane_t *plane;
  TVec point;
  vobjGetParam(plane, point);
  //P_GET_VEC(point);
  //P_GET_PTR(sec_plane_t, plane);
  const float res = plane->GetPointZClamped(point);
  if (!isFiniteF(res)) { VObject::VMDumpCallStack(); Sys_Error("invalid call to `GetPlanePointZ()` (probably called with vertical plane)"); }
  RET_FLOAT(res);
}

//native static final float GetPointZRevClamped (const ref sec_plane_t plane, const TVec point);
IMPLEMENT_FUNCTION(VGameObject, GetPointZRevClamped) {
  sec_plane_t *plane;
  TVec point;
  vobjGetParam(plane, point);
  //P_GET_VEC(point);
  //P_GET_PTR(sec_plane_t, plane);
  const float res = plane->GetPointZRevClamped(point);
  if (!isFiniteF(res)) { VObject::VMDumpCallStack(); Sys_Error("invalid call to `GetPlanePointZ()` (probably called with vertical plane)"); }
  RET_FLOAT(res);
}

//native static final bool SectorHas3DFloors (const sector_t *sector);
IMPLEMENT_FUNCTION(VGameObject, SectorHas3DFloors) {
  sector_t *sector;
  vobjGetParam(sector);
  //P_GET_PTR(sector_t, sector);
  if (sector) {
    RET_BOOL(sector->Has3DFloors());
  } else {
    RET_BOOL(false);
  }
}

//native static final bool SectorHas3DSlopes (const sector_t *sector);
IMPLEMENT_FUNCTION(VGameObject, SectorHas3DSlopes) {
  sector_t *sector;
  vobjGetParam(sector);
  //P_GET_PTR(sector_t, sector);
  if (sector) {
    RET_BOOL(sector->Has3DSlopes());
  } else {
    RET_BOOL(false);
  }
}


//==========================================================================
//
//  CheckPlanePass
//
//  checks for plane hit, returns hit point and `false` if hit
//  plane flags should be already checked
//
//==========================================================================
//static final bool CheckPlanePass (const ref TSecPlaneRef plane, const TVec linestart, const TVec lineend,
//                                  optional out TVec currhit, optional out bool isSky);
IMPLEMENT_FUNCTION(VGameObject, CheckPlanePass) {
  TVec ctmp(0.0f, 0.0f, 0.0f);
  bool tmpb = false;

  P_GET_PTR_OPT(bool, isSky, &tmpb);
  P_GET_PTR_OPT(TVec, currhit, &ctmp);
  P_GET_VEC(lineend);
  P_GET_VEC(linestart);
  P_GET_PTR(TSecPlaneRef, plane);

  RET_BOOL(VLevel::CheckPlanePass(*plane, linestart, lineend, *currhit, *isSky));
}


//static final bool CheckPassPlanes (const sector_t *sector,
//                                   TVec linestart, TVec lineend, int flagmask,
//                                   optional out TVec outHitPoint, optional out TVec outHitNormal,
//                                   optional out bool outIsSky);
IMPLEMENT_FUNCTION(VGameObject, CheckPassPlanes) {
  sector_t *sector;
  TVec linestart, lineend;
  vuint32 flagmask;
  VOptParamPtr<TVec> outHitPoint;
  VOptParamPtr<TVec> outHitNormal;
  VOptParamPtr<bool> outIsSky;
  vobjGetParam(sector, linestart, lineend, flagmask, outHitPoint, outHitNormal, outIsSky);
  RET_BOOL(VLevel::CheckPassPlanes(sector, linestart, lineend, flagmask, outHitPoint, outHitNormal, outIsSky, nullptr));
}


//static final float CheckPObjPassPlanes (const polyobj_t *po, TVec linestart, TVec lineend,
//                                        optional out TVec outHitPoint, optional out TVec outHitNormal);
IMPLEMENT_FUNCTION(VGameObject, CheckPObjPassPlanes) {
  polyobj_t *po;
  TVec linestart, lineend;
  VOptParamPtr<TVec> outHitPoint;
  VOptParamPtr<TVec> outHitNormal;
  vobjGetParam(po, linestart, lineend, outHitPoint, outHitNormal);
  RET_FLOAT(VLevel::CheckPObjPassPlanes(po, linestart, lineend, outHitPoint, outHitNormal, nullptr));
}
