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
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#ifdef VAVOOM_CUSTOM_SPECIAL_SDL
# include <SDL.h>
#else
# include <SDL2/SDL.h>
#endif
#if !defined(_WIN32) && !defined(__SWITCH__)
#elif defined(__SWITCH__)
# include <switch.h>
#endif

#if defined(USE_FPU_MATH)
# define VAVOOM_ALLOW_FPU_DEBUG
#elif defined(__linux__)
# if defined(__x86_64__) || defined(__i386__)
#  define VAVOOM_ALLOW_FPU_DEBUG
# endif
#endif

#if !defined(VAVOOM_K8_DEVELOPER) && defined(VAVOOM_ALLOW_FPU_DEBUG)
# undef VAVOOM_ALLOW_FPU_DEBUG
#endif

#ifdef VAVOOM_ALLOW_FPU_DEBUG
# ifndef _GNU_SOURCE
#  define _GNU_SOURCE
# endif
# include <fenv.h>
#endif

#include "gamedefs.h"
#include "host.h"
#include "filesys/files.h"


//==========================================================================
//
//  Sys_Shutdown
//
//==========================================================================
void Sys_Shutdown () {
}


//==========================================================================
//
//  PutEndText
//
//  Function to write the Doom end message text
//
//  Copyright (C) 1998 by Udo Munk <udo@umserver.umnet.de>
//
//  This code is provided AS IS and there are no guarantees, none.
//  Feel free to share and modify.
//
//==========================================================================
static void PutEndText (const char *text) {
  int i, j;
  int att = -1;
  int nlflag = 0;
  char *col;

  // if option -noendtxt is set, don't print the text.
  if (cli_ShowEndText <= 0) return;

  // if the xterm has more then 80 columns we need to add nl's
  col = getenv("COLUMNS");
  if (col) {
    if (VStr::atoi(col) > 80) ++nlflag;
  } else {
    ++nlflag;
  }

  // print 80x25 text and deal with the attributes too
  for (i = 1; i <= 80 * 25; ++i, text += 2) {
    // attribute first
    j = (vuint8)text[1];
    // attribute changed?
    if (j != att) {
      /*static*/ const char map[] = "04261537";
      // save current attribute
      att = j;
      // set new attribute: bright, foreground, background (we don't have bright background)
      printf("\033[0;%s3%c;4%cm", (j & 0x88) ? "1;" : "", map[j & 7], map[(j & 0x70) >> 4]);
    }

    // now the text
    if (*text < 32) putchar('.'); else putchar(*text);

    // do we need a nl?
    if (nlflag && !(i % 80)) {
      att = 0;
      puts("\033[0m");
    }
  }

  // all attributes off
  printf("\033[0m");

  if (nlflag) printf("\n");
}


//==========================================================================
//
//  Sys_Quit
//
//  Shuts down net game, saves defaults, prints the exit text message,
// goes to text mode, and exits.
//
//==========================================================================
void Sys_Quit (const char *EndText) {
  // shutdown system
  Host_Shutdown();
  if (developer) GLog.Log(NAME_Dev, "calling `SDL_Quit()`");
  SDL_Quit();
  // throw the end text at the screen
  if (EndText) {
    if (developer) GLog.Log(NAME_Dev, "showing endtext");
    PutEndText(EndText);
  }
  // exit
  if (developer) GLog.Log(NAME_Dev, "exiting");
#ifdef _WIN32
  //ExitProcess(0);
  //TerminateProcess(GetCurrentProcess(), 0);
#endif
  exit(0);
}


//==========================================================================
//
//  Sys_ConsoleInput
//
//==========================================================================
char *Sys_ConsoleInput () {
  /*
  static char text[256];
  int     len;
  fd_set  fdset;
  struct timeval timeout;

  FD_ZERO(&fdset);
  FD_SET(0, &fdset); // stdin
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  if (select(1, &fdset, nullptr, nullptr, &timeout) == -1 || !FD_ISSET(0, &fdset)) return nullptr;

  len = read(0, text, sizeof(text));
  if (len < 1) return nullptr;
  text[len-1] = 0;    // rip off the /n and terminate

  return text;
  */
  return nullptr;
}


//==========================================================================
//
//  signal_handler
//
//  Shuts down system, on error signal
//
//==========================================================================
static volatile int sigReceived = 0;

static void signal_handler (int /*s*/) {
  sigReceived = 1;
  VObject::vmAbortBySignal += 1;
}


