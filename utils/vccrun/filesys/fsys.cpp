//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2018 Ketmar Dark
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 3
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************

#include "fsys.h"


// ////////////////////////////////////////////////////////////////////////// //
VStr fsysBaseDir = VStr("./"); // always ends with "/" (fill be fixed by `fsysInit()` if necessary)
bool fsysDiskFirst = true; // default is true


// ////////////////////////////////////////////////////////////////////////// //
enum { MaxOpenPaks = 1024 };

static FSysDriverBase *openPaks[MaxOpenPaks]; // 0 is always basedir
static int openPakCount = 0;


// ////////////////////////////////////////////////////////////////////////// //
//typedef FSysDriver* (*FSysOpenPakFn) (VStream *);

struct FSysDriverCreator {
  FSysOpenPakFn ldr;
  int prio;
  FSysDriverCreator *next;
};

static FSysDriverCreator *creators = nullptr;


void FSysRegisterDriver (FSysOpenPakFn ldr, int prio) {
  if (!ldr) return;

  FSysDriverCreator *prev = nullptr, *cur = creators;
  while (cur && cur->prio >= prio) {
    prev = cur;
    cur = cur->next;
  }

  auto it = new FSysDriverCreator;
  it->ldr = ldr;
  it->prio = prio;
  it->next = cur;

  if (prev) prev->next = it; else creators = it;
}


// ////////////////////////////////////////////////////////////////////////// //
FSysDriverBase::~FSysDriverBase () {
  delete htable;
  htable = nullptr;
  htableSize = 0;
}


void FSysDriverBase::buildNameHashTable () {
  delete htable;
  htable = nullptr;
  int dlen = getNameCount();
  htableSize = (vuint32)dlen;
  if (dlen == 0) return;
  htable = new HashTableEntry[dlen];
  for (int f = 0; f < dlen; ++f) htable[f] = HashTableEntry(); // just in case
  for (int idx = dlen-1; idx >= 0; --idx) {
    vuint32 nhash = fnvHashBufCI(getNameByIndex(idx)); // never zero
    vuint32 hidx = nhash%(vuint32)dlen;
    if (htable[hidx].didx == 0xffffffffU) {
      // first item
      htable[hidx].hash = nhash;
      htable[hidx].didx = (vuint32)idx;
      //assert(htable.ptr[hidx].prev == 0xffffffffU);
    } else {
      // chain
      while (htable[hidx].prev != 0xffffffffU) hidx = htable[hidx].prev;
      // find free slot
      vuint32 freeslot = hidx;
      for (vuint32 count = 0; count < (vuint32)dlen; ++count) {
        freeslot = (freeslot+1)%(vuint32)dlen;
        if (htable[freeslot].hash == 0) break; // i found her!
      }
      //if (htable.ptr[freeslot].hash != 0) assert(0, "wtf?!");
      htable[hidx].prev = freeslot;
      htable[freeslot].hash = nhash;
      htable[freeslot].didx = (vuint32)idx;
      //assert(htable.ptr[freeslot].prev == uint.max);
    }
  }
}


// index or -1
int FSysDriverBase::findName (const VStr &fname) const {
  vuint32 nhash = fnvHashBufCI(fname);
  vuint32 hidx = nhash%htableSize;
  while (hidx != 0xffffffffU && htable[hidx].hash != 0) {
    if (htable[hidx].hash == nhash) {
      vuint32 didx = htable[hidx].didx;
      if (getNameByIndex(didx).equ1251CI(fname)) return (int)didx;
    }
    hidx = htable[hidx].prev;
  }
  // alas, and it is guaranteed that we have no such file here
  return -1;
}


bool FSysDriverBase::hasFile (const VStr &fname) const {
  return (findName(fname) >= 0);
}


VStream *FSysDriverBase::open (const VStr &fname) const {
  int idx = findName(fname);
  if (idx < 0) return nullptr;
  return open(idx);
}


