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
//
//  Dynamic string class.
//
//**************************************************************************
#include "core.h"
#include "ryu.h"

#define VCORE_USE_RYU_FLOAT_PARSER

/*
#if defined(VCORE_ALLOW_STRTODEX)
# if defined(__x86_64__) || defined(__aarch64__) || defined(_WIN32) || defined(__i386__)
#  if !defined(USE_FPU_MATH)
#   define VCORE_USE_STRTODEX
#  endif
# endif
#endif
*/

/*
#if defined(VCORE_ALLOW_STRTODEX)
# ifndef VCORE_USE_STRTODEX
#  define VCORE_USE_STRTODEX
# endif
#endif
*/
// always
#ifndef VCORE_USE_STRTODEX
# define VCORE_USE_STRTODEX
#endif

#ifdef VCORE_USE_STRTODEX
# ifdef VCORE_USE_RYU_FLOAT_PARSER
/* nothing here */
# else
#  include "strtod_plan9.h"
# endif
#else
# define VCORE_USE_LOCALE
#endif

//#define VCORE_STUPID_ATOF

#ifdef VCORE_USE_LOCALE
# include "locale.h"
#endif


#define VSTR_DOUBLE_GROW_LIMIT  1024
#define VSTR_THIRD_GROW_LIMIT   0x100000 /*1MB*/


//==========================================================================
//
//  vstrCalcGrowStoreSizeSizeT
//
//==========================================================================
static size_t vstrCalcGrowStoreSizeSizeT (size_t newlen) noexcept {
  size_t newsz = (size_t)newlen;
  if (newsz < 64u) return (newsz|0x3fu)+1u+63u;
  if (newsz < VSTR_DOUBLE_GROW_LIMIT) return (newsz+(newsz>>1))|0x3fu;
  if (newsz < VSTR_THIRD_GROW_LIMIT) return (newsz+newsz/3u)|0x3fu;
  if (newsz < 0x40000000u) return newsz+32768u;
  Sys_Error("out of memory for VStr storage");
  __builtin_trap();
}


//==========================================================================
//
//  vstrCalcInitialStoreSizeSizeT
//
//  always overallocate a little
//
//==========================================================================
static size_t vstrCalcInitialStoreSizeSizeT (size_t newlen) noexcept {
  if (newlen >= 0x40000000u) Sys_Error("out of memory for VStr storage");
  size_t newsz = (size_t)newlen;
  return (newsz < 64u ? (newsz|0x0fu) : (newsz|0x3fu));
}


//==========================================================================
//
//  vstrCalcInitialStoreSizeInt
//
//==========================================================================
static VVA_FORCEINLINE int vstrCalcInitialStoreSizeInt (int newlen) noexcept {
  return (int)vstrCalcInitialStoreSizeSizeT((size_t)newlen);
}


#ifdef VCORE_USE_STRTODEX
//==========================================================================
//
//  strtofEx
//
//==========================================================================
static bool strtofEx (float *resptr, const char *s) noexcept {
  if (!s || !s[0]) return false;
  #ifdef VCORE_USE_RYU_FLOAT_PARSER
  float tmpf = 0.0f;
  if (!resptr) resptr = &tmpf;
  const size_t slen = strlen(s);
  if (slen > 0x3fffffffu) return false;
  const int res = ryu_s2f(s, (int)slen, resptr, RYU_FLAG_ALLOW_ALL);
  if (res != RYU_SUCCESS) *resptr = 0.0f; // just in case
  return (res == RYU_SUCCESS);
  #else
  char *end;
  float res = fmtstrtof(s, &end, nullptr);
  if (!isFiniteF(res) || *end) return false;
  // remove sign from zero
  if (res == 0) memset(&res, 0, sizeof(res));
  if (resptr) *resptr = res;
  return true;
  #endif
}
#endif


// ////////////////////////////////////////////////////////////////////////// //
const VStr VStr::EmptyString = VStr();


// ////////////////////////////////////////////////////////////////////////// //
const vuint16 VStr::cp1251[128] = {
  0x0402,0x0403,0x201A,0x0453,0x201E,0x2026,0x2020,0x2021,0x20AC,0x2030,0x0409,0x2039,0x040A,0x040C,0x040B,0x040F,
  0x0452,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,0x003F,0x2122,0x0459,0x203A,0x045A,0x045C,0x045B,0x045F,
  0x00A0,0x040E,0x045E,0x0408,0x00A4,0x0490,0x00A6,0x00A7,0x0401,0x00A9,0x0404,0x00AB,0x00AC,0x00AD,0x00AE,0x0407,
  0x00B0,0x00B1,0x0406,0x0456,0x0491,0x00B5,0x00B6,0x00B7,0x0451,0x2116,0x0454,0x00BB,0x0458,0x0405,0x0455,0x0457,
  0x0410,0x0411,0x0412,0x0413,0x0414,0x0415,0x0416,0x0417,0x0418,0x0419,0x041A,0x041B,0x041C,0x041D,0x041E,0x041F,
  0x0420,0x0421,0x0422,0x0423,0x0424,0x0425,0x0426,0x0427,0x0428,0x0429,0x042A,0x042B,0x042C,0x042D,0x042E,0x042F,
  0x0430,0x0431,0x0432,0x0433,0x0434,0x0435,0x0436,0x0437,0x0438,0x0439,0x043A,0x043B,0x043C,0x043D,0x043E,0x043F,
  0x0440,0x0441,0x0442,0x0443,0x0444,0x0445,0x0446,0x0447,0x0448,0x0449,0x044A,0x044B,0x044C,0x044D,0x044E,0x044F,
};

const vuint16 VStr::cpKOI[128] = {
  0x2500,0x2502,0x250C,0x2510,0x2514,0x2518,0x251C,0x2524,0x252C,0x2534,0x253C,0x2580,0x2584,0x2588,0x258C,0x2590,
  0x2591,0x2592,0x2593,0x2320,0x25A0,0x2219,0x221A,0x2248,0x2264,0x2265,0x00A0,0x2321,0x00B0,0x00B2,0x00B7,0x00F7,
  0x2550,0x2551,0x2552,0x0451,0x0454,0x2554,0x0456,0x0457,0x2557,0x2558,0x2559,0x255A,0x255B,0x0491,0x255D,0x255E,
  0x255F,0x2560,0x2561,0x0401,0x0404,0x2563,0x0406,0x0407,0x2566,0x2567,0x2568,0x2569,0x256A,0x0490,0x256C,0x00A9,
  0x044E,0x0430,0x0431,0x0446,0x0434,0x0435,0x0444,0x0433,0x0445,0x0438,0x0439,0x043A,0x043B,0x043C,0x043D,0x043E,
  0x043F,0x044F,0x0440,0x0441,0x0442,0x0443,0x0436,0x0432,0x044C,0x044B,0x0437,0x0448,0x044D,0x0449,0x0447,0x044A,
  0x042E,0x0410,0x0411,0x0426,0x0414,0x0415,0x0424,0x0413,0x0425,0x0418,0x0419,0x041A,0x041B,0x041C,0x041D,0x041E,
  0x041F,0x042F,0x0420,0x0421,0x0422,0x0423,0x0416,0x0412,0x042C,0x042B,0x0417,0x0428,0x042D,0x0429,0x0427,0x042A,
};

char VStr::wc2shitmap[65536];
char VStr::wc2koimap[65536];

struct VstrInitr_fuck_you_gnu_binutils_fuck_you_fuck_you_fuck_you {
  VstrInitr_fuck_you_gnu_binutils_fuck_you_fuck_you_fuck_you () noexcept {
    // wc->1251
    memset(VStr::wc2shitmap, '?', sizeof(VStr::wc2shitmap));
    for (int f = 0; f < 128; ++f) VStr::wc2shitmap[f] = (char)f;
    for (int f = 0; f < 128; ++f) VStr::wc2shitmap[VStr::cp1251[f]] = (char)(f+128);
    // wc->koi
    memset(VStr::wc2koimap, '?', sizeof(VStr::wc2koimap));
    for (int f = 0; f < 128; ++f) VStr::wc2koimap[f] = (char)f;
    for (int f = 0; f < 128; ++f) VStr::wc2koimap[VStr::cpKOI[f]] = (char)(f+128);
  }
};

VstrInitr_fuck_you_gnu_binutils_fuck_you_fuck_you_fuck_you vstrInitr_fuck_you_gnu_binutils_fuck_you_fuck_you_fuck_you;


// ////////////////////////////////////////////////////////////////////////// //
// fast state-machine based UTF-8 decoder; using 8 bytes of memory
// code points from invalid range will never be valid, this is the property of the state machine
// see http://bjoern.hoehrmann.de/utf-8/decoder/dfa/
// [0..$16c-1]
// maps bytes to character classes
const uint8_t VUtf8DecoderFast::utf8dfa[0x16c] = {
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 00-0f
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 10-1f
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 20-2f
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 30-3f
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 40-4f
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 50-5f
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 60-6f
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 70-7f
  0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01, // 80-8f
  0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09, // 90-9f
  0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07, // a0-af
  0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07, // b0-bf
  0x08,0x08,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02, // c0-cf
  0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02, // d0-df
  0x0a,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x04,0x03,0x03, // e0-ef
  0x0b,0x06,0x06,0x06,0x05,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08, // f0-ff
  // maps a combination of a state of the automaton and a character class to a state
  0x00,0x0c,0x18,0x24,0x3c,0x60,0x54,0x0c,0x0c,0x0c,0x30,0x48,0x0c,0x0c,0x0c,0x0c, // 100-10f
  0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x00,0x0c,0x0c,0x0c,0x0c,0x0c,0x00, // 110-11f
  0x0c,0x00,0x0c,0x0c,0x0c,0x18,0x0c,0x0c,0x0c,0x0c,0x0c,0x18,0x0c,0x18,0x0c,0x0c, // 120-12f
  0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x18,0x0c,0x0c,0x0c,0x0c,0x0c,0x18,0x0c,0x0c, // 130-13f
  0x0c,0x0c,0x0c,0x0c,0x0c,0x18,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x24, // 140-14f
  0x0c,0x24,0x0c,0x0c,0x0c,0x24,0x0c,0x0c,0x0c,0x0c,0x0c,0x24,0x0c,0x24,0x0c,0x0c, // 150-15f
  0x0c,0x24,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c};


// ////////////////////////////////////////////////////////////////////////// //
int VStr::float2str (char *buf, float v) noexcept { return f2s_buffered(v, buf); }
int VStr::double2str (char *buf, double v) noexcept { return d2s_buffered(v, buf); }


//==========================================================================
//
//  VStr::VStr
//
//==========================================================================
VStr::VStr (int v) noexcept : dataptr(nullptr) {
  char buf[64];
  int len = (int)snprintf(buf, sizeof(buf), "%d", v);
  setContent(buf, len);
}


//==========================================================================
//
//  VStr::VStr
//
//==========================================================================
VStr::VStr (unsigned v) noexcept : dataptr(nullptr) {
  char buf[64];
  int len = (int)snprintf(buf, sizeof(buf), "%u", v);
  setContent(buf, len);
}


//==========================================================================
//
//  VStr::VStr
//
//==========================================================================
VStr::VStr (float v) noexcept : dataptr(nullptr) {
  char buf[FloatBufSize];
  int len = f2s_buffered(v, buf);
  setContent(buf, len);
}


//==========================================================================
//
//  VStr::VStr
//
//==========================================================================
VStr::VStr (double v) noexcept : dataptr(nullptr) {
  char buf[DoubleBufSize];
  int len = d2s_buffered(v, buf);
  setContent(buf, len);
}


