//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
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

#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
# include "gamedefs.h"
# include "progdefs.h"
#else
# if defined(IN_VCC)
#  include "../utils/vcc/vcc.h"
# elif defined(VCC_STANDALONE_EXECUTOR)
#  include "../vccrun/vcc_run.h"
# endif
#endif

// MACROS ------------------------------------------------------------------

#define MAX_PROG_STACK  (10000)
#define STACK_ID  (0x45f6cd4b)

#define CHECK_STACK_OVERFLOW
#define CHECK_STACK_UNDERFLOW
#define CHECK_FOR_EMPTY_STACK

//#define VMEXEC_RUNDUMP

#ifdef VMEXEC_RUNDUMP
static int k8edIndent = 0;

static void printIndent () { for (int f = k8edIndent; f > 0; --f) fputc(' ', stdout); }
static void enterIndent () { ++k8edIndent; }
static void leaveIndent () { if (--k8edIndent < 0) *(int *)0 = 0; }
#endif


// ////////////////////////////////////////////////////////////////////////// //
VStack *pr_stackPtr;

static VMethod *current_func = nullptr;
static VStack pr_stack[MAX_PROG_STACK];

struct VCSlice {
  vuint8 *ptr; // it is easier to index this way
  vint32 length;
};


//==========================================================================
//
//  PR_Init
//
//==========================================================================
void PR_Init () {
  // set stack ID for overflow / underflow checks
  pr_stack[0].i = STACK_ID;
  pr_stack[MAX_PROG_STACK-1].i = STACK_ID;
  pr_stackPtr = pr_stack+1;
}


//==========================================================================
//
//  PR_OnAbort
//
//==========================================================================
void PR_OnAbort () {
  current_func = nullptr;
  pr_stackPtr = pr_stack+1;
}


//==========================================================================
//
//  PR_Profile1
//
//==========================================================================
extern "C" void PR_Profile1 () {
  ++(current_func->Profile1);
}


//==========================================================================
//
//  PR_Profile2
//
//==========================================================================
extern "C" void PR_Profile2 () {
  if (current_func && (!(current_func->Flags&FUNC_Native))) ++(current_func->Profile2);
}

//==========================================================================
//
//  PR_Profile2_end
//
//==========================================================================
extern "C" void PR_Profile2_end () {}


// ////////////////////////////////////////////////////////////////////////// //
// stack trace utilities

struct CallStackItem {
  VMethod *func;
  const vuint8 *ip;
  VStack *sp;
};

static CallStackItem *callStack = nullptr;
static vuint32 cstUsed = 0, cstSize = 0;


static inline void cstFixTopIPSP (const vuint8 *ip) {
  if (cstUsed > 0) {
    callStack[cstUsed-1].ip = ip;
    callStack[cstUsed-1].sp = pr_stackPtr;
  }
}


static void cstPush (VMethod *func) {
  if (cstUsed == cstSize) {
    //FIXME: handle OOM here
    cstSize += 16384;
    callStack = (CallStackItem *)realloc(callStack, sizeof(callStack[0])*cstSize);
  }
  callStack[cstUsed].func = func;
  callStack[cstUsed].ip = nullptr;
  callStack[cstUsed].sp = pr_stackPtr;
  ++cstUsed;
}


static inline void cstPop () {
  if (cstUsed > 0) --cstUsed;
}


// `ip` can be null
static void cstDump (const vuint8 *ip) {
  //ip = func->Statements.Ptr();
  fprintf(stderr, "\n\n=== VaVoomScript Call Stack (%u) ===\n", cstUsed);
  if (cstUsed > 0) {
    // do the best thing we can
    if (!ip && callStack[cstUsed-1].ip) ip = callStack[cstUsed-1].ip;
    for (vuint32 sp = cstUsed; sp > 0; --sp) {
      VMethod *func = callStack[sp-1].func;
      TLocation loc = func->FindPCLocation(sp == cstUsed ? ip : callStack[sp-1].ip);
      if (!loc.isInternal()) {
        fprintf(stderr, "  %03u: %s (%s:%d)\n", cstUsed-sp, *func->GetFullName(), *loc.GetSource(), loc.GetLine());
      } else {
        fprintf(stderr, "  %03u: %s\n", cstUsed-sp, *func->GetFullName());
      }
    }
  }
  fprintf(stderr, "=============================\n\n");
}


//==========================================================================
//
//  RunFunction
//
//==========================================================================

#ifdef __GNUC__
# define USE_COMPUTED_GOTO  1
#endif

#if USE_COMPUTED_GOTO
# define PR_VM_SWITCH(op)  goto *vm_labels[op];
# define PR_VM_CASE(x)   Lbl_ ## x:
# define PR_VM_BREAK     goto *vm_labels[*ip];
# define PR_VM_DEFAULT
#else
# define PR_VM_SWITCH(op)  switch(op)
# define PR_VM_CASE(x)   case x:
# define PR_VM_BREAK     break
# define PR_VM_DEFAULT   default:
#endif

#define ReadInt16(ip)   (*(vint16 *)(ip))
#define ReadInt32(ip)   (*(vint32 *)(ip))
#define ReadPtr(ip)     (*(void **)(ip))
#define ReadType(T, ip) do { \
  T.Type = (ip)[0]; \
  T.ArrayInnerType = (ip)[1]; \
  T.InnerType = (ip)[2]; \
  T.PtrLevel = (ip)[3]; \
  T.ArrayDim = ReadInt32((ip)+4); \
  T.Class = (VClass *)ReadPtr((ip)+8); \
  ip += 8+sizeof(VClass *); \
} while (0)


