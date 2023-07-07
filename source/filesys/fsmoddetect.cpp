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
// directly included from "files.cpp"
/*
enum {
  //AD_NONE,
  AD_SKULLDASHEE = 1,
  AD_HARMONY,
  AD_SQUARE,
  AD_HMOON,
};
*/

VStr flWarningMessage;
int flACSType = FL_ACS_Default;


struct VDetectorInfo {
public:
  struct FileLump {
    VStr name;
    int size; // -1: don't care
    VStr md5; // empty: don't care
    bool asLump; // check as lump?
  };

public:
  VStr name;
  bool showMessage;
  VStr warningMessage;
  VStr type;
  // preconditions
  int zscriptLumpCheck; // -1: don't care
  VStr gameTitle; // empty: don't care
  TArray<FileLump> reqiredContent;
  // resulting options
  VStr addmod;
  bool nomore;
  VStr gamename; // empty: don't force
  bool nakedbase;
  int bdw; // -1: don't care
  int gore; // -1: don't care
  int modblood; // -1: "nogore"
  bool iwad;
  bool sprofslump;
  bool ignorePlayerSpeed;
  int acsType;
  TArray<VStr> ignorePlayerClasses;

public:
  inline VDetectorInfo () noexcept
    : name()
    , showMessage(true)
    , warningMessage()
    , type("mod")
    //
    , zscriptLumpCheck(-1)
    , gameTitle()
    , reqiredContent()
    //
    , addmod()
    , nomore(false)
    , gamename()
    , nakedbase(false)
    , bdw(-1)
    , gore(-1)
    , modblood(1)
    , iwad(true)
    , sprofslump(true)
    , ignorePlayerSpeed(false)
    , acsType(FL_ACS_Default)
    , ignorePlayerClasses()
  {}

  // "{" just eaten
  void parseContent (VScriptParser *sc);

  void clearPre () {
    zscriptLumpCheck = -1;
    gameTitle.clear();
    reqiredContent.clear();
  }

  void clearOpts () {
    addmod.clear();
    nomore = false;
    gamename.clear();
    nakedbase = false;
    bdw = -1;
    gore = -1;
    modblood = 1;
    iwad = true;
    sprofslump = true;
    ignorePlayerSpeed = false;
    acsType = FL_ACS_Default;
    ignorePlayerClasses.clear();
  }

private:
  void parseInfo (VScriptParser *sc);
  void parsePre (VScriptParser *sc);
  void parseOpts (VScriptParser *sc);

  // -1: third option
  static int parseBool (VScriptParser *sc, const char *third=nullptr);
  static VStr parseString (VScriptParser *sc);
  static int parseInt (VScriptParser *sc);
  static int parseACSType (VScriptParser *sc);
  static void parseStringList (VScriptParser *sc, TArray<VStr> &list);

  void parseFileLump (VScriptParser *sc, bool asLump);
};


static TArray<VDetectorInfo *> detectorList;
static TMap<VStrCI, bool> ignoredPlayerClassesMap;


//==========================================================================
//
//  VDetectorInfo::parseBool
//
//==========================================================================
int VDetectorInfo::parseBool (VScriptParser *sc, const char *third) {
  sc->Expect("=");
  if (sc->Check("tan") || sc->Check("true")) { sc->Expect(";"); return 1; }
  if (sc->Check("ona") || sc->Check("false")) { sc->Expect(";"); return 0; }
  if (third) {
    if (sc->Check(third)) { sc->Expect(";"); return -1; }
  }
  sc->Error(va("boolean value expected, got '%s' instead", *sc->String));
  abort(); // unreachable code
}


//==========================================================================
//
//  VDetectorInfo::parseString
//
//==========================================================================
VStr VDetectorInfo::parseString (VScriptParser *sc) {
  sc->Expect("=");
  sc->ExpectString();
  VStr res = sc->String;
  sc->Expect(";");
  return res;
}


//==========================================================================
//
//  VDetectorInfo::parseInt
//
//==========================================================================
int VDetectorInfo::parseInt (VScriptParser *sc) {
  sc->Expect("=");
  sc->ExpectNumber();
  int res = sc->Number;
  sc->Expect(";");
  return res;
}


