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
//**	Local header for Direct3D drawer
//**
//**************************************************************************

#ifndef _D3D_LOCAL_H
#define _D3D_LOCAL_H

// HEADER FILES ------------------------------------------------------------

#include "winlocal.h"
#include "gamedefs.h"
#if 1
#define D3D_OVERLOADS
#include <d3d.h>
#else
#include <d3d8.h>
#include <dx7todx8.h>
#endif
#include "cl_local.h"
#include "r_shared.h"

// MACROS ------------------------------------------------------------------

#define MAX_TRANSLATED_SPRITES		256

#define BLOCK_WIDTH					128
#define BLOCK_HEIGHT				128
#define NUM_BLOCK_SURFS				32
#define NUM_CACHE_BLOCKS			(8 * 1024)

// TYPES -------------------------------------------------------------------

struct surfcache_t
{
	int			s;			// position in light surface
	int			t;
	int			width;		// size
	int			height;
	surfcache_t	*bprev;		// line list in block
	surfcache_t	*bnext;
	surfcache_t	*lprev;		// cache list in line
	surfcache_t	*lnext;
	surfcache_t	*chain;		// list of drawable surfaces
	surfcache_t	*addchain;	// list of specular surfaces
	int			blocknum;	// light surface index
	surfcache_t	**owner;
	dword		Light;		// checked for strobe flash
	int			dlight;
	surface_t	*surf;
	dword		lastframe;
};

struct MyD3DVertex
{
	float		x;			// Homogeneous coordinates
	float		y;
	float		z;
	dword		color;		// Vertex color
	float		texs;		// Texture coordinates
	float		text;
	float		lights;		// Lightmap coordinates
    float		lightt;

	MyD3DVertex() { }
	MyD3DVertex(const TVec& v, dword _color, float _s, float _t)
	{
		x = v.x;
		y = v.y;
		z = v.z;
		color = _color;
		texs = _s;
		text = _t;
		lights = 0.0;
		lightt = 0.0;
	}
	MyD3DVertex(const TVec& v, dword _color, float _s, float _t,
		float _ls, float _lt)
	{
		x = v.x;
		y = v.y;
		z = v.z;
		color = _color;
		texs = _s;
		text = _t;
		lights = _ls;
		lightt = _lt;
	}
	MyD3DVertex(float _x, float _y, dword _color, float _s, float _t)
	{
		x = _x;
		y = _y;
		z = 0.0;
		color = _color;
		texs = _s;
		text = _t;
		lights = 0.0;
		lightt = 0.0;
	}
};

#define MYD3D_VERTEX_FORMAT		(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX2)

struct MyD3DMatrix : public D3DMATRIX
{
public:
	MyD3DMatrix()
	{
	}
	MyD3DMatrix(float f11, float f12, float f13, float f14,
				float f21, float f22, float f23, float f24,
				float f31, float f32, float f33, float f34,
				float f41, float f42, float f43, float f44 )
	{
		_11 = f11; _12 = f12; _13 = f13; _14 = f14;
		_21 = f21; _22 = f22; _23 = f23; _24 = f24;
		_31 = f31; _32 = f32; _33 = f33; _34 = f34;
		_41 = f41; _42 = f42; _43 = f43; _44 = f44;
	}

	// access grants
	float& operator () ( UINT iRow, UINT iCol )
	{
		return m[iRow][iCol];
	}
	float operator () ( UINT iRow, UINT iCol ) const
	{
		return m[iRow][iCol];
	}

	friend void MatrixMultiply(MyD3DMatrix &out, const MyD3DMatrix& a,
		const MyD3DMatrix& b);
	MyD3DMatrix operator * (const MyD3DMatrix& mat) const
	{
		MyD3DMatrix matT;
		MatrixMultiply(matT, *this, mat);
		return matT;
	}
};

class VDirect3DDrawer : public VDrawer
{
public:
	VDirect3DDrawer();
	void Init();
	void InitData();
	bool SetResolution(int, int, int);
	void InitResolution();
	void NewMap();
	void SetPalette(int);
	void StartUpdate();
	void Update();
	void BeginDirectUpdate();
	void EndDirectUpdate();
	void Shutdown();
	void* ReadScreen(int*, bool*);
	void FreeSurfCache(surfcache_t*);

