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
//**
//**  Dehacked patch parsing
//**
//**************************************************************************
#include "../gamedefs.h"
#include "../psim/p_entity.h"
#include "../language.h"
#include "../mapinfo.h"
#include "../filesys/files.h"
#include "../sound/sound.h"
#include "vc_dehacked.h"


static VCvarB dbg_dehacked_codepointers("dbg_dehacked_codepointers", false, "Show DEHACKED replaced code pointers?", CVAR_PreInit|CVAR_NoShadow);
static VCvarB dbg_dehacked_thing_replaces("dbg_dehacked_thing_replaces", false, "Show DEHACKED thing replaces?", CVAR_PreInit|CVAR_NoShadow);
static VCvarB dbg_dehacked_frame_replaces("dbg_dehacked_frame_replaces", false, "Show DEHACKED frame sprite replaces?", CVAR_PreInit|CVAR_NoShadow);
static VCvarB dbg_dehacked_dump_all("dbg_dehacked_dump_all", false, "Show all DEHACKED debug dumps?", CVAR_PreInit|CVAR_NoShadow);


static TArray<VStr> cli_DehList;
static int cli_NoAnyDehacked = 0;
static int cli_DehackedDebug = 0;
static int cli_DehackedIgnoreWeapons = 0;

/*static*/ bool cliRegister_dehacked_args =
  VParsedArgs::RegisterFlagSet("-deh-debug", "!show dehacked loader debug messages", &cli_DehackedDebug) &&
  VParsedArgs::RegisterFlagSet("-deh-no-weapons", "ignore weapons in dehacked (not BEX)", &cli_DehackedIgnoreWeapons) &&
  VParsedArgs::RegisterFlagSet("-disable-dehacked", "disable all and any dehacked patches", &cli_NoAnyDehacked) &&
  GParsedArgs.RegisterCallback("-deh", "load next files as dehacked patch", [] (VArgs &args, int idx) -> int {
    for (++idx; !VParsedArgs::IsArgBreaker(args, idx); ++idx) {
      VStr mn = args[idx];
      if (!mn.isEmpty() && Sys_FileExists(mn)) {
        bool found = false;
        for (auto &&s : cli_DehList) {
          #ifdef _WIN32
          if (s.strEquCI(mn)) { found = true; break; }
          #else
          if (s.strEqu(mn)) { found = true; break; }
          #endif
        }
        if (!found) cli_DehList.append(mn);
      }
    }
    return idx;
  });


struct VCodePtrInfo {
  VStr Name;
  VMethod *Method;
};


struct VDehFlag {
  int Which;
  const char *Name;
  unsigned int Mask;
};


VCvarI Infighting("infighting", 0, "Allow infighting for all monster types?", 0/*CVAR_ServerInfo*/);


static TMapNC<VName, bool> ReplacedSprites; // lowercased
static TMapNC<VName, VName> SpriteReplacements; // lowercased, origname, newname


static char *Patch;
static char *PatchPtr;
static char *String;
static char *ValueString;
static int value;


struct DehState {
  VState *State;
  VMethod *Action;
  VClass *Class;
  // as i modified barrel states a little, we need this
  bool BarrelFixStop;
  bool BarrelRemoveAction;

  inline DehState () noexcept
    : State(nullptr)
    , Action(nullptr)
    , Class(nullptr)
    , BarrelFixStop(false)
    , BarrelRemoveAction(false)
  {}

  void FixMe () noexcept {
    if (BarrelFixStop) { State->NextState = nullptr; BarrelFixStop = false; }
    if (BarrelRemoveAction) { State->Function = nullptr; BarrelRemoveAction = false; }
  }
};


static TArray<VName> Sprites;
static TArray<VClass *> EntClasses;
static TArray<bool> EntClassTouched; // by some dehacked definition
static TArray<VClass *> WeaponClasses;
static TArray<VClass *> AmmoClasses;
static TArray<DehState> DehStates;
static TArray<int> CodePtrStatesIdx; // index in `DehStates`
static TArray<VCodePtrInfo> CodePtrs;
static TArray<VName> Sounds;

static VClass *GameInfoClass;
static VClass *DoomPlayerClass;
static VClass *BfgClass;
static VClass *HealthBonusClass;
static VClass *SoulsphereClass;
static VClass *MegaHealthClass;
static VClass *GreenArmorClass;
static VClass *BlueArmorClass;
static VClass *ArmorBonusClass;

static TArray<FReplacedString> SfxNames;
static TArray<FReplacedString> MusicNames;
static TArray<FReplacedString> SpriteNames;
static VLanguage *EngStrings;

static const VDehFlag DehFlags[] = {
  { 0, "SPECIAL", 0x00000001 },
  { 0, "SOLID", 0x00000002 },
  { 0, "SHOOTABLE", 0x00000004 },
  { 0, "NOSECTOR", 0x00000008 },
  { 0, "NOBLOCKMAP", 0x00000010 },
  { 0, "AMBUSH", 0x00000020 },
  { 0, "JUSTHIT", 0x00000040 },
  { 0, "JUSTATTACKED", 0x00000080 },
  { 0, "SPAWNCEILING", 0x00000100 },
  { 0, "NOGRAVITY", 0x00000200 },
  { 0, "DROPOFF", 0x00000400 },
  { 0, "PICKUP", 0x00000800 },
  { 0, "NOCLIP", 0x00001000 },
  { 0, "FLOAT", 0x00004000 },
  { 0, "TELEPORT", 0x00008000 },
  { 0, "MISSILE", 0x00010000 },
  { 0, "DROPPED", 0x00020000 },
  { 0, "SHADOW", 0x00040000 },
  { 0, "NOBLOOD", 0x00080000 },
  { 0, "CORPSE", 0x00100000 },
  { 0, "INFLOAT", 0x00200000 },
  { 0, "COUNTKILL", 0x00400000 },
  { 0, "COUNTITEM", 0x00800000 },
  { 0, "SKULLFLY", 0x01000000 },
  { 0, "NOTDMATCH", 0x02000000 },
  { 0, "TRANSLATION1", 0x04000000 },
  { 0, "TRANSLATION", 0x04000000 },
  { 0, "TRANSLATION2", 0x08000000 },
  { 0, "UNUSED1", 0x08000000 },
  { 0, "TRANSLUC25", 0x10000000 },
  { 0, "TRANSLUC75", 0x20000000 },
  { 0, "STEALTH", 0x40000000 },
  { 0, "UNUSED4", 0x40000000 },
  { 0, "TRANSLUCENT", 0x80000000 },
  { 0, "TRANSLUC50", 0x80000000 },
  { 1, "LOGRAV", 0x00000001 },
  { 1, "WINDTHRUST", 0x00000002 },
  { 1, "FLOORBOUNCE", 0x00000004 },
  { 1, "BLASTED", 0x00000008 },
  { 1, "FLY", 0x00000010 },
  { 1, "FLOORCLIP", 0x00000020 },
  { 1, "SPAWNFLOAT", 0x00000040 },
  { 1, "NOTELEPORT", 0x00000080 },
  { 1, "RIP", 0x00000100 },
  { 1, "PUSHABLE", 0x00000200 },
  { 1, "CANSLIDE", 0x00000400 },
  { 1, "ONMOBJ", 0x00000800 },
  { 1, "PASSMOBJ", 0x00001000 },
  { 1, "CANNOTPUSH", 0x00002000 },
  { 1, "THRUGHOST", 0x00004000 },
  { 1, "BOSS", 0x00008000 },
  { 1, "FIREDAMAGE", 0x00010000 },
  { 1, "NODMGTHRUST", 0x00020000 },
  { 1, "TELESTOMP", 0x00040000 },
  { 1, "DONTDRAW", 0x00080000 },
  { 1, "FLOATBOB", 0x00100000 },
  { 1, "IMPACT", 0x00200000 },
  { 1, "PUSHWALL", 0x00400000 },
  { 1, "MCROSS", 0x00800000 },
  { 1, "PCROSS", 0x01000000 },
  { 1, "CANTLEAVEFLOORPIC", 0x02000000 },
  { 1, "NONSHOOTABLE", 0x04000000 },
  { 1, "INVULNERABLE", 0x08000000 },
  { 1, "DORMANT", 0x10000000 },
  { 1, "ICEDAMAGE", 0x20000000 },
  { 1, "SEEKERMISSILE", 0x40000000 },
  { 1, "REFLECTIVE", 0x80000000 },
  // ignored flags
  { 0, "SLIDE", 0 },
  { 0, "UNUSED2", 0 },
  { 0, "UNUSED3", 0 },
};


static int dehCurrLine = 0;
static bool strEOL = false;
static bool bexMode = false;
static VStr dehFileName;
static bool dehWarningsEnabled = true;


struct StInfo {
  VClass *stclass;
  VStr stname;
  VState *stfirst;
  int stcps;
  int count;
};

static TArray<StInfo> dehStInfo; // debug


//==========================================================================
//
//  dehStInfoFind
//
//==========================================================================
static StInfo *dehStInfoFind (int idx) noexcept {
  for (auto &&ss : dehStInfo) if (idx >= ss.stcps && idx < ss.stcps+ss.count) return &ss;
  Sys_Error("invalid deh stinfo index %d", idx);
  return nullptr;
}


//==========================================================================
//
//  isWeaponState
//
//  FIXME: remove hardcode here!
//
//==========================================================================
static inline bool isWeaponState (int idx) noexcept { return (idx >= 0 && idx < 157); }


//==========================================================================
//
//  isWeaponSprite
//
//  FIXME: remove hardcode here!
//
//==========================================================================
static bool isWeaponSprite (const char *oldStr) noexcept {
  if (!oldStr || !oldStr[0]) return false;
  if (!oldStr[1] || !oldStr[2] || !oldStr[3] || oldStr[4]) return false;
  if (VStr::strEquCI(oldStr, "CLIP")) return true;
  if (VStr::strEquCI(oldStr, "AMMO")) return true;
  if (VStr::strEquCI(oldStr, "ROCK")) return true;
  if (VStr::strEquCI(oldStr, "BROK")) return true;
  if (VStr::strEquCI(oldStr, "CELL")) return true;
  if (VStr::strEquCI(oldStr, "CELP")) return true;
  if (VStr::strEquCI(oldStr, "SHEL")) return true;
  if (VStr::strEquCI(oldStr, "SBOX")) return true;
  char buf[4];
  buf[0] = oldStr[0];
  buf[1] = oldStr[1];
  buf[2] = oldStr[2];
  buf[3] = 0;
  if (VStr::strEquCI(buf, "SHT")) return true;
  if (VStr::strEquCI(buf, "PUN")) return true;
  if (VStr::strEquCI(buf, "PIS")) return true;
  if (VStr::strEquCI(buf, "SAW")) return true;
  if (VStr::strEquCI(buf, "CHG")) return true;
  if (VStr::strEquCI(buf, "MIS")) return true;
  if (VStr::strEquCI(buf, "BFG")) return true;
  if (VStr::strEquCI(buf, "PLS")) return true;
  return false;
}


//==========================================================================
//
//  IsDehReplacedSprite
//
//==========================================================================
bool IsDehReplacedSprite (VName spname) {
  return ReplacedSprites.has(spname);
}


//==========================================================================
//
//  GetDehReplacedSprite
//
//==========================================================================
VName GetDehReplacedSprite (VName oldname) {
  auto rp = SpriteReplacements.get(oldname);
  return (rp ? *rp : NAME_None);
}


