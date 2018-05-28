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

#include "vc_local.h"


//==========================================================================
//
//  VAssignment::VAssignment
//
//==========================================================================
VAssignment::VAssignment (VAssignment::EAssignOper AOper, VExpression *AOp1, VExpression *AOp2, const TLocation &ALoc)
  : VExpression(ALoc)
  , Oper(AOper)
  , op1(AOp1)
  , op2(AOp2)
{
  if (!op2) {
    ParseError(Loc, "Expression required on the right side of assignment operator");
    return;
  }
}


//==========================================================================
//
//  VAssignment::~VAssignment
//
//==========================================================================
VAssignment::~VAssignment () {
  if (op1) { delete op1; op1 = nullptr; }
  if (op2) { delete op2; op2 = nullptr; }
}


//==========================================================================
//
//  VAssignment::SyntaxCopy
//
//==========================================================================
VExpression *VAssignment::SyntaxCopy () {
  auto res = new VAssignment();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VAssignment::DoRestSyntaxCopyTo
//
//==========================================================================
void VAssignment::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VAssignment *)e;
  res->Oper = Oper;
  res->op1 = (op1 ? op1->SyntaxCopy() : nullptr);
  res->op2 = (op2 ? op2->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VAssignment::DoResolve
//
//==========================================================================
VExpression *VAssignment::DoResolve (VEmitContext &ec) {
  if (op1) op1 = op1->ResolveAssignmentTarget(ec);
  if (op2) op2 = (op1 && op1->Type.Type == TYPE_Float ? op2->ResolveFloat(ec) : op2->ResolveAssignmentValue(ec));

  if (!op1 || !op2) { delete this; return nullptr; }

  if (op1->IsPropertyAssign()) {
    if (Oper != Assign) {
      ParseError(Loc, "Only `=` can be used to assign to a property");
      delete this;
      return nullptr;
    }
    VPropertyAssign* e = (VPropertyAssign*)op1;
    e->NumArgs = 1;
    e->Args[0] = op2;
    op1 = nullptr;
    op2 = nullptr;
    delete this;
    return e->Resolve(ec);
  }

  if (op1->IsDynArraySetNum()) {
    if (Oper != Assign && Oper != AddAssign && Oper != MinusAssign) {
      ParseError(Loc, "Only `=`, `+=`, or `-=` can be used to resize an array");
      delete this;
      return nullptr;
    }
    op2->Type.CheckMatch(Loc, VFieldType(TYPE_Int));
    VDynArraySetNum *e = (VDynArraySetNum*)op1;
    e->NumExpr = op2;
         if (Oper == Assign) e->opsign = 0;
    else if (Oper == AddAssign) e->opsign = 1;
    else if (Oper == MinusAssign) e->opsign = -1;
    op1 = nullptr;
    op2 = nullptr;
    delete this;
    return e->Resolve(ec);
  }

  op2->Type.CheckMatch(Loc, op1->RealType);
  op1->RequestAddressOf();
  return this;
}


//==========================================================================
//
//  VAssignment::Emit
//
//==========================================================================
void VAssignment::Emit (VEmitContext &ec) {
  op1->Emit(ec);
  op2->Emit(ec);

  switch (Oper) {
    case Assign:
           if (op1->RealType.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_AssignDrop);
      else if (op1->RealType.Type == TYPE_Byte && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_ByteAssignDrop);
      else if (op1->RealType.Type == TYPE_Float && op2->Type.Type == TYPE_Float) ec.AddStatement(OPC_AssignDrop);
      else if (op1->RealType.Type == TYPE_Name && op2->Type.Type == TYPE_Name) ec.AddStatement(OPC_AssignDrop);
      else if (op1->RealType.Type == TYPE_String && op2->Type.Type == TYPE_String) ec.AddStatement(OPC_AssignStrDrop);
      else if (op1->RealType.Type == TYPE_Pointer && op2->Type.Type == TYPE_Pointer) ec.AddStatement(OPC_AssignPtrDrop);
      else if (op1->RealType.Type == TYPE_Vector && op2->Type.Type == TYPE_Vector) ec.AddStatement(OPC_VAssignDrop);
      else if (op1->RealType.Type == TYPE_Class && (op2->Type.Type == TYPE_Class || (op2->Type.Type == TYPE_Reference && op2->Type.Class == nullptr))) ec.AddStatement(OPC_AssignPtrDrop);
      else if (op1->RealType.Type == TYPE_State && (op2->Type.Type == TYPE_State || (op2->Type.Type == TYPE_Reference && op2->Type.Class == nullptr))) ec.AddStatement(OPC_AssignPtrDrop);
      else if (op1->RealType.Type == TYPE_Reference && op2->Type.Type == TYPE_Reference) ec.AddStatement(OPC_AssignPtrDrop);
      else if (op1->RealType.Type == TYPE_Bool && op2->Type.Type == TYPE_Int) {
             if (op1->RealType.BitMask&0x000000ff) ec.AddStatement(OPC_AssignBool0, (int)op1->RealType.BitMask);
        else if (op1->RealType.BitMask&0x0000ff00) ec.AddStatement(OPC_AssignBool1, (int)(op1->RealType.BitMask >> 8));
        else if (op1->RealType.BitMask&0x00ff0000) ec.AddStatement(OPC_AssignBool2, (int)(op1->RealType.BitMask >> 16));
        else ec.AddStatement(OPC_AssignBool3, (int)(op1->RealType.BitMask >> 24));
      } else if (op1->RealType.Type == TYPE_Delegate && op2->Type.Type == TYPE_Delegate) {
        ec.AddStatement(OPC_AssignDelegate);
      } else if (op1->RealType.Type == TYPE_Delegate && op2->Type.Type == TYPE_Reference && op2->Type.Class == nullptr) {
        ec.AddStatement(OPC_PushNull);
        ec.AddStatement(OPC_AssignDelegate);
      } else {
        ParseError(Loc, "Expression type mismatch");
      }
      break;

    case AddAssign:
           if (op1->RealType.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_AddVarDrop);
      else if (op1->RealType.Type == TYPE_Byte && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_ByteAddVarDrop);
      else if (op1->RealType.Type == TYPE_Float && op2->Type.Type == TYPE_Float) ec.AddStatement(OPC_FAddVarDrop);
      else if (op1->RealType.Type == TYPE_Vector && op2->Type.Type == TYPE_Vector) ec.AddStatement(OPC_VAddVarDrop);
      else ParseError(Loc, "Expression type mismatch");
      break;

    case MinusAssign:
           if (op1->RealType.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_SubVarDrop);
      else if (op1->RealType.Type == TYPE_Byte && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_ByteSubVarDrop);
      else if (op1->RealType.Type == TYPE_Float && op2->Type.Type == TYPE_Float) ec.AddStatement(OPC_FSubVarDrop);
      else if (op1->RealType.Type == TYPE_Vector && op2->Type.Type == TYPE_Vector) ec.AddStatement(OPC_VSubVarDrop);
      else ParseError(Loc, "Expression type mismatch");
      break;

    case MultiplyAssign:
           if (op1->RealType.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_MulVarDrop);
      else if (op1->RealType.Type == TYPE_Byte && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_ByteMulVarDrop);
      else if (op1->RealType.Type == TYPE_Float && op2->Type.Type == TYPE_Float) ec.AddStatement(OPC_FMulVarDrop);
      else if (op1->RealType.Type == TYPE_Vector && op2->Type.Type == TYPE_Float) ec.AddStatement(OPC_VScaleVarDrop);
      else ParseError(Loc, "Expression type mismatch");
      break;

    case DivideAssign:
           if (op1->RealType.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_DivVarDrop);
      else if (op1->RealType.Type == TYPE_Byte && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_ByteDivVarDrop);
      else if (op1->RealType.Type == TYPE_Float && op2->Type.Type == TYPE_Float) ec.AddStatement(OPC_FDivVarDrop);
      else if (op1->RealType.Type == TYPE_Vector && op2->Type.Type == TYPE_Float) ec.AddStatement(OPC_VIScaleVarDrop);
      else ParseError(Loc, "Expression type mismatch");
      break;

    case ModAssign:
           if (op1->RealType.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_ModVarDrop);
      else if (op1->RealType.Type == TYPE_Byte && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_ByteModVarDrop);
      else ParseError(Loc, "Expression type mismatch");
      break;

    case AndAssign:
           if (op1->RealType.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_AndVarDrop);
      else if (op1->RealType.Type == TYPE_Byte && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_ByteAndVarDrop);
      else ParseError(Loc, "Expression type mismatch");
      break;

    case OrAssign:
           if (op1->RealType.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_OrVarDrop);
      else if (op1->RealType.Type == TYPE_Byte && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_ByteOrVarDrop);
      //FIXME This is wrong!
      else if (op1->RealType.Type == TYPE_Bool && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_OrVarDrop);
      else ParseError(Loc, "Expression type mismatch");
      break;

    case XOrAssign:
           if (op1->RealType.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_XOrVarDrop);
      else if (op1->RealType.Type == TYPE_Byte && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_ByteXOrVarDrop);
      else ParseError(Loc, "Expression type mismatch");
      break;

    case LShiftAssign:
           if (op1->RealType.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_LShiftVarDrop);
      else if (op1->RealType.Type == TYPE_Byte && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_ByteLShiftVarDrop);
      else ParseError(Loc, "Expression type mismatch");
      break;

    case RShiftAssign:
           if (op1->RealType.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_RShiftVarDrop);
      else if (op1->RealType.Type == TYPE_Byte && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_ByteRShiftVarDrop);
      else ParseError(Loc, "Expression type mismatch");
      break;

    default:
      ParseError(Loc, "VC INTERNAL COMPILER ERROR: unknown assign operation");
  }
}


//==========================================================================
//
//  VAssignment::IsAssignExpr
//
//==========================================================================
bool VAssignment::IsAssignExpr () const {
  return true;
}


//==========================================================================
//
//  VPropertyAssign::VPropertyAssign
//
//==========================================================================
VPropertyAssign::VPropertyAssign (VExpression *ASelfExpr, VMethod *AFunc, bool AHaveSelf, const TLocation &ALoc)
  : VInvocation(ASelfExpr, AFunc, nullptr, AHaveSelf, false, ALoc, 0, nullptr)
{
}


//==========================================================================
//
//  VPropertyAssign::SyntaxCopy
//
//==========================================================================
VExpression *VPropertyAssign::SyntaxCopy () {
  auto res = new VPropertyAssign();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VPropertyAssign::DoRestSyntaxCopyTo
//
//==========================================================================
void VPropertyAssign::DoSyntaxCopyTo (VExpression *e) {
  VInvocation::DoSyntaxCopyTo(e);
}


//==========================================================================
//
//  VPropertyAssign::IsPropertyAssign
//
//==========================================================================
bool VPropertyAssign::IsPropertyAssign () const {
  return true;
}
