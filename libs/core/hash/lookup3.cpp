/*
-------------------------------------------------------------------------------
lookup3.c, by Bob Jenkins, May 2006, Public Domain.

These are functions for producing 32-bit hashes for hash table lookup.
hashword(), hashlittle(), hashlittle2(), hashbig(), mix(), and final()
are externally useful functions.  Routines to test the hash are included
if SELF_TEST is defined.  You can use this free for any purpose.  It's in
the public domain.  It has no warranty.

You probably want to use hashlittle().  hashlittle() and hashbig()
hash byte arrays.  hashlittle() is is faster than hashbig() on
little-endian machines.  Intel and AMD are little-endian machines.
On second thought, you probably want hashlittle2(), which is identical to
hashlittle() except it returns two 32-bit hashes for the price of one.
You could implement hashbig2() if you wanted but I haven't bothered here.

If you want to find a hash of, say, exactly 7 integers, do
  a = i1;  b = i2;  c = i3;
  mix(a,b,c);
  a += i4; b += i5; c += i6;
  mix(a,b,c);
  a += i7;
  final(a,b,c);
then use c as the hash value.  If you have a variable length array of
4-byte integers to hash, use hashword().  If you have a byte array (like
a character string), use hashlittle().  If you have several byte arrays, or
a mix of things, see the comments above hashlittle().

Why is this so big?  I read 12 bytes at a time into 3 4-byte integers,
then mix those integers.  This is fast (you can do a lot more thorough
mixing with 12*3 instructions on 3 integers than you can with 3 instructions
on 1 byte), but shoehorning those bytes into integers efficiently is messy.
-------------------------------------------------------------------------------
*/
#include <stddef.h>
#include <stdint.h>


#define hashsize(n) ((uint32_t)1<<(n))
#define hashmask(n) (hashsize(n)-1)
#define rot(x,k) (((x)<<(k))|((x)>>(32-(k))))

/*
-------------------------------------------------------------------------------
mix -- mix 3 32-bit values reversibly.

This is reversible, so any information in (a,b,c) before mix() is
still in (a,b,c) after mix().

If four pairs of (a,b,c) inputs are run through mix(), or through
mix() in reverse, there are at least 32 bits of the output that
are sometimes the same for one pair and different for another pair.
This was tested for:
* pairs that differed by one bit, by two bits, in any combination
  of top bits of (a,b,c), or in any combination of bottom bits of
  (a,b,c).
* "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
  the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
  is commonly produced by subtraction) look like a single 1-bit
  difference.
* the base values were pseudorandom, all zero but one bit set, or
  all zero plus a counter that starts at zero.

Some k values for my "a-=c; a^=rot(c,k); c+=b;" arrangement that
satisfy this are
    4  6  8 16 19  4
    9 15  3 18 27 15
   14  9  3  7 17  3
Well, "9 15 3 18 27 15" didn't quite get 32 bits diffing
for "differ" defined as + with a one-bit base and a two-bit delta.  I
used http://burtleburtle.net/bob/hash/avalanche.html to choose
the operations, constants, and arrangements of the variables.

This does not achieve avalanche.  There are input bits of (a,b,c)
that fail to affect some output bits of (a,b,c), especially of a.  The
most thoroughly mixed value is c, but it doesn't really even achieve
avalanche in c.

This allows some parallelism.  Read-after-writes are good at doubling
the number of bits affected, so the goal of mixing pulls in the opposite
direction as the goal of parallelism.  I did what I could.  Rotates
seem to cost as much as shifts on every machine I could lay my hands
on, and rotates are much kinder to the top and bottom bits, so I used
rotates.
-------------------------------------------------------------------------------
*/
#define mix(a,b,c) \
{ \
  a -= c;  a ^= rot(c, 4);  c += b; \
  b -= a;  b ^= rot(a, 6);  a += c; \
  c -= b;  c ^= rot(b, 8);  b += a; \
  a -= c;  a ^= rot(c,16);  c += b; \
  b -= a;  b ^= rot(a,19);  a += c; \
  c -= b;  c ^= rot(b, 4);  b += a; \
}

