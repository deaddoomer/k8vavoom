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
//**	Copyright (C) 1999-2002 J�nis Legzdi��
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

#ifndef OPCODE_INFO

#define PROG_MAGIC		"VPRG"
#define PROG_VERSION	19

#define	MAX_PARAMS		16

enum EType
{
	ev_void,
	ev_int,
	ev_float,
	ev_name,
	ev_string,
	ev_pointer,
	ev_reference,
	ev_array,
	ev_struct,
	ev_vector,
	ev_classid,
	ev_bool,
	ev_delegate,
	ev_state,
	ev_unknown,

	NUM_BASIC_TYPES
};

enum
{
	FIELD_Native	= 0x0001,	//	Native serialisation
	FIELD_Transient	= 0x0002,	//	Not to be saved
	FIELD_Private	= 0x0004,	//	Private field
	FIELD_ReadOnly	= 0x0008,	//	Read-only field
};

enum
{
	FUNC_Native		= 0x0001,	// Native method
	FUNC_Static		= 0x0002,	// Static method
	FUNC_VarArgs	= 0x0004,	// Variable argument count
	FUNC_Final		= 0x0008,	// Final version of a method
};

enum
{
	MEMBER_Package,
	MEMBER_Field,
	MEMBER_Method,
	MEMBER_State,
	MEMBER_Const,
	MEMBER_Struct,
	MEMBER_Class,
};

enum
{
	OPCARGS_None,
	OPCARGS_Member,
	OPCARGS_BranchTarget,
	OPCARGS_IntBranchTarget,
	OPCARGS_Int,
	OPCARGS_Name,
	OPCARGS_String,
	OPCARGS_FieldOffset,
	OPCARGS_VTableIndex,
	OPCARGS_TypeSize,
};

enum
{
#define DECLARE_OPC(name, args)		OPC_##name
#endif

	DECLARE_OPC(Done, None),

	//	Call / return
	DECLARE_OPC(Call, Member),
	DECLARE_OPC(PushVFunc, VTableIndex),
	DECLARE_OPC(ICall, None),
	DECLARE_OPC(Return, None),
	DECLARE_OPC(ReturnL, None),
	DECLARE_OPC(ReturnV, None),

	//	Branching
	DECLARE_OPC(Goto, BranchTarget),
	DECLARE_OPC(IfGoto, BranchTarget),
	DECLARE_OPC(IfNotGoto, BranchTarget),
	DECLARE_OPC(IfTopGoto, BranchTarget),
	DECLARE_OPC(IfNotTopGoto, BranchTarget),
	DECLARE_OPC(CaseGoto, IntBranchTarget),

	//	Push constants.
	DECLARE_OPC(PushNumber, Int),
	DECLARE_OPC(PushName, Name),
	DECLARE_OPC(PushString, String),
	DECLARE_OPC(PushClassId, Member),
	DECLARE_OPC(PushState, Member),
	DECLARE_OPC(PushNull, None),

	//	Loading of variables
	DECLARE_OPC(LocalAddress, Int),
	DECLARE_OPC(Offset, FieldOffset),
	DECLARE_OPC(ArrayElement, TypeSize),
	DECLARE_OPC(PushPointed, None),
	DECLARE_OPC(VPushPointed, None),
	DECLARE_OPC(PushPointedPtr, None),
	DECLARE_OPC(PushBool, Int),
	DECLARE_OPC(PushPointedDelegate, None),

	//	Integer opeartors
	DECLARE_OPC(Add, None),
	DECLARE_OPC(Subtract, None),
	DECLARE_OPC(Multiply, None),
	DECLARE_OPC(Divide, None),
	DECLARE_OPC(Modulus, None),
	DECLARE_OPC(Equals, None),
	DECLARE_OPC(NotEquals, None),
	DECLARE_OPC(Less, None),
	DECLARE_OPC(Greater, None),
	DECLARE_OPC(LessEquals, None),
	DECLARE_OPC(GreaterEquals, None),
	DECLARE_OPC(AndLogical, None),
	DECLARE_OPC(OrLogical, None),
	DECLARE_OPC(NegateLogical, None),
	DECLARE_OPC(AndBitwise, None),
	DECLARE_OPC(OrBitwise, None),
	DECLARE_OPC(XOrBitwise, None),
	DECLARE_OPC(LShift, None),
	DECLARE_OPC(RShift, None),
	DECLARE_OPC(UnaryMinus, None),
	DECLARE_OPC(BitInverse, None),

