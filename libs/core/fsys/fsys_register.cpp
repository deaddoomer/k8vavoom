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

// ////////////////////////////////////////////////////////////////////////// //
// i have to do this, otherwise the linker will optimise openers away
/*
#include "formats/fsys_wad.cpp"
#include "formats/fsys_zip.cpp"
#include "formats/fsys_qkpak.cpp"
*/


// ////////////////////////////////////////////////////////////////////////// //
static VSearchPath *openArchiveWAD (VStream *strm, VStr filename, bool FixVoices) {
  if (strm->TotalSize() < 12) return nullptr;
  strm->Seek(0);
  if (strm->IsError()) return nullptr;
  return VWadFile::Create(filename, FixVoices, strm);
}


// ////////////////////////////////////////////////////////////////////////// //
class FSys_Internal_Init_Readers {
public:
  FSys_Internal_Init_Readers (bool) {
    // check for 7z and other faulty archive formats first
    // 7z
    static __attribute__((used)) FArchiveReaderInfo vavoom_fsys_archive_opener_7z("7z",
      [](VStream * /*strm*/, VStr filename, bool /*FixVoices*/) -> VSearchPath* {
        Sys_Error("7z archives aren't supported, so '%s' cannot be opened!", *filename);
        return nullptr;
      }, "7z\xbc\xaf\x27\x1c", -666);
    // rar
    static __attribute__((used)) FArchiveReaderInfo vavoom_fsys_archive_opener_rar("rar",
      [](VStream * /*strm*/, VStr filename, bool /*FixVoices*/) -> VSearchPath* {
        Sys_Error("Rar archives aren't supported, so '%s' cannot be opened!", *filename);
        return nullptr;
      }, "Rar!", -666);
    // ar
    static __attribute__((used)) FArchiveReaderInfo vavoom_fsys_archive_opener_ar("ar",
      [](VStream * /*strm*/, VStr filename, bool /*FixVoices*/) -> VSearchPath* {
        Sys_Error("ar archives aren't supported, so '%s' cannot be opened!", *filename);
        return nullptr;
      }, "!<arch>\x0a", -666);
    // tar
    static __attribute__((used)) FArchiveReaderInfo vavoom_fsys_archive_opener_tar("tar",
      [](VStream *strm, VStr filename, bool /*FixVoices*/) -> VSearchPath* {
        if (strm->TotalSize() < 257+16) return nullptr;
        char sign[5];
        strm->Seek(257);
        if (strm->IsError()) return nullptr;
        strm->Serialize(sign, 5);
        if (strm->IsError()) return nullptr;
        if (memcmp(sign, "ustar", 5) != 0) return nullptr;
        Sys_Error("tar archives aren't supported, so '%s' cannot be opened!", *filename);
        return nullptr;
      }, nullptr, -666);
    // gzip
    static __attribute__((used)) FArchiveReaderInfo vavoom_fsys_archive_opener_gzip("gzip",
      [](VStream *strm, VStr filename, bool /*FixVoices*/) -> VSearchPath* {
        if (strm->TotalSize() < 10) return nullptr;
        strm->Seek(3);
        if (strm->IsError()) return nullptr;
        vuint8 flags;
        *strm << flags;
        if (flags&0xe0) return nullptr; // reserved flags must be zero
        Sys_Error("gzip archives aren't supported, so '%s' cannot be opened!", *filename);
        return nullptr;
      }, "\x1f\x8b\x08", -666);
    // dfwad
    static __attribute__((used)) FArchiveReaderInfo vavoom_fsys_archive_opener_dfwad("dfwad",
      [](VStream * /*strm*/, VStr filename, bool /*FixVoices*/) -> VSearchPath* {
        Sys_Error("DFWAD archives aren't supported, so '%s' cannot be opened!", *filename);
        return nullptr;
      }, "DFWAD\x01", -666);

    // WAD
    static __attribute__((used)) FArchiveReaderInfo vavoom_fsys_archive_opener_iwad("iwad", &openArchiveWAD, "IWAD");
    static __attribute__((used)) FArchiveReaderInfo vavoom_fsys_archive_opener_pwad("pwad", &openArchiveWAD, "PWAD");

    // PAK
    static __attribute__((used)) FArchiveReaderInfo vavoom_fsys_archive_opener_pak("pak",
      [](VStream *strm, VStr filename, bool /*FixVoices*/) -> VSearchPath* {
        if (strm->TotalSize() < 12) return nullptr;
        strm->Seek(0);
        if (strm->IsError()) return nullptr;
        return new VQuakePakFile(strm, filename, 1);
      }, "PACK");
    static __attribute__((used)) FArchiveReaderInfo vavoom_fsys_archive_opener_sin("sin",
      [](VStream *strm, VStr filename, bool /*FixVoices*/) -> VSearchPath* {
        if (strm->TotalSize() < 12) return nullptr;
        strm->Seek(0);
        if (strm->IsError()) return nullptr;
        return new VQuakePakFile(strm, filename, 2);
      }, "SPAK");

    // VWAD
    static __attribute__((used)) FArchiveReaderInfo vavoom_fsys_archive_opener_vwad("vwad",
      [](VStream *strm, VStr filename, bool /*FixVoices*/) -> VSearchPath* {
        if (strm->TotalSize() <= 4*3+32+64) return nullptr;
        strm->Seek(0);
        if (strm->IsError()) return nullptr;
        return new VVWadFile(strm, filename);
      }, "VWAD");

    // DFWAD
    /*
    static __attribute__((used)) FArchiveReaderInfo vavoom_fsys_archive_opener_dfwad("dfwad",
      [](VStream *strm, VStr filename, bool FixVoices) -> VSearchPath* {
        if (strm->TotalSize() < 8) return nullptr;
        strm->Seek(0);
        if (strm->IsError()) return nullptr;
        return new VDFWadFile(strm, filename);
      }, "DFWAD\x01");
    */

    // ZIP
    // checking for this is slow, but the sorter will check it last, as it has no signature
    // still, give it lower priority in case we'll have other signature-less formats
    static __attribute__((used)) FArchiveReaderInfo vavoom_fsys_archive_opener_zip("zip",
      [](VStream *strm, VStr filename, bool /*FixVoices*/) -> VSearchPath* {
        if (strm->TotalSize() < 16) return nullptr;
        vuint32 cdofs = VZipFile::SearchCentralDir(strm);
        if (cdofs == 0) return nullptr;
        return new VZipFile(strm, filename, cdofs);
      }, nullptr, 999);
  }
};

__attribute__((used)) FSys_Internal_Init_Readers fsys_internal_init_readers_variable(true);
