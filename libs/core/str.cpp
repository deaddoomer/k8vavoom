//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************
//
//  Dynamic string class.
//
//**************************************************************************

//#include <cctype>

#include "core.h"

#if !defined _WIN32 && !defined DJGPP
#undef stricmp  //  Allegro defines them
#undef strnicmp
#define stricmp   strcasecmp
#define strnicmp  strncasecmp
#endif


// ////////////////////////////////////////////////////////////////////////// //
const VStr VStr::EmptyString = VStr();


// ////////////////////////////////////////////////////////////////////////// //
static char va_buffer[16][65536];
static int va_bufnum = 0;


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

char VStr::wc2shitmap[65536];


class VStrInternalInitializer {
public:
  VStrInternalInitializer (int n) {
    memset(VStr::wc2shitmap, '?', sizeof(VStr::wc2shitmap));
    for (int f = 0; f < 128; ++f) VStr::wc2shitmap[f] = (char)f;
    for (int f = 0; f < 128; ++f) VStr::wc2shitmap[VStr::cp1251[f]] = (char)(f+128);
  }
};

static VStrInternalInitializer vstrInitr(666);


// ////////////////////////////////////////////////////////////////////////// //
// fast state-machine based UTF-8 decoder; using 8 bytes of memory
// code points from invalid range will never be valid, this is the property of the state machine
// see http://bjoern.hoehrmann.de/utf-8/decoder/dfa/
// [0..$16c-1]
// maps bytes to character classes
const vuint8 VUtf8DecoderFast::utf8dfa[0x16c] = {
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
VStr &VStr::utf8Append (vuint32 code) {
  if (code < 0 || code > 0x10FFFF) return operator+=('?');
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


bool VStr::isUtf8Valid () const {
  int slen = length();
  if (slen < 1) return true;
  int pos = 0;
  while (pos < slen) {
    int len = utf8CodeLen(data[pos]);
    if (len < 1) return false; // invalid sequence start
    if (pos+len-1 > slen) return false; // out of chars in string
    --len;
    ++pos;
    // check other sequence bytes
    while (len > 0) {
      if ((data[pos]&0xC0) != 0x80) return false;
      --len;
      ++pos;
    }
  }
  return true;
}


// ////////////////////////////////////////////////////////////////////////// //
VStr VStr::toLowerCase1251 () const {
  int slen = length();
  if (slen < 1) return VStr();
  for (int f = 0; f < slen; ++f) {
    if (locase1251(data[f]) != data[f]) {
      VStr res;
      res.Resize(slen);
      for (int c = 0; c < slen; ++c) res.data[c] = locase1251(data[c]);
      return res;
    }
  }
  return VStr(this);
}


VStr VStr::toUpperCase1251 () const {
  int slen = length();
  if (slen < 1) return VStr();
  for (int f = 0; f < slen; ++f) {
    if (upcase1251(data[f]) != data[f]) {
      VStr res;
      res.Resize(slen);
      for (int c = 0; c < slen; ++c) res.data[c] = upcase1251(data[c]);
      return res;
    }
  }
  return VStr(this);
}


// ////////////////////////////////////////////////////////////////////////// //
VStr VStr::utf2win () const {
  VStr res;
  if (length()) {
    VUtf8DecoderFast dc;
    for (size_t f = 0; f < length(); ++f) {
      if (dc.put(data[f])) res += wchar2win(dc.codepoint);
    }
  }
  return res;
}


VStr VStr::win2utf () const {
  VStr res;
  if (length()) {
    for (size_t f = 0; f < length(); ++f) {
      vuint8 ch = (vuint8)data[f];
      if (ch > 127) res.utf8Append(cp1251[ch-128]); else res += (char)ch;
    }
  }
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
bool VStr::fnameEqu1251CI (const char *s) const {
  size_t slen = length();
  if (!s || !s[0]) return (slen == 0);
  size_t pos = 0;
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


// ////////////////////////////////////////////////////////////////////////// //
VStr VStr::mid (int start, int len) const {
  guard(VStr::mid);
  int mylen = (int)length();
  if (mylen == 0) return VStr();
  if (len <= 0 || start >= mylen) return VStr();
  if (start < 0) {
    if (start+len <= 0) return VStr();
    len += start;
    start = 0;
  }
  if (start+len > mylen) {
    if ((len = mylen-start) < 1) return VStr();
  }
  if (start == 0 && len == mylen) return VStr(*this);
  return VStr(data+start, len);
  unguard;
}


void VStr::MakeMutable () {
  guard(VStr::MakeMutable);
  if (!data || *refp() == 1) return; // nothing to do
  // allocate new string
  size_t newsize = (size_t)*lenp()+sizeof(int)*2+1;
  char *newdata = (char *)malloc(newsize);
  if (!newdata) Sys_Error("Out of memory");
  memcpy(newdata, data-sizeof(int)*2, newsize);
  --(*refp()); // decrement old refcounter
  data = newdata+sizeof(int)*2; // skip bookkeeping data
  *refp() = 1; // set new refcounter
  unguard;
}


void VStr::Resize (int newlen) {
  guard(VStr::Resize);

  //check(newlen >= 0);

  if (newlen <= 0) {
    // free string
    if (data) {
      if (--(*refp()) == 0) free(data-sizeof(int)*2);
      data = nullptr;
    }
    return;
  }

  int oldlen = (int)length();

  if (newlen == oldlen) {
    // same length, make string unique (just in case)
    if (data && *refp() != 1) MakeMutable();
    return;
  }

  // get new (or resize old) memory buffer
  size_t newsize = newlen+sizeof(int)*2+1;

  // if this string is unique, use `realloc()`, otherwise allocate new buffer
  if (data && *refp() == 1) {
    // realloc
    char *newdata = (char *)realloc(data-sizeof(int)*2, newsize);
    if (!newdata) Sys_Error("Out of memory");
    data = newdata+sizeof(int)*2;
  } else {
    // new buffer
    char *newdata = (char *)malloc(newsize);
    if (!newdata) Sys_Error("Out of memory");
    char *newstrdata = newdata+sizeof(int)*2;

    // copy old contents, if any
    if (data) {
      // has old content, copy it
      if (newlen < oldlen) {
        // new string is smaller
        memcpy(newstrdata, data, newlen);
      } else {
        // new string is bigger
        memcpy(newstrdata, data, oldlen+1); // take zero too; it should be overwritten by the caller later
      }
      // decref old string
      if (--(*refp()) == 0) {
        // this is something that should not happen, fail hard
        *(int *)0 = 0;
        //free(data-sizeof(int)*2);
      }
    } else {
      // no old content
      newstrdata[0] = 0; // to be on a safe side
    }

    // setup new pointer and bookkeeping
    data = newstrdata;
    *refp() = 1;
  }

  *lenp() = newlen; // set new length
  data[newlen] = 0; // set trailing zero, this is required by `SetContent()`

  unguard;
}


void VStr::SetContent (const char *s, int len) {
  guard(VStr::SetContent);
  if (s && s[0] && len != 0) {
    if (len < 0) len = (s ? (int)strlen(s) : 0);
    if (len) {
      // check for pathological case: is `s` inside our data?
      if (s == data && (size_t)len == length()) {
        // this is prolly `VStr(*otherstr)` case, so just increment refcount
        ++(*refp());
      } else if (isMyData(s, len)) {
        // make temporary copy
        char tbuf[128];
        if ((size_t)len <= sizeof(tbuf)) {
          memcpy(tbuf, s, len);
          Resize(len);
          memcpy(data, tbuf, len);
        } else {
          char *temp = (char *)malloc(len);
          if (!temp) Sys_Error("Out of memory");
          memcpy(temp, s, len);
          Resize(len);
          memcpy(data, temp, len);
          free(temp);
        }
      } else {
        Resize(len);
        memcpy(data, s, len);
      }
    } else {
      Clear();
    }
  } else {
    Clear();
  }
  unguard;
}


bool VStr::StartsWith (const char* s) const {
  guard(VStr::StartsWith);
  if (!s || !s[0]) return false;
  size_t l = Length(s);
  if (l > length()) return false;
  return (NCmp(data, s, l) == 0);
  unguard;
}


bool VStr::StartsWith (const VStr &s) const {
  guard(VStr::StartsWith);
  size_t l = s.length();
  if (l > length()) return false;
  return (NCmp(data, *s, l) == 0);
  unguard;
}


bool VStr::EndsWith (const char* s) const {
  guard(VStr::EndsWith);
  if (!s || !s[0]) return false;
  size_t l = Length(s);
  if (l > length()) return false;
  return (NCmp(data+length()-l, s, l) == 0);
  unguard;
}


bool VStr::EndsWith (const VStr &s) const {
  guard(VStr::EndsWith);
  size_t l = s.length();
  if (l > length()) return false;
  return (NCmp(data+length()-l, *s, l) == 0);
  unguard;
}


VStr VStr::ToLower () const {
  guard(VStr::ToLower);
  if (!data) return VStr();
  bool hasWork = false;
  int l = length();
  for (int i = 0; i < l; ++i) if (data[i] >= 'A' && data[i] <= 'Z') { hasWork = true; break; }
  if (hasWork) {
    VStr res(*this);
    res.MakeMutable();
    for (int i = 0; i < l; ++i) if (res.data[i] >= 'A' && res.data[i] <= 'Z') res.data[i] += 32; // poor man's tolower()
    return res;
  } else {
    return VStr(*this);
  }
  unguard;
}


VStr VStr::ToUpper () const {
  guard(VStr::ToUpper);
  if (!data) return VStr();
  bool hasWork = false;
  int l = length();
  for (int i = 0; i < l; ++i) if (data[i] >= 'a' && data[i] <= 'z') { hasWork = true; break; }
  if (hasWork) {
    VStr res(*this);
    res.MakeMutable();
    for (int i = 0; i < l; ++i) if (res.data[i] >= 'a' && res.data[i] <= 'z') res.data[i] -= 32; // poor man's toupper()
    return res;
  } else {
    return VStr(*this);
  }
  unguard;
}


int VStr::IndexOf (char c) const {
  guard(VStr::IndexOf);
  if (data) {
    const char *pos = (const char *)memchr(data, c, *lenp());
    return (pos ? (int)(pos-data) : -1);
  } else {
    return -1;
  }
  unguard;
}


int VStr::IndexOf (const char *s) const {
  guard(VStr::IndexOf);
  if (!s || !s[0]) return -1;
  int sl = int(Length(s));
  int l = int(length());
  for (int i = 0; i <= l-sl; ++i) if (NCmp(data+i, s, sl) == 0) return i;
  return -1;
  unguard;
}


int VStr::IndexOf (const VStr &s) const {
  guard(VStr::IndexOf);
  int sl = int(s.length());
  if (!sl) return -1;
  int l = int(length());
  for (int i = 0; i <= l-sl; ++i) if (NCmp(data+i, *s, sl) == 0) return i;
  return -1;
  unguard;
}


int VStr::LastIndexOf (char c) const {
  guard(VStr::LastIndexOf);
  if (data) {
    const char *pos = (const char *)memrchr(data, c, *lenp());
    return (pos ? (int)(pos-data) : -1);
  } else {
    return -1;
  }
  unguard;
}


int VStr::LastIndexOf (const char* s) const {
  guard(VStr::LastIndexOf);
  if (!s || !s[0]) return -1;
  int sl = int(Length(s));
  int l = int(length());
  for (int i = l-sl; i >= 0; --i) if (NCmp(data+i, s, sl) == 0) return i;
  return -1;
  unguard;
}


int VStr::LastIndexOf (const VStr &s) const {
  guard(VStr::LastIndexOf);
  int sl = int(s.length());
  if (!sl) return -1;
  int l = int(length());
  for (int i = l-sl; i >= 0; --i) if (NCmp(data + i, *s, sl) == 0) return i;
  return -1;
  unguard;
}


VStr VStr::Replace (const char *Search, const char *Replacement) const {
  guard(VStr::Replace);
  if (length() == 0) return VStr(); // nothing to replace in an empty string

  size_t SLen = Length(Search);
  size_t RLen = Length(Replacement);
  if (!SLen) return VStr(*this); // nothing to search for

  VStr res = VStr(*this);
  size_t i = 0;
  while (i <= res.length()-SLen) {
    if (NCmp(res.data+i, Search, SLen) == 0) {
      // if search and replace strings are of the same size,
      // we can just copy the data and avoid memory allocations
      if (SLen == RLen) {
        res.MakeMutable();
        memcpy(res.data+i, Replacement, RLen);
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
  unguard;
}


VStr VStr::Replace (const VStr &Search, const VStr &Replacement) const {
  guard(VStr::Replace);
  if (length() == 0) return VStr(); // nothing to replace in an empty string

  size_t SLen = Search.length();
  size_t RLen = Replacement.length();
  if (!SLen) return VStr(*this); // nothing to search for

  VStr res(*this);
  size_t i = 0;
  while (i <= res.length()-SLen) {
    if (NCmp(res.data+i, *Search, SLen) == 0) {
      // if search and replace strings are of the same size,
      // we can just copy the data and avoid memory allocations
      if (SLen == RLen) {
        res.MakeMutable();
        memcpy(res.data+i, *Replacement, RLen);
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
  unguard;
}


VStr VStr::Utf8Substring (int start, int len) const {
  check(start >= 0);
  check(start <= (int)Utf8Length());
  check(len >= 0);
  check(start+len <= (int)Utf8Length());
  if (!len) return VStr();
  int RealStart = int(ByteLengthForUtf8(data, start));
  int RealLen = int(ByteLengthForUtf8(data, start+len)-RealStart);
  return VStr(*this, RealStart, RealLen);
}


void VStr::Split (char c, TArray<VStr>& A) const {
  guard(VStr::Split);

  A.Clear();
  if (!data) return;

  int start = 0;
  int len = int(length());
  for (int i = 0; i <= len; ++i) {
    if (i == len || data[i] == c) {
      if (start != i) A.Append(VStr(*this, start, i-start));
      start = i+1;
    }
  }

  unguard;
}


void VStr::Split (const char *chars, TArray<VStr>& A) const {
  guard(VStr::Split);

  A.Clear();
  if (!data) return;

  int start = 0;
  int len = int(length());
  for (int i = 0; i <= len; ++i) {
    bool DoSplit = (i == len);
    for (const char* pChar = chars; !DoSplit && *pChar; ++pChar) DoSplit = (data[i] == *pChar);
    if (DoSplit) {
      if (start != i) A.Append(VStr(*this, start, i-start));
      start = i+1;
    }
  }

  unguard;
}


// split string to path components; first component can be '/', others has no slashes
void VStr::SplitPath (TArray<VStr>& arr) const {
  guard(VStr::SplitPath);

  arr.Clear();
  if (!data) return;

  int pos = 0;
  int len = *lenp();

#if !defined(_WIN32)
  if (data[0] == '/') arr.Append(VStr("/"));
  while (pos < len) {
    if (data[pos] == '/') { ++pos; continue; }
    int epos = pos+1;
    while (epos < len && data[epos] != '/') ++epos;
    arr.Append(VStr(*this, pos, epos-pos));
    pos = epos+1;
  }
#else
  if (data[0] == '/' || data[0] == '\\') {
    arr.Append(VStr("/"));
  } else if (data[1] == ':' && (data[2] == '/' || data[2] == '\\')) {
    arr.Append(VStr(*this, 0, 3));
    pos = 3;
  }
  while (pos < len) {
    if (data[pos] == '/' || data[pos] == '\\') { ++pos; continue; }
    int epos = pos+1;
    while (epos < len && data[epos] != '/' && data[epos] != '\\') ++epos;
    arr.Append(VStr(*this, pos, epos-pos));
    pos = epos+1;
  }
#endif

  unguard;
}


bool VStr::IsValidUtf8 () const {
  guard(VStr::IsValidUtf8);
  if (!data) return true;
  for (const char* c = data; *c;) {
    if ((*c&0x80) == 0) {
      ++c;
    } else if ((*c&0xe0) == 0xc0) {
      if ((c[1]&0xc0) != 0x80) return false;
      c += 2;
    } else if ((*c&0xf0) == 0xe0) {
      if ((c[1]&0xc0) != 0x80 || (c[2]&0xc0) != 0x80) return false;
      c += 3;
    } else if ((*c&0xf8) == 0xf0) {
      if ((c[1]&0xc0) != 0x80 || (c[2]&0xc0) != 0x80 || (c[3]&0xc0) != 0x80) return false;
      c += 4;
    } else {
      return false;
    }
  }
  return true;
  unguard;
}


VStr VStr::Latin1ToUtf8 () const {
  guard(VStr::Latin1ToUtf8);
  VStr res;
  for (size_t i = 0; i < length(); ++i) res += FromChar((vuint8)data[i]);
  return res;
  unguard;
}


VStr VStr::EvalEscapeSequences () const {
  guard(VStr::EvalEscapeSequences);
  VStr res;
  char val;
  for (const char* c = data; *c; ++c) {
    if (*c == '\\') {
      ++c;
      switch (*c) {
        case 't': res += '\t'; break;
        case 'n': res += '\n'; break;
        case 'r': res += '\r'; break;
        case 'c': res += TEXT_COLOUR_ESCAPE; break;
        case 'x':
          val = 0;
          ++c;
          for (int i = 0; i < 2; ++i) {
                 if (*c >= '0' && *c <= '9') val = (val<<4)+*c-'0';
            else if (*c >= 'a' && *c <= 'f') val = (val<<4)+10+*c-'a';
            else if (*c >= 'A' && *c <= 'F') val = (val<<4)+10+*c-'A';
            else break;
            ++c;
          }
          --c;
          res += val;
          break;
        case '0': case '1': case '2': case '3':
        case '4': case '5': case '6': case '7':
          val = 0;
          for (int i = 0; i < 3; ++i) {
            if (*c >= '0' && *c <= '7') val = (val<<3)+*c-'0'; else break;
            ++c;
          }
          --c;
          res += val;
          break;
        case '\n':
          break;
        case 0:
          --c;
          break;
        default:
          res += *c;
          break;
      }
    } else {
      res += *c;
    }
  }
  return res;
  unguard;
}


VStr VStr::RemoveColours () const {
  guard(VStr::RemoveColours);
  if (!data) return VStr();
  const int oldlen = (int)length();
  // calculate new length
  int newlen = 0;
  int pos = 0;
  while (pos < oldlen) {
    char c = data[pos++];
    if (c == TEXT_COLOUR_ESCAPE) {
      if (data[pos] == '[') {
        ++pos;
        while (pos < oldlen && data[pos] != ']') ++pos;
        ++pos;
      }
    } else {
      if (!c) break;
      ++newlen;
    }
  }
  if (newlen == oldlen) return VStr(*this);
  // build new string
  VStr res;
  res.Resize(newlen);
  pos = 0;
  newlen = 0;
  while (pos < oldlen) {
    char c = data[pos++];
    if (c == TEXT_COLOUR_ESCAPE) {
      if (data[pos] == '[') {
        ++pos;
        while (pos < oldlen && data[pos] != ']') ++pos;
        ++pos;
      }
    } else {
      if (!c) break;
      if ((size_t)newlen >= res.length()) break; // oops
      res.data[newlen++] = c;
    }
  }
  return res;
  unguard;
}


VStr VStr::ExtractFilePath () const {
  guard(FL_ExtractFilePath);
  const char *src = data+length();
#if !defined(_WIN32)
  while (src != data && src[-1] != '/') --src;
#else
  while (src != data && src[-1] != '/' && src[-1] != '\\') --src;
#endif
  return VStr(*this, 0, src-data);
  unguard;
}


VStr VStr::ExtractFileName() const {
  guard(VStr:ExtractFileName);
  const char *src = data+length();
#if !defined(_WIN32)
  while (src != data && src[-1] != '/') --src;
#else
  while (src != data && src[-1] != '/' && src[-1] != '\\') --src;
#endif
  return VStr(src);
  unguard;
}


VStr VStr::ExtractFileBase () const {
  guard(VStr::ExtractFileBase);
  int i = int(length());

  if (i == 0) return VStr();

#if !defined(_WIN32)
  // back up until a \ or the start
  while (i && data[i-1] != '/') --i;
#else
  while (i && data[i-1] != '/' && data[i-1] != '\\') --i;
#endif

  // copy up to eight characters
  int start = i;
  int length = 0;
  while (data[i] && data[i] != '.') {
    if (++length == 9) Sys_Error("Filename base of %s >8 chars", data);
    ++i;
  }
  return VStr(*this, start, length);
  unguard;
}


VStr VStr::ExtractFileExtension () const {
  guard(VStr::ExtractFileExtension);
  const char *src = data+length();
  while (src != data) {
    char ch = src[-1];
    if (ch == '.') return VStr(src);
#if !defined(_WIN32)
    if (ch == '/') return VStr();
#else
    if (ch == '/' || ch == '\\') return VStr();
#endif
    --src;
  }
  return VStr();
  unguard;
}


VStr VStr::StripExtension () const {
  guard(VStr::StripExtension);
  const char *src = data+length();
  while (src != data) {
    char ch = src[-1];
    if (ch == '.') return VStr(*this, 0, src-data-1);
#if !defined(_WIN32)
    if (ch == '/') break;
#else
    if (ch == '/' || ch == '\\') break;
#endif
    --src;
  }
  return VStr(*this);
  unguard;
}


VStr VStr::DefaultPath (const VStr &basepath) const {
  guard(VStr::DefaultPath);
#if !defined(_WIN32)
  if (data && data[0] == '/') return *this; // absolute path location
#else
  if (data && data[0] == '/') return *this; // absolute path location
  if (data && data[0] == '\\') return *this; // absolute path location
  if (data && data[1] == ':' && (data[2] == '/' || data[2] == '\\')) return *this; // absolute path location
#endif
  return basepath+(*this);
  unguard;
}


// if path doesn't have a .EXT, append extension (extension should include the leading dot)
VStr VStr::DefaultExtension (const VStr &extension) const {
  guard(VStr::DefaultExtension);
  const char *src = data+length();
  while (src != data) {
    char ch = src[-1];
    if (ch == '.') return VStr(*this);
#if !defined(_WIN32)
    if (ch == '/') break;
#else
    if (ch == '/' || ch == '\\') break;
#endif
    --src;
  }
  return VStr(*this)+extension;
  unguard;
}


VStr VStr::FixFileSlashes () const {
  guard(VStr::FixFileSlashes);
  bool hasWork = false;
  for (const char* c = data; *c; ++c) if (*c == '\\') { hasWork = true; break; }
  if (hasWork) {
    VStr res(*this);
    res.MakeMutable();
    for (char* c = res.data; *c; ++c) if (*c == '\\') *c = '/';
    return res;
  } else {
    return VStr(*this);
  }
  unguard;
}


size_t VStr::Utf8Length (const char *s) {
  guard(VStr::Utf8Length);
  size_t count = 0;
  if (s) {
    for (const char* c = s; *c; ++c) if ((*c&0xc0) != 0x80) ++count;
  }
  return count;
  unguard;
}


size_t VStr::ByteLengthForUtf8 (const char *s, size_t N) {
  guard(VStr::ByteLengthForUtf8);
  if (s) {
    size_t count = 0;
    const char* c;
    for (c = s; *c; ++c) {
      if ((*c&0xc0) != 0x80) {
        if (count == N) return c-s;
        ++count;
      }
    }
    check(N == count);
    return c-s;
  } else {
    return 0;
  }
  unguard;
}


int VStr::GetChar (const char *&s) {
  guard(VStr::GetChar);
  if ((vuint8)*s < 128) return *s++;
  int cnt, val;
  if ((*s&0xe0) == 0xc0) {
    val = *s&0x1f;
    cnt = 1;
  } else if ((*s&0xf0) == 0xe0) {
    val = *s&0x0f;
    cnt = 2;
  } else if ((*s&0xf8) == 0xf0) {
    val = *s&0x07;
    cnt = 3;
  } else {
    Sys_Error("Not a valid UTF-8");
    return 0;
  }
  ++s;

  do {
    if ((*s&0xc0) != 0x80) Sys_Error("Not a valid UTF-8");
    val = (val<<6)|(*s&0x3f);
    ++s;
  } while (--cnt);

  return val;
  unguard;
}


VStr VStr::FromChar (int c) {
  guard(VStr::FromChar);
  char res[8];
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
  unguard;
}


//==========================================================================
//
//  va
//
// Very usefull function from QUAKE
// Does a varargs printf into a temp buffer, so I don't need to have
// varargs versions of all text functions.
// FIXME: make this buffer size safe someday
//
//==========================================================================
__attribute__((format(printf, 1, 2))) char *va (const char *text, ...) {
  va_list args;
  va_bufnum = (va_bufnum+1)&15;
  va_start(args, text);
  vsnprintf(va_buffer[va_bufnum], 65535, text, args);
  va_end(args);
  return va_buffer[va_bufnum];
}
