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
//  VLiteralBase
//
//==========================================================================
class VLiteralBase : public VExpression {
public:
  VLiteralBase (const TLocation &ALoc);

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

protected:
  VLiteralBase () {}
};


//==========================================================================
//
//  VIntLiteral
//
//==========================================================================
class VIntLiteral : public VLiteralBase {
public:
  vint32 Value;

  VIntLiteral (vint32, const TLocation &);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual bool IsIntConst () const override;
  virtual vint32 GetIntConst () const override;

  virtual VStr toString () const override;

protected:
  VIntLiteral () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VFloatLiteral
//
//==========================================================================
class VFloatLiteral : public VLiteralBase {
public:
  float Value;

  VFloatLiteral (float, const TLocation &);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual bool IsFloatConst () const override;
  virtual float GetFloatConst () const override;

  virtual VStr toString () const override;

protected:
  VFloatLiteral () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VNameLiteral
//
//==========================================================================
class VNameLiteral : public VLiteralBase {
public:
  VName Value;

  VNameLiteral (VName, const TLocation &);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual bool IsNameConst () const override;
  virtual VName GetNameConst () const override;

  virtual VStr toString () const override;

protected:
  VNameLiteral () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VClassNameLiteral
//
//==========================================================================
class VClassNameLiteral : public VNameLiteral {
public:
  VClassNameLiteral (VName, const TLocation &);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual bool IsClassNameConst () const override;

  virtual VStr toString () const override;

protected:
  VClassNameLiteral () {}
};


//==========================================================================
//
//  VStringLiteral
//
//==========================================================================
class VStringLiteral : public VLiteralBase {
public:
  vint32 Value;
  VStr strval;

  VStringLiteral (VStr asval, vint32, const TLocation &);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual bool IsStrConst () const override;
  virtual const VStr &GetStrConst (VPackage *) const override; //WARNING! returns by reference!

  virtual VStr toString () const override;

protected:
  VStringLiteral () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VSelf
//
//==========================================================================
class VSelf : public VLiteralBase {
public:
  VSelf (const TLocation &);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool IsSelfLiteral () const override;

  virtual VStr toString () const override;

protected:
  VSelf () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VSelfClass
//
//==========================================================================
class VSelfClass : public VLiteralBase {
public:
  VSelfClass (const TLocation &);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool IsSelfClassLiteral () const override;

  virtual VStr toString () const override;

protected:
  VSelfClass () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VNoneLiteral
//
//==========================================================================
class VNoneLiteral : public VLiteralBase {
public:
  VNoneLiteral (const TLocation &);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool IsNoneLiteral () const override;

  virtual VStr toString () const override;

protected:
  VNoneLiteral () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VNoneDelegateLiteral
//
//==========================================================================
class VNoneDelegateLiteral : public VLiteralBase {
public:
  VNoneDelegateLiteral (const TLocation &);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool IsNoneDelegateLiteral () const override;

  virtual VStr toString () const override;

protected:
  VNoneDelegateLiteral () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VNullLiteral
//
//==========================================================================
class VNullLiteral : public VLiteralBase {
public:
  VNullLiteral (const TLocation &);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool IsNullLiteral () const override;

  virtual VStr toString () const override;

protected:
  VNullLiteral () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDollar
//
//==========================================================================
class VDollar : public VLiteralBase {
public:
  VDollar (const TLocation &);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VStr toString () const override;

protected:
  VDollar () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};
