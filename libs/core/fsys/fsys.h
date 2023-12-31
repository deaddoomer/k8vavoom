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
#ifndef VAVOOM_FSYS_H
#define VAVOOM_FSYS_H

#ifndef VAVOOM_CORE_HEADER
# include "../core.h"
#endif

// k8: about .gwa supplemental wads.
// those are remnants of the early times, and not really used these days.
// you still can put PVS there, but PVS seems to be totally unused
// these days too; so i opted to exclude GWA support.
// i had a define to turn GWA support on, but now i removed it completely.

typedef unsigned char FSysPublicKey[32];


// empty/NULL `signer` means "unknown signer"
void FSYS_RegisterPublicKey (const char *signer, const FSysPublicKey key);

// returns NULL for unknown key
const char *FSYS_FindPublicKey (const FSysPublicKey key);

bool FSYS_DecodePublicKey (const char *asciikeystr, FSysPublicKey key);

void FSYS_Shutdown ();

void FSYS_InitOptions (VParsedArgs &pargs);

bool FL_FileExists (VStr fname, int *lump=nullptr);
void FL_CreatePath (VStr Path);

// if `lump` is not `nullptr`, sets it to file lump or to -1
VStream *FL_OpenFileRead (VStr Name, int *lump=nullptr);

// open "system" (i.e. disk-only) files
VStream *FL_OpenSysFileRead (VStr Name);
VStream *FL_OpenSysFileWrite (VStr Name);

// search file only in "basepaks"
// if `lump` is not `nullptr`, sets it to file lump or to -1
VStream *FL_OpenFileReadBaseOnly (VStr Name, int *lump=nullptr);

// reject absolute names, names with ".", and names with ".."
bool FL_IsSafeDiskFileName (VStr fname);


extern EName fsys_report_added_paks_logtype;

// used in various utilities; DO NOT TOUCH!
enum {
  FSYS_ARCHIVES_NORMAL = 0,
  FSYS_ARCHIVES_SIMPLE = 1,
  FSYS_ARCHIVES_SIMPLE_KEEP_PREFIX = 2,
};
extern int fsys_simple_archives;

extern bool fsys_developer_debug;
extern int fsys_IgnoreZScript;
extern bool fsys_DisableBDW;
extern bool fsys_WarningReportsEnabled; // default is `true`

// used for game detection
extern bool fsys_EnableAuxSearch;

extern bool fsys_skipSounds;
extern bool fsys_skipSprites;
extern bool fsys_skipDehacked;

extern bool fsys_report_added_paks;
extern bool fsys_no_dup_reports;
extern int fsys_dev_dump_paks;


// this is internal now, use the api to set it
//extern TArray<VStr> fsys_game_filters;

void FL_ClearGameFilters ();

// add new filter; it should start with "filter/"
// duplicate filters will be ignored
// returns 0 for "no error"
enum {
  FL_ADDFILTER_OK = 0,
  FL_ADDFILTER_INVALID = -1,
  FL_ADDFILTER_DUPLICATE = -2,
};
int FL_AddGameFilter (VStr path);


// all appended wads will be marked as "user wads" from this point on
void FL_StartUserWads ();
// stop marking user wads
void FL_EndUserWads ();


// boom namespaces
enum EWadNamespace {
  WADNS_Global,
  WADNS_Sprites,
  WADNS_Flats,
  WADNS_ColorMaps,
  WADNS_ACSLibrary,
  WADNS_NewTextures,
  WADNS_Voices,
  WADNS_HiResTextures,
  // jdoom/doomsday hires textures
  WADNS_HiResTexturesDDay,
  WADNS_HiResFlatsDDay,
  // special pk3 directory for base paks
  WADNS_AfterIWad,

  // special namespaces for zip files, in wad file they will be searched in global namespace
  WADNS_ZipSpecial,
  WADNS_Patches,
  WADNS_Graphics,
  WADNS_Sounds,
  WADNS_Music,

  // all
  WADNS_Any, // only 8-char names, no empty lump names allowed
  WADNS_AllFiles, // even empty lump names are allowed
};


// for non-wad files (zip, pk3, pak), it adds add subarchives from a root directory
void W_AddDiskFile (VStr FileName, bool FixVoices=false);
// returns `true` if file was added
bool W_AddDiskFileOptional (VStr FileName, bool FixVoices=false);
// this mounts disk directory as PK3 archive
void W_MountDiskDir (VStr dirname);
// this removes all added files
void W_Shutdown ();


