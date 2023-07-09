/* coded by Ketmar // Invisible Vector (psyc://ketmar.no-ip.org/~Ketmar)
 * Understanding is not required. Only obedience.
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar.
 * See http://www.wtfpl.net/ for more details.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "vwadvfs.h"

//#define VWAD_DEBUG_WILDPATH


// ////////////////////////////////////////////////////////////////////////// //
#if defined(__cplusplus)
extern "C" {
#endif


// ////////////////////////////////////////////////////////////////////////// //
#define VWAD_NOFIDX  ((uint32_t)0xffffffffU)


// ////////////////////////////////////////////////////////////////////////// //
/* turning off inlining saves ~5kb of binary code on x86 */
#ifdef _MSC_VER
# define CC25519_INLINE  inline __attribute__((always_inline))
#else
/* dunno, let's hope this works */
# define CC25519_INLINE  inline
#endif


// ////////////////////////////////////////////////////////////////////////// //
// M$VC support modelled after Dasho code, thank you!
#ifdef _MSC_VER
# define vwad__builtin_expect(cond_val_,exp_val_)  (cond_val_)
# define vwad__builtin_trap  abort
# define vwad_packed_struct
# define vwad_push_pack  __pragma(pack(push, 1))
# define vwad_pop_pack   __pragma(pack(pop))
#else
# define vwad__builtin_expect  __builtin_expect
# define vwad__builtin_trap    __builtin_trap
# define vwad_packed_struct  __attribute__((packed))
# define vwad_push_pack
# define vwad_pop_pack
#endif


// ////////////////////////////////////////////////////////////////////////// //
typedef unsigned char ed25519_signature[64];
typedef unsigned char ed25519_public_key[32];
typedef unsigned char ed25519_secret_key[32];
typedef unsigned char ed25519_random_seed[32];


// ////////////////////////////////////////////////////////////////////////// //
static CC25519_INLINE void put_u32 (void *dest, uint32_t u) {
  ((uint8_t *)dest)[0] = (uint8_t)u;
  ((uint8_t *)dest)[1] = (uint8_t)(u>>8);
  ((uint8_t *)dest)[2] = (uint8_t)(u>>16);
  ((uint8_t *)dest)[3] = (uint8_t)(u>>24);
}


static CC25519_INLINE uint64_t get_u64 (const void *src) {
  uint64_t res = ((const uint8_t *)src)[0];
  res |= ((uint64_t)((const uint8_t *)src)[1])<<8;
  res |= ((uint64_t)((const uint8_t *)src)[2])<<16;
  res |= ((uint64_t)((const uint8_t *)src)[3])<<24;
  res |= ((uint64_t)((const uint8_t *)src)[4])<<32;
  res |= ((uint64_t)((const uint8_t *)src)[5])<<40;
  res |= ((uint64_t)((const uint8_t *)src)[6])<<48;
  res |= ((uint64_t)((const uint8_t *)src)[7])<<56;
  return res;
}

static CC25519_INLINE uint32_t get_u32 (const void *src) {
  uint32_t res = ((const uint8_t *)src)[0];
  res |= ((uint32_t)((const uint8_t *)src)[1])<<8;
  res |= ((uint32_t)((const uint8_t *)src)[2])<<16;
  res |= ((uint32_t)((const uint8_t *)src)[3])<<24;
  return res;
}