//==========================================================================
//
//  VStr::Serialise
//
//  serialisation operator
//
//==========================================================================
VStream &VStr::Serialise (VStream &Strm) {
  vint32 len = length();
  Strm << STRM_INDEX(len);
  vassert(len >= 0);
  if (Strm.IsLoading()) {
    // reading
    if (Strm.IsError() || len < 0 || len > 32*1024*1024) { // arbitrary limit
      clear();
      Strm.SetError();
      return Strm;
    }
    if (len) {
      resize(len);
      makeMutable();
      Strm.Serialise(dataptr, len+1); // eat last byte which should be zero...
      if (Strm.IsError() || dataptr[len] != 0) {
        clear();
        Strm.SetError();
        return Strm;
      }
      //dataptr[len] = 0; // ...and force zero
    } else {
      clear();
      // zero byte is always there
      vuint8 b;
      Strm << b;
      if (Strm.IsError() || b != 0) {
        clear();
        Strm.SetError();
        return Strm;
      }
    }
  } else {
    // writing
    if (len) Strm.Serialise(getData(), len);
    // always write terminating zero
    vuint8 b = 0;
    Strm << b;
  }
  return Strm;
}


//==========================================================================
//
//  VStr::Serialise
//
//  serialisation operator
//
//==========================================================================
VStream &VStr::Serialise (VStream &Strm) const {
  vassert(!Strm.IsLoading());
  vint32 len = length();
  Strm << STRM_INDEX(len);
  if (len) Strm.Serialise(getData(), len);
  // always write terminating zero
  vuint8 b = 0;
  Strm << b;
  return Strm;
}


// ////////////////////////////////////////////////////////////////////////// //
VStr &VStr::operator += (float v) noexcept { char buf[FloatBufSize]; (void)f2s_buffered(v, buf); return operator+=(buf); }
VStr &VStr::operator += (double v) noexcept { char buf[DoubleBufSize]; (void)d2s_buffered(v, buf); return operator+=(buf); }


//==========================================================================
//
//  VStr::utf8Append
//
//==========================================================================
VStr &VStr::utf8Append (vuint32 code) noexcept {
  if (code > 0x10FFFF || !isValidCodepoint((int)code)) return operator+=('?');
  if (code <= 0x7f) return operator+=((char)(code&0xff));
  if (code <= 0x7FF) {
    operator+=((char)(0xC0|(code>>6)));
    return operator+=((char)(0x80|(code&0x3F)));
  }
  if (code <= 0xFFFF) {
    operator+=((char)(0xE0|(code>>12)));
    operator+=((char)(0x80|((code>>6)&0x3f)));
    return operator+=((char)(0x80|(code&0x3F)));
  }
  if (code <= 0x10FFFF) {
    operator+=((char)(0xF0|(code>>18)));
    operator+=((char)(0x80|((code>>12)&0x3f)));
    operator+=((char)(0x80|((code>>6)&0x3f)));
    return operator+=((char)(0x80|(code&0x3F)));
  }
  return operator+=('?');
}


//==========================================================================
//
//  VStr::isUtf8Valid
//
//==========================================================================
VVA_CHECKRESULT bool VStr::isUtf8Valid () const noexcept {
  int slen = length();
  if (slen < 1) return true;
  int pos = 0;
  const char *data = getData();
  while (pos < slen) {
    int uclen = CalcUtf8CharByteSize(data[pos++]);
    if (!uclen) return true;
    if (uclen < 1) return false; // invalid sequence start
    if (pos+uclen-1 > slen) return false; // out of chars in string
    // check other sequence bytes
    while (--uclen) if (!IsValidUtf8Continuation(data[pos++])) return false;
  }
  return true;
}


//==========================================================================
//
//  VStr::isUtf8Valid
//
//==========================================================================
VVA_CHECKRESULT bool VStr::isUtf8Valid (const char *s) noexcept {
  if (!s) return true;
  while (*s) {
    int uclen = CalcUtf8CharByteSize(*s++);
    if (!uclen) return true;
    if (uclen < 1) return false; // invalid sequence start
    // check other sequence bytes
    while (--uclen) if (!IsValidUtf8Continuation(*s++)) return false;
  }
  return true;
}


//==========================================================================
//
//  VStr::toLowerCase1251
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::toLowerCase1251 () const noexcept {
  const int slen = length();
  if (slen < 1) return VStr();
  const char *data = getData();
  for (int f = 0; f < slen; ++f) {
    if (locase1251(data[f]) != data[f]) {
      VStr res(*this);
      res.makeMutable();
      for (int c = 0; c < slen; ++c) res.dataptr[c] = locase1251(res.dataptr[c]);
      return res;
    }
  }
  return VStr(*this);
}


//==========================================================================
//
//  VStr::toUpperCase1251
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::toUpperCase1251 () const noexcept {
  const int slen = length();
  if (slen < 1) return VStr();
  const char *data = getData();
  for (int f = 0; f < slen; ++f) {
    if (upcase1251(data[f]) != data[f]) {
      VStr res(*this);
      res.makeMutable();
      for (int c = 0; c < slen; ++c) res.dataptr[c] = upcase1251(res.dataptr[c]);
      return res;
    }
  }
  return VStr(*this);
}


//==========================================================================
//
//  VStr::toLowerCaseKOI
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::toLowerCaseKOI () const noexcept {
  const int slen = length();
  if (slen < 1) return VStr();
  const char *data = getData();
  for (int f = 0; f < slen; ++f) {
    if (locaseKOI(data[f]) != data[f]) {
      VStr res(*this);
      res.makeMutable();
      for (int c = 0; c < slen; ++c) res.dataptr[c] = locaseKOI(res.dataptr[c]);
      return res;
    }
  }
  return VStr(*this);
}


//==========================================================================
//
//  VStr::toUpperCaseKOI
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::toUpperCaseKOI () const noexcept {
  const int slen = length();
  if (slen < 1) return VStr();
  const char *data = getData();
  for (int f = 0; f < slen; ++f) {
    if (upcaseKOI(data[f]) != data[f]) {
      VStr res(*this);
      res.makeMutable();
      for (int c = 0; c < slen; ++c) res.dataptr[c] = upcaseKOI(res.dataptr[c]);
      return res;
    }
  }
  return VStr(*this);
}


//==========================================================================
//
//  VStr::ICmp
//
//==========================================================================
VVA_CHECKRESULT int VStr::ICmp (const char *s0, const char *s1) noexcept {
  if (!s0) s0 = "";
  if (!s1) s1 = "";
  for (;;) {
    const int c0 = locase1251(*s0++)&0xff;
    const int c1 = locase1251(*s1++)&0xff;
    const int diff = c0-c1;
    if (diff) return (diff < 0 ? -1 : 1);
    // `c0` and `c1` are equal
    if (!c0) return 0;
  }
  // it never came here
  __builtin_trap();
}


//==========================================================================
//
//  VStr::NICmp
//
//==========================================================================
VVA_CHECKRESULT int VStr::NICmp (const char *s0, const char *s1, size_t max) noexcept {
  if (max == 0) return 0;
  if (!s0) s0 = "";
  if (!s1) s1 = "";
  while (max--) {
    const int c0 = locase1251(*s0++)&0xff;
    const int c1 = locase1251(*s1++)&0xff;
    const int diff = c0-c1;
    if (diff) return (diff < 0 ? -1 : 1);
    // `c0` and `c1` are equal
    if (!c0) return 0;
  }
  // equal up to `max` chars
  return 0;
}


//==========================================================================
//
//  VStr::utf2win
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::utf2win () const noexcept {
  // check if we should do anything at all
  int len = length();
  if (!len) return VStr(*this);
  for (const vuint8 *s = (const vuint8 *)getData(); *s; ++s) {
    if (*s >= 128) {
      VStr res;
      VUtf8DecoderFast dc;
      const char *data = getData();
      while (len--) { if (dc.put(*data++)) res += wchar2win(dc.codepoint); }
      return res;
    }
  }
  return VStr(*this);
}


//==========================================================================
//
//  VStr::win2utf
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::win2utf () const noexcept {
  // check if we should do anything at all
  int len = length();
  if (!len) return VStr(*this);
  for (const vuint8 *s = (const vuint8 *)getData(); *s; ++s) {
    if (*s >= 128) {
      VStr res;
      const vuint8 *data = (const vuint8 *)getData();
      while (len--) {
        vuint8 ch = *data++;
        if (ch > 127) res.utf8Append(cp1251[ch-128]); else res += (char)ch;
      }
      return res;
    }
  }
  return VStr(*this);
}


//==========================================================================
//
//  VStr::utf2koi
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::utf2koi () const noexcept {
  // check if we should do anything at all
  int len = length();
  if (!len) return VStr(*this);
  for (const vuint8 *s = (const vuint8 *)getData(); *s; ++s) {
    if (*s >= 128) {
      VStr res;
      VUtf8DecoderFast dc;
      const char *data = getData();
      while (len--) { if (dc.put(*data++)) res += wchar2koi(dc.codepoint); }
      return res;
    }
  }
  return VStr(*this);
}


//==========================================================================
//
//  VStr::koi2utf
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::koi2utf () const noexcept {
  // check if we should do anything at all
  int len = length();
  if (!len) return VStr(*this);
  for (const vuint8 *s = (const vuint8 *)getData(); *s; ++s) {
    if (*s >= 128) {
      VStr res;
      const vuint8 *data = (const vuint8 *)getData();
      while (len--) {
        vuint8 ch = *data++;
        if (ch > 127) res.utf8Append(cpKOI[ch-128]); else res += (char)ch;
      }
      return res;
    }
  }
  return VStr(*this);
}


//==========================================================================
//
//  VStr::fnameEqu1251CI
//
//==========================================================================
VVA_CHECKRESULT bool VStr::fnameEqu1251CI (const char *s) const noexcept {
  size_t slen = (size_t)length();
  if (!s || !s[0]) return (slen == 0);
  size_t pos = 0;
  const char *data = getData();
  while (pos < slen && *s) {
    if (data[pos] == '/') {
      if (*s != '/') return false;
      while (pos < slen && data[pos] == '/') ++pos;
      while (*s == '/') ++s;
      continue;
    }
    if (locase1251(data[pos]) != locase1251(*s)) return false;
    ++pos;
    ++s;
  }
  return (*s == 0);
}


//==========================================================================
//
//  VStr::mid
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::mid (int start, int len) const noexcept {
  const int mylen = length();
  if (mylen == 0) return VStr();
  if (len <= 0 || start >= mylen) return VStr();
  if (start < 0) {
    start = -start;
    if (start < 0) return VStr();
    if (start >= len) return VStr(); // too far
    len -= start;
    start = 0;
  }
  if (mylen-start < len) {
    len = mylen-start;
    if (len < 1) return VStr();
  }
  if (start == 0 && len == mylen) return VStr(*this);
  return VStr(getData()+start, len);
}


//==========================================================================
//
//  VStr::left
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::left (int len) const noexcept {
  if (len < 1) return VStr();
  if (len >= length()) return VStr(*this);
  return mid(0, len);
}


//==========================================================================
//
//  VStr::right
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::right (int len) const noexcept {
  if (len < 1) return VStr();
  if (len >= length()) return VStr(*this);
  return mid(length()-len, len);
}


//==========================================================================
//
//  VStr::leftskip
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::leftskip (int skiplen) const noexcept {
  if (skiplen < 1) return VStr(*this);
  if (skiplen >= length()) return VStr();
  return mid(skiplen, length());
}


//==========================================================================
//
//  VStr::rightskip
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::rightskip (int skiplen) const noexcept {
  if (skiplen < 1) return VStr(*this);
  if (skiplen >= length()) return VStr();
  return mid(0, length()-skiplen);
}


