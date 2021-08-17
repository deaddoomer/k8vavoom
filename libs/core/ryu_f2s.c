// Copyright 2018 Ulf Adams
//
// The contents of this file may be used under the terms of the Apache License,
// Version 2.0.
//
//    (See accompanying file LICENSE-Apache or copy at
//     http://www.apache.org/licenses/LICENSE-2.0)
//
// Alternatively, the contents of this file may be used under the terms of
// the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE-Boost or copy at
//     https://www.boost.org/LICENSE_1_0.txt)
//
// Unless required by applicable law or agreed to in writing, this software
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.

// Runtime compiler options:
// -DRYU_DEBUG Generate verbose debugging output to stdout.

#include "ryu.h"
#ifdef RYU_DEBUG
# undef RYU_DEBUG
#endif

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef RYU_DEBUG
#include <stdio.h>
#endif
//#include <stdio.h>


/*#include "ryu/common.h"*/
// Returns the number of decimal digits in v, which must not contain more than 9 digits.
static inline uint32_t decimalLength9(const uint32_t v) {
  // Function precondition: v is not a 10-digit number.
  // (f2s: 9 digits are sufficient for round-tripping.)
  // (d2fixed: We print 9-digit blocks.)
  assert(v < 1000000000);
  if (v >= 100000000) { return 9; }
  if (v >= 10000000) { return 8; }
  if (v >= 1000000) { return 7; }
  if (v >= 100000) { return 6; }
  if (v >= 10000) { return 5; }
  if (v >= 1000) { return 4; }
  if (v >= 100) { return 3; }
  if (v >= 10) { return 2; }
  return 1;
}

// Returns e == 0 ? 1 : [log_2(5^e)]; requires 0 <= e <= 3528.
static inline int32_t log2pow5(const int32_t e) {
  // This approximation works up to the point that the multiplication overflows at e = 3529.
  // If the multiplication were done in 64 bits, it would fail at 5^4004 which is just greater
  // than 2^9297.
  assert(e >= 0);
  assert(e <= 3528);
  return (int32_t) ((((uint32_t) e) * 1217359) >> 19);
}

// Returns e == 0 ? 1 : ceil(log_2(5^e)); requires 0 <= e <= 3528.
static inline int32_t pow5bits(const int32_t e) {
  // This approximation works up to the point that the multiplication overflows at e = 3529.
  // If the multiplication were done in 64 bits, it would fail at 5^4004 which is just greater
  // than 2^9297.
  assert(e >= 0);
  assert(e <= 3528);
  return (int32_t) (((((uint32_t) e) * 1217359) >> 19) + 1);
}

// Returns e == 0 ? 1 : ceil(log_2(5^e)); requires 0 <= e <= 3528.
static inline int32_t ceil_log2pow5(const int32_t e) {
  return log2pow5(e) + 1;
}

// Returns floor(log_10(2^e)); requires 0 <= e <= 1650.
static inline uint32_t log10Pow2(const int32_t e) {
  // The first value this approximation fails for is 2^1651 which is just greater than 10^297.
  assert(e >= 0);
  assert(e <= 1650);
  return (((uint32_t) e) * 78913) >> 18;
}

// Returns floor(log_10(5^e)); requires 0 <= e <= 2620.
static inline uint32_t log10Pow5(const int32_t e) {
  // The first value this approximation fails for is 5^2621 which is just greater than 10^1832.
  assert(e >= 0);
  assert(e <= 2620);
  return (((uint32_t) e) * 732923) >> 20;
}

static inline int copy_special_str(char * const result, bool sign, const bool exponent, const bool mantissa) {
  if (mantissa) {
    memcpy(result, "NaN", 3);
    return 3;
  }
  if (exponent) {
    result[0] = (sign ? '-' : '+');
    memcpy(result + 1, "Inf", 3);
    return 4;
  }
  if (sign) {
    result[0] = '-';
    result[1] = '0';
    return 2;
  } else {
    result[0] = '0';
    return 1;
  }
}

static inline uint32_t float_to_bits(const float f) {
  uint32_t bits = 0;
  memcpy(&bits, &f, sizeof(float));
  return bits;
}

static inline uint64_t double_to_bits(const double d) {
  uint64_t bits = 0;
  memcpy(&bits, &d, sizeof(double));
  return bits;
}


/*#include "ryu/f2s_intrinsics.h"*/
// This table is generated by PrintFloatLookupTable.
#define FLOAT_POW5_INV_BITCOUNT 59
#define FLOAT_POW5_BITCOUNT 61

static uint64_t FLOAT_POW5_INV_SPLIT[55] = {
  576460752303423489u,  461168601842738791u,  368934881474191033u,  295147905179352826u,
  472236648286964522u,  377789318629571618u,  302231454903657294u,  483570327845851670u,
  386856262276681336u,  309485009821345069u,  495176015714152110u,  396140812571321688u,
  316912650057057351u,  507060240091291761u,  405648192073033409u,  324518553658426727u,
  519229685853482763u,  415383748682786211u,  332306998946228969u,  531691198313966350u,
  425352958651173080u,  340282366920938464u,  544451787073501542u,  435561429658801234u,
  348449143727040987u,  557518629963265579u,  446014903970612463u,  356811923176489971u,
  570899077082383953u,  456719261665907162u,  365375409332725730u,  292300327466180584u,
  467680523945888934u,  374144419156711148u,  299315535325368918u,  478904856520590269u,
  383123885216472215u,  306499108173177772u,  490398573077084435u,  392318858461667548u,
  313855086769334039u,  502168138830934462u,  401734511064747569u,  321387608851798056u,
  514220174162876889u,  411376139330301511u,  329100911464241209u,  526561458342785934u,
  421249166674228747u,  336999333339382998u,  539198933343012796u,  431359146674410237u,
  345087317339528190u,  552139707743245103u,  441711766194596083u
};
static const uint64_t FLOAT_POW5_SPLIT[47] = {
  1152921504606846976u, 1441151880758558720u, 1801439850948198400u, 2251799813685248000u,
  1407374883553280000u, 1759218604441600000u, 2199023255552000000u, 1374389534720000000u,
  1717986918400000000u, 2147483648000000000u, 1342177280000000000u, 1677721600000000000u,
  2097152000000000000u, 1310720000000000000u, 1638400000000000000u, 2048000000000000000u,
  1280000000000000000u, 1600000000000000000u, 2000000000000000000u, 1250000000000000000u,
  1562500000000000000u, 1953125000000000000u, 1220703125000000000u, 1525878906250000000u,
  1907348632812500000u, 1192092895507812500u, 1490116119384765625u, 1862645149230957031u,
  1164153218269348144u, 1455191522836685180u, 1818989403545856475u, 2273736754432320594u,
  1421085471520200371u, 1776356839400250464u, 2220446049250313080u, 1387778780781445675u,
  1734723475976807094u, 2168404344971008868u, 1355252715606880542u, 1694065894508600678u,
  2117582368135750847u, 1323488980084844279u, 1654361225106055349u, 2067951531382569187u,
  1292469707114105741u, 1615587133892632177u, 2019483917365790221u
};


