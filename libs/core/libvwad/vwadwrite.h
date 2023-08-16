/* coded by Ketmar // Invisible Vector (psyc://ketmar.no-ip.org/~Ketmar)
 * Understanding is not required. Only obedience.
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar.
 * See http://www.wtfpl.net/ for more details.
 */
/*
  VWADs are chunked archives with zlib-comparable compression ratio.
  usually VWAD size on max copression level is little lesser than
  the corresponding ZIP file. the most useful feature of VWAD is the
  ability to read files non-sequentially without unpacking the whole
  file first. i.e. seeking in archive files is quite cheap, and can
  be used freely.

  any archive can be signed with Ed25519 digital signature. please,
  note that you have to provide good cryptographically strong PRNG
  by yourself (actually, you should use it to generate a private key,
  the library itself doesn't have any PRNG generator).

  note that while archive chunks are "encrypted", this "encryption"
  is not cryptographically strong, and used mostly to hide non-compressed
  data from simple viewing tools.

  archived files can be annotated with arbitrary "group name". this can
  be used in various content creation tools. group tags are not used
  by the library itself. group names are case-insensitive.

  also, archived files can have a 64-bit last modification timestamp
  (seconds since Unix Epoch). you can use zero as timestamp if you
  don't care.

  file names can containprintable unicode chars from the first plane
  (see `vwadwr_is_uni_printable()`), and names are case-insensitive.
  all names should be utf-8 encoded. also note that whice case folding
  is unicode-aware, not all codepoints are implemented. latin-1 and
  basic cyrillic should be safe, though.
  path delimiter is "/" (and only "/").
  you can use `vwadwr_is_valid_file_name()` to check filename validity,
  and `vwadwr_is_valid_group_name()` to check group name validity.


  archive can be tagged with author name, short description, and a comment.
  author name and description can contain unicode chars, but cannot have
  control chars (with codes < 31).
  comments can be multiline (only '\x0a' is allowed as a newline char).
  tabs are allowed too.
  you can use `vwadwr_is_valid_comment()` to check if comment text is valid.

  currently, only limited subset of the first unicode plane is supported.
  deal with it.

  WARNING! WARNING! WARNING! WARNING! WARNING! WARNING! WARNING! WARNING!
  the reader is not thread-safe. i.e. you cannot use opened handle to
  read different files in different threads without locking. but different
  handles for different threads are allowed.
  WARNING! WARNING! WARNING! WARNING! WARNING! WARNING! WARNING! WARNING!
*/
#ifndef VWADWRITER_HEADER
#define VWADWRITER_HEADER

#include <stdio.h>
#include <stdlib.h>


