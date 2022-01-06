//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
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
#include "core.h"


//==========================================================================
//
//  VXmlNode::VXmlNode
//
//==========================================================================
VXmlNode::VXmlNode ()
  : Parent(nullptr)
  , FirstChild(nullptr)
  , LastChild(nullptr)
  , PrevSibling(nullptr)
  , NextSibling(nullptr)
{
}


//==========================================================================
//
//  VXmlNode::~VXmlNode
//
//==========================================================================
VXmlNode::~VXmlNode () {
  while (FirstChild) {
    VXmlNode *Temp = FirstChild;
    FirstChild = FirstChild->NextSibling;
    delete Temp;
  }
}


//==========================================================================
//
//  VXmlNode::FindChildOf
//
//==========================================================================
VXmlNode *VXmlNode::FindChildOf (const bool match, const char *name, va_list ap) noexcept {
  for (VXmlNode *N = FirstChild; N; N = N->NextSibling) {
    bool found = N->Name.strEqu(name);
    if (!found) {
      va_list apc;
      va_copy(apc, ap);
      for (;;) {
        const char *n = va_arg(apc, const char *);
        if (!n) break;
        if (N->Name.strEqu(n)) { found = true; break; }
      }
      va_end(apc);
    }
    if (found == match) return N;
  }
  return nullptr;
}


//==========================================================================
//
//  VXmlNode::FindAttrOf
//
//==========================================================================
VXmlAttribute *VXmlNode::FindAttrOf (const bool match, const char *name, va_list ap) noexcept {
  for (auto &&attr : Attributes) {
    bool found = attr.Name.strEqu(name);
    if (!found) {
      va_list apc;
      va_copy(apc, ap);
      for (;;) {
        const char *n = va_arg(apc, const char *);
        if (!n) break;
        if (attr.Name.strEqu(n)) { found = true; break; }
      }
      va_end(apc);
    }
    if (found == match) return &attr;
  }
  return nullptr;
}


//==========================================================================
//
//  VXmlNode::FindBadChild
//
//==========================================================================
__attribute__ ((sentinel)) VXmlNode *VXmlNode::FindBadChild (const char *name, ...) noexcept {
  va_list ap;
  va_start(ap, name);
  VXmlNode *res = FindChildOf(false, name, ap);
  va_end(ap);
  return res;
}


//==========================================================================
//
//  VXmlNode::FindBadAttribute
//
//==========================================================================
__attribute__ ((sentinel)) VXmlAttribute *VXmlNode::FindBadAttribute (const char *name, ...) noexcept {
  va_list ap;
  va_start(ap, name);
  VXmlAttribute *res = FindAttrOf(false, name, ap);
  va_end(ap);
  return res;
}


//==========================================================================
//
//  VXmlNode::FindFirstChildOf
//
//==========================================================================
__attribute__ ((sentinel)) VXmlNode *VXmlNode::FindFirstChildOf (const char *name, ...) noexcept {
  va_list ap;
  va_start(ap, name);
  VXmlNode *res = FindChildOf(true, name, ap);
  va_end(ap);
  return res;
}


//==========================================================================
//
//  VXmlNode::FindFirstAttributeOf
//
//==========================================================================
__attribute__ ((sentinel)) VXmlAttribute *VXmlNode::FindFirstAttributeOf (const char *name, ...) noexcept {
  va_list ap;
  va_start(ap, name);
  VXmlAttribute *res = FindAttrOf(true, name, ap);
  va_end(ap);
  return res;
}


//==========================================================================
//
//  VXmlNode::FindChild
//
//==========================================================================
VXmlNode *VXmlNode::FindChild (const char *AName) const {
  if (!AName || !AName[0]) return nullptr;
  for (VXmlNode *N = FirstChild; N; N = N->NextSibling) {
    if (N->Name == AName) return N;
  }
  return nullptr;
}