	//	Rendering stuff
	void SetupView(const refdef_t*);
	void WorldDrawing();
	void EndView();

	//	Texture stuff
	void InitTextures();
	void SetTexture(int);
	void SetSpriteLump(int, int);

	//	Polygon drawing
	void DrawPolygon(TVec*, int, int, int);
	void BeginSky();
	void DrawSkyPolygon(TVec*, int, int, float, int, float);
	void EndSky();
	void DrawMaskedPolygon(TVec*, int, int, int);
	void DrawSpritePolygon(TVec*, int, int, int, dword);
	void DrawAliasModel(const TVec&, const TAVec&, VModel*, int, int, const char*, dword, int, bool);

	//	Particles
	void StartParticles();
	void DrawParticle(particle_t *);
	void EndParticles();

	//	Drawing
	void DrawPic(float, float, float, float, float, float, float, float, int, int);
	void DrawPicShadow(float, float, float, float, float, float, float, float, int, int);
	void FillRectWithFlat(float, float, float, float, float, float, float, float, const char*);
	void FillRect(float, float, float, float, dword);
	void ShadeRect(int, int, int, int, int);
	void DrawConsoleBackground(int);
	void DrawSpriteLump(float, float, float, float, int, int, boolean);

	//	Automap
	void StartAutomap();
	void DrawLine(int, int, dword, int, int, dword);
	void EndAutomap();

private:
	void Setup2D();
	void FlushTextures();
	void ReleaseTextures();
	static int ToPowerOf2(int);
#if DIRECT3D_VERSION >= 0x0800
	LPDIRECT3DTEXTURE8 CreateSurface(int, int, int, bool);
#else
	LPDIRECTDRAWSURFACE7 CreateSurface(int, int, int, bool);
#endif
	void SetPic(int);
	void GenerateTexture(int);
	void GenerateTranslatedSprite(int, int, int);
#if DIRECT3D_VERSION >= 0x0800
	void UploadTextureImage(LPDIRECT3DTEXTURE8, int, int, int, rgba_t*);
#else
	void UploadTextureImage(LPDIRECTDRAWSURFACE7, int, int, rgba_t*);
#endif
	void AdjustGamma(rgba_t *, int);
	void ResampleTexture(int, int, const byte*, int, int, byte*);
	void MipMap(int, int, byte*);
#if DIRECT3D_VERSION >= 0x0800
	LPDIRECT3DTEXTURE8 UploadTexture8(int, int, byte*, rgba_t*);
	LPDIRECT3DTEXTURE8 UploadTexture(int, int, rgba_t*);
#else
	LPDIRECTDRAWSURFACE7 UploadTexture8(int, int, byte*, rgba_t*);
	LPDIRECTDRAWSURFACE7 UploadTexture(int, int, rgba_t*);
#endif

	void FlushCaches(bool);
	void FlushOldCaches();
	surfcache_t	*AllocBlock(int, int);
	surfcache_t	*FreeBlock(surfcache_t*, bool);
	void CacheSurface(surface_t*);

#if DIRECT3D_VERSION < 0x0800
	static HRESULT CALLBACK EnumDevicesCallback(
		LPSTR lpDeviceDesc,
		LPSTR lpDeviceName,
		LPD3DDEVICEDESC7 lpD3DDeviceDesc,
		LPVOID);
	static HRESULT CALLBACK EnumZBufferCallback(LPDDPIXELFORMAT pf, void* dst);
	static HRESULT CALLBACK EnumPixelFormatsCallback(LPDDPIXELFORMAT pf, void* dst);
	static HRESULT CALLBACK EnumPixelFormats32Callback(LPDDPIXELFORMAT pf, void* dst);
	static void LogPrimCaps(FOutputDevice &Ar, const D3DPRIMCAPS &pc);
	static void LogDeviceDesc(FOutputDevice &Ar, const LPD3DDEVICEDESC7 dd);
	static void LogPixelFormat(FOutputDevice &Ar, const LPDDPIXELFORMAT pf);
#endif

