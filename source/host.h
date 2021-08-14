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
#ifndef VAVOOM_HOST_HEADER
#define VAVOOM_HOST_HEADER


void Host_Init ();
void Host_Shutdown ();
void Host_Frame ();
const char *Host_GetCoreDump ();
bool Host_StartTitleMap ();
VStr Host_GetConfigDir ();

// called if CLI arguments contains some map selections command
void Host_CLIMapStartFound ();
bool Host_IsCLIMapStartFound ();

bool Host_IsDangerousTimeout ();

// call this after saving/loading/map loading, so we won't unnecessarily skip frames
void Host_ResetSkipFrames ();

// this does GC rougly twice per second (unless forced)
void Host_CollectGarbage (bool forced=false, bool resetUniqueId=false);

void Host_InitStreamCallbacks ();

extern bool host_initialised;
extern bool host_request_exit;
extern bool host_gdb_mode;


// no more than 250 FPS
/*#define max_fps_cap_double  (1.0/250.0)*/
#define max_fps_cap_float   ((float)(1.0f/250.0f))

extern float host_frametime; // time delta for the current frame
extern double host_systime; // current `Sys_Time()`; used for consistency, updated in `FilterTime()`
extern vuint64 host_systime64_msec; // monotonic time, in milliseconds
extern int host_framecount; // used in demo playback

extern int cli_ShowEndText;


#if defined(__i386__) || defined(__x86_64__)
__inline__ __attribute__((unused,gnu_inline,always_inline)) static void host_debug_break_internal (void) {
  __asm__ volatile("int $0x03");
}
__inline__ __attribute__((unused,gnu_inline,always_inline)) static void host_debug_break (void) { if (host_gdb_mode) host_debug_break_internal(); }
#else
__inline__ __attribute__((unused,gnu_inline,always_inline)) static void host_debug_break (void) {}
#endif


#endif