static CC25519_INLINE uint16_t get_u16 (const void *src) {
  uint16_t res = ((const uint8_t *)src)[0];
  res |= ((uint16_t)((const uint8_t *)src)[1])<<8;
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
static CC25519_INLINE uint32_t hashU32 (uint32_t v) {
  v ^= v >> 16;
  v *= 0x21f0aaadu;
  v ^= v >> 15;
  v *= 0x735a2d97u;
  v ^= v >> 15;
  return v;
}

static uint32_t deriveSeed (uint32_t seed, const uint8_t *buf, size_t buflen) {
  for (uint32_t f = 0; f < buflen; f += 1) {
    seed = hashU32((seed + 0x9E3779B9u) ^ buf[f]);
  }
  // hash it again
  return hashU32(seed + 0x9E3779B9u);
}

static void crypt_buffer (uint32_t xseed, uint64_t nonce, void *buf, uint32_t bufsize) {
  // use xoroshiro-derived PRNG, because i don't need cryptostrong xor at all
  #define MB32X  do { \
    /* 2 to the 32 divided by golden ratio; adding forms a Weyl sequence */ \
    rval = (xseed += 0x9E3779B9u); \
    rval ^= rval << 13; \
    rval ^= rval >> 17; \
    rval ^= rval << 5; \
    /*rval = hashU32(xseed += 0x9E3779B9u); -- mulberry32*/ \
  } while (0)

  xseed += nonce;
  uint32_t rval;

  uint32_t *b32 = (uint32_t *)buf;
  while (bufsize >= 4) {
    MB32X;
    put_u32(b32, get_u32(b32) ^ rval);
    ++b32;
    bufsize -= 4;
  }
  if (bufsize) {
    // final [1..3] bytes
    MB32X;
    uint32_t n;
    uint8_t *b = (uint8_t *)b32;
    switch (bufsize) {
      case 3:
        n = b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16);
        n ^= rval;
        b[0] = (uint8_t)n;
        b[1] = (uint8_t)(n>>8);
        b[2] = (uint8_t)(n>>16);
        break;
      case 2:
        n = b[0]|((uint32_t)b[1]<<8);
        n ^= rval;
        b[0] = (uint8_t)n;
        b[1] = (uint8_t)(n>>8);
        break;
      case 1:
        b[0] ^= rval;
        break;
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
#define crc32_init  (0xffffffffU)

static const uint32_t crctab[16] = {
  0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
  0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
  0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
  0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c,
};


#define CRC32BYTE(bt_)  do { \
  crc32 ^= (uint8_t)(bt_); \
  crc32 = crctab[crc32&0x0f]^(crc32>>4); \
  crc32 = crctab[crc32&0x0f]^(crc32>>4); \
} while (0)


static CC25519_INLINE uint32_t crc32_part (uint32_t crc32, const void *src, size_t len) {
  const uint8_t *buf = (const uint8_t *)src;
  while (len--) {
    CRC32BYTE(*buf++);
  }
  return crc32;
}

static CC25519_INLINE uint32_t crc32_final (uint32_t crc32) {
  return crc32^0xffffffffU;
}

static CC25519_INLINE uint32_t crc32_buf (const void *src, size_t len) {
  return crc32_final(crc32_part(crc32_init, src, len));
}


// ////////////////////////////////////////////////////////////////////////// //
// Z85 codec
static const char *z85_enc_alphabet =
  "0123456789abcdefghijklmnopqrstuvwxyzABCDEFG"
  "HIJKLMNOPQRSTUVWXYZ.-:+=^!/*?&<>()[]{}@%$#";

static const uint8_t z85_dec_alphabet [96] = {
  0x00, 0x44, 0x00, 0x54, 0x53, 0x52, 0x48, 0x00,
  0x4B, 0x4C, 0x46, 0x41, 0x00, 0x3F, 0x3E, 0x45,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x40, 0x00, 0x49, 0x42, 0x4A, 0x47,
  0x51, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A,
  0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32,
  0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A,
  0x3B, 0x3C, 0x3D, 0x4D, 0x00, 0x4E, 0x43, 0x00,
  0x00, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
  0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
  0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
  0x21, 0x22, 0x23, 0x4F, 0x00, 0x50, 0x00, 0x00
};


void vwad_z85_encode_key (const vwad_public_key inkey, vwad_z85_key enkey) {
  uint8_t sdata[32 + 4];
  memcpy(sdata, inkey, 32);
  const uint32_t crc32 = crc32_buf(sdata, 32);
  put_u32(&sdata[32], crc32);
  unsigned dpos = 0, spos = 0, value = 0;
  while (spos < 32 + 4) {
    value = value * 256u + sdata[spos++];
    if (spos % 4u == 0) {
      unsigned divisor = 85 * 85 * 85 * 85;
      while (divisor) {
        char ech = z85_enc_alphabet[value / divisor % 85u];
        if (ech == '/') ech = '~';
        enkey[dpos++] = ech;
        divisor /= 85u;
      }
      value = 0;
    }
  }
  if (dpos != (unsigned)sizeof(vwad_z85_key) - 1u) vwad__builtin_trap();
  enkey[dpos] = 0;
}


vwad_result vwad_z85_decode_key (const vwad_z85_key enkey, vwad_public_key outkey) {
  if (enkey == NULL || outkey == NULL) return -1;
  uint8_t ddata[32 + 4];
  unsigned dpos = 0, spos = 0, value = 0;
  while (spos < (unsigned)sizeof(vwad_z85_key) - 1) {
    char inch = enkey[spos++];
    switch (inch) {
      case 0: return -1;
      case '\\': inch = '/'; break;
      case '~': inch = '/'; break;
      case '|': inch = '!'; break;
      case ',': inch = '.'; break;
      case ';': inch = ':'; break;
      default: break;
    }
    if (!strchr(z85_enc_alphabet, inch)) return -1;
    value = value * 85 + z85_dec_alphabet[(uint8_t)inch - 32];
    if (spos % 5u == 0) {
      unsigned divisor = 256 * 256 * 256;
      while (divisor) {
        ddata[dpos++] = value / divisor % 256u;
        divisor /= 256u;
      }
      value = 0;
    }
  }
  if (dpos != 32 + 4) vwad__builtin_trap();
  if (enkey[spos] != 0) return -1;
  const uint32_t crc32 = crc32_buf(ddata, 32);
  if (crc32 != get_u32(&ddata[32])) return -1; // bad checksum
  memcpy(outkey, ddata, 32);
  return 0;
}


// ////////////////////////////////////////////////////////////////////////// //
// compact25519 dev-version
// Source: https://github.com/DavyLandman/compact25519
// Licensed under CC0-1.0
// Based on Daniel Beer's Public Domain c25519 implementation
// https://www.dlbeer.co.nz/oss/c25519.html version: 2017-10-05
typedef struct cc_ed25519_iostream_t cc_ed25519_iostream;
struct cc_ed25519_iostream_t {
  // return negative on failure
  int (*total_size) (cc_ed25519_iostream *strm);
  // read exactly `bufsize` bytes; return non-zero on failure
  // will never be called with 0 or negative `bufsize`
  int (*read) (cc_ed25519_iostream *strm, int startpos, void *buf, int bufsize);
  // user data
  void *udata;
};


#define ED25519_SEED_SIZE (32)
#define ED25519_PUBLIC_KEY_SIZE (32)
#define ED25519_PRIVATE_KEY_SIZE (64)
#define ED25519_SIGNATURE_SIZE (64)


struct sha512_state {
  uint64_t h[8];
};

#define SHA512_BLOCK_SIZE  (128)
#define SHA512_HASH_SIZE  (64)

static const struct sha512_state sha512_initial_state = { {
  0x6a09e667f3bcc908LL, 0xbb67ae8584caa73bLL,
  0x3c6ef372fe94f82bLL, 0xa54ff53a5f1d36f1LL,
  0x510e527fade682d1LL, 0x9b05688c2b3e6c1fLL,
  0x1f83d9abfb41bd6bLL, 0x5be0cd19137e2179LL,
} };

static const uint64_t round_k[80] = {
  0x428a2f98d728ae22LL, 0x7137449123ef65cdLL,
  0xb5c0fbcfec4d3b2fLL, 0xe9b5dba58189dbbcLL,
  0x3956c25bf348b538LL, 0x59f111f1b605d019LL,
  0x923f82a4af194f9bLL, 0xab1c5ed5da6d8118LL,
  0xd807aa98a3030242LL, 0x12835b0145706fbeLL,
  0x243185be4ee4b28cLL, 0x550c7dc3d5ffb4e2LL,
  0x72be5d74f27b896fLL, 0x80deb1fe3b1696b1LL,
  0x9bdc06a725c71235LL, 0xc19bf174cf692694LL,
  0xe49b69c19ef14ad2LL, 0xefbe4786384f25e3LL,
  0x0fc19dc68b8cd5b5LL, 0x240ca1cc77ac9c65LL,
  0x2de92c6f592b0275LL, 0x4a7484aa6ea6e483LL,
  0x5cb0a9dcbd41fbd4LL, 0x76f988da831153b5LL,
  0x983e5152ee66dfabLL, 0xa831c66d2db43210LL,
  0xb00327c898fb213fLL, 0xbf597fc7beef0ee4LL,
  0xc6e00bf33da88fc2LL, 0xd5a79147930aa725LL,
  0x06ca6351e003826fLL, 0x142929670a0e6e70LL,
  0x27b70a8546d22ffcLL, 0x2e1b21385c26c926LL,
  0x4d2c6dfc5ac42aedLL, 0x53380d139d95b3dfLL,
  0x650a73548baf63deLL, 0x766a0abb3c77b2a8LL,
  0x81c2c92e47edaee6LL, 0x92722c851482353bLL,
  0xa2bfe8a14cf10364LL, 0xa81a664bbc423001LL,
  0xc24b8b70d0f89791LL, 0xc76c51a30654be30LL,
  0xd192e819d6ef5218LL, 0xd69906245565a910LL,
  0xf40e35855771202aLL, 0x106aa07032bbd1b8LL,
  0x19a4c116b8d2d0c8LL, 0x1e376c085141ab53LL,
  0x2748774cdf8eeb99LL, 0x34b0bcb5e19b48a8LL,
  0x391c0cb3c5c95a63LL, 0x4ed8aa4ae3418acbLL,
  0x5b9cca4f7763e373LL, 0x682e6ff3d6b2b8a3LL,
  0x748f82ee5defb2fcLL, 0x78a5636f43172f60LL,
  0x84c87814a1f0ab72LL, 0x8cc702081a6439ecLL,
  0x90befffa23631e28LL, 0xa4506cebde82bde9LL,
  0xbef9a3f7b2c67915LL, 0xc67178f2e372532bLL,
  0xca273eceea26619cLL, 0xd186b8c721c0c207LL,
  0xeada7dd6cde0eb1eLL, 0xf57d4f7fee6ed178LL,
  0x06f067aa72176fbaLL, 0x0a637dc5a2c898a6LL,
  0x113f9804bef90daeLL, 0x1b710b35131c471bLL,
  0x28db77f523047d84LL, 0x32caab7b40c72493LL,
  0x3c9ebe0a15c9bebcLL, 0x431d67c49c100d4cLL,
  0x4cc5d4becb3e42b6LL, 0x597f299cfc657e2aLL,
  0x5fcb6fab3ad6faecLL, 0x6c44198c4a475817LL,
};

static CC25519_INLINE uint64_t load64 (const uint8_t *x) {
  uint64_t r = *(x++);
  r = (r << 8) | *(x++);
  r = (r << 8) | *(x++);
  r = (r << 8) | *(x++);
  r = (r << 8) | *(x++);
  r = (r << 8) | *(x++);
  r = (r << 8) | *(x++);
  r = (r << 8) | *(x++);

  return r;
}

static CC25519_INLINE void store64 (uint8_t *x, uint64_t v) {
  x += 7; *(x--) = v;
  v >>= 8; *(x--) = v;
  v >>= 8; *(x--) = v;
  v >>= 8; *(x--) = v;
  v >>= 8; *(x--) = v;
  v >>= 8; *(x--) = v;
  v >>= 8; *(x--) = v;
  v >>= 8; *(x--) = v;
}

static CC25519_INLINE uint64_t rot64 (uint64_t x, int bits) {
  return (x >> bits) | (x << (64 - bits));
}

static void sha512_block (struct sha512_state *s, const uint8_t *blk) {
  uint64_t w[16];
  uint64_t a, b, c, d, e, f, g, h;
  int i;

  for (i = 0; i < 16; i++) {
    w[i] = load64(blk);
    blk += 8;
  }

  a = s->h[0];
  b = s->h[1];
  c = s->h[2];
  d = s->h[3];
  e = s->h[4];
  f = s->h[5];
  g = s->h[6];
  h = s->h[7];

  for (i = 0; i < 80; i++) {
    const uint64_t wi = w[i & 15];
    const uint64_t wi15 = w[(i + 1) & 15];
    const uint64_t wi2 = w[(i + 14) & 15];
    const uint64_t wi7 = w[(i + 9) & 15];
    const uint64_t s0 = rot64(wi15, 1) ^ rot64(wi15, 8) ^ (wi15 >> 7);
    const uint64_t s1 = rot64(wi2, 19) ^ rot64(wi2, 61) ^ (wi2 >> 6);

    const uint64_t S0 = rot64(a, 28) ^ rot64(a, 34) ^ rot64(a, 39);
    const uint64_t S1 = rot64(e, 14) ^ rot64(e, 18) ^ rot64(e, 41);
    const uint64_t ch = (e & f) ^ ((~e) & g);
    const uint64_t temp1 = h + S1 + ch + round_k[i] + wi;
    const uint64_t maj = (a & b) ^ (a & c) ^ (b & c);
    const uint64_t temp2 = S0 + maj;

    h = g;
    g = f;
    f = e;
    e = d + temp1;
    d = c;
    c = b;
    b = a;
    a = temp1 + temp2;

    w[i & 15] = wi + s0 + wi7 + s1;
  }

  s->h[0] += a;
  s->h[1] += b;
  s->h[2] += c;
  s->h[3] += d;
  s->h[4] += e;
  s->h[5] += f;
  s->h[6] += g;
  s->h[7] += h;
}

static CC25519_INLINE void sha512_init (struct sha512_state *s) {
  memcpy(s, &sha512_initial_state, sizeof(*s));
}

static void sha512_final (struct sha512_state *s, const uint8_t *blk, size_t total_size) {
  uint8_t temp[SHA512_BLOCK_SIZE] = {0};
  const size_t last_size = total_size & (SHA512_BLOCK_SIZE - 1);

  if (last_size) memcpy(temp, blk, last_size);
  temp[last_size] = 0x80;

  if (last_size > 111) {
    sha512_block(s, temp);
    memset(temp, 0, sizeof(temp));
  }

  store64(temp + SHA512_BLOCK_SIZE - 8, total_size << 3);
  sha512_block(s, temp);
}

static void sha512_get (const struct sha512_state *s, uint8_t *hash,
                        unsigned int offset, unsigned int len)
{
  int i;

  if (offset > SHA512_BLOCK_SIZE) return;

  if (len > SHA512_BLOCK_SIZE - offset) len = SHA512_BLOCK_SIZE - offset;

  i = offset >> 3;
  offset &= 7;

  if (offset) {
    uint8_t tmp[8];
    unsigned int c = 8 - offset;

    if (c > len) c = len;

    store64(tmp, s->h[i++]);
    memcpy(hash, tmp + offset, c);
    len -= c;
    hash += c;
  }

  while (len >= 8) {
    store64(hash, s->h[i++]);
    hash += 8;
    len -= 8;
  }

  if (len) {
    uint8_t tmp[8];

    store64(tmp, s->h[i]);
    memcpy(hash, tmp, len);
  }
}

#define F25519_SIZE  (32)

static const uint8_t f25519_one[F25519_SIZE] = {1};

static CC25519_INLINE void f25519_copy (uint8_t *x, const uint8_t *a) {
  memcpy(x, a, F25519_SIZE);
}

#define FPRIME_SIZE  (32)

struct ed25519_pt {
  uint8_t x[F25519_SIZE];
  uint8_t y[F25519_SIZE];
  uint8_t t[F25519_SIZE];
  uint8_t z[F25519_SIZE];
};

static const struct ed25519_pt ed25519_base = {
  .x = {
    0x1a, 0xd5, 0x25, 0x8f, 0x60, 0x2d, 0x56, 0xc9,
    0xb2, 0xa7, 0x25, 0x95, 0x60, 0xc7, 0x2c, 0x69,
    0x5c, 0xdc, 0xd6, 0xfd, 0x31, 0xe2, 0xa4, 0xc0,
    0xfe, 0x53, 0x6e, 0xcd, 0xd3, 0x36, 0x69, 0x21
  },
  .y = {
    0x58, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66
  },
  .t = {
    0xa3, 0xdd, 0xb7, 0xa5, 0xb3, 0x8a, 0xde, 0x6d,
    0xf5, 0x52, 0x51, 0x77, 0x80, 0x9f, 0xf0, 0x20,
    0x7d, 0xe3, 0xab, 0x64, 0x8e, 0x4e, 0xea, 0x66,
    0x65, 0x76, 0x8b, 0xd7, 0x0f, 0x5f, 0x87, 0x67
  },
  .z = {1, 0}
};

static const struct ed25519_pt ed25519_neutral = {
  .x = {0},
  .y = {1, 0},
  .t = {0},
  .z = {1, 0}
};

#define ED25519_PACK_SIZE  F25519_SIZE
#define ED25519_EXPONENT_SIZE  32

static CC25519_INLINE void ed25519_copy (struct ed25519_pt *dst, const struct ed25519_pt *src) {
  memcpy(dst, src, sizeof(*dst));
}

#define EDSIGN_SECRET_KEY_SIZE  (32)
#define EDSIGN_PUBLIC_KEY_SIZE  (32)
#define EDSIGN_SIGNATURE_SIZE   (64)

static CC25519_INLINE void f25519_load (uint8_t *x, uint32_t c) {
  unsigned int i;

  for (i = 0; i < sizeof(c); i++) {
    x[i] = c;
    c >>= 8;
  }

  for (; i < F25519_SIZE; i++) x[i] = 0;
}

static void f25519_select (uint8_t *dst, const uint8_t *zero, const uint8_t *one,
                           uint8_t condition)
{
  const uint8_t mask = -condition;
  int i;

  for (i = 0; i < F25519_SIZE; i++) {
    dst[i] = zero[i] ^ (mask & (one[i] ^ zero[i]));
  }
}

static void f25519_normalize (uint8_t *x) {
  uint8_t minusp[F25519_SIZE];
  uint16_t c;
  int i;

  c = (x[31] >> 7) * 19;
  x[31] &= 127;

  for (i = 0; i < F25519_SIZE; i++) {
    c += x[i];
    x[i] = c;
    c >>= 8;
  }

  c = 19;

  for (i = 0; i + 1 < F25519_SIZE; i++) {
    c += x[i];
    minusp[i] = c;
    c >>= 8;
  }

  c += ((uint16_t)x[i]) - 128;
  minusp[31] = c;

  f25519_select(x, minusp, x, (c >> 15) & 1);
}

static CC25519_INLINE uint8_t f25519_eq (const uint8_t *x, const uint8_t *y) {
  uint8_t sum = 0;
  int i;

  for (i = 0; i < F25519_SIZE; i++) sum |= x[i] ^ y[i];

  sum |= (sum >> 4);
  sum |= (sum >> 2);
  sum |= (sum >> 1);

  return (sum ^ 1) & 1;
}

static void f25519_add (uint8_t *r, const uint8_t *a, const uint8_t *b) {
  uint16_t c = 0;
  int i;

  for (i = 0; i < F25519_SIZE; i++) {
    c >>= 8;
    c += ((uint16_t)a[i]) + ((uint16_t)b[i]);
    r[i] = c;
  }

  r[31] &= 127;
  c = (c >> 7) * 19;

  for (i = 0; i < F25519_SIZE; i++) {
    c += r[i];
    r[i] = c;
    c >>= 8;
  }
}

static void f25519_sub (uint8_t *r, const uint8_t *a, const uint8_t *b) {
  uint32_t c = 0;
  int i;

  c = 218;
  for (i = 0; i + 1 < F25519_SIZE; i++) {
    c += 65280 + ((uint32_t)a[i]) - ((uint32_t)b[i]);
    r[i] = c;
    c >>= 8;
  }

  c += ((uint32_t)a[31]) - ((uint32_t)b[31]);
  r[31] = c & 127;
  c = (c >> 7) * 19;

  for (i = 0; i < F25519_SIZE; i++) {
    c += r[i];
    r[i] = c;
    c >>= 8;
  }
}

static void f25519_neg (uint8_t *r, const uint8_t *a) {
  uint32_t c = 0;
  int i;

  c = 218;
  for (i = 0; i + 1 < F25519_SIZE; i++) {
    c += 65280 - ((uint32_t)a[i]);
    r[i] = c;
    c >>= 8;
  }

  c -= ((uint32_t)a[31]);
  r[31] = c & 127;
  c = (c >> 7) * 19;

  for (i = 0; i < F25519_SIZE; i++) {
    c += r[i];
    r[i] = c;
    c >>= 8;
  }
}

static void f25519_mul__distinct (uint8_t *r, const uint8_t *a, const uint8_t *b) {
  uint32_t c = 0;
  int i;

  for (i = 0; i < F25519_SIZE; i++) {
    int j;

    c >>= 8;
    for (j = 0; j <= i; j++) {
      c += ((uint32_t)a[j]) * ((uint32_t)b[i - j]);
    }

    for (; j < F25519_SIZE; j++) {
      c += ((uint32_t)a[j]) *
           ((uint32_t)b[i + F25519_SIZE - j]) * 38;
    }

    r[i] = c;
  }

  r[31] &= 127;
  c = (c >> 7) * 19;

  for (i = 0; i < F25519_SIZE; i++) {
    c += r[i];
    r[i] = c;
    c >>= 8;
  }
}

static void f25519_mul_c (uint8_t *r, const uint8_t *a, uint32_t b) {
  uint32_t c = 0;
  int i;

  for (i = 0; i < F25519_SIZE; i++) {
    c >>= 8;
    c += b * ((uint32_t)a[i]);
    r[i] = c;
  }

  r[31] &= 127;
  c >>= 7;
  c *= 19;

  for (i = 0; i < F25519_SIZE; i++) {
    c += r[i];
    r[i] = c;
    c >>= 8;
  }
}

static void f25519_inv__distinct (uint8_t *r, const uint8_t *x) {
  uint8_t s[F25519_SIZE];
  int i;

  f25519_mul__distinct(s, x, x);
  f25519_mul__distinct(r, s, x);

  for (i = 0; i < 248; i++) {
    f25519_mul__distinct(s, r, r);
    f25519_mul__distinct(r, s, x);
  }

  f25519_mul__distinct(s, r, r);

  f25519_mul__distinct(r, s, s);
  f25519_mul__distinct(s, r, x);

  f25519_mul__distinct(r, s, s);

  f25519_mul__distinct(s, r, r);
  f25519_mul__distinct(r, s, x);

  f25519_mul__distinct(s, r, r);
  f25519_mul__distinct(r, s, x);
}

static void exp2523 (uint8_t *r, const uint8_t *x, uint8_t *s) {
  int i;

  f25519_mul__distinct(r, x, x);
  f25519_mul__distinct(s, r, x);

  for (i = 0; i < 248; i++) {
    f25519_mul__distinct(r, s, s);
    f25519_mul__distinct(s, r, x);
  }

  f25519_mul__distinct(r, s, s);

  f25519_mul__distinct(s, r, r);
  f25519_mul__distinct(r, s, x);
}

static void f25519_sqrt (uint8_t *r, const uint8_t *a) {
  uint8_t v[F25519_SIZE];
  uint8_t i[F25519_SIZE];
  uint8_t x[F25519_SIZE];
  uint8_t y[F25519_SIZE];

  f25519_mul_c(x, a, 2);
  exp2523(v, x, y);

  f25519_mul__distinct(y, v, v);
  f25519_mul__distinct(i, x, y);
  f25519_load(y, 1);
  f25519_sub(i, i, y);

  f25519_mul__distinct(x, v, a);
  f25519_mul__distinct(r, x, i);
}

static void fprime_select (uint8_t *dst,
                           const uint8_t *zero, const uint8_t *one,
                           uint8_t condition)
{
  const uint8_t mask = -condition;
  int i;

  for (i = 0; i < FPRIME_SIZE; i++) {
    dst[i] = zero[i] ^ (mask & (one[i] ^ zero[i]));
  }
}

static void raw_try_sub (uint8_t *x, const uint8_t *p) {
  uint8_t minusp[FPRIME_SIZE];
  uint16_t c = 0;
  int i;

  for (i = 0; i < FPRIME_SIZE; i++) {
    c = ((uint16_t)x[i]) - ((uint16_t)p[i]) - c;
    minusp[i] = c;
    c = (c >> 8) & 1;
  }

  fprime_select(x, minusp, x, c);
}

static int prime_msb (const uint8_t *p) {
  int i;
  uint8_t x;

  for (i = FPRIME_SIZE - 1; i >= 0; i--) {
    if (p[i]) break;
  }

  x = p[i];
  i <<= 3;

  while (x) {
    x >>= 1;
    i++;
  }

  return i - 1;
}

static void shift_n_bits (uint8_t *x, int n) {
  uint16_t c = 0;
  int i;

  for (i = 0; i < FPRIME_SIZE; i++) {
    c |= ((uint16_t)x[i]) << n;
    x[i] = c;
    c >>= 8;
  }
}

static CC25519_INLINE int min_int (int a, int b) {
  return a < b ? a : b;
}

static void fprime_from_bytes (uint8_t *n,
                               const uint8_t *x, size_t len,
                               const uint8_t *modulus)
{
  const int preload_total = min_int(prime_msb(modulus) - 1, len << 3);
  const int preload_bytes = preload_total >> 3;
  const int preload_bits = preload_total & 7;
  const int rbits = (len << 3) - preload_total;
  int i;

  memset(n, 0, FPRIME_SIZE);

  for (i = 0; i < preload_bytes; i++) {
    n[i] = x[len - preload_bytes + i];
  }

  if (preload_bits) {
    shift_n_bits(n, preload_bits);
    n[0] |= x[len - preload_bytes - 1] >> (8 - preload_bits);
  }

  for (i = rbits - 1; i >= 0; i--) {
    const uint8_t bit = (x[i >> 3] >> (i & 7)) & 1;

    shift_n_bits(n, 1);
    n[0] |= bit;
    raw_try_sub(n, modulus);
  }
}

static CC25519_INLINE void ed25519_project (struct ed25519_pt *p, const uint8_t *x, const uint8_t *y) {
  f25519_copy(p->x, x);
  f25519_copy(p->y, y);
  f25519_load(p->z, 1);
  f25519_mul__distinct(p->t, x, y);
}

static CC25519_INLINE void ed25519_unproject (uint8_t *x, uint8_t *y, const struct ed25519_pt *p) {
  uint8_t z1[F25519_SIZE];

  f25519_inv__distinct(z1, p->z);
  f25519_mul__distinct(x, p->x, z1);
  f25519_mul__distinct(y, p->y, z1);

  f25519_normalize(x);
  f25519_normalize(y);
}

static const uint8_t ed25519_d[F25519_SIZE] = {
  0xa3, 0x78, 0x59, 0x13, 0xca, 0x4d, 0xeb, 0x75,
  0xab, 0xd8, 0x41, 0x41, 0x4d, 0x0a, 0x70, 0x00,
  0x98, 0xe8, 0x79, 0x77, 0x79, 0x40, 0xc7, 0x8c,
  0x73, 0xfe, 0x6f, 0x2b, 0xee, 0x6c, 0x03, 0x52
};

static CC25519_INLINE void ed25519_pack (uint8_t *c, const uint8_t *x, const uint8_t *y) {
  uint8_t tmp[F25519_SIZE];
  uint8_t parity;

  f25519_copy(tmp, x);
  f25519_normalize(tmp);
  parity = (tmp[0] & 1) << 7;

  f25519_copy(c, y);
  f25519_normalize(c);
  c[31] |= parity;
}

static uint8_t ed25519_try_unpack (uint8_t *x, uint8_t *y, const uint8_t *comp) {
  const int parity = comp[31] >> 7;
  uint8_t a[F25519_SIZE];
  uint8_t b[F25519_SIZE];
  uint8_t c[F25519_SIZE];

  f25519_copy(y, comp);
  y[31] &= 127;

  f25519_mul__distinct(c, y, y);

  f25519_mul__distinct(b, c, ed25519_d);
  f25519_add(a, b, f25519_one);
  f25519_inv__distinct(b, a);

  f25519_sub(a, c, f25519_one);

  f25519_mul__distinct(c, a, b);

  f25519_sqrt(a, c);
  f25519_neg(b, a);

  f25519_select(x, a, b, (a[0] ^ parity) & 1);

  f25519_mul__distinct(a, x, x);
  f25519_normalize(a);
  f25519_normalize(c);

  return f25519_eq(a, c);
}

static const uint8_t ed25519_k[F25519_SIZE] = {
  0x59, 0xf1, 0xb2, 0x26, 0x94, 0x9b, 0xd6, 0xeb,
  0x56, 0xb1, 0x83, 0x82, 0x9a, 0x14, 0xe0, 0x00,
  0x30, 0xd1, 0xf3, 0xee, 0xf2, 0x80, 0x8e, 0x19,
  0xe7, 0xfc, 0xdf, 0x56, 0xdc, 0xd9, 0x06, 0x24
};

static void ed25519_add (struct ed25519_pt *r,
                         const struct ed25519_pt *p1, const struct ed25519_pt *p2)
{
  uint8_t a[F25519_SIZE];
  uint8_t b[F25519_SIZE];
  uint8_t c[F25519_SIZE];
  uint8_t d[F25519_SIZE];
  uint8_t e[F25519_SIZE];
  uint8_t f[F25519_SIZE];
  uint8_t g[F25519_SIZE];
  uint8_t h[F25519_SIZE];

  f25519_sub(c, p1->y, p1->x);
  f25519_sub(d, p2->y, p2->x);
  f25519_mul__distinct(a, c, d);
  f25519_add(c, p1->y, p1->x);
  f25519_add(d, p2->y, p2->x);
  f25519_mul__distinct(b, c, d);
  f25519_mul__distinct(d, p1->t, p2->t);
  f25519_mul__distinct(c, d, ed25519_k);
  f25519_mul__distinct(d, p1->z, p2->z);
  f25519_add(d, d, d);
  f25519_sub(e, b, a);
  f25519_sub(f, d, c);
  f25519_add(g, d, c);
  f25519_add(h, b, a);
  f25519_mul__distinct(r->x, e, f);
  f25519_mul__distinct(r->y, g, h);
  f25519_mul__distinct(r->t, e, h);
  f25519_mul__distinct(r->z, f, g);
}

static void ed25519_double (struct ed25519_pt *r, const struct ed25519_pt *p) {
  uint8_t a[F25519_SIZE];
  uint8_t b[F25519_SIZE];
  uint8_t c[F25519_SIZE];
  uint8_t e[F25519_SIZE];
  uint8_t f[F25519_SIZE];
  uint8_t g[F25519_SIZE];
  uint8_t h[F25519_SIZE];

  f25519_mul__distinct(a, p->x, p->x);
  f25519_mul__distinct(b, p->y, p->y);
  f25519_mul__distinct(c, p->z, p->z);
  f25519_add(c, c, c);
  f25519_add(f, p->x, p->y);
  f25519_mul__distinct(e, f, f);
  f25519_sub(e, e, a);
  f25519_sub(e, e, b);
  f25519_sub(g, b, a);
  f25519_sub(f, g, c);
  f25519_neg(h, b);
  f25519_sub(h, h, a);
  f25519_mul__distinct(r->x, e, f);
  f25519_mul__distinct(r->y, g, h);
  f25519_mul__distinct(r->t, e, h);
  f25519_mul__distinct(r->z, f, g);
}

static void ed25519_smult (struct ed25519_pt *r_out, const struct ed25519_pt *p,
                           const uint8_t *e)
{
  struct ed25519_pt r;
  int i;

  ed25519_copy(&r, &ed25519_neutral);

  for (i = 255; i >= 0; i--) {
    const uint8_t bit = (e[i >> 3] >> (i & 7)) & 1;
    struct ed25519_pt s;

    ed25519_double(&r, &r);
    ed25519_add(&s, &r, p);

    f25519_select(r.x, r.x, s.x, bit);
    f25519_select(r.y, r.y, s.y, bit);
    f25519_select(r.z, r.z, s.z, bit);
    f25519_select(r.t, r.t, s.t, bit);
  }

  ed25519_copy(r_out, &r);
}

#define EXPANDED_SIZE  (64)

static const uint8_t ed25519_order[FPRIME_SIZE] = {
  0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
  0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10
};

static CC25519_INLINE uint8_t upp (struct ed25519_pt *p, const uint8_t *packed) {
  uint8_t x[F25519_SIZE];
  uint8_t y[F25519_SIZE];
  uint8_t ok = ed25519_try_unpack(x, y, packed);

  ed25519_project(p, x, y);
  return ok;
}

static CC25519_INLINE void pp (uint8_t *packed, const struct ed25519_pt *p) {
  uint8_t x[F25519_SIZE];
  uint8_t y[F25519_SIZE];

  ed25519_unproject(x, y, p);
  ed25519_pack(packed, x, y);
}

static CC25519_INLINE void sm_pack (uint8_t *r, const uint8_t *k) {
  struct ed25519_pt p;

  ed25519_smult(&p, &ed25519_base, k);
  pp(r, &p);
}

static int hash_with_prefix (uint8_t *out_fp,
                             uint8_t *init_block, unsigned int prefix_size,
                             cc_ed25519_iostream *strm)
{
  struct sha512_state s;

  const int xxlen = strm->total_size(strm);
  if (xxlen < 0) return -1;
  const size_t len = (size_t)xxlen;

  uint8_t msgblock[SHA512_BLOCK_SIZE];

  sha512_init(&s);

  if (len < SHA512_BLOCK_SIZE && len + prefix_size < SHA512_BLOCK_SIZE) {
    if (len > 0) {
      if (strm->read(strm, 0, msgblock, (int)len) != 0) return -1;
      memcpy(init_block + prefix_size, msgblock, len);
    }
    sha512_final(&s, init_block, len + prefix_size);
  } else {
    size_t i;

    if (strm->read(strm, 0, msgblock, SHA512_BLOCK_SIZE - prefix_size) != 0) return -1;
    memcpy(init_block + prefix_size, msgblock, SHA512_BLOCK_SIZE - prefix_size);
    sha512_block(&s, init_block);

    for (i = SHA512_BLOCK_SIZE - prefix_size;
         i + SHA512_BLOCK_SIZE <= len;
         i += SHA512_BLOCK_SIZE)
    {
      if (strm->read(strm, (int)i, msgblock, SHA512_BLOCK_SIZE) != 0) return -1;
      sha512_block(&s, msgblock);
    }

    const int left = (int)len - (int)i;
    if (left < 0) vwad__builtin_trap();
    if (left > 0 && strm->read(strm, (int)i, msgblock, left) != 0) return -1;

    sha512_final(&s, msgblock, len + prefix_size);
  }

  sha512_get(&s, init_block, 0, SHA512_HASH_SIZE);
  fprime_from_bytes(out_fp, init_block, SHA512_HASH_SIZE, ed25519_order);

  return 0;
}

static int hash_message (uint8_t *z, const uint8_t *r, const uint8_t *a,
                         cc_ed25519_iostream *strm)
{
  uint8_t block[SHA512_BLOCK_SIZE];

  memcpy(block, r, 32);
  memcpy(block + 32, a, 32);
  return hash_with_prefix(z, block, 64, strm);
}

static int edsign_verify_stream (const uint8_t *signature, const uint8_t *pub,
                                 cc_ed25519_iostream *strm)
{
  struct ed25519_pt p;
  struct ed25519_pt q;
  uint8_t lhs[F25519_SIZE];
  uint8_t rhs[F25519_SIZE];
  uint8_t z[FPRIME_SIZE];
  uint8_t ok = 1;

  if (hash_message(z, signature, pub, strm) != 0) return -1;

  sm_pack(lhs, signature + 32);

  ok &= upp(&p, pub);
  ed25519_smult(&p, &p, z);
  ok &= upp(&q, signature);
  ed25519_add(&p, &p, &q);
  pp(rhs, &p);

  return (ok & f25519_eq(lhs, rhs) ? 0 : -1);
}


// ////////////////////////////////////////////////////////////////////////// //
void (*vwad_logf) (int type, const char *fmt, ...) = NULL;

#define logf(type_,...)  do { \
  if (vwad_logf) { \
    vwad_logf(VWAD_LOG_ ## type_, __VA_ARGS__); \
  } \
} while (0)


// ////////////////////////////////////////////////////////////////////////// //
void (*vwad_assertion_failed) (const char *fmt, ...) = NULL;

static inline const char *SkipPathPartCStr (const char *s) {
  const char *lastSlash = NULL;
  for (const char *t = s; *t; ++t) {
    if (*t == '/') lastSlash = t+1;
    #ifdef _WIN32
    if (*t == '\\') lastSlash = t+1;
    #endif
  }
  return (lastSlash ? lastSlash : s);
}

#define vassert(cond_)  do { if (vwad__builtin_expect((!(cond_)), 0)) { const int line__assf = __LINE__; \
    if (vwad_assertion_failed) { \
      vwad_assertion_failed("%s:%d: Assertion in `%s` failed: %s\n", \
        SkipPathPartCStr(__FILE__), line__assf, __PRETTY_FUNCTION__, #cond_); \
      vwad__builtin_trap(); /* just in case */ \
    } else { \
      vwad__builtin_trap(); \
    } \
  } \
} while (0)