//==========================================================================
//
//  VXmlNode::FindChild
//
//==========================================================================
VXmlNode *VXmlNode::FindChild (VStr AName) const {
  if (AName.isEmpty()) return nullptr;
  for (VXmlNode *N = FirstChild; N; N = N->NextSibling) {
    if (N->Name == AName) return N;
  }
  return nullptr;
}


//==========================================================================
//
//  VXmlNode::GetChild
//
//==========================================================================
VXmlNode *VXmlNode::GetChild (const char *AName) const {
  VXmlNode *N = FindChild(AName);
  if (!N) Sys_Error("XML node '%s' not found in node '%s' at %s", (AName ? AName : ""), *Name, *Loc.toStringNoCol());
  return N;
}


//==========================================================================
//
//  VXmlNode::GetChild
//
//==========================================================================
VXmlNode *VXmlNode::GetChild (VStr AName) const {
  VXmlNode *N = FindChild(AName);
  if (!N) Sys_Error("XML node '%s' not found in node '%s' at %s", *AName, *Name, *Loc.toStringNoCol());
  return N;
}


//==========================================================================
//
//  VXmlNode::FindNext
//
//==========================================================================
VXmlNode *VXmlNode::FindNext (const char *AName) const {
  if (!AName || !AName[0]) return nullptr;
  for (VXmlNode *N = NextSibling; N; N = N->NextSibling) {
    if (N->Name == AName) return N;
  }
  return nullptr;
}


//==========================================================================
//
//  VXmlNode::FindNext
//
//==========================================================================
VXmlNode *VXmlNode::FindNext (VStr AName) const {
  if (AName.isEmpty()) return nullptr;
  for (VXmlNode *N = NextSibling; N; N = N->NextSibling) {
    if (N->Name == AName) return N;
  }
  return nullptr;
}


//==========================================================================
//
//  VXmlNode::FindNext
//
//==========================================================================
VXmlNode *VXmlNode::FindNext () const {
  for (VXmlNode *N = NextSibling; N; N = N->NextSibling) {
    if (N->Name == Name) return N;
  }
  return nullptr;
}


//==========================================================================
//
//  VXmlNode::HasAttribute
//
//==========================================================================
bool VXmlNode::HasAttribute (const char *AttrName) const {
  if (!AttrName || !AttrName[0]) return false;
  for (auto &&attr : Attributes) if (attr.Name == AttrName) return true;
  return false;
}


//==========================================================================
//
//  VXmlNode::FindChild
//
//==========================================================================
bool VXmlNode::HasAttribute (VStr AttrName) const {
  if (AttrName.isEmpty()) return false;
  for (auto &&attr : Attributes) if (attr.Name == AttrName) return true;
  return false;
}


//==========================================================================
//
//  VXmlNode::GetAttribute
//
//==========================================================================
VStr VXmlNode::GetAttribute (const char *AttrName, bool Required) const {
  if (AttrName && AttrName[0]) {
    for (auto &&attr : Attributes) if (attr.Name == AttrName) return attr.Value;
  }
  if (Required) {
    /*
    if (Attributes.length()) {
      GLog.Logf("=== node \"%s\" attrs ===", *Name);
      for (auto &&a : Attributes) GLog.Logf("  \"%s\"=\"%s\"", *a.Name, *a.Value);
    }
    */
    Sys_Error("XML attribute \"%s\" not found in node \"%s\" at %s", (AttrName ? AttrName : ""), *Name, *Loc.toStringNoCol());
  }
  return VStr::EmptyString;
}


//==========================================================================
//
//  VXmlNode::GetAttribute
//
//==========================================================================
VStr VXmlNode::GetAttribute (VStr AttrName, bool Required) const {
  if (!AttrName.isEmpty()) {
    for (auto &&attr : Attributes) if (attr.Name == AttrName) return attr.Value;
  }
  if (Required) {
    /*
    if (Attributes.length()) {
      GLog.Logf("=== node \"%s\" attrs ===", *Name);
      for (auto &&a : Attributes) GLog.Logf("  \"%s\"=\"%s\"", *a.Name, *a.Value);
    }
    */
    Sys_Error("XML attribute \"%s\" not found in node \"%s\" at %s", *AttrName, *Name, *Loc.toStringNoCol());
  }
  return VStr::EmptyString;
}


