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
//**  Based on sources from zlib with following notice:
//**
//**  Copyright (C) 1998-2004 Gilles Vollant
//**
//**  This software is provided 'as-is', without any express or implied
//**  warranty.  In no event will the authors be held liable for any damages
//**  arising from the use of this software.
//**
//**  Permission is granted to anyone to use this software for any purpose,
//**  including commercial applications, and to alter it and redistribute it
//**  freely, subject to the following restrictions:
//**
//**  1. The origin of this software must not be misrepresented; you must
//**  not claim that you wrote the original software. If you use this
//**  software in a product, an acknowledgment in the product documentation
//**  would be appreciated but is not required.
//**  2. Altered source versions must be plainly marked as such, and must
//**  not be misrepresented as being the original software.
//**  3. This notice may not be removed or altered from any source
//**  distribution.
//**
//**************************************************************************
#include "../fsys_local.h"
#ifdef _WIN32
# include <time.h>
#endif

#ifdef VAVOOM_USE_LIBLZMA
# error "using liblzma is deprecated"
#endif

#define Z_LZMA  (14)
#define Z_STORE  (0)

//#define K8_UNLZMA_DEBUG


enum {
  SIZECENTRALDIRITEM = 0x2e,
  SIZEZIPLOCALHEADER = 0x1e,
};


static const char *moreresdirs[] = {
  "models/",
  "filter/",
  nullptr,
};


#include "fsys_zipfread.cpp"


//==========================================================================
//
//  VZipFile::VZipFile
//
//  takes ownership
//
//==========================================================================
VZipFile::VZipFile (VStream *fstream, VStr name, vuint32 cdofs)
  : VPakFileBase(name)
{
  OpenArchive(fstream, cdofs);
  if (fstream->IsError()) Sys_Error("error opening archive \"%s\"", *PakFileName);
}