/*
-------------------------------------------------------------------------------
final -- final mixing of 3 32-bit values (a,b,c) into c

Pairs of (a,b,c) values differing in only a few bits will usually
produce values of c that look totally different.  This was tested for
* pairs that differed by one bit, by two bits, in any combination
  of top bits of (a,b,c), or in any combination of bottom bits of
  (a,b,c).
* "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
  the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
  is commonly produced by subtraction) look like a single 1-bit
  difference.
* the base values were pseudorandom, all zero but one bit set, or
  all zero plus a counter that starts at zero.

These constants passed:
 14 11 25 16 4 14 24
 12 14 25 16 4 14 24
and these came close:
  4  8 15 26 3 22 24
 10  8 15 26 3 22 24
 11  8 15 26 3 22 24
-------------------------------------------------------------------------------
*/
#define final(a,b,c) \
{ \
  c ^= b; c -= rot(b,14); \
  a ^= c; a -= rot(c,11); \
  b ^= a; b -= rot(a,25); \
  c ^= b; c -= rot(b,16); \
  a ^= c; a -= rot(c, 4); \
  b ^= a; b -= rot(a,14); \
  c ^= b; c -= rot(b,24); \
}


/*
--------------------------------------------------------------------
 This works on all machines.  To be useful, it requires
 -- that the key be an array of uint32_t's, and
 -- that the length be the number of uint32_t's in the key

 The function hashword() is identical to hashlittle() on little-endian
 machines, and identical to hashbig() on big-endian machines,
 except that the length has to be measured in uint32_ts rather than in
 bytes.  hashlittle() is more complicated than hashword() only because
 hashlittle() has to dance around fitting the key bytes into registers.
--------------------------------------------------------------------
*/
uint32_t bjHashU32v (
  const __attribute__((__may_alias__)) uint32_t *k, /* the key, an array of uint32_t values */
  size_t count, /* the length of the key, in uint32_ts */
  uint32_t initval) noexcept /* the previous hash, or an arbitrary value */
{
  uint32_t a, b, c;

  /* Set up the internal state */
  a = b = c = 0xdeadbeefU+(((uint32_t)count)<<2)+initval;

  /*------------------------------------------------- handle most of the key */
  while (count > 3) {
    a += k[0];
    b += k[1];
    c += k[2];
    mix(a, b, c);
    count -= 3;
    k += 3;
  }

  /*------------------------------------------- handle the last 3 uint32_t's */
  /* all the case statements fall through */
  switch (count) {
    case 3: c += k[2]; /* fallthrough */
    case 2: b += k[1]; /* fallthrough */
    case 1: a += k[0];
      final(a, b, c); /* fallthrough */
    case 0: /* case 0: nothing left to add */
      break;
  }
  /*------------------------------------------------------ report the result */
  return c;
}


/*
--------------------------------------------------------------------
hashword2() -- same as hashword(), but take two seeds and return two
32-bit values.  pc and pb must both be nonnull, and *pc and *pb must
both be initialized with seeds.  If you pass in (*pb)==0, the output
(*pc) will be the same as the return value from hashword().
--------------------------------------------------------------------
*/
void bjHashU32v64P (
  const __attribute__((__may_alias__)) uint32_t *k,                   /* the key, an array of uint32_t values */
  size_t count, /* the length of the key, in uint32_ts */
  uint32_t *pc, /* IN: seed OUT: primary hash value */
  uint32_t *pb) noexcept /* IN: more seed OUT: secondary hash value */
{
  uint32_t a, b, c;

  /* Set up the internal state */
  a = b = c = 0xdeadbeefU+((uint32_t)(count<<2))+(*pc);
  c += *pb;

  /*------------------------------------------------- handle most of the key */
  while (count > 3) {
    a += k[0];
    b += k[1];
    c += k[2];
    mix(a, b, c);
    count -= 3;
    k += 3;
  }

  /*------------------------------------------- handle the last 3 uint32_t's */
  /* all the case statements fall through */
  switch (count) {
    case 3: c += k[2]; /* fallthrough */
    case 2: b += k[1]; /* fallthrough */
    case 1: a += k[0];
      final(a, b, c); /* fallthrough */
    case 0: /* case 0: nothing left to add */
      break;
  }
  /*------------------------------------------------------ report the result */
  *pc = c;
  *pb = b;
}


