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
#include "vc_local.h"


// ////////////////////////////////////////////////////////////////////////// //
#if defined(IN_VCC)
#define PROG_MAGIC    "VPRG"
#define PROG_VERSION  (53)

struct dprograms_t {
  char magic[4]; // "VPRG"
  int version;

  int ofs_names;
  int num_names;

  int num_strings;
  int ofs_strings;

  int ofs_mobjinfo;
  int num_mobjinfo;

  int ofs_scriptids;
  int num_scriptids;

  int ofs_exportinfo;
  int ofs_exportdata;
  int num_exports;

  int ofs_imports;
  int num_imports;
};


// ////////////////////////////////////////////////////////////////////////// //
struct VProgsImport {
  vuint8 Type;
  VName Name;
  vint32 OuterIndex;
  VName ParentClassName;  // for decorate class imports

  VMemberBase *Obj;

  VProgsImport () : Type(0), Name(NAME_None), OuterIndex(0), Obj(0) {}
  VProgsImport (VMemberBase *InObj, vint32 InOuterIndex);

  friend VStream &operator << (VStream &Strm, VProgsImport &I) {
    Strm << I.Type << I.Name << STRM_INDEX(I.OuterIndex);
    if (I.Type == MEMBER_DecorateClass) Strm << I.ParentClassName;
    return Strm;
  }
};


// ////////////////////////////////////////////////////////////////////////// //
struct VProgsExport {
  vuint8 Type;
  VName Name;

  VMemberBase *Obj;

  VProgsExport () : Type(0), Name(NAME_None), Obj(0) {}
  VProgsExport(VMemberBase *InObj);

  friend VStream &operator << (VStream &Strm, VProgsExport &E) {
    return Strm << E.Type << E.Name;
  }
};


//==========================================================================
//
//  VProgsImport::VProgsImport
//
//==========================================================================
VProgsImport::VProgsImport (VMemberBase *InObj, vint32 InOuterIndex)
  : Type(InObj->MemberType)
  , Name(InObj->Name)
  , OuterIndex(InOuterIndex)
  , Obj(InObj)
{
}


//==========================================================================
//
//  VProgsExport::VProgsExport
//
//==========================================================================
VProgsExport::VProgsExport (VMemberBase *InObj)
  : Type(InObj->MemberType)
  , Name(InObj->Name)
  , Obj(InObj)
{
}


//==========================================================================
//
//  VProgsWriter
//
//==========================================================================
class VProgsWriter : public VStream {
private:
  FILE *File;

public:
  vint32 *NamesMap;
  vint32 *MembersMap;
  TArray<VName> Names;
  TArray<VProgsImport> Imports;
  TArray<VProgsExport> Exports;

  VProgsWriter (FILE *InFile) : File(InFile) {
    bLoading = false;
    NamesMap = new vint32[VName::GetNumNames()];
    for (int i = 0; i < VName::GetNumNames(); ++i) NamesMap[i] = -1;
    MembersMap = new vint32[VMemberBase::GMembers.Num()];
    memset(MembersMap, 0, VMemberBase::GMembers.Num()*sizeof(vint32));
  }

  virtual ~VProgsWriter () override { Close(); }

  // stream interface
  virtual void Seek (int InPos) override { if (fseek(File, InPos, SEEK_SET)) bError = true; }
  virtual int Tell () override { return ftell(File); }
  virtual int TotalSize () override {
    int CurPos = ftell(File);
    fseek(File, 0, SEEK_END);
    int Size = ftell(File);
    fseek(File, CurPos, SEEK_SET);
    return Size;
  }
  virtual bool AtEnd () override { return !!feof(File); }
  virtual bool Close () override { return !bError; }
  virtual void Serialise (void *V, int Length) override { if (fwrite(V, Length, 1, File) != 1) bError = true; }
  virtual void Flush () override { if (fflush(File)) bError = true; }

  virtual void io (VName &Name) override {
    int TmpIdx = NamesMap[Name.GetIndex()];
    *this << STRM_INDEX(TmpIdx);
  }

  virtual void io (VMemberBase *&Ref) override {
    int TmpIdx = (Ref ? MembersMap[Ref->MemberIndex] : 0);
    *this << STRM_INDEX(TmpIdx);
  }

  int GetMemberIndex (VMemberBase *Obj) {
    if (!Obj) return 0;
    if (!MembersMap[Obj->MemberIndex]) {
      MembersMap[Obj->MemberIndex] = -Imports.Append(VProgsImport(Obj, GetMemberIndex(Obj->Outer)))-1;
    }
    return MembersMap[Obj->MemberIndex];
  }

