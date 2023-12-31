//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2023 Ketmar Dark
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 3
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************

#include "fsys.h"

#ifndef WIN32
// normal OS
# include <dirent.h>
# include <fcntl.h>
# include <signal.h>
# include <time.h>
# include <unistd.h>
# include <sys/stat.h>
# include <sys/time.h>
#else
// broken os
# include <windows.h>
# include <fcntl.h>
# define ftime fucked_ftime
# include <io.h>
# undef ftime
# include <direct.h>
# include <sys/timeb.h>
# include <sys/stat.h>
#endif


// ////////////////////////////////////////////////////////////////////////// //
static mythread_mutex paklock;
//WARNING! THIS IS NOT THREAD-SAFE, BUT I DON'T CARE!
static volatile vint32 paklockInited = 0;


//==========================================================================
//
//  getBinaryPath
//
//==========================================================================
static char *getBinaryPath () {
  static char mydir[8192];
  static bool gotMyDir = false;
  if (gotMyDir) return mydir;
  gotMyDir = true;
  memset(mydir, 0, sizeof(mydir));
#ifdef __SWITCH__
  strcpy(mydir, "./");
#elif !defined(_WIN32)
  char buf[128];
  pid_t pid = getpid();
  snprintf(buf, sizeof(buf), "/proc/%u/exe", (unsigned int)pid);
  if (readlink(buf, mydir, sizeof(mydir)-1) < 0) {
    mydir[0] = '.';
    mydir[1] = '\0';
  } else {
    char *p = (char *)strrchr(mydir, '/');
    if (!p) {
      mydir[0] = '.';
      mydir[1] = '/';
      mydir[2] = '\0';
    } else {
      p[1] = '\0';
    }
  }
#else
  char *p;
  GetModuleFileName(GetModuleHandle(NULL), mydir, sizeof(mydir)-1);
  p = strrchr(mydir, '\\');
  if (!p) strcpy(mydir, "./"); else p[1] = '\0';
  for (p = mydir; *p; ++p) if (*p == '\\') *p = '/';
#endif
  return mydir;
}


//==========================================================================
//
//  fsysGetBinaryPath
//
//==========================================================================
VStr fsysGetBinaryPath () {
  return VStr(getBinaryPath());
}


// ////////////////////////////////////////////////////////////////////////// //
VStr fsysBaseDir = VStr("./"); // always ends with "/" (fill be fixed by `fsysInit()` if necessary)
bool fsysDiskFirst = true; // default is true
bool fsysKillCommonZipPrefix = false;


