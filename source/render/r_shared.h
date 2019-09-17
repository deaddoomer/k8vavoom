//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, version 3 of the License ONLY.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**  You should have received a copy of the GNU General Public License
//**  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//**
//**************************************************************************
//**
//**  Defines shared by refresh and drawer
//**
//**************************************************************************
#ifndef VAVOOM_RENDER_SHARED_H
#define VAVOOM_RENDER_SHARED_H

// there is little need to use bigger translation tables
// usually, 5 bits of color info is enough, so 32x32x32
// color cube is ok for our purposes. but meh...

//#define VAVOOM_RGB_TABLE_7_BITS
#define VAVOOM_RGB_TABLE_6_BITS
//#define VAVOOM_RGB_TABLE_5_BITS

#include "drawer.h"


// ////////////////////////////////////////////////////////////////////////// //
// color maps
enum {
  CM_Default,
  CM_Inverse,
  CM_Gold,
  CM_Red,
  CM_Green,

  CM_Max,
};

// simulate light fading using dark fog
enum {
  FADE_LIGHT = 0xff010101u,
};


// ////////////////////////////////////////////////////////////////////////// //
struct texinfo_t {
  TVec saxis;
  float soffs;
  TVec taxis;
  float toffs;
  VTexture *Tex;
  vint32 noDecals;
  // 1.1f for solid surfaces
  // alpha for masked surfaces
  // not always right, tho (1.0f vs 1.1f); need to be checked and properly fixed
  float Alpha;
  vint32 Additive;
  vuint8 ColorMap;

  inline bool isEmptyTexture () const { return (!Tex || Tex->Type == TEXTYPE_Null); }

  // call this to check if we need to change OpenGL texture
  inline bool needChange (const texinfo_t &other) const {
    if (&other == this) return false;
    return
      Tex != other.Tex ||
      ColorMap != other.ColorMap ||
      FASI(saxis) != FASI(other.saxis) ||
      FASI(soffs) != FASI(other.soffs) ||
      FASI(taxis) != FASI(other.taxis) ||
      FASI(toffs) != FASI(other.toffs);
  }

  // call this to cache info for `needChange()`
  inline void updateLastUsed (const texinfo_t &other) {
    if (&other == this) return;
    Tex = other.Tex;
    ColorMap = other.ColorMap;
    saxis = other.saxis;
    soffs = other.soffs;
    taxis = other.taxis;
    toffs = other.toffs;
    // other fields doesn't matter
  }

  inline void resetLastUsed () {
    Tex = nullptr; // it is enough, but to be sure...
    ColorMap = 255; // impossible colormap
  }

  inline void initLastUsed () {
    saxis = taxis = TVec(-99999, -99999, -99999);
    soffs = toffs = -99999;
    Tex = nullptr;
    noDecals = false;
    Alpha = -666;
    Additive = 666;
    ColorMap = 255;
  }
};


struct surface_t {
  enum {
    MAXWVERTS = 8+8, // maximum number of vertices in wsurf (world/wall surface)
  };

  enum {
    DF_MASKED       = 1u<<0, // this surface has "masked" texture
    DF_WSURF        = 1u<<1, // is this world/wall surface? such surfs are guaranteed to have space for `MAXWVERTS`
    DF_FIX_CRACKS   = 1u<<2, // this surface must be subdivised to fix "cracks"
    DF_CALC_LMAP    = 1u<<3, // calculate static lightmap
    //DF_FLIP_PLANE   = 1u<<4, // flip plane
    DF_NO_FACE_CULL = 1u<<5, // ignore face culling
  };

  enum {
    TF_TOP     = 1u<<0,
    TF_BOTTOM  = 1u<<1,
    TF_MIDDLE  = 1u<<2,
    TF_FLOOR   = 1u<<3,
    TF_CEILING = 1u<<4,
    TF_TOPHACK = 1u<<5,
  };

