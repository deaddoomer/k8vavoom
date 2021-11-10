// RIPEMD-160, based on the algorithm description
// public domain
#include "ripemd160.h"
#include <string.h>

//#define RIPEMD160_SELFTEST

#if defined (__cplusplus)
extern "C" {
#endif

#if 0
#ifdef WORDS_BIGENDIAN
# undef WORDS_BIGENDIAN
#endif

#if defined(__BIG_ENDIAN__)
# define WORDS_BIGENDIAN 1
#elif defined(__LITTLE_ENDIAN__)
# ifdef WORDS_BIGENDIAN
#  undef WORDS_BIGENDIAN
# endif
#else
# ifdef BSD
#  include <sys/endian.h>
# else
#  include <endian.h>
# endif
# if __BYTE_ORDER == __BIG_ENDIAN
#  define WORDS_BIGENDIAN 1
# elif __BYTE_ORDER == __LITTLE_ENDIAN
#  ifdef WORDS_BIGENDIAN
#   undef WORDS_BIGENDIAN
#  endif
# else
#  error "unable to determine endianess!"
# endif
#endif
#ifdef WORDS_BIGENDIAN
# define VAVOOM_BIG_ENDIAN
#endif
#else
# include "../endianness.h"
#endif


/* RIPEMD160_ROL(x, n) cyclically rotates x over n bits to the left */
/* x must be of an unsigned 32 bits type and 0 <= n < 32. */
#define RIPEMD160_ROL(x,n) (((x)<<(n))|((x)>>(32-(n))))

/* the five basic functions RIPEMD160_F(), RIPEMD160_G() and RIPEMD160_H() */
#define RIPEMD160_F(x,y,z)  ((x)^(y)^(z))
#define RIPEMD160_G(x,y,z)  (((x)&(y))|(~(x)&(z)))
#define RIPEMD160_H(x,y,z)  (((x)|~(y))^(z))
#define RIPEMD160_I(x,y,z)  (((x)&(z))|((y)&~(z)))
#define RIPEMD160_J(x,y,z)  ((x)^((y)|~(z)))

/* the ten basic operations RIPEMD160_FF() through RIPEMD160_III() */
#define RIPEMD160_FF(a,b,c,d,e,x,s)  \
  (a) += RIPEMD160_F((b), (c), (d))+(x); \
  (a) = RIPEMD160_ROL((a), (s))+(e); \
  (c) = RIPEMD160_ROL((c), 10);

#define RIPEMD160_GG(a,b,c,d,e,x,s)  \
  (a) += RIPEMD160_G((b), (c), (d))+(x)+0x5a827999U; \
  (a) = RIPEMD160_ROL((a), (s))+(e); \
  (c) = RIPEMD160_ROL((c), 10); \

#define RIPEMD160_HH(a,b,c,d,e,x,s)  \
  (a) += RIPEMD160_H((b), (c), (d))+(x)+0x6ed9eba1U; \
  (a) = RIPEMD160_ROL((a), (s))+(e); \
  (c) = RIPEMD160_ROL((c), 10); \

#define RIPEMD160_II(a,b,c,d,e,x,s)  \
  (a) += RIPEMD160_I((b), (c), (d))+(x)+0x8f1bbcdcU; \
  (a) = RIPEMD160_ROL((a), (s))+(e); \
  (c) = RIPEMD160_ROL((c), 10); \

#define RIPEMD160_JJ(a,b,c,d,e,x,s)  \
  (a) += RIPEMD160_J((b), (c), (d))+(x)+0xa953fd4eU; \
  (a) = RIPEMD160_ROL((a), (s))+(e); \
  (c) = RIPEMD160_ROL((c), 10); \

#define RIPEMD160_FFF(a,b,c,d,e,x,s)  \
  (a) += RIPEMD160_F((b), (c), (d))+(x); \
  (a) = RIPEMD160_ROL((a), (s))+(e); \
  (c) = RIPEMD160_ROL((c), 10); \

#define RIPEMD160_GGG(a,b,c,d,e,x,s)  \
  (a) += RIPEMD160_G((b), (c), (d))+(x)+0x7a6d76e9U; \
  (a) = RIPEMD160_ROL((a), (s))+(e); \
  (c) = RIPEMD160_ROL((c), 10); \

#define RIPEMD160_HHH(a,b,c,d,e,x,s)  \
  (a) += RIPEMD160_H((b), (c), (d))+(x)+0x6d703ef3U; \
  (a) = RIPEMD160_ROL((a), (s))+(e); \
  (c) = RIPEMD160_ROL((c), 10); \

#define RIPEMD160_III(a,b,c,d,e,x,s)  \
  (a) += RIPEMD160_I((b), (c), (d))+(x)+0x5c4dd124U; \
  (a) = RIPEMD160_ROL((a), (s))+(e); \
  (c) = RIPEMD160_ROL((c), 10); \

#define RIPEMD160_JJJ(a,b,c,d,e,x,s)  \
  (a) += RIPEMD160_J((b), (c), (d))+(x)+0x50a28be6U; \
  (a) = RIPEMD160_ROL((a), (s))+(e); \
  (c) = RIPEMD160_ROL((c), 10); \


static void ripemd160_compress (uint32_t *wkbuf, const uint32_t *X) {
  uint32_t aa = wkbuf[0];
  uint32_t bb = wkbuf[1];
  uint32_t cc = wkbuf[2];
  uint32_t dd = wkbuf[3];
  uint32_t ee = wkbuf[4];
  uint32_t aaa = wkbuf[0];
  uint32_t bbb = wkbuf[1];
  uint32_t ccc = wkbuf[2];
  uint32_t ddd = wkbuf[3];
  uint32_t eee = wkbuf[4];

  /* round 1 */
  RIPEMD160_FF(aa, bb, cc, dd, ee, X[ 0], 11);
  RIPEMD160_FF(ee, aa, bb, cc, dd, X[ 1], 14);
  RIPEMD160_FF(dd, ee, aa, bb, cc, X[ 2], 15);
  RIPEMD160_FF(cc, dd, ee, aa, bb, X[ 3], 12);
  RIPEMD160_FF(bb, cc, dd, ee, aa, X[ 4],  5);
  RIPEMD160_FF(aa, bb, cc, dd, ee, X[ 5],  8);
  RIPEMD160_FF(ee, aa, bb, cc, dd, X[ 6],  7);
  RIPEMD160_FF(dd, ee, aa, bb, cc, X[ 7],  9);
  RIPEMD160_FF(cc, dd, ee, aa, bb, X[ 8], 11);
  RIPEMD160_FF(bb, cc, dd, ee, aa, X[ 9], 13);
  RIPEMD160_FF(aa, bb, cc, dd, ee, X[10], 14);
  RIPEMD160_FF(ee, aa, bb, cc, dd, X[11], 15);
  RIPEMD160_FF(dd, ee, aa, bb, cc, X[12],  6);
  RIPEMD160_FF(cc, dd, ee, aa, bb, X[13],  7);
  RIPEMD160_FF(bb, cc, dd, ee, aa, X[14],  9);
  RIPEMD160_FF(aa, bb, cc, dd, ee, X[15],  8);

  /* round 2 */
  RIPEMD160_GG(ee, aa, bb, cc, dd, X[ 7],  7);
  RIPEMD160_GG(dd, ee, aa, bb, cc, X[ 4],  6);
  RIPEMD160_GG(cc, dd, ee, aa, bb, X[13],  8);
  RIPEMD160_GG(bb, cc, dd, ee, aa, X[ 1], 13);
  RIPEMD160_GG(aa, bb, cc, dd, ee, X[10], 11);
  RIPEMD160_GG(ee, aa, bb, cc, dd, X[ 6],  9);
  RIPEMD160_GG(dd, ee, aa, bb, cc, X[15],  7);
  RIPEMD160_GG(cc, dd, ee, aa, bb, X[ 3], 15);
  RIPEMD160_GG(bb, cc, dd, ee, aa, X[12],  7);
  RIPEMD160_GG(aa, bb, cc, dd, ee, X[ 0], 12);
  RIPEMD160_GG(ee, aa, bb, cc, dd, X[ 9], 15);
  RIPEMD160_GG(dd, ee, aa, bb, cc, X[ 5],  9);
  RIPEMD160_GG(cc, dd, ee, aa, bb, X[ 2], 11);
  RIPEMD160_GG(bb, cc, dd, ee, aa, X[14],  7);
  RIPEMD160_GG(aa, bb, cc, dd, ee, X[11], 13);
  RIPEMD160_GG(ee, aa, bb, cc, dd, X[ 8], 12);

  /* round 3 */
  RIPEMD160_HH(dd, ee, aa, bb, cc, X[ 3], 11);
  RIPEMD160_HH(cc, dd, ee, aa, bb, X[10], 13);
  RIPEMD160_HH(bb, cc, dd, ee, aa, X[14],  6);
  RIPEMD160_HH(aa, bb, cc, dd, ee, X[ 4],  7);
  RIPEMD160_HH(ee, aa, bb, cc, dd, X[ 9], 14);
  RIPEMD160_HH(dd, ee, aa, bb, cc, X[15],  9);
  RIPEMD160_HH(cc, dd, ee, aa, bb, X[ 8], 13);
  RIPEMD160_HH(bb, cc, dd, ee, aa, X[ 1], 15);
  RIPEMD160_HH(aa, bb, cc, dd, ee, X[ 2], 14);
  RIPEMD160_HH(ee, aa, bb, cc, dd, X[ 7],  8);
  RIPEMD160_HH(dd, ee, aa, bb, cc, X[ 0], 13);
  RIPEMD160_HH(cc, dd, ee, aa, bb, X[ 6],  6);
  RIPEMD160_HH(bb, cc, dd, ee, aa, X[13],  5);
  RIPEMD160_HH(aa, bb, cc, dd, ee, X[11], 12);
  RIPEMD160_HH(ee, aa, bb, cc, dd, X[ 5],  7);
  RIPEMD160_HH(dd, ee, aa, bb, cc, X[12],  5);

  /* round 4 */
  RIPEMD160_II(cc, dd, ee, aa, bb, X[ 1], 11);
  RIPEMD160_II(bb, cc, dd, ee, aa, X[ 9], 12);
  RIPEMD160_II(aa, bb, cc, dd, ee, X[11], 14);
  RIPEMD160_II(ee, aa, bb, cc, dd, X[10], 15);
  RIPEMD160_II(dd, ee, aa, bb, cc, X[ 0], 14);
  RIPEMD160_II(cc, dd, ee, aa, bb, X[ 8], 15);
  RIPEMD160_II(bb, cc, dd, ee, aa, X[12],  9);
  RIPEMD160_II(aa, bb, cc, dd, ee, X[ 4],  8);
  RIPEMD160_II(ee, aa, bb, cc, dd, X[13],  9);
  RIPEMD160_II(dd, ee, aa, bb, cc, X[ 3], 14);
  RIPEMD160_II(cc, dd, ee, aa, bb, X[ 7],  5);
  RIPEMD160_II(bb, cc, dd, ee, aa, X[15],  6);
  RIPEMD160_II(aa, bb, cc, dd, ee, X[14],  8);
  RIPEMD160_II(ee, aa, bb, cc, dd, X[ 5],  6);
  RIPEMD160_II(dd, ee, aa, bb, cc, X[ 6],  5);
  RIPEMD160_II(cc, dd, ee, aa, bb, X[ 2], 12);

  /* round 5 */
  RIPEMD160_JJ(bb, cc, dd, ee, aa, X[ 4],  9);
  RIPEMD160_JJ(aa, bb, cc, dd, ee, X[ 0], 15);
  RIPEMD160_JJ(ee, aa, bb, cc, dd, X[ 5],  5);
  RIPEMD160_JJ(dd, ee, aa, bb, cc, X[ 9], 11);
  RIPEMD160_JJ(cc, dd, ee, aa, bb, X[ 7],  6);
  RIPEMD160_JJ(bb, cc, dd, ee, aa, X[12],  8);
  RIPEMD160_JJ(aa, bb, cc, dd, ee, X[ 2], 13);
  RIPEMD160_JJ(ee, aa, bb, cc, dd, X[10], 12);
  RIPEMD160_JJ(dd, ee, aa, bb, cc, X[14],  5);
  RIPEMD160_JJ(cc, dd, ee, aa, bb, X[ 1], 12);
  RIPEMD160_JJ(bb, cc, dd, ee, aa, X[ 3], 13);
  RIPEMD160_JJ(aa, bb, cc, dd, ee, X[ 8], 14);
  RIPEMD160_JJ(ee, aa, bb, cc, dd, X[11], 11);
  RIPEMD160_JJ(dd, ee, aa, bb, cc, X[ 6],  8);
  RIPEMD160_JJ(cc, dd, ee, aa, bb, X[15],  5);
  RIPEMD160_JJ(bb, cc, dd, ee, aa, X[13],  6);

  /* parallel round 1 */
  RIPEMD160_JJJ(aaa, bbb, ccc, ddd, eee, X[ 5],  8);
  RIPEMD160_JJJ(eee, aaa, bbb, ccc, ddd, X[14],  9);
  RIPEMD160_JJJ(ddd, eee, aaa, bbb, ccc, X[ 7],  9);
  RIPEMD160_JJJ(ccc, ddd, eee, aaa, bbb, X[ 0], 11);
  RIPEMD160_JJJ(bbb, ccc, ddd, eee, aaa, X[ 9], 13);
  RIPEMD160_JJJ(aaa, bbb, ccc, ddd, eee, X[ 2], 15);
  RIPEMD160_JJJ(eee, aaa, bbb, ccc, ddd, X[11], 15);
  RIPEMD160_JJJ(ddd, eee, aaa, bbb, ccc, X[ 4],  5);
  RIPEMD160_JJJ(ccc, ddd, eee, aaa, bbb, X[13],  7);
  RIPEMD160_JJJ(bbb, ccc, ddd, eee, aaa, X[ 6],  7);
  RIPEMD160_JJJ(aaa, bbb, ccc, ddd, eee, X[15],  8);
  RIPEMD160_JJJ(eee, aaa, bbb, ccc, ddd, X[ 8], 11);
  RIPEMD160_JJJ(ddd, eee, aaa, bbb, ccc, X[ 1], 14);
  RIPEMD160_JJJ(ccc, ddd, eee, aaa, bbb, X[10], 14);
  RIPEMD160_JJJ(bbb, ccc, ddd, eee, aaa, X[ 3], 12);
  RIPEMD160_JJJ(aaa, bbb, ccc, ddd, eee, X[12],  6);

  /* parallel round 2 */
  RIPEMD160_III(eee, aaa, bbb, ccc, ddd, X[ 6],  9);
  RIPEMD160_III(ddd, eee, aaa, bbb, ccc, X[11], 13);
  RIPEMD160_III(ccc, ddd, eee, aaa, bbb, X[ 3], 15);
  RIPEMD160_III(bbb, ccc, ddd, eee, aaa, X[ 7],  7);
  RIPEMD160_III(aaa, bbb, ccc, ddd, eee, X[ 0], 12);
  RIPEMD160_III(eee, aaa, bbb, ccc, ddd, X[13],  8);
  RIPEMD160_III(ddd, eee, aaa, bbb, ccc, X[ 5],  9);
  RIPEMD160_III(ccc, ddd, eee, aaa, bbb, X[10], 11);
  RIPEMD160_III(bbb, ccc, ddd, eee, aaa, X[14],  7);
  RIPEMD160_III(aaa, bbb, ccc, ddd, eee, X[15],  7);
  RIPEMD160_III(eee, aaa, bbb, ccc, ddd, X[ 8], 12);
  RIPEMD160_III(ddd, eee, aaa, bbb, ccc, X[12],  7);
  RIPEMD160_III(ccc, ddd, eee, aaa, bbb, X[ 4],  6);
  RIPEMD160_III(bbb, ccc, ddd, eee, aaa, X[ 9], 15);
  RIPEMD160_III(aaa, bbb, ccc, ddd, eee, X[ 1], 13);
  RIPEMD160_III(eee, aaa, bbb, ccc, ddd, X[ 2], 11);

  /* parallel round 3 */
  RIPEMD160_HHH(ddd, eee, aaa, bbb, ccc, X[15],  9);
  RIPEMD160_HHH(ccc, ddd, eee, aaa, bbb, X[ 5],  7);
  RIPEMD160_HHH(bbb, ccc, ddd, eee, aaa, X[ 1], 15);
  RIPEMD160_HHH(aaa, bbb, ccc, ddd, eee, X[ 3], 11);
  RIPEMD160_HHH(eee, aaa, bbb, ccc, ddd, X[ 7],  8);
  RIPEMD160_HHH(ddd, eee, aaa, bbb, ccc, X[14],  6);
  RIPEMD160_HHH(ccc, ddd, eee, aaa, bbb, X[ 6],  6);
  RIPEMD160_HHH(bbb, ccc, ddd, eee, aaa, X[ 9], 14);
  RIPEMD160_HHH(aaa, bbb, ccc, ddd, eee, X[11], 12);
  RIPEMD160_HHH(eee, aaa, bbb, ccc, ddd, X[ 8], 13);
  RIPEMD160_HHH(ddd, eee, aaa, bbb, ccc, X[12],  5);
  RIPEMD160_HHH(ccc, ddd, eee, aaa, bbb, X[ 2], 14);
  RIPEMD160_HHH(bbb, ccc, ddd, eee, aaa, X[10], 13);
  RIPEMD160_HHH(aaa, bbb, ccc, ddd, eee, X[ 0], 13);
  RIPEMD160_HHH(eee, aaa, bbb, ccc, ddd, X[ 4],  7);
  RIPEMD160_HHH(ddd, eee, aaa, bbb, ccc, X[13],  5);

  /* parallel round 4 */
  RIPEMD160_GGG(ccc, ddd, eee, aaa, bbb, X[ 8], 15);
  RIPEMD160_GGG(bbb, ccc, ddd, eee, aaa, X[ 6],  5);
  RIPEMD160_GGG(aaa, bbb, ccc, ddd, eee, X[ 4],  8);
  RIPEMD160_GGG(eee, aaa, bbb, ccc, ddd, X[ 1], 11);
  RIPEMD160_GGG(ddd, eee, aaa, bbb, ccc, X[ 3], 14);
  RIPEMD160_GGG(ccc, ddd, eee, aaa, bbb, X[11], 14);
  RIPEMD160_GGG(bbb, ccc, ddd, eee, aaa, X[15],  6);
  RIPEMD160_GGG(aaa, bbb, ccc, ddd, eee, X[ 0], 14);
  RIPEMD160_GGG(eee, aaa, bbb, ccc, ddd, X[ 5],  6);
  RIPEMD160_GGG(ddd, eee, aaa, bbb, ccc, X[12],  9);
  RIPEMD160_GGG(ccc, ddd, eee, aaa, bbb, X[ 2], 12);
  RIPEMD160_GGG(bbb, ccc, ddd, eee, aaa, X[13],  9);
  RIPEMD160_GGG(aaa, bbb, ccc, ddd, eee, X[ 9], 12);
  RIPEMD160_GGG(eee, aaa, bbb, ccc, ddd, X[ 7],  5);
  RIPEMD160_GGG(ddd, eee, aaa, bbb, ccc, X[10], 15);
  RIPEMD160_GGG(ccc, ddd, eee, aaa, bbb, X[14],  8);

  /* parallel round 5 */
  RIPEMD160_FFF(bbb, ccc, ddd, eee, aaa, X[12] ,  8);
  RIPEMD160_FFF(aaa, bbb, ccc, ddd, eee, X[15] ,  5);
  RIPEMD160_FFF(eee, aaa, bbb, ccc, ddd, X[10] , 12);
  RIPEMD160_FFF(ddd, eee, aaa, bbb, ccc, X[ 4] ,  9);
  RIPEMD160_FFF(ccc, ddd, eee, aaa, bbb, X[ 1] , 12);
  RIPEMD160_FFF(bbb, ccc, ddd, eee, aaa, X[ 5] ,  5);
  RIPEMD160_FFF(aaa, bbb, ccc, ddd, eee, X[ 8] , 14);
  RIPEMD160_FFF(eee, aaa, bbb, ccc, ddd, X[ 7] ,  6);
  RIPEMD160_FFF(ddd, eee, aaa, bbb, ccc, X[ 6] ,  8);
  RIPEMD160_FFF(ccc, ddd, eee, aaa, bbb, X[ 2] , 13);
  RIPEMD160_FFF(bbb, ccc, ddd, eee, aaa, X[13] ,  6);
  RIPEMD160_FFF(aaa, bbb, ccc, ddd, eee, X[14] ,  5);
  RIPEMD160_FFF(eee, aaa, bbb, ccc, ddd, X[ 0] , 15);
  RIPEMD160_FFF(ddd, eee, aaa, bbb, ccc, X[ 3] , 13);
  RIPEMD160_FFF(ccc, ddd, eee, aaa, bbb, X[ 9] , 11);
  RIPEMD160_FFF(bbb, ccc, ddd, eee, aaa, X[11] , 11);

  /* combine results */
  ddd += cc+wkbuf[1]; /* final result for ctx->wkbuf[0] */
  wkbuf[1] = wkbuf[2]+dd+eee;
  wkbuf[2] = wkbuf[3]+ee+aaa;
  wkbuf[3] = wkbuf[4]+aa+bbb;
  wkbuf[4] = wkbuf[0]+bb+ccc;
  wkbuf[0] = ddd;
}


void ripemd160_init (RIPEMD160_Ctx *ctx) {
  if (!ctx) return;
  memset(ctx, 0, sizeof(*ctx));
  ctx->wkbuf[0] = 0x67452301UL;
  ctx->wkbuf[1] = 0xefcdab89UL;
  ctx->wkbuf[2] = 0x98badcfeUL;
  ctx->wkbuf[3] = 0x10325476UL;
  ctx->wkbuf[4] = 0xc3d2e1f0UL;
}


static inline void ripemd160_putbyte (RIPEMD160_Ctx *ctx, uint8_t b) {
  ctx->chunkd[ctx->chunkpos>>2] |= ((uint32_t)b)<<((ctx->chunkpos&0x03U)<<3);
  if (++ctx->chunkpos == 64U) {
    ripemd160_compress(ctx->wkbuf, ctx->chunkd);
    memset(ctx->chunkd, 0, sizeof(ctx->chunkd));
    ctx->chunkpos = 0U;
  }
}


void ripemd160_put (RIPEMD160_Ctx *ctx, const void *data, size_t datasize) {
  if (!ctx || !data || !datasize) return;
  #if UINTPTR_MAX <= 0xffffffffU
    if (ctx->total+datasize <= ctx->total) ++ctx->totalhi;
    ctx->total += datasize;
  #elif UINTPTR_MAX == 0xffffffffffffffffU
    if (ctx->total+(uint32_t)datasize <= ctx->total) ++ctx->totalhi;
    ctx->total += (uint32_t)datasize;
    ctx->totalhi += (uint32_t)(datasize>>32);
  #else
  # error "unknown pointer size"
  #endif
  const uint8_t *b = (const uint8_t *)data;
#ifdef VAVOOM_BIG_ENDIAN
  while (datasize--) ripemd160_putbyte(ctx, *b++);
#else
  // process full chunks, if possible
  if (ctx->chunkpos == 0 && datasize >= (size_t)64) {
    do {
      ripemd160_compress(ctx->wkbuf, (const uint32_t *)b);
      b += 64;
    } while ((datasize -= (size_t)64) >= (size_t)64);
    if (datasize == 0) return;
  }
  // we can use `memcpy()` here
  uint32_t left = 64U-ctx->chunkpos;
  if ((size_t)left > datasize) left = (uint32_t)datasize;
  memcpy(((uint8_t *)ctx->chunkd)+ctx->chunkpos, b, (size_t)left);
  if ((ctx->chunkpos += left) == 64U) {
    ripemd160_compress(ctx->wkbuf, ctx->chunkd);
    b += left;
    datasize -= (size_t)left;
    while (datasize >= (size_t)64) {
      ripemd160_compress(ctx->wkbuf, (const uint32_t *)b);
      b += 64;
      datasize -= 64;
    }
    if (datasize) {
      memcpy(ctx->chunkd, b, datasize);
      memset(((uint8_t *)ctx->chunkd)+datasize, 0, (size_t)64-datasize);
      ctx->chunkpos = (uint32_t)datasize&0x3fU;
    } else {
      memset(ctx->chunkd, 0, sizeof(ctx->chunkd));
      ctx->chunkpos = 0U;
    }
  }
  #if 0
  // this is invariant
  else {
    datasize -= (size_t)left;
    if (datasize) __builtin_trap();
  }
  #endif
#endif
}


void ripemd160_finish (const RIPEMD160_Ctx *ctx, void *hash) {
  if (!ctx || !hash) return;
  RIPEMD160_Ctx rctx;
  memcpy((void *)&rctx, (void *)ctx, sizeof(RIPEMD160_Ctx));

  // padding with `1` bit
  ripemd160_putbyte(&rctx, 0x80);
  // length in bits goes into two last dwords
  if (64U-rctx.chunkpos < 8) {
    //while (rctx.chunkpos) ripemd160_putbyte(&rctx, 0);
    ripemd160_compress(rctx.wkbuf, rctx.chunkd);
    memset(rctx.chunkd, 0, sizeof(rctx.chunkd));
  }
  rctx.chunkd[14] = ctx->total<<3;
  rctx.chunkd[15] = (ctx->total>>29)|(ctx->totalhi<<3);
  ripemd160_compress(rctx.wkbuf, rctx.chunkd);

  memcpy(hash, (void *)rctx.wkbuf, RIPEMD160_BYTES);
}


int ripemd160_canput (const RIPEMD160_Ctx *ctx, const size_t datasize) {
  if (!ctx) return 0;
  #if UINTPTR_MAX <= 0xffffffffU
    return (ctx->totalhi < 0x1fffffffU+(ctx->total+datasize > ctx->total));
  #elif UINTPTR_MAX == 0xffffffffffffffffU
    // totally untested
    const uint32_t lo = (uint32_t)datasize;
    const uint32_t hi = (uint32_t)(datasize>>32);
    if (ctx->totalhi+hi >= 0x20000000U) return 0;
    return ((hi+ctx->totalhi) < 0x1fffffffU+(ctx->total+lo > ctx->total));
  #else
  # error "unknown pointer size"
  #endif
}


#ifdef RIPEMD160_SELFTEST
#include <stdio.h>
static void selftest_str (const char *str, const char *res, int slen) {
  RIPEMD160_Ctx ctx;
  uint8_t hash[RIPEMD160_BYTES];
  char hhex[RIPEMD160_BYTES*2+2];

  if (slen < 0) slen = (int)strlen(str);
  ripemd160_init(&ctx);
  ripemd160_put(&ctx, str, slen);
  ripemd160_finish(&ctx, hash);
  for (unsigned c = 0; c < RIPEMD160_BYTES; ++c) snprintf(hhex+c*2, 3, "%02x", hash[c]);

  if (strcmp(hhex, res) != 0) {
    fprintf(stderr, "FAILURE!\n");
    fprintf(stderr, "string  : %.*s\n", (unsigned)slen, str);
    fprintf(stderr, "result  : %s\n", hhex);
    fprintf(stderr, "expected: %s\n", res);
    __builtin_trap();
  }
}


void ripemd160_selftest (void) {
  selftest_str("", "9c1185a5c5e9fc54612808977ee8f548b2258d31", -1);
  selftest_str("abcdefghijklmnopqrstuvwxyz", "f71c27109c692c1b56bbdceb5b9d2865b3708dbc", -1);

  selftest_str("a", "0bdc9d2d256b3ee9daae347be6f4dc835a467ffe", -1);
  selftest_str("abc", "8eb208f7e05d987a9b044a8e98c6b087f15a0bfc", -1);
  selftest_str("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", "12a053384a9c0c88e405a06c27dcf49ada62eb2b", -1);
  selftest_str("message digest", "5d0689ef49d2fae572b881b123a85ffa21595f36", -1);
  selftest_str("abcdefghijklmnopqrstuvwxyz", "f71c27109c692c1b56bbdceb5b9d2865b3708dbc", -1);
  selftest_str("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", "b0e20b6e3116640286ed3a87a5713079b21f5189", -1);
  selftest_str("1234567890123456789012345678901234567890" "1234567890123456789012345678901234567890", "9b752e45573d4b39f4dbd3323cab82bf63326bfb", -1);

  selftest_str(
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
    , "160dd118d84b2ceff71261ab78fb9239e257c8d3", -1);

  // 160 zero bits
  char zero[160/8];
  memset(zero, 0, sizeof(zero));
  selftest_str(zero, "5c00bd4aca04a9057c09b20b05f723f2e23deb65", 160/8);
}
#endif


#if defined (__cplusplus)
}
#endif