  void AddExport (VMemberBase *Obj) {
    MembersMap[Obj->MemberIndex] = Exports.Append(VProgsExport(Obj))+1;
  }
};


//==========================================================================
//
//  VImportsCollector
//
//==========================================================================
class VImportsCollector : public VStream {
  VProgsWriter &Writer;
  VPackage *Package;

public:
  VImportsCollector (VProgsWriter &AWriter, VPackage *APackage) : Writer(AWriter) , Package(APackage) {
    bLoading = false;
  }

  virtual void io (VName &Name) override {
    if (Writer.NamesMap[Name.GetIndex()] == -1) Writer.NamesMap[Name.GetIndex()] = Writer.Names.Append(Name);
  }

  virtual void io (VMemberBase *&Ref) override {
    if (Ref != Package) Writer.GetMemberIndex(Ref);
  }
};


//==========================================================================
//
//  VProgsReader
//
//==========================================================================
class VProgsReader : public VStream {
private:
  VStream *Stream;

public:
  VName *NameRemap;
  int NumImports;
  VProgsImport *Imports;
  int NumExports;
  VProgsExport *Exports;

  VProgsReader (VStream *InStream)
    : Stream(InStream)
    , NameRemap(0)
    , NumImports(0)
    , NumExports(0)
    , Exports(0)
  {
    bLoading = true;
  }

  virtual ~VProgsReader ()  override{
    Close();
    delete[] NameRemap;
    delete[] Imports;
    delete[] Exports;
    delete Stream;
  }

  // stream interface
  virtual void Serialise (void *Data, int Len) override { Stream->Serialise(Data, Len); }
  virtual void Seek (int Pos) override { Stream->Seek(Pos); }
  virtual int Tell () override { return Stream->Tell(); }
  virtual int TotalSize () override { return Stream->TotalSize(); }
  virtual bool AtEnd () override { return Stream->AtEnd(); }
  virtual void Flush () override { Stream->Flush(); }
  virtual bool Close () override { return Stream->Close(); }

  virtual void io (VName &Name) override {
    int NameIndex;
    *this << STRM_INDEX(NameIndex);
    Name = NameRemap[NameIndex];
  }

  virtual void io (VMemberBase *&Ref) override {
    int ObjIndex;
    *this << STRM_INDEX(ObjIndex);
    if (ObjIndex > 0) {
      check(ObjIndex <= NumExports);
      Ref = Exports[ObjIndex-1].Obj;
    } else if (ObjIndex < 0) {
      check(-ObjIndex <= NumImports);
      Ref = Imports[-ObjIndex-1].Obj;
    } else {
      Ref = nullptr;
    }
  }

  VMemberBase *GetImport (int Index) {
    VProgsImport &I = Imports[Index];
    if (!I.Obj) {
      if (I.Type == MEMBER_Package) {
        I.Obj = VMemberBase::StaticLoadPackage(I.Name, TLocation());
      } else if (I.Type == MEMBER_DecorateClass) {
        for (int i = 0; i < VMemberBase::GDecorateClassImports.Num(); ++i) {
          if (VMemberBase::GDecorateClassImports[i]->Name == I.Name) {
            I.Obj = VMemberBase::GDecorateClassImports[i];
            break;
          }
        }
        if (!I.Obj) I.Obj = VClass::FindClass(*I.Name);
        if (!I.Obj) {
          VClass *Tmp = new VClass(I.Name, nullptr, TLocation());
          Tmp->MemberType = MEMBER_DecorateClass;
          Tmp->ParentClassName = I.ParentClassName;
          VMemberBase::GDecorateClassImports.Append(Tmp);
          I.Obj = Tmp;
        }
      } else {
        I.Obj = VMemberBase::StaticFindMember(I.Name, GetImport(-I.OuterIndex-1), I.Type);
      }
    }
    return I.Obj;
  }

  void ResolveImports () {
    for (int i = 0; i < NumImports; ++i) GetImport(i);
  }
};
#endif // IN_VCC


//==========================================================================
//
//  VPackage::VPackage
//
//==========================================================================
VPackage::VPackage ()
  : VMemberBase(MEMBER_Package, NAME_None, nullptr, TLocation())
  , StringCount(0)
  , KnownEnums()
  , NumBuiltins(0)
  //, Checksum(0)
  //, Reader(nullptr)
{
  InitStringPool();
}


//==========================================================================
//
//  VPackage::VPackage
//
//==========================================================================
VPackage::VPackage (VName AName)
  : VMemberBase(MEMBER_Package, AName, nullptr, TLocation())
  , KnownEnums()
  , NumBuiltins(0)
  //, Checksum(0)
  //, Reader(nullptr)
{
  InitStringPool();
}


