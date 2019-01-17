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

// maximum result length is 15 bytes (plus one byte for terminating zero, if necessary)
#define F2S_MAX_BUFFER_SIZE  (16)

// returns length, doesn't put terminating zero; 15 bytes of buffer is enough
static int f2s_buffered_n (float f, char *result);
// returns length without terminating zero, but puts that zero; 16 bytes of buffer is enough
static __attribute__((unused)) int f2s_buffered (float f, char *result);

#include <assert.h>
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

#if defined(_M_IX86) || defined(_M_ARM)
#define RYU_32_BIT_PLATFORM
#endif

// Returns e == 0 ? 1 : ceil(log_2(5^e)).
static inline int32_t pow5bits(const int32_t e) {
  // This approximation works up to the point that the multiplication overflows at e = 3529.
  // If the multiplication were done in 64 bits, it would fail at 5^4004 which is just greater
  // than 2^9297.
  assert(e >= 0);
  assert(e <= 3528);
  return (int32_t) (((((uint32_t) e) * 1217359) >> 19) + 1);
}

// Returns floor(log_10(2^e)).
static inline uint32_t log10Pow2(const int32_t e) {
  // The first value this approximation fails for is 2^1651 which is just greater than 10^297.
  assert(e >= 0);
  assert(e <= 1650);
  return (((uint32_t) e) * 78913) >> 18;
}

// Returns floor(log_10(5^e)).
static inline uint32_t log10Pow5(const int32_t e) {
  // The first value this approximation fails for is 5^2621 which is just greater than 10^1832.
  assert(e >= 0);
  assert(e <= 2620);
  return (((uint32_t) e) * 732923) >> 20;
}

static inline int copy_special_str(char * const result, const bool sign, const bool exponent, const bool mantissa) {
  if (mantissa) {
    memcpy(result, "NaN", 3);
    return 3;
  }
  if (exponent) {
    result[0] = (sign ? '-' : '+');
    memcpy(result + 1, "Inf", 3);
    return 1 + 3;
  }
  memcpy(result, "0", 1);
  return 1;
}

static inline uint32_t float_to_bits(const float f) {
  uint32_t bits = 0;
  memcpy(&bits, &f, sizeof(float));
  return bits;
}

/*
static inline uint64_t double_to_bits(const double d) {
  uint64_t bits = 0;
  memcpy(&bits, &d, sizeof(double));
  return bits;
}
*/

#define FLOAT_MANTISSA_BITS 23
#define FLOAT_EXPONENT_BITS 8
#define FLOAT_BIAS 127

// This table is generated by PrintFloatLookupTable.
#define FLOAT_POW5_INV_BITCOUNT 59
static const uint64_t FLOAT_POW5_INV_SPLIT[31] = {
  576460752303423489u, 461168601842738791u, 368934881474191033u, 295147905179352826u,
  472236648286964522u, 377789318629571618u, 302231454903657294u, 483570327845851670u,
  386856262276681336u, 309485009821345069u, 495176015714152110u, 396140812571321688u,
  316912650057057351u, 507060240091291761u, 405648192073033409u, 324518553658426727u,
  519229685853482763u, 415383748682786211u, 332306998946228969u, 531691198313966350u,
  425352958651173080u, 340282366920938464u, 544451787073501542u, 435561429658801234u,
  348449143727040987u, 557518629963265579u, 446014903970612463u, 356811923176489971u,
  570899077082383953u, 456719261665907162u, 365375409332725730u
};
#define FLOAT_POW5_BITCOUNT 61
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


static inline uint32_t pow5Factor (uint32_t value) {
  uint32_t count = 0;
  for (;;) {
    assert(value != 0);
    const uint32_t q = value/5;
    const uint32_t r = value%5;
    if (r != 0) {
      break;
    }
    value = q;
    ++count;
  }
  return count;
}


// returns true if value is divisible by 5^p
static inline bool multipleOfPowerOf5 (const uint32_t value, const uint32_t p) {
  return pow5Factor(value) >= p;
}


// returns true if value is divisible by 2^p
static inline bool multipleOfPowerOf2 (const uint32_t value, const uint32_t p) {
  // return __builtin_ctz(value) >= p;
  return (value&((1u<<p)-1)) == 0;
}


