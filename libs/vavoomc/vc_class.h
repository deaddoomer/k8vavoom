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

enum ENativeConstructor { EC_NativeConstructor };


// flags describing a class
enum EClassFlags {
  // base flags
  CLASS_Native               = 0x0001,
  CLASS_Abstract             = 0x0002, // class is abstract and can't be instantiated directly
  CLASS_SkipSuperStateLabels = 0x0004, // don't copy state labels
  CLASS_Transient            = 0x8000,
};

// flags describing a class instance
enum EClassObjectFlags {
  CLASSOF_Native     = 0x00000001, // native
  CLASSOF_PostLoaded = 0x00000002, // `PostLoad()` has been called
};


struct mobjinfo_t {
public:
  // for `flags`
  enum {
    FlagNoSkill = 0x0001,
    FlagSpecial = 0x0002,
  };

  int DoomEdNum;
  vint32 GameFilter;
  VClass *Class;
  vint32 flags; // bit0: anyskill; bit1: special is set
  vint32 special;
  vint32 args[5];

//private:
  vint32 nextidx; // -1, or next info with the same DoomEdNum

//public:
  //friend VStream &operator << (VStream &, mobjinfo_t &);
};


//==========================================================================
//
//  VStateLabel
//
//==========================================================================
struct VStateLabel {
  // persistent fields
  VName Name;
  VState *State;
  TArray<VStateLabel> SubLabels;

  VStateLabel () : Name(NAME_None), State(nullptr) {}

  friend VStream &operator << (VStream &, VStateLabel &);
};


//==========================================================================
//
//  VStateLabelDef
//
//==========================================================================
struct VStateLabelDef {
  VStr Name;
  VState *State;
  TLocation Loc;
  VName GotoLabel;
  vint32 GotoOffset;

  VStateLabelDef () : State(nullptr), GotoLabel(NAME_None), GotoOffset(0) {}
};


//==========================================================================
//
//  VRepField
//
//==========================================================================
struct VRepField {
  VName Name;
  TLocation Loc;
  VMemberBase *Member;

  friend VStream &operator << (VStream &Strm, VRepField &Fld) { return Strm << Fld.Member; }
};


//==========================================================================
//
//  VRepInfo
//
//==========================================================================
struct VRepInfo {
  bool Reliable;
  VMethod *Cond;
  TArray<VRepField> RepFields;

  friend VStream &operator << (VStream &Strm, VRepInfo &RI) { return Strm << RI.Cond << RI.RepFields; }
};


//==========================================================================
//
//  VDecorateStateAction
//
//==========================================================================
struct VDecorateStateAction {
  VName Name;
  VMethod *Method;
};


//==========================================================================
//
//  VDecorateUserVarDef
//
//==========================================================================
struct VDecorateUserVarDef {
  VName name;
  TLocation loc;
  VFieldType type; // can be array too
};


//==========================================================================
//
//  VLightEffectDef
//
//==========================================================================
struct VLightEffectDef {
  VName Name;
  vuint8 Type;
  vint32 Color;
  float Radius;
  float Radius2;
  float MinLight;
  TVec Offset;
  float Chance;
  float Interval;
  float Scale;
  vint32 NoSelfShadow; // this will become flags
};


//==========================================================================
//
//  VParticleEffectDef
//
//==========================================================================
struct VParticleEffectDef {
  VName Name;
  vuint8 Type;
  vuint8 Type2;
  vint32 Color;
  TVec Offset;
  vint32 Count;
  float OrgRnd;
  TVec Velocity;
  float VelRnd;
  float Accel;
  float Grav;
  float Duration;
  float Ramp;
};


//==========================================================================
//
//  VSpriteEffect
//
//==========================================================================
struct VSpriteEffect {
  vint32 SpriteIndex;
  vint8 Frame;
  VLightEffectDef *LightDef;
  VParticleEffectDef *PartDef;
};


//==========================================================================
//
//  VClass
//
//==========================================================================
class VClass : public VMemberBase {
public:
  enum { LOWER_CASE_HASH_SIZE = 8 * 1024 };

