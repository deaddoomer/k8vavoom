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
#include "../core.h"


// ////////////////////////////////////////////////////////////////////////// //
/*
if bit 7 of first byte is not set, this is one-byte number in range [0..127].
if bit 7 is set, this is encoded number in the following format:

  bits 5-6:
    0: this is 13-bit number in range [0..8191] (max: 0x1fff)
    1: this is 21-bit number in range [0..2097151] (max: 0x1fffff)
    2: this is 29-bit number in range [0..536870911] (max: 0x1fffffff)
    3: extended number, next 2 bits are used to specify format; result should be xored with -1

  bit 3-4 for type 3:
    0: this is 11-bit number in range [0..2047] (max: 0x7ff)
    1: this is 19-bit number in range [0..524287] (max: 0x7ffff)
    2: this is 27-bit number in range [0..134217727] (max: 0x7ffffff)
    3: read next 4 bytes as 32 bit number (other bits in this byte should be zero)
*/


// returns number of bytes required to decode full number, in range [1..5]
int decodeVarIntLength (const vuint8 firstByte) noexcept {
  if ((firstByte&0x80) == 0) {
    return 1;
  } else {
    // multibyte
    switch (firstByte&0x60) {
      case 0x00: return 2;
      case 0x20: return 3;
      case 0x40: return 4;
      case 0x60: // most complex format
        switch (firstByte&0x18) {
          case 0x00: return 2;
          case 0x08: return 3;
          case 0x10: return 4;
          case 0x18: return 5;
        }
    }
  }
  return -1; // the thing that should not be
}


// returns decoded number; can consume up to 5 bytes
vuint32 decodeVarInt (const void *data) noexcept {
  const vuint8 *buf = (const vuint8 *)data;
  if ((buf[0]&0x80) == 0) {
    return buf[0];
  } else {
    // multibyte
    switch (buf[0]&0x60) {
      case 0x00: return ((buf[0]&0x1f)<<8)|buf[1];
      case 0x20: return ((buf[0]&0x1f)<<16)|(buf[1]<<8)|buf[2];
      case 0x40: return ((buf[0]&0x1f)<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3];
      case 0x60: // most complex format
        switch (buf[0]&0x18) {
          case 0x00: return (vuint32)(((buf[0]&0x07)<<8)|buf[1])^0xffffffffU;
          case 0x08: return (vuint32)(((buf[0]&0x07)<<16)|(buf[1]<<8)|buf[2])^0xffffffffU;
          case 0x10: return (vuint32)(((buf[0]&0x07)<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3])^0xffffffffU;
          case 0x18: return (vuint32)((buf[1]<<24)|(buf[2]<<16)|(buf[3]<<8)|buf[4])^0xffffffffU;
        }
    }
  }
  return 0xffffffffU; // the thing that should not be
}


// returns number of used bytes; can consume up to 5 bytes
int encodeVarInt (void *data, vuint32 n) noexcept {
  vuint8 temp[8];
  vuint8 *buf = (vuint8 *)data;
  if (!buf) buf = temp;
  if (n <= 0x1fffffff) {
    // positive
    if (n <= 0x7f) {
      *buf = (vuint8)n;
      return 1;
    }
    if (n <= 0x1fff) {
      *buf++ = (vuint8)(n>>8)|0x80;
      *buf = (vuint8)(n&0xff);
      return 2;
    }
    if (n <= 0x1fffff) {
      *buf++ = (vuint8)(n>>16)|(0x80|0x20);
      *buf++ = (vuint8)((n>>8)&0xff);
      *buf = (vuint8)(n&0xff);
      return 3;
    }
    // invariant: n <= 0x1fffffff
    *buf++ = (vuint8)(n>>24)|(0x80|0x40);
    *buf++ = (vuint8)((n>>16)&0xff);
    *buf++ = (vuint8)((n>>8)&0xff);
    *buf = (vuint8)(n&0xff);
    return 4;
  } else {
    // either negative, or full 32 bits required; format 3
    // first, xor it
    n ^= 0xffffffffU;
    if (n <= 0x7ff) {
      *buf++ = (vuint8)(n>>8)|(0x80|0x60);
      *buf = (vuint8)(n&0xff);
      return 2;
    }
    if (n <= 0x7ffff) {
      *buf++ = (vuint8)(n>>16)|(0x80|0x60|0x08);
      *buf++ = (vuint8)((n>>8)&0xff);
      *buf = (vuint8)(n&0xff);
      return 3;
    }
    if (n <= 0x7ffffff) {
      *buf++ = (vuint8)(n>>24)|(0x80|0x60|0x10);
      *buf++ = (vuint8)((n>>16)&0xff);
      *buf++ = (vuint8)((n>>8)&0xff);
      *buf = (vuint8)(n&0xff);
      return 4;
    }
    // full 32 bits
    *buf++ = (vuint8)(0x80|0x60|0x18);
    *buf++ = (vuint8)((n>>24)&0xff);
    *buf++ = (vuint8)((n>>16)&0xff);
    *buf++ = (vuint8)((n>>8)&0xff);
    *buf = (vuint8)(n&0xff);
    return 5;
  }
}



