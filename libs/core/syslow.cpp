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
//**  Copyright (C) 2018-2022 Ketmar Dark
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
//**  Low-level OS-dependent functions
//**
//**************************************************************************
#include "core.h"

#ifdef ANDROID
#  include <android/asset_manager.h>
#  include <android/asset_manager_jni.h>
#  include <SDL.h>
#  include <jni.h>
#endif

#ifdef __linux__
# ifndef ANDROID
#  ifndef _GNU_SOURCE
#   define _GNU_SOURCE
#  endif
/*#  include <pthread.h>*/
#  include <sched.h>
# endif
#endif

/*
  static void *operator new (size_t size);
  static void *operator new[] (size_t size);
  static void operator delete (void *p);
  static void operator delete[] (void *p);
*/
#include <new>

// this will give us a constant time precision
#define timeOffset  (4294967296000.0)
double Sys_Time_Offset () { return timeOffset/1000.0; }

#if 0
moved to zone
void *operator new [] (std::size_t s) noexcept(false) /*throw(std::bad_alloc)*/ {
  //fprintf(stderr, "new[]\n");
  return Z_Calloc(s);
}


void operator delete [] (void *p) throw () {
  //fprintf(stderr, "delete[]\n");
  Z_Free(p);
}
#endif


static VStr sys_NormalizeUserName (const char *s) {
  if (!s) return VStr("PLAYER").makeImmutableRetSelf();
  while (*s && (unsigned char)(*s) <= ' ') ++s;
  if (!s[0]) return VStr("PLAYER").makeImmutableRetSelf();
  VStr res;
  while (*s) {
    char ch = *s++;
    if ((unsigned char)ch <= ' ' || ch >= 127) ch = '_';
    res += ch;
  }
  while (res.length() && (unsigned char)(res[res.length()-1]) <= ' ') res.chopRight(1);
  if (res.length() == 0) return VStr("PLAYER").makeImmutableRetSelf();
  return res;
}


#ifndef WIN32
#define CANON_PATH_DELIM  '/'
static inline bool isPathDelimiter (char ch) noexcept { return (ch == '/'); }
static inline bool isRootPath (const VStr &s) noexcept { return (s.length() > 0 && s[0] == '/'); }
static inline int getRootPathLength (const VStr &s) noexcept { return (s.length() > 0 && s[0] == '/' ? 1 : 0); }
#else
//#define CANON_PATH_DELIM  '\\'
#define CANON_PATH_DELIM  '/'
static inline bool isPathDelimiter (char ch) noexcept { return (ch == '/' || ch == '\\'); }
static inline bool isRootPath (const VStr &s) noexcept {
  return
   (s.length() > 0 && (s[0] == '/' || s[0] == '\\')) ||
   (s.length() > 2 && s[1] == ':' && (s[2] == '/' || s[2] == '\\'));
}
static inline int getRootPathLength (const VStr &s) noexcept {
  return
   s.length() > 0 && (s[0] == '/' || s[0] == '\\') ? 1 :
   s.length() > 2 && s[1] == ':' && (s[2] == '/' || s[2] == '\\') ? 3 : 0;
}
#endif

#ifdef ANDROID
static inline bool isApkPath (const VStr &s) noexcept {
  return s.length() >= 1 && (s[0] == '.') && (s.length() == 1 || s[1] == '/');
}
static inline VStr getApkPath (const VStr &s) noexcept {
  return isApkPath(s) ? VStr(s, 2, s.length() - 2) : VStr();
}
static AAssetManager *androidAssetManager;
static void getApkAssetManager () {
  if (androidAssetManager == nullptr) {
    JNIEnv *env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    assert(env != nullptr);
    jobject activity = (jobject)SDL_AndroidGetActivity();
    assert(activity != nullptr);
    jclass clazz(env->GetObjectClass(activity));
    //jclass clazz(env->FindClass("android/view/ContextThemeWrapper"));
    assert(clazz != nullptr);
    jmethodID method = env->GetMethodID(clazz, "getAssets", "()Landroid/content/res/AssetManager;");
    assert(method != nullptr);
    jobject assetManager = env->CallObjectMethod(activity, method);
    assert(assetManager != nullptr);
    androidAssetManager = AAssetManager_fromJava(env, assetManager);
    env->DeleteLocalRef(activity);
    env->DeleteLocalRef(clazz);
  }
}

static bool tryApkFile (VStr path) {
  if (isApkPath(path)) {
    getApkAssetManager();
    if (androidAssetManager != nullptr) {
      VStr s = getApkPath(path);
      AAsset *a = AAssetManager_open(androidAssetManager, *s, AASSET_MODE_STREAMING);
      if (a != nullptr) {
        AAsset_close(a);
        return true;
      }
    }
  }
  return false;
}

static bool tryApkDir (VStr path) {
  if (isApkPath(path)) {
    getApkAssetManager();
    if (androidAssetManager != nullptr) {
      VStr s = getApkPath(path);
      AAssetDir* d = AAssetManager_openDir(androidAssetManager, *s);
      if (d != nullptr) {
        AAssetDir_close(d);
        return true;
      }
    }
  }
  return false;
}

#endif


//#define SFFCI_FULL_SCAN_DEBUG

#ifdef SFFCI_FULL_SCAN
# undef SFFCI_FULL_SCAN
#endif
#ifdef WIN32
# define SFFCI_FULL_SCAN
#endif
/*
#ifndef SFFCI_FULL_SCAN
# define SFFCI_FULL_SCAN
#endif
*/

