/***************************************************************************
 *  Pentagram HTTP/HTTPS server
 *  Copyright (C) 2023 Ketmar Dark
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ***************************************************************************/
#include "hashfunc.h"

/*
void MurmurHash3_x86_32  ( const void * key, int len, uint32_t seed, void * out );
void MurmurHash3_x86_128 ( const void * key, int len, uint32_t seed, void * out );
void MurmurHash3_x64_128 ( const void * key, int len, uint32_t seed, void * out );
*/

//-----------------------------------------------------------------------------
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.

// Note - The x86 and x64 versions do _not_ produce the same results, as the
// algorithms are optimized for their respective platforms. You can still
// compile and run any of them on any platform, but your performance with the
// non-native version will be less than optimal.

//-----------------------------------------------------------------------------
// Platform-specific functions and macros
static VVA_FORCEINLINE VVA_CONST uint32_t rotl32 (uint32_t x, uint8_t r) noexcept { return (x<<r)|(x>>(32u-r)); }


//-----------------------------------------------------------------------------
// Block read - if your platform needs to do endian-swapping or can only
// handle aligned reads, do the conversion here
//#define getblock32(p_,i_)  ((p_)[(i_)])


//-----------------------------------------------------------------------------
// Finalization mix - force all bits of a hash block to avalanche
static VVA_FORCEINLINE VVA_CONST uint32_t fmix32 (uint32_t h) noexcept {
  h ^= h>>16;
  h *= 0x85ebca6b;
  h ^= h>>13;
  h *= 0xc2b2ae35;
  h ^= h>>16;
  return h;
}


#define c1_32  0xcc9e2d51U
#define c2_32  0x1b873593U


//==========================================================================
//
//  murmurHash3Buf
//
//==========================================================================
uint32_t murmurHash3Buf (const VVA_MAYALIAS void *key, size_t len, uint32_t seed) noexcept {
  const VVA_MAYALIAS uint8_t *data = (const VVA_MAYALIAS uint8_t *)key;
  const size_t nblocks = len/4U;

  uint32_t h1 = seed;

  // body
  const VVA_MAYALIAS uint32_t *blocks = (const VVA_MAYALIAS uint32_t *)data;
  size_t i = nblocks;
  while (i--) {
    uint32_t k1 = *blocks++; //getblock32(blocks,i);
    k1 *= c1_32;
    k1 = rotl32(k1,15);
    k1 *= c2_32;
    h1 ^= k1;
    h1 = rotl32(h1,13);
    h1 = h1*5U+0xe6546b64U;
  }

  // tail
  const VVA_MAYALIAS uint8_t *tail = (const VVA_MAYALIAS uint8_t *)(data+nblocks*4U);
  uint32_t k1 = 0u;
  switch (len&3U) {
    case 3: k1 ^= tail[2]<<16; /* fallthrough */
    case 2: k1 ^= tail[1]<<8; /* fallthrough */
    case 1: k1 ^= tail[0]; k1 *= c1_32; k1 = rotl32(k1,15); k1 *= c2_32; h1 ^= k1; /* fallthrough */
  }

  // finalization
  h1 ^= len;
  return fmix32(h1);
}


//==========================================================================
//
//  murmurHash3BufCI
//
//==========================================================================
uint32_t murmurHash3BufCI (const VVA_MAYALIAS void *key, size_t len, uint32_t seed) noexcept {
  const VVA_MAYALIAS uint8_t *data = (const VVA_MAYALIAS uint8_t *)key;
  const size_t nblocks = len/4U;

  uint32_t h1 = seed;

  // body
  const VVA_MAYALIAS uint32_t *blocks = (const VVA_MAYALIAS uint32_t *)data;
  size_t i = nblocks;
  while (i--) {
    uint32_t k1 = *blocks++; //getblock32(blocks,i);
    k1 |= 0x20202020u; // ignore case
    k1 *= c1_32;
    k1 = rotl32(k1,15);
    k1 *= c2_32;
    h1 ^= k1;
    h1 = rotl32(h1,13);
    h1 = h1*5U+0xe6546b64U;
  }

  // tail
  const VVA_MAYALIAS uint8_t *tail = (const VVA_MAYALIAS uint8_t *)(data+nblocks*4U);
  uint32_t k1 = 0u;
  switch (len&3U) {
    case 3: k1 ^= (tail[2]|0x20u)<<16; /* fallthrough */
    case 2: k1 ^= (tail[1]|0x20u)<<8; /* fallthrough */
    case 1: k1 ^= (tail[0]|0x20u); k1 *= c1_32; k1 = rotl32(k1,15); k1 *= c2_32; h1 ^= k1; /* fallthrough */
  }

  // finalization
  h1 ^= len;
  return fmix32(h1);
}


