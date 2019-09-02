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
#include "gamedefs.h"
#include "../../libs/core/fsys/fsys_local.h"


extern VCvarB game_release_mode;
//extern VCvarI game_override_mode;

static VCvarB dbg_dump_gameinfo("dbg_dump_gameinfo", false, "Dump parsed game.txt?", CVAR_PreInit);
static VCvarB gz_skip_menudef("_gz_skip_menudef", false, "Skip gzdoom menudef parsing?", CVAR_PreInit);

static VCvarB __dbg_debug_preinit("__dbg_debug_preinit", false, "Dump preinits?", CVAR_PreInit);

VCvarS game_name("game_name", "unknown", "The Name Of The Game.", CVAR_Rom);

int cli_WAll = 0;

bool fsys_hasPwads = false; // or paks
bool fsys_hasMapPwads = false; // or paks
bool fsys_DisableBloodReplacement = false; // for custom modes


int fsys_warp_n0 = -1;
int fsys_warp_n1 = -1;
VStr fsys_warp_cmd;

static bool fsys_onlyOneBaseFile = false;

static VStr cliGameMode;
static const char *cliGameCStr = nullptr;

GameOptions game_options;


struct AuxFile {
  VStr name;
  bool optional;
};


struct version_t {
  VStr param;
  TArray<VStr> MainWads;
  VStr GameDir;
  TArray<AuxFile> AddFiles;
  TArray<VStr> BaseDirs;
  int ParmFound;
  bool FixVoices;
  VStr warp;
  TArray<VStr> filters;
  GameOptions options;
};


VStr fl_basedir;
VStr fl_savedir;
VStr fl_gamedir;

static TArray<VStr> IWadDirs;
static int IWadIndex;
static VStr warpTpl;
static TArray<ArgVarValue> preinitCV;
static ArgVarValue emptyAV;

static TArray<VStr> cliModesList;
static int cli_oldSprites = 0;


// ////////////////////////////////////////////////////////////////////////// //
extern "C" {
  int cliHelpSorter (const void *aa, const void *bb, void *) {
    if (aa == bb) return 0;
    const VParsedArgs::ArgHelp *a = (const VParsedArgs::ArgHelp *)aa;
    const VParsedArgs::ArgHelp *b = (const VParsedArgs::ArgHelp *)bb;
    // if any of it doesn't have help...
    if (!!(a->arghelp) != !!(b->arghelp)) {
      if (a->arghelp) {
        vassert(!b->arghelp);
        return -1;
      }
      if (b->arghelp) {
        vassert(!a->arghelp);
        return 1;
      }
      vassert(0);
    }
    return VStr::ICmp(a->argname, b->argname);
  }
}


void FL_CollectPreinits () {
  if (GArgs.CheckParm("-help") || GArgs.CheckParm("--help")) {
    TArray<VParsedArgs::ArgHelp> list;
    VParsedArgs::GetArgList(list);
    if (list.length()) {
      timsort_r(list.ptr(), list.length(), sizeof(VParsedArgs::ArgHelp), &cliHelpSorter, nullptr);
      int maxlen = 0;
      for (auto &&ainfo : list) {
        int len = (int)strlen(ainfo.argname);
        if (maxlen < len) maxlen = len;
      }
      for (auto &&ainfo : list) {
        #ifdef _WIN32
        fprintf(stderr, "%*s -- %s\n", -maxlen, ainfo.argname, ainfo.arghelp);
        #else
        GLog.Logf("%*s -- %s", -maxlen, ainfo.argname, ainfo.arghelp);
        #endif
      }
    }
    Z_Exit(0);
  }

  if (GArgs.CheckParm("-help-developer") || GArgs.CheckParm("--help-developer") ||
      GArgs.CheckParm("-help-dev") || GArgs.CheckParm("--help-dev"))
  {
    TArray<VParsedArgs::ArgHelp> list;
    VParsedArgs::GetArgList(list, true);
    if (list.length()) {
      timsort_r(list.ptr(), list.length(), sizeof(VParsedArgs::ArgHelp), &cliHelpSorter, nullptr);
      int maxlen = 0;
      for (auto &&ainfo : list) {
        int len = (int)strlen(ainfo.argname);
        if (maxlen < len) maxlen = len;
      }
      for (auto &&ainfo : list) {
        #ifdef _WIN32
        fprintf(stderr, "%*s -- %s\n", -maxlen, ainfo.argname, (ainfo.arghelp ? ainfo.arghelp : "undocumented"));
        #else
        GLog.Logf("%*s -- %s", -maxlen, ainfo.argname, (ainfo.arghelp ? ainfo.arghelp : "undocumented"));
        #endif
      }
    }
    Z_Exit(0);
  }

  int aidx = 1;
  bool inMinus = false;
  while (aidx < GArgs.Count()) {
    const char *args = GArgs[aidx];
    //GLog.Logf("FL_CollectPreinits: argc=%d; aidx=%d; inminus=%d; args=<%s>", GArgs.Count(), aidx, (int)inMinus, args);
    if (!inMinus) {
      if (!args[0]) {
        GArgs.removeAt(aidx);
        continue;
      }
    }
    ++aidx;
    // skip "-" commands
    if (args[0] == '-') {
      inMinus = true;
      continue;
    }
    if (args[0] != '+') {
      continue;
    }
    // stray "+"?
    if (!args[1]) {
      inMinus = true;
      continue;
    }
    inMinus = false;
    // extract command name
    int spos = 2;
    while (args[spos] && (vuint8)(args[spos]) > ' ') ++spos;
    VStr vname = VStr(args+1, spos-1);
    int flg = VCvar::GetVarFlags(*vname);
    if (flg < 0 || !(flg&CVAR_PreInit)) {
      inMinus = true;
      continue;
    }
    // collect value
    VStr val;
    while (args[spos] && (vuint8)(args[spos]) <= ' ') ++spos;
    if (args[spos]) {
      // in the same arg
      /*
      if (aidx < GArgs.Count()) {
        const char *a2 = GArgs[aidx];
        if (a2[0] != '+' && a2[0] != '-') {
          inMinus = true;
          continue;
        }
      }
      */
      val = VStr(args+spos);
      // remove this arg
      --aidx;
      GArgs.removeAt(aidx);
    } else {
      // in another arg
      if (aidx >= GArgs.Count()) break;
      /*
      if (aidx+1 < GArgs.Count()) {
        const char *a2 = GArgs[aidx+1];
        if (a2[0] != '+' && a2[0] != '-') {
          inMinus = true;
          continue;
        }
      }
      */
      val = VStr(GArgs[aidx]);
      // remove two args
      --aidx;
      GArgs.removeAt(aidx);
      GArgs.removeAt(aidx);
    }
    // save it
    ArgVarValue &vv = preinitCV.alloc();
    vv.varname = vname;
    vv.value = val;
    //GLog.Logf("FL_CollectPreinits:   new var '%s' with value '%s'", *vname, *val);
    //HACK!
    if (vname.ICmp("__dbg_debug_preinit") == 0) VCvar::Set(*vname, val);
  }
  if (__dbg_debug_preinit && preinitCV.length()) {
    for (int f = 0; f < GArgs.Count(); ++f) GCon->Logf("::: %d: <%s>", f, GArgs[f]);
    for (int f = 0; f < preinitCV.length(); ++f) {
      const ArgVarValue &vv = preinitCV[f];
      GCon->Logf(":DOSET:%d: <%s> = <%s>", f, *vv.varname.quote(), *vv.value.quote());
    }
  }
}


void FL_ProcessPreInits () {
  if (__dbg_debug_preinit) {
    if (preinitCV.length()) for (int f = 0; f < GArgs.Count(); ++f) GCon->Logf("::: %d: <%s>", f, GArgs[f]);
  }
  for (int f = 0; f < preinitCV.length(); ++f) {
    const ArgVarValue &vv = preinitCV[f];
    VCvar::Set(*vv.varname, vv.value);
    if (__dbg_debug_preinit) GCon->Logf(":SET:%d: <%s> = <%s>", f, *vv.varname.quote(), *vv.value.quote());
  }
}


bool FL_HasPreInit (VStr varname) {
  if (varname.length() == 0 || preinitCV.length() == 0) return false;
  for (int f = 0; f < preinitCV.length(); ++f) {
    if (preinitCV[f].varname.ICmp(varname) == 0) return true;
  }
  return false;
}


void FL_ClearPreInits () {
  preinitCV.clear();
}


// used to set "preinit" cvars
int FL_GetPreInitCount () {
  return preinitCV.length();
}


const ArgVarValue &FL_GetPreInitAt (int idx) {
  if (idx < 0 || idx >= preinitCV.length()) return emptyAV;
  return preinitCV[idx];
}


// ////////////////////////////////////////////////////////////////////////// //
struct GroupMask {
  VStr mask;
  bool enabled;

  bool isGlob () const { return (mask.indexOf('*') >= 0 || mask.indexOf('?') >= 0 || mask.indexOf('[') >= 0); }
  bool match (VStr s) const { return s.globmatch(mask, false); }
};


struct CustomModeInfo {
  VStr name;
  TArray<VStr> aliases;
  TArray<VStr> pwads;
  TArray<VStr> postpwads;
  TArray<GroupMask> autoskips;
  bool disableBloodReplacement;
  bool disableGoreMod;
  bool disableBDW;
  VStr basedir;
  // has any sense only for modes loaded from "~/.k8vavoom/modes.rc"
  TArray<VStr> basedirglob;
  bool reported;

  CustomModeInfo () : name(), aliases(), pwads(), postpwads(), autoskips(), disableBloodReplacement(false), disableGoreMod(false), disableBDW(false), basedir(), basedirglob(), reported(false) {}

