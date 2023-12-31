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

/*
  you can define struct methods via name mangling, like this:
    void MyStruct::method (args...) [const] { ... }
  the first hidden argument is always `[const] ref MyStruct self`.

  you can call this as normal method:
    mystruct.method(...)

  note that struct method is automatically final.
 */

class VStruct : public VMemberBase {
public:
  // persistent fields
  VStruct *ParentStruct;
  vuint8 IsVector;
  // size in stack units when used as local variable
  vint32 StackSize;
  // structure fields
  VField *Fields;
  // structure methods (namespacing)
  TArray<VMethod *> Methods;
  // is taking address allowed?
  bool Untagged;
  // if set, struct cannot be assigned, or passed by value
  bool NoAssign;

  // compiler fields
  VName ParentStructName;
  TLocationLine ParentStructLoc;
  bool Defined;

  // run-time fields
  bool PostLoaded;
  vint32 Size;
  vuint8 Alignment;
  VField *ReferenceFields;
  VField *DestructorFields;

  struct AliasInfo {
    VName aliasName;
    VName origName;
    TLocationLine loc;
    int aframe; // for loop checking
  };

  TMap<VName, AliasInfo> AliasList; // key: alias
  int AliasFrameNum;

  VName ResolveAlias (VName aname, bool nocase=false); // returns `aname` for unknown alias, or `NAME_None` for alias loop

private:
  // cache various requests (<0: not cached; 0: false; !0: true)
  int cacheNeedDTor; // >0: bit0: has fields to destruct; bit1: has dtor method
  int cacheNeedCleanup;

  void UpdateDTorCache ();

public:
  VStruct (VName AName, VMemberBase *AOuter, TLocation ALoc);
  virtual void CompilerShutdown () override;

  virtual void PostLoad () override;

  void AddMethod (VMethod *m);
  VMethod *FindMethod (VName Name, bool bRecursive=true);
  VMethod *FindAccessibleMethod (VName Name, VStruct *self=nullptr, const TLocation *loc=nullptr);
  VMethod *FindDtor (bool bRecursive=true);

  void AddField (VField *f);
  VField *FindField (VName);

  bool NeedsDestructor (); // any
  bool NeedsFieldsDestruction ();
  bool NeedsMethodDestruction ();

  // resolves parent struct
  bool Define ();

  // calls `Define()` for fields, inits some other internal fields
  // must be called after `Define()`
  bool DefineMembers ();

  // calls `Emit()` for methods
  void Emit ();

  bool IsA (const VStruct *s) const;

  void CalcFieldOffsets ();
  void InitReferences ();
  void InitDestructorFields ();
  static void SkipSerialisedObject (VStream &);
  void SerialiseObject (VStream &, vuint8 *);
  bool NetSerialiseObject (VStream &, VNetObjectsMapBase *, vuint8 *, bool vecprecise=true);
  void DeepCopyObject (vuint8 *Dst, const vuint8 *Src);
  bool NeedToCleanObject ();
  bool CleanObject (vuint8 *); // returns `true` if something was cleaned
  void DestructObject (vuint8 *Data);
  void ZeroObject (vuint8 *Data, bool calldtors);
  bool IdenticalObject (const vuint8 *, const vuint8 *, bool vecprecise=true);

  static VStruct *CreateWrapperStruct (VExpression *aTypeExpr, VMemberBase *AOuter, TLocation ALoc); // takes ownership

  friend inline VStream &operator << (VStream &Strm, VStruct *&Obj) { return Strm << *(VMemberBase **)&Obj; }
};
