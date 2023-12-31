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
//**    Handles WAD file header, directory, lump I/O.
//**
//**************************************************************************
#include "fsys_local.h"

//#define VAVOOM_FSYS_DEBUG_OPENERS
//#define VAVOOM_FSYS_DEBUG_FAST_SEEK


#define GET_LUMP_FILE(num)    (fsysSearchPaths[((num)>>16)&0xffff])
#define FILE_INDEX(num)       ((num)>>16)
#define LUMP_INDEX(num)       ((num)&0xffff)
#define MAKE_HANDLE(wi, num)  (((wi)<<16)+(num))


// ////////////////////////////////////////////////////////////////////////// //
extern TArray<VStr> fsysWadFileNames;

static int AuxiliaryIndex = -1;

static TMap<VStr, int> fullNameTexLumpChecked;

bool fsys_EnableAuxSearch = false;

// don't search files in auxiliary wads, ever
static inline int getSPCount () { return (AuxiliaryIndex >= 0 && !fsys_EnableAuxSearch ? AuxiliaryIndex : fsysSearchPaths.length()); }


static int auxMarkCounter = 0;

struct FSysAuxMark {
  int counter;
  int AuxIndex; // copy of `AuxiliaryIndex`
  int FreeFromFile; // free from this file
};


// ////////////////////////////////////////////////////////////////////////// //
FArchiveReaderInfo *arcInfoHead = nullptr;
bool arcInfoArrayRecreate = true;
TArray<FArchiveReaderInfo *> fsysArchiveOpeners;
int arcInfoMaxSignLen = 0;
TArray<vuint8> arcInfoSignBuf;


// ////////////////////////////////////////////////////////////////////////// //
static int OpenerCmpFunc (const void *aa, const void *bb, void * /*udata*/) {
  if (aa == bb) return 0;
  const FArchiveReaderInfo *a = *(const FArchiveReaderInfo **)aa;
  const FArchiveReaderInfo *b = *(const FArchiveReaderInfo **)bb;
  if (a->sign && a->sign[0]) {
    // `a` has a signature, check `b`
    if (!b->sign || !b->sign[0]) return -1; // prefer `a` to signature-less format anyway
  } else {
    // `a` doesn't have a signature, check `b`
    if (b->sign && b->sign[0]) return 1; // prefer `b` to signature-less format anyway
  }
  return a->priority-b->priority;
}


//==========================================================================
//
//  FArchiveReaderInfo::FArchiveReaderInfo
//
//==========================================================================
FArchiveReaderInfo::FArchiveReaderInfo (const char *afmtname, OpenCB ocb, const char *asign, int apriority)
  : next(nullptr)
  , openCB(ocb)
  , fmtname(afmtname)
  , sign(asign)
  , priority(apriority)
{
  if (arcInfoHead) {
    FArchiveReaderInfo *last = arcInfoHead;
    while (last->next) last = last->next;
    last->next = this;
  } else {
    arcInfoHead = this;
  }
  arcInfoArrayRecreate = true;
  #ifdef VAVOOM_FSYS_DEBUG_OPENERS
  fprintf(stderr, "*** REGISTERING OPENER FOR '%s' (signature is '%s') ***\n", afmtname, (asign ? asign : ""));
  #endif
}


//==========================================================================
//
//  FArchiveReaderInfo::OpenArchive
//
//==========================================================================
VSearchPath *FArchiveReaderInfo::OpenArchive (VStream *strm, VStr filename, bool FixVoices) {
  if (!strm || strm->IsError()) return nullptr; // sanity check

  // fill opener array
  if (arcInfoArrayRecreate) {
    arcInfoArrayRecreate = false;
    int count = 0;
    for (FArchiveReaderInfo *opl = arcInfoHead; opl; opl = opl->next) ++count;
    fsysArchiveOpeners.clear();
    fsysArchiveOpeners.resize(count);
    // now fill it
    arcInfoMaxSignLen = 0;
    for (FArchiveReaderInfo *opl = arcInfoHead; opl; opl = opl->next) {
      fsysArchiveOpeners.append(opl);
      if (opl->sign && opl->sign[0]) {
        size_t slen = strlen(opl->sign);
        if (slen > 1024) Sys_Error("signature too long for archive format '%s'!", opl->fmtname);
        if (slen > (vuint32)arcInfoMaxSignLen) arcInfoMaxSignLen = (vint32)slen;
      }
    }
    timsort_r(fsysArchiveOpeners.ptr(), fsysArchiveOpeners.length(), sizeof(FArchiveReaderInfo *), &OpenerCmpFunc, nullptr);
  }

  if (arcInfoSignBuf.length() != arcInfoMaxSignLen) arcInfoSignBuf.setLength(arcInfoMaxSignLen);
  int lastsignlen = 0;
  #ifdef VAVOOM_FSYS_DEBUG_OPENERS
  GLog.Logf(NAME_Debug, "=== checking '%s' with %d openers ===", *filename, fsysArchiveOpeners.length());
  #endif
  for (auto &&op : fsysArchiveOpeners) {
    #ifdef VAVOOM_FSYS_DEBUG_OPENERS
    GLog.Logf(NAME_Debug, "  trying opener for '%s'...", op->fmtname);
    #endif
    // has signature?
    if (op->sign && op->sign[0]) {
      // has signature, perform signature check
      int slen = (int)strlen(op->sign);
      if (lastsignlen < slen) {
        if (strm->Tell() != 0) strm->Seek(0);
        if (strm->IsError()) return nullptr;
        memset(arcInfoSignBuf.ptr(), 0, slen);
        strm->Serialise(arcInfoSignBuf.ptr(), slen);
        if (strm->IsError()) return nullptr;
        lastsignlen = slen;
      }
      if (memcmp(arcInfoSignBuf.ptr(), op->sign, slen) != 0) {
        // bad signature
        #ifdef VAVOOM_FSYS_DEBUG_OPENERS
        GLog.Logf(NAME_Debug, "    signature check failed for '%s'...", op->fmtname);
        #endif
        continue;
      }
      if (strm->Tell() != slen) strm->Seek(0);
      if (strm->IsError()) return nullptr;
      VSearchPath *spt = op->openCB(strm, filename, FixVoices);
      if (spt) {
        #ifdef VAVOOM_FSYS_DEBUG_OPENERS
        GLog.Logf(NAME_Debug, "  opened '%s' as '%s'...", *filename, op->fmtname);
        #endif
        return spt;
      }
    } else {
      // no signature
      if (strm->Tell() != 0) strm->Seek(0);
      if (strm->IsError()) return nullptr;
      VSearchPath *spt = op->openCB(strm, filename, FixVoices);
      if (spt) {
        #ifdef VAVOOM_FSYS_DEBUG_OPENERS
        GLog.Logf(NAME_Debug, "  opened '%s' as '%s'...", *filename, op->fmtname);
        #endif
        return spt;
      }
    }
  }
  return nullptr;
}


