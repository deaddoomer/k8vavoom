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
//  VExprParens::VExprParens
//
//==========================================================================
VExprParens::VExprParens (VExpression *AOp, const TLocation &ALoc)
  : VExpression(ALoc)
  , op(AOp)
{
}


//==========================================================================
//
//  VExprParens::~VExprParens
//
//==========================================================================
VExprParens::~VExprParens () {
  if (op) { delete op; op = nullptr; }
}


//==========================================================================
//
//  VExprParens::SyntaxCopy
//
//==========================================================================
VExpression *VExprParens::SyntaxCopy () {
  auto res = new VExprParens();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VExprParens::DoSyntaxCopyTo
//
//==========================================================================
void VExprParens::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VExprParens *)e;
  res->op = (op ? op->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VExprParens::DoResolve
//
//==========================================================================
VExpression *VExprParens::DoResolve (VEmitContext &ec) {
  VExpression *res = (op ? op->Resolve(ec) : nullptr);
  op = nullptr;
  delete this;
  return res;
}


//==========================================================================
//
//  VExprParens::Emit
//
//==========================================================================
void VExprParens::Emit (VEmitContext &) {
  VCFatalError("VExprParens::Emit: the thing that should not be");
}


//==========================================================================
//
//  VExprParens::IsParens
//
//==========================================================================
bool VExprParens::IsParens () const {
  return true;
}


//==========================================================================
//
//  VExprParens::toString
//
//==========================================================================
VStr VExprParens::toString () const {
  return VStr("(")+e2s(op)+")";
}


//==========================================================================
//
//  VUnary::VUnary
//
//==========================================================================
VUnary::VUnary (EUnaryOp AOper, VExpression *AOp, const TLocation &ALoc, bool aopresolved, bool aFromDecorate)
  : VExpression(ALoc)
  , Oper(AOper)
  , op(AOp)
  , opresolved(aopresolved)
  , FromDecorate(aFromDecorate)
{
  if (!op) ParseError(Loc, "Expression expected");
}


//==========================================================================
//
//  VUnary::~VUnary
//
//==========================================================================
VUnary::~VUnary () {
  if (op) { delete op; op = nullptr; }
}


//==========================================================================
//
//  VUnary::SyntaxCopy
//
//==========================================================================
VExpression *VUnary::SyntaxCopy () {
  auto res = new VUnary();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VUnary::IsUnaryMath
//
//==========================================================================
bool VUnary::IsUnaryMath () const {
  return true;
}


//==========================================================================
//
//  VUnary::DoSyntaxCopyTo
//
//==========================================================================
void VUnary::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VUnary *)e;
  res->Oper = Oper;
  res->op = (op ? op->SyntaxCopy() : nullptr);
  res->opresolved = opresolved;
  res->FromDecorate = FromDecorate;
}


//==========================================================================
//
//  VUnary::DoResolve
//
//==========================================================================
VExpression *VUnary::DoResolve (VEmitContext &ec) {
  if (Oper == TakeAddress && op && ec.SelfClass) {
    vassert(!opresolved);
    if (op->IsSingleName()) {
      VMethod *M = nullptr;
      if (ec.SelfStruct) M = ec.SelfStruct->FindAccessibleMethod(((VSingleName *)op)->Name, ec.SelfStruct, &Loc);
      if (!M) M = ec.SelfClass->FindAccessibleMethod(((VSingleName *)op)->Name, ec.SelfClass, &Loc);
      if (M && (M->Flags&FUNC_Iterator) == 0) {
        //fprintf(stderr, "SINGLE NAME GETADDR! <%s>\n", *((VSingleName *)op)->Name);
        VExpression *e = new VSelf(Loc);
        if (e) e = e->Resolve(ec);
        if (e) e = new VDelegateVal(e, M, Loc);
        delete this;
        return (e ? e->Resolve(ec) : nullptr);
      }
    } else if (op->IsDotField()) {
      VExpression *xop = ((VDotField *)op)->op;
      VName fname = *((VDotField *)op)->FieldName;
      if (xop) {
        xop = xop->SyntaxCopy();
        xop = xop->Resolve(ec);
        if (xop) {
          if (xop->Type.Type == TYPE_Reference) {
            VMethod *M = xop->Type.Class->FindAccessibleMethod(fname, ec.SelfClass, &Loc);
            if (M && (M->Flags&FUNC_Iterator) == 0) {
              //fprintf(stderr, "DOTFIELD NAME GETADDR! <%s>\n", *((VDotField *)op)->FieldName);
              VExpression *e = new VDelegateVal(xop, M, Loc);
              delete this;
              return e->Resolve(ec);
            } else {
              //fprintf(stderr, "FUCK DOTFIELD NAME GETADDR! <%s> %p\n", *((VDotField *)op)->FieldName, M);
            }
          }
          delete xop;
        }
      }
    }
  }

  if (op && !opresolved) {
    if (Oper == Not) op = op->ResolveBoolean(ec); else op = op->Resolve(ec);
    opresolved = true;
  }
  if (!op) { delete this; return nullptr; }

  switch (Oper) {
    case Plus:
      Type = op->Type;
      if (op->Type.Type != TYPE_Int && op->Type.Type != TYPE_Float && op->Type.Type == TYPE_Vector) {
        ParseError(Loc, "Expression type mismatch");
        delete this;
        return nullptr;
      } else {
        VExpression *e = op;
        op = nullptr;
        delete this;
        return e;
      }
    case Minus:
           if (op->Type.Type == TYPE_Int) Type = TYPE_Int;
      else if (op->Type.Type == TYPE_Float) Type = TYPE_Float;
      else if (op->Type.Type == TYPE_Vector) Type = op->Type;
      else {
        ParseError(Loc, "Expression type mismatch");
        delete this;
        return nullptr;
      }
      break;
    case Not:
      Type = TYPE_Int;
      break;
    case BitInvert:
      if (FromDecorate && op->Type.Type == TYPE_Float) {
        op = new VScalarToInt(op, false);
        if (op) op = op->Resolve(ec);
        if (!op) { delete this; return nullptr; }
      }
      if (op->Type.Type != TYPE_Int) {
        ParseError(Loc, "Expression type mismatch");
        delete this;
        return nullptr;
      }
      Type = TYPE_Int;
      break;
    case TakeAddress:
      if (op->Type.Type == TYPE_Reference) {
        ParseError(Loc, "Tried to take address of reference");
        delete this;
        return nullptr;
      } else {
        op->RequestAddressOf();
        Type = op->RealType.MakePointerType();
      }
      break;
  }

  // optimize integer constants
  if (op->IsIntConst()) {
    vint32 Value = op->GetIntConst();
    VExpression *e = nullptr;
    switch (Oper) {
      case Minus: e = new VIntLiteral(-Value, Loc); break;
      case Not: e = new VIntLiteral(!Value, Loc); break;
      case BitInvert: e = new VIntLiteral(~Value, Loc); break;
      default: break;
    }
    if (e) {
      delete this;
      return e->Resolve(ec);
    }
  }

  // lognot operation resolved to boolean (int, actually), no need to check for it
  if (Oper == Minus) {
    // optimize float constants
    if (op->IsFloatConst()) {
      float Value = op->GetFloatConst();
      VExpression *e = new VFloatLiteral(-Value, Loc);
      delete this;
      return e->Resolve(ec);
    }

    // optimize vector constants
    if (op->IsConstVectorCtor()) {
      TVec v = ((VVectorExpr *)op)->GetConstValue();
      v = -v;
      VExpression *e = new VVectorExpr(v, op->Loc);
      delete this;
      return e->Resolve(ec);
    }
  }

  SetResolved();
  return this;
}