  CustomModeInfo (const CustomModeInfo &src) : name(), aliases(), pwads(), postpwads(), autoskips(), disableBloodReplacement(false), disableGoreMod(false), disableBDW(false), basedir(), basedirglob(), reported(false) { copyFrom(src); }

  CustomModeInfo &operator = (const CustomModeInfo &src) { copyFrom(src); return *this; }

  void clear () {
    name.clear();
    aliases.clear();
    pwads.clear();
    postpwads.clear();
    autoskips.clear();
    disableBloodReplacement = false;
    disableGoreMod = false;
    disableBDW = false;
    basedirglob.clear();
    reported = false;
  }

  void appendPWad (VStr str) {
    if (str.isEmpty()) return;
    for (auto &&s : pwads) if (s.strEqu(str)) return;
    pwads.append(str);
  }

  void appendPostPWad (VStr str) {
    if (str.isEmpty()) return;
    for (auto &&s : postpwads) if (s.strEqu(str)) return;
    postpwads.append(str);
  }

  void copyFrom (const CustomModeInfo &src) {
    if (&src == this) return;
    clear();
    name = src.name;
    // copy aliases
    aliases.setLength(src.aliases.length());
    for (auto &&it : src.aliases.itemsIdx()) aliases[it.index()] = it.value();
    // copy pwads
    pwads.setLength(src.pwads.length());
    for (auto &&it : src.pwads.itemsIdx()) pwads[it.index()] = it.value();
    // copy postwads
    postpwads.setLength(src.postpwads.length());
    for (auto &&it : src.postpwads.itemsIdx()) postpwads[it.index()] = it.value();
    // copy autoskips
    autoskips.setLength(src.autoskips.length());
    for (auto &&it : src.autoskips.itemsIdx()) autoskips[it.index()] = it.value();
    // copy flags
    disableBloodReplacement = src.disableBloodReplacement;
    disableGoreMod = src.disableGoreMod;
    disableBDW = src.disableBDW;
    // copy other things
    basedir = src.basedir;
    basedirglob.setLength(src.basedirglob.length());
    for (auto &&it : src.basedirglob.itemsIdx()) basedirglob[it.index()] = it.value();
    reported = src.reported;
  }

  // used to build final mode definition
  void merge (const CustomModeInfo &src) {
    if (&src == this) return;
    for (auto &&s : src.pwads) appendPWad(s);
    for (auto &&s : src.postpwads) appendPostPWad(s);
    for (auto &&s : src.autoskips) autoskips.append(s);
    if (src.disableBloodReplacement) disableBloodReplacement = true;
    if (src.disableGoreMod) disableGoreMod = true;
    if (src.disableBDW) disableBDW = true;
  }

  bool isMyAlias (VStr s) const {
    s = s.xstrip();
    if (s.isEmpty()) return false;
    for (auto &a : aliases) if (a.strEquCI(s)) return true;
    return false;
  }

  static VStr stripBaseDirShit (VStr dirname) {
    while (!dirname.isEmpty() && (dirname[0] == '/' || dirname[0] == '\\')) dirname.chopLeft(1);
    while (!dirname.isEmpty() && (dirname[dirname.length()-1] == '/' || dirname[dirname.length()-1] == '\\')) dirname.chopRight(1);
    if (dirname.startsWithCI("basev/")) dirname.chopLeft(6);
    return dirname;
  }

  bool isGoodBaseDir (VStr dirname) const {
    if (basedirglob.length() == 0) return true;
    dirname = stripBaseDirShit(dirname);
    if (dirname.isEmpty()) return true;
    for (auto &g : basedirglob) if (dirname.globMatchCI(g)) return true;
    return false;
  }

  // `mode` keyword skipped, expects mode name
  // mode must be cleared
  void parse (VScriptParser *sc) {
    //clear();
    sc->ExpectString();
    name = sc->String;
    sc->Expect("{");
    while (!sc->Check("}")) {
      if (sc->Check("pwad")) {
        for (;;) {
          sc->ExpectString();
          appendPWad(sc->String);
          if (!sc->Check(",")) break;
        }
      } else if (sc->Check("postpwad")) {
        for (;;) {
          sc->ExpectString();
          appendPostPWad(sc->String);
          if (!sc->Check(",")) break;
        }
      } else if (sc->Check("skipauto")) {
        for (;;) {
          sc->ExpectString();
          sc->String = sc->String.xstrip();
          if (!sc->String.isEmpty()) {
            GroupMask &gi = autoskips.alloc();
            gi.mask = sc->String;
            gi.enabled = false;
          }
          if (!sc->Check(",")) break;
        }
      } else if (sc->Check("forceauto")) {
        for (;;) {
          sc->ExpectString();
          GroupMask &gi = autoskips.alloc();
          gi.mask = sc->String;
          gi.enabled = true;
          if (!sc->Check(",")) break;
        }
      } else if (sc->Check("DisableBloodReplacement")) {
        disableBloodReplacement = true;
      } else if (sc->Check("DisableGoreMod")) {
        disableGoreMod = true;
      } else if (sc->Check("DisableBDW")) {
        disableBDW = true;
      } else if (sc->Check("alias")) {
        sc->ExpectString();
        VStr k = sc->String.xstrip();
        if (!k.isEmpty()) aliases.append(k);
      } else if (sc->Check("basedir")) {
        sc->ExpectString();
        VStr k = stripBaseDirShit(sc->String.xstrip());
        if (!k.isEmpty()) basedirglob.append(k);
      } else {
        sc->Error(va("unknown command '%s'", *sc->String));
      }
    }
  }
};


static CustomModeInfo customMode;
static TArray<CustomModeInfo> userModes; // from "~/.k8vavoom/modes.rc"
static TArray<VStr> postPWads; // from autoload


//==========================================================================
//
//  LoadModesFromStream
//
//  deletes stream
//
//==========================================================================
static void LoadModesFromStream (VStream *rcstrm, TArray<CustomModeInfo> &modes, VStr basedir=VStr::EmptyString) {
  if (!rcstrm) return;
  VScriptParser *sc = new VScriptParser(rcstrm->GetName(), rcstrm);
  while (!sc->AtEnd()) {
    if (sc->Check("alias")) {
      sc->ExpectString();
      VStr k = sc->String.xstrip();
      sc->Expect("is");
      sc->ExpectString();
      VStr v = sc->String.xstrip();
      if (!k.isEmpty() && !v.isEmpty() && !k.strEquCI(v)) {
        bool found = false;
        for (int f = 0; f < modes.length(); ++f) {
          if (modes[f].name.strEquCI(v)) {
            found = true;
            modes[f].aliases.append(k);
            break;
          }
        }
        if (!found) sc->Message(va("cannot set alias '%s', because mode '%s' is unknown", *k, *v));
      }
      continue;
    }
    sc->Expect("mode");
    CustomModeInfo mode;
    mode.clear();
    mode.basedir = basedir;
    mode.parse(sc);
    // append mode
    bool found = false;
    for (int f = 0; f < modes.length(); ++f) {
      if (modes[f].name.strEquCI(mode.name)) {
        // i found her!
        found = true;
        modes[f] = mode;
        break;
      }
    }
    if (!found) modes.append(mode);
  }
  delete sc;
}


//==========================================================================
//
//  ParseUserModes
//
//  "game_name" cvar must be set
//
//==========================================================================
static void ParseUserModes () {
  if (game_name.asStr().isEmpty()) return; // just in case
  VStream *rcstrm = FL_OpenFileReadInCfgDir("modes.rc");
  if (!rcstrm) return;
  GCon->Logf(NAME_Init, "parsing user mode definitions from '%s'...", *rcstrm->GetName());

  // load modes
  LoadModesFromStream(rcstrm, userModes);
}


//==========================================================================
//
//  ApplyUserModes
//
//==========================================================================
static void ApplyUserModes (VStr basedir) {
  // apply modes
  for (auto &&mname : cliModesList) {
    CustomModeInfo *nfo = nullptr;
    for (auto &&mode : userModes) {
      if (mode.isGoodBaseDir(basedir) && mode.name.strEquCI(mname)) {
        nfo = &mode;
        break;
      }
    }
    if (!nfo) {
      for (auto &&mode : userModes) {
        if (mode.isGoodBaseDir(basedir) && mode.isMyAlias(mname)) {
          nfo = &mode;
          break;
        }
      }
    }
    if (nfo) {
      if (!nfo->reported) {
        nfo->reported = true;
        GCon->Logf(NAME_Init, "activating user mode '%s'", *nfo->name);
      }
      customMode.merge(*nfo);
    }
  }
}


