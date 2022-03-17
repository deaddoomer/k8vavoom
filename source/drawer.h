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
//**  Copyright (C) 2018-2022 Ketmar Dark
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
#ifndef DRAWER_HEADER
#define DRAWER_HEADER

#include "render/r_public.h"


// ////////////////////////////////////////////////////////////////////////// //
struct surface_t;
struct surfcache_t; // see "render/r_shared.h"
struct VMeshModel;
class VPortal;

extern VCvarF gl_maxdist;
extern VCvarF r_lights_radius;


// ////////////////////////////////////////////////////////////////////////// //
// TVec is not packed too
// WARNING! make sure that our union is the same as TVec!
// /*__attribute__((packed))*/
struct SurfVertex {
  union {
    TVec v;
    struct {
      float x, y, z;
    };
  };
  // `owner` will be set in lightmap t-junction fixer
  // it can be `nullptr` if this is the original point
  // (or added from the subsector that owns the surface)
  // used to remove points we don't need anymore before creating new ones
  //subsector_t *ownersub; // if set, this vertex was added from flat t-junction fixer
  sec_surface_t *ownerssf; // if set, this vertex was added from flat t-junction fixer
  seg_t *ownerseg; // if set, this vertex was added from wall t-junction fixer

  inline SurfVertex () noexcept {}
  inline ~SurfVertex () noexcept {}

  inline const TVec &vec () const noexcept { return v; }
  inline const TVec *vecptr () const noexcept { return &v; }

  inline void setVec (const float ax, const float ay, const float az) noexcept { x = ax; y = ay; z = az; }
  inline void setVec (const TVec &av) noexcept { x = av.x; y = av.y; z = av.z; }

  inline void clearSetVec (const TVec &av) noexcept { x = av.x; y = av.y; z = av.z; ownerssf = nullptr; ownerseg = nullptr; }
};
//static_assert(sizeof(SurfVertex) == sizeof(float)*3+sizeof(subsector_t *), "invalid SurfVertex size");


// ////////////////////////////////////////////////////////////////////////// //
struct particle_t {
  // drawing info
  TVec org; // position
  vuint32 color; // ARGB color
  float Size;
  // handled by refresh
  particle_t *next; // next in the list
  TVec vel; // velocity
  TVec accel; // acceleration
  float die; // cl.time when particle will be removed
  vint32 type;
  float ramp;
  float gravity;
  float dur; // for pt_fading
};


// ////////////////////////////////////////////////////////////////////////// //
//TODO: add profiler, check if several dirty rects are better
struct VDirtyArea {
public:
  int x0, y0;
  int x1, y1; // exclusive

public:
  inline VDirtyArea () noexcept : x0(0), y0(0), x1(0), y1(0) {}
  inline void clear () noexcept { x0 = y0 = x1 = y1 = 0; }
  inline bool isValid () const noexcept { return (x1-x0 > 0 && y1-y0 > 0); }
  inline int getWidth () const noexcept { return x1-x0; }
  inline int getHeight () const noexcept { return y1-y0; }
  inline void addDirty (int ax0, int ay0, int awdt, int ahgt) noexcept {
    if (awdt < 1 || ahgt < 1) return;
    if (isValid()) {
      x0 = min2(x0, ax0);
      y0 = min2(y0, ay0);
      x1 = max2(x1, ax0+awdt);
      y1 = max2(y1, ay0+ahgt);
    } else {
      x0 = ax0;
      y0 = ay0;
      x1 = ax0+awdt;
      y1 = ay0+ahgt;
    }
  }
};


// ////////////////////////////////////////////////////////////////////////// //
struct trans_sprite_t;

// used to pass render style parameters
struct RenderStyleInfo {
  enum {
    Subtractive = -1,
    Normal = 0,
    Translucent = 1,
    Additive = 2,
    DarkTrans = 3, // k8vavoom special
    Shaded = 4, // translucent, shaded; `stencilColor` is shade color
    AddShaded = 5, // additive, shaded; `stencilColor` is shade color
    Fuzzy = 6,
  };

  enum {
    FlagNoDepthWrite    = 1u<<0, // no z-buffer write
    FlagOffset          = 1u<<1, // do offsetting (used for flat-aligned sprites)
    FlagNoCull          = 1u<<2, // don't cull faces
    FlagOnlyTranslucent = 1u<<3, // draw only non-solid pixels? (used for translucent textures)

    FlagOptionsMask  = 0xffu,

    FlagFlat     = 1u<<8, // flat sprite
    FlagWall     = 1u<<9, // wall sprite
    FlagOriented = 1u<<10, // oriented sprite
    FlagShadow   = 1u<<11, // shadow sprite
    // used in sorter/renderer
    FlagMirror   = 1u<<12, // mirror surface; put here to avoid checking if something is a surface or a sprite
    FlagCeiling  = 1u<<13, // this is ceiling surface (used in sorter)
    FlagFloor    = 1u<<13, // this is floor surface (used in sorter)
  };

  vuint32 seclight; // not used by hw renderer, but used by high-level renderer
  // hw renderer options
  vuint32 light; // high byte is intensity for colored light; color is multiplied by this
  vuint32 fade;
  vuint32 stencilColor; // if high byte is 0, this is not a stenciled sprite
  vint32 translucency; // see enum above
  float alpha; // should be valid, and non-zero
  unsigned flags;

