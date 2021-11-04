// RIPEMD-160 secure cryptographic hash, based on the algorithm description
// public domain
#ifndef RMD160_HEADER_FILE
#define RMD160_HEADER_FILE

#include <stddef.h>
#include <stdint.h>

#if defined (__cplusplus)
extern "C" {
#endif


#define RIPEMD160_BITS   (160)

/* Buffer size for RIPEMD-160 hash, in bytes.
*/
#define RIPEMD160_BYTES  (RIPEMD160_BITS>>3)


/* RIPEMD-160 context.
*/
typedef struct {
  uint32_t wkbuf[RIPEMD160_BYTES>>2];
  uint32_t chunkd[16];
  uint32_t chunkpos;
  uint32_t total;
  uint32_t totalhi;
} RIPEMD160_Ctx;


/* Initialize RIPEMD-160 context.
** This also can be used to wipe the context data.
** If `ctx` is NULL, this will do nothing.
*/
void ripemd160_init (RIPEMD160_Ctx *ctx);

/* Update RIPEMD-160 with some data.
** If either `ctx` or `data` is NULL, this will do nothing.
**
** WARNING!
**   Maximum number of bytes RIPEMD-160 can digest is 2^61.
**   If you will put more (total) bytes into it, the result
**   will be a garbage.
**   You can use `ripemd160_canput()` to check if there is still enough room.
**   I.e. you can call `ripemd160_canput()` before each call to `ripemd160_put()`
**   to check if it is safe to continue.
*/
void ripemd160_put (RIPEMD160_Ctx *ctx, const void *data, size_t datasize);

/* Calculate final hash value (20 bytes).
** `hash` must be at least of `ELLE_RIPEMD160_BYTES` size (20 bytes).
** You can keep putting data to `ctx` after this (hence the `const` here).
** If either `ctx` or `hash` is NULL, this will do nothing.
*/
void ripemd160_finish (const RIPEMD160_Ctx *ctx, void *hash);

/* Check if there is still enough room for `datasize` bytes.
** If `ctx` is NULL, will return zero.
*/
int ripemd160_canput (const RIPEMD160_Ctx *ctx, const size_t datasize);


#if defined (__cplusplus)
}
#endif
#endif
