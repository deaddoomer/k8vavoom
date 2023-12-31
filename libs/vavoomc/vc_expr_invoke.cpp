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
#include <limits.h>
#include <float.h>
#include <math.h>

// builtin codes
#define BUILTIN_OPCODE_INFO
#include "vc_progdefs.h"

#define DICTDISPATCH_OPCODE_INFO
#include "vc_progdefs.h"

#define DYNARRDISPATCH_OPCODE_INFO
#include "vc_progdefs.h"


//==========================================================================
//
//  VDynCastWithVar::VDynCastWithVar
//
//==========================================================================
VDynCastWithVar::VDynCastWithVar (VExpression *awhat, VExpression *adestclass, const TLocation &aloc)
  : VExpression(aloc)
  , what(awhat)
  , destclass(adestclass)
{
}


//==========================================================================
//
//  VDynCastWithVar::~VDynCastWithVar
//
//==========================================================================
VDynCastWithVar::~VDynCastWithVar () {
  delete what; what = nullptr;
  delete destclass; destclass = nullptr;
}


//==========================================================================
//
//  VDynCastWithVar::HasSideEffects
//
//==========================================================================
bool VDynCastWithVar::HasSideEffects () {
  return
    (what && what->HasSideEffects()) ||
    (destclass && destclass->HasSideEffects());
}


//==========================================================================
//
//  VDynCastWithVar::VisitChildren
//
//==========================================================================
void VDynCastWithVar::VisitChildren (VExprVisitor *v) {
  if (!v->stopIt && what) what->Visit(v);
  if (!v->stopIt && destclass) destclass->Visit(v);
}


//==========================================================================
//
//  VDynCastWithVar::SyntaxCopy
//
//==========================================================================
VExpression *VDynCastWithVar::SyntaxCopy () {
  auto res = new VDynCastWithVar();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDynCastWithVar::DoSyntaxCopyTo
//
//==========================================================================
void VDynCastWithVar::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDynCastWithVar *)e;
  res->what = (what ? what->SyntaxCopy() : nullptr);
  res->destclass = (destclass ? destclass->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDynCastWithVar::DoResolve
//
//==========================================================================
VExpression *VDynCastWithVar::DoResolve (VEmitContext &ec) {
  if (what) what = what->Resolve(ec);
  if (destclass) destclass = destclass->Resolve(ec);
  if (!what || !destclass) { delete this; return nullptr; }

  if (destclass->Type.Type != TYPE_Class) {
    ParseError(destclass->Loc, "Dynamic cast destination must be of a class type");
    delete this;
    return nullptr;
  }

  if (what->Type.Type != TYPE_Class && what->Type.Type != TYPE_Reference) {
    ParseError(what->Loc, "Dynamic cast destination must be of a class or an object type");
    delete this;
    return nullptr;
  }

  // result of this is of `what` type with replaced class
  Type = destclass->Type;
  Type.Type = what->Type.Type;
  Type.Class = destclass->Type.Class;
  SetResolved();
  return this;
}


//==========================================================================
//
//  VDynCastWithVar::Emit
//
//==========================================================================
void VDynCastWithVar::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  if (what) what->Emit(ec);
  if (destclass) destclass->Emit(ec);
  ec.AddStatement((what && what->Type.Type == TYPE_Class ? OPC_DynamicClassCastIndirect : OPC_DynamicCastIndirect), Loc);
}


//==========================================================================
//
//  VDynCastWithVar::toString
//
//==========================================================================
VStr VDynCastWithVar::toString () const {
  return e2s(destclass)+"("+e2s(what)+")";
}


//==========================================================================
//
//  VArgMarshall::VArgMarshall
//
//==========================================================================
VArgMarshall::VArgMarshall (VExpression *ae)
  : VExpression(ae->Loc)
  , e(ae)
  , isRef(false)
  , isOut(false)
  , marshallOpt(false)
  , argName(NAME_None)
{
}


//==========================================================================
//
//  VArgMarshall::VArgMarshall
//
//==========================================================================
VArgMarshall::VArgMarshall (VName aname, const TLocation &aloc)
  : VExpression(aloc)
  , e(nullptr)
  , isRef(false)
  , isOut(false)
  , marshallOpt(false)
  , argName(aname)
{
}


//==========================================================================
//
//  VArgMarshall::HasSideEffects
//
//==========================================================================
bool VArgMarshall::HasSideEffects () {
  return (e && e->HasSideEffects());
}


//==========================================================================
//
//  VArgMarshall::VisitChildren
//
//==========================================================================
void VArgMarshall::VisitChildren (VExprVisitor *v) {
  if (!v->stopIt && e) e->Visit(v);
}


//==========================================================================
//
//  VArgMarshall::~VArgMarshall
//
//==========================================================================
VArgMarshall::~VArgMarshall () {
  delete e;
}


//==========================================================================
//
//  VArgMarshall::DoResolve
//
//==========================================================================
VExpression *VArgMarshall::DoResolve (VEmitContext &ec) {
  if (e) e = e->Resolve(ec);
  if (!e) { delete this; return nullptr; }
  VExpression *res = e;
  e = nullptr;
  delete this;
  return res;
}


//==========================================================================
//
//  VArgMarshall::SyntaxCopy
//
//==========================================================================
VExpression *VArgMarshall::SyntaxCopy () {
  auto res = new VArgMarshall();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VArgMarshall::DoSyntaxCopyTo
//
//==========================================================================
void VArgMarshall::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VArgMarshall *)e;
  res->e = (this->e ? this->e->SyntaxCopy() : nullptr);
  res->isRef = isRef;
  res->isOut = isOut;
  res->marshallOpt = marshallOpt;
  res->argName = argName;
}


//==========================================================================
//
//  VArgMarshall::Emit
//
//==========================================================================
void VArgMarshall::Emit (VEmitContext &/*ec*/) {
  VCFatalError("The thing that should not be (VArgMarshall::Emit)");
}


//==========================================================================
//
//  VArgMarshall::IsMarshallArg
//
//==========================================================================
bool VArgMarshall::IsMarshallArg () const {
  return true;
}


//==========================================================================
//
//  VArgMarshall::IsRefArg
//
//==========================================================================
bool VArgMarshall::IsRefArg () const {
  return isRef;
}


//==========================================================================
//
//  VArgMarshall::IsOutArg
//
//==========================================================================
bool VArgMarshall::IsOutArg () const {
  return isOut;
}


//==========================================================================
//
//  VArgMarshall::IsOptMarshallArg
//
//==========================================================================
bool VArgMarshall::IsOptMarshallArg () const {
  return marshallOpt;
}


//==========================================================================
//
//  VArgMarshall::IsDefaultArg
//
//==========================================================================
bool VArgMarshall::IsDefaultArg () const {
  return (e == nullptr);
}


//==========================================================================
//
//  VArgMarshall::IsNamedArg
//
//==========================================================================
bool VArgMarshall::IsNamedArg () const {
  return (argName != NAME_None);
}


//==========================================================================
//
//  VArgMarshall::GetArgName
//
//==========================================================================
VName VArgMarshall::GetArgName () const {
  return argName;
}


//==========================================================================
//
//  VArgMarshall::toString
//
//==========================================================================
VStr VArgMarshall::toString () const {
  VStr res;
  if (isRef) res += "ref ";
  if (isOut) res += "out ";
  if (argName != NAME_None) { res = *argName; res += ":"; }
  res += (e ? e2s(e) : "default");
  if (marshallOpt) res += "!optional";
  return res;
}



//==========================================================================
//
//  VInvocationBase::VInvocationBase
//
//==========================================================================
VInvocationBase::VInvocationBase (int ANumArgs, VExpression **AArgs, const TLocation &ALoc)
  : VExpression(ALoc)
  , NumArgs(ANumArgs)
{
  memset(Args, 0, sizeof(Args)); // why not
  for (int i = 0; i < NumArgs; ++i) Args[i] = AArgs[i];
}


//==========================================================================
//
//  VInvocationBase::~VInvocationBase
//
//==========================================================================
VInvocationBase::~VInvocationBase () {
  for (int i = 0; i < NumArgs; ++i) { delete Args[i]; Args[i] = nullptr; }
  NumArgs = 0;
}


//==========================================================================
//
//  VInvocationBase::HasSideEffects
//
//==========================================================================
bool VInvocationBase::HasSideEffects () {
  return true;
}


//==========================================================================
//
//  VInvocationBase::VisitChildren
//
//==========================================================================
void VInvocationBase::VisitChildren (VExprVisitor *v) {
  for (int f = 0; !v->stopIt && f < NumArgs; f += 1) {
    if (Args[f]) Args[f]->Visit(v);
  }
}