//==========================================================================
//
//  VUnary::Emit
//
//==========================================================================
void VUnary::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  op->Emit(ec);

  switch (Oper) {
    case Plus:
      break;
    case Minus:
           if (op->Type.Type == TYPE_Int) ec.AddStatement(OPC_UnaryMinus, Loc);
      else if (op->Type.Type == TYPE_Float) ec.AddStatement(OPC_FUnaryMinus, Loc);
      else if (op->Type.Type == TYPE_Vector) ec.AddStatement(OPC_VUnaryMinus, Loc);
      break;
    case Not:
      ec.AddStatement(OPC_NegateLogical, Loc);
      break;
    case BitInvert:
      ec.AddStatement(OPC_BitInverse, Loc);
      break;
    case TakeAddress:
      break;
  }
}


//==========================================================================
//
//  VUnary::EmitBranchable
//
//==========================================================================
void VUnary::EmitBranchable (VEmitContext &ec, VLabel Lbl, bool OnTrue) {
  //WARNING! do not optimise here, external optimiser will do the work (so we can avoid emiting drops)
  EmitCheckResolved(ec);
  if (Oper == Not) {
    op->EmitBranchable(ec, Lbl, !OnTrue);
  } else {
    VExpression::EmitBranchable(ec, Lbl, OnTrue);
  }
}


//==========================================================================
//
//  VUnary::getOpName
//
//==========================================================================
const char *VUnary::getOpName () const {
  switch (Oper) {
    case Plus: return "+";
    case Minus: return "-";
    case Not: return "!";
    case BitInvert: return "~";
    case TakeAddress: return "&";
  }
  return "wtf?!";
}


//==========================================================================
//
// VUnary::toString
//
//==========================================================================
VStr VUnary::toString () const {
  VStr res;
  res += getOpName();
  res += "(";
  res += e2s(op);
  res += ")";
  return res;
}


//==========================================================================
//
//  VUnaryMutator::VUnaryMutator
//
//==========================================================================
VUnaryMutator::VUnaryMutator (EIncDec AOper, VExpression *AOp, const TLocation &ALoc)
  : VExpression(ALoc)
  , Oper(AOper)
  , op(AOp)
{
  if (!op) ParseError(Loc, "Expression expected");
}


//==========================================================================
//
//  VUnaryMutator::~VUnaryMutator
//
//==========================================================================
VUnaryMutator::~VUnaryMutator () {
  if (op) { delete op; op = nullptr; }
}


//==========================================================================
//
//  VUnaryMutator::IsUnaryMutator
//
//==========================================================================
bool VUnaryMutator::IsUnaryMutator () const {
  return true;
}


//==========================================================================
//
//  VUnaryMutator::SyntaxCopy
//
//==========================================================================
VExpression *VUnaryMutator::SyntaxCopy () {
  auto res = new VUnaryMutator();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VUnaryMutator::DoSyntaxCopyTo
//
//==========================================================================
void VUnaryMutator::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VUnaryMutator *)e;
  res->Oper = Oper;
  res->op = (op ? op->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VUnaryMutator::DoResolve
//
//==========================================================================
VExpression *VUnaryMutator::DoResolve (VEmitContext &ec) {
  if (op) op = op->Resolve(ec);
  if (!op) {
    delete this;
    return nullptr;
  }

  if (op->Type.Type != TYPE_Int) {
    ParseError(Loc, "Expression type mismatch");
    delete this;
    return nullptr;
  }

  if (op->IsReadOnly()) {
    ParseError(Loc, "Cannot modify read-only destination (0)");
    delete this;
    return nullptr;
  }

  if (IsReadOnly()) {
    ParseError(Loc, "Cannot modify read-only destination (1)");
    delete this;
    return nullptr;
  }

  Type = TYPE_Int;
  op->RequestAddressOf();
  SetResolved();
  return this;
}


//==========================================================================
//
//  VUnaryMutator::Emit
//
//==========================================================================
void VUnaryMutator::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  op->Emit(ec);
  switch (Oper) {
    case PreInc: ec.AddStatement(OPC_PreInc, Loc); break;
    case PreDec: ec.AddStatement(OPC_PreDec, Loc); break;
    case PostInc: ec.AddStatement(OPC_PostInc, Loc); break;
    case PostDec: ec.AddStatement(OPC_PostDec, Loc); break;
    case Inc: ec.AddStatement(OPC_IncDrop, Loc); break;
    case Dec: ec.AddStatement(OPC_DecDrop, Loc); break;
  }
}


//==========================================================================
//
//  VUnaryMutator::AddDropResult
//
//==========================================================================
VExpression *VUnaryMutator::AddDropResult () {
  switch (Oper) {
    case PreInc: case PostInc: Oper = Inc; break;
    case PreDec: case PostDec: Oper = Dec; break;
    case Inc: case Dec: VCFatalError("Should not happen (inc/dec)");
  }
  Type = TYPE_Void;
  return this;
}


//==========================================================================
//
//  VUnaryMutator::getOpName
//
//==========================================================================
const char *VUnaryMutator::getOpName () const {
  switch (Oper) {
    case PreInc: return "++(pre)";
    case PreDec: return "--(pre)";
    case PostInc: return "++(post)";
    case PostDec: return "--(post)";
    case Inc: return "(++)";
    case Dec: return "(--)";
  }
  return "wtf?!";
}


//==========================================================================
//
//  VUnaryMutator::toString
//
//==========================================================================
VStr VUnaryMutator::toString () const {
  VStr res;
  switch (Oper) {
    case PreInc: res += "++"; break;
    case PreDec: res += "--"; break;
    default: break;
  }
  res += e2s(op);
  switch (Oper) {
    case PostInc: res += "++"; break;
    case PostDec: res += "--"; break;
    case Inc: res += "(++)"; break;
    case Dec: res += "(--)"; break;
    default: break;
  }
  return res;
}


//==========================================================================
//
//  VBinary::VBinary
//
//==========================================================================
VBinary::VBinary (EBinOp AOper, VExpression *AOp1, VExpression *AOp2, const TLocation &ALoc, bool aFromDecorate)
  : VExpression(ALoc)
  , Oper(AOper)
  , op1(AOp1)
  , op2(AOp2)
  , FromDecorate(aFromDecorate)
{
  if (!op2) ParseError(Loc, "Expression expected");
}


//==========================================================================
//
//  VBinary::~VBinary
//
//==========================================================================
VBinary::~VBinary () {
  if (op1) { delete op1; op1 = nullptr; }
  if (op2) { delete op2; op2 = nullptr; }
}


