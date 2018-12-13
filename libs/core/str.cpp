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


void VStr::vstrInitr_fuck_you_gnu_binutils_fuck_you_fuck_you_fuck_you () {
  memset(VStr::wc2shitmap, '?', sizeof(VStr::wc2shitmap));
  for (int f = 0; f < 128; ++f) VStr::wc2shitmap[f] = (char)f;
  for (int f = 0; f < 128; ++f) VStr::wc2shitmap[VStr::cp1251[f]] = (char)(f+128);
}


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
VStr::VStr (int v) : dataptr(nullptr) {
  char buf[64];
  int len = (int)snprintf(buf, sizeof(buf), "%d", v);
  setContent(buf, len);
}

VStr::VStr (unsigned v) : dataptr(nullptr) {
  char buf[64];
  int len = (int)snprintf(buf, sizeof(buf), "%u", v);
  setContent(buf, len);
}

VStr::VStr (float v) : dataptr(nullptr) {
  char buf[64];
  int len = (int)snprintf(buf, sizeof(buf), "%f", v);
  setContent(buf, len);
}

VStr::VStr (double v) : dataptr(nullptr) {
  char buf[64];
  int len = (int)snprintf(buf, sizeof(buf), "%f", v);
  setContent(buf, len);
}


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
  const char *data = getData();
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


