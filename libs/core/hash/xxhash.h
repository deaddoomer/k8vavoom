/*
 * xxHash - Extremely Fast Hash algorithm
 * Header File
 * Copyright (C) 2012-2020 Yann Collet
 *
 * BSD 2-Clause License (https://www.opensource.org/licenses/bsd-license.php)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You can contact the author at:
 *   - xxHash homepage: https://www.xxhash.com
 *   - xxHash source repository: https://github.com/Cyan4973/xxHash
 */
/* ****************************************************************
 *  Stable API
 *****************************************************************/
#ifndef XXHASH_H_5627135585666179
#define XXHASH_H_5627135585666179 1

#if defined (__cplusplus)
extern "C" {
#endif


/* *************************************
*  Version
***************************************/
#define XXH_VERSION_MAJOR    0
#define XXH_VERSION_MINOR    8
#define XXH_VERSION_RELEASE  1
#define XXH_VERSION_NUMBER  (XXH_VERSION_MAJOR *100*100 + XXH_VERSION_MINOR *100 + XXH_VERSION_RELEASE)

#define XXH_PUBLIC_API

/* ****************************
*  Definitions
******************************/
#include <stddef.h>   /* size_t */
typedef enum { XXH_OK=0, XXH_ERROR } XXH_errorcode;


/*-**********************************************************************
*  32-bit hash
************************************************************************/
#include <stdint.h>
typedef uint32_t XXH32_hash_t;

/*!
 * @}
 *
 * @defgroup xxh32_family XXH32 family
 * @ingroup public
 * Contains functions used in the classic 32-bit xxHash algorithm.
 *
 * @note
 *   XXH32 is considered rather weak by today's standards.
 *   The @ref xxh3_family provides competitive speed for both 32-bit and 64-bit
 *   systems, and offers true 64/128 bit hash results. It provides a superior
 *   level of dispersion, and greatly reduces the risks of collisions.
 *
 * @see @ref xxh64_family, @ref xxh3_family : Other xxHash families
 * @see @ref xxh32_impl for implementation details
 * @{
 */

/*!
 * @brief Calculates the 32-bit hash of @p input using xxHash32.
 *
 * Speed on Core 2 Duo @ 3 GHz (single thread, SMHasher benchmark): 5.4 GB/s
 *
 * @param input The block of data to be hashed, at least @p length bytes in size.
 * @param length The length of @p input, in bytes.
 * @param seed The 32-bit seed to alter the hash's output predictably.
 *
 * @pre
 *   The memory between @p input and @p input + @p length must be valid,
 *   readable, contiguous memory. However, if @p length is `0`, @p input may be
 *   `NULL`. In C++, this also must be *TriviallyCopyable*.
 *
 * @return The calculated 32-bit hash value.
 *
 * @see
 *    XXH64(), XXH3_64bits_withSeed(), XXH3_128bits_withSeed(), XXH128():
 *    Direct equivalents for the other variants of xxHash.
 * @see
 *    XXH32_createState(), XXH32_update(), XXH32_digest(): Streaming version.
 */
XXH_PUBLIC_API XXH32_hash_t XXH32 (const void* input, size_t length, XXH32_hash_t seed);

/*!
 * Streaming functions generate the xxHash value from an incremental input.
 * This method is slower than single-call functions, due to state management.
 * For small inputs, prefer `XXH32()` and `XXH64()`, which are better optimized.
 *
 * An XXH state must first be allocated using `XXH*_createState()`.
 *
 * Start a new hash by initializing the state with a seed using `XXH*_reset()`.
 *
 * Then, feed the hash state by calling `XXH*_update()` as many times as necessary.
 *
 * The function returns an error code, with 0 meaning OK, and any other value
 * meaning there is an error.
 *
 * Finally, a hash value can be produced anytime, by using `XXH*_digest()`.
 * This function returns the nn-bits hash as an int or long long.
 *
 * It's still possible to continue inserting input into the hash state after a
 * digest, and generate new hash values later on by invoking `XXH*_digest()`.
 *
 * When done, release the state using `XXH*_freeState()`.
 *
 * Example code for incrementally hashing a file:
 * @code{.c}
 *    #include <stdio.h>
 *    #include <xxhash.h>
 *    #define BUFFER_SIZE 256
 *
 *    // Note: XXH64 and XXH3 use the same interface.
 *    XXH32_hash_t
 *    hashFile(FILE* stream)
 *    {
 *        XXH32_state_t* state;
 *        unsigned char buf[BUFFER_SIZE];
 *        size_t amt;
 *        XXH32_hash_t hash;
 *
 *        state = XXH32_createState();       // Create a state
 *        assert(state != NULL);             // Error check here
 *        XXH32_reset(state, 0xbaad5eed);    // Reset state with our seed
 *        while ((amt = fread(buf, 1, sizeof(buf), stream)) != 0) {
 *            XXH32_update(state, buf, amt); // Hash the file in chunks
 *        }
 *        hash = XXH32_digest(state);        // Finalize the hash
 *        XXH32_freeState(state);            // Clean up
 *        return hash;
 *    }
 * @endcode
 */

/*!
 * @typedef struct XXH32_state_s XXH32_state_t
 * @brief The opaque state struct for the XXH32 streaming API.
 *
 * @see XXH32_state_s for details.
 */
typedef struct XXH32_state_s XXH32_state_t;

/* ****************************************************************************
 * This section contains declarations which are not guaranteed to remain stable.
 * They may change in future versions, becoming incompatible with a different
 * version of the library.
 * These declarations should only be used with static linking.
 * Never use them in association with dynamic linking!
 ***************************************************************************** */

/*
 * These definitions are only present to allow static allocation
 * of XXH states, on stack or in a struct, for example.
 * Never **ever** access their members directly.
 */

/*!
 * @internal
 * @brief Structure for XXH32 streaming API.
 *
 * @note This is only defined when @ref XXH_STATIC_LINKING_ONLY,
 * @ref XXH_INLINE_ALL, or @ref XXH_IMPLEMENTATION is defined. Otherwise it is
 * an opaque type. This allows fields to safely be changed.
 *
 * Typedef'd to @ref XXH32_state_t.
 * Do not access the members of this struct directly.
 * @see XXH64_state_s, XXH3_state_s
 */
struct XXH32_state_s {
   XXH32_hash_t total_len_32; /*!< Total length hashed, modulo 2^32 */
   XXH32_hash_t large_len;    /*!< Whether the hash is >= 16 (handles @ref total_len_32 overflow) */
   XXH32_hash_t v1;           /*!< First accumulator lane */
   XXH32_hash_t v2;           /*!< Second accumulator lane */
   XXH32_hash_t v3;           /*!< Third accumulator lane */
   XXH32_hash_t v4;           /*!< Fourth accumulator lane */
   XXH32_hash_t mem32[4];     /*!< Internal buffer for partial reads. Treated as unsigned char[16]. */
   XXH32_hash_t memsize;      /*!< Amount of data in @ref mem32 */
   XXH32_hash_t reserved;     /*!< Reserved field. Do not read or write to it, it may be removed. */
};   /* typedef'd to XXH32_state_t */

/* ****************************************************************************
 * end of exposed internal state
 ***************************************************************************** */


/*!
 * @brief Allocates an @ref XXH32_state_t.
 *
 * Must be freed with XXH32_freeState().
 * @return An allocated XXH32_state_t on success, `NULL` on failure.
 */
//XXH_PUBLIC_API XXH32_state_t* XXH32_createState(void);
/*!
 * @brief Frees an @ref XXH32_state_t.
 *
 * Must be allocated with XXH32_createState().
 * @param statePtr A pointer to an @ref XXH32_state_t allocated with @ref XXH32_createState().
 * @return XXH_OK.
 */
//XXH_PUBLIC_API XXH_errorcode  XXH32_freeState(XXH32_state_t* statePtr);
/*!
 * @brief Copies one @ref XXH32_state_t to another.
 *
 * @param dst_state The state to copy to.
 * @param src_state The state to copy from.
 * @pre
 *   @p dst_state and @p src_state must not be `NULL` and must not overlap.
 */
XXH_PUBLIC_API void XXH32_copyState (XXH32_state_t *dst_state, const XXH32_state_t *src_state);

/*!
 * @brief Resets an @ref XXH32_state_t to begin a new hash.
 *
 * This function resets and seeds a state. Call it before @ref XXH32_update().
 *
 * @param statePtr The state struct to reset.
 * @param seed The 32-bit seed to alter the hash result predictably.
 *
 * @pre
 *   @p statePtr must not be `NULL`.
 *
 * @return @ref XXH_OK on success, @ref XXH_ERROR on failure.
 */
XXH_PUBLIC_API XXH_errorcode XXH32_reset (XXH32_state_t *statePtr, XXH32_hash_t seed);

/*!
 * @brief Consumes a block of @p input to an @ref XXH32_state_t.
 *
 * Call this to incrementally consume blocks of data.
 *
 * @param statePtr The state struct to update.
 * @param input The block of data to be hashed, at least @p length bytes in size.
 * @param length The length of @p input, in bytes.
 *
 * @pre
 *   @p statePtr must not be `NULL`.
 * @pre
 *   The memory between @p input and @p input + @p length must be valid,
 *   readable, contiguous memory. However, if @p length is `0`, @p input may be
 *   `NULL`. In C++, this also must be *TriviallyCopyable*.
 *
 * @return @ref XXH_OK on success, @ref XXH_ERROR on failure.
 */
XXH_PUBLIC_API XXH_errorcode XXH32_update (XXH32_state_t *statePtr, const __attribute__((__may_alias__)) void *input, size_t length);

/*!
 * @brief Returns the calculated hash value from an @ref XXH32_state_t.
 *
 * @note
 *   Calling XXH32_digest() will not affect @p statePtr, so you can update,
 *   digest, and update again.
 *
 * @param statePtr The state struct to calculate the hash from.
 *
 * @pre
 *  @p statePtr must not be `NULL`.
 *
 * @return The calculated xxHash32 value from that state.
 */
XXH_PUBLIC_API XXH32_hash_t XXH32_digest (const XXH32_state_t *statePtr);


#if defined (__cplusplus)
}
#endif


#endif
