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

#ifndef _COMMON_H
#define _COMMON_H

#ifndef __GNUC__
#define __attribute__(whatever)
#endif

#if defined __unix__ && !defined DJGPP
#undef stricmp	//	Allegro defines them
#undef strnicmp
#define stricmp		strcasecmp
#define strnicmp	strncasecmp
#endif

//==========================================================================
//
//	Basic types
//
//==========================================================================

#define MINCHAR		((char)0x80)
#define MINSHORT    ((short)0x8000)
#define MININT      ((int)0x80000000L)
#define MINLONG     ((long)0x80000000L)

#define MAXCHAR		((char)0x7f)
#define MAXSHORT    ((short)0x7fff)
#define MAXINT      ((int)0x7fffffff)
#define MAXLONG     ((long)0x7fffffff)

typedef int					boolean;	//	Must be 4 bytes long
typedef unsigned char 		byte;
typedef unsigned short	 	word;
typedef unsigned long	 	dword;
#ifndef _WIN32
typedef long long 			__int64;
#endif

#endif