/*
-------------------------------------------------------------------------------
hashlittle() -- hash a variable-length key into a 32-bit value
  k       : the key (the unaligned variable-length array of bytes)
  length  : the length of the key, counting by bytes
  initval : can be any 4-byte value
Returns a 32-bit value.  Every bit of the key affects every bit of
the return value.  Two keys differing by one or two bits will have
totally different hash values.

The best hash table sizes are powers of 2.  There is no need to do
mod a prime (mod is sooo slow!).  If you need less than 32 bits,
use a bitmask.  For example, if you need only 10 bits, do
  h = (h & hashmask(10));
In which case, the hash table should have hashsize(10) elements.

If you are hashing n strings (uint8_t **)k, do it like this:
  for (i=0, h=0; i<n; ++i) h = hashlittle( k[i], len[i], h);

By Bob Jenkins, 2006.  bob_jenkins@burtleburtle.net.  You may use this
code any way you wish, private, educational, or commercial.  It's free.

Use for hash table lookup, or anything where one collision in 2^^32 is
acceptable.  Do NOT use for cryptographic purposes.
-------------------------------------------------------------------------------
*/
uint32_t bjHashBuf (const __attribute__((__may_alias__)) void *key, size_t length, uint32_t initval) noexcept {
  uint32_t a, b, c;                                          /* internal state */

  /* set up the internal state */
  a = b = c = 0xdeadbeefU+((uint32_t)length)+initval;

  if ((((uintptr_t)key)&0x03u) == 0) {
    // aligned by 4 bytes
    const __attribute__((__may_alias__)) uint32_t *k = (const __attribute__((__may_alias__)) uint32_t *)key; /* read 32-bit chunks */

    /*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */
    while (length > 12) {
      a += k[0];
      b += k[1];
      c += k[2];
      mix(a, b, c);
      length -= 12;
      k += 3;
    }

    /*----------------------------- handle the last (probably partial) block */
    /*
     * "k[2]&0xffffff" actually reads beyond the end of the string, but
     * then masks off the part it's not allowed to read.  Because the
     * string is aligned, the masked-off tail is in the same word as the
     * rest of the string.  Every machine with memory protection I've seen
     * does it on word boundaries, so is OK with this.  But VALGRIND will
     * still catch it and complain.  The masking trick does make the hash
     * noticably faster for short strings (like English words).
     */
#ifndef VALGRIND
    switch (length) {
      case 12: c += k[2]; b += k[1]; a += k[0]; break;
      case 11: c += k[2]&0xffffffU; b += k[1]; a += k[0]; break;
      case 10: c += k[2]&0xffffU; b += k[1]; a += k[0]; break;
      case 9: c += k[2]&0xffU; b += k[1]; a += k[0]; break;
      case 8: b += k[1]; a += k[0]; break;
      case 7: b += k[1]&0xffffffU; a += k[0]; break;
      case 6: b += k[1]&0xffffU; a += k[0]; break;
      case 5: b += k[1]&0xffU; a += k[0]; break;
      case 4: a += k[0]; break;
      case 3: a += k[0]&0xffffffU; break;
      case 2: a += k[0]&0xffffU; break;
      case 1: a += k[0]&0xffU; break;
      case 0: return c; /* zero length strings require no mixing */
    }
#else /* make valgrind happy */
    const __attribute__((__may_alias__)) uint8_t *k8 = (const __attribute__((__may_alias__)) uint8_t *)k;
    switch (length) {
      case 12: c += k[2]; b += k[1]; a += k[0]; break;
      case 11: c += ((uint32_t)k8[10])<<16; /* fall through */
      case 10: c += ((uint32_t)k8[9])<<8; /* fall through */
      case 9: c += k8[8]; /* fall through */
      case 8: b += k[1]; a += k[0]; break;
      case 7: b += ((uint32_t)k8[6])<<16; /* fall through */
      case 6: b += ((uint32_t)k8[5])<<8; /* fall through */
      case 5: b += k8[4]; /* fall through */
      case 4: a += k[0]; break;
      case 3: a += ((uint32_t)k8[2])<<16; /* fall through */
      case 2: a += ((uint32_t)k8[1])<<8; /* fall through */
      case 1: a += k8[0]; break;
      case 0: return c;
    }
#endif /* !valgrind */
  } else if ((((uintptr_t)key)&0x01u) == 0) {
    // aligned by 2 bytes
    const __attribute__((__may_alias__)) uint16_t *k = (const __attribute__((__may_alias__)) uint16_t *)key; /* read 16-bit chunks */

    /*--------------- all but last block: aligned reads and different mixing */
    while (length > 12) {
      a += k[0]+(((uint32_t)k[1])<<16);
      b += k[2]+(((uint32_t)k[3])<<16);
      c += k[4]+(((uint32_t)k[5])<<16);
      mix(a, b, c);
      length -= 12;
      k += 6;
    }

    /*----------------------------- handle the last (probably partial) block */
    const __attribute__((__may_alias__)) uint8_t  *k8 = (const __attribute__((__may_alias__)) uint8_t *)k;
    switch (length) {
      case 12:
        c += k[4]+(((uint32_t)k[5])<<16);
        b += k[2]+(((uint32_t)k[3])<<16);
        a += k[0]+(((uint32_t)k[1])<<16);
        break;
      case 11: c += ((uint32_t)k8[10])<<16; /* fall through */
      case 10:
        c += k[4];
        b += k[2]+(((uint32_t)k[3])<<16);
        a += k[0]+(((uint32_t)k[1])<<16);
        break;
      case 9: c += k8[8]; /* fall through */
      case 8:
        b += k[2]+(((uint32_t)k[3])<<16);
        a += k[0]+(((uint32_t)k[1])<<16);
        break;
      case 7: b += ((uint32_t)k8[6])<<16; /* fall through */
      case 6:
        b += k[2];
        a += k[0]+(((uint32_t)k[1])<<16);
        break;
      case 5: b += k8[4]; /* fall through */
      case 4:
        a += k[0]+(((uint32_t)k[1])<<16);
        break;
      case 3: a += ((uint32_t)k8[2])<<16; /* fall through */
      case 2: a += k[0]; break;
      case 1: a += k8[0]; break;
      case 0: return c; /* zero length requires no mixing */
    }
  } else {
    /* need to read the key one byte at a time */
    const __attribute__((__may_alias__)) uint8_t *k = (const __attribute__((__may_alias__)) uint8_t *)key;

    /*--------------- all but the last block: affect some 32 bits of (a,b,c) */
    while (length > 12) {
      a += k[0];
      a += ((uint32_t)k[1])<<8;
      a += ((uint32_t)k[2])<<16;
      a += ((uint32_t)k[3])<<24;
      b += k[4];
      b += ((uint32_t)k[5])<<8;
      b += ((uint32_t)k[6])<<16;
      b += ((uint32_t)k[7])<<24;
      c += k[8];
      c += ((uint32_t)k[9])<<8;
      c += ((uint32_t)k[10])<<16;
      c += ((uint32_t)k[11])<<24;
      mix(a, b, c);
      length -= 12;
      k += 12;
    }

    /*-------------------------------- last block: affect all 32 bits of (c) */
    /* all the case statements fall through */
    switch (length) {
      case 12: c += ((uint32_t)k[11])<<24; /* fall through */
      case 11: c += ((uint32_t)k[10])<<16; /* fall through */
      case 10: c += ((uint32_t)k[9])<<8; /* fall through */
      case 9: c += k[8]; /* fall through */
      case 8: b += ((uint32_t)k[7])<<24; /* fall through */
      case 7: b += ((uint32_t)k[6])<<16; /* fall through */
      case 6: b += ((uint32_t)k[5])<<8; /* fall through */
      case 5: b += k[4]; /* fall through */
      case 4: a += ((uint32_t)k[3])<<24; /* fall through */
      case 3: a += ((uint32_t)k[2])<<16; /* fall through */
      case 2: a += ((uint32_t)k[1])<<8; /* fall through */
      case 1: a += k[0]; break;
      case 0: return c;
    }
  }

  final(a, b, c);
  return c;
}


