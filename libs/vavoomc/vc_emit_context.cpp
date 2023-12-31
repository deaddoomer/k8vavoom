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

//#define VV_EMIT_DISABLE_LOCAL_REUSE
//#define VV_EMIT_DUMP_SLOT_ALLOC


VStatementInfo StatementInfo[NUM_OPCODES] = {
#define DECLARE_OPC(name, args)   { #name, OPCARGS_##args, 0}
#define OPCODE_INFO
#include "vc_progdefs.h"
};

VStatementBuiltinInfo StatementBuiltinInfo[] = {
#define BUILTIN_OPCODE_INFO
#define DECLARE_OPC_BUILTIN(name)  { #name }
#include "vc_progdefs.h"
  { nullptr },
};

VStatementBuiltinInfo StatementDictDispatchInfo[] = {
#define DICTDISPATCH_OPCODE_INFO
#define DECLARE_OPC_DICTDISPATCH(name)  { #name }
#include "vc_progdefs.h"
  { nullptr },
};

VStatementBuiltinInfo StatementDynArrayDispatchInfo[] = {
#define DYNARRDISPATCH_OPCODE_INFO
#define DECLARE_OPC_DYNARRDISPATCH(name)  { #name }
#include "vc_progdefs.h"
  { nullptr },
};

#define DICTDISPATCH_OPCODE_INFO
#include "vc_progdefs.h"

#define DYNARRDISPATCH_OPCODE_INFO
#include "vc_progdefs.h"


struct SaveIntVal {
  int *vptr;
  int oldval;

  SaveIntVal () = delete;
  SaveIntVal (const SaveIntVal &) = delete;
  SaveIntVal &operator = (const SaveIntVal &) = delete;
  SaveIntVal (int *avar, int newval) noexcept { vptr = avar; oldval = *avar; *avar = newval; }
  ~SaveIntVal () noexcept { *vptr = oldval; }
};


vuint32 VEmitContext::maxInstrUsed = 0;


struct EmitBuffer {
  FInstruction* Instrs;
  vuint32 InstrsUsed;
  vuint32 InstrsAlloted;
  EmitBuffer *next;
  int used;

  inline EmitBuffer () noexcept : Instrs(nullptr), InstrsUsed(0), InstrsAlloted(0), next(nullptr), used(0) {}

  inline void clear () noexcept {
    if (Instrs) Z_Free(Instrs);
    Instrs = nullptr;
    InstrsUsed = InstrsAlloted = 0;
  }

  FInstruction &allocInstruction () {
    if (InstrsUsed == InstrsAlloted) {
      InstrsAlloted += 3072;
      Instrs = (FInstruction*)Z_Realloc(Instrs, sizeof(Instrs[0])*InstrsAlloted);
    }
    FInstruction* res = &Instrs[InstrsUsed++];
    memset((void *)res, 0, sizeof(*res));
    if (VEmitContext::maxInstrUsed < InstrsUsed) VEmitContext::maxInstrUsed = InstrsUsed;
    return *res;
  }

  inline int getInstrCount () noexcept { return (int)InstrsUsed; }
  inline FInstruction &getInstr (int idx) { vassert(idx >= 0 && (unsigned)idx < InstrsUsed); return Instrs[(unsigned)idx]; }
};


static EmitBuffer *ebHead = nullptr;


//==========================================================================
//
//  allocEmitBuffer
//
//==========================================================================
static EmitBuffer *allocEmitBuffer () {
  for (EmitBuffer *eb = ebHead; eb; eb = eb->next) {
    if (eb->used == 0) {
      eb->used = 1;
      return eb;
    }
  }
  EmitBuffer *res = (EmitBuffer*)Z_Calloc(sizeof(EmitBuffer));
  if (ebHead) ebHead->next = res; else ebHead = res;
  res->used = 1;
  return res;
}


//==========================================================================
//
//  releaseEmitBuffer
//
//==========================================================================
void releaseEmitBuffer (EmitBuffer *eb) {
  if (!eb) return;
  assert(eb->used);
  eb->used = 0;
  eb->InstrsUsed = 0;
}


//==========================================================================
//
//  VEmitContext::getEmitBufferCount
//
//==========================================================================
int VEmitContext::getEmitBufferCount () noexcept {
  int res = 0;
  for (EmitBuffer *eb = ebHead; eb; eb = eb->next) ++res;
  return res;
}


//==========================================================================
//
//  VEmitContext::getTotalEmitBufferSize
//
//==========================================================================
int VEmitContext::getTotalEmitBufferSize () noexcept {
  int res = 0;
  for (EmitBuffer *eb = ebHead; eb; eb = eb->next) res += (int)eb->InstrsAlloted;
  return res*(int)sizeof(ebHead->Instrs[0]);
}


//==========================================================================
//
//  VEmitContext::releaseAllEmitBuffers
//
//==========================================================================
void VEmitContext::releaseAllEmitBuffers () noexcept {
  for (EmitBuffer *eb = ebHead; eb; eb = eb->next) if (!eb->used) eb->clear();
}



