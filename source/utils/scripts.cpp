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
#if !defined(VCC_STANDALONE_EXECUTOR)
# include "gamedefs.h"
# include "language.h"
# include "filesys/files.h"
#elif defined(VCC_STANDALONE_EXECUTOR)
# include "../../vccrun/vcc_run_vc.h"
# include "../../libs/vavoomc/vc_public.h"
# include "scripts.h"
#endif

#if !defined(VCC_STANDALONE_EXECUTOR)
static VCvarB dbg_show_name_remap("dbg_show_name_remap", false, "Show hacky name remapping", CVAR_PreInit|CVAR_NoShadow);
#endif


// ////////////////////////////////////////////////////////////////////////// //
class VScriptsParser : public VObject {
  DECLARE_CLASS(VScriptsParser, VObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VScriptsParser)

  VScriptParser *Int;

  virtual void Destroy () override;
  void CheckInterface ();

#if !defined(VCC_STANDALONE_EXECUTOR)
  DECLARE_FUNCTION(OpenLumpName)
  DECLARE_FUNCTION(OpenLumpIndex)
#endif
  DECLARE_FUNCTION(OpenLumpFullName)
  DECLARE_FUNCTION(OpenString)
  DECLARE_FUNCTION(get_String)
  DECLARE_FUNCTION(get_Number)
  DECLARE_FUNCTION(get_Float)
  DECLARE_FUNCTION(get_Crossed)
  DECLARE_FUNCTION(get_Quoted)
  DECLARE_FUNCTION(get_SourceLump)
  DECLARE_FUNCTION(set_SourceLump)
  DECLARE_FUNCTION(get_Escape)
  DECLARE_FUNCTION(set_Escape)
  DECLARE_FUNCTION(get_CMode)
  DECLARE_FUNCTION(set_CMode)
  DECLARE_FUNCTION(get_AllowNumSign)
  DECLARE_FUNCTION(set_AllowNumSign)
  DECLARE_FUNCTION(get_EDGEMode)
  DECLARE_FUNCTION(set_EDGEMode)
  DECLARE_FUNCTION(get_EndOfText)
  DECLARE_FUNCTION(IsText)
  DECLARE_FUNCTION(IsAtEol)
  DECLARE_FUNCTION(IsCMode)
  DECLARE_FUNCTION(IsAllowNumSign)
  DECLARE_FUNCTION(SetCMode)
  DECLARE_FUNCTION(SetAllowNumSign)
  DECLARE_FUNCTION(IsEscape)
  DECLARE_FUNCTION(SetEscape)
  DECLARE_FUNCTION(AtEnd)
  DECLARE_FUNCTION(GetString)
#if !defined(VCC_STANDALONE_EXECUTOR)
  DECLARE_FUNCTION(ExpectColor)
#endif
  DECLARE_FUNCTION(ExpectString)
  DECLARE_FUNCTION(ExpectLoneChar)
  DECLARE_FUNCTION(Check)
  DECLARE_FUNCTION(CheckStartsWith)
  DECLARE_FUNCTION(Expect)
  DECLARE_FUNCTION(CheckIdentifier)
  DECLARE_FUNCTION(ExpectIdentifier)
  DECLARE_FUNCTION(CheckNumber)
  DECLARE_FUNCTION(ExpectNumber)
  DECLARE_FUNCTION(CheckFloat)
  DECLARE_FUNCTION(ExpectFloat)
  DECLARE_FUNCTION(ResetQuoted)
  DECLARE_FUNCTION(ResetCrossed)
  DECLARE_FUNCTION(SkipBracketed)
  DECLARE_FUNCTION(UnGet)
  DECLARE_FUNCTION(SkipLine)
  DECLARE_FUNCTION(FileName)
  DECLARE_FUNCTION(CurrLine)
  DECLARE_FUNCTION(TokenLine)
  DECLARE_FUNCTION(ScriptError)
  DECLARE_FUNCTION(ScriptMessage)

  DECLARE_FUNCTION(SavePos)
  DECLARE_FUNCTION(RestorePos)

#if !defined(VCC_STANDALONE_EXECUTOR)
  DECLARE_FUNCTION(FindRelativeIncludeLump)
  DECLARE_FUNCTION(FindIncludeLump)
#endif
};

IMPLEMENT_CLASS(V, ScriptsParser)


static uint32_t c2spec[256/32]; // c-like two-chars specials
static uint32_t cidterm[256/32]; // c-like identifier terminator
static uint32_t cnumterm[256/32]; // c-like number terminator
static uint32_t ncidterm[256/32]; // non-c-like identifier terminator

struct CharClassifier {
  static VVA_FORCEINLINE bool isC2Spec (char ch) noexcept { return (c2spec[(ch>>5)&0x07]&(1U<<(ch&0x1f))); }
  static VVA_FORCEINLINE bool isCIdTerm (char ch) noexcept { return (cidterm[(ch>>5)&0x07]&(1U<<(ch&0x1f))); }
  static VVA_FORCEINLINE bool isCNumTerm (char ch) noexcept { return (cnumterm[(ch>>5)&0x07]&(1U<<(ch&0x1f))); }
  static VVA_FORCEINLINE bool isNCIdTerm (char ch) noexcept { return (ncidterm[(ch>>5)&0x07]&(1U<<(ch&0x1f))); }

  static VVA_FORCEINLINE void setCharBit (vuint32 *set, char ch) noexcept { set[(ch>>5)&0x07] |= (1U<<(ch&0x1f)); }

  static VVA_FORCEINLINE bool isDigit (char ch) noexcept { return (ch >= '0' && ch <= '9'); }

  static inline bool isNumStart (const char *s, bool allowNumSign) noexcept {
    if (allowNumSign) if (*s == '+' || *s == '-') ++s;
    if (*s == '.') ++s;
    return isDigit(*s);
  }

  CharClassifier () noexcept {
    /*static*/ const char *cIdTerm = "`~!#$%^&*(){}[]/=\\?-+|;:<>,\"'"; // was with '@'
    /*static*/ const char *ncIdTerm = "{}|=,;\"'";
    /*static*/ const char *c2specStr = "=!<>+-*/%&|^~";
    memset(c2spec, 0, sizeof(cidterm));
    memset(cidterm, 0, sizeof(cidterm));
    memset(cnumterm, 0, sizeof(cnumterm));
    memset(ncidterm, 0, sizeof(ncidterm));
    for (const char *s = c2specStr; *s; ++s) {
      setCharBit(c2spec, *s);
    }
    for (const char *s = cIdTerm; *s; ++s) {
      setCharBit(cidterm, *s);
      setCharBit(cnumterm, *s);
    }
    for (const char *s = ncIdTerm; *s; ++s) setCharBit(ncidterm, *s);
    // blanks will terminate too
    for (int ch = 0; ch <= 32; ++ch) {
      setCharBit(cidterm, ch);
      setCharBit(cnumterm, ch);
      setCharBit(ncidterm, ch);
    }
    // c identified is terminated with dot
    setCharBit(cidterm, '.');
    // sanity check
    for (int f = 0; f < 256; ++f) {
      if (f <= 32 || f == '.' || strchr(cIdTerm, f)) {
        vassert(isCIdTerm(f));
        if (f == '.') { vassert(!isCNumTerm(f)); } else { vassert(isCNumTerm(f)); }
      } else {
        vassert(!isCIdTerm(f));
        vassert(!isCNumTerm(f));
      }
    }
  }
};

CharClassifier charClassifierInit;



//==========================================================================
//
//  VScriptSavedPos::saveFrom
//
//==========================================================================
void VScriptSavedPos::saveFrom (const VScriptParser &par) noexcept {
  Line = par.Line;
  TokLine = par.TokLine;
  String = par.String;
  Name8 = par.Name8;
  Name = par.Name;
  Number = par.Number;
  Float = par.Float;

  ScriptPtr = par.ScriptPtr;
  TokStartPtr = par.TokStartPtr;
  TokStartLine = par.TokStartLine;
  flags =
    (par.CMode ? Flag_CMode : Flag_None)|
    (par.Escape ? Flag_Escape : Flag_None)|
    (par.AllowNumSign ? Flag_AllowNumSign : Flag_None)|
    (par.EDGEMode ? Flag_EDGEMode : Flag_None)|
    //
    (par.End ? Flag_End : Flag_None)|
    (par.Crossed ? Flag_Crossed : Flag_None)|
    (par.QuotedString ? Flag_QuotedString : Flag_None)|
    0u;
}


