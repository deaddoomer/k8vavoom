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
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
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
#include <stdlib.h>
#include <string.h>
#include "../libs/core/hashfunc.h"

struct VertexInfo {
  float xy[2];
  int idx;

  VertexInfo (const float ax, const float ay, int aidx) { xy[0] = ax; xy[1] = ay; idx = aidx; }
  //inline bool operator == (const VertexInfo &vi) const { return (xy[0] == vi.xy[0] && xy[1] == vi.xy[1]); }
  //inline bool operator != (const VertexInfo &vi) const { return (xy[0] != vi.xy[0] || xy[1] != vi.xy[1]); }
  inline bool operator == (const VertexInfo &vi) const { return (memcmp(xy, &vi.xy, sizeof(xy)) == 0); }
  inline bool operator != (const VertexInfo &vi) const { return (memcmp(xy, &vi.xy, sizeof(xy)) != 0); }
};

inline vuint32 GetTypeHash (const VertexInfo &vi) { return joaatHashBuf(vi.xy, sizeof(vi.xy)); }


// ////////////////////////////////////////////////////////////////////////// //
#include "gamedefs.h"
#include "sv_local.h"


enum {
  ML_PASSUSE_BOOM = 0x0200, // Boom's ML_PASSUSE flag (conflicts with ML_REPEAT_SPECIAL)

  MTF_AMBUSH      = 0x0008, // deaf monsters/do not react to sound
  MTF_DORMANT     = 0x0010, // the thing is dormant
  MTF_GSINGLE     = 0x0100, // appearing in game modes
  MTF_GCOOP       = 0x0200,
  MTF_GDEATHMATCH = 0x0400,
  MTF_SHADOW      = 0x0800,
  MTF_ALTSHADOW   = 0x1000,
  MTF_FRIENDLY    = 0x2000,
  MTF_STANDSTILL  = 0x4000,
};


// ////////////////////////////////////////////////////////////////////////// //
class VUdmfParser {
public:
  // supported namespaces; use bits to have faster cheks
  enum {
    // standard namespaces
    NS_Doom       = 0x01,
    NS_Heretic    = 0x02,
    NS_Hexen      = 0x04,
    NS_Strife     = 0x08,
    // Vavoom's namespace
    NS_Vavoom     = 0x10,
    // ZDoom's namespaces
    NS_ZDoom      = 0x20,
    NS_ZDoomTranslated = 0x40,
  };

  enum {
    TK_None,
    TK_Int,
    TK_Float,
    TK_String,
    TK_Identifier,
  };

  struct VParsedLine {
    line_t L;
    int V1Index;
    int V2Index;
  };

  struct VParsedSide {
    side_t S;
    VStr TopTexture;
    VStr MidTexture;
    VStr BotTexture;
    int SectorIndex;
  };

  VScriptParser sc;
  bool bExtended;
  vuint8 NS;
  VStr Key;
  int ValType;
  int ValInt;
  float ValFloat;
  VStr Val;
  TArray<vertex_t> ParsedVertexes;
  TArray<sector_t> ParsedSectors;
  TArray<VParsedLine> ParsedLines;
  TArray<VParsedSide> ParsedSides;
  TArray<mthing_t> ParsedThings;

public:
  VUdmfParser (int Lump);
  void Parse (VLevel *Level, const mapInfo_t &MInfo);
  void ParseVertex ();
  void ParseSector (VLevel *Level);
  void ParseLineDef (const mapInfo_t &MInfo);
  void ParseSideDef ();
  void ParseThing ();
  void ParseKey ();
  bool CanSilentlyIgnoreKey () const;
  int CheckInt ();
  float CheckFloat ();
  bool CheckBool ();
  VStr CheckString ();
  void Flag (int &Field, int Mask);
  void Flag (vuint32 &Field, int Mask);

  vuint32 CheckColor ();
};


//==========================================================================
//
//  VUdmfParser::VUdmfParser
//
//==========================================================================
VUdmfParser::VUdmfParser (int Lump) : sc("textmap", W_CreateLumpReaderNum(Lump)) {
}


//==========================================================================
//
//  VUdmfParser::CanSilentlyIgnoreKey
//
//==========================================================================
bool VUdmfParser::CanSilentlyIgnoreKey () const {
  return
    Key.startsWithCI("user_") ||
    Key.strEquCI("comment");
}


//==========================================================================
//
//  VUdmfParser::ParseKey
//
//==========================================================================
void VUdmfParser::ParseKey () {
  // get key and value
  sc.ExpectString();
  Key = sc.String;
  sc.Expect("=");

  ValType = TK_None;
  if (sc.CheckQuotedString()) {
    ValType = TK_String;
    Val = sc.String;
  } else if (sc.Check("+")) {
    if (sc.CheckNumber()) {
      ValType = TK_Int;
      ValInt = sc.Number;
      Val = VStr(ValInt);
    } else if (sc.CheckFloat()) {
      ValType = TK_Float;
      ValFloat = sc.Float;
      Val = VStr(ValFloat);
    } else {
      sc.Message(va("Numeric constant expected (%s)", *Key));
    }
  } else if (sc.Check("-")) {
    if (sc.CheckNumber()) {
      ValType = TK_Int;
      ValInt = -sc.Number;
      Val = VStr(ValInt);
    } else if (sc.CheckFloat()) {
      ValType = TK_Float;
      ValFloat = -sc.Float;
      Val = VStr(ValFloat);
    } else {
      sc.Message(va("Numeric constant expected (%s)", *Key));
    }
  } else if (sc.CheckNumber()) {
    ValType = TK_Int;
    ValInt = sc.Number;
    Val = VStr(ValInt);
  } else if (sc.CheckFloat()) {
    ValType = TK_Float;
    ValFloat = sc.Float;
    Val = VStr(ValFloat);
  } else {
    sc.ExpectString();
    ValType = TK_Identifier;
    Val = sc.String;
  }
  sc.Expect(";");
}