//==========================================================================
//
//  VSerialisable::~VSerialisable
//
//==========================================================================
VSerialisable::~VSerialisable () {
}



//==========================================================================
//
//  VStreamIOMapper::~VStreamIOMapper
//
//==========================================================================
VStreamIOMapper::~VStreamIOMapper () {
}



//==========================================================================
//
//  VStreamIOStrMapper::~VStreamIOStrMapper
//
//==========================================================================
VStreamIOStrMapper::~VStreamIOStrMapper () {
}



//==========================================================================
//
//  VStream::~VStream
//
//==========================================================================
VStream::~VStream () {
}


//==========================================================================
//
//  VStream::IsFastSeek
//
//==========================================================================
bool VStream::IsFastSeek () const noexcept {
  return bFastSeek;
}


//==========================================================================
//
//  VStream::SetFastSeek
//
//==========================================================================
void VStream::SetFastSeek (bool value) noexcept {
  bFastSeek = value;
}


//==========================================================================
//
//  VStream::AttachStringMapper
//
//==========================================================================
void VStream::AttachStringMapper (VStreamIOStrMapper *amapper) {
  StrMapper = amapper;
}


//==========================================================================
//
//  VStream::IsExtendedFormat
//
//  this is used in VObject serialisers; default is `false`
//
//==========================================================================
bool VStream::IsExtendedFormat () const noexcept {
  return false;
}


//==========================================================================
//
//  VStream::OpenExtendedSection
//
//==========================================================================
bool VStream::OpenExtendedSection (VStr name, bool seekable) {
  return !IsError();
}


//==========================================================================
//
//  VStream::CloseExtendedSection
//
//==========================================================================
bool VStream::CloseExtendedSection () {
  return !IsError();
}


//==========================================================================
//
//  VStream::HasExtendedSection
//
//==========================================================================
bool VStream::HasExtendedSection (VStr name) {
  return false;
}


//==========================================================================
//
//  VStream::CloneSelf
//
//  not all streams can be cloned
//  returns `nullptr` if cannot (default option)
//
//==========================================================================
VStream *VStream::CloneSelf () {
  return nullptr;
}


//==========================================================================
//
//  VStream::GetName
//
//==========================================================================
VStr VStream::GetName () const {
  return VStr();
}


//==========================================================================
//
//  VStream::SetError
//
//==========================================================================
void VStream::SetError () {
  bError = true;
}


//==========================================================================
//
//  VStream::IsError
//
//==========================================================================
bool VStream::IsError () const noexcept {
  return bError;
}


//==========================================================================
//
//  VStream::Serialise
//
//==========================================================================
void VStream::Serialise (void *, int) {
  //__builtin_trap(); // k8: nope, we need dummy one
}


//==========================================================================
//
//  VStream::Serialise
//
//==========================================================================
void VStream::Serialise (const void *buf, int len) {
  if (!IsSaving()) Sys_Error("VStream::Serialise(const): expecting writer stream");
  Serialise((void *)buf, len);
}


//==========================================================================
//
//  VStream::SerialiseBits
//
//==========================================================================
void VStream::SerialiseBits (void *Data, int Length) {
  Serialise(Data, (Length+7)>>3);
  if (IsLoading() && (Length&7)) {
    ((vuint8*)Data)[Length>>3] &= (1<<(Length&7))-1;
  }
}


//==========================================================================
//
//  VStream::SerialiseInt
//
//==========================================================================
void VStream::SerialiseInt (vuint32 &Value/*, vuint32*/) {
  *this << Value;
}


//==========================================================================
//
//  VStream::Seek
//
//==========================================================================
void VStream::Seek (int offset) {
  if (bError) return;
  if (offset < 0 || offset != Tell()) { SetError(); return; }
}


//==========================================================================
//
//  VStream::Seek
//
//==========================================================================
int VStream::Tell () {
  SetError();
  return -1;
}


//==========================================================================
//
//  VStream::TotalSize
//
//==========================================================================
int VStream::TotalSize () {
  SetError();
  return -1;
}


//==========================================================================
//
//  VStream::Flush
//
//==========================================================================
void VStream::Flush () {
}


//==========================================================================
//
//  VStream::Close
//
//==========================================================================
bool VStream::Close () {
  return !bError;
}


//==========================================================================
//
//  VStream::AtEnd
//
//==========================================================================
bool VStream::AtEnd () {
  if (IsError()) return true;
  int Pos = Tell();
  return (!IsError() && Pos != -1 && Pos >= TotalSize());
}


//==========================================================================
//
//  VStream::io
//
//==========================================================================
void VStream::io (VName &v) {
  //__builtin_trap(); // k8: nope, we need dummy one
  if (Mapper) Mapper->io(v);
}


