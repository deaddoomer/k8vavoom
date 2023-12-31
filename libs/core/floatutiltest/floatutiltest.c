#define __STDC_WANT_IEC_60559_BFP_EXT__
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "../mathutil_float.h"
#include "../ryu.h"
#include "grisu2dbl.h"


static char *f2s_bufs[16] = {NULL};
static unsigned f2s_currbuf = 0;


static char *getnextbuf () {
  if (!f2s_bufs[f2s_currbuf]) f2s_bufs[f2s_currbuf] = malloc(1024);
  char *res = f2s_bufs[f2s_currbuf];
  f2s_currbuf = (f2s_currbuf+1)%16;
  return res;
}


static const char *f2s (const float f, const char *fmt) {
  char *res = getnextbuf();
  strfromf(res, 1020, fmt, f);
  return res;
}

static const char *d2sg2 (const float f) {
  char *res = getnextbuf();
  int len = grisu2_dtoa(f, res);
  res[len] = 0;
  return res;
}

static const char *f2sg2 (const float f) {
  char *res = getnextbuf();
  int len = grisu2_ftoa(f, res);
  res[len] = 0;
  return res;
}

static const char *f2sryu (const float f) {
  char *res = getnextbuf();
  f2s_buffered(f, res);
  return res;
}

static const char *d2sryu (const float f) {
  char *res = getnextbuf();
  d2s_buffered(f, res);
  return res;
}


static void dumpFloat (const char *msg, const float f) {
  const uint32_t t = *(const uint32_t *)(&f);
  const float f1 = zeroNanInfDenormalsF(f);
  const uint32_t t1 = *(const uint32_t *)(&f1);
  const float f2 = zeroDenormalsF(f);
  const uint32_t t2 = *(const uint32_t *)(&f2);
  printf("=== %s -- float: %s (%s) : 0x%08x (%s : %s) : nodenorm: 0x%08x %s (%s) : onlynodenorm: 0x%08x %s (%s) ===\n", msg, f2s(f, "%f"), f2s(f, "%g"), t, f2s(killInfNaNF(f), "%f"), f2s(killInfNaNF(f), "%g"), t1, f2s(f1, "%f"), f2s(f1, "%g"), t2, f2s(f2, "%f"), f2s(f2, "%g"));
  printf("  ryu      : %s\n", f2sryu(f));
  printf("  g2 (hack): %s\n", f2sg2(f));
  printf("  g2 (dbl) : %s\n", d2sg2(f));
  printf("  ryu (dbl): %s\n", d2sryu(f));
  printf("  floatsign: %s : %s : 0x%08x\n", f2s(floatSign(f), "%f"), f2s(floatSign(f), "%g"), fltconv_floatasuint(floatSign(f)));
  printf("  sign: %d\n", fltconv_getsign(f));
  printf("  exponent: %u : %d\n", fltconv_getexponent(f), fltconv_getsignedexponent(f));
  printf("  mantissa: 0x%08x (%u)\n", fltconv_getmantissa(f), fltconv_getmantissa(f));
  printf("  isFiniteF    : %d\n", isFiniteF(f));
  printf("  isNaNF       : %d\n", isNaNF(f));
  printf("  isInfF       : %d\n", isInfF(f));
  printf("  isDenormalF  : %d\n", isDenormalF(f));
  printf("  isZeroInfNaNF: %d\n", isZeroInfNaNF(f));
  printf("  isZeroF      : %d : %d\n", isZeroF(f), (int)(f == 0.0f));
  printf("  isNegativeF  : %d : %d\n", isNegativeF(f), (int)(f < 0.0f));
  printf("  isPositiveF  : %d : %d\n", isPositiveF(f), (int)(f > 0.0f));
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
  dumpFloat("0.1f", 0.1f);
  dumpFloat("0.2f", 0.2f);
  dumpFloat("0.3f", 0.3f);
  dumpFloat("0.4f", 0.4f);
  float denorm = fltconv_constructfloat(1, 0, 1); // very small positive denormal
  dumpFloat("minimum positive denormal", denorm);
  denorm = fltconv_constructfloat(-1, 0, 1); // very small negative denormal
  dumpFloat("minimum negative denormal", denorm);
  denorm = fltconv_constructfloat(-1, 0, FLTCONV_NAN_MAX_MANTISSA); // maximum positive denornal
  dumpFloat("maximum negative denormal", denorm);
  denorm = fltconv_constructfloat(1, 0, FLTCONV_NAN_MAX_MANTISSA); // maximum positive denornal
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
  dumpFloat("-HUGE_VALF", -HUGE_VALF);
  dumpFloat("+HUGE_VALF", +HUGE_VALF);
  dumpFloat("-FLT_MAX", -FLT_MAX);
  dumpFloat("+FLT_MAX", +FLT_MAX);
  dumpFloat("-FLT_MIN", -FLT_MIN);
  dumpFloat("+FLT_MIN", +FLT_MIN);
  float tt = +FLT_MAX;
  tt *= 1000.0f;
  dumpFloat("+FLT_MAX*1000.0f", tt);
  return 0;
}
