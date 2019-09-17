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
//**  Copyright (C) 2018-2019 Ketmar Dark
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
int decodeVarIntLength (const vuint8 firstByte);

// returns decoded number; can consume up to 5 bytes
vuint32 decodeVarInt (const void *data);

// returns number of used bytes; can consume up to 5 bytes
int encodeVarInt (void *data, vuint32 n);


// ////////////////////////////////////////////////////////////////////////// //
// VObject knows how to serialize itself, others should inherit from this
class VSerialisable {
public:
  VSerialisable () {}
  virtual ~VSerialisable ();

  virtual void Serialise (VStream &) = 0;

  virtual VName GetClassName () = 0;
};


// ////////////////////////////////////////////////////////////////////////// //
// object i/o is using mappers internally, so let's make it explicit
class VStreamIOMapper {
public:
  VStreamIOMapper () {}
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
// base class for various streams
class VStream {
protected:
  bool bLoading; // are we loading or saving?
  bool bError; // did we have any errors?

public:
  VStreamIOMapper *Mapper;
  vuint16 version; // stream version; usually purely informational

public:
  VStream () : bLoading(true), bError(false), Mapper(nullptr), version(0) {}
  virtual ~VStream ();

  // status requests
  inline bool IsLoading () const { return bLoading;}
  inline bool IsSaving () const { return !bLoading; }
  //inline bool IsError () const { return bError; }
  virtual bool IsError () const;

  inline void Serialize (void *buf, int len) { Serialise(buf, len); }
  inline void Serialize (const void *buf, int len) { Serialise(buf, len); }
  void Serialise (const void *buf, int len); // only write

  // stream interface
  virtual VStr GetName () const;
  virtual void Serialise (void *Data, int Length);
  virtual void SerialiseBits (void *Data, int Length);
  virtual void SerialiseInt (vuint32 &Value/*, vuint32 Max*/);
  virtual void Seek (int);
  virtual int Tell ();
  virtual int TotalSize ();
  virtual bool AtEnd ();
  virtual void Flush ();
  virtual bool Close ();

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
};

// stream serialisation operators
// it is fuckin' impossible to do template constraints in shit-plus-plus, so fuck it
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, VName &v) { Strm.io(v); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, VStr &v) { Strm.io(v); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, VObject *&v) { Strm.io(v); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, VMemberBase *&v) { Strm.io(v); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, VSerialisable *&v) { Strm.io(v); return Strm; }

static inline __attribute__((unused)) VStream &operator << (VStream &Strm, vint8 &Val) { Strm.Serialise(&Val, 1); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, vuint8 &Val) { Strm.Serialise(&Val, 1); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, vint16 &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, vuint16 &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, vint32 &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, vuint32 &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, vint64 &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, vuint64 &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, float &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, double &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }

// writers
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, const VName &v) { vassert(!Strm.IsLoading()); Strm.io((VName &)v); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, const VStr &v) { vassert(!Strm.IsLoading()); Strm.io((VStr &)v); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, const VObject *&v) { vassert(!Strm.IsLoading()); Strm.io((VObject *&)v); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, const VMemberBase *&v) { vassert(!Strm.IsLoading()); Strm.io((VMemberBase *&)v); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, const VSerialisable *&v) { vassert(!Strm.IsLoading()); Strm.io((VSerialisable *&)v); return Strm; }

static inline __attribute__((unused)) VStream &operator << (VStream &Strm, const vint8 &Val) { vassert(!Strm.IsLoading()); Strm.Serialise((void *)&Val, 1); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, const vuint8 &Val) { vassert(!Strm.IsLoading()); Strm.Serialise((void *)&Val, 1); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, const vint16 &Val) { vassert(!Strm.IsLoading()); Strm.SerialiseLittleEndian((void *)&Val, sizeof(Val)); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, const vuint16 &Val) { vassert(!Strm.IsLoading()); Strm.SerialiseLittleEndian((void *)&Val, sizeof(Val)); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, const vint32 &Val) { vassert(!Strm.IsLoading()); Strm.SerialiseLittleEndian((void *)&Val, sizeof(Val)); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, const vuint32 &Val) { vassert(!Strm.IsLoading()); Strm.SerialiseLittleEndian((void *)&Val, sizeof(Val)); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, const vint64 &Val) { vassert(!Strm.IsLoading()); Strm.SerialiseLittleEndian((void *)&Val, sizeof(Val)); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, const vuint64 &Val) { vassert(!Strm.IsLoading()); Strm.SerialiseLittleEndian((void *)&Val, sizeof(Val)); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, const float &Val) { vassert(!Strm.IsLoading()); Strm.SerialiseLittleEndian((void *)&Val, sizeof(Val)); return Strm; }
static inline __attribute__((unused)) VStream &operator << (VStream &Strm, const double &Val) { vassert(!Strm.IsLoading()); Strm.SerialiseLittleEndian((void *)&Val, sizeof(Val)); return Strm; }


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
};
#define STRM_INDEX(val)  (*(VStreamCompactIndex *)&(val))

class VStreamCompactIndexU {
public:
  vuint32 Val;
  friend VStream &operator << (VStream &, VStreamCompactIndexU &);
};
#define STRM_INDEX_U(val)  (*(VStreamCompactIndexU *)&(val))
