//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2010 JƒÅnis Legzdi≈Ü≈°
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
#ifndef VAVOOM_CORELIB_MATHUTIL_H
#define VAVOOM_CORELIB_MATHUTIL_H

#include "mathutil_float.h"


// `smoothstep` performs smooth Hermite interpolation between 0 and 1 when edge0 < x < edge1
// results are undefined if edge0 ô edge1
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_FORCEINLINE float smoothstep (const float edge0, const float edge1, float x) noexcept {
  // scale, bias and saturate x to 0..1 range
  x = (x-edge0)/(edge1-edge0);
  if (!isFiniteF(x)) return 1;
  if (x < 0) x = 0; else if (x > 1) x = 1;
  // evaluate polynomial
  return x*x*(3-2*x); // GLSL reference
  //return x*x*x*(x*(x*6-15)+10); // Ken Perlin version
}

// `smoothstep` performs smooth Hermite interpolation between 0 and 1 when edge0 < x < edge1
// results are undefined if edge0 ô edge1
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_FORCEINLINE float smoothstepPerlin (const float edge0, const float edge1, float x) noexcept {
  // scale, bias and saturate x to 0..1 range
  x = (x-edge0)/(edge1-edge0);
  if (!isFiniteF(x)) return 1;
  if (x < 0) x = 0; else if (x > 1) x = 1;
  // evaluate polynomial
  return x*x*x*(x*(x*6-15)+10); // Ken Perlin version
}


//k8: this is UB in shitplusplus, but i really can't care less
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_FORCEINLINE float fastInvSqrtf (const float n) noexcept {
  union { float f; vuint32 i; } ufi;
  const float ndiv2 = n*0.5f;
  ufi.f = n;
  //ufi.i = 0x5f3759dfu-(ufi.i>>1);
  // initial guess
  ufi.i = 0x5f375a86u-(ufi.i>>1); // Chris Lomont says that this is more accurate constant
  // one step of newton algorithm; can be repeated to increase accuracy
  ufi.f = ufi.f*(1.5f-(ndiv2*ufi.f*ufi.f));
  // perform one more step; it doesn't really takes much time, but gives a lot better accuracy
  ufi.f = ufi.f*(1.5f-(ndiv2*ufi.f*ufi.f));
  return ufi.f;
}

//k8: this is UB in shitplusplus, but i really can't care less
// k8: meh, i don't care; ~0.0175 as error margin is ok for us
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_FORCEINLINE float fastInvSqrtfLP (const float n) noexcept {
  union { float f; vuint32 i; } ufi;
  const float ndiv2 = n*0.5f;
  ufi.f = n;
  //ufi.i = 0x5f3759dfu-(ufi.i>>1);
  // initial guess
  ufi.i = 0x5f375a86u-(ufi.i>>1); // Chris Lomont says that this is more accurate constant
  // one step of newton algorithm; can be repeated to increase accuracy
  ufi.f = ufi.f*(1.5f-(ndiv2*ufi.f*ufi.f));
  return ufi.f;
}


/*
#define min2(x, y)   ((x) <= (y) ? (x) : (y))
#define max2(x, y)   ((x) >= (y) ? (x) : (y))
#define midval(min, val, max)  max2(min, min2(val, max))
*/
template <typename T> constexpr VVA_CONST VVA_CHECKRESULT VVA_FORCEINLINE T min2 (const T a, const T b) noexcept { return (a <= b ? a : b); }
template <typename T> constexpr VVA_CONST VVA_CHECKRESULT VVA_FORCEINLINE T max2 (const T a, const T b) noexcept { return (a >= b ? a : b); }
//template <typename T> constexpr VVA_CONST VVA_CHECKRESULT VVA_FORCEINLINE T midval (const T min, const T val, const T max) noexcept { return max2(min, min2(val, max)); }
//template <typename T> constexpr VVA_CONST VVA_CHECKRESULT VVA_FORCEINLINE T clampval (const T val, const T min, const T max) noexcept { return max2(min, min2(val, max)); }
template <typename T> constexpr VVA_CONST VVA_CHECKRESULT VVA_FORCEINLINE T midval (const T min, const T val, const T max) noexcept { return (val < min ? min : val > max ? max : val); }
template <typename T> constexpr VVA_CONST VVA_CHECKRESULT VVA_FORCEINLINE T clampval (const T val, const T min, const T max) noexcept { return (val < min ? min : val > max ? max : val); }
// `bound` must be positive!
template <typename T> constexpr VVA_CONST VVA_CHECKRESULT VVA_FORCEINLINE T clampWithBound (const T val, const T bound) noexcept { return (val < -bound ? -bound : val > bound ? bound : val); }

