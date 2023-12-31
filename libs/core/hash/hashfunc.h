//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
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


// ////////////////////////////////////////////////////////////////////////// //
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


// ////////////////////////////////////////////////////////////////////////// //
// 16-bit integer hashes
// https://github.com/skeeto/hash-prospector
// bias = 0.023840118344741465
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT uint16_t hashU16 (uint16_t x) noexcept {
  x += x<<7; x ^= x>>8;
  x += x<<3; x ^= x>>2;
  x += x<<4; x ^= x>>8;
  return x;
}


// ////////////////////////////////////////////////////////////////////////// //
// 32-bit integer hashes
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT inline uint32_t hashU32bj (uint32_t a) noexcept {
  a -= (a<<6);
  a ^= (a>>17);
  a -= (a<<9);
  a ^= (a<<4);
  a -= (a<<3);
  a ^= (a<<10);
  a ^= (a>>15);
  return a;
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


// // [16 21f0aaad 15 d35a2d97 15] = 0.10760229515479501
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT inline uint32_t hash32UPro107 (uint32_t x) noexcept {
  x ^= x >> 16;
  x *= 0x21f0aaadU;
  x ^= x >> 15;
  x *= 0xd35a2d97U;
  x ^= x >> 15;
  return x;
}


// exact bias: 0.020888578919738908
//It's statistically indistinguishable from a random permutation of all 32-bit integers.
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT inline uint32_t hash32UPro3 (uint32_t x) noexcept {
  x ^= x >> 17;
  x *= 0xed5ad4bbU;
  x ^= x >> 11;
  x *= 0xac4c1b51U;
  x ^= x >> 15;
  x *= 0x31848babU;
  x ^= x >> 14;
  return x;
}


static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT inline uint32_t hash32Ubj3 (uint32_t x) noexcept {
  #define BJX_ROT(x,k) (((x)<<(k))|((x)>>(32-(k))))
  uint32_t a, b, c;
  a = b = c = 0xdeadbeefU+(((uint32_t)1u/*count*/)<<2)+/*initval*/0x29aU;
  a += x;
  // final(a, b, c)
  c ^= b; c -= BJX_ROT(b,14);
  a ^= c; a -= BJX_ROT(c,11);
  b ^= a; b -= BJX_ROT(a,25);
  c ^= b; c -= BJX_ROT(b,16);
  a ^= c; a -= BJX_ROT(c, 4);
  b ^= a; b -= BJX_ROT(a,14);
  c ^= b; c -= BJX_ROT(b,24);
  //
  return c;
  #undef BJX_ROT
}


// ////////////////////////////////////////////////////////////////////////// //
// fnv-1a: http://www.isthe.com/chongo/tech/comp/fnv/
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT inline uint32_t fnvHashBufCI (const VVA_MAYALIAS void *buf, size_t len) noexcept {
  uint32_t hash = 2166136261U; // fnv offset basis
  const VVA_MAYALIAS uint8_t *s = (const uint8_t *)buf;
  while (len--) {
    //hash ^= (uint8_t)locase1251(*s++);
    hash ^= (*s++)|0x20; // this converts ASCII capitals to locase (and destroys other, but who cares)
    hash *= 16777619U; // 32-bit fnv prime
  }
  // better avalanche
  hash += hash << 13;
  hash ^= hash >> 7;
  hash += hash << 3;
  hash ^= hash >> 17;
  hash += hash << 5;
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
  // better avalanche
  hash += hash << 13;
  hash ^= hash >> 7;
  hash += hash << 3;
  hash ^= hash >> 17;
  hash += hash << 5;
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
  // better avalanche
  hash += hash << 13;
  hash ^= hash >> 7;
  hash += hash << 3;
  hash ^= hash >> 17;
  hash += hash << 5;
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
  // better avalanche
  hash += hash << 13;
  hash ^= hash >> 7;
  hash += hash << 3;
  hash ^= hash >> 17;
  hash += hash << 5;
  return hash;
}


// ////////////////////////////////////////////////////////////////////////// //
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


// ////////////////////////////////////////////////////////////////////////// //
// joaat
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


static VVA_FORCEINLINE VVA_PURE uint32_t joaatHashPart (uint32_t hash, const VVA_MAYALIAS void *buf, size_t len) {
  const VVA_MAYALIAS uint8_t *s = (const uint8_t *)buf;
  while (len--) {
    hash += *s++;
    hash += hash<<10;
    hash ^= hash>>6;
  }
  return hash;
}

static VVA_FORCEINLINE VVA_PURE uint32_t joaatHashFinalize (uint32_t hash) {
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
  uint32_t initval=0x29aU) noexcept; /* the previous hash, or an arbitrary value */

// hash `count` 32-bit integers into 2 32-bit values
void bjHashU32v64P (
  const VVA_MAYALIAS uint32_t *k, /* the key, an array of uint32_t values */
  size_t count, /* the length of the key, in uint32_ts */
  uint32_t *pc, /* IN: seed OUT: primary hash value */
  uint32_t *pb) noexcept; /* IN: more seed OUT: secondary hash value */

VVA_CHECKRESULT uint32_t bjHashBuf (const VVA_MAYALIAS void *key, size_t length, uint32_t initval=0x29aU) noexcept;
VVA_CHECKRESULT uint32_t bjHashBufCI (const __attribute__((__may_alias__)) void *key, size_t length, uint32_t initval=0x29aU) noexcept;

static VVA_FORCEINLINE VVA_PURE VVA_CHECKRESULT
uint32_t bjHashStr (const VVA_MAYALIAS void *str) noexcept {
  return (str ? bjHashBuf(str, strlen((const char *)str)) : 0u);
}

static VVA_FORCEINLINE VVA_PURE VVA_CHECKRESULT
uint32_t bjHashStrCI (const VVA_MAYALIAS void *str) noexcept {
  return (str ? bjHashBufCI(str, strlen((const char *)str)) : 0u);
}

void bjHashBuf64P (
  const VVA_MAYALIAS void *key, /* the key to hash */
  size_t length, /* length of the key */
  uint32_t *pc, /* IN: primary initval, OUT: primary hash */
  uint32_t *pb) noexcept; /* IN: secondary initval, OUT: secondary hash */

VVA_CHECKRESULT uint64_t bjHashBuf64 (const VVA_MAYALIAS void *key, size_t length, uint64_t initval=0x29aU) noexcept;
VVA_CHECKRESULT uint64_t bjHashU32v64 (const VVA_MAYALIAS uint32_t *k, size_t count, uint64_t initval=0x29aU) noexcept;

// hash one 32-bit value
VVA_CHECKRESULT uint32_t bjHashU32 (uint32_t k, uint32_t initval=0x29aU) noexcept;
// hash one 32-bit value into two 32-bit values
void bjHashU3264P (const uint32_t k, uint32_t *pc, uint32_t *pb) noexcept;

// hash one 32-bit value into two 32-bit values
VVA_CHECKRESULT uint64_t bjHashU3264 (const uint32_t k, uint64_t initval=0x29aU) noexcept;


// ////////////////////////////////////////////////////////////////////////// //
// half-sip
VVA_CHECKRESULT uint32_t halfsip24HashBuf (const VVA_MAYALIAS void *in, const size_t inlen, const uint32_t seed=0u) noexcept;
VVA_CHECKRESULT uint32_t halfsip24HashBufCI (const VVA_MAYALIAS void *in, const size_t inlen, const uint32_t seed=0u) noexcept;

static VVA_FORCEINLINE VVA_OKUNUSED VVA_CHECKRESULT uint32_t halfsip24HashStr (const VVA_MAYALIAS void *str) noexcept { return (str ? halfsip24HashBuf(str, strlen((const char *)str)) : 0u); }
static VVA_FORCEINLINE VVA_OKUNUSED VVA_CHECKRESULT uint32_t halfsip24HashStrCI (const VVA_MAYALIAS void *str) noexcept { return (str ? halfsip24HashBufCI(str, strlen((const char *)str)) : 0u); }


// ////////////////////////////////////////////////////////////////////////// //
// murmur3
VVA_CHECKRESULT uint32_t murmurHash3Buf (const VVA_MAYALIAS void *key, size_t len, uint32_t seed=0x29aU) noexcept;
VVA_CHECKRESULT uint32_t murmurHash3BufCI (const VVA_MAYALIAS void *key, size_t len, uint32_t seed=0x29aU) noexcept;

static VVA_FORCEINLINE VVA_PURE VVA_CHECKRESULT
uint32_t murmurHash3Str (const VVA_MAYALIAS void *str) noexcept {
  return (str ? murmurHash3Buf(str, strlen((const char *)str)) : 0u);
}

static VVA_FORCEINLINE VVA_PURE VVA_CHECKRESULT
uint32_t murmurHash3StrCI (const VVA_MAYALIAS void *str) noexcept {
  return (str ? murmurHash3BufCI(str, strlen((const char *)str)) : 0u);
}


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
// bj's lookup2
uint32_t bjLookup2Buf (const void *data, size_t length, uint32_t initval=0x29aU);
uint32_t bjLookup2BufCI (const void *data, size_t length, uint32_t initval=0x29aU);


static VVA_FORCEINLINE VVA_PURE VVA_CHECKRESULT
uint32_t bjLookup2HashStr (const VVA_MAYALIAS void *str) noexcept {
  return (str ? bjLookup2Buf(str, strlen((const char *)str)) : 0u);
}

static VVA_FORCEINLINE VVA_PURE VVA_CHECKRESULT
uint32_t bjLookup2HashStrCI (const VVA_MAYALIAS void *str) noexcept {
  return (str ? bjLookup2BufCI(str, strlen((const char *)str)) : 0u);
}


// ////////////////////////////////////////////////////////////////////////// //
// hash functin to use with 32-bit integers
// Bob Jenkins' hash, no muls
//#define hashU32     hashU32bj
// prospector, 2 muls
//#define hashU32     hash32UPro107
// prospector, 3 muls
//#define hashU32     hash32UPro3
// BJ's lookup3, quite heavy, but meh
#define hashU32     hash32Ubj3


// ////////////////////////////////////////////////////////////////////////// //
// used for hash tables
VVA_FORCEINLINE VVA_PURE VVA_CHECKRESULT uint32_t GetTypeHash (const uint8_t n) noexcept { return hashU32((const uint32_t)n); }
VVA_FORCEINLINE VVA_PURE VVA_CHECKRESULT uint32_t GetTypeHash (const uint16_t n) noexcept { return hashU16((const uint16_t)n); }
VVA_FORCEINLINE VVA_PURE VVA_CHECKRESULT uint32_t GetTypeHash (const uint32_t n) noexcept { return hashU32((const uint32_t)n); }
VVA_FORCEINLINE VVA_PURE VVA_CHECKRESULT uint32_t GetTypeHash (const uint64_t n) noexcept { return hashU32((const uint32_t)n)+hashU32((const uint32_t)(n>>32)); }
VVA_FORCEINLINE VVA_PURE VVA_CHECKRESULT uint32_t GetTypeHash (const int8_t n) noexcept { return hashU32((const uint32_t)n); }
VVA_FORCEINLINE VVA_PURE VVA_CHECKRESULT uint32_t GetTypeHash (const int16_t n) noexcept { return hashU16((const uint16_t)n); }
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

//#define vvHashStrZ    bjHashStr
//#define vvHashStrZCI  halfsip24HashStrCI

//#define vvHashStrZ    bjLookup2HashStr
//#define vvHashStrZCI  bjLookup2HashStrCI

#define vvHashStrZ    murmurHash3Str
#define vvHashStrZCI  murmurHash3StrCI

//#define vvHashStrZ    bjHashStr
//#define vvHashStrZCI  bjHashStrCI


// used in cvarsys
//#define vvHashBufCI   fnvHashBufCI
//#define vvHashBufCI   halfsip24HashBufCI
//#define vvHashBufCI   bjLookup2HashBufCI
//#define vvHashBufCI   murmurHash3BufCI
#define vvHashBufCI   bjHashBufCI


#endif