  inline RenderStyleInfo () noexcept : seclight(0u), light(0u), fade(0u), stencilColor(0u), translucency(0), alpha(1.0f), flags(0u) {}

  inline void setOnlyTranslucent () noexcept { flags |= FlagOnlyTranslucent; }

  inline bool isAdditive () const noexcept { return (translucency == Additive || translucency == AddShaded || translucency == Subtractive); }
  inline bool isTranslucent () const noexcept { return (translucency || alpha < 1.0f); }
  inline bool isShaded () const noexcept { return (translucency == Shaded || translucency == AddShaded); }
  // this is how "shadow" render style is rendered
  inline bool isShadow () const noexcept { return (translucency == Translucent && stencilColor == 0xff000000u); }
  inline bool isFuzzy () const noexcept { return (translucency == Fuzzy); }
  inline bool isStenciled () const noexcept { return (stencilColor && translucency != Shaded && translucency != AddShaded); }

  const char *toCString () const noexcept { return va("light=0x%08x; fade=0x%08x; stc=0x%08x; trans=%d; alpha=%g; flags=0x%04x", light, fade, stencilColor, translucency, alpha, flags); }
};


// ////////////////////////////////////////////////////////////////////////// //
class VRenderLevelDrawer : public VRenderLevelPublic {
protected:
  bool mIsShadowVolumeRenderer;

public:
  enum {
    TSP_Wall,
    TSP_Sprite,
    TSP_Model,
    TSP_ModelGlass,
  };

  struct trans_sprite_t {
    /*SurfVertex*/TVec Verts[4]; // only for sprites
    union {
      surface_t *surf; // for masked polys
      VEntity *ent; // for sprites and alias models
    };
    int prio; // for things
    int lump; // basically, has any sense only for sprites, has no sense for alias models
    TVec normal; // not set for alias models
    TVec origin; // not set for walls
    union {
      float pdist; // masked polys and sprites
      float TimeFrac; // alias models
    };
    TVec saxis; // masked polys and sprites
    TVec taxis; // masked polys and sprites
    TVec texorg; // masked polys and sprites
    int translation; // masked polys and sprites
    int type; // TSP_xxx -- 0: masked polygon (wall); 1: sprite; 2: alias model
    float dist; // for soriting
    vuint32 objid; // for entities
    RenderStyleInfo rstyle;

    // no need to setup this
    inline trans_sprite_t () noexcept {}
    inline trans_sprite_t (ENoInit) noexcept {}
    inline ~trans_sprite_t () noexcept {}
  };

  struct DrawLists {
    // render lists; various queue functions will put surfaces there
    // those arrays are never cleared, only reset
    // each surface is marked with `currQueueFrame`
    // note that there is no overflow protection, so don't leave
    // the game running one level for weeks ;-)
    TArrayNC<surface_t *> DrawSurfListSolid; // solid surfaces
    TArrayNC<surface_t *> DrawSurfListMasked; // masked surfaces
    TArrayNC<surface_t *> DrawSkyList;
    TArrayNC<surface_t *> DrawHorizonList;

    TArrayNC<trans_sprite_t> DrawSurfListAlpha; // alpha-blended surfaces
    TArrayNC<trans_sprite_t> DrawSpriListAlpha; // alpha-blended sprites
    TArrayNC<trans_sprite_t> DrawSpriteShadowsList; // sprite shadows
    TArrayNC<trans_sprite_t> DrawSpriteList; // non-translucent sprites

    inline void resetAll () {
      DrawSurfListSolid.reset();
      DrawSurfListMasked.reset();
      DrawSkyList.reset();
      DrawHorizonList.reset();
      DrawSurfListAlpha.reset();
      DrawSpriListAlpha.reset();
      DrawSpriteShadowsList.reset();
      DrawSpriteList.reset();
    }
  };

public:
  TArray<DrawLists> DrawListStack;

  int PortalDepth;
  int PortalUsingStencil;
  vuint32 currDLightFrame;
  vuint32 currQueueFrame;

public:
  inline DrawLists &GetCurrentDLS () noexcept { return DrawListStack[DrawListStack.length()-1]; }

  // WARNING! call *ONLY* for surfaces
  inline trans_sprite_t *AllocTransSurface (const RenderStyleInfo &ri) noexcept {
    if (ri.flags&RenderStyleInfo::FlagShadow) {
      // just in case
      return &GetCurrentDLS().DrawSpriteShadowsList.alloc();
    } else if (ri.alpha < 1.0f || ri.isTranslucent()) {
      return &GetCurrentDLS().DrawSurfListAlpha.alloc();
    } else {
      // just in case
      return &GetCurrentDLS().DrawSpriteList.alloc();
    }
  }

  // WARNING! call *ONLY* for sprites (and models)
  inline trans_sprite_t *AllocTransSprite (const RenderStyleInfo &ri) noexcept {
    if (ri.flags&RenderStyleInfo::FlagShadow) {
      return &GetCurrentDLS().DrawSpriteShadowsList.alloc();
    } else if (ri.alpha < 1.0f || ri.isTranslucent()) {
      return &GetCurrentDLS().DrawSpriListAlpha.alloc();
    } else {
      return &GetCurrentDLS().DrawSpriteList.alloc();
    }
  }