//==========================================================================
//
//  Sys_FindFileCI
//
//==========================================================================
VStr Sys_FindFileCI (VStr path, bool lastIsDir) {
  VStr realpath;

  #ifdef SFFCI_FULL_SCAN_DEBUG
  GLog.Logf(NAME_Debug, "Sys_FindFileCI: path=<%s> (lastIsDir=%d)", *path, (int)lastIsDir);
  #endif
  if (path.length() == 0) return realpath;
  if (isRootPath(path)) {
    realpath = path.left(getRootPathLength(path));
    path.chopLeft(getRootPathLength(path));
    #ifdef WIN32
    // hack for UNC
    if (realpath.length() == 1 && path.length() && isPathDelimiter(path[0])) {
      realpath += path[0];
      path.chopLeft(1);
    } else {
      for (int f = 0; f < realpath.length(); ++f) if (isPathDelimiter(realpath[f])) realpath.getMutableCStr()[f] = CANON_PATH_DELIM;
    }
    #endif
    while (path.length() && isPathDelimiter(path[0])) path.chopLeft(1);
  }

  while (path.length() && isPathDelimiter(path[path.length()-1])) path.chopRight(1);
  if (path.length() == 0) return realpath;

  #ifdef SFFCI_FULL_SCAN_DEBUG
  GLog.Logf(NAME_Debug, " realpath=<%s>; path=<%s>", *realpath, *path);
  #endif
  while (path.length()) {
    if (isPathDelimiter(path[0])) {
      //if (realpath.length() && !isPathDelimiter(realpath[0])) realpath += path[0];
      path.chopLeft(1);
      continue;
    }
    // get path component
    int pos = 0;
    while (pos < path.length() && !isPathDelimiter(path[pos])) ++pos;
    VStr ppart = path.left(pos);
    path.chopLeft(pos+1);
    #ifdef SFFCI_FULL_SCAN_DEBUG
    GLog.Logf(NAME_Debug, "  realpath=<%s>; path=<%s>; ppart=<%s>", *realpath, *path, *ppart);
    #endif
    // ignore "current dir"
    if (ppart == ".") continue;
    // process "upper dir" in-place
    if (ppart == "..") {
      int start = (isRootPath(realpath) ? getRootPathLength(realpath) : 0);
      int end = realpath.length();
      while (end > start && !isPathDelimiter(realpath[end])) --end;
      if (start == 0 && end > 0 && isPathDelimiter(realpath[end-1])) --end;
      realpath = realpath.left(end);
      #ifdef SFFCI_FULL_SCAN_DEBUG
      GLog.Logf(NAME_Debug, "    updir: realpath=<%s>", *realpath);
      #endif
      continue;
    }
    // add to real path
    if (realpath.length() && !isPathDelimiter(realpath[realpath.length()-1])) realpath += CANON_PATH_DELIM;
    //VStr prevpath = realpath;
    //realpath += ppart;
    const bool wantDir = (lastIsDir || !path.isEmpty());
    // check "as-is"
    #ifndef SFFCI_FULL_SCAN
    VStr newpath = realpath+ppart;
    //if (Sys_FileTime(newpath) != -1) { realpath = newpath; continue; } // i found her!
    bool okname = (wantDir ? Sys_DirExists(newpath) : Sys_FileExists(newpath));
    if (okname) { realpath = newpath; continue; } // i found her!
    // check for all-upper
    VStr npx = realpath+ppart.toUpperCase();
    if (npx != newpath) {
      okname = (wantDir ? Sys_DirExists(npx) : Sys_FileExists(npx));
      if (okname) { realpath = npx; continue; } // i found her!
    }
    // check for all-lower
    npx = realpath+ppart.toLowerCase();
    if (npx != newpath) {
      okname = (wantDir ? Sys_DirExists(npx) : Sys_FileExists(npx));
      if (okname) { realpath = npx; continue; } // i found her!
    }
    // split to name and extension, and check for all upper/all lower separately
    VStr ppExt = ppart.extractFileExtension();
    if (!ppExt.isEmpty() && ppExt != ".") {
      VStr ppBase = ppart.stripExtension();
      for (unsigned f = 0; f < 4; ++f) {
        npx = realpath;
        if (f&1) npx += ppBase.toUpperCase(); else npx += ppBase.toLowerCase();
        if (f&2) npx += ppExt.toUpperCase(); else npx += ppExt.toLowerCase();
        if (npx != newpath) {
          okname = (wantDir ? Sys_DirExists(npx) : Sys_FileExists(npx));
          if (okname) { realpath = npx; break; } // i found her!
        }
      }
      if (okname) continue;
    }
    #endif
    // scan directory
    void *dh = Sys_OpenDir((realpath.length() ? realpath : VStr(".")), wantDir);
    if (!dh) {
      #ifdef SFFCI_FULL_SCAN_DEBUG
      GLog.Logf(NAME_Debug, "     CANNOT OPEN DIR <%s>", *(realpath.length() ? realpath : VStr(".")));
      #endif
      return VStr::EmptyString; // alas
    }
    #ifdef SFFCI_FULL_SCAN
    VStr inexactName;
    vassert(inexactName.length() == 0);
    #endif
    for (;;) {
      VStr fname = Sys_ReadDir(dh);
      if (fname.length() == 0) {
        #ifdef SFFCI_FULL_SCAN
        if (inexactName.length()) break;
        #endif
        // alas
        Sys_CloseDir(dh);
        return VStr::EmptyString;
      }
      #ifdef SFFCI_FULL_SCAN_DEBUG
      GLog.Logf(NAME_Debug, "   fname=<%s>; ppart=<%s>; wantDir=%d", *fname, *ppart, (int)wantDir);
      #endif
      VStr diskName;
      if (wantDir) {
        if (!fname.endsWith("/")) continue;
        diskName = fname.left(fname.length()-1);
      } else {
        if (fname.endsWith("/")) continue; // just in case
        diskName = fname;
      }
      #ifdef SFFCI_FULL_SCAN
      if (ppart == diskName) { inexactName.clear(); break; }
      if (inexactName.length() == 0 && ppart.strEquCI(diskName)) inexactName = diskName;
      #else
      if (ppart.strEquCI(diskName)) { realpath += diskName; break; }
      #endif
    }
    Sys_CloseDir(dh);
    #ifdef SFFCI_FULL_SCAN
      #ifdef SFFCI_FULL_SCAN_DEBUG
      GLog.Logf(NAME_Debug, "   inexactName: <%s>", *inexactName);
      #endif
    if (inexactName.length() != 0) realpath += inexactName; else realpath += ppart;
    #endif
    #ifdef SFFCI_FULL_SCAN_DEBUG
    GLog.Logf(NAME_Debug, "  sce: realpath=<%s>; path=<%s>; ppart=<%s>", *realpath, *path, *ppart);
    #endif
  }
  vassert(path.length() == 0);
  #ifdef SFFCI_FULL_SCAN_DEBUG
  GLog.Logf(NAME_Debug, " done: realpath=<%s>", *realpath);
  #endif
  if (lastIsDir) {
    if (!Sys_DirExists(realpath)) return VStr::EmptyString; // oops
  } else {
    if (!Sys_FileExists(realpath)) return VStr::EmptyString; // oops
  }
  #ifdef SFFCI_FULL_SCAN_DEBUG
  GLog.Logf(NAME_Debug, " exit: realpath=<%s>", *realpath);
  #endif
  return realpath;
}


