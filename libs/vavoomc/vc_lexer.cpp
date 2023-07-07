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
#include "vc_local.h"

//#define VC_LEXER_DUMP_COLLECTED_NUMBERS
#define VC_LEXER_USE_TOKEN_HASHTABLE


// ////////////////////////////////////////////////////////////////////////// //
const char *VLexer::TokenNames[] = {
#define VC_LEXER_DEFTOKEN(name,str)  str,
#include "vc_lexer_tokens.h"
#undef VC_LEXER_DEFTOKEN
  nullptr
};

char VLexer::ASCIIToChrCode[256];
vuint8 VLexer::ASCIIToHexDigit[256];
bool VLexer::tablesInited = false;

static unsigned tkCanBeToken[256];
static unsigned tkCanBeSpecialToken[256];
static unsigned tkMinLen;
static unsigned tkMaxLen;
static bool tkHashInited = false;
#ifdef VC_LEXER_USE_TOKEN_HASHTABLE
# define VC_LEXER_HTABLE_SIZE  (1024u)
struct THEntry {
  unsigned slen;
  unsigned tidx;
  unsigned next;
  const char *str;
};
static THEntry tkHashBuckets[TK_TotalTokenCount+64]; // index 0 is unused
#endif


//==========================================================================
//
//  AddTokenToHashTable
//
//==========================================================================
static void AddTokenToHashTable (const char *s, unsigned tidx, unsigned &bkidx) noexcept {
  if (!s || !s[0]) return;
  const unsigned slen = (unsigned)strlen(s);
  if (tkMinLen == 0 || tkMinLen > slen) tkMinLen = slen;
  if (tkMaxLen < slen) tkMaxLen = slen;
#ifdef VC_LEXER_USE_TOKEN_HASHTABLE
  const unsigned htidx = (vuint8)s[0];
  THEntry &e = tkHashBuckets[bkidx++];
  e.slen = slen;
  e.tidx = tidx;
  e.next = tkCanBeToken[htidx];
  e.str = s;
  tkCanBeToken[htidx] = bkidx-1u;
#else
  tkCanBeToken[(vuint8)s[0]] = 1;
#endif
}


//==========================================================================
//
//  InitTokenHashTable
//
//==========================================================================
static void InitTokenHashTable () noexcept {
  tkHashInited = true;
  memset(tkCanBeToken, 0, sizeof(tkCanBeToken));
  memset(tkCanBeSpecialToken, 0, sizeof(tkCanBeSpecialToken));
#ifdef VC_LEXER_USE_TOKEN_HASHTABLE
  memset(tkHashBuckets, 0, sizeof(tkHashBuckets));
#endif
  unsigned bkidx = 1u;
  tkMinLen = tkMaxLen = 0;
  for (unsigned tidx = TK_Abstract; tidx < /*TK_URShiftAssign*/TK_TotalTokenCount; ++tidx) {
    const char *s = VLexer::TokenNames[tidx];
    if (tidx < TK_URShiftAssign || s[1]) {
      AddTokenToHashTable(s, tidx, bkidx);
    } else {
      vassert(!s[1]);
      vassert(tidx >= TK_URShiftAssign);
      tkCanBeSpecialToken[(vuint8)s[0]] = tidx;
    }
  }
  // hacks
  AddTokenToHashTable("nullptr", (unsigned)TK_Null, bkidx);
  AddTokenToHashTable("NULL", (unsigned)TK_Null, bkidx);
/*
#ifdef VC_LEXER_USE_TOKEN_HASHTABLE
  GLog.Logf(NAME_Debug, "%u buckets used (%u : %u)", bkidx-1, tkMinLen, tkMaxLen);
#endif
*/
}


//==========================================================================
//
//  FindToken
//
//==========================================================================
static EToken FindToken (const char *s, const unsigned slen) noexcept {
  if (slen < tkMinLen || slen > tkMaxLen) return TK_NoToken;
#ifdef VC_LEXER_USE_TOKEN_HASHTABLE
  unsigned n = tkCanBeToken[(vuint8)s[0]];
  while (n) {
    const THEntry &e = tkHashBuckets[n];
    if (e.slen == slen && s[0] == e.str[0] && strcmp(s, e.str) == 0) {
      //GLog.Logf(NAME_Debug, "FOUND! %08x %2u %4u <%s> : <%s>", e.hash, e.slen, e.tidx, e.str, s);
      return (EToken)e.tidx;
    }
    n = e.next;
  }
  //GLog.Logf(NAME_Debug, "*** NOT FOUND! <%s>", s);
  return TK_NoToken;
#else
  if (tkCanBeSpecialToken[(vuint8)s[0]]) {
    for (unsigned tidx = TK_URShiftAssign; tidx < TK_TotalTokenCount; ++tidx) {
      const char *tstr = VLexer::TokenNames[tidx];
      if (s[0] == tstr[0] && strcmp(s, tstr) == 0) return (EToken)tidx;
    }
  } else if (tkCanBeToken[(vuint8)s[0]]) {
    // hacks
    if (s[0] == 'n' && s[1] == 'u' && strcmp(s, "nullptr") == 0) return TK_Null;
    if (s[0] == 'N' && s[1] == 'U' && strcmp(s, "NULL") == 0) return TK_Null;
    //k8: it was a giant `switch`, but meh... it is 2018^w 2019^w 2020^w 2021 now!
    for (unsigned tidx = TK_Abstract; tidx < TK_URShiftAssign; ++tidx) {
      const char *tstr = VLexer::TokenNames[tidx];
      if (s[0] == tstr[0] && strcmp(s, tstr) == 0) return (EToken)tidx;
    }
  }
  return TK_NoToken;
#endif
}


//==========================================================================
//
//  VLexer::VLexer
//
//==========================================================================
VLexer::VLexer () noexcept
  : sourceOpen(false)
  , currCh(0)
  , src(nullptr)
  , totalSize(0)
  , Token(TK_NoToken)
  , Number(0)
  , Float(0)
  , Name(NAME_None)
  , userdata(nullptr)
  , dgOpenFile(nullptr)
{
  memset(tokenStringBuffer, 0, sizeof(tokenStringBuffer));
  if (!tkHashInited) InitTokenHashTable();
  if (!tablesInited) {
    for (int i = 0; i < 256; ++i) {
      ASCIIToChrCode[i] = CHR_Special;
      ASCIIToHexDigit[i] = NON_HEX_DIGIT;
    }
    for (int i = '0'; i <= '9'; ++i) {
      ASCIIToChrCode[i] = CHR_Number;
      ASCIIToHexDigit[i] = i-'0';
    }
    for (int i = 'A'; i <= 'F'; ++i) ASCIIToHexDigit[i] = 10+(i-'A');
    for (int i = 'a'; i <= 'f'; ++i) ASCIIToHexDigit[i] = 10+(i-'a');
    for (int i = 'A'; i <= 'Z'; ++i) ASCIIToChrCode[i] = CHR_Letter;
    for (int i = 'a'; i <= 'z'; ++i) ASCIIToChrCode[i] = CHR_Letter;
    ASCIIToChrCode[(int)'\"'] = CHR_Quote;
    ASCIIToChrCode[(int)'\''] = CHR_SingleQuote;
    ASCIIToChrCode[(int)'_'] = CHR_Letter;
    ASCIIToChrCode[0] = CHR_EOF;
    ASCIIToChrCode[EOF_CHARACTER] = CHR_EOF;
    tablesInited = true;
  }
  String = tokenStringBuffer;
}


//==========================================================================
//
//  VLexer::~VLexer
//
//==========================================================================
VLexer::~VLexer () {
  while (src) PopSource();
  sourceOpen = false;
}