//==========================================================================
//
//  VScriptSavedPos::restoreTo
//
//==========================================================================
void VScriptSavedPos::restoreTo (VScriptParser &par) const noexcept {
  par.Line = Line;
  par.TokLine = TokLine;
  par.String = String;
  par.Name8 = Name8;
  par.Name = Name;
  par.Number = Number;
  par.Float = Float;

  par.ScriptPtr = ScriptPtr;
  par.TokStartPtr = TokStartPtr;
  par.TokStartLine = TokStartLine;

  par.CMode = !!(flags&Flag_CMode);
  par.Escape = !!(flags&Flag_Escape);
  par.AllowNumSign = !!(flags&Flag_AllowNumSign);
  par.EDGEMode = !!(flags&Flag_EDGEMode);
  par.End = !!(flags&Flag_End);
  par.Crossed = !!(flags&Flag_Crossed);
  par.QuotedString = !!(flags&Flag_QuotedString);
}



//==========================================================================
//
//  VScriptParser::VScriptParser
//
//==========================================================================
VScriptParser::VScriptParser (VStr name, VStream *Strm, int aSourceLump)
  : Line(1)
  , TokLine(1)
  , End(false)
  , Crossed(false)
  , QuotedString(false)
  , ScriptName(name.cloneUniqueMT())
  , SrcIdx(-1)
  , CMode(false)
  , Escape(true)
  , AllowNumSign(false)
  , EDGEMode(false)
  , SourceLump(aSourceLump)
{
  if (!Strm) {
    ScriptSize = 1;
    ScriptBuffer = new char[ScriptSize+1];
    ScriptBuffer[0] = '\n';
    ScriptBuffer[1] = 0;
  } else {
    try {
      if (Strm->IsError()) Host_Error("cannot read definition file '%s:%s'", *name, *Strm->GetName());
      ScriptSize = Strm->TotalSize();
      if (Strm->IsError()) Host_Error("cannot read definition file '%s:%s'", *name, *Strm->GetName());
      ScriptBuffer = new char[ScriptSize+1];
      Strm->Serialise(ScriptBuffer, ScriptSize);
      ScriptBuffer[ScriptSize] = 0;
      if (Strm->IsError()) { delete ScriptBuffer; Host_Error("cannot read definition file '%s:%s'", *name, *Strm->GetName()); }
    } catch (...) {
      VStream::Destroy(Strm);
      throw;
    }
    VStream::Destroy(Strm);
  }

  ScriptPtr = ScriptBuffer;
  ScriptEndPtr = ScriptPtr+ScriptSize;

  TokStartPtr = ScriptPtr;
  TokStartLine = Line;

  // skip garbage some editors add in the begining of UTF-8 files
  if (*(const vuint8 *)ScriptPtr == 0xef && *(const vuint8 *)(ScriptPtr+1) == 0xbb && *(const vuint8 *)(ScriptPtr+2) == 0xbf) ScriptPtr += 3;
}


//==========================================================================
//
//  VScriptParser::VScriptParser
//
//==========================================================================
VScriptParser::VScriptParser (VStr name, const char *atext, int aSourceLump) noexcept
  : Line(1)
  , TokLine(1)
  , End(false)
  , Crossed(false)
  , QuotedString(false)
  , ScriptName(name.cloneUniqueMT())
  , SrcIdx(-1)
  , CMode(false)
  , Escape(true)
  , AllowNumSign(false)
  , EDGEMode(false)
  , SourceLump(aSourceLump)
{
  if (atext && atext[0]) {
    ScriptSize = (int)strlen(atext);
    ScriptBuffer = new char[ScriptSize+1];
    if (ScriptSize) memcpy(ScriptBuffer, atext, ScriptSize);
    ScriptBuffer[ScriptSize] = 0;
  } else {
    ScriptSize = 1;
    ScriptBuffer = new char[ScriptSize+1];
    ScriptBuffer[0] = 0;
  }

  ScriptPtr = ScriptBuffer;
  ScriptEndPtr = ScriptPtr+ScriptSize;

  TokStartPtr = ScriptPtr;
  TokStartLine = Line;

  // skip garbage some editors add in the begining of UTF-8 files
  if (*(const vuint8 *)ScriptPtr == 0xef && *(const vuint8 *)(ScriptPtr+1) == 0xbb && *(const vuint8 *)(ScriptPtr+2) == 0xbf) ScriptPtr += 3;
}


#if !defined(VCC_STANDALONE_EXECUTOR)
//==========================================================================
//
//  VScriptParser::NewWithLump
//
//==========================================================================
VScriptParser *VScriptParser::NewWithLump (int Lump) {
  if (Lump < 0) return new VScriptParser("{nullfile}", "");
  VStream *st = W_CreateLumpReaderNum(Lump);
  if (!st) return new VScriptParser("{nullfile}", "");
  return new VScriptParser(W_FullLumpName(Lump), st, Lump);
}
#endif


//==========================================================================
//
//  VScriptParser::~VScriptParser
//
//==========================================================================
VScriptParser::~VScriptParser () noexcept {
  delete[] ScriptBuffer;
  ScriptBuffer = nullptr;
}


//==========================================================================
//
//  VScriptParser::clone
//
//==========================================================================
VScriptParser *VScriptParser::clone () const noexcept {
  VScriptParser *res = new VScriptParser();

  res->ScriptBuffer = new char[ScriptSize+1];
  if (ScriptSize) memcpy(res->ScriptBuffer, ScriptBuffer, ScriptSize);
  res->ScriptBuffer[ScriptSize] = 0;

  res->ScriptPtr = res->ScriptBuffer+(ScriptPtr-ScriptBuffer);
  res->ScriptEndPtr = res->ScriptBuffer+(ScriptEndPtr-ScriptBuffer);

  res->TokStartPtr = res->ScriptBuffer+(TokStartPtr-ScriptBuffer);
  res->TokStartLine = res->TokStartLine;

  res->Line = Line;
  res->TokLine = TokLine;
  res->End = End;
  res->Crossed = Crossed;
  res->QuotedString = QuotedString;
  res->String = String;
  res->Name8 = Name8;
  res->Name = Name;
  res->Number = Number;
  res->Float = Float;

  res->ScriptName = ScriptName.cloneUniqueMT();
  res->ScriptSize = ScriptSize;
  res->CMode = CMode;
  res->Escape = Escape;
  res->AllowNumSign = AllowNumSign;
  res->EDGEMode = EDGEMode;
  res->SourceLump = SourceLump;

  return res;
}


//==========================================================================
//
// VScriptParser::IsText
//
//==========================================================================
bool VScriptParser::IsText () noexcept {
  int i = 0;
  while (i < ScriptSize) {
    vuint8 ch = *(const vuint8 *)(ScriptBuffer+(i++));
    if (ch == 127) return false;
    if (ch < ' ' && ch != '\n' && ch != '\r' && ch != '\t') return false;
    if (ch < 128) continue;
    // utf8 check
    int cnt, val;
         if ((ch&0xe0) == 0xc0) { val = ch&0x1f; cnt = 1; }
    else if ((ch&0xf0) == 0xe0) { val = ch&0x0f; cnt = 2; }
    else if ((ch&0xf8) == 0xf0) { val = ch&0x07; cnt = 3; }
    else return false; // invalid utf8
    do {
      if (i >= ScriptSize) return false;
      ch = ScriptBuffer[i++];
      if ((ch&0xc0) != 0x80) return false; // invalid utf8
      val = (val<<6)|(ch&0x3f);
    } while (--cnt);
    // check for valid codepoint
    if (!(val < 0xD800 || (val > 0xDFFF && val <= 0x10FFFF))) return false; // invalid codepoint
  }
  return true;
}


//==========================================================================
//
//  VScriptParser::AtEnd
//
//==========================================================================
bool VScriptParser::AtEnd () noexcept {
  if (GetString()) {
    //fprintf(stderr, "<%s>\n", *String);
    UnGet();
    return false;
  }
  return true;
}


//==========================================================================
//
//  VScriptParser::IsAtEol
//
//==========================================================================
bool VScriptParser::IsAtEol () noexcept {
  int commentLevel = 0;
  for (const char *s = ScriptPtr; s < ScriptEndPtr; ++s) {
    const vuint8 ch = *(const vuint8 *)s;
    if (ch == '\r' && s[1] == '\n') return true;
    if (ch == '\n') return true;
    if (!commentLevel) {
      if (!CMode && ch == ';') return true; // this is single-line comment, it always ends with EOL
      const char c1 = s[1];
      if (ch == '/' && c1 == '/') return true; // this is single-line comment, it always ends with EOL
      if (ch == '/' && c1 == '*') {
        // multiline comment
        ++s; // skip slash
        commentLevel = -1;
        continue;
      }
      if (ch == '/' && c1 == '+') {
        // multiline comment
        ++s; // skip slash
        commentLevel = 1;
        continue;
      }
      if (ch > ' ') return false;
    } else {
      // in multiline comment
      const char c1 = s[1];
      if (commentLevel < 0) {
        if (ch == '*' && c1 == '/') {
          ++s; // skip star
          commentLevel = 0;
        }
      } else {
        if (ch == '/' && c1 == '+') {
          ++s; // skip slash
          ++commentLevel;
        } else if (ch == '+' && c1 == '/') {
          ++s; // skip plus
          --commentLevel;
        }
      }
    }
  }
  return true;
}


