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

class VXmlAttribute {
public:
  VStr Name;
  VStr Value;
  VTextLocation Loc;
};


class VXmlNode {
public:
  VStr Name;
  VStr Value;
  VTextLocation Loc;
  VXmlNode *Parent;
  VXmlNode *FirstChild;
  VXmlNode *LastChild;
  VXmlNode *PrevSibling;
  VXmlNode *NextSibling;
  TArray<VXmlAttribute> Attributes;

public:
  VXmlNode ();
  ~VXmlNode ();

  VXmlNode *FindChild (const char *AName) const;
  VXmlNode *FindChild (VStr AName) const;
  VXmlNode *GetChild (const char *AName) const;
  VXmlNode *GetChild (VStr AName) const;
  VXmlNode *FindNext (const char *AName) const;
  VXmlNode *FindNext (VStr AName) const;
  VXmlNode *FindNext () const; // with the same name as the current one
  bool HasAttribute (const char *AttrName) const;
  bool HasAttribute (VStr AttrName) const;
  VStr GetAttribute (const char *AttrName, bool Required=true) const;
  VStr GetAttribute (VStr AttrName, bool Required=true) const;
  const VTextLocation GetAttributeLoc (const char *AttrName) const;
  const VTextLocation GetAttributeLoc (VStr AttrName) const;
};


class VXmlDocument {
private:
  enum { UTF8, WIN1251, KOI8 };

private:
  char *Buf;
  int CurrPos;
  int EndPos;
  int Encoding;
  VTextLocation CurrLoc;

public:
  VStr Name;
  VXmlNode Root;

public:
  void Parse (VStream &Strm, VStr AName);

private:
  vuint32 GetChar (); // with correct encoding

  void SkipWhitespace ();
  bool SkipComment ();
  void Error (const char *msg);
  void ErrorAtLoc (const VTextLocation &loc, const char *msg);
  void ErrorAtCurrLoc (const char *msg);
  VStr ParseName ();
  VStr ParseAttrValue (char EndChar);
  bool ParseAttribute (VStr &AttrName, VStr &AttrValue);
  void ParseNode (VXmlNode *);
  VStr HandleReferences (VStr);
};
