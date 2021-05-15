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
  };
  // dcsurf bit values
  enum {
    Wall = 0u,
    Floor = 1u,
    Ceiling = 2u,
    FakeFlag = 4u,
    SurfTypeMask = 0x03,
  };
  decal_t *prev; // in seg/sreg/slidesec
  decal_t *next; // in seg/sreg/slidesec
  seg_t *seg; // this is non-null for wall decals
  subregion_t *sreg; // this is non-null for floor/ceiling decals
  vuint32 dcsurf; // decal type
  sector_t *slidesec; // backsector for SlideXXX, or owning sector for floor/ceiling decal (only in VLevel list)
  int eregindex; // sector region index for floor/ceiling decal (only in VLevel list)
  VName dectype;
  //VName picname;
  VTextureID texture;
  int translation;
  vuint32 flags;
  // z and x positions has no image offset added
  float worldx, worldy; // world coordinates for floor/ceiling decals
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
};


// ////////////////////////////////////////////////////////////////////////// //
class VRenderLevelPublic : public VInterface {
public: //k8: i am too lazy to write accessor methods
  bool staticLightsFiltered;

  // base planes to create fov-based frustum
  TClipBase clip_base;
  refdef_t refdef;

public:
  struct LightInfo {
    TVec origin;
    float radius;
    vuint32 color;
    bool active;
  };

public:
  VRenderLevelPublic () noexcept;

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


//==========================================================================
//
//  R_LookupRGB
//
//==========================================================================
#if defined(VAVOOM_RGB_TABLE_7_BITS)
# if defined(VAVOOM_RGB_TABLE_6_BITS) || defined(VAVOOM_RGB_TABLE_5_BITS)
#  error "choose only one RGB table size"
# endif
static inline vuint8 VVA_OKUNUSED R_LookupRGB (vint32 r, vint32 g, vint32 b) {
  return r_rgbtable[(((vuint32)clampToByte(r)<<13)&0x1fc000)|(((vuint32)clampToByte(g)<<6)&0x3f80)|((clampToByte(b)>>1)&0x7fU)];
}
#elif defined(VAVOOM_RGB_TABLE_6_BITS)
# if defined(VAVOOM_RGB_TABLE_7_BITS) || defined(VAVOOM_RGB_TABLE_5_BITS)
#  error "choose only one RGB table size"
# endif
static inline vuint8 VVA_OKUNUSED R_LookupRGB (vint32 r, vint32 g, vint32 b) {
  return r_rgbtable[(((vuint32)clampToByte(r)<<10)&0x3f000U)|(((vuint32)clampToByte(g)<<4)&0xfc0U)|((clampToByte(b)>>2)&0x3fU)];
}
#elif defined(VAVOOM_RGB_TABLE_5_BITS)
# if defined(VAVOOM_RGB_TABLE_6_BITS) || defined(VAVOOM_RGB_TABLE_7_BITS)
#  error "choose only one RGB table size"
# endif
static inline vuint8 VVA_OKUNUSED R_LookupRGB (vint32 r, vint32 g, vint32 b) {
  return r_rgbtable[(((vuint32)clampToByte(r)<<7)&0x7c00U)|(((vuint32)clampToByte(g)<<2)&0x3e0U)|((clampToByte(b)>>3)&0x1fU)];
}
#else
#  error "choose RGB table size"
#endif
