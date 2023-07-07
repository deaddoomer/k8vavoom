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
#ifndef VTEXTPARSER_HEADER
#define VTEXTPARSER_HEADER


class VTextLocation {
private:
  int Line;
  int Col;
  VStr FileName;

private:
  void SetFileName (VStr fname) noexcept;

public:
  inline VTextLocation () noexcept : Line(0), Col(0), FileName() {}
  inline VTextLocation (VStr fname, int ALine, int ACol) noexcept : Line(ALine > 0 ? ALine : 0), Col(ACol > 0 ? ACol : 0), FileName() { SetFileName(fname); }

  inline int GetLine () const noexcept { return Line; }
  inline void SetLine (int ALine) noexcept { Line = ALine; }

  inline int GetCol () const noexcept { return Col; }
  inline void SetCol (int ACol) noexcept { Col = ACol; }

  inline VStr GetSourceFile () const noexcept { return FileName; }
  inline void SetSourceFile (VStr fname) noexcept { SetFileName(fname); }

  inline VStr GetSourceFileStr () const noexcept { return (FileName.isEmpty() ? VStr::EmptyString : FileName+":"); }

  VStr toString () const noexcept; // source file, line, column
  VStr toStringNoCol () const noexcept; // source file, line
  VStr toStringLineCol () const noexcept; // line, column
  VStr toStringShort () const noexcept; // source file w/o path, line

  inline void ConsumeChar (bool doNewline) noexcept {
    if (doNewline) { ++Line; Col = 1; } else ++Col;
  }

  inline void ConsumeChars (int count) noexcept { Col += count; }
  inline void NewLine () noexcept { ++Line; Col = 1; }

public:
  static int GetFileNameCount () noexcept;
  static void ResetFileNames () noexcept;
};


class VTextParser {
public:
  // token types
  enum EToken {
    TK_Error = -1,
    TK_EOF = 0,
    TK_Id,
    TK_Int,
    TK_Float,
    TK_Name, // single-quoted
    TK_String, // double-quoted
    TK_Delim,
  };

private:
  enum { EOF_CHARACTER = 0 };
  enum { EOL_CHARACTER = '\n' };

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
    char *FileText;
    char *FileStart;
    char *FilePtr;
    char *FileEnd;
    char currCh;
    VTextLocation Loc;
    VTextLocation CurrChLoc;
    int SourceIdx;
    bool IncLineNumber;
    TArray<int> IfStates;
    bool Skipping;
    bool EDGEMode; // ignore underscores? (default: false)
    bool SignedNumbers; // default: false
  };

  bool sourceOpen;
  char currCh;
  TArray<VStr> defines;
  TArray<VStr> includePath;
  VSourceFile *src;

private:
  // read next character into `currCh`
  void NextChr ();
  // 0 is "currCh"
  char Peek (int dist=0) const noexcept;

  // this skips comment, `currCh` is the first non-comment char
  // at enter, `currCh` must be the first comment char
  // returns non-zero if some comment was skipped
  // if positive, there was a newline
  int SkipComment ();

  // returns `true` if newline was skipped
  bool SkipWhitespaceAndComments ();

  // skip current line, correctly process comments
  // returns `true` if no non-whitespace and non-comment chars were seen
  // used to skip lines in preprocessor
  // doesn't skip EOL char
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
  void PushSource (VStr FileName, bool asEDGE=false);
  void PushSource (VStream *Strm, VStr FileName, bool asEDGE=false); // takes ownership
  void PopSource ();
  void ProcessNumberToken ();
  void ProcessChar ();
  void ProcessQuoted (const char ech);
  void ProcessQuoteToken ();
  void ProcessSingleQuoteToken ();
  void ProcessLetterToken ();
  void ProcessSpecialToken ();
  // returns `true` if this is a system include
  bool ProcessFileName (const char *pdname);

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

  void clearToken () noexcept;