//==========================================================================
//
//  VInvocationBase::DoSyntaxCopyTo
//
//==========================================================================
void VInvocationBase::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VInvocationBase *)e;
  memset(res->Args, 0, sizeof(res->Args));
  res->NumArgs = NumArgs;
  for (int i = 0; i < NumArgs; ++i) res->Args[i] = (Args[i] ? Args[i]->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VInvocationBase::IsAnyInvocation
//
//==========================================================================
bool VInvocationBase::IsAnyInvocation () const {
  return true;
}


//==========================================================================
//
//  VInvocationBase::IsAnyInvocation
//
//==========================================================================
VStr VInvocationBase::args2str () const {
  VStr res("(");
  for (int f = 0; f < NumArgs; ++f) {
    if (f != 0) res += ", ";
    if (Args[f]) res += Args[f]->toString(); else res += "default";
  }
  res += ")";
  return res;
}



//==========================================================================
//
//  VSuperInvocation::VSuperInvocation
//
//==========================================================================
VSuperInvocation::VSuperInvocation(VName AName, int ANumArgs, VExpression **AArgs, const TLocation &ALoc)
  : VInvocationBase(ANumArgs, AArgs, ALoc)
  , Name(AName)
{
}


//==========================================================================
//
//  VSuperInvocation::SyntaxCopy
//
//==========================================================================
VExpression *VSuperInvocation::SyntaxCopy () {
  auto res = new VSuperInvocation();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VSuperInvocation::DoSyntaxCopyTo
//
//==========================================================================
void VSuperInvocation::DoSyntaxCopyTo (VExpression *e) {
  VInvocationBase::DoSyntaxCopyTo(e);
  auto res = (VSuperInvocation *)e;
  res->Name = Name;
}


//==========================================================================
//
//  VSuperInvocation::DoResolve
//
//==========================================================================
VExpression *VSuperInvocation::DoResolve (VEmitContext &ec) {
  // struct
  if (ec.SelfStruct && ec.SelfStruct->ParentStruct) {
    VMethod *Func = ec.SelfStruct->ParentStruct->FindAccessibleMethod(Name, ec.SelfStruct, &Loc);
    if (Func) {
      VInvocation *e = new VInvocation(nullptr, Func, nullptr, false, true, Loc, NumArgs, Args);
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }
  }

  // class
  if (ec.SelfClass && ec.SelfClass->ParentClass) {
    VMethod *Func = ec.SelfClass->ParentClass->FindAccessibleMethod(Name, ec.SelfClass, &Loc);
    if (Func) {
      VInvocation *e = new VInvocation(nullptr, Func, nullptr, false, true, Loc, NumArgs, Args);
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }
  }

  if (VStr::Cmp(*Name, "write") == 0 || VStr::Cmp(*Name, "writeln") == 0) {
    VExpression *e = new VInvokeWrite((VStr::Cmp(*Name, "writeln") == 0), Loc, NumArgs, Args);
    NumArgs = 0;
    delete this;
    return e->Resolve(ec);
  }

  if (!ec.SelfClass) {
    ParseError(Loc, ":: not in method");
    delete this;
    return nullptr;
  }

  ParseError(Loc, "No such method `%s` in parent class `%s` of class `%s`", *Name, (ec.SelfClass->ParentClass ? ec.SelfClass->ParentClass->GetName() : "<none>"), ec.SelfClass->GetName());
  delete this;
  return nullptr;
}


//==========================================================================
//
//  VSuperInvocation::Emit
//
//==========================================================================
void VSuperInvocation::Emit (VEmitContext &) {
  ParseError(Loc, "Should not happen (VSuperInvocation)");
}


//==========================================================================
//
//  VSuperInvocation::GetVMethod
//
//==========================================================================
VMethod *VSuperInvocation::GetVMethod (VEmitContext &ec) {
  if (!ec.SelfClass || !ec.SelfClass->ParentClass) return nullptr;
  return ec.SelfClass->ParentClass->FindAccessibleMethod(Name, ec.SelfClass, &Loc);
}


//==========================================================================
//
//  VSuperInvocation::IsMethodNameChangeable
//
//==========================================================================
bool VSuperInvocation::IsMethodNameChangeable () const {
  return true;
}


//==========================================================================
//
//  VSuperInvocation::GetMethodName
//
//==========================================================================
VName VSuperInvocation::GetMethodName () const {
  return Name;
}


//==========================================================================
//
//  VSuperInvocation::SetMethodName
//
//==========================================================================
void VSuperInvocation::SetMethodName (VName aname) {
  Name = aname;
}


//==========================================================================
//
//  VSuperInvocation::toString
//
//==========================================================================
VStr VSuperInvocation::toString () const {
  return VStr("::")+(*Name)+args2str();
}



//==========================================================================
//
//  VCastOrInvocation::VCastOrInvocation
//
//==========================================================================
VCastOrInvocation::VCastOrInvocation (VName AName, const TLocation &ALoc, int ANumArgs, VExpression **AArgs)
  : VInvocationBase(ANumArgs, AArgs, ALoc)
  , Name(AName)
{
}


//==========================================================================
//
//  VCastOrInvocation::SyntaxCopy
//
//==========================================================================
VExpression *VCastOrInvocation::SyntaxCopy () {
  auto res = new VCastOrInvocation();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VCastOrInvocation::DoSyntaxCopyTo
//
//==========================================================================
void VCastOrInvocation::DoSyntaxCopyTo (VExpression *e) {
  VInvocationBase::DoSyntaxCopyTo(e);
  auto res = (VCastOrInvocation *)e;
  res->Name = Name;
}


//==========================================================================
//
//  VCastOrInvocation::DoResolve
//
//==========================================================================
VExpression *VCastOrInvocation::DoResolve (VEmitContext &ec) {
  // look for delegate-typed local var
  int num = ec.CheckForLocalVar(Name);
  if (num != -1) {
    VFieldType tp = ec.GetLocalVarType(num);
    if (tp.Type != TYPE_Class && tp.Type != TYPE_Delegate) {
      // allow further checks
      /*
      ParseError(Loc, "Cannot call non-delegate local `%s`", *Name);
      delete this;
      return nullptr;
      */
    } else {
      if (tp.Type == TYPE_Delegate) {
        VInvocation *e = new VInvocation(tp.Function, num, Loc, NumArgs, Args);
        NumArgs = 0;
        delete this;
        return e->Resolve(ec);
      }
      // cast to class variable
      if (NumArgs != 1 || !Args[0] || Args[0]->IsDefaultArg()) {
        ParseError(Loc, "Dynamic cast of `%s` requires one argument", *Name);
        delete this;
        return nullptr;
      }
      VExpression *e = new VDynCastWithVar(Args[0],
                                           new VLocalVar(num, ec.IsLocalUnsafe(num), Loc),
                                           Loc);
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }
  }

  // find struct method
  if (ec.SelfStruct) {
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
        e = new VInvocation(nullptr, M, nullptr, false, false, Loc, NumArgs, Args);
      } else {
        e = new VDotInvocation(new VSelf(Loc), Name, Loc, NumArgs, Args);
      }
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }
  }

  VClass *Class = VMemberBase::StaticFindClass(Name);
  if (Class) {
    if (NumArgs != 1 || !Args[0] || Args[0]->IsDefaultArg()) {
      ParseError(Loc, "Dynamic cast of `%s` requires 1 argument", *Name);
      delete this;
      return nullptr;
    }
    VExpression *e = new VDynamicCast(Class, Args[0], Loc);
    NumArgs = 0;
    delete this;
    return e->Resolve(ec);
  }

  if (ec.SelfClass) {
    VMethod *M = ec.SelfClass->FindAccessibleMethod(Name, ec.SelfClass, &Loc);
    if (M) {
      if (M->Flags&FUNC_Iterator) {
        ParseError(Loc, "Iterator methods can only be used in foreach statements");
        delete this;
        return nullptr;
      }
      VInvocation *e = new VInvocation(nullptr, M, nullptr, false, false, Loc, NumArgs, Args);
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }

    VField *field = ec.SelfClass->FindField(Name, Loc, ec.SelfClass);
    if (field != nullptr) {
      if (field->Type.Type == TYPE_Delegate) {
        VInvocation *e = new VInvocation(nullptr, field->Func, field, false, false, Loc, NumArgs, Args);
        NumArgs = 0;
        delete this;
        return e->Resolve(ec);
      } else if (field->Type.Type == TYPE_Class) {
        // cast to class variable
        if (NumArgs != 1 || !Args[0] || Args[0]->IsDefaultArg()) {
          ParseError(Loc, "Dynamic cast requires one argument");
          delete this;
          return nullptr;
        }
        VExpression *e = new VSelf(Loc);
        if (e) e = e->Resolve(ec);
        if (e) {
          e = new VFieldAccess(e, field, Loc, 0);
          e = new VDynCastWithVar(Args[0], e, Loc);
        }
        NumArgs = 0;
        delete this;
        return (e ? e->Resolve(ec) : nullptr);
      }
    }

    if (VStr::Cmp(*Name, "write") == 0 || VStr::Cmp(*Name, "writeln") == 0) {
      VExpression *e = new VInvokeWrite((VStr::Cmp(*Name, "writeln") == 0), Loc, NumArgs, Args);
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }
  }

  ParseError(Loc, "Unknown method `%s`", *Name);
  delete this;
  return nullptr;
}


//==========================================================================
//
//  VCastOrInvocation::ResolveIterator
//
//==========================================================================
VExpression *VCastOrInvocation::ResolveIterator (VEmitContext &ec) {
  VMethod *M = ec.SelfClass->FindAccessibleMethod(Name, ec.SelfClass, &Loc);
  if (!M) {
    ParseError(Loc, "Unknown method `%s`", *Name);
    delete this;
    return nullptr;
  }
  if ((M->Flags&FUNC_Iterator) == 0) {
    ParseError(Loc, "`%s` is not an iterator method", *Name);
    delete this;
    return nullptr;
  }

  VInvocation *e = new VInvocation(nullptr, M, nullptr, false, false, Loc, NumArgs, Args);
  NumArgs = 0;
  delete this;
  return e->Resolve(ec);
}


//==========================================================================
//
//  VCastOrInvocation::Emit
//
//==========================================================================
void VCastOrInvocation::Emit (VEmitContext &) {
  ParseError(Loc, "Should not happen (VCastOrInvocation)");
}


//==========================================================================
//
//  VCastOrInvocation::GetVMethod
//
//==========================================================================
VMethod *VCastOrInvocation::GetVMethod (VEmitContext &ec) {
  if (!ec.SelfClass) return nullptr;
  return ec.SelfClass->FindAccessibleMethod(Name, ec.SelfClass, &Loc);
}


//==========================================================================
//
//  VCastOrInvocation::IsMethodNameChangeable
//
//==========================================================================
bool VCastOrInvocation::IsMethodNameChangeable () const {
  return true;
}


//==========================================================================
//
//  VCastOrInvocation::GetMethodName
//
//==========================================================================
VName VCastOrInvocation::GetMethodName () const {
  return Name;
}


//==========================================================================
//
//  VCastOrInvocation::SetMethodName
//
//==========================================================================
void VCastOrInvocation::SetMethodName (VName aname) {
  Name = aname;
}


//==========================================================================
//
//  VCastOrInvocation::toString
//
//==========================================================================
VStr VCastOrInvocation::toString () const {
  return VStr(Name)+args2str();
}



//==========================================================================
//
//  VDotInvocation::VDotInvocation
//
//==========================================================================
VDotInvocation::VDotInvocation (VExpression *ASelfExpr, VName AMethodName, const TLocation &ALoc, int ANumArgs, VExpression **AArgs)
  : VInvocationBase(ANumArgs, AArgs, ALoc)
  , SelfExpr(ASelfExpr)
  , MethodName(AMethodName)
{
}


//==========================================================================
//
//  VDotInvocation::~VDotInvocation
//
//==========================================================================
VDotInvocation::~VDotInvocation () {
  if (SelfExpr) { delete SelfExpr; SelfExpr = nullptr; }
}


//==========================================================================
//
//  VDotInvocation::SyntaxCopy
//
//==========================================================================
VExpression *VDotInvocation::SyntaxCopy () {
  auto res = new VDotInvocation();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDotInvocation::DoSyntaxCopyTo
//
//==========================================================================
void VDotInvocation::DoSyntaxCopyTo (VExpression *e) {
  VInvocationBase::DoSyntaxCopyTo(e);
  auto res = (VDotInvocation *)e;
  res->SelfExpr = (SelfExpr ? SelfExpr->SyntaxCopy() : nullptr);
  res->MethodName = MethodName;
}


//==========================================================================
//
//  VDotInvocation::DoReResolvePtr
//
//  this is used to avoid some pasta in `DoResolve()`
//  it re-resolves pointer to (*selfCopy), or deletes `selfCopy`, and
//  assigns result to `SelfExpr`
//  it also returns `false` on error (and does `delete this`)
//
//==========================================================================
bool VDotInvocation::DoReResolvePtr (VEmitContext &ec, VExpression *&selfCopy) {
  vassert(SelfExpr);
  vassert(selfCopy);
  vassert(selfCopy != SelfExpr);
  if (SelfExpr->Type.Type == TYPE_Pointer) {
    delete SelfExpr;
    SelfExpr = new VPushPointed(selfCopy, selfCopy->Loc);
    selfCopy = nullptr;
    SelfExpr = SelfExpr->Resolve(ec);
    if (!SelfExpr) { delete this; return false; }
  } else {
    delete selfCopy;
    selfCopy = nullptr;
  }
  return true;
}


//==========================================================================
//
//  VDotInvocation::DoResolve
//
//==========================================================================
VExpression *VDotInvocation::DoResolve (VEmitContext &ec) {
  //if (SelfExpr) GLog.Logf(NAME_Debug, "VDotInvocation::DoResolve: self=%s; mtn=<%s>", *SelfExpr->toString(), *MethodName);
  VExpression *selfCopy = (SelfExpr ? SelfExpr->SyntaxCopy() : nullptr);

  if (SelfExpr) SelfExpr = SelfExpr->Resolve(ec);
  if (!SelfExpr) {
    delete selfCopy;
    delete this;
    return nullptr;
  }

  if (SelfExpr->Type.Type == TYPE_DynamicArray) {
    delete selfCopy;

    // allow `length()` for cpp-addicts
    if (MethodName == NAME_Num || MethodName == NAME_Length || MethodName == NAME_length ||
        MethodName == "length1" || MethodName == "length2")
    {
      if (NumArgs != 0) {
        ParseError(Loc, "Cannot call `%s` with arguments", *MethodName);
        delete this;
        return nullptr;
      }
      SelfExpr->RequestAddressOf();
      VExpression *e = new VDynArrayGetNum(SelfExpr, (MethodName == "length1" ? 1 : MethodName == "length2" ? 2 : 0), Loc);
      SelfExpr = nullptr;
      delete this;
      return e->Resolve(ec);
    }

    // set new array size, 1d or 2d
    if (MethodName == "SetSize" || MethodName == "setSize" ||
        MethodName == "SetLength" || MethodName == "setLength") {
      if (NumArgs < 1 || NumArgs > 2) {
        ParseError(Loc, "Method `%s` expects one or two arguments", *MethodName);
        delete this;
        return nullptr;
      }
      // for 2d case, defaults are allowed
      if (NumArgs == 1 && (!Args[0] || Args[0]->IsDefaultArg())) {
        ParseError(Loc, "Method `%s` does not accept default arguments", *MethodName);
        delete this;
        return nullptr;
      }
      // check ref/out
      for (int f = 0; f < NumArgs; ++f) {
        if (!Args[f]) continue;
        if (Args[f]->IsRefArg()) {
          ParseError(Loc, "Method `%s` cannot has `ref` argument", *MethodName);
          delete this;
          return nullptr;
        }
        if (Args[f]->IsOutArg()) {
          ParseError(Loc, "Method `%s` cannot has `out` argument", *MethodName);
          delete this;
          return nullptr;
        }
      }
      // ok
      if (SelfExpr->IsReadOnly()) {
        ParseError(Loc, "Cannot set length of read-only array");
        delete this;
        return nullptr;
      }
      SelfExpr->RequestAddressOf();
      VDynArraySetNum *e = new VDynArraySetNum(SelfExpr, Args[0], Args[1], Loc);
      e->asSetSize = true;
      SelfExpr = nullptr;
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }

    if (MethodName == NAME_Insert || MethodName == "insert") {
      if (NumArgs == 1) {
        if (Args[0] && Args[0]->GetArgName() == "count") {
          ParseError(Loc, "`count` without index");
          delete this;
          return nullptr;
        }
        // default count is 1
        Args[1] = new VIntLiteral(1, Loc);
        NumArgs = 2;
      }
      if (NumArgs != 2 || !Args[0] || Args[0]->IsDefaultArg() || !Args[1] || Args[1]->IsDefaultArg()) {
        ParseError(Loc, "Insert requires 1 or 2 arguments");
        delete this;
        return nullptr;
      }
      if (Args[0]->IsRefArg() || Args[0]->IsOutArg()) {
        ParseError(Args[0]->Loc, "Insert cannot has `ref`/`out` argument");
        delete this;
        return nullptr;
      }
      if (Args[1]->IsRefArg() || Args[1]->IsOutArg()) {
        ParseError(Args[1]->Loc, "Insert cannot has `ref`/`out` argument");
        delete this;
        return nullptr;
      }
      VExpression *e = new VDynArrayInsert(SelfExpr, Args[0], Args[1], Loc);
      SelfExpr = nullptr;
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }

    if (MethodName == NAME_Remove || MethodName == "remove") {
      if (NumArgs == 1) {
        if (Args[0] && Args[0]->GetArgName() == "count") {
          ParseError(Loc, "`count` without index");
          delete this;
          return nullptr;
        }
        // default count is 1
        Args[1] = new VIntLiteral(1, Loc);
        NumArgs = 2;
      }
      if (NumArgs != 2 || !Args[0] || Args[0]->IsDefaultArg() || !Args[1] || Args[1]->IsDefaultArg()) {
        ParseError(Loc, "Insert requires 1 or 2 arguments");
        delete this;
        return nullptr;
      }
      if (Args[0]->IsRefArg() || Args[0]->IsOutArg()) {
        ParseError(Args[0]->Loc, "Remove cannot has `ref`/`out` argument");
        delete this;
        return nullptr;
      }
      if (Args[1]->IsRefArg() || Args[1]->IsOutArg()) {
        ParseError(Args[1]->Loc, "Remove cannot has `ref`/`out` argument");
        delete this;
        return nullptr;
      }
      VExpression *e = new VDynArrayRemove(SelfExpr, Args[0], Args[1], Loc);
      SelfExpr = nullptr;
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }

    // clear array, but don't reallocate
    if (MethodName == "clear" || MethodName == "reset") {
      if (NumArgs != 0) {
        ParseError(Loc, "`.reset` requires no arguments");
        delete this;
        return nullptr;
      }
      VExpression *e = new VDynArrayClear(SelfExpr, Loc);
      SelfExpr = nullptr;
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }

    if (MethodName == "sort") {
      if (NumArgs != 1) {
        ParseError(Loc, "`.sort` require delegate argument");
        delete this;
        return nullptr;
      }
      if (Args[0] && Args[0]->GetArgName() != NAME_None && Args[0]->GetArgName() != "lessfn") {
        ParseError(Loc, "`.sort` argument name must be `lessfn`");
        delete this;
        return nullptr;
      }
      if (Args[0]->IsDefaultArg()) {
        ParseError(Loc, "`.sort` argument name must not be default");
        delete this;
        return nullptr;
      }
      VExpression *e = new VDynArraySort(SelfExpr, Args[0], Loc);
      SelfExpr = nullptr;
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }

    if (MethodName == "swap") {
      if (NumArgs != 2) {
        ParseError(Loc, "`.swap` requires two arguments");
        delete this;
        return nullptr;
      }
      if (Args[0] && Args[0]->GetArgName() != NAME_None && Args[0]->GetArgName() == "idx0") {
        ParseError(Loc, "`.swap` 1st argument name must be `idx0`");
        delete this;
        return nullptr;
      }
      if (Args[1] && Args[1]->GetArgName() != NAME_None && Args[1]->GetArgName() == "idx1") {
        ParseError(Loc, "`.swap` 1st argument name must be `idx1`");
        delete this;
        return nullptr;
      }
      if (!Args[0] || Args[0]->IsDefaultArg() || !Args[1] || Args[1]->IsDefaultArg()) {
        ParseError(Loc, "`.swap` requires two arguments");
        delete this;
        return nullptr;
      }
      if (Args[0]->IsRefArg() || Args[0]->IsOutArg()) {
        ParseError(Loc, "`.swap` arguments cannot be `ref`");
        delete this;
        return nullptr;
      }
      if (Args[1]->IsRefArg() || Args[1]->IsOutArg()) {
        ParseError(Loc, "`.swap` arguments cannot be `out`");
        delete this;
        return nullptr;
      }
      VExpression *e = new VDynArraySwap1D(SelfExpr, Args[0], Args[1], Loc);
      SelfExpr = nullptr;
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }

    if (MethodName == "copyFrom") {
      if (NumArgs != 1 || !Args[0] || Args[0]->IsDefaultArg()) {
        ParseError(Loc, "`.copyFrom` requires one argument");
        delete this;
        return nullptr;
      }
      if (Args[0]->IsRefArg() || Args[0]->IsOutArg()) {
        ParseError(Loc, "`.copyFrom` arguments cannot be `ref`");
        delete this;
        return nullptr;
      }
      VExpression *e = new VDynArrayCopyFrom(SelfExpr, Args[0], Loc);
      SelfExpr = nullptr;
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }

    if (MethodName == "alloc") {
      if (NumArgs != 0) {
        ParseError(Loc, "`.alloc` should nave no arguments");
        delete this;
        return nullptr;
      }
      VExpression *e = new VDynArrayAllocElement(SelfExpr, Loc);
      SelfExpr = nullptr;
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }

    ParseError(Loc, "Invalid operation on dynamic array");
    delete this;
    return nullptr;
  }

  // Class.Method -- for static methods
  if (SelfExpr->Type.IsNormalOrPointerType(TYPE_Class)) {
    delete selfCopy;
    if (!SelfExpr->Type.Class) {
      ParseError(Loc, "Class name expected at the left side of `.`");
      delete this;
      return nullptr;
    }
    VMethod *M = SelfExpr->Type.Class->FindAccessibleMethod(MethodName, ec.SelfClass, &Loc);
    if (!M) {
      ParseError(Loc, "Method `%s` not found in class `%s`", *MethodName, SelfExpr->Type.Class->GetName());
      delete this;
      return nullptr;
    }
    //fprintf(stderr, "TYPE: %s\n", *SelfExpr->Type.GetName());
    if (M->Flags&FUNC_Iterator) {
      ParseError(Loc, "Iterator methods can only be used in foreach statements");
      delete this;
      return nullptr;
    }
    if ((M->Flags&FUNC_Static) == 0) {
      ParseError(Loc, "Only static methods can be called with this syntax");
      delete this;
      return nullptr;
    }
    // statics has no self
    VExpression *e = new VInvocation(nullptr, M, nullptr, false, false, Loc, NumArgs, Args);
    NumArgs = 0;
    delete this;
    return e->Resolve(ec);
  }

  // struct methods
  if (SelfExpr->Type.IsNormalOrPointerType(TYPE_Struct) || SelfExpr->Type.IsNormalOrPointerType(TYPE_Vector)) {
    VStruct *selfStruct = SelfExpr->Type.Struct;
    if (!selfStruct && SelfExpr->Type.IsNormalOrPointerType(TYPE_Vector)) selfStruct = VMemberBase::StaticFindTVec();
    if (!selfStruct) VCFatalError("VC: internal compiler error: no struct in `VDotInvocation::DoResolve()`");
    VMethod *M = selfStruct->FindAccessibleMethod(MethodName, /*SelfExpr->Type.Struct*/ec.SelfStruct, &Loc);
    // do not fail here, this could be a delegate invocation
    if (M) {
      if (SelfExpr->Type.Type != TYPE_Pointer) {
        if (SelfExpr->IsTypeExpr()) {
          if (!M->IsStatic()) ParseError(Loc, "cannot call non-static method `%s::%s` as static", *selfStruct->Name, *MethodName);
          // do not re-resolve
          delete selfCopy;
          selfCopy = nullptr;
        } else {
          // re-resolve with `&`
          delete SelfExpr;
          SelfExpr = nullptr;
          selfCopy = new VUnary(VUnary::TakeAddressForced, selfCopy, selfCopy->Loc);
        }
      } else {
        // do not re-resolve
        delete selfCopy;
        selfCopy = nullptr;
      }
      // need to re-resolve from the copy?
      if (!SelfExpr) {
        // yes
        vassert(selfCopy);
        SelfExpr = selfCopy->Resolve(ec);
        selfCopy = nullptr; // just in case
        if (!SelfExpr) { delete this; return nullptr; }
      } else {
        // no
        vassert(!selfCopy);
      }
      if (M->Flags&FUNC_Iterator) {
        ParseError(Loc, "Iterator methods can only be used in foreach statements");
        delete this;
        return nullptr;
      }
      //GLog.Logf(NAME_Debug, "compiling call to struct method `%s::%s`", *selfStruct->Name, *MethodName);
      VExpression *e = new VInvocation(SelfExpr, M, nullptr, true, false, Loc, NumArgs, Args);
      SelfExpr = nullptr;
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }
  }

  if (!SelfExpr->Type.IsNormalOrPointerType(TYPE_Reference)) {
    // translate method name for some built-in types
    if (SelfExpr->Type.IsNormalOrPointerType(TYPE_String)) {
      // string
      if (ec.OuterClass) {
        VName newname = ec.OuterClass->FindInPropMap(TYPE_String, MethodName);
        if (newname != NAME_None) {
          // i found her!
          MethodName = newname;
        }
      }
    } else if (SelfExpr->Type.IsNormalOrPointerType(TYPE_Name)) {
      // name
      if (ec.OuterClass) {
        VName newname = ec.OuterClass->FindInPropMap(TYPE_Name, MethodName);
        if (newname != NAME_None) {
          // i found her!
          MethodName = newname;
        }
      }
    } else if (SelfExpr->Type.IsNormalOrPointerType(TYPE_Float)) {
      // float
      if (VStr::ICmp(*MethodName, "isnan") == 0 ||
          VStr::ICmp(*MethodName, "isinf") == 0 ||
          VStr::ICmp(*MethodName, "isfinite") == 0)
      {
        if (NumArgs != 0) {
          ParseError(Loc, "`float` builtin `%s` cannot have args", *MethodName);
          delete selfCopy;
          delete this;
          return nullptr;
        }
        if (SelfExpr->Type.Type == TYPE_Pointer) selfCopy = new VPushPointed(selfCopy, selfCopy->Loc);
        VExpression *e = new VDotField(selfCopy, MethodName, Loc);
        delete this;
        return e->Resolve(ec);
      }
    } else if (SelfExpr->Type.IsNormalOrPointerType(TYPE_Dictionary)) {
      // dictionary
      if (MethodName == "clear" || MethodName == "reset") {
        if (!DoReResolvePtr(ec, selfCopy)) return nullptr;
        if (NumArgs != 0) {
          ParseError(Loc, "Dictionary builtin `%s` cannot have args", *MethodName);
          delete this;
          return nullptr;
        }
        VExpression *e = new VDictClearOrReset(SelfExpr, (MethodName == "clear"), Loc);
        SelfExpr = nullptr;
        delete this;
        return e->Resolve(ec);
      }
      if (MethodName == "find" || MethodName == "get") {
        if (!DoReResolvePtr(ec, selfCopy)) return nullptr;
        if (NumArgs != 1) {
          ParseError(Loc, "Dictionary builtin `%s` should have exactly one arg (key)", *MethodName);
          delete this;
          return nullptr;
        }
        VExpression *e = new VDictFind(SelfExpr, Args[0], Loc);
        SelfExpr = nullptr;
        NumArgs = 0;
        delete this;
        return e->Resolve(ec);
      }
      if (MethodName == "del" || MethodName == "delete" || MethodName == "remove") {
        if (!DoReResolvePtr(ec, selfCopy)) return nullptr;
        if (NumArgs != 1) {
          ParseError(Loc, "Dictionary builtin `%s` should have exactly one arg (key)", *MethodName);
          delete this;
          return nullptr;
        }
        VExpression *e = new VDictDelete(SelfExpr, Args[0], Loc);
        SelfExpr = nullptr;
        NumArgs = 0;
        delete this;
        return e->Resolve(ec);
      }
      if (MethodName == "put" || MethodName == "ins" || MethodName == "insert") {
        if (!DoReResolvePtr(ec, selfCopy)) return nullptr;
        if (NumArgs != 2) {
          ParseError(Loc, "Dictionary builtin `%s` should have two args (key and value)", *MethodName);
          delete this;
          return nullptr;
        }
        VExpression *e = new VDictPut(SelfExpr, Args[0], Args[1], Loc);
        SelfExpr = nullptr;
        NumArgs = 0;
        delete this;
        return e->Resolve(ec);
      }
      if (MethodName == "compact" || MethodName == "rehash") {
        if (!DoReResolvePtr(ec, selfCopy)) return nullptr;
        if (NumArgs != 0) {
          ParseError(Loc, "Dictionary builtin `%s` should have no args", *MethodName);
          delete this;
          return nullptr;
        }
        VExpression *e = new VDictRehash(SelfExpr, (MethodName == "compact"), Loc);
        SelfExpr = nullptr;
        NumArgs = 0;
        delete this;
        return e->Resolve(ec);
      }
      if (MethodName == "firstIndex") {
        if (!DoReResolvePtr(ec, selfCopy)) return nullptr;
        if (NumArgs != 0) {
          ParseError(Loc, "Dictionary builtin `%s` cannot have args", *MethodName);
          delete this;
          return nullptr;
        }
        VExpression *e = new VDictFirstIndex(SelfExpr, Loc);
        SelfExpr = nullptr;
        delete this;
        return e->Resolve(ec);
      }
      if (MethodName == "isValidIndex") {
        if (!DoReResolvePtr(ec, selfCopy)) return nullptr;
        if (NumArgs != 1) {
          ParseError(Loc, "Dictionary builtin `%s` should have one arg", *MethodName);
          delete this;
          return nullptr;
        }
        VExpression *e = new VDictIsValidIndex(SelfExpr, Args[0], Loc);
        SelfExpr = nullptr;
        NumArgs = 0;
        delete this;
        return e->Resolve(ec);
      }
      if (MethodName == "nextIndex" || MethodName == "removeAndNextIndex") {
        if (!DoReResolvePtr(ec, selfCopy)) return nullptr;
        if (NumArgs != 1) {
          ParseError(Loc, "Dictionary builtin `%s` should have one arg", *MethodName);
          delete this;
          return nullptr;
        }
        VExpression *e = new VDictNextIndex(SelfExpr, Args[0], (MethodName == "removeAndNextIndex"), Loc);
        SelfExpr = nullptr;
        NumArgs = 0;
        delete this;
        return e->Resolve(ec);
      }
      if (MethodName == "keyAtIndex") {
        if (!DoReResolvePtr(ec, selfCopy)) return nullptr;
        if (NumArgs != 1) {
          ParseError(Loc, "Dictionary builtin `%s` should have one arg", *MethodName);
          delete this;
          return nullptr;
        }
        VExpression *e = new VDictKeyAtIndex(SelfExpr, Args[0], Loc);
        SelfExpr = nullptr;
        NumArgs = 0;
        delete this;
        return e->Resolve(ec);
      }
      if (MethodName == "valueAtIndex") {
        if (!DoReResolvePtr(ec, selfCopy)) return nullptr;
        if (NumArgs != 1) {
          ParseError(Loc, "Dictionary builtin `%s` should have one arg", *MethodName);
          delete this;
          return nullptr;
        }
        VExpression *e = new VDictValueAtIndex(SelfExpr, Args[0], Loc);
        SelfExpr = nullptr;
        NumArgs = 0;
        delete this;
        return e->Resolve(ec);
      }
    } else if (SelfExpr->Type.IsNormalOrPointerType(TYPE_Struct) || SelfExpr->Type.IsNormalOrPointerType(TYPE_Vector)) {
      // each struct/vector has `zero()` method
      if (MethodName == "zero") {
        if (!DoReResolvePtr(ec, selfCopy)) return nullptr;
        if (NumArgs != 0) {
          ParseError(Loc, "`.zero` requires no arguments");
          delete this;
          return nullptr;
        }
        VExpression *e = new VStructZero(SelfExpr, Loc);
        SelfExpr = nullptr;
        NumArgs = 0;
        delete this;
        return e->Resolve(ec);
      }
      // try delegate
      if (SelfExpr->Type.IsNormalOrPointerType(TYPE_Struct)) {
        VField *field = SelfExpr->Type.Struct->FindField(MethodName);
        if (field && field->Type.Type == TYPE_Delegate) {
          if (!ec.SelfClass) {
            ParseError(Loc, "static delegates aren't supported");
            delete this;
            return nullptr;
          }
          //if (!DoReResolvePtr(ec, selfCopy)) return nullptr;
          if (SelfExpr->Type.Type != TYPE_Pointer) SelfExpr->RequestAddressOf();
          VExpression *fldAccess = new VFieldAccess(SelfExpr, field, Loc, 0);
          fldAccess->RequestAddressOf();
          VExpression *xe = new VSelfClass(Loc);
          if (xe) xe = xe->Resolve(ec);
          if (xe) {
            VInvocation *e = new VInvocation(xe, fldAccess, field->Func, Loc, NumArgs, Args);
            SelfExpr = nullptr;
            NumArgs = 0;
            delete this;
            return e->Resolve(ec);
          } else {
            delete this;
            return nullptr;
          }
        }
      }
    }
    // try UFCS
    if (NumArgs+1 <= VMethod::MAX_PARAMS) {
      int newArgC = NumArgs+1;
      VExpression *ufcsArgs[VMethod::MAX_PARAMS+1];
      for (int f = 0; f < NumArgs; ++f) ufcsArgs[f+1] = Args[f];
      //if (SelfExpr->Type.Type == TYPE_Pointer) selfCopy = new VPushPointed(selfCopy, selfCopy->Loc);
      //ufcsArgs[0] = selfCopy;
      ufcsArgs[0] = SelfExpr;
      if (VInvocation::FindMethodWithSignature(ec, MethodName, newArgC, ufcsArgs)) {
        VCastOrInvocation *call = new VCastOrInvocation(MethodName, Loc, newArgC, ufcsArgs);
        // don't delete `SelfExpr`, it is used; but delete `selfCopy`
        SelfExpr = nullptr;
        delete selfCopy;
        NumArgs = 0; // also, don't delete args
        delete this;
        return call->Resolve(ec);
      }
      // if `SelfExpr` is a pointer, try UFCS with dereferenced pointer
      if (SelfExpr->Type.Type == TYPE_Pointer) {
        selfCopy = new VPushPointed(selfCopy, selfCopy->Loc);
        ufcsArgs[0] = selfCopy;
        if (VInvocation::FindMethodWithSignature(ec, MethodName, newArgC, ufcsArgs)) {
          VCastOrInvocation *call = new VCastOrInvocation(MethodName, Loc, newArgC, ufcsArgs);
          // don't delete `selfCopy`, it is used
          NumArgs = 0; // also, don't delete args
          delete this;
          return call->Resolve(ec);
        }
      }
    }
    //GLog.Logf(NAME_Debug, "SelfExpr(%s)=%s (%d)", *SelfExpr->Type.GetName(), *SelfExpr->toString(), SelfExpr->Type.Type);
    ParseError(Loc, "Object reference expected at the left side of `.` (0)");
    delete selfCopy;
    delete this;
    return nullptr;
  }

  VField *field = SelfExpr->Type.Class->FindField(MethodName, Loc, ec.SelfClass);
  if (field && field->Type.Type == TYPE_Delegate) {
    // don't need it anymore
    //delete selfCopy;
    if (!DoReResolvePtr(ec, selfCopy)) return nullptr;
    VExpression *e = new VInvocation(SelfExpr, field->Func, field, true, false, Loc, NumArgs, Args);
    SelfExpr = nullptr;
    NumArgs = 0;
    delete this;
    return e->Resolve(ec);
  }

  VMethod *M = SelfExpr->Type.Class->FindAccessibleMethod(MethodName, ec.SelfClass, &Loc);
  if (M) {
    /*
    if (SelfExpr->Type.Class && SelfExpr->Type.Class != ec.SelfClass && !SelfExpr->Type.Class->Defined) {
      fprintf(stderr, "!!!! <%s>\n", SelfExpr->Type.Class->GetName());
      SelfExpr->Type.Class->Define();
    }
    fprintf(stderr, "!!!! CALL: <%s>\n", *M->GetFullName());
    */
    // don't need it anymore
    //delete selfCopy;
    if (!DoReResolvePtr(ec, selfCopy)) return nullptr;
    if (M->Flags&FUNC_Iterator) {
      ParseError(Loc, "Iterator methods can only be used in foreach statements");
      delete this;
      return nullptr;
    }
    VExpression *e = new VInvocation(SelfExpr, M, nullptr, true, false, Loc, NumArgs, Args);
    SelfExpr = nullptr;
    NumArgs = 0;
    delete this;
    return e->Resolve(ec);
  }

  // try UFCS
  if (NumArgs+1 <= VMethod::MAX_PARAMS) {
    int newArgC = NumArgs+1;
    VExpression *ufcsArgs[VMethod::MAX_PARAMS+1];
    for (int f = 0; f < NumArgs; ++f) ufcsArgs[f+1] = Args[f];
    if (SelfExpr->Type.Type == TYPE_Pointer) selfCopy = new VPushPointed(selfCopy, selfCopy->Loc);
    ufcsArgs[0] = selfCopy;
    if (VInvocation::FindMethodWithSignature(ec, MethodName, newArgC, ufcsArgs)) {
      VCastOrInvocation *call = new VCastOrInvocation(MethodName, Loc, newArgC, ufcsArgs);
      // don't delete `selfCopy`, it is used
      NumArgs = 0; // also, don't delete args
      delete this;
      return call->Resolve(ec);
    }
  }

  // don't need it anymore
  delete selfCopy;

  if (!(SelfExpr->Type.Class->ObjectFlags&CLASSOF_PostLoaded)) {
    ParseError(Loc, "No such method `%s` in class `%s` (that class is not post-loaded yet!)", *MethodName, SelfExpr->Type.Class->GetName());
  } else {
    ParseError(Loc, "No such method `%s` in class `%s`", *MethodName, SelfExpr->Type.Class->GetName());
  }
  delete this;
  return nullptr;
}


//==========================================================================
//
//  VDotInvocation::ResolveIterator
//
//==========================================================================
VExpression *VDotInvocation::ResolveIterator (VEmitContext &ec) {
  if (SelfExpr) SelfExpr = SelfExpr->Resolve(ec);
  if (!SelfExpr) {
    delete this;
    return nullptr;
  }

  if (SelfExpr->Type.Type != TYPE_Reference) {
    ParseError(Loc, "Object reference expected at the left side of `.`");
    delete this;
    return nullptr;
  }

  VMethod *M = SelfExpr->Type.Class->FindAccessibleMethod(MethodName, ec.SelfClass, &Loc);
  if (!M) {
    ParseError(Loc, "No such method %s in class `%s` (iterator resolving)", *MethodName, SelfExpr->Type.Class->GetName());
    delete this;
    return nullptr;
  }
  if ((M->Flags&FUNC_Iterator) == 0) {
    ParseError(Loc, "`%s` is not an iterator method", *MethodName);
    delete this;
    return nullptr;
  }

  VExpression *e = new VInvocation(SelfExpr, M, nullptr, true, false, Loc, NumArgs, Args);
  SelfExpr = nullptr;
  NumArgs = 0;
  delete this;
  return e->Resolve(ec);
}


//==========================================================================
//
//  VDotInvocation::Emit
//
//==========================================================================
void VDotInvocation::Emit (VEmitContext &) {
  ParseError(Loc, "Should not happen (VDotInvocation)");
}


//==========================================================================
//
//  VDotInvocation::GetVMethod
//
//==========================================================================
VMethod *VDotInvocation::GetVMethod (VEmitContext &ec) {
  if (!ec.SelfClass || !SelfExpr) return nullptr;

  VGagErrors gag;

  VExpression *eself = SelfExpr->SyntaxCopy()->Resolve(ec);
  if (!eself) return nullptr;

  if (eself->Type.Type == TYPE_Reference || eself->Type.Type == TYPE_Class) {
    VMethod *res = eself->Type.Class->FindAccessibleMethod(MethodName, ec.SelfClass, &Loc);
    delete eself;
    return res;
  }

  delete eself;
  return nullptr;
}


//==========================================================================
//
//  VDotInvocation::IsMethodNameChangeable
//
//==========================================================================
bool VDotInvocation::IsMethodNameChangeable () const {
  return true;
}


//==========================================================================
//
//  VDotInvocation::GetMethodName
//
//==========================================================================
VName VDotInvocation::GetMethodName () const {
  return MethodName;
}


//==========================================================================
//
//  VDotInvocation::SetMethodName
//
//==========================================================================
void VDotInvocation::SetMethodName (VName aname) {
  MethodName = aname;
}


//==========================================================================
//
//  VDotInvocation::toString
//
//==========================================================================
VStr VDotInvocation::toString () const {
  return e2s(SelfExpr)+"."+VStr(MethodName)+args2str();
}



//==========================================================================
//
//  VTypeInvocation::VTypeInvocation
//
//==========================================================================
VTypeInvocation::VTypeInvocation (VExpression *aTypeExpr, VName aMethodName, const TLocation &aloc, int argc, VExpression **argv)
  : VInvocationBase(argc, argv, aloc)
  , TypeExpr(aTypeExpr)
  , MethodName(aMethodName)
{
}


//==========================================================================
//
//  VTypeInvocation::~VTypeInvocation
//
//==========================================================================
VTypeInvocation::~VTypeInvocation () {
  delete TypeExpr; TypeExpr = nullptr;
}


//==========================================================================
//
//  VTypeInvocation::SyntaxCopy
//
//==========================================================================
VExpression *VTypeInvocation::SyntaxCopy () {
  auto res = new VTypeInvocation();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VTypeInvocation::~VTypeInvocation
//
//==========================================================================
void VTypeInvocation::DoSyntaxCopyTo (VExpression *e) {
  VInvocationBase::DoSyntaxCopyTo(e);
  auto res = (VTypeInvocation *)e;
  res->TypeExpr = (TypeExpr ? TypeExpr->SyntaxCopy() : nullptr);
  res->MethodName = MethodName;
}


//==========================================================================
//
//  VTypeInvocation::DoResolve
//
//==========================================================================
VExpression *VTypeInvocation::DoResolve (VEmitContext &ec) {
  if (!TypeExpr) return nullptr;
  TypeExpr = TypeExpr->ResolveAsType(ec);
  if (!TypeExpr || !TypeExpr->IsTypeExpr()) {
    ParseError(Loc, "Type expected");
    delete this;
    return nullptr;
  }

  // `int` properties
  if (TypeExpr->Type.Type == TYPE_Int) {
    if (MethodName == "min") {
      if (NumArgs != 0) { ParseError(Loc, "`int.%s` cannot have arguments", *MethodName); delete this; return nullptr; }
      VExpression *e = new VIntLiteral((int)0x80000000, Loc);
      if (e) e = e->Resolve(ec);
      delete this;
      return e;
    }
    if (MethodName == "max") {
      if (NumArgs != 0) { ParseError(Loc, "`int.%s` cannot have arguments", *MethodName); delete this; return nullptr; }
      VExpression *e = new VIntLiteral((int)0x7fffffff, Loc);
      if (e) e = e->Resolve(ec);
      delete this;
      return e;
    }
    ParseError(Loc, "invalid `int` property `%s`", *MethodName);
    delete this;
    return nullptr;
  }

  // `float` properties
  if (TypeExpr->Type.Type == TYPE_Float) {
    if (MethodName == "min") {
      if (NumArgs != 0) { ParseError(Loc, "`float.%s` cannot have arguments", *MethodName); delete this; return nullptr; }
      VExpression *e = new VFloatLiteral(-FLT_MAX, Loc);
      if (e) e = e->Resolve(ec);
      delete this;
      return e;
    }
    if (MethodName == "min_int") {
      if (NumArgs != 0) { ParseError(Loc, "`float.%s` cannot have arguments", *MethodName); delete this; return nullptr; }
      VExpression *e = new VFloatLiteral(-0x3fffffff, Loc);
      if (e) e = e->Resolve(ec);
      delete this;
      return e;
    }
    if (MethodName == "max") {
      if (NumArgs != 0) { ParseError(Loc, "`float.%s` cannot have arguments", *MethodName); delete this; return nullptr; }
      VExpression *e = new VFloatLiteral(FLT_MAX, Loc);
      if (e) e = e->Resolve(ec);
      delete this;
      return e;
    }
    if (MethodName == "max_int") {
      if (NumArgs != 0) { ParseError(Loc, "`float.%s` cannot have arguments", *MethodName); delete this; return nullptr; }
      VExpression *e = new VFloatLiteral(0x3fffffff, Loc); /* 2147483520 */
      if (e) e = e->Resolve(ec);
      delete this;
      return e;
    }
    if (MethodName == "min_norm" || MethodName == "min_normal" || MethodName == "min_normalized") {
      if (NumArgs != 0) { ParseError(Loc, "`float.%s` cannot have arguments", *MethodName); delete this; return nullptr; }
      VExpression *e = new VFloatLiteral(FLT_MIN, Loc);
      if (e) e = e->Resolve(ec);
      delete this;
      return e;
    }
    if (MethodName == "nan") {
      if (NumArgs != 0) { ParseError(Loc, "`float.%s` cannot have arguments", *MethodName); delete this; return nullptr; }
      VExpression *e = new VFloatLiteral(NAN, Loc);
      if (e) e = e->Resolve(ec);
      delete this;
      return e;
    }
    if (MethodName == "inf" || MethodName == "infinity") {
      if (NumArgs != 0) { ParseError(Loc, "`float.%s` cannot have arguments", *MethodName); delete this; return nullptr; }
      VExpression *e = new VFloatLiteral(INFINITY, Loc);
      if (e) e = e->Resolve(ec);
      delete this;
      return e;
    }
    ParseError(Loc, "invalid `float` property `%s`", *MethodName);
    delete this;
    return nullptr;
  }

  // `string` properties
  if (TypeExpr->Type.Type == TYPE_String) {
    VClass *cls = VClass::FindClass("Object");
    if (cls) {
      const char *newMethod = nullptr;
           if (MethodName == "repeat") newMethod = "strrepeat";
      else if (MethodName == "fromChar") newMethod = "strFromChar";
      else if (MethodName == "fromCharUtf8") newMethod = "strFromCharUtf8";
      else if (MethodName == "fromInt") newMethod = "strFromInt";
      else if (MethodName == "fromFloat") newMethod = "strFromFloat";
      if (newMethod) {
        // convert to invocation
        VExpression *e = new VInvocation(nullptr, cls->FindMethodChecked(newMethod), nullptr, false, false, Loc, NumArgs, Args);
        NumArgs = 0; // don't clear args
        e = e->Resolve(ec);
        delete this;
        return e;
      }
    }
    ParseError(Loc, "invalid `string` property `%s`", *MethodName);
    delete this;
    return nullptr;
  }

  // `name` properties
  if (TypeExpr->Type.Type == TYPE_Name) {
    if (MethodName == "none" || MethodName == "empty") {
      if (NumArgs != 0) { ParseError(Loc, "`name.%s` cannot have arguments", *MethodName); delete this; return nullptr; }
      VExpression *e = new VNameLiteral(NAME_None, Loc);
      if (e) e = e->Resolve(ec);
      delete this;
      return e;
    }
    ParseError(Loc, "invalid `name` property `%s`", *MethodName);
    delete this;
    return nullptr;
  }

  ParseError(Loc, "invalid `%s` property `%s`", *TypeExpr->Type.GetName(), *MethodName);
  delete this;
  return nullptr;
}


//==========================================================================
//
//  VTypeInvocation::Emit
//
//==========================================================================
void VTypeInvocation::Emit (VEmitContext &) {
  ParseError(Loc, "Should not happen (VTypeInvocation)");
}


//==========================================================================
//
//  VTypeInvocation::GetVMethod
//
//==========================================================================
VMethod *VTypeInvocation::GetVMethod (VEmitContext &/*ec*/) {
  return nullptr;
}


//==========================================================================
//
//  VTypeInvocation::IsMethodNameChangeable
//
//==========================================================================
bool VTypeInvocation::IsMethodNameChangeable () const {
  return false;
}


//==========================================================================
//
//  VTypeInvocation::GetMethodName
//
//==========================================================================
VName VTypeInvocation::GetMethodName () const {
  return MethodName;
}


//==========================================================================
//
//  VTypeInvocation::SetMethodName
//
//==========================================================================
void VTypeInvocation::SetMethodName (VName aname) {
  MethodName = aname;
}


//==========================================================================
//
//  VTypeInvocation::toString
//
//==========================================================================
VStr VTypeInvocation::toString () const {
  return e2s(TypeExpr)+"."+VStr(MethodName)+args2str();
}



//==========================================================================
//
//  VInvocation::VInvocation
//
//==========================================================================
VInvocation::VInvocation (VExpression *ASelfExpr, VMethod *AFunc, VField *ADelegateField,
                          bool AHaveSelf, bool ABaseCall, const TLocation &ALoc, int ANumArgs,
                          VExpression **AArgs)
  : VInvocationBase(ANumArgs, AArgs, ALoc)
  , SelfExpr(ASelfExpr)
  , Func(AFunc)
  , DelegateField(ADelegateField)
  , DelegateLocal(-666)
  , HaveSelf(AHaveSelf)
  , BaseCall(ABaseCall)
  , CallerState(nullptr)
  , DgPtrExpr(nullptr)
  , OverrideBuiltin(-1)
{
}


//==========================================================================
//
//  VInvocation::VInvocation
//
//==========================================================================
VInvocation::VInvocation (VMethod *AFunc, int ADelegateLocal, const TLocation &ALoc, int ANumArgs, VExpression **AArgs)
  : VInvocationBase(ANumArgs, AArgs, ALoc)
  , SelfExpr(nullptr)
  , Func(AFunc)
  , DelegateField(nullptr)
  , DelegateLocal(ADelegateLocal)
  , HaveSelf(false)
  , BaseCall(false)
  , CallerState(nullptr)
  , DgPtrExpr(nullptr)
  , OverrideBuiltin(-1)
{
}


//==========================================================================
//
//  VInvocation::VInvocation
//
//==========================================================================
VInvocation::VInvocation (VExpression *ASelfExpr, VExpression *ADgPtrExpr, VMethod *AFunc, const TLocation &ALoc, int ANumArgs, VExpression **AArgs)
  : VInvocationBase(ANumArgs, AArgs, ALoc)
  , SelfExpr(ASelfExpr)
  , Func(AFunc)
  , DelegateField(nullptr)
  , DelegateLocal(-666)
  , HaveSelf(true)
  , BaseCall(false)
  , CallerState(nullptr)
  , DgPtrExpr(ADgPtrExpr)
  , OverrideBuiltin(-1)
{
}


//==========================================================================
//
//  VInvocation::~VInvocation
//
//==========================================================================
VInvocation::~VInvocation() {
  if (SelfExpr) { delete SelfExpr; SelfExpr = nullptr; }
  if (DgPtrExpr) { delete DgPtrExpr; DgPtrExpr = nullptr; }
}


//==========================================================================
//
//  VInvocation::SyntaxCopy
//
//==========================================================================
VExpression *VInvocation::SyntaxCopy () {
  auto res = new VInvocation();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VCastOrInvocation::DoSyntaxCopyTo
//
//==========================================================================
void VInvocation::DoSyntaxCopyTo (VExpression *e) {
  VInvocationBase::DoSyntaxCopyTo(e);
  auto res = (VInvocation *)e;
  // no need to copy private fields
  res->SelfExpr = (SelfExpr ? SelfExpr->SyntaxCopy() : nullptr);
  res->Func = Func;
  res->DelegateField = DelegateField;
  res->DelegateLocal = DelegateLocal;
  res->HaveSelf = HaveSelf;
  res->BaseCall = BaseCall;
  res->CallerState = CallerState;
  res->DgPtrExpr = (DgPtrExpr ? DgPtrExpr->SyntaxCopy() : nullptr);
  res->OverrideBuiltin = OverrideBuiltin;
}


//==========================================================================
//
//  VInvocation::RebuildArgs
//
//==========================================================================
bool VInvocation::RebuildArgs () {
  if (NumArgs == 0) return true;
  int maxParams = (Func->Flags&FUNC_VarArgs ? VMethod::MAX_PARAMS-1 : Func->NumParams);
  if (NumArgs > maxParams) {
    ParseError(Loc, "Too many method arguments");
    return false;
  }
  VExpression *newArgs[VMethod::MAX_PARAMS+1];
  bool isSet[VMethod::MAX_PARAMS+1];
  for (int f = 0; f < VMethod::MAX_PARAMS+1; ++f) { newArgs[f] = nullptr; isSet[f] = false; }
  int firstNamed = -1;
  for (int f = 0; f < NumArgs; ++f) {
    if (Args[f] && Args[f]->IsNamedArg()) {
      firstNamed = f;
      break;
    }
    newArgs[f] = Args[f];
    isSet[f] = true;
  }
  if (firstNamed < 0) return true; // nothing to do
  // do actual rebuild
  int newNumArgs = firstNamed;
  int newIdx = -1;
  for (; firstNamed < NumArgs; ++firstNamed) {
    if (Args[firstNamed] && Args[firstNamed]->IsNamedArg()) {
      // named
      int realIdx = Func->FindArgByName(Args[firstNamed]->GetArgName());
      //HACK: ending underscore means "nobody cares"
      if (realIdx < 0 && firstNamed < Func->NumParams) {
        const char *ss = *Func->Params[firstNamed].Name;
        if (ss[0] && ss[strlen(ss)-1] == '_') realIdx = firstNamed;
      }
      if (realIdx < 0) {
        if (realIdx < -1) {
          ParseError(Args[firstNamed]->Loc, "there are several arguments named `%s`, differs only in case", *Args[firstNamed]->GetArgName());
        } else {
          ParseError(Args[firstNamed]->Loc, "no argument named `%s`", *Args[firstNamed]->GetArgName());
        }
        return false;
      }
      if (isSet[realIdx]) {
        ParseError(Args[firstNamed]->Loc, "duplicate argument named `%s`", *Args[firstNamed]->GetArgName());
        return false;
      }
      newIdx = realIdx;
    } else {
      // unnamed
      if (newIdx < 0) VCFatalError("VC: internal compiler error (VInvocation::RebuildArgs)");
      if (newIdx >= maxParams) {
        ParseError(Loc, "Too many method arguments");
        return false;
      }
      if (isSet[newIdx]) {
        ParseError((Args[firstNamed] ? Args[firstNamed]->Loc : Loc), "duplicate argument #%d", newIdx+1);
        return false;
      }
    }
    isSet[newIdx] = true;
    newArgs[newIdx] = Args[firstNamed];
    ++newIdx;
    if (newIdx > newNumArgs) newNumArgs = newIdx;
  }
  // copy new list
  NumArgs = newNumArgs;
  for (int f = 0; f < newNumArgs; ++f) Args[f] = newArgs[f];
  return true;
}


//==========================================================================
//
//  VInvocation::DoResolve
//
//==========================================================================
VExpression *VInvocation::DoResolve (VEmitContext &ec) {
       if (Func && (Func->Flags&FUNC_Decorate) != 0) CheckDecorateParams(ec);
  else if (ec.Package->Name == NAME_decorate) CheckDecorateParams(ec);

  if (DelegateLocal >= 0) {
    //FIXME
    const VLocalVarDef &loc = ec.GetLocalByIndex(DelegateLocal);
    if (loc.ParamFlags&(FPARM_Out|FPARM_Ref)) {
      ParseError(Loc, "ref locals aren't supported yet (sorry)");
      delete this;
      return nullptr;
    }
  }

  if (DgPtrExpr) {
    DgPtrExpr = DgPtrExpr->Resolve(ec);
    if (!DgPtrExpr) {
      delete this;
      return nullptr;
    }
  }

  // check for virutal method call
  if (ec.VCallsDisabled) {
    bool DirectCall = (BaseCall || (Func->Flags&FUNC_Final) != 0);
    if (DirectCall || DelegateField || DelegateLocal >= 0 || DgPtrExpr) {
      // not a virtual call
    } else {
      ParseError(Loc, "calling virtual method is disabled in current context");
      delete this;
      return nullptr;
    }
  }

  for (int f = 0; f < VMethod::MAX_PARAMS; ++f) optmarshall[f] = -1;

  // resolve arguments
  int requiredParams = Func->NumParams;
  int maxParams = (Func->Flags&FUNC_VarArgs ? VMethod::MAX_PARAMS-1 : Func->NumParams);
  bool ArgsOk = RebuildArgs();
  if (!ArgsOk) { delete this; return nullptr; }
  for (int i = 0; i < NumArgs; ++i) {
    if (i >= maxParams) {
      ParseError((Args[i] ? Args[i]->Loc : Loc), "Too many method arguments");
      ArgsOk = false;
      break;
    }
    if (Args[i] && Args[i]->IsDefaultArg()) {
      // unpack it, why not
      delete Args[i];
      Args[i] = nullptr;
    }
    if (Args[i]) {
      // check for `ref`/`out` validness
      if (Args[i]->IsRefArg() && (i >= requiredParams || (Func->ParamFlags[i]&FPARM_Ref) == 0)) {
        ParseError(Args[i]->Loc, "`ref` argument for non-ref parameter #%d", i+1);
        ArgsOk = false;
        break;
      }
      if (Args[i]->IsOutArg() && (i >= requiredParams || (Func->ParamFlags[i]&FPARM_Out) == 0)) {
        ParseError(Args[i]->Loc, "`out` argument for non-out parameter #%d", i+1);
        ArgsOk = false;
        break;
      }
      /*
      if (i < requiredParams && (Func->ParamFlags[i]|(FPARM_Ref|FPARM_Out))) {
        GLog.Logf(NAME_Debug, "MT:<%s>: arg#%d: pt=%s; at=%s (%s)", *Func->GetFullName(), i+1, *Func->ParamTypes[i].GetName(), *Args[i]->Type.GetName(), *Args[i]->toString());
      }
      */
      // need to resolve it before determining marshal `specified_` var
      // but the type may be changed by `Resolve()`, so save the flag here
      const bool wantOptMarshal = Args[i]->IsOptMarshallArg();
      // this can be already resolved if we're in `VPropertyAssign`
      if (!Args[i]->IsResolved()) Args[i] = Args[i]->Resolve(ec);
      if (!Args[i]) { ArgsOk = false; break; }
      // check for struct copying
      if ((Func->ParamFlags[i]&(FPARM_Ref|FPARM_Out)) == 0 &&
          Args[i]->Type.Type == TYPE_Struct &&
          Args[i]->Type.Struct->NoAssign)
      {
        ParseError(Args[i]->Loc, "cannot pass noassign struct by value for parameter #%d", i+1);
        ArgsOk = false;
        break;
      }
      // marshaling
      if (wantOptMarshal && i < VMethod::MAX_PARAMS &&
          Args[i]->IsLocalVarExpr() && (Func->ParamFlags[i]&FPARM_Optional) != 0)
      {
        // setup marshaling
        VLocalVar *ve = (VLocalVar *)Args[i];
        const VLocalVarDef &L = ec.GetLocalByIndex(ve->num);
        // can be unnamed
        if (L.Name != NAME_None) {
          VStr spname = VStr("specified_")+(*L.Name);
          const int lidx = ec.CheckForLocalVar(VName(*spname));
          if (lidx >= 0) {
            const VLocalVarDef &LL = ec.GetLocalByIndex(lidx);
            // allow ints and local bools
            if (LL.Type.Type == TYPE_Int || LL.Type.Type == TYPE_Bool) {
              optmarshall[i] = lidx;
            } else {
              // bad type
              ParseError(Args[i]->Loc, "`!optional` marshaling for parameter #%d got `specified_%s` of type `%s` instead of int/bool", i+1, *L.Name, *LL.Type.GetName());
              ArgsOk = false;
              break;
            }
          }
        }
      }
      //GLog.Logf(NAME_Debug, "%s: ...#%d: <%s>", *Args[i]->Loc.toStringNoCol(), i, *Args[i]->toString());
    }
  }

  if (!ArgsOk) {
    delete this;
    return nullptr;
  }

  // this also adds argc to vararg functions
  CheckParams(ec);

  Type = Func->ReturnType;
  if (Type.Type == TYPE_Byte || Type.Type == TYPE_Bool) Type = VFieldType(TYPE_Int);
  if (Func->Flags&FUNC_Spawner) Type.Class = Args[0]->Type.Class;

  // we may create new locals, so activate local reuse mechanics
  for (int f = 0; f < VMethod::MAX_PARAMS; ++f) lcidx[f] = -1;

  // for omited `optional ref`/`optional out`, create temporary locals
  for (int i = 0; i < NumArgs; ++i) {
    if (!Args[i] && i < VMethod::MAX_PARAMS) {
      if ((Func->ParamFlags[i]&(FPARM_Out|FPARM_Ref)) != 0) {
        // create temporary
        VLocalVarDef &L = ec.NewLocal(NAME_None, Func->ParamTypes[i], VEmitContext::Unsafe, Loc);
        L.Visible = false; // it is unnamed, and hidden ;-)
        lcidx[i] = L.GetIndex();
      }
    }
  }
  SetResolved();

  // some special functions will be converted to builtins, try to const-optimize 'em
  if (Func->builtinOpc >= 0) {
    #if 0
    VExpression *e = OptimizeBuiltin(ec);
    GLog.Logf(NAME_Debug, "VInvocation::OptimizeBuiltin:res: <%s>", StatementBuiltinInfo[Func->builtinOpc].name);
    return e;
    #else
    return OptimizeBuiltin(ec);
    #endif
  }
  return this;
}


//==========================================================================
//
//  VInvocation::CheckSimpleConstArgs
//
//  used by `OptimizeBuiltin`; `types` are `TYPE_xxx`
//
//==========================================================================
bool VInvocation::CheckSimpleConstArgs (const int argc, const int *types, const bool exactArgC) const {
  if (exactArgC) {
    if (argc != NumArgs) return false;
  } else {
    if (argc > NumArgs) return false;
  }
  for (int f = 0; f < argc; ++f) {
    if (!Args[f]) return false; // cannot omit anything (yet)
    switch (types[f]) {
      case TYPE_Int: if (!Args[f]->IsIntConst()) return false; break;
      case TYPE_Float: if (!Args[f]->IsFloatConst()) return false; break;
      case TYPE_String: if (!Args[f]->IsStrConst()) return false; break;
      case TYPE_Name: if (!Args[f]->IsNameConst()) return false; break;
      case TYPE_Vector: if (!Args[f]->IsConstVectorCtor()) return false; break;
      default: return false; // unknown request
    }
  }
  return true;
}


//==========================================================================
//
//  ExtractSwizzleXY
//
//  if `e` is `vec.xy`, extract `e` operand, and delete `e`
//
//==========================================================================
static VExpression *ExtractSwizzleXY (VExpression *e) {
  //return nullptr;
  if (!e || !e->IsSwizzle()) return nullptr;
  VVectorSwizzleExpr *swe = (VVectorSwizzleExpr *)e;
  if (!swe->op && !swe->IsSwizzleXY()) return nullptr;
  VExpression *res = swe->op;
  swe->op = nullptr;
  delete swe;
  return res;
}


//==========================================================================
//
//  VInvocation::OptimizeBuiltin
//
//==========================================================================
VExpression *VInvocation::OptimizeBuiltin (VEmitContext &ec) {
  OverrideBuiltin = -1; // just in case
  if (!Func || Func->builtinOpc < 0) return this; // sanity check
  //GLog.Logf(NAME_Debug, "%s: VInvocation::OptimizeBuiltin: <%s>", *Loc.toStringNoCol(), StatementBuiltinInfo[Func->builtinOpc].name);
  TVec v0(0, 0, 0), v1(0, 0, 0), v2(0, 0, 0);
  float fv, fv2, fvres;
  VExpression *e = nullptr;
  switch (Func->builtinOpc) {
    case OPC_Builtin_IntAbs:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Int})) return this;
      e = new VIntLiteral((Args[0]->GetIntConst() < 0 ? -Args[0]->GetIntConst() : Args[0]->GetIntConst()), Loc);
      break;
    case OPC_Builtin_FloatAbs:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      fv = Args[0]->GetFloatConst();
      if (isNaNF(fv)) return this;
      e = new VFloatLiteral(fabsf(fv), Loc);
      break;
    case OPC_Builtin_IntSign:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Int})) return this;
      e = new VIntLiteral((Args[0]->GetIntConst() < 0 ? -1 : Args[0]->GetIntConst() > 0 ? 1 : 0), Loc);
      break;
    case OPC_Builtin_FloatSign:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      fv = Args[0]->GetFloatConst();
      if (isNaNF(fv)) return this;
      e = new VFloatLiteral((fv < 0.0f ? -1.0f : fv > 0.0f ? 1.0f : 0.0f), Loc);
      break;
    case OPC_Builtin_IntMin:
      if (!CheckSimpleConstArgs(2, (const int []){TYPE_Int, TYPE_Int})) return this;
      e = new VIntLiteral((Args[0]->GetIntConst() < Args[1]->GetIntConst() ? Args[0]->GetIntConst() : Args[1]->GetIntConst()), Loc);
      break;
    case OPC_Builtin_IntMax:
      if (!CheckSimpleConstArgs(2, (const int []){TYPE_Int, TYPE_Int})) return this;
      e = new VIntLiteral((Args[0]->GetIntConst() > Args[1]->GetIntConst() ? Args[0]->GetIntConst() : Args[1]->GetIntConst()), Loc);
      break;
    case OPC_Builtin_FloatMin:
      if (!CheckSimpleConstArgs(2, (const int []){TYPE_Float, TYPE_Float})) return this;
      fv = Args[0]->GetFloatConst();
      if (isNaNF(fv)) return this;
      fv2 = Args[1]->GetFloatConst();
      if (isNaNF(fv2)) return this;
      e = new VFloatLiteral(min2(fv, fv2), Loc);
      break;
    case OPC_Builtin_FloatMax:
      if (!CheckSimpleConstArgs(2, (const int []){TYPE_Float, TYPE_Float})) return this;
      fv = Args[0]->GetFloatConst();
      if (isNaNF(fv)) return this;
      fv2 = Args[1]->GetFloatConst();
      if (isNaNF(fv2)) return this;
      e = new VFloatLiteral(max2(fv, fv2), Loc);
      break;
    case OPC_Builtin_IntClamp: // (val, min, max)
      if (!CheckSimpleConstArgs(3, (const int []){TYPE_Int, TYPE_Int, TYPE_Int})) return this;
      {
        const int v = Args[0]->GetIntConst();
        const int low = Args[1]->GetIntConst();
        const int high = Args[2]->GetIntConst();
        e = new VIntLiteral(clampval(v, low, high), Loc);
      }
      break;
    case OPC_Builtin_FloatClamp: // (val, min, max)
      if (!CheckSimpleConstArgs(3, (const int []){TYPE_Float, TYPE_Float, TYPE_Float})) return this;
      {
        const float v = Args[0]->GetIntConst();
        const float low = Args[1]->GetIntConst();
        const float high = Args[2]->GetIntConst();
        if (isNaNF(v) || isNaNF(low) || isNaNF(high)) return this;
        e = new VFloatLiteral(clampval(v, low, high), Loc);
      }
      break;
    case OPC_Builtin_FloatIsNaN:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      e = new VIntLiteral((isNaNF(Args[0]->GetFloatConst()) ? 1 : 0), Loc);
      break;
    case OPC_Builtin_FloatIsInf:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      e = new VIntLiteral((isInfF(Args[0]->GetFloatConst()) ? 1 : 0), Loc);
      break;
    case OPC_Builtin_FloatIsFinite:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      e = new VIntLiteral((isFiniteF(Args[0]->GetFloatConst()) ? 1 : 0), Loc);
      break;
    case OPC_Builtin_DegToRad:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      fv = Args[0]->GetFloatConst();
      if (!isFiniteF(fv)) return this;
      e = new VFloatLiteral(DEG2RADF(fv), Loc);
      break;
    case OPC_Builtin_RadToDeg:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      fv = Args[0]->GetFloatConst();
      if (!isFiniteF(fv)) return this;
      e = new VFloatLiteral(RAD2DEGF(fv), Loc);
      break;
    case OPC_Builtin_AngleMod360:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      fv = Args[0]->GetFloatConst();
      if (!isFiniteF(fv)) return this;
      e = new VFloatLiteral(AngleMod(fv), Loc);
      break;
    case OPC_Builtin_AngleMod180:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      fv = Args[0]->GetFloatConst();
      if (!isFiniteF(fv)) return this;
      e = new VFloatLiteral(AngleMod180(fv), Loc);
      break;
    case OPC_Builtin_Sin:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      fv = Args[0]->GetFloatConst();
      if (!isFiniteF(fv)) return this;
      e = new VFloatLiteral(msin(fv), Loc);
      break;
    case OPC_Builtin_Cos:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      fv = Args[0]->GetFloatConst();
      if (!isFiniteF(fv)) return this;
      e = new VFloatLiteral(mcos(fv), Loc);
      break;
    case OPC_Builtin_Tan:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      fv = Args[0]->GetFloatConst();
      if (!isFiniteF(fv)) return this;
      e = new VFloatLiteral(mtan(fv), Loc);
      break;
    case OPC_Builtin_ASin:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      fv = Args[0]->GetFloatConst();
      if (!isFiniteF(fv)) return this;
      e = new VFloatLiteral(masin(fv), Loc);
      break;
    case OPC_Builtin_ACos:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      fv = Args[0]->GetFloatConst();
      if (!isFiniteF(fv)) return this;
      e = new VFloatLiteral(macos(fv), Loc);
      break;
    case OPC_Builtin_ATan:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      fv = Args[0]->GetFloatConst();
      if (!isFiniteF(fv)) return this;
      fv = atanf(fv);
      e = new VFloatLiteral(RAD2DEGF(fv), Loc);
      break;
    case OPC_Builtin_Sqrt:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      fv = Args[0]->GetFloatConst();
      if (!isFiniteF(fv)) return this;
      e = new VFloatLiteral(sqrtf(fv), Loc);
      break;
    case OPC_Builtin_ATan2:
      if (!CheckSimpleConstArgs(2, (const int []){TYPE_Float, TYPE_Float})) return this;
      fv = Args[0]->GetFloatConst();
      if (!isFiniteF(fv)) return this;
      fv2 = Args[1]->GetFloatConst();
      if (!isFiniteF(fv2)) return this;
      e = new VFloatLiteral(matan(fv, fv2), Loc);
      break;
    case OPC_Builtin_FMod:
      if (!CheckSimpleConstArgs(2, (const int []){TYPE_Float, TYPE_Float})) return this;
      fv = Args[0]->GetFloatConst();
      if (!isFiniteF(fv)) return this;
      fv2 = Args[1]->GetFloatConst();
      if (!isFiniteF(fv2) || fv2 == 0.0f) return this;
      fvres = fmodf(fv, fv2);
      if (!isFiniteF(fvres)) return this;
      e = new VFloatLiteral(fvres, Loc);
      break;
    case OPC_Builtin_FModPos:
      if (!CheckSimpleConstArgs(2, (const int []){TYPE_Float, TYPE_Float})) return this;
      fv = Args[0]->GetFloatConst();
      if (!isFiniteF(fv)) return this;
      fv2 = Args[1]->GetFloatConst();
      if (!isFiniteF(fv2) || fv2 == 0.0f) return this;
      fvres = fmodf(fv, fv2);
      if (!isFiniteF(fvres)) return this;
      if (fvres < 0.0f) fvres += fabsf(fv2);
      e = new VFloatLiteral(fvres, Loc);
      break;
    case OPC_Builtin_FPow:
      if (!CheckSimpleConstArgs(2, (const int []){TYPE_Float, TYPE_Float})) return this;
      fv = Args[0]->GetFloatConst();
      if (!isFiniteF(fv)) return this;
      fv2 = Args[1]->GetFloatConst();
      if (!isFiniteF(fv2) || fv2 == 0.0f) return this;
      fvres = powf(fv, fv2);
      if (!isFiniteF(fvres)) return this;
      e = new VFloatLiteral(fvres, Loc);
      break;
    case OPC_Builtin_FLog2:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      fv = Args[0]->GetFloatConst();
      if (!isFiniteF(fv)) return this;
      fvres = log2f(fv);
      if (!isFiniteF(fvres)) return this;
      e = new VFloatLiteral(fvres, Loc);
      break;
    case OPC_Builtin_FloatEquEps:
      if (!CheckSimpleConstArgs(3, (const int []){TYPE_Float, TYPE_Float, TYPE_Float})) return this;
      fv = Args[0]->GetFloatConst(); // v0
      if (!isFiniteF(fv)) return this;
      fv2 = Args[1]->GetFloatConst(); // v1
      if (!isFiniteF(fv2)) return this;
      fvres = Args[2]->GetFloatConst(); // eps
      if (!isFiniteF(fv2)) return this;
      e = new VIntLiteral((fabsf(fv-fv2) <= fvres ? 1 : 0), Loc);
      break;
    case OPC_Builtin_FloatEquEpsLess:
      if (!CheckSimpleConstArgs(3, (const int []){TYPE_Float, TYPE_Float, TYPE_Float})) return this;
      fv = Args[0]->GetFloatConst(); // v0
      if (!isFiniteF(fv)) return this;
      fv2 = Args[1]->GetFloatConst(); // v1
      if (!isFiniteF(fv2)) return this;
      fvres = Args[2]->GetFloatConst(); // eps
      if (!isFiniteF(fv2)) return this;
      e = new VIntLiteral((fabsf(fv-fv2) < fvres ? 1 : 0), Loc);
      break;
    case OPC_Builtin_VecLength:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Vector})) {
        // `vec.xy.length` -> `vec.length2D`
        e = ExtractSwizzleXY(Args[0]);
        if (e) { Args[0] = e; OverrideBuiltin = OPC_Builtin_VecLength2D; }
        return this;
      }
      v0 = ((VVectorExpr *)Args[0])->GetConstValue();
      fv = v0.length();
      if (!isFiniteF(fv)) return this;
      e = new VFloatLiteral(fv, Loc);
      break;
    case OPC_Builtin_VecLengthSquared:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Vector})) {
        // `vec.xy.lengthSquared` -> `vec.length2DSquared`
        e = ExtractSwizzleXY(Args[0]);
        if (e) { Args[0] = e; OverrideBuiltin = OPC_Builtin_VecLength2DSquared; }
        return this;
      }
      v0 = ((VVectorExpr *)Args[0])->GetConstValue();
      fv = v0.lengthSquared();
      if (!isFiniteF(fv)) return this;
      e = new VFloatLiteral(fv, Loc);
      break;
    case OPC_Builtin_VecLength2D:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Vector})) return this;
      v0 = ((VVectorExpr *)Args[0])->GetConstValue();
      fv = v0.length2D();
      if (!isFiniteF(fv)) return this;
      e = new VFloatLiteral(fv, Loc);
      break;
    case OPC_Builtin_VecLength2DSquared:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Vector})) return this;
      v0 = ((VVectorExpr *)Args[0])->GetConstValue();
      fv = v0.length2DSquared();
      if (!isFiniteF(fv)) return this;
      e = new VFloatLiteral(fv, Loc);
      break;
    case OPC_Builtin_VecNormalize:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Vector})) {
        // `vec.xy.normalise` -> `vec.normalise2D`
        e = ExtractSwizzleXY(Args[0]);
        if (e) { Args[0] = e; OverrideBuiltin = OPC_Builtin_VecNormalize2D; }
        return this;
      }
      v0 = ((VVectorExpr *)Args[0])->GetConstValue();
      v0.normaliseInPlace(); //v0 = v0.normalise();
      if (!v0.isValid()) return this;
      e = new VVectorExpr(v0, Loc);
      break;
    case OPC_Builtin_VecNormalize2D:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Vector})) return this;
      v0 = ((VVectorExpr *)Args[0])->GetConstValue();
      v0.normalise2DInPlace(); //v0 = v0.normalise2D();
      if (!v0.isValid()) return this;
      e = new VVectorExpr(v0, Loc);
      break;
    case OPC_Builtin_VecDot:
      //fprintf(stderr, "op0=<%s>; op1=<%s>\n", *Args[0]->toString(), *Args[1]->toString());
      if (!CheckSimpleConstArgs(2, (const int []){TYPE_Vector, TYPE_Vector})) return this;
      v0 = ((VVectorExpr *)Args[0])->GetConstValue();
      v1 = ((VVectorExpr *)Args[1])->GetConstValue();
      fv = v0.dot(v1);
      if (!isFiniteF(fv)) ParseError(Loc, "dotproduct is INF/NAN");
      e = new VFloatLiteral(fv, Loc);
      break;
    case OPC_Builtin_VecDot2D:
      if (!CheckSimpleConstArgs(2, (const int []){TYPE_Vector, TYPE_Vector})) return this;
      v0 = ((VVectorExpr *)Args[0])->GetConstValue();
      v1 = ((VVectorExpr *)Args[1])->GetConstValue();
      fv = v0.dot2D(v1);
      if (!isFiniteF(fv)) ParseError(Loc, "dotproduct2d is INF/NAN");
      e = new VFloatLiteral(fv, Loc);
      break;
    case OPC_Builtin_VecCross:
      if (!CheckSimpleConstArgs(2, (const int []){TYPE_Vector, TYPE_Vector})) return this;
      v0 = ((VVectorExpr *)Args[0])->GetConstValue();
      v1 = ((VVectorExpr *)Args[1])->GetConstValue();
      v0 = v0.cross(v1);
      if (!v0.isValid()) ParseError(Loc, "crossproduct is INF/NAN");
      e = new VVectorExpr(v0, Loc);
      break;
    case OPC_Builtin_VecCross2D:
      if (!CheckSimpleConstArgs(2, (const int []){TYPE_Vector, TYPE_Vector})) return this;
      v0 = ((VVectorExpr *)Args[0])->GetConstValue();
      v1 = ((VVectorExpr *)Args[1])->GetConstValue();
      fv = v0.cross2D(v1);
      if (!isFiniteF(fv)) ParseError(Loc, "crossproduct2d is INF/NAN");
      e = new VFloatLiteral(fv, Loc);
      break;
    case OPC_Builtin_RoundF2I:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      fv = Args[0]->GetFloatConst();
      if (!isFiniteF(fv)) return this;
      e = new VIntLiteral((int)roundf(fv), Loc);
      break;
    case OPC_Builtin_RoundF2F:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      fv = Args[0]->GetFloatConst();
      if (!isFiniteF(fv)) return this;
      e = new VFloatLiteral(roundf(fv), Loc);
      break;
    case OPC_Builtin_TruncF2I:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      fv = Args[0]->GetFloatConst();
      if (!isFiniteF(fv)) return this;
      e = new VIntLiteral((int)truncf(fv), Loc);
      break;
    case OPC_Builtin_TruncF2F:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      fv = Args[0]->GetFloatConst();
      if (!isFiniteF(fv)) return this;
      e = new VFloatLiteral(truncf(fv), Loc);
      break;
    case OPC_Builtin_FloatCeil:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      fv = Args[0]->GetFloatConst();
      if (!isFiniteF(fv)) return this;
      e = new VFloatLiteral(ceilf(fv), Loc);
      break;
    case OPC_Builtin_FloatFloor:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Float})) return this;
      fv = Args[0]->GetFloatConst();
      if (!isFiniteF(fv)) return this;
      e = new VFloatLiteral(floorf(fv), Loc);
      break;
    // [-3]: a; [-2]: b, [-1]: delta
    case OPC_Builtin_FloatLerp:
      if (!CheckSimpleConstArgs(3, (const int []){TYPE_Float, TYPE_Float, TYPE_Float})) return this;
      e = new VFloatLiteral(Args[0]->GetFloatConst()+(Args[1]->GetFloatConst()-Args[0]->GetFloatConst())*Args[2]->GetFloatConst(), Loc);
      break;
    case OPC_Builtin_IntLerp:
      if (!CheckSimpleConstArgs(3, (const int []){TYPE_Int, TYPE_Int, TYPE_Float})) return this;
      e = new VIntLiteral((int)roundf(Args[0]->GetIntConst()+(Args[1]->GetIntConst()-Args[0]->GetIntConst())*Args[2]->GetFloatConst()), Loc);
      break;
    case OPC_Builtin_FloatSmoothStep:
      if (!CheckSimpleConstArgs(3, (const int []){TYPE_Float, TYPE_Float, TYPE_Float})) return this;
      e = new VFloatLiteral(smoothstep(Args[0]->GetFloatConst(), Args[1]->GetFloatConst(), Args[2]->GetFloatConst()), Loc);
      break;
    case OPC_Builtin_FloatSmoothStepPerlin:
      if (!CheckSimpleConstArgs(3, (const int []){TYPE_Float, TYPE_Float, TYPE_Float})) return this;
      e = new VFloatLiteral(smoothstepPerlin(Args[0]->GetFloatConst(), Args[1]->GetFloatConst(), Args[2]->GetFloatConst()), Loc);
      break;
    case OPC_Builtin_NameToIIndex:
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Name})) return this;
      e = new VIntLiteral(Args[0]->GetNameConst().GetIndex(), Loc);
      break;
    case OPC_Builtin_VectorClampF: // (val, min, max)
      if (!CheckSimpleConstArgs(3, (const int []){TYPE_Vector, TYPE_Float, TYPE_Float})) return this;
      v0 = ((VVectorExpr *)Args[0])->GetConstValue();
      if (v0.isValid()) {
        const float vmin = Args[1]->GetFloatConst();
        const float vmax = Args[2]->GetFloatConst();
        if (isFiniteF(vmin) && isFiniteF(vmax)) {
          v0.x = clampval(v0.x, vmin, vmax);
          v0.y = clampval(v0.y, vmin, vmax);
          v0.z = clampval(v0.z, vmin, vmax);
        } else if (isFiniteF(vmin)) {
          v0.x = min2(vmin, v0.x);
          v0.y = min2(vmin, v0.y);
          v0.z = min2(vmin, v0.z);
        } else if (isFiniteF(vmax)) {
          v0.x = max2(vmax, v0.x);
          v0.y = max2(vmax, v0.y);
          v0.z = max2(vmax, v0.z);
        }
        e = new VVectorExpr(v0, Loc);
      } else {
        return this;
      }
      break;
    case OPC_Builtin_VectorClampScaleF: // (val, absmax)
      if (!CheckSimpleConstArgs(2, (const int []){TYPE_Vector, TYPE_Float})) return this;
      v0 = ((VVectorExpr *)Args[0])->GetConstValue();
      if (v0.isValid()) {
        const float vabsmax = Args[1]->GetFloatConst();
        v0.clampScaleInPlace(vabsmax);
        e = new VVectorExpr(v0, Loc);
      } else {
        return this;
      }
      break;
    case OPC_Builtin_VectorClampV: // (val, min, max)
      if (!CheckSimpleConstArgs(3, (const int []){TYPE_Vector, TYPE_Vector, TYPE_Vector})) return this;
      v0 = ((VVectorExpr *)Args[0])->GetConstValue();
      v1 = ((VVectorExpr *)Args[1])->GetConstValue();
      v2 = ((VVectorExpr *)Args[2])->GetConstValue();
      if (v0.isValid()) {
        if (v1.isValid() && v2.isValid()) {
          v0.x = clampval(v0.x, v1.x, v2.x);
          v0.y = clampval(v0.y, v1.y, v2.y);
          v0.z = clampval(v0.z, v1.z, v2.z);
        } else if (v1.isValid()) {
          v0.x = min2(v1.x, v0.x);
          v0.y = min2(v1.y, v0.y);
          v0.z = min2(v1.z, v0.z);
        } else if (v2.isValid()) {
          v0.x = max2(v2.x, v0.x);
          v0.y = max2(v2.y, v0.y);
          v0.z = max2(v2.z, v0.z);
        }
        e = new VVectorExpr(v0, Loc);
      } else {
        return this;
      }
      break;
    case OPC_Builtin_VectorMinV: // (v0, v1)
      if (!CheckSimpleConstArgs(2, (const int []){TYPE_Vector, TYPE_Vector})) return this;
      v0 = ((VVectorExpr *)Args[0])->GetConstValue();
      v1 = ((VVectorExpr *)Args[1])->GetConstValue();
      if (v0.isValid() && v1.isValid()) {
        v0.x = min2(v1.x, v0.x);
        v0.y = min2(v1.y, v0.y);
        v0.z = min2(v1.z, v0.z);
        e = new VVectorExpr(v0, Loc);
      } else {
        return this;
      }
      break;
    case OPC_Builtin_VectorMaxV: // (v0, v1)
      if (!CheckSimpleConstArgs(2, (const int []){TYPE_Vector, TYPE_Vector})) return this;
      v0 = ((VVectorExpr *)Args[0])->GetConstValue();
      v1 = ((VVectorExpr *)Args[1])->GetConstValue();
      if (v0.isValid() && v1.isValid()) {
        v0.x = max2(v1.x, v0.x);
        v0.y = max2(v1.y, v0.y);
        v0.z = max2(v1.z, v0.z);
        e = new VVectorExpr(v0, Loc);
      } else {
        return this;
      }
      break;
    case OPC_Builtin_VectorMinF: // (v, f)
      if (!CheckSimpleConstArgs(2, (const int []){TYPE_Vector, TYPE_Float})) return this;
      v0 = ((VVectorExpr *)Args[0])->GetConstValue();
      fv = Args[1]->GetFloatConst();
      if (v0.isValid() && isFiniteF(fv)) {
        v0.x = min2(fv, v0.x);
        v0.y = min2(fv, v0.y);
        v0.z = min2(fv, v0.z);
        e = new VVectorExpr(v0, Loc);
      } else {
        return this;
      }
      break;
    case OPC_Builtin_VectorMaxF: // (v, f)
      if (!CheckSimpleConstArgs(2, (const int []){TYPE_Vector, TYPE_Float})) return this;
      v0 = ((VVectorExpr *)Args[0])->GetConstValue();
      fv = Args[1]->GetFloatConst();
      if (v0.isValid() && isFiniteF(fv)) {
        v0.x = max2(fv, v0.x);
        v0.y = max2(fv, v0.y);
        v0.z = max2(fv, v0.z);
        e = new VVectorExpr(v0, Loc);
      } else {
        return this;
      }
      break;
    case OPC_Builtin_VectorAbs: // (v)
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Vector})) return this;
      v0 = ((VVectorExpr *)Args[0])->GetConstValue();
      if (v0.isValid()) {
        v0.x = fabsf(v0.x);
        v0.y = fabsf(v0.y);
        v0.z = fabsf(v0.z);
        e = new VVectorExpr(v0, Loc);
      } else {
        return this;
      }
      break;

    // special cvar optimisations
    case OPC_Builtin_IsCvarExists:
      //GLog.Logf(NAME_Debug, "%s: opt: OPC_Builtin_IsCvarExists...", *Loc.toStringNoCol());
      if (!Func->IsStatic() || DelegateLocal >= 0) ParseError(Loc, "Cannot call non-static builtin `%s`", *Func->Name);
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Name})) {
        //GLog.Log(NAME_Debug, "skipped optimisation: OPC_Builtin_IsCvarExists");
        OverrideBuiltin = OPC_Builtin_IsCvarExistsRT; // convert to RT builtin
      }
      /*
      else {
        GLog.Logf(NAME_Debug, "OPC_Builtin_IsCvarExists: '%s'", *Args[0]->GetNameConst());
      }
      */
      return this;
    case OPC_Builtin_GetCvarInt:
      if (!Func->IsStatic() || DelegateLocal >= 0) ParseError(Loc, "Cannot call non-static builtin `%s`", *Func->Name);
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Name})) {
        OverrideBuiltin = OPC_Builtin_GetCvarIntRT; // convert to RT builtin
      }
      return this;
    case OPC_Builtin_GetCvarFloat:
      if (!Func->IsStatic() || DelegateLocal >= 0) ParseError(Loc, "Cannot call non-static builtin `%s`", *Func->Name);
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Name})) {
        OverrideBuiltin = OPC_Builtin_GetCvarFloatRT; // convert to RT builtin
      }
      return this;
    case OPC_Builtin_GetCvarStr:
      if (!Func->IsStatic() || DelegateLocal >= 0) ParseError(Loc, "Cannot call non-static builtin `%s`", *Func->Name);
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Name})) {
        OverrideBuiltin = OPC_Builtin_GetCvarStrRT; // convert to RT builtin
      }
      return this;
    case OPC_Builtin_GetCvarBool:
      if (!Func->IsStatic() || DelegateLocal >= 0) ParseError(Loc, "Cannot call non-static builtin `%s`", *Func->Name);
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Name})) {
        OverrideBuiltin = OPC_Builtin_GetCvarBoolRT; // convert to RT builtin
      }
      return this;

    case OPC_Builtin_SetCvarInt:
      if (!Func->IsStatic() || DelegateLocal >= 0) ParseError(Loc, "Cannot call non-static builtin `%s`", *Func->Name);
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Name}, false)) {
        OverrideBuiltin = OPC_Builtin_SetCvarIntRT; // convert to RT builtin
      }
      return this;
    case OPC_Builtin_SetCvarFloat:
      if (!Func->IsStatic() || DelegateLocal >= 0) ParseError(Loc, "Cannot call non-static builtin `%s`", *Func->Name);
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Name}, false)) {
        OverrideBuiltin = OPC_Builtin_SetCvarFloatRT; // convert to RT builtin
      }
      return this;
    case OPC_Builtin_SetCvarStr:
      if (!Func->IsStatic() || DelegateLocal >= 0) ParseError(Loc, "Cannot call non-static builtin `%s`", *Func->Name);
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Name}, false)) {
        OverrideBuiltin = OPC_Builtin_SetCvarStrRT; // convert to RT builtin
      }
      return this;
    case OPC_Builtin_SetCvarBool:
      if (!Func->IsStatic() || DelegateLocal >= 0) ParseError(Loc, "Cannot call non-static builtin `%s`", *Func->Name);
      if (!CheckSimpleConstArgs(1, (const int []){TYPE_Name}, false)) {
        OverrideBuiltin = OPC_Builtin_SetCvarBoolRT; // convert to RT builtin
      }
      return this;

    default: break;
  }
  if (e) {
    delete this;
    return e->Resolve(ec);
  }
  return this;
}


