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
#include "fsys_local.h"
/*#include "../libvwad/vwadvfs.h"*/


EName fsys_report_added_paks_logtype = NAME_Init;

int fsys_simple_archives = FSYS_ARCHIVES_NORMAL;
bool fsys_developer_debug = false;
int fsys_IgnoreZScript = 1;
bool fsys_DisableBDW = false;
bool fsys_report_added_paks = true;
bool fsys_WarningReportsEnabled = true;

bool fsys_skipSounds = false;
bool fsys_skipSprites = false;
bool fsys_skipDehacked = false;

// autodetected wad/pk3
int fsys_detected_mod = AD_NONE;
VStr fsys_detected_mod_wad;

// local
int fsys_dev_dump_paks = 0;


TArray<VSearchPath *> fsysSearchPaths;
TArray<VStr> fsysWadFileNames;
TArray<VStr> fsys_game_filters;

mythread_mutex fsys_glock;


struct KnownKey {
  FSysPublicKey key;
  char *signer;
  KnownKey *next;
};


static KnownKey *knownKeyList;


//==========================================================================
//
//  FSYS_DecodePublicKey
//
//==========================================================================
bool FSYS_DecodePublicKey (const char *asciikeystr, FSysPublicKey key) {
  if (!key || !asciikeystr || !asciikeystr[0]) return false;
  vwad_z85_key kstr;
  size_t dpos = 0;
  char ch;
  do {
    ch = *asciikeystr++;
    if (ch > 32 && ch < 127) {
      if (dpos == sizeof(kstr)) return false;
      kstr[dpos++] = ch;
    }
  } while (ch != 0);
  if (dpos != sizeof(kstr) - 1u) return false;
  kstr[dpos] = 0;
  if (vwad_z85_decode_key(kstr, key) != 0) return false;
  return true;
}


//==========================================================================
//
//  FSYS_FindPublicKey
//
//  returns NULL for unknown key
//
//==========================================================================
const char *FSYS_FindPublicKey (const FSysPublicKey key) {
  if (key != nullptr) {
    for (KnownKey *k = knownKeyList; k != nullptr; k = k->next) {
      if (memcmp(k->key, key, 32) == 0) return k->signer;
    }
  }
  return NULL;
}


//==========================================================================
//
//  FSYS_RegisterPublicKey
//
//  empty/NULL `signer` means "unknown signer"
//
//==========================================================================
void FSYS_RegisterPublicKey (const char *signer, const FSysPublicKey key) {
  KnownKey *k;
  KnownKey *p;

  if (!key) return;
  if (signer == nullptr) signer = "";

  while (signer[0] && (uint8_t)signer[0] <= 32) ++signer;

  size_t slen = strlen(signer);
  char *sgn;
  if (slen > 127) {
    slen = 127;
    sgn = (char *)Z_Malloc(slen + 16);
    if (sgn == nullptr) return;
    memcpy(sgn, signer, slen);
    sgn[slen] = 0; strcat(sgn, "...");
  } else {
    sgn = (char *)Z_Malloc(slen + 1);
    if (sgn == nullptr) return;
    strcpy(sgn, signer);
  }

  slen = strlen(sgn);
  while (slen != 0 && (uint8_t)sgn[slen - 1] <= 32) --slen;
  sgn[slen] = 0;

  if (!sgn[slen]) {
    Z_Free(sgn);
    p = nullptr;
    k = knownKeyList;
    while (k != nullptr && memcmp(k->key, key, 32) != 0) {
      p = k; k = k->next;
    }
    if (k != nullptr) {
      if (p != nullptr) p->next = k->next; else knownKeyList = k->next;
      Z_Free(k->signer);
      Z_Free(k);
    }
  } else {
    for (size_t f = 0; f < slen; ++f) {
      if (sgn[f] < 32 || sgn[f] >= 127) sgn[f] = '?';
    }
    k = knownKeyList;
    while (k != nullptr && memcmp(k->key, key, 32) != 0) k = k->next;
    if (k) {
      Z_Free(k->signer);
      k->signer = sgn;
    } else {
      k = (KnownKey *)Z_Calloc(sizeof(KnownKey));
      if (k == nullptr) { Z_Free(sgn); return; }
      memcpy(k->key, key, 32);
      k->signer = sgn;
      k->next = knownKeyList;
      knownKeyList = k;
    }
  }
}


