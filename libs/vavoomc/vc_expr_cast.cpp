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


//==========================================================================
//
//  VCastExpressionBase
//
//==========================================================================
VCastExpressionBase::VCastExpressionBase (VExpression *AOp) : VExpression(AOp->Loc), op(AOp) {}
VCastExpressionBase::VCastExpressionBase (const TLocation &ALoc) : VExpression(ALoc), op(nullptr) {}
VCastExpressionBase::~VCastExpressionBase () { if (op) { delete op; op = nullptr; } }


//==========================================================================
//
//  VCastExpressionBase::HasSideEffects
//
//==========================================================================
bool VCastExpressionBase::HasSideEffects () {
  return (op && op->HasSideEffects());
}


//==========================================================================
//
//  VCastExpressionBase::VisitChildren
//
//==========================================================================
void VCastExpressionBase::VisitChildren (VExprVisitor *v) {
  if (!v->stopIt && op) op->Visit(v);
}


//==========================================================================
//
//  VCastExpressionBase::DoSyntaxCopyTo
//
//==========================================================================
void VCastExpressionBase::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VCastExpressionBase *)e;
  res->op = (op ? op->SyntaxCopy() : nullptr);
}

//==========================================================================
//
//  VCastExpressionBase::DoResolve
//
//==========================================================================
VExpression *VCastExpressionBase::DoResolve (VEmitContext &ec) {
  if (op && !op->IsResolved()) {
    op = op->Resolve(ec);
    if (!op) { delete this; return nullptr; }
  }
  SetResolved();
  return this;
}


//==========================================================================
//
//  VDelegateToBool::VDelegateToBool
//
//==========================================================================
VDelegateToBool::VDelegateToBool (VExpression *AOp)
  : VCastExpressionBase(AOp)
{
  Type = TYPE_Int;
  if (op && op->IsResolved()) op->RequestAddressOf();
}


//==========================================================================
//
//  VDelegateToBool::SyntaxCopy
//
//==========================================================================
VExpression *VDelegateToBool::SyntaxCopy () {
  auto res = new VDelegateToBool();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDelegateToBool::Emit
//
//==========================================================================
VExpression *VDelegateToBool::DoResolve (VEmitContext &ec) {
  if (op && !op->IsResolved()) {
    op = op->Resolve(ec);
    if (op) op->RequestAddressOf();
  }
  if (!op) { delete this; return nullptr; }
  if (op->Type.Type != TYPE_Delegate) {
    ParseError(Loc, "cannot convert type `%s` to `bool`", *op->Type.GetName());
    delete this;
    return nullptr;
  }
  Type = TYPE_Int;
  SetResolved();
  return this;
}


//==========================================================================
//
//  VDelegateToBool::Emit
//
//==========================================================================
void VDelegateToBool::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  op->Emit(ec);
  ec.AddStatement(OPC_PushPointedPtr, Loc);
  ec.AddStatement(OPC_PtrToBool, Loc);
}


//==========================================================================
//
//  VDelegateToBool::toString
//
//==========================================================================
VStr VDelegateToBool::toString () const {
  return VStr("bool(")+e2s(op)+")";
}



//==========================================================================
//
//  VStringToBool::VStringToBool
//
//==========================================================================
VStringToBool::VStringToBool (VExpression *AOp)
  : VCastExpressionBase(AOp)
{
  Type = TYPE_Int;
}


