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
//  VLiteralBase::VLiteralBase
//
//==========================================================================
VLiteralBase::VLiteralBase (const TLocation &ALoc)
  : VExpression(ALoc)
{
}


//==========================================================================
//
//  VLiteralBase::HasSideEffects
//
//==========================================================================
bool VLiteralBase::HasSideEffects () {
  return false;
}


//==========================================================================
//
//  VLiteralBase::VisitChildren
//
//==========================================================================
void VLiteralBase::VisitChildren (VExprVisitor *v) {
}


//==========================================================================
//
//  VIntLiteral::VIntLiteral
//
//==========================================================================
VIntLiteral::VIntLiteral (vint32 AValue, const TLocation &ALoc)
  : VLiteralBase(ALoc)
  , Value(AValue)
{
  Type = TYPE_Int;
}


//==========================================================================
//
//  VIntLiteral::SyntaxCopy
//
//==========================================================================
VExpression *VIntLiteral::SyntaxCopy () {
  auto res = new VIntLiteral();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VIntLiteral::DoSyntaxCopyTo
//
//==========================================================================
void VIntLiteral::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VIntLiteral *)e;
  res->Value = Value;
}


//==========================================================================
//
//  VIntLiteral::DoResolve
//
//==========================================================================
VExpression *VIntLiteral::DoResolve (VEmitContext &) {
  SetResolved();
  return this;
}


//==========================================================================
//
//  VIntLiteral::Emit
//
//==========================================================================
void VIntLiteral::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  ec.EmitPushNumber(Value, Loc);
}


//==========================================================================
//
//  VIntLiteral::GetIntConst
//
//==========================================================================
vint32 VIntLiteral::GetIntConst () const {
  return Value;
}


//==========================================================================
//
//  VIntLiteral::IsIntConst
//
//==========================================================================
bool VIntLiteral::IsIntConst () const {
  return true;
}


//==========================================================================
//
//  VIntLiteral::toString
//
//==========================================================================
VStr VIntLiteral::toString () const {
  return VStr(Value);
}



//==========================================================================
//
//  VFloatLiteral::VFloatLiteral
//
//==========================================================================
VFloatLiteral::VFloatLiteral (float AValue, const TLocation &ALoc)
  : VLiteralBase(ALoc)
  , Value(AValue)
{
  Type = TYPE_Float;
}


//==========================================================================
//
//  VFloatLiteral::SyntaxCopy
//
//==========================================================================
VExpression *VFloatLiteral::SyntaxCopy () {
  auto res = new VFloatLiteral();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VFloatLiteral::DoSyntaxCopyTo
//
//==========================================================================
void VFloatLiteral::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VFloatLiteral *)e;
  res->Value = Value;
}


//==========================================================================
//
//  VFloatLiteral::DoResolve
//
//==========================================================================
VExpression *VFloatLiteral::DoResolve (VEmitContext &) {
  SetResolved();
  return this;
}


//==========================================================================
//
//  VFloatLiteral::Emit
//
//==========================================================================
void VFloatLiteral::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  ec.AddStatement(OPC_PushNumber, Value, Loc);
}


//==========================================================================
//
//  VFloatLiteral::IsFloatConst
//
//==========================================================================
bool VFloatLiteral::IsFloatConst () const {
  return true;
}


//==========================================================================
//
//  VFloatLiteral::GetFloatConst
//
//==========================================================================
float VFloatLiteral::GetFloatConst () const {
  return Value;
}


//==========================================================================
//
//  VFloatLiteral::toString
//
//==========================================================================
VStr VFloatLiteral::toString () const {
  return VStr(Value);
}



//==========================================================================
//
//  VNameLiteral::VNameLiteral
//
//==========================================================================
VNameLiteral::VNameLiteral (VName AValue, const TLocation &ALoc)
  : VLiteralBase(ALoc)
  , Value(AValue)
{
  Type = TYPE_Name;
}


//==========================================================================
//
//  VNameLiteral::SyntaxCopy
//
//==========================================================================
VExpression *VNameLiteral::SyntaxCopy () {
  auto res = new VNameLiteral();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VNameLiteral::DoSyntaxCopyTo
//
//==========================================================================
void VNameLiteral::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VNameLiteral *)e;
  res->Value = Value;
}


//==========================================================================
//
//  VNameLiteral::IsNameConst
//
//==========================================================================
bool VNameLiteral::IsNameConst () const {
  return true;
}


