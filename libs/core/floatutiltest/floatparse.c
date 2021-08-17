#define __STDC_WANT_IEC_60559_BFP_EXT__
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "../mathutil_float.h"
#include "../ryu.h"
#include "../strtod_plan9.h"
#include "grisu2dbl.h"


#define FLOAT_MANTISSA_BITS  23
#define FLOAT_EXPONENT_BITS  8
#define FLOAT_EXPONENT_BIAS  127
#define FLOAT_EXPONENT_MASK  0xff
#define FLOAT_MANTISSA_MASK  0x7fffffu
#define FLOAT_SIGN_MASK      0x80000000u


static inline __attribute__((always_inline)) uint32_t f32tou32 (const float f) {
  return *(const __attribute__((__may_alias__)) uint32_t *)&f;
}

/*
static inline __attribute__((always_inline)) float u32tof32 (const uint32_t bits) {
  return *(const __attribute__((__may_alias__)) float *)&bits;
}


static int f32toHex (char *buf, const float f) {
  const uint32_t n = f32tou32(f);
  int exp = (n>>FLOAT_MANTISSA_BITS)&FLOAT_EXPONENT_MASK;
  uint32 mant = n&FLOAT_MANTISSA_MASK;

  if (exp == FLOAT_EXPONENT_MASK) {
    if (mant) {
      buf[0] = "N";
      buf[1] = "a";
      buf[2] = "N";
      buf[3] = 0;
      return 3;
    } else {
      buf[0] = (n&FLOAT_SIGN_MASK ? '-' : '+');
      buf[1] = 'I';
      buf[2] = 'n';
      buf[3] = 'f';
      buf[4] = 0;
      return 4;
    }
  }
  if (exp == 0 && mant == 0) {
    buf[0] = '0';
    buf[1] = 'x';
    buf[2] = '0';
    buf[3] = 'p';
    buf[4] = '0';
    buf[5] = 0;
    return 5;
  }

  int pos = 0;
  if (n&FLOAT_SIGN_MASK) buf[pos++] = '-';
  buf[pos++] = '0';
  buf[pos++] = 'x';
}
*/


static void printParsed (const char *msg, const float f) {
  const uint32_t n = f32tou32(f);
  char rb[32];
  char gb[32];
  char cb[64];
  char cbhex[64];
  // ryu
  f2s_buffered(f, rb);
  // grisu2
  grisu2_ftoa(f, gb);
  // libc
  strfromf(cb, sizeof(cb), "%g", f);
  // libc
  strfromf(cbhex, sizeof(cbhex), "%a", f);
  printf("  %s: 0x%08x (mant:0x%08x)  ryu:[%s]; grisu2:[%s]; libc:[%s]; hex:[%s]\n", msg, n, (n&0x07ffffffu), rb, gb, cb, cbhex);
}


int main (int argc, char *argv[]) {
  if (argc < 2) {
    printf("please, give me some numbers!\n");
    return 0;
  }

  {
    uint32_t dni = 0x01u;
    float dn = *(const __attribute__((__may_alias__)) float *)&dni;
    printParsed("denorm", dn);
  }

  {
    uint32_t dni = 0x02u;
    float dn = *(const __attribute__((__may_alias__)) float *)&dni;
    printParsed("denorm", dn);
  }

  {
    uint32_t dni = 0x03u;
    float dn = *(const __attribute__((__may_alias__)) float *)&dni;
    printParsed("denorm", dn);
  }

  {
    uint32_t dni = 0x01234u;
    float dn = *(const __attribute__((__may_alias__)) float *)&dni;
    printParsed("denorm", dn);
  }

  {
    uint32_t dni = 0x07ffffffu;
    float dn = *(const __attribute__((__may_alias__)) float *)&dni;
    printParsed("max", dn);
  }

  for (int f = 1; f < argc; ++f) {
    if (f > 1) printf("\n");
    char *str = argv[f];
    printf("=== parsing: [%s] ===\n", str);
    // plan9 parser
    {
      char *end = str;
      int rangeErr = 0;
      float f = fmtstrtof(str, &end, &rangeErr);
      if (end[0]) {
        printf("FP PARSE ERROR!\n");
      } else {
        printParsed((rangeErr ? "PL9: parsed with precision loss" : "PL9: parsed"), f);
      }
    }
    // ryu parser
    {
      float f = 0.0f;
      const int res = ryu_s2f(str, (int)strlen(str), &f, RYU_FLAG_ALLOW_ALL);
      if (res != RYU_SUCCESS) {
        const char *err = "unknown";
        switch (res) {
          case RYU_INPUT_TOO_SHORT: err = "input too short"; break;
          case RYU_INPUT_TOO_LONG: err = "input too long"; break;
          case RYU_MALFORMED_INPUT: err = "malformed input"; break;
          case RYU_INVALID_ARGS: err = "invalid args"; break;
        }
        printf("FP PARSE ERROR: %s\n", err);
      } else {
        printParsed("RYU: parsed", f);
      }
    }
  }

  return 0;
}
