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
//**  Copyright (C) 2018-2020 Ketmar Dark
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

//#define VC_USE_LIBC_FLOAT_CHECKERS
#include <math.h>
#ifdef VC_USE_LIBC_FLOAT_CHECKERS
# define isFiniteF  isfinite
# define isNaNF     isnan
# define isInfF     isinf
#else
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
inline bool isFiniteF (const float v) {
  const union { float f; vuint32 x; } u = {v};
  return ((u.x&0x7f800000u) != 0x7f800000u);
}

static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
inline bool isNaNF (const float v) {
  const union { float f; vuint32 x; } u = {v};
  return ((u.x<<1) > 0xff000000u);
}

static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
inline bool isInfF (const float v) {
  const union { float f; vuint32 x; } u = {v};
  return ((u.x<<1) == 0xff000000u);
}
#endif

// this turns all nan/inf values into positive zero
static VVA_OKUNUSED inline void killInfNaNF (float &f) {
  vint32 fi = *(vint32 *)&f;
  fi &= (((fi>>23)&0xff)-0xff)>>31;
}


static VVA_OKUNUSED inline bool isZeroInfNaN (const float f) {
  const vuint32 fi = *(const vuint32 *)&f;
  const vuint8 exp = (vuint8)((fi>>23)&0xffu);
  return
    exp == 0xffu ||
    (exp == 0x00u && (fi&0x7fffffu) == 0);
}


// `smoothstep` performs smooth Hermite interpolation between 0 and 1 when edge0 < x < edge1
// results are undefined if edge0 � edge1
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
inline float smoothstep (const float edge0, const float edge1, float x) {
  // scale, bias and saturate x to 0..1 range
  x = (x-edge0)/(edge1-edge0);
  if (!isFiniteF(x)) return 1;
  if (x < 0) x = 0; else if (x > 1) x = 1;
  // evaluate polynomial
  return x*x*(3-2*x); // GLSL reference
  //return x*x*x*(x*(x*6-15)+10); // Ken Perlin version
}

// `smoothstep` performs smooth Hermite interpolation between 0 and 1 when edge0 < x < edge1
// results are undefined if edge0 � edge1
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
inline float smoothstepPerlin (const float edge0, const float edge1, float x) {
  // scale, bias and saturate x to 0..1 range
  x = (x-edge0)/(edge1-edge0);
  if (!isFiniteF(x)) return 1;
  if (x < 0) x = 0; else if (x > 1) x = 1;
  // evaluate polynomial
  return x*x*x*(x*(x*6-15)+10); // Ken Perlin version
}


extern "C" {
//k8: this is UB in shitplusplus, but i really can't care less
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
inline float fastInvSqrtf (const float n) {
  union { float f; vuint32 i; } ufi;
  const float ndiv2 = n*0.5f;
  ufi.f = n;
  //ufi.i = 0x5f3759dfu-(ufi.i>>1);
  // initial guess
  ufi.i = 0x5f375a86u-(ufi.i>>1); // Chris Lomont says that this is more accurate constant
  // one step of newton algorithm; can be repeated to increase accuracy
  ufi.f = ufi.f*(1.5f-(ndiv2*ufi.f*ufi.f));
#if 1 /* k8: meh, i don't care; ~0.0175 as error margin is ok for us */
  // perform one more step; it doesn't really takes much time, but gives alot better accuracy
  ufi.f = ufi.f*(1.5f-(ndiv2*ufi.f*ufi.f));
#endif
  return ufi.f;
}
}


/*
#define min2(x, y)   ((x) <= (y) ? (x) : (y))
#define max2(x, y)   ((x) >= (y) ? (x) : (y))
#define midval(min, val, max)  max2(min, min2(val, max))
*/
template <class T> constexpr VVA_CONST VVA_CHECKRESULT VVA_ALWAYS_INLINE T min2 (const T a, const T b) { return (a <= b ? a : b); }
template <class T> constexpr VVA_CONST VVA_CHECKRESULT VVA_ALWAYS_INLINE T max2 (const T a, const T b) { return (a >= b ? a : b); }
template <class T> constexpr VVA_CONST VVA_CHECKRESULT VVA_ALWAYS_INLINE T midval (const T min, const T val, const T max) { return max2(min, min2(val, max)); }
template <class T> constexpr VVA_CONST VVA_CHECKRESULT VVA_ALWAYS_INLINE T clampval (const T val, const T min, const T max) { return max2(min, min2(val, max)); }

