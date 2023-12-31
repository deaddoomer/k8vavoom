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

//#define VV_DEBUG_ALLOC_RELEASE


//==========================================================================
//
//  VLocalDecl::VLocalDecl
//
//==========================================================================
VLocalDecl::VLocalDecl (const TLocation &ALoc) : VExpression(ALoc) {
}


//==========================================================================
//
//  VLocalDecl::~VLocalDecl
//
//==========================================================================
VLocalDecl::~VLocalDecl () {
  for (int i = 0; i < Vars.length(); ++i) {
    if (Vars[i].TypeExpr) { delete Vars[i].TypeExpr; Vars[i].TypeExpr = nullptr; }
    if (Vars[i].Value) { delete Vars[i].Value; Vars[i].Value = nullptr; }
    if (Vars[i].TypeOfExpr) { delete Vars[i].TypeOfExpr; Vars[i].TypeOfExpr = nullptr; }
  }
}


//==========================================================================
//
//  VLocalDecl::HasSideEffects
//
//==========================================================================
bool VLocalDecl::HasSideEffects () {
  bool res = false;
  for (int i = 0; !res && i < Vars.length(); ++i) {
    if (Vars[i].Value) res = Vars[i].Value->HasSideEffects();
  }
  return res;
}


//==========================================================================
//
//  VLocalDecl::VisitChildren
//
//==========================================================================
void VLocalDecl::VisitChildren (VExprVisitor *v) {
  for (int i = 0; !v->stopIt && i < Vars.length(); ++i) {
    if (Vars[i].Value) {
      const int odi = v->locDefIdx;
      VLocalDecl *odc = v->locDecl;
      v->locDefIdx = i;
      v->locDecl = this;
      Vars[i].Value->Visit(v);
      v->locDefIdx = odi;
      v->locDecl = odc;
    }
  }
}


//==========================================================================
//
//  VLocalDecl::SyntaxCopy
//
//==========================================================================
VExpression *VLocalDecl::SyntaxCopy () {
  auto res = new VLocalDecl();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VLocalDecl::DoSyntaxCopyTo
//
//==========================================================================
void VLocalDecl::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VLocalDecl *)e;
  res->Vars.setLength(Vars.length());
  for (int f = 0; f < Vars.length(); ++f) {
    res->Vars[f] = Vars[f];
    if (Vars[f].TypeExpr) res->Vars[f].TypeExpr = Vars[f].TypeExpr->SyntaxCopy();
    if (Vars[f].Value) res->Vars[f].Value = Vars[f].Value->SyntaxCopy();
    if (Vars[f].TypeOfExpr) res->Vars[f].TypeOfExpr = Vars[f].TypeOfExpr->SyntaxCopy();
  }
}


//==========================================================================
//
//  VLocalDecl::DoResolve
//
//==========================================================================
VExpression *VLocalDecl::DoResolve (VEmitContext &ec) {
  VCFatalError("internal compiler error: VLocalDecl::DoResolve should not be called directly");
  Declare(ec);
  SetResolved();
  return this;
}


//==========================================================================
//
//  VLocalDecl::Emit
//
//==========================================================================
void VLocalDecl::Emit (VEmitContext &/*ec*/) {
  VCFatalError("internal compiler error: VLocalDecl::Emit should not be called directly");
  //EmitInitialisations(ec);
}