//==========================================================================
//
//  VSearchPath::ListWadFiles
//
//==========================================================================
void VSearchPath::ListWadFiles (TArray<VStr> &/*list*/) {
}


//==========================================================================
//
//  VSearchPath::ListPk3Files
//
//==========================================================================
void VSearchPath::ListPk3Files (TArray<VStr> &/*list*/) {
}


//==========================================================================
//
//  AddArchiveFile_NoLock
//
//==========================================================================
static void AddArchiveFile_NoLock (VStr filename, VSearchPath *arc, bool allowpk3) {
  //fsysSearchPaths.Append(Zip); // already done by the caller

  if (fsys_simple_archives != FSYS_ARCHIVES_NORMAL) return;

  // add all WAD/PK3 files in the root of the archive file
  TArray<VStr> wadlist;
  arc->ListWadFiles(wadlist);
  if (allowpk3) arc->ListPk3Files(wadlist);

  for (auto &&wadname : wadlist) {
    VStream *WadStrm = arc->OpenFileRead(wadname, nullptr);
    if (!WadStrm) continue;
    if (WadStrm->TotalSize() > 0x1fffffff) { delete WadStrm; continue; } // file is too big (arbitrary limit)

    VStream *MemStrm;
    if (WadStrm->IsFastSeek()) {
      MemStrm = WadStrm;
      WadStrm = nullptr;
      #ifdef VAVOOM_FSYS_DEBUG_FAST_SEEK
      GLog.Logf(NAME_Debug, "FAST(AAF): %s:%s", *filename, *wadname);
      #endif
    } else {
      // decompress WAD file into a memory stream, since reading from ZIP will be very slow
      MemStrm = new VMemoryStream(filename+":"+wadname, WadStrm);
      bool err = WadStrm->IsError();
      delete WadStrm;
      WadStrm = nullptr;
      #ifdef VAVOOM_FSYS_DEBUG_FAST_SEEK
      GLog.Logf(NAME_Debug, "SLOW(AAF): %s:%s", *filename, *wadname);
      #endif
      if (err) { delete MemStrm; Sys_Error("cannot open WAD: \"%s:%s\"", *filename, *wadname); }
    }

    VSearchPath *wad = FArchiveReaderInfo::OpenArchive(MemStrm, filename+":"+wadname, false); // don't fix voices
    if (!wad) { delete MemStrm; continue; } // unknown format

    // if this is not a doom wad, and nested pk3s are not allowed, don't add it
    if (!allowpk3 && !wad->IsWad()) { delete wad; continue; }

    //W_AddFileFromZip(ZipName+":"+Wads[i], MemStrm);
    if (fsys_report_added_paks) GLog.Logf(fsys_report_added_paks_logtype, "Adding nested archive '%s'...", *wad->GetPrefix());
    fsysWadFileNames.Append(wadname);
    fsysSearchPaths.Append(wad);

    // if this is not a doom wad, and nested pk3s are allowed, recursively scan it
    if (allowpk3 && !wad->IsWad()) {
      if (fsys_report_added_paks) GLog.Logf(fsys_report_added_paks_logtype, "Adding nested archives from '%s'...", *wad->GetPrefix());
      AddArchiveFile_NoLock(wad->GetPrefix(), wad, false); // no nested pk3s allowed
    }
  }
}


//==========================================================================
//
//  W_AddDiskFile
//
//  All files are optional, but at least one file must be found (PWAD, if
//  all required lumps are present). Files with a .wad extension are archive
//  files with multiple lumps. Other files are single lumps with the base
//  filename for the lump name.
//
//  for non-wad files (zip, pk3, pak), it adds add subarchives
//  from a root directory
//
//==========================================================================
void W_AddDiskFile (VStr FileName, bool /*FixVoices*/) {
  int wadtime = Sys_FileTime(FileName);
  if (wadtime == -1) Sys_Error("Required file \"%s\" doesn't exist!", *FileName);

  VStream *strm = FL_OpenSysFileRead(FileName);
  if (!strm) Sys_Error("Cannot read required file \"%s\"!", *FileName);

  if (fsys_report_added_paks) GLog.Logf(fsys_report_added_paks_logtype, "Adding archive '%s'...", *FileName);

  MyThreadLocker glocker(&fsys_glock);
  VSearchPath *Wad = FArchiveReaderInfo::OpenArchive(strm, FileName);

  bool doomWad = false;

  if (!Wad) {
    if (strm->IsError()) Sys_Error("Cannot read required file \"%s\"!", *FileName);
    Wad = VWadFile::CreateSingleLumpStream(strm, FileName);
  } else {
    doomWad = Wad->IsWad();
  }

  fsysWadFileNames.Append(FileName);
  /* old code
  VStr ext = FileName.ExtractFileExtension();
  VWadFile *Wad = new VWadFile();
  if (!ext.strEquCI(".wad")) {
    Wad->OpenSingleLump(FileName);
  } else {
    Wad->Open(FileName, FixVoices, nullptr);
  }
  */

  fsysSearchPaths.Append(Wad);

  if (!doomWad) AddArchiveFile_NoLock(FileName, Wad, true); // allow nested wads
}


//==========================================================================
//
//  W_AddDiskFileOptional
//
//==========================================================================
bool W_AddDiskFileOptional (VStr FileName, bool /*FixVoices*/) {
  int wadtime = Sys_FileTime(FileName);
  if (wadtime == -1) return false;

  VStream *strm = FL_OpenSysFileRead(FileName);
  if (!strm) return false;

  if (fsys_report_added_paks) GLog.Logf(fsys_report_added_paks_logtype, "Adding archive '%s'...", *FileName);

  MyThreadLocker glocker(&fsys_glock);
  VSearchPath *Wad = FArchiveReaderInfo::OpenArchive(strm, FileName);

  bool doomWad = false;

  if (!Wad) {
    if (strm->IsError()) { VStream::Destroy(strm); return false; }
    //GLog.Logf(NAME_Debug, "OPTDISKFILE: cannot detect format for '%s'...", *FileName);
    Wad = VWadFile::CreateSingleLumpStream(strm, FileName);
  } else {
    doomWad = Wad->IsWad();
  }

  fsysWadFileNames.Append(FileName);
  fsysSearchPaths.Append(Wad);

  if (!doomWad) AddArchiveFile_NoLock(FileName, Wad, true); // allow nested wads

  return true;
}


//==========================================================================
//
//  W_MountDiskDir
//
//==========================================================================
void W_MountDiskDir (VStr dirname) {
  if (dirname.isEmpty()) return;
  VDirPakFile *dpak = new VDirPakFile(dirname);
  fsysSearchPaths.append(dpak);
  AddArchiveFile_NoLock(dirname, dpak, true); // allow nested wads
}


