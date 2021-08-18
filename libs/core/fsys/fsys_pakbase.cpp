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
#include "fsys_local.h"


extern bool fsys_skipSounds;
extern bool fsys_skipSprites;
extern bool fsys_skipDehacked;
bool fsys_no_dup_reports = false;
bool fsys_hide_sprofs = false;


// ////////////////////////////////////////////////////////////////////////// //
static bool fsys_mark_as_user = false;

// all appended wads will be marked as "user wads" from this point on
void FL_StartUserWads () { fsys_mark_as_user = true; }
// stop marking user wads
void FL_EndUserWads () { fsys_mark_as_user = false; }


// ////////////////////////////////////////////////////////////////////////// //
const VPK3ResDirInfo PK3ResourceDirs[] = {
  { "sprites/", WADNS_Sprites },
  { "flats/", WADNS_Flats },
  { "colormaps/", WADNS_ColorMaps },
  { "acs/", WADNS_ACSLibrary },
  { "textures/", WADNS_NewTextures },
  { "voices/", WADNS_Voices },
  { "hires/", WADNS_HiResTextures },
  { "data/jdoom/textures/", WADNS_HiResTexturesDDay },
  { "data/jdoom/flats/", WADNS_HiResFlatsDDay },
  { "patches/", WADNS_Patches },
  { "graphics/", WADNS_Graphics },
  { "sounds/", WADNS_Sounds },
  { "music/", WADNS_Music },
  { "after_iwad/", WADNS_AfterIWad },
  { nullptr, WADNS_ZipSpecial },
};


static const char *PK3IgnoreExts[] = {
  ".wad",
  ".iwad",
  ".dfwad",
  ".dfzip",
  ".zip",
  ".7z",
  ".pk3",
  ".pk7",
  ".pak",
  ".ipk3",
  ".ipak",
  ".exe",
  ".com",
  ".bat",
  ".ini",
  ".cpp",
  //".acs",
  ".doc",
  ".me",
  ".rtf",
  ".rsp",
  ".now",
  ".htm",
  ".html",
  ".wri",
  ".nfo",
  ".diz",
  ".bbs",
  nullptr
};


//==========================================================================
//
//  VFS_ShouldIgnoreExt
//
//==========================================================================
bool VFS_ShouldIgnoreExt (VStr fname) {
  if (fname.length() == 0) return false;
  for (const char **s = PK3IgnoreExts; *s; ++s) if (fname.endsWithNoCase(*s)) return true;
  //if (fname.length() > 12 && fname.endsWithNoCase(".txt")) return true;
  return false;
}


//==========================================================================
//
//  FSysModDetectorHelper::findLump
//
//  return lump *index*, or -1
//
//==========================================================================
int FSysModDetectorHelper::findLump (const char *lumpname, int size, const char *md5) {
  if (!lumpname || !lumpname[0]) return -1;
  if (md5 && md5[0] && strlen(md5) != 32) return -1; // invalid md5
  if (strlen(lumpname) > 8) return -1;
  VName lname = VName(lumpname, VName::FindLower);
  if (lname == NAME_None) return -1;
  auto npp = dir->lumpmap.get(lname);
  if (!npp) return -1;
  for (int fidx = *npp; fidx >= 0 && fidx < dir->files.length(); fidx = dir->files[fidx].nextLump) {
    if (size >= 0 && dir->files[fidx].filesize != size) continue;
    if (md5 && md5[0] && !pak->CalculateMD5(fidx).strEquCI(md5)) continue;
    return fidx;
  }
  return -1;
}


//==========================================================================
//
//  FSysModDetectorHelper::findFile
//
//  return lump *index*, or -1
//
//==========================================================================
int FSysModDetectorHelper::findFile (const char *filename, int size, const char *md5) {
  if (!filename || !filename[0]) return -1;
  if (md5 && md5[0] && strlen(md5) != 32) return -1; // invalid md5
  VStr fname = VStr(filename).fixSlashes().toLowerCase();
  while (!fname.isEmpty() && fname[0] == '/') fname.chopLeft(1);
  if (fname.isEmpty()) return -1;
  auto npp = dir->filemap.get(fname);
  if (!npp) return -1;
  for (int fidx = *npp; fidx >= 0 && fidx < dir->files.length(); fidx = dir->files[fidx].prevFile) {
    if (size >= 0 && dir->files[fidx].filesize != size) continue;
    if (md5 && md5[0] && !pak->CalculateMD5(fidx).strEquCI(md5)) continue;
    return fidx;
  }
  return -1;
}


//==========================================================================
//
//  FSysModDetectorHelper::hasLump
//
//  hasLump("dehacked", 1066, "6bf56571d1f34d7cd7378b95556d67f8")
//
//==========================================================================
bool FSysModDetectorHelper::hasLump (const char *lumpname, int size, const char *md5) {
  return (findLump(lumpname, size, md5) >= 0);
}


//==========================================================================
//
//  FSysModDetectorHelper::hasFile
//
//  this checks for file; no globs allowed!
//
//==========================================================================
bool FSysModDetectorHelper::hasFile (const char *filename, int size, const char *md5) {
  return (findFile(filename, size, md5) >= 0);
}


