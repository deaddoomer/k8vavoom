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

class VTypeExpr;


// ////////////////////////////////////////////////////////////////////////// //
class VExpression {
public:
  VFieldType Type;
  VFieldType RealType;
  int Flags;
  TLocation Loc;
  static vuint32 TotalMemoryUsed;

protected:
  VExpression () {} // used in SyntaxCopy

public:
  VExpression (const TLocation &ALoc);
  virtual ~VExpression ();
  virtual VExpression *SyntaxCopy () = 0; // this should be called on *UNRESOLVED* expression
  virtual VExpression *DoResolve (VEmitContext &ec) = 0; // tho shon't call this twice, neither thrice!
  VExpression *Resolve (VEmitContext &ec); // this will usually just call `DoResolve()`
  VExpression *ResolveBoolean (VEmitContext &ec); // actually, *TO* boolean
  VExpression *ResolveFloat (VEmitContext &ec); // actually, *TO* float
  VExpression *CoerceToFloat (); // expression *MUST* be already resolved
  virtual VTypeExpr *ResolveAsType (VEmitContext &ec);
  virtual VExpression *ResolveAssignmentTarget (VEmitContext &ec);
  virtual VExpression *ResolveAssignmentValue (VEmitContext &ec);
  virtual VExpression *ResolveIterator (VEmitContext &ec);
  virtual void RequestAddressOf ();
  virtual void Emit (VEmitContext &ec) = 0;
  virtual void EmitBranchable (VEmitContext &ec, VLabel Lbl, bool OnTrue);
  void EmitPushPointedCode (VFieldType type, VEmitContext &ec); // yeah, non-virtual
  virtual bool IsValidTypeExpression () const;
  virtual bool IsIntConst () const;
  virtual bool IsFloatConst () const;
  virtual bool IsStrConst () const;
  virtual bool IsNameConst () const;
  virtual vint32 GetIntConst () const;
  virtual float GetFloatConst () const;
  virtual VStr GetStrConst (VPackage *) const;
  virtual VName GetNameConst () const;
  virtual bool IsDefaultObject () const;
  virtual bool IsPropertyAssign () const;
  virtual bool IsDynArraySetNum () const;
  virtual bool AddDropResult ();
  virtual bool IsDecorateSingleName () const;
  virtual bool IsLocalVarDecl () const;
  virtual bool IsLocalVarExpr () const;
  virtual bool IsAssignExpr () const;
  virtual bool IsBinaryMath () const;

  void *operator new (size_t size);
  void *operator new[] (size_t size);
  void operator delete (void *p);
  void operator delete[] (void *p);

protected:
  // `e` should be of correct type
  virtual void DoSyntaxCopyTo (VExpression *e);
};
