/* coded by Ketmar // Invisible Vector (psyc://ketmar.no-ip.org/~Ketmar)
 * Understanding is not required. Only obedience.
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/wtfpl/COPYING for more details.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "vwadvfs.h"


typedef unsigned char ed25519_signature[64];
typedef unsigned char ed25519_public_key[32];
typedef unsigned char ed25519_secret_key[32];

#if !defined(VWAD_VFS_DISABLE_ED25519_CODE)
/*
  Public domain by Andrew M. <liquidsun@gmail.com>

  Ed25519 reference implementation using Ed25519-donna
*/
typedef struct ed25519_iostream_t ed25519_iostream;
struct ed25519_iostream_t {
  // return non-zero on failure
  int (*rewind) (ed25519_iostream *strm);
  // read up to bufsize bytes; return 0 on EOF, negative on failure
  // will never be called with 0 or negative `bufsize`
  int (*read) (ed25519_iostream *strm, void *buf, int bufsize);
  // user data
  void *udata;
};

/* moved above
typedef unsigned char ed25519_signature[64];
typedef unsigned char ed25519_public_key[32];
typedef unsigned char ed25519_secret_key[32];
*/

// return 0 on success, non-zero on failure
static int ed25519_sign_check_stream (ed25519_iostream *strm,
                                      const ed25519_public_key pk,
                                      const ed25519_signature RS);


/* reference/slow SHA-512. really, do not use this */
#define SHA512_HASH_BLOCK_SIZE  (128)
typedef struct ed25519_sha512_state_t {
  uint64_t H[8];
  uint64_t T[2];
  uint32_t leftover;
  uint8_t buffer[SHA512_HASH_BLOCK_SIZE];
} ed25519_sha512_state;

typedef struct ed25519_sha512_state_t ed25519_hash_context;
enum { ed25519_sha512_hash_size = 512/8 };
typedef uint8_t ed25519_sha512_hash[ed25519_sha512_hash_size];


static void ed25519_hash_init (ed25519_hash_context *S);
static void ed25519_hash_update (ed25519_hash_context *S, const void *in, size_t inlen);
static void ed25519_hash_final (ed25519_hash_context *S, ed25519_sha512_hash hash);
//static void ed25519_hash (ed25519_sha512_hash hash, const void *in, size_t inlen);


#include <sys/param.h>
#define DONNA_INLINE inline __attribute__((always_inline))
#define DONNA_NOINLINE __attribute__((noinline))
#define ALIGN(x) __attribute__((aligned(x)))
#define ROTL32(a,b) (((a) << (b)) | ((a) >> (32 - b)))
#define ROTR32(a,b) (((a) >> (b)) | ((a) << (32 - b)))

#define mul32x32_64(a,b) (((uint64_t)(a))*(b))

#if 0
# undef DONNA_INLINE
# define DONNA_INLINE
#endif

