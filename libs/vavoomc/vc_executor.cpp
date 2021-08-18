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
//**  Copyright (C) 2018-2021 Ketmar Dark
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
//**
//**    Execution of PROGS.
//**
//**************************************************************************
//#define VCC_STUPID_TRACER
//#define VMEXEC_RUNDUMP
//#define VCC_DEBUG_CVAR_CACHE

#include "vc_public.h"
#include "vc_progdefs.h"

// builtin codes
#define BUILTIN_OPCODE_INFO
#include "vc_progdefs.h"

#define DICTDISPATCH_OPCODE_INFO
#include "vc_progdefs.h"

#define DYNARRDISPATCH_OPCODE_INFO
#include "vc_progdefs.h"

#include "vc_emit_context.h"


#define MAX_PROG_STACK  (10000)
#define STACK_ID  (0x45f6cd4b)

#define CHECK_STACK_OVERFLOW
#define CHECK_STACK_OVERFLOW_RT
#define CHECK_STACK_UNDERFLOW
//#define CHECK_FOR_EMPTY_STACK

#define CHECK_FOR_INF_NAN_DIV

// debug feature, nan and inf can be used for various purposes
// but meh...

// check for nan/inf in unary minus
//#define CHECK_FOR_NANS_INFS

// check for invalid vectors in various math operations
#define CHECK_FOR_NANS_INFS_VEC

// loading nan/inf vector from field
//#define CHECK_FOR_NANS_INFS_VEC_FLD

// check for returning invalid vector
//#define CHECK_FOR_NANS_INFS_RETVEC

// we don't really need it
//#define DENORMALS_PROTECTION

#ifdef DENORMALS_PROTECTION
# define VM_zeroDenormalsF            zeroDenormalsF
# define VM_zeroDenormalsFI32InPlace  zeroDenormalsFI32InPlace
#else
# define VM_zeroDenormalsF
# define VM_zeroDenormalsFI32InPlace
#endif


#ifdef VMEXEC_RUNDUMP
static int k8edIndent = 0;

static void printIndent () { for (int f = k8edIndent; f > 0; --f) fputc(' ', stderr); }
static void enterIndent () { ++k8edIndent; }
static void leaveIndent () { if (--k8edIndent < 0) abort(); }
#endif

enum { MaxDynArrayLength = 1024*1024*512 };


// ////////////////////////////////////////////////////////////////////////// //
//static VMethod *current_func = nullptr;
static VStack pr_stack[MAX_PROG_STACK+16384];


enum { BreakCheckLimit = 65536 }; // arbitrary
static unsigned breakCheckCount = 0;
int VObject::ProfilerEnabled = 0;
volatile unsigned VObject::vmAbortBySignal = 0;
VStack *VObject::pr_stackPtr = &pr_stack[1];

struct VCSlice {
  vuint8 *ptr; // it is easier to index this way
  vint32 length;
};


//==========================================================================
//
//  VObject::VMCheckStack
//
//==========================================================================
VStack *VObject::VMCheckStack (int size) noexcept {
  if (size < 0 || size > MAX_PROG_STACK) { VMDumpCallStack(); VPackage::InternalFatalError(va("requested invald VM stack slot count (%d)", size)); }
  int used = (int)(ptrdiff_t)(pr_stackPtr-&pr_stack[0]);
  if (used < 1 || used > MAX_PROG_STACK || MAX_PROG_STACK-size < used) { VMDumpCallStack(); VPackage::InternalFatalError("out of VM stack"); }
  return pr_stackPtr;
}


//==========================================================================
//
//  VObject::VMCheckAndClearStack
//
//==========================================================================
VStack *VObject::VMCheckAndClearStack (int size) noexcept {
  VStack *sp = VMCheckStack(size);
  if (size) memset(sp, 0, size*sizeof(VStack));
  return sp;
}


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
    callStack[cstUsed-1].sp = VObject::pr_stackPtr;
  }
}


static void cstPush (VMethod *func) {
  if (cstUsed == cstSize) {
    //FIXME: handle OOM here
    cstSize += 16384;
    callStack = (CallStackItem *)Z_Realloc(callStack, sizeof(callStack[0])*cstSize);
  }
  callStack[cstUsed].func = func;
  callStack[cstUsed].ip = nullptr;
  callStack[cstUsed].sp = VObject::pr_stackPtr;
  ++cstUsed;
}


static inline void cstPop () {
  if (cstUsed > 0) --cstUsed;
}


// `ip` can be null
static void cstDump (const vuint8 *ip, bool toStdErr=false) {
  if (VObject::DumpBacktraceToStdErr) toStdErr = true; // hard override
  //ip = func->Statements.Ptr();
  if (toStdErr) {
    fprintf(stderr, "\n\n=== Vavoom C call stack (%u) ===\n", cstUsed);
  } else {
    GLog.Logf(NAME_Error, "%s", "");
    GLog.Logf(NAME_Error, "*** Vavoom C code crashed!");
    GLog.Logf(NAME_Error, "=== Vavoom C call stack (%u) ===", cstUsed);
  }
  if (cstUsed > 0) {
    // do the best thing we can
    if (!ip && callStack[cstUsed-1].ip) ip = callStack[cstUsed-1].ip;
    const char *outs;
    for (vuint32 sp = cstUsed; sp > 0; --sp) {
      VMethod *func = callStack[sp-1].func;
      TLocation loc = func->FindPCLocation(sp == cstUsed ? ip : callStack[sp-1].ip);
      if (!loc.isInternal()) {
        outs = va("  %03u: %s (%s:%d)", cstUsed-sp, *func->GetFullName(), *loc.GetSourceFile(), loc.GetLine());
      } else {
        outs = va("  %03u: %s", cstUsed-sp, *func->GetFullName());
      }
      if (toStdErr) {
        fprintf(stderr, "%s\n", outs);
      } else {
        GLog.Logf(NAME_Error, "%s", outs);
      }
    }
  }
  if (toStdErr) {
    fprintf(stderr, "=============================\n\n");
  } else {
    GLog.Logf(NAME_Error, "=============================");
  }
}


//==========================================================================
//
//  PR_Init
//
//==========================================================================
void VObject::PR_Init () {
  // set stack ID for overflow / underflow checks
  pr_stack[0].i = STACK_ID;
  pr_stack[MAX_PROG_STACK-1].i = STACK_ID;
  VObject::pr_stackPtr = pr_stack+1;

  cstSize = 16384;
  callStack = (CallStackItem *)Z_Realloc(callStack, sizeof(callStack[0])*cstSize);
  cstUsed = 0;
}


//==========================================================================
//
//  PR_OnAbort
//
//==========================================================================
void VObject::PR_OnAbort () {
  //current_func = nullptr;
  VObject::pr_stackPtr = pr_stack+1;
  cstUsed = 0;
}


//==========================================================================
//
//  RunFunction
//
//==========================================================================

#if !defined(VCC_STUPID_TRACER)
# define USE_COMPUTED_GOTO  1
# else
# warning "computed gotos are off"
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

#define VM_CHECK_SIGABORT  do { \
  if ((breakCheckCount--) == 0) { \
    if (VObject::vmAbortBySignal) { cstDump(ip); VPackage::ExecutorAbort("VC VM execution aborted"); } \
    breakCheckCount = BreakCheckLimit; \
  } \
} while (0)


#define ReadU8(ip)     (*(vuint8 *)(ip))
#define ReadInt16(ip)  (*(vint16 *)(ip))
#define ReadInt32(ip)  (*(vint32 *)(ip))
#define ReadPtr(ip)    (*(void **)(ip))


static VScriptIterator **iterStack = nullptr;
static int iterStackUsed = 0;
static int iterStackSize = 0;


//==========================================================================
//
//  pushOldIterator
//
//==========================================================================
static void pushOldIterator (VScriptIterator *iter) {
  if (iterStackUsed == iterStackSize) {
    // grow
    iterStackSize = ((iterStackSize+1)|0x3ff)+1;
    iterStack = (VScriptIterator **)Z_Realloc(iterStack, iterStackSize*sizeof(iterStack[0]));
    if (!iterStack) { cstDump(nullptr); VPackage::InternalFatalError("popOldIterator: out of memory for iterator stack"); }
    if (iterStackUsed >= iterStackSize) { cstDump(nullptr); VPackage::InternalFatalError("popOldIterator: WTF?!"); }
  }
  iterStack[iterStackUsed++] = iter;
}


//==========================================================================
//
//  popOldIterator
//
//==========================================================================
static void popOldIterator () {
  if (iterStackUsed == 0) { cstDump(nullptr); VPackage::InternalFatalError("popOldIterator: iterator stack underflow"); }
  VScriptIterator *it = iterStack[--iterStackUsed];
  if (it) it->Finished();
}


// ////////////////////////////////////////////////////////////////////////// //
// WARNING! NO CHECKS!
struct ProfileTimer {
public:
  vuint64 stt;
  vuint64 total;

public:
  inline ProfileTimer () noexcept : stt(0), total(0) {}

  inline void start () noexcept { stt = Sys_GetTimeCPUNano(); }
  inline void stop () noexcept { if (stt) total += Sys_GetTimeCPUNano()-stt; stt = 0; }
  inline bool isRunning () const noexcept { return !!stt; }
};


// this automatically adds totals on exit
struct MethodProfiler {
public:
  VMethod *func;
  ProfileTimer timer;
  bool active;

public:
  VV_DISABLE_COPY(MethodProfiler)
  inline MethodProfiler (VMethod *afunc) noexcept : func(afunc), timer(), active(false) {}
  inline ~MethodProfiler () {
    if (active) {
      timer.stop();
      if (timer.total) {
        func->Profile.totalTime += timer.total;
        if (func->Profile.minTime && func->Profile.minTime < timer.total) func->Profile.minTime = timer.total;
        func->Profile.maxTime = max2(func->Profile.maxTime, timer.total);
      }
      active = false; // just in case
    }
  }
  inline void activate () noexcept { if (active) return; active = true; ++func->Profile.callCount; timer.start(); }
};


//==========================================================================
//
//  GetAndCacheCVar
//
//==========================================================================
static inline VCvar *GetAndCacheCVar (vuint8 *ip) noexcept {
  VName varname = VName::CreateWithIndexSafe(*(const vint32 *)(ip+2));
  VCvar *vp = *(VCvar **)(ip+2+4);
  if (!vp) {
    bool canCache;
    if (VObject::GetVCvarObject) {
      canCache = true;
      vp = VObject::GetVCvarObject(varname, &canCache);
    } else {
      canCache = true;
      vp = (varname != NAME_None ? VCvar::FindVariable(*varname) : nullptr);
    }
    if (!vp) VPackage::InternalFatalError(va("cannot get value of non-existent cvar '%s'", *varname));
    if (canCache) {
      #ifdef VCC_DEBUG_CVAR_CACHE
      GLog.Logf(NAME_Debug, "*** CACHING CVAR '%s' ADDRESS %p", *varname, vp);
      #endif
      *(VCvar **)(ip+2+4) = vp;
    }
  } else {
    #ifdef VCC_DEBUG_CVAR_CACHE
    GLog.Logf(NAME_Debug, "*** CACHED CVAR '%s' ADDRESS %p", *varname, vp);
    #endif
  }
  return vp;
}


