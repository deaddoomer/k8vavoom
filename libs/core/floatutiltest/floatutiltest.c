#define __STDC_WANT_IEC_60559_BFP_EXT__
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "/home/ketmar/back/vavoom_dev/src/libs/core/mathutil_float.h"

static char *f2s_bufs[16] = {NULL};
static unsigned f2s_currbuf = 0;


static const char *f2s (const float f, const char *fmt) {
  if (!f2s_bufs[f2s_currbuf]) f2s_bufs[f2s_currbuf] = malloc(1024);
  char *res = f2s_bufs[f2s_currbuf];
  f2s_currbuf = (f2s_currbuf+1)%16;
  strfromf(res, 1020, fmt, f);
  return res;
}


static void dumpFloat (const char *msg, const float f) {
  const uint32_t t = *(const uint32_t *)(&f);
  const float f1 = zeroNanInfDenormalsF(f);
  const uint32_t t1 = *(const uint32_t *)(&f1);
  const float f2 = zeroOnlyDenormalsF(f);
  const uint32_t t2 = *(const uint32_t *)(&f2);
  printf("=== %s -- float: %s (%s) : 0x%08x (%s : %s) : nodenorm: 0x%08x %s (%s) : onlynodenorm: 0x%08x %s (%s) ===\n", msg, f2s(f, "%f"), f2s(f, "%g"), t, f2s(killInfNaNF(f), "%f"), f2s(killInfNaNF(f), "%g"), t1, f2s(f1, "%f"), f2s(f1, "%g"), t2, f2s(f2, "%f"), f2s(f2, "%g"));
  printf("  sign: %d\n", fltconv_getsign(f));
  printf("  exponent: %u : %d\n", fltconv_getexponent(f), fltconv_getsignedexponent(f));
  printf("  mantissa: 0x%08x (%u)\n", fltconv_getmantissa(f), fltconv_getmantissa(f));
  printf("  isFiniteF   : %d\n", isFiniteF(f));
  printf("  isNaNF      : %d\n", isNaNF(f));
  printf("  isInfF      : %d\n", isInfF(f));
  printf("  isDenormalF : %d\n", isDenormalF(f));
  printf("  isZeroInfNaN: %d\n", isZeroInfNaN(f));
  printf("  isZeroF     : %d : %d\n", isZeroF(f), (int)(f == 0.0f));
  printf("  isNegativeF : %d : %d\n", isNegativeF(f), (int)(f < 0.0f));
  printf("  isPositiveF : %d : %d\n", isPositiveF(f), (int)(f > 0.0f));
  printf("  isLessZeroF    : %d : %d%s\n", isLessZeroF(f), (int)(f < 0.0f), (isLessZeroF(f) != (int)(f < 0.0f) ? "  !!!FAIL!!!" : ""));
  printf("  isGreatZeroF   : %d : %d%s\n", isGreatZeroF(f), (int)(f > 0.0f), (isGreatZeroF(f) != (int)(f > 0.0f) ? "  !!!FAIL!!!" : ""));
  printf("  isLessEquZeroF : %d : %d%s\n", isLessEquZeroF(f), (int)(f <= 0.0f), (isLessEquZeroF(f) != (int)(f <= 0.0f) ? "  !!!FAIL!!!" : ""));
  printf("  isGreatEquZeroF: %d : %d%s\n", isGreatEquZeroF(f), (int)(f >= 0.0f), (isGreatEquZeroF(f) != (int)(f >= 0.0f) ? "  !!!FAIL!!!" : ""));
  printf("\n");
}


int main () {
  dumpFloat("0.5f", 0.5f);
  dumpFloat("2.0f", 2.0f);
  dumpFloat("-0.5f", -0.5f);
  dumpFloat("-2.0f", -2.0f);
  float denorm = fltconv_constructfloat(1, 0, 1); // very small positive denormal
  dumpFloat("minimum positive denormal", denorm);
  denorm = fltconv_constructfloat(-1, 0, 1); // very small negative denormal
  dumpFloat("minimum negative denormal", denorm);
  denorm = fltconv_constructfloat(-1, 0, FLTCONV_NAN_MAX_MANTISSA); // maximum positive denornal
  dumpFloat("maximum positive denormal", denorm);
  float pz = fltconv_create_positive_zero();
  float nz = fltconv_create_negative_zero();
  dumpFloat("positive zero", pz);
  dumpFloat("negative zero", nz);
  dumpFloat("-1.0f", -1.0f);
  dumpFloat("+1.0f", +1.0f);
  dumpFloat("-INFINITY", -INFINITY);
  dumpFloat("+INFINITY", +INFINITY);
  dumpFloat("-NAN", -NAN);
  dumpFloat("+NAN", +NAN);
  dumpFloat("-HUGE_VALF", +HUGE_VALF);
  dumpFloat("+HUGE_VALF", -HUGE_VALF);
  dumpFloat("-FLT_MAX", -FLT_MAX);
  dumpFloat("+FLT_MAX", +FLT_MAX);
  dumpFloat("-FLT_MIN", -FLT_MIN);
  dumpFloat("+FLT_MIN", +FLT_MIN);
  return 0;
}