//==========================================================================
//
//  DehFatal
//
//==========================================================================
static __attribute__((format(printf, 1, 2))) void DehFatal (const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char *res = vavarg(fmt, ap);
  va_end(ap);
  if (!dehFileName.isEmpty()) {
    Sys_Error("DEHACKED FATAL ERROR: %s:%d: %s", *dehFileName, dehCurrLine, res);
  } else {
    Sys_Error("DEHACKED FATAL ERROR: %s", res);
  }
}


//==========================================================================
//
//  Warning
//
//==========================================================================
static __attribute__((format(printf, 1, 2))) void Warning (const char *fmt, ...) {
  if (vcWarningsSilenced) return;
  if (!dehWarningsEnabled) return;
  va_list ap;
  va_start(ap, fmt);
  char *res = vavarg(fmt, ap);
  va_end(ap);
  if (!dehFileName.isEmpty()) {
    GLog.Logf(NAME_Warning, "%s:%d: %s", *dehFileName, dehCurrLine, res);
  } else {
    GLog.Logf(NAME_Warning, "DEHACKED: %s", res);
  }
}


//==========================================================================
//
//  Message
//
//==========================================================================
static __attribute__((format(printf, 1, 2))) void Message (const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char *res = vavarg(fmt, ap);
  va_end(ap);
  if (!dehFileName.isEmpty()) {
    GLog.Logf(NAME_Init, "%s:%d: %s", *dehFileName, dehCurrLine, res);
  } else {
    GLog.Logf(NAME_Init, "DEHACKED: %s", res);
  }
}


//==========================================================================
//
//  DHDebugLog
//
//==========================================================================
static __attribute__((format(printf, 1, 2))) void DHDebugLog (const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char *res = vavarg(fmt, ap);
  va_end(ap);
  if (!dehFileName.isEmpty()) {
    GLog.Logf(NAME_Debug, "%s:%d: %s", *dehFileName, dehCurrLine, res);
  } else {
    GLog.Logf(NAME_Debug, "DEHACKED: %s", res);
  }
}


//==========================================================================
//
//  GetLine
//
//  this trims leading and trailing spaces
//  result is in `String` (modifyable)
//
//==========================================================================
static bool GetLine () {
  do {
    ++dehCurrLine;

    if (!*PatchPtr) {
      String = nullptr;
      strEOL = true;
      return false;
    }

    String = PatchPtr;

    while (*PatchPtr && *PatchPtr != '\n') {
      if (*(vuint8 *)PatchPtr < ' ') *PatchPtr = ' ';
      ++PatchPtr;
    }

    if (*PatchPtr == '\n') {
      *PatchPtr = 0;
      ++PatchPtr;
    }

    while (*String && *(vuint8 *)String <= ' ') ++String;

    // comments
    if (*String == '#') { *String = 0; continue; }
    if (bexMode && String[0] == '/' && String[1] == '/') { *String = 0; continue; }

    char *End = String+VStr::Length(String);
    while (End > String && (vuint8)End[-1] <= ' ') {
      End[-1] = 0;
      --End;
    }

    // strip comments
    /*
    if (bexMode) {
      char *s = String;
      //char qch = 0;
      while (*s) {
        if (qch) {
          if (s[0] == '\\') {
            ++s;
            if (*s) ++s;
            continue;
          }
          if (s[0] == qch) qch = 0;
          ++s;
          continue;
        }
        // not in string
        if ((vuint8)s[0] <= ' ') { ++s; continue; }
        if (s[0] == '\'' || s[0] == '"') { qch = *s++; continue; }
        if (s[0] == '/' && s[1] == '/') { *s = 0; break; }
        ++s;
      }
    }
    */
  } while (!*String);

  strEOL = false;
  return true;
}


//==========================================================================
//
//  GetToken
//
//==========================================================================
/*
static VStr GetToken () {
  if (strEOL) return VStr();
  while (*String && *(vuint8 *)String <= ' ') ++String;
  if (!String[0]) { strEOL = true; return VStr(); }
  VStr res;
  while (*String && *(vuint8 *)String <= ' ') res += *String++;
  return res;
}
*/


//==========================================================================
//
//  ParseParam
//
//  on success return:
//    `String` contains a key
//    `ValueString` contains string value of a key
//    `value` contians numeric value of a key
//
//==========================================================================
static bool ParseParam () {
  if (!GetLine()) return false;

  char *val = strchr(String, '=');
  if (!val) return false;

  ValueString = val+1;
  while (*ValueString && *(vuint8 *)ValueString <= ' ') ++ValueString;
  value = (*ValueString ? VStr::atoi(ValueString) : 0);

  // strip trailing spaces from key name
  do {
    *val = 0;
    --val;
  } while (val >= String && *(vuint8 *)val <= ' ');

  // remove double spaces from key name
  char *dptr = String;
  char *sptr = String;
  while (*sptr) {
    if (dptr != sptr) *dptr = *sptr;
    ++dptr;
    ++sptr;
    // skip extra spaces
    while (*sptr && (vuint8)sptr[-1] <= ' ' && (vuint8)sptr[0] <= ' ') ++sptr;
  }
  *dptr = 0;

  return true;
}


//==========================================================================
//
//  ParseFlag
//
//==========================================================================
static void ParseFlag (VStr FlagName, int *Values, bool *Changed) {
  if (FlagName.Length() == 0) return;
  if (FlagName[0] == '-' || (FlagName[0] >= '0' && FlagName[0] <= '9')) {
    // clear flags that were not used by Doom engine as well as SLIDE flag which dosen't exist anymore
    Values[0] = VStr::atoi(*FlagName)&0x0fffdfff;
    Changed[0] = true;
    //GCon->Logf(NAME_Debug, "DEHACKED: ParseFlag: '%s' -> %d 0x%08x", *FlagName, Values[0], (unsigned)Values[0]);
    return;
  }
  for (size_t i = 0; i < ARRAY_COUNT(DehFlags); ++i) {
    if (FlagName.strEquCI(DehFlags[i].Name)) {
      Values[DehFlags[i].Which] |= DehFlags[i].Mask;
      Changed[DehFlags[i].Which] = true;
      return;
    }
  }
  Warning("Unknown flag '%s'", *FlagName);
}


//==========================================================================
//
//  ParseRenderStyle
//
//==========================================================================
static int ParseRenderStyle () {
  int RenderStyle = STYLE_Normal;
       if (VStr::strEquCI(ValueString, "STYLE_None")) RenderStyle = STYLE_None;
  else if (VStr::strEquCI(ValueString, "STYLE_Normal")) RenderStyle = STYLE_Normal;
  else if (VStr::strEquCI(ValueString, "STYLE_Fuzzy")) RenderStyle = STYLE_Fuzzy;
  else if (VStr::strEquCI(ValueString, "STYLE_SoulTrans")) RenderStyle = STYLE_SoulTrans;
  else if (VStr::strEquCI(ValueString, "STYLE_OptFuzzy")) RenderStyle = STYLE_OptFuzzy;
  else if (VStr::strEquCI(ValueString, "STYLE_Translucent")) RenderStyle = STYLE_Translucent;
  else if (VStr::strEquCI(ValueString, "STYLE_Add")) RenderStyle = STYLE_Add;
  else Warning("Bad render style '%s'", ValueString);
  return RenderStyle;
}


//==========================================================================
//
//  DoThingState
//
//==========================================================================
static void DoThingState (VClass *Ent, const char *StateLabel) {
  if (value < 0 || value >= DehStates.length()) {
    Warning("Bad state '%d' for thing '%s'", value, (Ent ? Ent->GetName() : "<undefined>"));
  } else {
    DehStates[value].FixMe();
    Ent->SetStateLabel(StateLabel, DehStates[value].State);
  }
}


//==========================================================================
//
//  DoThingSound
//
//==========================================================================
static void DoThingSound (VClass *Ent, const char *FieldName) {
  // if it's not a number, treat it like a sound defined in SNDINFO
  if (ValueString[0] < '0' || ValueString[0] > '9') {
    Ent->SetFieldNameValue(FieldName, ValueString);
    return;
  }

  // reject dehextra
  if (value >= 500 && value <= 699) DehFatal("DEHEXTRA is not supported and will never be. Sorry. (sound #%d) (%d)", value, Sounds.length());

  if (value < 0 || value >= Sounds.length()) {
    Warning("Bad sound index %d for '%s'", value, (Ent ? Ent->GetName() : "<undefined>"));
    return;
  }

  Ent->SetFieldNameValue(FieldName, Sounds[value]);
}