//==========================================================================
//
//  VZipFile::VZipFile
//
//==========================================================================
void VZipFile::OpenArchive (VStream *fstream, vuint32 cdofs) {
  archStream = fstream;
  vassert(archStream);

  vuint32 central_pos = (cdofs ? cdofs : SearchCentralDir(archStream));
  if (central_pos == 0 || (vint32)central_pos == -1) {
    // check for 7zip idiocity
    if (!fstream->IsError() && fstream->TotalSize() >= 2) {
      char hdr[2] = {0};
      fstream->Seek(0);
      fstream->Serialise(hdr, 2);
      if (memcmp(hdr, "7z", 2) == 0) Sys_Error("DO NOT RENAME YOUR 7Z ARCHIVES TO PK3, THIS IS IDIOCITY! REJECTED \"%s\"", *PakFileName);
    }
    Sys_Error("cannot load zip/pk3 file \"%s\"", *PakFileName);
  }
  //vassert(central_pos);

  archStream->Seek(central_pos);

  vuint32 Signature;
  vuint16 number_disk; // number of the current dist, used for spaning ZIP
  vuint16 number_disk_with_CD; // number the the disk with central dir, used for spaning ZIP
  vuint16 number_entry_CD; // total number of entries in the central dir (same than number_entry on nospan)
  vuint16 size_comment; // size of the global comment of the zipfile
  vuint16 NumFiles;

  *archStream
    // the signature, already checked
    << Signature
    // number of this disk
    << number_disk
    // number of the disk with the start of the central directory
    << number_disk_with_CD
    // total number of entries in the central dir on this disk
    << NumFiles
    // total number of entries in the central dir
    << number_entry_CD;

  vassert(number_entry_CD == NumFiles);
  vassert(number_disk_with_CD == 0);
  vassert(number_disk == 0);

  vuint32 size_central_dir; // size of the central directory
  vuint32 offset_central_dir; // offset of start of central directory with respect to the starting disk number

  *archStream
    << size_central_dir
    << offset_central_dir
    << size_comment;

  vassert(central_pos >= offset_central_dir+size_central_dir);

  BytesBeforeZipFile = central_pos-(offset_central_dir+size_central_dir);

  const bool isPK3 =
    PakFileName.endsWithCI(".pk3") ||
    PakFileName.endsWithCI(".ipk3") ||
    PakFileName.endsWithCI(".pk7") ||
    PakFileName.endsWithCI(".ipk7") ||
    PakFileName.endsWithCI(".vwad");
  type = (isPK3 ? PAK : OTHER);
  bool canHasPrefix = true;
  if (isPK3 || (fsys_simple_archives == FSYS_ARCHIVES_SIMPLE_KEEP_PREFIX)) canHasPrefix = false; // do not remove prefixes in pk3
  //GLog.Logf("*** ARK: <%s>:<%s> pfx=%d", *PakFileName, *PakFileName.ExtractFileExtension(), (int)canHasPrefix);

  // set the current file of the zipfile to the first file
  vuint32 pos_in_central_dir = offset_central_dir;
  for (int i = 0; i < NumFiles; ++i) {
    //VPakFileInfo &file_info = files[i];
    VPakFileInfo file_info;

    archStream->Seek(pos_in_central_dir+BytesBeforeZipFile);

    vuint32 Magic;
    vuint16 version; // version made by
    vuint16 version_needed; // version needed to extract
    vuint16 dosTime; // last mod file date in Dos fmt
    vuint16 dosDate; // last mod file date in Dos fmt
    vuint16 size_file_extra; // extra field length
    vuint16 size_file_comment; // file comment length
    vuint16 disk_num_start; // disk number start
    vuint16 internal_fa; // internal file attributes
    vuint32 external_fa; // external file attributes

    // we check the magic
    *archStream
      << Magic
      << version
      << version_needed
      << file_info.flag
      << file_info.compression
      << dosTime
      << dosDate
      << file_info.crc32
      << file_info.packedsize
      << file_info.filesize
      << file_info.filenamesize
      << size_file_extra
      << size_file_comment
      << disk_num_start
      << internal_fa
      << external_fa
      << file_info.pakdataofs;

    if (Magic != 0x02014b50) Sys_Error("corrupted ZIP file \"%s\"", *fstream->GetName());

    char *filename_inzip = new char[file_info.filenamesize+1];
    filename_inzip[file_info.filenamesize] = '\0';
    archStream->Serialise(filename_inzip, file_info.filenamesize);
    VStr zfname = VStr(filename_inzip).ToLower().FixFileSlashes();
    VStr xxfname;

    // hack for "pk3tovwad"
    if (fsys_simple_archives != FSYS_ARCHIVES_NORMAL) {
      xxfname = VStr(filename_inzip).FixFileSlashes();
    } else {
      xxfname.Clear();
    }

    delete[] filename_inzip;
    filename_inzip = nullptr;

    bool addIt = (!zfname.isEmpty() && !zfname.endsWith("/"));

    // ignore encrypted files
    if (addIt && (file_info.flag&((1u<<0)|(1u<<6))) != 0) addIt = false;

    if (addIt) {
      // fix some idiocity introduced by some shitdoze doom tools
      for (;;) {
             if (zfname.startsWith("./")) zfname.chopLeft(2);
        else if (zfname.startsWith("../")) zfname.chopLeft(3);
        else if (zfname.startsWith("/")) zfname.chopLeft(1);
        else break;
      }
      addIt = (!zfname.isEmpty() && !zfname.endsWith("/"));
    }

    if (addIt) {
      file_info.SetFileName(zfname);
      if (fsys_simple_archives != FSYS_ARCHIVES_NORMAL) {
        // fix some idiocity introduced by some shitdoze doom tools
        for (;;) {
               if (xxfname.startsWith("./")) xxfname.chopLeft(2);
          else if (xxfname.startsWith("../")) xxfname.chopLeft(3);
          else if (xxfname.startsWith("/")) xxfname.chopLeft(1);
          else break;
        }
        file_info.SetDiskName(xxfname);

        struct tm xtime;
        memset((void *)&xtime, 0, sizeof(xtime));
        xtime.tm_year = ((dosDate >> 9) & 0x7f) + 1980 - 1900;
        xtime.tm_mon = (dosDate >> 5) & 0x0f;
        if (xtime.tm_mon < 1) xtime.tm_mon = 1; else if (xtime.tm_mon > 12) xtime.tm_mon = 12;
        xtime.tm_mon -= 1;
        xtime.tm_mday = dosDate & 0x01f;
        if (xtime.tm_mday < 1) xtime.tm_mday = 1; else if (xtime.tm_mday > 31) xtime.tm_mday = 31;

        xtime.tm_sec = (dosTime & 0x1f) * 2;
        if (xtime.tm_sec > 59) xtime.tm_sec = 59;
        xtime.tm_min = (dosTime >> 5) & 0x3f;
        if (xtime.tm_min > 59) xtime.tm_min = 59;
        xtime.tm_hour = (dosTime >> 11) & 0x1f;
        if (xtime.tm_hour > 23) xtime.tm_hour = 23;

        uint64_t ftime = mktime(&xtime);
        file_info.filetime = ftime;
      }

      if (canHasPrefix && file_info.fileNameIntr.IndexOf('/') == -1) canHasPrefix = false;

      // ignore "dehacked.txt" included in some idarchive zips with "dehacked.exe"
      if (isPK3 || !zfname.strEquCI("dehacked.txt")) {
        if (canHasPrefix) {
          for (const VPK3ResDirInfo *di = PK3ResourceDirs; di->pfx; ++di) {
            if (file_info.fileNameIntr.StartsWith(di->pfx)) { canHasPrefix = false; break; }
          }
          if (canHasPrefix) {
            for (const char **dn = moreresdirs; *dn; ++dn) {
              if (file_info.fileNameIntr.StartsWith(*dn)) { canHasPrefix = false; break; }
            }
          }
        }

        file_info.SetFastSeek(file_info.compression == Z_STORE);
        pakdir.append(file_info);
      }
    }

    // set the current file of the zipfile to the next file
    pos_in_central_dir += SIZECENTRALDIRITEM+file_info.filenamesize+size_file_extra+size_file_comment;
  }

  // find and remove common prefix
  if (canHasPrefix && pakdir.files.length() > 0) {
    if (fsys_simple_archives == FSYS_ARCHIVES_NORMAL) {
      VStr xpfx = pakdir.files[0].fileNameIntr;
      const int sli = xpfx.IndexOf('/');
      if (sli > 0 && !xpfx.startsWithCI("filter/")) {
        xpfx = VStr(xpfx, 0, sli+1); // extract prefix
        for (int i = 0; i < pakdir.files.length(); ++i) {
          if (!pakdir.files[i].fileNameIntr.StartsWith(xpfx)) { canHasPrefix = false; break; }
        }
        if (canHasPrefix) {
          // remove prefix
          //GLog.Logf("*** ARK: <%s>:<%s> pfx=<%s>", *PakFileName, *PakFileName.ExtractFileExtension(), *xpfx);
          for (int i = 0; i < pakdir.files.length(); ++i) {
            pakdir.files[i].SetFileName(VStr(pakdir.files[i].fileNameIntr, sli+1, pakdir.files[i].fileNameIntr.length()-sli-1));
            //printf("new: <%s>\n", *Files[i].Name);
          }
        }
      }
    } else if (fsys_simple_archives == FSYS_ARCHIVES_SIMPLE) {
      VStr xpfx = pakdir.files[0].diskNameIntr;
      const int sli = xpfx.IndexOf('/');
      if (sli > 0 && !xpfx.startsWithCI("filter/")) {
        xpfx = VStr(xpfx, 0, sli+1); // extract prefix
        for (int i = 0; i < pakdir.files.length(); ++i) {
          if (!pakdir.files[i].diskNameIntr.StartsWith(xpfx)) { canHasPrefix = false; break; }
        }
        if (canHasPrefix) {
          // remove prefix
          //GLog.Logf("*** ARK: <%s>:<%s> pfx=<%s>", *PakFileName, *PakFileName.ExtractFileExtension(), *xpfx);
          for (int i = 0; i < pakdir.files.length(); ++i) {
            VStr fn = VStr(pakdir.files[i].diskNameIntr, sli+1, pakdir.files[i].diskNameIntr.length()-sli-1);
            pakdir.files[i].SetDiskName(fn);
            pakdir.files[i].SetFileName(fn);
            //printf("new: <%s>\n", *Files[i].Name);
          }
        }
      }
    }
  }

  // hack for "pk3tovwad"
  if (fsys_simple_archives != FSYS_ARCHIVES_NORMAL) type = PAK;

  pakdir.buildLumpNames();
  pakdir.buildNameMaps(false, this);
}


