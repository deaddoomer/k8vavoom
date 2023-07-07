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
#include "../fsys_local.h"

#include "vwadvfs.h"


static const uint8_t k8PubKey[32] = {
  0x46,0x0D,0x08,0x92,0x32,0x3A,0xFC,0x41,
  0xDD,0x7D,0x4F,0x5B,0x12,0xCA,0x9E,0x6C,
  0x6F,0x54,0x7D,0x45,0x3F,0xC1,0x5C,0x6A,
  0x80,0xCA,0xED,0x0D,0x17,0x37,0x0F,0xA6,
};


static void *xxalloc (vwad_memman *mman, uint32_t size) {
  return Z_Malloc(size);
}

static void xxfree (vwad_memman *mman, void *p) {
  return Z_Free(p);
}

static vwad_memman memman = {
  .alloc = &xxalloc,
  .free = &xxfree,
};


static vwad_result ioseek (vwad_iostream *strm, int pos) {
  VStream *s = (VStream *)strm->udata;
  s->Seek(pos);
  if (s->IsError()) return -1;
  return 0;
}


static vwad_result ioread (vwad_iostream *strm, void *buf, int bufsize) {
  VStream *s = (VStream *)strm->udata;
  s->Serialise(buf, bufsize);
  if (s->IsError()) return -1;
  return 0;
}


static void logger (int type, const char *fmt, ...) {
  va_list ap;
  switch (type) {
    case VWAD_LOG_NOTE:
      #if 1
      va_start(ap, fmt);
      GLog.doWrite(NAME_Init, fmt, ap, true);
      va_end(ap);
      #endif
      return;
    case VWAD_LOG_WARNING:
      va_start(ap, fmt);
      GLog.doWrite(NAME_Warning, fmt, ap, true);
      va_end(ap);
      return;
    case VWAD_LOG_ERROR:
      va_start(ap, fmt);
      GLog.doWrite(NAME_Error, fmt, ap, true);
      va_end(ap);
      return;
    case VWAD_LOG_DEBUG:
      #if 0
      va_start(ap, fmt);
      GLog.doWrite(NAME_Debug, fmt, ap, true);
      #endif
      va_end(ap);
      return;
    default: break;
  }
  #if 0
  va_start(ap, fmt);
  GLog.doWrite(NAME_Log, fmt, ap, true);
  va_end(ap);
  #endif
}


static void assertion_failed (const char *fmt, ...) {
  static char msgbuf[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);
  va_end(ap);
  Sys_Error("%s", msgbuf);
}


#include "fsys_vwadfread.cpp"


//==========================================================================
//
//  VVWadFile::VVWadFile
//
//  takes ownership
//
//==========================================================================
VVWadFile::VVWadFile (VStream *fstream, VStr name)
  : VPakFileBase(name)
{
  vwad_logf = logger;
  vwad_assertion_failed = assertion_failed;
  OpenArchive(fstream);
  if (fstream->IsError()) Sys_Error("error opening archive \"%s\"", *PakFileName);
}


//==========================================================================
//
//  VVWadFile::~VVWadFile
//
//==========================================================================
VVWadFile::~VVWadFile () {
  if (vw_handle) {
    vwad_close_archive((vwad_handle **)&vw_handle);
  }
  Z_Free(vw_strm); vw_strm = nullptr;
}


//==========================================================================
//
//  VVWadFile::VVWadFile
//
//==========================================================================
void VVWadFile::OpenArchive (VStream *fstream) {
  vassert(fstream);
  vwad_iostream *iostrm = (vwad_iostream *)Z_Calloc(sizeof(vwad_iostream));
  vassert(iostrm);

  iostrm->seek = &ioseek;
  iostrm->read = &ioread;
  iostrm->udata = (void *)fstream;
  vw_strm = iostrm;

  vwad_handle *wad = vwad_open_archive(iostrm, VWAD_OPEN_DEFAULT|VWAD_OPEN_NO_MAIN_COMMENT, &memman);
  if (!wad) {
    Sys_Error("cannot load vwad file \"%s\"", *PakFileName);
  }
  vw_handle = wad;

  if (vwad_has_pubkey(wad) && vwad_is_authenticated(wad)) {
    vwad_public_key pubkey;
    if (vwad_get_pubkey(wad, pubkey) == 0) {
      if (memcmp(pubkey, k8PubKey, sizeof(pubkey)) == 0) {
        GLog.Logf(NAME_Init, "SIGNED BY: Ketmar Dark");
      } else {
        const char *signer = FSYS_FindPublicKey(pubkey);
        if (signer) {
          GLog.Logf(NAME_Init, "SIGNED BY: %s", signer);
        } else {
          GLog.Logf(NAME_Init, "SIGNED ARCHIVE");
        }
      }
    }
  }

  const char *author = vwad_get_archive_author(wad);
  if (author[0]) GLog.Logf(NAME_Init, "AUTHOR(S): %s", author);

  const char *title = vwad_get_archive_title(wad);
  if (title[0]) GLog.Logf(NAME_Init, "TITLE: %s", title);

  /*
  const char *comment = vwad_get_archive_comment(wad);
  if (comment) {
    char cmt[64];
    int pos = 0;
    while (pos < 63 && comment[pos] && comment[pos] != '\n') {
      cmt[pos] = comment[pos];
      ++pos;
    }
    if (pos < 63) {
      cmt[pos] = 0;
      GLog.Logf(NAME_Init, "COMMENT: %s", cmt);
    }
  }
  vwad_free_archive_comment(wad);
  */

  type = PAK;

  const int maxFIdx = vwad_get_max_fidx(wad);
  #if 0
  GLog.Logf(NAME_Debug, "VWAD: %d files...", maxFIdx);
  #endif
  for (int i = 1; i <= maxFIdx; ++i) {
    const char *fname = vwad_get_file_name(wad, i);
    vassert(fname && fname[0] && fname[strlen(fname) - 1] != '/');

    #if 0
    GLog.Logf(NAME_Debug, "VWAD: %d: '%s' (%d)", i, fname, vwad_get_file_size(wad, i));
    #endif

    VPakFileInfo fi;
    fi.SetFileName(fname);
    fi.filetime = vwad_get_ftime(wad, i);
    fi.pakdataofs = i; // index
    fi.filesize = vwad_get_file_size(wad, i);
    pakdir.append(fi);
  }

  pakdir.buildLumpNames();
  pakdir.buildNameMaps(false, this);
}


//==========================================================================
//
//  VVWadFile::CreateLumpReaderNum
//
//==========================================================================
VStream *VVWadFile::CreateLumpReaderNum (int Lump) {
  vassert(Lump >= 0);
  vassert(Lump < pakdir.files.length());
  return new VVWadFileReader(PakFileName+":"+pakdir.files[Lump].fileNameIntr,
                             pakdir.files[Lump],
                             (vwad_handle *)vw_handle, (vwad_iostream *)vw_strm,
                             &rdlock);
}
