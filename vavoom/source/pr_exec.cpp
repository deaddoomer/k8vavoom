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
//**
//**    Execution of PROGS.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#define DO_GUARD_SLOW	1
#include "gamedefs.h"
#include "progdefs.h"

// MACROS ------------------------------------------------------------------

#define MAX_PROG_STACK	10000
#define STACK_ID		0x45f6cd4b

# define CHECK_STACK_OVERFLOW
//# define CHECK_PARM_COUNT
//	Not needed, if PROGS are correct
# define CHECK_STACK_UNDERFLOW
//# define CHECK_FUNC_NUM
//# define CHECK_VALID_OPCODE
# define CHECK_FOR_EMPTY_STACK
//# define CHECK_VALID_VAR_NUM

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern "C" void TestCaller();

// PUBLIC DATA DEFINITIONS -------------------------------------------------

FBuiltinInfo *FBuiltinInfo::Builtins;

extern "C" { 
int				*pr_stackPtr; 
VMethod			*current_func = NULL;
}

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int				pr_stack[MAX_PROG_STACK];

// CODE --------------------------------------------------------------------

//==========================================================================
//
//	PR_Init
//
//==========================================================================

void PR_Init()
{
	//	Set stack ID for overflow / underflow checks
	pr_stack[0] = STACK_ID;
	pr_stack[MAX_PROG_STACK - 1] = STACK_ID;
	pr_stackPtr = pr_stack + 1;
}

//==========================================================================
//
//	PR_OnAbort
//
//==========================================================================

void PR_OnAbort()
{
	current_func = NULL;
	pr_stackPtr = pr_stack + 1;
}

//==========================================================================
//
//	PR_Profile1
//
//==========================================================================

extern "C" void PR_Profile1()
{
	current_func->Profile1++;
}

//==========================================================================
//
//	PR_Profile2
//
//==========================================================================

extern "C" void PR_Profile2()
{
	if (current_func && (!(current_func->Flags & FUNC_Native)))
	{
		current_func->Profile2++;
	}
}
extern "C" void PR_Profile2_end(void){}

//==========================================================================
//
//	PR_Traceback
//
//==========================================================================

void PR_Traceback()
{
	if (current_func)
	{
		Host_CoreDump("(%s)", *current_func->GetFullName());
	}
}

//==========================================================================
//
//	TProgs::Load
//
//==========================================================================

void TProgs::Load(const char *AName)
{
	guard(TProgs::Load);
	Pkg = VMemberBase::StaticLoadPackage(VName(AName, VName::AddLower8));
	unguard;
}

//==========================================================================
//
//	TProgs::Unload
//
//==========================================================================

void TProgs::Unload()
{
}

//==========================================================================
//
//  TProgs::FindStruct
//
//==========================================================================

VStruct* TProgs::FindStruct(VName InName, VClass* InClass)
{
	guard(TProgs::FindStruct);
	for (int i = 0; i < VMemberBase::GMembers.Num(); i++)
		if (VMemberBase::GMembers[i]->MemberType == MEMBER_Struct &&
			VMemberBase::GMembers[i]->Name == InName && 
			((VStruct*)VMemberBase::GMembers[i])->Outer == InClass)
			return (VStruct*)VMemberBase::GMembers[i];
	return NULL;
	unguard;
}

//==========================================================================
//
//  RunFunction
//
//==========================================================================