  // should be called before rendering a frame
  // (i.e. in initial render, `RenderPlayerView()`)
  // creates 0th element of the stack
  void ResetDrawStack ();

  void PushDrawLists ();
  void PopDrawLists ();

  struct DrawListStackMark {
  public:
    VRenderLevelDrawer *owner;
    int level;
  public:
    VV_DISABLE_COPY(DrawListStackMark)
    inline DrawListStackMark (VRenderLevelDrawer *aowner) noexcept : owner(aowner), level(aowner->DrawListStack.length()) {
      vassert(owner);
      vassert(level > 0);
    }
    inline ~DrawListStackMark () {
      vassert(owner);
      vassert(level > 0);
      vassert(owner->DrawListStack.length() >= level);
      while (owner->DrawListStack.length() > level) owner->PopDrawLists();
      owner = nullptr;
      level = 0;
    }
  };

public:
  VRenderLevelDrawer (VLevel *ALevel) noexcept;

  // lightmap chain iterator (used in renderer)
  // block number+1 or 0
  virtual vuint32 GetLightChainHead () = 0;
  // block number+1 or 0
  virtual vuint32 GetLightChainNext (vuint32 bnum) = 0;

  virtual VDirtyArea &GetLightBlockDirtyArea (vuint32 bnum) = 0;
  virtual rgba_t *GetLightBlock (vuint32 bnum) = 0;
  virtual surfcache_t *GetLightChainFirst (vuint32 bnum) = 0;

  virtual void BuildLightMap (surface_t *) = 0;

  virtual void UpdateSubsectorFlatSurfaces (subsector_t *sub, bool dofloors, bool doceils, bool forced=false) = 0;

  inline bool IsShadowVolumeRenderer () const noexcept { return mIsShadowVolumeRenderer; }
  // shadowmap renderer is a part of a shadow volume renderer (sorry, this is ugly hack, i know)
  virtual bool IsShadowMapRenderer () const noexcept = 0;

  virtual bool IsNodeRendered (const node_t *node) const noexcept = 0;
  virtual bool IsSubsectorRendered (const subsector_t *sub) const noexcept = 0;

  virtual void PrecacheLevel () = 0;
  virtual void UncacheLevel () = 0;

public: // automap
  typedef bool (*AMCheckSubsectorCB) (const subsector_t *sub);
  typedef bool (*AMIsHiddenSubsectorCB) (const subsector_t *sub);
  typedef void (*AMMapXYtoFBXYCB) (float *destx, float *desty, float x, float y);

  virtual void RenderTexturedAutomap (
    // coords to check BSP bbox visibility
    float m_x, float m_y, float m_x2, float m_y2,
    bool doFloors, // floors or ceiling?
    float alpha,
    // returns `true` if this subsector should be rendered
    AMCheckSubsectorCB CheckSubsector,
    // returns `true` if this subsector is hidden (called only for subsectors that passed the previous check)
    AMIsHiddenSubsectorCB IsHiddenSubsector,
    // converts map coords to framebuffer coords
    // passed as callback, so automap can do its business with scale, rotation, and such
    AMMapXYtoFBXYCB MapXYtoFBXY
  ) = 0;

public:
  static bool CalculateRenderStyleInfo (RenderStyleInfo &ri, int RenderStyle, float Alpha, vuint32 StencilColor=0) noexcept;

  virtual void RenderCrosshair () = 0;

  #if 0
  static inline bool IsAdditiveStyle (int style) {
    switch (style) {
      case STYLE_AddStencil:
      case STYLE_Add:
      case STYLE_Subtract:
      case STYLE_AddShaded:
        return true;
      default: break;
    }
    return false;
  }

  static inline int CoerceRenderStyle (int style) {
    switch (style) {
      case STYLE_None:
      case STYLE_Normal:
      case STYLE_Fuzzy:
      case STYLE_SoulTrans:
      case STYLE_OptFuzzy:
      case STYLE_Stencil:
      case STYLE_AddStencil:
      case STYLE_Translucent:
      case STYLE_Add:
      case STYLE_Dark:
        return style;
      case STYLE_Shaded: return STYLE_Stencil; // not implemented
      case STYLE_TranslucentStencil: return STYLE_Translucent; // not implemented
      case STYLE_Shadow: return /*STYLE_Translucent*/STYLE_Fuzzy; // not implemented
      case STYLE_Subtract: return STYLE_Add; // not implemented
      case STYLE_AddShaded: return STYLE_Add; // not implemented
      default: break;
    }
    return STYLE_Normal;
  }
  #endif

  // limit light distance with both `r_lights_radius` and `gl_maxdist`
  static inline float GetLightMaxDist () noexcept {
    return fmax(0.0f, (gl_maxdist > 0.0f ? fmin(gl_maxdist.asFloat(), r_lights_radius.asFloat()) : r_lights_radius.asFloat()));
  }

  static inline float GetLightMaxDistDef (const float defval=4096.0f) noexcept {
    const float maxLightDist = (gl_maxdist > 0.0f ? fmin(gl_maxdist.asFloat(), r_lights_radius.asFloat()) : r_lights_radius.asFloat());
    return (maxLightDist < 8.0f ? defval : maxLightDist);
  }
};


