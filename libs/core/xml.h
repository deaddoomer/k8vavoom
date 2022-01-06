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
#ifndef VAVOOM_XML_PARSER_HEADER
#define VAVOOM_XML_PARSER_HEADER


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

private:
  VXmlNode *FindChildOf (const bool match, const char *name, va_list ap) noexcept;
  VXmlAttribute *FindAttrOf (const bool match, const char *name, va_list ap) noexcept;

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

  // finish names with `nullptr`
  // names should be simple strings, a-la "abc"
  VXmlNode *FindBadChild (const char *name, ...) noexcept __attribute__ ((sentinel));

  // finish names with `nullptr`
  // names should be simple strings, a-la "abc"
  VXmlAttribute *FindBadAttribute (const char *name, ...) noexcept __attribute__ ((sentinel));

  // finish names with `nullptr`
  // names should be simple strings, a-la "abc"
  VXmlNode *FindFirstChildOf (const char *name, ...) noexcept __attribute__ ((sentinel));

  // finish names with `nullptr`
  // names should be simple strings, a-la "abc"
  VXmlAttribute *FindFirstAttributeOf (const char *name, ...) noexcept __attribute__ ((sentinel));

  inline bool HasChildren () const noexcept { return !!FirstChild; }
  inline bool HasAttributes () const noexcept { return (Attributes.length() != 0); }

public:
  // range iteration
  // WARNING! don't add/remove array elements in iterator loop!

  class NodeIterator {
  public:
    VXmlNode *Node;
  public:
    inline NodeIterator (VXmlNode *startNode) noexcept : Node(startNode) {}
    inline NodeIterator (const NodeIterator &it) noexcept : Node(it.Node) {}
    inline NodeIterator begin () noexcept { return NodeIterator(*this); }
    inline NodeIterator end () noexcept { return NodeIterator(nullptr); }
    inline bool operator == (const NodeIterator &b) const noexcept { return (Node == b.Node); }
    inline bool operator != (const NodeIterator &b) const noexcept { return (Node != b.Node); }
    // yeah, the same
    inline VXmlNode* operator * () const noexcept { return Node; } // required for iterator
    //inline VXmlNode* operator -> () const noexcept { return Node; } // required for iterator
    inline void operator ++ () noexcept { if (Node) Node = Node->NextSibling; } // this is enough for iterator
  };
  inline NodeIterator allChildren () noexcept { return NodeIterator(FirstChild); }

  class NamedNodeIterator {
  public:
    VXmlNode *Node;
  public:
    inline NamedNodeIterator (VXmlNode *startNode) noexcept : Node(startNode) {}
    inline NamedNodeIterator (const NamedNodeIterator &it) noexcept : Node(it.Node) {}
    inline NamedNodeIterator begin () noexcept { return NamedNodeIterator(*this); }
    inline NamedNodeIterator end () noexcept { return NamedNodeIterator(nullptr); }
    inline bool operator == (const NamedNodeIterator &b) const noexcept { return (Node == b.Node); }
    inline bool operator != (const NamedNodeIterator &b) const noexcept { return (Node != b.Node); }
    // yeah, the same
    inline VXmlNode* operator * () const noexcept { return Node; } // required for iterator
    //inline VXmlNode* operator -> () const noexcept { return Node; } // required for iterator
    // this is the only difference (sighs)
    inline void operator ++ () noexcept { if (Node) Node = Node->FindNext(); } // this is enough for iterator
  };
  inline NamedNodeIterator childrenWithName (const char *aname) noexcept { return NamedNodeIterator(FindChild(aname)); }
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


#endif
