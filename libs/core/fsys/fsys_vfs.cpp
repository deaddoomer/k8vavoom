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
//**
//**    Handles WAD file header, directory, lump I/O.
//**
//**************************************************************************
#include "fsys_local.h"


#define GET_LUMP_FILE(num)    SearchPaths[((num)>>16)&0xffff]
#define FILE_INDEX(num)       ((num)>>16)
#define LUMP_INDEX(num)       ((num)&0xffff)
#define MAKE_HANDLE(wi, num)  (((wi)<<16)+(num))


// ////////////////////////////////////////////////////////////////////////// //
extern TArray<VStr> wadfiles;

static int AuxiliaryIndex = 0;

static TMap<VStr, int> fullNameTexLumpChecked;

bool fsys_EnableAuxSearch = false;

// don't search files in auxiliary wads, ever
static inline int getSPCount () { return (AuxiliaryIndex && !fsys_EnableAuxSearch ? AuxiliaryIndex : SearchPaths.length()); }


//==========================================================================
//
//  W_AddFile
//
//  All files are optional, but at least one file must be found (PWAD, if
//  all required lumps are present). Files with a .wad extension are wadlink
//  files with multiple lumps. Other files are single lumps with the base
//  filename for the lump name.
//
//==========================================================================
void W_AddFile (const VStr &FileName, bool FixVoices) {
  int wadtime;

  wadtime = Sys_FileTime(FileName);
  if (wadtime == -1) Sys_Error("Required file %s doesn't exist", *FileName);

  wadfiles.Append(FileName);

  VStr ext = FileName.ExtractFileExtension();
  VWadFile *Wad = new VWadFile;
  if (!ext.strEquCI(".wad")) {
    Wad->OpenSingleLump(FileName);
  } else {
    Wad->Open(FileName, FixVoices, nullptr);
  }
  SearchPaths.Append(Wad);
}


//==========================================================================
//
//  W_AddFileFromZip
//
//==========================================================================
void W_AddFileFromZip (const VStr &WadName, VStream *WadStrm) {
  // add WAD file
  wadfiles.Append(WadName);
  VWadFile *Wad = new VWadFile;
  Wad->Open(WadName, false, WadStrm);
  SearchPaths.Append(Wad);
}


//==========================================================================
//
//  W_StartAuxiliary
//
//==========================================================================
int W_StartAuxiliary () {
  if (!AuxiliaryIndex) AuxiliaryIndex = SearchPaths.length();
  return MAKE_HANDLE(AuxiliaryIndex, 0);
}


//==========================================================================
//
//  W_OpenAuxiliary
//
//==========================================================================
int W_OpenAuxiliary (const VStr &FileName) {
  W_CloseAuxiliary();
  AuxiliaryIndex = SearchPaths.length();
  VStream *WadStrm = FL_OpenFileRead(FileName);
  if (!WadStrm) { AuxiliaryIndex = 0; return -1; }
  //fprintf(stderr, "*** AUX: '%s'\n", *FileName);
  auto olen = wadfiles.length();
  W_AddFileFromZip(FileName, WadStrm);
  wadfiles.setLength(olen);
  return MAKE_HANDLE(AuxiliaryIndex, 0);
}


//==========================================================================
//
//  zipAddWads
//
//==========================================================================
static void zipAddWads (VZipFile *zip, const VStr &zipName) {
  if (!zip) return;
  TArray<VStr> list;
  // scan for wads
  zip->ListWadFiles(list);
  for (int f = 0; f < list.length(); ++f) {
    VStream *wadstrm = zip->OpenFileRead(list[f]);
    if (!wadstrm) continue;
    if (wadstrm->TotalSize() < 16) { delete wadstrm; continue; }
    VStream *memstrm = new VMemoryStream(zip->GetPrefix()+":"+list[f], wadstrm);
    bool err = wadstrm->IsError();
    delete wadstrm;
    if (err) { delete memstrm; continue; }
    char sign[4];
    memstrm->Serialise(sign, 4);
    if (memcmp(sign, "PWAD", 4) != 0 && memcmp(sign, "IWAD", 4) != 0) { delete memstrm; continue; }
    memstrm->Seek(0);
    VWadFile *wad = new VWadFile;
    wad->Open(zipName+":"+list[f], false, memstrm);
    SearchPaths.Append(wad);
  }
}