//==========================================================================
//
//  W_AddFileFromZip
//
//==========================================================================
void W_AddFileFromZip (VStr WadName, VStream *WadStrm) {
  // add WAD file
  MyThreadLocker glocker(&fsys_glock);
  fsysWadFileNames.Append(WadName);
  VWadFile *Wad = VWadFile::Create(WadName, false, WadStrm);
  fsysSearchPaths.Append(Wad);
}


//==========================================================================
//
//  W_CloseAuxiliaryNoLock
//
//==========================================================================
static void W_CloseAuxiliary_NoLock () {
  if (AuxiliaryIndex >= 0) {
    ++auxMarkCounter;
    // close all additional files
    for (int f = fsysSearchPaths.length()-1; f >= AuxiliaryIndex; --f) fsysSearchPaths[f]->Close();
    for (int f = fsysSearchPaths.length()-1; f >= AuxiliaryIndex; --f) {
      delete fsysSearchPaths[f];
      fsysSearchPaths[f] = nullptr;
    }
    fsysSearchPaths.setLength(AuxiliaryIndex);
    AuxiliaryIndex = -1;
  }
}


//==========================================================================
//
//  W_CloseAuxiliary
//
//==========================================================================
void W_CloseAuxiliary () {
  MyThreadLocker glocker(&fsys_glock);
  W_CloseAuxiliary_NoLock();
}


//==========================================================================
//
//  W_StartAuxiliary
//
//==========================================================================
int W_StartAuxiliary () {
  MyThreadLocker glocker(&fsys_glock);
  if (AuxiliaryIndex < 0) {
    AuxiliaryIndex = fsysSearchPaths.length();
    ++auxMarkCounter;
  }
  return MAKE_HANDLE(AuxiliaryIndex, 0);
}


//==========================================================================
//
//  W_OpenAuxiliary
//
//==========================================================================
int W_OpenAuxiliary (VStr FileName) {
  MyThreadLocker glocker(&fsys_glock);
  W_CloseAuxiliary_NoLock();
  vassert(AuxiliaryIndex < 0);
  AuxiliaryIndex = fsysSearchPaths.length();
  VStream *WadStrm = FL_OpenFileRead_NoLock(FileName, nullptr);
  if (!WadStrm) { AuxiliaryIndex = -1; return -1; }
  //fprintf(stderr, "*** AUX: '%s'\n", *FileName);
  auto olen = fsysWadFileNames.length();

  //W_AddFileFromZip(FileName, WadStrm); copypasted here
  VSearchPath *Wad = FArchiveReaderInfo::OpenArchive(WadStrm, FileName);
  if (!Wad) {
    if (WadStrm->IsError()) Sys_Error("Cannot read required file \"%s\"!", *FileName);
    VWadFile *wadone = VWadFile::CreateSingleLumpStream(WadStrm, FileName);
    fsysSearchPaths.Append(wadone);
  } else {
    fsysSearchPaths.Append(Wad);
  }

  /* old code
  {
    //fsysWadFileNames.Append(FileName);
    VWadFile *Wad = new VWadFile();
    Wad->Open(FileName, false, WadStrm);
    fsysSearchPaths.Append(Wad);
  }
  */

  // just in case
  fsysWadFileNames.setLength(olen);
  return MAKE_HANDLE(AuxiliaryIndex, 0);
}


//==========================================================================
//
//  W_GetFirstAuxFile
//
//  returns -1 if no aux archives were opened
//
//==========================================================================
int W_GetFirstAuxFile () {
  MyThreadLocker glocker(&fsys_glock);
  return (AuxiliaryIndex >= 0 ? AuxiliaryIndex : -1);
}


//==========================================================================
//
//  W_GetFirstAuxLump
//
//  returns -1 if no aux archives were opened, or lump id
//
//==========================================================================
int W_GetFirstAuxLump () {
  MyThreadLocker glocker(&fsys_glock);
  return (AuxiliaryIndex >= 0 ? MAKE_HANDLE(AuxiliaryIndex, 0) : -1);
}


//==========================================================================
//
//  zipAddWads
//
//==========================================================================
static void zipAddWads (VSearchPath *zip, VStr zipName) {
  if (!zip) return;
  TArray<VStr> list;
  // scan for wads
  zip->ListWadFiles(list);
  for (auto &&wadname : list) {
    VStream *wadstrm = zip->OpenFileRead(wadname, nullptr);
    if (!wadstrm) continue;
    if (wadstrm->TotalSize() < 16) { delete wadstrm; continue; }
    VStream *memstrm;
    if (wadstrm->IsFastSeek()) {
      memstrm = wadstrm;
      wadstrm = nullptr;
      #ifdef VAVOOM_FSYS_DEBUG_FAST_SEEK
      GLog.Logf(NAME_Debug, "FAST(ZAW): %s:%s", *zip->GetPrefix(), *wadname);
      #endif
    } else {
      memstrm = new VMemoryStream(zip->GetPrefix()+":"+wadname, wadstrm);
      bool err = wadstrm->IsError();
      delete wadstrm;
      wadstrm = nullptr;
      if (err) { delete memstrm; continue; }
      #ifdef VAVOOM_FSYS_DEBUG_FAST_SEEK
      GLog.Logf(NAME_Debug, "SLOW(ZAW): %s:%s", *zip->GetPrefix(), *wadname);
      #endif
    }
    char sign[4];
    memstrm->Serialise(sign, 4);
    if (memcmp(sign, "PWAD", 4) != 0 && memcmp(sign, "IWAD", 4) != 0) { delete memstrm; continue; }
    memstrm->Seek(0);
    VWadFile *wad = VWadFile::Create(zipName+":"+wadname, false, memstrm);
    fsysSearchPaths.Append(wad);
  }
}