//==========================================================================
//
//  SetupCustomMode
//
//==========================================================================
static void SetupCustomMode (VStr basedir) {
  //customMode.clear();

  if (!basedir.isEmpty() && !basedir.endsWith("/")) basedir += "/";
  VStream *rcstrm = FL_OpenSysFileRead(basedir+"modes.rc");
  if (!rcstrm) return;
  GCon->Logf(NAME_Init, "parsing mode definitions from '%s'...", *rcstrm->GetName());

  // load modes
  TArray<CustomModeInfo> modes;
  LoadModesFromStream(rcstrm, modes, basedir);
  if (modes.length() == 0) return; // nothing to do

  // build active mode
  customMode.basedir = basedir;
  for (auto &&mname : cliModesList) {
    const CustomModeInfo *nfo = nullptr;
    for (auto &&mode : modes) {
      if (mode.name.strEquCI(mname)) {
        nfo = &mode;
        break;
      }
    }
    if (!nfo) {
      for (auto &&mode : modes) {
        if (mode.isMyAlias(mname)) {
          nfo = &mode;
          break;
        }
      }
    }
    if (nfo) {
      GCon->Logf(NAME_Init, "activating mode '%s'", *nfo->name);
      customMode.merge(*nfo);
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
struct PWadFile {
  VStr fname;
  bool skipSounds;
  bool skipSprites;
  bool skipDehacked;
  bool storeInSave;
  bool asDirectory;

  PWadFile ()
    : fname()
    , skipSounds(false)
    , skipSprites(false)
    , skipDehacked(false)
    , storeInSave(false)
    , asDirectory(false)
  {}
};


static TArray<PWadFile> pwadList;
static int doStartMap = 0;


static int pwflag_SkipSounds = 0;
static int pwflag_SkipSprites = 0;
static int pwflag_SkipDehacked = 0;
static int pwflag_StoreInSave = 1;


static void tempMount (const PWadFile &pwf) {
  if (pwf.fname.isEmpty()) return;

  W_StartAuxiliary(); // just in case

  if (pwf.asDirectory) {
    VDirPakFile *dpak = new VDirPakFile(pwf.fname);
    //if (!dpak->hasFiles()) { delete dpak; return; }

    SearchPaths.append(dpak);

    // add all WAD files in the root
    TArray<VStr> wads;
    dpak->ListWadFiles(wads);
    for (int i = 0; i < wads.length(); ++i) {
      VStream *wadst = dpak->OpenFileRead(wads[i]);
      if (!wadst) continue;
      W_AddAuxiliaryStream(wadst, WAuxFileType::VFS_Archive);
    }

    // add all pk3 files in the root
    TArray<VStr> pk3s;
    dpak->ListPk3Files(pk3s);
    for (int i = 0; i < pk3s.length(); ++i) {
      VStream *pk3st = dpak->OpenFileRead(pk3s[i]);
      W_AddAuxiliaryStream(pk3st, WAuxFileType::VFS_Archive);
    }
  } else {
    VStream *strm = FL_OpenSysFileRead(pwf.fname);
    if (!strm) {
      //GCon->Logf(NAME_Init, "TEMPMOUNT: OOPS0: %s", *pwf.fname);
      return;
    }
    if (strm->TotalSize() < 16) {
      delete strm;
      //GCon->Logf(NAME_Init, "TEMPMOUNT: OOPS1: %s", *pwf.fname);
      return;
    }
    char sign[4];
    strm->Serialise(sign, 4);
    strm->Seek(0);
    if (memcmp(sign, "PWAD", 4) == 0 || memcmp(sign, "IWAD", 4) == 0) {
      //GCon->Logf(NAME_Init, "TEMPMOUNT: WAD: %s", *pwf.fname);
      W_AddAuxiliaryStream(strm, WAuxFileType::VFS_Wad);
    } else {
      VStr ext = pwf.fname.ExtractFileExtension();
      if (ext.strEquCI(".pk3")) {
        //GCon->Logf(NAME_Init, "TEMPMOUNT: PK3: %s", *pwf.fname);
        W_AddAuxiliaryStream(strm, WAuxFileType::VFS_Archive);
      } else if (ext.strEquCI(".zip")) {
        //GCon->Logf(NAME_Init, "TEMPMOUNT: ZIP: %s", *pwf.fname);
        W_AddAuxiliaryStream(strm, WAuxFileType::VFS_Zip);
      }
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
static TArray<VStr> wpklist; // everything is lowercased


//==========================================================================
//
//  FL_GetWadPk3List
//
//==========================================================================
const TArray<VStr> &FL_GetWadPk3List () {
  return wpklist;
}


//==========================================================================
//
//  wpkAppend
//
//==========================================================================
static void wpkAppend (VStr fname, bool asystem) {
  if (fname.length() == 0) return;
  VStr fn = fname.toLowerCase();
  if (!asystem) {
    fn = fn.extractFileName();
    if (fn.length() == 0) fn = fname.toLowerCase();
  }
  // check for duplicates
  for (int f = wpklist.length()-1; f >= 0; --f) {
    if (wpklist[f] != fn) continue;
    // i found her!
    return;
  }
  /*
  WadPakInfo &wi = wpklist.Alloc();
  wi.path = fn;
  wi.isSystem = asystem;
  */
  wpklist.append(fn);
}


// ////////////////////////////////////////////////////////////////////////// //
__attribute__((unused)) static int cmpfuncCI (const void *v1, const void *v2) {
  return ((VStr*)v1)->ICmp((*(VStr*)v2));
}

static int cmpfuncCINoExt (const void *v1, const void *v2) {
  return ((VStr *)v1)->StripExtension().ICmp(((VStr *)v2)->StripExtension());
}


//==========================================================================
//
//  AddAnyFile
//
//==========================================================================
static void AddAnyFile (VStr fname, bool allowFail, bool fixVoices=false) {
  if (fname.length() == 0) {
    if (!allowFail) Sys_Error("cannot add empty file");
    return;
  }
  if (!Sys_FileExists(fname)) {
    if (!allowFail) Sys_Error("cannot add file \"%s\"", *fname);
    GCon->Logf(NAME_Warning, "cannot add file \"%s\"", *fname);
    return;
  }
  if (!W_AddDiskFileOptional(fname, (allowFail ? false : fixVoices))) {
    if (!allowFail) Sys_Error("cannot add file \"%s\"", *fname);
    GCon->Logf(NAME_Warning, "cannot add file \"%s\"", *fname);
  }
}


enum { CM_PRE_PWADS, CM_POST_PWADS };

//==========================================================================
//
//  CustomModeLoadPwads
//
//==========================================================================
static void CustomModeLoadPwads (int type) {
  TArray<VStr> &list = (type == CM_PRE_PWADS ? customMode.pwads : customMode.postpwads);
  //GCon->Logf(NAME_Init, "CustomModeLoadPwads: type=%d; len=%d", type, list.length());

  // load post-file pwads from autoload here too
  if (type == CM_POST_PWADS && postPWads.length() > 0) {
    GCon->Logf(NAME_Init, "loading autoload post-pwads");
    for (auto &&wn : postPWads) {
      GCon->Logf(NAME_Init, "autoload post-pwad: %s...", *wn);
      AddAnyFile(wn, true); // allow fail
    }
  }

  for (auto &&fname : list) {
    if (fname.isEmpty()) continue;
    if (!fname.startsWith("/")) fname = customMode.basedir+fname;
    GCon->Logf(NAME_Init, "mode pwad: %s...", *fname);
    AddAnyFile(fname, true); // allow fail
  }
}


static TMap<VStr, bool> cliGroupMap; // true: enabled; false: disabled
static TArray<GroupMask> cliGroupMask;


//==========================================================================
//
//  AddAutoloadRC
//
//==========================================================================
void AddAutoloadRC (VStr aubasedir) {
  VStream *aurc = FL_OpenSysFileRead(aubasedir+"autoload.rc");
  if (!aurc) return;

  // collect autoload groups to skip
  // add skips from custom mode
  for (int f = 0; f < customMode.autoskips.length(); ++f) {
    GroupMask &gi = customMode.autoskips[f];
    if (gi.isGlob()) {
      cliGroupMask.append(gi);
    } else {
      cliGroupMap.put(gi.mask.toLowerCase(), false);
    }
  }

  VScriptParser *sc = new VScriptParser(aurc->GetName(), aurc);

  while (!sc->AtEnd()) {
    sc->Expect("group");
    sc->ExpectString();
    VStr grpname = sc->String;
    bool enabled = !sc->Check("disabled");
    sc->Expect("{");
    // exact matches has precedence
    auto gmp = cliGroupMap.find(grpname.toLowerCase());
    if (gmp) {
      enabled = *gmp;
    } else {
      // process masks; backwards, so latest mask has precedence
      for (int f = cliGroupMask.length()-1; f >= 0; --f) {
        if (grpname.globmatch(cliGroupMask[f].mask, false)) {
          enabled = cliGroupMask[f].enabled;
          break;
        }
      }
    }
    if (!enabled) {
      GCon->Logf(NAME_Init, "skipping autoload group '%s'", *grpname);
      sc->SkipBracketed(true); // bracket eaten
      continue;
    }
    GCon->Logf(NAME_Init, "processing autoload group '%s'", *grpname);
    // get file list
    while (!sc->Check("}")) {
      if (sc->Check(",")) continue;
      bool postPWad = false;
      if (sc->Check("postpwad")) postPWad = true;
      if (!sc->GetString()) sc->Error("wad/pk3 path expected");
      if (sc->String.isEmpty()) continue;
      VStr fname = ((*sc->String)[0] == '/' ? sc->String : aubasedir+sc->String);
      if (postPWad) postPWads.append(fname); else AddAnyFile(fname, true); // allow fail
    }
  }

  delete sc;
}


//==========================================================================
//
//  AddGameDir
//
//==========================================================================
static void AddGameDir (VStr basedir, VStr dir) {
  GCon->Logf(NAME_Init, "adding game dir '%s'...", *dir);

  VStr bdx = basedir;
  if (bdx.length() == 0) bdx = "./";
  bdx = bdx+"/"+dir;

  if (!Sys_DirExists(bdx)) return;

  TArray<VStr> WadFiles;
  TArray<VStr> ZipFiles;

  // find all .wad/.pk3 files in that directory
  auto dirit = Sys_OpenDir(bdx);
  if (dirit) {
    for (VStr test = Sys_ReadDir(dirit); test.IsNotEmpty(); test = Sys_ReadDir(dirit)) {
      //fprintf(stderr, "  <%s>\n", *test);
      if (test[0] == '_' || test[0] == '.') continue; // skip it
      VStr ext = test.ExtractFileExtension();
           if (ext.strEquCI(".wad")) WadFiles.Append(test);
      else if (ext.strEquCI(".pk3")) ZipFiles.Append(test);
    }
    Sys_CloseDir(dirit);
    qsort(WadFiles.Ptr(), WadFiles.length(), sizeof(VStr), cmpfuncCINoExt);
    qsort(ZipFiles.Ptr(), ZipFiles.length(), sizeof(VStr), cmpfuncCINoExt);
  }

  // add system dir, if it has any files
  if (ZipFiles.length() || WadFiles.length()) wpkAppend(dir+"/", true); // don't strip path

  // now add wads, then pk3s
  for (int i = 0; i < WadFiles.length(); ++i) {
    //if (i == 0 && ZipFiles.length() == 0) wpkAppend(dir+"/"+WadFiles[i], true); // system pak
    W_AddDiskFile(bdx+"/"+WadFiles[i], false);
  }

  for (int i = 0; i < ZipFiles.length(); ++i) {
    //if (i == 0) wpkAppend(dir+"/"+ZipFiles[i], true); // system pak
    bool isBPK = ZipFiles[i].extractFileName().strEquCI("basepak.pk3");
    int spl = SearchPaths.length();
    /*AddDiskZipFile*/W_AddDiskFile(bdx+"/"+ZipFiles[i]);
    if (isBPK) {
      // mark "basepak" flags
      for (int cc = spl; cc < SearchPaths.length(); ++cc) {
        SearchPaths[cc]->basepak = true;
      }
    }
  }

  // custom mode
  SetupCustomMode(bdx);
  ApplyUserModes(dir);

  // add "autoload/*"
  if (!fsys_onlyOneBaseFile) {
    WadFiles.clear();
    ZipFiles.clear();
    if (bdx[bdx.length()-1] != '/') bdx += "/";
    bdx += "autoload";
    VStr bdxSlash = bdx+"/";
    // find all .wad/.pk3 files in that directory
    dirit = Sys_OpenDir(bdx);
    if (dirit) {
      for (VStr test = Sys_ReadDir(dirit); test.IsNotEmpty(); test = Sys_ReadDir(dirit)) {
        //fprintf(stderr, "  <%s>\n", *test);
        if (test[0] == '_' || test[0] == '.') continue; // skip it
        VStr ext = test.ExtractFileExtension();
             if (ext.strEquCI(".wad")) WadFiles.Append(test);
        else if (ext.strEquCI(".pk3")) ZipFiles.Append(test);
      }
      Sys_CloseDir(dirit);
      qsort(WadFiles.Ptr(), WadFiles.length(), sizeof(VStr), cmpfuncCINoExt);
      qsort(ZipFiles.Ptr(), ZipFiles.length(), sizeof(VStr), cmpfuncCINoExt);
    }

    // now add wads, then pk3s
    for (int i = 0; i < WadFiles.length(); ++i) W_AddDiskFile(bdxSlash+WadFiles[i], false);
    for (int i = 0; i < ZipFiles.length(); ++i) /*AddDiskZipFile*/W_AddDiskFile(bdxSlash+ZipFiles[i]);

    AddAutoloadRC(bdxSlash);
  }

  // finally add directory itself
  // k8: nope
  /*
  VFilesDir *info = new VFilesDir(bdx);
  SearchPaths.Append(info);
  */
}


//==========================================================================
//
//  AddGameDir
//
//==========================================================================
static void AddGameDir (VStr dir) {
  AddGameDir(fl_basedir, dir);
  //k8:wtf?!
  //if (fl_savedir.IsNotEmpty()) AddGameDir(fl_savedir, dir);
  fl_gamedir = dir;
}


//==========================================================================
//
//  FindMainWad
//
//==========================================================================
static VStr FindMainWad (VStr MainWad) {
  if (MainWad.length() == 0) return VStr();

  // if we have path separators, try relative path first
  bool hasSep = false;
  for (const char *s = *MainWad; *s; ++s) {
#ifdef _WIN32
    if (*s == '/' || *s == '\\') { hasSep = true; break; }
#else
    if (*s == '/') { hasSep = true; break; }
#endif
  }
#ifdef _WIN32
  if (!hasSep && MainWad.length() >= 2 && MainWad[1] == ':') hasSep = true;
#endif

  if (hasSep) {
    if (Sys_FileExists(MainWad)) return MainWad;
  }

  // first check in IWAD directories
  for (int i = 0; i < IWadDirs.length(); ++i) {
    if (Sys_FileExists(IWadDirs[i]+"/"+MainWad)) return IWadDirs[i]+"/"+MainWad;
  }

  // then look in the save directory
  //if (fl_savedir.IsNotEmpty() && Sys_FileExists(fl_savedir+"/"+MainWad)) return fl_savedir+"/"+MainWad;

  // finally in base directory
  if (Sys_FileExists(fl_basedir+"/"+MainWad)) return fl_basedir+"/"+MainWad;

  // just in case, check it as-is
  if (Sys_FileExists(MainWad)) return MainWad;

  return VStr();
}


//==========================================================================
//
//  SetupGameDir
//
//==========================================================================
static void SetupGameDir (VStr dirname) {
  AddGameDir(dirname);
}


//==========================================================================
//
//  ParseBase
//
//==========================================================================
static void ParseBase (VStr name, VStr mainiwad) {
  TArray<version_t> games;
  int selectedGame = -1;
  VStr UseName;

       if (fl_savedir.IsNotEmpty() && Sys_FileExists(fl_savedir+"/"+name)) UseName = fl_savedir+"/"+name;
  else if (Sys_FileExists(fl_basedir+"/"+name)) UseName = fl_basedir+"/"+name;
  else return;

  if (dbg_dump_gameinfo) GCon->Logf(NAME_Init, "Parsing game definition file \"%s\"...", *UseName);
  VScriptParser *sc = new VScriptParser(UseName, FL_OpenSysFileRead(UseName));
  while (!sc->AtEnd()) {
    version_t &dst = games.Alloc();
    dst.ParmFound = 0;
    dst.FixVoices = false;
    sc->Expect("game");
    sc->ExpectString();
    dst.GameDir = sc->String;
    if (dbg_dump_gameinfo) GCon->Logf(NAME_Init, " game dir: \"%s\"", *dst.GameDir);
    for (;;) {
      if (sc->Check("iwad")) {
        sc->ExpectString();
        if (sc->String.isEmpty()) continue;
        if (dst.MainWads.length() == 0) {
          dst.MainWads.Append(sc->String);
          if (dbg_dump_gameinfo) GCon->Logf(NAME_Init, "  iwad: \"%s\"", *sc->String);
        } else {
          sc->Error(va("duplicate iwad (%s) for game \"%s\"!", *sc->String, *dst.GameDir));
        }
        continue;
      }
      if (sc->Check("altiwad")) {
        sc->ExpectString();
        if (sc->String.isEmpty()) continue;
        if (dst.MainWads.length() == 0) {
          sc->Error(va("no iwad for game \"%s\"!", *dst.GameDir));
        }
        dst.MainWads.Append(sc->String);
        if (dbg_dump_gameinfo) GCon->Logf(NAME_Init, "  alternate iwad: \"%s\"", *sc->String);
        continue;
      }
      if (sc->Check("addfile")) {
        bool optional = sc->Check("optional");
        sc->ExpectString();
        if (sc->String.isEmpty()) continue;
        AuxFile &aux = dst.AddFiles.alloc();
        aux.name = sc->String;
        aux.optional = optional;
        if (dbg_dump_gameinfo) GCon->Logf(NAME_Init, "  aux file: \"%s\"", *sc->String);
        continue;
      }
      if (sc->Check("base")) {
        sc->ExpectString();
        if (sc->String.isEmpty()) continue;
        dst.BaseDirs.Append(sc->String);
        if (dbg_dump_gameinfo) GCon->Logf(NAME_Init, "  base: \"%s\"", *sc->String);
        continue;
      }
      if (sc->Check("param")) {
        sc->ExpectString();
        if (sc->String.length() < 2 || sc->String[0] != '-') sc->Error(va("invalid game (%s) param!", *dst.GameDir));
        dst.param = (*sc->String)+1;
        if (dbg_dump_gameinfo) GCon->Logf(NAME_Init, "  param: \"%s\"", (*sc->String)+1);
        dst.ParmFound = cliGameMode.strEquCI(dst.param);
        continue;
      }
      if (sc->Check("fixvoices")) {
        dst.FixVoices = true;
        if (dbg_dump_gameinfo) GCon->Logf(NAME_Init, "  fix voices: tan");
        continue;
      }
      if (sc->Check("warp")) {
        sc->ExpectString();
        dst.warp = VStr(sc->String);
        continue;
      }
      if (sc->Check("filter")) {
        sc->ExpectString();
        if (!sc->String.isEmpty()) dst.filters.append(VStr("filter/")+sc->String.toLowerCase());
        continue;
      }
      if (sc->Check("ashexen")) {
        sc->ExpectString();
        dst.options.hexenGame = true;
        continue;
      }
      break;
    }
    sc->Expect("end");
  }
  delete sc;
  if (dbg_dump_gameinfo) GCon->Logf(NAME_Init, "Done parsing game definition file \"%s\"...", *UseName);

  if (games.length() == 0) Sys_Error("No game definitions found!");

  int bestPIdx = -1;
  for (int gi = 0; gi < games.length(); ++gi) {
    version_t &G = games[gi];
    if (!G.ParmFound) continue;
    if (G.ParmFound > bestPIdx) {
      bestPIdx = G.ParmFound;
      selectedGame = gi;
    }
  }

  if (selectedGame >= 0) {
    game_name = *games[selectedGame].param;
    if (dbg_dump_gameinfo) GCon->Logf(NAME_Init, "SELECTED GAME: \"%s\"", *games[selectedGame].param);
  } else {
    if (games.length() != 1) {
      // try to select DooM or DooM II automatically
      fsys_EnableAuxSearch = true;
      {
        bool oldReport = fsys_no_dup_reports;
        fsys_no_dup_reports = true;
        W_CloseAuxiliary();
        for (int pwidx = 0; pwidx < pwadList.length(); ++pwidx) tempMount(pwadList[pwidx]);
        //GCon->Log("**********************************");
        // try "GAMEINFO" first
        VStr iwadGI;
        auto gilump = W_CheckNumForName("gameinfo");
        if (W_IsAuxLump(gilump)) {
          VScriptParser *gsc = new VScriptParser(W_FullLumpName(gilump), W_CreateLumpReaderNum(gilump));
          gsc->SetCMode(true);
          while (gsc->GetString()) {
            if (gsc->QuotedString) continue; // just in case
            if (gsc->String.strEquCI("iwad")) {
              gsc->Check("=");
              if (gsc->GetString()) iwadGI = gsc->String;
              continue;
            }
          }
          iwadGI = iwadGI.ExtractFileBaseName();
          if (iwadGI.length()) {
            if (!iwadGI.ExtractFileExtension().strEquCI(".wad")) iwadGI += ".wad";
            for (int gi = 0; gi < games.length() && selectedGame < 0; ++gi) {
              version_t &gmi = games[gi];
              for (int f = 0; f < gmi.MainWads.length(); ++f) {
                VStr gw = gmi.MainWads[f].extractFileBaseName();
                if (gw.strEquCI(iwadGI)) {
                  GCon->Logf(NAME_Init, "Detected game is '%s' (from gameinfo)", *gmi.param);
                  selectedGame = gi;
                  W_CloseAuxiliary();
                  break;
                }
              }
            }
          }
        }
        // try to guess from map name
        if (selectedGame < 0) {
          VStr mname = W_FindMapInAuxuliaries(nullptr);
          W_CloseAuxiliary();
          fsys_no_dup_reports = oldReport;
          if (!mname.isEmpty()) {
            //GCon->Logf("MNAME: <%s>", *mname);
            // found map, find DooM or DooM II game definition
            VStr gamename = (mname[0] == 'm' ? "doom2" : "doom");
            for (int gi = 0; gi < games.length(); ++gi) {
              version_t &G = games[gi];
              if (G.param.Cmp(gamename) == 0) {
                GCon->Logf(NAME_Init, "Detected game is '%s' (from map lump '%s')", *G.param, *mname);
                selectedGame = gi;
                break;
              }
            }
          }
        }
      }
      fsys_EnableAuxSearch = false;

      if (selectedGame < 0) {
        // try to detect game
        if (mainiwad.length() > 0) {
          for (int gi = 0; gi < games.length(); ++gi) {
            version_t &gmi = games[gi];
            bool okwad = false;
            VStr mw = mainiwad.extractFileBaseName();
            for (int f = 0; f < gmi.MainWads.length(); ++f) {
              VStr gw = gmi.MainWads[f].extractFileBaseName();
              if (mw.ICmp(gw) == 0) { okwad = true; break; }
            }
            if (okwad) {
              GCon->Logf(NAME_Init, "Detected game is '%s' (from iwad)", *gmi.param);
              selectedGame = gi;
              break;
            }
          }
        }
      }

      if (selectedGame < 0) {
        for (int gi = 0; gi < games.length(); ++gi) {
          version_t &gmi = games[gi];
          VStr mainWadPath;
          for (int f = 0; f < gmi.MainWads.length(); ++f) {
            mainWadPath = FindMainWad(gmi.MainWads[f]);
            if (!mainWadPath.isEmpty()) {
              GCon->Logf(NAME_Init, "Detected game is '%s' (iwad search)", *gmi.param);
              selectedGame = gi;
              break;
            }
          }
          if (selectedGame >= 0) break;
        }
      }

      if (selectedGame >= 0) {
        game_name = *games[selectedGame].param;
        GCon->Logf(NAME_Init, "detected game: \"%s\"", *games[selectedGame].param);
      }
    } else {
      selectedGame = 0;
    }
    if (selectedGame < 0) Sys_Error("Looks like I cannot find any IWADs. Did you forgot to specify -iwaddir?");
  }

  version_t &gmi = games[selectedGame];
  game_options = gmi.options;

  // look for the main wad file
  VStr mainWadPath;

  // try user-specified iwad
  if (mainiwad.length() > 0) {
    GCon->Logf(NAME_Init, "trying custom IWAD '%s'...", *mainiwad);
    mainWadPath = FindMainWad(mainiwad);
    if (mainWadPath.isEmpty()) {
      GCon->Logf(NAME_Warning, "custom IWAD '%s' not found!", *mainiwad);
    } else {
      GCon->Logf(NAME_Init, "found custom IWAD '%s'...", *mainWadPath);
    }
  }

  if (mainWadPath.length() == 0) {
    for (int f = 0; f < gmi.MainWads.length(); ++f) {
      mainWadPath = FindMainWad(gmi.MainWads[f]);
      if (!mainWadPath.isEmpty()) break;
    }
  }

  if (mainWadPath.isEmpty()) Sys_Error("Main wad file \"%s\" not found.", (gmi.MainWads.length() ? *gmi.MainWads[0] : "<none>"));

  //GCon->Logf("********* %d", gmi.filters.length());

  fsys_game_filters = gmi.filters;
  warpTpl = gmi.warp;

  IWadIndex = SearchPaths.length();
  //GCon->Logf("MAIN WAD(1): '%s'", *MainWadPath);
  wpkAppend(mainWadPath, false); // mark iwad as "non-system" file, so path won't be stored in savegame
  AddAnyFile(mainWadPath, false, gmi.FixVoices);

  for (int j = 0; j < gmi.AddFiles.length(); j++) {
    VStr FName = FindMainWad(gmi.AddFiles[j].name);
    if (FName.IsEmpty()) {
      if (gmi.AddFiles[j].optional) continue;
      Sys_Error("Required file \"%s\" not found", *gmi.AddFiles[j].name);
    }
    wpkAppend(FName, false); // mark additional files as "non-system", so path won't be stored in savegame
    AddAnyFile(FName, false);
  }

  for (int f = 0; f < gmi.BaseDirs.length(); ++f) AddGameDir(gmi.BaseDirs[f]);

  SetupGameDir(gmi.GameDir);
}


//==========================================================================
//
//  RenameSprites
//
//==========================================================================
static void RenameSprites () {
  VStream *Strm = FL_OpenFileRead("sprite_rename.txt");
  if (!Strm) return;

  VScriptParser *sc = new VScriptParser("sprite_rename.txt", Strm);
  TArray<VSpriteRename> Renames;
  TArray<VSpriteRename> AlwaysRenames;
  TArray<VLumpRename> LumpRenames;
  TArray<VLumpRename> AlwaysLumpRenames;
  while (!sc->AtEnd()) {
    bool Always = sc->Check("always");

    if (sc->Check("lump")) {
      sc->ExpectString();
      VStr Old = sc->String.ToLower();
      sc->ExpectString();
      VStr New = sc->String.ToLower();
      VLumpRename &R = (Always ? AlwaysLumpRenames.Alloc() : LumpRenames.Alloc());
      R.Old = *Old;
      R.New = *New;
      continue;
    }

    sc->ExpectString();
    if (sc->String.Length() != 4) sc->Error("Sprite name must be 4 chars long");
    VStr Old = sc->String.ToLower();

    sc->ExpectString();
    if (sc->String.Length() != 4) sc->Error("Sprite name must be 4 chars long");
    VStr New = sc->String.ToLower();

    VSpriteRename &R = Always ? AlwaysRenames.Alloc() : Renames.Alloc();
    R.Old[0] = Old[0];
    R.Old[1] = Old[1];
    R.Old[2] = Old[2];
    R.Old[3] = Old[3];
    R.New[0] = New[0];
    R.New[1] = New[1];
    R.New[2] = New[2];
    R.New[3] = New[3];
  }
  delete sc;

  bool RenameAll = !!cli_oldSprites;
  for (int i = 0; i < SearchPaths.length(); ++i) {
    if (RenameAll || i == IWadIndex) SearchPaths[i]->RenameSprites(Renames, LumpRenames);
    SearchPaths[i]->RenameSprites(AlwaysRenames, AlwaysLumpRenames);
  }
}


// ////////////////////////////////////////////////////////////////////////// //
extern VCvarB respawnparm;
extern VCvarI fastparm;
extern VCvarB NoMonsters;
extern VCvarI Skill;

int cli_NoMonsters = -1; // not specified
int cli_CompileAndExit = 0;
static int cli_FastMonsters = -1; // not specified
static int cli_Respawn = -1; // not specified
static int cli_NoMenuDef = -1; // not specified
static int cli_GoreMod = -1; // not specified
static int cli_BDWMod = -1; // not specified
static int cli_SkeeHUD = -1; // not specified

static const char *cli_BaseDir = nullptr;
static const char *cli_IWadName = nullptr;
static const char *cli_IWadDir = nullptr;
static const char *cli_SaveDir = nullptr;

static int reportIWads = 0;
static int reportPWads = 1;
static int cli_NakedBase = -1;


//==========================================================================
//
//  countFmtHash
//
//==========================================================================
static int countFmtHash (VStr str) {
  if (str.length() == 0) return 0;
  int count = 0;
  bool inHash = false;
  for (const char *s = *str; *s; ++s) {
    if (*s == '#') {
      if (!inHash) ++count;
      inHash = true;
    } else {
      inHash = false;
    }
  }
  return count;
}


//==========================================================================
//
//  cliFnameCollector
//
//==========================================================================
static int cliFnameCollector (VArgs &args, int idx) {
  pwflag_StoreInSave = 1;
  bool first = true;
  //++idx; // done by the called
  while (idx < args.Count()) {
    if (VStr::strEqu(args[idx], "-cosmetic")) {
      pwflag_StoreInSave = 0;
      ++idx;
      continue;
    }
    if (VParsedArgs::IsArgBreaker(args, idx)) break;
    VStr fname = args[idx++];
    bool sts = !!pwflag_StoreInSave;
    pwflag_StoreInSave = 1; // autoreset
    //GCon->Logf(NAME_Debug, "idx=%d; fname=<%s>", idx-1, *fname);
    if (fname.isEmpty()) { first = false; continue; }

    PWadFile pwf;
    pwf.fname = fname;
    pwf.skipSounds = !!pwflag_SkipSounds;
    pwf.skipSprites = !!pwflag_SkipSprites;
    pwf.skipDehacked = !!pwflag_SkipDehacked;
    pwf.storeInSave = sts;
    pwf.asDirectory = false;

    if (Sys_DirExists(fname)) {
      pwf.asDirectory = true;
      if (!first) {
        GCon->Logf(NAME_Init, "To mount directory '%s' as emulated PK3 file, you should use \"-file\".", *fname);
      } else {
        pwadList.append(pwf);
      }
    } else if (Sys_FileExists(fname)) {
      pwadList.append(pwf);
    } else {
      GCon->Logf(NAME_Init, "WARNING: File \"%s\" doesn't exist.", *fname);
    }
    first = false;
  }
  return idx;
}


//==========================================================================
//
//  FL_InitOptions
//
//==========================================================================
void FL_InitOptions () {
  fsys_ignoreSquare = 0; // check for "Adventures of Square"
  fsys_IgnoreZScript = 0;

  GArgs.AddFileOption("!1-game"); // '!' means "has args, and breaking" (number is argc)
  GArgs.AddFileOption("!1-logfile"); // don't register log file in saves
  GArgs.AddFileOption("!1-iwad");
  GArgs.AddFileOption("!1-iwaddir");
  GArgs.AddFileOption("!1-deh");
  GArgs.AddFileOption("!1-vc-decorate-ignore-file");

  FSYS_InitOptions(GParsedArgs);

  GParsedArgs.RegisterFlagSet("-c", "compile VavoomC/decorate code and immediately exit", &cli_CompileAndExit);

  GParsedArgs.RegisterFlagSet("-skip-sounds", "skip sounds in the following pwads", &pwflag_SkipSounds);
  GParsedArgs.RegisterFlagReset("-allow-sounds", "allow sounds in the following pwads", &pwflag_SkipSounds);
  // aliases
  GParsedArgs.RegisterFlagSet("-skipsounds", nullptr, &pwflag_SkipSounds);
  GParsedArgs.RegisterFlagReset("-allowsounds", nullptr, &pwflag_SkipSounds);

  GParsedArgs.RegisterFlagSet("-skip-sprites", "skip sprites in the following pwads", &pwflag_SkipSprites);
  GParsedArgs.RegisterFlagReset("-allow-sprites", "allow sprites in the following pwads", &pwflag_SkipSprites);
  // aliases
  GParsedArgs.RegisterFlagSet("-skipsprites", nullptr, &pwflag_SkipSprites);
  GParsedArgs.RegisterFlagReset("-allowsprites", nullptr, &pwflag_SkipSprites);

  GParsedArgs.RegisterFlagSet("-skip-dehacked", "skip dehacked in the following pwads", &pwflag_SkipDehacked);
  GParsedArgs.RegisterFlagReset("-allow-dehacked", "allow dehacked in the following pwads", &pwflag_SkipDehacked);
  // aliases
  GParsedArgs.RegisterFlagSet("-skipdehacked", nullptr, &pwflag_SkipDehacked);
  GParsedArgs.RegisterFlagReset("-allowdehacked", nullptr, &pwflag_SkipDehacked);

  GParsedArgs.RegisterFlagSet("-oldsprites", nullptr, &cli_oldSprites);
  GParsedArgs.RegisterAlias("-old-sprites", "-oldsprites");


  // filename collector
  GParsedArgs.RegisterCallback(nullptr, nullptr, [] (VArgs &args, int idx) -> int { return cliFnameCollector(args, idx); });
  GParsedArgs.RegisterCallback("-file", "add the following arguments as pwads", [] (VArgs &args, int idx) -> int { return cliFnameCollector(args, ++idx); });


  // modes collector
  GParsedArgs.RegisterCallback("-mode", "activate game mode from 'modes.rc'", [] (VArgs &args, int idx) -> int {
    ++idx;
    if (!VParsedArgs::IsArgBreaker(args, idx)) {
      VStr mn = args[idx++];
      if (!mn.isEmpty()) cliModesList.append(mn);
    }
    return idx;
  });


  // automatic groups collector
  GParsedArgs.RegisterCallback("-autoload", "activate game mode from 'autoload.rc'", [] (VArgs &args, int idx) -> int {
    ++idx;
    if (!VParsedArgs::IsArgBreaker(args, idx)) {
      VStr sg = args[idx++];
      if (!sg.isEmpty()) {
        GroupMask gi;
        gi.mask = sg;
        gi.enabled = true;
        if (gi.isGlob()) {
          cliGroupMask.append(gi);
        } else {
          cliGroupMap.put(sg.toLowerCase(), gi.enabled);
        }
      }
    }
    return idx;
  });
  GParsedArgs.RegisterAlias("-auto", "-autoload");
  GParsedArgs.RegisterAlias("-auto-load", "-autoload");

  GParsedArgs.RegisterCallback("-skip-autoload", "skip game mode from 'autoload.rc'", [] (VArgs &args, int idx) -> int {
    ++idx;
    if (!VParsedArgs::IsArgBreaker(args, idx)) {
      VStr sg = args[idx++];
      if (!sg.isEmpty()) {
        GroupMask gi;
        gi.mask = sg;
        gi.enabled = false;
        if (gi.isGlob()) {
          cliGroupMask.append(gi);
        } else {
          cliGroupMap.put(sg.toLowerCase(), gi.enabled);
        }
      }
    }
    return idx;
  });
  GParsedArgs.RegisterAlias("-skipauto", "-skip-autoload");
  GParsedArgs.RegisterAlias("-skip-auto-load", "-skip-autoload");

  // game type
  GParsedArgs.RegisterStringOption("-game", "select game type", &cliGameCStr);

  // add known game aliases
  //FIXME: unhardcode this!
  GParsedArgs.RegisterCallback("-doom", "select Doom/Ultimate Doom game", [] (VArgs &args, int idx) -> int { cliGameCStr = nullptr; cliGameMode = "doom"; return 0; });
  GParsedArgs.RegisterAlias("-doom1", "-doom");
  GParsedArgs.RegisterCallback("-doom2", "select Doom II game", [] (VArgs &args, int idx) -> int { cliGameCStr = nullptr; cliGameMode = "doom2"; return 0; });
  GParsedArgs.RegisterCallback("-tnt", "select TNT Evilution game", [] (VArgs &args, int idx) -> int { cliGameCStr = nullptr; cliGameMode = "tnt"; return 0; });
  GParsedArgs.RegisterAlias("-evilution", "-tnt");
  GParsedArgs.RegisterCallback("-plutonia", "select Plutonia game", [] (VArgs &args, int idx) -> int { cliGameCStr = nullptr; cliGameMode = "plutonia"; return 0; });
  GParsedArgs.RegisterCallback("-nerve", "select Doom II + No Rest for the Living game", [] (VArgs &args, int idx) -> int { cliGameCStr = nullptr; cliGameMode = "nerve"; return 0; });

  GParsedArgs.RegisterCallback("-freedoom", "select Freedoom Phase I game", [] (VArgs &args, int idx) -> int { cliGameCStr = nullptr; cliGameMode = "freedoom"; return 0; });
  GParsedArgs.RegisterAlias("-freedoom1", "-freedoom");
  GParsedArgs.RegisterCallback("-freedoom2", "select Freedoom Phase II game", [] (VArgs &args, int idx) -> int { cliGameCStr = nullptr; cliGameMode = "freedoom2"; return 0; });

  GParsedArgs.RegisterCallback("-heretic", "select Heretic game", [] (VArgs &args, int idx) -> int { cliGameCStr = nullptr; cliGameMode = "heretic"; return 0; });
  GParsedArgs.RegisterCallback("-hexen", "select Hexen game", [] (VArgs &args, int idx) -> int { cliGameCStr = nullptr; cliGameMode = "hexen"; return 0; });

  GParsedArgs.RegisterCallback("-strife", "select Strife game", [] (VArgs &args, int idx) -> int { cliGameCStr = nullptr; cliGameMode = "strife"; return 0; });
  GParsedArgs.RegisterCallback("-strifeteaser", "select Strife Teaser game", [] (VArgs &args, int idx) -> int { cliGameCStr = nullptr; cliGameMode = "strifeteaser"; return 0; });

  // hidden
  GParsedArgs.RegisterCallback("-complete", nullptr, [] (VArgs &args, int idx) -> int { cliGameCStr = nullptr; cliGameMode = "complete"; return 0; });
  GParsedArgs.RegisterCallback("-chex", nullptr, [] (VArgs &args, int idx) -> int { cliGameCStr = nullptr; cliGameMode = "chex"; return 0; });

  GParsedArgs.RegisterFlagSet("-k8runmap", "try to detect and run first pwad map automatically", &doStartMap);

  GParsedArgs.RegisterFlagSet("-fast", "fast monsters", &cli_FastMonsters);
  GParsedArgs.RegisterFlagReset("-slow", "slow monsters", &cli_FastMonsters);

  GParsedArgs.RegisterFlagSet("-respawn", "turn on respawning", &cli_Respawn);
  GParsedArgs.RegisterFlagReset("-no-respawn", nullptr, &cli_Respawn);

  GParsedArgs.RegisterFlagSet("-nomonsters", "disable monsters", &cli_NoMonsters);
  GParsedArgs.RegisterAlias("-no-monsters", "-nomonsters");
  GParsedArgs.RegisterFlagReset("-monsters", nullptr, &cli_NoMonsters);

  GParsedArgs.RegisterFlagSet("-nomenudef", "disable gzdoom MENUDEF parsing", &cli_NoMenuDef);
  GParsedArgs.RegisterAlias("-no-menudef", "-nomenudef");
  GParsedArgs.RegisterFlagReset("-allow-menudef", nullptr, &cli_NoMenuDef);

  GParsedArgs.RegisterFlagSet("-gore", "enable gore mod", &cli_GoreMod);
  GParsedArgs.RegisterFlagReset("-nogore", "disable gore mod", &cli_GoreMod);
  GParsedArgs.RegisterAlias("-no-gore", "-nogore");

  GParsedArgs.RegisterFlagSet("-bdw", "enable BDW (weapons) mod", &cli_BDWMod);
  GParsedArgs.RegisterFlagReset("-nobdw", "disable BDW (weapons) mod", &cli_BDWMod);
  GParsedArgs.RegisterAlias("-no-bdw", "-nobdw");

  // hidden
  GParsedArgs.RegisterFlagSet("-skeehud", nullptr, &cli_SkeeHUD);

  // "-skill"
  GParsedArgs.RegisterCallback("-skill", "select starting skill (3 is HMP; default is UV aka 4)", [] (VArgs &args, int idx) -> int {
    ++idx;
    if (!VParsedArgs::IsArgBreaker(args, idx)) {
      int skn = -1;
      if (!VStr::convertInt(args[idx], &skn)) {
             if (VStr::strEquCI(args[idx], "itytd") || VStr::strEquCI(args[idx], "baby")) skn = 1;
        else if (VStr::strEquCI(args[idx], "hntr") || VStr::strEquCI(args[idx], "easy")) skn = 2;
        else if (VStr::strEquCI(args[idx], "hmp") || VStr::strEquCI(args[idx], "medium")) skn = 3;
        else if (VStr::strEquCI(args[idx], "uv") || VStr::strEquCI(args[idx], "hard")) skn = 4;
        else if (VStr::strEquCI(args[idx], "nightmare")) skn = 5;
        else { GCon->Logf(NAME_Warning, "unknown skill name '%s'!", args[idx]); skn = 3; }
      }
      if (skn < 1) skn = 1; //else if (skn > 5) skn = 5;
      Skill = skn-1;
      ++idx;
    } else {
      GCon->Log(NAME_Warning, "skill name expected!");
    }
    return idx;
  });

  GParsedArgs.RegisterStringOption("-iwad", "override iwad file name", &cli_IWadName);
  GParsedArgs.RegisterStringOption("-iwaddir", "set directory to look for iwads", &cli_IWadDir);
  GParsedArgs.RegisterStringOption("-basedir", "set directory to look for base k8vavoom pk3s", &cli_BaseDir);
  GParsedArgs.RegisterStringOption("-savedir", "set directory to store saves and config files", &cli_SaveDir);

  // "-warp"
  GParsedArgs.RegisterCallback("-warp", "warp to map number", [] (VArgs &args, int idx) -> int {
    ++idx;
    int wmap1 = -1, wmap2 = -1;
    bool mapok = false, epiok = false;
    if (!VParsedArgs::IsArgBreaker(args, idx)) {
      mapok = VStr::convertInt(args[idx], &wmap1);
      if (mapok) {
        ++idx;
        if (!VParsedArgs::IsArgBreaker(args, idx)) {
          epiok = VStr::convertInt(args[idx], &wmap2);
          if (epiok) ++idx;
        }
      }
      if (!mapok) wmap1 = -1;
      if (!epiok) wmap2 = -1;
    }
    if (wmap1 < 0) wmap1 = -1;
    if (wmap2 < 0) wmap2 = -1;
    fsys_warp_n0 = wmap1;
    fsys_warp_n1 = wmap2;
    return idx;
  });

  GParsedArgs.RegisterFlagSet("-reportiwad", "report loaded iwads (disabled by default)", &reportIWads);
  GParsedArgs.RegisterAlias("-reportiwads", "-reportiwad");

  GParsedArgs.RegisterFlagSet("-silentpwads", "do not report loaded iwads (report by default)", &reportPWads);
  GParsedArgs.RegisterAlias("-silentpwad", "-silentpwads");
  GParsedArgs.RegisterAlias("-silencepwad", "-silentpwads");
  GParsedArgs.RegisterAlias("-silencepwads", "-silentpwads");

  GParsedArgs.RegisterFlagSet("-nakedbase", "skip all autoloads", &cli_NakedBase);
  GParsedArgs.RegisterAlias("-naked-base", "-nakedbase");

  // hidden
  GParsedArgs.RegisterFlagSet("-Wall", nullptr, &cli_WAll);
}


//==========================================================================
//
//  FL_Init
//
//==========================================================================
void FL_Init () {
  const char *p;
  VStr mainIWad = VStr();

  fsys_warp_n0 = -1;
  fsys_warp_n1 = -1;
  fsys_warp_cmd = VStr();

  // if it is set, than it was the latest
  if (cliGameCStr) cliGameMode = cliGameCStr;

       if (cli_FastMonsters == 1) fastparm = 1;
  else if (cli_FastMonsters == 0) fastparm = -1;

  if (cli_Respawn == 1) respawnparm = true;
  if (cli_NoMonsters == 1) NoMonsters = true;
  if (cli_NoMenuDef == 1) gz_skip_menudef = true;

  bool isChex = false;
  if (cliGameMode.strEquCI("chex")) {
    cliGameMode = "doom2"; // arbitrary
    //game_override_mode = GAME_Chex;
    fsys_onlyOneBaseFile = true; // disable autoloads
    isChex = true;
  }

  if (cli_IWadName && cli_IWadName[0]) mainIWad = cli_IWadName;


  fsys_report_added_paks = !!reportIWads;

  if (!isChex) fsys_onlyOneBaseFile = (cli_NakedBase > 0);

  // set up base directory (main data files)
  fl_basedir = ".";
  p = cli_BaseDir;
  if (p) {
    fl_basedir = p;
    if (fl_basedir.isEmpty()) fl_basedir = ".";
  } else {
    static const char *defaultBaseDirs[] = {
#ifdef __SWITCH__
      "/switch/k8vavoom",
      ".",
#elif !defined(_WIN32)
      "/opt/vavoom/share/k8vavoom",
      "/opt/k8vavoom/share/k8vavoom",
      "/usr/local/share/k8vavoom",
      "/usr/share/k8vavoom",
      "!/../share/k8vavoom",
      ".",
#else
      "!/share",
      "!/.",
      ".",
#endif
      nullptr,
    };
    for (const char **tbd = defaultBaseDirs; *tbd; ++tbd) {
      VStr dir = VStr(*tbd);
      if (dir[0] == '!') {
        dir.chopLeft(1);
        dir = GParsedArgs.getBinDir()+dir;
      } else if (dir[0] == '~') {
        dir.chopLeft(1);
        const char *hdir = getenv("HOME");
        if (hdir && hdir[0]) {
          dir = VStr(hdir)+dir;
        } else {
          continue;
        }
      }
      if (Sys_DirExists(dir)) {
        fl_basedir = dir;
        break;
      }
    }
    if (fl_basedir.isEmpty()) Sys_Error("cannot find basedir; use \"-basedir dir\" to set it");
  }

  // set up save directory (files written by engine)
  p = cli_SaveDir;
  if (p && p[0]) {
    fl_savedir = p;
  } else {
#if !defined(_WIN32)
    const char *HomeDir = getenv("HOME");
    if (HomeDir && HomeDir[0]) fl_savedir = VStr(HomeDir)+"/.k8vavoom";
#else
    fl_savedir = ".";
#endif
  }

  // set up additional directories where to look for IWAD files
  p = cli_IWadDir;
  if (p) {
    if (p[0]) IWadDirs.Append(p);
  } else {
    static const char *defaultIwadDirs[] = {
      ".",
      "!/.",
#ifdef __SWITCH__
      "/switch/k8vavoom/iwads",
      "/switch/k8vavoom",
#elif !defined(_WIN32)
      "~/.k8vavoom/iwads",
      "~/.k8vavoom/iwad",
      "~/.k8vavoom",
      "~/.vavoom/iwads",
      "~/.vavoom/iwad",
      "~/.vavoom",
      "/opt/vavoom/share/k8vavoom",
      "/opt/k8vavoom/share/k8vavoom",
      "/usr/local/share/k8vavoom",
      "/usr/share/k8vavoom",
      "!/../share/k8vavoom",
#else
      "~",
      "!/iwads",
      "!/iwad",
#endif
      nullptr,
    };
    for (const char **tbd = defaultIwadDirs; *tbd; ++tbd) {
      VStr dir = VStr(*tbd);
      if (dir[0] == '!') {
        dir.chopLeft(1);
        dir = GParsedArgs.getBinDir()+dir;
      } else if (dir[0] == '~') {
        dir.chopLeft(1);
        const char *hdir = getenv("HOME");
        if (hdir && hdir[0]) {
          dir = VStr(hdir)+dir;
        } else {
          continue;
        }
      }
      if (Sys_DirExists(dir)) IWadDirs.Append(dir);
    }
  }
  // envvar
  {
    const char *dwd = getenv("DOOMWADDIR");
    if (dwd && dwd[0]) IWadDirs.Append(dwd);
  }
#ifdef _WIN32
  // home dir (if any)
  /*
  {
    const char *hd = getenv("HOME");
    if (hd && hd[0]) IWadDirs.Append(hd);
  }
  */
#endif
  // and current dir
  //IWadDirs.Append(".");

  ParseUserModes();

  AddGameDir("basev/common");

  //collectPWads();

  ParseBase("basev/games.txt", mainIWad);
#ifdef DEVELOPER
  // i need progs to be loaded from files
  //fl_devmode = true;
#endif

  // process "warp", do it here, so "+var" will be processed after "map nnn"
  // postpone, and use `P_TranslateMapEx()` in host initialization
  if (warpTpl.length() > 0 && fsys_warp_n0 >= 0) {
    int fmtc = countFmtHash(warpTpl);
    if (fmtc >= 1 && fmtc <= 2) {
      if (fmtc == 2 && fsys_warp_n1 == -1) { fsys_warp_n1 = fsys_warp_n0; fsys_warp_n0 = 1; } // "-warp n" is "-warp 1 n" for ExMx
      VStr cmd = "map ";
      int spos = 0;
      int numidx = 0;
      while (spos < warpTpl.length()) {
        if (warpTpl[spos] == '#') {
          int len = 0;
          while (spos < warpTpl.length() && warpTpl[spos] == '#') { ++len; ++spos; }
          char tbuf[64];
          snprintf(tbuf, sizeof(tbuf), "%d", (numidx == 0 ? fsys_warp_n0 : fsys_warp_n1));
          VStr n = VStr(tbuf);
          while (n.length() < len) n = VStr("0")+n;
          cmd += n;
          ++numidx;
        } else {
          cmd += warpTpl[spos++];
        }
      }
      cmd += "\n";
      //GCmdBuf.Insert(cmd);
      fsys_warp_cmd = cmd;
      fsys_warp_n0 = -1;
      fsys_warp_n1 = -1;
    }
  }

  if (!customMode.disableGoreMod) {
    if (/*game_release_mode ||*/ isChex) {
      if (cli_GoreMod == 1) AddGameDir("basev/mods/gore"); // explicitly enabled
    } else {
      if (cli_GoreMod != 0) AddGameDir("basev/mods/gore"); // not disabled
    }
  } else {
    GCon->Logf(NAME_Init, "Gore mod disabled");
  }

  if (isChex) AddGameDir("basev/mods/chex");

  // mark "iwad" flags
  for (int i = 0; i < SearchPaths.length(); ++i) {
    SearchPaths[i]->iwad = true;
  }

  // load custom mode pwads
  if (customMode.disableBloodReplacement) fsys_DisableBloodReplacement = true;
  if (customMode.disableBDW) fsys_DisableBDW = true;
  CustomModeLoadPwads(CM_PRE_PWADS);

  int mapnum = -1;
  VStr mapname;
  bool mapinfoFound = false;

  // mount pwads
  fsys_report_added_paks = reportPWads;
  for (int pwidx = 0; pwidx < pwadList.length(); ++pwidx) {
    PWadFile &pwf = pwadList[pwidx];
    fsys_skipSounds = pwf.skipSounds;
    fsys_skipSprites = pwf.skipSprites;
    int nextfid = W_NextMountFileId();

    if (pwf.asDirectory) {
      if (pwf.storeInSave) wpkAppend(pwf.fname, false); // non-system pak
      GCon->Logf(NAME_Init, "Mounting directory '%s' as emulated PK3 file.", *pwf.fname);
      //AddPakDir(pwf.fname);
      W_MountDiskDir(pwf.fname);
    } else {
      if (pwf.storeInSave) wpkAppend(pwf.fname, false); // non-system pak
      AddAnyFile(pwf.fname, true);
    }
    fsys_hasPwads = true;

    //GCon->Log("**************************");
    if (doStartMap && !mapinfoFound) {
      //GCon->Logf("::: %d : %d", nextfid, W_NextMountFileId());
      for (; nextfid < W_NextMountFileId(); ++nextfid) {
        if (W_CheckNumForNameInFile(NAME_mapinfo, nextfid) >= 0 ||
            W_CheckNumForNameInFile(VName("zmapinfo", VName::Add), nextfid) >= 0)
        {
          GCon->Logf(NAME_Init, "FOUND 'mapinfo'!");
          mapinfoFound = true;
          fsys_hasMapPwads = true;
          break;
        }
        int midx = -1;
        VStr mname = W_FindMapInLastFile(nextfid, &midx);
        if (mname.length() && (mapnum < 0 || midx < mapnum)) {
          mapnum = midx;
          mapname = mname;
          fsys_hasMapPwads = true;
        }
      }
    } else if (!fsys_hasMapPwads) {
      for (; nextfid < W_NextMountFileId(); ++nextfid) {
        if (W_CheckNumForNameInFile(NAME_mapinfo, nextfid) >= 0 ||
            W_CheckNumForNameInFile(VName("zmapinfo", VName::Add), nextfid) >= 0)
        {
          fsys_hasMapPwads = true;
          break;
        }
        int midx = -1;
        VStr mname = W_FindMapInLastFile(nextfid, &midx);
        if (mname.length() && (mapnum < 0 || midx < mapnum)) {
          fsys_hasMapPwads = true;
          break;
        }
      }
    }
  }
  fsys_skipSounds = false;
  fsys_skipSprites = false;

  // load custom mode pwads
  CustomModeLoadPwads(CM_POST_PWADS);

  fsys_report_added_paks = reportIWads;

  if (!fsys_DisableBDW && cli_BDWMod > 0) AddGameDir("basev/mods/bdw");

  if (cli_SkeeHUD > 0 || fsys_detected_mod == AD_SKULLDASHEE) {
    if (fsys_detected_mod == AD_SKULLDASHEE) GCon->Logf(NAME_Init, "SkullDash EE detected, loading HUD");
    AddGameDir("basev/mods/skeehud");
  }

  fsys_report_added_paks = reportPWads;

  RenameSprites();

  if (doStartMap && fsys_warp_cmd.isEmpty() && !mapinfoFound && mapnum > 0 && mapname.length()) {
    GCon->Logf(NAME_Init, "FOUND MAP: %s", *mapname);
    mapname = va("map \"%s\"\n", *mapname.quote());
    //GCmdBuf.Insert(mapname);
    fsys_warp_cmd = mapname;
  } else if (doStartMap && fsys_warp_cmd.isEmpty() && mapinfoFound) {
    fsys_warp_cmd = "__k8_run_first_map";
  }

  // look for "+map"
  if (fsys_warp_cmd.length() == 0) {
    if (GArgs.CheckParm("+map") != 0) Host_CLIMapStartFound();
  }
}


//==========================================================================
//
//  FL_Shutdown
//
//==========================================================================
void FL_Shutdown () {
  fl_basedir.Clean();
  fl_savedir.Clean();
  fl_gamedir.Clean();
  IWadDirs.Clear();
  FSYS_Shutdown();
}


//==========================================================================
//
//  FL_OpenFileWrite
//
//==========================================================================
VStream *FL_OpenFileWrite (VStr Name, bool isFullName) {
  VStr tmpName;
  if (isFullName) {
    tmpName = Name;
  } else {
    if (fl_savedir.IsNotEmpty()) {
      tmpName = fl_savedir+"/"+fl_gamedir+"/"+Name;
    } else {
      tmpName = fl_basedir+"/"+fl_gamedir+"/"+Name;
    }
  }
  return FL_OpenSysFileWrite(tmpName);
}


//==========================================================================
//
//  FL_OpenFileReadInCfgDir
//
//==========================================================================
VStream *FL_OpenFileReadInCfgDir (VStr Name) {
  VStr diskName = FL_GetConfigDir()+"/"+Name;
  FILE *File = fopen(*diskName, "rb");
  if (File) return new VStdFileStreamRead(File, diskName);
  return FL_OpenFileRead(Name);
}


//==========================================================================
//
//  FL_OpenFileWriteInCfgDir
//
//==========================================================================
VStream *FL_OpenFileWriteInCfgDir (VStr Name) {
  VStr diskName = FL_GetConfigDir()+"/"+Name;
  return FL_OpenSysFileWrite(diskName);
}


//==========================================================================
//
//  FL_GetConfigDir
//
//==========================================================================
VStr FL_GetConfigDir () {
  VStr res;
#if !defined(_WIN32)
  const char *HomeDir = getenv("HOME");
  if (HomeDir && HomeDir[0]) {
    res = VStr(HomeDir)+"/.k8vavoom";
    Sys_CreateDirectory(res);
  } else {
    //res = (fl_savedir.IsNotEmpty() ? fl_savedir : fl_basedir);
    res = (fl_savedir.IsNotEmpty() ? fl_savedir : ".");
  }
#else
  //res = (fl_savedir.IsNotEmpty() ? fl_savedir : fl_basedir);
  res = (fl_savedir.IsNotEmpty() ? fl_savedir : ".");
#endif
  Sys_CreateDirectory(res);
  return res;
}


//==========================================================================
//
//  FL_GetCacheDir
//
//==========================================================================
VStr FL_GetCacheDir () {
  VStr res = FL_GetConfigDir();
  if (res.isEmpty()) return res;
  res += "/.mapcache";
  Sys_CreateDirectory(res);
  return res;
}


//==========================================================================
//
//  FL_GetSavesDir
//
//==========================================================================
VStr FL_GetSavesDir () {
  VStr res = FL_GetConfigDir();
  if (res.isEmpty()) return res;
  res += "/saves";
  Sys_CreateDirectory(res);
  return res;
}


//==========================================================================
//
//  FL_GetScreenshotsDir
//
//==========================================================================
VStr FL_GetScreenshotsDir () {
  VStr res = FL_GetConfigDir();
  if (res.isEmpty()) return res;
  res += "/sshots";
  Sys_CreateDirectory(res);
  return res;
}


//==========================================================================
//
//  FL_GetUserDataDir
//
//==========================================================================
VStr FL_GetUserDataDir (bool shouldCreate) {
  VStr res = FL_GetConfigDir();
  res += "/userdata";
  //res += '/'; res += game_name;
  if (shouldCreate) Sys_CreateDirectory(res);
  return res;
}
