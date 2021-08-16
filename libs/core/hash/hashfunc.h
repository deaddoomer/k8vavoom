//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2018-2021 Ketmar Dark
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
#ifndef VAVOOM_CORE_LIB_HASHFUNC
#define VAVOOM_CORE_LIB_HASHFUNC

#include "../common.h"


static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT inline int digitInBase (char ch, const int base=10) noexcept {
  if (base < 1 || base > 36 || ch < '0') return -1;
  if (base <= 10) return (ch < 48+base ? ch-48 : -1);
  if (ch >= '0' && ch <= '9') return ch-48;
  if (ch >= 'a' && ch <= 'z') ch -= 32; // poor man's tolower()
  if (ch < 'A' || ch >= 65+(base-10)) return -1;
  return ch-65+10;
}


static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT inline char upcase1251 (const char ch) noexcept {
  if ((vuint8)ch < 128) return ch-(ch >= 'a' && ch <= 'z' ? 32 : 0);
  if ((vuint8)ch >= 224 /*&& (vuint8)ch <= 255*/) return (vuint8)ch-32;
  if ((vuint8)ch == 184 || (vuint8)ch == 186 || (vuint8)ch == 191) return (vuint8)ch-16;
  if ((vuint8)ch == 162 || (vuint8)ch == 179) return (vuint8)ch-1;
  return ch;
}


static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT inline char locase1251 (const char ch) noexcept {
  if ((vuint8)ch < 128) return ch+(ch >= 'A' && ch <= 'Z' ? 32 : 0);
  if ((vuint8)ch >= 192 && (vuint8)ch <= 223) return (vuint8)ch+32;
  if ((vuint8)ch == 168 || (vuint8)ch == 170 || (vuint8)ch == 175) return (vuint8)ch+16;
  if ((vuint8)ch == 161 || (vuint8)ch == 178) return (vuint8)ch+1;
  return ch;
}


static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT inline vuint32 nextPOTU32 (const vuint32 x) noexcept {
  vuint32 res = x;
  res |= (res>>1);
  res |= (res>>2);
  res |= (res>>4);
  res |= (res>>8);
  res |= (res>>16);
  // already pot?
  if (x != 0 && (x&(x-1)) == 0) res &= ~(res>>1); else ++res;
  return res;
}


static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT inline vuint32 hashU32 (vuint32 a) noexcept {
  a -= (a<<6);
  a = a^(a>>17);
  a -= (a<<9);
  a = a^(a<<4);
  a -= (a<<3);
  a = a^(a<<10);
  a = a^(a>>15);
  return a;
}

// https://github.com/skeeto/hash-prospector
// bias = 0.023840118344741465
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT vuint16 hashU16 (vuint16 x) noexcept {
  x += x<<7; x ^= x>>8;
  x += x<<3; x ^= x>>2;
  x += x<<4; x ^= x>>8;
  return x;
}

// https://github.com/skeeto/hash-prospector
// [16 0x7feb352du 15 0x846ca68bu 16] bias: 0.17353355999581582
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT inline vuint32 permuteU32 (vuint32 x) noexcept {
  x ^= x>>16;
  x *= 0x7feb352du;
  x ^= x>>15;
  x *= 0x846ca68bu;
  x ^= x>>16;
  return x;
}

// https://github.com/skeeto/hash-prospector
// inverse
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT inline vuint32 permuteU32Inv (vuint32 x) noexcept {
  x ^= x>>16;
  x *= 0x43021123u;
  x ^= x>>15^x>>30;
  x *= 0x1d69e2a5u;
  x ^= x>>16;
  return x;
}


// fnv-1a: http://www.isthe.com/chongo/tech/comp/fnv/
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT inline vuint32 fnvHashBufCI (const void *buf, size_t len) noexcept {
  vuint32 hash = 2166136261U; // fnv offset basis
  const vuint8 *s = (const vuint8 *)buf;
  while (len--) {
    hash ^= (vuint8)locase1251(*s++);
    hash *= 16777619U; // 32-bit fnv prime
  }
  return hash;
}


// fnv-1a: http://www.isthe.com/chongo/tech/comp/fnv/
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT inline vuint32 fnvHashBuf (const void *buf, size_t len) noexcept {
  vuint32 hash = 2166136261U; // fnv offset basis
  if (len) {
    const vuint8 *s = (const vuint8 *)buf;
    while (len--) {
      hash ^= *s++;
      hash *= 16777619U; // 32-bit fnv prime
    }
  }
  return hash;
}


// fnv-1a: http://www.isthe.com/chongo/tech/comp/fnv/
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT inline vuint32 fnvHashStr (const void *buf) noexcept {
  vuint32 hash = 2166136261U; // fnv offset basis
  if (buf) {
    const vuint8 *s = (const vuint8 *)buf;
    while (*s) {
      hash ^= *s++;
      hash *= 16777619U; // 32-bit fnv prime
    }
  }
  return hash;
}


// fnv-1a: http://www.isthe.com/chongo/tech/comp/fnv/
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT inline vuint32 fnvHashStrCI (const void *buf) noexcept {
  vuint32 hash = 2166136261U; // fnv offset basis
  if (buf) {
    const vuint8 *s = (const vuint8 *)buf;
    while (*s) {
      hash ^= (vuint8)locase1251(*s++);
      hash *= 16777619U; // 32-bit fnv prime
    }
  }
  return hash;
}


// djb
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT inline vuint32 djbHashBufCI (const void *buf, size_t len) noexcept {
  vuint32 hash = 5381;
  const vuint8 *s = (const vuint8 *)buf;
  while (len--) hash = ((hash<<5)+hash)+(vuint8)locase1251(*s++);
  return hash;
}


// djb
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT inline vuint32 djbHashBuf (const void *buf, size_t len) noexcept {
  vuint32 hash = 5381;
  const vuint8 *s = (const vuint8 *)buf;
  while (len-- > 0) hash = ((hash<<5)+hash)+(*s++);
  return hash;
}


static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT inline vuint32 joaatHashBuf (const void *buf, size_t len, vuint32 seed=0) noexcept {
  vuint32 hash = seed;
  const vuint8 *s = (const vuint8 *)buf;
  while (len--) {
    hash += *s++;
    hash += hash<<10;
    hash ^= hash>>6;
  }
  // finalize
  hash += hash<<3;
  hash ^= hash>>11;
  hash += hash<<15;
  return hash;
}


static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT inline vuint32 joaatHashBufCI (const void *buf, size_t len, vuint32 seed=0) noexcept {
  vuint32 hash = seed;
  const vuint8 *s = (const vuint8 *)buf;
  while (len--) {
    hash += (vuint8)locase1251(*s++);
    hash += hash<<10;
    hash ^= hash>>6;
  }
  // finalize
  hash += hash<<3;
  hash ^= hash>>11;
  hash += hash<<15;
  return hash;
}


inline VVA_PURE VVA_CHECKRESULT vuint32 GetTypeHash (int n) noexcept { return hashU32((vuint32)n); }
inline VVA_PURE VVA_CHECKRESULT vuint32 GetTypeHash (vuint32 n) noexcept { return hashU32(n); }
inline VVA_PURE VVA_CHECKRESULT vuint32 GetTypeHash (vuint64 n) noexcept { return hashU32((vuint32)n)^hashU32((vuint32)(n>>32)); }
inline VVA_PURE VVA_CHECKRESULT vuint32 GetTypeHash (const void *n) noexcept { return GetTypeHash((uintptr_t)n); }

#endif
