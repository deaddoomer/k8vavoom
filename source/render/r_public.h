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
//**  Copyright (C) 2018-2021 Ketmar Dark
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
struct particle_t;
struct dlight_t;


// ////////////////////////////////////////////////////////////////////////// //
// there is little need to use bigger translation tables
// usually, 5 bits of color info is enough, so 32x32x32
// color cube is ok for our purposes. but meh...

//#define VAVOOM_RGB_TABLE_7_BITS
#define VAVOOM_RGB_TABLE_6_BITS
//#define VAVOOM_RGB_TABLE_5_BITS


// ////////////////////////////////////////////////////////////////////////// //
struct refdef_t {
  int x, y;
  int width, height;
  float fovx, fovy;
  bool drawworld;
  bool DrawCamera;
};


// ////////////////////////////////////////////////////////////////////////// //
struct fakefloor_t {
  sec_plane_t floorplane;
  sec_plane_t ceilplane;
  sec_params_t params;
  enum {
    FLAG_CreatedByLoader = 1u<<0,
  };
  vuint32 flags;
};


// ////////////////////////////////////////////////////////////////////////// //
class VDecalAnim;

struct decal_t {
  // flags bit values
  enum {
    SlideFloor = 0x0001U, // curz: offset with floorz, slide with it
    SlideCeil = 0x0002U, // curz: offset with ceilingz, slide with it
    FlipX = 0x0010U,
    FlipY = 0x0020U,
    Fullbright = 0x0100U,
    Fuzzy = 0x0200U,
    SideDefOne = 0x0800U,
    NoMidTex = 0x1000U, // don't render on middle texture
    NoTopTex = 0x2000U, // don't render on top texture
    NoBotTex = 0x4000U, // don't render on bottom texture
    FromV2   = 0x8000U, // use `v2` vertex as base (for wall decals on partner segs)
    // special decal types
    BloodSplat = 0x10000U,
    BootPrint  = 0x20000U,
  };

  // dcsurf bit values
  enum {
    Wall = 0u,
    Floor = 1u, // either base region floor, or 3d subregion floor
    Ceiling = 2u, // either base region ceiling, or 3d subregion ceiling
    // note that `3u` is invalid
    FakeFlag = 4u,
    SurfTypeMask = 0x03u,
  };

  decal_t *prev; // in seg/sreg/slidesec
  decal_t *next; // in seg/sreg/slidesec
  seg_t *seg; // this is non-null for wall decals
  subregion_t *sreg; // this is non-null for floor/ceiling decals (only set if renderer is active)
  vuint32 dcsurf; // decal type
  sector_t *slidesec; // backsector for SlideXXX
  subsector_t *sub; // owning subsector for floor/ceiling decal
  int eregindex; // sector region index for floor/ceiling decal (only in VLevel list)
  VName dectype;
  VName bootname; // boot decal name
  float boottime; // how long it bootprints should be emited after stepping onto this?
  //VName picname;
  VTextureID texture;
  int translation;
  unsigned flags;
  // z and x positions has no image offset added
  float worldx, worldy; // world coordinates for floor/ceiling decals
  float angle; // decal rotation angle (around its center point)
  float orgz; // original z position for wall decals
  float curz; // z position (offset with floor/ceiling TexZ if not midtex, see `flags`)
  float xdist; // offset from linedef start, in map units
  float ofsX, ofsY; // for animators
  float origScaleX, origScaleY; // for animators
  float scaleX, scaleY; // actual
  float origAlpha; // for animators
  float alpha; // decal alpha
  float addAlpha; // alpha for additive translucency (not supported yet)
  VDecalAnim *animator; // decal animator (can be nullptr)
  decal_t *prevanimated; // so we can skip static decals
  decal_t *nextanimated; // so we can skip static decals
  // for flat decals
  float bbox2d[4]; // 2d bounding box for the original (maximum) flat decal size
  // in VLevel subsector decal list
  decal_t *subprev;
  decal_t *subnext;
  // in renderer region decal list
  decal_t *sregprev;
  decal_t *sregnext;

  /*
  unsigned uid; // unique id, so we can process all clones
  // linked list of all decals with the given uid
  decal_t *uidprev;
  decal_t *uidnext;
  */

  // cache
  unsigned lastFlags;
  float lastScaleX, lastScaleY;
  float lastWorldX, lastWorldY;
  float lastAngle;
  float lastOfsX, lastOfsY;
  float lastCurZ;
  float lastPlaneDist, lastTexZ;
  // calculated cached values
  TVec saxis, taxis;
  float soffs, toffs;
  TVec v1, v2, v3, v4;