//==========================================================================
//
//  VDetectorInfo::parseStringList
//
//==========================================================================
void VDetectorInfo::parseStringList (VScriptParser *sc, TArray<VStr> &list) {
  sc->Expect("=");
  for (;;) {
    if (sc->Check(";")) return;
    sc->ExpectString();
    list.append(sc->String);
    if (!sc->Check(",")) break;
  }
  sc->Expect(";");
}


//==========================================================================
//
//  VDetectorInfo::parseFileLump
//
//==========================================================================
void VDetectorInfo::parseFileLump (VScriptParser *sc, bool asLump) {
  sc->Expect("{");
  FileLump fli;
  fli.name = VStr::EmptyString;
  fli.size = -1;
  fli.md5 = VStr::EmptyString;
  fli.asLump = asLump;
  while (!sc->Check("}")) {
         if (sc->Check("name")) fli.name = parseString(sc);
    else if (sc->Check("size")) fli.size = parseInt(sc);
    else if (sc->Check("md5")) fli.md5 = parseString(sc);
    else sc->Error(va("unknown %s command '%s'", (asLump ? "lump" : "file"), *sc->String));
  }

  if (fli.name.isEmpty()) sc->Error(va("required %s has no name", (asLump ? "lump" : "file")));
  if (!fli.md5.isEmpty() && fli.md5.length() != 32) sc->Error(va("required %s '%s' has invalid md5 '%s'", (asLump ? "lump" : "file"), *fli.name, *fli.md5));

  reqiredContent.append(fli);
}


//==========================================================================
//
//  VDetectorInfo::parseInfo
//
//==========================================================================
void VDetectorInfo::parseInfo (VScriptParser *sc) {
  sc->Expect("{");
  while (!sc->Check("}")) {
         if (sc->Check("showmessage")) showMessage = !!parseBool(sc);
    else if (sc->Check("warning")) warningMessage = parseString(sc);
    else if (sc->Check("type")) type = parseString(sc);
    else sc->Error(va("unknown info command '%s'", *sc->String));
  }
}


//==========================================================================
//
//  VDetectorInfo::parsePre
//
//==========================================================================
void VDetectorInfo::parsePre (VScriptParser *sc) {
  if (sc->Check("clear")) clearPre();
  sc->Expect("{");
  while (!sc->Check("}")) {
         if (sc->Check("zscript")) zscriptLumpCheck = parseBool(sc, "dontcare");
    else if (sc->Check("gametitle")) gameTitle = parseString(sc);
    else if (sc->Check("file")) parseFileLump(sc, false);
    else if (sc->Check("lump")) parseFileLump(sc, true);
    else sc->Error(va("unknown info command '%s'", *sc->String));
  }
}


//==========================================================================
//
//  VDetectorInfo::parseACSType
//
//==========================================================================
int VDetectorInfo::parseACSType (VScriptParser *sc) {
  int res = FL_ACS_Default;
  sc->Expect("=");
       if (sc->Check("default")) res = FL_ACS_Default;
  else if (sc->Check("zandronum") || sc->Check("zandro")) res = FL_ACS_Zandronum;
  else if (sc->Check("zdoom")) res = FL_ACS_ZDoom;
  else sc->Error(va("unknown asc type '%s'", *sc->String));
  sc->Expect(";");
  return res;
}


//==========================================================================
//
//  VDetectorInfo::parseOpts
//
//==========================================================================
void VDetectorInfo::parseOpts (VScriptParser *sc) {
  if (sc->Check("clear")) clearOpts();
  sc->Expect("{");
  while (!sc->Check("}")) {
         if (sc->Check("addmod")) addmod = parseString(sc);
    else if (sc->Check("nomore")) nomore = parseBool(sc);
    else if (sc->Check("gamename")) gamename = parseString(sc);
    else if (sc->Check("nakedbase")) nakedbase = parseBool(sc);
    else if (sc->Check("bdw")) bdw = parseBool(sc, "dontcare");
    else if (sc->Check("gore")) gore = parseBool(sc, "dontcare");
    else if (sc->Check("modblood")) modblood = parseBool(sc, "nogore");
    else if (sc->Check("iwad")) { iwad = parseBool(sc); if (!iwad) sprofslump = false; }
    else if (sc->Check("sprofslump")) sprofslump = parseBool(sc);
    else if (sc->Check("ignorePlayerSpeed")) ignorePlayerSpeed = parseBool(sc);
    else if (sc->Check("acstype")) acsType = parseACSType(sc);
    else if (sc->Check("ignoreplayerclasses")) parseStringList(sc, ignorePlayerClasses);
    else sc->Error(va("unknown options command '%s'", *sc->String));
  }
}