//==========================================================================
//
//  VScriptParser::SkipComments
//
//  this is moved out of `SkipBlanks()`, so i can use it in `SkipLine()`
//
//==========================================================================
void VScriptParser::SkipComments (bool changeFlags) noexcept {
  while (ScriptPtr < ScriptEndPtr) {
    const char c0 = *ScriptPtr;
    const char c1 = (ScriptPtr+1 < ScriptEndPtr ? ScriptPtr[1] : 0);
    // single-line comment?
    if ((!CMode && c0 == ';') || (c0 == '/' && c1 == '/')) {
      while (*ScriptPtr++ != '\n') if (ScriptPtr >= ScriptEndPtr) break;
      if (changeFlags) { ++Line; Crossed = true; }
      continue;
    }
    // multiline comment?
    if (c0 == '/' && c1 == '*') {
      ScriptPtr += 2;
      while (ScriptPtr < ScriptEndPtr) {
        if (ScriptPtr[0] == '*' && ScriptPtr[1] == '/') { ScriptPtr += 2; break; }
        // check for new-line character
        if (changeFlags && *ScriptPtr == '\n') { ++Line; Crossed = true; }
        ++ScriptPtr;
      }
      continue;
    }
    // multiline nesting comment?
    if (c0 == '/' && c1 == '+') {
      int level = 1;
      ScriptPtr += 2;
      while (ScriptPtr < ScriptEndPtr) {
        if (ScriptPtr[0] == '/' && ScriptPtr[1] == '+') { ScriptPtr += 2; ++level; continue; }
        if (ScriptPtr[0] == '+' && ScriptPtr[1] == '/') {
          ScriptPtr += 2;
          if (--level == 0) break;
          continue;
        }
        // check for new-line character
        if (changeFlags && *ScriptPtr == '\n') { ++Line; Crossed = true; }
        ++ScriptPtr;
      }
      continue;
    }
    // not a comment, stop skipping
    break;
  }
  if (ScriptPtr >= ScriptEndPtr) {
    ScriptPtr = ScriptEndPtr;
    if (changeFlags) End = true;
  }
}


//==========================================================================
//
//  VScriptParser::SkipBlanks
//
//==========================================================================
void VScriptParser::SkipBlanks (bool changeFlags) noexcept {
  while (ScriptPtr < ScriptEndPtr) {
    SkipComments(changeFlags);
    if (*(const vuint8 *)ScriptPtr <= ' ') {
      if (changeFlags && *ScriptPtr == '\n') { ++Line; Crossed = true; }
      ++ScriptPtr;
      continue;
    }
    break;
  }
  if (ScriptPtr >= ScriptEndPtr) {
    ScriptPtr = ScriptEndPtr;
    if (changeFlags) End = true;
  }
}


//==========================================================================
//
//  VScriptParser::PeekChar
//
//==========================================================================
char VScriptParser::PeekOrSkipChar (bool doSkip) noexcept {
  char res = 0;
  char *oldSPtr = ScriptPtr;
  int oldLine = Line;
  bool oldCross = Crossed;
  bool oldEnd = End;
  SkipBlanks(true); // change flags
  if (ScriptPtr < ScriptEndPtr) {
    res = *ScriptPtr;
    if (doSkip) ++ScriptPtr;
  }
  if (!doSkip) {
    ScriptPtr = oldSPtr;
    Line = oldLine;
    Crossed = oldCross;
    End = oldEnd;
  }
  return res;
}


//==========================================================================
//
//  VScriptParser::ParseQuotedString
//
//  starting quite is eaten
//
//==========================================================================
void VScriptParser::ParseQuotedString (const char qch) noexcept {
  while (ScriptPtr < ScriptEndPtr) {
    char ch = *ScriptPtr++;
    // quote char?
    if (ch == qch) {
      if (!CMode) break;
      // double quote is string continuation in c mode
      if (*ScriptPtr != qch) break;
      // check next char
      if ((vuint8)ScriptPtr[1] < ' ') break;
      ++ScriptPtr; // skip quote char
      continue;
    }
    // newline?
    if (ch == '\n' || ch == '\r') {
      // convert from DOS format to UNIX format
      if (ch == '\r' && *ScriptPtr == '\n') ++ScriptPtr;
      ch = '\n';
      if (CMode) {
        /*if (String.length() == 0)*/ Error("Unterminated string constant");
      } else {
        String += ch;
      }
      ++Line;
      Crossed = true;
      continue;
    }
    // escape?
    if (Escape && ch == '\\' && ScriptPtr[0]) {
      const char c1 = *ScriptPtr++;
      // continuation?
      if (c1 == '\n' || c1 == '\r') {
        ++Line;
        Crossed = true;
        if (c1 == '\r' && *ScriptPtr == '\n') ++ScriptPtr;
        continue;
      }
      if (CMode) {
        // c-mode escape
        bool okescape = true;
        switch (c1) {
          case 'r': ch = '\r'; break;
          case 'n': ch = '\n'; break;
          case 'c': case 'C': ch = TEXT_COLOR_ESCAPE; break;
          case 'e': ch = '\x1b'; break;
          case '\t': ch = '\t'; break;
          case '"': case '\'': case ' ': case '\\': case '`': ch = c1; break;
          case 'x':
            if (VStr::digitInBase(*ScriptPtr, 16) < 0) {
              okescape = false;
            } else {
              int n0 = VStr::digitInBase(*ScriptPtr++, 16);
              int n1 = VStr::digitInBase(*ScriptPtr, 16);
              if (n1 >= 0) { n0 = n0*16+n1; ++ScriptPtr; }
              if (!n0) n0 = 32;
              ch = n0;
            }
            break;
          default: okescape = false; break;
        }
        String += ch;
        if (!okescape) String += c1;
      } else {
        // non-c-mode escape
        bool okescape = true;
        switch (c1) {
          case 'r': ch = '\r'; break;
          case 'n': ch = '\n'; break;
          case 'c': case 'C': ch = TEXT_COLOR_ESCAPE; break;
          case '\\': case '\"': case '\'': case '`': ch = c1; break;
          default: okescape = false; break;
        }
        String += ch;
        if (!okescape) {
          String += c1;
        } else {
          // escaped eol and eol == one eol
          if (ch == '\r' || ch == '\n') {
            if (*ScriptPtr == '\n') {
              ++Line;
              Crossed = true;
              ++ScriptPtr;
            } else if (*ScriptPtr == '\r' && ScriptPtr[1] == '\n') {
              ++Line;
              Crossed = true;
              ScriptPtr += 2;
            }
          }
        }
      }
      continue;
    }
    // normal char
    String += ch;
  }
}


//==========================================================================
//
//  skipNum
//
//==========================================================================
static const char *skipNum (const char *s, const char *end, int base) {
  if (base <= 0) base = 10;
  if (s >= end) return nullptr;
  if (VStr::digitInBase(*s, base) < 0) return nullptr;
  ++s;
  while (s < end) {
    if (*s != '_') {
      if (VStr::digitInBase(*s, base) < 0) return s;
    }
    ++s;
  }
  return s;
}


//==========================================================================
//
//  isValidNum
//
//  returns number end, or `nullptr` if definitely not a number
//
//==========================================================================
static const char *isValidNum (const char *s, const char *end) {
  if (s >= end) return nullptr;

  // sign
  if (*s == '+' || *s == '-') {
    if (++s >= end) return nullptr;
  }

  // hex number?
  if (*s == '0' && s+1 < end && s+2 < end && (s[1] == 'x' || s[1] == 'X')) {
    s += 2;
    return skipNum(s, end, 16);
  }

  if (*s != '.') {
    // integral part
    s = skipNum(s, end, 10);
    if (!s || s >= end) return s;
  } else {
    // no integral part, so fractional part should have at least one digit
    if (s+1 >= end) return nullptr;
    if (s[1] < '0' || s[1] > '9') return nullptr;
  }

  // fractional part
  if (*s == '.') {
    if (++s >= end) return s;
    if (*s >= '0' && *s <= '9') {
      s = skipNum(s, end, 10);
      if (!s || s >= end) return s;
    }
  }

  // exponent
  if (*s != 'e' && *s != 'E') return s;
  if (++s >= end) return nullptr;

  // sign
  if (*s == '+' || *s == '-') {
    if (++s >= end) return nullptr;
  }

  // exponent digits
  return skipNum(s, end, 10);
}