//==========================================================================
//
//  GetRTCVar
//
//==========================================================================
static inline VCvar *GetRTCVar (int nameidx) noexcept {
  VName varname = VName::CreateWithIndexSafe(nameidx);
  VCvar *vp;
  if (VObject::GetVCvarObject) {
    bool canCache = false;
    vp = VObject::GetVCvarObject(varname, &canCache);
  } else {
    vp = (varname != NAME_None ? VCvar::FindVariable(*varname) : nullptr);
  }
  if (!vp) VPackage::InternalFatalError(va("cannot set value of non-existent cvar '%s'", *varname));
  return vp;
}


//==========================================================================
//
//  RunFunction
//
//==========================================================================
static void RunFunction (VMethod *func) {
  vuint8 *ip = nullptr;
  VStack *sp;
  VStack *local_vars;

  //current_func = func;

  if (!func) { cstDump(nullptr); VPackage::InternalFatalError("Trying to execute null function"); }

  const bool profEnabled = !!VObject::ProfilerEnabled;
  const bool profOnlyFunc = (VObject::ProfilerEnabled > 0);

  MethodProfiler mprof(func);
  if (profEnabled) mprof.activate();

  //fprintf(stderr, "FN(%d): <%s>\n", cstUsed, *func->GetFullName());

  if (func->Flags&FUNC_Net) {
    VStack *Params = VObject::pr_stackPtr-func->ParamsSize;
    if (((VObject *)Params[0].p)->ExecuteNetMethod(func)) return;
  }

  if (func->Flags&FUNC_Native) {
    // native function, first statement is pointer to function
    cstPush(func);
    func->NativeFunc();
    cstPop();
    return;
  }

  cstPush(func);

  // cache stack pointer in register
  sp = VObject::pr_stackPtr;

  // setup local vars
  //fprintf(stderr, "FUNC: <%s> (%s) ParamsSize=%d; NumLocals=%d; NumParams=%d\n", *func->GetFullName(), *func->Loc.toStringNoCol(), func->ParamsSize, func->NumLocals, func->NumParams);
  if (func->NumLocals < func->ParamsSize) { cstDump(nullptr); VPackage::InternalFatalError(va("Miscompiled function (locals=%d, params=%d)", func->NumLocals, func->ParamsSize)); }
  local_vars = sp-func->ParamsSize;
  if (func->NumLocals-func->ParamsSize != 0) memset(sp, 0, (func->NumLocals-func->ParamsSize)*sizeof(VStack));
  sp += func->NumLocals-func->ParamsSize;

  ip = func->Statements.Ptr();

#ifdef VMEXEC_RUNDUMP
  enterIndent(); printIndent(); fprintf(stderr, "ENTERING VC FUNCTION `%s`; sp=%d\n", *func->GetFullName(), (int)(sp-local_vars));
#endif

  VM_CHECK_SIGABORT;

#if USE_COMPUTED_GOTO
    static void *vm_labels[] = {
# define DECLARE_OPC(name, args) &&Lbl_OPC_ ## name
# define OPCODE_INFO
# include "vc_progdefs.h"
    0 };
#endif

  #define VC_VM_LABEL_TABLE
  #include "vc_executor_builtins_dynarr.cpp"
  #include "vc_executor_builtins.cpp"
  #include "vc_executor_builtins_dict.cpp"
  #undef VC_VM_LABEL_TABLE

  const vuint8 *origip = nullptr;

  // the main execution loop
  for (;;) {
func_loop:

#ifdef VCC_STUPID_TRACER
    fprintf(stderr, "*** %s: %6u: %s (sp=%d)\n", *func->GetFullName(), (unsigned)(ip-func->Statements.Ptr()), StatementInfo[*ip].name, (int)(sp-local_vars));
#endif

    PR_VM_SWITCH(*ip) {
      PR_VM_CASE(OPC_Done)
        cstDump(ip);
        VPackage::InternalFatalError("Empty function or invalid opcode");
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Call)
        //GLog.Logf("*** (%s) *** (callStack=%p; sp=%p; sptop=%p; cstUsed=%u)", *((VMethod *)ReadPtr(ip+1))->GetFullName(), callStack, sp, pr_stack, cstUsed);
        #ifdef CHECK_STACK_OVERFLOW_RT
        if (sp >= &pr_stack[MAX_PROG_STACK-4]) {
          cstDump(ip);
          VPackage::InternalFatalError(va("ExecuteFunction: Stack overflow in `%s`", *func->GetFullName()));
        }
        #endif
        VObject::pr_stackPtr = sp;
        cstFixTopIPSP(ip);
        //cstDump(ip);
        if (profOnlyFunc) mprof.timer.stop();
        RunFunction((VMethod *)ReadPtr(ip+1));
        if (profOnlyFunc) mprof.timer.start();
        //current_func = func;
        ip += 1+sizeof(void *);
        sp = VObject::pr_stackPtr;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PushVFunc)
        sp[0].p = ((VObject *)sp[-1].p)->GetVFunctionIdx(ReadInt16(ip+1));
        ip += 3;
        ++sp;
        PR_VM_BREAK;

      /*
      PR_VM_CASE(OPC_PushVFuncB)
        sp[0].p = ((VObject *)sp[-1].p)->GetVFunctionIdx(ip[1]);
        ip += 2;
        ++sp;
        PR_VM_BREAK;
      */

      PR_VM_CASE(OPC_PushFunc)
        sp[0].p = ReadPtr(ip+1);
        ip += 1+sizeof(void *);
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VCall)
        VObject::pr_stackPtr = sp;
        if (!sp[-ip[3]].p) { cstDump(ip); VPackage::InternalFatalError("Reference not set to an instance of an object"); }
        cstFixTopIPSP(ip);
        if (profOnlyFunc) mprof.timer.stop();
        RunFunction(((VObject *)sp[-ip[3]].p)->GetVFunctionIdx(ReadInt16(ip+1)));
        if (profOnlyFunc) mprof.timer.start();
        ip += 4;
        //current_func = func;
        sp = VObject::pr_stackPtr;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VCallB)
        VObject::pr_stackPtr = sp;
        if (!sp[-ip[2]].p) { cstDump(ip); VPackage::InternalFatalError("Reference not set to an instance of an object"); }
        cstFixTopIPSP(ip);
        if (profOnlyFunc) mprof.timer.stop();
        RunFunction(((VObject *)sp[-ip[2]].p)->GetVFunctionIdx(ip[1]));
        if (profOnlyFunc) mprof.timer.start();
        ip += 3;
        //current_func = func;
        sp = VObject::pr_stackPtr;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DelegateCall)
        {
          // get pointer to the delegate
          void **pDelegate = (void **)((vuint8 *)sp[-ip[5]].p+ReadInt32(ip+1));
          // push proper self object
          if (!pDelegate[0]) { cstDump(ip); VPackage::InternalFatalError("Delegate is not initialised"); }
          if (!pDelegate[1]) { cstDump(ip); VPackage::InternalFatalError("Delegate is not initialised (empty method)"); }
          if ((uintptr_t)pDelegate[1] < 65536) { cstDump(ip); VPackage::InternalFatalError("Delegate is completely fucked"); }
          sp[-ip[5]].p = pDelegate[0];
          VObject::pr_stackPtr = sp;
          cstFixTopIPSP(ip);
          if (profOnlyFunc) mprof.timer.stop();
          RunFunction((VMethod *)pDelegate[1]);
          if (profOnlyFunc) mprof.timer.start();
        }
        ip += 6;
        //current_func = func;
        sp = VObject::pr_stackPtr;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DelegateCallS)
        {
          // get pointer to the delegate
          void **pDelegate = (void **)((vuint8 *)sp[-ip[3]].p+ReadInt16(ip+1));
          // push proper self object
          if (!pDelegate[0]) { cstDump(ip); VPackage::InternalFatalError("Delegate is not initialised"); }
          if (!pDelegate[1]) { cstDump(ip); VPackage::InternalFatalError("Delegate is not initialised (empty method)"); }
          if ((uintptr_t)pDelegate[1] < 65536) { cstDump(ip); VPackage::InternalFatalError("Delegate is completely fucked"); }
          sp[-ip[3]].p = pDelegate[0];
          VObject::pr_stackPtr = sp;
          cstFixTopIPSP(ip);
          if (profOnlyFunc) mprof.timer.stop();
          RunFunction((VMethod *)pDelegate[1]);
          if (profOnlyFunc) mprof.timer.start();
        }
        ip += 4;
        //current_func = func;
        sp = VObject::pr_stackPtr;
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
          if (!pDelegate[0]) { cstDump(ip); VPackage::InternalFatalError("Delegate is not initialised"); }
          if (!pDelegate[1]) { cstDump(ip); VPackage::InternalFatalError("Delegate is not initialised (empty method)"); }
          if ((uintptr_t)pDelegate[1] < 65536) { cstDump(ip); VPackage::InternalFatalError("Delegate is completely fucked"); }
          sp[-sofs].p = pDelegate[0];
          VObject::pr_stackPtr = sp;
          cstFixTopIPSP(ip);
          if (profOnlyFunc) mprof.timer.stop();
          RunFunction((VMethod *)pDelegate[1]);
          if (profOnlyFunc) mprof.timer.start();
        }
        //current_func = func;
        sp = VObject::pr_stackPtr;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Return)
        //vensure(sp == local_vars+func->NumLocals);
        if (sp != local_vars+func->NumLocals) { cstDump(ip); VPackage::InternalFatalError(va("assertion failed: `sp == local_vars+func->NumLocals`: ip=%d; diff=%d; func->NumLocals=%d", (int)(ptrdiff_t)(ip-func->Statements.Ptr()), (int)(ptrdiff_t)(sp-(local_vars+func->NumLocals)), func->NumLocals)); }
        #ifdef VMEXEC_RUNDUMP
        printIndent(); fprintf(stderr, "LEAVING VC FUNCTION `%s`; sp=%d\n", *func->GetFullName(), (int)(sp-pr_stack)); leaveIndent();
        #endif
        VObject::pr_stackPtr = local_vars;
        cstPop();
        return;

      PR_VM_CASE(OPC_ReturnL)
        vensure(sp == local_vars+func->NumLocals+1);
        #ifdef VMEXEC_RUNDUMP
        printIndent(); fprintf(stderr, "LEAVING VC FUNCTION `%s`; sp=%d\n", *func->GetFullName(), (int)(sp-pr_stack)); leaveIndent();
        #endif
        ((VStack *)local_vars)[0] = sp[-1];
        VObject::pr_stackPtr = local_vars+1;
        cstPop();
        return;

      PR_VM_CASE(OPC_ReturnV)
        vensure(sp == local_vars+func->NumLocals+3);
        #ifdef VMEXEC_RUNDUMP
        printIndent(); fprintf(stderr, "LEAVING VC FUNCTION `%s`; sp=%d\n", *func->GetFullName(), (int)(sp-pr_stack)); leaveIndent();
        #endif
        #ifdef CHECK_FOR_NANS_INFS_RETVEC
        if (!isFiniteFI32(sp[-3].i) || !isFiniteFI32(sp[-2].i) || !isFiniteFI32(sp[-1].i)) { cstDump(ip); VPackage::InternalFatalError("returning NAN/INF vector"); }
        #endif
        ((VStack *)local_vars)[0] = sp[-3];
        ((VStack *)local_vars)[1] = sp[-2];
        ((VStack *)local_vars)[2] = sp[-1];
        VObject::pr_stackPtr = local_vars+3;
        cstPop();
        return;

      PR_VM_CASE(OPC_GotoB)
        VM_CHECK_SIGABORT;
        ip += ip[1];
        PR_VM_BREAK;

      PR_VM_CASE(OPC_GotoNB)
        VM_CHECK_SIGABORT;
        ip -= ip[1];
        PR_VM_BREAK;

      /*
      PR_VM_CASE(OPC_GotoS)
        ip += ReadInt16(ip+1);
        PR_VM_BREAK;
      */

      PR_VM_CASE(OPC_Goto)
        VM_CHECK_SIGABORT;
        ip += ReadInt32(ip+1);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IfGotoB)
        VM_CHECK_SIGABORT;
        if (sp[-1].i) ip += ip[1]; else ip += 2;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IfGotoNB)
        VM_CHECK_SIGABORT;
        if (sp[-1].i) ip -= ip[1]; else ip += 2;
        --sp;
        PR_VM_BREAK;

      /*
      PR_VM_CASE(OPC_IfGotoS)
        if (sp[-1].i) ip += ReadInt16(ip+1); else ip += 3;
        --sp;
        PR_VM_BREAK;
      */

      PR_VM_CASE(OPC_IfGoto)
        VM_CHECK_SIGABORT;
        if (sp[-1].i) ip += ReadInt32(ip+1); else ip += 5;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IfNotGotoB)
        VM_CHECK_SIGABORT;
        if (!sp[-1].i) ip += ip[1]; else ip += 2;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IfNotGotoNB)
        VM_CHECK_SIGABORT;
        if (!sp[-1].i) ip -= ip[1]; else ip += 2;
        --sp;
        PR_VM_BREAK;

      /*
      PR_VM_CASE(OPC_IfNotGotoS)
        if (!sp[-1].i) ip += ReadInt16(ip+1); else ip += 3;
        --sp;
        PR_VM_BREAK;
      */

      PR_VM_CASE(OPC_IfNotGoto)
        VM_CHECK_SIGABORT;
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

      /*
      PR_VM_CASE(OPC_CaseGotoS)
        if (ReadInt16(ip+1) == sp[-1].i) {
          ip += ReadInt16(ip+3);
          --sp;
        } else {
          ip += 5;
        }
        PR_VM_BREAK;
      */

      PR_VM_CASE(OPC_CaseGoto)
        if (ReadInt32(ip+1) == sp[-1].i) {
          ip += ReadInt16(ip+5);
          --sp;
        } else {
          ip += 7;
        }
        PR_VM_BREAK;

      // yeah, exactly the same as normal `OPC_CaseGoto`
      PR_VM_CASE(OPC_CaseGotoN)
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

      /*
      PR_VM_CASE(OPC_PushNumberS)
        sp->i = ReadInt16(ip+1);
        ip += 3;
        ++sp;
        PR_VM_BREAK;
      */

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

      // bytecode arg: pointer to the immutable `VStr`
      PR_VM_CASE(OPC_PushString)
        {
          sp->p = ReadPtr(ip+1);
          ip += 1+sizeof(void *);
          const VStr *S = (const VStr *)sp[0].p;
          sp[0].p = nullptr;
          *(VStr *)&sp[0].p = *S;
          ++sp;
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
        /* nope, this is allowed
        #ifdef CHECK_FOR_NANS_INFS_VEC
        if (!isFiniteFI32(sp[0].i) || !isFiniteFI32(sp[1].i) || !isFiniteFI32(sp[2].i)) { cstDump(ip); VPackage::InternalFatalError("creating NAN/INF vector"); }
        #endif
        */
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
        if (!sp[-1].p) { cstDump(ip); VPackage::InternalFatalError("Reference not set to an instance of an object"); }
        sp[-1].p = (vuint8 *)sp[-1].p+ReadInt32(ip+1);
        ip += 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_OffsetS)
        if (!sp[-1].p) { cstDump(ip); VPackage::InternalFatalError("Reference not set to an instance of an object"); }
        sp[-1].p = (vuint8 *)sp[-1].p+ReadInt16(ip+1);
        ip += 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FieldValue)
        if (!sp[-1].p) { cstDump(ip); VPackage::InternalFatalError("Reference not set to an instance of an object"); }
        sp[-1].i = *(vint32 *)((vuint8 *)sp[-1].p+ReadInt32(ip+1));
        ip += 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FieldValueS)
        if (!sp[-1].p) { cstDump(ip); VPackage::InternalFatalError("Reference not set to an instance of an object"); }
        sp[-1].i = *(vint32 *)((vuint8 *)sp[-1].p+ReadInt16(ip+1));
        ip += 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VFieldValue)
        if (!sp[-1].p) { cstDump(ip); VPackage::InternalFatalError("Reference not set to an instance of an object"); }
        {
          const TVec *vp = (const TVec *)((vuint8 *)sp[-1].p+ReadInt32(ip+1));
          sp[1].f = vp->z;
          sp[0].f = vp->y;
          sp[-1].f = vp->x;
          #ifdef CHECK_FOR_NANS_INFS_VEC_FLD
          if (!isFiniteFI32(sp[1].i) || !isFiniteFI32(sp[0].i) || !isFiniteFI32(sp[-1].i)) { cstDump(ip); VPackage::InternalFatalError("loading NAN/INF vector"); }
          #endif
        }
        sp += 2;
        ip += 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VFieldValueS)
        if (!sp[-1].p) { cstDump(ip); VPackage::InternalFatalError("Reference not set to an instance of an object"); }
        {
          TVec *vp = (TVec *)((vuint8 *)sp[-1].p+ReadInt16(ip+1));
          sp[1].f = vp->z;
          sp[0].f = vp->y;
          sp[-1].f = vp->x;
        }
        sp += 2;
        ip += 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PtrFieldValue)
        if (!sp[-1].p) { cstDump(ip); VPackage::InternalFatalError("Reference not set to an instance of an object"); }
        sp[-1].p = *(void **)((vuint8 *)sp[-1].p+ReadInt32(ip+1));
        ip += 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_PtrFieldValueS)
        if (!sp[-1].p) { cstDump(ip); VPackage::InternalFatalError("Reference not set to an instance of an object"); }
        sp[-1].p = *(void **)((vuint8 *)sp[-1].p+ReadInt16(ip+1));
        ip += 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_StrFieldValue)
        if (!sp[-1].p) { cstDump(ip); VPackage::InternalFatalError("Reference not set to an instance of an object"); }
        {
          VStr *Ptr = (VStr *)((vuint8 *)sp[-1].p+ReadInt32(ip+1));
          sp[-1].p = nullptr;
          *(VStr *)&sp[-1].p = *Ptr;
        }
        ip += 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_StrFieldValueS)
        if (!sp[-1].p) { cstDump(ip); VPackage::InternalFatalError("Reference not set to an instance of an object"); }
        {
          VStr *Ptr = (VStr *)((vuint8 *)sp[-1].p+ReadInt16(ip+1));
          sp[-1].p = nullptr;
          *(VStr *)&sp[-1].p = *Ptr;
        }
        ip += 3;
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
          cstDump(ip); VPackage::InternalFatalError("Reference not set to an instance of an object");
        }
        ip += 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteFieldValue)
        if (!sp[-1].p) { cstDump(ip); VPackage::InternalFatalError("Reference not set to an instance of an object"); }
        sp[-1].i = *((vuint8 *)sp[-1].p+ReadInt32(ip+1));
        ip += 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteFieldValueS)
        if (!sp[-1].p) { cstDump(ip); VPackage::InternalFatalError("Reference not set to an instance of an object"); }
        sp[-1].i = *((vuint8 *)sp[-1].p+ReadInt16(ip+1));
        ip += 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Bool0FieldValue)
        if (!sp[-1].p) { cstDump(ip); VPackage::InternalFatalError("Reference not set to an instance of an object"); }
        sp[-1].i = !!(*(vint32 *)((vuint8 *)sp[-1].p+ReadInt32(ip+1))&ip[5]);
        ip += 6;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Bool0FieldValueS)
        if (!sp[-1].p) { cstDump(ip); VPackage::InternalFatalError("Reference not set to an instance of an object"); }
        sp[-1].i = !!(*(vint32 *)((vuint8 *)sp[-1].p+ReadInt16(ip+1))&ip[3]);
        ip += 4;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Bool1FieldValue)
        if (!sp[-1].p) { cstDump(ip); VPackage::InternalFatalError("Reference not set to an instance of an object"); }
        sp[-1].i = !!(*(vint32 *)((vuint8 *)sp[-1].p+ReadInt32(ip+1))&(ip[5]<<8));
        ip += 6;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Bool1FieldValueS)
        if (!sp[-1].p) { cstDump(ip); VPackage::InternalFatalError("Reference not set to an instance of an object"); }
        sp[-1].i = !!(*(vint32 *)((vuint8 *)sp[-1].p+ReadInt16(ip+1))&(ip[3]<<8));
        ip += 4;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Bool2FieldValue)
        if (!sp[-1].p) { cstDump(ip); VPackage::InternalFatalError("Reference not set to an instance of an object"); }
        sp[-1].i = !!(*(vint32 *)((vuint8 *)sp[-1].p+ReadInt32(ip+1))&(ip[5]<<16));
        ip += 6;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Bool2FieldValueS)
        if (!sp[-1].p) { cstDump(ip); VPackage::InternalFatalError("Reference not set to an instance of an object"); }
        sp[-1].i = !!(*(vint32 *)((vuint8 *)sp[-1].p+ReadInt16(ip+1))&(ip[3]<<16));
        ip += 4;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Bool3FieldValue)
        if (!sp[-1].p) { cstDump(ip); VPackage::InternalFatalError("Reference not set to an instance of an object"); }
        sp[-1].i = !!(*(vint32 *)((vuint8 *)sp[-1].p+ReadInt32(ip+1))&(ip[5]<<24));
        ip += 6;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Bool3FieldValueS)
        if (!sp[-1].p) { cstDump(ip); VPackage::InternalFatalError("Reference not set to an instance of an object"); }
        sp[-1].i = !!(*(vint32 *)((vuint8 *)sp[-1].p+ReadInt16(ip+1))&(ip[3]<<24));
        ip += 4;
        PR_VM_BREAK;

      // won't pop index
      // [-2]: op
      // [-1]: idx
      PR_VM_CASE(OPC_CheckArrayBounds)
        if (sp[-1].i < 0 || sp[-1].i >= ReadInt32(ip+1)) {
          cstDump(ip);
          VPackage::InternalFatalError(va("Array index %d is out of bounds (%d)", sp[-1].i, ReadInt32(ip+1)));
        }
        ip += 5;
        PR_VM_BREAK;

      // won't pop index
      // [-3]: op
      // [-2]: idx
      // [-1]: idx2
      PR_VM_CASE(OPC_CheckArrayBounds2D)
        if (sp[-2].i < 0 || sp[-2].i >= ReadInt16(ip+1)) { cstDump(ip); VPackage::InternalFatalError(va("First array index %d is out of bounds (%d)", sp[-2].i, ReadInt16(ip+1))); }
        if (sp[-1].i < 0 || sp[-1].i >= ReadInt16(ip+1+2)) { cstDump(ip); VPackage::InternalFatalError(va("Second array index %d is out of bounds (%d)", sp[-1].i, ReadInt16(ip+1+2))); }
        ip += 1+2+2+4;
        PR_VM_BREAK;

      // [-2]: op
      // [-1]: idx
      PR_VM_CASE(OPC_ArrayElement)
        sp[-2].p = (vuint8 *)sp[-2].p+sp[-1].i*ReadInt32(ip+1);
        ip += 5;
        --sp;
        PR_VM_BREAK;

      /*
      PR_VM_CASE(OPC_ArrayElementS)
        sp[-2].p = (vuint8 *)sp[-2].p+sp[-1].i*ReadInt16(ip+1);
        ip += 3;
        --sp;
        PR_VM_BREAK;
      */

      PR_VM_CASE(OPC_ArrayElementB)
        sp[-2].p = (vuint8 *)sp[-2].p+sp[-1].i*ip[1];
        ip += 2;
        --sp;
        PR_VM_BREAK;

      // [-3]: op
      // [-2]: idx
      // [-1]: idx2
      PR_VM_CASE(OPC_ArrayElement2D)
        sp[-3].p = (vuint8 *)sp[-3].p+(sp[-1].i*ReadInt16(ip+1)+sp[-2].i)*ReadInt32(ip+1+2+2);
        sp -= 2;
        ip += 1+2+2+4;
        PR_VM_BREAK;

      // [-1]: index
      // [-2]: ptr to VCSlice
      PR_VM_CASE(OPC_SliceElement)
        if (sp[-2].p) {
          int idx = sp[-1].i;
          VCSlice vs = *(VCSlice *)sp[-2].p;
          if (!vs.ptr) vs.length = 0; else if (vs.length < 0) vs.length = 0; // just in case
          if (idx < 0 || idx >= vs.length) { cstDump(ip); VPackage::InternalFatalError(va("Slice index %d is out of range (%d)", idx, vs.length)); }
          sp[-2].p = vs.ptr+idx*ReadInt32(ip+1);
        } else {
          cstDump(ip);
          VPackage::InternalFatalError(va("Slice index %d is out of range (0)", sp[-1].i));
        }
        ip += 5;
        --sp;
        PR_VM_BREAK;

      // [-1]: ptr to VCSlice
      PR_VM_CASE(OPC_SliceToBool)
        if (sp[-1].p) {
          VCSlice vs = *(VCSlice *)sp[-1].p;
          sp[-1].i = !!(vs.ptr && vs.length);
        } else {
          sp[-1].i = 0;
        }
        PR_VM_BREAK;

      /*
      PR_VM_CASE(OPC_OffsetPtr)
        if (!sp[-1].p) { cstDump(ip); VPackage::InternalFatalError("Cannot offset null pointer"); }
        sp[-1].p = (vuint8 *)sp[-1].p+ReadInt32(ip+1);
        ip += 5;
        PR_VM_BREAK;
      */

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
        {
          const TVec *ptr = (const TVec *)sp[-1].p;
          sp[1].f = ptr->z;
          sp[0].f = ptr->y;
          sp[-1].f = ptr->x;
        }
        /* nope, this is allowed
        #ifdef CHECK_FOR_NANS_INFS_VEC
        if (!isFiniteFI32(sp[1].i) || !isFiniteFI32(sp[0].i) || !isFiniteFI32(sp[-1].i)) { cstDump(ip); VPackage::InternalFatalError("pushing NAN/INF vector"); }
        #endif
        */
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
        if (!sp[-1].i) { cstDump(ip); VPackage::InternalFatalError("Division by 0"); }
        BINOP_Q(i, /=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Modulus)
        if (!sp[-1].i) { cstDump(ip); VPackage::InternalFatalError("Division by 0"); }
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

      PR_VM_CASE(OPC_URShift)
        ++ip;
        sp[-2].u >>= sp[-1].i;
        --sp;
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

    #define ASSIGNOPNN(type, mem, op) \
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
        if (!sp[-1].i) { cstDump(ip); VPackage::InternalFatalError("Division by 0"); }
        ASSIGNOP(vint32, i, /=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ModVarDrop)
        if (!sp[-1].i) { cstDump(ip); VPackage::InternalFatalError("Division by 0"); }
        ASSIGNOP(vint32, i, %=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_AndVarDrop)
        ASSIGNOP(vuint32, i, &=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_OrVarDrop)
        ASSIGNOP(vuint32, i, |=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_XOrVarDrop)
        ASSIGNOP(vuint32, i, ^=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_LShiftVarDrop)
        ASSIGNOP(vuint32, i, <<=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_RShiftVarDrop)
        ASSIGNOP(vint32, i, >>=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_URShiftVarDrop)
        ASSIGNOP(vuint32, u, >>=);
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
        if (!sp[-1].i) { cstDump(ip); VPackage::InternalFatalError("Division by 0"); }
        ASSIGNOP(vuint8, i, /=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ByteModVarDrop)
        if (!sp[-1].i) { cstDump(ip); VPackage::InternalFatalError("Division by 0"); }
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

      // [-1] src
      // [-2] dest
      PR_VM_CASE(OPC_CatAssignVarDrop)
        ++ip;
        *(VStr *)sp[-2].p += *(VStr *)&sp[-1].p;
        ((VStr *)&sp[-1].p)->Clean();
        sp -= 2;
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
        if (isZeroFI32(sp[-1].i)) { cstDump(ip); VPackage::InternalFatalError("Division by 0"); }
        #ifdef CHECK_FOR_INF_NAN_DIV
        if (!isFiniteFI32(sp[-1].i)) { cstDump(ip); if (isNaNFU32((uint32_t)sp[-1].i)) VPackage::InternalFatalError("Division by NAN"); else VPackage::InternalFatalError("Division by INF"); }
        #endif
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
        #ifdef CHECK_FOR_NANS_INFS
        if (!isFiniteFI32(sp[-1].i)) { cstDump(ip); VPackage::InternalFatalError("unary with NAN/INF"); }
        #endif
        //sp[-1].f = -sp[-1].f;
        sp[-1].i ^= 0x80000000; // direct modification of float32 sign bit
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
        if (isZeroFI32(sp[-1].i)) { cstDump(ip); VPackage::InternalFatalError("Division by 0"); }
        #ifdef CHECK_FOR_INF_NAN_DIV
        if (!isFiniteFI32(sp[-1].i)) { cstDump(ip); if (isNaNFU32((uint32_t)sp[-1].i)) VPackage::InternalFatalError("Division by NAN"); else VPackage::InternalFatalError("Division by INF"); }
        #endif
        ASSIGNOP(float, f, /=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VAdd)
        #ifdef CHECK_FOR_NANS_INFS_VEC
        //if (!isFiniteFI32(sp[-6].i) || !isFiniteFI32(sp[-5].i) || !isFiniteFI32(sp[-4].i)) { cstDump(ip); VPackage::InternalFatalError("vec+ op0 is NAN/INF"); }
        //if (!isFiniteFI32(sp[-3].i) || !isFiniteFI32(sp[-2].i) || !isFiniteFI32(sp[-1].i)) { cstDump(ip); VPackage::InternalFatalError("vec+ op1 is NAN/INF"); }
        if (!isFiniteFI32(sp[-6].i) || !isFiniteFI32(sp[-5].i) || !isFiniteFI32(sp[-4].i) || !isFiniteFI32(sp[-3].i) || !isFiniteFI32(sp[-2].i) || !isFiniteFI32(sp[-1].i)) { cstDump(ip); VPackage::InternalFatalError("vec+ with NAN/INF"); }
        #endif
        sp[-6].f += sp[-3].f;
        sp[-5].f += sp[-2].f;
        sp[-4].f += sp[-1].f;
        sp -= 3;
        ++ip;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VSubtract)
        #ifdef CHECK_FOR_NANS_INFS_VEC
        //if (!isFiniteFI32(sp[-6].i) || !isFiniteFI32(sp[-5].i) || !isFiniteFI32(sp[-4].i)) { cstDump(ip); VPackage::InternalFatalError("vec- op0 is NAN/INF"); }
        //if (!isFiniteFI32(sp[-3].i) || !isFiniteFI32(sp[-2].i) || !isFiniteFI32(sp[-1].i)) { cstDump(ip); VPackage::InternalFatalError("vec- op1 is NAN/INF"); }
        if (!isFiniteFI32(sp[-6].i) || !isFiniteFI32(sp[-5].i) || !isFiniteFI32(sp[-4].i) || !isFiniteFI32(sp[-3].i) || !isFiniteFI32(sp[-2].i) || !isFiniteFI32(sp[-1].i)) { cstDump(ip); VPackage::InternalFatalError("vec- with NAN/INF"); }
        #endif
        sp[-6].f -= sp[-3].f;
        sp[-5].f -= sp[-2].f;
        sp[-4].f -= sp[-1].f;
        sp -= 3;
        ++ip;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VPreScale)
        {
          #ifdef CHECK_FOR_NANS_INFS_VEC
          if (!isFiniteFI32(sp[-4].i) || !isFiniteFI32(sp[-3].i) || !isFiniteFI32(sp[-2].i) || !isFiniteFI32(sp[-1].i)) { cstDump(ip); VPackage::InternalFatalError("vecprescale with NAN/INF"); }
          #endif
          const float scale = VM_zeroDenormalsF(sp[-4].f);
          sp[-4].f = scale*sp[-3].f;
          sp[-3].f = scale*sp[-2].f;
          sp[-2].f = scale*sp[-1].f;
          VM_zeroDenormalsFI32InPlace(&sp[-4].i);
          VM_zeroDenormalsFI32InPlace(&sp[-3].i);
          VM_zeroDenormalsFI32InPlace(&sp[-2].i);
          --sp;
          ++ip;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VPostScale)
        #ifdef CHECK_FOR_NANS_INFS_VEC
        if (!isFiniteFI32(sp[-4].i) || !isFiniteFI32(sp[-3].i) || !isFiniteFI32(sp[-2].i) || !isFiniteFI32(sp[-1].i)) { cstDump(ip); VPackage::InternalFatalError("vecscale with NAN/INF"); }
        #endif
        {
          const float scale = VM_zeroDenormalsF(sp[-1].f);
          sp[-4].f *= scale;
          sp[-3].f *= scale;
          sp[-2].f *= scale;
        }
        #ifdef CHECK_FOR_NANS_INFS_VEC
        // we may get infinity with multiplication
        if (!isFiniteFI32(sp[-4].i) || !isFiniteFI32(sp[-3].i) || !isFiniteFI32(sp[-2].i)) { cstDump(ip); VPackage::InternalFatalError("vecscale vec is NAN/INF (exit)"); }
        #endif
        // also, we may get denormals with multiplication
        VM_zeroDenormalsFI32InPlace(&sp[-4].i);
        VM_zeroDenormalsFI32InPlace(&sp[-3].i);
        VM_zeroDenormalsFI32InPlace(&sp[-2].i);
        --sp;
        ++ip;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VIScale)
        #ifdef CHECK_FOR_NANS_INFS_VEC
        if (!isFiniteFI32(sp[-4].i) || !isFiniteFI32(sp[-3].i) || !isFiniteFI32(sp[-2].i) || !isFiniteFI32(sp[-1].i)) { cstDump(ip); VPackage::InternalFatalError("veciscale with NAN/INF"); }
        #endif
        if (isZeroFI32(sp[-1].i)) { cstDump(ip); VPackage::InternalFatalError("veciscale with zero scale"); }
        {
          const float scale = sp[-1].f;
          sp[-4].f /= scale;
          sp[-3].f /= scale;
          sp[-2].f /= scale;
        }
        // we cannot get infinity with division, but we may get denormals
        VM_zeroDenormalsFI32InPlace(&sp[-4].i);
        VM_zeroDenormalsFI32InPlace(&sp[-3].i);
        VM_zeroDenormalsFI32InPlace(&sp[-2].i);
        --sp;
        ++ip;
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
        /* no need to, i believe
        #ifdef CHECK_FOR_NANS_INFS_VEC
        if (!isFiniteFI32(sp[-3].i) || !isFiniteFI32(sp[-2].i) || !isFiniteFI32(sp[-1].i)) { cstDump(ip); VPackage::InternalFatalError("vecunary vec is NAN/INF"); }
        #endif
        */
        /*
        sp[-3].f = -sp[-3].f;
        sp[-2].f = -sp[-2].f;
        sp[-1].f = -sp[-1].f;
        */
        // direct manipulation with float32 sign bit
        sp[-3].i ^= 0x80000000;
        sp[-2].i ^= 0x80000000;
        sp[-1].i ^= 0x80000000;
        ++ip;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VFixVecParam)
        {
          vint32 Idx = ip[1];
          ip += 2;
          TVec *v = (TVec *)&local_vars[Idx];
          v->y = local_vars[Idx+1].f;
          v->z = local_vars[Idx+2].f;
        }
        PR_VM_BREAK;

