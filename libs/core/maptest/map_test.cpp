//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
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
#define EXCESSIVE_CHECKS
#define EXCESSIVE_CHECKS_ITERATOR
#define EXCESSIVE_COMPACT
#define EXCESSIVE_REHASH

#define CORE_MAP_TEST


typedef unsigned int vuint32;


#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../common.h"
#include "../hash/hashfunc.h"

#define vassert(cond_)  do { \
  if (!(cond_)) { \
    fprintf(stderr, "%s:%d: Assertion failed: %s\n", __FILE__, __LINE__, #cond_); \
    __builtin_trap(); \
  } \
} while (0)



static void fatal (const char *msg) {
  fprintf(stderr, "FATAL: %s\n", msg);
  __builtin_trap();
}


void Sys_Error (const char *error, ...) {
  va_list argptr;
  static char buf[16384]; //WARNING! not thread-safe!

  va_start(argptr,error);
  vsnprintf(buf, sizeof(buf), error, argptr);
  va_end(argptr);

  fatal(buf);
}


#include "../zone.h"
#include "../map.h"


// ////////////////////////////////////////////////////////////////////////// //
// tests for hash table
enum { MaxItems = 16384 };


int its[MaxItems];
TMap<int, int> hash;


void checkHash (bool dump=false) {
  int count = 0;
  if (dump) printf("====== CHECK ======\n");
  for (int i = 0; i < MaxItems; ++i) {
    if (dump) {
      auto flag = hash.get(i);
      printf(" check #%d; v=%d; flag=%d\n", i, (flag ? *flag : 0), (flag ? 1 : 0));
    }
    if (its[i] >= 0) {
      ++count;
      if (!hash.has(i)) fatal("(0.0) fuuuuuuuuuuuu");
      auto vp = hash.get(i);
      if (!vp) fatal("(0.1) fuuuuuuuuuuuu");
      if (*vp != its[i]) fatal("(0.2) fuuuuuuuuuuuu");
    } else {
      if (hash.has(i)) fatal("(0.3) fuuuuuuuuuuuu");
    }
  }
  if (count != hash.count()) fatal("(0.4) fuuuuuuuuuuuu");
  if (dump) printf("------\n");
}


void testIterator (bool verbose) {
  if (verbose) printf("  (iteration, normal)\n");
  static int marks[MaxItems];
  int count = 0;
  for (int i = 0; i < MaxItems; ++i) marks[i] = 0;
  for (auto it = hash.first(); it; ++it) {
    //writeln('key=', k, '; value=', v);
    auto k = it.getKey();
    auto v = it.getValue();
    if (marks[k]) fatal("duplicate entry in iterator");
    if (its[k] != v) fatal("invalid entry in iterator");
    marks[k] = 1;
    ++count;
  }
  if (count != hash.length()) {
    printf("0: count=%d; hash.count=%d\n", count, hash.count());
    //raise Exception.Create('lost entries in iterator');
  }
  if (verbose) printf("  (iteration, foreach)\n");
  // foreach iterator
  for (auto &&it : hash.first()) {
    auto k = it.getKey();
    auto v = it.getValue();
    if (marks[k] != 1) fatal("foreach iterator fuckup");
    if (its[k] != v) fatal("invalid entry in foreach iterator");
    ++marks[k];
    --count;
  }
  if (count != 0) {
    printf("0: count=%d; hash.count=%d\n", count, hash.count());
    //raise Exception.Create('lost entries in iterator');
  }
  if (verbose) printf("  (iteration, counters)\n");
  count = 0;
  for (int i = 0; i < MaxItems; ++i) if (marks[i]) ++count;
  if (count != hash.count()) {
    printf("1: count=%d; hash.count=%d\n", count, hash.count());
    fatal("lost entries in iterator");
  }
  if (hash.count() != hash.countItems()) {
    printf("OOPS: count=%d; countItems=%d\n", hash.count(), hash.countItems());
    fatal("fuck");
  }
}