//==========================================================================
//
//  VInvocation::Emit
//
//==========================================================================
void VInvocation::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);

  const int builtIn = (Func->builtinOpc >= 0 ? (OverrideBuiltin >= 0 ? OverrideBuiltin : Func->builtinOpc) : -1);

  // special cvar optimisations
  if (builtIn >= OPC_Builtin_IsCvarExists && builtIn <= OPC_Builtin_GetCvarBool) {
    ec.AddCVarBuiltin(builtIn, Args[0]->GetNameConst(), Loc);
    return;
  }

  // special cvar optimisations
  if (builtIn >= OPC_Builtin_SetCvarInt && builtIn <= OPC_Builtin_SetCvarBool) {
    Args[1]->Emit(ec);
    ec.AddCVarBuiltin(builtIn, Args[0]->GetNameConst(), Loc);
    return;
  }

  if (SelfExpr) {
    if (!Func->IsStatic()) SelfExpr->Emit(ec);
  }

  bool DirectCall = (BaseCall || (Func->Flags&FUNC_Final) != 0);

  if (DelegateLocal >= 0) {
    //ec.EmitPushNumber(0, Loc); // `self`, will be replaced by executor
    ec.AddStatement(OPC_PushNull, Loc); // `self`, will be replaced by executor
  } else {
    if (Func->IsStatic()) {
      // it is allowed to call static methods with `var.name()`
      //if (HaveSelf) ParseError(Loc, "Invalid static function call");
    } else if (!HaveSelf) {
      if (ec.CurrentFunc->IsStatic()) ParseError(Loc, "Cannot call non-static method `%s` from static method `%s`", *Func->Name, *ec.CurrentFunc->Name);
      ec.AddStatement(OPC_LocalValue0, Loc);
    }
  }

  // we may create new locals, so activate local reuse mechanics
  //auto guard = ec.EnterScopeEmit();

  vint32 SelfOffset = 1;
  for (int i = 0; i < NumArgs; ++i) {
    if (!Args[i]) {
      if (i >= Func->NumParams) VCFatalError("VC: Internal compiler error (VInvocation::Emit): too many params");
      if (Func->ParamFlags[i]&(FPARM_Out|FPARM_Ref)) {
        // get local address
        if (lcidx[i] < 0) VCFatalError("VC: Internal compiler error (VInvocation::Emit)");
        // make sure struct / class field offsets have been calculated
        /*
        if (Func->ParamTypes[i].Type == TYPE_Struct) {
          Func->ParamTypes[i].Struct->PostLoad();
        }
        */
        const VLocalVarDef &loc = ec.GetLocalByIndex(lcidx[i]);
        //GLog.Logf(NAME_Debug, "%s:000: invoke: arg #%d: lofs=%d", *Loc.toStringNoCol(), i, loc.Offset);
        ec.AllocateLocalSlot(lcidx[i], VEmitContext::Safe);
        //GLog.Logf(NAME_Debug, "%s:001: invoke: arg #%d: lofs=%d; reused=%d", *Loc.toStringNoCol(), i, loc.Offset, (int)loc.reused);
        if (loc.reused) ec.EmitLocalZero(lcidx[i], Loc); // forced zero if reused
        // check if we won't forgont to allocate the local
        if (loc.Offset == -666) {
          // this means that the local was not properly allocated
          VCFatalError("%s: local #%d (%s) was not properly allocated (internal compiler error)", *Loc.toStringNoCol(), lcidx[i], (loc.Name != NAME_None ? *loc.Name : "<hidden>"));
        }
        ec.EmitLocalAddress(loc.Offset, Loc);
        ++SelfOffset; // pointer
      } else {
        // nonref
        switch (Func->ParamTypes[i].Type) {
          case TYPE_Int:
          case TYPE_Byte:
          case TYPE_Bool:
          case TYPE_Float:
          case TYPE_Name:
            ec.EmitPushNumber(0, Loc);
            ++SelfOffset;
            break;
          case TYPE_String:
          case TYPE_Pointer:
          case TYPE_Reference:
          case TYPE_Class:
          case TYPE_State:
            ec.AddStatement(OPC_PushNull, Loc);
            ++SelfOffset;
            break;
          case TYPE_Vector:
            ec.EmitPushNumber(0, Loc);
            ec.EmitPushNumber(0, Loc);
            ec.EmitPushNumber(0, Loc);
            SelfOffset += 3;
            break;
          case TYPE_Delegate:
            ec.AddStatement(OPC_PushNull, Loc);
            ec.AddStatement(OPC_PushNull, Loc);
            SelfOffset += 2;
            break;
          default:
            // optional?
            ParseError(Loc, "Bad optional parameter type");
            break;
        }
      }
      // omited ref args can be non-optional
      if (Func->ParamFlags[i]&FPARM_Optional) {
        ec.EmitPushNumber(0, Loc);
        ++SelfOffset;
      }
    } else {
      if (Func->ParamFlags[i]&(FPARM_Out|FPARM_Ref)) {
        // consider ref/out as read/written
        if (Args[i]->IsLocalVarExpr()) {
          VLocalVar *lv = (VLocalVar *)Args[i];
          ec.MarkLocalUsedByIdx(lv->num);
        }
        Args[i]->Emit(ec);
        SelfOffset += 1;
      } else {
        //SelfOffset += (Args[i]->Type.Type == TYPE_Vector ? 3 : Args[i]->Type.Type == TYPE_Delegate ? 2 : 1);
        if ((Func->Flags&(FUNC_Native|FUNC_VarArgs)) == (FUNC_Native|FUNC_VarArgs)) {
          switch (Args[i]->Type.Type) {
            case TYPE_Array:
            case TYPE_DynamicArray:
            case TYPE_SliceArray:
            case TYPE_Dictionary:
              Args[i]->RequestAddressOf();
              SelfOffset += 1;
              break;
            default:
              SelfOffset += Args[i]->Type.GetStackSize();
              break;
          }
        } else {
          SelfOffset += Args[i]->Type.GetStackSize();
        }
        //GLog.Logf(NAME_Debug, "%s: #%d: <%s>", *Args[i]->Loc.toStringNoCol(), i, *Args[i]->toString());
        Args[i]->Emit(ec);
      }
      if (Func->ParamFlags[i]&FPARM_Optional) {
        // marshall "specified_*"?
        if (i < VMethod::MAX_PARAMS && optmarshall[i] >= 0) {
          // i found her!
          //HACK: it is safe (and necessary) to resolve here
          VExpression *xlv = new VLocalVar(optmarshall[i], VEmitContext::Safe, Args[i]->Loc);
          xlv = xlv->Resolve(ec);
          if (!xlv) VCFatalError("VC: internal compiler error (13496)");
          xlv->Emit(ec);
          delete xlv;
        } else {
          // not optional, so always specified
          ec.EmitPushNumber(1, Loc);
        }
        ++SelfOffset;
      }
    }
    // push type for varargs (except the last arg, as it is a simple int counter)
    if (Func->printfFmtArgIdx >= 0 && (Func->Flags&FUNC_VarArgs) != 0 && i >= Func->NumParams && i != NumArgs-1) {
      if (Args[i]) {
        ec.AddStatement(OPC_DoPushTypePtr, Args[i]->Type, Loc);
      } else {
        auto vtp = VFieldType(TYPE_Void);
        ec.AddStatement(OPC_DoPushTypePtr, vtp, Loc);
      }
      ++SelfOffset;
    }
  }

  // some special functions will be converted to builtins
  if (builtIn >= 0) {
    ec.AddBuiltin(builtIn, Loc);
  } else if (DirectCall) {
    ec.AddStatement(OPC_Call, Func, SelfOffset, Loc);
  } else if (DelegateField) {
    ec.AddStatement(OPC_DelegateCall, DelegateField, SelfOffset, Loc);
  } else if (DelegateLocal >= 0) {
    // get address of local
    const VLocalVarDef &loc = ec.GetLocalByIndex(DelegateLocal);
    // check if we won't forgont to allocate the local
    if (loc.Offset == -666) {
      // this means that the local was not properly allocated
      VCFatalError("%s: local #%d (%s) was not properly allocated (internal compiler error)", *Loc.toStringNoCol(), DelegateLocal, (loc.Name != NAME_None ? *loc.Name : "<hidden>"));
    }
    ec.EmitLocalAddress(loc.Offset, Loc);
    ec.AddStatement(OPC_DelegateCallPtr, loc.Type, SelfOffset, Loc);
  } else if (DgPtrExpr) {
    if (DgPtrExpr) DgPtrExpr->Emit(ec);
    VFieldType tp = VFieldType(TYPE_Delegate);
    tp.Function = Func;
    ec.AddStatement(OPC_DelegateCallPtr, tp, SelfOffset, Loc);
  } else {
    ec.AddStatement(OPC_VCall, Func, SelfOffset, Loc);
  }

  // destruct all anonymous locals
  for (int f = VMethod::MAX_PARAMS-1; f >= 0; --f) {
    if (lcidx[f] != -1) {
      ec.EmitLocalDtor(lcidx[f], Loc);
      ec.ReleaseLocalSlot(lcidx[f]);
    }
  }
}