//==========================================================================
//
//  VLexer::AddDefine
//
//==========================================================================
void VLexer::AddDefine (VStr CondName, bool showWarning) {
  if (CondName.length() == 0) return; // get lost!
  // check for redefined names
  for (auto &&ds : defines) {
    if (ds == CondName) {
      if (showWarning) ParseWarning(Location, "Redefined conditional '%s'", *CondName);
      return;
    }
  }
  defines.Append(CondName);
}


//==========================================================================
//
//  VLexer::RemoveDefine
//
//==========================================================================
void VLexer::RemoveDefine (VStr CondName, bool showWarning) {
  if (CondName.length() == 0) return; // get lost!
  bool removed = false;
  int i = 0;
  while (i < defines.length()) {
    if (defines[i] == CondName) { removed = true; defines.removeAt(i); } else ++i;
  }
  if (showWarning && !removed) ParseWarning(Location, "Undefined conditional '%s'", *CondName);
}


//==========================================================================
//
//  VLexer::HasDefine
//
//==========================================================================
bool VLexer::HasDefine (VStr CondName) noexcept {
  if (CondName.length() == 0) return false;
  for (auto &&ds : defines) if (ds == CondName) return true;
  return false;
}


//==========================================================================
//
//  VLexer::doOpenFile
//
//  returns `null` if file not found
//  by default it tries to call `dgOpenFile()`, if it is specified,
//  otherwise falls back to standard vfs
//
//==========================================================================
VStream *VLexer::doOpenFile (VStr filename) {
  if (filename.length() == 0) return nullptr; // just in case
  VStr fname = filename;
#ifdef WIN32
  fname = fname.fixSlashes();
#endif
  if (dgOpenFile) return dgOpenFile(this, fname);
  return VPackage::OpenFileStreamRO(fname);
}


//==========================================================================
//
//  VLexer::OpenSource
//
//==========================================================================
void VLexer::OpenSource (VStr FileName) {
  // read file and prepare for compilation
  PushSource(FileName);
  sourceOpen = true;
  Token = TK_NoToken;
}


//==========================================================================
//
//  VLexer::OpenSource
//
//==========================================================================
void VLexer::OpenSource (VStream *astream, VStr FileName) {
  // read file and prepare for compilation
  PushSource(astream, FileName);
  sourceOpen = true;
  Token = TK_NoToken;
}


//==========================================================================
//
//  VLexer::PushSource
//
//==========================================================================
void VLexer::PushSource (VStr FileName) {
  PushSource(doOpenFile(FileName), FileName);
}


//==========================================================================
//
//  VLexer::PushSource
//
//==========================================================================
void VLexer::PushSource (VStream *Strm, VStr FileName) {
  if (!Strm) {
    VCFatalError("VC: Couldn't open '%s'", *FileName);
    return;
  }

  VSourceFile *NewSrc = new VSourceFile();
  NewSrc->Next = src;
  src = NewSrc;

  // copy file name
  NewSrc->FileName = FileName;

  // extract path to the file
  const char *PathEnd = *FileName+FileName.Length()-1;
#ifdef WIN32
  while (PathEnd >= *FileName && *PathEnd != '/' && *PathEnd != '\\') --PathEnd;
#else
  while (PathEnd >= *FileName && *PathEnd != '/') --PathEnd;
#endif
  if (PathEnd >= *FileName) NewSrc->Path = VStr(FileName, 0, (PathEnd-(*FileName))+1);

  // read the file
  int FileSize = Strm->TotalSize();
  if (Strm->IsError() || FileSize < 0) { VStream::Destroy(Strm); VCFatalError("VC: Couldn't read '%s'", *FileName); return; }
  NewSrc->FileStart = new char[FileSize+1];
  Strm->Serialise(NewSrc->FileStart, FileSize);
  if (Strm->IsError() || FileSize < 0) { VStream::Destroy(Strm); VCFatalError("VC: Couldn't read '%s'", *FileName); return; }
  VStream::Destroy(Strm);

  totalSize += FileSize;

  NewSrc->FileStart[FileSize] = 0; // this is not really required, but let's make the whole buffer initialized
  NewSrc->FileEnd = NewSrc->FileStart+FileSize;
  NewSrc->FilePtr = NewSrc->FileStart;

  // skip garbage some editors add in the begining of UTF-8 files (BOM)
  if (FileSize >= 3 && (vuint8)NewSrc->FilePtr[0] == 0xef && (vuint8)NewSrc->FilePtr[1] == 0xbb && (vuint8)NewSrc->FilePtr[2] == 0xbf) NewSrc->FilePtr += 3;

  // save current character and location to be able to restore them
  NewSrc->currCh = currCh;
  NewSrc->Loc = Location;
  NewSrc->CurrChLoc = CurrChLocation;

  NewSrc->SourceIdx = TLocation::AddSourceFile(FileName);
  NewSrc->IncLineNumber = false;
  NewSrc->NewLine = true;
  NewSrc->Skipping = false;
  Location = TLocation(NewSrc->SourceIdx, 1, 1);
  CurrChLocation = TLocation(NewSrc->SourceIdx, 1, 0); // 0, 'cause `NextChr()` will do `ConsumeChar()`
  NextChr();
}


//==========================================================================
//
//  VLexer::PopSource
//
//==========================================================================
void VLexer::PopSource () {
  if (!src) return;
  if (src->IfStates.length()) ParseError(Location, "#ifdef without a corresponding #endif");
  VSourceFile *Tmp = src;
  delete[] Tmp->FileStart;
  Tmp->FileStart = nullptr;
  src = Tmp->Next;
  currCh = Tmp->currCh;
  Location = Tmp->Loc;
  CurrChLocation = Tmp->CurrChLoc;
  delete Tmp;
}


//==========================================================================
//
//  VLexer::NextToken
//
//==========================================================================
void VLexer::NextToken () {
  if (!src) { NewLine = false; Token = TK_EOF; return; }
  NewLine = src->NewLine;
  do {
    tokenStringBuffer[0] = 0;
    SkipWhitespaceAndComments();
    Location = CurrChLocation;

    if (src->NewLine) {
      NewLine = true;
      // a new line has been started, check preprocessor directive
      src->NewLine = false;
      if (currCh == '#') {
        ProcessPreprocessor();
        continue;
      }
    }

    switch (ASCIIToChrCode[(vuint8)currCh]) {
      case CHR_EOF: PopSource(); Token = (src ? TK_NoToken : TK_EOF); break;
      case CHR_Letter: ProcessLetterToken(true); break;
      case CHR_Number: ProcessNumberToken(); break;
      case CHR_Quote: ProcessQuoteToken(); break;
      case CHR_SingleQuote: ProcessSingleQuoteToken(); break;
      default: ProcessSpecialToken(); break;
    }

    if (Token != TK_EOF && src->Skipping) Token = TK_NoToken;
  } while (Token == TK_NoToken);
}


//==========================================================================
//
//  VLexer::NextChr
//
//  read next character into `currCh`
//
//==========================================================================
void VLexer::NextChr () {
  if (src->FilePtr >= src->FileEnd) {
    currCh = EOF_CHARACTER;
    return;
  }
  CurrChLocation.ConsumeChar(src->IncLineNumber);
  src->IncLineNumber = false;
  currCh = *src->FilePtr++;
  if ((vuint8)currCh < ' ' || (vuint8)currCh == EOF_CHARACTER) {
    if (currCh == '\n') {
      src->IncLineNumber = true;
      src->NewLine = true;
    }
    currCh = ' ';
  }
}


