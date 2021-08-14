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
//** directrly included from "vc_executor.cpp"
//**************************************************************************
#ifdef VC_VM_LABEL_TABLE

#define DECLARE_OPC_DYNARRDISPATCH(name) &&Lbl_OPC_DynArrDispatch_ ## name
static void *vm_labels_dynarrdisp[] = {
#include "vc_progdefs_dynarrdispatch.h"
0 };
#undef DECLARE_OPC_DYNARRDISPATCH

#else

#define PR_VMBN_SWITCH(op)  goto *vm_labels_dynarrdisp[op];
#define PR_VMBN_CASE(x)     Lbl_ ## x:
#if USE_COMPUTED_GOTO
# define PR_VMBN_BREAK  PR_VM_BREAK
#else
# define PR_VMBN_BREAK  goto vm_labels_dynarrdisp_done
#endif

origip = ip++;
VFieldType Type = VFieldType::ReadTypeMem(ip);
PR_VMBN_SWITCH(*ip++) {
  // [-2]: *dynarray
  // [-1]: length
  PR_VMBN_CASE(OPC_DynArrDispatch_DynArraySetNum)
    {
      VScriptArray &A = *(VScriptArray *)sp[-2].p;
      int newsize = sp[-1].i;
      // allow clearing for 2d arrays
      if (A.Is2D() && newsize != 0) { cstDump(origip); VPackage::InternalFatalError("Cannot resize 2D array"); }
      if (newsize < 0) { cstDump(origip); VPackage::InternalFatalError(va("Array index %d is negative", newsize)); }
      if (newsize > MaxDynArrayLength) { cstDump(origip); VPackage::InternalFatalError(va("Array index %d is too big", newsize)); }
      A.SetNum(newsize, Type);
      sp -= 2;
    }
    PR_VMBN_BREAK;
  // [-2]: *dynarray
  // [-1]: delta
  PR_VMBN_CASE(OPC_DynArrDispatch_DynArraySetNumMinus)
    {
      VScriptArray &A = *(VScriptArray *)sp[-2].p;
      int newsize = sp[-1].i;
      // allow clearing for 2d arrays
      if (A.Is2D() && newsize != 0 && newsize != A.length()) { cstDump(origip); VPackage::InternalFatalError("Cannot resize 2D array"); }
      if (newsize < 0) { cstDump(origip); VPackage::InternalFatalError(va("Array shrink delta %d is negative", newsize)); }
      if (newsize > MaxDynArrayLength) { cstDump(origip); VPackage::InternalFatalError(va("Array shrink delta %d is too big", newsize)); }
      if (A.length() < newsize) { cstDump(origip); VPackage::InternalFatalError(va("Array shrink delta %d is too big (%d)", newsize, A.length())); }
      if (newsize > 0) A.SetNumMinus(newsize, Type);
      sp -= 2;
    }
    PR_VMBN_BREAK;
  // [-2]: *dynarray
  // [-1]: delta
  PR_VMBN_CASE(OPC_DynArrDispatch_DynArraySetNumPlus)
    {
      VScriptArray &A = *(VScriptArray *)sp[-2].p;
      int newsize = sp[-1].i;
      // allow clearing for 2d arrays
      if (A.Is2D() && newsize != 0 && newsize != A.length()) { cstDump(origip); VPackage::InternalFatalError("Cannot resize 2D array"); }
      if (newsize < 0) { cstDump(origip); VPackage::InternalFatalError(va("Array grow delta %d is negative", newsize)); }
      if (newsize > MaxDynArrayLength) { cstDump(origip); VPackage::InternalFatalError(va("Array grow delta %d is too big", newsize)); }
      if (A.length() > MaxDynArrayLength || MaxDynArrayLength-A.length() < newsize) { cstDump(origip); VPackage::InternalFatalError(va("Array grow delta %d is too big (%d)", newsize, A.length())); }
      if (newsize > 0) A.SetNumPlus(newsize, Type);
      sp -= 2;
    }
    PR_VMBN_BREAK;
  // [-3]: *dynarray
  // [-2]: index
  // [-1]: count
  PR_VMBN_CASE(OPC_DynArrDispatch_DynArrayInsert)
    {
      VScriptArray &A = *(VScriptArray *)sp[-3].p;
      if (A.Is2D()) { cstDump(origip); VPackage::InternalFatalError("Cannot insert into 2D array"); }
      int index = sp[-2].i;
      int count = sp[-1].i;
      if (count < 0) { cstDump(origip); VPackage::InternalFatalError(va("Array count %d is negative", count)); }
      if (index < 0) { cstDump(origip); VPackage::InternalFatalError(va("Array index %d is negative", index)); }
      if (index > A.length()) { cstDump(origip); VPackage::InternalFatalError(va("Index %d outside the bounds of an array (%d)", index, A.length())); }
      if (A.length() > MaxDynArrayLength || MaxDynArrayLength-A.length() < count) { cstDump(origip); VPackage::InternalFatalError("Out of memory for dynarray"); }
      if (count > 0) A.Insert(index, count, Type);
      sp -= 3;
    }
    PR_VMBN_BREAK;
  // [-3]: *dynarray
  // [-2]: index
  // [-1]: count
  PR_VMBN_CASE(OPC_DynArrDispatch_DynArrayRemove)
    {
      VScriptArray &A = *(VScriptArray *)sp[-3].p;
      if (A.Is2D()) { cstDump(origip); VPackage::InternalFatalError("Cannot insert into 2D array"); }
      int index = sp[-2].i;
      int count = sp[-1].i;
      if (count < 0) { cstDump(origip); VPackage::InternalFatalError(va("Array count %d is negative", count)); }
      if (index < 0) { cstDump(origip); VPackage::InternalFatalError(va("Array index %d is negative", index)); }
      if (index > A.length()) { cstDump(origip); VPackage::InternalFatalError(va("Index %d outside the bounds of an array (%d)", index, A.length())); }
      if (count > A.length()-index) { cstDump(origip); VPackage::InternalFatalError(va("Array count %d is too big at %d (%d)", count, index, A.length())); }
      if (count > 0) A.Remove(index, count, Type);
      sp -= 3;
    }
    PR_VMBN_BREAK;
  // [-1]: *dynarray
  PR_VMBN_CASE(OPC_DynArrDispatch_DynArrayClear)
    {
      VScriptArray &A = *(VScriptArray *)sp[-1].p;
      A.Reset(Type);
      --sp;
    }
    PR_VMBN_BREAK;
  // [-1]: delegate
  // [-2]: self
  // [-3]: *dynarray
  // in code: type
  PR_VMBN_CASE(OPC_DynArrDispatch_DynArraySort)
    //fprintf(stderr, "sp=%p\n", sp);
    {
      VScriptArray &A = *(VScriptArray *)sp[-3].p;
      if (A.Is2D()) { cstDump(origip); VPackage::InternalFatalError("Cannot sort non-flat arrays"); }
      // get self
      VObject *dgself = (VObject *)sp[-2].p;
      // get pointer to the delegate
      VMethod *dgfunc = (VMethod *)sp[-1].p;
      if (!dgself) {
        if (!dgfunc || (dgfunc->Flags&FUNC_Static) == 0) { cstDump(origip); VPackage::InternalFatalError("Delegate is not initialised"); }
      }
      if (!dgfunc) { cstDump(origip); VPackage::InternalFatalError("Delegate is not initialised (empty method)"); }
      // fix stack, so we can call a delegate properly
      VObject::pr_stackPtr = sp;
      cstFixTopIPSP(ip);
      if (!A.Sort(Type, dgself, dgfunc)) { cstDump(origip); VPackage::InternalFatalError("Internal error in array sorter"); }
    }
    //current_func = func;
    sp = VObject::pr_stackPtr;
    //fprintf(stderr, "sp=%p\n", sp);
    sp -= 3;
    PR_VMBN_BREAK;
  // [-1]: idx1
  // [-2]: idx0
  // [-3]: *dynarray
  PR_VMBN_CASE(OPC_DynArrDispatch_DynArraySwap1D)
    {
      VScriptArray &A = *(VScriptArray *)sp[-3].p;
      if (A.Is2D()) { cstDump(origip); VPackage::InternalFatalError("Cannot swap items of non-flat arrays"); }
      int idx0 = sp[-2].i;
      int idx1 = sp[-1].i;
      if (idx0 < 0 || idx0 >= A.length()) { cstDump(origip); VPackage::InternalFatalError(va("Index %d outside the bounds of an array (%d)", idx0, A.length())); }
      if (idx1 < 0 || idx1 >= A.length()) { cstDump(origip); VPackage::InternalFatalError(va("Index %d outside the bounds of an array (%d)", idx1, A.length())); }
      A.SwapElements(idx0, idx1, Type);
      sp -= 3;
    }
    PR_VMBN_BREAK;
  // [-2]: *dynarray
  // [-1]: size
  PR_VMBN_CASE(OPC_DynArrDispatch_DynArraySetSize1D)
    {
      VScriptArray &A = *(VScriptArray *)sp[-2].p;
      int newsize = sp[-1].i;
      if (newsize < 0) { cstDump(origip); VPackage::InternalFatalError(va("Array size %d is negative", newsize)); }
      if (newsize > MaxDynArrayLength) { cstDump(origip); VPackage::InternalFatalError(va("Array size %d is too big", newsize)); }
      A.SetNum(newsize, Type); // this will flatten it
      sp -= 2;
    }
    PR_VMBN_BREAK;
  // [-3]: *dynarray
  // [-2]: size1
  // [-1]: size2
  PR_VMBN_CASE(OPC_DynArrDispatch_DynArraySetSize2D)
    {
      VScriptArray &A = *(VScriptArray *)sp[-3].p;
      int newsize1 = sp[-2].i;
      int newsize2 = sp[-1].i;
      if (newsize1 < 1) { cstDump(origip); VPackage::InternalFatalError(va("Array size %d is too small", newsize1)); }
      if (newsize1 > MaxDynArrayLength) { cstDump(origip); VPackage::InternalFatalError(va("Array size %d is too big", newsize1)); }
      if (newsize2 < 1) { cstDump(origip); VPackage::InternalFatalError(va("Array size %d is too small", newsize2)); }
      if (newsize2 > MaxDynArrayLength) { cstDump(origip); VPackage::InternalFatalError(va("Array size %d is too big", newsize2)); }
      A.SetSize2D(newsize1, newsize2, Type);
      sp -= 3;
    }
    PR_VMBN_BREAK;
  // [-1]: *dynarray
  PR_VMBN_CASE(OPC_DynArrDispatch_DynArrayAlloc)
    {
      VScriptArray &A = *(VScriptArray *)sp[-1].p;
      sp[-1].p = A.Alloc(Type);
    }
    PR_VMBN_BREAK;
  // [-1]: *dynarray
  PR_VMBN_CASE(OPC_DynArrDispatch_DynArrayToBool)
    {
      VScriptArray &A = *(VScriptArray *)sp[-1].p;
      sp[-1].i = !!(A.length());
    }
    PR_VMBN_BREAK;
  /*
  PR_VMBN_DEFAULT:
    cstDump(origip);
    VPackage::InternalFatalError(va("Dictionary opcode %d is not implemented", ip[-1]));
  */
}

#if USE_COMPUTED_GOTO
#else
vm_labels_dynarrdisp_done: (void)0;
#endif

#undef PR_VMBN_SWITCH
#undef PR_VMBN_CASE
#undef PR_VMBN_BREAK

#endif