	word MakeCol16(byte r, byte g, byte b, byte a)
	{
		return word(((a >> (8 - abits)) << ashift) |
			((r >> (8 - rbits)) << rshift) |
			((g >> (8 - gbits)) << gshift) |
			((b >> (8 - bbits)) << bshift));
	}
	dword MakeCol32(byte r, byte g, byte b, byte a)
	{
		return (a << ashift32) | (r << rshift32) |
			(g << gshift32) | (b << bshift32);
	}

#if DIRECT3D_VERSION >= 0x0800
	HMODULE						DLLHandle;

	//	Direct3D interfaces
	LPDIRECT3D8					Direct3D;
	LPDIRECT3DDEVICE8			RenderDevice;

	D3DVIEWPORT8				viewData;
#else
	//	DirectDraw interfaces
	LPDIRECTDRAW7				DDraw;
	LPDIRECTDRAWSURFACE7		PrimarySurface;
	LPDIRECTDRAWSURFACE7		RenderSurface;
	LPDIRECTDRAWSURFACE7		ZBuffer;

	//	Direct3D interfaces
	LPDIRECT3D7					Direct3D;
	LPDIRECT3DDEVICE7			RenderDevice;

	D3DVIEWPORT7				viewData;

	DDPIXELFORMAT				PixelFormat;
	DDPIXELFORMAT				PixelFormat32;
#endif
	MyD3DMatrix					IdentityMatrix;
	MyD3DMatrix					matProj;
	MyD3DMatrix					matView;
	dword						SurfaceMemFlag;
	bool						square_textures;
	int							maxTexSize;
	int							maxMultiTex;
	int							TexStage;

	int							abits;
	int							ashift;
	int							rbits;
	int							rshift;
	int							gbits;
	int							gshift;
	int							bbits;
	int							bshift;

	int							ashift32;
	int							rshift32;
	int							gshift32;
	int							bshift32;

	float						tex_iw;
	float						tex_ih;

	int							lastgamma;

	//	Texture filters.
#if DIRECT3D_VERSION >= 0x0800
	D3DTEXTUREFILTERTYPE		magfilter;
	D3DTEXTUREFILTERTYPE		minfilter;
	D3DTEXTUREFILTERTYPE		mipfilter;
#else
	D3DTEXTUREMAGFILTER			magfilter;
	D3DTEXTUREMINFILTER			minfilter;
	D3DTEXTUREMIPFILTER			mipfilter;
#endif

	//	Textures.
#if DIRECT3D_VERSION >= 0x0800
	LPDIRECT3DTEXTURE8			*trsprdata;
	LPDIRECT3DTEXTURE8			particle_texture;
#else
	LPDIRECTDRAWSURFACE7		*trsprdata;
	LPDIRECTDRAWSURFACE7		particle_texture;
#endif
	int							trsprlump[MAX_TRANSLATED_SPRITES];
	int							trsprtnum[MAX_TRANSLATED_SPRITES];
	int							tscount;

	//	Lightmaps.
#if DIRECT3D_VERSION >= 0x0800
	LPDIRECT3DTEXTURE8			*light_surf;
#else
	LPDIRECTDRAWSURFACE7		*light_surf;
#endif
	rgba_t						light_block[NUM_BLOCK_SURFS][BLOCK_WIDTH * BLOCK_HEIGHT];
	bool						block_changed[NUM_BLOCK_SURFS];
	surfcache_t					*light_chain[NUM_BLOCK_SURFS];

	//	Specular lightmaps.
#if DIRECT3D_VERSION >= 0x0800
	LPDIRECT3DTEXTURE8			*add_surf;
#else
	LPDIRECTDRAWSURFACE7		*add_surf;
#endif
	rgba_t						add_block[NUM_BLOCK_SURFS][BLOCK_WIDTH * BLOCK_HEIGHT];
	bool						add_changed[NUM_BLOCK_SURFS];
	surfcache_t					*add_chain[NUM_BLOCK_SURFS];