VStr VStr::toUpperCase1251 () const {
  int slen = length();
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


// ////////////////////////////////////////////////////////////////////////// //
VStr VStr::utf2win () const {
  VStr res;
  if (length()) {
    VUtf8DecoderFast dc;
    const char *data = getData();
    for (int f = 0; f < length(); ++f) {
      if (dc.put(data[f])) res += wchar2win(dc.codepoint);
    }
  }
  return res;
}


VStr VStr::win2utf () const {
  VStr res;
  if (length()) {
    const char *data = getData();
    for (int f = 0; f < length(); ++f) {
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


// ////////////////////////////////////////////////////////////////////////// //
VStr VStr::mid (int start, int len) const {
  guard(VStr::mid);
  int mylen = length();
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
  return VStr(getData()+start, len);
  unguard;
}


VStr VStr::left (int len) const {
  if (len < 1) return VStr();
  if (len >= length()) return VStr(*this);
  return mid(0, len);
}


VStr VStr::right (int len) const {
  if (len < 1) return VStr();
  if (len >= length()) return VStr(*this);
  return mid(length()-len, len);
}


void VStr::chopLeft (int len) {
  if (len < 1) return;
  if (len >= length()) { clear(); return; }
  makeMutable();
  memmove(dataptr, dataptr+len, length()-len);
  resize(length()-len);
}


void VStr::chopRight (int len) {
  if (len < 1) return;
  if (len >= length()) { clear(); return; }
  resize(length()-len);
}


// ////////////////////////////////////////////////////////////////////////// //
void VStr::makeImmutable () {
  guard(VStr::makeImmutable);
  if (!dataptr) return; // nothing to do
  /*
  if (store()->rc >= 0 && store()->rc < 0xffff) {
    store()->rc = store()->rc+0xffff; // wow, big rc count, so it will be copied on mutation
  }
  */
  store()->rc = -0x0fffffff; // any negative means "immutable"
  unguard;
}


void VStr::makeMutable () {
  guard(VStr::makeMutable);
  if (!dataptr || store()->rc == 1) return; // nothing to do
  // allocate new string
  Store *oldstore = store();
  const char *olddata = dataptr;
  size_t olen = (size_t)oldstore->length;
  size_t newsz = olen+64; // overallocate a little
  Store *newdata = (Store *)Z_Malloc(sizeof(Store)+newsz+1);
  if (!newdata) Sys_Error("Out of memory");
  newdata->length = (int)olen;
  newdata->alloted = (int)newsz;
  newdata->rc = 1;
  dataptr = ((char *)newdata)+sizeof(Store);
  // copy old data
  memcpy(dataptr, olddata, olen+1);
  if (oldstore->rc > 0) --oldstore->rc; // decrement old refcounter
#ifdef VAVOOM_TEST_VSTR
  fprintf(stderr, "VStr: makeMutable: old=%p(%d); new=%p(%d)\n", oldstore+1, oldstore->rc, dataptr, newdata->rc);
#endif
  unguard;
}


void VStr::resize (int newlen) {
  guard(VStr::resize);

  // free string?
  if (newlen <= 0) {
    decref();
    return;
  }

  int oldlen = length();

  if (newlen == oldlen) {
    // same length, make string unique (just in case)
    makeMutable();
    return;
  }

  // new allocation?
  if (!dataptr) {
    size_t newsz = (size_t)(newlen+64);
    Store *ns = (Store *)Z_Malloc(sizeof(Store)+newsz+1);
    if (!ns) Sys_Error("Out of memory");
    ns->length = newlen;
    ns->alloted = newsz;
    ns->rc = 1;
    #ifdef VAVOOM_TEST_VSTR
    fprintf(stderr, "VStr: realloced(new): old=%p(%d); new=%p(%d)\n", dataptr, 0, ns+1, ns->rc);
    #endif
    dataptr = ((char *)ns)+sizeof(Store);
    dataptr[newlen] = 0;
    return;
  }

  // unique?
  if (store()->rc == 1) {
    // do in-place reallocs
    if (newlen < oldlen) {
      // shrink
      if (newlen < store()->alloted/2) {
        // realloc
        store()->alloted = newlen+64;
        Store *ns = (Store *)Z_Realloc(store(), sizeof(Store)+(size_t)store()->alloted+1);
        if (!ns) Sys_Error("Out of memory");
        #ifdef VAVOOM_TEST_VSTR
        fprintf(stderr, "VStr: realloced(shrink): old=%p(%d); new=%p(%d)\n", dataptr, store()->rc, ns+1, ns->rc);
        #endif
        dataptr = ((char *)ns)+sizeof(Store);
      }
    } else {
      // grow
      if (newlen > store()->alloted) {
        // need more room
        size_t newsz = (size_t)(newlen+(newlen < 0x0fffffff ? newlen/2 : 0));
        Store *ns = (Store *)Z_Realloc(store(), sizeof(Store)+newsz+1);
        if (!ns) {
          // try exact
          ns = (Store *)Z_Realloc(store(), sizeof(Store)+(size_t)newlen+1);
          if (!ns) Sys_Error("Out of memory");
          newsz = (size_t)newlen;
        }
        ns->alloted = newsz;
        #ifdef VAVOOM_TEST_VSTR
        fprintf(stderr, "VStr: realloced(grow): old=%p(%d); new=%p(%d)\n", dataptr, store()->rc, ns+1, ns->rc);
        #endif
        dataptr = ((char *)ns)+sizeof(Store);
      }
    }
    store()->length = newlen;
  } else {
    // not unique, have to allocate new data
    int alloclen;

    if (newlen < oldlen) {
      // shrink
      alloclen = newlen+64;
    } else {
      // grow
      alloclen = newlen+(newlen < 0x0fffffff ? newlen/2 : 0);
    }

    // allocate new storage
    Store *ns = (Store *)Z_Malloc(sizeof(Store)+alloclen+1);
    if (!ns) {
      // try exact
      ns = (Store *)Z_Malloc(sizeof(Store)+(size_t)newlen+1);
      if (!ns) Sys_Error("Out of memory");
      alloclen = newlen;
    }

    // copy data
    if (newlen > oldlen) {
      memcpy(((char *)ns)+sizeof(Store), dataptr, oldlen+1);
    } else {
      memcpy(((char *)ns)+sizeof(Store), dataptr, newlen+1);
    }
    // setup info
    ns->length = newlen;
    ns->alloted = alloclen;
    ns->rc = 1;
    // decrement old rc
    if (store()->rc > 0) --store()->rc;
    #ifdef VAVOOM_TEST_VSTR
    fprintf(stderr, "VStr: realloced(new): old=%p(%d); new=%p(%d)\n", dataptr, store()->rc, ns+1, ns->rc);
    #endif
    // use new data
    dataptr = ((char *)ns)+sizeof(Store);
  }

  // some functions expects this
  dataptr[newlen] = 0;

  unguard;
}


void VStr::setContent (const char *s, int len) {
  guard(VStr::setContent);
  if (s && s[0]) {
    if (len < 0) len = (int)strlen(s);
    size_t newsz = len+64;
    Store *ns = (Store *)Z_Malloc(sizeof(Store)+(size_t)newsz+1);
    if (!ns) Sys_Error("Out of memory");
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
  unguard;
}


bool VStr::StartsWith (const char *s) const {
  guard(VStr::StartsWith);
  if (!s || !s[0]) return false;
  int l = length(s);
  if (l > length()) return false;
  return (memcmp(getData(), s, l) == 0);
  unguard;
}


bool VStr::StartsWith (const VStr &s) const {
  guard(VStr::StartsWith);
  int l = s.length();
  if (l > length()) return false;
  return (memcmp(getData(), *s, l) == 0);
  unguard;
}


bool VStr::EndsWith (const char *s) const {
  guard(VStr::EndsWith);
  if (!s || !s[0]) return false;
  int l = Length(s);
  if (l > length()) return false;
  return (memcmp(getData()+length()-l, s, l) == 0);
  unguard;
}


bool VStr::EndsWith (const VStr &s) const {
  guard(VStr::EndsWith);
  int l = s.length();
  if (l > length()) return false;
  return (memcmp(getData()+length()-l, *s, l) == 0);
  unguard;
}


bool VStr::startsWithNoCase (const char *s) const {
  guard(VStr::StartsWith);
  if (!s || !s[0]) return false;
  int l = length(s);
  if (l > length()) return false;
  return (NICmp(getData(), s, l) == 0);
  unguard;
}


bool VStr::startsWithNoCase (const VStr &s) const {
  guard(VStr::StartsWith);
  int l = s.length();
  if (l > length()) return false;
  return (NICmp(getData(), *s, l) == 0);
  unguard;
}


bool VStr::endsWithNoCase (const char *s) const {
  guard(VStr::EndsWith);
  if (!s || !s[0]) return false;
  int l = Length(s);
  if (l > length()) return false;
  return (NICmp(getData()+length()-l, s, l) == 0);
  unguard;
}


bool VStr::endsWithNoCase (const VStr &s) const {
  guard(VStr::EndsWith);
  int l = s.length();
  if (l > length()) return false;
  return (NICmp(getData()+length()-l, *s, l) == 0);
  unguard;
}


bool VStr::startsWith (const char *str, const char *part) {
  if (!str || !str[0]) return false;
  if (!part || !part[0]) return false;
  int slen = VStr::length(str);
  int plen = VStr::length(part);
  if (plen > slen) return false;
  return (memcmp(str, part, plen) == 0);
}


bool VStr::endsWith (const char *str, const char *part) {
  if (!str || !str[0]) return false;
  if (!part || !part[0]) return false;
  int slen = VStr::length(str);
  int plen = VStr::length(part);
  if (plen > slen) return false;
  return (memcmp(str+slen-plen, part, plen) == 0);
}


bool VStr::startsWithNoCase (const char *str, const char *part) {
  if (!str || !str[0]) return false;
  if (!part || !part[0]) return false;
  int slen = VStr::length(str);
  int plen = VStr::length(part);
  if (plen > slen) return false;
  return (NICmp(str, part, plen) == 0);
}


bool VStr::endsWithNoCase (const char *str, const char *part) {
  if (!str || !str[0]) return false;
  if (!part || !part[0]) return false;
  int slen = VStr::length(str);
  int plen = VStr::length(part);
  if (plen > slen) return false;
  return (NICmp(str+slen-plen, part, plen) == 0);
}


VStr VStr::ToLower () const {
  guard(VStr::ToLower);
  if (!dataptr) return VStr();
  bool hasWork = false;
  int l = length();
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
  unguard;
}


VStr VStr::ToUpper () const {
  guard(VStr::ToUpper);
  const char *data = getData();
  if (!data) return VStr();
  bool hasWork = false;
  int l = length();
  for (int i = 0; i < l; ++i) if (data[i] >= 'a' && data[i] <= 'z') { hasWork = true; break; }
  if (hasWork) {
    VStr res(*this);
    res.makeMutable();
    for (int i = 0; i < l; ++i) if (res.dataptr[i] >= 'a' && res.dataptr[i] <= 'z') res.dataptr[i] -= 32; // poor man's toupper()
    return res;
  } else {
    return VStr(*this);
  }
  unguard;
}


int VStr::IndexOf (char c) const {
  guard(VStr::IndexOf);
  const char *data = getData();
  if (data && length()) {
    const char *pos = (const char *)memchr(data, c, length());
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
  if (l == 0 || sl == 0) return -1;
  const char *data = getData();
  for (int i = 0; i <= l-sl; ++i) if (NCmp(data+i, s, sl) == 0) return i;
  return -1;
  unguard;
}


int VStr::IndexOf (const VStr &s) const {
  guard(VStr::IndexOf);
  int sl = int(s.length());
  if (!sl) return -1;
  int l = int(length());
  if (l == 0) return -1;
  const char *data = getData();
  for (int i = 0; i <= l-sl; ++i) if (NCmp(data+i, *s, sl) == 0) return i;
  return -1;
  unguard;
}


int VStr::LastIndexOf (char c) const {
  guard(VStr::LastIndexOf);
  const char *data = getData();
  if (data && length()) {
#if !defined(WIN32) && !defined(NO_MEMRCHR)
    const char *pos = (const char *)memrchr(data, c, length());
    return (pos ? (int)(pos-data) : -1);
#else
    size_t pos = length();
    while (pos > 0 && data[pos-1] != c) --pos;
    return (pos ? (int)(pos-1) : -1);
#endif
  } else {
    return -1;
  }
  unguard;
}


int VStr::LastIndexOf (const char *s) const {
  guard(VStr::LastIndexOf);
  if (!s || !s[0]) return -1;
  int sl = int(Length(s));
  int l = int(length());
  if (l == 0 || sl == 0) return -1;
  const char *data = getData();
  for (int i = l-sl; i >= 0; --i) if (NCmp(data+i, s, sl) == 0) return i;
  return -1;
  unguard;
}


int VStr::LastIndexOf (const VStr &s) const {
  guard(VStr::LastIndexOf);
  int sl = int(s.length());
  if (!sl) return -1;
  int l = int(length());
  if (l == 0) return -1;
  const char *data = getData();
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
    if (NCmp(res.getData()+i, *Search, SLen) == 0) {
      // if search and replace strings are of the same size,
      // we can just copy the data and avoid memory allocations
      if (SLen == RLen) {
        res.makeMutable();
        memcpy(res.dataptr+i, *Replacement, RLen);
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
  int RealStart = int(ByteLengthForUtf8(getData(), start));
  int RealLen = int(ByteLengthForUtf8(getData(), start+len)-RealStart);
  return VStr(*this, RealStart, RealLen);
}


void VStr::Split (char c, TArray<VStr> &A) const {
  guard(VStr::Split);
  A.Clear();
  const char *data = getData();
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


void VStr::Split (const char *chars, TArray<VStr> &A) const {
  guard(VStr::Split);
  A.Clear();
  const char *data = getData();
  if (!data) return;
  int start = 0;
  int len = int(length());
  for (int i = 0; i <= len; ++i) {
    bool DoSplit = (i == len);
    if (!DoSplit) {
      for (const char *pChar = chars; !DoSplit && *pChar; ++pChar) {
        DoSplit = (data[i] == *pChar);
        if (DoSplit) break;
      }
    }
    if (DoSplit) {
      if (start != i) A.Append(VStr(*this, start, i-start));
      start = i+1;
    }
  }
  unguard;
}


void VStr::SplitOnBlanks (TArray<VStr> &A, bool doQuotedStrings) const {
  guard(VStr::SplitOnBlanks);
  A.Clear();
  const char *data = getData();
  if (!data) return;
  int len = int(length());
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
  unguard;
}


// split string to path components; first component can be '/', others has no slashes
void VStr::SplitPath (TArray<VStr>& arr) const {
  guard(VStr::SplitPath);

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
  if (data[0] == '/' || data[0] == '\\') {
    arr.Append(VStr("/"));
  } else if (data[1] == ':' && (data[2] == '/' || data[2] == '\\')) {
    arr.Append(mid(0, 3));
    pos = 3;
  }
  while (pos < len) {
    if (data[pos] == '/' || data[pos] == '\\') { ++pos; continue; }
    int epos = pos+1;
    while (epos < len && data[epos] != '/' && data[epos] != '\\') ++epos;
    arr.Append(mid(pos, epos-pos));
    pos = epos+1;
  }
#endif

  unguard;
}


bool VStr::IsValidUtf8 () const {
  guard(VStr::IsValidUtf8);
  const char *data = getData();
  if (!data) return true;
  for (const char *c = data; *c;) {
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
  const char *data = getData();
  VStr res;
  for (int i = 0; i < length(); ++i) res += FromChar((vuint8)data[i]);
  return res;
  unguard;
}


VStr VStr::EvalEscapeSequences () const {
  guard(VStr::EvalEscapeSequences);
  VStr res;
  const char *c = getData();
  if (!c || !c[0]) return res;
  int val;
  while (*c) {
    if (c[0] == '\\' && c[1]) {
      ++c;
      switch (*c++) {
        case 't': res += '\t'; break;
        case 'n': res += '\n'; break;
        case 'r': res += '\r'; break;
        case 'e': res += '\x1b'; break;
        case 'c': res += TEXT_COLOUR_ESCAPE; break;
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
  return res;
  unguard;
}


bool VStr::MustBeSanitized () const {
  int len = (int)length();
  if (len < 1) return false;
  for (const vuint8 *s = (const vuint8 *)getData(); *s; ++s) {
    if (*s < ' ' || *s == 127) return true;
  }
  return false;
}


bool VStr::MustBeSanitized (const char *str) {
  if (!str || !str[0]) return false;
  for (const vuint8 *s = (const vuint8 *)str; *s; ++s) {
    if (*s != '\n' && *s != '\t') {
      if (*s < ' ' || *s == 127) return true;
    }
  }
  return false;
}


VStr VStr::RemoveColours () const {
  guard(VStr::RemoveColours);
  const char *data = getData();
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
      } else {
        ++pos;
      }
    } else {
      if (!c) break;
      if (c != '\n' && c != '\t') {
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
    if (c == TEXT_COLOUR_ESCAPE) {
      if (data[pos] == '[') {
        ++pos;
        while (pos < oldlen && data[pos] != ']') ++pos;
        ++pos;
      } else {
        ++pos;
      }
    } else {
      if (!c) break;
      if (newlen >= res.length()) break; // oops
      if (c != '\n' && c != '\t') {
        if ((vuint8)c < ' ' || c == 127) continue;
      }
      res.dataptr[newlen++] = c;
    }
  }
  return res;
  unguard;
}


VStr VStr::ExtractFilePath () const {
  guard(FL_ExtractFilePath);
  const char *src = getData()+length();
#if !defined(_WIN32)
  while (src != getData() && src[-1] != '/') --src;
#else
  while (src != getData() && src[-1] != '/' && src[-1] != '\\') --src;
#endif
  return VStr(*this, 0, src-getData());
  unguard;
}


VStr VStr::ExtractFileName() const {
  guard(VStr:ExtractFileName);
  const char *data = getData();
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

  const char *data = getData();
#if !defined(_WIN32)
  // back up until a \ or the start
  while (i && data[i-1] != '/') --i;
#else
  while (i && data[i-1] != '/' && data[i-1] != '\\' && data[i-1] != ':') --i;
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


VStr VStr::ExtractFileBaseName () const {
  guard(VStr::ExtractFileBaseName);
  int i = int(length());

  if (i == 0) return VStr();

  const char *data = getData();
#if !defined(_WIN32)
  // back up until a \ or the start
  while (i && data[i-1] != '/') --i;
#else
  while (i && data[i-1] != '/' && data[i-1] != '\\' && data[i-1] != ':') --i;
#endif

  return VStr(*this, i, length()-i);
  unguard;
}


VStr VStr::ExtractFileExtension () const {
  guard(VStr::ExtractFileExtension);
  const char *data = getData();
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
  const char *data = getData();
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
  const char *data = getData();
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
  const char *data = getData();
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
  const char *data = getData();
  bool hasWork = false;
  for (const char *c = data; *c; ++c) if (*c == '\\') { hasWork = true; break; }
  if (hasWork) {
    VStr res(*this);
    res.makeMutable();
    for (char *c = res.dataptr; *c; ++c) if (*c == '\\') *c = '/';
    return res;
  } else {
    return VStr(*this);
  }
  unguard;
}


int VStr::Utf8Length (const char *s, int len) {
  guard(VStr::Utf8Length);
  if (len < 0) len = (s && s[0] ? (int)strlen(s) : 0);
  int count = 0;
  while (len-- > 0) if (((*s++)&0xc0) != 0x80) ++count;
  return count;
  unguard;
}


size_t VStr::ByteLengthForUtf8 (const char *s, size_t N) {
  guard(VStr::ByteLengthForUtf8);
  if (s) {
    size_t count = 0;
    const char *c;
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
    //Sys_Error("Not a valid UTF-8");
    ++s;
    return '?';
  }
  ++s;

  do {
    if ((*s&0xc0) != 0x80) {
      //Sys_Error("Not a valid UTF-8");
      return '?';
    }
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


bool VStr::needQuoting () const {
  int len = length();
  const char *data = getData();
  for (int f = 0; f < len; ++f) {
    vuint8 ch = (vuint8)data[f];
    if (ch < ' ' || ch == '\\' || ch == '"' || ch >= 127) return true;
  }
  return false;
}


VStr VStr::quote (bool addQCh) const {
  int len = length();
  char hexb[6];
  const char *data = getData();
  for (int f = 0; f < len; ++f) {
    vuint8 ch = (vuint8)data[f];
    if (ch < ' ' || ch == '\\' || ch == '"' || ch >= 127) {
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
            case TEXT_COLOUR_ESCAPE: res += "\\c"; break;
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
  return VStr(*this);
}


//==========================================================================
//
//  VStr::buf2hex
//
//==========================================================================
VStr VStr::buf2hex (const void *buf, int buflen) {
  static const char *hexd = "0123456789abcdef";
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
bool VStr::convertInt (const char *s, int *outv) {
  bool neg = false;
  int dummy = 0;
  if (!outv) outv = &dummy;
  *outv = 0;
  if (!s || !s[0]) return false;
  while (*s && (vuint8)*s <= ' ') ++s;
  if (*s == '+') ++s; else if (*s == '-') { neg = true; ++s; }
  if (!s[0]) return false;
  if (s[0] < '0' || s[0] > '9') return false;
  int base = 10;
  if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
    if (!s[2] || digitInBase(s[2], 16) < 0) return false;
    base = 16;
    s += 2;
  }
  while (*s) {
    char ch = *s++;
    int d = digitInBase(ch, base);
    if (d < 0) { *outv = 0; return false; }
    *outv = (*outv)*base+d;
  }
  while (*s && (vuint8)*s <= ' ') ++s;
  if (*s) { *outv = 0; return false; }
  if (neg) *outv = -(*outv);
  return true;
}


//==========================================================================
//
//  VStr::convertFloat
//
//==========================================================================
bool VStr::convertFloat (const char *s, float *outv, const float *defval) {
  float dummy = 0;
  if (!outv) outv = &dummy;
  *outv = 0.0f;
  if (!s || !s[0]) { if (defval) *outv = *defval; return false; }
  while (*s && (vuint8)*s <= ' ') ++s;
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
}


//==========================================================================
//
//  VStr::globmatch
//
//==========================================================================
bool VStr::globmatch (const char *pat, const char *str, bool caseSensitive) {
  bool star = false;
  if (!pat) pat = "";
  if (!str) str = "";
loopStart:
  size_t patpos = 0;
  for (size_t i = 0; str[i]; ++i) {
    char sch = str[i];
    //if (patpos == pat.length) goto starCheck;
    if (!pat[patpos]) goto starCheck;
    switch (pat[patpos++]) {
      case '\\':
        //if (patpos == pat.length) return false; // malformed pattern
        if (!pat[patpos]) return false; // malformed pattern
        ++patpos;
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
      default:
      defcheck:
        if (caseSensitive) {
          if (sch != pat[patpos-1]) goto starCheck;
        } else {
          if (upcase1251(sch) != upcase1251(pat[patpos-1])) goto starCheck;
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
void VStr::Tokenise (TArray <VStr> &args) const {
  //args.reset();
  if (length() == 0) return;
  const char *str = getCStr();
  while (*str) {
    // whitespace
    if ((vuint8)*str <= ' ') { ++str; continue; }

    // string?
    if (*str == '"' || *str == '\'') {
      char qch = *str++;
      VStr ss;
      int cc, d;
      while (*str && *str != qch) {
        if (str[0] == '\\' && str[1]) {
          ++str;
          switch (*str++) {
            case 't': ss += '\t'; break;
            case 'n': ss += '\n'; break;
            case 'r': ss += '\r'; break;
            case 'e': ss += '\x1b'; break;
            case 'c': ss += TEXT_COLOUR_ESCAPE; break;
            case '\\': case '"': case '\'': ss += str[-1]; break;
            case 'x':
              cc = 0;
              d = (*str ? VStr::digitInBase(*str, 16) : -1);
              if (d >= 0) {
                cc = d;
                ++str;
                d = (*str ? VStr::digitInBase(*str, 16) : -1);
                if (d >= 0) { cc = cc*16+d; ++str; }
                ss += (char)cc;
              } else {
                ss += str[-1];
              }
              break;
            default: // ignore other quotes
              //ss += '\\';
              ss += str[-1];
              break;
          }
        } else {
          ss += *str++;
        }
      }
      if (*str) { check(*str == qch); ++str; } // skip closing quote
      //fprintf(stderr, "*#*<%s>\n", *ss);
      args.append(ss);
    } else {
      // simple arg
      const char *end = str;
      while ((vuint8)*end > ' ') ++end;
      args.append(VStr(str, (int)(ptrdiff_t)(end-str)));
      //fprintf(stderr, "*:*<%s>\n", *args[args.length()-1]);
      str = end;
    }
  }
  /*
  {
    fprintf(stderr, "<%s> : %d\n", getCStr(), args.length());
    for (int f = 0; f < args.length(); ++f) fprintf(stderr, "  %d: <%s>\n", f, *args[f]);
  }
  */
}


//==========================================================================
//
// va
//
// Very useful function from Quake.
// Does a varargs printf into a temp buffer, so I don't need to have
// varargs versions of all text functions.
//
//==========================================================================
__attribute__((format(printf, 1, 2))) char *va (const char *text, ...) {
  static char va_buffer[32][32768];
  static int va_bufnum = 0;
  va_list args;
  va_bufnum = (va_bufnum+1)&31;
  va_start(args, text);
  vsnprintf(va_buffer[va_bufnum], 32767, text, args);
  va_end(args);
  return va_buffer[va_bufnum];
}