//==========================================================================
//
//  W_AddAuxiliaryStream
//
//==========================================================================
int W_AddAuxiliaryStream (VStream *strm, WAuxFileType ftype) {
  if (!strm) return -1;
  MyThreadLocker glocker(&fsys_glock);

  //if (strm.TotalSize() < 16) return -1;
  if (AuxiliaryIndex < 0) AuxiliaryIndex = fsysSearchPaths.length();
  int residx = fsysSearchPaths.length();
  //GLog.Logf("AUX: %s", *strm->GetName());

  if (ftype != WAuxFileType::VFS_Wad) {
    // guess it
    VSearchPath *zip = FArchiveReaderInfo::OpenArchive(strm, strm->GetName());
    if (!zip) return -1;
    //VZipFile *zip = new VZipFile(strm, strm->GetName());
    fsysSearchPaths.Append(zip);
    // scan for wads and pk3s
    if (ftype == WAuxFileType::VFS_Zip) {
      zipAddWads(zip, strm->GetName());
      // scan for pk3s
      TArray<VStr> list;
      zip->ListPk3Files(list);
      for (auto &&pk3name : list) {
        VStream *zipstrm = zip->OpenFileRead(pk3name, nullptr);
        if (!zipstrm) continue;
        if (zipstrm->TotalSize() < 16) { delete zipstrm; continue; }
        VSearchPath *pk3;
        VStream *memstrm;
        if (zipstrm->IsFastSeek()) {
          memstrm = zipstrm;
          zipstrm = nullptr;
          #ifdef VAVOOM_FSYS_DEBUG_FAST_SEEK
          GLog.Logf(NAME_Debug, "FAST(AAS): %s:%s", *zip->GetPrefix(), *pk3name);
          #endif
        } else {
          memstrm = new VMemoryStream(zip->GetPrefix()+":"+pk3name, zipstrm);
          bool err = zipstrm->IsError();
          delete zipstrm;
          zipstrm = nullptr;
          if (err) { delete memstrm; continue; }
          //GLog.Logf("AUX: %s", *(strm->GetName()+":"+wadname));
          #ifdef VAVOOM_FSYS_DEBUG_FAST_SEEK
          GLog.Logf(NAME_Debug, "SLOW(AAS): %s:%s", *zip->GetPrefix(), *pk3name);
          #endif
        }
        // guess
        pk3 = FArchiveReaderInfo::OpenArchive(memstrm, zip->GetPrefix()+":"+pk3name);
        if (pk3) {
          //VZipFile *pk3 = new VZipFile(memstrm, strm->GetName()+":"+wadname);
          fsysSearchPaths.Append(pk3);
          zipAddWads(pk3, pk3->GetPrefix());
        } else {
          delete memstrm;
          delete zipstrm;
        }
      }
    }
  } else {
    VWadFile *wad = VWadFile::Create(strm->GetName(), false, strm);
    fsysSearchPaths.Append(wad);
  }

  return MAKE_HANDLE(residx, 0);
}


//==========================================================================
//
//  W_MarkAuxiliary
//
//  can return `nullptr` on error
//
//==========================================================================
FSysAuxMark *W_MarkAuxiliary () {
  MyThreadLocker glocker(&fsys_glock);
  if (AuxiliaryIndex < 0) return nullptr;
  FSysAuxMark *res = (FSysAuxMark *)Z_Calloc(sizeof(FSysAuxMark));
  res->counter = auxMarkCounter;
  res->AuxIndex = AuxiliaryIndex;
  res->FreeFromFile = fsysSearchPaths.length();
  return res;
}


//==========================================================================
//
//  W_ReleaseAuxiliary
//
//  will free the mark, it cannot be used after this
//
//==========================================================================
void W_ReleaseAuxiliary (FSysAuxMark *&mark) {
  if (!mark) return;
  MyThreadLocker glocker(&fsys_glock);
  if (mark->counter == auxMarkCounter &&
      mark->AuxIndex == AuxiliaryIndex &&
      mark->FreeFromFile < fsysSearchPaths.length())
  {
    // close all additional files
    //GLog.Logf(NAME_Debug, "W_ReleaseAuxiliary: auxindex=%d; from=%d; last=%d", mark->AuxIndex, mark->FreeFromFile, fsysSearchPaths.length()-1);
    for (int f = fsysSearchPaths.length()-1; f >= mark->FreeFromFile; --f) fsysSearchPaths[f]->Close();
    for (int f = fsysSearchPaths.length()-1; f >= mark->FreeFromFile; --f) {
      delete fsysSearchPaths[f];
      fsysSearchPaths[f] = nullptr;
    }
    fsysSearchPaths.setLength(mark->FreeFromFile);
  }
  Z_Free(mark);
  mark = nullptr;
}


//==========================================================================
//
//  W_IsInAuxiliaryMode
//
//==========================================================================
bool W_IsInAuxiliaryMode () {
  MyThreadLocker glocker(&fsys_glock);
  return (AuxiliaryIndex >= 0);
}


//==========================================================================
//
//  W_IsInAuxiliaryMode
//
//==========================================================================
int W_AuxiliaryOpenedArchives () {
  MyThreadLocker glocker(&fsys_glock);
  if (AuxiliaryIndex < 0) return 0;
  return fsysSearchPaths.length()-AuxiliaryIndex;
}


//==========================================================================
//
//  W_CheckNumForName
//
//  Returns -1 if name not found.
//
//==========================================================================
int W_CheckNumForName (VName Name, EWadNamespace NS) {
  if (Name == NAME_None) return -1;
  MyThreadLocker glocker(&fsys_glock);
  for (int wi = getSPCount()-1; wi >= 0; --wi) {
    int i = fsysSearchPaths[wi]->CheckNumForName(Name, NS);
    if (i >= 0) return MAKE_HANDLE(wi, i);
  }
  // not found
  return -1;
}


//==========================================================================
//
//  W_FindFirstLumpOccurence
//
//==========================================================================
int W_FindFirstLumpOccurence (VName lmpname, EWadNamespace NS) {
  if (lmpname == NAME_None) return -1;
  MyThreadLocker glocker(&fsys_glock);
  for (int wi = 0; wi < getSPCount(); ++wi) {
    int i = fsysSearchPaths[wi]->CheckNumForName(lmpname, NS, true);
    if (i >= 0) return MAKE_HANDLE(wi, i);
  }
  return -1;
}


//==========================================================================
//
//  W_GetNumForName
//
//  Calls W_CheckNumForName, but bombs out if not found.
//
//==========================================================================
int W_GetNumForName (VName Name, EWadNamespace NS) {
  vassert(Name != NAME_None);
  int i = W_CheckNumForName(Name, NS);
  if (i == -1) Sys_Error("W_GetNumForName: \"%s\" not found!", *Name);
  return i;
}


//==========================================================================
//
//  W_CheckNumForNameInFile
//
//  Returns -1 if name not found.
//
//==========================================================================
int W_CheckNumForNameInFile (VName Name, int File, EWadNamespace NS) {
  MyThreadLocker glocker(&fsys_glock);
  if (File < 0 || File >= getSPCount()) return -1;
  int i = fsysSearchPaths[File]->CheckNumForName(Name, NS);
  if (i >= 0) return MAKE_HANDLE(File, i);
  // not found
  return -1;
}


//==========================================================================
//
//  W_CheckNumForNameInFileOrLower
//
//  Returns -1 if name not found.
//
//==========================================================================
int W_CheckNumForNameInFileOrLower (VName Name, int File, EWadNamespace NS) {
  MyThreadLocker glocker(&fsys_glock);
  if (File >= getSPCount()) File = getSPCount()-1;
  while (File >= 0) {
    int i = fsysSearchPaths[File]->CheckNumForName(Name, NS);
    if (i >= 0) return MAKE_HANDLE(File, i);
    --File;
  }
  // not found
  return -1;
}


