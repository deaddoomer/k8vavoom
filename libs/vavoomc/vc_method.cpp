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
#include "vc_local.h"
#include "vc_mcopt.h"

//#define VCM_DEBUG_DEBUGINFO


// ////////////////////////////////////////////////////////////////////////// //
#define BYTES_CODE_POOL_SIZE   (1024*1024*1)
#define BYTES_DEBUG_POOL_SIZE  (1024*1024*1/4)

struct VMBytesPool {
  vuint8* code; // points after the pool end
  size_t used;
  size_t alloted;
  VMBytesPool *next;
};


struct VMPoolInfo {
  VMBytesPool* head;
  VMBytesPool* tail;
  size_t stpos;
  unsigned lastline;
  unsigned lastfile;
  unsigned lastwasnorm; // 666: initial write
  size_t poolSize;

  inline VMPoolInfo (const size_t apoosize) noexcept
    : head(nullptr)
    , tail(nullptr)
    , stpos(~(size_t)0)
    , lastline(0u)
    , lastfile(0u)
    , lastwasnorm(0u)
    , poolSize(apoosize) {}
};


static VMPoolInfo vmCodePool = VMPoolInfo(BYTES_CODE_POOL_SIZE);
static VMPoolInfo vmDebugPool = VMPoolInfo(BYTES_DEBUG_POOL_SIZE);


//==========================================================================
//
//  vmStartPool
//
//==========================================================================
static void vmStartPool (VMPoolInfo* nfo) {
  vassert(nfo->stpos == ~(size_t)0);
  if (!nfo->tail) {
    vassert(!nfo->head);
    VMBytesPool* pp = (VMBytesPool*)Z_Malloc(sizeof(VMBytesPool)+nfo->poolSize+4);
    vassert(pp);
    pp->code = (vuint8*)(pp+1);
    pp->used = 0;
    pp->alloted = nfo->poolSize;
    pp->next = nullptr;
    nfo->head = nfo->tail = pp;
  }
  nfo->stpos = nfo->tail->used;
}


//==========================================================================
//
//  vmEndPool
//
//==========================================================================
static vuint8 *vmEndPool (VMPoolInfo* nfo, vuint32 *dbgsize) {
  vassert(nfo->head);
  vassert(nfo->tail);
  vassert(!nfo->tail->next);
  vassert(nfo->stpos != ~(size_t)0);
  if (dbgsize) *dbgsize = (vuint32)(nfo->tail->used-nfo->stpos);
  vuint8 *res = nfo->tail->code+nfo->stpos;
  nfo->stpos = ~(size_t)0;
  return res;
}


//==========================================================================
//
//  vmCodeOffset
//
//==========================================================================
static int vmCodeOffset (const VMPoolInfo *nfo) {
  vassert(nfo->stpos != ~(size_t)0);
  return (int)(nfo->tail->used-nfo->stpos);
}


//==========================================================================
//
//  vmEmitBytes
//
//==========================================================================
static void vmEmitBytes (VMPoolInfo* nfo, const void *buf, const unsigned len) {
  if (!len) return;
  vassert(len < nfo->poolSize);
  vassert(nfo->stpos != ~(size_t)0);
  VMBytesPool* pool = nfo->tail;
  // if we don't have enough room, allocate a new pool, and move the code there
  if (pool->alloted-pool->used < len) {
    const size_t codesz = pool->used-nfo->stpos;
    vassert(codesz+len <= nfo->poolSize);
    VMBytesPool* pp = (VMBytesPool*)Z_Malloc(sizeof(VMBytesPool)+nfo->poolSize+4);
    vassert(pp);
    pp->code = (vuint8*)(pp+1);
    pp->used = 0;
    pp->alloted = nfo->poolSize;
    pp->next = nullptr;
    nfo->tail->next = pp;
    nfo->tail = pp;
    if (codesz) memcpy(pp->code, pool->code+nfo->stpos, codesz);
    pp->used = codesz;
    pool = pp;
    nfo->stpos = 0;
  }
  vassert(pool->alloted-pool->used >= len);
  memcpy(pool->code+pool->used, buf, len);
  pool->used += len;
}


static VVA_OKUNUSED VVA_FORCEINLINE void vmEmitUByte (VMPoolInfo* nfo, const vuint8 v) { vmEmitBytes(nfo, &v, 1); }
static VVA_OKUNUSED VVA_FORCEINLINE void vmEmitUShort (VMPoolInfo* nfo, const vuint16 v) { vmEmitBytes(nfo, &v, 2); }
static VVA_OKUNUSED VVA_FORCEINLINE void vmEmitUInt (VMPoolInfo* nfo, const vuint32 v) { vmEmitBytes(nfo, &v, 4); }
static VVA_OKUNUSED VVA_FORCEINLINE void vmEmitPtr (VMPoolInfo* nfo, const void *ptr) { vmEmitBytes(nfo, &ptr, sizeof(void*)); }

static VVA_OKUNUSED VVA_FORCEINLINE void vmEmitSShort (VMPoolInfo* nfo, const vint16 v) { vmEmitBytes(nfo, &v, 2); }
static VVA_OKUNUSED VVA_FORCEINLINE void vmEmitSInt (VMPoolInfo* nfo, const vint32 v) { vmEmitBytes(nfo, &v, 4); }

static void vmPatchBytes (VMPoolInfo* nfo, const int offset, const void *buf, const unsigned len) {
  if (!len) return;
  vassert(len < nfo->poolSize);
  vassert(offset >= 0 && offset < (int)nfo->poolSize);
  vassert(nfo->stpos != ~(size_t)0);
  VMBytesPool* pool = nfo->tail;
  const size_t csize = (nfo->tail->used-nfo->stpos);
  vassert((unsigned)offset < csize && (unsigned)offset+len <= csize);
  memcpy(pool->code+nfo->stpos+(unsigned)offset, buf, len);
}

//static VVA_OKUNUSED VVA_FORCEINLINE void vmPutUByte (VMPoolInfo* nfo, const int offset, const vuint8 v) { vmPutBytes(nfo, offset, &v, 1); }
//static VVA_OKUNUSED VVA_FORCEINLINE void vmPutUShort (VMPoolInfo* nfo, const int offset, const vuint16 v) { vmPutBytes(nfo, offset, &v, 2); }
//static VVA_OKUNUSED VVA_FORCEINLINE void vmPutUInt (VMPoolInfo* nfo, const int offset, const vuint32 v) { vmPutBytes(nfo, offset, &v, 4); }
//static VVA_OKUNUSED VVA_FORCEINLINE void vmPutPtr (VMPoolInfo* nfo, const int offset, const void *ptr) { vmPutBytes(nfo, offset, &ptr, sizeof(void*)); }

static VVA_OKUNUSED VVA_FORCEINLINE void vmPatchSByte (VMPoolInfo* nfo, const int offset, const vint8 v) { vmPatchBytes(nfo, offset, &v, 1); }
static VVA_OKUNUSED VVA_FORCEINLINE void vmPatchSShort (VMPoolInfo* nfo, const int offset, const vint16 v) { vmPatchBytes(nfo, offset, &v, 2); }
static VVA_OKUNUSED VVA_FORCEINLINE void vmPatchSInt (VMPoolInfo* nfo, const int offset, const vint32 v) { vmPatchBytes(nfo, offset, &v, 4); }


// ////////////////////////////////////////////////////////////////////////// //
unsigned VMethod::GetCodePoolCount () noexcept {
  unsigned res = 0;
  for (const VMBytesPool *pool = vmCodePool.head; pool; pool = pool->next) ++res;
  return res;
}

size_t VMethod::GetTotalCodePoolSize () noexcept {
  size_t res = 0;
  for (const VMBytesPool *pool = vmCodePool.head; pool; pool = pool->next) res += pool->alloted;
  return res;
}

unsigned VMethod::GetDebugPoolCount () noexcept {
  unsigned res = 0;
  for (const VMBytesPool *pool = vmDebugPool.head; pool; pool = pool->next) ++res;
  return res;
}