#ifdef CHECK_FOR_NANS_INFS_VEC
    #define VASSIGNOP(op) \
      { \
        TVec *ptr = (TVec *)sp[-4].p; \
        if (!ptr->isValid()) { cstDump(ip); VPackage::InternalFatalError(va("vassign op '%s' op0 is NAN/INF", #op)); } \
        if (!isFiniteFI32(sp[-3].i) || !isFiniteFI32(sp[-2].i) || !isFiniteFI32(sp[-1].i)) { cstDump(ip); VPackage::InternalFatalError(va("vassign op '%s' op1 is NAN/INF", #op)); } \
        ptr->x op sp[-3].f; \
        ptr->y op sp[-2].f; \
        ptr->z op sp[-1].f; \
        if (!ptr->isValid()) { cstDump(ip); VPackage::InternalFatalError(va("vassign op '%s' op0 is NAN/INF (exit)", #op)); } \
        ptr->fixDenormalsInPlace(); /*just in case*/\
        sp -= 4; \
        ++ip; \
      }
#else
    #define VASSIGNOP(op) \
      { \
        ++ip; \
        TVec *ptr = (TVec *)sp[-4].p; \
        ptr->x op sp[-3].f; \
        ptr->y op sp[-2].f; \
        ptr->z op sp[-1].f; \
        ptr->fixDenormalsInPlace(); /*just in case*/\
        sp -= 4; \
      }
#endif

      PR_VM_CASE(OPC_VAssignDrop)
        // cannot use it, because it checks destination vector
        //VASSIGNOP(=);
        {
          TVec *ptr = (TVec *)sp[-4].p;
          ptr->x = sp[-3].f;
          ptr->y = sp[-2].f;
          ptr->z = sp[-1].f;
          ptr->fixDenormalsInPlace(); /*just in case*/
        }
        sp -= 4;
        ++ip;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VAddVarDrop)
        VASSIGNOP(+=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VSubVarDrop)
        VASSIGNOP(-=);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VScaleVarDrop)
        #ifdef CHECK_FOR_NANS_INFS_VEC
        if (!isFiniteFI32(sp[-1].i)) { cstDump(ip); VPackage::InternalFatalError("vecscaledrop scale is NAN/INF"); }
        #endif
        ++ip;
        {
          TVec *ptr = (TVec *)sp[-2].p;
          (*ptr) *= VM_zeroDenormalsF(sp[-1].f);
          #ifdef CHECK_FOR_NANS_INFS_VEC
          // we may get infinity with multiplication
          if (!ptr->isValid()) { cstDump(ip); VPackage::InternalFatalError("vecscaledrop vec is NAN/INF (exit)"); }
          #endif
          ptr->fixDenormalsInPlace();
        }
        //*(TVec *)sp[-2].p *= sp[-1].f;
        sp -= 2;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VIScaleVarDrop)
        if (isZeroFI32(sp[-1].i)) { cstDump(ip); VPackage::InternalFatalError("Vector division by 0"); }
        #ifdef CHECK_FOR_NANS_INFS_VEC
        if (!isFiniteFI32(sp[-1].i)) { cstDump(ip); if (isNaNFU32((uint32_t)sp[-1].i)) VPackage::InternalFatalError("Vector division by NAN"); else VPackage::InternalFatalError("Vector division by INF"); }
        #endif
        ++ip;
        {
          TVec *ptr = (TVec *)sp[-2].p;
          (*ptr) /= sp[-1].f;
          // we cannot get infinity with division, but we may get denormals
          ptr->fixDenormalsInPlace();
        }
        //*(TVec *)sp[-2].p /= sp[-1].f;
        sp -= 2;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VectorDirect)
        switch (ip[1]) {
          case 0: break;
          case 1: sp[-3].f = sp[-2].f; break;
          case 2: sp[-3].f = sp[-1].f; break;
          default: { cstDump(ip); VPackage::InternalFatalError(va("Invalid direct vector access index %u", (unsigned)ip[1])); }
        }
        ip += 2;
        sp -= 2;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VectorSwizzleDirect)
        {
          int sw = ip[1]|(ip[2]<<8);
          // common swizzle -- `vec.xy`
          if (sw == (VCVSE_X|(VCVSE_Y<<VCVSE_Shift))) {
            sp[-1].f = 0.0f;
          } else {
            TVec v(sp[-3].f, sp[-2].f, sp[-1].f);
            for (int spidx = 0; spidx <= 2; ++spidx) {
              //GLog.Logf("SWD(0): spidx=%d (%g); el=0x%02x; neg=%d", spidx, sp[-3+spidx].f, (sw&VCVSE_ElementMask), (sw&VCVSE_Negate));
              switch (sw&VCVSE_ElementMask) {
                case VCVSE_Zero: sp[-3+spidx].f = 0.0f; break;
                case VCVSE_One: sp[-3+spidx].f = 1.0f; break;
                case VCVSE_X: sp[-3+spidx].f = v.x; break;
                case VCVSE_Y: sp[-3+spidx].f = v.y; break;
                case VCVSE_Z: sp[-3+spidx].f = v.z; break;
                default: { cstDump(ip); VPackage::InternalFatalError(va("Invalid direct vector access mask 0x%02x (0x%01x)", ip[1], sw&VCVSE_Mask)); }
              }
              if (sw&VCVSE_Negate) sp[-3+spidx].f = -sp[-3+spidx].f;
              //GLog.Logf("SWD(1): spidx=%d (%g); el=0x%02x; neg=%d", spidx, sp[-3+spidx].f, (sw&VCVSE_ElementMask), (sw&VCVSE_Negate));
              sw >>= VCVSE_Shift;
            }
          }
          //GLog.Logf("SWD: v=(%g,%g,%g); res=(%g,%g,%g)", v.x, v.y, v.z, sp[-3].f, sp[-2].f, sp[-1].f);
        }
        ip += 3;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FloatToBool)
        // so inf/nan yields `false`
        sp[-1].i = !isZeroInfNaNFI32(sp[-1].i);
        ++ip;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_VectorToBool)
        // `false` is either zero, or invalid vector
        {
          const int32_t n = sp[-3].i|sp[-2].i|sp[-1].i;
          sp[-3].i = ((isFiniteFI32(sp[-3].i) && isFiniteFI32(sp[-2].i) && isFiniteFI32(sp[-1].i)) && !isZeroFI32(n));
        }
        sp -= 2;
        ++ip;
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
          // do not clean it, as this opcode receives pointer to str
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
          v = (v >= 0 && v < ((VStr *)&sp[-2].p)->length() ? (vuint8)(*((VStr *)&sp[-2].p))[v] : 0);
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
          // do not clean it, as this opcode receives pointer to str
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
            VPackage::InternalFatalError(va("Slice [%d..%d] is out of range (%d)", lo, hi, vs.length));
          } else {
            sp -= 1; // drop one unused limit
            // push slice
            sp[-2].p = vs.ptr+lo*ReadInt32(ip+1);
            sp[-1].i = hi-lo;
          }
        } else {
          cstDump(ip); VPackage::InternalFatalError("Cannot operate on none-slice");
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

      PR_VM_CASE(OPC_PtrSubtract)
        {
          if (!sp[-2].p) { cstDump(ip); VPackage::InternalFatalError("Invalid pointer math (first operand is `nullptr`)"); }
          if (!sp[-1].p) { cstDump(ip); VPackage::InternalFatalError("Invalid pointer math (second operand is `nullptr`)"); }
          //if ((uintptr_t)sp[-2].p < (uintptr_t)sp[-1].p) { cstDump(ip); VPackage::InternalFatalError("Invalid pointer math (first operand is out of range)"); }
          int tsize = ReadInt32(ip+1);
          ptrdiff_t diff = ((intptr_t)sp[-2].p-(intptr_t)sp[-1].p)/tsize;
          if (diff < -0x7fffffff || diff > 0x7fffffff) { cstDump(ip); VPackage::InternalFatalError("Invalid pointer math (difference is too big)"); }
          sp -= 1;
          sp[-1].i = (int)diff;
        }
        ip += 5;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IntToFloat)
        ++ip;
        {
          const float fv = (float)sp[-1].i;
          if (!isFiniteF(fv)) { cstDump(ip); VPackage::InternalFatalError("Invalid int->float conversion"); }
          sp[-1].f = fv;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_FloatToInt)
        ++ip;
        if (!isFiniteFI32(sp[-1].i)) { cstDump(ip); VPackage::InternalFatalError("Invalid float->int conversion"); }
        {
          const vint32 itemp = (vint32)sp[-1].f;
          sp[-1].i = itemp;
        }
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
          //VName n = VName((EName)sp[-1].i);
          VName n = VName::CreateWithIndexSafe(sp[-1].i);
          sp[-1].p = nullptr;
          *(VStr *)&sp[-1].p = VStr(n);
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ClearPointedStr)
        ((VStr *)sp[-1].p)->Clean();
        ip += 1;
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ClearPointedStruct)
        ((VStruct *)ReadPtr(ip+1))->DestructObject((vuint8 *)sp[-1].p);
        ip += 1+sizeof(void *);
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_CZPointedStruct)
        ((VStruct *)ReadPtr(ip+1))->ZeroObject((vuint8 *)sp[-1].p, true); // call dtors on zeroing
        ip += 1+sizeof(void *);
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ZeroPointedStruct)
        ((VStruct *)ReadPtr(ip+1))->ZeroObject((vuint8 *)sp[-1].p, false); // don't call dtors on zeroing
        ip += 1+sizeof(void *);
        --sp;
        PR_VM_BREAK;

      // [-2] destptr
      // [-1] srcptr
      PR_VM_CASE(OPC_TypeDeepCopy)
        {
          origip = ip++;
          VFieldType stp = VFieldType::ReadTypeMem(ip);
          if (!sp[-2].p && !sp[-1].p) {
            sp -= 2;
          } else {
            if (!sp[-2].p) { cstDump(origip); VPackage::InternalFatalError("destination is nullptr"); }
            if (!sp[-1].p) { cstDump(origip); VPackage::InternalFatalError("source is nullptr"); }
            VField::CopyFieldValue((const vuint8 *)sp[-1].p, (vuint8 *)sp[-2].p, stp);
            sp -= 2;
          }
        }
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
        ASSIGNOPNN(void *, p, =);
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

      // [-2]: *dynarray
      // [-1]: index
      PR_VM_CASE(OPC_DynArrayElement)
        if (sp[-1].i < 0 || sp[-1].i >= ((VScriptArray *)sp[-2].p)->Num()) {
          cstDump(ip);
          VPackage::InternalFatalError(va("Index %d outside the bounds of an array (%d)", sp[-1].i, ((VScriptArray *)sp[-2].p)->Num()));
        }
        sp[-2].p = ((VScriptArray *)sp[-2].p)->Ptr()+sp[-1].i*ReadInt32(ip+1);
        ip += 5;
        --sp;
        PR_VM_BREAK;

      // [-2]: *dynarray
      // [-1]: index
      PR_VM_CASE(OPC_DynArrayElementB)
        if (sp[-1].i < 0 || sp[-1].i >= ((VScriptArray *)sp[-2].p)->Num()) {
          cstDump(ip);
          VPackage::InternalFatalError(va("Index %d outside the bounds of an array (%d)", sp[-1].i, ((VScriptArray *)sp[-2].p)->Num()));
        }
        sp[-2].p = ((VScriptArray *)sp[-2].p)->Ptr()+sp[-1].i*ip[1];
        ip += 2;
        --sp;
        PR_VM_BREAK;

      // [-2]: *dynarray
      // [-1]: index
      // pointer to a new element left on the stack
      PR_VM_CASE(OPC_DynArrayElementGrow)
        {
          ++ip;
          VFieldType Type = VFieldType::ReadTypeMem(ip);
          VScriptArray &A = *(VScriptArray *)sp[-2].p;
          int idx = sp[-1].i;
          if (idx < 0) { cstDump(ip); VPackage::InternalFatalError(va("Array index %d is negative", idx)); }
          if (idx >= A.length()) {
            if (A.Is2D()) { cstDump(ip); VPackage::InternalFatalError("Cannot grow 2D array"); }
            if (idx >= MaxDynArrayLength) { cstDump(ip); VPackage::InternalFatalError(va("Array index %d is too big", idx)); }
            A.SetNum(idx+1, Type);
          }
          sp[-2].p = A.Ptr()+idx*Type.GetSize();
          --sp;
        }
        PR_VM_BREAK;

      // [-1]: *dynarray
      PR_VM_CASE(OPC_DynArrayGetNum)
        ++ip;
        sp[-1].i = ((VScriptArray *)sp[-1].p)->Num();
        PR_VM_BREAK;

      // [-1]: *dynarray
      PR_VM_CASE(OPC_DynArrayGetNum1)
        ++ip;
        sp[-1].i = ((VScriptArray *)sp[-1].p)->length1();
        PR_VM_BREAK;

      // [-1]: *dynarray
      PR_VM_CASE(OPC_DynArrayGetNum2)
        ++ip;
        sp[-1].i = ((VScriptArray *)sp[-1].p)->length2();
        PR_VM_BREAK;

      // this is always followed by type and opcode
      PR_VM_CASE(OPC_DynArrayDispatch)
        {
          #include "vc_executor_builtins_dynarr.cpp"
        }
        PR_VM_BREAK;

      // [-3]: *dynarray
      // [-2]: index1
      // [-1]: index2
      // pointer to a new element left on the stack
      PR_VM_CASE(OPC_DynArrayElement2D)
        {
          VScriptArray &A = *(VScriptArray *)sp[-3].p;
          int idx1 = sp[-2].i;
          int idx2 = sp[-1].i;
          // 1d arrays can be accessed as 2d if second index is 0
          if (idx2 != 0 && !A.Is2D()) { cstDump(ip); VPackage::InternalFatalError("Cannot index 1D array as 2D"); }
          if (idx1 < 0) { cstDump(ip); VPackage::InternalFatalError(va("Array index %d is too small", idx1)); }
          if (idx1 >= A.length1()) { cstDump(ip); VPackage::InternalFatalError(va("Array index %d is too big (%d)", idx1, A.length1())); }
          if (idx2 < 0) { cstDump(ip); VPackage::InternalFatalError(va("Array size %d is too small", idx2)); }
          if (idx2 >= A.length2()) { cstDump(ip); VPackage::InternalFatalError(va("Array size %d is too big (%d)", idx2, A.length2())); }
          sp[-3].p = A.Ptr()+(idx2*A.length1()+idx1)*ReadInt32(ip+1);
          ip += 5;
          sp -= 2;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DynamicCast)
        sp[-1].p = (sp[-1].p && ((VObject *)sp[-1].p)->IsA((VClass *)ReadPtr(ip+1)) ? sp[-1].p : nullptr);
        ip += 1+sizeof(void *);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DynamicClassCast)
        sp[-1].p = (sp[-1].p && ((VClass *)sp[-1].p)->IsChildOf((VClass *)ReadPtr(ip+1)) ? sp[-1].p : nullptr);
        ip += 1+sizeof(void *);
        PR_VM_BREAK;

      // [-2]: what to cast
      // [-1]: destination class
      PR_VM_CASE(OPC_DynamicCastIndirect)
        sp[-2].p = (sp[-1].p && sp[-2].p && ((VObject *)sp[-2].p)->IsA((VClass *)sp[-1].p) ? sp[-2].p : nullptr);
        --sp;
        ++ip;
        PR_VM_BREAK;

      // [-2]: what to cast
      // [-1]: destination class
      PR_VM_CASE(OPC_DynamicClassCastIndirect)
        sp[-2].p = (sp[-1].p && sp[-2].p && ((VClass *)sp[-2].p)->IsChildOf((VClass *)sp[-1].p) ? sp[-2].p : nullptr);
        --sp;
        ++ip;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_GetDefaultObj)
        ++ip;
        if (!sp[-1].p) { cstDump(ip); VPackage::InternalFatalError("Reference not set to an instance of an object"); }
        sp[-1].p = ((VObject *)sp[-1].p)->GetClass()->Defaults;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_GetClassDefaultObj)
        ++ip;
        if (!sp[-1].p) { cstDump(ip); VPackage::InternalFatalError("Reference not set to an instance of an object"); }
        sp[-1].p = ((VClass *)sp[-1].p)->Defaults;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IteratorInit)
        ++ip;
        pushOldIterator((VScriptIterator *)sp[-1].p);
        --sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IteratorNext)
        if (iterStackUsed == 0) { cstDump(ip); VPackage::InternalFatalError("No active iterators (but we should have one)"); }
        ++ip;
        sp->i = iterStack[iterStackUsed-1]->GetNext();
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_IteratorPop)
        if (iterStackUsed == 0) { cstDump(ip); VPackage::InternalFatalError("No active iterators (but we should have one)"); }
        popOldIterator();
        ++ip;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DoPushTypePtr)
        ++ip;
        sp->p = ip;
        sp += 1;
        // skip type
        (void)VFieldType::ReadTypeMem(ip);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ZeroByPtr)
        if (sp[-1].p) {
          int bcount = ReadInt32(ip+1);
          if (bcount > 0) memset(sp[-1].p, 0, bcount);
        }
        --sp;
        ip += 1+4;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_ZeroSlotsByPtr)
        if (sp[-1].p) {
          int bcount = ReadInt32(ip+1);
          if (bcount > 0) memset(sp[-1].p, 0, bcount*sizeof(VStack));
        }
        --sp;
        ip += 1+4;
        PR_VM_BREAK;

      // [-1]: obj
      PR_VM_CASE(OPC_GetObjClassPtr)
        if (sp[-1].p) sp[-1].p = ((VObject *)sp[-1].p)->GetClass();
        ++ip;
        PR_VM_BREAK;

