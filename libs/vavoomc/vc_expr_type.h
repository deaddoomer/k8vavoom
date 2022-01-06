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
//  VTypeExpr
//
//==========================================================================
class VTypeExpr : public VExpression {
public:
  // also, key type for dictionaries
  VExpression *Expr;
  VName MetaClassName;
  bool bResolved;

  VTypeExpr (VFieldType atype, const TLocation &aloc);
  virtual ~VTypeExpr () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  VStr GetName () const;

  virtual bool IsTypeExpr () const override;

  static VTypeExpr *NewTypeExpr (VFieldType atype, const TLocation &aloc);
  // this one resolves `TYPE_Vector` to `TVec` struct
  static VTypeExpr *NewTypeExprFromAuto (VFieldType atype, const TLocation &aloc);

  virtual VStr toString () const override;

protected:
  VTypeExpr () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VTypeExprSimple
//
//  Not class, not array, not delegate, not pointer
//
//==========================================================================
class VTypeExprSimple : public VTypeExpr {
public:
  VTypeExprSimple (EType atype, const TLocation &aloc);
  VTypeExprSimple (VFieldType atype, const TLocation &aloc);
  virtual VExpression *SyntaxCopy () override;
  virtual VTypeExpr *ResolveAsType (VEmitContext &) override;

  virtual bool IsAutoTypeExpr () const override;
  virtual bool IsSimpleType () const override;

protected:
  VTypeExprSimple () {}
};


//==========================================================================
//
//  VTypeExprClass
//
//==========================================================================
class VTypeExprClass : public VTypeExpr {
public:
  VTypeExprClass (VName AMetaClassName, const TLocation &aloc);
  virtual VExpression *SyntaxCopy () override;
  virtual VTypeExpr *ResolveAsType (VEmitContext &) override;
  //virtual void Emit (VEmitContext &) override;

  virtual bool IsClassType () const override;

protected:
  VTypeExprClass () {}
};


//==========================================================================
//
//  VPointerType
//
//==========================================================================
class VPointerType : public VTypeExpr {
public:
  VPointerType (VExpression *AExpr, const TLocation &ALoc);
  virtual VExpression *SyntaxCopy () override;
  virtual VTypeExpr *ResolveAsType (VEmitContext &) override;

  virtual bool IsPointerType () const override;

protected:
  VPointerType () {}
};


//==========================================================================
//
//  VFixedArrayType
//
//==========================================================================
class VFixedArrayType : public VTypeExpr {
public:
  VExpression *SizeExpr;
  VExpression *SizeExpr2;

  VFixedArrayType (VExpression *AExpr, VExpression *ASizeExpr, VExpression *ASizeExpr2, const TLocation &ALoc);
  virtual ~VFixedArrayType () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VTypeExpr *ResolveAsType (VEmitContext &) override;

  virtual bool IsAnyArrayType () const override;
  virtual bool IsStaticArrayType () const override;

protected:
  VFixedArrayType () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDynamicArrayType
//
//==========================================================================
class VDynamicArrayType : public VTypeExpr {
public:
  VDynamicArrayType (VExpression *AExpr, const TLocation &ALoc);
  virtual VExpression *SyntaxCopy () override;
  virtual VTypeExpr *ResolveAsType (VEmitContext &) override;

  virtual bool IsAnyArrayType () const override;
  virtual bool IsDynamicArrayType () const override;

protected:
  VDynamicArrayType () {}
};


//==========================================================================
//
//  VSliceType
//
//==========================================================================
class VSliceType : public VTypeExpr {
public:
  VSliceType (VExpression *AExpr, const TLocation &ALoc);
  virtual VExpression *SyntaxCopy () override;
  virtual VTypeExpr *ResolveAsType (VEmitContext &) override;

  virtual bool IsAnyArrayType () const override;
  virtual bool IsSliceType () const override;

protected:
  VSliceType () {}
};


//==========================================================================
//
//  VDelegateType
//
//==========================================================================
class VDelegateType : public VTypeExpr {
public:
  vint32 Flags; // FUNC_XXX
  vint32 NumParams;
  VMethodParam Params[VMethod::MAX_PARAMS];
  vuint8 ParamFlags[VMethod::MAX_PARAMS];

  // aexpr: return type
  VDelegateType (VExpression *aexpr, const TLocation &aloc);
  VDelegateType (VFieldType atype, const TLocation &aloc);
  virtual ~VDelegateType () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VTypeExpr *ResolveAsType (VEmitContext &ec) override;

  virtual bool IsDelegateType () const override;

  VMethod *CreateDelegateMethod (VMemberBase *aowner);

protected:
  VDelegateType () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDictType
//
//==========================================================================
class VDictType : public VTypeExpr {
public:
  VExpression *VExpr; // value type

public:
  VDictType (VExpression *AKExpr, VExpression *AVExpr, const TLocation &ALoc);
  virtual ~VDictType () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VTypeExpr *ResolveAsType (VEmitContext &) override;

  virtual bool IsDictType () const override;

protected:
  VDictType () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};