//==========================================================================
//
//  FSysModDetectorHelper::checkLump
//
//  can be used to check zscript lump
//
//==========================================================================
bool FSysModDetectorHelper::checkLump (int lumpidx, int size, const char *md5) {
  if (size < 0 && (!md5 || !md5[0])) return (lumpidx >= 0 && lumpidx < dir->files.length()); // any lump will do
  // we want to check size/md5
  if (lumpidx < 0 || lumpidx >= dir->files.length()) return false; // no such lump, failed
  if (md5 && md5[0] && strlen(md5) != 32) return false; // invalid md5
  if (size >= 0 && dir->files[lumpidx].filesize != size) return false;
  if (md5 && md5[0] && !pak->CalculateMD5(lumpidx).strEquCI(md5)) return false;
  return true;
}


//==========================================================================
//
//  FSysModDetectorHelper::getLumpSize
//
//==========================================================================
int FSysModDetectorHelper::getLumpSize (int lumpidx) {
  if (lumpidx < 0 || lumpidx >= dir->files.length()) return -1;
  return dir->files[lumpidx].filesize;
}


//==========================================================================
//
//  FSysModDetectorHelper::getLumpMD5
//
//==========================================================================
VStr FSysModDetectorHelper::getLumpMD5 (int lumpidx) {
  if (lumpidx < 0 || lumpidx >= dir->files.length()) return VStr::EmptyString;
  return pak->CalculateMD5(lumpidx);
}


//==========================================================================
//
//  FSysModDetectorHelper::createLumpReader
//
//  returns `nullptr` on error
//  WARNING: ALWAYS DELETE THE STREAM!
//
//==========================================================================
VStream *FSysModDetectorHelper::createLumpReader (int lumpidx) {
  if (lumpidx < 0 || lumpidx >= dir->files.length()) return nullptr;
  return pak->CreateLumpReaderNum(lumpidx);
}


// ////////////////////////////////////////////////////////////////////////// //
static TArray<fsysModDetectorCB> modDetectorList;


//==========================================================================
//
//  fsysRegisterModDetector
//
//==========================================================================
void fsysRegisterModDetector (fsysModDetectorCB cb) {
  if (!cb) return;
  for (auto &&it : modDetectorList) if (it == cb) return;
  modDetectorList.append(cb);
}


//==========================================================================
//
//  callModDetectors
//
//==========================================================================
static int callModDetectors (VFileDirectory *adir, VPakFileBase *apak, int seenZScriptLump) {
  bool allowZShit = false;
  if (modDetectorList.length() == 0) return AD_NONE;
  FSysModDetectorHelper hlp(adir, apak);
  for (auto &&it : modDetectorList) {
    int res = it(hlp, seenZScriptLump);
    if (res == AD_ALLOW_ZSCRIPT) { allowZShit = true; res = AD_NONE; }
    if (res != AD_NONE) return res;
  }
  return (allowZShit ? AD_ALLOW_ZSCRIPT : AD_NONE);
}


//==========================================================================
//
//  VSearchPath::VSearchPath
//
//==========================================================================
VSearchPath::VSearchPath ()
  : type(PAK) // safe choice
  , iwad(false)
  , basepak(false)
  , userwad(false)
  , cosmetic(false)
  , required(false)
  , hadfilters(false)
{
  if (fsys_mark_as_user) userwad = true;
}


//==========================================================================
//
//  VSearchPath::~VSearchPath
//
//==========================================================================
VSearchPath::~VSearchPath () {
}



//==========================================================================
//
//  VFileDirectory::VFileDirectory
//
//==========================================================================
VFileDirectory::VFileDirectory ()
  : owner(nullptr)
  , files()
  , lumpmap()
  , filemap()
{
}


//==========================================================================
//
//  VFileDirectory::VFileDirectory
//
//==========================================================================
VFileDirectory::VFileDirectory (VPakFileBase *aowner, bool aaszip)
  : owner(aowner)
  , files()
  , lumpmap()
  , filemap()
  , aszip(aaszip)
{
}


//==========================================================================
//
//  VFileDirectory::~VFileDirectory
//
//==========================================================================
VFileDirectory::~VFileDirectory () {
  clear();
}


//==========================================================================
//
//  VFileDirectory::getArchiveName
//
//==========================================================================
const VStr VFileDirectory::getArchiveName () const {
  return (owner ? owner->PakFileName.cloneUniqueMT() : VStr::EmptyString);
}


//==========================================================================
//
//  VFileDirectory::append
//
//==========================================================================
void VFileDirectory::append (const VPakFileInfo &fi) {
  if (files.length() >= 65520) Sys_Error("Archive \"%s\" contains too many files", *getArchiveName());
  files.append(fi);
  //GLog.Logf(NAME_Debug, "VFileDirectory::append:<%s>: <%s>", (owner ? *owner->PakFileName : ""), *fi.fileNameIntr);
}


