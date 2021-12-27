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
#include "core.h"


// all generators inited with "0x29a"
BJPRNGCtx g_bjprng_ctx = {0xe4a5c8ac, 0x00b57aaf, 0xcf096672, 0x56f252d8};
PCG32_Ctx g_small_pcg3232_ctx = 0x108f6a58; // long name, to avoid confusions
PCG3264_Ctx g_pcg3264_ctx = 0x853c49e6748fea9bULL;


//==========================================================================
//
//  RandomInit
//
//  call this to seed with random seed
//
//==========================================================================
void RandomInit () VVA_NOEXCEPT {
  // init pcg3264
  do { prng_randombytes(&g_pcg3264_ctx, sizeof(g_pcg3264_ctx)); } while (g_pcg3264_ctx == 0);
  // init pcg32
  do { prng_randombytes(&g_small_pcg3232_ctx, sizeof(g_small_pcg3232_ctx)); } while (g_small_pcg3232_ctx == 0);
  // init bjprng
  vint32 rn;
  do { prng_randombytes(&rn, sizeof(rn)); } while (rn == 0);
  bjprng_raninit(&g_bjprng_ctx, rn);
}


//**************************************************************************
// SplitMix; mostly used to generate 64-bit seeds
//**************************************************************************
uint64_t splitmix64_next (uint64_t *state) VVA_NOEXCEPT {
  uint64_t result = *state;
  *state = result+(uint64_t)0x9E3779B97f4A7C15ULL;
  result = (result^(result>>30))*(uint64_t)0xBF58476D1CE4E5B9ULL;
  result = (result^(result>>27))*(uint64_t)0x94D049BB133111EBULL;
  return result^(result>>31);
}

void splitmix64_seedU32 (uint64_t *state, uint32_t seed) VVA_NOEXCEPT {
  // hashU32
  uint32_t res = seed;
  res -= (res<<6);
  res ^= (res>>17);
  res -= (res<<9);
  res ^= (res<<4);
  res -= (res<<3);
  res ^= (res<<10);
  res ^= (res>>15);
  uint64_t n = res;
  n <<= 32;
  n |= seed;
  *state = n;
}

void splitmix64_seedU64 (uint64_t *state, uint32_t seed0, uint32_t seed1) VVA_NOEXCEPT {
  // hashU32
  uint32_t res = seed0;
  res -= (res<<6);
  res ^= (res>>17);
  res -= (res<<9);
  res ^= (res<<4);
  res -= (res<<3);
  res ^= (res<<10);
  res ^= (res>>15);
  uint64_t n = res;
  n <<= 32;
  // hashU32
  res = seed1;
  res -= (res<<6);
  res ^= (res>>17);
  res -= (res<<9);
  res ^= (res<<4);
  res -= (res<<3);
  res ^= (res<<10);
  res ^= (res>>15);
  n |= res;
  *state = n;
}



//**************************************************************************
// Bob Jenkins' ISAAC Crypto-PRNG
//**************************************************************************

// this will throw away four initial blocks, in attempt to avoid
// potential weakness in the first 8192 bits of output.
// this is deemed to be unnecessary, though.
#define ISAAC_BETTER_INIT


#define ISAAC_MIX(a,b,c,d,e,f,g,h)  { \
  a ^= b<<11;               d += a; b += c; \
  b ^= (c&0xffffffffu)>>2;  e += b; c += d; \
  c ^= d<<8;                f += c; d += e; \
  d ^= (e&0xffffffffu)>>16; g += d; e += f; \
  e ^= f<<10;               h += e; f += g; \
  f ^= (g&0xffffffffu)>>4;  a += f; g += h; \
  g ^= h<<8;                b += g; h += a; \
  h ^= (a&0xffffffffu)>>9;  c += h; a += b; \
}


#define ISAAC_ind(mm,x)  ((mm)[(x>>2)&(ISAAC_RAND_SIZE-1)])
#define ISAAC_step(mix,a,b,mm,m,m2,r,x)  { \
  x = *m;  \
  a = ((a^(mix))+(*(m2++))); \
  *(m++) = y = (ISAAC_ind(mm, x)+a+b); \
  *(r++) = b = (ISAAC_ind(mm, y>>ISAAC_RAND_SIZE_SHIFT)+x)&0xffffffffu; \
}


