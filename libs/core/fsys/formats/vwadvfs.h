/* coded by Ketmar // Invisible Vector (psyc://ketmar.no-ip.org/~Ketmar)
 * Understanding is not required. Only obedience.
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/wtfpl/COPYING for more details.
 */
#ifndef VWADVFS_HEADER
#define VWADVFS_HEADER

#if defined(__cplusplus)
extern "C" {
#endif

// define this to omit ed25519 code.
// the library will be fully functional, but will not
// check digital signatures.
// uncommenting this will save you ~100KB of compiled code.
// WARNING! do not use this define to check if ed25519 is
//          enabled, use `vwad_can_check_auth()` API instead!
// basically, there is no reason to uncomment this thing.
//#define VWAD_VFS_DISABLE_ED25519_CODE


// for self-documentation purposes
typedef int vwad_bool;

// file index; 0 means "no file", otherwise positive
typedef int vwad_fidx;

// file descriptor; negative means "invalid", zero or positive is ok
typedef int vwad_fd;

// for self-documentation purposes
// 0 is success, negative value is error
typedef int vwad_result;

// opaque handle
typedef struct vwad_handle_t vwad_handle;

// i/o stream
typedef struct vwad_iostream_t vwad_iostream;
struct vwad_iostream_t {
  // return non-zero on failure; failure can be delayed
  // will never be called with negative `pos`
  vwad_result (*seek) (vwad_iostream *strm, int pos);
  // read *exactly* bufsize bytes; return 0 on success, negative on failure
  // will never be called with zero or negative `bufsize`
  vwad_result (*read) (vwad_iostream *strm, void *buf, int bufsize);
  // user data
  void *udata;
};

// memory allocation
typedef struct vwad_memman_t vwad_memman;
struct vwad_memman_t {
  // will never be called with zero `size`
  // return NULL on OOM
  void *(*alloc) (vwad_memman *mman, uint32_t size);
  // will never be called with NULL `p`
  void (*free) (vwad_memman *mman, void *p);
  // user data
  void *udata;
};

// Ed25519 public key, 32 bytes
typedef unsigned char vwad_public_key[32];


enum {
  VWAD_LOG_NOTE,
  VWAD_LOG_WARNING,
  VWAD_LOG_ERROR,
  VWAD_LOG_DEBUG,
};

// logging; can be NULL
// `type` is the above enum
extern void (*vwad_logf) (int type, const char *fmt, ...);

// assertion; can be NULL, then the lib will simply traps
extern void (*vwad_assertion_failed) (const char *fmt, ...);


// you can use this function to check if digital signature
// authentication is available. the library can be built
// without ed25519 authentication code, and this function
// allows to check signatures availability in runtime.
vwad_bool vwad_can_check_auth (void);


// flags for `vwad_open_stream()`, to be bitwise ORed
#define VWAD_OPEN_DEFAULT          (0u)
// if you are not interested in archive comment, use this flag.
// it will save some memory.
#define VWAD_OPEN_NO_MAIN_COMMENT  (0x2000u)
// this flag can be used to omit digital signature checks.
// signature checks are fast, so you should use this flag
// only if you have a partially corrupted file which you
// want to read anyway.
// note that ed25519 public key will be read and accessible
// even with disabled checks, but you should not use it to
// determine the authorship, for example.
#define VWAD_OPEN_NO_SIGN_CHECK    (0x4000u)
// WARNING! DO NOT USE THIS!
// this can be used to recover files with partially corrupted data,
// but DON'T pass it "for speed", CRC checks are fast, and you
// should not omit them.
#define VWAD_OPEN_NO_CRC_CHECKS    (0x8000u)

// on success, will copy stream pointer and memman pointer
// (i.e. they should be alive until `vwad_close()`).
// if `mman` is `NULL`, use libc memory manager.
vwad_handle *vwad_open_archive (vwad_iostream *strm, unsigned flags, vwad_memman *mman);
// will free handle memory, and clean handle pointer.
void vwad_close_archive (vwad_handle **wadp);

// can return NULL if there is no comment, or `VWAD_OPEN_NO_MAIN_COMMENT` passed
const char *vwad_get_archive_comment (vwad_handle *wad);

// forget main archive comment and free its memory.
// WARNING! `vwad_get_archive_comment()` result will be undefined!
void vwad_free_archive_comment (vwad_handle *wad);

vwad_bool vwad_has_opened_files (vwad_handle *wad);

// check if opened wad is authenticated. if the library was
// built without ed25519 support, or the wad was opened with
// "omit signature check" flag, there could be a public key,
// but the wad will not be authenticated with that key.
vwad_bool vwad_is_authenticated (vwad_handle *wad);
// check if the wad has a ed25519 public key. it doesn't matter
// if the wad was authenticated or not, the key is always avaliable
// (if the wad contains it, of course).
vwad_bool vwad_has_pubkey (vwad_handle *wad);
// on failure, `pubkey` content is undefined.
vwad_result vwad_get_pubkey (vwad_handle *wad, vwad_public_key pubkey);

// return maximum fidx in this wad.
// avaliable files are [1..res] (i.e. including the returned value).
// on error, returns 0.
vwad_fidx vwad_get_max_fidx (vwad_handle *wad);
// first index is 1, last is `vwad_get_file_count() - 1`.
// i.e. index 0 is unused (it usually means "no file").
const char *vwad_get_file_name (vwad_handle *wad, vwad_fidx fidx);
// returns negative number on error.
int vwad_get_file_size (vwad_handle *wad, vwad_fidx fidx);

// find file by name. internally, the library is using a hash
// table, so this is quite fast.
// searching is case-insensitive for ASCII chars.
// use slashes ("/") to separate directories. do not start
// `name` with a slash, use strictly one slash to separate
// path parts. "." and ".." pseudodirs are not supported.
vwad_fidx vwad_find_file (vwad_handle *wad, const char *name);

// returns 0 if there is an error, or the time is not set
// returned time is in UTC, seconds since Epoch (or 0 if unknown)
unsigned long long vwad_get_ftime (vwad_handle *wad, vwad_fidx fidx);

// open file for reading using its index
// (obtained from `vwad_find_file()`, for example).
// return file handle or negative value on error.
// (note that 0 is a valid file handle!)
// note that maximum number of simultaneously opened files is 128.
vwad_fd vwad_fopen (vwad_handle *wad, vwad_fidx fidx);
void vwad_fclose (vwad_handle *wad, vwad_fd fd);
// return 0 if fd is valid, but closed, and negative number on error.
vwad_fidx vwad_fdfidx (vwad_handle *wad, vwad_fd fd);

vwad_result vwad_seek (vwad_handle *wad, vwad_fd fd, int pos);
// return position, or negative number on error.
int vwad_tell (vwad_handle *wad, vwad_fd fd);
// return number of read bytes, or negative on error.
int vwad_read (vwad_handle *wad, vwad_fd fd, void *buf, int len);


#if defined(__cplusplus)
}
#endif
#endif
