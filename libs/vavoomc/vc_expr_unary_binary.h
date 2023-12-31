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

//==========================================================================
//
//  VExprParens
//
//==========================================================================
class VExprParens : public VExpression {
public:
  VExpression *op;

  VExprParens (VExpression *, const TLocation &);
  virtual ~VExprParens () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool IsParens () const override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VExprParens () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VUnary
//
//==========================================================================
class VUnary : public VExpression {
public:
  enum EUnaryOp {
    Plus,
    Minus,
    Not,
    BitInvert,
    TakeAddress,
    TakeAddressForced, // doesn't check for safety
  };

public:
  EUnaryOp Oper;
  VExpression *op;
  bool opresolved;
  // string and name comparisons are case-insensitive in decorate
  bool FromDecorate;

  VUnary (EUnaryOp AOper, VExpression *AOp, const TLocation &ALoc, bool aopresolved=false, bool aFromDecorate=false);
  virtual ~VUnary () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual void EmitBranchable (VEmitContext &, VLabel, bool) override;

  virtual bool IsUnaryMath () const override;

  const char *getOpName () const;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VUnary () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VUnaryMutator
//
//==========================================================================
class VUnaryMutator : public VExpression {
public:
  enum EIncDec {
    PreInc,
    PreDec,
    PostInc,
    PostDec,
    Inc,
    Dec,
  };

public:
  EIncDec Oper;
  VExpression *op;

  VUnaryMutator (EIncDec, VExpression *, const TLocation &);
  virtual ~VUnaryMutator () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual VExpression *AddDropResult () override;

  virtual bool IsUnaryMutator () const override;

  const char *getOpName () const;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VUnaryMutator () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VBinary
//
//==========================================================================
class VBinary : public VExpression {
public:
  enum EBinOp {
    Add,
    Subtract,
    Multiply,
    Divide,
    Modulus,
    LShift,
    RShift,
    URShift,
    StrCat,
    And,
    XOr,
    Or,
    Equals,
    NotEquals,
    Less,
    LessEquals,
    Greater,
    GreaterEquals,
    IsA,
    NotIsA,
  };

public:
  EBinOp Oper;
  VExpression *op1;
  VExpression *op2;
  // string and name comparisons are case-insensitive in decorate
  bool FromDecorate;

  VBinary (EBinOp AOper, VExpression *AOp1, VExpression *AOp2, const TLocation &ALoc, bool aFromDecorate=false);
  virtual ~VBinary () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual bool IsBinaryMath () const override;

  virtual VStr toString () const override;

  static bool needParens (EBinOp me, EBinOp inner);

  const char *getOpName () const;

  inline bool IsComparison () const noexcept { return (Oper >= Equals && Oper <= GreaterEquals); }
  inline bool IsBitLogic () const noexcept { return (Oper >= And && Oper <= Or); }

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

protected:
  VBinary () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;

  static int calcPrio (EBinOp op);

  static inline bool IsOpZero (VExpression *e) { return (e ? (e->IsIntConst() ? (e->GetIntConst() == 0) : e->IsFloatConst() ? (e->GetFloatConst() == 0.0f) : false) : false); }
  static inline bool IsOpOne (VExpression *e) { return (e ? (e->IsIntConst() ? (e->GetIntConst() == 1) : e->IsFloatConst() ? (e->GetFloatConst() == 1.0f) : false) : false); }
  static inline bool IsOpMinusOne (VExpression *e) { return (e ? (e->IsIntConst() ? (e->GetIntConst() == -1) : e->IsFloatConst() ? (e->GetFloatConst() == -1.0f) : false) : false); }
  static inline bool IsOpNegative (VExpression *e) { return (e ? (e->IsIntConst() ? (e->GetIntConst() < 0) : e->IsFloatConst() ? (e->GetFloatConst() < 0.0f) : false) : false); }
  static inline bool IsOpPositive (VExpression *e) { return (e ? (e->IsIntConst() ? (e->GetIntConst() > 0) : e->IsFloatConst() ? (e->GetFloatConst() > 0.0f) : false) : false); }
};


//==========================================================================
//
//  VBinaryLogical
//
//==========================================================================
class VBinaryLogical : public VExpression {
public:
  enum ELogOp {
    And,
    Or,
  };

public:
  ELogOp Oper;
  VExpression *op1;
  VExpression *op2;

  VBinaryLogical (ELogOp, VExpression *, VExpression *, const TLocation &);
  virtual ~VBinaryLogical () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual void EmitBranchable (VEmitContext &, VLabel, bool) override;

  const char *getOpName () const;
  virtual bool IsBinaryLogical () const override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VBinaryLogical () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};