  surface_t *next;
  texinfo_t *texinfo; // points to owning `sec_surface_t`
  TPlane plane; // was pointer
  sec_plane_t *HorizonPlane;
  vuint32 Light; // light level and color
  vuint32 Fade;
  float glowFloorHeight;
  float glowCeilingHeight;
  vuint32 glowFloorColor;
  vuint32 glowCeilingColor;
  subsector_t *subsector; // owning subsector
  seg_t *seg; // owning seg (can be `nullptr` for floor/ceiling)
  vuint32 typeFlags; // TF_xxx
  // not exposed to VC
  int lmsize, lmrgbsize; // to track static lightmap memory
  vuint8 *lightmap;
  rgb_t *lightmap_rgb;
  vuint32 queueframe; // this is used to prevent double queuing
  vuint32 dlightframe;
  vuint32 dlightbits;
  vuint32 drawflags; // DF_XXX
  int count;
  /*short*/int texturemins[2];
  /*short*/int extents[2];
  surfcache_t *CacheSurf;
  int plvisible; // cached visibility flag, set in main BSP collector (VRenderLevelShared::SurfCheckAndQueue)
  //vuint32 fixvertbmp; // for world surfaces, this is bitmap of "fix" additional surfaces (bit 1 means "added fix")
  TVec verts[1]; // dynamic array

  // to use in renderer
  inline bool IsVisible (const TVec &point) const {
    return (!(drawflags&DF_NO_FACE_CULL) ? !plane.PointOnSide(point) : (plane.PointOnSide2(point) != 2));
  }

  inline int PointOnSide (const TVec &point) const {
    return plane.PointOnSide(point);
  }

  inline bool SphereTouches (const TVec &center, float radius) const {
    return plane.SphereTouches(center, radius);
  }

  inline float GetNormalZ () const { return plane.normal.z; }
  inline const TVec &GetNormal () const { return plane.normal; }
  inline float GetDist () const { return plane.dist; }

  inline void GetPlane (TPlane *p) const { *p = plane; }
};


// ////////////////////////////////////////////////////////////////////////// //
// camera texture
class VCameraTexture : public VTexture {
public:
  bool bNeedsUpdate;
  bool bUpdated;

  VCameraTexture (VName, int, int);
  virtual ~VCameraTexture () override;
  virtual bool CheckModified () override;
  virtual vuint8 *GetPixels () override;
  virtual void Unload () override;
  void CopyImage ();
  virtual VTexture *GetHighResolutionTexture () override;
};


class VRenderLevelShared;


// ////////////////////////////////////////////////////////////////////////// //
// base class for portals
class VPortal {
public:
  VRenderLevelShared *RLev;
  TArray<surface_t *> Surfs;
  int Level;
  bool stackedSector;

  VPortal (VRenderLevelShared *ARLev);
  virtual ~VPortal ();
  virtual bool NeedsDepthBuffer () const;
  virtual bool IsSky () const;
  virtual bool IsMirror () const;
  virtual bool IsStack () const;
  virtual bool MatchSky (class VSky *) const;
  virtual bool MatchSkyBox (VEntity *) const;
  virtual bool MatchMirror (TPlane *) const;
  void Draw (bool);
  virtual void DrawContents () = 0;

protected:
  void SetUpRanges (const refdef_t &refdef, VViewClipper &Range, bool Revert, bool SetFrustum);
};


// ////////////////////////////////////////////////////////////////////////// //
// one alias model frame
struct VMeshFrame {
  VStr Name;
  TVec Scale;
  TVec Origin;
  TVec *Verts;
  TVec *Normals;
  TPlane *Planes;
  vuint32 TriCount;
  // cached offsets on OpenGL buffer
  vuint32 VertsOffset;
  vuint32 NormalsOffset;
};

#pragma pack(push,1)
// texture coordinates
struct VMeshSTVert {
  float S;
  float T;
};

// one mesh triangle
struct VMeshTri {
  vuint16 VertIndex[3];
};

struct VMeshEdge {
  vuint16 Vert1;
  vuint16 Vert2;
  vint16 Tri1;
  vint16 Tri2;
};
#pragma pack(pop)

struct mmdl_t;

struct VMeshModel {
  struct SkinInfo {
    VName fileName;
    int textureId; // -1: not loaded yet
    int shade; // -1: none
  };

  VStr Name;
  int MeshIndex;
  TArray<SkinInfo> Skins;
  TArray<VMeshFrame> Frames;
  TArray<TVec> AllVerts;
  TArray<TVec> AllNormals;
  TArray<TPlane> AllPlanes;
  TArray<VMeshSTVert> STVerts;
  TArray<VMeshTri> Tris; // vetex indicies
  TArray<VMeshEdge> Edges; // for `Tris`
  bool loaded;
  bool Uploaded;
  bool HadErrors;
  vuint32 VertsBuffer;
  vuint32 IndexBuffer;
};


// ////////////////////////////////////////////////////////////////////////// //
void R_DrawViewBorder ();
void R_ParseMapDefSkyBoxesScript (VScriptParser *sc);
VName R_HasNamedSkybox (VStr aname);