//==========================================================================
//
//  VScriptParser::ParseCMode
//
//==========================================================================
void VScriptParser::ParseCMode () noexcept {
  if (ScriptPtr+1 < ScriptEndPtr) {
    // special double-character eq-token?
    if (ScriptPtr[1] == '=' && CharClassifier::isC2Spec(ScriptPtr[0])) {
      String += *ScriptPtr++;
      String += *ScriptPtr++;
      return;
    }

    // special double-character token?
    if ((ScriptPtr[0] == '&' && ScriptPtr[1] == '&') ||
        (ScriptPtr[0] == '|' && ScriptPtr[1] == '|') ||
        (ScriptPtr[0] == '<' && ScriptPtr[1] == '<') ||
        (ScriptPtr[0] == '>' && ScriptPtr[1] == '>') ||
        (ScriptPtr[0] == ':' && ScriptPtr[1] == ':') ||
        (ScriptPtr[0] == '+' && ScriptPtr[1] == '+') ||
        (ScriptPtr[0] == '-' && ScriptPtr[1] == '-'))
    {
      if (ScriptPtr[0] == '>' && ScriptPtr[1] == '>' && ScriptPtr[2] == '>') String += *ScriptPtr++; // for `>>>`
      String += *ScriptPtr++;
      String += *ScriptPtr++;
      return;
    }
  }

  // number?
  // have to do it this way to omit underscores
  if (CharClassifier::isNumStart(ScriptPtr, AllowNumSign)) {
    const char *ee = isValidNum(ScriptPtr, ScriptEndPtr);
    if (ee && (ee >= ScriptEndPtr || CharClassifier::isCIdTerm(*ee))) {
      // looks like a valid number
      while (ScriptPtr < ee) {
        const char ch = *ScriptPtr++;
        if (ch == '_') continue;
        String += ch;
      }
      return;
    }
  }

  // special single-character token?
  if (CharClassifier::isCIdTerm(*ScriptPtr)) {
    String += *ScriptPtr++;
    return;
  }

  // normal identifier
  while (ScriptPtr < ScriptEndPtr) {
    const char ch = *ScriptPtr++;
    // eh... allow single quote inside an identifier
    if (ch == '\'' && !String.isEmpty() && ScriptPtr[0] && !CharClassifier::isCIdTerm(ScriptPtr[0])) {
      String += ch;
      continue;
    }
    if (CharClassifier::isCIdTerm(ch)) { --ScriptPtr; break; }
    if (ch != '_' || !EDGEMode) String += ch;
  }
}


//==========================================================================
//
//  VScriptParser::ParseNonCMode
//
//==========================================================================
void VScriptParser::ParseNonCMode () noexcept {
  // special single-character tokens
  if (CharClassifier::isNCIdTerm(*ScriptPtr)) {
    String += *ScriptPtr++;
    return;
  }

  // normal identifier
  while (ScriptPtr < ScriptEndPtr) {
    const char ch = *ScriptPtr++;
    // eh... allow single quote inside an identifier
    if (ch == '\'' && !String.isEmpty() && ScriptPtr[0] && !CharClassifier::isNCIdTerm(ScriptPtr[0])) {
      String += ch;
      continue;
    }
    if (CharClassifier::isNCIdTerm(ch)) { --ScriptPtr; break; }
    // check for comments
    if (ch == '/') {
      const char c1 = *ScriptPtr;
      if (c1 == '/' || c1 == '*' || c1 == '+') {
        --ScriptPtr;
        break;
      }
    }
    if (ch != '_' || !EDGEMode) String += ch;
  }
}


//==========================================================================
//
//  VScriptParser::GetString
//
//==========================================================================
bool VScriptParser::GetString () noexcept {
  TokStartPtr = ScriptPtr;
  TokStartLine = Line;

  Crossed = false;
  QuotedString = false;

  SkipBlanks(true); // change flags

  // check for end of script
  if (ScriptPtr >= ScriptEndPtr) {
    TokStartPtr = ScriptEndPtr;
    TokStartLine = Line;
    End = true;
    return false;
  }

  TokLine = Line;

  String.Clean();

  if (*ScriptPtr == '\"' || *ScriptPtr == '\'') {
    // quoted string
    const char qch = *ScriptPtr++;
    QuotedString = true;
    ParseQuotedString(qch);
  } else if (CMode) {
    ParseCMode();
  } else {
    ParseNonCMode();
  }

  return true;
}


//==========================================================================
//
//  VScriptParser::SkipLine
//
//==========================================================================
void VScriptParser::SkipLine () noexcept {
  Crossed = false;
  QuotedString = false;
  while (ScriptPtr < ScriptEndPtr) {
    SkipComments(true);
    if (Crossed || ScriptPtr >= ScriptEndPtr) break;
    if (*ScriptPtr++ == '\n') {
      ++Line;
      break;
    }
  }
  Crossed = false;

  if (ScriptPtr >= ScriptEndPtr) {
    ScriptPtr = ScriptEndPtr;
    End = true;
  }

  TokStartPtr = ScriptEndPtr;
  TokStartLine = Line;
}


//==========================================================================
//
//  VScriptParser::ExpectString
//
//==========================================================================
void VScriptParser::ExpectString () {
  if (!GetString()) Error("String expected");
}


//==========================================================================
//
//  VScriptParser::ExpectLoneChar
//
//==========================================================================
void VScriptParser::ExpectLoneChar () {
  UnGet();
  char ch = PeekOrSkipChar(true); // skip
  if (ch && ScriptPtr < ScriptEndPtr) {
    if (ch == '"' && ScriptPtr[0] == '\\' && ScriptPtr[1] && ScriptPtr[2] == '"') {
      ch = ScriptPtr[1];
      ScriptPtr += 3;
    } else if (ch == '"' && ScriptPtr[0] && ScriptPtr[1] == '"') {
      ch = ScriptPtr[0];
      ScriptPtr += 2;
    } else {
      // check for delimiter (space or comment)
      vuint8 nch = *(const vuint8 *)ScriptPtr;
      if (nch > ' ' && nch == '/' && ScriptEndPtr-ScriptPtr > 1) {
        if (ScriptPtr[1] != '/' && ScriptPtr[1] != '*' && ScriptPtr[1] != '+') ch = 0;
      }
    }
  }
  if (!ch) Error("Char expected");
  String.clear();
  String += ch;
}


//==========================================================================
//
//  ConvertStrToName8
//
//==========================================================================
static VName ConvertStrToName8 (VScriptParser *sc, VStr str, bool onlyWarn=true, VName *defval=nullptr) noexcept {
#if !defined(VCC_STANDALONE_EXECUTOR)
  // translate "$name" strings
  if (str.length() > 1 && str[0] == '$') {
    VStr qs = str.mid(1, str.length()-1).toLowerCase();
    if (GLanguage.HasTranslation(*qs)) {
      qs = *GLanguage[*qs];
      if (dbg_show_name_remap) GCon->Logf(NAME_Debug, "**** <%s>=<%s>\n", *str, *qs);
      str = qs;
    }
  }
#endif

  if (str.Length() > 8) {
#if !defined(VCC_STANDALONE_EXECUTOR)
    VStr oldstr = str;
#endif
         if (str.endsWithCI(".png")) str.chopRight(4);
    else if (str.endsWithCI(".jpg")) str.chopRight(4);
    else if (str.endsWithCI(".bmp")) str.chopRight(4);
    else if (str.endsWithCI(".pcx")) str.chopRight(4);
    else if (str.endsWithCI(".lmp")) str.chopRight(4);
    else if (str.endsWithCI(".jpeg")) str.chopRight(5);
#if !defined(VCC_STANDALONE_EXECUTOR)
    if (oldstr != str) {
      GCon->Logf(NAME_Warning, "%s: Name '%s' converted to '%s'", *sc->GetLoc().toStringNoCol(), *oldstr, *str);
    }
#endif
  }

  if (str.Length() > 8) {
#if !defined(VCC_STANDALONE_EXECUTOR)
    GCon->Logf(NAME_Warning, "%s: Name '%s' is too long", *sc->GetLoc().toStringNoCol(), *str);
#endif
    if (!onlyWarn) sc->Error(va("Name '%s' is too long", *str));
    if (defval) return *defval;
  }

  return VName(*str, VName::AddLower8);
}


//==========================================================================
//
//  VScriptParser::ExpectName8
//
//==========================================================================
void VScriptParser::ExpectName8 () {
  if (!GetString()) Error("Short name expected");
  Name8 = ConvertStrToName8(this, String, false); // error
}


//==========================================================================
//
//  VScriptParser::ExpectName8Warn
//
//==========================================================================
void VScriptParser::ExpectName8Warn () {
  if (!GetString()) Error("Short name expected");
  Name8 = ConvertStrToName8(this, String); // no error
}


//==========================================================================
//
//  VScriptParser::ExpectName8WarnOrFilePath
//
//==========================================================================
bool VScriptParser::ExpectName8WarnOrFilePath () {
  if (!GetString()) Error("Short name or path expected");
  String = String.fixSlashes();
  // hack for "vile/1", etc.
  const int slpos = String.indexOf('/');
  if (slpos < 0 || (String.length() <= 8 && slpos >= String.length()-2)) {
    Name8 = ConvertStrToName8(this, String); // no error
    return true;
  }
  Name = NAME_None;
  Name8 = NAME_None;
  return false;
}


