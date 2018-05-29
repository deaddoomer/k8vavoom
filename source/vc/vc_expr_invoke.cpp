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
//  VBaseInvocation::VBaseInvocation
//
//==========================================================================
VInvocationBase::VInvocationBase (int ANumArgs, VExpression **AArgs, const TLocation& ALoc)
  : VExpression(ALoc)
  , NumArgs(ANumArgs)
{
  memset(Args, 0, sizeof(Args)); // why not
  for (int i = 0; i < NumArgs; ++i) Args[i] = AArgs[i];
}

VInvocationBase::~VInvocationBase () {
  for (int i = 0; i < NumArgs; ++i) { delete Args[i]; Args[i] = nullptr; }
  NumArgs = 0;
}

void VInvocationBase::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VInvocationBase *)e;
  memset(res->Args, 0, sizeof(res->Args));
  res->NumArgs = NumArgs;
  for (int i = 0; i < NumArgs; ++i) res->Args[i] = (Args[i] ? Args[i]->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VBaseInvocation::VBaseInvocation
//
//==========================================================================
VBaseInvocation::VBaseInvocation(VName AName, int ANumArgs, VExpression **AArgs, const TLocation& ALoc)
  : VInvocationBase(ANumArgs, AArgs, ALoc)
  , Name(AName)
{
}


//==========================================================================
//
//  VBaseInvocation::SyntaxCopy
//
//==========================================================================
VExpression *VBaseInvocation::SyntaxCopy () {
  auto res = new VBaseInvocation();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VBaseInvocation::DoSyntaxCopyTo
//
//==========================================================================
void VBaseInvocation::DoSyntaxCopyTo (VExpression *e) {
  VInvocationBase::DoSyntaxCopyTo(e);
  auto res = (VBaseInvocation *)e;
  res->Name = Name;
}


//==========================================================================
//
//  VBaseInvocation::DoResolve
//
//==========================================================================
VExpression *VBaseInvocation::DoResolve (VEmitContext &ec) {
  guard(VBaseInvocation::DoResolve);

  if (!ec.SelfClass) {
    ParseError(Loc, ":: not in method");
    delete this;
    return nullptr;
  }

  VMethod *Func = ec.SelfClass->ParentClass->FindMethod(Name);
  if (!Func) {
    ParseError(Loc, "No such method %s", *Name);
    delete this;
    return nullptr;
  }

  VInvocation *e = new VInvocation(nullptr, Func, nullptr, false, true, Loc, NumArgs, Args);
  NumArgs = 0;
  delete this;
  return e->Resolve(ec);
  unguard;
}


//==========================================================================
//
//  VBaseInvocation::Emit
//
//==========================================================================
void VBaseInvocation::Emit (VEmitContext &) {
  guard(VBaseInvocation::Emit);
  ParseError(Loc, "Should not happen (VBaseInvocation)");
  unguard;
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
  VClass *Class = VMemberBase::StaticFindClass(Name);
  if (Class) {
    if (NumArgs != 1 || !Args[0]) {
      ParseError(Loc, "Dynamic cast requires 1 argument");
      delete this;
      return nullptr;
    }
    VExpression* e = new VDynamicCast(Class, Args[0], Loc);
    NumArgs = 0;
    delete this;
    return e->Resolve(ec);
  }

  if (ec.SelfClass) {
    VMethod *M = ec.SelfClass->FindMethod(Name);
    if (M) {
      if (M->Flags & FUNC_Iterator) {
        ParseError(Loc, "Iterator methods can only be used in foreach statements");
        delete this;
        return nullptr;
      }
      VInvocation* e = new VInvocation(nullptr, M, nullptr, false, false, Loc, NumArgs, Args);
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }

    VField *field = ec.SelfClass->FindField(Name, Loc, ec.SelfClass);
    if (field != nullptr && field->Type.Type == TYPE_Delegate) {
      VInvocation* e = new VInvocation(nullptr, field->Func, field, false, false, Loc, NumArgs, Args);
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }
  }

  ParseError(Loc, "Unknown method %s", *Name);
  delete this;
  return nullptr;
}


//==========================================================================
//
//  VCastOrInvocation::ResolveIterator
//
//==========================================================================
VExpression *VCastOrInvocation::ResolveIterator (VEmitContext &ec) {
  VMethod *M = ec.SelfClass->FindMethod(Name);
  if (!M) {
    ParseError(Loc, "Unknown method %s", *Name);
    delete this;
    return nullptr;
  }
  if ((M->Flags&FUNC_Iterator) == 0) {
    ParseError(Loc, "%s is not an iterator method", *Name);
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
//  VDotInvocation::DoResolve
//
//==========================================================================
VExpression *VDotInvocation::DoResolve (VEmitContext &ec) {
  if (SelfExpr) SelfExpr = SelfExpr->Resolve(ec);
  if (!SelfExpr) {
    delete this;
    return nullptr;
  }

  if (SelfExpr->Type.Type == TYPE_DynamicArray) {
    if (MethodName == NAME_Insert) {
      if (NumArgs == 1) {
        // default count is 1
        Args[1] = new VIntLiteral(1, Loc);
        NumArgs = 2;
      }
      if (NumArgs != 2) {
        ParseError(Loc, "Insert requires 1 or 2 arguments");
        delete this;
        return nullptr;
      }
      VExpression *e = new VDynArrayInsert(SelfExpr, Args[0], Args[1], Loc);
      SelfExpr = nullptr;
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }

    if (MethodName == NAME_Remove) {
      if (NumArgs == 1) {
        // default count is 1
        Args[1] = new VIntLiteral(1, Loc);
        NumArgs = 2;
      }
      if (NumArgs != 2) {
        ParseError(Loc, "Insert requires 1 or 2 arguments");
        delete this;
        return nullptr;
      }
      VExpression *e = new VDynArrayRemove(SelfExpr, Args[0], Args[1], Loc);
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
  if (SelfExpr->Type.Type == TYPE_Class) {
    if (!SelfExpr->Type.Class) {
      ParseError(Loc, "Class name expected at the left side of `.`");
      delete this;
      return nullptr;
    }
    VMethod *M = SelfExpr->Type.Class->FindMethod(MethodName);
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

  if (SelfExpr->Type.Type != TYPE_Reference) {
    ParseError(Loc, "Object reference expected at the left side of `.`");
    delete this;
    return nullptr;
  }

  VMethod *M = SelfExpr->Type.Class->FindMethod(MethodName);
  if (M) {
    if (M->Flags & FUNC_Iterator) {
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

  VField *field = SelfExpr->Type.Class->FindField(MethodName, Loc, ec.SelfClass);
  if (field && field->Type.Type == TYPE_Delegate) {
    VExpression *e = new VInvocation(SelfExpr, field->Func, field, true, false, Loc, NumArgs, Args);
    SelfExpr = nullptr;
    NumArgs = 0;
    delete this;
    return e->Resolve(ec);
  }

  ParseError(Loc, "No such method %s", *MethodName);
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

  VMethod *M = SelfExpr->Type.Class->FindMethod(MethodName);
  if (!M) {
    ParseError(Loc, "No such method %s", *MethodName);
    delete this;
    return nullptr;
  }
  if ((M->Flags&FUNC_Iterator) == 0) {
    ParseError(Loc, "%s is not an iterator method", *MethodName);
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
//  VInvocation::VInvocation
//
//==========================================================================

VInvocation::VInvocation (VExpression* ASelfExpr, VMethod* AFunc, VField* ADelegateField,
                          bool AHaveSelf, bool ABaseCall, const TLocation& ALoc, int ANumArgs,
                          VExpression** AArgs)
  : VInvocationBase(ANumArgs, AArgs, ALoc)
  , SelfExpr(ASelfExpr)
  , Func(AFunc)
  , DelegateField(ADelegateField)
  , HaveSelf(AHaveSelf)
  , BaseCall(ABaseCall)
  , CallerState(nullptr)
  , MultiFrameState(false)
{
}


//==========================================================================
//
//  VInvocation::~VInvocation
//
//==========================================================================
VInvocation::~VInvocation() {
  if (SelfExpr) { delete SelfExpr; SelfExpr = nullptr; }
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
  res->SelfExpr = (SelfExpr ? SelfExpr->SyntaxCopy() : nullptr);
  res->Func = Func;
  res->DelegateField = DelegateField;
  res->HaveSelf = HaveSelf;
  res->BaseCall = BaseCall;
  res->CallerState = CallerState;
  res->MultiFrameState = MultiFrameState;
}


//==========================================================================
//
//  VInvocation::DoResolve
//
//==========================================================================
VExpression *VInvocation::DoResolve (VEmitContext &ec) {
  guard(VInvocation::DoResolve);
  if (ec.Package->Name == NAME_decorate) CheckDecorateParams(ec);

  // resolve arguments
  bool ArgsOk = true;
  for (int i = 0; i < NumArgs; ++i) {
    if (Args[i]) {
      Args[i] = Args[i]->Resolve(ec);
      if (!Args[i]) ArgsOk = false;
    }
  }

  if (!ArgsOk) {
    delete this;
    return nullptr;
  }

  CheckParams(ec);

  Type = Func->ReturnType;
  if (Type.Type == TYPE_Byte || Type.Type == TYPE_Bool) Type = VFieldType(TYPE_Int);
  if (Func->Flags&FUNC_Spawner) Type.Class = Args[0]->Type.Class;

  return this;
  unguard;
}


//==========================================================================
//
//  VInvocation::Emit
//
//==========================================================================
void VInvocation::Emit (VEmitContext &ec) {
  guard(VInvocation::Emit);
  if (SelfExpr) SelfExpr->Emit(ec);

  bool DirectCall = (BaseCall || (Func->Flags&FUNC_Final) != 0);

  if (Func->Flags&FUNC_Static) {
    if (HaveSelf) ParseError(Loc, "Invalid static function call");
  } else {
    if (!HaveSelf) {
      if (ec.CurrentFunc->Flags & FUNC_Static) ParseError(Loc, "An object is required to call non-static methods");
      ec.AddStatement(OPC_LocalValue0);
    }
  }

  vint32 SelfOffset = 1;
  for (int i = 0; i < NumArgs; ++i) {
    if (!Args[i]) {
      switch (Func->ParamTypes[i].Type) {
        case TYPE_Int:
        case TYPE_Byte:
        case TYPE_Bool:
        case TYPE_Float:
        case TYPE_Name:
          ec.EmitPushNumber(0);
          ++SelfOffset;
          break;
        case TYPE_String:
        case TYPE_Pointer:
        case TYPE_Reference:
        case TYPE_Class:
        case TYPE_State:
          ec.AddStatement(OPC_PushNull);
          ++SelfOffset;
          break;
        case TYPE_Vector:
          ec.EmitPushNumber(0);
          ec.EmitPushNumber(0);
          ec.EmitPushNumber(0);
          SelfOffset += 3;
          break;
        default:
          ParseError(Loc, "Bad optional parameter type");
          break;
      }
      ec.EmitPushNumber(0);
      ++SelfOffset;
    } else {
      Args[i]->Emit(ec);
      SelfOffset += (Args[i]->Type.Type == TYPE_Vector ? 3 : 1);
      if (Func->ParamFlags[i]&FPARM_Optional) {
        ec.EmitPushNumber(1);
        ++SelfOffset;
      }
    }
  }

       if (DirectCall) ec.AddStatement(OPC_Call, Func);
  else if (DelegateField) ec.AddStatement(OPC_DelegateCall, DelegateField, SelfOffset);
  else ec.AddStatement(OPC_VCall, Func, SelfOffset);
  unguard;
}


//==========================================================================
//
//  VInvocation::FindMethodWithSignature
//
//==========================================================================
VMethod *VInvocation::FindMethodWithSignature (VEmitContext &ec, VName name, int argc, VExpression **argv) {
  if (argc < 0 || argc > VMethod::MAX_PARAMS) return nullptr;
  if (!ec.SelfClass) return nullptr;
  VMethod *m = ec.SelfClass->FindMethod(name);
  if (!m) return nullptr;
  if (!IsGoodMethodParams(ec, m, argc, argv)) return nullptr;
  return m;
}


//==========================================================================
//
//  VInvocation::IsGoodMethodParams
//
//==========================================================================
bool VInvocation::IsGoodMethodParams (VEmitContext &ec, VMethod *m, int argc, VExpression **argv) {
  if (argc < 0 || argc > VMethod::MAX_PARAMS) return false;
  if (!m) return false;

  // determine parameter count
  int requiredParams = m->NumParams;
  int maxParams = (m->Flags&FUNC_VarArgs ? VMethod::MAX_PARAMS-1 : m->NumParams);

  for (int i = 0; i < argc; ++i) {
    if (i < requiredParams) {
      if (!argv[i]) {
        if (!(m->ParamFlags[i]&FPARM_Optional)) return false; // ommited non-optional
        continue;
      }
      if (ec.Package->Name == NAME_decorate) {
        switch (m->ParamTypes[i].Type) {
          case TYPE_Int:
          case TYPE_Float:
            if (argv[i]->Type.Type == TYPE_Float || argv[i]->Type.Type == TYPE_Int) continue;
            break;
        }
      }
      if (m->ParamFlags[i]&FPARM_Out) {
        if (!argv[i]->Type.Equals(m->ParamTypes[i])) {
          //FIXME: This should be error
          if (!(m->ParamFlags[argc]&FPARM_Optional)) {
            // check, but don't raise any errors
            if (!argv[i]->Type.CheckMatch(argv[i]->Loc, m->ParamTypes[i], false)) return false;
            //ParseError(Args[i]->Loc, "Out parameter types must be equal");
          }
        }
      } else {
        if (m->ParamTypes[i].Type == TYPE_Float && argv[i]->Type.Type == TYPE_Int) continue;
        // check, but don't raise any errors
        if (!argv[i]->Type.CheckMatch(argv[i]->Loc, m->ParamTypes[i], false)) return false;;
      }
    } else if (!argv[i]) {
      return false;
    }
  }

  if (argc > maxParams) return false;

  while (argc < requiredParams) {
    if (!(m->ParamFlags[argc]&FPARM_Optional)) return false;
    ++argc;
  }

  return true;
}


//==========================================================================
//
//  VInvocation::CheckParams
//
//==========================================================================
void VInvocation::CheckParams (VEmitContext &ec) {
  guard(VInvocation::CheckParams);

  // determine parameter count
  int argsize = 0;
  int requiredParams = Func->NumParams;
  int maxParams = (Func->Flags&FUNC_VarArgs ? VMethod::MAX_PARAMS-1 : Func->NumParams);

  for (int i = 0; i < NumArgs; ++i) {
    if (i < requiredParams) {
      if (!Args[i]) {
        if (!(Func->ParamFlags[i] & FPARM_Optional)) ParseError(Loc, "Cannot omit non-optional argument");
        argsize += Func->ParamTypes[i].GetStackSize();
      } else {
        if (ec.Package->Name == NAME_decorate) {
          switch (Func->ParamTypes[i].Type) {
            case TYPE_Int:
              if (Args[i]->IsFloatConst()) {
                int Val = (int)(Args[i]->GetFloatConst());
                TLocation Loc = Args[i]->Loc;
                delete Args[i];
                Args[i] = nullptr;
                Args[i] = new VIntLiteral(Val, Loc);
                Args[i] = Args[i]->Resolve(ec);
              } else if (Args[i]->Type.Type == TYPE_Float) {
                Args[i] = (new VScalarToInt(Args[i]))->Resolve(ec);
              }
              break;
            case TYPE_Float:
              if (Args[i]->IsIntConst()) {
                int Val = Args[i]->GetIntConst();
                TLocation Loc = Args[i]->Loc;
                delete Args[i];
                Args[i] = nullptr;
                Args[i] = new VFloatLiteral(Val, Loc);
                Args[i] = Args[i]->Resolve(ec);
              } else if (Args[i]->Type.Type == TYPE_Int) {
                Args[i] = (new VScalarToFloat(Args[i]))->Resolve(ec);
              }
              break;
          }
        }
        if (Func->ParamFlags[i]&FPARM_Out) {
          if (!Args[i]->Type.Equals(Func->ParamTypes[i])) {
            //FIXME: This should be error
            if (!(Func->ParamFlags[NumArgs]&FPARM_Optional)) {
              Args[i]->Type.CheckMatch(Args[i]->Loc, Func->ParamTypes[i]);
              //ParseError(Args[i]->Loc, "Out parameter types must be equal");
            }
          }
          Args[i]->RequestAddressOf();
        } else {
          if (!(Func->ParamFlags[NumArgs]&FPARM_Optional)) {
            if (Func->ParamTypes[i].Type == TYPE_Float) {
              if (Args[i]->IsIntConst()) {
                int Val = Args[i]->GetIntConst();
                TLocation Loc = Args[i]->Loc;
                delete Args[i];
                Args[i] = nullptr;
                Args[i] = new VFloatLiteral(Val, Loc);
                Args[i] = Args[i]->Resolve(ec);
              } else if (Args[i]->Type.Type == TYPE_Int) {
                Args[i] = (new VScalarToFloat(Args[i]))->Resolve(ec);
              }
            }
            Args[i]->Type.CheckMatch(Args[i]->Loc, Func->ParamTypes[i]);
          }
        }
        argsize += Args[i]->Type.GetStackSize();
      }
    } else if (!Args[i]) {
           if (Func->Flags&FUNC_VarArgs) ParseError(Loc, "Cannot omit arguments for vararg function");
      else if (i >= Func->NumParams) ParseError(Loc, "Cannot omit extra arguments for vararg function");
      else ParseError(Loc, "Cannot omit argument (for some reason)");
    } else {
      argsize += Args[i]->Type.GetStackSize();
    }
  }

  if (NumArgs > maxParams) ParseError(Loc, "Incorrect number of arguments, need %d, got %d.", maxParams, NumArgs);

  while (NumArgs < requiredParams) {
    if (Func->ParamFlags[NumArgs] & FPARM_Optional) {
      Args[NumArgs] = nullptr;
      ++NumArgs;
    } else {
      ParseError(Loc, "Incorrect argument count %d, should be %d", NumArgs, requiredParams);
      break;
    }
  }

  if (Func->Flags&FUNC_VarArgs) {
    Args[NumArgs++] = new VIntLiteral(argsize/4-requiredParams, Loc);
  }

  unguard;
}


//==========================================================================
//
//  VInvocation::CheckDecorateParams
//
//==========================================================================
void VInvocation::CheckDecorateParams (VEmitContext &ec) {
  guard(VInvocation::CheckDecorateParams);

  int maxParams;
  int requiredParams = Func->NumParams;
  if (Func->Flags & FUNC_VarArgs) {
    maxParams = VMethod::MAX_PARAMS-1;
  } else {
    maxParams = Func->NumParams;
  }

  if (NumArgs > maxParams) ParseError(Loc, "Incorrect number of arguments, need %d, got %d.", maxParams, NumArgs);

  for (int i = 0; i < NumArgs; ++i) {
    if (i >= requiredParams) continue;
    if (!Args[i]) continue;
    switch (Func->ParamTypes[i].Type) {
      case TYPE_Name:
        if (Args[i]->IsDecorateSingleName()) {
          VDecorateSingleName *E = (VDecorateSingleName *)Args[i];
          Args[i] = new VNameLiteral(*E->Name, E->Loc);
          delete E;
          E = nullptr;
        } else if (Args[i]->IsStrConst()) {
          VStr Val = Args[i]->GetStrConst(ec.Package);
          TLocation ALoc = Args[i]->Loc;
          delete Args[i];
          Args[i] = nullptr;
          Args[i] = new VNameLiteral(*Val, ALoc);
        }
        break;
      case TYPE_String:
        if (Args[i]->IsDecorateSingleName()) {
          VDecorateSingleName *E = (VDecorateSingleName *)Args[i];
          Args[i] = new VStringLiteral(ec.Package->FindString(*E->Name), E->Loc);
          delete E;
          E = nullptr;
        }
        break;
      case TYPE_Class:
        if (Args[i]->IsDecorateSingleName()) {
          VDecorateSingleName *E = (VDecorateSingleName *)Args[i];
          Args[i] = new VStringLiteral(ec.Package->FindString(*E->Name), E->Loc);
          delete E;
          E = nullptr;
        }
        if (Args[i]->IsStrConst()) {
          VStr CName = Args[i]->GetStrConst(ec.Package);
          TLocation ALoc = Args[i]->Loc;
          VClass *Cls = VClass::FindClassNoCase(*CName);
          if (!Cls) {
            ParseWarning(ALoc, "No such class %s", *CName);
            delete Args[i];
            Args[i] = nullptr;
            Args[i] = new VNoneLiteral(ALoc);
          } else if (Func->ParamTypes[i].Class && !Cls->IsChildOf(Func->ParamTypes[i].Class)) {
            ParseWarning(ALoc, "Class %s is not a descendant of %s", *CName, Func->ParamTypes[i].Class->GetName());
            delete Args[i];
            Args[i] = nullptr;
            Args[i] = new VNoneLiteral(ALoc);
          } else {
            delete Args[i];
            Args[i] = nullptr;
            Args[i] = new VClassConstant(Cls, ALoc);
          }
        }
        break;
      case TYPE_State:
        if (Args[i]->IsIntConst()) {
          int Offs = Args[i]->GetIntConst();
          TLocation ALoc = Args[i]->Loc;
          if (Offs < 0) {
            ParseError(ALoc, "Negative state jumps are not allowed");
          } else if (Offs == 0) {
            // 0 means no state
            delete Args[i];
            Args[i] = nullptr;
            Args[i] = new VNoneLiteral(ALoc);
          } else {
            check(CallerState);
            VState* S = CallerState->GetPlus(Offs, true);
            if (!S) {
              ParseError(ALoc, "Bad state jump offset");
            } else {
              delete Args[i];
              Args[i] = nullptr;
              Args[i] = new VStateConstant(S, ALoc);
            }
          }
        } else if (Args[i]->IsStrConst()) {
          VStr Lbl = Args[i]->GetStrConst(ec.Package);
          TLocation ALoc = Args[i]->Loc;
          int DCol = Lbl.IndexOf("::");
          if (DCol >= 0) {
            // jump to a specific parent class state, resolve it and pass value directly
            VStr ClassName(Lbl, 0, DCol);
            VClass *CheckClass;
            if (ClassName.ICmp("Super")) {
              CheckClass = ec.SelfClass->ParentClass;
            } else {
              CheckClass = VClass::FindClassNoCase(*ClassName);
              if (!CheckClass) {
                ParseError(ALoc, "No such class %s", *ClassName);
              } else if (!ec.SelfClass->IsChildOf(CheckClass)) {
                ParseError(ALoc, "%s is not a subclass of %s", ec.SelfClass->GetName(), CheckClass->GetName());
                CheckClass = nullptr;
              }
            }
            if (CheckClass) {
              VStr LblName(Lbl, DCol+2, Lbl.Length()-DCol-2);
              TArray<VName> Names;
              VMemberBase::StaticSplitStateLabel(LblName, Names);
              VStateLabel *StLbl = CheckClass->FindStateLabel(Names, true);
              if (!StLbl) {
                ParseError(ALoc, "No such state %s", *Lbl);
              } else {
                delete Args[i];
                Args[i] = nullptr;
                Args[i] = new VStateConstant(StLbl->State, ALoc);
              }
            }
          } else {
            // it's a virtual state jump
            VExpression* TmpArgs[1];
            TmpArgs[0] = Args[i];
            Args[i] = new VInvocation(nullptr, ec.SelfClass->FindMethodChecked("FindJumpState"), nullptr, false, false, Args[i]->Loc, 1, TmpArgs);
          }
        }
        break;
    }
  }
  unguard;
}