//==========================================================================
//
//  VUdmfParser::CheckInt
//
//==========================================================================
int VUdmfParser::CheckInt () {
  if (ValType != TK_Int) { sc.Message(va("Integer value expected for key '%s'", *Key)); ValInt = 0; }
  return ValInt;
}


//==========================================================================
//
//  VUdmfParser::CheckFloat
//
//==========================================================================
float VUdmfParser::CheckFloat () {
  if (ValType != TK_Int && ValType != TK_Float) { sc.Message(va("Float value expected for key '%s'", *Key)); ValInt = 0; ValFloat = 0; }
  return (ValType == TK_Int ? ValInt : ValFloat);
}


//==========================================================================
//
//  VUdmfParser::CheckBool
//
//==========================================================================
bool VUdmfParser::CheckBool () {
  if (ValType == TK_Identifier) {
    if (Val.strEquCI("true") || Val.strEquCI("on") || Val.strEquCI("tan")) return true;
    if (Val.strEquCI("false") || Val.strEquCI("off") || Val.strEquCI("ona")) return false;
  }
  sc.Message(va("Boolean value expected for key '%s'", *Key));
  return false;
}


//==========================================================================
//
//  VUdmfParser::CheckString
//
//==========================================================================
VStr VUdmfParser::CheckString () {
  if (ValType != TK_String) { sc.Message(va("String value expected for key '%s'", *Key)); Val = VStr(); }
  return Val;
}


//==========================================================================
//
//  VUdmfParser::CheckColor
//
//==========================================================================
vuint32 VUdmfParser::CheckColor () {
  if (ValType == TK_Int) return (ValInt&0xffffff);
  return ParseHex(*CheckString());
}


//==========================================================================
//
//  VUdmfParser::Flag
//
//==========================================================================
void VUdmfParser::Flag (int &Field, int Mask) {
  if (CheckBool()) Field |= Mask; else Field &= ~Mask;
}


//==========================================================================
//
//  VUdmfParser::Flag
//
//==========================================================================
void VUdmfParser::Flag (vuint32 &Field, int Mask) {
  if (CheckBool()) Field |= Mask; else Field &= ~Mask;
}


//==========================================================================
//
//  VUdmfParser::Parse
//
//==========================================================================
void VUdmfParser::Parse (VLevel *Level, const mapInfo_t &MInfo) {
  sc.SetCMode(true);

  bExtended = false;

  // get namespace name
  sc.Expect("namespace");
  sc.Expect("=");
  sc.ExpectString();
  VStr Namespace = sc.String;
  sc.Expect(";");

  // Vavoom's namespace?
       if (Namespace.strEquCI("Vavoom")) { NS = NS_Vavoom; bExtended = true; }
  // standard namespaces?
  else if (Namespace.strEquCI("Doom")) NS = NS_Doom;
  else if (Namespace.strEquCI("Heretic")) NS = NS_Heretic;
  else if (Namespace.strEquCI("Hexen")) { NS = NS_Hexen; bExtended = true; }
  else if (Namespace.strEquCI("Strife")) NS = NS_Strife;
  // ZDoom namespaces?
  else if (Namespace.strEquCI("ZDoom")) { NS = NS_ZDoom; bExtended = true; }
  else if (Namespace.strEquCI("ZDoomTranslated")) NS = NS_ZDoomTranslated;
  else {
    sc.Message(va("UDMF: unknown namespace '%s'", *Namespace));
    NS = 0; // unknown namespace
  }

  while (!sc.AtEnd()) {
         if (sc.Check("vertex")) ParseVertex();
    else if (sc.Check("sector")) ParseSector(Level);
    else if (sc.Check("linedef")) ParseLineDef(MInfo);
    else if (sc.Check("sidedef")) ParseSideDef();
    else if (sc.Check("thing")) ParseThing();
    else sc.Error("Syntax error");
  }
}


//==========================================================================
//
//  VUdmfParser::ParseVertex
//
//==========================================================================
void VUdmfParser::ParseVertex () {
  // allocate a new vertex
  vertex_t &V = ParsedVertexes.Alloc();
  V = TVec(0, 0, 0);
  sc.Expect("{");
  while (!sc.Check("}")) {
    ParseKey();
    if (Key.strEquCI("x")) {
      V.x = CheckFloat();
      continue;
    }
    if (Key.strEquCI("y")) {
      V.y = CheckFloat();
      continue;
    }
    if (!CanSilentlyIgnoreKey()) sc.Message(va("UDMF: unknown vertex property '%s' with value '%s'", *Key, *Val));
  }
}