  // nore that floor/ceiling type should be correctly set for 3d floor subregions
  // i.e. decal on top of 3d floor is ceiling decal

  inline bool isWall () const noexcept { return (dcsurf == Wall); }
  inline bool isNonWall () const noexcept { return ((dcsurf&SurfTypeMask) != Wall); }
  inline bool isFloor () const noexcept { return ((dcsurf&SurfTypeMask) == Floor); }
  inline bool isCeiling () const noexcept { return ((dcsurf&SurfTypeMask) == Ceiling); }
  // for fake floor/ceiling? (not implemented yet)
  inline bool isFake () const noexcept { return (dcsurf&FakeFlag); }

  inline bool isFloorBloodSplat () const noexcept { return (((dcsurf&SurfTypeMask) == Floor) && (flags&BloodSplat)); }

  // WARNING! call this ONLY for known non-wall decals!
  // non-base regions has their floor and ceiling switched
  // so floor decal for 3d floor region is actually ceiling decal, and vice versa
  //inline bool isFloorEx () const noexcept { return (eregindex ? isCeiling() : isFloor()); }

  // should be called after animator was set
  // might be called by animation thinker too
  void calculateBBox () noexcept;

  inline bool needRecalc (const float pdist, const float tz) const noexcept {
    return
      lastFlags != flags ||
      FASI(lastPlaneDist) != FASI(pdist) ||
      FASI(lastTexZ) != tz ||
      FASI(lastScaleX) != FASI(scaleX) ||
      FASI(lastScaleY) != FASI(scaleY) ||
      FASI(lastWorldX) != FASI(worldx) ||
      FASI(lastWorldY) != FASI(worldy) ||
      FASI(lastOfsX) != FASI(ofsX) ||
      FASI(lastOfsY) != FASI(ofsY) ||
      FASI(lastCurZ) != FASI(curz) ||
      FASI(lastAngle) != FASI(angle);
  }

  // doesn't update axes and offsets
  inline void updateCache (const float pdist, const float tz) noexcept {
    lastFlags = flags;
    lastPlaneDist = pdist;
    lastTexZ = tz;
    lastScaleX = scaleX;
    lastScaleY = scaleY;
    lastWorldX = worldx;
    lastWorldY = worldy;
    lastOfsX = ofsX;
    lastOfsY = ofsY;
    lastCurZ = curz;
    lastAngle = angle;
  }

  inline void invalidateCache () noexcept { lastPlaneDist = FLT_MAX; } // arbitrary number
};


// ////////////////////////////////////////////////////////////////////////// //
class VRenderLevelPublic : public VInterface {
public:
  struct LightInfo {
    TVec origin;
    float radius;
    vuint32 color;
    bool active;
  };

public: //k8: i am too lazy to write accessor methods
  bool staticLightsFiltered;

  // base planes to create fov-based frustum
  TClipBase clip_base;
  refdef_t refdef;

  VLevel *Level;

public:
  VRenderLevelPublic (VLevel *ALevel) noexcept;

  inline VLevel *GetLevel () const noexcept { return Level; }

  virtual void PreRender () = 0;
  virtual void PObjModified (polyobj_t *po) = 0; // fix polyobject segs
  virtual void SetupFakeFloors (sector_t *) = 0;
  virtual void RenderPlayerView () = 0;

  virtual void SectorModified (sector_t *sec) = 0; // this sector *may* be moved

  virtual void ResetStaticLights () = 0;
  virtual void AddStaticLightRGB (vuint32 OwnerUId, const VLightParams &lpar) = 0;
  virtual void MoveStaticLightByOwner (vuint32 OwnerUId, const TVec &origin) = 0;
  virtual void RemoveStaticLightByOwner (vuint32 OwnerUId) = 0;
  virtual int GetNumberOfStaticLights () = 0;

  // for polyobjects
  // called to invalidate polyobject containing subsector
  virtual void InvalidateStaticLightmapsSubs (subsector_t *sub) = 0;
  // only polyobject itself
  virtual void InvalidatePObjLMaps (polyobj_t *po) = 0;

  virtual void ClearReferences () = 0;

  virtual dlight_t *AllocDlight (VThinker *, const TVec &lorg, float radius, int lightid=-1) = 0;
  virtual dlight_t *FindDlightById (int lightid) = 0;
  virtual void DecayLights (float) = 0;

  // called from `PreRender()` to register all level thinkers with `ThinkerAdded()`
  virtual void RegisterAllThinkers () = 0;
  // adds suid -> object mapping
  virtual void ThinkerAdded (VThinker *Owner) = 0;
  // this removes light, and removes suid -> object mapping
  virtual void ThinkerDestroyed (VThinker *Owner) = 0;