static inline uint32_t pow5factor_32(uint32_t value) {
  uint32_t count = 0;
  for (;;) {
    assert(value != 0);
    const uint32_t q = value / 5;
    const uint32_t r = value % 5;
    if (r != 0) {
      break;
    }
    value = q;
    ++count;
  }
  return count;
}

// Returns true if value is divisible by 5^p.
static inline bool multipleOfPowerOf5_32(const uint32_t value, const uint32_t p) {
  return pow5factor_32(value) >= p;
}

// Returns true if value is divisible by 2^p.
static inline bool multipleOfPowerOf2_32(const uint32_t value, const uint32_t p) {
  // __builtin_ctz doesn't appear to be faster here.
  return (value & ((1u << p) - 1)) == 0;
}

// It seems to be slightly faster to avoid uint128_t here, although the
// generated code for uint128_t looks slightly nicer.
static inline uint32_t mulShift32(const uint32_t m, const uint64_t factor, const int32_t shift) {
  assert(shift > 32);

  // The casts here help MSVC to avoid calls to the __allmul library
  // function.
  const uint32_t factorLo = (uint32_t)(factor);
  const uint32_t factorHi = (uint32_t)(factor >> 32);
  const uint64_t bits0 = (uint64_t)m * factorLo;
  const uint64_t bits1 = (uint64_t)m * factorHi;

  // On 32-bit platforms we can avoid a 64-bit shift-right since we only
  // need the upper 32 bits of the result and the shift value is > 32.
  const uint32_t bits0Hi = (uint32_t)(bits0 >> 32);
  uint32_t bits1Lo = (uint32_t)(bits1);
  uint32_t bits1Hi = (uint32_t)(bits1 >> 32);
  bits1Lo += bits0Hi;
  bits1Hi += (bits1Lo < bits0Hi);
  if (shift >= 64) {
    // s2f can call this with a shift value >= 64, which we have to handle.
    // This could now be slower than the !defined(RYU_32_BIT_PLATFORM) case.
    return (uint32_t)(bits1Hi >> (shift - 64));
  } else {
    const int32_t s = shift - 32;
    return (bits1Hi << (32 - s)) | (bits1Lo >> s);
  }
}

static inline uint32_t mulPow5InvDivPow2(const uint32_t m, const uint32_t q, const int32_t j) {
  return mulShift32(m, FLOAT_POW5_INV_SPLIT[q], j);
}

static inline uint32_t mulPow5divPow2(const uint32_t m, const uint32_t i, const int32_t j) {
  return mulShift32(m, FLOAT_POW5_SPLIT[i], j);
}


/*#include "ryu/digit_table.h"*/
// A table of all two-digit numbers. This is used to speed up decimal digit
// generation by copying pairs of digits into the final output.
static const char DIGIT_TABLE[200] = {
  '0','0','0','1','0','2','0','3','0','4','0','5','0','6','0','7','0','8','0','9',
  '1','0','1','1','1','2','1','3','1','4','1','5','1','6','1','7','1','8','1','9',
  '2','0','2','1','2','2','2','3','2','4','2','5','2','6','2','7','2','8','2','9',
  '3','0','3','1','3','2','3','3','3','4','3','5','3','6','3','7','3','8','3','9',
  '4','0','4','1','4','2','4','3','4','4','4','5','4','6','4','7','4','8','4','9',
  '5','0','5','1','5','2','5','3','5','4','5','5','5','6','5','7','5','8','5','9',
  '6','0','6','1','6','2','6','3','6','4','6','5','6','6','6','7','6','8','6','9',
  '7','0','7','1','7','2','7','3','7','4','7','5','7','6','7','7','7','8','7','9',
  '8','0','8','1','8','2','8','3','8','4','8','5','8','6','8','7','8','8','8','9',
  '9','0','9','1','9','2','9','3','9','4','9','5','9','6','9','7','9','8','9','9'
};



#define FLOAT_MANTISSA_BITS 23
#define FLOAT_EXPONENT_BITS 8
#define FLOAT_BIAS 127

// A floating decimal representing m * 10^e.
typedef struct floating_decimal_32 {
  uint32_t mantissa;
  // Decimal exponent's range is -45 to 38
  // inclusive, and can fit in a short if needed.
  int32_t exponent;
} floating_decimal_32;