//==========================================================================
//
//  VStr::chopLeft
//
//==========================================================================
void VStr::chopLeft (int len) noexcept {
  if (len < 1) return;
  if (len >= length()) { clear(); return; }
  makeMutable();
  memmove(dataptr, dataptr+len, length()-len);
  resize(length()-len);
}


//==========================================================================
//
//  VStr::chopRight
//
//==========================================================================
void VStr::chopRight (int len) noexcept {
  if (len < 1) return;
  if (len >= length()) { clear(); return; }
  resize(length()-len);
}


//==========================================================================
//
//  VStr::makeImmutable
//
//==========================================================================
void VStr::makeImmutable () noexcept {
  if (!dataptr) return; // nothing to do
  if (!atomicIsImmutable()) {
    //store()->rc = -0x0fffffff; // any negative means "immutable"
    atomicSetRC(-0x00ffffff); // any negative means "immutable"
  }
}


//==========================================================================
//
//  VStr::makeImmutableRetSelf
//
//==========================================================================
VStr &VStr::makeImmutableRetSelf () noexcept {
  makeImmutable();
  return *this;
}


//==========================================================================
//
//  VStr::cloneUnique
//
//  this will create an unique copy of the string,
//  which (copy) can be used in other threads
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::cloneUnique () const noexcept {
  if (!dataptr) return VStr();
  int len = length();
  vassert(len > 0);
  VStr res;
  res.setLength(len, 0); // fill with zeroes, why not?
  memcpy(res.dataptr, dataptr, len);
  return res;
}


//==========================================================================
//
//  VStr::cloneUniqueMT
//
//  this will create an unique copy of the string,
//  which (copy) can be used in other threads
//
//  doesn't clone immutable strings, though
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::cloneUniqueMT () const noexcept {
  if (!dataptr) return VStr();
  if (atomicIsImmutable()) return *this;
  const int len = length();
  vassert(len > 0);
  VStr res;
  res.setLength(len, 0); // fill with zeroes, why not?
  memcpy(res.dataptr, dataptr, len);
  return res;
}


//==========================================================================
//
//  VStr::makeMutable
//
//==========================================================================
void VStr::makeMutable () noexcept {
  if (!dataptr) return;
  if (atomicIsUnique()) return; // nothing to do
  // allocate new string
  Store *oldstore = store();
  const char *olddata = dataptr;
  size_t olen = (size_t)oldstore->length;
  size_t newsz = vstrCalcInitialStoreSizeSizeT(olen);
  Store *newdata = (Store *)Z_MallocNoClearNoFail(sizeof(Store)+newsz+1u);
  if (!newdata) {
    newsz = olen;
    newdata = (Store *)Z_MallocNoClearNoFail(sizeof(Store)+newsz+1u);
    if (!newdata) Sys_Error("Out of memory for VStr");
  }
  newdata->length = (vint32)olen;
  newdata->alloted = (vint32)newsz;
  newdata->rc = 1;
  dataptr = ((char *)newdata)+sizeof(Store);
  // copy old data
  memcpy(dataptr, olddata, olen+1);
  //if (oldstore->rc > 0) --oldstore->rc; // decrement old refcounter
  if (__atomic_load_n(&oldstore->rc, __ATOMIC_SEQ_CST) > 0) {
    (void)__atomic_sub_fetch(&oldstore->rc, 1, __ATOMIC_SEQ_CST);
  }
#ifdef VAVOOM_TEST_VSTR
  GLog.Logf(NAME_Debug, "VStr: makeMutable: old=%p(%d); new=%p(%d)", oldstore+1, oldstore->rc, dataptr, newdata->rc);
#endif
}


//==========================================================================
//
//  VStr::resize
//
//==========================================================================
void VStr::resize (int newlen) noexcept {
  // free string?
  if (newlen <= 0) {
    decref();
    return;
  }

  const int oldlen = length();

  if (newlen == oldlen) {
    // same length, make string unique (just in case)
    makeMutable();
    return;
  }

  // new allocation?
  if (!dataptr) {
    size_t newsz = vstrCalcInitialStoreSizeSizeT((size_t)newlen);
    Store *ns = (Store *)Z_MallocNoClearNoFail(sizeof(Store)+newsz+1u);
    if (!ns) {
      newsz = (size_t)newlen;
      ns = (Store *)Z_MallocNoClearNoFail(sizeof(Store)+newsz+1u);
      if (!ns) Sys_Error("Out of memory for VStr");
    }
    ns->length = newlen;
    ns->alloted = (vint32)newsz;
    ns->rc = 1;
    #ifdef VAVOOM_TEST_VSTR
    fprintf(stderr, "VStr: realloced(new): old=%p(%d); new=%p(%d)\n", dataptr, 0, ns+1, ns->rc);
    #endif
    dataptr = ((char *)ns)+sizeof(Store);
    dataptr[newlen] = 0;
    return;
  }

  // unique?
  if (atomicIsUnique()) {
    // do in-place reallocs
    if (newlen < oldlen) {
      // shrink
      if (newlen < store()->alloted/2) {
        // realloc
        store()->alloted = vstrCalcInitialStoreSizeInt(newlen);
        Store *ns = (Store *)Z_ReallocNoFail(store(), sizeof(Store)+(size_t)store()->alloted+1u);
        if (!ns) {
          store()->alloted = newlen;
          ns = (Store *)Z_ReallocNoFail(store(), sizeof(Store)+(size_t)store()->alloted+1u);
          if (!ns) Sys_Error("Out of memory for VStr");
        }
        #ifdef VAVOOM_TEST_VSTR
        fprintf(stderr, "VStr: realloced(shrink): old=%p(%d); new=%p(%d)\n", dataptr, store()->rc, ns+1, ns->rc);
        #endif
        dataptr = ((char *)ns)+sizeof(Store);
      }
    } else if (newlen > store()->alloted) {
      // need more room, grow
      size_t newsz = vstrCalcGrowStoreSizeSizeT((size_t)newlen);
      Store *ns = (Store *)Z_ReallocNoFail(store(), sizeof(Store)+newsz+1u);
      if (!ns) {
        // try exact
        newsz = (size_t)newlen;
        ns = (Store *)Z_ReallocNoFail(store(), sizeof(Store)+newsz+1u);
        if (!ns) Sys_Error("Out of memory for VStr");
      }
      ns->alloted = (vuint32)newsz;
      #ifdef VAVOOM_TEST_VSTR
      fprintf(stderr, "VStr: realloced(grow): old=%p(%d); new=%p(%d)\n", dataptr, store()->rc, ns+1, ns->rc);
      #endif
      dataptr = ((char *)ns)+sizeof(Store);
    }
    store()->length = newlen;
  } else {
    // not unique, have to allocate new data
    int alloclen = vstrCalcInitialStoreSizeInt(newlen);

    // allocate new storage
    Store *ns = (Store *)Z_MallocNoClearNoFail(sizeof(Store)+(size_t)alloclen+1u);
    if (!ns) {
      // try exact
      alloclen = newlen;
      ns = (Store *)Z_MallocNoClearNoFail(sizeof(Store)+(size_t)alloclen+1u);
      if (!ns) Sys_Error("Out of memory for VStr");
    }

    // copy data
    int copylen = (newlen < oldlen ? newlen : oldlen);
    if (copylen) memcpy(((char *)ns)+sizeof(Store), dataptr, (size_t)copylen);
    // setup info
    ns->length = newlen;
    ns->alloted = alloclen;
    ns->rc = 1;
    // decrement old rc
    if (!atomicIsImmutable()) {
      const int nrc = atomicDecRC();
      vassert(nrc > 0);
    }
    #ifdef VAVOOM_TEST_VSTR
    fprintf(stderr, "VStr: realloced(new): old=%p(%d); new=%p(%d)\n", dataptr, store()->rc, ns+1, ns->rc);
    #endif
    // use new data
    dataptr = ((char *)ns)+sizeof(Store);
  }

  // most code expects this
  dataptr[newlen] = 0;
}


//==========================================================================
//
//  VStr::setContent
//
//==========================================================================
void VStr::setContent (const char *s, int len) noexcept {
  if (s && s[0]) {
    if (len < 0) len = (int)strlen(s);
    size_t newsz = vstrCalcInitialStoreSizeSizeT((size_t)len);
    Store *ns = (Store *)Z_MallocNoClearNoFail(sizeof(Store)+(size_t)newsz+1u);
    if (!ns) {
      newsz = (size_t)len;
      ns = (Store *)Z_MallocNoClearNoFail(sizeof(Store)+(size_t)newsz+1u);
      if (!ns) Sys_Error("Out of memory for VStr");
    }
    ns->length = len;
    ns->alloted = (int)newsz;
    ns->rc = 1;
    memcpy(((char *)ns)+sizeof(Store), s, len);
    // free this string
    clear();
    // use new data
    dataptr = ((char *)ns)+sizeof(Store);
    dataptr[len] = 0;
    #ifdef VAVOOM_TEST_VSTR
    fprintf(stderr, "VStr: setContent: new=%p(%d)\n", dataptr, store()->rc);
    #endif
  } else {
    clear();
  }
}


//==========================================================================
//
//  VStr::setNameContent
//
//==========================================================================
void VStr::setNameContent (const VName InName) noexcept {
  clear();
  if (!VName::IsInitialised()) {
    setContent(*InName);
  } else {
    //GLog.Logf("NAME->STR: '%s'", *InName);
    dataptr = (char *)(*InName);
  }
}


//==========================================================================
//
//  VStr::StartsWith
//
//==========================================================================
VVA_CHECKRESULT bool VStr::StartsWith (const char *s) const noexcept {
  if (!s || !s[0]) return false;
  const int l = length(s);
  if (l > length()) return false;
  return (memcmp(getData(), s, (size_t)l) == 0);
}


//==========================================================================
//
//  VStr::StartsWith
//
//==========================================================================
VVA_CHECKRESULT bool VStr::StartsWith (const VStr &s) const noexcept {
  const int l = s.length();
  if (l == 0 || l > length()) return false;
  return (memcmp(getData(), *s, (size_t)l) == 0);
}


//==========================================================================
//
//  VStr::EndsWith
//
//==========================================================================
VVA_CHECKRESULT bool VStr::EndsWith (const char *s) const noexcept {
  if (!s || !s[0]) return false;
  const int l = Length(s);
  if (l > length()) return false;
  return (memcmp(getData()+length()-l, s, (size_t)l) == 0);
}


//==========================================================================
//
//  VStr::EndsWith
//
//==========================================================================
VVA_CHECKRESULT bool VStr::EndsWith (const VStr &s) const noexcept {
  const int l = s.length();
  if (l == 0 || l > length()) return false;
  return (memcmp(getData()+length()-l, *s, (size_t)l) == 0);
}


//==========================================================================
//
//  VStr::startsWithNoCase
//
//==========================================================================
VVA_CHECKRESULT bool VStr::startsWithNoCase (const char *s) const noexcept {
  if (!s || !s[0]) return false;
  const int l = length(s);
  if (l > length()) return false;
  return (NICmp(getData(), s, (size_t)l) == 0);
}


//==========================================================================
//
//  VStr::startsWithNoCase
//
//==========================================================================
VVA_CHECKRESULT bool VStr::startsWithNoCase (const VStr &s) const noexcept {
  const int l = s.length();
  if (l == 0 || l > length()) return false;
  return (NICmp(getData(), *s, (size_t)l) == 0);
}