//==========================================================================
//
//  W_AddAuxiliaryStream
//
//==========================================================================
int W_AddAuxiliaryStream (VStream *strm, WAuxFileType ftype) {
  if (!strm) return -1;
  //if (strm.TotalSize() < 16) return -1;
  if (!AuxiliaryIndex) AuxiliaryIndex = SearchPaths.length();
  int residx = SearchPaths.length();
  //GLog.Logf("AUX: %s", *strm->GetName());

  if (ftype != WAuxFileType::VFS_Wad) {
    VZipFile *zip = new VZipFile(strm, strm->GetName());
    SearchPaths.Append(zip);
    // scan for wads and pk3s
    if (ftype == WAuxFileType::VFS_Zip) {
      zipAddWads(zip, strm->GetName());
      // scan for pk3s
      TArray<VStr> list;
      zip->ListPk3Files(list);
      for (int f = 0; f < list.length(); ++f) {
        VStream *zipstrm = zip->OpenFileRead(list[f]);
        if (!zipstrm) continue;
        if (zipstrm->TotalSize() < 16) { delete zipstrm; continue; }
        VStream *memstrm = new VMemoryStream(zip->GetPrefix()+":"+list[f], zipstrm);
        bool err = zipstrm->IsError();
        delete zipstrm;
        if (err) { delete memstrm; continue; }
        //GLog.Logf("AUX: %s", *(strm->GetName()+":"+list[f]));
        VZipFile *pk3 = new VZipFile(memstrm, strm->GetName()+":"+list[f]);
        SearchPaths.Append(pk3);
        zipAddWads(pk3, pk3->GetPrefix());
      }
    }
  } else {
    VWadFile *wad = new VWadFile;
    //GLog.Logf("AUX: %s", *(strm->GetName()));
    wad->Open(strm->GetName(), false, strm);
    SearchPaths.Append(wad);
  }

  return MAKE_HANDLE(residx, 0);
}


