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

#include "d_local.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern vuint8	gammatable[5][256];

// PUBLIC DATA DEFINITIONS -------------------------------------------------

//
//	Colourmaps
//
vuint8*			colourmaps;	// Standard colourmap
vuint8			d_fadetable[32 * 256];	// Current colourmap
vuint16			d_fadetable16[32 * 256];
vuint16			d_fadetable16r[32 * 256];
vuint16			d_fadetable16g[32 * 256];
vuint16			d_fadetable16b[32 * 256];
vuint32			d_fadetable32[32 * 256];
vuint8			d_fadetable32r[32 * 256];
vuint8			d_fadetable32g[32 * 256];
vuint8			d_fadetable32b[32 * 256];

//
//	Translucency tables
//
vuint8*			tinttables[5];
vuint8*			AdditiveTransTables[10];
vuint16			scaletable[32][256];

vuint8*			consbgmap = NULL;

bool			ForcePaletteUpdate;
vuint32			CurrentFade;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static rgb_t	CurrentPal[256];

// CODE --------------------------------------------------------------------

//==========================================================================
//
//	CalcRGBTable8
//
//==========================================================================

static void CalcRGBTable8(vuint32 Fade)
{
	guard(CalcRGBTable8);
	int i = 0;
	for (int tn = 0; tn < 32; tn++)
	{
		float frac = 1.0 - tn / 32.0;
		int fog_r = ((Fade >> 16) & 255) * tn / 32;
		int fog_g = ((Fade >> 8) & 255) * tn / 32;
		int fog_b = (Fade & 255) * tn / 32;
		for (int ci = 0; ci < 256; ci++, i++)
		{
			if (!(i & 0xff))
			{
				d_fadetable16r[i] = 0x8000;
				d_fadetable16g[i] = 0x8000;
				d_fadetable16b[i] = 0x8000;
				d_fadetable[i] = 0;
				continue;
			}
			int r = (int)(r_palette[ci].r * frac + 0.5) + fog_r;
			int g = (int)(r_palette[ci].g * frac + 0.5) + fog_g;
			int b = (int)(r_palette[ci].b * frac + 0.5) + fog_b;
			if (r > 255)
				r = 255;
			if (g > 255)
				g = 255;
			if (b > 255)
				b = 255;
			d_fadetable16r[i] = (r << 7) & 0x7c00;
			d_fadetable16g[i] = (g << 2) & 0x03e0;
			d_fadetable16b[i] = (b >> 3) & 0x001f;
			if (Fade)
			{
				d_fadetable[i] = MakeCol8(r, g, b);
			}
		}
	}
	unguard;
}

//==========================================================================
//
//	CalcCol16Table
//
//==========================================================================

static void CalcCol16Table()
{
	guard(CalcCol16Table);
	byte *gt = gammatable[usegamma];
	for (int i = 0; i < 256; i++)
	{
		pal8_to16[i] = MakeCol(gt[r_palette[i].r], gt[r_palette[i].g],
			gt[r_palette[i].b]);
	}
	unguard;
}

//==========================================================================
//
//	CalcFadetable16
//
//==========================================================================

static void CalcFadetable16(rgb_t *pal, vuint32 Fade)
{
	guard(CalcFadetable16);
	byte *gt = gammatable[usegamma];
	int i = 0;
	for (int tn = 0; tn < 32; tn++)
	{
		int colm = 32 - tn;
		int fog_r = ((Fade >> 16) & 255) * tn / 32;
		int fog_g = ((Fade >> 8) & 255) * tn / 32;
		int fog_b = (Fade & 255) * tn / 32;
		for (int ci = 0; ci < 256; ci++, i++)
		{
			if (!(i & 0xff))
			{
				d_fadetable16[i] = 0;
				d_fadetable16r[i] = 0;
				d_fadetable16g[i] = 0;
				d_fadetable16b[i] = 0;
				continue;
			}
			int r = gt[pal[ci].r] * colm / 32 + fog_r;
			int g = gt[pal[ci].g] * colm / 32 + fog_g;
			int b = gt[pal[ci].b] * colm / 32 + fog_b;
			d_fadetable16[i] = MakeCol(r, g, b);
			d_fadetable16r[i] = MakeCol(r, 0, 0);
			d_fadetable16g[i] = MakeCol(0, g, 0);
			d_fadetable16b[i] = MakeCol(0, 0, b);
			//	For 16 bit we use colour 0 as transparent
			if (!d_fadetable16[i])
			{
				d_fadetable16[i] = 1;
			}
			if (!d_fadetable16b[i])
			{
				d_fadetable16b[i] = MakeCol(0, 0, 1);
			}
		}
	}
	unguard;
}

