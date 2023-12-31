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

class VExpression;
class VTypeExpr;
class VInvocation;
class VLocalDecl;


// ////////////////////////////////////////////////////////////////////////// //
class VExprVisitor {
public:
  bool stopIt;
  // if we are currently in local definition, this is >= 0
  int locDefIdx;
  // current local definiotion (if we are inside it)
  VLocalDecl *locDecl;

public:
  inline VExprVisitor () : stopIt(false), locDefIdx(-1), locDecl(nullptr) {}
  virtual ~VExprVisitor ();

  virtual void DoVisit (VExpression *expr) = 0;

  virtual void Visited (VExpression *expr) = 0;
};


class VExprVisitorChildrenFirst : public VExprVisitor {
public:
  virtual void DoVisit (VExpression *expr) override;
};


class VExprVisitorChildrenLast : public VExprVisitor {
public:
  // this can be set in `Visited()`, and will be autoreset
  bool skipChildren;

public:
  virtual void DoVisit (VExpression *expr) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VExpression {
public:
  enum { EXPR_Resolved = 0x40000000u };
public:
  struct AutoCopy {
  protected:
    VExpression *e;

  public:
    VV_DISABLE_COPY(AutoCopy)
    AutoCopy () : e(nullptr) {}
    AutoCopy (VExpression *ae) : e(ae ? ae->SyntaxCopy() : nullptr) {}
    ~AutoCopy () { delete e; e = nullptr; }
    inline bool isEmpty () const { return !!e; }
    // if you're used peeked autocopy, you can reset this one
    inline void reset () { e = nullptr; }
    // use `clear()` to delete `e`
    inline void clear () { delete e; e = nullptr; }
    // will not empty autocopy; USE WITH CARE!
    inline VExpression *peek () { return e; }
    // use `extract()` to extract `e` (and leave autocopy empty)
    inline VExpression *extract () { VExpression *res = e; e = nullptr; return res; }
    VExpression *SyntaxCopy () { return (e ? e->SyntaxCopy() : nullptr); }
    // delete current `e`, and remember `ae->SyntaxCopy()`
    inline void replaceSyntaxCopy (VExpression *ae) {
      if (!ae) { delete e; e = nullptr; return; }
      if (ae != e) {
        delete e;
        e = (ae ? ae->SyntaxCopy() : nullptr);
      } else {
        VCFatalError("VC: internal compiler error (AutoCopy::assignSyntaxCopy)");
      }
    }
    // delete current `e`, and remember `ae` (without syntax-copying)
    inline void replaceNoCopy (VExpression *ae) {
      if (!ae) { delete e; e = nullptr; return; }
      if (ae != e) {
        delete e;
        e = ae;
      } else {
        VCFatalError("VC: internal compiler error (AutoCopy::assignNoCopy)");
      }
    }
  };

public:
  VFieldType Type;
  VFieldType RealType;
  unsigned Flags;
  TLocation Loc;

public:
  static vuint32 TotalMemoryUsed;
  static vuint32 CurrMemoryUsed;
  static vuint32 PeakMemoryUsed;
  static vuint32 TotalMemoryFreed;
  static bool InCompilerCleanup;

protected:
  VExpression () {} // used in SyntaxCopy

public:
  inline bool IsReadOnly () const noexcept { return !!(Flags&FIELD_ReadOnly); }
  inline void SetReadOnly () noexcept { Flags |= FIELD_ReadOnly; }
  inline void ResetReadOnly () noexcept { Flags &= ~FIELD_ReadOnly; }

  inline bool IsResolved () const noexcept { return !!(Flags&EXPR_Resolved); }

protected:
  inline void SetResolved () noexcept { Flags |= EXPR_Resolved; }
  inline void ResetResolved () noexcept { Flags &= ~EXPR_Resolved; }

public:
  VExpression (const TLocation &ALoc);
  virtual ~VExpression ();

  virtual VVA_CHECKRESULT VExpression *SyntaxCopy () = 0; // this should be called on *UNRESOLVED* expression

  virtual VVA_CHECKRESULT VExpression *DoResolve (VEmitContext &ec) = 0; // tho shon't call this twice, neither thrice!

  VVA_CHECKRESULT VExpression *Resolve (VEmitContext &ec); // this will usually just call `DoResolve()`
  VVA_CHECKRESULT VExpression *ResolveBoolean (VEmitContext &ec); // actually, *TO* boolean
  VVA_CHECKRESULT VExpression *CoerceToBool (VEmitContext &ec); // expression *MUST* be already resolved
  VVA_CHECKRESULT VExpression *ResolveFloat (VEmitContext &ec); // actually, *TO* float
  VVA_CHECKRESULT VExpression *CoerceToFloat (VEmitContext &ec, bool implicit); // expression *MUST* be already resolved

  // returns -1 if cannot determine, or 0/1
  VVA_CHECKRESULT int IsBoolLiteral (VEmitContext &ec) const;

  VVA_CHECKRESULT virtual VTypeExpr *ResolveAsType (VEmitContext &ec);
  VVA_CHECKRESULT virtual VExpression *ResolveAssignmentTarget (VEmitContext &ec);
  VVA_CHECKRESULT virtual VExpression *ResolveAssignmentValue (VEmitContext &ec);
  VVA_CHECKRESULT virtual VExpression *ResolveIterator (VEmitContext &ec);
  // this will be called before actual assign resolving
  // return `nullptr` to indicate error, or consume `val` and set `resolved` to `true` if resolved
  // if `nullptr` is returned, both `this` and `val` should be destroyed
  VVA_CHECKRESULT virtual VExpression *ResolveCompleteAssign (VEmitContext &ec, VExpression *val, bool &resolved);

  // this coerces ints to floats, and fixes `none`\`nullptr` type
  static void CoerceTypes (VEmitContext &ec, VExpression *&op1, VExpression *&op2, bool coerceNoneDelegate); // expression *MUST* be already resolved

  virtual void RequestAddressOf ();
  virtual void RequestAddressOfForAssign (); // most of the time this forwards to `RequestAddressOf()`

  void EmitCheckResolved (VEmitContext &ec);
  virtual void Emit (VEmitContext &ec) = 0;
  virtual void EmitBranchable (VEmitContext &ec, VLabel Lbl, bool OnTrue);
  void EmitPushPointedCode (VFieldType type, VEmitContext &ec); // yeah, non-virtual

  // return non-nullptr to drop `VDropResult` node, and use the result directly
  // this is called on resolved node
  virtual VExpression *AddDropResult ();

  // type checks
  virtual bool IsValidTypeExpression () const;
  virtual bool IsIntConst () const;
  virtual bool IsFloatConst () const;
  virtual bool IsStrConst () const;
  virtual bool IsNameConst () const;
  virtual bool IsClassNameConst () const;
  virtual vint32 GetIntConst () const;
  virtual float GetFloatConst () const;
  virtual const VStr &GetStrConst (VPackage *) const; //WARNING! returns by reference!
  virtual VName GetNameConst () const;
  virtual bool IsSelfLiteral () const;
  virtual bool IsSelfClassLiteral () const;
  virtual bool IsNoneLiteral () const;
  virtual bool IsNoneDelegateLiteral () const;
  virtual bool IsNullLiteral () const;
  virtual bool IsDefaultObject () const;
  virtual bool IsPropertyAssign () const;
  virtual bool IsDynArraySetNum () const;
  virtual bool IsDecorateSingleName () const;
  virtual bool IsDecorateUserVar () const;
  virtual bool IsLocalVarDecl () const;
  virtual bool IsLocalVarExpr () const;
  virtual bool IsAssignExpr () const;
  virtual bool IsParens () const;
  virtual bool IsUnaryMath () const;
  virtual bool IsUnaryMutator () const;
  virtual bool IsBinaryMath () const;
  virtual bool IsBinaryLogical () const;
  virtual bool IsTernary () const;
  virtual bool IsSingleName () const;
  virtual bool IsDoubleName () const;
  virtual bool IsDotField () const;
  virtual bool IsFieldAccess () const;
  virtual bool IsMarshallArg () const;
  virtual bool IsRefArg () const;
  virtual bool IsOutArg () const;
  virtual bool IsOptMarshallArg () const;
  virtual bool IsDefaultArg () const;
  virtual bool IsNamedArg () const;
  virtual VName GetArgName () const;
  virtual bool IsAnyInvocation () const;
  virtual bool IsLLInvocation () const; // is this `VInvocation`?
  virtual bool IsTypeExpr () const;
  virtual bool IsAutoTypeExpr () const;
  virtual bool IsSimpleType () const;
  virtual bool IsReferenceType () const;
  virtual bool IsClassType () const;
  virtual bool IsPointerType () const;
  virtual bool IsAnyArrayType () const;
  virtual bool IsDictType () const;
  virtual bool IsStaticArrayType () const;
  virtual bool IsDynamicArrayType () const;
  virtual bool IsSliceType () const;
  virtual bool IsDelegateType () const;
  virtual bool IsVectorCtor () const;
  virtual bool IsConstVectorCtor () const;
  virtual bool IsComma () const;
  virtual bool IsCommaRetOp0 () const;
  virtual bool IsDropResult () const;
  virtual bool IsSwizzle () const;

  virtual void Visit (VExprVisitor *v);
  virtual void VisitChildren (VExprVisitor *v) = 0;

  // note that assign itself has side effect
  // WARNING! this should be called only on resolved expressions!
  virtual bool HasSideEffects () = 0;

  virtual VStr toString () const = 0;

  // this may return empty string, or "{type}"
  VStr GetMyTypeName () const;

  static inline VStr e2s (const VExpression *e) { return (e ? e->toString() : "<{null}>"); }

  // this will try to coerce some decorate argument to something sensible
  // used inly in the k8vavoom engine; define as fatal error for other users
  // `CallerState` can be `nullptr`
  // `argnum` is 1-based!
  // `invokation` can be `nullptr` for custom calls (and `CallerState` too)
  VVA_CHECKRESULT
  VExpression *MassageDecorateArg (VEmitContext &ec, VInvocation *invokation, VState *CallerState, const char *funcName,
                                   int argnum, const VFieldType &destType, bool isOptional, const TLocation *aloc=nullptr,
                                   bool *massaged=nullptr);

  // this resolves one-char strings and names to int literals too
  VVA_CHECKRESULT
  VExpression *ResolveToIntLiteralEx (VEmitContext &ec, bool allowFloatTrunc=false);

  static void *operator new (size_t size);
  static void *operator new[] (size_t size);
  static void operator delete (void *p);
  static void operator delete[] (void *p);

  static void ReportLeaks (bool detailed=false);

protected:
  // `e` should be of correct type
  virtual void DoSyntaxCopyTo (VExpression *e);
};