//==========================================================================
//
//  VXmlNode::GetAttributeLoc
//
//==========================================================================
const VTextLocation VXmlNode::GetAttributeLoc (const char *AttrName) const {
  if (AttrName && AttrName[0]) {
    for (auto &&attr : Attributes) if (attr.Name == AttrName) return attr.Loc;
  }
  return VTextLocation();
}


//==========================================================================
//
//  VXmlNode::GetAttributeLoc
//
//==========================================================================
const VTextLocation VXmlNode::GetAttributeLoc (VStr AttrName) const {
  if (!AttrName.isEmpty()) {
    for (auto &&attr : Attributes) if (attr.Name == AttrName) return attr.Loc;
  }
  return VTextLocation();
}


//==========================================================================
//
//  VXmlDocument::Parse
//
//==========================================================================
void VXmlDocument::Parse (VStream &Strm, VStr AName) {
  Name = AName;
  Encoding = UTF8;

  Buf = new char[Strm.TotalSize()+1];
  Strm.Seek(0);
  Strm.Serialise(Buf, Strm.TotalSize());
  Buf[Strm.TotalSize()] = 0;
  CurrPos = 0;
  EndPos = Strm.TotalSize();
  CurrLoc = VTextLocation(AName, 1, 1);

  // skip garbage some editors add to the begining of UTF-8 files
  if ((vuint8)Buf[0] == 0xef && (vuint8)Buf[1] == 0xbb && (vuint8)Buf[2] == 0xbf) CurrPos += 3;

  do { SkipWhitespace(); } while (SkipComment());

  if (CurrPos >= EndPos) Error("Empty document");

  if (!(Buf[CurrPos] == '<' && Buf[CurrPos+1] == '?' && Buf[CurrPos+2] == 'x' &&
        Buf[CurrPos+3] == 'm' && Buf[CurrPos+4] == 'l' && Buf[CurrPos+5] > 0 && Buf[CurrPos+5] <= ' '))
  {
    ErrorAtCurrLoc("XML declaration expected");
  }
  CurrLoc.ConsumeChars(5);
  CurrPos += 5;
  SkipWhitespace();

  VStr AttrName;
  VStr AttrValue;
  if (!ParseAttribute(AttrName, AttrValue)) ErrorAtCurrLoc("XML version expected");
  if (AttrName != "version") ErrorAtCurrLoc("XML version expected");
  if (AttrValue != "1.0" && AttrValue != "1.1") ErrorAtCurrLoc("Bad XML version");

  SkipWhitespace();
  while (ParseAttribute(AttrName, AttrValue)) {
    if (AttrName == "encoding") {
      VStr ec = AttrValue;
           if (ec.ICmp("UTF-8") == 0) Encoding = UTF8;
      else if (ec.ICmp("WINDOWS-1251") == 0) Encoding = WIN1251;
      else if (ec.ICmp("WINDOWS1251") == 0) Encoding = WIN1251;
      else if (ec.ICmp("CP-1251") == 0) Encoding = WIN1251;
      else if (ec.ICmp("CP1251") == 0) Encoding = WIN1251;
      else if (ec.ICmp("KOI-8") == 0) Encoding = KOI8;
      else if (ec.ICmp("KOI-8U") == 0) Encoding = KOI8;
      else if (ec.ICmp("KOI-8R") == 0) Encoding = KOI8;
      else if (ec.ICmp("KOI8") == 0) Encoding = KOI8;
      else if (ec.ICmp("KOI8U") == 0) Encoding = KOI8;
      else if (ec.ICmp("KOI8R") == 0) Encoding = KOI8;
      else ErrorAtCurrLoc(va("Unknown XML encoding '%s'", *ec));
    } else if (AttrName == "standalone") {
      if (AttrValue.ICmp("yes") != 0) ErrorAtCurrLoc("Only standalone is supported");
    } else {
      ErrorAtCurrLoc(va("Unknown attribute '%s'", *AttrName));
    }
    SkipWhitespace();
  }

  if (Buf[CurrPos] != '?' || Buf[CurrPos+1] != '>') ErrorAtCurrLoc("Bad syntax");
  CurrPos += 2;
  CurrLoc.ConsumeChars(2);

  do { SkipWhitespace(); } while (SkipComment());

  if (Buf[CurrPos] != '<') ErrorAtCurrLoc("Root node expected");
  ParseNode(&Root);

  do { SkipWhitespace(); } while (SkipComment());

  if (CurrPos != EndPos) ErrorAtCurrLoc("Text after root node");

  delete[] Buf;
  Buf = nullptr;
}


