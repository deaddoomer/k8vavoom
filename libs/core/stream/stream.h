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
//**
//**  Streams for loading and saving data.
//**
//**************************************************************************
class VStr;
struct VNTValue;
class VStream;
class VFieldType;


// ////////////////////////////////////////////////////////////////////////// //
// variable-length integer codec

// returns number of bytes required to decode full number, in range [1..5]
int decodeVarIntLength (const vuint8 firstByte) noexcept;

// returns decoded number; can consume up to 5 bytes
vuint32 decodeVarInt (const void *data) noexcept;

// returns number of used bytes; can consume up to 5 bytes
// you can pass `nullptr` as `data` to calculate length
int encodeVarInt (void *data, vuint32 n) noexcept;

// alas, have to have it here
VVA_OKUNUSED static inline constexpr int calcVarIntLength (vuint32 n) noexcept {
  if (n <= 0x1fffffff) {
    // positive
    if (n <= 0x7f) return 1;
    if (n <= 0x1fff) return 2;
    if (n <= 0x1fffff) return 3;
    return 4;
  } else {
    // either negative, or full 32 bits required; format 3
    // first, xor it
    n ^= 0xffffffffU;
    if (n <= 0x7ff) return 2;
    if (n <= 0x7ffff) return 3;
    if (n <= 0x7ffffff) return 4;
    return 5;
  }
}

enum { MaxVarIntLength = 5 };


// ////////////////////////////////////////////////////////////////////////// //
// VObject knows how to serialize itself, others should inherit from this
class VSerialisable {
public:
  inline VSerialisable () {}
  virtual ~VSerialisable ();

  virtual void Serialise (VStream &) = 0;

  virtual VName GetClassName () = 0;
};


// ////////////////////////////////////////////////////////////////////////// //
// object i/o is using mappers internally, so let's make it explicit
class VStreamIOMapper {
public:
  VV_DISABLE_COPY(VStreamIOMapper)

  inline VStreamIOMapper () {}
  virtual ~VStreamIOMapper ();

  // interface functions for objects and classes streams
  virtual void io (VName &) = 0;
  virtual void io (VStr &) = 0;
  virtual void io (VObject *&) = 0;
  virtual void io (VMemberBase *&) = 0;
  virtual void io (VSerialisable *&) = 0;

  virtual void SerialiseStructPointer (void *&Ptr, VStruct *Struct) = 0;
  virtual void SerialisePointer (void *&Ptr, const VFieldType &ptrtype) = 0;
};


// ////////////////////////////////////////////////////////////////////////// //
class VStream;

// game saver needs to map strings too, but only strings
// this takes priority over the normal mapper
class VStreamIOStrMapper {
public:
  VV_DISABLE_COPY(VStreamIOStrMapper)

  inline VStreamIOStrMapper () {}
  virtual ~VStreamIOStrMapper ();

  // interface functions for objects and classes streams
  virtual void io (VStream *st, VStr &) = 0;
};


// ////////////////////////////////////////////////////////////////////////// //
// base class for various streams
class VStream {
protected:
  bool bLoading; // are we loading or saving?
  bool bError; // did we have any errors?
  bool bFastSeek;

public:
  enum { Seekable = true, NonSeekable = false };

public:
  VStreamIOMapper *Mapper;
  VStreamIOStrMapper *StrMapper;
  vuint16 version; // stream version; usually purely informational

public:
  VV_DISABLE_COPY(VStream)

  VVA_FORCEINLINE VStream ()
    : bLoading(true)
    , bError(false)
    , bFastSeek(true)
    , Mapper(nullptr)
    , StrMapper(nullptr)
    , version(0)
  {}

  virtual ~VStream ();

  // status requests
  VVA_FORCEINLINE VVA_CHECKRESULT VVA_PURE bool IsLoading () const noexcept { return bLoading; }
  VVA_FORCEINLINE VVA_CHECKRESULT VVA_PURE bool IsSaving () const noexcept { return !bLoading; }
  virtual bool IsError () const noexcept;

