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
  the library itself doesn't need any external PRNG generator).

  note that while archive chunks are "encrypted", this "encryption"
  is not cryptographically strong, and used mostly to hide non-compressed
  data from simple viewing tools.

  archived files can be annotated with arbitrary "group name". this can
  be used in various content creation tools. group tags are not used
  by the library itself. ASCII chars in group name are case-insensitive.

  also, archived files can have a 64-bit last modification timestamp
  (seconds since Unix Epoch). you can use zero as timestamp if you
  don't care.

  file names can contain only ASCII chars, and names are case-insensitive.
  path delimiter is "/". this limitation is because i don't want to
  include unicode translation tables, or enforce any one-byte encoding.
  this may be changed in the future.

  archive can be tagged with author name, short description, and a comment.
  author name and description can contain only ASCII chars in range [32..127).
  comments can be multiline (only "\x0a" is allowed as a newline char), but
  still limited to ASCII.

  ASCII limitation may be relaxed in next library versions (i will prolly
  enable some unicode plane 1 chars, encoded as UTF-8).
*/
#ifndef VWADWRITER_HEADER
#define VWADWRITER_HEADER

// uncomment to turn off copying from the existing archive
// if uncommented, will remove dependency to "vwadvfs"
//#define VWADWR_DISABLE_COPY_FILES

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef VWADWR_DISABLE_COPY_FILES
# include "vwadvfs.h"
#endif


#if defined(__cplusplus)
extern "C" {
#endif


// for digital signatures
typedef unsigned char vwadwr_public_key[32];
typedef unsigned char vwadwr_secret_key[32];

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
  void *(*alloc) (vwadwr_memman *mman, uint32_t size);
  // will never be called with NULL `p`
  void (*free) (vwadwr_memman *mman, void *p);
  // user data
  void *udata;
};


enum {
  VWADWR_LOG_NOTE,
  VWADWR_LOG_WARNING,
  VWADWR_LOG_ERROR,
  VWADWR_LOG_DEBUG,
};

// logging; can be NULL
// `type` is the above enum
extern void (*vwadwr_logf) (int type, const char *fmt, ...);

// assertion; can be NULL, then the lib will simply traps
extern void (*vwadwr_assertion_failed) (const char *fmt, ...);


// ////////////////////////////////////////////////////////////////////////// //
void vwadwr_z85_encode_key (const vwadwr_public_key inkey, vwadwr_z85_key enkey);
vwadwr_result vwadwr_z85_decode_key (const vwadwr_z85_key enkey, vwadwr_public_key outkey);


// ////////////////////////////////////////////////////////////////////////// //
/* the algo is like this:
    vwadwr_new_dir
    add files with vwadwr_pack_file
    finish archive creation with vwadwr_finish, or free dir with vwadwr_free_dir
*/

#define VWADWR_NEW_DEFAULT    (0u)
#define VWADWR_NEW_DONT_SIGN  (0x4000u)

// error codes
#define VWADWR_NEW_OK           (0)
#define VWADWR_NEW_ERR_AUTHOR   (1)
#define VWADWR_NEW_ERR_TITLE    (2)
#define VWADWR_NEW_ERR_COMMENT  (3)
#define VWADWR_NEW_ERR_FLAGS    (4)
#define VWADWR_NEW_ERR_PRIVKEY  (5)
#define VWADWR_NEW_ERR_OTHER    (-1)


vwadwr_bool vwadwr_is_good_privkey (const vwadwr_secret_key privkey);

// use this to check if archive file name is valid.
// archive file name should not start with slash,
// should not end with slash, should not be empty,
// should not be longer than 255 chars, and
// should not contain double slashes.
// allowed chars: [32..127]
vwadwr_bool vwadwr_is_valid_file_name (const char *str);

// use this to check if archive group name is valid.
// group name not be longer than 255 chars.
// allowed chars: [32..255]
vwadwr_bool vwadwr_is_valid_group_name (const char *str);