//==========================================================================
//
//	CalcCol32Table
//
//==========================================================================

static void CalcCol32Table()
{
	guard(CalcCol32Table);
	byte *gt = gammatable[usegamma];
	for (int i = 0; i < 256; i++)
	{
		pal2rgb[i] = MakeCol(gt[r_palette[i].r], gt[r_palette[i].g],
			gt[r_palette[i].b]);
	}
	unguard;
}

//==========================================================================
//
//	CalcFadetable32
//
//==========================================================================

static void CalcFadetable32(rgb_t *pal, vuint32 Fade)
{
	guard(CalcFadetable32);
	byte *gt = gammatable[usegamma];
	int i = 0;
	for (int tn = 0; tn < 32; tn++)
	{
		int colm = 32 - tn;
		int fog_r = ((Fade >> 16) & 255) * tn / 32;
		int fog_g = ((Fade >> 8) & 255) * tn / 32;
		int fog_b = (Fade & 255) * tn / 32;
		for (int ci = 0; ci < 256; ci++, i++)
		{
			if (!(i & 0xff))
			{
				d_fadetable32[i] = 0;
				d_fadetable32r[i] = 0;
				d_fadetable32g[i] = 0;
				d_fadetable32b[i] = 0;
				continue;
			}
			int r = gt[pal[ci].r] * colm / 32 + fog_r;
			int g = gt[pal[ci].g] * colm / 32 + fog_g;
			int b = gt[pal[ci].b] * colm / 32 + fog_b;
			d_fadetable32[i] = MakeCol32(r, g, b);
			d_fadetable32r[i] = r;
			d_fadetable32g[i] = g;
			d_fadetable32b[i] = b;
			//	For 32 bit we use colour 0 as transparent
			if (!d_fadetable32[i])
			{
				d_fadetable32[i] = 1;
			}
			if (!d_fadetable32b[i])
			{
				d_fadetable32b[i] = 1;
			}
		}
	}
	unguard;
}

//==========================================================================
//
//	InitColourmaps
//
//==========================================================================

static void InitColourmaps()
{
	guard(InitColourmaps);
	// Load in the light tables,
	VStream* Strm = W_CreateLumpReaderName(NAME_colormap);
	colourmaps = new vuint8[Strm->TotalSize()];
	Strm->Serialise(colourmaps, Strm->TotalSize());
	delete Strm;
	//	Remap colour 0 to alternate balck colour
	for (int i = 0; i < 32 * 256; i++)
	{
		if (!(i & 0xff))
		{
			colourmaps[i] = 0;
		}
		else if (!colourmaps[i])
		{
			colourmaps[i] = r_black_colour;
		}
	}
	memcpy(d_fadetable, colourmaps, 32 * 256);
	unguard;
}

//==========================================================================
//
//	CreateTranslucencyTable
//
//==========================================================================

static vuint8* CreateTranslucencyTable(int transluc)
{
	guard(CreateTranslucencyTable);
	vuint8 temp[768];
	for (int i = 0; i < 256; i++)
	{
		temp[i * 3]     = r_palette[i].r * transluc / 100;
		temp[i * 3 + 1] = r_palette[i].g * transluc / 100;
		temp[i * 3 + 2] = r_palette[i].b * transluc / 100;
	}
	vuint8* table = new vuint8[0x10000];
	vuint8* p = table;
	for (int i = 0; i < 256; i++)
	{
		int r = r_palette[i].r * (100 - transluc) / 100;
		int g = r_palette[i].g * (100 - transluc) / 100;
		int b = r_palette[i].b * (100 - transluc) / 100;
		vuint8* q = temp;
		for (int j = 0; j < 256; j++)
		{
			*(p++) = MakeCol8(r + q[0], g + q[1], b + q[2]);
			q += 3;
		}
	}
	return table;
	unguard;
}

//==========================================================================
//
//	CreateAdditiveTranslucencyTable
//
//==========================================================================