// ////////////////////////////////////////////////////////////////////////// //
struct texinfo_t;
class VMeshModel;

struct AliasModelTrans {
  // transformation matrix
  VMatrix4 MatTrans;
  TAVec TransRot; // extracted from `MatTrans`, for normals
  // for frame interpolator; we can precalc this
  VMatrix4Decomposed DecTrans;
  // gzdoom specifics
  bool gzdoom;
  TVec gzScale; // gzdoom model scale
  TVec gzPreScaleOfs; // gzdoom pre-scale offset
  // model rotation center (shift by this, rotate, shift back)
  bool userotcenter;
  TVec RotCenter;

  inline AliasModelTrans () noexcept
    : MatTrans(VMatrix4::Identity)
    , TransRot(0.0f, 0.0f, 0.0f)
    , DecTrans()
    , gzdoom(false)
    , gzScale(1.0f, 1.0f, 1.0f)
    , gzPreScaleOfs(0.0f, 0.0f, 0.0f)
    , userotcenter(false)
    , RotCenter(0.0f, 0.0f, 0.0f)
  {}

  inline void decompose () noexcept {
    MatTrans.decompose(DecTrans);
    TransRot = MatTrans.getAngles();
    gzdoom = (gzScale != TVec(1.0f, 1.0f, 1.0f) || gzPreScaleOfs != TVec::ZeroVector);
    userotcenter = (RotCenter != TVec::ZeroVector);
  }
};


class VDrawer {
public:
  enum {
    VCB_InitVideo,
    VCB_DeinitVideo,
    VCB_InitResolution,
    VCB_DeinitResolution, //FIXME: not implemented yet
    VCB_FinishUpdate,
  };

  enum {
    CLEAR_COLOR = 1u<<0,
    CLEAR_DEPTH = 1u<<1,
    CLEAR_STENCIL = 1u<<2,
    CLEAR_ALL = (CLEAR_COLOR|CLEAR_DEPTH|CLEAR_STENCIL),
  };

protected:
  bool mInitialized;
  bool isShittyGPU;
  bool shittyGPUCheckDone;
  bool useReverseZ;
  bool HaveDepthClamp;
  bool DepthZeroOne; // use [0..1] depth instead of [-1..1]? (only matters for "normal z", "reverse z" always does this)
  bool canRenderShadowmaps;

  vuint32 updateFrame; // counter

  // moved here, so we can fix them with FBO change
  int ScrWdt, ScrHgt;

  /* set after creating the main window */
  static float mWindowAspect;

  static TArray<void (*) (int phase)> cbInitDeinit;

  static void callICB (int phase);

public:
  VViewportMats vpmats;
  // the following is here because alot of code can access it
  // current rendering position and angles
  TVec vieworg;
  TAVec viewangles;
  // calculated from `viewangles`, normalised
  TVec viewforward;
  TVec viewright;
  TVec viewup;
  // calculated from `vieworg` and `viewangles`
  TFrustum viewfrustum;
  bool MirrorFlip;
  bool MirrorClip;
  TPlane MirrorPlane; // active is `MirrorClip` is true
  static float LightFadeMult;

  // so we can draw it last
  bool needCrosshair;
  int prevCrosshairPic;

  // used in texture GC code
  // update on each new frame
  double CurrentTime;

public:
  static void RegisterICB (void (*cb) (int phase));

public:
  inline void ResetCrosshair () noexcept { needCrosshair = false; }
  inline void WantCrosshair () noexcept { needCrosshair = true; }
  // draws nothing if `needCrosshair` is false; resets flag
  void DrawCrosshair ();

/*
protected:
  virtual void PushDepthMask () = 0;
  virtual void PopDepthMask () = 0;
*/

public:
  VRenderLevelDrawer *RendLev;

  // we can precalc these in ctor (and we'll need 'em in various places)
  // note that they are untranslated
  static VMatrix4 LightViewMatrix[6];

public:
  VDrawer () noexcept;
  virtual ~VDrawer ();

  static inline float GetWindowAspect () noexcept { return mWindowAspect; }

  inline bool CanUseRevZ () const noexcept { return useReverseZ; }
  inline bool IsShittyGPU () const noexcept { return isShittyGPU; }
  inline bool IsInited () const noexcept { return mInitialized; }
  inline bool CanUseDepthClamp () const noexcept { return HaveDepthClamp; }
  inline bool IsDepthZeroOne () const noexcept { return DepthZeroOne; }

  inline bool CanRenderShadowMaps () const noexcept { return canRenderShadowmaps; }

  inline int getWidth () const noexcept { return ScrWdt; }
  inline int getHeight () const noexcept { return ScrHgt; }

  inline void SetUpdateFrame (vuint32 n) noexcept { updateFrame = n; }
  inline vuint32 GetUpdateFrame () const noexcept { return updateFrame; }
  inline void IncUpdateFrame () noexcept { if (++updateFrame == 0) updateFrame = 1; CurrentTime = Sys_Time(); }
  void ResetTextureUpdateFrames () noexcept;

