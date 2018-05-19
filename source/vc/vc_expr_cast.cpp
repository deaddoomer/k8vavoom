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

// HEADER FILES ------------------------------------------------------------

#include "vc_local.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
//  VDelegateToBool::VDelegateToBool
//
//==========================================================================

VDelegateToBool::VDelegateToBool(VExpression* AOp)
: VExpression(AOp->Loc)
, op(AOp)
{
  Type = TYPE_Int;
  op->RequestAddressOf();
}

//==========================================================================
//
//  VDelegateToBool::~VDelegateToBool
//
//==========================================================================

VDelegateToBool::~VDelegateToBool()
{
  if (op)
  {
    delete op;
    op = nullptr;
  }
}

//==========================================================================
//
//  VDelegateToBool::DoResolve
//
//==========================================================================

VExpression* VDelegateToBool::DoResolve(VEmitContext&)
{
  return this;
}

//==========================================================================
//
//  VDelegateToBool::Emit
//
//==========================================================================

void VDelegateToBool::Emit(VEmitContext& ec)
{
  op->Emit(ec);
  ec.AddStatement(OPC_PushPointedPtr);
  ec.AddStatement(OPC_PtrToBool);
}

//==========================================================================
//
//  VStringToBool::VStringToBool
//
//==========================================================================

VStringToBool::VStringToBool(VExpression* AOp)
: VExpression(AOp->Loc)
, op(AOp)
{
  Type = TYPE_Int;
}

//==========================================================================
//
//  VStringToBool::~VStringToBool
//
//==========================================================================

VStringToBool::~VStringToBool()
{
  if (op)
  {
    delete op;
    op = nullptr;
  }
}

//==========================================================================
//
//  VStringToBool::DoResolve
//
//==========================================================================

VExpression* VStringToBool::DoResolve(VEmitContext&)
{
  return this;
}

//==========================================================================
//
//  VStringToBool::Emit
//
//==========================================================================

void VStringToBool::Emit(VEmitContext& ec)
{
  op->Emit(ec);
  ec.AddStatement(OPC_StrToBool);
}

//==========================================================================
//
//  VPointerToBool::VPointerToBool
//
//==========================================================================

VPointerToBool::VPointerToBool(VExpression* AOp)
: VExpression(AOp->Loc)
, op(AOp)
{
  Type = TYPE_Int;
}

//==========================================================================
//
//  VPointerToBool::~VPointerToBool
//
//==========================================================================

VPointerToBool::~VPointerToBool()
{
  if (op)
  {
    delete op;
    op = nullptr;
  }
}

//==========================================================================
//
//  VPointerToBool::DoResolve
//
//==========================================================================

VExpression* VPointerToBool::DoResolve(VEmitContext&)
{
  return this;
}

//==========================================================================
//
//  VPointerToBool::Emit
//
//==========================================================================

void VPointerToBool::Emit(VEmitContext& ec)
{
  op->Emit(ec);
  ec.AddStatement(OPC_PtrToBool);
}


//==========================================================================
//
//  VScalarToFloat::VScalarToFloat
//
//==========================================================================
VScalarToFloat::VScalarToFloat (VExpression *AOp)
  : VExpression(AOp->Loc)
  , op(AOp)
{
  // convert it in-place
  if (op && op->IsIntConst()) {
    //printf("*** IN-PLACE CONVERSION OF %d\n", op->GetIntConst());
    VExpression *lit = new VFloatLiteral((float)op->GetIntConst(), op->Loc);
    delete op;
    op = lit; // op is resolved here, but literal resolves to itself, so it is ok
  }
  Type = TYPE_Float;
}


//==========================================================================
//
//  VScalarToFloat::~VScalarToFloat
//
//==========================================================================
VScalarToFloat::~VScalarToFloat () {
  if (op) {
    delete op;
    op = nullptr;
  }
}


//==========================================================================
//
//  VScalarToFloat::DoResolve
//
//==========================================================================
VExpression *VScalarToFloat::DoResolve (VEmitContext& ec) {
  //printf("VScalarToFloat::DoResolve!\n");
  if (!op) return nullptr;
  op = op->Resolve(ec);
  if (!op) { delete this; return nullptr; }
  switch (op->Type.Type) {
    case TYPE_Int:
    case TYPE_Byte:
    case TYPE_Bool:
      if (op->IsIntConst()) {
        VExpression *lit = new VFloatLiteral((float)op->GetIntConst(), op->Loc);
        delete op;
        op = lit->Resolve(ec); // just in case
        if (!op) { delete this; return nullptr; }
      }
      break;
    case TYPE_Float:
      break;
    default:
      ParseError(Loc, "cannot convert type to `float`");
      delete this;
      return nullptr;
  }
  return this;
}


