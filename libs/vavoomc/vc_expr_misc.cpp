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
//  VVectorExpr::VVectorExpr
//
//==========================================================================
VVectorExpr::VVectorExpr (VExpression *AOp1, VExpression *AOp2, VExpression *AOp3, const TLocation &ALoc)
  : VExpression(ALoc)
  , op1(AOp1)
  , op2(AOp2)
  , op3(AOp3)
{
  if (!op1) ParseError(Loc, "Expression expected");
  if (!op2) ParseError(Loc, "Expression expected");
  if (!op3) ParseError(Loc, "Expression expected");
}


//==========================================================================
//
//  VVectorExpr::VVectorExpr
//
//==========================================================================
VVectorExpr::VVectorExpr (const TVec &vv, const TLocation &aloc)
  : VExpression(aloc)
{
  op1 = new VFloatLiteral(vv.x, aloc);
  op2 = new VFloatLiteral(vv.y, aloc);
  op3 = new VFloatLiteral(vv.z, aloc);
}


//==========================================================================
//
//  VVectorExpr::~VVectorExpr
//
//==========================================================================
VVectorExpr::~VVectorExpr () {
  if (op1) { delete op1; op1 = nullptr; }
  if (op2) { delete op2; op2 = nullptr; }
  if (op3) { delete op3; op3 = nullptr; }
}


//==========================================================================
//
//  VVectorExpr::SyntaxCopy
//
//==========================================================================
VExpression *VVectorExpr::SyntaxCopy () {
  auto res = new VVectorExpr();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VVectorExpr::DoSyntaxCopyTo
//
//==========================================================================
void VVectorExpr::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VVectorExpr *)e;
  res->op1 = (op1 ? op1->SyntaxCopy() : nullptr);
  res->op2 = (op2 ? op2->SyntaxCopy() : nullptr);
  res->op3 = (op3 ? op3->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VVectorExpr::DoResolve
//
//==========================================================================
VExpression *VVectorExpr::DoResolve (VEmitContext &ec) {
  if (op1) op1 = op1->ResolveFloat(ec);
  if (op2) op2 = op2->ResolveFloat(ec);
  if (op3) op3 = op3->ResolveFloat(ec);
  if (!op1 || !op2 || !op3) {
    delete this;
    return nullptr;
  }

  if (op1->Type.Type != TYPE_Float) {
    ParseError(Loc, "Expression type mismatch, vector param 1 is not a float");
    delete this;
    return nullptr;
  }

  if (op2->Type.Type != TYPE_Float) {
    ParseError(Loc, "Expression type mismatch, vector param 2 is not a float");
    delete this;
    return nullptr;
  }

  if (op3->Type.Type != TYPE_Float) {
    ParseError(Loc, "Expression type mismatch, vector param 3 is not a float");
    delete this;
    return nullptr;
  }

  Type = TYPE_Vector;
  // no, this will break things like: `TAVec a = vector();`
  //Type.Struct = VMemberBase::StaticFindTVec();
  SetResolved();
  return this;
}


//==========================================================================
//
//  VVectorExpr::Emit
//
//==========================================================================
void VVectorExpr::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  if (op1) op1->Emit(ec);
  if (op1) op2->Emit(ec);
  if (op1) op3->Emit(ec);
}


//==========================================================================
//
//  VVectorExpr::IsVectorCtor
//
//==========================================================================
bool VVectorExpr::IsVectorCtor () const {
  return true;
}


//==========================================================================
//
//  VVectorExpr::IsConstVectorCtor
//
//==========================================================================
bool VVectorExpr::IsConstVectorCtor () const {
  return IsConst();
}


//==========================================================================
//
//  VVectorExpr::IsConst
//
//==========================================================================
// is this a const ctor? (should be called after resolving)
bool VVectorExpr::IsConst () const {
  if (!op1 || !op2 || !op3) return false;
  return
    (op1->IsFloatConst() || op1->IsIntConst()) &&
    (op2->IsFloatConst() || op2->IsIntConst()) &&
    (op3->IsFloatConst() || op3->IsIntConst());
}


//==========================================================================
//
//  VVectorExpr::GetConstValue
//
//==========================================================================
TVec VVectorExpr::GetConstValue () const {
  TVec res(0, 0, 0);
  if (!IsConst()) {
    ParseError(Loc, "Cannot get const value from non-const vector");
    return res;
  }
  res.x = (op1->IsFloatConst() ? op1->GetFloatConst() : (float)op1->GetIntConst());
  res.y = (op2->IsFloatConst() ? op2->GetFloatConst() : (float)op2->GetIntConst());
  res.z = (op3->IsFloatConst() ? op3->GetFloatConst() : (float)op3->GetIntConst());
  return res;
}


//==========================================================================
//
//  VVectorExpr::toString
//
//==========================================================================
VStr VVectorExpr::toString () const {
  return VStr("vector(")+e2s(op1)+","+e2s(op2)+","+e2s(op3)+")";
}