// ////////////////////////////////////////////////////////////////////////// //
class VStreamDiskReader : public VStream {
private:
  FILE *mFl;
  VStr mName;
  int size; // <0: not determined yet

private:
  void setError ();

public:
  VStreamDiskReader (FILE *afl, const VStr &aname=VStr(), bool asWriter=false);
  virtual ~VStreamDiskReader () noexcept(false) override;

  virtual const VStr &GetName () const override;
  virtual void Seek (int pos) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual bool Close () override;
  virtual void Serialise (void *buf, int len) override;
};


VStreamDiskReader::VStreamDiskReader (FILE* afl, const VStr &aname, bool asWriter) : mFl(afl), mName(aname), size(-1) {
  if (afl) fseek(afl, 0, SEEK_SET);
  bLoading = !asWriter;
}

VStreamDiskReader::~VStreamDiskReader () noexcept(false) { Close(); }

void VStreamDiskReader::setError () {
  if (mFl) { fclose(mFl); mFl = nullptr; }
  mName.clear();
  bError = true;
}

const VStr &VStreamDiskReader::GetName () const { return mName; }

void VStreamDiskReader::Seek (int pos) {
  if (!mFl) { setError(); return; }
  if (fseek(mFl, pos, SEEK_SET)) setError();
}

int VStreamDiskReader::Tell () { return (bError || !mFl ? 0 : ftell(mFl)); }

int VStreamDiskReader::TotalSize () {
  if (size < 0 && mFl && !bError) {
    auto opos = ftell(mFl);
    fseek(mFl, 0, SEEK_END);
    size = (int)ftell(mFl);
    fseek(mFl, opos, SEEK_SET);
  }
  return size;
}

bool VStreamDiskReader::AtEnd () { return (bError || !mFl || Tell() >= TotalSize()); }

bool VStreamDiskReader::Close () {
  if (mFl) { fclose(mFl); mFl = nullptr; }
  mName.clear();
  return !bError;
}

void VStreamDiskReader::Serialise (void *buf, int len) {
  if (bError || !mFl || len < 0) { setError(); return; }
  if (len == 0) return;
  if (bLoading) {
    if (fread(buf, len, 1, mFl) != 1) setError();
  } else {
    if (fwrite(buf, len, 1, mFl) != 1) setError();
  }
}


// ////////////////////////////////////////////////////////////////////////// //
class FSysDriverDisk : public FSysDriverBase {
private:
  VStr path;

protected:
  virtual const VStr &getNameByIndex (int idx) const override;
  virtual int getNameCount () const override;

protected:
  virtual VStream *open (int idx) const override;

public:
  FSysDriverDisk (const VStr &apath);
  virtual ~FSysDriverDisk () override;

  virtual bool hasFile (const VStr &fname) const;
  virtual VStream *open (const VStr &fname) const;
};


FSysDriverDisk::FSysDriverDisk (const VStr &apath) : FSysDriverBase() {
  path = apath;
  if (path.length() == 0) path = "./";
  if (!path.endsWith("/")) path += "/";
}

FSysDriverDisk::~FSysDriverDisk () {}

const VStr &FSysDriverDisk::getNameByIndex (int idx) const { *(int *)0 = 0; return VStr::EmptyString; } // the thing that should not be
int FSysDriverDisk::getNameCount () const { *(int *)0 = 0; return 0; } // the thing that should not be
VStream *FSysDriverDisk::open (int idx) const { *(int *)0 = 0; return nullptr; } // the thing that should not be

bool FSysDriverDisk::hasFile (const VStr &fname) const {
  if (fname.length() == 0) return false;
  VStr newname = path+fname;
  FILE *fl = fopen(*newname, "rb");
  if (!fl) return false;
  fclose(fl);
  return true;
}

VStream *FSysDriverDisk::open (const VStr &fname) const {
  if (fname.length() == 0) return nullptr;
  VStr newname = path+fname;
  FILE *fl = fopen(*newname, "rb");
  if (!fl) return nullptr;
  return new VStreamDiskReader(fl, fname);
}