static void RunFunction(VMethod *func)
{
	int			*current_statement = NULL;
	int			*sp;
	int			*local_vars;

	guardSlow(RunFunction);
    current_func = func;

	if (func->Flags & FUNC_Native)
	{
		//	Native function, first statement is pointer to function.
		func->NativeFunc();
		return;
	}

	//	Keep stack pointer in register
	sp = pr_stackPtr;

	//	Setup local vars
	local_vars = sp - func->NumParms;
	memset(sp, 0, (func->NumLocals - func->NumParms) * 4);
	sp += func->NumLocals - func->NumParms;

	current_statement = func->Statements.Ptr();

	//
    //	The main function loop
    //
    //	I realy hate using goto in a C program, but this is the only way
    // how to force gcc to create a jump directly here. while(1) would be
    // better, but then gcc, even with optimisations, creates an dummy check,
    // which only takes time.
    //
 func_loop:

	switch (*current_statement++)
	{
     case OPC_Done:
		Sys_Error("Empty function or invalid opcode");
		break;

	 case OPC_Return:
		pr_stackPtr = local_vars;
        return;

	 case OPC_ReturnL:
		local_vars[0] = sp[-1];
		pr_stackPtr = local_vars + 1;
        return;

	 case OPC_ReturnV:
		local_vars[0] = sp[-3];
		local_vars[1] = sp[-2];
		local_vars[2] = sp[-1];
		pr_stackPtr = local_vars + 3;
        return;

	case OPC_PushNumber:
	case OPC_PushString:
	case OPC_PushFunction:
	case OPC_PushClassId:
	case OPC_PushState:
	case OPC_PushName:
		*sp++ = *current_statement++;
		break;

	 case OPC_PushPointed:
		sp[-1] = *(int*)sp[-1];
		break;

	 case OPC_LocalAddress:
#ifdef CHECK_VALID_VAR_NUM
		if (*current_statement < 0 || *current_statement >= MAX_LOCALS)
	    {
	    	Sys_Error("Bad local num %d", *current_statement);
		}
#endif
		*sp++ = (int)&local_vars[*current_statement++];
		break;

	case OPC_Offset:
		sp[-1] += *current_statement++;
		break;

	case OPC_Add:
		sp--;
		sp[-1] += *sp;
		break;

	case OPC_Subtract:
		sp--;
		sp[-1] -= *sp;
		break;

	 case OPC_Multiply:
        sp--;
		sp[-1] *= *sp;
		break;

	 case OPC_Divide:
        sp--;
		sp[-1] /= *sp;
		break;

	 case OPC_Modulus:
        sp--;
		sp[-1] %= *sp;
		break;

	 case OPC_Equals:
        sp--;
		sp[-1] = sp[-1] == *sp;
		break;

	 case OPC_NotEquals:
        sp--;
		sp[-1] = sp[-1] != *sp;
		break;

	 case OPC_Less:
        sp--;
		sp[-1] = sp[-1] < *sp;
		break;

	 case OPC_Greater:
        sp--;
		sp[-1] = sp[-1] > *sp;
		break;

	 case OPC_LessEquals:
        sp--;
		sp[-1] = sp[-1] <= *sp;
		break;

	 case OPC_GreaterEquals:
        sp--;
		sp[-1] = sp[-1] >= *sp;
		break;

	 case OPC_AndLogical:
        sp--;
		sp[-1] = sp[-1] && *sp;
		break;

	 case OPC_OrLogical:
        sp--;
		sp[-1] = sp[-1] || *sp;
		break;

	 case OPC_NegateLogical:
		sp[-1] = !sp[-1];
		break;

	 case OPC_AndBitwise:
        sp--;
		sp[-1] &= *sp;
		break;

	 case OPC_OrBitwise:
        sp--;
		sp[-1] |= *sp;
		break;

	 case OPC_XOrBitwise:
        sp--;
		sp[-1] ^= *sp;
		break;

	 case OPC_LShift:
        sp--;
		sp[-1] <<= *sp;
		break;

	 case OPC_RShift:
        sp--;
		sp[-1] >>= *sp;
		break;

	 case OPC_UnaryMinus:
		sp[-1] = -sp[-1];
		break;

	 case OPC_BitInverse:
		sp[-1] = ~sp[-1];
		break;

	 case OPC_Call:
		pr_stackPtr = sp;
	    RunFunction((VMethod *)*current_statement++);
		current_func = func;
		sp = pr_stackPtr;
		break;

	 case OPC_Goto:
		current_statement = (int *)*current_statement;
		break;

	 case OPC_IfGoto:
        sp--;
		if (*sp)
		{
			current_statement = (int *)*current_statement;
		}
		else
	    {
	        current_statement++;
	    }
		break;

	 case OPC_IfNotGoto:
        sp--;
	    if (!*sp)
	    {
	    	current_statement = (int *)*current_statement;
		}
		else
	    {
	        current_statement++;
	    }
		break;

	 case OPC_CaseGoto:
	 case OPC_CaseGotoClassId:
	 case OPC_CaseGotoName:
		if (*current_statement++ == sp[-1])
	    {
	    	current_statement = (int *)*current_statement;
			sp--;
		}
		else
	    {
	        current_statement++;
	    }
		break;

	 case OPC_Drop:
		sp--;
		break;

	 case OPC_Assign:
        sp--;
		*(int*)sp[-1] = *sp;
		sp[-1] = *(int*)sp[-1];
		break;

	 case OPC_AddVar:
        sp--;
		*(int*)sp[-1] += *sp;
		sp[-1] = *(int*)sp[-1];
		break;

	 case OPC_SubVar:
        sp--;
		*(int*)sp[-1] -= *sp;
		sp[-1] = *(int*)sp[-1];
		break;

	 case OPC_MulVar:
        sp--;
		*(int*)sp[-1] *= *sp;
		sp[-1] = *(int*)sp[-1];
		break;

	 case OPC_DivVar:
        sp--;
		*(int*)sp[-1] /= *sp;
		sp[-1] = *(int*)sp[-1];
		break;

	 case OPC_ModVar:
        sp--;
		*(int*)sp[-1] %= *sp;
		sp[-1] = *(int*)sp[-1];
		break;

	 case OPC_AndVar:
        sp--;
		*(int*)sp[-1] &= *sp;
		sp[-1] = *(int*)sp[-1];
		break;

	 case OPC_OrVar:
        sp--;
		*(int*)sp[-1] |= *sp;
		sp[-1] = *(int*)sp[-1];
		break;

	 case OPC_XOrVar:
        sp--;
		*(int*)sp[-1] ^= *sp;
		sp[-1] = *(int*)sp[-1];
		break;

	 case OPC_LShiftVar:
        sp--;
		*(int*)sp[-1] <<= *sp;
		sp[-1] = *(int*)sp[-1];
		break;

	 case OPC_RShiftVar:
        sp--;
		*(int*)sp[-1] >>= *sp;
		sp[-1] = *(int*)sp[-1];
		break;

	 case OPC_PreInc:
#ifdef __GNUC__
		sp[-1] = ++(*(int*)sp[-1]);
#else
		{
			int		*ptr;

			ptr = (int*)sp[-1];
			++(*ptr);
			sp[-1] = *ptr;
		}
#endif
		break;

	 case OPC_PreDec:
#ifdef __GNUC__
		sp[-1] = --(*(int*)sp[-1]);
#else
		{
			int		*ptr;

			ptr = (int*)sp[-1];
			--(*ptr);
			sp[-1] = *ptr;
		}
#endif
		break;

	 case OPC_PostInc:
#ifdef __GNUC__
		sp[-1] = (*(int*)sp[-1])++;
#else
		{
			int		*ptr;

			ptr = (int*)sp[-1];
			sp[-1] = *ptr;
			(*ptr)++;
		}
#endif
		break;

	 case OPC_PostDec:
#ifdef __GNUC__
		sp[-1] = (*(int*)sp[-1])--;
#else
		{
			int		*ptr;

			ptr = (int*)sp[-1];
			sp[-1] = *ptr;
			(*ptr)--;
		}
#endif
		break;

	 case OPC_IfTopGoto:
	    if (sp[-1])
	    {
	    	current_statement = (int *)*current_statement;
	    }
		else
	    {
	        current_statement++;
	    }
		break;

	 case OPC_IfNotTopGoto:
	    if (!sp[-1])
	    {
	    	current_statement = (int *)*current_statement;
		}
	    else
	    {
	        current_statement++;
	    }
		break;

	 case OPC_AssignDrop:
        sp--;
		*(int*)sp[-1] = *sp;
		sp--;
		break;

	 case OPC_AddVarDrop:
        sp--;
		*(int*)sp[-1] += *sp;
		sp--;
		break;

	 case OPC_SubVarDrop:
        sp--;
		*(int*)sp[-1] -= *sp;
		sp--;
		break;

	 case OPC_MulVarDrop:
        sp--;
		*(int*)sp[-1] *= *sp;
		sp--;
		break;

	 case OPC_DivVarDrop:
        sp--;
		*(int*)sp[-1] /= *sp;
		sp--;
		break;

	 case OPC_ModVarDrop:
        sp--;
		*(int*)sp[-1] %= *sp;
		sp--;
		break;

	 case OPC_AndVarDrop:
        sp--;
		*(int*)sp[-1] &= *sp;
		sp--;
		break;

	 case OPC_OrVarDrop:
        sp--;
		*(int*)sp[-1] |= *sp;
		sp--;
		break;

	 case OPC_XOrVarDrop:
        sp--;
		*(int*)sp[-1] ^= *sp;
		sp--;
		break;

	 case OPC_LShiftVarDrop:
        sp--;
		*(int*)sp[-1] <<= *sp;
		sp--;
		break;

	 case OPC_RShiftVarDrop:
        sp--;
		*(int*)sp[-1] >>= *sp;
		sp--;
		break;

	 case OPC_IncDrop:
		(*(int*)sp[-1])++;
		sp--;
		break;

	 case OPC_DecDrop:
		(*(int*)sp[-1])--;
		sp--;
		break;

#define spf	((float*)sp)

//=====================================
	 case OPC_FAdd:
        sp--;
		spf[-1] += *spf;
		break;

	 case OPC_FSubtract:
        sp--;
		spf[-1] -= *spf;
		break;

	 case OPC_FMultiply:
        sp--;
		spf[-1] *= *spf;
		break;

	 case OPC_FDivide:
        sp--;
		spf[-1] /= *spf;
		break;

	 case OPC_FEquals:
        sp--;
		sp[-1] = spf[-1] == *spf;
		break;

	 case OPC_FNotEquals:
        sp--;
		sp[-1] = spf[-1] != *spf;
		break;

	 case OPC_FLess:
        sp--;
		sp[-1] = spf[-1] < *spf;
		break;

	 case OPC_FGreater:
        sp--;
		sp[-1] = spf[-1] > *spf;
		break;

	 case OPC_FLessEquals:
        sp--;
		sp[-1] = spf[-1] <= *spf;
		break;

	 case OPC_FGreaterEquals:
        sp--;
		sp[-1] = spf[-1] >= *spf;
		break;

	 case OPC_FUnaryMinus:
		spf[-1] = -spf[-1];
		break;

	 case OPC_FAddVar:
        sp--;
		*(float*)sp[-1] += *spf;
		sp[-1] = *(int*)sp[-1];
		break;

	 case OPC_FSubVar:
        sp--;
		*(float*)sp[-1] -= *spf;
		sp[-1] = *(int*)sp[-1];
		break;

	 case OPC_FMulVar:
        sp--;
		*(float*)sp[-1] *= *spf;
		sp[-1] = *(int*)sp[-1];
		break;

	 case OPC_FDivVar:
        sp--;
		*(float*)sp[-1] /= *spf;
		sp[-1] = *(int*)sp[-1];
		break;

	 case OPC_FAddVarDrop:
        sp--;
		*(float*)sp[-1] += *spf;
		sp--;
		break;

	 case OPC_FSubVarDrop:
        sp--;
		*(float*)sp[-1] -= *spf;
		sp--;
		break;

	 case OPC_FMulVarDrop:
        sp--;
		*(float*)sp[-1] *= *spf;
		sp--;
		break;

	 case OPC_FDivVarDrop:
        sp--;
		*(float*)sp[-1] /= *spf;
		sp--;
		break;

//=====================================

	 case OPC_Swap:
		{
			int tmp = sp[-2];
			sp[-2] = sp[-1];
			sp[-1] = tmp;
		}
		break;

	 case OPC_ICall:
		sp--;
		pr_stackPtr = sp;
	    RunFunction((VMethod *)*sp);
		current_func = func;
		sp = pr_stackPtr;
		break;

//=====================================

#define top_vec1	(*(TVec*)(sp - 6))
#define top_vec2	(*(TVec*)(sp - 3))
#define dec_top_vec	(*(TVec*)(sp - 4))

	 case OPC_VPushPointed:
		sp += 2;
		top_vec2 = *(TVec*)sp[-3];
		break;

	 case OPC_VAdd:
		top_vec1 = top_vec1 + top_vec2;
		sp -= 3;
		break;

	 case OPC_VSubtract:
		top_vec1 = top_vec1 - top_vec2;
		sp -= 3;
		break;

	 case OPC_VPreScale:
		dec_top_vec = spf[-4] * top_vec2;
		sp--;
		break;

	 case OPC_VPostScale:
		dec_top_vec = dec_top_vec * spf[-1];
		sp--;
		break;

	 case OPC_VIScale:
		dec_top_vec = dec_top_vec / spf[-1];
		sp--;
		break;

	 case OPC_VEquals:
		sp[-6] = top_vec1 == top_vec2;
		sp -= 5;
		break;

	 case OPC_VNotEquals:
		sp[-6] = top_vec1 != top_vec2;
		sp -= 5;
		break;

	 case OPC_VUnaryMinus:
		top_vec2 = -top_vec2;
		break;

	 case OPC_VDrop:
		sp -= 3;
		break;

	 case OPC_VAssign:
		*(TVec*)sp[-4] = top_vec2;
		dec_top_vec = *(TVec*)sp[-4];
        sp--;
		break;

	 case OPC_VAddVar:
		*(TVec*)sp[-4] += top_vec2;
		dec_top_vec = *(TVec*)sp[-4];
        sp--;
		break;

	 case OPC_VSubVar:
		*(TVec*)sp[-4] -= top_vec2;
		dec_top_vec = *(TVec*)sp[-4];
        sp--;
		break;

	 case OPC_VScaleVar:
		sp++;
		*(TVec*)sp[-3] *= spf[-2];
		top_vec2 = *(TVec*)sp[-3];
		break;

	 case OPC_VIScaleVar:
		sp++;
		*(TVec*)sp[-3] /= spf[-2];
		top_vec2 = *(TVec*)sp[-3];
		break;

	 case OPC_VAssignDrop:
		*(TVec*)sp[-4] = top_vec2;
        sp -= 4;
		break;

	 case OPC_VAddVarDrop:
		*(TVec*)sp[-4] += top_vec2;
        sp -= 4;
		break;

	 case OPC_VSubVarDrop:
		*(TVec*)sp[-4] -= top_vec2;
        sp -= 4;
		break;

	 case OPC_VScaleVarDrop:
		*(TVec*)sp[-2] *= spf[-1];
		sp -= 2;
		break;

	 case OPC_VIScaleVarDrop:
		*(TVec*)sp[-2] /= spf[-1];
		sp -= 2;
		break;

//=====================================

	 case OPC_Copy:
		*sp = sp[-1];
		sp++;
		break;

	 case OPC_Swap3:
		{
			int tmp = sp[-4];
			sp[-4] = sp[-3];
			sp[-3] = sp[-2];
			sp[-2] = sp[-1];
			sp[-1] = tmp;
		}
		break;

	 case OPC_DynamicCast:
		sp[-1] = sp[-1] && ((VObject*)sp[-1])->IsA((VClass*)*current_statement++) ? sp[-1] : 0;
		break;

	case OPC_PushBool:
		{
			int mask = *current_statement++;
			sp[-1] = !!(*(int*)sp[-1] & mask);
		}
		break;

	case OPC_AssignBool:
		{
			int mask = *current_statement++;
			sp--;
			if (*sp)
				*(int*)sp[-1] |= mask;
			else
				*(int*)sp[-1] &= ~mask;
			sp--;
		}
		break;

	case OPC_PushVFunc:
		*sp = (int)((VObject*)sp[-1])->GetVFunction(*current_statement++);
		sp++;
		break;

	case OPC_PushPointedDelegate:
		sp[0] = ((int*)sp[-1])[1];
		sp[-1] = ((int*)sp[-1])[0];
		sp++;
		break;

	case OPC_AssignDelegate:
		((int*)sp[-3])[0] = sp[-2];
		((int*)sp[-3])[1] = sp[-1];
		sp -= 3;
		break;

	default:
#ifdef CHECK_VALID_OPCODE
		Sys_Error("Invalid opcode %d", current_statement[-1]);
#endif
		break;
	}

    goto func_loop;
	unguardfSlow(("(%s %d)", *func->GetFullName(),
		current_statement - func->Statements.Ptr()));
}