//==========================================================================
//
//  VEmitContext::VEmitContext
//
//==========================================================================
VEmitContext::VEmitContext (VMemberBase *Member)
  : CurrentFunc(nullptr)
  , SelfClass(nullptr)
  , SelfStruct(nullptr)
  , Package(nullptr)
  , IndArray(nullptr)
  , OuterClass(nullptr)
  , FuncRetType(TYPE_Unknown)
  , EmitBuf(nullptr)
  //, localsofs(0)
  , InDefaultProperties(false)
  , VCallsDisabled(false)
  , InReturn(0)
  , InLoop(0)
{
  // find the class
  VMemberBase *CM = Member;
  while (CM && CM->MemberType != MEMBER_Class) CM = CM->Outer;
  OuterClass = SelfClass = (VClass *)CM;

  VMemberBase *PM = Member;
  while (PM != nullptr && PM->MemberType != MEMBER_Package) PM = PM->Outer;
  Package = (VPackage *)PM;

  // check for struct method
  if (Member != nullptr && Member->MemberType == MEMBER_Method) {
    VMethod *mt = (VMethod *)Member;
    if (mt->Flags&FUNC_StructMethod) {
      VMemberBase *SM = Member;
      while (SM && SM->MemberType != MEMBER_Struct) {
        if (SM == SelfClass) { SM = nullptr; break; }
        SM = SM->Outer;
      }
      if (SM) {
        //GLog.Logf(NAME_Debug, "compiling struct method `%s`", *Member->GetFullName());
        //SelfClass = nullptr; // we still need outer class
        SelfStruct = (VStruct *)SM;
      }
    }
  }

  if (Member != nullptr && Member->MemberType == MEMBER_Method) {
    CurrentFunc = (VMethod *)Member;
    //CurrentFunc->SelfTypeClass = nullptr;
    CurrentFunc->Instructions.Clear();
    //k8: nope, don't do this. many (most!) states with actions are creating anonymous functions
    //    with several statements only. this is ALOT of functions. and each get several kb of RAM.
    //    hello, OOM on any moderately sized mod. yay.
    //CurrentFunc->Instructions.Resize(1024);
    FuncRetType = CurrentFunc->ReturnType;
    if (SelfStruct) {
      vassert(CurrentFunc->Flags&FUNC_StructMethod);
    } else {
      vassert(!(CurrentFunc->Flags&FUNC_StructMethod));
    }
    // process `self(ClassName)`
    if (CurrentFunc->SelfTypeName != NAME_None) {
      if (SelfStruct) {
        ParseError(CurrentFunc->Loc, "You cannot force self for struct method `%s`", *CurrentFunc->GetFullName());
        CurrentFunc->SelfTypeName = NAME_None;
      } else {
        VClass *newSelfClass = VMemberBase::StaticFindClass(CurrentFunc->SelfTypeName);
        if (!newSelfClass) {
          ParseError(CurrentFunc->Loc, "No such class `%s` for forced self in method `%s`", *CurrentFunc->SelfTypeName, *CurrentFunc->GetFullName());
        } else {
          if (newSelfClass) {
            VClass *cc = SelfClass;
            while (cc && cc != newSelfClass) cc = cc->ParentClass;
                 if (!cc) ParseError(CurrentFunc->Loc, "Forced self `%s` for class `%s`, which is not super (method `%s`)", *CurrentFunc->SelfTypeName, SelfClass->GetName(), *CurrentFunc->GetFullName());
            //else if (cc == SelfClass) ParseWarning(CurrentFunc->Loc, "Forced self `%s` for the same class (old=%s; new=%s) (method `%s`)", *CurrentFunc->SelfTypeName, *SelfClass->GetFullName(), *cc->GetFullName(), *CurrentFunc->GetFullName());
            //else GLog.Logf(NAME_Debug, "%s: forced class `%s` for class `%s` (method `%s`)", *CurrentFunc->Loc.toStringNoCol(), *CurrentFunc->SelfTypeName, SelfClass->GetName(), *CurrentFunc->GetFullName());
            if (!cc->Defined) ParseError(CurrentFunc->Loc, "Forced self class `%s` is not defined for method `%s`", *CurrentFunc->SelfTypeName, *CurrentFunc->GetFullName());
            SelfClass = cc;
            if (CurrentFunc->SelfTypeClass && CurrentFunc->SelfTypeClass != cc) VCFatalError("internal compiler error (SelfTypeName)");
            CurrentFunc->SelfTypeClass = cc;
          } else {
            ParseError(CurrentFunc->Loc, "Forced self `%s` for nothing (wtf?!) (method `%s`)", *CurrentFunc->SelfTypeName, *CurrentFunc->GetFullName());
          }
        }
      }
    }
  }

  stackInit();
}


//==========================================================================
//
//  VEmitContext::~VEmitContext
//
//==========================================================================
VEmitContext::~VEmitContext () {
  releaseEmitBuffer(EmitBuf);
  EmitBuf = nullptr;
}


//==========================================================================
//
//  VEmitContext::allocInstruction
//
//==========================================================================
FInstruction &VEmitContext::allocInstruction () {
  if (!EmitBuf) EmitBuf = allocEmitBuffer();
  return EmitBuf->allocInstruction();
}


//==========================================================================
//
//  VEmitContext::getInstrCount
//
//==========================================================================
int VEmitContext::getInstrCount () noexcept {
  return (EmitBuf ? EmitBuf->getInstrCount() : 0);
}


//==========================================================================
//
//  VEmitContext::getInstr
//
//==========================================================================
FInstruction &VEmitContext::getInstr (int idx) {
  vassert(EmitBuf);
  return EmitBuf->getInstr(idx);
}


//==========================================================================
//
//  VEmitContext::EndCode
//
//==========================================================================
void VEmitContext::EndCode () {
  //if (lastFin) VCFatalError("Internal compiler error: unbalanced finalizers");
  //if (lastBC) VCFatalError("Internal compiler error: unbalanced break/cont");
  //if (scopeList.length() != 0) VCFatalError("Internal compiler error: unbalanced scopes");

  // fix-up labels.
  for (int i = 0; i < Fixups.length(); ++i) {
    if (Labels[Fixups[i].LabelIdx] < 0) VCFatalError("Label was not marked");
    if (Fixups[i].Arg == 1) {
      getInstr(Fixups[i].Pos).Arg1 = Labels[Fixups[i].LabelIdx];
    } else {
      getInstr(Fixups[i].Pos).Arg2 = Labels[Fixups[i].LabelIdx];
    }
  }

#ifdef OPCODE_STATS
  for (int i = 0; i < getInstrCount(); ++i) {
    ++StatementInfo[getInstr(i).Opcode].usecount;
  }
#endif

  // dummy finishing instruction should always present
  FInstruction &Dummy = allocInstruction();
  Dummy.Opcode = OPC_Done;

  CurrentFunc->Instructions.setLength(getInstrCount());
  for (int f = 0; f < getInstrCount(); ++f) CurrentFunc->Instructions[f] = getInstr(f);

  //CurrentFunc->Instructions.condense(); // just in case

  releaseEmitBuffer(EmitBuf);
  EmitBuf = nullptr;

  //CurrentFunc->DumpAsm();
}


//==========================================================================
//
//  VEmitContext::ClearLocalDefs
//
//==========================================================================
void VEmitContext::ClearLocalDefs () {
  LocalDefs.Clear();
  stackInit();
}


//==========================================================================
//
//  VEmitContext::stackInit
//
//==========================================================================
void VEmitContext::stackInit () {
  memset(slotInfo, SlotUnused, sizeof(slotInfo));
}