	//	Surface cache.
	surfcache_t					*freeblocks;
	surfcache_t					*cacheblocks[NUM_BLOCK_SURFS];
	surfcache_t					blockbuf[NUM_CACHE_BLOCKS];
	dword						cacheframecount;

	static VCvarI device;
	static VCvarI clear;
	static VCvarI tex_linear;
	static VCvarI dither;
	static VCvarI blend_sprites;
	static VCvarF maxdist;
	static VCvarI model_lighting;
	static VCvarI specular_highlights;
};

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PUBLIC DATA DECLARATIONS ------------------------------------------------

#endif

//**************************************************************************
//
//	$Log$
//	Revision 1.30  2006/04/05 17:23:37  dj_jl
//	More dynamic string usage in console command class.
//	Added class for handling command line arguments.
//
//	Revision 1.29  2006/02/05 14:11:00  dj_jl
//	Fixed conflict with Solaris.
//	
//	Revision 1.28  2005/05/26 16:50:14  dj_jl
//	Created texture manager class
//	
//	Revision 1.27  2005/05/03 14:57:00  dj_jl
//	Added support for specifying skin index.
//	
//	Revision 1.26  2004/08/21 17:22:15  dj_jl
//	Changed rendering driver declaration.
//	
//	Revision 1.25  2004/02/09 17:29:58  dj_jl
//	Increased block count
//	
//	Revision 1.24  2003/10/22 06:13:52  dj_jl
//	Freeing old blocks on overflow
//	
//	Revision 1.23  2003/03/08 12:08:04  dj_jl
//	Beautification.
//	
//	Revision 1.22  2002/08/28 16:39:19  dj_jl
//	Implemented sector light color.
//	
//	Revision 1.21  2002/07/13 07:38:00  dj_jl
//	Added drawers to the object tree.
//	
//	Revision 1.20  2002/03/28 17:56:52  dj_jl
//	Increased lightmap texture count.
//	
//	Revision 1.19  2002/01/07 12:16:41  dj_jl
//	Changed copyright year
//	
//	Revision 1.18  2001/11/09 14:18:40  dj_jl
//	Added specular highlights
//	
//	Revision 1.17  2001/10/27 07:45:01  dj_jl
//	Added gamma controls
//	
//	Revision 1.16  2001/10/18 17:36:31  dj_jl
//	A lots of changes for Alpha 2
//	
//	Revision 1.15  2001/10/12 17:28:26  dj_jl
//	Blending of sprite borders
//	
//	Revision 1.14  2001/10/09 17:21:39  dj_jl
//	Added sky begining and ending functions
//	
//	Revision 1.13  2001/10/04 17:22:05  dj_jl
//	My overloaded matrix, beautification
//	
//	Revision 1.12  2001/09/14 16:48:22  dj_jl
//	Switched to DirectX 8
//	
//	Revision 1.11  2001/09/12 17:31:27  dj_jl
//	Rectangle drawing and direct update for plugins
//	
//	Revision 1.10  2001/08/29 17:47:55  dj_jl
//	Added texture filtering variables
//	
//	Revision 1.9  2001/08/24 17:03:57  dj_jl
//	Added mipmapping, removed bumpmap test code
//	
//	Revision 1.8  2001/08/23 17:47:57  dj_jl
//	Started work on mipmapping
//	
//	Revision 1.7  2001/08/15 17:15:55  dj_jl
//	Drawer API changes, removed wipes
//	
//	Revision 1.6  2001/08/07 16:46:23  dj_jl
//	Added player models, skins and weapon
//	
//	Revision 1.5  2001/08/04 17:29:11  dj_jl
//	Added depth hack for weapon models
//	
//	Revision 1.4  2001/08/01 17:33:58  dj_jl
//	Fixed drawing of spite lump for player setup menu, beautification
//	
//	Revision 1.3  2001/07/31 17:16:30  dj_jl
//	Just moved Log to the end of file
//	
//	Revision 1.2  2001/07/27 14:27:54  dj_jl
//	Update with Id-s and Log-s, some fixes
//
//**************************************************************************