  virtual bool IsFastSeek () const noexcept;
  virtual void SetFastSeek (bool value) noexcept;

  // doesn't own the mapper
  // use `nullptr` to detach
  virtual void AttachStringMapper (VStreamIOStrMapper *amapper);

  // this is used in VObject serialisers; default is `false`
  virtual bool IsExtendedFormat () const noexcept;

  // extended serialisers can write data to separate sections.
  // for non-extended streams this is noop (and success).
  // the calls should be balanced. any extended section cannot
  // be opened twice (yet implementing stream may, or may not check this).
  // names are case-insensitive for ASCII (and only for ASCII).
  // if open of the section failed, implementing stream may or may not
  // be still valid (check with `IsError()`), but in any case you should
  // not call `CloseExtendedSection()` for failed open.
  // implementing streams should support at least 4 nested sections.
  // opening the same section twice for reading may not be an error.
  // opening the same section twice for writing is error.
  // returns success flag.
  virtual bool OpenExtendedSection (VStr name, bool seekable);

  // for non-extended streams this is noop (and success).
  // this may return error code too (due to flushing, for example).
  // even if error was returned, the section is still closed
  // (i.e. proper nesting maintained). it doesn't matter, of course,
  // if the stream itself became invalid (`IsError()` returns `true`).
  virtual bool CloseExtendedSection ();

  // opening non-existing section is error, hence this checker is here.
  // asking for empty name returns `false`.
  // non-extended streams always returns `false`.
  virtual bool HasExtendedSection (VStr name);

  // not all streams can be cloned; returns `nullptr` if cannot (default option)
  // CANNOT simply return `this`!
  virtual VStream *CloneSelf ();

  // call this to mark the stream as invalid
  virtual void SetError ();

  inline void Serialize (void *buf, int len) { Serialise(buf, len); }
  inline void Serialize (const void *buf, int len) { Serialise(buf, len); }
  void Serialise (const void *buf, int len); // only write

  // stream interface
  // note that `Tell()` and `TotalSize()` must be fast, so cache the values if necessary
  virtual VStr GetName () const;
  virtual void Serialise (void *Data, int Length);
  virtual void SerialiseBits (void *Data, int Length);
  virtual void SerialiseInt (vuint32 &Value/*, vuint32 Max*/);
  virtual void Seek (int offset); // sets error
  virtual int Tell (); // sets error
  virtual int TotalSize (); // sets error
  virtual bool AtEnd ();
  virtual void Flush ();

  // returns `false` on error
  // the rules are:
  //   * if you're calling this on a closed stream, it should return `bError`
  //   * if you're calling this on an errored stream, it can close it, but should retain `bError`
  //   * otherwise it should close the stream, may set `bError`, and will return `!bError`
  virtual bool Close (); // does nothing here

  // interface functions for objects and classes streams
  virtual void io (VName &);
  virtual void io (VStr &);
  virtual void io (VObject *&);
  virtual void io (VMemberBase *&);
  virtual void io (VSerialisable *&);

  virtual void SerialiseStructPointer (void *&Ptr, VStruct *Struct);
  virtual void SerialisePointer (void *&Ptr, const VFieldType &ptrtype);

  // serialise integers in particular byte order
  void SerialiseLittleEndian (void *Val, int Len);
  void SerialiseBigEndian (void *Val, int Len);

  void writef (const char *text, ...) __attribute__((format(printf, 2, 3)));
  void vawritef (const char *text, va_list ap);

  static VVA_FORCEINLINE bool Destroy (VStream *&s) {
    if (s) {
      const bool res = s->Close();
      delete s;
      s = nullptr;
      return res;
    } else {
      return true;
    }
  }
};

// stream serialisation operators
// it is fuckin' impossible to do template constraints in shit-plus-plus, so fuck it
static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, VName &v) { Strm.io(v); return Strm; }
static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, VStr &v) { Strm.io(v); return Strm; }
static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, VObject *&v) { Strm.io(v); return Strm; }
static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, VMemberBase *&v) { Strm.io(v); return Strm; }
static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, VSerialisable *&v) { Strm.io(v); return Strm; }

