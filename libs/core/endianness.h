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
#ifndef VAVOOM_CORE_LIB_ENDIANNESS
#define VAVOOM_CORE_LIB_ENDIANNESS

#include "common.h"

#ifdef __unix__
#include <sys/param.h>
#endif

#if defined(BSD)
#include <sys/types.h>
#define __BYTE_ORDER _BYTE_ORDER
#define __BIG_ENDIAN _BIG_ENDIAN
#define __LITTLE_ENDIAN _LITTLE_ENDIAN
#endif

#if defined(__SWITCH__)
// we know this for sure
# define VAVOOM_LITTLE_ENDIAN
enum { GBigEndian = 0 };
#elif defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN || \
    defined(__BIG_ENDIAN__) || \
    defined(__ARMEB__) || \
    defined(__THUMBEB__) || \
    defined(__AARCH64EB__) || \
    defined(_MIBSEB) || defined(__MIBSEB) || defined(__MIBSEB__)
# define VAVOOM_BIG_ENDIAN
enum { GBigEndian = 1 };
//static_assert(false, "sorry, big-endian architectures aren't supported yet!");
# error "sorry, big-endian architectures aren't supported yet!"
#elif defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN || \
    defined(__LITTLE_ENDIAN__) || \
    defined(__ARMEL__) || \
    defined(__THUMBEL__) || \
    defined(__AARCH64EL__) || \
    defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__) || \
    defined(_WIN32)
# define VAVOOM_LITTLE_ENDIAN
enum { GBigEndian = 0 };
#else
# error "I don't know what architecture this is!"
#endif

// endianess handling

#ifdef VAVOOM_HIDE_ENDIANNES_CONVERSIONS
// fuck you, c standard!
// hide this from shitptimiser
vint16 LittleShort (vint16 x);
vint32 LittleLong (vint32 x);
float LittleFloat (float x);

vint16 BigShort (vint16 x);
vint32 BigLong (vint32 x);
float BigFloat (float x);

#else

#ifdef VAVOOM_LITTLE_ENDIAN
// little-endian
static VVA_OKUNUSED VVA_ALWAYS_INLINE vint16 LittleShort (const vint16 x) VVA_NOEXCEPT { return x; }
static VVA_OKUNUSED VVA_ALWAYS_INLINE vuint16 LittleUShort (const vuint16 x) VVA_NOEXCEPT { return x; }
static VVA_OKUNUSED VVA_ALWAYS_INLINE vint32 LittleLong (const vint32 x) VVA_NOEXCEPT { return x; }
static VVA_OKUNUSED VVA_ALWAYS_INLINE vuint32 LittleULong (const vuint32 x) VVA_NOEXCEPT { return x; }
static VVA_OKUNUSED VVA_ALWAYS_INLINE float LittleFloat (const float x) VVA_NOEXCEPT { return x; }

#if 1
static VVA_OKUNUSED VVA_ALWAYS_INLINE vint16 BigShort (const vint16 x) VVA_NOEXCEPT { return (vint16)__builtin_bswap16((vuint16)x); }
static VVA_OKUNUSED VVA_ALWAYS_INLINE vuint16 BigUShort (const vuint16 x) VVA_NOEXCEPT { return __builtin_bswap16(x); }
static VVA_OKUNUSED VVA_ALWAYS_INLINE vint32 BigLong (const vint32 x) VVA_NOEXCEPT { return (vint32)__builtin_bswap32((vuint32)x); }
static VVA_OKUNUSED VVA_ALWAYS_INLINE vuint32 BigULong (const vuint32 x) VVA_NOEXCEPT { return __builtin_bswap32(x); }
#else
static VVA_OKUNUSED VVA_ALWAYS_INLINE vint16 BigShort (const vint16 x) VVA_NOEXCEPT { return ((vuint16)x>>8)|((vuint16)x<<8); }
static VVA_OKUNUSED VVA_ALWAYS_INLINE vuint16 BigUShort (const vuint16 x) VVA_NOEXCEPT { return ((vuint16)x>>8)|((vuint16)x<<8); }
static VVA_OKUNUSED VVA_ALWAYS_INLINE vint32 BigLong (const vint32 x) VVA_NOEXCEPT { return ((vuint32)x>>24)|(((vuint32)x>>8)&0xff00U)|(((vuint32)x<<8)&0xff0000U)|((vuint32)x<<24); }
static VVA_OKUNUSED VVA_ALWAYS_INLINE vuint32 BigULong (const vuint32 x) VVA_NOEXCEPT { return ((vuint32)x>>24)|(((vuint32)x>>8)&0xff00U)|(((vuint32)x<<8)&0xff0000U)|((vuint32)x<<24); }
#endif
static VVA_OKUNUSED VVA_ALWAYS_INLINE float BigFloat (const float x) VVA_NOEXCEPT { union { float f; vuint32 l; } a; a.f = x; a.l = BigULong(a.l); return a.f; }