public:
  EToken Token;
  VStr TokenStr; // collected string (always valid)
  int TokenInt; // int token value; set to 0 for non-int token
  float TokenFloat; // float token value; set to 0 for non-float token
  VTextLocation Location; // of the current token
  VTextLocation CurrChLocation; // of the current char (currCh)

  static VVA_FORCEINLINE bool isAlpha (const char ch) noexcept { return ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')); }
  static VVA_FORCEINLINE bool isDigit (const char ch) noexcept { return (ch >= '0' && ch <= '9'); }
  static VVA_FORCEINLINE bool isIdChar (const char ch) noexcept { return (ch == '_' || ch == '$' || isAlpha(ch)); }
  static VVA_FORCEINLINE bool isIdDigit (const char ch) noexcept { return (isIdChar(ch) || isDigit(ch)); }

  VVA_FORCEINLINE bool isBlankChar (const char ch) const noexcept { return ((vuint8)ch <= ' ' || (ch == '_' && src && src->EDGEMode)); }

public:
  // virtual file system callback (can be empty, and is empty by default)
  void *userdata; // arbitrary pointer, not used by the lexer
  // should return `null` if file not found
  // should NOT fail if file not found
  VStream *(*dgOpenFile) (VTextParser *self, VStr filename);

  // `currPath` can be empty
  // return `nullptr` for "file not found" error
  VStream *(*dgOpenIncludeStream) (VTextParser *self, VStr filename, VStr currPath, bool asSystem);

  void (*dgFatalError) (VTextParser *self, const VTextLocation &loc, const char *msg);

private:
  // called from ctors
  void initParser () noexcept;

  bool CheckIdInternal (const char *s, bool caseSens);
  void ExpectIdInternal (const char *s, bool caseSens);

public:
  VTextParser (VStream *st, bool asEDGE=false) noexcept; // will own the stream
  VTextParser () noexcept;
  virtual ~VTextParser ();

  inline bool IsEOF () const noexcept { return (Token == TK_EOF); }

  inline const char *GetTokenTypeName () const noexcept {
    switch (Token) {
      case TK_Error: return "error";
      case TK_EOF: return "EOF";
      case TK_Id: return "id";
      case TK_Int: return "int";
      case TK_Float: return "float";
      case TK_Name: return "name";
      case TK_String: return "string";
      case TK_Delim: return "delimiter";
    }
    return "fuck";
  }

  inline bool IsEDGEMode () const noexcept { return (src ? src->EDGEMode : false); }
  inline void SetEDGEMode (const bool v) noexcept { if (src) src->EDGEMode = v; }

  inline bool IsSignedNumbers () const noexcept { return (src ? src->SignedNumbers : false); }
  inline void SetSignedNumbers (const bool v) noexcept { if (src) src->SignedNumbers = v; }

  // returns `nullptr` if file not found
  // by default it tries to call `dgOpenFile()`, if it is specified
  // should NOT fail if file not found
  virtual VStream *doOpenFile (VStr filename);

  // override if necessary
  // will never be called with empty string
  virtual void MsgDebug (const VTextLocation &loc, const char *msg) const;
  virtual void MsgWarning (const VTextLocation &loc, const char *msg) const;
  virtual void MsgError (const VTextLocation &loc, const char *msg) const;

  // will call `dgFatalError()` or `Sys_Error()`
  virtual void FatalError (const VTextLocation &loc, const char *msg);

  virtual VStream *OpenIncludeStream (VStr filename, bool asSystem);

  void AddDefine (VStr CondName, bool showWarning=true);
  void RemoveDefine (VStr CondName, bool showWarning=true);
  bool HasDefine (VStr CondName) noexcept;

  inline void ClearDefines () noexcept { defines.clear(); }
  inline int DegfineCount () const noexcept { return defines.length(); }
  inline VStr DefineAt (int idx) const noexcept { return (idx >= 0 && idx < defines.length() ? defines[idx] : VStr::EmptyString); }

  void AddIncludePath (VStr DirName);

  inline void ClearIncludes () noexcept { includePath.clear(); }
  inline int IncludeCount () const noexcept { return includePath.length(); }
  inline VStr IncludeAt (int idx) const noexcept { return (idx >= 0 && idx < includePath.length() ? includePath[idx] : VStr::EmptyString); }

  void OpenSource (VStr FileName, bool asEDGE=false);
  void OpenSource (VStream *astream, VStr FileName, bool asEDGE=false); // takes stream ownership

  // return next non-blank non-comment char
  // most of the time this will be `currCh`, but not always
  char peekNextNonBlankChar () const noexcept;

  void NextToken ();

  // returns `true` if current token is a sign, and next non-blank char is a decimal digit
  bool IsPossibleSignedNumber () const noexcept;

  inline void ExpectId (const char *s) { ExpectIdInternal(s, true); }
  inline void ExpectIdCI (const char *s) { ExpectIdInternal(s, false); }
  void ExpectDelim (const char *s);

  VStr ExpectId ();
  VStr ExpectString (); // double-quoted string
  VStr ExpectName (); // single-quoted string

  int ExpectInt ();
  int ExpectSignedInt ();
  float ExpectFloat ();
  float ExpectSignedFloat ();

  float ExpectFloatAllowInt ();
  float ExpectSignedFloatAllowInt ();

  inline bool CheckId (const char *s) { return CheckIdInternal(s, true); }
  inline bool CheckIdCI (const char *s) { return CheckIdInternal(s, false); }
  bool CheckDelim (const char *s);

  bool CheckInt (int *val);
  bool CheckFloat (float *val);
};


#endif