#define DO_ISA_CLASS(tval_, fval_)  do { \
        if (sp[-2].p && sp[-1].p) { \
          VClass *c = (VClass *)sp[-2].p; \
          sp[-2].i = (c->IsChildOf((VClass *)sp[-1].p) ? (tval_) : (fval_)); \
        } else if (sp[-2].p || sp[-1].p) { \
          /* class isa none is false */ \
          /* none isa class is false */ \
          sp[-2].i = (fval_); \
        } else { \
          /* none isa none is true */ \
          sp[-2].i = (tval_); \
        } \
        --sp; \
        ++ip; \
      } while (0)

      // [-2]: class
      // [-1]: class
      PR_VM_CASE(OPC_ClassIsAClass)
        DO_ISA_CLASS(1, 0);
        PR_VM_BREAK;

      // [-2]: class
      // [-1]: class
      PR_VM_CASE(OPC_ClassIsNotAClass)
        DO_ISA_CLASS(0, 1);
        PR_VM_BREAK;

#define DO_ISA_CLASS_NAME(tval_, fval_)  do { \
        VName n = VName::CreateWithIndexSafe(sp[-1].i); \
        if (VStr::strEquCI(*n, "none")) n = NAME_None; /*HACK*/ \
        if (sp[-2].p) { \
          VClass *c = (VClass *)sp[-2].p; \
          sp[-2].i = (c->IsChildOfByName(n) ? (tval_) : (fval_)); \
        } else { \
          /* none isa none is true */ \
          sp[-2].i = (n == NAME_None ? (tval_) : (fval_)); \
        } \
        --sp; \
        ++ip; \
      } while (0)

      // [-2]: class
      // [-1]: name
      PR_VM_CASE(OPC_ClassIsAClassName)
        DO_ISA_CLASS_NAME(1, 0);
        PR_VM_BREAK;

      // [-2]: class
      // [-1]: name
      PR_VM_CASE(OPC_ClassIsNotAClassName)
        DO_ISA_CLASS_NAME(0, 1);
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DoWriteOne)
        {
          ++ip;
          VFieldType Type = VFieldType::ReadTypeMem(ip);
          VObject::pr_stackPtr = sp;
          VObject::PR_WriteOne(Type); // this will pop everything
          if (VObject::pr_stackPtr < pr_stack) { VObject::pr_stackPtr = pr_stack; cstDump(ip); VPackage::InternalFatalError("Stack underflow in `write`"); }
          sp = VObject::pr_stackPtr;
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DoWriteFlush)
        ++ip;
        VObject::PR_WriteFlush();
        PR_VM_BREAK;

      PR_VM_CASE(OPC_Builtin)
      PR_VM_CASE(OPC_BuiltinCVar)
        {
          #include "vc_executor_builtins.cpp"
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DictDispatch)
        {
          origip = ip;
          ++ip;
          VFieldType KType = VFieldType::ReadTypeMem(ip);
          VFieldType VType = VFieldType::ReadTypeMem(ip);
          const vuint8 dcopcode = *ip++;
          #include "vc_executor_builtins_dict.cpp"
          //ExecDictOperator(origip, ip, sp, KType, VType, dcopcode);
        }
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DupPOD)
        //{ fprintf(stderr, "OPC_DupPOD at %6u in FUNCTION `%s`; sp=%d\n", (unsigned)(ip-func->Statements.Ptr()), *func->GetFullName(), (int)(sp-pr_stack)); cstDump(ip); }
        ++ip;
        sp->p = sp[-1].p; // pointer copies everything
        ++sp;
        PR_VM_BREAK;

      PR_VM_CASE(OPC_DropPOD)
        ++ip;
        --sp;
        PR_VM_BREAK;

      /*
      PR_VM_CASE(OPC_SwapPOD)
        ++ip;
        {
          void *tmp = sp[-1].p;
          sp[-1].p = sp[-2].p;
          sp[-2].p = tmp;
        }
        PR_VM_BREAK;
      */

      PR_VM_CASE(OPC_GetIsDestroyed)
        {
          ++ip;
          VObject *obj = (VObject *)sp[-1].p;
          sp[-1].i = (obj ? obj->IsGoingToDie() : 1);
        }
        PR_VM_BREAK;

      PR_VM_DEFAULT
        cstDump(ip);
        VPackage::InternalFatalError(va("Invalid opcode %d", *ip));
    }
  }
  goto func_loop;

  #ifdef VMEXEC_RUNDUMP
  printIndent(); fprintf(stderr, "LEAVING VC FUNCTION `%s`; sp=%d\n", *func->GetFullName(), (int)(sp-pr_stack)); leaveIndent();
  #endif
}


