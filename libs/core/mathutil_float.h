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
#ifndef VAVOOM_CORELIB_MATHUTIL_FLOAT_H
#define VAVOOM_CORELIB_MATHUTIL_FLOAT_H

#ifdef __cplusplus
# define VV_FLTUTIL_NOEXCEPT  noexcept
# define VV_FLTUTIL_BOOL  bool
#else
# define VV_FLTUTIL_NOEXCEPT
# define VV_FLTUTIL_BOOL  int
#endif

// so this could be used as standalone header
#ifndef VVA_OKUNUSED
# define VVA_OKUNUSED     __attribute__((unused))
#endif
#ifndef VVA_CHECKRESULT
# define VVA_CHECKRESULT  __attribute__((warn_unused_result))
#endif
#ifndef VVA_PURE
# define VVA_PURE         __attribute__((pure))
#endif
#ifndef VVA_CONST
# define VVA_CONST        __attribute__((const))
#endif
#ifndef VVA_ALWAYS_INLINE
# define VVA_ALWAYS_INLINE  inline __attribute__((always_inline))
#endif


/*
  IEEE 754 32-bit binary float format:
    bits 0..22: mantissa (fractional part, without implied 1, unless the exponent is zero) (23 bits)
    bits 23..30: exponent (biased, to get the real signed exponent, subtract 127) (8 bits)
    bit 31: sign (1 means negative)

    mask for mantissa: 0x7fffffu

    special exponents:
      0: zero or denormal (zero if mantissa is zero, otherwise denormal)
      255: nan or infinity (infinity if mantissa is zero, otherwise nan)

    signaling nan has bit 22 reset (NOT set!). i.e. it is "is quiet" flag.

    denormals has exponent of -126, and the implied first binary digit is 0.
    fun fact: this way, zero is just a kind of denormal.
 */

/*
  IEEE 754 64-bit binary float format:
    bits 0..51: mantissa (fractional part, without implied 1, unless the exponent is zero) (52 bits)
    bits 52..62: exponent (biased, to get the real signed exponent, subtract 1023) (11 bits)
    bit 63: sign (1 means negative)

    special exponents:
      0: zero or denormal (zero if mantissa is zero, otherwise denormal)
      0x7ff (2047): nan or infinity (infinity if mantissa is zero, otherwise nan)

    signaling nan has bit 51 reset (NOT set!). i.e. it is "is quiet" flag.

    denormals has exponent of -1022, and the implied first binary digit is 0.
    fun fact: this way, zero is just a kind of denormal.
 */

//#define VC_USE_LIBC_FLOAT_CHECKERS
#include <math.h>
#include <float.h>
#include <stdint.h>

#ifdef VC_USE_LIBC_FLOAT_CHECKERS
# define isFiniteF  isfinite
# define isNaNF     isnan
# define isInfF     isinf
#else
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE VV_FLTUTIL_BOOL isFiniteF (const float v) VV_FLTUTIL_NOEXCEPT {
  const union { float f; uint32_t x; } u = {v};
  return ((u.x&0x7f800000u) != 0x7f800000u);
}

static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE VV_FLTUTIL_BOOL isNaNF (const float v) VV_FLTUTIL_NOEXCEPT {
  const union { float f; uint32_t x; } u = {v};
  return ((u.x<<1) > 0xff000000u);
}

static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE VV_FLTUTIL_BOOL isInfF (const float v) VV_FLTUTIL_NOEXCEPT {
  const union { float f; uint32_t x; } u = {v};
  return ((u.x<<1) == 0xff000000u);
}
#endif


static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE VV_FLTUTIL_BOOL isNaNFU32 (const uint32_t fv) VV_FLTUTIL_NOEXCEPT {
  return ((fv<<1) > 0xff000000u);
}

static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE VV_FLTUTIL_BOOL isInfFU32 (const uint32_t fv) VV_FLTUTIL_NOEXCEPT {
  return ((fv<<1) == 0xff000000u);
}