// ////////////////////////////////////////////////////////////////////////// //
// `fsysBaseDir` should be set before calling this
void fsysInit () {
  if (openPakCount == 0) {
         if (fsysBaseDir.length() == 0) fsysBaseDir = VStr("./");
    else if (!fsysBaseDir.endsWith("/")) fsysBaseDir += "/"; // fuck you, shitdoze
    openPaks[0] = new FSysDriverDisk(fsysBaseDir);
    openPakCount = 1;
  }
}


void fsysShutdown () {
}


// ////////////////////////////////////////////////////////////////////////// //
// append disk directory to the list of archives
void fsysAppendDir (const VStr &path) {
  if (path.length() == 0) return;
  if (openPakCount >= MaxOpenPaks) Sys_Error("too many pak files");
  openPaks[openPakCount++] = new FSysDriverDisk(path);
}


// append archive to the list of archives
// it will be searched in the current dir, and then in `fsysBaseDir`
bool fsysAppendPak (const VStr &fname) {
  if (fname.length() == 0) return false;
  FILE *fl = fopen(*fname, "rb");
  if (!fl) return false;
  return fsysAppendPak(new VStreamDiskReader(fl, fname));
}


// this will take ownership of `strm` (or kill it on error)
bool fsysAppendPak (VStream *strm) {
  if (!strm) return false;
  delete strm;
  return false;
}


// ////////////////////////////////////////////////////////////////////////// //
bool fsysFileExists (const VStr &fname) {
  if (openPakCount == 0) fsysInit();
  // try basedir first, if the corresponding flag is set
  if (fsysDiskFirst) {
    if (openPaks[0]->hasFile(fname)) return true;
  }
  // do other paks
  for (int f = openPakCount-1; f > 0; --f) {
    if (openPaks[f]->hasFile(fname)) return true;
  }
  // try basedir last, if the corresponding flag is set
  if (!fsysDiskFirst) {
    if (openPaks[0]->hasFile(fname)) return true;
  }
  return false;
}


// open file for reading, relative to basedir, and look into archives too
VStream *fsysOpenFile (const VStr &fname) {
  if (openPakCount == 0) fsysInit();
  // try basedir first, if the corresponding flag is set
  if (fsysDiskFirst) {
    auto res = openPaks[0]->open(fname);
    if (res) return res;
  }
  // do other paks
  for (int f = openPakCount-1; f > 0; --f) {
    auto res = openPaks[f]->open(fname);
    if (res) return res;
  }
  // try basedir last, if the corresponding flag is set
  if (!fsysDiskFirst) {
    auto res = openPaks[0]->open(fname);
    if (res) return res;
  }
  return nullptr;
}


// open file for reading, NOT relative to basedir
VStream *fsysOpenDiskFileWrite (const VStr &fname) {
  if (fname.length() == 0) return nullptr;
  FILE *fl = fopen(*fname, "wb");
  if (!fl) return nullptr;
  return new VStreamDiskReader(fl, fname, true);
}


VStream *fsysOpenDiskFile (const VStr &fname) {
  if (fname.length() == 0) return nullptr;
  FILE *fl = fopen(*fname, "rb");
  if (!fl) return nullptr;
  return new VStreamDiskReader(fl, fname);
}


// ////////////////////////////////////////////////////////////////////////// //
// VZipStreamReader
VZipStreamReader::VZipStreamReader (VStream *ASrcStream, vuint32 AUncompressedSize, bool asZipArchive)
  : VStream()
  , srcStream(ASrcStream)
  , initialised(false)
  , uncompressedSize(AUncompressedSize)
  , nextpos(0)
  , currpos(0)
  , zipArchive(asZipArchive)
{
  bLoading = true;

  // initialise zip stream structure
  zStream.total_out = 0;
  zStream.zalloc = (alloc_func)0;
  zStream.zfree = (free_func)0;
  zStream.opaque = (voidpf)0;

  if (srcStream) {
    // read in some initial data
    vint32 BytesToRead = BUFFER_SIZE;
    if (BytesToRead > srcStream->TotalSize()) BytesToRead = srcStream->TotalSize();
    srcStream->Seek(0);
    srcStream->Serialise(buffer, BytesToRead);
    if (srcStream->IsError()) { setError(); return; }
    zStream.next_in = buffer;
    zStream.avail_in = BytesToRead;
    // open zip stream
    int err = (zipArchive ? inflateInit2(&zStream, -MAX_WBITS) : inflateInit(&zStream));
    if (err != Z_OK) { setError(); return; }
    initialised = true;
  } else {
    bError = true;
  }
}


