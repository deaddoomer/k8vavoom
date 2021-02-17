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
#include "gamedefs.h"
#include "server/sv_local.h"


static int cli_NoMapinfoPlrClasses = 0;
static int cli_MapperIsIdiot = 0;
int cli_NoZMapinfo = 0; // need to be extern for files.cpp

/*static*/ bool cliRegister_mapinfo_args =
  VParsedArgs::RegisterFlagSet("-nomapinfoplayerclasses", "ignore player classes from MAPINFO", &cli_NoMapinfoPlrClasses) &&
  VParsedArgs::RegisterFlagSet("-nozmapinfo", "do not load ZMAPINFO", &cli_NoZMapinfo) &&
  VParsedArgs::RegisterFlagSet("-mapper-is-idiot", "!", &cli_MapperIsIdiot);


static VCvarB gm_default_pain_limit("gm_default_pain_limit", true, "Limit Pain Elemental skull spawning by default?", CVAR_Archive);


// ////////////////////////////////////////////////////////////////////////// //
// switches to C mode
struct SCParseModeSaver {
  VScriptParser *sc;
  bool oldCMode;
  bool oldEscape;

  SCParseModeSaver (VScriptParser *asc) : sc(asc) {
    oldCMode = sc->IsCMode();
    oldEscape = sc->IsEscape();
    sc->SetCMode(true);
    sc->SetEscape(true);
  }

  ~SCParseModeSaver () {
    sc->SetCMode(oldCMode);
    sc->SetEscape(oldEscape);
  }

  SCParseModeSaver (const SCParseModeSaver &);
  void operator = (const SCParseModeSaver &) const;
};


struct FMapSongInfo {
  VName MapName;
  VName SongName;
};

struct ParTimeInfo {
  VName MapName;
  int par;
};


struct SpawnEdFixup {
  VStr ClassName;
  int num;
  int flags;
  int special;
  int args[5];
};


// ////////////////////////////////////////////////////////////////////////// //
struct MapInfoCommand {
  const char *cmd;
  void (*handler) (VScriptParser *sc, bool newFormat, VMapInfo *info, bool &HexenMode);
  MapInfoCommand *next;
};


static MapInfoCommand *mclist = nullptr;
static TMap<VStr, MapInfoCommand *> mcmap; // key is lowercase name
static bool hasCustomDamageFactors = false;

enum {
  WSK_WAS_SKY1 = 1u<<0,
  WSK_WAS_SKY2 = 1u<<1,
};
static unsigned wasSky1Sky2 = 0u; // bit0: was sky1; bit1: was sky2


#define MAPINFOCMD(name_)  \
class MapInfoCommandImpl##name_ { \
public: \
  /*static*/ MapInfoCommand mci; \
  MapInfoCommandImpl##name_ (const char *aname) { \
    mci.cmd = aname; \
    mci.handler = &Handler; \
    mci.next = nullptr; \
    if (!mclist) { \
      mclist = &mci; \
    } else { \
      MapInfoCommand *last = mclist; \
      while (last->next) last = last->next; \
      last->next = &mci; \
    } \
  } \
  static void Handler (VScriptParser *sc, bool newFormat, VMapInfo *info, bool &HexenMode); \
}; \
/*MapInfoCommand MapInfoCommandImpl##name_ mci;*/ \
MapInfoCommandImpl##name_ mpiprzintrnlz_mici_##name_(#name_); \
void MapInfoCommandImpl##name_::Handler (VScriptParser *sc, bool newFormat, VMapInfo *info, bool &HexenMode)


//==========================================================================
//
//  checkEndBracket
//
//==========================================================================
static bool checkEndBracket (VScriptParser *sc) {
  if (sc->Check("}")) return true;
  if (sc->AtEnd()) {
    sc->Message("Some mapper cannot into proper mapinfo (missing '}')");
    return true;
  }
  return false;
}


//==========================================================================
//
//  ExpectBool
//
//==========================================================================
static bool ExpectBool (const char *optname, VScriptParser *sc) {
  if (sc->Check("true") || sc->Check("on") || sc->Check("tan")) return true;
  if (sc->CheckNumber()) return !!sc->Number;
  sc->Error(va("boolean value expected for option '%s'", optname));
  return false;
}


// ////////////////////////////////////////////////////////////////////////// //
VName P_TranslateMap (int map);
static void ParseMapInfo (VScriptParser *sc, int milumpnum);
static void ParseUMapinfo (VScriptParser *sc, int milumpnum);


// ////////////////////////////////////////////////////////////////////////// //
static char miWarningBuf[16384];

VVA_OKUNUSED __attribute__((format(printf, 2, 3))) static void miWarning (VScriptParser *sc, const char *fmt, ...) {
  va_list argptr;
  static char miWarningBuf[16384];
  va_start(argptr, fmt);
  vsnprintf(miWarningBuf, sizeof(miWarningBuf), fmt, argptr);
  va_end(argptr);
  if (sc) {
    GCon->Logf(NAME_Warning, "MAPINFO:%s: %s", *sc->GetLoc().toStringNoCol(), miWarningBuf);
  } else {
    GCon->Logf(NAME_Warning, "MAPINFO: %s", miWarningBuf);
  }
}


VVA_OKUNUSED __attribute__((format(printf, 2, 3))) static void miWarning (const TLocation &loc, const char *fmt, ...) {
  va_list argptr;
  va_start(argptr, fmt);
  vsnprintf(miWarningBuf, sizeof(miWarningBuf), fmt, argptr);
  va_end(argptr);
  GCon->Logf(NAME_Warning, "MAPINFO:%s: %s", *loc.toStringNoCol(), miWarningBuf);
}


// ////////////////////////////////////////////////////////////////////////// //
static VMapInfo DefaultMap;
static TArray<VMapInfo> MapInfo;
static TArray<FMapSongInfo> MapSongList;
static VClusterDef DefaultClusterDef;
static TArray<VClusterDef> ClusterDefs;
static TArray<VEpisodeDef> EpisodeDefs;
static TArray<VSkillDef> SkillDefs;

static bool mapinfoParsed = false;
static TArray<ParTimeInfo> partimes; // not a hashmap, so i can use `ICmp`

static TArray<VName> MapInfoPlayerClasses;

static TMap<int, SpawnEdFixup> SpawnNumFixups; // keyed by num
static TMap<int, SpawnEdFixup> DoomEdNumFixups; // keyed by num


//==========================================================================
//
//  VMapInfo::GetName
//
//==========================================================================
VStr VMapInfo::GetName () const {
  return (Flags&VLevelInfo::LIF_LookupName ? GLanguage[*Name] : Name);
}


//==========================================================================
//
//  VMapInfo::dump
//
//==========================================================================
void VMapInfo::dump (const char *msg) const {
  if (msg && msg[0]) GCon->Logf(NAME_Debug, "==== mapinfo: %s ===", msg); else GCon->Log(NAME_Debug, "==== mapinfo ===");
  GCon->Logf(NAME_Debug, "  LumpName: \"%s\"", *VStr(LumpName).quote());
  GCon->Logf(NAME_Debug, "  Name: \"%s\"", *Name.quote());
  GCon->Logf(NAME_Debug, "  LevelNum: %d", LevelNum);
  GCon->Logf(NAME_Debug, "  Cluster: %d", Cluster);
  GCon->Logf(NAME_Debug, "  WarpTrans: %d", WarpTrans);
  GCon->Logf(NAME_Debug, "  NextMap: \"%s\"", *VStr(NextMap).quote());
  GCon->Logf(NAME_Debug, "  SecretMap: \"%s\"", *VStr(SecretMap).quote());
  GCon->Logf(NAME_Debug, "  SongLump: \"%s\"", *VStr(SongLump).quote());
  GCon->Logf(NAME_Debug, "  Sky1Texture: %d", Sky1Texture);
  GCon->Logf(NAME_Debug, "  Sky2Texture: %d", Sky2Texture);
  GCon->Logf(NAME_Debug, "  Sky1ScrollDelta: %g", Sky1ScrollDelta);
  GCon->Logf(NAME_Debug, "  Sky2ScrollDelta: %g", Sky2ScrollDelta);
  GCon->Logf(NAME_Debug, "  SkyBox: \"%s\"", *VStr(SkyBox).quote());
  GCon->Logf(NAME_Debug, "  FadeTable: \"%s\"", *VStr(FadeTable).quote());
  GCon->Logf(NAME_Debug, "  Fade: 0x%08x", Fade);
  GCon->Logf(NAME_Debug, "  OutsideFog: 0x%08x", OutsideFog);
  GCon->Logf(NAME_Debug, "  Gravity: %g", Gravity);
  GCon->Logf(NAME_Debug, "  AirControl: %g", AirControl);
  GCon->Logf(NAME_Debug, "  Flags: 0x%08x", Flags);
  GCon->Logf(NAME_Debug, "  Flags2: 0x%08x", Flags2);
  GCon->Logf(NAME_Debug, "  EnterTitlePatch: \"%s\"", *VStr(EnterTitlePatch).quote());
  GCon->Logf(NAME_Debug, "  ExitTitlePatch: \"%s\"", *VStr(ExitTitlePatch).quote());
  GCon->Logf(NAME_Debug, "  ParTime: %d", ParTime);
  GCon->Logf(NAME_Debug, "  SuckTime: %d", SuckTime);
  GCon->Logf(NAME_Debug, "  HorizWallShade: %d", HorizWallShade);
  GCon->Logf(NAME_Debug, "  VertWallShade: %d", VertWallShade);
  GCon->Logf(NAME_Debug, "  Infighting: %d", Infighting);
  GCon->Logf(NAME_Debug, "  RedirectType: \"%s\"", *VStr(RedirectType).quote());
  GCon->Logf(NAME_Debug, "  RedirectMap: \"%s\"", *VStr(RedirectMap).quote());
  GCon->Logf(NAME_Debug, "  ExitPic: \"%s\"", *VStr(ExitPic).quote());
  GCon->Logf(NAME_Debug, "  EnterPic: \"%s\"", *VStr(EnterPic).quote());
  GCon->Logf(NAME_Debug, "  InterMusic: \"%s\"", *VStr(InterMusic).quote());
  GCon->Logf(NAME_Debug, "  ExitText: \"%s\"", *VStr(ExitText).quote());
  GCon->Logf(NAME_Debug, "  SecretExitText: \"%s\"", *VStr(SecretExitText).quote());
  GCon->Logf(NAME_Debug, "  InterBackdrop: \"%s\"", *VStr(InterBackdrop).quote());
  for (auto &&sac : SpecialActions) {
    GCon->Log(NAME_Debug, "  --- special action ---");
    GCon->Logf(NAME_Debug, "    TypeName: \"%s\"", *VStr(sac.TypeName).quote());
    GCon->Logf(NAME_Debug, "    Special: %d (%d,%d,%d,%d,%d)", sac.Special, sac.Args[0], sac.Args[1], sac.Args[2], sac.Args[3], sac.Args[4]);
  }
}


//==========================================================================
//
//  P_SetupMapinfoPlayerClasses
//
//==========================================================================
void P_SetupMapinfoPlayerClasses () {
  VClass *PPClass = VClass::FindClass("PlayerPawn");
  if (!PPClass) {
    GCon->Logf(NAME_Warning, "Can't find PlayerPawn class");
    return;
  }
  if (!flForcePlayerClass.isEmpty()) {
    VClass *Class = VClass::FindClassNoCase(*flForcePlayerClass);
    if (!Class) {
      GCon->Logf(NAME_Warning, "Mode-ForcePlayerClass: No such class `%s`", *flForcePlayerClass);
    } else if (!Class->IsChildOf(PPClass)) {
      GCon->Logf(NAME_Warning, "Mode-ForcePlayerClass: '%s' is not a player pawn class", *flForcePlayerClass);
    } else {
      GGameInfo->PlayerClasses.Clear();
      GGameInfo->PlayerClasses.Append(Class);
      GCon->Logf(NAME_Init, "mode-forced player class '%s'", *flForcePlayerClass);
      return;
    }
  }
  if (cli_NoMapinfoPlrClasses > 0) return;
  if (MapInfoPlayerClasses.length() == 0) return;
  GCon->Logf(NAME_Init, "setting up %d player class%s from mapinfo", MapInfoPlayerClasses.length(), (MapInfoPlayerClasses.length() != 1 ? "es" : ""));
  GGameInfo->PlayerClasses.Clear();
  for (int f = 0; f < MapInfoPlayerClasses.length(); ++f) {
    VClass *Class = VClass::FindClassNoCase(*MapInfoPlayerClasses[f]);
    if (!Class) {
      GCon->Logf(NAME_Warning, "No player class '%s'", *MapInfoPlayerClasses[f]);
      continue;
    }
    if (!Class->IsChildOf(PPClass)) {
      GCon->Logf(NAME_Warning, "'%s' is not a player pawn class", *MapInfoPlayerClasses[f]);
      continue;
    }
    GGameInfo->PlayerClasses.Append(Class);
  }
  if (GGameInfo->PlayerClasses.length() == 0) Sys_Error("no valid classes found in MAPINFO playerclass replacement");
}


//==========================================================================
//
//  appendNumFixup
//
//==========================================================================
static void appendNumFixup (TMap<int, SpawnEdFixup> &arr, VStr className, int num, int flags=0, int special=0, int arg1=0, int arg2=0, int arg3=0, int arg4=0, int arg5=0) {
  SpawnEdFixup *fxp = arr.find(num);
  if (fxp) {
    fxp->ClassName = className;
    fxp->flags = flags;
    fxp->special = special;
    fxp->args[0] = arg1;
    fxp->args[1] = arg2;
    fxp->args[2] = arg3;
    fxp->args[3] = arg4;
    fxp->args[4] = arg5;
    return;
  }
  SpawnEdFixup fx;
  fx.num = num;
  fx.flags = flags;
  fx.special = special;
  fx.args[0] = arg1;
  fx.args[1] = arg2;
  fx.args[2] = arg3;
  fx.args[3] = arg4;
  fx.args[4] = arg5;
  fx.ClassName = className;
  arr.put(num, fx);
}