static void RunFunction (VMethod *func) {
  vuint8 *ip = nullptr;
  VStack *sp;
  VStack *local_vars;
  VScriptIterator *ActiveIterators = nullptr;
  float ftemp;
  vint32 itemp;

  guard(RunFunction);
  current_func = func;

  if (func->Flags&FUNC_Net) {
    VStack *Params = pr_stackPtr-func->ParamsSize;
    if (((VObject *)Params[0].p)->ExecuteNetMethod(func)) return;
  }

  if (func->Flags&FUNC_Native) {
    // native function, first statement is pointer to function
    func->NativeFunc();
    return;
  }

  cstPush(func);

  // cache stack pointer in register
  sp = pr_stackPtr;

  // setup local vars
  local_vars = sp-func->ParamsSize;
  memset(sp, 0, (func->NumLocals-func->ParamsSize)*sizeof(VStack));
  sp += func->NumLocals-func->ParamsSize;

  ip = func->Statements.Ptr();

#ifdef VMEXEC_RUNDUMP
  enterIndent(); printIndent(); printf("ENTERING VC FUNCTION `%s`; sp=%d\n", *func->GetFullName(), (int)(sp-pr_stack));
#endif

  // the main execution loop
  for (;;) {
func_loop:

#if USE_COMPUTED_GOTO
    static void *vm_labels[] = {
#define DECLARE_OPC(name, args) &&Lbl_OPC_ ## name
#define OPCODE_INFO
#include "progdefs.h"
    0 };
#endif

    PR_VM_SWITCH(*ip) {
      PR_VM_CASE(OPC_Done)
        cstDump(ip);
        Sys_Error("Empty function or invalid opcode");
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Call)
        pr_stackPtr = sp;
        cstFixTopIPSP(ip);
        RunFunction((VMethod *)ReadPtr(ip+1));
        current_func = func;
        ip += 1+sizeof(void *);
        sp = pr_stackPtr;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushVFunc)
        sp[0].p = ((VObject *)sp[-1].p)->GetVFunctionIdx(ReadInt16(ip+1));
        ip += 3;
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushVFuncB)
        sp[0].p = ((VObject *)sp[-1].p)->GetVFunctionIdx(ip[1]);
        ip += 2;
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VCall)
        pr_stackPtr = sp;
        if (!sp[-ip[3]].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        cstFixTopIPSP(ip);
        RunFunction(((VObject *)sp[-ip[3]].p)->GetVFunctionIdx(ReadInt16(ip+1)));
        ip += 4;
        current_func = func;
        sp = pr_stackPtr;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VCallB)
        pr_stackPtr = sp;
        if (!sp[-ip[2]].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        cstFixTopIPSP(ip);
        RunFunction(((VObject *)sp[-ip[2]].p)->GetVFunctionIdx(ip[1]));
        ip += 3;
        current_func = func;
        sp = pr_stackPtr;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DelegateCall)
        {
          // get pointer to the delegate
          void **pDelegate = (void **)((vuint8 *)sp[-ip[5]].p+ReadInt32(ip+1));
          // push proper self object
          if (!pDelegate[0]) { cstDump(ip); Sys_Error("Delegate is not initialised"); }
          sp[-ip[5]].p = pDelegate[0];
          pr_stackPtr = sp;
          cstFixTopIPSP(ip);
          RunFunction((VMethod *)pDelegate[1]);
        }
        ip += 6;
        current_func = func;
        sp = pr_stackPtr;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DelegateCallS)
        {
          // get pointer to the delegate
          void **pDelegate = (void **)((vuint8 *)sp[-ip[3]].p+ReadInt16(ip+1));
          // push proper self object
          if (!pDelegate[0]) { cstDump(ip); Sys_Error("Delegate is not initialised"); }
          sp[-ip[3]].p = pDelegate[0];
          pr_stackPtr = sp;
          cstFixTopIPSP(ip);
          RunFunction((VMethod *)pDelegate[1]);
        }
        ip += 4;
        current_func = func;
        sp = pr_stackPtr;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DelegateCallB)
        {
          // get pointer to the delegate
          void **pDelegate = (void **)((vuint8 *)sp[-ip[2]].p+ip[1]);
          // push proper self object
          if (!pDelegate[0]) { cstDump(ip); Sys_Error("Delegate is not initialised"); }
          sp[-ip[2]].p = pDelegate[0];
          pr_stackPtr = sp;
          cstFixTopIPSP(ip);
          RunFunction((VMethod *)pDelegate[1]);
        }
        ip += 3;
        current_func = func;
        sp = pr_stackPtr;
        PR_VM_BREAK;

      // call delegate by a pushed pointer to it
      PR_VM_CASE(OPC_DelegateCallPtr)
        {
          // get args size (to get `self` offset)
          int sofs = ReadInt32(ip+1);
          ip += 5;
          // get pointer to the delegate
          void **pDelegate = (void **)sp[-1].p;
          // drop delegate argument
          sp -= 1;
          // push proper self object
          if (!pDelegate[0]) { cstDump(ip); Sys_Error("Delegate is not initialised"); }
          sp[-sofs].p = pDelegate[0];
          pr_stackPtr = sp;
          cstFixTopIPSP(ip);
          RunFunction((VMethod *)pDelegate[1]);
        }
        current_func = func;
        sp = pr_stackPtr;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Return)
        checkSlow(sp == local_vars+func->NumLocals);
        pr_stackPtr = local_vars;
        while (ActiveIterators) {
          VScriptIterator *Tmp = ActiveIterators;
          ActiveIterators = Tmp->Next;
          delete Tmp;
          Tmp = nullptr;
        }
        #ifdef VMEXEC_RUNDUMP
        printIndent(); printf("LEAVING VC FUNCTION `%s` (RETx); sp=%d\n", *func->GetFullName(), (int)(sp-pr_stack)); leaveIndent();
        #endif
        cstPop();
        return;

      PR_VM_CASE(OPC_ReturnL)
        checkSlow(sp == local_vars+func->NumLocals+1);
        ((VStack *)local_vars)[0] = sp[-1];
        pr_stackPtr = local_vars+1;
        while (ActiveIterators) {
          VScriptIterator *Tmp = ActiveIterators;
          ActiveIterators = Tmp->Next;
          delete Tmp;
          Tmp = nullptr;
        }
        #ifdef VMEXEC_RUNDUMP
        printIndent(); printf("LEAVING VC FUNCTION `%s` (RETL); sp=%d\n", *func->GetFullName(), (int)(sp-pr_stack)); leaveIndent();
        #endif
        cstPop();
        return;

      PR_VM_CASE(OPC_ReturnV)
        checkSlow(sp == local_vars+func->NumLocals+3);
        ((VStack *)local_vars)[0] = sp[-3];
        ((VStack *)local_vars)[1] = sp[-2];
        ((VStack *)local_vars)[2] = sp[-1];
        pr_stackPtr = local_vars+3;
        while (ActiveIterators) {
          VScriptIterator *Tmp = ActiveIterators;
          ActiveIterators = Tmp->Next;
          delete Tmp;
          Tmp = nullptr;
        }
        #ifdef VMEXEC_RUNDUMP
        printIndent(); printf("LEAVING VC FUNCTION `%s` (RETV); sp=%d\n", *func->GetFullName(), (int)(sp-pr_stack)); leaveIndent();
        #endif
        cstPop();
        return;

      PR_VM_CASE(OPC_GotoB)
        ip += ip[1];
        PR_VM_BREAK;

      PR_VM_CASE(OPC_GotoNB)
        ip -= ip[1];
        PR_VM_BREAK;

      PR_VM_CASE(OPC_GotoS)
        ip += ReadInt16(ip+1);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Goto)
        ip += ReadInt32(ip+1);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IfGotoB)
        if (sp[-1].i) ip += ip[1]; else ip += 2;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IfGotoNB)
        if (sp[-1].i) ip -= ip[1]; else ip += 2;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IfGotoS)
        if (sp[-1].i) ip += ReadInt16(ip+1); else ip += 3;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IfGoto)
        if (sp[-1].i) ip += ReadInt32(ip+1); else ip += 5;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IfNotGotoB)
        if (!sp[-1].i) ip += ip[1]; else ip += 2;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IfNotGotoNB)
        if (!sp[-1].i) ip -= ip[1]; else ip += 2;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IfNotGotoS)
        if (!sp[-1].i) ip += ReadInt16(ip+1); else ip += 3;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IfNotGoto)
        if (!sp[-1].i) ip += ReadInt32(ip+1); else ip += 5;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_CaseGotoB)
        if (ip[1] == sp[-1].i) {
          ip += ReadInt16(ip+2);
          --sp;
        } else {
          ip += 4;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_CaseGotoS)
        if (ReadInt16(ip+1) == sp[-1].i) {
          ip += ReadInt16(ip+3);
          --sp;
        } else {
          ip += 5;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_CaseGoto)
        if (ReadInt32(ip+1) == sp[-1].i) {
          ip += ReadInt16(ip+5);
          --sp;
        } else {
          ip += 7;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushNumber0)
        ++ip;
        sp->i = 0;
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushNumber1)
        ++ip;
        sp->i = 1;
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushNumberB)
        sp->i = ip[1];
        ip += 2;
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushNumberS)
        sp->i = ReadInt16(ip+1);
        ip += 3;
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushNumber)
        sp->i = ReadInt32(ip+1);
        ip += 5;
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushName)
        sp->i = ReadInt32(ip+1);
        ip += 5;
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushNameS)
        sp->i = ReadInt16(ip+1);
        ip += 3;
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushNameB)
        sp->i = ip[1];
        ip += 2;
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushString)
        sp->p = ReadPtr(ip+1);
        ip += 1+sizeof(void *);
        ++sp; {
          const char *S = (const char *)sp[-1].p;
          sp[-1].p = nullptr;
          *(VStr *)&sp[-1].p = S;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushClassId)
      PR_VM_CASE(OPC_PushState)
        sp->p = ReadPtr(ip+1);
        ip += 1+sizeof(void *);
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushNull)
        ++ip;
        sp->p = nullptr;
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalAddress0)
        ++ip;
        sp->p = &local_vars[0];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalAddress1)
        ++ip;
        sp->p = &local_vars[1];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalAddress2)
        ++ip;
        sp->p = &local_vars[2];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalAddress3)
        ++ip;
        sp->p = &local_vars[3];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalAddress4)
        ++ip;
        sp->p = &local_vars[4];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalAddress5)
        ++ip;
        sp->p = &local_vars[5];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalAddress6)
        ++ip;
        sp->p = &local_vars[6];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalAddress7)
        ++ip;
        sp->p = &local_vars[7];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalAddressB)
        sp->p = &local_vars[ip[1]];
        ip += 2;
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalAddressS)
        sp->p = &local_vars[ReadInt16(ip+1)];
        ip += 3;
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalAddress)
        sp->p = &local_vars[ReadInt32(ip+1)];
        ip += 5;
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalValue0)
        ++ip;
        *sp = local_vars[0];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalValue1)
        ++ip;
        *sp = local_vars[1];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalValue2)
        ++ip;
        *sp = local_vars[2];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalValue3)
        ++ip;
        *sp = local_vars[3];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalValue4)
        ++ip;
        *sp = local_vars[4];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalValue5)
        ++ip;
        *sp = local_vars[5];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalValue6)
        ++ip;
        *sp = local_vars[6];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalValue7)
        ++ip;
        *sp = local_vars[7];
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LocalValueB)
        *sp = local_vars[ip[1]];
        ip += 2;
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VLocalValueB)
        sp[0].f = ((TVec *)&local_vars[ip[1]])->x;
        sp[1].f = ((TVec *)&local_vars[ip[1]])->y;
        sp[2].f = ((TVec *)&local_vars[ip[1]])->z;
        ip += 2;
        sp += 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_StrLocalValueB)
        sp->p = nullptr;
        *(VStr *)&sp->p = *(VStr *)&local_vars[ip[1]];
        ip += 2;
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Offset)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].p = (vuint8 *)sp[-1].p+ReadInt32(ip+1);
        ip += 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_OffsetS)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].p = (vuint8 *)sp[-1].p+ReadInt16(ip+1);
        ip += 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_OffsetB)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].p = (vuint8 *)sp[-1].p+ip[1];
        ip += 2;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FieldValue)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = *(vint32 *)((vuint8 *)sp[-1].p+ReadInt32(ip+1));
        ip += 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FieldValueS)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = *(vint32 *)((vuint8 *)sp[-1].p+ReadInt16(ip+1));
        ip += 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FieldValueB)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = *(vint32 *)((vuint8 *)sp[-1].p+ip[1]);
        ip += 2;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VFieldValue)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        {
          TVec *vp = (TVec *)((vuint8 *)sp[-1].p+ReadInt32(ip+1));
          sp[1].f = vp->z;
          sp[0].f = vp->y;
          sp[-1].f = vp->x;
        }
        sp += 2;
        ip += 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VFieldValueS)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        {
          TVec *vp = (TVec *)((vuint8 *)sp[-1].p+ReadInt16(ip+1));
          sp[1].f = vp->z;
          sp[0].f = vp->y;
          sp[-1].f = vp->x;
        }
        sp += 2;
        ip += 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VFieldValueB)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        {
          TVec *vp = (TVec *)((vuint8 *)sp[-1].p+ip[1]);
          sp[1].f = vp->z;
          sp[0].f = vp->y;
          sp[-1].f = vp->x;
        }
        sp += 2;
        ip += 2;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PtrFieldValue)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].p = *(void **)((vuint8 *)sp[-1].p+ReadInt32(ip+1));
        ip += 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PtrFieldValueS)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].p = *(void **)((vuint8 *)sp[-1].p+ReadInt16(ip+1));
        ip += 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PtrFieldValueB)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].p = *(void **)((vuint8 *)sp[-1].p+ip[1]);
        ip += 2;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_StrFieldValue)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        {
          VStr *Ptr = (VStr *)((vuint8 *)sp[-1].p+ReadInt32(ip+1));
          sp[-1].p = nullptr;
          *(VStr *)&sp[-1].p = *Ptr;
        }
        ip += 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_StrFieldValueS)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        {
          VStr *Ptr = (VStr *)((vuint8 *)sp[-1].p+ReadInt16(ip+1));
          sp[-1].p = nullptr;
          *(VStr *)&sp[-1].p = *Ptr;
        }
        ip += 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_StrFieldValueB)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        {
          VStr *Ptr = (VStr *)((vuint8 *)sp[-1].p+ip[1]);
          sp[-1].p = nullptr;
          *(VStr *)&sp[-1].p = *Ptr;
        }
        ip += 2;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_SliceFieldValue)
        if (sp[-1].p) {
          vint32 ofs = ReadInt32(ip+1);
          VCSlice vs = *(VCSlice *)((vuint8 *)sp[-1].p+ofs);
          if (!vs.ptr) vs.length = 0; else if (vs.length < 0) vs.length = 0; // just in case
          sp[-1].p = vs.ptr;
          sp[0].i = vs.length;
          ++sp;
        } else {
          cstDump(ip); Sys_Error("Reference not set to an instance of an object");
        }
        ip += 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteFieldValue)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = *((vuint8 *)sp[-1].p+ReadInt32(ip+1));
        ip += 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteFieldValueS)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = *((vuint8 *)sp[-1].p+ReadInt16(ip+1));
        ip += 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteFieldValueB)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = *((vuint8 *)sp[-1].p+ip[1]);
        ip += 2;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Bool0FieldValue)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = !!(*(vint32 *)((vuint8 *)sp[-1].p+ReadInt32(ip+1))&ip[5]);
        ip += 6;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Bool0FieldValueS)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = !!(*(vint32 *)((vuint8 *)sp[-1].p+ReadInt16(ip+1))&ip[3]);
        ip += 4;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Bool0FieldValueB)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = !!(*(vint32 *)((vuint8 *)sp[-1].p+ip[1])&ip[2]);
        ip += 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Bool1FieldValue)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = !!(*(vint32 *)((vuint8 *)sp[-1].p+ReadInt32(ip+1))&(ip[5]<<8));
        ip += 6;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Bool1FieldValueS)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = !!(*(vint32 *)((vuint8 *)sp[-1].p+ReadInt16(ip+1))&(ip[3]<<8));
        ip += 4;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Bool1FieldValueB)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = !!(*(vint32 *)((vuint8 *)sp[-1].p+ip[1])&(ip[2]<<8));
        ip += 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Bool2FieldValue)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = !!(*(vint32 *)((vuint8 *)sp[-1].p+ReadInt32(ip+1))&(ip[5]<<16));
        ip += 6;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Bool2FieldValueS)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = !!(*(vint32 *)((vuint8 *)sp[-1].p+ReadInt16(ip+1))&(ip[3]<<16));
        ip += 4;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Bool2FieldValueB)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = !!(*(vint32 *)((vuint8 *)sp[-1].p+ip[1])&(ip[2]<<16));
        ip += 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Bool3FieldValue)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = !!(*(vint32 *)((vuint8 *)sp[-1].p+ReadInt32(ip+1))&(ip[5]<<24));
        ip += 6;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Bool3FieldValueS)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = !!(*(vint32 *)((vuint8 *)sp[-1].p+ReadInt16(ip+1))&(ip[3]<<24));
        ip += 4;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Bool3FieldValueB)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].i = !!(*(vint32 *)((vuint8 *)sp[-1].p+ip[1])&(ip[2]<<24));
        ip += 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ArrayElement)
        sp[-2].p = (vuint8 *)sp[-2].p+sp[-1].i*ReadInt32(ip+1);
        ip += 5;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ArrayElementS)
        sp[-2].p = (vuint8 *)sp[-2].p+sp[-1].i*ReadInt16(ip+1);
        ip += 3;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ArrayElementB)
        sp[-2].p = (vuint8 *)sp[-2].p+sp[-1].i*ip[1];
        ip += 2;
        --sp;
        PR_VM_BREAK;

      // [-1]: index
      // [-2]: ptr to VCSlice
      PR_VM_CASE(OPC_SliceElement)
        if (sp[-2].p) {
          int idx = sp[-1].i;
          VCSlice vs = *(VCSlice *)sp[-2].p;
          if (!vs.ptr) vs.length = 0; else if (vs.length < 0) vs.length = 0; // just in case
          if (idx < 0 || idx >= vs.length) { cstDump(ip); Sys_Error("Slice index %d is out of range (%d)", idx, vs.length); }
          sp[-2].p = vs.ptr+idx*ReadInt32(ip+1);
        } else {
          cstDump(ip);
          Sys_Error("Slice index %d is out of range (0)", sp[-1].i);
        }
        ip += 5;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_OffsetPtr)
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Cannot offset null pointer"); }
        sp[-1].p = (vuint8 *)sp[-1].p+ReadInt32(ip+1);
        ip += 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushPointed)
        ++ip;
        sp[-1].i = *(vint32 *)sp[-1].p;
        PR_VM_BREAK;

      // [-1]: ptr to VCSlice
      PR_VM_CASE(OPC_PushPointedSlice)
        if (sp[-1].p) {
          VCSlice vs = *(VCSlice *)sp[-1].p;
          if (!vs.ptr) vs.length = 0; else if (vs.length < 0) vs.length = 0; // just in case
          sp[-1].p = vs.ptr;
          sp[0].i = vs.length;
        } else {
          sp[-1].p = nullptr;
          sp[0].i = 0;
        }
        ip += 5;
        ++sp;
        PR_VM_BREAK;

      // [-1]: ptr to VCSlice
      PR_VM_CASE(OPC_PushPointedSliceLen)
        ++ip;
        if (sp[-1].p) {
          VCSlice vs = *(VCSlice *)sp[-1].p;
          if (!vs.ptr) vs.length = 0; else if (vs.length < 0) vs.length = 0; // just in case
          sp[-1].i = vs.length;
        } else {
          sp[-1].i = 0;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VPushPointed)
        ++ip;
        sp[1].f = ((TVec *)sp[-1].p)->z;
        sp[0].f = ((TVec *)sp[-1].p)->y;
        sp[-1].f = ((TVec *)sp[-1].p)->x;
        sp += 2;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushPointedPtr)
        ++ip;
        sp[-1].p = *(void **)sp[-1].p;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushPointedByte)
        ++ip;
        sp[-1].i = *(vuint8 *)sp[-1].p;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushBool0)
        {
          vuint32 mask = ip[1];
          ip += 2;
          sp[-1].i = !!(*(vint32 *)sp[-1].p&mask);
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushBool1)
        {
          vuint32 mask = ip[1]<<8;
          ip += 2;
          sp[-1].i = !!(*(vint32 *)sp[-1].p&mask);
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushBool2)
        {
          vuint32 mask = ip[1]<<16;
          ip += 2;
          sp[-1].i = !!(*(vint32 *)sp[-1].p&mask);
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushBool3)
        {
          vuint32 mask = ip[1]<<24;
          ip += 2;
          sp[-1].i = !!(*(vint32 *)sp[-1].p&mask);
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushPointedStr)
        {
          ++ip;
          VStr *Ptr = (VStr *)sp[-1].p;
          sp[-1].p = nullptr;
          *(VStr *)&sp[-1].p = *Ptr;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushPointedDelegate)
        ++ip;
        sp[0].p = ((void **)sp[-1].p)[1];
        sp[-1].p = ((void **)sp[-1].p)[0];
        ++sp;
        PR_VM_BREAK;

    #define BINOP(mem, op) \
      ++ip; \
      sp[-2].mem = sp[-2].mem op sp[-1].mem; \
      --sp;
    #define BINOP_Q(mem, op) \
      ++ip; \
      sp[-2].mem op sp[-1].mem; \
      --sp;

      PR_VM_CASE(OPC_Add)
        BINOP_Q(i, +=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Subtract)
        BINOP_Q(i, -=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Multiply)
        BINOP_Q(i, *=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Divide)
        if (!sp[-1].i) { cstDump(ip); Sys_Error("Division by 0"); }
        BINOP_Q(i, /=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Modulus)
        if (!sp[-1].i) { cstDump(ip); Sys_Error("Division by 0"); }
        BINOP_Q(i, %=);
        PR_VM_BREAK;

    #define BOOLOP(mem, op) \
      ++ip; \
      sp[-2].i = sp[-2].mem op sp[-1].mem; \
      --sp;

      PR_VM_CASE(OPC_Equals)
        BOOLOP(i, ==);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_NotEquals)
        BOOLOP(i, !=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Less)
        BOOLOP(i, <);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Greater)
        BOOLOP(i, >);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LessEquals)
        BOOLOP(i, <=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_GreaterEquals)
        BOOLOP(i, >=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_NegateLogical)
        ++ip;
        sp[-1].i = !sp[-1].i;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_AndBitwise)
        BINOP_Q(i, &=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_OrBitwise)
        BINOP_Q(i, |=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_XOrBitwise)
        BINOP_Q(i, ^=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LShift)
        BINOP_Q(i, <<=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_RShift)
        BINOP_Q(i, >>=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_UnaryMinus)
        ++ip;
        sp[-1].i = -sp[-1].i;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_BitInverse)
        ++ip;
        sp[-1].i = ~sp[-1].i;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PreInc)
        ++ip;
        {
          vint32 *ptr = (vint32 *)sp[-1].p;
          ++(*ptr);
          sp[-1].i = *ptr;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PreDec)
        ++ip;
        {
          vint32 *ptr = (vint32 *)sp[-1].p;
          --(*ptr);
          sp[-1].i = *ptr;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PostInc)
        ++ip;
        {
          vint32 *ptr = (vint32 *)sp[-1].p;
          sp[-1].i = *ptr;
          (*ptr)++;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PostDec)
        ++ip;
        {
          vint32 *ptr = (vint32 *)sp[-1].p;
          sp[-1].i = *ptr;
          (*ptr)--;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IncDrop)
        ++ip;
        ++(*(vint32 *)sp[-1].p);
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DecDrop)
        ++ip;
        --(*(vint32 *)sp[-1].p);
        --sp;
        PR_VM_BREAK;

    #define ASSIGNOP(type, mem, op) \
      ++ip; \
      *(type *)sp[-2].p op sp[-1].mem; \
      sp -= 2;

      PR_VM_CASE(OPC_AssignDrop)
        ASSIGNOP(vint32, i, =);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_AddVarDrop)
        ASSIGNOP(vint32, i, +=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_SubVarDrop)
        ASSIGNOP(vint32, i, -=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_MulVarDrop)
        ASSIGNOP(vint32, i, *=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DivVarDrop)
        if (!sp[-1].i) { cstDump(ip); Sys_Error("Division by 0"); }
        ASSIGNOP(vint32, i, /=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ModVarDrop)
        if (!sp[-1].i) { cstDump(ip); Sys_Error("Division by 0"); }
        ASSIGNOP(vint32, i, %=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_AndVarDrop)
        ASSIGNOP(vint32, i, &=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_OrVarDrop)
        ASSIGNOP(vint32, i, |=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_XOrVarDrop)
        ASSIGNOP(vint32, i, ^=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LShiftVarDrop)
        ASSIGNOP(vint32, i, <<=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_RShiftVarDrop)
        ASSIGNOP(vint32, i, >>=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_BytePreInc)
        ++ip;
        {
          vuint8 *ptr = (vuint8 *)sp[-1].p;
          ++(*ptr);
          sp[-1].i = *ptr;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_BytePreDec)
        ++ip;
        {
          vuint8 *ptr = (vuint8 *)sp[-1].p;
          --(*ptr);
          sp[-1].i = *ptr;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_BytePostInc)
        ++ip;
        {
          vuint8 *ptr = (vuint8 *)sp[-1].p;
          sp[-1].i = *ptr;
          (*ptr)++;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_BytePostDec)
        ++ip;
        {
          vuint8 *ptr = (vuint8 *)sp[-1].p;
          sp[-1].i = *ptr;
          (*ptr)--;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteIncDrop)
        ++ip;
        (*(vuint8 *)sp[-1].p)++;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteDecDrop)
        ++ip;
        (*(vuint8 *)sp[-1].p)--;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteAssignDrop)
        ASSIGNOP(vuint8, i, =);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteAddVarDrop)
        ASSIGNOP(vuint8, i, +=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteSubVarDrop)
        ASSIGNOP(vuint8, i, -=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteMulVarDrop)
        ASSIGNOP(vuint8, i, *=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteDivVarDrop)
        if (!sp[-1].i) { cstDump(ip); Sys_Error("Division by 0"); }
        ASSIGNOP(vuint8, i, /=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteModVarDrop)
        if (!sp[-1].i) { cstDump(ip); Sys_Error("Division by 0"); }
        ASSIGNOP(vuint8, i, %=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteAndVarDrop)
        ASSIGNOP(vuint8, i, &=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteOrVarDrop)
        ASSIGNOP(vuint8, i, |=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteXOrVarDrop)
        ASSIGNOP(vuint8, i, ^=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteLShiftVarDrop)
        ASSIGNOP(vuint8, i, <<=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteRShiftVarDrop)
        ASSIGNOP(vuint8, i, >>=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FAdd)
        BINOP_Q(f, +=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FSubtract)
        BINOP_Q(f, -=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FMultiply)
        BINOP_Q(f, *=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FDivide)
        if (!sp[-1].f) { cstDump(ip); Sys_Error("Division by 0"); }
        BINOP_Q(f, /=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FEquals)
        BOOLOP(f, ==);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FNotEquals)
        BOOLOP(f, !=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FLess)
        BOOLOP(f, <);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FGreater)
        BOOLOP(f, >);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FLessEquals)
        BOOLOP(f, <=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FGreaterEquals)
        BOOLOP(f, >=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FUnaryMinus)
        ++ip;
        sp[-1].f = -sp[-1].f;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FAddVarDrop)
        ASSIGNOP(float, f, +=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FSubVarDrop)
        ASSIGNOP(float, f, -=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FMulVarDrop)
        ASSIGNOP(float, f, *=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FDivVarDrop)
        ASSIGNOP(float, f, /=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VAdd)
        ++ip;
        sp[-6].f += sp[-3].f;
        sp[-5].f += sp[-2].f;
        sp[-4].f += sp[-1].f;
        sp -= 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VSubtract)
        ++ip;
        sp[-6].f -= sp[-3].f;
        sp[-5].f -= sp[-2].f;
        sp[-4].f -= sp[-1].f;
        sp -= 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VPreScale)
        {
          ++ip;
          float scale = sp[-4].f;
          sp[-4].f = scale*sp[-3].f;
          sp[-3].f = scale*sp[-2].f;
          sp[-2].f = scale*sp[-1].f;
          --sp;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VPostScale)
        ++ip;
        sp[-4].f *= sp[-1].f;
        sp[-3].f *= sp[-1].f;
        sp[-2].f *= sp[-1].f;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VIScale)
        ++ip;
        sp[-4].f /= sp[-1].f;
        sp[-3].f /= sp[-1].f;
        sp[-2].f /= sp[-1].f;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VEquals)
        ++ip;
        sp[-6].i = (sp[-6].f == sp[-3].f && sp[-5].f == sp[-2].f && sp[-4].f == sp[-1].f);
        sp -= 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VNotEquals)
        ++ip;
        sp[-6].i = (sp[-6].f != sp[-3].f || sp[-5].f != sp[-2].f || sp[-4].f != sp[-1].f);
        sp -= 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VUnaryMinus)
        ++ip;
        sp[-3].f = -sp[-3].f;
        sp[-2].f = -sp[-2].f;
        sp[-1].f = -sp[-1].f;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VFixParam)
        {
          vint32 Idx = ip[1];
          ip += 2;
          TVec *v = (TVec *)&local_vars[Idx];
          v->y = local_vars[Idx+1].f;
          v->z = local_vars[Idx+2].f;
        }
        PR_VM_BREAK;

    #define VASSIGNOP(op) \
      { \
        ++ip; \
        TVec *ptr = (TVec *)sp[-4].p; \
        ptr->x op sp[-3].f; \
        ptr->y op sp[-2].f; \
        ptr->z op sp[-1].f; \
        sp -= 4; \
      }

      PR_VM_CASE(OPC_VAssignDrop)
        VASSIGNOP(=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VAddVarDrop)
        VASSIGNOP(+=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VSubVarDrop)
        VASSIGNOP(-=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VScaleVarDrop)
        ++ip;
        *(TVec *)sp[-2].p *= sp[-1].f;
        sp -= 2;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VIScaleVarDrop)
        ++ip;
        *(TVec *)sp[-2].p /= sp[-1].f;
        sp -= 2;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_StrToBool)
        {
          ++ip;
          bool Val = ((VStr *)&sp[-1].p)->IsNotEmpty();
          ((VStr *)&sp[-1].p)->Clean();
          sp[-1].i = Val;
        }
        PR_VM_BREAK;

    #define STRCMPOP(cmpop) \
      { \
        ++ip; \
        int cmp = ((VStr *)&sp[-2].p)->Cmp(*((VStr *)&sp[-1].p)); \
        ((VStr *)&sp[-2].p)->Clean(); \
        ((VStr *)&sp[-1].p)->Clean(); \
        sp -= 1; \
        sp[-1].i = (cmp cmpop 0); \
      }

      PR_VM_CASE(OPC_StrEquals)
        //STRCMPOP(==)
        // the following is slightly faster for the same strings
        {
          ++ip;
          bool cmp = *((VStr *)&sp[-2].p) == *((VStr *)&sp[-1].p);
          ((VStr *)&sp[-2].p)->Clean();
          ((VStr *)&sp[-1].p)->Clean();
          sp -= 1;
          sp[-1].i = int(cmp);
        }
        PR_VM_BREAK;
      PR_VM_CASE(OPC_StrNotEquals)
        //STRCMPOP(!=)
        // the following is slightly faster for the same strings
        {
          ++ip;
          bool cmp = *((VStr *)&sp[-2].p) != *((VStr *)&sp[-1].p);
          ((VStr *)&sp[-2].p)->Clean();
          ((VStr *)&sp[-1].p)->Clean();
          sp -= 1;
          sp[-1].i = int(cmp);
        }
        PR_VM_BREAK;
      PR_VM_CASE(OPC_StrLess)
        STRCMPOP(<)
        PR_VM_BREAK;
      PR_VM_CASE(OPC_StrLessEqu)
        STRCMPOP(<=)
        PR_VM_BREAK;
      PR_VM_CASE(OPC_StrGreat)
        STRCMPOP(>)
        PR_VM_BREAK;
      PR_VM_CASE(OPC_StrGreatEqu)
        STRCMPOP(>=)
        PR_VM_BREAK;

      PR_VM_CASE(OPC_StrLength)
        {
          ++ip;
          auto len = (*((VStr **)&sp[-1].p))->Length();
          //((VStr *)&sp[-1].p)->Clean();
          sp[-1].i = (int)len;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_StrCat)
        {
          ++ip;
          VStr s = *((VStr *)&sp[-2].p)+*((VStr *)&sp[-1].p);
          ((VStr *)&sp[-2].p)->Clean();
          ((VStr *)&sp[-1].p)->Clean();
          sp -= 1;
          *((VStr *)&sp[-1].p) = s;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_StrGetChar)
        {
          ++ip;
          int v = sp[-1].i;
          v = (v >= 0 && (size_t)v < ((VStr *)&sp[-2].p)->length() ? (vuint8)(*((VStr *)&sp[-2].p))[v] : 0);
          ((VStr *)&sp[-2].p)->Clean();
          sp -= 1;
          sp[-1].i = v;
        }
        PR_VM_BREAK;

      // [-1]: char
      // [-2]: index
      // [-3]: *string
      PR_VM_CASE(OPC_StrSetChar)
        {
          ++ip;
          vuint8 ch = sp[-1].i&0xff;
          int idx = sp[-2].i;
          VStr *s = *(VStr **)&sp[-3].p;
          if (idx >= 0 && idx < (int)(s->length())) {
            char *data = s->GetMutableCharPointer(idx);
            *data = ch;
          }
          sp -= 3; // drop it all
        }
        PR_VM_BREAK;

      // [-1]: hi
      // [-2]: lo
      // [-3]: string
      PR_VM_CASE(OPC_StrSlice)
        {
          ++ip;
          int hi = sp[-1].i;
          int lo = sp[-2].i;
          VStr *s = (VStr *)&sp[-3].p;
          sp -= 2; // drop limits
          if (lo < 0 || hi <= lo || lo >= (int)s->length()) {
            s->clear();
          } else {
            if (hi > (int)s->length()) hi = (int)s->length();
            VStr ns = s->mid(lo, hi-lo);
            s->clear();
            *s = ns;
          }
        }
        PR_VM_BREAK;

      // [-1]: newstr
      // [-2]: hi
      // [-3]: lo
      // [-4]: *string
      // res: string
      PR_VM_CASE(OPC_StrSliceAssign)
        {
          ++ip;
          int hi = sp[-2].i;
          int lo = sp[-3].i;
          VStr ns = *(VStr *)&sp[-1].p;
          VStr *s = *(VStr **)&sp[-4].p;
          sp -= 4; // drop everything
          if (lo < 0 || hi <= lo || lo >= (int)s->length()) {
            // do nothing
          } else {
            if (hi > (int)s->length()) hi = (int)s->length();
            // get left part
            VStr ds = (lo > 0 ? s->mid(0, lo) : VStr());
            // append middle part
            ds += ns;
            // append right part
            if (hi < (int)s->length()) ds += s->mid(hi, (int)s->length()-hi);
            s->clear();
            *s = ds;
          }
        }
        PR_VM_BREAK;

      // [-1]: hi
      // [-2]: lo
      // [-3]: sliceptr
      // res: slice (ptr, len)
      PR_VM_CASE(OPC_SliceSlice)
        if (sp[-3].p) {
          int hi = sp[-1].i;
          int lo = sp[-2].i;
          VCSlice vs = *(VCSlice *)sp[-3].p;
          if (!vs.ptr) vs.length = 0; else if (vs.length < 0) vs.length = 0; // just in case
          if (lo < 0 || hi < lo || lo > vs.length || hi > vs.length) {
            cstDump(ip);
            Sys_Error("Slice [%d..%d] is out of range (%d)", lo, hi, vs.length);
          } else {
            sp -= 1; // drop one unused limit
            // push slice
            sp[-2].p = vs.ptr+lo*ReadInt32(ip+1);
            sp[-1].i = hi-lo;
          }
        } else {
          cstDump(ip); Sys_Error("Cannot operate on none-slice");
        }
        ip += 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_AssignStrDrop)
        ++ip;
        *(VStr *)sp[-2].p = *(VStr *)&sp[-1].p;
        ((VStr *)&sp[-1].p)->Clean();
        sp -= 2;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PtrEquals)
        BOOLOP(p, ==);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PtrNotEquals)
        BOOLOP(p, !=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PtrToBool)
        ++ip;
        sp[-1].i = !!sp[-1].p;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IntToFloat)
        ++ip;
        ftemp = (float)sp[-1].i;
        sp[-1].f = ftemp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FloatToInt)
        ++ip;
        itemp = (vint32)sp[-1].f;
        sp[-1].i = itemp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_StrToName)
        {
          ++ip;
          VName newname = VName(**((VStr *)&sp[-1].p));
          ((VStr *)&sp[-1].p)->Clean();
          sp[-1].i = newname.GetIndex();
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_NameToStr)
        {
          ++ip;
          VName n = VName((EName)sp[-1].i);
          sp[-1].p = nullptr;
          *(VStr *)&sp[-1].p = VStr(*n);
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IntAbs)
        ++ip;
        if (sp[-1].i < 0) sp[-1].i = -sp[-1].i;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FloatAbs)
        ++ip;
        if (sp[-1].f < 0) sp[-1].f = -sp[-1].f;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IntMin)
        ++ip;
        if (sp[-2].i > sp[-1].i) sp[-2].i = sp[-1].i;
        sp -= 1;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IntMax)
        ++ip;
        if (sp[-2].i < sp[-1].i) sp[-2].i = sp[-1].i;
        sp -= 1;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FloatMin)
        ++ip;
        if (sp[-2].f > sp[-1].f) sp[-2].f = sp[-1].f;
        sp -= 1;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FloatMax)
        ++ip;
        if (sp[-2].f < sp[-1].f) sp[-2].f = sp[-1].f;
        sp -= 1;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IntClamp)
        ++ip;
        /*
        #define MID(min, val, max)  MAX(min, MIN(val, max))
          Max  -1
          Min  -2
          Val  -3
        */
        sp[-3].i = MID(sp[-2].i, sp[-3].i, sp[-1].i);
        sp -= 2;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FloatClamp)
        ++ip;
        sp[-3].f = MID(sp[-2].f, sp[-3].f, sp[-1].f);
        sp -= 2;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ClearPointedStr)
        ((VStr *)sp[-1].p)->Clean();
        ip += 1;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ClearPointedStruct)
        ((VStruct *)ReadPtr(ip+1))->DestructObject((byte *)sp[-1].p);
        ip += 1+sizeof(void *);
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Drop)
        ++ip;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VDrop)
        ++ip;
        sp -= 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DropStr)
        ++ip;
        ((VStr *)&sp[-1].p)->Clean();
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_AssignPtrDrop)
        ASSIGNOP(void*, p, =);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_AssignBool0)
        {
          vuint32 mask = ip[1];
          if (sp[-1].i) {
            *(vint32 *)sp[-2].p |= mask;
          } else {
            *(vint32 *)sp[-2].p &= ~mask;
          }
          ip += 2;
          sp -= 2;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_AssignBool1)
        {
          vuint32 mask = ip[1]<<8;
          if (sp[-1].i) {
            *(vint32 *)sp[-2].p |= mask;
          } else {
            *(vint32 *)sp[-2].p &= ~mask;
          }
          ip += 2;
          sp -= 2;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_AssignBool2)
        {
          vuint32 mask = ip[1]<<16;
          if (sp[-1].i) {
            *(vint32 *)sp[-2].p |= mask;
          } else {
            *(vint32 *)sp[-2].p &= ~mask;
          }
          ip += 2;
          sp -= 2;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_AssignBool3)
        {
          vuint32 mask = ip[1]<<24;
          if (sp[-1].i) {
            *(vint32 *)sp[-2].p |= mask;
          } else {
            *(vint32 *)sp[-2].p &= ~mask;
          }
          ip += 2;
          sp -= 2;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_AssignDelegate)
        ++ip;
        ((void **)sp[-3].p)[0] = sp[-2].p;
        ((void **)sp[-3].p)[1] = sp[-1].p;
        sp -= 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DynArrayElement)
        if (sp[-1].i < 0 || sp[-1].i >= ((VScriptArray *)sp[-2].p)->Num()) { cstDump(ip); Sys_Error("Index outside the bounds of an array"); }
        sp[-2].p = ((VScriptArray *)sp[-2].p)->Ptr()+sp[-1].i*ReadInt32(ip+1);
        ip += 5;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DynArrayElementS)
        if (sp[-1].i < 0 || sp[-1].i >= ((VScriptArray *)sp[-2].p)->Num()) { cstDump(ip); Sys_Error("Index outside the bounds of an array"); }
        sp[-2].p = ((VScriptArray *)sp[-2].p)->Ptr()+sp[-1].i*ReadInt16(ip+1);
        ip += 3;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DynArrayElementB)
        if (sp[-1].i < 0 || sp[-1].i >= ((VScriptArray *)sp[-2].p)->Num()) { cstDump(ip); Sys_Error("Index outside the bounds of an array"); }
        sp[-2].p = ((VScriptArray *)sp[-2].p)->Ptr()+sp[-1].i*ip[1];
        ip += 2;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DynArrayElementGrow)
        {
          VFieldType Type;
          ++ip;
          ReadType(Type, ip);
          //ip += 9+sizeof(VClass *);
          if (sp[-1].i < 0) { cstDump(ip); Sys_Error("Array index is negative"); }
          VScriptArray &A = *(VScriptArray *)sp[-2].p;
          if (sp[-1].i >= A.Num()) A.SetNum(sp[-1].i+1, Type);
          sp[-2].p = A.Ptr()+sp[-1].i*Type.GetSize();
          --sp;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DynArrayGetNum)
        ++ip;
        sp[-1].i = ((VScriptArray *)sp[-1].p)->Num();
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DynArraySetNum)
        {
          VFieldType Type;
          ++ip;
          ReadType(Type, ip);
          //ip += 9+sizeof(VClass *);
          ((VScriptArray *)sp[-2].p)->SetNum(sp[-1].i, Type);
          sp -= 2;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DynArraySetNumMinus)
        {
          VFieldType Type;
          ++ip;
          ReadType(Type, ip);
          //ip += 9+sizeof(VClass *);
          ((VScriptArray *)sp[-2].p)->SetNumMinus(sp[-1].i, Type);
          sp -= 2;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DynArraySetNumPlus)
        {
          VFieldType Type;
          ++ip;
          ReadType(Type, ip);
          //ip += 9+sizeof(VClass *);
          ((VScriptArray *)sp[-2].p)->SetNumPlus(sp[-1].i, Type);
          sp -= 2;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DynArrayInsert)
        {
          VFieldType Type;
          ++ip;
          ReadType(Type, ip);
          //ip += 9+sizeof(VClass *);
          ((VScriptArray *)sp[-3].p)->Insert(sp[-2].i, sp[-1].i, Type);
          sp -= 3;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DynArrayRemove)
        {
          VFieldType Type;
          ++ip;
          ReadType(Type, ip);
          //ip += 9+sizeof(VClass *);
          ((VScriptArray *)sp[-3].p)->Remove(sp[-2].i, sp[-1].i, Type);
          sp -= 3;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DynamicCast)
        sp[-1].p = sp[-1].p && ((VObject *)sp[-1].p)->IsA(
          (VClass *)ReadPtr(ip+1)) ? sp[-1].p : 0;
        ip += 1+sizeof(void *);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DynamicClassCast)
        sp[-1].p = sp[-1].p && ((VClass *)sp[-1].p)->IsChildOf(
          (VClass *)ReadPtr(ip+1)) ? sp[-1].p : 0;
        ip += 1+sizeof(void *);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_GetDefaultObj)
        ++ip;
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].p = ((VObject *)sp[-1].p)->GetClass()->Defaults;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_GetClassDefaultObj)
        ++ip;
        if (!sp[-1].p) { cstDump(ip); Sys_Error("Reference not set to an instance of an object"); }
        sp[-1].p = ((VClass *)sp[-1].p)->Defaults;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IteratorInit)
        ++ip;
        ((VScriptIterator *)sp[-1].p)->Next = ActiveIterators;
        ActiveIterators = (VScriptIterator *)sp[-1].p;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IteratorNext)
        ++ip;
        checkSlow(ActiveIterators);
        sp->i = ActiveIterators->GetNext();
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IteratorPop)
        {
          ++ip;
          checkSlow(ActiveIterators);
          VScriptIterator *Temp = ActiveIterators;
          ActiveIterators = Temp->Next;
          delete Temp;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DoWriteOne)
        {
          VFieldType Type;
          ++ip;
          ReadType(Type, ip);
          //ip += 9+sizeof(VClass *);
          pr_stackPtr = sp;
          PR_WriteOne(Type); // this will pop everything
          if (pr_stackPtr < pr_stack) { pr_stackPtr = pr_stack; cstDump(ip); Sys_Error("Stack underflow in `write`"); }
          sp = pr_stackPtr;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DoWriteFlush)
        ++ip;
        PR_WriteFlush();
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DoPushTypePtr)
        ++ip;
        sp->p = ip;
        sp += 1;
        // skip type
        //VFieldType Type;
        //ReadType(Type, ip);
        ip += 8+sizeof(VClass *);
        PR_VM_BREAK;

      PR_VM_DEFAULT
        cstDump(ip);
        Sys_Error("Invalid opcode %d", *ip);
    }
  }

  goto func_loop;

  unguardf(("(%s %d)", *func->GetFullName(), (int)(ip-func->Statements.Ptr())));
}