enum WAuxFileType {
  VFS_Wad, // caller is 100% sure that this is IWAD/PWAD
  VFS_Zip, // guess it, and allow nested files
  VFS_Archive, // guess it, but don't allow nested files
};


struct FSysAuxMark;

// returns lump handle
int W_StartAuxiliary (); // returns first aux index
int W_OpenAuxiliary (VStr FileName); // -1: not found
//int W_AddAuxiliary (VStr FileName); // -1: not found
// -1: error/not found; otherwise handle of the first appended file
// on success it will own the stream, otherwise won't
int W_AddAuxiliaryStream (VStream *strm, WAuxFileType ftype);
void W_CloseAuxiliary (); // close all aux files
int W_GetFirstAuxFile (); // returns -1 if no aux archives were opened, or fileid
int W_GetFirstAuxLump (); // returns -1 if no aux archives were opened, or lump id

FSysAuxMark *W_MarkAuxiliary (); // can return `nullptr` on error; frees all opened auxes after calling this
//WARNING! make sure you don't have open files from auxes to be released!
void W_ReleaseAuxiliary (FSysAuxMark *&mark); // will free the mark, it cannot be used after this

bool W_IsInAuxiliaryMode ();
int W_AuxiliaryOpenedArchives ();

// all checks/finders returns -1 if nothing was found.
// well, except "get" kinds -- they're bombing out.
int W_CheckNumForName (VName Name, EWadNamespace NS=WADNS_Global);
int W_GetNumForName (VName Name, EWadNamespace NS=WADNS_Global);
int W_CheckNumForNameInFile (VName Name, int File, EWadNamespace NS=WADNS_Global);
int W_CheckNumForNameInFileOrLower (VName Name, int File, EWadNamespace NS=WADNS_Global);
int W_CheckFirstNumForNameInFile (VName Name, int File, EWadNamespace NS=WADNS_Global);
int W_CheckLastNumForNameInFile (VName Name, int File, EWadNamespace NS=WADNS_Global);
int W_FindACSObjectInFile (VStr Name, int File);

int W_CheckNumForFileName (VStr Name);
int W_CheckNumForFileNameInSameFile (int filelump, VStr Name);
int W_CheckNumForFileNameInSameFileOrLower (int filelump, VStr Name);
int W_CheckNumForTextureFileName (VStr Name);
int W_GetNumForFileName (VStr Name);
int W_FindLumpByFileNameWithExts (VStr BaseName, const char **Exts);

int W_LumpLength (int lump);
VName W_LumpName (int lump);
VStr W_FullLumpName (int lump);
VStr W_RealLumpName (int lump); // without pak prefix
VStr W_FullPakNameForLump (int lump);
VStr W_FullPakNameByFile (int fidx); // pass result of `W_LumpFile()`
int W_LumpFile (int lump);

// this is used to resolve animated ranges
// returns handle or -1
int W_FindFirstLumpOccurence (VName lmpname, EWadNamespace NS);

bool W_IsWadPK3File (int file);
bool W_IsPakFile (int file); // not a container, not a wad (usually pk3)

bool W_IsIWADFile (int file);
bool W_IsWADFile (int file); // not pk3, not disk
bool W_IsAuxFile (int file); // -1 is not aux ;-)

bool W_IsIWADLump (int lump);
bool W_IsWADLump (int lump); // not pk3, not disk
bool W_IsAuxLump (int lump); // -1 is not aux ;-)

bool W_IsUserWadFile (int file);
bool W_IsUserWadLump (int lump);

// checks if the "seek" operation on the given lump is fast
bool W_IsFastSeekLump (int lump);

void W_ReadFromLump (int lump, void *dest, int pos, int size);
VStr W_LoadTextLump (VName name);
void W_LoadLumpIntoArray (VName Lump, TArrayNC<vuint8>& Array);
void W_LoadLumpIntoArrayIdx (int Lump, TArrayNC<vuint8>& Array);
VStream *W_CreateLumpReaderNum (int lump);
VStream *W_CreateLumpReaderName (VName Name, EWadNamespace NS = WADNS_Global);

int W_StartIterationFromLumpFileNS (int File, EWadNamespace NS); // returns -1 if not found
int W_IterateNS (int Prev, EWadNamespace NS);
int W_IterateFile (int Prev, VStr Name);
int W_IterateDirectory (int Prev, VStr DirName, bool allowSubdirs=true);

// basically, returns number of mounted archives
int W_NextMountFileId ();

bool W_IsValidMapHeaderLump (int lump);