//==========================================================================
//
//  ReadThing
//
//==========================================================================
static void ReadThing (int num) {
  // reject dehextra
  if (num >= 150 && num <= 249) DehFatal("DEHEXTRA is not supported and will never be. Sorry. (thing #%d) (%d)", num, EntClasses.length());

  if (num < 1 || num > EntClasses.length()) {
    Warning("Invalid thing num %d", num);
    while (ParseParam()) {}
    return;
  }
  bool gotHeight = false;
  bool gotSpawnCeiling = false;
  bool hasSomeDefine = false;

  VClass *Ent = EntClasses[num-1];
  while (ParseParam()) {
    // id #
    if (VStr::strEquCI(String, "ID #")) {
      if (value) {
        // for info output
        vint32 oldid = -1;
        mobjinfo_t *nfo = VClass::FindMObjIdByClass(Ent, GGameInfo->GameFilterFlag);
        if (nfo) oldid = nfo->DoomEdNum;
        // actual work
        VClass::ReplaceMObjIdByClass(Ent, value, GGameInfo->GameFilterFlag);
        // info output
        nfo = VClass::FindMObjIdByClass(Ent, GGameInfo->GameFilterFlag);
        if (nfo && nfo->DoomEdNum == value) {
          if (dbg_dehacked_dump_all || dbg_dehacked_thing_replaces) DHDebugLog("replaced `%s` DoomEdNum #%d with #%d", Ent->GetName(), oldid, value);
        }
      } else {
        VClass::RemoveMObjIdByClass(Ent, GGameInfo->GameFilterFlag);
      }
      continue;
    }
    // hit points
    if (VStr::strEquCI(String, "Hit points")) {
      hasSomeDefine = true;
      Ent->SetFieldInt("Health", value);
      Ent->SetFieldInt("GibsHealth", -value);
      continue;
    }
    // reaction time
    if (VStr::strEquCI(String, "Reaction time")) {
      hasSomeDefine = true;
      Ent->SetFieldInt("ReactionCount", value);
      continue;
    }
    // missile damage
    if (VStr::strEquCI(String, "Missile damage")) {
      hasSomeDefine = true;
      Ent->SetFieldInt("MissileDamage", value);
      continue;
    }
    // width
    if (VStr::strEquCI(String, "Width")) {
      hasSomeDefine = true;
      Ent->SetFieldFloat("Radius", value/65536.0f);
      // also, reset render radius
      Ent->SetFieldFloat("RenderRadius", 0);
      continue;
    }
    // height
    if (VStr::strEquCI(String, "Height")) {
      gotHeight = true; // height changed
      hasSomeDefine = true;
      Ent->SetFieldFloat("Height", value/65536.0f);
      continue;
    }
    // mass
    if (VStr::strEquCI(String, "Mass")) {
      hasSomeDefine = true;
      Ent->SetFieldFloat("Mass", (value == 0x7fffffff ? 99999.0f : value));
      continue;
    }
    // speed
    if (VStr::strEquCI(String, "Speed")) {
      hasSomeDefine = true;
      if (value < 100) {
        Ent->SetFieldFloat("Speed", 35.0f*value);
      } else {
        Ent->SetFieldFloat("Speed", 35.0f*value/65536.0f);
      }
      continue;
    }
    // pain chance
    if (VStr::strEquCI(String, "Pain chance")) {
      hasSomeDefine = true;
      Ent->SetFieldFloat("PainChance", value/256.0f);
      continue;
    }
    // translucency
    if (VStr::strEquCI(String, "Translucency")) {
      //hasSomeDefine = true;
      Ent->SetFieldFloat("Alpha", value/65536.0f);
      Ent->SetFieldByte("RenderStyle", STYLE_Translucent);
      continue;
    }
    // alpha
    if (VStr::strEquCI(String, "Alpha")) {
      //hasSomeDefine = true;
      Ent->SetFieldFloat("Alpha", clampval(VStr::atof(ValueString, 1), 0.0f, 1.0f));
      continue;
    }
    // render style
    if (VStr::strEquCI(String, "Render Style")) {
      //hasSomeDefine = true;
      Ent->SetFieldByte("RenderStyle", ParseRenderStyle());
      continue;
    }
    // scale
    if (VStr::strEquCI(String, "Scale")) {
      hasSomeDefine = true;
      float Scale = VStr::atof(ValueString, 1);
      Scale = midval(0.0001f, Scale, 256.0f);
      Ent->SetFieldFloat("ScaleX", Scale);
      Ent->SetFieldFloat("ScaleY", Scale);
      continue;
    }
    // bitset
    if (VStr::strEquCI(String, "Bits")) {
      hasSomeDefine = true;
      TArray<VStr> Flags;
      VStr Tmp(ValueString);
      Tmp.Split(" ,+|\t\f\r", Flags);
      int Values[2] = { 0, 0 };
      bool Changed[2] = { false, false };
      for (int i = 0; i < Flags.length(); ++i) ParseFlag(Flags[i].ToUpper(), Values, Changed);
      if (Changed[0]) {
        gotSpawnCeiling = ((Values[0]&0x00000100) != 0);
        Ent->SetFieldBool("bSpecial", Values[0]&0x00000001);
        Ent->SetFieldBool("bSolid", Values[0]&0x00000002);
        Ent->SetFieldBool("bShootable", Values[0]&0x00000004);
        Ent->SetFieldBool("bNoSector", Values[0]&0x00000008);
        Ent->SetFieldBool("bNoBlockmap", Values[0]&0x00000010);
        Ent->SetFieldBool("bAmbush", Values[0]&0x00000020);
        Ent->SetFieldBool("bJustHit", Values[0]&0x00000040);
        Ent->SetFieldBool("bJustAttacked", Values[0]&0x00000080);
        Ent->SetFieldBool("bSpawnCeiling", Values[0]&0x00000100);
        Ent->SetFieldBool("bNoGravity", Values[0]&0x00000200);
        Ent->SetFieldBool("bDropOff", Values[0]&0x00000400);
        Ent->SetFieldBool("bPickUp", Values[0]&0x00000800);
        Ent->SetFieldBool("bColideWithThings", !(Values[0]&0x00001000));
        Ent->SetFieldBool("bColideWithWorld", !(Values[0]&0x00001000));
        Ent->SetFieldBool("bFloat", Values[0]&0x00004000);
        Ent->SetFieldBool("bTeleport", Values[0]&0x00008000);
        Ent->SetFieldBool("bMissile", Values[0]&0x00010000);
        Ent->SetFieldBool("bDropped", Values[0]&0x00020000);
        Ent->SetFieldBool("bShadow", Values[0]&0x00040000);
        Ent->SetFieldBool("bNoBlood", Values[0]&0x00080000);
        Ent->SetFieldBool("bCorpse", Values[0]&0x00100000);
        Ent->SetFieldBool("bInFloat", Values[0]&0x00200000);
        Ent->SetFieldBool("bCountKill", Values[0]&0x00400000);
        Ent->SetFieldBool("bCountItem", Values[0]&0x00800000);
        Ent->SetFieldBool("bSkullFly", Values[0]&0x01000000);
        Ent->SetFieldBool("bNoDeathmatch", Values[0]&0x02000000);
        Ent->SetFieldBool("bStealth", Values[0]&0x40000000);

        // set additional flags for missiles
        Ent->SetFieldBool("bActivatePCross", Values[0]&0x00010000);
        Ent->SetFieldBool("bNoTeleport", Values[0]&0x00010000);

        // set additional flags for monsters
        Ent->SetFieldBool("bMonster", Values[0]&0x00400000);
        Ent->SetFieldBool("bActivateMCross", Values[0]&0x00400000);
        // set push wall for both monsters and the player
        Ent->SetFieldBool("bActivatePushWall", (Values[0]&0x00400000) || num == 1);
        // also set pass mobj flag
        Ent->SetFieldBool("bPassMobj", num == 1 || (Values[0]&0x00400000) || (Values[0]&0x00010000));

        // translation
        Ent->SetFieldInt("Translation", Values[0]&0x0c000000 ? (TRANSL_Standard<<TRANSL_TYPE_SHIFT)+((Values[0]&0x0c000000)>>26)-1 : 0);

        // alpha and render style
        Ent->SetFieldFloat("Alpha", (Values[0]&0x00040000) ? /*0.1f*/0.35f :
          (Values[0]&0x80000000) ? /*0.5f*/0.55f :
          (Values[0]&0x10000000) ? /*0.25f*/0.35f :
          (Values[0]&0x20000000) ? 0.75f : 1.0f);
        Ent->SetFieldByte("RenderStyle", (Values[0]&0x00040000) ?
          STYLE_OptFuzzy : (Values[0]&0xb0000000) ? STYLE_Translucent : STYLE_Normal);
      }
      if (Changed[1]) {
        Ent->SetFieldBool("bWindThrust", Values[1]&0x00000002);
        Ent->SetFieldBool("bBlasted", Values[1]&0x00000008);
        Ent->SetFieldBool("bFly", Values[1]&0x00000010);
        Ent->SetFieldBool("bFloorClip", Values[1]&0x00000020);
        Ent->SetFieldBool("bSpawnFloat", Values[1]&0x00000040);
        Ent->SetFieldBool("bNoTeleport", Values[1]&0x00000080);
        Ent->SetFieldBool("bRip", Values[1]&0x00000100);
        Ent->SetFieldBool("bPushable", Values[1]&0x00000200);
        Ent->SetFieldBool("bSlide", Values[1]&0x00000400);
        Ent->SetFieldBool("bOnMobj", Values[1]&0x00000800);
        Ent->SetFieldBool("bPassMobj", Values[1]&0x00001000);
        Ent->SetFieldBool("bCannotPush", Values[1]&0x00002000);
        Ent->SetFieldBool("bThruGhost", Values[1]&0x00004000);
        Ent->SetFieldBool("bBoss", Values[1]&0x00008000);
        Ent->SetFieldBool("bNoDamageThrust", Values[1]&0x00020000);
        Ent->SetFieldBool("bTelestomp", Values[1]&0x00040000);
        Ent->SetFieldBool("bInvisible", Values[1]&0x00080000);
        Ent->SetFieldBool("bFloatBob", Values[1]&0x00100000);
        Ent->SetFieldBool("bActivateImpact", Values[1]&0x00200000);
        Ent->SetFieldBool("bActivatePushWall", Values[1]&0x00400000);
        Ent->SetFieldBool("bActivateMCross", Values[1]&0x00800000);
        Ent->SetFieldBool("bActivatePCross", Values[1]&0x01000000);
        Ent->SetFieldBool("bCantLeaveFloorpic", Values[1]&0x02000000);
        Ent->SetFieldBool("bNonShootable", Values[1]&0x04000000);
        Ent->SetFieldBool("bInvulnerable", Values[1]&0x08000000);
        Ent->SetFieldBool("bDormant", Values[1]&0x10000000);
        Ent->SetFieldBool("bSeekerMissile", Values[1]&0x40000000);
        Ent->SetFieldBool("bReflective", Values[1]&0x80000000);
        //  Things that used to be flags before.
        if (Values[1]&0x00000001) Ent->SetFieldFloat("Gravity", 0.125f);
             if (Values[1]&0x00010000) Ent->SetFieldNameValue("DamageType", "Fire");
        else if (Values[1]&0x20000000) Ent->SetFieldNameValue("DamageType", "Ice");
        if (Values[1]&0x00000004) Ent->SetFieldByte("BounceType", 1);
      }
      continue;
    }

    // states
    // spawn frame
    if (VStr::strEquCI(String, "Initial frame")) {
      hasSomeDefine = true;
      DoThingState(Ent, "Spawn");
      continue;
    }
    // see frame
    if (VStr::strEquCI(String, "First moving frame")) {
      hasSomeDefine = true;
      DoThingState(Ent, "See");
      continue;
    }
    // melee attack frame
    if (VStr::strEquCI(String, "Close attack frame")) {
      // don't change melee state for players
      hasSomeDefine = true;
      if (num != 1) DoThingState(Ent, "Melee");
      continue;
    }
    // missile attack frame
    if (VStr::strEquCI(String, "Far attack frame")) {
      // don't change missile state for players
      hasSomeDefine = true;
      if (num != 1) DoThingState(Ent, "Missile");
      continue;
    }
    // pain frame
    if (VStr::strEquCI(String, "Injury frame")) {
      hasSomeDefine = true;
      DoThingState(Ent, "Pain");
      continue;
    }
    // death frame
    if (VStr::strEquCI(String, "Death frame")) {
      hasSomeDefine = true;
      DoThingState(Ent, "Death");
      continue;
    }
    // xdeath frame
    if (VStr::strEquCI(String, "Exploding frame")) {
      hasSomeDefine = true;
      DoThingState(Ent, "XDeath");
      continue;
    }
    // raise frame
    if (VStr::strEquCI(String, "Respawn frame")) {
      hasSomeDefine = true;
      DoThingState(Ent, "Raise");
      continue;
    }

    // sounds
    if (VStr::strEquCI(String, "Alert sound")) { DoThingSound(Ent, "SightSound"); continue; }
    if (VStr::strEquCI(String, "Action sound")) { DoThingSound(Ent, "ActiveSound"); continue; }
    if (VStr::strEquCI(String, "Attack sound")) { DoThingSound(Ent, "AttackSound"); continue; }
    if (VStr::strEquCI(String, "Pain sound")) { DoThingSound(Ent, "PainSound"); continue; }
    if (VStr::strEquCI(String, "Death sound")) { DoThingSound(Ent, "DeathSound"); continue; }

    // reject mbf21
    if (VStr::strEquCI(String, "Infighting group") ||
        VStr::strEquCI(String, "Projectile group") ||
        VStr::strEquCI(String, "Splash group") ||
        VStr::strEquCI(String, "MBF21 Bits") ||
        VStr::strEquCI(String, "Rip sound") ||
        VStr::strEquCI(String, "Fast speed") ||
        VStr::strEquCI(String, "Melee range"))
    {
      DehFatal("MBF21 thing extensions are not supported and will never be. Sorry.");
    }

    // reject dehextra
    if (VStr::strEquCI(String, "Melee threshold") ||
        VStr::strEquCI(String, "Max target range") ||
        VStr::strEquCI(String, "Min missile chance") ||
        VStr::strEquCI(String, "Missile chance multiplier") ||
        VStr::strEquCI(String, "Dropped item"))
    {
      DehFatal("DEHEXTRA thing extensions are not supported and will never be. Sorry.");
    }

    Warning("Invalid mobj param '%s'", String);
  }

  // reset heights for things hanging from the ceiling that don't specify a new height
  if (!gotHeight && gotSpawnCeiling) {
    const float hgt = ((const VEntity *)Ent->Defaults)->Height;
    const float dvh = ((const VEntity *)Ent->Defaults)->VanillaHeight;
    if (dvh < 0 && hgt != -dvh) {
      Warning("forced vanilla thing height for '%s' (our:%g, vanilla:%g) (this is harmless)", *Ent->GetFullName(), hgt, -dvh);
      ((VEntity *)Ent->Defaults)->Height = -dvh;
    }
  }

  if (hasSomeDefine) EntClassTouched[num-1] = true;
}