//==========================================================================
//
//  VPackage::~VPackage
//
//==========================================================================
VPackage::~VPackage () {
}


//==========================================================================
//
//  VPackage::~VPackage
//
//==========================================================================
void VPackage::InitStringPool () {
  // strings
  memset(StringLookup, 0, sizeof(StringLookup));
  // 1-st string is empty
  StringInfo.Alloc();
  StringInfo[0].Offs = 0;
  StringInfo[0].Next = 0;
  StringCount = 1;
#if defined(VCC_OLD_PACKAGE_STRING_POOL)
  Strings.SetNum(4);
  memset(Strings.Ptr(), 0, 4);
#else
  StringInfo[0].str = VStr::EmptyString;
#endif
}


//==========================================================================
//
//  VPackage::CompilerShutdown
//
//==========================================================================
void VPackage::CompilerShutdown () {
  VMemberBase::CompilerShutdown();
  ParsedConstants.clear();
  ParsedStructs.clear();
  ParsedClasses.clear();
  ParsedDecorateImportClasses.clear();
}


//==========================================================================
//
//  VPackage::Serialise
//
//==========================================================================
void VPackage::Serialise (VStream &Strm) {
  VMemberBase::Serialise(Strm);
  vuint8 xver = 0; // current version is 0
  Strm << xver;
  // enums
  vint32 acount = (vint32)KnownEnums.count();
  Strm << STRM_INDEX(acount);
  if (Strm.IsLoading()) {
    KnownEnums.clear();
    while (acount-- > 0) {
      VName ename;
      Strm << ename;
      KnownEnums.put(ename, true);
    }
  } else {
    for (auto it = KnownEnums.first(); it; ++it) {
      VName ename = it.getKey();
      Strm << ename;
    }
  }
}


//==========================================================================
//
//  VPackage::StringHashFunc
//
//==========================================================================
vuint32 VPackage::StringHashFunc (const char *str) {
  //return (str && str[0] ? (str[0]^(str[1]<<4))&0xff : 0);
  return (str && str[0] ? joaatHashBuf(str, strlen(str)) : 0)&0x0fffU;
}


//==========================================================================
//
//  VPackage::FindString
//
//  Return offset in strings array.
//
//==========================================================================
int VPackage::FindString (const char *str) {
  if (!str || !str[0]) return 0;

  vuint32 hash = StringHashFunc(str);
  for (int i = StringLookup[hash]; i; i = StringInfo[i].Next) {
#if defined(VCC_OLD_PACKAGE_STRING_POOL)
    if (VStr::Cmp(&Strings[StringInfo[i].Offs], str) == 0) {
      return StringInfo[i].Offs;
    }
#else
    if (StringInfo[i].str.Cmp(str) == 0) {
      return StringInfo[i].Offs;
    }
#endif
  }

  // add new string
#if defined(VCC_OLD_PACKAGE_STRING_POOL)
  TStringInfo &SI = StringInfo.Alloc();
  int AddLen = VStr::Length(str)+1;
  while (AddLen&3) ++AddLen;
  int Ofs = Strings.Num();
  Strings.SetNum(Strings.Num()+AddLen);
  memset(&Strings[Ofs], 0, AddLen);
  VStr::Cpy(&Strings[Ofs], str);
#else
  int Ofs = StringInfo.length();
  check(Ofs == StringCount);
  ++StringCount;
  TStringInfo &SI = StringInfo.Alloc();
  // remember string
  SI.str = VStr(str);
  SI.str.makeImmutable();
#endif
  SI.Offs = Ofs;
  SI.Next = StringLookup[hash];
  StringLookup[hash] = StringInfo.length()-1;
  return SI.Offs;
}


//==========================================================================
//
//  VPackage::FindString
//
//  Return offset in strings array.
//
//==========================================================================
int VPackage::FindString (VStr s) {
  if (s.length() == 0) return 0;
  return FindString(*s);
}


//==========================================================================
//
//  VPackage::GetStringByIndex
//
//==========================================================================
const VStr &VPackage::GetStringByIndex (int idx) {
#if defined(VCC_OLD_PACKAGE_STRING_POOL)
  if (idx < 0 || idx >= Strings.length()) return VStr::EmptyString;
  return VStr(&Package->Strings[idx]);
#else
  if (idx < 0 || idx >= StringInfo.length()) return VStr::EmptyString;
  return StringInfo[idx].str;
#endif
}


//==========================================================================
//
//  VPackage::IsKnownEnum
//
//==========================================================================
bool VPackage::IsKnownEnum (VName EnumName) {
  return KnownEnums.has(EnumName);
}


