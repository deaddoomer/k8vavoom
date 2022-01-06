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
#include <stdlib.h>
#include <string.h>
#include "../../libs/core/core.h"


static int cli_WarnUnknownKeys = 1;

/*static*/ bool cliRegister_udmf_args =
  VParsedArgs::RegisterFlagSet("-Wudmf-unknown-keys", "!show warnings about unknown UDMF keys", &cli_WarnUnknownKeys) &&
  VParsedArgs::RegisterFlagReset("-Wno-udmf-unknown-keys", "!do not show warnings about unknown UDMF keys", &cli_WarnUnknownKeys);


// ////////////////////////////////////////////////////////////////////////// //
#include "../gamedefs.h"
#include "../psim/p_levelinfo.h"


enum {
  ML_PASSUSE_BOOM = 0x0200, // Boom's ML_PASSUSE flag (conflicts with ML_REPEAT_SPECIAL)
};


// ////////////////////////////////////////////////////////////////////////// //
class VUdmfParser {
public:
  // supported namespaces; use bits to have faster cheks
  enum {
    // standard namespaces
    NS_Doom            = 1<<0,
    NS_Heretic         = 1<<1,
    NS_Hexen           = 1<<2,
    NS_Strife          = 1<<3,
    // Vavoom's namespace
    NS_Vavoom          = 1<<4,
    // ZDoom's namespaces
    NS_ZDoom           = 1<<5,
    NS_ZDoomTranslated = 1<<6,
    NS_K8Vavoom        = 1<<7,
  };

  enum {
    TK_None,
    TK_Int,
    TK_Float,
    TK_String,
    TK_Identifier,
  };

  struct VValue {
    VName name;
    int i;
    float f;
  };

  struct VParsedLine {
    line_t L;
    int V1Index;
    int V2Index;
    int index;
    TLocation loc;
    TArray<VValue> userFields;
  };

  struct VParsedSide {
    side_t S;
    VStr TopTexture;
    VStr MidTexture;
    VStr BotTexture;
    int SectorIndex;
    int index;
    TLocation loc;
    TArray<VValue> userFields;
  };

  struct VParsedVertex {
    float x, y;
    float floorz, ceilingz;
    bool hasFloorZ;
    bool hasCeilingZ;
    unsigned hasXY;
    int index;
    TLocation loc;
    bool checked;

    void checkValidity (int ldefidx) {
      if (checked) return;
      if (hasXY != 3) Host_Error("%s: UDMF: incomplete vertex #%d coordinate data (used in linedef #%d)", *loc.toStringNoCol(), index, ldefidx);
      // be conservative here
      if (x < -32767 || x > 32767) Host_Error("%s: UDMF: vertex #%d `x` is out of range (%g) (used in linedef #%d)", *loc.toStringNoCol(), index, x, ldefidx);
      if (y < -32767 || y > 32767) Host_Error("%s: UDMF: vertex #%d `y` is out of range (%g) (used in linedef #%d)", *loc.toStringNoCol(), index, y, ldefidx);
      if (hasFloorZ && (floorz < -32767 || floorz > 32767)) Host_Error("%s: UDMF: vertex #%d `zfloor` is out of range (%g) (used in linedef #%d)", *loc.toStringNoCol(), index, floorz, ldefidx);
      if (hasCeilingZ && (ceilingz < -32767 || ceilingz > 32767)) Host_Error("%s: UDMF: vertex #%d `zceiling` is out of range (%g) (used in linedef #%d)", *loc.toStringNoCol(), index, ceilingz, ldefidx);
      checked = true;
    }
  };

  struct VParsedThing {
    mthing_t T;
    TLocation loc;
    TArray<VValue> userFields;
  };

  struct VParsedSector {
    sector_t S;
    TLocation loc;
    TArray<VValue> userFields;
  };

  VScriptParser sc;
  bool bExtended;
  bool bDoTranslation;
  vuint8 NS;
  VStr NamespaceStr;
  VStr Key;
  int ValType;
  int ValInt;
  float ValFloat;
  VStr Val;
  TLocation KeyLoc;
  TArray<VParsedVertex> ParsedVertexes;
  TArray<VParsedSector> ParsedSectors;
  TArray<VParsedLine> ParsedLines;
  TArray<VParsedSide> ParsedSides;
  TArray<VParsedThing> ParsedThings;

  bool warnUnknownKeys;

  // warning types
  enum {
    WT_VERTEX = 1<<0,
    WT_SECTOR = 1<<1,
    WT_LINE = 1<<2,
    WT_SIDE = 1<<3,
    WT_THING = 1<<4,
  };

  TMapNC<VName, int> keysWarned;
  int srcLump;

  void keyWarning (int wtype);

public:
  VUdmfParser (int Lump);
  void Parse (VLevel *Level, const VMapInfo &MInfo);
  void ParseVertex ();
  void ParseSector (VLevel *Level);
  void ParseLineDef (const VMapInfo &MInfo);
  void ParseSideDef ();
  void ParseThing ();
  void ParseKey ();
  bool CanSilentlyIgnoreKey () const;
  int CheckInt ();
  float CheckFloat ();
  float CheckFloatScalePositive (const char *msg=nullptr, vuint32 *flagptr=nullptr, vuint32 flipflag=0u); // > 0.0f
  bool CheckBool ();
  VStr CheckString ();
  void Flag (int &Field, int Mask);
  void Flag (vuint32 &Field, int Mask);

  void ParseMoreIds (TArray<vint32> &ids);

  bool CheckUserKey (TArray<VValue> &userFields);

  vuint32 CheckColor (vuint32 defval, vuint32 mask=0u);
};


//==========================================================================
//
//  VUdmfParser::VUdmfParser
//
//==========================================================================
VUdmfParser::VUdmfParser (int Lump)
  : sc(*W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump))
  , srcLump(Lump)
{
  warnUnknownKeys = !!cli_WarnUnknownKeys;
}


//==========================================================================
//
//  VUdmfParser::CanSilentlyIgnoreKey
//
//==========================================================================
bool VUdmfParser::CanSilentlyIgnoreKey () const {
  if (!warnUnknownKeys) return true;
  return
    Key.isEmpty() ||
    Key.startsWithCI("user_") ||
    Key.strEquCI("comment");
}


//==========================================================================
//
//  VUdmfParser::ParseKey
//
//==========================================================================
void VUdmfParser::keyWarning (int wtype) {
  if (!wtype) return;
  if (CanSilentlyIgnoreKey()) return;
  VName kn = VName(*Key, VName::AddLower);
  auto pxx = keysWarned.get(kn);
  if (pxx) {
    if (((*pxx)&wtype) == wtype) return; // already warned
    keysWarned.put(kn, (*pxx)|wtype);
  } else {
    keysWarned.put(kn, wtype);
  }
  const char *wtmsg = "unknown";
       if (wtype&WT_VERTEX) wtmsg = "vertex";
  else if (wtype&WT_SECTOR) wtmsg = "sector";
  else if (wtype&WT_LINE) wtmsg = "linedef";
  else if (wtype&WT_SIDE) wtmsg = "sidedef";
  else if (wtype&WT_THING) wtmsg = "thing";
  //sc.Message(va("UDMF: unknown %s property '%s'", wtmsg, *Key));
  GCon->Logf(NAME_Warning, "%s: unknown UDMF %s property '%s' around line %d", *W_FullLumpName(srcLump), wtmsg, *Key, KeyLoc.GetLine());
}


//==========================================================================
//
//  VUdmfParser::ParseKey
//
//==========================================================================
void VUdmfParser::ParseKey () {
  // get key and value
  KeyLoc = sc.GetVCLoc();
  sc.ExpectString();
  Key = sc.String;
  sc.Expect("=");

  ValType = TK_None;
  ValInt = 0;
  ValFloat = 0.0f;
  Val.clear();

  if (sc.CheckQuotedString()) {
    ValType = TK_String;
    Val = sc.String;
  } else if (sc.CheckNumberWithSign()) {
    ValType = TK_Int;
    ValInt = sc.Number;
    Val = VStr(ValInt);
  } else if (sc.CheckFloatWithSign()) {
    ValType = TK_Float;
    ValFloat = sc.Float;
    Val = VStr(ValFloat);
  } else {
    //sc.HostError(va("Cannot parse value '%s' for key '%s'", *sc.String, *Key));
    sc.ExpectString();
    ValType = TK_Identifier;
    Val = sc.String;
    if (Val == ";" || Val.isEmpty()) sc.HostError(va("Cannot parse value '%s' for key '%s'", *sc.String, *Key));
  }

  sc.Expect(";");
}


//==========================================================================
//
//  VUdmfParser::CheckInt
//
//==========================================================================
int VUdmfParser::CheckInt () {
  if (ValType != TK_Int) sc.HostError(va("Integer value expected for key '%s'", *Key));
  return ValInt;
}


//==========================================================================
//
//  VUdmfParser::CheckFloat
//
//==========================================================================
float VUdmfParser::CheckFloat () {
  if (ValType != TK_Int && ValType != TK_Float) sc.HostError(va("Float value expected for key '%s'", *Key));
  const float res = (ValType == TK_Int ? ValInt : ValFloat);
  if (!isFiniteF(res)) sc.HostError(va("Invalid float value for key '%s' (%s)", *Key, *Val));
  return res;
}