//==========================================================================
//
//  VSingleName::VSingleName
//
//==========================================================================
VSingleName::VSingleName (VName AName, const TLocation &ALoc, bool aBangSpec)
  : VExpression(ALoc)
  , Name(AName)
  , bIsBangSpecified(aBangSpec)
{
}


//==========================================================================
//
//  VSingleName::IsSingleName
//
//==========================================================================
bool VSingleName::IsSingleName () const { return true; }


//==========================================================================
//
//  VSingleName::SyntaxCopy
//
//==========================================================================
VExpression *VSingleName::SyntaxCopy () {
  auto res = new VSingleName();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VSingleName::DoSyntaxCopyTo
//
//==========================================================================
void VSingleName::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VSingleName *)e;
  res->Name = Name;
  res->bIsBangSpecified = bIsBangSpecified;
}


//==========================================================================
//
//  VSingleName::InternalResolve
//
//==========================================================================
VExpression *VSingleName::InternalResolve (VEmitContext &ec, VSingleName::AssType assType) {
  int num = ec.CheckForLocalVar(Name);
  if (num != -1) {
    VExpression *e = new VLocalVar(num, ec.IsLocalUnsafe(num), Loc);
    delete this;
    return e->Resolve(ec);
  }

  // check for `!specified`, but no `specified_xxx` local
  if (bIsBangSpecified && VStr::startsWith(*Name, "specified_")) {
    if (assType == AssType::AssTarget) {
      ParseError(Loc, "cannot set `!specified` for non-optional argument `%s`", *Name+10);
      delete this;
      return nullptr;
    }
    // always true
    VExpression *e = new VIntLiteral(1, Loc);
    delete this;
    return e->Resolve(ec);
  }

  // resolve struct field or method
  if (ec.SelfStruct) {
    // field
    VField *field = ec.SelfStruct->FindField(Name);
    if (field) {
      const bool isRO = (ec.CurrentFunc && (ec.CurrentFunc->Flags&FUNC_ConstSelf));
      if (assType == AssType::AssTarget && isRO) {
        ParseError(Loc, "`self` is read-only (const method `%s`)", *ec.CurrentFunc->GetFullName());
        delete this;
        return nullptr;
      }
      VExpression *e = new VSelf(Loc);
      if (e) e = e->Resolve(ec);
      if (e) e = new VFieldAccess(e, field, Loc, (isRO ? FIELD_ReadOnly : 0));
      delete this;
      return (e ? e->Resolve(ec) : nullptr);
    }
    // method
    VMethod *M = ec.SelfStruct->FindAccessibleMethod(Name, ec.SelfStruct, &Loc);
    if (M) {
      if (M->IsIterator()) {
        ParseError(Loc, "Iterator methods can only be used in foreach statements");
        delete this;
        return nullptr;
      }
      // rewrite as invoke
      VExpression *e;
      if (M->IsStatic()) {
        e = new VInvocation(nullptr, M, nullptr, false, false, Loc, 0, nullptr);
      } else {
        //e = new VInvocation((new VSelf(Loc))->Resolve(ec), M, nullptr, true, false, Loc, 0, nullptr);
        if (ec.CurrentFunc && (ec.CurrentFunc->Flags&FUNC_ConstSelf) != (M->Flags&FUNC_ConstSelf)) {
          ParseError(Loc, "`self` is read-only (const method `%s`), but invocation method is not", *ec.CurrentFunc->GetFullName());
          delete this;
          return nullptr;
        }
        e = new VDotInvocation(new VSelf(Loc), Name, Loc, 0, nullptr);
      }
      delete this;
      return e->Resolve(ec);
    }
  }

  // class field/property/etc
  if (ec.SelfClass) {
    VConstant *Const = ec.SelfClass->FindConstant(Name);
    if (Const) {
      VExpression *e = new VConstantValue(Const, Loc);
      delete this;
      return e->Resolve(ec);
    }

    // fields first
    VField *field = ec.SelfClass->FindField(Name, Loc, ec.SelfClass);
    if (field) {
      //if (VStr(*Name).startsWith("user_")) fprintf(stderr, "***ROUTED TO FIELD: '%s' (class:%s)\n", *Name, ec.SelfClass->GetName());
      const bool isRO = (ec.CurrentFunc && (ec.CurrentFunc->Flags&FUNC_ConstSelf));
      VExpression *e;
      // "normal" access: call delegate (if it is operand-less)
      /*if (assType == AssType::Normal && field->Type.Type == TYPE_Delegate && field->Func && field->Func->NumParams == 0) {
        e = new VInvocation(nullptr, field->Func, field, false, false, Loc, 0, nullptr);
      } else*/ {
        e = new VSelf(Loc);
        if (e) e = e->Resolve(ec);
        if (e) e = new VFieldAccess(e, field, Loc, (isRO ? FIELD_ReadOnly : 0));
      }
      delete this;
      return (e ? e->Resolve(ec) : nullptr);
    }

    // built-in object properties
    if (assType == Normal && (Name == "isDestroyed" || Name == "IsDestroyed")) {
      VExpression *e = new VObjectPropGetIsDestroyed(new VSelf(Loc), Loc);
      delete this;
      return e->Resolve(ec);
    }

    VProperty *Prop = ec.SelfClass->FindProperty(Name);
    if (Prop) {
      if (assType == AssType::AssTarget) {
        if (ec.InDefaultProperties) {
          if (!Prop->DefaultField) {
            ParseError(Loc, "Property `%s` has no default field set", *Name);
            delete this;
            return nullptr;
          }
          VExpression *e = new VSelf(Loc);
          if (e) e = e->Resolve(ec);
          if (e) e = new VFieldAccess(e, Prop->DefaultField, Loc, 0);
          delete this;
          return (e ? e->Resolve(ec) : nullptr);
          //return e->ResolveAssignmentTarget(ec); //k8:??? why not this?
        } else {
          if (Prop->SetFunc) {
            VExpression *e;
            if ((Prop->SetFunc->Flags&FUNC_Static) != 0) {
              e = new VPropertyAssign(nullptr, Prop->SetFunc, false, Loc);
            } else {
              e = new VSelf(Loc);
              if (e) e = e->Resolve(ec);
              if (e) e = new VPropertyAssign(e, Prop->SetFunc, true, Loc);
            }
            delete this;
            // assignment will call resolve
            return e;
          } else if (Prop->WriteField) {
            VExpression *e;
            if (ec.SelfClass && ec.CurrentFunc && (ec.CurrentFunc->Flags&FUNC_Static) != 0) {
              //e = new VFieldAccess(new VClassConstant(ec.SelfClass, Loc), Prop->WriteField, Loc, 0);
              ParseError(Loc, "Static fields are not supported yet");
              delete this;
              return nullptr;
            } else {
              e = new VSelf(Loc);
              if (e) e = e->Resolve(ec);
              if (e) e = new VFieldAccess(e, Prop->WriteField, Loc, 0);
            }
            delete this;
            return (e ? e->ResolveAssignmentTarget(ec) : nullptr);
          } else {
            ParseError(Loc, "Property `%s` cannot be set", *Name);
            delete this;
            return nullptr;
          }
        }
      } else {
        if (Prop->GetFunc) {
          VExpression *e;
          if ((Prop->GetFunc->Flags&FUNC_Static) != 0) {
            e = new VInvocation(nullptr, Prop->GetFunc, nullptr, false, false, Loc, 0, nullptr);
          } else {
            e = new VSelf(Loc);
            if (e) e = e->Resolve(ec);
            if (e) e = new VInvocation(e, Prop->GetFunc, nullptr, true, false, Loc, 0, nullptr);
          }
          delete this;
          return (e ? e->Resolve(ec) : nullptr);
        } else if (Prop->ReadField) {
          VExpression *e;
          if (ec.SelfClass && ec.CurrentFunc && (ec.CurrentFunc->Flags&FUNC_Static) != 0) {
            //e = new VFieldAccess(new VClassConstant(ec.SelfClass, Loc), Prop->ReadField, Loc, 0);
            ParseError(Loc, "Static fields are not supported yet");
            delete this;
            return nullptr;
          } else {
            e = new VSelf(Loc);
            if (e) e = e->Resolve(ec);
            if (e) e = new VFieldAccess(e, Prop->ReadField, Loc, 0);
          }
          delete this;
          return (e ? e->Resolve(ec) : nullptr);
        } else {
          ParseError(Loc, "Property `%s` cannot be read", *Name);
          delete this;
          return nullptr;
        }
      }
    }

    VMethod *M = ec.SelfClass->FindAccessibleMethod(Name, ec.SelfClass, &Loc);
    if (M) {
      if (M->Flags&FUNC_Iterator) {
        ParseError(Loc, "Iterator methods can only be used in foreach statements");
        delete this;
        return nullptr;
      }
      // rewrite as invoke
      VExpression *e;
      if ((M->Flags&FUNC_Static) != 0) {
        e = new VInvocation(nullptr, M, nullptr, false, false, Loc, 0, nullptr);
      } else {
        //e = new VInvocation((new VSelf(Loc))->Resolve(ec), M, nullptr, true, false, Loc, 0, nullptr);
        e = new VDotInvocation(new VSelf(Loc), Name, Loc, 0, nullptr);
      }
      delete this;
      return e->Resolve(ec);
    }
  }

  VConstant *Const = ec.Package->FindConstant(Name);
  if (Const) {
    VExpression *e = new VConstantValue(Const, Loc);
    delete this;
    return e->Resolve(ec);
  }

  VClass *Class = VMemberBase::StaticFindClass(Name);
  if (Class) {
    VExpression *e = new VClassConstant(Class, Loc);
    delete this;
    return e->Resolve(ec);
  }

  Class = ec.Package->FindDecorateImportClass(Name);
  if (Class) {
    VExpression *e = new VClassConstant(Class, Loc);
    delete this;
    return e->Resolve(ec);
  }

  Const = (VConstant *)VMemberBase::StaticFindMember(Name, ANY_PACKAGE, MEMBER_Const);
  if (Const) {
    VExpression *e = new VConstantValue(Const, Loc);
    delete this;
    return e->Resolve(ec);
  }

  if (assType == Normal && ec.SelfClass && (Name == "write" || Name == "writeln")) {
    VExpression *e = new VInvokeWrite((Name == "writeln"), Loc, 0, nullptr);
    delete this;
    return e->Resolve(ec);
  }

  if (assType == AssType::Normal) return ResolveAsType(ec);

  ParseError(Loc, "Illegal expression identifier `%s`", *Name);
  delete this;
  return nullptr;
}