//==========================================================================
//
//  VClass::AddKnownEnum
//
//==========================================================================
bool VPackage::AddKnownEnum (VName EnumName) {
  if (IsKnownEnum(EnumName)) return true;
  KnownEnums.put(EnumName, true);
  return false;
}


//==========================================================================
//
//  VPackage::FindConstant
//
//==========================================================================
VConstant *VPackage::FindConstant (VName Name, VName EnumName) {
  VMemberBase *m = StaticFindMember(Name, this, MEMBER_Const, EnumName);
  if (m) return (VConstant *)m;
  return nullptr;
}


//==========================================================================
//
//  VPackage::FindDecorateImportClass
//
//==========================================================================
VClass *VPackage::FindDecorateImportClass (VName AName) const {
  for (int i = 0; i < ParsedDecorateImportClasses.Num(); ++i) {
    if (ParsedDecorateImportClasses[i]->Name == AName) {
      return ParsedDecorateImportClasses[i];
    }
  }
  return nullptr;
}


//==========================================================================
//
//  VPackage::Emit
//
//==========================================================================
void VPackage::Emit () {
  devprintf("Importing packages\n");
  for (int i = 0; i < PackagesToLoad.Num(); ++i) {
    devprintf("  importing package '%s'...\n", *PackagesToLoad[i].Name);
    PackagesToLoad[i].Pkg = StaticLoadPackage(PackagesToLoad[i].Name, PackagesToLoad[i].Loc);
  }

  if (vcErrorCount) BailOut();

  devprintf("Defining constants\n");
  for (int i = 0; i < ParsedConstants.Num(); ++i) {
    devprintf("  defining constant '%s'...\n", *ParsedConstants[i]->Name);
    ParsedConstants[i]->Define();
  }

  devprintf("Defining structs\n");
  for (int i = 0; i < ParsedStructs.Num(); ++i) {
    devprintf("  defining struct '%s'...\n", *ParsedStructs[i]->Name);
    ParsedStructs[i]->Define();
  }

  devprintf("Defining classes\n");
  for (int i = 0; i < ParsedClasses.Num(); ++i) {
    devprintf("  defining class '%s'...\n", *ParsedClasses[i]->Name);
    ParsedClasses[i]->Define();
  }

  for (int i = 0; i < ParsedDecorateImportClasses.Num(); ++i) {
    devprintf("  defining decorate import class '%s'...\n", *ParsedDecorateImportClasses[i]->Name);
    ParsedDecorateImportClasses[i]->Define();
  }

  if (vcErrorCount) BailOut();

  devprintf("Defining struct members\n");
  for (int i = 0; i < ParsedStructs.Num(); ++i) {
    ParsedStructs[i]->DefineMembers();
  }

  devprintf("Defining class members\n");
  for (int i = 0; i < ParsedClasses.Num(); ++i) {
    ParsedClasses[i]->DefineMembers();
  }

  if (vcErrorCount) BailOut();

  devprintf("Emiting classes\n");
  for (int i = 0; i < ParsedClasses.Num(); ++i) {
    ParsedClasses[i]->Emit();
  }

  if (vcErrorCount) BailOut();
}