// ////////////////////////////////////////////////////////////////////////// //
void (*vwad_debug_open_file) (vwad_handle *wad, vwad_fidx fidx, vwad_fd fd) = NULL;
void (*vwad_debug_close_file) (vwad_handle *wad, vwad_fidx fidx, vwad_fd fd) = NULL;
void (*vwad_debug_read_chunk) (vwad_handle *wad, int bidx, vwad_fidx fidx, vwad_fd fd, int chunkidx) = NULL;
void (*vwad_debug_flush_chunk) (vwad_handle *wad, int bidx, vwad_fidx fidx, vwad_fd fd, int chunkidx) = NULL;


// ////////////////////////////////////////////////////////////////////////// //
// memory allocation
static CC25519_INLINE void *xalloc (vwad_memman *mman, size_t size) {
  vassert(size > 0 && size <= 0x7ffffff0u);
  if (mman != NULL) return mman->alloc(mman, (uint32_t)size); else return malloc(size);
}

static CC25519_INLINE void *zalloc (vwad_memman *mman, size_t size) {
  vassert(size > 0 && size <= 0x7ffffff0u);
  void *p = (mman != NULL ? mman->alloc(mman, (uint32_t)size) : malloc(size));
  if (p) memset(p, 0, size);
  return p;
}

static CC25519_INLINE void xfree (vwad_memman *mman, void *p) {
  if (p != NULL) {
    if (mman != NULL) mman->free(mman, p); else free(p);
  }
}


// ////////////////////////////////////////////////////////////////////////// //
unsigned vwad_crc32_init (void) { return crc32_init; }
unsigned vwad_crc32_part (unsigned crc32, const void *src, size_t len) { return crc32_part(crc32, src, len); }
unsigned vwad_crc32_final (unsigned crc32) { return crc32_final(crc32); }


// ////////////////////////////////////////////////////////////////////////// //
typedef int intbool_t;

enum {
  int_false = 0,
  int_true = 1
};

typedef struct {
  uint32_t x1, x2, x;
  const uint8_t *src;
  int spos, send;
} EntDecoder;

typedef struct {
  uint16_t p1, p2;
} Predictor;


typedef struct {
  Predictor pred[2];
  int ctx;
} BitPPM;

typedef struct {
  Predictor predBits[2 * 256];
  int ctxBits;
} BytePPM;

typedef struct {
  // 8 bits, twice
  BytePPM bytes[2];
  // "second byte" flag
  BitPPM moreFlag;
} WordPPM;


// ////////////////////////////////////////////////////////////////////////// //
static CC25519_INLINE uint8_t DecGetByte (EntDecoder *ee) {
  if (ee->spos < ee->send) {
    return ee->src[ee->spos++];
  } else {
    ee->spos = 0x7fffffff;
    return 0;
  }
}