//==========================================================================
//
//  VEmitContext::stackAlloc
//
//  size in stack slots; returns -1 on error
//
//==========================================================================
int VEmitContext::stackAlloc (int size, bool *reused) {
  if (reused) *reused = true; // just in case
  if (size < 0 || size > MaxStackSlots) return -1;

  // for zero-size locals (just in case), try to get an unused slot
  if (size == 0) {
    int seenFree = -1;
    for (int pos = 0; pos < MaxStackSlots; ++pos) {
      if (slotInfo[pos] == SlotUnused) {
        if (reused) *reused = false;
        return pos;
      }
      if (seenFree < 0 && slotInfo[pos] == SlotFree) seenFree = pos;
    }
    if (reused) *reused = true;
    return seenFree;
  }

  // if we are not inside a loop, try to allocate a fresh stack slot, to avoid useless zeroing
  if (!InLoop) {
    for (int pos = 0; pos < MaxStackSlots; ++pos) {
      if (slotInfo[pos] == SlotUnused) {
        int end = pos+1;
        while (end < MaxStackSlots && end-pos < size && slotInfo[end] == SlotUnused) ++end;
        // does it fit?
        if (end-pos >= size) {
          // mark used
          memset(slotInfo+pos, SlotUsed, size);
          if (reused) *reused = false;
          return pos;
        }
        // skip this gap (loop increment will skip end non-empty slot automatically)
        pos = end;
      }
    }
  }

  // no free room (or in a loop), try first good slot
  for (int pos = 0; pos < MaxStackSlots; ++pos) {
    unsigned char slotflags = slotInfo[pos];
    if (slotflags != SlotUsed) {
      int end = pos+1;
      while (end < MaxStackSlots && end-pos < size) {
        const unsigned char ssl = slotInfo[end];
        if (ssl == SlotUsed) break;
        slotflags |= ssl;
        ++end;
      }
      // does it fit?
      if (end-pos >= size) {
        // mark used
        memset(slotInfo+pos, SlotUsed, size);
        if (reused) *reused = (slotflags != SlotUnused);
        return pos;
      }
      // skip this gap (loop increment will skip end used slot automatically)
      pos = end;
    }
  }

  // no such free slot
  return -1;
}


//==========================================================================
//
//  VEmitContext::stackFree
//
//==========================================================================
void VEmitContext::stackFree (int pos, int size) {
  vassert(pos >= 0);
  vassert(size >= 0);
  vassert(pos < MaxStackSlots);
  vassert(size <= MaxStackSlots);
  vassert(pos+size <= MaxStackSlots);
  if (size == 0) return;

  #ifndef VV_EMIT_DISABLE_LOCAL_REUSE
  const int end = pos+size;
  for (int f = pos; f < end; ++f) {
    vassert(slotInfo[f] == SlotUsed);
    slotInfo[f] = SlotFree;
  }
  #endif
}


//==========================================================================
//
//  VEmitContext::AllocateLocalSlot
//
//  allocate stack slot for this local
//
//==========================================================================
void VEmitContext::AllocateLocalSlot (int idx, bool aUnsafe) {
  vassert(idx >= 0 && idx < LocalDefs.length());
  VLocalVarDef &loc = LocalDefs[idx];
  if (loc.invalid) return; // already dead
  vassert(loc.Offset == -666);
  vassert(loc.stackSize >= 0);
  bool realloced = true; // just in case
  const int ofs = stackAlloc(loc.stackSize, &realloced);
  if (ofs < 0) {
    ParseError(loc.Loc, "too many local vars");
    loc.Offset = 1;
    loc.invalid = true;
    loc.reused = false; // it doesn't matter
    loc.isUnsafe = false;
  } else {
    #ifdef VV_EMIT_DUMP_SLOT_ALLOC
    GLog.Logf(NAME_Debug, "%s: allocated local `%s` (idx=%d; ofs=%d; size=%d; realloced=%d)", *loc.Loc.toStringNoCol(), *loc.Name, idx, ofs, loc.stackSize, (int)realloced);
    #endif
    loc.Offset = ofs;
    // if we are in a loop, always clear it
    loc.reused = (realloced || InLoop > 0);
    loc.isUnsafe = aUnsafe;
  }
}


//==========================================================================
//
//  VEmitContext::ReleaseLocalSlot
//
//==========================================================================
void VEmitContext::ReleaseLocalSlot (int idx) {
  vassert(idx >= 0 && idx < LocalDefs.length());
  VLocalVarDef &loc = LocalDefs[idx];
  if (loc.invalid) return; // already dead
  vassert(loc.Offset >= 0);
  vassert(loc.stackSize >= 0);
  #ifdef VV_EMIT_DUMP_SLOT_ALLOC
  GLog.Logf(NAME_Debug, "%s: freeing local `%s` (idx=%d; ofs=%d; size=%d; realloced=%d)", *loc.Loc.toStringNoCol(), *loc.Name, idx, loc.Offset, loc.stackSize, (int)loc.reused);
  #endif
  stackFree(loc.Offset, loc.stackSize);
  loc.Offset = -666;
  loc.isUnsafe = false;
}


//==========================================================================
//
//  VEmitContext::ReserveStack
//
//  reserve stack slots; used to setup function arguments
//
//==========================================================================
int VEmitContext::ReserveStack (int size) {
  vassert(size >= 0 && size <= MaxStackSlots);
  int pos = 0;
  while (pos < MaxStackSlots) {
    const unsigned char currslot = slotInfo[pos];
    if (currslot == SlotUnused) break;
    if (currslot == SlotFree) VCFatalError("cannot reserve stack after any local allocation");
    ++pos;
  }
  if (pos+size > MaxStackSlots) return -1; // oops
  // check stack integrity (why not?)
  for (int f = pos; f < pos+size; ++f) {
    if (slotInfo[pos] != SlotUnused) VCFatalError("cannot reserve stack after any local allocation");
  }
  if (size > 0) memset(slotInfo+pos, SlotUsed, size);
  return pos;
}


//==========================================================================
//
//  VEmitContext::ReserveLocalSlot
//
//  reserve slot for this local; used to setup function arguments
//
//==========================================================================
void VEmitContext::ReserveLocalSlot (int idx) {
  vassert(idx >= 0 && idx < LocalDefs.length());
  VLocalVarDef &loc = LocalDefs[idx];
  vassert(!loc.invalid);
  vassert(!loc.reused);
  vassert(loc.Offset == -666);
  vassert(loc.stackSize >= 0);
  //int size = (loc.ParamFlags&(FPARM_Out|FPARM_Ref) ? 1 : loc.stackSize);
  const int ofs = ReserveStack(loc.stackSize);
  if (ofs < 0) {
    ParseError(loc.Loc, "too many local vars");
    loc.Offset = 1;
    loc.invalid = true;
  } else {
    loc.Offset = ofs;
  }
}