//==========================================================================
//
//  murmurHash3Init
//
//==========================================================================
void murmurHash3Init (Murmur3Ctx *ctx, uint32_t seed) noexcept {
  if (!ctx) return;
  memset((void *)ctx, 0, sizeof(Murmur3Ctx));
  ctx->h1 = seed;
}


//==========================================================================
//
//  murmurHash3Update
//
//==========================================================================
void murmurHash3Update (Murmur3Ctx *ctx, const VVA_MAYALIAS void *key, size_t len) noexcept {
  if (!ctx || !len) return;

  // need to accumulate more bytes?
  if (ctx->bufused) {
    const VVA_MAYALIAS uint8_t *b = (const VVA_MAYALIAS uint8_t *)key;
    for (;;) {
      ctx->buf32 |= (uint32_t)(*b++)<<(ctx->bufused<<3);
      ++ctx->totalbytes;
      if (++ctx->bufused == 4) {
        ctx->bufused = 0;
        // update hash
        uint32_t k1 = ctx->buf32;
        ctx->buf32 = 0u;
        k1 *= c1_32;
        k1 = rotl32(k1,15);
        k1 *= c2_32;
        ctx->h1 ^= k1;
        ctx->h1 = rotl32(ctx->h1,13);
        ctx->h1 = ctx->h1*5U+0xe6546b64U;
        if (--len == 0) return;
        key = (const VVA_MAYALIAS void *)b;
        break;
      }
      if (--len == 0) return;
    }
  }

  // process dwords
  ctx->totalbytes += len;
  const VVA_MAYALIAS uint32_t *b32 = (const VVA_MAYALIAS uint32_t *)key;
  uint32_t h1 = ctx->h1;
  size_t l4 = len>>2;
  while (l4--) {
    // update hash
    uint32_t k1 = *b32++;
    k1 *= c1_32;
    k1 = rotl32(k1,15);
    k1 *= c2_32;
    h1 ^= k1;
    h1 = rotl32(h1,13);
    h1 = h1*5U+0xe6546b64U;
  }
  ctx->h1 = h1;

  // accumulate rest
  len &= 0x03u;
  if (len) {
    const VVA_MAYALIAS uint8_t *b = (const VVA_MAYALIAS uint8_t *)b32;
    while (len--) {
      ctx->buf32 |= (uint32_t)(*b++)<<(ctx->bufused<<3);
      ++ctx->bufused;
    }
  }
}


//==========================================================================
//
//  murmurHash3Final
//
//==========================================================================
uint32_t murmurHash3Final (const Murmur3Ctx *ctx) noexcept {
  if (!ctx) return 0u;
  uint32_t h1 = ctx->h1;
  // tail
  if (ctx->bufused) {
    uint32_t k1 = 0u;
    switch (ctx->bufused&0x03u) {
      case 1: k1 &= 0xffu; break;
      case 2: k1 &= 0xffffu; break;
      case 3: k1 &= 0xffffffu; break;
    }
    k1 *= c1_32; k1 = rotl32(k1,15); k1 *= c2_32; h1 ^= k1;
  }
  // finalization
  h1 ^= ctx->totalbytes;
  return fmix32(h1);
}


#define c1_128  0x239b961bU
#define c2_128  0xab0e9789U
#define c3_128  0x38b34ae5U
#define c4_128  0xa1e38b93U


