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
//**  Copyright (C) 2018-2021 Ketmar Dark
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
//  EToken
//
//  Token types.
//
//==========================================================================
enum EToken {
#define VC_LEXER_DEFTOKEN(name,str)  TK_ ## name,
#include "vc_lexer_tokens.h"
#undef VC_LEXER_DEFTOKEN
  TK_TotalTokenCount,
};


//==========================================================================
//
//  VLexer
//
//  Lexer class.
//
//==========================================================================
class VLexer {
private:
  enum { MAX_QUOTED_LENGTH = 512 }; // with trailing zero
  enum { MAX_IDENTIFIER_LENGTH = 64 }; // with trailing zero; must be less or equal to `MAX_QUOTED_LENGTH`
  enum { EOF_CHARACTER = 127 };
  enum { NON_HEX_DIGIT = 255 };

  static_assert((unsigned)MAX_IDENTIFIER_LENGTH <= (unsigned)MAX_QUOTED_LENGTH, "invalid VLexer configuration");
  static_assert((unsigned)MAX_IDENTIFIER_LENGTH <= (unsigned)NAME_SIZE+1, "invalid VLexer configuration");

  enum {
    CHR_EOF,
    CHR_Letter,
    CHR_Number,
    CHR_Quote,
    CHR_SingleQuote,
    CHR_Special
  };

  enum {
    IF_False,     // skipping the content
    IF_True,      // parsing the content
    IF_ElseFalse, // else case, skipping content
    IF_ElseTrue,  // else case, parsing content
    IF_Skip,      // conditon inside curently skipped code
    IF_ElseSkip,  // else case inside curently skipped code
  };

  struct VSourceFile {
    VSourceFile *Next; // nesting stack
    VStr FileName;
    VStr Path;
    char *FileStart;
    char *FilePtr;
    char *FileEnd;
    char currCh;
    TLocation Loc;
    TLocation CurrChLoc;
    int SourceIdx;
    bool IncLineNumber;
    bool NewLine;
    TArray<int> IfStates;
    bool Skipping;
  };

  //k8: initialization is not thread-safe, but i don't care for now
  static char ASCIIToChrCode[256];
  static vuint8 ASCIIToHexDigit[256];
  static bool tablesInited;

  char tokenStringBuffer[MAX_QUOTED_LENGTH];
  bool sourceOpen;
  char currCh;
  TArray<VStr> defines;
  TArray<VStr> includePath;
  VSourceFile *src;

  inline bool checkStrTk (const char *tokname) const { return (strcmp(tokenStringBuffer, tokname) == 0); }

  // read next character into `currCh`
  void NextChr ();
  // 0 is "currCh"
  char Peek (int dist=0) const noexcept; 

  // this skips comment, `currCh` is the first non-comment char
  // at enter, `currCh` must be the first comment char
  // returns `true` if some comment was skipped
  bool SkipComment ();

  void SkipWhitespaceAndComments ();

  // skip current line, correctly process comments
  // returns `true` if no non-whitespace and non-comment chars were seen
  // used to skip lines in preprocessor
  bool SkipCurrentLine ();

  void ProcessPreprocessor ();
  void ProcessUnknownDirective ();
  void ProcessLineDirective ();
  void ProcessDefine ();
  void ProcessUndef ();
  void ProcessIf ();
  void ProcessIfDef (bool OnTrue);
  void ProcessElse ();
  void ProcessEndIf ();
  void ProcessInclude ();
  void PushSource (VStr FileName);
  void PushSource (VStream *Strm, VStr FileName); // takes ownership
  void PopSource ();
  void ProcessNumberToken ();
  void ProcessChar ();
  void ProcessQuoted (const char ech);
  void ProcessQuoteToken ();
  void ProcessSingleQuoteToken ();
  void ProcessLetterToken (bool);
  void ProcessSpecialToken ();
  void ProcessFileName ();

  // `#if` expression parser
  // <0: error
  int ProcessIfTerm ();
  int ProcessIfLAnd ();
  int ProcessIfLOr ();
  int ProcessIfExpr ();

private:
  // lookahead support
  // is string at [spos..epos) equal to `s`?
  bool isNStrEqu (int spos, int epos, const char *s) const noexcept;
  bool posAtEOS (int cpos) const;
  vuint8 peekChar (int cpos) const; // returns 0 on EOS
  bool skipBlanksFrom (int &cpos) const; // returns `false` on EOS
  EToken skipTokenFrom (int &cpos, VStr *str) const; // calls skipBlanksFrom, returns token type or TK_NoToken

public:
  EToken Token;
  TLocation Location; // of the current token
  TLocation CurrChLocation; // of the current char (currCh)
  vint32 Number;
  float Float;
  char *String;
  VName Name;
  bool NewLine;

  static const char *TokenNames[];

  static inline bool isAlpha (const char ch) noexcept { return ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')); }
  static inline bool isDigit (const char ch) noexcept { return (ch >= '0' && ch <= '9'); }

public:
  // virtual file system callback (can be empty, and is empty by default)
  void *userdata; // arbitrary pointer, not used by the lexer
  // should return `null` if file not found
  // should NOT fail if file not found
  VStream *(*dgOpenFile) (VLexer *self, VStr filename);

public:
  VLexer () noexcept;
  ~VLexer ();

  // returns `nullptr` if file not found
  // by default it tries to call `dgOpenFile()`, if it is specified,
  // otherwise falls back to standard vfs (`VPackage::OpenFileStreamRO()`)
  // should NOT fail if file not found
  virtual VStream *doOpenFile (VStr filename);

  void AddDefine (VStr CondName, bool showWarning=true);
  void RemoveDefine (VStr CondName, bool showWarning=true);
  bool HasDefine (VStr CondName) noexcept;

  void AddIncludePath (VStr DirName);

  void OpenSource (VStr FileName);
  void OpenSource (VStream *astream, VStr FileName); // takes stream ownership

  // return next non-blank non-comment char
  // most of the time this will be `currCh`, but not always
  char peekNextNonBlankChar () const noexcept;

  // this is freakin' slow, and won't cross "include" boundaries
  // offset==0 means "current token"
  // this doesn't process conditional directives,
  // so it is useful only for limited lookups
  EToken peekTokenType (int offset=1, VStr *tkstr=nullptr) const;

  void NextToken ();
  bool Check (EToken tk);
  void Expect (EToken tk);
  void Expect (EToken tk, ECompileError error);
  VStr ExpectAnyIdentifier (); // this also allows keywords

  // check for identifier (it cannot be a keyword)
  bool Check (const char *id, bool caseSensitive=true);
  // expect identifier (it cannot be a keyword)
  void Expect (const char *id, bool caseSensitive=true);
};
