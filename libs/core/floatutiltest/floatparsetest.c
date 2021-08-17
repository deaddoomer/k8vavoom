#define __STDC_WANT_IEC_60559_BFP_EXT__
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "../mathutil_float.h"
#include "../ryu.h"
#include "../strtod_plan9.h"
#include "grisu2dbl.h"


static int systimeInited = 0;
#ifdef __linux__
static time_t systimeSecBase = 0;

#define SYSTIME_CHECK_INIT()  do { \
  if (!systimeInited) { systimeInited = 1; systimeSecBase = ts.tv_sec; } \
} while (0)

#else
static int systimeSecBase = 0;

#define SYSTIME_CHECK_INIT()  do { \
  if (!systimeInited) { systimeInited = 1; systimeSecBase = tp.tv_sec; } \
} while (0)

#endif


//==========================================================================
//
//  Sys_Time_64
//
//  return valud should not be zero
//
//==========================================================================
static uint64_t Sys_Time_Ex () {
#ifdef __linux__
  struct timespec ts;
  #ifdef ANDROID // CrystaX 10.3.2 supports only this
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) __builtin_trap();
  #else
    if (clock_gettime(/*CLOCK_MONOTONIC*/CLOCK_MONOTONIC_RAW, &ts) != 0) __builtin_trap();
  #endif
  SYSTIME_CHECK_INIT();
  // we don't actually need nanosecond precision here
  // one second is 1,000,000,000 nanoseconds
  // therefore, one millisecond is 1,000,000 nanoseconds
  const uint64_t msc =
    ((uint64_t)(ts.tv_sec-systimeSecBase+1)*(uint64_t)1000)+ // seconds->milliseconds
    ((uint64_t)ts.tv_nsec/(uint64_t)1000000); // nanoseconds->milliseconds
#else
  struct timeval tp;
  struct timezone tzp;
  gettimeofday(&tp, &tzp);
  SYSTIME_CHECK_INIT();
  // one second is 1,000,000 microseconds
  // therefore, one millisecond is 1,000 microseconds
  const uint64_t msc =
    ((uint64_t)(tp.tv_sec-systimeSecBase+1)*(uint64_t)1000)+ // seconds->milliseconds
    ((uint64_t)tp.tv_usec/(uint64_t)1000); // microseconds->milliseconds
#endif
  return msc;
}


int main () {
  // test all floats except nans and infinities
  FILE *fo = fopen("zlog.txt", "w");

  for (int phase = 0; phase < 2; ++phase) {
    printf("testing %s...\n", (phase == 0 ? "Ryu+Ryu parser" : phase == 1 ? "Grisu2+Ryu parser" : phase == 2 ? "Ryu+Plan9 parser" : "Grisu2+Plan9 parser"));
    fprintf(fo, "testing %s...\n", (phase == 0 ? "Ryu+Ryu parser" : phase == 1 ? "Grisu2+Ryu parser" : phase == 2 ? "Ryu+Plan9 parser" : "Grisu2+Plan9 parser"));
    fflush(fo);
    uint64_t stt = Sys_Time_Ex();
    #pragma omp parallel for shared(fo)
    for (unsigned n = 1/*0x15ae43fd*//*0x4c000000*/; n < 0x7f800000; ++n) {
      char str[64];
      const float f = *(const __attribute__((__may_alias__)) float *)&n;
      // to string
      str[0] = 0;
      if ((phase&1) == 0) {
        // ryu
        f2s_buffered(f, str);
      } else {
        // grisu2
        grisu2_ftoa(f, str);
      }
      // and back
      float f1 = 0.0f;
      if ((phase&2) != 0) {
        // plan9 parser
        char *end = str;
        int rangeErr = 0;
        f1 = fmtstrtof(str, &end, &rangeErr);
        if (rangeErr || end[0]) {
          fprintf(stderr, "FP PARSE ERROR! n:0x%08x;  f:%f (%g)\n", n, f, f);
          fprintf(fo, "FP PARSE ERROR! n:0x%08x;  f:%f (%g)\n", n, f, f);
          fflush(fo);
          //abort();
          continue;
        }
      } else {
        // ryu parser
        int res = ryu_s2f(str, (int)strlen(str), &f1, RYU_FLAG_DEFAULT);
        if (res != RYU_SUCCESS) {
          fprintf(stderr, "FP PARSE ERROR! n:0x%08x;  f:%f (%g) [%s]\n", n, f, f, str);
          fprintf(fo, "FP PARSE ERROR! n:0x%08x;  f:%f (%g) [%s]\n", n, f, f, str);
          fflush(fo);
          //abort();
          continue;
        }
      }
      if (memcmp((void *)&f, (void *)&f1, 4) != 0) {
        const unsigned n1 = *(const __attribute__((__may_alias__)) uint32_t *)&f1;
        /*
        char t2[64];
        grisu2_ftoa(f, t2);
        float f2 = 0.0f;
        int res = ryu_s2f(t2, (int)strlen(t2), &f2, RYU_FLAG_DEFAULT);
        const unsigned n2 = *(const __attribute__((__may_alias__)) uint32_t *)&f2;
        */
        fprintf(stderr, "FP INVALID ROUNDTRIP: n:0x%08x;  n1:0x%08x  f:%f (%g)  f1:%f (%g) [%s]\n", n, n1, f, f, f1, f1, str);
        fprintf(fo, "FP INVALID ROUNDTRIP: n:0x%08x;  n1:0x%08x  f:%f (%g)  f1:%f (%g) [%s]\n", n, n1, f, f, f1, f1, str);
        fflush(fo);
        //abort();
        continue;
      }
    }
    uint64_t ett = Sys_Time_Ex();
    uint32_t ctime = (uint32_t)(ett-stt);
    printf("test time: %u.%u\n", ctime/1000u, ctime%1000u);
  }

  fclose(fo);

  return 0;
}
