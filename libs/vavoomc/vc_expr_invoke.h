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
//  VDynCastWithVar
//
//==========================================================================
class VDynCastWithVar : public VExpression {
public:
  VExpression *what;
  VExpression *destclass;

public:
  VDynCastWithVar (VExpression *awhat, VExpression *adestclass, const TLocation &aloc);
  virtual ~VDynCastWithVar () override;
  virtual VExpression *DoResolve (VEmitContext &ec) override;
  virtual void Emit (VEmitContext &ec) override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VExpression *SyntaxCopy () override;
  virtual VStr toString () const override;

protected:
  VDynCastWithVar () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VArgMarshall
//
//==========================================================================
class VArgMarshall : public VExpression {
public:
  VExpression *e;
  bool isRef;
  bool isOut;
  bool marshallOpt;
  VName argName;

public:
  VArgMarshall (VExpression *ae);
  VArgMarshall (VName aname, const TLocation &aloc);
  virtual ~VArgMarshall () override;
  virtual VExpression *DoResolve (VEmitContext &ec) override;
  virtual void Emit (VEmitContext &ec) override;

  virtual VExpression *SyntaxCopy () override;

  virtual bool IsMarshallArg () const override;
  virtual bool IsRefArg () const override;
  virtual bool IsOutArg () const override;
  virtual bool IsOptMarshallArg () const override;
  virtual bool IsDefaultArg () const override;

  virtual bool IsNamedArg () const override;
  virtual VName GetArgName () const override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VArgMarshall () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VInvocationBase
//
//==========================================================================
class VInvocationBase : public VExpression {
public:
  int NumArgs;
  VExpression *Args[VMethod::MAX_PARAMS+1];

  VInvocationBase (int ANumArgs, VExpression **AArgs, const TLocation &ALoc);
  virtual ~VInvocationBase () override;

  virtual bool IsAnyInvocation () const override;

  virtual VMethod *GetVMethod (VEmitContext &ec) = 0;

  virtual bool IsMethodNameChangeable () const = 0;
  virtual VName GetMethodName () const = 0;
  virtual void SetMethodName (VName name) = 0;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

protected:
  VInvocationBase () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;

  // generates "(" and ")" too
  VStr args2str () const;
};


//==========================================================================
//
//  VSuperInvocation
//
//==========================================================================
class VSuperInvocation : public VInvocationBase {
public:
  VName Name;

  VSuperInvocation (VName, int, VExpression **, const TLocation &);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VMethod *GetVMethod (VEmitContext &ec) override;

  virtual bool IsMethodNameChangeable () const override;
  virtual VName GetMethodName () const override;
  virtual void SetMethodName (VName aname) override;

  virtual VStr toString () const override;

protected:
  VSuperInvocation () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VCastOrInvocation
//
//  parser creates this for `id(...)`
//  it will be resolved to actual cast or invocation node
//
//==========================================================================
class VCastOrInvocation : public VInvocationBase {
public:
  VName Name;