//==========================================================================
//
//  TProgs::ExecuteFunction
//
//==========================================================================

int TProgs::ExecuteFunction(VMethod *func)
{
	guard(TProgs::ExecuteFunction);
	VMethod		*prev_func;
	int				ret = 0;

	//	Run function
	prev_func = current_func;
	RunFunction(func);
	current_func = prev_func;

	//	Get return value
	if (func->Type)
    {
		ret = *(--pr_stackPtr);
	}

#ifdef CHECK_FOR_EMPTY_STACK
	//	After executing base function stack must be empty
    if (!current_func && pr_stackPtr != pr_stack + 1)
    {
    	Sys_Error("ExecuteFunction: Stack is not empty after executing function:\n%s\nstack = %p, sp = %p",
            *func->Name, pr_stack, pr_stackPtr);
    }
#endif

#ifdef CHECK_STACK_UNDERFLOW
	//	Check, if stack wasn't underflowed
	if (pr_stack[0] != STACK_ID)
   	{
   		Sys_Error("ExecuteFunction: Stack underflow in %s", *func->Name);
    }
#endif

#ifdef CHECK_STACK_OVERFLOW
	//	Check, if stack wasn't overflowed
	if (pr_stack[MAX_PROG_STACK - 1] != STACK_ID)
   	{
   		Sys_Error("ExecuteFunction: Stack overflow in %s", *func->Name);
    }
#endif

	//	All done
	return ret;
	unguardf(("(%s)", *func->GetFullName()));
}

