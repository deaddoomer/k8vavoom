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
//**  as published by the Free Software Foundation; either version 3
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************
#ifndef FSYS_HEADER_FILE
#define FSYS_HEADER_FILE

#include "../../libs/core/core.h"


// ////////////////////////////////////////////////////////////////////////// //
extern VStr fsysBaseDir; // always ends with "/" (fill be fixed by `fsysInit()` if necessary)
extern bool fsysDiskFirst; // default is true
extern bool fsysKillCommonZipPrefix; // default false


enum { fsysAnyPak = -666 };


// ////////////////////////////////////////////////////////////////////////// //
// `fsysBaseDir` should be set before calling this
void fsysInit ();
void fsysShutdown ();

// WARNING! NOT THREAD-SAFE!
VStr fsysGetBinaryPath ();


// append disk directory to the list of archives
int fsysAppendDir (VStr path, VStr apfx=VStr());

// append archive to the list of archives
// it will be searched in the current dir, and then in `fsysBaseDir`
// returns pack id or 0
int fsysAppendPak (VStr fname, int pakid=fsysAnyPak);

// this will take ownership of `strm` (or kill it on error)
// returns pack id or 0
int fsysAppendPak (VStream *strm, VStr apfx=VStr());

// remove given pack from pack list
void fsysRemovePak (int pakid);

// remove all packs from pakid and later
void fsysRemovePaksFrom (int pakid);

// 0: no such pack
int fsysFindPakByPrefix (VStr pfx);

// return pack file path for the given pack id (or empty string)
VStr fsysGetPakPath (int pakid);

// return pack prefix for the given pack id (or empty string)
VStr fsysGetPakPrefix (int pakid);

int fsysGetLastPakId ();

bool fsysFileExists (VStr fname, int pakid=fsysAnyPak);
//void fsysCreatePath (VStr path);

// open file for reading, relative to basedir, and look into archives too
VStream *fsysOpenFile (VStr fname, int pakid=fsysAnyPak);
// open file for reading, relative to basedir, and look into archives too
VStream *fsysOpenFileSimple (VStr fname/*, int pakid=fsysAnyPak*/);

// open file for reading, relative to basedir, and look into archives too
VStream *fsysOpenFileAnyExt (VStr fname, int pakid=fsysAnyPak);

// open file for reading, NOT relative to basedir
VStream *fsysOpenDiskFileWrite (VStr fname);
VStream *fsysOpenDiskFile (VStr fname);

// find file with any extension
VStr fsysFileFindAnyExt (VStr fname, int pakid=fsysAnyPak);


// ////////////////////////////////////////////////////////////////////////// //
class FSysDriverBase {
  friend class VStreamPakFile;
  friend VStr fsysForEachPakFile (bool (*dg) (VStr fname));

protected:
  virtual VStr getNameByIndex (int idx) const = 0;
  virtual int getNameCount () const = 0;

protected:
  struct HashTableEntry {
    vuint32 hash; // name hash; name is lowercased
    vuint32 prev; // previous name with the same reduced hash position
    vuint32 didx; // dir index

    HashTableEntry () : hash(0), prev(0xffffffffU), didx(0xffffffffU) {}
  };

  // fnv-1a: http://www.isthe.com/chongo/tech/comp/fnv/
  static inline vuint32 fnvHashBufCI (const void *buf, size_t len) {
    vuint32 hash = 2166136261U; // fnv offset basis
    const vuint8 *s = (const vuint8 *)buf;
    while (len-- > 0) {
      hash ^= (vuint8)(VStr::locase1251(*s++));
      hash *= 16777619U; // 32-bit fnv prime
    }
    return (hash ? hash : 1); // this is unlikely, but...
  }

  static inline vuint32 fnvHashBufCI (VStr s) {
    if (s.length() == 0) return 1;
    return fnvHashBufCI(*s, s.length());
  }

protected:
  volatile int mOpenedFiles;
  VStr mPrefix; // this can be used to open named paks
  VStr mFilePath; // if opened from file
  vuint32 htableSize;
  HashTableEntry* htable; // for names, in reverse order; so name lookups will be faster
    // the algo is:
    //   htable[hashStr(name)%htable.length]: check if hash is ok, and name is ok
    //   if not ok, jump to htable[curht.prev], repeat
  bool mActive;

protected:
  // call this after you done building directory
  // never modify directory after that (or call `buildNameHashTable()` again)
  void buildNameHashTable ();

  // index or -1
  int findName (VStr fname) const;

protected:
  // should return `nullptr` on failure
  virtual VStream *openWithIndex (int idx) = 0;

  virtual void fileOpened (VStream *s);
  virtual void fileClosed (VStream *s);

public:
  FSysDriverBase ();
  virtual ~FSysDriverBase ();

  virtual bool canBeDestroyed ();

  virtual bool active ();
  virtual void deactivate ();

  inline void setPrefix (VStr apfx) { mPrefix = apfx; }
  inline VStr getPrefix () const { return mPrefix; }

  inline void setFilePath (VStr s) { mFilePath = s; }
  inline VStr getFilePath () const { return mFilePath; }

  virtual bool hasFile (VStr fname);

  virtual VStr findFileWithAnyExt (VStr fname);

  // should return `nullptr` on failure
  virtual VStream *open (VStr fname);
};


// ////////////////////////////////////////////////////////////////////////// //
typedef FSysDriverBase* (*FSysOpenPakFn) (VStream *);

// loaders with higher priority will be tried first
void FSysRegisterDriver (FSysOpenPakFn ldr, const char *drvname, int prio=1000);


