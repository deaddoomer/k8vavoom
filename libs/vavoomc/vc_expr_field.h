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
//  VFieldBase
//
//==========================================================================
class VFieldBase : public VExpression {
public:
  VExpression *op;
  VName FieldName;

public:
  VFieldBase (VExpression *AOp, VName AFieldName, const TLocation &ALoc);
  virtual ~VFieldBase () override;

protected:
  VFieldBase () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VPointerField
//
//==========================================================================
class VPointerField : public VFieldBase {
public:
  VPointerField (VExpression *AOp, VName AFieldName, const TLocation &ALoc);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VStr toString () const override;

protected:
  VPointerField () {}

  // return result of this unconditionally
  // this uses or deletes `opcopy`
  VExpression *TryUFCS (VEmitContext &ec, AutoCopy &opcopy, const char *errdatatype, VMemberBase *mb);
};


//==========================================================================
//
//  VDotField
//
//==========================================================================
class VDotField : public VFieldBase {
private:
  enum AssType { Normal, AssTarget, AssValue };

  int builtin; // >0: generate builtin

private:
  // `Prop` must not be null!
  VExpression *DoPropertyResolve (VEmitContext &ec, VProperty *Prop, AssType assType);

public:
  VDotField (VExpression *AOp, VName AFieldName, const TLocation &ALoc);

  VExpression *InternalResolve (VEmitContext &ec, AssType assType);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual VExpression *ResolveAssignmentTarget (VEmitContext &) override;
  virtual VExpression *ResolveAssignmentValue (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual bool IsDotField () const override;

  virtual VStr toString () const override;

protected:
  VDotField () {}
};


//==========================================================================
//
//  VFieldAccess
//
//==========================================================================
class VFieldAccess : public VExpression {
public:
  VExpression *op;
  VField *field;
  bool AddressRequested;

  VFieldAccess (VExpression *AOp, VField *AField, const TLocation &ALoc, int ExtraFlags);
  virtual ~VFieldAccess () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void RequestAddressOf () override;
  virtual void Emit (VEmitContext &) override;

  virtual bool IsFieldAccess () const override;

  virtual VStr toString () const override;

protected:
  VFieldAccess () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};




//==========================================================================
//
//  VVectorDirectFieldAccess
//
//  this accesses "immediate" (i.e. pure stack-pushed) vector field
//  op must be already resolved
//  this is created from `VDotField` resolver
//
//==========================================================================
class VVectorDirectFieldAccess : public VExpression {
public:
  VExpression *op;
  int index;

  VVectorDirectFieldAccess (VExpression *AOp, int AIndex, const TLocation &ALoc);
  virtual ~VVectorDirectFieldAccess () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  //virtual void RequestAddressOf () override;
  virtual void Emit (VEmitContext &) override;

  virtual VStr toString () const override;

protected:
  VVectorDirectFieldAccess () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VVectorSwizzleExpr
//
//  this accesses "immediate" (i.e. pure stack-pushed) vector field
//  op must be already resolved
//  this is created from `VDotField` resolver
//
//==========================================================================
class VVectorSwizzleExpr : public VExpression {
public:
  // returns swizzle or -1
  static int ParseOneSwizzle (const char *&s);
  static int ParseSwizzles (const char *s);

public:
  VExpression *op;
  int index; // actually, three swizzles
  bool direct;

  VVectorSwizzleExpr (VExpression *AOp, int ASwizzle, bool ADirect, const TLocation &ALoc);
  virtual ~VVectorSwizzleExpr () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &ec) override;
  //virtual void RequestAddressOf () override;
  virtual void Emit (VEmitContext &ec) override;

  virtual bool IsSwizzle () const override;

  virtual VStr toString () const override;

  inline int GetSwizzleX () const noexcept { return (index&VCVSE_Mask); }
  inline int GetSwizzleY () const noexcept { return ((index>>VCVSE_Shift)&VCVSE_Mask); }
  inline int GetSwizzleZ () const noexcept { return ((index>>(VCVSE_Shift*2))&VCVSE_Mask); }

  inline bool IsSwizzleIdentity () const noexcept { return (GetSwizzleX() == VCVSE_X && GetSwizzleY() == VCVSE_Y && GetSwizzleZ() == VCVSE_Z); }
  inline bool IsSwizzleIdentityNeg () const noexcept { return (GetSwizzleX() == (VCVSE_X|VCVSE_Negate) && GetSwizzleY() == (VCVSE_Y|VCVSE_Negate) && GetSwizzleZ() == (VCVSE_Z|VCVSE_Negate)); }
  inline bool IsSwizzleXY () const noexcept { return (index == (VCVSE_X|(VCVSE_Y<<VCVSE_Shift))); }

  inline bool IsSwizzleConstant () const noexcept {
    return
      (GetSwizzleX()&VCVSE_ElementMask) <= VCVSE_One &&
      (GetSwizzleY()&VCVSE_ElementMask) <= VCVSE_One &&
      (GetSwizzleZ()&VCVSE_ElementMask) <= VCVSE_One;
  }

  TVec GetSwizzleConstant () const noexcept;

  TVec DoSwizzle (TVec v) const noexcept;

  static VStr SwizzleToStr (int index);

protected:
  // each optimiser returns `true` if something was changed
  // optimisers must be called after resolving `op`

  bool OptimiseSwizzleConsts (VEmitContext &ec);
  bool OptimiseSwizzleSwizzle (VEmitContext &ec);
  bool OptimiseSwizzleCtor (VEmitContext &ec);

protected:
  VVectorSwizzleExpr () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDelegateVal
//
//==========================================================================
class VDelegateVal : public VExpression {
public:
  VExpression *op;
  VMethod *M;

  VDelegateVal (VExpression *AOp, VMethod *AM, const TLocation &ALoc);
  virtual ~VDelegateVal () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VStr toString () const override;

protected:
  VDelegateVal () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};
