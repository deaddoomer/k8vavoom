//**************************************************************************
//**
//**	##   ##    ##    ##   ##   ####     ####   ###     ###
//**	##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**	 ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**	 ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**	  ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**	   #    ##    ##    #      ####     ####   ##       ##
//**
//**	$Id$
//**
//**	Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**	This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**	This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************

#include "gamedefs.h"
#include "progdefs.h"

bool					VMemberBase::GObjInitialized;
VClass*					VMemberBase::GClasses;
TArray<VMemberBase*>	VMemberBase::GMembers;
TArray<VPackage*>		VMemberBase::GLoadedPackages;

TArray<mobjinfo_t>		VClass::GMobjInfos;
TArray<mobjinfo_t>		VClass::GScriptIds;
TArray<VName>			VClass::GSpriteNames;
TArray<VName>			VClass::GModelNames;

#define DECLARE_OPC(name, args)		{ OPCARGS_##args }
#define OPCODE_INFO
static struct
{
	int		Args;
} OpcodeInfo[NUM_OPCODES] =
{
#include "progdefs.h"
};

//==========================================================================
//
//	VProgsImport
//
//==========================================================================

struct VProgsImport
{
	vuint8			Type;
	VName			Name;
	vint32			OuterIndex;

	VMemberBase*	Obj;

	VProgsImport()
	: Type(0)
	, Name(NAME_None)
	, OuterIndex(0)
	, Obj(0)
	{}

	friend VStream& operator<<(VStream& Strm, VProgsImport& I)
	{
		return Strm << I.Type << I.Name << STRM_INDEX(I.OuterIndex);
	}
};

//==========================================================================
//
//	VProgsExport
//
//==========================================================================

struct VProgsExport
{
	vuint8			Type;
	VName			Name;

	VMemberBase*	Obj;

	VProgsExport()
	: Type(0)
	, Name(NAME_None)
	, Obj(0)
	{}

	friend VStream& operator<<(VStream& Strm, VProgsExport& E)
	{
		return Strm << E.Type << E.Name;
	}
};

//==========================================================================
//
//	VProgsReader
//
//==========================================================================

class VProgsReader : public VStream
{
private:
	VStream*			Stream;

public:
	VName*				NameRemap;
	int					NumImports;
	VProgsImport*		Imports;
	int					NumExports;
	VProgsExport*		Exports;

	VProgsReader(VStream* InStream)
	: Stream(InStream)
	, NameRemap(0)
	{
		bLoading = true;
	}
	~VProgsReader()
	{
		delete[] NameRemap;
		delete[] Imports;
		delete[] Exports;
		delete Stream;
	}

	//	Stream interface.
	void Serialise(void* Data, int Len)
	{
		Stream->Serialise(Data, Len);
	}
	void Seek(int Pos)
	{
		Stream->Seek(Pos);
	}
	int Tell()
	{
		return Stream->Tell();
	}
	int TotalSize()
	{
		return Stream->TotalSize();
	}
	bool AtEnd()
	{
		return Stream->AtEnd();
	}
	void Flush()
	{
		Stream->Flush();
	}
	bool Close()
	{
		return Stream->Close();
	}

	VStream& operator<<(VName& Name)
	{
		int NameIndex;
		*this << STRM_INDEX(NameIndex);
		Name = NameRemap[NameIndex];
		return *this;
	}
	VStream& operator<<(VMemberBase*& Ref)
	{
		int ObjIndex;
		*this << STRM_INDEX(ObjIndex);
		if (ObjIndex > 0)
		{
			check(ObjIndex <= NumExports);
			Ref = Exports[ObjIndex - 1].Obj;
		}
		else if (ObjIndex < 0)
		{
			check(-ObjIndex <= NumImports);
			Ref = Imports[-ObjIndex - 1].Obj;
		}
		else
		{
			Ref = NULL;
		}
		return *this;
	}

	VMemberBase* GetImport(int Index)
	{
		VProgsImport& I = Imports[Index];
		if (!I.Obj)
		{
			if (I.Type == MEMBER_Package)
				I.Obj = VMemberBase::StaticLoadPackage(I.Name);
			else
				I.Obj = VMemberBase::StaticFindMember(I.Name,
					GetImport(-I.OuterIndex - 1), I.Type);
		}
		return I.Obj;
	}
	void ResolveImports()
	{
		for (int i = 0; i < NumImports; i++)
			GetImport(i);
	}
};

//==========================================================================
//
//	VMemberBase::VMemberBase
//
//==========================================================================

VMemberBase::VMemberBase(vuint8 InMemberType, VName AName)
: MemberType(InMemberType)
, Outer(0)
, Name(AName)
{
	if (GObjInitialized)
	{
		GMembers.Append(this);
	}
}

//==========================================================================
//
//	VMemberBase::~VMemberBase
//
//==========================================================================

VMemberBase::~VMemberBase()
{
}

//==========================================================================
//
//	VMemberBase::GetFullName
//
//==========================================================================

VStr VMemberBase::GetFullName() const
{
	if (Outer)
		return Outer->GetFullName() + "." + Name;
	return VStr(Name);
}

//==========================================================================
//
//	VMemberBase::GetPackage
//
//==========================================================================

VPackage* VMemberBase::GetPackage() const
{
	guard(VMemberBase::GetPackage);
	for (const VMemberBase* p = this; p; p = p->Outer)
		if (p->MemberType == MEMBER_Package)
			return (VPackage*)p;
	Sys_Error("Member object %s not in a package", *GetFullName());
	return NULL;
	unguard;
}

//==========================================================================
//
//	VMemberBase::Serialise
//
//==========================================================================

void VMemberBase::Serialise(VStream& Strm)
{
	Strm << Outer;
}

//==========================================================================
//
//	VMemberBase::PostLoad
//
//==========================================================================

void VMemberBase::PostLoad()
{
}

//==========================================================================
//
//	VMemberBase::Shutdown
//
//==========================================================================

void VMemberBase::Shutdown()
{
}

//==========================================================================
//
//	VMemberBase::StaticInit
//
//==========================================================================

void VMemberBase::StaticInit()
{
	guard(VMemberBase::StaticInit);
	VClass::GModelNames.Append(NAME_None);
	for (VClass* C = GClasses; C; C = C->LinkNext)
		GMembers.Append(C);
	GObjInitialized = true;
	unguard;
}

//==========================================================================
//
//	VMemberBase::StaticExit
//
//==========================================================================

void VMemberBase::StaticExit()
{
	for (int i = 0; i < GMembers.Num(); i++)
	{
		if (GMembers[i]->MemberType != MEMBER_Class ||
			!(((VClass*)GMembers[i])->ObjectFlags & CLASSOF_Native))
		{
			delete GMembers[i];
		}
		else
		{
			GMembers[i]->Shutdown();
		}
	}
	GMembers.Clear();
	GLoadedPackages.Clear();
	VClass::GMobjInfos.Clear();
	VClass::GScriptIds.Clear();
	VClass::GSpriteNames.Clear();
	VClass::GModelNames.Clear();
	GObjInitialized = false;
}

//==========================================================================
//
//	VMemberBase::StaticLoadPackage
//
//==========================================================================