//==========================================================================
//
//  ReadSound
//
//==========================================================================
static void ReadSound (int num) {
  // reject dehextra
  if (num >= 500 && num <= 699) DehFatal("DEHEXTRA is not supported and will never be. Sorry. (sound #%d)", num);
  bool ignoreit = false;
  if (num < 0 || num >= Sounds.length()) {
    Warning("Bad sound index %d", num);
    ignoreit = true;
  }
  while (ParseParam()) {
    if (VStr::strEquCI(String, "Zero/One")) {
      if (ignoreit) continue;
      // singularity
      if (!GSoundManager) continue;
      const int sid = GSoundManager->GetSoundID(Sounds[num]);
      if (sid) {
        //GCon->Logf(NAME_Debug, "setting bSingularity for sound #%d (%s) to %d", num, *Sounds[num], (int)(!!value));
        GSoundManager->SetSingularity(sid, !!value);
      }
      continue;
    }

    if (VStr::strEquCI(String, "Value")) {
      if (ignoreit) continue;
      // priority
      if (!GSoundManager) continue;
      const int sid = GSoundManager->GetSoundID(Sounds[num]);
      if (sid) {
        //GCon->Logf(NAME_Debug, "setting priority for sound #%d (%s) to %d", num, *Sounds[num], value);
        GSoundManager->SetPriority(sid, value);
      }
      continue;
    }

    if (VStr::strEquCI(String, "Offset")) { if (!ignoreit) Warning("cannot set lump name offset for sound #%d", num); continue; }  // lump name offset - can't handle
    if (VStr::strEquCI(String, "Zero 1")) { if (!ignoreit) Warning("cannot set lump num for sound #%d", num); continue; }          // lump num - can't be set
    if (VStr::strEquCI(String, "Zero 2")) { if (!ignoreit) Warning("cannot set data pointer for sound #%d", num); continue; }      // data pointer - can't be set
    if (VStr::strEquCI(String, "Zero 3")) { if (!ignoreit) Warning("cannot set usefulness for sound #%d", num); continue; }        // usefulness - removed
    if (VStr::strEquCI(String, "Zero 4")) { if (!ignoreit) Warning("cannot set link for sound #%d", num); continue; }              // link - removed
    if (VStr::strEquCI(String, "Neg. One 1")) { if (!ignoreit) Warning("cannot set link pitch for sound #%d", num); continue; }    // link pitch - removed
    if (VStr::strEquCI(String, "Neg. One 2")) { if (!ignoreit) Warning("cannot set link volume for sound #%d", num); continue; }   // link volume - removed

    Warning("Invalid sound param '%s'", String);
  }
}


//==========================================================================
//
//  ReadState
//
//==========================================================================
static void ReadState (int num) {
  // reject dehextra
  if (num >= 1089 && num <= 3999) DehFatal("DEHEXTRA is not supported and will never be. Sorry. (state #%d) (%d)", num, DehStates.length());

  // check index
  if (num >= DehStates.length() || num < 0) {
    Warning("Invalid state num %d", num);
    while (ParseParam()) {}
    return;
  }

  // state 0 is a special state
  if (num == 0) {
    while (ParseParam()) {}
    return;
  }

  const bool ignoreIt = (cli_DehackedIgnoreWeapons && isWeaponState(num));

  if (cli_DehackedDebug) {
    StInfo *sti = dehStInfoFind(num);
    GCon->Logf(NAME_Debug, "DEH:%d: ReadState(%d): class `%s`; stcps=%d; ofs=%d%s", dehCurrLine, num, sti->stclass->GetName(), sti->stcps, num-sti->stcps, (ignoreIt ? " -- IGNORED" : ""));
  }

  //TODO: do we need to set `EntClassTouched` here?
  //k8: yes
  while (ParseParam()) {
    if (ignoreIt) continue;
    // sprite base
    if (VStr::strEquCI(String, "Sprite number")) {
      // reject dehextra
      if (value >= 145 && value <= 244) DehFatal("DEHEXTRA is not supported and will never be. Sorry. (sprite #%d) (%d)", value, Sprites.length());
      if (value < 0 || value >= Sprites.length()) {
        Warning("Bad sprite index %d for frame #%d", value, num);
      } else {
        if (dbg_dehacked_dump_all || dbg_dehacked_frame_replaces) {
          DHDebugLog("frame #%d; old sprite is '%s'(%d), new sprite is '%s' (%s)", num, *DehStates[num].State->SpriteName, DehStates[num].State->SpriteIndex,
            *Sprites[value], *DehStates[num].State->Loc.toStringNoCol());
        }
        //GCon->Logf(NAME_Debug, "DEHACKED: frame #%d; prev sprite is '%s' (%d)", num, *DehStates[num].State->SpriteName, DehStates[num].State->SpriteIndex);
        DehStates[num].FixMe();
        DehStates[num].State->SpriteName = Sprites[value];
        DehStates[num].State->SpriteIndex = (Sprites[value] != NAME_None ? VClass::FindAddSprite(Sprites[value]) : 1);
        //GCon->Logf(NAME_Debug, "DEHACKED: frame #%d; NEW sprite is '%s' (%d)", num, *DehStates[num].State->SpriteName, DehStates[num].State->SpriteIndex);
      }
      continue;
    }
    // sprite frame
    if (VStr::strEquCI(String, "Sprite subnumber")) {
      if (value&0x8000) {
        value &= 0x7fff;
        value |= VState::FF_FULLBRIGHT;
      }
      DehStates[num].FixMe();
      DehStates[num].State->Frame = value;
      continue;
    }
    // state duration
    if (VStr::strEquCI(String, "Duration")) {
      DehStates[num].FixMe();
      DehStates[num].State->Time = (value < 0 ? value : value/35.0f);
      continue;
    }
    // next state
    if (VStr::strEquCI(String, "Next frame")) {
      if (value >= DehStates.length() || value < 0) {
        Warning("Invalid next state %d", value);
      } else {
        DehStates[num].FixMe();
        DehStates[value].FixMe();
        DehStates[num].State->NextState = DehStates[value].State;
        if (cli_DehackedDebug) {
          StInfo *sti = dehStInfoFind(value);
          GCon->Logf(NAME_Debug, "DEH:%d: ReadState(%d):   next: class `%s`; stcps=%d; ofs=%d", dehCurrLine, num, sti->stclass->GetName(), sti->stcps, value-sti->stcps);
        }
      }
      continue;
    }
    if (VStr::strEquCI(String, "Unknown 1")) { DehStates[num].FixMe(); DehStates[num].State->Misc1 = value; continue; }
    if (VStr::strEquCI(String, "Unknown 2")) { DehStates[num].FixMe(); DehStates[num].State->Misc2 = value; continue; }
    if (VStr::strEquCI(String, "Action pointer")) { Warning("Tried to set action pointer."); continue; }

    // reject mbf21
    if (VStr::strEquCI(String, "MBF21 Bits") ||
        VStr::strEquCI(String, "Args1") || VStr::strEquCI(String, "Args2") ||
        VStr::strEquCI(String, "Args3") || VStr::strEquCI(String, "Args4") ||
        VStr::strEquCI(String, "Args5") || VStr::strEquCI(String, "Args6") ||
        VStr::strEquCI(String, "Args7") || VStr::strEquCI(String, "Args8"))
    {
      DehFatal("MBF21 state extensions are not supported and will never be. Sorry.");
    }

    Warning("Invalid state param '%s'", String);
  }
}


//==========================================================================
//
//  ReadSpriteName
//
//==========================================================================
static void ReadSpriteName (int) {
  while (ParseParam()) {
    if (VStr::strEquCI(String, "Offset")) {} // can't handle
    else Warning("Invalid sprite name param '%s'", String);
  }
}


//==========================================================================
//
//  FixScale
//
//==========================================================================
static inline vint32 FixScale (vint32 a, vint32 b, vint32 c) {
  return (c ? (vint32)(((vint64)a*b)/c) : b);
}


//==========================================================================
//
//  ReadAmmo
//
//==========================================================================
static void ReadAmmo (int num) {
  // check index
  if (num >= AmmoClasses.length() || num < 0) {
    Warning("Invalid ammo num %d", num);
    while (ParseParam()) {}
    return;
  }

  const bool ignoreIt = cli_DehackedIgnoreWeapons;

  VClass *Weapon = VClass::FindClass("Weapon");
  VClass *Ammo = AmmoClasses[num];

  if (!Ammo) {
    Warning("trying to change null ammo num %d", num);
    while (ParseParam()) {}
    return;
  }

  const int oldVal = Ammo->GetFieldInt("Amount"); // we'll need it later

  int maxVal = -1;
  int perVal = -1;

  while (ParseParam()) {
         if (VStr::strEquCI(String, "Max ammo")) maxVal = value;
    else if (VStr::strEquCI(String, "Per ammo")) perVal = value;
    else Warning("Invalid ammo param '%s'", String);
  }

  if (maxVal < 0 && perVal < 0) return; // nothing to do

  // sanitise values
  if (maxVal >= 0 && perVal >= 0) {
    if (maxVal < perVal) maxVal = perVal;
  }

  if (!ignoreIt) {
    // set values
    if (maxVal >= 0) {
      Ammo->SetFieldInt("MaxAmount", maxVal);
      Ammo->SetFieldInt("BackpackMaxAmount", maxVal*2);
    }

    if (perVal >= 0) {
      Ammo->SetFieldInt("Amount", perVal);
      Ammo->SetFieldInt("BackpackAmount", perVal);
    }

    // fix up amounts in derived classes
    for (VClass *C = VMemberBase::GClasses; C; C = C->LinkNext) {
      if (C == Ammo) continue;
      if (C->IsChildOf(Ammo)) {
        // derived ammo
        if (maxVal >= 0) {
          // fix maximum value
          C->SetFieldInt("MaxAmount", maxVal);
          C->SetFieldInt("BackpackMaxAmount", maxVal*2);
        }
        if (perVal >= 0) {
          // fix default amout
          const int amnt = FixScale(C->GetFieldInt("Amount"), perVal, oldVal);
          if (maxVal < amnt) {
            // sanitise max amount
            C->SetFieldInt("MaxAmount", amnt);
            C->SetFieldInt("BackpackMaxAmount", amnt*2);
          }
          C->SetFieldInt("Amount", amnt);
          C->SetFieldInt("BackpackAmount", amnt);
        }
      } else if (Weapon && C->IsChildOf(Weapon)) {
        // fix weapon "ammo give"
        if (C->GetFieldClassValue("AmmoType1") == Ammo) {
          //GCon->Logf(NAME_Debug, "*** fixing ammo1 for weapon '%s' (ammo '%s'); perVal=%d; oldVal=%d", C->GetName(), Ammo->GetName(), perVal, oldVal);
          C->SetFieldInt("AmmoGive1", FixScale(C->GetFieldInt("AmmoGive1"), perVal, oldVal));
        }
        if (C->GetFieldClassValue("AmmoType2") == Ammo) {
          C->SetFieldInt("AmmoGive2", FixScale(C->GetFieldInt("AmmoGive2"), perVal, oldVal));
          //GCon->Logf(NAME_Debug, "*** fixing ammo2 for weapon '%s' (ammo '%s')", C->GetName(), Ammo->GetName());
        }
      }
    }
  }

  /*
  // fix up amounts in weapon classes
  for (int i = 0; i < WeaponClasses.length(); ++i) {
    VClass *C = WeaponClasses[i];
    if (GetClassFieldClass(C, "AmmoType1") == Ammo) SetClassFieldInt(C, "AmmoGive1", GetClassFieldInt(Ammo, "Amount")*2);
    if (GetClassFieldClass(C, "AmmoType2") == Ammo) SetClassFieldInt(C, "AmmoGive2", GetClassFieldInt(Ammo, "Amount")*2);
  }
  */
}