//==========================================================================
//
//  VScalarToFloat::Emit
//
//==========================================================================
void VScalarToFloat::Emit (VEmitContext& ec) {
  op->Emit(ec);
  switch (op->Type.Type) {
    case TYPE_Int:
    case TYPE_Byte:
    case TYPE_Bool:
      ec.AddStatement(OPC_IntToFloat);
      break;
    case TYPE_Float: // nothing to do
      break;
    default:
      ParseError(Loc, "Internal compiler error (VScalarToFloat::Emit)");
  }
}


//==========================================================================
//
//  VScalarToInt::VScalarToInt
//
//==========================================================================
VScalarToInt::VScalarToInt (VExpression *AOp)
  : VExpression(AOp->Loc)
  , op(AOp)
{
  // convert it in-place
  if (op && op->IsFloatConst()) {
    VExpression *lit = new VIntLiteral((vint32)op->GetFloatConst(), op->Loc);
    delete op;
    op = lit; // op is resolved here, but literal resolves to itself, so it is ok
  }
  Type = TYPE_Int;
}


//==========================================================================
//
//  VScalarToInt::~VScalarToInt
//
//==========================================================================
VScalarToInt::~VScalarToInt () {
  if (op) {
    delete op;
    op = nullptr;
  }
}


//==========================================================================
//
//  VScalarToInt::DoResolve
//
//==========================================================================
VExpression *VScalarToInt::DoResolve (VEmitContext& ec) {
  //printf("VScalarToInt::DoResolve!\n");
  if (!op) return nullptr;
  op = op->Resolve(ec);
  if (!op) { delete this; return nullptr; }
  switch (op->Type.Type) {
    case TYPE_Int:
    case TYPE_Byte:
    case TYPE_Bool:
      break;
    case TYPE_Float:
      if (op->IsFloatConst()) {
        VExpression *lit = new VIntLiteral((vint32)op->GetFloatConst(), op->Loc);
        delete op;
        op = lit->Resolve(ec); // just in case
        if (!op) { delete this; return nullptr; }
      }
      break;
    default:
      ParseError(Loc, "cannot convert type to `int`");
      delete this;
      return nullptr;
  }
  return this;
}


//==========================================================================
//
//  VScalarToInt::Emit
//
//==========================================================================
void VScalarToInt::Emit (VEmitContext& ec) {
  op->Emit(ec);
  switch (op->Type.Type) {
    case TYPE_Int:
    case TYPE_Byte:
    case TYPE_Bool:
      // nothing to do
      break;
    case TYPE_Float:
      ec.AddStatement(OPC_FloatToInt);
      break;
    default:
      ParseError(Loc, "Internal compiler error (VScalarToInt::Emit)");
      return;
  }
}


//==========================================================================
//
//  VDynamicCast::VDynamicCast
//
//==========================================================================

VDynamicCast::VDynamicCast(VClass* AClass, VExpression* AOp, const TLocation& ALoc)
: VExpression(ALoc)
, Class(AClass)
, op(AOp)
{
}

//==========================================================================
//
//  VDynamicCast::~VDynamicCast
//
//==========================================================================

VDynamicCast::~VDynamicCast()
{
  if (op)
  {
    delete op;
    op = nullptr;
  }
}

//==========================================================================
//
//  VDynamicCast::DoResolve
//
//==========================================================================

VExpression* VDynamicCast::DoResolve(VEmitContext& ec)
{
  if (op)
    op = op->Resolve(ec);
  if (!op)
  {
    delete this;
    return nullptr;
  }

  if (op->Type.Type != TYPE_Reference)
  {
    ParseError(Loc, "Bad expression, class reference required");
    delete this;
    return nullptr;
  }
  Type = VFieldType(Class);
  return this;
}

//==========================================================================
//
//  VDynamicCast::Emit
//
//==========================================================================

void VDynamicCast::Emit(VEmitContext& ec)
{
  op->Emit(ec);
  ec.AddStatement(OPC_DynamicCast, Class);
}

//==========================================================================
//
//  VDynamicClassCast::VDynamicClassCast
//
//==========================================================================

VDynamicClassCast::VDynamicClassCast(VName AClassName, VExpression* AOp,
  const TLocation& ALoc)