//==========================================================================
//
//  VSingleName::DoResolve
//
//==========================================================================
VExpression *VSingleName::DoResolve (VEmitContext &ec) {
  return InternalResolve(ec, AssType::Normal);
}


//==========================================================================
//
//  VSingleName::ResolveAssignmentTarget
//
//==========================================================================
VExpression *VSingleName::ResolveAssignmentTarget (VEmitContext &ec) {
  return InternalResolve(ec, AssType::AssTarget);
}


//==========================================================================
//
//  VSingleName::ResolveAssignmentValue
//
//==========================================================================
VExpression *VSingleName::ResolveAssignmentValue (VEmitContext &ec) {
  return InternalResolve(ec, AssType::AssValue);
}


//==========================================================================
//
//  VSingleName::ResolveAsType
//
//==========================================================================
VTypeExpr *VSingleName::ResolveAsType (VEmitContext &ec) {
  // this does enum types too
  Type = VMemberBase::StaticFindType(ec.SelfClass, Name);

  if (Type.Type == TYPE_Unknown) {
    ParseError(Loc, "Invalid identifier, bad type name `%s`", *Name);
    delete this;
    return nullptr;
  }

  if (Type.Type == TYPE_Automatic) VCFatalError("VC INTERNAL COMPILER ERROR: unresolved automatic type (1)!");

  auto e = VTypeExpr::NewTypeExpr(Type, Loc);
  delete this;
  return e->ResolveAsType(ec);
}