//==========================================================================
//
//  VBinary::SyntaxCopy
//
//==========================================================================
VExpression *VBinary::SyntaxCopy () {
  auto res = new VBinary();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VBinary::DoSyntaxCopyTo
//
//==========================================================================
void VBinary::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VBinary *)e;
  res->Oper = Oper;
  res->op1 = (op1 ? op1->SyntaxCopy() : nullptr);
  res->op2 = (op2 ? op2->SyntaxCopy() : nullptr);
  res->FromDecorate = FromDecorate;
}


//==========================================================================
//
//  VBinary::calcPrio
//
//==========================================================================
//FIXME: BUGGY!
int VBinary::calcPrio (EBinOp op) {
  switch (op) {
    case Add:
    case Subtract:
      return 2;
    case Multiply:
    case Divide:
    case Modulus:
      return 3;
    case LShift:
    case RShift:
    case URShift:
      return 4;
    case And:
      return 5;
    case XOr:
      return 6;
    case Or:
      return 7;
    case Equals:
    case NotEquals:
      return 8;
    case Less:
    case LessEquals:
    case Greater:
    case GreaterEquals:
      return 9;
    case StrCat:
      return 2;
    case IsA:
    case NotIsA:
      return 1;
  };
  VCFatalError("VC: internal error (VBinary::calcPrio)");
  return -1;
}


//==========================================================================
//
//  VBinary::needParens
//
//==========================================================================
//FIXME: BUGGY!
bool VBinary::needParens (EBinOp me, EBinOp inner) {
  return (calcPrio(me) < calcPrio(inner));
}