#if defined(__cplusplus)
extern "C" {
#endif

static DONNA_INLINE uint32_t U8TO32_LE(const unsigned char *p) {
  return
  (((uint32_t)(p[0])      ) |
   ((uint32_t)(p[1]) <<  8) |
   ((uint32_t)(p[2]) << 16) |
   ((uint32_t)(p[3]) << 24));
}


static size_t hash_stream (ed25519_iostream *strm, ed25519_hash_context *ctx) {
  size_t mlen = 0;
  uint8_t buf[1024];
  int rd;
  if (strm->rewind(strm) != 0) return 0;
  do {
    rd = strm->read(strm, buf, (int)sizeof(buf));
    if (rd > 0) {
      ed25519_hash_update(ctx, buf, (size_t)rd);
      mlen += (size_t)rd;
    }
  } while (rd > 0);
  return (rd < 0 ? 0 : mlen);
}


/*
  Public domain by Andrew M. <liquidsun@gmail.com>
  See: https://github.com/floodyberry/curve25519-donna

  32 bit integer curve25519 implementation
*/

typedef uint32_t bignum25519[10];
typedef uint32_t bignum25519align16[12];

static const uint32_t reduce_mask_25 = (1 << 25) - 1;
static const uint32_t reduce_mask_26 = (1 << 26) - 1;


/* out = in */
DONNA_INLINE static void
curve25519_copy(bignum25519 out, const bignum25519 in) {
  out[0] = in[0];
  out[1] = in[1];
  out[2] = in[2];
  out[3] = in[3];
  out[4] = in[4];
  out[5] = in[5];
  out[6] = in[6];
  out[7] = in[7];
  out[8] = in[8];
  out[9] = in[9];
}

/* out = a + b */
DONNA_INLINE static void
curve25519_add(bignum25519 out, const bignum25519 a, const bignum25519 b) {
  out[0] = a[0] + b[0];
  out[1] = a[1] + b[1];
  out[2] = a[2] + b[2];
  out[3] = a[3] + b[3];
  out[4] = a[4] + b[4];
  out[5] = a[5] + b[5];
  out[6] = a[6] + b[6];
  out[7] = a[7] + b[7];
  out[8] = a[8] + b[8];
  out[9] = a[9] + b[9];
}

DONNA_INLINE static void
curve25519_add_after_basic(bignum25519 out, const bignum25519 a, const bignum25519 b) {
  uint32_t c;
  out[0] = a[0] + b[0]    ; c = (out[0] >> 26); out[0] &= reduce_mask_26;
  out[1] = a[1] + b[1] + c; c = (out[1] >> 25); out[1] &= reduce_mask_25;
  out[2] = a[2] + b[2] + c; c = (out[2] >> 26); out[2] &= reduce_mask_26;
  out[3] = a[3] + b[3] + c; c = (out[3] >> 25); out[3] &= reduce_mask_25;
  out[4] = a[4] + b[4] + c; c = (out[4] >> 26); out[4] &= reduce_mask_26;
  out[5] = a[5] + b[5] + c; c = (out[5] >> 25); out[5] &= reduce_mask_25;
  out[6] = a[6] + b[6] + c; c = (out[6] >> 26); out[6] &= reduce_mask_26;
  out[7] = a[7] + b[7] + c; c = (out[7] >> 25); out[7] &= reduce_mask_25;
  out[8] = a[8] + b[8] + c; c = (out[8] >> 26); out[8] &= reduce_mask_26;
  out[9] = a[9] + b[9] + c; c = (out[9] >> 25); out[9] &= reduce_mask_25;
  out[0] += 19 * c;
}

DONNA_INLINE static void
curve25519_add_reduce(bignum25519 out, const bignum25519 a, const bignum25519 b) {
  uint32_t c;
  out[0] = a[0] + b[0]    ; c = (out[0] >> 26); out[0] &= reduce_mask_26;
  out[1] = a[1] + b[1] + c; c = (out[1] >> 25); out[1] &= reduce_mask_25;
  out[2] = a[2] + b[2] + c; c = (out[2] >> 26); out[2] &= reduce_mask_26;
  out[3] = a[3] + b[3] + c; c = (out[3] >> 25); out[3] &= reduce_mask_25;
  out[4] = a[4] + b[4] + c; c = (out[4] >> 26); out[4] &= reduce_mask_26;
  out[5] = a[5] + b[5] + c; c = (out[5] >> 25); out[5] &= reduce_mask_25;
  out[6] = a[6] + b[6] + c; c = (out[6] >> 26); out[6] &= reduce_mask_26;
  out[7] = a[7] + b[7] + c; c = (out[7] >> 25); out[7] &= reduce_mask_25;
  out[8] = a[8] + b[8] + c; c = (out[8] >> 26); out[8] &= reduce_mask_26;
  out[9] = a[9] + b[9] + c; c = (out[9] >> 25); out[9] &= reduce_mask_25;
  out[0] += 19 * c;
}

/* multiples of p */
static const uint32_t twoP0       = 0x07ffffda;
static const uint32_t twoP13579   = 0x03fffffe;
static const uint32_t twoP2468    = 0x07fffffe;
static const uint32_t fourP0      = 0x0fffffb4;
static const uint32_t fourP13579  = 0x07fffffc;
static const uint32_t fourP2468   = 0x0ffffffc;

/* out = a - b */
DONNA_INLINE static void
curve25519_sub(bignum25519 out, const bignum25519 a, const bignum25519 b) {
  uint32_t c;
  out[0] = twoP0     + a[0] - b[0]    ; c = (out[0] >> 26); out[0] &= reduce_mask_26;
  out[1] = twoP13579 + a[1] - b[1] + c; c = (out[1] >> 25); out[1] &= reduce_mask_25;
  out[2] = twoP2468  + a[2] - b[2] + c; c = (out[2] >> 26); out[2] &= reduce_mask_26;
  out[3] = twoP13579 + a[3] - b[3] + c; c = (out[3] >> 25); out[3] &= reduce_mask_25;
  out[4] = twoP2468  + a[4] - b[4] + c;
  out[5] = twoP13579 + a[5] - b[5]    ;
  out[6] = twoP2468  + a[6] - b[6]    ;
  out[7] = twoP13579 + a[7] - b[7]    ;
  out[8] = twoP2468  + a[8] - b[8]    ;
  out[9] = twoP13579 + a[9] - b[9]    ;
}

/* out = a - b, where a is the result of a basic op (add,sub) */
DONNA_INLINE static void
curve25519_sub_after_basic(bignum25519 out, const bignum25519 a, const bignum25519 b) {
  uint32_t c;
  out[0] = fourP0     + a[0] - b[0]    ; c = (out[0] >> 26); out[0] &= reduce_mask_26;
  out[1] = fourP13579 + a[1] - b[1] + c; c = (out[1] >> 25); out[1] &= reduce_mask_25;
  out[2] = fourP2468  + a[2] - b[2] + c; c = (out[2] >> 26); out[2] &= reduce_mask_26;
  out[3] = fourP13579 + a[3] - b[3] + c; c = (out[3] >> 25); out[3] &= reduce_mask_25;
  out[4] = fourP2468  + a[4] - b[4] + c; c = (out[4] >> 26); out[4] &= reduce_mask_26;
  out[5] = fourP13579 + a[5] - b[5] + c; c = (out[5] >> 25); out[5] &= reduce_mask_25;
  out[6] = fourP2468  + a[6] - b[6] + c; c = (out[6] >> 26); out[6] &= reduce_mask_26;
  out[7] = fourP13579 + a[7] - b[7] + c; c = (out[7] >> 25); out[7] &= reduce_mask_25;
  out[8] = fourP2468  + a[8] - b[8] + c; c = (out[8] >> 26); out[8] &= reduce_mask_26;
  out[9] = fourP13579 + a[9] - b[9] + c; c = (out[9] >> 25); out[9] &= reduce_mask_25;
  out[0] += 19 * c;
}

DONNA_INLINE static void
curve25519_sub_reduce(bignum25519 out, const bignum25519 a, const bignum25519 b) {
  uint32_t c;
  out[0] = fourP0     + a[0] - b[0]    ; c = (out[0] >> 26); out[0] &= reduce_mask_26;
  out[1] = fourP13579 + a[1] - b[1] + c; c = (out[1] >> 25); out[1] &= reduce_mask_25;
  out[2] = fourP2468  + a[2] - b[2] + c; c = (out[2] >> 26); out[2] &= reduce_mask_26;
  out[3] = fourP13579 + a[3] - b[3] + c; c = (out[3] >> 25); out[3] &= reduce_mask_25;
  out[4] = fourP2468  + a[4] - b[4] + c; c = (out[4] >> 26); out[4] &= reduce_mask_26;
  out[5] = fourP13579 + a[5] - b[5] + c; c = (out[5] >> 25); out[5] &= reduce_mask_25;
  out[6] = fourP2468  + a[6] - b[6] + c; c = (out[6] >> 26); out[6] &= reduce_mask_26;
  out[7] = fourP13579 + a[7] - b[7] + c; c = (out[7] >> 25); out[7] &= reduce_mask_25;
  out[8] = fourP2468  + a[8] - b[8] + c; c = (out[8] >> 26); out[8] &= reduce_mask_26;
  out[9] = fourP13579 + a[9] - b[9] + c; c = (out[9] >> 25); out[9] &= reduce_mask_25;
  out[0] += 19 * c;
}

/* out = -a */
DONNA_INLINE static void
curve25519_neg(bignum25519 out, const bignum25519 a) {
  uint32_t c;
  out[0] = twoP0     - a[0]    ; c = (out[0] >> 26); out[0] &= reduce_mask_26;
  out[1] = twoP13579 - a[1] + c; c = (out[1] >> 25); out[1] &= reduce_mask_25;
  out[2] = twoP2468  - a[2] + c; c = (out[2] >> 26); out[2] &= reduce_mask_26;
  out[3] = twoP13579 - a[3] + c; c = (out[3] >> 25); out[3] &= reduce_mask_25;
  out[4] = twoP2468  - a[4] + c; c = (out[4] >> 26); out[4] &= reduce_mask_26;
  out[5] = twoP13579 - a[5] + c; c = (out[5] >> 25); out[5] &= reduce_mask_25;
  out[6] = twoP2468  - a[6] + c; c = (out[6] >> 26); out[6] &= reduce_mask_26;
  out[7] = twoP13579 - a[7] + c; c = (out[7] >> 25); out[7] &= reduce_mask_25;
  out[8] = twoP2468  - a[8] + c; c = (out[8] >> 26); out[8] &= reduce_mask_26;
  out[9] = twoP13579 - a[9] + c; c = (out[9] >> 25); out[9] &= reduce_mask_25;
  out[0] += 19 * c;
}


/* out = a * b */
#define curve25519_mul_noinline curve25519_mul
static void
curve25519_mul(bignum25519 out, const bignum25519 a, const bignum25519 b) {
  uint32_t r0,r1,r2,r3,r4,r5,r6,r7,r8,r9;
  uint32_t s0,s1,s2,s3,s4,s5,s6,s7,s8,s9;
  uint64_t m0,m1,m2,m3,m4,m5,m6,m7,m8,m9,c;
  uint32_t p;

  r0 = b[0];
  r1 = b[1];
  r2 = b[2];
  r3 = b[3];
  r4 = b[4];
  r5 = b[5];
  r6 = b[6];
  r7 = b[7];
  r8 = b[8];
  r9 = b[9];

  s0 = a[0];
  s1 = a[1];
  s2 = a[2];
  s3 = a[3];
  s4 = a[4];
  s5 = a[5];
  s6 = a[6];
  s7 = a[7];
  s8 = a[8];
  s9 = a[9];

  m1 = mul32x32_64(r0, s1) + mul32x32_64(r1, s0);
  m3 = mul32x32_64(r0, s3) + mul32x32_64(r1, s2) + mul32x32_64(r2, s1) + mul32x32_64(r3, s0);
  m5 = mul32x32_64(r0, s5) + mul32x32_64(r1, s4) + mul32x32_64(r2, s3) + mul32x32_64(r3, s2) + mul32x32_64(r4, s1) + mul32x32_64(r5, s0);
  m7 = mul32x32_64(r0, s7) + mul32x32_64(r1, s6) + mul32x32_64(r2, s5) + mul32x32_64(r3, s4) + mul32x32_64(r4, s3) + mul32x32_64(r5, s2) + mul32x32_64(r6, s1) + mul32x32_64(r7, s0);
  m9 = mul32x32_64(r0, s9) + mul32x32_64(r1, s8) + mul32x32_64(r2, s7) + mul32x32_64(r3, s6) + mul32x32_64(r4, s5) + mul32x32_64(r5, s4) + mul32x32_64(r6, s3) + mul32x32_64(r7, s2) + mul32x32_64(r8, s1) + mul32x32_64(r9, s0);

  r1 *= 2;
  r3 *= 2;
  r5 *= 2;
  r7 *= 2;

  m0 = mul32x32_64(r0, s0);
  m2 = mul32x32_64(r0, s2) + mul32x32_64(r1, s1) + mul32x32_64(r2, s0);
  m4 = mul32x32_64(r0, s4) + mul32x32_64(r1, s3) + mul32x32_64(r2, s2) + mul32x32_64(r3, s1) + mul32x32_64(r4, s0);
  m6 = mul32x32_64(r0, s6) + mul32x32_64(r1, s5) + mul32x32_64(r2, s4) + mul32x32_64(r3, s3) + mul32x32_64(r4, s2) + mul32x32_64(r5, s1) + mul32x32_64(r6, s0);
  m8 = mul32x32_64(r0, s8) + mul32x32_64(r1, s7) + mul32x32_64(r2, s6) + mul32x32_64(r3, s5) + mul32x32_64(r4, s4) + mul32x32_64(r5, s3) + mul32x32_64(r6, s2) + mul32x32_64(r7, s1) + mul32x32_64(r8, s0);

  r1 *= 19;
  r2 *= 19;
  r3 = (r3 / 2) * 19;
  r4 *= 19;
  r5 = (r5 / 2) * 19;
  r6 *= 19;
  r7 = (r7 / 2) * 19;
  r8 *= 19;
  r9 *= 19;

  m1 += (mul32x32_64(r9, s2) + mul32x32_64(r8, s3) + mul32x32_64(r7, s4) + mul32x32_64(r6, s5) + mul32x32_64(r5, s6) + mul32x32_64(r4, s7) + mul32x32_64(r3, s8) + mul32x32_64(r2, s9));
  m3 += (mul32x32_64(r9, s4) + mul32x32_64(r8, s5) + mul32x32_64(r7, s6) + mul32x32_64(r6, s7) + mul32x32_64(r5, s8) + mul32x32_64(r4, s9));
  m5 += (mul32x32_64(r9, s6) + mul32x32_64(r8, s7) + mul32x32_64(r7, s8) + mul32x32_64(r6, s9));
  m7 += (mul32x32_64(r9, s8) + mul32x32_64(r8, s9));

  r3 *= 2;
  r5 *= 2;
  r7 *= 2;
  r9 *= 2;

  m0 += (mul32x32_64(r9, s1) + mul32x32_64(r8, s2) + mul32x32_64(r7, s3) + mul32x32_64(r6, s4) + mul32x32_64(r5, s5) + mul32x32_64(r4, s6) + mul32x32_64(r3, s7) + mul32x32_64(r2, s8) + mul32x32_64(r1, s9));
  m2 += (mul32x32_64(r9, s3) + mul32x32_64(r8, s4) + mul32x32_64(r7, s5) + mul32x32_64(r6, s6) + mul32x32_64(r5, s7) + mul32x32_64(r4, s8) + mul32x32_64(r3, s9));
  m4 += (mul32x32_64(r9, s5) + mul32x32_64(r8, s6) + mul32x32_64(r7, s7) + mul32x32_64(r6, s8) + mul32x32_64(r5, s9));
  m6 += (mul32x32_64(r9, s7) + mul32x32_64(r8, s8) + mul32x32_64(r7, s9));
  m8 += (mul32x32_64(r9, s9));

                               r0 = (uint32_t)m0 & reduce_mask_26; c = (m0 >> 26);
  m1 += c;                     r1 = (uint32_t)m1 & reduce_mask_25; c = (m1 >> 25);
  m2 += c;                     r2 = (uint32_t)m2 & reduce_mask_26; c = (m2 >> 26);
  m3 += c;                     r3 = (uint32_t)m3 & reduce_mask_25; c = (m3 >> 25);
  m4 += c;                     r4 = (uint32_t)m4 & reduce_mask_26; c = (m4 >> 26);
  m5 += c;                     r5 = (uint32_t)m5 & reduce_mask_25; c = (m5 >> 25);
  m6 += c;                     r6 = (uint32_t)m6 & reduce_mask_26; c = (m6 >> 26);
  m7 += c;                     r7 = (uint32_t)m7 & reduce_mask_25; c = (m7 >> 25);
  m8 += c;                     r8 = (uint32_t)m8 & reduce_mask_26; c = (m8 >> 26);
  m9 += c;                     r9 = (uint32_t)m9 & reduce_mask_25; p = (uint32_t)(m9 >> 25);
  m0 = r0 + mul32x32_64(p,19); r0 = (uint32_t)m0 & reduce_mask_26; p = (uint32_t)(m0 >> 26);
  r1 += p;

  out[0] = r0;
  out[1] = r1;
  out[2] = r2;
  out[3] = r3;
  out[4] = r4;
  out[5] = r5;
  out[6] = r6;
  out[7] = r7;
  out[8] = r8;
  out[9] = r9;
}

/* out = in*in */
static void
curve25519_square(bignum25519 out, const bignum25519 in) {
  uint32_t r0,r1,r2,r3,r4,r5,r6,r7,r8,r9;
  uint32_t d6,d7,d8,d9;
  uint64_t m0,m1,m2,m3,m4,m5,m6,m7,m8,m9,c;
  uint32_t p;

  r0 = in[0];
  r1 = in[1];
  r2 = in[2];
  r3 = in[3];
  r4 = in[4];
  r5 = in[5];
  r6 = in[6];
  r7 = in[7];
  r8 = in[8];
  r9 = in[9];

  m0 = mul32x32_64(r0, r0);
  r0 *= 2;
  m1 = mul32x32_64(r0, r1);
  m2 = mul32x32_64(r0, r2) + mul32x32_64(r1, r1 * 2);
  r1 *= 2;
  m3 = mul32x32_64(r0, r3) + mul32x32_64(r1, r2    );
  m4 = mul32x32_64(r0, r4) + mul32x32_64(r1, r3 * 2) + mul32x32_64(r2, r2);
  r2 *= 2;
  m5 = mul32x32_64(r0, r5) + mul32x32_64(r1, r4    ) + mul32x32_64(r2, r3);
  m6 = mul32x32_64(r0, r6) + mul32x32_64(r1, r5 * 2) + mul32x32_64(r2, r4) + mul32x32_64(r3, r3 * 2);
  r3 *= 2;
  m7 = mul32x32_64(r0, r7) + mul32x32_64(r1, r6    ) + mul32x32_64(r2, r5) + mul32x32_64(r3, r4    );
  m8 = mul32x32_64(r0, r8) + mul32x32_64(r1, r7 * 2) + mul32x32_64(r2, r6) + mul32x32_64(r3, r5 * 2) + mul32x32_64(r4, r4    );
  m9 = mul32x32_64(r0, r9) + mul32x32_64(r1, r8    ) + mul32x32_64(r2, r7) + mul32x32_64(r3, r6    ) + mul32x32_64(r4, r5 * 2);

  d6 = r6 * 19;
  d7 = r7 * 2 * 19;
  d8 = r8 * 19;
  d9 = r9 * 2 * 19;

  m0 += (mul32x32_64(d9, r1    ) + mul32x32_64(d8, r2    ) + mul32x32_64(d7, r3    ) + mul32x32_64(d6, r4 * 2) + mul32x32_64(r5, r5 * 2 * 19));
  m1 += (mul32x32_64(d9, r2 / 2) + mul32x32_64(d8, r3    ) + mul32x32_64(d7, r4    ) + mul32x32_64(d6, r5 * 2));
  m2 += (mul32x32_64(d9, r3    ) + mul32x32_64(d8, r4 * 2) + mul32x32_64(d7, r5 * 2) + mul32x32_64(d6, r6    ));
  m3 += (mul32x32_64(d9, r4    ) + mul32x32_64(d8, r5 * 2) + mul32x32_64(d7, r6    ));
  m4 += (mul32x32_64(d9, r5 * 2) + mul32x32_64(d8, r6 * 2) + mul32x32_64(d7, r7    ));
  m5 += (mul32x32_64(d9, r6    ) + mul32x32_64(d8, r7 * 2));
  m6 += (mul32x32_64(d9, r7 * 2) + mul32x32_64(d8, r8    ));
  m7 += (mul32x32_64(d9, r8    ));
  m8 += (mul32x32_64(d9, r9    ));

                               r0 = (uint32_t)m0 & reduce_mask_26; c = (m0 >> 26);
  m1 += c;                     r1 = (uint32_t)m1 & reduce_mask_25; c = (m1 >> 25);
  m2 += c;                     r2 = (uint32_t)m2 & reduce_mask_26; c = (m2 >> 26);
  m3 += c;                     r3 = (uint32_t)m3 & reduce_mask_25; c = (m3 >> 25);
  m4 += c;                     r4 = (uint32_t)m4 & reduce_mask_26; c = (m4 >> 26);
  m5 += c;                     r5 = (uint32_t)m5 & reduce_mask_25; c = (m5 >> 25);
  m6 += c;                     r6 = (uint32_t)m6 & reduce_mask_26; c = (m6 >> 26);
  m7 += c;                     r7 = (uint32_t)m7 & reduce_mask_25; c = (m7 >> 25);
  m8 += c;                     r8 = (uint32_t)m8 & reduce_mask_26; c = (m8 >> 26);
  m9 += c;                     r9 = (uint32_t)m9 & reduce_mask_25; p = (uint32_t)(m9 >> 25);
  m0 = r0 + mul32x32_64(p,19); r0 = (uint32_t)m0 & reduce_mask_26; p = (uint32_t)(m0 >> 26);
  r1 += p;

  out[0] = r0;
  out[1] = r1;
  out[2] = r2;
  out[3] = r3;
  out[4] = r4;
  out[5] = r5;
  out[6] = r6;
  out[7] = r7;
  out[8] = r8;
  out[9] = r9;
}


/* out = in ^ (2 * count) */
static void
curve25519_square_times(bignum25519 out, const bignum25519 in, int count) {
  uint32_t r0,r1,r2,r3,r4,r5,r6,r7,r8,r9;
  uint32_t d6,d7,d8,d9;
  uint64_t m0,m1,m2,m3,m4,m5,m6,m7,m8,m9,c;
  uint32_t p;

  r0 = in[0];
  r1 = in[1];
  r2 = in[2];
  r3 = in[3];
  r4 = in[4];
  r5 = in[5];
  r6 = in[6];
  r7 = in[7];
  r8 = in[8];
  r9 = in[9];

  do {
    m0 = mul32x32_64(r0, r0);
    r0 *= 2;
    m1 = mul32x32_64(r0, r1);
    m2 = mul32x32_64(r0, r2) + mul32x32_64(r1, r1 * 2);
    r1 *= 2;
    m3 = mul32x32_64(r0, r3) + mul32x32_64(r1, r2    );
    m4 = mul32x32_64(r0, r4) + mul32x32_64(r1, r3 * 2) + mul32x32_64(r2, r2);
    r2 *= 2;
    m5 = mul32x32_64(r0, r5) + mul32x32_64(r1, r4    ) + mul32x32_64(r2, r3);
    m6 = mul32x32_64(r0, r6) + mul32x32_64(r1, r5 * 2) + mul32x32_64(r2, r4) + mul32x32_64(r3, r3 * 2);
    r3 *= 2;
    m7 = mul32x32_64(r0, r7) + mul32x32_64(r1, r6    ) + mul32x32_64(r2, r5) + mul32x32_64(r3, r4    );
    m8 = mul32x32_64(r0, r8) + mul32x32_64(r1, r7 * 2) + mul32x32_64(r2, r6) + mul32x32_64(r3, r5 * 2) + mul32x32_64(r4, r4    );
    m9 = mul32x32_64(r0, r9) + mul32x32_64(r1, r8    ) + mul32x32_64(r2, r7) + mul32x32_64(r3, r6    ) + mul32x32_64(r4, r5 * 2);

    d6 = r6 * 19;
    d7 = r7 * 2 * 19;
    d8 = r8 * 19;
    d9 = r9 * 2 * 19;

    m0 += (mul32x32_64(d9, r1    ) + mul32x32_64(d8, r2    ) + mul32x32_64(d7, r3    ) + mul32x32_64(d6, r4 * 2) + mul32x32_64(r5, r5 * 2 * 19));
    m1 += (mul32x32_64(d9, r2 / 2) + mul32x32_64(d8, r3    ) + mul32x32_64(d7, r4    ) + mul32x32_64(d6, r5 * 2));
    m2 += (mul32x32_64(d9, r3    ) + mul32x32_64(d8, r4 * 2) + mul32x32_64(d7, r5 * 2) + mul32x32_64(d6, r6    ));
    m3 += (mul32x32_64(d9, r4    ) + mul32x32_64(d8, r5 * 2) + mul32x32_64(d7, r6    ));
    m4 += (mul32x32_64(d9, r5 * 2) + mul32x32_64(d8, r6 * 2) + mul32x32_64(d7, r7    ));
    m5 += (mul32x32_64(d9, r6    ) + mul32x32_64(d8, r7 * 2));
    m6 += (mul32x32_64(d9, r7 * 2) + mul32x32_64(d8, r8    ));
    m7 += (mul32x32_64(d9, r8    ));
    m8 += (mul32x32_64(d9, r9    ));

                                 r0 = (uint32_t)m0 & reduce_mask_26; c = (m0 >> 26);
    m1 += c;                     r1 = (uint32_t)m1 & reduce_mask_25; c = (m1 >> 25);
    m2 += c;                     r2 = (uint32_t)m2 & reduce_mask_26; c = (m2 >> 26);
    m3 += c;                     r3 = (uint32_t)m3 & reduce_mask_25; c = (m3 >> 25);
    m4 += c;                     r4 = (uint32_t)m4 & reduce_mask_26; c = (m4 >> 26);
    m5 += c;                     r5 = (uint32_t)m5 & reduce_mask_25; c = (m5 >> 25);
    m6 += c;                     r6 = (uint32_t)m6 & reduce_mask_26; c = (m6 >> 26);
    m7 += c;                     r7 = (uint32_t)m7 & reduce_mask_25; c = (m7 >> 25);
    m8 += c;                     r8 = (uint32_t)m8 & reduce_mask_26; c = (m8 >> 26);
    m9 += c;                     r9 = (uint32_t)m9 & reduce_mask_25; p = (uint32_t)(m9 >> 25);
    m0 = r0 + mul32x32_64(p,19); r0 = (uint32_t)m0 & reduce_mask_26; p = (uint32_t)(m0 >> 26);
    r1 += p;
  } while (--count);

  out[0] = r0;
  out[1] = r1;
  out[2] = r2;
  out[3] = r3;
  out[4] = r4;
  out[5] = r5;
  out[6] = r6;
  out[7] = r7;
  out[8] = r8;
  out[9] = r9;
}

/* Take a little-endian, 32-byte number and expand it into polynomial form */
static void
curve25519_expand(bignum25519 out, const unsigned char in[32]) {
  static const union { uint8_t b[2]; uint16_t s; } endian_check = {{1,0}};
  uint32_t x0,x1,x2,x3,x4,x5,x6,x7;

  if (endian_check.s == 1) {
    x0 = *(uint32_t *)(in + 0);
    x1 = *(uint32_t *)(in + 4);
    x2 = *(uint32_t *)(in + 8);
    x3 = *(uint32_t *)(in + 12);
    x4 = *(uint32_t *)(in + 16);
    x5 = *(uint32_t *)(in + 20);
    x6 = *(uint32_t *)(in + 24);
    x7 = *(uint32_t *)(in + 28);
    } else {
    #define F(s)                         \
      ((((uint32_t)in[s + 0])      ) | \
       (((uint32_t)in[s + 1]) <<  8) | \
       (((uint32_t)in[s + 2]) << 16) | \
       (((uint32_t)in[s + 3]) << 24))
    x0 = F(0);
    x1 = F(4);
    x2 = F(8);
    x3 = F(12);
    x4 = F(16);
    x5 = F(20);
    x6 = F(24);
    x7 = F(28);
    #undef F
  }

  out[0] = (                        x0       ) & 0x3ffffff;
  out[1] = ((((uint64_t)x1 << 32) | x0) >> 26) & 0x1ffffff;
  out[2] = ((((uint64_t)x2 << 32) | x1) >> 19) & 0x3ffffff;
  out[3] = ((((uint64_t)x3 << 32) | x2) >> 13) & 0x1ffffff;
  out[4] = ((                       x3) >>  6) & 0x3ffffff;
  out[5] = (                        x4       ) & 0x1ffffff;
  out[6] = ((((uint64_t)x5 << 32) | x4) >> 25) & 0x3ffffff;
  out[7] = ((((uint64_t)x6 << 32) | x5) >> 19) & 0x1ffffff;
  out[8] = ((((uint64_t)x7 << 32) | x6) >> 12) & 0x3ffffff;
  out[9] = ((                       x7) >>  6) & 0x1ffffff;
}

/* Take a fully reduced polynomial form number and contract it into a
 * little-endian, 32-byte array
 */
static void
curve25519_contract(unsigned char out[32], const bignum25519 in) {
  bignum25519 f;
  curve25519_copy(f, in);

  #define carry_pass() \
    f[1] += f[0] >> 26; f[0] &= reduce_mask_26; \
    f[2] += f[1] >> 25; f[1] &= reduce_mask_25; \
    f[3] += f[2] >> 26; f[2] &= reduce_mask_26; \
    f[4] += f[3] >> 25; f[3] &= reduce_mask_25; \
    f[5] += f[4] >> 26; f[4] &= reduce_mask_26; \
    f[6] += f[5] >> 25; f[5] &= reduce_mask_25; \
    f[7] += f[6] >> 26; f[6] &= reduce_mask_26; \
    f[8] += f[7] >> 25; f[7] &= reduce_mask_25; \
    f[9] += f[8] >> 26; f[8] &= reduce_mask_26;

  #define carry_pass_full() \
    carry_pass() \
    f[0] += 19 * (f[9] >> 25); f[9] &= reduce_mask_25;

  #define carry_pass_final() \
    carry_pass() \
    f[9] &= reduce_mask_25;

  carry_pass_full()
  carry_pass_full()

  /* now t is between 0 and 2^255-1, properly carried. */
  /* case 1: between 0 and 2^255-20. case 2: between 2^255-19 and 2^255-1. */
  f[0] += 19;
  carry_pass_full()

  /* now between 19 and 2^255-1 in both cases, and offset by 19. */
  f[0] += (reduce_mask_26 + 1) - 19;
  f[1] += (reduce_mask_25 + 1) - 1;
  f[2] += (reduce_mask_26 + 1) - 1;
  f[3] += (reduce_mask_25 + 1) - 1;
  f[4] += (reduce_mask_26 + 1) - 1;
  f[5] += (reduce_mask_25 + 1) - 1;
  f[6] += (reduce_mask_26 + 1) - 1;
  f[7] += (reduce_mask_25 + 1) - 1;
  f[8] += (reduce_mask_26 + 1) - 1;
  f[9] += (reduce_mask_25 + 1) - 1;

  /* now between 2^255 and 2^256-20, and offset by 2^255. */
  carry_pass_final()

  #undef carry_pass
  #undef carry_full
  #undef carry_final

  f[1] <<= 2;
  f[2] <<= 3;
  f[3] <<= 5;
  f[4] <<= 6;
  f[6] <<= 1;
  f[7] <<= 3;
  f[8] <<= 4;
  f[9] <<= 6;

  #define F(i, s) \
    out[s+0] |= (unsigned char )(f[i] & 0xff); \
    out[s+1] = (unsigned char )((f[i] >> 8) & 0xff); \
    out[s+2] = (unsigned char )((f[i] >> 16) & 0xff); \
    out[s+3] = (unsigned char )((f[i] >> 24) & 0xff);

  out[0] = 0;
  out[16] = 0;
  F(0,0);
  F(1,3);
  F(2,6);
  F(3,9);
  F(4,12);
  F(5,16);
  F(6,19);
  F(7,22);
  F(8,25);
  F(9,28);
  #undef F
}


/*
  Public domain by Andrew M. <liquidsun@gmail.com>
  See: https://github.com/floodyberry/curve25519-donna

  Curve25519 implementation agnostic helpers
*/

/*
 * In:  b =   2^5 - 2^0
 * Out: b = 2^250 - 2^0
 */
static void
curve25519_pow_two5mtwo0_two250mtwo0(bignum25519 b) {
  bignum25519 ALIGN(16) t0,c;

  /* 2^5  - 2^0 */ /* b */
  /* 2^10 - 2^5 */ curve25519_square_times(t0, b, 5);
  /* 2^10 - 2^0 */ curve25519_mul_noinline(b, t0, b);
  /* 2^20 - 2^10 */ curve25519_square_times(t0, b, 10);
  /* 2^20 - 2^0 */ curve25519_mul_noinline(c, t0, b);
  /* 2^40 - 2^20 */ curve25519_square_times(t0, c, 20);
  /* 2^40 - 2^0 */ curve25519_mul_noinline(t0, t0, c);
  /* 2^50 - 2^10 */ curve25519_square_times(t0, t0, 10);
  /* 2^50 - 2^0 */ curve25519_mul_noinline(b, t0, b);
  /* 2^100 - 2^50 */ curve25519_square_times(t0, b, 50);
  /* 2^100 - 2^0 */ curve25519_mul_noinline(c, t0, b);
  /* 2^200 - 2^100 */ curve25519_square_times(t0, c, 100);
  /* 2^200 - 2^0 */ curve25519_mul_noinline(t0, t0, c);
  /* 2^250 - 2^50 */ curve25519_square_times(t0, t0, 50);
  /* 2^250 - 2^0 */ curve25519_mul_noinline(b, t0, b);
}

/*
 * z^(p - 2) = z(2^255 - 21)
 */
static void
curve25519_recip(bignum25519 out, const bignum25519 z) {
  bignum25519 ALIGN(16) a,t0,b;

  /* 2 */ curve25519_square_times(a, z, 1); /* a = 2 */
  /* 8 */ curve25519_square_times(t0, a, 2);
  /* 9 */ curve25519_mul_noinline(b, t0, z); /* b = 9 */
  /* 11 */ curve25519_mul_noinline(a, b, a); /* a = 11 */
  /* 22 */ curve25519_square_times(t0, a, 1);
  /* 2^5 - 2^0 = 31 */ curve25519_mul_noinline(b, t0, b);
  /* 2^250 - 2^0 */ curve25519_pow_two5mtwo0_two250mtwo0(b);
  /* 2^255 - 2^5 */ curve25519_square_times(b, b, 5);
  /* 2^255 - 21 */ curve25519_mul_noinline(out, b, a);
}

/*
 * z^((p-5)/8) = z^(2^252 - 3)
 */
static void
curve25519_pow_two252m3(bignum25519 two252m3, const bignum25519 z) {
  bignum25519 ALIGN(16) b,c,t0;

  /* 2 */ curve25519_square_times(c, z, 1); /* c = 2 */
  /* 8 */ curve25519_square_times(t0, c, 2); /* t0 = 8 */
  /* 9 */ curve25519_mul_noinline(b, t0, z); /* b = 9 */
  /* 11 */ curve25519_mul_noinline(c, b, c); /* c = 11 */
  /* 22 */ curve25519_square_times(t0, c, 1);
  /* 2^5 - 2^0 = 31 */ curve25519_mul_noinline(b, t0, b);
  /* 2^250 - 2^0 */ curve25519_pow_two5mtwo0_two250mtwo0(b);
  /* 2^252 - 2^2 */ curve25519_square_times(b, b, 2);
  /* 2^252 - 3 */ curve25519_mul_noinline(two252m3, b, z);
}

/*
  Public domain by Andrew M. <liquidsun@gmail.com>
*/


/*
  Arithmetic modulo the group order n = 2^252 +  27742317777372353535851937790883648493 = 7237005577332262213973186563042994240857116359379907606001950938285454250989

  k = 32
  b = 1 << 8 = 256
  m = 2^252 + 27742317777372353535851937790883648493 = 0x1000000000000000000000000000000014def9dea2f79cd65812631a5cf5d3ed
  mu = floor( b^(k*2) / m ) = 0xfffffffffffffffffffffffffffffffeb2106215d086329a7ed9ce5a30a2c131b
*/

#define bignum256modm_bits_per_limb 30
#define bignum256modm_limb_size 9

typedef uint32_t bignum256modm_element_t;
typedef bignum256modm_element_t bignum256modm[9];

static const bignum256modm modm_m = {
  0x1cf5d3ed, 0x20498c69, 0x2f79cd65, 0x37be77a8,
  0x00000014, 0x00000000, 0x00000000, 0x00000000,
  0x00001000
};

static const bignum256modm modm_mu = {
  0x0a2c131b, 0x3673968c, 0x06329a7e, 0x01885742,
  0x3fffeb21, 0x3fffffff, 0x3fffffff, 0x3fffffff,
  0x000fffff
};

static bignum256modm_element_t
lt_modm(bignum256modm_element_t a, bignum256modm_element_t b) {
  return (a - b) >> 31;
}

/* see HAC, Alg. 14.42 Step 4 */
static void
reduce256_modm(bignum256modm r) {
  bignum256modm t;
  bignum256modm_element_t b = 0, pb, mask;

  /* t = r - m */
  pb = 0;
  pb += modm_m[0]; b = lt_modm(r[0], pb); t[0] = (r[0] - pb + (b << 30)); pb = b;
  pb += modm_m[1]; b = lt_modm(r[1], pb); t[1] = (r[1] - pb + (b << 30)); pb = b;
  pb += modm_m[2]; b = lt_modm(r[2], pb); t[2] = (r[2] - pb + (b << 30)); pb = b;
  pb += modm_m[3]; b = lt_modm(r[3], pb); t[3] = (r[3] - pb + (b << 30)); pb = b;
  pb += modm_m[4]; b = lt_modm(r[4], pb); t[4] = (r[4] - pb + (b << 30)); pb = b;
  pb += modm_m[5]; b = lt_modm(r[5], pb); t[5] = (r[5] - pb + (b << 30)); pb = b;
  pb += modm_m[6]; b = lt_modm(r[6], pb); t[6] = (r[6] - pb + (b << 30)); pb = b;
  pb += modm_m[7]; b = lt_modm(r[7], pb); t[7] = (r[7] - pb + (b << 30)); pb = b;
  pb += modm_m[8]; b = lt_modm(r[8], pb); t[8] = (r[8] - pb + (b << 16));

  /* keep r if r was smaller than m */
  mask = b - 1;
  r[0] ^= mask & (r[0] ^ t[0]);
  r[1] ^= mask & (r[1] ^ t[1]);
  r[2] ^= mask & (r[2] ^ t[2]);
  r[3] ^= mask & (r[3] ^ t[3]);
  r[4] ^= mask & (r[4] ^ t[4]);
  r[5] ^= mask & (r[5] ^ t[5]);
  r[6] ^= mask & (r[6] ^ t[6]);
  r[7] ^= mask & (r[7] ^ t[7]);
  r[8] ^= mask & (r[8] ^ t[8]);
}

/*
  Barrett reduction,  see HAC, Alg. 14.42

  Instead of passing in x, pre-process in to q1 and r1 for efficiency
*/
static void
barrett_reduce256_modm(bignum256modm r, const bignum256modm q1, const bignum256modm r1) {
  bignum256modm q3, r2;
  uint64_t c;
  bignum256modm_element_t f, b, pb;

  /* q1 = x >> 248 = 264 bits = 9 30 bit elements
     q2 = mu * q1
     q3 = (q2 / 256(32+1)) = q2 / (2^8)^(32+1) = q2 >> 264 */
  c  = mul32x32_64(modm_mu[0], q1[7]) + mul32x32_64(modm_mu[1], q1[6]) + mul32x32_64(modm_mu[2], q1[5]) + mul32x32_64(modm_mu[3], q1[4]) + mul32x32_64(modm_mu[4], q1[3]) + mul32x32_64(modm_mu[5], q1[2]) + mul32x32_64(modm_mu[6], q1[1]) + mul32x32_64(modm_mu[7], q1[0]);
  c >>= 30;
  c += mul32x32_64(modm_mu[0], q1[8]) + mul32x32_64(modm_mu[1], q1[7]) + mul32x32_64(modm_mu[2], q1[6]) + mul32x32_64(modm_mu[3], q1[5]) + mul32x32_64(modm_mu[4], q1[4]) + mul32x32_64(modm_mu[5], q1[3]) + mul32x32_64(modm_mu[6], q1[2]) + mul32x32_64(modm_mu[7], q1[1]) + mul32x32_64(modm_mu[8], q1[0]);
  f = (bignum256modm_element_t)c; q3[0] = (f >> 24) & 0x3f; c >>= 30;
  c += mul32x32_64(modm_mu[1], q1[8]) + mul32x32_64(modm_mu[2], q1[7]) + mul32x32_64(modm_mu[3], q1[6]) + mul32x32_64(modm_mu[4], q1[5]) + mul32x32_64(modm_mu[5], q1[4]) + mul32x32_64(modm_mu[6], q1[3]) + mul32x32_64(modm_mu[7], q1[2]) + mul32x32_64(modm_mu[8], q1[1]);
  f = (bignum256modm_element_t)c; q3[0] |= (f << 6) & 0x3fffffff; q3[1] = (f >> 24) & 0x3f; c >>= 30;
  c += mul32x32_64(modm_mu[2], q1[8]) + mul32x32_64(modm_mu[3], q1[7]) + mul32x32_64(modm_mu[4], q1[6]) + mul32x32_64(modm_mu[5], q1[5]) + mul32x32_64(modm_mu[6], q1[4]) + mul32x32_64(modm_mu[7], q1[3]) + mul32x32_64(modm_mu[8], q1[2]);
  f = (bignum256modm_element_t)c; q3[1] |= (f << 6) & 0x3fffffff; q3[2] = (f >> 24) & 0x3f; c >>= 30;
  c += mul32x32_64(modm_mu[3], q1[8]) + mul32x32_64(modm_mu[4], q1[7]) + mul32x32_64(modm_mu[5], q1[6]) + mul32x32_64(modm_mu[6], q1[5]) + mul32x32_64(modm_mu[7], q1[4]) + mul32x32_64(modm_mu[8], q1[3]);
  f = (bignum256modm_element_t)c; q3[2] |= (f << 6) & 0x3fffffff; q3[3] = (f >> 24) & 0x3f; c >>= 30;
  c += mul32x32_64(modm_mu[4], q1[8]) + mul32x32_64(modm_mu[5], q1[7]) + mul32x32_64(modm_mu[6], q1[6]) + mul32x32_64(modm_mu[7], q1[5]) + mul32x32_64(modm_mu[8], q1[4]);
  f = (bignum256modm_element_t)c; q3[3] |= (f << 6) & 0x3fffffff; q3[4] = (f >> 24) & 0x3f; c >>= 30;
  c += mul32x32_64(modm_mu[5], q1[8]) + mul32x32_64(modm_mu[6], q1[7]) + mul32x32_64(modm_mu[7], q1[6]) + mul32x32_64(modm_mu[8], q1[5]);
  f = (bignum256modm_element_t)c; q3[4] |= (f << 6) & 0x3fffffff; q3[5] = (f >> 24) & 0x3f; c >>= 30;
  c += mul32x32_64(modm_mu[6], q1[8]) + mul32x32_64(modm_mu[7], q1[7]) + mul32x32_64(modm_mu[8], q1[6]);
  f = (bignum256modm_element_t)c; q3[5] |= (f << 6) & 0x3fffffff; q3[6] = (f >> 24) & 0x3f; c >>= 30;
  c += mul32x32_64(modm_mu[7], q1[8]) + mul32x32_64(modm_mu[8], q1[7]);
  f = (bignum256modm_element_t)c; q3[6] |= (f << 6) & 0x3fffffff; q3[7] = (f >> 24) & 0x3f; c >>= 30;
  c += mul32x32_64(modm_mu[8], q1[8]);
  f = (bignum256modm_element_t)c; q3[7] |= (f << 6) & 0x3fffffff; q3[8] = (bignum256modm_element_t)(c >> 24);

  /* r1 = (x mod 256^(32+1)) = x mod (2^8)(31+1) = x & ((1 << 264) - 1)
     r2 = (q3 * m) mod (256^(32+1)) = (q3 * m) & ((1 << 264) - 1) */
  c = mul32x32_64(modm_m[0], q3[0]);
  r2[0] = (bignum256modm_element_t)(c & 0x3fffffff); c >>= 30;
  c += mul32x32_64(modm_m[0], q3[1]) + mul32x32_64(modm_m[1], q3[0]);
  r2[1] = (bignum256modm_element_t)(c & 0x3fffffff); c >>= 30;
  c += mul32x32_64(modm_m[0], q3[2]) + mul32x32_64(modm_m[1], q3[1]) + mul32x32_64(modm_m[2], q3[0]);
  r2[2] = (bignum256modm_element_t)(c & 0x3fffffff); c >>= 30;
  c += mul32x32_64(modm_m[0], q3[3]) + mul32x32_64(modm_m[1], q3[2]) + mul32x32_64(modm_m[2], q3[1]) + mul32x32_64(modm_m[3], q3[0]);
  r2[3] = (bignum256modm_element_t)(c & 0x3fffffff); c >>= 30;
  c += mul32x32_64(modm_m[0], q3[4]) + mul32x32_64(modm_m[1], q3[3]) + mul32x32_64(modm_m[2], q3[2]) + mul32x32_64(modm_m[3], q3[1]) + mul32x32_64(modm_m[4], q3[0]);
  r2[4] = (bignum256modm_element_t)(c & 0x3fffffff); c >>= 30;
  c += mul32x32_64(modm_m[0], q3[5]) + mul32x32_64(modm_m[1], q3[4]) + mul32x32_64(modm_m[2], q3[3]) + mul32x32_64(modm_m[3], q3[2]) + mul32x32_64(modm_m[4], q3[1]) + mul32x32_64(modm_m[5], q3[0]);
  r2[5] = (bignum256modm_element_t)(c & 0x3fffffff); c >>= 30;
  c += mul32x32_64(modm_m[0], q3[6]) + mul32x32_64(modm_m[1], q3[5]) + mul32x32_64(modm_m[2], q3[4]) + mul32x32_64(modm_m[3], q3[3]) + mul32x32_64(modm_m[4], q3[2]) + mul32x32_64(modm_m[5], q3[1]) + mul32x32_64(modm_m[6], q3[0]);
  r2[6] = (bignum256modm_element_t)(c & 0x3fffffff); c >>= 30;
  c += mul32x32_64(modm_m[0], q3[7]) + mul32x32_64(modm_m[1], q3[6]) + mul32x32_64(modm_m[2], q3[5]) + mul32x32_64(modm_m[3], q3[4]) + mul32x32_64(modm_m[4], q3[3]) + mul32x32_64(modm_m[5], q3[2]) + mul32x32_64(modm_m[6], q3[1]) + mul32x32_64(modm_m[7], q3[0]);
  r2[7] = (bignum256modm_element_t)(c & 0x3fffffff); c >>= 30;
  c += mul32x32_64(modm_m[0], q3[8]) + mul32x32_64(modm_m[1], q3[7]) + mul32x32_64(modm_m[2], q3[6]) + mul32x32_64(modm_m[3], q3[5]) + mul32x32_64(modm_m[4], q3[4]) + mul32x32_64(modm_m[5], q3[3]) + mul32x32_64(modm_m[6], q3[2]) + mul32x32_64(modm_m[7], q3[1]) + mul32x32_64(modm_m[8], q3[0]);
  r2[8] = (bignum256modm_element_t)(c & 0xffffff);

  /* r = r1 - r2
     if (r < 0) r += (1 << 264) */
  pb = 0;
  pb += r2[0]; b = lt_modm(r1[0], pb); r[0] = (r1[0] - pb + (b << 30)); pb = b;
  pb += r2[1]; b = lt_modm(r1[1], pb); r[1] = (r1[1] - pb + (b << 30)); pb = b;
  pb += r2[2]; b = lt_modm(r1[2], pb); r[2] = (r1[2] - pb + (b << 30)); pb = b;
  pb += r2[3]; b = lt_modm(r1[3], pb); r[3] = (r1[3] - pb + (b << 30)); pb = b;
  pb += r2[4]; b = lt_modm(r1[4], pb); r[4] = (r1[4] - pb + (b << 30)); pb = b;
  pb += r2[5]; b = lt_modm(r1[5], pb); r[5] = (r1[5] - pb + (b << 30)); pb = b;
  pb += r2[6]; b = lt_modm(r1[6], pb); r[6] = (r1[6] - pb + (b << 30)); pb = b;
  pb += r2[7]; b = lt_modm(r1[7], pb); r[7] = (r1[7] - pb + (b << 30)); pb = b;
  pb += r2[8]; b = lt_modm(r1[8], pb); r[8] = (r1[8] - pb + (b << 24));

  reduce256_modm(r);
  reduce256_modm(r);
}

static void
expand256_modm(bignum256modm out, const unsigned char *in, size_t len) {
  unsigned char work[64] = {0};
  bignum256modm_element_t x[16];
  bignum256modm q1;

  memcpy(work, in, len);
  x[0] = U8TO32_LE(work +  0);
  x[1] = U8TO32_LE(work +  4);
  x[2] = U8TO32_LE(work +  8);
  x[3] = U8TO32_LE(work + 12);
  x[4] = U8TO32_LE(work + 16);
  x[5] = U8TO32_LE(work + 20);
  x[6] = U8TO32_LE(work + 24);
  x[7] = U8TO32_LE(work + 28);
  x[8] = U8TO32_LE(work + 32);
  x[9] = U8TO32_LE(work + 36);
  x[10] = U8TO32_LE(work + 40);
  x[11] = U8TO32_LE(work + 44);
  x[12] = U8TO32_LE(work + 48);
  x[13] = U8TO32_LE(work + 52);
  x[14] = U8TO32_LE(work + 56);
  x[15] = U8TO32_LE(work + 60);

  /* r1 = (x mod 256^(32+1)) = x mod (2^8)(31+1) = x & ((1 << 264) - 1) */
  out[0] = (                         x[0]) & 0x3fffffff;
  out[1] = ((x[ 0] >> 30) | (x[ 1] <<  2)) & 0x3fffffff;
  out[2] = ((x[ 1] >> 28) | (x[ 2] <<  4)) & 0x3fffffff;
  out[3] = ((x[ 2] >> 26) | (x[ 3] <<  6)) & 0x3fffffff;
  out[4] = ((x[ 3] >> 24) | (x[ 4] <<  8)) & 0x3fffffff;
  out[5] = ((x[ 4] >> 22) | (x[ 5] << 10)) & 0x3fffffff;
  out[6] = ((x[ 5] >> 20) | (x[ 6] << 12)) & 0x3fffffff;
  out[7] = ((x[ 6] >> 18) | (x[ 7] << 14)) & 0x3fffffff;
  out[8] = ((x[ 7] >> 16) | (x[ 8] << 16)) & 0x00ffffff;

  /* 8*31 = 248 bits, no need to reduce */
  if (len < 32)
    return;

  /* q1 = x >> 248 = 264 bits = 9 30 bit elements */
  q1[0] = ((x[ 7] >> 24) | (x[ 8] <<  8)) & 0x3fffffff;
  q1[1] = ((x[ 8] >> 22) | (x[ 9] << 10)) & 0x3fffffff;
  q1[2] = ((x[ 9] >> 20) | (x[10] << 12)) & 0x3fffffff;
  q1[3] = ((x[10] >> 18) | (x[11] << 14)) & 0x3fffffff;
  q1[4] = ((x[11] >> 16) | (x[12] << 16)) & 0x3fffffff;
  q1[5] = ((x[12] >> 14) | (x[13] << 18)) & 0x3fffffff;
  q1[6] = ((x[13] >> 12) | (x[14] << 20)) & 0x3fffffff;
  q1[7] = ((x[14] >> 10) | (x[15] << 22)) & 0x3fffffff;
  q1[8] = ((x[15] >>  8)                );

  barrett_reduce256_modm(out, q1, out);
}

static void
contract256_slidingwindow_modm(signed char r[256], const bignum256modm s, int windowsize) {
  int i,j,k,b;
  int m = (1 << (windowsize - 1)) - 1, soplen = 256;
  signed char *bits = r;
  bignum256modm_element_t v;

  /* first put the binary expansion into r  */
  for (i = 0; i < 8; i++) {
    v = s[i];
    for (j = 0; j < 30; j++, v >>= 1)
      *bits++ = (v & 1);
  }
  v = s[8];
  for (j = 0; j < 16; j++, v >>= 1)
    *bits++ = (v & 1);

  /* Making it sliding window */
  for (j = 0; j < soplen; j++) {
    if (!r[j])
      continue;

    for (b = 1; (b < (soplen - j)) && (b <= 6); b++) {
      if ((r[j] + (r[j + b] << b)) <= m) {
        r[j] += r[j + b] << b;
        r[j + b] = 0;
      } else if ((r[j] - (r[j + b] << b)) >= -m) {
        r[j] -= r[j + b] << b;
        for (k = j + b; k < soplen; k++) {
          if (!r[k]) {
            r[k] = 1;
            break;
          }
          r[k] = 0;
        }
      } else if (r[j + b]) {
        break;
      }
    }
  }
}


typedef unsigned char hash_512bits[64];

/*
  Timing safe memory compare
*/
static int
ed25519_verify(const unsigned char *x, const unsigned char *y, size_t len) {
  size_t differentbits = 0;
  while (len--)
    differentbits |= (*x++ ^ *y++);
  return (int) (1 & ((differentbits - 1) >> 8));
}


/*
 * Arithmetic on the twisted Edwards curve -x^2 + y^2 = 1 + dx^2y^2
 * with d = -(121665/121666) = 37095705934669439343138083508754565189542113879843219016388785533085940283555
 * Base point: (15112221349535400772501151409588531511454012693041857206046113283949847762202,46316835694926478169428394003475163141307993866256225615783033603165251855960);
 */

typedef struct ge25519_t {
  bignum25519 x, y, z, t;
} ge25519;

typedef struct ge25519_p1p1_t {
  bignum25519 x, y, z, t;
} ge25519_p1p1;

typedef struct ge25519_niels_t {
  bignum25519 ysubx, xaddy, t2d;
} ge25519_niels;

typedef struct ge25519_pniels_t {
  bignum25519 ysubx, xaddy, z, t2d;
} ge25519_pniels;


/*
  d
*/

static const bignum25519 ALIGN(16) ge25519_ecd = {
  0x035978a3,0x00d37284,0x03156ebd,0x006a0a0e,0x0001c029,0x0179e898,0x03a03cbb,0x01ce7198,0x02e2b6ff,0x01480db3
};

static const bignum25519 ALIGN(16) ge25519_ec2d = {
  0x02b2f159,0x01a6e509,0x022add7a,0x00d4141d,0x00038052,0x00f3d130,0x03407977,0x019ce331,0x01c56dff,0x00901b67
};

/*
  sqrt(-1)
*/

static const bignum25519 ALIGN(16) ge25519_sqrtneg1 = {
  0x020ea0b0,0x0186c9d2,0x008f189d,0x0035697f,0x00bd0c60,0x01fbd7a7,0x02804c9e,0x01e16569,0x0004fc1d,0x00ae0c92
};

static const ge25519_niels ALIGN(16) ge25519_niels_sliding_multiples[32] = {
  {{0x0340913e,0x000e4175,0x03d673a2,0x002e8a05,0x03f4e67c,0x008f8a09,0x00c21a34,0x004cf4b8,0x01298f81,0x0113f4be},{0x018c3b85,0x0124f1bd,0x01c325f7,0x0037dc60,0x033e4cb7,0x003d42c2,0x01a44c32,0x014ca4e1,0x03a33d4b,0x001f3e74},{0x037aaa68,0x00448161,0x0093d579,0x011e6556,0x009b67a0,0x0143598c,0x01bee5ee,0x00b50b43,0x0289f0c6,0x01bc45ed}},
  {{0x00fcd265,0x0047fa29,0x034faacc,0x01ef2e0d,0x00ef4d4f,0x014bd6bd,0x00f98d10,0x014c5026,0x007555bd,0x00aae456},{0x00ee9730,0x016c2a13,0x017155e4,0x01874432,0x00096a10,0x01016732,0x01a8014f,0x011e9823,0x01b9a80f,0x01e85938},{0x01d0d889,0x01a4cfc3,0x034c4295,0x0110e1ae,0x0162508c,0x00f2db4c,0x0072a2c6,0x0098da2e,0x02f12b9b,0x0168a09a}},
  {{0x0047d6ba,0x0060b0e9,0x0136eff2,0x008a5939,0x03540053,0x0064a087,0x02788e5c,0x00be7c67,0x033eb1b5,0x005529f9},{0x00a5bb33,0x00af1102,0x01a05442,0x001e3af7,0x02354123,0x00bfec44,0x01f5862d,0x00dd7ba3,0x03146e20,0x00a51733},{0x012a8285,0x00f6fc60,0x023f9797,0x003e85ee,0x009c3820,0x01bda72d,0x01b3858d,0x00d35683,0x0296b3bb,0x010eaaf9}},
  {{0x023221b1,0x01cb26aa,0x0074f74d,0x0099ddd1,0x01b28085,0x00192c3a,0x013b27c9,0x00fc13bd,0x01d2e531,0x0075bb75},{0x004ea3bf,0x00973425,0x001a4d63,0x01d59cee,0x01d1c0d4,0x00542e49,0x01294114,0x004fce36,0x029283c9,0x01186fa9},{0x01b8b3a2,0x00db7200,0x00935e30,0x003829f5,0x02cc0d7d,0x0077adf3,0x0220dd2c,0x0014ea53,0x01c6a0f9,0x01ea7eec}},
  {{0x039d8064,0x01885f80,0x00337e6d,0x01b7a902,0x02628206,0x015eb044,0x01e30473,0x0191f2d9,0x011fadc9,0x01270169},{0x02a8632f,0x0199e2a9,0x00d8b365,0x017a8de2,0x02994279,0x0086f5b5,0x0119e4e3,0x01eb39d6,0x0338add7,0x00d2e7b4},{0x0045af1b,0x013a2fe4,0x0245e0d6,0x014538ce,0x038bfe0f,0x01d4cf16,0x037e14c9,0x0160d55e,0x0021b008,0x01cf05c8}},
  {{0x01864348,0x01d6c092,0x0070262b,0x014bb844,0x00fb5acd,0x008deb95,0x003aaab5,0x00eff474,0x00029d5c,0x0062ad66},{0x02802ade,0x01c02122,0x01c4e5f7,0x00781181,0x039767fb,0x01703406,0x0342388b,0x01f5e227,0x022546d8,0x0109d6ab},{0x016089e9,0x00cb317f,0x00949b05,0x01099417,0x000c7ad2,0x011a8622,0x0088ccda,0x01290886,0x022b53df,0x00f71954}},
  {{0x027fbf93,0x01c04ecc,0x01ed6a0d,0x004cdbbb,0x02bbf3af,0x00ad5968,0x01591955,0x0094f3a2,0x02d17602,0x00099e20},{0x02007f6d,0x003088a8,0x03db77ee,0x00d5ade6,0x02fe12ce,0x0107ba07,0x0107097d,0x00482a6f,0x02ec346f,0x008d3f5f},{0x032ea378,0x0028465c,0x028e2a6c,0x018efc6e,0x0090df9a,0x01a7e533,0x039bfc48,0x010c745d,0x03daa097,0x0125ee9b}},
  {{0x028ccf0b,0x00f36191,0x021ac081,0x012154c8,0x034e0a6e,0x01b25192,0x00180403,0x01d7eea1,0x00218d05,0x010ed735},{0x03cfeaa0,0x01b300c4,0x008da499,0x0068c4e1,0x0219230a,0x01f2d4d0,0x02defd60,0x00e565b7,0x017f12de,0x018788a4},{0x03d0b516,0x009d8be6,0x03ddcbb3,0x0071b9fe,0x03ace2bd,0x01d64270,0x032d3ec9,0x01084065,0x0210ae4d,0x01447584}},
  {{0x0020de87,0x00e19211,0x01b68102,0x00b5ac97,0x022873c0,0x01942d25,0x01271394,0x0102073f,0x02fe2482,0x01c69ff9},{0x010e9d81,0x019dbbe5,0x0089f258,0x006e06b8,0x02951883,0x018f1248,0x019b3237,0x00bc7553,0x024ddb85,0x01b4c964},{0x01c8c854,0x0060ae29,0x01406d8e,0x01cff2f9,0x00cff451,0x01778d0c,0x03ac8c41,0x01552e59,0x036559ee,0x011d1b12}},
  {{0x00741147,0x0151b219,0x01092690,0x00e877e6,0x01f4d6bb,0x0072a332,0x01cd3b03,0x00dadff2,0x0097db5e,0x0086598d},{0x01c69a2b,0x01decf1b,0x02c2fa6e,0x013b7c4f,0x037beac8,0x013a16b5,0x028e7bda,0x01f6e8ac,0x01e34fe9,0x01726947},{0x01f10e67,0x003c73de,0x022b7ea2,0x010f32c2,0x03ff776a,0x00142277,0x01d38b88,0x00776138,0x03c60822,0x01201140}},
  {{0x0236d175,0x0008748e,0x03c6476d,0x013f4cdc,0x02eed02a,0x00838a47,0x032e7210,0x018bcbb3,0x00858de4,0x01dc7826},{0x00a37fc7,0x0127b40b,0x01957884,0x011d30ad,0x02816683,0x016e0e23,0x00b76be4,0x012db115,0x02516506,0x0154ce62},{0x00451edf,0x00bd749e,0x03997342,0x01cc2c4c,0x00eb6975,0x01a59508,0x03a516cf,0x00c228ef,0x0168ff5a,0x01697b47}},
  {{0x00527359,0x01783156,0x03afd75c,0x00ce56dc,0x00e4b970,0x001cabe9,0x029e0f6d,0x0188850c,0x0135fefd,0x00066d80},{0x02150e83,0x01448abf,0x02bb0232,0x012bf259,0x033c8268,0x00711e20,0x03fc148f,0x005e0e70,0x017d8bf9,0x0112b2e2},{0x02134b83,0x001a0517,0x0182c3cc,0x00792182,0x0313d799,0x001a3ed7,0x0344547e,0x01f24a0d,0x03de6ad2,0x00543127}},
  {{0x00dca868,0x00618f27,0x015a1709,0x00ddc38a,0x0320fd13,0x0036168d,0x0371ab06,0x01783fc7,0x0391e05f,0x01e29b5d},{0x01471138,0x00fca542,0x00ca31cf,0x01ca7bad,0x0175bfbc,0x01a708ad,0x03bce212,0x01244215,0x0075bb99,0x01acad68},{0x03a0b976,0x01dc12d1,0x011aab17,0x00aba0ba,0x029806cd,0x0142f590,0x018fd8ea,0x01a01545,0x03c4ad55,0x01c971ff}},
  {{0x00d098c0,0x000afdc7,0x006cd230,0x01276af3,0x03f905b2,0x0102994c,0x002eb8a4,0x015cfbeb,0x025f855f,0x01335518},{0x01cf99b2,0x0099c574,0x01a69c88,0x00881510,0x01cd4b54,0x0112109f,0x008abdc5,0x0074647a,0x0277cb1f,0x01e53324},{0x02ac5053,0x01b109b0,0x024b095e,0x016997b3,0x02f26bb6,0x00311021,0x00197885,0x01d0a55a,0x03b6fcc8,0x01c020d5}},
  {{0x02584a34,0x00e7eee0,0x03257a03,0x011e95a3,0x011ead91,0x00536202,0x00b1ce24,0x008516c6,0x03669d6d,0x004ea4a8},{0x00773f01,0x0019c9ce,0x019f6171,0x01d4afde,0x02e33323,0x01ad29b6,0x02ead1dc,0x01ed51a5,0x01851ad0,0x001bbdfa},{0x00577de5,0x00ddc730,0x038b9952,0x00f281ae,0x01d50390,0x0002e071,0x000780ec,0x010d448d,0x01f8a2af,0x00f0a5b7}},
  {{0x031f2541,0x00d34bae,0x0323ff9d,0x003a056d,0x02e25443,0x00a1ad05,0x00d1bee8,0x002f7f8e,0x03007477,0x002a24b1},{0x0114a713,0x01457e76,0x032255d5,0x01cc647f,0x02a4bdef,0x0153d730,0x00118bcf,0x00f755ff,0x013490c7,0x01ea674e},{0x02bda3e8,0x00bb490d,0x00f291ea,0x000abf40,0x01dea321,0x002f9ce0,0x00b2b193,0x00fa54b5,0x0128302f,0x00a19d8b}},
  {{0x022ef5bd,0x01638af3,0x038c6f8a,0x01a33a3d,0x039261b2,0x01bb89b8,0x010bcf9d,0x00cf42a9,0x023d6f17,0x01da1bca},{0x00e35b25,0x000d824f,0x0152e9cf,0x00ed935d,0x020b8460,0x01c7b83f,0x00c969e5,0x01a74198,0x0046a9d9,0x00cbc768},{0x01597c6a,0x0144a99b,0x00a57551,0x0018269c,0x023c464c,0x0009b022,0x00ee39e1,0x0114c7f2,0x038a9ad2,0x01584c17}},
  {{0x03b0c0d5,0x00b30a39,0x038a6ce4,0x01ded83a,0x01c277a6,0x01010a61,0x0346d3eb,0x018d995e,0x02f2c57c,0x000c286b},{0x0092aed1,0x0125e37b,0x027ca201,0x001a6b6b,0x03290f55,0x0047ba48,0x018d916c,0x01a59062,0x013e35d4,0x0002abb1},{0x003ad2aa,0x007ddcc0,0x00c10f76,0x0001590b,0x002cfca6,0x000ed23e,0x00ee4329,0x00900f04,0x01c24065,0x0082fa70}},
  {{0x02025e60,0x003912b8,0x0327041c,0x017e5ee5,0x02c0ecec,0x015a0d1c,0x02b1ce7c,0x0062220b,0x0145067e,0x01a5d931},{0x009673a6,0x00e1f609,0x00927c2a,0x016faa37,0x01650ef0,0x016f63b5,0x03cd40e1,0x003bc38f,0x0361f0ac,0x01d42acc},{0x02f81037,0x008ca0e8,0x017e23d1,0x011debfe,0x01bcbb68,0x002e2563,0x03e8add6,0x000816e5,0x03fb7075,0x0153e5ac}},
  {{0x02b11ecd,0x016bf185,0x008f22ef,0x00e7d2bb,0x0225d92e,0x00ece785,0x00508873,0x017e16f5,0x01fbe85d,0x01e39a0e},{0x01669279,0x017c810a,0x024941f5,0x0023ebeb,0x00eb7688,0x005760f1,0x02ca4146,0x0073cde7,0x0052bb75,0x00f5ffa7},{0x03b8856b,0x00cb7dcd,0x02f14e06,0x001820d0,0x01d74175,0x00e59e22,0x03fba550,0x00484641,0x03350088,0x01c3c9a3}},
  {{0x00dcf355,0x0104481c,0x0022e464,0x01f73fe7,0x00e03325,0x0152b698,0x02ef769a,0x00973663,0x00039b8c,0x0101395b},{0x01805f47,0x019160ec,0x03832cd0,0x008b06eb,0x03d4d717,0x004cb006,0x03a75b8f,0x013b3d30,0x01cfad88,0x01f034d1},{0x0078338a,0x01c7d2e3,0x02bc2b23,0x018b3f05,0x0280d9aa,0x005f3d44,0x0220a95a,0x00eeeb97,0x0362aaec,0x00835d51}},
  {{0x01b9f543,0x013fac4d,0x02ad93ae,0x018ef464,0x0212cdf7,0x01138ba9,0x011583ab,0x019c3d26,0x028790b4,0x00e2e2b6},{0x033bb758,0x01f0dbf1,0x03734bd1,0x0129b1e5,0x02b3950e,0x003bc922,0x01a53ec8,0x018c5532,0x006f3cee,0x00ae3c79},{0x0351f95d,0x0012a737,0x03d596b8,0x017658fe,0x00ace54a,0x008b66da,0x0036c599,0x012a63a2,0x032ceba1,0x00126bac}},
  {{0x03dcfe7e,0x019f4f18,0x01c81aee,0x0044bc2b,0x00827165,0x014f7c13,0x03b430f0,0x00bf96cc,0x020c8d62,0x01471997},{0x01fc7931,0x001f42dd,0x00ba754a,0x005bd339,0x003fbe49,0x016b3930,0x012a159c,0x009f83b0,0x03530f67,0x01e57b85},{0x02ecbd81,0x0096c294,0x01fce4a9,0x017701a5,0x0175047d,0x00ee4a31,0x012686e5,0x008efcd4,0x0349dc54,0x01b3466f}},
  {{0x02179ca3,0x01d86414,0x03f0afd0,0x00305964,0x015c7428,0x0099711e,0x015d5442,0x00c71014,0x01b40b2e,0x01d483cf},{0x01afc386,0x01984859,0x036203ff,0x0045c6a8,0x0020a8aa,0x00990baa,0x03313f10,0x007ceede,0x027429e4,0x017806ce},{0x039357a1,0x0142f8f4,0x0294a7b6,0x00eaccf4,0x0259edb3,0x01311e6e,0x004d326f,0x0130c346,0x01ccef3c,0x01c424b2}},
  {{0x0364918c,0x00148fc0,0x01638a7b,0x01a1fd5b,0x028ad013,0x0081e5a4,0x01a54f33,0x0174e101,0x003d0257,0x003a856c},{0x00051dcf,0x00f62b1d,0x0143d0ad,0x0042adbd,0x000fda90,0x01743ceb,0x0173e5e4,0x017bc749,0x03b7137a,0x0105ce96},{0x00f9218a,0x015b8c7c,0x00e102f8,0x0158d7e2,0x0169a5b8,0x00b2f176,0x018b347a,0x014cfef2,0x0214a4e3,0x017f1595}},
  {{0x006d7ae5,0x0195c371,0x0391e26d,0x0062a7c6,0x003f42ab,0x010dad86,0x024f8198,0x01542b2a,0x0014c454,0x0189c471},{0x0390988e,0x00b8799d,0x02e44912,0x0078e2e6,0x00075654,0x01923eed,0x0040cd72,0x00a37c76,0x0009d466,0x00c8531d},{0x02651770,0x00609d01,0x0286c265,0x0134513c,0x00ee9281,0x005d223c,0x035c760c,0x00679b36,0x0073ecb8,0x016faa50}},
  {{0x02c89be4,0x016fc244,0x02f38c83,0x018beb72,0x02b3ce2c,0x0097b065,0x034f017b,0x01dd957f,0x00148f61,0x00eab357},{0x0343d2f8,0x003398fc,0x011e368e,0x00782a1f,0x00019eea,0x00117b6f,0x0128d0d1,0x01a5e6bb,0x01944f1b,0x012b41e1},{0x03318301,0x018ecd30,0x0104d0b1,0x0038398b,0x03726701,0x019da88c,0x002d9769,0x00a7a681,0x031d9028,0x00ebfc32}},
  {{0x0220405e,0x0171face,0x02d930f8,0x017f6d6a,0x023b8c47,0x0129d5f9,0x02972456,0x00a3a524,0x006f4cd2,0x004439fa},{0x00c53505,0x0190c2fd,0x00507244,0x009930f9,0x01a39270,0x01d327c6,0x0399bc47,0x01cfe13d,0x0332bd99,0x00b33e7d},{0x0203f5e4,0x003627b5,0x00018af8,0x01478581,0x004a2218,0x002e3bb7,0x039384d0,0x0146ea62,0x020b9693,0x0017155f}},
  {{0x03c97e6f,0x00738c47,0x03b5db1f,0x01808fcf,0x01e8fc98,0x01ed25dd,0x01bf5045,0x00eb5c2b,0x0178fe98,0x01b85530},{0x01c20eb0,0x01aeec22,0x030b9eee,0x01b7d07e,0x0187e16f,0x014421fb,0x009fa731,0x0040b6d7,0x00841861,0x00a27fbc},{0x02d69abf,0x0058cdbf,0x0129f9ec,0x013c19ae,0x026c5b93,0x013a7fe7,0x004bb2ba,0x0063226f,0x002a95ca,0x01abefd9}},
  {{0x02f5d2c1,0x00378318,0x03734fb5,0x01258073,0x0263f0f6,0x01ad70e0,0x01b56d06,0x01188fbd,0x011b9503,0x0036d2e1},{0x0113a8cc,0x01541c3e,0x02ac2bbc,0x01d95867,0x01f47459,0x00ead489,0x00ab5b48,0x01db3b45,0x00edb801,0x004b024f},{0x00b8190f,0x011fe4c2,0x00621f82,0x010508d7,0x001a5a76,0x00c7d7fd,0x03aab96d,0x019cd9dc,0x019c6635,0x00ceaa1e}},
  {{0x01085cf2,0x01fd47af,0x03e3f5e1,0x004b3e99,0x01e3d46a,0x0060033c,0x015ff0a8,0x0150cdd8,0x029e8e21,0x008cf1bc},{0x00156cb1,0x003d623f,0x01a4f069,0x00d8d053,0x01b68aea,0x01ca5ab6,0x0316ae43,0x0134dc44,0x001c8d58,0x0084b343},{0x0318c781,0x0135441f,0x03a51a5e,0x019293f4,0x0048bb37,0x013d3341,0x0143151e,0x019c74e1,0x00911914,0x0076ddde}},
  {{0x006bc26f,0x00d48e5f,0x00227bbe,0x00629ea8,0x01ea5f8b,0x0179a330,0x027a1d5f,0x01bf8f8e,0x02d26e2a,0x00c6b65e},{0x01701ab6,0x0051da77,0x01b4b667,0x00a0ce7c,0x038ae37b,0x012ac852,0x03a0b0fe,0x0097c2bb,0x00a017d2,0x01eb8b2a},{0x0120b962,0x0005fb42,0x0353b6fd,0x0061f8ce,0x007a1463,0x01560a64,0x00e0a792,0x01907c92,0x013a6622,0x007b47f1}}
};


/*
  conversions
*/

DONNA_INLINE static void
ge25519_p1p1_to_partial(ge25519 *r, const ge25519_p1p1 *p) {
  curve25519_mul(r->x, p->x, p->t);
  curve25519_mul(r->y, p->y, p->z);
  curve25519_mul(r->z, p->z, p->t);
}

DONNA_INLINE static void
ge25519_p1p1_to_full(ge25519 *r, const ge25519_p1p1 *p) {
  curve25519_mul(r->x, p->x, p->t);
  curve25519_mul(r->y, p->y, p->z);
  curve25519_mul(r->z, p->z, p->t);
  curve25519_mul(r->t, p->x, p->y);
}

static void
ge25519_full_to_pniels(ge25519_pniels *p, const ge25519 *r) {
  curve25519_sub(p->ysubx, r->y, r->x);
  curve25519_add(p->xaddy, r->y, r->x);
  curve25519_copy(p->z, r->z);
  curve25519_mul(p->t2d, r->t, ge25519_ec2d);
}

/*
  adding & doubling
*/

static void
ge25519_double_p1p1(ge25519_p1p1 *r, const ge25519 *p) {
  bignum25519 a,b,c;

  curve25519_square(a, p->x);
  curve25519_square(b, p->y);
  curve25519_square(c, p->z);
  curve25519_add_reduce(c, c, c);
  curve25519_add(r->x, p->x, p->y);
  curve25519_square(r->x, r->x);
  curve25519_add(r->y, b, a);
  curve25519_sub(r->z, b, a);
  curve25519_sub_after_basic(r->x, r->x, r->y);
  curve25519_sub_after_basic(r->t, c, r->z);
}

static void
ge25519_nielsadd2_p1p1(ge25519_p1p1 *r, const ge25519 *p, const ge25519_niels *q, unsigned char signbit) {
  const bignum25519 *qb = (const bignum25519 *)q;
  bignum25519 *rb = (bignum25519 *)r;
  bignum25519 a,b,c;

  curve25519_sub(a, p->y, p->x);
  curve25519_add(b, p->y, p->x);
  curve25519_mul(a, a, qb[signbit]); /* x for +, y for - */
  curve25519_mul(r->x, b, qb[signbit^1]); /* y for +, x for - */
  curve25519_add(r->y, r->x, a);
  curve25519_sub(r->x, r->x, a);
  curve25519_mul(c, p->t, q->t2d);
  curve25519_add_reduce(r->t, p->z, p->z);
  curve25519_copy(r->z, r->t);
  curve25519_add(rb[2+signbit], rb[2+signbit], c); /* z for +, t for - */
  curve25519_sub(rb[2+(signbit^1)], rb[2+(signbit^1)], c); /* t for +, z for - */
}

static void
ge25519_pnielsadd_p1p1(ge25519_p1p1 *r, const ge25519 *p, const ge25519_pniels *q, unsigned char signbit) {
  const bignum25519 *qb = (const bignum25519 *)q;
  bignum25519 *rb = (bignum25519 *)r;
  bignum25519 a,b,c;

  curve25519_sub(a, p->y, p->x);
  curve25519_add(b, p->y, p->x);
  curve25519_mul(a, a, qb[signbit]); /* ysubx for +, xaddy for - */
  curve25519_mul(r->x, b, qb[signbit^1]); /* xaddy for +, ysubx for - */
  curve25519_add(r->y, r->x, a);
  curve25519_sub(r->x, r->x, a);
  curve25519_mul(c, p->t, q->t2d);
  curve25519_mul(r->t, p->z, q->z);
  curve25519_add_reduce(r->t, r->t, r->t);
  curve25519_copy(r->z, r->t);
  curve25519_add(rb[2+signbit], rb[2+signbit], c); /* z for +, t for - */
  curve25519_sub(rb[2+(signbit^1)], rb[2+(signbit^1)], c); /* t for +, z for - */
}

static void
ge25519_double(ge25519 *r, const ge25519 *p) {
  ge25519_p1p1 t;
  ge25519_double_p1p1(&t, p);
  ge25519_p1p1_to_full(r, &t);
}

static void
ge25519_pnielsadd(ge25519_pniels *r, const ge25519 *p, const ge25519_pniels *q) {
  bignum25519 a,b,c,x,y,z,t;

  curve25519_sub(a, p->y, p->x);
  curve25519_add(b, p->y, p->x);
  curve25519_mul(a, a, q->ysubx);
  curve25519_mul(x, b, q->xaddy);
  curve25519_add(y, x, a);
  curve25519_sub(x, x, a);
  curve25519_mul(c, p->t, q->t2d);
  curve25519_mul(t, p->z, q->z);
  curve25519_add(t, t, t);
  curve25519_add_after_basic(z, t, c);
  curve25519_sub_after_basic(t, t, c);
  curve25519_mul(r->xaddy, x, t);
  curve25519_mul(r->ysubx, y, z);
  curve25519_mul(r->z, z, t);
  curve25519_mul(r->t2d, x, y);
  curve25519_copy(y, r->ysubx);
  curve25519_sub(r->ysubx, r->ysubx, r->xaddy);
  curve25519_add(r->xaddy, r->xaddy, y);
  curve25519_mul(r->t2d, r->t2d, ge25519_ec2d);
}


/*
  pack & unpack
*/

static void
ge25519_pack(unsigned char r[32], const ge25519 *p) {
  bignum25519 tx, ty, zi;
  unsigned char parity[32];
  curve25519_recip(zi, p->z);
  curve25519_mul(tx, p->x, zi);
  curve25519_mul(ty, p->y, zi);
  curve25519_contract(r, ty);
  curve25519_contract(parity, tx);
  r[31] ^= ((parity[0] & 1) << 7);
}

static int
ge25519_unpack_negative_vartime(ge25519 *r, const unsigned char p[32]) {
  static const unsigned char zero[32] = {0};
  static const bignum25519 one = {1};
  unsigned char parity = p[31] >> 7;
  unsigned char check[32];
  bignum25519 t, root, num, den, d3;

  curve25519_expand(r->y, p);
  curve25519_copy(r->z, one);
  curve25519_square(num, r->y); /* x = y^2 */
  curve25519_mul(den, num, ge25519_ecd); /* den = dy^2 */
  curve25519_sub_reduce(num, num, r->z); /* x = y^1 - 1 */
  curve25519_add(den, den, r->z); /* den = dy^2 + 1 */

  /* Computation of sqrt(num/den) */
  /* 1.: computation of num^((p-5)/8)*den^((7p-35)/8) = (num*den^7)^((p-5)/8) */
  curve25519_square(t, den);
  curve25519_mul(d3, t, den);
  curve25519_square(r->x, d3);
  curve25519_mul(r->x, r->x, den);
  curve25519_mul(r->x, r->x, num);
  curve25519_pow_two252m3(r->x, r->x);

  /* 2. computation of r->x = num * den^3 * (num*den^7)^((p-5)/8) */
  curve25519_mul(r->x, r->x, d3);
  curve25519_mul(r->x, r->x, num);

  /* 3. Check if either of the roots works: */
  curve25519_square(t, r->x);
  curve25519_mul(t, t, den);
  curve25519_sub_reduce(root, t, num);
  curve25519_contract(check, root);
  if (!ed25519_verify(check, zero, 32)) {
    curve25519_add_reduce(t, t, num);
    curve25519_contract(check, t);
    if (!ed25519_verify(check, zero, 32))
      return 0;
    curve25519_mul(r->x, r->x, ge25519_sqrtneg1);
  }

  curve25519_contract(check, r->x);
  if ((check[0] & 1) == parity) {
    curve25519_copy(t, r->x);
    curve25519_neg(r->x, t);
  }
  curve25519_mul(r->t, r->x, r->y);
  return 1;
}


/*
  scalarmults
*/

#define S1_SWINDOWSIZE 5
#define S1_TABLE_SIZE (1<<(S1_SWINDOWSIZE-2))
#define S2_SWINDOWSIZE 7
#define S2_TABLE_SIZE (1<<(S2_SWINDOWSIZE-2))

/* computes [s1]p1 + [s2]basepoint */
static void
ge25519_double_scalarmult_vartime(ge25519 *r, const ge25519 *p1, const bignum256modm s1, const bignum256modm s2) {
  signed char slide1[256], slide2[256];
  ge25519_pniels pre1[S1_TABLE_SIZE];
  ge25519 d1;
  ge25519_p1p1 t;
  int32_t i;

  contract256_slidingwindow_modm(slide1, s1, S1_SWINDOWSIZE);
  contract256_slidingwindow_modm(slide2, s2, S2_SWINDOWSIZE);

  ge25519_double(&d1, p1);
  ge25519_full_to_pniels(pre1, p1);
  for (i = 0; i < S1_TABLE_SIZE - 1; i++)
    ge25519_pnielsadd(&pre1[i+1], &d1, &pre1[i]);

  /* set neutral */
  memset(r, 0, sizeof(ge25519));
  r->y[0] = 1;
  r->z[0] = 1;

  i = 255;
  while ((i >= 0) && !(slide1[i] | slide2[i]))
    i--;

  for (; i >= 0; i--) {
    ge25519_double_p1p1(&t, r);

    if (slide1[i]) {
      ge25519_p1p1_to_full(r, &t);
      ge25519_pnielsadd_p1p1(&t, r, &pre1[abs(slide1[i]) / 2], (unsigned char)slide1[i] >> 7);
    }

    if (slide2[i]) {
      ge25519_p1p1_to_full(r, &t);
      ge25519_nielsadd2_p1p1(&t, r, &ge25519_niels_sliding_multiples[abs(slide2[i]) / 2], (unsigned char)slide2[i] >> 7);
    }

    ge25519_p1p1_to_partial(r, &t);
  }
}


/* reference/slow SHA-512. really, do not use this */
#if 1
//#define SHA512_HASH_BLOCK_SIZE  (128)
#define HASH_DIGEST_SIZE (64)

/*
typedef struct ed25519_sha512_state_t {
  uint64_t H[8];
  uint64_t T[2];
  uint32_t leftover;
  uint8_t buffer[SHA512_HASH_BLOCK_SIZE];
} ed25519_sha512_state;
*/

typedef struct ed25519_sha512_state_t ed25519_hash_context;

static const uint64_t sha512_constants[80] = {
  0x428a2f98d728ae22ull, 0x7137449123ef65cdull, 0xb5c0fbcfec4d3b2full, 0xe9b5dba58189dbbcull,
  0x3956c25bf348b538ull, 0x59f111f1b605d019ull, 0x923f82a4af194f9bull, 0xab1c5ed5da6d8118ull,
  0xd807aa98a3030242ull, 0x12835b0145706fbeull, 0x243185be4ee4b28cull, 0x550c7dc3d5ffb4e2ull,
  0x72be5d74f27b896full, 0x80deb1fe3b1696b1ull, 0x9bdc06a725c71235ull, 0xc19bf174cf692694ull,
  0xe49b69c19ef14ad2ull, 0xefbe4786384f25e3ull, 0x0fc19dc68b8cd5b5ull, 0x240ca1cc77ac9c65ull,
  0x2de92c6f592b0275ull, 0x4a7484aa6ea6e483ull, 0x5cb0a9dcbd41fbd4ull, 0x76f988da831153b5ull,
  0x983e5152ee66dfabull, 0xa831c66d2db43210ull, 0xb00327c898fb213full, 0xbf597fc7beef0ee4ull,
  0xc6e00bf33da88fc2ull, 0xd5a79147930aa725ull, 0x06ca6351e003826full, 0x142929670a0e6e70ull,
  0x27b70a8546d22ffcull, 0x2e1b21385c26c926ull, 0x4d2c6dfc5ac42aedull, 0x53380d139d95b3dfull,
  0x650a73548baf63deull, 0x766a0abb3c77b2a8ull, 0x81c2c92e47edaee6ull, 0x92722c851482353bull,
  0xa2bfe8a14cf10364ull, 0xa81a664bbc423001ull, 0xc24b8b70d0f89791ull, 0xc76c51a30654be30ull,
  0xd192e819d6ef5218ull, 0xd69906245565a910ull, 0xf40e35855771202aull, 0x106aa07032bbd1b8ull,
  0x19a4c116b8d2d0c8ull, 0x1e376c085141ab53ull, 0x2748774cdf8eeb99ull, 0x34b0bcb5e19b48a8ull,
  0x391c0cb3c5c95a63ull, 0x4ed8aa4ae3418acbull, 0x5b9cca4f7763e373ull, 0x682e6ff3d6b2b8a3ull,
  0x748f82ee5defb2fcull, 0x78a5636f43172f60ull, 0x84c87814a1f0ab72ull, 0x8cc702081a6439ecull,
  0x90befffa23631e28ull, 0xa4506cebde82bde9ull, 0xbef9a3f7b2c67915ull, 0xc67178f2e372532bull,
  0xca273eceea26619cull, 0xd186b8c721c0c207ull, 0xeada7dd6cde0eb1eull, 0xf57d4f7fee6ed178ull,
  0x06f067aa72176fbaull, 0x0a637dc5a2c898a6ull, 0x113f9804bef90daeull, 0x1b710b35131c471bull,
  0x28db77f523047d84ull, 0x32caab7b40c72493ull, 0x3c9ebe0a15c9bebcull, 0x431d67c49c100d4cull,
  0x4cc5d4becb3e42b6ull, 0x597f299cfc657e2aull, 0x5fcb6fab3ad6faecull, 0x6c44198c4a475817ull
};

static uint64_t
sha512_ROTR64(uint64_t x, int k) {
  return (x >> k) | (x << (64 - k));
}

static uint64_t
sha512_LOAD64_BE(const uint8_t *p) {
  return
    ((uint64_t)p[0] << 56) |
    ((uint64_t)p[1] << 48) |
    ((uint64_t)p[2] << 40) |
    ((uint64_t)p[3] << 32) |
    ((uint64_t)p[4] << 24) |
    ((uint64_t)p[5] << 16) |
    ((uint64_t)p[6] <<  8) |
    ((uint64_t)p[7]      );
}

static void
sha512_STORE64_BE(uint8_t *p, uint64_t v) {
  p[0] = (uint8_t)(v >> 56);
  p[1] = (uint8_t)(v >> 48);
  p[2] = (uint8_t)(v >> 40);
  p[3] = (uint8_t)(v >> 32);
  p[4] = (uint8_t)(v >> 24);
  p[5] = (uint8_t)(v >> 16);
  p[6] = (uint8_t)(v >>  8);
  p[7] = (uint8_t)(v      );
}

#define Ch(x,y,z)  (z ^ (x & (y ^ z)))
#define Maj(x,y,z) (((x | y) & z) | (x & y))
#define S0(x)      (sha512_ROTR64(x, 28) ^ sha512_ROTR64(x, 34) ^ sha512_ROTR64(x, 39))
#define S1(x)      (sha512_ROTR64(x, 14) ^ sha512_ROTR64(x, 18) ^ sha512_ROTR64(x, 41))
#define G0(x)      (sha512_ROTR64(x,  1) ^ sha512_ROTR64(x,  8) ^ (x >>  7))
#define G1(x)      (sha512_ROTR64(x, 19) ^ sha512_ROTR64(x, 61) ^ (x >>  6))
#define W0(in,i)   (sha512_LOAD64_BE(&in[i * 8]))
#define W1(i)      (G1(w[i - 2]) + w[i - 7] + G0(w[i - 15]) + w[i - 16])
#define STEP(i) \
  t1 = S0(r[0]) + Maj(r[0], r[1], r[2]); \
  t0 = r[7] + S1(r[4]) + Ch(r[4], r[5], r[6]) + sha512_constants[i] + w[i]; \
  r[7] = r[6]; \
  r[6] = r[5]; \
  r[5] = r[4]; \
  r[4] = r[3] + t0; \
  r[3] = r[2]; \
  r[2] = r[1]; \
  r[1] = r[0]; \
  r[0] = t0 + t1;

static void
sha512_blocks(ed25519_hash_context *S, const void *invoid, size_t blocks) {
  uint64_t r[8], w[80], t0, t1;
  size_t i;
  const uint8_t *in = (const uint8_t *)invoid;

  for (i = 0; i < 8; i++) r[i] = S->H[i];

  while (blocks--) {
    for (i =  0; i < 16; i++) { w[i] = W0(in, i); }
    for (i = 16; i < 80; i++) { w[i] = W1(i); }
    for (i =  0; i < 80; i++) { STEP(i); }
    for (i =  0; i <  8; i++) { r[i] += S->H[i]; S->H[i] = r[i]; }
    S->T[0] += SHA512_HASH_BLOCK_SIZE * 8;
    S->T[1] += (!S->T[0]) ? 1 : 0;
    in += SHA512_HASH_BLOCK_SIZE;
  }
}

static void ed25519_hash_init (ed25519_hash_context *S) {
  S->H[0] = 0x6a09e667f3bcc908ull;
  S->H[1] = 0xbb67ae8584caa73bull;
  S->H[2] = 0x3c6ef372fe94f82bull;
  S->H[3] = 0xa54ff53a5f1d36f1ull;
  S->H[4] = 0x510e527fade682d1ull;
  S->H[5] = 0x9b05688c2b3e6c1full;
  S->H[6] = 0x1f83d9abfb41bd6bull;
  S->H[7] = 0x5be0cd19137e2179ull;
  S->T[0] = 0;
  S->T[1] = 0;
  S->leftover = 0;
}

static void ed25519_hash_update (ed25519_hash_context *S, const void *invoid, size_t inlen) {
  size_t blocks, want;
  const uint8_t *in = (const uint8_t *)invoid;

  /* handle the previous data */
  if (S->leftover) {
    want = (SHA512_HASH_BLOCK_SIZE - S->leftover);
    want = (want < inlen) ? want : inlen;
    memcpy(S->buffer + S->leftover, in, want);
    S->leftover += (uint32_t)want;
    if (S->leftover < SHA512_HASH_BLOCK_SIZE)
      return;
    in += want;
    inlen -= want;
    sha512_blocks(S, S->buffer, 1);
  }

  /* handle the current data */
  blocks = (inlen & ~(SHA512_HASH_BLOCK_SIZE - 1));
  S->leftover = (uint32_t)(inlen - blocks);
  if (blocks) {
    sha512_blocks(S, in, blocks / SHA512_HASH_BLOCK_SIZE);
    in += blocks;
  }

  /* handle leftover data */
  if (S->leftover)
    memcpy(S->buffer, in, S->leftover);
}

static void ed25519_hash_final (ed25519_hash_context *S, ed25519_sha512_hash hash) {
  uint64_t t0 = S->T[0] + (S->leftover * 8), t1 = S->T[1];

  S->buffer[S->leftover] = 0x80;
  if (S->leftover <= 111) {
    memset(S->buffer + S->leftover + 1, 0, 111 - S->leftover);
  } else {
    memset(S->buffer + S->leftover + 1, 0, 127 - S->leftover);
    sha512_blocks(S, S->buffer, 1);
    memset(S->buffer, 0, 112);
  }

  sha512_STORE64_BE(S->buffer + 112, t1);
  sha512_STORE64_BE(S->buffer + 120, t0);
  sha512_blocks(S, S->buffer, 1);

  sha512_STORE64_BE(&hash[ 0], S->H[0]);
  sha512_STORE64_BE(&hash[ 8], S->H[1]);
  sha512_STORE64_BE(&hash[16], S->H[2]);
  sha512_STORE64_BE(&hash[24], S->H[3]);
  sha512_STORE64_BE(&hash[32], S->H[4]);
  sha512_STORE64_BE(&hash[40], S->H[5]);
  sha512_STORE64_BE(&hash[48], S->H[6]);
  sha512_STORE64_BE(&hash[56], S->H[7]);
}

/*
void
ED25519_FN(ed25519_hash)(ed25519_sha512_hash hash, const void *in, size_t inlen) {
  ed25519_hash_context ctx;
  ed25519_hash_init(&ctx);
  ed25519_hash_update(&ctx, in, inlen);
  ed25519_hash_final(&ctx, hash);
}
*/
#endif


static size_t
ed25519_hram_stream(hash_512bits hram, const ed25519_signature RS, const ed25519_public_key pk, ed25519_iostream *strm) {
  ed25519_hash_context ctx;
  ed25519_hash_init(&ctx);
  ed25519_hash_update(&ctx, RS, 32);
  ed25519_hash_update(&ctx, pk, 32);
  //ed25519_hash_update(&ctx, m, mlen);
  const size_t mlen = hash_stream(strm, &ctx);
  if (mlen != 0) ed25519_hash_final(&ctx, hram);
  return mlen;
}

static int ed25519_sign_check_stream (ed25519_iostream *strm, const ed25519_public_key pk,
                                      const ed25519_signature RS)
{
  ge25519 ALIGN(16) R, A;
  hash_512bits hash;
  bignum256modm hram, S;
  unsigned char checkR[32];

  if ((RS[63] & 224) || !ge25519_unpack_negative_vartime(&A, pk))
    return -1;

  /* hram = H(R,A,m) */
  const size_t mlen = ed25519_hram_stream(hash, RS, pk, strm);
  if (mlen == 0) return -1;
  expand256_modm(hram, hash, 64);

  /* S */
  expand256_modm(S, RS + 32, 32);

  /* SB - H(R,A,m)A */
  ge25519_double_scalarmult_vartime(&R, &A, hram, S);
  ge25519_pack(checkR, &R);

  /* check that R = SB - H(R,A,m)A */
  return ed25519_verify(RS, checkR, 32) ? 0 : -1;
}

#if defined(__cplusplus)
}
#endif
#endif