// this is used in VavoomC VM executor
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE VV_FLTUTIL_BOOL isFiniteFI32 (const int32_t v) VV_FLTUTIL_NOEXCEPT {
  return ((v&0x7f800000u) != 0x7f800000u);
}

// this ignores sign bit; zero float is all zeroes except the sign bit
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE int32_t isNonZeroFI32 (const int32_t v) VV_FLTUTIL_NOEXCEPT {
  return (v&0x7fffffffu);
}

// this ignores sign bit; zero float is all zeroes except the sign bit
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE VV_FLTUTIL_BOOL isZeroFI32 (const int32_t v) VV_FLTUTIL_NOEXCEPT {
  return !(v&0x7fffffffu);
}

static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE VV_FLTUTIL_BOOL isZeroInfNaNFI32 (const int32_t fi) VV_FLTUTIL_NOEXCEPT {
  const uint8_t exp = (uint8_t)((fi>>23)&0xffu);
  return
    exp == 0xffu ||
    (exp == 0x00u && (fi&0x7fffffu) == 0);
}


// this turns all denormals to positive zero
// also, turns negative zero to positive zero
static VVA_OKUNUSED VVA_ALWAYS_INLINE void zeroDenormalsFI32InPlace (__attribute__((__may_alias__)) int32_t *fi) VV_FLTUTIL_NOEXCEPT {
  if (!((*fi)&0x7f800000u)) *fi = 0u; // kill denormals
}


// this turns all nan/inf values into positive zero
static VVA_OKUNUSED VVA_ALWAYS_INLINE void killInfNaNFInPlace (float *f) VV_FLTUTIL_NOEXCEPT {
  __attribute__((__may_alias__)) int32_t *fi = (__attribute__((__may_alias__)) int32_t *)f;
  *fi &= ((((*fi)>>23)&0xff)-0xff)>>31;
}

// this turns all nan/inf values into positive zero
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE float killInfNaNF (const float f) VV_FLTUTIL_NOEXCEPT {
  int32_t fi = *(const __attribute__((__may_alias__)) int32_t *)&f;
  fi &= (((fi>>23)&0xff)-0xff)>>31;
  return *((__attribute__((__may_alias__)) float *)&fi);
}

// this turns all nan/inf and denormals to positive zero
// also, turns negative zero to positive zero
static VVA_OKUNUSED VVA_ALWAYS_INLINE void zeroNanInfDenormalsFInPlace (float *f) VV_FLTUTIL_NOEXCEPT {
  __attribute__((__may_alias__)) int32_t *fi = (__attribute__((__may_alias__)) int32_t *)f;
  if (!((*fi)&0x7f800000u)) *fi = 0u; // kill denormals
  else *fi &= ((((*fi)>>23)&0xff)-0xff)>>31; // kill nan/inf
}

// this turns all nan/inf and denormals to positive zero
// also, turns negative zero to positive zero
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE float zeroNanInfDenormalsF (const float f) VV_FLTUTIL_NOEXCEPT {
  int32_t fi = *(const __attribute__((__may_alias__)) int32_t *)&f;
  if (!(fi&0x7f800000u)) fi = 0u; // kill denormals
  else fi &= (((fi>>23)&0xff)-0xff)>>31; // kill nan/inf
  return *((__attribute__((__may_alias__)) float *)&fi);
}

// this turns all denormals to positive zero
// also, turns negative zero to positive zero
static VVA_OKUNUSED VVA_ALWAYS_INLINE void zeroDenormalsFInPlace (float *f) VV_FLTUTIL_NOEXCEPT {
  __attribute__((__may_alias__)) int32_t *fi = (__attribute__((__may_alias__)) int32_t *)f;
  if (!((*fi)&0x7f800000u)) *fi = 0u; // kill denormals
}