void ISAAC_nextblock (ISAAC_Ctx *ctx) VVA_NOEXCEPT {
  uint32_t x, y, *m, *m2, *mend;
  uint32_t *mm = ctx->randmem;
  uint32_t *r = ctx->randrsl;
  uint32_t a = ctx->randa;
  uint32_t b = ctx->randb+(++ctx->randc);
  for (m = mm, mend = m2 = m+(ISAAC_RAND_SIZE/2u); m < mend; ) {
    ISAAC_step(a<<13, a, b, mm, m, m2, r, x);
    ISAAC_step((a&0xffffffffu) >>6 , a, b, mm, m, m2, r, x);
    ISAAC_step(a<<2 , a, b, mm, m, m2, r, x);
    ISAAC_step((a&0xffffffffu) >>16, a, b, mm, m, m2, r, x);
  }
  for (m2 = mm; m2 < mend; ) {
    ISAAC_step(a<<13, a, b, mm, m, m2, r, x);
    ISAAC_step((a&0xffffffffu) >>6 , a, b, mm, m, m2, r, x);
    ISAAC_step(a<<2 , a, b, mm, m, m2, r, x);
    ISAAC_step((a&0xffffffffu) >>16, a, b, mm, m, m2, r, x);
  }
  ctx->randa = a;
  ctx->randb = b;
}


// if (flag==TRUE), then use the contents of randrsl[0..ISAAC_RAND_SIZE-1] to initialize mm[]
void ISAAC_init (ISAAC_Ctx *ctx, unsigned flag) VVA_NOEXCEPT {
  uint32_t a, b, c, d, e, f, g, h;
  uint32_t *m;
  uint32_t *r;

  ctx->randa = ctx->randb = ctx->randc = 0;
  m = ctx->randmem;
  r = ctx->randrsl;
  a = b = c = d = e = f = g = h = 0x9e3779b9u; // the golden ratio

  // scramble it
  for (unsigned i = 0; i < 4u; ++i) ISAAC_MIX(a, b, c, d, e, f, g, h);

  if (flag) {
    // initialize using the contents of r[] as the seed
    for (unsigned i = 0; i < ISAAC_RAND_SIZE; i += 8) {
      a += r[i  ]; b += r[i+1];
      c += r[i+2]; d += r[i+3];
      e += r[i+4]; f += r[i+5];
      g += r[i+6]; h += r[i+7];
      ISAAC_MIX(a, b, c, d, e, f, g, h);
      m[i  ] = a; m[i+1] = b; m[i+2] = c; m[i+3] = d;
      m[i+4] = e; m[i+5] = f; m[i+6] = g; m[i+7] = h;
    }
    // do a second pass to make all of the seed affect all of m
    for (unsigned i = 0; i < ISAAC_RAND_SIZE; i += 8) {
      a += m[i  ]; b += m[i+1];
      c += m[i+2]; d += m[i+3];
      e += m[i+4]; f += m[i+5];
      g += m[i+6]; h += m[i+7];
      ISAAC_MIX(a, b, c, d, e, f, g, h);
      m[i  ] = a; m[i+1] = b; m[i+2] = c; m[i+3] = d;
      m[i+4] = e; m[i+5] = f; m[i+6] = g; m[i+7] = h;
    }
  } else {
    for (unsigned i = 0; i < ISAAC_RAND_SIZE; i += 8) {
      // fill in mm[] with messy stuff
      ISAAC_MIX(a, b, c, d, e, f, g, h);
      m[i  ] = a; m[i+1] = b; m[i+2] = c; m[i+3] = d;
      m[i+4] = e; m[i+5] = f; m[i+6] = g; m[i+7] = h;
    }
  }

  #ifdef ISAAC_BETTER_INIT
  // throw away first 8192 bits
  ISAAC_nextblock(ctx);
  ISAAC_nextblock(ctx);
  ISAAC_nextblock(ctx);
  ISAAC_nextblock(ctx);
  #endif
  ISAAC_nextblock(ctx); // fill in the first set of results
  ctx->randcnt = ISAAC_RAND_SIZE; // prepare to use the first set of results
}


uint32_t ISAAC_next (ISAAC_Ctx *ctx) VVA_NOEXCEPT {
  return
   ctx->randcnt-- ?
     (uint32_t)ctx->randrsl[ctx->randcnt] :
     (ISAAC_nextblock(ctx), ctx->randcnt = ISAAC_RAND_SIZE-1, (uint32_t)ctx->randrsl[ctx->randcnt]);
}


//**************************************************************************
// *Really* minimal PCG32_64 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)
//**************************************************************************
void pcg3264_init (PCG3264_Ctx *rng) VVA_NOEXCEPT {
  *rng/*->state*/ = (uint64_t)0x853c49e6748fea9bULL;
  /*rng->inc = (uint64_t)0xda3e39cb94b95bdbULL;*/
}