//==========================================================================
//
//  VEmitContext::CalcUsedStackSize
//
//==========================================================================
int VEmitContext::CalcUsedStackSize () const noexcept {
  // perform a forward scan, it is slightly better for cache
  // (it doesn't matter much, because this is called only once, but still...)
  int res = 1; // always reserve at least one, we will rarely have a pure function that does nothing at all
  for (int pos = 1; pos < MaxStackSlots; ++pos) {
    if (slotInfo[pos] != SlotUnused) res = pos+1;
  }
  return res;
}


//==========================================================================
//
//  VEmitContext::NewLocal
//
//==========================================================================
VLocalVarDef &VEmitContext::NewLocal (VName aname, const VFieldType &atype,
                                      bool aUnsafe, const TLocation &aloc, vuint32 pflags)
{
  const int ssz = (pflags&(FPARM_Out|FPARM_Ref) ? 1 : atype.GetStackSize());

  VLocalVarDef &loc = LocalDefs.Alloc();
  loc.Name = aname;
  loc.Loc = aloc;
  loc.Offset = -666;
  loc.Type = atype;
  loc.Visible = true;
  loc.WasRead = false;
  loc.WasWrite = false;
  loc.ParamFlags = pflags;
  loc.ldindex = LocalDefs.length()-1;
  loc.reused = false;
  loc.stackSize = ssz;
  loc.invalid = false;
  loc.isUnsafe = aUnsafe;

  return loc;
}


//==========================================================================
//
//  VEmitContext::GetLocalByIndex
//
//==========================================================================
VLocalVarDef &VEmitContext::GetLocalByIndex (int idx) noexcept {
  if (idx < 0 || idx >= LocalDefs.length()) VCFatalError("VC INTERNAL COMPILER ERROR IN `VEmitContext::GetLocalByIndex()`");
  return LocalDefs[idx];
}


//==========================================================================
//
//  VEmitContext::MarkLocalReadByIdx
//
//==========================================================================
void VEmitContext::MarkLocalReadByIdx (int idx) noexcept {
  if (idx >= 0 && idx < LocalDefs.length()) LocalDefs[idx].WasRead = true;
}


//==========================================================================
//
//  VEmitContext::MarkLocalWrittenByIdx
//
//==========================================================================
void VEmitContext::MarkLocalWrittenByIdx (int idx) noexcept {
  if (idx >= 0 && idx < LocalDefs.length()) LocalDefs[idx].WasWrite = true;
}


//==========================================================================
//
//  VEmitContext::MarkLocalUsedByIdx
//
//==========================================================================
void VEmitContext::MarkLocalUsedByIdx (int idx) noexcept {
  if (idx >= 0 && idx < LocalDefs.length()) LocalDefs[idx].WasRead = LocalDefs[idx].WasWrite = true;
}


//==========================================================================
//
//  VEmitContext::IsLocalReadByIdx
//
//==========================================================================
bool VEmitContext::IsLocalReadByIdx (int idx) const noexcept {
  return (idx >= 0 && idx < LocalDefs.length() ? LocalDefs[idx].WasRead : false);
}


//==========================================================================
//
//  VEmitContext::IsLocalWrittenByIdx
//
//==========================================================================
bool VEmitContext::IsLocalWrittenByIdx (int idx) const noexcept {
  return (idx >= 0 && idx < LocalDefs.length() ? LocalDefs[idx].WasWrite : false);
}


//==========================================================================
//
//  VEmitContext::IsLocalUsedByIdx
//
//==========================================================================
bool VEmitContext::IsLocalUsedByIdx (int idx) const noexcept {
  return (idx >= 0 && idx < LocalDefs.length() ? LocalDefs[idx].WasRead && LocalDefs[idx].WasWrite : false);
}


//==========================================================================
//
//  VEmitContext::SaveLocalsReadWrite
//
//==========================================================================
void VEmitContext::SaveLocalsReadWrite (LocalsRWState &state) {
  state.setLengthNoResize(LocalDefs.length());
  for (int f = 0; f < LocalDefs.length(); f += 1) {
    state[f].WasRead = LocalDefs[f].WasRead;
    state[f].WasWrite = LocalDefs[f].WasWrite;
    state[f].assign = LocalDefs[f].assign;
  }
}


//==========================================================================
//
//  VEmitContext::RestoreLocalsReadWrite
//
//==========================================================================
void VEmitContext::RestoreLocalsReadWrite (const LocalsRWState &state) {
  vassert(state.length() == LocalDefs.length());
  for (int f = 0; f < LocalDefs.length(); f += 1) {
    LocalDefs[f].WasRead = state[f].WasRead;
    LocalDefs[f].WasWrite = state[f].WasWrite;
    LocalDefs[f].assign = state[f].assign;
  }
}


//==========================================================================
//
//  VEmitContext::CheckForLocalVar
//
//==========================================================================
int VEmitContext::CheckForLocalVar (VName Name) {
  if (Name == NAME_None) return -1;
  for (auto &&it : LocalDefs.itemsIdx()) {
    const VLocalVarDef &loc = it.value();
    if (!loc.Visible) continue;
    if (VObject::cliCaseSensitiveLocals) {
      if (loc.Name == Name) return it.index();
    } else {
      if (VStr::strEquCI(*loc.Name, *Name)) return it.index();
    }
  }
  return -1;
}


//==========================================================================
//
//  VEmitContext::CheckForLocalVarCI
//
//==========================================================================
int VEmitContext::CheckForLocalVarCI (VName Name) {
  if (Name == NAME_None) return -1;
  for (auto &&it : LocalDefs.itemsIdx()) {
    const VLocalVarDef &loc = it.value();
    if (!loc.Visible) continue;
    if (VStr::strEquCI(*loc.Name, *Name)) return it.index();
  }
  return -1;
}