struct CurrFuncHolder {
  VMethod **place;
  VMethod *prevfunc;

  CurrFuncHolder (VMethod **aplace, VMethod *newfunc) : place(aplace), prevfunc(*aplace) { *aplace = newfunc; }
  ~CurrFuncHolder () { *place = prevfunc; place = nullptr; }
  CurrFuncHolder &operator = (const CurrFuncHolder &) = delete;
};


//==========================================================================
//
//  VObject::ExecuteFunction
//
//  ALL arguments must be pushed
//
//==========================================================================
VFuncRes VObject::ExecuteFunction (VMethod *func) {
  //fprintf(stderr, "*** VObject::ExecuteFunction: <%s>\n", *func->GetFullName());

  VFuncRes ret;

  // run function
  {
    //CurrFuncHolder cfholder(&current_func, func);
#ifdef VMEXEC_RUNDUMP
    enterIndent(); printIndent(); fprintf(stderr, "***ENTERING `%s` (RETx); sp=%d (max2:%u)\n", *func->GetFullName(), (int)(pr_stackPtr-pr_stack), (unsigned)MAX_PROG_STACK);
#endif
    RunFunction(func);
  }

  // get return value
  if (func->ReturnType.Type) {
    const int tsz = func->ReturnType.GetStackSize();
    switch (func->ReturnType.Type) {
      case TYPE_Void: abort(); // the thing that should not be
      case TYPE_Int: vassert(tsz == 1); ret = VFuncRes(pr_stackPtr[-1].i); break;
      case TYPE_Byte: vassert(tsz == 1); ret = VFuncRes(pr_stackPtr[-1].i); break;
      case TYPE_Bool: vassert(tsz == 1); ret = VFuncRes(pr_stackPtr[-1].i); break;
      case TYPE_Float: vassert(tsz == 1); ret = VFuncRes(pr_stackPtr[-1].f); break;
      case TYPE_Name: vassert(tsz == 1); ret = VFuncRes(*(VName *)(&pr_stackPtr[-1])); break;
      case TYPE_String: vassert(tsz == 1); ret = VFuncRes(*(VStr *)(&pr_stackPtr[-1].p)); ((VStr *)(&pr_stackPtr[-1].p))->clear(); break;
      case TYPE_Reference: vassert(tsz == 1); ret = VFuncRes((VClass *)(pr_stackPtr[-1].p)); break;
      case TYPE_Class: vassert(tsz == 1); ret = VFuncRes((VObject *)(pr_stackPtr[-1].p)); break;
      case TYPE_State: vassert(tsz == 1); ret = VFuncRes((VState *)(pr_stackPtr[-1].p)); break;
      case TYPE_Vector: vassert(tsz == 3); ret = VFuncRes(pr_stackPtr[-3].f, pr_stackPtr[-2].f, pr_stackPtr[-1].f); break;
      default: break;
    }
    pr_stackPtr -= tsz;
    /*
    if (tsz == 1) {
      --pr_stackPtr;
      ret = *pr_stackPtr;
      //FIXME
      pr_stackPtr->p = nullptr; // for strings, caller should take care of releasing a string
    } else {
      pr_stackPtr -= tsz;
    }
    */
  }
#ifdef VMEXEC_RUNDUMP
  printIndent(); fprintf(stderr, "***LEAVING `%s` (RETx); sp=%d, (max2:%u)\n", *func->GetFullName(), (int)(pr_stackPtr-pr_stack), (unsigned)MAX_PROG_STACK); leaveIndent();
#endif

#ifdef CHECK_FOR_EMPTY_STACK
  // after executing base function stack must be empty
  if (!current_func && pr_stackPtr != pr_stack+1) {
    cstDump(nullptr);
    VPackage::InternalFatalError(va("ExecuteFunction: Stack is not empty after executing function:\n%s\nstack=%p, oldsp=%p, diff=%d", *func->Name, pr_stack, pr_stackPtr, (int)(ptrdiff_t)(pr_stack-pr_stackPtr)));
    #ifdef VMEXEC_RUNDUMP
    abort();
    #endif
  }
#endif

#ifdef CHECK_STACK_UNDERFLOW
  // check if stack wasn't underflowed
  if (pr_stack[0].i != STACK_ID) {
    cstDump(nullptr);
    VPackage::InternalFatalError(va("ExecuteFunction: Stack underflow in %s", *func->Name));
    #ifdef VMEXEC_RUNDUMP
    abort();
    #endif
  }
#endif

#ifdef CHECK_STACK_OVERFLOW
  // check if stack wasn't overflowed
  if (pr_stack[MAX_PROG_STACK-1].i != STACK_ID) {
    cstDump(nullptr);
    VPackage::InternalFatalError(va("ExecuteFunction: Stack overflow in `%s`", *func->Name));
    #ifdef VMEXEC_RUNDUMP
    abort();
    #endif
  }
#endif

  // all done
  return ret;
}


