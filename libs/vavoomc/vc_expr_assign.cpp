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
//**  Copyright (C) 2018-2020 Ketmar Dark
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
  if (op1 && op2 && Oper == Assign) {
    bool resolved = false;
    op1 = op1->ResolveCompleteAssign(ec, op2, resolved);
    if (!op1) {
      op2 = nullptr;
      delete this;
      return nullptr;
    }
    if (resolved) {
      VExpression *e = op1;
      op1 = nullptr;
      op2 = nullptr;
      delete this;
      return e;
    }
  }

  // to optimize `+= 1` and `-= 1`
  /*
  AutoCopy op1copy, op2copy;
  if (Oper == AddAssign || Oper == MinusAssign) {
    op1copy.assignSyntaxCopy(op1);
    op2copy.assignSyntaxCopy(op2);
  }
  */

  if (op1) op1 = op1->ResolveAssignmentTarget(ec);
  if (op2) op2 = (op1 && op1->Type.Type == TYPE_Float ? op2->ResolveFloat(ec) : op2->ResolveAssignmentValue(ec));

  if (!op1 || !op2) { delete this; return nullptr; }

  if (op1->IsReadOnly()) {
    ParseError(Loc, "Cannot assign to read-only destination");
    delete this;
    return nullptr;
  }

  if (op1->IsPropertyAssign()) {
    if (Oper != Assign) {
      ParseError(Loc, "Only `=` can be used to assign to a property");
      delete this;
      return nullptr;
    }
    VPropertyAssign *e = (VPropertyAssign *)op1;
    e->NumArgs = 1;
    e->Args[0] = op2;
    op1 = nullptr;
    op2 = nullptr;
    delete this;
    return e->Resolve(ec);
  }

  if (op1->IsDynArraySetNum()) {
    if (Oper != Assign && Oper != AddAssign && Oper != MinusAssign) {
      ParseError(Loc, "Only `=`, `+=`, or `-=` can be used to resize a dynamic array");
      delete this;
      return nullptr;
    }
    op2->Type.CheckMatch(false, Loc, VFieldType(TYPE_Int));
    VDynArraySetNum *e = (VDynArraySetNum *)op1;
    e->NumExpr = op2;
         if (Oper == Assign) e->opsign = 0;
    else if (Oper == AddAssign) e->opsign = 1;
    else if (Oper == MinusAssign) e->opsign = -1;
    op1 = nullptr;
    op2 = nullptr;
    delete this;
    return e->Resolve(ec);
  }

  // struct assignment
  if (Oper == Assign && op1->Type.Type == TYPE_Struct && op2->Type.Type == TYPE_Struct) {
    op2->Type.CheckMatch(true/*asref*/, Loc, op1->Type);
    op1->RequestAddressOfForAssign();
    op2->RequestAddressOf();
  } else {
    if (op1->Type.Type == TYPE_Vector && (Oper == AddAssign || Oper == MinusAssign || Oper == MultiplyAssign || Oper == DivideAssign)) {
      switch (op2->Type.Type) {
        case TYPE_Int:
        case TYPE_Byte:
        case TYPE_Float:
          if (Oper == MultiplyAssign || Oper == DivideAssign) {
            op2 = op2->CoerceToFloat(ec);
          } else {
            goto dodefvec;
          }
          break;
        case TYPE_Vector:
          break;
        default:
         dodefvec:
          if (!op2->Type.CheckMatch(false, Loc, op1->RealType)) {
            delete this;
            return nullptr;
          }
          break;
      }
    } else {
      if (!op2->Type.CheckMatch(false, Loc, op1->RealType)) {
        delete this;
        return nullptr;
      }
    }
    op1->RequestAddressOfForAssign();
  }

  return this;
}