static vuint8* CreateAdditiveTranslucencyTable(float Alpha)
{
	guard(CreateAdditiveTranslucencyTable);
	vuint8* table = new vuint8[0x10000];
	vuint8* p = table;
	for (int i = 0; i < 256; i++)
	{
		int r = (int)(r_palette[i].r * Alpha);
		int g = (int)(r_palette[i].g * Alpha);
		int b = (int)(r_palette[i].b * Alpha);
		for (int j = 0; j < 256; j++)
		{
			*(p++) = MakeCol8(MIN(r + r_palette[j].r, 255),
				MIN(g + r_palette[j].g, 255), MIN(b + r_palette[j].b, 255));
		}
	}
	return table;
	unguard;
}

//==========================================================================
//
//	InitTranslucencyTables
//
//==========================================================================

static void InitTranslucencyTables()
{
	guard(InitTranslucencyTables);
	for (int i = 0; i < 5; i++)
	{
		tinttables[i] = CreateTranslucencyTable((i + 1) * 10);
	}
	for (int i = 0; i < 10; i++)
	{
		AdditiveTransTables[i] = CreateAdditiveTranslucencyTable(
			float(i + 1) / 10.0);
	}

	for (int t = 0; t < 32; t++)
	{
		for (int i = 0; i < 256; i++)
		{
			scaletable[t][i] = (i << 8) * t / 31;
		}
	}
	unguard;
}

//==========================================================================
//
//	VSoftwareDrawer::InitData
//
//==========================================================================

void VSoftwareDrawer::InitData()
{
	guard(VSoftwareDrawer::InitData);
	InitColourmaps();
	InitTranslucencyTables();
	unguard;
}

//==========================================================================
//
//	VSoftwareDrawer::UpdatePalette
//
//==========================================================================

void VSoftwareDrawer::UpdatePalette()
{
	guard(VSoftwareDrawer::UpdatePalette);
	int		i, j;
	bool	newshifts;
	int		r,g,b;
	int		dstr, dstg, dstb, perc;

	newshifts = ForcePaletteUpdate;
	ForcePaletteUpdate = false;

	for (i = 0; i < NUM_CSHIFTS; i++)
	{
		vuint32 Val = cl ? cl->CShifts[i] : 0;
		if (Val != GClGame->prev_cshifts[i])
		{
			newshifts = true;
			GClGame->prev_cshifts[i] = Val;
		}
	}

	if (!newshifts)
	{
		return;
	}

	rgba_t* basepal = r_palette;
	rgb_t* newpal = CurrentPal;
	
	for (i = 0; i < 256; i++)
	{
		r = basepal->r;
		g = basepal->g;
		b = basepal->b;
		basepal++;
	
		for (j = 0; j < NUM_CSHIFTS; j++)
		{
			vuint32 Val = cl ? cl->CShifts[j] : 0;
			perc = (Val >> 24) & 0xff;
			dstr = (Val >> 16) & 0xff;
			dstg = (Val >> 8) & 0xff;
			dstb = Val & 0xff;
			r += (perc * (dstr - r)) >> 8;
			g += (perc * (dstg - g)) >> 8;
			b += (perc * (dstb - b)) >> 8;
		}
		
		newpal->r = r;
		newpal->g = g;
		newpal->b = b;
		newpal++;
	}

	if (ScreenBPP == 8)
	{
		SetPalette8((vuint8*)CurrentPal);
	}
	else if (PixelBytes == 2)
	{
		CalcCol16Table();
		CalcFadetable16(CurrentPal, CurrentFade);
		FlushCaches(true);
		FlushTextureCaches();
	}
	else
	{
		CalcCol32Table();
		CalcFadetable32(CurrentPal, CurrentFade);
		FlushCaches(true);
		FlushTextureCaches();
	}
	unguard;
}

//==========================================================================
//
//	VSoftwareDrawer::NewMap
//
//==========================================================================

void VSoftwareDrawer::NewMap()
{
	guard(VSoftwareDrawer::NewMap);
	FlushCaches(false);
	unguard;
}

//==========================================================================
//
//	VSoftwareDrawer::SetFade
//
//==========================================================================

void VSoftwareDrawer::SetFade(vuint32 NewFade)
{
	guard(VSoftwareDrawer::SetFade);
	if (CurrentFade == NewFade)
	{
		return;
	}

	if (ScreenBPP == 8)
	{
		if (!(NewFade & 0x00ffffff))
		{
			memcpy(d_fadetable, colourmaps, 32 * 256);
		}
		CalcRGBTable8(NewFade);
	}
	else if (PixelBytes == 2)
	{
		CalcFadetable16(CurrentPal, NewFade);
	}
	else
	{
		CalcFadetable32(CurrentPal, NewFade);
	}
	CurrentFade = NewFade;
	unguard;
}
