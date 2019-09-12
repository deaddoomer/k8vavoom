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

enum ECompileError {
  ERR_NONE,
  //  File errors
  ERR_CANT_OPEN_FILE,
  ERR_CANT_OPEN_DBGFILE,
  //  Tokeniser erros
  ERR_BAD_RADIX_CONSTANT,
  ERR_STRING_TOO_LONG,
  ERR_EOF_IN_STRING,
  ERR_NEW_LINE_INSIDE_QUOTE,
  ERR_UNKNOWN_ESC_CHAR,
  ERR_IDENTIFIER_TOO_LONG,
  ERR_BAD_CHARACTER,
  //  Syntax errors
  ERR_MISSING_LPAREN,
  ERR_MISSING_RPAREN,
  ERR_MISSING_LBRACE,
  ERR_MISSING_RBRACE,
  ERR_MISSING_COLON,
  ERR_MISSING_SEMICOLON,
  ERR_UNEXPECTED_EOF,
  ERR_BAD_DO_STATEMENT,
  ERR_INVALID_IDENTIFIER,
  ERR_FUNCTION_REDECLARED,
  ERR_MISSING_RFIGURESCOPE,
  ERR_BAD_ARRAY,
  ERR_EXPR_TYPE_MISTMATCH,
  ERR_MISSING_COMMA,

  NUM_ERRORS
};

extern int vcWarningsSilenced;

void ParseWarning (const TLocation &, const char *text, ...) __attribute__((format(printf, 2, 3)));
void ParseWarningAsError (const TLocation &, const char *text, ...) __attribute__((format(printf, 2, 3)));
void ParseError (const TLocation &, const char *text, ...) __attribute__((format(printf, 2, 3)));
void ParseError (const TLocation &, ECompileError error);
void ParseError (const TLocation &, ECompileError error, const char *text, ...) __attribute__((format(printf, 3, 4)));
void BailOut () __attribute__((noreturn));
void VCFatalError (const char *text, ...) __attribute__((noreturn, format(printf, 1, 2)));


// ////////////////////////////////////////////////////////////////////////// //
extern int vcErrorCount;
extern bool vcErrorIncludeCol;


// ////////////////////////////////////////////////////////////////////////// //
// "gag mode" counter; incremented only when errors are "gagged"
extern int vcGagErrorCount;
extern int vcGagErrors; // !0: errors are gagged

// handy class to automatically "ungag" on scope/function exit
class VGagErrors {
private:
  int mGagErrCount;
  int mGagCount; // consistency checks
public:
  VGagErrors () : mGagErrCount(vcGagErrorCount), mGagCount(vcGagErrors) { ++vcGagErrors; }
  ~VGagErrors () { --vcGagErrors; if (vcGagErrors != mGagCount) VCFatalError("unbalanced `VGagErrors`"); }
  // check if there were some new errors since gagging with this object
  inline bool wasErrors () const { return (vcGagErrorCount > mGagErrCount); }
private: // no copies
  VGagErrors (const VGagErrors &v);
  void operator = (const VGagErrors &v);
};
