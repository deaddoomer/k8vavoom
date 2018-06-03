////**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************

#include "vc_local.h"


// ////////////////////////////////////////////////////////////////////////// //
const char *VLexer::TokenNames[] = {
  "",
  "END OF FILE",
  "IDENTIFIER",
  "NAME LITERAL",
  "STRING LITERAL",
  "INTEGER LITERAL",
  "FLOAT LITERAL",
  // keywords
  "abstract",
  "array",
  "auto",
  "bool",
  "break",
  "byte",
  "case",
  "class",
  "const",
  "continue",
  "decorate",
  "default",
  "defaultproperties",
  "delegate",
  "do",
  "else",
  "enum",
  "false",
  "final",
  "float",
  "for",
  "foreach",
  "game",
  "get",
  "if",
  "import",
  "int",
  "iterator",
  "name",
  "native",
  "none",
  "nullptr",
  "optional",
  "out",
  "override",
  "private",
  "protected",
  "readonly",
  "ref",
  "reliable",
  "replication",
  "return",
  "self",
  "set",
  "spawner",
  "state",
  "states",
  "static",
  "string",
  "struct",
  "switch",
  "transient",
  "true",
  "unreliable",
  "vector",
  "void",
  "while",
  "__mobjinfo__",
  "__scriptid__",
  // punctuation
  "...",
  "<<=",
  ">>=",
  "..",
  "+=",
  "-=",
  "*=",
  "/=",
  "%=",
  "&=",
  "|=",
  "^=",
  "==",
  "!=",
  "<=",
  ">=",
  "&&",
  "||",
  "<<",
  ">>",
  "++",
  "--",
  "->",
  "::",
  "<",
  ">",
  "?",
  "&",
  "|",
  "^",
  "~",
  "!",
  "+",
  "-",
  "*",
  "/",
  "%",
  "(",
  ")",
  ".",
  ",",
  ";",
  ":",
  "=",
  "[",
  "]",
  "{",
  "}",
  "$",
};

char VLexer::ASCIIToChrCode[256];
vuint8 VLexer::ASCIIToHexDigit[256];
bool VLexer::tablesInited = false;


