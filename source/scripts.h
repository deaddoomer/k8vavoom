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
  /*double*/float Float; // oops; it should be double, but VC cannot into doubles

  char *ScriptPtr;
  char *TokStartPtr;
  vint32 TokStartLine;
  enum {
    Flag_CMode = 1u<<0,
    Flag_Escape = 1u<<1,
    Flag_AllowNumSign = 1u<<2,
    //
    Flag_End = 1u<<3,
    Flag_Crossed = 1u<<4,
    Flag_QuotedString = 1u<<5,
  };
  vuint32 flags;


public:
  VScriptSavedPos (const VScriptParser &par) { saveFrom(par); }

  void saveFrom (const VScriptParser &par);
  void restoreTo (VScriptParser &par) const;
};


class VScriptParser {
  friend struct VScriptSavedPos;

public:
  int Line;
  int TokLine;
  bool End;
  bool Crossed;
  bool QuotedString;
  VStr String;
  VName Name8;
  VName Name;
  int Number;
  /*double*/float Float;

private:
  VStr ScriptName;
  char *ScriptBuffer;
  char *ScriptPtr;
  char *ScriptEndPtr;
  char *TokStartPtr;
  int TokStartLine;
  int ScriptSize;
  int SrcIdx;
  //bool AlreadyGot;
  bool CMode;
  bool Escape;
  bool AllowNumSign;

private:
  VScriptParser () {}

  // advances current position
  // if `changeFlags` is `true`, changes `Crossed` and `Line`
  void SkipComments (bool changeFlags);
  void SkipBlanks (bool changeFlags);

  // slow! returns 0 on EOF
  char PeekOrSkipChar (bool doSkip);

public:
  // it also saves current modes
  inline VScriptSavedPos SavePos () const { return VScriptSavedPos(*this); }

  // it also restores saved modes
  inline void RestorePos (const VScriptSavedPos &pos) { pos.restoreTo(*this); }

public:
  VV_DISABLE_COPY(VScriptParser)

  // deletes `Strm`
  VScriptParser (VStr name, VStream *Strm);
  VScriptParser (VStr name, const char *atext);

  ~VScriptParser ();

  VScriptParser *clone () const;

  inline int GetScriptSize () const noexcept { return ScriptSize; }

  bool IsText ();
  bool IsAtEol ();
  inline void SetCMode (bool val) { CMode = val; }
  inline void SetEscape (bool val) { Escape = val; }
  bool AtEnd ();
  bool GetString ();
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
  bool Check (const char *str);
  bool CheckStartsWith (const char *str);
  void Expect (const char *name);
  bool CheckQuotedString ();
  bool CheckIdentifier ();
  bool CheckNumber ();
  void ExpectNumber (bool allowFloat=false, bool truncFloat=true);
  bool CheckNumberWithSign ();
  void ExpectNumberWithSign ();
  bool CheckFloat ();
  void ExpectFloat ();
  bool CheckFloatWithSign ();
  void ExpectFloatWithSign ();
  void ResetQuoted ();
  void ResetCrossed ();
  void UnGet ();
  void SkipBracketed (bool bracketEaten=false);
  void SkipLine ();
  void Message (const char *message);
  void DebugMessage (const char *message);
  void MessageErr (const char *message);
  void Error (const char *message);
#if !defined(VCC_STANDALONE_EXECUTOR)
  void HostError (const char *message);
#endif

  // slow! returns 0 on EOF
  // doesn't affect flags
  inline char PeekChar () { return PeekOrSkipChar(false); }
  // affects flags
  inline char SkipChar () { return PeekOrSkipChar(true); }

  TLocation GetLoc ();

  inline VStr GetScriptName () const { return ScriptName.cloneUnique(); }
  inline bool IsCMode () const { return CMode; }
  inline bool IsEscape () const { return Escape; }

  // for C mode
  inline bool IsAllowNumSign () const { return AllowNumSign; }
  inline void SetAllowNumSign (bool v) { AllowNumSign = v; }
};


#endif