//==========================================================================
//
//  VStringToBool::SyntaxCopy
//
//==========================================================================
VExpression *VStringToBool::SyntaxCopy () {
  auto res = new VStringToBool();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VStringToBool::DoResolve
//
//==========================================================================
VExpression *VStringToBool::DoResolve (VEmitContext &ec) {
  if (op && !op->IsResolved()) op = op->Resolve(ec);
  if (!op) { delete this; return nullptr; }
  if (op->Type.Type != TYPE_String) {
    ParseError(Loc, "cannot convert type `%s` to `bool`", *op->Type.GetName());
    delete this;
    return nullptr;
  }
  if (op->IsStrConst()) {
    // do it inplace
    VStr s = op->GetStrConst(ec.Package);
    VExpression *e = new VIntLiteral((s.length() ? 1 : 0), Loc);
    delete this;
    return e->Resolve(ec);
  }
  Type = TYPE_Int;
  SetResolved();
  return this;
}


//==========================================================================
//
//  VStringToBool::Emit
//
//==========================================================================
void VStringToBool::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  op->Emit(ec);
  ec.AddStatement(OPC_StrToBool, Loc);
}


//==========================================================================
//
//  VStringToBool::toString
//
//==========================================================================
VStr VStringToBool::toString () const {
  return VStr("bool(")+e2s(op)+")";
}



//==========================================================================
//
//  VNameToBool::VNameToBool
//
//==========================================================================
VNameToBool::VNameToBool (VExpression *AOp)
  : VCastExpressionBase(AOp)
{
  Type = TYPE_Int;
}


//==========================================================================
//
//  VNameToBool::SyntaxCopy
//
//==========================================================================
VExpression *VNameToBool::SyntaxCopy () {
  auto res = new VNameToBool();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VNameToBool::DoResolve
//
//==========================================================================
VExpression *VNameToBool::DoResolve (VEmitContext &ec) {
  if (op && !op->IsResolved()) op = op->Resolve(ec);
  if (!op) { delete this; return nullptr; }
  if (op->Type.Type != TYPE_Name) {
    ParseError(Loc, "cannot convert type `%s` to `bool`", *op->Type.GetName());
    delete this;
    return nullptr;
  }
  if (op->IsNameConst()) {
    // do it inplace
    VName n = op->GetNameConst();
    VExpression *e = new VIntLiteral((n != NAME_None ? 1 : 0), Loc);
    delete this;
    return e->Resolve(ec);
  }
  Type = TYPE_Int;
  SetResolved();
  return this;
}


//==========================================================================
//
//  VNameToBool::Emit
//
//==========================================================================
void VNameToBool::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  op->Emit(ec);
  // no further conversion required
}


//==========================================================================
//
//  VNameToBool::toString
//
//==========================================================================
VStr VNameToBool::toString () const {
  return VStr("bool(")+e2s(op)+")";
}



//==========================================================================
//
//  VFloatToBool::VFloatToBool
//
//==========================================================================
VFloatToBool::VFloatToBool (VExpression *AOp)
  : VCastExpressionBase(AOp)
{
  Type = TYPE_Int;
}


//==========================================================================
//
//  VFloatToBool::SyntaxCopy
//
//==========================================================================
VExpression *VFloatToBool::SyntaxCopy () {
  auto res = new VFloatToBool();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VFloatToBool::DoResolve
//
//==========================================================================
VExpression *VFloatToBool::DoResolve (VEmitContext &ec) {
  if (op && !op->IsResolved()) op = op->Resolve(ec);
  if (!op) { delete this; return nullptr; }
  if (op->Type.Type != TYPE_Float) {
    ParseError(Loc, "cannot convert type `%s` to `bool`", *op->Type.GetName());
    delete this;
    return nullptr;
  }
  if (op->IsFloatConst()) {
    // do it inplace
    VExpression *e = new VIntLiteral((isZeroInfNaNF(op->GetFloatConst()) ? 0 : 1), Loc); // so inf/nan yields `false`
    delete this;
    return e->Resolve(ec);
  }
  Type = TYPE_Int;
  SetResolved();
  return this;
}


//==========================================================================
//
//  VFloatToBool::Emit
//
//==========================================================================
void VFloatToBool::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  op->Emit(ec);
  ec.AddStatement(OPC_FloatToBool, Loc);
}


//==========================================================================
//
//  VFloatToBool::toString
//
//==========================================================================
VStr VFloatToBool::toString () const {
  return VStr("bool(")+e2s(op)+")";
}



