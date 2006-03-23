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

// HEADER FILES ------------------------------------------------------------

#include "vcc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
//	TModifiers::Parse
//
//	Parse supported modifiers.
//
//==========================================================================

int TModifiers::Parse()
{
	int Modifiers = 0;
	bool done = false;
	do
	{
		if (TK_Check(KW_NATIVE))
		{
			Modifiers |= Native;
		}
		else if (TK_Check(KW_STATIC))
		{
			Modifiers |= Static;
		}
		else if (TK_Check(KW_ABSTRACT))
		{
			Modifiers |= Abstract;
		}
		else if (TK_Check(KW_PRIVATE))
		{
			Modifiers |= Private;
		}
		else if (TK_Check(KW_READONLY))
		{
			Modifiers |= ReadOnly;
		}
		else if (TK_Check(KW_TRANSIENT))
		{
			Modifiers |= Transient;
		}
		else if (TK_Check(KW_FINAL))
		{
			Modifiers |= Final;
		}
		else
		{
			done = true;
		}
	} while (!done);
	return Modifiers;
}

//==========================================================================
//
//	TModifiers::Name
//
//	Return string representation of a modifier.
//
//==========================================================================

const char* TModifiers::Name(int Modifier)
{
	switch (Modifier)
	{
	case Native:	return "native";
	case Static:	return "static";
	case Abstract:	return "abstract";
	case Private:	return "private";
	case ReadOnly:	return "readonly";
	case Transient:	return "transient";
	case Final:		return "final";
	}
	return "";
}

//==========================================================================
//
//	TModifiers::Check
//
//	Verify that modifiers are valid in current context.
//
//==========================================================================

int TModifiers::Check(int Modifers, int Allowed)
{
	int Bad = Modifers & ~Allowed;
	if (Bad)
	{
		for (int i = 0; i < 32; i++)
			if (Bad & (1 << i))
				ParseError("%s modifier is not allowed", Name(1 << i));
		return Modifers & Allowed;
	}
	return Modifers;
}

//==========================================================================
//
//	TModifiers::MethodAttr
//
//	Convert modifiers to method attributes.
//
//==========================================================================

int TModifiers::MethodAttr(int Modifiers)
{
	int Attributes = 0;
	if (Modifiers & Native)
		Attributes |= FUNC_Native;
	if (Modifiers & Static)
		Attributes |= FUNC_Static;
	if (Modifiers & Final)
		Attributes |= FUNC_Final;
	return Attributes;
}

//==========================================================================
//
//	TModifiers::ClassAttr
//
//	Convert modifiers to class attributes.
//
//==========================================================================

int TModifiers::ClassAttr(int Modifiers)
{
	int Attributes = 0;
	return Attributes;
}

//==========================================================================
//
//	TModifiers::FieldAttr
//
//	Convert modifiers to field attributes.
//
//==========================================================================

int TModifiers::FieldAttr(int Modifiers)
{
	int Attributes = 0;
	if (Modifiers & Native)
		Attributes |= FIELD_Native;
	if (Modifiers & Transient)
		Attributes |= FIELD_Transient;
	if (Modifiers & Private)
		Attributes |= FIELD_Private;
	if (Modifiers & ReadOnly)
		Attributes |= FIELD_ReadOnly;
	return Attributes;
}

//**************************************************************************
//
//	$Log$
//	Revision 1.4  2006/03/23 23:11:27  dj_jl
//	Added support for final methods.
//
//	Revision 1.3  2006/03/13 21:24:21  dj_jl
//	Added support for read-only, private and transient fields.
//	
//	Revision 1.2  2006/02/25 17:07:57  dj_jl
//	Linked list of fields, export all type info.
//	
//	Revision 1.1  2006/02/20 19:34:32  dj_jl
//	Created modifiers class.
//	
//**************************************************************************