//==========================================================================
//
//  VEmitContext::CheckLocalDecl
//
//  returns `true` if there were no errors
//
//==========================================================================
bool VEmitContext::CheckLocalDecl (VName locname, const TLocation &locloc) {
  if (locname == NAME_None) return true; // just in case
  bool res = true;
  auto ccloc = CheckForLocalVar(locname);
  if (ccloc != -1) {
    //ParseError(locloc, "Redefined identifier `%s`", *locname);
    ParseError(locloc, "Redeclared local `%s` (previous declaration is at %s)", *locname, *GetLocalByIndex(ccloc).Loc.toStringNoCol());
    res = false;
  }
  if (VObject::cliCaseSensitiveLocals) {
    ccloc = CheckForLocalVarCI(locname);
    if (ccloc != -1) {
      ParseWarning(locloc, "Local `%s` is case-equal to `%s` at %s", *locname, *GetLocalByIndex(ccloc).Name, *GetLocalByIndex(ccloc).Loc.toStringNoCol());
      res = false;
    }
  } else {
    if (SelfClass) {
      SaveIntVal ov(&VObject::cliCaseSensitiveFields, 0);

      VConstant *oldConst = SelfClass->FindConstant(locname);
      if (oldConst) { ParseError(locloc, "Local `%s` conflicts with constant at %s", *locname, *oldConst->Loc.toStringNoCol()); res = false; }

      /*
      oldConst = SelfClass->FindDecorateConstant(VStr(locname));
      if (oldConst) { ParseError(locloc, "Local `%s` conflicts with decorate constant at %s", *locname, *oldConst->Loc.toStringNoCol()); res = false; }
      */

      VField *oldField = SelfClass->FindField(locname);
      if (oldField) { ParseError(locloc, "Local `%s` conflicts with field at %s", *locname, *oldField->Loc.toStringNoCol()); res = false; }

      VMethod *oldMethod = SelfClass->FindMethod(locname);
      if (oldMethod) { ParseError(locloc, "Local `%s` conflicts with method at %s", *locname, *oldMethod->Loc.toStringNoCol()); res = false; }

      VProperty *oldProp = SelfClass->FindProperty(locname);
      if (oldProp) { ParseError(locloc, "Local `%s` conflicts with property at %s", *locname, *oldProp->Loc.toStringNoCol()); res = false; }

      /*
      oldProp = SelfClass->FindDecorateProperty(VStr(locname));
      if (oldProp) { ParseError(locloc, "Local `%s` conflicts with decorate property at %s", *locname, *oldProp->Loc.toStringNoCol()); res = false; }
      */
    }
  }
  return res;
}


//==========================================================================
//
//  VEmitContext::GetLocalVarType
//
//==========================================================================
VFieldType VEmitContext::GetLocalVarType (int idx) {
  if (idx < 0 || idx >= LocalDefs.length()) return VFieldType(TYPE_Unknown);
  return LocalDefs[idx].Type;
}


//==========================================================================
//
//  VEmitContext::IsLocalUnsafe
//
//==========================================================================
bool VEmitContext::IsLocalUnsafe (int idx) {
  if (idx < 0 || idx >= LocalDefs.length()) return false;
  return LocalDefs[idx].isUnsafe;
}


//==========================================================================
//
//  VEmitContext::DefineLabel
//
//==========================================================================
VLabel VEmitContext::DefineLabel () {
  Labels.Append(-1);
  return VLabel(Labels.length()-1);
}


//==========================================================================
//
//  VEmitContext::MarkLabel
//
//==========================================================================
void VEmitContext::MarkLabel (VLabel l) {
  if (l.Index < 0 || l.Index >= Labels.length()) VCFatalError("Bad label index %d", l.Index);
  if (Labels[l.Index] >= 0) VCFatalError("Label has already been marked");
  Labels[l.Index] = getInstrCount();
}


//==========================================================================
//
//  VEmitContext::AddStatement
//
//==========================================================================
void VEmitContext::AddStatement (int statement, const TLocation &aloc) {
  if (StatementInfo[statement].Args != OPCARGS_None) VCFatalError("Opcode doesn't take 0 params");
  FInstruction &I = allocInstruction();
  I.Opcode = statement;
  I.Arg1 = 0;
  I.Arg2 = 0;
  I.loc = aloc;
}


//==========================================================================
//
//  VEmitContext::AddStatement
//
//==========================================================================
void VEmitContext::AddStatement (int statement, int parm1, const TLocation &aloc) {
  if (StatementInfo[statement].Args != OPCARGS_Byte &&
      StatementInfo[statement].Args != OPCARGS_Short &&
      StatementInfo[statement].Args != OPCARGS_Int &&
      StatementInfo[statement].Args != OPCARGS_String)
  {
    VCFatalError("Opcode doesn't take 1 param");
  }
  FInstruction &I = allocInstruction();
  I.Opcode = statement;
  I.Arg1 = parm1;
  I.Arg2 = 0;
  I.loc = aloc;
}


//==========================================================================
//
//  VEmitContext::AddStatement
//
//==========================================================================
void VEmitContext::AddStatement (int statement, float FloatArg, const TLocation &aloc) {
  if (StatementInfo[statement].Args != OPCARGS_Int) VCFatalError("Opcode doesn't take float argument");
  FInstruction &I = allocInstruction();
  I.Opcode = statement;
  //I.Arg1 = *(vint32 *)&FloatArg;
  //memcpy(&I.Arg1, &FloatArg, 4);
  I.Arg1F = FloatArg;
  I.Arg1IsFloat = true;
  I.loc = aloc;
}


//==========================================================================
//
//  VEmitContext::AddStatement
//
//==========================================================================
void VEmitContext::AddStatement (int statement, VName NameArg, const TLocation &aloc) {
  if (StatementInfo[statement].Args != OPCARGS_Name) VCFatalError("Opcode doesn't take name argument");
  FInstruction &I = allocInstruction();
  I.Opcode = statement;
  I.NameArg = NameArg;
  I.loc = aloc;
}


//==========================================================================
//
//  VEmitContext::AddStatement
//
//==========================================================================
void VEmitContext::AddStatement (int statement, VMemberBase *Member, const TLocation &aloc) {
  if (StatementInfo[statement].Args != OPCARGS_Member &&
      StatementInfo[statement].Args != OPCARGS_FieldOffset &&
      StatementInfo[statement].Args != OPCARGS_VTableIndex)
  {
    VCFatalError("Opcode doesn't take member as argument");
  }
  FInstruction &I = allocInstruction();
  I.Opcode = statement;
  I.Member = Member;
  I.loc = aloc;
}


//==========================================================================
//
//  VEmitContext::AddStatement
//
//==========================================================================
void VEmitContext::AddStatement (int statement, VMemberBase *Member, int Arg, const TLocation &aloc) {
  if (StatementInfo[statement].Args != OPCARGS_VTableIndex_Byte &&
      StatementInfo[statement].Args != OPCARGS_FieldOffset_Byte &&
      StatementInfo[statement].Args != OPCARGS_Member_Int)
  {
    VCFatalError("Opcode doesn't take member and byte as argument");
  }
  FInstruction &I = allocInstruction();
  I.Opcode = statement;
  I.Member = Member;
  I.Arg2 = Arg;
  I.loc = aloc;
}