  // this should show some kind of splash screen
  // it is called very early in startup sequence
  // the drawer should destroy splash screen on first resolution change
  // should return `false` splash screen feature is not implemented
  virtual bool ShowLoadingSplashScreen () = 0;
  virtual bool IsLoadingSplashActive () = 0;
  // this can be called regardless of splash screen availability, and even after splash was hidden
  virtual void DrawLoadingSplashText (const char *text, int len=-1) = 0;
  // this is called to hide all splash screens
  // note that it can be called many times, and
  // even if splash screen feature is not avilable
  virtual void HideSplashScreens () = 0;

  virtual void Init () = 0;
  // fsmode: 0: windowed; 1: scaled FS; 2: real FS
  virtual bool SetResolution (int AWidth, int AHeight, int fsmode) = 0;
  virtual void InitResolution () = 0;
  virtual void DeinitResolution () = 0;

  virtual void UnloadAliasModel (VMeshModel *Mdl) = 0;

  // recreate FBOs with fp/int formats (alpha is not guaranteed)
  // used for overbright
  virtual bool RecreateFBOs (bool wantFP) = 0;
  virtual bool IsMainFBOFloat () = 0;
  virtual bool IsCameraFBO () = 0;

  virtual void PrepareMainFBO () = 0;
  virtual void RestoreMainFBO () = 0;

  // normalize overbrighting for fp textures
  virtual void PostprocessOvebright () = 0;

  virtual void EnableBlend () = 0;
  virtual void DisableBlend () = 0;
  virtual void SetBlendEnabled (const bool v) = 0;

  virtual void Posteffect_Bloom (int ax, int ay, int awidth, int aheight) = 0;
  virtual void Posteffect_Tonemap (int ax, int ay, int awidth, int aheight, bool restoreMatrices) = 0;
  virtual void Posteffect_ColorMap (int cmap, int ax, int ay, int awidth, int aheight) = 0;
  virtual void Posteffect_Underwater (float time, int ax, int ay, int awidth, int aheight, bool restoreMatrices) = 0;
  virtual void Posteffect_CAS (float coeff, int ax, int ay, int awidth, int aheight, bool restoreMatrices) = 0;

  // contrary to the name, this must be called at the very beginning of the rendering pass, right after setting the matices
  virtual void PrepareForPosteffects () = 0;
  // call this after finishing all posteffects, to restore main FBO in the proper state
  // this is required because posteffects can attach non-FP texture to FBO
  virtual void FinishPosteffects () = 0;

  // the following posteffects should be called after the whole screen was rendered!
  virtual void Posteffect_ColorBlind (int mode) = 0;
  virtual void Posteffect_ColorMatrix (const float mat[12]) = 0;

  virtual void ApplyFullscreenPosteffects () = 0;
  virtual void FinishFullscreenPosteffects () = 0;

  // can be called several times
  virtual void LevelRendererCreated (VRenderLevelPublic *Renderer) = 0;
  // can be called several times in a row (i.e. one creation may lead to many shutdowns)
  virtual void LevelRendererDestroyed () = 0;

  virtual void StartUpdate () = 0;
  virtual void ClearScreen (unsigned clearFlags=CLEAR_COLOR) = 0;
  virtual void Setup2D () = 0;
  virtual void Update (bool fullUpdate=true) = 0;
  virtual void Shutdown () = 0;
  virtual void *ReadScreen (int *bpp, bool *bot2top) = 0;
  virtual void ReadBackScreen (int Width, int Height, rgba_t *Dest) = 0;
  virtual void WarpMouseToWindowCenter () = 0;
  virtual void GetMousePosition (int *mx, int *my) = 0;

  // rendring stuff
  virtual bool UseFrustumFarClip () = 0;
  virtual void SetupView (VRenderLevelDrawer *ARLev, const refdef_t *rd) = 0;
  virtual void SetupViewOrg () = 0;
  virtual void EndView () = 0;
  virtual void RenderTint (vuint32 CShift) = 0;

  // you can populate VBOs here
  virtual void BeforeDrawWorldLMap () = 0;
  virtual void BeforeDrawWorldSV () = 0;

  // lightmapped renderer
  virtual void DrawLightmapWorld () = 0;

  virtual void SetMainFBO (bool forced=false) = 0;

  virtual void ClearCameraFBOs () = 0;
  virtual int GetCameraFBO (int texnum, int width, int height) = 0; // returns index or -1; (re)creates FBO if necessary
  virtual int FindCameraFBO (int texnum) = 0; // returns index or -1
  virtual void SetCameraFBO (int cfboindex) = 0;
  virtual GLuint GetCameraFBOTextureId (int cfboindex) = 0; // returns 0 if cfboindex is invalid

  // this copies main FBO to wipe FBO, so we can run wipe shader
  virtual void PrepareWipe () = 0;
  // render wipe from wipe to main FBO
  // should be called after `StartUpdate()`
  // and (possibly) rendering something to the main FBO
  // time is in seconds, from zero to...
  // returns `false` if wipe is complete
  // -1 means "show saved wipe screen"
  virtual bool RenderWipe (float time) = 0;

  // renderer require this
  virtual void GLEnableOffset () = 0;
  virtual void GLDisableOffset () = 0;
  // this also enables it if it was disabled
  virtual void GLPolygonOffset (const float afactor, const float aunits) = 0;

  // this takes into account `CanUseRevZ()`
  inline void GLPolygonOffsetEx (const float afactor, const float aunits) {
    if (CanUseRevZ()) GLPolygonOffset(afactor, aunits); else GLPolygonOffset(-afactor, -aunits);
  }