static inline floating_decimal_32 f2d(const uint32_t ieeeMantissa, const uint32_t ieeeExponent) {
  int32_t e2;
  uint32_t m2;
  if (ieeeExponent == 0) {
    // We subtract 2 so that the bounds computation has 2 additional bits.
    e2 = 1 - FLOAT_BIAS - FLOAT_MANTISSA_BITS - 2;
    m2 = ieeeMantissa;
  } else {
    e2 = (int32_t) ieeeExponent - FLOAT_BIAS - FLOAT_MANTISSA_BITS - 2;
    m2 = (1u << FLOAT_MANTISSA_BITS) | ieeeMantissa;
  }
  const bool even = (m2 & 1) == 0;
  const bool acceptBounds = even;

#ifdef RYU_DEBUG
  printf("-> %u * 2^%d\n", m2, e2 + 2);
#endif

  // Step 2: Determine the interval of valid decimal representations.
  const uint32_t mv = 4 * m2;
  const uint32_t mp = 4 * m2 + 2;
  // Implicit bool -> int conversion. True is 1, false is 0.
  const uint32_t mmShift = ieeeMantissa != 0 || ieeeExponent <= 1;
  const uint32_t mm = 4 * m2 - 1 - mmShift;

  // Step 3: Convert to a decimal power base using 64-bit arithmetic.
  uint32_t vr, vp, vm;
  int32_t e10;
  bool vmIsTrailingZeros = false;
  bool vrIsTrailingZeros = false;
  uint8_t lastRemovedDigit = 0;
  if (e2 >= 0) {
    const uint32_t q = log10Pow2(e2);
    e10 = (int32_t) q;
    const int32_t k = FLOAT_POW5_INV_BITCOUNT + pow5bits((int32_t) q) - 1;
    const int32_t i = -e2 + (int32_t) q + k;
    vr = mulPow5InvDivPow2(mv, q, i);
    vp = mulPow5InvDivPow2(mp, q, i);
    vm = mulPow5InvDivPow2(mm, q, i);
#ifdef RYU_DEBUG
    printf("%u * 2^%d / 10^%u\n", mv, e2, q);
    printf("V+=%u\nV =%u\nV-=%u\n", vp, vr, vm);
#endif
    if (q != 0 && (vp - 1) / 10 <= vm / 10) {
      // We need to know one removed digit even if we are not going to loop below. We could use
      // q = X - 1 above, except that would require 33 bits for the result, and we've found that
      // 32-bit arithmetic is faster even on 64-bit machines.
      const int32_t l = FLOAT_POW5_INV_BITCOUNT + pow5bits((int32_t) (q - 1)) - 1;
      lastRemovedDigit = (uint8_t) (mulPow5InvDivPow2(mv, q - 1, -e2 + (int32_t) q - 1 + l) % 10);
    }
    if (q <= 9) {
      // The largest power of 5 that fits in 24 bits is 5^10, but q <= 9 seems to be safe as well.
      // Only one of mp, mv, and mm can be a multiple of 5, if any.
      if (mv % 5 == 0) {
        vrIsTrailingZeros = multipleOfPowerOf5_32(mv, q);
      } else if (acceptBounds) {
        vmIsTrailingZeros = multipleOfPowerOf5_32(mm, q);
      } else {
        vp -= multipleOfPowerOf5_32(mp, q);
      }
    }
  } else {
    const uint32_t q = log10Pow5(-e2);
    e10 = (int32_t) q + e2;
    const int32_t i = -e2 - (int32_t) q;
    const int32_t k = pow5bits(i) - FLOAT_POW5_BITCOUNT;
    int32_t j = (int32_t) q - k;
    vr = mulPow5divPow2(mv, (uint32_t) i, j);
    vp = mulPow5divPow2(mp, (uint32_t) i, j);
    vm = mulPow5divPow2(mm, (uint32_t) i, j);
#ifdef RYU_DEBUG
    printf("%u * 5^%d / 10^%u\n", mv, -e2, q);
    printf("%u %d %d %d\n", q, i, k, j);
    printf("V+=%u\nV =%u\nV-=%u\n", vp, vr, vm);
#endif
    if (q != 0 && (vp - 1) / 10 <= vm / 10) {
      j = (int32_t) q - 1 - (pow5bits(i + 1) - FLOAT_POW5_BITCOUNT);
      lastRemovedDigit = (uint8_t) (mulPow5divPow2(mv, (uint32_t) (i + 1), j) % 10);
    }
    if (q <= 1) {
      // {vr,vp,vm} is trailing zeros if {mv,mp,mm} has at least q trailing 0 bits.
      // mv = 4 * m2, so it always has at least two trailing 0 bits.
      vrIsTrailingZeros = true;
      if (acceptBounds) {
        // mm = mv - 1 - mmShift, so it has 1 trailing 0 bit iff mmShift == 1.
        vmIsTrailingZeros = mmShift == 1;
      } else {
        // mp = mv + 2, so it always has at least one trailing 0 bit.
        --vp;
      }
    } else if (q < 31) { // TODO(ulfjack): Use a tighter bound here.
      vrIsTrailingZeros = multipleOfPowerOf2_32(mv, q - 1);
#ifdef RYU_DEBUG
      printf("vr is trailing zeros=%s\n", vrIsTrailingZeros ? "true" : "false");
#endif
    }
  }
#ifdef RYU_DEBUG
  printf("e10=%d\n", e10);
  printf("V+=%u\nV =%u\nV-=%u\n", vp, vr, vm);
  printf("vm is trailing zeros=%s\n", vmIsTrailingZeros ? "true" : "false");
  printf("vr is trailing zeros=%s\n", vrIsTrailingZeros ? "true" : "false");
#endif

  // Step 4: Find the shortest decimal representation in the interval of valid representations.
  int32_t removed = 0;
  uint32_t output;
  if (vmIsTrailingZeros || vrIsTrailingZeros) {
    // General case, which happens rarely (~4.0%).
    while (vp / 10 > vm / 10) {
#ifdef __clang__ // https://bugs.llvm.org/show_bug.cgi?id=23106
      // The compiler does not realize that vm % 10 can be computed from vm / 10
      // as vm - (vm / 10) * 10.
      vmIsTrailingZeros &= vm - (vm / 10) * 10 == 0;
#else
      vmIsTrailingZeros &= vm % 10 == 0;
#endif
      vrIsTrailingZeros &= lastRemovedDigit == 0;
      lastRemovedDigit = (uint8_t) (vr % 10);
      vr /= 10;
      vp /= 10;
      vm /= 10;
      ++removed;
    }
#ifdef RYU_DEBUG
    printf("V+=%u\nV =%u\nV-=%u\n", vp, vr, vm);
    printf("d-10=%s\n", vmIsTrailingZeros ? "true" : "false");
#endif
    if (vmIsTrailingZeros) {
      while (vm % 10 == 0) {
        vrIsTrailingZeros &= lastRemovedDigit == 0;
        lastRemovedDigit = (uint8_t) (vr % 10);
        vr /= 10;
        vp /= 10;
        vm /= 10;
        ++removed;
      }
    }
#ifdef RYU_DEBUG
    printf("%u %d\n", vr, lastRemovedDigit);
    printf("vr is trailing zeros=%s\n", vrIsTrailingZeros ? "true" : "false");
#endif
    if (vrIsTrailingZeros && lastRemovedDigit == 5 && vr % 2 == 0) {
      // Round even if the exact number is .....50..0.
      lastRemovedDigit = 4;
    }
    // We need to take vr + 1 if vr is outside bounds or we need to round up.
    output = vr + ((vr == vm && (!acceptBounds || !vmIsTrailingZeros)) || lastRemovedDigit >= 5);
  } else {
    // Specialized for the common case (~96.0%). Percentages below are relative to this.
    // Loop iterations below (approximately):
    // 0: 13.6%, 1: 70.7%, 2: 14.1%, 3: 1.39%, 4: 0.14%, 5+: 0.01%
    while (vp / 10 > vm / 10) {
      lastRemovedDigit = (uint8_t) (vr % 10);
      vr /= 10;
      vp /= 10;
      vm /= 10;
      ++removed;
    }
#ifdef RYU_DEBUG
    printf("%u %d\n", vr, lastRemovedDigit);
    printf("vr is trailing zeros=%s\n", vrIsTrailingZeros ? "true" : "false");
#endif
    // We need to take vr + 1 if vr is outside bounds or we need to round up.
    output = vr + (vr == vm || lastRemovedDigit >= 5);
  }
  const int32_t exp = e10 + removed;

#ifdef RYU_DEBUG
  printf("V+=%u\nV =%u\nV-=%u\n", vp, vr, vm);
  printf("O=%u\n", output);
  printf("EXP=%d\n", exp);
#endif

  floating_decimal_32 fd;
  fd.exponent = exp;
  fd.mantissa = output;
  return fd;
}

