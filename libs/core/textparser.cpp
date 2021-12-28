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
#include "core.h"


#define MAP_SIZE  (1024u)
#define MAP_MASK  (MAP_SIZE-1u)

struct FileNameItem {
  VStr fname;
  vuint32 hash;
  FileNameItem *next; // 0: end of bucket
};

static FileNameItem *fileNameBuckets[MAP_SIZE];
static unsigned fileNameCount = 0;

//   0: not inited
// 666: initalizing
//   1: locked
//   2: unlocked
static atomic_int bucketsInited = 0;


struct BucketLock {
  VV_DISABLE_COPY(BucketLock)

  VVA_FORCEINLINE BucketLock () noexcept {
    atomic_int v = atomic_cmp_xchg(&bucketsInited, 0, 666);
    if (!v) {
      // init
      fileNameCount = 0;
      for (unsigned f = 0; f < MAP_SIZE; ++f) fileNameBuckets[f] = nullptr;
      atomic_store(&bucketsInited, 1); // locked
      return;
    }
    while (v != 1) v = atomic_cmp_xchg(&bucketsInited, 2, 1);
  }

  VVA_FORCEINLINE ~BucketLock () noexcept {
    atomic_store(&bucketsInited, 2);
  }
};


//==========================================================================
//
//  ResetFileNamesInternal
//
//==========================================================================
static void ResetFileNamesInternal () noexcept {
  if (fileNameCount) {
    fileNameCount = 0;
    for (unsigned f = 0; f < MAP_SIZE; ++f) {
      FileNameItem *n = fileNameBuckets[f];
      while (n) {
        FileNameItem *c = n;
        n = n->next;
        delete c;
      }
      fileNameBuckets[f] = nullptr;
    }
  }
}


//==========================================================================
//
//  VTextLocation::GetFileNameCount
//
//==========================================================================
int VTextLocation::GetFileNameCount () noexcept {
  BucketLock lock;
  return (int)fileNameCount;
}


//==========================================================================
//
//  VTextLocation::ResetFileNames
//
//==========================================================================
void VTextLocation::ResetFileNames () noexcept {
  BucketLock lock;
  ResetFileNamesInternal();
}


//==========================================================================
//
//  VTextLocation::SetFileName
//
//==========================================================================
void VTextLocation::SetFileName (VStr fname) noexcept {
  if (fname.isEmpty()) { FileName.clear(); return; }
  BucketLock lock;
  vuint32 hash = joaatHashBuf(*fname, (size_t)fname.length());
  unsigned bnum = hash&MAP_MASK;
  FileNameItem *p = nullptr;
  FileNameItem *c = fileNameBuckets[bnum];
  while (c) {
    if (c->hash == hash && c->fname == fname) {
      // move to the top
      if (p) {
        p->next = c->next;
        c->next = fileNameBuckets[bnum];
        fileNameBuckets[bnum] = c;
      }
      FileName = c->fname;
      return;
    }
    p = c;
    c = c->next;
  }
  if (fileNameCount >= MAP_SIZE*3) ResetFileNamesInternal();
  // not found, add new
  c = new FileNameItem;
  c->fname = fname;
  c->hash = hash;
  c->next = fileNameBuckets[bnum];
  fileNameBuckets[bnum] = c;
  FileName = c->fname;
  ++fileNameCount;
}


//==========================================================================
//
//  VTextLocation::toString
//
//==========================================================================
VStr VTextLocation::toString () const noexcept {
  if (GetLine()) {
    if (GetCol() > 0) {
      return GetSourceFileStr()+VStr(GetLine())+":"+VStr(GetCol());
    } else {
      return GetSourceFileStr()+VStr(GetLine())+":1";
    }
  } else {
    return VStr("(nowhere)");
  }
}


//==========================================================================
//
//  VTextLocation::toStringNoCol
//
//==========================================================================
VStr VTextLocation::toStringNoCol () const noexcept {
  if (GetLine()) {
    return GetSourceFileStr()+VStr(GetLine());
  } else {
    return VStr("(nowhere)");
  }
}


//==========================================================================
//
//  VTextLocation::toStringLineCol
//
//==========================================================================
VStr VTextLocation::toStringLineCol () const noexcept {
  if (GetLine()) {
    if (GetCol() > 0) {
      return VStr(GetLine())+":"+VStr(GetCol());
    } else {
      return VStr(GetLine())+":1";
    }
  } else {
    return VStr("(nowhere)");
  }
}


//==========================================================================
//
//  VTextLocation::toStringShort
//
// only file name and line number
//
//==========================================================================
VStr VTextLocation::toStringShort () const noexcept {
  VStr s = GetSourceFile();
  int pos = s.length();
  while (pos > 0 && s[pos-1] != ':' && s[pos-1] != '/' && s[pos-1] != '\\') --pos;
  s = s.mid(pos, s.length());
  if (!s.isEmpty()) return s+":"+VStr(GetLine());
  return VStr(GetLine());
}



//==========================================================================
//
//  VTextParser::initParser
//
//==========================================================================
void VTextParser::initParser () noexcept {
  sourceOpen = false;
  currCh = EOF_CHARACTER;
  src = nullptr;
  Token = TK_EOF;
  TokenInt = 0;
  TokenFloat = 0.0f;
  userdata = nullptr;
  dgOpenFile = nullptr;
  dgFatalError = nullptr;
}


//==========================================================================
//
//  VTextParser::VTextParser
//
//==========================================================================
VTextParser::VTextParser () noexcept {
  initParser();
}


//==========================================================================
//
//  VTextParser::VTextParser
//
//==========================================================================
VTextParser::VTextParser (VStream *st, bool asEDGE) noexcept {
  initParser();
  if (st) {
    PushSource(st, st->GetName(), asEDGE);
    sourceOpen = true;
  }
  clearToken();
}


//==========================================================================
//
//  VTextParser::~VTextParser
//
//==========================================================================
VTextParser::~VTextParser () {
  while (src) PopSource();
  sourceOpen = false;
}


//==========================================================================
//
//  VTextParser::clearToken
//
//==========================================================================
void VTextParser::clearToken () noexcept {
  Token = TK_EOF;
  TokenStr.clear();
  TokenInt = 0;
  TokenFloat = 0.0f;
}