static void DecInit (EntDecoder *ee, const void *inbuf, uint32_t insize) {
  ee->x1 = 0; ee->x2 = 0xFFFFFFFFU;
  ee->src = (const uint8_t *)inbuf; ee->spos = 0; ee->send = insize;
  ee->x = DecGetByte(ee);
  ee->x = (ee->x << 8) + DecGetByte(ee);
  ee->x = (ee->x << 8) + DecGetByte(ee);
  ee->x = (ee->x << 8) + DecGetByte(ee);
}

static CC25519_INLINE intbool_t DecDecode (EntDecoder *ee, int p) {
  uint32_t xmid = ee->x1 + (uint32_t)(((uint64_t)((ee->x2 - ee->x1) & 0xffffffffU) * (uint32_t)p) >> 17);
  intbool_t bit = (ee->x <= xmid);
  if (bit) ee->x2 = xmid; else ee->x1 = xmid + 1;
  while ((ee->x1 ^ ee->x2) < (1u << 24)) {
    ee->x1 <<= 8;
    ee->x2 = (ee->x2 << 8) + 255;
    ee->x = (ee->x << 8) + DecGetByte(ee);
  }
  return bit;
}


// ////////////////////////////////////////////////////////////////////////// //
static void PredInit (Predictor *pp) {
  pp->p1 = 1 << 15; pp->p2 = 1 << 15;
}

static CC25519_INLINE int PredGetP (Predictor *pp) {
  return (int)((uint32_t)pp->p1 + (uint32_t)pp->p2);
}

static CC25519_INLINE void PredUpdate (Predictor *pp, intbool_t bit) {
  if (bit) {
    pp->p1 += ((~((uint32_t)pp->p1)) >> 3) & 0b0001111111111111;
    pp->p2 += ((~((uint32_t)pp->p2)) >> 6) & 0b0000001111111111;
  } else {
    pp->p1 -= ((uint32_t)pp->p1) >> 3;
    pp->p2 -= ((uint32_t)pp->p2) >> 6;
  }
}

static CC25519_INLINE intbool_t PredGetBitDecodeUpdate (Predictor *pp, EntDecoder *dec) {
  int p = PredGetP(pp);
  intbool_t bit = DecDecode(dec, p);
  PredUpdate(pp, bit);
  return bit;
}


// ////////////////////////////////////////////////////////////////////////// //
static void BitPPMInit (BitPPM *ppm, int initstate) {
  for (size_t f = 0; f < sizeof(ppm->pred) / sizeof(ppm->pred[0]); ++f) PredInit(&ppm->pred[f]);
  ppm->ctx = !!initstate;
}

static CC25519_INLINE intbool_t BitPPMDecode (BitPPM *ppm, EntDecoder *dec) {
  intbool_t bit = PredGetBitDecodeUpdate(&ppm->pred[ppm->ctx], dec);
  if (bit) ppm->ctx = 1; else ppm->ctx = 0;
  return bit;
}


// ////////////////////////////////////////////////////////////////////////// //
static void BytePPMInit (BytePPM *ppm) {
  for (size_t f = 0; f < sizeof(ppm->predBits) / sizeof(ppm->predBits[0]); ++f) PredInit(&ppm->predBits[f]);
  ppm->ctxBits = 0;
}

static CC25519_INLINE uint8_t BytePPMDecodeByte (BytePPM *ppm, EntDecoder *dec) {
  uint32_t n = 1;
  int cofs = ppm->ctxBits * 256;
  do {
    intbool_t bit = PredGetBitDecodeUpdate(&ppm->predBits[cofs + n], dec);
    n += n; if (bit) ++n;
  } while (n < 0x100);
  n -= 0x100;
  ppm->ctxBits = (n >= 0x80);
  return (uint8_t)n;
}



// ////////////////////////////////////////////////////////////////////////// //
static void WordPPMInit (WordPPM *ppm) {
  BytePPMInit(&ppm->bytes[0]); BytePPMInit(&ppm->bytes[1]);
  BitPPMInit(&ppm->moreFlag, 0);
}

static CC25519_INLINE int WordPPMDecodeInt (WordPPM *ppm, EntDecoder *dec) {
  int n = BytePPMDecodeByte(&ppm->bytes[0], dec);
  if (BitPPMDecode(&ppm->moreFlag, dec)) {
    n += BytePPMDecodeByte(&ppm->bytes[1], dec) * 0x100;
  }
  return n;
}


//==========================================================================
//
//  DecompressLZFF3
//
//==========================================================================
static intbool_t DecompressLZFF3 (const void *src, int srclen, void *dest, int unpsize) {
  intbool_t error;
  EntDecoder dec;
  BytePPM ppmData;
  WordPPM ppmMtOfs, ppmMtLen, ppmLitLen;
  BitPPM ppmLitFlag;
  int litcount, n;
  uint32_t dictpos, spos;

  #define PutByte(bt_)  do { \
    if (unpsize != 0) { \
      ((uint8_t *)dest)[dictpos++] = (uint8_t)(bt_); unpsize -= 1; \
    } else { \
      error = int_true; \
    } \
  } while (0)

  if (srclen < 1 || srclen > 0x1fffffff) return 0;
  if (unpsize < 1 || unpsize > 0x1fffffff) return 0;

  error = int_false;
  dictpos = 0;

  BytePPMInit(&ppmData);
  WordPPMInit(&ppmMtOfs);
  WordPPMInit(&ppmMtLen);
  WordPPMInit(&ppmLitLen);
  BitPPMInit(&ppmLitFlag, 1);

  DecInit(&dec, src, srclen);

  if (!BitPPMDecode(&ppmLitFlag, &dec)) error = int_true;
  else {
    uint8_t sch;
    int len, ofs;

    litcount = WordPPMDecodeInt(&ppmLitLen, &dec) + 1;
    while (!error && litcount != 0) {
      litcount -= 1;
      n = BytePPMDecodeByte(&ppmData, &dec);
      PutByte((uint8_t)n);
      error = error || (dec.spos > dec.send);
    }

    while (!error && unpsize != 0) {
      if (BitPPMDecode(&ppmLitFlag, &dec)) {
        litcount = WordPPMDecodeInt(&ppmLitLen, &dec) + 1;
        while (!error && litcount != 0) {
          litcount -= 1;
          n = BytePPMDecodeByte(&ppmData, &dec);
          PutByte((uint8_t)n);
          error = error || (dec.spos > dec.send);
        }
      } else {
        len = WordPPMDecodeInt(&ppmMtLen, &dec) + 3;
        ofs = WordPPMDecodeInt(&ppmMtOfs, &dec) + 1;
        error = error || (dec.spos > dec.send) || (len > unpsize) || (ofs > dictpos);
        if (!error) {
          spos = dictpos - ofs;
          while (!error && len != 0) {
            len -= 1;
            sch = ((const uint8_t *)dest)[spos];
            spos++;
            PutByte(sch);
          }
        }
      }
    }
  }

  return (!error && unpsize == 0);
}


// ////////////////////////////////////////////////////////////////////////// //
/* is the given codepoint considered printable?
i restrict it to some useful subset.
unifuck is unifucked, but i hope that i sorted out all idiotic diactritics and control chars. */
static CC25519_INLINE vwad_bool is_uni_printable (uint16_t ch) {
  return
    (ch >= 0x0001 && ch <= 0x024F) || // basic latin
    (ch >= 0x0390 && ch <= 0x0482) || // some greek, and cyrillic w/o combiners
    (ch >= 0x048A && ch <= 0x052F) ||
    (ch >= 0x1E00 && ch <= 0x1EFF) || // latin extended additional
    (ch >= 0x2000 && ch <= 0x2C7F) || // some general punctuation, extensions, etc.
    (ch >= 0x2E00 && ch <= 0x2E42) || // supplemental punctuation
    (ch >= 0xAB30 && ch <= 0xAB65);   // more latin extended
}


/* determine utf-8 sequence length (in bytes) by its first char.
returns length (up to 4) or 0 on invalid first char
doesn't allow overlongs */
static CC25519_INLINE uint32_t utf_char_len (const void *str) {
  const uint8_t ch = *((const uint8_t *)str);
  if (ch < 0x80) return 1;
  else if ((ch&0x0E0) == 0x0C0) return (ch != 0x0C0 && ch != 0x0C1 ? 2 : 0);
  else if ((ch&0x0F0) == 0x0E0) return 3;
  else if ((ch&0x0F8) == 0x0F0) return 4;
  else return 0;
}


static CC25519_INLINE uint16_t utf_decode (const char **strp) {
  const uint8_t *bp = (const uint8_t *)*strp;
  uint16_t res = (uint16_t)utf_char_len(bp);
  uint8_t ch = *bp;
  if (res < 1 || res > 3) {
    res = VWAD_REPLACEMENT_CHAR;
    *strp += 1;
  } else if (ch < 0x80) {
    res = ch;
    *strp += 1;
  } else if ((ch&0x0E0) == 0x0C0) {
    if (ch == 0x0C0 || ch == 0x0C1) {
      res = VWAD_REPLACEMENT_CHAR;
      *strp += 1;
    } else {
      res = ch - 0x0C0;
      ch = bp[1];
      if ((ch&0x0C0) != 0x80) {
        res = VWAD_REPLACEMENT_CHAR;
        *strp += 1;
      } else {
        res = res * 64 + ch - 128;
        *strp += 2;
      }
    }
  } else if ((ch&0x0F0) == 0x0E0) {
    res = ch - 0x0E0;
    ch = bp[1];
    if ((ch&0x0C0) != 0x80) {
      res = VWAD_REPLACEMENT_CHAR;
      *strp += 1;
    } else {
      res = res * 64 + ch - 128;
      ch = bp[2];
      if ((ch&0x0C0) != 0x80) {
        res = VWAD_REPLACEMENT_CHAR;
        *strp += 1;
      } else {
        res = res * 64 + ch - 128;
        *strp += 3;
      }
    }
  } else {
    res = VWAD_REPLACEMENT_CHAR;
  }
  if (res && !is_uni_printable(res)) res = VWAD_REPLACEMENT_CHAR;
  return res;
}


static CC25519_INLINE uint16_t unilower (uint16_t ch) {
  if ((ch >= 'A' && ch <= 'Z') ||
      (ch >= 0x00C0 && ch <= 0x00D6) ||
      (ch >= 0x00D8 && ch <= 0x00DE) ||
      (ch >= 0x0410 && ch <= 0x042F))
  {
    return ch + 0x20;
  }
  if (ch == 0x0178) return 0x00FF;
  if (ch >= 0x0400 && ch <= 0x040F) return ch + 0x50;
  if ((ch >= 0x1E00 && ch <= 0x1E95) ||
      (ch >= 0x1EA0 && ch <= 0x1EFF))
  {
    return ch|0x01;
  }
  if (ch == 0x1E9E) return 0x00DF;
  return ch;
}


//==========================================================================
//
//  vwad_utf_char_len
//
//==========================================================================
uint32_t vwad_utf_char_len (const void *str) {
  return (str ? utf_char_len(str) : 0);
}


//==========================================================================
//
//  vwad_is_uni_printable
//
//==========================================================================
vwad_bool vwad_is_uni_printable (uint16_t ch) {
  return is_uni_printable(ch);
}


//==========================================================================
//
//  vwad_utf_decode
//
//  advances `strp` at least by one byte
//
//==========================================================================
uint16_t vwad_utf_decode (const char **strp) {
  return utf_decode(strp);
}


//==========================================================================
//
//  vwad_uni_tolower
//
//==========================================================================
uint16_t vwad_uni_tolower (uint16_t ch) {
  return unilower(ch);
}


// ////////////////////////////////////////////////////////////////////////// //
static uint32_t joaatHashStrCI (const char *key) {
  uint32_t hash = 0x29a;
  uint32_t len = 0;
  while (*key) {
    uint16_t ch = unilower(utf_decode(&key));
    hash += (uint8_t)ch;
    hash += hash<<10;
    hash ^= hash>>6;
    if (ch >= 0x100) {
      hash += (uint8_t)(ch>>8);
      hash += hash<<10;
      hash ^= hash>>6;
    }
    ++len;
  }
  // mix length
  hash += (uint8_t)len;
  hash += hash<<10;
  hash ^= hash>>6;
  if (len >= 0x100) {
    hash += (uint8_t)(len>>8);
    hash += hash<<10;
    hash ^= hash>>6;
  }
  // finalize
  hash += hash<<3;
  hash ^= hash>>11;
  hash += hash<<15;
  return hash;
}

static CC25519_INLINE uint32_t hashStrCI (const void *buf) {
  return (buf ? joaatHashStrCI((const char *)buf) : 0u);
}


// ////////////////////////////////////////////////////////////////////////// //
static vwad_bool strEquCI (const char *s0, const char *s1) {
  if (!s0 || !s1) return 0;
  uint16_t c0 = unilower(utf_decode(&s0));
  uint16_t c1 = unilower(utf_decode(&s1));
  while (c0 && c1 && c0 == c1) {
    if (c0 == VWAD_REPLACEMENT_CHAR || c1 == VWAD_REPLACEMENT_CHAR) return 0;
    c0 = unilower(utf_decode(&s0));
    c1 = unilower(utf_decode(&s1));
  }
  return (c0 == 0 && c1 == 0);
}


// ////////////////////////////////////////////////////////////////////////// //
#define HASH_BUCKETS  (1024)

#define MAX_OPENED_FILES  (128)
// it must be at least not less than max opened files
#define MAX_GLOB_BUFFERS  MAX_OPENED_FILES


vwad_push_pack
typedef struct vwad_packed_struct {
  uint32_t ofs;     // offset in stream
  uint16_t upksize; // unpacked chunk size-1
  uint16_t pksize;  // packed chunk size (0 means "unpacked")
} ChunkInfo;

typedef struct vwad_packed_struct {
  uint32_t fchunk;     // first chunk
  uint32_t nhash;      // name hash
  uint32_t bknext;     // next name in bucket
  uint32_t gnameofs;   // group name offset
  uint64_t ftime;      // since Epoch, 0 is "unknown"
  uint32_t crc32;      // full crc32
  uint32_t upksize;    // unpacked file size
  uint32_t chunkCount; // number of chunks
  uint32_t nameofs;    // name offset in names array
} FileInfo;

typedef struct vwad_packed_struct {
  uint32_t crc32;
  uint16_t version;
  uint16_t flags;
  uint32_t dirofs;
  uint16_t u_cmt_size;
  uint16_t p_cmt_size;
  uint32_t cmt_crc32;
} MainFileHeader;

typedef struct vwad_packed_struct {
  uint32_t pkdir_crc32;
  uint32_t dir_crc32;
  uint32_t pkdirsize;
  uint32_t upkdirsize;
} MainDirHeader;
vwad_pop_pack

