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

enum
{
	CVAR_Archive	= 1,	//	Set to cause it to be saved to config.cfg
	CVAR_UserInfo	= 2,	//	Added to userinfo  when changed
	CVAR_ServerInfo	= 4,	//	Added to serverinfo when changed
	CVAR_Init		= 8,	//	Don't allow change from console at all,
							// but can be set from the command line
	CVAR_Latch		= 16,	//	Save changes until server restart
	CVAR_Rom		= 32,	//	Display only, cannot be set by user at all
	CVAR_Cheat		= 64,	//	Can not be changed if cheats are disabled
	CVAR_Modified	= 128,	//	Set each time the cvar is changed
};

//
//	Console variable
//
class VCvar
{
protected:
	const char*	Name;				//	Variable's name
	const char*	DefaultString;		//	Default value
	VStr		StringValue;		//	Current value
	int			Flags;				//	CVAR_ flags
	int			IntValue;			//	atoi(string)
	float		FloatValue;			//	atof(string)
	VCvar*		Next;				//	For linked list if variables
	VStr		LatchedString;		//	For CVAR_LATCH variables

public:
	VCvar(const char* AName, const char* ADefault, int AFlags = 0);
	void Register();
	void Set(int value);
	void Set(float value);
	void Set(const VStr& value);
	bool IsModified();

	static void Init();

	static int GetInt(const char* var_name);
	static float GetFloat(const char* var_name);
	static const char* GetCharp(const char* var_name);
	static VStr GetString(const char* var_name);

	static void Set(const char* var_name, int value);
	static void Set(const char* var_name, float value);
	static void Set(const char* var_name, const VStr& value);

	static bool Command(const TArray<VStr>& Args);
	static void WriteVariables(FILE* f);

	static void Unlatch();
	static void SetCheating(bool);

	friend class TCmdCvarList;

private:
	void DoSet(const VStr& value);

	static VCvar*	Variables;
	static bool		Initialised;
	static bool		Cheating;

	static VCvar* FindVariable(const char* name);
};

//	Cvar, that can be used as int variable
class VCvarI : public VCvar
{
public:
	VCvarI(const char* AName, const char* ADefault, int AFlags = 0)
		: VCvar(AName, ADefault, AFlags)
	{
	}

	operator int() const
	{
		return IntValue;
	}

	VCvarI &operator = (int AValue)
	{
		Set(AValue);
		return *this;
	}
};

//	Cvar, that can be used as float variable
class VCvarF : public VCvar
{
public:
	VCvarF(const char* AName, const char* ADefault, int AFlags = 0)
		: VCvar(AName, ADefault, AFlags)
	{
	}

	operator float() const
	{
		return FloatValue;
	}

	VCvarF &operator = (float AValue)
	{
		Set(AValue);
		return *this;
	}
};

//	Cvar, that can be used as char* variable
class VCvarS : public VCvar
{
public:
	VCvarS(const char* AName, const char* ADefault, int AFlags = 0)
		: VCvar(AName, ADefault, AFlags)
	{
	}

	operator const char*() const
	{
		return *StringValue;
	}

	VCvarS &operator = (const char* AValue)
	{
		Set(AValue);
		return *this;
	}
};

//**************************************************************************
//
//	$Log$
//	Revision 1.11  2006/04/05 17:23:37  dj_jl
//	More dynamic string usage in console command class.
//	Added class for handling command line arguments.
//
//	Revision 1.10  2006/03/29 22:32:27  dj_jl
//	Changed console variables and command buffer to use dynamic strings.
//	
//	Revision 1.9  2002/07/23 16:29:55  dj_jl
//	Replaced console streams with output device class.
//	
//	Revision 1.8  2002/01/07 12:16:41  dj_jl
//	Changed copyright year
//	
//	Revision 1.7  2001/12/18 19:05:03  dj_jl
//	Made VCvar a pure C++ class
//	
//	Revision 1.6  2001/10/04 17:18:23  dj_jl
//	Implemented the rest of cvar flags
//	
//	Revision 1.5  2001/09/05 12:21:42  dj_jl
//	Release changes
//	
//	Revision 1.4  2001/08/29 17:50:42  dj_jl
//	Implemented CVAR_LATCH
//	
//	Revision 1.3  2001/07/31 17:16:30  dj_jl
//	Just moved Log to the end of file
//	
//	Revision 1.2  2001/07/27 14:27:54  dj_jl
//	Update with Id-s and Log-s, some fixes
//
//**************************************************************************