//==========================================================================
//
//  VXmlDocument::SkipWhitespace
//
//==========================================================================
void VXmlDocument::SkipWhitespace () {
  while (Buf[CurrPos] > 0 && (vuint8)Buf[CurrPos] <= ' ') {
    CurrLoc.ConsumeChar(Buf[CurrPos] == '\n');
    ++CurrPos;
  }
}


//==========================================================================
//
//  VXmlDocument::SkipComment
//
//==========================================================================
bool VXmlDocument::SkipComment () {
  if (Buf[CurrPos] == '<' && Buf[CurrPos+1] == '!' &&
      Buf[CurrPos+2] == '-' && Buf[CurrPos+3] == '-')
  {
    // skip comment
    CurrPos += 4;
    CurrLoc.ConsumeChars(4);
    while (CurrPos < EndPos-2 && (Buf[CurrPos] != '-' || Buf[CurrPos+1] != '-' || Buf[CurrPos+2] != '>')) {
      CurrLoc.ConsumeChar(Buf[CurrPos] == '\n');
      ++CurrPos;
    }
    if (CurrPos >= EndPos-2) ErrorAtCurrLoc("Unterminated comment");
    CurrPos += 3;
    CurrLoc.ConsumeChars(3);
    return true;
  }
  return false;
}


//==========================================================================
//
//  VXmlDocument::Error
//
//==========================================================================
void VXmlDocument::Error (const char *msg) {
  Sys_Error("%s: %s", *Name, msg);
}


//==========================================================================
//
//  VXmlDocument::ErrorAtLoc
//
//==========================================================================
void VXmlDocument::ErrorAtLoc (const VTextLocation &loc, const char *msg) {
  Sys_Error("%s: %s", *loc.toStringNoCol(), msg);
}


//==========================================================================
//
//  VXmlDocument::ErrorAtCurrLoc
//
//==========================================================================
void VXmlDocument::ErrorAtCurrLoc (const char *msg) {
  ErrorAtLoc(CurrLoc, msg);
}


//==========================================================================
//
//  VXmlDocument::ParseName
//
//==========================================================================
VStr VXmlDocument::ParseName () {
  VStr Ret;
  char c = Buf[CurrPos];
  if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '-' || c == ':')) return VStr();

  do {
    CurrLoc.ConsumeChar(c == '\n');
    Ret += c;
    CurrPos++;
    c = Buf[CurrPos];
  } while ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == ':' || c == '.');

  return Ret;
}


//==========================================================================
//
//  VXmlDocument::GetChar
//
//  with correct encoding
//
//==========================================================================
vuint32 VXmlDocument::GetChar () {
  if (CurrPos >= EndPos) ErrorAtCurrLoc("unexpected EOF");
  char ch = Buf[CurrPos++];
  if (ch == '\r' && Buf[CurrPos] == '\n') { ch = '\n'; ++CurrPos; }
  CurrLoc.ConsumeChar(ch == '\n');
  if (!ch) ch = ' ';
  if ((vuint8)ch < 128) return (vuint32)(ch&0xff);
  switch (Encoding) {
    case WIN1251: return VStr::cp1251[((vuint8)ch)-128];
    case KOI8: return VStr::cpKOI[((vuint8)ch)-128];
    case UTF8:
      {
        VUtf8DecoderFast dc;
        if (!dc.put(ch)) {
          while (CurrPos < EndPos) {
            CurrLoc.ConsumeChar(Buf[CurrPos] == '\n');
            if (dc.put(Buf[CurrPos++])) {
              if (dc.invalid()) ErrorAtCurrLoc("invalid UTF-8 char");
              return dc.codepoint;
            }
          }
        }
        ErrorAtCurrLoc("invalid UTF-8 char");
      }
  }
  ErrorAtCurrLoc("WUT?!");
  return 0;
}


