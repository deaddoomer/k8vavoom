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
#include <signal.h>
#include <time.h>
//#include <xmmintrin.h>

#include "vcc_run.h"
#include "../libs/vavoomc/vc_local.h"

#include "modules/mod_sound/sound.h"

#ifdef SERIALIZER_USE_LIBHA
# include "filesys/halib/libha.h"
#endif

#include "modules/sdlgl/mod_sdlgl.h"


// ////////////////////////////////////////////////////////////////////////// //
//#define DEBUG_OBJECT_LOADER


// ////////////////////////////////////////////////////////////////////////// //
VObject *mainObject = nullptr;
VStr appName;
bool compileOnly = false;
bool writeToConsole = true; //FIXME
bool dumpProfile = false;
static bool isGDB = false;


// ////////////////////////////////////////////////////////////////////////// //
static VStr buildConfigName (const VStr &optfile) {
  for (int f = 0; f < optfile.length(); ++f) {
    char ch = optfile[f];
    if (ch >= '0' && ch <= '9') continue;
    if (ch >= 'A' && ch <= 'Z') continue;
    if (ch >= 'a' && ch <= 'z') continue;
    if (ch == '_' || ch == ' ' || ch == '.') continue;
    return VStr();
  }
#ifdef _WIN32
  if (optfile.length()) {
    return fsysGetBinaryPath()+VStr(".")+optfile+".cfg";
  } else {
    return fsysGetBinaryPath()+VStr(".options.cfg");
  }
#else
  if (optfile.length()) {
    return VStr(".")+optfile+".cfg";
  } else {
    return VStr(".options.cfg");
  }
#endif
}


// ////////////////////////////////////////////////////////////////////////// //
#include "vcc_run_serializer.cpp"
#include "vcc_run_net.cpp"


// ////////////////////////////////////////////////////////////////////////// //
struct VFNEvProcList {
  VCC_ProcessEventFn handler;
  void *udata;
  VFNEvProcList *next;
};

static VFNEvProcList *evProcListHead = nullptr;


void RegisterEventProcessor (VCC_ProcessEventFn handler, void *udata) {
  if (!handler) return;
  VFNEvProcList *prev = nullptr;
  for (VFNEvProcList *curr = evProcListHead; curr; prev = curr, curr = curr->next) {
    if (curr->handler == handler && curr->udata == udata) return;
  }
  VFNEvProcList *c = (VFNEvProcList *)Z_Calloc(sizeof(VFNEvProcList));
  if (prev) prev->next = c; else evProcListHead = c;
  c->handler = handler;
  c->udata = udata;
  c->next = nullptr;
}


void UnregisterEventProcessor (VCC_ProcessEventFn handler, void *udata) {
  if (!handler) return;
  VFNEvProcList *prev = nullptr;
  for (VFNEvProcList *curr = evProcListHead; curr; prev = curr, curr = curr->next) {
    if (curr->handler == handler && curr->udata) {
      if (prev) prev->next = curr->next; else evProcListHead = curr->next;
      Z_Free(curr);
      return;
    }
  }
}


// timer API for non-SDL apps
// returns timer id, or 0 on error
struct VCCTimer {
  int id;
  unsigned mstimeout;
  uint64_t fireTime;
  bool oneShot;
};

static TArray<VCCTimer> vccTimers;
static uint64_t nextTimerTimeout = 0; //0xffffffffffffffffULL;


static int VCC_AddTimer (int mstimeout, bool oneShot) {
  if (mstimeout < 0) return 0;
  int n = 0;
  while (n < vccTimers.length() && vccTimers[n].id) ++n;
  VCCTimer *tm;
  if (n >= vccTimers.length()) {
    tm = &vccTimers.alloc();
    n = vccTimers.length();
  } else {
    tm = &vccTimers[n++];
  }
  tm->id = n;
  tm->mstimeout = (unsigned)mstimeout;
  Sys_Time_Ex(&tm->fireTime);
  tm->fireTime += tm->mstimeout;
  tm->oneShot = oneShot;
  if (mstimeout == 0 && !oneShot) tm->mstimeout = 1u;
  if (!nextTimerTimeout || tm->fireTime < nextTimerTimeout) nextTimerTimeout = tm->fireTime;
  return tm->id;
}