#ifndef WIN32
// normal OS
#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <pwd.h>
#include <sys/stat.h>
#if !defined(__SWITCH__) && !defined(__CYGWIN__)
# include <sys/syscall.h>   /* For SYS_xxx definitions */
#endif
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <utime.h>
/*
#ifdef _POSIX_PRIORITY_SCHEDULING
# include <sched.h>
# define HAVE_YIELD
#else
# warning "*** NO sched_yield() found! ***"
#endif
*/

struct DirInfo {
  DIR *dh;
  VStr path; // with slash
  bool wantDirs;
#ifdef ANDROID
  AAssetDir *adir;
#endif
};


enum {
  FT_Invalid,
  FT_Dir,
  FT_Regular,
};


//==========================================================================
//
//  GetFileType
//
//==========================================================================
static int GetFileType (VStr filename) noexcept {
  if (filename.isEmpty()) return FT_Invalid;
  if (filename.strEqu(".") || filename.strEqu("..")) return FT_Dir;
  #ifdef ANDROID
  if (isApkPath(filename)) {
    if (tryApkFile(filename)) return FT_Regular;
    if (tryApkDir(filename)) return FT_Dir;
  }
  #endif
  struct stat st;
  if (stat(*filename, &st) == -1) return FT_Invalid;
  if (S_ISREG(st.st_mode) != 0) return FT_Regular;
  if (S_ISDIR(st.st_mode) != 0) return FT_Dir;
  #if 0
  // debug
  VStr ss;
  unsigned n = (unsigned)st.st_mode;
  do {
    const char ch = n%8+'0';
    ss = VStr(ch)+ss;
  } while ((n /= 8) != 0);
  GLog.Logf(NAME_Debug, "***GetFileType: '%s' -> 0%s", *filename, *ss);
  #endif
  return FT_Invalid;
}


//==========================================================================
//
//  isRegularFile
//
//==========================================================================
static bool isRegularFile (VStr filename) {
  const int ftp = GetFileType(filename);
  return (ftp == FT_Regular);
}


//==========================================================================
//
//  isDirectory
//
//==========================================================================
static bool isDirectory (VStr filename) {
  const int ftp = GetFileType(filename);
  return (ftp == FT_Dir);
}


//==========================================================================
//
//  Sys_FileExists
//
//==========================================================================
bool Sys_FileExists (VStr filename) {
  //GLog.Logf(NAME_Debug, "Sys_FileExists(%s)", *filename);
  if (filename.isEmpty()) return false;
  #ifdef ANDROID
  if (isApkPath(filename)) {
    return tryApkFile(filename);
  }
  #endif
  return (access(*filename, R_OK) == 0 && isRegularFile(filename));
}


//==========================================================================
//
//  Sys_DirExists
//
//==========================================================================
bool Sys_DirExists (VStr path) {
  //GLog.Logf(NAME_Debug, "Sys_DirExists(%s)", *path);
  if (path.isEmpty()) return false;
  path = path.removeTrailingSlash();
  #ifdef ANDROID
  if (isApkPath(path)) {
    return tryApkDir(path);
  }
  #endif
  //return (access(*path, /*R_OK*/F_OK) == 0 && isDirectory(path));
  return isDirectory(path);
}


//==========================================================================
//
//  Sys_FileDelete
//
//==========================================================================
void Sys_FileDelete (VStr filename) {
  //GLog.Logf(NAME_Debug, "Sys_FileDelete(%s)", *filename);
  #ifdef ANDROID
  if (isApkPath(filename)) return;
  #endif
  if (filename.length()) unlink(*filename);
}


//==========================================================================
//
//  Sys_FileTime
//
//  Returns -1 if not present
//
//==========================================================================
int Sys_FileTime (VStr path) {
//  GLog.Logf("Sys_FileTime(%s)", *path);
  if (path.isEmpty()) return -1;
#ifdef ANDROID
  if (isApkPath(path)) return 0;
#endif
  struct stat buf;
  if (stat(*path, &buf) == -1) return -1;
  return (S_ISREG(buf.st_mode) ? buf.st_mtime : -1);
}


//==========================================================================
//
//  Sys_Touch
//
//==========================================================================
bool Sys_Touch (VStr path) {
  //GLog.Logf(NAME_Debug, "Sys_Touch(%s)", *path);
  if (path.isEmpty()) return -1;
  #ifdef ANDROID
  if (isApkPath(path)) return 0;
  #endif
  utimbuf tv;
  tv.actime = tv.modtime = time(NULL);
  return (utime(*path, &tv) == 0);
}