// this turns all denormals to positive zero
// also, turns negative zero to positive zero
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE float zeroDenormalsF (const float f) VV_FLTUTIL_NOEXCEPT {
  int32_t fi = *(const __attribute__((__may_alias__)) int32_t *)&f;
  if (!(fi&0x7f800000u)) fi = 0u; // kill denormals
  return *((__attribute__((__may_alias__)) float *)&fi);
}


// is float denormalised? (zero is not considered as denormal here)
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE VV_FLTUTIL_BOOL isDenormalF (const float v) VV_FLTUTIL_NOEXCEPT {
  const union { float f; uint32_t x; } u = {v};
  return ((u.x&0x7f800000u) == 0u && (u.x&0x007fffffu) != 0u);
}

static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE VV_FLTUTIL_BOOL isZeroInfNaNF (const float f) VV_FLTUTIL_NOEXCEPT {
  const uint32_t fi = *(const __attribute__((__may_alias__)) uint32_t *)&f;
  const uint8_t exp = (uint8_t)((fi>>23)&0xffu);
  return
    exp == 0xffu ||
    (exp == 0x00u && (fi&0x7fffffu) == 0);
}

// this ignores sign bit; zero float is all zeroes except the sign bit
// returns 0 or some unspecified non-zero unsigned integer
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE unsigned isZeroFU32 (const float f) VV_FLTUTIL_NOEXCEPT {
  return ((*(const __attribute__((__may_alias__)) uint32_t *)&f)&0x7fffffffu);
}

// this ignores sign bit; zero float is all zeroes except the sign bit
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE VV_FLTUTIL_BOOL isZeroF (const float f) VV_FLTUTIL_NOEXCEPT {
  return !((*(const __attribute__((__may_alias__)) uint32_t *)&f)&0x7fffffffu);
}

// doesn't check for nan/inf
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE VV_FLTUTIL_BOOL isNegativeF (const float f) VV_FLTUTIL_NOEXCEPT {
  return !!((*(const __attribute__((__may_alias__)) uint32_t *)&f)&0x80000000u);
}

// doesn't check for nan/inf
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE VV_FLTUTIL_BOOL isPositiveF (const float f) VV_FLTUTIL_NOEXCEPT {
  return !((*(const __attribute__((__may_alias__)) uint32_t *)&f)&0x80000000u);
}


// doesn't check for nan/inf (and can return invalid results for some nans)
// returns `true` if the float is less than zero
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE VV_FLTUTIL_BOOL isLessZeroF (const float f) VV_FLTUTIL_NOEXCEPT {
  // all negative numbers (including negative zero) has bit 31 set
  return ((*(const __attribute__((__may_alias__)) uint32_t *)&f) > 0x80000000u);
}

// doesn't check for nan/inf (and can return invalid results for some nans)
// returns `true` if the float is greater than zero
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE VV_FLTUTIL_BOOL isGreatZeroF (const float f) VV_FLTUTIL_NOEXCEPT {
  // all positive numbers has bit 31 reset, positive zero is `0`
  // subtracting 1 will convert positive zero to negative nan
  // yet negative zero will be converted to positive nan
  return ((*(const __attribute__((__may_alias__)) uint32_t *)&f)-1u < 0x7fffffffu);
}

// doesn't check for nan/inf (and can return invalid results for some nans)
// returns `true` if the float is less or equal to zero
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE VV_FLTUTIL_BOOL isLessEquZeroF (const float f) VV_FLTUTIL_NOEXCEPT {
  // all positive numbers has bit 31 reset, positive zero is `0`
  // subtracting 1 will convert positive zero to negative nan
  // yet negative zero will be converted to positive nan
  return ((*(const __attribute__((__may_alias__)) uint32_t *)&f)-1u >= 0x7fffffffu);
}

// doesn't check for nan/inf (and can return invalid results for some nans)
// returns `true` if the float is greater or equal to zero
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE VV_FLTUTIL_BOOL isGreatEquZeroF (const float f) VV_FLTUTIL_NOEXCEPT {
  // `0x80000000u` is "negative zero", all positive numbers has bit 31 reset
  return ((*(const __attribute__((__may_alias__)) uint32_t *)&f) <= 0x80000000u);
}