typedef struct {
  uint32_t cidx; // chunk number in file
  uint32_t size; // 0: the buffer is not used
  uint32_t era;
  uint8_t data[65536];
} FileBuffer;

typedef struct {
  uint32_t fidx;  // file index for this fd; 0 means "unused"
  uint32_t fofs;  // virtual file offset, in bytes
  uint32_t bidx;  // current cache buffer index (in `globCache`)
} OpenedFile;

struct vwad_handle_t {
  vwad_iostream *strm;
  vwad_memman *mman;
  uint32_t flags;
  ed25519_public_key pubkey;
  char *comment;       // can be NULL
  char author[128];    // author string
  char title[128];     // title string
  uint8_t *updir;      // unpacked directory
  ChunkInfo *chunks;   // points to the unpacked directory
  uint32_t xorRndSeed; // seed for block decryptor
  uint32_t chunkCount; // number of elements in `chunks` array
  // files (0 is unused)
  FileInfo *files;     // points to the unpacked directory
  uint32_t fileCount;  // number of elements in `files` array
  // file names (0-terminated)
  const char *names;   // points to the unpacked directory
  // directory hash table
  uint32_t buckets[HASH_BUCKETS];
  // public key
  uint32_t haspubkey; // bit 0: has key; bit 1: authenticated
  // opened files
  OpenedFile fds[MAX_OPENED_FILES];
  int fdsUsed; // to avoid excessive scans
  // temporary buffer to unpack chunks (data + crc32)
  uint8_t pkdata[65536 + 4];
  // global cache
  uint32_t globCacheSize; // 0 means "each opened file owns exactly one buffer"
  FileBuffer *globCache[MAX_GLOB_BUFFERS];
  uint32_t lastera;
};


typedef struct {
  vwad_iostream *strm;
  int currpos, size;
} EdInfo;


//==========================================================================
//
//  ed_total_size
//
//==========================================================================
static int ed_total_size (cc_ed25519_iostream *strm) {
  EdInfo *nfo = (EdInfo *)strm->udata;
  return nfo->size - (4+64+32); // without header
}


//==========================================================================
//
//  ed_read
//
//==========================================================================
static int ed_read (cc_ed25519_iostream *strm, int startpos, void *buf, int bufsize) {
  EdInfo *nfo = (EdInfo *)strm->udata;
  if (startpos < 0) return -1; // oops
  startpos += 4+64+32; // skip header
  if (startpos >= nfo->size) return -1;
  const int max = nfo->size - startpos;
  if (bufsize > max) bufsize = max;
  if (nfo->currpos != startpos) {
    if (nfo->strm->seek(nfo->strm, startpos) != 0) return -1;
    nfo->currpos = startpos + bufsize;
  } else {
    nfo->currpos += bufsize;
  }
  return nfo->strm->read(nfo->strm, buf, bufsize);
}


// ////////////////////////////////////////////////////////////////////////// //
static CC25519_INLINE vwad_bool is_path_delim (char ch) {
  return (ch == '/' || ch == '\\');
}


//==========================================================================
//
//  vwad_normalize_file_name
//
//==========================================================================
vwad_result vwad_normalize_file_name (const char *fname, char res[256]) {
  if (fname == NULL || res == NULL) return -1;
  size_t spos = 0, dpos = 0;
  if (fname[0] == '.' && is_path_delim(fname[1])) ++fname;
  else if (is_path_delim(fname[0])) { res[0] = '/'; dpos = 1; }
  while (dpos <= 255 && fname[spos]) {
    char ch = fname[spos++];
    if (ch < 32 || ch >= 127) {
      dpos = 256;
    } else if (ch == '/' || ch == '\\') {
      if (dpos != 0 && res[dpos - 1] != '/') res[dpos++] = '/';
      // skip "." and ".."
      while (is_path_delim(fname[spos])) ++spos;
      if (fname[spos] == '.' && fname[spos + 1] == '.' && is_path_delim(fname[spos + 2])) {
        spos += 2;
        if (dpos <= 1) dpos = 256;
        else {
          // remove last directory
          vassert(res[dpos - 1] == '/');
          --dpos;
          while (dpos > 0 && res[dpos - 1] != '/') --dpos;
        }
      } else if (fname[spos] == '.' && is_path_delim(fname[spos + 1])) {
        spos += 1;
      }
    } else {
      res[dpos++] = ch;
    }
  }
  if (dpos == 0 || dpos > 255) {
    res[0] = 0; // why not
    return -1;
  } else {
    res[dpos] = 0;
    return 0;
  }
}


//==========================================================================
//
//  is_valid_string
//
//==========================================================================
static vwad_bool is_valid_string (const char *str) {
  if (!str) return 1;
  // check chars
  uint16_t ch;
  do { ch = utf_decode(&str); } while (ch >= 32 && ch != VWAD_REPLACEMENT_CHAR);
  return (ch == 0);
}


//==========================================================================
//
//  is_valid_comment
//
//==========================================================================
static vwad_bool is_valid_comment (const char *cmt, uint32_t cmtlen) {
  vwad_bool res = 1;
  if (cmt != NULL) {
    uint16_t ch;
    do {
      ch = utf_decode(&cmt);
      if (ch < 32 && (ch != 9 && ch != 10)) ch = VWAD_REPLACEMENT_CHAR;
    } while (ch != 0 && ch != VWAD_REPLACEMENT_CHAR);
    return (ch == 0);
  } else {
    res = (cmtlen == 0);
  }
  return res;
}


//==========================================================================
//
//  is_valid_file_name
//
//==========================================================================
static vwad_bool is_valid_file_name (const char *str) {
  if (!str || !str[0] || str[0] == '/') return 0;
  if ((uintptr_t)str&0x03) return 0; // it should be aligned
  const size_t slen = strlen(str);
  if (slen > 255) return 0; // too long
  if (str[slen - 1] == '/') return 0; // should not end with a slash
  size_t eofs = slen;
  do {
    if (str[eofs]) return 0;
    ++eofs;
  } while ((eofs&0x03) != 0);
  // check chars
  uint16_t ch;
  do { ch = utf_decode(&str); } while (ch >= 32 && ch != VWAD_REPLACEMENT_CHAR);
  return (ch == 0);
}


//==========================================================================
//
//  is_valid_group_name
//
//==========================================================================
static vwad_bool is_valid_group_name (const char *str) {
  if (!str) return 0;
  if ((uintptr_t)str&0x03) return 0; // it should be aligned
  const size_t slen = strlen(str);
  if (slen > 255) return 0; // too long
  size_t eofs = slen;
  do {
    if (str[eofs]) return 0;
    ++eofs;
  } while ((eofs&0x03) != 0);
  // check chars
  uint16_t ch;
  do { ch = utf_decode(&str); } while (ch >= 32 && ch != VWAD_REPLACEMENT_CHAR);
  return (ch == 0);
}


