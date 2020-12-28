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
//**  Copyright (C) 2018-2020 Ketmar Dark
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

void Host_Init ();
void Host_Shutdown ();
void Host_Frame ();
void __attribute__((noreturn, format(printf, 1, 2))) __declspec(noreturn) Host_EndGame (const char *message, ...);
void __attribute__((noreturn, format(printf, 1, 2))) __declspec(noreturn) Host_Error (const char *error, ...);
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
void Host_CollectGarbage (bool forced=false);

extern VCvarB developer;

extern bool host_initialised;
extern bool host_request_exit;


// no more than 250 FPS
#define max_fps_cap_double  (0.004)
#define max_fps_cap_float   (0.004f)

extern int host_frametics;
extern double host_frametime;
extern double host_framefrac;
extern double host_time; // used in UI and network heartbits; accumulates frame times
extern double systime; // current `Sys_Time()`; used for consistency, updated in `FilterTime()`
extern int host_framecount;
extern int cli_ShowEndText;