//==========================================================================
//
//  VFileDirectory::appendAndRegister
//
//==========================================================================
/*
int VFileDirectory::appendAndRegister (const VPakFileInfo &fi) {
  if (files.length() >= 65520) Sys_Error("Archive \"%s\" contains too many files", *getArchiveName());
  // link lumps
  int f = files.length();
  files.append(fi);
  GLog.Logf(NAME_Debug, "VFileDirectory::append:<%s>: <%s>", (owner ? *owner->PakFileName : ""), *fi.fileNameIntr);

  VPakFileInfo &nfo = files[f];
  VName lmp = nfo.lumpName;
  nfo.nextLump = -1; // just in case
  nfo.prevFile = -1; // just in case
  if (lmp != NAME_None) {
    auto lp = lumpmap.get(lmp);
    if (lp) {
      int lnum = *lp, pnum = -1;
      for (;;) {
        pnum = lnum;
        lnum = files[lnum].nextLump;
        if (lnum == -1) break;
      }
      nfo.nextLump = pnum;
    }
    lumpmap.put(lmp, f);
  }

  if (nfo.fileNameIntr.length()) {
    // put files into hashmap
    auto pfp = filemap.get(nfo.fileNameIntr);
    if (pfp) nfo.prevFile = *pfp;
    filemap.put(nfo.fileNameIntr, f);
  }

  return f;
}
*/


//==========================================================================
//
//  VFileDirectory::clear
//
//==========================================================================
void VFileDirectory::clear () {
  filemap.clear();
  lumpmap.clear();
  files.clear();
}


//==========================================================================
//
//  IsHideRemoveFilterFileName
//
//==========================================================================
static bool IsHideRemoveFilterFileName (VStr &fn) {
  VStr fext = fn.ExtractFileExtension();
  if (fext.strEqu(".hide") || fext.strEqu(".remove")) {
    fn = fn.StripExtension();
    return true;
  }
  return false;
}


//==========================================================================
//
//  VFileDirectory::buildLumpNames
//
//  won't touch entries with `lumpName != NAME_None`
//  backslashes should be already converted to normal slashes
//
//  this also performs filtering
//
//==========================================================================
void VFileDirectory::buildLumpNames () {
  if (files.length() > 65520) Sys_Error("Archive \"%s\" contains too many files", *getArchiveName());

  TMap<VStr, bool> fnamedelmap; // files to delete, from filters

  // build "files to remove" list from filters
  for (auto &&fi : files) {
    VStr fn(fi.fileNameIntr);
    if (!fn.startsWith("filter/")) continue;
    if (owner) owner->hadfilters = true;
    if (!FL_CheckFilterName(fn)) continue;
    //GLog.Logf(NAME_Debug, "FILTER FILE: <%s> -> <%s>", *fi.fileNameIntr, *fn);
    // special extensions ".hide" and ".remove" will hide the file
    (void)IsHideRemoveFilterFileName(fn);
    // put everything in "hide map", becase the corresponding files will be hard-replaced
    if (!fn.isEmpty() && !fn.endsWith("/")) fnamedelmap.put(fn, true);
  }

  // we need index too, so let's do it without iterator
  for (int i = 0; i < files.length(); ++i) {
    VPakFileInfo &fi = files[i];
    if (!fi.fileNameIntr.isLowerCase()) fi.SetFileName(fi.fileNameIntr.toLowerCase()); // just in case

    if (fi.lumpName != NAME_None) {
      if (!VStr::isLowerCase(*fi.lumpName)) {
        GLog.Logf(NAME_Warning, "Archive \"%s\" contains non-lowercase lump name '%s'", *getArchiveName(), *fi.lumpName);
      }
    } else {
      if (fi.fileNameIntr.isEmpty() || fi.fileNameIntr.endsWith("/")) continue;

      // filtering
      if (fi.fileNameIntr.startsWith("filter/")) {
        VStr fn(fi.fileNameIntr);
        if (!FL_CheckFilterName(fn)) {
          //GLog.Logf(NAME_Init, "FILTER CHECK: dropped '%s'", *fi.fileNameIntr);
          // hide this file
          fi.ClearFileName(); // hide this file
          continue;
        }
        #if 0
        else {
          GLog.Logf(NAME_Init, "FILTER CHECK: '%s' -> '%s'", *fi.fileNameIntr, *fn);
        }
        #endif
        // special extensions ".hide" and ".remove" will hide the file
        if (IsHideRemoveFilterFileName(fn)) {
          fi.ClearFileName(); // hide this file
          continue;
        }
        fi.SetFileName(fn);
      } else {
        // check "deletion map"
        if (fnamedelmap.has(fi.fileNameIntr)) {
          fi.ClearFileName(); // hide this file
          continue;
        }
      }

      // set up lump name for WAD-like access
      VStr lumpName(fi.fileNameIntr.ExtractFileName().StripExtension());

      // do not register ".rc" lumps in "k8vavoom" dir
      if (fi.fileNameIntr.startsWithCI("k8vavoom/")) {
        if (fi.fileNameIntr.endsWithCI(".rc") || lumpName.length() > 8) {
          //GLog.Logf(NAME_Debug, "***+++*** IGNORED: <%s> (%s)", *fi.fileNameIntr, *lumpName);
          fi.lumpNamespace = -1;
          continue;
        }
      }

      if (fi.lumpNamespace == -1 && !lumpName.isEmpty()) {
        // map some directories to WAD namespaces
        if (fi.fileNameIntr.IndexOf('/') == -1) {
          fi.lumpNamespace = WADNS_Global;
        } else {
          fi.lumpNamespace = -1;
          for (const VPK3ResDirInfo *di = PK3ResourceDirs; di->pfx; ++di) {
            if (fi.fileNameIntr.StartsWith(di->pfx)) {
              fi.lumpNamespace = di->wadns;
              break;
            }
          }
        }
      }

      // anything from other directories won't be accessed as lump
      if (fi.lumpNamespace == -1) {
        lumpName.clear();
      } else {
        // hide wad files, 'cause they may conflict with normal files
        // wads will be correctly added by a separate function
        if (VFS_ShouldIgnoreExt(fi.fileNameIntr)) {
          fi.lumpNamespace = -1;
          lumpName.clear();
        }
      }

      // hide "sprofs" lumps?
      if (fsys_hide_sprofs && fi.lumpNamespace == WADNS_Global && lumpName.strEqu("sprofs")) {
        fi.lumpNamespace = -1;
        lumpName.clear();
      }

      if ((fsys_skipSounds && fi.lumpNamespace == WADNS_Sounds) ||
          (fsys_skipSprites && fi.lumpNamespace == WADNS_Sprites))
      {
        fi.lumpNamespace = -1;
        lumpName.clear();
      }

      if (fsys_skipDehacked && lumpName.strEqu("dehacked")) {
        fi.lumpNamespace = -1;
        lumpName.clear();
      }

      // for sprites \ is a valid frame character, but is not allowed to
      // be in a file name, so we do a little mapping here
      if (fi.lumpNamespace == WADNS_Sprites && !lumpName.isEmpty()) {
        lumpName = lumpName.Replace("^", "\\");
      }

      //GLog.Logf(NAME_Debug, "ZIP <%s> mapped to <%s> (%d)", *fi.fileNameIntr, *lumpName, (int)fi.lumpNamespace);

      // final lump name
      if (!lumpName.isEmpty()) {
        fi.lumpName = VName(*lumpName, VName::AddLower8);
      }
    }
  }
}


