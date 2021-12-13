/* Copyright (C) 2011 by Valentin Ochs
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/* Minor changes by Rich Felker for integration in musl, 2011-04-27. */

/* Smoothsort, an adaptive variant of Heapsort.  Memory usage: O(1).
   Run time: Worst case O(n log n), close to O(n) in the mostly-sorted case. */
#include "smsort.h"

#include <string.h>

//#define SMSORT_DEBUG_000

#if 0
// non-asm implementation
static inline int a_ctz_l (uint32_t x) {
  static const char debruijn32[32] = {
    0, 1, 23, 2, 29, 24, 19, 3, 30, 27, 25, 11, 20, 8, 4, 13,
    31, 22, 28, 18, 26, 10, 7, 12, 21, 17, 9, 6, 16, 5, 15, 14
  };
  return debruijn32[(x&-x)*0x076be629>>27];
}


static inline int a_ctz_64 (uint64_t x) {
  uint32_t y = (uint32_t)x;
  if (!y) {
    y = (uint32_t)(x>>32);
    return 32+a_ctz_l(y);
  }
  return a_ctz_l(y);
}
#endif

#ifdef SMSORT_DEBUG_000
# include <stdio.h>
#endif


//#include "atomic.h"
//#define ntz(x) a_ctz_l((x))
#if INTPTR_MAX == INT32_MAX
# define ntz(x)  __builtin_ctz(x)
#elif INTPTR_MAX == INT64_MAX
# define ntz(x)  __builtin_ctzl(x)
#else
# error "Environment not 32 or 64-bit."
#endif


static inline int pntz (size_t p[2]) {
  int r = (p[0]-1 ? ntz(p[0]-1) : 0); // alas, bultin fails on zero
  if (r != 0 || (r = 8*(int)sizeof(size_t)+(p[1] ? ntz(p[1]) : 0)) != 8*(int)sizeof(size_t)) {
    return r;
  }
  return 0;
}


#define CYCLEI(tp_)  do { \
  --n; /* to avoid doing `-1` in the loop, and in the last line */ \
  const tp_ v0 = *(const tp_ *)ar[0]; \
  for (i = 0; i < n; ++i) *(tp_ *)ar[i] = *(const tp_ *)ar[i+1]; \
  *(tp_ *)ar[n] = v0; \
} while (0)


static void cycle (size_t width, unsigned char* ar[], int n) {
  if (n < 2) return;
  int i;

  // common type sizes
  if (width <= 8) {
    switch (width) {
      case 1: CYCLEI(uint8_t); return;
      case 2: CYCLEI(uint16_t); return;
      case 4: CYCLEI(uint32_t); return;
      case 8: CYCLEI(uint64_t); return;
    }
  }

  unsigned char tmp[256];
  size_t l;
  ar[n] = tmp;
  while (width) {
    l = (sizeof(tmp) < width ? sizeof(tmp) : width);
    memcpy(ar[n], ar[0], l);
    for (i = 0; i < n; ++i) {
      memcpy(ar[i], ar[i+1], l);
      ar[i] += l;
    }
    width -= l;
  }
}


/* shl() and shr() need n > 0 */
static inline void shl (size_t p[2], int n) {
  if (n >= 8*sizeof(size_t)) {
    n -= 8*sizeof(size_t);
    p[1] = p[0];
    p[0] = 0;
  }
  p[1] <<= n;
  p[1] |= p[0]>>(sizeof(size_t)*8-n);
  p[0] <<= n;
}


static inline void shr (size_t p[2], int n) {
  if (n >= 8*sizeof(size_t)) {
    n -= 8*sizeof(size_t);
    p[0] = p[1];
    p[1] = 0;
  }
  p[0] >>= n;
  p[0] |= p[1]<<(sizeof(size_t)*8-n);
  p[1] >>= n;
}


static void sift (unsigned char *head, size_t width, smsort_cmpfun cmp, void *arg, int pshift, size_t lp[]) {
  unsigned char *rt, *lf;
  unsigned char *ar[14*sizeof(size_t)+1];
  int i = 1;

  ar[0] = head;
  while (pshift > 1) {
    rt = head-width;
    lf = head-width-lp[pshift-2];

    if (cmp(ar[0], lf, arg) >= 0 && cmp(ar[0], rt, arg) >= 0) {
      break;
    }
    if (cmp(lf, rt, arg) >= 0) {
      ar[i++] = lf;
      head = lf;
      pshift -= 1;
    } else {
      ar[i++] = rt;
      head = rt;
      pshift -= 2;
    }
  }
  cycle(width, ar, i);
}


