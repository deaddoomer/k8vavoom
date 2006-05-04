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

class VRootWindow : public VModalWindow
{
#ifdef ZONE_DEBUG_NEW
#undef new
#endif
	DECLARE_CLASS(VRootWindow, VModalWindow, 0)
#ifdef ZONE_DEBUG_NEW
#define new ZONE_DEBUG_NEW
#endif

	VRootWindow(void);
	void Init(void);
	void Init(VWindow *) { Sys_Error("Root canot have a parent"); }

	void PaintWindows(void);
	void TickWindows(float DeltaTime);

	static void StaticInit(void);
};

extern VRootWindow *GRoot;