//==========================================================================
//
//  VUdmfParser::CheckFloatScalePositive
//
//  > 0.0f
//
//==========================================================================
float VUdmfParser::CheckFloatScalePositive (const char *msg, vuint32 *flagptr, vuint32 flipflag) {
  if (ValType != TK_Int && ValType != TK_Float) sc.HostError(va("Float value expected for key '%s'", *Key));
  float res = (ValType == TK_Int ? ValInt : ValFloat);
  if (!isFiniteF(res)) sc.HostError(va("Invalid float value for key '%s' (%s)", *Key, *Val));
  // let zero scale be 1.0
  if (res == 0.0f) {
    //if (!msg) msg = va("Positive float value expected for key '%s', but got zero", *Key);
    //sc.MessageErr(va("%s (%g)", msg, res));
    res = 1.0f;
  }
  if (res <= 0.0f) {
    if (!flagptr) {
      if (!msg) msg = va("Positive float value expected for key '%s'", *Key);
      if (NS == NS_K8Vavoom) sc.HostError(va("%s (%g)", msg, res));
      sc.MessageErr(va("%s (%g)", msg, res));
    }
    res = -res;
    if (res == 0.0f) res = 1.0f;
    if (flagptr && flipflag) {
      //TODO: check for conflicting flags
      *flagptr ^= flipflag;
    }
  }
  return res;
}


//==========================================================================
//
//  VUdmfParser::CheckBool
//
//==========================================================================
bool VUdmfParser::CheckBool () {
  if (ValType == TK_Identifier) {
    if (Val.strEquCI("true")) return true;
    if (Val.strEquCI("false")) return false;
    if (NS == NS_K8Vavoom) {
      if (Val.strEquCI("on") || Val.strEquCI("tan")) return true;
      if (Val.strEquCI("off") || Val.strEquCI("ona")) return false;
    }
  }
  sc.HostError(va("Boolean value expected for key '%s'", *Key));
  return false;
}


//==========================================================================
//
//  VUdmfParser::CheckString
//
//==========================================================================
VStr VUdmfParser::CheckString () {
  if (ValType != TK_String) { sc.HostError(va("String value expected for key '%s'", *Key)); Val = VStr(); }
  return Val;
}