/* the same as bjHashBufCI, but can be used for case-insensitive search */
uint32_t bjHashBufCI (const __attribute__((__may_alias__)) void *key, size_t length, uint32_t initval) noexcept {
  #define KK(n_)  (k[n_]|0x20202020u)
  #define K8(n_)  (k8[n_]|0x20u)
  #define K6(n_)  (k6[n_]|0x2020u)

  uint32_t a, b, c; // internal state

  // set up the internal state
  a = b = c = 0xdeadbeefU+((uint32_t)length)+initval;

  if ((((uintptr_t)key)&0x03u) == 0) {
    // aligned by 4 bytes; process by 32-bit chunks
    const __attribute__((__may_alias__)) uint32_t *k = (const __attribute__((__may_alias__)) uint32_t *)key;

    // all but last block: aligned reads and affect 32 bits of (a,b,c)
    while (length > 12) {
      a += KK(0);
      b += KK(1);
      c += KK(2);
      mix(a, b, c);
      length -= 12;
      k += 3;
    }

    // handle the last (probably partial) block
    /*
     * "k[2]&0xffffff" actually reads beyond the end of the string, but
     * then masks off the part it's not allowed to read.  Because the
     * string is aligned, the masked-off tail is in the same word as the
     * rest of the string.  Every machine with memory protection I've seen
     * does it on word boundaries, so is OK with this.  But VALGRIND will
     * still catch it and complain.  The masking trick does make the hash
     * noticably faster for short strings (like English words).
     */
#ifndef VALGRIND
    switch (length) {
      case 12: c += KK(2); b += KK(1); a += KK(0); break;
      case 11: c += KK(2)&0xffffffU; b += KK(1); a += KK(0); break;
      case 10: c += KK(2)&0xffffU; b += KK(1); a += KK(0); break;
      case 9: c += KK(2)&0xffU; b += KK(1); a += KK(0); break;
      case 8: b += KK(1); a += KK(0); break;
      case 7: b += KK(1)&0xffffffU; a += KK(0); break;
      case 6: b += KK(1)&0xffffU; a += KK(0); break;
      case 5: b += KK(1)&0xffU; a += KK(0); break;
      case 4: a += KK(0); break;
      case 3: a += KK(0)&0xffffffU; break;
      case 2: a += KK(0)&0xffffU; break;
      case 1: a += KK(0)&0xffU; break;
      case 0: return c; // zero length strings require no mixing
    }
#else /* make valgrind happy */
    const __attribute__((__may_alias__)) uint8_t *k8 = (const __attribute__((__may_alias__)) uint8_t *)k;
    switch (length) {
      case 12: c += KK(2); b += KK(1); a += KK(0); break;
      case 11: c += ((uint32_t)K8(10))<<16; /* fall through */
      case 10: c += ((uint32_t)K8(9))<<8; /* fall through */
      case 9: c += K8(8); /* fall through */
      case 8: b += KK(1); a += KK(0); break;
      case 7: b += ((uint32_t)K8(6))<<16; /* fall through */
      case 6: b += ((uint32_t)K8(5))<<8; /* fall through */
      case 5: b += K8(4); /* fall through */
      case 4: a += KK(0); break;
      case 3: a += ((uint32_t)K8(2))<<16; /* fall through */
      case 2: a += ((uint32_t)K8(1))<<8; /* fall through */
      case 1: a += K8(0); break;
      case 0: return c;
    }
#endif /* !valgrind */
  } else if ((((uintptr_t)key)&0x01u) == 0) {
    // aligned by 2 bytes; process by 16-bit chunks
    const __attribute__((__may_alias__)) uint16_t *k6 = (const __attribute__((__may_alias__)) uint16_t *)key; /* read 16-bit chunks */

    // all but last block: aligned reads and different mixing
    while (length > 12) {
      a += K6(0)+(((uint32_t)K6(1))<<16);
      b += K6(2)+(((uint32_t)K6(3))<<16);
      c += K6(4)+(((uint32_t)K6(5))<<16);
      mix(a, b, c);
      length -= 12;
      k6 += 6;
    }

    // handle the last (probably partial) block
    const __attribute__((__may_alias__)) uint8_t *k8 = (const __attribute__((__may_alias__)) uint8_t *)k6;
    switch (length) {
      case 12:
        c += K6(4)+(((uint32_t)K6(5))<<16);
        b += K6(2)+(((uint32_t)K6(3))<<16);
        a += K6(0)+(((uint32_t)K6(1))<<16);
        break;
      case 11: c += ((uint32_t)K8(10))<<16; /* fall through */
      case 10:
        c += K6(4);
        b += K6(2)+(((uint32_t)K6(3))<<16);
        a += K6(0)+(((uint32_t)K6(1))<<16);
        break;
      case 9: c += K8(8); /* fall through */
      case 8:
        b += K6(2)+(((uint32_t)K6(3))<<16);
        a += K6(0)+(((uint32_t)K6(1))<<16);
        break;
      case 7: b += ((uint32_t)K8(6))<<16; /* fall through */
      case 6:
        b += K6(2);
        a += K6(0)+(((uint32_t)K6(1))<<16);
        break;
      case 5: b += K8(4); /* fall through */
      case 4:
        a += K6(0)+(((uint32_t)K6(1))<<16);
        break;
      case 3: a += ((uint32_t)K8(2))<<16; /* fall through */
      case 2: a += K6(0); break;
      case 1: a += K8(0); break;
      case 0: return c; /* zero length requires no mixing */
    }
  } else {
    // need to read the key one byte at a time
    const __attribute__((__may_alias__)) uint8_t *k8 = (const __attribute__((__may_alias__)) uint8_t *)key;

    // all but the last block: affect some 32 bits of (a,b,c)
    while (length > 12) {
      a += K8(0);
      a += ((uint32_t)K8(1))<<8;
      a += ((uint32_t)K8(2))<<16;
      a += ((uint32_t)K8(3))<<24;
      b += K8(4);
      b += ((uint32_t)K8(5))<<8;
      b += ((uint32_t)K8(6))<<16;
      b += ((uint32_t)K8(7))<<24;
      c += K8(8);
      c += ((uint32_t)K8(9))<<8;
      c += ((uint32_t)K8(10))<<16;
      c += ((uint32_t)K8(11))<<24;
      mix(a, b, c);
      length -= 12;
      k8 += 12;
    }

    // last block: affect all 32 bits of (c)
    switch (length) {
      case 12: c += ((uint32_t)K8(11))<<24; /* fall through */
      case 11: c += ((uint32_t)K8(10))<<16; /* fall through */
      case 10: c += ((uint32_t)K8(9))<<8; /* fall through */
      case 9: c += K8(8); /* fall through */
      case 8: b += ((uint32_t)K8(7))<<24; /* fall through */
      case 7: b += ((uint32_t)K8(6))<<16; /* fall through */
      case 6: b += ((uint32_t)K8(5))<<8; /* fall through */
      case 5: b += K8(4); /* fall through */
      case 4: a += ((uint32_t)K8(3))<<24; /* fall through */
      case 3: a += ((uint32_t)K8(2))<<16; /* fall through */
      case 2: a += ((uint32_t)K8(1))<<8; /* fall through */
      case 1: a += K8(0); break;
      case 0: return c;
    }
  }

  final(a, b, c);
  return c;

  #undef KK
  #undef K8
  #undef K6
}


