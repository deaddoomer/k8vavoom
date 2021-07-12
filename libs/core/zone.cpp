//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
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
//**
//**  Memory Allocation.
//**
//**************************************************************************
//#include "mimalloc/mimalloc.h"
#include "core.h"

#ifdef __cplusplus
extern "C" {
#endif


#ifdef VAVOOM_CORE_COUNT_ALLOCS
int zone_malloc_call_count = 0;
int zone_realloc_call_count = 0;
int zone_free_call_count = 0;
#endif


#ifdef VAVOOM_USE_MIMALLOC
static volatile uint32_t zShuttingDown = 0;
//   0: not initialized
// 666: initializing
//   1: allowed
//  69: disabled
static volatile uint32_t zMimallocAllowed = 0u;
#endif


#ifdef VAVOOM_USE_MIMALLOC
static __attribute__((unused)) bool xstrEquCI (const char *s0, const char *s1) noexcept {
  if (!s0) return !s1;
  if (!s1) return false;
  while (*s0 && ((uint8_t)*s0) <= 32) ++s0;
  while (*s1 && ((uint8_t)*s1) <= 32) ++s1;
  while (*s0 && *s1) {
    uint8_t c0 = (uint8_t)*s0++;
    if (c0 >= 'A' && c0 <= 'Z') c0 = c0-'A'+'a';
    uint8_t c1 = (uint8_t)*s1++;
    if (c1 >= 'A' && c1 <= 'Z') c1 = c1-'A'+'a';
    if (c0 != c1) return false;
  }
  while (*s0 && ((uint8_t)*s0) <= 32) ++s0;
  while (*s1 && ((uint8_t)*s1) <= 32) ++s1;
  return (!*s0 && !*s1);
}


static inline bool zIsMimallocAllowed () noexcept {
  uint32_t v = 0u; // value to compare
  if (__atomic_compare_exchange_n(&zMimallocAllowed, &v, 666u, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
    // `zMimallocAllowed` was zero (now 666u), need to init
    v = 1u;
    const char *ee = getenv("VAVOOM_MIMALLOC");
    if (ee) {
      if (xstrEquCI(ee, "0") ||
          xstrEquCI(ee, "ona") ||
          xstrEquCI(ee, "off") ||
          xstrEquCI(ee, "no") ||
          xstrEquCI(ee, "false"))
      {
        fprintf(stderr, "ZONE WARNING: MIMALLOC DISABLED!\n");
        v = 69u;
      }
    }
    __atomic_store_n(&zMimallocAllowed, v, __ATOMIC_SEQ_CST);
  } else {
    // `zMimallocAllowed` is non-zero (now 666u), and copied to `v`
    if (v == 666u) {
      // currently initializing, wait
      while (__atomic_load_n(&zMimallocAllowed, __ATOMIC_SEQ_CST) == 666u) {}
      v = __atomic_load_n(&zMimallocAllowed, __ATOMIC_SEQ_CST);
    }
  }
  return (v == 1u);
}
#endif


static inline void ZManDeactivate () noexcept {
#ifdef VAVOOM_USE_MIMALLOC
  __atomic_store_n(&zShuttingDown, 1u, __ATOMIC_SEQ_CST);
#endif
}


static inline bool isZManActive () noexcept {
#ifdef VAVOOM_USE_MIMALLOC
  return (__atomic_load_n(&zShuttingDown, __ATOMIC_SEQ_CST) == 0u);
#else
  return true;
#endif
}


#ifdef VAVOOM_USE_MIMALLOC
# define malloc_fn(sz_)        (zIsMimallocAllowed() ? ::mi_malloc(sz_) : ::malloc(sz_))
# define realloc_fn(ptr_,sz_)  (zIsMimallocAllowed() ? ::mi_realloc(ptr_,sz_) : ::realloc(ptr_,sz_))
# define free_fn(ptr_)         (zIsMimallocAllowed() ? ::mi_free(ptr_) : ::free(ptr_))
#else
# define malloc_fn(sz_)        ::malloc(sz_)
# define realloc_fn(ptr_,sz_)  ::realloc(ptr_,sz_)
# define free_fn(ptr_)         ::free(ptr_)
#endif


const char *Z_GetAllocatorType () noexcept {
#ifdef VAVOOM_USE_MIMALLOC
  return (zIsMimallocAllowed() ? "mi-malloc" : "standard");
#else
  return "standard";
#endif
}


__attribute__((malloc)) __attribute__((alloc_size(1))) __attribute__((returns_nonnull))
void *Z_Malloc (size_t size) noexcept {
#ifdef VAVOOM_CORE_COUNT_ALLOCS
  ++zone_malloc_call_count;
#endif
  size += !size;
  void *res = malloc_fn(size);
  if (!res) Sys_Error("out of memory for %u bytes!", (unsigned int)size);
  memset(res, 0, size); // just in case
  return res;
}


__attribute__((malloc)) __attribute__((alloc_size(1))) __attribute__((returns_nonnull))
void *Z_MallocNoClear (size_t size) noexcept {
#ifdef VAVOOM_CORE_COUNT_ALLOCS
  ++zone_malloc_call_count;
#endif
  size += !size;
  void *res = malloc_fn(size);
  if (!res) Sys_Error("out of memory for %u bytes!", (unsigned int)size);
  return res;
}


__attribute__((malloc)) __attribute__((alloc_size(1))) __attribute__((returns_nonnull))
void *Z_MallocNoClearNoFail (size_t size) noexcept {
#ifdef VAVOOM_CORE_COUNT_ALLOCS
  ++zone_malloc_call_count;
#endif
  size += !size;
  return malloc_fn(size);
}


__attribute__((alloc_size(2))) void *Z_Realloc (void *ptr, size_t size) noexcept {
#ifdef VAVOOM_CORE_COUNT_ALLOCS
  ++zone_realloc_call_count;
#endif
#ifdef VAVOOM_USE_MIMALLOC
  if (size == 0 && !isZManActive()) return nullptr; // don't bother freeing it
#endif
  if (size) {
    void *res = realloc_fn(ptr, size);
    if (!res) Sys_Error("out of memory for %u bytes!", (unsigned int)size);
    return res;
  } else {
    if (ptr) free_fn(ptr);
    return nullptr;
  }
}


__attribute__((alloc_size(2))) void *Z_ReallocNoFail (void *ptr, size_t size) noexcept {
#ifdef VAVOOM_CORE_COUNT_ALLOCS
  ++zone_realloc_call_count;
#endif
#ifdef VAVOOM_USE_MIMALLOC
  if (size == 0 && !isZManActive()) return nullptr; // don't bother freeing it
#endif
  if (size) {
    return realloc_fn(ptr, size);
  } else {
    if (ptr) free_fn(ptr);
    return nullptr;
  }
}


__attribute__((malloc)) __attribute__((alloc_size(1))) __attribute__((returns_nonnull))
void *Z_Calloc (size_t size) noexcept {
  size += !size;
#if !defined(VAVOOM_USE_MIMALLOC)
  void *res = ::calloc(1, size);
#else
  void *res = (zIsMimallocAllowed() ? ::mi_zalloc(size) : ::calloc(1, size));
#endif
  if (!res) Sys_Error("out of memory for %u bytes!", (unsigned int)size);
/*
#if !defined(VAVOOM_USE_MIMALLOC)
  memset(res, 0, size); // just in case
#endif
*/
  return res;
}


void Z_Free (void *ptr) noexcept {
  if (!isZManActive()) return; // don't bother
#ifdef VAVOOM_CORE_COUNT_ALLOCS
  ++zone_free_call_count;
#endif
  //fprintf(stderr, "Z_FREE! (%p)\n", ptr);
  if (ptr) free_fn(ptr);
}


// call this when exiting a thread function, to reclaim thread heaps
void Z_ThreadDone () noexcept {
#if defined(VAVOOM_USE_MIMALLOC)
  if (isZManActive()) ::mi_thread_done();
#endif
}


void Z_ShuttingDown () noexcept {
  ZManDeactivate();
}


__attribute__((noreturn)) void Z_Exit (int exitcode) noexcept {
  Z_ShuttingDown();
  exit(exitcode);
}


#ifdef __cplusplus
}
#endif


void *operator new (size_t size) noexcept(false) {
  //fprintf(stderr, "NEW: %u\n", (unsigned int)size);
  return Z_Calloc(size);
}

void *operator new[] (size_t size) noexcept(false) {
  //fprintf(stderr, "NEW[]: %u\n", (unsigned int)size);
  return Z_Calloc(size);
}
/*
void *operator new (size_t size) noexcept {
  //fprintf(stderr, "NEW: %u\n", (unsigned int)size);
  return Z_Calloc(size);
}

void *operator new[] (size_t size) noexcept {
  //fprintf(stderr, "NEW[]: %u\n", (unsigned int)size);
  return Z_Calloc(size);
}
*/

void operator delete (void *p) noexcept {
  if (!isZManActive()) return; // shitdoze hack
  //fprintf(stderr, "delete (%p)\n", p);
  Z_Free(p);
}

void operator delete[] (void *p) noexcept {
  if (!isZManActive()) return; // shitdoze hack
  //fprintf(stderr, "delete[] (%p)\n", p);
  Z_Free(p);
}