//==========================================================================
//
//  VBinary::DoResolve
//
//==========================================================================
VExpression *VBinary::DoResolve (VEmitContext &ec) {
  if (op1 && op2) {
    // "a == b == c" and such should not compile
    // bitwise ops has lower precedence than comparisons (due to C roots), so warn about it too
    if (IsComparison() || IsBitLogic()) {
      VExpression *xo = nullptr;
           if (op1->IsBinaryMath() && ((VBinary *)op1)->IsComparison()) xo = op1;
      else if (op2->IsBinaryMath() && ((VBinary *)op2)->IsComparison()) xo = op2;
      if (xo) {
        ParseError(Loc, "doing `%s` with `%s` is probably not what you want", ((VBinary *)xo)->getOpName(), getOpName());
        delete this;
        return nullptr;
      }
    }
    // `!a & b` is not what you want
    if (IsBitLogic()) {
      VExpression *xo = nullptr;
           if (op1->IsUnaryMath() && ((VUnary *)op1)->Oper == VUnary::Not) xo = op1;
      else if (op2->IsUnaryMath() && ((VUnary *)op2)->Oper == VUnary::Not) xo = op2;
      if (xo) {
        ParseError(Loc, "doing `%s` with `%s` is probably not what you want", ((VUnary *)xo)->getOpName(), getOpName());
        delete this;
        return nullptr;
      }
    }
  }

  if (op1) op1 = op1->Resolve(ec);
  if (op2) op2 = op2->Resolve(ec);
  if (!op1 || !op2) { delete this; return nullptr; }

  // convert decorate crap (int operations on floats)
  if (FromDecorate) {
    switch (Oper) {
      case Modulus:
        // fp modulus is undefined
        if (op2->Type.Type == TYPE_Float) {
          if (!op2->IsFloatConst()) {
            ParseError(Loc, "floating point modulo is not defined");
            delete this;
            return nullptr;
          }
          // stupid check for valid integer
          const float fv = op2->IsFloatConst();
          const int iv = (int)fv;
          const float f2 = (float)iv;
          if (fv != f2) {
            ParseError(Loc, "floating point modulo is not defined");
            delete this;
            return nullptr;
          }
        }
        /* fallthrough */
      case LShift:
      case RShift:
      case URShift:
      case And:
      case XOr:
      case Or:
        // convert both operands to ints
        if (op1->Type.Type == TYPE_Float) {
          op1 = new VScalarToInt(op1, false);
          if (op1) op1 = op1->Resolve(ec);
          if (!op1) { delete this; return nullptr; }
        }
        if (op2->Type.Type == TYPE_Float) {
          op2 = new VScalarToInt(op2, false);
          if (op2) op2 = op2->Resolve(ec);
          if (!op2) { delete this; return nullptr; }
        }
      default:
        break;
    }
  }

  // if we're doing comparisons, and one operand is int, and second is
  // one-char name or one-char string, convert it to int too, as this
  // is something like `str[1] == "a"`
  // also, allow mixing strings and names in comparisons
  switch (Oper) {
    case Less:
    case LessEquals:
    case Greater:
    case GreaterEquals:
    case Equals:
    case NotEquals:
      // first is int, second is one-char string
      if (op1->Type.Type == TYPE_Int && op2->Type.Type == TYPE_String &&
          op2->IsStrConst() && op2->GetStrConst(ec.Package).length() == 1)
      {
        VStr s = op2->GetStrConst(ec.Package);
        VExpression *e = new VIntLiteral((vuint8)s[0], op2->Loc);
        delete op2;
        op2 = e->Resolve(ec); // will never fail
      }
      else // first is int, second is one-char name
      if (op1->Type.Type == TYPE_Int && op2->Type.Type == TYPE_Name &&
          op2->IsNameConst() && VStr::length(*op2->GetNameConst()) == 1)
      {
        const char *s = *op2->GetNameConst();
        VExpression *e = new VIntLiteral((vuint8)s[0], op2->Loc);
        delete op2;
        op2 = e->Resolve(ec); // will never fail
      }
      else // second is int, first is one-char string
      if (op2->Type.Type == TYPE_Int && op1->Type.Type == TYPE_String &&
          op1->IsStrConst() && op1->GetStrConst(ec.Package).length() == 1)
      {
        VStr s = op1->GetStrConst(ec.Package);
        VExpression *e = new VIntLiteral((vuint8)s[0], op1->Loc);
        delete op1;
        op1 = e->Resolve(ec); // will never fail
      }
      else // second is int, first is one-char name
      if (op2->Type.Type == TYPE_Int && op1->Type.Type == TYPE_Name &&
          op1->IsNameConst() && VStr::length(*op1->GetNameConst()) == 1)
      {
        const char *s = *op1->GetNameConst();
        VExpression *e = new VIntLiteral((vuint8)s[0], op1->Loc);
        delete op1;
        op1 = e->Resolve(ec); // will never fail
      }
      else // first is string, second is name
      if (op1->Type.Type == TYPE_String && op2->Type.Type == TYPE_Name)
      {
        // convert second to string (to avoid creating alot of names)
        VExpression *e = new VCastToString(op2); // it is ok to pass resolved expression here
        op2 = e->Resolve(ec);
        if (!op2) { delete this; return nullptr; } // just in case
      }
      else // first is name, second is string
      if (op1->Type.Type == TYPE_Name && op2->Type.Type == TYPE_String)
      {
        // convert first to string (to avoid creating alot of names)
        VExpression *e = new VCastToString(op1); // it is ok to pass resolved expression here
        op1 = e->Resolve(ec);
        if (!op1) { delete this; return nullptr; } // just in case
      }
      else // both are names, but we are in decorate
      if (FromDecorate && op1->Type.Type == TYPE_Name && op2->Type.Type == TYPE_Name)
      {
        // convert both to strings
        VExpression *e = new VCastToString(op1); // it is ok to pass resolved expression here
        op1 = e->Resolve(ec);
        if (!op1) { delete this; return nullptr; } // just in case
        e = new VCastToString(op2); // it is ok to pass resolved expression here
        op2 = e->Resolve(ec);
        if (!op2) { delete this; return nullptr; } // just in case
      }
      break;
    default:
      break;
  }

  // coerce both types if it is possible
  VExpression::CoerceTypes(ec, op1, op2, false); // don't coerce "none delegate"
  if (!op1 || !op2) { delete this; return nullptr; }

  if (!FromDecorate) {
    // optimise literal string comparisons
    if (op1->Type.Type == TYPE_String && op1->IsStrConst() &&
        op2->Type.Type == TYPE_String && op2->IsStrConst())
    {
      VStr s1 = op1->GetStrConst(ec.Package);
      VStr s2 = op2->GetStrConst(ec.Package);
      VExpression *e = nullptr;
      switch (Oper) {
        case Less: e = new VIntLiteral((s1 < s2 ? 1 : 0), Loc); break;
        case LessEquals: e = new VIntLiteral((s1 <= s2 ? 1 : 0), Loc); break;
        case Greater: e = new VIntLiteral((s1 > s2 ? 1 : 0), Loc); break;
        case GreaterEquals: e = new VIntLiteral((s1 >= s2 ? 1 : 0), Loc); break;
        case Equals: e = new VIntLiteral((s1 == s2 ? 1 : 0), Loc); break;
        case NotEquals: e = new VIntLiteral((s1 != s2 ? 1 : 0), Loc); break;
        default: break;
      }
      if (e) {
        delete this;
        return e->Resolve(ec);
      }
    }
    // optimise literal name comparisons
    if (op1->Type.Type == TYPE_Name && op1->IsNameConst() &&
        op2->Type.Type == TYPE_Name && op2->IsNameConst())
    {
      VName n1 = op1->GetNameConst();
      VName n2 = op2->GetNameConst();
      VExpression *e = nullptr;
      switch (Oper) {
        case Equals: e = new VIntLiteral((n1 == n2 ? 1 : 0), Loc); break;
        case NotEquals: e = new VIntLiteral((n1 != n2 ? 1 : 0), Loc); break;
        default: break;
      }
      if (e) {
        delete this;
        return e->Resolve(ec);
      }
    }
  } else {
    // from decorate, case-insensitive comparisons
    if (op1->Type.Type == TYPE_String && op1->IsStrConst() &&
        op2->Type.Type == TYPE_String && op2->IsStrConst())
    {
      VStr s1 = op1->GetStrConst(ec.Package);
      VStr s2 = op2->GetStrConst(ec.Package);
      VExpression *e = nullptr;
      switch (Oper) {
        case Less: e = new VIntLiteral((s1.ICmp(s2) < 0 ? 1 : 0), Loc); break;
        case LessEquals: e = new VIntLiteral((s1.ICmp(s2) <= 0 ? 1 : 0), Loc); break;
        case Greater: e = new VIntLiteral((s1.ICmp(s2) > 0 ? 1 : 0), Loc); break;
        case GreaterEquals: e = new VIntLiteral((s1.ICmp(s2) >= 0 ? 1 : 0), Loc); break;
        case Equals: e = new VIntLiteral((s1.ICmp(s2) == 0 ? 1 : 0), Loc); break;
        case NotEquals: e = new VIntLiteral((s1.ICmp(s2) != 0 ? 1 : 0), Loc); break;
        default: break;
      }
      if (e) {
        delete this;
        return e->Resolve(ec);
      }
    }
    // rewrite string comparisons to method calls
    if (op1->Type.Type == TYPE_String && op2->Type.Type == TYPE_String) {
      bool validCmp = false;
      switch (Oper) {
        case Less:
        case LessEquals:
        case Greater:
        case GreaterEquals:
        case Equals:
        case NotEquals:
          validCmp = true;
          break;
        default: break;
      }
      if (validCmp) {
        VMethod *mt = VObject::StaticClass()->FindMethodChecked("stricmp");
        vassert(mt);
        if (!mt->IsStatic() || mt->IsVarArgs() || mt->IsStructMethod() || mt->IsIterator()) {
          ParseError(Loc, "invalid `stricmp()` method");
          delete this;
          return nullptr;
        }
        VExpression *args[2] = { op1, op2 }; // it should be safe to pass resolved expression here
        VExpression *estrcmp = new VInvocation(nullptr/*self*/, mt, nullptr/*dgfield*/, false/*selfexpr*/, false/*basecall*/, Loc, 2, args);
        // always with zero
        VExpression *ezero = new VIntLiteral(0, Loc);
        // create proper comparison
        VExpression *ecmp = new VBinary(Oper, estrcmp, ezero, Loc);
        op1 = op2 = nullptr;
        delete this;
        return ecmp->Resolve(ec);
      }
    }
  }

  // determine resulting type (and check operand types)
  switch (Oper) {
    case Add:
    case Subtract:
           if (op1->Type.Type == TYPE_Int && op2->Type.Type == TYPE_Int) Type = TYPE_Int;
      else if (op1->Type.Type == TYPE_Float && op2->Type.Type == TYPE_Float) Type = TYPE_Float;
      else if (op1->Type.Type == TYPE_Vector && op2->Type.Type == TYPE_Vector) Type = TYPE_Vector;
      else if (Oper == Subtract && (op1->Type.IsPointer() && op2->Type.IsPointer())) {
        // we can subtract pointers to get index, 'cause why not?
        // still, check if pointers are of the same type
        if (!op1->Type.GetPointerInnerType().Equals(op2->Type.GetPointerInnerType())) {
          ParseError(Loc, "Pointer type mismatch");
          delete this;
          return nullptr;
        }
        Type = TYPE_Int;
      } else {
        ParseError(Loc, "Expression type mismatch");
        delete this;
        return nullptr;
      }
      break;
    case Multiply:
           if (op1->Type.Type == TYPE_Int && op2->Type.Type == TYPE_Int) Type = TYPE_Int;
      else if (op1->Type.Type == TYPE_Float && op2->Type.Type == TYPE_Float) Type = TYPE_Float;
      else if (op1->Type.Type == TYPE_Vector && op2->Type.Type == TYPE_Float) Type = TYPE_Vector;
      else if (op1->Type.Type == TYPE_Float && op2->Type.Type == TYPE_Vector) Type = TYPE_Vector;
      else {
        ParseError(Loc, "Expression type mismatch");
        delete this;
        return nullptr;
      }
      break;
    case Divide:
           if (op1->Type.Type == TYPE_Int && op2->Type.Type == TYPE_Int) Type = TYPE_Int;
      else if (op1->Type.Type == TYPE_Float && op2->Type.Type == TYPE_Float) Type = TYPE_Float;
      else if (op1->Type.Type == TYPE_Vector && op2->Type.Type == TYPE_Float) Type = TYPE_Vector;
      else {
        ParseError(Loc, "Expression type mismatch");
        delete this;
        return nullptr;
      }
      break;
    case Modulus:
    case LShift:
    case RShift:
    case URShift:
    case And:
    case XOr:
    case Or:
      if (op1->Type.Type == TYPE_Int && op2->Type.Type == TYPE_Int) {
        Type = TYPE_Int;
      } else {
        ParseError(Loc, "Expression type mismatch");
        delete this;
        return nullptr;
      }
      break;
    case Less:
    case LessEquals:
    case Greater:
    case GreaterEquals:
      if (op1->Type.Type != op2->Type.Type ||
          (op1->Type.Type != TYPE_Int && op1->Type.Type != TYPE_Float && op1->Type.Type != TYPE_String))
      {
        ParseError(Loc, "Expression type mismatch");
        delete this;
        return nullptr;
      }
      Type = TYPE_Int;
      break;
    case Equals:
    case NotEquals:
      if (op1->Type.Type != op2->Type.Type ||
          (op1->Type.Type != TYPE_Int &&
           op1->Type.Type != TYPE_Float &&
           op1->Type.Type != TYPE_Name &&
           op1->Type.Type != TYPE_Pointer &&
           op1->Type.Type != TYPE_Vector &&
           op1->Type.Type != TYPE_Class &&
           op1->Type.Type != TYPE_State &&
           op1->Type.Type != TYPE_Reference &&
           op1->Type.Type != TYPE_String))
      {
        ParseError(Loc, "Expression type mismatch");
        delete this;
        return nullptr;
      }
      // still, check of pointers are of the same type
      if (op1->Type.IsPointer() && !op1->Type.IsCompatiblePointerRelaxed(op2->Type)) {
        ParseError(Loc, "%s", va("Pointer type mismatch (`%s` vs `%s`)", *op1->Type.GetName(), *op2->Type.GetName()));
        delete this;
        return nullptr;
      }
      Type = TYPE_Int;
      break;
    case StrCat:
      if (op1->Type.Type != TYPE_String || op2->Type.Type != TYPE_String) {
        ParseError(Loc, "String concatenation expects two strings");
        delete this;
        return nullptr;
      }
      // optimize literals
      if (op1->IsStrConst() && op2->IsStrConst()) {
        VStr s = op1->GetStrConst(ec.Package)+op2->GetStrConst(ec.Package);
        int val = ec.Package->FindString(s);
        VExpression *e = new VStringLiteral(s, val, Loc);
        delete this;
        return e->Resolve(ec);
      }
      Type = TYPE_String;
      break;
    case IsA:
    case NotIsA:
      //FIXME: check if both sides are class literals, and do it in compile time
      if (op1->Type.Type == TYPE_Class && op2->Type.Type == TYPE_Class) {
        Type = TYPE_Int;
      } else if (op1->Type.Type == TYPE_Class || op1->Type.Type == TYPE_Reference) {
        if (op2->Type.Type != TYPE_Class && op2->Type.Type != TYPE_Reference && op2->Type.Type != TYPE_Name) {
          ParseError(Loc, "`isa` expects class/object/name as second operand");
          delete this;
          return nullptr;
        }
        Type = TYPE_Int;
      } else {
        ParseError(Loc, "`isa` expects class or object as first operand");
        delete this;
        return nullptr;
      }
      break;
    default: VCFatalError("VBinary::DoResolve: forgot to handle oper %d in typecheck", (int)Oper);
  }

  // division by zero check
  if ((Oper == Divide || Oper == Modulus) && IsOpZero(op2)) {
    ParseError(Loc, "Division by 0");
    delete this;
    return nullptr;
  }

  // negative shift check
  if ((Oper == LShift || Oper == RShift || Oper == URShift) && IsOpNegative(op2)) {
    ParseError(Loc, "shifting by negative value is undefined");
    delete this;
    return nullptr;
  }

  // optimise shifts and some bitops; note that constant folding will be done later
  if (op2->IsIntConst()) {
    vint32 Value2 = op2->GetIntConst();
    VExpression *e = nullptr;
    // shift by zero means "noop"
    switch (Oper) {
      case LShift:
        if (Value2 == 0) {
          e = op1;
          op1 = nullptr;
        } else if (Value2 > 31) {
          e = new VIntLiteral(0, Loc);
          e = e->Resolve(ec);
        }
        break;
      case RShift:
        if (Value2 == 0) {
          e = op1;
          op1 = nullptr;
        }
        break;
      case URShift:
        if (Value2 == 0) {
          e = op1;
          op1 = nullptr;
        } else if (Value2 > 31) {
          e = new VIntLiteral(0, Loc);
          e = e->Resolve(ec);
        }
        break;
      case And:
        if (Value2 == 0) {
          e = new VIntLiteral(0, Loc);
          e = e->Resolve(ec);
        } else if (Value2 == -1) {
          // all bits set
          e = op1;
          op1 = nullptr;
        }
        break;
      case XOr:
      case Or:
        if (Value2 == 0) {
          e = op1;
          op1 = nullptr;
        }
        break;
      default: break;
    }
    if (e) {
      delete this;
      return e;
    }
  }

  // optimize integer constants
  if (op1->IsIntConst() && op2->IsIntConst()) {
    vint32 Value1 = op1->GetIntConst();
    vint32 Value2 = op2->GetIntConst();
    VExpression *e = nullptr;
    switch (Oper) {
      case Add: e = new VIntLiteral(Value1+Value2, Loc); break;
      case Subtract: e = new VIntLiteral(Value1-Value2, Loc); break;
      case Multiply: e = new VIntLiteral(Value1*Value2, Loc); break;
      case Divide:
        if (!Value2) {
          ParseError(Loc, "Division by 0");
          delete this;
          return nullptr;
        }
        e = new VIntLiteral(Value1/Value2, Loc);
        break;
      case Modulus:
        if (!Value2) {
          ParseError(Loc, "Division by 0");
          delete this;
          return nullptr;
        }
        e = new VIntLiteral(Value1%Value2, Loc);
        break;
      case LShift:
        if (Value2 < 0 || Value2 > 31) {
          ParseError(Loc, "shl by %d", Value2);
          delete this;
          return nullptr;
        }
        e = new VIntLiteral(Value1<<Value2, Loc);
        break;
      case RShift:
        if (Value2 < 0 || Value2 > 31) {
          ParseError(Loc, "shr by %d", Value2);
          delete this;
          return nullptr;
        }
        e = new VIntLiteral(Value1>>Value2, Loc);
        break;
      case URShift:
        if (Value2 < 0 || Value2 > 31) {
          ParseError(Loc, "ushr by %d", Value2);
          delete this;
          return nullptr;
        }
        e = new VIntLiteral((vint32)((vuint32)Value1>>((vuint32)Value2&0x1fu)), Loc);
        break;
      case Less: e = new VIntLiteral(Value1 < Value2, Loc); break;
      case LessEquals: e = new VIntLiteral(Value1 <= Value2, Loc); break;
      case Greater: e = new VIntLiteral(Value1 > Value2, Loc); break;
      case GreaterEquals: e = new VIntLiteral(Value1 >= Value2, Loc); break;
      case Equals: e = new VIntLiteral(Value1 == Value2, Loc); break;
      case NotEquals: e = new VIntLiteral(Value1 != Value2, Loc); break;
      case And: e = new VIntLiteral(Value1&Value2, Loc); break;
      case XOr: e = new VIntLiteral(Value1^Value2, Loc); break;
      case Or: e = new VIntLiteral(Value1|Value2, Loc); break;
      default: break;
    }
    if (e) {
      delete this;
      return e->Resolve(ec);
    }
  }

  // optimize float constants
  if (op1->IsFloatConst() && op2->IsFloatConst()) {
    float Value1 = op1->GetFloatConst();
    float Value2 = op2->GetFloatConst();
    VExpression *e = nullptr;
    switch (Oper) {
      case Add: e = new VFloatLiteral(Value1+Value2, Loc); break;
      case Subtract: e = new VFloatLiteral(Value1-Value2, Loc); break;
      case Multiply: e = new VFloatLiteral(Value1*Value2, Loc); break;
      case Divide:
        if (!Value2) {
          ParseError(Loc, "Division by 0");
          delete this;
          return nullptr;
        }
        if (!isFiniteF(Value2)) {
          ParseError(Loc, "Division by inf/nan");
          delete this;
          return nullptr;
        }
        e = new VFloatLiteral(Value1/Value2, Loc);
        break;
      default: break;
    }
    if (e) { delete this; return e->Resolve(ec); }
  }

  // simplify/optimise some expressions with a known result
  switch (Oper) {
    case Add:
      // optimize 0+n
      if (IsOpZero(op1)) { VExpression *e = op2; op2 = nullptr; delete this; return e; }
      // optimize n+0
      if (IsOpZero(op2)) { VExpression *e = op1; op1 = nullptr; delete this; return e; }
      break;
    case Subtract:
      // optimize 0-n
      if (IsOpZero(op1)) { VExpression *e = new VUnary(VUnary::Minus, op2, Loc, true/*opresolved*/); op2 = nullptr; delete this; return e->Resolve(ec); }
      // optimize n-0
      if (IsOpZero(op2)) { VExpression *e = op1; op1 = nullptr; delete this; return e; }
      break;
    case Multiply:
      // optimize 0*n
      if (IsOpZero(op1)) { VExpression *e = op1; op1 = nullptr; delete this; return e; }
      // optimize n*0
      if (IsOpZero(op2)) { VExpression *e = op2; op2 = nullptr; delete this; return e; }
      // optimize 1*n
      if (IsOpOne(op1)) { VExpression *e = op2; op2 = nullptr; delete this; return e; }
      // optimize n*1
      if (IsOpOne(op2)) { VExpression *e = op1; op1 = nullptr; delete this; return e; }
      // optimize -1*n
      if (IsOpMinusOne(op1)) { VExpression *e = new VUnary(VUnary::Minus, op2, Loc, true/*opresolved*/); op2 = nullptr; delete this; return e->Resolve(ec); }
      // optimize n*(-1)
      if (IsOpMinusOne(op2)) { VExpression *e = new VUnary(VUnary::Minus, op1, Loc, true/*opresolved*/); op1 = nullptr; delete this; return e->Resolve(ec); }
      break;
    case Divide:
      // optimize n/1
      if (IsOpOne(op2)) { VExpression *e = op1; op1 = nullptr; delete this; return e; }
      /*
      // optimize n/2.0 and n/(-2.0)
      if (op2 && op2->IsFloatConst() && (op2->GetFloatConst() == 2.0f || op2->GetFloatConst() == -2.0f)) {
        Oper = Multiply;
        VExpression *ef = new VFloatLiteral((op2->GetFloatConst() == 2.0f ? 0.5f : -0.5f), op2->Loc);
        delete op2;
        op2 = ef;
      }
      */
      break;
    default: break;
  }

  SetResolved();
  return this;
}


