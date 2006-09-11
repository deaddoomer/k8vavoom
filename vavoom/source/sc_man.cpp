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
//**	Copyright (C) 1999-2006 Jānis Legzdiņš
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

#include "gamedefs.h"

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
//	VScriptParser::VScriptParser
//
//==========================================================================

VScriptParser::VScriptParser(const VStr& name, VStream* Strm)
: Line(1)
, End(false)
, Crossed(false)
, ScriptName(name)
, AlreadyGot(false)
{
	guard(VScriptParser::VScriptParser);
	ScriptSize = Strm->TotalSize();
	ScriptBuffer = new char[ScriptSize + 1];
	Strm->Serialise(ScriptBuffer, ScriptSize);
	ScriptBuffer[ScriptSize] = 0;
	delete Strm;
	ScriptPtr = ScriptBuffer;
	ScriptEndPtr = ScriptPtr + ScriptSize;

	//	Skip garbage some editors add in the begining of UTF-8 files.
	if ((vuint8)ScriptPtr[0] == 0xef && (vuint8)ScriptPtr[1] == 0xbb &&
		(vuint8)ScriptPtr[2] == 0xbf)
	{
		ScriptPtr += 3;
	}
	unguard;
}

//==========================================================================
//
//	VScriptParser::~VScriptParser
//
//==========================================================================

VScriptParser::~VScriptParser()
{
	guard(VScriptParser::~VScriptParser);
	delete[] ScriptBuffer;
	unguard;
}

//==========================================================================
//
//	VScriptParser::AtEnd
//
//==========================================================================

bool VScriptParser::AtEnd()
{
	guard(VScriptParser::AtEnd);
	if (GetString())
	{
		UnGet();
		return false;
	}
	return true;
	unguard;
}

//==========================================================================
//
//	VScriptParser::GetString
//
//==========================================================================

bool VScriptParser::GetString()
{
	guard(VScriptParser::GetString);
	//	Check if we already have a token available.
	if (AlreadyGot)
	{
		AlreadyGot = false;
		return true;
	}

	//	Check for end of script.
	if (ScriptPtr >= ScriptEndPtr)
	{
		End = true;
		return false;
	}

	Crossed = false;
	bool foundToken = false;
	while (foundToken == false)
	{
		//	Skip whitespace.
		while ((vuint8)*ScriptPtr <= 32)
		{
			if (ScriptPtr >= ScriptEndPtr)
			{
				End = true;
				return false;
			}
			//	Check for new-line character.
			if (*ScriptPtr++ == '\n')
			{
				Line++;
				Crossed = true;
			}
		}

		//	Check for end of script.
		if (ScriptPtr >= ScriptEndPtr)
		{
			End = true;
			return false;
		}

		//	Check for coments
		if (*ScriptPtr == ';' || (ScriptPtr[0] == '/' && ScriptPtr[1] == '/'))
		{
			// Skip comment
			while (*ScriptPtr++ != '\n')
			{
				if (ScriptPtr >= ScriptEndPtr)
				{
					End = true;
					return false;
				}
			}
			Line++;
			Crossed = true;
		}
		else
		{
			// Found a token
			foundToken = true;
		}
	}

	String.Clean();
	if (*ScriptPtr == '\"')
	{
		//	Quoted string
		ScriptPtr++;
		while (*ScriptPtr != '\"')
		{
			if (*ScriptPtr == '\n')
			{
				Line++;
			}
			if (*ScriptPtr == '\\')
			{
				ScriptPtr++;
				if (ScriptPtr == ScriptEndPtr)
				{
					break;
				}
				switch (*ScriptPtr)
				{
				case '\"':
					String += '\"';
					break;
				default:
					String += *ScriptPtr;
				}
				ScriptPtr++;
			}
			else
			{
				String += *ScriptPtr++;
			}
			if (ScriptPtr == ScriptEndPtr)
			{
				break;
			}
		}
		ScriptPtr++;
	}
	else
	{
		//	Normal string
		while ((vuint8)*ScriptPtr > 32 && !(*ScriptPtr == ';' ||
			(ScriptPtr[0] == '/' && ScriptPtr[1] == '/')))
		{
			String += *ScriptPtr++;
			if (ScriptPtr == ScriptEndPtr)
			{
				break;
			}
		}
	}
	return true;
	unguard;
}