template <typename T> constexpr VVA_CONST VVA_CHECKRESULT VVA_FORCEINLINE T min3 (const T a, const T b, const T c) noexcept { return min2(min2(a, b), c); }
template <typename T> constexpr VVA_CONST VVA_CHECKRESULT VVA_FORCEINLINE T max3 (const T a, const T b, const T c) noexcept { return max2(max2(a, b), c); }

template <typename T> constexpr VVA_CONST VVA_CHECKRESULT VVA_FORCEINLINE T signval (const T a) noexcept { return (a < (T)0 ? (T)-1 : a > (T)0 ? (T)+1 : (T)0); }

template <typename T> VVA_FORCEINLINE void swap2 (T &a, T &b) noexcept { const T tmp = a; a = b; b = tmp; }
template <typename T> VVA_FORCEINLINE void minswap2 (T &vmin, T &vmax) noexcept { if (vmin > vmax) { const T tmp = vmin; vmin = vmax; vmax = tmp; } }


//==========================================================================
//
//  Angles
//
//==========================================================================

#ifndef M_PI
//#define M_PI  (3.14159265358979323846)
#define M_PI  (0x1.921fb54442d18p+1)
#endif

//#define DEG2RAD_MULT_D  (0.017453292519943296)
//#define RAD2DEG_MULT_D  (57.2957795130823209)

#define DEG2RAD_MULT_D  (0x1.1df46a2529d39p-6)
#define RAD2DEG_MULT_D  (0x1.ca5dc1a63c1f8p+5)

#define DEG2RAD_MULT_F  (0x1.1df46ap-6f)
#define RAD2DEG_MULT_F  (0x1.ca5dc2p+5f)

#define DEG2RADD(a)  ((double)(a)*(double)DEG2RAD_MULT_D)
#define RAD2DEGD(a)  ((double)(a)*(double)RAD2DEG_MULT_D)

#define DEG2RADF(a)  ((float)(a)*(float)DEG2RAD_MULT_F)
#define RAD2DEGF(a)  ((float)(a)*(float)RAD2DEG_MULT_F)


//int mlog2 (int val);
//int mround (float);
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT VVA_FORCEINLINE int mround (const float Val) noexcept { return (int)floorf(Val+0.5f); }

static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_FORCEINLINE int ToPowerOf2 (int val) noexcept {
  /*
  int answer = 1;
  while (answer < val) answer <<= 1;
  return answer;
  */
  if (val < 1) val = 1;
  --val;
  val |= val>>1;
  val |= val>>2;
  val |= val>>4;
  val |= val>>8;
  val |= val>>16;
  ++val;
  return val;
}

//float AngleMod (float angle);
//float AngleMod180 (float angle);

// returns angle normalized to the range [0 <= angle < 360]
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT
VVA_FORCEINLINE float AngleMod (float angle) noexcept {
#if 1
  angle = fmodf(angle, 360.0f);
  if (angle < 0.0f) angle += 360.0f;
  //if (angle >= 360.0f) angle -= 360.0f; //k8: this should not happen
#else
  angle = (float)((360.0/65536)*((int)((double)angle*(65536/360.0))&65535));
#endif
  return angle;
}
#define AngleMod360 AngleMod

// returns angle normalized to the range [-180 < angle <= 180]
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT
VVA_FORCEINLINE float AngleMod180 (float angle) noexcept {
  angle = AngleMod(angle);
  if (angle > 180.0f) angle -= 360.0f;
  return angle;
}


static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT
VVA_FORCEINLINE double AngleModD (double angle) noexcept {
#if 1
  angle = fmod(angle, 360.0);
  if (angle < 0.0) angle += 360.0;
  //if (angle >= 360.0) angle -= 360.0; //k8: this should not happen
#else
  angle = (360.0/65536)*((int)(angle*(65536/360.0))&65535);
#endif
  return angle;
}
#define AngleMod360D AngleModD