//==========================================================================
//
//  VLexer::Peek
//
//  0 is "currCh"
//
//==========================================================================
char VLexer::Peek (int dist) const noexcept {
  if (dist < 0) ParseError(Location, "VC INTERNAL COMPILER ERROR: peek dist is negative!");
  if (dist == 0) {
    return (src->FilePtr > src->FileStart ? src->FilePtr[-1] : src->FilePtr[0]);
  } else {
    --dist;
    if (src->FileEnd-src->FilePtr <= dist) return EOF_CHARACTER;
    return src->FilePtr[dist];
  }
}


//==========================================================================
//
//  VLexer::SkipComment
//
//  this skips comment, `currCh` is the first non-comment char
//  at enter, `currCh` must be the first comment char
//  returns `true` if some comment was skipped
//
//==========================================================================
bool VLexer::SkipComment () {
  if (currCh != '/') return false;
  char c1 = Peek(1);
  // single-line?
  if (c1 == '/') {
    NextChr();
    vassert(currCh == '/');
    do {
      NextChr();
      if (currCh == EOF_CHARACTER) break;
    } while (!src->IncLineNumber);
    NextChr();
    return true;
  }
  // multiline?
  if (c1 == '*') {
    NextChr();
    vassert(currCh == '*');
    for (;;) {
      NextChr();
      if (currCh == EOF_CHARACTER) {
        ParseError(Location, "End of file inside a comment");
        return true;
      }
      if (currCh == '*' && Peek(1) == '/') {
        NextChr();
        vassert(currCh == '/');
        break;
      }
    }
    NextChr();
    return true;
  }
  // multiline, nested?
  if (c1 == '+') {
    NextChr();
    vassert(currCh == '+');
    int level = 1;
    for (;;) {
      NextChr();
      if (currCh == EOF_CHARACTER) {
        ParseError(Location, "End of file inside a comment");
        return true;
      }
      if (currCh == '+' && Peek(1) == '/') {
        NextChr();
        vassert(currCh == '/');
        --level;
        if (level == 0) break;
      } else if (currCh == '/' && Peek(1) == '+') {
        NextChr();
        vassert(currCh == '+');
        ++level;
      }
    }
    NextChr();
    return true;
  }
  // not a comment
  return false;
}


//==========================================================================
//
//  VLexer::SkipWhitespaceAndComments
//
//==========================================================================
void VLexer::SkipWhitespaceAndComments () {
  Location = CurrChLocation;
  while (currCh != EOF_CHARACTER) {
    if (SkipComment()) continue;
    if ((vuint8)currCh > ' ') break;
    NextChr();
  }
}


//==========================================================================
//
//  VLexer::peekNextNonBlankChar
//
//==========================================================================
char VLexer::peekNextNonBlankChar () const noexcept {
  const char *fpos = src->FilePtr;
  if (fpos > src->FileStart) --fpos; // unget last char
  while (fpos < src->FileEnd) {
    char ch = *fpos++;
    if ((vuint8)ch <= ' ') continue;
    // comment?
    if (ch == '/' && fpos < src->FileEnd) {
      ch = *fpos++;
      // single-line?
      if (ch == '/') {
        while (fpos < src->FileEnd && *fpos != '\n') ++fpos;
        continue;
      }
      // multiline?
      if (ch == '*') {
        while (fpos < src->FileEnd) {
          if (*fpos == '*' && src->FileEnd-fpos > 1 && fpos[1] == '/') { fpos += 2; break; }
          ++fpos;
        }
        continue;
      }
      // multiline nested?
      if (ch == '+') {
        int level = 1;
        while (fpos < src->FileEnd) {
          if (*fpos == '+' && src->FileEnd-fpos > 1 && fpos[1] == '/') {
            fpos += 2;
            if (--level == 0) break;
          } else if (*fpos == '/' && src->FileEnd-fpos > 1 && fpos[1] == '+') {
            ++level;
          } else {
            ++fpos;
          }
        }
        continue;
      }
      return '/';
    }
    return ch;
  }
  return EOF_CHARACTER;
}


//==========================================================================
//
//  VLexer::SkipCurrentLine
//
//  skip current line, correctly process comments
//  returns `true` if no non-whitespace and non-comment chars were seen
//  used to skip lines in preprocessor
//
//==========================================================================
bool VLexer::SkipCurrentLine () {
  if (src->NewLine || currCh != EOF_CHARACTER) return true;
  bool noBadChars = true;
  while (!src->NewLine && currCh != EOF_CHARACTER) {
    if (SkipComment()) continue;
    if ((vuint8)currCh > ' ') noBadChars = false;
    NextChr();
  }
  if (currCh != EOF_CHARACTER) {
    vassert(currCh == ' ');
    NextChr();
  }
  return noBadChars;
}


//==========================================================================
//
//  VLexer::ProcessPreprocessor
//
//==========================================================================
void VLexer::ProcessPreprocessor () {
  NextChr();
  while (currCh != EOF_CHARACTER && !src->NewLine && (vuint8)currCh <= ' ') NextChr();

  if (src->NewLine || currCh == EOF_CHARACTER) {
    if (src->Skipping) (void)SkipCurrentLine(); else ParseError(Location, "Compiler directive expected");
    return;
  }

  if (ASCIIToChrCode[(vuint8)currCh] != CHR_Letter) {
    if (src->Skipping) {
      (void)SkipCurrentLine();
    } else {
      ParseError(Location, "Compiler directive expected");
      while (!src->NewLine && currCh != EOF_CHARACTER) NextChr();
    }
    return;
  }

  ProcessLetterToken(false);
  VStr dname(tokenStringBuffer);

       if (VStr::strEqu(tokenStringBuffer, "line")) ProcessLineDirective();
  else if (VStr::strEqu(tokenStringBuffer, "define")) ProcessDefine();
  else if (VStr::strEqu(tokenStringBuffer, "undef")) ProcessUndef();
  else if (VStr::strEqu(tokenStringBuffer, "ifdef")) ProcessIfDef(true);
  else if (VStr::strEqu(tokenStringBuffer, "ifndef")) ProcessIfDef(false);
  else if (VStr::strEqu(tokenStringBuffer, "if")) ProcessIf();
  else if (VStr::strEqu(tokenStringBuffer, "else")) ProcessElse();
  else if (VStr::strEqu(tokenStringBuffer, "endif")) ProcessEndIf();
  else if (VStr::strEqu(tokenStringBuffer, "include")) { ProcessInclude(); Token = TK_NoToken; return; }
  else ProcessUnknownDirective();

  Token = TK_NoToken;

  if (src->Skipping) {
    (void)SkipCurrentLine();
  } else {
    //SkipWhitespaceAndComments();
    // a new-line is expected at the end of preprocessor directive
    //if (!src->NewLine) ParseError(Location, "Compiler directive contains extra code");
    if (!SkipCurrentLine()) ParseError(Location, "Compiler directive `%s` contains extra code", *dname);
  }
}


//==========================================================================
//
//  VLexer::ProcessUnknownDirective
//
//==========================================================================
void VLexer::ProcessUnknownDirective () {
  if (!src->Skipping) ParseError(Location, "Unknown compiler directive `%s`", tokenStringBuffer);
}