static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, vint8 &Val) { Strm.Serialise(&Val, 1); return Strm; }
static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, vuint8 &Val) { Strm.Serialise(&Val, 1); return Strm; }
static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, vint16 &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, vuint16 &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, vint32 &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, vuint32 &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, vint64 &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, vuint64 &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, float &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, double &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }

// writers
static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, const VName &v) { vassert(!Strm.IsLoading()); Strm.io((VName &)v); return Strm; }
static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, const VStr &v) { vassert(!Strm.IsLoading()); Strm.io((VStr &)v); return Strm; }
static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, const VObject *&v) { vassert(!Strm.IsLoading()); Strm.io((VObject *&)v); return Strm; }
static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, const VMemberBase *&v) { vassert(!Strm.IsLoading()); Strm.io((VMemberBase *&)v); return Strm; }
static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, const VSerialisable *&v) { vassert(!Strm.IsLoading()); Strm.io((VSerialisable *&)v); return Strm; }

static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, const vint8 &Val) { vassert(!Strm.IsLoading()); Strm.Serialise((void *)&Val, 1); return Strm; }
static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, const vuint8 &Val) { vassert(!Strm.IsLoading()); Strm.Serialise((void *)&Val, 1); return Strm; }
static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, const vint16 &Val) { vassert(!Strm.IsLoading()); Strm.SerialiseLittleEndian((void *)&Val, sizeof(Val)); return Strm; }
static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, const vuint16 &Val) { vassert(!Strm.IsLoading()); Strm.SerialiseLittleEndian((void *)&Val, sizeof(Val)); return Strm; }
static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, const vint32 &Val) { vassert(!Strm.IsLoading()); Strm.SerialiseLittleEndian((void *)&Val, sizeof(Val)); return Strm; }
static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, const vuint32 &Val) { vassert(!Strm.IsLoading()); Strm.SerialiseLittleEndian((void *)&Val, sizeof(Val)); return Strm; }
static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, const vint64 &Val) { vassert(!Strm.IsLoading()); Strm.SerialiseLittleEndian((void *)&Val, sizeof(Val)); return Strm; }
static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, const vuint64 &Val) { vassert(!Strm.IsLoading()); Strm.SerialiseLittleEndian((void *)&Val, sizeof(Val)); return Strm; }
static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, const float &Val) { vassert(!Strm.IsLoading()); Strm.SerialiseLittleEndian((void *)&Val, sizeof(Val)); return Strm; }
static inline VVA_OKUNUSED VStream &operator << (VStream &Strm, const double &Val) { vassert(!Strm.IsLoading()); Strm.SerialiseLittleEndian((void *)&Val, sizeof(Val)); return Strm; }


// ////////////////////////////////////////////////////////////////////////// //
// stream reader helper
template<class T> T Streamer (VStream &Strm) {
  T Val;
  Strm << Val;
  return Val;
}


// ////////////////////////////////////////////////////////////////////////// //
// class for serialising integer values in a compact way
// uses variable-int encoding
class VStreamCompactIndex {
public:
  vint32 Val;
  friend VStream &operator << (VStream &, VStreamCompactIndex &);
  static inline int CalcBytes (vint32 val) noexcept { return calcVarIntLength((vuint32)val); }
};
#define STRM_INDEX(val)  (*(VStreamCompactIndex *)&(val))
#define STRM_INDEX_BYTES(val)  (VStreamCompactIndex::CalcBytes(val))

class VStreamCompactIndexU {
public:
  vuint32 Val;
  friend VStream &operator << (VStream &, VStreamCompactIndexU &);
  static int CalcBytes (vuint32 val) noexcept { return calcVarIntLength((vuint32)val); }
};
#define STRM_INDEX_U(val)  (*(VStreamCompactIndexU *)&(val))
#define STRM_INDEX_U_BYTES(val)  (VStreamCompactIndexU::CalcBytes(val))
