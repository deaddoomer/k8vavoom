// RIPEMD-160/RIPEMD-320 secure cryptographic hash, based on the algorithm description
// public domain
#ifndef RMD160_HEADER_FILE
#define RMD160_HEADER_FILE

#include <stddef.h>
#include <stdint.h>

#if defined (__cplusplus)
extern "C" {
#endif


#define RIPEMD160_BITS   (160)
// buffer size for RIPEMD-160 hash, in bytes
#define RIPEMD160_BYTES  (RIPEMD160_BITS>>3)


#define RIPEMD320_BITS   (320)
// buffer size for RIPEMD-320 hash, in bytes
#define RIPEMD320_BYTES  (RIPEMD320_BITS>>3)


typedef uint8_t RIPEMD160_hash[RIPEMD160_BYTES];
typedef uint8_t RIPEMD320_hash[RIPEMD320_BYTES];


// RIPEMD-160 context
typedef struct {
  uint32_t wkbuf[RIPEMD160_BYTES>>2];
  uint8_t chunkd[64];
  uint32_t chunkpos;
  uint32_t total;
  uint32_t totalhi;
} RIPEMD160_Ctx;


// RIPEMD-3200 context
typedef struct {
  uint32_t wkbuf[RIPEMD320_BYTES>>2];
  uint8_t chunkd[64];
  uint32_t chunkpos;
  uint32_t total;
  uint32_t totalhi;
} RIPEMD320_Ctx;


// Initialize RIPEMD-160 context.
// This also can be used to wipe the context data.
// If `ctx` is NULL, this will segfault.
void ripemd160_init (RIPEMD160_Ctx *ctx);

// Update RIPEMD-160 with some data.
// If either `ctx` or `data` is NULL, this will segfault.
//
// WARNING!
//   Maximum number of bytes RIPEMD-160 can digest is 2^61.
//   If you will put more (total) bytes into it, the result
//   will be a garbage.
//   You can use `ripemd160_canput()` to check if there is still enough room.
//   I.e. you can call `ripemd160_canput()` before each call to `ripemd160_put()`
//   to check if it is safe to continue.
void ripemd160_put (RIPEMD160_Ctx *ctx, const void *data, size_t datasize);

// Calculate final hash value (20 bytes).
// `hash` must be at least of `RIPEMD160_BYTES` size (20 bytes).
// You can keep putting data to `ctx` after this (hence the `const` here).
// If either `ctx` or `hash` is NULL, this will do segfault.
void ripemd160_finish (const RIPEMD160_Ctx *ctx, void *hash);

// Check if there is still enough room for `datasize` bytes.
// If `ctx` is NULL, will return segfault.
int ripemd160_canput (const RIPEMD160_Ctx *ctx, const size_t datasize);


// ///////////////////////////////////////////////////////////////////////// //
// PLEASE, NOTE: RIPEMD-320 is *IN* *NO* *WAY* more secure than RIPEMD-160.
// actually, this is the same hash function, just used to produce bigger output.
// that is, it can be used when you need more than 20 bytes of the result.
// it has the same speed as RIPEMD-160, and less correlation than running
// two instances of RIPEMD-160 (or hashing the result of RIPEMD-160 to get
// another 20 bytes).

// Initialize RIPEMD-320 context.
// This also can be used to wipe the context data.
// If `ctx` is NULL, this will segfault.
void ripemd320_init (RIPEMD320_Ctx *ctx);

// Update RIPEMD-320 with some data.
// If either `ctx` or `data` is NULL, this will segfault.
//
// WARNING!
//   Maximum number of bytes RIPEMD-320 can digest is 2^61.
//   If you will put more (total) bytes into it, the result
//   will be a garbage.
//   You can use `ripemd320_canput()` to check if there is still enough room.
//   I.e. you can call `ripemd320_canput()` before each call to `ripemd320_put()`
//   to check if it is safe to continue.
void ripemd320_put (RIPEMD320_Ctx *ctx, const void *data, size_t datasize);

// Calculate final hash value (20 bytes).
// `hash` must be at least of `RIPEMD320_BYTES` size (20 bytes).
// You can keep putting data to `ctx` after this (hence the `const` here).
// If either `ctx` or `hash` is NULL, this will do segfault.
void ripemd320_finish (const RIPEMD320_Ctx *ctx, void *hash);

// Check if there is still enough room for `datasize` bytes.
// If `ctx` is NULL, will return segfault.
int ripemd320_canput (const RIPEMD320_Ctx *ctx, const size_t datasize);


#if defined (__cplusplus)
}
#endif
#endif