static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT
VVA_FORCEINLINE double AngleMod180D (double angle) noexcept {
  angle = AngleModD(angle);
  if (angle > 180.0) angle -= 360.0;
  return angle;
}

// returns difference in range [-180..180]
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT
VVA_FORCEINLINE float AngleDiff (float afrom, float ato) noexcept { return AngleMod(ato-afrom+180.0f)-180.0f; }

#ifdef NO_SINCOS

// TODO: NEON-based impl?
static VVA_OKUNUSED VVA_FORCEINLINE void sincosf_dumb (float x, float *vsin, float *vcos) noexcept {
  *vsin = sinf(x);
  *vcos = cosf(x);
}

static VVA_OKUNUSED VVA_FORCEINLINE void sincosd_dumb (double x, double *vsin, double *vcos) noexcept {
  *vsin = sin(x);
  *vcos = cos(x);
}

#define sincosf sincosf_dumb
#define sincosd sincosd_dumb

#else

#define sincosd sincos

#endif

static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT VVA_FORCEINLINE float msin (const float angle) noexcept { return sinf(DEG2RADF(angle)); }
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT VVA_FORCEINLINE float mcos (const float angle) noexcept { return cosf(DEG2RADF(angle)); }
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT VVA_FORCEINLINE float mtan (const float angle) noexcept { return tanf(DEG2RADF(angle)); }
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT VVA_FORCEINLINE float masin (const float x) noexcept { return zeroDenormalsF(RAD2DEGF(asinf(x))); }
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT VVA_FORCEINLINE float macos (const float x) noexcept { return zeroDenormalsF(RAD2DEGF(acosf(x))); }
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT VVA_FORCEINLINE float matan (const float y, const float x) noexcept { return zeroDenormalsF(RAD2DEGF(atan2f(y, x))); }
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT VVA_FORCEINLINE double matand (const double y, const double x) noexcept { return zeroDenormalsF(RAD2DEGD(atan2(y, x))); }
static VVA_OKUNUSED VVA_FORCEINLINE void msincos (const float angle, float *vsin, float *vcos) noexcept { return sincosf(DEG2RADF(angle), vsin, vcos); }
static VVA_OKUNUSED VVA_FORCEINLINE void msincosd (const double angle, double *vsin, double *vcos) noexcept { return sincosd(DEG2RADD(angle), vsin, vcos); }


static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT VVA_FORCEINLINE float ByteToAngle (vuint8 angle) noexcept { return (float)(angle*360.0f/256.0f); }
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT VVA_FORCEINLINE vuint8 AngleToByte (const float angle) noexcept { return (vuint8)(AngleMod(angle)*256.0f/360.0f); }


static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_FORCEINLINE vint32 scaleInt (vint32 a, vint32 b, vint32 c) noexcept { return (vint32)(((vint64)a*b)/c); }

static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_FORCEINLINE vuint32 scaleUInt (vuint32 a, vuint32 b, vuint32 c) noexcept { return (vuint32)(((vuint64)a*b)/c); }


// this is actually branch-less for ints on x86, and even for longs on x86_64
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_FORCEINLINE vuint8 clampToByte (vint32 n) noexcept {
  n &= -(vint32)(n >= 0);
  return (vuint8)(n|((255-(vint32)n)>>31));
  //return (n < 0 ? 0 : n > 255 ? 255 : n);
}

static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_FORCEINLINE vuint8 clampToByteU (vuint32 n) noexcept {
  return (vuint8)((n&0xff)|(255-((-(vint32)(n < 256))>>24)));
}


// Neumaier-Kahan algorithm
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT
VVA_FORCEINLINE float neumsum2 (float v0, const float v1) noexcept {
  // one iteration
  const float t = v0+v1;
  return t+(fabsf(v0) >= fabsf(v1) ? (v0-t)+v1 : (v1-t)+v0);
}