//==========================================================================
//
//  W_CheckFirstNumForNameInFile
//
//  Returns -1 if name not found.
//
//==========================================================================
int W_CheckFirstNumForNameInFile (VName Name, int File, EWadNamespace NS) {
  MyThreadLocker glocker(&fsys_glock);
  if (File < 0 || File >= getSPCount()) return -1;
  int i = fsysSearchPaths[File]->CheckNumForName(Name, NS, true);
  if (i >= 0) return MAKE_HANDLE(File, i);
  // not found
  return -1;
}


//==========================================================================
//
//  W_CheckLastNumForNameInFile
//
//  Returns -1 if name not found.
//
//==========================================================================
int W_CheckLastNumForNameInFile (VName Name, int File, EWadNamespace NS) {
  MyThreadLocker glocker(&fsys_glock);
  if (File < 0 || File >= getSPCount()) return -1;
  int i = fsysSearchPaths[File]->CheckNumForName(Name, NS, false); // want last
  if (i >= 0) return MAKE_HANDLE(File, i);
  // not found
  return -1;
}


//==========================================================================
//
//  W_CheckNumForFileName
//
//  Returns -1 if name not found.
//
//==========================================================================
int W_CheckNumForFileName (VStr Name) {
  MyThreadLocker glocker(&fsys_glock);
  for (int wi = getSPCount()-1; wi >= 0; --wi) {
    int i = fsysSearchPaths[wi]->CheckNumForFileName(Name);
    if (i >= 0) return MAKE_HANDLE(wi, i);
  }
  // not found
  return -1;
}


//==========================================================================
//
//  W_CheckNumForFileNameInSameFile
//
//  Returns -1 if name not found.
//
//==========================================================================
int W_CheckNumForFileNameInSameFile (int filelump, VStr Name) {
  if (filelump < 0) return W_CheckNumForFileName(Name);
  int fidx = FILE_INDEX(filelump);
  MyThreadLocker glocker(&fsys_glock);
  if (fidx < 0 || fidx >= getSPCount()) return -1;
  VSearchPath *w = fsysSearchPaths[fidx];
  int i = w->CheckNumForFileName(Name);
  if (i >= 0) return MAKE_HANDLE(fidx, i);
  // not found
  return -1;
}


//==========================================================================
//
//  W_CheckNumForFileNameInSameFileOrLower
//
//  Returns -1 if name not found.
//
//==========================================================================
int W_CheckNumForFileNameInSameFileOrLower (int filelump, VStr Name) {
  if (filelump < 0) return W_CheckNumForFileName(Name);
  int fidx = FILE_INDEX(filelump);
  MyThreadLocker glocker(&fsys_glock);
  if (fidx >= getSPCount()) fidx = getSPCount()-1;
  while (fidx >= 0) {
    VSearchPath *w = fsysSearchPaths[fidx];
    int i = w->CheckNumForFileName(Name);
    if (i >= 0) return MAKE_HANDLE(fidx, i);
    --fidx;
  }
  // not found
  return -1;
}


//==========================================================================
//
//  W_FindACSObjectInFile
//
//  Returns -1 if name not found.
//
//==========================================================================
int W_FindACSObjectInFile (VStr Name, int File) {
  // check auxiliaries too
  MyThreadLocker glocker(&fsys_glock);
  if (File < 0 || File >= fsysSearchPaths.length()) return -1;
  while (File >= 0) {
    int i = fsysSearchPaths[File]->FindACSObject(Name);
    if (i >= 0) return MAKE_HANDLE(File, i);
    --File;
  }
  // not found
  return -1;
}


//==========================================================================
//
//  tryWithExtension
//
//==========================================================================
static int tryWithExtension (VStr name, const char *ext) {
  if (ext && *ext) name = name+ext;
  for (int wi = getSPCount()-1; wi >= 0; --wi) {
    int i = fsysSearchPaths[wi]->CheckNumForFileName(name);
    if (i >= 0) return MAKE_HANDLE(wi, i);
  }
  return -1;
}


//==========================================================================
//
//  W_CheckNumForTextureFileName
//
//  Returns -1 if name not found.
//
//==========================================================================
int W_CheckNumForTextureFileName (VStr Name) {
  /*static*/ const char *textureExts[] = { ".png", ".jpg", ".tga", ".imgz", ".lmp", ".jpeg", ".pcx", ".bmp", nullptr };

  MyThreadLocker glocker(&fsys_glock);
  VStr loname = (Name.isLowerCase() ? Name : Name.toLowerCase());
  auto ip = fullNameTexLumpChecked.get(loname);
  if (ip) return *ip;

  int res = -1;

  if ((res = tryWithExtension(Name, nullptr)) >= 0) { fullNameTexLumpChecked.put(loname, res); return res; }

  // try "textures/..."
  VStr fname = VStr("textures/")+Name;
  if ((res = tryWithExtension(fname, nullptr)) >= 0) { fullNameTexLumpChecked.put(loname, res); return res; }
  // various other image extensions
  for (const char **extptr = textureExts; *extptr; ++extptr) {
    if ((res = tryWithExtension(fname, *extptr)) >= 0) { fullNameTexLumpChecked.put(loname, res); return res; }
  }

  fullNameTexLumpChecked.put(loname, -1);
  return -1;
}


//==========================================================================
//
//  W_GetNumForFileName
//
//  Calls W_CheckNumForFileName, but bombs out if not found.
//
//==========================================================================
int W_GetNumForFileName (VStr Name) {
  int i = W_CheckNumForFileName(Name);
  if (i == -1) Sys_Error("W_GetNumForFileName: %s not found!", *Name);
  return i;
}


//==========================================================================
//
//  W_LumpLength
//
//  Returns the buffer size needed to load the given lump.
//
//==========================================================================
int W_LumpLength (int lump) {
  MyThreadLocker glocker(&fsys_glock);
  if (lump < 0 || FILE_INDEX(lump) >= fsysSearchPaths.length()) Sys_Error("W_LumpLength: %i >= num_wad_files", FILE_INDEX(lump));
  VSearchPath *w = GET_LUMP_FILE(lump);
  int lumpindex = LUMP_INDEX(lump);
  return w->LumpLength(lumpindex);
}


//==========================================================================
//
//  W_LumpName
//
//==========================================================================
VName W_LumpName (int lump) {
  MyThreadLocker glocker(&fsys_glock);
  if (lump < 0 || FILE_INDEX(lump) >= fsysSearchPaths.length()) return NAME_None;
  VSearchPath *w = GET_LUMP_FILE(lump);
  int lumpindex = LUMP_INDEX(lump);
  return w->LumpName(lumpindex);
}


