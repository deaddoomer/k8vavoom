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
  res->Vars.SetNum(Vars.Num());
  for (int f = 0; f < Vars.Num(); ++f) {
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
  Declare(ec);
  return this;
}


//==========================================================================
//
//  VLocalDecl::Emit
//
//==========================================================================
void VLocalDecl::Emit (VEmitContext &ec) {
  EmitInitialisations(ec);
}


//==========================================================================
//
//  VLocalDecl::Declare
//
//==========================================================================
//#include <typeinfo>
void VLocalDecl::Declare (VEmitContext &ec) {
  for (int i = 0; i < Vars.length(); ++i) {
    VLocalEntry &e = Vars[i];

    if (ec.CheckForLocalVar(e.Name) != -1) {
      //const VLocalVarDef &loc = ec.GetLocalByIndex(ec.CheckForLocalVar(e.Name));
      //fprintf(stderr, "duplicate '%s'(%d) (old(%d) is at %s:%d)\n", *e.Name, ec.GetCurrCompIndex(), loc.GetCompIndex(), *loc.Loc.GetSource(), loc.Loc.GetLine());
      ParseError(e.Loc, "Redefined identifier `%s`", *e.Name);
    } else {
      //fprintf(stderr, "NEW '%s'(%d) (%s:%d)\n", *e.Name, ec.GetCurrCompIndex(), *e.Loc.GetSource(), e.Loc.GetLine());
    }

    // resolve automatic type
    if (e.TypeExpr->Type.Type == TYPE_Automatic) {
      VExpression *te = (e.Value ? e.Value : e.TypeOfExpr);
      if (!te) Sys_Error("VC INTERNAL COMPILER ERROR: automatic type without initializer!");
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
          ParseError(e.TypeExpr->Loc, "Cannot infer type for variable `%s`", *e.Name);
          delete e.TypeExpr; // delete old `automatic` type
          e.TypeExpr = VTypeExpr::NewTypeExprFromAuto(VFieldType(TYPE_Int), te->Loc);
        }
      } else {
        auto res = te->SyntaxCopy()->Resolve(ec);
        if (!res) {
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

    e.TypeExpr = e.TypeExpr->ResolveAsType(ec);
    if (!e.TypeExpr) continue;

    //fprintf(stderr, "LOC: <%s>; type: <%s>\n", *e.Name, *e.TypeExpr->Type.GetName());

    VFieldType Type = e.TypeExpr->Type;
    if (Type.Type == TYPE_Void || Type.Type == TYPE_Automatic) ParseError(e.TypeExpr->Loc, "Bad variable type for variable `%s`", *e.Name);

    VLocalVarDef &L = ec.AllocLocal(e.Name, Type, e.Loc);
    L.ParamFlags = (e.isRef ? FPARM_Ref : 0)|(e.isConst ? FPARM_Const : 0);
    //if (e.isRef) fprintf(stderr, "*** <%s:%d> is REF\n", *e.Name, L.ldindex);
    e.locIdx = L.ldindex;

    // resolve initialisation
    if (e.Value) {
      L.Visible = false; // hide from initializer expression
      VExpression *op1 = new VLocalVar(L.ldindex, e.Loc);
      e.Value = new VAssignment(VAssignment::Assign, op1, e.Value, e.Loc);
      e.Value = e.Value->Resolve(ec);
      L.Visible = true; // and make it visible again
      e.emitClear = false;
      // if clear is not necessary, and we are assigning default value, drop assign
      if (!L.reused && !ec.IsInLoop() && e.Value && e.Value->IsAssignExpr() &&
          ((VAssignment *)e.Value)->Oper == VAssignment::Assign &&
          ((VAssignment *)e.Value)->op2)
      {
        VExpression *val = ((VAssignment *)e.Value)->op2;
        bool killInit = false;
        switch (val->Type.Type) {
          case TYPE_Int:
          case TYPE_Byte:
          case TYPE_Bool:
            killInit = (val->IsIntConst() && val->GetIntConst() == 0);
            break;
          case TYPE_Float:
            killInit = (val->IsFloatConst() && val->GetFloatConst() == 0);
            break;
          case TYPE_Name:
            killInit = (val->IsNameConst() && val->GetNameConst() == NAME_None);
            break;
          case TYPE_String:
            killInit = (val->IsStrConst() && val->GetStrConst(ec.Package).length() == 0);
            break;
          case TYPE_Pointer:
            killInit = val->IsNullLiteral();
            break;
          case TYPE_Reference:
          case TYPE_Class:
          case TYPE_State:
            killInit = val->IsNoneLiteral();
            break;
          case TYPE_Delegate:
            killInit = (val->IsNoneDelegateLiteral() || val->IsNoneLiteral() || val->IsNullLiteral());
            break;
          case TYPE_Vector:
            if (val->IsConstVectorCtor()) {
              VVectorExpr *vc = (VVectorExpr *)val;
              TVec vec = vc->GetConstValue();
              killInit = (vec.x == 0 && vec.y == 0 && vec.z == 0);
            }
            break;
        }
        if (killInit) {
          //ParseWarning(Loc, "initializer for `%s` removed", *e.Name);
          //fprintf(stderr, "!!!%s (%s)\n", *e.Name, *val->Type.GetName());
          //fprintf(stderr, "initializer for `%s` removed\n", *e.Name);
          delete e.Value;
          e.Value = nullptr;
        }
      }
    } else {
      //e.emitClear = L.reused;
      //TODO: only set this for locals in loop blocks
      //e.emitClear = true; // always clear, so locals in loop won't retain old values, for example
      e.emitClear = L.reused || ec.IsInLoop();
    }
  }
}


//==========================================================================
//
//  VLocalDecl::EmitInitialisations
//
//==========================================================================
void VLocalDecl::EmitInitialisations (VEmitContext &ec) {
  for (int i = 0; i < Vars.length(); ++i) {
    if (Vars[i].Value) {
      Vars[i].Value->Emit(ec);
    } else if (Vars[i].emitClear) {
      if (Vars[i].locIdx < 0) VCFatalError("VC: internal compiler error (VLocalDecl::EmitInitialisations)");
      ec.EmitOneLocalDtor(Vars[i].locIdx, Loc, true); // zero it
    }
  }
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
VLocalVar::VLocalVar (int ANum, const TLocation &ALoc)
  : VExpression(ALoc)
  , num(ANum)
  , AddressRequested(false)
  , PushOutParam(false)
  , locSavedFlags(0)
{
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
  return this;
}


//==========================================================================
//
//  VLocalVar::RequestAddressOf
//
//==========================================================================
void VLocalVar::RequestAddressOf () {
  if (PushOutParam) {
    PushOutParam = false;
    return;
  }
  if (AddressRequested) ParseError(Loc, "Multiple address of");
  AddressRequested = true;
}


//==========================================================================
//
//  VLocalVar::Emit
//
//==========================================================================
void VLocalVar::Emit (VEmitContext &ec) {
  if (AddressRequested) {
    const VLocalVarDef &loc = ec.GetLocalByIndex(num);
    ec.EmitLocalAddress(loc.Offset, Loc);
  } else if (locSavedFlags&(FPARM_Out|FPARM_Ref)) {
    ec.EmitLocalPtrValue(num, Loc);
    if (PushOutParam) {
      const VLocalVarDef &loc = ec.GetLocalByIndex(num);
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