  enum SpriteType {
    SP_Normal,
    SP_PSprite,
  };

  // texture stuff
  virtual void PrecacheTexture (VTexture *Tex) = 0;
  virtual void PrecacheSpriteTexture (VTexture *Tex, SpriteType sptype) = 0;
  virtual void FlushOneTexture (VTexture *tex, bool forced=false) = 0; // unload one texture
  virtual void FlushTextures (bool forced=false) = 0; // unload all textures

  // polygon drawing
  virtual void StartSkyPolygons () = 0;
  virtual void EndSkyPolygons () = 0;
  virtual void DrawSkyPolygon (surface_t *surf, bool bIsSkyBox, VTexture *Texture1,
                               float offs1, VTexture *Texture2, float offs2, int CMap) = 0;

  virtual void DrawMaskedPolygon (surface_t *surf, float Alpha, bool Additive, bool DepthWrite, bool onlyTranslucent) = 0;

  //virtual void BeginTranslucentPolygonDecals () = 0;
  //virtual void DrawTranslucentPolygonDecals (surface_t *surf, float Alpha, bool Additive) = 0;

  virtual void DrawSpritePolygon (float time, const TVec *cv, VTexture *Tex,
                                  const RenderStyleInfo &ri,
                                  VTextureTranslation *Translation, int CMap,
                                  const TVec &sprnormal, float sprpdist,
                                  const TVec &saxis, const TVec &taxis, const TVec &texorg,
                                  SpriteType sptype=SP_Normal) = 0;

  virtual void DrawAliasModel (const TVec &origin, const TAVec &angles, const AliasModelTrans &Transform,
                               VMeshModel *Mdl, int frame, int nextframe, VTexture *Skin, VTextureTranslation *Trans,
                               int CMap, const RenderStyleInfo &ri, bool is_view_model,
                               float Inter, bool Interpolate, bool ForceDepthUse, bool AllowTransparency,
                               bool onlyDepth) = 0;

  virtual void DrawSpriteShadowMap (const TVec *cv, VTexture *Tex, const TVec &sprnormal,
                                    const TVec &saxis, const TVec &taxis, const TVec &texorg) = 0;

  virtual bool StartPortal (VPortal *Portal, bool UseStencil) = 0;
  virtual void EndPortal (VPortal *Portal, bool UseStencil) = 0;

  virtual void ForceClearStencilBuffer () = 0;
  virtual void ForceMarkStencilBufferDirty () = 0;

  //  particles
  virtual void StartParticles () = 0;
  virtual void DrawParticle (particle_t *) = 0;
  virtual void EndParticles () = 0;

  // drawing
  virtual void DrawPic (float x1, float y1, float x2, float y2,
                        float s1, float t1, float s2, float t2,
                        VTexture *Tex, VTextureTranslation *Trans, float Alpha) = 0;
  // recolor picture using intensity
  virtual void DrawPicRecolored (float x1, float y1, float x2, float y2,
                                 float s1, float t1, float s2, float t2,
                                 VTexture *Tex, int rgbcolor, float Alpha) = 0;
  virtual void DrawPicShadow (float x1, float y1, float x2, float y2,
                              float s1, float t1, float s2, float t2,
                              VTexture *Tex, float shade) = 0;
  virtual void FillRectWithFlat (float x1, float y1, float x2, float y2,
                                 float s1, float t1, float s2, float t2, VTexture *Tex) = 0;
  virtual void FillRectWithFlatRepeat (float x1, float y1, float x2, float y2,
                                       float s1, float t1, float s2, float t2, VTexture *Tex) = 0;
  virtual void FillRect (float x1, float y1, float x2, float y2, vuint32 color, float alpha=1.0f) = 0;
  virtual void DrawRect (float x1, float y1, float x2, float y2, vuint32 color, float alpha=1.0f) = 0;
  virtual void ShadeRect (float x1, float y1, float x2, float y2, float darkening) = 0;
  // last pixel is not set
  virtual void DrawLine (int x1, int y1, int x2, int y2, vuint32 color, float alpha=1.0f) = 0;
  virtual void DrawConsoleBackground (int h) = 0;
  virtual void DrawSpriteLump (float x1, float y1, float x2, float y2,
                               VTexture *Tex, VTextureTranslation *Translation, bool flip) = 0;

  static float CalcRealHexHeight (float h) noexcept;
  static void CalcHexVertices (float vx[6], float vy[6], float x0, float y0, float w, float h) noexcept;
  static bool IsPointInsideHex (float x, float y, float x0, float y0, float w, float h) noexcept;

  // polygon must be convex
  // `stride` is in number of floats
  static bool IsPointInside2DPolyInternal (const float x, const float y, int vcount, const float vx[], size_t xstride, const float vy[], size_t ystride) noexcept;
  // polygon must be convex
  static bool IsPointInside2DPoly (const float x, const float y, int vcount, const float vx[], const float vy[]) noexcept;
  // polygon must be convex
  // here `vxy` contains `x`,`y` pairs
  static bool IsPointInside2DPoly (const float x, const float y, int vcount, const float vxy[]) noexcept;

