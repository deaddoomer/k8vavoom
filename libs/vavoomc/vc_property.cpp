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
//  VProperty::VProperty
//
//==========================================================================
VProperty::VProperty (VName AName, VMemberBase *AOuter, TLocation ALoc)
  : VMemberBase(MEMBER_Property, AName, AOuter, ALoc)
  , Type(TYPE_Void)
  , GetFunc(nullptr)
  , SetFunc(nullptr)
  , DefaultField(nullptr)
  , ReadField(nullptr)
  , WriteField(nullptr)
  , Flags(0)
  , TypeExpr(nullptr)
  , DefaultFieldName(NAME_None)
  , ReadFieldName(NAME_None)
  , WriteFieldName(NAME_None)
{
}


//==========================================================================
//
//  VProperty::~VProperty
//
//==========================================================================
VProperty::~VProperty () {
  delete TypeExpr; TypeExpr = nullptr;
}


//==========================================================================
//
//  VProperty::CompilerShutdown
//
//==========================================================================
void VProperty::CompilerShutdown () {
  VMemberBase::CompilerShutdown();
  delete TypeExpr; TypeExpr = nullptr;
}


//==========================================================================
//
//  VProperty::Define
//
//==========================================================================
bool VProperty::Define () {
  if (TypeExpr) {
    VEmitContext ec(this);
    TypeExpr = TypeExpr->ResolveAsType(ec);
  }
  if (!TypeExpr) return false;

  if (TypeExpr->Type.Type == TYPE_Void) {
    ParseError(TypeExpr->Loc, "Property cannot have `void` type");
    return false;
  }
  Type = TypeExpr->Type;

  if (DefaultFieldName != NAME_None) {
    DefaultField = ((VClass *)Outer)->FindField(DefaultFieldName, Loc, (VClass *)Outer);
    if (!DefaultField) {
      ParseError(Loc, "No such field `%s`", *DefaultFieldName);
      return false;
    }
  }

  if (ReadFieldName != NAME_None) {
    ReadField = ((VClass *)Outer)->FindField(ReadFieldName, Loc, (VClass *)Outer);
    if (!ReadField) {
      ParseError(Loc, "No such field `%s`", *ReadFieldName);
      return false;
    }
  }

  if (WriteFieldName != NAME_None) {
    WriteField = ((VClass *)Outer)->FindField(WriteFieldName, Loc, (VClass *)Outer);
    if (!WriteField) {
      ParseError(Loc, "No such field `%s`", *WriteFieldName);
      return false;
    }
  }

  VProperty *BaseProp = nullptr;
  if (((VClass *)Outer)->ParentClass) BaseProp = ((VClass *)Outer)->ParentClass->FindProperty(Name);
  if (BaseProp) {
    if (VMemberBase::optDeprecatedLaxOverride || VMemberBase::koraxCompatibility) Flags |= PROP_Override; // force `override`
    if ((Flags&PROP_Override) == 0) ParseError(Loc, "Overriding virtual property `%s` without `override` keyword", *GetFullName());
    if (BaseProp->Flags&PROP_Final) ParseError(Loc, "Property `%s` alaready has been declared `final` and cannot be overridden", *GetFullName());
         if (BaseProp->Flags&PROP_Private) ParseError(Loc, "Overriding private property `%s` is not allowed", *GetFullName());
    else if ((BaseProp->Flags&PROP_VisibilityMask) != (Flags&PROP_VisibilityMask)) ParseError(Loc, "Overriding property `%s` with different visibility is not allowed", *GetFullName());
    if (!Type.Equals(BaseProp->Type)) ParseError(Loc, "Property redeclared with a different type");
    // transfer decorate visibility
    Flags |= BaseProp->Flags&(PROP_Decorate);
  }

  return true;
}