//==========================================================================
//
//  VUdmfParser::ParseSector
//
//==========================================================================
void VUdmfParser::ParseSector (VLevel *Level) {
  sector_t &S = ParsedSectors.Alloc();
  memset((void *)&S, 0, sizeof(sector_t));
  S.floor.Set(TVec(0, 0, 1), 0);
  S.floor.XScale = 1.0f;
  S.floor.YScale = 1.0f;
  S.floor.Alpha = 1.0f;
  S.floor.MirrorAlpha = 1.0f;
  S.floor.LightSourceSector = -1;
  S.ceiling.Set(TVec(0, 0, -1), 0);
  S.ceiling.XScale = 1.0f;
  S.ceiling.YScale = 1.0f;
  S.ceiling.Alpha = 1.0f;
  S.ceiling.MirrorAlpha = 1.0f;
  S.ceiling.LightSourceSector = -1;
  S.params.lightlevel = 160;
  S.params.LightColour = 0x00ffffff;
  S.seqType = -1; // default seqType
  S.Gravity = 1.0f;  // default sector gravity of 1.0
  S.Zone = -1;

  sc.Expect("{");
  while (!sc.Check("}")) {
    ParseKey();

    if (Key.strEquCI("heightfloor")) {
      float FVal = CheckFloat();
      S.floor.dist = FVal;
      S.floor.TexZ = FVal;
      S.floor.minz = FVal;
      S.floor.maxz = FVal;
      continue;
    }

    if (Key.strEquCI("heightceiling")) {
      float FVal = CheckFloat();
      S.ceiling.dist = -FVal;
      S.ceiling.TexZ = FVal;
      S.ceiling.minz = FVal;
      S.ceiling.maxz = FVal;
      continue;
    }

    if (Key.strEquCI("texturefloor")) {
      S.floor.pic = Level->TexNumForName(*Val, TEXTYPE_Flat, false, true);
      continue;
    }

    if (Key.strEquCI("textureceiling")) {
      S.ceiling.pic = Level->TexNumForName(*Val, TEXTYPE_Flat, false, true);
      continue;
    }

    if (Key.strEquCI("lightlevel")) {
      S.params.lightlevel = CheckInt();
      continue;
    }

    if (Key.strEquCI("special")) {
      S.special = CheckInt();
      continue;
    }

    if (Key.strEquCI("id")) {
      S.tag = CheckInt();
      continue;
    }

    // extensions
    if (NS&(NS_Vavoom|NS_ZDoom|NS_ZDoomTranslated)) {
      if (Key.strEquCI("xpanningfloor")) {
        S.floor.xoffs = CheckFloat();
        continue;
      }

      if (Key.strEquCI("ypanningfloor")) {
        S.floor.yoffs = CheckFloat();
        continue;
      }

      if (Key.strEquCI("xpanningceiling")) {
        S.ceiling.xoffs = CheckFloat();
        continue;
      }

      if (Key.strEquCI("ypanningceiling")) {
        S.ceiling.yoffs = CheckFloat();
        continue;
      }

      if (Key.strEquCI("xscalefloor")) {
        S.floor.XScale = CheckFloat();
        continue;
      }

      if (Key.strEquCI("yscalefloor")) {
        S.floor.YScale = CheckFloat();
        continue;
      }

      if (Key.strEquCI("xscaleceiling")) {
        S.ceiling.XScale = CheckFloat();
        continue;
      }

      if (Key.strEquCI("yscaleceiling")) {
        S.ceiling.YScale = CheckFloat();
        continue;
      }

      if (Key.strEquCI("rotationfloor")) {
        S.floor.Angle = CheckFloat();
        continue;
      }

      if (Key.strEquCI("rotationceiling")) {
        S.ceiling.Angle = CheckFloat();
        continue;
      }

      if (Key.strEquCI("gravity")) {
        S.Gravity = CheckFloat();
        continue;
      }

      if (Key.strEquCI("lightcolor")) {
        S.params.LightColour = CheckColor();
        continue;
      }

      if (Key.strEquCI("fadecolor")) {
        S.params.Fade = CheckColor();
        continue;
      }

      if (Key.strEquCI("silent")) {
        Flag(S.SectorFlags, sector_t::SF_Silent);
        continue;
      }

      if (Key.strEquCI("nofallingdamage")) {
        Flag(S.SectorFlags, sector_t::SF_NoFallingDamage);
        continue;
      }

      /*
      if (NS&(NS_Vavoom|NS_ZDoom|NS_ZDoomTranslated)) {
        if (Key.strEquCI("lightfloor")) {
          int lt = CheckInt();
          if (S.params.lightlevel < lt) S.params.lightlevel = lt;
          continue;
        }
        if (Key.strEquCI("lightceiling")) {
          int lt = CheckInt();
          if (S.params.lightlevel < lt) S.params.lightlevel = lt;
          continue;
        }
      }
      */
    }

    if (!CanSilentlyIgnoreKey()) sc.Message(va("UDMF: unknown sector property '%s' with value '%s'", *Key, *Val));
  }
}


