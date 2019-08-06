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

// ////////////////////////////////////////////////////////////////////////// //
class VExpression;
class VStatement;
class VLexer;

#define ANY_PACKAGE  ((VPackage*)-1)
#define ANY_MEMBER   (255)


enum {
  MEMBER_Package,
  MEMBER_Field,
  MEMBER_Property,
  MEMBER_Method,
  MEMBER_State,
  MEMBER_Const,
  MEMBER_Struct,
  MEMBER_Class,

  // a fake type for DECORATE class imports
  MEMBER_DecorateClass,
};

enum FERes {
  FOREACH_NEXT,
  FOREACH_STOP,
};


//==========================================================================
//
//  VMemberBase
//
//  The base class of all objects.
//
//==========================================================================
class VMemberBase {
public:
  // some global options
  static bool optDeprecatedLaxOverride; // true: don't require `override` on overriden methods
  static bool optDeprecatedLaxStates; // true: ignore missing states in state resolver

protected:
  vuint32 mMemberId; // never zero, monotonically increasing
  static vuint32 lastUsedMemberId; // monotonically increasing

public:
  // internal variables
  vuint8 MemberType;
  vint32 MemberIndex;
  VName Name;
  VMemberBase *Outer;
  TLocation Loc;

private:
  VMemberBase *HashNext;
  VMemberBase *HashNextLC;

  static TMapNC<VName, VMemberBase *> gMembersMap;
  static TMapNC<VName, VMemberBase *> gMembersMapLC; // lower-cased names
  static TArray<VPackage *> gPackageList;

  static void PutToNameHash (VMemberBase *self);
  static void RemoveFromNameHash (VMemberBase *self);

  static void DumpNameMap (TMapNC<VName, VMemberBase *> &map, bool caseSensitive);

public:
  static bool GObjInitialised;
  static bool GObjShuttingDown;
  static TArray<VMemberBase *> GMembers;

  static TArray<VStr> GPackagePath;
  static TArray<VPackage *> GLoadedPackages;
  static TArray<VClass *> GDecorateClassImports;

  static VClass *GClasses; // linked list of all classes

  static bool unsafeCodeAllowed; // true by default
  static bool unsafeCodeWarning; // true by default
  static bool koraxCompatibility; // false by default
  static bool koraxCompatibilityWarnings; // true by default

public:
  VMemberBase (vuint8, VName, VMemberBase *, const TLocation &);
  virtual ~VMemberBase ();

  virtual void CompilerShutdown ();

  inline vuint32 GetMemberId () const { return mMemberId; }

  // for each name
  // WARNING! don't add/remove ANY named members from callback!
  // return `FOREACH_STOP` from callback to stop (and return current member)
  // template function should accept `VMemberBase *`, and return `FERes`
  // templated, so i can use lambdas
  // k8: don't even ask me. fuck shitplusplus.
  template<typename TDg> static VMemberBase *ForEachNamed (VName aname, TDg &&dg, bool caseSensitive=true) {
    decltype(dg((VMemberBase *)nullptr)) test_ = FERes::FOREACH_NEXT;
    (void)test_;
    if (aname == NAME_None) return nullptr; // oops
    if (!caseSensitive) {
      // use lower-case map
      aname = VName(*aname, VName::FindLower);
      if (aname == NAME_None) return nullptr; // no such name, no chance to find a member
      VMemberBase **mpp = gMembersMapLC.find(aname);
      if (!mpp) return nullptr;
      for (VMemberBase *m = *mpp; m; m = m->HashNextLC) {
        FERes res = dg(m);
        if (res == FERes::FOREACH_STOP) return m;
      }
    } else {
      // use normal map
      VMemberBase **mpp = gMembersMap.find(aname);
      if (!mpp) return nullptr;
      for (VMemberBase *m = *mpp; m; m = m->HashNext) {
        FERes res = dg(m);
        if (res == FERes::FOREACH_STOP) return m;
      }
    }
    return nullptr;
  }
  template<typename TDg> static inline VMemberBase *ForEachNamedCI (VName aname, TDg &&dg) { return ForEachNamed(aname, dg, false); }

  // accessors
  inline const char *GetName () const { return *Name; }
  inline const VName GetVName () const { return Name; }
  VStr GetFullName () const;
  VPackage *GetPackage () const;
  bool IsIn (VMemberBase *) const;

  virtual void Serialise (VStream &);
  virtual void PostLoad ();
  virtual void Shutdown ();

  static void DumpNameMaps ();

  static void StaticInit (); // don't call directly!
  static void StaticExit (); // don't call directly!

  static void StaticAddPackagePath (const char *);
  static VPackage *StaticLoadPackage (VName, const TLocation &);
  static VMemberBase *StaticFindMember (VName AName, VMemberBase *AOuter, vuint8 AType, VName EnumName=NAME_None/*, bool caseSensitive=true*/);
  //static inline VMemberBase *StaticFindMemberNoCase (VName AName, VMemberBase *AOuter, vuint8 AType, VName EnumName=NAME_None) { return StaticFindMember(AName, AOuter, AType, EnumName, false); }

  //FIXME: this looks ugly
  static VFieldType StaticFindType (VClass *, VName);
  static VClass *StaticFindClass (VName AName, bool caseSensitive=true);
  static inline VClass *StaticFindClassNoCase (VName AName) { return StaticFindClass(AName, false); }

  // will not clear `list`
  static void StaticGetClassListNoCase (TArray<VStr> &list, const VStr &prefix, VClass *isaClass=nullptr);

  static VClass *StaticFindClassByGameObjName (VName aname, VName pkgname);

  static void StaticSplitStateLabel (const VStr &LabelName, TArray<VName> &Parts, bool appendToParts=false);

  static void StaticAddIncludePath (const char *);
  static void StaticAddDefine (const char *);

  static void InitLexer (VLexer &lex);

  // call this when you will definitely won't compile anything anymore
  // this will free some AST leftovers and other intermediate compiler data
  static void StaticCompilerShutdown ();

  static bool doAsmDump;

public:
  // virtual file system callback for reading source files (can be empty, and is empty by default)
  static void *userdata; // arbitrary pointer, not used by the lexer
  // should return `null` if file not found
  // should NOT fail if file not found
  static VStream *(*dgOpenFile) (const VStr &filename, void *userdata);

private:
  static TArray<VStr> incpathlist;
  static TArray<VStr> definelist;
};

inline vuint32 GetTypeHash (const VMemberBase *M) { return (M ? hashU32(M->GetMemberId()) : 0); }