//==========================================================================
//
//  DoWeaponState
//
//==========================================================================
static void DoWeaponState (VClass *Weapon, const char *StateLabel) {
  if (value < 0 || value >= DehStates.length()) {
    Warning("Invalid weapon state %d for weapon '%s'", value, (Weapon ? Weapon->GetName() : "<undefined>"));
  } else {
    DehStates[value].FixMe();
    Weapon->SetStateLabel(StateLabel, DehStates[value].State);
  }
}


//==========================================================================
//
//  ReadWeapon
//
//==========================================================================
static void ReadWeapon (int num) {
  // check index
  if (num < 0 || num >= WeaponClasses.length()) {
    Warning("Invalid weapon num %d", num);
    while (ParseParam()) {}
    return;
  }

  const bool ignoreIt = cli_DehackedIgnoreWeapons;

  VClass *Weapon = WeaponClasses[num];
  while (ParseParam()) {
    if (ignoreIt) continue;

    // ammo type
    if (VStr::strEquCI(String, "Ammo type")) {
      VClass *ammo = (value >= 0 && value < AmmoClasses.length() ? AmmoClasses[value] : nullptr);
      if (ammo) {
        Message("replacing ammo for weapon '%s' with '%s'", Weapon->GetName(), ammo->GetName());
        Weapon->SetFieldClassValue("AmmoType1", ammo);
        Weapon->SetFieldInt("AmmoGive1", ammo->GetFieldInt("Amount")*2);
        if (Weapon->GetFieldInt("AmmoUse1") == 0) Weapon->SetFieldInt("AmmoUse1", 1);
      } else {
        Message("replacing ammo for weapon '%s' with nothing", Weapon->GetName());
        Weapon->SetFieldClassValue("AmmoType1", nullptr);
        Weapon->SetFieldBool("bAmmoOptional", true);
      }
      continue;
    }

    // ammo use / ammo per shot
    if (VStr::strEquCI(String, "Ammo use") || VStr::strEquCI(String, "Ammo per shot")) {
      Weapon->SetFieldInt("AmmoUse1", value);
      continue;
    }

    // min ammo
    if (VStr::strEquCI(String, "Min Ammo")) {
      // unused
      continue;
    }

    // states
    if (VStr::strEquCI(String, "Deselect frame")) { DoWeaponState(Weapon, "Deselect"); continue; } //k8: this was "Select", bug?
    if (VStr::strEquCI(String, "Select frame")) { DoWeaponState(Weapon, "Select"); continue; } //k8: this was "Deselect", bug?
    if (VStr::strEquCI(String, "Bobbing frame")) { DoWeaponState(Weapon, "Ready"); continue; }
    if (VStr::strEquCI(String, "Shooting frame")) { DoWeaponState(Weapon, "Fire"); continue; }
    if (VStr::strEquCI(String, "Firing frame")) { DoWeaponState(Weapon, "Flash"); continue; }

    // reject mbf21
    if (VStr::strEquCI(String, "MBF21 Bits")) {
      DehFatal("MBF21 weapon extensions are not supported and will never be. Sorry.");
    }

    Warning("Invalid weapon param '%s' for weapon '%s'", String, (Weapon ? Weapon->GetName() : "<undefined>"));
  }
}


//==========================================================================
//
//  ReadPointer
//
//==========================================================================
static void ReadPointer (int num) {
  if (num < 0 || num >= CodePtrStatesIdx.length()) {
    Warning("Invalid pointer");
    while (ParseParam()) {}
    return;
  }

  const bool ignoreIt = (cli_DehackedIgnoreWeapons && isWeaponState(CodePtrStatesIdx[num]));

  if (cli_DehackedDebug) {
    const int xnum = CodePtrStatesIdx[num];
    StInfo *sti = dehStInfoFind(xnum);
    GCon->Logf(NAME_Debug, "DEH:%d: ReadPointer(%d): class `%s`; stcps=%d; ofs=%d%s", dehCurrLine, xnum, sti->stclass->GetName(), sti->stcps, xnum-sti->stcps, (ignoreIt ? " -- IGNORED" : ""));
  }

  while (ParseParam()) {
    if (ignoreIt) continue;

    if (VStr::strEquCI(String, "Codep Frame")) {
      if (value < 0 || value >= DehStates.length()) {
        Warning("Invalid source state %d", value);
      } else {
        DehStates[CodePtrStatesIdx[num]].FixMe();
        DehStates[value].FixMe();
        VState *st = DehStates[CodePtrStatesIdx[num]].State;
        if (dbg_dehacked_dump_all || dbg_dehacked_codepointers) {
          DHDebugLog("replacing codep frame #%d code pointer; old is (%d) '%s', new is '%s' (%s)", num, value, (st->Function ? *st->Function->GetFullName() : "none"),
            (DehStates[value].Action ? *DehStates[value].Action->GetFullName() : "none"), *st->Loc.toStringNoCol());
        }
        if (cli_DehackedDebug) {
          const char *ffname = (st->Function ? *st->Function->GetFullName() : "{WTF?!}");
          GCon->Logf(NAME_Debug, "DEH:%d: ReadPointer(%d):   replacing `%s` with `%s`", dehCurrLine, num, ffname, *DehStates[value].Action->GetFullName());
        }
        st->Function = DehStates[value].Action;
      }
      continue;
    }

    Warning("Invalid pointer param '%s'", String);
  }
}


//==========================================================================
//
//  ReadCodePtr
//
//==========================================================================
static void ReadCodePtr (int) {
  // cannot use `DC_SetupStateMethod()` here, because creating new wrappers requires postloading
  while (ParseParam()) {
    if (VStr::NICmp(String, "Frame", 5) == 0 && (vuint8)String[5] <= ' ') {
      int Index = VStr::atoi(String+6);

      // reject dehextra
      if (Index >= 1089 && Index <= 3999) DehFatal("DEHEXTRA is not supported and will never be. Sorry. (frame #%d) (%d)", Index, DehStates.length());

      if (Index < 0 || Index >= DehStates.length()) {
        Warning("Bad frame index %d", Index);
        continue;
      }

      const bool ignoreIt = (cli_DehackedIgnoreWeapons && isWeaponState(Index));

      DehStates[Index].FixMe();
      VState *State = DehStates[Index].State;
      if ((ValueString[0] == 'A' || ValueString[0] == 'a') && ValueString[1] == '_') ValueString += 2;

      bool found = false;
      for (auto &&it : CodePtrs) {
        if (it.Name.strEquCI(ValueString)) {
          if (dbg_dehacked_dump_all || dbg_dehacked_codepointers) DHDebugLog("replacing frame #%d code pointer; old is '%s', new is '%s' (%s)", Index, (State->Function ? *State->Function->GetFullName() : "none"), (it.Method ? *it.Method->GetFullName() : "none"), *State->Loc.toStringNoCol());
          if (!ignoreIt) State->Function = it.Method;
          //DC_SetupStateMethod(State, it.Method);
          found = true;
          break;
        }
      }

      if (!found) {
        // try autorouting
        VClass *Class = VClass::FindClass("Actor");
        if (Class) {
          VStr mtn(va("A_%s", ValueString));
          VMethod *Method = Class->FindMethodNoCase(mtn);
          if (Method && (!Method->IsDecorate() || !Method->IsGoodStateMethod())) Method = nullptr;
          if (!Method) {
            mtn = va("decorate_A_%s", ValueString);
            Method = Class->FindMethodNoCase(mtn);
          }
          if (Method && Method->IsDecorate() && Method->IsGoodStateMethod()) {
            //Message("*** %s -> %s", ValueString, *Method->GetFullName());
            if (dbg_dehacked_dump_all || dbg_dehacked_codepointers) DHDebugLog("replacing frame #%d code pointer; old is '%s', new is '%s' (%s)", Index, (State->Function ? *State->Function->GetFullName() : "none"), (Method ? *Method->GetFullName() : "none"), *State->Loc.toStringNoCol());
            if (!ignoreIt) State->Function = Method;
            //DC_SetupStateMethod(State, Method);
            found = true;
          }
        }
      }

      if (!found) {
        // reject dehextra
        if (VStr::strEquCI(ValueString, "Spawn")) {
          DehFatal("DEHEXTRA frame extensions are not supported and will never be. Sorry.");
        }
        Warning("Invalid code pointer '%s'", ValueString);
      }
      continue;
    }

    Warning("Invalid code pointer param '%s'", String);
  }
}


//==========================================================================
//
//  ReadCheats
//
//==========================================================================
static void ReadCheats (int) {
  // old cheat handling is removed
  while (ParseParam()) {}
}


//==========================================================================
//
//  DoPowerupColor
//
//==========================================================================
static void DoPowerupColor (const char *ClassName) {
  VClass *Power = VClass::FindClass(ClassName);
  if (!Power) {
    Warning("Can't find powerup class '%s'", ClassName);
    return;
  }

  int r, g, b;
  float a;
  if (sscanf(ValueString, "%d %d %d %f", &r, &g, &b, &a) != 4) {
    Warning("Bad powerup color '%s' for class '%s'", ValueString, ClassName);
    return;
  }
  r = midval(0, r, 255);
  g = midval(0, g, 255);
  b = midval(0, b, 255);
  a = midval(0.0f, a, 1.0f);
  Power->SetFieldInt("BlendColor", (r<<16)|(g<<8)|b|int(a*255)<<24);
}