//==========================================================================
//
//  VSingleName::Emit
//
//==========================================================================
void VSingleName::Emit (VEmitContext &) {
  ParseError(Loc, "Should not happen (VSingleName)");
}


//==========================================================================
//
//  VSingleName::IsValidTypeExpression
//
//==========================================================================
bool VSingleName::IsValidTypeExpression () const {
  return true;
}


//==========================================================================
//
//  VSingleName::toString
//
//==========================================================================
VStr VSingleName::toString () const {
  if (bIsBangSpecified && VStr::startsWith(*Name, "specified_")) {
    return VStr(*Name+10)+"!specified";
  }
  return VStr(Name);
}



//==========================================================================
//
//  VDoubleName::VDoubleName
//
//==========================================================================
VDoubleName::VDoubleName (VName AName1, VName AName2, const TLocation &ALoc)
  : VExpression(ALoc)
  , Name1(AName1)
  , Name2(AName2)
{
}


//==========================================================================
//
//  VDoubleName::SyntaxCopy
//
//==========================================================================
VExpression *VDoubleName::SyntaxCopy () {
  auto res = new VDoubleName();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDoubleName::DoSyntaxCopyTo
//
//==========================================================================
void VDoubleName::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDoubleName *)e;
  res->Name1 = Name1;
  res->Name2 = Name2;
}


//==========================================================================
//
//  VDoubleName::DoResolve
//
//==========================================================================
VExpression *VDoubleName::DoResolve (VEmitContext &ec) {
  VClass *Class = VMemberBase::StaticFindClass(Name1);
  if (!Class) {
    ParseError(Loc, "No such class `%s`", *Name1);
    delete this;
    return nullptr;
  }

  VConstant *Const = Class->FindConstant(Name2);
  if (Const) {
    VExpression *e = new VConstantValue(Const, Loc);
    delete this;
    return e->Resolve(ec);
  }

  ParseError(Loc, "No such constant or state `%s::%s`", *Name1, *Name2);
  delete this;
  return nullptr;
}


//==========================================================================
//
//  VDoubleName::ResolveAsType
//
//==========================================================================
VTypeExpr *VDoubleName::ResolveAsType (VEmitContext &ec) {
  VClass *Class = VMemberBase::StaticFindClass(Name1);

  if (!Class) {
    ParseError(Loc, "No such class `%s`", *Name1);
    delete this;
    return nullptr;
  }

  Type = VMemberBase::StaticFindType(Class, Name2);

  if (Type.Type == TYPE_Unknown) {
    // try enum name
    if (Class->IsKnownEnum(Name2)) {
      auto e = VTypeExpr::NewTypeExpr(VFieldType(TYPE_Int), Loc);
      delete this;
      return e->ResolveAsType(ec);
    }
    ParseError(Loc, "Invalid identifier, bad type name `%s::%s`", *Name1, *Name2);
    delete this;
    return nullptr;
  }

  if (Type.Type == TYPE_Automatic) VCFatalError("VC INTERNAL COMPILER ERROR: unresolved automatic type (2)!");

  auto e = VTypeExpr::NewTypeExpr(Type, Loc);
  delete this;
  return e->ResolveAsType(ec);
}