//==========================================================================
//
//  VLocalDecl::EmitInitialisations
//
//==========================================================================
void VLocalDecl::EmitInitialisations (VEmitContext &ec, bool inloop) {
  for (auto &&loc : Vars) {
    if (loc.locIdx < 0) VCFatalError("VC: internal compiler error (VLocalDecl::EmitInitialisations)");
    // do we need to zero variable memory?
    // the variable was properly destructed beforehand (this is invariant)
    const VLocalVarDef &ldef = ec.GetLocalByIndex(loc.locIdx);
    #ifdef VV_DEBUG_ALLOC_RELEASE
    GLog.Logf(NAME_Debug, "VLocalDecl::EmitInitialisations: name=`%s`; idx=%d; ofs=%d; reused=%d; %s", *ldef.Name, loc.locIdx, ldef.Offset, (int)ldef.reused, *ldef.Loc.toStringNoCol());
    #endif
    // zero if reused, and need to be zeroed before initialisation
    const bool needZero = ((inloop || ldef.reused) && ldef.Type.NeedZeroingOnSlotReuse());
    if (needZero) ec.EmitLocalZero(loc.locIdx, Loc);
    // initialise
    if (loc.Value) {
      // emit init assign
      loc.Value->Emit(ec);
    } else if (!needZero && (inloop || ldef.reused)) {
      // if it has no init, and it is reused or in loop, zero it too
      // this is required, so locals will have known default values
      ec.EmitLocalZero(loc.locIdx, Loc);
    }
  }
}


//==========================================================================
//
//  VLocalDecl::EmitDtors
//
//==========================================================================
void VLocalDecl::EmitDtors (VEmitContext &ec) {
  for (auto &&loc : Vars) ec.EmitLocalDtor(loc.locIdx, Loc);
}


//==========================================================================
//
//  VLocalDecl::Allocate
//
//==========================================================================
void VLocalDecl::Allocate (VEmitContext &ec) {
  for (auto &&loc : Vars) {
    ec.AllocateLocalSlot(loc.locIdx, loc.isUnsafe);
    #ifdef VV_DEBUG_ALLOC_RELEASE
    VLocalVarDef &ldef = ec.GetLocalByIndex(loc.locIdx);
    GLog.Logf(NAME_Debug, "VLocalDecl::Allocate: name=`%s`; idx=%d; ofs=%d; reused=%d; %s", *ldef.Name, loc.locIdx, ldef.Offset, (int)ldef.reused, *ldef.Loc.toStringNoCol());
    #endif
  }
}


//==========================================================================
//
//  VLocalDecl::Release
//
//==========================================================================
void VLocalDecl::Release (VEmitContext &ec) {
  for (auto &&loc : Vars) {
    ec.ReleaseLocalSlot(loc.locIdx);
    #ifdef VV_DEBUG_ALLOC_RELEASE
    VLocalVarDef &ldef = ec.GetLocalByIndex(loc.locIdx);
    GLog.Logf(NAME_Debug, "VLocalDecl::Release: name=`%s`; idx=%d; ofs=%d; reused=%d; %s", *ldef.Name, loc.locIdx, ldef.Offset, (int)ldef.reused, *ldef.Loc.toStringNoCol());
    #endif
  }
}


//==========================================================================
//
//  VLocalDecl::Hide
//
//  hide all declared locals
//
//==========================================================================
void VLocalDecl::Hide (VEmitContext &ec) {
  for (auto &&e : Vars) {
    if (e.locIdx >= 0) {
      VLocalVarDef &loc = ec.GetLocalByIndex(e.locIdx);
      loc.Visible = false;
    }
  }
}