//==========================================================================
//
//  VStr::endsWithNoCase
//
//==========================================================================
VVA_CHECKRESULT bool VStr::endsWithNoCase (const char *s) const noexcept {
  if (!s || !s[0]) return false;
  const int l = Length(s);
  if (l > length()) return false;
  return (NICmp(getData()+length()-l, s, (size_t)l) == 0);
}


//==========================================================================
//
//  VStr::endsWithNoCase
//
//==========================================================================
VVA_CHECKRESULT bool VStr::endsWithNoCase (const VStr &s) const noexcept {
  const int l = s.length();
  if (l == 0 || l > length()) return false;
  return (NICmp(getData()+length()-l, *s, (size_t)l) == 0);
}


//==========================================================================
//
//  VStr::startsWith
//
//==========================================================================
VVA_CHECKRESULT bool VStr::startsWith (const char *str, const char *part) noexcept {
  if (!str || !str[0]) return false;
  if (!part || !part[0]) return false;
  const int slen = VStr::length(str);
  const int plen = VStr::length(part);
  if (plen > slen) return false;
  return (memcmp(str, part, (size_t)plen) == 0);
}


//==========================================================================
//
//  VStr::endsWith
//
//==========================================================================
VVA_CHECKRESULT bool VStr::endsWith (const char *str, const char *part) noexcept {
  if (!str || !str[0]) return false;
  if (!part || !part[0]) return false;
  const int slen = VStr::length(str);
  const int plen = VStr::length(part);
  if (plen > slen) return false;
  return (memcmp(str+slen-plen, part, (size_t)plen) == 0);
}


//==========================================================================
//
//  VStr::startsWithNoCase
//
//==========================================================================
VVA_CHECKRESULT bool VStr::startsWithNoCase (const char *str, const char *part) noexcept {
  if (!str || !str[0]) return false;
  if (!part || !part[0]) return false;
  const int slen = VStr::length(str);
  const int plen = VStr::length(part);
  if (plen > slen) return false;
  return (NICmp(str, part, (size_t)plen) == 0);
}


//==========================================================================
//
//  VStr::endsWithNoCase
//
//==========================================================================
VVA_CHECKRESULT bool VStr::endsWithNoCase (const char *str, const char *part) noexcept {
  if (!str || !str[0]) return false;
  if (!part || !part[0]) return false;
  const int slen = VStr::length(str);
  const int plen = VStr::length(part);
  if (plen > slen) return false;
  return (NICmp(str+slen-plen, part, (size_t)plen) == 0);
}


//==========================================================================
//
//  VStr::ToLower
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::ToLower () const noexcept {
  if (!dataptr) return VStr();
  const int l = length();
  if (!l) return VStr();
  bool hasWork = false;
  const char *data = getData();
  for (int i = 0; i < l; ++i) if (data[i] >= 'A' && data[i] <= 'Z') { hasWork = true; break; }
  if (hasWork) {
    VStr res(*this);
    res.makeMutable();
    for (int i = 0; i < l; ++i) if (res.dataptr[i] >= 'A' && res.dataptr[i] <= 'Z') res.dataptr[i] += 32; // poor man's tolower()
    return res;
  } else {
    return VStr(*this);
  }
}


//==========================================================================
//
//  VStr::ToUpper
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::ToUpper () const noexcept {
  const char *data = getData();
  if (!data) return VStr();
  int l = length();
  if (!l) return VStr();
  bool hasWork = false;
  for (int i = 0; i < l; ++i) if (data[i] >= 'a' && data[i] <= 'z') { hasWork = true; break; }
  if (hasWork) {
    VStr res(*this);
    res.makeMutable();
    for (int i = 0; i < l; ++i) if (res.dataptr[i] >= 'a' && res.dataptr[i] <= 'z') res.dataptr[i] -= 32; // poor man's toupper()
    return res;
  } else {
    return VStr(*this);
  }
}


//==========================================================================
//
//  VStr::isLowerCase
//
//==========================================================================
VVA_CHECKRESULT bool VStr::isLowerCase () const noexcept {
  const char *dp = getData();
  return VStr::isLowerCase(dp);
}


//==========================================================================
//
//  VStr::isLowerCase
//
//==========================================================================
VVA_CHECKRESULT bool VStr::isLowerCase (const char *s) noexcept {
  if (!s) return true;
  while (*s) {
    if (*s >= 'A' && *s <= 'Z') return false;
    ++s;
  }
  return true;
}


#define NORM_STPOS()  do { \
  if (stpos < 0) { \
    if (stpos == MIN_VINT32) stpos = 0; else stpos += l; \
    if (stpos < 0) stpos = 0; \
  } \
  if (stpos >= l) return -1; \
} while (0)


//==========================================================================
//
//  k8memmem
//
//==========================================================================
static inline VVA_CHECKRESULT const char *k8memmem (const char *hay, size_t haylen, const char *need, size_t needlen) noexcept {
  if (haylen < needlen || needlen == 0) return nullptr;
  haylen -= needlen;
  ++haylen;
  while (haylen--) {
    if (memcmp(hay, need, needlen) == 0) return hay;
    ++hay;
  }
  return nullptr;
}


//==========================================================================
//
//  k8memrmem
//
//==========================================================================
static inline VVA_CHECKRESULT const char *k8memrmem (const char *hay, size_t haylen, const char *need, size_t needlen) noexcept {
  if (haylen < needlen || needlen == 0) return nullptr;
  haylen -= needlen;
  hay += haylen;
  ++haylen;
  while (haylen--) {
    if (memcmp(hay, need, needlen) == 0) return hay;
    --hay;
  }
  return nullptr;
}


//==========================================================================
//
//  VStr::IndexOf
//
//==========================================================================
VVA_CHECKRESULT int VStr::IndexOf (char c, int stpos) const noexcept {
  const char *data = getData();
  if (data && length()) {
    int l = int(length());
    NORM_STPOS();
    const char *pos = (const char *)memchr(data+stpos, c, length()-stpos);
    return (pos ? (int)(ptrdiff_t)(pos-data) : -1);
  } else {
    return -1;
  }
}


//==========================================================================
//
//  VStr::IndexOf
//
//==========================================================================
VVA_CHECKRESULT int VStr::IndexOf (const char *s, int stpos) const noexcept {
  if (!s || !s[0]) return -1;
  int sl = int(Length(s));
  int l = int(length());
  if (l == 0 || sl == 0) return -1;
  NORM_STPOS();
  if (l-stpos < sl) return -1;
#if 0
  const char *data = getData()+stpos;
  for (int i = 0; i <= l-sl-stpos; ++i) if (NCmp(data+i, s, sl) == 0) return i;
#else
  const char *data = getData();
  const char *pos = k8memmem(data+stpos, l-stpos, s, sl);
  return (pos ? (int)(ptrdiff_t)(pos-data) : -1);
#endif
  return -1;
}


//==========================================================================
//
//  VStr::IndexOf
//
//==========================================================================
VVA_CHECKRESULT int VStr::IndexOf (const VStr &s, int stpos) const noexcept {
  int sl = int(s.length());
  if (!sl) return -1;
  int l = int(length());
  if (l == 0) return -1;
  NORM_STPOS();
  if (l-stpos < sl) return -1;
#if 0
  const char *data = getData()+stpos;
  for (int i = 0; i <= l-sl-stpos; ++i) if (NCmp(data+i, *s, sl) == 0) return i;
#else
  const char *data = getData();
  const char *pos = k8memmem(data+stpos, l-stpos, *s, sl);
  return (pos ? (int)(ptrdiff_t)(pos-data) : -1);
#endif
  return -1;
}


//==========================================================================
//
//  VStr::LastIndexOf
//
//==========================================================================
VVA_CHECKRESULT int VStr::LastIndexOf (char c, int stpos) const noexcept {
  const char *data = getData();
  if (data && length()) {
    int l = int(length());
    NORM_STPOS();
#if !defined(_WIN32) && !defined(NO_MEMRCHR)
    const char *pos = (const char *)memrchr(data+stpos, c, l-stpos);
    return (pos ? (int)(pos-data) : -1);
#else
    size_t pos = (size_t)l;
    while (pos > (size_t)stpos && data[pos-1] != c) --pos;
    return (pos ? (int)(ptrdiff_t)(pos-1) : -1);
#endif
  } else {
    return -1;
  }
}


//==========================================================================
//
//  VStr::LastIndexOf
//
//==========================================================================
VVA_CHECKRESULT int VStr::LastIndexOf (const char *s, int stpos) const noexcept {
  if (!s || !s[0]) return -1;
  int sl = int(Length(s));
  int l = int(length());
  if (l == 0 || sl == 0) return -1;
  NORM_STPOS();
  if (l-stpos < sl) return -1;
#if 0
  const char *data = getData()+stpos;
  for (int i = l-sl-stpos; i >= 0; --i) if (NCmp(data+i, s, sl) == 0) return i;
#else
  const char *data = getData();
  const char *pos = k8memrmem(data+stpos, l-stpos, s, sl);
  return (pos ? (int)(ptrdiff_t)(pos-data) : -1);
#endif
  return -1;
}


//==========================================================================
//
//  VStr::LastIndexOf
//
//==========================================================================
VVA_CHECKRESULT int VStr::LastIndexOf (const VStr &s, int stpos) const noexcept {
  int sl = int(s.length());
  if (!sl) return -1;
  int l = int(length());
  if (l == 0) return -1;
  //fprintf(stderr, "0: stpos=%d; len=%d; slen=%d\n", stpos, l, sl);
  NORM_STPOS();
  if (l-stpos < sl) return -1;
#if 0
  const char *data = getData()+stpos;
  for (int i = l-sl-stpos; i >= 0; --i) if (NCmp(data+i, *s, sl) == 0) return i;
#else
  const char *data = getData();
  //fprintf(stderr, "1: stpos=%d; len=%d; slen=%d (%s)\n", stpos, l, sl, data+stpos);
  const char *pos = k8memrmem(data+stpos, l-stpos, *s, sl);
  return (pos ? (int)(ptrdiff_t)(pos-data) : -1);
#endif
  return -1;
}


//==========================================================================
//
//  VStr::Replace
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::Replace (const char *Search, const char *Replacement) const noexcept {
  if (length() == 0) return VStr(); // nothing to replace in an empty string
  if (!Search || !Search[0]) return VStr(*this);
  if (!Replacement) Replacement = "";

  size_t SLen = Length(Search);
  size_t RLen = Length(Replacement);
  if (!SLen) return VStr(*this); // nothing to search for

  VStr res(*this);
  size_t i = 0;
  while ((size_t)res.length() >= SLen && i <= (size_t)res.length()-SLen) {
    if (NCmp(res.getData()+i, Search, SLen) == 0) {
      // if search and replace strings are of the same size,
      // we can just copy the data and avoid memory allocations
      if (SLen == RLen) {
        res.makeMutable();
        memcpy(res.dataptr+i, Replacement, RLen);
      } else {
        //FIXME: optimize this!
        res = VStr(res, 0, int(i))+Replacement+VStr(res, int(i+SLen), int(res.length()-i-SLen));
      }
      i += RLen;
    } else {
      ++i;
    }
  }

  return res;
}


//==========================================================================
//
//  VStr::Utf8Substring
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::Utf8Substring (int start, int len) const noexcept {
  vassert(start >= 0);
  vassert(start <= (int)Utf8Length());
  vassert(len >= 0);
  vassert(start+len <= (int)Utf8Length());
  if (!len) return VStr();
  int RealStart = int(ByteLengthForUtf8(getData(), start));
  int RealLen = int(ByteLengthForUtf8(getData(), start+len)-RealStart);
  return VStr(*this, RealStart, RealLen);
}