void pcg3264_seedU32 (PCG3264_Ctx *rng, uint32_t seed) VVA_NOEXCEPT {
  uint64_t smx;
  splitmix64_seedU32(&smx, seed);
  *rng/*->state*/ = splitmix64_next(&smx);
  /*rng->inc = splitmix64(&smx)|1u;*/
}


//**************************************************************************
// *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)
//**************************************************************************
// this one doesn't repeat values (it seems)

void pcg32_seedU32 (PCG32_Ctx *rng, uint32_t seed) VVA_NOEXCEPT {
  uint64_t smx;
  splitmix64_seedU32(&smx, seed);
  const uint32_t incr = (uint32_t)splitmix64_next(&smx)|1u; // why not?
  *rng = incr+seed;
  *rng = (*rng)*747796405U+incr;
}

void pcg32_init (PCG32_Ctx *rng) VVA_NOEXCEPT {
  pcg32_seedU32(rng, 0x29au);
}


//==========================================================================
//
//  FRandomFullSlow
//
//==========================================================================
float FRandomFullSlow () VVA_NOEXCEPT {
  // 0x3f800000U is 1.0f
  // 0x3fffffffU is slightly less than 2.0f
  // sadly, we have a modulo here, but without it, the chances to get 2.0 are quite low
  // use bitmask-reject, it is quite fast, and doesn't require any divisions
  for (;;) {
    vuint32 u32 = (GenRandomU32()&0xffffffu);
    if (u32 <= 0x800000u) {
      u32 += 0x3f800000U;
      // yes, "UB". unfuck your compiler.
      return (*((const __attribute__((__may_alias__)) float*)&u32))-1.0f; // this is the same as `/(float)0x7fffffu`
    }
  }
}


//==========================================================================
//
//  FRandomBetweenSlow
//
//  FIXME: not really different for now
//
//==========================================================================
float FRandomBetweenSlow (const float minv, const float maxv) VVA_NOEXCEPT {
  if (!isFiniteF(minv) || !isFiniteF(maxv)) {
    if (isFiniteF(minv)) return minv;
    if (isFiniteF(maxv)) return maxv;
    return 0.0f;
  }
  const float range = maxv-minv;
  if (range == 0.0f) return minv;
  return FRandomFullSlow()*range+minv;
}


//==========================================================================
//
//  FRandomBetween
//
//==========================================================================
float FRandomBetween (const float minv, const float maxv) VVA_NOEXCEPT {
  if (!isFiniteF(minv) || !isFiniteF(maxv)) {
    if (isFiniteF(minv)) return minv;
    if (isFiniteF(maxv)) return maxv;
    return 0.0f;
  }
  const float range = maxv-minv;
  if (range == 0.0f) return minv;
  // inlined `FRandomFull()`
  // 0x3f800000U is 1.0f
  // 0x3fffffffU is slightly less than 2.0f
  // sadly, we have a modulo here, but without it, the chances to get 2.0 are quite low
  // this is, of course, biased, but meh...
  vuint32 u32 = 0x3f800000U+(GenRandomU32()%0x800001u);
  // yes, "UB". unfuck your compiler.
  return ((*((const __attribute__((__may_alias__)) float*)&u32))-1.0f)*range+minv; // this is the same as `/(float)0x7fffffu`
}


//==========================================================================
//
//  IRandomBetweenSlow
//
//==========================================================================
int IRandomBetweenSlow (int minv, int maxv) VVA_NOEXCEPT {
  if (minv == maxv) return minv;
  if (maxv < minv) { const int tmp = minv; minv = maxv; maxv = tmp; }
  const uint32_t range = (uint32_t)maxv-(uint32_t)minv;
  if (range == 0) return GenRandomU32(); // just in case
  uint32_t mask = 0xffffffffU;
  mask >>= __builtin_clz(range);
  for (;;) {
    const uint32_t x = GenRandomU32()&mask;
    if (x <= range) return minv+(int32_t)x;
  }
}


//==========================================================================
//
//  IRandomBetween
//
//==========================================================================
int IRandomBetween (int minv, int maxv) VVA_NOEXCEPT {
  if (minv == maxv) return minv;
  if (maxv < minv) { const int tmp = minv; minv = maxv; maxv = tmp; }
  const uint32_t range = (uint32_t)maxv-(uint32_t)minv;
  if (range == 0 || range == 0xffffffffU) return GenRandomU32(); // just in case
  return minv+(int32_t)(GenRandomU32()%(range+1U));
}
