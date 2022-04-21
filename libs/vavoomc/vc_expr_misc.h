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
//**  Copyright (C) 2018-2022 Ketmar Dark
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
//  VVectorExpr
//
//==========================================================================
class VVectorExpr : public VExpression {
public:
  VExpression *op1;
  VExpression *op2;
  VExpression *op3;

  VVectorExpr (VExpression *AOp1, VExpression *AOp2, VExpression *AOp3, const TLocation &ALoc);
  explicit VVectorExpr (const TVec &vv, const TLocation &aloc);
  virtual ~VVectorExpr () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool IsVectorCtor () const override;
  virtual bool IsConstVectorCtor () const override;

  // is this a const ctor? (should be called after resolving)
  bool IsConst () const;
  TVec GetConstValue () const;

  inline VExpression *getOp (int idx) noexcept { return (idx == 0 ? op1 : idx == 1 ? op2 : idx == 2 ? op3 : nullptr); }
  inline void setOp (int idx, VExpression *e) noexcept { vassert(idx >= 0 && idx <= 2); if (idx == 0) op1 = e; else if (idx == 1) op2 = e; else op3 = e; }

  virtual VStr toString () const override;

protected:
  VVectorExpr () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VSingleName
//
//==========================================================================
class VSingleName : public VExpression {
private:
  enum AssType { Normal, AssTarget, AssValue };

public:
  VName Name;

  VSingleName (VName, const TLocation &);
  virtual VExpression *SyntaxCopy () override;
  VExpression *InternalResolve (VEmitContext &ec, AssType assType);
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual VExpression *ResolveAssignmentTarget (VEmitContext &) override;
  virtual VExpression *ResolveAssignmentValue (VEmitContext &) override;
  virtual VTypeExpr *ResolveAsType (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual bool IsValidTypeExpression () const override;
  virtual bool IsSingleName () const override;

  virtual VStr toString () const override;

protected:
  VSingleName () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDoubleName
//
//  `a::b`
//
//==========================================================================
class VDoubleName : public VExpression {
public:
  VName Name1;
  VName Name2;

  VDoubleName (VName, VName, const TLocation &);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual VTypeExpr *ResolveAsType (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual bool IsValidTypeExpression () const override;
  virtual bool IsDoubleName () const override;

  virtual VStr toString () const override;

protected:
  VDoubleName () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDefaultObject
//
//==========================================================================
class VDefaultObject : public VExpression {
public:
  VExpression *op;

  VDefaultObject (VExpression *, const TLocation &);
  virtual ~VDefaultObject () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual bool IsDefaultObject () const override;

  virtual VStr toString () const override;

protected:
  VDefaultObject () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VPushPointed
//
//==========================================================================
class VPushPointed : public VExpression {
public:
  VExpression *op;
  bool AddressRequested;

  VPushPointed (VExpression *, const TLocation &);
  virtual ~VPushPointed () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void RequestAddressOf () override;
  virtual void Emit (VEmitContext &) override;

  virtual VStr toString () const override;

protected:
  VPushPointed () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VConditional
//
//==========================================================================
class VConditional : public VExpression {
public:
  VExpression *op;
  VExpression *op1;
  VExpression *op2;

  VConditional (VExpression *, VExpression *, VExpression *, const TLocation &);
  virtual ~VConditional () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool IsTernary () const override;

  virtual VStr toString () const override;

protected:
  VConditional () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VCommaExprRetOp0
//
//  this executes op0, then op1, and drops result of op1
//
//  not used by the main compiler, but used by DECORATE codegen
//  in DECORATE, it is used to implement prefix `++` and `--`
//
//==========================================================================
class VCommaExprRetOp0 : public VExpression {
public:
  VExpression *op0; // before comma
  VExpression *op1; // after comma

  VCommaExprRetOp0 (VExpression *abefore, VExpression *aafter, const TLocation &aloc);
  virtual ~VCommaExprRetOp0 () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool IsComma () const override;
  virtual bool IsCommaRetOp0 () const override;

  virtual VStr toString () const override;

protected:
  VCommaExprRetOp0 () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDropResult
//
//==========================================================================
class VDropResult : public VExpression {
public:
  VExpression *op;

  VDropResult (VExpression *);
  virtual ~VDropResult () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool IsDropResult () const override;

  virtual VStr toString () const override;

protected:
  VDropResult () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VClassConstant
//
//==========================================================================
class VClassConstant : public VExpression {
public:
  VClass *Class;

  VClassConstant (VClass *AClass, const TLocation &ALoc);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VStr toString () const override;

protected:
  VClassConstant () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VStateConstant
//
//==========================================================================
class VStateConstant : public VExpression {
public:
  VState *State;

  VStateConstant (VState *AState, const TLocation &ALoc);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VStr toString () const override;

protected:
  VStateConstant () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VConstantValue
//
//==========================================================================
class VConstantValue : public VExpression {
public:
  VConstant *Const;

  VConstantValue (VConstant *AConst, const TLocation &ALoc);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual bool IsIntConst () const override;
  virtual bool IsFloatConst () const override;
  virtual vint32 GetIntConst () const override;
  virtual float GetFloatConst () const override;

  virtual VStr toString () const override;

protected:
  VConstantValue () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDecorateSingleName
//
//==========================================================================
class VDecorateSingleName : public VExpression {
public:
  VStr Name;
  //HACK: set to `true` if this is codeblock local access
  bool localAccess;

  VDecorateSingleName (VStr AName, const TLocation &ALoc);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual bool IsDecorateSingleName () const override;

  virtual VStr toString () const override;

protected:
  VDecorateSingleName ();
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VObjectPropGetIsDestroyed
//
//==========================================================================
class VObjectPropGetIsDestroyed : public VExpression {
public:
  VExpression *ObjExpr;

  // `AObjExpr` should NOT be already resolved
  VObjectPropGetIsDestroyed (VExpression *AObjExpr, const TLocation &ALoc);
  virtual ~VObjectPropGetIsDestroyed () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VStr toString () const override;

protected:
  VObjectPropGetIsDestroyed () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};