//==========================================================================
//
//  W_FullLumpName_NoLock
//
//==========================================================================
static VVA_OKUNUSED VStr W_FullLumpName_NoLock (int lump) {
  if (lump < 0 || FILE_INDEX(lump) >= fsysSearchPaths.length()) return VStr("<invalid>");
  VSearchPath *w = GET_LUMP_FILE(lump);
  int lumpindex = LUMP_INDEX(lump);
  //return w->GetPrefix()+":"+*(w->LumpName(lumpindex));
  return w->GetPrefix()+":"+(w->LumpFileName(lumpindex));
}


//==========================================================================
//
//  W_FullLumpName
//
//==========================================================================
VStr W_FullLumpName (int lump) {
  MyThreadLocker glocker(&fsys_glock);
  if (lump < 0 || FILE_INDEX(lump) >= fsysSearchPaths.length()) return VStr("<invalid>");
  VSearchPath *w = GET_LUMP_FILE(lump);
  int lumpindex = LUMP_INDEX(lump);
  //return w->GetPrefix()+":"+*(w->LumpName(lumpindex));
  return w->GetPrefix()+":"+(w->LumpFileName(lumpindex));
}


//==========================================================================
//
//  W_RealLumpName
//
//==========================================================================
VStr W_RealLumpName (int lump) {
  MyThreadLocker glocker(&fsys_glock);
  if (lump < 0 || FILE_INDEX(lump) >= fsysSearchPaths.length()) return VStr();
  VSearchPath *w = GET_LUMP_FILE(lump);
  int lumpindex = LUMP_INDEX(lump);
  //return w->GetPrefix()+":"+*(w->LumpName(lumpindex));
  return w->LumpFileName(lumpindex);
}


//==========================================================================
//
//  W_FullPakNameForLump
//
//==========================================================================
VStr W_FullPakNameForLump (int lump) {
  MyThreadLocker glocker(&fsys_glock);
  if (lump < 0 || FILE_INDEX(lump) >= fsysSearchPaths.length()) return VStr("<invalid>");
  VSearchPath *w = GET_LUMP_FILE(lump);
  return w->GetPrefix();
}


//==========================================================================
//
//  W_FullPakNameByFile
//
//==========================================================================
VStr W_FullPakNameByFile (int fidx) {
  MyThreadLocker glocker(&fsys_glock);
  if (fidx < 0 || fidx >= fsysSearchPaths.length()) return VStr("<invalid>");
  VSearchPath *w = fsysSearchPaths[fidx];
  return w->GetPrefix();
}


//==========================================================================
//
//  W_LumpFile
//
//  Returns file index of the given lump.
//
//==========================================================================
int W_LumpFile (int lump) {
  return (lump < 0 ? -1 : FILE_INDEX(lump));
}


//==========================================================================
//
//  W_ReadFromLump
//
//==========================================================================
void W_ReadFromLump (int lump, void *dest, int pos, int size) {
  MyThreadLocker glocker(&fsys_glock);
  if (lump < 0 || FILE_INDEX(lump) >= fsysSearchPaths.length()) Sys_Error("W_ReadFromLump: %i >= num_wad_files", FILE_INDEX(lump));
  VSearchPath *w = GET_LUMP_FILE(lump);
  w->ReadFromLump(LUMP_INDEX(lump), dest, pos, size);
}


//==========================================================================
//
//  W_CreateLumpReaderNum
//
//==========================================================================
VStream *W_CreateLumpReaderNum (int lump) {
  MyThreadLocker glocker(&fsys_glock);
  if (lump < 0 || FILE_INDEX(lump) >= fsysSearchPaths.length()) Sys_Error("W_CreateLumpReaderNum: %i >= num_wad_files", FILE_INDEX(lump));
  return GET_LUMP_FILE(lump)->CreateLumpReaderNum(LUMP_INDEX(lump));
}


//==========================================================================
//
//  W_CreateLumpReaderName
//
//==========================================================================
VStream *W_CreateLumpReaderName (VName Name, EWadNamespace NS) {
  return W_CreateLumpReaderNum(W_GetNumForName(Name, NS));
}


//==========================================================================
//
//  W_StartIterationFromLumpFileNS
//
//==========================================================================
int W_StartIterationFromLumpFileNS (int File, EWadNamespace NS) {
  if (File < 0) return -1;
  MyThreadLocker glocker(&fsys_glock);
  if (File >= getSPCount()) return -1;
  for (int li = 0; File < getSPCount(); ++File, li = 0) {
    li = fsysSearchPaths[File]->IterateNS(li, NS);
    if (li != -1) return MAKE_HANDLE(File, li);
  }
  return -1;
}


//==========================================================================
//
//  W_IterateNS_NoLock
//
//==========================================================================
static int W_IterateNS_NoLock (int Prev, EWadNamespace NS) {
  if (Prev < 0) Prev = -1;
  int wi = FILE_INDEX(Prev+1);
  int li = LUMP_INDEX(Prev+1);
  for (; wi < getSPCount(); ++wi, li = 0) {
    li = fsysSearchPaths[wi]->IterateNS(li, NS);
    if (li != -1) return MAKE_HANDLE(wi, li);
  }
  return -1;
}


//==========================================================================
//
//  W_IterateNS
//
//==========================================================================
int W_IterateNS (int Prev, EWadNamespace NS) {
  MyThreadLocker glocker(&fsys_glock);
  return W_IterateNS_NoLock(Prev, NS);
}


//==========================================================================
//
//  W_IterateFile
//
//==========================================================================
int W_IterateFile (int Prev, VStr Name) {
  MyThreadLocker glocker(&fsys_glock);
  if (Name.isEmpty()) return -1;
  if (Prev < 0) Prev = -1;
  //GLog.Logf(NAME_Dev, "W_IterateFile: Prev=%d (%d); fn=<%s>", Prev, getSPCount(), *Name);
  for (int wi = FILE_INDEX(Prev)+1; wi < getSPCount(); ++wi) {
    int li = fsysSearchPaths[wi]->CheckNumForFileName(Name);
    //GLog.Logf(NAME_Dev, "W_IterateFile: wi=%d (%d); fn=<%s>; li=%d", wi, getSPCount(), *Name, li);
    if (li != -1) return MAKE_HANDLE(wi, li);
  }
  return -1;
}