#if defined(IN_VCC)
//==========================================================================
//
//  VPackage::WriteObject
//
//==========================================================================
void VPackage::WriteObject (VStr name) {
  FILE *f;
  dprograms_t progs;

  devprintf("Writing object\n");

  f = fopen(*name, "wb");
  if (!f) FatalError("Can't open file \"%s\".", *name);

  VProgsWriter Writer(f);
  for (int i = 0; i < VMemberBase::GMembers.Num(); ++i) {
    if (VMemberBase::GMembers[i]->IsIn(this)) Writer.AddExport(VMemberBase::GMembers[i]);
  }

  // add decorate class imports
  for (int i = 0; i < ParsedDecorateImportClasses.Num(); ++i) {
    VProgsImport I(ParsedDecorateImportClasses[i], 0);
    I.Type = MEMBER_DecorateClass;
    I.ParentClassName = ParsedDecorateImportClasses[i]->ParentClassName;
    Writer.MembersMap[ParsedDecorateImportClasses[i]->MemberIndex] = -Writer.Imports.Append(I)-1;
  }

  // collect list of imported objects and used names
  VImportsCollector Collector(Writer, this);
  for (int i = 0; i < Writer.Exports.Num(); ++i) Collector << Writer.Exports[i];
  for (int i = 0; i < Writer.Exports.Num(); ++i) Writer.Exports[i].Obj->Serialise(Collector);
  for (int i = 0; i < Writer.Imports.Num(); ++i) Collector << Writer.Imports[i];

  // now write the object file
  memset(&progs, 0, sizeof(progs));
  Writer.Serialise(&progs, sizeof(progs));

  // serialise names
  progs.ofs_names = Writer.Tell();
  progs.num_names = Writer.Names.Num();
  for (int i = 0; i < Writer.Names.Num(); ++i) {
    //Writer << *VName::GetEntry(Writer.Names[i].GetIndex());
    const char *EName = *Writer.Names[i];
    vuint8 len = (vuint8)VStr::length(EName);
    Writer << len;
    if (len) Writer.Serialise((void *)EName, len);
  }

  progs.ofs_strings = Writer.Tell();
#if defined(VCC_OLD_PACKAGE_STRING_POOL)
  progs.num_strings = Strings.Num();
  Writer.Serialise(&Strings[0], Strings.Num());
#else
  progs.num_strings = StringInfo.length();
  vint32 count = progs.num_strings;
  Writer << count;
  for (int stridx = 0; stridx < StringInfo.length(); ++stridx) {
    Writer << StringInfo[stridx].str;
  }
#endif

  //FIXME
  //progs.ofs_mobjinfo = Writer.Tell();
  //progs.num_mobjinfo = VClass::GMobjInfos.Num();
  //for (int i = 0; i < VClass::GMobjInfos.Num(); ++i) Writer << VClass::GMobjInfos[i];

  //progs.ofs_scriptids = Writer.Tell();
  //progs.num_scriptids = VClass::GScriptIds.Num();
  //for (int i = 0; i < VClass::GScriptIds.Num(); ++i) Writer << VClass::GScriptIds[i];

  // serialise imports
  progs.num_imports = Writer.Imports.Num();
  progs.ofs_imports = Writer.Tell();
  for (int i = 0; i < Writer.Imports.Num(); ++i) Writer << Writer.Imports[i];

  progs.num_exports = Writer.Exports.Num();

  // serialise object infos
  progs.ofs_exportinfo = Writer.Tell();
  for (int i = 0; i < Writer.Exports.Num(); ++i) Writer << Writer.Exports[i];

  // serialise objects
  progs.ofs_exportdata = Writer.Tell();
  for (int i = 0; i < Writer.Exports.Num(); ++i) Writer.Exports[i].Obj->Serialise(Writer);

#if !defined(VCC_STANDALONE_EXECUTOR)
  // print statistics
  devprintf("            count   size\n");
  devprintf("Header     %6d %6ld\n", 1, (long int)sizeof(progs));
  devprintf("Names      %6d %6d\n", Writer.Names.Num(), progs.ofs_strings - progs.ofs_names);
#if defined(VCC_OLD_PACKAGE_STRING_POOL)
  devprintf("Strings    %6d %6d\n", StringInfo.Num(), Strings.Num());
#else
  devprintf("Strings    %6d\n", StringInfo.Num());
#endif
  devprintf("Builtins   %6d\n", NumBuiltins);
  //devprintf("Mobj info  %6d %6d\n", VClass::GMobjInfos.Num(), progs.ofs_scriptids - progs.ofs_mobjinfo);
  //devprintf("Script Ids %6d %6d\n", VClass::GScriptIds.Num(), progs.ofs_imports - progs.ofs_scriptids);
  devprintf("Imports    %6d %6d\n", Writer.Imports.Num(), progs.ofs_exportinfo - progs.ofs_imports);
  devprintf("Exports    %6d %6d\n", Writer.Exports.Num(), progs.ofs_exportdata - progs.ofs_exportinfo);
  devprintf("Type data  %6d %6d\n", Writer.Exports.Num(), Writer.Tell() - progs.ofs_exportdata);
  devprintf("TOTAL SIZE       %7d\n", Writer.Tell());
#endif

  // write header
  memcpy(progs.magic, PROG_MAGIC, 4);
  progs.version = PROG_VERSION;
  Writer.Seek(0);
  Writer.Serialise(progs.magic, 4);
  for (int i = 1; i < (int)sizeof(progs)/4; ++i) Writer << ((int *)&progs)[i];

#ifdef OPCODE_STATS
  devprintf("\n-----------------------------------------------\n\n");
  for (int i = 0; i < NUM_OPCODES; ++i) {
    devprintf("%-16s %d\n", StatementInfo[i].name, StatementInfo[i].usecount);
  }
  devprintf("%d opcodes\n", NUM_OPCODES);
#endif

  fclose(f);
}
#endif // IN_VCC