VPackage* VMemberBase::StaticLoadPackage(VName InName)
{
	guard(VMemberBase::StaticLoadPackage);
	int				i;
	VName*			NameRemap;
	dprograms_t		Progs;
	TCRC			crc;
	VProgsReader*	Reader;

	//	Check if already loaded.
	for (i = 0; i < GLoadedPackages.Num(); i++)
		if (GLoadedPackages[i]->Name == InName)
			return GLoadedPackages[i];

	if (fl_devmode && FL_FindFile(va("progs/%s.dat", *InName)))
	{
		//	Load PROGS from a specified file
		Reader = new VProgsReader(FL_OpenFileRead(va("progs/%s.dat", *InName)));
	}
	else
	{
		//	Load PROGS from wad file
		Reader = new VProgsReader(W_CreateLumpReaderName(InName, WADNS_Progs));
	}

	//	Calcutate CRC
	crc.Init();
	for (i = 0; i < Reader->TotalSize(); i++)
	{
		crc + Streamer<byte>(*Reader);
	}

	// byte swap the header
	Reader->Seek(0);
	Reader->Serialise(Progs.magic, 4);
	for (i = 1; i < (int)sizeof(Progs) / 4; i++)
	{
		*Reader << ((int*)&Progs)[i];
	}

	if (strncmp(Progs.magic, PROG_MAGIC, 4))
		Sys_Error("Progs has wrong file ID, possibly older version");
	if (Progs.version != PROG_VERSION)
		Sys_Error("Progs has wrong version number (%i should be %i)",
			Progs.version, PROG_VERSION);

	// Read names
	NameRemap = new VName[Progs.num_names];
	Reader->Seek(Progs.ofs_names);
	for (i = 0; i < Progs.num_names; i++)
	{
		VNameEntry E;
		*Reader << E;
		NameRemap[i] = E.Name;
	}
	Reader->NameRemap = NameRemap;

	Reader->Imports = new VProgsImport[Progs.num_imports];
	Reader->NumImports = Progs.num_imports;
	Reader->Seek(Progs.ofs_imports);
	for (i = 0; i < Progs.num_imports; i++)
	{
		*Reader << Reader->Imports[i];
	}
	Reader->ResolveImports();

	VProgsExport* Exports = new VProgsExport[Progs.num_exports];
	Reader->Exports = Exports;
	Reader->NumExports = Progs.num_exports;

	VPackage* Pkg = new VPackage(InName);
	GLoadedPackages.Append(Pkg);
	Pkg->Checksum = crc;
	Pkg->Reader = Reader;

	//	Create objects
	Reader->Seek(Progs.ofs_exportinfo);
	for (i = 0; i < Progs.num_exports; i++)
	{
		*Reader << Exports[i];
		switch (Exports[i].Type)
		{
		case MEMBER_Package:
			Exports[i].Obj = new VPackage(Exports[i].Name);
			break;
		case MEMBER_Field:
			Exports[i].Obj = new VField(Exports[i].Name);
			break;
		case MEMBER_Method:
			Exports[i].Obj = new VMethod(Exports[i].Name);
			break;
		case MEMBER_State:
			Exports[i].Obj = new VState(Exports[i].Name);
			break;
		case MEMBER_Const:
			Exports[i].Obj = new VConstant(Exports[i].Name);
			break;
		case MEMBER_Struct:
			Exports[i].Obj = new VStruct(Exports[i].Name);
			break;
		case MEMBER_Class:
			Exports[i].Obj = VClass::FindClass(*Exports[i].Name);
			if (!Exports[i].Obj)
			{
				Exports[i].Obj = new VClass(Exports[i].Name);
			}
			break;
		}
	}

	//	Read strings.
	Pkg->Strings = new char[Progs.num_strings];
	Reader->Seek(Progs.ofs_strings);
	Reader->Serialise(Pkg->Strings, Progs.num_strings);

	//	Serialise objects.
	Reader->Seek(Progs.ofs_exportdata);
	for (i = 0; i < Progs.num_exports; i++)
	{
		Exports[i].Obj->Serialise(*Reader);
		if (!Exports[i].Obj->Outer)
			Exports[i].Obj->Outer = Pkg;
	}

	//	Set up info tables.
	Reader->Seek(Progs.ofs_mobjinfo);
	for (i = 0; i < Progs.num_mobjinfo; i++)
	{
		*Reader << VClass::GMobjInfos.Alloc();
	}
	Reader->Seek(Progs.ofs_scriptids);
	for (i = 0; i < Progs.num_scriptids; i++)
	{
		*Reader << VClass::GScriptIds.Alloc();
	}

	for (i = 0; i < Progs.num_exports; i++)
	{
		Exports[i].Obj->PostLoad();
	}

	delete Reader;
	Pkg->Reader = NULL;
	return Pkg;
	unguard;
}

//==========================================================================
//
//	VMemberBase::StaticFindMember
//
//==========================================================================

VMemberBase* VMemberBase::StaticFindMember(VName InName,
	VMemberBase* InOuter, vuint8 InType)
{
	guard(VMemberBase::StaticFindMember);
	for (int i = 0; i < GMembers.Num(); i++)
		if (GMembers[i]->MemberType == InType &&
			GMembers[i]->Name == InName && GMembers[i]->Outer == InOuter)
			return GMembers[i];
	return NULL;
	unguard;
}

//==========================================================================
//
//	VPackage::VPackage
//
//==========================================================================

VPackage::VPackage(VName AName)
: VMemberBase(MEMBER_Package, AName)
, Checksum(0)
, Strings(0)
, Reader(0)
{
}

//==========================================================================
//
//	VPackage::~VPackage
//
//==========================================================================

VPackage::~VPackage()
{
	guard(VPackage::~VPackage);
	if (Strings)
		delete[] Strings;
	unguard;
}

//==========================================================================
//
//	VPackage::Serialise
//
//==========================================================================

void VPackage::Serialise(VStream& Strm)
{
	guard(VPackage::Serialise);
	VMemberBase::Serialise(Strm);
	unguard;
}

//==========================================================================
//
//	VField::VField
//
//==========================================================================

VField::VField(VName AName)
: VMemberBase(MEMBER_Field, AName)
, Next(0)
, NextReference(0)
, DestructorLink(0)
, Ofs(0)
, Func(0)
, Flags(0)
{
	memset(&Type, 0, sizeof(Type));
}

//==========================================================================
//
//	VField::Serialise
//
//==========================================================================

void VField::Serialise(VStream& Strm)
{
	guard(VField::Serialise);
	VMemberBase::Serialise(Strm);
	Strm << Next
		<< Type
		<< Func
		<< STRM_INDEX(Flags);
	unguard;
}

//==========================================================================
//
//	operator VStream << FType
//
//==========================================================================

VStream& operator<<(VStream& Strm, VField::FType& T)
{
	guard(operator VStream << FType);
	Strm << T.Type;
	byte RealType = T.Type;
	if (RealType == ev_array)
	{
		Strm << T.ArrayInnerType
			<< STRM_INDEX(T.ArrayDim);
		RealType = T.ArrayInnerType;
	}
	if (RealType == ev_pointer)
	{
		Strm << T.InnerType
			<< T.PtrLevel;
		RealType = T.InnerType;
	}
	if (RealType == ev_reference)
		Strm << T.Class;
	else if (RealType == ev_struct || RealType == ev_vector)
		Strm << T.Struct;
	else if (RealType == ev_delegate)
		Strm << T.Function;
	else if (RealType == ev_bool)
		Strm << T.BitMask;
	return Strm;
	unguard;
}

