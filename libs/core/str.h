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
// dynamic string class
//
//**************************************************************************


// ////////////////////////////////////////////////////////////////////////// //
#define TEXT_COLOR_ESCAPE      '\034'
#define TEXT_COLOR_ESCAPE_STR  "\034"


extern int secs2msecs (const double secs) noexcept;

// guaranteed up to 16 separate values
extern char *va (const char *text, ...) noexcept __attribute__((format(printf, 1, 2))) VVA_CHECKRESULT;
extern char *vavarg (const char *text, va_list ap) noexcept VVA_CHECKRESULT;
extern const char *comatoze (vuint32 n, const char *sfx=nullptr) noexcept;
// returns string that can be directly printed
extern const char *secs2timestr (const double secs) noexcept;



struct VStr_ByCharIterator;

// ////////////////////////////////////////////////////////////////////////// //
// WARNING! this cannot be bigger than one pointer, or VM will break!
// WARNING! this is NOT MT-SAFE! if you want to use it from multiple threads,
//          make sure to `cloneUniqueMT()` it, and pass to each thread its own VStr!
//          but note that immutable strings *are* MT-safe.
class VStr {
  friend struct VStr_ByCharIterator;
public:
  //HACK: multithreaded locking is done by orring setting `rc` with 0x80000000 (which makes the string immutable, btw)
  struct __attribute__((packed)) Store {
    vint32 length;
    vint32 alloted;
    vint32 rc; // negative number means "immutable string"
    // actual string data starts after this struct; and this is where `data` points
    vint32 dummy; // this is to get natural align on 8-byte boundary
  };
  static_assert(sizeof(Store) == 8*2, "invalid size for `VStr::Store` struct");
  static_assert(__builtin_offsetof(Store, rc)%8 == 0, "invalid rc alignent for `VStr::Store` struct");