//==========================================================================
//
//  ReadMisc
//
//==========================================================================
static void ReadMisc (int) {
  while (ParseParam()) {
    if (VStr::strEquCI(String, "Initial Health")) { GameInfoClass->SetFieldInt("INITIAL_HEALTH", value); continue; }
    if (VStr::strEquCI(String, "Initial Bullets")) {
      TArray<VDropItemInfo>& List = *(TArray<VDropItemInfo>*)(DoomPlayerClass->Defaults+DoomPlayerClass->FindFieldChecked("DropItemList")->Ofs);
      for (int i = 0; i < List.length(); ++i) if (List[i].Type && List[i].Type->Name == "Clip") List[i].Amount = value;
      continue;
    }
    if (VStr::strEquCI(String, "Max Health")) { HealthBonusClass->SetFieldInt("MaxAmount", 2*value); continue; }
    if (VStr::strEquCI(String, "Max Armor")) { ArmorBonusClass->SetFieldInt("MaxSaveAmount", value); continue; }
    if (VStr::strEquCI(String, "Green Armor Class")) {
      GreenArmorClass->SetFieldInt("SaveAmount", 100*value);
      GreenArmorClass->SetFieldFloat("SavePercent", value == 1 ? 1.0f/3.0f : 1.0f*0.5f);
      continue;
    }
    if (VStr::strEquCI(String, "Blue Armor Class")) {
      BlueArmorClass->SetFieldInt("SaveAmount", 100*value);
      BlueArmorClass->SetFieldFloat("SavePercent", value == 1 ? 1.0f/3.0f : 1.0f*0.5f);
      continue;
    }
    if (VStr::strEquCI(String, "Max Soulsphere")) { SoulsphereClass->SetFieldInt("MaxAmount", value); continue; }
    if (VStr::strEquCI(String, "Soulsphere Health")) { SoulsphereClass->SetFieldInt("Amount", value); continue; }
    if (VStr::strEquCI(String, "Megasphere Health")) {
      MegaHealthClass->SetFieldInt("Amount", value);
      MegaHealthClass->SetFieldInt("MaxAmount", value);
      continue;
    }
    if (VStr::strEquCI(String, "God Mode Health")) { GameInfoClass->SetFieldInt("GOD_HEALTH", value); continue; }
    if (VStr::strEquCI(String, "IDFA Armor")) continue; // cheat removed
    if (VStr::strEquCI(String, "IDFA Armor Class")) continue; // cheat removed
    if (VStr::strEquCI(String, "IDKFA Armor")) continue; // cheat removed
    if (VStr::strEquCI(String, "IDKFA Armor Class")) continue; // cheat removed
    if (VStr::strEquCI(String, "BFG Cells/Shot")) { BfgClass->SetFieldInt("AmmoUse1", value); continue; }
    if (VStr::strEquCI(String, "Monsters Infight")) { Infighting = value; continue; }
    if (VStr::strEquCI(String, "Monsters Ignore Each Other")) { Infighting = (value ? -1 : 0); continue; }
    if (VStr::strEquCI(String, "Powerup Color Invulnerability")) { DoPowerupColor("PowerInvulnerable"); continue; }
    if (VStr::strEquCI(String, "Powerup Color Berserk")) { DoPowerupColor("PowerStrength"); continue; }
    if (VStr::strEquCI(String, "Powerup Color Invisibility")) { DoPowerupColor("PowerInvisibility"); continue; }
    if (VStr::strEquCI(String, "Powerup Color Radiation Suit")) { DoPowerupColor("PowerIronFeet"); continue; }
    if (VStr::strEquCI(String, "Powerup Color Infrared")) { DoPowerupColor("PowerLightAmp"); continue; }
    if (VStr::strEquCI(String, "Powerup Color Tome of Power")) { DoPowerupColor("PowerWeaponLevel2"); continue; }
    if (VStr::strEquCI(String, "Powerup Color Wings of Wrath")) { DoPowerupColor("PowerFlight"); continue; }
    if (VStr::strEquCI(String, "Powerup Color Speed")) { DoPowerupColor("PowerSpeed"); continue; }
    if (VStr::strEquCI(String, "Powerup Color Minotaur")) { DoPowerupColor("PowerMinotaur"); continue; }
    if (VStr::strEquCI(String, "Rocket Explosion Style")) { GameInfoClass->SetFieldInt("DehExplosionStyle", ParseRenderStyle()); continue; }
    if (VStr::strEquCI(String, "Rocket Explosion Alpha")) { GameInfoClass->SetFieldFloat("DehExplosionAlpha", VStr::atof(ValueString, 1)); continue; }

    Warning("Invalid misc '%s'", String);
  }

  // 0xdd means "enable infighting"
       if (Infighting == 0xdd) Infighting = 1;
  else if (Infighting != -1) Infighting = 0;
}


//==========================================================================
//
//  ReadPars
//
//==========================================================================
static void ReadPars (int) {
  while (GetLine()) {
    if (strchr(String, '=')) {
      Warning("Unknown key in Pars section (%s)", String);
      continue;
    }
    TArray<VStr> cmda;
    VStr line = VStr(String);
    line.splitOnBlanks(cmda);
    //GLog.Logf(":::<%s>\n=== len: %d", *line, cmda.length()); for (int f = 0; f < cmda.length(); ++f) GLog.Logf("  %d: <%s>", f, *cmda[f]);
    if (cmda.length() < 1) return;
    if (!cmda[0].strEquCI("par")) return;
    if (cmda.length() < 3 || cmda.length() > 4) {
      Warning("Bad par time");
      continue;
    }
    int nums[4];
    bool ok = true;
    memset(nums, 0, sizeof(nums));
    for (int f = 1; f < cmda.length(); ++f) {
      if (!cmda[f].convertInt(&nums[f]) || nums[f] < 0) {
        Warning("Bad par time");
        ok = false;
        break;
      }
    }
    if (!ok) continue;
    if (cmda.length() == 4) {
      VName MapName = va("e%dm%d", nums[1], nums[2]);
      P_SetParTime(MapName, nums[3]);
    } else {
      VName MapName = va("map%02d", nums[1]%100);
      P_SetParTime(MapName, nums[2]);
    }
  }
}


//==========================================================================
//
//  FindString
//
//==========================================================================
static void FindString (const char *oldStr, const char *newStr) {
  // sounds
  bool SoundFound = false;
  for (int i = 0; i < SfxNames.length(); ++i) {
    if (SfxNames[i].Old.strEquCI(oldStr)) {
      SfxNames[i].New = VStr(newStr).toLowerCase();
      SfxNames[i].Replaced = true;
      // continue, because other sounds can use the same sound
      SoundFound = true;
    }
  }
  if (SoundFound) return;

  // music
  bool SongFound = false;
  for (int i = 0; i < MusicNames.length(); ++i) {
    if (MusicNames[i].Old.strEquCI(oldStr)) {
      MusicNames[i].New = VStr(newStr).toLowerCase();
      MusicNames[i].Replaced = true;
      // there could be duplicates
      SongFound = true;
    }
  }
  if (SongFound) return;

  // sprite names
  for (int i = 0; i < SpriteNames.length(); ++i) {
    if (SpriteNames[i].Old.strEquCI(oldStr)) {
      if (!VStr::strEquCI(newStr, oldStr)) {
        if (cli_DehackedIgnoreWeapons && isWeaponSprite(oldStr)) {
          // do nothing
        } else {
          SpriteNames[i].New = VStr(newStr).toLowerCase();
          SpriteNames[i].Replaced = true;
        }
      }
      return;
    }
  }

  VName Id = EngStrings->GetStringIdCI(oldStr);
  if (Id != NAME_None) {
    GLanguage.ReplaceString(Id, newStr);
    return;
  }

  vuint32 hash = XXH32(oldStr, strlen(oldStr), 0x29a);
  if (hash != 0xde390ed6u && /* startup header line, doom2 */
      hash != 0x414bc626u && /* "modified" text, doom2 */
      hash != 0xd254973du && /* "Degreelessness Mode On" */
      hash != 0x41c08d13u)   /* "Degreelessness Mode Off" */
  {
    Warning("Not found old (0x%08x) \"%s\" new \"%s\"", hash, oldStr, newStr);
  }
}


//==========================================================================
//
//  ReadText
//
//==========================================================================
static void ReadText (int oldSize) {
  char *lenPtr;
  int newSize;
  char *oldStr;
  char *newStr;
  int len;

  lenPtr = strtok(nullptr, " \t");
  if (!lenPtr) return;
  newSize = VStr::atoi(lenPtr);

  oldStr = new char[oldSize+1];
  newStr = new char[newSize+1];

  len = 0;
  while (*PatchPtr && len < oldSize) {
    if (*PatchPtr == '\r') { ++PatchPtr; continue; }
    if (*PatchPtr == '\n') ++dehCurrLine;
    oldStr[len] = *PatchPtr;
    ++PatchPtr;
    ++len;
  }
  oldStr[len] = 0;

  len = 0;
  while (*PatchPtr && len < newSize) {
    if (*PatchPtr == '\r') { ++PatchPtr; continue; }
    if (*PatchPtr == '\n') ++dehCurrLine;
    newStr[len] = *PatchPtr;
    ++PatchPtr;
    ++len;
  }
  newStr[len] = 0;

  FindString(oldStr, newStr);

  delete[] oldStr;
  delete[] newStr;

  GetLine();
}


//==========================================================================
//
//  ReplaceSpecialChars
//
//==========================================================================
static VStr ReplaceSpecialChars (VStr &In) {
  VStr Ret;
  const char *pStr = *In;
  while (*pStr) {
    char c = *pStr++;
    if (c != '\\') {
      Ret += c;
    } else {
      switch (*pStr) {
        case 'n': case 'N': Ret += '\n'; break;
        case 't': case 'T': Ret += '\t'; break;
        case 'r': case 'R': Ret += '\r'; break;
        case 'x': case 'X':
          c = 0;
          ++pStr;
          for (int i = 0; i < 2; ++i) {
            c <<= 4;
                 if (*pStr >= '0' && *pStr <= '9') c += *pStr-'0';
            else if (*pStr >= 'a' && *pStr <= 'f') c += 10+*pStr-'a';
            else if (*pStr >= 'A' && *pStr <= 'F') c += 10+*pStr-'A';
            else break;
            ++pStr;
          }
          Ret += c;
          break;
        case '0': case '1': case '2': case '3':
        case '4': case '5': case '6': case '7':
          c = 0;
          for (int i = 0; i < 3; ++i) {
            c <<= 3;
            if (*pStr >= '0' && *pStr <= '7') c += *pStr-'0'; else break;
            ++pStr;
          }
          Ret += c;
          break;
        default:
          Ret += *pStr;
          break;
      }
      ++pStr;
    }
  }
  return Ret;
}


//==========================================================================
//
//  ReadStrings
//
//==========================================================================
static void ReadStrings (int) {
  while (ParseParam()) {
    VName Id = *VStr(String).ToLower();
    VStr Val;
    do {
      char *End = ValueString+VStr::Length(ValueString)-1;
      while (End >= ValueString && (vuint8)*End <= ' ') --End;
      End[1] = 0;
      if (End >= ValueString && *End == '\\') {
        *End = 0;
        Val += ValueString;
        ValueString = PatchPtr;
        while (*PatchPtr && *PatchPtr != '\n') ++PatchPtr;
        if (*PatchPtr == '\n') {
          ++dehCurrLine;
          *PatchPtr = 0;
          ++PatchPtr;
        }
        while (*ValueString && *ValueString <= ' ') ++ValueString;
      } else {
        Val += ValueString;
        ValueString = nullptr;
      }
    } while (ValueString && *ValueString);
    Val = ReplaceSpecialChars(Val);
    GLanguage.ReplaceString(Id, Val);
  }
}