//==========================================================================
//
//  VUdmfParser::ParseLineDef
//
//==========================================================================
void VUdmfParser::ParseLineDef (const mapInfo_t &MInfo) {
  VParsedLine &L = ParsedLines.Alloc();
  memset((void *)&L, 0, sizeof(VParsedLine));
  L.V1Index = -1;
  L.V2Index = -1;
  L.L.alpha = 1.0f;
  L.L.LineTag = bExtended ? -1 : 0;
  L.L.sidenum[0] = -1;
  L.L.sidenum[1] = -1;
  if (MInfo.Flags&MAPINFOF_ClipMidTex) L.L.flags |= ML_CLIP_MIDTEX;
  if (MInfo.Flags&MAPINFOF_WrapMidTex) L.L.flags |= ML_WRAP_MIDTEX;
  //bool HavePassUse = false;

  VStr arg0str;
  bool hasArg0Str = false;

  sc.Expect("{");
  while (!sc.Check("}")) {
    ParseKey();

    if (Key.strEquCI("id")) {
      L.L.LineTag = CheckInt();
      continue;
    }

    if (Key.strEquCI("v1")) {
      L.V1Index = CheckInt();
      continue;
    }

    if (Key.strEquCI("v2")) {
      L.V2Index = CheckInt();
      continue;
    }

    if (Key.strEquCI("blocking")) {
      Flag(L.L.flags, ML_BLOCKING);
      continue;
    }

    if (Key.strEquCI("blockmonsters")) {
      Flag(L.L.flags, ML_BLOCKMONSTERS);
      continue;
    }

    if (Key.strEquCI("twosided")) {
      Flag(L.L.flags, ML_TWOSIDED);
      continue;
    }

    if (Key.strEquCI("dontpegtop")) {
      Flag(L.L.flags, ML_DONTPEGTOP);
      continue;
    }

    if (Key.strEquCI("dontpegbottom")) {
      Flag(L.L.flags, ML_DONTPEGBOTTOM);
      continue;
    }

    if (Key.strEquCI("secret")) {
      Flag(L.L.flags, ML_SECRET);
      continue;
    }

    if (Key.strEquCI("blocksound")) {
      Flag(L.L.flags, ML_SOUNDBLOCK);
      continue;
    }

    if (Key.strEquCI("dontdraw")) {
      Flag(L.L.flags, ML_DONTDRAW);
      continue;
    }

    if (Key.strEquCI("mapped")) {
      Flag(L.L.flags, ML_MAPPED);
      continue;
    }

    if (Key.strEquCI("special")) {
      L.L.special = CheckInt();
      continue;
    }

    if (Key.strEquCI("arg0")) {
      L.L.arg1 = CheckInt();
      continue;
    }

    if (Key.strEquCI("arg1")) {
      L.L.arg2 = CheckInt();
      continue;
    }

    if (Key.strEquCI("arg2")) {
      L.L.arg3 = CheckInt();
      continue;
    }

    if (Key.strEquCI("arg3")) {
      L.L.arg4 = CheckInt();
      continue;
    }

    if (Key.strEquCI("arg4")) {
      L.L.arg5 = CheckInt();
      continue;
    }

    if (Key.strEquCI("sidefront")) {
      L.L.sidenum[0] = CheckInt();
      continue;
    }

    if (Key.strEquCI("sideback")) {
      L.L.sidenum[1] = CheckInt();
      continue;
    }

    // doom specific flags
    if (NS&(NS_Doom|NS_Vavoom|NS_ZDoom|NS_ZDoomTranslated)) {
      if (Key.strEquCI("passuse")) {
        if (bExtended) {
          /*HavePassUse =*/(void)CheckBool(); //k8: dunno
        } else {
          Flag(L.L.flags, ML_PASSUSE_BOOM);
        }
        continue;
      }
    }

    // strife specific flags
    if (NS&(NS_Strife|NS_Vavoom|NS_ZDoom|NS_ZDoomTranslated)) {
      if (Key.strEquCI("arg0str")) {
        //FIXME: actually, this is valid only for ACS specials
        arg0str = CheckString();
        hasArg0Str = true;
        continue;
      }

      if (Key.strEquCI("translucent")) {
        L.L.alpha = (CheckBool() ? 0.666f : 1.0f);
        continue;
      }

      if (Key.strEquCI("jumpover")) {
        Flag(L.L.flags, ML_RAILING);
        continue;
      }

      if (Key.strEquCI("blockfloaters")) {
        Flag(L.L.flags, ML_BLOCK_FLOATERS);
        continue;
      }
    }

    // hexen's extensions
    if (NS&(NS_Hexen|NS_Vavoom|NS_ZDoom|NS_ZDoomTranslated)) {
      if (Key.strEquCI("playercross")) {
        Flag(L.L.SpacFlags, SPAC_Cross);
        continue;
      }

      if (Key.strEquCI("playeruse")) {
        Flag(L.L.SpacFlags, SPAC_Use);
        continue;
      }

      if (Key.strEquCI("playeruseback")) {
        Flag(L.L.SpacFlags, SPAC_UseBack);
        continue;
      }

      if (Key.strEquCI("monstercross")) {
        Flag(L.L.SpacFlags, SPAC_MCross);
        continue;
      }

      if (Key.strEquCI("monsteruse")) {
        Flag(L.L.SpacFlags, SPAC_MUse);
        continue;
      }

      if (Key.strEquCI("impact")) {
        Flag(L.L.SpacFlags, SPAC_Impact);
        continue;
      }

      if (Key.strEquCI("playerpush")) {
        Flag(L.L.SpacFlags, SPAC_Push);
        continue;
      }

      if (Key.strEquCI("monsterpush")) {
        Flag(L.L.SpacFlags, SPAC_MPush);
        continue;
      }

      if (Key.strEquCI("missilecross")) {
        Flag(L.L.SpacFlags, SPAC_PCross);
        continue;
      }

      if (Key.strEquCI("repeatspecial")) {
        Flag(L.L.flags, ML_REPEAT_SPECIAL);
        continue;
      }
    }

    // extensions
    if (NS&(NS_Vavoom|NS_ZDoom|NS_ZDoomTranslated)) {
      if (Key.strEquCI("alpha")) {
        L.L.alpha = CheckFloat();
        L.L.alpha = midval(0.0f, L.L.alpha, 1.0f);
        continue;
      }

      if (Key.strEquCI("renderstyle")) {
        VStr RS = CheckString();
             if (RS.strEquCI("translucent")) L.L.flags &= ~ML_ADDITIVE;
        else if (RS.strEquCI("add")) L.L.flags |= ML_ADDITIVE;
        else sc.Message("Bad linedef render style");
        continue;
      }

      if (Key.strEquCI("anycross")) {
        Flag(L.L.SpacFlags, SPAC_AnyCross);
        continue;
      }

      if (Key.strEquCI("monsteractivate")) {
        Flag(L.L.flags, ML_MONSTERSCANACTIVATE);
        continue;
      }

      if (Key.strEquCI("blockplayers")) {
        Flag(L.L.flags, ML_BLOCKPLAYERS);
        continue;
      }

      if (Key.strEquCI("blockeverything")) {
        Flag(L.L.flags, ML_BLOCKEVERYTHING);
        continue;
      }

      if (Key.strEquCI("firstsideonly")) {
        Flag(L.L.flags, ML_FIRSTSIDEONLY);
        continue;
      }

      if (Key.strEquCI("zoneboundary")) {
        Flag(L.L.flags, ML_ZONEBOUNDARY);
        continue;
      }

      if (Key.strEquCI("clipmidtex")) {
        Flag(L.L.flags, ML_CLIP_MIDTEX);
        continue;
      }

      if (Key.strEquCI("wrapmidtex")) {
        Flag(L.L.flags, ML_WRAP_MIDTEX);
        continue;
      }

      if (Key.strEquCI("blockprojectiles")) {
        Flag(L.L.flags, ML_BLOCKPROJECTILE);
        continue;
      }

      if (Key.strEquCI("blockuse")) {
        Flag(L.L.flags, ML_BLOCKUSE);
        continue;
      }

      if (Key.strEquCI("blocksight")) {
        Flag(L.L.flags, ML_BLOCKSIGHT);
        continue;
      }

      if (Key.strEquCI("blockhitscan")) {
        Flag(L.L.flags, ML_BLOCKHITSCAN);
        continue;
      }

      //TODO
      if (Key.strEquCI("Checkswitchrange")) {
        Flag(L.L.flags, ML_CHECKSWITCHRANGE);
        continue;
      }

      if (Key.strEquCI("midtex3d")) {
        //GCon->Logf(NAME_Warning, "%s: UDMF: `midtex3d` is not fully implemented", *sc.GetLoc().toStringNoCol());
        Flag(L.L.flags, ML_3DMIDTEX);
        continue;
      }
    }

    if (!CanSilentlyIgnoreKey()) sc.Message(va("UDMF: unknown linedef property '%s' with value '%s'", *Key, *Val));
  }

  //FIXME: actually, this is valid only for special runacs range for now; write a proper thingy instead
  if (hasArg0Str && ((L.L.special >= 80 && L.L.special <= 86) || L.L.special == 226)) {
    VName sn = VName(*arg0str, VName::AddLower); // 'cause script names are lowercased
    if (sn.GetIndex() != NAME_None) {
      L.L.arg1 = -(int)sn.GetIndex();
      //GCon->Logf("*** ACS NAMED LINE SPECIAL %d: name is (%d) '%s'", L.L.special, sn.GetIndex(), *sn);
    } else {
      L.L.arg1 = 0;
    }
  }
}