VZipStreamReader::~VZipStreamReader () {
  Close();
}


bool VZipStreamReader::Close () {
  if (initialised) { inflateEnd(&zStream); initialised = false; }
  if (srcStream) { delete srcStream; srcStream = nullptr; }
  return !bError;
}


void VZipStreamReader::setError () {
  if (initialised) { inflateEnd(&zStream); initialised = false; }
  if (srcStream) { delete srcStream; srcStream = nullptr; }
  bError = true;
}


// just read, no `nextpos` advancement
// returns number of bytes read, -1 on error, or 0 on EOF
int VZipStreamReader::readSomeBytes (void *buf, int len) {
  if (len <= 0) return -1;
  if (!srcStream) return -1;
  if (bError) return -1;
  if (srcStream->IsError()) return -1;

  zStream.next_out = (Bytef *)buf;
  zStream.avail_out = len;
  int bytesRead = 0;
  while (zStream.avail_out > 0) {
    // get more compressed data (if necessary)
    if (zStream.avail_in == 0) {
      //if (srcStream->AtEnd()) break;
      vint32 left = srcStream->TotalSize()-srcStream->Tell();
      if (left <= 0) break; // eof
      vint32 bytesToRead = BUFFER_SIZE;
      if (bytesToRead > left) bytesToRead = left;
      srcStream->Serialise(buffer, bytesToRead);
      if (srcStream->IsError()) return -1;
      zStream.next_in = buffer;
      zStream.avail_in = bytesToRead;
    }
    // unpack some data
    vuint32 totalOutBefore = zStream.total_out;
    int err = inflate(&zStream, /*Z_SYNC_FLUSH*/Z_NO_FLUSH);
    if (err != Z_OK && err != Z_STREAM_END) return -1;
    vuint32 totalOutAfter = zStream.total_out;
    bytesRead += totalOutAfter-totalOutBefore;
    if (err != Z_OK) break;
  }
  return bytesRead;
}


void VZipStreamReader::Serialise (void* buf, int len) {
  if (len == 0) return;
  if (!initialised || len < 0 || !srcStream || srcStream->IsError()) setError();
  if (bError) return;

  if (nextpos < currpos) {
    // rewing stream
    if (initialised) {
      inflateEnd(&zStream);
      initialised = false;
    }
    vint32 BytesToRead = BUFFER_SIZE;
    memset(&zStream, 0, sizeof(zStream));
    srcStream->Seek(0);
    srcStream->Serialise(buffer, BytesToRead);
    if (srcStream->IsError()) { setError(); return; }
    zStream.next_in = buffer;
    zStream.avail_in = BytesToRead;
    // open zip stream
    int err = (zipArchive ? inflateInit2(&zStream, -MAX_WBITS) : inflateInit(&zStream));
    if (err != Z_OK) { setError(); return; }
    initialised = true;
    currpos = 0;
  }

  while (nextpos < currpos) {
    char tmpbuf[256];
    int toread = currpos-nextpos;
    if (toread > 256) toread = 256;
    int rd = readSomeBytes(tmpbuf, toread);
    if (rd <= 0) { setError(); return; }
    currpos += rd;
  }

  if (nextpos != currpos) { setError(); return; } // just in case

  vuint8 *dest = (vuint8 *)buf;
  while (len > 0) {
    int rd = readSomeBytes(dest, len);
    if (rd <= 0) { setError(); return; }
    len -= rd;
    nextpos = (currpos += rd);
    dest += rd;
  }
}


void VZipStreamReader::Seek (int pos) {
  if (bError) return;

  if (pos < 0) { setError(); return; }

  if (uncompressedSize == 0xffffffffU) {
    // size is unknown
    nextpos = pos;
  } else {
    if ((vuint32)pos > uncompressedSize) pos = (vint32)uncompressedSize;
    nextpos = pos;
  }
}