  virtual particle_t *NewParticle (const TVec &porg) = 0;

  virtual int GetStaticLightCount () const noexcept = 0;
  virtual LightInfo GetStaticLight (int idx) const noexcept = 0;

  virtual int GetDynamicLightCount () const noexcept = 0;
  virtual LightInfo GetDynamicLight (int idx) const noexcept = 0;

  virtual void NukeLightmapCache () = 0;
  virtual void ResetLightmaps (bool recalcNow) = 0;

  virtual void FullWorldUpdate (bool forceClientOrigin) = 0;

  virtual bool isNeedLightmapCache () const noexcept = 0;
  virtual void saveLightmaps (VStream *strm) = 0;
  virtual bool loadLightmaps (VStream *strm) = 0;

  // `dflags` is `VDrawer::ELFlag_XXX` set
  // returns 0 for unknown
  virtual vuint32 CalcEntityLight (VEntity *lowner, unsigned dflags) = 0;

  // will create new decals for all touching subsectors
  // `origdc` must be valid, with proper flags and region number
  // `origdc` won't be modified, owned, or destroyed
  //void SpreadDecal (const decal_t *origdc);

  // this will NOT take ownership, nor create any clones!
  // will include decal into list of decals for the given subregion
  virtual void AppendFlatDecal (decal_t *dc) = 0;
  // call this before destroying the decal
  virtual void RemoveFlatDecal (decal_t *dc) = 0;

  virtual decal_t *GetSRegFloorDecalHead (const subregion_t *sreg) noexcept = 0;
  virtual decal_t *GetSRegCeilingDecalHead (const subregion_t *sreg) noexcept = 0;
};


// ////////////////////////////////////////////////////////////////////////// //
// r_data
void R_InitData ();
void R_ShutdownData ();
void R_InstallSprite (const char *, int);
void R_InstallSpriteComplete ();
bool R_AreSpritesPresent (int);
int R_ParseDecorateTranslation (VScriptParser *sc, int GameMax, VStr trname=VStr::EmptyString);
int R_FindTranslationByName (VStr trname);
int R_GetBloodTranslation (int Col, bool allowAdd);

// returns 0 if translation cannot be created
int R_CreateDesaturatedTranslation (int AStart, int AEnd, float rs, float gs, float bs, float re, float ge, float be);
int R_CreateBlendedTranslation (int AStart, int AEnd, int r, int g, int b);
int R_CreateTintedTranslation (int AStart, int AEnd, int r, int g, int b, int amount);

// color 0 *WILL* be translated too!
int R_CreateColorTranslation (const VColorRGBA newpal[256]);
void R_GetGamePalette (VColorRGBA newpal[256]);
void R_GetTranslatedPalette (int transnum, VColorRGBA newpal[256]);

void R_ParseEffectDefs ();
VLightEffectDef *R_FindLightEffect (VStr Name);

// r_main
void R_Init (); // Called by startup code.
void R_Start (VLevel *);
void R_SetViewSize (int blocks);
void R_ForceViewSizeUpdate ();
void R_RenderPlayerView ();
VTextureTranslation *R_GetCachedTranslation (int, VLevel *);

// r_things
// ignoreVScr: draw on framebuffer, ignore virutal screen
void R_DrawSpritePatch (float x, float y, int sprite, int frame=0, int rot=0,
                        int TranslStart=0, int TranslEnd=0, int Color=0, float scale=1.0f,
                        bool ignoreVScr=false);
void R_InitSprites ();

//  2D graphics
void R_DrawPic (int x, int y, int handle, float Alpha=1.0f);
void R_DrawPicScaled (int x, int y, int handle, float scale, float Alpha=1.0f);
void R_DrawPicFloat (float x, float y, int handle, float Alpha=1.0f);
// wdt and hgt are in [0..1] range
void R_DrawPicPart (int x, int y, float pwdt, float phgt, int handle, float Alpha=1.0f);
void R_DrawPicFloatPart (float x, float y, float pwdt, float phgt, int handle, float Alpha=1.0f);
void R_DrawPicPartEx (int x, int y, float tx0, float ty0, float tx1, float ty1, int handle, float Alpha=1.0f);
void R_DrawPicFloatPartEx (float x, float y, float tx0, float ty0, float tx1, float ty1, int handle, float Alpha=1.0f);

// calculated with regards to screen size
float R_GetAspectRatio ();
// calculated without regards to screen size (i.e. 4.0/3.0, for example)
float R_GetAspectRatioValue ();