static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE float fltconv_create_positive_zero () VV_FLTUTIL_NOEXCEPT {
  const uint32_t t = 0u;
  return *((const __attribute__((__may_alias__)) float *)&t);
}

static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE float fltconv_create_negative_zero () VV_FLTUTIL_NOEXCEPT {
  const uint32_t t = 0x80000000u;
  return *((const __attribute__((__may_alias__)) float *)&t);
}


// -1 or +1
// note that zero has a sign too!
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE int fltconv_getsign (const float f) VV_FLTUTIL_NOEXCEPT {
  //return ((*(const uint32_t *)&f)&0x80000000u ? -1 : +1);
  // this extends sign bit, and then sets the least significant bit
  // this way we'll get either -1 (if sign bit is set), or 1 (if sign bit is reset)
  return ((*(const __attribute__((__may_alias__)) int32_t *)&f)>>31)|0x01;
}

// always positive, [0..255]
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE int fltconv_getexponent (const float f) VV_FLTUTIL_NOEXCEPT {
  return (int)(((*(const __attribute__((__may_alias__)) uint32_t *)&f)>>23)&0xffu);
}

// signed and clamped exponent: [-126..127]
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE int fltconv_getsignedexponent (const float f) VV_FLTUTIL_NOEXCEPT {
  const int res = (int)(((*(const __attribute__((__may_alias__)) uint32_t *)&f)>>23)&0xffu)-127;
  return
    res < -126 ? -126 :
    res > 127 ? 127 :
    res;
}

// always positive, [0..0x7f_ffff] (or [0..8388607])
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE int fltconv_getmantissa (const float f) VV_FLTUTIL_NOEXCEPT {
  return (int)((*(const __attribute__((__may_alias__)) uint32_t *)&f)&0x7fffffu);
}


#define FLTCONV_NAN_QUITET_BIT    (0x400000)
#define FLTCONV_NAN_QUITET_BIT_U  (0x400000u)

#define FLTCONV_NAN_MAX_MANTISSA    (0x7fffff)
#define FLTCONV_NAN_MAX_MANTISSA_U  (0x7fffffu)

// invalid values leads to "positive" quiet NaN, payload with all bits set
// note that exponent is UNSIGNED!
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE float fltconv_constructfloat (const int sign, const int exponent, const int mantissa) VV_FLTUTIL_NOEXCEPT {
  if ((sign != 1 && sign != -1) ||
      exponent < 0 || exponent > 255 ||
      mantissa < 0 || mantissa > 0x7fffff)
  {
    const uint32_t t = 0x7fffffffu;
    return *((const __attribute__((__may_alias__)) float *)&t);
  } else {
    const uint32_t t =
      (sign < 0 ? 0x80000000u : 0u)|
      (((unsigned)(exponent&0xffu))<<23)|
      ((unsigned)mantissa&0x7fffffu);
    return *((const __attribute__((__may_alias__)) float *)&t);
  }
}


static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE uint32_t fltconv_floatasuint (const float f) VV_FLTUTIL_NOEXCEPT {
  return *(const __attribute__((__may_alias__)) uint32_t *)&f;
}

static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE float fltconv_uintasfloat (const uint32_t fi) VV_FLTUTIL_NOEXCEPT {
  return *(const __attribute__((__may_alias__)) float *)&fi;
}


// 0, -1 or +1
static VVA_OKUNUSED VVA_CONST VVA_CHECKRESULT
VVA_ALWAYS_INLINE float floatSign (const float f) VV_FLTUTIL_NOEXCEPT {
  uint32_t fi = *(const __attribute__((__may_alias__)) int32_t *)&f;
  fi = (fi&0x7fffffff ? (0x3f800000u/*1.0f*/|(fi&0x80000000u)) : 0u);
  return *((__attribute__((__may_alias__)) float *)&fi);
}


#endif