//==========================================================================
//
//  VAssignment::Emit
//
//==========================================================================
void VAssignment::Emit (VEmitContext &ec) {
  if (!op1 || !op2) return; // just in case
  switch (Oper) {
    case Assign:
      op1->Emit(ec);
      op2->Emit(ec);
    doassign:
           if (op1->RealType.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_AssignDrop, Loc);
      else if (op1->RealType.Type == TYPE_Byte && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_ByteAssignDrop, Loc);
      else if (op1->RealType.Type == TYPE_Float && op2->Type.Type == TYPE_Float) ec.AddStatement(OPC_AssignDrop, Loc);
      else if (op1->RealType.Type == TYPE_Name && op2->Type.Type == TYPE_Name) ec.AddStatement(OPC_AssignDrop, Loc);
      else if (op1->RealType.Type == TYPE_String && op2->Type.Type == TYPE_String) ec.AddStatement(OPC_AssignStrDrop, Loc);
      else if (op1->RealType.Type == TYPE_Pointer && op2->Type.Type == TYPE_Pointer) ec.AddStatement(OPC_AssignPtrDrop, Loc);
      else if (op1->RealType.Type == TYPE_Vector && op2->Type.Type == TYPE_Vector) ec.AddStatement(OPC_VAssignDrop, Loc);
      else if (op1->RealType.Type == TYPE_Class && (op2->Type.Type == TYPE_Class || (op2->Type.Type == TYPE_Reference && op2->Type.Class == nullptr))) ec.AddStatement(OPC_AssignPtrDrop, Loc);
      else if (op1->RealType.Type == TYPE_State && (op2->Type.Type == TYPE_State || (op2->Type.Type == TYPE_Reference && op2->Type.Class == nullptr))) ec.AddStatement(OPC_AssignPtrDrop, Loc);
      else if (op1->RealType.Type == TYPE_Reference && op2->Type.Type == TYPE_Reference) ec.AddStatement(OPC_AssignPtrDrop, Loc);
      else if (op1->RealType.Type == TYPE_Bool && op2->Type.Type == TYPE_Int) {
             if (op1->RealType.BitMask&0x000000ff) ec.AddStatement(OPC_AssignBool0, (int)op1->RealType.BitMask, Loc);
        else if (op1->RealType.BitMask&0x0000ff00) ec.AddStatement(OPC_AssignBool1, (int)(op1->RealType.BitMask>>8), Loc);
        else if (op1->RealType.BitMask&0x00ff0000) ec.AddStatement(OPC_AssignBool2, (int)(op1->RealType.BitMask>>16), Loc);
        else ec.AddStatement(OPC_AssignBool3, (int)(op1->RealType.BitMask>>24), Loc);
      } else if (op1->RealType.Type == TYPE_Delegate && op2->Type.Type == TYPE_Delegate) {
        ec.AddStatement(OPC_AssignDelegate, Loc);
      } else if (op1->RealType.Type == TYPE_Delegate && op2->Type.Type == TYPE_Reference && op2->Type.Class == nullptr) {
        // note: `op2` can be `none` literal, but it doesn't matter here
        ec.AddStatement(OPC_PushNull, Loc);
        ec.AddStatement(OPC_AssignDelegate, Loc);
      } else if (op1->RealType.Type == TYPE_Struct) {
        vassert(op2->RealType.Type == TYPE_Struct);
        vassert(op1->RealType.Struct->IsA(op2->RealType.Struct));
        ec.AddStatement(OPC_TypeDeepCopy, op2->RealType, Loc);
      } else {
        ParseError(Loc, "Expression type mismatch (`%s` and `%s`)", *op1->Type.GetName(), *op2->Type.GetName());
      }
      break;

    case AddAssign:
      // optimize `+= 1`, `+= -1`, `+= 0`
      if (op2->IsIntConst()) {
        if (op2->GetIntConst() == 1) {
          // +1
          if (op1->RealType.Type == TYPE_Int) { op1->Emit(ec); ec.AddStatement(OPC_IncDrop, Loc); break; }
          if (op1->RealType.Type == TYPE_Byte) { op1->Emit(ec); ec.AddStatement(OPC_ByteIncDrop, Loc); break; }
        } else if (op2->GetIntConst() == -1) {
          // -1
          if (op1->RealType.Type == TYPE_Int) { op1->Emit(ec); ec.AddStatement(OPC_DecDrop, Loc); break; }
          if (op1->RealType.Type == TYPE_Byte) { op1->Emit(ec); ec.AddStatement(OPC_ByteDecDrop, Loc); break; }
        } else if (op2->GetIntConst() == 0) {
          // +0
          if (op1->RealType.Type == TYPE_Int || op1->RealType.Type == TYPE_Byte) break;
        }
      } else if (op2->IsFloatConst()) {
        // +0 for floats
        if (op2->GetFloatConst() == 0 && op1->RealType.Type == TYPE_Float) break;
      }
      op1->Emit(ec);
      op2->Emit(ec);
           if (op1->RealType.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_AddVarDrop, Loc);
      else if (op1->RealType.Type == TYPE_Byte && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_ByteAddVarDrop, Loc);
      else if (op1->RealType.Type == TYPE_Float && op2->Type.Type == TYPE_Float) ec.AddStatement(OPC_FAddVarDrop, Loc);
      else if (op1->RealType.Type == TYPE_Vector && op2->Type.Type == TYPE_Vector) ec.AddStatement(OPC_VAddVarDrop, Loc);
      else ParseError(Loc, "Expression type mismatch (`%s` and `%s`)", *op1->Type.GetName(), *op2->Type.GetName());
      break;

    case MinusAssign:
      // optimize `+= 1`, `+= -1`, `+= 0`
      if (op2->IsIntConst()) {
        if (op2->GetIntConst() == 1) {
          // -1
          if (op1->RealType.Type == TYPE_Int) { op1->Emit(ec); ec.AddStatement(OPC_DecDrop, Loc); break; }
          if (op1->RealType.Type == TYPE_Byte) { op1->Emit(ec); ec.AddStatement(OPC_ByteDecDrop, Loc); break; }
        } else if (op2->GetIntConst() == -1) {
          // +1
          if (op1->RealType.Type == TYPE_Int) { op1->Emit(ec); ec.AddStatement(OPC_IncDrop, Loc); break; }
          if (op1->RealType.Type == TYPE_Byte) { op1->Emit(ec); ec.AddStatement(OPC_ByteIncDrop, Loc); break; }
        } else if (op2->GetIntConst() == 0) {
          // -0
          if (op1->RealType.Type == TYPE_Int || op1->RealType.Type == TYPE_Byte) break;
        }
      } else if (op2->IsFloatConst()) {
        // -0 for floats
        if (op2->GetFloatConst() == 0 && op1->RealType.Type == TYPE_Float) break;
      }
      op1->Emit(ec);
      op2->Emit(ec);
           if (op1->RealType.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_SubVarDrop, Loc);
      else if (op1->RealType.Type == TYPE_Byte && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_ByteSubVarDrop, Loc);
      else if (op1->RealType.Type == TYPE_Float && op2->Type.Type == TYPE_Float) ec.AddStatement(OPC_FSubVarDrop, Loc);
      else if (op1->RealType.Type == TYPE_Vector && op2->Type.Type == TYPE_Vector) ec.AddStatement(OPC_VSubVarDrop, Loc);
      else ParseError(Loc, "Expression type mismatch (`%s` and `%s`)", *op1->Type.GetName(), *op2->Type.GetName());
      break;

    case MultiplyAssign:
      // optimize `*= 1`, `*= 0`
      if (op2->IsIntConst()) {
        if (op2->GetIntConst() == 1) {
          // *1
          if (op1->RealType.Type == TYPE_Int || op1->RealType.Type == TYPE_Byte) break;
        } else if (op2->GetIntConst() == 0) {
          // *0
          if (op1->RealType.Type == TYPE_Int || op1->RealType.Type == TYPE_Byte) {
            op1->Emit(ec);
            ec.EmitPushNumber(0, Loc);
            goto doassign;
          }
        }
      } else if (op2->IsFloatConst()) {
        // *1 for floats: it is safe, 'cause nan*1==nan, and [+-]inf*1 == [+-]inf
        if (op2->GetFloatConst() == 1 && op1->RealType.Type == TYPE_Float) break;
      }
      op1->Emit(ec);
      op2->Emit(ec);
           if (op1->RealType.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_MulVarDrop, Loc);
      else if (op1->RealType.Type == TYPE_Byte && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_ByteMulVarDrop, Loc);
      else if (op1->RealType.Type == TYPE_Float && op2->Type.Type == TYPE_Float) ec.AddStatement(OPC_FMulVarDrop, Loc);
      else if (op1->RealType.Type == TYPE_Vector && op2->Type.Type == TYPE_Float) ec.AddStatement(OPC_VScaleVarDrop, Loc);
      else ParseError(Loc, "Expression type mismatch (`%s` and `%s`)", *op1->Type.GetName(), *op2->Type.GetName());
      break;

    case DivideAssign:
      // optimize `/= 1`
      if (op2->IsIntConst() && op2->GetIntConst() == 1) {
        if (op1->RealType.Type == TYPE_Int || op1->RealType.Type == TYPE_Byte) break;
      } else if (op2->IsFloatConst()) {
        // /1 for floats: it is safe, 'cause nan/1==nan, and [+-]inf/1 == [+-]inf
        if (op2->GetFloatConst() == 1 && op1->RealType.Type == TYPE_Float) break;
      }
      op1->Emit(ec);
      op2->Emit(ec);
           if (op1->RealType.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_DivVarDrop, Loc);
      else if (op1->RealType.Type == TYPE_Byte && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_ByteDivVarDrop, Loc);
      else if (op1->RealType.Type == TYPE_Float && op2->Type.Type == TYPE_Float) ec.AddStatement(OPC_FDivVarDrop, Loc);
      else if (op1->RealType.Type == TYPE_Vector && op2->Type.Type == TYPE_Float) ec.AddStatement(OPC_VIScaleVarDrop, Loc);
      else ParseError(Loc, "Expression type mismatch (`%s` and `%s`)", *op1->Type.GetName(), *op2->Type.GetName());
      break;

    case ModAssign:
      // optimize `%= 1`
      if (op2->IsIntConst() && op2->GetIntConst() == 1) {
        if (op1->RealType.Type == TYPE_Int || op1->RealType.Type == TYPE_Byte) {
          op1->Emit(ec);
          ec.EmitPushNumber(0, Loc);
          goto doassign;
        }
      }
      op1->Emit(ec);
      op2->Emit(ec);
           if (op1->RealType.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_ModVarDrop, Loc);
      else if (op1->RealType.Type == TYPE_Byte && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_ByteModVarDrop, Loc);
      else ParseError(Loc, "Expression type mismatch (`%s` and `%s`)", *op1->Type.GetName(), *op2->Type.GetName());
      break;

    case AndAssign:
      // optimize `&= 0`
      if (op2->IsIntConst() && op2->GetIntConst() == 0) {
        if (op1->RealType.Type == TYPE_Int || op1->RealType.Type == TYPE_Byte) {
          op1->Emit(ec);
          ec.EmitPushNumber(0, Loc);
          goto doassign;
        }
      }
      op1->Emit(ec);
      op2->Emit(ec);
           if (op1->RealType.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_AndVarDrop, Loc);
      else if (op1->RealType.Type == TYPE_Byte && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_ByteAndVarDrop, Loc);
      else ParseError(Loc, "Expression type mismatch (`%s` and `%s`)", *op1->Type.GetName(), *op2->Type.GetName());
      break;

    case OrAssign:
      // optimize `|= 0`
      if (op2->IsIntConst() && op2->GetIntConst() == 0) {
        if (op1->RealType.Type == TYPE_Int || op1->RealType.Type == TYPE_Byte) break;
      }
      op1->Emit(ec);
      op2->Emit(ec);
           if (op1->RealType.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_OrVarDrop, Loc);
      else if (op1->RealType.Type == TYPE_Byte && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_ByteOrVarDrop, Loc);
      //FIXME This is wrong!
      else if (op1->RealType.Type == TYPE_Bool && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_OrVarDrop, Loc);
      else ParseError(Loc, "Expression type mismatch (`%s` and `%s`)", *op1->Type.GetName(), *op2->Type.GetName());
      break;

    case XOrAssign:
      // optimize `^= 0`
      if (op2->IsIntConst() && op2->GetIntConst() == 0) {
        if (op1->RealType.Type == TYPE_Int || op1->RealType.Type == TYPE_Byte) break;
      }
      op1->Emit(ec);
      op2->Emit(ec);
           if (op1->RealType.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_XOrVarDrop, Loc);
      else if (op1->RealType.Type == TYPE_Byte && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_ByteXOrVarDrop, Loc);
      else ParseError(Loc, "Expression type mismatch (`%s` and `%s`)", *op1->Type.GetName(), *op2->Type.GetName());
      break;

    case LShiftAssign:
      // optimize `<<= 0`
      if (op2->IsIntConst() && op2->GetIntConst() == 0) {
        if (op1->RealType.Type == TYPE_Int || op1->RealType.Type == TYPE_Byte) break;
      }
      op1->Emit(ec);
      op2->Emit(ec);
           if (op1->RealType.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_LShiftVarDrop, Loc);
      else if (op1->RealType.Type == TYPE_Byte && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_ByteLShiftVarDrop, Loc);
      else ParseError(Loc, "Expression type mismatch (`%s` and `%s`)", *op1->Type.GetName(), *op2->Type.GetName());
      break;

    case RShiftAssign:
      // optimize `>>= 0`
      if (op2->IsIntConst() && op2->GetIntConst() == 0) {
        if (op1->RealType.Type == TYPE_Int || op1->RealType.Type == TYPE_Byte) break;
      }
      op1->Emit(ec);
      op2->Emit(ec);
           if (op1->RealType.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_RShiftVarDrop, Loc);
      else if (op1->RealType.Type == TYPE_Byte && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_ByteRShiftVarDrop, Loc);
      else ParseError(Loc, "Expression type mismatch (`%s` and `%s`)", *op1->Type.GetName(), *op2->Type.GetName());
      break;

    case URShiftAssign:
      // optimize `<<<= 0`
      if (op2->IsIntConst() && op2->GetIntConst() == 0) {
        if (op1->RealType.Type == TYPE_Int || op1->RealType.Type == TYPE_Byte) break;
      }
      op1->Emit(ec);
      op2->Emit(ec);
           if (op1->RealType.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_URShiftVarDrop, Loc);
      else if (op1->RealType.Type == TYPE_Byte && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_ByteRShiftVarDrop, Loc);
      else ParseError(Loc, "Expression type mismatch (`%s` and `%s`)", *op1->Type.GetName(), *op2->Type.GetName());
      break;

    case CatAssign:
      // optimize `~= ""`
      if (op2->IsStrConst() && op2->GetStrConst(ec.Package).length() == 0) {
        if (op1->RealType.Type == TYPE_String) break;
      }
      op1->Emit(ec);
      op2->Emit(ec);
           if (op1->RealType.Type == TYPE_String && op2->Type.Type == TYPE_String) ec.AddStatement(OPC_CatAssignVarDrop, Loc);
      else ParseError(Loc, "Expression type mismatch (`%s` and `%s`)", *op1->Type.GetName(), *op2->Type.GetName());
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
//  VAssignment::toString
//
//==========================================================================
VStr VAssignment::toString () const {
  VStr res = e2s(op1);
  switch (Oper) {
    case Assign: res += " = "; break;
    case AddAssign: res += " += "; break;
    case MinusAssign: res += " -= "; break;
    case MultiplyAssign: res += " *= "; break;
    case DivideAssign: res += " /= "; break;
    case ModAssign: res += " %= "; break;
    case AndAssign: res += " &= "; break;
    case OrAssign: res += " |= "; break;
    case XOrAssign: res += " ^= "; break;
    case LShiftAssign: res += " <<= "; break;
    case RShiftAssign: res += " >>= "; break;
    case URShiftAssign: res += " >>>= "; break;
    case CatAssign: res += " ~= "; break;
  }
  res += e2s(op2);
  return res;
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