//==========================================================================
//
//  VDoubleName::Emit
//
//==========================================================================
void VDoubleName::Emit (VEmitContext &) {
  ParseError(Loc, "Should not happen (VDoubleName)");
}


//==========================================================================
//
//  VDoubleName::IsValidTypeExpression
//
//==========================================================================
bool VDoubleName::IsValidTypeExpression () const {
  return true;
}


//==========================================================================
//
//  VDoubleName::IsDoubleName
//
//==========================================================================
bool VDoubleName::IsDoubleName () const {
  return true;
}


//==========================================================================
//
//  VDoubleName::toString
//
//==========================================================================
VStr VDoubleName::toString () const {
  return VStr(Name1)+"::"+VStr(Name2);
}



//==========================================================================
//
//  VDefaultObject::VDefaultObject
//
//==========================================================================
VDefaultObject::VDefaultObject (VExpression *AOp, const TLocation &ALoc)
  : VExpression(ALoc)
  , op(AOp)
{
}


//==========================================================================
//
//  VDefaultObject::~VDefaultObject
//
//==========================================================================
VDefaultObject::~VDefaultObject () {
  if (op) { delete op; op = nullptr; }
}


//==========================================================================
//
//  VDefaultObject::SyntaxCopy
//
//==========================================================================
VExpression *VDefaultObject::SyntaxCopy () {
  auto res = new VDefaultObject();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDefaultObject::DoSyntaxCopyTo
//
//==========================================================================
void VDefaultObject::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDefaultObject *)e;
  res->op = (op ? op->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDefaultObject::DoResolve
//
//==========================================================================
VExpression *VDefaultObject::DoResolve (VEmitContext &ec) {
  if (op) op = op->Resolve(ec);
  if (!op) {
    delete this;
    return nullptr;
  }

  if (op->Type.Type == TYPE_Reference) {
    Type = op->Type;
    SetResolved();
    return this;
  }

  if (op->Type.Type == TYPE_Class) {
    if (!op->Type.Class) {
      ParseError(Loc, "A typed class value required");
      delete this;
      return nullptr;
    }
    Type = VFieldType(op->Type.Class);
    SetResolved();
    return this;
  }

  ParseError(Loc, "Reference or class expected on left side of default");
  delete this;
  return nullptr;
}


//==========================================================================
//
//  VDefaultObject::Emit
//
//==========================================================================
void VDefaultObject::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  op->Emit(ec);
       if (op->Type.Type == TYPE_Reference) ec.AddStatement(OPC_GetDefaultObj, Loc);
  else if (op->Type.Type == TYPE_Class) ec.AddStatement(OPC_GetClassDefaultObj, Loc);
}


//==========================================================================
//
//  VDefaultObject::IsDefaultObject
//
//==========================================================================
bool VDefaultObject::IsDefaultObject () const {
  return true;
}


//==========================================================================
//
//  VDefaultObject::toString
//
//==========================================================================
VStr VDefaultObject::toString () const {
  return VStr("default");
}



//==========================================================================
//
//  VPushPointed::VPushPointed
//
//==========================================================================
VPushPointed::VPushPointed (VExpression *AOp, const TLocation &ALoc)
  : VExpression(ALoc)
  , op(AOp)
  , AddressRequested(false)
{
  if (!op) {
    ParseError(Loc, "Expression expected");
    return;
  }
  if (op->IsReadOnly()) SetReadOnly();
}


//==========================================================================
//
//  VPushPointed::~VPushPointed
//
//==========================================================================
VPushPointed::~VPushPointed () {
  if (op) { delete op; op = nullptr; }
}


//==========================================================================
//
//  VPushPointed::SyntaxCopy
//
//==========================================================================
VExpression *VPushPointed::SyntaxCopy () {
  auto res = new VPushPointed();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VPushPointed::DoSyntaxCopyTo
//
//==========================================================================
void VPushPointed::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VPushPointed *)e;
  res->op = (op ? op->SyntaxCopy() : nullptr);
  res->AddressRequested = AddressRequested;
}


//==========================================================================
//
//  VPushPointed::DoResolve
//
//==========================================================================
VExpression *VPushPointed::DoResolve (VEmitContext &ec) {
  if (op) op = op->Resolve(ec);
  if (!op) {
    delete this;
    return nullptr;
  }

  if (op->Type.Type != TYPE_Pointer) {
    ParseError(Loc, "Expression syntax error");
    delete this;
    return nullptr;
  }

  Type = op->Type.GetPointerInnerType();
  RealType = Type;
  if (Type.Type == TYPE_Byte || Type.Type == TYPE_Bool) Type = VFieldType(TYPE_Int);
  SetResolved();
  return this;
}


//==========================================================================
//
//  VPushPointed::RequestAddressOf
//
//==========================================================================
void VPushPointed::RequestAddressOf () {
  if (RealType.Type == TYPE_Void) {
    ParseError(Loc, "Bad address operation");
    return;
  }
  if (AddressRequested) ParseError(Loc, "Multiple address of pushpointed (%s)", *toString());
  AddressRequested = true;
}


//==========================================================================
//
//  VPushPointed::Emit
//
//==========================================================================
void VPushPointed::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  op->Emit(ec);
  if (!AddressRequested) EmitPushPointedCode(RealType, ec);
}


