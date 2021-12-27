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
#ifndef VAVOOM_CORE_LIB_PRNGS
#define VAVOOM_CORE_LIB_PRNGS


// ////////////////////////////////////////////////////////////////////////// //
// http://burtleburtle.net/bob/rand/smallprng.html
typedef struct BJPRNGCtx_t {
  uint32_t a, b, c, d;
} BJPRNGCtx;

#define bjprng_rot(x,k) (((x)<<(k))|((x)>>(32-(k))))
static VVA_ALWAYS_INLINE VVA_OKUNUSED VVA_CHECKRESULT
uint32_t bjprng_ranval (BJPRNGCtx *x) VVA_NOEXCEPT {
  uint32_t e = x->a-bjprng_rot(x->b, 27);
  x->a = x->b^bjprng_rot(x->c, 17);
  x->b = x->c+x->d;
  x->c = x->d+e;
  x->d = e+x->a;
  return x->d;
}

static inline VVA_OKUNUSED void bjprng_raninit (BJPRNGCtx *x, uint32_t seed) VVA_NOEXCEPT {
  x->a = 0xf1ea5eed;
  x->b = x->c = x->d = seed;
  for (unsigned i = 0; i < 32; ++i) {
    //(void)bjprng_ranval(x);
    uint32_t e = x->a-bjprng_rot(x->b, 27);
    x->a = x->b^bjprng_rot(x->c, 17);
    x->b = x->c+x->d;
    x->c = x->d+e;
    x->d = e+x->a;
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// SplitMix; mostly used to generate 64-bit seeds
uint64_t splitmix64_next (uint64_t *state) VVA_NOEXCEPT;
void splitmix64_seedU32 (uint64_t *state, uint32_t seed) VVA_NOEXCEPT;
void splitmix64_seedU64 (uint64_t *state, uint32_t seed0, uint32_t seed1) VVA_NOEXCEPT;


// ////////////////////////////////////////////////////////////////////////// //
//**************************************************************************
// *Really* minimal PCG32_64 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)
//**************************************************************************
typedef struct __attribute__((packed)) {
  /*
  uint64_t state; // rng state: all values are possible
  uint64_t inc; // controls which RNG sequence (stream) is selected; must *always* be odd
  */
  int32_t lo, hi;
} PCG3264Ctx_ClassChecker;

typedef uint64_t PCG3264_Ctx;

#if defined(__cplusplus)
  static_assert(sizeof(PCG3264Ctx_ClassChecker) == sizeof(PCG3264_Ctx), "invalid `PCG3264_Ctx` size");
#else
  _Static_assert(sizeof(PCG3264Ctx_ClassChecker) == sizeof(PCG3264_Ctx), "invalid `PCG3264_Ctx` size");
#endif


void pcg3264_init (PCG3264_Ctx *rng) VVA_NOEXCEPT;
void pcg3264_seedU32 (PCG3264_Ctx *rng, uint32_t seed) VVA_NOEXCEPT;

static VVA_OKUNUSED inline uint32_t pcg3264_next (PCG3264_Ctx *rng) VVA_NOEXCEPT {
  const uint64_t oldstate = *rng/*->state*/;
  // advance internal state
  *rng/*->state*/ = oldstate*(uint64_t)6364136223846793005ULL+(/*rng->inc|1u*/(uint64_t)1442695040888963407ULL);
  // calculate output function (XSH RR), uses old state for max ILP
  const uint32_t xorshifted = ((oldstate>>18)^oldstate)>>27;
  const uint8_t rot = oldstate>>59;
  //return (xorshifted>>rot)|(xorshifted<<((-rot)&31));
  return (xorshifted>>rot)|(xorshifted<<(32-rot));
}


// ////////////////////////////////////////////////////////////////////////// //
// this one doesn't repeat values (it seems)
//**************************************************************************
// *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)
//**************************************************************************
typedef uint32_t PCG32_Ctx;

void pcg32_seedU32 (PCG32_Ctx *rng, uint32_t seed) VVA_NOEXCEPT;
void pcg32_init (PCG32_Ctx *rng) VVA_NOEXCEPT;

static VVA_OKUNUSED inline uint32_t pcg32_next (PCG32_Ctx *rng) VVA_NOEXCEPT {
  const uint32_t oldstate = *rng;
  *rng = oldstate*747796405U+2891336453U; //pcg_oneseq_32_step_r(rng);
  //return (uint16_t)(((oldstate>>11u)^oldstate)>>((oldstate>>30u)+11u)); //pcg_output_xsh_rs_32_16(oldstate);
  //pcg_output_rxs_m_xs_32_32(oldstate);
  const uint32_t word = ((oldstate>>((oldstate>>28u)+4u))^oldstate)*277803737u;
  return (word>>22u)^word;
}


//**************************************************************************
// Bob Jenkins' ISAAC Crypto-PRNG
//**************************************************************************
// this will throw away four initial blocks, in attempt to avoid
// potential weakness in the first 8192 bits of output.
// this is deemed to be unnecessary, though.
#define ISAAC_RAND_SIZE_SHIFT  (8)
#define ISAAC_RAND_SIZE        (1u<<ISAAC_RAND_SIZE_SHIFT)

// context of random number generator
typedef struct {
  uint32_t randcnt;
  uint32_t randrsl[ISAAC_RAND_SIZE];
  uint32_t randmem[ISAAC_RAND_SIZE];
  uint32_t randa;
  uint32_t randb;
  uint32_t randc;
} ISAAC_Ctx;


void ISAAC_nextblock (ISAAC_Ctx *ctx) VVA_NOEXCEPT;

// if (flag==TRUE), then use the contents of randrsl[0..ISAAC_RAND_SIZE-1] to initialize mm[]
void ISAAC_init (ISAAC_Ctx *ctx, unsigned flag) VVA_NOEXCEPT;

// call to retrieve a single 32-bit random value
uint32_t ISAAC_next (ISAAC_Ctx *ctx) VVA_NOEXCEPT;


// ////////////////////////////////////////////////////////////////////////// //
// initialized with `RandomInit()`
// this one is really used
extern PCG3264_Ctx g_pcg3264_ctx;
// the following are just here in case somebody may need them
extern BJPRNGCtx g_bjprng_ctx;
extern PCG32_Ctx g_small_pcg3232_ctx; // long name, to avoid confusions

void RandomInit () VVA_NOEXCEPT; // call this to seed all global generator states with some random seeds


//static VVA_ALWAYS_INLINE VVA_OKUNUSED VVA_CHECKRESULT uint32_t GenRandomU31 () { return bjprng_ranval(&g_bjprng_ctx)&0x7fffffffu; }
//static inline VVA_OKUNUSED VVA_CHECKRESULT uint8_t P_Random () { return bjprng_ranval(&g_bjprng_ctx)&0xff; }

static VVA_ALWAYS_INLINE VVA_OKUNUSED VVA_CHECKRESULT uint32_t GenRandomU31 () VVA_NOEXCEPT { return pcg3264_next(&g_pcg3264_ctx)&0x7fffffffu; }
static VVA_ALWAYS_INLINE VVA_OKUNUSED VVA_CHECKRESULT uint32_t GenRandomU32 () VVA_NOEXCEPT { return pcg3264_next(&g_pcg3264_ctx)/*&0xffffffffu*/; }
static VVA_ALWAYS_INLINE VVA_OKUNUSED VVA_CHECKRESULT uint8_t P_Random () VVA_NOEXCEPT { return pcg3264_next(&g_pcg3264_ctx)&0xff; } // [0..255]
#ifdef __cplusplus
static VVA_ALWAYS_INLINE VVA_OKUNUSED VVA_CHECKRESULT bool GenRandomBool () VVA_NOEXCEPT { return (pcg3264_next(&g_pcg3264_ctx)/*&0xffffffffu*/ < 0x80000000U); }
#else
static VVA_ALWAYS_INLINE VVA_OKUNUSED VVA_CHECKRESULT int GenRandomBool () VVA_NOEXCEPT { return (pcg3264_next(&g_pcg3264_ctx)/*&0xffffffffu*/ < 0x80000000U); }
#endif


// [0..1)
static VVA_ALWAYS_INLINE VVA_OKUNUSED VVA_CHECKRESULT float FRandom () VVA_NOEXCEPT {
  // 0x3f800000U is 1.0f
  // 0x3fffffffU is slightly less than 2.0f
  vuint32 u32 = 0x3f800000U|(GenRandomU32()&0x7fffffu);
  // yes, "UB". unfuck your compiler.
  return (*((const __attribute__((__may_alias__)) float*)&u32))-1.0f; // this is the same as `/(float)0x7fffffu`
}


// [0..1], biased
static VVA_ALWAYS_INLINE VVA_OKUNUSED VVA_CHECKRESULT float FRandomFull () VVA_NOEXCEPT {
  // 0x3f800000U is 1.0f
  // 0x3fffffffU is slightly less than 2.0f
  // sadly, we have a modulo here, but without it, the chances to get 2.0 are quite low
  // this is, of course, biased, but meh...
  vuint32 u32 = 0x3f800000U+(GenRandomU32()%0x800001u);
  // yes, "UB". unfuck your compiler.
  return (*((const __attribute__((__may_alias__)) float*)&u32))-1.0f; // this is the same as `/(float)0x7fffffu`
}


VVA_CHECKRESULT float FRandomFullSlow () VVA_NOEXCEPT; // [0..1], less biased
VVA_CHECKRESULT float FRandomBetween (const float minv, const float maxv) VVA_NOEXCEPT; // [minv..maxv]
VVA_CHECKRESULT float FRandomBetweenSlow (const float minv, const float maxv) VVA_NOEXCEPT; // [minv..maxv], slightly better distribution

VVA_CHECKRESULT int IRandomBetween (int minv, int maxv) VVA_NOEXCEPT; // [minv..maxv]
VVA_CHECKRESULT int IRandomBetweenSlow (int minv, int maxv) VVA_NOEXCEPT; // [minv..maxv], slightly better distribution


#endif