int VCC_SetTimer (int mstimeout) {
  return VCC_AddTimer(mstimeout, true);
}

int VCC_SetInterval (int mstimeout) {
  return VCC_AddTimer(mstimeout, false);
}


void VCC_CancelInterval (int iid) {
  if (iid < 1 || iid >= vccTimers.length()) return;
  if (!vccTimers[iid].id) return;
  vccTimers[iid].id = 0;
  int ffree = vccTimers.length();
  while (ffree > 0 && !vccTimers[ffree].id) --ffree;
  if (ffree != vccTimers.length()) vccTimers.setLength(ffree, false); // do not resize array storage
  nextTimerTimeout = 0;
  for (auto &&tm : vccTimers) {
    if (!tm.id) continue;
    if (!nextTimerTimeout || tm.fireTime < nextTimerTimeout) nextTimerTimeout = tm.fireTime;
  }
}


// return max timeout or -1; never zero
static int VCC_ProcessTimers () {
  if (vccTimers.length() == 0) {
    nextTimerTimeout = 0;
    return -1;
  }

  uint64_t ctt = 0;
  Sys_Time_Ex(&ctt);

  int firstfree = 0;
  int currtm = 0;

  for (auto &&tm : vccTimers) {
    //GLog.Logf("TM#%u: id=%d; to=%u; ftm=%llu", currtm, tm.id, tm.mstimeout, tm.fireTime);
    ++currtm;
    if (!tm.id) continue;
    if (tm.fireTime <= ctt) {
      event_t ev;
      ev.clear();
      ev.type = ev_timer;
      ev.timerid = tm.id;
      VObject::PostEvent(ev);
      if (tm.oneShot) {
        tm.id = 0;
      } else {
        tm.fireTime = ctt+tm.mstimeout;
        firstfree = currtm;
      }
    } else {
      firstfree = currtm;
    }
  }

  if (firstfree != vccTimers.length()) vccTimers.setLength(firstfree, false); // do not resize array storage

  nextTimerTimeout = 0;
  for (auto &&tm : vccTimers) {
    if (!tm.id) continue;
    if (!nextTimerTimeout || tm.fireTime < nextTimerTimeout) nextTimerTimeout = tm.fireTime;
  }

  //GLog.Logf(NAME_Debug, "ntt=%llu", nextTimerTimeout);

  if (nextTimerTimeout <= ctt) return -1;
  if (nextTimerTimeout-ctt > 1000000) return 1000000;
  return (int)(nextTimerTimeout-ctt);
}


static mythread_event eventLoopPingEvent;
static atomic_int quitEventPosted = 0;


static void VCC_PingEventLoopDefault () {
  //GLog.Logf(NAME_Debug, "pinging event loop (%d)...", VObject::CountQueuedEvents());
  mythread_event_signal(&eventLoopPingEvent);
}


static void VCC_WaitEventsDefault () {
  for (;;) {
    const int ttout = VCC_ProcessTimers();
    if (VObject::CountQueuedEvents()) {
      mythread_event_reset(&eventLoopPingEvent);
      return;
    }
    if (ttout < 0) {
      mythread_event_wait(&eventLoopPingEvent, nullptr);
      return;
    }
    //GLog.Logf(NAME_Debug, "ttout=%d", ttout);
    (void)mythread_event_wait_timeout(&eventLoopPingEvent, ttout, nullptr);
  }
}


VCC_WaitEventsFn VCC_WaitEvents = &VCC_WaitEventsDefault;
VCC_PingEventLoopFn VCC_PingEventLoop = &VCC_PingEventLoopDefault;