//==========================================================================
//
//  VLexer::VLexer
//
//==========================================================================
VLexer::VLexer ()
  : sourceOpen(false)
  , src(nullptr)
  , Token(TK_NoToken)
  , Number(0)
  , Float(0)
  , Name(NAME_None)
{
  memset(tokenStringBuffer, 0, sizeof(tokenStringBuffer));
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
//  VLexer::OpenSource
//
//==========================================================================
void VLexer::OpenSource (const VStr &FileName) {
  // read file and prepare for compilation
  PushSource(Location, FileName);
  sourceOpen = true;
  Token = TK_NoToken;
}


//==========================================================================
//
//  VLexer::OpenSource
//
//==========================================================================
void VLexer::OpenSource (VStream *astream, const VStr &FileName) {
  // read file and prepare for compilation
  PushSource(Location, astream, FileName);
  sourceOpen = true;
  Token = TK_NoToken;
}


//==========================================================================
//
//  VLexer::PushSource
//
//==========================================================================
void VLexer::PushSource (TLocation &Loc, const VStr &FileName) {
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
  VStream *Strm = FL_OpenFileRead(FileName);
#else
  VStream *Strm = fsysOpenFile(FileName);
#endif
  PushSource(Loc, Strm, FileName);
}


//==========================================================================
//
//  VLexer::PushSource
//
//==========================================================================
void VLexer::PushSource (TLocation &Loc, VStream *Strm, const VStr &FileName) {
  if (!Strm) {
    FatalError("Couldn't open %s", *FileName);
    return;
  }

  VSourceFile *NewSrc = new VSourceFile();
  NewSrc->Next = src;
  src = NewSrc;

  // copy file name
  NewSrc->FileName = FileName;

  // extract path to the file.
  const char *PathEnd = *FileName+FileName.Length()-1;
  while (PathEnd >= *FileName && *PathEnd != '/' && *PathEnd != '\\') --PathEnd;
  if (PathEnd >= *FileName) NewSrc->Path = VStr(FileName, 0, (PathEnd-(*FileName))+1);

  // read the file
  int FileSize = Strm->TotalSize();
  NewSrc->FileStart = new char[FileSize+1];
  Strm->Serialise(NewSrc->FileStart, FileSize);
  Strm->Close();
  delete Strm;

  NewSrc->FileStart[FileSize] = 0;
  NewSrc->FileEnd = NewSrc->FileStart+FileSize;
  NewSrc->FilePtr = NewSrc->FileStart;

  // skip garbage some editors add in the begining of UTF-8 files (BOM)
  if ((vuint8)NewSrc->FilePtr[0] == 0xef && (vuint8)NewSrc->FilePtr[1] == 0xbb && (vuint8)NewSrc->FilePtr[2] == 0xbf) NewSrc->FilePtr += 3;

  // save current character and location to be able to restore them
  NewSrc->currCh = currCh;
  NewSrc->Loc = Location;

  NewSrc->SourceIdx = TLocation::AddSourceFile(FileName);
  NewSrc->Line = 1;
  NewSrc->IncLineNumber = false;
  NewSrc->NewLine = true;
  NewSrc->Skipping = false;
  Location = TLocation(NewSrc->SourceIdx, NewSrc->Line);
  NextChr();
}


//==========================================================================
//
//  VLexer::PopSource
//
//==========================================================================
void VLexer::PopSource () {
  if (!src) return;
  if (src->IfStates.Num()) ParseError(Location, "#ifdef without a corresponding #endif");
  VSourceFile *Tmp = src;
  delete[] Tmp->FileStart;
  Tmp->FileStart = nullptr;
  src = Tmp->Next;
  currCh = Tmp->currCh;
  Location = Tmp->Loc;
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
//==========================================================================
void VLexer::NextChr () {
  if (src->FilePtr >= src->FileEnd) {
    currCh = EOF_CHARACTER;
    return;
  }
  if (src->IncLineNumber) {
    ++src->Line;
    Location = TLocation(src->SourceIdx, src->Line);
    src->IncLineNumber = false;
  }
  currCh = *src->FilePtr++;
  if ((vuint8)currCh < ' ') {
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
//==========================================================================
char VLexer::Peek (int dist) const {
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
//  VLexer::SkipWhitespaceAndComments
//
//==========================================================================
void VLexer::SkipWhitespaceAndComments () {
  bool Done;
  do {
    Done = true;
    while (currCh == ' ') NextChr();
    if (currCh == '/' && *src->FilePtr == '*') {
      // block comment
      NextChr();
      do {
        NextChr();
        if (currCh == EOF_CHARACTER) {
          ParseError(Location, "End of file inside a comment");
          return;
        }
      } while (currCh != '*' || *src->FilePtr != '/');
      NextChr();
      NextChr();
      Done = false;
    } else if (currCh == '/' && *src->FilePtr == '+') {
      // nested block comment
      NextChr();
      int level = 1;
      for (;;) {
        NextChr();
        if (currCh == EOF_CHARACTER) {
          ParseError(Location, "End of file inside a comment");
          return;
        }
        if (currCh == '+' && *src->FilePtr == '/') {
          NextChr();
          NextChr();
          if (--level == 0) break;
        } else if (currCh == '/' && *src->FilePtr == '+') {
          NextChr();
          NextChr();
          ++level;
        } else {
          NextChr();
        }
      }
      Done = false;
    } else if (currCh == '/' && *src->FilePtr == '/') {
      // c++ style comment
      NextChr();
      do {
        NextChr();
        if (currCh == EOF_CHARACTER) {
          ParseError(Location, "End of file inside a comment");
          return;
        }
      } while (!src->IncLineNumber);
      Done = false;
    }
  } while (!Done);
}


//==========================================================================
//
//  VLexer::peekNextNonBlankChar
//
//==========================================================================
char VLexer::peekNextNonBlankChar () const {
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
//  VLexer::ProcessPreprocessor
//
//==========================================================================
void VLexer::ProcessPreprocessor () {
  NextChr();

  if (src->NewLine || currCh == EOF_CHARACTER) {
    ParseError(Location, "Bad directive.");
    return;
  }

  if (ASCIIToChrCode[(vuint8)currCh] != CHR_Letter) {
    ParseError(Location, "Bad directive.");
    while (!src->NewLine && currCh != EOF_CHARACTER) NextChr();
    return;
  }

  ProcessLetterToken(false);

  if (!VStr::Cmp(tokenStringBuffer, "line")) {
    // read line number
    SkipWhitespaceAndComments();
    if (ASCIIToChrCode[(vuint8)currCh] != CHR_Number) ParseError(Location, "Bad directive.");
    ProcessNumberToken();
    src->Line = Number-1;

    // read file name
    SkipWhitespaceAndComments();
    if (ASCIIToChrCode[(vuint8)currCh] != CHR_Quote) ParseError(Location, "Bad directive.");
    ProcessFileName();
    src->SourceIdx = TLocation::AddSourceFile(String);
    Location = TLocation(src->SourceIdx, src->Line);

    // ignore flags
    while (!src->NewLine) NextChr();
  } else if (!VStr::Cmp(tokenStringBuffer, "define")) {
    ProcessDefine();
  } else if (!VStr::Cmp(tokenStringBuffer, "ifdef")) {
    ProcessIf(true);
  } else if (!VStr::Cmp(tokenStringBuffer, "ifndef")) {
    ProcessIf(false);
  } else if (!VStr::Cmp(tokenStringBuffer, "else")) {
    ProcessElse();
  } else if (!VStr::Cmp(tokenStringBuffer, "endif")) {
    ProcessEndIf();
  } else if (!VStr::Cmp(tokenStringBuffer, "include")) {
    ProcessInclude();
    return;
  } else {
    ParseError(Location, "Bad directive.");
    while (!src->NewLine && currCh != EOF_CHARACTER) NextChr();
  }
  Token = TK_NoToken;

  SkipWhitespaceAndComments();
  // a new-line is expected at the end of preprocessor directive.
  if (!src->NewLine) ParseError(Location, "Bad directive.");
}


//==========================================================================
//
//  VLexer::ProcessDefine
//
//==========================================================================
void VLexer::ProcessDefine () {
  SkipWhitespaceAndComments();

  // argument to the #define must be on the same line.
  if (src->NewLine || currCh == EOF_CHARACTER) {
    ParseError(Location, "Bad directive.");
    return;
  }

  // parse name to be defined
  if (ASCIIToChrCode[(vuint8)currCh] != CHR_Letter) {
    ParseError(Location, "Bad directive.");
    while (!src->NewLine && currCh != EOF_CHARACTER) NextChr();
    return;
  }
  ProcessLetterToken(false);

  if (src->Skipping) return;

  AddDefine(tokenStringBuffer);
}


//==========================================================================
//
//  VLexer::AddDefine
//
//==========================================================================
void VLexer::AddDefine (const VStr &CondName, bool showWarning) {
  if (CondName.length() == 0) return; // get lost!
  // check for redefined names
  for (int i = 0; i < defines.length(); ++i) {
    if (defines[i] == CondName) {
      if (showWarning) ParseWarning(Location, "Redefined conditional");
      return;
    }
  }
  defines.Append(CondName);
}


//==========================================================================
//
//  VLexer::ProcessIf
//
//==========================================================================
void VLexer::ProcessIf (bool OnTrue) {
  SkipWhitespaceAndComments();

  // argument to the #ifdef must be on the same line
  if (src->NewLine || currCh == EOF_CHARACTER) {
    ParseError(Location, "Bad directive.");
    return;
  }

  // parse condition name
  if (ASCIIToChrCode[(vuint8)currCh] != CHR_Letter) {
    ParseError(Location, "Bad directive.");
    while (!src->NewLine && currCh != EOF_CHARACTER) NextChr();
    return;
  }

  ProcessLetterToken(false);

  if (src->Skipping) {
    src->IfStates.Append(IF_Skip);
  } else {
    // check if the names has been defined
    bool Found = false;
    for (int i = 0; i < defines.Num(); ++i) {
      if (defines[i] == tokenStringBuffer) {
        Found = true;
        break;
      }
    }
    if (Found == OnTrue) {
      src->IfStates.Append(IF_True);
    } else {
      src->IfStates.Append(IF_False);
      src->Skipping = true;
    }
  }
}


//==========================================================================
//
//  VLexer::ProcessElse
//
//==========================================================================
void VLexer::ProcessElse () {
  if (!src->IfStates.Num()) {
    ParseError(Location, "#else without an #ifdef/#ifndef");
    return;
  }
  switch (src->IfStates[src->IfStates.Num()-1]) {
    case IF_True:
      src->IfStates[src->IfStates.Num()-1] = IF_ElseFalse;
      src->Skipping = true;
      break;
    case IF_False:
      src->IfStates[src->IfStates.Num()-1] = IF_ElseTrue;
      src->Skipping = false;
      break;
    case IF_Skip:
      src->IfStates[src->IfStates.Num()-1] = IF_ElseSkip;
      break;
    case IF_ElseTrue:
    case IF_ElseFalse:
    case IF_ElseSkip:
      ParseError(Location, "Multiple #else directives for a single #ifdef");
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
  if (!src->IfStates.Num()) {
    ParseError(Location, "#endif without an #ifdef/#ifndef");
    return;
  }
  src->IfStates.RemoveIndex(src->IfStates.Num()-1);
  if (src->IfStates.Num() > 0) {
    switch (src->IfStates[src->IfStates.Num()-1]) {
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
  SkipWhitespaceAndComments();

  // file name must be on the same line
  if (src->NewLine || currCh == EOF_CHARACTER) {
    ParseError(Location, "Bad directive.");
    return;
  }

  // parse file name
  if (currCh != '\"') {
    ParseError(Location, "Bad directive.");
    while (!src->NewLine && currCh != EOF_CHARACTER) NextChr();
    return;
  }
  ProcessFileName();
  TLocation Loc = Location;

  Token = TK_NoToken;
  SkipWhitespaceAndComments();
  // a new-line is expected at the end of preprocessor directive.
  if (!src->NewLine) ParseError(Location, "Bad directive.");

  if (src->Skipping) return;

  // check if it's an absolute path location.
  if (tokenStringBuffer[0] != '/' && tokenStringBuffer[0] != '\\') {
    // first try relative to the current source file
    if (src->Path.IsNotEmpty()) {
      VStr FileName = src->Path+VStr(tokenStringBuffer);
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
      VStream *Strm = FL_OpenFileRead(FileName);
#else
      VStream *Strm = fsysOpenFile(FileName);
#endif
      if (Strm) {
        PushSource(Loc, Strm, FileName);
        return;
      }
    }

    for (int i = includePath.Num() - 1; i >= 0; --i) {
      VStr FileName = includePath[i]+VStr(tokenStringBuffer);
#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
      VStream *Strm = FL_OpenFileRead(FileName);
#else
      VStream *Strm = fsysOpenFile(FileName);
#endif
      if (Strm) {
        PushSource(Loc, Strm, FileName);
        return;
      }
    }
  }

  // either it's relative to the current directory or absolute path
  PushSource(Loc, tokenStringBuffer);
}


//==========================================================================
//
//  VLexer::AddIncludePath
//
//==========================================================================
void VLexer::AddIncludePath (const VStr &DirName) {
  if (DirName.length() == 0) return; // get lost
  VStr copy = DirName;
  // append trailing slash if needed
#ifndef _WIN32
  if (!copy.EndsWith("/")) copy += '/';
#else
  if (!copy.EndsWith("/") && !copy.EndsWith("\\")) copy += '/';
#endif
  // check for duplicate pathes
  for (int i = 0; i < includePath.length(); ++i) {
#ifndef _WIN32
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

  char c = currCh;
  NextChr();
  Number = c-'0';

  // hex/octal/decimal/binary constant?
  if (c == '0') {
    int base = 0;
    switch (currCh) {
      case 'x': case 'X': base = 16; break;
      case 'd': case 'D': base = 10; break;
      case 'o': case 'O': base = 8; break;
      case 'b': case 'B': base = 2; break;
    }
    if (base != 0) {
      NextChr();
      for (;;) {
        if (currCh != '_') {
          if (base == 16) {
            if (ASCIIToHexDigit[(vuint8)currCh] == NON_HEX_DIGIT) break;
            Number = (Number<<4)+ASCIIToHexDigit[(vuint8)currCh];
          } else {
            if (currCh < '0' || currCh >= '0'+base) break;
            Number = Number*base+(currCh-'0');
          }
        }
        NextChr();
      }
      if (isAlpha(currCh)) ParseError(Location, "Invalid number");
      return;
    }
  }

  for (;;) {
    if (currCh != '_') {
      if (ASCIIToChrCode[(vuint8)currCh] != CHR_Number) break;
      Number = 10*Number+(currCh-'0');
    }
    NextChr();
  }

  if (currCh == '.') {
    char nch = Peek(1);
    if ((nch >= 'A' && nch <= 'Z') || (nch >= 'a' && nch <= 'z') || nch == '_') {
      // num dot smth
    } else {
      Token = TK_FloatLiteral;
      NextChr(); // skip dot
      Float = Number;
      float fmul = 0.1;
      for (;;) {
        if (currCh != '_') {
          if (ASCIIToChrCode[(vuint8)currCh] != CHR_Number) break;
          Float += (currCh-'0')*fmul;
          fmul /= 10.0;
        }
        NextChr();
      }
      if (currCh == 'f') NextChr();
    }
  }

  if (isAlpha(currCh)) ParseError(Location, "Invalid number");
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
      case 'c': currCh = TEXT_COLOUR_ESCAPE; break;
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
//  VLexer::ProcessQuoteToken
//
//==========================================================================
void VLexer::ProcessQuoteToken () {
  Token = TK_StringLiteral;
  int len = 0;
  NextChr();
  while (currCh != '\"') {
    if (len >= MAX_QUOTED_LENGTH-1) {
      ParseError(Location, ERR_STRING_TOO_LONG);
      NextChr();
      continue;
    }
    ProcessChar();
    tokenStringBuffer[len] = currCh;
    NextChr();
    ++len;
  }
  tokenStringBuffer[len] = 0;
  NextChr();
}


//==========================================================================
//
//  VLexer::ProcessSingleQuoteToken
//
//==========================================================================
void VLexer::ProcessSingleQuoteToken () {
  Token = TK_NameLiteral;
  int len = 0;
  NextChr();
  while (currCh != '\'') {
    if (len >= MAX_IDENTIFIER_LENGTH-1) {
      ParseError(Location, ERR_STRING_TOO_LONG);
      NextChr();
      continue;
    }
    ProcessChar();
    tokenStringBuffer[len] = currCh;
    NextChr();
    ++len;
  }
  tokenStringBuffer[len] = 0;
  NextChr();
  Name = tokenStringBuffer;
}


//==========================================================================
//
//  VLexer::ProcessLetterToken
//
//==========================================================================
void VLexer::ProcessLetterToken (bool CheckKeywords) {
  Token = TK_Identifier;
  int len = 0;
  while (ASCIIToChrCode[(vuint8)currCh] == CHR_Letter || ASCIIToChrCode[(vuint8)currCh] == CHR_Number) {
    if (len == MAX_IDENTIFIER_LENGTH-1) {
      ParseError(Location, ERR_IDENTIFIER_TOO_LONG);
      NextChr();
      continue;
    }
    tokenStringBuffer[len] = currCh;
    ++len;
    NextChr();
  }
  tokenStringBuffer[len] = 0;

  if (!CheckKeywords) return;

  //k8: there were alot of `if`s with char-by-char comparisons, but meh... it is 2018 now.
  const char *s = tokenStringBuffer;
  switch (s[0]) {
    case '_':
      if (s[1] == '_') {
             if (checkStrTk("__mobjinfo__")) Token = TK_MobjInfo;
        else if (checkStrTk("__scriptid__")) Token = TK_ScriptId;
      }
      break;
    case 'a':
           if (checkStrTk("abstract")) Token = TK_Abstract;
      else if (checkStrTk("array")) Token = TK_Array;
      else if (checkStrTk("auto")) Token = TK_Auto;
      break;
    case 'b':
           if (checkStrTk("bool")) Token = TK_Bool;
      else if (checkStrTk("break")) Token = TK_Break;
      else if (checkStrTk("byte")) Token = TK_Byte;
      break;
    case 'c':
           if (checkStrTk("case")) Token = TK_Case;
      else if (checkStrTk("class")) Token = TK_Class;
      else if (checkStrTk("const")) Token = TK_Const;
      else if (checkStrTk("continue")) Token = TK_Continue;
      break;
    case 'd':
           if (checkStrTk("decorate")) Token = TK_Decorate;
      else if (checkStrTk("default")) Token = TK_Default;
      else if (checkStrTk("defaultproperties")) Token = TK_DefaultProperties;
      else if (checkStrTk("delegate")) Token = TK_Delegate;
      else if (checkStrTk("do")) Token = TK_Do;
      break;
    case 'e':
           if (checkStrTk("else")) Token = TK_Else;
      else if (checkStrTk("enum")) Token = TK_Enum;
      break;
    case 'f':
           if (checkStrTk("false")) Token = TK_False;
      else if (checkStrTk("final")) Token = TK_Final;
      else if (checkStrTk("float")) Token = TK_Float;
      else if (checkStrTk("for")) Token = TK_For;
      else if (checkStrTk("foreach")) Token = TK_Foreach;
      break;
    case 'g':
           if (checkStrTk("game")) Token = TK_Game;
      else if (checkStrTk("get")) Token = TK_Get;
      break;
    case 'i':
           if (checkStrTk("if")) Token = TK_If;
      else if (checkStrTk("import")) Token = TK_Import;
      else if (checkStrTk("int")) Token = TK_Int;
      else if (checkStrTk("iterator")) Token = TK_Iterator;
      break;
    case 'n':
           if (checkStrTk("name")) Token = TK_Name;
      else if (checkStrTk("native")) Token = TK_Native;
      else if (checkStrTk("none")) Token = TK_None;
      else if (checkStrTk("null")) Token = TK_Null;
      else if (checkStrTk("nullptr")) Token = TK_Null;
      break;
    case 'o':
           if (checkStrTk("optional")) Token = TK_Optional;
      else if (checkStrTk("out")) Token = TK_Out;
      else if (checkStrTk("override")) Token = TK_Override;
      break;
    case 'p':
      if (checkStrTk("private")) Token = TK_Private;
      if (checkStrTk("protected")) Token = TK_Protected;
      break;
    case 'r':
           if (checkStrTk("return")) Token = TK_Return;
      else if (checkStrTk("ref")) Token = TK_Ref;
      else if (checkStrTk("readonly")) Token = TK_ReadOnly;
      else if (checkStrTk("reliable")) Token = TK_Reliable;
      else if (checkStrTk("replication")) Token = TK_Replication;
      break;
    case 's':
           if (checkStrTk("string")) Token = TK_String;
      else if (checkStrTk("switch")) Token = TK_Switch;
      else if (checkStrTk("self")) Token = TK_Self;
      else if (checkStrTk("set")) Token = TK_Set;
      else if (checkStrTk("struct")) Token = TK_Struct;
      else if (checkStrTk("spawner")) Token = TK_Spawner;
      else if (checkStrTk("static")) Token = TK_Static;
      else if (checkStrTk("state")) Token = TK_State;
      else if (checkStrTk("states")) Token = TK_States;
      break;
    case 't':
           if (checkStrTk("true")) Token = TK_True;
      else if (checkStrTk("transient")) Token = TK_Transient;
      break;
    case 'u':
      if (checkStrTk("unreliable")) Token = TK_Unreliable;
      break;
    case 'v':
           if (checkStrTk("void")) Token = TK_Void;
      else if (checkStrTk("vector")) Token = TK_Vector;
      break;
    case 'w':
      if (checkStrTk("while")) Token = TK_While;
      break;
    case 'N':
      if (checkStrTk("NULL")) Token = TK_Null;
      break;
  }

  if (Token == TK_Identifier) Name = tokenStringBuffer;
}


//==========================================================================
//
//  VLexer::ProcessSpecialToken
//
//==========================================================================
void VLexer::ProcessSpecialToken () {
  char c = currCh;
  NextChr();
  switch (c) {
    case '+':
           if (currCh == '=') { Token = TK_AddAssign; NextChr(); }
      else if (currCh == '+') { Token = TK_Inc; NextChr(); }
      else Token = TK_Plus;
      break;
    case '-':
           if (currCh == '=') { Token = TK_MinusAssign; NextChr(); }
      else if (currCh == '-') { Token = TK_Dec; NextChr(); }
      else if (currCh == '>') { Token = TK_Arrow; NextChr(); }
      else Token = TK_Minus;
      break;
    case '*':
      if (currCh == '=') { Token = TK_MultiplyAssign; NextChr(); }
      else Token = TK_Asterisk;
      break;
    case '/':
      if (currCh == '=') { Token = TK_DivideAssign; NextChr(); }
      else Token = TK_Slash;
      break;
    case '%':
      if (currCh == '=') { Token = TK_ModAssign; NextChr(); }
      else Token = TK_Percent;
      break;
    case '=':
      if (currCh == '=') { Token = TK_Equals; NextChr(); }
      else Token = TK_Assign;
      break;
    case '<':
      if (currCh == '<') {
        NextChr();
        if (currCh == '=') { Token = TK_LShiftAssign; NextChr(); }
        else Token = TK_LShift;
      } else if (currCh == '=') {
        Token = TK_LessEquals;
        NextChr();
      } else {
        Token = TK_Less;
      }
      break;
    case '>':
      if (currCh == '>') {
        NextChr();
        if (currCh == '=') { Token = TK_RShiftAssign; NextChr(); }
        else Token = TK_RShift;
      } else if (currCh == '=') {
        Token = TK_GreaterEquals;
        NextChr();
      } else {
        Token = TK_Greater;
      }
      break;
    case '!':
      if (currCh == '=') { Token = TK_NotEquals; NextChr(); }
      else Token = TK_Not;
      break;
    case '&':
           if (currCh == '=') { Token = TK_AndAssign; NextChr(); }
      else if (currCh == '&') { Token = TK_AndLog; NextChr(); }
      else Token = TK_And;
      break;
    case '|':
           if (currCh == '=') { Token = TK_OrAssign; NextChr(); }
      else if (currCh == '|') { Token = TK_OrLog; NextChr(); }
      else Token = TK_Or;
      break;
    case '^':
      if (currCh == '=') { Token = TK_XOrAssign; NextChr(); }
      else Token = TK_XOr;
      break;
    case '.':
      if (currCh == '.') {
        NextChr();
        if (currCh == '.') { Token = TK_VarArgs; NextChr(); }
        else Token = TK_DotDot;
      } else {
        Token = TK_Dot;
      }
      break;
    case ':':
      if (currCh == ':') { Token = TK_DColon; NextChr(); }
      else Token = TK_Colon;
      break;
    case '(': Token = TK_LParen; break;
    case ')': Token = TK_RParen; break;
    case '?': Token = TK_Quest; break;
    case '~': Token = TK_Tilde; break;
    case ',': Token = TK_Comma; break;
    case ';': Token = TK_Semicolon; break;
    case '[': Token = TK_LBracket; break;
    case ']': Token = TK_RBracket; break;
    case '{': Token = TK_LBrace; break;
    case '}': Token = TK_RBrace; break;
    case '$': Token = TK_Dollar; break;
    default:
      ParseError(Location, ERR_BAD_CHARACTER, "Unknown punctuation \'%c\'", currCh);
      Token = TK_NoToken;
      break;
  }
}


//==========================================================================
//
//  VLexer::ProcessFileName
//
//==========================================================================
void VLexer::ProcessFileName () {
  int len = 0;
  NextChr();
  while (currCh != '\"') {
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
  if (Token != tk) ParseError(Location, "expected %s, found %s", TokenNames[tk], TokenNames[Token]);
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
  if (Token != tk) ParseError(Location, error, "expected %s, found %s", TokenNames[tk], TokenNames[Token]);
  NextToken();
}