//==========================================================================
//
//  VDetectorInfo::parseContent
//
//  "{" just eaten
//
//==========================================================================
void VDetectorInfo::parseContent (VScriptParser *sc) {
  while (!sc->Check("}")) {
         if (sc->Check("info")) parseInfo(sc);
    else if (sc->Check("preconditions")) parsePre(sc);
    else if (sc->Check("options")) parseOpts(sc);
    else sc->Error(va("unknown detector command '%s'", *sc->String));
  }
}


//==========================================================================
//
//  FreeDetectors
//
//==========================================================================
static void FreeDetectors () {
  for (auto &&it : detectorList) delete it;
  detectorList.clear();
}


//==========================================================================
//
//  ParseDetectors
//
//==========================================================================
static void ParseDetectors (VStr name, bool inHomeDir) {
  if (name.isEmpty()) return;

  //GCon->Logf(NAME_Debug, "===<%s> : <%s>===", *name, *fl_configdir);
  if (name.isAbsolutePath()) {
    if (!Sys_FileExists(name)) return;
  } else {
    if (inHomeDir) {
      if (fl_configdir.IsNotEmpty() && Sys_FileExists(fl_configdir+"/"+name)) {
        name = fl_configdir+"/"+name;
      } else {
        return;
      }
    } else {
      if (Sys_FileExists(fl_basedir+"/"+name)) {
        name = fl_basedir+"/"+name;
      } else {
        return;
      }
    }
  }

  GCon->Logf(NAME_Init, "Parsing detectors file \"%s\"", *name);
  VScriptParser *sc = new VScriptParser(name, FL_OpenSysFileRead(name));
  sc->SetCMode(true);
  for (;;) {
    if (sc->Check("detector")) {
      const bool doExtend = sc->Check("extend");
      sc->ExpectString();
      VStr dname = sc->String;
      if (dname.isEmpty()) sc->Error("empty detector name");
      VDetectorInfo *dt = nullptr;
      int idx = 0;
      for (; idx < detectorList.length(); ++idx) if (detectorList[idx]->name.strEquCI(dname)) break;
      if (idx < detectorList.length()) {
        if (doExtend) {
          dt = detectorList[idx]; // reuse
        } else {
          // remove existing
          delete detectorList[idx];
          detectorList.removeAt(idx);
        }
      } else {
        if (doExtend) sc->Error(va("cannot extend unknown detector '%s'", *dname));
      }
      sc->Expect("{");
      if (!dt) {
        // append new
        dt = new VDetectorInfo();
        detectorList.append(dt);
      }
      dt->name = dname;
      dt->parseContent(sc);
      if (dt->gameTitle.isEmpty() && dt->reqiredContent.length() == 0) sc->Error(va("empty detector '%s'", *dname));
      continue;
    }
    if (!sc->GetString()) break;
    sc->Error(va("unknown detection section '%s'", *sc->String));
  }
  delete sc;
}


//==========================================================================
//
//  loadGameTitles
//
//==========================================================================
static void loadGameTitles (FSysModDetectorHelper &hlp, TArray<VStr> &titles) {
  int fidx = hlp.findLump("GAMEINFO");
  if (fidx < 0) return;
  if (hlp.getLumpSize(fidx) > 16384) return; // sanity check
  VStream *gi = hlp.createLumpReader(fidx);
  if (!gi) return;
  VScriptParser *sc = new VScriptParser("gameinfo", gi);
  sc->SetCMode(true);
  while (sc->GetString()) {
    if (!sc->String.strEquCI("STARTUPTITLE")) continue;
    if (!sc->Check("=")) continue;
    if (!sc->GetString()) break;
    if (!sc->String.isEmpty()) titles.append(sc->String);
  }
  delete sc;
}