//==========================================================================
//
//  Sys_CurrFileTime
//
//==========================================================================
int Sys_CurrFileTime () {
  return (int)time(NULL);
}


//==========================================================================
//
//  Sys_CreateDirectory
//
//==========================================================================
bool Sys_CreateDirectory (VStr path) {
  //GLog.Logf(NAME_Debug, "Sys_CreateDirectory(%s)", *path);
  if (path.isEmpty()) return false;
  #ifdef ANDROID
  if (isApkPath(path)) return false;
  #endif
  return (mkdir(*path, 0777) == 0);
}


//==========================================================================
//
//  Sys_OpenDir
//
//==========================================================================
void *Sys_OpenDir (VStr path, bool wantDirs) {
  //GLog.Logf(NAME_Debug, "Sys_OpenDir(%s)", *path);
  if (path.isEmpty()) return nullptr;
  path = path.removeTrailingSlash();
  #ifdef ANDROID
  getApkAssetManager();
  if (androidAssetManager != nullptr) {
    VStr s = getApkPath(path);
    AAssetDir* d = AAssetManager_openDir(androidAssetManager, *s);
    if (d != nullptr) {
      auto res = (DirInfo *)Z_Malloc(sizeof(DirInfo));
      if (!res) {
        AAssetDir_close(d);
        return nullptr;
      }
      memset((void *)res, 0, sizeof(DirInfo));
      res->adir = d;
      res->path = "./" + s;
      res->wantDirs = wantDirs;
    }
  }
  #endif
  DIR *dh = opendir(*path);
  if (!dh) return nullptr;
  auto res = (DirInfo *)Z_Malloc(sizeof(DirInfo));
  if (!res) { closedir(dh); return nullptr; }
  memset((void *)res, 0, sizeof(DirInfo));
  res->dh = dh;
  res->path = path;
  res->wantDirs = wantDirs;
  if (res->path.length() == 0) res->path = "./";
  if (res->path[res->path.length()-1] != '/') res->path += "/";
  return (void *)res;
}


//==========================================================================
//
//  Sys_ReadDir
//
//==========================================================================
VStr Sys_ReadDir (void *adir) {
  //GLog.Logf(NAME_Debug, "Sys_ReadDir");
  if (!adir) return VStr();
  DirInfo *dh = (DirInfo *)adir;
  #ifdef ANDROID
  // android library returns only list of files
  // make your own jni wrapper if you really need directories
  if (dh->adir != nullptr) {
    const char *n;
    while ((n = AAssetDir_getNextFileName(dh->adir)) != nullptr) {
      int len = strlen(n);
      if (n[len - 1] == '/' && dh->wantDirs) continue;
      return dh->path + VStr(n);
    }
    AAssetDir_close(dh->adir);
    dh->adir = nullptr;
    dh->path.clear();
    return VStr();
  }
  #endif
  if (!dh->dh) return VStr();
  for (;;) {
    struct dirent *de = readdir(dh->dh);
    if (!de) break;
    if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
    VStr diskName = dh->path+de->d_name;
    const int ftype = GetFileType(diskName);
    if (ftype == FT_Invalid) continue; // just in case
    if (ftype == FT_Dir && !dh->wantDirs) continue; // skip directories
    if (ftype != FT_Dir && ftype != FT_Regular) continue; // something strange
    VStr res = VStr(de->d_name);
    if (ftype == FT_Dir) res += "/";
    return res;
  }
  closedir(dh->dh);
  dh->dh = nullptr;
  dh->path.clear();
  return VStr();
}


//==========================================================================
//
//  Sys_CloseDir
//
//==========================================================================
void Sys_CloseDir (void *adir) {
  //GLog.Logf(NAME_Debug, "Sys_CloseDir");
  if (adir) {
    DirInfo *dh = (DirInfo *)adir;
    #ifdef ANDROID
    if (dh->adir) AAssetDir_close(dh->adir);
    #endif
    if (dh->dh) closedir(dh->dh);
    dh->path.clear();
    Z_Free((void *)dh);
  }
}


int Sys_TimeMinPeriodMS () { return 0; }
int Sys_TimeMaxPeriodMS () { return 0; }


static bool systimeInited = false;
#ifdef __linux__
static time_t systimeSecBase = 0;

#define SYSTIME_CHECK_INIT()  do { \
  if (!systimeInited) { systimeInited = true; systimeSecBase = ts.tv_sec; } \
} while (0)

#else
static int systimeSecBase = 0;

#define SYSTIME_CHECK_INIT()  do { \
  if (!systimeInited) { systimeInited = true; systimeSecBase = tp.tv_sec; } \
} while (0)

#endif

#ifdef __linux__
# ifndef ANDROID
static cpu_set_t cpusetOther;
static int cpuMain = -1;
# endif
#endif


//==========================================================================
//
//  Sys_PinThread
//
//  pin current thread to single CPU
//
//==========================================================================
void Sys_PinThread () {
#ifdef __linux__
# ifndef ANDROID
  if (cpuMain < 0) {
    CPU_ZERO(&cpusetOther);
    sched_getaffinity(0/*current thread*/, sizeof(cpu_set_t), &cpusetOther);
    if (CPU_COUNT(&cpusetOther) < 2) return;
    #if 0
    for (int n = 0; n < CPU_SETSIZE; ++n) if (CPU_ISSET(n, &cpusetOther)) fprintf(stderr, "***MAIN THREAD CAN RUN ON CPU #%d\n", n);
    #endif
    int cid = 0;
    while (cid < CPU_SETSIZE && !CPU_ISSET(cid, &cpusetOther)) ++cid;
    if (cid < CPU_SETSIZE) {
      cpuMain = cid;
      CPU_CLR(cid, &cpusetOther);
    }
  }
  if (cpuMain >= 0) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpuMain, &cpuset);
    //pthread_setaffinity_np(this_tid, sizeof(cpu_set_t), &cpuset);
    sched_setaffinity(0/*current thread*/, sizeof(cpu_set_t), &cpuset);
    #if 0
    CPU_ZERO(&cpuset);
    sched_getaffinity(0/*current thread*/, sizeof(cpu_set_t), &cpuset);
    for (int n = 0; n < CPU_SETSIZE; ++n) if (CPU_ISSET(n, &cpuset)) fprintf(stderr, "***MAIN THREAD CAN RUN ON CPU #%d\n", n);
    #endif
  }