//==========================================================================
//
//  VScriptParser::ExpectName8Def
//
//==========================================================================
void VScriptParser::ExpectName8Def (VName def) {
  if (!GetString()) Error("Short name expected");
  Name8 = ConvertStrToName8(this, String, true, &def); // no error
}


//==========================================================================
//
//  VScriptParser::ExpectName
//
//==========================================================================
void VScriptParser::ExpectName () {
  if (!GetString()) Error("Name expected");
  Name = VName(*String, VName::AddLower);
}


#if !defined(VCC_STANDALONE_EXECUTOR)
//==========================================================================
//
//  ExpectName
//
//  returns parsed color, either in string form, or r,g,b triplet
//
//==========================================================================
vuint32 VScriptParser::ExpectColor () {
  if (!GetString()) Error("Color expected");
  //vuint32 clr = M_LookupColorName(String);
  //if (clr) return clr;
  // hack to allow numbers like "008000"
  if (QuotedString || String.length() > 3) {
    //GCon->Logf("COLOR(0): <%s> (0x%08x)", *String, M_ParseColor(*String));
    return M_ParseColor(*String);
  }
  // should be r,g,b triplet
  UnGet();
  //ExpectNumber();
  if (!CheckNumber()) {
    ExpectString();
    //GCon->Logf("COLOR(1): <%s> (0x%08x)", *String, M_ParseColor(*String));
    return M_ParseColor(*String);
  }
  int r = clampToByte(Number);
  Check(",");
  ExpectNumber();
  int g = clampToByte(Number);
  Check(",");
  ExpectNumber();
  int b = clampToByte(Number);
  //GCon->Logf("COLOR: rgb(%d,%d,%d)", r, g, b);
  return 0xff000000u|(r<<16)|(g<<8)|b;
}
#endif


//==========================================================================
//
//  VScriptParser::Check
//
//==========================================================================
bool VScriptParser::Check (const char *str) noexcept {
  vassert(str);
  vassert(str[0]);
  if (GetString()) {
    if (!String.ICmp(str)) return true;
    UnGet();
  }
  return false;
}


//==========================================================================
//
//  VScriptParser::CheckStartsWith
//
//==========================================================================
bool VScriptParser::CheckStartsWith (const char *str) noexcept {
  vassert(str);
  vassert(str[0]);
  if (GetString()) {
    VStr s = VStr(str);
    if (String.length() < s.length()) { UnGet(); return false; }
    VStr s2 = String.left(s.length());
    if (s2.ICmp(s) != 0) { UnGet(); return false; }
    return true;
  }
  return false;
}


//==========================================================================
//
//  VScriptParser::Expect
//
//==========================================================================
void VScriptParser::Expect (const char *name) {
  vassert(name);
  vassert(name[0]);
  if (!GetString()) Error(va("`%s` expected", name));
  if (String.ICmp(name)) Error(va("Bad syntax, `%s` expected, got `%s`.", name, *String.quote()));
}


//==========================================================================
//
//  VScriptParser::CheckQuotedString
//
//==========================================================================
bool VScriptParser::CheckQuotedString () noexcept {
  if (!GetString()) return false;
  if (!QuotedString) {
    UnGet();
    return false;
  }
  return true;
}


//==========================================================================
//
//  VScriptParser::CheckIdentifier
//
//==========================================================================
bool VScriptParser::CheckIdentifier () noexcept {
  if (!GetString()) return false;

  // quoted strings are not valid identifiers
  if (QuotedString) {
    UnGet();
    return false;
  }

  if (String.Length() < 1) {
    UnGet();
    return false;
  }

  // identifier must start with a letter, a number or an underscore
  char c = String[0];
  if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) {
    UnGet();
    return false;
  }

  // it must be followed by letters, numbers and underscores
  for (int i = 1; i < String.Length(); ++i) {
    c = String[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) {
      UnGet();
      return false;
    }
  }
  return true;
}


//==========================================================================
//
//  VScriptParser::ExpectIdentifier
//
//==========================================================================
void VScriptParser::ExpectIdentifier () {
  if (!CheckIdentifier()) Error(va("Identifier expected, got `%s`.", *String.quote()));
}


//==========================================================================
//
//  NormalizeFuckedGozzoNumber
//
//==========================================================================
static VStr NormalizeFuckedGozzoNumber (VStr String) noexcept {
  VStr str = String.xstrip();
  if (str.length() && strchr("lfLF", str[str.length()-1])) {
    str.chopRight(1);
    str = String.xstrip();
  }
  return str;
}


//==========================================================================
//
//  VScriptParser::CheckNumber
//
//==========================================================================
bool VScriptParser::CheckNumber () noexcept {
  if (GetString()) {
    VStr str = NormalizeFuckedGozzoNumber(String);
    if (str.length() > 0) {
      if (str.convertInt(&Number)) {
        //GCon->Logf("VScriptParser::CheckNumber: <%s> is %d", *String, Number);
        return true;
      }
    }
    UnGet();
  }
  return false;
}


//==========================================================================
//
//  VScriptParser::ExpectNumber
//
//==========================================================================
void VScriptParser::ExpectNumber (bool allowFloat, bool truncFloat) {
  if (!GetString()) {
    Error("Integer expected");
  } else {
    VStr str = NormalizeFuckedGozzoNumber(String);
    if (str.length() > 0) {
      char *stopper;
      Number = strtol(*str, &stopper, 0);
      if (*stopper != 0) {
        if (allowFloat && *stopper == '.') {
          if (truncFloat) {
            Message(va("Bad numeric constant \"%s\" (integer expected; truncated to %d).", *String, (int)Number));
          } else {
            if (stopper[1] >= '5') ++Number;
            if (Number == 0) Number = 1; // just in case
            Message(va("Bad numeric constant \"%s\" (integer expected; rounded to %d).", *String, (int)Number));
          }
          //fprintf(stderr, "%d\n", (int)Number);
          //Error(va("Bad numeric constant \"%s\".", *String));
        } else {
          Error(va("Bad numeric constant \"%s\".", *String));
        }
      }
    } else {
      Error("Integer expected");
    }
  }
}


//==========================================================================
//
//  VScriptParser::CheckNumberWithSign
//
//==========================================================================
bool VScriptParser::CheckNumberWithSign () noexcept {
  char *savedPtr = TokStartPtr;
  int savedLine = TokStartLine;
  bool neg = Check("-");
  bool pos = !neg && Check("+");
  if (neg || pos) {
    if (CheckNumber()) {
      if (neg) Number = -Number;
      return true;
    }
    // unget minus
    ScriptPtr = savedPtr;
    Line = savedLine;
    return false;
  } else {
    return CheckNumber();
  }
}


//==========================================================================
//
//  VScriptParser::ExpectNumberWithSign
//
//==========================================================================
void VScriptParser::ExpectNumberWithSign () {
  if (Check("-")) {
    ExpectNumber();
    Number = -Number;
  } else {
    ExpectNumber();
  }
}


//==========================================================================
//
//  VScriptParser::parseFloat
//
//  uses `String`
//
//==========================================================================
float VScriptParser::parseFloat (bool silent, bool *error) {
  bool etmp = false;
  if (!error) error = &etmp;
  *error = false;

  VStr str = NormalizeFuckedGozzoNumber(String);
  if (str.length() == 0) {
    *error = true;
    if (!silent) Error("Float expected");
    return 0.0f;
  }

  //FIXME: detect when we want to use a really big number
  VStr sl = str.toLowerCase();
  if (sl.StartsWith("0x") || sl.StartsWith("-0x") || sl.StartsWith("+0x")) {
    const char *s = *str;
    bool neg = false;
    switch (*s) {
      case '-':
        neg = true;
        /* fallthrough */
      case '+':
        ++s;
        break;
    }
    s += 2; // skip "0x"

    float val = 0.0f;
    bool skip = false;
    while (*s) {
      int dg = VStr::digitInBase(*s++, 16);
      if (dg < 0) {
        *error = true;
        if (!silent) Error(va("Bad floating point constant \"%s\"", *String));
        return 0.0f;
      }
      if (!skip) {
        val = val*16.0f+(float)dg;
        if (val > 1.0e12f) {
          if (!silent) GLog.Logf(NAME_Warning, "%s: DON'T BE IDIOTS, THIS IS TOO MUCH FOR A FLOAT: '%s'", *GetLoc().toStringNoCol(), *String);
          skip = true;
        }
      }
    }
    if (!silent) GLog.Logf(NAME_Warning, "%s: hex value '%s' for floating constant", *GetLoc().toStringNoCol(), *String);
    if (neg) val = -val;
    return val;
  } else {
    float ff = 0.0f;
    if (!str.convertFloat(&ff)) {
      // mo...dders from LCA loves numbers like "90000000000000000000000000000000000000000000000000"
      const char *s = *str;
      bool neg = false;
      switch (*s) {
        case '-':
          neg = true;
          /* fallthrough */
        case '+':
          ++s;
          break;
      }
      while (*s >= '0' && *s <= '9') ++s;
      if (*s) {
        *error = true;
        if (!silent) Error(va("Bad floating point constant \"%s\".", *String));
        return 0.0f;
      }
      if (!silent) GLog.Logf(NAME_Warning, "%s: DON'T BE IDIOTS, THIS IS TOO MUCH FOR A FLOAT: '%s'", *GetLoc().toStringNoCol(), *String);
      ff = 1.0e12f;
      if (neg) ff = -ff;
    } else {
      if (isNaNF(ff)) {
        *error = true;
        if (!silent) Error(va("Bad floating point constant \"%s\".", *String));
        return 0.0f;
      }
      if (isInfF(ff)) {
        if (!silent) GLog.Logf(NAME_Warning, "%s: WUTAFUCK IS THIS SO-CALLED FLOAT!? -- '%s'", *GetLoc().toStringNoCol(), *String);
        ff = (isPositiveF(ff) ? 1.0e12f : -1.0e12f);
      } else if (ff < -1.0e12f || ff > +1.0e12f) {
        if (!silent) GLog.Logf(NAME_Warning, "%s: DON'T BE IDIOTS, THIS IS TOO MUCH FOR A FLOAT: '%s'", *GetLoc().toStringNoCol(), *String);
        if (ff < -1.0e12f) ff = -1.0e12f; else ff = 1.0e12f;
      }
    }
    return ff;
  }
}