//==========================================================================
//
//  detectFromList
//
//==========================================================================
static int detectFromList (FSysModDetectorHelper &hlp, int seenZScriptLump) {
  TArray<VStr> titles;
  bool titlescanned = false;
  int res = AD_NONE;

  for (auto &&dc : detectorList) {
    //GCon->Logf(NAME_Debug, "detecting '%s' (000)", *dc->name);
    // zscript?
    if (dc->zscriptLumpCheck == 0 && seenZScriptLump >= 0) continue;
    if (dc->zscriptLumpCheck > 0 && seenZScriptLump < 0) continue;
    //GCon->Logf(NAME_Debug, "detecting '%s' (001)", *dc->name);
    // check title
    if (!dc->gameTitle.isEmpty()) {
      if (!titlescanned) { titlescanned = true; loadGameTitles(hlp, titles); }
      bool found = false;
      for (auto &&title : titles) if (title.globMatchCI(dc->gameTitle)) { found = true; break; }
      //GCon->Logf(NAME_Debug, "*** TITLE: %s (%d)", *dc->gameTitle, (int)found);
      if (!found) continue;
    }
    // check required files
    bool failed = false;
    for (auto &&fli : dc->reqiredContent) {
      if (fli.asLump) {
        if (fli.name.stripExtension().strEquCI("zscript")) {
          if (seenZScriptLump < 0 || !hlp.checkLump(seenZScriptLump, fli.size, *fli.md5)) failed = true;
        } else {
          if (!hlp.hasLump(*fli.name, fli.size, *fli.md5)) failed = true;
        }
        //GCon->Logf(NAME_Debug, "detecting '%s' (002): lump=<%s>, size=%d; md5=<%s>; failed=%d", *dc->name, *fli.name, fli.size, *fli.md5, (int)failed);
      } else {
        if (!hlp.hasFile(*fli.name, fli.size, *fli.md5)) failed = true;
      }
      if (failed) break;
    }
    if (failed) continue;
    // hit! show hit message
    if (dc->showMessage) {
      if (dc->type.isEmpty()) {
        GCon->Logf(NAME_Init, "Detected: %s", *dc->name);
      } else {
        GCon->Logf(NAME_Init, "Detected %s: %s", *dc->type, *dc->name);
      }
    }
    if (!dc->warningMessage.isEmpty()) {
      if (!flWarningMessage.isEmpty()) flWarningMessage += "\n\n";
      flWarningMessage += dc->warningMessage;
    }
    // "detected", don't stop
    res = -1;
    // set options
    if (dc->acsType != FL_ACS_Default) flACSType = dc->acsType;
    if (!dc->addmod.isEmpty()) mdetect_AddMod(dc->addmod);
    if (!dc->gamename.isEmpty()) mdetect_SetGameName(dc->gamename);
    if (dc->nakedbase) mdetect_ClearAndBlockCustomModes();
    if (dc->bdw >= 0) { fsys_DisableBDW = (dc->bdw == 0); cli_BDWMod = dc->bdw; }
    if (dc->gore >= 0) {
      if (cli_GoreMod != dc->gore) GCon->Logf(NAME_Init, "Gore mod %s.", (dc->gore ? "enabled" : "disabled"));
      cli_GoreMod = dc->gore;
    }
    if (dc->modblood <= 0) {
      if (dc->modblood < 0) dc->modblood = (cli_GoreMod == 0);
      fsys_DisableBloodReplacement = (dc->modblood == 0);
    }
    if (!dc->iwad) mdetect_DisableIWads();
    if (!dc->sprofslump) modNoBaseSprOfs = true;
    if (dc->ignorePlayerSpeed) decoIgnorePlayerSpeed = true;
    for (auto &&cn : dc->ignorePlayerClasses) ignoredPlayerClassesMap.put(cn, true);
    if (dc->nomore) return 1; // detected, stop
  }

  return res;
}


//==========================================================================
//
//  detectCzechbox
//
//==========================================================================
static int detectCzechbox (FSysModDetectorHelper &hlp, int /*seenZScriptLump*/) {
  if (hlp.hasLump("dehacked", 1066, "6bf56571d1f34d7cd7378b95556d67f8")) {
    GLog.Log(NAME_Init, "Detected PWAD: CzechBox release");
    return AD_NONE;
  }
  if (hlp.hasLump("dehacked", 1072, "b93dbb8163e0a512e7b76d60d885b41c")) {
    GLog.Log(NAME_Init, "Detected PWAD: CzechBox beta");
    return AD_NONE;
  }
  return AD_NONE;
}