//==========================================================================
//
//  VVectorToBool::VVectorToBool
//
//==========================================================================
VVectorToBool::VVectorToBool (VExpression *AOp)
  : VCastExpressionBase(AOp)
{
  Type = TYPE_Int;
}


//==========================================================================
//
//  VVectorToBool::SyntaxCopy
//
//==========================================================================
VExpression *VVectorToBool::SyntaxCopy () {
  auto res = new VVectorToBool();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VVectorToBool::DoResolve
//
//==========================================================================
VExpression *VVectorToBool::DoResolve (VEmitContext &ec) {
  if (op && !op->IsResolved()) op = op->Resolve(ec);
  if (!op) { delete this; return nullptr; }
  if (op->Type.Type != TYPE_Vector) {
    ParseError(Loc, "cannot convert type `%s` to `bool`", *op->Type.GetName());
    delete this;
    return nullptr;
  }
  if (op->IsConstVectorCtor()) {
    // do it inplace
    TVec v = ((VVectorExpr *)op)->GetConstValue();
    VExpression *e = new VIntLiteral((v.toBool() ? 1 : 0), Loc); // so inf/nan yields `false`
    delete this;
    return e->Resolve(ec);
  }
  Type = TYPE_Int;
  SetResolved();
  return this;
}


//==========================================================================
//
//  VVectorToBool::Emit
//
//==========================================================================
void VVectorToBool::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  op->Emit(ec);
  ec.AddStatement(OPC_VectorToBool, Loc);
}


//==========================================================================
//
//  VVectorToBool::toString
//
//==========================================================================
VStr VVectorToBool::toString () const {
  return VStr("bool(")+e2s(op)+")";
}



//==========================================================================
//
//  VPointerToBool::VPointerToBool
//
//==========================================================================
VPointerToBool::VPointerToBool (VExpression *AOp)
  : VCastExpressionBase(AOp)
{
  Type = TYPE_Int;
}


//==========================================================================
//
//  VPointerToBool::SyntaxCopy
//
//==========================================================================
VExpression *VPointerToBool::SyntaxCopy () {
  auto res = new VPointerToBool();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VPointerToBool::DoResolve
//
//==========================================================================
VExpression *VPointerToBool::DoResolve (VEmitContext &ec) {
  if (op && !op->IsResolved()) op = op->Resolve(ec);
  if (!op) { delete this; return nullptr; }
  // do it in-place for pointers
  switch (op->Type.Type) {
    case TYPE_Pointer:
      if (op->IsNullLiteral()) {
        VExpression *e = new VIntLiteral(0, Loc);
        delete this;
        return e->Resolve(ec);
      }
      break;
    case TYPE_Reference:
    case TYPE_Class:
    case TYPE_State:
      if (op->IsNoneLiteral()) {
        VExpression *e = new VIntLiteral(0, Loc);
        delete this;
        return e->Resolve(ec);
      }
      break;
    default:
      ParseError(Loc, "cannot convert type `%s` to `bool`", *op->Type.GetName());
      delete this;
      return nullptr;
  }
  Type = TYPE_Int;
  SetResolved();
  return this;
}


//==========================================================================
//
//  VPointerToBool::Emit
//
//==========================================================================
void VPointerToBool::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  op->Emit(ec);
  if (op->Type.Type == TYPE_State) {
    ec.AddStatement(OPC_StateToBool, Loc);
  } else {
    ec.AddStatement(OPC_PtrToBool, Loc);
  }
}


//==========================================================================
//
//  VPointerToBool::toString
//
//==========================================================================
VStr VPointerToBool::toString () const {
  return VStr("bool(")+e2s(op)+")";
}



//==========================================================================
//
//  VScalarToFloat::VScalarToFloat
//
//==========================================================================
VScalarToFloat::VScalarToFloat (VExpression *AOp, bool AImplicit)
  : VCastExpressionBase(AOp)
  , implicit(AImplicit)
{
  Type = TYPE_Float;
}