//==========================================================================
//
//  VLexer::ProcessLineDirective
//
//==========================================================================
void VLexer::ProcessLineDirective () {
  if (src->Skipping) return;

  // read line number
  SkipWhitespaceAndComments();
  if (src->NewLine || currCh == EOF_CHARACTER || ASCIIToChrCode[(vuint8)currCh] != CHR_Number) ParseError(Location, "`#line`: line number expected");
  ProcessNumberToken();
  if (Token != TK_IntLiteral) ParseError(Location, "`#line`: integer expected");
  auto lno = Number;

  // read file name
  SkipWhitespaceAndComments();
  if (src->NewLine || currCh == EOF_CHARACTER || ASCIIToChrCode[(vuint8)currCh] != CHR_Quote) ParseError(Location, "`#line`: file name expected");
  ProcessFileName();
  src->SourceIdx = TLocation::AddSourceFile(String);
  int srcidx = src->SourceIdx;

  // ignore flags
  (void)SkipCurrentLine();

  if (currCh != EOF_CHARACTER) {
    Location = TLocation(srcidx, lno, 1);
    CurrChLocation = Location;
  }
}


//==========================================================================
//
//  VLexer::ProcessDefine
//
//==========================================================================
void VLexer::ProcessDefine () {
  if (src->Skipping) return;
  SkipWhitespaceAndComments();

  // argument to the #define must be on the same line
  if (src->NewLine || currCh == EOF_CHARACTER) {
    ParseError(Location, "`#define`: missing argument");
    return;
  }

  // parse name to be defined
  if (ASCIIToChrCode[(vuint8)currCh] != CHR_Letter) {
    ParseError(Location, "`#define`: invalid define name");
    while (!src->NewLine && currCh != EOF_CHARACTER) NextChr();
    return;
  }
  ProcessLetterToken(false);

  //if (src->Skipping) return;
  AddDefine(tokenStringBuffer);
}


//==========================================================================
//
//  VLexer::ProcessUndef
//
//==========================================================================
void VLexer::ProcessUndef () {
  if (src->Skipping) return;
  SkipWhitespaceAndComments();

  // argument to the #undef must be on the same line
  if (src->NewLine || currCh == EOF_CHARACTER) {
    ParseError(Location, "`#undef`: missing argument");
    return;
  }

  // parse name to be defined
  if (ASCIIToChrCode[(vuint8)currCh] != CHR_Letter) {
    ParseError(Location, "`#undef`: invalid define name");
    while (!src->NewLine && currCh != EOF_CHARACTER) NextChr();
    return;
  }
  ProcessLetterToken(false);

  //if (src->Skipping) return;
  RemoveDefine(tokenStringBuffer);
}


//==========================================================================
//
//  VLexer::ProcessIfDef
//
//==========================================================================
void VLexer::ProcessIfDef (bool OnTrue) {
  const char *ifname = (OnTrue ? "#ifdef" : "#ifndef");

  // if we're skipping now, don't bother
  if (src->Skipping) {
    src->IfStates.Append(IF_Skip);
    return;
  }

  SkipWhitespaceAndComments();

  // argument to the #ifdef must be on the same line
  if (src->NewLine || currCh == EOF_CHARACTER) {
    ParseError(Location, "`%s`: missing argument", ifname);
    return;
  }

  // parse condition name
  if (ASCIIToChrCode[(vuint8)currCh] != CHR_Letter) {
    ParseError(Location, "`%s`: invalid argument", ifname);
    SkipCurrentLine();
    return;
  }
  ProcessLetterToken(false);

  // check if the name has been defined
  bool found = HasDefine(tokenStringBuffer);
  if (found == OnTrue) {
    src->IfStates.Append(IF_True);
  } else {
    src->IfStates.Append(IF_False);
    src->Skipping = true;
  }
}


//==========================================================================
//
//  VLexer::ProcessIfTerm
//
//  result is <0: error
//
//==========================================================================
int VLexer::ProcessIfTerm () {
  SkipWhitespaceAndComments();
  if (src->NewLine || currCh == EOF_CHARACTER) {
    ParseError(Location, "`#if`: missing argument");
    return -1;
  }
  // number?
  if (ASCIIToChrCode[(vuint8)currCh] == CHR_Number) {
    ProcessNumberToken();
    if (Token != TK_IntLiteral) {
      ParseError(Location, "`#if`: integer expected");
      return -1;
    }
    return (Number ? 1 : 0);
  }
  // `!`?
  if (currCh == '!') {
    NextChr();
    int res = ProcessIfTerm();
    if (res >= 0) res = 1-res;
    return res;
  }
  // `(`?
  if (currCh == '(') {
    NextChr();
    int res = ProcessIfExpr();
    if (res >= 0) {
      SkipWhitespaceAndComments();
      if (src->NewLine || currCh != ')') {
        ParseError(Location, "`#if`: `)` expected");
        return -1;
      }
      NextChr();
    }
    return res;
  }
  // `defined`?
  if (currCh == 'd') {
    ProcessLetterToken(false);
    if (!VStr::strEqu(tokenStringBuffer, "defined")) {
      ParseError(Location, "`#if`: `defined` expected");
      return -1;
    }
    SkipWhitespaceAndComments();
    if (src->NewLine || currCh != '(') {
      ParseError(Location, "`#if`: `(` expected");
      return -1;
    }
    NextChr();
    SkipWhitespaceAndComments();
    if (src->NewLine || ASCIIToChrCode[(vuint8)currCh] != CHR_Letter) {
      ParseError(Location, "`#if`: identifier expected");
      return -1;
    }
    ProcessLetterToken(false);
    int res = (HasDefine(tokenStringBuffer) ? 1 : 0);
    SkipWhitespaceAndComments();
    if (src->NewLine || currCh != ')') {
      ParseError(Location, "`#if`: `)` expected");
      return -1;
    }
    NextChr();
    return res;
  }
  // `option`?
  if (currCh == 'o') {
    ProcessLetterToken(false);
    if (!VStr::strEqu(tokenStringBuffer, "option")) {
      ParseError(Location, "`#if`: `option` expected");
      return -1;
    }
    SkipWhitespaceAndComments();
    if (src->NewLine || currCh != '(') {
      ParseError(Location, "`#if`: `(` expected");
      return -1;
    }
    NextChr();
    SkipWhitespaceAndComments();
    if (src->NewLine || ASCIIToChrCode[(vuint8)currCh] != CHR_Letter) {
      ParseError(Location, "`#if`: identifier expected");
      return -1;
    }
    ProcessLetterToken(false);
    int res = 0;
         if (VStr::strEquCI(tokenStringBuffer, "cilocals")) res = !VObject::cliCaseSensitiveLocals;
    else if (VStr::strEquCI(tokenStringBuffer, "cslocals")) res = VObject::cliCaseSensitiveLocals;
    else if (VStr::strEquCI(tokenStringBuffer, "cifields")) res = !VObject::cliCaseSensitiveFields;
    else if (VStr::strEquCI(tokenStringBuffer, "csfields")) res = VObject::cliCaseSensitiveFields;
    else ParseError(Location, "`#if`: unknown option `%s`", tokenStringBuffer);
    SkipWhitespaceAndComments();
    if (src->NewLine || currCh != ')') {
      ParseError(Location, "`#if`: `)` expected");
      return -1;
    }
    NextChr();
    return res;
  }
  // other
  ParseError(Location, "`#if`: unexpected token");
  return -1;
}


//==========================================================================
//
//  VLexer::ProcessIfLAnd
//
//  result is <0: error
//
//==========================================================================
int VLexer::ProcessIfLAnd () {
  int res = ProcessIfTerm();
  if (res < 0) return res;
  for (;;) {
    SkipWhitespaceAndComments();
    if (src->NewLine || currCh == EOF_CHARACTER) return res;
    if (currCh != '&') return res;
    NextChr();
    if (src->NewLine || currCh != '&') {
      ParseError(Location, "`#if`: `&&` expected");
      return -1;
    }
    NextChr();
    int op2 = ProcessIfTerm();
    if (op2 < 0) return op2;
    res = (res && op2 ? 1 : 0);
  }
}