# endif
#endif
}


//==========================================================================
//
//  Sys_PinOtherThread
//
//  pin current thread to any CPU except the first one
//
//==========================================================================
void Sys_PinOtherThread () {
#ifdef __linux__
# ifndef ANDROID
  if (cpuMain >= 0) {
    //pthread_setaffinity_np(this_tid, sizeof(cpu_set_t), &cpuset);
    sched_setaffinity(0/*current thread*/, sizeof(cpu_set_t), &cpusetOther);
    #if 0
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    sched_getaffinity(0/*current thread*/, sizeof(cpu_set_t), &cpuset);
    for (int n = 0; n < CPU_SETSIZE; ++n) if (CPU_ISSET(n, &cpuset)) fprintf(stderr, "***(FINAL)SECONDARY THREAD CAN RUN ON CPU #%d\n", n);
    #endif
  }
# endif
#endif
}


//==========================================================================
//
//  Sys_Time_Micro
//
//  never returns 0; returns the time in microseconds(msecs*1000)
//
//==========================================================================
uint64_t Sys_Time_Micro () {
#ifdef __linux__
  struct timespec ts;
  #ifdef ANDROID // CrystaX 10.3.2 supports only this
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) Sys_Error("clock_gettime failed");
  #else
    if (clock_gettime(/*CLOCK_MONOTONIC*/CLOCK_MONOTONIC_RAW, &ts) != 0) Sys_Error("clock_gettime failed");
  #endif
  SYSTIME_CHECK_INIT();
  // we don't actually need nanosecond precision here
  // one second is 1,000,000,000 nanoseconds
  // therefore, one millisecond is 1,000,000 nanoseconds
  return
    ((uint64_t)(ts.tv_sec-systimeSecBase+1)*(uint64_t)1000000)+ // seconds->microseconds
    ((uint64_t)ts.tv_nsec/(uint64_t)1000); // nanoseconds->microseconds
#else
  struct timeval tp;
  struct timezone tzp;
  gettimeofday(&tp, &tzp);
  SYSTIME_CHECK_INIT();
  // one second is 1,000,000 microseconds
  // therefore, one millisecond is 1,000 microseconds
  return
    ((uint64_t)(tp.tv_sec-systimeSecBase+1)*(uint64_t)1000000)+ // seconds->microseconds
    ((uint64_t)tp.tv_usec);
#endif
}


//==========================================================================
//
//  Sys_Time_ExU
//
//  return valud should not be zero
//
//==========================================================================
double Sys_Time_ExU (uint64_t *usecs) {
  const uint64_t msc = Sys_Time_Micro();
  if (usecs) *usecs = msc;
  return (timeOffset+(double)msc)/1000000.0;
}


//==========================================================================
//
//  Sys_GetTimeNano
//
//  get time (start point is arbitrary) in nanoseconds
//
//==========================================================================
vuint64 Sys_GetTimeNano () {
#ifdef __linux__
  struct timespec ts;
  #ifdef ANDROID // CrystaX 10.3.2 supports only this
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) Sys_Error("clock_gettime failed");
  #else
    if (clock_gettime(/*CLOCK_MONOTONIC*/CLOCK_MONOTONIC_RAW, &ts) != 0) Sys_Error("clock_gettime failed");
  #endif
  SYSTIME_CHECK_INIT();
  return (uint64_t)(ts.tv_sec-systimeSecBase+1)*(uint64_t)1000000000+(uint64_t)ts.tv_nsec;
#else
  // in shitdoze, we have millisecond precision at best anyway
  uint64_t usecs;
  (void)Sys_Time_ExU(&usecs);
  return usecs*(uint64_t)1000;
#endif
}


#ifdef __linux__
static bool systimeCPUInited = false;
static time_t systimeCPUSecBase = 0;
#endif


//==========================================================================
//
//  Sys_Time_CPU
//
//  return value should not be zero
//
//==========================================================================
double Sys_Time_CPU () {
#ifdef __linux__
  struct timespec ts;
  if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) != 0) Sys_Error("clock_gettime failed");
  if (!systimeCPUInited) { systimeCPUInited = true; systimeCPUSecBase = ts.tv_sec; }
  return (ts.tv_sec-systimeCPUSecBase)+ts.tv_nsec/1000000000.0+1.0;
#else
  // in shitdoze, we have millisecond precision at best anyway
  return Sys_Time_ExU(nullptr);
#endif
}


#ifdef __linux__
static bool systimeCPUNanoInited = false;
static time_t systimeCPUNanoSecBase = 0;
#endif


//==========================================================================
//
//  Sys_GetTimeCPUNano
//
//==========================================================================
vuint64 Sys_GetTimeCPUNano () {
#ifdef __linux__
  struct timespec ts;
  if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) != 0) Sys_Error("clock_gettime failed");
  if (!systimeCPUNanoInited) { systimeCPUNanoInited = true; systimeCPUNanoSecBase = ts.tv_sec; }
  return (ts.tv_sec-systimeCPUNanoSecBase+1)*1000000000ULL+ts.tv_nsec;
#else
  return Sys_GetTimeNano();
#endif
}