//==========================================================================
//
//	TProgs::Exec
//
//==========================================================================

int TProgs::Exec(VMethod *func)
{
#ifdef CHECK_PARM_COUNT
    if (Functions[fnum]->NumParms != 0)
    {
    	Sys_Error("TProgs::Exec: Function %s haves %d parms, not 0",
    		FuncName(fnum), Functions[fnum]->NumParms);
    }
#endif
    return ExecuteFunction(func);
}

//==========================================================================
//
//	TProgs::Exec
//
//==========================================================================

int TProgs::Exec(VMethod *func, int parm1)
{
	int		*p;

#ifdef CHECK_PARM_COUNT
    if (Functions[fnum]->NumParms != 1)
    {
    	Sys_Error("TProgs::Exec: Function %s haves %d parms, not 1",
    		FuncName(fnum), Functions[fnum]->NumParms);
    }
#endif
	p = pr_stackPtr;
	pr_stackPtr += 1;
	p[0] = parm1;
    return ExecuteFunction(func);
}

//==========================================================================
//
//	TProgs::Exec
//
//==========================================================================

int TProgs::Exec(VMethod *func, int parm1, int parm2)
{
	int		*p;

#ifdef CHECK_PARM_COUNT
    if (Functions[fnum]->NumParms != 2)
    {
    	Sys_Error("TProgs::Exec: Function %s haves %d parms, not 2",
    		FuncName(fnum), Functions[fnum]->NumParms);
    }
#endif
	p = pr_stackPtr;
	pr_stackPtr += 2;
	p[0] = parm1;
	p[1] = parm2;
    return ExecuteFunction(func);
}