//==========================================================================
//
//  VLexer::ProcessIfLOr
//
//  result is <0: error
//
//==========================================================================
int VLexer::ProcessIfLOr () {
  int res = ProcessIfLAnd();
  if (res < 0) return res;
  for (;;) {
    SkipWhitespaceAndComments();
    if (src->NewLine || currCh == EOF_CHARACTER) return res;
    if (currCh != '|') return res;
    NextChr();
    if (src->NewLine || currCh != '|') {
      ParseError(Location, "`#if`: `||` expected");
      return -1;
    }
    NextChr();
    int op2 = ProcessIfLAnd();
    if (op2 < 0) return op2;
    res = (res || op2 ? 1 : 0);
  }
}


//==========================================================================
//
//  VLexer::ProcessIfExpr
//
//  result is <0: error
//
//==========================================================================
int VLexer::ProcessIfExpr () {
  return ProcessIfLOr();
}


//==========================================================================
//
//  VLexer::ProcessIf
//
//==========================================================================
void VLexer::ProcessIf () {
  // if we're skipping now, don't bother
  if (src->Skipping) {
    src->IfStates.Append(IF_Skip);
    return;
  }

  SkipWhitespaceAndComments();

  int val = ProcessIfExpr();
  bool isTrue = (val > 0);

  if (isTrue) {
    src->IfStates.Append(IF_True);
  } else {
    src->IfStates.Append(IF_False);
    src->Skipping = true;
  }
}


//==========================================================================
//
//  VLexer::ProcessElse
//
//==========================================================================
void VLexer::ProcessElse () {
  if (!src->IfStates.length()) {
    ParseError(Location, "`#else` without an `#if`");
    return;
  }
  switch (src->IfStates[src->IfStates.length()-1]) {
    case IF_True:
      src->IfStates[src->IfStates.length()-1] = IF_ElseFalse;
      src->Skipping = true;
      break;
    case IF_False:
      src->IfStates[src->IfStates.length()-1] = IF_ElseTrue;
      src->Skipping = false;
      break;
    case IF_Skip:
      src->IfStates[src->IfStates.length()-1] = IF_ElseSkip;
      break;
    case IF_ElseTrue:
    case IF_ElseFalse:
    case IF_ElseSkip:
      ParseError(Location, "Multiple `#else` directives for a single `#ifdef`");
      src->Skipping = true;
      break;
  }
}


//==========================================================================
//
//  VLexer::ProcessEndIf
//
//==========================================================================
void VLexer::ProcessEndIf () {
  if (!src->IfStates.length()) {
    ParseError(Location, "`#endif` without an `#if`");
    return;
  }
  src->IfStates.RemoveIndex(src->IfStates.length()-1);
  if (src->IfStates.length() > 0) {
    switch (src->IfStates[src->IfStates.length()-1]) {
      case IF_True:
      case IF_ElseTrue:
        src->Skipping = false;
        break;
      case IF_False:
      case IF_ElseFalse:
        src->Skipping = true;
        break;
      case IF_Skip:
      case IF_ElseSkip:
        break;
    }
  } else {
    src->Skipping = false;
  }
}


//==========================================================================
//
//  VLexer::ProcessInclude
//
//==========================================================================
void VLexer::ProcessInclude () {
  if (src->Skipping) { (void)SkipCurrentLine(); return; }
  SkipWhitespaceAndComments();

  // file name must be on the same line
  if (src->NewLine || currCh == EOF_CHARACTER) {
    ParseError(Location, "`#include`: file name expected");
    return;
  }

  // parse file name
  if (currCh != '\"') {
    ParseError(Location, "`#include`: string expected");
    while (!src->NewLine && currCh != EOF_CHARACTER) NextChr();
    return;
  }
  ProcessFileName();
  //TLocation Loc = Location;

  // a new-line is expected at the end of preprocessor directive
  if (!SkipCurrentLine()) {
    ParseError(Location, "`#include`: no extra arguments allowed");
    return;
  }

  // check if it's an absolute path location
  if (tokenStringBuffer[0] != '/' && tokenStringBuffer[0] != '\\') {
    // first try relative to the current source file
    if (src->Path.IsNotEmpty()) {
      VStr FileName = src->Path+VStr(tokenStringBuffer);
      VStream *Strm = doOpenFile(FileName);
      if (Strm) {
        PushSource(Strm, FileName);
        return;
      }
    }

    for (int i = includePath.length()-1; i >= 0; --i) {
      VStr FileName = includePath[i]+VStr(tokenStringBuffer);
      VStream *Strm = doOpenFile(FileName);
      if (Strm) {
        PushSource(Strm, FileName);
        return;
      }
    }
  }

  // either it's relative to the current directory or absolute path
  PushSource(tokenStringBuffer);
}


//==========================================================================
//
//  VLexer::AddIncludePath
//
//==========================================================================
void VLexer::AddIncludePath (VStr DirName) {
  if (DirName.length() == 0) return; // get lost
  VStr copy = DirName;
  // append trailing slash if needed
#ifndef WIN32
  if (!copy.EndsWith("/")) copy += '/';
#else
  if (!copy.EndsWith("/") && !copy.EndsWith("\\")) copy += '/';
  copy = copy.fixSlashes();
#endif
  // check for duplicate pathes
  for (int i = 0; i < includePath.length(); ++i) {
#ifndef WIN32
    if (includePath[i] == copy) return;
#else
    if (includePath[i].ICmp(copy) == 0) return;
#endif
  }
  includePath.Append(copy);
}


