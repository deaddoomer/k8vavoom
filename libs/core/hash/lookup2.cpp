/*
-------------------------------------------------------------------------------
lookup2.c, by Bob Jenkins, Public Domain.
--------------------------------------------------------------------
mix -- mix 3 32-bit values reversibly.
For every delta with one or two bits set, and the deltas of all three
  high bits or all three low bits, whether the original value of a,b,c
  is almost all zero or is uniformly distributed,
* If mix() is run forward or backward, at least 32 bits in a,b,c
  have at least 1/4 probability of changing.
* If mix() is run forward, every bit of c will change between 1/3 and
  2/3 of the time.  (Well, 22/100 and 78/100 for some 2-bit deltas.)
mix() was built out of 36 single-cycle latency instructions in a
  structure that could supported 2x parallelism, like so:
      a -= b;
      a -= c; x = (c>>13);
      b -= c; a ^= x;
      b -= a; x = (a<<8);
      c -= a; b ^= x;
      c -= b; x = (b>>13);
      ...
  Unfortunately, superscalar Pentiums and Sparcs can't take advantage
  of that parallelism.  They've also turned some of those single-cycle
  latency instructions into multi-cycle latency instructions.  Still,
  this is the fastest good hash I could find.  There were about 2^^68
  to choose from.  I only looked at a billion or so.
--------------------------------------------------------------------
*/
#define mix(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8); \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12);  \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
}

/*
--------------------------------------------------------------------
hash() -- hash a variable-length key into a 32-bit value
  k       : the key (the unaligned variable-length array of bytes)
  len     : the length of the key, counting by bytes
  initval : can be any 4-byte value
Returns a 32-bit value.  Every bit of the key affects every bit of
the return value.  Every 1-bit and 2-bit delta achieves avalanche.
About 6*len+35 instructions.

The best hash table sizes are powers of 2.  There is no need to do
mod a prime (mod is sooo slow!).  If you need less than 32 bits,
use a bitmask.  For example, if you need only 10 bits, do
  h = (h & hashmask(10));
In which case, the hash table should have hashsize(10) elements.

If you are hashing n strings (uint8_t **)k, do it like this:
  for (i=0, h=0; i<n; ++i) h = hash( k[i], len[i], h);

By Bob Jenkins, 1996.  bob_jenkins@burtleburtle.net.  You may use this
code any way you wish, private, educational, or commercial.  It's free.

See http://burtleburtle.net/bob/hash/evahash.html
Use for hash table lookup, or anything where one collision in 2^^32 is
acceptable.  Do NOT use for cryptographic purposes.
--------------------------------------------------------------------
*/
#include <stddef.h>
#include <stdint.h>


uint32_t bjLookup2Buf (const void *data, size_t length, uint32_t initval) {
  #define KK(n_)  ((uint32_t)k[n_])
  uint32_t a, b, c;

  // set up the internal state
  size_t len = length;
  const uint8_t *k = (const uint8_t *)data;
  a = b = 0x9e3779b9u; // the golden ratio; an arbitrary value
  c = initval;         // the previous hash value

  // handle most of the key
  while (len >= 12) {
    a += (KK(0) +(KK(1)<<8) +(KK(2)<<16) +(KK(3)<<24));
    b += (KK(4) +(KK(5)<<8) +(KK(6)<<16) +(KK(7)<<24));
    c += (KK(8) +(KK(9)<<8) +(KK(10)<<16)+(KK(11)<<24));
    mix(a, b, c);
    k += 12; len -= 12;
  }

  // handle the last 11 bytes
  c += (uint32_t)length;
  // all the case statements fall through
  switch (len) {
  case 11: c += (KK(10)<<24); /* fallthrough */
  case 10: c += (KK(9)<<16); /* fallthrough */
  case 9 : c += (KK(8)<<8); /* fallthrough */
    // the first byte of c is reserved for the length
  case 8 : b += (KK(7)<<24); /* fallthrough */
  case 7 : b += (KK(6)<<16); /* fallthrough */
  case 6 : b += (KK(5)<<8); /* fallthrough */
  case 5 : b += KK(4); /* fallthrough */
  case 4 : a += (KK(3)<<24); /* fallthrough */
  case 3 : a += (KK(2)<<16); /* fallthrough */
  case 2 : a += (KK(1)<<8); /* fallthrough */
  case 1 : a += KK(0); /* fallthrough */
    // case 0: nothing left to add
  }
  mix(a, b, c);

  // report the result
  return c;
  #undef KK
}


uint32_t bjLookup2BufCI (const void *data, size_t length, uint32_t initval) {
  #define KK(n_)  ((uint32_t)(k[n_]|0x20u))
  uint32_t a, b, c;

  // set up the internal state
  size_t len = length;
  const uint8_t *k = (const uint8_t *)data;
  a = b = 0x9e3779b9u; // the golden ratio; an arbitrary value
  c = initval;         // the previous hash value

  // handle most of the key
  while (len >= 12) {
    a += (KK(0) +(KK(1)<<8) +(KK(2)<<16) +(KK(3)<<24));
    b += (KK(4) +(KK(5)<<8) +(KK(6)<<16) +(KK(7)<<24));
    c += (KK(8) +(KK(9)<<8) +(KK(10)<<16)+(KK(11)<<24));
    mix(a, b, c);
    k += 12; len -= 12;
  }

  // handle the last 11 bytes
  c += (uint32_t)length;
  // all the case statements fall through
  switch (len) {
  case 11: c += (KK(10)<<24); /* fallthrough */
  case 10: c += (KK(9)<<16); /* fallthrough */
  case 9 : c += (KK(8)<<8); /* fallthrough */
    // the first byte of c is reserved for the length
  case 8 : b += (KK(7)<<24); /* fallthrough */
  case 7 : b += (KK(6)<<16); /* fallthrough */
  case 6 : b += (KK(5)<<8); /* fallthrough */
  case 5 : b += KK(4); /* fallthrough */
  case 4 : a += (KK(3)<<24); /* fallthrough */
  case 3 : a += (KK(2)<<16); /* fallthrough */
  case 2 : a += (KK(1)<<8); /* fallthrough */
  case 1 : a += KK(0); /* fallthrough */
    // case 0: nothing left to add
  }
  mix(a, b, c);

  // report the result
  return c;
  #undef KK
}