// if `mman` is `NULL`, use libc memory manager
// will return `NULL` on any error (including invalid comment)
// `privkey` cannot be NULL, and should be filled with GOOD random data
vwadwr_dir *vwadwr_new_dir (vwadwr_memman *mman, vwadwr_iostream *outstrm,
                            const char *author, /* can be NULL */
                            const char *title, /* can be NULL */
                            const char *comment, /* can be NULL */
                            unsigned flags,
                            /* cannot be NULL; must ALWAYS be filled with GOOD random bytes */
                            const vwadwr_secret_key privkey,
                            vwadwr_public_key respubkey, /* OUT; can be NULL */
                            int *error); /* OUT; can be NULL */
void vwadwr_free_dir (vwadwr_dir **dirp);
vwadwr_bool vwadwr_is_valid_dir (const vwadwr_dir *dir);

// this will free `dir`
vwadwr_result vwadwr_finish (vwadwr_dir **dirp);

// return 0 to stop
typedef vwadwr_bool vwadwr_pack_progress (vwadwr_dir *dir, int read, int written, void *udata);


enum {
  VADWR_COMP_DISABLE = -1,
  /* no literal-only, no lazy matching */
  VADWR_COMP_FASTEST = 0,
  /* with literal-only, no lazy matching */
  VADWR_COMP_FAST = 1,
  /* no literal-only, with lazy matching */
  VADWR_COMP_MEDIUM = 2,
  /* with literal-only, with lazy matching */
  VADWR_COMP_BEST = 3,
};

// comment may contain only ASCII chars, spaces, tabs, and '\x0a' for newlines.
// group name may contain chars in range [32..255]. ASCII is case-insensitive.
// file name may contain only ASCII chars.
vwadwr_result vwadwr_pack_file (vwadwr_dir *dir, vwadwr_iostream *instrm,
                                int level, /* VADWR_COMP_xxx */
                                const char *pkfname,
                                const char *groupname, /* can be NULL */
                                unsigned long long ftime, /* can be 0; seconds since Epoch */
                                int *upksizep, int *pksizep,
                                vwadwr_pack_progress progcb, void *progudata);

#ifndef VWADWR_DISABLE_COPY_FILES
// used to copy file from the existing archive without repacking
// comment may contain only ASCII chars, spaces, tabs, and '\x0a' for newlines.
// group name may contain chars in range [32..255]. ASCII is case-insensitive.
// file name may contain only ASCII chars.
vwadwr_result vwadwr_copy_file (vwadwr_dir *dir,
                                vwad_handle *srcwad, vwad_fidx fidx,
                                const char *pkfname, /* new name; can be NULL to retain */
                                const char *groupname, /* can be NULL to retain */
                                unsigned long long ftime, /* can be 0 to retain */
                                int *upksizep, int *pksizep,
                                vwadwr_pack_progress progcb, void *progudata);
#endif


// ////////////////////////////////////////////////////////////////////////// //
// the following API is using the libc memory manager

vwadwr_iostream *vwadwr_new_file_stream (FILE *fl);
// this will close the file
vwadwr_result vwadwr_close_file_stream (vwadwr_iostream *strm);
// this will NOT close the file
void vwadwr_free_file_stream (vwadwr_iostream *strm);


// ////////////////////////////////////////////////////////////////////////// //
unsigned vwadwr_crc32_init (void);
unsigned vwadwr_crc32_part (unsigned crc32, const void *src, size_t len);
unsigned vwadwr_crc32_final (unsigned crc32);


// ////////////////////////////////////////////////////////////////////////// //
// case-insensitive wildcard matching
// returns:
//   -1: malformed pattern
//    0: equal
//    1: not equal
vwadwr_result vwadwr_wildmatch (const char *pat, size_t plen, const char *str, size_t slen);
vwadwr_result vwadwr_wildmatch_path (const char *pat, size_t plen, const char *str, size_t slen);


#if defined(__cplusplus)
}
#endif
#endif
