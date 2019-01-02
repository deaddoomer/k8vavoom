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
#include "gamedefs.h"
#include "fs_local.h"


#define GET_LUMP_FILE(num)    SearchPaths[(num)>>16]
#define FILE_INDEX(num)       ((num)>>16)
#define LUMP_INDEX(num)       ((num)&0xffff)
#define MAKE_HANDLE(wi, num)  (((wi)<<16)+(num))


// ////////////////////////////////////////////////////////////////////////// //
extern TArray<VStr> wadfiles;

static int AuxiliaryIndex;

static TMap<VStr, int> fullNameTexLumpChecked;


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
void W_AddFile (const VStr &FileName, const VStr &GwaDir, bool FixVoices) {
  guard(W_AddFile);
  int wadtime;

  wadtime = Sys_FileTime(FileName);
  if (wadtime == -1) Sys_Error("Required file %s doesn't exist", *FileName);

  wadfiles.Append(FileName);

  VStr ext = FileName.ExtractFileExtension().ToLower();
  VWadFile *Wad = new VWadFile;
  if (ext != "wad" && ext != "gwa") {
    Wad->OpenSingleLump(FileName);
  } else {
    Wad->Open(FileName, GwaDir, FixVoices, nullptr);
  }
  SearchPaths.Append(Wad);

  if (ext == "wad") {
    VStr gl_name;

    bool FoundGwa = false;
    if (GwaDir.IsNotEmpty()) {
      gl_name = GwaDir+"/"+FileName.ExtractFileName().StripExtension()+".gwa";
      if (Sys_FileTime(gl_name) >= wadtime) {
        W_AddFile(gl_name, VStr(), false);
        FoundGwa = true;
      }
    }

    if (!FoundGwa) {
      gl_name = FileName.StripExtension()+".gwa";
      if (Sys_FileTime(gl_name) >= wadtime) {
        W_AddFile(gl_name, VStr(), false);
      } else {
        // leave empty slot for GWA file
        SearchPaths.Append(new VWadFile);
      }
    }
  }
  unguard;
}


//==========================================================================
//
//  W_AddFileFromZip
//
//==========================================================================
void W_AddFileFromZip (const VStr &WadName, VStream *WadStrm, const VStr &GwaName, VStream *GwaStrm) {
  guard(W_AddFileFromZip);
  // add WAD file
  wadfiles.Append(WadName);
  VWadFile *Wad = new VWadFile;
  Wad->Open(WadName, VStr(), false, WadStrm);
  SearchPaths.Append(Wad);
  if (GwaStrm) {
    // add GWA file
    wadfiles.Append(GwaName);
    VWadFile *Gwa = new VWadFile;
    Gwa->Open(GwaName, VStr(), false, GwaStrm);
    SearchPaths.Append(Gwa);
  } else {
    // leave empty slot for GWA file
    SearchPaths.Append(new VWadFile);
  }
  unguard;
}


//==========================================================================
//
//  W_OpenAuxiliary
//
//==========================================================================
int W_OpenAuxiliary (const VStr &FileName) {
  guard(W_OpenAuxiliary);
  W_CloseAuxiliary();
  AuxiliaryIndex = SearchPaths.length();
  VStr GwaName = FileName.StripExtension()+".gwa";
  VStream *WadStrm = FL_OpenFileRead(FileName);
  VStream *GwaStrm = FL_OpenFileRead(GwaName);
  W_AddFileFromZip(FileName, WadStrm, GwaName, GwaStrm);
  return MAKE_HANDLE(AuxiliaryIndex, 0);
  unguard;
}


//==========================================================================
//
//  W_CloseAuxiliary
//
//==========================================================================
void W_CloseAuxiliary () {
  guard(W_CloseAuxiliary);
  if (AuxiliaryIndex) {
    SearchPaths[AuxiliaryIndex]->Close();
    SearchPaths[AuxiliaryIndex+1]->Close();
    delete SearchPaths[AuxiliaryIndex];
    SearchPaths[AuxiliaryIndex] = nullptr;
    delete SearchPaths[AuxiliaryIndex+1];
    SearchPaths[AuxiliaryIndex+1] = nullptr;
    SearchPaths.SetNum(AuxiliaryIndex);
    AuxiliaryIndex = 0;
  }
  unguard;
}


//==========================================================================
//
//  W_CheckNumForName
//
//  Returns -1 if name not found.
//
//==========================================================================
int W_CheckNumForName (VName Name, EWadNamespace NS) {
  guard(W_CheckNumForName);

  for (int wi = SearchPaths.length()-1; wi >= 0; --wi) {
    int i = SearchPaths[wi]->CheckNumForName(Name, NS);
    if (i >= 0) return MAKE_HANDLE(wi, i);
  }

  /*
  // k8: try "name.lmp"
  VStr xname = VStr(*Name)+".lmp";
  for (int wi = SearchPaths.length()-1; wi >= 0; --wi) {
    int i = SearchPaths[wi]->CheckNumForFileName(xname);
    if (i >= 0) return MAKE_HANDLE(wi, i);
  }
  */

  // not found
  return -1;
  unguard;
}


