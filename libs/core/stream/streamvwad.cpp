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
#include "../core.h"


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


static vwad_result VArch_seek (vwad_iostream *strm, int pos) {
  VStream *s = (VStream *)strm->udata;
  s->Seek(pos);
  if (s->IsError()) return -1;
  return 0;
}


static vwad_result VArch_read (vwad_iostream *strm, void *buf, int bufsize) {
  VStream *s = (VStream *)strm->udata;
  s->Serialise(buf, bufsize);
  if (s->IsError()) return -1;
  return 0;
}


//==========================================================================
//
//  VVWadArchive::VVWadArchive
//
//==========================================================================
VVWadArchive::VVWadArchive (VStr aArchName)
  : archname()
  , vw_handle(nullptr)
  , srcStream(nullptr)
  , srcStreamOwn(false)
  , openlist(nullptr)
{
  vw_strm.seek = &VArch_seek;
  vw_strm.read = &VArch_read;
  vw_strm.udata = nullptr;

  srcStream = FL_OpenSysFileRead(aArchName);
  if (srcStream) {
    srcStreamOwn = true;
    vw_strm.udata = (void *)srcStream;
    vw_handle = vwad_open_archive(&vw_strm, VWAD_OPEN_DEFAULT|VWAD_OPEN_NO_MAIN_COMMENT, &memman);
    if (!vw_handle) {
      VStream::Destroy(srcStream);
      vw_strm.udata = nullptr;
      srcStreamOwn = false;
    }
  }
}


//==========================================================================
//
//  VVWadArchive::VVWadArchive
//
//==========================================================================
VVWadArchive::VVWadArchive (VStr aArchName, VStream *strm, bool owned)
  : archname()
  , vw_handle(nullptr)
  , srcStream(nullptr)
  , srcStreamOwn(owned)
  , openlist(nullptr)
{
  vw_strm.seek = &VArch_seek;
  vw_strm.read = &VArch_read;
  vw_strm.udata = nullptr;

  if (strm) {
    srcStream = strm;
    vw_strm.udata = (void *)srcStream;
    vw_handle = vwad_open_archive(&vw_strm, VWAD_OPEN_DEFAULT|VWAD_OPEN_NO_MAIN_COMMENT, &memman);
    if (!vw_handle) {
      if (owned) VStream::Destroy(srcStream);
      vw_strm.udata = nullptr;
      srcStreamOwn = false;
    }
  }
}


//==========================================================================
//
//  VVWadArchive::~VVWadArchive
//
//==========================================================================
VVWadArchive::~VVWadArchive () {
  Close();
}


//==========================================================================
//
//  VVWadArchive::Close
//
//==========================================================================
void VVWadArchive::Close () {
  while (openlist) {
    VStream *s = openlist;
    VStream::Destroy(s);
  }
  if (vw_handle) vwad_close_archive(&vw_handle);
  archname.clear();
  vw_handle = nullptr;
  if (srcStream && srcStreamOwn) VStream::Destroy(srcStream);
  srcStream = nullptr;
  srcStreamOwn = false;
  vassert(openlist == nullptr);
}


//==========================================================================
//
//  VVWadArchive::OpenFile
//
//  return `nullptr` on error
//
//==========================================================================
VStream *VVWadArchive::OpenFile (VStr name) {
  VStream *res = nullptr;
  if (vw_handle) {
    vwad_fidx fidx = vwad_find_file(vw_handle, *name);
    if (fidx >= 0) {
      int fd = vwad_fopen(vw_handle, fidx);
      if (fd >= 0) {
        VStream *res = new VVWadStreamReader(this, fd);
        if (res->IsError()) { VStream::Destroy(res); res = nullptr; }
      }
    }
  }
  return res;
}


//==========================================================================
//
//  VVWadStreamReader::VVWadStreamReader
//
//==========================================================================
/*
VVWadStreamReader::VVWadStreamReader (VStr aArchName, vwad_handle *a_vw_handle,
                                      vwad_iostream *a_vw_strm, vwad_fd afd)
  : archname(aArchName)
  , vw_handle(a_vw_handle)
  , vw_strm(a_vw_strm)
  , fd(afd)
{
  bLoading = true;
  //fd = vwad_fopen(vw_handle, aInfo.pakdataofs);

  if (afd < 0) {
    SetError();
  }
}
*/


//==========================================================================
//
//  VVWadStreamReader::VVWadStreamReader
//
//==========================================================================
/*
VVWadStreamReader::VVWadStreamReader (VStr aArchName, vwad_handle *a_vw_handle,
                                      vwad_iostream *a_vw_strm, VStr name)
  : archname(aArchName)
  , vw_handle(a_vw_handle)
  , vw_strm(a_vw_strm)
  , fd(-1)
{
  bLoading = true;
  vwad_fidx fidx = vwad_find_file(vw_handle, *name);
  if (fidx < 0) {
    SetError();
  } else {
    fd = vwad_fopen(vw_handle, fidx);
    if (fd < 0) {
      SetError();
    }
  }
}
*/


//==========================================================================
//
//  VVWadStreamReader::VVWadStreamReader
//
//==========================================================================
VVWadStreamReader::VVWadStreamReader (VVWadArchive *aarc, vwad_fd afd)
  : arc(nullptr)
  , vw_handle(nullptr)
  , vw_strm(nullptr)
  , fd(-1)
  , next(nullptr)
{
  bLoading = true;

  if (aarc && aarc->vw_handle && afd >= 0) {
    arc = aarc;
    vw_handle = aarc->vw_handle;
    vw_strm = &aarc->vw_strm;
    fd = afd;
    // register
    next = arc->openlist;
    arc->openlist = next;
  } else {
    SetError();
  }
}