//==========================================================================
//
//  VBinary::Emit
//
//==========================================================================
void VBinary::Emit (VEmitContext &ec) {
  if (!op1 || !op2) return;
  EmitCheckResolved(ec);

  // `isa` and `!isa`
  if (Oper == IsA || Oper == NotIsA) {
    if (op1->Type.Type == TYPE_Class && op2->Type.Type == TYPE_Class) {
      //VCFatalError("VC: internal compiler error (VBinary::Emit:Class:Class)");
      op1->Emit(ec);
      op2->Emit(ec);
      ec.AddStatement((Oper == IsA ? OPC_ClassIsAClass : OPC_ClassIsNotAClass), Loc);
    } else if (op1->Type.Type == TYPE_Class || op1->Type.Type == TYPE_Reference) {
      if (op2->Type.Type != TYPE_Name) {
        if (op2->Type.Type != TYPE_Class && op2->Type.Type != TYPE_Reference) {
          VCFatalError("VC: internal compiler error (VBinary::Emit:ClassObj:?)");
        }
      }
      op1->Emit(ec);
      if (op1->Type.Type == TYPE_Reference) ec.AddStatement(OPC_GetObjClassPtr, Loc); // load class
      op2->Emit(ec);
      if (op2->Type.Type == TYPE_Reference) ec.AddStatement(OPC_GetObjClassPtr, Loc); // load class
      if (op2->Type.Type == TYPE_Name) {
        ec.AddStatement((Oper == IsA ? OPC_ClassIsAClassName : OPC_ClassIsNotAClassName), Loc);
      } else {
        ec.AddStatement((Oper == IsA ? OPC_ClassIsAClass : OPC_ClassIsNotAClass), Loc);
      }
      return;
    } else {
      VCFatalError("VC: internal compiler error (VBinary::Emit:?:?)");
    }
    return;
  }

  op1->Emit(ec);
  op2->Emit(ec);

  bool err = false;
  switch (Oper) {
    case Add:
           if (op1->Type.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_Add, Loc);
      else if (op1->Type.Type == TYPE_Float && op2->Type.Type == TYPE_Float) ec.AddStatement(OPC_FAdd, Loc);
      else if (op1->Type.Type == TYPE_Vector && op2->Type.Type == TYPE_Vector) ec.AddStatement(OPC_VAdd, Loc);
      else err = true;
      break;
    case Subtract:
           if (op1->Type.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_Subtract, Loc);
      else if (op1->Type.Type == TYPE_Float && op2->Type.Type == TYPE_Float) ec.AddStatement(OPC_FSubtract, Loc);
      else if (op1->Type.Type == TYPE_Vector && op2->Type.Type == TYPE_Vector) ec.AddStatement(OPC_VSubtract, Loc);
      else if (op1->Type.IsPointer() && op2->Type.IsPointer()) ec.AddStatement(OPC_PtrSubtract, op1->Type.GetPointerInnerType(), Loc);
      else err = true;
      break;
    case Multiply:
           if (op1->Type.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_Multiply, Loc);
      else if (op1->Type.Type == TYPE_Float && op2->Type.Type == TYPE_Float) ec.AddStatement(OPC_FMultiply, Loc);
      else if (op1->Type.Type == TYPE_Vector && op2->Type.Type == TYPE_Float) ec.AddStatement(OPC_VPostScale, Loc);
      else if (op1->Type.Type == TYPE_Float && op2->Type.Type == TYPE_Vector) ec.AddStatement(OPC_VPreScale, Loc);
      else err = true;
      break;
    case Divide:
           if (op1->Type.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_Divide, Loc);
      else if (op1->Type.Type == TYPE_Float && op2->Type.Type == TYPE_Float) ec.AddStatement(OPC_FDivide, Loc);
      else if (op1->Type.Type == TYPE_Vector && op2->Type.Type == TYPE_Float) ec.AddStatement(OPC_VIScale, Loc);
      else err = true;
      break;
    case Modulus:
      if (op1->Type.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_Modulus, Loc);
      else err = true;
      break;
    case LShift:
      if (op1->Type.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_LShift, Loc);
      else err = true;
      break;
    case RShift:
      if (op1->Type.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_RShift, Loc);
      else err = true;
      break;
    case URShift:
      if (op1->Type.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_URShift, Loc);
      else err = true;
      break;
    case Less:
           if (op1->Type.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_Less, Loc);
      else if (op1->Type.Type == TYPE_Float && op2->Type.Type == TYPE_Float) ec.AddStatement(OPC_FLess, Loc);
      else if (op1->Type.Type == TYPE_String && op2->Type.Type == TYPE_String) ec.AddStatement(OPC_StrLess, Loc);
      else err = true;
      break;
    case LessEquals:
           if (op1->Type.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_LessEquals, Loc);
      else if (op1->Type.Type == TYPE_Float && op2->Type.Type == TYPE_Float) ec.AddStatement(OPC_FLessEquals, Loc);
      else if (op1->Type.Type == TYPE_String && op2->Type.Type == TYPE_String) ec.AddStatement(OPC_StrLessEqu, Loc);
      else err = true;
      break;
    case Greater:
           if (op1->Type.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_Greater, Loc);
      else if (op1->Type.Type == TYPE_Float && op2->Type.Type == TYPE_Float) ec.AddStatement(OPC_FGreater, Loc);
      else if (op1->Type.Type == TYPE_String && op2->Type.Type == TYPE_String) ec.AddStatement(OPC_StrGreat, Loc);
      else err = true;
      break;
    case GreaterEquals:
           if (op1->Type.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_GreaterEquals, Loc);
      else if (op1->Type.Type == TYPE_Float && op2->Type.Type == TYPE_Float) ec.AddStatement(OPC_FGreaterEquals, Loc);
      else if (op1->Type.Type == TYPE_String && op2->Type.Type == TYPE_String) ec.AddStatement(OPC_StrGreatEqu, Loc);
      else err = true;
      break;
    case Equals:
           if (op1->Type.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_Equals, Loc);
      else if (op1->Type.Type == TYPE_Float && op2->Type.Type == TYPE_Float) ec.AddStatement(OPC_FEquals, Loc);
      else if (op1->Type.Type == TYPE_Name && op2->Type.Type == TYPE_Name) ec.AddStatement(OPC_Equals, Loc);
      else if (op1->Type.Type == TYPE_Pointer && op2->Type.Type == TYPE_Pointer) ec.AddStatement(OPC_PtrEquals, Loc);
      else if (op1->Type.Type == TYPE_Vector && op2->Type.Type == TYPE_Vector) ec.AddStatement(OPC_VEquals, Loc);
      else if (op1->Type.Type == TYPE_Class && op2->Type.Type == TYPE_Class) ec.AddStatement(OPC_PtrEquals, Loc);
      else if (op1->Type.Type == TYPE_State && op2->Type.Type == TYPE_State) ec.AddStatement(OPC_PtrEquals, Loc);
      else if (op1->Type.Type == TYPE_Reference && op2->Type.Type == TYPE_Reference) ec.AddStatement(OPC_PtrEquals, Loc);
      else if (op1->Type.Type == TYPE_String && op2->Type.Type == TYPE_String) ec.AddStatement(OPC_StrEquals, Loc);
      else err = true;
      break;
    case NotEquals:
           if (op1->Type.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_NotEquals, Loc);
      else if (op1->Type.Type == TYPE_Float && op2->Type.Type == TYPE_Float) ec.AddStatement(OPC_FNotEquals, Loc);
      else if (op1->Type.Type == TYPE_Name && op2->Type.Type == TYPE_Name) ec.AddStatement(OPC_NotEquals, Loc);
      else if (op1->Type.Type == TYPE_Pointer && op2->Type.Type == TYPE_Pointer) ec.AddStatement(OPC_PtrNotEquals, Loc);
      else if (op1->Type.Type == TYPE_Vector && op2->Type.Type == TYPE_Vector) ec.AddStatement(OPC_VNotEquals, Loc);
      else if (op1->Type.Type == TYPE_Class && op2->Type.Type == TYPE_Class) ec.AddStatement(OPC_PtrNotEquals, Loc);
      else if (op1->Type.Type == TYPE_State && op2->Type.Type == TYPE_State) ec.AddStatement(OPC_PtrNotEquals, Loc);
      else if (op1->Type.Type == TYPE_Reference && op2->Type.Type == TYPE_Reference) ec.AddStatement(OPC_PtrNotEquals, Loc);
      else if (op1->Type.Type == TYPE_String && op2->Type.Type == TYPE_String) ec.AddStatement(OPC_StrNotEquals, Loc);
      else err = true;
      break;
    case And:
      if (op1->Type.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_AndBitwise, Loc);
      else err = true;
      break;
    case XOr:
      if (op1->Type.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_XOrBitwise, Loc);
      else err = true;
      break;
    case Or:
      if (op1->Type.Type == TYPE_Int && op2->Type.Type == TYPE_Int) ec.AddStatement(OPC_OrBitwise, Loc);
      else err = true;
      break;
    case StrCat:
      if (op1->Type.Type == TYPE_String && op2->Type.Type == TYPE_String) ec.AddStatement(OPC_StrCat, Loc);
      else err = true;
      break;
    case IsA: case NotIsA: VCFatalError("wtf?!");
    default: VCFatalError("VBinary::Emit: forgot to handle oper %d in code emiter", (int)Oper);
  }
  if (err) VCFatalError("VBinary::Emit(%s): internal compiler error", getOpName());
}