//==========================================================================
//
//  FSysSavedState::needReload
//
//==========================================================================
bool FSysSavedState::needReload () const noexcept {
  for (auto &&sp : svSearchPaths) if (sp->HadFilters()) return true;
  return false;
}


//==========================================================================
//
//  FSysSavedState::save
//
//  this clears current archives
//
//==========================================================================
void FSysSavedState::save () {
  if (saved) Sys_Error("FSysSavedState: double save");
  saved = true;
  svSearchPaths = fsysSearchPaths;
  fsysSearchPaths.reset();
  svwadfiles = fsysWadFileNames;
  fsysWadFileNames.reset();
}


//==========================================================================
//
//  FSysSavedState::unload
//
//  free all loaded archives (they must be reloaded)
//  call this instead of `restore` if `needReload()` returned `true`
//
//==========================================================================
void FSysSavedState::unload () {
  if (!saved) Sys_Error("FSysSavedState: cannot restore empty save");
  saved = false;
  for (auto &&it : svSearchPaths) { delete it; it = nullptr; }
  svSearchPaths.clear();
  svwadfiles.clear();
}


//==========================================================================
//
//  FSysSavedState::restore
//
//  this resets saved state
//
//==========================================================================
void FSysSavedState::restore () {
  if (!saved) Sys_Error("FSysSavedState: cannot restore empty save");
  saved = false;
  for (auto &&it : svSearchPaths) fsysSearchPaths.append(it);
  for (auto &&it : svwadfiles) fsysWadFileNames.append(it);
  svSearchPaths.clear();
  svwadfiles.clear();
}


// ////////////////////////////////////////////////////////////////////////// //
class FSys_Internal_Init_Class {
public:
  FSys_Internal_Init_Class (bool) {
    mythread_mutex_init(&fsys_glock);
  }
};

__attribute__((used)) FSys_Internal_Init_Class fsys_internal_init_class_variable_(true);


//==========================================================================
//
//  FSYS_Shutdown
//
//==========================================================================
void FSYS_InitOptions (VParsedArgs &pargs) {
  pargs.RegisterFlagSet("-ignore-zscript", "!", &fsys_IgnoreZScript);
  pargs.RegisterFlagSet("-fsys-dump-paks", "!dump loaded pak files", &fsys_dev_dump_paks);
}


//==========================================================================
//
//  FSYS_Shutdown
//
//==========================================================================
void FSYS_Shutdown () {
  MyThreadLocker glocker(&fsys_glock);
  for (int i = 0; i < fsysSearchPaths.length(); ++i) {
    delete fsysSearchPaths[i];
    fsysSearchPaths[i] = nullptr;
  }
  fsysSearchPaths.Clear();
  fsysWadFileNames.Clear();
}


//==========================================================================
//
//  FL_ClearGameFilters
//
//==========================================================================
void FL_ClearGameFilters () {
  fsys_game_filters.clear();
}


//==========================================================================
//
//  cmpGameFilter
//
//  the best filter (the longest one) is always at the top
//
//==========================================================================
static int cmpGameFilter (const void *aa, const void *bb, void *) {
  if (aa == bb) return 0;
  const VStr *a = (const VStr *)aa;
  const VStr *b = (const VStr *)bb;
  const auto alen = a->length();
  const auto blen = b->length();
  if (alen < blen) return +1; // a.length is less then b.length, b should come first
  if (alen > blen) return -1; // a.length is greater then b.length, a should come first
  // we don't need alphabetical ordering, nobody cares
  return 0;
}