//==========================================================================
//
//  IsDupLumpAllowed
//
//==========================================================================
static bool IsDupLumpAllowed (VName name) {
  return
    name == NAME_things ||
    name == NAME_linedefs ||
    name == NAME_sidedefs ||
    name == NAME_vertexes ||
    name == NAME_segs ||
    name == NAME_ssectors ||
    name == NAME_nodes ||
    name == NAME_sectors ||
    name == NAME_reject ||
    name == NAME_blockmap ||
    name == NAME_textmap ||
    name == NAME_endmap ||
    name == NAME_behavior ||
    name == NAME_dialogue ||
    name == NAME_znodes ||
    name == NAME_gl_level ||
    name == NAME_gl_pvs ||
    //name == NAME_vfxdefs || // nope, let's not tolerate bad practices
    name == NAME_sndinfo ||
    name == NAME_sndseq ||
    name == NAME_reverbs ||
    name == NAME_gldefs ||
    //sigh
    name == NAME_terrain || // nope, let's not tolerate bad practices
    name == NAME_lockdefs || // nope, let's not tolerate bad practices
    name == NAME_language || // nope, let's not tolerate bad practices
    name == NAME_decorate ||
    name == NAME_decaldef ||
    // more crap
    name == NAME_gl_vert ||
    name == NAME_gl_segs ||
    name == NAME_gl_ssect ||
    name == NAME_gl_nodes ||
    name == NAME_script ||
    name == NAME_scripts ||
    name == NAME_dehacked ||
    false;
}