  struct TextureInfo {
    VStr texImage;
    int frameWidth;
    int frameHeight;
    int frameOfsX;
    int frameOfsY;
  };

  enum ReplaceType {
    Replace_None = 0,
    // decorate-style replace
    Replace_Normal,
    // HACK: find the latest child, and replace it.
    //       this will obviously not work if class has many children,
    //       but still useful to replace things like `LineSpecialLevelInfo`,
    //       because each game creates its own subclass of that.
    //       actually, we will replace subclass with the longest subclassing chain.
    Replace_LatestChild,
    // VavoomC replacement, affect parents (but doesn't force replaces)
    Replace_NextParents,
    Replace_NextParents_LastChild,
    // this does decorate-style replacement, and also replaces
    // all parents for direct children
    Replace_Parents,
    Replace_Parents_LatestChild,
  };

  inline bool DoesLastChildReplacement () const {
    return
      DoesReplacement == ReplaceType::Replace_LatestChild ||
      DoesReplacement == ReplaceType::Replace_NextParents_LastChild ||
      DoesReplacement == ReplaceType::Replace_Parents_LatestChild;
  }

  inline bool DoesParentReplacement () const { return (DoesReplacement >= ReplaceType::Replace_NextParents); }
  inline bool DoesForcedParentReplacement () const { return (DoesReplacement >= ReplaceType::Replace_Parents); }

public:
  // persistent fields
  VClass *ParentClass;
  VField *Fields;
  VState *States;
  TArray<VMethod *> Methods;
  VMethod *DefaultProperties;
  TArray<VRepInfo> RepInfos;
  TArray<VStateLabel> StateLabels;
  VName ClassGameObjName; // class can has a "game object name" (used in Spelunky Remake, for example)

  // compiler fields
  VName ParentClassName;
  TLocation ParentClassLoc;
  ReplaceType DoesReplacement;
  VExpression *GameExpr;
  VExpression *MobjInfoExpr;
  VExpression *ScriptIdExpr;
  TArray<VStruct *> Structs;
  TArray<VConstant *> Constants;
  TArray<VProperty *> Properties;
  TArray<VStateLabelDef> StateLabelDefs;
  bool Defined;
  bool DefinedAsDependency;

  // this is built when class postloaded
  TMapNC<VName, VMethod *> MethodMap;
  // contains both commands and autocompleters
  TMap<VStr, VMethod *> ConCmdListMts; // names are lowercased

  // new-style state options and textures
  TMapDtor<VStr, TextureInfo> dfStateTexList;
  VStr dfStateTexDir;
  vint32 dfStateTexDirSet;

  // internal per-object variables
  vuint32 ObjectFlags; // private EClassObjectFlags used by object manager
  VClass *LinkNext; // next class in linked list

  vint32 ClassSize;
  vint32 ClassUnalignedSize;
  vuint32 ClassFlags; // EClassFlags
  VMethod **ClassVTable;
  void (*ClassConstructor) ();

  vint32 ClassNumMethods;

  VField *ReferenceFields;
  VField *DestructorFields;
  VField *NetFields;
  VMethod *NetMethods;
  VState *NetStates;
  TArray<VState *> StatesLookup;
  vint32 NumNetFields;

  vuint8 *Defaults;

  VClass *Replacement;
  VClass *Replacee;

  TArray<VDecorateStateAction> DecorateStateActions;
  TMapNC<VName, VName> DecorateStateFieldTrans; // field translation; key is decorate name (lowercased), value is real field name

  TArray<VSpriteEffect> SpriteEffects;

  //VName LowerCaseName;
  //VClass *LowerCaseHashNext;

  static TArray<VName> GSpriteNames;
  //static VClass *GLowerCaseHashTable[LOWER_CASE_HASH_SIZE];

  struct AliasInfo {
    VName aliasName;
    VName origName;
    TLocation loc;
    int aframe; // for loop checking
  };

  TMap<VName, AliasInfo> AliasList; // key: alias
  int AliasFrameNum;