//==========================================================================
//
//  VLexer::ProcessNumberToken
//
//==========================================================================
void VLexer::ProcessNumberToken () {
  Token = TK_IntLiteral;
  // collect number into buffer, because we have to call a proper parser for floats
  bool isHex = false;
  char numbuf[256];
  unsigned nbpos = 1; // [0] will be `currCh`
  Number = 0;
  char c = currCh;
  numbuf[0] = c;
  NextChr();
  if (c == '0') {
    int base = 0;
    switch (currCh) {
      case 'x': case 'X': isHex = true; numbuf[nbpos++] = 'x'; break;
      case 'd': case 'D': base = 10; break;
      case 'o': case 'O': base = 8; break;
      case 'b': case 'B': base = 2; break;
    }
    if (base != 0) {
      // non-hex radix, only integers
      NextChr(); // skip radix char
      for (;;) {
        if (currCh != '_') {
          int d = VStr::digitInBase(currCh, base);
          if (d < 0) break;
          Number = Number*base+d;
        }
        NextChr();
      }
      #ifdef VC_LEXER_DUMP_COLLECTED_NUMBERS
      GLog.Logf(NAME_Debug, "*** CONVERTED INT(base=%d): %d", base, Number);
      #endif
      if (isAlpha(currCh)) ParseError(Location, "Invalid number");
      return;
    }
    if (isHex) NextChr(); // skip 'x'
  }

  // this may be integer or float, possibly in hex form

  // collect integral part
  int xbase = (isHex ? 16 : 10);
  for (;;) {
    if (currCh != '_') {
      int d = VStr::digitInBase(currCh, xbase);
      if (d < 0) break;
      if (nbpos >= sizeof(numbuf)-1) ParseError(Location, "Invalid number");
      numbuf[nbpos++] = currCh;
    }
    NextChr();
  }

  // floating dot and then non-numeric char?
  // it is so things like `10.seconds` are allowed
  if (currCh == '.') {
    const char nch = Peek(1);
    if (VStr::digitInBase(nch, xbase) < 0) {
      // num dot smth
      numbuf[nbpos++] = 0; // we always have the room for trailing zero
      #ifdef VC_LEXER_DUMP_COLLECTED_NUMBERS
      GLog.Logf(NAME_Debug, "*** COLLECTED INT(0): <%s>", numbuf);
      #endif
      if (!VStr::convertInt(numbuf, &Number)) ParseError(Location, "Invalid floating number");
      return;
    }
  }

  // float number?
  if (currCh == 'f' || currCh == '.' ||
      (isHex && (currCh == 'p' || currCh == 'P')) ||
      (!isHex && (currCh == 'e' || currCh == 'E')))
  {
    // float number
    Token = TK_FloatLiteral;

    // dotted part
    if (currCh == '.') {
      if (nbpos >= sizeof(numbuf)-1) ParseError(Location, "Invalid number");
      numbuf[nbpos++] = '.';
      NextChr(); // skip dot
      for (;;) {
        if (currCh != '_') {
          int d = VStr::digitInBase(currCh, xbase);
          if (d < 0) break;
          if (nbpos >= sizeof(numbuf)-1) ParseError(Location, "Invalid number");
          numbuf[nbpos++] = currCh;
        }
        NextChr();
      }
    }

    // exponent
    if ((isHex && (currCh == 'p' || currCh == 'P')) ||
        (!isHex && (currCh == 'e' || currCh == 'E')))
    {
      if (nbpos >= sizeof(numbuf)-1) ParseError(Location, "Invalid floating number exponent");
      numbuf[nbpos++] = (isHex ? 'p' : 'e');
      NextChr(); // skip e/p
      if (currCh == '+' || currCh == '-') {
        if (nbpos >= sizeof(numbuf)-1) ParseError(Location, "Invalid floating number exponent");
        numbuf[nbpos++] = currCh;
        NextChr(); // skip sign
      }
      for (;;) {
        if (currCh != '_') {
          int d = VStr::digitInBase(currCh, 10/*xbase*/); // it is decimal for both types
          if (d < 0) break;
          if (nbpos >= sizeof(numbuf)-1) { ParseError(Location, "Invalid floating number exponent"); nbpos = (unsigned)(sizeof(numbuf)-1); }
          numbuf[nbpos++] = currCh;
        }
        NextChr();
      }
    }

    // skip optional 'f'
    if (currCh == 'f') NextChr();

    if (isAlpha(currCh) || (currCh >= '0' && currCh <= '9')) ParseError(Location, "Invalid floating number");
    if (nbpos >= sizeof(numbuf)-1) { ParseError(Location, "Invalid floating number"); nbpos = (unsigned)(sizeof(numbuf)-1); }
    numbuf[nbpos++] = 0; // we always have the room for trailing zero
    #ifdef VC_LEXER_DUMP_COLLECTED_NUMBERS
    GLog.Logf(NAME_Debug, "*** COLLECTED FLOAT: <%s>\n", numbuf);
    #endif
    if (!VStr::convertFloat(numbuf, &Float)) ParseError(Location, "Invalid floating number");
    Number = (int)Float;
  } else {
    // not a float
    if (isAlpha(currCh) || (currCh >= '0' && currCh <= '9')) ParseError(Location, "Invalid integer number");
    if (nbpos >= sizeof(numbuf)-1) { ParseError(Location, "Invalid integer number"); nbpos = (unsigned)(sizeof(numbuf)-1); }
    numbuf[nbpos++] = 0; // we always have the room for trailing zero
    #ifdef VC_LEXER_DUMP_COLLECTED_NUMBERS
    GLog.Logf(NAME_Debug, "*** COLLECTED INT(1): <%s>\n", numbuf);
    #endif
    if (!VStr::convertInt(numbuf, &Number)) ParseError(Location, "Invalid integer number");
  }
}


//==========================================================================
//
//  VLexer::ProcessChar
//
//==========================================================================
void VLexer::ProcessChar () {
  if (currCh == EOF_CHARACTER) {
    ParseError(Location, ERR_EOF_IN_STRING);
    BailOut();
  }
  if (src->IncLineNumber) ParseError(Location, ERR_NEW_LINE_INSIDE_QUOTE);
  if (currCh == '\\') {
    // special symbol
    NextChr();
    if (currCh == EOF_CHARACTER) { ParseError(Location, ERR_EOF_IN_STRING); BailOut(); }
    if (src->IncLineNumber) ParseError(Location, ERR_NEW_LINE_INSIDE_QUOTE);
    switch (currCh) {
      case 'n': currCh = '\n'; break;
      case 'r': currCh = '\r'; break;
      case 't': currCh = '\t'; break;
      case '\'': currCh = '\''; break;
      case '"': currCh = '"'; break;
      case '\\': currCh = '\\'; break;
      case 'c': currCh = TEXT_COLOR_ESCAPE; break;
      case 'e': currCh = '\x1b'; break;
      case 'x': case 'X':
        {
          NextChr();
          if (currCh == EOF_CHARACTER) { ParseError(Location, ERR_EOF_IN_STRING); BailOut(); }
          if (ASCIIToHexDigit[(vuint8)currCh] == NON_HEX_DIGIT) { ParseError(Location, ERR_EOF_IN_STRING); BailOut(); }
          int n = ASCIIToHexDigit[(vuint8)currCh];
          // second digit
          if (src->FilePtr < src->FileEnd && ASCIIToHexDigit[(vuint8)(*src->FilePtr)] != NON_HEX_DIGIT) {
            NextChr();
            n = n*16+ASCIIToHexDigit[(vuint8)currCh];
          }
          currCh = (char)n;
        }
        break;
      default: ParseError(Location, ERR_UNKNOWN_ESC_CHAR);
    }
  }
}


//==========================================================================
//
//  VLexer::ProcessQuoted
//
//==========================================================================
void VLexer::ProcessQuoted (const char ech) {
  const unsigned maxlen = (ech == '"' ? MAX_QUOTED_LENGTH : (NAME_SIZE+1 < MAX_QUOTED_LENGTH ? NAME_SIZE+1 : MAX_QUOTED_LENGTH))-1;
  unsigned len = 0;
  bool collecting = true;
  NextChr();
  while (currCh != ech) {
    if (len >= maxlen) {
      if (collecting) { ParseError(Location, ERR_STRING_TOO_LONG); collecting = false; len = maxlen; }
      NextChr();
      continue;
    }
    ProcessChar(); // this also checks for newline
    tokenStringBuffer[len] = currCh;
    NextChr();
    ++len;
  }
  tokenStringBuffer[len] = 0;
  NextChr();
}


//==========================================================================
//
//  VLexer::ProcessQuoteToken
//
//==========================================================================
void VLexer::ProcessQuoteToken () {
  Token = TK_StringLiteral;
  ProcessQuoted('"');
}


//==========================================================================
//
//  VLexer::ProcessSingleQuoteToken
//
//==========================================================================
void VLexer::ProcessSingleQuoteToken () {
  Token = TK_NameLiteral;
  ProcessQuoted('\'');
  Name = tokenStringBuffer;
}


//==========================================================================
//
//  VLexer::ProcessLetterToken
//
//==========================================================================
void VLexer::ProcessLetterToken (bool CheckKeywords) {
  Token = TK_Identifier;
  unsigned len = 0;
  bool collecting = true;
  while (ASCIIToChrCode[(vuint8)currCh] == CHR_Letter || ASCIIToChrCode[(vuint8)currCh] == CHR_Number) {
    if (len >= MAX_IDENTIFIER_LENGTH-1) {
      if (collecting) { ParseError(Location, ERR_IDENTIFIER_TOO_LONG); collecting = false; len = MAX_IDENTIFIER_LENGTH-1; }
      NextChr();
      continue;
    }
    tokenStringBuffer[len] = currCh;
    ++len;
    NextChr();
  }
  tokenStringBuffer[len] = 0;

  if (!CheckKeywords) return;

  const EToken tkk = FindToken(tokenStringBuffer, len);
  if (tkk != TK_NoToken && tkk < TK_URShiftAssign) Token = tkk;

  if (Token == TK_Identifier) Name = tokenStringBuffer;
}