//==========================================================================
//
//  VFileDirectory::buildNameMaps
//
//  call this when all lump names are built
//
//==========================================================================
void VFileDirectory::buildNameMaps (bool rebuilding, VPakFileBase *pak) {
  bool doReports = (rebuilding ? false : !fsys_no_dup_reports);
  if (doReports) {
    const char *specialWadNames[] = {
      "doom",
      "doom1",
      "doomu",
      "doom2",
      "doom2f",
      "tnt",
      "plutonia",
      "nerve",
      "heretic",
      "heretic1",
      "hexen",
      "hexdd",
      "strife0",
      "strife1",
      "chex",
      "chex1",
      "chex2",
      "chex3",
      "hackx",
      "voices",
      nullptr,
    };
    VStr fn = getArchiveName().ExtractFileBaseName().toLowerCase();
    VStr ext = fn.ExtractFileExtension();
    if (ext.strEqu(".wad") || ext.strEqu(".iwad") || ext.strEqu(".pk3") || ext.strEqu(".ipk3")) {
      fn = fn.StripExtension();
      for (const char **swnp = specialWadNames; *swnp; ++swnp) {
        const char *wn = *swnp;
        if (fn.strEquCI(wn)) { doReports = false; break; }
      }
    }
  }
  lumpmap.clear();
  filemap.clear();
  TMap<VStr, bool> dupsReported;
  TMapNC<VName, int> lastSeenLump;

  int seenZScriptLump = -1; // so we can calculate checksum later
  bool warnZScript = true;
  bool zscriptAllowed = false;

  for (int f = 0; f < files.length(); ++f) {
    VPakFileInfo &fi = files[f];
    // link lumps
    VName lmp = fi.lumpName;
    fi.nextLump = -1; // just in case
    fi.prevFile = -1; // just in case

    // check for zscript
    if (seenZScriptLump < 0 && fi.lumpNamespace == WADNS_Global && lmp == NAME_zscript) {
      seenZScriptLump = f;
      // ignore it for now (yeah, inverted condition)
      if (!fsys_IgnoreZScript) {
        fi.lumpName = NAME_None;
        continue;
      }
    }

    // process various hide flags
    const bool doHide =
      (fsys_hide_sprofs && fi.lumpNamespace == WADNS_Global && lmp == NAME_sprofs) ||
      (fsys_skipSounds && fi.lumpNamespace == WADNS_Sounds) ||
      (fsys_skipSprites && fi.lumpNamespace == WADNS_Sprites) ||
      (fsys_skipDehacked && lmp == NAME_dehacked);

    if (doHide) {
      lmp = NAME_None;
      fi.lumpName = NAME_None;
    }

    // register lump
    if (lmp != NAME_None) {
      auto lsidp = lastSeenLump.get(lmp);
      if (!lsidp) {
        // new lump
        lumpmap.put(lmp, f);
        lastSeenLump.put(lmp, f); // for index chain
      } else {
        // we'we seen it before
        vassert(files[*lsidp].nextLump == -1);
        files[*lsidp].nextLump = f; // link to the next one
        *lsidp = f; // update index
        if (doReports && !aszip && !IsDupLumpAllowed(lmp)) {
          if (!dupsReported.put(fi.fileNameIntr, true)) {
            if (fsys_WarningReportsEnabled) {
              GLog.Logf(NAME_Warning, "duplicate lump \"%s\" in archive \"%s\".", *fi.fileNameIntr, *getArchiveName());
              GLog.Logf(NAME_Warning, "THIS IS WRONG. DO NOT USE BROKEN TOOLS TO CREATE %s FILES!", (aszip ? "ARCHIVE" : "WAD"));
            }
          }
        }
      }
    }

    // register file
    if (fi.fileNameIntr.length()) {
      if (doReports && aszip && filemap.has(fi.fileNameIntr)) {
        if (!dupsReported.put(fi.fileNameIntr, true)) {
          if (fsys_WarningReportsEnabled) {
            GLog.Logf(NAME_Warning, "duplicate file \"%s\" in archive \"%s\".", *fi.fileNameIntr, *getArchiveName());
            GLog.Logf(NAME_Warning, "THIS IS WRONG. DO NOT USE BROKEN TOOLS TO CREATE %s FILES!", (aszip ? "ARCHIVE" : "WAD"));
          }
        }
      }

      // put files into hashmap
      auto pfp = filemap.get(fi.fileNameIntr);
      if (pfp) fi.prevFile = *pfp;
      filemap.put(fi.fileNameIntr, f);
    }
    //if (fsys_dev_dump_paks) GLog.Logf(NAME_Debug, "%s: %s", *PakFileName, *Files[f].fileNameIntr);
  }

  int modid = (pak && !fsys_detected_mod ? callModDetectors(this, pak, seenZScriptLump) : 0);
  if (modid) zscriptAllowed = true; // detector will bomb out if it doesn't want that mod
  if (fsys_detected_mod) zscriptAllowed = true;

  // bomb out on zscript
  if (!zscriptAllowed && seenZScriptLump >= 0) {
    if (fsys_IgnoreZScript) {
      if (warnZScript && fsys_WarningReportsEnabled) { warnZScript = false; GLog.Logf(NAME_Error, "Archive \"%s\" contains zscript!", *getArchiveName()); }
    } else {
      Sys_Error("Archive \"%s\" contains zscript!", *getArchiveName());
    }
  }

  if (modid > 0) {
    fsys_detected_mod = modid;
    fsys_detected_mod_wad = getArchiveName();
  }

  if (!rebuilding && fsys_dev_dump_paks) {
    GLog.Logf("======== PAK: %s ========", *getArchiveName());
    for (int f = 0; f < files.length(); ++f) {
      VPakFileInfo &fi = files[f];
      GLog.Logf("  %d: file=<%s>; lump=<%s>; ns=%d; size=%d; ofs=%d", f, *fi.fileNameIntr, *fi.lumpName, fi.lumpNamespace, fi.filesize, fi.pakdataofs);
    }
  }
}


//==========================================================================
//
//  VFileDirectory::normalizeFileName
//
//==========================================================================
void VFileDirectory::normalizeFileName (VStr &fname) {
  if (fname.length() == 0) return;
  for (;;) {
         if (fname.startsWith("./")) fname.chopLeft(2);
    else if (fname.startsWith("../")) fname.chopLeft(3);
    else if (fname.startsWith("/")) fname.chopLeft(1);
    else break;
  }
  if (!fname.isLowerCase()) fname = fname.toLowerCase();
  return;
}


//==========================================================================
//
//  VFileDirectory::normalizeLumpName
//
//==========================================================================
VName VFileDirectory::normalizeLumpName (VName lname) {
  if (lname == NAME_None) return NAME_None;
  if (!VStr::isLowerCase(*lname)) lname = VName(*lname, VName::AddLower);
  return lname;
}


//==========================================================================
//
//  VFileDirectory::fileExists
//
//  if `lump` is not `nullptr`, sets it to file lump or to -1
//
//==========================================================================
bool VFileDirectory::fileExists (VStr fname, int *lump) {
  normalizeFileName(fname);
  if (lump) {
    if (fname.length() != 0) {
      auto pp = filemap.get(fname);
      if (pp) {
        *lump = *pp;
        return true;
      }
    }
    *lump = -1;
    return false;
  } else {
    if (fname.length() == 0) return false;
    return filemap.has(fname);
  }
}