  virtual void DrawHex (float x0, float y0, float w, float h, vuint32 color, float alpha=1.0f) = 0;
  virtual void FillHex (float x0, float y0, float w, float h, vuint32 color, float alpha=1.0f) = 0;
  virtual void ShadeHex (float x0, float y0, float w, float h, float darkening) = 0;

  virtual void BeginTexturedPolys () = 0;
  virtual void EndTexturedPolys () = 0;
  virtual void DrawTexturedPoly (const texinfo_t *tinfo, TVec light, float alpha, int vcount, const TVec *verts, const SurfVertex *origverts=nullptr) = 0;

  // automap
  virtual void StartAutomap (bool asOverlay) = 0;
  virtual void DrawLineAM (float x1, float y1, vuint32 c1, float x2, float y2, vuint32 c2) = 0;
  virtual void EndAutomap () = 0;

  // advanced drawing
  virtual bool SupportsShadowVolumeRendering () = 0;
  virtual bool SupportsShadowMapRendering () = 0;

  virtual void GLDisableDepthWriteSlow () noexcept = 0;
  virtual void PushDepthMaskSlow () noexcept = 0;
  virtual void PopDepthMaskSlow () noexcept = 0;

  virtual void GLDisableDepthTestSlow () noexcept = 0;
  virtual void GLEnableDepthTestSlow () noexcept = 0;

  virtual void DrawWorldAmbientPass () = 0;
  virtual void BeginShadowVolumesPass () = 0;
  virtual void BeginLightShadowVolumes (const TVec &LightPos, const float Radius, bool useZPass, bool hasScissor, const int scoords[4], const refdef_t *rd) = 0;
  virtual void EndLightShadowVolumes () = 0;
  virtual void RenderSurfaceShadowVolume (VLevel *Level, const surface_t *surf, const TVec &LightPos, float Radius) = 0;

  virtual void BeginLightShadowMaps (const TVec &LightPos, const float Radius) = 0;
  virtual void EndLightShadowMaps () = 0;
  virtual void SetupLightShadowMap (unsigned int facenum) = 0;

  //virtual void DrawSurfaceShadowMap (const surface_t *surf) = 0;
  // may modify lists (sort)
  virtual void UploadShadowSurfaces (TArrayNC<surface_t *> &solid, TArrayNC<surface_t *> &masked) = 0;
  virtual void RenderShadowMaps (TArrayNC<surface_t *> &solid, TArrayNC<surface_t *> &masked) = 0;

  virtual void BeginLightPass (const TVec &LightPos, const float Radius, float LightMin, vuint32 Color, const bool aspotLight, const TVec &aconeDir, const float aconeAngle, bool doShadow) = 0;
  virtual void EndLightPass () = 0;

  //virtual void DrawSurfaceLight (surface_t *Surf) = 0;
  virtual void RenderSolidLightSurfaces (TArrayNC<surface_t *> &slist) = 0;
  virtual void RenderMaskedLightSurfaces (TArrayNC<surface_t *> &slist) = 0;

  virtual void DrawWorldTexturesPass () = 0;
  virtual void DrawWorldFogPass () = 0;
  virtual void EndFogPass () = 0;

  virtual void BeginModelsAmbientPass () = 0;
  virtual void EndModelsAmbientPass () = 0;
  virtual void DrawAliasModelAmbient (const TVec &origin, const TAVec &angles,
                                      const AliasModelTrans &Transform,
                                      VMeshModel *Mdl, int frame, int nextframe,
                                      VTexture *Skin, vuint32 light, float Alpha,
                                      float Inter, bool Interpolate,
                                      bool ForceDepth, bool AllowTransparency) = 0;

  virtual void BeginModelShadowMaps (const TVec &LightPos, const float Radius) = 0;
  virtual void EndModelShadowMaps () = 0;
  virtual void SetupModelShadowMap (unsigned int facenum) = 0;
  virtual void DrawAliasModelShadowMap (const TVec &origin, const TAVec &angles,
                                        const AliasModelTrans &Transform,
                                        VMeshModel *Mdl, int frame, int nextframe,
                                        VTexture *Skin, float Alpha, float Inter,
                                        bool Interpolate, bool AllowTransparency) = 0;

  virtual void BeginModelsLightPass (const TVec &LightPos, float Radius, float LightMin, vuint32 Color, const bool aspotLight, const TVec &aconeDir, const float aconeAngle, bool doShadow) = 0;
  virtual void EndModelsLightPass () = 0;
  virtual void DrawAliasModelLight (const TVec &origin, const TAVec &angles,
                                    const AliasModelTrans &Transform,
                                    VMeshModel *Mdl, int frame, int nextframe,
                                    VTexture *Skin, float Alpha, float Inter,
                                    bool Interpolate, bool AllowTransparency) = 0;

  virtual void BeginModelsShadowsPass (TVec &LightPos, float LightRadius) = 0;
  virtual void EndModelsShadowsPass () = 0;
  virtual void DrawAliasModelShadow (const TVec &origin, const TAVec &angles,
                                     const AliasModelTrans &Transform,
                                     VMeshModel *Mdl, int frame, int nextframe,
                                     float Inter, bool Interpolate,
                                     const TVec &LightPos, float LightRadius) = 0;

