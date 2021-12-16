//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
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
#ifndef VAVOOM_CORE_LIB_COMMON
#define VAVOOM_CORE_LIB_COMMON

#if !defined(_WIN32) && !defined(__CYGWIN__)
# define __declspec(whatever)
#endif

//k8: sorry
#ifndef VVA_OKUNUSED
# define VVA_OKUNUSED     __attribute__((unused))
#endif
#ifndef VVA_CHECKRESULT
# define VVA_CHECKRESULT  __attribute__((warn_unused_result))
#endif
#ifndef VVA_PURE
# define VVA_PURE         __attribute__((pure))
#endif
#ifndef VVA_CONST
# define VVA_CONST        __attribute__((const))
#endif
#ifndef VVA_MAYALIAS
# define VVA_MAYALIAS     __attribute__((__may_alias__))
#endif
#ifndef VVA_ALWAYS_INLINE
# define VVA_ALWAYS_INLINE  inline __attribute__((always_inline))
#endif
#ifndef VVA_NO_INLINE
# define VVA_NO_INLINE      __attribute__((noinline))
#endif


#ifdef __cplusplus
# define VVA_NOEXCEPT   noexcept
# define VVA_CONSTEXPR  constexpr
#else
# define VVA_CONSTEXPR
# define VVA_NOEXCEPT
#endif


//==========================================================================
//
//  Standard macros
//
//==========================================================================

// number of elements in an array
#define ARRAY_COUNT(array)  (sizeof(array)/sizeof((array)[0]))


//==========================================================================
//
//  Basic types
//
//==========================================================================

#define MIN_VINT8   ((vint8)-128)
#define MIN_VINT16  ((vint16)-32768)
#define MIN_VINT32  ((vint32)-2147483648)

#define MAX_VINT8   ((vint8)0x7f)
#define MAX_VINT16  ((vint16)0x7fff)
#define MAX_VINT32  ((vint32)0x7fffffff)

#define MAX_VUINT8  ((vuint8)0xff)
#define MAX_VUINT16 ((vuint16)0xffff)
#define MAX_VUINT32 ((vuint32)0xffffffff)

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/*#include <inttypes.h>*/
typedef int8_t    __attribute__((__may_alias__)) vint8;
typedef uint8_t   __attribute__((__may_alias__)) vuint8;
typedef int16_t   __attribute__((__may_alias__)) vint16;
typedef uint16_t  __attribute__((__may_alias__)) vuint16;
typedef int32_t   __attribute__((__may_alias__)) vint32;
typedef uint32_t  __attribute__((__may_alias__)) vuint32;
typedef int64_t   __attribute__((__may_alias__)) vint64;
typedef uint64_t  __attribute__((__may_alias__)) vuint64;
typedef float     __attribute__((__may_alias__)) vfloat;
typedef double    __attribute__((__may_alias__)) vdouble;


#ifdef __cplusplus
//static_assert(sizeof(ubyte) == 1, "invalid ubyte");
static_assert(sizeof(vint8) == 1, "invalid vint8");
static_assert(sizeof(vuint8) == 1, "invalid vuint8");
static_assert(sizeof(vint16) == 2, "invalid vint16");
static_assert(sizeof(vuint16) == 2, "invalid vuint16");
static_assert(sizeof(vint32) == 4, "invalid vint32");
static_assert(sizeof(vuint32) == 4, "invalid vuint32");
static_assert(sizeof(vint64) == 8, "invalid vint64");
static_assert(sizeof(vuint64) == 8, "invalid vuint64");
static_assert(sizeof(vfloat) == 4, "invalid vfloat");
static_assert(sizeof(vdouble) == 8, "invalid vdouble");

#include <string.h>


// ctor argument to avoid initialisation
// must be explicitly defined in the corresponding class/struct
enum ENoInit { E_NoInit };

// "clearing placement new"
// use it like this:
//   T myobj;
//   new(&myobj, E_ArrayNew, E_ArrayNew) T();
enum EArrayNew { E_ArrayNew };
VVA_ALWAYS_INLINE VVA_OKUNUSED void *operator new (size_t sz, void *ptr, EArrayNew, EArrayNew) noexcept { if (sz) memset(ptr, 0, sz); return ptr; }

// "non-clearing placement new"
// use it like this:
//   T myobj;
//   new(&myobj, E_ArrayNew, E_NoInit) T();
VVA_ALWAYS_INLINE VVA_OKUNUSED void *operator new (size_t sz, void *ptr, EArrayNew, ENoInit) noexcept { return ptr; }
#endif


static inline VVA_CHECKRESULT VVA_CONST uint16_t foldHash32to16 (const uint32_t h) VVA_NOEXCEPT { return (uint16_t)(h+(h>>16)); }
static inline VVA_CHECKRESULT VVA_CONST uint8_t foldHash32to8 (uint32_t h) VVA_NOEXCEPT { h = foldHash32to16(h); return (uint8_t)(h+(h>>8)); }


#ifdef __cplusplus
//==========================================================================
//
//  Standard C++ macros
//
//==========================================================================

// disable copying for a class/struct
#define VV_DISABLE_COPY(cname_) \
  cname_ (const cname_ &) = delete; \
  cname_ &operator = (const cname_ &) = delete;


//==========================================================================
//
//  Basic templates
//
//==========================================================================
/* defined in "mathutil.h", as `min2`, `max2`, and `clampval`
template<class T> T Min(T val1, T val2) { return (val1 < val2 ? val1 : val20; }
template<class T> T Max(T val1, T val2) { return (val1 > val2 ? val1 : val2); }
template<class T> T Clamp(T val, T low, T high) { return (val < low ? low : val > high ? high : val); }
*/


// FUCK YOU, SHITPLUSPLUS! WHY CAN'T WE FUCKIN' DECLARE THIS FUCKIN' TEMPLATE,
// AND THEN USE IT TO DETECT THINGS THAT WERE DECLARED LATER?!!

//==========================================================================
//
//  use SFINAE to detect hash function
//
//==========================================================================
/*
template<class T, class=void> struct VEnableIfHasHashMethod {};
template<class T> struct VEnableIfHasHashMethod<T, decltype(T::CalculateHash, void())> { typedef void type; };

template<class T, class=void> struct VUniversalHasher {
  template<typename VTT> static VVA_ALWAYS_INLINE VVA_CHECKRESULT uint32_t CalcHash (const VTT &akey) noexcept { return GetTypeHash(akey); }
};
template<class T> struct VUniversalHasher<T, typename VEnableIfHasHashMethod<T>::type> {
  template<typename VTT> static VVA_ALWAYS_INLINE VVA_CHECKRESULT uint32_t CalcHash (const VTT &akey) noexcept { return akey.GetTypeHash(); }
};

template<typename VT> static VVA_OKUNUSED VVA_CHECKRESULT uint32_t CalcTypeHash (const VT &v) noexcept {
  return VUniversalHasher<VT>::CalcHash(v);
}
*/
#endif


#endif