	//	Increment / decrement
	DECLARE_OPC(PreInc, None),
	DECLARE_OPC(PreDec, None),
	DECLARE_OPC(PostInc, None),
	DECLARE_OPC(PostDec, None),
	DECLARE_OPC(IncDrop, None),
	DECLARE_OPC(DecDrop, None),

	//	Integer assignment operators
	DECLARE_OPC(AssignDrop, None),
	DECLARE_OPC(AddVarDrop, None),
	DECLARE_OPC(SubVarDrop, None),
	DECLARE_OPC(MulVarDrop, None),
	DECLARE_OPC(DivVarDrop, None),
	DECLARE_OPC(ModVarDrop, None),
	DECLARE_OPC(AndVarDrop, None),
	DECLARE_OPC(OrVarDrop, None),
	DECLARE_OPC(XOrVarDrop, None),
	DECLARE_OPC(LShiftVarDrop, None),
	DECLARE_OPC(RShiftVarDrop, None),

	//	Floating point operators
	DECLARE_OPC(FAdd, None),
	DECLARE_OPC(FSubtract, None),
	DECLARE_OPC(FMultiply, None),
	DECLARE_OPC(FDivide, None),
	DECLARE_OPC(FEquals, None),
	DECLARE_OPC(FNotEquals, None),
	DECLARE_OPC(FLess, None),
	DECLARE_OPC(FGreater, None),
	DECLARE_OPC(FLessEquals, None),
	DECLARE_OPC(FGreaterEquals, None),
	DECLARE_OPC(FUnaryMinus, None),

	//	Floating point assignment operators
	DECLARE_OPC(FAddVarDrop, None),
	DECLARE_OPC(FSubVarDrop, None),
	DECLARE_OPC(FMulVarDrop, None),
	DECLARE_OPC(FDivVarDrop, None),

	//	Vector operators
	DECLARE_OPC(VAdd, None),
	DECLARE_OPC(VSubtract, None),
	DECLARE_OPC(VPreScale, None),
	DECLARE_OPC(VPostScale, None),
	DECLARE_OPC(VIScale, None),
	DECLARE_OPC(VEquals, None),
	DECLARE_OPC(VNotEquals, None),
	DECLARE_OPC(VUnaryMinus, None),
	DECLARE_OPC(VFixParam, Int),

	//	Vector assignment operators
	DECLARE_OPC(VAssignDrop, None),
	DECLARE_OPC(VAddVarDrop, None),
	DECLARE_OPC(VSubVarDrop, None),
	DECLARE_OPC(VScaleVarDrop, None),
	DECLARE_OPC(VIScaleVarDrop, None),

	//	Pointer opeartors
	DECLARE_OPC(PtrEquals, None),
	DECLARE_OPC(PtrNotEquals, None),
	DECLARE_OPC(PtrToBool, None),

	//	Drop result
	DECLARE_OPC(Drop, None),
	DECLARE_OPC(VDrop, None),

	//	Swap stack elements.
	DECLARE_OPC(Swap, None),
	DECLARE_OPC(Swap3, None),

	//	Special assignment operators
	DECLARE_OPC(AssignPtrDrop, None),
	DECLARE_OPC(AssignBool, Int),
	DECLARE_OPC(AssignDelegate, None),

	//	Dynamic cast
	DECLARE_OPC(DynamicCast, Member),

#undef DECLARE_OPC
#ifndef OPCODE_INFO
	NUM_OPCODES
};

struct dprograms_t
{
	char	magic[4];		//	"VPRG"
	int		version;

	int		ofs_names;
	int		num_names;

	int		num_strings;
	int		ofs_strings;

	int		ofs_mobjinfo;
	int		num_mobjinfo;

	int		ofs_scriptids;
	int		num_scriptids;

	int		ofs_exportinfo;
	int		ofs_exportdata;
	int		num_exports;

	int		ofs_imports;
	int		num_imports;
};

#endif