//==========================================================================
//
//  VBinary::IsBinaryMath
//
//==========================================================================
bool VBinary::IsBinaryMath () const {
  return true;
}


//==========================================================================
//
//  VBinary::getOpName
//
//==========================================================================
const char *VBinary::getOpName () const {
  switch (Oper) {
    case Add: return "+";
    case Subtract: return "-";
    case Multiply: return "*";
    case Divide: return "/";
    case Modulus: return "%";
    case LShift: return "<<";
    case RShift: return ">>";
    case URShift: return ">>>";
    case StrCat: return "~";
    case And: return "&";
    case XOr: return "^";
    case Or: return "|";
    case Equals: return "==";
    case NotEquals: return "!=";
    case Less: return "<";
    case LessEquals: return "<=";
    case Greater: return ">";
    case GreaterEquals: return ">=";
    case IsA: return "isa";
    case NotIsA: return "!isa";
  }
  return "<wtf?!>";
}


//==========================================================================
//
//  VBinary::toString
//
//==========================================================================
VStr VBinary::toString () const {
  VStr res;
  if (!op1 || !op1->IsBinaryMath()) {
    res += e2s(op1);
  } else {
    if (needParens(Oper, ((const VBinary *)op1)->Oper)) res += "(";
    res += e2s(op1);
    if (needParens(Oper, ((const VBinary *)op1)->Oper)) res += ")";
  }
  if (Oper >= Equals) res += " ";
  res += getOpName();
  if (Oper >= Equals) res += " ";
  if (!op2 || !op2->IsBinaryMath()) {
    res += e2s(op2);
  } else {
    if (needParens(Oper, ((const VBinary *)op2)->Oper)) res += "(";
    res += e2s(op2);
    if (needParens(Oper, ((const VBinary *)op2)->Oper)) res += ")";
  }
  return res;
}



