//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************
//**
//**  Defines shared by refresh and drawer
//**
//**************************************************************************

#ifndef _R_SHARED_H
#define _R_SHARED_H

// HEADER FILES ------------------------------------------------------------

#include "fmd2defs.h"
#include "drawer.h"

// MACROS ------------------------------------------------------------------

//  Colour maps
enum
{
  CM_Default,
  CM_Inverse,
  CM_Gold,
  CM_Red,
  CM_Green,

  CM_Max
};

//  Simulate light fading using dark fog
enum
{
  FADE_LIGHT = 0xff010101
};

// TYPES -------------------------------------------------------------------

class TClipPlane : public TPlane
{
public:
  TClipPlane    *next;

  int       clipflag;
  TVec      enter;
  TVec      exit;
  int       entered;
  int       exited;
};

struct texinfo_t
{
  TVec      saxis;
  float     soffs;
  TVec      taxis;
  float     toffs;
  VTexture *Tex;
  bool noDecals;
  //  1.1 for solid surfaces
  // Alpha for masked surfaces
  float     Alpha;
  bool      Additive;
  vuint8      ColourMap;
};

struct surface_t
{
  surface_t *next;
  surface_t *DrawNext;
  texinfo_t *texinfo;
  TPlane *plane;
  sec_plane_t *HorizonPlane;
  vuint32     Light;    //  Light level and colour.
  vuint32     Fade;
  vuint8 *lightmap;
  rgb_t *lightmap_rgb;
  int       dlightframe;
  int       dlightbits;
  int       count;
  short     texturemins[2];
  short     extents[2];
  surfcache_t *CacheSurf;
  seg_t *dcseg; // seg with decals for this surface
  TVec      verts[1];
};

//
//  Camera texture.
//
class VCameraTexture : public VTexture
{
public:
  vuint8 *Pixels;
  bool    bNeedsUpdate;
  bool    bUpdated;

  VCameraTexture(VName, int, int);
  virtual ~VCameraTexture() override;
  virtual bool CheckModified() override;
  virtual vuint8 *GetPixels() override;
  virtual void Unload() override;
  void CopyImage();
  virtual VTexture *GetHighResolutionTexture() override;
};

class VRenderLevelShared;

//
//  Base class for portals.
//
class VPortal
{
public:
  VRenderLevelShared *RLev;
  TArray<surface_t*>    Surfs;
  int           Level;

  VPortal(VRenderLevelShared *ARLev);
  virtual ~VPortal();
  virtual bool NeedsDepthBuffer() const;
  virtual bool IsSky() const;
  virtual bool MatchSky(class VSky*) const;
  virtual bool MatchSkyBox(VEntity*) const;
  virtual bool MatchMirror(TPlane*) const;
  void Draw(bool);
  virtual void DrawContents() = 0;

protected:
  void SetUpRanges(VViewClipper&, bool);
};

struct VMeshFrame
{
  TVec *Verts;
  TVec *Normals;
  TPlane *Planes;
  vuint32     VertsOffset;
  vuint32     NormalsOffset;
};

struct VMeshSTVert
{
  float     S;
  float     T;
};

struct VMeshTri
{
  vuint16       VertIndex[3];
};

struct VMeshEdge
{
  vuint16       Vert1;
  vuint16       Vert2;
  vint16        Tri1;
  vint16        Tri2;
};

struct VMeshModel
{
  VStr        Name;
  mmdl_t *Data;   // only access through Mod_Extradata
  TArray<VName>   Skins;
  TArray<VMeshFrame>  Frames;
  TArray<TVec>    AllVerts;
  TArray<TVec>    AllNormals;
  TArray<TPlane>    AllPlanes;
  TArray<VMeshSTVert> STVerts;
  TArray<VMeshTri>  Tris;
  TArray<VMeshEdge> Edges;
  bool        Uploaded;
  vuint32       VertsBuffer;
  vuint32       IndexBuffer;
};

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void R_DrawViewBorder();

// PUBLIC DATA DECLARATIONS ------------------------------------------------

//
// POV related.
//
extern TVec       vieworg;
extern TVec       viewforward;
extern TVec       viewright;
extern TVec       viewup;
extern TAVec      viewangles;

extern VCvarB     r_fog;
extern VCvarF     r_fog_r;
extern VCvarF     r_fog_g;
extern VCvarF     r_fog_b;
extern VCvarF     r_fog_start;
extern VCvarF     r_fog_end;
extern VCvarF     r_fog_density;

extern VCvarB     r_vsync;

extern VCvarB     r_fade_light;
extern VCvarF     r_fade_factor;

extern VCvarF     r_sky_bright_factor;

extern TClipPlane   view_clipplanes[5];

extern bool       MirrorFlip;
extern bool       MirrorClip;

extern int        r_dlightframecount;
extern bool       r_light_add;
extern vuint32      blocklightsr[18 * 18];
extern vuint32      blocklightsg[18 * 18];
extern vuint32      blocklightsb[18 * 18];

extern rgba_t     r_palette[256];
extern vuint8     r_black_colour;
extern vuint8     r_white_colour;

extern vuint8     r_rgbtable[32 * 32 * 32 + 4];

extern int        usegamma;
extern vuint8     gammatable[5][256];

extern float      PixelAspect;

extern VTextureTranslation  ColourMaps[CM_Max];

enum { SHADEDOT_QUANT = 16 };
extern float      r_avertexnormal_dots[SHADEDOT_QUANT][256];

//==========================================================================
//
//  R_LookupRBG
//
//==========================================================================

static inline int R_LookupRGB(vuint8 r, vuint8 g, vuint8 b)
{
  return r_rgbtable[((r << 7) & 0x7c00) + ((g << 2) & 0x3e0) +
    ((b >> 3) & 0x1f)];
}

void R_ParseMapDefSkyBoxesScript (VScriptParser *sc);
VName R_HasNamedSkybox (const VStr &aname);

#endif