//==========================================================================
//
//  VEmitContext::AddStatement
//
//==========================================================================
void VEmitContext::AddStatement (int statement, const VFieldType &TypeArg, const TLocation &aloc) {
  if (StatementInfo[statement].Args != OPCARGS_TypeSize &&
      StatementInfo[statement].Args != OPCARGS_Type &&
      StatementInfo[statement].Args != OPCARGS_A2DDimsAndSize)
  {
    VCFatalError("Opcode doesn't take type as argument");
  }
  FInstruction &I = allocInstruction();
  I.Opcode = statement;
  I.TypeArg = TypeArg;
  I.loc = aloc;
}


//==========================================================================
//
//  VEmitContext::AddStatement
//
//==========================================================================
void VEmitContext::AddStatement (int statement, const VFieldType &TypeArg, int Arg, const TLocation &aloc) {
  if (StatementInfo[statement].Args != OPCARGS_Type_Int &&
      StatementInfo[statement].Args != OPCARGS_ArrElemType_Int &&
      StatementInfo[statement].Args != OPCARGS_TypeAD)
  {
    VCFatalError("Opcode doesn't take type as argument");
  }
  FInstruction &I = allocInstruction();
  I.Opcode = statement;
  I.TypeArg = TypeArg;
  I.Arg2 = Arg;
  I.loc = aloc;
}


//==========================================================================
//
//  VEmitContext::AddStatement
//
//==========================================================================
void VEmitContext::AddStatement (int statement, const VFieldType &TypeArg, const VFieldType &TypeArg1, int OpCode, const TLocation &aloc) {
  if (StatementInfo[statement].Args != OPCARGS_TypeDD) {
    VCFatalError("Opcode doesn't take types as argument");
  }
  FInstruction &I = allocInstruction();
  I.Opcode = statement;
  I.TypeArg = TypeArg;
  I.TypeArg1 = TypeArg1;
  I.Arg2 = OpCode;
  I.loc = aloc;
}


//==========================================================================
//
//  VEmitContext::AddStatement
//
//==========================================================================
void VEmitContext::AddStatement (int statement, VLabel Lbl, const TLocation &aloc) {
  if (StatementInfo[statement].Args != OPCARGS_BranchTarget) VCFatalError("Opcode doesn't take label as argument");
  FInstruction &I = allocInstruction();
  I.Opcode = statement;
  I.Arg1 = 0;
  I.Arg2 = 0;

  VLabelFixup &Fix = Fixups.Alloc();
  Fix.Pos = getInstrCount()-1;
  Fix.Arg = 1;
  Fix.LabelIdx = Lbl.Index;
  I.loc = aloc;
}


//==========================================================================
//
//  VEmitContext::AddStatement
//
//==========================================================================
void VEmitContext::AddStatement (int statement, int parm1, VLabel Lbl, const TLocation &aloc) {
  if (StatementInfo[statement].Args != OPCARGS_ByteBranchTarget &&
      StatementInfo[statement].Args != OPCARGS_ShortBranchTarget &&
      StatementInfo[statement].Args != OPCARGS_IntBranchTarget &&
      StatementInfo[statement].Args != OPCARGS_NameBranchTarget)
  {
    VCFatalError("Opcode doesn't take 2 params");
  }
  FInstruction &I = allocInstruction();
  I.Opcode = statement;
  I.Arg1 = parm1;
  I.Arg2 = 0;

  VLabelFixup &Fix = Fixups.Alloc();
  Fix.Pos = getInstrCount()-1;
  Fix.Arg = 2;
  Fix.LabelIdx = Lbl.Index;
  I.loc = aloc;
}


//==========================================================================
//
//  VEmitContext::AddBuiltin
//
//==========================================================================
void VEmitContext::AddBuiltin (int b, const TLocation &aloc) {
  //if (StatementInfo[statement].Args != OPCARGS_Builtin) VCFatalError("Opcode doesn't take builtin");
  FInstruction &I = allocInstruction();
  I.Opcode = OPC_Builtin;
  I.Arg1 = b;
  I.Arg2 = 0;
  I.loc = aloc;
}


//==========================================================================
//
//  VEmitContext::AddCVarBuiltin
//
//==========================================================================
void VEmitContext::AddCVarBuiltin (int b, VName n, const TLocation &aloc) {
  FInstruction &I = allocInstruction();
  I.Opcode = OPC_BuiltinCVar;
  I.Arg1 = b;
  I.Arg2 = n.GetIndex();
  I.loc = aloc;
}


//==========================================================================
//
//  VEmitContext::EmitPushNumber
//
//==========================================================================
void VEmitContext::EmitPushNumber (int Val, const TLocation &aloc) {
       if (Val == 0) AddStatement(OPC_PushNumber0, aloc);
  else if (Val == 1) AddStatement(OPC_PushNumber1, aloc);
  else if (Val >= 0 && Val < 256) AddStatement(OPC_PushNumberB, Val, aloc);
  //else if (Val >= MIN_VINT16 && Val <= MAX_VINT16) AddStatement(OPC_PushNumberS, Val, aloc);
  else AddStatement(OPC_PushNumber, Val, aloc);
}


//==========================================================================
//
//  VEmitContext::EmitLocalAddress
//
//==========================================================================
void VEmitContext::EmitLocalAddress (int Ofs, const TLocation &aloc) {
       if (Ofs == 0) AddStatement(OPC_LocalAddress0, aloc);
  else if (Ofs == 1) AddStatement(OPC_LocalAddress1, aloc);
  else if (Ofs == 2) AddStatement(OPC_LocalAddress2, aloc);
  else if (Ofs == 3) AddStatement(OPC_LocalAddress3, aloc);
  else if (Ofs == 4) AddStatement(OPC_LocalAddress4, aloc);
  else if (Ofs == 5) AddStatement(OPC_LocalAddress5, aloc);
  else if (Ofs == 6) AddStatement(OPC_LocalAddress6, aloc);
  else if (Ofs == 7) AddStatement(OPC_LocalAddress7, aloc);
  else if (Ofs < 256) AddStatement(OPC_LocalAddressB, Ofs, aloc);
  else if (Ofs < MAX_VINT16) AddStatement(OPC_LocalAddressS, Ofs, aloc);
  else AddStatement(OPC_LocalAddress, Ofs, aloc);
}