//==========================================================================
//
//  VInvocation::IsLLInvocation
//
//==========================================================================
bool VInvocation::IsLLInvocation () const {
  return true;
}


//==========================================================================
//
//  VInvocation::FindMethodWithSignature
//
//==========================================================================
VMethod *VInvocation::FindMethodWithSignature (VEmitContext &ec, VName name, int argc, VExpression **argv, const TLocation *loc) {
  if (argc < 0 || argc > VMethod::MAX_PARAMS) return nullptr;
  if (ec.SelfStruct) {
    VMethod *m = ec.SelfStruct->FindAccessibleMethod(name, ec.SelfStruct, loc);
    if (m && IsGoodMethodParams(ec, m, argc, argv)) return m;
  }
  if (!ec.SelfClass) return nullptr;
  VMethod *m = ec.SelfClass->FindAccessibleMethod(name, ec.SelfClass, loc);
  if (!m) return nullptr;
  if (!IsGoodMethodParams(ec, m, argc, argv)) return nullptr;
  return m;
}


//==========================================================================
//
//  VInvocation::IsGoodMethodParams
//
//  argv are not resolved, but should be resolvable without errors
//
//==========================================================================
bool VInvocation::IsGoodMethodParams (VEmitContext &ec, VMethod *m, int argc, VExpression **argv) {
  if (argc < 0 || argc > VMethod::MAX_PARAMS) return false;
  if (!m) return false;

  // determine parameter count
  int requiredParams = m->NumParams;
  int maxParams = (m->Flags&FUNC_VarArgs ? VMethod::MAX_PARAMS-1 : m->NumParams);

  if (argc > maxParams) return false;

  VGagErrors gag;

  for (int i = 0; i < argc; ++i) {
    if (i < requiredParams) {
      if (!argv[i]) {
        if (!(m->ParamFlags[i]&FPARM_Optional)) return false; // ommited non-optional
        continue;
      }
      // check for `ref`/`out` validness
      if (argv[i]->IsRefArg() && (m->ParamFlags[i]&FPARM_Ref) == 0) return false;
      if (argv[i]->IsOutArg() && (m->ParamFlags[i]&FPARM_Out) == 0) return false;
      // resolve it
      //FIXME: this can be already resolved if we're in `VPropertyAssign`
      bool doDeleteAA = !argv[i]->IsResolved();
      VExpression *aa = (doDeleteAA ? argv[i]->SyntaxCopy()->Resolve(ec) : argv[i]);
      if (!aa) return false;
      // other checks
      if (ec.Package->Name == NAME_decorate) {
        switch (m->ParamTypes[i].Type) {
          case TYPE_Int:
          case TYPE_Float:
            if (aa->Type.Type == TYPE_Float || aa->Type.Type == TYPE_Int) { if (doDeleteAA) delete aa; continue; }
            break;
        }
      }
      if (m->ParamFlags[i]&(FPARM_Out|FPARM_Ref)) {
        if (!aa->Type.Equals(m->ParamTypes[i])) {
          // check, but don't raise any errors
          if (!aa->Type.CheckMatch(true, aa->Loc, m->ParamTypes[i], false)) { if (doDeleteAA) delete aa; return false; }
        }
      } else {
        if (m->ParamTypes[i].Type == TYPE_Float && aa->Type.Type == TYPE_Int) { if (doDeleteAA) delete aa; continue; }
        // check, but don't raise any errors
        if (!aa->Type.CheckMatch(false, aa->Loc, m->ParamTypes[i], false)) { if (doDeleteAA) delete aa; return false; }
      }
      if (doDeleteAA) delete aa;
    } else {
      // vararg
      if (!argv[i]) return false;
    }
  }

  while (argc < requiredParams) {
    if (!(m->ParamFlags[argc]&FPARM_Optional)) return false;
    ++argc;
  }

  return !gag.wasErrors();
}