//==========================================================================
//
//  VScalarToFloat::DoSyntaxCopyTo
//
//==========================================================================
void VScalarToFloat::DoSyntaxCopyTo (VExpression *e) {
  VCastExpressionBase::DoSyntaxCopyTo(e);
  auto res = (VScalarToFloat *)e;
  res->implicit = implicit;
}


//==========================================================================
//
//  VScalarToFloat::SyntaxCopy
//
//==========================================================================
VExpression *VScalarToFloat::SyntaxCopy () {
  auto res = new VScalarToFloat();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VScalarToFloat::DoResolve
//
//==========================================================================
VExpression *VScalarToFloat::DoResolve (VEmitContext &ec) {
  if (op && !op->IsResolved()) op = op->Resolve(ec);
  if (!op) { delete this; return nullptr; }
  switch (op->Type.Type) {
    case TYPE_Int:
    case TYPE_Byte:
    case TYPE_Bool:
      if (op->IsIntConst()) {
        const float fval = (float)op->GetIntConst();
        if (!VMemberBase::optDeprecatedLaxConversions && !VIsGoodIntAsFloat(op->GetIntConst())) {
          ParseError(Loc, "cannot convert type `%s` to `float`", *op->Type.GetName());
        }
        VExpression *e = new VFloatLiteral(fval, op->Loc);
        delete this;
        return e->Resolve(ec);
      } else {
        if (implicit && !VMemberBase::optDeprecatedLaxConversions) {
          ParseError(Loc, "cannot implicitly convert type `%s` to `float`", *op->Type.GetName());
        }
      }
      break;
    case TYPE_Float:
      if (op->IsFloatConst()) {
        VExpression *e = op;
        op = nullptr;
        delete this;
        vassert(e->IsResolved());
        return e;
      }
      break;
    default:
      ParseError(Loc, "cannot convert type `%s` to `float`", *op->Type.GetName());
      delete this;
      return nullptr;
  }
  SetResolved();
  return this;
}


//==========================================================================
//
//  VScalarToFloat::Emit
//
//==========================================================================
void VScalarToFloat::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  op->Emit(ec);
  switch (op->Type.Type) {
    case TYPE_Int:
    case TYPE_Byte:
    case TYPE_Bool:
      ec.AddStatement(OPC_IntToFloat, Loc);
      break;
    case TYPE_Float: // nothing to do
      break;
    default:
      ParseError(Loc, "Internal compiler error (VScalarToFloat::Emit)");
  }
}


//==========================================================================
//
//  VScalarToFloat::toString
//
//==========================================================================
VStr VScalarToFloat::toString () const {
  return VStr("float(")+e2s(op)+")";
}



//==========================================================================
//
//  VScalarToInt::VScalarToInt
//
//==========================================================================
VScalarToInt::VScalarToInt (VExpression *AOp, bool AImplicit)
  : VCastExpressionBase(AOp)
  , implicit(AImplicit)
{
  Type = TYPE_Int;
}


//==========================================================================
//
//  VScalarToInt::DoSyntaxCopyTo
//
//==========================================================================
void VScalarToInt::DoSyntaxCopyTo (VExpression *e) {
  VCastExpressionBase::DoSyntaxCopyTo(e);
  auto res = (VScalarToInt *)e;
  res->implicit = implicit;
}