//==========================================================================
//
//  VStream::io
//
//==========================================================================
void VStream::io (VStr &s) {
  if (StrMapper) StrMapper->io(this, s);
  else if (Mapper) Mapper->io(s);
  else s.Serialise(*this);
}


//==========================================================================
//
//  VStream::io
//
//==========================================================================
void VStream::io (VObject *&v) {
  //__builtin_trap(); // k8: nope, we need dummy one
  if (Mapper) Mapper->io(v);
}


//==========================================================================
//
//  VStream::io
//
//==========================================================================
void VStream::io (VMemberBase *&v) {
  //__builtin_trap(); // k8: nope, we need dummy one
  if (Mapper) Mapper->io(v);
}


//==========================================================================
//
//  VStream::io
//
//==========================================================================
void VStream::io (VSerialisable *&v) {
  if (Mapper) Mapper->io(v); else __builtin_trap();
}


//==========================================================================
//
//  VStream::SerialiseStructPointer
//
//==========================================================================
void VStream::SerialiseStructPointer (void *&Ptr, VStruct *Struct) {
  if (Mapper) Mapper->SerialiseStructPointer(Ptr, Struct);
}


//==========================================================================
//
//  VStream::SerialisePointer
//
//==========================================================================
void VStream::SerialisePointer (void *&Ptr, const VFieldType &ptrtype) {
  if (Mapper) Mapper->SerialisePointer(Ptr, ptrtype);
}


//==========================================================================
//
//  VStream::SerialiseLittleEndian
//
//==========================================================================
void VStream::SerialiseLittleEndian (void *Val, int Len) {
#ifdef VAVOOM_BIG_ENDIAN
  // swap byte order
  for (int i = Len-1; i >= 0; --i) Serialise(((vuint8 *)Val)+i, 1);
#else
  // already in correct byte order
  Serialise(Val, Len);
#endif
}


//==========================================================================
//
//  VStream::SerialiseBigEndian
//
//==========================================================================
void VStream::SerialiseBigEndian (void *Val, int Len) {
#ifdef VAVOOM_LITTLE_ENDIAN
  // swap byte order
  for (int i = Len - 1; i >= 0; i--) Serialise(((vuint8 *)Val)+i, 1);
#else
  // already in correct byte order
  Serialise(Val, Len);
#endif
}


//==========================================================================
//
//  operator <<
//
//==========================================================================
VStream &operator << (VStream &Strm, VStreamCompactIndex &I) {
  vuint8 buf[5];
  if (Strm.IsLoading()) {
    Strm << buf[0];
    const int length = decodeVarIntLength(buf[0]);
    if (length > 1) Strm.Serialise(buf+1, length-1);
    I.Val = (vint32)decodeVarInt(buf);
  } else {
    const int length = encodeVarInt(buf, (vuint32)I.Val);
    Strm.Serialise(buf, length);
  }
  return Strm;
}


//==========================================================================
//
//  operator <<
//
//==========================================================================
VStream &operator << (VStream &Strm, VStreamCompactIndexU &I) {
  vuint8 buf[5];
  if (Strm.IsLoading()) {
    Strm << buf[0];
    const int length = decodeVarIntLength(buf[0]);
    if (length > 1) Strm.Serialise(buf+1, length-1);
    I.Val = decodeVarInt(buf);
  } else {
    const int length = encodeVarInt(buf, I.Val);
    Strm.Serialise(buf, length);
  }
  return Strm;
}


//==========================================================================
//
//  VStream::vawritef
//
//==========================================================================
void VStream::vawritef (const char *text, va_list ap) {
  /*static*/ const char *errorText = "ERROR CREATING STRING!";
  if (IsError()) return;

  char buf[512];
  va_list apcopy;

  va_copy(apcopy, ap);
  int size = vsnprintf(buf, sizeof(buf), text, apcopy);
  va_end(apcopy);

  if (size < 0) { Serialise(errorText, (int)strlen(errorText)); return; }

  if (size >= (int)sizeof(buf)-1) {
    char *dynbuf = (char *)Z_MallocNoClear(size+32);
    if (!dynbuf) { Serialise(errorText, (int)strlen(errorText)); return; }
    try {
      va_copy(apcopy, ap);
      size = vsnprintf(dynbuf, size+32, text, apcopy);
      va_end(apcopy);
      if (size < 0) { Serialise(errorText, (int)strlen(errorText)); return; }
      Serialise(dynbuf, size);
      Z_Free(dynbuf);
    } catch (...) {
      Z_Free(dynbuf);
      throw;
    }
  } else {
    Serialise(buf, size);
  }
}


//==========================================================================
//
//  VStream::writef
//
//==========================================================================
__attribute__((format(printf, 2, 3))) void VStream::writef (const char *text, ...) {
  if (IsError()) return;

  va_list ap;
  va_start(ap, text);
  vawritef(text, ap);
  va_end(ap);
}