//==========================================================================
//
//  VInvocation::CheckParams
//
//  this also adds argc to vararg functions
//  must be called after resolving all args
//
//==========================================================================
void VInvocation::CheckParams (VEmitContext &ec) {
  //GLog.Logf(NAME_Debug, "%s: VInvocation::CheckParams: %s", *Loc.toStringNoCol(), *Func->GetFullName());
  // determine parameter count
  //int argsize = 0;
  int requiredParams = Func->NumParams;
  int maxParams = (Func->Flags&FUNC_VarArgs ? VMethod::MAX_PARAMS-1 : Func->NumParams);

  for (int i = 0; i < NumArgs; ++i) {
    if (i < requiredParams) {
      if (!Args[i]) {
        if ((Func->ParamFlags[i]&(FPARM_Out|FPARM_Ref)) != 0) {
          //argsize += 1; // pointer
        } else {
          if (!(Func->ParamFlags[i]&FPARM_Optional)) ParseError(Loc, "Cannot omit non-optional argument #%d to `%s`", i+1, *Func->GetFullName());
          //argsize += Func->ParamTypes[i].GetStackSize();
        }
      } else if ((Args[i]->IsNoneLiteral() || Args[i]->IsNullLiteral()) && (Func->ParamFlags[i]&(FPARM_Out|FPARM_Ref)) != 0) {
        // `ref`/`out` arg can be ommited with `none` or `null`
        delete Args[i];
        Args[i] = nullptr;
        //argsize += 1; // pointer
      } else {
        // check for `ref`/`out` validness
        //if (Args[i]->IsRefArg() && (Func->ParamFlags[i]&FPARM_Ref) == 0) ParseError(Args[i]->Loc, "`ref` argument for non-ref parameter");
        //if (Args[i]->IsOutArg() && (Func->ParamFlags[i]&FPARM_Out) == 0) ParseError(Args[i]->Loc, "`out` argument for non-ref parameter");
        if (ec.Package->Name == NAME_decorate) {
          switch (Func->ParamTypes[i].Type) {
            case TYPE_Int:
              if (Args[i]->IsFloatConst()) {
                TLocation Loc = Args[i]->Loc;
                const int Val = (int)(Args[i]->GetFloatConst());
                if (!VMemberBase::optDeprecatedLaxConversions && !VIsGoodFloatAsInt(Args[i]->GetFloatConst())) {
                  ParseError(Loc, "Invalid float->int cast for arg #%d", i+1);
                }
                delete Args[i];
                Args[i] = nullptr;
                Args[i] = new VIntLiteral(Val, Loc);
                Args[i] = Args[i]->Resolve(ec);
              } else if (Args[i]->Type.Type == TYPE_Float) {
                Args[i] = new VScalarToInt(Args[i], true);
                if (Args[i]) Args[i] = Args[i]->Resolve(ec);
              }
              break;
            case TYPE_Float:
              if (Args[i]->IsIntConst()) {
                TLocation Loc = Args[i]->Loc;
                const int Val = Args[i]->GetIntConst();
                const float fval = (float)Val;
                if (Val != (int)fval) {
                  ParseError(Loc, "Invalid int->float cast for arg #%d", i+1);
                }
                delete Args[i];
                Args[i] = nullptr;
                Args[i] = new VFloatLiteral(Val, Loc);
                Args[i] = Args[i]->Resolve(ec);
              } else if (Args[i]->Type.Type == TYPE_Int) {
                Args[i] = new VScalarToFloat(Args[i], true);
                if (Args[i]) Args[i] = Args[i]->Resolve(ec);
              }
              break;
          }
        }
        //GLog.Logf(NAME_Debug, "%s: checking arg #%d (func: out=%d; ref=%d); argtype=%s", *Args[i]->Loc.toStringNoCol(), i, (Func->ParamFlags[i]&FPARM_Out ? 1 : 0), (Func->ParamFlags[i]&FPARM_Ref ? 1 : 0), *Args[i]->Type.GetName());
        // ref/out args: no int->float conversion allowed
        if (Func->ParamFlags[i]&(FPARM_Out|FPARM_Ref)) {
          if (!Args[i]->Type.Equals(Func->ParamTypes[i])) {
            //FIXME: This should be an error
            /*
            if (!(Func->ParamFlags[NumArgs]&FPARM_Optional)) {
              Args[i]->Type.CheckMatch(true, Args[i]->Loc, Func->ParamTypes[i]);
            }
            */
            if (!Args[i]->Type.CheckMatch(true, Args[i]->Loc, Func->ParamTypes[i])) {
              ParseError(Args[i]->Loc, "Out parameter types must be equal for arg #%d (want `%s`, but got `%s`)", i+1, *Func->ParamTypes[i].GetName(), *Args[i]->Type.GetName());
            } else {
              /*
              fprintf(stderr, "PTRPTR: %s (%s : %s); (%u:%u) (%d:%d)\n", *Args[i]->Loc.toString(), *Args[i]->Type.GetName(), *Func->ParamTypes[i].GetName(),
                (unsigned)Args[i]->Type.Type, (unsigned)Func->ParamTypes[i].Type,
                Args[i]->Type.PtrLevel, Func->ParamTypes[i].PtrLevel);
              */
            }
          }
          // now check r/o flags
          if (Args[i]->IsReadOnly() && (Func->ParamFlags[i]&FPARM_Const) == 0) {
            ParseError(Args[i]->Loc, "Cannot pass const argument #%d as non-const", i+1);
          }
          // check for some forbidden things
          if (Args[i]->IsFieldAccess()) {
            bool ok = true;
            if (Func->ParamTypes[i].Type == TYPE_Bool) {
              // bool arg
              if (Args[i]->RealType.Type == TYPE_Byte || Args[i]->RealType.Type == TYPE_Bool) ok = false;
            } else if (Func->ParamTypes[i].Type == TYPE_Byte) {
              // byte arg
              if (Args[i]->RealType.Type != TYPE_Byte) ok = false;
            } else if (Func->ParamTypes[i].Type == TYPE_Int) {
              // int arg
              if (Args[i]->RealType.Type != TYPE_Int) ok = false;
            }
            if (!ok) {
              ParseError(Args[i]->Loc, "Cannot pass field of type `%s` as reference for argument #%d (not implemented yet)", *Args[i]->RealType.GetName(), i+1);
            }
          }
          //GLog.Logf(NAME_Debug, "MT:<%s>: arg#%d: pt=%s; at=%s (%s) (%s)", *Func->GetFullName(), i+1, *Func->ParamTypes[i].GetName(), *Args[i]->Type.GetName(), *Args[i]->RealType.GetName(), *Args[i]->toString());
          Args[i]->RequestAddressOf();
        } else {
          // normal args: do int->float conversion
          if (Func->ParamTypes[i].Type == TYPE_Float) {
            if (Args[i]->IsIntConst()) {
              TLocation Loc = Args[i]->Loc;
              const int val = Args[i]->GetIntConst();
              const float fval = (float)val;
              if (!VMemberBase::optDeprecatedLaxConversions && !VIsGoodIntAsFloat(val)) {
                ParseError(Loc, "Cannot convert argument to float for arg #%d", i+1);
              }
              delete Args[i];
              Args[i] = new VFloatLiteral(fval, Loc);
              if (Args[i]) Args[i] = Args[i]->Resolve(ec); // literal's `Reslove()` does nothing, but...
            } else if (Args[i]->Type.Type == TYPE_Int || Args[i]->Type.Type == TYPE_Byte) {
              auto erloc = Args[i]->Loc;
              Args[i] = new VScalarToFloat(Args[i], true);
              if (Args[i]) Args[i] = Args[i]->Resolve(ec);
              if (!Args[i]) ParseError(erloc, "Cannot convert argument to float for arg #%d", i+1);
            }
          }
          if (!Args[i]->Type.CheckMatch(false, Args[i]->Loc, Func->ParamTypes[i], false)) {
            ParseError(Args[i]->Loc, "Invalid argument #%d type '%s' in method '%s'", i+1, *Args[i]->Type.GetName(), *Func->GetFullName());
            //fprintf(stderr, "  <%s>\n", *Args[i]->toString());
          }
        }
        //argsize += Args[i]->Type.GetStackSize();
      }
    } else if (!Args[i]) {
           if (Func->Flags&FUNC_VarArgs) ParseError(Loc, "Cannot omit arguments for vararg function");
      else if (i >= Func->NumParams) ParseError(Loc, "Cannot omit extra arguments for vararg function");
      else ParseError(Loc, "Cannot omit argument (for some reason)");
    } else {
      //argsize += Args[i]->Type.GetStackSize();
    }
  }

  if (NumArgs > maxParams) ParseError(Loc, "Incorrect number of arguments to `%s`, need %d, got %d", *Func->GetFullName(), maxParams, NumArgs);

  while (NumArgs < requiredParams) {
    if (Func->ParamFlags[NumArgs]&FPARM_Optional) {
      Args[NumArgs] = nullptr;
      ++NumArgs;
    } else {
      ParseError(Loc, "Incorrect argument count %d to `%s`, should be %d", NumArgs, *Func->GetFullName(), requiredParams);
      break;
    }
  }

  if (Func->Flags&FUNC_VarArgs) {
    int argc = NumArgs-requiredParams;
    Args[NumArgs] = new VIntLiteral(argc, Loc);
    if (Args[NumArgs]) Args[NumArgs] = Args[NumArgs]->Resolve(ec);
    vassert(Args[NumArgs]);
    ++NumArgs;
  }
}


