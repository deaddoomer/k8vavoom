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
#ifndef VAVOOM_SCRIPTS_HEADER
#define VAVOOM_SCRIPTS_HEADER


class VScriptParser;


struct VScriptSavedPos {
public:
  vint32 Line;
  vint32 TokLine;
  VStr String;
  VName Name8;
  VName Name;
  vint32 Number;
  float Float; // oops; it was double, but VC cannot into doubles

  char *ScriptPtr;
  char *TokStartPtr;
  vint32 TokStartLine;
  enum {
    Flag_None = 0u,
    //
    Flag_CMode = 1u<<0,
    Flag_Escape = 1u<<1,
    Flag_AllowNumSign = 1u<<2,
    Flag_EDGEMode = 1u<<3,
    //
    Flag_End = 1u<<4,
    Flag_Crossed = 1u<<5,
    Flag_QuotedString = 1u<<6,
  };
  vuint32 flags;


public:
  inline VScriptSavedPos (const VScriptParser &par) noexcept { saveFrom(par); }

  void saveFrom (const VScriptParser &par) noexcept;
  void restoreTo (VScriptParser &par) const noexcept;
};


/*
 defaults:
   CMode: false
   Escape: true
   AllowNumSign: true

  `AllowNumSign` has any effect only for c-mode, non-c mode doesn't differentiate between numbers and ids

  if `CMode` is `false`:
    ';' marks single-line comment
    allowed newlines in quoted string
    identifiers can include single-quote char (but cannot start or end with it, and no double SQ),
    and other special chars except and ` {}|=,;" ` (excluding back quotes)

  if `CMode` is `true`:
    identifiers cannot include `@`, `.`, and other non-alpha chars (including `$`), but
    can include single-quote char (yet cannot start or end with it, and no double SQ).
    special non-alpha identifiers includes c-like tokens, a-la `==` or `>>>`

  `Escape` allows escape processing inside quoted strings.
  non-c escape set is very limited:
    \r
    \n
    \c (color escape)
  in c mode, some more escapes are allowed, including:
    \e (\x1b)
    \xNN -- hex char code, 0 will be converted to space
*/
class VScriptParser {
  friend struct VScriptSavedPos;

public:
  int Line;
  int TokLine;
  // set if `GetString()` reached EOT without reading anything
  bool End;
  // set if `GetString()` moved to the new line, reset otherwise
  bool Crossed;
  // set if `GetString()` got quoted string
  bool QuotedString;
  VStr String;
  VName Name8;
  VName Name;
  int Number;
  float Float;

private:
  VStr ScriptName;
  mutable int SrcIdx;
  char *ScriptBuffer;
  char *ScriptPtr;
  char *ScriptEndPtr;
  char *TokStartPtr;
  int TokStartLine;
  int ScriptSize;
  bool CMode;
  bool Escape;
  bool AllowNumSign;
  bool EDGEMode; // ignore underscores in indentifiers?

public:
  int SourceLump; // usually -1

private:
  inline VScriptParser () noexcept : SrcIdx(-1), SourceLump(-1) {}

  // advances current position
  // if `changeFlags` is `true`, changes `Crossed` and `Line`
  void SkipComments (bool changeFlags) noexcept;
  void SkipBlanks (bool changeFlags) noexcept;

  // slow! returns 0 on EOF
  char PeekOrSkipChar (bool doSkip) noexcept;

  void ParseQuotedString (const char qch) noexcept;

  void ParseCMode () noexcept;
  void ParseNonCMode () noexcept;

  // uses `String`
  float parseFloat (bool silent, bool *error);

public:
  // it also saves current modes
  inline VScriptSavedPos SavePos () const noexcept { return VScriptSavedPos(*this); }

  // it also restores saved modes
  inline void RestorePos (const VScriptSavedPos &pos) noexcept { pos.restoreTo(*this); }

public:
  VV_DISABLE_COPY(VScriptParser)

  // deletes `Strm`
  VScriptParser (VStr name, VStream *Strm, int aSourceLump=-1);
  VScriptParser (VStr name, const char *atext, int aSourceLump=-1) noexcept;

#if !defined(VCC_STANDALONE_EXECUTOR)
  // this sets `SourceLump`
  // can return `nullptr` for invalid lump
  static VScriptParser *NewWithLump (int Lump);
#endif