//==========================================================================
//
//  VUdmfParser::ParseSideDef
//
//==========================================================================
void VUdmfParser::ParseSideDef () {
  VParsedSide &S = ParsedSides.Alloc();
  memset((void *)&S, 0, sizeof(VParsedSide));
  S.TopTexture = "-";
  S.MidTexture = "-";
  S.BotTexture = "-";
  S.S.Top.ScaleX = S.S.Top.ScaleY = 1.0f;
  S.S.Bot.ScaleX = S.S.Bot.ScaleY = 1.0f;
  S.S.Mid.ScaleX = S.S.Mid.ScaleY = 1.0f;
  float XOffs = 0;
  float YOffs = 0;

  sc.Expect("{");
  while (!sc.Check("}")) {
    ParseKey();

    if (Key.strEquCI("offsetx")) {
      XOffs = CheckFloat();
      continue;
    }

    if (Key.strEquCI("offsety")) {
      YOffs = CheckFloat();
      continue;
    }

    if (Key.strEquCI("texturetop")) {
      S.TopTexture = CheckString();
      continue;
    }

    if (Key.strEquCI("texturebottom")) {
      S.BotTexture = CheckString();
      continue;
    }

    if (Key.strEquCI("texturemiddle")) {
      S.MidTexture = CheckString();
      continue;
    }

    if (Key.strEquCI("sector")) {
      S.SectorIndex = CheckInt();
      continue;
    }

    // extensions
    if (NS&(NS_Vavoom|NS_ZDoom|NS_ZDoomTranslated)) {
      if (Key.strEquCI("offsetx_top")) {
        S.S.Top.TextureOffset = CheckFloat();
        continue;
      }

      if (Key.strEquCI("offsety_top")) {
        S.S.Top.RowOffset = CheckFloat();
        continue;
      }

      if (Key.strEquCI("offsetx_mid")) {
        S.S.Mid.TextureOffset = CheckFloat();
        continue;
      }

      if (Key.strEquCI("offsety_mid")) {
        S.S.Mid.RowOffset = CheckFloat();
        continue;
      }

      if (Key.strEquCI("offsetx_bottom")) {
        S.S.Bot.TextureOffset = CheckFloat();
        continue;
      }

      if (Key.strEquCI("offsety_bottom")) {
        S.S.Bot.RowOffset = CheckFloat();
        continue;
      }

      if (Key.strEquCI("scalex_top")) {
        S.S.Top.ScaleX = CheckFloat();
        continue;
      }

      if (Key.strEquCI("scaley_top")) {
        S.S.Top.ScaleY = CheckFloat();
        continue;
      }

      if (Key.strEquCI("scalex_mid")) {
        S.S.Mid.ScaleX = CheckFloat();
        continue;
      }

      if (Key.strEquCI("scaley_mid")) {
        S.S.Mid.ScaleY = CheckFloat();
        continue;
      }

      if (Key.strEquCI("scalex_bottom")) {
        S.S.Bot.ScaleX = CheckFloat();
        continue;
      }

      if (Key.strEquCI("scaley_bottom")) {
        S.S.Bot.ScaleY = CheckFloat();
        continue;
      }

      if (Key.strEquCI("light")) {
        S.S.Light = CheckInt();
        //GCon->Logf("sidedef #%d: light=%d", ParsedSides.length()-1, S.S.Light);
        continue;
      }

      if (Key.strEquCI("lightabsolute")) {
        Flag(S.S.Flags, SDF_ABSLIGHT);
        continue;
      }

      if (Key.strEquCI("wrapmidtex")) {
        Flag(S.S.Flags, SDF_WRAPMIDTEX);
        continue;
      }

      if (Key.strEquCI("clipmidtex")) {
        Flag(S.S.Flags, SDF_CLIPMIDTEX);
        continue;
      }
    }

    if (!CanSilentlyIgnoreKey()) sc.Message(va("UDMF: unknown sidedef property '%s' with value '%s'", *Key, *Val));
  }

  S.S.Top.TextureOffset += XOffs;
  S.S.Mid.TextureOffset += XOffs;
  S.S.Bot.TextureOffset += XOffs;
  S.S.Top.RowOffset += YOffs;
  S.S.Mid.RowOffset += YOffs;
  S.S.Bot.RowOffset += YOffs;
}