// this struct is used to hold temp strings
// we're doing it this way to avoid calling ctors/dtors each time
// should be kept in sync with corelib (or even moved there)
struct VStrPool {
private:
  unsigned used;
  void *pool[VMethod::MAX_PARAMS];

public:
  VV_DISABLE_COPY(VStrPool)
  inline VStrPool () noexcept : used(0) {}
  inline ~VStrPool () { clear(); }

  inline void clear () {
    for (unsigned f = 0; f < used; ++f) ((VStr *)(&pool[f]))->clear();
    used = 0;
  }

  inline VStr *alloc () noexcept {
    if (used == VMethod::MAX_PARAMS) VPackage::InternalFatalError("out of strings in `VStrPool`");
    // this is valid initialisation for VStr
    VStr *res = (VStr *)(&pool[used++]);
    memset((void *)res, 0, sizeof(VStr));
    return res;
  }
};

static_assert(sizeof(VStr) == sizeof(void *), "invalid VStr size");


// for simple types w/o ctor/dtor
template<typename T> struct VSimpleTypePool {
private:
  unsigned used;
  T pool[VMethod::MAX_PARAMS*3]; // for vectors

public:
  VV_DISABLE_COPY(VSimpleTypePool)
  inline VSimpleTypePool () noexcept : used(0) {}
  inline ~VSimpleTypePool () noexcept { used = 0; }

  inline void clear () noexcept { used = 0; }

  inline T *alloc () noexcept {
    if (used == VMethod::MAX_PARAMS*3) VPackage::InternalFatalError("out of room in `VSimpleTypePool`");
    // this is valid initialisation
    T *res = &pool[used++];
    memset((void *)res, 0, sizeof(T));
    return res;
  }

  // for vectors/delegates
  inline T *nalloc (unsigned count) noexcept {
    vassert(count > 0 && count < VMethod::MAX_PARAMS);
    if (used+count > VMethod::MAX_PARAMS*3) VPackage::InternalFatalError("out of room in `VSimpleTypePool`");
    // this is valid initialisation
    T *res = &pool[used];
    memset((void *)res, 0, sizeof(T)*count);
    used += count;
    return res;
  }
};