//==========================================================================
//
//  vwad_open_archive
//
//==========================================================================
vwad_handle *vwad_open_archive (vwad_iostream *strm, unsigned flags, vwad_memman *mman) {
  if (!strm || !strm->seek || !strm->read) {
    logf(ERROR, "vwad_open_archive: invalid stream");
    return NULL;
  }
  if (strm->seek(strm, 0) != 0) {
    logf(ERROR, "vwad_open_archive: cannot seek to 0");
    return NULL;
  }

  ed25519_public_key pubkey;
  ed25519_signature edsign;
  MainFileHeader mhdr;
  MainDirHeader dhdr;
  uint8_t *wadcomment = NULL;
  char sign[4];
  char author[128];
  char title[128];
  uint8_t aslen, tslen;

  // file signature
  if (strm->read(strm, sign, 4) != 0) {
    logf(ERROR, "vwad_open_archive: cannot read signature");
    return NULL;
  }
  if (memcmp(sign, "VWAD", 4) != 0) {
    logf(ERROR, "vwad_open_archive: invalid signature");
    return NULL;
  }
  // digital signature
  if (strm->read(strm, &edsign, (uint32_t)sizeof(edsign)) != 0) {
    logf(ERROR, "vwad_open_archive: cannot read edsign");
    return NULL;
  }
  // public key
  if (strm->read(strm, &pubkey, (uint32_t)sizeof(pubkey)) != 0) {
    logf(ERROR, "vwad_open_archive: cannot read pubkey");
    return NULL;
  }

  // lengthes
  if (strm->read(strm, &aslen, 1) != 0) {
    logf(ERROR, "vwad_open_archive: cannot read author length");
    return NULL;
  }
  if (strm->read(strm, &tslen, 1) != 0) {
    logf(ERROR, "vwad_open_archive: cannot read title length");
    return NULL;
  }

  // read author
  if (strm->read(strm, sign, 2) != 0) {
    logf(ERROR, "vwad_open_archive: cannot read author padding");
    return NULL;
  }
  if (memcmp(sign, "\x0d\x0a", 2) != 0) {
    logf(ERROR, "vwad_open_archive: invalid author padding");
    return NULL;
  }
  if (aslen != 0 && strm->read(strm, author, (int)aslen) != 0) {
    logf(ERROR, "vwad_open_archive: cannot read author");
    return NULL;
  }
  if (aslen > 127) {
    logf(ERROR, "vwad_open_archive: invalid author string length");
    return NULL;
  }
  author[aslen] = 0;
  if (!is_valid_string(author)) {
    logf(WARNING, "vwad_open_archive: invalid author string contents, discarded");
    memset(author, 0, sizeof(author));
  }

  // read title
  if (strm->read(strm, sign, 2) != 0) {
    logf(ERROR, "vwad_open_archive: cannot read title padding");
    return NULL;
  }
  if (memcmp(sign, "\x0d\x0a", 2) != 0) {
    logf(ERROR, "vwad_open_archive: invalid title padding");
    return NULL;
  }
  if (tslen != 0 && strm->read(strm, title, (int)tslen) != 0) {
    logf(ERROR, "vwad_open_archive: cannot read title");
    return NULL;
  }
  if (tslen > 127) {
    logf(ERROR, "vwad_open_archive: invalid title string length");
    return NULL;
  }
  title[tslen] = 0;
  if (!is_valid_string(title)) {
    logf(WARNING, "vwad_open_archive: invalid title string contents, discarded");
    memset(title, 0, sizeof(title));
  }

  // read final padding
  if (strm->read(strm, sign, 4) != 0) {
    logf(ERROR, "vwad_open_archive: cannot read title padding");
    return NULL;
  }
  if (memcmp(sign, "\x0d\x0a\x1b\x00", 4) != 0) {
    logf(ERROR, "vwad_open_archive: invalid final padding");
    return NULL;
  }

  // read main header
  if (strm->read(strm, &mhdr, (uint32_t)sizeof(mhdr)) != 0) {
    logf(ERROR, "vwad_open_archive: cannot read main header");
    return NULL;
  }

  uint32_t fcofs = 4 + (uint32_t)sizeof(edsign) + (uint32_t)sizeof(pubkey) +
                   1+1+2 + aslen + 2 + tslen + 4 +
                   (uint32_t)sizeof(mhdr);

  uint32_t pkseed, seed;

  // decrypt public key
  #if 0
  logf(DEBUG, "author: %s", author);
  logf(DEBUG, "title: %s", title);
  #endif
  pkseed = deriveSeed(0xa29, edsign, (uint32_t)sizeof(ed25519_signature));
  pkseed = deriveSeed(pkseed, (const uint8_t *)author, strlen(author));
  pkseed = deriveSeed(pkseed, (const uint8_t *)title, strlen(title));
  #if 0
  logf(DEBUG, "kkseed: 0x%08x", pkseed);
  #endif
  crypt_buffer(pkseed, 0x29a, pubkey, (uint32_t)sizeof(ed25519_public_key));

  // create initial seed from pubkey, author, and title
  pkseed = deriveSeed(0x29c, pubkey, sizeof(ed25519_public_key));
  pkseed = deriveSeed(pkseed, (const uint8_t *)author, strlen(author));
  pkseed = deriveSeed(pkseed, (const uint8_t *)title, strlen(title));
  #if 0
  logf(DEBUG, "pkseed: 0x%08x", pkseed);
  #endif
  crypt_buffer(pkseed, 1, &mhdr, (uint32_t)sizeof(mhdr));
  if (mhdr.version != 0) {
    logf(ERROR, "vwad_open_archive: invalid version");
    return NULL;
  }
  if (mhdr.flags != 0 && mhdr.flags != 1) {
    logf(ERROR, "vwad_open_archive: invalid flags");
    return NULL;
  }

  mhdr.crc32 = get_u32(&mhdr.crc32);
  mhdr.version = get_u16(&mhdr.version);
  mhdr.flags = get_u16(&mhdr.flags);
  mhdr.dirofs = get_u32(&mhdr.dirofs);
  mhdr.u_cmt_size = get_u16(&mhdr.u_cmt_size);
  mhdr.p_cmt_size = get_u16(&mhdr.p_cmt_size);
  mhdr.cmt_crc32 = get_u32(&mhdr.cmt_crc32);

  if (mhdr.u_cmt_size == 0 && mhdr.cmt_crc32 != 0) {
    logf(ERROR, "vwad_open_archive: corrupted header data");
    return NULL;
  }

  if (mhdr.crc32 != crc32_buf(&mhdr.version, (uint32_t)sizeof(mhdr) - 4)) {
    logf(ERROR, "vwad_open_archive: corrupted header data");
    return NULL;
  }

  if (mhdr.dirofs <= 4+32+64+(int)sizeof(mhdr)+(int)mhdr.p_cmt_size) {
    logf(ERROR, "vwad_open_archive: invalid directory offset");
    return NULL;
  }

  if (mhdr.u_cmt_size == 0 && mhdr.p_cmt_size != 0) {
    logf(ERROR, "vwad_open_archive: invalid comment size");
    return NULL;
  }

  // read comment, because we need it for seed generation
  if (mhdr.u_cmt_size != 0) {
    if (mhdr.p_cmt_size == 0) {
      // not packed
      fcofs += mhdr.u_cmt_size;
      wadcomment = zalloc(mman, mhdr.u_cmt_size + 1); // +1 for reusing
      if (wadcomment == NULL) {
        logf(DEBUG, "vwad_open_archive: cannot allocate buffer for comment");
        goto error;
      }
      if (strm->read(strm, wadcomment, mhdr.u_cmt_size) != 0) {
        xfree(mman, wadcomment);
        logf(DEBUG, "vwad_open_archive: cannot read comment data");
        goto error;
      }
      // update seed
      seed = deriveSeed(pkseed, wadcomment, mhdr.u_cmt_size);
    } else {
      // packed
      fcofs += mhdr.p_cmt_size;
      wadcomment = zalloc(mman, mhdr.p_cmt_size);
      if (wadcomment == NULL) {
        logf(DEBUG, "vwad_open_archive: cannot allocate buffer for comment");
        goto error;
      }
      if (strm->read(strm, wadcomment, mhdr.p_cmt_size) != 0) {
        xfree(mman, wadcomment);
        logf(DEBUG, "vwad_open_archive: cannot read comment data");
        goto error;
      }
      // update seed
      seed = deriveSeed(pkseed, wadcomment, mhdr.p_cmt_size);
    }
  } else {
    // still seed
    seed = deriveSeed(pkseed, wadcomment, 0);
  }
  #if 0
  logf(DEBUG, "xnseed: 0x%08x", seed);
  #endif

  // determine file size
  if (strm->seek(strm, mhdr.dirofs) != 0) {
    xfree(mman, wadcomment);
    logf(ERROR, "vwad_open_archive: cannot seek to directory");
    return NULL;
  }

  logf(DEBUG, "vwad_open_archive: dirofs=0x%08x", mhdr.dirofs);

  if (strm->read(strm, &dhdr, (uint32_t)sizeof(dhdr)) != 0) {
    xfree(mman, wadcomment);
    logf(ERROR, "vwad_open_archive: cannot read directory header");
    return NULL;
  }

  crypt_buffer(seed, 0xfffffffeU, &dhdr, (uint32_t)sizeof(dhdr));

  dhdr.pkdir_crc32 = get_u32(&dhdr.pkdir_crc32);
  dhdr.dir_crc32 = get_u32(&dhdr.dir_crc32);
  dhdr.pkdirsize = get_u32(&dhdr.pkdirsize);
  dhdr.upkdirsize = get_u32(&dhdr.upkdirsize);

  logf(DEBUG, "vwad_open_archive: pkdirsize=0x%08x", dhdr.pkdirsize);
  logf(DEBUG, "vwad_open_archive: upkdirsize=0x%08x", dhdr.upkdirsize);
  if (dhdr.pkdirsize == 0 || dhdr.pkdirsize > 0xffffff) {
    xfree(mman, wadcomment);
    logf(ERROR, "vwad_open_archive: invalid directory size");
    return NULL;
  }
  if (dhdr.upkdirsize <= 4*11 || dhdr.upkdirsize > 0xffffff) {
    xfree(mman, wadcomment);
    logf(ERROR, "vwad_open_archive: invalid directory size");
    return NULL;
  }
  if (0x7fffffffU - mhdr.dirofs < dhdr.pkdirsize) {
    xfree(mman, wadcomment);
    logf(ERROR, "vwad_open_archive: invalid directory size");
    return NULL;
  }

  // check digital signature
  uint32_t haspubkey = 0;
  if ((mhdr.flags & 0x01) == 0) haspubkey = 1;
  if (haspubkey) {
    if ((flags & VWAD_OPEN_NO_SIGN_CHECK) == 0) {
      cc_ed25519_iostream edstrm;
      EdInfo nfo;
      nfo.strm = strm;
      nfo.currpos = -1;
      nfo.size = (int)(mhdr.dirofs + dhdr.pkdirsize + (int)sizeof(dhdr));
      logf(DEBUG, "vwad_open_archive: file size: %d", nfo.size);
      edstrm.udata = &nfo;
      edstrm.total_size = ed_total_size;
      edstrm.read = ed_read;

      logf(NOTE, "checking digital signature...");
      int sres = edsign_verify_stream(edsign, pubkey, &edstrm);
      if (sres != 0) {
        xfree(mman, wadcomment);
        logf(ERROR, "vwad_open_archive: invalid digital signature");
        return NULL;
      }
      haspubkey = 3; // has a key, and authenticated
    }
  }

  // read and unpack directory
  if (strm->seek(strm, mhdr.dirofs + (int)sizeof(dhdr)) != 0) {
    xfree(mman, wadcomment);
    logf(ERROR, "vwad_open_archive: cannot seek to directory data");
    return NULL;
  }

  void *unpkdir = xalloc(mman, dhdr.upkdirsize + 4); // always end it with 0
  if (!unpkdir) {
    xfree(mman, wadcomment);
    logf(ERROR, "vwad_open_archive: cannot allocate memory for unpacked directory");
    return NULL;
  }
  put_u32(unpkdir + dhdr.upkdirsize, 0);

  void *pkdir = xalloc(mman, dhdr.pkdirsize);
  if (!pkdir) {
    xfree(mman, wadcomment);
    xfree(mman, unpkdir);
    logf(ERROR, "vwad_open_archive: cannot allocate memory for packed directory");
    return NULL;
  }

  if (strm->read(strm, pkdir, dhdr.pkdirsize) != 0) {
    xfree(mman, wadcomment);
    xfree(mman, pkdir);
    xfree(mman, unpkdir);
    logf(ERROR, "vwad_open_archive: cannot read directory data");
    return NULL;
  }

  crypt_buffer(seed, 0xffffffffU, pkdir, dhdr.pkdirsize);

  uint32_t crc32 = crc32_buf(pkdir, dhdr.pkdirsize);
  if (crc32 != dhdr.pkdir_crc32) {
    xfree(mman, wadcomment);
    xfree(mman, pkdir);
    xfree(mman, unpkdir);
    logf(DEBUG, "vwad_open_archive: pkcrc: file=0x%08x; real=0x%08x", dhdr.pkdir_crc32, crc32);
    logf(ERROR, "vwad_open_archive: corrupted packed directory data");
    return NULL;
  }

  if (!DecompressLZFF3(pkdir, dhdr.pkdirsize, unpkdir, dhdr.upkdirsize)) {
    xfree(mman, wadcomment);
    xfree(mman, pkdir);
    xfree(mman, unpkdir);
    logf(ERROR, "vwad_open_archive: cannot decompress directory");
    return NULL;
  }
  xfree(mman, pkdir);

  crc32 = crc32_buf(unpkdir, dhdr.upkdirsize);
  if (crc32 != dhdr.dir_crc32) {
    xfree(mman, wadcomment);
    xfree(mman, unpkdir);
    logf(DEBUG, "vwad_open_archive: upkcrc: file=0x%08x; real=0x%08x", dhdr.dir_crc32, crc32);
    logf(ERROR, "vwad_open_archive: corrupted unpacked directory data");
    return NULL;
  }

  // allocate wad handle
  vwad_handle *wad = zalloc(mman, sizeof(vwad_handle));
  if (wad == NULL) {
    xfree(mman, wadcomment);
    xfree(mman, unpkdir);
    logf(ERROR, "vwad_open_archive: cannot allocate memory for vwad handle");
    return NULL;
  }
  wad->strm = strm;
  wad->mman = mman;
  wad->flags = (uint32_t)flags;
  wad->updir = unpkdir;
  wad->xorRndSeed = seed;
  if (haspubkey) {
    vassert(sizeof(wad->pubkey) == sizeof(pubkey));
    memcpy(wad->pubkey, pubkey, sizeof(wad->pubkey));
    wad->haspubkey = haspubkey;
  } else {
    memset(wad->pubkey, 0, sizeof(wad->pubkey));
    wad->haspubkey = 0;
  }

  strcpy(wad->author, author);
  strcpy(wad->title, title);

  // get counters
  wad->chunkCount = get_u32(wad->updir + 0);
  wad->fileCount = get_u32(wad->updir + 4);

  // setup and check chunks
  uint32_t upofs = 4+4;
  if (wad->chunkCount == 0 || wad->chunkCount > 0x1fffffffU ||
      wad->chunkCount*(uint32_t)sizeof(ChunkInfo) >= dhdr.upkdirsize ||
      wad->chunkCount*(uint32_t)sizeof(ChunkInfo) >= dhdr.upkdirsize - upofs)
  {
    logf(DEBUG, "invalid chunk count (%u)", wad->chunkCount);
    goto error;
  }
  logf(DEBUG, "chunk count: %u", wad->chunkCount);
  wad->chunks = (ChunkInfo *)(wad->updir + upofs);
  upofs += wad->chunkCount * (uint32_t)sizeof(ChunkInfo);

  for (uint32_t cidx = 0; cidx < wad->chunkCount; ++cidx) {
    ChunkInfo *ci = &wad->chunks[cidx];
    ci->pksize = get_u16(&ci->pksize);
    if (ci->ofs != 0 || ci->upksize != 0) {
      logf(DEBUG, "invalid chunk data (0; idx=%u): ofs=%u; upksize=%u",
           cidx, ci->ofs, ci->upksize);
      goto error;
    }
    ci->ofs = 0xffffffffU;
  }

  // files
  if (upofs >= dhdr.upkdirsize || dhdr.upkdirsize - upofs < (uint32_t)sizeof(FileInfo) + 8) {
    logf(DEBUG, "invalid directory data (files, 0)");
    goto error;
  }

  if (wad->fileCount < 1 || wad->fileCount > 0xffffU ||
      wad->fileCount * (uint32_t)sizeof(FileInfo) >= dhdr.upkdirsize ||
      wad->fileCount * (uint32_t)sizeof(FileInfo) >= dhdr.upkdirsize - upofs)
  {
    logf(DEBUG, "invalid file count (%u)", wad->fileCount);
    goto error;
  }
  logf(DEBUG, "file count: %u", wad->fileCount);
  wad->files = (FileInfo *)(wad->updir + upofs);
  upofs += wad->fileCount * (uint32_t)sizeof(FileInfo);

  // names
  if (upofs >= dhdr.upkdirsize || dhdr.upkdirsize - upofs < 8) {
    logf(DEBUG, "invalid directory data (names, 0)");
    goto error;
  }
  const uint32_t namesSize = dhdr.upkdirsize - upofs;
  if (namesSize < 8 || namesSize > 0x3fffffffU || (namesSize&0x03) != 0) {
    logf(DEBUG, "invalid names size (%u)", namesSize);
    goto error;
  }
  logf(DEBUG, "name table size: %u", namesSize);
  wad->names = (char *)(wad->updir + upofs);

  // unpack comment
  if ((flags & VWAD_OPEN_NO_MAIN_COMMENT) == 0) {
    if (mhdr.u_cmt_size != 0) {
      if (mhdr.p_cmt_size == 0) {
        // unpacked, just use it as is
        wad->comment = (char *)wadcomment;
        wadcomment = NULL;
        // decrypt comment with pk-seed
        crypt_buffer(pkseed, 2, wad->comment, mhdr.u_cmt_size);
      } else {
        // packed
        wad->comment = zalloc(mman, mhdr.u_cmt_size + 1);
        if (wad->comment == NULL) {
          xfree(mman, wadcomment);
          logf(DEBUG, "vwad_open_archive: out of memory for comment data");
          goto error;
        }
        // decrypt comment with pk-seed
        crypt_buffer(pkseed, 2, wadcomment, mhdr.p_cmt_size);
        const intbool_t cupres = DecompressLZFF3(wadcomment, (int)mhdr.p_cmt_size,
                                                 wad->comment, (int)mhdr.u_cmt_size);
        xfree(mman, wadcomment);
        if (!cupres) {
          logf(DEBUG, "vwad_open_archive: cannot decompress packed comment data");
          goto error;
        }
      }
      if (mhdr.cmt_crc32 != crc32_buf(wad->comment, mhdr.u_cmt_size)) {
        logf(WARNING, "vwad_open_archive: corrupted comment data, comment discarded");
        xfree(mman, wad->comment);
        wad->comment = NULL;
      } else if (!is_valid_comment(wad->comment, mhdr.u_cmt_size)) {
        logf(WARNING, "vwad_open_archive: invalid comment data, comment discarded");
        xfree(mman, wad->comment);
        wad->comment = NULL;
      }
    } else {
      vassert(wadcomment == NULL);
    }
  } else {
    xfree(mman, wadcomment);
  }

  uint32_t chunkOfs = fcofs;
  uint32_t currChunk = 0;

  for (uint32_t fidx = 0; fidx < wad->fileCount; ++fidx) {
    FileInfo *fi = &wad->files[fidx];

    if (fi->nhash != 0 || fi->bknext != 0 || fi->fchunk != 0) {
      logf(DEBUG, "invalid file data (zero fields are non-zero)");
      goto error;
    }

    fi->ftime = get_u64(&fi->ftime);
    fi->crc32 = get_u32(&fi->crc32);
    fi->upksize = get_u32(&fi->upksize);
    fi->chunkCount = get_u32(&fi->chunkCount);
    fi->nameofs = get_u32(&fi->nameofs);
    fi->gnameofs = get_u32(&fi->gnameofs);

    if (fi->chunkCount == 0) {
      if (fi->upksize != 0) {
        logf(DEBUG, "invalid file data (file size, !0)");
        goto error;
      }
    } else {
      if (fi->upksize == 0) {
        logf(DEBUG, "invalid file data (file size, 0");
        goto error;
      }
    }

    if (fi->upksize > 0x7fffffffU || fi->nameofs >= namesSize) {
      logf(DEBUG, "invalid file data (name offset)");
      goto error;
    }

    // should be aligned
    if (fi->nameofs < 4 || (fi->nameofs&0x03) != 0) {
      logf(DEBUG, "invalid file data (name align)");
      goto error;
    }

    const char *name = wad->names + fi->nameofs;
    if (!is_valid_file_name(name)) {
      logf(DEBUG, "invalid file data (file name) (%s)", name);
      goto error;
    }

    if (fi->upksize > 0x7fffffffU || fi->gnameofs >= namesSize) {
      logf(DEBUG, "invalid file data (group name offset)");
      goto error;
    }

    // should be aligned
    if (fi->gnameofs&0x03) {
      logf(DEBUG, "invalid file data (group name align)");
      goto error;
    }

    if (!is_valid_group_name(wad->names + fi->gnameofs)) {
      logf(DEBUG, "invalid file data (group name)");
      goto error;
    }

    // insert name into hash table (also, check for duplicates)
    fi->nhash = hashStrCI(name);
    const uint32_t bkt = fi->nhash % HASH_BUCKETS;

    if (wad->buckets[bkt]) {
      FileInfo *bkfi = &wad->files[wad->buckets[bkt]];
      while (bkfi != NULL && !strEquCI(name, wad->names + bkfi->nameofs)) {
        if (bkfi->bknext) bkfi = &wad->files[bkfi->bknext]; else bkfi = NULL;
      }
      if (bkfi != NULL) {
        logf(DEBUG, "duplicate file name (%s)", name);
        goto error;
      }
    }

    fi->bknext = wad->buckets[bkt];
    wad->buckets[bkt] = fidx;

    // fix chunks
    uint32_t left = fi->upksize;
    if (left != 0) {
      fi->fchunk = currChunk;
      for (uint32_t cnn = 0; cnn < fi->chunkCount; ++cnn) {
        if (left == 0) {
          logf(DEBUG, "invalid file data (out of chunks)");
          goto error;
        }
        if (currChunk >= wad->chunkCount) {
          logf(DEBUG, "invalid file data (chunks)");
          goto error;
        }
        if (wad->chunks[currChunk].ofs != 0xffffffffU) {
          logf(DEBUG, "invalid file data (chunks, oops)");
          goto error;
        }
        if (chunkOfs >= mhdr.dirofs) {
          logf(DEBUG, "invalid file data (chunk offset); fidx=%u; cofs=0x%08x; dofs=0x%08x",
                  fidx, chunkOfs, mhdr.dirofs);
          goto error;
        }
        wad->chunks[currChunk].ofs = chunkOfs;
        vassert(left != 0);
        if (left > 65536) {
          wad->chunks[currChunk].upksize = 65535;
          left -= 65536;
        } else {
          wad->chunks[currChunk].upksize = left - 1;
          left = 0;
        }
        chunkOfs += 4; // crc32
        if (wad->chunks[currChunk].pksize == 0) {
          // unpacked chunk
          chunkOfs += wad->chunks[currChunk].upksize + 1;
        } else {
          // packed chunk
          chunkOfs += wad->chunks[currChunk].pksize;
        }
        if (chunkOfs > mhdr.dirofs) {
          logf(DEBUG, "invalid file data (chunk offset 1); fidx=%u/%u; cofs=0x%08x; dofs=0x%08x",
                  fidx, wad->fileCount, chunkOfs, mhdr.dirofs);
          goto error;
        }
        currChunk += 1;
      }
    }
  }
  if (chunkOfs != mhdr.dirofs) {
    logf(DEBUG, "invalid file data (extra chunk); cofs=0x%08x; dofs=0x%08x", chunkOfs, mhdr.dirofs);
    goto error;
  }

  wad->lastera = 1;

  // mark everything as unused
  for (unsigned f = 0; f < MAX_OPENED_FILES; ++f) wad->fds[f].fidx = VWAD_NOFIDX;

  // default cache settings
  vwad_set_archive_cache(wad, 4);

  return wad;

error:
  if (wad != NULL) {
    xfree(mman, wad->updir);
    memset(wad, 0, sizeof(vwad_handle)); // just in case
    xfree(mman, wad);
  }
  logf(ERROR, "vwad_open_archive: cannot parse directory");
  return NULL;
}