//==========================================================================
//
//  processNumFixups
//
//==========================================================================
static void processNumFixups (const char *errname, bool ismobj, TMap<int, SpawnEdFixup> &fixups) {
  //GCon->Logf(NAME_Debug, "fixing '%s' (%d)", errname, fixups.count());
#if 0
  int f = 0;
  while (f < list.length()) {
    mobjinfo_t &nfo = list[f];
    SpawnEdFixup *fxp = fixups.find(nfo.DoomEdNum);
    if (fxp) {
      SpawnEdFixup fix = *fxp;
      VStr cname = fxp->ClassName;
      //GCon->Logf(NAME_Debug, "    MAPINFO: class '%s' for %s got doomed num %d (got %d)", *cname, errname, fxp->num, nfo.DoomEdNum);
      fixups.del(nfo.DoomEdNum);
      /*
      if (cname.length() == 0 || cname.ICmp("none") == 0) {
        // remove it
        list.removeAt(f);
        continue;
      }
      */
      // set it
      VClass *cls;
      if (cname.length() == 0 || cname.ICmp("none") == 0) {
        cls = nullptr;
      } else {
        cls = VClass::FindClassNoCase(*cname);
        if (!cls) GCon->Logf(NAME_Warning, "MAPINFO: class '%s' for %s %d not found", *cname, errname, nfo.DoomEdNum);
      }
      nfo.Class = cls;
      nfo.GameFilter = GAME_Any;
      nfo.flags = fix.flags;
      nfo.special = fix.special;
      nfo.args[0] = fix.args[0];
      nfo.args[1] = fix.args[1];
      nfo.args[2] = fix.args[2];
      nfo.args[3] = fix.args[3];
      nfo.args[4] = fix.args[4];
    }
    ++f;
  }
#endif
  //GCon->Logf(NAME_Debug, "  appending '%s' (%d)", errname, fixups.count());
  // append new
  for (auto it = fixups.first(); it; ++it) {
    SpawnEdFixup *fxp = &it.getValue();
    if (fxp->num <= 0) continue;
    VStr cname = fxp->ClassName;
    //GCon->Logf(NAME_Debug, "    MAPINFO0: class '%s' for %s got doomed num %d", *cname, errname, fxp->num);
    // set it
    VClass *cls;
    if (cname.length() == 0 || cname.ICmp("none") == 0) {
      cls = nullptr;
    } else {
      cls = VClass::FindClassNoCase(*cname);
      if (!cls) GCon->Logf(NAME_Warning, "MAPINFO: class '%s' for %s %d not found", *cname, errname, fxp->num);
    }
    //GCon->Logf(NAME_Debug, "    MAPINFO1: class '%s' for %s got doomed num %d", *cname, errname, fxp->num);
    if (!cls) {
      if (ismobj) {
        VClass::RemoveMObjId(fxp->num, GGameInfo->GameFilterFlag);
      } else {
        VClass::RemoveScriptId(fxp->num, GGameInfo->GameFilterFlag);
      }
    } else {
      mobjinfo_t *nfo = (ismobj ? VClass::AllocMObjId(fxp->num, GGameInfo->GameFilterFlag, cls) : VClass::AllocScriptId(fxp->num, GGameInfo->GameFilterFlag, cls));
      if (nfo) {
        nfo->Class = cls;
        nfo->flags = fxp->flags;
        nfo->special = fxp->special;
        nfo->args[0] = fxp->args[0];
        nfo->args[1] = fxp->args[1];
        nfo->args[2] = fxp->args[2];
        nfo->args[3] = fxp->args[3];
        nfo->args[4] = fxp->args[4];
      }
    }
  }
  fixups.clear();
}


//==========================================================================
//
//  findSavedPar
//
//  returns -1 if not found
//
//==========================================================================
static int findSavedPar (VName map) {
  if (map == NAME_None) return -1;
  for (int f = partimes.length()-1; f >= 0; --f) {
    if (VStr::ICmp(*partimes[f].MapName, *map) == 0) return partimes[f].par;
  }
  return -1;
}


//==========================================================================
//
//  loadSkyTexture
//
//==========================================================================
static int loadSkyTexture (VScriptParser *sc, VName name, bool silent=false) {
  static TMapNC<VName, int> forceList;

  if (name == NAME_None) return GTextureManager.DefaultTexture;

  VName loname = VName(*name, VName::AddLower);
  auto tidp = forceList.find(loname);
  if (tidp) return *tidp;

  //int Tex = GTextureManager.NumForName(sc->Name8, TEXTYPE_Wall, false);
  //info->Sky1Texture = GTextureManager.NumForName(sc->Name8, TEXTYPE_Wall, false);
  int Tex = GTextureManager.CheckNumForName(name, TEXTYPE_SkyMap, true);
  if (Tex > 0) return Tex;

  Tex = GTextureManager.CheckNumForName(name, TEXTYPE_Wall, true);
  if (Tex >= 0) {
    forceList.put(loname, Tex);
    return Tex;
  }

  Tex = GTextureManager.CheckNumForName(name, TEXTYPE_WallPatch, false);
  if (Tex < 0) {
    int LumpNum = W_CheckNumForTextureFileName(*name);
    if (LumpNum >= 0) {
      Tex = GTextureManager.FindTextureByLumpNum(LumpNum);
      if (Tex < 0) {
        VTexture *T = VTexture::CreateTexture(TEXTYPE_WallPatch, LumpNum);
        if (!T) T = VTexture::CreateTexture(TEXTYPE_Any, LumpNum);
        if (T) {
          Tex = GTextureManager.AddTexture(T);
          T->Name = NAME_None;
        }
      }
    } else {
      LumpNum = W_CheckNumForName(name, WADNS_Patches);
      if (LumpNum < 0) LumpNum = W_CheckNumForTextureFileName(*name);
      if (LumpNum >= 0) {
        Tex = GTextureManager.AddTexture(VTexture::CreateTexture(TEXTYPE_WallPatch, LumpNum));
      } else {
        // DooM:Complete has some patches in "sprites/"
        LumpNum = W_CheckNumForName(name, WADNS_Sprites);
        if (LumpNum >= 0) Tex = GTextureManager.AddTexture(VTexture::CreateTexture(TEXTYPE_Any, LumpNum));
      }
    }
  }

  if (Tex < 0) Tex = GTextureManager.CheckNumForName(name, TEXTYPE_Any, false);

  if (Tex < 0) Tex = GTextureManager.AddPatch(name, TEXTYPE_WallPatch, true);

  if (Tex < 0) {
    if (silent) return -1;
    miWarning(sc, "sky '%s' not found; replaced with 'sky1'", *name);
    Tex = GTextureManager.CheckNumForName("sky1", TEXTYPE_SkyMap, true);
    if (Tex < 0) Tex = GTextureManager.CheckNumForName("sky1", TEXTYPE_Wall, true);
    forceList.put(loname, Tex);
    return Tex;
    //return GTextureManager.DefaultTexture;
  }

  forceList.put(loname, Tex);
  miWarning(sc, "force-loaded sky '%s'", *name);
  return Tex;
}


//==========================================================================
//
//  LoadMapInfoLump
//
//==========================================================================
static void LoadMapInfoLump (int Lump, bool doFixups=true) {
  GCon->Logf(NAME_Init, "mapinfo file: '%s'", *W_FullLumpName(Lump));
  ParseMapInfo(new VScriptParser(W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump)), Lump);
  if (doFixups) {
    processNumFixups("DoomEdNum", true, DoomEdNumFixups);
    processNumFixups("SpawnNum", false, SpawnNumFixups);
  }
}


//==========================================================================
//
//  LoadAllMapInfoLumpsInFile
//
//  do this scanning fuckery, because some idiotic tools
//  loves duplicate lumps
//
//==========================================================================
static void LoadAllMapInfoLumpsInFile (int miLump, int zmiLump, int vmiLump) {
  if (miLump < 0 && zmiLump < 0) return;
  VName milname;
  if (vmiLump >= 0) {
    milname = VName("vmapinfo", VName::Add);
    zmiLump = vmiLump;
  } else if (zmiLump >= 0) {
    milname = VName("zmapinfo", VName::Add);
  } else {
    vassert(miLump >= 0);
    zmiLump = miLump;
    milname = NAME_mapinfo;
  }
  vassert(zmiLump >= 0);
  int currFile = W_LumpFile(zmiLump);
  bool wasLoaded = false;
  for (; zmiLump >= 0; zmiLump = W_IterateNS(zmiLump, WADNS_Global)) {
    if (W_LumpFile(zmiLump) != currFile) break;
    if (W_LumpName(zmiLump) == milname) {
      wasLoaded = true;
      LoadMapInfoLump(zmiLump, false); // no fixups yet
    }
  }
  // do fixups if somethig was loaded
  if (wasLoaded) {
    processNumFixups("DoomEdNum", true, DoomEdNumFixups);
    processNumFixups("SpawnNum", false, SpawnNumFixups);
  }
}


//==========================================================================
//
//  LoadUmapinfoLump
//
//==========================================================================
static void LoadUmapinfoLump (int lump) {
  if (lump < 0) return;
  GCon->Logf(NAME_Init, "umapinfo file: '%s'", *W_FullLumpName(lump));
  ParseUMapinfo(new VScriptParser(W_FullLumpName(lump), W_CreateLumpReaderNum(lump)), lump);
}


//==========================================================================
//
//  InitMapInfo
//
//==========================================================================
void InitMapInfo () {
  // use "zmapinfo" if it is present?
  bool zmapinfoAllowed = (cli_NoZMapinfo >= 0);
  if (!zmapinfoAllowed) GCon->Logf(NAME_Init, "zmapinfo parsing disabled by user");

  int lastMapinfoFile = -1; // haven't seen yet
  int lastMapinfoLump = -1; // haven't seen yet
  int lastVMapinfoLump = -1; // haven't seen yet
  int lastZMapinfoLump = -1; // haven't seen yet
  int lastUmapinfoLump = -1; // haven't seen yet
  bool doSkipFile = false;
  VName nameVMI = VName("vmapinfo", VName::Add);
  VName nameZMI = VName("zmapinfo", VName::Add);
  VName nameUMI = VName("umapinfo", VName::Add);

  TArray<int> fileKeyconfLump;

  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    int currFile = W_LumpFile(Lump);
    if (doSkipFile) {
      if (currFile == lastMapinfoFile) continue;
      doSkipFile = false; // just in case
    }
    // if we hit another file, load last seen [z]mapinfo lump
    if (currFile != lastMapinfoFile) {
      //GCon->Logf(NAME_Debug, "*** new archive at '%s' ***", *W_FullLumpName(Lump));
      LoadAllMapInfoLumpsInFile(lastMapinfoLump, lastZMapinfoLump, lastVMapinfoLump);
      if (lastMapinfoLump < 0 && lastZMapinfoLump < 0 && lastVMapinfoLump < 0 && lastUmapinfoLump >= 0) {
        LoadUmapinfoLump(lastUmapinfoLump);
      }
      // reset/update remembered lump indicies
      lastMapinfoFile = currFile;
      lastMapinfoLump = lastVMapinfoLump = lastZMapinfoLump = lastUmapinfoLump = -1; // haven't seen yet
      // load keyconfs from previous files
      for (auto &&klmp : fileKeyconfLump) VCommand::LoadKeyconfLump(klmp);
      fileKeyconfLump.resetNoDtor();
      // skip zip files
      doSkipFile = !W_IsWadPK3File(currFile);
      if (doSkipFile) continue;
    }
    // remember keyconf
    if (W_LumpName(Lump) == NAME_keyconf) fileKeyconfLump.append(Lump);
    // remember last seen [z]mapinfo lump
    if (lastMapinfoLump < 0 && W_LumpName(Lump) == NAME_mapinfo) lastMapinfoLump = Lump;
    if (zmapinfoAllowed && lastZMapinfoLump < 0 && W_LumpName(Lump) == nameZMI) lastZMapinfoLump = Lump;
    if (lastVMapinfoLump < 0 && W_LumpName(Lump) == nameVMI) lastVMapinfoLump = Lump;
    if (lastUmapinfoLump < 0 && W_LumpName(Lump) == nameUMI) lastUmapinfoLump = Lump;
  }

  // load last seen mapinfos
  LoadAllMapInfoLumpsInFile(lastMapinfoLump, lastZMapinfoLump, lastVMapinfoLump);
  if (lastMapinfoLump < 0 && lastZMapinfoLump < 0 && lastVMapinfoLump < 0 && lastUmapinfoLump >= 0) {
    LoadUmapinfoLump(lastUmapinfoLump);
  }

  mapinfoParsed = true;

  // load latest keyconfs
  for (auto &&klmp : fileKeyconfLump) VCommand::LoadKeyconfLump(klmp);
  fileKeyconfLump.resetNoDtor();

  for (int i = 0; i < MapInfo.Num(); ++i) {
    if (VStr(MapInfo[i].NextMap).StartsWith("&wt@")) {
      MapInfo[i].NextMap = P_TranslateMap(VStr::atoi(*MapInfo[i].NextMap+4));
    }
    if (VStr(MapInfo[i].SecretMap).StartsWith("&wt@")) {
      MapInfo[i].SecretMap = P_TranslateMap(VStr::atoi(*MapInfo[i].SecretMap+4));
    }
  }

  for (int i = 0; i < EpisodeDefs.Num(); ++i) {
    if (VStr(EpisodeDefs[i].Name).StartsWith("&wt@")) {
      EpisodeDefs[i].Name = P_TranslateMap(VStr::atoi(*EpisodeDefs[i].Name+4));
    }
    if (VStr(EpisodeDefs[i].TeaserName).StartsWith("&wt@")) {
      EpisodeDefs[i].TeaserName = P_TranslateMap(VStr::atoi(*EpisodeDefs[i].TeaserName+4));
    }
  }

  // set up default map info returned for maps that have not defined in MAPINFO
  memset((void *)&DefaultMap, 0, sizeof(DefaultMap));
  DefaultMap.Name = "Unnamed";
  DefaultMap.Sky1Texture = loadSkyTexture(nullptr, "sky1"); //GTextureManager.CheckNumForName("sky1", TEXTYPE_Wall, true, true);
  DefaultMap.Sky2Texture = DefaultMap.Sky1Texture;
  DefaultMap.FadeTable = NAME_colormap;
  DefaultMap.HorizWallShade = -8;
  DefaultMap.VertWallShade = 8;
  //GCon->Logf(NAME_Debug, "*** DEFAULT MAP: Sky1Texture=%d", DefaultMap.Sky1Texture);

  // we don't need it anymore
  mcmap.clear();

  if (hasCustomDamageFactors) SV_ReplaceCustomDamageFactors();
}


//==========================================================================
//
//  SetMapDefaults
//
//==========================================================================
static void SetMapDefaults (VMapInfo &Info) {
  Info.LumpName = NAME_None;
  Info.Name = VStr();
  Info.LevelNum = 0;
  Info.Cluster = 0;
  Info.WarpTrans = 0;
  Info.NextMap = NAME_None;
  Info.SecretMap = NAME_None;
  Info.SongLump = NAME_None;
  //Info.Sky1Texture = GTextureManager.DefaultTexture;
  //Info.Sky2Texture = GTextureManager.DefaultTexture;
  Info.Sky1Texture = loadSkyTexture(nullptr, "sky1"); //GTextureManager.CheckNumForName("sky1", TEXTYPE_Wall, true, true);
  Info.Sky2Texture = Info.Sky1Texture;
  //Info.Sky2Texture = GTextureManager.DefaultTexture;
  Info.Sky1ScrollDelta = 0;
  Info.Sky2ScrollDelta = 0;
  Info.SkyBox = NAME_None;
  Info.FadeTable = NAME_colormap;
  Info.Fade = 0;
  Info.OutsideFog = 0;
  Info.Gravity = 0;
  Info.AirControl = 0;
  Info.Flags = 0;
  Info.Flags2 = (gm_default_pain_limit ? VLevelInfo::LIF2_CompatLimitPain : 0);
  Info.EnterTitlePatch = NAME_None;
  Info.ExitTitlePatch = NAME_None;
  Info.ParTime = 0;
  Info.SuckTime = 0;
  Info.HorizWallShade = -8;
  Info.VertWallShade = 8;
  Info.Infighting = 0;
  Info.SpecialActions.Clear();
  Info.RedirectType = NAME_None;
  Info.RedirectMap = NAME_None;
  Info.ExitPic = NAME_None;
  Info.EnterPic = NAME_None;
  Info.InterMusic = NAME_None;
  Info.ExitText = VStr::EmptyString;
  Info.SecretExitText = VStr::EmptyString;
  Info.InterBackdrop = NAME_None;

  if (GGameInfo->Flags&VGameInfo::GIF_DefaultLaxMonsterActivation) {
    Info.Flags2 |= VLevelInfo::LIF2_LaxMonsterActivation;
  }
}