//==========================================================================
//
//  VInvocation::CheckDecorateParams
//
//==========================================================================
void VInvocation::CheckDecorateParams (VEmitContext &ec) {
  int requiredParams = Func->NumParams;
  int maxParams = (Func->Flags&FUNC_VarArgs ? VMethod::MAX_PARAMS-1 : Func->NumParams);

  if (NumArgs > maxParams) ParseError(Loc, "Incorrect number of arguments to `%s`, need %d, got %d", Func->GetName(), maxParams, NumArgs);

  //int oldNA = NumArgs;
  for (int i = 0; i < NumArgs; ++i) {
    //if (NumArgs != oldNA) GLog.Logf(NAME_Debug, "**** NumArgs changed from %d to %d; i=%d", oldNA, NumArgs, i);
    if (i >= requiredParams) continue;
    if (!Args[i]) continue;
    // params flags checking moved to decorate massager
    Args[i] = Args[i]->MassageDecorateArg(ec, this, CallerState, Func->GetName(), i+1, Func->ParamTypes[i], !!(Func->ParamFlags[i]&FPARM_Optional));
  }

  // some warnings
  if (NumArgs > maxParams) {
    ParseError(Loc, "Incorrect number of arguments to `%s`, need %d, got %d", Func->GetName(), maxParams, NumArgs);
  } else {
    if (Args[0] && Args[0]->IsIntConst() && VStr::Cmp(Func->GetName(), "A_Jump") == 0) {
      int prob = Args[0]->GetIntConst();
      if (prob < 1) ParseWarning(Loc, "this `A_Jump` is never taken");
    }
  }
}


