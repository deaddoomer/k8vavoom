//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
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

// ////////////////////////////////////////////////////////////////////////// //
// stream for reading from raw memory chunk
class VMemoryStreamRO : public VStream {
protected:
  const vuint8 *Data;
  int Pos;
  int DataSize;
  bool FreeData; // free data with `Z_Free()` when this stream is destroyed?
  VStr StreamName;

public:
  VV_DISABLE_COPY(VMemoryStreamRO)

  VMemoryStreamRO (); // so we can declare it, and initialize later
  VMemoryStreamRO (VStr strmName, const void *adata, int adataSize, bool takeOwnership=false);
  VMemoryStreamRO (VStr strmName, VStream *strm); // from current position to stream end

  virtual ~VMemoryStreamRO () override;

  void Clear ();

  void Setup (VStr strmName, const void *adata, int adataSize, bool takeOwnership=false);
  void Setup (VStr strmName, VStream *strm); // from current position to stream end

  virtual void Serialise (void *Data, int Length) override;
  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;

  inline const vuint8 *GetPtr () const { return Data; }

  virtual VStr GetName () const override;
};


// ////////////////////////////////////////////////////////////////////////// //
// stream for reading and writing using `TArray`
class VMemoryStream : public VStream {
protected:
  TArrayNC<vuint8> Array;
  int Pos;
  VStr StreamName;

public:
  VV_DISABLE_COPY(VMemoryStream)

  // initialise empty writing stream
  VMemoryStream ();
  VMemoryStream (VStr strmName);
  // initialise reading streams
  VMemoryStream (VStr strmName, const void *, int, bool takeOwnership=false);
  VMemoryStream (VStr strmName, const TArrayNC<vuint8> &);
  VMemoryStream (VStr strmName, VStream *strm); // from current position to stream end

  virtual ~VMemoryStream () override;

  virtual void Serialise (void *Data, int Length) override;
  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool Close () override;

  inline void BeginRead () { bLoading = true; }
  inline void BeginWrite () { bLoading = false; }
  inline TArrayNC<vuint8> &GetArray () { return Array; }

  virtual VStr GetName () const override;
};


// ////////////////////////////////////////////////////////////////////////// //
// similar to VMemoryStream, but uses reference to an external array
class VArrayStream : public VStream {
protected:
  TArrayNC<vuint8> &Array;
  int Pos;
  VStr StreamName;

public:
  VV_DISABLE_COPY(VArrayStream)

  VArrayStream (VStr strmName, TArrayNC<vuint8> &);
  virtual ~VArrayStream () override;

  virtual void Serialise (void *Data, int Length) override;
  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;

  inline void BeginRead () { bLoading = true; }
  inline void BeginWrite () { bLoading = false; }
  inline TArrayNC<vuint8> &GetArray () { return Array; }

  virtual VStr GetName () const override;
};


// ////////////////////////////////////////////////////////////////////////// //
// stream for reading and writing in memory
// this does allocation by 8KB pages, and you cannot get direct pointer
// the idea is that you will use this as writer, then switch it to reader,
// and pass it to zip stream or something
// WARNING! not tested yet
class VPagedMemoryStream : public VStream {
private:
  enum { FullPageSize = 8192 };
  enum { DataPerPage = FullPageSize-sizeof(vuint8 *) };
  // each page contains pointer to the next page, and `FullPageSize-sizeof(void*)` bytes of data
  vuint8 *first; // first page
  vuint8 *curr; // current page for reader, last page for writer
  int pos; // current position
  int size; // current size
  VStr StreamName;

public:
  VV_DISABLE_COPY(VPagedMemoryStream)

  // initialise empty writing stream
  VPagedMemoryStream (VStr strmName);
  virtual ~VPagedMemoryStream () override;

  virtual void Serialise (void *Data, int Length) override;
  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool Close () override;

  inline void BeginRead () { bLoading = true; pos = 0; curr = first; }
  inline void BeginWrite () { bLoading = true; pos = 0; curr = first; }

  inline void SwitchToReader () { bLoading = true; }
  inline void SwitchToWriter () { bLoading = false; }

  void CopyTo (VStream *strm);

  virtual VStr GetName () const override;
};


// ////////////////////////////////////////////////////////////////////////// //
// owns afl
class VStdFileStreamBase : public VStream {
private:
  FILE *mFl;
  VStr mName;
  int size; // <0: not determined yet

private:

public:
  VV_DISABLE_COPY(VStdFileStreamBase)

  VStdFileStreamBase (FILE *afl, VStr aname, bool asWriter);
  virtual ~VStdFileStreamBase () override;

  virtual void SetError () override;
  virtual VStr GetName () const override;
  virtual void Seek (int pos) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual bool Close () override;
  virtual void Serialise (void *buf, int len) override;
};

// owns afl
class VStdFileStreamRead : public VStdFileStreamBase {
public:
  VV_DISABLE_COPY(VStdFileStreamRead)

  inline VStdFileStreamRead (FILE *afl, VStr aname=VStr()) : VStdFileStreamBase(afl, aname, false) {}
};