//==========================================================================
//
//	VField::SerialiseFieldValue
//
//==========================================================================

void VField::SerialiseFieldValue(VStream& Strm, byte* Data, const VField::FType& Type)
{
	guard(SerialiseFieldValue);
	VField::FType IntType;
	int InnerSize;
	switch (Type.Type)
	{
	case ev_int:
		Strm << *(int*)Data;
		break;

	case ev_float:
		Strm << *(float*)Data;
		break;

	case ev_bool:
		if (Type.BitMask == 1)
			Strm << *(int*)Data;
		break;

	case ev_vector:
		Strm << *(TVec*)Data;
		break;

	case ev_name:
		Strm << *(VName*)Data;
		break;

	case ev_string:
		Strm << *(VStr*)Data;
		break;

	case ev_pointer:
		if (Type.InnerType == ev_struct)
			Strm.SerialiseStructPointer(*(void**)Data, Type.Struct);
		else
		{
			dprintf("Don't know how to serialise pointer type %d\n", Type.InnerType);
			Strm << *(int*)Data;
		}
		break;

	case ev_reference:
		Strm.SerialiseReference(*(VObject**)Data, Type.Class);
		break;

	case ev_classid:
		if (Strm.IsLoading())
		{
			VName CName;
			Strm << CName;
			if (CName != NAME_None)
			{
				*(VClass**)Data = VClass::FindClass(*CName);
			}
			else
			{
				*(VClass**)Data = NULL;
			}
		}
		else
		{
			VName CName = NAME_None;
			if (*(VClass**)Data)
			{
				CName = (*(VClass**)Data)->GetVName();
			}
			Strm << CName;
		}
		break;

	case ev_state:
		if (Strm.IsLoading())
		{
			VName CName;
			VName SName;
			Strm << CName << SName;
			if (SName != NAME_None)
			{
				*(VState**)Data = VClass::FindClass(*CName)->FindStateChecked(SName);
			}
			else
			{
				*(VState**)Data = NULL;
			}
		}
		else
		{
			VName CName = NAME_None;
			VName SName = NAME_None;
			if (*(VState**)Data)
			{
				CName = (*(VState**)Data)->Outer->GetVName();
				SName = (*(VState**)Data)->Name;
			}
			Strm << CName << SName;
		}
		break;

	case ev_delegate:
		Strm.SerialiseReference(*(VObject**)Data, Type.Class);
		if (Strm.IsLoading())
		{
			VName FuncName;
			Strm << FuncName;
			if (*(VObject**)Data)
				((VMethod**)Data)[1] = (*(VObject**)Data)->GetVFunction(FuncName);
		}
		else
		{
			VName FuncName = NAME_None;
			if (*(VObject**)Data)
				FuncName = ((VMethod**)Data)[1]->Name;
			Strm << FuncName;
		}
		break;

	case ev_struct:
		Type.Struct->SerialiseObject(Strm, Data);
		break;

	case ev_array:
		IntType = Type;
		IntType.Type = Type.ArrayInnerType;
		InnerSize = IntType.Type == ev_vector ? 12 : IntType.Type == ev_struct ? IntType.Struct->Size : 4;
		for (int i = 0; i < Type.ArrayDim; i++)
		{
			SerialiseFieldValue(Strm, Data + i * InnerSize, IntType);
		}
		break;
	}
	unguard;
}

//==========================================================================
//
//	VField::CleanField
//
//==========================================================================

void VField::CleanField(byte* Data, const VField::FType& Type)
{
	guard(CleanField);
	VField::FType IntType;
	int InnerSize;
	switch (Type.Type)
	{
	case ev_reference:
		if (*(VObject**)Data && (*(VObject**)Data)->GetFlags() & _OF_CleanupRef)
		{
			*(VObject**)Data = NULL;
		}
		break;

	case ev_delegate:
		if (*(VObject**)Data && (*(VObject**)Data)->GetFlags() & _OF_CleanupRef)
		{
			*(VObject**)Data = NULL;
			((VMethod**)Data)[1] = NULL;
		}
		break;

	case ev_struct:
		Type.Struct->CleanObject(Data);
		break;

	case ev_array:
		IntType = Type;
		IntType.Type = Type.ArrayInnerType;
		InnerSize = IntType.GetSize();
		for (int i = 0; i < Type.ArrayDim; i++)
		{
			CleanField(Data + i * InnerSize, IntType);
		}
		break;
	}
	unguard;
}

//==========================================================================
//
//	VField::DestructField
//
//==========================================================================

void VField::DestructField(byte* Data, const VField::FType& Type)
{
	guard(DestructField);
	VField::FType IntType;
	int InnerSize;
	switch (Type.Type)
	{
	case ev_string:
		((VStr*)Data)->Clean();
		break;

	case ev_struct:
		Type.Struct->DestructObject(Data);
		break;

	case ev_array:
		IntType = Type;
		IntType.Type = Type.ArrayInnerType;
		InnerSize = IntType.GetSize();
		for (int i = 0; i < Type.ArrayDim; i++)
		{
			DestructField(Data + i * InnerSize, IntType);
		}
		break;
	}
	unguard;
}

//==========================================================================
//
//	VField::FType::GetSize
//
//==========================================================================

int VField::FType::GetSize() const
{
	guard(VField::FType::GetSize);
	switch (Type)
	{
	case ev_int:		return sizeof(vint32);
	case ev_float:		return sizeof(float);
	case ev_name:		return sizeof(VName);
	case ev_string:		return sizeof(VStr);
	case ev_pointer:	return sizeof(void*);
	case ev_reference:	return sizeof(VObject*);
	case ev_array:		return ArrayDim * GetArrayInnerType().GetSize();
	case ev_struct:		return (Struct->Size + 3) & ~3;
	case ev_vector:		return sizeof(TVec);
	case ev_classid:	return sizeof(VClass*);
	case ev_state:		return sizeof(VState*);
	case ev_bool:		return sizeof(vuint32);
	case ev_delegate:	return sizeof(VObject*) + sizeof(VMethod*);
	}
	return 0;
	unguard;
}

//==========================================================================
//
//	VField::FType::GetArrayInnerType
//
//==========================================================================

VField::FType VField::FType::GetArrayInnerType() const
{
	guard(VField::FType::GetArrayInnerType);
	if (Type != ev_array)
	{
		Sys_Error("Not an array type");
		return *this;
	}
	VField::FType ret = *this;
	ret.Type = ArrayInnerType;
	ret.ArrayInnerType = ev_void;
	ret.ArrayDim = 0;
	return ret;
	unguard;
}

//==========================================================================
//
//	VMethod::VMethod
//
//==========================================================================

VMethod::VMethod(VName AName)
: VMemberBase(MEMBER_Method, AName)
, NumParms(0)
, NumLocals(0)
, Type(0)
, Flags(0)
, Profile1(0)
, Profile2(0)
, NativeFunc(0)
, NumInstructions(0)
, Instructions(0)
, VTableIndex(0)
{
}

//==========================================================================
//
//  VMethod::~VMethod
//
//==========================================================================

VMethod::~VMethod()
{
	guard(VMethod::~VMethod);
	if (Instructions)
		delete[] Instructions;
	unguard;
}