//==========================================================================
//
//  VUdmfParser::ParseThing
//
//==========================================================================
void VUdmfParser::ParseThing () {
  mthing_t &T = ParsedThings.Alloc();
  memset((void *)&T, 0, sizeof(mthing_t));

  VStr arg0str;
  bool hasArg0Str = false;

  sc.Expect("{");
  while (!sc.Check("}")) {
    ParseKey();

    if (Key.strEquCI("x")) {
      T.x = CheckFloat();
      continue;
    }

    if (Key.strEquCI("y")) {
      T.y = CheckFloat();
      continue;
    }

    if (Key.strEquCI("height")) {
      T.height = CheckFloat();
      continue;
    }

    if (Key.strEquCI("angle")) {
      T.angle = CheckInt();
      continue;
    }

    if (Key.strEquCI("type")) {
      T.type = CheckInt();
      continue;
    }

    if (Key.strEquCI("ambush")) {
      Flag(T.options, MTF_AMBUSH);
      continue;
    }

    if (Key.strEquCI("single")) {
      Flag(T.options, MTF_GSINGLE);
      continue;
    }

    if (Key.strEquCI("dm")) {
      Flag(T.options, MTF_GDEATHMATCH);
      continue;
    }

    if (Key.strEquCI("coop")) {
      Flag(T.options, MTF_GCOOP);
      continue;
    }

    // skills (up to, and including 16)
    if (Key.startsWithCI("skill")) {
      int skill = -1;
      if (VStr::convertInt((*Key)+5, &skill)) {
        if (skill >= 1 && skill <= 16) {
          Flag(T.SkillClassFilter, 0x0001<<(skill-1));
          continue;
        }
      }
    }

    // MBF friendly flag
    if (NS&(NS_Hexen|NS_Vavoom|NS_ZDoom|NS_ZDoomTranslated)) {
      if (Key.strEquCI("friend")) {
        Flag(T.options, MTF_FRIENDLY);
        continue;
      }
    }

    // strife specific flags
    if (NS&(NS_Hexen|NS_Vavoom|NS_ZDoom|NS_ZDoomTranslated)) {
      if (Key.strEquCI("standing")) {
        Flag(T.options, MTF_STANDSTILL);
        continue;
      }

      if (Key.strEquCI("strifeally")) {
        Flag(T.options, MTF_FRIENDLY);
        continue;
      }

      if (Key.strEquCI("translucent")) {
        Flag(T.options, MTF_SHADOW);
        continue;
      }

      if (Key.strEquCI("invisible")) {
        Flag(T.options, MTF_ALTSHADOW);
        continue;
      }
    }

    // hexen's extensions
    if (NS&(NS_Hexen|NS_Vavoom|NS_ZDoom|NS_ZDoomTranslated)) {
      if (Key.strEquCI("arg0str")) {
        //FIXME: actually, this is valid only for ACS specials
        //       this can be color name for dynamic lights too
        //T.arg1 = M_ParseColour(*CheckString())&0xffffffu;
        arg0str = CheckString();
        hasArg0Str = true;
        continue;
      }


      if (Key.strEquCI("id")) {
        T.tid = CheckInt();
        continue;
      }

      if (Key.strEquCI("dormant")) {
        Flag(T.options, MTF_DORMANT);
        continue;
      }

      if (Key.strEquCI("special")) {
        T.special = CheckInt();
        continue;
      }

      if (Key.strEquCI("arg0")) {
        T.arg1 = CheckInt();
        continue;
      }

      if (Key.strEquCI("arg1")) {
        T.arg2 = CheckInt();
        continue;
      }

      if (Key.strEquCI("arg2")) {
        T.arg3 = CheckInt();
        continue;
      }

      if (Key.strEquCI("arg3")) {
        T.arg4 = CheckInt();
        continue;
      }

      if (Key.strEquCI("arg4")) {
        T.arg5 = CheckInt();
        continue;
      }

      // classes (up to, and including 16)
      if (Key.startsWithCI("class")) {
        int cls = -1;
        if (VStr::convertInt((*Key)+5, &cls)) {
          if (cls >= 1 && cls <= 16) {
            Flag(T.SkillClassFilter, 0x00010000<<(cls-1));
            continue;
          }
        }
      }
    }

    if (!CanSilentlyIgnoreKey()) sc.Message(va("UDMF: unknown thing property '%s' with value '%s'", *Key, *Val));
  }

  //FIXME: actually, this is valid only for special runacs range for now; write a proper thingy instead
  if (hasArg0Str && ((T.special >= 80 && T.special <= 86) || T.special == 226)) {
    VName sn = VName(*arg0str, VName::AddLower); // 'cause script names are lowercased
    if (sn.GetIndex() != NAME_None) {
      T.arg1 = -(int)sn.GetIndex();
      //GCon->Logf("*** ACS NAMED THING SPECIAL %d: name is (%d) '%s'", T.special, sn.GetIndex(), *sn);
    } else {
      T.arg1 = 0;
    }
  }
}