// ////////////////////////////////////////////////////////////////////////// //
// POV related
// k8: this should be prolly moved to renderer, and recorded in render list
// if i'll do that, i will be able to render stacked sectors, and mirrors
// without portals.
//
// in render lists it is prolly enough to store current view transformation
// matrix, because surface visibility flags are already set by the queue manager.
//
// another thing to consider is queue manager limitation: one surface can be
// queued only once. this is not a hard limitation, though, as queue manager is
// using arrays to keep surface pointers, but it is handy for various render
// checks. we prolly need to increment queue frame counter when view changes.
extern TVec vieworg;
extern TVec viewforward;
extern TVec viewright;
extern TVec viewup;
extern TAVec viewangles;
extern TFrustum view_frustum;

extern bool MirrorFlip;
extern bool MirrorClip;


// ////////////////////////////////////////////////////////////////////////// //
//extern VCvarI r_fog;
extern VCvarF r_fog_r;
extern VCvarF r_fog_g;
extern VCvarF r_fog_b;
extern VCvarF r_fog_start;
extern VCvarF r_fog_end;
extern VCvarF r_fog_density;

extern VCvarB r_vsync;
extern VCvarB r_vsync_adaptive;

extern VCvarB r_fade_light;
extern VCvarF r_fade_factor;

extern VCvarF r_sky_bright_factor;

extern rgba_t r_palette[256];
extern vuint8 r_black_color;
extern vuint8 r_white_color;

#if defined(VAVOOM_RGB_TABLE_7_BITS)
# define VAVOOM_COLOR_COMPONENT_MAX  (128)
# define VAVOOM_COLOR_COMPONENT_BITS (7)
#elif defined(VAVOOM_RGB_TABLE_6_BITS)
# define VAVOOM_COLOR_COMPONENT_MAX  (64)
# define VAVOOM_COLOR_COMPONENT_BITS (6)
#else
# define VAVOOM_COLOR_COMPONENT_MAX  (32)
# define VAVOOM_COLOR_COMPONENT_BITS (5)
#endif
extern vuint8 r_rgbtable[VAVOOM_COLOR_COMPONENT_MAX*VAVOOM_COLOR_COMPONENT_MAX*VAVOOM_COLOR_COMPONENT_MAX+4];

extern int usegamma;
//extern const vuint8 gammatable[5][256];
extern const vuint8 *getGammaTable (int idx);

extern float PixelAspect;

extern VTextureTranslation ColorMaps[CM_Max];


//==========================================================================
//
//  R_LookupRGB
//
//==========================================================================
#if defined(VAVOOM_RGB_TABLE_7_BITS)
# if defined(VAVOOM_RGB_TABLE_6_BITS) || defined(VAVOOM_RGB_TABLE_5_BITS)
#  error "choose only one RGB table size"
# endif
static inline vuint8 __attribute__((unused)) R_LookupRGB (vint32 r, vint32 g, vint32 b) {
  return r_rgbtable[(((vuint32)clampToByte(r)<<13)&0x1fc000)|(((vuint32)clampToByte(g)<<6)&0x3f80)|((clampToByte(b)>>1)&0x7fU)];
}
#elif defined(VAVOOM_RGB_TABLE_6_BITS)
# if defined(VAVOOM_RGB_TABLE_7_BITS) || defined(VAVOOM_RGB_TABLE_5_BITS)
#  error "choose only one RGB table size"
# endif
static inline vuint8 __attribute__((unused)) R_LookupRGB (vint32 r, vint32 g, vint32 b) {
  return r_rgbtable[(((vuint32)clampToByte(r)<<10)&0x3f000U)|(((vuint32)clampToByte(g)<<4)&0xfc0U)|((clampToByte(b)>>2)&0x3fU)];
}
#elif defined(VAVOOM_RGB_TABLE_5_BITS)
# if defined(VAVOOM_RGB_TABLE_6_BITS) || defined(VAVOOM_RGB_TABLE_7_BITS)
#  error "choose only one RGB table size"
# endif
static inline vuint8 __attribute__((unused)) R_LookupRGB (vint32 r, vint32 g, vint32 b) {
  return r_rgbtable[(((vuint32)clampToByte(r)<<7)&0x7c00U)|(((vuint32)clampToByte(g)<<2)&0x3e0U)|((clampToByte(b)>>3)&0x1fU)];
}
#else
#  error "choose RGB table size"
#endif


#endif