//==========================================================================
//
//  W_GetNumForName
//
//  Calls W_CheckNumForName, but bombs out if not found.
//
//==========================================================================
int W_GetNumForName (VName Name, EWadNamespace NS) {
  guard(W_GetNumForName);
  check(Name != NAME_None);
  int i = W_CheckNumForName(Name, NS);
  if (i == -1) Sys_Error("W_GetNumForName: \"%s\" not found!", *Name);
  return i;
  unguard;
}


//==========================================================================
//
//  W_CheckNumForNameInFile
//
//  Returns -1 if name not found.
//
//==========================================================================
int W_CheckNumForNameInFile (VName Name, int File, EWadNamespace NS) {
  guard(W_CheckNumForNameInFile);
  if (File < 0 || File >= SearchPaths.length()) return -1;
  int i = SearchPaths[File]->CheckNumForName(Name, NS);
  if (i >= 0) return MAKE_HANDLE(File, i);
  // not found
  return -1;
  unguard;
}


//==========================================================================
//
//  W_CheckNumForFileName
//
//  Returns -1 if name not found.
//
//==========================================================================
int W_CheckNumForFileName (const VStr &Name) {
  guard(W_CheckNumForFileName);
  for (int wi = SearchPaths.length()-1; wi >= 0; --wi) {
    int i = SearchPaths[wi]->CheckNumForFileName(Name);
    if (i >= 0) return MAKE_HANDLE(wi, i);
  }
  // not found
  return -1;
  unguard;
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
  if (fidx < 0 || fidx >= SearchPaths.length()) return -1;
  VSearchPath *w = GET_LUMP_FILE(filelump);
  int i = w->CheckNumForFileName(Name);
  if (i >= 0) return MAKE_HANDLE(fidx, i);
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
  for (int wi = SearchPaths.length()-1; wi >= 0; --wi) {
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
  guard(W_CheckNumForTextureFileName);

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
  unguard;
}


//==========================================================================
//
//  W_GetNumForFileName
//
//  Calls W_CheckNumForFileName, but bombs out if not found.
//
//==========================================================================
int W_GetNumForFileName (const VStr &Name) {
  guard(W_GetNumForFileName);
  int i = W_CheckNumForFileName(Name);
  if (i == -1) Sys_Error("W_GetNumForFileName: %s not found!", *Name);
  return i;
  unguard;
}


//==========================================================================
//
//  W_LumpLength
//
//  Returns the buffer size needed to load the given lump.
//
//==========================================================================
int W_LumpLength (int lump) {
  guard(W_LumpLength);
  if (FILE_INDEX(lump) >= SearchPaths.length()) Sys_Error("W_LumpLength: %i >= num_wad_files", FILE_INDEX(lump));
  VSearchPath *w = GET_LUMP_FILE(lump);
  int lumpindex = LUMP_INDEX(lump);
  return w->LumpLength(lumpindex);
  unguard;
}


//==========================================================================
//
//  W_LumpName
//
//==========================================================================
VName W_LumpName (int lump) {
  guard(W_LumpName);
  if (FILE_INDEX(lump) >= SearchPaths.length()) return NAME_None;
  VSearchPath *w = GET_LUMP_FILE(lump);
  int lumpindex = LUMP_INDEX(lump);
  return w->LumpName(lumpindex);
  unguard;
}


//==========================================================================
//
//  W_FullLumpName
//
//==========================================================================
VStr W_FullLumpName (int lump) {
  guard(W_FullLumpName);
  if (FILE_INDEX(lump) >= SearchPaths.length()) return VStr("<invalid>");
  VSearchPath *w = GET_LUMP_FILE(lump);
  int lumpindex = LUMP_INDEX(lump);
  //return w->GetPrefix()+":"+*(w->LumpName(lumpindex));
  return w->GetPrefix()+":"+*(w->LumpFileName(lumpindex));
  unguard;
}


//==========================================================================
//
//  W_LumpFile
//
//  Returns file index of the given lump.
//
//==========================================================================
int W_LumpFile (int lump) {
  return FILE_INDEX(lump);
}


//==========================================================================
//
//  W_ReadFromLump
//
//==========================================================================
void W_ReadFromLump (int lump, void *dest, int pos, int size) {
  guard(W_ReadFromLump);
  if (FILE_INDEX(lump) >= SearchPaths.length()) Sys_Error("W_ReadFromLump: %i >= num_wad_files", FILE_INDEX(lump));
  VSearchPath *w = GET_LUMP_FILE(lump);
  w->ReadFromLump(LUMP_INDEX(lump), dest, pos, size);
  unguard;
}


//==========================================================================
//
//  W_CreateLumpReaderNum
//
//==========================================================================
VStream *W_CreateLumpReaderNum (int lump) {
  guard(W_CreateLumpReaderNum);
  return GET_LUMP_FILE(lump)->CreateLumpReaderNum(LUMP_INDEX(lump));
  unguard;
}


//==========================================================================
//
//  W_CreateLumpReaderName
//
//==========================================================================
VStream *W_CreateLumpReaderName (VName Name, EWadNamespace NS) {
  guard(W_CreateLumpReaderName);
  return W_CreateLumpReaderNum(W_GetNumForName(Name, NS));
  unguard;
}


//==========================================================================
//
//  W_IterateNS
//
//==========================================================================
int W_IterateNS (int Prev, EWadNamespace NS) {
  guard(W_IterateNS);
  if (Prev < 0) Prev = -1;
  int wi = FILE_INDEX(Prev+1);
  int li = LUMP_INDEX(Prev+1);
  for (; wi < SearchPaths.length(); ++wi, li = 0) {
    li = SearchPaths[wi]->IterateNS(li, NS);
    if (li != -1) return MAKE_HANDLE(wi, li);
  }
  return -1;
  unguard;
}


//==========================================================================
//
//  W_IterateFile
//
//==========================================================================
int W_IterateFile (int Prev, const VStr &Name) {
  guard(W_IterateFile);
  for (int wi = FILE_INDEX(Prev)+1; wi < SearchPaths.length(); ++wi) {
    int li = SearchPaths[wi]->CheckNumForFileName(Name);
    if (li != -1) return MAKE_HANDLE(wi, li);
  }
  return -1;
  unguard;
}


//==========================================================================
//
//  W_FindLumpByFileNameWithExts
//
//==========================================================================
int W_FindLumpByFileNameWithExts (const VStr &BaseName, const char **Exts) {
  guard(W_FindLumpByFileNameWithExts);
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
  unguard;
}


//==========================================================================
//
//  W_LoadTextLump
//
//==========================================================================
VStr W_LoadTextLump (VName name) {
  guard(W_LoadTextLump);
  VStream *Strm = W_CreateLumpReaderName(name);
  int msgSize = Strm->TotalSize();
  char *buf = new char[msgSize + 1];
  Strm->Serialise(buf, msgSize);
  delete Strm;
  Strm = nullptr;
  buf[msgSize] = 0; // append terminator
  VStr Ret = buf;
  delete[] buf;
  buf = nullptr;
  if (!Ret.IsValidUtf8()) {
    GCon->Logf("%s is not a valid UTF-8 text lump, assuming Latin 1", *name);
    Ret = Ret.Latin1ToUtf8();
  }
  return Ret;
  unguard;
}


//==========================================================================
//
//  W_CreateLumpReaderNum
//
//==========================================================================
void W_LoadLumpIntoArray (VName LumpName, TArray<vuint8> &Array) {
  int Lump = W_CheckNumForFileName(*LumpName);
  if (Lump < 0) Lump = W_GetNumForName(LumpName);
  VStream *Strm = W_CreateLumpReaderNum(Lump);
  check(Strm);
  Array.SetNum(Strm->TotalSize());
  Strm->Serialise(Array.Ptr(), Strm->TotalSize());
  delete Strm;
  Strm = nullptr;
}


//==========================================================================
//
//  W_Shutdown
//
//==========================================================================
void W_Shutdown () {
  guard(W_Shutdown);
  for (int i = SearchPaths.length()-1; i >= 0; --i) {
    delete SearchPaths[i];
    SearchPaths[i] = nullptr;
  }
  SearchPaths.Clear();
  unguard;
}


//==========================================================================
//
//  W_FindMapInLastFile
//
//==========================================================================
int W_NextMoundFileId () {
  return SearchPaths.length();
}


//==========================================================================
//
//  W_FindMapInLastFile
//
//==========================================================================
VStr W_FindMapInLastFile (int fileid, int *mapnum) {
  if (mapnum) *mapnum = -1;
  if (fileid < 0 || fileid >= SearchPaths.length()) return VStr();
  int found = 0xffff;
  bool doom1 = false;
  for (int lump = SearchPaths[fileid]->IterateNS(0, WADNS_Global); lump >= 0; lump = SearchPaths[fileid]->IterateNS(lump+1, WADNS_Global)) {
    VName ln = SearchPaths[fileid]->LumpName(lump);
    if (ln == NAME_None) continue;
    const char *name = *ln;
    //GCon->Logf("*** <%s>", name);
    // doom1
    if (name[0] == 'e' && name[1] && name[2] == 'm' && name[3] && !name[4]) {
      int e = VStr::digitInBase(name[1], 10);
      int m = VStr::digitInBase(name[3], 10);
      if (e < 0 || m < 0) continue;
      if (e >= 1 && e <= 4 && m >= 1 && m <= 9) {
        int n = e*10+m;
        if (!doom1 || n < found) {
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
    if (doom1) return VStr(va("e%dm%d", found/10, found%10));
    return VStr(va("map%02d", found));
  }
  return VStr();
}