//==========================================================================
//
//	TProgs::Exec
//
//==========================================================================

int TProgs::Exec(VMethod *func, int parm1, int parm2, int parm3)
{
	int		*p;

#ifdef CHECK_PARM_COUNT
    if (Functions[fnum]->NumParms != 3)
    {
    	Sys_Error("TProgs::Exec: Function %s haves %d parms, not 3",
    		FuncName(fnum), Functions[fnum]->NumParms);
    }
#endif
	p = pr_stackPtr;
	pr_stackPtr += 3;
	p[0] = parm1;
	p[1] = parm2;
	p[2] = parm3;
    return ExecuteFunction(func);
}

//==========================================================================
//
//	TProgs::Exec
//
//==========================================================================

int TProgs::Exec(VMethod *func, int parm1, int parm2, int parm3, int parm4)
{
	int		*p;

#ifdef CHECK_PARM_COUNT
    if (Functions[fnum]->NumParms != 4)
    {
    	Sys_Error("TProgs::Exec: Function %s haves %d parms, not 4",
    		FuncName(fnum), Functions[fnum]->NumParms);
    }
#endif
	p = pr_stackPtr;
	pr_stackPtr += 4;
	p[0] = parm1;
	p[1] = parm2;
	p[2] = parm3;
	p[3] = parm4;
    return ExecuteFunction(func);
}

//==========================================================================
//
//	TProgs::Exec
//
//==========================================================================

int TProgs::Exec(VMethod *func, int parm1, int parm2, int parm3, int parm4,
	int parm5)
{
	int		*p;

#ifdef CHECK_PARM_COUNT
    if (Functions[fnum]->NumParms != 5)
    {
    	Sys_Error("TProgs::Exec: Function %s haves %d parms, not 5",
    		FuncName(fnum), Functions[fnum]->NumParms);
    }
#endif
	p = pr_stackPtr;
	pr_stackPtr += 5;
	p[0] = parm1;
	p[1] = parm2;
	p[2] = parm3;
	p[3] = parm4;
	p[4] = parm5;
    return ExecuteFunction(func);
}

//==========================================================================
//
//	TProgs::Exec
//
//==========================================================================

int TProgs::Exec(VMethod *func, int parm1, int parm2, int parm3, int parm4,
	int parm5, int parm6)
{
	int		*p;

#ifdef CHECK_PARM_COUNT
    if (Functions[fnum]->NumParms != 6)
    {
    	Sys_Error("TProgs::Exec: Function %s haves %d parms, not 6",
    		FuncName(fnum), Functions[fnum]->NumParms);
    }
#endif
	p = pr_stackPtr;
	pr_stackPtr += 6;
	p[0] = parm1;
	p[1] = parm2;
	p[2] = parm3;
	p[3] = parm4;
	p[4] = parm5;
	p[5] = parm6;
    return ExecuteFunction(func);
}

//==========================================================================
//
//	TProgs::Exec
//
//==========================================================================

int TProgs::Exec(VMethod *func, int parm1, int parm2, int parm3, int parm4,
	int parm5, int parm6, int parm7)
{
	int		*p;

#ifdef CHECK_PARM_COUNT
    if (Functions[fnum]->NumParms != 7)
    {
    	Sys_Error("TProgs::Exec: Function %s haves %d parms, not 7",
    		FuncName(fnum), Functions[fnum]->NumParms);
    }
#endif
	p = pr_stackPtr;
	pr_stackPtr += 7;
	p[0] = parm1;
	p[1] = parm2;
	p[2] = parm3;
	p[3] = parm4;
	p[4] = parm5;
	p[5] = parm6;
	p[6] = parm7;
    return ExecuteFunction(func);
}

//==========================================================================
//
//	TProgs::Exec
//
//==========================================================================

