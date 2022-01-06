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
//**  Copyright (C) 2018-2022 Ketmar Dark
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
//** directrly included from "vc_executor.cpp"
//**************************************************************************
#ifdef VC_VM_LABEL_TABLE

#define DECLARE_OPC_DICTDISPATCH(name) &&Lbl_OPC_DictDispatch_ ## name
static void *vm_labels_dictdisp[] = {
#include "vc_progdefs_dictdispatch.h"
0 };
#undef DECLARE_OPC_DICTDISPATCH

#else

#define PR_VMBN_SWITCH(op)  goto *vm_labels_dictdisp[op];
#define PR_VMBN_CASE(x)     Lbl_ ## x:
#if USE_COMPUTED_GOTO
# define PR_VMBN_BREAK  PR_VM_BREAK
#else
# define PR_VMBN_BREAK  goto vm_labels_dictdisp_done
#endif

VScriptDict *ht;
VScriptDictElem e, v, *r;
PR_VMBN_SWITCH(dcopcode) {
  // clear
  // [-1]: VScriptDict
  PR_VMBN_CASE(OPC_DictDispatch_Clear)
    ht = (VScriptDict *)sp[-1].p;
    if (!ht) { cstDump(origip); VPackage::InternalFatalError("uninitialized dictionary"); }
    ht->clear();
    --sp;
    PR_VMBN_BREAK;
  // reset
  // [-1]: VScriptDict
  PR_VMBN_CASE(OPC_DictDispatch_Reset)
    ht = (VScriptDict *)sp[-1].p;
    if (!ht) { cstDump(origip); VPackage::InternalFatalError("uninitialized dictionary"); }
    ht->reset();
    --sp;
    PR_VMBN_BREAK;
  // length
  // [-1]: VScriptDict
  PR_VMBN_CASE(OPC_DictDispatch_Length)
    ht = (VScriptDict *)sp[-1].p;
    if (!ht) { cstDump(origip); VPackage::InternalFatalError("uninitialized dictionary"); }
    sp[-1].i = ht->length();
    PR_VMBN_BREAK;
  // capacity
  // [-1]: VScriptDict
  PR_VMBN_CASE(OPC_DictDispatch_Capacity)
    ht = (VScriptDict *)sp[-1].p;
    if (!ht) { cstDump(origip); VPackage::InternalFatalError("uninitialized dictionary"); }
    sp[-1].i = ht->capacity();
    PR_VMBN_BREAK;
  // find
  // [-2]: VScriptDict
  // [-1]: keyptr
  PR_VMBN_CASE(OPC_DictDispatch_Find)
    ht = (VScriptDict *)sp[-2].p;
    if (!ht) { cstDump(origip); VPackage::InternalFatalError("uninitialized dictionary"); }
    VScriptDictElem::CreateFromPtr(e, sp[-1].p, KType, true); // calc hash
    r = ht->find(e);
    if (r) {
      if (VType.Type == TYPE_String || VScriptDictElem::isSimpleType(VType)) {
        sp[-2].p = &r->value;
      } else {
        sp[-2].p = r->value;
      }
    } else {
      sp[-2].p = nullptr;
    }
    --sp;
    PR_VMBN_BREAK;
  // put
  // [-3]: VScriptDict
  // [-2]: keyptr
  // [-1]: valptr
  PR_VMBN_CASE(OPC_DictDispatch_Put)
    // strings are increfed by loading opcode, so it is ok
    ht = (VScriptDict *)sp[-3].p;
    if (!ht) { cstDump(origip); VPackage::InternalFatalError("uninitialized dictionary"); }
    VScriptDictElem::CreateFromPtr(e, sp[-2].p, KType, true); // calc hash
    VScriptDictElem::CreateFromPtr(v, sp[-1].p, VType, false); // no hash
    sp[-3].i = (ht->put(e, v) ? 1 : 0);
    sp -= 2;
    PR_VMBN_BREAK;
  // delete
  // [-2]: VScriptDict
  // [-1]: keyptr
  PR_VMBN_CASE(OPC_DictDispatch_Delete)
    ht = (VScriptDict *)sp[-2].p;
    if (!ht) { cstDump(origip); VPackage::InternalFatalError("uninitialized dictionary"); }
    VScriptDictElem::CreateFromPtr(e, sp[-1].p, KType, true); // calc hash
    sp[-2].i = (ht->del(e) ? 1 : 0);
    --sp;
    PR_VMBN_BREAK;
  // clear by ptr
  // [-1]: VScriptDict*
  PR_VMBN_CASE(OPC_DictDispatch_ClearPointed)
    ht = (VScriptDict *)sp[-1].p;
    if (!ht) { cstDump(origip); VPackage::InternalFatalError("uninitialized dictionary"); }
    ht->clear();
    --sp;
    PR_VMBN_BREAK;
  // first index
  // [-1]: VScriptDict*
  PR_VMBN_CASE(OPC_DictDispatch_FirstIndex)
    ht = (VScriptDict *)sp[-1].p;
    if (!ht) { cstDump(origip); VPackage::InternalFatalError("uninitialized dictionary"); }
    sp[-1].i = (ht->map ? ht->map->getFirstIIdx() : -1);
    PR_VMBN_BREAK;
  // is valid index?
  // [-2]: VScriptDict*
  // [-1]: index
  PR_VMBN_CASE(OPC_DictDispatch_IsValidIndex)
    ht = (VScriptDict *)sp[-2].p;
    if (!ht) { cstDump(origip); VPackage::InternalFatalError("uninitialized dictionary"); }
    sp[-2].i = ((ht->map ? ht->map->isValidIIdx(sp[-1].i) : false) ? 1 : 0);
    --sp;
    PR_VMBN_BREAK;
  // next index
  // [-2]: VScriptDict*
  // [-1]: index
  PR_VMBN_CASE(OPC_DictDispatch_NextIndex)
    ht = (VScriptDict *)sp[-2].p;
    if (!ht) { cstDump(origip); VPackage::InternalFatalError("uninitialized dictionary"); }
    sp[-2].i = (ht->map ? ht->map->getNextIIdx(sp[-1].i) : -1);
    --sp;
    PR_VMBN_BREAK;
  // delete current and next index
  // [-2]: VScriptDict*
  // [-1]: index
  PR_VMBN_CASE(OPC_DictDispatch_DelAndNextIndex)
    ht = (VScriptDict *)sp[-2].p;
    if (!ht) { cstDump(origip); VPackage::InternalFatalError("uninitialized dictionary"); }
    sp[-2].i = (ht->map ? ht->map->removeCurrAndGetNextIIdx(sp[-1].i) : -1);
    --sp;
    PR_VMBN_BREAK;
  // key at index
  // [-2]: VScriptDict
  // [-1]: index
  PR_VMBN_CASE(OPC_DictDispatch_GetKeyAtIndex)
    ht = (VScriptDict *)sp[-2].p;
    if (!ht) { cstDump(origip); VPackage::InternalFatalError("uninitialized dictionary"); }
    {
      const VScriptDictElem *ep = (ht->map ? ht->map->getKeyIIdx(sp[-1].i) : nullptr);
      if (ep) {
        if (KType.Type == TYPE_String) {
          sp[-2].p = nullptr;
          *((VStr *)&sp[-2].p) = *((VStr *)&ep->value);
        } else {
          sp[-2].p = ep->value;
        }
      } else {
        sp[-2].p = nullptr;
      }
    }
    --sp;
    PR_VMBN_BREAK;
  // value at index
  // [-2]: VScriptDict
  // [-1]: index
  PR_VMBN_CASE(OPC_DictDispatch_GetValueAtIndex)
    ht = (VScriptDict *)sp[-2].p;
    if (!ht) { cstDump(origip); VPackage::InternalFatalError("uninitialized dictionary"); }
    {
      VScriptDictElem *ep = (ht->map ? ht->map->getValueIIdx(sp[-1].i) : nullptr);
      if (ep) {
        if (VType.Type == TYPE_String || VScriptDictElem::isSimpleType(VType)) {
          sp[-2].p = &ep->value;
        } else {
          sp[-2].p = ep->value;
        }
      } else {
        sp[-2].p = nullptr;
      }
    }
    --sp;
    PR_VMBN_BREAK;
  // compact
  // [-1]: VScriptDict
  PR_VMBN_CASE(OPC_DictDispatch_Compact)
    ht = (VScriptDict *)sp[-1].p;
    if (!ht) { cstDump(origip); VPackage::InternalFatalError("uninitialized dictionary"); }
    if (ht->map) ht->map->compact();
    --sp;
    PR_VMBN_BREAK;
  // rehash
  // [-1]: VScriptDict
  PR_VMBN_CASE(OPC_DictDispatch_Rehash)
    ht = (VScriptDict *)sp[-1].p;
    if (!ht) { cstDump(origip); VPackage::InternalFatalError("uninitialized dictionary"); }
    if (ht->map) ht->map->rehash();
    --sp;
    PR_VMBN_BREAK;
  // [-1]: VScriptDict
  PR_VMBN_CASE(OPC_DictDispatch_DictToBool)
    ht = (VScriptDict *)sp[-1].p;
    sp[-1].i = !!(ht && ht->map && ht->map->length());
    PR_VMBN_BREAK;
}
/*
cstDump(origip);
VPackage::InternalFatalError(va("Dictionary opcode %d is not implemented", dcopcode));
*/

#if USE_COMPUTED_GOTO
#else
vm_labels_dictdisp_done: (void)0;
#endif

#undef PR_VMBN_SWITCH
#undef PR_VMBN_CASE
#undef PR_VMBN_BREAK

#endif