//==========================================================================
//
//  VLexer::ProcessSpecialToken
//
//==========================================================================
void VLexer::ProcessSpecialToken () {
  char tkbuf[16]; // way too much
  #if 0
  Token = TK_NoToken;
  size_t tkbpos = 0;
  for (;;) {
    tkbuf[tkbpos] = currCh;
    tkbuf[tkbpos+1] = 0;
    EToken ntk = TK_NoToken;
    for (unsigned tidx = TK_URShiftAssign; tidx < TK_TotalTokenCount; ++tidx) {
      if (tkbuf[0] == TokenNames[tidx][0] && strcmp(tkbuf, TokenNames[tidx]) == 0) {
        ntk = (EToken)tidx;
        break;
      }
    }
    // not found?
    if (ntk == TK_NoToken) {
      // use last found token
      if (Token == TK_NoToken) ParseError(Location, ERR_BAD_CHARACTER, "Unknown punctuation '%s'", tkbuf);
      return;
    }
    // new token found, eat one char and repeat
    Token = ntk;
    NextChr();
    if (++tkbpos >= (size_t)(sizeof(tkbuf)-1)) VCFatalError("VC: something is very wrong with the lexer");
  }
  #else
  tkbuf[0] = currCh;
  tkbuf[1] = 0;
  Token = (EToken)tkCanBeSpecialToken[(vuint8)currCh];
  if (Token == TK_NoToken) {
    ParseError(Location, ERR_BAD_CHARACTER, "Unknown punctuation '%s'", tkbuf);
    return;
  }
  size_t tkbpos = 1;
  for (;;) {
    // new token found, eat one char and repeat
    NextChr();
    tkbuf[tkbpos++] = currCh;
    tkbuf[tkbpos] = 0;
    if (tkbpos >= (size_t)(sizeof(tkbuf)-1)) VCFatalError("VC: something is very wrong with the lexer");
    EToken ttk = FindToken(tkbuf, tkbpos);
    if (ttk == TK_NoToken || ttk < TK_URShiftAssign) break; // cannot be longer
    Token = ttk;
  }
  #endif
}


//==========================================================================
//
//  VLexer::ProcessFileName
//
//==========================================================================
void VLexer::ProcessFileName () {
  int len = 0;
  NextChr();
  while (currCh != '"') {
    if (currCh == '\\') {
      currCh = '/';
      char ch = Peek(1);
      if (ch == '\\') {
        NextChr();
        vassert(currCh == '\\');
        currCh = '/';
      } else {
        ParseError(Location, "invalid escaping in file name");
      }
    }
    if (len >= MAX_QUOTED_LENGTH-1) {
      ParseError(Location, ERR_STRING_TOO_LONG);
      NextChr();
      continue;
    }
    if (currCh == EOF_CHARACTER) {
      ParseError(Location, ERR_EOF_IN_STRING);
      break;
    }
    if (src->IncLineNumber) ParseError(Location, ERR_NEW_LINE_INSIDE_QUOTE);
    tokenStringBuffer[len] = currCh;
    NextChr();
    ++len;
  }
  tokenStringBuffer[len] = 0;
  NextChr();
}


//==========================================================================
//
//  VLexer::Check
//
//==========================================================================
bool VLexer::Check (EToken tk) {
  if (Token == tk) { NextToken(); return true; }
  return false;
}


//==========================================================================
//
//  VLexer::Expect
//
//  Report error, if current token is not equals to tk.
//  Take next token.
//
//==========================================================================
void VLexer::Expect (EToken tk) {
  if (Token != tk) ParseError(Location, "expected `%s`, found `%s`", TokenNames[tk], TokenNames[Token]);
  NextToken();
}


//==========================================================================
//
//  VLexer::Expect
//
//  Report error, if current token is not equals to tk.
//  Take next token.
//
//==========================================================================
void VLexer::Expect (EToken tk, ECompileError error) {
  if (Token != tk) ParseError(Location, error, "expected `%s`, found `%s`", TokenNames[tk], TokenNames[Token]);
  NextToken();
}


//==========================================================================
//
//  VLexer::ExpectAnyIdentifier
//
//  this also allows keywords
//
//==========================================================================
VStr VLexer::ExpectAnyIdentifier () {
  VStr res;
       if (Token == TK_Identifier) res = VStr(Name);
  else if (Token >= TK_Abstract && Token < TK_URShiftAssign) res = TokenNames[Token]; //WARNING! sync this with keywords!
  else { ParseError(Location, "identifier expected"); res = "<unknown>"; }
  NextToken();
  return res;
}


//==========================================================================
//
//  VLexer::Check
//
//  check for identifier (it cannot be a keyword)
//
//==========================================================================
bool VLexer::Check (const char *id, bool caseSensitive) {
  vassert(id);
  vassert(id[0]);
  if (Token != TK_Identifier) return false;
  bool ok = ((caseSensitive ? VStr::Cmp(id, *Name) : VStr::ICmp(id, *Name)) == 0);
  if (!ok) return false;
  NextToken();
  return true;
}


//==========================================================================
//
//  VLexer::Expect
//
//  expect identifier (it cannot be a keyword)
//
//==========================================================================
void VLexer::Expect (const char *id, bool caseSensitive) {
  vassert(id);
  vassert(id[0]);
  if (Token != TK_Identifier) ParseError(Location, "expected `%s`, found `%s`", id, TokenNames[Token]);
  bool ok = ((caseSensitive ? VStr::Cmp(id, *Name) : VStr::ICmp(id, *Name)) == 0);
  if (!ok) ParseError(Location, "expected `%s`, found `%s`", id, *Name);
  NextToken();
}


//==========================================================================
//
//  VLexer::isNStrEqu
//
//==========================================================================
bool VLexer::isNStrEqu (int spos, int epos, const char *s) const noexcept {
  if (!s) s = "";
  if (spos >= epos) return (s[0] == 0);
  if (spos < 0 || epos > src->FileEnd-src->FileStart) return false;
  auto slen = (int)strlen(s);
  if (epos-spos != slen) return false;
  return (memcmp(src->FileStart+spos, s, slen) == 0);
}


//==========================================================================
//
//  VLexer::posAtEOS
//
//==========================================================================
bool VLexer::posAtEOS (int cpos) const {
  if (cpos < 0) cpos = 0;
  return (cpos >= src->FileEnd-src->FileStart);
}


//==========================================================================
//
//  VLexer::peekChar
//
//  returns 0 on EOS
//
//==========================================================================
vuint8 VLexer::peekChar (int cpos) const {
  if (cpos < 0) cpos = 0;
  if (cpos >= src->FileEnd-src->FileStart) return 0;
  vuint8 ch = src->FileStart[cpos];
  if (ch == 0) ch = ' ';
  return ch;
}