int VCC_RunEventLoop (event_t *quitev) {
  VClass *mklass = VClass::FindClass("Main");
  if (!mklass) Sys_Error("class `Main` not found!");

  VMethod *onEventVC = mklass->FindMethod("onEvent");
  if (onEventVC && (onEventVC->Flags&FUNC_VarArgs) == 0 && onEventVC->ReturnType.Type == TYPE_Void && onEventVC->NumParams == 1 &&
      ((onEventVC->ParamTypes[0].Type == TYPE_Pointer &&
        onEventVC->ParamFlags[0] == 0 &&
        onEventVC->ParamTypes[0].GetPointerInnerType().Type == TYPE_Struct &&
        onEventVC->ParamTypes[0].GetPointerInnerType().Struct->Name == "event_t") ||
       ((onEventVC->ParamFlags[0]&(FPARM_Out|FPARM_Ref)) != 0 &&
        onEventVC->ParamTypes[0].Type == TYPE_Struct &&
        onEventVC->ParamTypes[0].Struct->Name == "event_t")))
  {
    // ok
  } else {
    Sys_Error("no suitable `onEvent` method found!");
  }

  //GLog.Log(NAME_Debug, "VCC_RunEventLoop: started");
  double lastGetEvent = -1.0;
  event_t ev;
  for (;;) {
    //GLog.Logf(NAME_Debug, "VCC_RunEventLoop: waiting for events (%d)...", VObject::CountQueuedEvents());
    /*if (VObject::CountQueuedEvents() == 0)*/ VCC_WaitEvents();
    //GLog.Logf(NAME_Debug, "VCC_RunEventLoop: has %d events to process...", VObject::CountQueuedEvents());

    bool wasEvent = false;
    for (int ecount = VObject::CountQueuedEvents(); ecount > 0; --ecount) {
      ev.clear();
      if (!VObject::GetEvent(&ev)) break;
      //lastGetEvent = Sys_Time();
      wasEvent = true;
      if (ev.type == ev_quit) atomic_set(&quitEventPosted, 0);

      // run handlers
      VFNEvProcList *c = evProcListHead;
      if (c) {
        while (c) {
          VFNEvProcList *cc = c;
          c = c->next;
          cc->handler(ev, cc->udata);
          if (ev.isEatenOrCancelled()) break;
        }
      } else if (onEventVC) {
        if ((onEventVC->Flags&FUNC_Static) == 0) P_PASS_REF((VObject *)mainObject);
        P_PASS_REF((event_t *)&ev);
        VObject::ExecuteFunction(onEventVC);
      }

      if (ev.type == ev_socket) {
        sockmodAckEvent(ev.sockev, ev.sockid, ev.sockdata, ev.isEaten(), ev.isCancelled());
      }
      if (ev.type == ev_quit && !ev.isEatenOrCancelled()) {
        if (quitev) *quitev = ev;
        return ev.data1;
      }
      if (ev.type == ev_closequery && !ev.isEatenOrCancelled()) {
        PostQuitEvent(0);
      }
    }

    const int qposted = atomic_get(&quitEventPosted);
    if (qposted == 2) {
      if (VObject::PostEvent(ev)) atomic_set(&quitEventPosted, 1);
    }

    const double ctt = Sys_Time();
    if (lastGetEvent < 0.0) lastGetEvent = ctt;
    if (!wasEvent) {
      // minute passed, ping resulted in no events
      if (ctt-lastGetEvent > 30.0) Sys_Error("fatal error in `VCC_WaitEvent()`");
    } else {
      lastGetEvent = ctt;
    }
  }
}


// `exitcode` will be put to `data1`
void PostQuitEvent (int exitcode) {
  const int qposted = atomic_get(&quitEventPosted);
  // do not spam
  if (qposted != 1) {
    event_t ev;
    ev.clear();
    ev.type = ev_quit;
    ev.data1 = exitcode;
    if (!VObject::PostEvent(ev)) {
      atomic_set(&quitEventPosted, 2);
    } else {
      atomic_set(&quitEventPosted, 1);
    }
  }
  VCC_PingEventLoop();
}


// ////////////////////////////////////////////////////////////////////////// //
#if defined(WIN32)
# include <windows.h>
#endif

static __attribute__((noreturn, format(printf, 1, 2))) void InternalHostError (const char *error, ...) {
#if defined(VCC_STANDALONE_EXECUTOR) && defined(WIN32)
  static char workString[1024];
  va_list argPtr;
  va_start(argPtr, error);
  vsnprintf(workString, sizeof(workString), error, argPtr);
  va_end(argPtr);
  MessageBox(NULL, workString, "VaVoom/C Runner Fatal Error", MB_OK);
#else
  fprintf(stderr, "FATAL: ");
  va_list argPtr;
  va_start(argPtr, error);
  vfprintf(stderr, error, argPtr);
  va_end(argPtr);
  fprintf(stderr, "\n");
#endif
  if (!isGDB) exit(1);
  abort();
}