//==========================================================================
//
//  VTextParser::MsgDebug
//
//==========================================================================
void VTextParser::MsgDebug (const VTextLocation &loc, const char *msg) const {
  GLog.Logf(NAME_Debug, "%s: %s", *loc.toStringNoCol(), msg);
}


//==========================================================================
//
//  VTextParser::MsgWarning
//
//==========================================================================
void VTextParser::MsgWarning (const VTextLocation &loc, const char *msg) const {
  GLog.Logf(NAME_Warning, "%s: %s", *loc.toStringNoCol(), msg);
}


//==========================================================================
//
//  VTextParser::MsgError
//
//==========================================================================
void VTextParser::MsgError (const VTextLocation &loc, const char *msg) const {
  GLog.Logf(NAME_Error, "%s: %s", *loc.toStringNoCol(), msg);
}


//==========================================================================
//
//  VTextParser::FatalError
//
//==========================================================================
void VTextParser::FatalError (const VTextLocation &loc, const char *msg) {
  if (dgFatalError) {
    dgFatalError(this, loc, msg);
  } else {
    Sys_Error("%s: %s", *loc.toStringNoCol(), msg);
  }
  __builtin_trap();
}


//==========================================================================
//
//  VTextParser::AddDefine
//
//==========================================================================
void VTextParser::AddDefine (VStr CondName, bool showWarning) {
  if (CondName.length() == 0) return; // get lost!
  // check for redefined names
  for (auto &&ds : defines) {
    if (ds == CondName) {
      if (showWarning) MsgWarning(Location, va("Redefined conditional '%s'", *CondName));
      return;
    }
  }
  defines.Append(CondName);
}


//==========================================================================
//
//  VTextParser::RemoveDefine
//
//==========================================================================
void VTextParser::RemoveDefine (VStr CondName, bool showWarning) {
  if (CondName.length() == 0) return; // get lost!
  bool removed = false;
  int i = 0;
  while (i < defines.length()) {
    if (defines[i] == CondName) { removed = true; defines.removeAt(i); } else ++i;
  }
  if (showWarning && !removed) MsgWarning(Location, va("Undefined conditional '%s'", *CondName));
}


//==========================================================================
//
//  VTextParser::HasDefine
//
//==========================================================================
bool VTextParser::HasDefine (VStr CondName) noexcept {
  if (CondName.length() == 0) return false;
  for (auto &&ds : defines) if (ds == CondName) return true;
  return false;
}


//==========================================================================
//
//  VTextParser::doOpenFile
//
//  returns `null` if file not found
//  by default it tries to call `dgOpenFile()`, if it is specified,
//  otherwise falls back to standard vfs
//
//==========================================================================
VStream *VTextParser::doOpenFile (VStr filename) {
  if (filename.length() == 0) return nullptr; // just in case
  VStr fname = filename;
  #ifdef WIN32
  fname = fname.fixSlashes();
  #endif
  if (dgOpenFile) return dgOpenFile(this, fname);
  return nullptr;
}


//==========================================================================
//
//  VTextParser::OpenSource
//
//==========================================================================
void VTextParser::OpenSource (VStr FileName, bool asEDGE) {
  // read file and prepare for compilation
  PushSource(FileName, asEDGE);
  sourceOpen = true;
  clearToken();
}


//==========================================================================
//
//  VTextParser::OpenSource
//
//==========================================================================
void VTextParser::OpenSource (VStream *astream, VStr FileName, bool asEDGE) {
  // read file and prepare for compilation
  PushSource(astream, FileName, asEDGE);
  sourceOpen = true;
  clearToken();
}


//==========================================================================
//
//  VTextParser::PushSource
//
//==========================================================================
void VTextParser::PushSource (VStr FileName, bool asEDGE) {
  VStream *strm = doOpenFile(FileName);
  if (!strm) FatalError(Location, va("cannot open file '%s'", *FileName));
  PushSource(strm, FileName, asEDGE);
}


//==========================================================================
//
//  VTextParser::PushSource
//
//==========================================================================
void VTextParser::PushSource (VStream *Strm, VStr FileName, bool asEDGE) {
  if (!Strm) FatalError(Location, va("no stream provided for file '%s'", *FileName));
  if (FileName.isEmpty()) FileName = Strm->GetName();

  int count = 0;
  for (const VSourceFile *s = src; s; s = s->Next) ++count;
  if (count >= 64) FatalError(Location, "too many nested includes");

  VSourceFile *NewSrc;
  int FileSize;
  try {
    NewSrc = new VSourceFile();
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
    FileSize = Strm->TotalSize();
    if (Strm->IsError() || FileSize < 0) { VStream::Destroy(Strm); FatalError(VTextLocation(), va("VC: Couldn't read '%s'", *FileName)); return; }
    NewSrc->FileStart = new char[FileSize+1];
    Strm->Serialise(NewSrc->FileStart, FileSize);
    if (Strm->IsError() || FileSize < 0) { VStream::Destroy(Strm); FatalError(VTextLocation(), va("VC: Couldn't read '%s'", *FileName)); return; }
  } catch (...) {
    VStream::Destroy(Strm);
    throw;
  }
  VStream::Destroy(Strm);

  NewSrc->FileStart[FileSize] = 0; // this is not really required, but let's make the whole buffer initialized
  NewSrc->FileEnd = NewSrc->FileStart+FileSize;
  NewSrc->FilePtr = NewSrc->FileStart;
  NewSrc->FileText = NewSrc->FileStart;
  NewSrc->EDGEMode = asEDGE;
  NewSrc->SignedNumbers = false;

  // skip garbage some editors add in the begining of UTF-8 files (BOM)
  if (FileSize >= 3 && (vuint8)NewSrc->FilePtr[0] == 0xef && (vuint8)NewSrc->FilePtr[1] == 0xbb && (vuint8)NewSrc->FilePtr[2] == 0xbf) {
    NewSrc->FileStart += 3;
    NewSrc->FilePtr += 3;
  }

  // save current character and location to be able to restore them
  NewSrc->currCh = currCh;
  NewSrc->Loc = Location;
  NewSrc->CurrChLoc = CurrChLocation;

  NewSrc->IncLineNumber = false;
  NewSrc->Skipping = false;
  Location = VTextLocation(FileName, 1, 1);
  CurrChLocation = VTextLocation(FileName, 1, 0); // 0, 'cause `NextChr()` will do `ConsumeChar()`
  NextChr();
}