int TProgs::Exec(VMethod *func, int parm1, int parm2, int parm3, int parm4,
	int parm5, int parm6, int parm7, int parm8)
{
	int		*p;

#ifdef CHECK_PARM_COUNT
    if (Functions[fnum]->NumParms != 8)
    {
    	Sys_Error("TProgs::Exec: Function %s haves %d parms, not 8",
    		FuncName(fnum), Functions[fnum]->NumParms);
    }
#endif
	p = pr_stackPtr;
	pr_stackPtr += 8;
	p[0] = parm1;
	p[1] = parm2;
	p[2] = parm3;
	p[3] = parm4;
	p[4] = parm5;
	p[5] = parm6;
	p[6] = parm7;
	p[7] = parm8;
    return ExecuteFunction(func);
}

//==========================================================================
//
//	TProgs::Exec
//
//==========================================================================

int TProgs::Exec(VMethod *func, int parm1, int parm2, int parm3, int parm4,
	int parm5, int parm6, int parm7, int parm8, int parm9)
{
	int		*p;

#ifdef CHECK_PARM_COUNT
    if (Functions[fnum]->NumParms != 9)
    {
    	Sys_Error("TProgs::Exec: Function %s haves %d parms, not 9",
    		FuncName(fnum), Functions[fnum]->NumParms);
    }
#endif
	p = pr_stackPtr;
	pr_stackPtr += 9;
	p[0] = parm1;
	p[1] = parm2;
	p[2] = parm3;
	p[3] = parm4;
	p[4] = parm5;
	p[5] = parm6;
	p[6] = parm7;
	p[7] = parm8;
	p[8] = parm9;
    return ExecuteFunction(func);
}

//==========================================================================
//
//	TProgs::Exec
//
//==========================================================================

int TProgs::Exec(VMethod *func, int parm1, int parm2, int parm3, int parm4,
	int parm5, int parm6, int parm7, int parm8, int parm9, int parm10)
{
	int		*p;

#ifdef CHECK_PARM_COUNT
    if (Functions[fnum]->NumParms != 10)
    {
    	Sys_Error("TProgs::Exec: Function %s haves %d parms, not 10",
    		FuncName(fnum), Functions[fnum]->NumParms);
    }
#endif
	p = pr_stackPtr;
	pr_stackPtr += 10;
	p[0] = parm1;
	p[1] = parm2;
	p[2] = parm3;
	p[3] = parm4;
	p[4] = parm5;
	p[5] = parm6;
	p[6] = parm7;
	p[7] = parm8;
	p[8] = parm9;
	p[9] = parm10;
    return ExecuteFunction(func);
}

//==========================================================================
//
//	TProgs::Exec
//
//==========================================================================

int TProgs::Exec(VMethod *func, int parm1, int parm2, int parm3, int parm4,
	int parm5, int parm6, int parm7, int parm8, int parm9, int parm10,
	int parm11)
{
	int		*p;

#ifdef CHECK_PARM_COUNT
    if (Functions[fnum]->NumParms != 11)
    {
    	Sys_Error("TProgs::Exec: Function %s haves %d parms, not 11",
    		FuncName(fnum), Functions[fnum]->NumParms);
    }
#endif
	p = pr_stackPtr;
	pr_stackPtr += 11;
	p[0] = parm1;
	p[1] = parm2;
	p[2] = parm3;
	p[3] = parm4;
	p[4] = parm5;
	p[5] = parm6;
	p[6] = parm7;
	p[7] = parm8;
	p[8] = parm9;
	p[9] = parm10;
	p[10] = parm11;
    return ExecuteFunction(func);
}

//==========================================================================
//
//	TProgs::Exec
//
//==========================================================================

int TProgs::Exec(VMethod *func, int parm1, int parm2, int parm3, int parm4,
	int parm5, int parm6, int parm7, int parm8, int parm9, int parm10,
	int parm11, int parm12)
{
	int		*p;

#ifdef CHECK_PARM_COUNT
    if (Functions[fnum]->NumParms != 12)
    {
    	Sys_Error("TProgs::Exec: Function %s haves %d parms, not 12",
    		FuncName(fnum), Functions[fnum]->NumParms);
    }
#endif
	p = pr_stackPtr;
	pr_stackPtr += 12;
	p[0] = parm1;
	p[1] = parm2;
	p[2] = parm3;
	p[3] = parm4;
	p[4] = parm5;
	p[5] = parm6;
	p[6] = parm7;
	p[7] = parm8;
	p[8] = parm9;
	p[9] = parm10;
	p[10] = parm11;
	p[11] = parm12;
    return ExecuteFunction(func);
}

//==========================================================================
//
//	TProgs::Exec
//
//==========================================================================

int TProgs::Exec(VMethod *func, int parm1, int parm2, int parm3, int parm4,
	int parm5, int parm6, int parm7, int parm8, int parm9, int parm10,
	int parm11, int parm12, int parm13)
{
	int		*p;

#ifdef CHECK_PARM_COUNT
    if (Functions[fnum]->NumParms != 13)
    {
    	Sys_Error("TProgs::Exec: Function %s haves %d parms, not 13",
    		FuncName(fnum), Functions[fnum]->NumParms);
    }
#endif
	p = pr_stackPtr;
	pr_stackPtr += 13;
	p[0] = parm1;
	p[1] = parm2;
	p[2] = parm3;
	p[3] = parm4;
	p[4] = parm5;
	p[5] = parm6;
	p[6] = parm7;
	p[7] = parm8;
	p[8] = parm9;
	p[9] = parm10;
	p[10] = parm11;
	p[11] = parm12;
	p[12] = parm13;
    return ExecuteFunction(func);
}

//==========================================================================
//
//	TProgs::Exec
//
//==========================================================================