//==========================================================================
//
//  VObject::ExecuteFunction
//
//==========================================================================
VStack VObject::ExecuteFunction (VMethod *func) {
  guard(VObject::ExecuteFunction);

  VMethod *prev_func;
  VStack ret;

  ret.i = 0;
  ret.p = nullptr;
  // run function
  prev_func = current_func;
#ifdef VMEXEC_RUNDUMP
  enterIndent(); printIndent(); printf("***ENTERING `%s` (RETx); sp=%d (MAX:%u)\n", *func->GetFullName(), (int)(pr_stackPtr-pr_stack), (unsigned)MAX_PROG_STACK);
#endif
  RunFunction(func);
  current_func = prev_func;

  // get return value
  if (func->ReturnType.Type) {
    --pr_stackPtr;
    ret = *pr_stackPtr;
  }
#ifdef VMEXEC_RUNDUMP
  printIndent(); printf("***LEAVING `%s` (RETx); sp=%d, (MAX:%u)\n", *func->GetFullName(), (int)(pr_stackPtr-pr_stack), (unsigned)MAX_PROG_STACK); leaveIndent();
#endif

#ifdef CHECK_FOR_EMPTY_STACK
  // after executing base function stack must be empty
  if (!current_func && pr_stackPtr != pr_stack+1) {
    cstDump(nullptr);
    Sys_Error("ExecuteFunction: Stack is not empty after executing function:\n%s\nstack = %p, oldsp = %p", *func->Name, pr_stack, pr_stackPtr);
    #ifdef VMEXEC_RUNDUMP
    *(int *)0 = 0;
    #endif
  }
#endif

#ifdef CHECK_STACK_UNDERFLOW
  // check if stack wasn't underflowed
  if (pr_stack[0].i != STACK_ID) {
    cstDump(nullptr);
    Sys_Error("ExecuteFunction: Stack underflow in %s", *func->Name);
    #ifdef VMEXEC_RUNDUMP
    *(int *)0 = 0;
    #endif
  }
#endif

#ifdef CHECK_STACK_OVERFLOW
  // check if stack wasn't overflowed
  if (pr_stack[MAX_PROG_STACK-1].i != STACK_ID) {
    cstDump(nullptr);
    Sys_Error("ExecuteFunction: Stack overflow in %s", *func->Name);
    #ifdef VMEXEC_RUNDUMP
    *(int *)0 = 0;
    #endif
  }
#endif

  // all done
  return ret;
  unguardf(("(%s)", *func->GetFullName()));
}


