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

#include "gl_local.h"
extern "C" {
#include <GL/amesa.h>
}

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static AMesaVisual		RenderVisual;
static AMesaBuffer		RenderBuffer;
static AMesaContext		RenderContext = NULL;

// CODE --------------------------------------------------------------------

//==========================================================================
//
//	VOpenGLDrawer::Init
//
// 	Determine the hardware configuration
//
//==========================================================================

void VOpenGLDrawer::Init()
{
}

//==========================================================================
//
// 	VOpenGLDrawer::SetResolution
//
// 	Set up the video mode
//
//==========================================================================

bool VOpenGLDrawer::SetResolution(int InWidth, int InHeight, int InBPP)
{
	guard(VOpenGLDrawer::SetResolution);
	int Width = InWidth;
	int Height = InHeight;
	int BPP = InBPP;
	if (!Width || !Height)
	{
		//	Set defaults
		Width = 640;
		Height = 480;
		BPP = 16;
	}

	if (BPP == 15) BPP = 16;

	if (BPP < 16)
	{
		//	True-color only
		return false;
	}

	Shutdown();

	RenderVisual = AMesaCreateVisual(true, BPP, 16, 0, 0);
	if (!RenderVisual)
	{
		return false;
	}
	RenderBuffer = AMesaCreateBuffer(RenderVisual, Width, Height);
	if (!RenderBuffer)
	{
		return false;
	}
	RenderContext = AMesaCreateContext(RenderVisual, NULL);
	if (!RenderContext)
	{
		return false;
	}
	AMesaMakeCurrent(RenderContext, RenderBuffer);

	ScreenWidth = Width;
	ScreenHeight = Height;
	ScreenBPP = BPP;

	return true;
	unguard;
}

//==========================================================================
//
//	VOpenGLDrawer::GetExtFuncPtr
//
//==========================================================================

void *VOpenGLDrawer::GetExtFuncPtr(const char*)
{
	return NULL;
}

//==========================================================================
//
//	VOpenGLDrawer::Update
//
// 	Blit to the screen / Flip surfaces
//
//==========================================================================

void VOpenGLDrawer::Update(void)
{
	guard(VOpenGLDrawer::Update);
	glFlush();
	AMesaSwapBuffers(RenderBuffer);
	unguard;
}

//==========================================================================
//
// 	VOpenGLDrawer::Shutdown
//
//	Close the graphics
//
//==========================================================================

void VOpenGLDrawer::Shutdown(void)
{
	guard(VOpenGLDrawer::Shutdown);
	DeleteTextures();
	if (RenderContext)
	{
		AMesaDestroyContext(RenderContext);
		RenderContext = NULL;
	}
	if (RenderBuffer)
	{
		AMesaDestroyBuffer(RenderBuffer);
		RenderBuffer = NULL;
	}
	if (RenderVisual)
	{
		AMesaDestroyVisual(RenderVisual);
		RenderVisual = NULL;
	}
	unguard;
}

//**************************************************************************
//
//	$Log$
//	Revision 1.8  2005/04/28 07:16:15  dj_jl
//	Fixed some warnings, other minor fixes.
//
//	Revision 1.7  2002/07/13 07:38:00  dj_jl
//	Added drawers to the object tree.
//	
//	Revision 1.6  2002/01/11 08:12:01  dj_jl
//	Added guard macros
//	
//	Revision 1.5  2002/01/07 12:16:42  dj_jl
//	Changed copyright year
//	
//	Revision 1.4  2001/08/04 17:32:04  dj_jl
//	Added support for multitexture extensions
//	
//	Revision 1.3  2001/07/31 17:16:30  dj_jl
//	Just moved Log to the end of file
//	
//	Revision 1.2  2001/07/27 14:27:54  dj_jl
//	Update with Id-s and Log-s, some fixes
//
//**************************************************************************