//==========================================================================
//
//  VStr::Split
//
//==========================================================================
void VStr::Split (char c, TArray<VStr> &A, bool keepEmpty) const noexcept {
  A.Clear();
  const char *data = getData();
  if (!data) return;
  if (!c) { A.Append(*this); return; }
  int start = 0;
  const int len = length();
  for (int i = 0; i <= len; ++i) {
    if (i == len || data[i] == c) {
      if (keepEmpty || start != i) A.Append(VStr(*this, start, i-start));
      start = i+1;
    }
  }
}


//==========================================================================
//
//  VStr::Split
//
//==========================================================================
void VStr::Split (const char *chars, TArray<VStr> &A, bool keepEmpty) const noexcept {
  A.Clear();
  const char *data = getData();
  if (!data) return;
  if (!chars) { A.Append(*this); return; }
  int start = 0;
  const int len = length();
  for (int i = 0; i <= len; ++i) {
    if (i == len || strchr(chars, data[i])) {
      if (keepEmpty || start != i) A.Append(VStr(*this, start, i-start));
      start = i+1;
    }
  }
}


//==========================================================================
//
//  VStr::SplitOnBlanks
//
//==========================================================================
void VStr::SplitOnBlanks (TArray<VStr> &A, bool doQuotedStrings) const noexcept {
  A.Clear();
  const char *data = getData();
  if (!data) return;
  int len = length();
  int pos = 0;
  while (pos < len) {
    vuint8 ch = (vuint8)data[pos++];
    if (ch <= ' ') continue;
    int start = pos-1;
    if (doQuotedStrings && (ch == '\'' || ch == '"')) {
      vuint8 ech = ch;
      while (pos < len) {
        ch = (vuint8)data[pos++];
        if (ch == ech) break;
        if (ch == '\\') { if (pos < len) ++pos; }
      }
    } else {
      while (pos < len && (vuint8)data[pos] > ' ') ++pos;
    }
    A.append(VStr(*this, start, pos-start));
  }
}


//==========================================================================
//
//  VStr::SplitPath
//
//  split string to path components
//
//  first returned component can be '/'.
//  for shitdoze, first returned component can be:
//    "/"
//    "x:"
//    "x:/"
//    "//unc"
//
//  others has no slashes
//
//==========================================================================
void VStr::SplitPath (TArray<VStr> &arr) const noexcept {
  arr.Clear();
  const char *data = getData();
  if (!data) return;

  int pos = 0;
  int len = length();

#if !defined(_WIN32)
  if (data[0] == '/') arr.Append(VStr("/"));
  while (pos < len) {
    if (data[pos] == '/') { ++pos; continue; }
    int epos = pos+1;
    while (epos < len && data[epos] != '/') ++epos;
    arr.Append(mid(pos, epos-pos));
    pos = epos+1;
  }
#else
  if (IsPathSeparatorCharStrict(data[0])) {
    arr.Append(VStr("/"));
    pos = 1;
  } else if (data[1] == ':') {
    if (IsPathSeparatorCharStrict(data[2])) {
      arr.Append(mid(0, 3));
      pos = 3;
    } else if (!data[2]) {
      arr.append(*this);
      return;
    } else {
      arr.Append(left(2));
      pos = 2;
    }
  } else if (IsPathSeparatorCharStrict(data[0]) && IsPathSeparatorCharStrict(data[1])) {
    // unc path
    if (!data[2]) { arr.Append(VStr("/")); return; }
    VStr res("//");
    pos = 3;
    while (pos < len && !IsPathSeparatorCharStrict(data[pos])) res += data[pos++];
  }
  while (pos < len) {
    if (IsPathSeparatorCharStrict(data[pos])) { ++pos; continue; }
    int epos = pos+1;
    while (epos < len && !IsPathSeparatorCharStrict(data[epos])) ++epos;
    arr.Append(mid(pos, epos-pos));
    pos = epos+1;
  }
#endif
}


//==========================================================================
//
//  VStr::IsSplittedPathAbsolute
//
//==========================================================================
bool VStr::IsSplittedPathAbsolute (const TArray<VStr> &spp) noexcept {
  if (spp.length() == 0) return false;
  // check for absolute pathes
#if !defined(_WIN32)
  return (spp[0] == "/");
#else
  VStr fsp = spp[0];
  if (fsp == "/") return true;
  if (fsp.length() >= 2 && fsp[1] == ':') return true;
  return false;
#endif
}


//==========================================================================
//
//  VStr::trimRight
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::trimRight () const noexcept {
  const char *s = getCStr();
  const int len = length();
  int pos = len;
  while (pos > 0 && (vuint8)(s[pos-1]) <= ' ') --pos;
  if (pos == 0) return VStr();
  if (pos == len) return VStr(*this);
  return left(pos);
}


//==========================================================================
//
//  VStr::trimLeft
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::trimLeft () const noexcept {
  const char *s = getCStr();
  const int len = length();
  int pos = 0;
  while (pos < len && (vuint8)(s[pos]) <= ' ') ++pos;
  if (pos == len) return VStr();
  if (pos == 0) return VStr(*this);
  return mid(pos, len);
}


//==========================================================================
//
//  VStr::trimAll
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::trimAll () const noexcept {
  const char *s = getCStr();
  const int len = length();
  int lc = 0, rc = len;
  while (lc < len && (vuint8)(s[lc]) <= ' ') ++lc;
  while (rc > lc && (vuint8)(s[rc-1]) <= ' ') --rc;
  if (lc == 0 && rc == len) return VStr(*this);
  if (lc == rc) return VStr();
  return mid(lc, rc-lc);
}


//==========================================================================
//
//  VStr::Latin1ToUtf8
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::Latin1ToUtf8 () const noexcept {
  const char *data = getData();
  if (!data) return VStr();
  VStr res;
  for (int i = 0; i < length(); ++i) res += FromUtf8Char((vuint8)data[i]);
  return res;
}


//==========================================================================
//
//  VStr::xmlEscape
//
//  FIXME: make this faster
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::xmlEscape () const noexcept {
  const char *c = getData();
  if (!c || !c[0]) return VStr();
  for (const char *s = c; *s; ++s) {
    switch (*s) {
      case '&':
      case '"':
      case '\'':
      case '<':
      case '>':
        // do the real work
        {
          VStr res;
          while (*c) {
            switch (*c) {
              case '&': res += "&amp;"; break;
              case '"': res += "&quot;"; break;
              case '\'': res += "&apos;"; break;
              case '<': res += "&lt;"; break;
              case '>': res += "&gt;"; break;
              default: res += *c; break;
            }
            ++c;
          }
          return res;
        }
    }
  }
  return VStr(*this);
}


//==========================================================================
//
//  VStr::xmlUnescape
//
//  FIXME: make this faster
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::xmlUnescape () const noexcept {
  const char *c = getData();
  if (!c || !c[0]) return VStr();
  if (!strchr(c, '&')) return VStr(*this);
  VStr res;
  while (*c) {
    if (c[0] == '&') {
      if (c[1] == 'a' && c[2] == 'm' && c[3] == 'p' && c[4] == ';') {
        res += '&';
        c += 5;
        continue;
      }
      if (c[1] == 'q' && c[2] == 'u' && c[3] == 'o' && c[4] == 't' && c[5] == ';') {
        res += '"';
        c += 6;
        continue;
      }
      if (c[1] == 'a' && c[2] == 'p' && c[3] == 'o' && c[4] == 's' && c[5] == ';') {
        res += '\'';
        c += 6;
        continue;
      }
      if (c[1] == 'l' && c[2] == 't' && c[3] == ';') {
        res += '<';
        c += 4;
        continue;
      }
      if (c[1] == 'g' && c[2] == 't' && c[3] == ';') {
        res += '>';
        c += 4;
        continue;
      }
    }
    res += *c++;
  }
  return res;
}


//==========================================================================
//
//  VStr::EvalEscapeSequences
//
//  this translates "\\c" to `\c`
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::EvalEscapeSequences () const noexcept {
  const char *c = getCStr();
  if (!c[0]) return VStr();
  if (!strchr(c, '\\')) return VStr(*this);
  VStr res;
  int val;
  while (*c) {
    const char *slashp = strchr(c, '\\');
    if (!slashp) slashp = c+strlen(c);
    if (slashp != c) {
      res.appendCStr(c, (int)(ptrdiff_t)(slashp-c));
      c = slashp;
      if (!c[0]) break;
    }
    vassert(c[0] == '\\');
    if (!c[1]) { res += '\\'; break; }
    // interpret escape sequence
    char ec = c[1];
    c += 2;
    switch (ec) {
      case 't': res += '\t'; break;
      case 'n': res += '\n'; break;
      case 'r': res += '\r'; break;
      case 'e': res += '\x1b'; break;
      case 'c': res += (char)TEXT_COLOR_ESCAPE; break;
      case 'x':
        val = digitInBase(*c, 16);
        if (val >= 0) {
          ++c;
          int d = digitInBase(*c, 16);
          if (d >= 0) {
            ++c;
            val = val*16+d;
          }
          if (val == 0) val = ' ';
          res += (char)val;
        }
        break;
      case '0': case '1': case '2': case '3':
      case '4': case '5': case '6': case '7':
        val = 0;
        for (int i = 0; i < 3; ++i) {
          int d = digitInBase(*c, 8);
          if (d < 0) break;
          val = val*8+d;
          ++c;
        }
        if (val == 0) val = ' ';
        res += (char)val;
        break;
      case '\n':
        break;
      case '\r':
        if (*c == '\n') ++c;
        break;
      default:
        res += '\\';
        res += ec;
        break;
    }
  }
  /*
  int val;
  while (*c) {
    if (c[0] == '\\' && c[1]) {
      ++c;
      switch (*c++) {
        case 't': res += '\t'; break;
        case 'n': res += '\n'; break;
        case 'r': res += '\r'; break;
        case 'e': res += '\x1b'; break;
        case 'c': res += TEXT_COLOR_ESCAPE; break;
        case 'x':
          val = digitInBase(*c, 16);
          if (val >= 0) {
            ++c;
            int d = digitInBase(*c, 16);
            if (d >= 0) {
              ++c;
              val = val*16+d;
            }
            if (val == 0) val = ' ';
            res += (char)val;
          }
          break;
        case '0': case '1': case '2': case '3':
        case '4': case '5': case '6': case '7':
          val = 0;
          for (int i = 0; i < 3; ++i) {
            int d = digitInBase(*c, 8);
            if (d < 0) break;
            val = val*8+d;
            ++c;
          }
          if (val == 0) val = ' ';
          res += (char)val;
          break;
        case '\n':
          break;
        case '\r':
          if (*c == '\n') ++c;
          break;
        default:
          res += c[-1];
          break;
      }
    } else {
      res += *c++;
    }
  }
  */
  return res;
}


//==========================================================================
//
//  VStr::MustBeSanitized
//
//==========================================================================
VVA_CHECKRESULT bool VStr::MustBeSanitized () const noexcept {
  int len = (int)length();
  if (len < 1) return false;
  const vuint8 *s = (const vuint8 *)getData();
  while (len--) {
    vuint8 ch = *s++;
    ++s;
    if (ch != '\n' && ch != '\t' && ch != '\r') {
      if (ch < ' ' || ch == 127) return true;
    }
  }
  return false;
}


//==========================================================================
//
//  VStr::MustBeSanitized
//
//==========================================================================
VVA_CHECKRESULT bool VStr::MustBeSanitized (const char *str) noexcept {
  if (!str || !str[0]) return false;
  for (const vuint8 *s = (const vuint8 *)str; *s; ++s) {
    if (*s != '\n' && *s != '\t' && *s != '\r') {
      if (*s < ' ' || *s == 127) return true;
    }
  }
  return false;
}