//==========================================================================
//
//  VTextParser::PopSource
//
//==========================================================================
void VTextParser::PopSource () {
  if (!src) return;
  if (src->IfStates.length()) FatalError(Location, "#ifdef without a corresponding #endif");
  VSourceFile *Tmp = src;
  delete[] Tmp->FileText;
  Tmp->FileText = Tmp->FileStart = nullptr;
  src = Tmp->Next;
  currCh = Tmp->currCh;
  Location = Tmp->Loc;
  CurrChLocation = Tmp->CurrChLoc;
  delete Tmp;
}


//==========================================================================
//
//  VTextParser::NextToken
//
//==========================================================================
void VTextParser::NextToken () {
  if (!src) { clearToken(); return; }
  bool inNewLine = (src->FilePtr == src->FileStart);
  for (;;) {
    clearToken();
    if (SkipWhitespaceAndComments()) inNewLine = true;
    Location = CurrChLocation;

    //GLog.Logf(NAME_Debug, "***%s: inNewLine=%d; ch=%c (%d)", *Location.toStringNoCol(), inNewLine, ((vuint8)currCh <= ' ' || currCh == 127 ? '.' : currCh), currCh);

    if (currCh == EOF_CHARACTER) {
      PopSource();
      clearToken();
      if (!src) break;
      inNewLine = true;
      continue;
    }

    // check for preprocessor directive
    if (inNewLine && currCh == '#') {
      //GLog.Logf(NAME_Debug, "***%s: PRE!", *Location.toStringNoCol());
      ProcessPreprocessor();
      inNewLine = true; // preprocessor skips the line
      clearToken();
    } else {
      inNewLine = false; // not a preprocessor

           if (isIdChar(currCh)) ProcessLetterToken();
      else if (isDigit(currCh)) ProcessNumberToken();
      else if (currCh == '"') ProcessQuoteToken();
      else if (currCh == '\'') ProcessSingleQuoteToken();
      else ProcessSpecialToken();

      //GLog.Logf(NAME_Debug, "+++%s: skipping=%d; tktype=%s; inNewLine=%d; ch=%c (%d)", *CurrChLocation.toStringNoCol(), (int)src->Skipping, GetTokenTypeName(), inNewLine, ((vuint8)currCh <= ' ' || currCh == 127 ? '.' : currCh), currCh);
      if (src->Skipping || Token == TK_EOF) continue;
      break;
    }
  }
}


//==========================================================================
//
//  VTextParser::NextChr
//
//  read next character into `currCh`
//
//==========================================================================
void VTextParser::NextChr () {
  if (src->FilePtr >= src->FileEnd) {
    currCh = EOF_CHARACTER;
    return;
  }
  CurrChLocation.ConsumeChar(src->IncLineNumber);
  src->IncLineNumber = false;
  currCh = *src->FilePtr++;
  if ((vuint8)currCh < ' ' || (vuint8)currCh == EOF_CHARACTER) {
    if (currCh == EOL_CHARACTER) {
      src->IncLineNumber = true;
    } else {
      currCh = ' ';
    }
  }
}


