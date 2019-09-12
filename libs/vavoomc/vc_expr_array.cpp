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

#define DICTDISPATCH_OPCODE_INFO
#include "vc_progdefs.h"

#define DYNARRDISPATCH_OPCODE_INFO
#include "vc_progdefs.h"


//==========================================================================
//
//  VArrayElement::VArrayElement
//
//==========================================================================
VArrayElement::VArrayElement (VExpression *AOp, VExpression *AInd, const TLocation &ALoc, bool aSkipBounds)
  : VExpression(ALoc)
  , opscopy(nullptr)
  , genStringAssign(false)
  , sval(nullptr)
  , resolvingInd2(false)
  , op(AOp)
  , ind(AInd)
  , ind2(nullptr)
  , AddressRequested(false)
  , IsAssign(false)
  , skipBoundsChecking(aSkipBounds)
{
  if (!ind) {
    ParseError(Loc, "Expression expected");
    return;
  }
}


//==========================================================================
//
//  VArrayElement::VArrayElement
//
//==========================================================================
VArrayElement::VArrayElement (VExpression *AOp, VExpression *AInd, VExpression *AInd2, const TLocation &ALoc, bool aSkipBounds)
  : VExpression(ALoc)
  , opscopy(nullptr)
  , genStringAssign(false)
  , sval(nullptr)
  , op(AOp)
  , ind(AInd)
  , ind2(AInd2)
  , AddressRequested(false)
  , IsAssign(false)
  , skipBoundsChecking(aSkipBounds)
{
  if (!ind) {
    ParseError(Loc, "Expression expected");
    return;
  }
}


//==========================================================================
//
//  VArrayElement::~VArrayElement
//
//==========================================================================
VArrayElement::~VArrayElement () {
  opscopy.clear();
  if (op) { delete op; op = nullptr; }
  if (ind) { delete ind; ind = nullptr; }
  if (ind2) { delete ind2; ind2 = nullptr; }
  if (sval) { delete sval; sval = nullptr; }
}


//==========================================================================
//
//  VArrayElement::SyntaxCopy
//
//==========================================================================
VExpression *VArrayElement::SyntaxCopy () {
  auto res = new VArrayElement();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VArrayElement::DoSyntaxCopyTo
//
//==========================================================================
void VArrayElement::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VArrayElement *)e;
  res->genStringAssign = genStringAssign;
  res->sval = (sval ? sval->SyntaxCopy() : nullptr);
  res->resolvingInd2 = resolvingInd2;
  res->op = (op ? op->SyntaxCopy() : nullptr);
  res->ind = (ind ? ind->SyntaxCopy() : nullptr);
  res->ind2 = (ind2 ? ind2->SyntaxCopy() : nullptr);
  res->AddressRequested = AddressRequested;
  res->IsAssign = IsAssign;
  res->skipBoundsChecking = skipBoundsChecking;
}


//==========================================================================
//
//  VArrayElement::InternalResolve
//
//==========================================================================
VExpression *VArrayElement::InternalResolve (VEmitContext &ec, bool assTarget) {
  if (!op || !ind) {
    delete this;
    return nullptr;
  }

  // we need a copy for opDollar
  //VExpression *opcopy = op->SyntaxCopy();
  opscopy.replaceSyntaxCopy(op);

  op = op->Resolve(ec);

  bool wasInd2 = (ind2 != nullptr);

  // note that 1d access to 2d array is ok
  if (wasInd2 && op) {
    bool wasErr = true;
    if (op->Type.Type == TYPE_Array) {
      if (!op->Type.IsArray2D()) {
        ParseError(Loc, "2d access to 1d array");
      } else {
        wasErr = false;
      }
    } else if (op->Type.Type == TYPE_DynamicArray) {
      wasErr = false;
    } else {
      ParseError(Loc, "Only arrays can be 2d yet");
    }
    if (wasErr) {
      opscopy.clear();
      delete this;
      return nullptr;
    }
  }

  VExpression *indcopy = (ind ? ind->SyntaxCopy() : nullptr);
  VExpression *ind2copy = (ind2 ? ind2->SyntaxCopy() : nullptr);

  if (op) {
    // resolve index expression
    auto oldIndArray = ec.SetIndexArray(this);
    resolvingInd2 = false;
    if (ind) ind = ind->Resolve(ec);
    resolvingInd2 = true;
    if (ind2) ind2 = ind2->Resolve(ec);
    ec.SetIndexArray(oldIndArray);
  }

  if (!op || !ind || (wasInd2 && !ind2)) {
    delete indcopy;
    delete ind2copy;
    delete this;
    return nullptr;
  }

  // convert `class[idx]` to `opIndex` funcall
  if (op->Type.Type == TYPE_Reference || op->Type.Type == TYPE_Class) {
    if (ind2) {
      ParseError(Loc, "No `opIndex` support for 2d array access yet");
      delete indcopy;
      delete ind2copy;
      ParseError(Loc, "Cannot find `opIndex`");
      delete this;
      return nullptr;
    }
    if (assTarget) VCFatalError("VC: internal compiler error (VArrayElement::InternalResolve)");
    VName mtname = VName("opIndex");
    // try typed method first: opIndex<index>
    VStr itname = "opIndex";
    switch (ind->Type.Type) {
      case TYPE_Int: case TYPE_Byte: case TYPE_Bool: itname += "Int"; break;
      case TYPE_Float: itname += "Float"; break;
      case TYPE_Name: itname += "Name"; break;
      case TYPE_String: itname += "String"; break;
      case TYPE_Reference: itname += "Object"; break;
      case TYPE_Class: itname += "Class"; break;
      case TYPE_State: itname += "State"; break;
    }
    if (op->Type.Class->FindAccessibleMethod(*itname, ec.SelfClass)) mtname = VName(*itname);
    if (!op->Type.Class->FindAccessibleMethod(mtname, ec.SelfClass)) mtname = NAME_None;
    if (mtname == NAME_None) {
      delete indcopy;
      delete ind2copy;
      ParseError(Loc, "Cannot find `opIndex`");
      delete this;
      return nullptr;
    }
    VExpression *e = new VDotInvocation(opscopy.extract(), mtname, Loc, 1, &indcopy);
    delete ind2copy;
    delete this;
    return e->Resolve(ec);
  }

  // don't need those anymore
  //delete opcopy;
  opscopy.clear();
  delete indcopy;
  delete ind2copy;

  if (ind->Type.Type != TYPE_Int || (ind2 && ind2->Type.Type != TYPE_Int)) {
    ParseError(Loc, "Array index must be of integer type");
    delete this;
    return nullptr;
  }

  if (op->Type.IsAnyIndexableArray()) {
    // check bounds for static arrays
    if (!skipBoundsChecking && ind->IsIntConst() && (!ind2 || ind2->IsIntConst())) {
      if (ind->GetIntConst() < 0 || (ind2 && ind2->GetIntConst() < 0)) {
        ParseError(Loc, "Negative array index");
        delete this;
        return nullptr;
      }
      if (op->Type.Type == TYPE_Array) {
        int xdim = (ind2 ? op->Type.GetFirstDim() : op->Type.GetArrayDim());
        if (ind->GetIntConst() >= xdim) {
          ParseError(Loc, "Array index %d out of bounds (%d)", ind->GetIntConst(), xdim);
          delete this;
          return nullptr;
        }
        if (ind2 && ind2->GetIntConst() >= op->Type.GetSecondDim()) {
          ParseError(Loc, "Secondary array index %d out of bounds (%d)", ind2->GetIntConst(), op->Type.GetSecondDim());
          delete this;
          return nullptr;
        }
        skipBoundsChecking = true;
        // convert 2d access to 1d access (it is slightly faster this way)
        if (ind2) {
          int x = ind->GetIntConst();
          int y = ind2->GetIntConst();
          VExpression *ne = new VIntLiteral(y*op->Type.GetFirstDim()+x, ind->Loc);
          delete ind2;
          delete ind;
          ind2 = nullptr;
          ind = ne->Resolve(ec); // just in case
        }
      }
    }
    // if second index is 0, convert 2d access to 1d access (it is slightly faster this way)
    if (ind2 && ind2->IsIntConst() && ind2->GetIntConst() == 0) {
      delete ind2;
      ind2 = nullptr;
    }
    Flags = op->Flags;
    Type = op->Type.GetArrayInnerType();
    if (assTarget && (Flags&FIELD_ReadOnly) != 0) {
      ParseError(Loc, "Cannot assign to read-only array");
      delete this;
      return nullptr;
    }
    //!!!if (!assTarget) op->Flags &= ~FIELD_ReadOnly;
    op->RequestAddressOf();
  } else if (op->Type.Type == TYPE_String) {
    // check bounds
    /*k8: it is tolerant to this
    if (ind->IsIntConst() && ind->GetIntConst() < 0) {
      ParseError(Loc, "Negative string index");
      delete this;
      return nullptr;
    }
    */
    if (assTarget) {
      ParseError(Loc, "Strings are immutable (yet)");
      delete this;
      return nullptr;
    } else {
      RealType = op->Type;
      Type = VFieldType(TYPE_Int);
    }
  } else if (op->Type.Type == TYPE_Pointer) {
    Flags = 0;
    Type = op->Type.GetPointerInnerType();
  } else {
    ParseError(Loc, "Bad operation with array");
    delete this;
    return nullptr;
  }

  // check for unsafe pointer access
  if (op->Type.Type == TYPE_Pointer) {
    if (ind->IsIntConst() && ind->GetIntConst() == 0) {
      // the only safe case, do nothing
    } else {
      if (!VMemberBase::unsafeCodeAllowed) {
        ParseError(Loc, "Unsafe pointer access is forbidden");
        delete this;
        return nullptr;
      } else if (VMemberBase::unsafeCodeWarning) {
        ParseWarning(Loc, "Unsafe pointer access");
      }
    }
  }

  RealType = Type;
  if (Type.Type == TYPE_Byte || Type.Type == TYPE_Bool) Type = VFieldType(TYPE_Int);
  return this;
}


//==========================================================================
//
//  VArrayElement::DoResolve
//
//==========================================================================
VExpression *VArrayElement::DoResolve (VEmitContext &ec) {
  return InternalResolve(ec, false);
}


//==========================================================================
//
//  VArrayElement::ResolveAssignmentTarget
//
//==========================================================================
VExpression *VArrayElement::ResolveAssignmentTarget (VEmitContext &ec) {
  IsAssign = true;
  return InternalResolve(ec, true);
}