//==========================================================================
//
//  PF_Fixme
//
//==========================================================================

static void PF_Fixme()
{
	Sys_Error("unimplemented bulitin");
}

//==========================================================================
//
//	VMethod::Serialise
//
//==========================================================================

void VMethod::Serialise(VStream& Strm)
{
	guard(VMethod::Serialise);
	VMemberBase::Serialise(Strm);
	VField::FType TmpType;
	vint32 TmpNumParams;
	vint32 TmpNumLocals;
	vint32 TmpFlags;
	vint32 ParamsSize;
	Strm << STRM_INDEX(TmpNumLocals)
		<< STRM_INDEX(TmpFlags)
		<< TmpType
		<< STRM_INDEX(TmpNumParams)
		<< STRM_INDEX(ParamsSize);
	Type = TmpType.Type;
	NumParms = ParamsSize;
	NumLocals = TmpNumLocals;
	Flags = TmpFlags;
	for (int i = 0; i < TmpNumParams; i++)
		Strm << TmpType;

	//	Set up builtins
	if (NumParms > 16)
		Sys_Error("Function has more than 16 params");
	for (FBuiltinInfo* B = FBuiltinInfo::Builtins; B; B = B->Next)
	{
		if (Outer == B->OuterClass && !strcmp(*Name, B->Name))
		{
			if (Flags & FUNC_Native)
			{
				NativeFunc = B->Func;
				break;
			}
			else
			{
				Sys_Error("PR_LoadProgs: Builtin %s redefined", B->Name);
			}
		}
	}
	if (!NativeFunc && Flags & FUNC_Native)
	{
		//	Default builtin
		NativeFunc = PF_Fixme;
#if defined CLIENT && defined SERVER
		//	Don't abort with error, because it will be done, when this
		// function will be called (if it will be called).
		GCon->Logf(NAME_Dev, "WARNING: Builtin %s not found!",
			*GetFullName());
#endif
	}

	//
	//	Read instructions
	//
	Strm << STRM_INDEX(NumInstructions);
	Instructions = new FInstruction[NumInstructions];
	for (int i = 0; i < NumInstructions; i++)
	{
		vuint8 Opc;
		Strm << Opc;
		Instructions[i].Opcode = Opc;
		switch (OpcodeInfo[Opc].Args)
		{
		case OPCARGS_None:
			break;
		case OPCARGS_Member:
		case OPCARGS_FieldOffset:
		case OPCARGS_VTableIndex:
			Strm << Instructions[i].Member;
			break;
		case OPCARGS_BranchTarget:
			Strm << Instructions[i].Arg1;
			break;
		case OPCARGS_ByteBranchTarget:
		case OPCARGS_ShortBranchTarget:
		case OPCARGS_IntBranchTarget:
			Strm << STRM_INDEX(Instructions[i].Arg1);
			Strm << Instructions[i].Arg2;
			break;
		case OPCARGS_Byte:
		case OPCARGS_Short:
		case OPCARGS_Int:
			Strm << STRM_INDEX(Instructions[i].Arg1);
			break;
		case OPCARGS_Name:
			Strm << Instructions[i].NameArg;
			break;
		case OPCARGS_String:
			Strm << Instructions[i].Arg1;
			break;
		case OPCARGS_TypeSize:
			Strm << Instructions[i].TypeArg;
			break;
		}
	}
	unguard;
}

//==========================================================================
//
//	VMethod::PostLoad
//
//==========================================================================

void VMethod::PostLoad()
{
	guard(VMethod::PostLoad);
	//FIXME It should be called only once so it's safe for now.
	//if (ObjectFlags & CLASSOF_PostLoaded)
	//{
	//	return;
	//}

	CompileCode();

	//ObjectFlags |= CLASSOF_PostLoaded;
	unguard;
}

//==========================================================================
//
//	VMethod::CompileCode
//
//==========================================================================

#define WriteUInt8(p)	Statements.Append(p)
#define WriteInt16(p)	Statements.SetNum(Statements.Num() + 2); \
	*(vint16*)&Statements[Statements.Num() - 2] = (p)
#define WriteInt32(p)	Statements.SetNum(Statements.Num() + 4); \
	*(vint32*)&Statements[Statements.Num() - 4] = (p)
#define WritePtr(p)		Statements.SetNum(Statements.Num() + sizeof(void*)); \
	*(void**)&Statements[Statements.Num() - sizeof(void*)] = (p)

void VMethod::CompileCode()
{
	guard(VMethod::CompileCode);
	Statements.Clear();
	if (!NumInstructions)
	{
		return;
	}

	OptimiseInstructions();

	for (int i = 0; i < NumInstructions - 1; i++)
	{
		Instructions[i].Address = Statements.Num();
		Statements.Append(Instructions[i].Opcode);
		switch (OpcodeInfo[Instructions[i].Opcode].Args)
		{
		case OPCARGS_None:
			break;
		case OPCARGS_Member:
			WritePtr(Instructions[i].Member);
			break;
		case OPCARGS_BranchTargetB:
			WriteUInt8(0);
			break;
		case OPCARGS_BranchTargetNB:
			WriteUInt8(0);
			break;
		case OPCARGS_BranchTargetS:
			WriteInt16(0);
			break;
		case OPCARGS_BranchTarget:
			WriteInt32(0);
			break;
		case OPCARGS_ByteBranchTarget:
			WriteUInt8(Instructions[i].Arg1);
			WriteInt16(0);
			break;
		case OPCARGS_ShortBranchTarget:
			WriteInt16(Instructions[i].Arg1);
			WriteInt16(0);
			break;
		case OPCARGS_IntBranchTarget:
			WriteInt32(Instructions[i].Arg1);
			WriteInt16(0);
			break;
		case OPCARGS_Byte:
			WriteUInt8(Instructions[i].Arg1);
			break;
		case OPCARGS_Short:
			WriteInt16(Instructions[i].Arg1);
			break;
		case OPCARGS_Int:
			WriteInt32(Instructions[i].Arg1);
			break;
		case OPCARGS_Name:
			WriteInt32(Instructions[i].NameArg.GetIndex());
			break;
		case OPCARGS_NameS:
			WriteInt16(Instructions[i].NameArg.GetIndex());
			break;
		case OPCARGS_NameB:
			WriteUInt8(Instructions[i].NameArg.GetIndex());
			break;
		case OPCARGS_String:
			WritePtr(GetPackage()->Strings + Instructions[i].Arg1);
			break;
		case OPCARGS_FieldOffset:
			//	Make sure struct / class field offsets have been calculated.
			Instructions[i].Member->Outer->PostLoad();
			WriteInt32(((VField*)Instructions[i].Member)->Ofs);
			break;
		case OPCARGS_FieldOffsetS:
			//	Make sure struct / class field offsets have been calculated.
			Instructions[i].Member->Outer->PostLoad();
			WriteInt16(((VField*)Instructions[i].Member)->Ofs);
			break;
		case OPCARGS_FieldOffsetB:
			//	Make sure struct / class field offsets have been calculated.
			Instructions[i].Member->Outer->PostLoad();
			WriteUInt8(((VField*)Instructions[i].Member)->Ofs);
			break;
		case OPCARGS_VTableIndex:
			//	Make sure class virtual table has been calculated.
			Instructions[i].Member->Outer->PostLoad();
			WriteInt16(((VMethod*)Instructions[i].Member)->VTableIndex);
			break;
		case OPCARGS_VTableIndexB:
			//	Make sure class virtual table has been calculated.
			Instructions[i].Member->Outer->PostLoad();
			WriteUInt8(((VMethod*)Instructions[i].Member)->VTableIndex);
			break;
		case OPCARGS_TypeSize:
			WriteInt32(Instructions[i].TypeArg.GetSize());
			break;
		case OPCARGS_TypeSizeS:
			WriteInt16(Instructions[i].TypeArg.GetSize());
			break;
		case OPCARGS_TypeSizeB:
			WriteUInt8(Instructions[i].TypeArg.GetSize());
			break;
		}
	}
	Instructions[NumInstructions - 1].Address = Statements.Num();

	for (int i = 0; i < NumInstructions - 1; i++)
	{
		switch (OpcodeInfo[Instructions[i].Opcode].Args)
		{
		case OPCARGS_BranchTargetB:
			Statements[Instructions[i].Address + 1] =
				Instructions[Instructions[i].Arg1].Address -
				Instructions[i].Address;
			break;
		case OPCARGS_BranchTargetNB:
			Statements[Instructions[i].Address + 1] =
				Instructions[i].Address -
				Instructions[Instructions[i].Arg1].Address;
			break;
		case OPCARGS_BranchTargetS:
			*(vint16*)&Statements[Instructions[i].Address + 1] =
				Instructions[Instructions[i].Arg1].Address -
				Instructions[i].Address;
			break;
		case OPCARGS_BranchTarget:
			*(vint32*)&Statements[Instructions[i].Address + 1] =
				Instructions[Instructions[i].Arg1].Address -
				Instructions[i].Address;
			break;
		case OPCARGS_ByteBranchTarget:
			*(vint16*)&Statements[Instructions[i].Address + 2] =
				Instructions[Instructions[i].Arg2].Address -
				Instructions[i].Address;
			break;
		case OPCARGS_ShortBranchTarget:
			*(vint16*)&Statements[Instructions[i].Address + 3] =
				Instructions[Instructions[i].Arg2].Address -
				Instructions[i].Address;
			break;
		case OPCARGS_IntBranchTarget:
			*(vint16*)&Statements[Instructions[i].Address + 5] =
				Instructions[Instructions[i].Arg2].Address -
				Instructions[i].Address;
			break;
		}
	}

	//	We don't need instructions anymore.
	delete[] Instructions;
	Instructions = NULL;
	unguard;
}