//==========================================================================
//
//  VLocalDecl::Declare
//
//==========================================================================
bool VLocalDecl::Declare (VEmitContext &ec) {
  bool retres = true;
  for (int i = 0; i < Vars.length(); ++i) {
    VLocalEntry &e = Vars[i];

    if (!ec.CheckLocalDecl(e.Name, e.Loc)) retres = false;

    // resolve automatic type
    if (e.TypeExpr->Type.Type == TYPE_Automatic) {
      VExpression *te = (e.Value ? e.Value : e.TypeOfExpr);
      if (!te) VCFatalError("VC INTERNAL COMPILER ERROR: automatic type without initializer!");
      if (e.ctorInit) {
        retres = false;
        ParseError(e.Loc, "cannot determine type from ctor for local `%s`", *e.Name);
      }
      // resolve type
      if (e.toeIterArgN >= 0) {
        // special resolving for iterator
        if (te->IsAnyInvocation()) {
          VGagErrors gag;
          VMethod *mnext = ((VInvocationBase *)te)->GetVMethod(ec);
          if (mnext && e.toeIterArgN < mnext->NumParams) {
            //fprintf(stderr, "*** <%s>\n", *mnext->ParamTypes[e.toeIterArgN].GetName()); abort();
            delete e.TypeExpr; // delete old `automatic` type
            e.TypeExpr = VTypeExpr::NewTypeExprFromAuto(mnext->ParamTypes[e.toeIterArgN], te->Loc);
          }
        }
        if (e.TypeExpr->Type.Type == TYPE_Automatic) {
          retres = false;
          ParseError(e.TypeExpr->Loc, "Cannot infer type for variable `%s`", *e.Name);
          delete e.TypeExpr; // delete old `automatic` type
          e.TypeExpr = VTypeExpr::NewTypeExprFromAuto(VFieldType(TYPE_Int), te->Loc);
        }
      } else {
        auto res = te->SyntaxCopy()->Resolve(ec);
        if (!res) {
          retres = false;
          ParseError(e.Loc, "Cannot resolve type for identifier `%s`", *e.Name);
          delete e.TypeExpr; // delete old `automatic` type
          e.TypeExpr = new VTypeExprSimple(TYPE_Void, te->Loc);
        } else {
          //fprintf(stderr, "*** automatic type resolved to `%s`\n", *(res->Type.GetName()));
          delete e.TypeExpr; // delete old `automatic` type
          e.TypeExpr = VTypeExpr::NewTypeExprFromAuto(res->Type, te->Loc);
          delete res;
        }
      }
    }

    //GLog.Logf(NAME_Debug, "LOC:000: <%s>; type: <%s>\n", *e.Name, *e.TypeExpr->toString());
    e.TypeExpr = e.TypeExpr->ResolveAsType(ec);
    if (!e.TypeExpr) {
      retres = false;
      // create dummy local
      VLocalVarDef &L = ec.NewLocal(e.Name, VFieldType(TYPE_Void), VEmitContext::Safe, e.Loc);
      L.ParamFlags = (e.isRef ? FPARM_Ref : 0)|(e.isConst ? FPARM_Const : 0);
      e.locIdx = L.ldindex;
      continue;
    }
    //GLog.Logf(NAME_Debug, "LOC:001: <%s>; type: <%s>\n", *e.Name, *e.TypeExpr->Type.GetName());

    VFieldType Type = e.TypeExpr->Type;
    if (Type.Type == TYPE_Void || Type.Type == TYPE_Automatic) {
      retres = false;
      ParseError(e.TypeExpr->Loc, "Bad variable type for variable `%s`", *e.Name);
    }

    //VLocalVarDef &L = ec.AllocLocal(e.Name, Type, e.Loc);
    VLocalVarDef &L = ec.NewLocal(e.Name, Type, e.isUnsafe, e.Loc);
    L.ParamFlags = (e.isRef ? FPARM_Ref : 0)|(e.isConst ? FPARM_Const : 0);
    //if (e.isRef) fprintf(stderr, "*** <%s:%d> is REF\n", *e.Name, L.ldindex);
    e.locIdx = L.ldindex;

    // resolve initialisation
    if (e.Value) {
      // invocation means "constructor call"
      if (e.ctorInit) {
        if (Type.Type != TYPE_Struct) {
          ParseError(e.Value->Loc, "cannot construct something that is not a struct");
        } else {
          e.Value = e.Value->Resolve(ec);
          if (!e.Value) retres = false;
        }
      } else {
        L.Visible = false; // hide from initializer expression
        VExpression *op1 = new VLocalVar(L.ldindex, L.isUnsafe, e.Loc);
        e.Value = new VAssignment(VAssignment::Assign, op1, e.Value, e.Loc);
        e.Value = e.Value->Resolve(ec);
        if (!e.Value) retres = false;
        L.Visible = true; // and make it visible again
        // if we are assigning default value, drop assign
        if (e.Value && e.Value->IsAssignExpr() &&
            ((VAssignment *)e.Value)->Oper == VAssignment::Assign &&
            ((VAssignment *)e.Value)->op2)
        {
          VExpression *val = ((VAssignment *)e.Value)->op2;
          bool defaultInit = false;
          switch (val->Type.Type) {
            case TYPE_Int:
            case TYPE_Byte:
            case TYPE_Bool:
              defaultInit = (val->IsIntConst() && val->GetIntConst() == 0);
              break;
            case TYPE_Float:
              //if (val->IsFloatConst()) GLog.Logf(NAME_Debug, "var: name=<%s>; idx=<%d>; init=%s; valconst=%g", *e.Name, e.locIdx, *e.Value->toString(), val->GetFloatConst());
              defaultInit = (val->IsFloatConst() && val->GetFloatConst() == 0);
              //if (val->IsFloatConst()) GLog.Logf(NAME_Debug, "var: name=<%s>; idx=<%d>; init=%s; valconst=%g; def=%d (%d)", *e.Name, e.locIdx, *e.Value->toString(), val->GetFloatConst(), (int)(defaultInit), (int)(val->GetFloatConst() == 0));
              break;
            case TYPE_Name:
              defaultInit = (val->IsNameConst() && val->GetNameConst() == NAME_None);
              break;
            case TYPE_String:
              defaultInit = (val->IsStrConst() && val->GetStrConst(ec.Package).length() == 0);
              break;
            case TYPE_Pointer:
              defaultInit = val->IsNullLiteral();
              break;
            case TYPE_Reference:
            case TYPE_Class:
            case TYPE_State:
              defaultInit = val->IsNoneLiteral();
              break;
            case TYPE_Delegate:
              defaultInit = (val->IsNoneDelegateLiteral() || val->IsNoneLiteral() || val->IsNullLiteral());
              break;
            case TYPE_Vector:
              if (val->IsConstVectorCtor()) {
                VVectorExpr *vc = (VVectorExpr *)val;
                const TVec vec = vc->GetConstValue();
                defaultInit = (vec.x == 0 && vec.y == 0 && vec.z == 0);
              }
              break;
          }
          // drop default init if it is equal to default value
          if (defaultInit) {
            delete e.Value;
            e.Value = nullptr;
          }
        }
      }
    }
    #ifdef VV_DEBUG_ALLOC_RELEASE
    {
      VLocalVarDef &ldef = ec.GetLocalByIndex(e.locIdx);
      GLog.Logf(NAME_Debug, "VLocalDecl::Declare(%d): name=`%s`; idx=%d; ofs=%d; reused=%d; %s", i, *ldef.Name, e.locIdx, ldef.Offset, (int)ldef.reused, *ldef.Loc.toStringNoCol());
    }
    #endif
  }
  return retres;
}