//==========================================================================
//
//  W_IterateDirectory
//
//==========================================================================
int W_IterateDirectory (int Prev, VStr DirName, bool allowSubdirs) {
  MyThreadLocker glocker(&fsys_glock);
  if (DirName.isEmpty()) {
    DirName = "/";
  } else {
    DirName = DirName.fixSlashes().AppendTrailingSlash();
  }
  bool rootDir = (DirName == "/" || DirName == "//");
  bool onlyBase = false;
  if (DirName.startsWith("/")) {
    DirName.chopLeft(1);
    if (DirName.startsWith("/")) {
      onlyBase = true;
      DirName.chopLeft(1);
      if (DirName.isEmpty()) rootDir = true;
    }
  }
  if (Prev < 0) Prev = -1;
  int wi = FILE_INDEX(Prev+1);
  int li = LUMP_INDEX(Prev+1);
  //GLog.Logf(NAME_Debug, "W_IterateDirectory: Curr=%d; wi=%d/%d; li=%d; DirName=<%s> (rootDir=%d); allowSubdirs=%d", Prev+1, wi, getSPCount(), li, *DirName, (int)rootDir, (int)allowSubdirs);
  for (; wi < getSPCount(); ++wi, li = 0) {
    if (onlyBase && !fsysSearchPaths[wi]->basepak) continue;
    for (;; ++li) {
      li = fsysSearchPaths[wi]->IterateNS(li, WADNS_AllFiles);
      if (li == -1) break;
      VStr fn = fsysSearchPaths[wi]->LumpFileName(li);
      if (fn.isEmpty()) continue; // just in case
      //GLog.Logf(NAME_Debug, "  li=%d; fn=<%s> : %d", li, *fn, (int)fn.startsWithCI(DirName));
      if (rootDir) {
        if (!allowSubdirs && fn.indexOf('/') >= 0) continue;
      } else {
        if (!fn.startsWithCI(DirName)) continue;
        if (!allowSubdirs && fn.indexOf('/', DirName.length()) >= 0) continue;
      }
      return MAKE_HANDLE(wi, li);
    }
  }
  return -1;
}


//==========================================================================
//
//  W_FindLumpByFileNameWithExts
//
//==========================================================================
int W_FindLumpByFileNameWithExts (VStr BaseName, const char **Exts) {
  int Found = -1;
  for (const char **Ext = Exts; *Ext; ++Ext) {
    VStr Check = BaseName+"."+(*Ext);
    int Lump = W_CheckNumForFileName(Check);
    if (Lump <= Found) continue;
    // For files from the same directory the order of extensions defines the priority order
    if (Found >= 0 && W_LumpFile(Found) == W_LumpFile(Lump)) continue;
    Found = Lump;
  }
  return Found;
}


//==========================================================================
//
//  W_LoadTextLump
//
//==========================================================================
VStr W_LoadTextLump (VName name) {
  VStream *Strm = W_CreateLumpReaderName(name);
  if (!Strm) {
    GLog.Logf(NAME_Warning, "cannot load text lump '%s'", *name);
    return VStr::EmptyString;
  }
  int msgSize = Strm->TotalSize();
  char *buf = new char[msgSize+1];
  Strm->Serialise(buf, msgSize);
  if (Strm->IsError()) {
    GLog.Logf(NAME_Warning, "cannot load text lump '%s'", *name);
    return VStr::EmptyString;
  }
  VStream::Destroy(Strm);

  buf[msgSize] = 0; // append terminator
  VStr Ret = buf;
  delete[] buf;

  if (!Ret.IsValidUtf8()) {
    GLog.Logf(NAME_Warning, "'%s' is not a valid UTF-8 text lump, assuming Latin 1", *name);
    Ret = Ret.Latin1ToUtf8();
  }
  return Ret;
}


//==========================================================================
//
//  W_LoadLumpIntoArrayIdx
//
//==========================================================================
void W_LoadLumpIntoArrayIdx (int Lump, TArrayNC<vuint8> &Array) {
  VStream *Strm = W_CreateLumpReaderNum(Lump);
  if (!Strm) Sys_Error("error reading lump with index %d", Lump);
  vassert(Strm);
  Array.setLength(Strm->TotalSize());
  Strm->Serialise(Array.Ptr(), Strm->TotalSize());
  if (Strm->IsError()) { VStream::Destroy(Strm); Sys_Error("error reading lump '%s'", *W_FullLumpName(Lump)); }
  VStream::Destroy(Strm);
}


//==========================================================================
//
//  W_LoadLumpIntoArray
//
//==========================================================================
void W_LoadLumpIntoArray (VName LumpName, TArrayNC<vuint8> &Array) {
  int Lump = W_CheckNumForFileName(*LumpName);
  if (Lump < 0) Lump = W_GetNumForName(LumpName);
  VStream *Strm = W_CreateLumpReaderNum(Lump);
  vassert(Strm);
  Array.setLength(Strm->TotalSize());
  Strm->Serialise(Array.Ptr(), Strm->TotalSize());
  if (Strm->IsError()) { VStream::Destroy(Strm); Sys_Error("error reading lump '%s'", *W_FullLumpName(Lump)); }
  VStream::Destroy(Strm);
}


//==========================================================================
//
//  W_Shutdown
//
//==========================================================================
void W_Shutdown () {
  MyThreadLocker glocker(&fsys_glock);
  for (int i = fsysSearchPaths.length()-1; i >= 0; --i) {
    delete fsysSearchPaths[i];
    fsysSearchPaths[i] = nullptr;
  }
  fsysSearchPaths.Clear();
}


//==========================================================================
//
//  W_NextMountFileId
//
//==========================================================================
int W_NextMountFileId () {
  MyThreadLocker glocker(&fsys_glock);
  return fsysSearchPaths.length();
}


//==========================================================================
//
//  W_ValidateUDMFMapLumps_NoLock
//
//==========================================================================
static bool W_ValidateUDMFMapLumps_NoLock (int lump) {
  if (lump < 0) return false;
  int fidx = FILE_INDEX(lump);
  if (fidx < 0 || fidx >= fsysSearchPaths.length()) return false;
  int lidx = LUMP_INDEX(lump);
  VSearchPath *sp = fsysSearchPaths[fidx];
  vassert(sp);
  if (sp->LumpName(++lidx) != NAME_textmap) return false; // also, skips the header
  bool wasBeh = false, wasBlock = false, wasRej = false, wasDialog = false, wasZNodes = false;
  for (;;) {
    VName lname = sp->LumpName(++lidx); // prefix, to skip the header
    if (lname == NAME_endmap) break;
    if (lname == NAME_None || lname == NAME_textmap) return false;
    if (lname == NAME_behavior) { if (wasBeh) return false; wasBeh = true; continue; }
    if (lname == NAME_blockmap) { if (wasBlock) return false; wasBlock = true; continue; }
    if (lname == NAME_reject) { if (wasRej) return false; wasRej = true; continue; }
    if (lname == NAME_dialogue) { if (wasDialog) return false; wasDialog = true; continue; }
    if (lname == NAME_znodes) { if (wasZNodes) return false; wasZNodes = true; continue; }
  }
  return true;
}