//==========================================================================
//
//  VPushPointed::toString
//
//==========================================================================
VStr VPushPointed::toString () const {
  return VStr("*(")+e2s(op)+")";
}



//==========================================================================
//
//  VConditional::VConditional
//
//==========================================================================
VConditional::VConditional (VExpression *AOp, VExpression *AOp1, VExpression *AOp2, const TLocation &ALoc)
  : VExpression(ALoc)
  , op(AOp)
  , op1(AOp1)
  , op2(AOp2)
{
  if (!op1) { ParseError(Loc, "Expression expected"); return; }
  if (!op2) { ParseError(Loc, "Expression expected"); return; }
}


//==========================================================================
//
//  VConditional::~VConditional
//
//==========================================================================
VConditional::~VConditional () {
  if (op) { delete op; op = nullptr; }
  if (op1) { delete op1; op1 = nullptr; }
  if (op2) { delete op2; op2 = nullptr; }
}


//==========================================================================
//
//  VConditional::IsTernary
//
//==========================================================================
bool VConditional::IsTernary () const {
  return true;
}


//==========================================================================
//
//  VConditional::SyntaxCopy
//
//==========================================================================
VExpression *VConditional::SyntaxCopy () {
  auto res = new VConditional();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VConditional::DoSyntaxCopyTo
//
//==========================================================================
void VConditional::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VConditional *)e;
  res->op = (op ? op->SyntaxCopy() : nullptr);
  res->op1 = (op1 ? op1->SyntaxCopy() : nullptr);
  res->op2 = (op2 ? op2->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VConditional::DoResolve
//
//==========================================================================
VExpression *VConditional::DoResolve (VEmitContext &ec) {
  if (op) {
    if (op->IsParens()) {
      VExpression *e = ((VExprParens *)op)->op;
      if (e->IsBinaryMath() && !((VBinary *)e)->IsComparison()) {
        ParseError(Loc, "I don't like this syntax; please, either remove parens, or use them properly; thank you.");
        delete this;
        return nullptr;
      }
    }
    //VExpression *ce = (op->IsParens() ? ((VExprParens *)op)->op : op);
    if (op->IsBinaryMath()) {
      VBinary *b = (VBinary *)op;
      if (!b->IsComparison() && b->op1 && b->op2 && (b->op1->IsParens() || b->op2->IsParens())) {
        ParseError(Loc, "Your idea of how ternaries are working is probably wrong; please, use parens to make your intent clear.");
        delete this;
        return nullptr;
      }
    }
  }

  if (op) op = op->ResolveBoolean(ec);
  if (op1) op1 = op1->Resolve(ec);
  if (op2) op2 = op2->Resolve(ec);
  if (!op || !op1 || !op2) {
    delete this;
    return nullptr;
  }

  VExpression::CoerceTypes(ec, op1, op2, true); // coerce "none delegate" here
  if (!op1 || !op2) { delete this; return nullptr; }

  op1->Type.CheckMatch(false, Loc, op2->Type);
  Type = op1->Type;
  if ((op1->Type.Type == TYPE_Pointer && op1->Type.InnerType == TYPE_Void) ||
      op1->IsNoneDelegateLiteral() || op1->IsNoneLiteral())
  {
    Type = op2->Type;
  }

  SetResolved();
  return this;
}


//==========================================================================
//
//  VConditional::Emit
//
//==========================================================================
void VConditional::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  VLabel FalseTarget = ec.DefineLabel();
  VLabel End = ec.DefineLabel();

  op->EmitBranchable(ec, FalseTarget, false);
  op1->Emit(ec);
  ec.AddStatement(OPC_Goto, End, Loc);
  ec.MarkLabel(FalseTarget);
  op2->Emit(ec);
  ec.MarkLabel(End);
}


//==========================================================================
//
//  VConditional::toString
//
//==========================================================================
VStr VConditional::toString () const {
  return VStr("(")+e2s(op)+" ? "+e2s(op1)+" : "+e2s(op2)+")";
}



//==========================================================================
//
//  VCommaExprRetOp0::VCommaExprRetOp0
//
//==========================================================================
VCommaExprRetOp0::VCommaExprRetOp0 (VExpression *abefore, VExpression *aafter, const TLocation &aloc)
  : VExpression(aloc)
  , op0(abefore)
  , op1(aafter)
{
  if (!abefore) { ParseError(Loc, "Expression expected"); return; }
  if (!aafter) { ParseError(Loc, "Expression expected"); return; }
}


//==========================================================================
//
//  VCommaExprRetOp0::~VCommaExprRetOp0
//
//==========================================================================
VCommaExprRetOp0::~VCommaExprRetOp0 () {
  if (op0) { delete op0; op0 = nullptr; }
  if (op1) { delete op1; op1 = nullptr; }
}