//==========================================================================
//
//  VScriptParser::CheckFloat
//
//==========================================================================
bool VScriptParser::CheckFloat () noexcept {
  if (!GetString()) return false;
  if (!CMode || !AllowNumSign) {
    if (String.length() && (String[0] == '+' || String[0] == '-')) {
      UnGet();
      return false;
    }
  }
  bool error = false;
  const float ff = parseFloat(true, &error);
  if (error) UnGet(); else Float = ff;
  return !error;
}


//==========================================================================
//
//  VScriptParser::ExpectFloat
//
//==========================================================================
void VScriptParser::ExpectFloat () {
  if (!GetString()) Error("Float expected");
  bool error = false;
  const float ff = parseFloat(false, &error);
  if (error) Error("Float expected");
  Float = ff;
}


//==========================================================================
//
//  VScriptParser::CheckFloatWithSign
//
//==========================================================================
bool VScriptParser::CheckFloatWithSign () noexcept {
  char *savedPtr = TokStartPtr;
  int savedLine = TokStartLine;
  bool neg = Check("-");
  bool pos = !neg && Check("+");
  if (neg || pos) {
    if (CheckFloat()) {
      if (neg) Float = -Float;
      return true;
    }
    // unget minus
    ScriptPtr = savedPtr;
    Line = savedLine;
    return false;
  } else {
    return CheckFloat();
  }
}


//==========================================================================
//
//  VScriptParser::ExpectFloatWithSign
//
//==========================================================================
void VScriptParser::ExpectFloatWithSign () {
  if (Check("-")) {
    ExpectFloat();
    Float = -Float;
  } else {
    Check("+");
    ExpectFloat();
  }
}


//==========================================================================
//
//  VScriptParser::ResetQuoted
//
//==========================================================================
void VScriptParser::ResetQuoted () noexcept {
  /*if (TokStartPtr != ScriptPtr)*/ QuotedString = false;
}


//==========================================================================
//
//  VScriptParser::ResetCrossed
//
//==========================================================================
void VScriptParser::ResetCrossed () noexcept {
  /*if (TokStartPtr != ScriptPtr)*/ Crossed = false;
}


//==========================================================================
//
//  VScriptParser::UnGet
//
//  Assumes that `GetString()` was called at least once
//
//==========================================================================
void VScriptParser::UnGet () noexcept {
  //AlreadyGot = true;
  ScriptPtr = TokStartPtr;
  Line = TokStartLine;
  //Crossed = false;
}


//==========================================================================
//
//  VScriptParser::SkipBracketed
//
//==========================================================================
void VScriptParser::SkipBracketed (bool bracketEaten) noexcept {
  if (!bracketEaten) {
    for (;;) {
      ResetQuoted();
      if (!GetString()) return;
      if (QuotedString) continue;
      if (String.length() == 1 && String[0] == '{') {
        break;
      }
    }
  }
  int level = 1;
  for (;;) {
    ResetQuoted();
    if (!GetString()) break;
    if (QuotedString) continue;
    if (String.length() == 1) {
      if (String[0] == '{') {
        ++level;
      } else if (String[0] == '}') {
        if (--level == 0) return;
      }
    }
  }
}


//==========================================================================
//
//  VScriptParser::Message
//
//==========================================================================
void VScriptParser::Message (const char *message) {
  const char *Msg = (message ? message : "Bad syntax.");
#if !defined(VCC_STANDALONE_EXECUTOR)
  GCon->Logf(NAME_Warning, "%s:%d: %s", *ScriptName, TokLine, Msg);
#else
  GLog.WriteLine(NAME_Warning, "%s:%d: %s", *ScriptName, TokLine, Msg);
#endif
}


//==========================================================================
//
//  VScriptParser::DebugMessage
//
//==========================================================================
void VScriptParser::DebugMessage (const char *message) {
  const char *Msg = (message ? message : "Bad syntax.");
#if !defined(VCC_STANDALONE_EXECUTOR)
  GCon->Logf(NAME_Debug, "%s:%d: %s", *ScriptName, TokLine, Msg);
#else
  GLog.WriteLine(NAME_Debug, "%s:%d: %s", *ScriptName, TokLine, Msg);
#endif
}


//==========================================================================
//
//  VScriptParser::MessageErr
//
//==========================================================================
void VScriptParser::MessageErr (const char *message) {
  const char *Msg = (message ? message : "Bad syntax.");
#if !defined(VCC_STANDALONE_EXECUTOR)
  GCon->Logf(NAME_Error, "%s:%d: %s", *ScriptName, TokLine, Msg);
#else
  GLog.WriteLine(NAME_Error, "%s:%d: %s", *ScriptName, TokLine, Msg);
#endif
}


//==========================================================================
//
//  VScriptParser::Error
//
//==========================================================================
void VScriptParser::Error (const char *message) {
  const char *Msg = (message ? message : "Bad syntax.");
  Sys_Error("Script error at %s:%d: %s", *ScriptName, TokLine, Msg);
}


//==========================================================================
//
//  VScriptParser::HostError
//
//==========================================================================
#if !defined(VCC_STANDALONE_EXECUTOR)
void VScriptParser::HostError (const char *message) {
  const char *Msg = (message ? message : "Bad syntax.");
  Host_Error("Script error at %s:%d: %s", *ScriptName, TokLine, Msg);
}
#endif


//==========================================================================
//
//  VScriptParser::Messagef
//
//==========================================================================
__attribute__((format(printf, 2, 3))) void VScriptParser::Messagef (const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  const char *s = vavarg(fmt, ap);
  va_end(ap);
  Message(s);
}


//==========================================================================
//
//  VScriptParser::DebugMessagef
//
//==========================================================================
__attribute__((format(printf, 2, 3))) void VScriptParser::DebugMessagef (const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  const char *s = vavarg(fmt, ap);
  va_end(ap);
  DebugMessage(s);
}


//==========================================================================
//
//  VScriptParser::MessageErrf
//
//==========================================================================
__attribute__((format(printf, 2, 3))) void VScriptParser::MessageErrf (const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  const char *s = vavarg(fmt, ap);
  va_end(ap);
  MessageErr(s);
}


//==========================================================================
//
//  VScriptParser::Errorf
//
//==========================================================================
__attribute__((format(printf, 2, 3))) void VScriptParser::Errorf (const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  const char *s = vavarg(fmt, ap);
  va_end(ap);
  Error(s);
}


#if !defined(VCC_STANDALONE_EXECUTOR)
//==========================================================================
//
//  VScriptParser::HostErrorf
//
//==========================================================================
__attribute__((format(printf, 2, 3))) void VScriptParser::HostErrorf (const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  const char *s = vavarg(fmt, ap);
  va_end(ap);
  HostError(s);
}
#endif


//==========================================================================
//
//  VScriptParser::GetVCLoc
//
//==========================================================================
TLocation VScriptParser::GetVCLoc () const noexcept {
  if (SrcIdx == -1) SrcIdx = TLocation::AddSourceFile(ScriptName);
  return TLocation(SrcIdx, TokLine, 1);
}


