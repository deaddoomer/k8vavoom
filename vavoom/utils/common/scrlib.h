//**************************************************************************
//**
//**	##   ##    ##    ##   ##   ####     ####   ###     ###
//**	##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**	 ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**	 ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**	  ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**	   #    ##    ##    #      ####     ####   ##       ##
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

#ifndef _SCRIPTS_H
#define _SCRIPTS_H

// HEADER FILES ------------------------------------------------------------

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void SC_Open(const char *name);
void SC_Close(void);
bool SC_GetString(void);
void SC_MustGetString(void);
void SC_MustGetStringName(const char *name);
bool SC_GetNumber(void);
void SC_MustGetNumber(void);
bool SC_GetFloat(void);
void SC_MustGetFloat(void);
void SC_UnGet(void);
bool SC_Compare(const char *text);
int SC_MatchString(const char **strings);
int SC_MustMatchString(const char **strings);
void SC_ScriptError(const char *message);

// PUBLIC DATA DECLARATIONS ------------------------------------------------

extern char 	*sc_String;
extern int 		sc_Number;
extern double	sc_Float;
extern int 		sc_Line;
extern bool 	sc_End;
extern bool 	sc_Crossed;

#endif