//==========================================================================
//
//  FL_AddGameFilter
//
//  add new filter; it should start with "filter/"
//  duplicate filters will be ignored
//  returns 0 for "no error"
//
//==========================================================================
int FL_AddGameFilter (VStr path) {
  if (path.isEmpty()) return FL_ADDFILTER_INVALID;
  path = path.toLowerCase().fixSlashes();
  while (path.length() && path[0] == '/') path.chopLeft(1);
  while (path.length() && path.endsWith("/")) path.chopRight(1);
  if (path.isEmpty()) return FL_ADDFILTER_INVALID;
  if (!path.startsWith("filter/")) return FL_ADDFILTER_INVALID;
  if (path.length() < 8) return FL_ADDFILTER_INVALID; // just in case
  // look for duplicates
  for (auto &&flt : fsys_game_filters) if (flt.strEqu(path)) return FL_ADDFILTER_DUPLICATE;
  fsys_game_filters.append(path);
  // sort them, because why not?
  //GLog.Log(NAME_Debug, ":::: B: ===="); for (auto &&s : fsys_game_filters) GLog.Logf(NAME_Debug, "  <%s>", *s);
  timsort_r(fsys_game_filters.ptr(), fsys_game_filters.length(), sizeof(VStr), &cmpGameFilter, nullptr);
  //GLog.Log(NAME_Debug, ":::: A: ===="); for (auto &&s : fsys_game_filters) GLog.Logf(NAME_Debug, "  <%s>", *s);
  return FL_ADDFILTER_OK;
}


//==========================================================================
//
//  FL_CheckFilterName
//
//  returns `false` if file was filtered out (and clears name)
//  returns `true` if file should be kept (and modifies name if necessary)
//  file name *MUST* be lowercased, and path components *MUST* be
//  separated by a single slash
//
//==========================================================================
bool FL_CheckFilterName (VStr &fname) {
  //GLog.Logf(NAME_Debug, "000: CHECKFILTER: <%s>", *fname);
  if (fname.isEmpty()) return false; // empty names should not be kept ;-)
  if (!fname.startsWith("filter/")) return true; // keep it
  //GLog.Logf(NAME_Debug, "001: CHECKFILTER: <%s>", *fname);
  // this prevents reading archives without filtering, but meh
  // special filter for our engine
  if (fname.startsWith("filter/engine_k8vavoom/")) fname = "filter/"+fname.leftskip(23);
  //GLog.Logf(NAME_Debug, "002: CHECKFILTER: <%s>", *fname);
  // allow "any" and "anything" for all games
  // "any"
  if (fname.startsWith("filter/any/")) {
    fname.chopLeft(11);
    if (fname.isEmpty() || fname.endsWith("/")) { fname.clear(); return false; } // drop it
    return true;
  }
  //GLog.Logf(NAME_Debug, "003: CHECKFILTER: <%s>", *fname);
  // "anything"
  if (fname.startsWith("filter/anything/")) {
    fname.chopLeft(16);
    if (fname.isEmpty() || fname.endsWith("/")) { fname.clear(); return false; } // drop it
    return true; // keep it
  }
  //GLog.Logf(NAME_Debug, "004: CHECKFILTER: <%s> (%d)", *fname, fsys_game_filters.length());
  if (fsys_game_filters.length() == 0 || fname.endsWith("/")) { fname.clear(); return false; } // drop it (it is a directory, or we have no filters set)
  //GLog.Logf(NAME_Debug, "005: CHECKFILTER: <%s>", *fname);
  // filters are sorted from the longest one to the shortest one
  // also, a filter doesn't contain trailing slash
  for (VStr fs : fsys_game_filters) {
    //GLog.Logf(NAME_Debug, "006:   CHECKFILTER: <%s> : <%s>", *fname, *fs);
    if (fname.length() > fs.length()+1 && fname[fs.length()] == '/' && fname.startsWith(fs)) {
      fname.chopLeft(fs.length()+1);
      while (fname.length() && fname[0] == '/') fname.chopLeft(1);
      return !fname.isEmpty(); // just in case
    }
  }
  // drop it
  //GLog.Logf(NAME_Debug, "007: CHECKFILTER: <%s> -- DROP", *fname);
  fname.clear();
  return false;
}


//==========================================================================
//
//  FL_FileExists
//
//==========================================================================
bool FL_FileExists (VStr fname, int *lump) {
  if (!fname.isEmpty()) {
    MyThreadLocker glocker(&fsys_glock);
    for (int i = fsysSearchPaths.length()-1; i >= 0; --i) {
      if (fsysSearchPaths[i]->FileExists(fname, lump)) return true;
    }
  }
  if (lump) *lump = -1;
  return false;
}