// owns afl
class VStdFileStreamWrite : public VStdFileStreamBase {
public:
  VV_DISABLE_COPY(VStdFileStreamWrite)

  inline VStdFileStreamWrite (FILE *afl, VStr aname=VStr()) : VStdFileStreamBase(afl, aname, true) {}
};


// this function is called by the engine to open disk files
// return any reading stream or `nullptr` for "file not found"
extern VStream *CreateDiskStreamRead (VStr fname, VStr streamname=VStr::EmptyString);
extern VStream *CreateDiskStreamWrite (VStr fname, VStr streamname=VStr::EmptyString);


// ////////////////////////////////////////////////////////////////////////// //
// doesn't own srcstream by default
// does full stream proxing (i.e. forwards all virtual methods)
class VPartialStreamRO : public VStream {
private:
  mutable mythread_mutex lock;
  mutable mythread_mutex *lockptr;
  VStream *srcStream;
  int stpos;
  int srccurpos;
  int partlen;
  bool srcOwned;
  bool closed;
  VStr myname;

private:
  bool checkValidityCond (bool mustBeTrue) noexcept;
  inline bool checkValidity () noexcept { return checkValidityCond(true); }

public:
  VV_DISABLE_COPY(VPartialStreamRO)

  // doesn't own passed stream
  VPartialStreamRO (VStream *ASrcStream, int astpos, int apartlen=-1, bool aOwnSrc=false);
  // allowClone:
  //   <0: never
  //    0: auto
  //   >0: always
  VPartialStreamRO (VStr aname, VStream *ASrcStream, int astpos, int apartlen, mythread_mutex *alockptr, int allowClone=0);
  // will free source stream if necessary
  virtual ~VPartialStreamRO () override;

  virtual bool IsFastSeek () const noexcept override;
  virtual void SetFastSeek (bool value) noexcept override;

  // stream interface
  virtual VStr GetName () const override;
  virtual bool IsError () const noexcept override;
  virtual void Serialise (void *Data, int Length) override;
  virtual void SerialiseBits (void *Data, int Length) override;
  virtual void SerialiseInt (vuint32 &Value/*, vuint32 Max*/) override;

  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual void Flush () override;
  // won't free stream
  virtual bool Close () override;

  // interface functions for objects and classes streams
  virtual void io (VName &) override;
  virtual void io (VStr &) override;
  virtual void io (VObject *&) override;
  virtual void io (VMemberBase *&) override;
  virtual void io (VSerialisable *&) override;

  virtual void SerialiseStructPointer (void *&Ptr, VStruct *Struct) override;
};


// ////////////////////////////////////////////////////////////////////////// //
typedef void (*VCheckedStreamAbortFn) (const char *msg) __attribute__((noreturn));

extern VCheckedStreamAbortFn VCheckedStreamAbortFnDefault; // set this to something if you want to use it instead of `Sys_Error()`

// owns srcstream
// will throw `Host_Error()` on any error
// should not be used with `new`
class VCheckedStream : public VStream {
public:
  // automatically copies streams without a fast seek
  enum CopyMode {
    VCStrmNoCopy = -1,
    VCStrmAutoCopy = 0,
    VCStrmForceCopy = 1,
  };

private:
  mutable VStream *srcStream;

public:
  VCheckedStreamAbortFn abortFn; // if `nullptr`, use default

public:
  __attribute__((noreturn)) void FatalError (const char *msg) const;

private:
  void checkError () const;
  void checkValidityCond (bool mustBeTrue);
  inline void checkValidity () { checkValidityCond(true); }

  void openStreamAndCopy (VStream *st, CopyMode mode);

public:
  VV_DISABLE_COPY(VCheckedStream)

  explicit VCheckedStream (VStream *ASrcStream); // this should not be used with `new`; does not copy
  // this seeks to 0
  explicit VCheckedStream (VStream *ASrcStream, CopyMode mode/*=VCStrmAutoCopy*/); // this should not be used with `new`
  explicit VCheckedStream (int LumpNum); // this should not be used with `new`; auto mode; doesn't use `Sys_Error()`
  explicit VCheckedStream (int LumpNum, bool aUseSysError); // this should not be used with `new`; auto mode
  virtual ~VCheckedStream () override;

  virtual bool IsFastSeek () const noexcept override;
  virtual void SetFastSeek (bool value) noexcept override;

  // stream interface
  virtual VStr GetName () const override;
  virtual bool IsError () const noexcept override; // this will call `Host_Error()`
  virtual void Serialise (void *Data, int Length) override;
  virtual void SerialiseBits (void *Data, int Length) override;
  virtual void SerialiseInt (vuint32 &Value/*, vuint32 Max*/) override;

  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual void Flush () override;
  virtual bool Close () override;

  // interface functions for objects and classes streams
  virtual void io (VName &) override;
  virtual void io (VStr &) override;
  virtual void io (VObject *&) override;
  virtual void io (VMemberBase *&) override;
  virtual void io (VSerialisable *&) override;

  virtual void SerialiseStructPointer (void *&Ptr, VStruct *Struct) override;
};
