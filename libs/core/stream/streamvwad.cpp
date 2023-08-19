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

//#define VVX_DEBUG_WRITER


static void logger (int type, const char *fmt, ...) {
  va_list ap;
  switch (type) {
    case VWADWR_LOG_NOTE:
      #if 1
      va_start(ap, fmt);
      GLog.doWrite(NAME_Debug, fmt, ap, true);
      va_end(ap);
      #endif
      return;
    case VWADWR_LOG_WARNING:
      va_start(ap, fmt);
      GLog.doWrite(NAME_Warning, fmt, ap, true);
      va_end(ap);
      return;
    case VWADWR_LOG_ERROR:
      va_start(ap, fmt);
      GLog.doWrite(NAME_Error, fmt, ap, true);
      va_end(ap);
      return;
    case VWADWR_LOG_DEBUG:
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


static void *xxalloc (vwad_memman *mman, uint32_t size) {
  return Z_Malloc(size);
}

static void xxfree (vwad_memman *mman, void *p) {
  return Z_Free(p);
}

static void *wrxxalloc (vwadwr_memman *mman, uint32_t size) {
  return Z_Malloc(size);
}

static void wrxxfree (vwadwr_memman *mman, void *p) {
  return Z_Free(p);
}

static vwad_memman memman = {
  .alloc = &xxalloc,
  .free = &xxfree,
};

static vwadwr_memman wrmemman = {
  .alloc = &wrxxalloc,
  .free = &wrxxfree,
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
    s->Close();
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
//  VVWadArchive::FileExists
//
//==========================================================================
bool VVWadArchive::FileExists (VStr name) {
  bool res = false;
  if (vw_handle) res = (vwad_find_file(vw_handle, *name) >= 0);
  return res;
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
    const vwad_fd fd = vwad_open_file(vw_handle, *name);
    if (fd >= 0) {
      res = new VVWadStreamReader(this, fd);
      if (res->IsError()) VStream::Destroy(res);
    }
  }
  return res;
}


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
    //GLog.Logf(NAME_Debug, "afd=%d", afd);
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
//  VVWadStreamReader::GetGroupName
//
//==========================================================================
VStr VVWadStreamReader::GetGroupName () const {
  if (!arc || fd < 0) return VStr::EmptyString;
  return VStr(vwad_get_file_group_name(vw_handle, vwad_fdfidx(vw_handle, fd)));
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

  if (!st->IsLoading()) {
    GLog.Logf(NAME_Error, "SHIT! reading from writer stream!");
  }

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
//  VStream_write
//
//  write *exactly* bufsize bytes; return 0 on success, negative on failure
//
//==========================================================================
static int VStream_write (vwadwr_iostream *strm, const void *buf, int bufsize) {
  VStream *st = (VStream *)strm->udata;
  if (st->IsError()) return -1;

  if (st->IsLoading()) {
    GLog.Logf(NAME_Error, "SHIT! writing to reader stream!");
  }

  st->Serialise(buf, bufsize);
  return (!st->IsError() ? 0 : -1);
}


//==========================================================================
//
//  VVWadNewArchive::VVWadNewArchive
//
//  create to stream; will close owned stream even on open error
//  stream will be seeked to 0
//
//==========================================================================
VVWadNewArchive::VVWadNewArchive (VStr aArchName, VStr author, VStr title,
                                  VStream *dstrm, bool owned)
  : archname(aArchName)
  , vw_handle(nullptr)
  , destStream(nullptr)
  , destStreamOwn(owned)
  , openlist(nullptr)
  , error(true)
{
  if (!vwadwr_logf) vwadwr_logf = logger;
  if (!vwadwr_assertion_failed) vwadwr_assertion_failed = assertion_failed;

  if (!dstrm || dstrm->IsLoading() || dstrm->IsError()) {
    if (owned && dstrm) VStream::Destroy(dstrm);
    return;
  }

  if (dstrm->Tell() != 0) dstrm->Seek(0);
  if (dstrm->IsError()) {
    if (owned && dstrm) VStream::Destroy(dstrm);
    return;
  }

  vwadwr_secret_key privkey;
  do {
    prng_randombytes(privkey, sizeof(vwadwr_secret_key));
  } while (!vwadwr_is_good_privkey(privkey));

  vw_strm.seek = &VStream_seek;
  vw_strm.tell = &VStream_tell;
  vw_strm.read = &VStream_read;
  vw_strm.write = &VStream_write;
  vw_strm.udata = (void *)dstrm;

  vw_handle = vwadwr_new_archive(&wrmemman, &vw_strm, *author, *title,
                                 NULL, /* no comment */
                                 VWADWR_NEW_DONT_SIGN, privkey,
                                 NULL, NULL);
  if (!vw_handle || dstrm->IsError()) {
    if (owned && dstrm) VStream::Destroy(dstrm);
    return;
  }

  destStream = dstrm;
  error = false;
}


//==========================================================================
//
//  VVWadNewArchive::~VVWadNewArchive
//
//==========================================================================
VVWadNewArchive::~VVWadNewArchive () {
  Close();
}


//==========================================================================
//
//  VVWadNewArchive::Close
//
//  finish creating, write final dir, and so on
//  return success flag
//
//==========================================================================
bool VVWadNewArchive::Close () {
  if (openlist) {
    SetError();
    vassert(!openlist);
  }
  if (vw_handle) {
    const vwadwr_result xres = vwadwr_finish_archive(&vw_handle);
    vassert(vw_handle == nullptr);
    if (xres != VWADWR_OK) SetError();
  }
  if (destStreamOwn && destStream) VStream::Destroy(destStream);
  destStream = nullptr;
  return !error;
}


//==========================================================================
//
//  VVWadNewArchive::SetError
//
//  this also closes the archive handle, and possibly destroys destination stream
//
//==========================================================================
void VVWadNewArchive::SetError () {
  while (openlist) {
    VVWadStreamWriter *wr = openlist;
    openlist = wr->next; wr->next = nullptr;
    #ifdef VVX_DEBUG_WRITER
    GLog.Logf(NAME_Debug, "WARC: SetError: %s", *wr->fname);
    #endif
    wr->SetError();
  }
  if (vw_handle) {
    vwadwr_free_archive(&vw_handle);
    vassert(vw_handle == nullptr);
  }
  if (destStreamOwn && destStream) VStream::Destroy(destStream);
  destStream = nullptr;
  error = true;
}


//==========================================================================
//
//  VVWadNewArchive::CreateFile
//
//  return `nullptr` on error
//  otherwise, return unseekable stream suitable for writing
//  close the stream to finish file
//
//==========================================================================
VStream *VVWadNewArchive::CreateFile (VStr name, int level/* VADWR_COMP_xxx */,
                                      VStr groupname, vwadwr_ftime ftime,
                                      bool buffit)
{
  if (error) return nullptr;
  if (!vw_handle) {
    #ifdef VVX_DEBUG_WRITER
    if (active) {
      GLog.Logf(NAME_Debug, "WARC: CreateDup: %s (new: %s)", *active->fname, *name);
    }
    #endif
    SetError();
    return nullptr;
  }

  const vwadwr_fhandle fd = vwadwr_create_file(vw_handle, level, *name, *groupname, ftime);
  if (fd < 0) {
    #ifdef VVX_DEBUG_WRITER
    GLog.Logf(NAME_Debug, "WARC: CreateFileErr: %s; err=%d", *name, fd);
    #endif
    SetError();
    return nullptr;
  }

  return new VVWadStreamWriter(this, name, fd, buffit);
}


//==========================================================================
//
//  VVWadStreamWriter::VVWadStreamWriter
//
//==========================================================================
VVWadStreamWriter::VVWadStreamWriter (VVWadNewArchive *aarc, VStr afname,
                                      vwadwr_fhandle afd, bool buffit)
  : arc(aarc)
  , stbuf(nullptr)
  , fd(afd)
  , fname(afname)
  , currpos(0)
  , seekpos(0)
  , next(nullptr)
{
  bLoading = false;
  if (aarc == nullptr || !aarc->IsOpen() || aarc->IsError()) {
    SetError();
  } else {
    next = aarc->openlist;
    aarc->openlist = this;
    if (buffit) {
      stbuf = new VMemoryStream();
      stbuf->BeginWrite();
    }
    #ifdef VVX_DEBUG_WRITER
    GLog.Logf(NAME_Debug, "WR: Created: %s; buffit=%d", *fname, (int)buffit);
    #endif
  }
}


//==========================================================================
//
//  VVWadStreamWriter::~VVWadStreamWriter
//
//==========================================================================
VVWadStreamWriter::~VVWadStreamWriter () {
  DoClose();
}


//==========================================================================
//
//  VVWadStreamWriter::DoClose
//
//==========================================================================
void VVWadStreamWriter::DoClose () {
  if (arc != nullptr) {
    #ifdef VVX_DEBUG_WRITER
    GLog.Logf(NAME_Debug, "WR: DoClose: %s", *fname);
    #endif
    vwadwr_archive *wad = nullptr;
    VVWadNewArchive *aarc = arc;
    arc = nullptr;

    // remove from the parent list
    VVWadStreamWriter *prev = nullptr;
    VVWadStreamWriter *wr = aarc->openlist;
    while (wr != nullptr && wr != this) { prev = wr; wr = wr->next; }
    if (wr != nullptr) {
      if (prev == nullptr) aarc->openlist = next; else prev->next = next;
      wad = aarc->vw_handle;
      if (IsError()) aarc->SetError();
    } else {
      #ifdef VVX_DEBUG_WRITER
      GLog.Logf(NAME_Debug, "WR: DoClose: %s -- orphan!", *fname);
      #endif
    }

    if (!IsError()) {
      if (wad != nullptr) {
        // write buffered stream?
        if (stbuf != nullptr) {
          TArrayNC<vuint8> data = stbuf->GetArray();
          vwadwr_result wres = vwadwr_write(aarc->vw_handle, fd, data.Ptr(),
                                            (vwadwr_uint)data.length());
          if (wres != VWADWR_OK) {
            #ifdef VVX_DEBUG_WRITER
            GLog.Logf(NAME_Debug, "WR: BUFWRITE ERROR=%d", wres);
            #endif
            aarc->SetError();
            SetError();
          }
        }
        if (vwadwr_close_file(wad, fd) != VWADWR_OK) {
          #ifdef VVX_DEBUG_WRITER
          GLog.Logf(NAME_Debug, "WR: DoClose: %s -- close error", *fname);
          #endif
          aarc->SetError();
          SetError();
        }
      } else {
        #ifdef VVX_DEBUG_WRITER
        GLog.Logf(NAME_Debug, "WR: DoClose: %s -- xxorphan!", *fname);
        #endif
        aarc->SetError();
        SetError();
      }
    }
  }

  // we don't need a buffer anymore
  if (stbuf != nullptr) { stbuf->Close(); delete stbuf; stbuf = nullptr; }
  fd = -1;

  #ifdef VVX_DEBUG_WRITER
  GLog.Logf(NAME_Debug, "WR: DoClose: %s (%d)", *fname, (int)IsError());
  #endif
  fname.clear();
}


//==========================================================================
//
//  VVWadStreamWriter::Close
//
//==========================================================================
bool VVWadStreamWriter::Close () {
  DoClose();
  return !IsError();
}


//==========================================================================
//
//  VVWadStreamWriter::SetError
//
//==========================================================================
void VVWadStreamWriter::SetError () {
  #ifdef VVX_DEBUG_WRITER
  GLog.Logf(NAME_Debug, "WR: SetError: %s (%d)", *fname, (int)IsError());
  #endif

  if (stbuf != nullptr) { stbuf->Close(); delete stbuf; stbuf = nullptr; }

  VVWadNewArchive *aarc = arc;
  arc = nullptr;

  if (aarc != nullptr) {
    if (fd >= 0) (void)vwadwr_close_file(aarc->vw_handle, fd);

    // remove from the parent list
    VVWadStreamWriter *prev = nullptr;
    VVWadStreamWriter *wr = aarc->openlist;
    while (wr != nullptr && wr != this) { prev = wr; wr = wr->next; }
    if (wr != nullptr) {
      if (prev == nullptr) aarc->openlist = next; else prev->next = next;
      if (IsError()) aarc->SetError();
    } else {
      #ifdef VVX_DEBUG_WRITER
      GLog.Logf(NAME_Debug, "WR: DoClose: %s -- orphan!", *fname);
      #endif
    }
  }

  fname.clear();
  fd = -1;

  VStream::SetError();
}


//==========================================================================
//
//  VVWadStreamWriter::GetName
//
//==========================================================================
VStr VVWadStreamWriter::GetName () const {
  if (!arc) return "<shit>";
  return VStr(arc->archname + ":" + fname);
}


//==========================================================================
//
//  VVWadStreamWriter::Serialise
//
//==========================================================================
void VVWadStreamWriter::Serialise (void *buf, int len) {
  if (len == 0 || IsError()) return;
  #if 0 && defined(VVX_DEBUG_WRITER)
  GLog.Logf(NAME_Debug, "WR: Serialise: %s (len=%d; buf=%p; err=%d)",
            *fname, len, buf, (int)IsError());
  #endif
  if (len < 0 || !buf || !arc) {
    #ifdef VVX_DEBUG_WRITER
    GLog.Logf(NAME_Debug, "WR: ARG ERROR! arc=%p", arc);
    #endif
    SetError();
    return;
  }

  if (stbuf != nullptr) {
    if (stbuf->IsError()) {
      #ifdef VVX_DEBUG_WRITER
      GLog.Log(NAME_Debug, "WR: WRITE TO ERRORED BUFFSTREAM");
      #endif
      SetError();
      return;
    }
    vassert(currpos == stbuf->Tell());
    if (seekpos != currpos) {
      #ifdef VVX_DEBUG_WRITER
      GLog.Logf(NAME_Debug, "WR: BEFORE SEEK (sp=%d; cp=%d; xp=%d; ts=%d)",
                seekpos, currpos, stbuf->Tell(), stbuf->TotalSize());
      #endif
      stbuf->Seek(seekpos);
      if (stbuf->IsError()) {
        #ifdef VVX_DEBUG_WRITER
        GLog.Logf(NAME_Debug, "WR: SEEK FAIL (sp=%d; cp=%d)", seekpos, currpos);
        #endif
        SetError();
        return;
      }
      currpos = seekpos;
      vassert(currpos == stbuf->Tell());
    }
    stbuf->Serialise(buf, len);
    if (stbuf->IsError()) {
      #ifdef VVX_DEBUG_WRITER
      GLog.Log(NAME_Debug, "WR: WRITE FAIL");
      #endif
      SetError();
    } else {
      seekpos = (currpos += len);
    }
  } else {
    if (seekpos != currpos) {
      #ifdef VVX_DEBUG_WRITER
      GLog.Logf(NAME_Debug, "WR: ARG ERROR! seekpos=%d; currpos=%d", seekpos, currpos);
      #endif
      SetError();
    }

    const vwadwr_result res = vwadwr_write(arc->vw_handle, fd, buf, (vwadwr_uint)len);
    if (res != VWADWR_OK) {
      #ifdef VVX_DEBUG_WRITER
      GLog.Logf(NAME_Debug, "WR: ERROR=%d", res);
      #endif
      SetError();
    } else {
      seekpos = (currpos += len);
    }
  }
}


//==========================================================================
//
//  VVWadStreamWriter::Seek
//
//==========================================================================
void VVWadStreamWriter::Seek (int pos) {
  if (stbuf != nullptr) {
    if (!stbuf->IsError()) {
      if (pos < 0 || pos > stbuf->TotalSize()) {
        #ifdef VVX_DEBUG_WRITER
        GLog.Logf(NAME_Debug, "WR: SeekError: %s; pos=%d; currpos=%d", *fname, pos, currpos);
        #endif
      } else {
        seekpos = pos;
      }
    }
  } else {
    if (pos < 0 || pos > currpos) {
      #ifdef VVX_DEBUG_WRITER
      GLog.Logf(NAME_Debug, "WR: SeekError: %s; pos=%d; currpos=%d", *fname, pos, currpos);
      #endif
      SetError();
    } else {
      seekpos = pos;
    }
  }
}


//==========================================================================
//
//  VVWadStreamWriter::Tell
//
//==========================================================================
int VVWadStreamWriter::Tell () {
  return seekpos;
}


//==========================================================================
//
//  VVWadStreamWriter::TotalSize
//
//==========================================================================
int VVWadStreamWriter::TotalSize () {
  return (stbuf != nullptr ? stbuf->TotalSize() : currpos);
}


//==========================================================================
//
//  VVWadStreamWriter::AtEnd
//
//==========================================================================
bool VVWadStreamWriter::AtEnd () {
  return (stbuf != nullptr ? stbuf->AtEnd() : true);
}