// ////////////////////////////////////////////////////////////////////////// //
#if defined(__cplusplus)
extern "C" {
#endif


// ////////////////////////////////////////////////////////////////////////// //
void (*vwad_logf) (int type, const char *fmt, ...) = NULL;

#define logf(type_,...)  do { \
  if (vwad_logf) { \
    vwad_logf(VWAD_LOG_ ## type_, __VA_ARGS__); \
  } \
} while (0)


// ////////////////////////////////////////////////////////////////////////// //
void (*vwad_assertion_failed) (const char *fmt, ...) = NULL;

static inline __attribute__((cold)) const char *SkipPathPartCStr (const char *s) {
  const char *lastSlash = NULL;
  for (const char *t = s; *t; ++t) {
    if (*t == '/') lastSlash = t+1;
    #ifdef _WIN32
    if (*t == '\\') lastSlash = t+1;
    #endif
  }
  return (lastSlash ? lastSlash : s);
}

#define vassert(cond_)  do { if (__builtin_expect((!(cond_)), 0)) { const int line__assf = __LINE__; \
    if (vwad_assertion_failed) { \
      vwad_assertion_failed("%s:%d: Assertion in `%s` failed: %s\n", \
        SkipPathPartCStr(__FILE__), line__assf, __PRETTY_FUNCTION__, #cond_); \
      __builtin_trap(); /* just in case */ \
    } else { \
      __builtin_trap(); \
    } \
  } \
} while (0)