//==========================================================================
//
//	VMethod::OptimiseInstructions
//
//==========================================================================

void VMethod::OptimiseInstructions()
{
	guard(VMethod::OptimiseInstructions);
	int Addr = 0;
	for (int i = 0; i < NumInstructions - 1; i++)
	{
		switch (Instructions[i].Opcode)
		{
		case OPC_PushVFunc:
			//	Make sure class virtual table has been calculated.
			Instructions[i].Member->Outer->PostLoad();
			if (((VMethod*)Instructions[i].Member)->VTableIndex < 256)
			{
				Instructions[i].Opcode = OPC_PushVFuncB;
			}
			break;

		case OPC_Offset:
			//	Make sure struct / class field offsets have been calculated.
			Instructions[i].Member->Outer->PostLoad();
			if (((VField*)Instructions[i].Member)->Ofs < 256)
			{
				Instructions[i].Opcode = OPC_OffsetB;
			}
			else if (((VField*)Instructions[i].Member)->Ofs <= MAXSHORT)
			{
				Instructions[i].Opcode = OPC_OffsetS;
			}
			break;

		case OPC_ArrayElement:
			if (Instructions[i].TypeArg.GetSize() < 256)
			{
				Instructions[i].Opcode = OPC_ArrayElementB;
			}
			else if (Instructions[i].TypeArg.GetSize() < MAXSHORT)
			{
				Instructions[i].Opcode = OPC_ArrayElementS;
			}
			break;

		case OPC_PushName:
			if (Instructions[i].NameArg.GetIndex() < 256)
			{
				Instructions[i].Opcode = OPC_PushNameB;
			}
			else if (Instructions[i].NameArg.GetIndex() < MAXSHORT)
			{
				Instructions[i].Opcode = OPC_PushNameS;
			}
			break;
		}

		//	Calculate approximate addresses for jump instructions.
		Instructions[i].Address = Addr;
		switch (OpcodeInfo[Instructions[i].Opcode].Args)
		{
		case OPCARGS_None:
			Addr++;
			break;
		case OPCARGS_Member:
		case OPCARGS_String:
			Addr += 1 + sizeof(void*);
			break;
		case OPCARGS_BranchTargetB:
		case OPCARGS_BranchTargetNB:
		case OPCARGS_Byte:
		case OPCARGS_NameB:
		case OPCARGS_FieldOffsetB:
		case OPCARGS_VTableIndexB:
		case OPCARGS_TypeSizeB:
			Addr += 2;
			break;
		case OPCARGS_BranchTargetS:
		case OPCARGS_Short:
		case OPCARGS_NameS:
		case OPCARGS_FieldOffsetS:
		case OPCARGS_VTableIndex:
		case OPCARGS_TypeSizeS:
			Addr += 3;
			break;
		case OPCARGS_ByteBranchTarget:
			Addr += 4;
			break;
		case OPCARGS_BranchTarget:
		case OPCARGS_ShortBranchTarget:
		case OPCARGS_Int:
		case OPCARGS_Name:
		case OPCARGS_FieldOffset:
		case OPCARGS_TypeSize:
			Addr += 5;
			break;
		case OPCARGS_IntBranchTarget:
			Addr += 7;
			break;
		}
	}

	//	Now do jump instructions.
	vint32 Offs;
	for (int i = 0; i < NumInstructions - 1; i++)
	{
		switch (OpcodeInfo[Instructions[i].Opcode].Args)
		{
		case OPCARGS_BranchTarget:
			Offs = Instructions[Instructions[i].Arg1].Address -
				Instructions[i].Address;
			if (Offs >= 0 && Offs < 256)
			{
				Instructions[i].Opcode -= 3;
			}
			else if (Offs < 0 && Offs > -256)
			{
				Instructions[i].Opcode -= 2;
			}
			else if (Offs >= MINSHORT && Offs <= MAXSHORT)
			{
				Instructions[i].Opcode -= 1;
			}
			break;
		}
	}
	Instructions[NumInstructions - 1].Address = Addr;
	unguard;
}

//==========================================================================
//
//	VState::VState
//
//==========================================================================

VState::VState(VName AName)
: VMemberBase(MEMBER_State, AName)
, SpriteName(NAME_None)
, SpriteIndex(0)
, frame(0)
, ModelName(NAME_None)
, ModelIndex(0)
, model_frame(0)
, time(0)
, nextstate(0)
, function(0)
, Next(0)
{
}

//==========================================================================
//
//	VState::Serialise
//
//==========================================================================