static inline uint32_t mulShift (const uint32_t m, const uint64_t factor, const int32_t shift) {
  assert(shift > 32);

  // the casts here help MSVC to avoid calls to the __allmul library function
  const uint32_t factorLo = (uint32_t)(factor);
  const uint32_t factorHi = (uint32_t)(factor>>32);
  const uint64_t bits0 = (uint64_t)m*factorLo;
  const uint64_t bits1 = (uint64_t)m*factorHi;

#ifdef RYU_32_BIT_PLATFORM
  // on 32-bit platforms we can avoid a 64-bit shift-right since we only
  // need the upper 32 bits of the result and the shift value is > 32.
  const uint32_t bits0Hi = (uint32_t)(bits0>>32);
  uint32_t bits1Lo = (uint32_t)(bits1);
  uint32_t bits1Hi = (uint32_t)(bits1>>32);
  bits1Lo += bits0Hi;
  bits1Hi += (bits1Lo < bits0Hi);
  const int32_t s = shift-32;
  return (bits1Hi<<(32-s))|(bits1Lo>>s);
#else // RYU_32_BIT_PLATFORM
  const uint64_t sum = (bits0>>32)+bits1;
  const uint64_t shiftedSum = sum>>(shift-32);
  assert(shiftedSum <= UINT32_MAX);
  return (uint32_t)shiftedSum;
#endif // RYU_32_BIT_PLATFORM
}


static inline uint32_t mulPow5InvDivPow2 (const uint32_t m, const uint32_t q, const int32_t j) {
  return mulShift(m, FLOAT_POW5_INV_SPLIT[q], j);
}


static inline uint32_t mulPow5divPow2 (const uint32_t m, const uint32_t i, const int32_t j) {
  return mulShift(m, FLOAT_POW5_SPLIT[i], j);
}