//==========================================================================
//
//  VNameLiteral::GetNameConst
//
//==========================================================================
VName VNameLiteral::GetNameConst () const {
  return Value;
}


//==========================================================================
//
//  VNameLiteral::DoResolve
//
//==========================================================================
VExpression *VNameLiteral::DoResolve (VEmitContext &) {
  SetResolved();
  return this;
}


//==========================================================================
//
//  VNameLiteral::Emit
//
//==========================================================================
void VNameLiteral::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  ec.AddStatement(OPC_PushName, Value, Loc);
}


//==========================================================================
//
//  VNameLiteral::toString
//
//==========================================================================
VStr VNameLiteral::toString () const {
  return VStr("'")+VStr(Value)+"'";
}



//==========================================================================
//
//  VClassNameLiteral::VClassNameLiteral
//
//==========================================================================
VClassNameLiteral::VClassNameLiteral (VName AValue, const TLocation &ALoc)
  : VNameLiteral(AValue, ALoc)
{
}


//==========================================================================
//
//  VClassNameLiteral::SyntaxCopy
//
//==========================================================================
VExpression *VClassNameLiteral::SyntaxCopy () {
  auto res = new VClassNameLiteral();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VClassNameLiteral::IsClassNameConst
//
//==========================================================================
bool VClassNameLiteral::IsClassNameConst () const {
  return true;
}


//==========================================================================
//
//  VClassNameLiteral::DoResolve
//
//==========================================================================
VExpression *VClassNameLiteral::DoResolve (VEmitContext &ec) {
  VClass *Class = VMemberBase::StaticFindClass(Value);
  if (Class) {
    VExpression *e = new VClassConstant(Class, Loc);
    delete this;
    return e->Resolve(ec);
  }

  Class = ec.Package->FindDecorateImportClass(Value);
  if (Class) {
    VExpression *e = new VClassConstant(Class, Loc);
    delete this;
    return e->Resolve(ec);
  }

  ParseError(Loc, "Unknown class name `%s`", *Value);
  delete this;
  return nullptr;
}


//==========================================================================
//
//  VClassNameLiteral::Emit
//
//==========================================================================
void VClassNameLiteral::Emit (VEmitContext &/*ec*/) {
  VCFatalError("VClassNameLiteral::Emit: the thing that should not be!");
}


//==========================================================================
//
//  VClassNameLiteral::toString
//
//==========================================================================
VStr VClassNameLiteral::toString () const {
  return VStr("class!")+VStr(Value);
}



//==========================================================================
//
//  VStringLiteral::VStringLiteral
//
//==========================================================================
VStringLiteral::VStringLiteral (VStr asval, vint32 AValue, const TLocation &ALoc)
  : VLiteralBase(ALoc)
  , Value(AValue)
  , strval(asval)
{
  Type = TYPE_String;
}


//==========================================================================
//
//  VStringLiteral::SyntaxCopy
//
//==========================================================================
VExpression *VStringLiteral::SyntaxCopy () {
  auto res = new VStringLiteral();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VStringLiteral::DoSyntaxCopyTo
//
//==========================================================================
void VStringLiteral::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VStringLiteral *)e;
  res->Value = Value;
  res->strval = strval;
}


//==========================================================================
//
//  VStringLiteral::DoResolve
//
//==========================================================================
VExpression *VStringLiteral::DoResolve (VEmitContext &) {
  SetResolved();
  return this;
}


//==========================================================================
//
//  VStringLiteral::Emit
//
//==========================================================================
void VStringLiteral::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  ec.AddStatement(OPC_PushString, Value, Loc);
}


//==========================================================================
//
//  VStringLiteral::IsStrConst
//
//==========================================================================
bool VStringLiteral::IsStrConst () const {
  return true;
}


//==========================================================================
//
//  VStringLiteral::GetStrConst
//
// WARNING! returns by reference!
//
//==========================================================================
const VStr &VStringLiteral::GetStrConst (VPackage *Pkg) const {
  return Pkg->GetStringByIndex(Value);
}


//==========================================================================
//
//  VStringLiteral::toString
//
//==========================================================================
VStr VStringLiteral::toString () const {
  return VStr("\"")+strval.quote()+"\"";
}



//==========================================================================
//
//  VSelf::VSelf
//
//==========================================================================
VSelf::VSelf (const TLocation &ALoc) : VLiteralBase(ALoc) {
}