  char *dataptr; // string, 0-terminated (0 is not in length); can be null

protected:
  VVA_CHECKRESULT VVA_ALWAYS_INLINE Store *store () noexcept { return (dataptr ? (Store *)(dataptr-sizeof(Store)) : nullptr); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE Store *store () const noexcept { return (dataptr ? (Store *)(dataptr-sizeof(Store)) : nullptr); }

  // should be called only when storage is available
  VVA_CHECKRESULT VVA_ALWAYS_INLINE int atomicGetRC () const noexcept { return __atomic_load_n(&((const Store *)(dataptr-sizeof(Store)))->rc, __ATOMIC_SEQ_CST); }
  // should be called only when storage is available
  VVA_ALWAYS_INLINE void atomicSetRC (int newval) noexcept { __atomic_store_n(&((Store *)(dataptr-sizeof(Store)))->rc, newval, __ATOMIC_SEQ_CST); }
  // should be called only when storage is available
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool atomicIsImmutable () const noexcept { return (__atomic_load_n(&((const Store *)(dataptr-sizeof(Store)))->rc, __ATOMIC_SEQ_CST) < 0); }
  // should be called only when storage is available
  // immutable strings aren't unique
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool atomicIsUnique () const noexcept { return (__atomic_load_n(&((const Store *)(dataptr-sizeof(Store)))->rc, __ATOMIC_SEQ_CST) == 1); }
  // should be called only when storage is available
  // returns new value
  // WARNING: will happily modify immutable RC!
  VVA_ALWAYS_INLINE void atomicIncRC () const noexcept { (void)__atomic_add_fetch(&((Store *)(dataptr-sizeof(Store)))->rc, 1, __ATOMIC_SEQ_CST); }
  VVA_ALWAYS_INLINE int atomicIncRCRetOld () const noexcept { return __atomic_fetch_add(&((Store *)(dataptr-sizeof(Store)))->rc, 1, __ATOMIC_SEQ_CST); }
  // should be called only when storage is available
  // returns new value
  // WARNING: will happily modify immutable RC!
  VVA_ALWAYS_INLINE int atomicDecRC () const noexcept { return __atomic_sub_fetch(&((Store *)(dataptr-sizeof(Store)))->rc, 1, __ATOMIC_SEQ_CST); }
  VVA_ALWAYS_INLINE void atomicDecRCVoid () const noexcept { (void)__atomic_sub_fetch(&((Store *)(dataptr-sizeof(Store)))->rc, 1, __ATOMIC_SEQ_CST); }

  VVA_CHECKRESULT VVA_ALWAYS_INLINE char *getData () noexcept { return dataptr; }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE const char *getData () const noexcept { return dataptr; }

  VVA_ALWAYS_INLINE void incref () const noexcept { if (dataptr && !atomicIsImmutable()) atomicIncRC(); }

  // WARNING! may free `data` contents!
  // this also clears `data`
  inline void decref () noexcept {
    if (dataptr) {
      if (!atomicIsImmutable()) {
        if (atomicDecRC() == 0) {
          #ifdef VAVOOM_TEST_VSTR
          fprintf(stderr, "VStr: freeing %p\n", dataptr);
          #endif
          Z_Free(store());
        }
      }
      dataptr = nullptr;
    }
  }

  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool isMyData (const char *buf, int len) const noexcept { return (dataptr && buf && (uintptr_t)buf < (uintptr_t)dataptr+length() && (uintptr_t)buf+len >= (uintptr_t)dataptr); }

  VVA_ALWAYS_INLINE void assign (const VStr &instr) noexcept {
    if (&instr != this) {
      if (instr.dataptr) {
        if (instr.dataptr != dataptr) {
          instr.incref();
          decref();
          dataptr = (char *)instr.dataptr;
        }
      } else {
        clear();
      }
    }
  }

  void makeMutable () noexcept; // and unique
  void resize (int newlen) noexcept; // always makes string unique; also, always sets [length] to 0; clears string on `newlen == 0`
  void setContent (const char *s, int len=-1) noexcept;
  void setNameContent (const VName InName) noexcept;

public:
  // debug
  VVA_ALWAYS_INLINE int dbgGetRef () const noexcept { return (dataptr ? atomicGetRC() : 0); }

public:
  // some utilities
  static bool convertInt (const char *s, int *outv, bool loose=false) noexcept;
  static bool convertFloat (const char *s, float *outv, const float *defval=nullptr) noexcept;

  static float atof (const char *s) noexcept { float res = 0; convertFloat(s, &res, nullptr); return res; }
  static float atof (const char *s, float defval) noexcept { float res = 0; convertFloat(s, &res, &defval); return res; }

  static int atoiStrict (const char *s) noexcept { int res = 0; if (!convertInt(s, &res)) res = 0; return res; }
  static int atoiStrict (const char *s, int defval) noexcept { int res = 0; if (!convertInt(s, &res)) res = defval; return res; }

  static int atoi (const char *s) noexcept { int res = 0; convertInt(s, &res, true); return res; }

  enum { FloatBufSize = 32 }; /* 16 is enough, but let's play safe */
  /* returns string length without trailing zero */
  static int float2str (char *buf, float v) noexcept; // 0-terminated

  enum { DoubleBufSize = 64 }; /* 25 is enough, but let's play safe */
  /* returns string length without trailing zero */
  static int double2str (char *buf, double v) noexcept; // 0-terminated

public:
  VVA_ALWAYS_INLINE VStr () noexcept : dataptr(nullptr) {}
  VVA_ALWAYS_INLINE VStr (const VStr &instr) noexcept : dataptr(instr.dataptr) { incref(); }
  VVA_ALWAYS_INLINE VStr (const char *instr, int len=-1) noexcept : dataptr(nullptr) { setContent(instr, len); }
  VVA_ALWAYS_INLINE VStr (const VStr &instr, int start, int len) noexcept : dataptr(nullptr) { assign(instr.mid(start, len)); }

  explicit VVA_ALWAYS_INLINE VStr (const VName InName) noexcept : dataptr(nullptr) { setNameContent(InName); }
  explicit VVA_ALWAYS_INLINE VStr (const EName InName) noexcept : dataptr(nullptr) { setNameContent(*VName(InName)); }

  explicit VVA_ALWAYS_INLINE VStr (char v) noexcept : dataptr(nullptr) { setContent(&v, 1); }
  explicit VVA_ALWAYS_INLINE VStr (bool v) noexcept : dataptr(nullptr) { setContent(v ? "true" : "false"); }
  explicit VStr (int v) noexcept;
  explicit VStr (unsigned v) noexcept;
  explicit VStr (float v) noexcept;
  explicit VStr (double v) noexcept;

  VVA_ALWAYS_INLINE ~VStr () noexcept { clear(); }

  // this will create an unique copy of the string, which (copy) can be used in other threads
  VVA_CHECKRESULT VStr cloneUnique () const noexcept;

  // this will create an unique copy of the string, which (copy) can be used in other threads
  // doesn't clone immutable strings, though
  VVA_CHECKRESULT VStr cloneUniqueMT () const noexcept;

  void makeImmutable () noexcept;
  VVA_CHECKRESULT VStr &makeImmutableRetSelf () noexcept;

  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool isNullOrImmutable () const noexcept { return (!dataptr || atomicIsImmutable()); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool isNull () const noexcept { return !dataptr; }

  // clears the string
  VVA_ALWAYS_INLINE void Clean () noexcept { decref(); }
  VVA_ALWAYS_INLINE void Clear () noexcept { decref(); }
  VVA_ALWAYS_INLINE void clear () noexcept { decref(); }

  // returns length of the string
  VVA_CHECKRESULT VVA_ALWAYS_INLINE int Length () const noexcept { return (dataptr ? store()->length : 0); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE int length () const noexcept { return (dataptr ? store()->length : 0); }

  inline void setLength (int len, char fillChar=' ') noexcept {
    if (len < 0) len = 0;
    resize(len);
    if (len > 0) memset(getData(), fillChar&0xff, len);
  }
  VVA_ALWAYS_INLINE void SetLength (int len, char fillChar=' ') noexcept { setLength(len, fillChar); }

  VVA_CHECKRESULT VVA_ALWAYS_INLINE int getCapacity () const noexcept { return (dataptr ? store()->alloted : 0); }

  // returns number of characters in a UTF-8 string
  VVA_CHECKRESULT VVA_ALWAYS_INLINE int Utf8Length () const noexcept { return Utf8Length(getCStr(), length()); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE int utf8Length () const noexcept { return Utf8Length(getCStr(), length()); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE int utf8length () const noexcept { return Utf8Length(getCStr(), length()); }

  // returns C string
  // `*` can be used in some dummied-out macros
  /*VVA_CHECKRESULT*/ VVA_ALWAYS_INLINE const char *operator * () const noexcept { return (dataptr ? getData() : ""); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE const char *getCStr () const noexcept { return (dataptr ? getData() : ""); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE char *getMutableCStr () noexcept { makeMutable(); return (dataptr ? getData() : nullptr); }

  // character accessors
  VVA_CHECKRESULT VVA_ALWAYS_INLINE char operator [] (int idx) const noexcept { return (dataptr && idx >= 0 && idx < length() ? getData()[idx] : 0); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE char *GetMutableCharPointer (int idx) noexcept { makeMutable(); return (dataptr ? &dataptr[idx] : nullptr); }

  // checks if string is empty
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool IsEmpty () const noexcept { return (length() == 0); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool isEmpty () const noexcept { return (length() == 0); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool IsNotEmpty () const noexcept { return (length() != 0); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool isNotEmpty () const noexcept { return (length() != 0); }

  // if start is negative, `len` will be adjusted!
  VVA_CHECKRESULT VStr mid (int start, int len) const noexcept;
  VVA_CHECKRESULT VStr left (int len) const noexcept;
  VVA_CHECKRESULT VStr right (int len) const noexcept;

  VVA_CHECKRESULT VStr leftskip (int skiplen) const noexcept;
  VVA_CHECKRESULT VStr rightskip (int skiplen) const noexcept;

  void chopLeft (int len) noexcept;
  void chopRight (int len) noexcept;

  // assignement operators
  VVA_ALWAYS_INLINE void operator = (const char *instr) noexcept { setContent(instr); }
  VVA_ALWAYS_INLINE void operator = (const VStr &instr) noexcept { assign(instr); }

  VStr &appendCStr (const char *instr, int len=-1) noexcept {
    if (len < 0) len = (int)(instr && instr[0] ? strlen(instr) : 0);
    if (len) {
      if (isMyData(instr, len)) {
        // need to copy it
        VStr s(instr, len);
        operator+=(s);
      } else {
        const int l = length();
        resize(l+len);
        memcpy(dataptr+l, instr, len);
        dataptr[l+len] = 0;
      }
    }
    return *this;
  }

  // concatenation operators
  VVA_ALWAYS_INLINE VStr &operator += (const char *instr) noexcept { return appendCStr(instr, -1); }

  VStr &operator += (const VStr &instr) noexcept {
    const int inl = instr.length();
    if (inl) {
      const int l = length();
      if (l) {
        VStr s(instr); // this is cheap
        resize(l+inl);
        memcpy(dataptr+l, s.getData(), inl+1);
      } else {
        assign(instr);
      }
    }
    return *this;
  }

  inline VStr &operator += (char inchr) noexcept {
    const int l = length();
    resize(l+1);
    dataptr[l] = inchr;
    return *this;
  }

  VVA_ALWAYS_INLINE VStr &operator += (bool v) noexcept { return operator+=(v ? "true" : "false"); }
  VVA_ALWAYS_INLINE VStr &operator += (int v) noexcept { char buf[64]; snprintf(buf, sizeof(buf), "%d", v); return operator+=(buf); }
  VVA_ALWAYS_INLINE VStr &operator += (unsigned v) noexcept { char buf[64]; snprintf(buf, sizeof(buf), "%u", v); return operator+=(buf); }
  //VVA_ALWAYS_INLINE VStr &operator += (float v) noexcept { char buf[64]; snprintf(buf, sizeof(buf), "%f", v); return operator+=(buf); }
  //VVA_ALWAYS_INLINE VStr &operator += (double v) noexcept { char buf[64]; snprintf(buf, sizeof(buf), "%f", v); return operator+=(buf); }
  VStr &operator += (float v) noexcept;
  VStr &operator += (double v) noexcept;
  VVA_ALWAYS_INLINE VStr &operator += (const VName &v) noexcept { return operator+=(*v); }

  friend VVA_CHECKRESULT VStr operator + (const VStr &S1, const char *S2) noexcept { VStr res(S1); res += S2; return res; }
  friend VVA_CHECKRESULT VStr operator + (const VStr &S1, const VStr &S2) noexcept { VStr res(S1); res += S2; return res; }
  friend VVA_CHECKRESULT VStr operator + (const VStr &S1, char S2) noexcept { VStr res(S1); res += S2; return res; }
  friend VVA_CHECKRESULT VStr operator + (const VStr &S1, bool v) noexcept { VStr res(S1); res += v; return res; }
  friend VVA_CHECKRESULT VStr operator + (const VStr &S1, int v) noexcept { VStr res(S1); res += v; return res; }
  friend VVA_CHECKRESULT VStr operator + (const VStr &S1, unsigned v) noexcept { VStr res(S1); res += v; return res; }
  friend VVA_CHECKRESULT VStr operator + (const VStr &S1, float v) noexcept { VStr res(S1); res += v; return res; }
  friend VVA_CHECKRESULT VStr operator + (const VStr &S1, double v) noexcept { VStr res(S1); res += v; return res; }
  friend VVA_CHECKRESULT VStr operator + (const VStr &S1, const VName &v) noexcept { VStr res(S1); res += v; return res; }

  // comparison operators
  friend VVA_CHECKRESULT bool operator == (const VStr &S1, const char *S2) noexcept { return (Cmp(*S1, S2) == 0); }
  friend VVA_CHECKRESULT bool operator == (const VStr &S1, const VStr &S2) noexcept { return (S1.getData() == S2.getData() ? true : (Cmp(*S1, *S2) == 0)); }
  friend VVA_CHECKRESULT bool operator != (const VStr &S1, const char *S2) noexcept { return (Cmp(*S1, S2) != 0); }
  friend VVA_CHECKRESULT bool operator != (const VStr &S1, const VStr &S2) noexcept { return (S1.getData() == S2.getData() ? false : (Cmp(*S1, *S2) != 0)); }
  friend VVA_CHECKRESULT bool operator < (const VStr &S1, const char *S2) noexcept { return (Cmp(*S1, S2) < 0); }
  friend VVA_CHECKRESULT bool operator < (const VStr &S1, const VStr &S2) noexcept { return (S1.getData() == S2.getData() ? false : (Cmp(*S1, *S2) < 0)); }
  friend VVA_CHECKRESULT bool operator > (const VStr &S1, const char *S2) noexcept { return (Cmp(*S1, S2) > 0); }
  friend VVA_CHECKRESULT bool operator > (const VStr &S1, const VStr &S2) noexcept { return (S1.getData() == S2.getData() ? false : (Cmp(*S1, *S2) > 0)); }
  friend VVA_CHECKRESULT bool operator <= (const VStr &S1, const char *S2) noexcept { return (Cmp(*S1, S2) <= 0); }
  friend VVA_CHECKRESULT bool operator <= (const VStr &S1, const VStr &S2) noexcept { return (S1.getData() == S2.getData() ? true : (Cmp(*S1, *S2) <= 0)); }
  friend VVA_CHECKRESULT bool operator >= (const VStr &S1, const char *S2) noexcept { return (Cmp(*S1, S2) >= 0); }
  friend VVA_CHECKRESULT bool operator >= (const VStr &S1, const VStr &S2) noexcept { return (S1.getData() == S2.getData() ? true : (Cmp(*S1, *S2) >= 0)); }

  // comparison functions
  VVA_CHECKRESULT VVA_ALWAYS_INLINE int Cmp (const char *S2) const noexcept { return Cmp(getData(), S2); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE int Cmp (const VStr &S2) const noexcept { return Cmp(getData(), *S2); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE int ICmp (const char *S2) const noexcept { return ICmp(getData(), S2); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE int ICmp (const VStr &S2) const noexcept { return ICmp(getData(), *S2); }

  VVA_CHECKRESULT VVA_ALWAYS_INLINE int cmp (const char *S2) const noexcept { return Cmp(getData(), S2); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE int cmp (const VStr &S2) const noexcept { return Cmp(getData(), *S2); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE int icmp (const char *S2) const noexcept { return ICmp(getData(), S2); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE int icmp (const VStr &S2) const noexcept { return ICmp(getData(), *S2); }

  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool StrEqu (const char *S2) const noexcept { return (Cmp(getData(), S2) == 0); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool StrEqu (const VStr &S2) const noexcept { return (Cmp(getData(), *S2) == 0); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool StrEquCI (const char *S2) const noexcept { return (ICmp(getData(), S2) == 0); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool StrEquCI (const VStr &S2) const noexcept { return (ICmp(getData(), *S2) == 0); }

  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool strequ (const char *S2) const noexcept { return (Cmp(getData(), S2) == 0); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool strequ (const VStr &S2) const noexcept { return (Cmp(getData(), *S2) == 0); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool strequCI (const char *S2) const noexcept { return (ICmp(getData(), S2) == 0); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool strequCI (const VStr &S2) const noexcept { return (ICmp(getData(), *S2) == 0); }

  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool strEqu (const char *S2) const noexcept { return (Cmp(getData(), S2) == 0); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool strEqu (const VStr &S2) const noexcept { return (Cmp(getData(), *S2) == 0); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool strEquCI (const char *S2) const noexcept { return (ICmp(getData(), S2) == 0); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool strEquCI (const VStr &S2) const noexcept { return (ICmp(getData(), *S2) == 0); }

  VVA_CHECKRESULT bool StartsWith (const char *) const noexcept;
  VVA_CHECKRESULT bool StartsWith (const VStr &) const noexcept;
  VVA_CHECKRESULT bool EndsWith (const char *) const noexcept;
  VVA_CHECKRESULT bool EndsWith (const VStr &) const noexcept;

  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool startsWith (const char *s) const noexcept { return StartsWith(s); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool startsWith (const VStr &s) const noexcept { return StartsWith(s); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool endsWith (const char *s) const noexcept { return EndsWith(s); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool endsWith (const VStr &s) const noexcept { return EndsWith(s); }

  VVA_CHECKRESULT bool startsWithNoCase (const char *s) const noexcept;
  VVA_CHECKRESULT bool startsWithNoCase (const VStr &s) const noexcept;
  VVA_CHECKRESULT bool endsWithNoCase (const char *s) const noexcept;
  VVA_CHECKRESULT bool endsWithNoCase (const VStr &s) const noexcept;

  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool StartsWithNoCase (const char *s) const noexcept { return startsWithNoCase(s); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool StartsWithNoCase (const VStr &s) const noexcept { return startsWithNoCase(s); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool EndsWithNoCase (const char *s) const noexcept { return endsWithNoCase(s); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool EndsWithNoCase (const VStr &s) const noexcept { return endsWithNoCase(s); }

  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool StartsWithCI (const char *s) const noexcept { return startsWithNoCase(s); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool StartsWithCI (const VStr &s) const noexcept { return startsWithNoCase(s); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool EndsWithCI (const char *s) const noexcept { return endsWithNoCase(s); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool EndsWithCI (const VStr &s) const noexcept { return endsWithNoCase(s); }

  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool startsWithCI (const char *s) const noexcept { return startsWithNoCase(s); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool startsWithCI (const VStr &s) const noexcept { return startsWithNoCase(s); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool endsWithCI (const char *s) const noexcept { return endsWithNoCase(s); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool endsWithCI (const VStr &s) const noexcept { return endsWithNoCase(s); }

  static VVA_CHECKRESULT bool startsWith (const char *str, const char *part) noexcept;
  static VVA_CHECKRESULT bool endsWith (const char *str, const char *part) noexcept;
  static VVA_CHECKRESULT bool startsWithNoCase (const char *str, const char *part) noexcept;
  static VVA_CHECKRESULT bool endsWithNoCase (const char *str, const char *part) noexcept;

  static VVA_CHECKRESULT VVA_ALWAYS_INLINE bool StartsWith (const char *str, const char *part) noexcept { return startsWith(str, part); }
  static VVA_CHECKRESULT VVA_ALWAYS_INLINE bool SndsWith (const char *str, const char *part) noexcept { return endsWith(str, part); }
  static VVA_CHECKRESULT VVA_ALWAYS_INLINE bool startsWithCI (const char *str, const char *part) noexcept { return startsWithNoCase(str, part); }
  static VVA_CHECKRESULT VVA_ALWAYS_INLINE bool endsWithCI (const char *str, const char *part) noexcept { return endsWithNoCase(str, part); }

  VVA_CHECKRESULT VStr ToLower () const noexcept;
  VVA_CHECKRESULT VStr ToUpper () const noexcept;

  VVA_CHECKRESULT VVA_ALWAYS_INLINE VStr toLowerCase () const noexcept { return ToLower(); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE VStr toUpperCase () const noexcept { return ToUpper(); }

  VVA_CHECKRESULT bool isLowerCase () const noexcept;
  VVA_CHECKRESULT static bool isLowerCase (const char *s) noexcept;

  VVA_CHECKRESULT int IndexOf (char pch, int stpos=0) const noexcept;
  VVA_CHECKRESULT int IndexOf (const char *ps, int stpos=0) const noexcept;
  VVA_CHECKRESULT int IndexOf (const VStr &ps, int stpos=0) const noexcept;
  VVA_CHECKRESULT int LastIndexOf (char pch, int stpos=0) const noexcept;
  VVA_CHECKRESULT int LastIndexOf (const char *ps, int stpos=0) const noexcept;
  VVA_CHECKRESULT int LastIndexOf (const VStr &ps, int stpos=0) const noexcept;

  VVA_CHECKRESULT VVA_ALWAYS_INLINE int indexOf (char v, int stpos=0) const noexcept { return IndexOf(v, stpos); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE int indexOf (const char *v, int stpos=0) const noexcept { return IndexOf(v, stpos); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE int indexOf (const VStr &v, int stpos=0) const noexcept { return IndexOf(v, stpos); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE int lastIndexOf (char v, int stpos=0) const noexcept { return LastIndexOf(v, stpos); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE int lastIndexOf (const char *v, int stpos=0) const noexcept { return LastIndexOf(v, stpos); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE int lastIndexOf (const VStr &v, int stpos=0) const noexcept { return LastIndexOf(v, stpos); }

  VVA_CHECKRESULT VStr Replace (const char *Search, const char *Replacement) const noexcept;
  VVA_CHECKRESULT VVA_ALWAYS_INLINE VStr Replace (VStr Search, VStr Replacement) const noexcept { return Replace(*Search, *Replacement); }

  VVA_CHECKRESULT VVA_ALWAYS_INLINE VStr replace (const char *s0, const char *s1) const noexcept { return Replace(s0, s1); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE VStr replace (const VStr &s0, const VStr &s1) const noexcept { return Replace(*s0, *s1); }

  VVA_CHECKRESULT VStr Utf8Substring (int start, int len) const noexcept;
  VVA_CHECKRESULT VVA_ALWAYS_INLINE VStr utf8Substring (int start, int len) const noexcept { return Utf8Substring(start, len); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE VStr utf8substring (int start, int len) const noexcept { return Utf8Substring(start, len); }

  // split on char `c`
  // if `keepEmpty` is `true`, keep empty components
  // all slpitters will clear the array
  void Split (char c, TArray<VStr> &A, bool keepEmpty=false) const noexcept;
  void Split (const char *chars, TArray<VStr> &A, bool keepEmpty=false) const noexcept;
  void SplitOnBlanks (TArray<VStr> &A, bool doQuotedStrings=false) const noexcept;

  VVA_ALWAYS_INLINE void split (char c, TArray<VStr> &a, bool keepEmpty=false) const noexcept { return Split(c, a, keepEmpty); }
  VVA_ALWAYS_INLINE void split (const char *s, TArray<VStr> &a, bool keepEmpty=false) const noexcept { return Split(s, a, keepEmpty); }
  VVA_ALWAYS_INLINE void splitOnBlanks (TArray<VStr> &a, bool doQuotedStrings=false) const noexcept { return SplitOnBlanks(a, doQuotedStrings); }

  // split string to path components; first component can be '/', others has no slashes
  void SplitPath (TArray<VStr> &arr) const noexcept;
  VVA_ALWAYS_INLINE void splitPath (TArray<VStr> &arr) const noexcept { return SplitPath(arr); }

  static bool IsSplittedPathAbsolute (const TArray<VStr> &spp) noexcept;

  VVA_CHECKRESULT static bool isUtf8Valid (const char *s) noexcept;
  VVA_CHECKRESULT bool isUtf8Valid () const noexcept;

  VVA_CHECKRESULT VVA_ALWAYS_INLINE static bool IsUtf8Valid (const char *s) noexcept { return isUtf8Valid(s); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool IsUtf8Valid () const noexcept { return isUtf8Valid(); }

  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool IsValidUtf8 () const noexcept { return isUtf8Valid(); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool isValidUtf8 () const noexcept { return isUtf8Valid(); }

  VVA_CHECKRESULT VStr Latin1ToUtf8 () const noexcept;

  // serialisation operator
  VStream &Serialise (VStream &Strm);
  VStream &Serialise (VStream &Strm) const;

  // if `addQCh` is `true`, add '"' if something was quoted
  // if `forceQCh` is `true`, always add '"'
  VVA_CHECKRESULT VStr quote (bool addQCh=false, bool forceQCh=false) const noexcept;
  VVA_CHECKRESULT bool needQuoting () const noexcept;

  VVA_CHECKRESULT VVA_ALWAYS_INLINE VStr forceQuote () const noexcept { return quote(true, true); } // always adds quotes

  VVA_CHECKRESULT VStr xmlEscape () const noexcept;
  VVA_CHECKRESULT VStr xmlUnescape () const noexcept;

  // this translates "\\c" to `\c`
  VVA_CHECKRESULT VStr EvalEscapeSequences () const noexcept;

  VVA_CHECKRESULT VStr RemoveColors () const noexcept;
  VVA_CHECKRESULT bool MustBeSanitized () const noexcept;
  static VVA_CHECKRESULT bool MustBeSanitized (const char *str) noexcept;

  VVA_CHECKRESULT VStr ExtractFilePath () const noexcept;
  VVA_CHECKRESULT VStr ExtractFileName () const noexcept;
  VVA_CHECKRESULT VStr ExtractFileBase (bool doSysError=true) const noexcept; // this tries to get only name w/o extension, and calls `Sys_Error()` on too long names
  VVA_CHECKRESULT VStr ExtractFileBaseName () const noexcept;
  VVA_CHECKRESULT VStr ExtractFileExtension () const noexcept; // with a dot
  VVA_CHECKRESULT VStr StripExtension () const noexcept;
  VVA_CHECKRESULT VStr DefaultPath (VStr basepath) const noexcept;
  VVA_CHECKRESULT VStr DefaultExtension (VStr extension) const noexcept;
  VVA_CHECKRESULT VStr FixFileSlashes () const noexcept;
  VVA_CHECKRESULT VStr AppendTrailingSlash () const noexcept;
  VVA_CHECKRESULT VStr RemoveTrailingSlash () const noexcept;
  VVA_CHECKRESULT VStr AppendPath (const VStr &path) const noexcept;

  VVA_CHECKRESULT VVA_ALWAYS_INLINE VStr extractFilePath () const noexcept { return ExtractFilePath(); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE VStr extractFileName () const noexcept { return ExtractFileName(); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE VStr extractFileBase (bool doSysError=true) const noexcept { return ExtractFileBase(doSysError); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE VStr extractFileBaseName () const noexcept { return ExtractFileBaseName(); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE VStr extractFileExtension () const noexcept { return ExtractFileExtension(); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE VStr stripExtension () const noexcept { return StripExtension(); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE VStr defaultPath (VStr basepath) const noexcept { return DefaultPath(basepath); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE VStr defaultExtension (VStr extension) const noexcept { return DefaultExtension(extension); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE VStr fixSlashes () const noexcept { return FixFileSlashes(); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE VStr appendTrailingSlash () const noexcept { return AppendTrailingSlash(); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE VStr removeTrailingSlash () const noexcept { return RemoveTrailingSlash(); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE VStr appendPath (const VStr &path) const noexcept { return AppendPath(path); }

  // removes all blanks
  VVA_CHECKRESULT VStr trimRight () const noexcept;
  VVA_CHECKRESULT VStr trimLeft () const noexcept;
  VVA_CHECKRESULT VStr trimAll () const noexcept;

  // from my iv.strex
  VVA_CHECKRESULT VVA_ALWAYS_INLINE VStr xstrip () const noexcept { return trimAll(); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE VStr xstripleft () const noexcept { return trimLeft(); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE VStr xstripright () const noexcept { return trimRight(); }

  VVA_CHECKRESULT bool IsAbsolutePath () const noexcept;
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool isAbsolutePath () const noexcept { return IsAbsolutePath(); }

  VVA_CHECKRESULT static VVA_ALWAYS_INLINE bool IsPathSeparatorChar (const char ch) noexcept {
    #if !defined(_WIN32)
    return (ch == '/');
    #else
    return (ch == '/' || ch == '\\' || ch == ':');
    #endif
  }

  VVA_CHECKRESULT static VVA_ALWAYS_INLINE bool IsPathSeparatorCharStrict (const char ch) noexcept {
    #if !defined(_WIN32)
    return (ch == '/');
    #else
    return (ch == '/' || ch == '\\');
    #endif
  }

  // reject absolute names, names with ".", and names with "..", and names ends with path delimiter
  VVA_CHECKRESULT bool isSafeDiskFileName () const noexcept;

  static VVA_CHECKRESULT VVA_ALWAYS_INLINE int Length (const char *s) noexcept { return (s ? (int)strlen(s) : 0); }
  static VVA_CHECKRESULT VVA_ALWAYS_INLINE int length (const char *s) noexcept { return (s ? (int)strlen(s) : 0); }

  // calculates length of the following utf-8 sequence from its first char, or -1 for invalid first char
  // zero char means zero length
  static VVA_CHECKRESULT inline int CalcUtf8CharByteSize (const char ch) noexcept {
    if (!ch) return 0;
    if ((vuint8)ch < 128) return 1;
    if (((vuint8)ch&0xe0) == 0xc0) return 2;
    if (((vuint8)ch&0xf0) == 0xe0) return 3;
    if (((vuint8)ch&0xf8) == 0xe8) return 4;
    // overlongs are not supported
    //if (((vuint8)ch&0xfc) == 0xf8) return 5;
    //if (((vuint8)ch&0xfe) == 0xfc) return 6;
    return -1; // invalid utf-8
  }

  static VVA_CHECKRESULT VVA_ALWAYS_INLINE bool IsValidUtf8Continuation (const char ch) noexcept { return (((vuint8)ch&0xc0) == 0x80); }

  static VVA_CHECKRESULT VVA_ALWAYS_INLINE bool isValidCodepoint (int c) noexcept { return ((c >= 0 && c < 0xD800) || (c > 0xDFFF && c <= 0x10FFFF)); } // is given codepoint valid?

  static VVA_CHECKRESULT int Utf8Length (const char *s, int len=-1) noexcept;
  static VVA_CHECKRESULT VVA_ALWAYS_INLINE int utf8Length (const char *s, int len=-1) noexcept { return (int)Utf8Length(s, len); }
  static VVA_CHECKRESULT size_t ByteLengthForUtf8 (const char *s, size_t N) noexcept; // `N` must be valid!
  // get utf8 char; advances pointer, returns '?' on invalid char, or 0 on string end
  static VVA_CHECKRESULT int Utf8GetChar (const char *&s) noexcept;
  static VVA_CHECKRESULT VStr FromUtf8Char (int) noexcept;

  static VVA_CHECKRESULT VVA_ALWAYS_INLINE int Cmp (const char *S1, const char *S2) noexcept { return (S1 == S2 ? 0 : strcmp((S1 ? S1 : ""), (S2 ? S2 : ""))); }
  static VVA_CHECKRESULT VVA_ALWAYS_INLINE int NCmp (const char *S1, const char *S2, size_t N) noexcept { return (S1 == S2 ? 0 : strncmp((S1 ? S1 : ""), (S2 ? S2 : ""), N)); }

  static VVA_CHECKRESULT int ICmp (const char *s0, const char *s1) noexcept;
  static VVA_CHECKRESULT int NICmp (const char *s0, const char *s1, size_t max) noexcept;

  static VVA_CHECKRESULT VVA_ALWAYS_INLINE bool strequ (const char *S1, const char *S2) noexcept { return (Cmp(S1, S2) == 0); }
  static VVA_CHECKRESULT VVA_ALWAYS_INLINE bool strequCI (const char *S1, const char *S2) noexcept { return (ICmp(S1, S2) == 0); }
  static VVA_CHECKRESULT VVA_ALWAYS_INLINE bool nstrequ (const char *S1, const char *S2, size_t max) noexcept { return (NCmp(S1, S2, max) == 0); }
  static VVA_CHECKRESULT VVA_ALWAYS_INLINE bool nstrequCI (const char *S1, const char *S2, size_t max) noexcept { return (NICmp(S1, S2, max) == 0); }

  static VVA_CHECKRESULT VVA_ALWAYS_INLINE bool StrEqu (const char *S1, const char *S2) noexcept { return (Cmp(S1, S2) == 0); }
  static VVA_CHECKRESULT VVA_ALWAYS_INLINE bool StrEquCI (const char *S1, const char *S2) noexcept { return (ICmp(S1, S2) == 0); }
  static VVA_CHECKRESULT VVA_ALWAYS_INLINE bool NStrEqu (const char *S1, const char *S2, size_t max) noexcept { return (NCmp(S1, S2, max) == 0); }
  static VVA_CHECKRESULT VVA_ALWAYS_INLINE bool NStrEquCI (const char *S1, const char *S2, size_t max) noexcept { return (NICmp(S1, S2, max) == 0); }

  static VVA_CHECKRESULT VVA_ALWAYS_INLINE bool strEqu (const char *S1, const char *S2) noexcept { return (Cmp(S1, S2) == 0); }
  static VVA_CHECKRESULT VVA_ALWAYS_INLINE bool strEquCI (const char *S1, const char *S2) noexcept { return (ICmp(S1, S2) == 0); }
  static VVA_CHECKRESULT VVA_ALWAYS_INLINE bool nstrEqu (const char *S1, const char *S2, size_t max) noexcept { return (NCmp(S1, S2, max) == 0); }
  static VVA_CHECKRESULT VVA_ALWAYS_INLINE bool nstrEquCI (const char *S1, const char *S2, size_t max) noexcept { return (NICmp(S1, S2, max) == 0); }

  static VVA_ALWAYS_INLINE void Cpy (char *dst, const char *src) noexcept {
    if (dst) { if (src) strcpy(dst, src); else *dst = 0; }
  }

  // will write terminating zero; buffer should be at leasn [N+1] bytes long
  static inline void NCpy (char *dst, const char *src, size_t N) noexcept {
    if (dst && src && N && src[0]) {
      size_t slen = strlen(src);
      if (slen > N) slen = N;
      memcpy(dst, src, slen);
      dst[slen] = 0;
    } else {
      if (dst) *dst = 0;
    }
  }

  static VVA_CHECKRESULT VVA_ALWAYS_INLINE char ToUpper (char c) noexcept { return (c >= 'a' && c <= 'z' ? c-32 : c); }
  static VVA_CHECKRESULT VVA_ALWAYS_INLINE char ToLower (char c) noexcept { return (c >= 'A' && c <= 'Z' ? c+32 : c); }

  static VVA_CHECKRESULT VVA_ALWAYS_INLINE char toupper (char c) noexcept { return (c >= 'a' && c <= 'z' ? c-32 : c); }
  static VVA_CHECKRESULT VVA_ALWAYS_INLINE char tolower (char c) noexcept { return (c >= 'A' && c <= 'Z' ? c+32 : c); }

  // append codepoint to this string, in utf-8
  VStr &utf8Append (vuint32 code) noexcept;

  VVA_CHECKRESULT VStr utf2win () const noexcept;
  VVA_CHECKRESULT VStr win2utf () const noexcept;

  VVA_CHECKRESULT VStr utf2koi () const noexcept;
  VVA_CHECKRESULT VStr koi2utf () const noexcept;

  VVA_CHECKRESULT VStr toLowerCase1251 () const noexcept;
  VVA_CHECKRESULT VStr toUpperCase1251 () const noexcept;

  VVA_CHECKRESULT VStr toLowerCaseKOI () const noexcept;
  VVA_CHECKRESULT VStr toUpperCaseKOI () const noexcept;

  VVA_CHECKRESULT inline bool equ1251CI (const VStr &s) const noexcept {
    size_t slen = (size_t)length();
    if (slen != (size_t)s.length()) return false;
    for (size_t f = 0; f < slen; ++f) if (locase1251(getData()[f]) != locase1251(s[f])) return false;
    return true;
  }

  VVA_CHECKRESULT inline bool equ1251CI (const char *s) const noexcept {
    size_t slen = length();
    if (!s || !s[0]) return (slen == 0);
    if (slen != strlen(s)) return false;
    for (size_t f = 0; f < slen; ++f) if (locase1251(getData()[f]) != locase1251(s[f])) return false;
    return true;
  }

  VVA_CHECKRESULT inline bool equKOICI (const VStr &s) const noexcept {
    size_t slen = (size_t)length();
    if (slen != (size_t)s.length()) return false;
    for (size_t f = 0; f < slen; ++f) if (locaseKOI(getData()[f]) != locaseKOI(s[f])) return false;
    return true;
  }

  VVA_CHECKRESULT inline bool equKOICI (const char *s) const noexcept {
    size_t slen = length();
    if (!s || !s[0]) return (slen == 0);
    if (slen != strlen(s)) return false;
    for (size_t f = 0; f < slen; ++f) if (locaseKOI(getData()[f]) != locaseKOI(s[f])) return false;
    return true;
  }

  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool fnameEqu1251CI (const VStr &s) const noexcept { return fnameEqu1251CI(s.getData()); }
  VVA_CHECKRESULT bool fnameEqu1251CI (const char *s) const noexcept;

  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool fnameEquKOICI (const VStr &s) const noexcept { return fnameEquKOICI(s.getData()); }
  VVA_CHECKRESULT bool fnameEquKOICI (const char *s) const noexcept;

  static VVA_CHECKRESULT VStr buf2hex (const void *buf, int buflen) noexcept;

  VVA_ALWAYS_INLINE bool convertInt (int *outv) const noexcept { return convertInt(getCStr(), outv); }
  VVA_ALWAYS_INLINE bool convertFloat (float *outv) const noexcept { return convertFloat(getCStr(), outv); }

  static VVA_CHECKRESULT bool globmatch (const char *str, const char *pat, bool caseSensitive=true) noexcept;
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool globmatch (const char *pat, bool caseSensitive=true) const noexcept { return globmatch(getData(), pat, caseSensitive); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool globmatch (const VStr &pat, bool caseSensitive=true) const noexcept { return globmatch(getData(), *pat, caseSensitive); }

  static VVA_CHECKRESULT VVA_ALWAYS_INLINE bool globMatch (const char *str, const char *pat, bool caseSensitive=true) noexcept { return globmatch(str, pat, caseSensitive); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool globMatch (const char *pat, bool caseSensitive=true) const noexcept { return globmatch(getData(), pat, caseSensitive); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool globMatch (const VStr &pat, bool caseSensitive=true) const noexcept { return globmatch(getData(), *pat, caseSensitive); }

  static VVA_CHECKRESULT VVA_ALWAYS_INLINE bool globmatchCI (const char *str, const char *pat) noexcept { return globmatch(str, pat, false); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool globmatchCI (const char *pat) const noexcept { return globmatch(getData(), pat, false); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool globmatchCI (const VStr &pat) const noexcept { return globmatch(getData(), *pat, false); }

  static VVA_CHECKRESULT VVA_ALWAYS_INLINE bool globMatchCI (const char *str, const char *pat) noexcept { return globmatch(str, pat, false); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool globMatchCI (const char *pat) const noexcept { return globmatch(getData(), pat, false); }
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool globMatchCI (const VStr &pat) const noexcept { return globmatch(getData(), *pat, false); }

  // will not clear `args`
  void Tokenise (TArray <VStr> &args) const noexcept;
  VVA_ALWAYS_INLINE void Tokenize (TArray <VStr> &args) const noexcept { Tokenise(args); }
  VVA_ALWAYS_INLINE void tokenise (TArray <VStr> &args) const noexcept { Tokenise(args); }
  VVA_ALWAYS_INLINE void tokenize (TArray <VStr> &args) const noexcept { Tokenise(args); }

  // this finds start of the next command
  // commands are ';'-delimited
  // processes quotes as `tokenise` does
  // skips all leading spaces by default (i.e. result can be >0 even if there are no ';')
  int findNextCommand (int stpos=0, bool skipLeadingSpaces=true) const noexcept;

  // returns a value from zero to one indicating an approximate
  // percentage of how closely two strings match
  // pass `-1` as any length to use `strlen()`
  // case difference matters too
  // this algo is not a speed demon, but it does the work
  static float ApproxMatch (const char *left, int leftlen, const char *right, int rightlen) noexcept;

  float VVA_ALWAYS_INLINE ApproxMatch (const char *right, int rightlen=-1) noexcept { return ApproxMatch(getData(), length(), right, rightlen); }
  float VVA_ALWAYS_INLINE ApproxMatch (VStr right) noexcept { return ApproxMatch(getData(), length(), right.getData(), right.length()); }

  float VVA_ALWAYS_INLINE approxMatch (const char *right, int rightlen=-1) noexcept { return ApproxMatch(getData(), length(), right, rightlen); }
  float VVA_ALWAYS_INLINE approxMatch (VStr right) noexcept { return ApproxMatch(getData(), length(), right.getData(), right.length()); }

public:
  static VVA_CHECKRESULT VVA_ALWAYS_INLINE char wchar2win (vuint32 wc) noexcept { return (wc < 65536 ? wc2shitmap[wc] : '?'); }
  static VVA_CHECKRESULT VVA_ALWAYS_INLINE char wchar2koi (vuint32 wc) noexcept { return (wc < 65536 ? wc2koimap[wc] : '?'); }

  static VVA_CHECKRESULT VVA_ALWAYS_INLINE VVA_CONST int digitInBase (char ch, int base=10) noexcept {
    if (base < 1 || base > 36 || ch < '0') return -1;
    if (base <= 10) return (ch < 48+base ? ch-48 : -1);
    if (ch >= '0' && ch <= '9') return ch-48;
    if (ch >= 'a' && ch <= 'z') ch -= 32; // poor man tolower()
    if (ch < 'A' || ch >= 65+(base-10)) return -1;
    return ch-65+10;
  }

  static VVA_CHECKRESULT VVA_ALWAYS_INLINE VVA_CONST char upcase1251 (char ch) noexcept {
    if ((vuint8)ch < 128) return ch-(ch >= 'a' && ch <= 'z' ? 32 : 0);
    if ((vuint8)ch >= 224 /*&& (vuint8)ch <= 255*/) return (vuint8)ch-32;
    if ((vuint8)ch == 184 || (vuint8)ch == 186 || (vuint8)ch == 191) return (vuint8)ch-16;
    if ((vuint8)ch == 162 || (vuint8)ch == 179) return (vuint8)ch-1;
    return ch;
  }

  static VVA_CHECKRESULT VVA_ALWAYS_INLINE VVA_CONST char locase1251 (char ch) noexcept {
    if ((vuint8)ch < 128) return ch+(ch >= 'A' && ch <= 'Z' ? 32 : 0);
    if ((vuint8)ch >= 192 && (vuint8)ch <= 223) return (vuint8)ch+32;
    if ((vuint8)ch == 168 || (vuint8)ch == 170 || (vuint8)ch == 175) return (vuint8)ch+16;
    if ((vuint8)ch == 161 || (vuint8)ch == 178) return (vuint8)ch+1;
    return ch;
  }

  static VVA_CHECKRESULT VVA_ALWAYS_INLINE VVA_CONST bool isAlpha2151 (char ch) noexcept {
    if (ch >= 'A' && ch <= 'Z') return true;
    if (ch >= 'a' && ch <= 'z') return true;
    if ((vuint8)ch >= 191) return true;
    switch ((vuint8)ch) {
      case 161: case 162: case 168: case 170: case 175:
      case 178: case 179: case 184: case 186:
        return true;
    }
    return false;
  }

  /* koi8-u */
  static VVA_CHECKRESULT VVA_ALWAYS_INLINE VVA_CONST int locaseKOI (char ch) noexcept {
    if ((vuint8)ch < 128) {
      if (ch >= 'A' && ch <= 'Z') ch += 32;
    } else {
      if ((vuint8)ch >= 224 /*&& (vuint8)ch <= 255*/) ch -= 32;
      else {
        switch ((vuint8)ch) {
          case 179: case 180: case 182: case 183: case 189: ch -= 16; break;
        }
      }
    }
    return ch;
  }

  static VVA_CHECKRESULT VVA_ALWAYS_INLINE VVA_CONST int upcaseKOI (char ch) noexcept {
    if ((vuint8)ch < 128) {
      if (ch >= 'a' && ch <= 'z') ch -= 32;
    } else {
      if ((vuint8)ch >= 192 && (vuint8)ch <= 223) ch += 32;
      else {
        switch ((vuint8)ch) {
          case 163: case 164: case 166: case 167: case 173: ch += 16; break;
        }
      }
    }
    return ch;
  }

  static VVA_CHECKRESULT VVA_ALWAYS_INLINE VVA_CONST bool isAlphaKOI (char ch) noexcept {
    if (ch >= 'A' && ch <= 'Z') return true;
    if (ch >= 'a' && ch <= 'z') return true;
    if ((vuint8)ch >= 192) return true;
    switch ((vuint8)ch) {
      case 163: case 164: case 166: case 167: case 173:
      case 179: case 180: case 182: case 183: case 189:
        return true;
    }
    return false;
  }

  static VVA_CHECKRESULT VVA_ALWAYS_INLINE VVA_CONST bool isAlphaAscii (char ch) noexcept {
    return
      (ch >= 'A' && ch <= 'Z') ||
      (ch >= 'a' && ch <= 'z');
  }

  // returns length of the following utf-8 sequence from its first char, or -1 for invalid first char
  /*
  static VVA_CHECKRESULT VVA_ALWAYS_INLINE VVA_CONST int utf8CodeLen (char ch) noexcept {
    if ((vuint8)ch < 0x80) return 1;
    if ((ch&0xFE) == 0xFC) return 6;
    if ((ch&0xFC) == 0xF8) return 5;
    if ((ch&0xF8) == 0xF0) return 4;
    if ((ch&0xF0) == 0xE0) return 3;
    if ((ch&0xE0) == 0xC0) return 2;
    return -1; // invalid
  }
  */

  /*
  static VVA_CHECKRESULT VVA_ALWAYS_INLINE bool isPathDelimiter (const char ch) noexcept {
    #ifdef _WIN32
      return (ch == '/' || ch == '\\' || ch == ':');
    #else
      return (ch == '/');
    #endif
  }

  static VVA_CHECKRESULT VVA_ALWAYS_INLINE bool IsPathDelimiter (const char ch) noexcept { return isPathDelimiter(ch); }
  */

  static VVA_CHECKRESULT bool isSafeDiskFileName (const VStr &fname) noexcept { return fname.isSafeDiskFileName(); }

public:

  VStr_ByCharIterator begin () noexcept;
  const VStr_ByCharIterator begin () const noexcept;
  VStr_ByCharIterator end () noexcept;
  const VStr_ByCharIterator end () const noexcept;

public:
  static const vuint16 cp1251[128];
  static char wc2shitmap[65536];

  static const vuint16 cpKOI[128];
  static char wc2koimap[65536];

  static const VStr EmptyString;

public:
  static inline VVA_OKUNUSED VVA_CHECKRESULT
  VStr size2human (vuint32 size) noexcept {
         if (size < 1024*1024) return va("%u%s KB", size/1024, (size%1024 >= 512 ? ".5" : ""));
    else if (size < 1024*1024*1024) return va("%u%s MB", size/(1024*1024), (size%(1024*1024) >= 1024 ? ".5" : ""));
    else return va("%u%s GB", size/(1024*1024*1024), (size%(1024*1024*1024) >= 1024*1024 ? ".5" : ""));
  }
};


struct VStr_ByCharIterator {
public:
  VStr s;
  int index;

public:
  VVA_ALWAYS_INLINE VStr_ByCharIterator (VStr st) noexcept : s(st), index(0) {}
  VVA_ALWAYS_INLINE VStr_ByCharIterator (VStr st, bool) noexcept : s(st), index(-1) {}

  VVA_ALWAYS_INLINE bool operator == (const VStr_ByCharIterator &other) const noexcept { return ((index < 0 && other.index < 0) || (s.getData() == other.s.getData() && index == other.index)); }
  VVA_ALWAYS_INLINE bool operator != (const VStr_ByCharIterator &other) const noexcept { return !operator==(other); }
  VVA_ALWAYS_INLINE char operator * () const noexcept { return (index >= 0 && index < s.length() ? s.getData()[(unsigned)index] : 0); } /* required for iterator */
  VVA_ALWAYS_INLINE void operator ++ () noexcept { if (index >= 0) { if (++index >= s.length()) index = -1; } } /* this is enough for iterator */
};


// results MUST be equal
VVA_ALWAYS_INLINE VVA_PURE uint32_t GetTypeHash (const char *s) noexcept { return vvHashStrZ(s); }
VVA_ALWAYS_INLINE VVA_PURE uint32_t GetTypeHash (const VStr &s) noexcept { return vvHashStrZ(*s); }


// ////////////////////////////////////////////////////////////////////////// //
struct VUtf8DecoderFast {
public:
  enum {
    Replacement = 0xFFFD, // replacement char for invalid unicode
    Accept = 0,
    Reject = 12,
  };

  static VVA_CHECKRESULT VVA_ALWAYS_INLINE bool isValidCodepoint (int c) noexcept { return ((c >= 0 && c < 0xD800) || (c > 0xDFFF && c <= 0x10FFFF)); } // is given codepoint valid?

protected:
  uint32_t state;

public:
  uint32_t codepoint; // decoded codepoint (valid only when decoder is in "complete" state)

public:
  VVA_ALWAYS_INLINE VUtf8DecoderFast () noexcept : state(Accept), codepoint(0) {}

  VVA_ALWAYS_INLINE VUtf8DecoderFast &operator = (const VUtf8DecoderFast &other) noexcept = default;

  VVA_ALWAYS_INLINE void reset () noexcept { state = Accept; codepoint = 0; }

  // is current character valid and complete? take `codepoint` then
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool complete () const noexcept { return (state == Accept); }
  // is current character invalid and complete? take `Replacement` then
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool invalid () const noexcept { return (state == Reject); }
  // is current character complete (valid or invaluid)? take `codepoint` then
  VVA_CHECKRESULT VVA_ALWAYS_INLINE bool hasCodePoint () const noexcept { return (state == Accept || state == Reject); }

  // process another input byte; returns `true` if codepoint is complete
  VVA_ALWAYS_INLINE bool put (const uint8_t c) noexcept {
    if (state == Reject) { state = Accept; codepoint = 0; } // restart from invalid state
    uint8_t tp = utf8dfa[c];
    codepoint = (state != Accept ? (c&0x3f)|(codepoint<<6) : (0xff>>tp)&c);
    state = utf8dfa[256+state+tp];
    if (state == Reject) codepoint = Replacement;
    return (state == Accept || state == Reject);
  }

protected:
  static const uint8_t utf8dfa[0x16c];
};

// required for Vavoom C VM
static_assert(sizeof(VStr) == sizeof(void *), "oops");
