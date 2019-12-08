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
//**  Copyright (C) 2018-2019 Ketmar Dark
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

static VVA_OKUNUSED inline vint16 LittleShort (vint16 x) noexcept { return x; }
static VVA_OKUNUSED inline vint32 LittleLong (vint32 x) noexcept { return x; }
static VVA_OKUNUSED inline float LittleFloat (float x) noexcept { return x; }

static VVA_OKUNUSED inline vint16 BigShort (vint16 x) noexcept { return ((vuint16)x>>8)|((vuint16)x<<8); }
static VVA_OKUNUSED inline vint32 BigLong (vint32 x) noexcept { return ((vuint32)x>>24)|(((vuint32)x>>8)&0xff00U)|(((vuint32)x<<8)&0xff0000U)|((vuint32)x<<24); }
static VVA_OKUNUSED inline float BigFloat (float x) noexcept { union { float f; vint32 l; } a; a.f = x; a.l = BigLong(a.l); return a.f; }

static VVA_OKUNUSED inline vuint16 LittleUShort (vuint16 x) noexcept { return x; }
static VVA_OKUNUSED inline vuint32 LittleULong (vuint32 x) noexcept { return x; }

static VVA_OKUNUSED inline vuint16 BigUShort (vuint16 x) noexcept { return ((vuint16)x>>8)|((vuint16)x<<8); }
static VVA_OKUNUSED inline vuint32 BigULong (vuint32 x) noexcept { return ((vuint32)x>>24)|(((vuint32)x>>8)&0xff00U)|(((vuint32)x<<8)&0xff0000U)|((vuint32)x<<24); }

#else

static VVA_OKUNUSED inline vint16 BigShort (vint16 x) noexcept { return x; }
static VVA_OKUNUSED inline vint32 BigLong (vint32 x) noexcept { return x; }
static VVA_OKUNUSED inline float BigFloat (float x) noexcept { return x; }

static VVA_OKUNUSED inline vint16 LittleShort (vint16 x) noexcept { return ((vuint16)x>>8)|((vuint16)x<<8); }
static VVA_OKUNUSED inline vint32 LittleLong (vint32 x) noexcept { return ((vuint32)x>>24)|(((vuint32)x>>8)&0xff00U)|(((vuint32)x<<8)&0xff0000U)|((vuint32)x<<24); }
static VVA_OKUNUSED inline float LittleFloat (float x) noexcept { union { float f; vint32 l; } a; a.f = x; a.l = LittleLong(a.l); return a.f; }

static VVA_OKUNUSED inline vuint16 BigUShort (vuint16 x) noexcept { return x; }
static VVA_OKUNUSED inline vuint32 BigULong (vuint32 x) noexcept { return x; }

static VVA_OKUNUSED inline vuint16 LittleShort (vuint16 x) noexcept { return ((vuint16)x>>8)|((vuint16)x<<8); }
static VVA_OKUNUSED inline vuint32 LittleLong (vuint32 x) noexcept { return ((vuint32)x>>24)|(((vuint32)x>>8)&0xff00U)|(((vuint32)x<<8)&0xff0000U)|((vuint32)x<<24); }

#endif

#endif // VAVOOM_HIDE_ENDIANNES_CONVERSIONS