//==========================================================================
//
//  Sys_YieldMicro
//
//==========================================================================
void Sys_YieldMicro (unsigned microsecs) {
  //usleep(1);
  /*#ifdef HAVE_YIELD*/
  #if 0
  sched_yield();
  #else
  //const struct timespec sleepTime = {0, 28500000};
  //const struct timespec sleepTime = {0, 1000000}; // one millisecond
  #if 0
  const struct timespec sleepTime = {0, 100000}; // 0.1 millisecond
  nanosleep(&sleepTime, nullptr);
  #else
  if (microsecs >= 1000000) microsecs = 1000000-1; else if (!microsecs) microsecs = 50; // 0.05 millisecs
  const struct timespec sleepTime = {0, (int32_t)microsecs*1000};
  nanosleep(&sleepTime, nullptr);
  #endif
  #endif
}


//==========================================================================
//
//  Sys_GetCPUCount
//
//==========================================================================
int Sys_GetCPUCount () {
  //nprocs_max = sysconf(_SC_NPROCESSORS_CONF);
#ifdef NO_SYSCONF
# ifdef __SWITCH__
  int res = 4;
# else
  int res = 1;
# endif
#else
  int res = sysconf(_SC_NPROCESSORS_ONLN);
#endif
  if (res < 1) return 1;
  if (res > 64) return 64; // arbitrary limit
  return res;
}


//==========================================================================
//
//  Sys_GetCurrentTID
//
//  let's hope that 32 bits are enough for thread ids on all OSes, lol
//
//==========================================================================
/*
vuint32 Sys_GetCurrentTID () {
  static __thread pid_t cachedTID = (pid_t)-1;
  if (__builtin_expect(cachedTID == (pid_t)-1, 0)) {
#ifdef __linux__
    cachedTID = syscall(__NR_gettid);
#else
// WARNING TO IMPLEMENTORS: this must be *FAST*!
// this function is used (or will be used) to protect several things
// like `VStr`, and it should not incur significant overhead, or your
// port will be dog-slow.
# error "Please, implement `Sys_GetCurrentTID()` for your OS!"
#endif
  }
  return (vuint32)cachedTID;
}
*/


//==========================================================================
//
//  Sys_GetUserName
//
//==========================================================================
VStr Sys_GetUserName () {
#if !defined(__SWITCH__) && !defined(ANDROID)
  uid_t uid = geteuid();
  struct passwd *pw = getpwuid(uid);
  if (pw) return sys_NormalizeUserName(pw->pw_name);
#endif
  return sys_NormalizeUserName(nullptr);
}


#else

//==========================================================================
//
//  shitdoze
//
//==========================================================================

#include <signal.h>
#include <fcntl.h>
#define ftime fucked_ftime
#include <io.h>
#undef ftime
#include <direct.h>
#include <conio.h>
#include <sys/timeb.h>
#include <sys/stat.h>
#include <time.h>
#include <utime.h>
#include <windows.h>

#ifndef R_OK
# define R_OK  (4)
#endif


typedef /*NTSYSAPI*/ NTSTATUS NTAPI (*RtlGetVersionFn)(/*IN OUT*/ PRTL_OSVERSIONINFOEXW lpVersionInformation);

static void __attribute__((constructor)) ctor_checkshitdoze_ctor (void) {
  RtlGetVersionFn gpi;
  HMODULE libh = GetModuleHandleW(L"ntdll.dll");
  if (!libh) return;
  gpi = (RtlGetVersionFn)(void *)GetProcAddress(libh, "RtlGetVersion");
  if (!gpi) return;
  RTL_OSVERSIONINFOEXW ver;
  memset((void *)&ver, 0, sizeof(ver));
  ver.dwOSVersionInfoSize = sizeof(/*OSVERSIONINFOEXW*/ver);
  if (gpi(&ver) != 0) return;
  #if 0
  static char buf[1024];
  snprintf(buf, sizeof(buf), "ver: major=%u; minor=%u; build=%u; platform=%u; suite=0x%04x; product=0x%02x", (unsigned)ver.dwMajorVersion, (unsigned)ver.dwMinorVersion, (unsigned)ver.dwBuildNumber,
    (unsigned)ver.dwPlatformId, (unsigned)ver.wSuiteMask, (unsigned)ver.wProductType);
  MessageBox(NULL, buf, "k8vavoom", MB_OK);
  //TerminateProcess(GetCurrentProcess(), 1);
  #endif
  const char *ee = getenv("K8VAVOOM_SKIP_WINVER_CHECK");
  if (ee && strcmp(ee, "tan") == 0) return;
  const char *msg = nullptr;
  bool fatal = false;
  //ver.dwMajorVersion = 11; ver.dwBuildNumber = 33000;
  if (ver.dwMajorVersion > 6 && ver.dwMajorVersion < 10) {
    msg = "Failed to determine your windows version.\r\nContinue on your own risk!\r\n\r\nTHIS IS NOT A K8VAVOOM BUG!";
  } else if (ver.dwMajorVersion > 10) {
    msg = "Windows 11 and higher are not supported.\r\nSorry!\r\nContinue on your own risk,\r\nbut expect bugs and crashes.\r\n\r\nTHIS IS NOT A K8VAVOOM BUG!";
  } else if (ver.dwMajorVersion == 10) {
    if (ver.dwMinorVersion > 0 || ver.dwBuildNumber >= 22000) {
      msg = "Windows 11 preview is not supported.\n\rSorry, but cannot continue.\r\n\r\nTHIS IS NOT A K8VAVOOM BUG!";
      fatal = true;
    }
  }
  if (msg) {
    srand((unsigned)GetTickCount());
    UINT flags = MB_ICONERROR|MB_SETFOREGROUND|MB_TOPMOST|(fatal ? MB_OK : MB_OKCANCEL);
    if (!fatal && (rand()&0xff) > 127) flags ^= MB_DEFBUTTON2;
    int res = MessageBoxA(NULL, msg, "k8vavoom Windows Version Check Warning", flags);
    if (res == IDCANCEL) fatal = true;
    if (!fatal && (rand()&0xff) > 127) {
      //if ((rand()&0xff) > 127) flags ^= MB_DEFBUTTON2;
      const int stt = rand()%5+((rand()&0xff) > 127 ? 3 : 2);
      Sleep(stt*1000);
      flags |= MB_DEFBUTTON2;
      static char xmsg[256];
      snprintf(xmsg, sizeof(xmsg), "LET ME REPEAT IT:\r\n\r\n%s", msg);
      res = MessageBoxA(NULL, xmsg, "k8vavoom Windows Version Check Warning!", flags);
      if (res == IDCANCEL) fatal = true;
    }
    if (fatal) {
      TerminateProcess(GetCurrentProcess(), 1);
      ExitProcess(1); // just in case
    }
  }
}


