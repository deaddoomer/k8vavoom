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
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
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


bool Sys_FileExists (const VStr &filename);
bool Sys_DirExists (const VStr &path);
int Sys_FileTime (const VStr &path); // returns -1 if not present, or time in unix epoch
int Sys_CurrFileTime (); // return current time in unix epoch
bool Sys_CreateDirectory (const VStr &path);

void Sys_FileDelete (const VStr &filename);

// can return `nullptr` for invalid path
void *Sys_OpenDir (const VStr &path, bool wantDirs=false); // nullptr: error
// returns empty string on end-of-dir; returns names w/o path
// if `wantDirs` is true, dir names are ends with "/"; never returns "." and ".."
VStr Sys_ReadDir (void *adir);
void Sys_CloseDir (void *adir);

double Sys_Time_CPU (); // this tries to return CPU time used by the process
double Sys_Time ();
void Sys_Yield ();

int Sys_GetCPUCount ();

#define Sys_EmptyTID  ((vuint32)0xffffffffu)

// get unique ID of the current thread
// let's hope that 32 bits are enough for thread ids on all OSes, lol
vuint32 Sys_GetCurrentTID ();

// returns system user name suitable for using as player name
// never returns empty string
VStr Sys_GetUserName ();