#if defined(__cplusplus)
extern "C" {
#endif


// WARNING! if compiler complains with something like "size of unnamed array is negative",
//          it means that type sizes are wrong!

// this should be 8 bit (i hope so)
typedef unsigned char vwadwr_ubyte;
// this should be 16 bit (i hope so)
typedef unsigned short vwadwr_ushort;
// this should be 32 bit (i hope so)
typedef unsigned int vwadwr_uint;
// this should be 64 bit (i hope so)
typedef unsigned long long vwadwr_uint64;

// size checks
typedef char vwadwr_temp_typedef_check_char[(sizeof(char) == 1) ? 1 : -1];
typedef char vwadwr_temp_typedef_check_ubyte[(sizeof(vwadwr_ubyte) == 1) ? 1 : -1];
typedef char vwadwr_temp_typedef_check_ushort[(sizeof(vwadwr_ushort) == 2) ? 1 : -1];
typedef char vwadwr_temp_typedef_check_uint[(sizeof(vwadwr_uint) == 4) ? 1 : -1];
typedef char vwadwr_temp_typedef_check_uint64[(sizeof(vwadwr_uint64) == 8) ? 1 : -1];

// this should be 64 bit (i hope so)
typedef vwadwr_uint64 vwadwr_ftime;

// for digital signatures
typedef vwadwr_ubyte vwadwr_public_key[32];
typedef vwadwr_ubyte vwadwr_secret_key[32];

// 40 bytes of key, 5 bytes of crc32, plus zero
typedef char vwadwr_z85_key[46];


// for self-documentation purposes
typedef int vwadwr_bool;

// for self-documentation purposes
// 0 is success, negative value is error
typedef int vwadwr_result;


// opacue directory handle
typedef struct vwadwr_dir_t vwadwr_dir;


// i/o stream
typedef struct vwadwr_iostream_t vwadwr_iostream;
struct vwadwr_iostream_t {
  // return non-zero on failure; failure can be delayed
  // will never be called with negative `pos`
  vwadwr_result (*seek) (vwadwr_iostream *strm, int pos);
  // return negative on failure
  int (*tell) (vwadwr_iostream *strm);
  // read at most bufsize bytes; return number of read bytes, or negative on failure
  // will never be called with zero or negative `bufsize`
  // this is used only for creating digital signatures;
  // if you don't need digital signatures, you can leave this as `NULL`
  int (*read) (vwadwr_iostream *strm, void *buf, int bufsize);
  // write *exactly* bufsize bytes; return 0 on success, negative on failure
  // will never be called with zero or negative `bufsize`
  vwadwr_result (*write) (vwadwr_iostream *strm, const void *buf, int bufsize);
  // user data
  void *udata;
};

// memory allocation
typedef struct vwadwr_memman_t vwadwr_memman;
struct vwadwr_memman_t {
  // will never be called with zero `size`
  // return NULL on OOM
  void *(*alloc) (vwadwr_memman *mman, vwadwr_uint size);
  // will never be called with NULL `p`
  void (*free) (vwadwr_memman *mman, void *p);
  // user data
  void *udata;
};


#define VWADWR_LOG_NOTE     (0)
#define VWADWR_LOG_WARNING  (1)
#define VWADWR_LOG_ERROR    (2)
#define VWADWR_LOG_DEBUG    (3)

// logging; can be NULL
// `type` is the above enum
extern void (*vwadwr_logf) (int type, const char *fmt, ...);

// assertion; can be NULL, then the lib will simply traps
extern void (*vwadwr_assertion_failed) (const char *fmt, ...);


// ////////////////////////////////////////////////////////////////////////// //
// you can encode keys in printable ASCII chars, and decode them back.
// encoded key contains a checksum, so mistyped keys could be detected.
void vwadwr_z85_encode_key (const vwadwr_public_key inkey, vwadwr_z85_key enkey);
vwadwr_result vwadwr_z85_decode_key (const vwadwr_z85_key enkey, vwadwr_public_key outkey);


// ////////////////////////////////////////////////////////////////////////// //
// error codes
#define VWADWR_OK           (0)
// invalid author string
#define VWADWR_ERR_AUTHOR   (-1)
// invalid title string
#define VWADWR_ERR_TITLE    (-2)
// invalid comment text
#define VWADWR_ERR_COMMENT  (-3)
// invalid flags
#define VWADWR_ERR_FLAGS    (-4)
// invalid private key
#define VWADWR_ERR_PRIVKEY  (-5)
// out of memory
#define VWADWR_ERR_MEM      (-6)
// invalid file name
#define VWADWR_ERR_NAME     (-7)
// invalid file group name
#define VWADWR_ERR_GROUP    (-8)
// directory check: bad name align (should not happen)
#define VWADWR_ERR_NAMES_ALIGN  (-9)
// directory check: bad name table size (too big)
#define VWADWR_ERR_NAMES_SIZE   (-10)
// directory check: bad chunk count (too many)
#define VWADWR_ERR_CHUNK_COUNT  (-11)
// directory check: bad file count (too many)
#define VWADWR_ERR_FILE_COUNT   (-12)
// resulting vwad archive is too big
#define VWADWR_ERR_VWAD_TOO_BIG (-13)
// trying to pack file bigger than 4 gb
#define VWADWR_ERR_FILE_TOO_BIG (-14)
// duplicate file name
#define VWADWR_ERR_DUP_FILE     (-15)
// directory too big
#define VWADWR_ERR_DIR_TOO_BIG  (-16)
// invalid ascii key encoding
#define VWADWR_ERR_BAD_ASCII    (-17)
// various i/o errors (i don't care what exactly gone wrong)
#define VWADWR_ERR_IO_ERROR     (-18)
// trying to create new file while old one is still open
#define VWADWR_ERR_FILE_OPEN    (-19)
// trying to writing new file data (or closing new file) without opening it first
#define VWADWR_ERR_FILE_CLOSED  (-20)
// trying to write raw chunk to normal file, or vice versa
#define VWADWR_ERR_INVALID_MODE (-21)
// function argument error (some pointer is NULL, or such)
#define VWADWR_ERR_ARGS         (-669)
// some other error
#define VWADWR_ERR_OTHER       (-666)


// ////////////////////////////////////////////////////////////////////////// //
// various validators

// check if the provided encryption/signing key is "good enough".
// note that trying to create an archive with "non-good" key will fail.
// good crypto PRNG will almost always generate good keys.
vwadwr_bool vwadwr_is_good_privkey (const vwadwr_secret_key privkey);

// use this to check if archived file name is valid.
// archive file name should not start with slash,
// should not end with slash, should not be empty,
// should not be longer than 255 chars, and
// should not contain double slashes.
vwadwr_bool vwadwr_is_valid_file_name (const char *str);

// use this to check if archived group name is valid.
// group name not be longer than 255 chars.
// `NULL` is valid group name.
vwadwr_bool vwadwr_is_valid_group_name (const char *str);

// check if comment text is valid.
// `NULL` is valid comment text.
vwadwr_bool vwadwr_is_valid_comment (const char *str);


// ////////////////////////////////////////////////////////////////////////// //
/* the algo is like this:
    vwadwr_new_dir
    add files
    finish archive creation with vwadwr_finish, or free dir with vwadwr_free_dir
*/

// flags for `vwadwr_new_dir()`
#define VWADWR_NEW_DEFAULT    (0u)
#define VWADWR_NEW_DONT_SIGN  (0x4000u)

// if `mman` is `NULL`, use libc memory manager
// will return `NULL` on any error (including invalid comment)
// `privkey` cannot be NULL, and should be filled with GOOD random data
vwadwr_dir *vwadwr_new_dir (vwadwr_memman *mman, vwadwr_iostream *outstrm,
                            const char *author, /* can be NULL */
                            const char *title, /* can be NULL */
                            const char *comment, /* can be NULL */
                            vwadwr_uint flags,
                            /* cannot be NULL; must ALWAYS be filled with GOOD random bytes */
                            const vwadwr_secret_key privkey,
                            vwadwr_public_key respubkey, /* OUT; can be NULL */
                            int *error); /* OUT; can be NULL */

// this can be used to abort archive creation. it doesn't write FAT,
// just frees all memory.
// `*dirp` will be set to NULL.
// it is safe to pass `NULL` as `dirp` and/or as `*dirp`.
void vwadwr_free_dir (vwadwr_dir **dirp);

// get memory manager for the given archive
vwadwr_memman *vwadwr_dir_get_memman (vwadwr_dir *dir);
// get i/o stream for the given archive.
vwadwr_iostream *vwadwr_dir_get_outstrm (vwadwr_dir *dir);

// you can call this befire `vwadwr_free_dir()` to perform basic checks.
vwadwr_bool vwadwr_is_valid_dir (const vwadwr_dir *dir);

// this is the same as above, but returns more detailed failure
// reason using error code.
vwadwr_result vwadwr_check_dir (const vwadwr_dir *dir);

// this will write the FAT directory, and call `vwadwr_free_dir()` automatically
// (even in case of error). it frees handle because there is not much could be
// done with it after calling "finish" anyway.
// `*dirp` will be set to NULL.
// it is safe to pass `NULL` as `dirp` and/or as `*dirp`.
vwadwr_result vwadwr_finish (vwadwr_dir **dirp);


// compression levels.
// PLEASE, do not use numbers directly, i may change them at any time.
/* no compression */
#define VWADWR_COMP_DISABLE  (-1)
/* literal-only mode (silly!) */
#define VWADWR_COMP_FASTEST  (0)
/* no literal-only, with no lazy matching */
#define VWADWR_COMP_FAST     (1)
/* no literal-only, with lazy matching */
#define VWADWR_COMP_MEDIUM   (2)
/* with literal-only, with lazy matching */
#define VWADWR_COMP_BEST     (3)


// can be used during file writing to get some stats
// also, can be used after closing a file to get final stats
int vwadwr_get_last_file_packed_size (vwadwr_dir *dir);
int vwadwr_get_last_file_unpacked_size (vwadwr_dir *dir);
int vwadwr_get_last_file_chunk_count (vwadwr_dir *dir);

// you can use this API to write files with the usual file write-like API.
// please note that you cannot seek backwards in this case, and only
// one file can be created for writing. i.e. you must close the file before
// creating a new one, and data can be only appended.
// this is for `vwadwr_write()`.
// if this returned error, there is no reason to continue; call `vwadwr_free_dir()`
vwadwr_result vwadwr_create_file (vwadwr_dir *dir, int level, /* VWADWR_COMP_xxx */
                                  const char *pkfname,
                                  const char *groupname, /* can be NULL */
                                  vwadwr_ftime ftime); /* can be 0; seconds since Epoch */

// if this returned error, there is no reason to continue; call `vwadwr_free_dir()`
vwadwr_result vwadwr_close_file (vwadwr_dir *dir);

// trying to write 0 bytes is not an error, just a no-op
// (yet some error checking will still be done).
// if this returned error, there is no reason to continue; call `vwadwr_free_dir()`
vwadwr_result vwadwr_write (vwadwr_dir *dir, const void *buf, vwadwr_uint len);

// you can use this API to write files with the usual file write-like API.
// please note that you cannot seek backwards in this case, and only
// one file can be created for writing. i.e. you must close the file before
// creating a new one, and data can be only appended.
// this is for `vwadwr_write_raw_chunk()`.
// if this returned error, there is no reason to continue; call `vwadwr_free_dir()`
vwadwr_result vwadwr_create_raw_file (vwadwr_dir *dir,
                                      const char *pkfname,
                                      const char *groupname, /* can be NULL */
                                      vwadwr_uint filecrc32,
                                      vwadwr_ftime ftime); /* can be 0; seconds since Epoch */

// use to copy raw decrypted chunks from other VWAD(s).
// use raw reading API in reader to obtain chunk data.
// `pksz`, `upksz` and `packed` are exactly what `vwad_get_raw_file_chunk_info()` returns
// `chunk` is what `vwad_read_raw_file_chunk()` read
vwadwr_result vwadwr_write_raw_chunk (vwadwr_dir *dir, const void *chunk,
                                      int pksz, int upksz, int packed);

// if this returned error, there is no reason to continue; call `vwadwr_free_dir()`
vwadwr_result vwadwr_close_raw_file (vwadwr_dir *dir);


// ////////////////////////////////////////////////////////////////////////// //
// the following API is using the libc memory manager

vwadwr_iostream *vwadwr_new_file_stream (FILE *fl);
// this will close the file
vwadwr_result vwadwr_close_file_stream (vwadwr_iostream *strm);
// this will NOT close the file
void vwadwr_free_file_stream (vwadwr_iostream *strm);


// ////////////////////////////////////////////////////////////////////////// //
// your bog stadard CRC32 calculator. uses the same poly as zlib.

vwadwr_uint vwadwr_crc32_init (void);
vwadwr_uint vwadwr_crc32_part (vwadwr_uint crc32, const void *src, vwadwr_uint len);
vwadwr_uint vwadwr_crc32_final (vwadwr_uint crc32);


// ////////////////////////////////////////////////////////////////////////// //
// case-insensitive utf-8 wildcard matching
// returns:
//   -1: malformed pattern
//    0: equal
//    1: not equal
vwadwr_result vwadwr_wildmatch (const char *pat, vwadwr_uint plen, const char *str, vwadwr_uint slen);
vwadwr_result vwadwr_wildmatch_path (const char *pat, vwadwr_uint plen, const char *str, vwadwr_uint slen);


// ////////////////////////////////////////////////////////////////////////// //
// WARNING! the following API works only with unicode plane 1 -- [0..65535]!

// "invalid char" unicode code
#define VWADWR_REPLACEMENT_CHAR  (0x0FFFD)

vwadwr_uint vwadwr_utf_char_len (const void *str);
// advances `strp` at least by one byte
// returns `VWADWR_REPLACEMENT_CHAR` on invalid char
vwadwr_ushort vwadwr_utf_decode (const char **strp);

vwadwr_bool vwadwr_is_uni_printable (vwadwr_ushort ch);
vwadwr_ushort vwadwr_uni_tolower (vwadwr_ushort ch);


#if defined(__cplusplus)
}
#endif
#endif