#if !defined(VCC_STANDALONE_EXECUTOR)
//==========================================================================
//
//  VScriptParser::FindRelativeIncludeLump
//
//==========================================================================
int VScriptParser::FindRelativeIncludeLump (int srclump, VStr fname) noexcept {
  if (fname.isEmpty()) return -1;
  if (srclump < 0 || fname[0] == '/') return -1;

  VStr fn = W_RealLumpName(srclump);
  if (fn.indexOf('/') < 0) return -1;
  fn = fn.ExtractFilePath();
  fname = fn.appendPath(fname);

  return W_CheckNumForFileNameInSameFile(srclump, fname);
}


//==========================================================================
//
//  VScriptParser::FindIncludeLump
//
//==========================================================================
int VScriptParser::FindIncludeLump (int srclump, VStr fname) noexcept {
  if (fname.isEmpty()) return -1;
  //GLog.Logf(NAME_Debug, "***FindIncludeLumpEx: srclump=%d; fname='%s' (srcfname='%s')", srclump, *fname, *W_FullLumpName(srclump));

  int Lump = (srclump >= 0 ? W_CheckNumForFileNameInSameFile(srclump, fname) : W_CheckNumForFileName(fname));
  if (Lump >= 0) return Lump;

  fname = fname.fixSlashes();
  //GLog.Logf(NAME_Debug, "   fname='%s'", *fname);

  Lump = FindRelativeIncludeLump(srclump, fname);
  if (Lump >= 0) return Lump;

  //GLog.Logf(NAME_Debug, "   XXX: fname='%s'", *fname);
  // check WAD lump only if it's no longer than 8 characters and has no path separator
  if (/*(srclump == -1 || W_IsWADLump(srclump)) &&*/ fname.indexOf('/') < 0) {
    VStr noext = fname.stripExtension();
    //GLog.Logf(NAME_Debug, "  NOEXT: %s", *noext);
    if (!noext.isEmpty() && noext.length() <= 8) {
      VName nn(*fname, VName::FindLower8);
      if (nn != NAME_None) {
        //GLog.Logf(NAME_Debug, "    NN: %s", *nn);
        Lump = (srclump < 0 ? W_CheckNumForName(nn) : W_CheckNumForNameInFile(nn, W_LumpFile(srclump)));
        if (Lump >= 0) return Lump;
      }
    }
  }

  return Lump;
}
#endif



// ////////////////////////////////////////////////////////////////////////// //
//  VScriptsParser
// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  VScriptsParser::Destroy
//
//==========================================================================
void VScriptsParser::Destroy () {
  if (Int) {
    delete Int;
    Int = nullptr;
  }
  Super::Destroy();
}


//==========================================================================
//
//  VScriptsParser::CheckInterface
//
//==========================================================================
void VScriptsParser::CheckInterface () {
  if (!Int) Sys_Error("No script currently open");
}



//==========================================================================
//
//  VScriptsParser natives
//
//==========================================================================

#if !defined(VCC_STANDALONE_EXECUTOR)
IMPLEMENT_FUNCTION(VScriptsParser, OpenLumpName) {
  VName Name;
  vobjGetParamSelf(Name);
  if (Self->Int) {
    delete Self->Int;
    Self->Int = nullptr;
  }
  int Lump = W_GetNumForName(Name);
  if (Lump < 0) Sys_Error("cannot open non-existing lump '%s'", *Name);
  Self->Int = VScriptParser::NewWithLump(Lump);
}

IMPLEMENT_FUNCTION(VScriptsParser, OpenLumpIndex) {
  int lump;
  vobjGetParamSelf(lump);
  if (Self->Int) {
    delete Self->Int;
    Self->Int = nullptr;
  }
  if (lump < 0) Sys_Error("cannot open non-existing lump");
  VStream *st = W_CreateLumpReaderNum(lump);
  if (!st) Sys_Error("cannot open non-existing lump");
  Self->Int = new VScriptParser(W_FullLumpName(lump), st, lump);
}
#endif

IMPLEMENT_FUNCTION(VScriptsParser, OpenLumpFullName) {
  VStr Name;
  vobjGetParamSelf(Name);
#if !defined(VCC_STANDALONE_EXECUTOR)
  if (Self->Int) {
    delete Self->Int;
    Self->Int = nullptr;
  }
  // first try disk file
  if (FL_IsSafeDiskFileName(Name)) {
    VStr diskName = FL_GetUserDataDir(false)+"/"+Name;
    VStream *st = FL_OpenSysFileRead(*diskName);
    if (st) {
      bool ok = true;
      VStr s;
      if (st->TotalSize() > 0) {
        s.setLength(st->TotalSize());
        st->Serialise(s.getMutableCStr(), s.length());
        ok = !st->IsError();
      }
      VStream::Destroy(st);
      if (!ok) Sys_Error("cannot read file '%s'", *Name);
      Self->Int = new VScriptParser(*Name, *s);
      return;
    }
  }
  if (Name.length() >= 2 && Name[0] == '/' && Name[1] == '/') {
    VStream *strm = FL_OpenFileRead(Name);
    if (!strm) Sys_Error("file '%s' not found", *Name);
    Self->Int = new VScriptParser(*Name, strm);
  } else {
    int num = W_GetNumForFileName(Name);
    //int num = W_IterateFile(-1, *Name);
    if (num < 0) Sys_Error("file '%s' not found", *Name);
    Self->Int = VScriptParser::NewWithLump(num);
  }
#elif defined(VCC_STANDALONE_EXECUTOR)
  if (Self->Int) {
    delete Self->Int;
    Self->Int = nullptr;
  }
  VStream *st = fsysOpenFile(*Name);
  if (!st) Sys_Error("file '%s' not found", *Name);
  bool ok = true;
  VStr s;
  if (st->TotalSize() > 0) {
    s.setLength(st->TotalSize());
    st->Serialise(s.getMutableCStr(), s.length());
    ok = !st->IsError();
  }
  VStream::Destroy(st);
  if (!ok) Sys_Error("cannot read file '%s'", *Name);
  Self->Int = new VScriptParser(*Name, *s);
#else
  Sys_Error("file '%s' not found", *Name);
#endif
}

IMPLEMENT_FUNCTION(VScriptsParser, OpenString) {
  VName Name;
  VStr s;
  vobjGetParamSelf(Name, s);
  if (Self->Int) {
    delete Self->Int;
    Self->Int = nullptr;
  }
  Self->Int = new VScriptParser(*Name, *s);
}

IMPLEMENT_FUNCTION(VScriptsParser, get_String) {
  vobjGetParamSelf();
  Self->CheckInterface();
  RET_STR(Self->Int->String);
}

IMPLEMENT_FUNCTION(VScriptsParser, get_Number) {
  vobjGetParamSelf();
  Self->CheckInterface();
  RET_INT(Self->Int->Number);
}

IMPLEMENT_FUNCTION(VScriptsParser, get_Float) {
  vobjGetParamSelf();
  Self->CheckInterface();
  RET_FLOAT(Self->Int->Float);
}

IMPLEMENT_FUNCTION(VScriptsParser, get_Crossed) {
  vobjGetParamSelf();
  Self->CheckInterface();
  RET_BOOL(Self->Int->Crossed);
}

IMPLEMENT_FUNCTION(VScriptsParser, get_Quoted) {
  vobjGetParamSelf();
  Self->CheckInterface();
  RET_BOOL(Self->Int->QuotedString);
}

IMPLEMENT_FUNCTION(VScriptsParser, get_SourceLump) {
  vobjGetParamSelf();
  Self->CheckInterface();
  RET_BOOL(Self->Int->SourceLump);
}

IMPLEMENT_FUNCTION(VScriptsParser, set_SourceLump) {
  vuint32 value;
  vobjGetParamSelf(value);
  Self->CheckInterface();
  Self->Int->SourceLump = value;
}

IMPLEMENT_FUNCTION(VScriptsParser, get_Escape) {
  vobjGetParamSelf();
  Self->CheckInterface();
  RET_BOOL(Self->Int->IsEscape());
}

IMPLEMENT_FUNCTION(VScriptsParser, set_Escape) {
  bool value;
  vobjGetParamSelf(value);
  Self->CheckInterface();
  Self->Int->SetEscape(value);
}

IMPLEMENT_FUNCTION(VScriptsParser, get_CMode) {
  vobjGetParamSelf();
  Self->CheckInterface();
  RET_BOOL(Self->Int->IsCMode());
}

IMPLEMENT_FUNCTION(VScriptsParser, set_CMode) {
  bool value;
  vobjGetParamSelf(value);
  Self->CheckInterface();
  Self->Int->SetCMode(value);
}

IMPLEMENT_FUNCTION(VScriptsParser, get_AllowNumSign) {
  vobjGetParamSelf();
  Self->CheckInterface();
  RET_BOOL(Self->Int->IsAllowNumSign());
}

IMPLEMENT_FUNCTION(VScriptsParser, set_AllowNumSign) {
  bool value;
  vobjGetParamSelf(value);
  Self->CheckInterface();
  Self->Int->SetAllowNumSign(value);
}