static void trinkle (unsigned char *head, size_t width, smsort_cmpfun cmp, void *arg, size_t pp[2], int pshift, int trusty, size_t lp[]) {
  unsigned char *stepson,
                *rt, *lf;
  size_t p[2];
  unsigned char *ar[14*sizeof(size_t)+1];
  int i = 1;
  int trail;

  p[0] = pp[0];
  p[1] = pp[1];

  ar[0] = head;
  while (p[0] != 1 || p[1] != 0) {
    stepson = head-lp[pshift];
    if (cmp(stepson, ar[0], arg) <= 0) {
      break;
    }
    if (!trusty && pshift > 1) {
      rt = head-width;
      lf = head-width-lp[pshift-2];
      if (cmp(rt, stepson, arg) >= 0 || cmp(lf, stepson, arg) >= 0) {
        break;
      }
    }

    ar[i++] = stepson;
    head = stepson;
    trail = pntz(p);
    shr(p, trail);
    pshift += trail;
    trusty = 0;
  }
  if (!trusty) {
    cycle(width, ar, i);
    sift(head, width, cmp, arg, pshift, lp);
  }
}


#ifdef SMSORT_DEBUG_000
# define DBGINS  fprintf(stderr, "INS! (%u:%u)\n", width, nel);
#else
# define DBGINS
#endif


// insertion sort using integer types
#define INSSORT(tp_)  do { \
  tp_ *arr = (tp_ *)base; \
  unsigned i = 1; \
  while (i < (unsigned)nel) { \
    unsigned j = i; \
    while (j && cmp(arr+j-1U, arr+j, arg) > 0) { \
      const tp_ tmpv = arr[j-1U]; \
      arr[j-1U] = arr[j]; \
      arr[j] = tmpv; \
      --j; \
    } \
    ++i; \
  } \
} while (0)


void smsort_r (void *base, size_t nel, size_t width, smsort_cmpfun cmp, void *arg) {
  size_t i, size = width*nel;

  if (!size || nel < 2) return;

  // use insertion sort for small arrays
  if (nel <= 16 && width <= 8) {
    switch (width) {
      case 1: DBGINS INSSORT(uint8_t); return;
      case 2: DBGINS INSSORT(uint16_t); return;
      case 4: DBGINS INSSORT(uint32_t); return;
      case 8: DBGINS INSSORT(uint64_t); return;
    }
  }
  #ifdef SMSORT_DEBUG_000
  fprintf(stderr, "SMOOTH! (%u:%u)\n", width, nel);
  #endif

  size_t lp[12*sizeof(size_t)];
  unsigned char *head, *high;
  size_t p[2] = {1, 0};
  int pshift = 1;
  int trail;

  head = base;
  high = head+size-width;

  /* Precompute Leonardo numbers, scaled by element width */
  for (lp[0]=lp[1]=width, i=2; (lp[i]=lp[i-2]+lp[i-1]+width) < size; i++) {}

  while (head < high) {
    if ((p[0]&3) == 3) {
      sift(head, width, cmp, arg, pshift, lp);
      shr(p, 2);
      pshift += 2;
    } else {
      if (lp[pshift-1] >= high-head) {
        trinkle(head, width, cmp, arg, p, pshift, 0, lp);
      } else {
        sift(head, width, cmp, arg, pshift, lp);
      }

      if (pshift == 1) {
        shl(p, 1);
        pshift = 0;
      } else {
        shl(p, pshift-1);
        pshift = 1;
      }
    }

    p[0] |= 1;
    head += width;
  }

  trinkle(head, width, cmp, arg, p, pshift, 0, lp);

  while (pshift != 1 || p[0] != 1 || p[1] != 0) {
    if (pshift <= 1) {
      trail = pntz(p);
      shr(p, trail);
      pshift += trail;
    } else {
      shl(p, 2);
      pshift -= 2;
      p[0] ^= 7;
      shr(p, 1);
      trinkle(head-lp[pshift]-width, width, cmp, arg, p, pshift+1, 1, lp);
      shl(p, 1);
      p[0] |= 1;
      trinkle(head-width, width, cmp, arg, p, pshift, 1, lp);
    }
    head -= width;
  }
}
