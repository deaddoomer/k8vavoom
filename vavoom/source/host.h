//**************************************************************************
//**
//**	##   ##    ##    ##   ##   ####     ####   ###     ###
//**	##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**	 ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**	 ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**	  ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**	   #    ##    ##    #      ####     ####   ##       ##
//**
//**	$Id$
//**
//**	Copyright (C) 1999-2001 J�nis Legzdi��
//**
//**	This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**	This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void Host_Init(void);
void Host_Shutdown(void);
void Host_Frame(void);
void Host_EndGame(const char *message, ...)
	__attribute__((format(printf, 1, 2))) __attribute__((noreturn));
void Host_Error(const char *error, ...)
	__attribute__((format(printf, 1, 2))) __attribute__((noreturn));

// PUBLIC DATA DECLARATIONS ------------------------------------------------

extern TCvarI		developer;

extern Game_t		Game;
extern boolean		ExtendedWAD;// true if main WAD is the extended version

extern boolean 		DevMaps;	// true = Map development mode
extern char*		DevMapsDir;	// development maps directory

extern boolean		host_initialized;

extern int			host_frametics;
extern double		host_frametime;
extern double		host_time;
extern double		realtime;
extern int			host_framecount;

//**************************************************************************
//
//	$Log$
//	Revision 1.3  2001/07/31 17:16:30  dj_jl
//	Just moved Log to the end of file
//
//	Revision 1.2  2001/07/27 14:27:54  dj_jl
//	Update with Id-s and Log-s, some fixes
//
//**************************************************************************