#if 0
/*k8: old code*/
static int to_chars(const floating_decimal_32 v, const bool sign, char* const result) {
  // Step 5: Print the decimal representation.
  int index = 0;
  if (sign) {
    result[index++] = '-';
  }

  uint32_t output = v.mantissa;
  const uint32_t olength = decimalLength9(output);

#ifdef RYU_DEBUG
  printf("DIGITS=%u\n", v.mantissa);
  printf("OLEN=%u\n", olength);
  printf("EXP=%u\n", v.exponent + olength);
#endif

  // Print the decimal digits.
  // The following code is equivalent to:
  // for (uint32_t i = 0; i < olength - 1; ++i) {
  //   const uint32_t c = output % 10; output /= 10;
  //   result[index + olength - i] = (char) ('0' + c);
  // }
  // result[index] = '0' + output % 10;
  uint32_t i = 0;
  while (output >= 10000) {
#ifdef __clang__ // https://bugs.llvm.org/show_bug.cgi?id=38217
    const uint32_t c = output - 10000 * (output / 10000);
#else
    const uint32_t c = output % 10000;
#endif
    output /= 10000;
    const uint32_t c0 = (c % 100) << 1;
    const uint32_t c1 = (c / 100) << 1;
    memcpy(result + index + olength - i - 1, DIGIT_TABLE + c0, 2);
    memcpy(result + index + olength - i - 3, DIGIT_TABLE + c1, 2);
    i += 4;
  }
  if (output >= 100) {
    const uint32_t c = (output % 100) << 1;
    output /= 100;
    memcpy(result + index + olength - i - 1, DIGIT_TABLE + c, 2);
    i += 2;
  }
  if (output >= 10) {
    const uint32_t c = output << 1;
    // We can't use memcpy here: the decimal dot goes between these two digits.
    result[index + olength - i] = DIGIT_TABLE[c + 1];
    result[index] = DIGIT_TABLE[c];
  } else {
    result[index] = (char) ('0' + output);
  }

  // Print decimal point if needed.
  if (olength > 1) {
    result[index + 1] = '.';
    index += olength + 1;
  } else {
    ++index;
  }

  // Print the exponent.
  result[index++] = 'e';
  int32_t exp = v.exponent + (int32_t) olength - 1;

  if (exp < 0) {
    result[index++] = '-';
    exp = -exp;
  }

  if (exp >= 10) {
    memcpy(result + index, DIGIT_TABLE + 2 * exp, 2);
    index += 2;
  } else {
    result[index++] = (char) ('0' + exp);
  }

  return index;
}
#else
// step 5: print the decimal representation
// k8: i heavily modified this one
static int to_chars (const floating_decimal_32 v, const bool sign, char* const result) {
  uint32_t output = v.mantissa;
  const int32_t olength = decimalLength9(output);

#ifdef RYU_DEBUG
  printf("DIGITS=%u\n", v.mantissa);
  printf("OLEN=%d\n", olength);
  printf("EXP=%d\n", v.exponent+olength);
#endif

  // convert decimal to string, without dots and sign
  char dbuf[16]; // max is 9
  char *dbpos = dbuf+sizeof(dbuf)-1;
  *dbpos = 0;
  while (output >= 10000) {
    const uint32_t c = output%10000;
    output /= 10000;
    const uint32_t c0 = (c%100)<<1;
    const uint32_t c1 = (c/100)<<1;
    dbpos -= 4;
    memcpy(dbpos+0, DIGIT_TABLE+c1, 2);
    memcpy(dbpos+2, DIGIT_TABLE+c0, 2);
  }
  if (output >= 100) {
    const uint32_t c = (output%100)<<1;
    output /= 100;
    dbpos -= 2;
    memcpy(dbpos, DIGIT_TABLE+c, 2);
  }
  if (output >= 10) {
    const uint32_t c = output<<1;
    dbpos -= 2;
    memcpy(dbpos, DIGIT_TABLE+c, 2);
  } else {
    dbpos -= 1;
    *dbpos = (char)('0'+output);
  }

  char *res = result;
  if (sign) *res++ = '-';

  // if we don't need any exponent, simply copy the result, and get out
  int32_t exp = v.exponent;
  if (exp == 0) {
    memcpy(res, dbpos, (unsigned)olength);
    return olength+sign;
  }

  // if we can move decimal point (and/or append zeroes), and still fit into 15 chars, do it
  const int32_t maxlen = 15-sign; // minus one byte for sign

  // can we just append zeroes, and go with it?
  if (exp > 0 && exp < maxlen && olength+exp <= maxlen) {
    memcpy(res, dbpos, (unsigned)olength);
    memset(res+olength, '0', exp);
    return olength+sign+exp;
  }

  if (exp < 0) {
    // can we simply put a dot where it belongs?
    const int32_t pexp = -exp;
    if (pexp < olength && olength+1 <= maxlen) {
      // we can insert a dot
      const int32_t beforedot = olength+exp;
      assert(beforedot > 0);
      memcpy(res, dbpos, (unsigned)beforedot);
      res += beforedot;
      dbpos += beforedot;
      *res++ = '.';
      // rest of the number
      memcpy(res, dbpos, (unsigned)pexp);
      res += pexp;
      return (int)(ptrdiff_t)(res-result);
    }

    // can we pad with dot and zeroes?
    if (pexp >= olength && pexp+2 <= maxlen) {
      // yeah, there is enough room, so do it
      *res++ = '0';
      *res++ = '.';
      const unsigned zcount = (unsigned)(pexp-olength);
      if (zcount) { memset(res, '0', zcount); res += zcount; }
      memcpy(res, dbpos, olength);
      res += olength;
      return (int)(ptrdiff_t)(res-result);
    }
  }

  // use scientific notation
  if (olength == 1) {
    *res++ = *dbpos;
  } else {
    exp += (int)olength-1;
    *res++ = *dbpos++;
    *res++ = '.';
    memcpy(res, dbpos, olength-1);
    res += olength-1;
  }

  if (exp != 0) {
    // add exponent
    *res++ = 'e';
    if (exp < 0) { *res++ = '-'; exp = -exp; } else *res++ = '+';
    if (exp >= 10) {
      memcpy(res, DIGIT_TABLE+2*exp, 2);
      res += 2;
    } else {
      *res++ = (char)('0'+exp);
    }
  }

  return (int)(ptrdiff_t)(res-result);
}
#endif