//==========================================================================
//
//  SelfDestructBuf
//
//==========================================================================
struct SelfDestructBuf {
  vuint8 *buf;

  SelfDestructBuf (vint32 sz) {
    buf = (vuint8 *)Z_Malloc(sz);
    if (!buf) Sys_Error("Out of memory!");
  }

  ~SelfDestructBuf () { Z_Free(buf); }
};


//==========================================================================
//
//  VZipFile::SearchCentralDir
//
//  locate the Central directory of a zipfile
//  (at the end, just before the global comment)
//
//==========================================================================
vuint32 VZipFile::SearchCentralDir (VStream *strm) {
  enum { MaxBufSize = 65578 };

  vint32 fsize = strm->TotalSize();
  if (fsize < 16) return 0;

  vuint32 rd = (fsize < MaxBufSize ? fsize : MaxBufSize);
  SelfDestructBuf sdbuf(rd);

  strm->Seek(fsize-rd);
  if (strm->IsError()) return 0;
  strm->Serialise(sdbuf.buf, rd);
  if (strm->IsError()) return 0;

  for (int f = rd-8; f >= 0; --f) {
    if (sdbuf.buf[f] == 0x50 && sdbuf.buf[f+1] == 0x4b && sdbuf.buf[f+2] == 0x05 && sdbuf.buf[f+3] == 0x06) {
      // i found her!
      return fsize-rd+f;
    }
  }

  return 0;
}


//==========================================================================
//
//  VZipFile::CreateLumpReaderNum
//
//==========================================================================
VStream *VZipFile::CreateLumpReaderNum (int Lump) {
  vassert(Lump >= 0);
  vassert(Lump < pakdir.files.length());
  return new VZipFileReader(PakFileName+":"+pakdir.files[Lump].fileNameIntr, archStream, BytesBeforeZipFile, pakdir.files[Lump], &rdlock);
}