//==========================================================================
//
//  murmurHash3Buf128
//
//  returns 16-byte hash value in `out`
//
//==========================================================================
void murmurHash3Buf128 (const VVA_MAYALIAS void *key, const size_t len, uint32_t seed, VVA_MAYALIAS void *out) noexcept {
  const VVA_MAYALIAS uint8_t *data = (const VVA_MAYALIAS uint8_t *)key;
  const size_t nblocks = len/16U;

  uint32_t h1 = seed;
  uint32_t h2 = seed;
  uint32_t h3 = seed;
  uint32_t h4 = seed;

  // body
  const VVA_MAYALIAS uint32_t *blocks = (const VVA_MAYALIAS uint32_t *)data;
  size_t i = nblocks;
  while (i--) {
    uint32_t k1 = *blocks++; //getblock32(blocks, i*4+0);
    uint32_t k2 = *blocks++; //getblock32(blocks, i*4+1);
    uint32_t k3 = *blocks++; //getblock32(blocks, i*4+2);
    uint32_t k4 = *blocks++; //getblock32(blocks, i*4+3);
    k1 *= c1_128; k1 = rotl32(k1,15); k1 *= c2_128; h1 ^= k1;
    h1 = rotl32(h1,19); h1 += h2; h1 = h1*5U+0x561ccd1bU;
    k2 *= c2_128; k2 = rotl32(k2,16); k2 *= c3_128; h2 ^= k2;
    h2 = rotl32(h2,17); h2 += h3; h2 = h2*5U+0x0bcaa747U;
    k3 *= c3_128; k3 = rotl32(k3,17); k3 *= c4_128; h3 ^= k3;
    h3 = rotl32(h3,15); h3 += h4; h3 = h3*5U+0x96cd1c35U;
    k4 *= c4_128; k4 = rotl32(k4,18); k4 *= c1_128; h4 ^= k4;
    h4 = rotl32(h4,13); h4 += h1; h4 = h4*5U+0x32ac3b17U;
  }

  // tail
  const VVA_MAYALIAS uint8_t *tail = (const VVA_MAYALIAS uint8_t *)(data+nblocks*16U);

  uint32_t k1 = 0;
  uint32_t k2 = 0;
  uint32_t k3 = 0;
  uint32_t k4 = 0;

  switch (len&15U) {
    case 15: k4 ^= tail[14]<<16; /* fallthrough */
    case 14: k4 ^= tail[13]<<8; /* fallthrough */
    case 13: k4 ^= tail[12]<<0; k4 *= c4_128; k4  = rotl32(k4,18); k4 *= c1_128; h4 ^= k4; /* fallthrough */

    case 12: k3 ^= tail[11]<<24; /* fallthrough */
    case 11: k3 ^= tail[10]<<16; /* fallthrough */
    case 10: k3 ^= tail[ 9]<<8; /* fallthrough */
    case  9: k3 ^= tail[ 8]<<0; k3 *= c3_128; k3  = rotl32(k3,17); k3 *= c4_128; h3 ^= k3; /* fallthrough */

    case  8: k2 ^= tail[ 7]<<24; /* fallthrough */
    case  7: k2 ^= tail[ 6]<<16; /* fallthrough */
    case  6: k2 ^= tail[ 5]<<8; /* fallthrough */
    case  5: k2 ^= tail[ 4]<<0; k2 *= c2_128; k2  = rotl32(k2,16); k2 *= c3_128; h2 ^= k2; /* fallthrough */

    case  4: k1 ^= tail[ 3]<<24; /* fallthrough */
    case  3: k1 ^= tail[ 2]<<16; /* fallthrough */
    case  2: k1 ^= tail[ 1]<<8; /* fallthrough */
    case  1: k1 ^= tail[ 0]<<0; k1 *= c1_128; k1  = rotl32(k1,15); k1 *= c2_128; h1 ^= k1; /* fallthrough */
  }

  // finalization
  h1 ^= len; h2 ^= len; h3 ^= len; h4 ^= len;

  h1 += h2; h1 += h3; h1 += h4;
  h2 += h1; h3 += h1; h4 += h1;

  h1 = fmix32(h1);
  h2 = fmix32(h2);
  h3 = fmix32(h3);
  h4 = fmix32(h4);

  h1 += h2; h1 += h3; h1 += h4;
  h2 += h1; h3 += h1; h4 += h1;

  ((VVA_MAYALIAS uint32_t *)out)[0] = h1;
  ((VVA_MAYALIAS uint32_t *)out)[1] = h2;
  ((VVA_MAYALIAS uint32_t *)out)[2] = h3;
  ((VVA_MAYALIAS uint32_t *)out)[3] = h4;
}