//==========================================================================
//
//  VStr::RemoveColors
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::RemoveColors () const noexcept {
  const char *data = getData();
  if (!data) return VStr();
  const int oldlen = (int)length();
  // calculate new length
  int newlen = 0;
  int pos = 0;
  while (pos < oldlen) {
    char c = data[pos++];
    if (c == TEXT_COLOR_ESCAPE) {
      if (pos >= oldlen) break;
      c = data[pos++];
      if (!c) break;
      if (c == '[') {
        while (pos < oldlen && data[pos] && data[pos] != ']') ++pos;
        if (pos < oldlen && data[pos]) ++pos;
      }
    } else {
      if (!c) break;
      if (c != '\n' && c != '\t' && c != '\r') {
        if ((vuint8)c < ' ' || c == 127) continue;
      }
      ++newlen;
    }
  }
  if (newlen == oldlen) return VStr(*this);
  // build new string
  VStr res;
  res.resize(newlen);
  pos = 0;
  newlen = 0;
  while (pos < oldlen) {
    char c = data[pos++];
    if (c == TEXT_COLOR_ESCAPE) {
      if (pos >= oldlen) break;
      c = data[pos++];
      if (!c) break;
      if (c == '[') {
        while (pos < oldlen && data[pos] && data[pos] != ']') ++pos;
        if (pos < oldlen && data[pos]) ++pos;
      }
    } else {
      if (!c) break;
      if (newlen >= res.length()) break; // oops
      if (c != '\n' && c != '\t' && c != '\r') {
        if ((vuint8)c < ' ' || c == 127) continue;
      }
      res.dataptr[newlen++] = c;
    }
  }
  return res;
}


//==========================================================================
//
//  VStr::ExtractFilePath
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::ExtractFilePath () const noexcept {
  if (!length()) return VStr();
  const char *src = getData()+length();
  while (src != getData() && !IsPathSeparatorChar(src[-1])) --src;
  return VStr(*this, 0, src-getData());
}


//==========================================================================
//
//  VStr::ExtractFileName
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::ExtractFileName () const noexcept {
  if (!length()) return VStr();
  const char *data = getData();
  const char *src = data+length();
  while (src != data && !IsPathSeparatorChar(src[-1])) --src;
  return VStr(src);
}


//==========================================================================
//
//  VStr::ExtractFileBase
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::ExtractFileBase (bool doSysError) const noexcept {
  int i = length();
  if (i == 0) return VStr();

  const char *data = getData();
  // back up until a \ or the start
  while (i && !IsPathSeparatorChar(data[i-1])) --i;

  // copy up to eight characters
  int start = i;
  int length = 0;
  while (data[i] && data[i] != '.') {
    if (++length == 9) {
      if (doSysError) Sys_Error("Filename base of \"%s\" >8 chars", data);
      break;
    }
    ++i;
  }
  return VStr(*this, start, length);
}


//==========================================================================
//
//  VStr::ExtractFileBaseName
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::ExtractFileBaseName () const noexcept {
  int i = length();
  if (i == 0) return VStr();

  const char *data = getData();
  // back up until a \ or the start
  while (i && !IsPathSeparatorChar(data[i-1])) --i;

  return VStr(*this, i, length()-i);
}


//==========================================================================
//
//  VStr::ExtractFileExtension
//
//  with a dot
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::ExtractFileExtension () const noexcept {
  if (!length()) return VStr();
  const char *data = getData();
  const char *src = data+length();
  while (src != data) {
    char ch = src[-1];
    if (ch == '.') return VStr(src-1);
    if (IsPathSeparatorChar(ch)) return VStr();
    --src;
  }
  return VStr();
}


//==========================================================================
//
//  VStr::StripExtension
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::StripExtension () const noexcept {
  if (!length()) return VStr();
  const char *data = getData();
  const char *src = data+length();
  while (src != data) {
    char ch = src[-1];
    if (ch == '.') return VStr(*this, 0, src-data-1);
    if (IsPathSeparatorChar(ch)) break;
    --src;
  }
  return VStr(*this);
}


//==========================================================================
//
//  VStr::DefaultPath
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::DefaultPath (VStr basepath) const noexcept {
  if (!length()) return basepath;
  if (IsAbsolutePath()) return VStr(*this);
  return basepath.AppendPath(*this);
}


//==========================================================================
//
//  VStr::DefaultExtension
//
//  if path doesn't have a .EXT, append extension
//  (extension should include the leading dot)
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::DefaultExtension (VStr extension) const noexcept {
  if (!length()) return extension;
  const char *data = getData();
  const char *src = data+length();
  while (src != data) {
    char ch = src[-1];
    if (ch == '.') return VStr(*this);
    if (IsPathSeparatorChar(ch)) break;
    --src;
  }
  return VStr(*this)+extension;
}


//==========================================================================
//
//  VStr::FixFileSlashes
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::FixFileSlashes () const noexcept {
  if (!length()) return VStr();
  const char *data = getData();
  if (strchr(data, '\\')) {
    VStr res(*this);
    res.makeMutable();
    char *c = res.dataptr;
    while (*c) {
      c = (char *)strchr(c, '\\');
      if (!c) break;
      *c = '/';
      ++c;
    }
    return res;
  } else {
    return VStr(*this);
  }
}


//==========================================================================
//
//  VStr::AppendTrailingSlash
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::AppendTrailingSlash () const noexcept {
  const int slen = length();
  if (slen < 1) return VStr(*this);
  const char *data = getData();
#if !defined(_WIN32)
  if (data[slen-1] == '/') return VStr(*this);
#else
  if (slen == 2 && data[1] == ':') return VStr(*this); // disk name only, safe
  if (IsPathSeparatorChar(data[slen-1])) return VStr(*this);
#endif
  VStr res(*this);
  res += '/';
  return res;
}


//==========================================================================
//
//  VStr::RemoveTrailingSlash
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::RemoveTrailingSlash () const noexcept {
  const int len = length();
  if (len < 2) return VStr(*this);
  const char *data = getData();
#if !defined(_WIN32)
  if (data[len-1] != '/') return VStr(*this);
  int newlen = len-1;
  while (newlen > 0 && data[newlen-1] == '/') --newlen;
  if (newlen) return VStr(data, newlen);
  return VStr("/");
#else
  if (!IsPathSeparatorCharStrict(data[len-1])) return VStr(*this);
  if (len == 3 && data[1] == ':') return VStr(*this); // oops, "x:\"
  VStr path(*this);
  path.chopRight(1);
  return path.RemoveTrailingSlash();
#endif
}


//==========================================================================
//
//  VStr::AppendPath
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::AppendPath (const VStr &path) const noexcept {
  if (path.isEmpty()) return VStr(*this);
  if (isEmpty()) return path;
  VStr res(*this);
  if (!IsPathSeparatorCharStrict(path[0]) && !IsPathSeparatorCharStrict(res[res.length()-1])) res += "/";
  res += path;
  return res;
}


//==========================================================================
//
//  VStr::IsAbsolutePath
//
//==========================================================================
VVA_CHECKRESULT bool VStr::IsAbsolutePath () const noexcept {
  if (length() < 1) return false;
  const char *data = getData();
  if (IsPathSeparatorCharStrict(data[0])) return true;
#if defined(_WIN32)
  if (length() > 1 && data[1] == ':') return true;
#endif
  return false;
}


//==========================================================================
//
//  VStr::Utf8Length
//
//==========================================================================
VVA_CHECKRESULT int VStr::Utf8Length (const char *s, int len) noexcept {
  if (len < 0) len = (s && s[0] ? (int)strlen(s) : 0);
  int count = 0;
  while (len--) {
    const vuint8 b = (vuint8)*s++;
    if (!b) break;
    ++count;
    const int btlen = CalcUtf8CharByteSize((char)b);
    if (btlen < 2) continue;
    size_t clen = (size_t)(btlen-1); // extra bytes
    do {
      if (!len || !s[0]) return count;
      if (!IsValidUtf8Continuation(*s)) break;
      --len;
      ++s;
    } while (--clen);
  }
  return count;
}


//==========================================================================
//
//  VStr::ByteLengthForUtf8
//
//==========================================================================
VVA_CHECKRESULT size_t VStr::ByteLengthForUtf8 (const char *s, size_t N) noexcept {
  if (s) {
    size_t count = 0;
    while (N--) {
      const vuint8 b = (vuint8)*s++;
      if (!b) break;
      ++count;
      const int btlen = CalcUtf8CharByteSize((char)b);
      if (btlen < 2) continue;
      size_t clen = (size_t)(btlen-1); // extra bytes
      do {
        if (!N || !s[0]) return count;
        if (!IsValidUtf8Continuation(*s)) break;
        --N;
        ++count;
        ++s;
      } while (--clen);
    }
    return count;
  } else {
    return 0;
  }
}


//==========================================================================
//
//  VStr::Utf8GetChar
//
//==========================================================================
VVA_CHECKRESULT int VStr::Utf8GetChar (const char *&s) noexcept {
  if (!s || !s[0]) return 0;

  const vuint8 b = (vuint8)*s++;
  if (b < 128) return b;

  int cnt = CalcUtf8CharByteSize((char)b);
  if (cnt < 0) return '?'; // invalid utf-8

  int val;
  switch (cnt) {
    case 1: return b;
    case 2: val = b&0x1f; break;
    case 3: val = b&0x0f; break;
    case 4: val = b&0x07; break;
    default: return '?'; // somewhing is very wrong
  }
  --cnt;

  do {
    if (!IsValidUtf8Continuation(*s)) return '?'; // invald UTF-8
    val = (val<<6)|(*s&0x3f);
    ++s;
  } while (--cnt);

  if (val == 0) val = ' '; // just in case

  if (!isValidCodepoint(val)) val = '?';
  return val;
}


//==========================================================================
//
//  VStr::FromUtf8Char
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::FromUtf8Char (int c) noexcept {
  char res[8];
  if (!isValidCodepoint(c)) return VStr("?");
  if (c < 0x80) {
    res[0] = c;
    res[1] = 0;
  } else if (c < 0x800) {
    res[0] = 0xc0|(c&0x1f);
    res[1] = 0x80|((c>>5)&0x3f);
    res[2] = 0;
  } else if (c < 0x10000) {
    res[0] = 0xe0|(c&0x0f);
    res[1] = 0x80|((c>>4)&0x3f);
    res[2] = 0x80|((c>>10)&0x3f);
    res[3] = 0;
  } else {
    res[0] = 0xf0|(c&0x07);
    res[1] = 0x80|((c>>3)&0x3f);
    res[2] = 0x80|((c>>9)&0x3f);
    res[3] = 0x80|((c>>15)&0x3f);
    res[4] = 0;
  }
  return res;
}


//==========================================================================
//
//  VStr::needQuoting
//
//==========================================================================
VVA_CHECKRESULT bool VStr::needQuoting () const noexcept {
  int len = length();
  const char *data = getData();
  for (int f = 0; f < len; ++f) {
    vuint8 ch = (vuint8)data[f];
    if (ch <= ' ' || ch == '\\' || ch == '"' || ch >= 127) return true;
  }
  return false;
}