  ~VScriptParser () noexcept;

  VScriptParser *clone () const noexcept;

  inline int GetScriptSize () const noexcept { return ScriptSize; }

  bool IsText () noexcept;
  bool IsAtEol () noexcept;

  inline bool IsCMode () const noexcept { return CMode; }
  inline void SetCMode (bool val) noexcept { CMode = val; }
  inline bool IsEscape () const noexcept { return Escape; }
  inline void SetEscape (bool val) noexcept { Escape = val; }
  inline bool IsEDGEMode () const noexcept { return EDGEMode; }
  inline void SetEDGEMode (bool val) noexcept { EDGEMode = val; }
  // for C mode
  inline bool IsAllowNumSign () const noexcept { return AllowNumSign; }
  inline void SetAllowNumSign (bool v) noexcept { AllowNumSign = v; }

  bool AtEnd () noexcept;
  bool GetString () noexcept;
#if !defined(VCC_STANDALONE_EXECUTOR)
  vuint32 ExpectColor (); // returns parsed color, either in string form, or r,g,b triplet
#endif

  void ExpectString ();
  void ExpectLoneChar (); // in `String`
  void ExpectName8 ();
  void ExpectName8Warn (); // this sets both `Name` and `Name8`, with warning
  // this sets both `Name` and `Name8`, with warning if the string doesn't contain slashes
  // returns `true` if we got a simple name
  // it also fixes slashes
  bool ExpectName8WarnOrFilePath ();
  void ExpectName8Def (VName def);
  void ExpectName ();
  void ExpectIdentifier ();
  void ExpectNumber (bool allowFloat=false, bool truncFloat=true);
  void ExpectNumberWithSign ();
  void ExpectFloat ();
  void ExpectFloatWithSign ();
  void Expect (const char *name);

  bool Check (const char *str) noexcept;
  bool CheckStartsWith (const char *str) noexcept;
  bool CheckQuotedString () noexcept;
  bool CheckIdentifier () noexcept;
  bool CheckNumber () noexcept;
  bool CheckNumberWithSign () noexcept;
  bool CheckFloat () noexcept;
  bool CheckFloatWithSign () noexcept;

  void ResetQuoted () noexcept;
  void ResetCrossed () noexcept;

  void UnGet () noexcept;

  void SkipBracketed (bool bracketEaten=false) noexcept;
  void SkipLine () noexcept;

  void Message (const char *message);
  void DebugMessage (const char *message);
  void MessageErr (const char *message);
  void Error (const char *message);
#if !defined(VCC_STANDALONE_EXECUTOR)
  void HostError (const char *message);
#endif

  void Messagef (const char *fmt, ...) __attribute__((format(printf, 2, 3)));
  void DebugMessagef (const char *fmt, ...) __attribute__((format(printf, 2, 3)));
  void MessageErrf (const char *fmt, ...) __attribute__((format(printf, 2, 3)));
  void Errorf (const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#if !defined(VCC_STANDALONE_EXECUTOR)
  void HostErrorf (const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#endif

  // slow! returns 0 on EOF
  // doesn't affect flags
  inline char PeekChar () noexcept { return PeekOrSkipChar(false); }
  // affects flags
  inline char SkipChar () noexcept { return PeekOrSkipChar(true); }

  inline VTextLocation GetLoc () const noexcept { return VTextLocation(ScriptName, TokLine, 1); }

  TLocation GetVCLoc () const noexcept;

  inline VStr GetScriptName () const noexcept { return ScriptName.cloneUniqueMT(); }

#if !defined(VCC_STANDALONE_EXECUTOR)
  // the following will try to find an include in the same file as `srclump`
  // negative result means "file not found"
  // worker
  static VVA_CHECKRESULT int FindRelativeIncludeLump (int srclump, VStr fname) noexcept;
  // main API
  static VVA_CHECKRESULT int FindIncludeLump (int srclump, VStr fname) noexcept;
#endif
};


#endif