//==========================================================================
//
//  VBinaryLogical::VBinaryLogical
//
//==========================================================================
VBinaryLogical::VBinaryLogical (ELogOp AOper, VExpression *AOp1, VExpression *AOp2, const TLocation &ALoc)
  : VExpression(ALoc)
  , Oper(AOper)
  , op1(AOp1)
  , op2(AOp2)
{
  if (!op2) ParseError(Loc, "Expression expected");
}


//==========================================================================
//
//  VBinaryLogical::~VBinaryLogical
//
//==========================================================================
VBinaryLogical::~VBinaryLogical () {
  if (op1) { delete op1; op1 = nullptr; }
  if (op2) { delete op2; op2 = nullptr; }
}


//==========================================================================
//
//  VBinaryLogical::SyntaxCopy
//
//==========================================================================
VExpression *VBinaryLogical::SyntaxCopy () {
  auto res = new VBinaryLogical();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VBinaryLogical::DoSyntaxCopyTo
//
//==========================================================================
void VBinaryLogical::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VBinaryLogical *)e;
  res->Oper = Oper;
  res->op1 = (op1 ? op1->SyntaxCopy() : nullptr);
  res->op2 = (op2 ? op2->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VBinaryLogical::IsBinaryLogical
//
//==========================================================================
bool VBinaryLogical::IsBinaryLogical () const {
  return true;
}


//==========================================================================
//
//  VBinaryLogical::DoResolve
//
//==========================================================================
VExpression *VBinaryLogical::DoResolve (VEmitContext &ec) {
  if (op1 && op2) {
    {
      VExpression *xo = nullptr;
      // "a & b && c" is not what you want
           if (op1->IsBinaryMath() && ((VBinary *)op1)->IsBitLogic()) xo = op1;
      else if (op2->IsBinaryMath() && ((VBinary *)op2)->IsBitLogic()) xo = op2;
      if (xo) {
        ParseError(Loc, "doing `%s` with `%s` is probably not what you want", ((VBinary *)xo)->getOpName(), getOpName());
        delete this;
        return nullptr;
      }
    }
    {
      VExpression *xo = nullptr;
      // `a && b || c`, `a || b && c`
           if (op1->IsBinaryLogical() && ((VBinaryLogical *)op1)->Oper != Oper) xo = op1;
      else if (op2->IsBinaryLogical() && ((VBinaryLogical *)op2)->Oper != Oper) xo = op2;
      if (xo) {
        ParseError(Loc, "doing `%s` with `%s` is probably not what you want (precedence)", ((VBinaryLogical *)xo)->getOpName(), getOpName());
        delete this;
        return nullptr;
      }
    }
  }

  //if (op1) op1 = op1->ResolveBoolean(ec);
  //if (op2) op2 = op2->ResolveBoolean(ec);

  // do resolving in two steps:
  // first, resolve to perform some checks
  // second, coerce to bool
  if (op1) op1 = op1->Resolve(ec);
  if (op2) op2 = op2->Resolve(ec);

  if (!op1 || !op2) { delete this; return nullptr; }

  // perform some more checks
  if (op1->IsIntConst() && (op1->GetIntConst() != 0 && op1->GetIntConst() != 1)) {
    ParseError(Loc, "suspicious `%s` (first operand looks strange)", getOpName());
    delete this;
    return nullptr;
  }
  if (op2->IsIntConst() && (op2->GetIntConst() != 0 && op2->GetIntConst() != 1)) {
    ParseError(Loc, "suspicious `%s` (second operand looks strange)", getOpName());
    delete this;
    return nullptr;
  }

  // convert to booleans
  op1 = op1->CoerceToBool(ec);
  op2 = op2->CoerceToBool(ec);
  if (!op1 || !op2) { delete this; return nullptr; }

  Type = TYPE_Int;

  // optimize constant cases
  if (op1->IsIntConst() && op2->IsIntConst()) {
    vint32 Value1 = op1->GetIntConst();
    vint32 Value2 = op2->GetIntConst();
    VExpression *e = nullptr;
    switch (Oper) {
      case And: e = new VIntLiteral((Value1 && Value2 ? 1 : 0), Loc); break;
      case Or: e = new VIntLiteral((Value1 || Value2 ? 1 : 0), Loc); break;
    }
    if (e) {
      delete this;
      return e->Resolve(ec);
    }
  }

  SetResolved();
  return this;
}


//==========================================================================
//
//  VBinaryLogical::Emit
//
//==========================================================================
void VBinaryLogical::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  VLabel Push01 = ec.DefineLabel();
  VLabel End = ec.DefineLabel();

  op1->EmitBranchable(ec, Push01, Oper == Or);

  op2->Emit(ec);
  ec.AddStatement(OPC_Goto, End, Loc);

  ec.MarkLabel(Push01);
  ec.AddStatement((Oper == And ? OPC_PushNumber0 : OPC_PushNumber1), Loc);

  ec.MarkLabel(End);
}