/*
 * hashlittle2: return 2 32-bit hash values
 *
 * This is identical to hashlittle(), except it returns two 32-bit hash
 * values instead of just one.  This is good enough for hash table
 * lookup with 2^^64 buckets, or if you want a second hash if you're not
 * happy with the first, or if you want a probably-unique 64-bit ID for
 * the key.  *pc is better mixed than *pb, so use *pc first.  If you want
 * a 64-bit value do something like "*pc + (((uint64_t)*pb)<<32)".
 */
void bjHashBuf64P (
  const __attribute__((__may_alias__)) void *key, /* the key to hash */
  size_t length, /* length of the key */
  uint32_t *pc, /* IN: primary initval, OUT: primary hash */
  uint32_t *pb) noexcept /* IN: secondary initval, OUT: secondary hash */
{
  uint32_t a, b, c; /* internal state */

  /* set up the internal state */
  a = b = c = 0xdeadbeefU+((uint32_t)length)+(*pc);
  c += *pb;

  if ((((uintptr_t)key)&0x03u) == 0) {
    // aligned by 4 bytes
    const __attribute__((__may_alias__)) uint32_t *k = (const __attribute__((__may_alias__)) uint32_t *)key; /* read 32-bit chunks */

    /*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */
    while (length > 12) {
      a += k[0];
      b += k[1];
      c += k[2];
      mix(a, b, c);
      length -= 12;
      k += 3;
    }

    /*----------------------------- handle the last (probably partial) block */
    /*
     * "k[2]&0xffffff" actually reads beyond the end of the string, but
     * then masks off the part it's not allowed to read.  Because the
     * string is aligned, the masked-off tail is in the same word as the
     * rest of the string.  Every machine with memory protection I've seen
     * does it on word boundaries, so is OK with this.  But VALGRIND will
     * still catch it and complain.  The masking trick does make the hash
     * noticably faster for short strings (like English words).
     */
#ifndef VALGRIND
    switch (length) {
      case 12: c += k[2]; b += k[1]; a += k[0]; break;
      case 11: c += k[2]&0xffffffU; b += k[1]; a += k[0]; break;
      case 10: c += k[2]&0xffffU; b += k[1]; a += k[0]; break;
      case 9: c += k[2]&0xffU; b += k[1]; a += k[0]; break;
      case 8: b += k[1]; a += k[0]; break;
      case 7: b += k[1]&0xffffffU; a += k[0]; break;
      case 6: b += k[1]&0xffffU; a += k[0]; break;
      case 5: b += k[1]&0xffU; a += k[0]; break;
      case 4: a += k[0]; break;
      case 3: a += k[0]&0xffffffU; break;
      case 2: a += k[0]&0xffffU; break;
      case 1: a += k[0]&0xffU; break;
      case 0: *pc = c; *pb = b; return; /* zero length strings require no mixing */
    }
#else /* make valgrind happy */
    const __attribute__((__may_alias__)) uint8_t *k8 = (const __attribute__((__may_alias__)) uint8_t *)k;
    switch (length) {
      case 12: c += k[2]; b += k[1]; a += k[0]; break;
      case 11: c += ((uint32_t)k8[10])<<16; /* fall through */
      case 10: c += ((uint32_t)k8[9])<<8; /* fall through */
      case 9: c += k8[8]; /* fall through */
      case 8: b += k[1]; a += k[0]; break;
      case 7: b += ((uint32_t)k8[6])<<16; /* fall through */
      case 6: b += ((uint32_t)k8[5])<<8; /* fall through */
      case 5: b += k8[4]; /* fall through */
      case 4: a += k[0]; break;
      case 3: a += ((uint32_t)k8[2])<<16; /* fall through */
      case 2: a += ((uint32_t)k8[1])<<8; /* fall through */
      case 1: a += k8[0]; break;
      case 0: *pc = c; *pb = b; return; /* zero length strings require no mixing */
    }
#endif /* !valgrind */
  } else if ((((uintptr_t)key)&0x01u) == 0) {
    // aligned by 2 bytes
    const __attribute__((__may_alias__)) uint16_t *k = (const __attribute__((__may_alias__)) uint16_t *)key; /* read 16-bit chunks */

    /*--------------- all but last block: aligned reads and different mixing */
    while (length > 12) {
      a += k[0]+(((uint32_t)k[1])<<16);
      b += k[2]+(((uint32_t)k[3])<<16);
      c += k[4]+(((uint32_t)k[5])<<16);
      mix(a, b, c);
      length -= 12;
      k += 6;
    }

    /*----------------------------- handle the last (probably partial) block */
    const __attribute__((__may_alias__)) uint8_t *k8 = (const __attribute__((__may_alias__)) uint8_t *)k;
    switch (length) {
      case 12:
        c += k[4]+(((uint32_t)k[5])<<16);
        b += k[2]+(((uint32_t)k[3])<<16);
        a += k[0]+(((uint32_t)k[1])<<16);
        break;
      case 11: c += ((uint32_t)k8[10])<<16; /* fall through */
      case 10:
        c += k[4];
        b += k[2]+(((uint32_t)k[3])<<16);
        a += k[0]+(((uint32_t)k[1])<<16);
        break;
      case 9: c += k8[8]; /* fall through */
      case 8:
        b += k[2]+(((uint32_t)k[3])<<16);
        a += k[0]+(((uint32_t)k[1])<<16);
        break;
      case 7: b += ((uint32_t)k8[6])<<16; /* fall through */
      case 6:
        b += k[2];
        a += k[0]+(((uint32_t)k[1])<<16);
        break;
      case 5: b += k8[4]; /* fall through */
      case 4:
        a += k[0]+(((uint32_t)k[1])<<16);
        break;
      case 3: a += ((uint32_t)k8[2])<<16; /* fall through */
      case 2: a += k[0]; break;
      case 1: a += k8[0]; break;
      case 0: *pc = c; *pb = b; return;  /* zero length strings require no mixing */
    }
  } else {
    /* need to read the key one byte at a time */
    const __attribute__((__may_alias__)) uint8_t *k = (const __attribute__((__may_alias__)) uint8_t *)key;

    /*--------------- all but the last block: affect some 32 bits of (a,b,c) */
    while (length > 12) {
      a += k[0];
      a += ((uint32_t)k[1])<<8;
      a += ((uint32_t)k[2])<<16;
      a += ((uint32_t)k[3])<<24;
      b += k[4];
      b += ((uint32_t)k[5])<<8;
      b += ((uint32_t)k[6])<<16;
      b += ((uint32_t)k[7])<<24;
      c += k[8];
      c += ((uint32_t)k[9])<<8;
      c += ((uint32_t)k[10])<<16;
      c += ((uint32_t)k[11])<<24;
      mix(a, b, c);
      length -= 12;
      k += 12;
    }

    /*-------------------------------- last block: affect all 32 bits of (c) */
    /* all the case statements fall through */
    switch (length) {
      case 12: c += ((uint32_t)k[11])<<24; /* fall through */
      case 11: c += ((uint32_t)k[10])<<16; /* fall through */
      case 10: c += ((uint32_t)k[9])<<8; /* fall through */
      case 9: c += k[8]; /* fall through */
      case 8: b += ((uint32_t)k[7])<<24; /* fall through */
      case 7: b += ((uint32_t)k[6])<<16; /* fall through */
      case 6: b += ((uint32_t)k[5])<<8; /* fall through */
      case 5: b += k[4]; /* fall through */
      case 4: a += ((uint32_t)k[3])<<24; /* fall through */
      case 3: a += ((uint32_t)k[2])<<16; /* fall through */
      case 2: a += ((uint32_t)k[1])<<8; /* fall through */
      case 1: a += k[0]; break;
      case 0: *pc = c; *pb = b; return; /* zero length strings require no mixing */
    }
  }

  final(a, b, c);
  *pc = c;
  *pb = b;
}