void __attribute__((noreturn)) __declspec(noreturn) VPackage::HostErrorBuiltin (VStr msg) { InternalHostError("%s", *msg); }
void __attribute__((noreturn)) __declspec(noreturn) VPackage::SysErrorBuiltin (VStr msg) { Sys_Error("%s", *msg); }
void __attribute__((noreturn)) __declspec(noreturn) VPackage::AssertErrorBuiltin (VStr msg) { Sys_Error("Assertion failure: %s", *msg); }

void __attribute__((noreturn)) __declspec(noreturn) VPackage::ExecutorAbort (const char *msg) { InternalHostError("%s", msg); }
void __attribute__((noreturn)) __declspec(noreturn) VPackage::IOError (const char *msg) { InternalHostError("I/O Error: %s", msg); }

void __attribute__((noreturn)) __declspec(noreturn) VPackage::CompilerFatalError (const char *msg) { Sys_Error("%s", msg); }
void __attribute__((noreturn)) __declspec(noreturn) VPackage::InternalFatalError (const char *msg) { Sys_Error("%s", msg); }


static void OnSysError (const char *msg) {
  fprintf(stderr, "FATAL: %s\n", msg);
  if (!isGDB) exit(1);
  abort();
}


// ////////////////////////////////////////////////////////////////////////// //
/*
class VVccLog : public VLogListener {
public:
  virtual void Serialise (const char* text, EName event) override {
    devprintf("%s", text);
  }
};
*/


// ////////////////////////////////////////////////////////////////////////// //
__attribute__((noreturn, format(printf, 1, 2))) void Host_Error (const char *error, ...) {
  va_list argptr;
  static char buf[16384]; //WARNING! not thread-safe!

  va_start(argptr,error);
  vsnprintf(buf, sizeof(buf), error, argptr);
  va_end(argptr);

  Sys_Error("%s", buf);
}


// ////////////////////////////////////////////////////////////////////////// //
static VStr SourceFileName;
static TArray<VStr> scriptArgs;

//static int num_dump_asm;
//static const char *dump_asm_names[1024];
static bool DebugMode = false;
static FILE *DebugFile;

static VLexer Lex;
//static VVccLog VccLog;


// ////////////////////////////////////////////////////////////////////////// //
// if `buf` is `nullptr`, it means "flush"
static void vmWriter (const char *buf, bool debugPrint, VName wrname) {
  if (debugPrint && !DebugMode) return;
  if (!buf) {
    if (writeToConsole) conPutChar('\n'); else printf("\n");
  } else if (buf[0]) {
    if (writeToConsole) conWriteStr(buf, strlen(buf)); else printf("%s", buf);
  }
}


//==========================================================================
//
//  devprintf
//
//==========================================================================
__attribute__((format(printf, 1, 2))) int devprintf (const char *text, ...) {
  if (!DebugMode) return 0;
  va_list argPtr;
  FILE* fp = stderr; //(DebugFile ? DebugFile : stdout);
  va_start(argPtr, text);
  int ret = vfprintf(fp, text, argPtr);
  va_end(argPtr);
  fflush(fp);
  return ret;
}


//==========================================================================
//
//  Malloc
//
//==========================================================================
void* Malloc (size_t size) {
  if (!size) return nullptr;
  void *ptr = Z_Malloc(size);
  if (!ptr) VCFatalError("FATAL: couldn't alloc %d bytes", (int)size);
  memset(ptr, 0, size);
  return ptr;
}


//==========================================================================
//
//  Free
//
//==========================================================================
void Free (void* ptr) {
  if (ptr) Z_Free(ptr);
}


//==========================================================================
//
//  OpenFile
//
//==========================================================================
VStream* OpenFile (const VStr& Name) {
  return fsysOpenFile(Name);
  /*
  FILE* file = fopen(*Name, "rb");
  return (file ? new VFileReader(file) : nullptr);
  */
}


//==========================================================================
//
//  OpenDebugFile
//
//==========================================================================
static void OpenDebugFile (const VStr& name) {
  DebugFile = fopen(*name, "w");
  if (!DebugFile) VCFatalError("FATAL: can\'t open debug file \"%s\".", *name);
}