IMPLEMENT_FUNCTION(VScriptsParser, get_EDGEMode) {
  vobjGetParamSelf();
  Self->CheckInterface();
  RET_BOOL(Self->Int->IsEDGEMode());
}

IMPLEMENT_FUNCTION(VScriptsParser, set_EDGEMode) {
  bool value;
  vobjGetParamSelf(value);
  Self->CheckInterface();
  Self->Int->SetEDGEMode(value);
}

IMPLEMENT_FUNCTION(VScriptsParser, get_EndOfText) {
  vobjGetParamSelf();
  Self->CheckInterface();
  RET_BOOL(Self->Int->End);
}

IMPLEMENT_FUNCTION(VScriptsParser, IsText) {
  vobjGetParamSelf();
  Self->CheckInterface();
  RET_BOOL(Self->Int->IsText());
}

IMPLEMENT_FUNCTION(VScriptsParser, IsAtEol) {
  vobjGetParamSelf();
  Self->CheckInterface();
  RET_BOOL(Self->Int->IsAtEol());
}

IMPLEMENT_FUNCTION(VScriptsParser, IsCMode) {
  vobjGetParamSelf();
  Self->CheckInterface();
  RET_BOOL(Self->Int->IsCMode());
}

IMPLEMENT_FUNCTION(VScriptsParser, SetCMode) {
  bool On;
  vobjGetParamSelf(On);
  Self->CheckInterface();
  Self->Int->SetCMode(On);
}

IMPLEMENT_FUNCTION(VScriptsParser, IsAllowNumSign) {
  vobjGetParamSelf();
  Self->CheckInterface();
  RET_BOOL(Self->Int->IsAllowNumSign());
}

IMPLEMENT_FUNCTION(VScriptsParser, SetAllowNumSign) {
  bool On;
  vobjGetParamSelf(On);
  Self->CheckInterface();
  Self->Int->SetAllowNumSign(On);
}

IMPLEMENT_FUNCTION(VScriptsParser, IsEscape) {
  vobjGetParamSelf();
  Self->CheckInterface();
  RET_BOOL(Self->Int->IsEscape());
}

IMPLEMENT_FUNCTION(VScriptsParser, SetEscape) {
  bool On;
  vobjGetParamSelf(On);
  Self->CheckInterface();
  Self->Int->SetEscape(On);
}

IMPLEMENT_FUNCTION(VScriptsParser, AtEnd) {
  vobjGetParamSelf();
  Self->CheckInterface();
  RET_BOOL(Self->Int->AtEnd());
}

IMPLEMENT_FUNCTION(VScriptsParser, GetString) {
  vobjGetParamSelf();
  Self->CheckInterface();
  RET_BOOL(Self->Int->GetString());
}

IMPLEMENT_FUNCTION(VScriptsParser, ExpectLoneChar) {
  vobjGetParamSelf();
  Self->CheckInterface();
  Self->Int->ExpectLoneChar();
}

#if !defined(VCC_STANDALONE_EXECUTOR)
IMPLEMENT_FUNCTION(VScriptsParser, ExpectColor) {
  vobjGetParamSelf();
  Self->CheckInterface();
  RET_INT(Self->Int->ExpectColor());
}
#endif

IMPLEMENT_FUNCTION(VScriptsParser, ExpectString) {
  vobjGetParamSelf();
  Self->CheckInterface();
  Self->Int->ExpectString();
}

IMPLEMENT_FUNCTION(VScriptsParser, Check) {
  VStr Text;
  vobjGetParamSelf(Text);
  Self->CheckInterface();
  RET_BOOL(Self->Int->Check(*Text));
}

IMPLEMENT_FUNCTION(VScriptsParser, CheckStartsWith) {
  VStr Text;
  vobjGetParamSelf(Text);
  Self->CheckInterface();
  RET_BOOL(Self->Int->CheckStartsWith(*Text));
}

IMPLEMENT_FUNCTION(VScriptsParser, Expect) {
  VStr Text;
  vobjGetParamSelf(Text);
  Self->CheckInterface();
  Self->Int->Expect(*Text);
}

IMPLEMENT_FUNCTION(VScriptsParser, CheckIdentifier) {
  vobjGetParamSelf();
  Self->CheckInterface();
  RET_BOOL(Self->Int->CheckIdentifier());
}

IMPLEMENT_FUNCTION(VScriptsParser, ExpectIdentifier) {
  vobjGetParamSelf();
  Self->CheckInterface();
  Self->Int->ExpectIdentifier();
}

IMPLEMENT_FUNCTION(VScriptsParser, CheckNumber) {
  VOptParamBool withSign(false);
  vobjGetParamSelf(withSign);
  Self->CheckInterface();
  RET_BOOL(withSign ? Self->Int->CheckNumberWithSign() : Self->Int->CheckNumber());
}

IMPLEMENT_FUNCTION(VScriptsParser, ExpectNumber) {
  VOptParamBool withSign(false);
  vobjGetParamSelf(withSign);
  Self->CheckInterface();
  if (withSign) Self->Int->ExpectNumberWithSign(); else Self->Int->ExpectNumber();
}

IMPLEMENT_FUNCTION(VScriptsParser, CheckFloat) {
  VOptParamBool withSign(false);
  vobjGetParamSelf(withSign);
  Self->CheckInterface();
  RET_BOOL(withSign ? Self->Int->CheckFloatWithSign() : Self->Int->CheckFloat());
}

IMPLEMENT_FUNCTION(VScriptsParser, ExpectFloat) {
  VOptParamBool withSign(false);
  vobjGetParamSelf(withSign);
  Self->CheckInterface();
  if (withSign) Self->Int->ExpectFloatWithSign(); else Self->Int->ExpectFloat();
}

IMPLEMENT_FUNCTION(VScriptsParser, ResetQuoted) {
  vobjGetParamSelf();
  Self->CheckInterface();
  Self->Int->ResetQuoted();
}

IMPLEMENT_FUNCTION(VScriptsParser, ResetCrossed) {
  vobjGetParamSelf();
  Self->CheckInterface();
  Self->Int->ResetCrossed();
}

IMPLEMENT_FUNCTION(VScriptsParser, SkipBracketed) {
  VOptParamBool bracketEaten(false);
  vobjGetParamSelf(bracketEaten);
  Self->CheckInterface();
  Self->Int->SkipBracketed(bracketEaten);
}

IMPLEMENT_FUNCTION(VScriptsParser, SkipLine) {
  vobjGetParamSelf();
  Self->CheckInterface();
  Self->Int->SkipLine();
}

IMPLEMENT_FUNCTION(VScriptsParser, UnGet) {
  vobjGetParamSelf();
  Self->CheckInterface();
  Self->Int->UnGet();
}

IMPLEMENT_FUNCTION(VScriptsParser, FileName) {
  vobjGetParamSelf();
  Self->CheckInterface();
  RET_STR(Self->Int->GetScriptName());
}

IMPLEMENT_FUNCTION(VScriptsParser, CurrLine) {
  vobjGetParamSelf();
  Self->CheckInterface();
  RET_INT(Self->Int->Line);
}

IMPLEMENT_FUNCTION(VScriptsParser, TokenLine) {
  vobjGetParamSelf();
  Self->CheckInterface();
  RET_INT(Self->Int->TokLine);
}

IMPLEMENT_FUNCTION(VScriptsParser, ScriptError) {
  VStr Msg = PF_FormatString();
  vobjGetParamSelf();
  Self->CheckInterface();
  Self->Int->Error(*Msg);
}

IMPLEMENT_FUNCTION(VScriptsParser, ScriptMessage) {
  VStr Msg = PF_FormatString();
  vobjGetParamSelf();
  Self->CheckInterface();
  Self->Int->Message(*Msg);
}

IMPLEMENT_FUNCTION(VScriptsParser, SavePos) {
  VScriptSavedPos *pos;
  vobjGetParamSelf(pos);
  Self->CheckInterface();
  if (pos) *pos = Self->Int->SavePos();
}

IMPLEMENT_FUNCTION(VScriptsParser, RestorePos) {
  VScriptSavedPos *pos;
  vobjGetParamSelf(pos);
  Self->CheckInterface();
  if (pos) Self->Int->RestorePos(*pos);
}

#if !defined(VCC_STANDALONE_EXECUTOR)
//static native final int FindRelativeIncludeLump (int srclump, string fname);
IMPLEMENT_FUNCTION(VScriptsParser, FindRelativeIncludeLump) {
  int srclump;
  VStr fname;
  vobjGetParamSelf(srclump, fname);
  RET_INT(VScriptParser::FindRelativeIncludeLump(srclump, fname));
}

//static native final int FindIncludeLump (int srclump, string fname);
IMPLEMENT_FUNCTION(VScriptsParser, FindIncludeLump) {
  int srclump;
  VStr fname;
  vobjGetParamSelf(srclump, fname);
  RET_INT(VScriptParser::FindIncludeLump(srclump, fname));
}
#endif
