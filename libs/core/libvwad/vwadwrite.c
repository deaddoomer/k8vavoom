/* coded by Ketmar // Invisible Vector (psyc://ketmar.no-ip.org/~Ketmar)
 * Understanding is not required. Only obedience.
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar.
 * See http://www.wtfpl.net/ for more details.
 */
#include "vwadwrite.h"

#include <stdarg.h>
#include <string.h>

// to test the code
//#define VWAD_COMPILE_DECOMPRESSOR

#define VWAD_USE_NAME_LENGTHES


#define VWADWR_FILE_ENTRY_SIZE   (4 * 10)
#define VWADWR_CHUNK_ENTRY_SIZE  (4 + 2 + 2)


// ////////////////////////////////////////////////////////////////////////// //
#if defined(__cplusplus)
extern "C" {
#endif


// ////////////////////////////////////////////////////////////////////////// //
/* turning off inlining saves ~10kb of binary code on x86 */
#ifdef _MSC_VER
# define CC25519_INLINE  __forceinline
// not used if decompressor is not compiled in
# define VWAD_OKUNUSED
#else
# define CC25519_INLINE  inline __attribute__((always_inline))
// not used if decompressor is not compiled in
# define VWAD_OKUNUSED  __attribute__((unused))
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
# define vwad_packed_struct    __attribute__((packed))
# define vwad_push_pack
# define vwad_pop_pack
#endif


// ////////////////////////////////////////////////////////////////////////// //
static CC25519_INLINE vwadwr_uint get_u32 (const void *src) {
  vwadwr_uint res = ((const vwadwr_ubyte *)src)[0];
  res |= ((vwadwr_uint)((const vwadwr_ubyte *)src)[1])<<8;
  res |= ((vwadwr_uint)((const vwadwr_ubyte *)src)[2])<<16;
  res |= ((vwadwr_uint)((const vwadwr_ubyte *)src)[3])<<24;
  return res;
}

static CC25519_INLINE void put_u64 (void *dest, vwadwr_uint64 u) {
  ((vwadwr_ubyte *)dest)[0] = (vwadwr_ubyte)u;
  ((vwadwr_ubyte *)dest)[1] = (vwadwr_ubyte)(u>>8);
  ((vwadwr_ubyte *)dest)[2] = (vwadwr_ubyte)(u>>16);
  ((vwadwr_ubyte *)dest)[3] = (vwadwr_ubyte)(u>>24);
  ((vwadwr_ubyte *)dest)[4] = (vwadwr_ubyte)(u>>32);
  ((vwadwr_ubyte *)dest)[5] = (vwadwr_ubyte)(u>>40);
  ((vwadwr_ubyte *)dest)[6] = (vwadwr_ubyte)(u>>48);
  ((vwadwr_ubyte *)dest)[7] = (vwadwr_ubyte)(u>>56);
}

static CC25519_INLINE void put_u32 (void *dest, vwadwr_uint u) {
  ((vwadwr_ubyte *)dest)[0] = (vwadwr_ubyte)u;
  ((vwadwr_ubyte *)dest)[1] = (vwadwr_ubyte)(u>>8);
  ((vwadwr_ubyte *)dest)[2] = (vwadwr_ubyte)(u>>16);
  ((vwadwr_ubyte *)dest)[3] = (vwadwr_ubyte)(u>>24);
}

static CC25519_INLINE void put_u16 (void *dest, vwadwr_ushort u) {
  ((vwadwr_ubyte *)dest)[0] = (vwadwr_ubyte)u;
  ((vwadwr_ubyte *)dest)[1] = (vwadwr_ubyte)(u>>8);
}


// ////////////////////////////////////////////////////////////////////////// //
typedef vwadwr_ubyte ed25519_signature[64];
typedef vwadwr_ubyte ed25519_public_key[32];
typedef vwadwr_ubyte ed25519_secret_key[32];
typedef vwadwr_ubyte ed25519_random_seed[32];


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
#define ED25519_SIGNATURE_SIZE (64)


struct sha512_state {
  vwadwr_uint64 h[8];
};

#define SHA512_BLOCK_SIZE  (128)
#define SHA512_HASH_SIZE  (64)

static const struct sha512_state sha512_initial_state = { {
  0x6a09e667f3bcc908LL, 0xbb67ae8584caa73bLL,
  0x3c6ef372fe94f82bLL, 0xa54ff53a5f1d36f1LL,
  0x510e527fade682d1LL, 0x9b05688c2b3e6c1fLL,
  0x1f83d9abfb41bd6bLL, 0x5be0cd19137e2179LL,
} };

static const vwadwr_uint64 round_k[80] = {
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

static CC25519_INLINE vwadwr_uint64 load64 (const vwadwr_ubyte *x) {
  vwadwr_uint64 r = *(x++);
  r = (r << 8) | *(x++);
  r = (r << 8) | *(x++);
  r = (r << 8) | *(x++);
  r = (r << 8) | *(x++);
  r = (r << 8) | *(x++);
  r = (r << 8) | *(x++);
  r = (r << 8) | *(x++);

  return r;
}

static CC25519_INLINE void store64 (vwadwr_ubyte *x, vwadwr_uint64 v) {
  x += 7; *(x--) = v;
  v >>= 8; *(x--) = v;
  v >>= 8; *(x--) = v;
  v >>= 8; *(x--) = v;
  v >>= 8; *(x--) = v;
  v >>= 8; *(x--) = v;
  v >>= 8; *(x--) = v;
  v >>= 8; *(x--) = v;
}

static CC25519_INLINE vwadwr_uint64 rot64 (vwadwr_uint64 x, int bits) {
  return (x >> bits) | (x << (64 - bits));
}

static void sha512_block (struct sha512_state *s, const vwadwr_ubyte *blk) {
  vwadwr_uint64 w[16];
  vwadwr_uint64 a, b, c, d, e, f, g, h;
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
    const vwadwr_uint64 wi = w[i & 15];
    const vwadwr_uint64 wi15 = w[(i + 1) & 15];
    const vwadwr_uint64 wi2 = w[(i + 14) & 15];
    const vwadwr_uint64 wi7 = w[(i + 9) & 15];
    const vwadwr_uint64 s0 = rot64(wi15, 1) ^ rot64(wi15, 8) ^ (wi15 >> 7);
    const vwadwr_uint64 s1 = rot64(wi2, 19) ^ rot64(wi2, 61) ^ (wi2 >> 6);

    const vwadwr_uint64 S0 = rot64(a, 28) ^ rot64(a, 34) ^ rot64(a, 39);
    const vwadwr_uint64 S1 = rot64(e, 14) ^ rot64(e, 18) ^ rot64(e, 41);
    const vwadwr_uint64 ch = (e & f) ^ ((~e) & g);
    const vwadwr_uint64 temp1 = h + S1 + ch + round_k[i] + wi;
    const vwadwr_uint64 maj = (a & b) ^ (a & c) ^ (b & c);
    const vwadwr_uint64 temp2 = S0 + maj;

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

static void sha512_final (struct sha512_state *s, const vwadwr_ubyte *blk, vwadwr_uint total_size) {
  vwadwr_ubyte temp[SHA512_BLOCK_SIZE] = {0};
  const vwadwr_uint last_size = total_size & (SHA512_BLOCK_SIZE - 1);

  if (last_size) memcpy(temp, blk, last_size);
  temp[last_size] = 0x80;

  if (last_size > 111) {
    sha512_block(s, temp);
    memset(temp, 0, sizeof(temp));
  }

  store64(temp + SHA512_BLOCK_SIZE - 8, total_size << 3);
  sha512_block(s, temp);
}

static void sha512_get (const struct sha512_state *s, vwadwr_ubyte *hash,
                        vwadwr_uint offset, vwadwr_uint len)
{
  int i;

  if (offset > SHA512_BLOCK_SIZE) return;

  if (len > SHA512_BLOCK_SIZE - offset) len = SHA512_BLOCK_SIZE - offset;

  i = offset >> 3;
  offset &= 7;

  if (offset) {
    vwadwr_ubyte tmp[8];
    vwadwr_uint c = 8 - offset;

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
    vwadwr_ubyte tmp[8];

    store64(tmp, s->h[i]);
    memcpy(hash, tmp, len);
  }
}

#define F25519_SIZE  (32)

static CC25519_INLINE void f25519_copy (vwadwr_ubyte *x, const vwadwr_ubyte *a) {
  memcpy(x, a, F25519_SIZE);
}

#define FPRIME_SIZE  (32)

static CC25519_INLINE void fprime_copy (vwadwr_ubyte *x, const vwadwr_ubyte *a) {
  memcpy(x, a, FPRIME_SIZE);
}

struct ed25519_pt {
  vwadwr_ubyte x[F25519_SIZE];
  vwadwr_ubyte y[F25519_SIZE];
  vwadwr_ubyte t[F25519_SIZE];
  vwadwr_ubyte z[F25519_SIZE];
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

static CC25519_INLINE void ed25519_prepare (vwadwr_ubyte *e) {
  e[0] &= 0xf8;
  e[31] &= 0x7f;
  e[31] |= 0x40;
}

static CC25519_INLINE void ed25519_copy (struct ed25519_pt *dst, const struct ed25519_pt *src) {
  memcpy(dst, src, sizeof(*dst));
}

#define EDSIGN_SECRET_KEY_SIZE  (32)
#define EDSIGN_PUBLIC_KEY_SIZE  (32)
#define EDSIGN_SIGNATURE_SIZE   (64)

static void f25519_select (vwadwr_ubyte *dst, const vwadwr_ubyte *zero, const vwadwr_ubyte *one,
                           vwadwr_ubyte condition)
{
  const vwadwr_ubyte mask = -condition;
  int i;

  for (i = 0; i < F25519_SIZE; i++) {
    dst[i] = zero[i] ^ (mask & (one[i] ^ zero[i]));
  }
}

static void f25519_normalize (vwadwr_ubyte *x) {
  vwadwr_ubyte minusp[F25519_SIZE];
  vwadwr_ushort c;
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

  c += ((vwadwr_ushort)x[i]) - 128;
  minusp[31] = c;

  f25519_select(x, minusp, x, (c >> 15) & 1);
}

static void f25519_add (vwadwr_ubyte *r, const vwadwr_ubyte *a, const vwadwr_ubyte *b) {
  vwadwr_ushort c = 0;
  int i;

  for (i = 0; i < F25519_SIZE; i++) {
    c >>= 8;
    c += ((vwadwr_ushort)a[i]) + ((vwadwr_ushort)b[i]);
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

static void f25519_sub (vwadwr_ubyte *r, const vwadwr_ubyte *a, const vwadwr_ubyte *b) {
  vwadwr_uint c = 0;
  int i;

  c = 218;
  for (i = 0; i + 1 < F25519_SIZE; i++) {
    c += 65280 + ((vwadwr_uint)a[i]) - ((vwadwr_uint)b[i]);
    r[i] = c;
    c >>= 8;
  }

  c += ((vwadwr_uint)a[31]) - ((vwadwr_uint)b[31]);
  r[31] = c & 127;
  c = (c >> 7) * 19;

  for (i = 0; i < F25519_SIZE; i++) {
    c += r[i];
    r[i] = c;
    c >>= 8;
  }
}

static void f25519_neg (vwadwr_ubyte *r, const vwadwr_ubyte *a) {
  vwadwr_uint c = 0;
  int i;

  c = 218;
  for (i = 0; i + 1 < F25519_SIZE; i++) {
    c += 65280 - ((vwadwr_uint)a[i]);
    r[i] = c;
    c >>= 8;
  }

  c -= ((vwadwr_uint)a[31]);
  r[31] = c & 127;
  c = (c >> 7) * 19;

  for (i = 0; i < F25519_SIZE; i++) {
    c += r[i];
    r[i] = c;
    c >>= 8;
  }
}

static void f25519_mul__distinct (vwadwr_ubyte *r, const vwadwr_ubyte *a, const vwadwr_ubyte *b) {
  vwadwr_uint c = 0;
  int i;

  for (i = 0; i < F25519_SIZE; i++) {
    int j;

    c >>= 8;
    for (j = 0; j <= i; j++) {
      c += ((vwadwr_uint)a[j]) * ((vwadwr_uint)b[i - j]);
    }

    for (; j < F25519_SIZE; j++) {
      c += ((vwadwr_uint)a[j]) *
           ((vwadwr_uint)b[i + F25519_SIZE - j]) * 38;
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

static void f25519_inv__distinct (vwadwr_ubyte *r, const vwadwr_ubyte *x) {
  vwadwr_ubyte s[F25519_SIZE];
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

static void raw_add (vwadwr_ubyte *x, const vwadwr_ubyte *p) {
  vwadwr_ushort c = 0;
  int i;

  for (i = 0; i < FPRIME_SIZE; i++) {
    c += ((vwadwr_ushort)x[i]) + ((vwadwr_ushort)p[i]);
    x[i] = c;
    c >>= 8;
  }
}

static void fprime_select (vwadwr_ubyte *dst, const vwadwr_ubyte *zero, const vwadwr_ubyte *one,
                           vwadwr_ubyte condition)
{
  const vwadwr_ubyte mask = -condition;
  int i;

  for (i = 0; i < FPRIME_SIZE; i++) {
    dst[i] = zero[i] ^ (mask & (one[i] ^ zero[i]));
  }
}

static void raw_try_sub (vwadwr_ubyte *x, const vwadwr_ubyte *p) {
  vwadwr_ubyte minusp[FPRIME_SIZE];
  vwadwr_ushort c = 0;
  int i;

  for (i = 0; i < FPRIME_SIZE; i++) {
    c = ((vwadwr_ushort)x[i]) - ((vwadwr_ushort)p[i]) - c;
    minusp[i] = c;
    c = (c >> 8) & 1;
  }

  fprime_select(x, minusp, x, c);
}

static int prime_msb (const vwadwr_ubyte *p) {
  int i;
  vwadwr_ubyte x;

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

static void shift_n_bits (vwadwr_ubyte *x, int n) {
  vwadwr_ushort c = 0;
  int i;

  for (i = 0; i < FPRIME_SIZE; i++) {
    c |= ((vwadwr_ushort)x[i]) << n;
    x[i] = c;
    c >>= 8;
  }
}

static CC25519_INLINE int min_int (int a, int b) {
  return a < b ? a : b;
}

static void fprime_from_bytes (vwadwr_ubyte *n, const vwadwr_ubyte *x, vwadwr_uint len,
                               const vwadwr_ubyte *modulus)
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
    const vwadwr_ubyte bit = (x[i >> 3] >> (i & 7)) & 1;

    shift_n_bits(n, 1);
    n[0] |= bit;
    raw_try_sub(n, modulus);
  }
}

static CC25519_INLINE void fprime_add (vwadwr_ubyte *r, const vwadwr_ubyte *a, const vwadwr_ubyte *modulus) {
  raw_add(r, a);
  raw_try_sub(r, modulus);
}

static void fprime_mul (vwadwr_ubyte *r, const vwadwr_ubyte *a, const vwadwr_ubyte *b, const vwadwr_ubyte *modulus) {
  int i;

  memset(r, 0, FPRIME_SIZE);

  for (i = prime_msb(modulus); i >= 0; i--) {
    const vwadwr_ubyte bit = (b[i >> 3] >> (i & 7)) & 1;
    vwadwr_ubyte plusa[FPRIME_SIZE];

    shift_n_bits(r, 1);
    raw_try_sub(r, modulus);

    fprime_copy(plusa, r);
    fprime_add(plusa, a, modulus);

    fprime_select(r, r, plusa, bit);
  }
}

static CC25519_INLINE void ed25519_unproject (vwadwr_ubyte *x, vwadwr_ubyte *y, const struct ed25519_pt *p) {
  vwadwr_ubyte z1[F25519_SIZE];

  f25519_inv__distinct(z1, p->z);
  f25519_mul__distinct(x, p->x, z1);
  f25519_mul__distinct(y, p->y, z1);

  f25519_normalize(x);
  f25519_normalize(y);
}

static CC25519_INLINE void ed25519_pack (vwadwr_ubyte *c, const vwadwr_ubyte *x, const vwadwr_ubyte *y) {
  vwadwr_ubyte tmp[F25519_SIZE];
  vwadwr_ubyte parity;

  f25519_copy(tmp, x);
  f25519_normalize(tmp);
  parity = (tmp[0] & 1) << 7;

  f25519_copy(c, y);
  f25519_normalize(c);
  c[31] |= parity;
}

static const vwadwr_ubyte ed25519_k[F25519_SIZE] = {
  0x59, 0xf1, 0xb2, 0x26, 0x94, 0x9b, 0xd6, 0xeb,
  0x56, 0xb1, 0x83, 0x82, 0x9a, 0x14, 0xe0, 0x00,
  0x30, 0xd1, 0xf3, 0xee, 0xf2, 0x80, 0x8e, 0x19,
  0xe7, 0xfc, 0xdf, 0x56, 0xdc, 0xd9, 0x06, 0x24
};

static void ed25519_add (struct ed25519_pt *r,
                         const struct ed25519_pt *p1, const struct ed25519_pt *p2)
{
  vwadwr_ubyte a[F25519_SIZE];
  vwadwr_ubyte b[F25519_SIZE];
  vwadwr_ubyte c[F25519_SIZE];
  vwadwr_ubyte d[F25519_SIZE];
  vwadwr_ubyte e[F25519_SIZE];
  vwadwr_ubyte f[F25519_SIZE];
  vwadwr_ubyte g[F25519_SIZE];
  vwadwr_ubyte h[F25519_SIZE];

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
  vwadwr_ubyte a[F25519_SIZE];
  vwadwr_ubyte b[F25519_SIZE];
  vwadwr_ubyte c[F25519_SIZE];
  vwadwr_ubyte e[F25519_SIZE];
  vwadwr_ubyte f[F25519_SIZE];
  vwadwr_ubyte g[F25519_SIZE];
  vwadwr_ubyte h[F25519_SIZE];

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
                           const vwadwr_ubyte *e)
{
  struct ed25519_pt r;
  int i;

  ed25519_copy(&r, &ed25519_neutral);

  for (i = 255; i >= 0; i--) {
    const vwadwr_ubyte bit = (e[i >> 3] >> (i & 7)) & 1;
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

static const vwadwr_ubyte ed25519_order[FPRIME_SIZE] = {
  0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
  0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10
};

static CC25519_INLINE void expand_key (vwadwr_ubyte *expanded, const vwadwr_ubyte *secret) {
  struct sha512_state s;

  sha512_init(&s);
  sha512_final(&s, secret, EDSIGN_SECRET_KEY_SIZE);
  sha512_get(&s, expanded, 0, EXPANDED_SIZE);
  ed25519_prepare(expanded);
}

static CC25519_INLINE void pp (vwadwr_ubyte *packed, const struct ed25519_pt *p) {
  vwadwr_ubyte x[F25519_SIZE];
  vwadwr_ubyte y[F25519_SIZE];

  ed25519_unproject(x, y, p);
  ed25519_pack(packed, x, y);
}

static CC25519_INLINE void sm_pack (vwadwr_ubyte *r, const vwadwr_ubyte *k) {
  struct ed25519_pt p;

  ed25519_smult(&p, &ed25519_base, k);
  pp(r, &p);
}

static CC25519_INLINE void edsign_sec_to_pub (vwadwr_ubyte *pub, const vwadwr_ubyte *secret) {
  vwadwr_ubyte expanded[EXPANDED_SIZE];

  expand_key(expanded, secret);
  sm_pack(pub, expanded);
}

static int hash_with_prefix (vwadwr_ubyte *out_fp,
                             vwadwr_ubyte *init_block, vwadwr_uint prefix_size,
                             cc_ed25519_iostream *strm)
{
  struct sha512_state s;

  const int xxlen = strm->total_size(strm);
  if (xxlen < 0) return -1;
  const vwadwr_uint len = (vwadwr_uint)xxlen;

  vwadwr_ubyte msgblock[SHA512_BLOCK_SIZE];

  sha512_init(&s);

  if (len < SHA512_BLOCK_SIZE && len + prefix_size < SHA512_BLOCK_SIZE) {
    if (len > 0) {
      if (strm->read(strm, 0, msgblock, (int)len) != 0) return -1;
      memcpy(init_block + prefix_size, msgblock, len);
    }
    sha512_final(&s, init_block, len + prefix_size);
  } else {
    vwadwr_uint i;

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
    if (left > 0) {
      if (strm->read(strm, (int)i, msgblock, left) != 0) return -1;
    }

    sha512_final(&s, msgblock, len + prefix_size);
  }

  sha512_get(&s, init_block, 0, SHA512_HASH_SIZE);
  fprime_from_bytes(out_fp, init_block, SHA512_HASH_SIZE, ed25519_order);

  return 0;
}

static CC25519_INLINE int generate_k (vwadwr_ubyte *k, const vwadwr_ubyte *kgen_key,
                                      cc_ed25519_iostream *strm)
{
  vwadwr_ubyte block[SHA512_BLOCK_SIZE];
  memcpy(block, kgen_key, 32);
  return hash_with_prefix(k, block, 32, strm);
}

static int hash_message (vwadwr_ubyte *z, const vwadwr_ubyte *r, const vwadwr_ubyte *a,
                         cc_ed25519_iostream *strm)
{
  vwadwr_ubyte block[SHA512_BLOCK_SIZE];

  memcpy(block, r, 32);
  memcpy(block + 32, a, 32);
  return hash_with_prefix(z, block, 64, strm);
}

static int edsign_sign_stream (vwadwr_ubyte *signature, const vwadwr_ubyte *pub, const vwadwr_ubyte *secret,
                               cc_ed25519_iostream *strm)
{
  vwadwr_ubyte expanded[EXPANDED_SIZE];
  vwadwr_ubyte e[FPRIME_SIZE];
  vwadwr_ubyte s[FPRIME_SIZE];
  vwadwr_ubyte k[FPRIME_SIZE];
  vwadwr_ubyte z[FPRIME_SIZE];

  expand_key(expanded, secret);

  if (generate_k(k, expanded + 32, strm) != 0) return -1;
  sm_pack(signature, k);

  if (hash_message(z, signature, pub, strm) != 0) return -1;

  fprime_from_bytes(e, expanded, 32, ed25519_order);

  fprime_mul(s, z, e, ed25519_order);
  fprime_add(s, k, ed25519_order);
  memcpy(signature + 32, s, 32);

  return 0;
}


// ////////////////////////////////////////////////////////////////////////// //
static CC25519_INLINE vwadwr_uint hashU32 (vwadwr_uint v) {
  v ^= v >> 16;
  v *= 0x21f0aaadu;
  v ^= v >> 15;
  v *= 0x735a2d97u;
  v ^= v >> 15;
  return v;
}

static vwadwr_uint deriveSeed (vwadwr_uint seed, const vwadwr_ubyte *buf, vwadwr_uint buflen) {
  for (vwadwr_uint f = 0; f < buflen; f += 1) {
    seed = hashU32((seed + 0x9E3779B9u) ^ buf[f]);
  }
  // hash it again
  return hashU32(seed + 0x9E3779B9u);
}

static void crypt_buffer (vwadwr_uint xseed, vwadwr_uint64 nonce, void *buf, vwadwr_uint bufsize) {
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
  vwadwr_uint rval;

  vwadwr_uint *b32 = (vwadwr_uint *)buf;
  while (bufsize >= 4) {
    MB32X;
    put_u32(b32, get_u32(b32) ^ rval);
    ++b32;
    bufsize -= 4;
  }
  if (bufsize) {
    // final [1..3] bytes
    MB32X;
    vwadwr_uint n;
    vwadwr_ubyte *b = (vwadwr_ubyte *)b32;
    switch (bufsize) {
      case 3:
        n = b[0]|((vwadwr_uint)b[1]<<8)|((vwadwr_uint)b[2]<<16);
        n ^= rval;
        b[0] = (vwadwr_ubyte)n;
        b[1] = (vwadwr_ubyte)(n>>8);
        b[2] = (vwadwr_ubyte)(n>>16);
        break;
      case 2:
        n = b[0]|((vwadwr_uint)b[1]<<8);
        n ^= rval;
        b[0] = (vwadwr_ubyte)n;
        b[1] = (vwadwr_ubyte)(n>>8);
        break;
      case 1:
        b[0] ^= rval;
        break;
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
#define crc32_init  (0xffffffffU)

static const vwadwr_uint crctab[16] = {
  0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
  0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
  0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
  0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c,
};


#define CRC32BYTE(bt_)  do { \
  crc32 ^= (vwadwr_ubyte)(bt_); \
  crc32 = crctab[crc32&0x0f]^(crc32>>4); \
  crc32 = crctab[crc32&0x0f]^(crc32>>4); \
} while (0)


static CC25519_INLINE vwadwr_uint crc32_part (vwadwr_uint crc32, const void *src, vwadwr_uint len) {
  const vwadwr_ubyte *buf = (const vwadwr_ubyte *)src;
  while (len--) {
    CRC32BYTE(*buf++);
  }
  return crc32;
}

static CC25519_INLINE vwadwr_uint crc32_final (vwadwr_uint crc32) {
  return crc32^0xffffffffU;
}

static CC25519_INLINE vwadwr_uint crc32_buf (const void *src, vwadwr_uint len) {
  return crc32_final(crc32_part(crc32_init, src, len));
}


// ////////////////////////////////////////////////////////////////////////// //
// Z85 codec
static const char *z85_enc_alphabet =
  "0123456789abcdefghijklmnopqrstuvwxyzABCDEFG"
  "HIJKLMNOPQRSTUVWXYZ.-:+=^!/*?&<>()[]{}@%$#";

static const vwadwr_ubyte z85_dec_alphabet [96] = {
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


void vwadwr_z85_encode_key (const vwadwr_public_key inkey, vwadwr_z85_key enkey) {
  vwadwr_ubyte sdata[32 + 4];
  memcpy(sdata, inkey, 32);
  const vwadwr_uint crc32 = crc32_buf(sdata, 32);
  put_u32(&sdata[32], crc32);
  vwadwr_uint dpos = 0, spos = 0, value = 0;
  while (spos < 32 + 4) {
    value = value * 256u + sdata[spos++];
    if (spos % 4u == 0) {
      vwadwr_uint divisor = 85 * 85 * 85 * 85;
      while (divisor) {
        char ech = z85_enc_alphabet[value / divisor % 85u];
        if (ech == '/') ech = '~';
        enkey[dpos++] = ech;
        divisor /= 85u;
      }
      value = 0;
    }
  }
  if (dpos != (vwadwr_uint)sizeof(vwadwr_z85_key) - 1u) vwad__builtin_trap();
  enkey[dpos] = 0;
}


vwadwr_result vwadwr_z85_decode_key (const vwadwr_z85_key enkey, vwadwr_public_key outkey) {
  if (enkey == NULL || outkey == NULL) return VADWR_ERR_ARGS;
  vwadwr_ubyte ddata[32 + 4];
  vwadwr_uint dpos = 0, spos = 0, value = 0;
  while (spos < (vwadwr_uint)sizeof(vwadwr_z85_key) - 1) {
    char inch = enkey[spos++];
    switch (inch) {
      case 0: return VADWR_ERR_BAD_ASCII;
      case '\\': inch = '/'; break;
      case '~': inch = '/'; break;
      case '|': inch = '!'; break;
      case ',': inch = '.'; break;
      case ';': inch = ':'; break;
      default: break;
    }
    if (!strchr(z85_enc_alphabet, inch)) return VADWR_ERR_BAD_ASCII;
    value = value * 85 + z85_dec_alphabet[(vwadwr_ubyte)inch - 32];
    if (spos % 5u == 0) {
      vwadwr_uint divisor = 256 * 256 * 256;
      while (divisor) {
        ddata[dpos++] = value / divisor % 256u;
        divisor /= 256u;
      }
      value = 0;
    }
  }
  if (dpos != 32 + 4) vwad__builtin_trap();
  if (enkey[spos] != 0) return VADWR_ERR_BAD_ASCII;
  const vwadwr_uint crc32 = crc32_buf(ddata, 32);
  if (crc32 != get_u32(&ddata[32])) return VADWR_ERR_BAD_ASCII; // bad checksum
  memcpy(outkey, ddata, 32);
  return VWADWR_OK;
}


// ////////////////////////////////////////////////////////////////////////// //
// logging; can be NULL
// `type` is the above enum
void (*vwadwr_logf) (int type, const char *fmt, ...);

#define logf(type_,...)  do { \
  if (vwadwr_logf) { \
    vwadwr_logf(VWADWR_LOG_ ## type_, __VA_ARGS__); \
  } \
} while (0)


// ////////////////////////////////////////////////////////////////////////// //
void (*vwadwr_assertion_failed) (const char *fmt, ...) = NULL;

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

#ifdef _MSC_VER
#define vassert(cond_)  do { if (vwad__builtin_expect((!(cond_)), 0)) { const int line__assf = __LINE__; \
    if (vwadwr_assertion_failed) { \
      vwadwr_assertion_failed("%s:%d: Assertion in `%s` failed: %s\n", \
        SkipPathPartCStr(__FILE__), line__assf, __FUNCSIG__, #cond_); \
      vwad__builtin_trap(); /* just in case */ \
    } else { \
      vwad__builtin_trap(); \
    } \
  } \
} while (0)
#else
#define vassert(cond_)  do { if (vwad__builtin_expect((!(cond_)), 0)) { const int line__assf = __LINE__; \
    if (vwadwr_assertion_failed) { \
      vwadwr_assertion_failed("%s:%d: Assertion in `%s` failed: %s\n", \
        SkipPathPartCStr(__FILE__), line__assf, __PRETTY_FUNCTION__, #cond_); \
      vwad__builtin_trap(); /* just in case */ \
    } else { \
      vwad__builtin_trap(); \
    } \
  } \
} while (0)
#endif


// ////////////////////////////////////////////////////////////////////////// //
// memory allocation
static CC25519_INLINE void *xalloc (vwadwr_memman *mman, vwadwr_uint size) {
  vassert(size > 0 && size <= 0x7ffffff0u);
  if (mman != NULL) return mman->alloc(mman, (vwadwr_uint)size); else return malloc(size);
}


static CC25519_INLINE void *zalloc (vwadwr_memman *mman, vwadwr_uint size) {
  vassert(size > 0 && size <= 0x7ffffff0u);
  void *p = (mman != NULL ? mman->alloc(mman, (vwadwr_uint)size) : malloc(size));
  if (p) memset(p, 0, size);
  return p;
}


static CC25519_INLINE void xfree (vwadwr_memman *mman, void *p) {
  if (p != NULL) {
    if (mman != NULL) mman->free(mman, p); else free(p);
  }
}


// ////////////////////////////////////////////////////////////////////////// //
typedef int intbool_t;

enum {
  int_false = 0,
  int_true = 1
};


// ////////////////////////////////////////////////////////////////////////// //
typedef struct {
  vwadwr_uint x1, x2;
  vwadwr_ubyte *dest;
  int dpos, dend;
} EntEncoder;

typedef struct {
  vwadwr_uint x1, x2, x;
  const vwadwr_ubyte *src;
  int spos, send;
} EntDecoder;

typedef struct {
  vwadwr_ushort p1, p2;
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
static void EncInit (EntEncoder *ee, void *outbuf, vwadwr_uint obsize) {
  ee->x1 = 0; ee->x2 = 0xFFFFFFFFU;
  ee->dest = (vwadwr_ubyte *)outbuf; ee->dpos = 0;
  ee->dend = obsize;
}

static CC25519_INLINE void EncEncode (EntEncoder *ee, int p, intbool_t bit) {
  vwadwr_uint xmid = ee->x1 + (vwadwr_uint)(((vwadwr_uint64)((ee->x2 - ee->x1) & 0xffffffffU) * (vwadwr_uint)p) >> 17);
  if (bit) ee->x2 = xmid; else ee->x1 = xmid + 1;
  while ((ee->x1 ^ ee->x2) < (1u << 24)) {
    if (ee->dpos < ee->dend) {
      ee->dest[ee->dpos++] = (vwadwr_ubyte)(ee->x2 >> 24);
    } else {
      ee->dpos = 0x7fffffff - 8;
    }
    ee->x1 <<= 8;
    ee->x2 = (ee->x2 << 8) + 255;
  }
}

static void EncFlush (EntEncoder *ee) {
  if (ee->dpos + 4 <= ee->dend) {
    ee->dest[ee->dpos++] = (vwadwr_ubyte)(ee->x2 >> 24); ee->x2 <<= 8;
    ee->dest[ee->dpos++] = (vwadwr_ubyte)(ee->x2 >> 24); ee->x2 <<= 8;
    ee->dest[ee->dpos++] = (vwadwr_ubyte)(ee->x2 >> 24); ee->x2 <<= 8;
    ee->dest[ee->dpos++] = (vwadwr_ubyte)(ee->x2 >> 24); ee->x2 <<= 8;
  } else {
    ee->dpos = 0x7fffffff - 8;
  }
}


// ////////////////////////////////////////////////////////////////////////// //
#ifdef VWAD_COMPILE_DECOMPRESSOR
static VWAD_OKUNUSED CC25519_INLINE vwadwr_ubyte DecGetByte (EntDecoder *ee) {
  if (ee->spos < ee->send) {
    return ee->src[ee->spos++];
  } else {
    ee->spos = 0x7fffffff;
    return 0;
  }
}

static VWAD_OKUNUSED void DecInit (EntDecoder *ee, const void *inbuf, vwadwr_uint insize) {
  ee->x1 = 0; ee->x2 = 0xFFFFFFFFU;
  ee->src = (const vwadwr_ubyte *)inbuf; ee->spos = 0; ee->send = insize;
  ee->x = DecGetByte(ee);
  ee->x = (ee->x << 8) + DecGetByte(ee);
  ee->x = (ee->x << 8) + DecGetByte(ee);
  ee->x = (ee->x << 8) + DecGetByte(ee);
}

static VWAD_OKUNUSED CC25519_INLINE intbool_t DecDecode (EntDecoder *ee, int p) {
  vwadwr_uint xmid = ee->x1 + (vwadwr_uint)(((vwadwr_uint64)((ee->x2 - ee->x1) & 0xffffffffU) * (vwadwr_uint)p) >> 17);
  intbool_t bit = (ee->x <= xmid);
  if (bit) ee->x2 = xmid; else ee->x1 = xmid + 1;
  while ((ee->x1 ^ ee->x2) < (1u << 24)) {
    ee->x1 <<= 8;
    ee->x2 = (ee->x2 << 8) + 255;
    ee->x = (ee->x << 8) + DecGetByte(ee);
  }
  return bit;
}
#endif


// ////////////////////////////////////////////////////////////////////////// //
static void PredInit (Predictor *pp) {
  pp->p1 = 1 << 15; pp->p2 = 1 << 15;
}

static CC25519_INLINE int PredGetP (Predictor *pp) {
  return (int)((vwadwr_uint)pp->p1 + (vwadwr_uint)pp->p2);
}

static CC25519_INLINE void PredUpdate (Predictor *pp, intbool_t bit) {
  if (bit) {
    pp->p1 += ((~((vwadwr_uint)pp->p1)) >> 3) & 0b0001111111111111;
    pp->p2 += ((~((vwadwr_uint)pp->p2)) >> 6) & 0b0000001111111111;
  } else {
    pp->p1 -= ((vwadwr_uint)pp->p1) >> 3;
    pp->p2 -= ((vwadwr_uint)pp->p2) >> 6;
  }
}

static CC25519_INLINE int PredGetPAndUpdate (Predictor *pp, intbool_t bit) {
  int p = PredGetP(pp);
  PredUpdate(pp, bit);
  return p;
}

#ifdef VWAD_COMPILE_DECOMPRESSOR
static VWAD_OKUNUSED CC25519_INLINE intbool_t PredGetBitDecodeUpdate (Predictor *pp, EntDecoder *dec) {
  int p = PredGetP(pp);
  intbool_t bit = DecDecode(dec, p);
  PredUpdate(pp, bit);
  return bit;
}
#endif


// ////////////////////////////////////////////////////////////////////////// //
static void BitPPMInit (BitPPM *ppm, int initstate) {
  for (vwadwr_uint f = 0; f < (vwadwr_uint)sizeof(ppm->pred) / (vwadwr_uint)sizeof(ppm->pred[0]); ++f) {
    PredInit(&ppm->pred[f]);
  }
  ppm->ctx = !!initstate;
}

static CC25519_INLINE void BitPPMEncode (BitPPM *ppm, EntEncoder *enc, intbool_t bit) {
  int p = ppm->ctx;
  if (bit) ppm->ctx = 1; else ppm->ctx = 0;
  p = PredGetPAndUpdate(&ppm->pred[p], bit);
  EncEncode(enc, p, bit);
}

#ifdef VWAD_COMPILE_DECOMPRESSOR
static VWAD_OKUNUSED CC25519_INLINE intbool_t BitPPMDecode (BitPPM *ppm, EntDecoder *dec) {
  intbool_t bit = PredGetBitDecodeUpdate(&ppm->pred[ppm->ctx], dec);
  if (bit) ppm->ctx = 1; else ppm->ctx = 0;
  return bit;
}
#endif


// ////////////////////////////////////////////////////////////////////////// //
static void BytePPMInit (BytePPM *ppm) {
  for (vwadwr_uint f = 0; f < (vwadwr_uint)sizeof(ppm->predBits) / (vwadwr_uint)sizeof(ppm->predBits[0]); ++f) {
    PredInit(&ppm->predBits[f]);
  }
  ppm->ctxBits = 0;
}

static CC25519_INLINE void BytePPMEncodeByte (BytePPM *ppm, EntEncoder *enc, int bt) {
  int c2 = 1;
  int cofs = ppm->ctxBits * 256;
  ppm->ctxBits = (bt >= 0x80);
  for (int f = 0; f <= 7; ++f) {
    intbool_t bit = (bt&0x80) != 0; bt <<= 1;
    int p = PredGetPAndUpdate(&ppm->predBits[cofs + c2], bit);
    EncEncode(enc, p, bit);
    c2 += c2; if (bit) ++c2;
  }
}

#ifdef VWAD_COMPILE_DECOMPRESSOR
static VWAD_OKUNUSED CC25519_INLINE vwadwr_ubyte BytePPMDecodeByte (BytePPM *ppm, EntDecoder *dec) {
  vwadwr_uint n = 1;
  int cofs = ppm->ctxBits * 256;
  do {
    intbool_t bit = PredGetBitDecodeUpdate(&ppm->predBits[cofs + n], dec);
    n += n; if (bit) ++n;
  } while (n < 0x100);
  n -= 0x100;
  ppm->ctxBits = (n >= 0x80);
  return (vwadwr_ubyte)n;
}
#endif


// ////////////////////////////////////////////////////////////////////////// //
static void WordPPMInit (WordPPM *ppm) {
  BytePPMInit(&ppm->bytes[0]); BytePPMInit(&ppm->bytes[1]);
  BitPPMInit(&ppm->moreFlag, 0);
}

static CC25519_INLINE void WordPPMEncodeInt (WordPPM *ppm, EntEncoder *enc, int n) {
  BytePPMEncodeByte(&ppm->bytes[0], enc, n&0xff);
  if (n >= 0x100) {
    BitPPMEncode(&ppm->moreFlag, enc, 1);
    BytePPMEncodeByte(&ppm->bytes[1], enc, (n>>8)&0xff);
  } else {
    BitPPMEncode(&ppm->moreFlag, enc, 0);
  }
}


#ifdef VWAD_COMPILE_DECOMPRESSOR
static VWAD_OKUNUSED CC25519_INLINE int WordPPMDecodeInt (WordPPM *ppm, EntDecoder *dec) {
  int n = BytePPMDecodeByte(&ppm->bytes[0], dec);
  if (BitPPMDecode(&ppm->moreFlag, dec)) {
    n += BytePPMDecodeByte(&ppm->bytes[1], dec) * 0x100;
  }
  return n;
}
#endif


// ////////////////////////////////////////////////////////////////////////// //
//# define LZFF_HASH_SIZE   (1021u)
#define LZFF_HASH_SIZE    (2039u)
//# define LZFF_HASH_SIZE   (4093u)
#define LZFF_NUM_LIMIT    (0x10000)
#define LZFF_OFS_LIMIT    LZFF_NUM_LIMIT
#define LZFF_NUM_BUCKETS  (LZFF_OFS_LIMIT * 2)

typedef struct {
  vwadwr_uint sptr;
  vwadwr_uint bytes4;
  vwadwr_uint prev;
} LZFFHashEntry;


//==========================================================================
//
//  CompressLZFF3
//
//==========================================================================
static int CompressLZFF3 (vwadwr_memman *mman, const void *source, int srclen,
                          void *dest, int dstlen,
                          vwadwr_bool allow_lazy)
{
  vwadwr_uint bestofs, bestlen, he, b4, lp, wr;
  vwadwr_uint mmax, mlen, cpp, pos;
  vwadwr_uint mtbestofs, mtbestlen;
  vwadwr_uint ssizemax;
  vwadwr_uint srcsize;
  vwadwr_uint litcount, litpos, spos;
  vwadwr_uint hfree, heidx, ntidx;
  vwadwr_uint dict[LZFF_HASH_SIZE];
  const vwadwr_ubyte *src;
  EntEncoder enc;
  BytePPM ppmData;
  WordPPM ppmMtOfs, ppmMtLen, ppmLitLen;
  BitPPM ppmLitFlag;
  LZFFHashEntry *htbl;

  #define FlushLit()  do { \
    lp = litpos; \
    while (litcount != 0) { \
      BitPPMEncode(&ppmLitFlag, &enc, int_true); \
      wr = litcount; if (wr > LZFF_NUM_LIMIT) wr = LZFF_NUM_LIMIT; \
      litcount -= wr; \
      WordPPMEncodeInt(&ppmLitLen, &enc, wr - 1); \
      while (wr != 0) { \
        BytePPMEncodeByte(&ppmData, &enc, src[lp]); \
        lp += 1; wr -= 1; \
      } \
    } \
  } while (0)

  // repopulate hash buckets; this helps with huge files
  #define Rehash()  do { \
    memset(dict, -1, sizeof(dict)); \
    hfree = 0; \
    pos = (spos > LZFF_OFS_LIMIT + 1 ? spos - LZFF_OFS_LIMIT - 1 : 0); \
    if (pos >= spos) vwad__builtin_trap(); \
    b4 = \
      (vwadwr_uint)src[pos] + \
      ((vwadwr_uint)src[pos + 1] << 8) + \
      ((vwadwr_uint)src[pos + 2] << 16) + \
      ((vwadwr_uint)src[pos + 3] << 24); \
    do { \
      heidx = (b4 * 0x9E3779B1u) % LZFF_HASH_SIZE; \
      he = dict[heidx]; \
      ntidx = hfree++; \
      htbl[ntidx].sptr = pos; \
      htbl[ntidx].bytes4 = b4; \
      htbl[ntidx].prev = he; \
      dict[heidx] = ntidx; \
      ++pos; \
      /* update bytes */ \
      b4 = (b4 >> 8) + ((vwadwr_uint)src[pos + 3] << 24); \
    } while (pos != spos); \
  } while (0)

  #define AddHashAt()  do { \
    if (hfree == LZFF_NUM_BUCKETS) Rehash(); \
    b4 = \
      src[spos] + \
      (src[spos + 1] << 8) + \
      (src[spos + 2] << 16) + \
      (src[spos + 3] << 24); \
    heidx = (b4 * 0x9E3779B1u) % LZFF_HASH_SIZE; \
    he = dict[heidx]; \
    ntidx = hfree++; \
    htbl[ntidx].sptr = spos; \
    htbl[ntidx].bytes4 = b4; \
    htbl[ntidx].prev = he; \
    dict[heidx] = ntidx; \
    /*return he;*/ \
  } while (0)

  //void FindMatch (uint32_t *pbestofs, uint32_t *pbestlen)
  #define FindMatch(pbestofs, pbestlen)  do { \
    mtbestofs = 0; mtbestlen = 3; /* we want matches of at least 4 bytes */ \
    ssizemax = srcsize - spos; \
    AddHashAt(); \
    while (he != ~0) { \
      cpp = htbl[he].sptr; \
      if (spos - cpp > LZFF_OFS_LIMIT) he = ~0; \
      else { \
        mmax = srcsize - spos; \
        if (ssizemax < mmax) mmax = ssizemax; \
        if (LZFF_OFS_LIMIT < mmax) mmax = LZFF_OFS_LIMIT; \
        if (mmax > mtbestlen && htbl[he].bytes4 == b4) { \
          /* fast reject based on the last byte */ \
          if (mtbestlen == 3 || src[spos + mtbestlen] == src[cpp + mtbestlen]) { \
            /* yet another fast reject */ \
            if (mtbestlen <= 4 || \
                (src[spos + (mtbestlen>>1)] == src[cpp + (mtbestlen>>1)] && \
                 (mtbestlen < 8 || src[spos + mtbestlen - 1] == src[cpp + mtbestlen - 1]))) \
            { \
              mlen = 4; \
              while (mlen < mmax && src[spos + mlen] == src[cpp + mlen]) ++mlen; \
              if (mlen > mtbestlen) { \
                mtbestofs = spos - cpp; \
                mtbestlen = mlen; \
              } \
            } \
          } \
        } \
        he = htbl[he].prev; \
      } \
    } \
    pbestofs = mtbestofs; pbestlen = mtbestlen; \
  } while (0)


  if (srclen < 1 || srclen > 0x3fffffff) return VADWR_ERR_ARGS;
  if (dstlen < 1 || dstlen > 0x3fffffff) return VADWR_ERR_ARGS;

  src = (const vwadwr_ubyte *)source;
  srcsize = (vwadwr_uint)srclen;

  memset(dict, -1, sizeof(dict));
  htbl = xalloc(mman, (vwadwr_uint)sizeof(htbl[0]) * LZFF_NUM_BUCKETS);
  if (htbl == NULL) return VWADWR_ERR_MEM;
  hfree = 0;

  BytePPMInit(&ppmData);
  WordPPMInit(&ppmMtOfs);
  WordPPMInit(&ppmMtLen);
  WordPPMInit(&ppmLitLen);
  BitPPMInit(&ppmLitFlag, 1);

  EncInit(&enc, dest, (vwadwr_uint)dstlen);

  litpos = 0;
  if (srcsize <= 6) {
    // don't even bother trying
    litcount = srcsize;
  } else {
    // first byte is always a literal
    litcount = 1;
    spos = 1;
    while (spos < srcsize - 3) {
      FindMatch(bestofs, bestlen);
      //ProgReport();
      if (bestofs == 0) {
        if (litcount == 0) litpos = spos;
        ++litcount;
        ++spos;
      } else {
        // try match with the next char
        vwadwr_uint xdiff;
        if (allow_lazy && spos < srcsize - 4) {
          xdiff = 2;
          int doagain;
          do {
            vwadwr_uint bestofs1, bestlen1;
            spos += 1; FindMatch(bestofs1, bestlen1);
            if (bestlen1 >= bestlen + 2) {
              if (litcount == 0) litpos = spos - 1;
              ++litcount;
              bestofs = bestofs1; bestlen = bestlen1;
              doagain = (spos != srcsize - 3);
            } else {
              doagain = 0;
            }
          } while (doagain);
        } else {
          xdiff = 1;
        }
        if (litcount) FlushLit();
        BitPPMEncode(&ppmLitFlag, &enc, int_false);
        WordPPMEncodeInt(&ppmMtLen, &enc, bestlen - 3);
        WordPPMEncodeInt(&ppmMtOfs, &enc, bestofs - 1);
        spos += 1; bestlen -= xdiff;
        if (spos + bestlen < srcsize - 3) {
          while (bestlen != 0) {
            bestlen -= 1;
            AddHashAt();
            spos += 1;
          }
        } else {
          spos += bestlen;
        }
        if (enc.dpos >= 0x7fff0000) spos = srcsize;
      }
    }
    // last bytes as literals
    if (litcount == 0) litpos = spos;
    litcount += srcsize - spos;
  }

  if (enc.dpos < 0x7fff0000) FlushLit();
  EncFlush(&enc);

  xfree(mman, htbl);

  return (enc.dpos < 0x7fff0000 ? enc.dpos : -1);
}


//==========================================================================
//
//  CompressLZFF3LitOnly
//
//==========================================================================
static int CompressLZFF3LitOnly (const void *source, int srclen, void *dest, int dstlen) {
  int srcsize;
  int litcount;
  const vwadwr_ubyte *src;
  EntEncoder enc;
  BytePPM ppmData;
  WordPPM ppmLitLen;
  BitPPM ppmLitFlag;

  if (srclen < 1 || srclen > 0x3fffffff) return VADWR_ERR_ARGS;
  if (dstlen < 1 || dstlen > 0x3fffffff) return VADWR_ERR_ARGS;

  src = (const vwadwr_ubyte *)source;
  srcsize = (vwadwr_uint)srclen;

  BytePPMInit(&ppmData);
  WordPPMInit(&ppmLitLen);
  BitPPMInit(&ppmLitFlag, 1);

  EncInit(&enc, dest, (vwadwr_uint)dstlen);

  litcount = srcsize;

  int lp = 0;
  while (litcount != 0) {
    BitPPMEncode(&ppmLitFlag, &enc, int_true);
    int wr = litcount; if (wr > LZFF_NUM_LIMIT) wr = LZFF_NUM_LIMIT;
    litcount -= wr;
    WordPPMEncodeInt(&ppmLitLen, &enc, wr - 1);
    while (wr != 0) {
      BytePPMEncodeByte(&ppmData, &enc, src[lp]);
      lp += 1; wr -= 1;
      if ((wr&0x3ff) == 0 && enc.dpos >= 0x7fff0000) { litcount = 0; wr = 0; }
    }
  }

  EncFlush(&enc);

  return (enc.dpos < 0x7fff0000 ? enc.dpos : -1);
}


//==========================================================================
//
//  DecompressLZFF3
//
//==========================================================================
#ifdef VWAD_COMPILE_DECOMPRESSOR
static VWAD_OKUNUSED intbool_t DecompressLZFF3 (const void *src, int srclen,
                                                void *dest, int unpsize)
{
  intbool_t error;
  EntDecoder dec;
  BytePPM ppmData;
  WordPPM ppmMtOfs, ppmMtLen, ppmLitLen;
  BitPPM ppmLitFlag;
  int litcount, n;
  vwadwr_uint dictpos, spos;

  #define PutByte(bt_)  do { \
    if (unpsize != 0) { \
      ((vwadwr_ubyte *)dest)[dictpos++] = (vwadwr_ubyte)(bt_); unpsize -= 1; \
    } else { \
      error = int_true; \
    } \
  } while (0)

  if (srclen < 1 || srclen > 0x3fffffff) return 0;
  if (unpsize < 1 || unpsize > 0x3fffffff) return 0;

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
    vwadwr_ubyte sch;
    int len, ofs;

    litcount = WordPPMDecodeInt(&ppmLitLen, &dec) + 1;
    while (!error && litcount != 0) {
      litcount -= 1;
      n = BytePPMDecodeByte(&ppmData, &dec);
      PutByte((vwadwr_ubyte)n);
      error = error || (dec.spos > dec.send);
    }

    while (!error && unpsize != 0) {
      if (BitPPMDecode(&ppmLitFlag, &dec)) {
        litcount = WordPPMDecodeInt(&ppmLitLen, &dec) + 1;
        while (!error && litcount != 0) {
          litcount -= 1;
          n = BytePPMDecodeByte(&ppmData, &dec);
          PutByte((vwadwr_ubyte)n);
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
            sch = ((const vwadwr_ubyte *)dest)[spos];
            spos++;
            PutByte(sch);
          }
        }
      }
    }
  }

  return (!error && unpsize == 0);
}
#endif


// ////////////////////////////////////////////////////////////////////////// //
static vwadwr_result ioseek (vwadwr_iostream *strm, int pos) {
  FILE *fl = (FILE *)strm->udata;
  if (fseek(fl, pos, SEEK_SET) == 0) return 0;
  return -1;
}

static int iotell (vwadwr_iostream *strm) {
  FILE *fl = (FILE *)strm->udata;
  const long pos = ftell(fl);
  if (pos < 0 || pos >= 0x7ffffff0) return -1;
  return (int)pos;
}

static int ioread (vwadwr_iostream *strm, void *buf, int bufsize) {
  FILE *fl = (FILE *)strm->udata;
  return (int)fread(buf, 1, bufsize, fl);
}

static vwadwr_result iowrite (vwadwr_iostream *strm, const void *buf, int bufsize) {
  FILE *fl = (FILE *)strm->udata;
  if (fwrite(buf, bufsize, 1, fl) != 1) return -1;
  return 0;
}


//==========================================================================
//
//  vwadwr_new_file_stream
//
//==========================================================================
vwadwr_iostream *vwadwr_new_file_stream (FILE *fl) {
  vassert(fl != NULL);
  vwadwr_iostream *res = calloc(1, sizeof(vwadwr_iostream));
  res->seek = ioseek;
  res->tell = iotell;
  res->read = ioread;
  res->write = iowrite;
  res->udata = fl;
  return res;
}


//==========================================================================
//
//  vwadwr_close_file_stream
//
//==========================================================================
vwadwr_result vwadwr_close_file_stream (vwadwr_iostream *strm) {
  vwadwr_result res = 0;
  if (strm) {
    if (strm->udata) {
      if (fclose((FILE *)strm->udata) != 0) res = -1;
    }
    free(strm);
  }
  return res;
}


//==========================================================================
//
//  vwadwr_free_file_stream
//
//==========================================================================
void vwadwr_free_file_stream (vwadwr_iostream *strm) {
  if (strm) {
    free(strm);
  }
}


// ////////////////////////////////////////////////////////////////////////// //
vwadwr_uint vwadwr_crc32_init (void) { return crc32_init; }
vwadwr_uint vwadwr_crc32_part (vwadwr_uint crc32, const void *src, vwadwr_uint len) { return crc32_part(crc32, src, len); }
vwadwr_uint vwadwr_crc32_final (vwadwr_uint crc32) { return crc32_final(crc32); }


// ////////////////////////////////////////////////////////////////////////// //
/* is the given codepoint considered printable?
i restrict it to some useful subset.
unifuck is unifucked, but i hope that i sorted out all idiotic diactritics and control chars. */
static CC25519_INLINE vwadwr_bool is_uni_printable (vwadwr_ushort ch) {
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
static CC25519_INLINE vwadwr_uint utf_char_len (const void *str) {
  const vwadwr_ubyte ch = *((const vwadwr_ubyte *)str);
  if (ch < 0x80) return 1;
  else if ((ch&0x0E0) == 0x0C0) return (ch != 0x0C0 && ch != 0x0C1 ? 2 : 0);
  else if ((ch&0x0F0) == 0x0E0) return 3;
  else if ((ch&0x0F8) == 0x0F0) return 4;
  else return 0;
}


static CC25519_INLINE vwadwr_ushort utf_decode (const char **strp) {
  const vwadwr_ubyte *bp = (const vwadwr_ubyte *)*strp;
  vwadwr_ushort res = (vwadwr_ushort)utf_char_len(bp);
  vwadwr_ubyte ch = *bp;
  if (res < 1 || res > 3) {
    res = VWADWR_REPLACEMENT_CHAR;
    *strp += 1;
  } else if (ch < 0x80) {
    if (ch == 0x7f) res = VWADWR_REPLACEMENT_CHAR; else res = ch;
    *strp += 1;
  } else if ((ch&0x0E0) == 0x0C0) {
    if (ch == 0x0C0 || ch == 0x0C1) {
      res = VWADWR_REPLACEMENT_CHAR;
      *strp += 1;
    } else {
      res = ch - 0x0C0;
      ch = bp[1];
      if ((ch&0x0C0) != 0x80) {
        res = VWADWR_REPLACEMENT_CHAR;
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
      res = VWADWR_REPLACEMENT_CHAR;
      *strp += 1;
    } else {
      res = res * 64 + ch - 128;
      ch = bp[2];
      if ((ch&0x0C0) != 0x80) {
        res = VWADWR_REPLACEMENT_CHAR;
        *strp += 1;
      } else {
        res = res * 64 + ch - 128;
        *strp += 3;
      }
    }
  } else {
    res = VWADWR_REPLACEMENT_CHAR;
  }
  if (res && !is_uni_printable(res)) res = VWADWR_REPLACEMENT_CHAR;
  return res;
}


static CC25519_INLINE vwadwr_ushort unilower (vwadwr_ushort ch) {
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
//  vwadwr_utf_char_len
//
//==========================================================================
vwadwr_uint vwadwr_utf_char_len (const void *str) {
  return (str ? utf_char_len(str) : 0);
}


//==========================================================================
//
//  vwadwr_is_uni_printable
//
//==========================================================================
vwadwr_bool vwadwr_is_uni_printable (vwadwr_ushort ch) {
  return is_uni_printable(ch);
}


//==========================================================================
//
//  vwadwr_utf_decode
//
//  advances `strp` at least by one byte
//
//==========================================================================
vwadwr_ushort vwadwr_utf_decode (const char **strp) {
  return utf_decode(strp);
}


//==========================================================================
//
//  vwadwr_uni_tolower
//
//==========================================================================
vwadwr_ushort vwadwr_uni_tolower (vwadwr_ushort ch) {
  return unilower(ch);
}


// ////////////////////////////////////////////////////////////////////////// //
static vwadwr_uint joaatHashStrCI (const char *key) {
  #define JOAAT_MIX(b_)  do { \
    hash += (vwadwr_ubyte)(b_); \
    hash += hash<<10; \
    hash ^= hash>>6; \
  } while (0)

  vwadwr_uint hash = 0x29a;
  vwadwr_uint len = 0;
  while (*key) {
    vwadwr_ushort ch = unilower(utf_decode(&key));
    JOAAT_MIX(ch);
    if (ch >= 0x100) JOAAT_MIX(ch>>8);
    ++len;
  }
  // mix length (it should not be greater than 255)
  JOAAT_MIX(len);
  // finalize
  hash += hash<<3;
  hash ^= hash>>11;
  hash += hash<<15;
  return hash;
}

#define hashStrCI joaatHashStrCI


// ////////////////////////////////////////////////////////////////////////// //
static vwadwr_bool strEquCI (const char *s0, const char *s1) {
  if (!s0 || !s1) return 0;
  vwadwr_ushort c0 = unilower(utf_decode(&s0));
  vwadwr_ushort c1 = unilower(utf_decode(&s1));
  while (c0 != 0 && c1 != 0&& c0 == c1) {
    if (c0 == VWADWR_REPLACEMENT_CHAR || c1 == VWADWR_REPLACEMENT_CHAR) return 0;
    c0 = unilower(utf_decode(&s0));
    c1 = unilower(utf_decode(&s1));
  }
  return (c0 == 0 && c1 == 0);
}


//==========================================================================
//
//  is_path_delim
//
//==========================================================================
static int is_path_delim (char ch) {
  #ifdef WIN32
  return (ch == '\\' || ch == '/');
  #else
  return (ch == '/');
  #endif
}


//==========================================================================
//
//  normalize_name
//
//==========================================================================
static char *normalize_name (vwadwr_memman *mman, const char *s) {
  if (s == NULL) return NULL;
  for (;;) {
    if (is_path_delim(s[0])) s += 1;
    else if (s[0] == '.' && is_path_delim(s[1])) s += 2;
    else break;
  }
  if (!s[0]) return NULL;
  if (s[0] == '.' && s[1] == 0) return NULL;
  if (s[0] == '.' && s[1] == '.' && (s[2] == 0 || is_path_delim(s[2]))) return NULL;
  for (const char *t = s; *t; ++t) {
    if (is_path_delim(*t)) {
      if (t[1] == '.' && t[2] == '.' && (t[3] == 0 || is_path_delim(t[3]))) return NULL;
    } else if (*t == 0x7f || (*t > 0 && *t < 32)) {
      return NULL;
    }
  }
  vwadwr_uint rlen = 0;
  while (rlen <= 4096 && s[rlen] != 0) ++rlen;
  if (rlen == 0 || rlen > 4096 || is_path_delim(s[rlen - 1])) return NULL;
  char *res = zalloc(mman, rlen + 1);
  if (res == NULL) return NULL;
  vwadwr_uint dpos = 0;
  for (vwadwr_uint f = 0; f < rlen; f += 1) {
    char ch = s[f];
    #ifdef WIN32
    if (ch == '\\') ch = '/';
    #endif
    if (ch == '/') {
      if (dpos == 0 || res[dpos - 1] == '/') continue;
    }
    res[dpos++] = ch;
  }
  if (dpos == 0 || res[dpos - 1] == '/') { xfree(mman, res); return NULL; }
  vassert(dpos <= rlen);
  res[dpos] = 0;
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
#define HASH_BUCKETS  (1024)

typedef struct ChunkInfo_t ChunkInfo;
struct ChunkInfo_t {
  vwadwr_ushort pksize;  // packed chunk size (0 means "unpacked")
  ChunkInfo *next;
};


typedef struct GroupName_t GroupName;
struct GroupName_t {
  vwadwr_uint gnameofs; // !0: this group already registered
  char *name;
  GroupName *next;
};

typedef struct FileName_t FileName;
struct FileName_t {
  vwadwr_uint nameofs;
  char *name;
};

typedef struct FileInfo_t FileInfo;
struct FileInfo_t {
  vwadwr_uint upksize;    // unpacked file size (current or final)
  vwadwr_uint pksize;     // unpacked file size (current or final)
  vwadwr_uint chunkCount; // chunk count
  vwadwr_uint nhash;      // name hash
  vwadwr_uint crc32;      // full crc32
  vwadwr_uint64 ftime;      // file time since Epoch
  FileInfo *bknext;    // next name in bucket
  FileName *fname;
  GroupName *group; // points to some struct in dir
  FileInfo *next;
};


vwad_push_pack
typedef struct vwad_packed_struct {
  vwadwr_uint crc32;
  vwadwr_ushort version;
  vwadwr_ushort flags;
  vwadwr_uint dirofs;
  vwadwr_ushort u_cmt_size;
  vwadwr_ushort p_cmt_size;
  vwadwr_uint cmt_crc32;
} MainFileHeader;
vwad_pop_pack


// writing bytes
#define WR_MODE_WRITE  (0)
// copying raw chunks
#define WR_MODE_RAW    (1)

struct vwadwr_dir_t {
  vwadwr_memman *mman;
  vwadwr_iostream *outstrm;
  ed25519_secret_key privkey;
  vwadwr_bool has_privkey;
  ed25519_public_key pubkey; // this is also chacha20 key
  MainFileHeader mhdr;
  ChunkInfo *chunksHead;
  ChunkInfo *chunksTail;
  GroupName *groupNames;
  vwadwr_uint xorRndSeed;
  vwadwr_uint xorRndSeedPK;
  vwadwr_uint chunkCount; // number of used elements in `chunks` array
  // files (0 is unused)
  FileInfo *filesHead;
  FileInfo *filesTail;
  vwadwr_uint fileCount;
  vwadwr_uint namesSize;
  char author[256];
  char title[256];
  // file writing API
  FileInfo *wrfile; // non-NULL if we are writing file right now
  vwadwr_uint wrpos; // position in `wrchunk`
  // compression level, decoded
  int allow_litonly;
  int allow_lazy;
  int allow_lz;
  int wrmode;
  char wrchunk[65536]; // current buffer
  // two chunk comression buffers (data and CRC32)
  char pkbuf0[65536 + 4];
  char pkbuf1[65536 + 4];
  // directory hash table
  FileInfo *buckets[HASH_BUCKETS];
};


//==========================================================================
//
//  is_valid_file_name
//
//==========================================================================
static vwadwr_bool is_valid_file_name (const char *str) {
  if (!str || !str[0] || str[0] == '/') return 0;
  vwadwr_uint slen = 0;
  while (slen <= 255 && str[slen] != 0) slen += 1;
  if (slen > 255) return 0; // too long
  if (str[slen - 1] == '/') return 0; // should not end with a slash
  // check chars
  vwadwr_ushort ch;
  do { ch = utf_decode(&str); } while (ch >= 32 && ch != VWADWR_REPLACEMENT_CHAR);
  return (ch == 0);
}


//==========================================================================
//
//  is_valid_string
//
//==========================================================================
static vwadwr_bool is_valid_string (const char *cmt, int maxlen, vwadwr_bool oneline) {
  vwadwr_bool res = 1;
  if (cmt != NULL) {
    const char *cmtstart = cmt;
    while (res) {
      const vwadwr_ushort ch = utf_decode(&cmt);
      if ((size_t)(cmt - cmtstart) > (size_t)maxlen + 1) res = 0;
      else if (ch == 0) break;
      else if (ch == VWADWR_REPLACEMENT_CHAR) res = 0;
      else if (oneline) {
        if (ch < 32) res = 0;
      } else {
        if (ch < 32 && ch != 9 && ch != 10) res = 0;
      }
    }
  }
  return res;
}


//==========================================================================
//
//  vwadwr_free_dir
//
//==========================================================================
void vwadwr_free_dir (vwadwr_dir **dirp) {
  if (dirp) {
    vwadwr_dir *dir = *dirp;
    if (dir) {
      *dirp = NULL;
      vwadwr_memman *mman = dir->mman;
      //xfree(mman, dir->comment);
      ChunkInfo *ci = dir->chunksHead;
      while (ci != NULL) {
        ChunkInfo *cx = ci; ci = ci->next;
        xfree(mman, cx);
      }
      FileInfo *fi = dir->filesHead;
      while (fi != NULL) {
        FileInfo *fx = fi; fi = fi->next;
        xfree(mman, fx->fname->name);
        xfree(mman, fx->fname);
        xfree(mman, fx);
      }
      GroupName *gi = dir->groupNames;
      while (gi != NULL) {
        GroupName *fx = gi; gi = gi->next;
        xfree(mman, fx->name);
        xfree(mman, fx);
      }
      memset(dir, 0, sizeof(vwadwr_dir));
      xfree(mman, dir);
    }
  }
}


//==========================================================================
//
//  check_privkey
//
//==========================================================================
static vwadwr_bool check_privkey (const vwadwr_secret_key privkey) {
  int zcount = 0, ocount = 0;
  for (vwadwr_uint f = 0; f < (vwadwr_uint)sizeof(vwadwr_secret_key); f += 1) {
    switch (privkey[f]) {
      case 0: ++zcount; break;
      case 1: ++ocount; break;
    }
  }
  if (zcount > 2 || ocount > 2) return 0;
  for (vwadwr_uint f = 0; f < (vwadwr_uint)sizeof(vwadwr_secret_key) - 1u; f += 1) {
    const vwadwr_ubyte v = privkey[f];
    int count = 0;
    for (vwadwr_uint c = f + 1u; c < (vwadwr_uint)sizeof(vwadwr_secret_key); c += 1) {
      if (privkey[c] == v) {
        if (++count > 3) return 0;
      }
    }
  }
  return 1;
}


//==========================================================================
//
//  vwadwr_is_good_privkey
//
//==========================================================================
vwadwr_bool vwadwr_is_good_privkey (const vwadwr_secret_key privkey) {
  return (privkey != NULL && check_privkey(privkey));
}


//==========================================================================
//
//  vwadwr_new_dir
//
//==========================================================================
vwadwr_dir *vwadwr_new_dir (vwadwr_memman *mman, vwadwr_iostream *outstrm,
                            const char *author, /* can be NULL */
                            const char *title, /* can be NULL */
                            const char *comment, /* can be NULL */
                            vwadwr_uint flags,
                            const vwadwr_secret_key privkey,
                            vwadwr_public_key respubkey, /* OUT; can be NULL */
                            int *error) /* OUT; can be NULL */
{
  vwadwr_dir *res;
  int xerr = 0;
  if (error == NULL) error = &xerr; // to avoid endless checks
  if (respubkey) memset(respubkey, 0, sizeof(ed25519_public_key));
  if (outstrm == NULL) { *error = VWADWR_ERR_OTHER; return NULL; }
  if (privkey == NULL) { *error = VWADWR_ERR_PRIVKEY; return NULL; }
  if (!check_privkey(privkey)) { *error = VWADWR_ERR_PRIVKEY; return NULL; }
  if (!is_valid_string(author, 127, 1)) { *error = VWADWR_ERR_AUTHOR; return NULL; }
  if (!is_valid_string(title, 127, 1)) { *error = VWADWR_ERR_TITLE; return NULL; }
  if (!is_valid_string(comment, 65535, 0)) { *error = VWADWR_ERR_COMMENT; return NULL; }
  if ((flags & ~VWADWR_NEW_DONT_SIGN) != 0) { *error = VWADWR_ERR_FLAGS; return NULL; }
  *error = VWADWR_ERR_OTHER;

  res = zalloc(mman, (vwadwr_uint)sizeof(vwadwr_dir));
  if (res == NULL) return NULL;

  res->mman = mman;
  res->outstrm = outstrm;
  res->namesSize = 4; // first string is always empty
  res->fileCount = 0;
  res->wrfile = NULL;

  // public key
  logf(NOTE, "generating public key");
  ed25519_public_key pubkey;
  edsign_sec_to_pub(pubkey, privkey);
  if (respubkey) memcpy(respubkey, pubkey, sizeof(ed25519_public_key));
  memcpy(res->pubkey, pubkey, 32);
  memcpy(res->privkey, privkey, 32);

  if ((flags & VWADWR_NEW_DONT_SIGN) == 0) {
    res->has_privkey = 1;
  } else {
    res->has_privkey = 0;
  }

  // write header
  if (outstrm->seek(outstrm, 0) != 0) {
    logf(ERROR, "cannot seek to start");
    goto error;
  }
  if (outstrm->write(outstrm, "VWAD", 4) != 0) {
    logf(ERROR, "cannot write sign");
    goto error;
  }

  // signature is random bytes for now
  ed25519_signature edsign;
  memset(edsign, 0, sizeof(ed25519_signature));
  // fill dsign with giberish
  crypt_buffer(deriveSeed(0xa28, res->pubkey, (vwadwr_uint)sizeof(ed25519_secret_key)),
               0x29b, edsign, (vwadwr_uint)sizeof(ed25519_signature));
  if (outstrm->write(outstrm, edsign, (int)sizeof(ed25519_signature)) != 0) {
    logf(ERROR, "cannot write edsign");
    goto error;
  }

  // encrypt public key
  ed25519_public_key epk;
  memcpy(epk, res->pubkey, sizeof(ed25519_public_key));
  crypt_buffer(deriveSeed(0xa29, edsign, (vwadwr_uint)sizeof(ed25519_signature)),
               0x29a, epk, (vwadwr_uint)sizeof(ed25519_public_key));

  // write public key
  if (outstrm->write(outstrm, epk, (int)sizeof(ed25519_public_key)) != 0) {
    logf(ERROR, "cannot write public key");
    goto error;
  }

  // write author
  vwadwr_ubyte asslen = (author != NULL ? (vwadwr_ubyte)strlen(author) : 0u);
  if (asslen) memcpy(res->author, author, asslen); res->author[asslen] = 0;

  vwadwr_ubyte tsslen = (title != NULL ? (vwadwr_ubyte)strlen(title) : 0u);
  if (tsslen) memcpy(res->title, title, tsslen); res->title[tsslen] = 0;

  // lengthes
  if (outstrm->write(outstrm, &asslen, 1) != 0) {
    logf(ERROR, "cannot write author length");
    goto error;
  }
  if (outstrm->write(outstrm, &tsslen, 1) != 0) {
    logf(ERROR, "cannot write title length");
    goto error;
  }

  const char *newln = "\x0d\x0a\x1b";

  // write author
  if (outstrm->write(outstrm, newln, 2) != 0) {
    logf(ERROR, "cannot write author padding");
    goto error;
  }
  if (asslen != 0 && outstrm->write(outstrm, author, (int)asslen) != 0) {
    logf(ERROR, "cannot write author text");
    goto error;
  }
  // write title
  if (outstrm->write(outstrm, newln, 2) != 0) {
    logf(ERROR, "cannot write title padding");
    goto error;
  }
  if (tsslen != 0 && outstrm->write(outstrm, title, (int)tsslen) != 0) {
    logf(ERROR, "cannot write title text");
    goto error;
  }
  // final
  if (outstrm->write(outstrm, newln, 4) != 0) {
    logf(ERROR, "cannot write final padding");
    goto error;
  }

  // create initial seed from pubkey, author, and title
  res->xorRndSeed = deriveSeed(0x29c, res->pubkey, (vwadwr_uint)sizeof(ed25519_public_key));
  res->xorRndSeed = deriveSeed(res->xorRndSeed, (const vwadwr_ubyte *)author, (int)asslen);
  res->xorRndSeed = deriveSeed(res->xorRndSeed, (const vwadwr_ubyte *)title, (int)tsslen);
  // remember it
  res->xorRndSeedPK = res->xorRndSeed;

  // now create header fields
  put_u32(&res->mhdr.crc32, 0);
  put_u16(&res->mhdr.version, 0);
  vwadwr_ushort archflags = (res->has_privkey ? 0x00 : 0x01);
  #ifdef VWAD_USE_NAME_LENGTHES
  archflags |= 0x02;
  #endif
  put_u16(&res->mhdr.flags, archflags);
  // unpacked comment size
  const vwadwr_uint u_csz = (comment ? (vwadwr_uint)strlen(comment) : 0);
  vassert(u_csz < 65556);
  put_u16(&res->mhdr.u_cmt_size, u_csz);
  // dir offset (unknown for now)
  put_u32(&res->mhdr.dirofs, 0);

  // compress and write comment
  if (u_csz) {
    put_u32(&res->mhdr.cmt_crc32, crc32_buf(comment, u_csz));
    vwadwr_ubyte *pkc = xalloc(mman, u_csz);
    if (pkc == NULL) {
      logf(ERROR, "cannot alloc comment buffer");
      goto error;
    }
    const int pksz1 = CompressLZFF3LitOnly(comment, u_csz, pkc, u_csz);
    int pksz0 = CompressLZFF3(mman, comment, u_csz, pkc, u_csz, 1/*allow_lazy*/);
    if (pksz0 == -666) { xfree(mman, pkc); goto error; } // out of memory
    // recompress with "literals only" mode again, if it is better
    #if 1
    if (pksz1 > 0 && pksz1 < (int)u_csz && (pksz0 < 1 || pksz0 >= (int)u_csz || pksz1 < pksz0)) {
      pksz0 = CompressLZFF3LitOnly(comment, u_csz, pkc, u_csz);
    }
    #endif
    if (pksz0 < 1 || pksz0 >= (int)u_csz) {
      // write uncompressed
      logf(NOTE, "comment: cannot pack, write uncompressed");
      put_u16(&res->mhdr.p_cmt_size, 0);
      if (outstrm->write(outstrm, &res->mhdr, (int)sizeof(res->mhdr)) != 0) { xfree(mman, pkc); goto error; }
      memcpy(pkc, comment, u_csz);
      // encrypt comment with pk-seed
      crypt_buffer(res->xorRndSeedPK, 2, pkc, u_csz);
      // mix unpacked comment data
      res->xorRndSeed = deriveSeed(res->xorRndSeed, pkc, u_csz);
      if (outstrm->write(outstrm, pkc, (int)u_csz) != 0) { xfree(mman, pkc); goto error; }
    } else {
      // write compressed
      logf(NOTE, "comment: packed from %u to %d", u_csz, pksz0);
      put_u16(&res->mhdr.p_cmt_size, (vwadwr_ushort)pksz0);
      if (outstrm->write(outstrm, &res->mhdr, (int)sizeof(res->mhdr)) != 0) { xfree(mman, pkc); goto error; }
      // encrypt comment with pk-seed
      crypt_buffer(res->xorRndSeedPK, 2, pkc, (vwadwr_uint)pksz0);
      // mix packed comment data
      res->xorRndSeed = deriveSeed(res->xorRndSeed, pkc, (vwadwr_uint)pksz0);
      if (outstrm->write(outstrm, pkc, pksz0) != 0) { xfree(mman, pkc); goto error; }
    }
  } else {
    // still update the seed
    res->xorRndSeed = deriveSeed(res->xorRndSeed, NULL, 0);
    // write uncompressed nothing
    put_u32(&res->mhdr.cmt_crc32, 0);
    put_u16(&res->mhdr.p_cmt_size, 0);
    if (outstrm->write(outstrm, &res->mhdr, (int)sizeof(res->mhdr)) != 0) goto error;
  }
  #if 0
  logf(DEBUG, "pkseed: 0x%08x", res->xorRndSeedPK);
  logf(DEBUG, "xnseed: 0x%08x", res->xorRndSeed);
  #endif

  *error = 0;
  return res;

error:
  vwadwr_free_dir(&res);
  return NULL;
}


//==========================================================================
//
//  vwadwr_dir_get_memman
//
//==========================================================================
vwadwr_memman *vwadwr_dir_get_memman (vwadwr_dir *dir) {
  return (dir ? dir->mman : NULL);
}


//==========================================================================
//
//  vwadwr_dir_get_outstrm
//
//==========================================================================
vwadwr_iostream *vwadwr_dir_get_outstrm (vwadwr_dir *dir) {
  return (dir ? dir->outstrm : NULL);
}


//==========================================================================
//
//  vwadwr_append_chunk
//
//==========================================================================
static vwadwr_result vwadwr_append_chunk (vwadwr_dir *dir, vwadwr_ushort pksize) {
  if (dir == NULL) return VADWR_ERR_ARGS;
  if (dir->chunkCount >= 0x3fffffff) return VADWR_ERR_CHUNK_COUNT;
  ChunkInfo *ci = zalloc(dir->mman, (vwadwr_uint)sizeof(ChunkInfo));
  if (ci != NULL) {
    ci->pksize = pksize;
    ci->next = NULL;
    if (dir->chunksHead == NULL) {
      vassert(dir->chunkCount == 0);
      dir->chunksHead = ci;
    } else {
      vassert(dir->chunkCount != 0);
      dir->chunksTail->next = ci;
    }
    dir->chunkCount += 1;
    vassert(dir->chunkCount != 0);
    dir->chunksTail = ci;
    return VWADWR_OK;
  } else {
    return VWADWR_ERR_MEM;
  }
}


//==========================================================================
//
//  vwadwr_is_valid_file_name
//
//==========================================================================
vwadwr_bool vwadwr_is_valid_file_name (const char *str) {
  if (!str || !str[0] || str[0] == '/') return 0;
  vwadwr_uint slen = 0;
  while (slen <= 255 && str[slen] != 0) slen += 1;
  if (slen > 255) return 0;
  if (str[slen - 1] == '/') return 0; // should not end with a slash
  return is_valid_file_name(str);
}


//==========================================================================
//
//  vwadwr_is_valid_group_name
//
//==========================================================================
vwadwr_bool vwadwr_is_valid_group_name (const char *str) {
  if (!str) return 1;
  return is_valid_string(str, 255, 1);
}


//==========================================================================
//
//  vwadwr_register_group
//
//==========================================================================
static GroupName *vwadwr_register_group (vwadwr_dir *dir, const char *grpname, int *err) {
  *err = 0;
  vassert(grpname != NULL && grpname[0]);
  GroupName *gi = dir->groupNames;
  while (gi != NULL && !strEquCI(grpname, gi->name)) gi = gi->next;
  if (gi == NULL) {
    const vwadwr_uint slen = (vwadwr_uint)strlen(grpname);
    gi = zalloc(dir->mman, (vwadwr_uint)sizeof(GroupName));
    if (gi == NULL) { *err = -1; return NULL; }
    gi->name = zalloc(dir->mman, slen + 1);
    if (gi->name == NULL) { xfree(dir->mman, gi); *err = -1; return NULL; }
    strcpy(gi->name, grpname);
    gi->gnameofs = 0; // not registered yet
    dir->namesSize += slen + 1;
    // align
    if (dir->namesSize&0x03) dir->namesSize = (dir->namesSize|0x03)+1;
    gi->next = dir->groupNames;
    dir->groupNames = gi;
  }
  return gi;
}


//==========================================================================
//
//  vwadwr_append_file_info
//
//  WARNING! `dir->filesTail` should point to the new file info on exit!
//
//==========================================================================
static vwadwr_result vwadwr_append_file_info (vwadwr_dir *dir,
                                              const char *pkfname, const char *gname,
                                              vwadwr_uint crc32, vwadwr_uint64 ftime)
{
  if (dir == NULL || pkfname == NULL || pkfname[0] == 0) return VADWR_ERR_ARGS;

  char *fname = normalize_name(dir->mman, pkfname);
  if (fname == NULL) return VWADWR_ERR_MEM;

  if (!vwadwr_is_valid_file_name(fname)) {
    logf(ERROR, "bad file name: \"%s\"", fname);
    xfree(dir->mman, fname);
    return VWADWR_ERR_NAME;
  }

  if (dir->fileCount >= 0x00ffffffU) {
    xfree(dir->mman, fname);
    logf(ERROR, "too many files");
    return VADWR_ERR_FILE_COUNT;
  }

  const vwadwr_uint fnlen = (vwadwr_uint)strlen(fname);
  if (fnlen >= 512) {
    logf(ERROR, "file name too long: \"%s\"", fname);
    xfree(dir->mman, fname);
    return VWADWR_ERR_NAME;
  }

  if (dir->namesSize >= 0x3fffffff || 0x3fffffff - dir->namesSize < fnlen + 6 ||
      dir->namesSize < 4 || (dir->namesSize&0x03) != 0)
  {
    xfree(dir->mman, fname);
    logf(ERROR, "name table too big");
    return VADWR_ERR_NAMES_SIZE;
  }

  const vwadwr_uint hash = hashStrCI(fname);
  const vwadwr_uint bkt = hash % HASH_BUCKETS;
  FileInfo *fi = dir->buckets[bkt];
  while (fi != NULL) {
    if (fi->nhash == hash && strEquCI(fi->fname->name, fname)) {
      logf(ERROR, "duplicate file name: \"%s\" and \"%s\"", fname, fi->fname->name);
      xfree(dir->mman, fname);
      return VADWR_ERR_DUP_FILE;
    }
    fi = fi->bknext;
  }

  // create new file info
  fi = zalloc(dir->mman, (vwadwr_uint)sizeof(FileInfo));
  if (fi != NULL) {
    if (gname && gname[0]) {
      int err;
      fi->group = vwadwr_register_group(dir, gname, &err);
      if (err != 0) {
        xfree(dir->mman, fname);
        xfree(dir->mman, fi);
        return VWADWR_ERR_MEM;
      }
    } else {
      fi->group = NULL;
    }

    FileName *nn = zalloc(dir->mman, (vwadwr_uint)sizeof(FileName));
    if (nn == NULL) {
      xfree(dir->mman, fname);
      xfree(dir->mman, fi);
      return VWADWR_ERR_MEM;
    }
    nn->nameofs = 0;
    nn->name = fname;

    fi->upksize = 0;
    fi->pksize = 0;
    fi->chunkCount = 0;
    fi->nhash = hash;
    fi->bknext = dir->buckets[bkt];
    fi->ftime = ftime;
    fi->crc32 = crc32;
    dir->buckets[bkt] = fi;

    //fi->nameofs = dir->namesSize;
    dir->namesSize += fnlen + 1;
    // align
    if (dir->namesSize&0x03) dir->namesSize = (dir->namesSize|0x03)+1;
    fi->fname = nn;
    fi->next = NULL;

    if (dir->filesTail) dir->filesTail->next = fi; else dir->filesHead = fi;
    dir->filesTail = fi;

    ++dir->fileCount;

    return VWADWR_OK;
  } else {
    xfree(dir->mman, fname);
    return VWADWR_ERR_MEM;
  }
}


//==========================================================================
//
//  vwadwr_is_valid_dir
//
//==========================================================================
vwadwr_bool vwadwr_is_valid_dir (const vwadwr_dir *dir) {
  return
    dir != NULL &&
    dir->namesSize >= 8 &&
    (dir->namesSize&0x03) == 0 &&
    /*dir->chunkCount > 0 &&*/ dir->chunkCount <= 0x1fffffffU &&
    dir->fileCount > 0 && dir->fileCount <= 0x00ffffffU;
}


//==========================================================================
//
//  vwadwr_check_dir
//
//==========================================================================
vwadwr_result vwadwr_check_dir (const vwadwr_dir *dir) {
  if (dir == NULL) return VADWR_ERR_ARGS;
  if (dir->namesSize < 8) return VADWR_ERR_NAMES_SIZE;
  if ((dir->namesSize&0x03) != 0) return VADWR_ERR_NAMES_ALIGN;
  if (/*dir->chunkCount == 0 ||*/ dir->chunkCount > 0x1fffffffU) return VADWR_ERR_CHUNK_COUNT;
  if (dir->fileCount == 0 || dir->fileCount > 0x00ffffffU) return VADWR_ERR_FILE_COUNT;
  return VWADWR_OK;
}


//==========================================================================
//
//  vwadwr_write_directory
//
//==========================================================================
static vwadwr_result vwadwr_write_directory (vwadwr_dir *dir, vwadwr_iostream *strm,
                                             const vwadwr_uint dirofs)
{
  if (strm == NULL || strm->write == NULL) return VADWR_ERR_ARGS;
  const int dcheck = vwadwr_check_dir(dir);
  if (dcheck != VWADWR_OK) return dcheck;

  // calculate directory size
  const vwadwr_uint64 dirsz64 = (vwadwr_uint64)dir->namesSize+
                           4+(vwadwr_uint64)dir->fileCount*VWADWR_FILE_ENTRY_SIZE+
                           4+(vwadwr_uint64)dir->chunkCount*VWADWR_CHUNK_ENTRY_SIZE;

  if (dirsz64 > 0x04000000) {
    logf(ERROR, "directory too big");
    return VADWR_ERR_DIR_TOO_BIG;
  }

  const vwadwr_uint dirsz = (vwadwr_uint)dirsz64;

  vwadwr_ubyte *fdir = zalloc(dir->mman, dirsz);
  if (fdir == NULL) {
    return VWADWR_ERR_MEM;
  }

  vwadwr_uint nidx;

  // create names table
  vwadwr_uint namesStart = 4+dir->fileCount*VWADWR_FILE_ENTRY_SIZE +
                           4+dir->chunkCount*VWADWR_CHUNK_ENTRY_SIZE;
  vwadwr_uint fdirofs = 0;
  const vwadwr_uint z32 = 0;
  const vwadwr_ushort z16 = 0;

  // put counters
  put_u32(fdir + fdirofs, dir->chunkCount); fdirofs += 4;
  put_u32(fdir + fdirofs, dir->fileCount); fdirofs += 4;

  // put chunks
  for (ChunkInfo *ci = dir->chunksHead; ci; ci = ci->next) {
    memcpy(fdir + fdirofs, &z32, 4); fdirofs += 4;
    memcpy(fdir + fdirofs, &z16, 2); fdirofs += 2;
    put_u16(fdir + fdirofs, ci->pksize); fdirofs += 2;
  }
  vassert(fdirofs = 4+4 + dir->chunkCount*VWADWR_CHUNK_ENTRY_SIZE);

  // put files, and fill names table
  vwadwr_uint nameOfs = 4; // first string is always empty

  // put group names
  for (FileInfo *fi = dir->filesHead; fi; fi = fi->next) {
    if (fi->group != NULL && fi->group->gnameofs == 0) {
      // store group name
      fi->group->gnameofs = nameOfs;
      strcpy((char *)(fdir + namesStart + nameOfs), fi->group->name);
      nameOfs += (vwadwr_uint)strlen(fi->group->name) + 1;
      // align
      if (nameOfs&0x03) nameOfs = (nameOfs|0x03)+1;
      vassert(nameOfs + namesStart <= dirsz);
    }
  }

  // put file names, and assign offsets
  nidx = 0;
  for (FileInfo *fi = dir->filesHead; fi; fi = fi->next) {
    vassert(nidx != dir->fileCount);
    vassert(fi->fname != NULL);
    vassert(fi->fname->name != NULL);
    // remember offset
    fi->fname->nameofs = nameOfs;
    strcpy((char *)(fdir + namesStart + nameOfs), fi->fname->name);
    nameOfs += (vwadwr_uint)strlen(fi->fname->name) + 1;
    // align
    if (nameOfs&0x03) nameOfs = (nameOfs|0x03)+1;
    vassert(nameOfs + namesStart <= dirsz);
    nidx += 1;
  }
  vassert(nidx == dir->fileCount);
  vassert(nameOfs == dir->namesSize);

  // put file info
  #ifdef VWAD_USE_NAME_LENGTHES
  vwadwr_uint pnofs = 0;
  #endif
  vwadwr_uint ccc = 0;
  for (FileInfo *fi = dir->filesHead; fi; fi = fi->next) {
    vassert(fi->fname->nameofs != 0);
    memcpy(fdir + fdirofs, &z32, 4); fdirofs += 4; // first chunk will be calculated
    memcpy(fdir + fdirofs, &z32, 4); fdirofs += 4;
    memcpy(fdir + fdirofs, &z32, 4); fdirofs += 4;
    if (fi->group != NULL) {
      vassert(fi->group->gnameofs != 0);
      put_u32(fdir + fdirofs, fi->group->gnameofs); fdirofs += 4; // gnameofs
    } else {
      put_u32(fdir + fdirofs, 0); fdirofs += 4; // gnameofs
    }
    put_u64(fdir + fdirofs, fi->ftime); fdirofs += 8;
    put_u32(fdir + fdirofs, fi->crc32); fdirofs += 4;
    put_u32(fdir + fdirofs, fi->upksize); fdirofs += 4;
    put_u32(fdir + fdirofs, fi->chunkCount); fdirofs += 4;
    ccc += fi->chunkCount;
    #ifdef VWAD_USE_NAME_LENGTHES
    if (pnofs == 0) {
      put_u32(fdir + fdirofs, fi->fname->nameofs);
    } else {
      vassert(pnofs < fi->fname->nameofs);
      put_u32(fdir + fdirofs, fi->fname->nameofs - pnofs);
    }
    pnofs = fi->fname->nameofs;
    #else
    put_u32(fdir + fdirofs, fi->fname->nameofs);
    #endif
    fdirofs += 4;
  }
  vassert(fdirofs = 4+dir->chunkCount*VWADWR_CHUNK_ENTRY_SIZE +
                    4+dir->fileCount*VWADWR_FILE_ENTRY_SIZE);
  vassert(fdirofs = namesStart);
  vassert(ccc = dir->chunkCount);

  // names
  //put_u32(fdir + fdirofs, dir->namesSize); fdirofs += 4;
  fdirofs += nameOfs;
  vassert(fdirofs == dirsz);

  // write directory
  vwadwr_uint upk_crc32 = crc32_buf(fdir, dirsz);
  vwadwr_ubyte *pkdir = xalloc(dir->mman, 0xffffff + 1);
  vwadwr_uint pk_crc32;
  int pks = CompressLZFF3(dir->mman, fdir, dirsz, pkdir, 0xffffff, 1/*allow_lazy*/);
  vassert(pks > 0); // the thing that should not be
  vwadwr_ubyte *pkdir1 = xalloc(dir->mman, 0xffffff + 1);
  int pks1 = pks;
  if (pks1 < 1 || pks1 >= 0xffffff) pks1 = 0xffffff;
  pks1 = CompressLZFF3LitOnly(fdir, dirsz, pkdir1, pks1);
  if (pks1 > 0 && (pks < 1 || pks1 < pks)) {
    logf(DEBUG, "dir packer: pks1=%d; pks=%d", pks1, pks);
    xfree(dir->mman, pkdir);
    pkdir = pkdir1;
    pks = pks1;
  } else {
    //printf("!dir packer: pks1=%d; pks=%d\n", pks1, pks);
    xfree(dir->mman, pkdir1);
  }
  logf(NOTE, "dir: packed from %u to %d", dirsz, pks);
  xfree(dir->mman, fdir);


  if (0x7fffffffU - dirofs < (vwadwr_uint)pks) {
    xfree(dir->mman, pkdir);
    logf(ERROR, "directory (%u bytes) is too big", dirsz);
    return VADWR_ERR_VWAD_TOO_BIG;
  }


  vwadwr_ubyte dirheader[4 * 4];
  // packed dir data crc32
  pk_crc32 = crc32_buf(pkdir, (vwadwr_uint)pks);
  put_u32(dirheader + 0, pk_crc32);
  // unpacked dir data crc32
  put_u32(dirheader + 4, upk_crc32);
  // packed dir size
  vwadwr_uint tmp = (vwadwr_uint)pks;
  put_u32(dirheader + 8, tmp);
  // unpacked dir size
  put_u32(dirheader + 12, dirsz);

  // dir header
  crypt_buffer(dir->xorRndSeed, 0xfffffffeU, dirheader, 4 * 4);
  if (strm->write(strm, dirheader, 4 * 4) != 0) {
    xfree(dir->mman, pkdir);
    logf(ERROR, "write error");
    return VADWR_ERR_IO_ERROR;
  }

  // dir data
  crypt_buffer(dir->xorRndSeed, 0xffffffffU, pkdir, (vwadwr_uint)pks);
  if (strm->write(strm, pkdir, pks) != 0) {
    xfree(dir->mman, pkdir);
    logf(ERROR, "write error");
    return VADWR_ERR_IO_ERROR;
  }
  xfree(dir->mman, pkdir);

  return VWADWR_OK;
}


//==========================================================================
//
//  vwadwr_get_last_file_packed_size
//
//==========================================================================
int vwadwr_get_last_file_packed_size (vwadwr_dir *dir) {
  return (dir && dir->filesTail ? (int)dir->filesTail->pksize : 0);
}


//==========================================================================
//
//  vwadwr_get_last_file_unpacked_size
//
//==========================================================================
int vwadwr_get_last_file_unpacked_size (vwadwr_dir *dir) {
  return (dir && dir->filesTail ? (int)dir->filesTail->upksize : 0);
}


//==========================================================================
//
//  vwadwr_get_last_file_chunk_count
//
//==========================================================================
int vwadwr_get_last_file_chunk_count (vwadwr_dir *dir) {
  return (dir && dir->filesTail ? (int)dir->filesTail->chunkCount : 0);
}


//==========================================================================
//
//  vwadwr_create_file
//
//  you can use this API to write files with the usual fwrite-like API.
//  please note that you cannot seek backwards in this case, and only
//  one file can be created for writing.
//
//==========================================================================
vwadwr_result vwadwr_create_file (vwadwr_dir *dir, int level, /* VADWR_COMP_xxx */
                                  const char *pkfname,
                                  const char *groupname, /* can be NULL */
                                  vwadwr_ftime ftime) /* can be 0; seconds since Epoch */
{
  if (dir == NULL || pkfname == NULL) return VADWR_ERR_ARGS;

  if (dir->wrfile != NULL) return VADWR_ERR_FILE_OPEN;

  if (!vwadwr_is_valid_group_name(groupname)) return VWADWR_ERR_GROUP;

  if (!pkfname[0]) return VWADWR_ERR_NAME;

  dir->allow_litonly = 0;
  dir->allow_lazy = 0;
  dir->allow_lz = 0;
  if (level >= 0) {
    switch (level) {
      case VADWR_COMP_FASTEST:
        dir->allow_litonly = 1;
        dir->allow_lazy = 0;
        dir->allow_lz = 0;
        break;
      case VADWR_COMP_FAST:
        dir->allow_litonly = 0;
        dir->allow_lazy = 0;
        dir->allow_lz = 1;
        break;
      case VADWR_COMP_MEDIUM:
        dir->allow_litonly = 0;
        dir->allow_lazy = 1;
        dir->allow_lz = 1;
        break;
      case VADWR_COMP_BEST:
      default:
        dir->allow_litonly = 1;
        dir->allow_lazy = 1;
        dir->allow_lz = 1;
        break;
    }
  }

  char *xname = normalize_name(dir->mman, pkfname);
  if (xname == NULL) return VWADWR_ERR_NAME;

  // we don't know sizes and such yet, so use zeroes
  vwadwr_result rescode = vwadwr_append_file_info(dir, pkfname, groupname, 0, ftime);
  if (rescode != VWADWR_OK) {
    logf(ERROR, "cannot append file info");
    return rescode;
  }

  dir->wrfile = dir->filesTail;
  dir->wrfile->crc32 = crc32_init;

  dir->wrpos = 0;
  dir->wrmode = WR_MODE_WRITE;

  return VWADWR_OK;
}


//==========================================================================
//
//  vwadwr_flush_chunk
//
//==========================================================================
static vwadwr_result vwadwr_flush_chunk (vwadwr_dir *dir) {
  if (dir == NULL || dir->wrfile == NULL) return VADWR_ERR_ARGS;
  vassert(dir->wrpos > 0 && dir->wrpos <= 65536);

  vwadwr_result rescode;
  FileInfo *fi = dir->wrfile;

  if (fi->chunkCount >= 0x3fffffffU) return VADWR_ERR_CHUNK_COUNT;

  fi->crc32 = crc32_part(fi->crc32, dir->wrchunk, dir->wrpos);

  // compress
  const vwadwr_uint rd = dir->wrpos;
  const vwadwr_uint crc32 = crc32_buf(dir->wrchunk, rd);
  char *dest = dir->pkbuf0; // compressed buffer
  int pks;
  if (!dir->allow_lz) {
    // no LZ compression
    if (dir->allow_litonly) {
      pks = CompressLZFF3LitOnly(dir->wrchunk, rd, dest + 4, 65535);
      if (pks < 1) pks = -1;
    } else {
      pks = -1;
    }
  } else {
    pks = CompressLZFF3(dir->mman, dir->wrchunk, rd, dest + 4, 65535, dir->allow_lazy);
    if (pks == VWADWR_ERR_MEM) return VWADWR_ERR_MEM;
    if (dir->allow_litonly) {
      int pks1 = pks - 1;
      if (pks1 <= 0) pks1 = 65535;
      pks1 = CompressLZFF3LitOnly(dir->wrchunk, rd, dir->pkbuf1 + 4, pks1);
      if (pks1 > 0 && (pks <= 0 || pks1 < pks)) {
        // use this buffer
        dest = dir->pkbuf1;
        pks = pks1;
      }
    }
  }

  // append chunk and write it
  const vwadwr_uint nonce = 4 + dir->chunkCount;
  if (pks <= 0 || pks > 65535 || pks > (int)rd) {
    // raw chunk
    rescode = vwadwr_append_chunk(dir, 0);
    if (rescode != VWADWR_OK) return rescode;
    vassert(rd > 0 && rd <= 65536);
    memcpy(dest + 4, dir->wrchunk, rd);
    pks = (int)rd;
  } else {
    // packed chunk
    rescode = vwadwr_append_chunk(dir, (vwadwr_uint)pks);
    if (rescode != VWADWR_OK) return rescode;
  }
  put_u32(dest, crc32);
  crypt_buffer(dir->xorRndSeed, nonce, dest, (vwadwr_uint)pks + 4);
  if (dir->outstrm->write(dir->outstrm, dest, pks + 4) != 0) {
    return VADWR_ERR_IO_ERROR;
  }

  fi->upksize += dir->wrpos;
  fi->pksize += (vwadwr_uint)pks;
  fi->chunkCount += 1;

  if (fi->upksize > 0x7fffffffU) return VADWR_ERR_FILE_TOO_BIG;
  if (fi->pksize > 0x7fffffffU) return VADWR_ERR_FILE_TOO_BIG;

  dir->wrpos = 0;

  return VWADWR_OK;
}


//==========================================================================
//
//  vwadwr_write
//
//  writing 0 bytes is not an error
//
//==========================================================================
vwadwr_result vwadwr_write (vwadwr_dir *dir, const void *buf, vwadwr_uint len) {
  if (dir == NULL) return VADWR_ERR_ARGS;

  FileInfo *fi = dir->wrfile;
  if (fi == NULL) return VADWR_ERR_FILE_CLOSED;

  if (dir->wrmode != WR_MODE_WRITE) return VADWR_ERR_INVALID_MODE;

  if (len == 0 || len > 0x7ffffff0U) return VWADWR_OK;
  if (buf == NULL) return VADWR_ERR_ARGS;

  if (0x7ffffff0U - fi->upksize < len) return VADWR_ERR_FILE_TOO_BIG;

  vwadwr_result rescode;
  const char *src = (const char *)buf;
  while (len != 0) {
    int left = 65536 - (int)dir->wrpos;
    if (left > (int)len) left = (int)len;
    memcpy(dir->wrchunk + dir->wrpos, src, left);
    dir->wrpos += (vwadwr_uint)left;
    src += (vwadwr_uint)left;
    len -= (vwadwr_uint)left;
    vassert(dir->wrpos <= 65536);
    if (dir->wrpos == 65536) {
      // we have complete chunk, pack and write it
      rescode = vwadwr_flush_chunk(dir);
      if (rescode != VWADWR_OK) return rescode;
    }
  }

  return VWADWR_OK;
}


//==========================================================================
//
//  vwadwr_close_file
//
//  if this returned error, there is no reason to continue; call `vwadwr_free_dir()`
//
//==========================================================================
vwadwr_result vwadwr_close_file (vwadwr_dir *dir) {
  if (dir == NULL) return VADWR_ERR_ARGS;

  FileInfo *fi = dir->wrfile;
  if (fi == NULL) return VADWR_ERR_FILE_CLOSED;

  if (dir->wrmode != WR_MODE_WRITE) return VADWR_ERR_INVALID_MODE;

  // flush final chunk
  vwadwr_result rescode;
  if (dir->wrpos != 0) {
    rescode = vwadwr_flush_chunk(dir);
    if (rescode != VWADWR_OK) return rescode;
  }

  // final CRC32
  fi->crc32 = crc32_part(fi->crc32, dir->wrchunk, dir->wrpos);
  fi->crc32 = crc32_final(fi->crc32);

  dir->wrfile = NULL;

  return VWADWR_OK;
}


//==========================================================================
//
//  vwadwr_create_raw_file
//
//  you can use this API to write files with the usual fwrite-like API.
//  please note that you cannot seek backwards in this case, and only
//  one file can be created for writing.
//
//==========================================================================
vwadwr_result vwadwr_create_raw_file (vwadwr_dir *dir,
                                      const char *pkfname,
                                      const char *groupname, /* can be NULL */
                                      vwadwr_uint filecrc32,
                                      vwadwr_ftime ftime) /* can be 0; seconds since Epoch */
{
  if (dir == NULL || pkfname == NULL) return VADWR_ERR_ARGS;

  if (dir->wrfile != NULL) return VADWR_ERR_FILE_OPEN;

  if (!vwadwr_is_valid_group_name(groupname)) return VWADWR_ERR_GROUP;

  // we don't know sizes and such yet, so use zeroes
  vwadwr_result rescode = vwadwr_append_file_info(dir, pkfname, groupname, filecrc32, ftime);
  if (rescode != VWADWR_OK) {
    logf(ERROR, "cannot append file info");
    return rescode;
  }

  dir->wrfile = dir->filesTail;

  dir->wrmode = WR_MODE_RAW;

  return VWADWR_OK;
}


//==========================================================================
//
//  vwadwr_write_raw_chunk
//
//  used to copy raw decrypted chunks from other VWAD
//  use raw reading API in reader to obtain them
//  `pksz`, `upksz` and `packed` are exactly what `vwad_get_file_chunk_size()` returns
//  `chunk` is what `vwad_read_raw_file_chunk()` read
//
//==========================================================================
vwadwr_result vwadwr_write_raw_chunk (vwadwr_dir *dir, const void *chunk,
                                      int pksz, int upksz, int packed)
{
  if (dir == NULL) return VADWR_ERR_ARGS;
  if (chunk == NULL) return VADWR_ERR_ARGS;
  if (pksz < 5 || upksz < 1 || pksz > 65536 + 4 || upksz > 65536) return VADWR_ERR_ARGS;

  FileInfo *fi = dir->wrfile;

  if (fi == NULL) return VADWR_ERR_FILE_CLOSED;

  if (dir->wrmode != WR_MODE_RAW) return VADWR_ERR_INVALID_MODE;

  if (0x7ffffff0U - fi->upksize < (vwadwr_uint)pksz) return VADWR_ERR_FILE_TOO_BIG;

  if (fi->chunkCount >= 0x3fffffffU) return VADWR_ERR_CHUNK_COUNT;

  vwadwr_result rescode;
  const vwadwr_uint nonce = 4 + dir->chunkCount;
  vwadwr_uint csz = (vwadwr_uint)pksz - 4;
  if (!packed) {
    // raw chunk
    rescode = vwadwr_append_chunk(dir, 0);
    if (rescode != VWADWR_OK) return rescode;
  } else {
    // packed chunk
    rescode = vwadwr_append_chunk(dir, (vwadwr_uint)csz);
    if (rescode != VWADWR_OK) return rescode;
  }
  csz += 4; /* crc32 */
  memcpy(dir->pkbuf0, chunk, csz);
  crypt_buffer(dir->xorRndSeed, nonce, dir->pkbuf0, csz);
  if (dir->outstrm->write(dir->outstrm, dir->pkbuf0, csz) != 0) {
    return VADWR_ERR_IO_ERROR;
  }
  csz -= 4; /* crc32 */

  fi->upksize += (vwadwr_uint)upksz;
  fi->pksize += csz;
  fi->chunkCount += 1;

  if (fi->upksize > 0x7fffffffU) return VADWR_ERR_FILE_TOO_BIG;
  if (fi->pksize > 0x7fffffffU) return VADWR_ERR_FILE_TOO_BIG;

  return VWADWR_OK;
}


//==========================================================================
//
//  vwadwr_close_raw_file
//
//  if this returned error, there is no reason to continue; call `vwadwr_free_dir()`
//
//==========================================================================
vwadwr_result vwadwr_close_raw_file (vwadwr_dir *dir) {
  if (dir == NULL) return VADWR_ERR_ARGS;

  FileInfo *fi = dir->wrfile;

  if (fi == NULL) return VADWR_ERR_FILE_CLOSED;

  if (dir->wrmode != WR_MODE_RAW) return VADWR_ERR_INVALID_MODE;

  dir->wrfile = NULL;

  return VWADWR_OK;
}


// ////////////////////////////////////////////////////////////////////////// //
typedef struct {
  vwadwr_iostream *strm;
  int currpos, size;
} EdInfo;


static int ed_total_size (cc_ed25519_iostream *strm) {
  EdInfo *nfo = (EdInfo *)strm->udata;
  return nfo->size - (4+64+32); // without header
}


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
  while (bufsize != 0) {
    const int rd = nfo->strm->read(nfo->strm, buf, bufsize);
    if (rd <= 0) return -1;
    if ((bufsize -= rd) != 0) {
      buf = ((vwadwr_ubyte *)buf) + (vwadwr_uint)rd;
    }
  }
  return 0;
}


//==========================================================================
//
//  vwadwr_finish
//
//==========================================================================
vwadwr_result vwadwr_finish (vwadwr_dir **dirp) {
  if (dirp == NULL || *dirp == NULL) return VADWR_ERR_ARGS;

  vwadwr_result rescode;

  vwadwr_dir *dir = *dirp;
  if (dir->wrfile != NULL) return VADWR_ERR_FILE_OPEN;

  rescode = vwadwr_check_dir(dir);
  if (rescode != VWADWR_OK) {
    vwadwr_free_dir(dirp);
    logf(ERROR, "invalid directory");
    return rescode;
  }

  int dirofspos = dir->outstrm->tell(dir->outstrm);
  if (dirofspos <= 4*3+32+64 || dirofspos > 0x6fffffff) {
    vwadwr_free_dir(dirp);
    logf(ERROR, "archive too big");
    return VADWR_ERR_VWAD_TOO_BIG;
  }

  const vwadwr_uint dirofs = (vwadwr_uint)dirofspos;

  // pack and write main directory
  rescode = vwadwr_write_directory(dir, dir->outstrm, dirofs);
  if (rescode != VWADWR_OK) {
    vwadwr_free_dir(dirp);
    logf(ERROR, "cannot write directory");
    return rescode;
  }

  const int fout_size = dir->outstrm->tell(dir->outstrm);
  if (fout_size <= 0 || fout_size > 0x7ffffff0) {
    vwadwr_free_dir(dirp);
    logf(ERROR, "output file too big");
    return VADWR_ERR_VWAD_TOO_BIG;
  }

  // write header
  int sofs = 4+32+64+1+(int)strlen(dir->author)+1+(int)strlen(dir->title)+8;
  if (dir->outstrm->seek(dir->outstrm, sofs) != 0) {
    vwadwr_free_dir(dirp);
    logf(ERROR, "cannot seek to header");
    return VADWR_ERR_IO_ERROR;
  }

  // setup dir offset
  put_u32(&dir->mhdr.dirofs, dirofs);
  // calculate buffer crc32
  put_u32(&dir->mhdr.crc32, crc32_buf(&dir->mhdr.version, (vwadwr_uint)sizeof(dir->mhdr)-4));

  // encrypt and write main header using pk-derived seed
  crypt_buffer(dir->xorRndSeedPK, 1, &dir->mhdr, (vwadwr_uint)sizeof(dir->mhdr));
  if (dir->outstrm->write(dir->outstrm, &dir->mhdr, (int)sizeof(dir->mhdr)) != 0) {
    vwadwr_free_dir(dirp);
    logf(ERROR, "write error");
    return VADWR_ERR_IO_ERROR;
  }

  ed25519_signature edsign;
  if (dir->has_privkey) {
    cc_ed25519_iostream edstrm;
    EdInfo nfo;
    nfo.strm = dir->outstrm;
    nfo.currpos = -1;
    nfo.size = fout_size;
    if (nfo.size <= 0) {
      vwadwr_free_dir(dirp);
      logf(ERROR, "tell error");
      return VADWR_ERR_IO_ERROR;
    }
    edstrm.udata = &nfo;
    edstrm.total_size = ed_total_size;
    edstrm.read = ed_read;

    logf(NOTE, "creating signature");
    int sres = edsign_sign_stream(edsign, dir->pubkey, dir->privkey, &edstrm);
    if (sres != 0) {
      vwadwr_free_dir(dirp);
      logf(ERROR, "failed to sign data");
      return VWADWR_ERR_OTHER;
    }
  } else {
    // fill signature with file-dependent gibberish
    vwadwr_uint xseed = dir->fileCount;
    for (FileInfo *fi = dir->filesHead; fi != NULL; fi = fi->next) {
      xseed = hashU32(xseed ^ fi->upksize);
      xseed = hashU32(xseed ^ fi->chunkCount);
      xseed = hashU32(xseed ^ fi->crc32);
    }
    if (xseed == 0) xseed = deriveSeed(0xa27, NULL, 0);
    memset(edsign, 0, sizeof(ed25519_signature));
    crypt_buffer(xseed, 0x29b, edsign, (vwadwr_uint)sizeof(ed25519_signature));
  }

  // write signature (or gibberish)
  if (dir->outstrm->seek(dir->outstrm, 4) != 0) {
    vwadwr_free_dir(dirp);
    logf(ERROR, "cannot seek in output file");
    return VADWR_ERR_IO_ERROR;
  }
  if (dir->outstrm->write(dir->outstrm, edsign, (int)sizeof(ed25519_signature)) != 0) {
    vwadwr_free_dir(dirp);
    logf(ERROR, "write error");
    return VADWR_ERR_IO_ERROR;
  }

  // re-encrypt public key
  // create initial seed from signature, author, and title
  vwadwr_uint pkxseed;
  pkxseed = deriveSeed(0xa29, edsign, (vwadwr_uint)sizeof(ed25519_signature));
  pkxseed = deriveSeed(pkxseed, (const vwadwr_ubyte *)dir->author, (vwadwr_uint)strlen(dir->author));
  pkxseed = deriveSeed(pkxseed, (const vwadwr_ubyte *)dir->title, (vwadwr_uint)strlen(dir->title));
  #if 0
  logf(DEBUG, "kkseed: 0x%08x", pkxseed);
  #endif

  ed25519_public_key epk;
  memcpy(epk, dir->pubkey, sizeof(ed25519_public_key));
  crypt_buffer(pkxseed, 0x29a, epk, (vwadwr_uint)sizeof(ed25519_public_key));

  // write public key
  if (dir->outstrm->write(dir->outstrm, epk, (int)sizeof(ed25519_public_key)) != 0) {
    vwadwr_free_dir(dirp);
    logf(ERROR, "write error");
    return VADWR_ERR_IO_ERROR;
  }

  vwadwr_free_dir(dirp);
  return VWADWR_OK;
}


//==========================================================================
//
//  vwadwr_wildmatch
//
//  returns:
//    -1: malformed pattern
//     0: equal
//     1: not equal
//
//==========================================================================
vwadwr_result vwadwr_wildmatch (const char *pat, vwadwr_uint plen, const char *str, vwadwr_uint slen) {
  #define GETSCH(dst_)  do { \
    const char *stmp = &str[spos]; \
    const vwadwr_uint uclen = utf_char_len(stmp); \
    if (error || uclen == 0 || uclen > 3 || slen - spos < uclen) { error = 1; (dst_) = VWADWR_REPLACEMENT_CHAR; } \
    else { \
      (dst_) = unilower(utf_decode(&stmp)); \
      if ((dst_) < 32 || (dst_) == VWADWR_REPLACEMENT_CHAR) error = 1; \
      spos += uclen; \
    } \
  } while (0)

  #define GETPCH(dst_)  do { \
    const char *stmp = &pat[patpos]; \
    const vwadwr_uint uclen = utf_char_len(stmp); \
    if (error || uclen == 0 || uclen > 3 || plen - patpos < uclen) { error = 1; (dst_) = VWADWR_REPLACEMENT_CHAR; } \
    else { \
      (dst_) = unilower(utf_decode(&stmp)); \
      if ((dst_) < 32 || (dst_) == VWADWR_REPLACEMENT_CHAR) error = 1; \
      else patpos += uclen; \
    } \
  } while (0)

  vwadwr_ushort sch, c0, c1;
  vwadwr_bool hasMatch, inverted;
  vwadwr_bool star = 0, dostar = 0;
  vwadwr_uint patpos = 0, spos = 0;
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
//  vwadwr_wildmatch_path
//
//  this matches individual path parts
//  exception: if `pat` contains no slashes, match only the name
//
//==========================================================================
#ifdef VWAD_DEBUG_WILDPATH
#include <stdio.h>
#endif
vwadwr_result vwadwr_wildmatch_path (const char *pat, vwadwr_uint plen, const char *str, vwadwr_uint slen) {
  vwadwr_uint ppos, spos;
  vwadwr_bool pat_has_slash = 0;
  vwadwr_result res;

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
    res = vwadwr_wildmatch(pat, plen, str, slen);
  } else {
    // match by path parts
    #ifdef VWAD_DEBUG_WILDPATH
    fprintf(stderr, "=== pat:<%.*s>; str:<%.*s> ===\n",
            (vwadwr_uint)plen, pat, (vwadwr_uint)slen, str);
    #endif
    while (slen && str[0] == '/') { --slen; ++str; }
    res = 0;
    while (res == 0 && plen != 0 && slen != 0) {
      // find slash in pat and in str
      ppos = 0; while (ppos != plen && pat[ppos] != '/') ++ppos;
      spos = 0; while (spos != slen && str[spos] != '/') ++spos;
      #ifdef VWAD_DEBUG_WILDPATH
      fprintf(stderr, "  MT: ppos=%u; spos=%u; pat=<%.*s>; str=<%.*s> (ex: %d)\n",
              (vwadwr_uint)ppos, (vwadwr_uint)spos,
              (vwadwr_uint)ppos, pat, (vwadwr_uint)spos, str,
              ((ppos == plen) != (spos == slen)));
      #endif
      if ((ppos == plen) != (spos == slen)) {
        res = 1;
      } else {
        res = vwadwr_wildmatch(pat, ppos, str, spos);
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