//==========================================================================
//
//  detectMapinfoZScript
//
//  if we have UMAPINFO, or MAPINFO in old format,
//  assume that zscript can be ignored
//
//==========================================================================
static int detectMapinfoZScript (FSysModDetectorHelper &hlp, int seenZScriptLump) {
  if (seenZScriptLump < 0) return AD_NONE;

  if (hlp.hasLump("umapinfo")) {
    GCon->Log(NAME_Init, "zscript allowed due to UMAPINFO presence");
    return AD_ALLOW_ZSCRIPT;
  }

  if (hlp.hasLump("vmapinfo")) {
    GCon->Log(NAME_Init, "zscript allowed due to VMAPINFO presence");
    return AD_ALLOW_ZSCRIPT;
  }

  int lumpidx = hlp.findLump("mapinfo");
  if (lumpidx < 0) return AD_NONE;

  // if we have both ZMAPINFO and MAPINFO, assume that zshit can be ignored
  if (hlp.hasLump("zmapinfo")) {
    GCon->Log(NAME_Init, "zscript allowed due to both MAPINFO and ZMAPINFO presence");
    return AD_ALLOW_ZSCRIPT;
  }

  VStream *rd = hlp.createLumpReader(lumpidx);
  if (!rd) return AD_NONE;

  bool newFormat = false;
  VScriptParser *par = new VScriptParser("mapinfo", rd);
  while (par->GetString()) {
    if (!newFormat && par->String == "{") {
      newFormat = true;
    } else if (par->String.strEquCI("CanIgnoreZScript")) {
      bool allow = true;
      if (par->Check("=")) allow = par->Check("true");
      if (allow) {
        GCon->Log(NAME_Init, "zscript allowed due to MAPINFO 'CanIgnoreZScript' option");
        delete par;
        return AD_ALLOW_ZSCRIPT;
      }
    }
  }
  delete par;
  if (newFormat) return AD_NONE; // alas

  GCon->Log(NAME_Init, "zscript allowed due to old MAPINFO format");
  return AD_ALLOW_ZSCRIPT;
}


//==========================================================================
//
//  FL_RegisterModDetectors
//
//==========================================================================
static void FL_RegisterModDetectors () {
  fsysRegisterModDetector(&detectMapinfoZScript);
  fsysRegisterModDetector(&detectCzechbox);
  fsysRegisterModDetector(&detectFromList);
}


//==========================================================================
//
//  FL_IsIgnoredPlayerClass
//
//==========================================================================
bool FL_IsIgnoredPlayerClass (VStr cname) {
  if (cname.length() == 0) return true;
  return ignoredPlayerClassesMap.has(cname);
}


//==========================================================================
//
//  ParseDetectors
//
//==========================================================================
static void ParseVWadKeys (VStr name, bool inHomeDir) {
  if (name.isEmpty()) return;

  //GCon->Logf(NAME_Debug, "===<%s> : <%s>===", *name, *fl_configdir);
  if (name.isAbsolutePath()) {
    if (!Sys_FileExists(name)) return;
  } else {
    if (inHomeDir) {
      if (fl_configdir.IsNotEmpty() && Sys_FileExists(fl_configdir+"/"+name)) {
        name = fl_configdir+"/"+name;
      } else {
        return;
      }
    } else {
      if (Sys_FileExists(fl_basedir+"/"+name)) {
        name = fl_basedir+"/"+name;
      } else {
        return;
      }
    }
  }

  GCon->Logf(NAME_Init, "Parsing public keys file \"%s\"", *name);
  VScriptParser *sc = new VScriptParser(name, FL_OpenSysFileRead(name));
  sc->SetCMode(true);
  for (;;) {
    if (sc->Check("public_keys_for")) {
      sc->ExpectString();
      VStr signer = sc->String;
      sc->Expect("{");
      while (!sc->Check("}")) {
        if (sc->Check("key")) {
          if (!sc->Check("=")) {
            sc->Error("expecting '='");
          }
          if (!sc->GetString()) {
            sc->Error("expecting key string");
          }
          FSysPublicKey key;
          if (!FSYS_DecodePublicKey(*sc->String, key)) {
            sc->Error(va("cannot decode key string \"%s\"", *sc->String));
          }
          FSYS_RegisterPublicKey(*signer, key);
          if (!sc->Check(";")) {
            sc->Error("expecting ';'");
          }
        } else {
          sc->Error(va("unknown key option '%s'", *sc->String));
        }
      }
    } else {
      if (!sc->GetString()) break;
      sc->Error(va("unknown key section '%s'", *sc->String));
    }
  }
}