//==========================================================================
//
//  VInvocation::GetVMethod
//
//==========================================================================
VMethod *VInvocation::GetVMethod (VEmitContext &/*ec*/) {
  return nullptr;
}


//==========================================================================
//
//  VInvocation::IsMethodNameChangeable
//
//==========================================================================
bool VInvocation::IsMethodNameChangeable () const {
  return false;
}


//==========================================================================
//
//  VInvocation::GetMethodName
//
//==========================================================================
VName VInvocation::GetMethodName () const {
  return (Func ? Func->Name : NAME_None);
}


//==========================================================================
//
//  VInvocation::SetMethodName
//
//==========================================================================
void VInvocation::SetMethodName (VName /*aname*/) {
  VCFatalError("VC: Internal compiler error: `VInvocation::SetMethodName()` called");
}


//==========================================================================
//
//  VInvocation::toString
//
//==========================================================================
VStr VInvocation::toString () const {
  VStr res;
  if (HaveSelf) {
    res += "(";
    res += e2s(SelfExpr);
    res += ")";
  }
  if (BaseCall) res += "::";
  if (Func) res += "("+Func->GetFullName()+")"; else res += e2s(nullptr);
  //VField *DelegateField;
  //int DelegateLocal;
  //VState *CallerState;
  res += args2str();
  return res;
}