//==========================================================================
//
//  VPackage::LoadSourceObject
//
//  will delete `Strm`
//
//==========================================================================
void VPackage::LoadSourceObject (VStream *Strm, VStr filename, TLocation l) {
  if (!Strm) return;

  VLexer Lex;
  VMemberBase::InitLexer(Lex);
  Lex.OpenSource(Strm, filename);
  VParser Parser(Lex, this);
  Parser.Parse();
  Emit();

#if !defined(IN_VCC)
  //fprintf(stderr, "*** PACKAGE: %s\n", *Name);
  // copy mobj infos and spawn IDs
  //for (int i = 0; i < MobjInfo.Num(); ++i) VClass::GMobjInfos.Alloc() = MobjInfo[i];
  //for (int i = 0; i < ScriptIds.Num(); ++i) VClass::GScriptIds.Alloc() = ScriptIds[i];
  for (int i = 0; i < GMembers.Num(); ++i) {
    if (GMembers[i]->IsIn(this)) {
      //fprintf(stderr, "  *** postload for '%s'...\n", *GMembers[i]->Name);
      GMembers[i]->PostLoad();
    }
  }

  // create default objects
  for (int i = 0; i < ParsedClasses.Num(); ++i) {
    //fprintf(stderr, "  *** creating defaults for '%s'... (%d)\n", *ParsedClasses[i]->Name, (int)ParsedClasses[i]->Defined);
    ParsedClasses[i]->CreateDefaults();
    if (!ParsedClasses[i]->Outer) {
      //fprintf(stderr, "    *** setting outer\n");
      ParsedClasses[i]->Outer = this;
    }
  }

  #if !defined(VCC_STANDALONE_EXECUTOR)
  // we need to do this, 'cause k8vavoom 'engine' package has some classes w/o definitions (`Acs`, `Button`)
  if (Name == NAME_engine) {
    for (VClass *Cls = GClasses; Cls; Cls = Cls->LinkNext) {
      if (!Cls->Outer && Cls->MemberType == MEMBER_Class) {
        GLog.Logf(NAME_Warning, "package `engine` has hidden class `%s` (this is harmless)", *Cls->Name);
        Cls->PostLoad();
        Cls->CreateDefaults();
        Cls->Outer = this;
      }
    }
  }
  #endif
#endif
}