int main (int argc, char *argv[]) {
#if 0
  const char *ss = "abc";
  const uint32_t hall = bjHashBuf(ss, strlen(ss));
  uint32_t h0 = bjHashBuf(ss+0, 1);
  uint32_t h1 = bjHashBuf(ss+1, 1, h0);
  uint32_t h2 = bjHashBuf(ss+2, 1, h1);
  vassert(hall == h2);
#endif

  int xcount;

  for (int i = 0; i < MaxItems; ++i) its[i] = -1;

  //Randomize();
  unsigned seed = (unsigned)time(nullptr);

  if (argc > 1) {
    char *str = argv[1];
    char *end = str;
    unsigned long int ns = strtoul(str, &end, 0);
    if (*end) { fprintf(stderr, "invalid seed: '%s'\n", str); __builtin_trap(); }
    seed = (unsigned)ns;
  }

  printf("=== SEED: 0x%08x ===\n", seed);
  srand(seed);

  printf("testing: insertion\n");
  xcount = 0;
  for (int i = 0; i < MaxItems; ++i) {
    int v = rand()%MaxItems;
    //writeln('i=', i, '; v=', v, '; its[v]=', its[v]);
    if (its[v] >= 0) {
      if (!hash.has(v)) fatal("(1.0) fuuuuuuuuuuuu");
      auto vp = hash.get(v);
      if (!vp) fatal("(1.1) fuuuuuuuuuuuu");
      if (*vp != its[v]) fatal("(1.2) fuuuuuuuuuuuu");
    } else {
      its[v] = i;
      if (hash.put(v, i)) fatal("(1.3) fuuuuuuuuuuuu");
      ++xcount;
      if (xcount != hash.count()) fatal("(1.4) fuuuuuuuuuuuu");
    }
    #ifdef EXCESSIVE_CHECKS
    checkHash();
    #endif
    #ifdef EXCESSIVE_CHECKS_ITERATOR
    testIterator(false);
    #endif
  }
  if (xcount != hash.count()) fatal("(1.5) fuuuuuuuuuuuu");
  printf("capacity=%d; length=%d; maxprobecount=%u; maxcomparecount=%u\n", hash.capacity(), hash.length(), hash.getMaxProbeCount(), hash.getMaxCompareCount());
  checkHash();
  testIterator(true);

  printf("testing: deletion\n");
  #ifdef EXCESSIVE_REHASH
  hash.rehash();
  #endif
  for (int i = 0; i < MaxItems*8; ++i) {
    int v = rand()%MaxItems;
    //writeln('trying to delete ', v, '; its[v]=', its[v]);
    bool del = hash.del(v);
    //writeln('  del=', del);
    if (del) {
      if (its[v] < 0) fatal("(2.0) fuuuuuuuuuuuu");
      --xcount;
    } else {
      if (its[v] >= 0) fatal("(2.1) fuuuuuuuuuuuu");
    }
    its[v] = -1;
    if (xcount != hash.count()) fatal("(2.2) fuuuuuuuuuuuu");
    #ifdef EXCESSIVE_COMPACT
    hash.compact();
    if (xcount != hash.count()) fatal("(2.3) fuuuuuuuuuuuu");
    #endif
    #ifdef EXCESSIVE_CHECKS
    checkHash();
    #endif
    #ifdef EXCESSIVE_CHECKS_ITERATOR
    testIterator(false);
    #endif
    if (hash.count() == 0) break;
  }
  printf("%d items left in hash\n", hash.count());
  checkHash();
  testIterator(true);

  if (hash.count()) {
    printf("deleting by iteration...\n");
    for (auto &&it : hash.first()) {
      vassert(it.isValid());
      vassert(it.isValidEntry());
      it.removeCurrentNoAdvance();
      // `isValid()` may return `false` here
      vassert(!it.isValidEntry());
    }
    vassert(hash.count() == 0);
    #ifdef EXCESSIVE_COMPACT
    hash.compact();
    #endif
    int cnt = 0;
    for (auto &&it : hash.first()) {
      vassert(it.isValid());
      vassert(it.isValidEntry());
      ++cnt;
    }
    vassert(cnt == hash.count());
    vassert(!hash.first().isValid());
    vassert(!hash.first().isValidEntry());
  }

  return 0;
}