//==========================================================================
//
//  VSelf::SyntaxCopy
//
//==========================================================================
VExpression *VSelf::SyntaxCopy () {
  auto res = new VSelf();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VSelf::DoSyntaxCopyTo
//
//==========================================================================
void VSelf::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
}


//==========================================================================
//
//  VSelf::DoResolve
//
//==========================================================================
VExpression *VSelf::DoResolve (VEmitContext &ec) {
  if (!ec.SelfClass && !ec.SelfStruct) {
    ParseError(Loc, "`self` is used outside of member function");
    delete this;
    return nullptr;
  }
  if (ec.CurrentFunc->Flags&FUNC_Static) {
    ParseError(Loc, "`self` is used in a static method");
    delete this;
    return nullptr;
  }
  if (ec.SelfStruct) {
    Type = VFieldType(ec.SelfStruct).MakePointerType(); // it is always a pointer
  } else {
    Type = VFieldType(ec.SelfClass);
  }
  SetResolved();
  return this;
}


//==========================================================================
//
//  VSelf::Emit
//
//==========================================================================
void VSelf::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  ec.AddStatement(OPC_LocalValue0, Loc);
}


//==========================================================================
//
//  VSelf::IsSelfLiteral
//
//==========================================================================
bool VSelf::IsSelfLiteral () const {
  return true;
}


//==========================================================================
//
//  VSelf::toString
//
//==========================================================================
VStr VSelf::toString () const {
  return VStr("self");
}



//==========================================================================
//
//  VSelfClass::VSelfClass
//
//==========================================================================
VSelfClass::VSelfClass (const TLocation &ALoc) : VLiteralBase(ALoc) {
}


//==========================================================================
//
//  VSelfClass::SyntaxCopy
//
//==========================================================================
VExpression *VSelfClass::SyntaxCopy () {
  auto res = new VSelfClass();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VSelfClass::DoSyntaxCopyTo
//
//==========================================================================
void VSelfClass::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
}


//==========================================================================
//
//  VSelfClass::DoResolve
//
//==========================================================================
VExpression *VSelfClass::DoResolve (VEmitContext &ec) {
  if (!ec.SelfClass) {
    ParseError(Loc, "self used outside of member function");
    delete this;
    return nullptr;
  }
  Type = VFieldType(ec.SelfClass);
  if (ec.CurrentFunc->Flags&FUNC_Static) Type.Type = TYPE_Class;
  SetResolved();
  return this;
}


//==========================================================================
//
//  VSelfClass::Emit
//
//==========================================================================
void VSelfClass::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  if (ec.CurrentFunc->Flags&FUNC_Static) {
    ec.AddStatement(OPC_PushClassId, ec.SelfClass, Loc);
  } else {
    ec.AddStatement(OPC_LocalValue0, Loc);
  }
}


//==========================================================================
//
//  VSelfClass::IsSelfClassLiteral
//
//==========================================================================
bool VSelfClass::IsSelfClassLiteral () const {
  return true;
}


//==========================================================================
//
//  VSelfClass::toString
//
//==========================================================================
VStr VSelfClass::toString () const {
  return VStr("(self.Class)");
}



//==========================================================================
//
//  VNoneLiteral::VNoneLiteral
//
//==========================================================================
VNoneLiteral::VNoneLiteral (const TLocation &ALoc) : VLiteralBase(ALoc) {
  Type = VFieldType((VClass *)nullptr);
}


//==========================================================================
//
//  VNoneLiteral::SyntaxCopy
//
//==========================================================================
VExpression *VNoneLiteral::SyntaxCopy () {
  auto res = new VNoneLiteral();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VNoneLiteral::DoSyntaxCopyTo
//
//==========================================================================
void VNoneLiteral::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
}


//==========================================================================
//
//  VNoneLiteral::DoResolve
//
//==========================================================================
VExpression *VNoneLiteral::DoResolve (VEmitContext &) {
  SetResolved();
  return this;
}


//==========================================================================
//
//  VNoneLiteral::Emit
//
//==========================================================================
void VNoneLiteral::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  ec.AddStatement(OPC_PushNull, Loc);
}


//==========================================================================
//
//  VNoneLiteral::IsNoneLiteral
//
//==========================================================================
bool VNoneLiteral::IsNoneLiteral () const {
  return true;
}


//==========================================================================
//
//  VNoneLiteral::toString
//
//==========================================================================
VStr VNoneLiteral::toString () const {
  return VStr("none");
}