#if defined(IN_VCC)
//==========================================================================
//
// VPackage::LoadBinaryObject
//
// will delete `Strm`
//
//==========================================================================
void VPackage::LoadBinaryObject (VStream *Strm, VStr filename, TLocation l) {
  if (!Strm) return;

  VProgsReader *Reader = new VProgsReader(Strm);

  // calcutate CRC
/*
#if !defined(IN_VCC)
  auto crc = TCRC16();
  for (int i = 0; i < Reader->TotalSize(); ++i) crc += Streamer<vuint8>(*Reader);
#endif
*/

  // read the header
  dprograms_t Progs;
  Reader->Seek(0);
  Reader->Serialise(Progs.magic, 4);
  for (int i = 1; i < (int)sizeof(Progs)/4; ++i) *Reader << ((int *)&Progs)[i];

  if (VStr::NCmp(Progs.magic, PROG_MAGIC, 4)) {
    ParseError(l, "Package '%s' has wrong file ID", *Name);
    BailOut();
  }
  if (Progs.version != PROG_VERSION) {
    ParseError(l, "Package '%s' has wrong version number (%i should be %i)", *Name, Progs.version, PROG_VERSION);
    BailOut();
  }

  // read names
  VName *NameRemap = new VName[Progs.num_names];
  Reader->Seek(Progs.ofs_names);
  for (int i = 0; i < Progs.num_names; ++i) {
    /*
    VNameEntry E;
    *Reader << E;
    NameRemap[i] = E.Name;
    */
    char EName[NAME_SIZE+1];
    vuint8 len = 0;
    *Reader << len;
    check(len <= NAME_SIZE);
    if (len) Reader->Serialise(EName, len);
    EName[len] = 0;
    NameRemap[i] = VName(EName);
  }
  Reader->NameRemap = NameRemap;

  Reader->Imports = new VProgsImport[Progs.num_imports];
  Reader->NumImports = Progs.num_imports;
  Reader->Seek(Progs.ofs_imports);
  for (int i = 0; i < Progs.num_imports; ++i) *Reader << Reader->Imports[i];
  Reader->ResolveImports();

  VProgsExport *Exports = new VProgsExport[Progs.num_exports];
  Reader->Exports = Exports;
  Reader->NumExports = Progs.num_exports;

/*
#if !defined(IN_VCC)
  Checksum = crc;
  this->Reader = Reader;
#endif
*/

  // create objects
  Reader->Seek(Progs.ofs_exportinfo);
  for (int i = 0; i < Progs.num_exports; ++i) {
    *Reader << Exports[i];
    switch (Exports[i].Type) {
      case MEMBER_Package: Exports[i].Obj = new VPackage(Exports[i].Name); break;
      case MEMBER_Field: Exports[i].Obj = new VField(Exports[i].Name, nullptr, TLocation()); break;
      case MEMBER_Property: Exports[i].Obj = new VProperty(Exports[i].Name, nullptr, TLocation()); break;
      case MEMBER_Method: Exports[i].Obj = new VMethod(Exports[i].Name, nullptr, TLocation()); break;
      case MEMBER_State: Exports[i].Obj = new VState(Exports[i].Name, nullptr, TLocation()); break;
      case MEMBER_Const: Exports[i].Obj = new VConstant(Exports[i].Name, nullptr, TLocation()); break;
      case MEMBER_Struct: Exports[i].Obj = new VStruct(Exports[i].Name, nullptr, TLocation()); break;
      case MEMBER_Class:
#if !defined(IN_VCC)
        Exports[i].Obj = VClass::FindClass(*Exports[i].Name);
        if (!Exports[i].Obj)
#endif
        {
          Exports[i].Obj = new VClass(Exports[i].Name, nullptr, TLocation());
        }
        break;
      default: ParseError(l, "Package '%s' contains corrupted data", *Name); BailOut();
    }
  }

  // read strings
#if defined(VCC_OLD_PACKAGE_STRING_POOL)
  Strings.SetNum(Progs.num_strings);
  Reader->Seek(Progs.ofs_strings);
  Reader->Serialise(Strings.Ptr(), Progs.num_strings);
#else
  Reader->Seek(Progs.ofs_strings);
  StringInfo.clear();
  InitStringPool();
  {
    vint32 count = 0;
    *Reader << count;
    if (count < 0 || count >= 1024*1024*32) { ParseError(l, "Package '%s' contains corrupted string data", *Name); BailOut(); }
    for (int stridx = 0; stridx < count; ++stridx) {
      VStr s;
      *Reader << s;
      if (stridx == 0) {
        if (s.length() != 0) { ParseError(l, "Package '%s' contains corrupted string data", *Name); BailOut(); }
      } else {
        if (s.length() == 0 || !s[0]) { ParseError(l, "Package '%s' contains corrupted string data", *Name); BailOut(); }
      }
      int n = FindString(s);
      if (n != stridx) { ParseError(l, "Package '%s' contains corrupted string data", *Name); BailOut(); }
    }
  }
#endif

  // serialise objects
  Reader->Seek(Progs.ofs_exportdata);
  for (int i = 0; i < Progs.num_exports; ++i) {
    Exports[i].Obj->Serialise(*Reader);
    if (!Exports[i].Obj->Outer) Exports[i].Obj->Outer = this;
  }

#if !defined(IN_VCC)
  // set up info tables
  //Reader->Seek(Progs.ofs_mobjinfo);
  //for (int i = 0; i < Progs.num_mobjinfo; ++i) *Reader << VClass::GMobjInfos.Alloc();

  //Reader->Seek(Progs.ofs_scriptids);
  //for (int i = 0; i < Progs.num_scriptids; ++i) *Reader << VClass::GScriptIds.Alloc();

  for (int i = 0; i < Progs.num_exports; ++i) Exports[i].Obj->PostLoad();

  // create default objects
  for (int i = 0; i < Progs.num_exports; ++i) {
    if (Exports[i].Obj->MemberType == MEMBER_Class) {
      VClass *vc = (VClass *)Exports[i].Obj;
      vc->CreateDefaults();
      if (!vc->Outer) vc->Outer = this;
    }
  }

# if !defined(VCC_STANDALONE_EXECUTOR)
  // we need to do this, 'cause k8vavoom 'engine' package has some classes w/o definitions (`Acs`, `Button`)
  if (Name == NAME_engine) {
    for (VClass *Cls = GClasses; Cls; Cls = Cls->LinkNext) {
      if (!Cls->Outer && Cls->MemberType == MEMBER_Class) {
        Cls->PostLoad();
        Cls->CreateDefaults();
        Cls->Outer = this;
      }
    }
  }
# endif
#endif

  //k8: fuck you, shitplusplus: no finally
  //this->Reader = nullptr;
  delete Reader;
}
#endif


//==========================================================================
//
// VPackage::LoadObject
//
//==========================================================================
//#if !defined(IN_VCC)
static const char *pkgImportFiles[] = {
  "0package.vc",
  "package.vc",
  "0classes.vc",
  "classes.vc",
  nullptr
};
//#endif