  VCastOrInvocation (VName, const TLocation &, int, VExpression **);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual VExpression *ResolveIterator (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VMethod *GetVMethod (VEmitContext &ec) override;

  virtual bool IsMethodNameChangeable () const override;
  virtual VName GetMethodName () const override;
  virtual void SetMethodName (VName aname) override;

  virtual VStr toString () const override;

protected:
  VCastOrInvocation () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDotInvocation
//
//==========================================================================
class VDotInvocation : public VInvocationBase {
public:
  VExpression *SelfExpr;
  VName MethodName;

  VDotInvocation (VExpression *, VName, const TLocation &, int, VExpression **);
  virtual ~VDotInvocation () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual VExpression *ResolveIterator (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VMethod *GetVMethod (VEmitContext &ec) override;

  virtual bool IsMethodNameChangeable () const override;
  virtual VName GetMethodName () const override;
  virtual void SetMethodName (VName aname) override;

  virtual VStr toString () const override;

protected:
  VDotInvocation () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;

  // this is used to avoid some pasta in `DoResolve()`
  // it re-resolves pointer to (*selfCopy), or deletes `selfCopy`, and
  // assigns result to `SelfExpr`
  // it also returns `false` on error (and does `delete this`)
  bool DoReResolvePtr (VEmitContext &ec, VExpression *&selfCopy);
};


//==========================================================================
//
//  VTypeInvocation
//
//  This does properties for typeexprs, like `int.max`
//
//==========================================================================
class VTypeInvocation : public VInvocationBase {
public:
  VExpression *TypeExpr;
  VName MethodName;

  VTypeInvocation (VExpression *aTypeExpr, VName aMethodName, const TLocation &aloc, int argc, VExpression **argv);
  virtual ~VTypeInvocation () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VMethod *GetVMethod (VEmitContext &ec) override;

  virtual bool IsMethodNameChangeable () const override;
  virtual VName GetMethodName () const override;
  virtual void SetMethodName (VName aname) override;

  virtual VStr toString () const override;

protected:
  VTypeInvocation () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VInvocation
//
//==========================================================================
class VInvocation : public VInvocationBase {
private:
  // both set in `DoResolve()`
  int lcidx[VMethod::MAX_PARAMS]; // temp vars for omited `ref`/`out` args (<0: none)
  int optmarshall[VMethod::MAX_PARAMS]; // `!optional` marshal var index (<0: none)

public:
  VExpression *SelfExpr;
  VMethod *Func;
  VField *DelegateField;
  int DelegateLocal;
  bool HaveSelf;
  bool BaseCall;
  VState *CallerState;
  VExpression *DgPtrExpr; // for calling struct's delegate field, for example
  int OverrideBuiltin; // >=0: generate this builtin instead of func builtin
  // filled in resolver
  TArray<VEmitContext::VCompExit> elist;

  VInvocation (VExpression *ASelfExpr, VMethod *AFunc, VField *ADelegateField,
               bool AHaveSelf, bool ABaseCall, const TLocation &ALoc, int ANumArgs,
               VExpression **AArgs);
  VInvocation (VMethod *AFunc, int ADelegateLocal, const TLocation &ALoc, int ANumArgs, VExpression **AArgs);
  VInvocation (VExpression *ASelfExpr, VExpression *ADgPtrExpr, VMethod *AFunc, const TLocation &ALoc, int ANumArgs, VExpression **AArgs);
  virtual ~VInvocation () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VMethod *GetVMethod (VEmitContext &ec) override;

  virtual bool IsLLInvocation () const override;

  void CheckParams (VEmitContext &ec);
  void CheckDecorateParams (VEmitContext &);

  // arguments should not be resolved, but should be resolvable without errors
  static VMethod *FindMethodWithSignature (VEmitContext &ec, VName name, int argc, VExpression **argv, const TLocation *loc=nullptr);
  // arguments should not be resolved, but should be resolvable without errors
  static bool IsGoodMethodParams (VEmitContext &ec, VMethod *m, int argc, VExpression **argv);

  virtual bool IsMethodNameChangeable () const override;
  virtual VName GetMethodName () const override;
  virtual void SetMethodName (VName aname) override;

  virtual VStr toString () const override;

protected:
  VInvocation () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;

  // should be called only for functions with `builtinOpc >= 0`
  VExpression *OptimizeBuiltin (VEmitContext &ec);

  // used by `OptimizeBuiltin`; `types` are `TYPE_xxx`
  bool CheckSimpleConstArgs (const int argc, const int *types, const bool exactArgC=true) const;

  // rebuild arglist if there are named args
  bool RebuildArgs ();
};


//==========================================================================
//
//  VInvokeWrite
//
//==========================================================================
class VInvokeWrite : public VInvocationBase {
public:
  bool isWriteln;

  VInvokeWrite (bool aIsWriteln, const TLocation &ALoc, int ANumArgs, VExpression **AArgs);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VMethod *GetVMethod (VEmitContext &ec) override;

  virtual bool IsMethodNameChangeable () const override;
  virtual VName GetMethodName () const override;
  virtual void SetMethodName (VName aname) override;

  virtual VStr toString () const override;

protected:
  VInvokeWrite () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};