//==========================================================================
//
//  VArrayElement::ResolveCompleteAssign
//
//  this will be called before actual assign resolving
//  return `nullptr` to indicate error, or consume `val` and set `resolved` to `true` if resolved
//  if `nullptr` is returned, both `this` and `val` should be destroyed
//
//==========================================================================
VExpression *VArrayElement::ResolveCompleteAssign (VEmitContext &ec, VExpression *val, bool &resolved) {
  if (ind2) return this;

  if (op && (op->Flags&FIELD_ReadOnly) != 0) {
    ParseError(Loc, "Cannot assign to read-only destination");
    delete val;
    delete this;
    return nullptr;
  }

  VExpression *rop = op->SyntaxCopy()->Resolve(ec);
  if (!rop) { delete val; delete this; return nullptr; }

  // convert `class[idx]` to `opIndex` funcall
  if (rop->Type.Type == TYPE_Reference || rop->Type.Type == TYPE_Class) {
    resolved = true; // anyway
    VExpression *rval = val->SyntaxCopy()->Resolve(ec);
    VExpression *rind = ind->SyntaxCopy()->Resolve(ec);
    if (!rval || !rind) {
      delete rval;
      delete rind;
      delete rop;
      delete val;
      delete this;
      return nullptr;
    }
    VExpression *argv[2];
    argv[0] = ind;
    argv[1] = val;
    VName mtname = NAME_None;
    // try typed methods first: opIndex<index>Assign<index>
    VStr itname;
    switch (rind->Type.Type) {
      case TYPE_Int: case TYPE_Byte: case TYPE_Bool: itname += "Int"; break;
      case TYPE_Float: itname += "Float"; break;
      case TYPE_Name: itname += "Name"; break;
      case TYPE_String: itname += "String"; break;
      case TYPE_Reference: itname += "Object"; break;
      case TYPE_Class: itname += "Class"; break;
      case TYPE_State: itname += "State"; break;
    }
    VStr vtname;
    switch (rval->Type.Type) {
      case TYPE_Int: case TYPE_Byte: case TYPE_Bool: vtname += "Int"; break;
      case TYPE_Float: vtname += "Float"; break;
      case TYPE_Name: vtname += "Name"; break;
      case TYPE_String: vtname += "String"; break;
      case TYPE_Reference: vtname += "Object"; break;
      case TYPE_Class: vtname += "Class"; break;
      case TYPE_State: vtname += "State"; break;
    }
    //fprintf(stderr, "rind=<%s>; rval=<%s>; itname=<%s>; vtname=<%s>\n", *rind->Type.GetName(), *rval->Type.GetName(), *itname, *vtname);
    if (!itname.isEmpty() && !vtname.isEmpty()) {
      VStr n = VStr("opIndex")+itname+"Assign"+vtname;
      if (rop->Type.Class->FindAccessibleMethod(*n, ec.SelfClass)) mtname = VName(*n);
    }
    // try shorter
    if (mtname == NAME_None && !itname.isEmpty()) {
      VStr n = VStr("opIndex")+itname+"Assign";
      if (rop->Type.Class->FindAccessibleMethod(*n, ec.SelfClass)) mtname = VName(*n);
    }
    if (mtname == NAME_None && !vtname.isEmpty()) {
      VStr n = VStr("opIndexAssign")+vtname;
      if (rop->Type.Class->FindAccessibleMethod(*n, ec.SelfClass)) mtname = VName(*n);
    }
    if (mtname == NAME_None) {
      mtname = VName("opIndexAssign");
      if (!rop->Type.Class->FindAccessibleMethod(mtname, ec.SelfClass)) mtname = NAME_None;
    }
    // don't need those anymore
    delete rval;
    delete rind;
    delete rop;
    if (mtname == NAME_None) {
      ParseError(Loc, "Cannot find `opIndexAssign`");
      delete val;
      delete this;
      return nullptr;
    }
    // create invocation
    VExpression *e = new VDotInvocation(op, mtname, Loc, 2, argv);
    op = nullptr; // it was consumed by invocation
    ind = nullptr; // it was consumed by invocation
    delete this;
    return e->Resolve(ec);
  }

  if (rop->Type.Type != TYPE_String) {
    delete rop;
    return this; // go on with the normal assignment
  }

  resolved = true; // anyway

  // we need a copy for opDollar
  //VExpression *opcopy = op;
  opscopy.replaceNoCopy(op);
  op = rop;

  if (op) {
    // resolve index expression
    auto oldIndArray = ec.SetIndexArray(this);
    ind = ind->Resolve(ec);
    ec.SetIndexArray(oldIndArray);
  }
  sval = (val ? val->Resolve(ec) : nullptr);

  // we don't need this anymore
  //delete opcopy;
  opscopy.clear();

  if (!op || !ind || !sval) {
    delete this;
    return nullptr;
  }

  if (op->Type.Type != TYPE_String) {
    ParseError(Loc, "Something is *very* wrong with the compiler");
    delete this;
    return nullptr;
  }

  if (ind->Type.Type != TYPE_Int) {
    ParseError(Loc, "String index must be of integer type");
    delete this;
    return nullptr;
  }

  if (sval->Type.Type == TYPE_String && sval->IsStrConst() && sval->GetStrConst(ec.Package).length() == 1) {
    VStr s = sval->GetStrConst(ec.Package);
    val = new VIntLiteral((vuint8)s[0], sval->Loc);
    delete sval;
    sval = val->Resolve(ec); // will never fail
  } else if (sval->Type.Type == TYPE_Name && sval->IsNameConst() && VStr::length(*sval->GetNameConst()) == 1) {
    const char *s = *sval->GetNameConst();
    val = new VIntLiteral((vuint8)s[0], sval->Loc);
    delete sval;
    sval = val->Resolve(ec); // will never fail
  }

  if (sval->Type.Type != TYPE_Int && sval->Type.Type != TYPE_Byte) {
    ParseError(Loc, "Cannot assign type '%s' to string element", *sval->Type.GetName());
    delete this;
    return nullptr;
  }

  op->RequestAddressOf();

  genStringAssign = true;
  Type = VFieldType(TYPE_Void);
  return this;
}


//==========================================================================
//
//  VArrayElement::RequestAddressOf
//
//==========================================================================
void VArrayElement::RequestAddressOf () {
  if (op->Type.Type == TYPE_String) ParseError(Loc, "Cannot get string element address");
  if (AddressRequested) ParseError(Loc, "Multiple address of");
  AddressRequested = true;
}


//==========================================================================
//
//  VArrayElement::Emit
//
//==========================================================================
void VArrayElement::Emit (VEmitContext &ec) {
  if (op) op->Emit(ec);
  if (ind) ind->Emit(ec);
  if (ind2) ind2->Emit(ec);
  if (genStringAssign) {
    sval->Emit(ec);
    ec.AddStatement(OPC_StrSetChar, Loc);
  } else {
    if (op->Type.Type == TYPE_DynamicArray) {
      if (ind2) {
        ec.AddStatement(OPC_DynArrayElement2D, RealType, Loc);
      } else {
        ec.AddStatement((IsAssign ? OPC_DynArrayElementGrow : OPC_DynArrayElement), RealType, Loc);
      }
    } else if (op->Type.Type == TYPE_String) {
      if (IsAssign) {
        ParseError(Loc, "Strings are immutable (yet) -- codegen");
      } else {
        ec.AddStatement(OPC_StrGetChar, Loc);
        return;
      }
    } else if (op->Type.Type == TYPE_SliceArray) {
      ec.AddStatement(OPC_SliceElement, RealType, Loc);
    } else {
      // `Resolve()` will set this flag if it did static check
      if (!skipBoundsChecking) {
        if (ind2) {
          ec.AddStatement(OPC_CheckArrayBounds2D, op->Type, Loc);
        } else {
          ec.AddStatement(OPC_CheckArrayBounds, op->Type.GetArrayDim(), Loc);
        }
      }
      if (ind2) {
        ec.AddStatement(OPC_ArrayElement2D, op->Type, Loc);
      } else {
        ec.AddStatement(OPC_ArrayElement, RealType, Loc);
      }
    }
    if (!AddressRequested) EmitPushPointedCode(RealType, ec);
  }
}


//==========================================================================
//
//  VArrayElement::GetOpSyntaxCopy
//
//==========================================================================
VExpression *VArrayElement::GetOpSyntaxCopy () {
  return opscopy.SyntaxCopy();
}



//==========================================================================
//
//  VSliceOp::VSliceOp
//
//==========================================================================
VSliceOp::VSliceOp (VExpression *aop, VExpression *alo, VExpression *ahi, const TLocation &aloc)
  : VArrayElement(aop, alo, aloc)
  , hi(ahi)
{
  Flags = FIELD_ReadOnly;
}


//==========================================================================
//
//  VSliceOp::~VSliceOp
//
//==========================================================================
VSliceOp::~VSliceOp () {
  delete hi;
}