//==========================================================================
//
//  mainloop
//
//==========================================================================
static void mainloop (int argc, char **argv) {
  try {
    Host_InitStreamCallbacks();
    VObject::StaticInitOptions(GParsedArgs);
    FL_InitOptions();
    GArgs.Init(argc, argv, "-file");
    FL_CollectPreinits();
    GParsedArgs.parse(GArgs);

    if (SDL_InitSubSystem(SDL_INIT_VIDEO|SDL_INIT_EVENTS) < 0) Sys_Error("SDL_InitSubSystem(): %s\n", SDL_GetError());

#ifdef ANDROID
    SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");
    SDL_SetHint(SDL_HINT_ANDROID_SEPARATE_MOUSE_AND_TOUCH, "1");
#endif

    // install signal handlers
    signal(SIGTERM, signal_handler);
    signal(SIGINT,  signal_handler);
    #ifndef _WIN32
    signal(SIGQUIT, signal_handler);
    #endif

/*
#if defined(USE_FPU_MATH)
    fprintf(stderr, "DEBUG: setting FPU trap flags\n");
    //feenableexcept(FE_ALL_EXCEPT/ *FE_DIVBYZERO|FE_INVALID|FE_OVERFLOW* /);
    //feenableexcept(FE_DIVBYZERO|FE_INVALID|FE_OVERFLOW);
    feenableexcept(FE_DIVBYZERO|FE_INVALID);
#endif
*/

    #ifdef VAVOOM_ALLOW_FPU_DEBUG
    if (GArgs.CheckParm("-dev-fpu-alltraps") || GArgs.CheckParm("-dev-fpu-all-traps")) {
      feenableexcept(FE_DIVBYZERO|FE_INVALID|FE_OVERFLOW|FE_UNDERFLOW);
    } else if (GArgs.CheckParm("-dev-fpu-traps")) {
      feenableexcept(FE_DIVBYZERO|FE_INVALID);
    } else {
      //GCon->Logf("ROUND: %d (%d); EXCEPT: %d", fegetround(), FLT_ROUNDS, fegetexcept());
      feclearexcept(FE_ALL_EXCEPT);
    }
    // sse math can only round towards zero, so force it for FPU
    if (fesetround(0) != 0) GCon->Logf(NAME_Warning, "Cannot set float rounding mode (this is not fatal)");
    #endif

    Host_Init();
    /*
    GCon->Logf("COLOR 'dark slate gray' is 0x%08x", M_ParseColor(" dark slate  gray "));
    GCon->Logf("COLOR '#222' is 0x%08x", M_ParseColor("#222"));
    GCon->Logf("COLOR '#1234ef' is 0x%08x", M_ParseColor("#1234ef"));
    GCon->Logf("COLOR '12 34 ef' is 0x%08x", M_ParseColor("12 34 ef"));
    GCon->Logf("COLOR '2 3 4' is 0x%08x", M_ParseColor("  2 3  4 "));
    GCon->Logf("COLOR '255 255 255' is 0x%08x", M_ParseColor("255 255 255"));
    */

#ifdef __SWITCH__
    while (appletMainLoop()) {
#else
    for (;;) {
#endif
      Host_Frame();
      if (sigReceived) {
        GCon->Logf("*** SIGNAL RECEIVED ***");
        Host_Shutdown();
        fprintf(stderr, "*** TERMINATED BY SIGNAL ***\n");
        break;
      }
    }
  } catch (VavoomError &e) {
    //printf("\n%s\n", e.message);
    GCon->Logf(NAME_Error, "ERROR: %s", e.message);
    Host_Shutdown();
    if (developer) GLog.Log(NAME_Dev, "calling `SDL_Quit()`");
    SDL_Quit();
    if (developer) GLog.Log(NAME_Dev, "exiting");
    #ifdef _WIN32
    //ExitProcess(1);
    //TerminateProcess(GetCurrentProcess(), 1);
    #endif
    exit(1);
  }
  if (developer) GLog.Log(NAME_Dev, "calling `SDL_Quit()`");
  SDL_Quit();
  if (developer) GLog.Log(NAME_Dev, "mainloop complete");
}


//==========================================================================
//
//  main
//
//  Main program
//
//==========================================================================
int main (int argc, char **argv) {
#ifdef __SWITCH__
  socketInitializeDefault();
# ifdef SWITCH_NXLINK
  nxlinkStdio();
# endif
#endif

#ifdef ANDROID
  const char *s = SDL_AndroidGetExternalStoragePath();
  SDL_Log("External storage: [%s]", s);
  if (s) {
    setenv("HOME", s, 1); // HACK
  } else {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Holy shit", "External storage not available", NULL);
    return 1;
  }
#endif

  host_gdb_mode = false;
  for (int f = 1; f < argc; ++f) {
    if (strcmp(argv[f], "-gdb") == 0) {
      host_gdb_mode = true;
      for (int c = f+1; c < argc; ++c) argv[c-1] = argv[c];
      --argc;
      --f;
    } else if (strcmp(argv[f], "-conlog") == 0) {
      GLogTTYLog = true;
      GConTTYLogForced = true;
      for (int c = f+1; c < argc; ++c) argv[c-1] = argv[c];
      --argc;
      --f;
    } else if (strcmp(argv[f], "-nonetlog") == 0) {
      GConTTYLogForced = false;
      for (int c = f+1; c < argc; ++c) argv[c-1] = argv[c];
      --argc;
      --f;
    } else if (strcmp(argv[f], "-nottylog") == 0) {
      GLogTTYLog = false;
      GConTTYLogForced = false;
      for (int c = f+1; c < argc; ++c) argv[c-1] = argv[c];
      --argc;
      --f;
    }
  }

  Sys_PinThread(); // pin main thread to one CPU

  if (host_gdb_mode) {
    mainloop(argc, argv);
  } else {
    try {
      mainloop(argc, argv);
    } catch (...) {
      //fprintf(stderr, "\nExiting due to external exception\n");
      GCon->Logf("\nExiting due to external exception");
      Host_Shutdown();
      throw;
    }
  }
  Z_ShuttingDown();

#ifdef _WIN32
  //ExitProcess(0);
  //TerminateProcess(GetCurrentProcess(), 0);
#elif defined(__SWITCH__)
  socketExit();
#endif
  return 0;
}