int f2s_buffered_n(float f, char* result) {
  // Step 1: Decode the floating-point number, and unify normalized and subnormal cases.
  const uint32_t bits = float_to_bits(f);

#ifdef RYU_DEBUG
  printf("IN=");
  for (int32_t bit = 31; bit >= 0; --bit) {
    printf("%u", (bits >> bit) & 1);
  }
  printf("\n");
#endif

  // Decode bits into sign, mantissa, and exponent.
  const bool ieeeSign = ((bits >> (FLOAT_MANTISSA_BITS + FLOAT_EXPONENT_BITS)) & 1) != 0;
  const uint32_t ieeeMantissa = bits & ((1u << FLOAT_MANTISSA_BITS) - 1);
  const uint32_t ieeeExponent = (bits >> FLOAT_MANTISSA_BITS) & ((1u << FLOAT_EXPONENT_BITS) - 1);

  // Case distinction; exit early for the easy cases.
  if (ieeeExponent == ((1u << FLOAT_EXPONENT_BITS) - 1u) || (ieeeExponent == 0 && ieeeMantissa == 0)) {
    return copy_special_str(result, ieeeSign, ieeeExponent, ieeeMantissa);
  }

  const floating_decimal_32 v = f2d(ieeeMantissa, ieeeExponent);
  return to_chars(v, ieeeSign, result);
}

int f2s_buffered(float f, char* result) {
  const int index = f2s_buffered_n(f, result);

  // Terminate the string.
  result[index] = '\0';
  return index;
}

/*
char* f2s(float f) {
  char* const result = (char*) malloc(16);
  f2s_buffered(f, result);
  return result;
}
*/


/******************************************************************************/
#define DBL_MANT_EXTRA_BIT  (0x0010000000000000UL)
#define DBL_MANT_MASK       (0x000FFFFFFFFFFFFFUL)
#define DBL_SIGN_MASK       (0x8000000000000000UL)
#define DBL_EXP_SHIFT       (52)
#define DBL_EXP_SMASK       (0x7FFU)
#define DBL_EXP_OFFSET      (1023)


static inline double make_inf_dbl (int negative) {
  uint64_t uvinf = 0x7FF0000000000000UL;
  if (negative) uvinf |= DBL_SIGN_MASK;
  return *(const __attribute__((__may_alias__)) double *)&uvinf;
}

static inline double make_zero_dbl (int negative) {
  if (!negative) return 0.0;
  const uint64_t uvneg0 = 0x8000000000000000UL;
  return *(const __attribute__((__may_alias__)) double *)&uvneg0;
}


static double ldexp_intr_dbl (const double d, int pwr) {
  if (pwr == 0) return d;
  uint64_t n = *(const __attribute__((__may_alias__)) uint64_t *)&d;
  int exp = (n>>DBL_EXP_SHIFT)&DBL_EXP_SMASK;
  if (exp == 0 || exp == DBL_EXP_SMASK) return d;
  if (pwr < 0) {
    if (pwr < -((int)(DBL_EXP_SMASK-1))) return make_zero_dbl((n&DBL_SIGN_MASK) != 0);
    if ((exp += pwr) <= 0) return make_zero_dbl((n&DBL_SIGN_MASK) != 0);
  } else {
    if (pwr > (int)DBL_EXP_SMASK-1) return make_inf_dbl((n&DBL_SIGN_MASK) != 0);
    //k8: i am unsure about `int` here
    if ((exp += pwr) >= (int)DBL_EXP_SMASK) return make_inf_dbl((n&DBL_SIGN_MASK) != 0);
  }
  n &= DBL_MANT_MASK|DBL_SIGN_MASK;
  n |= ((uint64_t)(exp&DBL_EXP_SMASK))<<DBL_EXP_SHIFT;
  return *(const __attribute__((__may_alias__)) double *)&n;
}