//==========================================================================
//
//  VVWadStreamReader::~VVWadStreamReader
//
//==========================================================================
VVWadStreamReader::~VVWadStreamReader() {
  DoClose();
  Close();
}


//==========================================================================
//
//  VVWadStreamReader::DoClose
//
//==========================================================================
void VVWadStreamReader::DoClose () {
  if (fd >= 0) { vwad_fclose(vw_handle, fd); fd = -1; }
  if (arc) {
    VVWadStreamReader *p = nullptr;
    VVWadStreamReader *c = arc->openlist;
    while (c != nullptr && c != this) { p = c; c = c->next; }
    if (c != nullptr) {
      if (p == nullptr) arc->openlist = next; else p->next = next;
    }
  }
  arc = nullptr;
  vw_handle = nullptr;
  vw_strm = nullptr;
  fd = -1;
  next = nullptr;
}


//==========================================================================
//
//  VVWadStreamReader::GetName
//
//==========================================================================
VStr VVWadStreamReader::GetName () const {
  if (!arc || fd < 0) return "<shit>";
  return VStr(arc->archname + ":" + VStr(vwad_get_file_name(vw_handle, vwad_fdfidx(vw_handle, fd))));
}


//==========================================================================
//
//  VVWadStreamReader::SetError
//
//==========================================================================
void VVWadStreamReader::SetError () {
  DoClose();
  VStream::SetError();
}


//==========================================================================
//
//  VVWadStreamReader::Close
//
//==========================================================================
bool VVWadStreamReader::Close () {
  DoClose();
  return !bError;
}


//==========================================================================
//
//  VVWadStreamReader::Serialise
//
//==========================================================================
void VVWadStreamReader::Serialise (void *V, int length) {
  if (bError) return;
  if (length == 0) return;
  if (length < 0) { SetError(); return; }
  if (!V) { SetError(); return; }
  if (fd < 0) { SetError(); return; }

  while (length != 0) {
    int rd = vwad_read(vw_handle, fd, V, length);
    if (rd <= 0) { SetError(); return; }
    length -= rd;
    V = ((char *)V) + (unsigned)rd;
  }
}


//==========================================================================
//
//  VVWadStreamReader::Seek
//
//==========================================================================
void VVWadStreamReader::Seek (int InPos) {
  if (bError) return;
  if (vwad_seek(vw_handle, fd, InPos) != 0) {
    SetError();
  }
}


//==========================================================================
//
//  VVWadStreamReader::Tell
//
//==========================================================================
int VVWadStreamReader::Tell () {
  return vwad_tell(vw_handle, fd);
}


//==========================================================================
//
//  VVWadStreamReader::TotalSize
//
//==========================================================================
int VVWadStreamReader::TotalSize () {
  //MyThreadLocker locker(rdlock);
  return vwad_get_file_size(vw_handle, vwad_fdfidx(vw_handle, fd));
}


//==========================================================================
//
//  VVWadStreamReader::AtEnd
//
//==========================================================================
bool VVWadStreamReader::AtEnd () {
  return (vwad_tell(vw_handle, fd) >= TotalSize());
}


//==========================================================================
//
//  VStream_seek
//
//==========================================================================
static vwadwr_result VStream_seek (vwadwr_iostream *strm, int pos) {
  VStream *st = (VStream *)strm->udata;
  if (!st->IsError()) st->Seek(pos);
  return (!st->IsError() ? 0 : -1);
}


//==========================================================================
//
//  VStream_tell
//
//==========================================================================
static int VStream_tell (vwadwr_iostream *strm) {
  VStream *st = (VStream *)strm->udata;
  return (!st->IsError() ? st->Tell() : -1);
}


//==========================================================================
//
//  VStream_read
//
//  read at most bufsize bytes
//  return number of read bytes, or negative on failure
//
//==========================================================================
static int VStream_read (vwadwr_iostream *strm, void *buf, int bufsize) {
  VStream *st = (VStream *)strm->udata;
  if (st->IsError()) return -1;
  int pos = st->Tell();
  int size = st->TotalSize();
  if (pos < 0 || size < 0) return -1;
  int left = size - pos;
  if (left < 0) return -1;
  if (bufsize > left) bufsize = left;
  if (bufsize > 0) {
    st->Serialise(buf, bufsize);
    if (st->IsError()) return -1;
  }
  return bufsize;
}


//==========================================================================
//
//  vwadwr_write_vstream
//
//==========================================================================
vwadwr_result vwadwr_write_vstream (vwadwr_dir *wad, VStream *strm,
                                    int level, /* VADWR_COMP_xxx */
                                    const char *pkfname,
                                    const char *groupname, /* can be NULL */
                                    unsigned long long ftime, /* can be 0; seconds since Epoch */
                                    int *upksizep, int *pksizep,
                                    vwadwr_pack_progress progcb, void *progudata)
{
  if (!strm) return -1;
  if (!strm->IsLoading()) return -1;
  vwadwr_iostream ios;
  ios.seek = &VStream_seek;
  ios.tell = &VStream_tell;
  ios.read = &VStream_read;
  ios.write = nullptr;
  ios.udata = (void *)strm;
  return vwadwr_pack_file(wad, &ios, level, pkfname, groupname, ftime, upksizep, pksizep, progcb, progudata);
}