// ////////////////////////////////////////////////////////////////////////// //
class VStreamPakFile : public VStream {
private:
  FSysDriverBase *mDriver;

public:
  VStreamPakFile (FSysDriverBase *aDriver);
  virtual ~VStreamPakFile () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VStreamDiskFile : public VStreamPakFile {
private:
  FILE *mFl;
  VStr mName;
  int size; // <0: not determined yet

private:
  void setError ();

public:
  VStreamDiskFile (FILE *afl, VStr aname=VStr(), bool asWriter=false, FSysDriverBase *aDriver=nullptr);
  virtual ~VStreamDiskFile () override;

  virtual VStr GetName () const override;
  virtual void Seek (int pos) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual bool Close () override;
  virtual void Serialise (void *buf, int len) override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VPartialStreamReader : public VStreamPakFile {
private:
  mythread_mutex lock;
  VStream *srcStream;
  int stpos;
  int srccurpos;
  int partlen;

private:
  void setError ();

public:
  // doesn't own passed stream
  VPartialStreamReader (VStream *ASrcStream, int astpos, int apartlen=-1, FSysDriverBase *aDriver=nullptr);
  virtual ~VPartialStreamReader () override;

  virtual VStr GetName () const override;
  virtual void Serialise (void *, int) override;
  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual bool Close () override;
};


// ////////////////////////////////////////////////////////////////////////// //
/*
class VZLibStreamReader : public VStreamPakFile {
private:
  enum { BUFFER_SIZE = 16384 };

  mythread_mutex lock;
  VStream *srcStream;
  int stpos;
  int srccurpos;
  Bytef buffer[BUFFER_SIZE];
  z_stream zStream;
  bool initialised;
  vuint32 compressedSize;
  vuint32 uncompressedSize;
  int nextpos;
  int currpos;
  bool zipArchive;
  vuint32 origCrc32;
  vuint32 currCrc32;
  bool doCrcCheck;
  bool forceRewind;
  VStr mFileName;

private:
  void initialize ();

  void setError ();

  // just read, no `nextpos` advancement
  // returns number of bytes read, -1 on error, or 0 on EOF
  int readSomeBytes (void *buf, int len);

public:
  // doesn't own passed stream
  VZLibStreamReader (VStream *ASrcStream, vuint32 ACompressedSize=0xffffffffU, vuint32 AUncompressedSize=0xffffffffU, bool asZipArchive=false, FSysDriverBase *aDriver=nullptr);
  VZLibStreamReader (VStr fname, VStream *ASrcStream, vuint32 ACompressedSize=0xffffffffU, vuint32 AUncompressedSize=0xffffffffU, bool asZipArchive=false, FSysDriverBase *aDriver=nullptr);
  virtual ~VZLibStreamReader () override;

  virtual VStr GetName () const override;

  void setCrc (vuint32 acrc); // turns on CRC checking

  virtual void Serialise (void *, int) override;
  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual bool Close () override;
};


class VZipStreamWriter : public VStream {
private:
  enum { BUFFER_SIZE = 16384 };

  mythread_mutex lock;
  VStream *dstStream;
  Bytef buffer[BUFFER_SIZE];
  z_stream zStream;
  bool initialised;
  vuint32 currCrc32;
  bool doCrcCalc;

private:
  void setError ();

public:
  VZipStreamWriter (VStream *); // doesn't own passed stream
  virtual ~VZipStreamWriter () override;
  void setRequireCrc ();
  vuint32 getCrc32 () const; // crc32 over uncompressed data (if enabled)
  virtual void Serialise (void *, int) override;
  virtual void Seek (int) override;
  virtual void Flush () override;
  virtual bool Close () override;
};
*/


// ////////////////////////////////////////////////////////////////////////// //
class VStreamMemRead : public VStream {
private:
  const vuint8 *data;
  vint32 datasize;
  vint32 pos;

private:
  void setError () { bError = true; data = nullptr; datasize = pos = 0; }

public:
  VStreamMemRead (const vuint8 *adata, vuint32 adatasize);
  virtual ~VStreamMemRead () override;

  virtual void Serialise (void *buf, int count) override;
  virtual void Seek (int ofs) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual bool Close () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VStreamMemWrite : public VStream {
private:
  vuint8 *data;
  vint32 datasize;
  vint32 pos;

private:
  void setError ();

public:
  VStreamMemWrite (vint32 areservesize=-1);
  virtual ~VStreamMemWrite () override;

  inline vuint8 *getData () { return data; }

  virtual void Serialise (void *buf, int count) override;
  virtual void Seek (int ofs) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual bool Close () override;
};


// ////////////////////////////////////////////////////////////////////////// //
// returns -1 if not present
int fsysDiskFileTime (VStr path);
bool fsysDirExists (VStr path);
bool fsysCreateDirectory (VStr path);

void *fsysOpenDir (VStr path);
VStr fsysReadDir (void *adir);
void fsysCloseDir (void *adir);

// kinda like `GetTickCount()`, in seconds
double fsysCurrTick ();

VStr fsysForEachPakFile (bool (*dg) (VStr fname));

// creates file in `fsysBaseDir`
VStream *fsysCreateFileSafe (VStr fname, bool notrunccate=false);

bool fsysCreateDirSafe (VStr dirname);


// ////////////////////////////////////////////////////////////////////////// //
void fsys_Register_ZIP ();
void fsys_Register_DFWAD ();
void fsys_Register_DOOMWAD ();


#endif