//==========================================================================
//
//  VLexer::skipBlanksFrom
//
//  returns `false` on EOS
//
//==========================================================================
bool VLexer::skipBlanksFrom (int &cpos) const {
  if (cpos < 0) cpos = 0; // just in case
  for (;;) {
    vuint8 ch = peekChar(cpos);
    if (!ch) break; // EOS
    if (ch <= ' ') { ++cpos; continue; }
    if (ch != '/') return true; // not a comment
    ch = peekChar(cpos+1);
    // block comment?
    if (ch == '*') {
      cpos += 2;
      for (;;) {
        ch = peekChar(cpos);
        if (!ch) return false;
        if (ch == '*' && peekChar(cpos+1) == '/') { cpos += 2; break; }
        ++cpos;
      }
      continue;
    }
    // nested block comment?
    if (ch == '+') {
      int level = 1;
      cpos += 2;
      for (;;) {
        ch = peekChar(cpos);
        if (!ch) return false;
        if (ch == '+' && peekChar(cpos+1) == '/') {
          cpos += 2;
          if (--level == 0) break;
        } else if (ch == '/' && peekChar(cpos+1) == '+') {
          cpos += 2;
          ++level;
        } else {
          ++cpos;
        }
      }
      continue;
    }
    // c++ style comment
    if (ch == '/') {
      for (;;) {
        ch = peekChar(cpos);
        if (!ch) return false;
        ++cpos;
        if (ch == '\n') break;
      }
      continue;
    }
    return true; // nonblank
  }
  return false;
}


//==========================================================================
//
//  VLexer::skipTokenFrom
//
//  calls skipBlanksFrom, returns token type or TK_NoToken
//
//==========================================================================
EToken VLexer::skipTokenFrom (int &cpos, VStr *str=nullptr) const {
  if (str) str->clear();
  if (!skipBlanksFrom(cpos)) return TK_NoToken;
  // classify token
  vuint8 ch = peekChar(cpos);
  if (!ch) return TK_NoToken; // just in case

  char tmpbuf[MAX_IDENTIFIER_LENGTH];
  //fprintf(stderr, " tkstart=%d; ch=%c\n", cpos, ch);

  // quoted string?
  if (ch == '"' || ch == '\'') {
    if (str) (*str) += (char)ch;
    vuint8 ech = ch;
    ++cpos;
    for (;;) {
      ch = peekChar(cpos++);
      if (str) (*str) += (char)ch;
      if (ch == '\\') {
        ++cpos; // unconditionally skip next char
      } else {
        if (ch == ech) break;
      }
    }
    return (ech == '"' ? TK_StringLiteral : TK_NameLiteral);
  }

  // number?
  if (ch >= '0' && ch <= '9') {
    if (str) (*str) += (char)ch;
    int base = 0;
    if (ch == '0') {
      switch (peekChar(cpos+1)) {
        case 'b': case 'B': base = 2; break;
        case 'o': case 'O': base = 8; break;
        case 'd': case 'D': base = 10; break;
        case 'x': case 'X': base = 16; break;
      }
      if (base) {
        if (str) (*str) += (char)peekChar(cpos+1);
        cpos += 2;
      }
    }
    for (;;) {
      ch = peekChar(cpos);
      if (ch != '_' && VStr::digitInBase(ch, (base ? base : 10)) < 0) break;
      if (str) (*str) += (char)ch;
      ++cpos;
    }
    if (base != 0) return TK_IntLiteral; // for now, there is no non-decimal floating literals
    if (peekChar(cpos) == '.') {
      vuint8 nch = peekChar(cpos+1);
      if (isAlpha(nch) || nch == '_' || nch == '.' || nch == 0) return TK_IntLiteral; // num dot smth
      // floating literal
      if (str) (*str) += '.';
      ++cpos;
      for (;;) {
        ch = peekChar(cpos);
        if (ch != '_' && VStr::digitInBase(ch, 10) < 0) break;
        if (str) (*str) += (char)ch;
        ++cpos;
      }
      if (peekChar(cpos) == 'f') {
        if (str) (*str) += '.';
        ++cpos;
      }
      return TK_FloatLiteral;
    }
    return TK_IntLiteral;
  }

  // identifier?
  if (isAlpha(ch) || ch >= 128 || ch == '_') {
    // find identifier end
    int spos = cpos;
    for (;;) {
      ch = peekChar(cpos);
      if (!ch) break;
      if (isAlpha(ch) || ch >= 128 || ch == '_' || (ch >= '0' && ch <= '9')) {
        if (str) (*str) += (char)ch;
        ++cpos;
        continue;
      }
      break;
    }
    /*
    // check for synonyms
    if (isNStrEqu(spos, cpos, "NULL")) return TK_Null;
    if (isNStrEqu(spos, cpos, "null")) return TK_Null;
    // look in tokens
    for (unsigned f = TK_Abstract; f < TK_URShiftAssign; ++f) {
      if (isNStrEqu(spos, cpos, TokenNames[f])) return (EToken)f;
    }
    */
    if (tkCanBeToken[(vuint8)src->FileStart[spos]]) {
      const unsigned tkslen = (unsigned)(cpos-spos);
      if (tkslen >= tkMinLen && tkslen <= tkMaxLen) {
        vassert(tkslen < MAX_IDENTIFIER_LENGTH);
        memcpy(tmpbuf, src->FileStart+spos, tkslen);
        tmpbuf[tkslen] = 0;
        const EToken tkk = FindToken(tmpbuf, tkslen);
        if (tkk != TK_NoToken && tkk < TK_URShiftAssign) return tkk;
      }
    }
    return TK_Identifier;
  }

  // now collect the longest punctuation
  #if 0
  EToken tkres = TK_NoToken;
  int spos = cpos;
  //fprintf(stderr, " delimstart=%d; ch=%c\n", cpos, ch);
  for (;;) {
    // look in tokens
    bool found = false;
    for (unsigned f = TK_URShiftAssign; f < TK_TotalTokenCount; ++f) {
      if (isNStrEqu(spos, cpos+1, TokenNames[f])) {
        tkres = (EToken)f;
        found = true;
        //fprintf(stderr, "  delimend=%d; tk=<%s>\n", cpos+1, TokenNames[f]);
        break;
      }
    }
    if (!found) {
      if (str) {
        while (spos < cpos) (*str) += (char)src->FileStart[spos++];
      }
      return tkres;
    }
    ++cpos;
  }
  #else
  tmpbuf[0] = (vuint8)src->FileStart[cpos++];
  tmpbuf[1] = 0;
  EToken etk = (EToken)tkCanBeSpecialToken[(vuint8)tmpbuf[0]];
  if (etk == TK_NoToken) return TK_NoToken;
  size_t tkbpos = 1;
  while (src->FileStart[cpos]) {
    tmpbuf[tkbpos++] = src->FileStart[cpos];
    tmpbuf[tkbpos] = 0;
    EToken ttk = FindToken(tmpbuf, tkbpos);
    if (ttk == TK_NoToken || ttk < TK_URShiftAssign) break; // cannot be longer
    ++cpos;
    etk = ttk;
  }
  return etk;
  #endif
}


//==========================================================================
//
//  VLexer::peekTokenType
//
//  this is freakin' slow, and won't cross "include" boundaries
//  offset==0 means "current token"
//  this doesn't process conditional directives,
//  so it is useful only for limited lookups
//
//==========================================================================
EToken VLexer::peekTokenType (int offset, VStr *tkstr) const {
  if (tkstr) tkstr->clear();
  if (offset < 0) return TK_NoToken;
  if (offset == 0) return Token;
  if (src->FilePtr >= src->FileEnd) return TK_NoToken; // no more
  EToken tkres = TK_NoToken;
  int cpos = (int)(src->FilePtr-src->FileStart)-1; // current char is eaten
  //fprintf(stderr, "cpos=%d\n", cpos);
  while (offset-- > 0) {
    tkres = skipTokenFrom(cpos, (offset == 0 ? tkstr : nullptr));
    //fprintf(stderr, "  cpos=%d; <%s>\n", cpos, TokenNames[tkres]);
    if (tkres == TK_NoToken) break; // EOS or some error
  }
  return tkres;
}