//==========================================================================
//
//  VEmitContext::EmitPushPointedCode
//
//==========================================================================
void VEmitContext::EmitPushPointedCode (VFieldType type, const TLocation &aloc) {
  switch (type.Type) {
    case TYPE_Int:
    case TYPE_Float:
    case TYPE_Name:
      AddStatement(OPC_PushPointed, aloc);
      break;
    case TYPE_Byte:
      AddStatement(OPC_PushPointedByte, aloc);
      break;
    case TYPE_Bool:
           if (type.BitMask&0x000000ff) AddStatement(OPC_PushBool0, (int)(type.BitMask), aloc);
      else if (type.BitMask&0x0000ff00) AddStatement(OPC_PushBool1, (int)(type.BitMask>>8), aloc);
      else if (type.BitMask&0x00ff0000) AddStatement(OPC_PushBool2, (int)(type.BitMask>>16), aloc);
      else AddStatement(OPC_PushBool3, (int)(type.BitMask>>24), aloc);
      break;
    case TYPE_Pointer:
    case TYPE_Reference:
    case TYPE_Class:
    case TYPE_State:
      AddStatement(OPC_PushPointedPtr, aloc);
      break;
    case TYPE_Vector:
      AddStatement(OPC_VPushPointed, aloc);
      break;
    case TYPE_String:
      AddStatement(OPC_PushPointedStr, aloc);
      break;
    case TYPE_Delegate:
      AddStatement(OPC_PushPointedDelegate, aloc);
      break;
    case TYPE_SliceArray:
      AddStatement(OPC_PushPointedSlice, aloc);
      break;
    default:
      ParseError(aloc, "Bad push pointed");
      break;
  }
}


//==========================================================================
//
//  VEmitContext::EmitLocalValue
//
//==========================================================================
void VEmitContext::EmitLocalValue (int lcidx, const TLocation &aloc, int xofs) {
  if (lcidx < 0 || lcidx >= LocalDefs.length()) VCFatalError("VC: internal compiler error (VEmitContext::EmitLocalValue): lcidx=%d; max=%d", lcidx, LocalDefs.length());
  const VLocalVarDef &loc = LocalDefs[lcidx];
  int Ofs = loc.Offset+xofs;
  if (Ofs < 0 || Ofs > 1024*1024*32) VCFatalError("VC: internal compiler error (VEmitContext::EmitLocalValue): ofs=%d (lcidx=%d; name=`%s`; %s)", Ofs, lcidx, *loc.Name, *loc.Loc.toStringNoCol());
  if (Ofs < 256 && loc.Type.Type != TYPE_Delegate) {
    switch (loc.Type.Type) {
      case TYPE_Vector:
        AddStatement(OPC_VLocalValueB, Ofs, aloc);
        break;
      case TYPE_String:
        AddStatement(OPC_StrLocalValueB, Ofs, aloc);
        break;
      case TYPE_Bool:
        if (loc.Type.BitMask != 1) ParseError(aloc, "Strange local bool mask");
        /* fallthrough */
      default:
        if (Ofs >= 0 && Ofs <= 7) AddStatement(OPC_LocalValue0+Ofs, aloc);
        else AddStatement(OPC_LocalValueB, Ofs, aloc);
        break;
    }
  } else {
    EmitLocalAddress(loc.Offset, aloc);
    EmitPushPointedCode(loc.Type, aloc);
  }
}


//==========================================================================
//
//  VEmitContext::EmitLocalPtrValue
//
//==========================================================================
void VEmitContext::EmitLocalPtrValue (int lcidx, const TLocation &aloc, int xofs) {
  if (lcidx < 0 || lcidx >= LocalDefs.length()) VCFatalError("VC: internal compiler error (VEmitContext::EmitLocalPtrValue): lcidx=%d; max=%d", lcidx, LocalDefs.length());
  const VLocalVarDef &loc = LocalDefs[lcidx];
  int Ofs = loc.Offset+xofs;
  if (Ofs < 0 || Ofs > 1024*1024*32) VCFatalError("VC: internal compiler error (VEmitContext::EmitLocalPtrValue): ofs=%d (lcidx=%d; name=`%s`; %s)", Ofs, lcidx, *loc.Name, *loc.Loc.toStringNoCol());
  if (Ofs < 256) {
    if (Ofs >= 0 && Ofs <= 7) AddStatement(OPC_LocalValue0+Ofs, aloc);
    else AddStatement(OPC_LocalValueB, Ofs, aloc);
  } else {
    EmitLocalAddress(loc.Offset, aloc);
    AddStatement(OPC_PushPointedPtr, aloc);
  }
}


//==========================================================================
//
//  VEmitContext::EmitLocalZero
//
//==========================================================================
void VEmitContext::EmitLocalZero (int locidx, const TLocation &aloc) {
  vassert(locidx >= 0 && locidx < LocalDefs.length());

  const VLocalVarDef &loc = LocalDefs[locidx];

  // don't touch out/ref parameters
  if (loc.ParamFlags&(FPARM_Out|FPARM_Ref)) return;

  vassert(loc.Offset >= 0);

  if (loc.Type.Type == TYPE_Vector) {
    EmitLocalAddress(loc.Offset, aloc);
    AddStatement(OPC_ZeroSlotsByPtr, loc.Type.GetStackSize(), aloc);
    return;
  }

  if (loc.Type.Type == TYPE_Struct) {
    EmitLocalAddress(loc.Offset, aloc);
    //GLog.Logf(NAME_Debug, "ZEROSTRUCT<%s>: size=%d (%s:%d)", *loc.Type.Struct->Name, loc.Type.GetSize(), *loc.Type.GetName(), loc.Type.Type);
    // we cannot use `OPC_ZeroByPtr` here, because struct size is not calculated yet
    //AddStatement(OPC_ZeroByPtr, loc.Type.GetSize(), aloc);
    AddStatement(OPC_ZeroPointedStruct, loc.Type.Struct, aloc);
    return;
  }

  if (loc.Type.Type == TYPE_Array) {
    //k8: can we use size caluclation here?
    // i don't think so, because struct size is not calculated yet (it is done in postload [why, btw?])
    // so until i fix that, clear each array element separately
    for (int j = 0; j < loc.Type.GetArrayDim(); ++j) {
      EmitLocalAddress(loc.Offset, aloc);
      EmitPushNumber(j, aloc);
      AddStatement(OPC_ArrayElement, loc.Type.GetArrayInnerType(), aloc);
      if (loc.Type.ArrayInnerType == TYPE_Struct) {
        AddStatement(OPC_ZeroPointedStruct, loc.Type.Struct, aloc);
      } else {
        AddStatement(OPC_ZeroByPtr, loc.Type.GetSize(), aloc);
      }
    }
    return;
  }

  if (loc.Type.Type == TYPE_String ||
      loc.Type.Type == TYPE_Dictionary ||
      loc.Type.Type == TYPE_DynamicArray)
  {
    EmitLocalAddress(loc.Offset, aloc);
    AddStatement(OPC_ZeroSlotsByPtr, loc.Type.GetStackSize(), aloc);
    return;
  }

  // clear whole slot for boolean, just to be sure
  if (loc.Type.Type == TYPE_Bool) {
    EmitLocalAddress(loc.Offset, aloc);
    AddStatement(OPC_ZeroSlotsByPtr, loc.Type.GetStackSize(), aloc);
    return;
  }

  EmitLocalAddress(loc.Offset, aloc);
  AddStatement(OPC_ZeroByPtr, loc.Type.GetSize(), aloc);
}