//==========================================================================
//
//  VCommaExprRetOp0::SyntaxCopy
//
//==========================================================================
VExpression *VCommaExprRetOp0::SyntaxCopy () {
  auto res = new VCommaExprRetOp0();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VCommaExprRetOp0::DoSyntaxCopyTo
//
//==========================================================================
void VCommaExprRetOp0::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VCommaExprRetOp0 *)e;
  res->op0 = (op0 ? op0->SyntaxCopy() : nullptr);
  res->op1 = (op1 ? op1->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VCommaExprRetOp0::DoResolve
//
//==========================================================================
VExpression *VCommaExprRetOp0::DoResolve (VEmitContext &ec) {
  if (op0) op0 = op0->Resolve(ec);
  if (op1) {
    // automatically add result drop
    op1 = new VDropResult(op1);
    if (op1) op1 = op1->Resolve(ec);
  }
  if (!op0 || !op1) {
    delete this;
    return nullptr;
  }
  Type = op0->Type;
  SetResolved();
  return this;
}


//==========================================================================
//
//  VCommaExprRetOp0::Emit
//
//==========================================================================
void VCommaExprRetOp0::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  if (op0) op0->Emit(ec);
  if (op1) op1->Emit(ec);
}


//==========================================================================
//
//  VCommaExprRetOp0::IsComma
//
//==========================================================================
bool VCommaExprRetOp0::IsComma () const {
  return true;
}


//==========================================================================
//
//  VCommaExprRetOp0::IsCommaRetOp0
//
//==========================================================================
bool VCommaExprRetOp0::IsCommaRetOp0 () const {
  return true;
}


//==========================================================================
//
//  VCommaExprRetOp0::toString
//
//==========================================================================
VStr VCommaExprRetOp0::toString () const {
  return VStr("(")+e2s(op0)+",(void)"+e2s(op1)+")";
}



//==========================================================================
//
//  VDropResult::VDropResult
//
//==========================================================================
VDropResult::VDropResult (VExpression *AOp)
  : VExpression(AOp ? AOp->Loc : TLocation())
  , op(AOp)
{
}


//==========================================================================
//
//  VDropResult::~VDropResult
//
//==========================================================================
VDropResult::~VDropResult () {
  if (op) { delete op; op = nullptr; }
}


//==========================================================================
//
//  VDropResult::SyntaxCopy
//
//==========================================================================
VExpression *VDropResult::SyntaxCopy () {
  auto res = new VDropResult();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDropResult::DoSyntaxCopyTo
//
//==========================================================================
void VDropResult::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDropResult *)e;
  res->op = (op ? op->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDropResult::DoResolve
//
//==========================================================================
VExpression *VDropResult::DoResolve (VEmitContext &ec) {
  //k8: oops, this can be added to resolved expressions sometimes (FIXME!)
  if (op /*&& !op->IsResolved()*/) op = op->Resolve(ec);
  if (!op) { delete this; return nullptr; }

  if (op->Type.Type == TYPE_Delegate) {
    // paren-less delegate call
    //fprintf(stderr, ":<%s> : %s\n", *this->toString(), *shitppTypeNameObj(*op));
    ParseError(Loc, "Delegate call parameters are missing");
    delete this;
    return nullptr;
  }

  if (op->Type.Type == TYPE_Void) {
    VExpression *e = op;
    op = nullptr;
    delete this;
    return e;
  }

  if (op->Type.Type != TYPE_String && op->Type.GetStackSize() != 1 &&
      op->Type.Type != TYPE_Vector && op->Type.Type != TYPE_Void)
  {
    ParseError(Loc, "Expression result type (%s) cannot be dropped", *op->Type.GetName());
    delete this;
    return nullptr;
  }

  VExpression *dpr = op->AddDropResult();
  if (dpr) {
    //fprintf(stderr, "*** <%s> *** (%s)\n", *dpr->toString(), *dpr->Loc.toStringNoCol());
    op = nullptr;
    delete this;
    return dpr;
  }

  Type = TYPE_Void;
  if (op->IsReadOnly()) SetReadOnly();
  SetResolved();
  return this;
}


//==========================================================================
//
//  VDropResult::Emit
//
//==========================================================================
void VDropResult::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  op->Emit(ec);
       if (op->Type.Type == TYPE_String) ec.AddStatement(OPC_DropStr, Loc);
  else if (op->Type.Type == TYPE_Vector) ec.AddStatement(OPC_VDrop, Loc);
  else if (op->Type.GetStackSize() == 1) ec.AddStatement(OPC_Drop, Loc);
}


//==========================================================================
//
//  VDropResult::IsDropResult
//
//==========================================================================
bool VDropResult::IsDropResult () const {
  return true;
}


//==========================================================================
//
//  VDropResult::toString
//
//==========================================================================
VStr VDropResult::toString () const {
  return VStr("void(")+(op ? op->GetMyTypeName() : VStr())+e2s(op)+")";
}



//==========================================================================
//
//  VClassConstant::VClassConstant
//
//==========================================================================
VClassConstant::VClassConstant (VClass *AClass, const TLocation &ALoc)
  : VExpression(ALoc)
  , Class(AClass)
{
  Type = TYPE_Class;
  Type.Class = Class;
}