  virtual void BeginModelsTexturesPass () = 0;
  virtual void EndModelsTexturesPass () = 0;
  virtual void DrawAliasModelTextures (const TVec &origin, const TAVec &angles,
                                       const AliasModelTrans &Transform,
                                       VMeshModel *Mdl, int frame, int nextframe,
                                       VTexture *Skin, VTextureTranslation *Trans,
                                       int CMap, const RenderStyleInfo &ri, float Inter,
                                       bool Interpolate, bool ForceDepth, bool AllowTransparency) = 0;

  virtual void BeginModelsFogPass () = 0;
  virtual void EndModelsFogPass () = 0;
  virtual void DrawAliasModelFog (const TVec &origin, const TAVec &angles,
                                  const AliasModelTrans &Transform,
                                  VMeshModel *Mdl, int frame, int nextframe,
                                  VTexture *Skin, vuint32 Fade, float Alpha, float Inter,
                                  bool Interpolate, bool AllowTransparency) = 0;

  virtual void GetRealWindowSize (int *rw, int *rh) = 0;

  virtual void GetProjectionMatrix (VMatrix4 &mat) = 0;
  virtual void GetModelMatrix (VMatrix4 &mat) = 0;
  virtual void SetProjectionMatrix (const VMatrix4 &mat) = 0;
  virtual void SetModelMatrix (const VMatrix4 &mat) = 0;

  void CalcProjectionMatrix (VMatrix4 &ProjMat, /*VRenderLevelDrawer *ARLev,*/ const refdef_t *rd, bool noFarNearPlanes=false);
  void CalcModelMatrix (VMatrix4 &ModelMat, const TVec &origin, const TAVec &angles, bool MirrorFlip=false);
  void CalcOrthoMatrix (VMatrix4 &OrthoMat, const float left, const float right, const float bottom, const float top);

  void CalcSpotLightFaceView (VMatrix4 &ModelMat, const TVec &origin, unsigned int facenum);
  void CalcShadowMapProjectionMatrix (VMatrix4 &ProjMat, float Radius);

  static TVec GetLightViewUp (unsigned int facenum) noexcept;
  static TVec GetLightViewForward (unsigned int facenum) noexcept;
  static TVec GetLightViewRight (unsigned int facenum) noexcept;

  void SetOrthoProjection (const float left, const float right, const float bottom, const float top);

  // call this before doing light scissor calculations (can be called once per scene)
  // sets `vpmats`
  // scissor setup will use those matrices (but won't modify them)
  // also, `SetupViewOrg()` automatically call this method
  //virtual void LoadVPMatrices () = 0;
  // no need to do this:
  //   projection matrix and viewport set in `SetupView()`
  //   model matrix set in `SetupViewOrg()`

  // returns 0 if scissor has no sense; -1 if scissor is empty, and 1 if scissor is set
  virtual int SetupLightScissor (const TVec &org, float radius, int scoord[4], const TVec *geobbox=nullptr) = 0;
  virtual void ResetScissor () = 0;

  virtual void DebugRenderScreenRect (int x0, int y0, int x1, int y1, vuint32 color) = 0;

  virtual void SetScissorEnabled (bool v) = 0;
  virtual bool IsScissorEnabled () = 0;

  // scissor is set in screen coords, with (0,0) at the top left corner
  // returns `enabled` flag
  virtual bool GetScissor (int *x, int *y, int *w, int *h) = 0;
  // this doesn't automatically enables scissor
  // note that invalid scissor means "disabled"
  // but zero scissor size is valid
  virtual void SetScissor (int x, int y, int w, int h) = 0;

  // call this if you modified scissor by direct calls
  // this resets scissor state to disabled and (0,0)-(0,0), but won't call any graphics API
  virtual void ForceClearScissorState () = 0;

public: // lighting
  enum {
    ELFlag_IgnoreDynLights    = 1u<<0,
    ELFlag_IgnoreStaticLights = 1u<<1,
    ELFlag_IgnoreAmbLights    = 1u<<2,
    ELFlag_IgnoreGlowLights   = 1u<<3,
    ELFlag_AllowSelfLights    = 1u<<4,
  };

  // returns 0 for "unknown"
  // otherwise returns "IRGB"
  //   I: overal light level
  //   RGB: calculated light color
  vuint32 CalcEntityLight (VEntity *lowner, unsigned flags);
};


// ////////////////////////////////////////////////////////////////////////// //
// drawer types, menu system uses these numbers
enum {
  DRAWER_OpenGL,
  DRAWER_MAX
};


// ////////////////////////////////////////////////////////////////////////// //
// drawer description
struct FDrawerDesc {
  const char *Name;
  const char *Description;
  const char *CmdLineArg;
  VDrawer *(*Creator) ();

  FDrawerDesc (int Type, const char *AName, const char *ADescription,
    const char *ACmdLineArg, VDrawer *(*ACreator)());
};


// ////////////////////////////////////////////////////////////////////////// //
// drawer driver declaration macro
#define IMPLEMENT_DRAWER(TClass, Type, Name, Description, CmdLineArg) \
static VDrawer *Create##TClass() \
{ \
  return new TClass(); \
} \
FDrawerDesc TClass##Desc(Type, Name, Description, CmdLineArg, Create##TClass);


#ifdef CLIENT
extern VDrawer *Drawer;
#endif


// call this before initializing video, to tell shitdoze to GTFO
// does nothing on normal OSes
void R_FuckOffShitdoze ();


#endif