//==========================================================================
//
//  ParseNextMapName
//
//==========================================================================
static VName ParseNextMapName (VScriptParser *sc, bool HexenMode) {
  if (sc->CheckNumber()) {
    if (HexenMode) return va("&wt@%02d", sc->Number);
    return va("map%02d", sc->Number);
  }
  if (sc->Check("endbunny")) return "EndGameBunny";
  if (sc->Check("endcast")) return "EndGameCast";
  if (sc->Check("enddemon")) return "EndGameDemon";
  if (sc->Check("endchess")) return "EndGameChess";
  if (sc->Check("endunderwater")) return "EndGameUnderwater";
  if (sc->Check("endbuystrife")) return "EndGameBuyStrife";
  if (sc->Check("endpic")) {
    sc->Check(",");
    sc->ExpectName8();
    return va("EndGameCustomPic%s", *sc->Name8);
  }
  sc->ExpectString();
  if (sc->String.ToLower().StartsWith("endgame")) {
    switch (sc->String[7]) {
      case '1': return "EndGamePic1";
      case '2': return "EndGamePic2";
      case '3': return "EndGameBunny";
      case 'c': case 'C': return "EndGameCast";
      case 'w': case 'W': return "EndGameUnderwater";
      case 's': case 'S': return "EndGameStrife";
      default: return "EndGamePic3";
    }
  }
  return VName(*sc->String, VName::AddLower8);
}


//==========================================================================
//
//  DoCompatFlag
//
//==========================================================================
static void DoCompatFlag (VScriptParser *sc, VMapInfo *info, int Flag, bool newFormat) {
  int Set = 1;
  if (newFormat) {
    sc->Check("=");
    if (sc->CheckNumber()) Set = sc->Number;
  } else if (sc->CheckNumber()) {
    Set = sc->Number;
  }
  if (Flag) {
    if (Set) {
      info->Flags2 |= Flag;
    } else {
      info->Flags2 &= ~Flag;
    }
  }
}


//==========================================================================
//
//  skipUnimplementedCommand
//
//==========================================================================
static void skipUnimplementedCommand (VScriptParser *sc, bool wantArg) {
  VStr cmd = sc->String;
  if (sc->Check("=")) {
    miWarning(sc, "Unimplemented command '%s'", *cmd);
    sc->ExpectString();
    while (sc->Check(",")) {
      if (sc->Check("}")) { sc->UnGet(); break; }
      if (sc->AtEnd()) break;
      sc->ExpectString();
    }
  } else if (wantArg) {
    miWarning(sc, "Unimplemented old command '%s'", *cmd);
    sc->ExpectString();
  } else {
    miWarning(sc, "Unimplemented flag '%s'", *cmd);
  }
}


// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(levelnum) {
  if (newFormat) sc->Expect("=");
  sc->ExpectNumber();
  info->LevelNum = sc->Number;
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(cluster) {
  if (newFormat) sc->Expect("=");
  sc->ExpectNumber();
  info->Cluster = sc->Number;
  if (P_GetClusterDef(info->Cluster) == &DefaultClusterDef) {
    // add empty cluster def if it doesn't exist yet
    VClusterDef &C = ClusterDefs.Alloc();
    C.Cluster = info->Cluster;
    C.Flags = 0;
    C.EnterText = VStr();
    C.ExitText = VStr();
    C.Flat = NAME_None;
    C.Music = NAME_None;
    if (HexenMode) C.Flags |= CLUSTERF_Hub;
  }
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(warptrans) {
  if (newFormat) sc->Expect("=");
  sc->ExpectNumber();
  info->WarpTrans = sc->Number;
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(next) {
  if (newFormat) sc->Expect("=");
  info->NextMap = ParseNextMapName(sc, HexenMode);
  // hack for "complete"
  if (sc->Check("{")) {
    info->NextMap = "endgamec";
    sc->SkipBracketed(true); // bracket eaten
  } else if (newFormat && sc->Check(",")) {
    sc->ExpectString();
    // check for more commas?
  }
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(secret) {
  if (newFormat) sc->Expect("=");
  info->SecretMap = ParseNextMapName(sc, HexenMode);
}

MAPINFOCMD(secretnext) {
  if (newFormat) sc->Expect("=");
  info->SecretMap = ParseNextMapName(sc, HexenMode);
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(sky1) {
  wasSky1Sky2 |= WSK_WAS_SKY1;
  auto loc = sc->GetLoc();
  if (newFormat) sc->Expect("=");
  sc->ExpectName();
  //info->Sky1Texture = GTextureManager.NumForName(sc->Name, TEXTYPE_Wall, false);
  VName skbname = R_HasNamedSkybox(sc->String);
  if (skbname != NAME_None) {
    //k8: ok, this may be done to support sourceports that cannot into skyboxes
    miWarning(loc, "sky1 '%s' is actually a skybox (this is mostly harmless)", *sc->String);
    info->SkyBox = skbname;
    info->Sky1Texture = GTextureManager.DefaultTexture;
    info->Sky2Texture = GTextureManager.DefaultTexture;
    info->Sky1ScrollDelta = 0;
    info->Sky2ScrollDelta = 0;
    //GCon->Logf(NAME_Init, "using gz skybox '%s'", *skbname);
    if (!sc->IsAtEol()) {
      sc->Check(",");
      sc->ExpectFloatWithSign();
      if (HexenMode) sc->Float /= 256.0f;
      if (sc->Float != 0) miWarning(loc, "ignoring sky scroll for skybox '%s' (this is mostly harmless)", *skbname);
    }
  } else {
    info->SkyBox = NAME_None;
    info->Sky1Texture = loadSkyTexture(sc, sc->Name);
    info->Sky1ScrollDelta = 0;
    if (newFormat) {
      if (!sc->IsAtEol()) {
        sc->Check(",");
        sc->ExpectFloatWithSign();
        if (HexenMode) sc->Float /= 256.0f;
        info->Sky1ScrollDelta = sc->Float*35.0f;
      }
    } else {
      if (!sc->IsAtEol()) {
        sc->Check(",");
        sc->ExpectFloatWithSign();
        if (HexenMode) sc->Float /= 256.0f;
        info->Sky1ScrollDelta = sc->Float*35.0f;
      }
    }
  }
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(sky2) {
  wasSky1Sky2 |= WSK_WAS_SKY2;
  if (newFormat) sc->Expect("=");
  sc->ExpectName();
  //info->Sky2Texture = GTextureManager.NumForName(sc->Name8, TEXTYPE_Wall, false);
  //info->SkyBox = NAME_None; //k8:required or not???
  info->Sky2Texture = loadSkyTexture(sc, sc->Name);
  info->Sky2ScrollDelta = 0;
  if (newFormat) {
    if (!sc->IsAtEol()) {
      sc->Check(",");
      sc->ExpectFloatWithSign();
      if (HexenMode) sc->Float /= 256.0f;
      info->Sky1ScrollDelta = sc->Float*35.0f;
    }
  } else {
    if (!sc->IsAtEol()) {
      sc->Check(",");
      sc->ExpectFloatWithSign();
      if (HexenMode) sc->Float /= 256.0f;
      info->Sky2ScrollDelta = sc->Float*35.0f;
    }
  }
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(skybox) {
  auto loc = sc->GetLoc();
  if (newFormat) sc->Expect("=");
  sc->ExpectString();
  info->Sky1ScrollDelta = 0;
  info->Sky2ScrollDelta = 0;
  VName skbname = R_HasNamedSkybox(sc->String);
  if (skbname != NAME_None) {
    info->SkyBox = skbname;
    info->Sky1Texture = GTextureManager.DefaultTexture;
    info->Sky2Texture = GTextureManager.DefaultTexture;
  } else {
    if (cli_MapperIsIdiot > 0) {
      miWarning(loc, "skybox '%s' not found (mapper is idiot)!", *sc->String);
    } else {
      sc->Error(va("skybox '%s' not found (this mapinfo is broken)", *sc->String));
    }
    info->SkyBox = NAME_None;
    info->Sky1Texture = loadSkyTexture(sc, VName(*sc->String, VName::AddLower8));
    info->Sky2Texture = info->Sky1Texture;
  }
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(skyrotate) {
  miWarning(sc, "\"skyrotate\" command is not supported yet");
  if (newFormat) sc->Expect("=");
  sc->ExpectFloatWithSign();
  if (sc->Check(",")) sc->ExpectFloatWithSign();
  if (sc->Check(",")) sc->ExpectFloatWithSign();
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(doublesky) { info->Flags |= VLevelInfo::LIF_DoubleSky; }
MAPINFOCMD(lightning) { info->Flags |= VLevelInfo::LIF_Lightning; }
MAPINFOCMD(forcenoskystretch) { info->Flags |= VLevelInfo::LIF_ForceNoSkyStretch; }
MAPINFOCMD(skystretch) { info->Flags &= ~VLevelInfo::LIF_ForceNoSkyStretch; }
MAPINFOCMD(map07special) { info->Flags |= VLevelInfo::LIF_Map07Special; }
MAPINFOCMD(baronspecial) { info->Flags |= VLevelInfo::LIF_BaronSpecial; }
MAPINFOCMD(cyberdemonspecial) { info->Flags |= VLevelInfo::LIF_CyberDemonSpecial; }
MAPINFOCMD(spidermastermindspecial) { info->Flags |= VLevelInfo::LIF_SpiderMastermindSpecial; }
MAPINFOCMD(minotaurspecial) { info->Flags |= VLevelInfo::LIF_MinotaurSpecial; }
MAPINFOCMD(dsparilspecial) { info->Flags |= VLevelInfo::LIF_DSparilSpecial; }
MAPINFOCMD(ironlichspecial) { info->Flags |= VLevelInfo::LIF_IronLichSpecial; }
MAPINFOCMD(specialaction_exitlevel) { info->Flags &= ~(VLevelInfo::LIF_SpecialActionOpenDoor|VLevelInfo::LIF_SpecialActionLowerFloor); }
MAPINFOCMD(specialaction_opendoor) { info->Flags &= ~VLevelInfo::LIF_SpecialActionLowerFloor; info->Flags |= VLevelInfo::LIF_SpecialActionOpenDoor; }
MAPINFOCMD(specialaction_lowerfloor) { info->Flags |= VLevelInfo::LIF_SpecialActionLowerFloor; info->Flags &= ~VLevelInfo::LIF_SpecialActionOpenDoor; }
MAPINFOCMD(specialaction_killmonsters) { info->Flags |= VLevelInfo::LIF_SpecialActionKillMonsters; }
MAPINFOCMD(intermission) { info->Flags &= ~VLevelInfo::LIF_NoIntermission; }
MAPINFOCMD(nointermission) { info->Flags |= VLevelInfo::LIF_NoIntermission; }
MAPINFOCMD(nosoundclipping) { /* ignored */ }
MAPINFOCMD(noinventorybar) { /* ignored */ }
MAPINFOCMD(allowmonstertelefrags) { info->Flags |= VLevelInfo::LIF_AllowMonsterTelefrags; }
MAPINFOCMD(noallies) { info->Flags |= VLevelInfo::LIF_NoAllies; }
MAPINFOCMD(fallingdamage) { info->Flags &= ~(VLevelInfo::LIF_OldFallingDamage|VLevelInfo::LIF_StrifeFallingDamage); info->Flags |= VLevelInfo::LIF_FallingDamage; }
MAPINFOCMD(oldfallingdamage) { info->Flags &= ~(VLevelInfo::LIF_FallingDamage|VLevelInfo::LIF_StrifeFallingDamage); info->Flags |= VLevelInfo::LIF_OldFallingDamage; }
MAPINFOCMD(forcefallingdamage) { info->Flags &= ~(VLevelInfo::LIF_FallingDamage|VLevelInfo::LIF_StrifeFallingDamage); info->Flags |= VLevelInfo::LIF_OldFallingDamage; }
MAPINFOCMD(strifefallingdamage) { info->Flags &= ~(VLevelInfo::LIF_OldFallingDamage|VLevelInfo::LIF_FallingDamage); info->Flags |= VLevelInfo::LIF_StrifeFallingDamage; }
MAPINFOCMD(nofallingdamage) { info->Flags &= ~(VLevelInfo::LIF_OldFallingDamage|VLevelInfo::LIF_StrifeFallingDamage|VLevelInfo::LIF_FallingDamage); }
MAPINFOCMD(monsterfallingdamage) { info->Flags |= VLevelInfo::LIF_MonsterFallingDamage; }
MAPINFOCMD(nomonsterfallingdamage) { info->Flags &= ~VLevelInfo::LIF_MonsterFallingDamage; }
MAPINFOCMD(deathslideshow) { info->Flags |= VLevelInfo::LIF_DeathSlideShow; }
MAPINFOCMD(allowfreelook) { info->Flags &= ~VLevelInfo::LIF_NoFreelook; }
MAPINFOCMD(nofreelook) { info->Flags |= VLevelInfo::LIF_NoFreelook; }
MAPINFOCMD(allowjump) { info->Flags &= ~VLevelInfo::LIF_NoJump; }
MAPINFOCMD(allowcrouch) { info->Flags2 &= ~VLevelInfo::LIF2_NoCrouch; }
MAPINFOCMD(nojump) { info->Flags |= VLevelInfo::LIF_NoJump; }
MAPINFOCMD(nocrouch) { info->Flags2 |= VLevelInfo::LIF2_NoCrouch; }
MAPINFOCMD(resethealth) { info->Flags2 |= VLevelInfo::LIF2_ResetHealth; }
MAPINFOCMD(resetinventory) { info->Flags2 |= VLevelInfo::LIF2_ResetInventory; }
MAPINFOCMD(resetitems) { info->Flags2 |= VLevelInfo::LIF2_ResetItems; }
MAPINFOCMD(noautosequences) { info->Flags |= VLevelInfo::LIF_NoAutoSndSeq; }
MAPINFOCMD(activateowndeathspecials) { info->Flags |= VLevelInfo::LIF_ActivateOwnSpecial; }
MAPINFOCMD(killeractivatesdeathspecials) { info->Flags &= ~VLevelInfo::LIF_ActivateOwnSpecial; }
MAPINFOCMD(missilesactivateimpactlines) { info->Flags |= VLevelInfo::LIF_MissilesActivateImpact; }
MAPINFOCMD(missileshootersactivetimpactlines) { info->Flags &= ~VLevelInfo::LIF_MissilesActivateImpact; }
MAPINFOCMD(filterstarts) { info->Flags |= VLevelInfo::LIF_FilterStarts; }
MAPINFOCMD(infiniteflightpowerup) { info->Flags |= VLevelInfo::LIF_InfiniteFlightPowerup; }
MAPINFOCMD(noinfiniteflightpowerup) { info->Flags &= ~VLevelInfo::LIF_InfiniteFlightPowerup; }
MAPINFOCMD(clipmidtextures) { info->Flags |= VLevelInfo::LIF_ClipMidTex; }
MAPINFOCMD(wrapmidtextures) { info->Flags |= VLevelInfo::LIF_WrapMidTex; }
MAPINFOCMD(keepfullinventory) { info->Flags |= VLevelInfo::LIF_KeepFullInventory; }
MAPINFOCMD(compat_shorttex) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatShortTex, newFormat); }
MAPINFOCMD(compat_stairs) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatStairs, newFormat); }
MAPINFOCMD(compat_limitpain) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatLimitPain, newFormat); }
MAPINFOCMD(compat_nopassover) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatNoPassOver, newFormat); }
MAPINFOCMD(compat_notossdrops) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatNoTossDrops, newFormat); }
MAPINFOCMD(compat_useblocking) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatUseBlocking, newFormat); }
MAPINFOCMD(compat_nodoorlight) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatNoDoorLight, newFormat); }
MAPINFOCMD(compat_ravenscroll) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatRavenScroll, newFormat); }
MAPINFOCMD(compat_soundtarget) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatSoundTarget, newFormat); }
MAPINFOCMD(compat_dehhealth) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatDehHealth, newFormat); }
MAPINFOCMD(compat_trace) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatTrace, newFormat); }
MAPINFOCMD(compat_dropoff) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatDropOff, newFormat); }
MAPINFOCMD(compat_boomscroll) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatBoomScroll, newFormat); }
MAPINFOCMD(additive_scrollers) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatBoomScroll, newFormat); }
MAPINFOCMD(compat_invisibility) { DoCompatFlag(sc, info, VLevelInfo::LIF2_CompatInvisibility, newFormat); }
MAPINFOCMD(compat_sectorsounds) { DoCompatFlag(sc, info, 0, newFormat); }
MAPINFOCMD(compat_crossdropoff) { DoCompatFlag(sc, info, 0, newFormat); }

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(noinfighting) { info->Infighting = -1; }
MAPINFOCMD(normalinfighting) { info->Infighting = 0; }
MAPINFOCMD(totalinfighting) { info->Infighting = 1; }

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(fadetable) {
  if (newFormat) sc->Expect("=");
  sc->ExpectName8();
  info->FadeTable = sc->Name8;
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(fade) {
  if (newFormat) sc->Expect("=");
  sc->ExpectString();
  info->Fade = M_ParseColor(*sc->String);
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(outsidefog) {
  if (newFormat) sc->Expect("=");
  sc->ExpectString();
  info->OutsideFog = M_ParseColor(*sc->String);
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(music) {
  if (newFormat) sc->Expect("=");
  //sc->ExpectName8();
  //info->SongLump = sc->Name8;
  sc->ExpectName();
  info->SongLump = sc->Name;
  const char *nn = *sc->Name;
  if (nn[0] == '$') {
    ++nn;
    if (nn[0] && GLanguage.HasTranslation(nn)) {
      info->SongLump = VName(*GLanguage[nn], VName::AddLower);
    }
  }
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(cdtrack) { if (newFormat) sc->Expect("="); sc->ExpectNumber(); /*info->CDTrack = sc->Number;*/ }
MAPINFOCMD(cd_start_track) { if (newFormat) sc->Expect("="); sc->ExpectNumber(); /*cd_NonLevelTracks[CD_STARTTRACK] = sc->Number;*/ }
MAPINFOCMD(cd_end1_track) { if (newFormat) sc->Expect("="); sc->ExpectNumber(); /*cd_NonLevelTracks[CD_END1TRACK] = sc->Number;*/ }
MAPINFOCMD(cd_end2_track) { if (newFormat) sc->Expect("="); sc->ExpectNumber(); /*cd_NonLevelTracks[CD_END2TRACK] = sc->Number;*/ }
MAPINFOCMD(cd_end3_track) { if (newFormat) sc->Expect("="); sc->ExpectNumber(); /*cd_NonLevelTracks[CD_END3TRACK] = sc->Number;*/ }
MAPINFOCMD(cd_intermission_track) { if (newFormat) sc->Expect("="); sc->ExpectNumber(); /*cd_NonLevelTracks[CD_INTERTRACK] = sc->Number;*/ }
MAPINFOCMD(cd_title_track) { if (newFormat) sc->Expect("="); sc->ExpectNumber(); /*cd_NonLevelTracks[CD_TITLETRACK] = sc->Number;*/ }
MAPINFOCMD(cdid) { skipUnimplementedCommand(sc, true); }

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(gravity) {
  if (newFormat) sc->Expect("=");
  sc->ExpectNumber();
  info->Gravity = (float)sc->Number;
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(aircontrol) {
  if (newFormat) sc->Expect("=");
  sc->ExpectFloat();
  info->AirControl = sc->Float;
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(titlepatch) {
  //FIXME: quoted string is a textual level name
  if (newFormat) sc->Expect("=");
  sc->ExpectName8Def(NAME_None);
  info->EnterTitlePatch = info->ExitTitlePatch = sc->Name8;
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(par) {
  if (newFormat) sc->Expect("=");
  sc->ExpectNumber();
  info->ParTime = sc->Number;
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(sucktime) {
  if (newFormat) sc->Expect("=");
  sc->ExpectNumber();
  info->SuckTime = sc->Number;
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(vertwallshade) {
  if (newFormat) sc->Expect("=");
  sc->ExpectNumber();
  info->VertWallShade = midval(-128, sc->Number, 127);
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(horizwallshade) {
  if (newFormat) sc->Expect("=");
  sc->ExpectNumber();
  info->HorizWallShade = midval(-128, sc->Number, 127);
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(specialaction) {
  if (newFormat) sc->Expect("=");
  VMapSpecialAction &A = info->SpecialActions.Alloc();
  //sc->SetCMode(true);
  sc->ExpectString();
  A.TypeName = *sc->String.ToLower();
  sc->Expect(",");
  sc->ExpectString();
  A.Special = 0;
  for (int i = 0; i < LineSpecialInfos.Num(); ++i) {
    if (!LineSpecialInfos[i].Name.ICmp(sc->String)) {
      A.Special = LineSpecialInfos[i].Number;
      break;
    }
  }
  if (!A.Special) miWarning(sc, "Unknown action special '%s'", *sc->String);
  memset(A.Args, 0, sizeof(A.Args));
  for (int i = 0; i < 5 && sc->Check(","); ++i) {
    sc->ExpectNumber();
    A.Args[i] = sc->Number;
  }
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(redirect) {
  if (newFormat) sc->Expect("=");
  sc->ExpectString();
  info->RedirectType = *sc->String.ToLower();
  info->RedirectMap = ParseNextMapName(sc, HexenMode);
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(strictmonsteractivation) {
  info->Flags2 &= ~VLevelInfo::LIF2_LaxMonsterActivation;
  info->Flags2 |= VLevelInfo::LIF2_HaveMonsterActivation;
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(laxmonsteractivation) {
  info->Flags2 |= VLevelInfo::LIF2_LaxMonsterActivation;
  info->Flags2 |= VLevelInfo::LIF2_HaveMonsterActivation;
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(interpic) {
  if (newFormat) sc->Expect("=");
  //sc->ExpectName8();
  sc->ExpectString();
  info->ExitPic = *sc->String.ToLower();
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(enterpic) {
  if (newFormat) sc->Expect("=");
  //sc->ExpectName8();
  sc->ExpectString();
  info->EnterPic = *sc->String.ToLower();
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(exitpic) {
  if (newFormat) sc->Expect("=");
  //sc->ExpectName8();
  sc->ExpectString();
  info->ExitPic = *sc->String.ToLower();
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(intermusic) {
  if (newFormat) sc->Expect("=");
  sc->ExpectString();
  info->InterMusic = *sc->String.ToLower();
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(background) {
  sc->Message("'background' mapinfo command is not supported");
  if (newFormat) sc->Expect("=");
  //sc->ExpectName8();
  sc->ExpectString();
}

// ////////////////////////////////////////////////////////////////////////// //
MAPINFOCMD(airsupply) { skipUnimplementedCommand(sc, true); }
MAPINFOCMD(sndseq) { skipUnimplementedCommand(sc, true); }
MAPINFOCMD(sndinfo) { skipUnimplementedCommand(sc, true); }
MAPINFOCMD(soundinfo) { skipUnimplementedCommand(sc, true); }
MAPINFOCMD(bordertexture) { skipUnimplementedCommand(sc, true); }
MAPINFOCMD(f1) { skipUnimplementedCommand(sc, true); }
MAPINFOCMD(teamdamage) { skipUnimplementedCommand(sc, true); }
MAPINFOCMD(fogdensity) { skipUnimplementedCommand(sc, true); }
MAPINFOCMD(outsidefogdensity) { skipUnimplementedCommand(sc, true); }
MAPINFOCMD(skyfog) { skipUnimplementedCommand(sc, true); }
MAPINFOCMD(translator) { sc->MessageErr(va("*** map '%s' contains translator lump, it may not work!", *info->LumpName)); skipUnimplementedCommand(sc, true); }
MAPINFOCMD(lightmode) { skipUnimplementedCommand(sc, true); }
MAPINFOCMD(smoothlighting) { info->FakeContrast = 1; }
MAPINFOCMD(evenlighting) { info->FakeContrast = 2; }


//==========================================================================
//
//  FixSkyTexturesHack
//
//  another zdoom hack: check for "sky_maplump" sky texture
//
//==========================================================================
static void FixOneSkyTextureHack (VScriptParser *sc, VMapInfo *info, int skynum, vint32 &tx) {
  if (tx < 1) return;

  //GCon->Logf(NAME_Debug, "map '%s': sky1 '%s'", *info->LumpName, *GTextureManager.GetTextureName(tx));
  VName skn = VName(*(VStr(*GTextureManager.GetTextureName(tx))+"_"+(*info->LumpName)), VName::AddLower);
  if (VStr::length(*skn) <= 8) {
    //GCon->Logf(NAME_Debug, "map '%s': trying sky1 '%s'", *info->LumpName, *skn);
    int tt = loadSkyTexture(sc, skn, true);
    if (tt > 0) {
      GCon->Logf(NAME_Debug, "map '%s': sky%d '%s' replaced with '%s'", *info->LumpName, skynum, *GTextureManager.GetTextureName(tx), *GTextureManager.GetTextureName(tt));
      tx = tt;
      return;
    }
  }

  skn = VName(*(VStr("sky_")+(*info->LumpName)), VName::AddLower);
  if (VStr::length(*skn) <= 8) {
    //GCon->Logf(NAME_Debug, "map '%s': trying sky1 '%s'", *info->LumpName, *skn);
    int tt = loadSkyTexture(sc, skn, true);
    if (tt > 0) {
      GCon->Logf(NAME_Debug, "map '%s': sky%d '%s' replaced with '%s'", *info->LumpName, skynum, *GTextureManager.GetTextureName(tx), *GTextureManager.GetTextureName(tt));
      tx = tt;
      return;
    }
  }
}


//==========================================================================
//
//  FixSkyTexturesHack
//
//  another zdoom hack: check for "sky_maplump" sky texture
//
//==========================================================================
static void FixSkyTexturesHack (VScriptParser *sc, VMapInfo *info) {
  FixOneSkyTextureHack(sc, info, 1, info->Sky1Texture);
  //FixOneSkyTextureHack(sc, info, 2, info->Sky2Texture);

  // if we have "lightning", but no "sky2", make "sky2" equal to "sky1" (otherwise the sky may flicker)
  // actually, always do this, because why not?
  if (/*(info->Flags&VLevelInfo::LIF_Lightning) != 0 &&*/ (wasSky1Sky2&WSK_WAS_SKY2) == 0) {
    // has lighting, but no second sky; force second sky to be the same as the first one
    // (but only if the first one is not a skybox)
    if (info->SkyBox == NAME_None) {
      info->Sky2Texture = info->Sky1Texture;
      info->Sky2ScrollDelta = info->Sky1ScrollDelta;
    }
  }
}


//==========================================================================
//
//  ParseMapCommon
//
//==========================================================================
static void ParseMapCommon (VScriptParser *sc, VMapInfo *info, bool &HexenMode) {
  // build command map, if it is not built yet
  if (mcmap.length() == 0 && mclist) {
    for (MapInfoCommand *mcp = mclist; mcp; mcp = mcp->next) {
      VStr cn = VStr(mcp->cmd).toLowerCase().xstrip();
      if (cn.isEmpty()) Sys_Error("internal engine error: unnamed mapinfo command handler!");
      if (mcmap.put(cn, mcp)) Sys_Error("internal engine error: duplicate mapinfo command handler for '%s'!", mcp->cmd);
    }
  }

  // if we have "lightning", but no "sky2", make "sky2" equal to "sky1" (otherwise the sky may flicker)
  wasSky1Sky2 = 0u; // clear "was skyN" flag

  const bool newFormat = sc->Check("{");
  //if (newFormat) sc->SetCMode(true);
  // process optional tokens
  for (;;) {
    //sc->GetString(); sc->UnGet(); GCon->Logf(NAME_Debug, "%s: %s", *sc->GetLoc().toStringNoCol(), *sc->String);
    if (!sc->GetString()) break;
    auto mpp = mcmap.find(sc->String.toLowerCase());
    if (mpp) {
      (*(*mpp)->handler)(sc, newFormat, info, HexenMode);
    } else {
      //GCon->Logf(NAME_Debug, "%s: NOT FOUND cmd='%s' (new=%d)", *sc->GetLoc().toStringNoCol(), *sc->String, (int)newFormat);
      sc->UnGet();
      if (!newFormat) break;
      if (sc->Check("}")) break;
      if (!sc->GetString()) break;
      //sc->Message(va("*** unknown mapinfo command '%s', skipping", *sc->String));
      skipUnimplementedCommand(sc, false); // don't force args, but skip them
      if (sc->Check("}")) break;
      continue;
    }
    /*
    if (sc->CheckStartsWith("compat_")) {
      GCon->Logf(NAME_Warning, "%s: mapdef '%s' is not supported yet", *sc->GetLoc().toStringNoCol(), *sc->String);
      sc->Check("=");
      sc->CheckNumber();
    }
    */
    // these are stubs for now
    //} else if (sc->Check("noinventorybar")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("allowcrouch")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("pausemusicinmenus")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("allowrespawn")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("teamplayon")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("teamplayoff")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("checkswitchrange")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("nocheckswitchrange")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("unfreezesingleplayerconversations")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("smoothlighting")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("Grinding_PolyObj")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("UsePlayerStartZ")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("spawnwithweaponraised")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("noautosavehint")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("PrecacheTextures")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("PrecacheSounds")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("PrecacheClasses")) { skipUnimplementedCommand(sc, false);
    //} else if (sc->Check("intermissionmusic")) { skipUnimplementedCommand(sc, false);
  }
  //if (newFormat) sc->SetCMode(false);

  FixSkyTexturesHack(sc, info);

  // second sky defaults to first sky
  if (info->Sky2Texture == GTextureManager.DefaultTexture) info->Sky2Texture = info->Sky1Texture;
  if (info->Flags&VLevelInfo::LIF_DoubleSky) GTextureManager.SetFrontSkyLayer(info->Sky1Texture);
}


//==========================================================================
//
//  ParseNameOrLookup
//
//==========================================================================
static void ParseNameOrLookup (VScriptParser *sc, vuint32 lookupFlag, VStr *name, vuint32 *flags, bool newStyle) {
  if (sc->Check("lookup")) {
    if (sc->GetString()) {
      if (sc->QuotedString || sc->String != ",") sc->UnGet();
    }
    //if (newStyle) sc->Check(",");
    *flags |= lookupFlag;
    sc->ExpectString();
    if (sc->String.length() > 1 && sc->String[0] == '$') {
      *name = VStr(*sc->String+1).ToLower();
    } else {
      *name = sc->String.ToLower();
    }
  } else {
    sc->ExpectString();
    if (sc->String.Length() > 1 && sc->String[0] == '$') {
      *flags |= lookupFlag;
      *name = VStr(*sc->String+1).ToLower();
    } else {
      *flags &= ~lookupFlag;
      *name = sc->String;
      if (lookupFlag == VLevelInfo::LIF_LookupName) return;
      if (newStyle) {
        while (!sc->AtEnd()) {
          if (!sc->Check(",")) break;
          if (sc->AtEnd()) break;
          sc->ExpectString();
          while (!sc->QuotedString) {
            if (sc->String == "}") { sc->UnGet(); break; } // stray comma
            if (sc->String != ",") { sc->UnGet(); sc->Error("comma expected"); break; }
            if (sc->AtEnd()) break;
            sc->ExpectString();
          }
          *name += "\n";
          *name += sc->String;
        }
      } else {
        while (!sc->AtEnd()) {
          sc->ExpectString();
          if (sc->Crossed) { sc->UnGet(); break; }
          while (!sc->QuotedString) {
            if (sc->String != ",") { sc->UnGet(); sc->Error("comma expected"); break; }
            if (sc->AtEnd()) break;
            sc->ExpectString();
          }
          *name += "\n";
          *name += sc->String;
        }
      }
      //GCon->Logf(NAME_Debug, "COLLECTED: <%s>", **name);
    }
  }
}


//==========================================================================
//
//  ParseNameOrLookup
//
//==========================================================================
static void ParseNameOrLookup (VScriptParser *sc, vint32 lookupFlag, VStr *name, vint32 *flags, bool newStyle) {
  vuint32 lf = (vuint32)lookupFlag;
  vuint32 flg = (vuint32)*flags;
  ParseNameOrLookup(sc, lf, name, &flg, newStyle);
  *flags = (vint32)flg;
}


//==========================================================================
//
//  ParseUStringKey
//
//==========================================================================
static VStr ParseUStringKey (VScriptParser *sc) {
  sc->Expect("=");
  sc->ExpectString();
  return sc->String.xstrip();
}


//==========================================================================
//
//  ParseUBoolKey
//
//==========================================================================
static bool ParseUBoolKey (VScriptParser *sc) {
  sc->Expect("=");
  sc->ExpectString();
  VStr ss = sc->String.xstrip();
  if (ss.strEquCI("false") || ss.strEquCI("0")) return false;
  if (ss.strEquCI("true") || ss.strEquCI("1")) return true;
  sc->Error("boolean value expected");
  return false;
}


//==========================================================================
//
//  ParseMapUMapinfo
//
//==========================================================================
static void ParseMapUMapinfo (VScriptParser *sc, VMapInfo *info) {
  // if we have "lightning", but no "sky2", make "sky2" equal to "sky1" (otherwise the sky may flicker)
  wasSky1Sky2 = 0u; // clear "was skyN" flag
  bool wasEndGame = false;
  bool wasClearBossAction = false;
  VStr endType;
  VStr episodeName;

  sc->Expect("{");
  for (;;) {
    const TLocation loc = sc->GetLoc();

    if (sc->Check("}")) break;
    if (sc->AtEnd()) break;

    if (sc->Check("levelname")) {
      VStr ss = ParseUStringKey(sc);
      if (ss.length()) info->Name = ss;
      continue;
    }
    if (sc->Check("levelpic")) {
      VStr ss = ParseUStringKey(sc);
      info->EnterTitlePatch = info->ExitTitlePatch = VName(*ss, VName::AddLower);
      continue;
    }
    if (sc->Check("exitpic")) {
      VStr ss = ParseUStringKey(sc);
      info->ExitPic = VName(*ss, VName::AddLower);
      continue;
    }
    if (sc->Check("enterpic")) {
      VStr ss = ParseUStringKey(sc);
      info->EnterPic = VName(*ss, VName::AddLower);
      continue;
    }
    if (sc->Check("next")) {
      VStr ss = ParseUStringKey(sc);
      if (ss.length()) info->NextMap = VName(*ss, VName::AddLower);
      continue;
    }
    if (sc->Check("nextsecret")) {
      VStr ss = ParseUStringKey(sc);
      if (ss.length()) info->SecretMap = VName(*ss, VName::AddLower);
      continue;
    }
    if (sc->Check("skytexture")) {
      VStr ss = ParseUStringKey(sc);
      if (ss.length()) {
        wasSky1Sky2 |= WSK_WAS_SKY1;
        VName skbname = R_HasNamedSkybox(sc->String);
        if (skbname != NAME_None) {
          //k8: ok, this may be done to support sourceports that cannot into skyboxes
          miWarning(loc, "sky1 '%s' is actually a skybox (this is mostly harmless)", *sc->String);
          info->SkyBox = skbname;
          info->Sky1Texture = GTextureManager.DefaultTexture;
          info->Sky2Texture = GTextureManager.DefaultTexture;
          info->Sky1ScrollDelta = 0;
          info->Sky2ScrollDelta = 0;
        } else {
          info->SkyBox = NAME_None;
          info->Sky1Texture = loadSkyTexture(sc, VName(*ss, VName::AddLower));
          info->Sky1ScrollDelta = 0;
        }
      }
      continue;
    }
    if (sc->Check("music")) {
      VStr ss = ParseUStringKey(sc);
      if (ss.length()) info->SongLump = VName(*ss, VName::AddLower);
      continue;
    }
    if (sc->Check("partime")) {
      sc->Expect("=");
      sc->ExpectNumber();
      info->ParTime = sc->Number;
      continue;
    }
    if (sc->Check("endgame")) {
      wasEndGame = ParseUBoolKey(sc);
      continue;
    }
    if (sc->Check("endpic")) {
      VStr ss = ParseUStringKey(sc);
      if (ss.length()) {
        endType = VStr(va("EndGameCustomPic%s", *ss));
      } else {
        endType = "EndGamePic3"; // arbitrary decision, credits
      }
      continue;
    }
    if (sc->Check("endbunny")) {
      if (ParseUBoolKey(sc)) endType = "EndGameBunny";
      continue;
    }
    if (sc->Check("endcast")) {
      if (ParseUBoolKey(sc)) endType = "EndGameCast";
      continue;
    }
    if (sc->Check("bossaction")) {
      // clear all special map action flags
      info->Flags &= ~(
        VLevelInfo::LIF_Map07Special|
        VLevelInfo::LIF_BaronSpecial|
        VLevelInfo::LIF_CyberDemonSpecial|
        VLevelInfo::LIF_SpiderMastermindSpecial|
        VLevelInfo::LIF_MinotaurSpecial|
        VLevelInfo::LIF_DSparilSpecial|
        VLevelInfo::LIF_IronLichSpecial|
        VLevelInfo::LIF_SpecialActionOpenDoor|
        VLevelInfo::LIF_SpecialActionLowerFloor|
        VLevelInfo::LIF_SpecialActionKillMonsters);
      // can't we fuckin' have a complete specs for ANYTHING, for fuck's sake?!
      VStr className = ParseUStringKey(sc);
      if (className.strEquCI("clear")) {
        info->SpecialActions.clear();
        wasClearBossAction = true;
      } else {
        //bossaction = thingtype, linespecial, tag
        sc->Expect(",");
        sc->ExpectNumber();
        int special = sc->Number;
        //if (special < 0) sc->Error(va("invalid bossaction special %d", special));
        sc->Expect(",");
        sc->ExpectNumber();
        int tag = sc->Number;
        // allow no 0-tag specials here, unless a level exit
        if (className.length() && special > 0 && (tag != 0 || special == 11 || special == 51 || special == 52 || special == 124)) {
          // add special action
          if (info->SpecialActions.length() == 0) {
            VMapSpecialAction &aa = info->SpecialActions.Alloc();
            aa.TypeName = VName("UMapInfoActions");
            aa.Special = 666999; // this means nothing
          }
          VMapSpecialAction &A = info->SpecialActions.Alloc();
          A.TypeName = VName(*className);
          A.Special = -special; // it should be translated
          A.Args[0] = tag;
          A.Args[1] = A.Args[2] = A.Args[3] = A.Args[4] = 0;
          wasClearBossAction = false;
        } else {
          miWarning(sc, "Invalid bossaction special %d (tag %d)", special, tag);
        }
      }
      continue;
    }

    if (sc->Check("episode")) {
      VStr ss = ParseUStringKey(sc);
      if (ss.strEquCI("clear")) {
        EpisodeDefs.Clear();
      } else {
        VStr pic = ss;
        VStr epname;
        bool checkReplace = true;
        if (sc->Check(",")) {
          sc->ExpectString();
          epname = sc->String.xstrip();
          if (sc->Check(",")) sc->ExpectString(); // ignore key
        } else {
          epname = "Unnamed episode";
          checkReplace = false;
        }

        VEpisodeDef *EDef = nullptr;
        // check for replaced episode
        if (checkReplace) {
          for (int i = 0; i < EpisodeDefs.length(); ++i) {
            if (sc->Name == EpisodeDefs[i].Name) {
              EDef = &EpisodeDefs[i];
              break;
            }
          }
        }
        if (!EDef) EDef = &EpisodeDefs.Alloc();

        // set defaults
        EDef->Name = info->LumpName;
        EDef->TeaserName = NAME_None;
        EDef->Text = epname;
        EDef->PicName = VName(*ss, VName::AddLower);
        EDef->Flags = 0;
        EDef->Key = VStr();
        EDef->MapinfoSourceLump = info->MapinfoSourceLump;
      }
      continue;
    }

    if (sc->Check("nointermission")) {
      if (ParseUBoolKey(sc)) info->Flags |= VLevelInfo::LIF_NoIntermission; else info->Flags &= ~VLevelInfo::LIF_NoIntermission;
      continue;
    }

    // intertexts require creating new clusters; not now (and maybe never, because i don't really care)
    if (sc->Check("intertext")) {
      VStr exitText = ParseUStringKey(sc);
      while (sc->Check(",")) {
        exitText += "\n";
        sc->ExpectString();
        exitText += sc->String.xstrip();
      }
      exitText = exitText.xstrip();
      if (exitText.strEquCI("clear")) exitText = VStr(" ");
      info->ExitText = exitText;
      continue;
    }
    if (sc->Check("intertextsecret")) {
      VStr exitText = ParseUStringKey(sc);
      while (sc->Check(",")) {
        exitText += "\n";
        sc->ExpectString();
        exitText += sc->String.xstrip();
      }
      exitText = exitText.xstrip();
      if (exitText.strEquCI("clear")) exitText = VStr(" ");
      info->SecretExitText = exitText;
      continue;
    }
    if (sc->Check("interbackdrop")) {
      VStr ss = ParseUStringKey(sc);
      info->InterBackdrop = VName(*ss, VName::AddLower);
      continue;
    }
    if (sc->Check("intermusic")) {
      VStr ss = ParseUStringKey(sc);
      info->InterMusic = VName(*ss, VName::AddLower);
      continue;
    }

    sc->Error(va("Unknown UMAPINFO map key '%s'", *sc->String));
  }

  if (wasEndGame || endType.length()) {
    if (endType.length() == 0) endType = "EndGamePic3"; // arbitrary decision, credits
    info->NextMap = VName(*endType);
  }

  if (info->SecretMap == NAME_None) info->SecretMap = info->NextMap;

  if (wasClearBossAction) {
    vassert(info->SpecialActions.length() == 0);
    // special action
    VMapSpecialAction &A = info->SpecialActions.Alloc();
    A.TypeName = VName("UMapInfoDoNothing");
    A.Special = 666999; // this means nothing
  }

  FixSkyTexturesHack(sc, info);

  // second sky defaults to first sky
  if (info->Sky2Texture == GTextureManager.DefaultTexture) info->Sky2Texture = info->Sky1Texture;
  if (info->Flags&VLevelInfo::LIF_DoubleSky) GTextureManager.SetFrontSkyLayer(info->Sky1Texture);
}


//==========================================================================
//
//  ParseMap
//
//==========================================================================
static void ParseMap (VScriptParser *sc, bool &HexenMode, VMapInfo &Default, int milumpnum, bool umapinfo=false) {
  VMapInfo *info = nullptr;
  vuint32 savedFlags = 0;

  VName MapLumpName;
  if (!umapinfo && sc->CheckNumber()) {
    // map number, for Hexen compatibility
    HexenMode = true;
    if (sc->Number < 1 || sc->Number > 99) sc->Error("Map number out or range");
    MapLumpName = va("map%02d", sc->Number);
  } else {
    // map name
    sc->ExpectString();
    VStr nn = sc->String.xstrip();
    if (nn.length() == 0) sc->Error("empty map name");
    MapLumpName = VName(*sc->String, VName::AddLower);
  }

  // check for replaced map info
  bool replacement = false;
  for (int i = 0; i < MapInfo.Num(); ++i) {
    if (MapLumpName == MapInfo[i].LumpName) {
      info = &MapInfo[i];
      //GCon->Logf(NAME_Init, "replaced map '%s' (Sky1Texture=%d; default=%d)", *info->LumpName, info->Sky1Texture, Default.Sky1Texture);
      savedFlags = info->Flags;
      replacement = true;
      break;
    }
  }
  if (!info) info = &MapInfo.Alloc();

  // Copy defaults to current map definition
  info->LumpName = MapLumpName;
  if (!replacement) {
    info->LevelNum = Default.LevelNum;
    info->Cluster = Default.Cluster;
  }
  info->WarpTrans = Default.WarpTrans;
  if (!replacement) {
    info->NextMap = Default.NextMap;
    info->SecretMap = Default.SecretMap;
    info->SongLump = Default.SongLump;
  }
  if (!replacement) {
    info->Sky1Texture = Default.Sky1Texture;
    info->Sky2Texture = Default.Sky2Texture;
    info->Sky1ScrollDelta = Default.Sky1ScrollDelta;
    info->Sky2ScrollDelta = Default.Sky2ScrollDelta;
    info->SkyBox = Default.SkyBox;
  }
  info->FadeTable = Default.FadeTable;
  info->Fade = Default.Fade;
  info->OutsideFog = Default.OutsideFog;
  info->Gravity = Default.Gravity;
  info->AirControl = Default.AirControl;
  info->Flags = Default.Flags;
  info->Flags2 = Default.Flags2;
  info->EnterTitlePatch = Default.EnterTitlePatch;
  info->ExitTitlePatch = Default.ExitTitlePatch;
  info->ParTime = Default.ParTime;
  info->SuckTime = Default.SuckTime;
  info->HorizWallShade = Default.HorizWallShade;
  info->VertWallShade = Default.VertWallShade;
  info->Infighting = Default.Infighting;
  info->SpecialActions = Default.SpecialActions;
  info->RedirectType = Default.RedirectType;
  info->RedirectMap = Default.RedirectMap;
  info->ExitText = Default.ExitText;
  info->SecretExitText = Default.SecretExitText;
  info->InterBackdrop = Default.InterBackdrop;
  if (!replacement) {
    info->ExitPic = Default.ExitPic;
    info->EnterPic = Default.EnterPic;
    info->InterMusic = Default.InterMusic;
  }

  // copy "no intermission" flag from default map
  if (Default.Flags&VLevelInfo::LIF_NoIntermission) info->Flags |= VLevelInfo::LIF_NoIntermission;

  if (HexenMode) {
    info->Flags |= VLevelInfo::LIF_NoIntermission|
      VLevelInfo::LIF_FallingDamage|
      VLevelInfo::LIF_MonsterFallingDamage|
      VLevelInfo::LIF_NoAutoSndSeq|
      VLevelInfo::LIF_ActivateOwnSpecial|
      VLevelInfo::LIF_MissilesActivateImpact|
      VLevelInfo::LIF_InfiniteFlightPowerup;
  }

  // set saved par time
  int par = findSavedPar(MapLumpName);
  if (par > 0) {
    //GCon->Logf(NAME_Init, "found dehacked par time for map '%s' (%d)", *MapLumpName, par);
    info->ParTime = par;
  }

  if (!umapinfo) {
    // map name must follow the number
    ParseNameOrLookup(sc, VLevelInfo::LIF_LookupName, &info->Name, &info->Flags, false);
  }

  // set song lump name from SNDINFO script
  for (int i = 0; i < MapSongList.Num(); ++i) {
    if (MapSongList[i].MapName == info->LumpName) {
      info->SongLump = MapSongList[i].SongName;
    }
  }

  // set default levelnum for this map
  const char *mn = *MapLumpName;
  if (mn[0] == 'm' && mn[1] == 'a' && mn[2] == 'p' && mn[5] == 0) {
    int num = VStr::atoi(mn+3);
    if (num >= 1 && num <= 99) info->LevelNum = num;
  } else if (mn[0] == 'e' && mn[1] >= '0' && mn[1] <= '9' &&
             mn[2] == 'm' && mn[3] >= '0' && mn[3] <= '9')
  {
    info->LevelNum = (mn[1]-'1')*10+(mn[3]-'0');
  }

  info->MapinfoSourceLump = milumpnum;

  if (!umapinfo) {
    ParseMapCommon(sc, info, HexenMode);
  } else {
    // copy special actions, they should be explicitly cleared
    info->Flags = savedFlags&(
      VLevelInfo::LIF_Map07Special|
      VLevelInfo::LIF_BaronSpecial|
      VLevelInfo::LIF_CyberDemonSpecial|
      VLevelInfo::LIF_SpiderMastermindSpecial|
      VLevelInfo::LIF_MinotaurSpecial|
      VLevelInfo::LIF_DSparilSpecial|
      VLevelInfo::LIF_IronLichSpecial|
      VLevelInfo::LIF_SpecialActionOpenDoor|
      VLevelInfo::LIF_SpecialActionLowerFloor|
      VLevelInfo::LIF_SpecialActionKillMonsters);
    ParseMapUMapinfo(sc, info);
  }

  info->MapinfoSourceLump = milumpnum;

  // avoid duplicate levelnums, later one takes precedance
  if (info->LevelNum) {
    for (int i = 0; i < MapInfo.Num(); ++i) {
      if (MapInfo[i].LevelNum == info->LevelNum && &MapInfo[i] != info) {
        if (W_IsUserWadLump(MapInfo[i].MapinfoSourceLump)) {
          if (MapInfo[i].MapinfoSourceLump != info->MapinfoSourceLump) {
            GCon->Logf(NAME_Warning, "duplicate levelnum %d for maps '%s' and '%s' ('%s' zeroed)", info->LevelNum, *MapInfo[i].LumpName, *info->LumpName, *MapInfo[i].LumpName);
            GCon->Logf(NAME_Warning, "  first map is defined in '%s'", *W_FullLumpName(MapInfo[i].MapinfoSourceLump));
            GCon->Logf(NAME_Warning, "  second map is defined in '%s'", *W_FullLumpName(info->MapinfoSourceLump));
          } else {
            GCon->Logf(NAME_Warning, "duplicate levelnum %d for maps '%s' and '%s' ('%s' zeroed)", info->LevelNum, *MapInfo[i].LumpName, *info->LumpName, *info->LumpName);
            GCon->Logf(NAME_Warning, "  both maps are defined in '%s'", *W_FullLumpName(info->MapinfoSourceLump));
            // this means that latter episode is fucked -- so fuck it for real
            info->LevelNum = 0;
            break;
          }
        }
        MapInfo[i].LevelNum = 0;
      }
    }
  }
}


//==========================================================================
//
//  ParseClusterDef
//
//==========================================================================
static void ParseClusterDef (VScriptParser *sc) {
  VClusterDef *CDef = nullptr;
  sc->ExpectNumber();

  // check for replaced cluster def
  for (int i = 0; i < ClusterDefs.Num(); ++i) {
    if (sc->Number == ClusterDefs[i].Cluster) {
      CDef = &ClusterDefs[i];
      break;
    }
  }
  if (!CDef) CDef = &ClusterDefs.Alloc();

  // set defaults
  CDef->Cluster = sc->Number;
  CDef->Flags = 0;
  CDef->EnterText = VStr();
  CDef->ExitText = VStr();
  CDef->Flat = NAME_None;
  CDef->Music = NAME_None;

  //GCon->Logf(NAME_Debug, "=== NEW CLUSTER %d ===", CDef->Cluster);
  bool newFormat = sc->Check("{");
  //if (newFormat) sc->SetCMode(true);
  while (!sc->AtEnd()) {
    //if (sc->GetString()) { GCon->Logf(NAME_Debug, ":%s: CLUSTER(%d): <%s>", *sc->GetLoc().toStringNoCol(), (newFormat ? 1 : 0), *sc->String); sc->UnGet(); }
    if (sc->Check("hub")) {
      CDef->Flags |= CLUSTERF_Hub;
    } else if (sc->Check("entertext")) {
      if (newFormat) sc->Expect("=");
      ParseNameOrLookup(sc, CLUSTERF_LookupEnterText, &CDef->EnterText, &CDef->Flags, newFormat);
      //GCon->Logf(NAME_Debug, "::: <%s>", *CDef->EnterText);
    } else if (sc->Check("entertextislump")) {
      CDef->Flags |= CLUSTERF_EnterTextIsLump;
    } else if (sc->Check("exittext")) {
      if (newFormat) sc->Expect("=");
      ParseNameOrLookup(sc, CLUSTERF_LookupExitText, &CDef->ExitText, &CDef->Flags, newFormat);
    } else if (sc->Check("exittextislump")) {
      CDef->Flags |= CLUSTERF_ExitTextIsLump;
    } else if (sc->Check("flat")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectName8();
      CDef->Flat = sc->Name8;
      CDef->Flags &= ~CLUSTERF_FinalePic;
    } else if (sc->Check("pic")) {
      if (newFormat) sc->Expect("=");
      //sc->ExpectName8();
      sc->ExpectName();
      CDef->Flat = sc->Name;
      CDef->Flags |= CLUSTERF_FinalePic;
    } else if (sc->Check("music")) {
      if (newFormat) sc->Expect("=");
      //sc->ExpectName8();
      sc->ExpectName();
      CDef->Music = sc->Name;
    } else if (sc->Check("cdtrack")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectNumber();
      //CDef->CDTrack = sc->Number;
    } else if (sc->Check("cdid")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectNumber();
      //CDef->CDId = sc->Number;
    } else if (sc->Check("name")) {
      auto loc = sc->GetLoc();
      if (newFormat) sc->Expect("=");
      if (sc->Check("lookup")) {
        if (newFormat) sc->Expect(",");
      }
      sc->ExpectString();
      miWarning(loc, "Unimplemented cluster command 'name'");
    } else {
      if (newFormat) {
        if (!sc->Check("}")) {
          auto loc = sc->GetLoc();
          sc->ExpectString();
          VStr cmd = sc->String;
          //fprintf(stderr, "!!!!!!\n");
          if (sc->Check("=")) {
            //fprintf(stderr, "******\n");
            while (!sc->AtEnd()) {
              sc->ExpectString();
              if (!sc->Check(",")) break;
            }
          }
          miWarning(loc, "unknown clusterdef command '%s'", *cmd);
        } else {
          break;
          //sc->Error(va("'}' expected in clusterdef, but got \"%s\"", *sc->String));
        }
      } else {
        break;
      }
    }
  }
  //if (newFormat) sc->SetCMode(false);

  // make sure text lump names are in lower case
  if (CDef->Flags&CLUSTERF_EnterTextIsLump) CDef->EnterText = CDef->EnterText.ToLower();
  if (CDef->Flags&CLUSTERF_ExitTextIsLump) CDef->ExitText = CDef->ExitText.ToLower();
}


//==========================================================================
//
//  ParseEpisodeDef
//
//==========================================================================
static void ParseEpisodeDef (VScriptParser *sc, int milumpnum) {
  VEpisodeDef *EDef = nullptr;
  int EIdx = 0;
  sc->ExpectName();

  // check for replaced episode
  for (int i = 0; i < EpisodeDefs.Num(); ++i) {
    if (sc->Name == EpisodeDefs[i].Name) {
      EDef = &EpisodeDefs[i];
      EIdx = i;
      break;
    }
  }
  if (!EDef) {
    EDef = &EpisodeDefs.Alloc();
    EIdx = EpisodeDefs.Num()-1;
  }

  // check for removal of an episode
  if (sc->Check("remove")) {
    EpisodeDefs.RemoveIndex(EIdx);
    return;
  }

  // set defaults
  EDef->Name = sc->Name;
  EDef->TeaserName = NAME_None;
  EDef->Text = VStr();
  EDef->PicName = NAME_None;
  EDef->Flags = 0;
  EDef->Key = VStr();
  EDef->MapinfoSourceLump = milumpnum;

  if (sc->Check("teaser")) {
    sc->ExpectName();
    EDef->TeaserName = sc->Name;
  }

  bool newFormat = sc->Check("{");
  //if (newFormat) sc->SetCMode(true);
  while (!sc->AtEnd()) {
    if (sc->Check("name")) {
      if (newFormat) sc->Expect("=");
      ParseNameOrLookup(sc, EPISODEF_LookupText, &EDef->Text, &EDef->Flags, newFormat);
    } else if (sc->Check("picname")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectName();
      EDef->PicName = sc->Name;
    } else if (sc->Check("key")) {
      if (newFormat) sc->Expect("=");
      sc->ExpectString();
      EDef->Key = sc->String.ToLower();
    } else if (sc->Check("noskillmenu")) {
      EDef->Flags |= EPISODEF_NoSkillMenu;
    } else if (sc->Check("optional")) {
      EDef->Flags |= EPISODEF_Optional;
    } else {
      if (newFormat && !sc->AtEnd()) sc->Expect("}");
      break;
    }
  }
  //if (newFormat) sc->SetCMode(false);
}


//==========================================================================
//
//  ParseSkillDefOld
//
//==========================================================================
static void ParseSkillDefOld (VScriptParser *sc, VSkillDef *sdef) {
  while (!sc->AtEnd()) {
    if (sc->Check("AmmoFactor")) {
      sc->ExpectFloat();
      sdef->AmmoFactor = sc->Float;
    } else if (sc->Check("DropAmmoFactor")) {
      sc->ExpectFloat();
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill setting 'DropAmmoFactor' is not implemented yet.", *sc->GetLoc().toStringNoCol());
    } else if (sc->Check("DoubleAmmoFactor")) {
      sc->ExpectFloat();
      sdef->DoubleAmmoFactor = sc->Float;
    } else if (sc->Check("DamageFactor")) {
      sc->ExpectFloat();
      sdef->DamageFactor = sc->Float;
    } else if (sc->Check("FastMonsters")) {
      sdef->Flags |= SKILLF_FastMonsters;
    } else if (sc->Check("DisableCheats")) {
      //k8: no, really?
      //sdef->Flags |= SKILLF_DisableCheats;
    } else if (sc->Check("EasyBossBrain")) {
      sdef->Flags |= SKILLF_EasyBossBrain;
    } else if (sc->Check("AutoUseHealth")) {
      sdef->Flags |= SKILLF_AutoUseHealth;
    } else if (sc->Check("RespawnTime")) {
      sc->ExpectFloat();
      sdef->RespawnTime = sc->Float;
    } else if (sc->Check("RespawnLimit")) {
      sc->ExpectNumber();
      sdef->RespawnLimit = sc->Number;
    } else if (sc->Check("NoPain")) {
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'NoPain' is not implemented yet.", *sc->GetLoc().toStringNoCol());
    } else if (sc->Check("Aggressiveness")) {
      sc->ExpectFloatWithSign();
      if (sc->Float < 0) GCon->Logf(NAME_Warning, "%s:MAPINFO: \"Aggressiveness\" should be positive", *sc->GetLoc().toStringNoCol());
      sdef->Aggressiveness = 1.0f-midval(0.0f, (float)sc->Float, 1.0f);
    } else if (sc->Check("SpawnFilter")) {
      if (sc->CheckNumber()) {
        if (sc->Number > 0 && sc->Number < 31) sdef->SpawnFilter = 1<<(sc->Number-1);
      } else {
             if (sc->Check("Baby")) sdef->SpawnFilter = 1;
        else if (sc->Check("Easy")) sdef->SpawnFilter = 2;
        else if (sc->Check("Normal")) sdef->SpawnFilter = 4;
        else if (sc->Check("Hard")) sdef->SpawnFilter = 8;
        else if (sc->Check("Nightmare")) sdef->SpawnFilter = 16;
        else sc->ExpectString();
      }
    } else if (sc->Check("ACSReturn")) {
      sc->ExpectNumber();
      sdef->AcsReturn = sc->Number;
    } else if (sc->Check("Name")) {
      sc->ExpectString();
      sdef->MenuName = sc->String;
      sdef->Flags &= ~SKILLF_MenuNameIsPic;
    } else if (sc->Check("PlayerClassName")) {
      VSkillPlayerClassName &CN = sdef->PlayerClassNames.Alloc();
      sc->ExpectString();
      CN.ClassName = sc->String;
      sc->ExpectString();
      CN.MenuName = sc->String;
    } else if (sc->Check("PicName")) {
      sc->ExpectString();
      sdef->MenuName = sc->String.ToLower();
      sdef->Flags |= SKILLF_MenuNameIsPic;
    } else if (sc->Check("MustConfirm")) {
      sdef->Flags |= SKILLF_MustConfirm;
      if (sc->CheckQuotedString()) sdef->ConfirmationText = sc->String;
    } else if (sc->Check("Key")) {
      sc->ExpectString();
      sdef->Key = sc->String;
    } else if (sc->Check("TextColor")) {
      sc->ExpectString();
      sdef->TextColor = sc->String;
    } else {
      break;
    }
  }

  if (sdef->SpawnFilter == 0) {
    GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'SpawnFilter' is not set for skill '%s'; assume UV.", *sc->GetLoc().toStringNoCol(), *sdef->MenuName);
    sdef->SpawnFilter = 8; // UV
  }
}


//==========================================================================
//
//  ParseSkillDef
//
//==========================================================================
static void ParseSkillDef (VScriptParser *sc) {
  VSkillDef *sdef = nullptr;
  sc->ExpectString();

  // check for replaced skill
  for (int i = 0; i < SkillDefs.Num(); ++i) {
    if (sc->String.ICmp(SkillDefs[i].Name) == 0) {
      sdef = &SkillDefs[i];
      //GCon->Logf(NAME_Debug, "SKILLDEF:%s: replaced skill #%d '%s' (%s)", *sc->GetLoc().toStringNoCol(), i, *sc->String, *SkillDefs[i].Name);
      break;
    }
  }
  if (!sdef) {
    sdef = &SkillDefs.Alloc();
    sdef->Name = sc->String;
    //GCon->Logf(NAME_Debug, "SKILLDEF:%s: new skill #%d '%s'", *sc->GetLoc().toStringNoCol(), SkillDefs.length(), *sc->String);
  }

  // set defaults
  sdef->AmmoFactor = 1.0f;
  sdef->DoubleAmmoFactor = 2.0f;
  sdef->DamageFactor = 1.0f;
  sdef->RespawnTime = 0.0f;
  sdef->RespawnLimit = 0;
  sdef->Aggressiveness = 1.0f;
  sdef->SpawnFilter = 0;
  sdef->AcsReturn = SkillDefs.Num()-1;
  sdef->MenuName.Clean();
  sdef->PlayerClassNames.Clear();
  sdef->ConfirmationText.Clean();
  sdef->Key.Clean();
  sdef->TextColor.Clean();
  sdef->Flags = 0;
  // if skill definition contains replacements, clear the old ones
  // k8: i am not sure if i should keep old replacements here, but why not?
  bool sdefClearReplacements = true;

  VClass *eexCls = VClass::FindClassNoCase("Actor"); // we'll need it later

  if (!sc->Check("{")) { ParseSkillDefOld(sc, sdef); return; }
  SCParseModeSaver msave(sc);

  while (!checkEndBracket(sc)) {
    if (sc->Check("AmmoFactor")) {
      sc->Expect("=");
      sc->ExpectFloat();
      sdef->AmmoFactor = sc->Float;
    } else if (sc->Check("DoubleAmmoFactor")) {
      sc->Expect("=");
      sc->ExpectFloat();
      sdef->DoubleAmmoFactor = sc->Float;
    } else if (sc->Check("DropAmmoFactor")) {
      sc->Expect("=");
      sc->ExpectFloat();
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill setting 'DropAmmoFactor' is not implemented yet.", *sc->GetLoc().toStringNoCol());
    } else if (sc->Check("DamageFactor")) {
      sc->Expect("=");
      sc->ExpectFloat();
      sdef->DamageFactor = sc->Float;
    } else if (sc->Check("RespawnTime")) {
      sc->Expect("=");
      sc->ExpectFloat();
      sdef->RespawnTime = sc->Float;
    } else if (sc->Check("RespawnLimit")) {
      sc->Expect("=");
      sc->ExpectNumber();
      sdef->RespawnLimit = sc->Number;
    } else if (sc->Check("Aggressiveness")) {
      sc->Expect("=");
      sc->ExpectFloatWithSign();
      if (sc->Float < 0) GCon->Logf(NAME_Warning, "%s:MAPINFO: \"Aggressiveness\" should be positive", *sc->GetLoc().toStringNoCol());
      sdef->Aggressiveness = 1.0f-midval(0.0f, (float)sc->Float, 1.0f);
    } else if (sc->Check("SpawnFilter")) {
      sc->Expect("=");
      if (sc->CheckNumber()) {
        if (sc->Number > 0 && sc->Number < 31) {
          sdef->SpawnFilter = 1<<(sc->Number-1);
        } else {
          GCon->Logf(NAME_Warning, "MAPINFO:%s: invalid spawnfilter value %d", *sc->GetLoc().toStringNoCol(), sc->Number);
        }
      } else {
             if (sc->Check("Baby")) sdef->SpawnFilter = 1;
        else if (sc->Check("Easy")) sdef->SpawnFilter = 2;
        else if (sc->Check("Normal")) sdef->SpawnFilter = 4;
        else if (sc->Check("Hard")) sdef->SpawnFilter = 8;
        else if (sc->Check("Nightmare")) sdef->SpawnFilter = 16;
        else { sc->ExpectString(); GCon->Logf(NAME_Warning, "MAPINFO:%s: unknown spawnfilter '%s'", *sc->GetLoc().toStringNoCol(), *sc->String); }
      }
    } else if (sc->Check("ACSReturn")) {
      sc->Expect("=");
      sc->ExpectNumber();
      sdef->AcsReturn = sc->Number;
    } else if (sc->Check("Key")) {
      sc->Expect("=");
      sc->ExpectString();
      sdef->Key = sc->String;
    } else if (sc->Check("MustConfirm")) {
      sdef->Flags |= SKILLF_MustConfirm;
      if (sc->Check("=")) {
        sc->ExpectString();
        sdef->ConfirmationText = sc->String;
      }
    } else if (sc->Check("Name")) {
      sc->Expect("=");
      sc->ExpectString();
      sdef->MenuName = sc->String;
      sdef->Flags &= ~SKILLF_MenuNameIsPic;
    } else if (sc->Check("PlayerClassName")) {
      sc->Expect("=");
      VSkillPlayerClassName &CN = sdef->PlayerClassNames.Alloc();
      sc->ExpectString();
      CN.ClassName = sc->String;
      sc->Expect(",");
      sc->ExpectString();
      CN.MenuName = sc->String;
    } else if (sc->Check("PicName")) {
      sc->Expect("=");
      sc->ExpectString();
      sdef->MenuName = sc->String.ToLower();
      sdef->Flags |= SKILLF_MenuNameIsPic;
    } else if (sc->Check("TextColor")) {
      sc->Expect("=");
      sc->ExpectString();
      sdef->TextColor = sc->String;
    } else if (sc->Check("EasyBossBrain")) {
      sdef->Flags |= SKILLF_EasyBossBrain;
    } else if (sc->Check("EasyKey")) {
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'EasyKey' is not implemented yet.", *sc->GetLoc().toStringNoCol());
    } else if (sc->Check("FastMonsters")) {
      sdef->Flags |= SKILLF_FastMonsters;
    } else if (sc->Check("SlowMonsters")) {
      sdef->Flags |= SKILLF_SlowMonsters;
    } else if (sc->Check("DisableCheats")) {
      //k8: no, really?
      //sdef->Flags |= SKILLF_DisableCheats;
    } else if (sc->Check("AutoUseHealth")) {
      sdef->Flags |= SKILLF_AutoUseHealth;
    } else if (sc->Check("ReplaceActor")) {
      //GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'ReplaceActor' is not implemented yet.", *sc->GetLoc().toStringNoCol());
      sc->Expect("=");
      sc->ExpectString();
      VStr oldCN = sc->String;
      sc->Expect(",");
      sc->ExpectString();
      VStr newCN = sc->String;
      if (sdefClearReplacements) { sdef->Replacements.clear(); sdefClearReplacements = false; }
      if (eexCls && !oldCN.isEmpty()) {
        VClass *oldCls = VClass::FindClassNoCase(*oldCN);
        if (!oldCls) oldCls = VClass::FindClassNoCase(*oldCN.xstrip());
        if (!oldCls || !oldCls->IsChildOf(eexCls)) {
          GCon->Logf(NAME_Warning, "MAPINFO:%s: source class `%s` in 'ReplaceActor' is invalid.", *sc->GetLoc().toStringNoCol(), *oldCN);
        } else {
          // source is ok, check destination
          VClass *newCls = nullptr;
          bool newIsValid = true;
          if (!newCN.isEmpty() && newCN.xstrip().isEmpty()) newCN.clear(); // some morons are using a space as "nothing"
          if (!newCN.isEmpty() && !newCN.xstrip().strEquCI("none") && !newCN.xstrip().strEquCI("null")) {
            newCls = VClass::FindClassNoCase(*newCN);
            if (!newCls) newCls = VClass::FindClassNoCase(*newCN.xstrip());
            if (!newCls || !newCls->IsChildOf(eexCls)) {
              GCon->Logf(NAME_Warning, "MAPINFO:%s: destination class `%s` for source class `%s` in 'ReplaceActor' is invalid.", *sc->GetLoc().toStringNoCol(), *newCN, *oldCN);
              newIsValid = false;
            }
          }
          if (newIsValid) {
            VSkillMonsterReplacement &rp = sdef->Replacements.alloc();
            rp.oldClass = oldCls;
            rp.newClass = newCls;
          }
        }
      }
    } else if (sc->Check("MonsterHealth")) {
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'MonsterHealth' is not implemented yet.", *sc->GetLoc().toStringNoCol());
      sc->Expect("=");
      sc->ExpectFloat();
    } else if (sc->Check("FriendlyHealth")) {
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'FriendlyHealth' is not implemented yet.", *sc->GetLoc().toStringNoCol());
      sc->Expect("=");
      sc->ExpectFloat();
    } else if (sc->Check("NoPain")) {
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'NoPain' is not implemented yet.", *sc->GetLoc().toStringNoCol());
    } else if (sc->Check("DefaultSkill")) {
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'DefaultSkill' is not implemented yet.", *sc->GetLoc().toStringNoCol());
    } else if (sc->Check("ArmorFactor")) {
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'ArmorFactor' is not implemented yet.", *sc->GetLoc().toStringNoCol());
      sc->Expect("=");
      sc->ExpectFloat();
    } else if (sc->Check("NoInfighting")) {
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'NoInfighting' is not implemented yet.", *sc->GetLoc().toStringNoCol());
    } else if (sc->Check("TotalInfighting")) {
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'TotalInfighting' is not implemented yet.", *sc->GetLoc().toStringNoCol());
    } else if (sc->Check("HealthFactor")) {
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'HealthFactor' is not implemented yet.", *sc->GetLoc().toStringNoCol());
      sc->Expect("=");
      sc->ExpectFloat();
    } else if (sc->Check("KickbackFactor")) {
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'KickbackFactor' is not implemented yet.", *sc->GetLoc().toStringNoCol());
      sc->Expect("=");
      sc->ExpectFloat();
    } else if (sc->Check("NoMenu")) {
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'NoMenu' is not implemented yet.", *sc->GetLoc().toStringNoCol());
    } else if (sc->Check("PlayerRespawn")) {
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'PlayerRespawn' is not implemented yet.", *sc->GetLoc().toStringNoCol());
    } else if (sc->Check("ReplaceActor")) {
      GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'ReplaceActor' is not implemented yet.", *sc->GetLoc().toStringNoCol());
      sc->Expect("=");
      while (!sc->AtEnd()) {
        sc->ExpectString();
        if (!sc->Check(",")) break;
      }
    } else {
      sc->ExpectString();
      sc->Error(va("unknown MAPINFO skill command '%s'", *sc->String));
      break;
    }
  }

  if (sdef->SpawnFilter == 0) {
    GCon->Logf(NAME_Warning, "MAPINFO:%s: skill param 'SpawnFilter' is not set for skill '%s'; assume UV.", *sc->GetLoc().toStringNoCol(), *sdef->MenuName);
    sdef->SpawnFilter = 8; // UV
  }
}


//==========================================================================
//
//  ParseGameInfo
//
//==========================================================================
static void ParseGameInfo (VScriptParser *sc) {
  //auto cmode = sc->IsCMode();
  //sc->SetCMode(true);
  sc->Expect("{");
  //sc->SkipBracketed(true);
  for (;;) {
    if (sc->AtEnd()) { sc->Message("'}' not found"); break; }
    if (sc->Check("}")) break;
    if (sc->Check("PlayerClasses")) {
      MapInfoPlayerClasses.clear();
      sc->Expect("=");
      while (!sc->AtEnd()) {
        sc->ExpectString();
        if (sc->String.length()) {
          if (!FL_IsIgnoredPlayerClass(sc->String)) {
            MapInfoPlayerClasses.append(VName(*sc->String));
          } else {
            GCon->Logf(NAME_Init, "mapinfo player class '%s' ignored due to mod detector/mode orders", *sc->String);
          }
        }
        if (!sc->Check(",")) break;
      }
    } else if (sc->Check("weaponslot")) {
      sc->Expect("=");
      sc->ExpectNumber();
      int slot = sc->Number;
      if (slot < 0 || slot > 10) sc->Message(va("ignoring gameinfo weaponslot %d", slot));
      TArray<VStr> clist;
      clist.append("CmdSetSlot"); // arg0
      clist.append(VStr(slot)); // arg1 is number
      while (sc->Check(",")) {
        if (!sc->GetString()) sc->Error("unexpected gameinfo end in mapinfo");
        if (!sc->String.isEmpty()) clist.append(sc->String);
      }
      // we only have so many slots
      if (slot >= 0 && slot <= 10) {
        GGameInfo->eventCmdSetSlot(&clist, false); // as gameinfo
      }
    } else if (sc->Check("ForceKillScripts")) {
      sc->Expect("=");
      bool bval = ExpectBool("ForceKillScripts", sc);
      //mapInfoGameInfoInitial.bForceKillScripts = bval;
      if (bval) GGameInfo->Flags |= VGameInfo::GIF_ForceKillScripts; else GGameInfo->Flags &= ~VGameInfo::GIF_ForceKillScripts;
    } else if (sc->Check("SkyFlatName")) {
      sc->Expect("=");
      sc->ExpectString();
      if (sc->String.length() == 0 || sc->String.length() > 8) {
        miWarning(sc, "invalid sky flat name: '%s'", *sc->String);
      } else {
        GTextureManager.SetSkyFlatName(sc->String);
        SV_UpdateSkyFlat();
      }
    } else {
      sc->ExpectString();
      sc->Message(va("skipped gameinfo command '%s'", *sc->String));
      sc->Expect("=");
      sc->ExpectString();
      while (sc->Check(",")) {
        if (!sc->GetString()) sc->Error("unexpected gameinfo end in mapinfo");
      }
    }
  }
  //sc->SetCMode(cmode);
}


//==========================================================================
//
//  ParseDamageType
//
//==========================================================================
static void ParseDamageType (VScriptParser *sc) {
  sc->ExpectString(); // name
  if (sc->String.strEquCI("Normal")) sc->String = "None";
  VStr dfname = sc->String;
  sc->Expect("{");

  float factor = 1.0f;
  bool noarmor = false;
  bool replace = false;
  while (!checkEndBracket(sc)) {
    if (sc->Check("NoArmor")) {
      noarmor = true;
      continue;
    }
    if (sc->Check("ReplaceFactor")) {
      replace = true;
      continue;
    }
    if (sc->Check("Factor")) {
      sc->Expect("=");
      sc->ExpectFloat();
      factor = sc->Float;
      continue;
    }
    //FIXME: implement this!
    if (sc->Check("Obituary")) {
      sc->Expect("=");
      sc->ExpectString();
      continue;
    }
    sc->Error(va("unknown DamageType field '%s'", *sc->String));
  }

  // add or replace damage type
  VDamageFactor *fc = nullptr;
  for (auto &&df : CustomDamageFactors) {
    if (dfname.strEquCI(*df.DamageType)) {
      fc = &df;
      break;
    }
  }

  if (!fc) {
    fc = &CustomDamageFactors.alloc();
    memset((void *)fc, 0, sizeof(VDamageFactor));
    fc->DamageType = VName(*dfname);
  }

  fc->Factor = factor;
  if (replace) fc->Flags |= VDamageFactor::DF_REPLACE;
  if (noarmor) fc->Flags |= VDamageFactor::DF_NOARMOR;

  hasCustomDamageFactors = true;
}


//==========================================================================
//
//  ParseMapInfo
//
//==========================================================================
static void ParseMapInfo (VScriptParser *sc, int milumpnum) {
  const unsigned int MaxStack = 64;
  bool HexenMode = false;
  VScriptParser *scstack[MaxStack];
  unsigned int scsp = 0;
  bool error = false;

  // set up default map info
  VMapInfo Default;
  SetMapDefaults(Default);

  for (;;) {
    while (!sc->AtEnd()) {
      if (sc->Check("map")) {
        ParseMap(sc, HexenMode, Default, milumpnum);
      } else if (sc->Check("defaultmap")) {
        SetMapDefaults(Default);
        ParseMapCommon(sc, &Default, HexenMode);
      } else if (sc->Check("adddefaultmap")) {
        ParseMapCommon(sc, &Default, HexenMode);
      } else if (sc->Check("clusterdef")) {
        ParseClusterDef(sc);
      } else if (sc->Check("cluster")) {
        ParseClusterDef(sc);
      } else if (sc->Check("episode")) {
        ParseEpisodeDef(sc, milumpnum);
      } else if (sc->Check("clearepisodes")) {
        // clear episodes and clusterdefs
        EpisodeDefs.Clear();
        ClusterDefs.Clear();
      } else if (sc->Check("skill")) {
        ParseSkillDef(sc);
      } else if (sc->Check("clearskills")) {
        SkillDefs.Clear();
      } else if (sc->Check("include")) {
        sc->ExpectString();
        int lmp = W_CheckNumForFileName(sc->String);
        if (lmp >= 0) {
          if (scsp >= MaxStack) {
            sc->Error(va("mapinfo include nesting too deep"));
            error = true;
            break;
          }
          GCon->Logf(NAME_Init, "Including '%s'", *sc->String);
          scstack[scsp++] = sc;
          sc = new VScriptParser(/**sc->String*/W_FullLumpName(lmp), W_CreateLumpReaderNum(lmp));
          //ParseMapInfo(new VScriptParser(*sc->String, W_CreateLumpReaderNum(lmp)));
        } else {
          sc->Error(va("mapinfo include '%s' not found", *sc->String));
          error = true;
          break;
        }
      } else if (sc->Check("gameinfo")) {
        ParseGameInfo(sc);
      } else if (sc->Check("damagetype")) {
        ParseDamageType(sc);
        /*
        sc->Message("Unimplemented MAPINFO command `DamageType`");
        if (!sc->Check("{")) {
          sc->ExpectString();
          sc->SkipBracketed();
        } else {
          sc->SkipBracketed(true); // bracket eaten
        }
        */
      } else if (sc->Check("GameSkyFlatName")) {
        if (sc->Check("=")) {} // just in case
        sc->ExpectString();
        if (sc->String.length() == 0 || sc->String.length() > 8) {
          miWarning(sc, "invalid sky flat name: '%s'", *sc->String);
        } else {
          GTextureManager.SetSkyFlatName(sc->String);
          SV_UpdateSkyFlat();
        }
      } else if (sc->Check("intermission")) {
        sc->Message("Unimplemented MAPINFO command `Intermission`");
        if (!sc->Check("{")) {
          sc->ExpectString();
          sc->SkipBracketed();
        } else {
          sc->SkipBracketed(true); // bracket eaten
        }
      /*
      } else if (sc->Check("gamedefaults")) {
        GCon->Logf(NAME_Warning, "Unimplemented MAPINFO section gamedefaults");
        sc->SkipBracketed();
      } else if (sc->Check("automap")) {
        GCon->Logf(NAME_Warning, "Unimplemented MAPINFO command Automap");
        sc->SkipBracketed();
      } else if (sc->Check("automap_overlay")) {
        GCon->Logf(NAME_Warning, "Unimplemented MAPINFO command automap_overlay");
        sc->SkipBracketed();
      */
      } else if (sc->Check("DoomEdNums")) {
        //GCon->Logf(NAME_Debug, "*** <%s> ***", *sc->String);
        //auto cmode = sc->IsCMode();
        //sc->SetCMode(true);
        sc->Expect("{");
        for (;;) {
          if (checkEndBracket(sc)) break;
          sc->ExpectNumber();
          int num = sc->Number;
          sc->Expect("=");
          sc->ExpectString();
          VStr clsname = sc->String;
          int flags = 0;
          int special = 0;
          int args[5] = {0, 0, 0, 0, 0};
          if (sc->Check(",")) {
            auto loc = sc->GetLoc();
            bool doit = true;
            if (sc->Check("noskillflags")) { flags |= mobjinfo_t::FlagNoSkill; doit = sc->Check(","); }
            if (doit) {
              flags |= mobjinfo_t::FlagSpecial;
              sc->ExpectString();
              VStr spcname = sc->String;
              // no name?
              int argn = 0;
              int arg0 = 0;
              if (VStr::convertInt(*spcname, &arg0)) {
                spcname = VStr();
                special = -1;
                args[argn++] = arg0;
              }
              while (sc->Check(",")) {
                sc->ExpectNumber(true); // with sign, why not
                if (argn < 5) args[argn] = sc->Number;
                ++argn;
              }
              if (argn > 5) GCon->Logf(NAME_Warning, "MAPINFO:%s: too many arguments (%d) to special '%s'", *loc.toStringNoCol(), argn, *spcname);
              // find special number
              if (special == 0) {
                for (int sdx = 0; sdx < LineSpecialInfos.length(); ++sdx) {
                  if (LineSpecialInfos[sdx].Name.ICmp(spcname) == 0) {
                    special = LineSpecialInfos[sdx].Number;
                    break;
                  }
                }
              }
              if (!special) {
                flags &= ~mobjinfo_t::FlagSpecial;
                GCon->Logf(NAME_Warning, "MAPINFO:%s: special '%s' not found", *loc.toStringNoCol(), *spcname);
              }
              if (special == -1) special = 0; // special special ;-)
            }
          }
          //GCon->Logf(NAME_Debug, "MAPINFO: DOOMED: '%s', %d (%d)", *clsname, num, flags);
          appendNumFixup(DoomEdNumFixups, clsname, num, flags, special, args[0], args[1], args[2], args[3], args[4]);
        }
        //sc->SetCMode(cmode);
      } else if (sc->Check("SpawnNums")) {
        //auto cmode = sc->IsCMode();
        //sc->SetCMode(true);
        sc->Expect("{");
        for (;;) {
          if (checkEndBracket(sc)) break;
          sc->ExpectNumber();
          int num = sc->Number;
          sc->Expect("=");
          sc->ExpectString();
          appendNumFixup(SpawnNumFixups, VStr(sc->String), num);
        }
        //sc->SetCMode(cmode);
      } else if (sc->Check("author")) {
        sc->ExpectString();
      } else {
        VStr cmdname = sc->String;
        sc->ExpectString();
        if (sc->Check("{")) {
          GCon->Logf(NAME_Warning, "Unimplemented MAPINFO command '%s'", *cmdname);
          sc->SkipBracketed(true); // bracket eaten
        } else if (!sc->String.strEquCI("}")) { // some mappers cannot into mapinfo
          sc->Error(va("Invalid command '%s'", *sc->String));
          error = true;
          break;
        } else {
          sc->Message(va("Some mapper cannot into proper mapinfo (stray '%s')", *sc->String));
        }
      }
    }
    if (error) {
      while (scsp > 0) { delete sc; sc = scstack[--scsp]; }
      break;
    }
    if (scsp == 0) break;
    GCon->Logf(NAME_Init, "Finished included '%s'", *sc->GetLoc().GetSource());
    delete sc;
    sc = scstack[--scsp];
  }
  delete sc;
  sc = nullptr;
}


//==========================================================================
//
//  ParseUMapinfo
//
//==========================================================================
static void ParseUMapinfo (VScriptParser *sc, int milumpnum) {
  // set up default map info
  VMapInfo Default;
  SetMapDefaults(Default);
  bool HexenMode = false;

  while (!sc->AtEnd()) {
    if (sc->Check("map")) {
      ParseMap(sc, HexenMode, Default, milumpnum, true);
    } else {
      sc->Error(va("Unknown UMAPINFO key '%s'", *sc->String));
    }
  }

  delete sc;
}


//==========================================================================
//
//  QualifyMap
//
//==========================================================================
static int QualifyMap (int map) {
  return (map < 0 || map >= MapInfo.Num() ? 0 : map);
}


//==========================================================================
//
//  P_GetMapInfo
//
//==========================================================================
const VMapInfo &P_GetMapInfo (VName map) {
  for (int i = 0; i < MapInfo.Num(); ++i) {
    if (map == MapInfo[i].LumpName) return MapInfo[i];
  }
  return DefaultMap;
}


//==========================================================================
//
//  P_GetMapName
//
//==========================================================================
VStr P_GetMapName (int map) {
  return MapInfo[QualifyMap(map)].GetName();
}


//==========================================================================
//
//  P_GetMapInfoIndexByLevelNum
//
//  Returns map info index given a level number
//
//==========================================================================
int P_GetMapIndexByLevelNum (int map) {
  for (int i = 0; i < MapInfo.Num(); ++i) {
    if (MapInfo[i].LevelNum == map) return i;
  }
  // not found
  return 0;
}


//==========================================================================
//
//  P_GetMapLumpName
//
//==========================================================================
VName P_GetMapLumpName (int map) {
  return MapInfo[QualifyMap(map)].LumpName;
}


//==========================================================================
//
//  P_TranslateMap
//
//  Returns the map lump name given a warp map number.
//
//==========================================================================
VName P_TranslateMap (int map) {
  for (int i = MapInfo.length()-1; i >= 0; --i) {
    if (MapInfo[i].WarpTrans == map) return MapInfo[i].LumpName;
  }
  // not found
  return (MapInfo.length() > 0 ? MapInfo[0].LumpName : NAME_None);
}


//==========================================================================
//
//  P_TranslateMapEx
//
//  Returns the map lump name given a warp map number.
//
//==========================================================================
VName P_TranslateMapEx (int map) {
  for (int i = MapInfo.length()-1; i >= 0; --i) {
    if (MapInfo[i].WarpTrans == map) return MapInfo[i].LumpName;
  }
  // not found
  return NAME_None;
}


//==========================================================================
//
//  P_GetMapLumpNameByLevelNum
//
//  Returns the map lump name given a level number.
//
//==========================================================================
VName P_GetMapLumpNameByLevelNum (int map) {
  for (int i = 0; i < MapInfo.Num(); ++i) {
    if (MapInfo[i].LevelNum == map) return MapInfo[i].LumpName;
  }
  // not found, use map##
  return va("map%02d", map);
}


//==========================================================================
//
// P_PutMapSongLump
//
//==========================================================================
void P_PutMapSongLump (int map, VName lumpName) {
  FMapSongInfo &ms = MapSongList.Alloc();
  ms.MapName = va("map%02d", map);
  ms.SongName = lumpName;
}


//==========================================================================
//
//  P_GetClusterDef
//
//==========================================================================
const VClusterDef *P_GetClusterDef (int Cluster) {
  for (int i = 0; i < ClusterDefs.Num(); ++i) {
    if (Cluster == ClusterDefs[i].Cluster) return &ClusterDefs[i];
  }
  return &DefaultClusterDef;
}


//==========================================================================
//
//  P_GetNumEpisodes
//
//==========================================================================
int P_GetNumEpisodes () {
  return EpisodeDefs.Num();
}


//==========================================================================
//
//  P_GetNumMaps
//
//==========================================================================
int P_GetNumMaps () {
  return MapInfo.Num();
}


//==========================================================================
//
//  P_GetMapInfo
//
//==========================================================================
VMapInfo *P_GetMapInfoPtr (int mapidx) {
  return (mapidx >= 0 && mapidx < MapInfo.Num() ? &MapInfo[mapidx] : nullptr);
}


//==========================================================================
//
//  P_GetEpisodeDef
//
//==========================================================================
VEpisodeDef *P_GetEpisodeDef (int Index) {
  return &EpisodeDefs[Index];
}


//==========================================================================
//
//  P_GetNumSkills
//
//==========================================================================
int P_GetNumSkills () {
  return SkillDefs.Num();
}


//==========================================================================
//
//  P_GetSkillDef
//
//==========================================================================
const VSkillDef *P_GetSkillDef (int Index) {
  return &SkillDefs[Index];
}


//==========================================================================
//
//  P_GetMusicLumpNames
//
//==========================================================================
void P_GetMusicLumpNames (TArray<FReplacedString> &List) {
  for (int i = 0; i < MapInfo.Num(); ++i) {
    const char *MName = *MapInfo[i].SongLump;
    if (MName[0] == 'd' && MName[1] == '_') {
      FReplacedString &R = List.Alloc();
      R.Index = i;
      R.Replaced = false;
      R.Old = MName+2;
    }
  }
}


//==========================================================================
//
//  P_ReplaceMusicLumpNames
//
//==========================================================================
void P_ReplaceMusicLumpNames (TArray<FReplacedString> &List) {
  for (int i = 0; i < List.Num(); ++i) {
    if (List[i].Replaced) {
      MapInfo[List[i].Index].SongLump = VName(*(VStr("d_")+List[i].New), VName::AddLower8);
    }
  }
}


//==========================================================================
//
//  P_SetParTime
//
//==========================================================================
void P_SetParTime (VName Map, int Par) {
  if (Map == NAME_None || Par < 0) return;
  if (mapinfoParsed) {
    for (int i = 0; i < MapInfo.length(); ++i) {
      if (MapInfo[i].LumpName == NAME_None) continue;
      if (VStr::ICmp(*MapInfo[i].LumpName, *Map) == 0) {
        MapInfo[i].ParTime = Par;
        return;
      }
    }
    GCon->Logf(NAME_Warning, "No such map '%s' for par time", *Map);
  } else {
    ParTimeInfo &pi = partimes.alloc();
    pi.MapName = Map;
    pi.par = Par;
  }
}


//==========================================================================
//
//  IsMapPresent
//
//==========================================================================
bool IsMapPresent (VName MapName) {
  if (W_CheckNumForName(MapName) >= 0) return true;
  VStr FileName = va("maps/%s.wad", *MapName);
  if (FL_FileExists(FileName)) return true;
  return false;
}


//==========================================================================
//
//  COMMAND MapList
//
//==========================================================================
COMMAND(MapList) {
  for (int i = 0; i < MapInfo.Num(); ++i) {
    if (IsMapPresent(MapInfo[i].LumpName)) {
      GCon->Log(VStr(MapInfo[i].LumpName)+" - "+(MapInfo[i].Flags&VLevelInfo::LIF_LookupName ? GLanguage[*MapInfo[i].Name] : MapInfo[i].Name));
    }
  }
}


//==========================================================================
//
//  ShutdownMapInfo
//
//==========================================================================
void ShutdownMapInfo () {
  DefaultMap.Name.Clean();
  MapInfo.Clear();
  MapSongList.Clear();
  ClusterDefs.Clear();
  EpisodeDefs.Clear();
  SkillDefs.Clear();
}