//==========================================================================
//
//  VUdmfParser::CheckColor
//
//==========================================================================
vuint32 VUdmfParser::CheckColor (vuint32 defval, vuint32 mask) {
  if (ValType == TK_Int) return (ValInt&0xffffff)|mask;
  vuint32 clr = M_ParseColor(*CheckString(), true); // return zero if valid
  if (!clr) return defval;
  return (clr&0xffffffu)|mask;
  //return ParseHex(*CheckString());
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
//  VUdmfParser::ParseMoreIds
//
//==========================================================================
void VUdmfParser::ParseMoreIds (TArray<vint32> &idlist) {
  VStr idstr = CheckString();
  TArray<VStr> list;
  idstr.SplitOnBlanks(list);
  for (int f = 0; f < list.length(); ++f) {
    VStr s = list[f].xstrip();
    if (s.length() == 0) continue;
    int n = VStr::atoi(*s);
    if (!n || n == -1) continue;
    idlist.append(n);
  }
}


//==========================================================================
//
//  VUdmfParser::CheckUserKey
//
//==========================================================================
bool VUdmfParser::CheckUserKey (TArray<VValue> &userFields) {
  if (!Key.startsWithCI("user_")) return false;

  if (Key.length() <= 5) {
    GCon->Logf(NAME_Warning, "%s: invalid UDMF user field '%s'", *W_FullLumpName(srcLump), *Key);
    return false;
  }

  if (ValType != TK_Int && ValType != TK_Float) {
    GCon->Logf(NAME_Warning, "%s: invalid UDMF user field '%s' (string value: %s)", *W_FullLumpName(srcLump), *Key, *Val);
    return false;
  }

  VValue v;
  v.name = VName(*Key, VName::AddLower);
  if (ValType == TK_Int) {
    v.i = ValInt;
    v.f = ValInt;
  } else {
    if (!isFiniteF(ValFloat)) ValFloat = 0.0f;
    v.i = (int)ValFloat;
    v.f = ValFloat;
  }

  userFields.append(v);
  return true;
}


//==========================================================================
//
//  VUdmfParser::Parse
//
//==========================================================================
void VUdmfParser::Parse (VLevel *Level, const VMapInfo &MInfo) {
  sc.SetCMode(true);

  bExtended = false;
  bDoTranslation = true;

  // get namespace name
  sc.Expect("namespace");
  sc.Expect("=");
  sc.ExpectString();
  VStr Namespace = sc.String;
  sc.Expect(";");

  // Vavoom's namespace?
       if (Namespace.strEquCI("Vavoom")) { NS = NS_Vavoom; bExtended = true; bDoTranslation = false; }
  else if (Namespace.strEquCI("k8vavoom")) { NS = NS_K8Vavoom; bExtended = true; bDoTranslation = false; }
  // standard namespaces?
  else if (Namespace.strEquCI("Doom")) NS = NS_Doom;
  else if (Namespace.strEquCI("Heretic")) NS = NS_Heretic;
  else if (Namespace.strEquCI("Hexen")) { NS = NS_Hexen; bExtended = true; bDoTranslation = false; }
  else if (Namespace.strEquCI("Strife")) NS = NS_Strife;
  // ZDoom namespaces?
  else if (Namespace.strEquCI("ZDoom")) { NS = NS_ZDoom; bExtended = true; bDoTranslation = false; }
  else if (Namespace.strEquCI("ZDoomTranslated")) { NS = NS_ZDoomTranslated; bExtended = true; } // we need to perform translation
  else {
    //sc.HostError(va("UDMF: unknown namespace '%s'", *Namespace));
    GCon->Logf(NAME_Warning, "UDMF: unknown namespace '%s'", *Namespace);
    NS = NS_ZDoom; // unknown namespace, assume ZDoom
    bExtended = true;
    bDoTranslation = false;
  }

  NamespaceStr = Namespace;

  while (!sc.AtEnd()) {
         if (sc.Check("vertex")) ParseVertex();
    else if (sc.Check("sector")) ParseSector(Level);
    else if (sc.Check("linedef")) ParseLineDef(MInfo);
    else if (sc.Check("sidedef")) ParseSideDef();
    else if (sc.Check("thing")) ParseThing();
    else {
      VStr slocstr = sc.GetVCLoc().toStringNoCol();
      if (!sc.GetString()) break;
      VStr kn = sc.String;
      GCon->Logf(NAME_Error, "%s:UDMF ignoring wtfidontknow '%s'", *slocstr, *kn);
      if (sc.Check("=")) {
        sc.ExpectString();
        sc.Expect(";");
      } else {
        sc.Expect("{");
        sc.SkipBracketed(true); // bracket eaten
      }
    }
  }
}


//==========================================================================
//
//  VUdmfParser::ParseVertex
//
//==========================================================================
void VUdmfParser::ParseVertex () {
  // allocate a new vertex
  VParsedVertex &v = ParsedVertexes.Alloc();
  memset((void *)&v, 0, sizeof(VParsedVertex));
  v.index = ParsedVertexes.length()-1;
  v.loc = sc.GetVCLoc();

  sc.Expect("{");
  while (!sc.Check("}")) {
    ParseKey();
    if (Key.strEquCI("x")) {
      v.hasXY |= 1u;
      v.x = CheckFloat();
      continue;
    }
    if (Key.strEquCI("y")) {
      v.hasXY |= 2u;
      v.y = CheckFloat();
      continue;
    }
    if (Key.strEquCI("zfloor")) {
      v.floorz = CheckFloat();
      v.hasFloorZ = true;
      continue;
    }
    if (Key.strEquCI("zceiling")) {
      v.ceilingz = CheckFloat();
      v.hasCeilingZ = true;
      continue;
    }
    keyWarning(WT_VERTEX);
  }

  /*
  if (!hasX || !hasY) sc.HostError(va("UDMF: incomplete vertex #%d data", ParsedVertexes.length()-1));
  // be conservative here
  if (v.x < -32767 || v.x > 32767) sc.HostError(va("UDMF: vertex #%d `x` is out of range (%g)", ParsedVertexes.length()-1, v.x));
  if (v.y < -32767 || v.y > 32767) sc.HostError(va("UDMF: vertex #%d `y` is out of range (%g)", ParsedVertexes.length()-1, v.y));
  */
}


//==========================================================================
//
//  VUdmfParser::ParseSector
//
//==========================================================================
void VUdmfParser::ParseSector (VLevel *Level) {
  VParsedSector &S = ParsedSectors.Alloc();
  memset((void *)&S, 0, sizeof(VParsedSector));

  S.S.floor.Set(TVec(0, 0, 1), 0);
  S.S.floor.XScale = 1.0f;
  S.S.floor.YScale = 1.0f;
  S.S.floor.Alpha = 1.0f;
  S.S.floor.MirrorAlpha = 1.0f;
  S.S.floor.LightSourceSector = -1;
  S.S.ceiling.Set(TVec(0, 0, -1), 0);
  S.S.ceiling.XScale = 1.0f;
  S.S.ceiling.YScale = 1.0f;
  S.S.ceiling.Alpha = 1.0f;
  S.S.ceiling.MirrorAlpha = 1.0f;
  S.S.ceiling.LightSourceSector = -1;
  S.S.params.lightlevel = 160;
  S.S.params.LightColor = 0x00ffffff;
  S.S.seqType = -1; // default seqType
  S.S.Gravity = 1.0f;  // default sector gravity of 1.0
  S.S.Zone = -1;
  S.S.Damage = 0;
  S.S.DamageType = NAME_None; // default
  S.S.DamageInterval = 0; // default interval
  S.S.DamageLeaky = 0;

  bool fpvalid[4] = {false, false, false, false};
  float fval[4] = {0,0,0,0};
  bool cpvalid[4] = {false, false, false, false};
  float cval[4] = {0,0,0,0};

  sc.Expect("{");
  while (!sc.Check("}")) {
    ParseKey();

    if (CheckUserKey(S.userFields)) continue;

    if (Key.strEquCI("heightfloor")) {
      float FVal = CheckFloat();
      S.S.floor.dist = FVal;
      S.S.floor.TexZ = FVal;
      S.S.floor.minz = FVal;
      S.S.floor.maxz = FVal;
      continue;
    }

    if (Key.strEquCI("heightceiling")) {
      float FVal = CheckFloat();
      S.S.ceiling.dist = -FVal;
      S.S.ceiling.TexZ = FVal;
      S.S.ceiling.minz = FVal;
      S.S.ceiling.maxz = FVal;
      continue;
    }

    if (Key.strEquCI("texturefloor")) { S.S.floor.pic = Level->TexNumForName(*Val, TEXTYPE_Flat); continue; }
    if (Key.strEquCI("textureceiling")) { S.S.ceiling.pic = Level->TexNumForName(*Val, TEXTYPE_Flat); continue; }
    if (Key.strEquCI("lightlevel")) { S.S.params.lightlevel = clampval(CheckInt(), 0, 255); continue; }
    if (Key.strEquCI("special")) { S.S.special = CheckInt(); continue; }
    if (Key.strEquCI("id")) { S.S.sectorTag = CheckInt(); continue; }

    // extensions
    if (NS&(NS_K8Vavoom|NS_Vavoom|NS_ZDoom|NS_ZDoomTranslated)) {
      if (Key.strEquCI("xpanningfloor")) { S.S.floor.xoffs = CheckFloat(); continue; }
      if (Key.strEquCI("ypanningfloor")) { S.S.floor.yoffs = CheckFloat(); continue; }
      if (Key.strEquCI("xpanningceiling")) { S.S.ceiling.xoffs = CheckFloat(); continue; }
      if (Key.strEquCI("ypanningceiling")) { S.S.ceiling.yoffs = CheckFloat(); continue; }
      if (Key.strEquCI("xscalefloor")) { S.S.floor.XScale = CheckFloatScalePositive(va("invalid floor x scale for sector #%d", ParsedSectors.length()-1), &S.S.floor.flipFlags, SPF_FLIP_X|SPF_FLIP_X); continue; }
      if (Key.strEquCI("yscalefloor")) { S.S.floor.YScale = CheckFloatScalePositive(va("invalid floor y scale for sector #%d", ParsedSectors.length()-1), &S.S.floor.flipFlags, SPF_FLIP_Y|SPF_FLIP_Y); continue; }
      if (Key.strEquCI("xscaleceiling")) { S.S.ceiling.XScale = CheckFloatScalePositive(va("invalid ceiling x scale for sector #%d", ParsedSectors.length()-1)); continue; }
      if (Key.strEquCI("yscaleceiling")) { S.S.ceiling.YScale = CheckFloatScalePositive(va("invalid ceiling y scale for sector #%d", ParsedSectors.length()-1)); continue; }
      if (Key.strEquCI("rotationfloor")) { S.S.floor.Angle = AngleMod(CheckFloat()); continue; }
      if (Key.strEquCI("rotationceiling")) { S.S.ceiling.Angle = AngleMod(CheckFloat()); continue; }
      if (Key.strEquCI("gravity")) { S.S.Gravity = CheckFloat(); continue; }
      if (Key.strEquCI("lightcolor")) { S.S.params.LightColor = CheckColor(0x00ffffffu); continue; }
      if (Key.strEquCI("fadecolor")) { S.S.params.Fade = CheckColor(0u); continue; }
      if (Key.strEquCI("silent")) { Flag(S.S.SectorFlags, sector_t::SF_Silent); continue; }
      if (Key.strEquCI("nofallingdamage")) { Flag(S.S.SectorFlags, sector_t::SF_NoFallingDamage); continue; }

      if (Key.strEquCI("floorplane_a")) { fval[0] = CheckFloat(); fpvalid[0] = true; continue; }
      if (Key.strEquCI("floorplane_b")) { fval[1] = CheckFloat(); fpvalid[1] = true; continue; }
      if (Key.strEquCI("floorplane_c")) { fval[2] = CheckFloat(); fpvalid[2] = true; continue; }
      if (Key.strEquCI("floorplane_d")) { fval[3] = CheckFloat(); fpvalid[3] = true; continue; }

      if (Key.strEquCI("ceilingplane_a")) { cval[0] = CheckFloat(); cpvalid[0] = true; continue; }
      if (Key.strEquCI("ceilingplane_b")) { cval[1] = CheckFloat(); cpvalid[1] = true; continue; }
      if (Key.strEquCI("ceilingplane_c")) { cval[2] = CheckFloat(); cpvalid[2] = true; continue; }
      if (Key.strEquCI("ceilingplane_d")) { cval[3] = CheckFloat(); cpvalid[3] = true; continue; }

      if (Key.strEquCI("moreids")) { ParseMoreIds(S.S.moreTags); continue; }

      if (Key.strEquCI("lightfloor")) { S.S.params.lightFloor = clampval(CheckInt(), 0, 255); continue; }
      if (Key.strEquCI("lightceiling")) { S.S.params.lightCeiling = clampval(CheckInt(), 0, 255); continue; }

      if (Key.strEquCI("lightfloorabsolute")) { Flag(S.S.params.lightFCFlags, sec_params_t::LFC_FloorLight_Abs); continue; }
      if (Key.strEquCI("lightceilingabsolute")) { Flag(S.S.params.lightFCFlags, sec_params_t::LFC_CeilingLight_Abs); continue; }

      if (Key.strEquCI("floorglowcolor")) { S.S.params.glowFloor = CheckColor(0u, 0xff000000u); continue; }
      if (Key.strEquCI("ceilingglowcolor")) { S.S.params.glowCeiling = CheckColor(0u, 0xff000000u); continue; }

      if (Key.strEquCI("floor_reflect")) { S.S.floor.MirrorAlpha = 1.0f-clampval(CheckFloat(), 0.0f, 1.0f); continue; }
      if (Key.strEquCI("ceiling_reflect")) { S.S.ceiling.MirrorAlpha = 1.0f-clampval(CheckFloat(), 0.0f, 1.0f); continue; }

      if (Key.strEquCI("floorglowheight")) { S.S.params.glowFloorHeight = CheckFloat(); continue; }
      if (Key.strEquCI("ceilingglowheight")) { S.S.params.glowCeilingHeight = CheckFloat(); continue; }

      if (Key.strEquCI("waterzone")) { S.S.params.contents = (CheckBool() ? 1 : 0); continue; } // 1 is `CONTENTS_WATER`

      if (Key.strEquCI("hidden")) { if (CheckBool()) S.S.SectorFlags |= sector_t::SF_Hidden; continue; }
      if (Key.strEquCI("norespawn")) { if (CheckBool()) S.S.SectorFlags |= sector_t::SF_NoPlayerRespawn; continue; }

      // sector damage properties
      if (Key.strEquCI("damageamount")) { S.S.Damage = CheckInt(); continue; }
      if (Key.strEquCI("damagetype")) {
        VStr dmg = CheckString();
        VStr dmgs = dmg.xstrip();
        if (dmgs.isEmpty() || dmgs.strEquCI("none") || dmgs.strEquCI("normal")) dmg.clear();
        S.S.DamageType = VName(*dmg);
        continue;
      }
      if (Key.strEquCI("damageinterval")) {
        S.S.DamageInterval = CheckInt();
        // "0" means "default" in the engine; fix it
        if (S.S.DamageInterval == 0) S.S.DamageInterval = -1;
        continue;
      }
      if (Key.strEquCI("leakiness")) {
        S.S.DamageLeaky = CheckInt();
        // "0" means "default" in the engine; fix it
        if (S.S.DamageLeaky == 0) S.S.DamageLeaky = -1;
        continue;
      }
    }

    if (NS == NS_K8Vavoom) {
      /*
      if (Key.strEquCI("flip_x_floor")) { Flag(S.S.Flags, SDF_FLIP_X_TOP); continue; }
      if (Key.strEquCI("flip_y_floor")) { Flag(S.S.Flags, SDF_FLIP_Y_TOP); continue; }
      if (Key.strEquCI("flip_x_ceiling")) { Flag(S.S.Flags, SDF_FLIP_X_MID); continue; }
      if (Key.strEquCI("flip_y_ceiling")) { Flag(S.S.Flags, SDF_FLIP_Y_MID); continue; }
      */
    }

    keyWarning(WT_SECTOR);
  }

  if (S.S.params.glowFloorHeight >= 1) S.S.params.lightFCFlags |= sec_params_t::LFC_FloorLight_Glow; else S.S.params.glowFloorHeight = 0;
  if (S.S.params.glowCeilingHeight >= 1) S.S.params.lightFCFlags |= sec_params_t::LFC_CeilingLight_Glow; else S.S.params.glowCeilingHeight = 0;

  // setup slopes with floor/ceiling planes

  // floor
  if (fpvalid[0] && fpvalid[1] && fpvalid[2] && fpvalid[3]) {
    TPlane pl;
    pl.normal = TVec(fval[0], fval[1], fval[2]).normalise();
    if (pl.normal.isValid() && !pl.normal.isZero() && fabsf(pl.normal.z) > 0.01f) {
      pl.dist = -fval[3];
      if (pl.normal.z < 0) pl.FlipInPlace();
      *((TPlane *)&S.S.floor) = pl;
    }
  }

  // ceiling
  if (cpvalid[0] && cpvalid[1] && cpvalid[2] && cpvalid[3]) {
    TPlane pl;
    pl.normal = TVec(cval[0], cval[1], cval[2]).normalise();
    if (pl.normal.isValid() && !pl.normal.isZero() && fabsf(pl.normal.z) > 0.01f) {
      pl.dist = -cval[3];
      if (pl.normal.z > 0) pl.FlipInPlace();
      *((TPlane *)&S.S.ceiling) = pl;
    }
  }
}


//==========================================================================
//
//  VUdmfParser::ParseLineDef
//
//==========================================================================
void VUdmfParser::ParseLineDef (const VMapInfo &/*MInfo*/) {
  VParsedLine &L = ParsedLines.Alloc();
  memset((void *)&L, 0, sizeof(VParsedLine));
  L.index = ParsedLines.length()-1;
  L.loc = sc.GetVCLoc();
  L.V1Index = -1;
  L.V2Index = -1;
  L.L.locknumber = 0;
  L.L.alpha = 1.0f;
  L.L.lineTag = (bExtended ? -1 : 0);
  L.L.sidenum[0] = -1;
  L.L.sidenum[1] = -1;
  //if (MInfo.Flags&VLevelInfo::LIF_ClipMidTex) L.L.flags |= ML_CLIP_MIDTEX;
  //if (MInfo.Flags&VLevelInfo::LIF_WrapMidTex) L.L.flags |= ML_WRAP_MIDTEX;
  //bool HavePassUse = false;

  VStr arg0str;
  bool hasArg0Str = false;
  bool hasArgs = false;
  bool hasTranslucent = false;

  sc.Expect("{");
  while (!sc.Check("}")) {
    ParseKey();

    if (CheckUserKey(L.userFields)) continue;

    if (Key.strEquCI("id")) { L.L.lineTag = CheckInt(); continue; }
    if (Key.strEquCI("v1")) { L.V1Index = CheckInt(); continue; }
    if (Key.strEquCI("v2")) { L.V2Index = CheckInt(); continue; }
    if (Key.strEquCI("blocking")) { Flag(L.L.flags, ML_BLOCKING); continue; }
    if (Key.strEquCI("blockmonsters")) { Flag(L.L.flags, ML_BLOCKMONSTERS); continue; }
    if (Key.strEquCI("twosided")) { Flag(L.L.flags, ML_TWOSIDED); continue; }
    if (Key.strEquCI("dontpegtop")) { Flag(L.L.flags, ML_DONTPEGTOP); continue; }
    if (Key.strEquCI("dontpegbottom")) { Flag(L.L.flags, ML_DONTPEGBOTTOM); continue; }
    if (Key.strEquCI("secret")) { Flag(L.L.flags, ML_SECRET); continue; }
    if (Key.strEquCI("blocksound")) { Flag(L.L.flags, ML_SOUNDBLOCK); continue; }
    if (Key.strEquCI("dontdraw")) { Flag(L.L.flags, ML_DONTDRAW); continue; }
    if (Key.strEquCI("mapped")) { Flag(L.L.flags, ML_MAPPED); continue; }
    if (Key.strEquCI("special")) { L.L.special = CheckInt(); continue; }
    if (Key.strEquCI("arg0")) { hasArgs = true; L.L.arg1 = CheckInt(); continue; }
    if (Key.strEquCI("arg1")) { hasArgs = true; L.L.arg2 = CheckInt(); continue; }
    if (Key.strEquCI("arg2")) { hasArgs = true; L.L.arg3 = CheckInt(); continue; }
    if (Key.strEquCI("arg3")) { hasArgs = true; L.L.arg4 = CheckInt(); continue; }
    if (Key.strEquCI("arg4")) { hasArgs = true; L.L.arg5 = CheckInt(); continue; }
    if (Key.strEquCI("sidefront")) { L.L.sidenum[0] = CheckInt(); continue; }
    if (Key.strEquCI("sideback")) { L.L.sidenum[1] = CheckInt(); continue; }

    // doom specific flags
    if (NS&(NS_Doom|NS_K8Vavoom|NS_Vavoom|NS_ZDoom|NS_ZDoomTranslated)) {
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
    if (NS&(NS_Strife|NS_K8Vavoom|NS_Vavoom|NS_ZDoom|NS_ZDoomTranslated)) {
      if (Key.strEquCI("arg0str")) {
        //FIXME: actually, this is valid only for ACS specials
        arg0str = CheckString();
        hasArg0Str = true;
        hasArgs = true;
        continue;
      }

      if (Key.strEquCI("translucent")) { hasTranslucent = CheckBool(); continue; }
      if (Key.strEquCI("jumpover")) { Flag(L.L.flags, ML_RAILING); continue; }
      if (Key.strEquCI("blockfloaters")) { Flag(L.L.flags, ML_BLOCK_FLOATERS); continue; }
    }

    // hexen's extensions
    if (NS&(NS_Hexen|NS_K8Vavoom|NS_Vavoom|NS_ZDoom|NS_ZDoomTranslated)) {
      if (Key.strEquCI("playercross")) { Flag(L.L.SpacFlags, SPAC_Cross); continue; }
      if (Key.strEquCI("playeruse")) { Flag(L.L.SpacFlags, SPAC_Use); continue; }
      if (Key.strEquCI("playeruseback")) { Flag(L.L.SpacFlags, SPAC_UseBack); continue; }
      if (Key.strEquCI("monstercross")) { Flag(L.L.SpacFlags, SPAC_MCross); continue; }
      if (Key.strEquCI("monsteruse")) { Flag(L.L.SpacFlags, SPAC_MUse); continue; }
      if (Key.strEquCI("impact")) { Flag(L.L.SpacFlags, SPAC_Impact); continue; }
      if (Key.strEquCI("playerpush")) { Flag(L.L.SpacFlags, SPAC_Push); continue; }
      if (Key.strEquCI("monsterpush")) { Flag(L.L.SpacFlags, SPAC_MPush); continue; }
      if (Key.strEquCI("missilecross")) { Flag(L.L.SpacFlags, SPAC_PCross); continue; }
      if (Key.strEquCI("repeatspecial")) { Flag(L.L.flags, ML_REPEAT_SPECIAL); continue; }
    }

    // extensions
    if (NS&(NS_K8Vavoom|NS_Vavoom|NS_ZDoom|NS_ZDoomTranslated)) {
      if (Key.strEquCI("alpha")) {
        L.L.alpha = clampval(CheckFloat(), 0.0f, 1.0f);
        continue;
      }

      if (Key.strEquCI("renderstyle")) {
        VStr RS = CheckString();
             if (RS.strEquCI("translucent")) L.L.flags &= ~ML_ADDITIVE;
        else if (RS.strEquCI("add")) L.L.flags |= ML_ADDITIVE;
        else sc.Message(va("Bad linedef render style '%s'", *RS));
        continue;
      }

      if (Key.strEquCI("anycross")) { Flag(L.L.SpacFlags, SPAC_AnyCross); continue; }
      if (Key.strEquCI("monsteractivate")) { Flag(L.L.flags, ML_MONSTERSCANACTIVATE); continue; }
      if (Key.strEquCI("blockplayers")) { Flag(L.L.flags, ML_BLOCKPLAYERS); continue; }
      if (Key.strEquCI("blockeverything")) { Flag(L.L.flags, ML_BLOCKEVERYTHING); continue; }
      if (Key.strEquCI("firstsideonly")) { Flag(L.L.flags, ML_FIRSTSIDEONLY); continue; }
      if (Key.strEquCI("zoneboundary")) { Flag(L.L.flags, ML_ZONEBOUNDARY); continue; }
      if (Key.strEquCI("clipmidtex")) { Flag(L.L.flags, ML_CLIP_MIDTEX); continue; }
      if (Key.strEquCI("wrapmidtex")) { Flag(L.L.flags, ML_WRAP_MIDTEX); continue; }
      if (Key.strEquCI("blockprojectiles")) { Flag(L.L.flags, ML_BLOCKPROJECTILE); continue; }
      if (Key.strEquCI("blockuse")) { Flag(L.L.flags, ML_BLOCKUSE); continue; }
      if (Key.strEquCI("blocksight")) { Flag(L.L.flags, ML_BLOCKSIGHT); continue; }
      if (Key.strEquCI("blockhitscan")) { Flag(L.L.flags, ML_BLOCKHITSCAN); continue; }
      if (Key.strEquCI("Checkswitchrange")) { Flag(L.L.flags, ML_CHECKSWITCHRANGE); continue; } //TODO
      if (Key.strEquCI("midtex3d")) { Flag(L.L.flags, ML_3DMIDTEX); continue; }
      if (Key.strEquCI("locknumber")) { L.L.locknumber = CheckInt(); continue; }
      if (Key.strEquCI("moreids")) { ParseMoreIds(L.L.moreTags); continue; }

      //k8: not implemented
      if (Key.strEquCI("midtex3dimpassible")) { if (CheckBool()) continue; }
    }

    keyWarning(WT_LINE);
  }

  // dunno, let "alpha" property has the priority here
  if (hasTranslucent && L.L.alpha >= 1.0f) L.L.alpha = 0.666f;

  if (L.L.sidenum[0] < 0 && L.L.sidenum[1] < 0) GCon->Logf(NAME_Warning, "%s: UDMF: linedef #%d has no sidedefs", *L.loc.toStringNoCol(), L.index);

  // hack for vanilla-style doom maps (per UDMF specs)
  if (NS&(NS_Doom|NS_Heretic|NS_Hexen|NS_Strife)) {
    //FIXME: this should be backed with proper code changes
    //       see discussion here: https://www.doomworld.com/forum/topic/86102
    if (!hasArgs && L.L.lineTag > 0) {
      L.L.arg1 = L.L.lineTag;
    }
  }

  //FIXME: actually, this is valid only for special runacs range for now; write a proper thingy instead
  if ((L.L.special >= 80 && L.L.special <= 85) || L.L.special == 226) {
    if (hasArg0Str) {
      VName sn = VName(*arg0str, VName::AddLower); // 'cause script names are lowercased
      if (sn.GetIndex() != NAME_None) {
        L.L.arg1 = -(int)sn.GetIndex();
        //GCon->Logf("*** LINE #%d: ACS NAMED LINE SPECIAL %d: name is (%d) '%s'", ParsedLines.length()-1, L.L.special, sn.GetIndex(), *sn);
      } else {
        L.L.special = 0;
        L.L.arg1 = 0;
      }
    } else {
      // not a string
      if (L.L.arg1 < 0) {
        L.L.special = 0;
        L.L.arg1 = 0;
      }
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
  S.index = ParsedSides.length()-1;
  S.loc = sc.GetVCLoc();
  S.TopTexture = "-";
  S.MidTexture = "-";
  S.BotTexture = "-";
  S.S.Top.ScaleX = S.S.Top.ScaleY = 1.0f;
  S.S.Bot.ScaleX = S.S.Bot.ScaleY = 1.0f;
  S.S.Mid.ScaleX = S.S.Mid.ScaleY = 1.0f;
  float XOffs = 0;
  float YOffs = 0;
  bool hasSector = false;

  sc.Expect("{");
  while (!sc.Check("}")) {
    ParseKey();

    if (CheckUserKey(S.userFields)) continue;

    if (Key.strEquCI("offsetx")) { XOffs = CheckFloat(); continue; }
    if (Key.strEquCI("offsety")) { YOffs = CheckFloat(); continue; }
    if (Key.strEquCI("texturetop")) { S.TopTexture = CheckString(); continue; }
    if (Key.strEquCI("texturebottom")) { S.BotTexture = CheckString(); continue; }
    if (Key.strEquCI("texturemiddle")) { S.MidTexture = CheckString(); continue; }
    if (Key.strEquCI("sector")) { hasSector = true; S.SectorIndex = CheckInt(); continue; }

    // extensions
    if (NS&(NS_K8Vavoom|NS_Vavoom|NS_ZDoom|NS_ZDoomTranslated)) {
      if (Key.strEquCI("offsetx_top")) { S.S.Top.TextureOffset = CheckFloat(); continue; }
      if (Key.strEquCI("offsety_top")) { S.S.Top.RowOffset = CheckFloat(); continue; }
      if (Key.strEquCI("offsetx_mid")) { S.S.Mid.TextureOffset = CheckFloat(); continue; }
      if (Key.strEquCI("offsety_mid")) { S.S.Mid.RowOffset = CheckFloat(); continue; }
      if (Key.strEquCI("offsetx_bottom")) { S.S.Bot.TextureOffset = CheckFloat(); continue; }
      if (Key.strEquCI("offsety_bottom")) { S.S.Bot.RowOffset = CheckFloat(); continue; }
      if (Key.strEquCI("scalex_top")) { S.S.Top.ScaleX = CheckFloatScalePositive(va("invalid x top scale for side #%d", ParsedSides.length()-1), &S.S.Top.Flags, STP_FLIP_X|STP_BROKEN_FLIP_X); continue; }
      if (Key.strEquCI("scaley_top")) { S.S.Top.ScaleY = CheckFloatScalePositive(va("invalid y top scale for side #%d", ParsedSides.length()-1), &S.S.Top.Flags, STP_FLIP_Y|STP_BROKEN_FLIP_Y); continue; }
      if (Key.strEquCI("scalex_mid")) { S.S.Mid.ScaleX = CheckFloatScalePositive(va("invalid x mid scale for side #%d", ParsedSides.length()-1), &S.S.Mid.Flags, STP_FLIP_X|STP_BROKEN_FLIP_X); continue; }
      if (Key.strEquCI("scaley_mid")) { S.S.Mid.ScaleY = CheckFloatScalePositive(va("invalid y mid scale for side #%d", ParsedSides.length()-1), &S.S.Mid.Flags, STP_FLIP_Y|STP_BROKEN_FLIP_Y); continue; }
      if (Key.strEquCI("scalex_bottom")) { S.S.Bot.ScaleX = CheckFloatScalePositive(va("invalid x bottom scale for side #%d", ParsedSides.length()-1), &S.S.Bot.Flags, STP_FLIP_X|STP_BROKEN_FLIP_X); continue; }
      if (Key.strEquCI("scaley_bottom")) { S.S.Bot.ScaleY = CheckFloatScalePositive(va("invalid y bottom scale for side #%d", ParsedSides.length()-1), &S.S.Bot.Flags, STP_FLIP_Y|STP_BROKEN_FLIP_Y); continue; }
      if (Key.strEquCI("light")) { S.S.Light = clampval(CheckInt(), 0, 255); continue; }
      if (Key.strEquCI("lightabsolute")) { Flag(S.S.Flags, SDF_ABSLIGHT); continue; }
      if (Key.strEquCI("wrapmidtex")) { Flag(S.S.Flags, SDF_WRAPMIDTEX); continue; }
      if (Key.strEquCI("clipmidtex")) { Flag(S.S.Flags, SDF_CLIPMIDTEX); continue; }
      if (Key.strEquCI("nofakecontrast")) { Flag(S.S.Flags, SDF_NOFAKECTX); continue; }
      if (Key.strEquCI("smoothlighting")) { Flag(S.S.Flags, SDF_SMOOTHLIT); continue; }
      if (Key.strEquCI("nodecals")) { Flag(S.S.Flags, SDF_NODECAL); continue; }
    }

    if (NS == NS_K8Vavoom) {
      if (Key.strEquCI("flip_x_top")) { Flag(S.S.Top.Flags, STP_FLIP_X); continue; }
      if (Key.strEquCI("flip_y_top")) { Flag(S.S.Top.Flags, STP_FLIP_Y); continue; }
      if (Key.strEquCI("flip_x_mid")) { Flag(S.S.Mid.Flags, STP_FLIP_X); continue; }
      if (Key.strEquCI("flip_y_mid")) { Flag(S.S.Mid.Flags, STP_FLIP_Y); continue; }
      if (Key.strEquCI("flip_x_bottom")) { Flag(S.S.Bot.Flags, STP_FLIP_X); continue; }
      if (Key.strEquCI("flip_y_bottom")) { Flag(S.S.Bot.Flags, STP_FLIP_Y); continue; }

      if (Key.strEquCI("rotation_top")) { S.S.Top.BaseAngle = AngleMod(CheckFloat()); GCon->Logf(NAME_Warning, "%s: UDMF: side #%d is using unimplemented property '%s'", *KeyLoc.toStringNoCol(), S.index, *Key); continue; }
      if (Key.strEquCI("rotation_mid")) { S.S.Mid.BaseAngle = AngleMod(CheckFloat()); GCon->Logf(NAME_Warning, "%s: UDMF: side #%d is using unimplemented property '%s'", *KeyLoc.toStringNoCol(), S.index, *Key); continue; }
      if (Key.strEquCI("rotation_bottom")) { S.S.Bot.BaseAngle = AngleMod(CheckFloat()); GCon->Logf(NAME_Warning, "%s: UDMF: side #%d is using unimplemented property '%s'", *KeyLoc.toStringNoCol(), S.index, *Key); continue; }
    }

    keyWarning(WT_SIDE);
  }

  if (!hasSector) GCon->Logf(NAME_Warning, "%s: UDMF: side #%d has no assigned sector", *S.loc.toStringNoCol(), S.index);

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
  VParsedThing &T = ParsedThings.Alloc();
  memset((void *)&T, 0, sizeof(VParsedThing));

  VStr arg0str;
  bool hasArg0Str = false;
  unsigned hasXY = 0; // bitmask
  bool hasType = false;

  sc.Expect("{");
  while (!sc.Check("}")) {
    ParseKey();

    if (CheckUserKey(T.userFields)) continue;

    if (Key.strEquCI("x")) { hasXY |= 1u; T.T.x = CheckFloat(); continue; }
    if (Key.strEquCI("y")) { hasXY |= 2u; T.T.y = CheckFloat(); continue; }
    if (Key.strEquCI("height")) { T.T.height = CheckFloat(); continue; }
    if (Key.strEquCI("angle")) { T.T.angle = CheckFloat(); continue; }
    if (Key.strEquCI("pitch")) { T.T.pitch = CheckFloat(); T.T.udmfExFlags |= mthing_t::MTHF_UsePitch; continue; }
    if (Key.strEquCI("roll")) { T.T.roll = CheckFloat(); T.T.udmfExFlags |= mthing_t::MTHF_UseRoll; continue; }
    if (Key.strEquCI("yaw")) { T.T.angle = CheckFloat(); continue; }
    if (Key.strEquCI("type")) { hasType = true; T.T.type = CheckInt(); continue; }
    if (Key.strEquCI("ambush")) { Flag(T.T.options, MTF_AMBUSH); continue; }
    if (Key.strEquCI("single")) { Flag(T.T.options, MTF_GSINGLE); continue; }
    if (Key.strEquCI("dm")) { Flag(T.T.options, MTF_GDEATHMATCH); continue; }
    if (Key.strEquCI("coop")) { Flag(T.T.options, MTF_GCOOP); continue; }
    if (Key.strEquCI("scalex")) { T.T.scaleX = CheckFloat(); T.T.udmfExFlags |= mthing_t::MTHF_UseScaleX; continue; }
    if (Key.strEquCI("scaley")) { T.T.scaleY = CheckFloat(); T.T.udmfExFlags |= mthing_t::MTHF_UseScaleY; continue; }
    if (Key.strEquCI("scale")) { T.T.scaleX = T.T.scaleY = CheckFloat(); T.T.udmfExFlags |= mthing_t::MTHF_UseScaleX|mthing_t::MTHF_UseScaleY; continue; }
    if (Key.strEquCI("gravity")) { T.T.gravity = CheckFloat(); T.T.udmfExFlags |= mthing_t::MTHF_UseGravity; continue; }

    // skills (up to, and including 16)
    if (Key.startsWithCI("skill")) {
      int skill = -1;
      if (VStr::convertInt((*Key)+5, &skill)) {
        if (skill >= 1 && skill <= 16) {
          Flag(T.T.SkillClassFilter, 0x0001<<(skill-1));
          continue;
        }
      }
    }

    if (NS&(NS_Hexen|NS_K8Vavoom|NS_Vavoom|NS_ZDoom|NS_ZDoomTranslated)) {
      // MBF friendly flag
      if (Key.strEquCI("friend")) { Flag(T.T.options, MTF_FRIENDLY); continue; }
      // strife specific flags
      if (Key.strEquCI("standing")) { Flag(T.T.options, MTF_STANDSTILL); continue; }
      if (Key.strEquCI("strifeally")) { Flag(T.T.options, MTF_FRIENDLY); continue; }
      if (Key.strEquCI("translucent")) { Flag(T.T.options, MTF_SHADOW); continue; }
      if (Key.strEquCI("invisible")) { Flag(T.T.options, MTF_ALTSHADOW); continue; }
    }

    // hexen's extensions
    if (NS&(NS_Hexen|NS_K8Vavoom|NS_Vavoom|NS_ZDoom|NS_ZDoomTranslated)) {
      if (Key.strEquCI("arg0str")) {
        //FIXME: actually, this is valid only for ACS specials
        //       this can be color name for dynamic lights too
        //T.T.arg0 = M_ParseColor(*CheckString())&0xffffffu;
        arg0str = CheckString();
        hasArg0Str = true;
        T.T.arg0str = arg0str;
        T.T.udmfExFlags |= mthing_t::MTHF_UseArg0Str;
        continue;
      }

      if (Key.strEquCI("id")) { T.T.tid = CheckInt(); continue; }
      if (Key.strEquCI("dormant")) { Flag(T.T.options, MTF_DORMANT); continue; }
      if (Key.strEquCI("special")) { T.T.special = CheckInt(); continue; }
      if (Key.strEquCI("arg0")) { T.T.args[0] = CheckInt(); continue; }
      if (Key.strEquCI("arg1")) { T.T.args[1] = CheckInt(); continue; }
      if (Key.strEquCI("arg2")) { T.T.args[2] = CheckInt(); continue; }
      if (Key.strEquCI("arg3")) { T.T.args[3] = CheckInt(); continue; }
      if (Key.strEquCI("arg4")) { T.T.args[4] = CheckInt(); continue; }

      if (Key.strEquCI("health")) { T.T.health = CheckFloat(); T.T.udmfExFlags |= mthing_t::MTHF_UseHealth; continue; }

      if (Key.strEquCI("renderstyle")) {
        VStr s = CheckString().xstrip();
        if (s.length() == 0) continue;
        if (s.strEquCI("none")) { T.T.renderStyle = STYLE_None; T.T.udmfExFlags |= mthing_t::MTHF_UseRenderStyle; continue; }
        if (s.strEquCI("normal")) { T.T.renderStyle = STYLE_Normal; T.T.udmfExFlags |= mthing_t::MTHF_UseRenderStyle; continue; }
        if (s.strEquCI("add")) { T.T.renderStyle = STYLE_Add; T.T.udmfExFlags |= mthing_t::MTHF_UseRenderStyle; continue; }
        if (s.strEquCI("additive")) { T.T.renderStyle = STYLE_Add; T.T.udmfExFlags |= mthing_t::MTHF_UseRenderStyle; continue; }
        if (s.strEquCI("sub")) { T.T.renderStyle = STYLE_Subtract; T.T.udmfExFlags |= mthing_t::MTHF_UseRenderStyle; continue; }
        if (s.strEquCI("subtract")) { T.T.renderStyle = STYLE_Subtract; T.T.udmfExFlags |= mthing_t::MTHF_UseRenderStyle; continue; }
        if (s.strEquCI("subtractive")) { T.T.renderStyle = STYLE_Subtract; T.T.udmfExFlags |= mthing_t::MTHF_UseRenderStyle; continue; }
        if (s.strEquCI("stencil")) { T.T.renderStyle = STYLE_Stencil; T.T.udmfExFlags |= mthing_t::MTHF_UseRenderStyle; continue; }
        if (s.strEquCI("translucentstencil")) { T.T.renderStyle = STYLE_TranslucentStencil; T.T.udmfExFlags |= mthing_t::MTHF_UseRenderStyle; continue; }
        if (s.strEquCI("addstencil")) { T.T.renderStyle = STYLE_AddStencil; T.T.udmfExFlags |= mthing_t::MTHF_UseRenderStyle; continue; }
        if (s.strEquCI("shaded")) { T.T.renderStyle = STYLE_Shaded; T.T.udmfExFlags |= mthing_t::MTHF_UseRenderStyle; continue; }
        if (s.strEquCI("addshaded")) { T.T.renderStyle = STYLE_AddShaded; T.T.udmfExFlags |= mthing_t::MTHF_UseRenderStyle; continue; }
        if (s.strEquCI("translucent")) { T.T.renderStyle = STYLE_Translucent; T.T.udmfExFlags |= mthing_t::MTHF_UseRenderStyle; continue; }
        if (s.strEquCI("fuzzy")) { T.T.renderStyle = STYLE_Fuzzy; T.T.udmfExFlags |= mthing_t::MTHF_UseRenderStyle; continue; }
        if (s.strEquCI("optfuzzy")) { T.T.renderStyle = STYLE_OptFuzzy; T.T.udmfExFlags |= mthing_t::MTHF_UseRenderStyle; continue; }
        if (s.strEquCI("soultrans")) { T.T.renderStyle = STYLE_SoulTrans; T.T.udmfExFlags |= mthing_t::MTHF_UseRenderStyle; continue; }
        if (s.strEquCI("shadow")) { T.T.renderStyle = STYLE_Shadow; T.T.udmfExFlags |= mthing_t::MTHF_UseRenderStyle; continue; }
        if (s.strEquCI("k8dark")) { T.T.renderStyle = STYLE_Dark; T.T.udmfExFlags |= mthing_t::MTHF_UseRenderStyle; continue; }
        //sc.Message(va("UDMF: `renderstyle` property with value \"%s\" is not supported yet", *s));
        keyWarning(WT_THING);
        continue;
      }

      if (Key.strEquCI("fillcolor")) {
        T.T.stencilColor = CheckColor(0);
        T.T.udmfExFlags |= mthing_t::MTHF_UseStencilColor;
        continue;
      }

      if (Key.strEquCI("alpha")) {
        T.T.renderAlpha = clampval(CheckFloat(), 0.0f, 1.0f);
        T.T.udmfExFlags |= mthing_t::MTHF_UseRenderAlpha;
        continue;
      }

      // classes (up to, and including 16)
      if (Key.startsWithCI("class")) {
        int cls = -1;
        if (VStr::convertInt((*Key)+5, &cls)) {
          if (cls >= 1 && cls <= 16) {
            Flag(T.T.SkillClassFilter, 0x00010000<<(cls-1));
            continue;
          }
        }
      }

      if (Key.startsWithCI("countsecret")) {
        if (CheckBool()) T.T.udmfExFlags |= mthing_t::MTHF_CountAsSecret; else T.T.udmfExFlags &= ~mthing_t::MTHF_CountAsSecret;
        continue;
      }

      if (Key.startsWithCI("conversation")) { T.T.conversationId = CheckInt(); T.T.udmfExFlags |= mthing_t::MTHF_UseConversationId; continue; }

      // ignored properties
      if (Key.startsWithCI("score")) { if (CheckInt() == 0) continue; }
    }

    keyWarning(WT_THING);
  }

  if (hasXY != 3) sc.HostError(va("UDMF: thing #%d has no coordinates set", ParsedThings.length()-1));
  if (!hasType) sc.HostError(va("UDMF: thing #%d has no type set", ParsedThings.length()-1));

  //FIXME: actually, this is valid only for special runacs range for now; write a proper thingy instead
  if (hasArg0Str && ((T.T.special >= 80 && T.T.special <= 86) || T.T.special == 226)) {
    VName sn = VName(*arg0str, VName::AddLower); // 'cause script names are lowercased
    if (sn != NAME_None) {
      T.T.args[0] = -(int)sn.GetIndex();
      //GCon->Logf("*** ACS NAMED THING SPECIAL %d: name is (%d) '%s'", T.T.special, sn.GetIndex(), *sn);
    } else {
      T.T.args[0] = 0;
    }
  }
}


//==========================================================================
//
//  VLevel::LoadTextMap
//
//==========================================================================
void VLevel::LoadTextMap (int Lump, const VMapInfo &MInfo) {
  VUdmfParser Parser(Lump);
  Parser.Parse(this, MInfo);

  const bool doTranslation = Parser.bDoTranslation;

  UDMFNamespace = VName(*Parser.NamespaceStr, VName::AddLower);
  if (Parser.bExtended) LevelFlags |= LF_Extended;

  if (Parser.ParsedVertexes.length() == 0) Host_Error("UDMF: map has no vertices!");

  NumVertexes = Parser.ParsedVertexes.length();
  Vertexes = new TVec[NumVertexes];
  memset((void *)Vertexes, 0, sizeof(Vertexes[0]));

  // check if any line refers to incomplete vertex
  for (auto &&pl : Parser.ParsedLines) {
    if (pl.V1Index < 0 || pl.V1Index >= NumVertexes) Host_Error("%s: UDMF: bad linedef #%d vertex index %d", *pl.loc.toStringNoCol(), pl.index, pl.V1Index);
    if (pl.V2Index < 0 || pl.V2Index >= NumVertexes) Host_Error("%s: UDMF: bad linedef #%d vertex index %d", *pl.loc.toStringNoCol(), pl.index, pl.V2Index);
    Parser.ParsedVertexes[pl.V1Index].checkValidity(pl.index);
    Parser.ParsedVertexes[pl.V2Index].checkValidity(pl.index);
  }

  // copy vertexes
  //memcpy(Vertexes, Parser.ParsedVertexes.Ptr(), sizeof(TVec)*NumVertexes);
  bool hasVertexHeights = false;
  for (auto &&it : Parser.ParsedVertexes.itemsIdx()) {
    const VUdmfParser::VParsedVertex &pv = it.value();
    if (pv.checked) {
      // this vertex is used
      Vertexes[it.index()] = TVec(pv.x, pv.y);
      hasVertexHeights = (hasVertexHeights || pv.hasFloorZ || pv.hasCeilingZ);
    } else {
      // this vertex is unused, assign invalid coords to it
      Vertexes[it.index()] = TVec(-99999, -99999);
    }
  }

  // check for duplicate vertices
  TMapNC<Vertex2DInfo, int> vmap; // value: in parsed array
  TMapNC<int, int> vremap; // key: original vertex index; value: new vertex index

  for (int f = 0; f < NumVertexes; ++f) {
    Vertex2DInfo vi = Vertex2DInfo(Vertexes[f].x, Vertexes[f].y, f);
    auto ip = vmap.get(vi);
    if (ip) {
      vremap.put(f, *ip);
      GCon->Logf(NAME_Warning, "%s: UDMF: vertex %d is duplicate of vertex %d (defined at %s)", *Parser.ParsedVertexes[f].loc.toStringNoCol(), f, *ip, *Parser.ParsedVertexes[*ip].loc.toStringNoCol());
    } else {
      vremap.put(f, f);
      vmap.put(vi, f);
    }
  }

  // copy sectors
  NumSectors = Parser.ParsedSectors.length();
  Sectors = new sector_t[NumSectors];
  for (int i = 0; i < NumSectors; ++i) {
    Sectors[i] = Parser.ParsedSectors[i].S;
    Sectors[i].CreateBaseRegion();
    for (auto &&vv : Parser.ParsedSectors[i].userFields) {
      if (Sectors[i].sectorTag) putSectorKey(Sectors[i].sectorTag, *vv.name, vv.i, vv.f);
      for (auto &&mt : Sectors[i].moreTags) if (mt) putSectorKey(mt, *vv.name, vv.i, vv.f);
    }
  }
  HashSectors();

  // copy linedefs
  NumLines = Parser.ParsedLines.length();
  Lines = new line_t[NumLines];
  for (int i = 0; i < NumLines; ++i) {
    Lines[i] = Parser.ParsedLines[i].L;
    // vertex index validity already checked

    auto ip0 = vremap.get(Parser.ParsedLines[i].V1Index);
    if (!ip0 || *ip0 < 0 || *ip0 >= NumVertexes) Sys_Error("UDMF: internal error (v0)");
    Lines[i].v1 = &Vertexes[*ip0];

    auto ip1 = vremap.get(Parser.ParsedLines[i].V2Index);
    if (!ip1 || *ip1 < 0 || *ip1 >= NumVertexes) Sys_Error("UDMF: internal error (v1)");
    Lines[i].v2 = &Vertexes[*ip1];

    //Lines[i].v1 = &Vertexes[Parser.ParsedLines[i].V1Index];
    //Lines[i].v2 = &Vertexes[Parser.ParsedLines[i].V2Index];

    //if (i == 1018) GCon->Logf("LD1018: v0=%d; v1=%d; v0=(%f,%f); v1=(%f,%f)", *ip0, *ip1, Vertexes[*ip0].x, Vertexes[*ip0].y, Vertexes[*ip1].x, Vertexes[*ip1].y);

    for (auto &&vv : Parser.ParsedLines[i].userFields) {
      putLineIdxKey(i, *vv.name, vv.i, vv.f);
      if (Lines[i].lineTag) putLineKey(Lines[i].lineTag, *vv.name, vv.i, vv.f);
      for (auto &&mt : Lines[i].moreTags) if (mt) putLineKey(mt, *vv.name, vv.i, vv.f);
    }
  }

  if (doTranslation) {
    // translate level to Hexen format
    GGameInfo->eventTranslateLevel(this);
  }

  // copy sidedefs
  NumSides = Parser.ParsedSides.length();
  CreateSides();

  side_t *sd = Sides;
  for (int i = 0; i < NumSides; ++i, ++sd) {
    /*
    if (sd->BottomTexture < 0 || sd->BottomTexture >= Parser.ParsedSides.length()) {
      Host_Error("UDMF: bad sidedef index %d (broken UDMF)", (int)sd->BottomTexture);
    }
    */

    // this is dummy side, it was initialised in `CreateSides()`
    if (Sides[i].Flags == SDF_ABSLIGHT) continue;

    VUdmfParser::VParsedSide &Src = Parser.ParsedSides[sd->BottomTexture];
    int Spec = sd->MidTexture;
    int Tag = sd->TopTexture;
    *sd = Src.S;

    if (Src.SectorIndex < 0 || Src.SectorIndex >= NumSectors) {
      Host_Error("%s: UDFM: bad sector index %d in sidedef #%d", *Src.loc.toStringNoCol(), Src.SectorIndex, Src.index);
    }
    sd->Sector = &Sectors[Src.SectorIndex];

    if (Src.userFields.length()) {
      for (auto &&vv : Src.userFields) putSideIdxKey(i, *vv.name, vv.i, vv.f);

      if (sd->LineNum >= 0 && sd->LineNum < NumLines) {
        const line_t *l = &Lines[sd->LineNum];
        if (l->lineTag || l->moreTags.length()) {
          for (auto &&vv : Src.userFields) {
            for (int sdx = 0; sdx < 2; ++sdx) {
              if (l->sidenum[sdx] == i) {
                if (l->lineTag) putSideKey(sdx, l->lineTag, *vv.name, vv.i, vv.f);
                for (auto &&mt : l->moreTags) if (mt) putSideKey(sdx, mt, *vv.name, vv.i, vv.f);
              }
            }
          }
        }
      }
    }

    switch (Spec) {
      case LNSPEC_LineTranslucent:
        // in BOOM midtexture can be translucency table lump name
        sd->MidTexture = TexNumForName(*Src.MidTexture, TEXTYPE_Wall, true);
        sd->TopTexture = TexNumForName(*Src.TopTexture, TEXTYPE_Wall);
        sd->BottomTexture = TexNumForName(*Src.BotTexture, TEXTYPE_Wall);
        break;

      case LNSPEC_TransferHeights:
        sd->MidTexture = TexNumForName(*Src.MidTexture, TEXTYPE_Wall, true);
        sd->TopTexture = TexNumForName(*Src.TopTexture, TEXTYPE_Wall, true);
        sd->BottomTexture = TexNumForName(*Src.BotTexture, TEXTYPE_Wall, true);
        break;

      case LNSPEC_StaticInit:
        {
          bool HaveCol;
          bool HaveFade;
          vuint32 Col;
          vuint32 Fade;
          sd->MidTexture = TexNumForName(*Src.MidTexture, TEXTYPE_Wall);
          int TmpTop = TexNumOrColor(*Src.TopTexture, TEXTYPE_Wall, HaveCol, Col);
          sd->BottomTexture = TexNumOrColor(*Src.BotTexture, TEXTYPE_Wall, HaveFade, Fade);
          if (HaveCol || HaveFade) {
            for (int j = 0; j < NumSectors; ++j) {
              if (Sectors[j].IsTagEqual(Tag)) {
                if (HaveCol) Sectors[j].params.LightColor = Col;
                if (HaveFade) Sectors[j].params.Fade = Fade;
              }
            }
          }
          sd->TopTexture = TmpTop;
        }
        break;

      default:
        sd->MidTexture = TexNumForName(*Src.MidTexture, TEXTYPE_Wall);
        sd->TopTexture = TexNumForName(*Src.TopTexture, TEXTYPE_Wall);
        sd->BottomTexture = TexNumForName(*Src.BotTexture, TEXTYPE_Wall);
        break;
    }
    if (sd->TopTexture == 0 && !Src.TopTexture.isEmpty() && !Src.TopTexture.strEqu("-")) sd->Flags |= SDF_AAS_TOP;
    if (sd->BottomTexture == 0 && !Src.BotTexture.isEmpty() && !Src.BotTexture.strEqu("-")) sd->Flags |= SDF_AAS_BOT;
    if (sd->MidTexture == 0 && !Src.MidTexture.isEmpty() && !Src.MidTexture.strEqu("-")) sd->Flags |= SDF_AAS_MID;
  }

  // copy things
  NumThings = Parser.ParsedThings.length();
  Things = new mthing_t[NumThings+1];
  memset((void *)Things, 0, (NumThings+1)*sizeof(mthing_t));
  for (int f = 0; f < Parser.ParsedThings.length(); ++f) {
    Things[f] = Parser.ParsedThings[f].T;
    if (Things[f].tid) {
      for (auto &&vv : Parser.ParsedThings[f].userFields) putThingKey(Things[f].tid, *vv.name, vv.i, vv.f);
    }
  }

  // create slopes from vertex heights
  // this is allowed only for triangular sectors (otherwise the map will be distorted)
  if (hasVertexHeights && NumSectors > 0) {
    // create list of lines for each sector
    struct SecLines {
      int vidxFloor[3]; // has floorz
      int vidxFloorOther[3]; // has no ceilz
      int vidxCeil[3]; // has ceilz
      int vidxCeilOther[3]; // has no ceilz
      int lineCount;

      static inline void appendIndex (int vidx[3], int idx) noexcept {
        if (vidx[0] == idx || vidx[1] == idx || vidx[2] == idx) return;
             if (vidx[0] == -1) vidx[0] = idx;
        else if (vidx[1] == -1) vidx[1] = idx;
        else if (vidx[2] == -1) vidx[2] = idx;
      }

      inline void clear () noexcept {
        for (unsigned int c = 0; c < 3; ++c) {
          vidxFloor[c] = vidxFloorOther[c] = vidxCeil[c] = vidxCeilOther[c] = -1;
        }
        lineCount = 0;
      }

      inline void appendFloor (int idx) noexcept { appendIndex(vidxFloor, idx); }
      inline void appendFloorOther (int idx) noexcept { appendIndex(vidxFloorOther, idx); }
      inline void appendCeil (int idx) noexcept { appendIndex(vidxCeil, idx); }
      inline void appendCeilOther (int idx) noexcept { appendIndex(vidxCeilOther, idx); }

      static bool getVerts (int vres[3], const int vidx[3], const int vidxOther[3]) noexcept {
        if (vidx[0] == -1) return false;
        unsigned ridx = 0;
        for (unsigned f = 0; f < 3; ++f) if (vidx[f] != -1) vres[ridx++] = vidx[f];
        if (ridx < 3) {
          for (unsigned f = 0; f < 3 && ridx < 3; ++f) {
            const int idx = vidxOther[f];
            if (idx != -1) {
              bool ok = true;
              for (unsigned c = 0; c < ridx; ++c) if (vres[c] == idx) { ok = false; break; }
              if (ok) vres[ridx++] = idx;
            }
          }
        }
        return (ridx == 3);
      }

      bool inline getFloorVerts (int vres[3]) noexcept { return getVerts(vres, vidxFloor, vidxFloorOther); }
      bool inline getCeilVerts (int vres[3]) noexcept { return getVerts(vres, vidxCeil, vidxCeilOther); }
    };

    SecLines *seclines = new SecLines[NumSectors];
    for (int f = 0; f < NumSectors; ++f) seclines[f].clear();

    bool hasVertexSlopes = false;
    for (int i = 0; i < NumLines; ++i) {
      line_t *line = &Lines[i];
      VUdmfParser::VParsedLine &uline = Parser.ParsedLines[i];
      VUdmfParser::VParsedVertex &v1 = Parser.ParsedVertexes[uline.V1Index];
      VUdmfParser::VParsedVertex &v2 = Parser.ParsedVertexes[uline.V2Index];
      for (int sideidx = 0; sideidx < 2; ++sideidx) {
        if (line->sidenum[sideidx] < 0) continue;
        sector_t *sector = Sides[line->sidenum[sideidx]].Sector;
        if (!sector) continue; // just in case
        int snum = (int)(ptrdiff_t)(sector-&Sectors[0]);
        vassert(snum >= 0 && snum < NumSectors);
        SecLines *sl = &seclines[snum];
        ++sl->lineCount;
        if (sl->lineCount > 3) continue;
        //if (sl->invalid) continue;
        if (v1.hasFloorZ) sl->appendFloor(uline.V1Index); else sl->appendFloorOther(uline.V1Index);
        if (v2.hasFloorZ) sl->appendFloor(uline.V2Index); else sl->appendFloorOther(uline.V2Index);
        if (v1.hasCeilingZ) sl->appendCeil(uline.V1Index); else sl->appendCeilOther(uline.V1Index);
        if (v2.hasCeilingZ) sl->appendCeil(uline.V2Index); else sl->appendCeilOther(uline.V2Index);
        if (sl->vidxFloor[0] != -1 || sl->vidxCeil[0] != -1) hasVertexSlopes = true;
      }
    }

    int slopedFloors = 0, slopedCeilings = 0;
    int slopedFloorsBad = 0, slopedCeilingsBad = 0;

    // if we don't have good sectors, there is no reason to process anything
    if (hasVertexSlopes) {
      for (int secindex = 0; secindex < NumSectors; ++secindex) {
        SecLines *sl = &seclines[secindex];
        if (sl->lineCount != 3) continue; // skip invalid sectors

        sector_t *sector = &Sectors[secindex];
        int verts[3];

        if (sl->getFloorVerts(verts)) {
          TVec tvs[3];
          for (unsigned int f = 0; f < 3; ++f) {
            VUdmfParser::VParsedVertex *pv = &Parser.ParsedVertexes[verts[f]];
            tvs[f].x = pv->x;
            tvs[f].y = pv->y;
            tvs[f].z = (pv->hasFloorZ ? pv->floorz : sector->floor.dist);
          }
          TPlane pl;
          pl.SetFromTriangle(tvs[0], tvs[1], tvs[2]);
          // sanity check
          if (isFiniteF(pl.normal.z) && fabsf(pl.normal.z) > 0.0001f) {
            // flip, if necessary
            if (pl.normal.z < 0.0f) pl.FlipInPlace();
            *((TPlane *)&sector->floor) = pl;
            ++slopedFloors;
          } else {
            ++slopedFloorsBad;
          }
        }

        if (sl->getCeilVerts(verts)) {
          TVec tvs[3];
          for (unsigned int f = 0; f < 3; ++f) {
            VUdmfParser::VParsedVertex *pv = &Parser.ParsedVertexes[verts[f]];
            tvs[f].x = pv->x;
            tvs[f].y = pv->y;
            tvs[f].z = (pv->hasCeilingZ ? pv->ceilingz : -sector->ceiling.dist);
          }
          TPlane pl;
          pl.SetFromTriangle(tvs[0], tvs[1], tvs[2]);
          // sanity check
          if (isFiniteF(pl.normal.z) && fabsf(pl.normal.z) > 0.0001f) {
            // flip, if necessary
            if (pl.normal.z > 0.0f) pl.FlipInPlace();
            *((TPlane *)&sector->ceiling) = pl;
            ++slopedCeilings;
          } else {
            ++slopedCeilingsBad;
          }
        }
      }
      if (slopedFloors || slopedCeilings) GCon->Logf("UDMF: found %d height-based floor slopes, and %d ceiling slopes", slopedFloors, slopedCeilings);
      if (slopedFloorsBad || slopedCeilingsBad) GCon->Logf("UDMF: rejected %d height-based bad floor slopes, and %d bad ceiling slopes", slopedFloorsBad, slopedCeilingsBad);
    }

    delete[] seclines;
  }
}