//==========================================================================
//
//  VFileDirectory::lumpExists
//
//  namespace -1 means "any"
//
//==========================================================================
bool VFileDirectory::lumpExists (VName lname, vint32 ns) {
  lname = normalizeLumpName(lname);
  if (lname == NAME_None) return false;
  auto fp = lumpmap.get(lname);
  if (!fp) return false;
  if (ns < 0) return true;
  for (int f = *fp; f >= 0; f = files[f].nextLump) {
    if ((ns == WADNS_Any || ns == WADNS_AllFiles) || files[f].lumpNamespace == ns) return true;
  }
  return false;
}


//==========================================================================
//
//  VFileDirectory::findFile
//
//==========================================================================
int VFileDirectory::findFile (VStr fname) {
  normalizeFileName(fname);
  if (fname.length() == 0) return false;
  auto fp = filemap.get(fname);
  return (fp ? *fp : -1);
}


//==========================================================================
//
//  VFileDirectory::findFirstLump
//
//  namespace -1 means "any"
//
//==========================================================================
int VFileDirectory::findFirstLump (VName lname, vint32 ns) {
  lname = normalizeLumpName(lname);
  if (lname == NAME_None) return -1;
  auto fp = lumpmap.get(lname);
  if (!fp) return -1;
  if (ns < 0) return *fp;
  for (int f = *fp; f >= 0; f = files[f].nextLump) {
    if ((ns == WADNS_Any || ns == WADNS_AllFiles) || files[f].lumpNamespace == ns) return f;
  }
  return -1;
}


//==========================================================================
//
//  VFileDirectory::findLastLump
//
//==========================================================================
int VFileDirectory::findLastLump (VName lname, vint32 ns) {
  lname = normalizeLumpName(lname);
  if (lname == NAME_None) return -1;
  auto fp = lumpmap.get(lname);
  if (!fp) return -1;
  int res = -1;
  for (int f = *fp; f >= 0; f = files[f].nextLump) {
    if ((ns == WADNS_Any || ns == WADNS_AllFiles) || files[f].lumpNamespace == ns) res = f;
  }
  return res;
}


//==========================================================================
//
//  VFileDirectory::nextLump
//
//==========================================================================
int VFileDirectory::nextLump (vint32 curridx, vint32 ns, bool allowEmptyName8) {
  if (curridx < 0) curridx = 0;
  for (; curridx < files.length(); ++curridx) {
    if (ns != WADNS_AllFiles && !allowEmptyName8 && files[curridx].lumpName == NAME_None) continue;
    if ((ns == WADNS_Any || ns == WADNS_AllFiles) || files[curridx].lumpNamespace == ns) return curridx;
  }
  return -1;
}



//==========================================================================
//
//  VPakFileBase::VPakFileBase
//
//==========================================================================
VPakFileBase::VPakFileBase (VStr apakfilename, bool aaszip)
  : VSearchPath()
  , PakFileName(apakfilename)
  , pakdir(this, aaszip)
  , archStream(nullptr)
  , rdlockInited(false)
{
  initLock();
}


//==========================================================================
//
//  VPakFileBase::~VPakFileBase
//
//==========================================================================
VPakFileBase::~VPakFileBase () {
  Close();
  // just in case
  if (archStream) VStream::Destroy(archStream);
  deinitLock();
}


//==========================================================================
//
//  VPakFileBase::initLock
//
//==========================================================================
void VPakFileBase::initLock () {
  if (!rdlockInited) {
    mythread_mutex_init(&rdlock);
    rdlockInited = true;
  }
}


//==========================================================================
//
//  VPakFileBase::deinitLock
//
//==========================================================================
void VPakFileBase::deinitLock () {
  if (rdlockInited) {
    rdlockInited = false;
    mythread_mutex_destroy(&rdlock);
  }
}


//==========================================================================
//
//  VPakFileBase::GetPrefix
//
//==========================================================================
VStr VPakFileBase::GetPrefix () {
  return PakFileName;
}


//==========================================================================
//
//  VPakFileBase::UpdateLumpLength
//
//==========================================================================
void VPakFileBase::UpdateLumpLength (int Lump, int len) {
  if (len > 0) pakdir.files[Lump].filesize = len;
}


//==========================================================================
//
//  VPakFileBase::RenameSprites
//
//==========================================================================
void VPakFileBase::RenameSprites (const TArray<VSpriteRename> &A, const TArray<VLumpRename> &LA) {
  bool wasRename = false;
  for (auto &&L : pakdir.files) {
    //VPakFileInfo &L = pakdir.files[i];
    if (L.lumpNamespace != WADNS_Sprites) continue;
    for (auto &&ren : A) {
      if ((*L.lumpName)[0] != ren.Old[0] ||
          (*L.lumpName)[1] != ren.Old[1] ||
          (*L.lumpName)[2] != ren.Old[2] ||
          (*L.lumpName)[3] != ren.Old[3])
      {
        continue;
      }
      char newname[16];
      auto len = (int)strlen(*L.lumpName);
      if (len) {
        if (len > 12) len = 12;
        memcpy(newname, *L.lumpName, len);
      }
      newname[len] = 0;
      newname[0] = ren.New[0];
      newname[1] = ren.New[1];
      newname[2] = ren.New[2];
      newname[3] = ren.New[3];
      GLog.Logf(NAME_Dev, "%s: renaming sprite '%s' to '%s'", *PakFileName, *L.lumpName, newname);
      L.lumpName = newname;
      wasRename = true;
    }
    for (auto &&lra : LA) {
      if (L.lumpName == lra.Old) {
        L.lumpName = lra.New;
        wasRename = true;
      }
    }
  }
  if (wasRename) pakdir.buildNameMaps(true);
}