/*
//==========================================================================
//
//  PC_DumpAsm
//
//==========================================================================
static void PC_DumpAsm (const char* name) {
  char buf[1024];
  char *cname;
  char *fname;

  snprintf(buf, sizeof(buf), "%s", name);

  //FIXME! PATH WITH DOTS!
  if (strstr(buf, ".")) {
    cname = buf;
    fname = strstr(buf, ".")+1;
    fname[-1] = 0;
  } else {
    devprintf("Dump ASM: Bad name %s\n", name);
    return;
  }

  //printf("<%s>.<%s>\n", cname, fname);

  for (int i = 0; i < VMemberBase::GMembers.Num(); ++i) {
    //if (VMemberBase::GMembers[i]->MemberType == MEMBER_Method) printf("O:<%s>; N:<%s>\n", *VMemberBase::GMembers[i]->Outer->Name, *VMemberBase::GMembers[i]->Name);
    if (VMemberBase::GMembers[i]->MemberType == MEMBER_Method &&
        !VStr::Cmp(cname, *VMemberBase::GMembers[i]->Outer->Name) &&
        !VStr::Cmp(fname, *VMemberBase::GMembers[i]->Name))
    {
      ((VMethod*)VMemberBase::GMembers[i])->DumpAsm();
      return;
    }
  }

  devprintf("Dump ASM: %s not found!\n", name);
}


//==========================================================================
//
//  DumpAsm
//
//==========================================================================
static void DumpAsm () {
  for (int i = 0; i < num_dump_asm; ++i) PC_DumpAsm(dump_asm_names[i]);
}
*/


//==========================================================================
//
//  DisplayUsage
//
//==========================================================================
static void DisplayUsage () {
  printf("\n");
  printf("VCCRUN Copyright (c) 2000-2001 by JL, 2018,2019 by Ketmar Dark. (" __DATE__ " " __TIME__ "; opcodes: %d)\n", NUM_OPCODES);
  printf("Usage: vcc [options] source[.c] [object[.dat]]\n");
  printf("    -d<file>     Output debugging information into specified file\n");
  printf("    -a<function> Output function's ASM statements into debug file\n");
  printf("    -D<name>           Define macro\n");
  printf("    -I<directory>      Include files directory\n");
  printf("    -P<directory>      Package import files directory\n");
  printf("    -base <directory>  Set base directory for fsys\n");
  printf("    -file <name>       Add pak file\n");
  exit(1);
}