#define FLOAT_MANTISSA_BITS  23
#define FLOAT_EXPONENT_BITS  8
#define FLOAT_EXPONENT_BIAS  127
#define FLOAT_EXPONENT_MASK  0xff
#define FLOAT_MANTISSA_MASK  0x7fffffu
#define FLOAT_SIGN_MASK      0x80000000u
#define FLOAT_QUIETNAN_MASK  0x400000u


static inline __attribute__((always_inline)) uint32_t floor_log2 (const uint32_t value) {
  return 31-__builtin_clz(value);
}


static inline __attribute__((always_inline)) int32_t max32 (const int32_t a, const int32_t b) {
  return (a < b ? b : a);
}

static inline __attribute__((always_inline)) float int32Bits2Float (const uint32_t bits) {
  return *(const __attribute__((__may_alias__)) float *)&bits;
}


static inline float make_inf_flt (int negative) {
  uint32_t uinf = (FLOAT_EXPONENT_MASK<<FLOAT_MANTISSA_BITS);
  if (negative) uinf |= FLOAT_SIGN_MASK;
  return int32Bits2Float(uinf);
}


static inline float make_nan_flt (int negative, int signaling, uint32_t payload) {
  payload &= (FLOAT_MANTISSA_MASK&~FLOAT_QUIETNAN_MASK);
  if (!signaling) payload |= FLOAT_QUIETNAN_MASK;
  payload |= (FLOAT_EXPONENT_MASK<<FLOAT_MANTISSA_BITS);
  if (negative) payload |= FLOAT_SIGN_MASK;
  return int32Bits2Float(payload);
}


static inline float make_zero_flt (int negative) {
  uint32_t uzero = 0u;
  if (negative) uzero |= FLOAT_SIGN_MASK;
  return int32Bits2Float(uzero);
}


static float ldexp_intr_flt (const float f, int pwr) {
  if (pwr == 0) return f;
  uint32_t n = *(const __attribute__((__may_alias__)) uint32_t *)&f;
  int exp = (n>>FLOAT_MANTISSA_BITS)&FLOAT_EXPONENT_MASK;
  if (exp == 0 || exp == FLOAT_EXPONENT_MASK) return f; // zero, denormal, inf/nan
  if (pwr < 0) {
    if (pwr < -((int)(FLOAT_EXPONENT_MASK-1))) return (float)ldexp_intr_dbl((double)f, pwr); // precision loss
    if ((exp += pwr) <= 0) return (float)ldexp_intr_dbl((double)f, pwr); // precision loss (denormal)
  } else {
    if (pwr > (int)FLOAT_EXPONENT_MASK-1) return make_inf_flt(n&FLOAT_SIGN_MASK);
    if ((exp += pwr) >= (int)FLOAT_EXPONENT_MASK) return make_inf_flt(n&FLOAT_SIGN_MASK);
  }
  n &= FLOAT_MANTISSA_MASK|FLOAT_SIGN_MASK;
  n |= ((uint32_t)(exp&FLOAT_EXPONENT_MASK))<<FLOAT_MANTISSA_BITS;
  return *(const __attribute__((__may_alias__)) float *)&n;
}


static inline int hexDigit (const int ch) {
  return
    ch >= '0' && ch <= '9' ? ch-'0' :
    ch >= 'A' && ch <= 'F' ? ch-'A'+10 :
    ch >= 'a' && ch <= 'f' ? ch-'a'+10 :
    -1;
}


static inline int isGoodTrailingChar (const char ch) {
  return
    ch == 'f' || ch == 'F' ||
    ch == 'l' || ch == 'L';
}