int TProgs::Exec(VMethod *func, int parm1, int parm2, int parm3, int parm4,
	int parm5, int parm6, int parm7, int parm8, int parm9, int parm10,
	int parm11, int parm12, int parm13, int parm14)
{
	int		*p;

#ifdef CHECK_PARM_COUNT
    if (Functions[fnum]->NumParms != 14)
    {
    	Sys_Error("TProgs::Exec: Function %s haves %d parms, not 14",
    		FuncName(fnum), Functions[fnum]->NumParms);
    }
#endif
	p = pr_stackPtr;
	pr_stackPtr += 14;
	p[0] = parm1;
	p[1] = parm2;
	p[2] = parm3;
	p[3] = parm4;
	p[4] = parm5;
	p[5] = parm6;
	p[6] = parm7;
	p[7] = parm8;
	p[8] = parm9;
	p[9] = parm10;
	p[10] = parm11;
	p[11] = parm12;
	p[12] = parm13;
	p[13] = parm14;
    return ExecuteFunction(func);
}

//==========================================================================
//
//	TProgs::Exec
//
//==========================================================================

int TProgs::Exec(VMethod *func, int parm1, int parm2, int parm3, int parm4,
	int parm5, int parm6, int parm7, int parm8, int parm9, int parm10,
	int parm11, int parm12, int parm13, int parm14, int parm15)
{
	int		*p;

#ifdef CHECK_PARM_COUNT
    if (Functions[fnum]->NumParms != 15)
    {
    	Sys_Error("TProgs::Exec: Function %s haves %d parms, not 15",
    		FuncName(fnum), Functions[fnum]->NumParms);
    }
#endif
	p = pr_stackPtr;
	pr_stackPtr += 15;
	p[0] = parm1;
	p[1] = parm2;
	p[2] = parm3;
	p[3] = parm4;
	p[4] = parm5;
	p[5] = parm6;
	p[6] = parm7;
	p[7] = parm8;
	p[8] = parm9;
	p[9] = parm10;
	p[10] = parm11;
	p[11] = parm12;
	p[12] = parm13;
	p[13] = parm14;
	p[14] = parm15;
    return ExecuteFunction(func);
}

//==========================================================================
//
//	TProgs::Exec
//
//==========================================================================

int TProgs::Exec(VMethod *func, int parm1, int parm2, int parm3, int parm4,
	int parm5, int parm6, int parm7, int parm8, int parm9, int parm10,
	int parm11, int parm12, int parm13, int parm14, int parm15, int parm16)
{
	int		*p;

#ifdef CHECK_PARM_COUNT
    if (Functions[fnum]->NumParms != 16)
    {
    	Sys_Error("TProgs::Exec: Function %s haves %d parms, not 16",
    		FuncName(fnum), Functions[fnum]->NumParms);
    }
#endif
	p = pr_stackPtr;
	pr_stackPtr += 16;
	p[0] = parm1;
	p[1] = parm2;
	p[2] = parm3;
	p[3] = parm4;
	p[4] = parm5;
	p[5] = parm6;
	p[6] = parm7;
	p[7] = parm8;
	p[8] = parm9;
	p[9] = parm10;
	p[10] = parm11;
	p[11] = parm12;
	p[12] = parm13;
	p[13] = parm14;
	p[14] = parm15;
	p[15] = parm16;
    return ExecuteFunction(func);
}

//==========================================================================
//
//	TProgs::DumpProfile
//
//==========================================================================

void TProgs::DumpProfile()
{
	#define MAX_PROF	100
	int i;
	int profsort[MAX_PROF];
	int totalcount = 0;
	memset(profsort, 0, sizeof(profsort));
	for (i = 0; i < VMemberBase::GMembers.Num(); i++)
	{
		if (VMemberBase::GMembers[i]->MemberType != MEMBER_Method)
		{
			continue;
		}
		VMethod* Func = (VMethod*)VMemberBase::GMembers[i];
		if (!Func->Profile1)
		{
			//	Never called
			continue;
		}
		for (int j = 0; j < MAX_PROF; j++)
		{
			totalcount += Func->Profile2;
			if (((VMethod*)VMemberBase::GMembers[profsort[j]])->Profile2 <= Func->Profile2)
			{
				for (int k = MAX_PROF - 1; k > j; k--)
				{
					profsort[k] = profsort[k - 1];
				}
				profsort[j] = i;
				break;
			}
		}
	}
	if (!totalcount)
	{
		return;
	}
	for (i = 0; i < MAX_PROF && profsort[i]; i++)
	{
		VMethod* Func = (VMethod*)VMemberBase::GMembers[profsort[i]];
		GCon->Logf("%3.2f%% (%9d) %9d %s",
			(double)Func->Profile2 * 100.0 / (double)totalcount,
			(int)Func->Profile2, (int)Func->Profile1, *Func->GetFullName());
	}
}

int TProgs::GetStringOffs(const char *Str)
{
	return Str - Pkg->Strings;
}