//==========================================================================
//
//  vwad_close_archive
//
//==========================================================================
void vwad_close_archive (vwad_handle **wadp) {
  if (wadp) {
    vwad_handle *wad = *wadp;
    if (wad) {
      *wadp = NULL;
      vwad_memman *mman = wad->mman;
      // there is no need to close opened files, nothing is allocated for them
      for (unsigned c = 0; c < MAX_GLOB_BUFFERS; ++c) {
        xfree(mman, wad->globCache[c]);
      }
      xfree(mman, wad->updir);
      xfree(mman, wad->comment);
      memset(wad, 0, sizeof(vwad_handle)); // just in case
      xfree(mman, wad);
    }
  }
}


//==========================================================================
//
//  vwad_set_archive_cache
//
//==========================================================================
void vwad_set_archive_cache (vwad_handle *wad, int chunkCount) {
  if (wad != NULL) {
    if (chunkCount < 0) chunkCount = 0;
    else if (chunkCount > MAX_GLOB_BUFFERS) chunkCount = MAX_GLOB_BUFFERS;
    if (wad->globCacheSize != (uint32_t)chunkCount) {
      // file reader will check and invalidate buffers
      // there is no need to flush the cache, file reader will do the trick
      // but we need to free extra unused buffers
      // free extra buffers
      // for local caching, we may have duplicate buffers in global cache; just free 'em all!
      for (int c = chunkCount; c < MAX_GLOB_BUFFERS; ++c) {
        xfree(wad->mman, wad->globCache[c]);
        wad->globCache[c] = NULL;
      }
      wad->globCacheSize = (uint32_t)chunkCount;
    }
  }
}


//==========================================================================
//
//  vwad_get_archive_comment_size
//
//==========================================================================
unsigned int vwad_get_archive_comment_size (vwad_handle *wad) {
  return (wad && wad->comment ? (unsigned int)strlen(wad->comment) : 0);
}


//==========================================================================
//
//  vwad_get_archive_comment
//
//==========================================================================
void vwad_get_archive_comment (vwad_handle *wad, char *dest, unsigned int destsize) {
  if (!wad || !wad->comment || destsize < 2 || !dest) {
    if (dest && destsize) dest[0] = 0;
  } else {
    unsigned int csize = (unsigned int)strlen(wad->comment);
    if (csize > destsize) csize = destsize;
    if (csize) memcpy(dest, wad->comment, csize);
    dest[csize] = 0;
  }
}


//==========================================================================
//
//  vwad_get_archive_author
//
//==========================================================================
const char *vwad_get_archive_author (vwad_handle *wad) {
  return (wad ? wad->author : "");
}


//==========================================================================
//
//  vwad_get_archive_title
//
//==========================================================================
const char *vwad_get_archive_title (vwad_handle *wad) {
  return (wad ? wad->title : "");
}


//==========================================================================
//
//  vwad_free_archive_comment
//
//  forget main archive comment and free its memory.
//
//==========================================================================
void vwad_free_archive_comment (vwad_handle *wad) {
  if (wad && wad->comment) {
    xfree(wad->mman, wad->comment);
    wad->comment = NULL;
  }
}


//==========================================================================
//
//  vwad_is_authenticated
//
//==========================================================================
vwad_bool vwad_is_authenticated (vwad_handle *wad) {
  return (wad && (wad->haspubkey & 0x02) != 0);
}


//==========================================================================
//
//  vwad_has_pubkey
//
//==========================================================================
vwad_bool vwad_has_pubkey (vwad_handle *wad) {
  return (wad && (wad->haspubkey & 0x01) != 0);
}


//==========================================================================
//
//  vwad_get_pubkey
//
//  return 0 on success, otherwise `pubkey` content is undefined
//
//==========================================================================
vwad_result vwad_get_pubkey (vwad_handle *wad, vwad_public_key pubkey) {
  if (wad && wad->haspubkey) {
    if (pubkey) memcpy(pubkey, wad->pubkey, sizeof(wad->pubkey));
    return 0;
  } else {
    return -1;
  }
}


//==========================================================================
//
//  vwad_get_archive_file_count
//
//==========================================================================
vwad_fidx vwad_get_archive_file_count (vwad_handle *wad) {
  return (wad ? (vwad_fidx)wad->fileCount : 0);
}


//==========================================================================
//
//  vwad_get_file_name
//
//==========================================================================
const char *vwad_get_file_name (vwad_handle *wad, vwad_fidx fidx) {
  if (wad && fidx >= 0 && fidx < (vwad_fidx)wad->fileCount) {
    return wad->names + wad->files[fidx].nameofs;
  } else {
    return NULL;
  }
}


//==========================================================================
//
//  vwad_get_file_group_name
//
//==========================================================================
const char *vwad_get_file_group_name (vwad_handle *wad, vwad_fidx fidx) {
  if (wad && fidx >= 0 && fidx < (vwad_fidx)wad->fileCount) {
    return wad->names + wad->files[fidx].gnameofs;
  } else {
    return NULL;
  }
}


//==========================================================================
//
//  vwad_get_file_size
//
//==========================================================================
int vwad_get_file_size (vwad_handle *wad, vwad_fidx fidx) {
  if (wad && fidx >= 0 && fidx < (vwad_fidx)wad->fileCount) {
    return wad->files[fidx].upksize;
  } else {
    return -1;
  }
}


//==========================================================================
//
//  find_file
//
//==========================================================================
static vwad_fidx find_file (vwad_handle *wad, const char *name) {
  if (name != NULL) {
    for (;;) {
      if (name[0] == '/') ++name;
      else if (name[0] == '.' && name[1] == '/') name += 2;
      else break;
    }
  }
  if (wad && wad->fileCount > 0 && name != NULL && name[0]) {
    const uint32_t hash = hashStrCI(name);
    const uint32_t bkt = hash % HASH_BUCKETS;
    uint32_t fidx = wad->buckets[bkt];
    while (fidx != 0) {
      if (wad->files[fidx].nhash == hash &&
          strEquCI(wad->names + wad->files[fidx].nameofs, name))
      {
        return (vwad_fidx)fidx;
      }
      fidx = wad->files[fidx].bknext;
    }
  }
  return 0;
}


//==========================================================================
//
//  vwad_has_file
//
//==========================================================================
vwad_fidx vwad_find_file (vwad_handle *wad, const char *name) {
  return find_file(wad, name);
}


//==========================================================================
//
//  vwad_get_ftime
//
//  returns 0 if there is an error, or the time is not set
//
//==========================================================================
unsigned long long vwad_get_ftime (vwad_handle *wad, vwad_fidx fidx) {
  uint64_t res = 0;
  if (wad && fidx >= 0 && fidx < (vwad_fidx)wad->fileCount) {
    res = wad->files[fidx].ftime;
  }
  return res;
}


//==========================================================================
//
//  vwad_get_fcrc32
//
//  get crc32 for the whole file
//
//==========================================================================
unsigned vwad_get_fcrc32 (vwad_handle *wad, vwad_fidx fidx) {
  uint32_t res = 0;
  if (wad && fidx >= 0 && fidx < (vwad_fidx)wad->fileCount) {
    res = wad->files[fidx].crc32;
  }
  return res;
}


//==========================================================================
//
//  vwad_fopen
//
//  return file handle or -1
//  note that maximum number of simultaneously opened files is 128
//
//==========================================================================
vwad_fd vwad_fopen (vwad_handle *wad, vwad_fidx fidx) {
  if (wad && fidx >= 0 && fidx < (vwad_fidx)wad->fileCount) {
    // find free fd
    vwad_fd fd = 0;
    while (fd < MAX_OPENED_FILES && wad->fds[fd].fidx != VWAD_NOFIDX) fd += 1;
    if (fd != MAX_OPENED_FILES) {
      // i found her!
      OpenedFile *fl = &wad->fds[fd];
      fl->fidx = fidx;
      fl->fofs = 0;
      fl->bidx = 0;
      if (wad->fdsUsed < fd) wad->fdsUsed = fd;
      if (vwad_debug_open_file) vwad_debug_open_file(wad, fidx, fd);
      return fd;
    } else {
      return -2;
    }
  }
  return -1;
}


//==========================================================================
//
//  vwad_fclose
//
//==========================================================================
void vwad_fclose (vwad_handle *wad, vwad_fd fd) {
  if (wad && fd >= 0 && fd < MAX_OPENED_FILES) {
    OpenedFile *fl = &wad->fds[fd];
    if (fl->fidx != VWAD_NOFIDX) {
      if (vwad_debug_close_file) vwad_debug_close_file(wad, fl->fidx, fd);
      fl->fidx = VWAD_NOFIDX;
      // in local cache mode, free the corresponding buffer
      if (wad->globCacheSize == 0) {
        xfree(wad->mman, wad->globCache[fd]);
        wad->globCache[fd] = NULL;
      }
      // fix max fd
      if (fd == wad->fdsUsed) {
        while (fd >= 0 && wad->fds[fd].fidx == VWAD_NOFIDX) --fd;
        wad->fdsUsed = fd + 1;
      }
    }
  }
}


//==========================================================================
//
//  vwad_has_opened_files
//
//==========================================================================
vwad_bool vwad_has_opened_files (vwad_handle *wad) {
  return (wad && wad->fdsUsed > 0);
}


//==========================================================================
//
//  read_chunk
//
//==========================================================================
static vwad_result read_chunk (vwad_handle *wad, OpenedFile *fl, FileBuffer *buf, uint32_t cidx) {
  vassert(wad);
  vassert(cidx < wad->chunkCount);

  const uint32_t nonce = 4 + cidx;
  const ChunkInfo *ci = &wad->chunks[cidx];
  uint32_t cupsize = ci->upksize + 1;
  //vassert(cupsize > 0 && cupsize <= 65536);

  // seek to chunk
  if (wad->strm->seek(wad->strm, ci->ofs) != 0) {
    logf(ERROR, "read_chunk: cannot seek to chunk %u", cidx);
    return -1;
  }

  // read data
  if (ci->pksize == 0) {
    // unpacked
    if (wad->strm->read(wad->strm, &wad->pkdata[0], cupsize + 4) != 0) {
      buf->size = 0;
      logf(ERROR, "read_chunk: cannot read unpacked chunk %u", cidx);
      return -1;
    }
    crypt_buffer(wad->xorRndSeed, nonce, &wad->pkdata[0], cupsize + 4);
    memcpy(buf->data, &wad->pkdata[4], cupsize);
  } else {
    // packed
    if (wad->strm->read(wad->strm, &wad->pkdata[0], ci->pksize + 4) != 0) {
      buf->size = 0;
      logf(ERROR, "read_chunk: cannot read packed chunk %u", cidx);
      return -1;
    }
    crypt_buffer(wad->xorRndSeed, nonce, &wad->pkdata[0], ci->pksize + 4);
    if (!DecompressLZFF3(&wad->pkdata[4], ci->pksize, buf->data, cupsize)) {
      buf->size = 0;
      logf(ERROR, "read_chunk: cannot unpack chunk %u (%u -> %u)", cidx,
           ci->pksize, cupsize);
      return -1;
    }
  }

  // check crc
  if ((wad->flags & VWAD_OPEN_NO_CRC_CHECKS) == 0) {
    if (crc32_buf(buf->data, cupsize) != get_u32(&wad->pkdata[0])) {
      buf->size = 0;
      logf(ERROR, "read_chunk: corrupted chunk %u data (crc32)", cidx);
      return -1;
    }
  }

  buf->cidx = cidx;
  buf->size = cupsize;

  return 0;
}


