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
//  VArrayElement
//
//==========================================================================
class VArrayElement : public VExpression {
protected:
  AutoCopy opscopy;
  bool genStringAssign;
  VExpression *sval;
  bool resolvingInd2; // for opDollar

public:
  VExpression *op;
  VExpression *ind;
  VExpression *ind2; // for 2d access; null for 1d
  bool AddressRequested;
  bool IsAssign;
  bool skipBoundsChecking; // in range foreach, we can skip this
  bool AllowPointerIndexing;

  VArrayElement (VExpression *AOp, VExpression *AInd, const TLocation &ALoc, bool aSkipBounds=false);
  VArrayElement (VExpression *AOp, VExpression *AInd, VExpression *AInd2,
                 const TLocation &ALoc, bool aSkipBounds=false);
  virtual ~VArrayElement () override;
  VExpression *InternalResolve (VEmitContext &ec, bool assTarget);
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual VExpression *ResolveAssignmentTarget (VEmitContext &) override;
  virtual VExpression *ResolveCompleteAssign (VEmitContext &ec, VExpression *val, bool &resolved) override;
  virtual void RequestAddressOf () override;
  virtual void Emit (VEmitContext &) override;

  inline bool Is2D () const { return (ind2 != nullptr); }

  VExpression *GetOpSyntaxCopy ();

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VArrayElement () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;

  friend class VDollar; // to access opcopy
};


//==========================================================================
//
//  VSliceOp
//
//==========================================================================
class VSliceOp : public VArrayElement {
public:
  VExpression *hi;

  VSliceOp (VExpression *aop, VExpression *alo, VExpression *ahi, const TLocation &aloc);
  virtual ~VSliceOp () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &ec) override;
  virtual VExpression *ResolveAssignmentTarget (VEmitContext &ec) override;
  virtual VExpression *ResolveCompleteAssign (VEmitContext &ec, VExpression *val, bool &resolved) override;
  virtual void RequestAddressOf () override;
  virtual void Emit (VEmitContext &ec) override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VSliceOp () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDynArrayGetNum
//
//==========================================================================
class VDynArrayGetNum : public VExpression {
public:
  VExpression *ArrayExpr; // should be already resolved
  int dimNumber; // 0: total size

  VDynArrayGetNum (VExpression *AArrayExpr, int aDimNumber, const TLocation &ALoc);
  virtual ~VDynArrayGetNum () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VDynArrayGetNum () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDynArraySetNum
//
//==========================================================================
class VDynArraySetNum : public VExpression {
public:
  VExpression *ArrayExpr; // should be already resolved
  VExpression *NumExpr;
  VExpression *NumExpr2;
  int opsign; // <0: -=; >0: +=; 0: =; fixed in assign expression resolving
  bool asSetSize;

  VDynArraySetNum (VExpression *AArrayExpr, VExpression *ANumExpr, VExpression *ANumExpr2, const TLocation &ALoc);
  virtual ~VDynArraySetNum () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;
  virtual bool IsDynArraySetNum () const override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VDynArraySetNum () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDynArrayInsert
//
//==========================================================================
class VDynArrayInsert : public VExpression {
public:
  VExpression *ArrayExpr; // should be already resolved
  VExpression *IndexExpr;
  VExpression *CountExpr;

  VDynArrayInsert (VExpression *, VExpression *, VExpression *, const TLocation &);
  virtual ~VDynArrayInsert () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VDynArrayInsert () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDynArrayRemove
//
//==========================================================================
class VDynArrayRemove : public VExpression {
public:
  VExpression *ArrayExpr; // should be already resolved
  VExpression *IndexExpr;
  VExpression *CountExpr;

  VDynArrayRemove (VExpression *, VExpression *, VExpression *, const TLocation &);
  virtual ~VDynArrayRemove () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VDynArrayRemove () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDynArrayClear
//
//==========================================================================
class VDynArrayClear : public VExpression {
public:
  VExpression *ArrayExpr; // should be already resolved

  VDynArrayClear (VExpression *, const TLocation &);
  virtual ~VDynArrayClear () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VDynArrayClear () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDynArraySort
//
//==========================================================================
class VDynArraySort : public VExpression {
public:
  VExpression *ArrayExpr; // should be already resolved
  VExpression *DgExpr;

  VDynArraySort (VExpression *, VExpression *, const TLocation &);
  virtual ~VDynArraySort () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VDynArraySort () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;

private:
  bool checkDelegateType (VMethod *dg);
};


//==========================================================================
//
//  VDynArraySwap1D
//
//==========================================================================
class VDynArraySwap1D : public VExpression {
public:
  VExpression *ArrayExpr; // should be already resolved
  VExpression *Index0Expr;
  VExpression *Index1Expr;

  VDynArraySwap1D (VExpression *, VExpression *, VExpression *, const TLocation &);
  virtual ~VDynArraySwap1D () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VDynArraySwap1D () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDynArrayCopyFrom
//
//==========================================================================
class VDynArrayCopyFrom : public VExpression {
public:
  VExpression *ArrayExpr; // should be already resolved
  VExpression *SrcExpr;

  VDynArrayCopyFrom (VExpression *aarr, VExpression *asrc, const TLocation &aloc);
  virtual ~VDynArrayCopyFrom () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VDynArrayCopyFrom () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDynArrayAllocElement
//
//==========================================================================
class VDynArrayAllocElement : public VExpression {
public:
  VExpression *ArrayExpr; // should be already resolved

  VDynArrayAllocElement (VExpression *aarr, const TLocation &aloc);
  virtual ~VDynArrayAllocElement () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VDynArrayAllocElement () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VStringGetLength
//
//==========================================================================
class VStringGetLength : public VExpression {
public:
  VExpression *StrExpr;