// ////////////////////////////////////////////////////////////////////////// //
struct WadMapIterator {
public:
  int lump; // current lump

private:
  // this doesn't advance `-1`, and does nothing if `lump` is a map header lump
  void advanceToNextMapLump ();

public:
  inline WadMapIterator () noexcept : lump(-1) {}
  inline WadMapIterator (const WadMapIterator &it) noexcept : lump(it.lump) {}
  inline WadMapIterator (const WadMapIterator &/*it*/, bool /*asEnd*/) noexcept : lump(-1) {}
  inline void operator = (const WadMapIterator &it) noexcept { lump = it.lump; }

  static inline WadMapIterator FromWadFile (int aFile) noexcept {
    WadMapIterator it;
    it.lump = W_StartIterationFromLumpFileNS(aFile, WADNS_Global);
    it.advanceToNextMapLump();
    return it;
  }

  static inline WadMapIterator FromFirstAuxFile () noexcept {
    WadMapIterator it;
    it.lump = W_GetFirstAuxLump();
    it.advanceToNextMapLump();
    return it;
  }

  inline WadMapIterator begin () noexcept { return WadMapIterator(*this); }
  inline WadMapIterator end () noexcept { return WadMapIterator(*this, true); }
  inline bool operator == (const WadMapIterator &b) const noexcept { return (lump == b.lump); }
  inline bool operator != (const WadMapIterator &b) const noexcept { return (lump != b.lump); }
  inline WadMapIterator operator * () const noexcept { return WadMapIterator(*this); } /* required for iterator */
  inline void operator ++ () noexcept { if (lump >= 0) { ++lump; advanceToNextMapLump(); } } /* this is enough for iterator */

  inline bool isEmpty () const noexcept { return (lump < 0); }

  inline int getFile () const noexcept { return (lump >= 0 ? W_LumpFile(lump) : -1); }
  inline int getLength () const noexcept { return (lump >= 0 ? W_LumpLength(lump) : -1); }
  inline int getSize () const noexcept { return (lump >= 0 ? W_LumpLength(lump) : -1); }
  inline VName getName () const noexcept { return (lump >= 0 ? W_LumpName(lump) : NAME_None); }
  inline VStr getFullName () const noexcept { return (lump >= 0 ? W_FullLumpName(lump) : VStr::EmptyString); }
  inline VStr getRealName () const noexcept { return (lump >= 0 ? W_RealLumpName(lump) : VStr::EmptyString); } // without pak prefix
  inline VStr getFullPakName () const noexcept { return (lump >= 0 ? W_FullPakNameForLump(lump) : VStr::EmptyString); } // without pak prefix

  inline bool isAux () const noexcept { return (lump >= 0 ? W_IsAuxLump(lump) : false); }
  inline bool isIWAD () const noexcept { return (lump >= 0 ? W_IsIWADLump(lump) : false); }
  inline bool isIWad () const noexcept { return (lump >= 0 ? W_IsIWADLump(lump) : false); }
};


// ////////////////////////////////////////////////////////////////////////// //
struct WadNSIterator {
public:
  int lump; // current lump
  EWadNamespace ns;

public:
  inline WadNSIterator () noexcept : lump(-1), ns(WADNS_Global) {}
  inline WadNSIterator (EWadNamespace aNS) noexcept : lump(-1), ns(aNS) { lump = W_IterateNS(-1, aNS); }
  inline WadNSIterator (const WadNSIterator &it) noexcept : lump(it.lump), ns(it.ns) {}
  inline WadNSIterator (const WadNSIterator &it, bool /*asEnd*/) noexcept : lump(-1), ns(it.ns) {}
  inline void operator = (const WadNSIterator &it) noexcept { lump = it.lump; ns = it.ns; }

  static inline WadNSIterator FromWadFile (int aFile, EWadNamespace aNS) noexcept {
    WadNSIterator it;
    it.ns = aNS;
    it.lump = W_StartIterationFromLumpFileNS(aFile, aNS);
    return it;
  }

  inline WadNSIterator begin () noexcept { return WadNSIterator(*this); }
  inline WadNSIterator end () noexcept { return WadNSIterator(*this, true); }
  inline bool operator == (const WadNSIterator &b) const noexcept { return (lump == b.lump && ns == b.ns); }
  inline bool operator != (const WadNSIterator &b) const noexcept { return (lump != b.lump || ns != b.ns); }
  inline WadNSIterator operator * () const noexcept { return WadNSIterator(*this); } /* required for iterator */
  inline void operator ++ () noexcept { lump = (lump >= 0 ? W_IterateNS(lump, ns) : -1); } /* this is enough for iterator */