// known aspect ratios list
int R_GetAspectRatioCount () noexcept;
int R_GetAspectRatioHoriz (int idx) noexcept;
int R_GetAspectRatioVert (int idx) noexcept;
const char *R_GetAspectRatioDsc (int idx) noexcept;

bool R_EntModelNoSelfShadow (VEntity *mobj);

// r_sky
void R_InitSkyBoxes ();

// checks for sky flat
bool R_IsSkyFlatPlane (const sec_plane_t *SPlane);

// WARNING! those two checks can access VEntity!
// checks for sky flat or stacked sector
bool R_IsAnySkyFlatPlane (const sec_plane_t *SPlane);
// checks for stacked sector
bool R_IsStackedSectorPlane (const sec_plane_t *SPlane);
// should not be a stacked sector
bool R_IsStrictlySkyFlatPlane (const sec_plane_t *SPlane);

VName R_HasNamedSkybox (VStr aname);


struct SpriteTexInfo {
  enum {
    Flipped = 1u<<0,
  };

  int texid; // <0: no texture
  vuint32 flags;

  inline bool isFlipped () const noexcept { return (flags&Flipped); }
};

//  returns `false` if no texture found
bool R_GetSpriteTextureInfo (SpriteTexInfo *nfo, int sprindex, int sprframe);


// ////////////////////////////////////////////////////////////////////////// //
// camera texture
class VCameraTexture : public VTexture {
public:
  bool bUsedInFrame;
  double NextUpdateTime; // systime
  bool bUpdated;
  int camfboidx;
  bool bPixelsLoaded;

  VCameraTexture (VName, int, int);
  virtual ~VCameraTexture () override;
  virtual bool IsDynamicTexture () const noexcept override;
  virtual bool IsHugeTexture () const noexcept override;
  virtual bool CheckModified () override;
  virtual void ReleasePixels () override;
  virtual vuint8 *GetPixels () override;
  void CopyImage ();
  bool NeedUpdate ();
  virtual VTexture *GetHighResolutionTexture () override;
};


// ////////////////////////////////////////////////////////////////////////// //
extern TArray<int> AllModelTextures;

extern rgba_t r_palette[256];
extern vuint8 r_black_color; // actually, palette color 0 (it is usually black, but not always)
extern vuint8 r_white_color; // and this is really white

// remaps color 0 to the nearest color, emulates VGA if necessary
// returns index of new color 0
// if `forceOpacity` is set, colors [1..255] will be forced to full opacity
int R_ProcessPalette (rgba_t pal[256], bool forceOpacity=true);

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


//==========================================================================
//
//  R_LookupRGB
//
//==========================================================================
#if defined(VAVOOM_RGB_TABLE_7_BITS)
# if defined(VAVOOM_RGB_TABLE_6_BITS) || defined(VAVOOM_RGB_TABLE_5_BITS)
#  error "choose only one RGB table size"
# endif
static VVA_ALWAYS_INLINE VVA_OKUNUSED VVA_PURE vuint8 R_LookupRGB (const vint32 r, const vint32 g, const vint32 b) noexcept {
  return r_rgbtable[(((vuint32)clampToByte(r)<<13)&0x1fc000)|(((vuint32)clampToByte(g)<<6)&0x3f80)|((clampToByte(b)>>1)&0x7fU)];
}
#elif defined(VAVOOM_RGB_TABLE_6_BITS)
# if defined(VAVOOM_RGB_TABLE_7_BITS) || defined(VAVOOM_RGB_TABLE_5_BITS)
#  error "choose only one RGB table size"
# endif
static VVA_ALWAYS_INLINE VVA_OKUNUSED VVA_PURE vuint8 R_LookupRGB (const vint32 r, const vint32 g, const vint32 b) noexcept {
  return r_rgbtable[(((vuint32)clampToByte(r)<<10)&0x3f000U)|(((vuint32)clampToByte(g)<<4)&0xfc0U)|((clampToByte(b)>>2)&0x3fU)];
}
#elif defined(VAVOOM_RGB_TABLE_5_BITS)
# if defined(VAVOOM_RGB_TABLE_6_BITS) || defined(VAVOOM_RGB_TABLE_7_BITS)
#  error "choose only one RGB table size"
# endif
static VVA_ALWAYS_INLINE VVA_OKUNUSED VVA_PURE vuint8 R_LookupRGB (const vint32 r, const vint32 g, const vint32 b) noexcept {
  return r_rgbtable[(((vuint32)clampToByte(r)<<7)&0x7c00U)|(((vuint32)clampToByte(g)<<2)&0x3e0U)|((clampToByte(b)>>3)&0x1fU)];
}
#else
#  error "choose RGB table size"
#endif