//==========================================================================
//
//  VScalarToInt::SyntaxCopy
//
//==========================================================================
VExpression *VScalarToInt::SyntaxCopy () {
  auto res = new VScalarToInt();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VScalarToInt::DoResolve
//
//==========================================================================
VExpression *VScalarToInt::DoResolve (VEmitContext &ec) {
  if (op && !op->IsResolved()) op = op->Resolve(ec);
  if (!op) { delete this; return nullptr; }
  switch (op->Type.Type) {
    case TYPE_Int:
    case TYPE_Byte:
    case TYPE_Bool:
      if (op->IsIntConst()) {
        VExpression *e = op;
        op = nullptr;
        delete this;
        vassert(e->IsResolved());
        return e;
      }
      break;
    case TYPE_Float:
      if (op->IsFloatConst()) {
        const int ival = (vint32)op->GetFloatConst();
        if (!VMemberBase::optDeprecatedLaxConversions && !VIsGoodFloatAsInt(op->GetFloatConst())) {
          ParseError(Loc, "cannot convert type `%s` to `int`", *op->Type.GetName());
        }
        VExpression *e = new VIntLiteral(ival, op->Loc);
        delete this;
        return e->Resolve(ec);
      } else {
        if (implicit && !VMemberBase::optDeprecatedLaxConversions) {
          ParseError(Loc, "cannot implicitly convert type `%s` to `int`", *op->Type.GetName());
        }
      }
      break;
    default:
      ParseError(Loc, "cannot convert type `%s` to `int`", *op->Type.GetName());
      delete this;
      return nullptr;
  }
  SetResolved();
  return this;
}


//==========================================================================
//
//  VScalarToInt::Emit
//
//==========================================================================
void VScalarToInt::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  op->Emit(ec);
  switch (op->Type.Type) {
    case TYPE_Int:
    case TYPE_Byte:
    case TYPE_Bool:
      // nothing to do
      break;
    case TYPE_Float:
      ec.AddStatement(OPC_FloatToInt, Loc);
      break;
    default:
      ParseError(Loc, "Internal compiler error (VScalarToInt::Emit)");
      return;
  }
}


//==========================================================================
//
//  VScalarToInt::toString
//
//==========================================================================
VStr VScalarToInt::toString () const {
  return VStr("int(")+e2s(op)+")";
}



//==========================================================================
//
//  VCastToString::VCastToString
//
//==========================================================================
VCastToString::VCastToString (VExpression *AOp)
  : VCastExpressionBase(AOp)
{
  Type = TYPE_String;
}