  inline bool isEmpty () const noexcept { return (lump < 0); }

  inline int getFile () const noexcept { return (lump >= 0 ? W_LumpFile(lump) : -1); }
  inline int getLength () const noexcept { return (lump >= 0 ? W_LumpLength(lump) : -1); }
  inline int getSize () const noexcept { return (lump >= 0 ? W_LumpLength(lump) : -1); }
  inline VName getName () const noexcept { return (lump >= 0 ? W_LumpName(lump) : NAME_None); }
  inline VStr getFullName () const noexcept { return (lump >= 0 ? W_FullLumpName(lump) : VStr::EmptyString); }
  inline VStr getRealName () const noexcept { return (lump >= 0 ? W_RealLumpName(lump) : VStr::EmptyString); } // without pak prefix
  inline VStr getFullPakName () const noexcept { return (lump >= 0 ? W_FullPakNameForLump(lump) : VStr::EmptyString); } // without pak prefix

  inline bool isAux () const noexcept { return (lump >= 0 ? W_IsAuxLump(lump) : false); }
  inline bool isIWAD () const noexcept { return (lump >= 0 ? W_IsIWADLump(lump) : false); }
  inline bool isIWad () const noexcept { return (lump >= 0 ? W_IsIWADLump(lump) : false); }
};


// ////////////////////////////////////////////////////////////////////////// //
struct WadNSNameIterator {
public:
  int lump; // current lump
  EWadNamespace ns;
  VName lumpname;

private:
  // all fields are set
  inline void startIteration () noexcept {
    if (lumpname != NAME_None) {
      if (lump < 0) lump = W_IterateNS(-1, ns);
      for (; lump >= 0; lump = W_IterateNS(lump, ns)) {
        if (W_LumpName(lump) == lumpname) break;
      }
    } else {
      lump = -1;
    }
  }

  inline void nextStep () noexcept {
    if (lump >= 0) {
      for (lump = W_IterateNS(lump, ns); lump >= 0; lump = W_IterateNS(lump, ns)) {
        if (W_LumpName(lump) == lumpname) break;
      }
    }
  }

public:
  inline WadNSNameIterator () noexcept : lump(-1), ns(WADNS_Global), lumpname(NAME_None) {}
  inline WadNSNameIterator (VName aname, EWadNamespace aNS) noexcept : lump(-1), ns(aNS), lumpname(aname) { if (aname != NAME_None) lumpname = VName(*aname, VName::FindLower); startIteration(); }
  inline WadNSNameIterator (const char *aname, EWadNamespace aNS) noexcept : lump(-1), ns(aNS), lumpname(NAME_None) { lumpname = VName(aname, VName::FindLower); startIteration(); }
  inline WadNSNameIterator (VStr &str, EWadNamespace aNS) noexcept : lump(-1), ns(aNS), lumpname(NAME_None) { lumpname = VName(*str, VName::FindLower); startIteration(); }
  inline WadNSNameIterator (const WadNSNameIterator &it) noexcept : lump(it.lump), ns(it.ns), lumpname(it.lumpname) {}
  inline WadNSNameIterator (const WadNSNameIterator &it, bool /*asEnd*/) noexcept : lump(-1), ns(it.ns), lumpname(it.lumpname) {}
  inline void operator = (const WadNSNameIterator &it) noexcept { lump = it.lump; ns = it.ns; lumpname = it.lumpname; }

  static inline WadNSNameIterator FromWadFile (const char *aname, int aFile, EWadNamespace aNS) noexcept {
    WadNSNameIterator it;
    it.ns = aNS;
    it.lump = W_StartIterationFromLumpFileNS(aFile, aNS);
    it.lumpname = (aname && aname[0] ? VName(aname, VName::FindLower) : NAME_None);
    it.startIteration();
    return it;
  }

  static inline WadNSNameIterator FromWadFile (VName aname, int aFile, EWadNamespace aNS) noexcept {
    return FromWadFile((aname != NAME_None ? *aname : nullptr), aFile, aNS);
  }

  static inline WadNSNameIterator FromWadFile (VStr aname, int aFile, EWadNamespace aNS) noexcept {
    return FromWadFile(*aname, aFile, aNS);
  }