// ////////////////////////////////////////////////////////////////////////// //
static VStr normalizeFilePath (VStr path) {
  //fprintf(stderr, "0: <%s>\n", *path);
  int spos = 0;
  int slen = (int)path.length();
  // first, replace all slashes
  path = path.fixSlashes();
  bool hasLastSlash = path.endsWith("/");
  // first, remove things like ":/" (DF can have those)
  // also, remove root prefix
  while (spos < slen && (path[spos] == ':' || path[spos] == '/')) ++spos;
  // simply rebuild it
  VStr res;
  while (spos < slen) {
    if (path[spos] == '/') { ++spos; continue; }
    int epos = spos+1;
    while (epos < slen && path[epos] != '/') ++epos;
    // "."?
    if (epos-spos == 1 && path[spos] == '.') {
      // ignore it
      spos = epos;
      continue;
    }
    // ".."?
    if (epos-spos == 2 && path[spos] == '.' && path[spos+1] == '.') {
      // go up one dir (if there is any)
      auto dpos = res.lastIndexOf('/');
      if (dpos < 0) {
        res.clear();
      } else {
        res = res.left(dpos); // we don't need any slash
      }
      spos = epos;
      continue;
    }
    // normal name, just append it
    if (!res.isEmpty()) res += "/";
    res += path.mid(spos, epos-spos);
    spos = epos;
  }
  // append last slash, if there is any
  if (hasLastSlash && !res.isEmpty()) res += "/";
  //fprintf(stderr, "1: <%s>\n", *res);
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
enum { MaxOpenPaks = 65536 };

static FSysDriverBase *openPaks[MaxOpenPaks]; // 0 is always basedir
static int openPakCount = 0;


// ////////////////////////////////////////////////////////////////////////// //
//typedef FSysDriver* (*FSysOpenPakFn) (VStream *);

struct FSysDriverCreator {
  FSysOpenPakFn ldr;
  int prio;
  char *drvname;
  FSysDriverCreator *next;
};

static FSysDriverCreator *creators = nullptr;


void FSysRegisterDriver (FSysOpenPakFn ldr, const char *drvname, int prio) {
  if (!ldr) return;

  FSysDriverCreator *prev = nullptr, *cur = creators;
  while (cur && cur->prio >= prio) {
    prev = cur;
    cur = cur->next;
  }

  if (!drvname || !drvname[0]) drvname = "unnamed";

  //GLog.Logf(NAME_Debug, "registering loader '%s' (prio=%d)", drvname, prio);

  auto it = new FSysDriverCreator;
  it->ldr = ldr;
  it->prio = prio;
  it->next = cur;
  it->drvname = (char *)Z_Calloc((int)strlen(drvname)+1);
  strcpy(it->drvname, drvname);

  if (prev) prev->next = it; else creators = it;
}


// ////////////////////////////////////////////////////////////////////////// //
static __attribute((unused)) inline vuint32 fnameHashBufCI (VStr str) {
  size_t len = str.length();
  if (len == 0) return 1;
  // fnv-1a: http://www.isthe.com/chongo/tech/comp/fnv/
  vuint32 hash = 2166136261U; // fnv offset basis
  const vuint8 *s = (const vuint8 *)*str;
  while (len-- > 0) {
    vuint32 ch = VStr::locase1251(*s++);
    hash ^= ch;
    hash *= 16777619U; // 32-bit fnv prime
  }
  return (hash ? hash : 1); // this is unlikely, but...
}


FSysDriverBase::FSysDriverBase ()
  : mOpenedFiles(0)
  , mPrefix(VStr())
  , htableSize(0)
  , htable(nullptr)
  , mActive(true)
{
}


FSysDriverBase::~FSysDriverBase () {
  delete htable;
  htable = nullptr;
  htableSize = 0;
}


void FSysDriverBase::fileOpened (VStream *s) {
  if (s) ++mOpenedFiles;
}


void FSysDriverBase::fileClosed (VStream *s) {
  if (s) {
    if (--mOpenedFiles == 0 && !active() && canBeDestroyed()) {
      // kill it
      fsysInit();
      MyThreadLocker paklocker(&paklock);
      int pakid = 1;
      while (pakid < openPakCount && openPaks[pakid] != this) ++pakid;
      if (pakid < openPakCount) {
        openPaks[pakid] = nullptr;
        while (openPakCount > 1 && !openPaks[openPakCount-1]) --openPakCount;
      }
      delete this;
    }
  }
}


bool FSysDriverBase::canBeDestroyed () {
  return (mOpenedFiles == 0);
}


bool FSysDriverBase::active () {
  return mActive;
}


void FSysDriverBase::deactivate () {
  mActive = false;
}


void FSysDriverBase::buildNameHashTable () {
  delete htable;
  htable = nullptr;
  int dlen = getNameCount();
  htableSize = (vuint32)dlen;
  if (dlen == 0) return;
  htable = new HashTableEntry[dlen];
  for (int f = 0; f < dlen; ++f) htable[f] = HashTableEntry(); // just in case
  for (int idx = dlen-1; idx >= 0; --idx) {
    vuint32 nhash = fnameHashBufCI(getNameByIndex(idx)); // never zero
    vuint32 hidx = nhash%(vuint32)dlen;
    if (htable[hidx].didx == 0xffffffffU) {
      // first item
      htable[hidx].hash = nhash;
      htable[hidx].didx = (vuint32)idx;
      //assert(htable.ptr[hidx].prev == 0xffffffffU);
    } else {
      // chain
      while (htable[hidx].prev != 0xffffffffU) hidx = htable[hidx].prev;
      // find free slot
      vuint32 freeslot = hidx;
      for (vuint32 count = 0; count < (vuint32)dlen; ++count) {
        freeslot = (freeslot+1)%(vuint32)dlen;
        if (htable[freeslot].hash == 0) break; // i found her!
      }
      //if (htable.ptr[freeslot].hash != 0) assert(0, "wtf?!");
      htable[hidx].prev = freeslot;
      htable[freeslot].hash = nhash;
      htable[freeslot].didx = (vuint32)idx;
      //assert(htable.ptr[freeslot].prev == uint.max);
    }
  }
}


// index or -1
int FSysDriverBase::findName (VStr fname) const {
  vuint32 nhash = fnameHashBufCI(fname);
  vuint32 hidx = nhash%htableSize;
  while (hidx != 0xffffffffU && htable[hidx].hash != 0) {
    if (htable[hidx].hash == nhash) {
      vuint32 didx = htable[hidx].didx;
      if (getNameByIndex(didx).equ1251CI(fname)) return (int)didx;
    }
    hidx = htable[hidx].prev;
  }
  // alas, and it is guaranteed that we have no such file here
  return -1;
}


bool FSysDriverBase::hasFile (VStr fname) {
  return (findName(fname) >= 0);
}


VStr FSysDriverBase::findFileWithAnyExt (VStr fname) {
  if (fname.length() == 0) return VStr();
  if (hasFile(fname)) return fname;
  for (int f = getNameCount()-1; f >= 0; --f) {
    VStr name = getNameByIndex(f);
    if (name.length() < fname.length()) continue;
    name = name.stripExtension();
    if (name.length() != fname.length()) continue;
    if (name.equ1251CI(fname)) return getNameByIndex(f);
  }
  return VStr();
}


VStream *FSysDriverBase::open (VStr fname) {
  int idx = findName(fname);
  if (idx < 0) return nullptr;
  return openWithIndex(idx);
}


// ////////////////////////////////////////////////////////////////////////// //
VStreamDiskFile::VStreamDiskFile (FILE* afl, VStr aname, bool asWriter, FSysDriverBase *aDriver)
  : VStreamPakFile(aDriver)
  , mFl(afl)
  , mName(aname)
  , size(-1)
{
  if (afl) fseek(afl, 0, SEEK_SET);
  bLoading = !asWriter;
}

VStreamDiskFile::~VStreamDiskFile () {
  Close();
}

void VStreamDiskFile::setError () {
  if (mFl) { fclose(mFl); mFl = nullptr; }
  mName.clear();
  bError = true;
}

VStr VStreamDiskFile::GetName () const { return mName.cloneUnique(); }

void VStreamDiskFile::Seek (int pos) {
  if (!mFl) { setError(); return; }
  if (fseek(mFl, pos, SEEK_SET)) setError();
}

int VStreamDiskFile::Tell () { return (bError || !mFl ? 0 : ftell(mFl)); }

int VStreamDiskFile::TotalSize () {
  if (size < 0 && mFl && !bError) {
    auto opos = ftell(mFl);
    fseek(mFl, 0, SEEK_END);
    size = (int)ftell(mFl);
    fseek(mFl, opos, SEEK_SET);
  }
  return size;
}

bool VStreamDiskFile::AtEnd () { return (bError || !mFl || Tell() >= TotalSize()); }

bool VStreamDiskFile::Close () {
  if (mFl) { fclose(mFl); mFl = nullptr; }
  mName.clear();
  return !bError;
}

void VStreamDiskFile::Serialise (void *buf, int len) {
  if (bError || !mFl || len < 0) { setError(); return; }
  if (len == 0) return;
  if (bLoading) {
    if (fread(buf, len, 1, mFl) != 1) setError();
  } else {
    if (fwrite(buf, len, 1, mFl) != 1) setError();
  }
}


// ////////////////////////////////////////////////////////////////////////// //
class FSysDriverDisk : public FSysDriverBase {
private:
  VStr path;

private:
  static bool isGoodPath (VStr path);

protected:
  virtual VStr getNameByIndex (int idx) const override;
  virtual int getNameCount () const override;

protected:
  virtual VStream *openWithIndex (int idx) override;

public:
  FSysDriverDisk (VStr apath);
  virtual ~FSysDriverDisk () override;

  virtual bool canBeDestroyed () override;

  virtual bool hasFile (VStr fname) override;
  virtual VStream *open (VStr fname) override;
  virtual VStr findFileWithAnyExt (VStr fname) override;
};


bool FSysDriverDisk::isGoodPath (VStr path) {
  return !path.isEmpty();
/*
  if (path.length() == 0) return false;
  if (path == "/") return false;
  if (path == "." || path == "..") return false;
  if (path.endsWith("/.")) return false;
  if (path.endsWith("/..")) return false;
  if (path.indexOf("/./") >= 0) return false;
  if (path.indexOf("/../") >= 0) return false;
#ifdef WIN32
  if (path.endsWith("\\.")) return false;
  if (path.endsWith("\\..")) return false;
  if (path.indexOf("\\.\\") >= 0) return false;
  if (path.indexOf("\\..\\") >= 0) return false;
  if (path.indexOf("\\./") >= 0) return false;
  if (path.indexOf("\\../") >= 0) return false;
  if (path.indexOf("/.\\") >= 0) return false;
  if (path.indexOf("/..\\") >= 0) return false;
#endif
  return true;
*/
}


#ifndef WIN32
static VStr findFileNC (VStr fname, bool ignoreExt) {
  if (fname.length() == 0) return VStr();
  VStr res;
  if (fname[0] == '/') { res = "/"; fname.chopLeft(1); }
  while (fname.length() != 0) {
    if (fname[0] == '/') { fname.chopLeft(1); continue; }
    auto sle = fname.indexOf('/');
    VStr curname;
    if (sle < 0) {
      curname = fname;
      fname.clear();
      if (ignoreExt) curname = curname.stripExtension();
    } else {
      curname = fname.mid(0, sle);
      fname.chopLeft(sle+1);
    }
    if (sle > 0 || !ignoreExt) {
      VStr fullpath = res;
      if (fullpath.length() && fullpath != "/") fullpath += "/";
      fullpath += curname;
      if (access(*fullpath, F_OK) == 0) {
        res = fullpath;
        continue;
      }
    }
    // scan directory
    DIR *dir = opendir(res.length() ? *res : ".");
    if (!dir) return VStr();
    bool found = false;
    for (;;) {
      struct dirent *de = readdir(dir);
      if (!de) break;
      if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
      auto xname = VStr(de->d_name);
      if (sle < 0 && ignoreExt) xname = xname.stripExtension();
      if (xname.equ1251CI(curname)) {
        if (res.length() && res != "/") res += "/";
        res += de->d_name;
        found = true;
        break;
      }
    }
    closedir(dir);
    if (!found) return VStr();
  }
  if (res.endsWith("/")) return VStr();
  return res;
}
#else
static VStr findFileNC (VStr fname, bool ignoreExt) {
  if (fname.length() == 0) return VStr();
  //if (!ignoreExt) return fname;
  VStr path = fname.extractFilePath();
  VStr name = fname.extractFileName();
  if (ignoreExt) name = name.stripExtension();
  //fprintf(stderr, "SCAN: <%s> <%s>\n", *path, *name);
  void *dir = fsysOpenDir(path);
  if (!dir) return VStr();
  for (;;) {
    VStr dn = fsysReadDir(dir);
    if (dn.length() == 0) break;
    //fprintf(stderr, "  GOT: <%s> <%s>\n", *dn, *dn.stripExtension());
    if ((ignoreExt && dn.stripExtension().equ1251CI(name)) || (!ignoreExt && dn.equ1251CI(name))) {
      fsysCloseDir(dir);
      if (path.length() && !path.endsWith("/") && !path.endsWith("\\")) path += "/";
      path += dn;
      return path;
    }
  }
  fsysCloseDir(dir);
  return VStr();
}
#endif


FSysDriverDisk::FSysDriverDisk (VStr apath) : FSysDriverBase() {
  path = apath;
  if (path.length() == 0) path = "./";
  if (!path.endsWith("/")) path += "/";
  setFilePath(path);
}

FSysDriverDisk::~FSysDriverDisk () {}

bool FSysDriverDisk::canBeDestroyed () { return true; }

VStr FSysDriverDisk::getNameByIndex (int idx) const { *(int *)0 = 0; return VStr::EmptyString; } // the thing that should not be
int FSysDriverDisk::getNameCount () const { return 0; } // the thing that should not be
VStream *FSysDriverDisk::openWithIndex (int idx) { return nullptr; } // the thing that should not be

bool FSysDriverDisk::hasFile (VStr fname) {
  if (!isGoodPath(fname)) return false;
  VStr newname = findFileNC(path+fname, false);
  if (newname.length() == 0) return false;
  FILE *fl = fopen(*newname, "rb");
  if (!fl) return false;
  fclose(fl);
  return true;
}

VStr FSysDriverDisk::findFileWithAnyExt (VStr fname) {
  if (!isGoodPath(fname)) return VStr();
  VStr newname = findFileNC(path+fname, true); // ignore ext
  if (newname.length() == 0) return VStr();
  FILE *fl = fopen(*newname, "rb");
  if (!fl) return VStr();
  fclose(fl);
  return newname;
}

VStream *FSysDriverDisk::open (VStr fname) {
  //fprintf(stderr, "000: <%s>\n", *fname);
  if (!isGoodPath(fname)) return nullptr;
  //fprintf(stderr, "001: <%s><%s>\n", *path, *fname);
  VStr newname = findFileNC(path+fname, false);
  //fprintf(stderr, "002: <%s> <%s>\n", *fname, *newname);
  if (newname.length() == 0) {
    // hack for vccrun
    static const char *dirs[] = {
      "./",
      "./packages/",
      "!",
      "!!",
#ifdef VCCRUN_K8BUILD
      "/home/ketmar/back/vavoom_dev/src/vccrun/",
      "/home/ketmar/back/vavoom_dev/src/vccrun/packages/",
#endif
      nullptr
    };
    // look for file
    VStr mydir;
    for (const char **sdra = dirs; *sdra; ++sdra) {
      const char *sdr = *sdra;
      if (sdr[0] == '!') {
        // binary dir
        if (mydir.length() == 0) mydir = VStr(getBinaryPath());
        if (sdr[1] == '!') {
          // mydir+"packages/"
          newname = findFileNC(mydir+"packages/"+fname, false);
        } else {
          // mydir
          newname = findFileNC(mydir+fname, false);
        }
      } else {
        // normal dir
        newname = findFileNC(VStr(sdr)+fname, false);
      }
      if (newname.length() != 0) break;
    }
    if (newname.length() == 0) return nullptr;
  }
  FILE *fl = fopen(*newname, "rb");
  if (!fl) return nullptr;
  return new VStreamDiskFile(fl, fname);
}


// ////////////////////////////////////////////////////////////////////////// //
VStreamPakFile::VStreamPakFile (FSysDriverBase *aDriver)
  : VStream()
  , mDriver(aDriver)
{
  if (aDriver) aDriver->fileOpened(this);
}


VStreamPakFile::~VStreamPakFile () {
  if (mDriver) mDriver->fileClosed(this);
}


// ////////////////////////////////////////////////////////////////////////// //
// `fsysBaseDir` should be set before calling this
static void fsysInitInternal (bool addBaseDir) {
  //WARNING! THIS IS NOT THREAD-SAFE, BUT I DON'T CARE!
  // paklockInited: <0: initializing now; 0x0fffffff: initialized
  // 0 becomes -1, 2 becomes 1, and so on
  // check if we are initializing this crap in another thread
  if (paklockInited--) {
    // still positive? already initialized
    if (paklockInited > 0) {
      // restore value and exit
      ++paklockInited;
      return;
    }
    // it is negative, do spinlock wait, and then exit
    while (paklockInited < 0) {}
    return;
  }
  // nope, it is not initialized
  mythread_mutex_init(&paklock);
  if (addBaseDir && openPakCount == 0) {
    fsys_Register_ZIP();
    fsys_Register_DFWAD();
    fsys_Register_DOOMWAD();
         if (fsysBaseDir.length() == 0) fsysBaseDir = VStr("./");
    else if (!fsysBaseDir.endsWith("/")) fsysBaseDir += "/"; // fuck you, shitdoze
    openPaks[0] = new FSysDriverDisk(fsysBaseDir);
    openPakCount = 1;
  }
  // set "initialized" flag
  paklockInited = 0xffff;
}


void fsysInit () {
  fsysInitInternal(true);
}


void fsysShutdown () {
}


// ////////////////////////////////////////////////////////////////////////// //
// append disk directory to the list of archives
int fsysAppendDir (VStr path, VStr apfx) {
  if (path.length() == 0) return 0;
  fsysInitInternal(false);
  MyThreadLocker paklocker(&paklock);
  if (openPakCount >= MaxOpenPaks) Sys_Error("too many pak files");
  openPaks[openPakCount] = new FSysDriverDisk(path);
  openPaks[openPakCount]->setPrefix(apfx);
  return ++openPakCount;
}


// append archive to the list of archives
// it will be searched in the current dir, and then in `fsysBaseDir`
// returns pack id or 0
int fsysAppendPak (VStr fname, int pakid) {
  if (fname.length() == 0) return false;
  VStr fn = fname;
  VStream *fl = fsysOpenFile(fname, pakid);
  if (!fl) {
    VStr ext = fname.extractFileExtension();
    if (ext.equ1251CI(".pk3")) {
      fn = fname.stripExtension()+".wad";
      fl = fsysOpenFile(fn, pakid);
    } else if (ext.equ1251CI(".wad")) {
      fn = fname.stripExtension()+".pk3";
      fl = fsysOpenFile(fn, pakid);
    }
    if (!fl) return false;
  }
  int pos = (int)fn.length();
  while (pos > 0 && fn[pos-1] != '/' && fn[pos-1] != '\\') --pos;
#if 0
  int epos = (int)fn.length()-1;
  while (epos >= pos && fn[epos] != '.') --epos;
  if (epos < pos) epos = (int)fn.length();
  VStr pfx = fn.mid(pos, epos-pos);
  pfx += ".wad";
#else
  VStr pfx = fn.mid(pos, (int)fn.length()-pos);
#endif
  //return fsysAppendPak(new VStreamDiskFile(fl, fn), pfx);
  return fsysAppendPak(fl, pfx);
}


// this will take ownership of `strm` (or kill it on error)
// returns pack id or 0
int fsysAppendPak (VStream *strm, VStr apfx) {
  if (!strm) return 0;
  fsysInit();
  MyThreadLocker paklocker(&paklock);

  // it MUST append packs to the end of the list, so `fsysRemovePaksFrom()` will work properly
  if (openPakCount >= MaxOpenPaks) { VStream::Destroy(strm); Sys_Error("too many pak files"); }

  //GLog.Logf(NAME_Debug, "trying <%s> : pfx=<%s>", *strm->GetName(), *apfx);
  for (FSysDriverCreator *cur = creators; cur; cur = cur->next) {
    strm->Seek(0);
    if (strm->IsError()) break;
    //GLog.Logf(NAME_Debug, " !!! %d (%s)", strm->TotalSize(), cur->drvname);
    auto drv = cur->ldr(strm);
    //if (strm->IsError()) { delete drv; break; }
    if (drv) {
      //GLog.Logf(NAME_Debug, "  :: <%s> is '%s'", *strm->GetName(), cur->drvname);
      drv->setPrefix(apfx);
      drv->setFilePath(strm->GetName());
      openPaks[openPakCount++] = drv;
      return openPakCount;
    }
    //GLog.Logf(NAME_Debug, " +++ %d", (int)(strm->IsError()));
  }

  VStream::Destroy(strm);
  return 0;
}


// remove given pack from pack list
void fsysRemovePak (int pakid) {
  fsysInit();
  FSysDriverBase *tokill = nullptr;
  {
    MyThreadLocker paklocker(&paklock);
    if (pakid < 2 || pakid > openPakCount || !openPaks[pakid-1]) return;
    --pakid;
    if (openPaks[pakid]->canBeDestroyed()) {
      tokill = openPaks[pakid];
      openPaks[pakid] = nullptr;
      while (openPakCount > 1 && !openPaks[openPakCount-1]) --openPakCount;
    } else {
      openPaks[pakid]->deactivate();
    }
  }
  delete tokill;
}


// remove all packs from pakid and later
void fsysRemovePaksFrom (int pakid) {
  fsysInit();
  MyThreadLocker paklocker(&paklock);
  if (pakid < 2 || pakid > openPakCount) return;
  --pakid;
  for (int f = openPakCount-1; f >= pakid; --f) {
    if (!openPaks[f]) continue;
    if (openPaks[f]->canBeDestroyed()) {
      delete openPaks[f];
      openPaks[f] = nullptr;
    } else {
      openPaks[f]->deactivate();
    }
  }
  while (openPakCount > 1 && !openPaks[openPakCount-1]) --openPakCount;
}


// return pack file path for the given pack id (or empty string)
VStr fsysGetPakPath (int pakid) {
  fsysInit();
  MyThreadLocker paklocker(&paklock);
  if (pakid < 1 || pakid > openPakCount) return VStr();
  --pakid;
  if (!openPaks[pakid] || !openPaks[pakid]->active()) return VStr();
  return openPaks[pakid]->getFilePath();
}


// return pack prefix for the given pack id (or empty string)
VStr fsysGetPakPrefix (int pakid) {
  fsysInit();
  MyThreadLocker paklocker(&paklock);
  if (pakid < 1 || pakid > openPakCount) return VStr();
  --pakid;
  if (!openPaks[pakid] || !openPaks[pakid]->active()) return VStr();
  return openPaks[pakid]->getPrefix();
}


int fsysGetLastPakId () {
  fsysInit();
  MyThreadLocker paklocker(&paklock);
  return openPakCount;
}


// ////////////////////////////////////////////////////////////////////////// //
static void splitFileName (VStr infname, VStr &pfx, VStr &fname) {
  int pos = 0;
  while (pos < (int)infname.length()) {
    if (infname[pos] == '/' || infname[pos] == '\\') break;
    if (infname[pos] == ':') {
      pfx = infname.left(pos);
      fname = infname;
      fname.chopLeft(pos+1);
      return;
    }
    ++pos;
  }
  pfx = VStr();
  fname = infname;
}


static bool isPrefixEqu (VStr p0, VStr p1) {
  if (p0.length() == p1.length() && p0.equ1251CI(p1)) return true;
  VStr ext0 = p0.extractFileExtension();
  VStr ext1 = p0.extractFileExtension();
  if (ext0.equ1251CI(".pk3")) ext0 = ".wad";
  if (ext1.equ1251CI(".pk3")) ext1 = ".wad";
  VStr s0 = p0.stripExtension()+ext0;
  VStr s1 = p1.stripExtension()+ext1;
  return (s0.length() == s1.length() && s0.equ1251CI(s1));
}


// ////////////////////////////////////////////////////////////////////////// //
// 0: no such pack
int fsysFindPakByPrefix (VStr pfx) {
  if (pfx.length() == 0) return 0;
  fsysInit();
  MyThreadLocker paklocker(&paklock);
  // check non-basedir packs
  for (int f = openPakCount-1; f > 0; --f) {
    if (isPrefixEqu(openPaks[f]->getPrefix(), pfx)) return f+1;
  }
  return 0;
}


// ////////////////////////////////////////////////////////////////////////// //
bool fsysFileExists (VStr fname, int pakid) {
  fsysInit();
  VStr goodname = normalizeFilePath(fname);
  VStr pfx, fn;
  splitFileName(goodname, pfx, fn);
  MyThreadLocker paklocker(&paklock);
  // try basedir first, if the corresponding flag is set
#ifdef WIN32
  if ((pakid == fsysAnyPak || pakid == 1) && fsysDiskFirst && pfx.length() < 2)
#else
  if ((pakid == fsysAnyPak || pakid == 1) && fsysDiskFirst && pfx.length() == 0)
#endif
  {
    if (openPaks[0]->active() && openPaks[0]->hasFile(goodname)) return true;
  }
  // do other paks
  for (int f = openPakCount-1; f > 0; --f) {
    if (pakid != fsysAnyPak) {
      if (f != pakid-1) continue;
    }
    if (!openPaks[f]->active()) continue;
    if (pfx.length()) {
      if (isPrefixEqu(openPaks[f]->getPrefix(), pfx)) {
        if (openPaks[f]->hasFile(fn)) return true;
      }
    } else {
      if (openPaks[f]->hasFile(goodname)) return true;
    }
  }
  // try basedir last, if the corresponding flag is set
#ifdef WIN32
  if ((pakid == fsysAnyPak || pakid == 1) && !fsysDiskFirst && pfx.length() < 2)
#else
  if ((pakid == fsysAnyPak || pakid == 1) && !fsysDiskFirst && pfx.length() == 0)
#endif
  {
    if (openPaks[0]->active() && openPaks[0]->hasFile(goodname)) return true;
  }
  return false;
}


// open file for reading, relative to basedir, and look into archives too
VStream *fsysOpenFileSimple (VStr fname) {
  return fsysOpenFile(fname);
}


// open file for reading, relative to basedir, and look into archives too
VStream *fsysOpenFile (VStr fname, int pakid) {
  fsysInit();
  VStr goodname = normalizeFilePath(fname);
  VStr pfx, fn;
  splitFileName(goodname, pfx, fn);
  MyThreadLocker paklocker(&paklock);
  //fprintf(stderr, "fsysOpenFile: <%s>\n", *fname);
  // try basedir first, if the corresponding flag is set
#ifdef WIN32
  if ((pakid == fsysAnyPak || pakid == 1) && fsysDiskFirst && pfx.length() < 2 && openPaks[0]->active())
#else
  if ((pakid == fsysAnyPak || pakid == 1) && fsysDiskFirst && pfx.length() == 0 && openPaks[0]->active())
#endif
  {
    auto res = openPaks[0]->open(goodname);
    if (res) return res;
  }
  // do other paks
  for (int f = openPakCount-1; f > 0; --f) {
    if (pakid != fsysAnyPak) {
      if (f != pakid-1) continue;
    }
    if (!openPaks[f]->active()) continue;
    if (pfx.length()) {
      if (isPrefixEqu(openPaks[f]->getPrefix(), pfx)) {
        //fprintf(stderr, "checking PAK #%d for <%s>\n", f, *fn);
        auto res = openPaks[f]->open(fn);
        if (res) {
          //fprintf(stderr, "  FOUND!\n");
          return res;
        }
      }
    } else {
      auto res = openPaks[f]->open(goodname);
      if (res) return res;
    }
  }
  // try basedir last, if the corresponding flag is set
#ifdef WIN32
  if ((pakid == fsysAnyPak || pakid == 1) && !fsysDiskFirst && pfx.length() < 2 && openPaks[0]->active())
#else
  if ((pakid == fsysAnyPak || pakid == 1) && !fsysDiskFirst && pfx.length() == 0 && openPaks[0]->active())
#endif
  {
    auto res = openPaks[0]->open(goodname);
    if (res) return res;
  }
  return nullptr;
}


// open file for reading, relative to basedir, and look into archives too
VStream *fsysOpenFileAnyExt (VStr fname, int pakid) {
  VStr rname = fsysFileFindAnyExt(fname, pakid);
  if (rname.length() == 0) return nullptr;
  return fsysOpenFile(rname, pakid);
}


VStr fsysForEachPakFile (bool (*dg) (VStr fname)) {
  fsysInit();
  MyThreadLocker paklocker(&paklock);
  if (openPakCount < 1) return VStr::EmptyString;
  // try basedir first, if the corresponding flag is set
  if (fsysDiskFirst) {
    if (openPaks[0]->active()) {
      int len = openPaks[0]->getNameCount();
      for (int pidx = 0; pidx < len; ++pidx) {
        VStr fn = openPaks[0]->getNameByIndex(pidx);
        if (!fn.isEmpty() && dg(fn)) return fn;
      }
    }
  }
  // do other paks
  for (int f = openPakCount-1; f > 0; --f) {
    if (openPaks[f]->active()) {
      int len = openPaks[f]->getNameCount();
      for (int pidx = 0; pidx < len; ++pidx) {
        VStr fn = openPaks[f]->getNameByIndex(pidx);
        if (!fn.isEmpty() && dg(fn)) return fn;
      }
    }
  }
  // try basedir last, if the corresponding flag is set
  if (!fsysDiskFirst) {
    if (openPaks[0]->active()) {
      int len = openPaks[0]->getNameCount();
      for (int pidx = 0; pidx < len; ++pidx) {
        VStr fn = openPaks[0]->getNameByIndex(pidx);
        if (!fn.isEmpty() && dg(fn)) return fn;
      }
    }
  }
  return VStr::EmptyString;
}


// open file for writing, NOT relative to basedir
VStream *fsysOpenDiskFileWrite (VStr fname) {
  if (fname.length() == 0) return nullptr;
  FILE *fl = fopen(*fname, "wb");
  if (!fl) return nullptr;
  return new VStreamDiskFile(fl, fname, true);
}


// open file for reading, NOT relative to basedir
VStream *fsysOpenDiskFile (VStr fname) {
  if (fname.length() == 0) return nullptr;
  FILE *fl = fopen(*fname, "rb");
  if (!fl) return nullptr;
  return new VStreamDiskFile(fl, fname);
}


// find file with any extension
static VStr fsysFileFindAnyExtInternal (VStr fname, int pakid) {
  fsysInit();
  if (fsysFileExists(fname, pakid)) return fname;
  VStr goodname = normalizeFilePath(fname);
  VStr pfx, fn;
  splitFileName(goodname, pfx, fn);
  //fprintf(stderr, "fsysFileFindAnyExtInternal: <%s>; fn=<%s>; pfx=<%s>\n", *fname, *fn, *pfx);
  MyThreadLocker paklocker(&paklock);
  // try basedir first, if the corresponding flag is set
  if ((pakid == fsysAnyPak || pakid == 1) && fsysDiskFirst && openPaks[0]->active() &&
#ifdef WIN32
    pfx.length() < 2
#else
    pfx.length() == 0
#endif
  ) {
    auto res = openPaks[0]->findFileWithAnyExt(goodname);
    if (res.length()) return res;
  }
  // do other paks
  for (int f = openPakCount-1; f > 0; --f) {
    if (pakid != fsysAnyPak) {
      if (f != pakid-1) continue;
    }
    if (!openPaks[f]->active()) continue;
    if (pfx.length()) {
      if (isPrefixEqu(openPaks[f]->getPrefix(), pfx)) {
        auto res = openPaks[f]->findFileWithAnyExt(fn);
        if (res.length()) return (pfx.length() ? pfx+":"+res : res);
      }
    } else {
      auto res = openPaks[f]->findFileWithAnyExt(goodname);
      if (res.length()) return res;
    }
  }
  // try basedir last, if the corresponding flag is set
  if ((pakid == fsysAnyPak || pakid == 1) && !fsysDiskFirst && openPaks[0]->active() &&
#ifdef WIN32
    pfx.length() < 2
#else
    pfx.length() == 0
#endif
  ) {
    auto res = openPaks[0]->findFileWithAnyExt(goodname);
    if (res.length()) return res;
  }
  return VStr();
}


VStr fsysFileFindAnyExt (VStr fname, int pakid) {
  if (fname.length() == 0) return VStr();
  VStr res = fsysFileFindAnyExtInternal(fname, pakid);
  if (res.length()) return res;
  VStr f2 = fname.stripExtension();
  if (f2.length() == fname.length()) return VStr();
  return fsysFileFindAnyExtInternal(f2, pakid);
}


// ////////////////////////////////////////////////////////////////////////// //
VPartialStreamReader::VPartialStreamReader (VStream *ASrcStream, int astpos, int apartlen, FSysDriverBase *aDriver)
  : VStreamPakFile(aDriver)
  , srcStream(ASrcStream)
  , stpos(astpos)
  , srccurpos(astpos)
  , partlen(apartlen)
{
  mythread_mutex_init(&lock);
  bLoading = true;
  if (!srcStream) { bError = true; return; }
  if (partlen < 0) {
    MyThreadLocker locker(&lock);
    partlen = srcStream->TotalSize()-stpos;
    if (partlen < 0) partlen = 0;
  }
}

VPartialStreamReader::~VPartialStreamReader () {
  Close();
  mythread_mutex_destroy(&lock);
}

VStr VPartialStreamReader::GetName () const {
  return (srcStream ? srcStream->GetName() : VStr());
}

bool VPartialStreamReader::Close () {
  //if (srcStream) { delete srcStream; srcStream = nullptr; }
  srcStream = nullptr;
  return !bError;
}

void VPartialStreamReader::setError () {
  //if (srcStream) { delete srcStream; srcStream = nullptr; }
  srcStream = nullptr;
  bError = true;
}

void VPartialStreamReader::Serialise (void *buf, int len) {
  if (bError) return;
  if (len < 0) { setError(); return; }
  if (len == 0) return;
  if (srccurpos >= stpos+partlen) { setError(); return; }
  int left = stpos+partlen-srccurpos;
  if (left < len) { setError(); return; }
  MyThreadLocker locker(&lock);
  srcStream->Seek(srccurpos);
  srcStream->Serialise(buf, len);
  if (srcStream->IsError()) { setError(); return; }
  srccurpos += len;
}

void VPartialStreamReader::Seek (int pos) {
  if (pos < 0) pos = 0;
  if (pos > partlen) pos = partlen;
  srccurpos = stpos+pos;
}

int VPartialStreamReader::Tell () { return (bError ? 0 : srccurpos-stpos); }

int VPartialStreamReader::TotalSize () { return (bError ? 0 : partlen); }

bool VPartialStreamReader::AtEnd () { return (bError || srccurpos >= stpos+partlen); }


// ////////////////////////////////////////////////////////////////////////// //
/*
// VZLibStreamReader
VZLibStreamReader::VZLibStreamReader (VStream *ASrcStream, vuint32 ACompressedSize, vuint32 AUncompressedSize, bool asZipArchive, FSysDriverBase *aDriver)
  : VStreamPakFile(aDriver)
  , srcStream(ASrcStream)
  , initialised(false)
  , compressedSize(ACompressedSize)
  , uncompressedSize(AUncompressedSize)
  , nextpos(0)
  , currpos(0)
  , zipArchive(asZipArchive)
  , origCrc32(0)
  , currCrc32(0)
  , doCrcCheck(false)
  , forceRewind(false)
  , mFileName(VStr())
{
  mythread_mutex_init(&lock);
  initialize();
}


VZLibStreamReader::VZLibStreamReader (VStr fname, VStream *ASrcStream, vuint32 ACompressedSize, vuint32 AUncompressedSize, bool asZipArchive, FSysDriverBase *aDriver)
  : VStreamPakFile(aDriver)
  , srcStream(ASrcStream)
  , initialised(false)
  , compressedSize(ACompressedSize)
  , uncompressedSize(AUncompressedSize)
  , nextpos(0)
  , currpos(0)
  , zipArchive(asZipArchive)
  , origCrc32(0)
  , currCrc32(0)
  , doCrcCheck(false)
  , forceRewind(false)
  , mFileName(fname)
{
  mythread_mutex_init(&lock);
  initialize();
}


VZLibStreamReader::~VZLibStreamReader () {
  Close();
  mythread_mutex_destroy(&lock);
}


void VZLibStreamReader::initialize () {
  bLoading = true;

  // initialise zip stream structure
  memset(&zStream, 0, sizeof(zStream));
  /+
  zStream.total_out = 0;
  zStream.zalloc = (alloc_func)0;
  zStream.zfree = (free_func)0;
  zStream.opaque = (voidpf)0;
  +/

  if (srcStream) {
    MyThreadLocker locker(&lock);
    // read in some initial data
    stpos = srcStream->Tell();
    if (compressedSize == 0xffffffffU) compressedSize = (vuint32)(srcStream->TotalSize()-stpos);
    vint32 bytesToRead = BUFFER_SIZE;
    if (bytesToRead > (int)compressedSize) bytesToRead = (int)compressedSize;
    srcStream->Seek(stpos);
    srcStream->Serialise(buffer, bytesToRead);
    if (srcStream->IsError()) { setError(); return; }
    srccurpos = stpos+bytesToRead;
    zStream.next_in = buffer;
    zStream.avail_in = bytesToRead;
    // open zip stream
    int err = (zipArchive ? inflateInit2(&zStream, -MAX_WBITS) : inflateInit(&zStream));
    if (err != Z_OK) { setError(); return; }
    initialised = true;
  } else {
    bError = true;
  }
}


VStr VZLibStreamReader::GetName () const { return mFileName.cloneUnique(); }

// turns on CRC checking
void VZLibStreamReader::setCrc (vuint32 acrc) {
  if (doCrcCheck && origCrc32 == acrc) return;
  origCrc32 = acrc;
  doCrcCheck = true;
  currCrc32 = 0;
  forceRewind = true;
}


bool VZLibStreamReader::Close () {
  if (initialised) { inflateEnd(&zStream); initialised = false; }
  //if (srcStream) { delete srcStream; srcStream = nullptr; }
  srcStream = nullptr;
  return !bError;
}


void VZLibStreamReader::setError () {
  if (initialised) { inflateEnd(&zStream); initialised = false; }
  //if (srcStream) { delete srcStream; srcStream = nullptr; }
  srcStream = nullptr;
  bError = true;
}


// just read, no `nextpos` advancement
// returns number of bytes read, -1 on error, or 0 on EOF
// no need to lock here
int VZLibStreamReader::readSomeBytes (void *buf, int len) {
  if (len <= 0) return -1;
  if (!srcStream) return -1;
  if (bError) return -1;
  if (srcStream->IsError()) return -1;

  zStream.next_out = (Bytef *)buf;
  zStream.avail_out = len;
  int bytesRead = 0;
  while (zStream.avail_out > 0) {
    // get more compressed data (if necessary)
    if (zStream.avail_in == 0) {
      vint32 left = (int)compressedSize-(srccurpos-stpos);
      if (left <= 0) break; // eof
      srcStream->Seek(srccurpos);
      if (srcStream->IsError()) return -1;
      vint32 bytesToRead = BUFFER_SIZE;
      if (bytesToRead > left) bytesToRead = left;
      srcStream->Serialise(buffer, bytesToRead);
      if (srcStream->IsError()) return -1;
      srccurpos += bytesToRead;
      zStream.next_in = buffer;
      zStream.avail_in = bytesToRead;
    }
    // unpack some data
    vuint32 totalOutBefore = zStream.total_out;
    int err = inflate(&zStream, /+Z_SYNC_FLUSH+/Z_NO_FLUSH);
    if (err != Z_OK && err != Z_STREAM_END) return -1;
    vuint32 totalOutAfter = zStream.total_out;
    bytesRead += totalOutAfter-totalOutBefore;
    if (err != Z_OK) break;
  }
  if (bytesRead && doCrcCheck) currCrc32 = crc32(currCrc32, (const Bytef *)buf, bytesRead);
  return bytesRead;
}


void VZLibStreamReader::Serialise (void* buf, int len) {
  if (len == 0) return;
  MyThreadLocker locker(&lock);

  if (!initialised || len < 0 || !srcStream || srcStream->IsError()) setError();
  if (bError) return;

  if (currpos > nextpos || forceRewind) {
    //fprintf(stderr, "+++ REWIND <%s>: currpos=%d; nextpos=%d\n", *GetName(), currpos, nextpos);
    // rewind stream
    if (initialised) { inflateEnd(&zStream); initialised = false; }
    vint32 bytesToRead = BUFFER_SIZE;
    if (bytesToRead > (int)compressedSize) bytesToRead = (int)compressedSize;
    memset(&zStream, 0, sizeof(zStream));
    srcStream->Seek(stpos);
    srcStream->Serialise(buffer, bytesToRead);
    if (srcStream->IsError()) { setError(); return; }
    srccurpos = stpos+bytesToRead;
    zStream.next_in = buffer;
    zStream.avail_in = bytesToRead;
    // open zip stream
    int err = (zipArchive ? inflateInit2(&zStream, -MAX_WBITS) : inflateInit(&zStream));
    if (err != Z_OK) { setError(); return; }
    initialised = true;
    currpos = 0;
    forceRewind = false;
    currCrc32 = 0; // why not?
  }

  //if (currpos < nextpos) fprintf(stderr, "+++ SKIPPING <%s>: currpos=%d; nextpos=%d; toskip=%d\n", *GetName(), currpos, nextpos, nextpos-currpos);
  while (currpos < nextpos) {
    char tmpbuf[256];
    int toread = nextpos-currpos;
    if (toread > 256) toread = 256;
    int rd = readSomeBytes(tmpbuf, toread);
    //fprintf(stderr, "+++   SKIPREAD <%s>: currpos=%d; nextpos=%d; rd=%d; read=%d\n", *GetName(), currpos, nextpos, rd, toread);
    if (rd <= 0) { setError(); return; }
    currpos += rd;
  }

  if (nextpos != currpos) { setError(); return; } // just in case

  //fprintf(stderr, "+++ ZREAD <%s>: pos=%d; len=%d; end=%d (%u)\n", *GetName(), currpos, len, currpos+len, uncompressedSize);

  vuint8 *dest = (vuint8 *)buf;
  while (len > 0) {
    int rd = readSomeBytes(dest, len);
    if (rd <= 0) { setError(); return; }
    len -= rd;
    nextpos = (currpos += rd);
    dest += rd;
  }

  if (doCrcCheck && uncompressedSize != 0xffffffffU && (vuint32)nextpos == uncompressedSize) {
    if (currCrc32 != origCrc32) { setError(); return; } // alas
  }
}


void VZLibStreamReader::Seek (int pos) {
  if (bError) return;

  if (pos < 0) { setError(); return; }

  if (uncompressedSize == 0xffffffffU) {
    // size is unknown
    nextpos = pos;
  } else {
    if ((vuint32)pos > uncompressedSize) pos = (vint32)uncompressedSize;
    nextpos = pos;
  }
}


int VZLibStreamReader::Tell () { return nextpos; }


int VZLibStreamReader::TotalSize () {
  if (bError) return 0;
  if (uncompressedSize == 0xffffffffU) {
    // calculate size
    MyThreadLocker locker(&lock);
    for (;;) {
      char tmpbuf[256];
      int rd = readSomeBytes(tmpbuf, 256);
      if (rd < 0) { setError(); return 0; }
      if (rd == 0) break;
      currpos += rd;
    }
    uncompressedSize = (vuint32)currpos;
    //fprintf(stderr, "+++ scanned <%s>: size=%u\n", *GetName(), uncompressedSize);
  }
  return uncompressedSize;
}


bool VZLibStreamReader::AtEnd () { return (bError || nextpos >= TotalSize()); }


// ////////////////////////////////////////////////////////////////////////// //
VZipStreamWriter::VZipStreamWriter (VStream *ADstStream)
  : dstStream(ADstStream)
  , initialised(false)
  , currCrc32(0)
  , doCrcCalc(false)
{
  mythread_mutex_init(&lock);
  bLoading = false;

  // initialise zip stream structure
  zStream.total_in = 0;
  zStream.zalloc = (alloc_func)0;
  zStream.zfree = (free_func)0;
  zStream.opaque = (voidpf)0;

  // open zip stream
  int err = deflateInit(&zStream, Z_BEST_COMPRESSION);
  if (err != Z_OK) { bError = true; return; }
  zStream.next_out = buffer;
  zStream.avail_out = BUFFER_SIZE;
  initialised = true;
}


VZipStreamWriter::~VZipStreamWriter () {
  Close();
  mythread_mutex_destroy(&lock);
}


void VZipStreamWriter::setRequireCrc () {
  if (!doCrcCalc && zStream.total_in == 0) doCrcCalc = true;
}


vuint32 VZipStreamWriter::getCrc32 () const {
  return currCrc32;
}


void VZipStreamWriter::setError () {
  if (initialised) { deflateEnd(&zStream); initialised = false; }
  //if (dstStream) { delete dstStream; dstStream = nullptr; }
  dstStream = nullptr;
  bError = true;
}


void VZipStreamWriter::Serialise (void *buf, int len) {
  if (len == 0) return;
  MyThreadLocker locker(&lock);

  if (!initialised || len < 0 || !dstStream || dstStream->IsError()) setError();
  if (bError) return;

  if (doCrcCalc) currCrc32 = crc32(currCrc32, (const Bytef *)buf, len);

  zStream.next_in = (Bytef *)buf;
  zStream.avail_in = len;
  do {
    zStream.next_out = buffer;
    zStream.avail_out = BUFFER_SIZE;
    int err = deflate(&zStream, Z_NO_FLUSH);
    if (err == Z_STREAM_ERROR) { setError(); return; }
    if (zStream.avail_out != BUFFER_SIZE) {
      dstStream->Serialise(buffer, BUFFER_SIZE-zStream.avail_out);
      if (dstStream->IsError()) { setError(); return; }
    }
  } while (zStream.avail_out == 0);
  //vassert(zStream.avail_in == 0);
}


void VZipStreamWriter::Seek (int pos) {
  setError();
}


void VZipStreamWriter::Flush () {
  MyThreadLocker locker(&lock);

  if (!initialised || !dstStream || dstStream->IsError()) setError();
  if (bError) return;

  zStream.avail_in = 0;
  do {
    zStream.next_out = buffer;
    zStream.avail_out = BUFFER_SIZE;
    int err = deflate(&zStream, Z_FULL_FLUSH);
    if (err == Z_STREAM_ERROR) { setError(); return; }
    if (zStream.avail_out != BUFFER_SIZE) {
      dstStream->Serialise(buffer, BUFFER_SIZE-zStream.avail_out);
      if (dstStream->IsError()) { setError(); return; }
    }
  } while (zStream.avail_out == 0);
  dstStream->Flush();
  if (dstStream->IsError()) { setError(); return; }
}


bool VZipStreamWriter::Close () {
  if (initialised) {
    MyThreadLocker locker(&lock);
    zStream.avail_in = 0;
    do {
      zStream.next_out = buffer;
      zStream.avail_out = BUFFER_SIZE;
      int err = deflate(&zStream, Z_FINISH);
      if (err == Z_STREAM_ERROR) { setError(); return false; }
      if (zStream.avail_out != BUFFER_SIZE) {
        dstStream->Serialise(buffer, BUFFER_SIZE-zStream.avail_out);
        if (dstStream->IsError()) { setError(); return false; }
      }
    } while (zStream.avail_out == 0);
    deflateEnd(&zStream);
  }
  initialised = false;
  return !bError;
}
*/


// ////////////////////////////////////////////////////////////////////////// //
int fsysDiskFileTime (VStr path) { return Sys_FileTime(path); }
bool fsysCreateDirectory (VStr path) { return Sys_CreateDirectory(path); }
bool fsysDirExists (VStr path) { return Sys_DirExists(path); }
void *fsysOpenDir (VStr path) { return Sys_OpenDir(path); }
VStr fsysReadDir (void *adir) { return Sys_ReadDir(adir); }
void fsysCloseDir (void *adir) { Sys_CloseDir(adir); }
double fsysCurrTick () { return Sys_Time(); }


// creates file in `fsysBaseDir`
VStream *fsysCreateFileSafe (VStr fname, bool notrunccate) {
  if (fname.isEmpty()) return nullptr;
  //GLog.Logf(NAME_Debug, "000: <%s>", *fname);
  fname = normalizeFilePath(fname);
  //GLog.Logf(NAME_Debug, "001: <%s>", *fname);
  if (fname.isEmpty() || fname.endsWith("/")) return nullptr;
  fname = fsysBaseDir.appendPath(fname);
  //GLog.Logf(NAME_Debug, "002: <%s>", *fname);
  FILE *fl = fopen(*fname, (notrunccate ? "rb+" : "wb"));
  if (!fl) return nullptr;
  return new VStreamDiskFile(fl, fname, true);
}


bool fsysCreateDirSafe (VStr dirname) {
  if (dirname.isEmpty()) return false;
  dirname = normalizeFilePath(dirname);
  if (dirname.isEmpty()) return false;
  dirname = fsysBaseDir.appendPath(dirname);
  return Sys_CreateDirectory(dirname);
}


// ////////////////////////////////////////////////////////////////////////// //
VStreamMemRead::VStreamMemRead (const vuint8 *adata, vuint32 adatasize)
  : data(adata)
  , datasize(adatasize)
  , pos(0)
{
  if (!data || datasize < 0) { data = nullptr; datasize = 0; }
  bLoading = true;
}

VStreamMemRead::~VStreamMemRead () {
}

void VStreamMemRead::Serialise (void *buf, int count) {
  if (bError) return;
  if (count < 0) { setError(); return; }
  if (count > datasize-pos) { setError(); return; }
  memcpy(buf, data+pos, count);
  pos += count;
}

void VStreamMemRead::Seek (int ofs) {
       if (ofs < 0) ofs = 0;
  else if (ofs > datasize) ofs = datasize;
  pos = ofs;
}

int VStreamMemRead::Tell () { return pos; }
int VStreamMemRead::TotalSize () { return datasize; }
bool VStreamMemRead::AtEnd () { return (pos >= datasize); }
bool VStreamMemRead::Close () { data = nullptr; datasize = pos = 0; return !bError; }


// ////////////////////////////////////////////////////////////////////////// //
VStreamMemWrite::VStreamMemWrite (vint32 areservesize)
  : data(nullptr)
  , datasize(0)
  , pos(0)
{
  bLoading = false;
  if (areservesize > 0) {
    data = (vuint8 *)malloc(areservesize);
    if (!data) { bError = true; return; }
    datasize = areservesize;
  }
}

VStreamMemWrite::~VStreamMemWrite () { Close(); }

void VStreamMemWrite::setError () {
  bError = true;
  if (data) free(data);
  data = nullptr;
  datasize = pos = 0;
}

void VStreamMemWrite::Serialise (void *buf, int count) {
  if (bError) return;
  if (count < 0) { setError(); return; }
  if (pos > 0x3fffffff) { setError(); return; }
  if (0x3fffffff-pos < count) { setError(); return; }
  if (datasize-pos < count) {
    vint32 ndsize = ((pos+count)|0x3ffff)+1;
    vuint8 *nd = (vuint8 *)realloc(data, ndsize);
    if (!nd) { setError(); return; }
    data = nd;
    datasize = ndsize;
  }
  memcpy(data+pos, buf, count);
  pos += count;
}

void VStreamMemWrite::Seek (int ofs) {
       if (ofs < 0) ofs = 0;
  else if (ofs > datasize) ofs = datasize;
  pos = ofs;
}

int VStreamMemWrite::Tell () { return pos; }
int VStreamMemWrite::TotalSize () { return pos; }
bool VStreamMemWrite::AtEnd () { return true; }

bool VStreamMemWrite::Close () {
  if (data) free(data);
  data = nullptr;
  datasize = pos = 0;
  return !bError;
}