//==========================================================================
//
//  W_ValidateNormalMapLumps_NoLock
//
//==========================================================================
static bool W_ValidateNormalMapLumps_NoLock (int lump) {
  if (lump < 0) return false;
  int fidx = FILE_INDEX(lump);
  if (fidx < 0 || fidx >= fsysSearchPaths.length()) return false;
  int lidx = LUMP_INDEX(lump);
  VSearchPath *sp = fsysSearchPaths[fidx];
  vassert(sp);
  if (sp->LumpName(++lidx) != NAME_things) return false;
  if (sp->LumpName(++lidx) != NAME_linedefs) return false;
  if (sp->LumpName(++lidx) != NAME_sidedefs) return false;
  if (sp->LumpName(++lidx) != NAME_vertexes) return false;
  // optional lumps
  if (sp->LumpName(++lidx) != NAME_segs) --lidx;
  if (sp->LumpName(++lidx) != NAME_ssectors) --lidx;
  if (sp->LumpName(++lidx) != NAME_nodes) --lidx;
  // required sectors lump
  if (sp->LumpName(++lidx) != NAME_sectors) return false;
  // NAME_reject, NAME_blockmap, NAME_behavior, NAME_scripts are not interesting
  return true;
}


//==========================================================================
//
//  W_IsValidMapHeaderLump_NoLock
//
//==========================================================================
static bool W_IsValidMapHeaderLump_NoLock (int lump) {
  if (lump < 0) return false;
  int fidx = FILE_INDEX(lump);
  if (fidx < 0 || fidx >= fsysSearchPaths.length()) return false;
  int lidx = LUMP_INDEX(lump);
  if (fsysSearchPaths[fidx]->LumpName(lidx + 1) == NAME_textmap) {
    return W_ValidateUDMFMapLumps_NoLock(lump);
  } else {
    return W_ValidateNormalMapLumps_NoLock(lump);
  }
}


//==========================================================================
//
//  W_IsValidMapHeaderLump
//
//==========================================================================
bool W_IsValidMapHeaderLump (int lump) {
  MyThreadLocker glocker(&fsys_glock);
  return W_IsValidMapHeaderLump_NoLock(lump);
}


//==========================================================================
//
//  WadMapIterator::advanceToNextMapLump
//
//==========================================================================
void WadMapIterator::advanceToNextMapLump () {
  MyThreadLocker glocker(&fsys_glock);
  //GLog.Logf(NAME_Debug, "***START: lump=%d*** (%s)", lump, *W_FullLumpName_NoLock(lump));
  while (lump >= 0) {
    if (W_IsValidMapHeaderLump_NoLock(lump)) {
      //GLog.Logf(NAME_Debug, "MAP LUMP %d (%s)", lump, *W_FullLumpName_NoLock(lump));
      return;
    }
    //GLog.Logf(NAME_Debug, "skip lump %d (%s); fidx=%d; lidx=%d; spcount=%d; pakcount=%d", lump, *W_FullLumpName_NoLock(lump), FILE_INDEX(lump), LUMP_INDEX(lump), getSPCount(), fsysSearchPaths.length());
    lump = W_IterateNS_NoLock(lump, WADNS_Global);
  }
  //GLog.Log(NAME_Debug, "***DEAD***");
  lump = -1; // just in case
}


//==========================================================================
//
//  W_IsWadPK3File
//
//==========================================================================
bool W_IsWadPK3File (int fidx) {
  MyThreadLocker glocker(&fsys_glock);
  if (fidx < 0 || fidx >= getSPCount()) return false;
  VSearchPath *w = fsysSearchPaths[fidx];
  return w->IsAnyPak();
}


//==========================================================================
//
//  W_IsPakFile
//
//  not a container, not a wad (usually pk3)
//
//==========================================================================
bool W_IsPakFile (int fidx) {
  MyThreadLocker glocker(&fsys_glock);
  if (fidx < 0 || fidx >= getSPCount()) return false;
  VSearchPath *w = fsysSearchPaths[fidx];
  return (w->IsAnyPak() && !w->IsWad());
}


//==========================================================================
//
//  W_IsIWADFile
//
//==========================================================================
bool W_IsIWADFile (int fidx) {
  MyThreadLocker glocker(&fsys_glock);
  if (fidx < 0 || fidx >= getSPCount()) return false;
  return fsysSearchPaths[fidx]->iwad;
}


//==========================================================================
//
//  W_IsWADFile
//
//  not pk3, not disk
//
//==========================================================================
bool W_IsWADFile (int fidx) {
  MyThreadLocker glocker(&fsys_glock);
  if (fidx < 0 || fidx >= getSPCount()) return false;
  return fsysSearchPaths[fidx]->IsWad();
}


//==========================================================================
//
//  W_IsAuxFile
//
//==========================================================================
bool W_IsAuxFile (int fidx) {
  MyThreadLocker glocker(&fsys_glock);
  if (fidx < 0 || fidx >= getSPCount()) return false;
  return (fidx >= AuxiliaryIndex && fidx < fsysSearchPaths.length());
}


//==========================================================================
//
//  W_IsWADLump
//
//  not pk3, not disk
//
//==========================================================================
bool W_IsWADLump (int lump) {
  if (lump < 0) return false;
  const int fidx = FILE_INDEX(lump);
  return W_IsWADFile(fidx);
}


//==========================================================================
//
//  W_IsIWADLump
//
//==========================================================================
bool W_IsIWADLump (int lump) {
  if (lump < 0) return false;
  const int fidx = FILE_INDEX(lump);
  return W_IsIWADFile(fidx);
}


//==========================================================================
//
//  W_IsAuxLump
//
//==========================================================================
bool W_IsAuxLump (int lump) {
  MyThreadLocker glocker(&fsys_glock);
  if (lump < 0 || AuxiliaryIndex < 0) return false;
  const int fidx = FILE_INDEX(lump);
  return (fidx >= AuxiliaryIndex && fidx < fsysSearchPaths.length());
}


//==========================================================================
//
//  W_IsUserWadFile
//
//==========================================================================
bool W_IsUserWadFile (int fidx) {
  MyThreadLocker glocker(&fsys_glock);
  if (fidx < 0 || fidx >= getSPCount()) return false;
  return fsysSearchPaths[fidx]->userwad;
}


//==========================================================================
//
//  W_IsUserWadLump
//
//==========================================================================
bool W_IsUserWadLump (int lump) {
  if (lump < 0) return false;
  const int fidx = FILE_INDEX(lump);
  return W_IsUserWadFile(fidx);
}


//==========================================================================
//
//  W_IsFastSeekLump
//
//==========================================================================
bool W_IsFastSeekLump (int lump) {
  if (lump < 0) return false;
  const int fidx = FILE_INDEX(lump);
  if (fidx < 0) return false;
  MyThreadLocker glocker(&fsys_glock);
  return (fidx >= 0 && fidx < fsysSearchPaths.length()
          ? fsysSearchPaths[fidx]->IsFastSeek(LUMP_INDEX(fidx))
          : false);
}