//==========================================================================
//
//  VXmlDocument::ParseAttrValue
//
//==========================================================================
VStr VXmlDocument::ParseAttrValue (char EndChar) {
  VStr Ret;
  while (CurrPos < EndPos && Buf[CurrPos] != EndChar) Ret.utf8Append(GetChar());
  if (CurrPos >= EndPos) ErrorAtCurrLoc("Unterminated attribute value");
  ++CurrPos;
  CurrLoc.ConsumeChars(1);
  return HandleReferences(Ret);
}


//==========================================================================
//
//  VXmlDocument::ParseAttribute
//
//==========================================================================
bool VXmlDocument::ParseAttribute (VStr &AttrName, VStr &AttrValue) {
  AttrName = ParseName();
  if (AttrName.IsEmpty()) return false;
  SkipWhitespace();
  if (Buf[CurrPos] != '=') ErrorAtCurrLoc("Attribute value expected");
  ++CurrPos;
  CurrLoc.ConsumeChars(1);
  SkipWhitespace();
  if (CurrPos >= EndPos) ErrorAtCurrLoc("unexpected end of document");
  if (Buf[CurrPos] == '\"' || Buf[CurrPos] == '\'') {
    ++CurrPos;
    CurrLoc.ConsumeChars(1);
    AttrValue = ParseAttrValue(Buf[CurrPos-1]);
  } else {
    ErrorAtCurrLoc("Unquoted attribute value");
  }
  return true;
}