//==========================================================================
//
//  ProcessArgs
//
//==========================================================================
static void ProcessArgs (int ArgCount, char **ArgVector) {
  int count = 0; // number of file arguments
  bool nomore = false;

  VObject::cliCaseSensitiveLocals = 1;
  VObject::cliCaseSensitiveFields = 1;

  TArray<VStr> paklist;

  for (int i = 1; i < ArgCount; ++i) {
    const char *text = ArgVector[i];
    if (!text[0]) continue;
    if (count == 0 && !nomore && *text == '-') {
      ++text;
      if (*text == 0) DisplayUsage();
      if (text[0] == '-' && text[1] == 0) { nomore = true; continue; }
      if (strcmp(text, "gdb") == 0 || strcmp(text, "-gdb") == 0) {
        isGDB = true;
        continue;
      }
      if (strcmp(text, "allow-save") == 0 || strcmp(text, "-allow-save") == 0) {
        VGLTexture::savingAllowed = true;
        continue;
      }
      if (strcmp(text, "vc-case-insensitive-locals") == 0) { VObject::cliCaseSensitiveLocals = 0; continue; }
      if (strcmp(text, "vc-case-insensitive-fields") == 0) { VObject::cliCaseSensitiveFields = 0; continue; }
      if (strcmp(text, "vc-case-sensitive-locals") == 0) { VObject::cliCaseSensitiveLocals = 1; continue; }
      if (strcmp(text, "vc-case-sensitive-fields") == 0) { VObject::cliCaseSensitiveFields = 1; continue; }
      const char option = *text++;
      switch (option) {
        case 'd': DebugMode = true; if (*text) OpenDebugFile(text); break;
        case 'c': compileOnly = true; break;
        case 'a': /*if (!*text) DisplayUsage(); dump_asm_names[num_dump_asm++] = text;*/ VMemberBase::doAsmDump = true; break;
        case 'I': VMemberBase::StaticAddIncludePath(text); break;
        case 'D': VMemberBase::StaticAddDefine(text); break;
        case 'P': VMemberBase::StaticAddPackagePath(text); break;
        case 'W':
               if (strcmp(text, "all") == 0) VMemberBase::WarningUnusedLocals = true;
          else if (strcmp(text, "no") == 0) VMemberBase::WarningUnusedLocals = false;
          else Sys_Error("invalid '-W' option");
          break;
        case 'h': case 'v': DisplayUsage(); break;
        default:
          --text;
          if (VStr::Cmp(text, "nocol") == 0) {
            vcErrorIncludeCol = false;
          } else if (VStr::Cmp(text, "stderr-backtrace") == 0) {
            VObject::DumpBacktraceToStdErr = true;
          } else if (VStr::Cmp(text, "profile") == 0) {
            dumpProfile = true;
          } else if (VStr::Cmp(text, "base") == 0) {
            ++i;
            if (i >= ArgCount) DisplayUsage();
            fsysBaseDir = VStr(ArgVector[i]);
          } else if (VStr::Cmp(text, "file") == 0) {
            ++i;
            if (i >= ArgCount) DisplayUsage();
            paklist.append(VStr(":")+VStr(ArgVector[i]));
            //fprintf(stderr, "<%s>\n", ArgVector[i]);
          } else if (VStr::Cmp(text, "pakdir") == 0) {
            ++i;
            if (i >= ArgCount) DisplayUsage();
            paklist.append(VStr("/")+VStr(ArgVector[i]));
            //fprintf(stderr, "<%s>\n", ArgVector[i]);
          } else {
            //fprintf(stderr, "*<%s>\n", text);
            DisplayUsage();
          }
          break;
      }
      continue;
    }
    ++count;
    switch (count) {
      case 1: SourceFileName = VStr(text).DefaultExtension(".vc"); break;
      default: scriptArgs.Append(VStr(text)); break;
    }
  }

/* this is hacked into filesys
  //VMemberBase::StaticAddIncludePath(".");
  VMemberBase::StaticAddPackagePath(".");
  VMemberBase::StaticAddPackagePath("./packages");

  auto mydir = getBinaryDir();
  //fprintf(stderr, "<%s>\n", mydir);
  //VMemberBase::StaticAddIncludePath(mydir);
  VMemberBase::StaticAddPackagePath(mydir);

  VStr mypkg = VStr(mydir)+"/packages";
  //VMemberBase::StaticAddIncludePath(*mypkg);
  VMemberBase::StaticAddPackagePath(*mypkg);
*/

  /*
  if (!DebugFile) {
    VStr DbgFileName;
    DbgFileName = ObjectFileName.StripExtension()+".txt";
    OpenDebugFile(DbgFileName);
    DebugMode = true;
  }
  */

  bool gdatforced = false;
  if (paklist.length() == 0) {
    //fprintf(stderr, "forcing 'game.dat'\n");
    paklist.append(":game.dat");
    gdatforced = true;
  }

  fsysInit();
  for (int f = 0; f < paklist.length(); ++f) {
    VStr pname = paklist[f];
    if (pname.length() < 2) continue;
    char type = pname[0];
    pname.chopLeft(1);
    //fprintf(stderr, "!%c! <%s>\n", type, *pname);
    if (type == ':') {
      if (gdatforced) {
        //fprintf(stderr, "!!!000\n");
        VStream *pstm = fsysOpenFile(pname);
        if (pstm && fsysAppendPak(pstm)) {
          //fprintf(stderr, "!!!001\n");
          devprintf("added pak file '%s'...\n", *pname);
        } else {
          //fprintf(stderr, "CAN'T add pak file '%s'!\n", *pname);
        }
      } else {
        if (fsysAppendPak(pname)) {
          devprintf("added pak file '%s'...\n", *pname);
        } else {
          if (!gdatforced) fprintf(stderr, "CAN'T add pak file '%s'!\n", *pname);
        }
      }
    } else if (type == '/') {
      if (fsysAppendDir(pname)) {
        devprintf("added pak directory '%s'...\n", *pname);
      } else {
        if (!gdatforced) fprintf(stderr, "CAN'T add pak directory '%s'!\n", *pname);
      }
    }
  }

  if (count == 0) {
    if (SourceFileName.isEmpty()) {
      SourceFileName = fsysForEachPakFile([] (VStr fname) { /*fprintf(stderr, ":<%s>\n", *fname);*/ return fname.endsWith("main.vc"); });
    }
    auto dir = fsysOpenDir(".");
    if (dir) {
      for (;;) {
        auto fname = fsysReadDir(dir);
        if (fname.isEmpty()) break;
        //fprintf(stderr, "<%s>\n", *fname);
        if (fname.endsWith("main.vc")) {
          SourceFileName = fname;
          break;
        }
      }
      fsysCloseDir(dir);
    }
    if (SourceFileName.isEmpty()) DisplayUsage();
  }

  SourceFileName = SourceFileName.fixSlashes();
  devprintf("Main source file: %s\n", *SourceFileName);
}


