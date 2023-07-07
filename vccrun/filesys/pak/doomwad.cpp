//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2023 Ketmar Dark
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************
#include "../fsys.h"


// /////////////////////////////////////////////////////////////////////////// /
class VDoomWadFile : public FSysDriverBase {
private:
  struct FileInfo {
    VStr name;
    vuint32 pksize;
    vuint32 pkofs;
  };

private:
  VStream *fileStream; // source stream
  FileInfo *files;
  vint32 fileCount; // total number of files

private:
  bool openArchive ();

protected:
  virtual VStr getNameByIndex (int idx) const override;
  virtual int getNameCount () const override;
  // should return `nullptr` on failure
  virtual VStream *openWithIndex (int idx) override;

public:
  VDoomWadFile (VStream *fstream, VStr aname=VStr("<memory>")); // takes ownership on success
  virtual ~VDoomWadFile () override;

  inline bool isOpened () const { return (fileStream != nullptr); }
};


// /////////////////////////////////////////////////////////////////////////// /
// takes ownership
VDoomWadFile::VDoomWadFile (VStream *fstream, VStr aname)
  : FSysDriverBase()
  , fileStream(nullptr)
  , files(nullptr)
  , fileCount(0)
{
  if (fstream) {
    fileStream = fstream;
    const bool err = (!openArchive() || fstream->IsError());
    if (err) {
      fileStream = nullptr;
      fileCount = 0;
      delete[] fileStream;
      fileStream = nullptr;
    }
  }
}


VDoomWadFile::~VDoomWadFile () {
  delete[] files;
  delete fileStream;
}


VStr VDoomWadFile::getNameByIndex (int idx) const {
  if (idx < 0 || idx >= fileCount) return VStr::EmptyString;
  return files[idx].name;
}


int VDoomWadFile::getNameCount () const {
  return fileCount;
}


bool VDoomWadFile::openArchive () {
  //GLog.Logf(NAME_Debug, "DOOMWAD: checking '%s'...", *fileStream->GetName());

  char sign[4];
  fileStream->Serialize(sign, 4);
  if (fileStream->IsError()) return false;
  if (memcmp(sign, "IWAD", 4) != 0 && memcmp(sign, "PWAD", 4) != 0) return false;

  vuint32 count;
  *fileStream << count;
  if (fileStream->IsError()) return false;
  if (count == 0) return true;

  vuint32 dirofs;
  *fileStream << dirofs;
  if (fileStream->IsError()) return false;

  fileStream->Seek(dirofs);
  if (fileStream->IsError()) return false;

  //GLog.Logf(NAME_Debug, "DOOMWAD: count=%u; dirofs=0x%08x", count, dirofs);

  files = new FileInfo[count];

  fileCount = 0;
  while (count--) {
    vuint32 ofs, size;
    char name[9];
    *fileStream << ofs;
    if (fileStream->IsError()) return false;
    *fileStream << size;
    if (fileStream->IsError()) return false;
    fileStream->Serialize(name, 8);
    if (fileStream->IsError()) return false;
    name[8] = 0;
    if (name[0]) {
      if (ofs >= 0x1fffffffu || size >= 0x1fffffffu) return false;
      for (char *s = name; *s; ++s) if (*s >= 'A' && *s <= 'Z') s[0] += 32; // poor man's tolower
      files[fileCount].name = VStr(name);
      files[fileCount].pksize = size;
      files[fileCount].pkofs = ofs;
      ++fileCount;
    }
  }

  buildNameHashTable();

  return true;
}


VStream *VDoomWadFile::openWithIndex (int idx) {
  if (idx < 0 || idx >= fileCount) return nullptr;
  fileStream->Seek(files[idx].pkofs);
  return new VPartialStreamRO(fileStream, (int)files[idx].pkofs, (int)files[idx].pksize);
}


// ////////////////////////////////////////////////////////////////////////// //
static FSysDriverBase *doomwadLoader (VStream *strm) {
  if (!strm) return nullptr;
  auto res = new VDoomWadFile(strm);
  if (!res->isOpened()) { delete res; res = nullptr; }
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
void fsys_Register_DOOMWAD () {
  FSysRegisterDriver(&doomwadLoader, "pwad", 6696);
}