void VState::Serialise(VStream& Strm)
{
	guard(VState::Serialise);
	VMemberBase::Serialise(Strm);
	Strm << SpriteName
		<< STRM_INDEX(frame)
		<< ModelName
		<< STRM_INDEX(model_frame)
		<< time
		<< nextstate
		<< function
		<< Next;
	if (Strm.IsLoading())
	{
		SpriteIndex = VClass::FindSprite(SpriteName);
		ModelIndex = VClass::FindModel(ModelName);
	}
	unguard;
}

//==========================================================================
//
//	VState::IsInRange
//
//==========================================================================

bool VState::IsInRange(VState* Start, VState* End, int MaxDepth)
{
	guard(VState::IsInRange);
	int Depth = 0;
	VState* check = Start;
	do
	{
		if (check == this)
			return true;
		if (check)
			check = check->Next;
		Depth++;
	}
	while (Depth < MaxDepth && check != End);
	return false;
	unguard;
}

//==========================================================================
//
//	VConstant::VConstant
//
//==========================================================================

VConstant::VConstant(VName AName)
: VMemberBase(MEMBER_Const, AName)
, Type(0)
, Value(0)
{
}

//==========================================================================
//
//	VConstant::Serialise
//
//==========================================================================

void VConstant::Serialise(VStream& Strm)
{
	VMemberBase::Serialise(Strm);
	Strm << Type;
	switch (Type)
	{
	case ev_float:
		Strm << *(float*)&Value;
		break;

	case ev_name:
		Strm << *(VName*)&Value;
		break;

	default:
		Strm << STRM_INDEX(Value);
		break;
	}
}

//==========================================================================
//
//	VStruct::VStruct
//
//==========================================================================

VStruct::VStruct(VName AName)
: VMemberBase(MEMBER_Struct, AName)
, ObjectFlags(0)
, ParentStruct(0)
, Size(0)
, Fields(0)
, ReferenceFields(0)
, DestructorFields(0)
{
}

//==========================================================================
//
//	VStruct::Serialise
//
//==========================================================================

void VStruct::Serialise(VStream& Strm)
{
	guard(VStruct::Serialise);
	vint32 StackSize;
	VMemberBase::Serialise(Strm);
	Strm << ParentStruct
		<< IsVector
		<< STRM_INDEX(StackSize)
		<< Fields;
	unguard;
}

//==========================================================================
//
//	VStruct::PostLoad
//
//==========================================================================

void VStruct::PostLoad()
{
	if (ObjectFlags & CLASSOF_PostLoaded)
	{
		//	Already done.
		return;
	}

	//	Make sure parent struct has been set up.
	if (ParentStruct)
	{
		ParentStruct->PostLoad();
	}

	//	Calculate field offsets and class size.
	CalcFieldOffsets();

	//	Set up list of reference fields.
	InitReferences();

	//	Set up list of destructor fields.
	InitDestructorFields();

	ObjectFlags |= CLASSOF_PostLoaded;
}

//==========================================================================
//
//	VStruct::CalcFieldOffsets
//
//==========================================================================

void VStruct::CalcFieldOffsets()
{
	guard(VStruct::CalcFieldOffsets);
	int size = ParentStruct ? ParentStruct->Size : 0;
	VField* PrevField = NULL;
	for (VField* fi = Fields; fi; fi = fi->Next)
	{
		if (fi->Type.Type == ev_bool && PrevField &&
			PrevField->Type.Type == ev_bool &&
			PrevField->Type.BitMask != 0x80000000)
		{
			vuint32 bit_mask = PrevField->Type.BitMask << 1;
			if (fi->Type.BitMask != bit_mask)
				Sys_Error("Wrong bit mask");
			fi->Type.BitMask = bit_mask;
			fi->Ofs = PrevField->Ofs;
		}
		else
		{
			fi->Ofs = size;
			if (fi->Type.Type == ev_struct ||
				(fi->Type.Type == ev_array && fi->Type.ArrayInnerType == ev_struct))
			{
				//	Make sure struct size has been calculated.
				fi->Type.Struct->PostLoad();
			}
			size += fi->Type.GetSize();
		}
		PrevField = fi;
	}
	Size = size;
	unguard;
}

//==========================================================================
//
//	VStruct::InitReferences
//
//==========================================================================

void VStruct::InitReferences()
{
	guard(VStruct::InitReferences);
	ReferenceFields = NULL;
	if (ParentStruct)
	{
		ReferenceFields = ParentStruct->ReferenceFields;
	}

	for (VField* F = Fields; F; F = F->Next)
	{
		switch (F->Type.Type)
		{
		case ev_reference:
		case ev_delegate:
			F->NextReference = ReferenceFields;
			ReferenceFields = F;
			break;
		
		case ev_struct:
			F->Type.Struct->PostLoad();
			if (F->Type.Struct->ReferenceFields)
			{
				F->NextReference = ReferenceFields;
				ReferenceFields = F;
			}
			break;
		
		case ev_array:
			if (F->Type.ArrayInnerType == ev_reference)
			{
				F->NextReference = ReferenceFields;
				ReferenceFields = F;
			}
			else if (F->Type.ArrayInnerType == ev_struct)
			{
				F->Type.Struct->PostLoad();
				if (F->Type.Struct->ReferenceFields)
				{
					F->NextReference = ReferenceFields;
					ReferenceFields = F;
				}
			}
			break;
		}
	}
	unguard;
}

//==========================================================================
//
//	VStruct::InitDestructorFields
//
//==========================================================================

void VStruct::InitDestructorFields()
{
	guard(VStruct::InitDestructorFields);
	DestructorFields = NULL;
	if (ParentStruct)
	{
		DestructorFields = ParentStruct->DestructorFields;
	}

	for (VField* F = Fields; F; F = F->Next)
	{
		switch (F->Type.Type)
		{
		case ev_string:
			F->DestructorLink = DestructorFields;
			DestructorFields = F;
			break;
		
		case ev_struct:
			F->Type.Struct->PostLoad();
			if (F->Type.Struct->DestructorFields)
			{
				F->DestructorLink = DestructorFields;
				DestructorFields = F;
			}
			break;
		
		case ev_array:
			if (F->Type.ArrayInnerType == ev_string)
			{
				F->DestructorLink = DestructorFields;
				DestructorFields = F;
			}
			else if (F->Type.ArrayInnerType == ev_struct)
			{
				F->Type.Struct->PostLoad();
				if (F->Type.Struct->DestructorFields)
				{
					F->DestructorLink = DestructorFields;
					DestructorFields = F;
				}
			}
			break;
		}
	}
	unguard;
}

//==========================================================================
//
//	VStruct::SerialiseObject
//
//==========================================================================

void VStruct::SerialiseObject(VStream& Strm, byte* Data)
{
	guard(VStruct::SerialiseObject);
	//	Serialise parent struct's fields.
	if (ParentStruct)
	{
		ParentStruct->SerialiseObject(Strm, Data);
	}
	//	Serialise fields.
	for (VField* F = Fields; F; F = F->Next)
	{
		//	Skip native and transient fields.
		if (F->Flags & (FIELD_Native | FIELD_Transient))
		{
			continue;
		}
		VField::SerialiseFieldValue(Strm, Data + F->Ofs, F->Type);
	}
	unguardf(("(%s)", *Name));
}

//==========================================================================
//
//	VStruct::CleanObject
//
//==========================================================================

void VStruct::CleanObject(byte* Data)
{
	guard(VStruct::CleanObject);
	for (VField* F = ReferenceFields; F; F = F->NextReference)
	{
		VField::CleanField(Data + F->Ofs, F->Type);
	}
	unguardf(("(%s)", *Name));
}