//==========================================================================
//
//  VSliceOp::SyntaxCopy
//
//==========================================================================
VExpression *VSliceOp::SyntaxCopy () {
  auto res = new VSliceOp();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VSliceOp::DoSyntaxCopyTo
//
//==========================================================================
void VSliceOp::DoSyntaxCopyTo (VExpression *e) {
  VArrayElement::DoSyntaxCopyTo(e);
  auto res = (VSliceOp *)e;
  res->hi = (hi ? hi->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VSliceOp::DoResolve
//
//==========================================================================
VExpression *VSliceOp::DoResolve (VEmitContext &ec) {
  if (!op || !ind || !hi) {
    delete this;
    return nullptr;
  }

  // we need a copy for opDollar
  //VExpression *opcopy = op->SyntaxCopy();
  opscopy.replaceSyntaxCopy(op);

  op = op->Resolve(ec);
  if (op) {
    // resolve index expressions
    auto oldIndArray = ec.SetIndexArray(this);
    ind = ind->Resolve(ec);
    hi = hi->Resolve(ec);
    ec.SetIndexArray(oldIndArray);
  }

  // we don't need this anymore
  //delete opcopy;
  opscopy.clear();

  if (!op || !ind || !hi) {
    delete this;
    return nullptr;
  }

  if (op->Type.Type != TYPE_String && op->Type.Type != TYPE_SliceArray) {
    ParseError(Loc, "You can slice only string or another slice");
    delete this;
    return nullptr;
  }

  if (ind->Type.Type != TYPE_Int || hi->Type.Type != TYPE_Int) {
    ParseError(Loc, "Slice indicies must be of integer type");
    delete this;
    return nullptr;
  }

  if (op->Type.Type == TYPE_SliceArray) op->RequestAddressOf();

  Type = op->Type;
  return this;
}


//==========================================================================
//
//  VSliceOp::ResolveAssignmentTarget
//
//==========================================================================
VExpression *VSliceOp::ResolveAssignmentTarget (VEmitContext &ec) {
  // actually, this is handled in `ResolveCompleteAssign()`
  ParseError(Loc, "Cannot assign to slices");
  return nullptr;
}


//==========================================================================
//
//  VArrayElement::ResolveCompleteAssign
//
//==========================================================================
VExpression *VSliceOp::ResolveCompleteAssign (VEmitContext &ec, VExpression *val, bool &resolved) {
  resolved = true; // anyway

  if (op && (op->Flags&FIELD_ReadOnly) != 0) {
    ParseError(Loc, "Cannot assign to read-only destination");
    delete val;
    delete this;
    return nullptr;
  }

  VExpression *rop = op->SyntaxCopy()->Resolve(ec);
  if (!rop) { delete val; delete this; return nullptr; }

  if (rop->Type.Type != TYPE_String) {
    ParseError(Loc, "Only string slices are allowed in assign operation");
    delete rop;
    delete val;
    delete this;
    return nullptr;
  }

  // we need a copy for opDollar
  //VExpression *opcopy = op;
  opscopy.replaceNoCopy(op);
  op = rop;

  {
    // resolve index expression
    auto oldIndArray = ec.SetIndexArray(this);
    ind = ind->Resolve(ec);
    hi = hi->Resolve(ec);
    ec.SetIndexArray(oldIndArray);
  }
  sval = (val ? val->Resolve(ec) : nullptr);

  // we don't need this anymore
  //delete opcopy;
  opscopy.clear();

  if (!op || !ind || !hi || !sval) {
    delete this;
    return nullptr;
  }

  if (ind->Type.Type != TYPE_Int || hi->Type.Type != TYPE_Int) {
    ParseError(Loc, "String slice index must be of integer type");
    delete this;
    return nullptr;
  }

  if (sval->Type.Type != TYPE_String) {
    ParseError(Loc, "String slice new value must be of string type");
    delete this;
    return nullptr;
  }

  op->RequestAddressOf();

  genStringAssign = true;
  Type = VFieldType(TYPE_Void);
  return this;
}


//==========================================================================
//
//  VSliceOp::RequestAddressOf
//
//==========================================================================
void VSliceOp::RequestAddressOf () {
  ParseError(Loc, "Cannot get string slice address");
}


//==========================================================================
//
//  VSliceOp::Emit
//
//==========================================================================
void VSliceOp::Emit (VEmitContext &ec) {
  if (!op || !ind || !hi) return;
  op->Emit(ec);
  ind->Emit(ec);
  hi->Emit(ec);
  if (genStringAssign) {
    sval->Emit(ec);
    ec.AddStatement(OPC_StrSliceAssign, Loc);
  } else if (op->Type.Type == TYPE_String) {
    ec.AddStatement(OPC_StrSlice, Loc);
  } else if (op->Type.Type == TYPE_SliceArray) {
    ec.AddStatement(OPC_SliceSlice, op->Type.GetArrayInnerType(), Loc);
  } else {
    VCFatalError("VC: the thing that should not be (VSliceOp::Emit)");
  }
}


//==========================================================================
//
//  VDynArrayGetNum::VDynArrayGetNum
//
//==========================================================================
VDynArrayGetNum::VDynArrayGetNum (VExpression *AArrayExpr, int aDimNumber, const TLocation &ALoc)
  : VExpression(ALoc)
  , ArrayExpr(AArrayExpr)
  , dimNumber(aDimNumber)
{
}


//==========================================================================
//
//  VDynArrayGetNum::~VDynArrayGetNum
//
//==========================================================================
VDynArrayGetNum::~VDynArrayGetNum () {
  if (ArrayExpr) { delete ArrayExpr; ArrayExpr = nullptr; }
}


//==========================================================================
//
//  VDynArrayGetNum::SyntaxCopy
//
//==========================================================================
VExpression *VDynArrayGetNum::SyntaxCopy () {
  auto res = new VDynArrayGetNum();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDynArrayGetNum::DoRestSyntaxCopyTo
//
//==========================================================================
void VDynArrayGetNum::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDynArrayGetNum *)e;
  res->ArrayExpr = (ArrayExpr ? ArrayExpr->SyntaxCopy() : nullptr);
  res->dimNumber = dimNumber;
}


//==========================================================================
//
//  VDynArrayGetNum::DoResolve
//
//==========================================================================
VExpression *VDynArrayGetNum::DoResolve (VEmitContext &) {
  Type = VFieldType(TYPE_Int);
  return this;
}


//==========================================================================
//
//  VDynArrayGetNum::Emit
//
//==========================================================================
void VDynArrayGetNum::Emit (VEmitContext &ec) {
  if (ArrayExpr) ArrayExpr->Emit(ec);
  switch (dimNumber) {
    case 0: ec.AddStatement(OPC_DynArrayGetNum, Loc); break;
    case 1: ec.AddStatement(OPC_DynArrayGetNum1, Loc); break;
    case 2: ec.AddStatement(OPC_DynArrayGetNum2, Loc); break;
    default: VCFatalError("VC: internal error in (VDynArrayGetNum::Emit)");
  }
}


//==========================================================================
//
//  VDynArraySetNum::VDynArraySetNum
//
//==========================================================================
VDynArraySetNum::VDynArraySetNum (VExpression *AArrayExpr, VExpression *ANumExpr, VExpression *ANumExpr2, const TLocation &ALoc)
  : VExpression(ALoc)
  , ArrayExpr(AArrayExpr)
  , NumExpr(ANumExpr)
  , NumExpr2(ANumExpr2)
  , opsign(0)
  , asSetSize(!!ANumExpr2)
{
  Type = VFieldType(TYPE_Void);
}


//==========================================================================
//
//  VDynArraySetNum::~VDynArraySetNum
//
//==========================================================================
VDynArraySetNum::~VDynArraySetNum () {
  if (ArrayExpr) { delete ArrayExpr; ArrayExpr = nullptr; }
  if (NumExpr2) { delete NumExpr2; NumExpr2 = nullptr; }
  if (NumExpr) { delete NumExpr; NumExpr = nullptr; }
}


//==========================================================================
//
//  VDynArraySetNum::SyntaxCopy
//
//==========================================================================
VExpression *VDynArraySetNum::SyntaxCopy () {
  auto res = new VDynArraySetNum();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDynArraySetNum::DoRestSyntaxCopyTo
//
//==========================================================================
void VDynArraySetNum::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDynArraySetNum *)e;
  res->ArrayExpr = (ArrayExpr ? ArrayExpr->SyntaxCopy() : nullptr);
  res->NumExpr = (NumExpr ? NumExpr->SyntaxCopy() : nullptr);
  res->NumExpr2 = (NumExpr2 ? NumExpr2->SyntaxCopy() : nullptr);
  res->opsign = opsign;
  res->asSetSize = asSetSize;
}


//==========================================================================
//
//  VDynArraySetNum::DoResolve
//
//==========================================================================
VExpression *VDynArraySetNum::DoResolve (VEmitContext &ec) {
  if (asSetSize) {
    // in this case indicies are not resolved yet
    if (opsign != 0) {
      ParseError(Loc, "You cannot use '+=' or '-=' for `SetSize`");
      delete this;
      return nullptr;
    }
    if (opsign != 0) {
      ParseError(Loc, "You cannot use '+=' or '-=' for `SetSize`");
      delete this;
      return nullptr;
    }
    if (NumExpr) NumExpr = NumExpr->Resolve(ec);
    if (NumExpr2) {
      NumExpr2 = NumExpr2->Resolve(ec);
      if (!NumExpr2) { delete this; return nullptr; }
    }
    if (!NumExpr) { delete this; return nullptr; }
    NumExpr->Type.CheckMatch(false, NumExpr->Loc, VFieldType(TYPE_Int));
    if (NumExpr2) NumExpr2->Type.CheckMatch(false, NumExpr2->Loc, VFieldType(TYPE_Int));
  }
  return this;
}


//==========================================================================
//
//  VDynArraySetNum::Emit
//
//==========================================================================
void VDynArraySetNum::Emit (VEmitContext &ec) {
  if (ArrayExpr) ArrayExpr->Emit(ec);
  if (NumExpr) NumExpr->Emit(ec);
  if (NumExpr2) NumExpr2->Emit(ec);
  if (opsign == 0) {
    // normal assign
    if (asSetSize) {
      //ec.AddStatement((NumExpr2 ? OPC_DynArraySetSize2D : OPC_DynArraySetSize1D), ArrayExpr->Type.GetArrayInnerType(), Loc);
      ec.AddStatement(OPC_DynArrayDispatch, ArrayExpr->Type.GetArrayInnerType(), (NumExpr2 ? OPC_DynArrDispatch_DynArraySetSize2D : OPC_DynArrDispatch_DynArraySetSize1D), Loc);
    } else {
      //ec.AddStatement(OPC_DynArraySetNum, ArrayExpr->Type.GetArrayInnerType(), Loc);
      ec.AddStatement(OPC_DynArrayDispatch, ArrayExpr->Type.GetArrayInnerType(), OPC_DynArrDispatch_DynArraySetNum, Loc);
    }
  } else if (opsign < 0) {
    // -=
    //ec.AddStatement(OPC_DynArraySetNumMinus, ArrayExpr->Type.GetArrayInnerType(), Loc);
    ec.AddStatement(OPC_DynArrayDispatch, ArrayExpr->Type.GetArrayInnerType(), OPC_DynArrDispatch_DynArraySetNumMinus, Loc);
  } else {
    // +=
    //ec.AddStatement(OPC_DynArraySetNumPlus, ArrayExpr->Type.GetArrayInnerType(), Loc);
    ec.AddStatement(OPC_DynArrayDispatch, ArrayExpr->Type.GetArrayInnerType(), OPC_DynArrDispatch_DynArraySetNumPlus, Loc);
  }
}


//==========================================================================
//
//  VDynArraySetNum::IsDynArraySetNum
//
//==========================================================================
bool VDynArraySetNum::IsDynArraySetNum () const {
  return true;
}


//==========================================================================
//
//  VDynArrayInsert::VDynArrayInsert
//
//==========================================================================
VDynArrayInsert::VDynArrayInsert (VExpression *AArrayExpr, VExpression *AIndexExpr, VExpression *ACountExpr, const TLocation &ALoc)
  : VExpression(ALoc)
  , ArrayExpr(AArrayExpr)
  , IndexExpr(AIndexExpr)
  , CountExpr(ACountExpr)
{
}


//==========================================================================
//
//  VDynArrayInsert::~VDynArrayInsert
//
//==========================================================================
VDynArrayInsert::~VDynArrayInsert () {
  if (ArrayExpr) { delete ArrayExpr; ArrayExpr = nullptr; }
  if (IndexExpr) { delete IndexExpr; IndexExpr = nullptr; }
  if (CountExpr) { delete CountExpr; CountExpr = nullptr; }
}


//==========================================================================
//
//  VDynArrayInsert::SyntaxCopy
//
//==========================================================================
VExpression *VDynArrayInsert::SyntaxCopy () {
  auto res = new VDynArrayInsert();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDynArrayInsert::DoRestSyntaxCopyTo
//
//==========================================================================
void VDynArrayInsert::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDynArrayInsert *)e;
  res->ArrayExpr = (ArrayExpr ? ArrayExpr->SyntaxCopy() : nullptr);
  res->IndexExpr = (IndexExpr ? IndexExpr->SyntaxCopy() : nullptr);
  res->CountExpr = (CountExpr ? CountExpr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDynArrayInsert::DoResolve
//
//==========================================================================
VExpression *VDynArrayInsert::DoResolve (VEmitContext &ec) {
  if ((Flags&FIELD_ReadOnly) != 0 || (ArrayExpr && (ArrayExpr->Flags&FIELD_ReadOnly) != 0)) {
    ParseError(Loc, "Cannot change read-only array");
    delete this;
    return nullptr;
  }

  if (ArrayExpr) ArrayExpr->RequestAddressOf();
  // resolve arguments
  if (IndexExpr) IndexExpr = IndexExpr->Resolve(ec);
  if (CountExpr) CountExpr = CountExpr->Resolve(ec);
  if (!IndexExpr || !CountExpr) {
    delete this;
    return nullptr;
  }

  // check argument types
  if (IndexExpr->Type.Type != TYPE_Int) {
    ParseError(Loc, "Index must be integer expression");
    delete this;
    return nullptr;
  }

  if (CountExpr->Type.Type != TYPE_Int) {
    ParseError(Loc, "Count must be integer expression");
    delete this;
    return nullptr;
  }

  Type = VFieldType(TYPE_Void);
  return this;
}


//==========================================================================
//
//  VDynArrayInsert::Emit
//
//==========================================================================
void VDynArrayInsert::Emit (VEmitContext &ec) {
  ArrayExpr->Emit(ec);
  IndexExpr->Emit(ec);
  CountExpr->Emit(ec);
  //ec.AddStatement(OPC_DynArrayInsert, ArrayExpr->Type.GetArrayInnerType(), Loc);
  ec.AddStatement(OPC_DynArrayDispatch, ArrayExpr->Type.GetArrayInnerType(), OPC_DynArrDispatch_DynArrayInsert, Loc);
}



//==========================================================================
//
//  VDynArrayRemove::VDynArrayRemove
//
//==========================================================================
VDynArrayRemove::VDynArrayRemove (VExpression *AArrayExpr, VExpression *AIndexExpr, VExpression *ACountExpr, const TLocation &ALoc)
  : VExpression(ALoc)
  , ArrayExpr(AArrayExpr)
  , IndexExpr(AIndexExpr)
  , CountExpr(ACountExpr)
{
}


//==========================================================================
//
//  VDynArrayRemove::~VDynArrayRemove
//
//==========================================================================
VDynArrayRemove::~VDynArrayRemove () {
  if (ArrayExpr) { delete ArrayExpr; ArrayExpr = nullptr; }
  if (IndexExpr) { delete IndexExpr; IndexExpr = nullptr; }
  if (CountExpr) { delete CountExpr; CountExpr = nullptr; }
}


//==========================================================================
//
//  VDynArrayRemove::SyntaxCopy
//
//==========================================================================
VExpression *VDynArrayRemove::SyntaxCopy () {
  auto res = new VDynArrayRemove();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDynArrayRemove::DoRestSyntaxCopyTo
//
//==========================================================================
void VDynArrayRemove::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDynArrayRemove *)e;
  res->ArrayExpr = (ArrayExpr ? ArrayExpr->SyntaxCopy() : nullptr);
  res->IndexExpr = (IndexExpr ? IndexExpr->SyntaxCopy() : nullptr);
  res->CountExpr = (CountExpr ? CountExpr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDynArrayRemove::DoResolve
//
//==========================================================================
VExpression *VDynArrayRemove::DoResolve (VEmitContext &ec) {
  if ((Flags&FIELD_ReadOnly) != 0 || (ArrayExpr && (ArrayExpr->Flags&FIELD_ReadOnly) != 0)) {
    ParseError(Loc, "Cannot change read-only array");
    delete this;
    return nullptr;
  }

  if (ArrayExpr) ArrayExpr->RequestAddressOf();
  // resolve arguments
  if (IndexExpr) IndexExpr = IndexExpr->Resolve(ec);
  if (CountExpr) CountExpr = CountExpr->Resolve(ec);
  if (!IndexExpr || !CountExpr) {
    delete this;
    return nullptr;
  }

  // check argument types
  if (IndexExpr->Type.Type != TYPE_Int) {
    ParseError(Loc, "Index must be integer expression");
    delete this;
    return nullptr;
  }

  if (CountExpr->Type.Type != TYPE_Int) {
    ParseError(Loc, "Count must be integer expression");
    delete this;
    return nullptr;
  }

  Type = VFieldType(TYPE_Void);
  return this;
}


//==========================================================================
//
//  VDynArrayRemove::Emit
//
//==========================================================================
void VDynArrayRemove::Emit (VEmitContext &ec) {
  ArrayExpr->Emit(ec);
  IndexExpr->Emit(ec);
  CountExpr->Emit(ec);
  //ec.AddStatement(OPC_DynArrayRemove, ArrayExpr->Type.GetArrayInnerType(), Loc);
  ec.AddStatement(OPC_DynArrayDispatch, ArrayExpr->Type.GetArrayInnerType(), OPC_DynArrDispatch_DynArrayRemove, Loc);
}



//==========================================================================
//
//  VDynArrayClear::VDynArrayClear
//
//==========================================================================
VDynArrayClear::VDynArrayClear (VExpression *AArrayExpr, const TLocation &ALoc)
  : VExpression(ALoc)
  , ArrayExpr(AArrayExpr)
{
}


//==========================================================================
//
//  VDynArrayClear::~VDynArrayClear
//
//==========================================================================
VDynArrayClear::~VDynArrayClear () {
  if (ArrayExpr) { delete ArrayExpr; ArrayExpr = nullptr; }
}


//==========================================================================
//
//  VDynArrayClear::SyntaxCopy
//
//==========================================================================
VExpression *VDynArrayClear::SyntaxCopy () {
  auto res = new VDynArrayClear();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDynArrayClear::DoRestSyntaxCopyTo
//
//==========================================================================
void VDynArrayClear::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDynArrayClear *)e;
  res->ArrayExpr = (ArrayExpr ? ArrayExpr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDynArrayClear::DoResolve
//
//==========================================================================
VExpression *VDynArrayClear::DoResolve (VEmitContext &ec) {
  if ((Flags&FIELD_ReadOnly) != 0 || (ArrayExpr && (ArrayExpr->Flags&FIELD_ReadOnly) != 0)) {
    ParseError(Loc, "Cannot change read-only array");
    delete this;
    return nullptr;
  }
  if (ArrayExpr) ArrayExpr->RequestAddressOf();
  Type = VFieldType(TYPE_Void);
  return this;
}


//==========================================================================
//
//  VDynArrayClear::Emit
//
//==========================================================================
void VDynArrayClear::Emit (VEmitContext &ec) {
  ArrayExpr->Emit(ec);
  //ec.AddStatement(OPC_DynArrayClear, ArrayExpr->Type.GetArrayInnerType(), Loc);
  ec.AddStatement(OPC_DynArrayDispatch, ArrayExpr->Type.GetArrayInnerType(), OPC_DynArrDispatch_DynArrayClear, Loc);
}



//==========================================================================
//
//  VDynArraySort::VDynArraySort
//
//==========================================================================
VDynArraySort::VDynArraySort (VExpression *AArrayExpr, VExpression *ADgExpr, const TLocation &ALoc)
  : VExpression(ALoc)
  , ArrayExpr(AArrayExpr)
  , DgExpr(ADgExpr)
{
}


//==========================================================================
//
//  VDynArraySort::~VDynArraySort
//
//==========================================================================
VDynArraySort::~VDynArraySort () {
  if (ArrayExpr) { delete ArrayExpr; ArrayExpr = nullptr; }
  if (DgExpr) { delete DgExpr; DgExpr = nullptr; }
}


//==========================================================================
//
//  VDynArraySort::SyntaxCopy
//
//==========================================================================
VExpression *VDynArraySort::SyntaxCopy () {
  auto res = new VDynArraySort();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDynArraySort::DoRestSyntaxCopyTo
//
//==========================================================================
void VDynArraySort::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDynArraySort *)e;
  res->ArrayExpr = (ArrayExpr ? ArrayExpr->SyntaxCopy() : nullptr);
  res->DgExpr = (DgExpr ? DgExpr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDynArraySort::checkDelegateType
//
//==========================================================================
bool VDynArraySort::checkDelegateType (VMethod *dg) {
  if (!dg) { ParseError(Loc, "VDynArraySort::DoResolve: internal compiler error"); return false; }

  if (dg->NumParams != 2) { ParseError(Loc, "`.sort` delegate must have two parameters"); return false; }

  if (dg->ReturnType.Type != TYPE_Int) {
    ParseError(Loc, "`.sort` delegate must return `int`");
    return false;
  }

  VFieldType Type = ArrayExpr->Type.GetArrayInnerType();

  if (!dg->ParamTypes[0].Equals(Type) || !dg->ParamTypes[1].Equals(Type)) {
    ParseError(Loc, "`.sort` delegate parameters must be of the same type `%s`", *Type.GetName());
    return false;
  }

  // check if type should be passed as ref
  bool requireRef = false;
  if (Type.PtrLevel == 0) {
    switch (Type.Type) {
      case TYPE_Struct:
      case TYPE_Vector: //FIXME
      case TYPE_DynamicArray:
      case TYPE_Dictionary:
        requireRef = true;
        break;
      case TYPE_Delegate:
      case TYPE_Array:
      case TYPE_SliceArray: //FIXME
        return false;
      default:
        break;
    }
  }

  if (requireRef) {
    if ((dg->ParamFlags[0]&(FPARM_Out|FPARM_Ref)) == 0 || (dg->ParamFlags[1]&(FPARM_Out|FPARM_Ref)) == 0) {
      ParseError(Loc, "`.sort` array type `%s` must be passed by ref", *Type.GetName());
      return false;
    }
  }

  // no optional args allowed
  if ((dg->ParamFlags[0]|dg->ParamFlags[1])&FPARM_Optional) {
    ParseError(Loc, "`.sort` delegate parameters must not be optional");
    return false;
  }

  // if we have no self, this should be a static method
  //if (!self && (dg->Flags&FUNC_Static) == 0) return false;

  // check other flags
  if (dg->Flags&(FUNC_VarArgs|FUNC_Iterator)) {
    ParseError(Loc, "`.sort` delegate should not be iterator or vararg method");
    return false;
  }

  // ok, it looks that our delegate is valid
  return true;
}


//==========================================================================
//
//  VDynArraySort::DoResolve
//
//==========================================================================
VExpression *VDynArraySort::DoResolve (VEmitContext &ec) {
  if ((Flags&FIELD_ReadOnly) != 0 || (ArrayExpr && (ArrayExpr->Flags&FIELD_ReadOnly) != 0)) {
    ParseError(Loc, "Cannot change read-only array");
    delete this;
    return nullptr;
  }

  if (ArrayExpr) ArrayExpr->RequestAddressOf();
  // resolve arguments
  if (DgExpr) DgExpr = DgExpr->Resolve(ec);
  if (!DgExpr) {
    delete this;
    return nullptr;
  }

  // must be a delegate
  if (DgExpr->Type.Type != TYPE_Delegate) {
    ParseError(Loc, "`.sort` expects delegate argument");
    delete this;
    return nullptr;
  }

  VMethod *dg = DgExpr->Type.Function;
  if (!checkDelegateType(dg)) {
    delete this;
    return nullptr;
  }

  Type = VFieldType(TYPE_Void);
  return this;
}


//==========================================================================
//
//  VDynArraySort::Emit
//
//==========================================================================
void VDynArraySort::Emit (VEmitContext &ec) {
  ArrayExpr->Emit(ec);
  DgExpr->Emit(ec);
  //ec.AddStatement(OPC_DynArraySort, ArrayExpr->Type.GetArrayInnerType(), Loc);
  ec.AddStatement(OPC_DynArrayDispatch, ArrayExpr->Type.GetArrayInnerType(), OPC_DynArrDispatch_DynArraySort, Loc);
}



//==========================================================================
//
//  VDynArraySwap1D::VDynArraySwap1D
//
//==========================================================================
VDynArraySwap1D::VDynArraySwap1D (VExpression *AArrayExpr, VExpression *AIndex0Expr, VExpression *AIndex1Expr, const TLocation &ALoc)
  : VExpression(ALoc)
  , ArrayExpr(AArrayExpr)
  , Index0Expr(AIndex0Expr)
  , Index1Expr(AIndex1Expr)
{
}


//==========================================================================
//
//  VDynArraySwap1D::~VDynArraySwap1D
//
//==========================================================================
VDynArraySwap1D::~VDynArraySwap1D () {
  if (ArrayExpr) { delete ArrayExpr; ArrayExpr = nullptr; }
  if (Index0Expr) { delete Index0Expr; Index0Expr = nullptr; }
  if (Index1Expr) { delete Index1Expr; Index1Expr = nullptr; }
}


//==========================================================================
//
//  VDynArraySwap1D::SyntaxCopy
//
//==========================================================================
VExpression *VDynArraySwap1D::SyntaxCopy () {
  auto res = new VDynArraySwap1D();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDynArraySwap1D::DoRestSyntaxCopyTo
//
//==========================================================================
void VDynArraySwap1D::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDynArraySwap1D *)e;
  res->ArrayExpr = (ArrayExpr ? ArrayExpr->SyntaxCopy() : nullptr);
  res->Index0Expr = (Index0Expr ? Index0Expr->SyntaxCopy() : nullptr);
  res->Index1Expr = (Index1Expr ? Index1Expr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDynArraySwap1D::DoResolve
//
//==========================================================================
VExpression *VDynArraySwap1D::DoResolve (VEmitContext &ec) {
  if ((Flags&FIELD_ReadOnly) != 0 || (ArrayExpr && (ArrayExpr->Flags&FIELD_ReadOnly) != 0)) {
    ParseError(Loc, "Cannot change read-only array");
    delete this;
    return nullptr;
  }

  if (ArrayExpr) ArrayExpr->RequestAddressOf();
  // resolve arguments
  if (Index0Expr) Index0Expr = Index0Expr->Resolve(ec);
  if (Index1Expr) Index1Expr = Index1Expr->Resolve(ec);
  if (!Index0Expr || !Index1Expr) {
    delete this;
    return nullptr;
  }

  // check argument types
  if (Index0Expr->Type.Type != TYPE_Int) {
    ParseError(Loc, "Index0 must be integer expression");
    delete this;
    return nullptr;
  }

  if (Index1Expr->Type.Type != TYPE_Int) {
    ParseError(Loc, "Index1 must be integer expression");
    delete this;
    return nullptr;
  }

  Type = VFieldType(TYPE_Void);
  return this;
}


//==========================================================================
//
//  VDynArraySwap1D::Emit
//
//==========================================================================
void VDynArraySwap1D::Emit (VEmitContext &ec) {
  ArrayExpr->Emit(ec);
  Index0Expr->Emit(ec);
  Index1Expr->Emit(ec);
  //ec.AddStatement(OPC_DynArraySwap1D, ArrayExpr->Type.GetArrayInnerType(), Loc);
  ec.AddStatement(OPC_DynArrayDispatch, ArrayExpr->Type.GetArrayInnerType(), OPC_DynArrDispatch_DynArraySwap1D, Loc);
}



//==========================================================================
//
//  VDynArrayCopyFrom::VDynArrayCopyFrom
//
//==========================================================================
VDynArrayCopyFrom::VDynArrayCopyFrom (VExpression *aarr, VExpression *asrc, const TLocation &aloc)
  : VExpression(aloc)
  , ArrayExpr(aarr)
  , SrcExpr(asrc)
{
}


//==========================================================================
//
//  VDynArrayCopyFrom::~VDynArrayCopyFrom
//
//==========================================================================
VDynArrayCopyFrom::~VDynArrayCopyFrom () {
  if (ArrayExpr) { delete ArrayExpr; ArrayExpr = nullptr; }
  if (SrcExpr) { delete SrcExpr; SrcExpr = nullptr; }
}


//==========================================================================
//
//  VDynArrayCopyFrom::SyntaxCopy
//
//==========================================================================
VExpression *VDynArrayCopyFrom::SyntaxCopy () {
  auto res = new VDynArrayCopyFrom();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDynArrayCopyFrom::DoRestSyntaxCopyTo
//
//==========================================================================
void VDynArrayCopyFrom::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDynArrayCopyFrom *)e;
  res->ArrayExpr = (ArrayExpr ? ArrayExpr->SyntaxCopy() : nullptr);
  res->SrcExpr = (SrcExpr ? SrcExpr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDynArrayCopyFrom::DoResolve
//
//==========================================================================
VExpression *VDynArrayCopyFrom::DoResolve (VEmitContext &ec) {
  if ((Flags&FIELD_ReadOnly) != 0 || (ArrayExpr && (ArrayExpr->Flags&FIELD_ReadOnly) != 0)) {
    ParseError(Loc, "Cannot change read-only array");
    delete this;
    return nullptr;
  }

  // resolve arguments
  if (SrcExpr) SrcExpr = SrcExpr->Resolve(ec);
  if (!ArrayExpr || !SrcExpr) { delete this; return nullptr; }

  // check argument types
  if (!ArrayExpr->Type.Equals(SrcExpr->Type)) {
    ParseError(Loc, "arrays are not compatible (`%s` and `%s`)", *ArrayExpr->Type.GetName(), *SrcExpr->Type.GetName());
    delete this;
    return nullptr;
  }

  ArrayExpr->RequestAddressOf();
  SrcExpr->RequestAddressOf();

  Type = VFieldType(TYPE_Void);
  return this;
}


//==========================================================================
//
//  VDynArrayCopyFrom::Emit
//
//==========================================================================
void VDynArrayCopyFrom::Emit (VEmitContext &ec) {
  ArrayExpr->Emit(ec);
  SrcExpr->Emit(ec);
  ec.AddStatement(OPC_TypeDeepCopy, ArrayExpr->Type, Loc);
}



//==========================================================================
//
//  VDynArrayAllocElement::VDynArrayAllocElement
//
//==========================================================================
VDynArrayAllocElement::VDynArrayAllocElement (VExpression *aarr, const TLocation &aloc)
  : VExpression(aloc)
  , ArrayExpr(aarr)
{
}


//==========================================================================
//
//  VDynArrayAllocElement::~VDynArrayAllocElement
//
//==========================================================================
VDynArrayAllocElement::~VDynArrayAllocElement () {
  if (ArrayExpr) { delete ArrayExpr; ArrayExpr = nullptr; }
}


//==========================================================================
//
//  VDynArrayAllocElement::SyntaxCopy
//
//==========================================================================
VExpression *VDynArrayAllocElement::SyntaxCopy () {
  auto res = new VDynArrayAllocElement();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDynArrayAllocElement::DoRestSyntaxCopyTo
//
//==========================================================================
void VDynArrayAllocElement::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDynArrayAllocElement *)e;
  res->ArrayExpr = (ArrayExpr ? ArrayExpr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDynArrayAllocElement::DoResolve
//
//==========================================================================
VExpression *VDynArrayAllocElement::DoResolve (VEmitContext &ec) {
  if ((Flags&FIELD_ReadOnly) != 0 || (ArrayExpr && (ArrayExpr->Flags&FIELD_ReadOnly) != 0)) {
    ParseError(Loc, "Cannot change read-only array");
    delete this;
    return nullptr;
  }
  if (!ArrayExpr) { delete this; return nullptr; }
  ArrayExpr->RequestAddressOf();
  Type = ArrayExpr->Type.GetArrayInnerType().MakePointerType();
  return this;
}


//==========================================================================
//
//  VDynArrayAllocElement::Emit
//
//==========================================================================
void VDynArrayAllocElement::Emit (VEmitContext &ec) {
  ArrayExpr->Emit(ec);
  ec.AddStatement(OPC_DynArrayDispatch, ArrayExpr->Type.GetArrayInnerType(), OPC_DynArrDispatch_DynArrayAlloc, Loc);
}



//==========================================================================
//
//  VStringGetLength::VStringGetLength
//
//==========================================================================
VStringGetLength::VStringGetLength(VExpression *AStrExpr, const TLocation &ALoc)
  : VExpression(ALoc)
  , StrExpr(AStrExpr)
{
}


//==========================================================================
//
//  VStringGetLength::~VStringGetLength
//
//==========================================================================
VStringGetLength::~VStringGetLength () {
  if (StrExpr) {
    delete StrExpr;
    StrExpr = nullptr;
  }
}


//==========================================================================
//
//  VStringGetLength::SyntaxCopy
//
//==========================================================================
VExpression *VStringGetLength::SyntaxCopy () {
  auto res = new VStringGetLength();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VStringGetLength::DoRestSyntaxCopyTo
//
//==========================================================================
void VStringGetLength::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VStringGetLength *)e;
  res->StrExpr = (StrExpr ? StrExpr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VStringGetLength::DoResolve
//
//==========================================================================
VExpression *VStringGetLength::DoResolve (VEmitContext &ec) {
  // optimize it for string literals
  if (StrExpr->IsStrConst()) {
    VStr val = StrExpr->GetStrConst(ec.Package);
    VExpression *e = new VIntLiteral(val.length(), Loc);
    e = e->Resolve(ec);
    delete this;
    return e;
  }
  Type = VFieldType(TYPE_Int);
  return this;
}


//==========================================================================
//
//  VStringGetLength::Emit
//
//==========================================================================
void VStringGetLength::Emit (VEmitContext &ec) {
  StrExpr->Emit(ec);
  ec.AddStatement(OPC_StrLength, Loc);
}



//==========================================================================
//
//  VSliceGetLength::VSliceGetLength
//
//==========================================================================
VSliceGetLength::VSliceGetLength (VExpression *asexpr, const TLocation &aloc)
  : VExpression(aloc)
  , sexpr(asexpr)
{
}


//==========================================================================
//
//  VSliceGetLength::~VSliceGetLength
//
//==========================================================================
VSliceGetLength::~VSliceGetLength () {
  delete sexpr;
}


//==========================================================================
//
//  VSliceGetLength::SyntaxCopy
//
//==========================================================================
VExpression *VSliceGetLength::SyntaxCopy () {
  auto res = new VSliceGetLength();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VSliceGetLength::DoSyntaxCopyTo
//
//==========================================================================
void VSliceGetLength::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VSliceGetLength *)e;
  res->sexpr = (sexpr ? sexpr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VSliceGetLength::DoResolve
//
//==========================================================================
VExpression *VSliceGetLength::DoResolve (VEmitContext &ec) {
  Type = VFieldType(TYPE_Int);
  return this;
}


//==========================================================================
//
//  VSliceGetLength::Emit
//
//==========================================================================
void VSliceGetLength::Emit (VEmitContext &ec) {
  sexpr->Emit(ec);
  /*
  ec.AddStatement(OPC_OffsetPtr, (int)sizeof(void *), Loc);
  ec.AddStatement(OPC_PushPointed, Loc);
  */
  // use special instruction, so we can check for null ptr
  ec.AddStatement(OPC_PushPointedSliceLen, Loc);
}



//==========================================================================
//
//  VSliceGetPtr::VSliceGetPtr
//
//==========================================================================
VSliceGetPtr::VSliceGetPtr (VExpression *asexpr, const TLocation &aloc)
  : VExpression(aloc)
  , sexpr(asexpr)
{
}


//==========================================================================
//
//  VSliceGetPtr::~VSliceGetPtr
//
//==========================================================================
VSliceGetPtr::~VSliceGetPtr () {
  delete sexpr;
}


//==========================================================================
//
//  VSliceGetPtr::SyntaxCopy
//
//==========================================================================
VExpression *VSliceGetPtr::SyntaxCopy () {
  auto res = new VSliceGetPtr();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VSliceGetPtr::DoSyntaxCopyTo
//
//==========================================================================
void VSliceGetPtr::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VSliceGetPtr *)e;
  res->sexpr = (sexpr ? sexpr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VSliceGetPtr::DoResolve
//
//==========================================================================
VExpression *VSliceGetPtr::DoResolve (VEmitContext &ec) {
  //fprintf(stderr, "setype: <%s>\n", *sexpr->Type.GetName());
  Type = sexpr->Type.GetArrayInnerType();
  //Type = Type.MakePointerType();
  Flags |= sexpr->Flags&FIELD_ReadOnly;
  return this;
}


//==========================================================================
//
//  VSliceGetPtr::Emit
//
//==========================================================================
void VSliceGetPtr::Emit (VEmitContext &ec) {
  sexpr->Emit(ec);
  ec.AddStatement(OPC_PushPointedPtr, Loc);
}



//==========================================================================
//
//  VDictGetLength
//
//==========================================================================
VDictGetLength::VDictGetLength (VExpression *asexpr, const TLocation &aloc)
  : VExpression(aloc)
  , sexpr(asexpr)
{
}


//==========================================================================
//
//  VDictGetLength::~VDictGetLength
//
//==========================================================================
VDictGetLength::~VDictGetLength () {
  delete sexpr;
}


//==========================================================================
//
//  VDictGetLength::SyntaxCopy
//
//==========================================================================
VExpression *VDictGetLength::SyntaxCopy () {
  auto res = new VDictGetLength();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDictGetLength::DoSyntaxCopyTo
//
//==========================================================================
void VDictGetLength::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDictGetLength *)e;
  res->sexpr = (sexpr ? sexpr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDictGetLength::DoResolve
//
//==========================================================================
VExpression *VDictGetLength::DoResolve (VEmitContext &ec) {
  sexpr->RequestAddressOf();
  Type = VFieldType(TYPE_Int);
  return this;
}


//==========================================================================
//
//  VDictGetLength::Emit
//
//==========================================================================
void VDictGetLength::Emit (VEmitContext &ec) {
  sexpr->Emit(ec);
  ec.AddStatement(OPC_DictDispatch, sexpr->Type.GetDictKeyType(), sexpr->Type.GetDictValueType(), OPC_DictDispatch_Length, Loc);
}



//==========================================================================
//
//  VDictGetCapacity
//
//==========================================================================
VDictGetCapacity::VDictGetCapacity (VExpression *asexpr, const TLocation &aloc)
  : VExpression(aloc)
  , sexpr(asexpr)
{
}


//==========================================================================
//
//  VDictGetCapacity::~VDictGetCapacity
//
//==========================================================================
VDictGetCapacity::~VDictGetCapacity () {
  delete sexpr;
}


//==========================================================================
//
//  VDictGetCapacity::SyntaxCopy
//
//==========================================================================
VExpression *VDictGetCapacity::SyntaxCopy () {
  auto res = new VDictGetCapacity();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDictGetCapacity::DoSyntaxCopyTo
//
//==========================================================================
void VDictGetCapacity::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDictGetCapacity *)e;
  res->sexpr = (sexpr ? sexpr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDictGetCapacity::DoResolve
//
//==========================================================================
VExpression *VDictGetCapacity::DoResolve (VEmitContext &ec) {
  sexpr->RequestAddressOf();
  Type = VFieldType(TYPE_Int);
  return this;
}


//==========================================================================
//
//  VDictGetCapacity::Emit
//
//==========================================================================
void VDictGetCapacity::Emit (VEmitContext &ec) {
  sexpr->Emit(ec);
  ec.AddStatement(OPC_DictDispatch, sexpr->Type.GetDictKeyType(), sexpr->Type.GetDictValueType(), OPC_DictDispatch_Capacity, Loc);
}



//==========================================================================
//
//  VDictClearOrReset
//
//==========================================================================
VDictClearOrReset::VDictClearOrReset (VExpression *asexpr, bool aClear, const TLocation &aloc)
  : VExpression(aloc)
  , sexpr(asexpr)
  , doClear(aClear)
{
}


//==========================================================================
//
//  VDictClearOrReset::~VDictClearOrReset
//
//==========================================================================
VDictClearOrReset::~VDictClearOrReset () {
  delete sexpr;
}


//==========================================================================
//
//  VDictClearOrReset::SyntaxCopy
//
//==========================================================================
VExpression *VDictClearOrReset::SyntaxCopy () {
  auto res = new VDictClearOrReset();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDictClearOrReset::DoSyntaxCopyTo
//
//==========================================================================
void VDictClearOrReset::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDictClearOrReset *)e;
  res->sexpr = (sexpr ? sexpr->SyntaxCopy() : nullptr);
  res->doClear = doClear;
}


//==========================================================================
//
//  VDictClearOrReset::DoResolve
//
//==========================================================================
VExpression *VDictClearOrReset::DoResolve (VEmitContext &ec) {
  if (!sexpr) { delete this; return nullptr; }
  if ((Flags&FIELD_ReadOnly) != 0 || (sexpr->Flags&FIELD_ReadOnly) != 0) {
    ParseError(Loc, "Cannot change read-only dictionary");
    delete this;
    return nullptr;
  }
  sexpr->RequestAddressOf();
  Type = VFieldType(TYPE_Void);
  return this;
}


//==========================================================================
//
//  VDictClearOrReset::Emit
//
//==========================================================================
void VDictClearOrReset::Emit (VEmitContext &ec) {
  sexpr->Emit(ec);
  ec.AddStatement(OPC_DictDispatch, sexpr->Type.GetDictKeyType(), sexpr->Type.GetDictValueType(),
    (doClear ? OPC_DictDispatch_Clear : OPC_DictDispatch_Reset), Loc);
}



//==========================================================================
//
//  VDictRehash
//
//==========================================================================
VDictRehash::VDictRehash (VExpression *asexpr, bool aCompact, const TLocation &aloc)
  : VExpression(aloc)
  , sexpr(asexpr)
  , doCompact(aCompact)
{
}


//==========================================================================
//
//  VDictRehash::~VDictRehash
//
//==========================================================================
VDictRehash::~VDictRehash () {
  delete sexpr;
}


//==========================================================================
//
//  VDictRehash::SyntaxCopy
//
//==========================================================================
VExpression *VDictRehash::SyntaxCopy () {
  auto res = new VDictRehash();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDictRehash::DoSyntaxCopyTo
//
//==========================================================================
void VDictRehash::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDictRehash *)e;
  res->sexpr = (sexpr ? sexpr->SyntaxCopy() : nullptr);
  res->doCompact = doCompact;
}


//==========================================================================
//
//  VDictRehash::DoResolve
//
//==========================================================================
VExpression *VDictRehash::DoResolve (VEmitContext &ec) {
  if (!sexpr) { delete this; return nullptr; }
  if ((Flags&FIELD_ReadOnly) != 0 || (sexpr->Flags&FIELD_ReadOnly) != 0) {
    ParseError(Loc, "Cannot change read-only dictionary");
    delete this;
    return nullptr;
  }
  sexpr->RequestAddressOf();
  Type = VFieldType(TYPE_Void);
  return this;
}


//==========================================================================
//
//  VDictRehash::Emit
//
//==========================================================================
void VDictRehash::Emit (VEmitContext &ec) {
  sexpr->Emit(ec);
  ec.AddStatement(OPC_DictDispatch, sexpr->Type.GetDictKeyType(), sexpr->Type.GetDictValueType(),
    (doCompact ? OPC_DictDispatch_Compact : OPC_DictDispatch_Rehash), Loc);
}



//==========================================================================
//
//  VDictFind
//
//==========================================================================
VDictFind::VDictFind (VExpression *asexpr, VExpression *akeyexpr, const TLocation &aloc)
  : VExpression(aloc)
  , sexpr(asexpr)
  , keyexpr(akeyexpr)
{
}


//==========================================================================
//
//  VDictFind::~VDictFind
//
//==========================================================================
VDictFind::~VDictFind () {
  delete sexpr;
  delete keyexpr;
}


//==========================================================================
//
//  VDictFind::SyntaxCopy
//
//==========================================================================
VExpression *VDictFind::SyntaxCopy () {
  auto res = new VDictFind();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDictFind::DoSyntaxCopyTo
//
//==========================================================================
void VDictFind::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDictFind *)e;
  res->sexpr = (sexpr ? sexpr->SyntaxCopy() : nullptr);
  res->keyexpr = (keyexpr ? keyexpr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDictFind::DoResolve
//
//==========================================================================
VExpression *VDictFind::DoResolve (VEmitContext &ec) {
  if (!keyexpr) { ParseError(Loc, "`.find` cannot have empty argument"); delete this; return nullptr; }
  keyexpr = keyexpr->Resolve(ec);
  if (!keyexpr) { delete this; return nullptr; }

  if (sexpr->Type.GetDictKeyType().Type == TYPE_Float) keyexpr = keyexpr->CoerceToFloat(ec);
  if (!keyexpr->Type.Equals(sexpr->Type.GetDictKeyType())) {
    ParseError(Loc, "expected key type `%s`, but got `%s`", *sexpr->Type.GetDictKeyType().GetName(), *keyexpr->Type.GetName());
    delete this;
    return nullptr;
  }

  sexpr->RequestAddressOf();
  if (!VScriptDictElem::isSimpleType(sexpr->Type.GetDictKeyType())) keyexpr->RequestAddressOf();

  Type = sexpr->Type.GetDictValueType().MakePointerType();
  return this;
}


//==========================================================================
//
//  VDictFind::Emit
//
//==========================================================================
void VDictFind::Emit (VEmitContext &ec) {
  sexpr->Emit(ec);
  keyexpr->Emit(ec);
  ec.AddStatement(OPC_DictDispatch, sexpr->Type.GetDictKeyType(), sexpr->Type.GetDictValueType(), OPC_DictDispatch_Find, Loc);
}



//==========================================================================
//
//  VDictDelete
//
//==========================================================================
VDictDelete::VDictDelete (VExpression *asexpr, VExpression *akeyexpr, const TLocation &aloc)
  : VExpression(aloc)
  , sexpr(asexpr)
  , keyexpr(akeyexpr)
{
}


//==========================================================================
//
//  VDictDelete::~VDictDelete
//
//==========================================================================
VDictDelete::~VDictDelete () {
  delete sexpr;
  delete keyexpr;
}


//==========================================================================
//
//  VDictDelete::SyntaxCopy
//
//==========================================================================
VExpression *VDictDelete::SyntaxCopy () {
  auto res = new VDictDelete();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDictDelete::DoSyntaxCopyTo
//
//==========================================================================
void VDictDelete::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDictDelete *)e;
  res->sexpr = (sexpr ? sexpr->SyntaxCopy() : nullptr);
  res->keyexpr = (keyexpr ? keyexpr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDictDelete::DoResolve
//
//==========================================================================
VExpression *VDictDelete::DoResolve (VEmitContext &ec) {
  if ((Flags&FIELD_ReadOnly) != 0 || (sexpr && (sexpr->Flags&FIELD_ReadOnly) != 0)) {
    ParseError(Loc, "Cannot change read-only dictionary");
    delete this;
    return nullptr;
  }

  if (!keyexpr) { ParseError(Loc, "`.delete` cannot have empty argument"); delete this; return nullptr; }
  keyexpr = keyexpr->Resolve(ec);
  if (!keyexpr) { delete this; return nullptr; }

  if (sexpr->Type.GetDictKeyType().Type == TYPE_Float) keyexpr = keyexpr->CoerceToFloat(ec);
  if (!keyexpr->Type.Equals(sexpr->Type.GetDictKeyType())) {
    ParseError(Loc, "expected key type `%s`, but got `%s`", *sexpr->Type.GetDictKeyType().GetName(), *keyexpr->Type.GetName());
    delete this;
    return nullptr;
  }

  sexpr->RequestAddressOf();
  if (!VScriptDictElem::isSimpleType(sexpr->Type.GetDictKeyType())) keyexpr->RequestAddressOf();

  Type = VFieldType(TYPE_Int); // bool
  return this;
}


//==========================================================================
//
//  VDictDelete::Emit
//
//==========================================================================
void VDictDelete::Emit (VEmitContext &ec) {
  sexpr->Emit(ec);
  keyexpr->Emit(ec);
  ec.AddStatement(OPC_DictDispatch, sexpr->Type.GetDictKeyType(), sexpr->Type.GetDictValueType(), OPC_DictDispatch_Delete, Loc);
}



//==========================================================================
//
//  VDictPut
//
//==========================================================================
VDictPut::VDictPut (VExpression *asexpr, VExpression *akeyexpr, VExpression *avalexpr, const TLocation &aloc)
  : VExpression(aloc)
  , sexpr(asexpr)
  , keyexpr(akeyexpr)
  , valexpr(avalexpr)
{
}


//==========================================================================
//
//  VDictPut::~VDictPut
//
//==========================================================================
VDictPut::~VDictPut () {
  delete sexpr;
  delete keyexpr;
  delete valexpr;
}


//==========================================================================
//
//  VDictPut::SyntaxCopy
//
//==========================================================================
VExpression *VDictPut::SyntaxCopy () {
  auto res = new VDictPut();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDictPut::DoSyntaxCopyTo
//
//==========================================================================
void VDictPut::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDictPut *)e;
  res->sexpr = (sexpr ? sexpr->SyntaxCopy() : nullptr);
  res->keyexpr = (keyexpr ? keyexpr->SyntaxCopy() : nullptr);
  res->valexpr = (valexpr ? valexpr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDictPut::DoResolve
//
//==========================================================================
VExpression *VDictPut::DoResolve (VEmitContext &ec) {
  if (!sexpr) { delete this; return nullptr; }

  if ((Flags&FIELD_ReadOnly) != 0 || (sexpr->Flags&FIELD_ReadOnly) != 0) {
    ParseError(Loc, "Cannot change read-only dictionary");
    delete this;
    return nullptr;
  }

  if (!keyexpr || !valexpr) { ParseError(Loc, "`.put` cannot have empty arguments"); delete this; return nullptr; }
  keyexpr = keyexpr->Resolve(ec);
  valexpr = valexpr->Resolve(ec);
  if (!keyexpr || !valexpr) { delete this; return nullptr; }

  if (sexpr->Type.GetDictKeyType().Type == TYPE_Float) keyexpr = keyexpr->CoerceToFloat(ec);
  //if (!keyexpr->Type.Equals(sexpr->Type.GetDictKeyType()))
  if (!keyexpr->Type.CheckMatch(true, Loc, sexpr->Type.GetDictKeyType(), false))
  {
    ParseError(Loc, "expected key type `%s`, but got `%s`", *sexpr->Type.GetDictKeyType().GetName(), *keyexpr->Type.GetName());
    delete this;
    return nullptr;
  }

  if (sexpr->Type.GetDictValueType().Type == TYPE_Float) valexpr = valexpr->CoerceToFloat(ec);
  //if (!valexpr->Type.Equals(sexpr->Type.GetDictValueType()))
  if (!valexpr->Type.CheckMatch(true, Loc, sexpr->Type.GetDictValueType(), false))
  {
    ParseError(Loc, "expected value type `%s`, but got `%s`", *sexpr->Type.GetDictValueType().GetName(), *valexpr->Type.GetName());
    delete this;
    return nullptr;
  }

  sexpr->RequestAddressOf();
  if (!VScriptDictElem::isSimpleType(sexpr->Type.GetDictKeyType())) keyexpr->RequestAddressOf();
  if (!VScriptDictElem::isSimpleType(sexpr->Type.GetDictValueType())) valexpr->RequestAddressOf();

  Type = VFieldType(TYPE_Int); // bool
  return this;
}


//==========================================================================
//
//  VDictPut::Emit
//
//==========================================================================
void VDictPut::Emit (VEmitContext &ec) {
  sexpr->Emit(ec);
  keyexpr->Emit(ec);
  valexpr->Emit(ec);
  ec.AddStatement(OPC_DictDispatch, sexpr->Type.GetDictKeyType(), sexpr->Type.GetDictValueType(), OPC_DictDispatch_Put, Loc);
}



//==========================================================================
//
//  VDictFirstIndex
//
//==========================================================================
VDictFirstIndex::VDictFirstIndex (VExpression *asexpr, const TLocation &aloc)
  : VExpression(aloc)
  , sexpr(asexpr)
{
}


//==========================================================================
//
//  VDictFirstIndex::~VDictFirstIndex
//
//==========================================================================
VDictFirstIndex::~VDictFirstIndex () {
  delete sexpr;
}


//==========================================================================
//
//  VDictFirstIndex::SyntaxCopy
//
//==========================================================================
VExpression *VDictFirstIndex::SyntaxCopy () {
  auto res = new VDictFirstIndex();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDictFirstIndex::DoSyntaxCopyTo
//
//==========================================================================
void VDictFirstIndex::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDictFirstIndex *)e;
  res->sexpr = (sexpr ? sexpr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDictFirstIndex::DoResolve
//
//==========================================================================
VExpression *VDictFirstIndex::DoResolve (VEmitContext &ec) {
  sexpr->RequestAddressOf();
  Type = VFieldType(TYPE_Int);
  return this;
}


//==========================================================================
//
//  VDictFirstIndex::Emit
//
//==========================================================================
void VDictFirstIndex::Emit (VEmitContext &ec) {
  sexpr->Emit(ec);
  ec.AddStatement(OPC_DictDispatch, sexpr->Type.GetDictKeyType(), sexpr->Type.GetDictValueType(), OPC_DictDispatch_FirstIndex, Loc);
}



//==========================================================================
//
//  VDictIsValidIndex
//
//==========================================================================
VDictIsValidIndex::VDictIsValidIndex (VExpression *asexpr, VExpression *aidxexpr, const TLocation &aloc)
  : VExpression(aloc)
  , sexpr(asexpr)
  , idxexpr(aidxexpr)
{
}


//==========================================================================
//
//  VDictIsValidIndex::~VDictIsValidIndex
//
//==========================================================================
VDictIsValidIndex::~VDictIsValidIndex () {
  delete sexpr;
  delete idxexpr;
}


//==========================================================================
//
//  VDictIsValidIndex::SyntaxCopy
//
//==========================================================================
VExpression *VDictIsValidIndex::SyntaxCopy () {
  auto res = new VDictIsValidIndex();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDictIsValidIndex::DoSyntaxCopyTo
//
//==========================================================================
void VDictIsValidIndex::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDictIsValidIndex *)e;
  res->sexpr = (sexpr ? sexpr->SyntaxCopy() : nullptr);
  res->idxexpr = (idxexpr ? idxexpr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDictIsValidIndex::DoResolve
//
//==========================================================================
VExpression *VDictIsValidIndex::DoResolve (VEmitContext &ec) {
  if (!idxexpr) { ParseError(Loc, "`.isValidIndex` cannot have empty arguments"); delete this; return nullptr; }
  idxexpr = idxexpr->Resolve(ec);
  if (!idxexpr) { delete this; return nullptr; }

  if (idxexpr->Type.Type != TYPE_Int && idxexpr->Type.Type != TYPE_Byte) {
    ParseError(Loc, "`.isValidIndex` argument must be `int`, but got `%s`", *idxexpr->Type.GetName());
    delete this;
    return nullptr;
  }

  sexpr->RequestAddressOf();
  Type = VFieldType(TYPE_Int); // bool
  return this;
}


//==========================================================================
//
//  VDictIsValidIndex::Emit
//
//==========================================================================
void VDictIsValidIndex::Emit (VEmitContext &ec) {
  sexpr->Emit(ec);
  idxexpr->Emit(ec);
  ec.AddStatement(OPC_DictDispatch, sexpr->Type.GetDictKeyType(), sexpr->Type.GetDictValueType(), OPC_DictDispatch_IsValidIndex, Loc);
}



//==========================================================================
//
//  VDictNextIndex
//
//==========================================================================
VDictNextIndex::VDictNextIndex (VExpression *asexpr, VExpression *aidxexpr, bool aRemove, const TLocation &aloc)
  : VExpression(aloc)
  , sexpr(asexpr)
  , idxexpr(aidxexpr)
  , doRemove(aRemove)
{
}


//==========================================================================
//
//  VDictNextIndex::~VDictNextIndex
//
//==========================================================================
VDictNextIndex::~VDictNextIndex () {
  delete sexpr;
  delete idxexpr;
}


//==========================================================================
//
//  VDictNextIndex::SyntaxCopy
//
//==========================================================================
VExpression *VDictNextIndex::SyntaxCopy () {
  auto res = new VDictNextIndex();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDictNextIndex::DoSyntaxCopyTo
//
//==========================================================================
void VDictNextIndex::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDictNextIndex *)e;
  res->sexpr = (sexpr ? sexpr->SyntaxCopy() : nullptr);
  res->idxexpr = (idxexpr ? idxexpr->SyntaxCopy() : nullptr);
  res->doRemove = doRemove;
}


//==========================================================================
//
//  VDictNextIndex::DoResolve
//
//==========================================================================
VExpression *VDictNextIndex::DoResolve (VEmitContext &ec) {
  if (!idxexpr) { ParseError(Loc, "`.nextIndex` cannot have empty arguments"); delete this; return nullptr; }
  idxexpr = idxexpr->Resolve(ec);
  if (!idxexpr) { delete this; return nullptr; }

  if (idxexpr->Type.Type != TYPE_Int && idxexpr->Type.Type != TYPE_Byte) {
    ParseError(Loc, "`.nextIndex` argument must be `int`, but got `%s`", *idxexpr->Type.GetName());
    delete this;
    return nullptr;
  }

  if (doRemove && ((Flags&FIELD_ReadOnly) != 0 || (sexpr->Flags&FIELD_ReadOnly) != 0)) {
    ParseError(Loc, "Cannot remove elements from read-only dictionary");
    delete this;
    return nullptr;
  }

  sexpr->RequestAddressOf();
  Type = VFieldType(TYPE_Int);
  return this;
}


//==========================================================================
//
//  VDictNextIndex::Emit
//
//==========================================================================
void VDictNextIndex::Emit (VEmitContext &ec) {
  sexpr->Emit(ec);
  idxexpr->Emit(ec);
  ec.AddStatement(OPC_DictDispatch, sexpr->Type.GetDictKeyType(), sexpr->Type.GetDictValueType(),
    (doRemove ? OPC_DictDispatch_DelAndNextIndex : OPC_DictDispatch_NextIndex), Loc);
}



//==========================================================================
//
//  VDictKeyAtIndex
//
//==========================================================================
VDictKeyAtIndex::VDictKeyAtIndex (VExpression *asexpr, VExpression *aidxexpr, const TLocation &aloc)
  : VExpression(aloc)
  , sexpr(asexpr)
  , idxexpr(aidxexpr)
{
}


//==========================================================================
//
//  VDictKeyAtIndex::~VDictKeyAtIndex
//
//==========================================================================
VDictKeyAtIndex::~VDictKeyAtIndex () {
  delete sexpr;
  delete idxexpr;
}


//==========================================================================
//
//  VDictKeyAtIndex::SyntaxCopy
//
//==========================================================================
VExpression *VDictKeyAtIndex::SyntaxCopy () {
  auto res = new VDictKeyAtIndex();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDictKeyAtIndex::DoSyntaxCopyTo
//
//==========================================================================
void VDictKeyAtIndex::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDictKeyAtIndex *)e;
  res->sexpr = (sexpr ? sexpr->SyntaxCopy() : nullptr);
  res->idxexpr = (idxexpr ? idxexpr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDictKeyAtIndex::DoResolve
//
//==========================================================================
VExpression *VDictKeyAtIndex::DoResolve (VEmitContext &ec) {
  if (!idxexpr) { ParseError(Loc, "`.keyAtIndex` cannot have empty arguments"); delete this; return nullptr; }
  idxexpr = idxexpr->Resolve(ec);
  if (!idxexpr) { delete this; return nullptr; }

  if (idxexpr->Type.Type != TYPE_Int && idxexpr->Type.Type != TYPE_Byte) {
    ParseError(Loc, "`.keyAtIndex` argument must be `int`, but got `%s`", *idxexpr->Type.GetName());
    delete this;
    return nullptr;
  }

  sexpr->RequestAddressOf();
  Type = sexpr->Type.GetDictKeyType();
  if (Type.Type != TYPE_String && !VScriptDictElem::isSimpleType(Type)) Type = Type.MakePointerType();
  Flags |= FIELD_ReadOnly;
  return this;
}


//==========================================================================
//
//  VDictKeyAtIndex::Emit
//
//==========================================================================
void VDictKeyAtIndex::Emit (VEmitContext &ec) {
  sexpr->Emit(ec);
  idxexpr->Emit(ec);
  ec.AddStatement(OPC_DictDispatch, sexpr->Type.GetDictKeyType(), sexpr->Type.GetDictValueType(), OPC_DictDispatch_GetKeyAtIndex, Loc);
}



//==========================================================================
//
//  VDictValueAtIndex
//
//==========================================================================
VDictValueAtIndex::VDictValueAtIndex (VExpression *asexpr, VExpression *aidxexpr, const TLocation &aloc)
  : VExpression(aloc)
  , sexpr(asexpr)
  , idxexpr(aidxexpr)
{
}


//==========================================================================
//
//  VDictValueAtIndex::~VDictValueAtIndex
//
//==========================================================================
VDictValueAtIndex::~VDictValueAtIndex () {
  delete sexpr;
  delete idxexpr;
}


//==========================================================================
//
//  VDictValueAtIndex::SyntaxCopy
//
//==========================================================================
VExpression *VDictValueAtIndex::SyntaxCopy () {
  auto res = new VDictValueAtIndex();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDictValueAtIndex::DoSyntaxCopyTo
//
//==========================================================================
void VDictValueAtIndex::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDictValueAtIndex *)e;
  res->sexpr = (sexpr ? sexpr->SyntaxCopy() : nullptr);
  res->idxexpr = (idxexpr ? idxexpr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDictValueAtIndex::DoResolve
//
//==========================================================================
VExpression *VDictValueAtIndex::DoResolve (VEmitContext &ec) {
  if (!idxexpr) { ParseError(Loc, "`.valueAtIndex` cannot have empty arguments"); delete this; return nullptr; }
  idxexpr = idxexpr->Resolve(ec);
  if (!idxexpr) { delete this; return nullptr; }

  if (idxexpr->Type.Type != TYPE_Int && idxexpr->Type.Type != TYPE_Byte) {
    ParseError(Loc, "`.valueAtIndex` argument must be `int`, but got `%s`", *idxexpr->Type.GetName());
    delete this;
    return nullptr;
  }

  sexpr->RequestAddressOf();
  Type = sexpr->Type.GetDictValueType();
  Type = Type.MakePointerType();
  Flags |= sexpr->Flags&FIELD_ReadOnly;
  return this;
}


//==========================================================================
//
//  VDictValueAtIndex::Emit
//
//==========================================================================
void VDictValueAtIndex::Emit (VEmitContext &ec) {
  sexpr->Emit(ec);
  idxexpr->Emit(ec);
  ec.AddStatement(OPC_DictDispatch, sexpr->Type.GetDictKeyType(), sexpr->Type.GetDictValueType(), OPC_DictDispatch_GetValueAtIndex, Loc);
}



//==========================================================================
//
//  VStructZero
//
//==========================================================================
VStructZero::VStructZero (VExpression *asexpr, const TLocation &aloc)
  : VExpression(aloc)
  , sexpr(asexpr)
{
}


//==========================================================================
//
//  VStructZero::~VStructZero
//
//==========================================================================
VStructZero::~VStructZero () {
  delete sexpr;
}


//==========================================================================
//
//  VStructZero::SyntaxCopy
//
//==========================================================================
VExpression *VStructZero::SyntaxCopy () {
  auto res = new VStructZero();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VStructZero::DoSyntaxCopyTo
//
//==========================================================================
void VStructZero::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VStructZero *)e;
  res->sexpr = (sexpr ? sexpr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VStructZero::DoResolve
//
//==========================================================================
VExpression *VStructZero::DoResolve (VEmitContext &ec) {
  if (!sexpr) { delete this; return nullptr; }
  vassert(sexpr->Type.Type == TYPE_Struct || sexpr->Type.Type == TYPE_Vector);
  if (sexpr->Flags&FIELD_ReadOnly) {
    ParseError(Loc, "cannot zero read-only struct");
    delete this;
    return nullptr;
  }
  sexpr->RequestAddressOf();
  Type = VFieldType(TYPE_Void);
  return this;
}


//==========================================================================
//
//  VStructZero::Emit
//
//==========================================================================
void VStructZero::Emit (VEmitContext &ec) {
  sexpr->Emit(ec);
  if (sexpr->Type.Type == TYPE_Struct && sexpr->Type.Struct) {
    if (sexpr->Type.Struct->NeedsDestructor()) {
      ec.AddStatement(OPC_ZeroPointedStruct, sexpr->Type.Struct, Loc);
    } else {
      ec.AddStatement(OPC_ZeroByPtr, sexpr->Type.GetSize(), Loc);
    }
  } else {
    // default vector
    ec.AddStatement(OPC_ZeroByPtr, sexpr->Type.GetSize(), Loc);
  }
}