void VPackage::LoadObject (TLocation l) {
#if defined(IN_VCC)
  devprintf("Loading package %s\n", *Name);

  // load PROGS from a specified file
  VStream *f = fsysOpenFile(va("%s.dat", *Name));
  if (f) { LoadBinaryObject(f, va("%s.dat", *Name), l); return; }

  /*
  for (int i = 0; i < GPackagePath.length(); ++i) {
    VStr fname = GPackagePath[i]+"/"+Name+".dat";
    f = fsysOpenFile(*fname);
    if (f) { LoadBinaryObject(f, va("%s.dat", *Name), l); return; }
  }
  */

  for (int i = 0; i < GPackagePath.length(); ++i) {
    for (const char **pif = pkgImportFiles; *pif; ++pif) {
      //VStr mainVC = va("%s/progs/%s/%s", *GPackagePath[i], *Name, *pif);
      VStr mainVC = va("%s/%s/%s", *GPackagePath[i], *Name, *pif);
      //GLog.Logf(": <%s>", *mainVC);
      VStream *Strm = fsysOpenFile(*mainVC);
      if (Strm) {
        LoadSourceObject(Strm, mainVC, l);
        return;
      }
    }
  }

  ParseError(l, "Can't find package %s", *Name);

#elif defined(VCC_STANDALONE_EXECUTOR)
  devprintf("Loading package '%s'...\n", *Name);

  for (int i = 0; i < GPackagePath.length(); ++i) {
    for (const char **pif = pkgImportFiles; *pif; ++pif) {
      VStr mainVC = GPackagePath[i]+"/"+Name+"/"+(*pif);
      devprintf("  <%s>\n", *mainVC);
      VStream *Strm = fsysOpenFile(mainVC);
      if (Strm) { devprintf("  '%s'\n", *mainVC); LoadSourceObject(Strm, mainVC, l); return; }
    }
  }

  // if no package pathes specified, try "packages/"
  if (GPackagePath.length() == 0) {
    for (const char **pif = pkgImportFiles; *pif; ++pif) {
      VStr mainVC = VStr("packages/")+Name+"/"+(*pif);
      VStream *Strm = fsysOpenFile(mainVC);
      if (Strm) { devprintf("  '%s'\n", *mainVC); LoadSourceObject(Strm, mainVC, l); return; }
    }
  }

  // don't even try to load binary packages
  /*
  VStr mainVC = va("packages/%s.dat", *Name);
  VStream *Strm = (tnum ? fsysOpenFile(mainVC) : fsysOpenDiskFile(mainVC));
  if (Strm) { devprintf("  '%s'\n", *mainVC); LoadBinaryObject(Strm, mainVC, l); return; }
  */

  ParseError(l, "Can't find package %s", *Name);
  BailOut();

#else
  //fprintf(stderr, "Loading package '%s'...\n", *Name);

  // load PROGS from a specified file
  /*
  VStr mainVC = va("progs/%s.dat", *Name);
  VStream *Strm = FL_OpenFileRead(*mainVC);
  if (!Strm)
  */
  {
    for (const char **pif = pkgImportFiles; *pif; ++pif) {
      VStr mainVC = va("progs/%s/%s", *Name, *pif);
      if (FL_FileExists(*mainVC)) {
        // compile package
        //fprintf(stderr, "Loading package '%s' (%s)...\n", *Name, *mainVC);
        VStream *Strm = FL_OpenFileRead(*mainVC);
        LoadSourceObject(Strm, mainVC, l);
        return;
      }
    }
    Sys_Error("Progs package %s not found", *Name);
  }

  //LoadBinaryObject(Strm, mainVC, l);
#endif
}


/*
//==========================================================================
//
// VPackage::FindMObj
//
//==========================================================================
VClass *VPackage::FindMObj (vint32 id) const {
  int len = MobjInfo.length();
  for (int f = 0; f < len; ++f) if (MobjInfo[f].DoomEdNum == id) return MobjInfo[f].Class;
  return nullptr;
}


//==========================================================================
//
// VPackage::FindMObjInfo
//
//==========================================================================
mobjinfo_t *VPackage::FindMObjInfo (vint32 id) {
  int len = MobjInfo.length();
  for (int f = 0; f < len; ++f) if (MobjInfo[f].DoomEdNum == id) return &MobjInfo[f];
  return nullptr;
}


//==========================================================================
//
// VPackage::FindScriptId
//
//==========================================================================
VClass *VPackage::FindScriptId (vint32 id) const {
  int len = ScriptIds.length();
  for (int f = 0; f < len; ++f) if (ScriptIds[f].DoomEdNum == id) return ScriptIds[f].Class;
  return nullptr;
}
*/