static inline int32_t decimalLength (const uint32_t v) {
  // function precondition: v is not a 10-digit number
  // (9 digits are sufficient for round-tripping)
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


// a floating decimal representing m*10^e
typedef struct floating_decimal_32 {
  uint32_t mantissa;
  int32_t exponent;
} floating_decimal_32;


//k8: this returns exponent for integral mantissa, not for some implied dot
static inline floating_decimal_32 f2d (const uint32_t ieeeMantissa, const uint32_t ieeeExponent) {
  int32_t e2;
  uint32_t m2;
  if (ieeeExponent == 0) {
    // we subtract 2 so that the bounds computation has 2 additional bits
    e2 = 1-FLOAT_BIAS-FLOAT_MANTISSA_BITS-2;
    m2 = ieeeMantissa;
  } else {
    e2 = (int32_t)ieeeExponent-FLOAT_BIAS-FLOAT_MANTISSA_BITS-2;
    m2 = (1u<<FLOAT_MANTISSA_BITS)|ieeeMantissa;
  }
  const bool even = (m2&1) == 0;
  const bool acceptBounds = even;

#ifdef RYU_DEBUG
  printf("-> %u * 2^%d\n", m2, e2+2);
#endif

  // step 2: determine the interval of valid decimal representations
  const uint32_t mv = 4*m2;
  const uint32_t mp = 4*m2+2;
  // implicit bool -> int conversion: true is 1, false is 0
  const uint32_t mmShift = ieeeMantissa != 0 || ieeeExponent <= 1;
  const uint32_t mm = 4*m2-1-mmShift;

  // step 3: convert to a decimal power base using 64-bit arithmetic
  uint32_t vr, vp, vm;
  int32_t e10;
  bool vmIsTrailingZeros = false;
  bool vrIsTrailingZeros = false;
  uint8_t lastRemovedDigit = 0;
  if (e2 >= 0) {
    const uint32_t q = log10Pow2(e2);
    e10 = (int32_t)q;
    const int32_t k = FLOAT_POW5_INV_BITCOUNT+pow5bits((int32_t)q)-1;
    const int32_t i = -e2+(int32_t)q+k;
    vr = mulPow5InvDivPow2(mv, q, i);
    vp = mulPow5InvDivPow2(mp, q, i);
    vm = mulPow5InvDivPow2(mm, q, i);
#ifdef RYU_DEBUG
    printf("%u * 2^%d / 10^%u\n", mv, e2, q);
    printf("V+=%u\nV =%u\nV-=%u\n", vp, vr, vm);
#endif
    if (q != 0 && (vp-1)/10 <= vm/10) {
      // we need to know one removed digit even if we are not going to loop below.
      // we could use q = X - 1 above, except that would require 33 bits for the result, and
      // we've found that 32-bit arithmetic is faster even on 64-bit machines.
      const int32_t l = FLOAT_POW5_INV_BITCOUNT+pow5bits((int32_t)(q-1))-1;
      lastRemovedDigit = (uint8_t)(mulPow5InvDivPow2(mv, q-1, -e2+(int32_t)q-1+l)%10);
    }
    if (q <= 9) {
      // the largest power of 5 that fits in 24 bits is 5^10, but q <= 9 seems to be safe as well.
      // only one of mp, mv, and mm can be a multiple of 5, if any.
      if (mv%5 == 0) {
        vrIsTrailingZeros = multipleOfPowerOf5(mv, q);
      } else if (acceptBounds) {
        vmIsTrailingZeros = multipleOfPowerOf5(mm, q);
      } else {
        vp -= multipleOfPowerOf5(mp, q);
      }
    }
  } else {
    const uint32_t q = log10Pow5(-e2);
    e10 = (int32_t)q+e2;
    const int32_t i = -e2-(int32_t)q;
    const int32_t k = pow5bits(i)-FLOAT_POW5_BITCOUNT;
    int32_t j = (int32_t)q-k;
    vr = mulPow5divPow2(mv, (uint32_t)i, j);
    vp = mulPow5divPow2(mp, (uint32_t)i, j);
    vm = mulPow5divPow2(mm, (uint32_t)i, j);
#ifdef RYU_DEBUG
    printf("%u * 5^%d / 10^%u\n", mv, -e2, q);
    printf("%u %d %d %d\n", q, i, k, j);
    printf("V+=%u\nV =%u\nV-=%u\n", vp, vr, vm);
#endif
    if (q != 0 && (vp-1)/10 <= vm/10) {
      j = (int32_t)q-1-(pow5bits(i+1)-FLOAT_POW5_BITCOUNT);
      lastRemovedDigit = (uint8_t)(mulPow5divPow2(mv, (uint32_t)(i+1), j)%10);
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
      vrIsTrailingZeros = multipleOfPowerOf2(mv, q-1);
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

  // step 4: find the shortest decimal representation in the interval of valid representations
  int32_t removed = 0;
  uint32_t output;
  if (vmIsTrailingZeros || vrIsTrailingZeros) {
    // general case, which happens rarely (~4.0%)
    while (vp/10 > vm/10) {
      vmIsTrailingZeros &= vm%10 == 0;
      vrIsTrailingZeros &= lastRemovedDigit == 0;
      lastRemovedDigit = (uint8_t)(vr%10);
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
      while (vm%10 == 0) {
        vrIsTrailingZeros &= lastRemovedDigit == 0;
        lastRemovedDigit = (uint8_t)(vr%10);
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
    if (vrIsTrailingZeros && lastRemovedDigit == 5 && vr%2 == 0) {
      // round even if the exact number is .....50..0.
      lastRemovedDigit = 4;
    }
    // we need to take vr+1 if vr is outside bounds or we need to round up
    output = vr+((vr == vm && (!acceptBounds || !vmIsTrailingZeros)) || lastRemovedDigit >= 5);
  } else {
    // specialized for the common case (~96.0%); percentages below are relative to this
    // loop iterations below (approximately):
    // 0: 13.6%, 1: 70.7%, 2: 14.1%, 3: 1.39%, 4: 0.14%, 5+: 0.01%
    while (vp/10 > vm/10) {
      lastRemovedDigit = (uint8_t)(vr%10);
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
    output = vr+(vr == vm || lastRemovedDigit >= 5);
  }
  const int32_t exp = e10+removed;

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


// step 5: print the decimal representation
// k8: i heavily modified this one
static inline int to_chars (const floating_decimal_32 v, const bool sign, char* const result) {
  uint32_t output = v.mantissa;
  const int32_t olength = decimalLength(output);

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


static int f2s_buffered_n (float f, char *result) {
  // Step 1: Decode the floating-point number, and unify normalized and subnormal cases.
  const uint32_t bits = float_to_bits(f);

#ifdef RYU_DEBUG
  printf("IN=");
  for (int32_t bit = 31; bit >= 0; --bit) {
    printf("%u", (bits>>bit)&1);
  }
  printf("\n");
#endif

  // Decode bits into sign, mantissa, and exponent.
  const bool ieeeSign = ((bits>>(FLOAT_MANTISSA_BITS+FLOAT_EXPONENT_BITS))&1) != 0;
  const uint32_t ieeeMantissa = bits&((1u<<FLOAT_MANTISSA_BITS)-1);
  const uint32_t ieeeExponent = (bits>>FLOAT_MANTISSA_BITS)&((1u<<FLOAT_EXPONENT_BITS)-1);

  // Case distinction; exit early for the easy cases.
  if (ieeeExponent == ((1u<<FLOAT_EXPONENT_BITS)-1u) || (ieeeExponent == 0 && ieeeMantissa == 0)) {
    return copy_special_str(result, ieeeSign, ieeeExponent, ieeeMantissa);
  }

  const floating_decimal_32 v = f2d(ieeeMantissa, ieeeExponent);
  return to_chars(v, ieeeSign, result);
}


static __attribute__((unused)) int f2s_buffered (float f, char *result) {
  const int index = f2s_buffered_n(f, result);
  result[index] = '\0';
  return index;
}