//==========================================================================
//
//  VLocalDecl::IsLocalVarDecl
//
//==========================================================================
bool VLocalDecl::IsLocalVarDecl () const {
  return true;
}


//==========================================================================
//
//  VLocalDecl::toString
//
//==========================================================================
VStr VLocalDecl::toString () const {
  VStr res;
  for (int f = 0; f < Vars.length(); ++f) {
    if (res.length()) res += " ";
    res += e2s(Vars[f].TypeExpr);
    res += " ";
    res += *Vars[f].Name;
    res += "("+VStr(Vars[f].locIdx)+")";
    if (Vars[f].Value) res += Vars[f].Value->toString();
    res += ";";
  }
  return res;
}



//==========================================================================
//
//  VLocalVar::VLocalVar
//
//==========================================================================
VLocalVar::VLocalVar (int ANum, bool aUnsafe, const TLocation &ALoc)
  : VExpression(ALoc)
  , num(ANum)
  , AddressRequested(false)
  , PushOutParam(false)
  , locSavedFlags(0)
  , requestedAddr(false)
  , requestedAssAddr(false)
  , isUnsafe(aUnsafe)
{
}


//==========================================================================
//
//  VLocalVar::HasSideEffects
//
//==========================================================================
bool VLocalVar::HasSideEffects () {
  return false;
}


//==========================================================================
//
//  VLocalVar::VisitChildren
//
//==========================================================================
void VLocalVar::VisitChildren (VExprVisitor *v) {
}