//==========================================================================
//
//  ensure_buffer
//
//==========================================================================
static FileBuffer *ensure_buffer (vwad_handle *wad, vwad_fd fd, uint32_t ofs) {
  vassert(wad != NULL);
  vassert(fd >= 0 && fd < MAX_OPENED_FILES);

  OpenedFile *fl = &wad->fds[fd];
  vassert(fl->fidx != VWAD_NOFIDX);

  const FileInfo *fi = &wad->files[fl->fidx];
  if (ofs >= fi->upksize) return NULL;

  const uint32_t cidx = fi->fchunk + ofs / 65536;
  vassert(cidx >= fi->fchunk && cidx < fi->fchunk + fi->chunkCount);
  vassert(fl->bidx < MAX_GLOB_BUFFERS);
  FileBuffer *res = wad->globCache[fl->bidx];

  // is buffer valid?
  if (!res || res->cidx != cidx || res->size == 0) {
    // no, check if we already have the chunk buffered
    const uint32_t gbcSize = wad->globCacheSize;
    uint32_t bidx;
    FileBuffer *gb;
    uint32_t ggevict = ~0u;
    uint32_t goodera = 0xffffffffU;
    vwad_bool gfound = 0;
    vwad_bool ggevict_empty = 0;

    // for local cache, `gbcSize` is 0, so it is ok
    for (bidx = 0; !gfound && bidx < gbcSize; ++bidx) {
      gb = wad->globCache[bidx];
      if (gb != NULL && gb->size != 0 && gb->cidx == cidx) {
        // i found her!
        fl->bidx = bidx;
        gfound = 1;
      } else if (gb == NULL || gb->size == 0) {
        // empty global buffer
        if (!ggevict_empty) { ggevict = bidx; ggevict_empty = 1; }
      } else if (!ggevict_empty && gb != NULL && gb->era < goodera) {
        // non-empty global buffer, good for eviction
        ggevict = bidx; goodera = gb->era;
      }
    }

    if (gfound == 0) {
      if (gbcSize == 0) {
        // local cache
        vassert(ggevict == ~0u);
        ggevict = (uint32_t)fd;
      } else {
        // global cache
        vassert(ggevict != ~0u);
      }
      gb = wad->globCache[ggevict];
      if (vwad_debug_flush_chunk && gb && gb->size != 0) {
        vwad_debug_flush_chunk(wad, (int)ggevict, fl->fidx, fd, (int)gb->cidx);
      }
      if (gb == NULL) {
        // new buffer
        gb = xalloc(wad->mman, (uint32_t)sizeof(FileBuffer));
        if (gb == NULL) {
          logf(ERROR, "ensure_buffer: cannot allocate memory for chunk cache");
          return NULL;
        }
        gb->cidx = 0;
        gb->size = 0;
        gb->era = 0;
        wad->globCache[ggevict] = gb;
      }
      if (vwad_debug_read_chunk) {
        vwad_debug_read_chunk(wad, (int)ggevict, fl->fidx, fd, (int)cidx);
      }
      // we need to read the chunk
      if (read_chunk(wad, fl, gb, cidx) != 0) {
        return NULL;
      }
      fl->bidx = ggevict;
    }
  }

  // i found her!
  res = wad->globCache[fl->bidx];
  vassert(res != NULL);
  vassert(res->cidx == cidx);
  vassert(res->size == wad->chunks[cidx].upksize + 1);

  // fix buffer era
  if (res->era != wad->lastera) {
    if (wad->lastera == 0xffffffffU) {
      wad->lastera = 1;
      for (uint32_t f = 0; f < wad->globCacheSize; ++f) {
        if (wad->globCache[f] != NULL) wad->globCache[f]->era = 0;
      }
    }
    res->era = wad->lastera;
    ++wad->lastera;
  }

  // done
  return res;
}


//==========================================================================
//
//  vwad_read
//
//  return number of read bytes, or -1 on error
//
//==========================================================================
int vwad_read (vwad_handle *wad, vwad_fd fd, void *dest, int len) {
  int read = -1;
  if (wad && len >= 0 && fd >= 0 && fd < MAX_OPENED_FILES) {
    OpenedFile *fl = &wad->fds[fd];
    if (fl->fidx != VWAD_NOFIDX) {
      vassert(len == 0 || dest != NULL);
      const FileInfo *fi = &wad->files[fl->fidx];
      read = 0;
      while (len != 0 && fl->fofs < wad->files[fl->fidx].upksize) {
        const FileBuffer *fbuf = ensure_buffer(wad, fd, fl->fofs);
        if (!fbuf) {
          // oops
          read = -1;
          len = 0;
        } else {
          const uint32_t left = wad->files[fl->fidx].upksize - fl->fofs;
          uint32_t rd = (uint32_t)len;
          if (rd > left) rd = left;
          vassert(fbuf->size > 0 && fbuf->size <= 65536);
          const uint32_t bufskip = fl->fofs - (fbuf->cidx - fi->fchunk) * 65536;
          vassert(bufskip <= fbuf->size);
          const uint32_t bufleft = fbuf->size - bufskip;
          vassert(bufleft > 0);
          if (rd > bufleft) rd = bufleft;
          #if 0
          fprintf(stderr, "READ: ofs=0x%08x; len=%d; rd=%u; bufleft=%u; bufskip=%u\n",
                  fl->fofs, len, rd, bufleft, bufskip);
          #endif
          vassert(rd > 0 && rd <= (uint32_t)len);
          memcpy(dest, fbuf->data + bufskip, rd);
          len -= (int)rd;
          fl->fofs += rd;
          read += (int)rd;
          dest = (void *)((uint8_t *)dest + rd);
        }
      }
    }
  }
  return read;
}


//==========================================================================
//
//  vwad_fdfidx
//
//==========================================================================
vwad_fidx vwad_fdfidx (vwad_handle *wad, vwad_fd fd) {
  if (wad && fd >= 0 && fd < MAX_OPENED_FILES) {
    OpenedFile *fl = &wad->fds[fd];
    if (fl->fidx != VWAD_NOFIDX) return (vwad_fidx)fl->fidx;
  }
  return -1;
}


//==========================================================================
//
//  vwad_seek
//
//  return 0 on success
//
//==========================================================================
vwad_result vwad_seek (vwad_handle *wad, vwad_fd fd, int pos) {
  int res = -1;
  if (wad && pos >= 0 && fd >= 0 && fd < MAX_OPENED_FILES) {
    OpenedFile *fl = &wad->fds[fd];
    if (fl->fidx != VWAD_NOFIDX) {
      if ((uint32_t)pos <= wad->files[fl->fidx].upksize) {
        fl->fofs = (uint32_t)pos;
        res = 0;
      } else {
        res = -3;
      }
    } else {
      res = -4;
    }
  }
  return res;
}


//==========================================================================
//
//  vwad_tell
//
//==========================================================================
int vwad_tell (vwad_handle *wad, vwad_fd fd) {
  if (wad && fd >= 0 && fd < MAX_OPENED_FILES) {
    OpenedFile *fl = &wad->fds[fd];
    if (fl->fidx != VWAD_NOFIDX) return (int)fl->fofs;
  }
  return -1;
}


//==========================================================================
//
//  vwad_get_file_chunk_count
//
//  this is used in creator, to copy raw chunks
//
//==========================================================================
int vwad_get_file_chunk_count (vwad_handle *wad, vwad_fidx fidx) {
  if (wad && fidx >= 0 && fidx < (vwad_fidx)wad->fileCount) {
    return (int)wad->files[fidx].chunkCount;
  }
  return -1;
}


//==========================================================================
//
//  vwad_get_file_chunk_size
//
//  this is used in creator, to copy raw chunks
//
//==========================================================================
int vwad_get_file_chunk_size (vwad_handle *wad, vwad_fidx fidx, int chunkidx, int *upksz) {
  if (wad && fidx >= 0 && fidx < (vwad_fidx)wad->fileCount &&
      chunkidx >= 0 && chunkidx < (int)wad->files[fidx].chunkCount)
  {
    const ChunkInfo *ci = &wad->chunks[wad->files[fidx].fchunk + (uint32_t)chunkidx];
    if (upksz) *upksz = (int)ci->upksize + 4;
    return (int)(ci->pksize == 0 ? ci->upksize : ci->pksize) + 4; /* with CRC32 */
  }
  return -1;
}


//==========================================================================
//
//  vwad_read_raw_file_chunk
//
//  this is used in creator, to copy raw chunks
//
//==========================================================================
vwad_result vwad_read_raw_file_chunk (vwad_handle *wad, vwad_fidx fidx, int chunkidx, void *buf) {
  if (wad && fidx >= 0 && fidx < (vwad_fidx)wad->fileCount &&
      chunkidx >= 0 && chunkidx < (int)wad->files[fidx].chunkCount && buf != NULL)
  {
    const FileInfo *fi = &wad->files[fidx];
    const ChunkInfo *ci = &wad->chunks[fi->fchunk + (uint32_t)chunkidx];
    const uint32_t csize = (ci->pksize == 0 ? ci->upksize : ci->pksize) + 4u; /* with CRC32 */
    if (wad->strm->seek(wad->strm, ci->ofs) != 0) return -1;
    if (wad->strm->read(wad->strm, buf, (int)csize) != 0) return -1;
    // decrypt chunk
    const uint32_t nonce = 4 + fi->fchunk + (uint32_t)chunkidx;
    crypt_buffer(wad->xorRndSeed, nonce, buf, csize);
    return 0;
  }
  return -1;
}


//==========================================================================
//
//  vwad_wildmatch
//
//  returns:
//    -1: malformed pattern
//     0: equal
//     1: not equal
//
//==========================================================================
vwad_result vwad_wildmatch (const char *pat, size_t plen, const char *str, size_t slen) {
  #define GETSCH(dst_)  do { \
    const char *stmp = &str[spos]; \
    const uint32_t uclen = utf_char_len(stmp); \
    if (error || uclen == 0 || uclen > 3 || slen - spos < uclen) { error = 1; (dst_) = VWAD_REPLACEMENT_CHAR; } \
    else { \
      (dst_) = unilower(utf_decode(&stmp)); \
      if ((dst_) < 32 || (dst_) == VWAD_REPLACEMENT_CHAR) error = 1; \
      spos += uclen; \
    } \
  } while (0)

  #define GETPCH(dst_)  do { \
    const char *stmp = &pat[patpos]; \
    const uint32_t uclen = utf_char_len(stmp); \
    if (error || uclen == 0 || uclen > 3 || plen - patpos < uclen) { error = 1; (dst_) = VWAD_REPLACEMENT_CHAR; } \
    else { \
      (dst_) = unilower(utf_decode(&stmp)); \
      if ((dst_) < 32 || (dst_) == VWAD_REPLACEMENT_CHAR) error = 1; \
      else patpos += uclen; \
    } \
  } while (0)

  uint16_t sch, c0, c1;
  vwad_bool hasMatch, inverted;
  vwad_bool star = 0, dostar = 0;
  size_t patpos = 0, spos = 0;
  int error = 0;

  while (!error && !dostar && spos < slen) {
    if (patpos == plen) {
      dostar = 1;
    } else {
      GETSCH(sch); GETPCH(c0);
      if (!error) {
        switch (c0) {
          case '\\':
            GETPCH(c0);
            dostar = (sch != c0);
            break;
          case '?': // match anything except '.'
            dostar = (sch == '.');
            break;
          case '*':
            star = 1;
            --spos; // otherwise "*abc" will not match "abc"
            slen -= spos; str += spos;
            plen -= patpos; pat += patpos;
            while (plen != 0 && *pat == '*') { --plen; ++pat; }
            // restart the loop
            spos = 0; patpos = 0;
            break;
          case '[':
            hasMatch = 0;
            inverted = 0;
            if (patpos == plen) error = 1; // malformed pattern
            else if (pat[patpos] == '^') {
              inverted = 1;
              ++patpos;
              error = (patpos == plen); // malformed pattern?
            }
            if (!error) {
              do {
                GETPCH(c0);
                if (!error && patpos != plen && pat[patpos] == '-') {
                  // char range
                  ++patpos; // skip '-'
                  GETPCH(c1);
                } else {
                  c1 = c0;
                }
                // casts are UB, hence the stupid macro
                hasMatch = hasMatch || (sch >= c0 && sch <= c1);
              } while (!error && patpos != plen && pat[patpos] != ']');
            }
            error = error || (patpos == plen) || (pat[patpos] != ']'); // malformed pattern?
            dostar = !error && (hasMatch != inverted);
            break;
          default:
            dostar = (sch != c0);
            break;
        }
      }
    }
    // star check
    if (dostar && !error) {
      // `error` is always zero here
      if (!star) {
        // not equal, do not reset "dostar"
        spos = slen;
      } else {
        dostar = 0;
        if (plen == 0) {
          // equal
          spos = slen;
        } else {
          --slen; ++str; // slen is never zero here
          spos = 0; patpos = 0;
        }
      }
    }
  }

  int res;
  if (error) res = -1; // malformed pattern
  else if (dostar) res = 1; // not equal
  else {
    plen -= patpos; pat += patpos;
    while (plen != 0 && *pat == '*') { --plen; ++pat; }
    if (plen == 0) res = 0; else res = 1;
  }
  return res;
}


//==========================================================================
//
//  vwad_wildmatch_path
//
//  this matches individual path parts
//  exception: if `pat` contains no slashes, match only the name
//
//==========================================================================
#ifdef VWAD_DEBUG_WILDPATH
#include <stdio.h>
#endif
vwad_result vwad_wildmatch_path (const char *pat, size_t plen, const char *str, size_t slen) {
  size_t ppos, spos;
  vwad_bool pat_has_slash = 0;
  vwad_result res;

  while (plen && pat[0] == '/') { pat_has_slash = 1; --plen; ++pat; }
  // look for slashes
  if (!pat_has_slash) {
    ppos = 0; while (ppos < plen && pat[ppos] != '/') ++ppos;
    pat_has_slash = (ppos < plen);
  }

  // if no slashes in pattern, match only filename
  if (!pat_has_slash) {
    spos = slen; while (spos != 0 && str[spos - 1] != '/') --spos;
    slen -= spos; str += spos;
    res = vwad_wildmatch(pat, plen, str, slen);
  } else {
    // match by path parts
    #ifdef VWAD_DEBUG_WILDPATH
    fprintf(stderr, "=== pat:<%.*s>; str:<%.*s> ===\n",
            (unsigned)plen, pat, (unsigned)slen, str);
    #endif
    while (slen && str[0] == '/') { --slen; ++str; }
    res = 0;
    while (res == 0 && plen != 0 && slen != 0) {
      // find slash in pat and in str
      ppos = 0; while (ppos != plen && pat[ppos] != '/') ++ppos;
      spos = 0; while (spos != slen && str[spos] != '/') ++spos;
      #ifdef VWAD_DEBUG_WILDPATH
      fprintf(stderr, "  MT: ppos=%u; spos=%u; pat=<%.*s>; str=<%.*s> (ex: %d)\n",
              (unsigned)ppos, (unsigned)spos,
              (unsigned)ppos, pat, (unsigned)spos, str,
              ((ppos == plen) != (spos == slen)));
      #endif
      if ((ppos == plen) != (spos == slen)) {
        res = 1;
      } else {
        res = vwad_wildmatch(pat, ppos, str, spos);
        plen -= ppos; pat += ppos;
        slen -= spos; str += spos;
        while (plen && pat[0] == '/') { --plen; ++pat; }
        while (slen && str[0] == '/') { --slen; ++str; }
      }
    }
    #ifdef VWAD_DEBUG_WILDPATH
    fprintf(stderr, " res: %d\n", res);
    #endif
  }

  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
#if defined(__cplusplus)
}
#endif
