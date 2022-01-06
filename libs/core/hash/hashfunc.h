//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2018-2022 Ketmar Dark
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
#include <string.h>


static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT inline int digitInBase (char ch, const int base=10) noexcept {
  if (base < 1 || base > 36 || ch < '0') return -1;
  if (base <= 10) return (ch < 48+base ? ch-48 : -1);
  if (ch >= '0' && ch <= '9') return ch-48;
  if (ch >= 'a' && ch <= 'z') ch -= 32; // poor man's tolower()
  if (ch < 'A' || ch >= 65+(base-10)) return -1;
  return ch-65+10;
}


static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT inline char upcase1251 (const char ch) noexcept {
  if ((uint8_t)ch < 128) return ch-(ch >= 'a' && ch <= 'z' ? 32 : 0);
  if ((uint8_t)ch >= 224 /*&& (uint8_t)ch <= 255*/) return (uint8_t)ch-32;
  if ((uint8_t)ch == 184 || (uint8_t)ch == 186 || (uint8_t)ch == 191) return (uint8_t)ch-16;
  if ((uint8_t)ch == 162 || (uint8_t)ch == 179) return (uint8_t)ch-1;
  return ch;
}


static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT inline char locase1251 (const char ch) noexcept {
  if ((uint8_t)ch < 128) return ch+(ch >= 'A' && ch <= 'Z' ? 32 : 0);
  if ((uint8_t)ch >= 192 && (uint8_t)ch <= 223) return (uint8_t)ch+32;
  if ((uint8_t)ch == 168 || (uint8_t)ch == 170 || (uint8_t)ch == 175) return (uint8_t)ch+16;
  if ((uint8_t)ch == 161 || (uint8_t)ch == 178) return (uint8_t)ch+1;
  return ch;
}


// if `x` is already POT, returns `x`
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT inline uint32_t nextPOTU32 (const uint32_t x) noexcept {
  uint32_t res = x;
  res |= (res>>1);
  res |= (res>>2);
  res |= (res>>4);
  res |= (res>>8);
  res |= (res>>16);
  // already pot?
  if (x != 0 && (x&(x-1)) == 0) res &= ~(res>>1); else ++res;
  return res;
}