//==========================================================================
//
//  VEmitContext::EmitLocalDtor
//
//==========================================================================
void VEmitContext::EmitLocalDtor (int locidx, const TLocation &aloc) {
  vassert(locidx >= 0 && locidx < LocalDefs.length());

  const VLocalVarDef &loc = LocalDefs[locidx];

  // don't touch out/ref parameters
  if (loc.ParamFlags&(FPARM_Out|FPARM_Ref)) return;

  vassert(loc.Offset >= 0);

  if (loc.Type.Type == TYPE_String) {
    EmitLocalAddress(loc.Offset, aloc);
    AddStatement(OPC_ClearPointedStr, aloc);
    return;
  }

  if (loc.Type.Type == TYPE_Dictionary) {
    EmitLocalAddress(loc.Offset, aloc);
    AddStatement(OPC_DictDispatch, loc.Type.GetDictKeyType(), loc.Type.GetDictValueType(), OPC_DictDispatch_ClearPointed, aloc);
    return;
  }

  if (loc.Type.Type == TYPE_DynamicArray) {
    EmitLocalAddress(loc.Offset, aloc);
    AddStatement(OPC_PushNumber0, aloc);
    AddStatement(OPC_DynArrayDispatch, loc.Type.GetArrayInnerType(), OPC_DynArrDispatch_DynArraySetNum, aloc);
    return;
  }

  if (loc.Type.Type == TYPE_Struct) {
    // call struct dtors
    if (loc.Type.Struct->NeedsMethodDestruction()) {
      for (VStruct *st = loc.Type.Struct; st; st = st->ParentStruct) {
        VMethod *mt = st->FindDtor(false); // non-recursive
        if (mt) {
          vassert(mt->NumParams == 0);
          // emit `self`
          if (!mt->IsStatic()) EmitLocalAddress(loc.Offset, aloc);
          // emit call
          AddStatement(OPC_Call, mt, 1/*SelfOffset*/, aloc);
        }
      }
    }
    // clear fields
    if (loc.Type.Struct->NeedsFieldsDestruction()) {
      EmitLocalAddress(loc.Offset, aloc);
      AddStatement(OPC_ClearPointedStruct, loc.Type.Struct, aloc);
    }
    return;
  }

  if (loc.Type.Type == TYPE_Array) {
    if (loc.Type.ArrayInnerType == TYPE_String) {
      for (int j = 0; j < loc.Type.GetArrayDim(); ++j) {
        EmitLocalAddress(loc.Offset, aloc);
        EmitPushNumber(j, aloc);
        AddStatement(OPC_ArrayElement, loc.Type.GetArrayInnerType(), aloc);
        AddStatement(OPC_ClearPointedStr, aloc);
      }
    } else if (loc.Type.ArrayInnerType == TYPE_Struct) {
      // call struct dtors
      if (loc.Type.Struct->NeedsMethodDestruction()) {
        for (VStruct *st = loc.Type.Struct; st; st = st->ParentStruct) {
          VMethod *mt = st->FindDtor(false); // non-recursive
          if (mt) {
            vassert(mt->NumParams == 0);
            for (int j = 0; j < loc.Type.GetArrayDim(); ++j) {
              // emit `self`
              if (!mt->IsStatic()) {
                EmitLocalAddress(loc.Offset, aloc);
                EmitPushNumber(j, aloc);
                AddStatement(OPC_ArrayElement, loc.Type.GetArrayInnerType(), aloc);
              }
              // emit call
              AddStatement(OPC_Call, mt, 1/*SelfOffset*/, aloc);
            }
          }
        }
      }
      // clear
      if (loc.Type.Struct->NeedsFieldsDestruction()) {
        for (int j = 0; j < loc.Type.GetArrayDim(); ++j) {
          EmitLocalAddress(loc.Offset, aloc);
          EmitPushNumber(j, aloc);
          AddStatement(OPC_ArrayElement, loc.Type.GetArrayInnerType(), aloc);
          AddStatement(OPC_ClearPointedStruct, loc.Type.Struct, aloc);
        }
      }
    }
    return;
  }
}


//==========================================================================
//
//  VEmitContext::SetIndexArray
//
//==========================================================================
VArrayElement *VEmitContext::SetIndexArray (VArrayElement *el) {
  auto res = IndArray;
  IndArray = el;
  return res;
}


//==========================================================================
//
//  VEmitContext::SetIndexArray
//
//==========================================================================
void VEmitContext::EmitGotoTo (VName lblname, const TLocation &aloc) {
  if (lblname == NAME_None) VCFatalError("VC: internal compiler error (VEmitContext::EmitGotoTo)");
  for (int f = GotoLabels.length()-1; f >= 0; --f) {
    if (GotoLabels[f].name == lblname) {
      AddStatement(OPC_Goto, GotoLabels[f].jlbl, aloc);
      return;
    }
  }
  // add new goto label
  VGotoListItem it;
  it.jlbl = DefineLabel();
  it.name = lblname;
  it.loc = aloc;
  it.defined = false;
  GotoLabels.append(it);
  AddStatement(OPC_Goto, it.jlbl, aloc);
}


//==========================================================================
//
//  VEmitContext::SetIndexArray
//
//==========================================================================
void VEmitContext::EmitGotoLabel (VName lblname, const TLocation &aloc) {
  if (lblname == NAME_None) VCFatalError("VC: internal compiler error (VEmitContext::EmitGotoTo)");
  for (int f = GotoLabels.length()-1; f >= 0; --f) {
    if (GotoLabels[f].name == lblname) {
      if (GotoLabels[f].defined) {
        ParseError(aloc, "Duplicate label `%s` (previous is at %s:%d)", *lblname, *GotoLabels[f].loc.GetSourceFile(), GotoLabels[f].loc.GetLine());
      } else {
        MarkLabel(GotoLabels[f].jlbl);
      }
      return;
    }
  }
  // add new goto label
  VGotoListItem it;
  it.jlbl = DefineLabel();
  it.name = lblname;
  it.loc = aloc;
  it.defined = true;
  MarkLabel(it.jlbl);
  GotoLabels.append(it);
}
