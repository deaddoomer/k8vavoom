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
//**
//**	Defines shared by refresh and drawer
//**
//**************************************************************************

#ifndef _R_SHARED_H
#define _R_SHARED_H

// HEADER FILES ------------------------------------------------------------

#include "fmd2defs.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

class TClipPlane : public TPlane
{
public:
	TClipPlane		*next;

	int				clipflag;
	TVec			enter;
	TVec			exit;
	int				entered;
	int				exited;
};

struct texinfo_t
{
	TVec			saxis;
	float			soffs;
	TVec			taxis;
	float			toffs;
	VTexture*		Tex;
	//	1.1 for solid surfaces
	// Alpha for masked surfaces
	float			Alpha;
	bool			Additive;
};

struct surface_t
{
	surface_t*		next;
	surface_t*		DrawNext;
	texinfo_t*		texinfo;
	TPlane*			plane;
	vuint32			Light;		//	Light level and colour.
	vuint32			Fade;
	vuint8*			lightmap;
	rgb_t*			lightmap_rgb;
	int				dlightframe;
	int				dlightbits;
	int				count;
	short			texturemins[2];
	short			extents[2];
	surfcache_t*	cachespots[4];
	TVec			verts[1];
};

//
//	Camera texture.
//
class VCameraTexture : public VTexture
{
public:
	vuint8*		Pixels;
	bool		bNeedsUpdate;
	bool		bUpdated;

	VCameraTexture(VName, int, int);
	~VCameraTexture();
	bool CheckModified();
	vuint8* GetPixels();
	void Unload();
	void CopyImage();
	VTexture* GetHighResolutionTexture();
};

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void R_DrawViewBorder();

void *Mod_Extradata(VModel* mod);	// handles caching

// PUBLIC DATA DECLARATIONS ------------------------------------------------

//
// POV related.
//
extern "C" {
extern TVec				vieworg;
extern TVec				viewforward;
extern TVec				viewright;
extern TVec				viewup;
} // extern "C"
extern TAVec			viewangles;

extern VCvarI			r_fog;
extern VCvarF			r_fog_r;
extern VCvarF			r_fog_g;
extern VCvarF			r_fog_b;
extern VCvarF			r_fog_start;
extern VCvarF			r_fog_end;
extern VCvarF			r_fog_density;

extern int				extralight;
extern int				fixedlight;

extern "C" {
extern TClipPlane		view_clipplanes[4];
}

extern int				r_dlightframecount;
extern bool				r_light_add;
extern vuint32			blocklights[18 * 18];
extern vuint32			blocklightsr[18 * 18];
extern vuint32			blocklightsg[18 * 18];
extern vuint32			blocklightsb[18 * 18];
extern vuint32			blockaddlightsr[18 * 18];
extern vuint32			blockaddlightsg[18 * 18];
extern vuint32			blockaddlightsb[18 * 18];

extern rgba_t			r_palette[256];
extern vuint8			r_black_colour;

extern "C" {
extern vuint8			r_rgbtable[32 * 32 * 32 + 4];
};

extern int				usegamma;
extern vuint8			gammatable[5][256];

extern float			PixelAspect;

#endif