  inline WadNSNameIterator begin () noexcept { return WadNSNameIterator(*this); }
  inline WadNSNameIterator end () noexcept { return WadNSNameIterator(*this, true); }
  inline bool operator == (const WadNSNameIterator &b) const noexcept { return (lump == b.lump && ns == b.ns && lumpname == b.lumpname); }
  inline bool operator != (const WadNSNameIterator &b) const noexcept { return (lump != b.lump || ns != b.ns || lumpname != b.lumpname); }
  inline WadNSNameIterator operator * () const noexcept { return WadNSNameIterator(*this); } /* required for iterator */
  inline void operator ++ () noexcept { nextStep(); } /* this is enough for iterator */

  inline bool isEmpty () const noexcept { return (lump < 0); }

  inline int getFile () const noexcept { return (lump >= 0 ? W_LumpFile(lump) : -1); }
  inline int getLength () const noexcept { return (lump >= 0 ? W_LumpLength(lump) : -1); }
  inline int getSize () const noexcept { return (lump >= 0 ? W_LumpLength(lump) : -1); }
  inline VName getName () const noexcept { return (lump >= 0 ? W_LumpName(lump) : NAME_None); }
  inline VStr getFullName () const noexcept { return (lump >= 0 ? W_FullLumpName(lump) : VStr::EmptyString); }
  inline VStr getRealName () const noexcept { return (lump >= 0 ? W_RealLumpName(lump) : VStr::EmptyString); } // without pak prefix
  inline VStr getFullPakName () const noexcept { return (lump >= 0 ? W_FullPakNameForLump(lump) : VStr::EmptyString); } // without pak prefix

  inline bool isAux () const noexcept { return (lump >= 0 ? W_IsAuxLump(lump) : false); }
  inline bool isIWAD () const noexcept { return (lump >= 0 ? W_IsIWADLump(lump) : false); }
  inline bool isIWad () const noexcept { return (lump >= 0 ? W_IsIWADLump(lump) : false); }
};


// ////////////////////////////////////////////////////////////////////////// //
struct WadFileIterator {
public:
  int lump; // current lump
  VStr fname;

public:
  inline WadFileIterator () noexcept : lump(-1), fname() {}
  inline WadFileIterator (VStr afname) noexcept : lump(-1), fname(afname) { lump = W_IterateFile(-1, afname); }
  inline WadFileIterator (const WadFileIterator &it) noexcept : lump(it.lump), fname(it.fname) {}
  inline WadFileIterator (const WadFileIterator &it, bool /*asEnd*/) noexcept : lump(-1), fname(it.fname) {}
  inline void operator = (const WadFileIterator &it) noexcept { lump = it.lump; fname = it.fname; }

  static inline WadFileIterator FromFirstAuxFile () noexcept {
    WadFileIterator it;
    it.lump = W_GetFirstAuxLump();
    return it;
  }

  inline WadFileIterator begin () noexcept { return WadFileIterator(*this); }
  inline WadFileIterator end () noexcept { return WadFileIterator(*this, true); }
  inline bool operator == (const WadFileIterator &b) const noexcept { return (lump == b.lump); }
  inline bool operator != (const WadFileIterator &b) const noexcept { return (lump != b.lump); }
  inline WadFileIterator operator * () const noexcept { return WadFileIterator(*this); } /* required for iterator */
  inline void operator ++ () noexcept { lump = (lump >= 0 ? W_IterateFile(lump, fname) : -1); } /* this is enough for iterator */

  inline bool isEmpty () const noexcept { return (lump < 0); }

  inline int getFile () const noexcept { return (lump >= 0 ? W_LumpFile(lump) : -1); }
  inline int getLength () const noexcept { return (lump >= 0 ? W_LumpLength(lump) : -1); }
  inline int getSize () const noexcept { return (lump >= 0 ? W_LumpLength(lump) : -1); }
  inline VName getName () const noexcept { return (lump >= 0 ? W_LumpName(lump) : NAME_None); }
  inline VStr getFullName () const noexcept { return (lump >= 0 ? W_FullLumpName(lump) : VStr::EmptyString); }
  inline VStr getRealName () const noexcept { return (lump >= 0 ? W_RealLumpName(lump) : VStr::EmptyString); } // without pak prefix
  inline VStr getFullPakName () const noexcept { return (lump >= 0 ? W_FullPakNameForLump(lump) : VStr::EmptyString); } // without pak prefix

  inline bool isAux () const noexcept { return (lump >= 0 ? W_IsAuxLump(lump) : false); }
  inline bool isIWAD () const noexcept { return (lump >= 0 ? W_IsIWADLump(lump) : false); }
  inline bool isIWad () const noexcept { return (lump >= 0 ? W_IsIWADLump(lump) : false); }
};


#endif