//==========================================================================
//
//  FL_OpenFileReadBaseOnly_NoLock
//
//  if `lump` is not `nullptr`, sets it to file lump or to -1
//
//==========================================================================
VStream *FL_OpenFileReadBaseOnly_NoLock (VStr Name, int *lump) {
  if (!Name.isEmpty()) {
    for (int i = fsysSearchPaths.length()-1; i >= 0; --i) {
      if (!fsysSearchPaths[i]->basepak) continue;
      VStream *Strm = fsysSearchPaths[i]->OpenFileRead(Name, lump);
      if (Strm) return Strm;
    }
  }
  if (lump) *lump = -1;
  return nullptr;
}


//==========================================================================
//
//  FL_OpenFileRead_NoLock
//
//  if `lump` is not `nullptr`, sets it to file lump or to -1
//
//==========================================================================
VStream *FL_OpenFileRead_NoLock (VStr Name, int *lump) {
  if (!Name.isEmpty()) {
    if (Name.length() >= 2 && Name[0] == '/' && Name[1] == '/') {
      return FL_OpenFileReadBaseOnly_NoLock(Name.leftskip(2), lump);
    } else {
      for (int i = fsysSearchPaths.length()-1; i >= 0; --i) {
        VStream *Strm = fsysSearchPaths[i]->OpenFileRead(Name, lump);
        if (Strm) return Strm;
      }
    }
  }
  if (lump) *lump = -1;
  return nullptr;
}


//==========================================================================
//
//  FL_OpenFileReadBaseOnly
//
//  if `lump` is not `nullptr`, sets it to file lump or to -1
//
//==========================================================================
VStream *FL_OpenFileReadBaseOnly (VStr Name, int *lump) {
  if (Name.isEmpty()) {
    if (lump) *lump = -1;
    return nullptr;
  }
  MyThreadLocker glocker(&fsys_glock);
  return FL_OpenFileReadBaseOnly_NoLock(Name, lump);
}


//==========================================================================
//
//  FL_OpenFileRead
//
//  if `lump` is not `nullptr`, sets it to file lump or to -1
//
//==========================================================================
VStream *FL_OpenFileRead (VStr Name, int *lump) {
  if (Name.isEmpty()) {
    if (lump) *lump = -1;
    return nullptr;
  }
  MyThreadLocker glocker(&fsys_glock);
  return FL_OpenFileRead_NoLock(Name, lump);
}


//==========================================================================
//
//  FL_CreatePath
//
//==========================================================================
void FL_CreatePath (VStr Path) {
  if (Path.isEmpty() || Path == ".") return;
  TArray<VStr> spp;
  Path.SplitPath(spp);
  if (spp.length() == 0) return;
  const bool isAbsolute = VStr::IsSplittedPathAbsolute(spp);
  if (isAbsolute && spp.length() == 1) return;
  VStr newpath;
  for (int pos = 0; pos < spp.length(); ++pos) {
    if (pos == 0 && isAbsolute) { newpath += spp[0]; continue; }
    #ifdef _WIN32
    if (pos == 1 && newpath.endsWith(":")) {} else
    #endif
    if (newpath.length() && newpath[newpath.length()-1] != '/') newpath += "/";
    newpath += spp[pos];
    if (!Sys_DirExists(newpath)) Sys_CreateDirectory(newpath);
  }
}


//==========================================================================
//
//  FL_OpenSysFileRead
//
//==========================================================================
VStream *FL_OpenSysFileRead (VStr Name) {
  if (Name.isEmpty()) return nullptr;
  return CreateDiskStreamRead(Name);
}


//==========================================================================
//
//  FL_OpenSysFileWrite
//
//==========================================================================
VStream *FL_OpenSysFileWrite (VStr Name) {
  if (Name.isEmpty()) return nullptr;
  FL_CreatePath(Name.ExtractFilePath());
  return CreateDiskStreamWrite(Name);
}


//==========================================================================
//
//  FL_IsSafeDiskFileName
//
//==========================================================================
bool FL_IsSafeDiskFileName (VStr fname) {
  if (fname.isEmpty()) return false;
  return fname.isSafeDiskFileName();
}


// ////////////////////////////////////////////////////////////////////////// //
// i have to do this, otherwise the linker will optimise openers away
#include "fsys_register.cpp"