//==========================================================================
//
//  LoadDehackedFile
//
//==========================================================================
static void LoadDehackedFile (VStream *Strm, int sourceLump) {
  dehCurrLine = 0;
  Patch = new char[Strm->TotalSize()+1];
  Strm->Serialise(Patch, Strm->TotalSize());
  Patch[Strm->TotalSize()] = 0;
  VStream::Destroy(Strm);
  Strm = nullptr;
  PatchPtr = Patch;

  if (!VStr::NCmp(Patch, "Patch File for DeHackEd v", 25)) {
    if (Patch[25] < '3') {
      delete[] Patch;
      Patch = nullptr;
      Warning("DeHackEd patch is too old");
      return;
    }
    GetLine();
    int DVer = -1;
    int PFmt = -1;
    while (ParseParam()) {
           if (VStr::strEquCI(String, "Doom version")) DVer = value;
      else if (VStr::strEquCI(String, "Patch format")) PFmt = value;
      // skip some WhackEd3 stuff
      else if (VStr::strEquCI(String, "Engine config") ||
               VStr::strEquCI(String, "Data WAD") ||
               VStr::strEquCI(String, "IWAD")) continue;
      else Warning("Unknown parameter '%s'", String);
    }
    if (!String || DVer == -1 || PFmt == -1) {
      delete[] Patch;
      Patch = nullptr;
      Warning("Not a DeHackEd patch file");
      return;
    }
  } else {
    Warning("Missing DeHackEd header, assuming BEX file");
    bexMode = true;
    GetLine();
  }

  while (String) {
    char *Section = strtok(String, " \t");
    if (!Section) { GetLine(); continue; }

    char *numStr = strtok(nullptr, " \t");
    int i = 0;
    if (numStr) i = VStr::atoi(numStr);

         if (VStr::strEquCI(Section, "Thing")) ReadThing(i);
    else if (VStr::strEquCI(Section, "Sound")) ReadSound(i);
    else if (VStr::strEquCI(Section, "Frame")) ReadState(i);
    else if (VStr::strEquCI(Section, "Sprite")) ReadSpriteName(i);
    else if (VStr::strEquCI(Section, "Ammo")) ReadAmmo(i);
    else if (VStr::strEquCI(Section, "Weapon")) ReadWeapon(i);
    else if (VStr::strEquCI(Section, "Pointer")) ReadPointer(i);
    else if (VStr::strEquCI(Section, "Cheat")) ReadCheats(i);
    else if (VStr::strEquCI(Section, "Misc")) ReadMisc(i);
    else if (VStr::strEquCI(Section, "Text")) ReadText(i);
    else if (VStr::strEquCI(Section, "[Strings]")) ReadStrings(i);
    else if (VStr::strEquCI(Section, "[Pars]")) ReadPars(i);
    else if (VStr::strEquCI(Section, "[CodePtr]")) ReadCodePtr(i);
    else if (VStr::strEquCI(Section, "Include")) {
      VStr LumpName = numStr;
      //int Lump = W_CheckNumForFileName(LumpName);
      int Lump = (sourceLump >= 0 ? W_CheckNumForFileNameInSameFileOrLower(W_LumpFile(sourceLump), LumpName) : W_CheckNumForFileName(LumpName));
      // check WAD lump only if it's no longer than 8 characters and has no path separator
      if (Lump < 0 && LumpName.IndexOf('/') < 0) {
        VStr newName = LumpName.stripExtension();
        if (!newName.isEmpty() && newName.length() <= 8) {
          VName nn = VName(*newName, VName::FindLower8);
          if (nn != NAME_None) Lump = (sourceLump >= 0 ? W_CheckNumForNameInFileOrLower(nn, W_LumpFile(sourceLump)) : W_CheckNumForName(nn));
        }
      }
      if (Lump < 0) {
        Warning("Lump '%s' not found", *LumpName);
      } else {
        Message("including '%s' from '%s'", *LumpName, *W_FullLumpName(Lump));
        char *SavedPatch = Patch;
        char *SavedPatchPtr = PatchPtr;
        char *SavedString = String;
        LoadDehackedFile(W_CreateLumpReaderNum(Lump), Lump);
        Message("finished include '%s'.", *LumpName);
        Patch = SavedPatch;
        PatchPtr = SavedPatchPtr;
        String = SavedString;
      }
      GetLine();
    } else {
      Warning("Don't know how to handle \"%s\"", String);
      GetLine();
    }
  }
  delete[] Patch;
  Patch = nullptr;
}


//==========================================================================
//
//  LoadDehackedDefinitions
//
//==========================================================================
static void LoadDehackedDefinitions () {
  // open dehinfo script
  VStream *Strm = FL_OpenFileReadBaseOnly("dehinfo.txt");
  if (!Strm) Sys_Error("'dehinfo.txt' is required to parse dehacked patches");

  GCon->Logf(NAME_Init, "loading dehacked definitions from '%s'", *Strm->GetName());
  VScriptParser *sc = new VScriptParser("dehinfo.txt", Strm);

  // read sprite names
  sc->Expect("sprites");
  sc->Expect("{");
  while (!sc->Check("}")) {
    sc->ExpectString();
    if (sc->String.Length() != 4) sc->Error(va("Dehacked sprite name must be 4 characters long, got '%s'", *sc->String));
    Sprites.Append(VName(*sc->String, VName::AddLower)); //k8: was upper, why? sprite names are lowercased
  }

  // read states
  sc->Expect("states");
  sc->Expect("{");
  // append `S_NULL`
  DehStates.Append(DehState());
  // append other
  VState **StatesTail = &VClass::FindClass("Entity")->NetStates;
  while (*StatesTail) StatesTail = &(*StatesTail)->NetNext;
  VClass *EntEx = VClass::FindClass("EntityEx");
  while (!sc->Check("}")) {
    StInfo sti;
    // class name
    sc->ExpectString();
    VClass *StatesClass = VClass::FindClass(*sc->String);
    if (!StatesClass) sc->Error(va("Dehacked: no such class `%s`", *sc->String));
    if (!StatesClass->IsChildOf(EntEx)) sc->Error(va("Dehacked: class `%s` is not EntityEx", *sc->String));
    sti.stclass = StatesClass;
    // starting state specifier
    VState *S = nullptr;
    VState **pState = nullptr;
    if (sc->Check("First")) {
      sti.stname = "First";
      S = StatesClass->NetStates;
      pState = &StatesClass->NetStates;
    } else {
      sc->ExpectString();
      VStateLabel *Lbl = StatesClass->FindStateLabel(*sc->String);
      if (!Lbl) sc->Error(va("Dehacked: no such state `%s` in class `%s`", *sc->String, StatesClass->GetName()));
      sti.stname = sc->String;
      S = Lbl->State;
      pState = &StatesClass->NetStates;
      while (*pState && *pState != S) pState = &(*pState)->NetNext;
      if (!pState[0]) sc->Error("Bad state");
    }
    sti.stfirst = S;
    sti.stcps = DehStates.length();
    // number of states
    sc->ExpectNumber();
    if (sc->Number < 1) sc->Error(va("Dehacked: invalid state count in class `%s`", StatesClass->GetName()));
    sti.count = sc->Number;
    dehStInfo.append(sti);
    //GCon->Logf(NAME_Debug, "STATES: class=`%s`; count=%d", StatesClass->GetName(), sc->Number);
    //if (cli_DehackedDebug) GCon->Logf(NAME_Debug, "DEH: STATES: class=`%s`; cpsindex=%d; count=%d", StatesClass->GetName(), DehStates.length(), sc->Number);
    for (int i = 0; i < sc->Number; ++i) {
      if (!S) sc->Error(va("Dehacked: class `%s` doen't have that many states (%d, aborted at %d)", StatesClass->GetName(), sc->Number, i));
      DehState dehst;
      dehst.State = S;
      dehst.Action = S->Function;
      dehst.Class = StatesClass;
      dehst.BarrelFixStop = false;
      dehst.BarrelRemoveAction = false;
      DehStates.Append(dehst);
      // move net links to actor class
      *StatesTail = S;
      StatesTail = &S->NetNext;
      *pState = S->NetNext;
      S->NetNext = nullptr;
      S = S->Next;
    }
    // hack for barrels
    if (VStr::strEqu(StatesClass->GetName(), "ExplosiveBarrel")) {
      vassert(DehStates.length()-sti.stcps == 7);
      DehStates[DehStates.length()-3].BarrelRemoveAction = true;
      DehStates[DehStates.length()-3].Action = nullptr; // this frame has no action
      DehStates[DehStates.length()-1].BarrelFixStop = true;
    }
  }

  if (cli_DehackedDebug) {
    GCon->Log(NAME_Debug, "DEH: states");
    for (auto &&sti : dehStInfo) {
      GCon->Logf(NAME_Debug, "DEH: class `%s`; state `%s`; cps=%d; count=%d; stloc=%s", sti.stclass->GetName(), *sti.stname, sti.stcps, sti.count, *sti.stfirst->Loc.toStringNoCol());
    }
    GCon->Log(NAME_Debug, "DEH: ======");
  }

  // read code pointer state mapping
  sc->Expect("code_pointer_states");
  sc->Expect("{");
  if (cli_DehackedDebug) GCon->Log(NAME_Debug, "DEH: code_pointer_states");
  while (!sc->Check("}")) {
    sc->ExpectNumber();
    if (sc->Number < 0 || sc->Number >= DehStates.length()) sc->Error(va("Bad state index %d", sc->Number));
    //GCon->Logf(NAME_Debug, "DEHACKED: cpsidx=%d; cpnum=%d; state=%s", CodePtrStatesIdx.length(), sc->Number, *DehStates[sc->Number].State->Loc.toString());
    if (cli_DehackedDebug) {
      StInfo *sti = dehStInfoFind(sc->Number);
      const char *ffname = (DehStates[sc->Number].State->Function ? *DehStates[sc->Number].State->Function->GetFullName() : "{WTF?!}");
      GCon->Logf(NAME_Debug, "DEH: cps#%d: class `%s`; state `%s`; mt `%s`; cpsofs=%d; stloc=%s", sc->Number, sti->stclass->GetName(), *sti->stname, ffname,
        sc->Number-sti->stcps, *DehStates[sc->Number].State->Loc.toStringNoCol());
    }
    CodePtrStatesIdx.Append(sc->Number);
  }
  if (cli_DehackedDebug) GCon->Log(NAME_Debug, "DEH: ======");

  // read code pointers
  sc->Expect("code_pointers");
  sc->Expect("{");
  VCodePtrInfo &ANull = CodePtrs.Alloc();
  ANull.Name = "NULL";
  ANull.Method = nullptr;
  while (!sc->Check("}")) {
    sc->ExpectString();
    VStr Name = sc->String;
    sc->ExpectString();
    VStr ClassName = sc->String;
    sc->ExpectString();
    VStr MethodName = sc->String;
    VClass *Class = VClass::FindClass(*ClassName);
    if (Class == nullptr) sc->Error(va("Dehacked: no such class `%s`", *ClassName));
    VMethod *Method = Class->FindMethod(*MethodName);
    if (Method == nullptr) sc->Error(va("Dehacked: no such method `%s` in class `%s`", *MethodName, *ClassName));
    if (!Method->IsDecorate()) sc->Error(va("Dehacked: method `%s` in class `%s` is not a decorate method", *MethodName, *ClassName));
    if (!Method->IsGoodStateMethod()) sc->Error(va("Dehacked: method `%s` in class `%s` cannot be called without arguments", *Method->GetFullName(), *ClassName));
    VCodePtrInfo &P = CodePtrs.Alloc();
    P.Name = Name;
    P.Method = Method;
  }

  // read sound names
  sc->Expect("sounds");
  sc->Expect("{");
  Sounds.Append(NAME_None);
  while (!sc->Check("}")) {
    sc->ExpectString();
    Sounds.Append(*sc->String);
  }

  // create list of thing classes
  sc->Expect("things");
  sc->Expect("{");
  while (!sc->Check("}")) {
    sc->ExpectString();
    VClass *C = VClass::FindClass(*sc->String);
    if (!C) sc->Error(va("No such class %s", *sc->String));
    EntClasses.Append(C);
    EntClassTouched.append(false);
  }
  vassert(EntClasses.length() == EntClassTouched.length());

  // create list of weapon classes
  sc->Expect("weapons");
  sc->Expect("{");
  while (!sc->Check("}")) {
    if (sc->Check(",")) continue;
    sc->ExpectString();
    VClass *C = VClass::FindClass(*sc->String);
    if (!C) sc->Error(va("No such class %s", *sc->String));
    WeaponClasses.Append(C);
  }

  // create list of ammo classes
  sc->Expect("ammo");
  sc->Expect("{");
  while (!sc->Check("}")) {
    if (sc->Check(",")) continue;
    sc->ExpectString();
    if (sc->String.strEquCI("null")) {
      AmmoClasses.Append(nullptr);
    } else {
      VClass *C = VClass::FindClass(*sc->String);
      if (!C) sc->Message(va("No such ammo class '%s' (dehinfo)", *sc->String));
      AmmoClasses.Append(C);
    }
  }

  // original thing heights are encoded in decorate now (with `ProjectilePassHeight`)

  GameInfoClass = VClass::FindClass("MainGameInfo");
  DoomPlayerClass = VClass::FindClass("DoomPlayer");
  BfgClass = VClass::FindClass("BFG9000");
  HealthBonusClass = VClass::FindClass("HealthBonus");
  SoulsphereClass = VClass::FindClass("Soulsphere");
  MegaHealthClass = VClass::FindClass("MegasphereHealth");
  GreenArmorClass = VClass::FindClass("GreenArmor");
  BlueArmorClass = VClass::FindClass("BlueArmor");
  ArmorBonusClass = VClass::FindClass("ArmorBonus");

  delete sc;

  // get lists of strings to replace
  GSoundManager->GetSoundLumpNames(SfxNames);
  P_GetMusicLumpNames(MusicNames);
  VClass::GetSpriteNames(SpriteNames);
  EngStrings = new VLanguage();
  EngStrings->LoadStrings("en");
}