//==========================================================================
//
//  VClassConstant::SyntaxCopy
//
//==========================================================================
VExpression *VClassConstant::SyntaxCopy () {
  auto res = new VClassConstant();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VClassConstant::DoSyntaxCopyTo
//
//==========================================================================
void VClassConstant::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VClassConstant *)e;
  res->Class = Class;
}


//==========================================================================
//
//  VClassConstant::DoResolve
//
//==========================================================================
VExpression *VClassConstant::DoResolve (VEmitContext &) {
  SetResolved();
  return this;
}


//==========================================================================
//
//  VClassConstant::Emit
//
//==========================================================================
void VClassConstant::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  ec.AddStatement(OPC_PushClassId, Class, Loc);
}


//==========================================================================
//
//  VClassConstant::toString
//
//==========================================================================
VStr VClassConstant::toString () const {
  return VStr("(")+(Class ? VStr(Class->Name) : e2s(nullptr))+")";
}



//==========================================================================
//
//  VConstantValue::VConstantValue
//
//==========================================================================
VConstantValue::VConstantValue (VConstant *AConst, const TLocation &ALoc)
  : VExpression(ALoc)
  , Const(AConst)
{
}


//==========================================================================
//
//  VConstantValue::SyntaxCopy
//
//==========================================================================
VExpression *VConstantValue::SyntaxCopy () {
  auto res = new VConstantValue();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VConstantValue::DoSyntaxCopyTo
//
//==========================================================================
void VConstantValue::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VConstantValue *)e;
  res->Const = Const;
}


//==========================================================================
//
//  VConstantValue::DoResolve
//
//==========================================================================
VExpression *VConstantValue::DoResolve (VEmitContext &) {
  Type = (EType)Const->Type;
  SetResolved();
  return this;
}


//==========================================================================
//
//  VConstantValue::Emit
//
//==========================================================================
void VConstantValue::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  ec.EmitPushNumber(Const->Value, Loc);
}


//==========================================================================
//
//  VConstantValue::GetIntConst
//
//==========================================================================
vint32 VConstantValue::GetIntConst () const {
  if (Const->Type == TYPE_Int) return Const->Value;
  return VExpression::GetIntConst();
}


//==========================================================================
//
//  VConstantValue::GetFloatConst
//
//==========================================================================
float VConstantValue::GetFloatConst () const {
  if (Const->Type == TYPE_Float) return Const->FloatValue;
  return VExpression::GetFloatConst();
}


//==========================================================================
//
//  VConstantValue::IsIntConst
//
//==========================================================================
bool VConstantValue::IsIntConst () const {
  return (Const->Type == TYPE_Int);
}


//==========================================================================
//
//  VConstantValue::IsFloatConst
//
//==========================================================================
bool VConstantValue::IsFloatConst () const {
  return (Const->Type == TYPE_Float);
}


//==========================================================================
//
//  VConstantValue::toString
//
//==========================================================================
VStr VConstantValue::toString () const {
  return VStr("const(")+(Const ? Const->toString() : e2s(nullptr))+")";
}



//==========================================================================
//
//  VObjectPropGetIsDestroyed::VObjectPropGetIsDestroyed
//
//==========================================================================
VObjectPropGetIsDestroyed::VObjectPropGetIsDestroyed (VExpression *AObjExpr, const TLocation &ALoc)
  : VExpression(ALoc)
  , ObjExpr(AObjExpr)
{
}


//==========================================================================
//
//  VObjectPropGetIsDestroyed::~VObjectPropGetIsDestroyed
//
//==========================================================================
VObjectPropGetIsDestroyed::~VObjectPropGetIsDestroyed () {
  if (ObjExpr) {
    delete ObjExpr;
    ObjExpr = nullptr;
  }
}


//==========================================================================
//
//  VObjectPropGetIsDestroyed::SyntaxCopy
//
//==========================================================================
VExpression *VObjectPropGetIsDestroyed::SyntaxCopy () {
  auto res = new VObjectPropGetIsDestroyed();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VObjectPropGetIsDestroyed::DoSyntaxCopyTo
//
//==========================================================================
void VObjectPropGetIsDestroyed::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VObjectPropGetIsDestroyed *)e;
  res->ObjExpr = (ObjExpr ? ObjExpr->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VObjectPropGetIsDestroyed::DoResolve
//
//==========================================================================
VExpression *VObjectPropGetIsDestroyed::DoResolve (VEmitContext &ec) {
  if (ObjExpr) ObjExpr = ObjExpr->Resolve(ec);
  if (!ObjExpr) {
    delete this;
    return nullptr;
  }

  Type = VFieldType(TYPE_Int);
  SetResolved();
  return this;
}


//==========================================================================
//
//  VObjectPropGetIsDestroyed::Emit
//
//==========================================================================
void VObjectPropGetIsDestroyed::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  ObjExpr->Emit(ec);
  ec.AddStatement(OPC_GetIsDestroyed, Loc);
}


//==========================================================================
//
//  VObjectPropGetIsDestroyed::toString
//
//==========================================================================
VStr VObjectPropGetIsDestroyed::toString () const {
  return VExpression::e2s(ObjExpr)+".IsDestroyed";
}