size_t VMethod::GetTotalDebugPoolSize () noexcept {
  size_t res = 0;
  for (const VMBytesPool *pool = vmDebugPool.head; pool; pool = pool->next) res += pool->alloted;
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
/*
  debug info header:
    dw fileIndex
    dw firstline

  debug info element:
    db byteline

    byteline is:
      low 4 bits is command length; if 0 -- this is a special command
      high 4 bits is line offset from the previous record (can be 0)

  special commands when byte is 0 is stored in high 4 bits:
    db 0: no more debug data
    db 1: file index change; next 2 bytes is new file index (65535 means "no more data")
    db 2: next 2 bytes is new line index
    db 3: next 2 bytes is line add
    db 4: (actually, any other value)
    otherwise just add the next byte to line index
 */

//==========================================================================
//
//  vmStartDebugPool
//
//==========================================================================
static inline void vmStartDebugPool (VMPoolInfo* nfo) {
  vmStartPool(nfo);
  vmEmitUShort(nfo, 0); // fileIndex
  vmEmitUShort(nfo, 0); // firstLine
  nfo->lastline = 0u;
  nfo->lastfile = 0u;
  nfo->lastwasnorm = 666u;
}


//==========================================================================
//
//  vmEndDebugPool
//
//==========================================================================
static inline void *vmEndDebugPool (VMPoolInfo* nfo, vuint32 *dbgsize) {
  vmEmitUByte(nfo, 0); // end of data flag
  return vmEndPool(nfo, dbgsize);
}


//==========================================================================
//
//  vmDebugEmitLoc
//
//==========================================================================
static void vmDebugEmitLoc (VMPoolInfo* nfo, int len, const TLocation &loc) {
  if (!len) return;
  vassert(len > 0);

  const vuint32 ln = (vuint32)loc.GetLine();
  vuint32 sidx = (vuint32)loc.GetSrcIndex();
  if (sidx > 65534) sidx = 0; // this cannot happen, but well...

  do {
    const unsigned lastWN = nfo->lastwasnorm;
    nfo->lastwasnorm = 0u;

    // fix initial file index
    if (lastWN == 666u) {
      vuint16 *nptr = (vuint16*)(nfo->tail->code+nfo->stpos);
      // set first file index
      nptr[0] = sidx;
      nfo->lastfile = sidx;
      // set first line index
      nptr[1] = (ln < 65536 ? ln : 65535);
      nfo->lastline = nptr[1];
      //GLog.Logf(NAME_Debug, "0x%08x: LASTFILE=%u; LASTLINE=%u; 0x%08x", (unsigned)nptr, nfo->lastfile, nfo->lastline, (unsigned)(nfo->tail->code+nfo->tail->used));
    }

    #if 0
    // this is roughly what the previous code did
    // it was storing 3 ints for each VM instruction
    vmEmitUInt(nfo, 0);
    vmEmitUInt(nfo, 0);
    vmEmitUInt(nfo, 0);
    return;
    #endif

    // check if we need to record a new file index
    if (nfo->lastfile != sidx) {
      nfo->lastfile = sidx;
      // new file index command
      vmEmitUByte(nfo, 0x10);
      vmEmitUShort(nfo, (vuint16)sidx);
      continue;
    }

    // need to emit new line?
    if (ln < nfo->lastline) {
      nfo->lastline = (ln < 65536 ? ln : 65535);
      // newline command
      vmEmitUByte(nfo, 0x20);
      vmEmitUShort(nfo, (vuint16)nfo->lastline);
      continue;
    }

    // need to emit line shift?
    vuint32 sft = ln-nfo->lastline;
    if (sft > 15) {
      if (sft > 255) {
        // big lineshift command
        vmEmitUByte(nfo, 0x30);
        if (sft > 65535) sft = 65535;
        vmEmitUShort(nfo, (vuint16)sft);
      } else {
        // small lineshift command
        vmEmitUByte(nfo, 0x40);
        vmEmitUByte(nfo, (vuint8)sft);
      }
      nfo->lastline += sft;
      continue;
    }

    vuint8 *ccp = nfo->tail->code+nfo->tail->used-1;
    // check if we can merge command lengthes
    if (lastWN == 1u && sft == 0 && (ccp[0]&0x0f)+len < 16) {
      // fix old command
      *ccp += len; // this will never overflow
      vassert(len > 0 && len < 15);
      len = 0;
    } else {
      vassert(sft < 16);
      vassert(len > 0);
      // emit line and command length
      vuint8 cbt = (len > 15 ? 15 : len);
      cbt |= (sft<<4);
      vmEmitUByte(nfo, cbt);
      len -= cbt;
    }

    nfo->lastwasnorm = 1u;
    nfo->lastline += sft;
  } while (len > 0);
}


//==========================================================================
//
//  vmDebugFindLocForOfs
//
//==========================================================================
static TLocation vmDebugFindLocForOfs (const void *debugInfo, size_t pc) {
  if (!debugInfo) return TLocation();
  const vuint8* pp = (const vuint8 *)debugInfo;
  //GLog.Logf(NAME_Debug, "0x%08x: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x", (unsigned)pp, pp[0], pp[1], pp[2], pp[3], pp[4]);
  vuint16 fidx = *(const vuint16 *)pp; pp += 2;
  vuint32 lidx = *(const vuint16 *)pp; pp += 2;
  #ifdef VCM_DEBUG_DEBUGINFO
  GLog.Logf(NAME_Debug, "0x%08x: fidx=%u; lidx=%u", (unsigned)(pp-4), fidx, lidx);
  #endif
  for (;;) {
    vuint8 bb = *pp++;
    #ifdef VCM_DEBUG_DEBUGINFO
    GLog.Logf(NAME_Debug, "  0x%08x: bb=0x%02x pc=%u; fidx=%u; lidx=%u", (unsigned)(pp-1), bb, (unsigned)pc, fidx, lidx);
    #endif
    // special command?
    if ((bb&0x0f) == 0) {
      if (!bb) break;
      // new file index?
      if (bb == 0x10) {
        memcpy(&fidx, pp, 2);
        pp += 2;
        #ifdef VCM_DEBUG_DEBUGINFO
        GLog.Logf(NAME_Debug, "    NEW FIDX: %u", fidx);
        #endif
        continue;
      }
      // new line index?
      if (bb == 0x20) {
        vuint16 nlx;
        memcpy(&nlx, pp, 2);
        pp += 2;
        lidx = nlx;
        #ifdef VCM_DEBUG_DEBUGINFO
        GLog.Logf(NAME_Debug, "    NEW LIDX: %u", lidx);
        #endif
        continue;
      }
      // long line offset?
      if (bb == 0x30) {
        vuint16 nlx;
        memcpy(&nlx, pp, 2);
        pp += 2;
        lidx += nlx;
        #ifdef VCM_DEBUG_DEBUGINFO
        GLog.Logf(NAME_Debug, "    LONGOFS: %u", nlx);
        #endif
        continue;
      }
      // short line offset
      lidx += *pp++;
      #ifdef VCM_DEBUG_DEBUGINFO
      GLog.Logf(NAME_Debug, "    SHORTOFS: %u", bb);
      #endif
      continue;
    }
    // adjust line
    lidx += (bb>>4);
    // check pc
    if (pc < (bb&0x0f)) {
      // i found her!
      return TLocation((int)fidx, (int)lidx, 1);
    }
    #ifndef VCM_DEBUG_DEBUGINFO
    pc -= (bb&0x0f);
    #endif
  }
  return TLocation();
}


// ////////////////////////////////////////////////////////////////////////// //
FBuiltinInfo *FBuiltinInfo::Builtins;


//==========================================================================
//
//  FBuiltinInfo::FBuiltinInfo
//
//==========================================================================
FBuiltinInfo::FBuiltinInfo (const char *InName, VClass *InClass, builtin_t InFunc)
  : Name(InName)
  , OuterClass(InClass)
  , OuterClassName(nullptr)
  , OuterStructName(nullptr)
  , Func(InFunc)
  , used(false)
{
  Next = Builtins;
  Builtins = this;
}


//==========================================================================
//
//  FBuiltinInfo::FBuiltinInfo
//
//==========================================================================
FBuiltinInfo::FBuiltinInfo (const char *InName, const char *InClassName, const char *InStructName, builtin_t InFunc)
  : Name(InName)
  , OuterClass(nullptr)
  , OuterClassName(InClassName)
  , OuterStructName(InStructName)
  , Func(InFunc)
  , used(false)
{
  Next = Builtins;
  Builtins = this;
}



//==========================================================================
//
//  PF_Fixme
//
//==========================================================================
static void PF_Fixme () {
  VObject::VMDumpCallStack();
  VCFatalError("unimplemented bulitin");
}



//==========================================================================
//
//  VMethodParam::clear
//
//  this has to be non-inline due to `delete TypeExpr`
//
//==========================================================================
void VMethodParam::clear () {
  delete TypeExpr; TypeExpr = nullptr;
  // don't clear name, we'll need it
  //Name = NAME_None;
  //Loc = TLocation();
}


//==========================================================================
//
//  VMethod::VMethod
//
//==========================================================================
VMethod::VMethod (VName AName, VMemberBase *AOuter, TLocation ALoc)
  : VMemberBase(MEMBER_Method, AName, AOuter, ALoc)
  , mPostLoaded(false)
  , NumLocals(0)
  , Flags(0)
  , ReturnType(TYPE_Void)
  , NumParams(0)
  , ParamsSize(0)
  , SuperMethod(nullptr)
  , ReplCond(nullptr)
  , ReturnTypeExpr(nullptr)
  , Statement(nullptr)
  , SelfTypeName(NAME_None)
  , lmbCount(0)
  , printfFmtArgIdx(-1)
  , builtinOpc(-1)
  , Profile()
  , vmCodeStart(nullptr)
  , vmDebugInfo(nullptr)
  , vmCodeSize(0)
  , vmDebugInfoSize(0)
  , NativeFunc(0)
  , VTableIndex(-666)
  , NetIndex(0)
  , NextNetMethod(nullptr)
  , SelfTypeClass(nullptr)
  , defineResult(-1)
  , emitCalled(false)
  , jited(false)
{
  memset(ParamFlags, 0, sizeof(ParamFlags));
}


//==========================================================================
//
//  VMethod::~VMethod
//
//==========================================================================
VMethod::~VMethod() {
  delete ReturnTypeExpr; ReturnTypeExpr = nullptr;
  delete Statement; Statement = nullptr;
}


//==========================================================================
//
//  VMethod::CompilerShutdown
//
//==========================================================================
void VMethod::CompilerShutdown () {
  VMemberBase::CompilerShutdown();
  for (int f = 0; f < NumParams; ++f) Params[f].clear();
  delete ReturnTypeExpr; ReturnTypeExpr = nullptr;
  delete Statement; Statement = nullptr;
}


//==========================================================================
//
//  VMethod::Define
//
//==========================================================================
bool VMethod::Define () {
  if (emitCalled) VCFatalError("`Define()` after `Emit()` for %s", *GetFullName());
  if (defineResult >= 0) {
    if (defineResult != 666) GLog.Logf(NAME_Debug, "`Define()` for `%s` already called!", *GetFullName());
    return (defineResult > 0);
  }
  defineResult = 0;

  bool Ret = true;

  if (Flags&FUNC_Static) {
    if ((Flags&FUNC_Final) == 0) {
      ParseError(Loc, "Currently static methods must be final");
      Ret = false;
    }
  }

  if ((Flags&FUNC_VarArgs) != 0 && (Flags&FUNC_Native) == 0) {
    ParseError(Loc, "Only native methods can have varargs");
    Ret = false;
  }

  if ((Flags&(FUNC_Iterator|FUNC_Native)) == FUNC_Iterator) {
    ParseError(Loc, "Iterators can only be native");
    Ret = false;
  }

  VEmitContext ec(this);
  if (Flags&FUNC_NoVCalls) ec.VCallsDisabled = true;

  if (ReturnTypeExpr) ReturnTypeExpr = ReturnTypeExpr->ResolveAsType(ec);
  if (ReturnTypeExpr) {
    VFieldType t = ReturnTypeExpr->Type;
    if (t.Type != TYPE_Void) {
      // function's return type must be void, vector or with size 4
      t.CheckReturnable(ReturnTypeExpr->Loc);
    }
    ReturnType = t;
  } else {
    Ret = false;
  }

  // resolve parameters types
  ParamsSize = (Flags&FUNC_Static ? 0 : 1); // first is `self`
  for (int i = 0; i < NumParams; ++i) {
    VMethodParam &P = Params[i];

    if (P.TypeExpr) P.TypeExpr = P.TypeExpr->ResolveAsType(ec);
    if (!P.TypeExpr) { Ret = false; continue; }
    VFieldType type = P.TypeExpr->Type;

    if (type.Type == TYPE_Void) {
      ParseError(P.TypeExpr->Loc, "Bad variable type");
      Ret = false;
      continue;
    }

    ParamTypes[i] = type;
    //if ((ParamFlags[i]&FPARM_Optional) != 0 && (ParamFlags[i]&FPARM_Out) != 0) ParseError(P.Loc, "Modifiers `optional` and `out` are mutually exclusive");
    //if ((ParamFlags[i]&FPARM_Optional) != 0 && (ParamFlags[i]&FPARM_Ref) != 0) ParseError(P.Loc, "Modifiers `optional` and `ref` are mutually exclusive");
    if ((ParamFlags[i]&FPARM_Out) != 0 && (ParamFlags[i]&FPARM_Ref) != 0) {
      ParseError(P.Loc, "Modifiers `out` and `ref` are mutually exclusive");
    }

    if (ParamFlags[i]&(FPARM_Out|FPARM_Ref)) {
      ParamsSize += 1;
    } else {
      if (!type.CheckPassable(P.TypeExpr->Loc, false)) {
        ParseError(P.TypeExpr->Loc, "Invalid parameter #%d type '%s' in method '%s'", i+1,
                   *type.GetName(), *GetFullName());
      }
      ParamsSize += type.GetStackSize();
    }
    if (ParamFlags[i]&FPARM_Optional) ParamsSize += 1;
  }

  // if this is a overridden method, verify that return type and argument types match
  SuperMethod = nullptr;
  if (Outer->MemberType == MEMBER_Class && Name != NAME_None && ((VClass *)Outer)->ParentClass) {
    SuperMethod = ((VClass *)Outer)->ParentClass->FindMethod(Name);
  }

  if (SuperMethod) {
    // class order may be wrong, so define supermethod manually
    // this can happen if include order in "classes.vc" is wrong
    // it usually doesn't hurt, so let it work this way too
    // actually, this should not happen, because package sorts its classes,
    // but let's play safe here
    if (!SuperMethod->IsDefined()) {
      if (SuperMethod->Outer->MemberType == MEMBER_Class) {
        VClass *cc = (VClass *)SuperMethod->Outer;
        if (!cc->Defined) ParseError(Loc, "Defining virtual method `%s`, but parent class `%s` is not processed (internal compiler error)", *GetFullName(), *cc->GetFullName());
      }
      SuperMethod->Define();
      if (SuperMethod->defineResult > 0) {
        ParseWarning(Loc, "Forward-defined method `%s` for `%s`", *SuperMethod->GetFullName(), *GetFullName());
        SuperMethod->defineResult = 666;
      } else {
        Ret = false;
      }
    }

    if (VMemberBase::optDeprecatedLaxOverride || VMemberBase::koraxCompatibility) Flags |= FUNC_Override; // force `override`
    if ((Flags&FUNC_Override) == 0) {
      ParseError(Loc, "Overriding virtual method `%s` without `override` keyword", *GetFullName());
      Ret = false;
    }
    if (Ret && (SuperMethod->Flags&FUNC_Private) != 0) {
      ParseError(Loc, "Overriding private method `%s` is not allowed", *GetFullName());
      Ret = false;
    }
    if (Ret && (Flags&FUNC_Private) != 0) {
      ParseError(Loc, "Overriding with private method `%s` is not allowed", *GetFullName());
      Ret = false;
    }
    if (Ret && (SuperMethod->Flags&FUNC_Protected) != (Flags&FUNC_Protected)) {
      if ((SuperMethod->Flags&FUNC_Protected)) {
        ParseError(Loc, "Cannot override protected method `%s` with public one", *GetFullName());
        Ret = false;
      } else {
        //FIXME: not yet implemented
        ParseError(Loc, "Cannot override public method `%s` with protected one", *GetFullName());
        Ret = false;
      }
    }
    if (Ret && (SuperMethod->Flags&FUNC_Final)) {
      ParseError(Loc, "Method `%s` already has been declared as final and cannot be overridden", *GetFullName());
      Ret = false;
    }
    if (!SuperMethod->ReturnType.Equals(ReturnType)) {
      if (Ret) {
        //GLog.Logf(NAME_Debug, "::: %s", *SuperMethod->Loc.toString());
        ParseError(Loc, "Method `%s` redefined with different return type (wants '%s', got '%s')", *GetFullName(), *SuperMethod->ReturnType.GetName(), *ReturnType.GetName());
        abort();
      }
      Ret = false;
    } else if (SuperMethod->NumParams != NumParams) {
      if (Ret) ParseError(Loc, "Method `%s` redefined with different number of arguments", *GetFullName());
      Ret = false;
    } else {
      for (int i = 0; i < NumParams; ++i) {
        if (!SuperMethod->ParamTypes[i].Equals(ParamTypes[i])) {
          if (Ret) {
            ParseError(Loc, "Type of argument #%d differs from base class (expected `%s`, got `%s`)", i+1,
              *SuperMethod->ParamTypes[i].GetName(), *ParamTypes[i].GetName());
          }
          Ret = false;
        }
        if ((SuperMethod->ParamFlags[i]^ParamFlags[i])&(FPARM_Optional|FPARM_Out|FPARM_Ref|FPARM_Const)) {
          if (Ret) ParseError(Loc, "Modifiers of argument #%d differs from base class", i+1);
          Ret = false;
        }
      }
    }

    // inherit network flags and decorate visibility
    Flags |= SuperMethod->Flags&(FUNC_NetFlags|FUNC_Decorate);

    if (Flags&FUNC_Decorate) { ParseError(Loc, "Decorate action method cannot be virtual `%s`", *GetFullName()); Ret = false; }
  } else {
    if ((Flags&FUNC_Override) != 0) {
      ParseError(Loc, "Trying to override non-existing method `%s`", *GetFullName());
      Ret = false;
    }
  }

  // check for invalid decorate methods
  if (Flags&FUNC_Decorate) {
    if (Flags&FUNC_VarArgs) { ParseError(Loc, "Decorate action method cannot be vararg `%s`", *GetFullName()); Ret = false; }
    if (Flags&FUNC_Iterator) { ParseError(Loc, "Decorate action method cannot be iterator `%s`", *GetFullName()); Ret = false; }
    if (Flags&FUNC_Private) { ParseError(Loc, "Decorate action method cannot be private `%s`", *GetFullName()); Ret = false; }
    if (Flags&FUNC_Protected) { ParseError(Loc, "Decorate action method cannot be protected `%s`", *GetFullName()); Ret = false; }
    if (!(Flags&FUNC_Final)) { ParseError(Loc, "Decorate action method must be final `%s`", *GetFullName()); Ret = false; }
  }

  if (Flags&FUNC_Spawner) {
    // verify that it's a valid spawner method
    if (NumParams < 1) {
      ParseError(Loc, "Spawner method must have at least 1 argument");
    } else if (ParamTypes[0].Type != TYPE_Class) {
      ParseError(Loc, "Spawner method must have class as first argument");
    } else if (ReturnType.Type != TYPE_Reference && ReturnType.Type != TYPE_Class) {
      ParseError(Loc, "Spawner method must return an object reference or class");
    } else if (ReturnType.Class != ParamTypes[0].Class) {
      // hack for `SpawnObject (class)`
      if (ParamTypes[0].Class || ReturnType.Class->Name != "Object") {
        ParseError(Loc, "Spawner method must return an object of the same type as class");
      }
    }
  }

  defineResult = (Ret ? 1 : 0);
  return Ret;
}


// ////////////////////////////////////////////////////////////////////////// //
class VLocCollectVisitor;
class VExprLocVisitor;

class VLocCollectVisitor : public VStatementVisitorChildrenLast {
public:
  struct LocInfo {
    int lidx;
    int assidx;
    int setCount;
    int useCount;
  };

  VEmitContext *ec;
  VMethod *mt;
  int argmaxidx;
  TMapNC<int, LocInfo> lvars;

  int assidx;

public:
  VLocCollectVisitor (VEmitContext *aec, VMethod *amt, int amax);
  virtual ~VLocCollectVisitor ();

  virtual void VisitedExpr (VStatement *stmt, VExpression *expr) override;
  virtual void Visited (VStatement *stmt) override;
};


class VExprLocVisitor : public VExprVisitorChildrenLast {
public:
  VLocCollectVisitor *stv;
  bool indecl;

public:
  VExprLocVisitor (VLocCollectVisitor *astv);
  virtual ~VExprLocVisitor () override;

  virtual void Visited (VExpression *expr) override;
};


//==========================================================================
//
//  VExprLocVisitor::VExprLocVisitor
//
//==========================================================================
VExprLocVisitor::VExprLocVisitor (VLocCollectVisitor *astv)
  : VExprVisitorChildrenLast()
  , stv(astv)
  , indecl(false)
{
}


//==========================================================================
//
//  VExprLocVisitor::~VExprLocVisitor
//
//==========================================================================
VExprLocVisitor::~VExprLocVisitor () {
}



//==========================================================================
//
//  VLocCollectVisitor::VLocCollectVisitor
//
//==========================================================================
VLocCollectVisitor::VLocCollectVisitor (VEmitContext *aec, VMethod *amt, int amax)
  : VStatementVisitorChildrenLast()
  , ec(aec)
  , mt(amt)
  , argmaxidx(amax)
  , assidx(0)
{}


//==========================================================================
//
//  VExprLocVisitor::Visited
//
//==========================================================================
void VExprLocVisitor::Visited (VExpression *expr) {
  /*
  if (expr->IsLocalVarExpr()) {
    int lvidx = ((VLocalVar *)expr)->num;
    if (lvidx >= stv->argmaxidx && stv->ec->IsLocalUsedByIdx(lvidx)) {
      VLocalVarDef &ldef = stv->ec->GetLocalByIndex(lvidx);
      if (ldef.Name != NAME_None) {
        while (lvidx >= stv->lvseen.length()) {
          stv->lvseen.Append(false);
        }
        if (!stv->lvseen[lvidx]) {
          VLocalVarDef &GetLocalByIndex (int idx) noexcept;
          stv->lvseen[lvidx] = true;
          VLocCollectVisitor::LocInfo &li = stv->lvars.Alloc();
          li.lidx = lvidx;
        }
      }
    }
  }
  */
}


//==========================================================================
//
//  VLocCollectVisitor::~VLocCollectVisitor
//
//==========================================================================
VLocCollectVisitor::~VLocCollectVisitor () {
}


//==========================================================================
//
//  VLocCollectVisitor::VisitedExpr
//
//==========================================================================
void VLocCollectVisitor::VisitedExpr (VStatement *stmt, VExpression *expr) {
  //VExprLocVisitor ev(this);
  if (this->locDecl && expr->IsLocalVarDecl()) {
    //ev.indecl = true;
    //expr->Visit(&ev, ChildrenFirst);
    VLocalDecl *dc = (VLocalDecl *)expr;
    for (int f = 0; f < dc->Vars.length(); f += 1) {
      const VLocalEntry &le = dc->Vars[f];
      if (le.Name != NAME_None) {
        vassert(le.locIdx >= 0);
        VLocalVarDef &ldef = ec->GetLocalByIndex(le.locIdx);
        if (le.Value) {
          GLog.Logf(NAME_Debug, "METHOD: %s, local #%d: %s; init=%s",
                    *mt->GetFullName(), le.locIdx, *ldef.Name,
                    *le.Value->toString());
        } else {
          GLog.Logf(NAME_Debug, "METHOD: %s, local #%d: %s",
                    *mt->GetFullName(), le.locIdx, *ldef.Name);
        }
      }
    }
  }
}


//==========================================================================
//
//  VLocCollectVisitor::Visited
//
//==========================================================================
void VLocCollectVisitor::Visited (VStatement *stmt) {
  if (stmt->IsVarDecl()) {
    /*
    VLocalVarStatement *ld = (VLocalVarStatement *)stmt;
    VLocalDecl *Decl;
    */
  }
}


//==========================================================================
//
//  OptimiseLocals
//
//==========================================================================
static VStatement *OptimiseLocals (VEmitContext *ec, VMethod *mt, VStatement *Statement,
                                   int argmaxidx)
{
  #if 0
  VLocCollectVisitor stcv(ec, mt, argmaxidx);

  Statement->Visit(&stcv);

  /*
  if (stcv.lvars.length()) {
    GLog.Logf(NAME_Debug, "METHOD: %s, locals: %d", *mt->GetFullName(), stcv.lvars.length());
    for (int f = 0; f < stcv.lvars.length(); f += 1) {
      VLocalVarDef &ldef = ec->GetLocalByIndex(stcv.lvars[f].lidx);
      GLog.Logf(NAME_Debug, "  %d: %s", f, *ldef.Name);
    }
  }
  */
  #endif

  return Statement;
}


//==========================================================================
//
//  VMethod::Emit
//
//==========================================================================
void VMethod::Emit () {
  if (defineResult < 0) VCFatalError("`Emit()` before `Define()` for %s", *GetFullName());
  if (emitCalled) {
    GLog.Logf(NAME_Debug, "`Emit()` for `%s` already called!", *GetFullName());
    return;
  }
  emitCalled = true;

  if (Flags&FUNC_Native) {
    if (Statement) ParseError(Loc, "Native methods can't have a body");
    return;
  }

  if (Outer->MemberType == MEMBER_Field) return; // delegate

  if (!Statement) { ParseError(Loc, "Method body missing"); return; }

  // no need to do it here, as optimizer will do it for us
  /*
  if (ReturnTypeExpr && ReturnTypeExpr->Type.Type != TYPE_Void) {
    if (!Statement->IsEndsWithReturn()) {
      ParseError(Loc, "Missing `return` in one of the pathes of function `%s`", *GetFullName());
      return;
    }
  }
  */

  //fprintf(stderr, "*** EMIT000: <%s> (%s); ParamsSize=%d; NumLocals=%d; NumParams=%d\n", *GetFullName(), *Loc.toStringNoCol(), ParamsSize, NumLocals, NumParams);

  VEmitContext ec(this);
  if (Flags&FUNC_NoVCalls) ec.VCallsDisabled = true;

  ec.ClearLocalDefs();
  if ((Flags&FUNC_Static) == 0) ec.ReserveStack(1); // first is `self`
  if (Outer->MemberType == MEMBER_Class && this == ((VClass *)Outer)->DefaultProperties) {
    ec.InDefaultProperties = true;
  }

  // allocate method arguments
  for (int i = 0; i < NumParams; ++i) {
    VMethodParam &P = Params[i];
    // allocate argument
    if (P.Name != NAME_None) {
      //if (ec.CheckForLocalVar(P.Name) != -1) ParseError(P.Loc, "Redefined argument `%s`", *P.Name);
      ec.CheckLocalDecl(P.Name, P.Loc);
      VLocalVarDef &L = ec.NewLocal(P.Name, ParamTypes[i], VEmitContext::Safe, P.Loc, ParamFlags[i]);
      L.Visible = true;
      ec.ReserveLocalSlot(L.GetIndex());
      // mark created local as used, to avoid useless warnings
      ec.MarkLocalUsedByIdx(L.GetIndex());
      // k8: and we have to emit argument fixer there, because doing it in the loop below
      // will desync on the first optional arg. and it did this for years, lol.
      if (L.Type.Type == TYPE_Vector && (ParamFlags[i]&(FPARM_Out|FPARM_Ref)) == 0) {
        #if 0
        GLog.Logf(NAME_Debug, "MT:%s; VectorFix: arg:%d (%s) : (%s); locofs: %d; flags:0x%08x\n",
                  *GetFullName(), i, *Params[i].Name, *L.Name, L.Offset,
                  (unsigned)ParamFlags[i]);
        #endif
        ec.AddStatement(OPC_VFixVecParam, L.Offset, Loc);
      }
      // create optional
      if (ParamFlags[i]&FPARM_Optional) {
        VName specName(va("specified_%s", *P.Name));
        if (ec.CheckForLocalVar(specName) != -1) ParseError(P.Loc, "Redefined argument `%s`", *specName);
        VLocalVarDef &SL = ec.NewLocal(specName, TYPE_Int, VEmitContext::Safe, P.Loc);
        SL.Visible = true;
        ec.ReserveLocalSlot(SL.GetIndex());
        // mark created local as used, to avoid useless warnings
        ec.MarkLocalUsedByIdx(SL.GetIndex());
      }
    } else {
      // skip unnamed
      //FIXME: k8: how the fuck we could get an unnamed arg? the parser doesn't allow this.
      ec.ReserveStack(ParamFlags[i]&(FPARM_Out|FPARM_Ref) ? 1 : ParamTypes[i].GetStackSize());
      if (ParamFlags[i]&FPARM_Optional) ec.ReserveStack(1);
    }
  }

  //GLog.Logf(NAME_Debug, "*** METHOD(before): %s ***", *GetFullName());
  //GLog.Logf(NAME_Debug, "%s", *Statement->toString());

  Statement = Statement->Resolve(ec, nullptr);
  if (!Statement || !Statement->IsValid()) {
    //ParseError(Loc, "Cannot resolve statements in `%s`", *GetFullName());
    //fprintf(stderr, "===\n%s\n===\n", /*Statement->toString()*/*shitppTypeNameObj(*Statement));
    return;
  }

  Statement = OptimiseLocals(&ec, this, Statement, ec.GetLocalDefCount());

  //GLog.Logf(NAME_Debug, "*** METHOD(after): %s ***", *GetFullName());
  //GLog.Logf(NAME_Debug, "%s", *Statement->toString());

  Statement->Emit(ec, nullptr);

  // just in case; also, we should not have any alive finalizers here, but, again, just in case...
  if (ReturnType.Type == TYPE_Void) {
    Statement->EmitDtor(ec, true); // proper leaving
    ec.AddStatement(OPC_Return, Loc);
  }

  NumLocals = ec.CalcUsedStackSize();

  // dump unused locals
  if (WarningUnusedLocals) {
    for (int f = 0; f < ec.GetLocalDefCount(); ++f) {
      const VLocalVarDef &loc = ec.GetLocalByIndex(f);
      if (loc.Name == NAME_None) continue;
      // ignore some special names
      const char *nn = *loc.Name;
      if (nn[0] == '_' || VStr::startsWithCI(nn, "user_")) continue;
      if (!loc.WasRead && !loc.WasWrite) {
        GLog.Logf(NAME_Warning, "%s: unused local `%s` in method `%s`", *loc.Loc.toStringNoCol(), nn, *GetFullName());
      } else if (!loc.WasRead && loc.WasWrite) {
        GLog.Logf(NAME_Warning, "%s: unused write-only local `%s` in method `%s`", *loc.Loc.toStringNoCol(), nn, *GetFullName());
      }
    }
  }

  ec.EndCode();

       if (VMemberBase::doAsmDump) DumpAsm();
  else if (VObject::cliAsmDumpMethods.has(VStr(Name))) DumpAsm();

  OptimizeInstructions();

  // and dump it again for optimized case
       if (VMemberBase::doAsmDump) DumpAsm();
  else if (VObject::cliAsmDumpMethods.has(VStr(Name))) DumpAsm();

  // do not clear statement list here (it will be done in `CompilerShutdown()`)
}


//==========================================================================
//
//  VMethod::FindArgByName
//
//  -1: not found
//  -2: several case-insensitive matches
//
//==========================================================================
int VMethod::FindArgByName (VName aname) const noexcept {
  if (aname == NAME_None) return -1;
  for (int f = 0; f < NumParams; ++f) {
    if (Params[f].Name == aname) return f;
  }
  // try case-insensitive search, because why not?
  int res = -1;
  for (int f = 0; f < NumParams; ++f) {
    if (VStr::strEquCI(*Params[f].Name, *aname)) {
      if (res >= 0) return -1; // oops
      res = f;
    }
  }
  return res;
}


//==========================================================================
//
//  VMethod::CanBeCalledWithoutArguments
//
//==========================================================================
bool VMethod::CanBeCalledWithoutArguments () const noexcept {
  if (NumParams > MAX_PARAMS) return false; // just in case
  for (int f = 0; f < NumParams; ++f) {
    // optional?
    if (!(ParamFlags[f]&FPARM_Optional)) return false;
    // it should not be ref/out array/dict/struct (VM requires pointer to dummy object in this case)
    if ((ParamFlags[f]&(FPARM_Out|FPARM_Ref)) != 0) {
      if (ParamTypes[f].IsAnyArrayOrStruct()) return false;
    }
  }
  return true;
}


//==========================================================================
//
//  VMethod::DumpAsm
//
//  disassembles a method
//
//==========================================================================
void VMethod::DumpAsm () {
  VMemberBase *PM = Outer;
  while (PM->MemberType != MEMBER_Package) PM = PM->Outer;
  VPackage *Package = (VPackage *)PM;

  GLog.Logf(NAME_Debug, "--------------------------------------------");
  GLog.Logf(NAME_Debug, "Dump ASM function %s.%s (%d instructions)", *Outer->Name, *Name, (Flags&FUNC_Native ? 0 : Instructions.length()));
  if (Flags&FUNC_Native) {
    //  Builtin function
    GLog.Logf(NAME_Debug, "*** Builtin function.");
    return;
  }
  for (int s = 0; s < Instructions.length(); ++s) {
    // opcode
    VStr disstr;
    int st = Instructions[s].Opcode;
    disstr += va("%6d: %s", s, StatementInfo[st].name);
    if (Instructions[s].Opcode == OPC_VectorSwizzleDirect) {
      vassert(StatementInfo[st].Args == OPCARGS_Short);
      disstr += " (";
      disstr += VVectorSwizzleExpr::SwizzleToStr(Instructions[s].Arg1);
      disstr += ")";
    } else {
      switch (StatementInfo[st].Args) {
        case OPCARGS_None:
          break;
        case OPCARGS_Member:
          // name of the object
          disstr += va(" %s", *Instructions[s].Member->GetFullName());
          break;
        case OPCARGS_BranchTargetB:
        case OPCARGS_BranchTargetNB:
        //case OPCARGS_BranchTargetS:
        case OPCARGS_BranchTarget:
          disstr += va(" %6d", Instructions[s].Arg1);
          break;
        case OPCARGS_ByteBranchTarget:
        case OPCARGS_ShortBranchTarget:
        case OPCARGS_IntBranchTarget:
          disstr += va(" %6d, %6d", Instructions[s].Arg1, Instructions[s].Arg2);
          break;
        case OPCARGS_NameBranchTarget:
          disstr += va(" '%s', %6d", *VName(EName(Instructions[s].Arg1)), Instructions[s].Arg2);
          break;
        case OPCARGS_Byte:
        case OPCARGS_Short:
          disstr += va(" %6d (%x)", Instructions[s].Arg1, Instructions[s].Arg1);
          break;
        case OPCARGS_Int:
          if (Instructions[s].Arg1IsFloat) {
            disstr += va(" %f", Instructions[s].Arg1F);
          } else {
            disstr += va(" %6d (%x)", Instructions[s].Arg1, Instructions[s].Arg1);
          }
          break;
        case OPCARGS_Name:
          // name
          disstr += va(" '%s'", *Instructions[s].NameArg);
          break;
        case OPCARGS_String:
          // string
          disstr += va(" %s", *Package->GetStringByIndex(Instructions[s].Arg1).quote());
          break;
        case OPCARGS_FieldOffset:
          if (Instructions[s].Member) disstr += va(" %s", *Instructions[s].Member->Name); else disstr += va(" (0)");
          break;
        case OPCARGS_VTableIndex:
          disstr += va(" %s", *Instructions[s].Member->Name);
          break;
        case OPCARGS_VTableIndex_Byte:
        case OPCARGS_FieldOffset_Byte:
        case OPCARGS_FieldOffsetS_Byte:
          if (Instructions[s].Member) disstr += va(" %s %d", *Instructions[s].Member->Name, Instructions[s].Arg2); else disstr += va(" (0)%d", Instructions[s].Arg2);
          break;
        case OPCARGS_TypeSize:
        case OPCARGS_Type:
        case OPCARGS_A2DDimsAndSize:
          disstr += va(" %s", *Instructions[s].TypeArg.GetName());
          break;
        case OPCARGS_TypeDD:
          disstr += va(" %s!(%s,%s)", StatementDictDispatchInfo[Instructions[s].Arg2].name, *Instructions[s].TypeArg.GetName(), *Instructions[s].TypeArg1.GetName());
          break;
        case OPCARGS_TypeAD:
          disstr += va(" %s!(%s)", StatementDynArrayDispatchInfo[Instructions[s].Arg2].name, *Instructions[s].TypeArg.GetName());
          break;
        case OPCARGS_Builtin:
          disstr += va(" %s", StatementBuiltinInfo[Instructions[s].Arg1].name);
          break;
        case OPCARGS_BuiltinCVar:
          disstr += va(" %s('%s')", StatementBuiltinInfo[Instructions[s].Arg1].name, *VName::CreateWithIndexSafe(Instructions[s].Arg2));
          break;
        case OPCARGS_Member_Int:
          disstr += va(" %s (%d)", *Instructions[s].Member->GetFullName(), Instructions[s].Arg2);
          break;
        case OPCARGS_Type_Int:
          disstr += va(" %s (%d)", *Instructions[s].TypeArg.GetName(), Instructions[s].Arg2);
          break;
        case OPCARGS_ArrElemType_Int:
          disstr += va(" %s (%d)", *Instructions[s].TypeArg.GetName(), Instructions[s].Arg2);
          break;
      }
    }
    GLog.Logf(NAME_Debug, "%s", *disstr);
  }
}


//==========================================================================
//
//  VMethod::PostLoad
//
//  this should be never called for VCC
//
//==========================================================================
void VMethod::PostLoad () {
  //k8: it should be called only once, but let's play safe here
  if (mPostLoaded) {
    //GLog.Logf(NAME_Debug, "method `%s` was already postloaded", *GetFullName());
    return;
  }

  if (defineResult < 0) VCFatalError("`Define()` not called for `%s`", *GetFullName());
  if (!emitCalled) {
    // delegate declarations creates methods without a code, and without a name; tolerate those!
    if (Name != NAME_None || Instructions.length() != 0) {
      VCFatalError("`Emit()` not called before `PostLoad()` for `%s`", *GetFullName());
    }
    emitCalled = true; // why not?
  }

  // check if owning class correctly postloaded
  // but don't do this for anonymous methods -- they're prolly created for delegates (FIXME: change this!)
  if (VTableIndex < -1) {
    if (Name != NAME_None && !IsStructMethod()) {
      VMemberBase *origClass = Outer;
      while (origClass) {
        if (origClass->isStructMember()) {
          // this is struct method
        }
        if (origClass->isClassMember()) {
          // check for a valid class
          VCFatalError("owning class `%s` for `%s` wasn't correctly postloaded", origClass->GetName(), *GetFullName());
        } else if (origClass->isStateMember()) {
          // it belongs to a state, so it is a wrapper
          if ((Flags&FUNC_Final) == 0) VCFatalError("state method `%s` is not final", *GetFullName());
          VTableIndex = -1;
          break;
        }
        origClass = origClass->Outer;
      }
      if (!origClass) {
        if ((Flags&FUNC_Final) == 0) VCFatalError("owner-less method `%s` is not final", *GetFullName());
        VTableIndex = -1; // dunno, something strange here
      }
    } else {
      // delegate dummy method (never called anyway), or struct method
      VTableIndex = -1; // dunno, something strange here
    }
  }

  //GLog.Logf(NAME_Debug, "*** %s: %s", *Loc.toString(), *GetFullName());
  // set up builtins
  if (NumParams > VMethod::MAX_PARAMS) VCFatalError("Function has more than %i params", VMethod::MAX_PARAMS);

  for (FBuiltinInfo *B = FBuiltinInfo::Builtins; B; B = B->Next) {
    if (B->used) continue;
    if (!VStr::strEqu(*Name, B->Name)) continue;

    //GLog.Logf(NAME_Debug, "trying builtin '%s' (struct:`%s`; class:`%s`) : `%s` (stm=%d)", B->Name, (B->OuterStructName ? B->OuterStructName : "<none>"), (B->OuterClassName ? B->OuterClassName : "<none>"), *GetFullName(), (int)IsStructMethod());

    bool okClass = false;
    bool okStruct = false;
    if (B->OuterClass) {
      vassert(!B->OuterStructName);
      vassert(!B->OuterClassName);
      okClass = (Outer == B->OuterClass);
      if (IsStructMethod()) VCFatalError("Builtin `%s` should be struct method", *GetFullName());
    } else {
      //GLog.Logf(NAME_Debug, "trying builtin '%s' (struct:`%s`; class:`%s`)", B->Name, B->OuterStructName, B->OuterClassName);
      vassert(B->OuterStructName && B->OuterStructName[0]);
      vassert(B->OuterClassName && B->OuterClassName[0]);
      VMemberBase *outerS = Outer;
      while (outerS) {
        if (outerS->isStructMember()) break;
        if (outerS->isClassMember() || outerS->isStateMember()) { outerS = nullptr; break; }
        outerS = outerS->Outer;
      }
      //if (outerS) GLog.Logf(NAME_Debug, "...found struct `%s`", outerS->GetName());
      // check struct name
      if (!outerS || !VStr::strEqu(outerS->GetName(), B->OuterStructName)) continue;
      outerS = outerS->Outer;
      // check outer class name
      while (outerS) {
        if (outerS->isClassMember()) break;
        if (outerS->isStructMember() || outerS->isStateMember()) { outerS = nullptr; break; }
        outerS = outerS->Outer;
      }
      //if (outerS) GLog.Logf(NAME_Debug, "...found class `%s`", outerS->GetName());
      if (!outerS || !VStr::strEqu(outerS->GetName(), B->OuterClassName)) continue;
      if (!IsStructMethod()) VCFatalError("Builtin `%s` should be class method", *GetFullName());
      okStruct = true;
    }
    if (!okClass && !okStruct) continue;

    // check flags
    if ((Flags&FUNC_Native) == 0) VCFatalError("Trying to define `%s` as builtin, but it is not native", *GetFullName());
    if (builtinOpc >= 0) VCFatalError("Trying to redefine `%s` builtin", *GetFullName());

    // ok
    B->used = true;
    NativeFunc = B->Func;
    break;
  }

  if (builtinOpc < 0 && !NativeFunc && (Flags&FUNC_Native) != 0) {
    if (VObject::engineAllowNotImplementedBuiltins) {
      // default builtin
      NativeFunc = PF_Fixme;
      // don't abort with error, because it will be done, when this
      // function will be called (if it will be called)
      if (VObject::cliShowUndefinedBuiltins) GLog.Logf(NAME_Error, "Builtin `%s` not found!", *GetFullName());
    } else {
      // k8: actually, abort. define all builtins.
      VCFatalError("Builtin `%s` not implemented!", *GetFullName());
    }
  }

  GenerateCode();

  mPostLoaded = true;
}


//==========================================================================
//
//  VMethod::WriteType
//
//==========================================================================
void VMethod::WriteType (const VFieldType &tp) {
  vuint8 tbuf[VFieldType::MemSize*2];
  vuint8 *ptr = tbuf;
  tp.WriteTypeMem(ptr);
  vassert((ptrdiff_t)(ptr-tbuf) == VFieldType::MemSize);
  //for (vuint8 *p = tbuf; p != ptr; ++p) Statements.append(*p);
  vmEmitBytes(&vmCodePool, tbuf, (unsigned)(ptrdiff_t)(ptr-tbuf));
}


/*
#define WriteUInt8(p)  Statements.Append(p)
#define WriteInt16(p)  Statements.SetNum(Statements.length()+2); *(vint16 *)&Statements[Statements.length()-2] = (p)
#define WriteInt32(p)  Statements.SetNum(Statements.length()+4); *(vint32 *)&Statements[Statements.length()-4] = (p)
#define WritePtr(p)    Statements.SetNum(Statements.length()+sizeof(void *)); *(void **)&Statements[Statements.length()-sizeof(void *)] = (p)
*/

#define WriteUInt8(p)  vmEmitUByte(&vmCodePool, (p))
#define WriteInt16(p)  vmEmitSShort(&vmCodePool, (p))
#define WriteInt32(p)  vmEmitSInt(&vmCodePool, (p))
#define WritePtr(p)    vmEmitPtr(&vmCodePool, (p))

#define PatchInt8(ofs,p)   vmPatchSByte(&vmCodePool, (ofs), (p))
#define PatchInt16(ofs,p)  vmPatchSShort(&vmCodePool, (ofs), (p))
#define PatchInt32(ofs,p)  vmPatchSInt(&vmCodePool, (ofs), (p))


//==========================================================================
//
//  VMethod::GenerateCode
//
//  this generates VM (or other) executable code (to `Statements`)
//  from IR `Instructions`
//
//==========================================================================
void VMethod::GenerateCode () {
  vassert(!vmCodeStart);
  vassert(!vmDebugInfo);
  //Statements.Clear();
  if (!Instructions.length()) return;

  vmStartPool(&vmCodePool);
  vmStartDebugPool(&vmDebugPool);

  TArray<int> iaddr; // addresses of all generated instructions
  iaddr.resize(Instructions.length()); // we know the size beforehand

  // generate VM bytecode
  // `-1`, because the last one is always DONE opcode
  assert(Instructions[Instructions.length()-1].Opcode == OPC_Done);
  int prevIEnd = 0;
  for (int i = 0; i < Instructions.length()-1; ++i) {
    //Instructions[i].Address = Statements.length();
    vassert(iaddr.length() == i);
    //iaddr.append(Statements.length());
    //Statements.Append(Instructions[i].Opcode);
    const int istart = vmCodeOffset(&vmCodePool);
    vassert(istart == prevIEnd);
    iaddr.append(istart);
    WriteUInt8(Instructions[i].Opcode);
    // emit opcode arguments
    switch (StatementInfo[Instructions[i].Opcode].Args) {
      case OPCARGS_None: break;
      case OPCARGS_Member: WritePtr(Instructions[i].Member); break;
      case OPCARGS_BranchTargetB: WriteUInt8(0); break;
      case OPCARGS_BranchTargetNB: WriteUInt8(0); break;
      //case OPCARGS_BranchTargetS: WriteInt16(0); break;
      case OPCARGS_BranchTarget: WriteInt32(0); break;
      case OPCARGS_ByteBranchTarget: WriteUInt8(Instructions[i].Arg1); WriteInt16(0); break;
      case OPCARGS_ShortBranchTarget: WriteInt16(Instructions[i].Arg1); WriteInt16(0); break;
      case OPCARGS_IntBranchTarget: WriteInt32(Instructions[i].Arg1); WriteInt16(0); break;
      case OPCARGS_NameBranchTarget: WriteInt32(Instructions[i].Arg1); WriteInt16(0); break;
      case OPCARGS_Byte: WriteUInt8(Instructions[i].Arg1); break;
      case OPCARGS_Short: WriteInt16(Instructions[i].Arg1); break;
      case OPCARGS_Int: WriteInt32(Instructions[i].Arg1); break;
      case OPCARGS_Name: WriteInt32(Instructions[i].NameArg.GetIndex()); break;
      case OPCARGS_NameS: WriteInt16(Instructions[i].NameArg.GetIndex()); break;
      case OPCARGS_String:
        {
          const VStr *sptr = &(GetPackage()->GetStringByIndex(Instructions[i].Arg1));
          vassert(sptr && sptr->isNullOrImmutable());
          WritePtr((void *)sptr);
        }
        break;
      case OPCARGS_FieldOffset:
        // make sure struct / class field offsets have been calculated
        if (Instructions[i].Member) {
          Instructions[i].Member->Outer->PostLoad();
          WriteInt32(((VField *)Instructions[i].Member)->Ofs);
        } else {
          WriteInt32(0);
        }
        break;
      case OPCARGS_FieldOffsetS:
        // make sure struct / class field offsets have been calculated
        if (Instructions[i].Member) {
          Instructions[i].Member->Outer->PostLoad();
          WriteInt16(((VField *)Instructions[i].Member)->Ofs);
        } else {
          WriteInt16(0);
        }
        break;
      case OPCARGS_VTableIndex:
        // make sure class virtual table has been calculated
        Instructions[i].Member->Outer->PostLoad();
        WriteInt16(((VMethod *)Instructions[i].Member)->VTableIndex);
        break;
      case OPCARGS_VTableIndexB:
        // make sure class virtual table has been calculated
        Instructions[i].Member->Outer->PostLoad();
        WriteUInt8(((VMethod *)Instructions[i].Member)->VTableIndex);
        break;
      case OPCARGS_VTableIndex_Byte:
        // make sure class virtual table has been calculated
        Instructions[i].Member->Outer->PostLoad();
        WriteInt16(((VMethod *)Instructions[i].Member)->VTableIndex);
        WriteUInt8(Instructions[i].Arg2);
        break;
      case OPCARGS_VTableIndexB_Byte:
        // make sure class virtual table has been calculated
        Instructions[i].Member->Outer->PostLoad();
        WriteUInt8(((VMethod *)Instructions[i].Member)->VTableIndex);
        WriteUInt8(Instructions[i].Arg2);
        break;
      case OPCARGS_FieldOffset_Byte:
        // make sure struct / class field offsets have been calculated
        if (Instructions[i].Member) {
          Instructions[i].Member->Outer->PostLoad();
          WriteInt32(((VField *)Instructions[i].Member)->Ofs);
        } else {
          WriteInt32(0);
        }
        WriteUInt8(Instructions[i].Arg2);
        break;
      case OPCARGS_FieldOffsetS_Byte:
        // make sure struct / class field offsets have been calculated
        if (Instructions[i].Member) {
          Instructions[i].Member->Outer->PostLoad();
          WriteInt16(((VField *)Instructions[i].Member)->Ofs);
        } else {
          WriteInt16(0);
        }
        WriteUInt8(Instructions[i].Arg2);
        break;
      case OPCARGS_TypeSize: WriteInt32(Instructions[i].TypeArg.GetSize()); break;
      case OPCARGS_TypeSizeB: WriteUInt8(Instructions[i].TypeArg.GetSize()); break;
      case OPCARGS_Type: WriteType(Instructions[i].TypeArg); break;
      case OPCARGS_TypeDD:
        WriteType(Instructions[i].TypeArg);
        WriteType(Instructions[i].TypeArg1);
        WriteUInt8(Instructions[i].Arg2);
        break;
      case OPCARGS_A2DDimsAndSize:
        {
          // we got an array type here, but we need size of one array element, so DIY it
          // sorry for code duplication from `VArrayElement::InternalResolve()`
          VFieldType arr = Instructions[i].TypeArg;
          VFieldType itt = arr.GetArrayInnerType();
          if (itt.Type == TYPE_Byte || itt.Type == TYPE_Bool) itt = VFieldType(TYPE_Int);
          WriteInt16(arr.GetFirstDim());
          WriteInt16(arr.GetSecondDim());
          WriteInt32(itt.GetSize());
          //fprintf(stderr, "d0=%d; d1=%d; dim=%d; isz=%d; size=%d\n", arr.GetFirstDim(), arr.GetSecondDim(), arr.GetArrayDim(), itt.GetSize(), arr.GetSize());
        }
        break;
      case OPCARGS_Builtin: WriteUInt8(Instructions[i].Arg1); break;
      case OPCARGS_BuiltinCVar:
        WriteUInt8(Instructions[i].Arg1);
        WriteInt32(Instructions[i].Arg2); // name index
        WritePtr(nullptr); // cached VCvar pointer
        break;
      case OPCARGS_Member_Int: WritePtr(Instructions[i].Member); break; // int is not emited
      case OPCARGS_Type_Int: WriteInt32(Instructions[i].Arg2); break; // type is not emited
      case OPCARGS_ArrElemType_Int: WriteType(Instructions[i].TypeArg); WriteInt32(Instructions[i].Arg2); break;
      case OPCARGS_TypeAD: WriteType(Instructions[i].TypeArg); WriteUInt8(Instructions[i].Arg2); break;
    }
    //while (StatLocs.length() < Statements.length()) StatLocs.Append(Instructions[i].loc);
    const int cend = vmCodeOffset(&vmCodePool);
    vmDebugEmitLoc(&vmDebugPool, cend-prevIEnd, Instructions[i].loc);
    prevIEnd = cend;
  }
  //vmDebugEmitLoc(&vmDebugPool, vmCodeOffset(&vmCodePool)-prevIEnd, Instructions[Instructions.length()-1].loc);

  //Instructions[Instructions.length()-1].Address = Statements.length();
  vmDebugInfo = vmEndDebugPool(&vmDebugPool, &vmDebugInfoSize);

  vassert(iaddr.length() == Instructions.length()-1);
  //iaddr.append(Statements.length());
  iaddr.append(vmCodeOffset(&vmCodePool));

// there is no reason to generate last empty byte at all
// the compiler should properly terminate all methods, and if it didn't... well, everything is fubared then
#if 0
  #if 0
  if (vmCodeOffset(&vmCodePool)&0x03) {
    while (vmCodeOffset(&vmCodePool)&0x03) vmEmitUByte(&vmCodePool, 0);
  } else {
    vmEmitUInt(&vmCodePool, 0);
  }
  #else
  vmEmitUByte(&vmCodePool, 0);
  #endif
#endif

  // fix jump destinations
  for (int i = 0; i < Instructions.length()-1; ++i) {
    switch (StatementInfo[Instructions[i].Opcode].Args) {
      case OPCARGS_BranchTargetB:
        //Statements[iaddr[i]+1] = iaddr[Instructions[i].Arg1]-iaddr[i];
        PatchInt8(iaddr[i]+1, iaddr[Instructions[i].Arg1]-iaddr[i]);
        break;
      case OPCARGS_BranchTargetNB:
        //Statements[iaddr[i]+1] = iaddr[i]-iaddr[Instructions[i].Arg1];
        PatchInt8(iaddr[i]+1, iaddr[i]-iaddr[Instructions[i].Arg1]);
        break;
      //case OPCARGS_BranchTargetS:
      //  *(vint16 *)&Statements[iaddr[i]+1] = iaddr[Instructions[i].Arg1]-iaddr[i];
      //  break;
      case OPCARGS_BranchTarget:
        //*(vint32 *)&Statements[iaddr[i]+1] = iaddr[Instructions[i].Arg1]-iaddr[i];
        PatchInt32(iaddr[i]+1, iaddr[Instructions[i].Arg1]-iaddr[i]);
        break;
      case OPCARGS_ByteBranchTarget:
        //*(vint16 *)&Statements[iaddr[i]+2] = iaddr[Instructions[i].Arg2]-iaddr[i];
        PatchInt16(iaddr[i]+2, iaddr[Instructions[i].Arg2]-iaddr[i]);
        break;
      case OPCARGS_ShortBranchTarget:
        //*(vint16 *)&Statements[iaddr[i]+3] = iaddr[Instructions[i].Arg2]-iaddr[i];
        PatchInt16(iaddr[i]+3, iaddr[Instructions[i].Arg2]-iaddr[i]);
        break;
      case OPCARGS_IntBranchTarget:
      case OPCARGS_NameBranchTarget:
        //*(vint16 *)&Statements[iaddr[i]+5] = iaddr[Instructions[i].Arg2]-iaddr[i];
        PatchInt16(iaddr[i]+5, iaddr[Instructions[i].Arg2]-iaddr[i]);
        break;
    }
  }

  vmCodeStart = vmEndPool(&vmCodePool, &vmCodeSize);

  #if 0
  DumpAsm();
  GLog.Logf(NAME_Debug, " ** VM OPCODES: 0x%08x (%u) **", (unsigned)vmCodeStart, vmCodeSize);
  for (int i = 0; i < Instructions.length()-1; ++i) {
    int ofs = iaddr[i];
    int len = iaddr[i+1]-ofs;
    VStr ds = va("  0x%08x: %5d:(%d): %16s ", (unsigned)(vmCodeStart+ofs), ofs, len, StatementInfo[Instructions[i].Opcode].name);
    for (int n = 0; n < len; ++n) {
      vassert(ofs+n < (int)vmCodeSize);
      ds += va(" %02x", vmCodeStart[ofs+n]);
    }
    GLog.Logf(NAME_Debug, "%s", *ds);
  }
  /*
  for (vuint32 ofs = 0; ofs < vmCodeSize; ofs += 16) {
    VStr ds = va("  0x%08x:", (unsigned)(vmCodeStart+ofs));
    for (unsigned n = 0; n < 16 && ofs+n < vmCodeSize; ++n) {
      if (n == 8) ds += " ";
      ds += va(" %02x", vmCodeStart[ofs+n]);
    }
    GLog.Logf(NAME_Debug, "%s", *ds);
  }
  */
  #endif

  if (vcErrorCount == 0) CompileToNativeCode();

  // we don't need instructions anymore
  Instructions.Clear();
  //Statements.condense();
}


//==========================================================================
//
//  VMethod::OptimizeInstructions
//
//==========================================================================
void VMethod::OptimizeInstructions () {
  VMCOptimizer opt(this, Instructions);
  opt.optimizeAll();
  if (vcErrorCount == 0) {
    // do this last, as optimizer can remove some dead code
    opt.checkReturns();
    // this should be done as a last step
    opt.shortenInstructions();
  }
  opt.finish(); // this will copy result back to `Instructions`
}


//==========================================================================
//
//  VMethod::FindPCLocation
//
//==========================================================================
TLocation VMethod::FindPCLocation (const vuint8 *pc) {
  /*
  if (!pc || Statements.length() == 0) return TLocation();
  if (pc < Statements.Ptr()) return TLocation();
  size_t stidx = (size_t)(pc-Statements.Ptr());
  if (stidx >= (size_t)Statements.length()) return TLocation();
  if (stidx >= (size_t)StatLocs.length()) return TLocation(); // just in case
  return StatLocs[(int)stidx];
  */
  if (!pc || !vmCodeStart || vmCodeSize == 0 || pc < vmCodeStart) return TLocation();
  if (!vmDebugInfo) return TLocation();
  return vmDebugFindLocForOfs(vmDebugInfo, (size_t)(ptrdiff_t)(pc-vmCodeStart));
}


//==========================================================================
//
//  VMethod::CleanupParams
//
//  this can be called in `ExecuteNetMethod()` to do cleanup after RPC
//
//==========================================================================
void VMethod::CleanupParams () const {
  VStack *Param = VObject::pr_stackPtr-ParamsSize+(Flags&FUNC_Static ? 0 : 1); // skip self too
  for (int i = 0; i < NumParams; ++i) {
    switch (ParamTypes[i].Type) {
      case TYPE_Int:
      case TYPE_Byte:
      case TYPE_Bool:
      case TYPE_Float:
      case TYPE_Name:
      case TYPE_Pointer:
      case TYPE_Reference:
      case TYPE_Class:
      case TYPE_State:
        ++Param;
        break;
      case TYPE_String:
        ((VStr *)&Param->p)->clear();
        ++Param;
        break;
      case TYPE_Vector:
        Param += 3;
        break;
      default:
        VPackage::InternalFatalError(va("Bad method argument type `%s`", *ParamTypes[i].GetName()));
    }
    if (ParamFlags[i]&FPARM_Optional) ++Param;
  }
  VObject::pr_stackPtr -= ParamsSize;

  // push null return value
  switch (ReturnType.Type) {
    case TYPE_Void:
      break;
    case TYPE_Int:
    case TYPE_Byte:
    case TYPE_Bool:
    case TYPE_Name:
      VObject::PR_Push(0);
      break;
    case TYPE_Float:
      VObject::PR_Pushf(0);
      break;
    case TYPE_String:
      VObject::PR_PushStr(VStr());
      break;
    case TYPE_Pointer:
    case TYPE_Reference:
    case TYPE_Class:
    case TYPE_State:
      VObject::PR_PushPtr(nullptr);
      break;
    case TYPE_Vector:
      VObject::PR_Pushf(0);
      VObject::PR_Pushf(0);
      VObject::PR_Pushf(0);
      break;
    default:
      VPackage::InternalFatalError(va("Bad return value type `%s`", *ReturnType.GetName()));
  }
}


//==========================================================================
//
//  VMethod::ReportUnusedBuiltins
//
//  returns `true` if there was any
//
//==========================================================================
bool VMethod::ReportUnusedBuiltins () {
  bool res = false;
  for (FBuiltinInfo *B = FBuiltinInfo::Builtins; B; B = B->Next) {
    if (B->used) continue;
    res = true;
    if (B->OuterClass) {
      vassert(!B->OuterStructName);
      vassert(!B->OuterClassName);
      GLog.Logf(NAME_Error, "unused builtin '%s' (class:`%s`)", B->Name, *B->OuterClass->GetFullName());
    } else {
      if (B->OuterStructName && B->OuterClassName) {
        GLog.Logf(NAME_Error, "unused builtin '%s' (struct:`%s`; class:`%s`)", B->Name, B->OuterStructName, B->OuterClassName);
      } else if (B->OuterStructName) {
        GLog.Logf(NAME_Error, "unused builtin '%s' (struct:`%s`)", B->Name, B->OuterStructName);
      } else if (B->OuterClassName) {
        GLog.Logf(NAME_Error, "unused builtin '%s' (class:`%s`)", B->Name, B->OuterClassName);
      } else {
        GLog.Logf(NAME_Error, "unused builtin '%s' (orphan)", B->Name);
      }
    }
  }
  return res;
}


#include "vc_method_jit.cpp"