//==========================================================================
//
//  getFileWadName
//
//==========================================================================
static VStr getFileWadName (int fidx) {
  VStr pkname = W_FullPakNameByFile(fidx);
  if (!pkname.endsWithCI(".wad")) return VStr::EmptyString;
  auto lpos = pkname.lastIndexOf(':');
  if (lpos >= 0) pkname.chopLeft(lpos+1);
  return pkname.extractFileName();
}


//==========================================================================
//
//  FindDehackedLump
//
//  this is a hack for older wads that comes with "wadname.deh" in
//  their zip archive. it checks if the wad has built-in dehacked
//  lump, and if it isn't, looks for "wadname.deh" instead
//
//==========================================================================
static void FindDehackedLumps (TArray<int> &lumplist) {
  // build list of wad files
  struct WadInfo {
    VStr basename;
    int wadfidx;
    int dlump;
    bool isbex;
    bool disabled;
  };

  TArray<WadInfo> wadlist;
  TMap<VStrCI, int> dehmap; // key: "name.deh"; value: index in wadlist

  if (cli_NoExternalDeh <= 0) {
    for (int fnum = 0; fnum < W_NextMountFileId(); ++fnum) {
      VStr pkname = getFileWadName(fnum);
      if (pkname.isEmpty()) continue;
      VStr basename = pkname.stripExtension();
      if (basename.isEmpty()) continue;
      // remove duplicates
      int idx = -1;
      for (auto &&it : wadlist.itemsIdx()) {
        if (it.value().basename.strEquCI(basename)) {
          idx = it.index();
          break;
        }
      }
      if (idx < 0) {
        idx = wadlist.length();
        wadlist.alloc();
        dehmap.put(basename, idx);
      }
      WadInfo &wnfo = wadlist[idx];
      wnfo.basename = basename;
      wnfo.wadfidx = fnum;
      wnfo.dlump = -1;
      wnfo.isbex = false;
      wnfo.disabled = false;
      //GCon->Logf(NAME_Debug, "WAD: name=<%s>; deh=<%s>; fidx=%d", *wnfo.wadname, *wnfo.dehname, wnfo.wadfidx);
    }
  }

  VName name_dehsupp("dehsupp");

  // scan all files, put "dehacked" lumps, and "wadname.deh" lumps
  for (auto &&it : WadNSIterator(WADNS_Global)) {
    if (it.getName() == name_dehsupp) {
      for (int wx = 0; wx < wadlist.length(); ++wx) {
        if (wadlist[wx].wadfidx == it.getFile()) {
          wadlist[wx].disabled = true;
        }
      }
      continue;
    }
    // normal dehacked lump?
    if (it.getName() == NAME_dehacked) {
      //GCon->Logf(NAME_Debug, "dehacked lump: <%s>", *it.getFullName());
      if (cli_NoExternalDeh <= 0) {
        // remove this wad from the list for "wadname.deh" candidates
        VStr pkname = getFileWadName(it.getFile());
        if (pkname.length()) {
          VStr rname = pkname.stripExtension();
          auto dl = dehmap.get(rname);
          if (dl) {
            //GCon->Logf(NAME_Debug, "removed wad <%s> from named deh search list", *wadlist[*dl].wadname);
            WadInfo &wi = wadlist[*dl];
            // find and remove it from registered lumps list
            // this is required, because supporting .deh lumps are found first (due to how archive mounting works)
            if (wi.dlump >= 0) {
              for (int f = 0; f < lumplist.length(); ++f) {
                if (lumplist[f] == wi.dlump) {
                  GCon->Logf(NAME_Init, "Dropping standalone '%s' for '%s.wad' (replaced with '%s')", *W_FullLumpName(wi.dlump), *wi.basename, *W_FullLumpName(it.lump));
                  lumplist.removeAt(f);
                  --f;
                }
              }
            }
            wi.wadfidx = -1;
            dehmap.del(rname);
          }
        }
      }
      lumplist.append(it.lump);
      continue;
    }
    // "wadname.deh"?
    if (cli_NoExternalDeh <= 0) {
      VStr rname = it.getRealName();
      if (rname.endsWithCI(".deh") || rname.endsWithCI(".bex")) {
        bool isbex = rname.endsWithCI(".bex");
        rname = rname.extractFileName().stripExtension();
        auto dl = dehmap.get(rname);
        if (dl) {
          // use the fact that wads from zips/pk3s are mounted after the respective zip/pk3
          // this way, it is safe to add this dehacked file
          WadInfo &wi = wadlist[*dl];
          if (!isbex && wi.isbex) continue; // prefer ".bex" file
          if (wi.wadfidx > it.getFile() && wi.dlump < it.lump) {
            //GCon->Logf(NAME_Debug, "found named deh lump <%s> for wad <%s>", *it.getFullName(), *wi.wadname);
            //GCon->Logf(NAME_Debug, "found named deh lump <%s> for wad <%s> (wi.wadfidx=%d; wi.dlump=%d; fidx=%d; lump=%d)", *it.getFullName(), *wi.wadname, wi.wadfidx, wi.dlump, it.getFile(), it.lump);
            GCon->Logf(NAME_Init, "Found named deh lump \"%s\" for wad \"%s.wad\".", *it.getFullName(), *wi.basename);
            if (wi.dlump >= 0) {
              for (auto &&lit : lumplist.itemsIdx()) {
                if (lit.value() == wi.dlump) {
                  GCon->Logf(NAME_Init, "  dropped old deh \"%s\"", *W_FullLumpName(lit.value()));
                  lumplist.removeAt(lit.index());
                  break;
                }
              }
            }
            lumplist.append(it.lump);
            wi.dlump = it.lump;
            wi.isbex = isbex;
          } else {
            //GCon->Logf(NAME_Debug, "skipped named deh lump <%s> for wad <%s> (wi.wadfidx=%d; wi.dlump=%d; fidx=%d; lump=%d)", *it.getFullName(), *wi.wadname, wi.wadfidx, wi.dlump, it.getFile(), it.lump);
          }
        } else {
          //GCon->Logf(NAME_Debug, "skipped named deh lump <%s>", *it.getFullName());
        }
      }
    }
  }

  // now loop yet again, and remove all lumps from "disabled" archives
  for (int wx = 0; wx < wadlist.length(); ++wx) {
    if (wadlist[wx].disabled && wadlist[wx].dlump >= 0) {
      int llidx = 0;
      while (llidx < lumplist.length()) {
        if (lumplist[llidx] == wadlist[wx].dlump) {
          lumplist.removeAt(llidx);
        } else {
          ++llidx;
        }
      }
    }
  }
}


//==========================================================================
//
//  ProcessDehackedFiles
//
//==========================================================================
void ProcessDehackedFiles () {
  if (cli_NoAnyDehacked) return;

  TArray<int> dehlumps;
  FindDehackedLumps(dehlumps);
  if (cli_DehList.length() == 0 && dehlumps.length() == 0) return;

  LoadDehackedDefinitions();

  // parse dehacked patches
  for (auto &&dlump : dehlumps) {
    dehFileName = W_FullLumpName(dlump);
    GLog.Logf(NAME_Init, "Processing dehacked patch lump '%s'", *dehFileName);
    if (!game_options.warnDeh) {
      dehWarningsEnabled = !W_IsIWADLump(dlump); // do not print warnings for IWADs
    }
    LoadDehackedFile(W_CreateLumpReaderNum(dlump), dlump);
    dehWarningsEnabled = true;
  }

  for (auto &&dhs : cli_DehList) {
    GLog.Logf(NAME_Init, "Processing dehacked patch '%s'", *dhs);
    VStream *AStrm = FL_OpenSysFileRead(dhs);
    if (!AStrm) { GLog.Logf(NAME_Init, "No dehacked file '%s'", *dhs); continue; }
    dehFileName = dhs;
    LoadDehackedFile(AStrm, -1);
  }

  dehFileName.clear();

  for (int i = 0; i < EntClasses.length(); ++i) {
    // set all classes to use old style pickup handling
    if (EntClassTouched[i]) {
      //GCon->Logf(NAME_Debug, "dehacked touched `%s`", EntClasses[i]->GetName());
      EntClasses[i]->SetFieldBool("bDehackedSpecial", true);
    }
  }
  GameInfoClass->SetFieldBool("bDehacked", true);

  // do string replacement
  GSoundManager->ReplaceSoundLumpNames(SfxNames);
  P_ReplaceMusicLumpNames(MusicNames);
  VClass::ReplaceSpriteNames(SpriteNames);

  for (auto &&rs : SpriteNames) {
    if (!rs.Replaced) continue;
    ReplacedSprites.put(VName(*rs.Old, VName::AddLower), true);
    ReplacedSprites.put(VName(*rs.New, VName::AddLower), true);
    SpriteReplacements.put(VName(*rs.Old, VName::AddLower), VName(*rs.New, VName::AddLower));
  }

  VClass::StaticReinitStatesLookup();

  // clean up
  dehStInfo.Clear();
  Sprites.Clear();
  EntClasses.Clear();
  WeaponClasses.Clear();
  AmmoClasses.Clear();
  DehStates.Clear();
  CodePtrStatesIdx.Clear();
  CodePtrs.Clear();
  Sounds.Clear();
  SfxNames.Clear();
  MusicNames.Clear();
  SpriteNames.Clear();
  delete EngStrings;
  EngStrings = nullptr;
}