//==========================================================================
//
//  VCastToString::SyntaxCopy
//
//==========================================================================
VExpression *VCastToString::SyntaxCopy () {
  auto res = new VCastToString();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VCastToString::DoResolve
//
//==========================================================================
VExpression *VCastToString::DoResolve (VEmitContext &ec) {
  if (op && !op->IsResolved()) op = op->Resolve(ec);
  if (!op) { delete this; return nullptr; }
  switch (op->Type.Type) {
    case TYPE_String:
      break;
    case TYPE_Name:
      if (op->IsNameConst()) {
        // do it inplace
        VStr ns = VStr(op->GetNameConst());
        int val = ec.Package->FindString(*ns);
        VExpression *e = new VStringLiteral(ns, val, Loc);
        if (e) e = e->Resolve(ec);
        delete this;
        return e;
      }
      break;
    default:
      ParseError(Loc, "cannot convert type `%s` to `string`", *op->Type.GetName());
      delete this;
      return nullptr;
  }
  SetResolved();
  return this;
}


//==========================================================================
//
//  VCastToString::Emit
//
//==========================================================================
void VCastToString::Emit (VEmitContext &ec) {
  if (!op) return;
  EmitCheckResolved(ec);
  op->Emit(ec);
  switch (op->Type.Type) {
    case TYPE_String:
      break;
    case TYPE_Name:
      ec.AddStatement(OPC_NameToStr, Loc);
      break;
    default:
      ParseError(Loc, "cannot convert type `%s` to `string` (the thing that should not be)", *op->Type.GetName());
  }
}


//==========================================================================
//
//  VCastToString::toString
//
//==========================================================================
VStr VCastToString::toString () const {
  return VStr("string(")+e2s(op)+")";
}



//==========================================================================
//
//  VCastToName::VCastToName
//
//==========================================================================
VCastToName::VCastToName (VExpression *AOp)
  : VCastExpressionBase(AOp)
{
  Type = TYPE_Name;
}


//==========================================================================
//
//  VCastToName::SyntaxCopy
//
//==========================================================================
VExpression *VCastToName::SyntaxCopy () {
  auto res = new VCastToName();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VCastToName::DoResolve
//
//==========================================================================
VExpression *VCastToName::DoResolve (VEmitContext &ec) {
  if (op && !op->IsResolved()) op = op->Resolve(ec);
  if (!op) { delete this; return nullptr; }
  switch (op->Type.Type) {
    case TYPE_String:
      if (op->IsStrConst()) {
        // do it inplace
        VStr s = op->GetStrConst(ec.Package);
        VExpression *e = new VNameLiteral(VName(*s), Loc);
        if (e) e = e->Resolve(ec);
        delete this;
        return e;
      }
      break;
    case TYPE_Name:
      break;
    default:
      ParseError(Loc, "cannot convert type `%s` to `name`", *op->Type.GetName());
      delete this;
      return nullptr;
  }
  SetResolved();
  return this;
}


//==========================================================================
//
//  VCastToName::Emit
//
//==========================================================================
void VCastToName::Emit (VEmitContext &ec) {
  if (!op) return;
  EmitCheckResolved(ec);
  op->Emit(ec);
  switch (op->Type.Type) {
    case TYPE_String:
      ec.AddStatement(OPC_StrToName, Loc);
      break;
    case TYPE_Name:
      break;
    default:
      ParseError(Loc, "cannot convert type `%s` to `name` (the thing that should not be)", *op->Type.GetName());
  }
}


//==========================================================================
//
//  VCastToName::toString
//
//==========================================================================
VStr VCastToName::toString () const {
  return VStr("name(")+e2s(op)+")";
}



//==========================================================================
//
//  VDynamicCast::VDynamicCast
//
//==========================================================================
VDynamicCast::VDynamicCast (VClass *AClass, VExpression *AOp, const TLocation &ALoc)
  : VCastExpressionBase(ALoc)
  , Class(AClass)
{
  op = AOp;
}


//==========================================================================
//
//  VDynamicCast::SyntaxCopy
//
//==========================================================================
VExpression *VDynamicCast::SyntaxCopy () {
  auto res = new VDynamicCast();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDynamicCast::DoSyntaxCopyTo
//
//==========================================================================
void VDynamicCast::DoSyntaxCopyTo (VExpression *e) {
  VCastExpressionBase::DoSyntaxCopyTo(e);
  auto res = (VDynamicCast *)e;
  res->Class = Class;
}


//==========================================================================
//
//  VDynamicCast::DoResolve
//
//==========================================================================
VExpression *VDynamicCast::DoResolve (VEmitContext &ec) {
  if (op && !op->IsResolved()) op = op->Resolve(ec);
  if (!op) { delete this; return nullptr; }
  if (op->Type.Type != TYPE_Reference) {
    ParseError(Loc, "Bad expression, class reference required");
    delete this;
    return nullptr;
  }
  Type = VFieldType(Class);
  SetResolved();
  return this;
}


//==========================================================================
//
//  VDynamicCast::Emit
//
//==========================================================================
void VDynamicCast::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  op->Emit(ec);
  ec.AddStatement(OPC_DynamicCast, Class, Loc);
}


//==========================================================================
//
//  VDynamicCast::toString
//
//==========================================================================
VStr VDynamicCast::toString () const {
  return (Class ? VStr(Class->Name) : e2s(nullptr))+"("+e2s(op)+")";
}



//==========================================================================
//
//  VDynamicClassCast::VDynamicClassCast
//
//==========================================================================
VDynamicClassCast::VDynamicClassCast (VName AClassName, VExpression *AOp, const TLocation &ALoc)
  : VCastExpressionBase(ALoc)
  , ClassName(AClassName)
{
  op = AOp;
}


//==========================================================================
//
//  VDynamicClassCast::SyntaxCopy
//
//==========================================================================
VExpression *VDynamicClassCast::SyntaxCopy () {
  auto res = new VDynamicClassCast();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDynamicCast::DoSyntaxCopyTo
//
//==========================================================================
void VDynamicClassCast::DoSyntaxCopyTo (VExpression *e) {
  VCastExpressionBase::DoSyntaxCopyTo(e);
  auto res = (VDynamicClassCast *)e;
  res->ClassName = ClassName;
}


//==========================================================================
//
//  VDynamicClassCast::DoResolve
//
//==========================================================================
VExpression *VDynamicClassCast::DoResolve (VEmitContext &ec) {
  if (op && !op->IsResolved()) op = op->Resolve(ec);
  if (!op) { delete this; return nullptr; }

  if (op->Type.Type != TYPE_Class) {
    ParseError(Loc, "Bad expression, class type required");
    delete this;
    return nullptr;
  }

  Type = TYPE_Class;
  Type.Class = VMemberBase::StaticFindClass(ClassName);
  if (!Type.Class) {
    ParseError(Loc, "No such class `%s`", *ClassName);
    delete this;
    return nullptr;
  }

  SetResolved();
  return this;
}


//==========================================================================
//
//  VDynamicClassCast::Emit
//
//==========================================================================
void VDynamicClassCast::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  op->Emit(ec);
  ec.AddStatement(OPC_DynamicClassCast, Type.Class, Loc);
}


//==========================================================================
//
//  VDynamicClassCast::toString
//
//==========================================================================
VStr VDynamicClassCast::toString () const {
  return VStr(ClassName)+"("+e2s(op)+")";
}



//==========================================================================
//
//  VStructPtrCast::VStructPtrCast
//
//==========================================================================
VStructPtrCast::VStructPtrCast (VExpression *aop, VExpression *adest, const TLocation &aloc)
  : VCastExpressionBase(aloc)
  , dest(adest)
{
  op = aop;
}


//==========================================================================
//
//  VStructPtrCast::~VStructPtrCast
//
//==========================================================================
VStructPtrCast::~VStructPtrCast () {
  delete dest; dest = nullptr;
}


//==========================================================================
//
//  VStructPtrCast::VisitChildren
//
//==========================================================================
void VStructPtrCast::VisitChildren (VExprVisitor *v) {
  if (!v->stopIt && dest) dest->Visit(v);
}


//==========================================================================
//
//  VStructPtrCast::SyntaxCopy
//
//==========================================================================
VExpression *VStructPtrCast::SyntaxCopy () {
  auto res = new VStructPtrCast();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VStructPtrCast::DoSyntaxCopyTo
//
//==========================================================================
void VStructPtrCast::DoSyntaxCopyTo (VExpression *e) {
  VCastExpressionBase::DoSyntaxCopyTo(e);
  auto res = (VStructPtrCast *)e;
  res->dest = (dest ? dest->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VStructPtrCast::DoResolve
//
//==========================================================================
VExpression *VStructPtrCast::DoResolve (VEmitContext &ec) {
  if (op && !op->IsResolved()) op = op->Resolve(ec);
  if (dest) dest = dest->ResolveAsType(ec);
  if (!op || !dest) { delete this; return nullptr; }
  if (op->Type.Type != TYPE_Pointer || dest->Type.Type != TYPE_Pointer) {
    ParseError(Loc, "Casts are supported only for pointers yet");
    delete this;
    return nullptr;
  }
  //FIXME: this is unsafe!
  Type = dest->Type;
  SetResolved();
  return this;
}


//==========================================================================
//
//  VStructPtrCast::Emit
//
//==========================================================================
void VStructPtrCast::Emit (VEmitContext &ec) {
  if (op) {
    EmitCheckResolved(ec);
    op->Emit(ec);
  }
}


//==========================================================================
//
//  VStructPtrCast::toString
//
//==========================================================================
VStr VStructPtrCast::toString () const {
  return VStr("cast(")+e2s(dest)+"("+e2s(op)+")";
}



//==========================================================================
//
//  VDynArrayToBool::VDynArrayToBool
//
//==========================================================================
VDynArrayToBool::VDynArrayToBool (VExpression *AOp)
  : VCastExpressionBase(AOp)
{
  Type = TYPE_Int;
}


//==========================================================================
//
//  VDynArrayToBool::SyntaxCopy
//
//==========================================================================
VExpression *VDynArrayToBool::SyntaxCopy () {
  auto res = new VDynArrayToBool();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDynArrayToBool::DoResolve
//
//==========================================================================
VExpression *VDynArrayToBool::DoResolve (VEmitContext &ec) {
  if (op && !op->IsResolved()) op = op->Resolve(ec);
  if (!op) { delete this; return nullptr; }
  if (op->Type.Type != TYPE_DynamicArray) {
    ParseError(Loc, "cannot convert type `%s` to `bool`", *op->Type.GetName());
    delete this;
    return nullptr;
  }
  op->RequestAddressOf();
  Type = TYPE_Int;
  SetResolved();
  return this;
}


//==========================================================================
//
//  VDynArrayToBool::toString
//
//==========================================================================
VStr VDynArrayToBool::toString () const {
  return VStr("bool(")+e2s(op)+")";
}



//==========================================================================
//
//  VDictToBool::VDictToBool
//
//==========================================================================
VDictToBool::VDictToBool (VExpression *AOp)
  : VCastExpressionBase(AOp)
{
  Type = TYPE_Int;
}


//==========================================================================
//
//  VDictToBool::SyntaxCopy
//
//==========================================================================
VExpression *VDictToBool::SyntaxCopy () {
  auto res = new VDictToBool();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDictToBool::DoResolve
//
//==========================================================================
VExpression *VDictToBool::DoResolve (VEmitContext &ec) {
  if (op && !op->IsResolved()) op = op->Resolve(ec);
  if (!op) { delete this; return nullptr; }
  if (op->Type.Type != TYPE_Dictionary) {
    ParseError(Loc, "cannot convert type `%s` to `bool`", *op->Type.GetName());
    delete this;
    return nullptr;
  }
  op->RequestAddressOf();
  Type = TYPE_Int;
  SetResolved();
  return this;
}


//==========================================================================
//
//  VDictToBool::toString
//
//==========================================================================
VStr VDictToBool::toString () const {
  return VStr("bool(")+e2s(op)+")";
}


//==========================================================================
//
//  VSliceToBool::VSliceToBool
//
//==========================================================================
VSliceToBool::VSliceToBool (VExpression *AOp)
  : VCastExpressionBase(AOp)
{
  Type = TYPE_Int;
}


//==========================================================================
//
//  VSliceToBool::SyntaxCopy
//
//==========================================================================
VExpression *VSliceToBool::SyntaxCopy () {
  auto res = new VSliceToBool();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VSliceToBool::DoResolve
//
//==========================================================================
VExpression *VSliceToBool::DoResolve (VEmitContext &ec) {
  if (op && !op->IsResolved()) op = op->Resolve(ec);
  if (!op) { delete this; return nullptr; }
  if (op->Type.Type != TYPE_SliceArray) {
    ParseError(Loc, "cannot convert type `%s` to `bool`", *op->Type.GetName());
    delete this;
    return nullptr;
  }
  op->RequestAddressOf();
  Type = TYPE_Int;
  SetResolved();
  return this;
}


//==========================================================================
//
//  VSliceToBool::Emit
//
//==========================================================================
void VSliceToBool::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  op->Emit(ec);
  ec.AddStatement(OPC_SliceToBool, Loc);
}


//==========================================================================
//
//  VSliceToBool::toString
//
//==========================================================================
VStr VSliceToBool::toString () const {
  return VStr("bool(")+e2s(op)+")";
}