uint64_t bjHashBuf64 (const __attribute__((__may_alias__)) void *key, size_t length, uint64_t initval) noexcept {
  uint32_t pc = (uint32_t)initval, pb = (uint32_t)(initval>>32);
  bjHashBuf64P(key, length, &pc, &pb);
  return ((uint64_t)pc)+(((uint64_t)pb)<<32);
}


uint64_t bjHashU32v64 (const __attribute__((__may_alias__)) uint32_t *k, size_t count, uint64_t initval) noexcept {
  uint32_t pc = (uint32_t)initval, pb = (uint32_t)(initval>>32);
  bjHashU32v64P(k, count, &pc, &pb);
  return ((uint64_t)pc)+(((uint64_t)pb)<<32);
}


// hash one 32-bit value
uint32_t bjHashU32 (uint32_t k, uint32_t initval) noexcept {
  uint32_t a, b, c;
  a = b = c = 0xdeadbeefU+(((uint32_t)1u/*count*/)<<2)+initval;
  a += k;
  final(a, b, c);
  return c;
}


// hash one 32-bit value into two 32-bit values
void bjHashU3264P (const uint32_t k, uint32_t *pc, uint32_t *pb) noexcept {
  uint32_t a, b, c;
  a = b = c = 0xdeadbeefU+((uint32_t)(1u/*count*/<<2))+(*pc);
  c += *pb;
  a += k;
  final(a, b, c);
  *pc = c;
  *pb = b;
}


// hash one 32-bit value into two 32-bit values
uint64_t bjHashU3264 (const uint32_t k, uint64_t initval) noexcept {
  uint32_t pc = (uint32_t)initval, pb = (uint32_t)(initval>>32);
  bjHashU3264P(k, &pc, &pb);
  return ((uint64_t)pc)+(((uint64_t)pb)<<32);
}