  VName ResolveAlias (VName aname, bool nocase=false); // returns `aname` for unknown alias, or `NAME_None` for alias loop

  TMap<VName, bool> KnownEnums;

public:
  VClass (VName, VMemberBase *, const TLocation &);
  VClass (ENativeConstructor, size_t ASize, vuint32 AClassFlags, VClass *AParent, EName AName, void (*ACtor) ());
  virtual ~VClass () override;
  virtual void CompilerShutdown () override;

  // systemwide functions
  static VClass *FindClass (const char *);
  static VClass *FindClassNoCase (const char *);
  static int FindSprite (VName, bool = true);
  static void GetSpriteNames (TArray<FReplacedString> &);
  static void ReplaceSpriteNames (TArray<FReplacedString> &);
  static void StaticReinitStatesLookup ();

  static mobjinfo_t *AllocMObjId (vint32 id, int GameFilter, VClass *cls);
  static mobjinfo_t *AllocScriptId (vint32 id, int GameFilter, VClass *cls);

  static mobjinfo_t *FindMObjId (vint32 id, int GameFilter);
  static mobjinfo_t *FindMObjIdByClass (const VClass *cls, int GameFilter);
  static mobjinfo_t *FindScriptId (vint32 id, int GameFilter);

  static void RemoveMObjId (vint32 id, int GameFilter);
  static void RemoveScriptId (vint32 id, int GameFilter);
  static void RemoveMObjIdByClass (VClass *cls, int GameFilter);

  static void StaticDumpMObjInfo ();
  static void StaticDumpScriptIds ();

  virtual void Serialise (VStream &) override;
  // calculates field offsets, creates VTable, and fills other such info
  // must be called after `Define()` and `Emit()`
  virtual void PostLoad () override;
  virtual void Shutdown () override;

  void AddConstant (VConstant *);
  void AddField (VField *);
  void AddProperty (VProperty *);
  void AddState (VState *);
  void AddMethod (VMethod *);

  bool IsKnownEnum (VName EnumName);
  bool AddKnownEnum (VName EnumName); // returns `true` if enum was redefined

  VConstant *FindConstant (VName Name, VName EnumName=NAME_None);
  VConstant *FindPackageConstant (VMemberBase *pkg, VName Name, VName EnumName=NAME_None);

  VField *FindField (VName, bool bRecursive=true);
  VField *FindField (VName, const TLocation &, VClass *);
  VField *FindFieldChecked (VName);

  VProperty *FindProperty (VName);

  VMethod *FindMethod (VName Name, bool bRecursive=true);
  // this will follow `ParentClassName` instead of `ParentClass`
  VMethod *FindMethodNonPostLoaded (VName Name, bool bRecursive=true);
  //VMethod *FindMethodNoCase (VName Name, bool bRecursive=true);
  VMethod *FindMethodChecked (VName);
  VMethod *FindAccessibleMethod (VName Name, VClass *self=nullptr, const TLocation *loc=nullptr);
  int GetMethodIndex (VName);

  VState *FindState (VName);
  VState *FindStateChecked (VName);
  VStateLabel *FindStateLabel (VName AName, VName SubLabel=NAME_None, bool Exact=false);
  VStateLabel *FindStateLabel (TArray<VName> &, bool);

  VDecorateStateAction *FindDecorateStateAction (VName);
  VName FindDecorateStateFieldTrans (VName dcname);

  VClass *FindBestLatestChild (VName ignoreThis);

  // WARNING! method with such name should exist, or return value will be invalid
  //          this is valid only after class was postloaded
  bool isNonVirtualMethod (VName Name);

  // resolves parent name to parent class, calls `Define()` on constants and structs,
  // sets replacement, and checks for duplicate fields/consts
  bool Define ();

  // calls `DefineMembers()` for structs, `Define()` for methods, fields and properties (including default)
  // also calls `Define()` for states, and calls `DefineRepInfos()`
  // must be called after `Define()`
  bool DefineMembers ();

  // fills `RepXXX` fields; called from `DefineMembers()`, no need to call it manually
  bool DefineRepInfos ();