struct ShitdozeDir {
  HANDLE dir_handle;
  WIN32_FIND_DATA dir_buf;
  VStr path;
  bool gotName;
  bool wantDirs;
};


//==========================================================================
//
//  isRegularFile
//
//==========================================================================
static bool isRegularFile (VStr filename) {
  if (filename.length() == 0) return false;
  DWORD attrs = GetFileAttributes(*filename);
  if (attrs == INVALID_FILE_ATTRIBUTES) return false;
  return ((attrs&(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_DIRECTORY)) == 0);
}


//==========================================================================
//
//  isDirectory
//
//==========================================================================
static bool isDirectory (VStr filename) {
  if (filename.length() == 0) return false;
  DWORD attrs = GetFileAttributes(*filename);
  if (attrs == INVALID_FILE_ATTRIBUTES) return false;
  return ((attrs&FILE_ATTRIBUTE_DIRECTORY) != 0);
}


//==========================================================================
//
//  Sys_FileExists
//
//==========================================================================
bool Sys_FileExists (VStr filename) {
  //return (!filename.isEmpty() && access(*filename, R_OK) == 0 && isRegularFile(filename));
  return isRegularFile(filename);
}


//==========================================================================
//
//  Sys_DirExists
//
//==========================================================================
bool Sys_DirExists (VStr path) {
  return isDirectory(path);
  /*
  if (path.isEmpty()) return false;
  path = path.removeTrailingSlash();
  struct stat s;
  if (stat(*path, &s) == -1) return false;
  return !!(s.st_mode & S_IFDIR);
  */
}


//==========================================================================
//
//  Sys_FileDelete
//
//==========================================================================
void Sys_FileDelete (VStr filename) {
  if (filename.length()) DeleteFile(*filename);
}


//==========================================================================
//
//  Sys_FileTime
//
//  Returns -1 if not present
//
//==========================================================================
int Sys_FileTime (VStr path) {
  if (path.isEmpty()) return -1;
  if (!isRegularFile(path)) return -1;
  struct stat buf;
  if (stat(*path, &buf) == -1) return -1;
  return buf.st_mtime;
}


//==========================================================================
//
//  Sys_Touch
//
//==========================================================================
bool Sys_Touch (VStr path) {
  if (path.isEmpty()) return -1;
  utimbuf tv;
  tv.actime = tv.modtime = time(NULL);
  return (utime(*path, &tv) == 0);
}


//==========================================================================
//
//  Sys_CurrFileTime
//
//==========================================================================
int Sys_CurrFileTime () {
  return (int)time(NULL);
}


//==========================================================================
//
//  Sys_CreateDirectory
//
//==========================================================================
bool Sys_CreateDirectory (VStr path) {
  if (path.isEmpty()) return false;
  return (mkdir(*path) == 0);
}


//==========================================================================
//
//  Sys_OpenDir
//
//==========================================================================
void *Sys_OpenDir (VStr dirname, bool wantDirs) {
  if (dirname.isEmpty()) return nullptr;
  auto sd = (ShitdozeDir *)Z_Malloc(sizeof(ShitdozeDir));
  if (!sd) return nullptr;
  memset((void *)sd, 0, sizeof(ShitdozeDir));
  VStr path = dirname;
  if (path.length() == 0) path = "./";
  if (path[path.length()-1] != '/' && path[path.length()-1] != '\\' && path[path.length()-1] != ':') path += "/";
  sd->dir_handle = FindFirstFile(va("%s*.*", *path), &sd->dir_buf);
  if (sd->dir_handle == INVALID_HANDLE_VALUE) { Z_Free(sd); return nullptr; }
  sd->gotName = true;
  sd->path = path;
  sd->wantDirs = wantDirs;
  return (void *)sd;
}


//==========================================================================
//
//  Sys_ReadDir
//
//==========================================================================
VStr Sys_ReadDir (void *adir) {
  if (!adir) return VStr();
  auto sd = (ShitdozeDir *)adir;
  for (;;) {
    if (!sd->gotName) {
      if (FindNextFile(sd->dir_handle, &sd->dir_buf) != TRUE) return VStr();
    }
    sd->gotName = false;
    auto res = VStr(sd->dir_buf.cFileName);
    if (res == "." || res == "..") continue;
    VStr diskName = sd->path+res;
    bool isRegFile = isRegularFile(diskName);
    if (!isRegFile && !sd->wantDirs) continue; // skip directories
    if (!isRegFile && !isDirectory(diskName)) continue; // something strange
    if (!isRegFile) res += "/";
    return res;
  }
}


//==========================================================================
//
//  Sys_CloseDir
//
//==========================================================================
void Sys_CloseDir (void *adir) {
  if (adir) {
    auto sd = (ShitdozeDir *)adir;
    FindClose(sd->dir_handle);
    sd->path.clear();
    Z_Free(sd);
  }
}


//WARNING! query-perf-counter is susceptible to CPU throttling! DO NOT USE IT, EVER!
#include <mmsystem.h>
static bool shitdozeTimerInited = false;
static vuint32 shitdozeLastTime = 0;
static vuint64 shitdozeCurrTime = 1;
static vuint32 shitdozePeriodMin = 0;
static vuint32 shitdozePeriodMax = 0;
static vuint32 shitdozePeriodSet = 0;