//==========================================================================
//
//  VXmlDocument::ParseNode
//
//==========================================================================
void VXmlDocument::ParseNode (VXmlNode *Node) {
  if (Buf[CurrPos] != '<') ErrorAtCurrLoc("Bad tag start");
  Node->Loc = CurrLoc;
  ++CurrPos;
  CurrLoc.ConsumeChars(1);
  Node->Name = ParseName();
  if (Node->Name.IsEmpty()) ErrorAtCurrLoc("Bad or missing tag name");

  SkipWhitespace();
  VStr AttrName;
  VStr AttrValue;
  VTextLocation aloc = CurrLoc;
  while (ParseAttribute(AttrName, AttrValue)) {
    VXmlAttribute &A = Node->Attributes.Alloc();
    A.Name = AttrName;
    A.Value = AttrValue;
    A.Loc = aloc;
    SkipWhitespace();
    aloc = CurrLoc;
  }

  if (Buf[CurrPos] == '/' && Buf[CurrPos+1] == '>') {
    CurrPos += 2;
    CurrLoc.ConsumeChars(2);
  } else if (Buf[CurrPos] == '>') {
    ++CurrPos;
    CurrLoc.ConsumeChars(1);
    while (CurrPos < EndPos && (Buf[CurrPos] != '<' || Buf[CurrPos+1] != '/')) {
      if (Buf[CurrPos] == '<') {
        if (Buf[CurrPos+1] == '!' && Buf[CurrPos+2] == '-' && Buf[CurrPos+3] == '-') {
          SkipComment();
        } else if (Buf[CurrPos+1] == '!' && Buf[CurrPos+2] == '[' &&
                   Buf[CurrPos+3] == 'C' && Buf[CurrPos+4] == 'D' &&
                   Buf[CurrPos+5] == 'A' && Buf[CurrPos+6] == 'T' &&
                   Buf[CurrPos+7] == 'A' && Buf[CurrPos+8] == '[')
        {
          CurrPos += 9;
          CurrLoc.ConsumeChars(9);
          VStr TextVal;
          while (CurrPos < EndPos && (Buf[CurrPos] != ']' || Buf[CurrPos+1] != ']' || Buf[CurrPos+2] != '>')) {
            TextVal.utf8Append(GetChar());
          }
          if (CurrPos >= EndPos) ErrorAtCurrLoc("Unexpected end of file in CDATA");
          Node->Value += TextVal;
          CurrPos += 3;
          CurrLoc.ConsumeChars(3);
        } else {
          VXmlNode *NewChild = new VXmlNode();
          NewChild->PrevSibling = Node->LastChild;
          if (Node->LastChild) {
            Node->LastChild->NextSibling = NewChild;
          } else {
            Node->FirstChild = NewChild;
          }
          Node->LastChild = NewChild;
          NewChild->Parent = Node;
          ParseNode(NewChild);
        }
      } else {
        VStr TextVal;
        bool HasNonWhitespace = false;
        while (CurrPos < EndPos && Buf[CurrPos] != '<') {
          if (!HasNonWhitespace && (vuint8)Buf[CurrPos] > ' ') HasNonWhitespace = true;
          TextVal.utf8Append(GetChar());
        }
        if (HasNonWhitespace) Node->Value += HandleReferences(TextVal);
      }
    }
    if (CurrPos >= EndPos) ErrorAtCurrLoc("Unexpected end of file");
    CurrPos += 2;
    CurrLoc.ConsumeChars(2);
    VStr Test = ParseName();
    if (Node->Name != Test) ErrorAtCurrLoc("Wrong end tag");
    if (Buf[CurrPos] != '>') ErrorAtCurrLoc("Tag not closed");
    ++CurrPos;
    CurrLoc.ConsumeChars(1);
  } else {
    ErrorAtCurrLoc("Tag is not closed");
  }
}


//==========================================================================
//
//  VXmlDocument::HandleReferences
//
//==========================================================================
VStr VXmlDocument::HandleReferences (VStr AStr) {
  VStr Ret = AStr;
  for (int i = 0; i < Ret.length(); ++i) {
    if (Ret[i] == '&') {
      int EndPos = i+1;
      while (EndPos < Ret.length() && Ret[EndPos] != ';') ++EndPos;
      if (EndPos >= Ret.length()) ErrorAtCurrLoc("Unterminated character or entity reference");
      ++EndPos;
      VStr Seq = VStr(Ret, i, EndPos-i);
      VStr NewVal;
      if (Seq.length() > 4 && Seq[1] == '#' && Seq[2] == 'x') {
        int Val = 0;
        for (int j = 3; j < Seq.length()-1; ++j) {
          int dg = VStr::digitInBase(Seq[j], 16);
          if (dg < 0) ErrorAtCurrLoc("Bad character reference");
          Val = (Val<<4)|dg;
        }
        NewVal = VStr::FromUtf8Char(Val);
      } else if (Seq.length() > 3 && Seq[1] == '#') {
        int Val = 0;
        for (int j = 2; j < Seq.length()-1; ++j) {
          int dg = VStr::digitInBase(Seq[j], 10);
          if (dg < 0) ErrorAtCurrLoc("Bad character reference");
          Val = (Val*10)+dg;
        }
        NewVal = VStr::FromUtf8Char(Val);
      } else if (Seq == "&amp;") NewVal = "&";
        else if (Seq == "&quot;") NewVal = "\"";
        else if (Seq == "&apos;") NewVal = "\'";
        else if (Seq == "&lt;") NewVal = "<";
        else if (Seq == "&gt;") NewVal = ">";
        else ErrorAtCurrLoc(va("Unknown entity reference '%s'", *Seq));
      Ret = VStr(Ret, 0, i)+NewVal+VStr(Ret, EndPos, Ret.length()-EndPos);
      i += NewVal.length()-1;
    }
  }
  return Ret;
}