: VExpression(ALoc)
, ClassName(AClassName)
, op(AOp)
{
}

//==========================================================================
//
//  VDynamicClassCast::~VDynamicClassCast
//
//==========================================================================

VDynamicClassCast::~VDynamicClassCast()
{
  if (op)
  {
    delete op;
    op = nullptr;
  }
}

//==========================================================================
//
//  VDynamicClassCast::DoResolve
//
//==========================================================================

VExpression* VDynamicClassCast::DoResolve(VEmitContext& ec)
{
  if (op)
    op = op->Resolve(ec);
  if (!op)
  {
    delete this;
    return nullptr;
  }

  if (op->Type.Type != TYPE_Class)
  {
    ParseError(Loc, "Bad expression, class type required");
    delete this;
    return nullptr;
  }

  Type = TYPE_Class;
  Type.Class = VMemberBase::StaticFindClass(ClassName);
  if (!Type.Class)
  {
    ParseError(Loc, "No such class %s", *ClassName);
    delete this;
    return nullptr;
  }
  return this;
}

//==========================================================================
//
//  VDynamicClassCast::Emit
//
//==========================================================================

void VDynamicClassCast::Emit(VEmitContext& ec)
{
  op->Emit(ec);
  ec.AddStatement(OPC_DynamicClassCast, Type.Class);
}


//==========================================================================
//
//  VCastToString::VCastToString
//
//==========================================================================
VCastToString::VCastToString (VExpression *AOp)
  : VExpression(AOp->Loc)
  , op(AOp)
{
  Type = TYPE_String;
}


//==========================================================================
//
//  VCastToString::~VCastToString
//
//==========================================================================
VCastToString::~VCastToString () {
  if (op) {
    delete op;
    op = nullptr;
  }
}


//==========================================================================
//
//  VCastToString::DoResolve
//
//==========================================================================
VExpression *VCastToString::DoResolve (VEmitContext& ec) {
  if (!op) return nullptr;

  if (op->Type.Type != TYPE_String) {
    //TODO: convert it in-place
    VExpression *TmpArgs[1];
    TmpArgs[0] = op;
    op = new VInvocation(nullptr, ec.SelfClass->FindMethodChecked("NameToStr"), nullptr, false, false, Loc, 1, TmpArgs); // no self, not base call
  }

  op = op->Resolve(ec);
  if (!op) { delete this; return nullptr; }

  if (op->Type.Type != TYPE_String) {
    ParseError(Loc, "cannot convert type `%s` to `string`", *op->Type.GetName());
    delete this;
    return nullptr;
  }

  return this;
}


//==========================================================================
//
//  VCastToString::Emit
//
//==========================================================================
void VCastToString::Emit (VEmitContext& ec) {
  if (!op) return;
  op->Emit(ec);
  if (op->Type.Type != TYPE_String) ParseError(Loc, "cannot convert type to `string`");
}


//==========================================================================
//
//  VCastToName::VCastToName
//
//==========================================================================
VCastToName::VCastToName (VExpression *AOp)
  : VExpression(AOp->Loc)
  , op(AOp)
{
  Type = TYPE_Name;
}


//==========================================================================
//
//  VCastToName::~VCastToName
//
//==========================================================================
VCastToName::~VCastToName () {
  if (op) {
    delete op;
    op = nullptr;
  }
}


//==========================================================================
//
//  VCastToName::DoResolve
//
//==========================================================================
VExpression *VCastToName::DoResolve (VEmitContext& ec) {
  if (!op) return nullptr;

  if (op->Type.Type != TYPE_Name) {
    //TODO: convert it in-place
    VExpression *TmpArgs[1];
    TmpArgs[0] = op;
    op = new VInvocation(nullptr, ec.SelfClass->FindMethodChecked("StrToName"), nullptr, false, false, Loc, 1, TmpArgs); // no self, not base call
  }

  op = op->Resolve(ec);
  if (!op) { delete this; return nullptr; }

  if (op->Type.Type != TYPE_Name) {
    ParseError(Loc, "cannot convert type `%s` to `name`", *op->Type.GetName());
    delete this;
    return nullptr;
  }

  return this;
}


//==========================================================================
//
//  VCastToName::Emit
//
//==========================================================================
void VCastToName::Emit (VEmitContext& ec) {
  if (!op) return;
  op->Emit(ec);
  if (op->Type.Type != TYPE_Name) ParseError(Loc, "cannot convert type to `name`");
}