//==========================================================================
//
//  ryu_s2f
//
//==========================================================================
int ryu_s2f (const char *buffer, int len, float *result, const unsigned flags) {
  // default value: quiet positive nan with all bits set
  if (result) *result = make_nan_flt(0, 0, FLOAT_MANTISSA_MASK);

  if (!buffer || !result || len < 0) return RYU_INVALID_ARGS;

  // trim spaces
  while (len > 0 && (uint8_t)buffer[0] <= 32) { ++buffer; --len; }
  while (len > 0 && (uint8_t)buffer[len-1] <= 32) --len;

  if (len == 0) return RYU_INPUT_TOO_SHORT;
  if (len > 256) return RYU_INPUT_TOO_LONG;

  int m10digits = 0;
  int e10digits = 0;
  int dotIndex = len;
  int eIndex = len;
  uint32_t m10 = 0;
  int32_t e10 = 0;
  bool signedM = false;
  bool signedE = false;

  int i = 0;
  if (buffer[i] == '-') {
    signedM = true;
    ++i;
  } else if (buffer[i] == '+') {
    ++i;
  }

  // inf/nan
  if (len-i >= 3) {
    if ((buffer[i+0] == 'i' || buffer[i+0] == 'I') &&
        (buffer[i+1] == 'n' || buffer[i+1] == 'N') &&
        (buffer[i+2] == 'f' || buffer[i+2] == 'F'))
    {
      if ((flags&RYU_FLAG_ALLOW_INF) == 0) return RYU_MALFORMED_INPUT;
      if (len-i != 3) return RYU_MALFORMED_INPUT;
      *result = make_inf_flt(signedM);
      return RYU_SUCCESS;
    }
    if ((buffer[i+0] == 'n' || buffer[i+0] == 'N') &&
        (buffer[i+1] == 'a' || buffer[i+1] == 'A') &&
        (buffer[i+2] == 'n' || buffer[i+2] == 'N'))
    {
      if ((flags&RYU_FLAG_ALLOW_NAN) == 0) return RYU_MALFORMED_INPUT;
      if (len-i != 3) return RYU_MALFORMED_INPUT;
      *result = make_nan_flt(signedM, 0, 0);
      return RYU_SUCCESS;
    }
  }

  int wasDigits = 0;

  // 0x literals
  // this cannot parse denormals for now
  if (len-i >= 2 && buffer[i] == '0' && (buffer[i+1] == 'x' || buffer[i+1] == 'X')) {
    i += 2;
    if (i >= len || hexDigit(buffer[i]) < 0) return RYU_MALFORMED_INPUT;
    int ftype = -1; // 0: zero; 1: normalized
    int collexp = 0; // *binary* digits after the dot
    // collect into float; it is ok, as we are operating powers of two, not decimals
    float fv = 0.0f;
    while (i < len) {
      const char ch = buffer[i];
      if (wasDigits && ch == '_') { ++i; continue; }
      if (ch == '.') {
        if (ftype >= 0) return RYU_MALFORMED_INPUT; // double dot, wtf?!
        ftype = (fv != 0.0f);
      } else {
        const int hd = hexDigit(ch);
        if (hd < 0) break;
        wasDigits = 1;
        if (ftype == 0) {
          if (hd != 0) return RYU_MALFORMED_INPUT; // denormalised numbers aren't supported
        } else {
          // check for overflow here
          fv = fv*16.0f;
          fv += (float)hd;
          if (ftype > 0) collexp += 4;
        }
      }
      ++i;
    }
    if (!wasDigits) return RYU_MALFORMED_INPUT;
    // parse exponent
    int exp = 0;
    int expneg = 0;
    if (i < len && (buffer[i] == 'p' || buffer[i] == 'P')) {
      ++i;
      if (i >= len) return RYU_MALFORMED_INPUT;
      wasDigits = 0;
      switch (buffer[i]) {
        case '-':
          expneg = 1;
          /* fallthrough */
        case '+':
          ++i;
          break;
      }
      while (i < len) {
        const char ch = buffer[i];
        if (wasDigits && ch == '_') { ++i; continue; }
        if (ch < '0' || ch > '9') break;
        wasDigits = 1;
        exp = exp*10+(ch-'0');
        if (exp > 0x00ffffff) exp = 0x0ffffff;
        ++i;
      }
      if (!wasDigits) return RYU_MALFORMED_INPUT;
    }
    // done
    // skip trailing 'f' or 'l', if any
    if (i < len && isGoodTrailingChar(buffer[i])) ++i;
    if (i < len) return RYU_MALFORMED_INPUT;
    // zero?
    if (ftype == 0 || fv == 0.0f) {
      *result = make_zero_flt(signedM);
      return RYU_SUCCESS;
    }
    if (expneg) exp = -exp;
    exp -= collexp;
    if (signedM) fv = -fv;
    //fprintf(stderr, "ftype:%d; collexp:%d; exp:%d; expneg:%d; fv=%g : %a (0x%08x)\n", ftype, collexp, exp, expneg, fv, fv, *(const __attribute__((__may_alias__)) uint32_t *)&fv);
    if (exp != 0) fv = ldexp_intr_flt(fv, exp);
    *result = fv;
    return RYU_SUCCESS;
  }

  // normal numbers
  wasDigits = 0;
  int e10add = 0;
  for (; i < len; ++i) {
    char c = buffer[i];
    if (wasDigits && c == '_') continue;
    if (c == '.') {
      if (dotIndex != len) return RYU_MALFORMED_INPUT;
      dotIndex = i;
      continue;
    }
    if (c < '0' || c > '9') break;
    if (wasDigits < 0) {
      // too big
      ++e10add;
      //if (e10add > 128) return RYU_INPUT_TOO_LONG;
      continue;
    }
    wasDigits = 1;
    if (m10digits >= 9) {
      //return RYU_INPUT_TOO_LONG;
      //k8: this is a hack!
      if (c >= '5') m10 += 1;
      //m10 = 999999999;
      wasDigits = -1;
      ++e10add;
      continue;
    }
    m10 = 10*m10+(c-'0');
    if (m10 != 0) ++m10digits;
  }

  if (!wasDigits) return RYU_MALFORMED_INPUT;

  int expTooBig = 0;
  if (i < len && (buffer[i] == 'e' || buffer[i] == 'E')) {
    eIndex = i;
    ++i;
    if (i < len && (buffer[i] == '-' || buffer[i] == '+')) {
      signedE = (buffer[i] == '-');
      ++i;
    }
    wasDigits = 0;
    for (; i < len; ++i) {
      char c = buffer[i];
      if (wasDigits && c == '_') continue;
      if (c < '0' || c > '9') break;
      wasDigits = 1;
      if (expTooBig) continue;
      if (e10digits > 3) {
        expTooBig = 1;
      } else {
        e10 = 10*e10+(c-'0');
        if (e10 != 0) ++e10digits;
      }
    }
    if (!wasDigits) return RYU_MALFORMED_INPUT;
  }
  // skip trailing 'f' or 'l', if any
  if (i < len && isGoodTrailingChar(buffer[i])) ++i;
  if (i < len) return RYU_MALFORMED_INPUT;

  // if exponent is too big, return +/-Infinity or +/-0 instead
  if (expTooBig) {
    e10 = (signedE ? -69666 : 69666); // this guarantees proper rounding, etc.
  } else {
    if (signedE) e10 = -e10;
  }
  e10 -= (dotIndex < eIndex ? eIndex-dotIndex-1 : 0);

  e10 += e10add; // hack!
  /*
  if (m10 == 0) {
    *result = (signedM ? -0.0f : 0.0f);
    return RYU_SUCCESS;
  }
  */

#ifdef RYU_DEBUG
  printf("Input=%s\n", buffer);
  printf("m10digits = %d\n", m10digits);
  printf("e10digits = %d\n", e10digits);
  printf("m10 * 10^e10 = %u * 10^%d\n", m10, e10);
#endif

  if (m10digits+e10 <= -46 || m10 == 0) {
    // Number is less than 1e-46, which should be rounded down to 0; return +/-0.0
    *result = make_zero_flt(signedM);
    return RYU_SUCCESS;
  }
  if (m10digits+e10 >= 40) {
    // Number is larger than 1e+39, which should be rounded to +/-Infinity
    *result = make_inf_flt(signedM);
    return RYU_SUCCESS;
  }

  // Convert to binary float m2 * 2^e2, while retaining information about whether the conversion
  // was exact (trailingZeros).
  int32_t e2;
  uint32_t m2;
  bool trailingZeros;
  if (e10 >= 0) {
    // The length of m * 10^e in bits is:
    //   log2(m10 * 10^e10) = log2(m10) + e10 log2(10) = log2(m10) + e10 + e10 * log2(5)
    //
    // We want to compute the FLOAT_MANTISSA_BITS + 1 top-most bits (+1 for the implicit leading
    // one in IEEE format). We therefore choose a binary output exponent of
    //   log2(m10 * 10^e10) - (FLOAT_MANTISSA_BITS + 1).
    //
    // We use floor(log2(5^e10)) so that we get at least this many bits; better to
    // have an additional bit than to not have enough bits.
    e2 = floor_log2(m10)+e10+log2pow5(e10)-(FLOAT_MANTISSA_BITS+1);

    // We now compute [m10 * 10^e10 / 2^e2] = [m10 * 5^e10 / 2^(e2-e10)].
    // To that end, we use the FLOAT_POW5_SPLIT table.
    const int j = e2-e10-ceil_log2pow5(e10)+FLOAT_POW5_BITCOUNT;
    assert(j >= 0);
    m2 = mulPow5divPow2(m10, e10, j);

    // We also compute if the result is exact, i.e.,
    //   [m10 * 10^e10 / 2^e2] == m10 * 10^e10 / 2^e2.
    // This can only be the case if 2^e2 divides m10 * 10^e10, which in turn requires that the
    // largest power of 2 that divides m10 + e10 is greater than e2. If e2 is less than e10, then
    // the result must be exact. Otherwise we use the existing multipleOfPowerOf2 function.
    trailingZeros = e2 < e10 || (e2-e10 < 32 && multipleOfPowerOf2_32(m10, e2-e10));
  } else {
    e2 = floor_log2(m10)+e10-ceil_log2pow5(-e10)-(FLOAT_MANTISSA_BITS+1);

    // We now compute [m10 * 10^e10 / 2^e2] = [m10 / (5^(-e10) 2^(e2-e10))].
    int j = e2-e10+ceil_log2pow5(-e10)-1+FLOAT_POW5_INV_BITCOUNT;
    m2 = mulPow5InvDivPow2(m10, -e10, j);

    // We also compute if the result is exact, i.e.,
    //   [m10 / (5^(-e10) 2^(e2-e10))] == m10 / (5^(-e10) 2^(e2-e10))
    //
    // If e2-e10 >= 0, we need to check whether (5^(-e10) 2^(e2-e10)) divides m10, which is the
    // case iff pow5(m10) >= -e10 AND pow2(m10) >= e2-e10.
    //
    // If e2-e10 < 0, we have actually computed [m10 * 2^(e10 e2) / 5^(-e10)] above,
    // and we need to check whether 5^(-e10) divides (m10 * 2^(e10-e2)), which is the case iff
    // pow5(m10 * 2^(e10-e2)) = pow5(m10) >= -e10.
    trailingZeros = (e2 < e10 || (e2-e10 < 32 && multipleOfPowerOf2_32(m10, e2-e10)))
        && multipleOfPowerOf5_32(m10, -e10);
  }

#ifdef RYU_DEBUG
  printf("m2 * 2^e2 = %u * 2^%d\n", m2, e2);
#endif

  // Compute the final IEEE exponent.
  uint32_t ieee_e2 = (uint32_t) max32(0, e2+FLOAT_EXPONENT_BIAS+floor_log2(m2));

  if (ieee_e2 > 0xfe) {
    // Final IEEE exponent is larger than the maximum representable; return +/-Infinity.
    *result = make_inf_flt(signedM);
    return RYU_SUCCESS;
  }

  // We need to figure out how much we need to shift m2. The tricky part is that we need to take
  // the final IEEE exponent into account, so we need to reverse the bias and also special-case
  // the value 0.
  int32_t shift = (ieee_e2 == 0 ? 1 : ieee_e2)-e2-FLOAT_EXPONENT_BIAS-FLOAT_MANTISSA_BITS;
  assert(shift >= 0);
#ifdef RYU_DEBUG
  printf("ieee_e2 = %d\n", ieee_e2);
  printf("shift = %d\n", shift);
#endif

  // We need to round up if the exact value is more than 0.5 above the value we computed. That's
  // equivalent to checking if the last removed bit was 1 and either the value was not just
  // trailing zeros or the result would otherwise be odd.
  //
  // We need to update trailingZeros given that we have the exact output exponent ieee_e2 now.
  trailingZeros &= (m2&((1u<<(shift-1))-1)) == 0;
  uint32_t lastRemovedBit = (m2>>(shift-1))&1;
  const bool roundUp = (lastRemovedBit != 0) && (!trailingZeros || (((m2>>shift)&1) != 0));

#ifdef RYU_DEBUG
  printf("roundUp = %d\n", roundUp);
  printf("ieee_m2 = %u\n", (m2>>shift)+roundUp);
#endif
  uint32_t ieee_m2 = (m2>>shift)+roundUp;
  assert(ieee_m2 <= (1u<<(FLOAT_MANTISSA_BITS+1)));
  ieee_m2 &= (1u<<FLOAT_MANTISSA_BITS)-1;
  if (ieee_m2 == 0 && roundUp) {
    // Rounding up may overflow the mantissa.
    // In this case we move a trailing zero of the mantissa into the exponent.
    // Due to how the IEEE represents +/-Infinity, we don't need to check for overflow here.
    ++ieee_e2;
  }
  uint32_t ieee = (((((uint32_t)signedM)<<FLOAT_EXPONENT_BITS)|(uint32_t)ieee_e2)<<FLOAT_MANTISSA_BITS)|ieee_m2;
  *result = int32Bits2Float(ieee);
  return RYU_SUCCESS;
}