static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT inline uint32_t hashU32 (uint32_t a) noexcept {
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
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT uint16_t hashU16 (uint16_t x) noexcept {
  x += x<<7; x ^= x>>8;
  x += x<<3; x ^= x>>2;
  x += x<<4; x ^= x>>8;
  return x;
}

// https://github.com/skeeto/hash-prospector
// [16 0x7feb352du 15 0x846ca68bu 16] bias: 0.17353355999581582
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT inline uint32_t permuteU32 (uint32_t x) noexcept {
  x ^= x>>16;
  x *= 0x7feb352du;
  x ^= x>>15;
  x *= 0x846ca68bu;
  x ^= x>>16;
  return x;
}

// https://github.com/skeeto/hash-prospector
// inverse
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT inline uint32_t permuteU32Inv (uint32_t x) noexcept {
  x ^= x>>16;
  x *= 0x43021123u;
  x ^= x>>15^x>>30;
  x *= 0x1d69e2a5u;
  x ^= x>>16;
  return x;
}


// fnv-1a: http://www.isthe.com/chongo/tech/comp/fnv/
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT inline uint32_t fnvHashBufCI (const VVA_MAYALIAS void *buf, size_t len) noexcept {
  uint32_t hash = 2166136261U; // fnv offset basis
  const VVA_MAYALIAS uint8_t *s = (const uint8_t *)buf;
  while (len--) {
    //hash ^= (uint8_t)locase1251(*s++);
    hash ^= (*s++)|0x20; // this converts ASCII capitals to locase (and destroys other, but who cares)
    hash *= 16777619U; // 32-bit fnv prime
  }
  return hash;
}


// fnv-1a: http://www.isthe.com/chongo/tech/comp/fnv/
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT inline uint32_t fnvHashBuf (const VVA_MAYALIAS void *buf, size_t len) noexcept {
  uint32_t hash = 2166136261U; // fnv offset basis
  if (len) {
    const VVA_MAYALIAS uint8_t *s = (const uint8_t *)buf;
    while (len--) {
      hash ^= *s++;
      hash *= 16777619U; // 32-bit fnv prime
    }
  }
  return hash;
}


// fnv-1a: http://www.isthe.com/chongo/tech/comp/fnv/
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT inline uint32_t fnvHashStr (const VVA_MAYALIAS void *buf) noexcept {
  uint32_t hash = 2166136261U; // fnv offset basis
  if (buf) {
    const VVA_MAYALIAS uint8_t *s = (const uint8_t *)buf;
    while (*s) {
      hash ^= *s++;
      hash *= 16777619U; // 32-bit fnv prime
    }
  }
  return hash;
}


// fnv-1a: http://www.isthe.com/chongo/tech/comp/fnv/
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT inline uint32_t fnvHashStrCI (const VVA_MAYALIAS void *buf) noexcept {
  uint32_t hash = 2166136261U; // fnv offset basis
  if (buf) {
    const VVA_MAYALIAS uint8_t *s = (const uint8_t *)buf;
    while (*s) {
      //hash ^= (uint8_t)locase1251(*s++);
      hash ^= (*s++)|0x20; // this converts ASCII capitals to locase (and destroys other, but who cares)
      hash *= 16777619U; // 32-bit fnv prime
    }
  }
  return hash;
}


// djb
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT inline uint32_t djbHashBufCI (const VVA_MAYALIAS void *buf, size_t len) noexcept {
  uint32_t hash = 5381;
  const VVA_MAYALIAS uint8_t *s = (const uint8_t *)buf;
  //while (len--) hash = ((hash<<5)+hash)+(uint8_t)locase1251(*s++);
  while (len--) hash = ((hash<<5)+hash)+((*s++)|0x20); // this converts ASCII capitals to locase (and destroys other, but who cares)
  return hash;
}


// djb
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT inline uint32_t djbHashBuf (const VVA_MAYALIAS void *buf, size_t len) noexcept {
  uint32_t hash = 5381;
  const VVA_MAYALIAS uint8_t *s = (const uint8_t *)buf;
  while (len-- > 0) hash = ((hash<<5)+hash)+(*s++);
  return hash;
}


static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT inline uint32_t joaatHashBuf (const VVA_MAYALIAS void *buf, size_t len, uint32_t seed=0u) noexcept {
  uint32_t hash = seed;
  const VVA_MAYALIAS uint8_t *s = (const uint8_t *)buf;
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


static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT inline uint32_t joaatHashBufCI (const VVA_MAYALIAS void *buf, size_t len, uint32_t seed=0u) noexcept {
  uint32_t hash = seed;
  const VVA_MAYALIAS uint8_t *s = (const uint8_t *)buf;
  while (len--) {
    //hash += (uint8_t)locase1251(*s++);
    hash += (*s++)|0x20; // this converts ASCII capitals to locase (and destroys other, but who cares)
    hash += hash<<10;
    hash ^= hash>>6;
  }
  // finalize
  hash += hash<<3;
  hash ^= hash>>11;
  hash += hash<<15;
  return hash;
}


// ////////////////////////////////////////////////////////////////////////// //
// Bob Jenkins' LOOKUP3
// it won't return the same results for big-endian machines, but i don't care

// hash `count` 32-bit integers
VVA_CHECKRESULT uint32_t bjHashU32v (
  const VVA_MAYALIAS uint32_t *k, /* the key, an array of uint32_t values */
  size_t count, /* the length of the key, in uint32_ts */
  uint32_t initval=0u) noexcept; /* the previous hash, or an arbitrary value */

// hash `count` 32-bit integers into 2 32-bit values
void bjHashU32v64P (
  const VVA_MAYALIAS uint32_t *k, /* the key, an array of uint32_t values */
  size_t count, /* the length of the key, in uint32_ts */
  uint32_t *pc, /* IN: seed OUT: primary hash value */
  uint32_t *pb) noexcept; /* IN: more seed OUT: secondary hash value */

VVA_CHECKRESULT uint32_t bjHashBuf (const VVA_MAYALIAS void *key, size_t length, uint32_t initval=0u) noexcept;

void bjHashBuf64P (
  const VVA_MAYALIAS void *key, /* the key to hash */
  size_t length, /* length of the key */
  uint32_t *pc, /* IN: primary initval, OUT: primary hash */
  uint32_t *pb) noexcept; /* IN: secondary initval, OUT: secondary hash */

VVA_CHECKRESULT uint64_t bjHashBuf64 (const VVA_MAYALIAS void *key, size_t length, uint64_t initval=0u) noexcept;
VVA_CHECKRESULT uint64_t bjHashU32v64 (const VVA_MAYALIAS uint32_t *k, size_t count, uint64_t initval=0u) noexcept;

// hash one 32-bit value
VVA_CHECKRESULT uint32_t bjHashU32 (uint32_t k, uint32_t initval=0u) noexcept;
// hash one 32-bit value into two 32-bit values
void bjHashU3264P (const uint32_t k, uint32_t *pc, uint32_t *pb) noexcept;

// hash one 32-bit value into two 32-bit values
VVA_CHECKRESULT uint64_t bjHashU3264 (const uint32_t k, uint64_t initval=0u) noexcept;


static VVA_FORCEINLINE VVA_OKUNUSED VVA_CHECKRESULT uint32_t bjHashStr (const VVA_MAYALIAS void *str) noexcept { return (str ? bjHashBuf(str, strlen((const char *)str)) : 0u); }


// ////////////////////////////////////////////////////////////////////////// //
// half-sip
VVA_CHECKRESULT uint32_t halfsip24HashBuf (const VVA_MAYALIAS void *in, const size_t inlen, const uint32_t seed=0u) noexcept;
VVA_CHECKRESULT uint32_t halfsip24HashBufCI (const VVA_MAYALIAS void *in, const size_t inlen, const uint32_t seed=0u) noexcept;

static VVA_FORCEINLINE VVA_OKUNUSED VVA_CHECKRESULT uint32_t halfsip24HashStr (const VVA_MAYALIAS void *str) noexcept { return (str ? halfsip24HashBuf(str, strlen((const char *)str)) : 0u); }
static VVA_FORCEINLINE VVA_OKUNUSED VVA_CHECKRESULT uint32_t halfsip24HashStrCI (const VVA_MAYALIAS void *str) noexcept { return (str ? halfsip24HashBufCI(str, strlen((const char *)str)) : 0u); }


// ////////////////////////////////////////////////////////////////////////// //
// murmur3
VVA_CHECKRESULT uint32_t murmurHash3Buf (const VVA_MAYALIAS void *key, size_t len, uint32_t seed) noexcept;

#define MURMUR3_128_HASH_SIZE  16

// returns 16-byte hash value in `out`
void murmurHash3Buf128 (const VVA_MAYALIAS void *key, const size_t len, uint32_t seed, VVA_MAYALIAS void *out) noexcept;

// incremental MurMur3
typedef struct {
  uint32_t h1; // current hash value
  uint32_t buf32;
  uint32_t bufused; // 32 bits, for faster access
  size_t totalbytes; // will be used in finalizer
} Murmur3Ctx;

// it is faster to update it with dwords (so it won't have to accumulate anything)
void murmurHash3Init (Murmur3Ctx *ctx, uint32_t seed) noexcept;
void murmurHash3Update (Murmur3Ctx *ctx, const VVA_MAYALIAS void *key, size_t len) noexcept;
// note that you can keep using the context for updating after calling `murmurHash3Final()`
uint32_t murmurHash3Final (const Murmur3Ctx *ctx) noexcept;


// ////////////////////////////////////////////////////////////////////////// //
VVA_FORCEINLINE VVA_PURE VVA_CHECKRESULT uint32_t GetTypeHash (const uint8_t n) noexcept { return hashU32((const uint32_t)n); }
VVA_FORCEINLINE VVA_PURE VVA_CHECKRESULT uint32_t GetTypeHash (const uint16_t n) noexcept { return hashU32((const uint32_t)n); }
VVA_FORCEINLINE VVA_PURE VVA_CHECKRESULT uint32_t GetTypeHash (const uint32_t n) noexcept { return hashU32(n); }
VVA_FORCEINLINE VVA_PURE VVA_CHECKRESULT uint32_t GetTypeHash (const uint64_t n) noexcept { return hashU32((const uint32_t)n)+hashU32((const uint32_t)(n>>32)); }
VVA_FORCEINLINE VVA_PURE VVA_CHECKRESULT uint32_t GetTypeHash (const int8_t n) noexcept { return hashU32((const uint32_t)n); }
VVA_FORCEINLINE VVA_PURE VVA_CHECKRESULT uint32_t GetTypeHash (const int16_t n) noexcept { return hashU32((const uint32_t)n); }
VVA_FORCEINLINE VVA_PURE VVA_CHECKRESULT uint32_t GetTypeHash (const int32_t n) noexcept { return hashU32((const uint32_t)n); }
VVA_FORCEINLINE VVA_PURE VVA_CHECKRESULT uint32_t GetTypeHash (const int64_t n) noexcept { return hashU32((const uint32_t)n)+hashU32((const uint32_t)(n>>32)); }
VVA_FORCEINLINE VVA_PURE VVA_CHECKRESULT uint32_t GetTypeHash (const void *n) noexcept { return GetTypeHash((uintptr_t)n); }


// use `halfsip24HashBufCI()` for case-insensitive hashes
// it is mostly used for strings, and strings are 4-byte aligned, and it reads data by 4 bytes

// defines, so we can easily switch hash functions
//#define vvHashStrZ    fnvHashStr
//#define vvHashStrZCI  fnvHashStrCI

//#define vvHashStrZ    bjHashStr
//#define vvHashStrZCI  fnvHashStrCI

#define vvHashStrZ    bjHashStr
#define vvHashStrZCI  halfsip24HashStrCI


// used in cvarsys
//#define vvHashBufCI   fnvHashBufCI

#define vvHashBufCI   halfsip24HashBufCI


#endif