//==========================================================================
//
//  VStr::quote
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::quote (bool addQCh, bool forceQCh) const noexcept {
  int len = length();
  char hexb[6];
  if (forceQCh) addQCh = true;
  const char *data = getData();
  for (int f = 0; f < len; ++f) {
    vuint8 ch = (vuint8)data[f];
    if (ch <= ' ' || ch == '\\' || ch == '"' || ch >= 127) {
      // need to quote it
      VStr res;
      if (addQCh) res += '"';
      for (int c = 0; c < len; ++c) {
        ch = (vuint8)data[c];
        if (ch < ' ') {
          switch (ch) {
            case '\t': res += "\\t"; break;
            case '\n': res += "\\n"; break;
            case '\r': res += "\\r"; break;
            case 27: res += "\\e"; break;
            case TEXT_COLOR_ESCAPE: res += "\\c"; break;
            default:
              snprintf(hexb, sizeof(hexb), "\\x%02x", ch);
              res += hexb;
              break;
          }
        } else if (ch == '\\' || ch == '"') {
          res += "\\";
          res += (char)ch;
        } else if (ch >= 127) {
          snprintf(hexb, sizeof(hexb), "\\x%02x", ch);
          res += hexb;
        } else {
          res += (char)ch;
        }
      }
      if (addQCh) res += '"';
      return res;
    }
  }
  if (forceQCh) {
    VStr qs("\"");
    qs += *this;
    qs += "\"";
    return qs;
  }
  return VStr(*this);
}


//==========================================================================
//
//  VStr::buf2hex
//
//==========================================================================
VVA_CHECKRESULT VStr VStr::buf2hex (const void *buf, int buflen) noexcept {
  /*static*/ const char *hexd = "0123456789abcdef";
  VStr res;
  if (buflen < 0 || !buf) return res;
  const vuint8 *b = (const vuint8 *)buf;
  buflen *= 2;
  res.resize(buflen);
  char *str = res.dataptr;
  for (int f = 0; f < buflen; f += 2, ++b) {
    *str++ = hexd[((*b)>>4)&0x0f];
    *str++ = hexd[(*b)&0x0f];
  }
  return res;
}


//==========================================================================
//
//  VStr::convertInt
//
//==========================================================================
bool VStr::convertInt (const char *s, int *outv, bool loose) noexcept {
  bool neg = false;
  int dummy = 0;
  if (!outv) outv = &dummy;
  *outv = 0;
  if (!s || !s[0]) return false;
  while (*s && (vuint8)*s <= ' ') ++s;
  if (*s == '+') ++s; else if (*s == '-') { neg = true; ++s; }
  if (!s[0]) return false;
  if (s[0] < '0' || s[0] > '9') return false;
  int base = 0;
  if (s[0] == '0') {
    switch (s[1]) {
      case 'x': case 'X': base = 16; break;
      case 'o': case 'O': base = 8; break;
      case 'b': case 'B': base = 2; break;
      case 'd': case 'D': base = 10; break;
    }
    if (base) {
      if (!s[2] || digitInBase(s[2], base) < 0) return false;
      s += 2;
    }
  }
  if (!base) base = 10;
  while (*s) {
    char ch = *s++;
    if (ch == '_') continue;
    int d = digitInBase(ch, base);
    if (d < 0) { if (neg) *outv = -(*outv); if (!loose) *outv = 0; return false; }
    *outv = (*outv)*base+d;
  }
  if (neg) *outv = -(*outv);
  while (*s && (vuint8)*s <= ' ') ++s;
  if (*s) { if (!loose) *outv = 0; return false; }
  return true;
}


//==========================================================================
//
//  VStr::convertFloat
//
//==========================================================================
bool VStr::convertFloat (const char *s, float *outv, const float *defval) noexcept {
  float dummy = 0;
  if (!outv) outv = &dummy;
  *outv = 0.0f;
  if (!s || !s[0]) { if (defval) *outv = *defval; return false; }
  while (*s && (vuint8)*s <= ' ') ++s;
#ifdef VCORE_STUPID_ATOF
  bool neg = (s[0] == '-');
  if (s[0] == '+' || s[0] == '-') ++s;
  if (!s[0]) { if (defval) *outv = *defval; return false; }
  // int part
  bool wasNum = false;
  if (s[0] >= '0' && s[0] <= '9') {
    wasNum = true;
    while (s[0] >= '0' && s[0] <= '9') *outv = (*outv)*10+(*s++)-'0';
  }
  // fractional part
  if (s[0] == '.') {
    ++s;
    if (s[0] >= '0' && s[0] <= '9') {
      wasNum = true;
      float v = 0, div = 1.0f;
      while (s[0] >= '0' && s[0] <= '9') {
        div *= 10.0f;
        v = v*10+(*s++)-'0';
      }
      *outv += v/div;
    }
  }
  // 'e' part
  if (wasNum && (s[0] == 'e' || s[0] == 'E')) {
    //FIXME: we should do this backwards
    ++s;
    bool negexp = (s[0] == '-');
    if (s[0] == '-' || s[0] == '+') ++s;
    if (s[0] < '0' || s[0] > '9') { if (neg) *outv = -(*outv); if (defval) *outv = *defval; return false; }
    int exp = 0;
    while (s[0] >= '0' && s[0] <= '9') exp = exp*10+(*s++)-'0';
    while (exp != 0) {
      if (negexp) *outv /= 10.0f; else *outv *= 10.0f;
      --exp;
    }
  }
  if (neg) *outv = -(*outv);
  // skip trailing 'f' or 'l', if any
  if (wasNum && (s[0] == 'f' || s[0] == 'l')) ++s;
  // trailing spaces
  while (*s && (vuint8)*s <= ' ') ++s;
  if (*s || !wasNum) {
    if (defval) *outv = *defval; //else *outv = 0;
    return false;
  }
  return true;
/* VCORE_STUPID_ATOF */
#elif defined(VCORE_USE_STRTODEX)
  if (!strtofEx(outv, s)) {
    if (defval) *outv = *defval;
    return false;
  }
  return true;
#else
  if (!s[0]) { if (defval) *outv = *defval; return false; }
#ifdef VCORE_USE_LOCALE
  static int dpconvinited = -1; // >0: need conversion
  static char *dpointstr = nullptr; // this actually `const`
  if (dpconvinited < 0) {
    const char *dcs = (const char *)localeconv()->decimal_point;
    if (!dcs || !dcs[0] || strcmp(dcs, ".") == 0) {
      dpconvinited = 0;
      /*
      dpointstr = (char *)Z_MallocNoClear(2);
      dpointstr[0] = '.';
      dpointstr[1] = 0;
      */
    } else {
      dpconvinited = 1;
      dpointstr = (char *)Z_MallocNoClear(strlen(dcs)+1);
      memcpy(dpointstr, dcs, strlen(dcs)+1);
      //fprintf(stderr, "*** <%s> ***\n", dpointstr);
    }
  }
  // if locale has some different separator, do conversion
  if (dpconvinited > 0) {
    int newlen = 0;
    bool hasDPoint = false;
    for (const char *tmp = s; *tmp; ++tmp) {
      if (*tmp == '.') {
        newlen += (int)strlen(dpointstr);
        hasDPoint = true;
      } else {
        ++newlen;
      }
    }
    if (hasDPoint) {
      char *newstr = (char *)Z_MallocNoClear(newlen+8);
      char *dest = newstr;
      for (const char *tmp = s; *tmp; ++tmp) {
        if (*tmp == '.') {
          strcpy(dest, dpointstr);
          dest += strlen(dpointstr);
        } else {
          *dest++ = *tmp;
        }
      }
      *dest = 0;
      // do conversion
      char *end = nullptr;
      float res = strtof(newstr, &end);
      if (!isFiniteF(res)) { Z_Free(newstr); if (defval) *outv = *defval; return false; } // oops
      while (*end && *(unsigned char *)end <= ' ') ++end; // skip trailing spaces
      if (*end) { Z_Free(newstr); if (defval) *outv = *defval; return false; } // oops
      Z_Free(newstr);
      *outv = res;
      return true;
    }
  }
#endif /* VCORE_USE_LOCALE */
  // no locale, cannot convert (or no need to convert anything)
  {
    char *end = nullptr;
    float res = strtof(s, &end);
    if (!isFiniteF(res)) { if (defval) *outv = *defval; return false; } // oops
    while (*end && *(unsigned char *)end <= ' ') ++end; // skip trailing spaces
    if (*end) { if (defval) *outv = *defval; return false; } // oops
    *outv = res;
    return true;
  }
#endif /* VCORE_STUPID_ATOF */
}


//==========================================================================
//
//  VStr::globmatch
//
//==========================================================================
VVA_CHECKRESULT bool VStr::globmatch (const char *str, const char *pat, bool caseSensitive) noexcept {
  bool star = false;
  if (!pat) pat = "";
  if (!str) str = "";
loopStart:
  size_t patpos = 0;
  for (size_t i = 0; str[i]; ++i) {
    char sch = str[i];
    if (!caseSensitive) sch = upcase1251(sch);
    //if (patpos == pat.length) goto starCheck;
    switch (pat[patpos++]) {
      case 0: goto starCheck;
      case '\\':
        //if (patpos == pat.length) return false; // malformed pattern
        if (!pat[patpos]) return false; // malformed pattern
        ++patpos; /* skip screened char */
        goto defcheck;
      case '?': // match anything //except '.'
        //if (sch == '.') goto starCheck;
        break;
      case '*':
        star = true;
        //str = str[i..$];
        str += i;
        //pat = pat[patpos..$];
        pat += patpos;
        //while (pat.length && pat.ptr[0] == '*') pat = pat[1..$];
        while (*pat == '*') ++pat;
        //if (pat.length == 0) return true;
        if (*pat == 0) return true;
        goto loopStart;
      case '[':
        {
          int hasMatch = 0;
          int inverted = 0;
          if (pat[patpos] == '^') { inverted = 1; ++patpos; }
          while (pat[patpos] && pat[patpos] != ']') {
            char c0 = pat[patpos++];
            if (!caseSensitive) c0 = upcase1251(c0);
            char c1;
            if (pat[patpos] == '-') {
              // char range
              ++patpos; // skip '-'
              c1 = pat[patpos++];
              if (!c1) {
                --patpos;
                c1 = c0;
              }
              if (!caseSensitive) c1 = upcase1251(c1);
            } else {
              c1 = c0;
            }
            if (!hasMatch && (vuint8)sch >= (vuint8)c0 && (vuint8)sch <= (vuint8)c1) hasMatch = 1;
          }
          if (pat[patpos] == ']') ++patpos;
          if (inverted) hasMatch = !hasMatch;
          if (!hasMatch) goto starCheck;
          break;
        }
      default:
      defcheck:
        if (caseSensitive) {
          if (sch != pat[patpos-1]) goto starCheck;
        } else {
          if (sch != upcase1251(pat[patpos-1])) goto starCheck;
        }
        break;
    }
  }
  //pat = pat[patpos..$];
  pat += patpos;
  //while (pat.length && pat.ptr[0] == '*') pat = pat[1..$];
  while (*pat == '*') ++pat;
  //return (pat.length == 0);
  return (*pat == 0);

starCheck:
   if (!star) return false;
   //if (str.length) str = str[1..$];
   if (*str) ++str;
   goto loopStart;
}