  // `AStrExpr` should be already resolved
  VStringGetLength (VExpression *AStrExpr, const TLocation &ALoc);
  virtual ~VStringGetLength () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VStringGetLength () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VSliceGetLength
//
//==========================================================================
class VSliceGetLength : public VExpression {
public:
  VExpression *sexpr;

  // `asexpr` should be already resolved
  VSliceGetLength (VExpression *asexpr, const TLocation &aloc);
  virtual ~VSliceGetLength () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VSliceGetLength () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VSliceGetPtr
//
//==========================================================================
class VSliceGetPtr : public VExpression {
public:
  VExpression *sexpr;

  // `asexpr` should be already resolved
  VSliceGetPtr (VExpression *asexpr, const TLocation &aloc);
  virtual ~VSliceGetPtr () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VSliceGetPtr () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDictGetLength
//
//==========================================================================
class VDictGetLength : public VExpression {
public:
  VExpression *sexpr;

  // `asexpr` should be already resolved
  VDictGetLength (VExpression *asexpr, const TLocation &aloc);
  virtual ~VDictGetLength () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VDictGetLength () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDictGetCapacity
//
//==========================================================================
class VDictGetCapacity : public VExpression {
public:
  VExpression *sexpr;

  // `asexpr` should be already resolved
  VDictGetCapacity (VExpression *asexpr, const TLocation &aloc);
  virtual ~VDictGetCapacity () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VDictGetCapacity () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDictClearOrReset
//
//==========================================================================
class VDictClearOrReset : public VExpression {
public:
  VExpression *sexpr;
  bool doClear;

  // `asexpr` should be already resolved
  VDictClearOrReset (VExpression *asexpr, bool aClear, const TLocation &aloc);
  virtual ~VDictClearOrReset () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VDictClearOrReset () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDictRehash
//
//==========================================================================
class VDictRehash : public VExpression {
public:
  VExpression *sexpr;
  bool doCompact;

  // `asexpr` should be already resolved
  VDictRehash (VExpression *asexpr, bool aCompact, const TLocation &aloc);
  virtual ~VDictRehash () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VDictRehash () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDictFind
//
//==========================================================================
class VDictFind : public VExpression {
public:
  VExpression *sexpr;
  VExpression *keyexpr;

  // `asexpr` should be already resolved
  VDictFind (VExpression *asexpr, VExpression *akeyexpr, const TLocation &aloc);
  virtual ~VDictFind () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VDictFind () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDictDelete
//
//==========================================================================
class VDictDelete : public VExpression {
public:
  VExpression *sexpr;
  VExpression *keyexpr;

  // `asexpr` should be already resolved
  VDictDelete (VExpression *asexpr, VExpression *akeyexpr, const TLocation &aloc);
  virtual ~VDictDelete () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VDictDelete () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDictPut
//
//==========================================================================
class VDictPut : public VExpression {
public:
  VExpression *sexpr;
  VExpression *keyexpr;
  VExpression *valexpr;

  // `asexpr` should be already resolved
  VDictPut (VExpression *asexpr, VExpression *akeyexpr, VExpression *avalexpr, const TLocation &aloc);
  virtual ~VDictPut () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VDictPut () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDictFirstIndex
//
//==========================================================================
class VDictFirstIndex : public VExpression {
public:
  VExpression *sexpr;

  // `asexpr` should be already resolved
  VDictFirstIndex (VExpression *asexpr, const TLocation &aloc);
  virtual ~VDictFirstIndex () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VDictFirstIndex () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDictIsValidIndex
//
//==========================================================================
class VDictIsValidIndex : public VExpression {
public:
  VExpression *sexpr;
  VExpression *idxexpr;

  // `asexpr` should be already resolved
  VDictIsValidIndex (VExpression *asexpr, VExpression *aidxexpr, const TLocation &aloc);
  virtual ~VDictIsValidIndex () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VDictIsValidIndex () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDictNextIndex
//
//==========================================================================
class VDictNextIndex : public VExpression {
public:
  VExpression *sexpr;
  VExpression *idxexpr;
  bool doRemove;

  // `asexpr` should be already resolved
  VDictNextIndex (VExpression *asexpr, VExpression *aidxexpr, bool aRemove, const TLocation &aloc);
  virtual ~VDictNextIndex () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VDictNextIndex () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDictKeyAtIndex
//
//==========================================================================
class VDictKeyAtIndex : public VExpression {
public:
  VExpression *sexpr;
  VExpression *idxexpr;

  // `asexpr` should be already resolved
  VDictKeyAtIndex (VExpression *asexpr, VExpression *aidxexpr, const TLocation &aloc);
  virtual ~VDictKeyAtIndex () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VDictKeyAtIndex () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDictValueAtIndex
//
//==========================================================================
class VDictValueAtIndex : public VExpression {
public:
  VExpression *sexpr;
  VExpression *idxexpr;

  // `asexpr` should be already resolved
  VDictValueAtIndex (VExpression *asexpr, VExpression *aidxexpr, const TLocation &aloc);
  virtual ~VDictValueAtIndex () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VDictValueAtIndex () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VStructZero
//
//==========================================================================
class VStructZero : public VExpression {
public:
  VExpression *sexpr;

  // `asexpr` should be already resolved
  VStructZero (VExpression *asexpr, const TLocation &aloc);
  virtual ~VStructZero () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual bool HasSideEffects () override;
  virtual void VisitChildren (VExprVisitor *v) override;

  virtual VStr toString () const override;

protected:
  VStructZero () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};