//==========================================================================
//
//  VLocalVar::SyntaxCopy
//
//==========================================================================
VExpression *VLocalVar::SyntaxCopy () {
  auto res = new VLocalVar();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VLocalVar::DoSyntaxCopyTo
//
//==========================================================================
void VLocalVar::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VLocalVar *)e;
  res->num = num;
  res->AddressRequested = AddressRequested;
  res->PushOutParam = PushOutParam;
}


//==========================================================================
//
//  VLocalVar::DoResolve
//
//==========================================================================
VExpression *VLocalVar::DoResolve (VEmitContext &ec) {
  const VLocalVarDef &loc = ec.GetLocalByIndex(num);
  locSavedFlags = loc.ParamFlags;
  Type = loc.Type;
  RealType = loc.Type;
  if (Type.Type == TYPE_Byte || Type.Type == TYPE_Bool) Type = VFieldType(TYPE_Int);
  PushOutParam = !!(locSavedFlags&(FPARM_Out|FPARM_Ref));
  if (locSavedFlags&FPARM_Const) SetReadOnly();
  SetResolved();
  return this;
}


//==========================================================================
//
//  VLocalVar::InternalRequestAddressOf
//
//==========================================================================
void VLocalVar::InternalRequestAddressOf () {
  if (PushOutParam) {
    PushOutParam = false;
    return;
  }
  if (AddressRequested) {
    ParseError(Loc, "Multiple address of local (%s)", *toString());
    //VCFatalError("oops");
  }
  AddressRequested = true;
}


//==========================================================================
//
//  VLocalVar::RequestAddressOf
//
//==========================================================================
void VLocalVar::RequestAddressOf () {
  requestedAddr = true;
  InternalRequestAddressOf();
}


//==========================================================================
//
//  VLocalVar::RequestAddressOfForAssign
//
//==========================================================================
void VLocalVar::RequestAddressOfForAssign () {
  requestedAssAddr = true;
  InternalRequestAddressOf();
}


//==========================================================================
//
//  VLocalVar::Emit
//
//==========================================================================
void VLocalVar::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  const VLocalVarDef &loc = ec.GetLocalByIndex(num);

  // check if we won't forgont to allocate the local
  if (loc.Offset == -666) {
    // this means that the local was not properly allocated
    VCFatalError("%s: local #%d (%s) was not properly allocated (internal compiler error)", *Loc.toStringNoCol(), num, (loc.Name != NAME_None ? *loc.Name : "<hidden>"));
  }

  if (requestedAssAddr) {
    /* this gives a lot of false positives
    if (VMemberBase::WarningUnusedLocals) {
      if (ec.IsLocalWrittenByIdx(num) && !ec.IsLocalReadByIdx(num)) {
        const VLocalVarDef &loc = ec.GetLocalByIndex(num);
        if (loc.Name != NAME_None) {
          ParseWarning(Loc, "local `%s` is overwritten without reading", *loc.Name);
        }
      }
    }
    */
    ec.MarkLocalWrittenByIdx(num);
  } else {
    ec.MarkLocalReadByIdx(num);
  }

  //FIXME: mark complex types as used
  switch (Type.Type) {
    case TYPE_Struct:
    case TYPE_Array:
    case TYPE_DynamicArray:
    case TYPE_SliceArray:
    case TYPE_Dictionary:
      ec.MarkLocalUsedByIdx(num);
      break;
  }

  if (AddressRequested) {
    ec.EmitLocalAddress(loc.Offset, Loc);
  } else if (locSavedFlags&(FPARM_Out|FPARM_Ref)) {
    ec.EmitLocalPtrValue(num, Loc);
    if (PushOutParam) {
      EmitPushPointedCode(loc.Type, ec);
    }
  } else {
    ec.EmitLocalValue(num, Loc);
  }
}


//==========================================================================
//
//  VLocalVar::IsLocalVarExpr
//
//==========================================================================
bool VLocalVar::IsLocalVarExpr () const {
  return true;
}


//==========================================================================
//
//  VLocalDecl::toString
//
//==========================================================================
VStr VLocalVar::toString () const {
  VStr res("local(");
  res += VStr(num);
  res += ")";
  return res;
}