//==========================================================================
//
//  VInvokeWrite::VInvokeWrite
//
//==========================================================================
VInvokeWrite::VInvokeWrite (bool aIsWriteln, const TLocation &ALoc, int ANumArgs, VExpression **AArgs)
  : VInvocationBase(ANumArgs, AArgs, ALoc)
  , isWriteln(aIsWriteln)
{
}


//==========================================================================
//
//  VInvokeWrite::SyntaxCopy
//
//==========================================================================
VExpression *VInvokeWrite::SyntaxCopy () {
  auto res = new VInvokeWrite();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VInvokeWrite::DoSyntaxCopyTo
//
//==========================================================================
void VInvokeWrite::DoSyntaxCopyTo (VExpression *e) {
  VInvocationBase::DoSyntaxCopyTo(e);
  auto res = (VInvokeWrite *)e;
  res->isWriteln = isWriteln;
}


//==========================================================================
//
//  VInvokeWrite::DoResolve
//
//==========================================================================
VExpression *VInvokeWrite::DoResolve (VEmitContext &ec) {
  // resolve arguments
  bool ArgsOk = true;
  for (int i = 0; i < NumArgs; ++i) {
    if (Args[i]) {
      Args[i] = Args[i]->Resolve(ec);
      if (!Args[i]) {
        ArgsOk = false;
      } else {
        switch (Args[i]->Type.Type) {
          case TYPE_Int:
          case TYPE_Byte:
          case TYPE_Bool:
          case TYPE_Float:
          case TYPE_Name:
          case TYPE_String:
          case TYPE_Pointer:
          case TYPE_Vector:
          case TYPE_Class:
          case TYPE_State:
          case TYPE_Reference:
          case TYPE_Delegate:
            break;
          case TYPE_Struct:
          case TYPE_Array:
          case TYPE_DynamicArray:
          case TYPE_Dictionary:
            Args[i]->RequestAddressOf();
            break;
          case TYPE_SliceArray:
          case TYPE_Void:
          case TYPE_Unknown:
          case TYPE_Automatic:
          default:
            ParseError(Args[i]->Loc, "Cannot write type `%s`", *Args[i]->Type.GetName());
            ArgsOk = false;
            break;
        }
      }
    }
  }

  if (!ArgsOk) {
    delete this;
    return nullptr;
  }

  Type = VFieldType(TYPE_Void);
  SetResolved();
  return this;
}


//==========================================================================
//
//  VInvokeWrite::Emit
//
//==========================================================================
void VInvokeWrite::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  for (int i = 0; i < NumArgs; ++i) {
    if (!Args[i]) continue;
    Args[i]->Emit(ec);
    //ec.EmitPushNumber(Args[i]->Type.Type, Loc);
    ec.AddStatement(OPC_DoWriteOne, Args[i]->Type, Loc);
  }
  if (isWriteln) ec.AddStatement(OPC_DoWriteFlush, Loc);
}


//==========================================================================
//
//  VInvokeWrite::GetVMethod
//
//==========================================================================
VMethod *VInvokeWrite::GetVMethod (VEmitContext &/*ec*/) {
  return nullptr;
}


//==========================================================================
//
//  VInvokeWrite::IsMethodNameChangeable
//
//==========================================================================
bool VInvokeWrite::IsMethodNameChangeable () const {
  return false;
}


//==========================================================================
//
//  VInvokeWrite::GetMethodName
//
//==========================================================================
VName VInvokeWrite::GetMethodName () const {
  return VName(isWriteln ? "writeln" : "write");
}


//==========================================================================
//
//  VInvokeWrite::SetMethodName
//
//==========================================================================
void VInvokeWrite::SetMethodName (VName /*aname*/) {
  VCFatalError("VC: Internal compiler error: `VInvokeWrite::SetMethodName()` called");
}


//==========================================================================
//
//  VInvokeWrite::toString
//
//==========================================================================
VStr VInvokeWrite::toString () const {
  VStr res("write");
  if (isWriteln) res += "ln";
  res += args2str();
  return res;
}