  // calls `Emit()` for methods, emits state labels, calls `Emit()` for states, repinfo conditions, and default properties
  void Emit ();

  // this is special "castrated" emitter for decorate classes
  // as decorate classes cannot have replications, or new virtual methods, or such,
  // we don't need (and actually cannot) perform full `Emit()` on them
  void DecorateEmit ();

  // the same for `PostLoad()` -- it is called instead of normal `PostLoad()` for decorate classes
  void DecoratePostLoad ();

  void EmitStateLabels ();

  VState *ResolveStateLabel (const TLocation &, VName, int);
  void SetStateLabel (VName, VState *);
  void SetStateLabel (const TArray<VName> &, VState *);

  inline bool IsChildOf (const VClass *SomeBaseClass) const {
    for (const VClass *c = this; c; c = c->GetSuperClass()) if (SomeBaseClass == c) return true;
    return false;
  }

  inline bool IsChildOfByName (VName name) const {
    if (name == NAME_None) return false;
    for (const VClass *c = this; c; c = c->GetSuperClass()) {
      if (VStr::strEquCI(*name, *c->Name)) return true;
    }
    return false;
  }

  // accessors
  inline VClass *GetSuperClass () const { return ParentClass; }

  void CopyObject (const vuint8 *, vuint8 *);
  //void SerialiseObject (VStream &, VObject *); // moved to VObject
  void CleanObject (VObject *);
  void DestructObject (VObject *);
  VClass *CreateDerivedClass (VName, VMemberBase *, TArray<VDecorateUserVarDef> &, const TLocation &);
  VClass *GetReplacement ();
  VClass *GetReplacee ();
  // returns `false` if replacee cannot be set for some reason
  bool SetReplacement (VClass *cls); // assign `cls` as a replacement for this

  //void HashLowerCased ();

  // df state thingy
  void DFStateSetTexDir (VStr adir);
  void DFStateAddTexture (VStr tname, const TextureInfo &ti);
  VStr DFStateGetTexDir () const;
  bool DFStateGetTexture (VStr tname, TextureInfo &ti) const;

  // console command methods
  // returns index in lists, or -1
  VMethod *FindConCommandMethod (VStr name, bool exact=false); // skips autocompleters if `exact` is `false`
  inline VMethod *FindConCommandMethodExact (VStr name) { return FindConCommandMethod(name, true); } // doesn't skip anything

  // WARNING! don't add/remove ANY named members from callback!
  // return `FERes::FOREACH_STOP` from callback to stop (and return current member)
  // template function should accept `VClass *`, and return `FERes`
  // templated, so i can use lambdas
  // k8: don't even ask me. fuck shitplusplus.
  template<typename TDg> static VClass *ForEachChildOf (VClass *superCls, TDg &&dg) {
    decltype(dg((VClass *)nullptr)) test_ = FERes::FOREACH_NEXT;
    (void)test_;
    if (!superCls) return nullptr;
    for (auto &&m : GMembers) {
      if (m->MemberType != MEMBER_Class) continue;
      VClass *cls = (VClass *)m;
      if (superCls && !cls->IsChildOf(superCls)) continue;
      FERes res = dg(cls);
      if (res == FERes::FOREACH_STOP) return cls;
    }
    return nullptr;
  }

  template<typename TDg> static VClass *ForEachChildOf (const char *clsname, TDg &&dg) {
    if (!clsname || !clsname[0]) return nullptr;
    return ForEachChildOf(VClass::FindClass(clsname), dg);
  }

private:
  void CalcFieldOffsets ();
  void InitNetFields ();
  void InitReferences ();
  void InitDestructorFields ();
  void CreateVTable ();
  void CreateMethodMap (); // called from `CreateVTable()`
  void InitStatesLookup ();
#if !defined(IN_VCC)
  void CreateDefaults ();
#endif

public:
  friend inline VStream &operator << (VStream &Strm, VClass *&Obj) { return Strm << *(VMemberBase **)&Obj; }

  friend class VPackage;
};
