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
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
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

class VParser {
private:
  enum LocalType {
    LocalNormal,
    LocalFor,
    LocalForeach,
  };

private:
  VLexer &Lex;
  VPackage *Package;
  VMethod *currFunc; // for lambdas
  VClass *currClass; // for lambdas
  VSwitch *currSwitch;
  bool CheckForLocal;
  int anonLocalCount;

  // for error reporting
  TLocation lastCompoundStart;
  bool inCompound;

  TLocation lastCompoundEnd;
  bool hasCompoundEnd;

private:
  void ErrorFieldTypeExpected ();

  // not more than `MAX_PARAMS`; returns argc
  int ParseArgList (const TLocation &stloc, VExpression **argv);
  VExpression *ParseDotMethodCall (VExpression *, VName, const TLocation &);
  VExpression *ParseBaseMethodCall (VName, const TLocation &);
  VExpression *ParseMethodCallOrCast (VName, const TLocation &);
  VLocalDecl *ParseLocalVar (VExpression *TypeExpr, LocalType lt=LocalNormal, VExpression *size0=nullptr, VExpression *size1=nullptr);
  VExpression *ParseExpressionPriority0 ();
  VExpression *ParseExpressionPriority1 ();
  VExpression *ParseExpressionPriority2 ();
  VExpression *ParseExpressionPriority3 ();
  VExpression *ParseExpressionPriority4 ();
  VExpression *ParseExpressionPriority5 ();
  VExpression *ParseExpressionPriority5_1 ();
  VExpression *ParseExpressionPriority6 ();
  VExpression *ParseExpressionPriority7 ();
  VExpression *ParseExpressionPriority8 ();
  VExpression *ParseExpressionPriority9 ();
  VExpression *ParseExpressionPriority10 ();
  VExpression *ParseExpressionPriority11 ();
  VExpression *ParseExpressionPriority12 ();
  VExpression *ParseExpressionPriority13 ();
  VExpression *ParseExpressionPriority14 (bool allowAssign=false);
  VExpression *ParseExpression (bool allowAssign=false);
  VLocalDecl *CreateUnnamedLocal (VFieldType type, const TLocation &loc);
  VStatement *ParseForeachIterator (const TLocation &l);
  bool ParseForeachOptions (); // returns `true` if `reversed` was found
  VStatement *ParseForeachRange (const TLocation &l); // array or iota
  VStatement *ParseForeach ();
  VStatement *ParseStatement ();
  /*VCompound*/VStatement *ParseCompoundStatement (const TLocation &l);
  enum { TCRF_Const=0x01, TCRF_Ref=0x02 };
  void ParseOptionalConstRef (int *constref);
  VExpression *ParseOptionalTypeDecl (EToken tkend, int *constref); // used in things like `for (type var = ...)`
  VExpression *ParsePrimitiveType (); // this won't parse `type*` and delegates
  VExpression *ParseType (bool allowDelegates=false, int *constref=nullptr); // this won't parse `type*`
  VExpression *ParseTypePtrs (VExpression *type); // call this after `ParseType` to parse asterisks
  VExpression *ParseTypeWithPtrs (bool allowDelegates=false); // convenient wrapper
  void ParseMethodDef (VExpression *, VName, const TLocation &, VClass *, vint32, bool);
  void ParseDelegate (VExpression *RetType, VField *Delegate);
  VExpression *ParseLambda ();
  void ParseDefaultProperties (VClass *InClass, bool doparse, int defcount, VStatement **stats);
  void ParseStruct (VClass *InClass, bool IsVector);
  VName ParseStateString ();
  void ParseStates (VClass *InClass);
  void ParseStatesNewStyleUnused (VClass *inClass);
  void ParseStatesNewStyle (VClass *inClass);
  void ParseReplication (VClass *InClass);
  void ParseClass ();

public:
  VParser (VLexer &ALex, VPackage *APackage);
  void Parse ();
};