// ////////////////////////////////////////////////////////////////////////// //
// memory allocation
static inline void *xalloc (vwad_memman *mman, size_t size) {
  vassert(size > 0 && size <= 0x7ffffff0u);
  if (mman != NULL) return mman->alloc(mman, (uint32_t)size); else return malloc(size);
}


static inline void *zalloc (vwad_memman *mman, size_t size) {
  vassert(size > 0 && size <= 0x7ffffff0u);
  void *p = (mman != NULL ? mman->alloc(mman, (uint32_t)size) : malloc(size));
  if (p) memset(p, 0, size);
  return p;
}


static inline void xfree (vwad_memman *mman, void *p) {
  if (p != NULL) {
    if (mman != NULL) mman->free(mman, p); else free(p);
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


static inline uint32_t crc32_part (uint32_t crc32, const void *src, size_t len) {
  const uint8_t *buf = (const uint8_t *)src;
  while (len--) {
    CRC32BYTE(*buf++);
  }
  return crc32;
}

static inline uint32_t crc32_final (uint32_t crc32) {
  return crc32^0xffffffffU;
}

static inline uint32_t crc32_buf (const void *src, size_t len) {
  return crc32_final(crc32_part(crc32_init, src, len));
}


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
  Predictor predBits[2 * 2 * 256];
  Predictor predSize[2];
  int ctxBits[2];
  int ctxSize;
} BytePPM;


// ////////////////////////////////////////////////////////////////////////// //
static inline uint8_t DecGetByte (EntDecoder *ee) {
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

static inline intbool_t DecDecode (EntDecoder *ee, int p) {
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

static inline int PredGetP (Predictor *pp) {
  return (int)((uint32_t)pp->p1 + (uint32_t)pp->p2);
}

static inline void PredUpdate (Predictor *pp, intbool_t bit) {
  if (bit) {
    pp->p1 += ((~((uint32_t)pp->p1)) >> 3) & 0b0001111111111111;
    pp->p2 += ((~((uint32_t)pp->p2)) >> 6) & 0b0000001111111111;
  } else {
    pp->p1 -= ((uint32_t)pp->p1) >> 3;
    pp->p2 -= ((uint32_t)pp->p2) >> 6;
  }
}

static inline intbool_t PredGetBitDecodeUpdate (Predictor *pp, EntDecoder *dec) {
  int p = PredGetP(pp);
  intbool_t bit = DecDecode(dec, p);
  PredUpdate(pp, bit);
  return bit;
}


// ////////////////////////////////////////////////////////////////////////// //
static void BitPPMInit (BitPPM *ppm) {
  for (size_t f = 0; f < sizeof(ppm->pred) / sizeof(ppm->pred[0]); ++f) PredInit(&ppm->pred[f]);
  ppm->ctx = 1;
}

static inline intbool_t BitPPMDecode (BitPPM *ppm, EntDecoder *dec) {
  intbool_t bit = PredGetBitDecodeUpdate(&ppm->pred[ppm->ctx], dec);
  if (bit) ppm->ctx = 1; else ppm->ctx = 0;
  return bit;
}


// ////////////////////////////////////////////////////////////////////////// //
static void BytePPMInit (BytePPM *ppm) {
  for (size_t f = 0; f < sizeof(ppm->predBits) / sizeof(ppm->predBits[0]); ++f) PredInit(&ppm->predBits[f]);
  for (size_t f = 0; f < sizeof(ppm->predSize) / sizeof(ppm->predSize[0]); ++f) PredInit(&ppm->predSize[f]);
  ppm->ctxBits[0] = 0; ppm->ctxBits[1] = 0; ppm->ctxSize = 0;
}

static inline uint8_t BytePPMDecodeByte (BytePPM *ppm, EntDecoder *dec, int bidx) {
  uint32_t n = 1;
  int cofs = 512 * bidx + ppm->ctxBits[bidx] * 256;
  do {
    intbool_t bit = PredGetBitDecodeUpdate(&ppm->predBits[cofs + n], dec);
    n += n; if (bit) ++n;
  } while (n < 0x100);
  n -= 0x100;
  ppm->ctxBits[bidx] = (n >= 0x80);
  return (uint8_t)n;
}

static inline int BytePPMDecodeInt (BytePPM *ppm, EntDecoder *dec) {
  int n = BytePPMDecodeByte(ppm, dec, 0);
  intbool_t bit = PredGetBitDecodeUpdate(&ppm->predSize[ppm->ctxSize], dec);
  if (bit) {
    ppm->ctxSize = 1;
    n += BytePPMDecodeByte(ppm, dec, 1) * 0x100;
  } else {
    ppm->ctxSize = 0;
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
  BytePPM ppmData, ppmMtOfs, ppmMtLen, ppmLitLen;
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
  BytePPMInit(&ppmMtOfs);
  BytePPMInit(&ppmMtLen);
  BytePPMInit(&ppmLitLen);
  BitPPMInit(&ppmLitFlag);

  DecInit(&dec, src, srclen);

  if (!BitPPMDecode(&ppmLitFlag, &dec)) error = int_true;
  else {
    uint8_t sch;
    int len, ofs;

    litcount = BytePPMDecodeInt(&ppmLitLen, &dec) + 1;
    while (!error && litcount != 0) {
      litcount -= 1;
      n = BytePPMDecodeByte(&ppmData, &dec, 0);
      PutByte((uint8_t)n);
      error = error || (dec.spos > dec.send);
    }

    while (!error && unpsize != 0) {
      if (BitPPMDecode(&ppmLitFlag, &dec)) {
        litcount = BytePPMDecodeInt(&ppmLitLen, &dec) + 1;
        while (!error && litcount != 0) {
          litcount -= 1;
          n = BytePPMDecodeByte(&ppmData, &dec, 0);
          PutByte((uint8_t)n);
          error = error || (dec.spos > dec.send);
        }
      } else {
        len = BytePPMDecodeInt(&ppmMtLen, &dec) + 3;
        ofs = BytePPMDecodeInt(&ppmMtOfs, &dec) + 1;
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
// fnv-1a: http://www.isthe.com/chongo/tech/comp/fnv/
static __attribute__((unused)) inline uint32_t fnvHashStrCI (const void *buf) {
  uint32_t hash = 2166136261U; // fnv offset basis
  const uint8_t *s = (const uint8_t *)buf;
  while (*s) {
    uint8_t ch = *s++;
    if (ch == '\\') ch = '/';
    hash ^= ch|0x20; // this converts ASCII capitals to locase (and destroys other, but who cares)
    hash *= 16777619U; // 32-bit fnv prime
  }
  return hash;
}


static int nameEqu (const char *s0, const char *s1) {
  if (!s0 || !s1) return 0;
  while (*s0 && *s1) {
    uint8_t c0 = *s0++;
    if (c0 == '\\') c0 = '/';
    else if (c0 >= 'A' && c0 <= 'Z') c0 = c0 - 'A' + 'a';
    uint8_t c1 = *s1++;
    if (c1 == '\\') c1 = '/';
    else if (c1 >= 'A' && c1 <= 'Z') c1 = c1 - 'A' + 'a';
    if (c0 != c1) return 0;
  }
  return (!s0[0] && !s1[0]);
}


static inline uint32_t hashU32 (uint32_t a) {
  a -= (a<<6);
  a ^= (a>>17);
  a -= (a<<9);
  a ^= (a<<4);
  a -= (a<<3);
  a ^= (a<<10);
  a ^= (a>>15);
  return a;
}


// ////////////////////////////////////////////////////////////////////////// //
// it MUST be a prime
#define HASH_BUCKETS  (1021)

#define MAX_OPENED_FILES  (128)
#define MAX_BUFFERS       (2)


typedef struct __attribute__((packed)) {
  uint32_t ofs;     // offset in stream
  uint16_t upksize; // unpacked chunk size-1
  uint16_t pksize;  // packed chunk size (0 means "unpacked")
} ChunkInfo;

typedef struct __attribute__((packed)) {
  uint32_t fchunk;     // first chunk
  uint32_t nhash;      // name hash
  uint32_t bknext;     // next name in bucket
  uint64_t ftime;      // since Epoch, 0 is "unknown"
  uint32_t upksize;    // unpacked file size
  uint32_t chunkCount; // number of chunks
  uint32_t nameofs;    // name offset in names array
} FileInfo;

typedef struct {
  uint32_t fofs; // virtual file offset, aligned to 65536
  uint32_t size; // 0: the buffer is not used
  uint32_t era;
  uint8_t data[65536];
} FileBuffer;

typedef struct {
  uint32_t fidx;  // 0: unused
  uint32_t fofs;  // virtual file offset
  uint32_t lastera;
  uint32_t bidx;
  FileBuffer bufs[MAX_BUFFERS];
  uint8_t pkdata[65536];
} OpenedFile;

struct vwad_handle_t {
  vwad_iostream *strm;
  vwad_memman *mman;
  uint32_t flags;
  uint8_t *updir;      // unpacked directory
  ChunkInfo *chunks;   // points to the unpacked directory
  uint32_t chunkCount; // number of elements in `chunks` array
  // files (0 is unused)
  FileInfo *files;     // points to the unpacked directory
  uint32_t fileCount;  // number of elements in `files` array
  // file names (0-terminated)
  const char *names;   // points to the unpacked directory
  uint32_t namesSize;  // number of bytes in `names` array
  // directory hash table
  uint32_t buckets[HASH_BUCKETS];
  // public key
  uint32_t haspubkey; // bit 0: has key; bit 1: authenticated
  vwad_public_key pubkey;
  // opened files
  OpenedFile *fds[MAX_OPENED_FILES];
};


#if !defined(VWAD_VFS_DISABLE_ED25519_CODE)
typedef struct {
  vwad_iostream *strm;
  int pos, size;
} EdInfo;


//==========================================================================
//
//  ed_rewind
//
//==========================================================================
static int ed_rewind (ed25519_iostream *strm) {
  EdInfo *nfo = (EdInfo *)strm->udata;
  if (nfo->strm->seek(nfo->strm, 0) != 0) return -1;
  nfo->pos = 0;
  return 0;
}


static int ed_read (ed25519_iostream *strm, void *buf, int bufsize) {
  EdInfo *nfo = (EdInfo *)strm->udata;
  int pos = nfo->pos;
  if (pos < 0) return -1; // oops
  if (pos >= nfo->size) return 0;
  int max = nfo->size - pos;
  if (bufsize > max) bufsize = max;
  if (pos < 3*4) {
    max = 3*4 - pos;
    if (bufsize > max) bufsize = max;
    if (nfo->strm->read(nfo->strm, buf, bufsize) != 0) return -1;
  } else if (pos < 3*4 + (int)sizeof(ed25519_public_key) + (int)sizeof(ed25519_signature)) {
    int max = 3*4 + (int)sizeof(ed25519_public_key) + (int)sizeof(ed25519_signature) - pos;
    if (bufsize > max) bufsize = (int)max;
    memset(buf, 0, (size_t)bufsize);
    if (nfo->strm->seek(nfo->strm, pos + bufsize) != 0) return -1;
  } else {
    if (nfo->strm->read(nfo->strm, buf, bufsize) != 0) return -1;
  }
  nfo->pos += bufsize;
  return bufsize;
}
#endif


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
  char sign[4];
  uint32_t ver, dirofs;

  // file signature
  if (strm->read(strm, sign, 4) != 0) {
    logf(ERROR, "vwad_open_archive: cannot read signature");
    return NULL;
  }
  if (memcmp(sign, "VWAD", 4) != 0) {
    logf(ERROR, "vwad_open_archive: invalid signature");
    return NULL;
  }

  if (strm->read(strm, &ver, 4) != 0) {
    logf(ERROR, "vwad_open_archive: cannot read version");
    return NULL;
  }
  if (ver != 0) {
    logf(ERROR, "vwad_open_archive: invalid version");
    return NULL;
  }

  if (strm->read(strm, &dirofs, 4) != 0) {
    logf(ERROR, "vwad_open_archive: cannot read directory offset");
    return NULL;
  }
  if (dirofs <= 4*3+32+64) {
    logf(ERROR, "vwad_open_archive: invalid directory offset");
    return NULL;
  }

  if (strm->read(strm, &pubkey, (uint32_t)sizeof(pubkey)) != 0) {
    logf(ERROR, "vwad_open_archive: cannot read pubkey");
    return NULL;
  }
  if (strm->read(strm, &edsign, (uint32_t)sizeof(edsign)) != 0) {
    logf(ERROR, "vwad_open_archive: cannot read edsign");
    return NULL;
  }

  // determine file size
  if (strm->seek(strm, dirofs) != 0) {
    logf(ERROR, "vwad_open_archive: cannot seek to directory");
    return NULL;
  }

  logf(DEBUG, "vwad_open_archive: dirofs=0x%08x", dirofs);

  uint32_t pkdir_crc32;
  uint32_t dir_crc32;
  uint32_t pkdirsize;
  uint32_t upkdirsize;

  if (strm->read(strm, &pkdir_crc32, 4) != 0) {
    logf(ERROR, "vwad_open_archive: cannot read packed directory crc32");
    return NULL;
  }
  if (strm->read(strm, &dir_crc32, 4) != 0) {
    logf(ERROR, "vwad_open_archive: cannot read directory crc32");
    return NULL;
  }
  if (strm->read(strm, &pkdirsize, 4) != 0) {
    logf(ERROR, "vwad_open_archive: cannot read directory size");
    return NULL;
  }
  if (strm->read(strm, &upkdirsize, 4) != 0) {
    logf(ERROR, "vwad_open_archive: cannot read directory size");
    return NULL;
  }

  pkdirsize ^= hashU32(pkdir_crc32);
  upkdirsize ^= hashU32(dir_crc32);
  logf(DEBUG, "vwad_open_archive: pkdirsize=0x%08x", pkdirsize);
  logf(DEBUG, "vwad_open_archive: upkdirsize=0x%08x", upkdirsize);
  if (pkdirsize == 0 || pkdirsize > 0xffffff) {
    logf(ERROR, "vwad_open_archive: invalid directory size");
    return NULL;
  }
  if (upkdirsize <= 4*11 || upkdirsize > 0xffffff) {
    logf(ERROR, "vwad_open_archive: invalid directory size");
    return NULL;
  }
  if (0x7fffffffU - dirofs < pkdirsize) {
    logf(ERROR, "vwad_open_archive: invalid directory size");
    return NULL;
  }

  // check digital signature
  uint32_t haspubkey = 0;
  uint8_t sum = 0;
  for (size_t bidx = 0; bidx < sizeof(pubkey); ++bidx) sum |= pubkey[bidx];
  if (sum != 0) {
    haspubkey = 1; // has a key
    #if !defined(VWAD_VFS_DISABLE_ED25519_CODE)
    if ((flags & VWAD_OPEN_NO_SIGN_CHECK) == 0) {
      ed25519_iostream edstrm;
      EdInfo nfo;
      nfo.strm = strm;
      nfo.pos = 0; nfo.size = (int)(dirofs + pkdirsize + 4*4);
      logf(DEBUG, "vwad_open_archive: file size: %d", nfo.size);
      edstrm.udata = &nfo;
      edstrm.rewind = ed_rewind;
      edstrm.read = ed_read;

      logf(NOTE, "checking digital signature...");
      int sres = ed25519_sign_check_stream(&edstrm, pubkey, edsign);
      if (sres != 0) {
        logf(ERROR, "vwad_open_archive: invalid digital signature");
        return NULL;
      }
      haspubkey = 3; // has a key, and authenticated
    }
    #endif
  } else {
    for (size_t bidx = 0; bidx < sizeof(edsign); ++bidx) sum |= edsign[bidx];
    if (sum != 0) {
      logf(ERROR, "vwad_open_archive: invalid digital signature");
      return NULL;
    }
  }

  // read and unpack directory
  if (strm->seek(strm, dirofs + 4*4) != 0) {
    logf(ERROR, "vwad_open_archive: cannot seek to directory data");
    return NULL;
  }

  void *unpkdir = xalloc(mman, upkdirsize + 1); // always end it with 0
  if (!unpkdir) {
    logf(ERROR, "vwad_open_archive: cannot allocate memory for unpacked directory");
    return NULL;
  }
  ((uint8_t *)unpkdir)[upkdirsize] = 0;

  void *pkdir = xalloc(mman, pkdirsize);
  if (!pkdir) {
    xfree(mman, unpkdir);
    logf(ERROR, "vwad_open_archive: cannot allocate memory for packed directory");
    return NULL;
  }

  if (strm->read(strm, pkdir, pkdirsize) != 0) {
    xfree(mman, pkdir);
    xfree(mman, unpkdir);
    logf(ERROR, "vwad_open_archive: cannot read directory data");
    return NULL;
  }

  uint32_t crc32 = crc32_buf(pkdir, pkdirsize);
  if (crc32 != pkdir_crc32) {
    xfree(mman, pkdir);
    xfree(mman, unpkdir);
    logf(DEBUG, "vwad_open_archive: pkcrc: file=0x%08x; real=0x%08x", pkdir_crc32, crc32);
    logf(ERROR, "vwad_open_archive: corrupted packed directory data");
    return NULL;
  }

  if (!DecompressLZFF3(pkdir, pkdirsize, unpkdir, upkdirsize)) {
    xfree(mman, pkdir);
    xfree(mman, unpkdir);
    logf(ERROR, "vwad_open_archive: cannot decompress directory");
    return NULL;
  }
  xfree(mman, pkdir);

  crc32 = crc32_buf(unpkdir, upkdirsize);
  if (crc32 != dir_crc32) {
    xfree(mman, unpkdir);
    logf(DEBUG, "vwad_open_archive: upkcrc: file=0x%08x; real=0x%08x", dir_crc32, crc32);
    logf(ERROR, "vwad_open_archive: corrupted unpacked directory data");
    return NULL;
  }


  vwad_handle *wad = zalloc(mman, sizeof(vwad_handle));
  if (wad == NULL) {
    xfree(mman, unpkdir);
    logf(ERROR, "vwad_open_archive: cannot allocate memory for vwad handle");
    return NULL;
  }
  wad->strm = strm;
  wad->mman = mman;
  wad->flags = (uint32_t)flags;
  wad->updir = unpkdir;
  if (haspubkey) {
    vassert(sizeof(wad->pubkey) == sizeof(pubkey));
    memcpy(wad->pubkey, pubkey, sizeof(wad->pubkey));
    wad->haspubkey = haspubkey;
  } else {
    memset(wad->pubkey, 0, sizeof(wad->pubkey));
    wad->haspubkey = 0;
  }

  // setup and check chunks
  memcpy(&wad->chunkCount, wad->updir, 4);
  uint32_t upofs = 4;
  if (wad->chunkCount == 0 || wad->chunkCount > 0x3fffffffU/(uint32_t)sizeof(ChunkInfo) ||
      wad->chunkCount*(uint32_t)sizeof(ChunkInfo) >= upkdirsize ||
      wad->chunkCount*(uint32_t)sizeof(ChunkInfo) >= upkdirsize - upofs)
  {
    logf(DEBUG, "invalid chunk count (%u)", wad->chunkCount);
    goto error;
  }
  logf(DEBUG, "chunk count: %u", wad->chunkCount);
  wad->chunks = (ChunkInfo *)(wad->updir + upofs);
  upofs += wad->chunkCount * (uint32_t)sizeof(ChunkInfo);

  for (uint32_t cidx = 0; cidx < wad->chunkCount; ++cidx) {
    if (wad->chunks[cidx].ofs != 0 || wad->chunks[cidx].upksize != 0) {
      logf(DEBUG, "invalid chunk data (0; idx=%u): ofs=%u; upksize=%u",
           cidx, wad->chunks[cidx].ofs, wad->chunks[cidx].upksize);
      goto error;
    }
    wad->chunks[cidx].ofs = 0xffffffffU;
  }

  // files
  if (upofs >= upkdirsize || upkdirsize - upofs < 4 + (uint32_t)sizeof(FileInfo)) {
    logf(DEBUG, "invalid directory data (files, 0)");
    goto error;
  }
  memcpy(&wad->fileCount, wad->updir + upofs, 4); upofs += 4;
  if (wad->fileCount < 2 || wad->fileCount > 0x3fffffffU/(uint32_t)sizeof(FileInfo) ||
      wad->fileCount * (uint32_t)sizeof(FileInfo) >= upkdirsize ||
      wad->fileCount * (uint32_t)sizeof(FileInfo) >= upkdirsize - upofs)
  {
    logf(DEBUG, "invalid file count (%u)", wad->fileCount);
    goto error;
  }
  logf(DEBUG, "file count: %u", wad->fileCount);
  wad->files = (FileInfo *)(wad->updir + upofs);
  upofs += wad->fileCount * (uint32_t)sizeof(FileInfo);

  // names
  if (upofs >= upkdirsize || upkdirsize - upofs < 4 + 2) {
    logf(DEBUG, "invalid directory data (names, 0)");
    goto error;
  }
  memcpy(&wad->namesSize, wad->updir + upofs, 4); upofs += 4;
  if (wad->namesSize < 2 || wad->namesSize > 0x3fffffffU ||
      wad->namesSize >= upkdirsize ||
      wad->namesSize != upkdirsize - upofs)
  {
    logf(DEBUG, "invalid names size (%u)", wad->namesSize);
    goto error;
  }
  logf(DEBUG, "name table size: %u", wad->namesSize);
  wad->names = (char *)(wad->updir + upofs);

  // check if files are ok, and setup chunk info
  // 0th is unused, and should be zeroed
  if (wad->files[0].upksize != 0 ||
      wad->files[0].fchunk != 0 ||
      wad->files[0].chunkCount != 0 ||
      wad->files[0].nhash != 0 ||
      wad->files[0].bknext != 0 ||
      wad->files[0].nameofs != 0)
  {
    logf(DEBUG, "invalid file data");
    goto error;
  }

  uint32_t chunkOfs = 4*3 + 32 + 64;
  uint32_t currChunk = 0;

  for (uint32_t fidx = 1; fidx < wad->fileCount; ++fidx) {
    FileInfo *fi = &wad->files[fidx];

    if (fi->nhash != 0 || fi->bknext != 0 || fi->fchunk != 0) {
      logf(DEBUG, "invalid file data");
      goto error;
    }
    if (fi->chunkCount == 0) {
      if (fi->upksize != 0) {
        logf(DEBUG, "invalid file data");
        goto error;
      }
    } else {
      if (fi->upksize == 0) {
        logf(DEBUG, "invalid file data");
        goto error;
      }
    }
    if (fi->upksize > 0x7fffffffU || fi->nameofs >= wad->namesSize) {
      logf(DEBUG, "invalid file data");
      goto error;
    }
    // should be aligned
    if (fi->nameofs&0x03) {
      logf(DEBUG, "invalid file data");
      goto error;
    }

    const char *name = wad->names + fi->nameofs;
    if (!name[0]) {
      logf(DEBUG, "invalid file data");
      goto error;
    }

    // insert name into hash table
    fi->nhash = fnvHashStrCI(name);
    const uint32_t bkt = fi->nhash % HASH_BUCKETS;
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
        if (chunkOfs >= dirofs) {
          logf(DEBUG, "invalid file data (chunk offset); fidx=%u; cofs=0x%08x; dofs=0x%08x",
                  fidx, chunkOfs, dirofs);
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
        if (chunkOfs > dirofs) {
          logf(DEBUG, "invalid file data (chunk offset 1); fidx=%u/%u; cofs=0x%08x; dofs=0x%08x",
                  fidx, wad->fileCount, chunkOfs, dirofs);
          goto error;
        }
        currChunk += 1;
      }
    }
  }
  if (chunkOfs != dirofs) {
    logf(DEBUG, "invalid file data (extra chunk); cofs=0x%08x; dofs=0x%08x", chunkOfs, dirofs);
    goto error;
  }

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
      for (vwad_fd fd = 0; fd < MAX_OPENED_FILES; ++fd) {
        vwad_fclose(wad, fd);
      }
      xfree(mman, wad->updir);
      memset(wad, 0, sizeof(vwad_handle));
      xfree(mman, wad);
    }
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
//  vwad_get_max_fidx
//
//==========================================================================
vwad_fidx vwad_get_max_fidx (vwad_handle *wad) {
  if (wad && wad->fileCount > 1) {
    return wad->fileCount - 1;
  } else {
    return 0;
  }
}


//==========================================================================
//
//  vwad_get_file_name
//
//==========================================================================
const char *vwad_get_file_name (vwad_handle *wad, vwad_fidx fidx) {
  if (wad && fidx > 0 && wad->fileCount > 1 && fidx < wad->fileCount) {
    return wad->names + wad->files[fidx].nameofs;
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
  if (wad && fidx > 0 && wad->fileCount > 1 && fidx < wad->fileCount) {
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
  if (name) {
    for (;;) {
      if (name[0] == '/' || name[0] == '\\') ++name;
      else if (name[0] == '.' && (name[1] == '/' || name[1] == '\\')) name += 2;
      else break;
    }
  }
  if (wad && wad->fileCount > 1 && name && name[0]) {
    const uint32_t hash = fnvHashStrCI(name);
    const uint32_t bkt = hash % HASH_BUCKETS;
    uint32_t fidx = wad->buckets[bkt];
    while (fidx != 0) {
      if (wad->files[fidx].nhash == hash &&
          nameEqu(wad->names + wad->files[fidx].nameofs, name))
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
  if (wad && fidx > 0 && wad->fileCount > 1 && fidx < wad->fileCount) {
    res = wad->files[fidx].ftime;
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
  if (wad && fidx > 0 && wad->fileCount > 1 && fidx < wad->fileCount) {
    // find free fd
    vwad_fd fd = 0;
    while (fd < MAX_OPENED_FILES && wad->fds[fd] != NULL) fd += 1;
    if (fd != MAX_OPENED_FILES) {
      // i found her!
      OpenedFile *fl = zalloc(wad->mman, sizeof(OpenedFile));
      if (fl != NULL) {
        wad->fds[fd] = fl;
        fl->fidx = fidx;
        fl->fofs = 0;
        fl->lastera = 1;
        fl->bidx = 0;
        for (unsigned bidx = 0; bidx < MAX_BUFFERS; ++bidx) {
          fl->bufs[bidx].fofs = 0;
          fl->bufs[bidx].size = 0;
          fl->bufs[bidx].era = 0;
        }
        return fd;
      }
    } else {
      return -2;
    }
  }
  return -1;
}


//==========================================================================
//
//  vwad_has_opened_files
//
//==========================================================================
vwad_bool vwad_has_opened_files (vwad_handle *wad) {
  if (wad) {
    vwad_fd fd = 0;
    while (fd < MAX_OPENED_FILES) {
      if (wad->fds[fd] != NULL) return 1;
      ++fd;
    }
  }
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
  vassert(wad->fds[fd] != NULL);
  OpenedFile *fl = wad->fds[fd];
  const FileInfo *fi = &wad->files[fl->fidx];
  if (ofs >= fi->upksize) return NULL;
  const uint32_t bufofs = ofs / 65536; // virtual buffer offset
  FileBuffer *res = &fl->bufs[fl->bidx];
  // do we have this buffer?
  if (res->fofs != bufofs || res->size == 0) {
    int bidx = fl->bidx;
    while (bidx < MAX_BUFFERS && (fl->bufs[bidx].fofs != bufofs || fl->bufs[bidx].size == 0)) {
      bidx += 1;
    }
    if (bidx != MAX_BUFFERS && fl->bufs[bidx].fofs == bufofs && fl->bufs[bidx].size != 0) {
      #if 0
      fprintf(stderr, "*0:ofs=0x%08x; bufofs=0x%08x; bidx=%d; bofs=0x%08x; size=%d; era=%d\n",
              ofs, bufofs, bidx, fl->bufs[bidx].fofs, fl->bufs[bidx].size,
              fl->bufs[bidx].era);
      #endif
      fl->bidx = (uint32_t)bidx;
    } else {
      // need new buffer
      int goodidx = -1;
      uint32_t goodera = 0xffffffffU;
      bidx = 0;
      while (bidx != MAX_BUFFERS) {
        #if 0
        fprintf(stderr, "*1:ofs=0x%08x; bufofs=0x%08x; bidx=%d; bofs=0x%08x; size=%d; era=%d\n",
                ofs, bufofs, bidx, fl->bufs[bidx].fofs, fl->bufs[bidx].size,
                fl->bufs[bidx].era);
        #endif
        if (fl->bufs[bidx].size == 0) {
          goodidx = bidx; bidx = MAX_BUFFERS;
        } else {
          if (fl->bufs[bidx].era < goodera) {
            goodidx = bidx; goodera = fl->bufs[bidx].era;
          }
          bidx += 1;
        }
      }
      vassert(goodidx >= 0 && goodidx < MAX_BUFFERS);
      vassert(bufofs < fi->chunkCount);
      // unpack it
      const ChunkInfo *ci = &wad->chunks[fi->fchunk + bufofs];
      if (wad->strm->seek(wad->strm, ci->ofs) != 0) {
        logf(DEBUG, "ensure_buffer: cannot seek to chunk %u", fi->fchunk + bufofs);
        return NULL;
      }
      uint32_t crc32;
      if (wad->strm->read(wad->strm, &crc32, 4) != 0) {
        logf(DEBUG, "ensure_buffer: cannot read chunk %u crc32", fi->fchunk + bufofs);
        return NULL;
      }
      uint32_t cupsize = ci->upksize + 1;
      //vassert(cupsize > 0 && cupsize <= 65536);
      // packed data
      res = &fl->bufs[goodidx];
      if (ci->pksize == 0) {
        // unpacked, but xored
        if (wad->strm->read(wad->strm, &fl->pkdata, cupsize) != 0) {
          logf(DEBUG, "ensure_buffer: cannot read unpacked chunk %u", fi->fchunk + bufofs);
          return NULL;
        }
        uint8_t key[4];
        memcpy(key, &crc32, 4);
        const uint8_t *src = fl->pkdata;
        uint8_t *dest = res->data;
        for (uint32_t f = 0; f < cupsize; ++f) {
          dest[f] = src[f] ^ key[f % 4];
        }
      } else {
        // packed
        if (wad->strm->read(wad->strm, &fl->pkdata, ci->pksize) != 0) {
          logf(DEBUG, "ensure_buffer: cannot read packed chunk %u", fi->fchunk + bufofs);
          return NULL;
        }
        if (!DecompressLZFF3(fl->pkdata, ci->pksize, res->data, cupsize)) {
          logf(DEBUG, "ensure_buffer: cannot unpack chunk %u (%u -> %u)", fi->fchunk + bufofs,
                  ci->pksize, cupsize);
          res->size = 0;
          return NULL;
        }
      }
      // check crc
      res->size = cupsize;
      if ((wad->flags & VWAD_OPEN_NO_CRC_CHECKS) == 0) {
        if (crc32_buf(res->data, cupsize) != crc32) {
          logf(DEBUG, "ensure_buffer: corrupted chunk %u data (crc32)", fi->fchunk + bufofs);
          res->size = 0;
          return NULL;
        }
      }
      res->fofs = bufofs;
      fl->bidx = (uint32_t)goodidx;
    }
  }
  // i found her!
  res = &fl->bufs[fl->bidx];
  vassert(res->fofs == bufofs);
  vassert(res->size != 0);
  // fix buffer era
  if (res->era != fl->lastera) {
    if (fl->lastera == 0xffffffffU) {
      fl->lastera = 1;
      for (int f = 0; f < MAX_BUFFERS; ++f) fl->bufs[f].era = 0;
    }
    res->era = fl->lastera;
    ++fl->lastera;
  }
  return res;
}


//==========================================================================
//
//  vwad_fclose
//
//==========================================================================
void vwad_fclose (vwad_handle *wad, vwad_fd fd) {
  if (wad && fd >= 0 && fd < MAX_OPENED_FILES && wad->fileCount > 1) {
    OpenedFile *fl = wad->fds[fd];
    if (fl) {
      xfree(wad->mman, fl);
      wad->fds[fd] = NULL;
    }
  }
}


//==========================================================================
//
//  vwad_fdfidx
//
//==========================================================================
vwad_fidx vwad_fdfidx (vwad_handle *wad, vwad_fd fd) {
  if (wad && fd >= 0 && fd < MAX_OPENED_FILES && wad->fileCount > 1) {
    OpenedFile *fl = wad->fds[fd];
    if (fl) {
      return (vwad_fidx)fl->fidx;
    } else {
      return 0;
    }
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
  if (wad && pos >= 0 && fd >= 0 && fd < MAX_OPENED_FILES && wad->fileCount > 1) {
    OpenedFile *fl = wad->fds[fd];
    if (fl) {
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
  if (wad && fd >= 0 && fd < MAX_OPENED_FILES && wad->fileCount > 1) {
    OpenedFile *fl = wad->fds[fd];
    if (fl) {
      return (int)fl->fofs;
    } else {
      return -4;
    }
  }
  return -1;
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
  if (wad && len >= 0 && fd >= 0 && fd < MAX_OPENED_FILES && wad->fileCount > 1) {
    OpenedFile *fl = wad->fds[fd];
    if (fl) {
      vassert(len == 0 || dest != NULL);
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
          const uint32_t bufskip = fl->fofs - fbuf->fofs * 65536;
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
//  vwad_can_check_auth
//
//  returns boolean
//
//==========================================================================
vwad_bool vwad_can_check_auth (void) {
  #if !defined(VWAD_VFS_DISABLE_ED25519_CODE)
  return 1;
  #else
  return 0;
  #endif
}


// ////////////////////////////////////////////////////////////////////////// //
#if defined(__cplusplus)
}
#endif