//==========================================================================
//
//  VStr::Tokenise
//
//==========================================================================
void VStr::Tokenise (TArray <VStr> &args) const noexcept {
  if (isEmpty()) return;
  const char *str = getCStr();
  while (*str) {
    // whitespace
    const char ch = *str++;
    if ((vuint8)ch <= ' ') continue;
    // string?
    if (ch == '"' || ch == '\'') {
      VStr ss;
      const char qch = ch;
      const char *sstart = nullptr;
      while (*str && *str != qch) {
        if (str[0] == '\\' && str[1]) {
          if (sstart) {
            ss.appendCStr(sstart, (int)(ptrdiff_t)(str-sstart));
            sstart = nullptr;
          }
          ++str; // skip backslash
          switch (*str++) {
            case 't': ss += '\t'; break;
            case 'n': ss += '\n'; break;
            case 'r': ss += '\r'; break;
            case 'e': ss += '\x1b'; break;
            case 'c': ss += TEXT_COLOR_ESCAPE; break;
            case '\\': case '"': case '\'': ss += str[-1]; break;
            case 'x': case 'X':
              {
                int cc = 0;
                int d = (*str ? VStr::digitInBase(*str, 16) : -1);
                if (d >= 0) {
                  cc = d;
                  ++str;
                  d = (*str ? VStr::digitInBase(*str, 16) : -1);
                  if (d >= 0) { cc = cc*16+d; ++str; }
                  //if (!cc) cc = ' '; // just in case
                  ss += (char)cc;
                } else {
                  ss += str[-1];
                }
              } break;
            default: // ignore other quotes
              ss += str[-1];
              break;
          }
        } else {
          if (!sstart) sstart = str;
          ++str;
        }
      }
      if (sstart) ss.appendCStr(sstart, (int)(ptrdiff_t)(str-sstart));
      if (*str) { vassert(*str == qch); ++str; } // skip closing quote
      args.append(ss);
    } else {
      // simple arg
      --str;
      const char *end = str;
      while ((vuint8)*end > ' ') ++end;
      args.append(VStr(str, (int)(ptrdiff_t)(end-str)));
      str = end;
    }
  }
}


//==========================================================================
//
//  VStr::findNextCommand
//
//  this finds start of the next command
//  commands are ';'-delimited
//  processes quotes as `tokenise` does
//  skips all leading spaces by default
//  (i.e. result can be >0 even if there are no ';')
//
//==========================================================================
int VStr::findNextCommand (int stpos, bool skipLeadingSpaces) const noexcept {
  if (stpos < 0) stpos = 0;
  const int len = length();
  if (stpos >= len) return len;
  const char *str = getCStr();
  char qch = 0; // quote end char
  if (skipLeadingSpaces) { while (stpos < len && (vuint8)str[stpos] <= ' ') ++stpos; }
  vassert(stpos <= len);
  int lastGoodPos = stpos;
  while (stpos < len) {
    const char ch = str[stpos++];
    if (qch) {
      // in a string
           if (ch == qch) qch = 0;
      else if (ch == '\\') ++stpos;
    } else if (ch == ';') {
      // found command end, skip spaces
      if (skipLeadingSpaces) { while (stpos < len && (vuint8)str[stpos] <= ' ') ++stpos; }
      lastGoodPos = stpos;
    } else if (ch == '"' || ch == '\'') qch = ch;
  }
  vassert(lastGoodPos <= len);
  return lastGoodPos;
}


//==========================================================================
//
//  VStr::isSafeDiskFileName
//
//==========================================================================
VVA_CHECKRESULT bool VStr::isSafeDiskFileName () const noexcept {
  if (length() == 0) return false;
  const char *s = getCStr();
  if (IsPathSeparatorChar(s[0])) return false;
  for (; *s; ++s) {
    #if defined(_WIN32)
    if (*s == ':') return false;
    #endif
    if (IsPathSeparatorCharStrict(*s)) {
      // don't allow any dot-starting pathes
      if (s[1] == '.' || !s[1]) return false;
    }
  }
  return true;
}


//==========================================================================
//
//  VStr::ApproxMatch
//
//  this is the actual string matching algorithm.
//  it returns a value from zero to one indicating an approximate
//  percentage of how closely two strings match.
//
//  pass `-1` as any length to use `strlen()`.
//
//  based on the code from Game Programmings Gem Vol.6, by James Boer
//
//==========================================================================
float VStr::ApproxMatch (const char *left, int leftlen, const char *right, int rightlen) noexcept {
  const float CAP_MISMATCH_VAL = 0.9f;
  if (!left) left = "";
  if (!right) right = "";
  // get the length of the left, right, and longest string
  // (to use as a basis for comparison in value calculateions)
  const size_t leftSize = (leftlen < 0 ? strlen(left) : (size_t)leftlen);
  const size_t rightSize = (rightlen < 0 ? strlen(right) : (size_t)rightlen);
  const size_t largerSize = (leftSize > rightSize ? leftSize : rightSize);
  const char *leftPtr = left;
  const char *rightPtr = right;
  float matchVal = 0.0f;
  // iterate through the left string until we run out of room
  while (leftPtr != left+leftSize && rightPtr != right+rightSize) {
    // first, check for a simple left/right match
    if (*leftPtr == *rightPtr) {
      // if it matches, add a proportional character's value to the match total
      matchVal += 1.0f/largerSize;
      // advance both pointers
      ++leftPtr;
      ++rightPtr;
    }
    // if the simple match fails, try a match ignoring capitalization
    else if (locase1251(*leftPtr) == locase1251(*rightPtr)) {
      // we'll consider a capitalization mismatch worth 90% of a normal match
      matchVal += CAP_MISMATCH_VAL/largerSize;
      // advance both pointers
      ++leftPtr;
      ++rightPtr;
    } else {
      const char *lpbest = left+leftSize;
      const char *rpbest = right+rightSize;
      int totalCount = 0;
      int bestCount = INT_MAX;
      int leftCount = 0;
      int rightCount = 0;
      // here we loop through the left word in an outer loop,
      // but we also check for an early out by ensuring we
      // don't exceed our best current count
      for (const char *lp = leftPtr; lp != left+leftSize && leftCount+rightCount < bestCount; ++lp) {
        // inner loop counting
        for (const char *rp = rightPtr; rp != right+rightSize && leftCount+rightCount < bestCount; ++rp) {
          // at this point, we don't care about case
          if (locase1251(*lp) == locase1251(*rp)) {
            // this is the fitness measurement
            totalCount = leftCount+rightCount;
            if (totalCount < bestCount) {
              bestCount = totalCount;
              lpbest = lp;
              rpbest = rp;
            }
          }
          ++rightCount;
        }
        ++leftCount;
        rightCount = 0;
      }
      leftPtr = lpbest;
      rightPtr = rpbest;
    }
  }
  // clamp the value in case of floating-point error
       if (matchVal > 0.99f) matchVal = 1.0f;
  else if (matchVal < 0.01f) matchVal = 0.0f;
  return matchVal;
}


// ////////////////////////////////////////////////////////////////////////// //
VStr_ByCharIterator VStr::begin () noexcept { return VStr_ByCharIterator(*this); }
const VStr_ByCharIterator VStr::begin () const noexcept { return VStr_ByCharIterator(*this); }
VStr_ByCharIterator VStr::end () noexcept { return VStr_ByCharIterator(*this, true); }
const VStr_ByCharIterator VStr::end () const noexcept { return VStr_ByCharIterator(*this, true); }



// ////////////////////////////////////////////////////////////////////////// //
//WARNING! MUST BE POWER OF 2!
enum { VA_BUFFER_COUNT = 64u };

struct VaBuffer {
  char *buf;
  size_t bufsize;
  bool alloced;
  char initbuf[1024];

  VaBuffer () noexcept : buf(initbuf), bufsize(sizeof(initbuf)), alloced(false) {}
  ~VaBuffer () noexcept { if (alloced) Z_Free(buf); buf = nullptr; alloced = false; bufsize = 0; }

  void ensureSize (size_t size) noexcept {
    size = ((size+1)|0x1fff)+1;
    if (size <= bufsize) return;
    if (size > 1024*1024*2) Sys_Error("`va` buffer too big");
    char *newbuf = (char *)(alloced ? Z_ReallocNoFail(buf, size) : Z_MallocNoClearNoFail(size));
    if (!newbuf) Sys_Error("out of memory for `va` buffer");
    buf = newbuf;
    bufsize = size;
    alloced = true;
    buf[0] = 0; // why not?
  }
};


// use spinlock instead
static /*thread_local*/ VaBuffer vabufs[VA_BUFFER_COUNT];
static /*thread_local*/ unsigned vabufcurr = 0u;
static uint32_t vabuf_locked = 0u;


//==========================================================================
//
//  vavarg
//
//  not fully thread-safe, but i don't care
//
//==========================================================================
VVA_CHECKRESULT char *vavarg (const char *text, va_list ap) noexcept {
  // spinlock
  while (__sync_val_compare_and_swap(&vabuf_locked, 0u, 1u) == 0) {}
  const unsigned bufnum = vabufcurr;
  vabufcurr = (vabufcurr+1u)%VA_BUFFER_COUNT;
  VaBuffer &vbuf = vabufs[bufnum];
  va_list apcopy;
  va_copy(apcopy, ap);
  int size = vsnprintf(vbuf.buf, vbuf.bufsize, text, apcopy);
  va_end(apcopy);
  if (size >= 0 && size >= (int)vbuf.bufsize-1) {
    vabufs[bufnum].ensureSize((size_t)size);
    va_copy(apcopy, ap);
    vsnprintf(vbuf.buf, vbuf.bufsize, text, apcopy);
    va_end(apcopy);
  }
  __atomic_store_n(&vabuf_locked, 0u, __ATOMIC_SEQ_CST);
  return vbuf.buf;
}


//==========================================================================
//
//  va
//
//  Very useful function from Quake.
//  Does a varargs printf into a temp buffer, so I don't need to have
//  varargs versions of all text functions.
//
//==========================================================================
__attribute__((format(printf, 1, 2))) VVA_CHECKRESULT char *va (const char *text, ...) noexcept {
  va_list ap;
  va_start(ap, text);
  char *res = vavarg(text, ap);
  va_end(ap);
  return res;
}


#define COMATOZE_BUF_SIZE   (256)
#define COMATOZE_BUF_COUNT  (32)
static char comatozebufs[COMATOZE_BUF_SIZE][COMATOZE_BUF_COUNT];
static unsigned comatozebufidx = 0;


//==========================================================================
//
//  comatoze
//
//==========================================================================
const char *comatoze (vuint32 n, const char *sfx) noexcept {
  char *buf = comatozebufs[comatozebufidx++];
  if (comatozebufidx == COMATOZE_BUF_COUNT) comatozebufidx = 0;
  int bpos = (int)COMATOZE_BUF_SIZE;
  buf[--bpos] = 0;
  if (sfx) {
    size_t slen = strlen(sfx);
    while (slen--) {
      if (bpos > 0) buf[--bpos] = sfx[slen];
    }
  }
  int xcount = 0;
  do {
    if (xcount == 3) { if (bpos > 0) buf[--bpos] = ','; xcount = 0; }
    if (bpos > 0) buf[--bpos] = '0'+n%10;
    ++xcount;
  } while ((n /= 10) != 0);
  return &buf[bpos];
}


//==========================================================================
//
//  secs2msecs
//
//==========================================================================
int secs2msecs (const double secs) noexcept {
  const int msecs = (secs > 0.0 ? (int)(secs*1000.0+0.5) : 1);
  return msecs+!msecs;
}


//==========================================================================
//
//  secs2timestr
//
//==========================================================================
const char *secs2timestr (const double secs) noexcept {
  const int msecs = secs2msecs(secs);
  const char *s;
  if (msecs < 100) {
    s = comatoze(msecs, (msecs > 1 ? " msecs" : " msec"));
  } else {
    // seconds
    //const int scs = msecs/1000+(msecs%1000 >= 500);
    //s = comatoze(scs, (scs > 1 ? " secs" : " sec"));
    const int scs = msecs/1000;//+(msecs%1000 >= 500);
    const int hsc = (msecs/100)%10+((msecs/10)%10 >= 5);
    char xbuf[16];
    snprintf(xbuf, sizeof(xbuf), ".%d sec%s", hsc, (hsc == 0 ? "" : "s"));
    s = comatoze(scs, xbuf);
  }
  return s;
}