int Sys_TimeMinPeriodMS () { return (shitdozePeriodMin > 0x7fffffffU ? 0x7fffffff : (int)shitdozePeriodMin); }
int Sys_TimeMaxPeriodMS () { return (shitdozePeriodMax > 0x7fffffffU ? 0x7fffffff : (int)shitdozePeriodMax); }


struct ShitdozeTimerInit {
  ShitdozeTimerInit () {
    shitdozePeriodMin = 0;
    shitdozePeriodMax = 0;
    TIMECAPS tc;
    if (timeGetDevCaps(&tc, sizeof(tc)) == MMSYSERR_NOERROR) {
      shitdozePeriodMin = tc.wPeriodMin;
      shitdozePeriodMax = tc.wPeriodMax;
      shitdozePeriodSet = (tc.wPeriodMin < 2 ? 1 : tc.wPeriodMin);
    } else {
      shitdozePeriodSet = 1;
    }
    timeBeginPeriod(shitdozePeriodSet);
    shitdozeTimerInited = true;
    shitdozeLastTime = (vuint32)timeGetTime();
  }

  ~ShitdozeTimerInit () {
    timeEndPeriod(shitdozePeriodSet);
    shitdozeTimerInited = false;
  }
};

ShitdozeTimerInit thisIsFuckinShitdozeTimerInitializer;

static DWORD mainAffMask = 0;
static DWORD otherAffMask = 0;


//==========================================================================
//
//  Sys_PinThread
//
//  pin current thread to single CPU
//
//==========================================================================
void Sys_PinThread () {
  if (mainAffMask == 0) {
    DWORD_PTR pmask = 0, smask = 0;
    if (!GetProcessAffinityMask(GetCurrentProcess(), &pmask, &smask)) return;
    if (!pmask) return;
    // pin to the first avaliable CPU
    DWORD xmask = 1;
    while (xmask && !(pmask&xmask)) xmask = (DWORD)(xmask<<1);
    if (!xmask) return;
    if ((DWORD)(pmask&~xmask) == 0) return;
    mainAffMask = xmask;
    otherAffMask = (DWORD)(pmask&~xmask);
  }
  if (mainAffMask) SetThreadAffinityMask(GetCurrentThread(), mainAffMask);
}


//==========================================================================
//
//  Sys_PinOtherThread
//
//  pin current thread to any CPU except the first one
//
//==========================================================================
void Sys_PinOtherThread () {
  if (otherAffMask) SetThreadAffinityMask(GetCurrentThread(), otherAffMask);
}


//==========================================================================
//
//  Sys_Time_Micro
//
//  never returns 0; returns the time in microseconds(msecs*1000)
//
//==========================================================================
uint64_t Sys_Time_Micro () {
  if (!shitdozeTimerInited) Sys_Error("shitdoze shits itself");
  const vuint32 currtime = (vuint32)timeGetTime();
  // this properly deals with wraparounds
  shitdozeCurrTime += currtime-shitdozeLastTime;
  shitdozeLastTime = currtime;
  return (uint64_t)shitdozeCurrTime*(uint64_t)1000;
}


//==========================================================================
//
//  Sys_Time_ExU
//
//==========================================================================
double Sys_Time_ExU (uint64_t *usecs) {
  if (!shitdozeTimerInited) Sys_Error("shitdoze shits itself");
  const vuint32 currtime = (vuint32)timeGetTime();
  // this properly deals with wraparounds
  shitdozeCurrTime += currtime-shitdozeLastTime;
  shitdozeLastTime = currtime;
  if (usecs) *usecs = (uint64_t)shitdozeCurrTime*(uint64_t)1000;
  return (timeOffset+(double)shitdozeCurrTime)/1000.0;
}


//==========================================================================
//
//  Sys_GetTimeNano
//
//==========================================================================
vuint64 Sys_GetTimeNano () {
  uint64_t usecs;
  (void)Sys_Time_ExU(&usecs);
  return (uint64_t)(usecs+1000)*(uint64_t)1000; // microsecs -> nsecs
}


//==========================================================================
//
//  Sys_Time_CPU
//
//==========================================================================
double Sys_Time_CPU () {
  return Sys_Time();
}


//==========================================================================
//
//  Sys_GetTimeCPUNano
//
//==========================================================================
vuint64 Sys_GetTimeCPUNano () {
  return Sys_GetTimeNano();
}


//==========================================================================
//
//  Sys_YieldMicro
//
//==========================================================================
void Sys_YieldMicro (unsigned microsecs) {
  if (microsecs < 1500) { Sleep(0); return; } // oops
  if (microsecs < ((unsigned)shitdozePeriodSet+2)*1000) {
    Sleep(0);
  } else {
    // at lest 3 msecs here
    Sleep(microsecs/1000-2);
  }
}


//==========================================================================
//
//  Sys_GetCPUCount
//
//==========================================================================
int Sys_GetCPUCount () {
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  int res = (int)info.dwNumberOfProcessors;
  if (res < 1) return 1;
  if (res > 64) return 64; // arbitrary limit
  return res;
}


//==========================================================================
//
//  Sys_GetCurrentTID
//
//  let's hope that 32 bits are enough for thread ids on all OSes, lol
//
//==========================================================================
/*
vuint32 Sys_GetCurrentTID () {
  return (vuint32)GetCurrentThreadId();
}
*/


//==========================================================================
//
//  Sys_GetUserName
//
//==========================================================================
VStr Sys_GetUserName () {
  char buf[32];
  DWORD len;
  memset(buf, 0, sizeof(buf));
  if (!GetUserNameA(buf, &len)) buf[0] = 0;
  buf[sizeof(buf)-1] = 0; // just in case
  return sys_NormalizeUserName(buf);
}
#endif