//==========================================================================
//
//	VScriptParser::ExpectString
//
//==========================================================================

void VScriptParser::ExpectString()
{
	guard(VScriptParser::ExpectString);
	if (!GetString())
	{
		Error("Missing string.");
	}
	unguard;
}

//==========================================================================
//
//	VScriptParser::ExpectName8
//
//==========================================================================

void VScriptParser::ExpectName8()
{
	guard(VScriptParser::ExpectName8);
	ExpectString();
	if (String.Length() > 8)
	{
		Error("Name is too long");
	}
	Name8 = VName(*String, VName::AddLower8);
	unguard;
}

//==========================================================================
//
//	VScriptParser::Check
//
//==========================================================================

bool VScriptParser::Check(const char* str)
{
	guard(VScriptParser::Check);
	if (GetString())
	{
		if (!String.ICmp(str))
		{
			return true;
		}
		UnGet();
	}
	return false;
	unguard;
}

//==========================================================================
//
//	VScriptParser::Expect
//
//==========================================================================

void VScriptParser::Expect(const char* name)
{
	guard(VScriptParser::Expect);
	ExpectString();
	if (String.ICmp(name))
	{
		Error(va("Bad syntax, %s expected", name));
	}
	unguard;
}

//==========================================================================
//
//	VScriptParser::CheckNumber
//
//==========================================================================

bool VScriptParser::CheckNumber()
{
	guard(VScriptParser::CheckNumber);
	if (GetString())
	{
		char* stopper;
		Number = strtol(*String, &stopper, 0);
		if (*stopper == 0)
		{
			return true;
		}
		UnGet();
	}
	return false;
	unguard;
}

//==========================================================================
//
//	VScriptParser::ExpectNumber
//
//==========================================================================

void VScriptParser::ExpectNumber()
{
	guard(VScriptParser::ExpectNumber);
	if (GetString())
	{
		char* stopper;
		Number = strtol(*String, &stopper, 0);
		if (*stopper != 0)
		{
			Error(va("Bad numeric constant \"%s\".\n", *String));
		}
	}
	else
	{
		Error("Missing integer.");
	}
	unguard;
}

//==========================================================================
//
//	VScriptParser::CheckFloat
//
//==========================================================================

bool VScriptParser::CheckFloat()
{
	guard(VScriptParser::CheckFloat);
	if (GetString())
	{
		char* stopper;
		Float = strtod(*String, &stopper);
		if (*stopper != 0)
		{
			return true;
		}
		UnGet();
	}
	return false;
	unguard;
}

//==========================================================================
//
//	VScriptParser::ExpectFloat
//
//==========================================================================

void VScriptParser::ExpectFloat()
{
	guard(VScriptParser::ExpectFloat);
	if (GetString())
	{
		char* stopper;
		Float = strtod(*String, &stopper);
		if (*stopper != 0)
		{
			Error(va("Bad floating point constant \"%s\"", *String));
		}
	}
	else
	{
		Error("Missing float.");
	}
	unguard;
}

//==========================================================================
//
//	VScriptParser::UnGet
//
//	Assumes there is a valid string in sc_String.
//
//==========================================================================

void VScriptParser::UnGet()
{
	guard(VScriptParser::UnGet);
	AlreadyGot = true;
	unguard;
}

//==========================================================================
//
//	VScriptParser::Error
//
//==========================================================================

void VScriptParser::Error(const char* message)
{
	guard(VScriptParser::Error)
	const char* Msg = message ? message : "Bad syntax.";
	Sys_Error("Script error, \"%s\" line %d: %s", *ScriptName, Line, Msg);
	unguard;
}