//==========================================================================
//
//  VBinaryLogical::EmitBranchable
//
//==========================================================================
void VBinaryLogical::EmitBranchable (VEmitContext &ec, VLabel Lbl, bool OnTrue) {
  EmitCheckResolved(ec);
  switch (Oper) {
    case And:
      if (OnTrue) {
        VLabel End = ec.DefineLabel();
        op1->EmitBranchable(ec, End, false);
        op2->EmitBranchable(ec, Lbl, true);
        ec.MarkLabel(End);
      } else {
        op1->EmitBranchable(ec, Lbl, false);
        op2->EmitBranchable(ec, Lbl, false);
      }
      break;
    case Or:
      if (OnTrue) {
        op1->EmitBranchable(ec, Lbl, true);
        op2->EmitBranchable(ec, Lbl, true);
      } else {
        VLabel End = ec.DefineLabel();
        op1->EmitBranchable(ec, End, true);
        op2->EmitBranchable(ec, Lbl, false);
        ec.MarkLabel(End);
      }
      break;
  }
}


//==========================================================================
//
//  VBinaryLogical::getOpName
//
//==========================================================================
const char *VBinaryLogical::getOpName () const {
  switch (Oper) {
    case And: return "&&";
    case Or: return "||";
  }
  return "wtf?!";
}


//==========================================================================
//
//  VBinaryLogical::toString
//
//==========================================================================
VStr VBinaryLogical::toString () const {
  VStr res("(");
  res += e2s(op1);
  res += ") ";
  res += getOpName();
  res += " (";
  res += e2s(op2);
  res += ")";
  return res;
}