//==========================================================================
//
//  initialize
//
//==========================================================================
static void initialize () {
  DebugMode = false;
  DebugFile = nullptr;
  //num_dump_asm = 0;
  VName::StaticInit();
  //VMemberBase::StaticInit();
  VObject::StaticInit();
  VMemberBase::StaticAddDefine("VCC_STANDALONE_EXECUTOR");
#ifdef VCCRUN_HAS_SDL
  VMemberBase::StaticAddDefine("VCCRUN_HAS_SDL");
#endif
#ifdef VCCRUN_HAS_OPENGL
  VMemberBase::StaticAddDefine("VCCRUN_HAS_OPENGL");
#endif
#ifdef VCCRUN_HAS_OPENAL
  VMemberBase::StaticAddDefine("VCCRUN_HAS_OPENAL");
#endif
  VMemberBase::StaticAddDefine("VCCRUN_HAS_IMAGO");
  VObject::onExecuteNetMethodCB = &onExecuteNetMethod;
  VCvar::Init();
}


// ////////////////////////////////////////////////////////////////////////// //
// <0: error; bit 0: has arg; bit 1: returns int; bit 2: slice
static int checkArg (VMethod *mmain) {
  if (!mmain) return -1;
  if ((mmain->Flags&FUNC_VarArgs) != 0) return -1;
  //if (mmain->NumParams > 0 && mmain->ParamFlags[0] != 0) return -1;
  int res = 0;
  if (mmain->ReturnType.Type != TYPE_Void && mmain->ReturnType.Type != TYPE_Int) return -1;
  if (mmain->ReturnType.Type == TYPE_Int || mmain->ReturnType.Type == TYPE_Byte || mmain->ReturnType.Type == TYPE_Bool) res |= 0x02;
  if (mmain->NumParams != 0) {
    if (mmain->NumParams != 1) return -1;
    if (mmain->ParamFlags[0] == 0) {
      VFieldType atp = mmain->ParamTypes[0];
      devprintf("  ptype0: %s\n", *atp.GetName());
      if (atp.Type == TYPE_SliceArray) {
        atp = atp.GetArrayInnerType();
        if (atp.Type != TYPE_String) return -1;
        res |= 0x04;
      } else {
        if (atp.Type != TYPE_Pointer) return -1;
        atp = atp.GetPointerInnerType();
        if (atp.Type != TYPE_DynamicArray) return -1;
        atp = atp.GetArrayInnerType();
        if (atp.Type != TYPE_String) return -1;
        res |= 0x01;
      }
    } else if ((mmain->ParamFlags[0]&(FPARM_Out|FPARM_Ref)) != 0) {
      VFieldType atp = mmain->ParamTypes[0];
      devprintf("  ptype1: %s\n", *atp.GetName());
      if (atp.Type != TYPE_DynamicArray) return -1;
      atp = atp.GetArrayInnerType();
      if (atp.Type != TYPE_String) return -1;
      res |= 0x01;
    }
  }
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
int main (int argc, char **argv) {
  VFuncRes ret((int)0);

  GLogErrorToStderr = true;
  GLogWarningToStderr = true;

  //_mm_setcsr(_mm_getcsr()&~0x8040); // disable DAZ and FTZ
  //_mm_setcsr(_mm_getcsr()|0x8040); // enable DAZ and FTZ

  RandomInit();
  VCvar::Init();
  VCvar::HostInitComplete();

  VObject::standaloneExecutor = 1; // required
  VObject::engineAllowNotImplementedBuiltins = 1;
  VObject::cliShowUndefinedBuiltins = 0; // for now
  VMemberBase::StaticAddDefine("IN_VCC_RUN");

  srand(time(nullptr));
  SysErrorCB = &OnSysError;
  VObject::PR_WriterCB = &vmWriter;

  if (mythread_event_init(&eventLoopPingEvent, MYTHREAD_EVENT_AUTORESET) != 0) Sys_Error("cannot create event loop internal signal event");

  try {
    //GLog.AddListener(&VccLog);

    int starttime;
    int endtime;

    starttime = time(0);

    initialize();

    ProcessArgs(argc, argv);

    VObject::PR_Init();

    VMemberBase::StaticLoadPackage(VName("engine"), TLocation());
    //VMemberBase::StaticLoadPackage(VName("ui"), TLocation());
    // this emits code for all `PackagesToEmit()`
    VPackage::StaticEmitPackages();

    VPackage *CurrentPackage = new VPackage(VName("vccrun"));

    devprintf("Compiling '%s'...\n", *SourceFileName);

    VStream *strm = OpenFile(SourceFileName);
    if (!strm) {
      VCFatalError("FATAL: cannot open file '%s'", *SourceFileName);
    }

    CurrentPackage->LoadSourceObject(strm, SourceFileName, TLocation());
    VPackage::StaticEmitPackages();
    devprintf("Total memory used: %u\n", VExpression::TotalMemoryUsed);
    //DumpAsm();
    endtime = time(0);
    devprintf("Time elapsed: %02d:%02d\n", (endtime-starttime)/60, (endtime-starttime)%60);
    // free compiler memory
    VMemberBase::StaticCompilerShutdown();
    devprintf("Peak compiler memory usage: %s bytes.\n", comatoze(VExpression::PeakMemoryUsed));
    devprintf("Released compiler memory  : %s bytes.\n", comatoze(VExpression::TotalMemoryFreed));
    if (VExpression::CurrMemoryUsed != 0) {
      devprintf("Compiler leaks %s bytes (this is harmless).\n", comatoze(VExpression::CurrMemoryUsed));
    }

    VScriptArray scargs(scriptArgs);
    VClass *mklass = VClass::FindClass("Main");
    if (mklass && !compileOnly) {
      devprintf("Found class 'Main'\n");
      VMethod *mmain = mklass->FindAccessibleMethod("main");
      if (!mmain) mmain = mklass->FindAccessibleMethod("runmain");
      if (mmain) {
        devprintf(" Found method 'main()' (return type: %u:%s)\n", mmain->ReturnType.Type, *mmain->ReturnType.GetName());
        int atp = checkArg(mmain);
        if (atp < 0) VCFatalError("Main::main() should be either arg-less, or have one `array!string*` argument, and should be either `void`, or return `int`!");
        auto sss = VObject::VMGetStackPtr();
        mainObject = VObject::StaticSpawnWithReplace(mklass);
        if ((mmain->Flags&FUNC_Static) == 0) {
          //auto imain = SpawnWithReplace<VLevel>();
          P_PASS_REF((VObject *)mainObject);
        }
        if (atp&0x01) {
          // dunarray
          P_PASS_PTR(&scargs);
        } else if (atp&0x04) {
          // slice
          P_PASS_PTR(scargs.Ptr());
          P_PASS_INT(scargs.length());
        }
        ret = VObject::ExecuteFunction(mmain);
        if ((atp&0x02) == 0) ret = VFuncRes((int)0);
        if (sss != VObject::VMGetStackPtr()) VCFatalError("FATAL: stack imbalance!");
      }
    }

    if (dumpProfile) VObject::DumpProfile();
    VSoundManager::StaticShutdown();
    //VCvar::Shutdown();
    //VObject::StaticExit();
    //VName::StaticExit();
  } catch (VException &e) {
    ret = VFuncRes((int)-1);
#ifndef WIN32
    VCFatalError("FATAL: %s", e.What());
#else
    VCFatalError("%s", e.What());
#endif
  }
  VObject::StaticExit();

  Z_ShuttingDown();
  return ret.getInt();
}


#include "vcc_run_vobj.cpp"