//==========================================================================
//
//  VLevel::LoadTextMap
//
//==========================================================================
void VLevel::LoadTextMap (int Lump, const mapInfo_t &MInfo) {
  VUdmfParser Parser(Lump);
  Parser.Parse(this, MInfo);

  if (Parser.bExtended) LevelFlags |= LF_Extended;

  // copy vertexes
  NumVertexes = Parser.ParsedVertexes.Num();
  Vertexes = new vertex_t[NumVertexes];
  memcpy(Vertexes, Parser.ParsedVertexes.Ptr(), sizeof(vertex_t)*NumVertexes);

  // check for duplicate vertices
  TMapNC<VertexInfo, int> vmap; // value: in parsed array
  TMapNC<int, int> vremap; // key: original vertex index; value: new vertex index

  for (int f = 0; f < NumVertexes; ++f) {
    VertexInfo vi = VertexInfo(Vertexes[f].x, Vertexes[f].y, f);
    auto ip = vmap.find(vi);
    if (ip) {
      vremap.put(f, *ip);
      GCon->Logf("UDMF: vertex %d is duplicate of vertex %d", f, *ip);
    } else {
      vremap.put(f, f);
      vmap.put(vi, f);
    }
  }

  // copy sectors
  NumSectors = Parser.ParsedSectors.Num();
  Sectors = new sector_t[NumSectors];
  for (int i = 0; i < NumSectors; ++i) {
    Sectors[i] = Parser.ParsedSectors[i];
    Sectors[i].CreateBaseRegion();
  }
  HashSectors();

  // copy line defs
  NumLines = Parser.ParsedLines.Num();
  Lines = new line_t[NumLines];
  for (int i = 0; i < NumLines; ++i) {
    Lines[i] = Parser.ParsedLines[i].L;
    if (Parser.ParsedLines[i].V1Index < 0 || Parser.ParsedLines[i].V1Index >= NumVertexes) {
      Host_Error("Bad vertex index %d (07)", Parser.ParsedLines[i].V1Index);
    }
    if (Parser.ParsedLines[i].V2Index < 0 || Parser.ParsedLines[i].V2Index >= NumVertexes) {
      Host_Error("Bad vertex index %d (08)", Parser.ParsedLines[i].V2Index);
    }

    auto ip0 = vremap.find(Parser.ParsedLines[i].V1Index);
    if (!ip0 || *ip0 < 0 || *ip0 >= NumVertexes) Sys_Error("UDMF: internal error (v0)");
    Lines[i].v1 = &Vertexes[*ip0];

    auto ip1 = vremap.find(Parser.ParsedLines[i].V2Index);
    if (!ip1 || *ip1 < 0 || *ip1 >= NumVertexes) Sys_Error("UDMF: internal error (v1)");
    Lines[i].v2 = &Vertexes[*ip1];

    //Lines[i].v1 = &Vertexes[Parser.ParsedLines[i].V1Index];
    //Lines[i].v2 = &Vertexes[Parser.ParsedLines[i].V2Index];

    //if (i == 1018) GCon->Logf("LD1018: v0=%d; v1=%d; v0=(%f,%f); v1=(%f,%f)", *ip0, *ip1, Vertexes[*ip0].x, Vertexes[*ip0].y, Vertexes[*ip1].x, Vertexes[*ip1].y);
  }

  if (!(LevelFlags&LF_Extended)) {
    // translate level to Hexen format
    GGameInfo->eventTranslateLevel(this);
  }

  // copy side defs
  NumSides = Parser.ParsedSides.Num();
  CreateSides();
  side_t *sd = Sides;
  for (int i = 0; i < NumSides; ++i, ++sd) {
    if (sd->BottomTexture < 0 || sd->BottomTexture >= Parser.ParsedSides.length()) Host_Error("Bad sidedef index (broken UDMF)");
    VUdmfParser::VParsedSide &Src = Parser.ParsedSides[sd->BottomTexture];
    int Spec = sd->MidTexture;
    int Tag = sd->TopTexture;
    *sd = Src.S;

    if (Src.SectorIndex < 0 || Src.SectorIndex >= NumSectors) Host_Error("Bad sector index %d", Src.SectorIndex);
    sd->Sector = &Sectors[Src.SectorIndex];

    switch (Spec) {
      case LNSPEC_LineTranslucent:
        // in BOOM midtexture can be translucency table lump name
        //sd->MidTexture = GTextureManager.CheckNumForName(VName(*Src.MidTexture, VName::AddLower), TEXTYPE_Wall, true);
        //if (sd->MidTexture == -1) sd->MidTexture = GTextureManager.CheckNumForName(VName(*Src.MidTexture, VName::AddLower8), TEXTYPE_Wall, true);
        sd->MidTexture = TexNumForName2(*Src.MidTexture, TEXTYPE_Wall, true);
        if (sd->MidTexture == -1) sd->MidTexture = TexNumForName2(*Src.MidTexture, TEXTYPE_Wall, true);

        if (sd->MidTexture == -1) sd->MidTexture = 0;
        sd->TopTexture = TexNumForName(*Src.TopTexture, TEXTYPE_Wall, false, true);
        sd->BottomTexture = TexNumForName(*Src.BotTexture, TEXTYPE_Wall, false, true);
        break;

      case LNSPEC_TransferHeights:
        sd->MidTexture = TexNumForName(*Src.MidTexture, TEXTYPE_Wall, true, true);
        sd->TopTexture = TexNumForName(*Src.TopTexture, TEXTYPE_Wall, true, true);
        sd->BottomTexture = TexNumForName(*Src.BotTexture, TEXTYPE_Wall, true, true);
        break;

      case LNSPEC_StaticInit:
        {
          bool HaveCol;
          bool HaveFade;
          vuint32 Col;
          vuint32 Fade;
          sd->MidTexture = TexNumForName(*Src.MidTexture, TEXTYPE_Wall, false, true);
          int TmpTop = TexNumOrColour(*Src.TopTexture, TEXTYPE_Wall, HaveCol, Col);
          sd->BottomTexture = TexNumOrColour(*Src.BotTexture, TEXTYPE_Wall, HaveFade, Fade);
          if (HaveCol || HaveFade) {
            for (int j = 0; j < NumSectors; ++j) {
              if (Sectors[j].tag == Tag) {
                if (HaveCol) Sectors[j].params.LightColour = Col;
                if (HaveFade) Sectors[j].params.Fade = Fade;
              }
            }
          }
          sd->TopTexture = TmpTop;
        }
        break;

      default:
        sd->MidTexture = TexNumForName(*Src.MidTexture, TEXTYPE_Wall, false, true);
        sd->TopTexture = TexNumForName(*Src.TopTexture, TEXTYPE_Wall, false, true);
        sd->BottomTexture = TexNumForName(*Src.BotTexture, TEXTYPE_Wall, false, true);
        break;
    }
  }

  // copy things
  NumThings = Parser.ParsedThings.Num();
  Things = new mthing_t[NumThings];
  memcpy(Things, Parser.ParsedThings.Ptr(), sizeof(mthing_t)*NumThings);
}