//==========================================================================
//
//  W_CloseAuxiliary
//
//==========================================================================
void W_CloseAuxiliary () {
  if (AuxiliaryIndex) {
    // close all additional files
    for (int f = SearchPaths.length()-1; f >= AuxiliaryIndex; --f) SearchPaths[f]->Close();
    for (int f = SearchPaths.length()-1; f >= AuxiliaryIndex; --f) {
      delete SearchPaths[f];
      SearchPaths[f] = nullptr;
    }
    SearchPaths.setLength(AuxiliaryIndex);
    AuxiliaryIndex = 0;
  }
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
  for (int wi = getSPCount()-1; wi >= 0; --wi) {
    int i = SearchPaths[wi]->CheckNumForName(Name, NS);
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
  for (int wi = 0; wi < getSPCount(); ++wi) {
    int i = SearchPaths[wi]->CheckNumForName(lmpname, NS, true);
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
  check(Name != NAME_None);
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
  if (File < 0 || File >= getSPCount()) return -1;
  int i = SearchPaths[File]->CheckNumForName(Name, NS);
  if (i >= 0) return MAKE_HANDLE(File, i);
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
  if (File < 0 || File >= getSPCount()) return -1;
  int i = SearchPaths[File]->CheckNumForName(Name, NS, true);
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
int W_CheckNumForFileName (const VStr &Name) {
  for (int wi = getSPCount()-1; wi >= 0; --wi) {
    int i = SearchPaths[wi]->CheckNumForFileName(Name);
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
int W_CheckNumForFileNameInSameFile (int filelump, const VStr &Name) {
  if (filelump < 0) return W_CheckNumForFileName(Name);
  int fidx = FILE_INDEX(filelump);
  if (fidx < 0 || fidx >= getSPCount()) return -1;
  VSearchPath *w = SearchPaths[fidx];
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
int W_CheckNumForFileNameInSameFileOrLower (int filelump, const VStr &Name) {
  if (filelump < 0) return W_CheckNumForFileName(Name);
  int fidx = FILE_INDEX(filelump);
  if (fidx >= getSPCount()) fidx = getSPCount()-1;
  while (fidx >= 0) {
    VSearchPath *w = SearchPaths[fidx];
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
  if (File < 0 || File >= SearchPaths.length()) return -1;
  while (File >= 0) {
    int i = SearchPaths[File]->FindACSObject(Name);
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
    int i = SearchPaths[wi]->CheckNumForFileName(name);
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
int W_CheckNumForTextureFileName (const VStr &Name) {
  VStr loname = (Name.isLowerCase() ? Name : Name.toLowerCase());
  auto ip = fullNameTexLumpChecked.find(loname);
  if (ip) return *ip;

  int res = -1;

  if ((res = tryWithExtension(Name, nullptr)) >= 0) { fullNameTexLumpChecked.put(loname, res); return res; }

  // try "textures/..."
  VStr fname = VStr("textures/")+Name;
  if ((res = tryWithExtension(fname, nullptr)) >= 0) { fullNameTexLumpChecked.put(loname, res); return res; }
  // various other image extensions
  if ((res = tryWithExtension(fname, ".png")) >= 0) { fullNameTexLumpChecked.put(loname, res); return res; }
  if ((res = tryWithExtension(fname, ".jpg")) >= 0) { fullNameTexLumpChecked.put(loname, res); return res; }
  if ((res = tryWithExtension(fname, ".tga")) >= 0) { fullNameTexLumpChecked.put(loname, res); return res; }
  if ((res = tryWithExtension(fname, ".lmp")) >= 0) { fullNameTexLumpChecked.put(loname, res); return res; }
  if ((res = tryWithExtension(fname, ".jpeg")) >= 0) { fullNameTexLumpChecked.put(loname, res); return res; }

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
int W_GetNumForFileName (const VStr &Name) {
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
  if (lump < 0 || FILE_INDEX(lump) >= SearchPaths.length()) Sys_Error("W_LumpLength: %i >= num_wad_files", FILE_INDEX(lump));
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
  if (lump < 0 || FILE_INDEX(lump) >= SearchPaths.length()) return NAME_None;
  VSearchPath *w = GET_LUMP_FILE(lump);
  int lumpindex = LUMP_INDEX(lump);
  return w->LumpName(lumpindex);
}


//==========================================================================
//
//  W_FullLumpName
//
//==========================================================================
VStr W_FullLumpName (int lump) {
  if (lump < 0 || FILE_INDEX(lump) >= SearchPaths.length()) return VStr("<invalid>");
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
  if (lump < 0 || FILE_INDEX(lump) >= SearchPaths.length()) return VStr();
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
  if (lump < 0 || FILE_INDEX(lump) >= SearchPaths.length()) return VStr("<invalid>");
  VSearchPath *w = GET_LUMP_FILE(lump);
  return w->GetPrefix();
}


//==========================================================================
//
//  W_FullPakNameByFile
//
//==========================================================================
VStr W_FullPakNameByFile (int fidx) {
  if (fidx < 0 || fidx >= SearchPaths.length()) return VStr("<invalid>");
  VSearchPath *w = SearchPaths[fidx];
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
  if (lump < 0 || FILE_INDEX(lump) >= SearchPaths.length()) Sys_Error("W_ReadFromLump: %i >= num_wad_files", FILE_INDEX(lump));
  VSearchPath *w = GET_LUMP_FILE(lump);
  w->ReadFromLump(LUMP_INDEX(lump), dest, pos, size);
}


//==========================================================================
//
//  W_CreateLumpReaderNum
//
//==========================================================================
VStream *W_CreateLumpReaderNum (int lump) {
  if (lump < 0 || FILE_INDEX(lump) >= SearchPaths.length()) Sys_Error("W_CreateLumpReaderNum: %i >= num_wad_files", FILE_INDEX(lump));
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
  if (File >= getSPCount()) return -1;
  for (int li = 0; File < getSPCount(); ++File, li = 0) {
    li = SearchPaths[File]->IterateNS(li, NS);
    if (li != -1) return MAKE_HANDLE(File, li);
  }
  return -1;
}


//==========================================================================
//
//  W_IterateNS
//
//==========================================================================
int W_IterateNS (int Prev, EWadNamespace NS) {
  if (Prev < 0) Prev = -1;
  int wi = FILE_INDEX(Prev+1);
  int li = LUMP_INDEX(Prev+1);
  for (; wi < getSPCount(); ++wi, li = 0) {
    li = SearchPaths[wi]->IterateNS(li, NS);
    if (li != -1) return MAKE_HANDLE(wi, li);
  }
  return -1;
}


//==========================================================================
//
//  W_IterateFile
//
//==========================================================================
int W_IterateFile (int Prev, const VStr &Name) {
  if (Name.isEmpty()) return -1;
  //GLog.Logf(NAME_Dev, "W_IterateFile: Prev=%d (%d); fn=<%s>", Prev, getSPCount(), *Name);
  for (int wi = FILE_INDEX(Prev)+1; wi < getSPCount(); ++wi) {
    int li = SearchPaths[wi]->CheckNumForFileName(Name);
    //GLog.Logf(NAME_Dev, "W_IterateFile: wi=%d (%d); fn=<%s>; li=%d", wi, getSPCount(), *Name, li);
    if (li != -1) return MAKE_HANDLE(wi, li);
  }
  return -1;
}


//==========================================================================
//
//  W_FindLumpByFileNameWithExts
//
//==========================================================================
int W_FindLumpByFileNameWithExts (const VStr &BaseName, const char **Exts) {
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
  delete Strm;

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
void W_LoadLumpIntoArrayIdx (int Lump, TArray<vuint8> &Array) {
  VStream *Strm = W_CreateLumpReaderNum(Lump);
  if (!Strm) Sys_Error("error reading lump with index %d", Lump);
  check(Strm);
  Array.SetNum(Strm->TotalSize());
  Strm->Serialise(Array.Ptr(), Strm->TotalSize());
  if (Strm->IsError()) { delete Strm; Sys_Error("error reading lump '%s'", *W_FullLumpName(Lump)); }
  delete Strm;
}


//==========================================================================
//
//  W_LoadLumpIntoArray
//
//==========================================================================
void W_LoadLumpIntoArray (VName LumpName, TArray<vuint8> &Array) {
  int Lump = W_CheckNumForFileName(*LumpName);
  if (Lump < 0) Lump = W_GetNumForName(LumpName);
  VStream *Strm = W_CreateLumpReaderNum(Lump);
  check(Strm);
  Array.SetNum(Strm->TotalSize());
  Strm->Serialise(Array.Ptr(), Strm->TotalSize());
  if (Strm->IsError()) { delete Strm; Sys_Error("error reading lump '%s'", *W_FullLumpName(Lump)); }
  delete Strm;
}


//==========================================================================
//
//  W_Shutdown
//
//==========================================================================
void W_Shutdown () {
  for (int i = SearchPaths.length()-1; i >= 0; --i) {
    delete SearchPaths[i];
    SearchPaths[i] = nullptr;
  }
  SearchPaths.Clear();
}


//==========================================================================
//
//  W_NextMountFileId
//
//==========================================================================
int W_NextMountFileId () {
  return SearchPaths.length();
}


//==========================================================================
//
//  W_FindMapInLastFile
//
//==========================================================================
VStr W_FindMapInLastFile (int fileid, int *mapnum) {
  if (mapnum) *mapnum = -1;
  if (fileid < 0 || fileid >= getSPCount()) return VStr();
  int found = 0xffff;
  bool doom1 = false;
  char doom1ch = 'e';
  VStr longname;
  for (int lump = SearchPaths[fileid]->IterateNS(0, WADNS_Any, true); lump >= 0; lump = SearchPaths[fileid]->IterateNS(lump+1, WADNS_Any, true)) {
    const char *name;
    VName ln = SearchPaths[fileid]->LumpName(lump);
    if (ln != NAME_None) {
      name = *ln;
    } else {
      longname = SearchPaths[fileid]->LumpFileName(lump);
      if (longname.isEmpty()) continue;
      if (!longname.startsWithNoCase("maps/")) continue;
      longname = longname.StripExtension().toLowerCase();
      name = *longname+5;
      if (strchr(name, '/')) continue;
    }
    //GLog.Logf("*** <%s>", name);
    // doom1 (or kdizd)
    if ((name[0] == 'e' || name[0] == 'z') && name[1] && name[2] == 'm' && name[3] && !name[4]) {
      int e = VStr::digitInBase(name[1], 10);
      int m = VStr::digitInBase(name[3], 10);
      if (e < 0 || m < 0) continue;
      if (e >= 1 && e <= 9 && m >= 1 && m <= 9) {
        int n = e*10+m;
        if (!doom1 || n < found) {
          doom1ch = name[0];
          doom1 = true;
          found = n;
          if (mapnum) *mapnum = n;
        }
      }
      continue;
    }
    // doom2
    if (name[0] == 'm' && name[1] == 'a' && name[2] == 'p' && name[3] && name[4] && !name[5]) {
      int m0 = VStr::digitInBase(name[3], 10);
      int m1 = VStr::digitInBase(name[4], 10);
      if (m0 < 0 || m1 < 0) continue;
      int n = m0*10+m1;
      if (n < 1 || n > 32) continue;
      if (doom1 || n < found) {
        doom1 = false;
        found = n;
        if (mapnum) *mapnum = n;
      }
      continue;
    }
  }
  if (found < 0xffff) {
    if (doom1) return VStr(va("%c%dm%d", doom1ch, found/10, found%10));
    return VStr(va("map%02d", found));
  }
  return VStr();
}


//==========================================================================
//
//  W_FindMapInAuxuliaries
//
//==========================================================================
VStr W_FindMapInAuxuliaries (int *mapnum) {
  if (!AuxiliaryIndex) return VStr();
  for (int f = SearchPaths.length()-1; f >= AuxiliaryIndex; --f) {
    VStr mn = W_FindMapInLastFile(f, mapnum);
    //GLog.Logf(NAME_Init, "W_FindMapInAuxuliaries:<%s>: f=%d; ax=%d; mn=%s", *SearchPaths[f]->GetPrefix(), f, AuxiliaryIndex, *mn);
    if (!mn.isEmpty()) return mn;
  }
  return VStr();
}


//==========================================================================
//
//  W_IsIWADLump
//
//==========================================================================
bool W_IsIWADLump (int lump) {
  if (lump < 0) return false;
  int fidx = FILE_INDEX(lump);
  if (fidx < 0 || fidx >= getSPCount()) return false;
  return SearchPaths[fidx]->iwad;
}


//==========================================================================
//
//  W_IsAuxLump
//
//==========================================================================
bool W_IsAuxLump (int lump) {
  if (lump < 0 || !AuxiliaryIndex) return false;
  int fidx = FILE_INDEX(lump);
  return (fidx >= AuxiliaryIndex && fidx < SearchPaths.length());
}