//==========================================================================
//
//  VObject::ExecuteFunctionNoArgs
//
//==========================================================================
VFuncRes VObject::ExecuteFunctionNoArgs (VObject *Self, VMethod *func, bool allowVMTLookups) {
  if (!func) VPackage::InternalFatalError("ExecuteFunctionNoArgs: null func!");
  if (func->VTableIndex < -1) VPackage::InternalFatalError(va("method `%s` in class `%s` wasn't postloaded (%d)", *func->GetFullName(), (Self ? Self->GetClass()->GetName() : "<unknown>"), func->VTableIndex));

  if (!(func->Flags&FUNC_Static)) {
    if (!Self) VPackage::InternalFatalError(va("trying to call method `%s` without an object", *func->GetFullName()));
    // for virtual functions, check for correct class
    VClass *origClass = func->GetSelfClass();
    // for state wrapper, the class can be absent
    if (origClass) {
      // check for a valid class
      if (!Self->IsA(origClass)) {
        VPackage::InternalFatalError(va("object of class `%s` is not a subclass of `%s` for method `%s`", Self->GetClass()->GetName(), origClass->GetName(), *func->GetFullName()));
      }
    }
    /*
    if (!origClass) {
      #if 0
      if (Self) {
        GLog.Logf(NAME_Debug, "=== %s ===", Self->GetClass()->GetName());
        for (VClass *cc = Self->GetClass(); cc; cc = cc->GetSuperClass()) GLog.Logf(NAME_Debug, "  %s", *cc->GetFullName());
      }
      origClass = func;
      while (origClass) {
        GLog.Logf(NAME_Debug, ": %s (%s)", *origClass->GetFullName(), origClass->GetMemberTypeString());
        origClass = origClass->Outer;
      }
      #endif
      VPackage::InternalFatalError(va("trying to call method `%s` which doesn't belong to a class, or to a state (self is `%s`)", *func->GetFullName(), (Self ? Self->GetClass()->GetName() : "<none>")));
    }
    */
    // push `self`
    P_PASS_REF(Self);
    //GLog.Logf(NAME_Debug, "VObject::ExecuteFunctionNoArgs: class=`%s`; method=`%s`; allowVMT=%d; VTableIndex=%d", *Self->GetClass()->GetFullName(), *func->GetFullName(), (int)allowVMTLookups, func->VTableIndex);
  } else {
    vassert(func->VTableIndex == -1);
  }

  // route it with VMT
  if (func->VTableIndex >= 0) {
    if (allowVMTLookups) {
      func = Self->vtable[func->VTableIndex];
    } else {
      VPackage::InternalFatalError(va("trying to call virtual function `%s` in class `%s`, but VMT lookups are disabled", *func->GetFullName(), *Self->GetClass()->GetFullName()));
    }
  }

  if (func->NumParams > VMethod::MAX_PARAMS) VPackage::InternalFatalError(va("ExecuteFunctionNoArgs: function `%s` has too many parameters (%d)", *func->GetFullName(), func->NumParams)); // sanity check

  // placeholders for "ref" args
  VSimpleTypePool<int32_t> rints;
  VSimpleTypePool<float> rfloats; // for vectors too
  VSimpleTypePool<VName> rnames; // for vectors too
  VSimpleTypePool<void *> rptrs; // various pointers (including delegates)
  VStrPool rstrs;

  // push default values
  for (int f = 0; f < func->NumParams; ++f) {
    // out/ref arg
    if ((func->ParamFlags[f]&(FPARM_Out|FPARM_Ref)) != 0) {
      if (func->ParamTypes[f].IsAnyArrayOrStruct()) VPackage::InternalFatalError(va("ExecuteFunctionNoArgs: function `%s`, argument #%d is ref/out array/struct, this is not supported yet", *func->GetFullName(), f+1));
      switch (func->ParamTypes[f].Type) {
        case TYPE_Int:
        case TYPE_Byte:
        case TYPE_Bool:
          P_PASS_PTR(rints.alloc());
          break;
        case TYPE_Float:
          P_PASS_PTR(rfloats.alloc());
          break;
        case TYPE_Name:
          P_PASS_PTR(rnames.alloc());
          break;
        case TYPE_String:
          P_PASS_PTR(rstrs.alloc());
          break;
        case TYPE_Pointer:
        case TYPE_Reference:
        case TYPE_Class:
        case TYPE_State:
          P_PASS_PTR(rptrs.alloc());
          break;
        case TYPE_Vector:
          P_PASS_PTR(rfloats.nalloc(3));
          break;
        case TYPE_Delegate:
          P_PASS_PTR(rptrs.nalloc(2));
          break;
        default:
          VPackage::InternalFatalError(va("ExecuteFunctionNoArgs: function `%s`, argument #%d is of bad type `%s`", *func->GetFullName(), f+1, *func->ParamTypes[f].GetName()));
          break;
      }
      if ((func->ParamFlags[f]&FPARM_Optional) != 0) P_PASS_BOOL(false); // "specified" flag
    } else {
      if ((func->ParamFlags[f]&FPARM_Optional) == 0) VPackage::InternalFatalError(va("ExecuteFunctionNoArgs: function `%s`, argument #%d is not optional!", *func->GetFullName(), f+1));
      // push empty values
      switch (func->ParamTypes[f].Type) {
        case TYPE_Int:
        case TYPE_Byte:
        case TYPE_Bool:
          P_PASS_INT(0);
          break;
        case TYPE_Float:
          P_PASS_FLOAT(0);
          break;
        case TYPE_Name:
          P_PASS_NAME(NAME_None);
          break;
        case TYPE_String:
          P_PASS_STR(VStr());
          break;
        case TYPE_Pointer:
        case TYPE_Reference:
        case TYPE_Class:
        case TYPE_State:
          P_PASS_PTR(nullptr);
          break;
        case TYPE_Vector:
          P_PASS_VEC(TVec(0, 0, 0));
          break;
        case TYPE_Delegate:
          P_PASS_PTR(nullptr);
          P_PASS_PTR(nullptr);
          break;
        default:
          VPackage::InternalFatalError(va("ExecuteFunctionNoArgs: function `%s`, argument #%d is of bad type `%s`", *func->GetFullName(), f+1, *func->ParamTypes[f].GetName()));
          break;
      }
      P_PASS_BOOL(false); // "specified" flag
    }
  }

  return ExecuteFunction(func);
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
//  VObject::VMDumpCallStackToStdErr
//
//==========================================================================
void VObject::VMDumpCallStackToStdErr () {
  cstDump(nullptr, true);
}


//==========================================================================
//
//  VObject::DumpProfile
//
//==========================================================================
void VObject::DumpProfile () {
  DumpProfileInternal(-1);
  DumpProfileInternal(1);
}


// ////////////////////////////////////////////////////////////////////////// //
// sorter
static int profCmpTimes (const void *a, const void *b, void * /*udata*/) {
  const VMethod::ProfileInfo *f1 = &(*(const VMethod **)a)->Profile;
  const VMethod::ProfileInfo *f2 = &(*(const VMethod **)b)->Profile;
  if (f1 == f2) return 0;
  if (f1->totalTime == f2->totalTime) {
    if (f1->callCount == f2->callCount) return 0;
    return (f1->callCount > f2->callCount ? -1 : 1);
  }
  return (f1->totalTime > f2->totalTime ? -1 : 1);
}


//==========================================================================
//
//  nano2sec
//
//==========================================================================
static inline double nano2sec (vuint64 nano) {
  //return nano/1000000000.0;
  double res = nano/1000000000ULL;
  int ns = (int)(nano%1000000000ULL);
  ns /= 1000000;
  res += (double)ns/1000.0;
  return res;
}


//==========================================================================
//
//  VObject::DumpProfileInternal
//
//  <0: only native; >0: only script; 0: everything
//
//==========================================================================
void VObject::DumpProfileInternal (int type) {
  TArray<VMethod *> funclist;
  // collect
  for (int i = 0; i < VMemberBase::GMembers.length(); ++i) {
    if (VMemberBase::GMembers[i]->MemberType != MEMBER_Method) continue;
    VMethod *func = (VMethod *)VMemberBase::GMembers[i];
    if (!func->Profile.callCount) continue; // never called
    // check type
    if (type < 0 && (func->Flags&FUNC_Native) == 0) continue;
    if (type > 0 && (func->Flags&FUNC_Native) != 0) continue;
    funclist.append(func);
  }
  if (funclist.length() == 0) return;
  timsort_r(funclist.ptr(), funclist.length(), sizeof(VMethod *), &profCmpTimes, nullptr);

  const char *ptypestr = (type < 0 ? "native" : type > 0 ? "script" : "all");
  GLog.Logf("====== PROFILE (%s) ======", ptypestr);
  GLog.Log("....calls... .tottime. .mintime. .maxtime. name");
  for (auto &&func : funclist) {
    GLog.Logf("%12u %9.3f %9.3f %9.3f %s", func->Profile.callCount, nano2sec(func->Profile.totalTime), nano2sec(func->Profile.minTime), nano2sec(func->Profile.maxTime), *func->GetFullName());
  }
}


//==========================================================================
//
//  VObject::ClearProfiles
//
//==========================================================================
void VObject::ClearProfiles () {
  for (int i = 0; i < VMemberBase::GMembers.length(); ++i) {
    if (VMemberBase::GMembers[i]->MemberType != MEMBER_Method) continue;
    VMethod *func = (VMethod *)VMemberBase::GMembers[i];
    func->Profile.clear();
  }
}