//==========================================================================
//
//  VObject::VMDumpCallStack
//
//==========================================================================
void VObject::VMDumpCallStack () {
  cstDump(nullptr);
}


//==========================================================================
//
//  VObject::DumpProfile
//
//==========================================================================
void VObject::DumpProfile () {
  //#define MAX_PROF  100
  const int MAX_PROF = 100;
  int profsort[MAX_PROF];
  int totalcount = 0;
  memset(profsort, 0, sizeof(profsort));
  for (int i = 0; i < VMemberBase::GMembers.Num(); ++i) {
    if (VMemberBase::GMembers[i]->MemberType != MEMBER_Method) continue;
    VMethod *Func = (VMethod *)VMemberBase::GMembers[i];
    if (!Func->Profile1) continue; // never called
    for (int j = 0; j < MAX_PROF; ++j) {
      totalcount += Func->Profile2;
      if (((VMethod *)VMemberBase::GMembers[profsort[j]])->Profile2 <= Func->Profile2) {
        for (int k = MAX_PROF-1; k > j; --k) profsort[k] = profsort[k-1];
        profsort[j] = i;
        break;
      }
    }
  }
  if (!totalcount) return;
  for (int i = 0; i < MAX_PROF && profsort[i]; ++i) {
    VMethod *Func = (VMethod *)VMemberBase::GMembers[profsort[i]];
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
    GCon->Logf("%3.2f%% (%9d) %9d %s",
      (double)Func->Profile2*100.0/(double)totalcount,
      (int)Func->Profile2, (int)Func->Profile1, *Func->GetFullName());
#else
    fprintf(stderr, "%3.2f%% (%9d) %9d %s\n",
      (double)Func->Profile2*100.0/(double)totalcount,
      (int)Func->Profile2, (int)Func->Profile1, *Func->GetFullName());
#endif
  }
}