//==========================================================================
//
//  VPakFileBase::FileExists
//
//  if `lump` is not `nullptr`, sets it to file lump or to -1
//
//==========================================================================
bool VPakFileBase::FileExists (VStr fname, int *lump) {
  return pakdir.fileExists(fname, lump);
}


//==========================================================================
//
//  VPakFileBase::Close
//
//==========================================================================
void VPakFileBase::Close () {
  pakdir.clear();
  if (archStream) VStream::Destroy(archStream);
  deinitLock();
}


//==========================================================================
//
//  VPakFileBase::CheckNumForName
//
//==========================================================================
int VPakFileBase::CheckNumForName (VName lumpName, EWadNamespace NS, bool wantFirst) {
  int res = (wantFirst ? pakdir.findFirstLump(lumpName, NS) : pakdir.findLastLump(lumpName, NS));
  //GLog.Logf(NAME_Debug, "CheckNumForName:<%s>: ns=%d; first=%d; res=%d; name=<%s> (%s)", *PakFileName, NS, (int)wantFirst, res, *pakdir.normalizeLumpName(lumpName), *lumpName);
  return res;
}


//==========================================================================
//
//  VPakFileBase::CheckNumForFileName
//
//==========================================================================
int VPakFileBase::CheckNumForFileName (VStr aFileName) {
  return pakdir.findFile(aFileName);
}


//==========================================================================
//
//  VPakFileBase::FindACSObject
//
//==========================================================================
int VPakFileBase::FindACSObject (VStr fname) {
  VStr afn = fname.ExtractFileBaseName();
  if (afn.ExtractFileExtension().strEquCI(".o")) afn = afn.StripExtension();
  if (afn.length() == 0) return -1;
  VName ln = VName(*afn, VName::AddLower8);
  if (fsys_developer_debug) GLog.Logf(NAME_Dev, "*** ACS: looking for '%s' (normalized: '%s'; shorten: '%s')", *fname, *afn, *ln);
  int rough = -1;
  int prevlen = -1;
  const int count = pakdir.files.length();
  char sign[4];
  for (int f = 0; f < count; ++f) {
    const VPakFileInfo &fi = pakdir.files[f];
    if (fi.lumpNamespace != WADNS_ACSLibrary) continue;
    if (fi.lumpName != ln) continue;
    if (fi.filesize >= 0 && fi.filesize < 12) continue; // why not? (it is <0 for disk mounts, so check them anyway)
    if (fsys_developer_debug) GLog.Logf(NAME_Dev, "*** ACS0: fi='%s', lump='%s', ns=%d (%d)", *fi.fileNameIntr, *fi.lumpName, fi.lumpNamespace, WADNS_ACSLibrary);
    const VStr fn(fi.fileNameIntr.ExtractFileBaseName().StripExtension());
    if (fn.ICmp(afn) == 0) {
      // exact match
      if (fsys_developer_debug) GLog.Logf(NAME_Dev, "*** ACS0: %s", *fi.fileNameIntr);
      memset(sign, 0, 4);
      ReadFromLump(f, sign, 0, 4);
      if (memcmp(sign, "ACS", 3) != 0) continue;
      if (sign[3] != 0 && sign[3] != 'E' && sign[3] != 'e') continue;
      if (fsys_developer_debug) GLog.Logf(NAME_Dev, "*** ACS0: %s -- HIT!", *fi.fileNameIntr);
      return f;
    }
    if (rough < 0 || (fn.length() >= afn.length() && prevlen > fn.length())) {
      if (fsys_developer_debug) GLog.Logf(NAME_Dev, "*** ACS1: %s", *fi.fileNameIntr);
      memset(sign, 0, 4);
      ReadFromLump(f, sign, 0, 4);
      if (memcmp(sign, "ACS", 3) != 0) continue;
      if (sign[3] != 0 && sign[3] != 'E' && sign[3] != 'e') continue;
      if (fsys_developer_debug) GLog.Logf(NAME_Dev, "*** ACS1: %s -- HIT!", *fi.fileNameIntr);
      rough = f;
      prevlen = fn.length();
    }
  }
  return rough;
}


//==========================================================================
//
//  VPakFileBase::ReadFromLump
//
//==========================================================================
void VPakFileBase::ReadFromLump (int Lump, void *Dest, int Pos, int Size) {
  vassert(Lump >= 0);
  vassert(Lump < pakdir.files.length());
  vassert(Size >= 0);
  if (Size == 0) return;
  VStream *Strm = CreateLumpReaderNum(Lump);
  if (!Strm) Sys_Error("error reading lump '%s:%s'", *GetPrefix(), *pakdir.files[Lump].fileNameIntr);
  // special case: unpacked size is unknown, cache it
  int fsize = pakdir.files[Lump].filesize;
  if (fsize == -1) {
    fsize = Strm->TotalSize();
    if (Strm->IsError()) Sys_Error("error getting lump '%s:%s' size", *GetPrefix(), *pakdir.files[Lump].fileNameIntr);
    UpdateLumpLength(Lump, fsize);
  }
  if (fsize < 0 || Pos >= fsize || fsize-Pos < Size) {
    VStream::Destroy(Strm);
    Sys_Error("error reading lump '%s:%s' (out of data)", *GetPrefix(), *pakdir.files[Lump].fileNameIntr);
  }
  Strm->Seek(Pos);
  Strm->Serialise(Dest, Size);
  bool wasError = Strm->IsError();
  VStream::Destroy(Strm);
  if (wasError) Sys_Error("error reading lump '%s:%s'", *GetPrefix(), *pakdir.files[Lump].fileNameIntr);
}