//==========================================================================
//
//	VStruct::DestructObject
//
//==========================================================================

void VStruct::DestructObject(byte* Data)
{
	guard(VStruct::DestructObject);
	for (VField* F = DestructorFields; F; F = F->DestructorLink)
	{
		VField::DestructField(Data + F->Ofs, F->Type);
	}
	unguardf(("(%s)", *Name));
}

//==========================================================================
//
//	operator VStream << mobjinfo_t
//
//==========================================================================

VStream& operator<<(VStream& Strm, mobjinfo_t& MI)
{
	return Strm << STRM_INDEX(MI.doomednum)
		<< MI.class_id;
}

//==========================================================================
//
//	VClass::VClass
//
//==========================================================================

VClass::VClass(VName AName)
: VMemberBase(MEMBER_Class, AName)
, ObjectFlags(0)
, LinkNext(0)
, ParentClass(0)
, ClassSize(0)
, ClassFlags(0)
, ClassVTable(0)
, ClassConstructor(0)
, ClassNumMethods(0)
, Fields(0)
, ReferenceFields(0)
, States(0)
, DefaultProperties(0)
{
	guard(VClass::VClass);
	LinkNext = GClasses;
	GClasses = this;
	unguard;
}

//==========================================================================
//
//	VClass::VClass
//
//==========================================================================

VClass::VClass(ENativeConstructor, size_t ASize, dword AClassFlags, 
	VClass *AParent, EName AName, void(*ACtor)())
: VMemberBase(MEMBER_Class, AName)
, ObjectFlags(CLASSOF_Native)
, LinkNext(0)
, ParentClass(AParent)
, ClassSize(ASize)
, ClassFlags(AClassFlags)
, ClassVTable(0)
, ClassConstructor(ACtor)
, ClassNumMethods(0)
, Fields(0)
, ReferenceFields(0)
, States(0)
, DefaultProperties(0)
{
	guard(native VClass::VClass);
	LinkNext = GClasses;
	GClasses = this;
	unguard;
}

//==========================================================================
//
//	VClass::~VClass
//
//==========================================================================

VClass::~VClass()
{
	guard(VClass::~VClass);
	if (ClassVTable)
	{
		delete[] ClassVTable;
	}

	if (!GObjInitialized)
	{
		return;
	}
	//	Unlink from classes list.
	if (GClasses == this)
	{
		GClasses = LinkNext;
	}
	else
	{
		VClass* Prev = GClasses;
		while (Prev && Prev->LinkNext != this)
		{
			Prev = Prev->LinkNext;
		}
		if (Prev)
		{
			Prev->LinkNext = LinkNext;
		}
		else
		{
			GCon->Log(NAME_Dev, "VClass Unlink: Class not in list");
		}
	}
	unguard;
}

//==========================================================================
//
//	VClass::Shutdown
//
//==========================================================================

void VClass::Shutdown()
{
	guard(VClass::Shutdown);
	if (ClassVTable)
	{
		delete[] ClassVTable;
	}
	unguard;
}

//==========================================================================
//
//	VClass::Serialise
//
//==========================================================================

void VClass::Serialise(VStream& Strm)
{
	guard(VClass::Serialise);
	VMemberBase::Serialise(Strm);
	VClass* PrevParent = ParentClass;
	Strm << ParentClass
		<< Fields
		<< States
		<< DefaultProperties;
	if ((ObjectFlags & CLASSOF_Native) && ParentClass != PrevParent)
	{
		Sys_Error("Bad parent class, class %s, C++ %s, VavoomC %s)",
			GetName(), PrevParent ? PrevParent->GetName() : "(none)",
			ParentClass ? ParentClass->GetName() : "(none)");
	}
	unguard;
}

//==========================================================================
//
//	VClass::FindClass
//
//==========================================================================

VClass *VClass::FindClass(const char *AName)
{
	VName TempName(AName, VName::Find);
	if (TempName == NAME_None)
	{
		// No such name, no chance to find a class
		return NULL;
	}
	for (VClass* Cls = GClasses; Cls; Cls = Cls->LinkNext)
	{
		if (Cls->GetVName() == TempName)
		{
			return Cls;
		}
	}
	return NULL;
}

//==========================================================================
//
//	VClass::FindSprite
//
//==========================================================================

int VClass::FindSprite(VName Name)
{
	guard(VClass::FindSprite);
	for (int i = 0; i < GSpriteNames.Num(); i++)
		if (GSpriteNames[i] == Name)
			return i;
	return GSpriteNames.Append(Name);
	unguard;
}

//==========================================================================
//
//	VClass::FindModel
//
//==========================================================================

int VClass::FindModel(VName Name)
{
	guard(VClass::FindModel);
	for (int i = 0; i < GModelNames.Num(); i++)
		if (GModelNames[i] == Name)
			return i;
	return GModelNames.Append(Name);
	unguard;
}

//==========================================================================
//
//	VClass::FindFunction
//
//==========================================================================

VMethod *VClass::FindFunction(VName AName)
{
	guard(VClass::FindFunction);
	for (int i = 0; i < ClassNumMethods; i++)
	{
		if (ClassVTable[i]->Name == AName)
		{
			return ClassVTable[i];
		}
	}
	return NULL;
	unguard;
}

//==========================================================================
//
//	VClass::FindFunctionChecked
//
//==========================================================================

VMethod *VClass::FindFunctionChecked(VName AName)
{
	guard(VClass::FindFunctionChecked);
	VMethod *func = FindFunction(AName);
	if (!func)
	{
		Sys_Error("Function %s not found", *AName);
	}
	return func;
	unguard;
}

//==========================================================================
//
//	VClass::GetFunctionIndex
//
//==========================================================================

int VClass::GetFunctionIndex(VName AName)
{
	guard(VClass::GetFunctionIndex);
	for (int i = 0; i < ClassNumMethods; i++)
	{
		if (ClassVTable[i]->Name == AName)
		{
			return i;
		}
	}
	return -1;
	unguard;
}

//==========================================================================
//
//	VClass::FindState
//
//==========================================================================

VState* VClass::FindState(VName AName)
{
	guard(VClass::FindState);
	for (VState* s = States; s; s = s->Next)
	{
		if (s->Name == AName)
		{
			return s;
		}
	}
	if (ParentClass)
	{
		return ParentClass->FindState(AName);
	}
	return NULL;
	unguard;
}

//==========================================================================
//
//	VClass::FindStateChecked
//
//==========================================================================

VState* VClass::FindStateChecked(VName AName)
{
	guard(VClass::FindStateChecked);
	VState* s = FindState(AName);
	if (!s)
	{
		Sys_Error("State %s not found", *AName);
	}
	return s;
	unguard;
}

//==========================================================================
//
//	VClass::PostLoad
//
//==========================================================================

void VClass::PostLoad()
{
	if (ObjectFlags & CLASSOF_PostLoaded)
	{
		//	Already set up.
		return;
	}

	//	Make sure parent class has been set up.
	if (GetSuperClass())
	{
		GetSuperClass()->PostLoad();
	}

	//	Calculate field offsets and class size.
	CalcFieldOffsets();

	//	Initialise reference fields.
	InitReferences();

	//	Initialise destructor fields.
	InitDestructorFields();

	//	Create virtual table.
	CreateVTable();

	ObjectFlags |= CLASSOF_PostLoaded;
}