// Neumaier-Kahan algorithm
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT
VVA_FORCEINLINE float neumsum3 (float v0, const float v1, const float v2) noexcept {
  // first iteration
  const float t = v0+v1;
  const float c = (fabsf(v0) >= fabsf(v1) ? (v0-t)+v1 : (v1-t)+v0);
  // second iteration
  const float t1 = t+v2;
  return t1+c+(fabsf(t) >= fabsf(v2) ? (t-t1)+v2 : (v2-t1)+t);
}

// Neumaier-Kahan algorithm
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT
VVA_FORCEINLINE float neumsum4 (float v0, const float v1, const float v2, const float v3) noexcept {
  // first iteration
  float t = v0+v1;
  float c = (fabsf(v0) >= fabsf(v1) ? (v0-t)+v1 : (v1-t)+v0);
  v0 = t;
  // second iteration
  t = v0+v2;
  c += (fabsf(v0) >= fabsf(v2) ? (v0-t)+v2 : (v2-t)+v0);
  v0 = t;
  // third iteration
  t = v0+v3;
  c += (fabsf(v0) >= fabsf(v3) ? (v0-t)+v3 : (v3-t)+v0);
  return t+c;
}


// Neumaier-Kahan algorithm
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT
VVA_FORCEINLINE double neumsum2D (double v0, const double v1) noexcept {
  // one iteration
  const double t = v0+v1;
  return t+(fabs(v0) >= fabs(v1) ? (v0-t)+v1 : (v1-t)+v0);
}

// Neumaier-Kahan algorithm
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT
VVA_FORCEINLINE double neumsum3D (double v0, const double v1, const double v2) noexcept {
  // first iteration
  const double t = v0+v1;
  const double c = (fabs(v0) >= fabs(v1) ? (v0-t)+v1 : (v1-t)+v0);
  // second iteration
  const double t1 = t+v2;
  return t1+c+(fabs(t) >= fabs(v2) ? (t-t1)+v2 : (v2-t1)+t);
}


// simple exponential running average
struct RunningAverageExp {
protected:
  float fadeoff; // = 0.1f; // 10%
  float currValue; // = 0;

public:
  VVA_FORCEINLINE RunningAverageExp () noexcept : fadeoff(0.1f), currValue(0.0f) {}
  VVA_FORCEINLINE RunningAverageExp (float aFadeoff) noexcept : fadeoff(aFadeoff), currValue(0.0f) {}

  VVA_FORCEINLINE void reset () noexcept { currValue = 0.0f; }

  VVA_CHECKRESULT VVA_FORCEINLINE float getFadeoff () const noexcept { return fadeoff; }
  VVA_FORCEINLINE void setFadeoff (float aFadeoff) noexcept { fadeoff = aFadeoff; }

  VVA_FORCEINLINE void update (float newValue) noexcept { currValue = fadeoff*newValue+(1.0f-fadeoff)*currValue; }

  VVA_CHECKRESULT VVA_FORCEINLINE float getValue () const noexcept { return currValue; }
  VVA_FORCEINLINE void setValue (float aValue) noexcept { currValue = aValue; }
};


// ////////////////////////////////////////////////////////////////////////// //
//  xs_Float.h
//
// Source: "Know Your FPU: Fixing Floating Fast"
//         http://www.stereopsis.com/sree/fpu2006.html
//
// xs_CRoundToInt:  Round toward nearest, but ties round toward even (just like FISTP)
static VVA_OKUNUSED VVA_CHECKRESULT VVA_FORCEINLINE vint32 vxs_CRoundToInt (const double val, const double dmr) noexcept {
  union vxs_doubleints_ {
    double val;
    vuint32 ival[2];
  };
  vxs_doubleints_ uval;
  uval.val = val+dmr;
#ifdef VAVOOM_LITTLE_ENDIAN
  return uval.ival[/*_xs_iman_*/0];
#else
  return uval.ival[/*_xs_iman_*/1];
#endif
}


static VVA_FORCEINLINE VVA_OKUNUSED VVA_CHECKRESULT vint32 vxs_ToFix16_16 (const double val) noexcept {
  /*static*/ const double vxs_doublemagic_ = double(6755399441055744.0); //2^52*1.5, uses limited precisicion to floor
  return vxs_CRoundToInt(val, vxs_doublemagic_/(1<<16));
}


#endif