//==========================================================================
//
//  VPakFileBase::LumpLength
//
//==========================================================================
int VPakFileBase::LumpLength (int Lump) {
  if (Lump < 0 || Lump >= pakdir.files.length()) return 0;
  // special case: unpacked size is unknown, read and cache it
  if (pakdir.files[Lump].filesize == -1) {
    VStream *Strm = CreateLumpReaderNum(Lump);
    if (!Strm) Sys_Error("cannot get length for lump '%s:%s'", *GetPrefix(), *pakdir.files[Lump].fileNameIntr);
    int fsize = Strm->TotalSize();
    bool wasError = Strm->IsError();
    VStream::Destroy(Strm);
    if (wasError || /*pakdir.files[Lump].filesize*/fsize < 0) Sys_Error("cannot get length for lump '%s:%s'", *GetPrefix(), *pakdir.files[Lump].fileNameIntr);
    UpdateLumpLength(Lump, fsize);
    return fsize;
  }
  return pakdir.files[Lump].filesize;
}


//==========================================================================
//
//  VPakFileBase::LumpName
//
//==========================================================================
VName VPakFileBase::LumpName (int Lump) {
  if (Lump < 0 || Lump >= pakdir.files.length()) return NAME_None;
  return pakdir.files[Lump].lumpName;
}


//==========================================================================
//
//  VPakFileBase::LumpFileName
//
//==========================================================================
VStr VPakFileBase::LumpFileName (int Lump) {
  if (Lump < 0 || Lump >= pakdir.files.length()) return VStr();
  return pakdir.files[Lump].fileNameIntr;
}


//==========================================================================
//
//  VPakFileBase::IterateNS
//
//==========================================================================
int VPakFileBase::IterateNS (int Start, EWadNamespace NS, bool allowEmptyName8) {
  return pakdir.nextLump(Start, NS, allowEmptyName8);
}


//==========================================================================
//
//  VPakFileBase::ListWadFiles
//
//==========================================================================
void VPakFileBase::ListWadFiles (TArray<VStr> &list) {
  TMap<VStr, bool> hits;
  for (int i = 0; i < pakdir.files.length(); ++i) {
    const VPakFileInfo &fi = pakdir.files[i];
    if (fi.fileNameIntr.length() < 5) continue;
    // only .wad files
    if (!fi.fileNameIntr.EndsWith(".wad")) continue;
    // don't add WAD files in subdirectories
    if (fi.fileNameIntr.IndexOf('/') != -1) continue;
    if (hits.has(fi.fileNameIntr)) continue;
    hits.put(fi.fileNameIntr, true);
    list.Append(fi.fileNameIntr);
  }
}


//==========================================================================
//
//  VPakFileBase::ListPk3Files
//
//==========================================================================
void VPakFileBase::ListPk3Files (TArray<VStr> &list) {
  TMap<VStr, bool> hits;
  for (int i = 0; i < pakdir.files.length(); ++i) {
    const VPakFileInfo &fi = pakdir.files[i];
    if (fi.fileNameIntr.length() < 5) continue;
    // only .pk3 files
    if (!fi.fileNameIntr.EndsWith(".pk3")) continue;
    // don't add pk3 files in subdirectories
    if (fi.fileNameIntr.IndexOf('/') != -1) continue;
    if (hits.has(fi.fileNameIntr)) continue;
    hits.put(fi.fileNameIntr, true);
    list.Append(fi.fileNameIntr);
  }
}


//==========================================================================
//
//  VPakFileBase::CalculateMD5
//
//==========================================================================
VStr VPakFileBase::CalculateMD5 (int lumpidx) {
  if (lumpidx < 0 || lumpidx >= pakdir.files.length()) return VStr::EmptyString;
  VStream *strm = CreateLumpReaderNum(lumpidx);
  if (!strm) return VStr::EmptyString;
  MD5Context md5ctx;
  md5ctx.Init();
  enum { BufSize = 65536 };
  vuint8 *buf = new vuint8[BufSize];
  auto left = strm->TotalSize();
  while (left > 0) {
    int rd = (left > BufSize ? BufSize : left);
    strm->Serialise(buf, rd);
    if (strm->IsError()) {
      delete[] buf;
      VStream::Destroy(strm);
      return VStr::EmptyString;
    }
    left -= rd;
    md5ctx.Update(buf, (unsigned)rd);
  }
  delete[] buf;
  VStream::Destroy(strm);
  vuint8 md5digest[MD5Context::DIGEST_SIZE];
  md5ctx.Final(md5digest);
  return VStr::buf2hex(md5digest, MD5Context::DIGEST_SIZE);
}


//==========================================================================
//
//  VPakFileBase::OpenFileRead
//
//  if `lump` is not `nullptr`, sets it to file lump or to -1
//
//==========================================================================
VStream *VPakFileBase::OpenFileRead (VStr fname, int *lump) {
  int fidx = pakdir.findFile(fname);
  if (lump) *lump = fidx;
  if (fidx < 0) return nullptr;
  return CreateLumpReaderNum(fidx);
}