#else
// big-endian
#if 1
static VVA_OKUNUSED VVA_ALWAYS_INLINE vint16 LittleShort (const vint16 x) VVA_NOEXCEPT { return (vint16)__builtin_bswap16((vuint16)x); }
static VVA_OKUNUSED VVA_ALWAYS_INLINE vuint16 LittleUShort (const vuint16 x) VVA_NOEXCEPT { return __builtin_bswap16(x); }
static VVA_OKUNUSED VVA_ALWAYS_INLINE vint32 LittleLong (const vint32 x) VVA_NOEXCEPT { return (vint32)__builtin_bswap32((vuint32)x); }
static VVA_OKUNUSED VVA_ALWAYS_INLINE vuint32 LittleULong (const vuint32 x) VVA_NOEXCEPT { return __builtin_bswap32(x); }
#else
static VVA_OKUNUSED VVA_ALWAYS_INLINE vint16 LittleShort (const vint16 x) VVA_NOEXCEPT { return ((vuint16)x>>8)|((vuint16)x<<8); }
static VVA_OKUNUSED VVA_ALWAYS_INLINE vuint16 LittleUShort (const vuint16 x) VVA_NOEXCEPT { return ((vuint16)x>>8)|((vuint16)x<<8); }
static VVA_OKUNUSED VVA_ALWAYS_INLINE vint32 LittleLong (const vint32 x) VVA_NOEXCEPT { return ((vuint32)x>>24)|(((vuint32)x>>8)&0xff00U)|(((vuint32)x<<8)&0xff0000U)|((vuint32)x<<24); }
static VVA_OKUNUSED VVA_ALWAYS_INLINE vuint32 LittleULong (const vuint32 x) VVA_NOEXCEPT { return ((vuint32)x>>24)|(((vuint32)x>>8)&0xff00U)|(((vuint32)x<<8)&0xff0000U)|((vuint32)x<<24); }
#endif
static VVA_OKUNUSED VVA_ALWAYS_INLINE float LittleFloat (const float x) VVA_NOEXCEPT { union { float f; vint32 l; } a; a.f = x; a.l = LittleLong(a.l); return a.f; }

static VVA_OKUNUSED VVA_ALWAYS_INLINE vint16 BigShort (const vint16 x) VVA_NOEXCEPT { return x; }
static VVA_OKUNUSED VVA_ALWAYS_INLINE vuint16 BigUShort (const vuint16 x) VVA_NOEXCEPT { return x; }
static VVA_OKUNUSED VVA_ALWAYS_INLINE vint32 BigLong (const vint32 x) VVA_NOEXCEPT { return x; }
static VVA_OKUNUSED VVA_ALWAYS_INLINE vuint32 BigULong (const vuint32 x) VVA_NOEXCEPT { return x; }
static VVA_OKUNUSED VVA_ALWAYS_INLINE float BigFloat (const float x) VVA_NOEXCEPT { return x; }

#endif

#endif // VAVOOM_HIDE_ENDIANNES_CONVERSIONS


#endif