int VZipStreamReader::Tell () { return nextpos; }


int VZipStreamReader::TotalSize () {
  if (bError) return 0;
  if (uncompressedSize == 0xffffffffU) {
    // calculate size
    for (;;) {
      char tmpbuf[256];
      int rd = readSomeBytes(tmpbuf, 256);
      if (rd < 0) { setError(); return 0; }
      if (rd == 0) break;
      currpos += rd;
    }
    uncompressedSize = (vuint32)currpos;
  }
  return uncompressedSize;
}


bool VZipStreamReader::AtEnd () { return (bError ? true : TotalSize() == currpos); }


// ////////////////////////////////////////////////////////////////////////// //
VZipStreamWriter::VZipStreamWriter (VStream *ADstStream)
  : dstStream(ADstStream)
  , initialised(false)
{
  bLoading = false;

  // initialise zip stream structure
  zStream.total_in = 0;
  zStream.zalloc = (alloc_func)0;
  zStream.zfree = (free_func)0;
  zStream.opaque = (voidpf)0;

  // open zip stream
  int err = deflateInit(&zStream, Z_BEST_COMPRESSION);
  if (err != Z_OK) { bError = true; return; }
  zStream.next_out = buffer;
  zStream.avail_out = BUFFER_SIZE;
  initialised = true;
}


VZipStreamWriter::~VZipStreamWriter () {
  Close();
}


void VZipStreamWriter::setError () {
  if (initialised) { deflateEnd(&zStream); initialised = false; }
  if (dstStream) { delete dstStream; dstStream = nullptr; }
  bError = true;
}


void VZipStreamWriter::Serialise (void *buf, int len) {
  if (len == 0) return;
  if (!initialised || len < 0 || !dstStream || dstStream->IsError()) setError();
  if (bError) return;

  zStream.next_in = (Bytef *)buf;
  zStream.avail_in = len;
  do {
    zStream.next_out = buffer;
    zStream.avail_out = BUFFER_SIZE;
    int err = deflate(&zStream, Z_NO_FLUSH);
    if (err == Z_STREAM_ERROR) { setError(); return; }
    if (zStream.avail_out != BUFFER_SIZE) {
      dstStream->Serialise(buffer, BUFFER_SIZE-zStream.avail_out);
      if (dstStream->IsError()) { setError(); return; }
    }
  } while (zStream.avail_out == 0);
  //check(zStream.avail_in == 0);
}


void VZipStreamWriter::Seek (int pos) {
  setError();
}


void VZipStreamWriter::Flush () {
  if (!initialised || !dstStream || dstStream->IsError()) setError();
  if (bError) return;

  zStream.avail_in = 0;
  do {
    zStream.next_out = buffer;
    zStream.avail_out = BUFFER_SIZE;
    int err = deflate(&zStream, Z_FULL_FLUSH);
    if (err == Z_STREAM_ERROR) { setError(); return; }
    if (zStream.avail_out != BUFFER_SIZE) {
      dstStream->Serialise(buffer, BUFFER_SIZE-zStream.avail_out);
      if (dstStream->IsError()) { setError(); return; }
    }
  } while (zStream.avail_out == 0);
  dstStream->Flush();
  if (dstStream->IsError()) { setError(); return; }
}


bool VZipStreamWriter::Close () {
  if (initialised) {
    zStream.avail_in = 0;
    do {
      zStream.next_out = buffer;
      zStream.avail_out = BUFFER_SIZE;
      int err = deflate(&zStream, Z_FINISH);
      if (err == Z_STREAM_ERROR) { setError(); return false; }
      if (zStream.avail_out != BUFFER_SIZE) {
        dstStream->Serialise(buffer, BUFFER_SIZE-zStream.avail_out);
        if (dstStream->IsError()) { setError(); return false; }
      }
    } while (zStream.avail_out == 0);
    deflateEnd(&zStream);
  }
  initialised = false;
  return !bError;
}
