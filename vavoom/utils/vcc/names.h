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
//**	Copyright (C) 1999-2002 J�nis Legzdi��
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
//**
//**	Header file registering global hardcoded Vavoom names.
//**
//**************************************************************************

// Macros ------------------------------------------------------------------

// Define a message as an enumeration.
#ifndef REGISTER_NAME
	#define REGISTER_NAME(name)	NAME_##name,
	#define REGISTERING_ENUM
	enum EName {
#endif

// Hardcoded names which are not messages ----------------------------------

// Special zero value, meaning no name.
REGISTER_NAME(None)

REGISTER_NAME(state_t)
REGISTER_NAME(mobjinfo_t)
REGISTER_NAME(Object)

// Closing -----------------------------------------------------------------

#ifdef REGISTERING_ENUM
		NUM_HARDCODED_NAMES
	};
	#undef REGISTER_NAME
	#undef REGISTERING_ENUM
#endif

//**************************************************************************
//
//	$Log$
//	Revision 1.4  2005/11/29 19:31:43  dj_jl
//	Class and struct classes, removed namespaces, beautification.
//
//	Revision 1.3  2005/11/24 20:41:07  dj_jl
//	Cleaned up a bit.
//	
//	Revision 1.2  2002/01/25 18:05:58  dj_jl
//	Better string hash function
//	
//	Revision 1.1  2002/01/11 08:17:31  dj_jl
//	Added name subsystem, removed support for unsigned ints
//	
//**************************************************************************