template <class T> constexpr VVA_CONST VVA_CHECKRESULT VVA_ALWAYS_INLINE T min3 (const T a, const T b, const T c) { return min2(min2(a, b), c); }
template <class T> constexpr VVA_CONST VVA_CHECKRESULT VVA_ALWAYS_INLINE T max3 (const T a, const T b, const T c) { return max2(max2(a, b), c); }


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
static VVA_OKUNUSED VVA_CHECKRESULT inline int mround (const float Val) { return (int)floorf(Val+0.5f); }

static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
inline int ToPowerOf2 (int val) {
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
inline float AngleMod (float angle) {
#if 1
  angle = fmodf(angle, 360.0f);
  while (angle < 0.0f) angle += 360.0f;
  while (angle >= 360.0f) angle -= 360.0f;
#else
  angle = (360.0/65536)*((int)(angle*(65536/360.0))&65535);
#endif
  return angle;
}

// returns angle normalized to the range [-180 < angle <= 180]
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT
inline float AngleMod180 (float angle) {
#if 1
  angle = AngleMod(angle);
#else
  angle = (360.0/65536)*((int)(angle*(65536/360.0))&65535);
#endif
  if (angle > 180.0f) angle -= 360.0f;
  return angle;
}


static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT
inline double AngleModD (double angle) {
#if 1
  angle = fmod(angle, 360.0);
  while (angle < 0.0) angle += 360.0;
  while (angle >= 360.0) angle -= 360.0;
#else
  angle = (360.0/65536)*((int)(angle*(65536/360.0))&65535);
#endif
  return angle;
}

static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT
inline double AngleMod180D (double angle) {
#if 1
  angle = fmod(angle, 360.0);
  while (angle < -180.0) angle += 360.0;
  while (angle >= 180.0) angle -= 360.0;
#else
  angle += 180;
  angle = (360.0/65536)*((int)(angle*(65536/360.0))&65535);
  angle -= 180;
#endif
  return angle;
}

static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT
inline float AngleDiff (float afrom, float ato) { return AngleMod(ato-afrom+180.0f)-180.0f; }

#ifdef NO_SINCOS

// TODO: NEON-based impl?
static VVA_OKUNUSED inline void sincosf_dumb (float x, float *vsin, float *vcos) {
  *vsin = sinf(x);
  *vcos = cosf(x);
}

static VVA_OKUNUSED inline void sincosd_dumb (double x, double *vsin, double *vcos) {
  *vsin = sin(x);
  *vcos = cos(x);
}

#define sincosf sincosf_dumb
#define sincosd sincosd_dumb

#else

#define sincosd sincos

#endif

static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT inline float msin (const float angle) { return sinf(DEG2RADF(angle)); }
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT inline float mcos (const float angle) { return cosf(DEG2RADF(angle)); }
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT inline float mtan (const float angle) { return tanf(DEG2RADF(angle)); }
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT inline float masin (const float x) { return RAD2DEGF(asinf(x)); }
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT inline float macos (const float x) { return RAD2DEGF(acosf(x)); }
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT inline float matan (const float y, const float x) { return RAD2DEGF(atan2f(y, x)); }
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT inline double matand (const double y, const double x) { return RAD2DEGD(atan2(y, x)); }
static VVA_OKUNUSED inline void msincos (const float angle, float *vsin, float *vcos) { return sincosf(DEG2RADF(angle), vsin, vcos); }
static VVA_OKUNUSED inline void msincosd (const double angle, double *vsin, double *vcos) { return sincosd(DEG2RADD(angle), vsin, vcos); }


static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT inline float ByteToAngle (vuint8 angle) { return (float)(angle*360.0f/256.0f); }
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT inline vuint8 AngleToByte (const float angle) { return (vuint8)(AngleMod(angle)*256.0f/360.0f); }


static inline VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
vint32 scaleInt (vint32 a, vint32 b, vint32 c) {
  return (vint32)(((vint64)a*b)/c);
}

static inline VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
vuint32 scaleUInt (vuint32 a, vuint32 b, vuint32 c) {
  return (vuint32)(((vuint64)a*b)/c);
}


// this is actually branch-less for ints on x86, and even for longs on x86_64
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
inline vuint8 clampToByte (vint32 n) {
  n &= -(vint32)(n >= 0);
  return (vuint8)(n|((255-(vint32)n)>>31));
  //return (n < 0 ? 0 : n > 255 ? 255 : n);
}

static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
inline vuint8 clampToByteU (vuint32 n) {
  return (vuint8)((n&0xff)|(255-((-(vint32)(n < 256))>>24)));
}


// Neumaier-Kahan algorithm
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT
inline float neumsum2 (float v0, const float v1) {
  // one iteration
  const float t = v0+v1;
  return t+(fabsf(v0) >= fabsf(v1) ? (v0-t)+v1 : (v1-t)+v0);
}

// Neumaier-Kahan algorithm
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT
inline float neumsum3 (float v0, const float v1, const float v2) {
  // first iteration
  const float t = v0+v1;
  const float c = (fabsf(v0) >= fabsf(v1) ? (v0-t)+v1 : (v1-t)+v0);
  // second iteration
  const float t1 = t+v2;
  return t1+c+(fabsf(t) >= fabsf(v2) ? (t-t1)+v2 : (v2-t1)+t);
}

// Neumaier-Kahan algorithm
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT
inline float neumsum4 (float v0, const float v1, const float v2, const float v3) {
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
inline double neumsum2D (double v0, const double v1) {
  // one iteration
  const double t = v0+v1;
  return t+(fabs(v0) >= fabs(v1) ? (v0-t)+v1 : (v1-t)+v0);
}

// Neumaier-Kahan algorithm
static VVA_OKUNUSED VVA_PURE VVA_CHECKRESULT
inline double neumsum3D (double v0, const double v1, const double v2) {
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
  RunningAverageExp () : fadeoff(0.1f), currValue(0.0f) {}
  RunningAverageExp (float aFadeoff) : fadeoff(aFadeoff), currValue(0.0f) {}

  inline void reset () { currValue = 0.0f; }

  VVA_CHECKRESULT inline float getFadeoff () const { return fadeoff; }
  inline void setFadeoff (float aFadeoff) { fadeoff = aFadeoff; }

  inline void update (float newValue) { currValue = fadeoff*newValue+(1.0f-fadeoff)*currValue; }

  VVA_CHECKRESULT inline float getValue () const { return currValue; }
  inline void setValue (float aValue) { currValue = aValue; }
};


// ////////////////////////////////////////////////////////////////////////// //
//  xs_Float.h
//
// Source: "Know Your FPU: Fixing Floating Fast"
//         http://www.stereopsis.com/sree/fpu2006.html
//
// xs_CRoundToInt:  Round toward nearest, but ties round toward even (just like FISTP)
static VVA_OKUNUSED VVA_CHECKRESULT inline vint32 vxs_CRoundToInt (const double val, const double dmr) {
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


static inline VVA_OKUNUSED VVA_CHECKRESULT vint32 vxs_ToFix16_16 (const double val) {
  /*static*/ const double vxs_doublemagic_ = double(6755399441055744.0); //2^52*1.5, uses limited precisicion to floor
  return vxs_CRoundToInt(val, vxs_doublemagic_/(1<<16));
}


//==========================================================================
//
//  Are3DBBoxesOverlapIn2D
//
//==========================================================================
static VVA_OKUNUSED VVA_CHECKRESULT inline bool Are3DBBoxesOverlapIn2D (const float bbox0[6], const float bbox1[6]) {
  return !(
    bbox1[3+0] < bbox0[0+0] || bbox1[3+1] < bbox0[0+1] ||
    bbox1[0+0] > bbox0[3+0] || bbox1[0+1] > bbox0[3+1]
  );
}


#endif