//==========================================================================
//
//	VClass::CalcFieldOffsets
//
//==========================================================================

void VClass::CalcFieldOffsets()
{
	guard(VClass::CalcFieldOffsets);
	int PrevSize = ClassSize;
	int size = ParentClass ? ParentClass->ClassSize : 0;
	int numMethods = ParentClass ? ParentClass->ClassNumMethods : 0;
	VField* PrevField = NULL;
	for (int i = 0; i < GMembers.Num(); i++)
	{
		if (GMembers[i]->MemberType != MEMBER_Method ||
			GMembers[i]->Outer != this)
		{
			continue;
		}
		VMethod* M = (VMethod*)GMembers[i];
		if (M == DefaultProperties)
		{
			M->VTableIndex = -1;
			continue;
		}
		int MOfs = -1;
		if (ParentClass)
		{
			MOfs = ParentClass->GetFunctionIndex(M->Name);
		}
		if (MOfs == -1 && !(M->Flags & FUNC_Final))
		{
			MOfs = numMethods++;
		}
		M->VTableIndex = MOfs;
	}
	for (VField* fi = Fields; fi; fi = fi->Next)
	{
		if (fi->Type.Type == ev_bool && PrevField &&
			PrevField->Type.Type == ev_bool &&
			PrevField->Type.BitMask != 0x80000000)
		{
			vuint32 bit_mask = PrevField->Type.BitMask << 1;
			if (fi->Type.BitMask != bit_mask)
				Sys_Error("Wrong bit mask");
			fi->Type.BitMask = bit_mask;
			fi->Ofs = PrevField->Ofs;
		}
		else
		{
			fi->Ofs = size;
			if (fi->Type.Type == ev_struct ||
				(fi->Type.Type == ev_array && fi->Type.ArrayInnerType == ev_struct))
			{
				//	Make sure struct size has been calculated.
				fi->Type.Struct->PostLoad();
			}
			size += fi->Type.GetSize();
		}
		PrevField = fi;
	}
	ClassSize = size;
	ClassNumMethods = numMethods;
	if ((ObjectFlags & CLASSOF_Native) && ClassSize != PrevSize)
	{
		Sys_Error("Bad class size, class %s, C++ %d, VavoomC %d)",
			GetName(), PrevSize, ClassSize);
	}
	unguard;
}

//==========================================================================
//
//	VClass::InitReferences
//
//==========================================================================

void VClass::InitReferences()
{
	guard(VClass::InitReferences);
	ReferenceFields = NULL;
	if (GetSuperClass())
	{
		ReferenceFields = GetSuperClass()->ReferenceFields;
	}

	for (VField* F = Fields; F; F = F->Next)
	{
		switch (F->Type.Type)
		{
		case ev_reference:
		case ev_delegate:
			F->NextReference = ReferenceFields;
			ReferenceFields = F;
			break;
		
		case ev_struct:
			F->Type.Struct->PostLoad();
			if (F->Type.Struct->ReferenceFields)
			{
				F->NextReference = ReferenceFields;
				ReferenceFields = F;
			}
			break;
		
		case ev_array:
			if (F->Type.ArrayInnerType == ev_reference)
			{
				F->NextReference = ReferenceFields;
				ReferenceFields = F;
			}
			else if (F->Type.ArrayInnerType == ev_struct)
			{
				F->Type.Struct->PostLoad();
				if (F->Type.Struct->ReferenceFields)
				{
					F->NextReference = ReferenceFields;
					ReferenceFields = F;
				}
			}
			break;
		}
	}
	unguard;
}

//==========================================================================
//
//	VClass::InitDestructorFields
//
//==========================================================================

void VClass::InitDestructorFields()
{
	guard(VClass::InitDestructorFields);
	DestructorFields = NULL;
	if (GetSuperClass())
	{
		DestructorFields = GetSuperClass()->DestructorFields;
	}

	for (VField* F = Fields; F; F = F->Next)
	{
		switch (F->Type.Type)
		{
		case ev_string:
			F->DestructorLink = DestructorFields;
			DestructorFields = F;
			break;
		
		case ev_struct:
			F->Type.Struct->PostLoad();
			if (F->Type.Struct->DestructorFields)
			{
				F->DestructorLink = DestructorFields;
				DestructorFields = F;
			}
			break;
		
		case ev_array:
			if (F->Type.ArrayInnerType == ev_string)
			{
				F->DestructorLink = DestructorFields;
				DestructorFields = F;
			}
			else if (F->Type.ArrayInnerType == ev_struct)
			{
				F->Type.Struct->PostLoad();
				if (F->Type.Struct->DestructorFields)
				{
					F->DestructorLink = DestructorFields;
					DestructorFields = F;
				}
			}
			break;
		}
	}
	unguard;
}

//==========================================================================
//
//	VClass::CreateVTable
//
//==========================================================================

void VClass::CreateVTable()
{
	guard(VClass::CreateVTable);
	ClassVTable = new VMethod*[ClassNumMethods];
	memset(ClassVTable, 0, ClassNumMethods * sizeof(VMethod*));
	if (ParentClass)
	{
		memcpy(ClassVTable, ParentClass->ClassVTable,
			ParentClass->ClassNumMethods * sizeof(VMethod*));
	}
	for (int i = 0; i < GMembers.Num(); i++)
	{
		if (GMembers[i]->MemberType != MEMBER_Method ||
			GMembers[i]->Outer != this)
		{
			continue;
		}
		VMethod* M = (VMethod*)GMembers[i];
		if (M->VTableIndex == -1)
		{
			continue;
		}
		ClassVTable[M->VTableIndex] = M;
	}
	unguard;
}

//==========================================================================
//
//	VClass::SerialiseObject
//
//==========================================================================

void VClass::SerialiseObject(VStream& Strm, VObject* Obj)
{
	guard(SerialiseObject);
	//	Serialise parent class fields.
	if (GetSuperClass())
	{
		GetSuperClass()->SerialiseObject(Strm, Obj);
	}
	//	Serialise fields.
	for (VField* F = Fields; F; F = F->Next)
	{
		//	Skip native and transient fields.
		if (F->Flags & (FIELD_Native | FIELD_Transient))
		{
			continue;
		}
		VField::SerialiseFieldValue(Strm, (byte*)Obj + F->Ofs, F->Type);
	}
	unguardf(("(%s)", GetName()));
}

//==========================================================================
//
//	VClass::CleanObject
//
//==========================================================================

void VClass::CleanObject(VObject* Obj)
{
	guard(VClass::CleanObject);
	for (VField* F = ReferenceFields; F; F = F->NextReference)
	{
		VField::CleanField((byte*)Obj + F->Ofs, F->Type);
	}
	unguardf(("(%s)", GetName()));
}

//==========================================================================
//
//	VClass::DestructObject
//
//==========================================================================

void VClass::DestructObject(VObject* Obj)
{
	guard(VClass::DestructObject);
	for (VField* F = DestructorFields; F; F = F->DestructorLink)
	{
		VField::DestructField((byte*)Obj + F->Ofs, F->Type);
	}
	unguardf(("(%s)", GetName()));
}
