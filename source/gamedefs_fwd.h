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
#ifndef VAVOOM_COMMON_TYPES_HEADER
#define VAVOOM_COMMON_TYPES_HEADER


//==========================================================================
//
//  Forward declarations
//
//==========================================================================
class VName;
class VStr;
class VStream;

class VMemberBase;
class VPackage;
class VField;
class VMethod;
class VState;
class VConstant;
class VStruct;
class VClass;
class VNetObjectsMapBase;
class VNetObjectsMap;
struct mobjinfo_t;

class VObject;

class VNTValueIOEx;


class VLevel;
class VThinker;
class VEntity;
class VPlayerReplicationInfo;

class VBasePlayer;
struct VViewState;

struct particle_t;


extern VCvarS game_name;
extern int cli_WAll;
extern VStr flWarningMessage;

extern VCvarB developer;

extern void __attribute__((noreturn, format(printf, 1, 2))) __declspec(noreturn) Host_EndGame (const char *message, ...);
extern void __attribute__((noreturn, format(printf, 1, 2))) __declspec(noreturn) Host_Error (const char *error, ...);


extern void __attribute__((noreturn)) __declspec(noreturn) Sys_Quit (const char *);
extern void Sys_Shutdown ();

extern char *Sys_ConsoleInput ();


#endif