//==========================================================================
//
//  VTextParser::Peek
//
//  0 is "currCh"
//
//==========================================================================
char VTextParser::Peek (int dist) const noexcept {
  //if (dist < 0) FatalError(Location, "VC INTERNAL COMPILER ERROR: peek dist is negative!");
  if (dist < 0) return ' ';
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
//  VTextParser::SkipComment
//
//  this skips comment, `currCh` is the first non-comment char
//  at enter, `currCh` must be the first comment char
//  returns non-zero if some comment was skipped
//  if positive, there was a newline
//
//==========================================================================
int VTextParser::SkipComment () {
  if (currCh != '/') return 0;
  char c1 = Peek(1);

  // single-line?
  if (c1 == '/') {
    NextChr();
    vassert(currCh == '/');
    do {
      NextChr();
    } while (currCh != EOL_CHARACTER && currCh != EOF_CHARACTER);
    if (currCh == EOL_CHARACTER) {
      NextChr();
      return 1;
    }
    return -1;
  }

  // multiline?
  if (c1 == '*') {
    int res = -1;
    NextChr();
    vassert(currCh == '*');
    for (;;) {
      NextChr();
      if (currCh == EOF_CHARACTER) FatalError(Location, "End of file inside a comment");
      if (currCh == '*' && Peek(1) == '/') {
        NextChr();
        vassert(currCh == '/');
        break;
      }
      if (currCh == EOL_CHARACTER) res = 1;
    }
    NextChr();
    return res;
  }

  // multiline, nested?
  if (c1 == '+') {
    int res = -1;
    NextChr();
    vassert(currCh == '+');
    int level = 1;
    for (;;) {
      NextChr();
      if (currCh == EOF_CHARACTER) FatalError(Location, "End of file inside a comment");
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
      if (currCh == EOL_CHARACTER) res = 1;
    }
    NextChr();
    return res;
  }

  // not a comment
  return 0;
}


//==========================================================================
//
//  VTextParser::SkipWhitespaceAndComments
//
//  returns `true` if newline was skipped
//
//==========================================================================
bool VTextParser::SkipWhitespaceAndComments () {
  if (!src || currCh == EOF_CHARACTER) return false;
  bool wasNL = false;
  Location = CurrChLocation;
  while (currCh != EOF_CHARACTER) {
    const int cmt = SkipComment();
    if (cmt) { wasNL = (wasNL || cmt > 0); continue; }
    wasNL = (wasNL || currCh == EOL_CHARACTER);
    if (!isBlankChar(currCh)) break;
    NextChr();
  }
  return wasNL;
}


//==========================================================================
//
//  VTextParser::peekNextNonBlankChar
//
//==========================================================================
char VTextParser::peekNextNonBlankChar () const noexcept {
  const char *fpos = src->FilePtr;
  if (fpos > src->FileStart) --fpos; // unget last char
  while (fpos < src->FileEnd) {
    char ch = *fpos++;
    if (isBlankChar(ch)) continue;
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
//  VTextParser::SkipCurrentLine
//
//  skip current line, correctly process comments
//  returns `true` if no non-whitespace and non-comment chars were seen
//  used to skip lines in preprocessor
//  doesn't skip EOL char
//
//==========================================================================
bool VTextParser::SkipCurrentLine () {
  if (currCh == EOL_CHARACTER || currCh == EOF_CHARACTER) return true;
  bool seenBadChars = false;
  while (currCh != EOF_CHARACTER) {
    if (currCh == EOL_CHARACTER) break;
    const int cmt = SkipComment();
    if (cmt) {
      if (cmt > 0) break;
      continue;
    }
    seenBadChars = (seenBadChars || !isBlankChar(currCh));
    NextChr();
  }
  return !seenBadChars;
}


//==========================================================================
//
//  VTextParser::ProcessPreprocessor
//
//==========================================================================
void VTextParser::ProcessPreprocessor () {
  NextChr(); // skip '#'
  while (currCh != EOF_CHARACTER && currCh != EOL_CHARACTER && isBlankChar(currCh)) NextChr();

  if (currCh == EOL_CHARACTER || currCh == EOF_CHARACTER) {
    if (src->Skipping) {
      (void)SkipCurrentLine();
    } else {
      FatalError(Location, "Compiler directive expected");
    }
    return;
  }

  if (!isIdChar(currCh)) {
    if (src->Skipping) {
      (void)SkipCurrentLine();
    } else {
      FatalError(Location, "Compiler directive expected");
      while (currCh != EOL_CHARACTER && currCh != EOF_CHARACTER) NextChr();
    }
    return;
  }

  ProcessLetterToken();
  VStr dname(TokenStr);

       if (dname.strEqu("line")) ProcessLineDirective();
  else if (dname.strEqu("define")) ProcessDefine();
  else if (dname.strEqu("undef")) ProcessUndef();
  else if (dname.strEqu("ifdef")) ProcessIfDef(true);
  else if (dname.strEqu("ifndef")) ProcessIfDef(false);
  else if (dname.strEqu("if")) ProcessIf();
  else if (dname.strEqu("else")) ProcessElse();
  else if (dname.strEqu("endif")) ProcessEndIf();
  else if (dname.strEqu("include")) { ProcessInclude(); clearToken(); return; }
  else ProcessUnknownDirective();

  clearToken();

  if (src->Skipping) {
    (void)SkipCurrentLine();
  } else {
    if (!SkipCurrentLine()) FatalError(Location, va("Compiler directive `%s` contains extra code", *dname));
  }
}


//==========================================================================
//
//  VTextParser::ProcessUnknownDirective
//
//==========================================================================
void VTextParser::ProcessUnknownDirective () {
  if (!src->Skipping) FatalError(Location, va("Unknown compiler directive `%s`", *TokenStr));
}


//==========================================================================
//
//  VTextParser::ProcessLineDirective
//
//==========================================================================
void VTextParser::ProcessLineDirective () {
  if (src->Skipping) return;

  // read line number
  bool wasNL = SkipWhitespaceAndComments();
  if (wasNL || !isDigit(currCh)) FatalError(Location, "`#line`: line number expected");
  ProcessNumberToken();
  if (Token != TK_Int) FatalError(Location, "`#line`: integer expected");
  if (TokenInt < 1) FatalError(Location, "`#line`: positive integer expected");
  auto lno = TokenInt;

  // read file name (optional)
  VStr fname = Location.GetSourceFile();
  wasNL = SkipWhitespaceAndComments();
  if (!wasNL && currCh != EOF_CHARACTER) {
    (void)ProcessFileName("#line");
    fname = TokenStr;
  }

  // ignore flags
  (void)SkipCurrentLine();

  if (currCh != EOF_CHARACTER) {
    Location = VTextLocation(fname, lno, 1);
    CurrChLocation = Location;
  }
}


//==========================================================================
//
//  VTextParser::ProcessDefine
//
//==========================================================================
void VTextParser::ProcessDefine () {
  if (src->Skipping) return;
  const bool wasNL = SkipWhitespaceAndComments();

  // argument to the #define must be on the same line
  if (wasNL || currCh == EOF_CHARACTER) {
    FatalError(Location, "`#define`: missing argument");
    return;
  }

  // parse name to be defined
  if (!isIdChar(currCh)) FatalError(Location, "`#define`: invalid define name");
  ProcessLetterToken();

  //if (src->Skipping) return;
  AddDefine(TokenStr);
}


//==========================================================================
//
//  VTextParser::ProcessUndef
//
//==========================================================================
void VTextParser::ProcessUndef () {
  if (src->Skipping) return;
  const bool wasNL = SkipWhitespaceAndComments();

  // argument to the #undef must be on the same line
  if (wasNL || currCh == EOF_CHARACTER) {
    FatalError(Location, "`#undef`: missing argument");
    return;
  }

  // parse name to be defined
  if (!isIdChar(currCh)) FatalError(Location, "`#undef`: invalid define name");
  ProcessLetterToken();

  //if (src->Skipping) return;
  RemoveDefine(TokenStr);
}


//==========================================================================
//
//  VTextParser::ProcessIfDef
//
//==========================================================================
void VTextParser::ProcessIfDef (bool OnTrue) {
  const char *ifname = (OnTrue ? "#ifdef" : "#ifndef");

  // if we're skipping now, don't bother
  if (src->Skipping) {
    src->IfStates.Append(IF_Skip);
    return;
  }

  const bool wasNL = SkipWhitespaceAndComments();

  // argument to the #ifdef must be on the same line
  if (wasNL || currCh == EOF_CHARACTER) {
    FatalError(Location, va("`%s`: missing argument", ifname));
    return;
  }

  // parse condition name
  if (!isIdChar(currCh)) FatalError(Location, va("`%s`: invalid argument", ifname));
  ProcessLetterToken();

  // check if the name has been defined
  bool found = HasDefine(TokenStr);
  if (found == OnTrue) {
    src->IfStates.Append(IF_True);
  } else {
    src->IfStates.Append(IF_False);
    src->Skipping = true;
  }
}


//==========================================================================
//
//  VTextParser::ProcessIfTerm
//
//  result is <0: error
//
//==========================================================================
int VTextParser::ProcessIfTerm () {
  bool wasNL = SkipWhitespaceAndComments();
  if (wasNL || currCh == EOF_CHARACTER) {
    FatalError(Location, "`#if`: missing argument");
    return -1;
  }
  // number?
  if (isDigit(currCh)) {
    ProcessNumberToken();
    if (Token != TK_Int) {
      FatalError(Location, "`#if`: integer expected");
      return -1;
    }
    return (TokenInt ? 1 : 0);
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
      wasNL = SkipWhitespaceAndComments();
      if (wasNL || currCh != ')') {
        FatalError(Location, "`#if`: `)` expected");
        return -1;
      }
      NextChr();
    }
    return res;
  }
  // `defined`?
  if (currCh == 'd') {
    ProcessLetterToken();
    if (!TokenStr.strEqu("defined")) {
      FatalError(Location, "`#if`: `defined` expected");
      return -1;
    }
    wasNL = SkipWhitespaceAndComments();
    if (wasNL || currCh != '(') {
      FatalError(Location, "`#if`: `(` expected");
      return -1;
    }
    NextChr();
    wasNL = SkipWhitespaceAndComments();
    if (wasNL || !isIdChar(currCh)) {
      FatalError(Location, "`#if`: identifier expected");
      return -1;
    }
    ProcessLetterToken();
    int res = (HasDefine(TokenStr) ? 1 : 0);
    wasNL = SkipWhitespaceAndComments();
    if (wasNL || currCh != ')') {
      FatalError(Location, "`#if`: `)` expected");
      return -1;
    }
    NextChr();
    return res;
  }
  // `option`?
  if (currCh == 'o') {
    ProcessLetterToken();
    if (!TokenStr.strEqu("option")) {
      FatalError(Location, "`#if`: `option` expected");
      return -1;
    }
    wasNL = SkipWhitespaceAndComments();
    if (wasNL || currCh != '(') {
      FatalError(Location, "`#if`: `(` expected");
      return -1;
    }
    NextChr();
    wasNL = SkipWhitespaceAndComments();
    if (wasNL || !isIdChar(currCh)) {
      FatalError(Location, "`#if`: identifier expected");
      return -1;
    }
    ProcessLetterToken();
    int res = 0;
    /*
         if (TokenStr.strEquCI("cilocals")) res = !VObject::cliCaseSensitiveLocals;
    else if (TokenStr.strEquCI("cslocals")) res = VObject::cliCaseSensitiveLocals;
    else if (TokenStr.strEquCI("cifields")) res = !VObject::cliCaseSensitiveFields;
    else if (TokenStr.strEquCI("csfields")) res = VObject::cliCaseSensitiveFields;
    else */FatalError(Location, va("`#if`: unknown option `%s`", *TokenStr));
    wasNL = SkipWhitespaceAndComments();
    if (wasNL || currCh != ')') {
      FatalError(Location, "`#if`: `)` expected");
      return -1;
    }
    NextChr();
    return res;
  }
  // other
  FatalError(Location, "`#if`: unexpected token");
  return -1;
}


//==========================================================================
//
//  VTextParser::ProcessIfLAnd
//
//  result is <0: error
//
//==========================================================================
int VTextParser::ProcessIfLAnd () {
  int res = ProcessIfTerm();
  if (res < 0) return res;
  for (;;) {
    const bool wasNL = SkipWhitespaceAndComments();
    if (wasNL || currCh == EOF_CHARACTER) return res;
    if (currCh != '&') return res;
    NextChr();
    if (currCh != '&') {
      FatalError(Location, "`#if`: `&&` expected");
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
//  VTextParser::ProcessIfLOr
//
//  result is <0: error
//
//==========================================================================
int VTextParser::ProcessIfLOr () {
  int res = ProcessIfLAnd();
  if (res < 0) return res;
  for (;;) {
    const bool wasNL = SkipWhitespaceAndComments();
    if (wasNL || currCh == EOF_CHARACTER) return res;
    if (currCh != '|') return res;
    NextChr();
    if (currCh != '|') {
      FatalError(Location, "`#if`: `||` expected");
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
//  VTextParser::ProcessIfExpr
//
//  result is <0: error
//
//==========================================================================
int VTextParser::ProcessIfExpr () {
  return ProcessIfLOr();
}


//==========================================================================
//
//  VTextParser::ProcessIf
//
//==========================================================================
void VTextParser::ProcessIf () {
  // if we're skipping now, don't bother
  if (src->Skipping) {
    src->IfStates.Append(IF_Skip);
    return;
  }

  const bool wasNL = SkipWhitespaceAndComments();
  if (wasNL) { FatalError(Location, "invalid '#if'"); return; }

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
//  VTextParser::ProcessElse
//
//==========================================================================
void VTextParser::ProcessElse () {
  if (!src->IfStates.length()) {
    FatalError(Location, "`#else` without an `#if`");
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
      FatalError(Location, "Multiple `#else` directives for a single `#ifdef`");
      src->Skipping = true;
      break;
  }
}


//==========================================================================
//
//  VTextParser::ProcessEndIf
//
//==========================================================================
void VTextParser::ProcessEndIf () {
  if (!src->IfStates.length()) {
    FatalError(Location, "`#endif` without an `#if`");
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
//  VTextParser::OpenIncludeStream
//
//==========================================================================
VStream *VTextParser::OpenIncludeStream (VStr filename, bool asSystem) {
  if (filename.isEmpty()) return nullptr;
  if (dgOpenIncludeStream) return dgOpenIncludeStream(this, filename, src->Path, asSystem);

  // check if it's an absolute path location
  if (filename.isAbsolutePath()) return doOpenFile(filename);

  // first try relative to the current source file
  if (src->Path.IsNotEmpty()) {
    VStr fullname = src->Path.appendPath(filename);
    VStream *Strm = doOpenFile(fullname);
    if (Strm) return Strm;
  }

  for (int i = includePath.length()-1; i >= 0; --i) {
    VStr fullname = includePath[i].appendPath(filename);
    VStream *Strm = doOpenFile(fullname);
    if (Strm) return Strm;
  }

  return nullptr;
}


//==========================================================================
//
//  VTextParser::ProcessInclude
//
//==========================================================================
void VTextParser::ProcessInclude () {
  if (src->Skipping) { (void)SkipCurrentLine(); return; }
  const bool wasNL = SkipWhitespaceAndComments();

  // file name must be on the same line
  if (wasNL || currCh == EOF_CHARACTER) FatalError(Location, "`#include`: file name expected");

  // parse file name
  const bool asSystem = ProcessFileName("#include");
  VStream *Strm = OpenIncludeStream(TokenStr, asSystem);
  if (!Strm) FatalError(Location, va("%sinclude file '%s' not found", (asSystem ? "system " : ""), *TokenStr));

  // a new-line is expected at the end of preprocessor directive
  if (!SkipCurrentLine()) {
    VStream::Destroy(Strm);
    FatalError(Location, "`#include`: no extra arguments allowed");
  }

  PushSource(Strm, TokenStr);
}


//==========================================================================
//
//  VTextParser::AddIncludePath
//
//==========================================================================
void VTextParser::AddIncludePath (VStr DirName) {
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
//  VTextParser::ProcessNumberToken
//
//==========================================================================
void VTextParser::ProcessNumberToken () {
  clearToken();
  Token = TK_Int;
  // collect number into buffer, because we have to call a proper parser for floats
  bool isHex = false;
  char numbuf[256];
  unsigned nbpos = 1; // [0] will be `currCh`
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
      TokenStr = "0";
      TokenStr += currCh;
      NextChr(); // skip radix char
      unsigned ui = 0;
      for (;;) {
        if (currCh != '_') {
          int d = VStr::digitInBase(currCh, base);
          if (d < 0) break;
          //TokenInt = TokenInt*base+d;
          const unsigned nui = ui*(unsigned)base+(unsigned)d;
          if (nui < ui) FatalError(Location, "Integer overflow");
          ui = nui;
        }
        TokenStr += currCh;
        NextChr();
      }
      TokenInt = (int)ui;
      TokenFloat = TokenInt;
      if (isIdDigit(currCh)) FatalError(Location, "Invalid number");
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
      if (nbpos >= sizeof(numbuf)-1) FatalError(Location, "Invalid number");
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
      numbuf[nbpos++] = 0; // we always have the room for the trailing zero
      if (!VStr::convertInt(numbuf, &TokenInt)) FatalError(Location, "Invalid floating number");
      TokenFloat = TokenInt;
      TokenStr = VStr(numbuf);
      return;
    }
  }

  // float number?
  if (currCh == 'f' || currCh == '.' ||
      (isHex && (currCh == 'p' || currCh == 'P')) ||
      (!isHex && (currCh == 'e' || currCh == 'E')))
  {
    // float number
    Token = TK_Float;

    // dotted part
    if (currCh == '.') {
      if (nbpos >= sizeof(numbuf)-1) FatalError(Location, "Invalid number");
      numbuf[nbpos++] = '.';
      NextChr(); // skip dot
      for (;;) {
        if (currCh != '_') {
          int d = VStr::digitInBase(currCh, xbase);
          if (d < 0) break;
          if (nbpos >= sizeof(numbuf)-1) FatalError(Location, "Invalid number");
          numbuf[nbpos++] = currCh;
        }
        NextChr();
      }
    }

    // exponent
    if ((isHex && (currCh == 'p' || currCh == 'P')) ||
        (!isHex && (currCh == 'e' || currCh == 'E')))
    {
      if (nbpos >= sizeof(numbuf)-1) FatalError(Location, "Invalid floating number exponent");
      numbuf[nbpos++] = (isHex ? 'p' : 'e');
      NextChr(); // skip e/p
      if (currCh == '+' || currCh == '-') {
        if (nbpos >= sizeof(numbuf)-1) FatalError(Location, "Invalid floating number exponent");
        numbuf[nbpos++] = currCh;
        NextChr(); // skip sign
      }
      for (;;) {
        if (currCh != '_') {
          int d = VStr::digitInBase(currCh, 10/*xbase*/); // it is decimal for both types
          if (d < 0) break;
          if (nbpos >= sizeof(numbuf)-1) { FatalError(Location, "Invalid floating number exponent"); nbpos = (unsigned)(sizeof(numbuf)-1); }
          numbuf[nbpos++] = currCh;
        }
        NextChr();
      }
    }

    // skip optional 'f'
    if (currCh == 'f') NextChr();

    if (isIdDigit(currCh)) FatalError(Location, "Invalid floating number");
    if (nbpos >= sizeof(numbuf)-1) { FatalError(Location, "Invalid floating number"); nbpos = (unsigned)(sizeof(numbuf)-1); }
    numbuf[nbpos++] = 0; // we always have the room for trailing zero
    if (!VStr::convertFloat(numbuf, &TokenFloat)) FatalError(Location, "Invalid floating number");
    TokenInt = (int)TokenFloat;
    TokenStr = VStr(numbuf);
  } else {
    // not a float
    if (isIdDigit(currCh)) FatalError(Location, "Invalid integer number");
    if (nbpos >= sizeof(numbuf)-1) { FatalError(Location, "Invalid integer number"); nbpos = (unsigned)(sizeof(numbuf)-1); }
    numbuf[nbpos++] = 0; // we always have the room for trailing zero
    if (!VStr::convertInt(numbuf, &TokenInt)) FatalError(Location, "Invalid integer number");
    TokenFloat = TokenInt;
    TokenStr = VStr(numbuf);
  }
}


//==========================================================================
//
//  VTextParser::ProcessChar
//
//==========================================================================
void VTextParser::ProcessChar () {
  if (currCh == EOF_CHARACTER || currCh == EOL_CHARACTER) FatalError(Location, "Unterminated string");
  if (currCh == '\\') {
    // special symbol
    NextChr();
    if (currCh == EOF_CHARACTER || currCh == EOL_CHARACTER) FatalError(Location, "Unterminated string");
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
          if (currCh == EOF_CHARACTER || currCh == EOL_CHARACTER) FatalError(Location, "Unterminated string");
          int n = VStr::digitInBase(currCh, 16);
          if (n < 0) FatalError(Location, "Unterminated string");
          // second digit
          if (src->FilePtr < src->FileEnd && VStr::digitInBase(*src->FilePtr, 16)) {
            NextChr();
            n = n*16+VStr::digitInBase(currCh, 16);
          }
          currCh = (char)n;
        }
        break;
      default: FatalError(Location, "Unknown escape char");
    }
  }
}


//==========================================================================
//
//  VTextParser::ProcessQuoted
//
//==========================================================================
void VTextParser::ProcessQuoted (const char ech) {
  clearToken();
  NextChr();
  while (currCh != ech) {
    ProcessChar(); // this also checks for newline
    TokenStr += currCh;
    NextChr();
  }
  NextChr();
}


//==========================================================================
//
//  VTextParser::ProcessQuoteToken
//
//==========================================================================
void VTextParser::ProcessQuoteToken () {
  ProcessQuoted('"');
  Token = TK_String;
}


//==========================================================================
//
//  VTextParser::ProcessSingleQuoteToken
//
//==========================================================================
void VTextParser::ProcessSingleQuoteToken () {
  ProcessQuoted('\'');
  Token = TK_Name;
}


//==========================================================================
//
//  VTextParser::ProcessLetterToken
//
//==========================================================================
void VTextParser::ProcessLetterToken () {
  Token = TK_Id;
  TokenStr.clear();
  while (isIdDigit(currCh)) {
    if (currCh != '_' || !src->EDGEMode) TokenStr += currCh;
    NextChr();
  }
}


//==========================================================================
//
//  VTextParser::ProcessSpecialToken
//
//==========================================================================
void VTextParser::ProcessSpecialToken () {
  const char ch = currCh;
  TokenStr.clear();
  if ((ch == '+' || ch == '-') && src->SignedNumbers && isDigit(peekNextNonBlankChar())) {
    const bool neg = (ch == '-');
    NextChr(); // skip sign char
    (void)SkipWhitespaceAndComments();
    ProcessNumberToken();
    vassert(Token == TK_Int || Token == TK_Float);
    if (neg) {
      TokenInt = -TokenInt;
      TokenFloat = -TokenFloat;
    }
    return;
  }
  Token = TK_Delim;
  TokenStr += currCh;
  NextChr();
  switch (ch) {
    case '<':
      switch (currCh) {
        case '<':
          TokenStr += '<';
          NextChr();
          if (currCh == '<') {
            TokenStr += '<';
            NextChr();
          }
          break;
        case '=':
          TokenStr += currCh;
          NextChr();
          break;
      }
      break;
    case '>':
      switch (currCh) {
        case '>':
          TokenStr += '>';
          NextChr();
          if (currCh == '>') {
            TokenStr += '>';
            NextChr();
          }
          break;
        case '=':
          TokenStr += currCh;
          NextChr();
          break;
      }
      break;
    case '=':
    case '!':
      if (currCh == '=') {
        TokenStr += currCh;
        NextChr();
      }
      break;
    case '.':
      if (currCh == '.') {
        TokenStr += '.';
        NextChr();
        if (currCh == '.') {
          TokenStr += '.';
          NextChr();
        }
      }
      break;
  }
}


//==========================================================================
//
//  VTextParser::ProcessFileName
//
//  returns `true` if this is a system include
//
//==========================================================================
bool VTextParser::ProcessFileName (const char *pdname) {
  clearToken();
  bool asSys = false;
  char ech = '"';
  if (currCh == '"') {
    asSys = false;
    ech = '"';
  } else if (currCh == '<') {
    asSys = true;
    ech = '>';
  } else {
    FatalError(Location, va("`%s`: file name expected", pdname));
  }
  Token = TK_String;
  NextChr();
  while (currCh != ech) {
    if (currCh == '\\') {
      currCh = '/';
      char ch = Peek(1);
      if (ch == '\\') {
        NextChr();
        vassert(currCh == '\\');
        currCh = '/';
      } else {
        FatalError(Location, va("'%s' got invalid escaping in file name", pdname));
      }
    }
    if (currCh == EOF_CHARACTER || currCh == EOL_CHARACTER) FatalError(Location, va("'%s' file name is not terminated", pdname));
    TokenStr += currCh;
    NextChr();
  }
  NextChr();
  if (TokenStr.isEmpty()) FatalError(Location, va("`%s` expects non-Empty file name", pdname));
  return asSys;
}


//==========================================================================
//
//  VTextParser::isNStrEqu
//
//==========================================================================
bool VTextParser::isNStrEqu (int spos, int epos, const char *s) const noexcept {
  if (!s) s = "";
  if (spos >= epos) return (s[0] == 0);
  if (spos < 0 || epos > src->FileEnd-src->FileStart) return false;
  auto slen = (int)strlen(s);
  if (epos-spos != slen) return false;
  return (memcmp(src->FileStart+spos, s, slen) == 0);
}


//==========================================================================
//
//  VTextParser::posAtEOS
//
//==========================================================================
bool VTextParser::posAtEOS (int cpos) const {
  if (cpos < 0) cpos = 0;
  return (cpos >= src->FileEnd-src->FileStart);
}


//==========================================================================
//
//  VTextParser::peekChar
//
//  returns 0 on EOS
//
//==========================================================================
vuint8 VTextParser::peekChar (int cpos) const {
  if (cpos < 0) cpos = 0;
  if (cpos >= src->FileEnd-src->FileStart) return 0;
  vuint8 ch = src->FileStart[cpos];
  if (ch == 0) ch = ' ';
  return ch;
}


//==========================================================================
//
//  VTextParser::skipBlanksFrom
//
//  returns `false` on EOS
//
//==========================================================================
bool VTextParser::skipBlanksFrom (int &cpos) const {
  if (cpos < 0) cpos = 0; // just in case
  for (;;) {
    vuint8 ch = peekChar(cpos);
    if (!ch) break; // EOS
    if (isBlankChar(ch)) { ++cpos; continue; }
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
        if (ch == EOL_CHARACTER) break;
      }
      continue;
    }
    return true; // nonblank
  }
  return false;
}


//==========================================================================
//
//  VTextParser::ExpectIdInternal
//
//==========================================================================
void VTextParser::ExpectIdInternal (const char *s, bool caseSens) {
  if (!s) s = "";
  if (Token != TK_Id) FatalError(Location, va("identifier `%s` expected", s));
  if (s[0]) {
    const bool ok = (caseSens ? TokenStr.strEqu(s) : TokenStr.strEquCI(s));
    if (!ok) FatalError(Location, va("identifier `%s` expected, but got `%s`", s, *TokenStr));
  }
  NextToken();
}


//==========================================================================
//
//  VTextParser::ExpectDelim
//
//==========================================================================
void VTextParser::ExpectDelim (const char *s) {
  if (Token != TK_Delim) FatalError(Location, va("delimiter `%s` expected", (s ? s : "")));
  if (s && s[0]) {
    if (!TokenStr.strEqu(s)) FatalError(Location, va("delimiter `%s` expected, but got `%s`", s, *TokenStr));
  }
  NextToken();
}


//==========================================================================
//
//  VTextParser::ExpectId
//
//==========================================================================
VStr VTextParser::ExpectId () {
  if (Token != TK_Id) FatalError(Location, "identifier expected");
  VStr res = TokenStr;
  NextToken();
  return res;
}


//==========================================================================
//
//  VTextParser::ExpectString
//
//==========================================================================
VStr VTextParser::ExpectString () {
  if (Token != TK_String) FatalError(Location, "string expected");
  VStr res = TokenStr;
  NextToken();
  return res;
}


//==========================================================================
//
//  VTextParser::ExpectName
//
//==========================================================================
VStr VTextParser::ExpectName () {
  if (Token != TK_Name) FatalError(Location, "name expected");
  VStr res = TokenStr;
  NextToken();
  return res;
}


//==========================================================================
//
//  VTextParser::ExpectInt
//
//==========================================================================
int VTextParser::ExpectInt () {
  if (Token != TK_Int) FatalError(Location, "integer expected");
  const int res = TokenInt;
  NextToken();
  return res;
}


//==========================================================================
//
//  VTextParser::IsPossibleSignedNumber
//
//==========================================================================
bool VTextParser::IsPossibleSignedNumber () const noexcept {
  if (Token != TK_Delim) return false;
  if (TokenStr != "+" && TokenStr != "-") return false;
  const char ch = peekNextNonBlankChar();
  return isDigit(ch);
}


//==========================================================================
//
//  VTextParser::ExpectSignedInt
//
//==========================================================================
int VTextParser::ExpectSignedInt () {
  bool neg = false;
  if (Token == TK_Delim) {
         if (TokenStr == "+") NextToken();
    else if (TokenStr == "-") { neg = true; NextToken(); }
  }
  if (Token != TK_Int) FatalError(Location, "integer expected");
  int res = TokenInt;
  NextToken();
  if (neg) res = -res;
  return res;
}


//==========================================================================
//
//  VTextParser::ExpectFloat
//
//==========================================================================
float VTextParser::ExpectFloat () {
  if (Token != TK_Float) FatalError(Location, "integer expected");
  const float res = TokenFloat;
  NextToken();
  return res;
}


//==========================================================================
//
//  VTextParser::ExpectSignedFloat
//
//==========================================================================
float VTextParser::ExpectSignedFloat () {
  bool neg = false;
  if (Token == TK_Delim) {
         if (TokenStr == "+") NextToken();
    else if (TokenStr == "-") { neg = true; NextToken(); }
  }
  if (Token != TK_Float) FatalError(Location, "integer expected");
  float res = TokenFloat;
  NextToken();
  if (neg) res = -res;
  return res;
}


//==========================================================================
//
//  VTextParser::ExpectFloatAllowInt
//
//==========================================================================
float VTextParser::ExpectFloatAllowInt () {
  if (Token != TK_Float && Token != TokenInt) FatalError(Location, "number expected");
  const float res = TokenFloat;
  NextToken();
  return res;
}


//==========================================================================
//
//  VTextParser::ExpectSignedFloatAllowInt
//
//==========================================================================
float VTextParser::ExpectSignedFloatAllowInt () {
  bool neg = false;
  if (Token == TK_Delim) {
         if (TokenStr == "+") NextToken();
    else if (TokenStr == "-") { neg = true; NextToken(); }
  }
  if (Token != TK_Float && Token != TK_Int) FatalError(Location, "number expected");
  float res = TokenFloat;
  NextToken();
  if (neg) res = -res;
  return res;
}


//==========================================================================
//
//  VTextParser::CheckIdInternal
//
//==========================================================================
bool VTextParser::CheckIdInternal (const char *s, bool caseSens) {
  if (Token != TK_Id) return false;
  if (s[0]) {
    const bool ok = (caseSens ? TokenStr.strEqu(s) : TokenStr.strEquCI(s));
    if (!ok) return false;
  }
  NextToken();
  return true;
}


//==========================================================================
//
//  VTextParser::CheckDelim
//
//==========================================================================
bool VTextParser::CheckDelim (const char *s) {
  if (Token != TK_Delim) return false;
  if (s[0] && !TokenStr.strEqu(s)) return false;
  NextToken();
  return true;
}


//==========================================================================
//
//  VTextParser::CheckInt
//
//==========================================================================
bool VTextParser::CheckInt (int *val) {
  if (Token != TK_Int) return false;
  if (val) *val = TokenInt;
  NextToken();
  return true;
}


//==========================================================================
//
//  VTextParser::CheckFloat
//
//==========================================================================
bool VTextParser::CheckFloat (float *val) {
  if (Token != TK_Float) return false;
  if (val) *val = TokenFloat;
  NextToken();
  return true;
}