//==========================================================================
//
//  VNoneDelegateLiteral::VNoneDelegateLiteral
//
//==========================================================================
VNoneDelegateLiteral::VNoneDelegateLiteral (const TLocation &ALoc) : VLiteralBase(ALoc) {
  Type = VFieldType(TYPE_Delegate);
}


//==========================================================================
//
//  VNoneDelegateLiteral::SyntaxCopy
//
//==========================================================================
VExpression *VNoneDelegateLiteral::SyntaxCopy () {
  auto res = new VNoneDelegateLiteral();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VNoneDelegateLiteral::DoSyntaxCopyTo
//
//==========================================================================
void VNoneDelegateLiteral::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
}


//==========================================================================
//
//  VNoneDelegateLiteral::DoResolve
//
//==========================================================================
VExpression *VNoneDelegateLiteral::DoResolve (VEmitContext &) {
  SetResolved();
  return this;
}


//==========================================================================
//
//  VNoneDelegateLiteral::Emit
//
//==========================================================================
void VNoneDelegateLiteral::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  ec.AddStatement(OPC_PushNull, Loc);
  ec.AddStatement(OPC_PushNull, Loc);
}


//==========================================================================
//
//  VNoneDelegateLiteral::IsNoneDelegateLiteral
//
//==========================================================================
bool VNoneDelegateLiteral::IsNoneDelegateLiteral () const {
  return true;
}


//==========================================================================
//
//  VNoneDelegateLiteral::toString
//
//==========================================================================
VStr VNoneDelegateLiteral::toString () const {
  return VStr("nonedg");
}



//==========================================================================
//
//  VNullLiteral::VNullLiteral
//
//==========================================================================
VNullLiteral::VNullLiteral (const TLocation &ALoc) : VLiteralBase(ALoc) {
  Type = VFieldType(TYPE_Void).MakePointerType();
}


//==========================================================================
//
//  VNullLiteral::SyntaxCopy
//
//==========================================================================
VExpression *VNullLiteral::SyntaxCopy () {
  auto res = new VNullLiteral();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VNullLiteral::DoSyntaxCopyTo
//
//==========================================================================
void VNullLiteral::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
}


//==========================================================================
//
//  VNullLiteral::DoResolve
//
//==========================================================================
VExpression *VNullLiteral::DoResolve (VEmitContext &) {
  SetResolved();
  return this;
}


//==========================================================================
//
//  VNullLiteral::Emit
//
//==========================================================================
void VNullLiteral::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  ec.AddStatement(OPC_PushNull, Loc);
}


//==========================================================================
//
//  VNullLiteral::IsNullLiteral
//
//==========================================================================
bool VNullLiteral::IsNullLiteral () const {
  return true;
}


//==========================================================================
//
//  VNullLiteral::toString
//
//==========================================================================
VStr VNullLiteral::toString () const {
  return VStr("nullptr");
}



//==========================================================================
//
//  VDollar::VDollar
//
//==========================================================================
VDollar::VDollar (const TLocation &ALoc) : VLiteralBase(ALoc) {
}


//==========================================================================
//
//  VDollar::SyntaxCopy
//
//==========================================================================
VExpression *VDollar::SyntaxCopy () {
  auto res = new VDollar();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDollar::DoSyntaxCopyTo
//
//==========================================================================
void VDollar::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
}


//==========================================================================
//
//  VDollar::DoResolve
//
//==========================================================================
VExpression *VDollar::DoResolve (VEmitContext &ec) {
  if (!ec.IndArray) {
    ParseError(Loc, "`$` used outside of array/string indexing");
    delete this;
    return nullptr;
  }
  VExpression *e;
  if (ec.IndArray->Is2D()) {
    if (ec.IndArray->resolvingInd2) {
      e = new VDotField(ec.IndArray->GetOpSyntaxCopy(), VName("length2"), Loc);
    } else {
      e = new VDotField(ec.IndArray->GetOpSyntaxCopy(), VName("length1"), Loc);
    }
  } else {
    e = new VDotField(ec.IndArray->GetOpSyntaxCopy(), VName("length"), Loc);
  }
  delete this;
  return e->Resolve(ec);
}


//==========================================================================
//
//  VDollar::Emit
//
//==========================================================================
void VDollar::Emit (VEmitContext &/*ec*/) {
  ParseError(Loc, "the thing that should not be");
}


//==========================================================================
//
//  VDollar::toString
//
//==========================================================================
VStr VDollar::toString () const {
  return VStr("$");
}