char *TProgs::StrAtOffs(int Offs)
{
	return Pkg->Strings + Offs;
}
//**************************************************************************
//
//	$Log$
//	Revision 1.55  2006/03/29 22:32:27  dj_jl
//	Changed console variables and command buffer to use dynamic strings.
//
//	Revision 1.54  2006/03/26 13:06:18  dj_jl
//	Implemented support for modular progs.
//	
//	Revision 1.53  2006/03/23 18:31:59  dj_jl
//	Members tree.
//	
//	Revision 1.52  2006/03/18 16:51:15  dj_jl
//	Renamed type class names, better code serialisation.
//	
//	Revision 1.51  2006/03/13 19:29:57  dj_jl
//	Clean function local variables.
//	
//	Revision 1.50  2006/03/12 20:06:02  dj_jl
//	States as objects, added state variable type.
//	
//	Revision 1.49  2006/03/12 12:54:49  dj_jl
//	Removed use of bitfields for portability reasons.
//	
//	Revision 1.48  2006/03/10 19:31:25  dj_jl
//	Use serialisation for progs files.
//	
//	Revision 1.47  2006/03/06 13:02:32  dj_jl
//	Cleaning up references to destroyed objects.
//	
//	Revision 1.46  2006/03/04 16:01:34  dj_jl
//	File system API now uses strings.
//	
//	Revision 1.45  2006/03/02 23:24:35  dj_jl
//	Wad lump names stored as names.
//	
//	Revision 1.44  2006/02/27 20:45:26  dj_jl
//	Rewrote names class.
//	
//	Revision 1.43  2006/02/25 17:09:35  dj_jl
//	Import all progs type info.
//	
//	Revision 1.42  2006/02/19 20:36:02  dj_jl
//	Implemented support for delegates.
//	
//	Revision 1.41  2006/02/17 19:23:47  dj_jl
//	Removed support for progs global variables.
//	
//	Revision 1.40  2006/02/15 23:27:41  dj_jl
//	Added script ID class attribute.
//	
//	Revision 1.39  2006/02/13 18:34:34  dj_jl
//	Moved all server progs global functions to classes.
//	
//	Revision 1.38  2005/12/14 18:54:53  dj_jl
//	Removed compiler limitations.
//	
//	Revision 1.37  2005/12/07 22:53:26  dj_jl
//	Moved compiler generated data out of globals.
//	
//	Revision 1.36  2005/11/24 20:06:47  dj_jl
//	Renamed opcodes.
//	
//	Revision 1.35  2005/11/20 15:52:03  dj_jl
//	Fixes for MacOS X.
//	
//	Revision 1.34  2005/11/07 23:00:24  dj_jl
//	Fixed a compiler warning.
//	
//	Revision 1.33  2004/12/27 12:23:16  dj_jl
//	Multiple small changes for version 1.16
//	
//	Revision 1.32  2004/12/22 07:37:21  dj_jl
//	Increased argument count limit.
//	
//	Revision 1.31  2004/08/21 15:03:07  dj_jl
//	Remade VClass to be standalone class.
//	
//	Revision 1.30  2002/11/02 17:09:55  dj_jl
//	Some debugging stuff.
//	
//	Revision 1.29  2002/07/23 16:29:56  dj_jl
//	Replaced console streams with output device class.
//	
//	Revision 1.28  2002/05/03 17:04:35  dj_jl
//	Mangling of string pointers.
//	
//	Revision 1.27  2002/04/11 16:45:42  dj_jl
//	Ignoring natives in profiling.
//	
//	Revision 1.26  2002/03/16 17:53:12  dj_jl
//	Added opcode for pushing virtual function.
//	
//	Revision 1.25  2002/03/09 18:05:34  dj_jl
//	Added support for defining native functions outside pr_cmds
//	
//	Revision 1.24  2002/02/26 17:54:26  dj_jl
//	Importing special property info from progs and using it in saving.
//	
//	Revision 1.23  2002/02/16 16:29:26  dj_jl
//	Added support for bool variables
//	
//	Revision 1.22  2002/02/02 19:20:41  dj_jl
//	VMethod pointers used instead of the function numbers
//	
//	Revision 1.21  2002/01/28 18:44:44  dj_jl
//	Fixed dynamic cast
//	
//	Revision 1.20  2002/01/25 18:06:53  dj_jl
//	Little changes for progs profiling
//	
//	Revision 1.19  2002/01/17 18:21:40  dj_jl
//	Fixed Hexen class bug
//	
//	Revision 1.18  2002/01/11 08:07:17  dj_jl
//	Added names to progs
//	
//	Revision 1.17  2002/01/07 12:16:43  dj_jl
//	Changed copyright year
//	
//	Revision 1.16  2002/01/04 18:22:13  dj_jl
//	Beautification
//	
//	Revision 1.15  2002/01/03 18:38:25  dj_jl
//	Added guard macros and core dumps
//	
//	Revision 1.14  2001/12/27 17:39:10  dj_jl
//	Added method count to VClass
//	
//	Revision 1.13  2001/12/18 19:03:16  dj_jl
//	A lots of work on VObject
//	
//	Revision 1.12  2001/12/12 19:27:46  dj_jl
//	Added dynamic cast
//	
//	Revision 1.11  2001/12/03 19:21:45  dj_jl
//	Added swaping with vector
//	
//	Revision 1.10  2001/12/01 17:43:13  dj_jl
//	Renamed ClassBase to VObject
//	
//	Revision 1.9  2001/10/27 07:48:25  dj_jl
//	Added constructors and destructors
//	
//	Revision 1.8  2001/10/04 17:24:21  dj_jl
//	Got rid of some warnings
//	
//	Revision 1.7  2001/09/25 17:06:22  dj_jl
//	Added parent's vtable to vtable
//	
//	Revision 1.6  2001/09/20 16:30:28  dj_jl
//	Started to use object-oriented stuff in progs
//	
//	Revision 1.5  2001/08/31 17:28:00  dj_jl
//	Removed RANGECHECK
//	
//	Revision 1.4  2001/08/21 17:40:24  dj_jl
//	Real string pointers in progs
//	In devgame mode look for progs in <gamedir>/progs
//	
//	Revision 1.3  2001/07/31 17:16:31  dj_jl
//	Just moved Log to the end of file
//	
//	Revision 1.2  2001/07/27 14:27:54  dj_jl
//	Update with Id-s and Log-s, some fixes
//
//**************************************************************************
