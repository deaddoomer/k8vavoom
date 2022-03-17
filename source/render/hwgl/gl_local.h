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
#ifndef VAVOOM_GL_LOCAL_HEADER
#define VAVOOM_GL_LOCAL_HEADER

#include "../../gamedefs.h"
#include "../r_shared.h"

#ifdef GL4ES_NO_CONSTRUCTOR
#  define GL_GLEXT_PROTOTYPES
#endif

#ifdef _WIN32
# include <windows.h>
#endif
#ifdef USE_GLAD
# include "glad.h"
#else
# include <GL/gl.h>
#endif
#ifdef _WIN32
/*
# ifndef GL_GLEXT_PROTOTYPES
#  define GL_GLEXT_PROTOTYPES
# endif
*/
# include <GL/glext.h>
#endif

// OpenGL extensions
#define VV_GLDECLS
#include "gl_imports.h"
#undef VV_GLDECLS


// ////////////////////////////////////////////////////////////////////////// //
#define VV_SHADOWCUBE_DEPTH_TEXTURE


// ////////////////////////////////////////////////////////////////////////// //
extern VCvarB gl_gpu_debug;
extern VCvarF gl_alpha_threshold;
extern VCvarB gl_sort_textures;
extern VCvarB r_decals;
extern VCvarB r_decals_flat;
extern VCvarB r_decals_wall;
extern VCvarB r_decals_wall_masked;
extern VCvarB r_decals_wall_alpha;
extern VCvarB gl_decal_debug_nostencil;
extern VCvarB gl_decal_debug_noalpha;
extern VCvarB gl_decal_dump_max;
extern VCvarB gl_decal_reset_max;
extern VCvarB gl_uusort_textures;
extern VCvarB gl_dbg_adv_render_surface_textures;
extern VCvarB gl_dbg_adv_render_surface_fog;
extern VCvarB gl_dbg_render_stack_portal_bounds;
extern VCvarB gl_use_stencil_quad_clear;
extern VCvarI gl_dbg_use_zpass;
extern VCvarB gl_dbg_wireframe;
extern VCvarF gl_maxdist;
extern VCvarB r_brightmaps;
extern VCvarB r_brightmaps_sprite;
extern VCvarB r_brightmaps_additive;
extern VCvarB r_brightmaps_filter;
extern VCvarB r_glow_flat;
extern VCvarB gl_lmap_allow_partial_updates;

extern VCvarB gl_regular_prefill_depth;

extern VCvarI gl_release_ram_textures_mode;
extern VCvarI gl_release_ram_textures_mode_sprite;
extern VCvarB gl_crop_psprites;
extern VCvarB gl_crop_sprites;

extern VCvarB gl_dbg_fbo_blit_with_texture;
extern VCvarB gl_letterbox;
extern VCvarI gl_letterbox_filter;
extern VCvarS gl_letterbox_color;
extern VCvarF gl_letterbox_scale;

extern VCvarB gl_enable_depth_bounds;
extern VCvarB gl_enable_clip_control;

extern VCvarB gl_use_srgb;

extern VCvarI r_light_mode;


// ////////////////////////////////////////////////////////////////////////// //
typedef void (*VV_GLOBJECTLABEL) (GLenum identifier, GLuint name, GLsizei length, const GLchar *label);
typedef void (*VV_GLOBJECTPTRLABEL) (const void *ptr, GLsizei length, const GLchar *label);
typedef void (*VV_GLGETOBJECTLABEL) (GLenum identifier, GLuint name, GLsizei bufSize, GLsizei *length, GLchar *label);
typedef void (*VV_GLGETOBJECTPTRLABEL) (const void *ptr, GLsizei bufSize, GLsizei *length, GLchar *label);

// for `p_glObjectLabel()` and such, for `identifier`
#ifndef GL_BUFFER
#define GL_BUFFER                         0x82E0
#endif
#ifndef GL_SHADER
#define GL_SHADER                         0x82E1
#endif
#ifndef GL_PROGRAM
#define GL_PROGRAM                        0x82E2
#endif
#ifndef GL_QUERY
#define GL_QUERY                          0x82E3
#endif
#ifndef GL_PROGRAM_PIPELINE
#define GL_PROGRAM_PIPELINE               0x82E4
#endif
#ifndef GL_SAMPLER
#define GL_SAMPLER                        0x82E6
#endif

#ifndef GL_MAX_LABEL_LENGTH
#define GL_MAX_LABEL_LENGTH               0x82E8
#endif


// ////////////////////////////////////////////////////////////////////////// //
static inline const char *VGetGLErrorStr (const GLenum glerr) {
  switch (glerr) {
    case GL_NO_ERROR: return "no error";
    case GL_INVALID_ENUM: return "invalid enum";
    case GL_INVALID_VALUE: return "invalid value";
    case GL_INVALID_OPERATION: return "invalid operation";
    case GL_STACK_OVERFLOW: return "stack overflow";
    case GL_STACK_UNDERFLOW: return "stack underflow";
    case GL_OUT_OF_MEMORY: return "out of memory";
    default: break;
  }
  static char errstr[32];
  snprintf(errstr, sizeof(errstr), "0x%04x", (unsigned)glerr);
  return errstr;
}

#define GLDRW_RESET_ERROR()  (void)glGetError()

#define GLDRW_CHECK_ERROR(actionmsg_)  do { \
  GLenum glerr_ = glGetError(); \
  if (glerr_ != 0) Sys_Error("OpenGL: cannot %s, error: %s", actionmsg_, VGetGLErrorStr(glerr_)); \
} while (0)


// ////////////////////////////////////////////////////////////////////////// //
// shadowmap blur variants (we will create separate shaders for this)
enum {
  SMAP_NOBLUR,
  SMAP_BLUR4BI, // "VV_SMAP_BLUR4"
  SMAP_BLUR4SBI, // "VV_SMAP_SHITTY_BILINEAR"
  SMAP_BLUR4, // "VV_SMAP_BLUR4"
  SMAP_BLUR8, // "VV_SMAP_BLUR8"
  SMAP_BLUR8_FAST, // "VV_SMAP_BLUR8","VV_SMAP_BLUR_FAST8"
  SMAP_BLUR16, // "VV_SMAP_BLUR16"

  SMAP_BLUR_MAX,
};


#define VV_DECLARE_SMAP_SHADER(shad_)  \
  VShaderDef_##shad_ shad_##Blur [SMAP_BLUR_MAX];

#define VV_DECLARE_SMAP_SHADER_LEVEL \
  VV_DECLARE_SMAP_SHADER(ShadowsLightSMap) \
  VV_DECLARE_SMAP_SHADER(ShadowsLightSMapTex) \
  VV_DECLARE_SMAP_SHADER(ShadowsLightSMapSpot) \
  VV_DECLARE_SMAP_SHADER(ShadowsLightSMapSpotTex) \
  VV_DECLARE_SMAP_SHADER(ShadowsModelLightSMap) \
  VV_DECLARE_SMAP_SHADER(ShadowsModelLightSMapSpot)


// ////////////////////////////////////////////////////////////////////////// //
class VOpenGLDrawer : public VDrawer {
public:
  class VGLShader {
  public:
    enum {
      CondLess,
      CondLessEqu,
      CondEqu,
      CondGreater,
      CondGreaterEqu,
      CondNotEqu,
    };
  public:
    VGLShader *next;
    VOpenGLDrawer *owner;
    const char *progname;
    const char *incdir;
    const char *vssrcfile;
    const char *fssrcfile;
    // compiled vertex program
    GLhandleARB prog;
    int oglVersionCond;
    int oglVersion;  // high*100+low
    bool forCubemaps;
    TArray<VStr> defines;
    bool compiled; // for on-demand compiling

    typedef float glsl_float2[2];
    typedef float glsl_float3[3];
    typedef float glsl_float4[4];
    typedef float glsl_float9[9];

  public:
    inline VGLShader ()
      : next(nullptr)
      , owner(nullptr)
      , progname(nullptr)
      , vssrcfile(nullptr)
      , fssrcfile(nullptr)
      , prog(0)
      , oglVersionCond(CondGreaterEqu)
      , oglVersion(0)
      , forCubemaps(false)
      , compiled(false)
    {}

    inline void SetOpenGLVersion (int cond, int ver) noexcept { oglVersionCond = cond; oglVersion = ver; }
    inline void SetForCubemaps () noexcept { forCubemaps = true; }

    VVA_FORCEINLINE bool IsLoaded () const noexcept { return compiled; }

    bool CheckOpenGLVersion (int major, int minor, bool canCubemaps) noexcept;

    void MainSetup (VOpenGLDrawer *aowner, const char *aprogname, const char *aincdir, const char *avssrcfile, const char *afssrcfile);

    virtual void Compile ();
    virtual void Unload ();
    virtual void Setup (VOpenGLDrawer *aowner) = 0;
    virtual void LoadUniforms () = 0;
    virtual void UnloadUniforms () = 0;
    virtual void UploadChangedUniforms (bool forced=false) = 0;
    //virtual void UploadChangedAttrs () = 0;

    void Activate ();
    void Deactivate ();
    bool IsActive () const noexcept;

    static VVA_FORCEINLINE bool notEqual_float (const float &v1, const float &v2) noexcept { return (FASI(v1) != FASI(v2)); }
    static VVA_FORCEINLINE bool notEqual_int (const int32_t v1, const int32_t v2) noexcept { return (v1 != v2); }
    static VVA_FORCEINLINE bool notEqual_bool (const bool v1, const bool v2) noexcept { return (v1 != v2); }
    static VVA_FORCEINLINE bool notEqual_vec2 (const float *v1, const float *v2) noexcept { return (memcmp(v1, v2, sizeof(float)*2) != 0); }
    static VVA_FORCEINLINE bool notEqual_vec3 (const TVec &v1, const TVec &v2) noexcept { return (memcmp(&v1.x, &v2.x, sizeof(float)*3) != 0); }
    static VVA_FORCEINLINE bool notEqual_vec4 (const float *v1, const float *v2) noexcept { return (memcmp(v1, v2, sizeof(float)*4) != 0); }
    static VVA_FORCEINLINE bool notEqual_mat3 (const float *v1, const float *v2) noexcept { return (memcmp(v1, v2, sizeof(float)*9) != 0); }
    static VVA_FORCEINLINE bool notEqual_mat4 (const VMatrix4 &v1, const VMatrix4 &v2) noexcept { return (memcmp(&v1.m[0][0], &v2.m[0][0], sizeof(float)*16) != 0); }
    static VVA_FORCEINLINE bool notEqual_sampler2D (const vuint32 v1, const vuint32 v2) noexcept { return (v1 != v2); }
    static VVA_FORCEINLINE bool notEqual_sampler2DShadow (const vuint32 v1, const vuint32 v2) noexcept { return (v1 != v2); }
    static VVA_FORCEINLINE bool notEqual_samplerCube (const vuint32 v1, const vuint32 v2) noexcept { return (v1 != v2); }
    static VVA_FORCEINLINE bool notEqual_samplerCubeShadow (const vuint32 v1, const vuint32 v2) noexcept { return (v1 != v2); }

    static VVA_FORCEINLINE void copyValue_float (float &dest, const float &src) noexcept { dest = src; }
    static VVA_FORCEINLINE void copyValue_int (int32_t &dest, const int32_t src) noexcept { dest = src; }
    static VVA_FORCEINLINE void copyValue_bool (bool &dest, const bool &src) noexcept { dest = src; }
    static VVA_FORCEINLINE void copyValue_vec3 (TVec &dest, const TVec &src) noexcept { dest = src; }
    static VVA_FORCEINLINE void copyValue_mat4 (VMatrix4 &dest, const VMatrix4 &src) noexcept { memcpy(&dest.m[0][0], &src.m[0][0], sizeof(float)*16); }
    static VVA_FORCEINLINE void copyValue_vec4 (float *dest, const float *src) noexcept { memcpy(dest, src, sizeof(float)*4); }
    static VVA_FORCEINLINE void copyValue_vec2 (float *dest, const float *src) noexcept { memcpy(dest, src, sizeof(float)*2); }
    static VVA_FORCEINLINE void copyValue_mat3 (float *dest, const float *src) noexcept { memcpy(dest, src, sizeof(float)*9); }
    static VVA_FORCEINLINE void copyValue_sampler2D (vuint32 &dest, const vuint32 &src) noexcept { dest = src; }
    static VVA_FORCEINLINE void copyValue_sampler2DShadow (vuint32 &dest, const vuint32 &src) noexcept { dest = src; }
    static VVA_FORCEINLINE void copyValue_samplerCube (vuint32 &dest, const vuint32 &src) noexcept { dest = src; }
    static VVA_FORCEINLINE void copyValue_samplerCubeShadow (vuint32 &dest, const vuint32 &src) noexcept { dest = src; }
  };

  friend class VGLShader;

  class FBO {
    //friend class VOpenGLDrawer;
  private:
    class VOpenGLDrawer *mOwner;
    GLuint mFBO;
    GLuint mColorTid;
    GLuint mDepthStencilRBO; // use renderbuffer for depth/stencil (we don't need to read this info yet); 0 if none
    int mWidth;
    int mHeight;
    bool mLinearFilter;
    bool mIsFP;
    GLint mType; // texture type

  public:
    bool scrScaled;

  private:
    void createInternal (VOpenGLDrawer *aowner, int awidth, int aheight, bool createDepthStencil, bool mirroredRepeat, bool wantFP);

  public:
    VV_DISABLE_COPY(FBO)
    FBO ();
    ~FBO ();

    void SwapWith (FBO *other);

    VVA_FORCEINLINE bool isValid () const noexcept { return (mOwner != nullptr); }
    VVA_FORCEINLINE int getWidth () const noexcept { return mWidth; }
    VVA_FORCEINLINE int getHeight () const noexcept { return mHeight; }
    VVA_FORCEINLINE int getGLType () const noexcept { return (int)mType; }

    VVA_FORCEINLINE bool getLinearFilter () const noexcept { return mLinearFilter; }
    // has effect after texture recreation
    VVA_FORCEINLINE void setLinearFilter (bool v) noexcept { mLinearFilter = v; }

    VVA_FORCEINLINE GLuint getColorTid () const noexcept { return mColorTid; }
    VVA_FORCEINLINE GLuint getFBOid () const noexcept { return mFBO; }
    VVA_FORCEINLINE GLuint getDSRBTid () const noexcept { return mDepthStencilRBO; }

    bool isColor8Bit () const noexcept { return (!mIsFP && mType != GL_RGB10_A2); }
    bool isColor10Bit () const noexcept { return (mType == GL_RGB10_A2); }
    bool isColorFloat () const noexcept { return mIsFP; }

    void createTextureOnly (VOpenGLDrawer *aowner, int awidth, int aheight, bool wantFP=false, bool mirroredRepeat=false);
    void createDepthStencil (VOpenGLDrawer *aowner, int awidth, int aheight, bool wantFP=false, bool mirroredRepeat=false);
    void destroy ();

    void activate ();
    void deactivate ();

    // other FBO should have the same dimensions!
    // returns `false` on any error
    bool swapColorTextures (FBO *other);

    // this blits only color info
    // restore active FBO manually after calling this
    // it also can reset shader program
    // if `dest` is nullptr, blit to screen buffer (not yet)
    void blitTo (FBO *dest, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLenum filter);

    void blitToScreen ();
  };

  friend class FBO;

  VGLShader *currentActiveShader;
  FBO *currentActiveFBO;

  void DeactivateShader ();
  void ReactivateCurrentFBO ();

#include "gl_shaddef.hi"

VV_DECLARE_SMAP_SHADER_LEVEL

  void LoadShadowmapShaders ();

private:
  int glVerMajor;
  int glVerMinor;
  bool usingZPass; // if we are rendering shadow volumes, should we do "z-pass"?

  GLint savedDepthMask; // used in various begin/end methods
  // for `DrawTexturedPoly()` API
  VTexture *texturedPolyLastTex;
  float texturedPolyLastAlpha;
  TVec texturedPolyLastLight;
  // list of surfaces with masked textures, for z-prefill
  TArrayNC<surface_t *> zfillMasked;

  bool canIntoBloomFX;

  bool scissorEnabled;
  int scissorX, scissorY, scissorW, scissorH;

  // returns `false` if scissor is empty
  bool UploadCurrentScissorRect ();

private:
  // used in lijhgt renderer, set by `BeginLightPass()` and `BeginModelsLightPass()`
  bool spotLight; // is current light a spotlight?
  TVec coneDir; // current spotlight direction
  float coneAngle; // current spotlight cone angle

protected:
  VGLShader *shaderHead;

  void registerShader (VGLShader *shader);
  void CompileShaders (int glmajor, int glminor, bool canCubemaps);
  void DestroyShaders (); // unload from GPU, remove from list

  void UnloadShadowMapShaders ();

  void InitShaderProgress ();
  void DoneShaderProgress ();
  void ShowShaderProgress (int curr, int total);

protected:
  enum { MaxDepthMaskStack = 16 };
  GLint depthMaskStack[MaxDepthMaskStack];
  unsigned depthMaskSP;
  GLint currDepthMaskState;

  VVA_FORCEINLINE void GLForceDepthWrite () noexcept { currDepthMaskState = 1; glDepthMask(GL_TRUE); }
  VVA_FORCEINLINE void GLEnableDepthWrite () noexcept { if (!currDepthMaskState) { currDepthMaskState = 1; glDepthMask(GL_TRUE); } }
  VVA_FORCEINLINE void GLDisableDepthWrite () noexcept { if (currDepthMaskState) { currDepthMaskState = 0; glDepthMask(GL_FALSE); } }

  VVA_FORCEINLINE void PushDepthMask () noexcept {
    if (depthMaskSP >= MaxDepthMaskStack) Sys_Error("OpenGL: depth mask stack overflow");
    depthMaskStack[depthMaskSP++] = currDepthMaskState;
  }

  VVA_FORCEINLINE void PopDepthMask () noexcept {
    if (depthMaskSP == 0) Sys_Error("OpenGL: depth mask stack underflow");
    const GLint st = depthMaskStack[--depthMaskSP];
    if (currDepthMaskState != st) { currDepthMaskState = st; glDepthMask(st); }
  }

public:
  virtual void GLDisableDepthWriteSlow () noexcept override;
  virtual void PushDepthMaskSlow () noexcept override;
  virtual void PopDepthMaskSlow () noexcept override;

  virtual void GLDisableDepthTestSlow () noexcept override;
  virtual void GLEnableDepthTestSlow () noexcept override;

public:
  // scissor array indicies
  enum {
    SCS_MINX,
    SCS_MINY,
    SCS_MAXX,
    SCS_MAXY,
  };

  VVA_FORCEINLINE bool checkGLVer (int major, int minor) const noexcept { return (glVerMajor == major ? (glVerMinor >= minor) : (glVerMajor > major)); }

protected:
  // we have two shadowmaps: one for lights with radius less than 1020 (16-bit floats), and one for 1020+ (32-bit floats)
  struct ShadowCubeMapData {
    class VOpenGLDrawer *mOwner;
    GLuint cubeTexId;
    //GLuint cubeDepthTexId[6];
    #ifdef VV_SHADOWCUBE_DEPTH_TEXTURE
    GLuint cubeDepthTexId[6]; // render buffers for depth (we're not doing depth sampling for shadowmaps)
    #else
    GLuint cubeDepthRBId[6]; // render buffers for depth (we're not doing depth sampling for shadowmaps)
    #endif
    unsigned shift; // 64<<shift is size
    bool float32;

    #ifdef VV_SHADOWCUBE_DEPTH_TEXTURE
    static bool useDefaultDepth;
    #endif

    VVA_FORCEINLINE ShadowCubeMapData () noexcept : mOwner(nullptr), cubeTexId(0), shift(0u), float32(false) {
      #ifdef VV_SHADOWCUBE_DEPTH_TEXTURE
      memset((void *)&cubeDepthTexId[0], 0, sizeof(cubeDepthTexId));
      #else
      memset((void *)&cubeDepthRBId[0], 0, sizeof(cubeDepthRBId));
      #endif
    }

    VVA_FORCEINLINE VVA_CHECKRESULT bool isValid () const noexcept { return (mOwner && cubeTexId != 0); }

    void createTextures (VOpenGLDrawer *aowner, const unsigned ashift, const bool afloat32) noexcept;
    void destroyTextures () noexcept;
  };

  struct ShadowCubeMap : public ShadowCubeMapData {
    GLuint cubeFBO;
    unsigned int smapDirty; // bits 0..5
    unsigned int smapCurrentFace;

    VMatrix4 proj; // light projection matrix
    VMatrix4 lmpv[6]; // light view+projection matrices

    VVA_FORCEINLINE ShadowCubeMap () noexcept : ShadowCubeMapData(), cubeFBO(0), smapDirty(0x3f), smapCurrentFace(0) {}

    VVA_FORCEINLINE void setAllDirty () noexcept { smapDirty = 0x3f; }

    void createCube (VOpenGLDrawer *aowner, const unsigned ashift, const bool afloat32) noexcept;
    void destroyCube () noexcept;
  };

  enum {
    CUBE16 = 0u,
    CUBE32 = 1u,
  };

  // should be even!
  enum { MaxShadowCubes = 4 };

  ShadowCubeMap shadowCube[MaxShadowCubes];

  GLint shadowmapSize;
  unsigned int shadowmapPOT; // 128<<shadowmapPOT is shadowmap size

  GLint savedSMVPort[4];
  //VMatrix4 smapProj;
  texinfo_t smapLastTexinfo;
  texinfo_t smapLastSprTexinfo;

  unsigned smapCurrent; // index

protected:
  // clear current cube textures
  void ClearShadowMapsInternal ();

  // called in `FinishUpdate()`, so GPU could perform `glClear()` in parallel
  void ClearAllShadowMaps ();

  // activate cube (create it if necessary); calculate matrices
  // won't clear faces
  void PrepareShadowCube (const TVec &LightPos, const float Radius) noexcept;

  // also sets is as current
  // shadowmap FBO must be active
  void ActivateShadowMapFace (unsigned int facenum) noexcept;

  inline void SetCurrentShadowMapFace (unsigned int facenum) noexcept { shadowCube[smapCurrent].smapCurrentFace = facenum; }
  inline void MarkCurrentShadowMapDirty () noexcept { shadowCube[smapCurrent].smapDirty |= 1u<<shadowCube[smapCurrent].smapCurrentFace; }
  inline void MarkCurrentShadowMapClean () noexcept { shadowCube[smapCurrent].smapDirty &= ~(1u<<shadowCube[smapCurrent].smapCurrentFace); }

  inline bool IsShadowMapDirty (unsigned int facenum) const noexcept { return shadowCube[smapCurrent].smapDirty&(1u<<facenum); }
  inline bool IsCurrentShadowMapDirty () const noexcept {  return shadowCube[smapCurrent].smapDirty&(1u<<shadowCube[smapCurrent].smapCurrentFace); }
  inline bool IsAnyShadowMapDirty () const noexcept { return (shadowCube[smapCurrent].smapDirty&0x3f); }
  inline void MarkAllShadowMapsClear () noexcept { shadowCube[smapCurrent].smapDirty = 0u; }

public:
  // VDrawer interface
  VOpenGLDrawer ();
  virtual ~VOpenGLDrawer () override;

  virtual void InitResolution () override;
  virtual void DeinitResolution () override;

  // recreate FBOs with fp/int formats (alpha is not guaranteed)
  // used for overbright
  virtual bool RecreateFBOs (bool wantFP) override;
  virtual bool IsMainFBOFloat () override;
  virtual bool IsCameraFBO () override;

  // normalize overbrighting for fp textures
  virtual void PostprocessOvebright () override;

  // shadowmap management
  // this also unload shaders
  void DestroyShadowCube ();
  void CreateShadowCube ();

  // call in the very first shadowmap stage
  void EnsureShadowMapCube ();

  virtual void StartUpdate () override;
  void FinishUpdate ();

  virtual void ClearScreen (unsigned clearFlags=CLEAR_COLOR) override;
  virtual void Setup2D () override;

  virtual void *ReadScreen (int *, bool *) override;
  virtual void ReadBackScreen (int, int, rgba_t *) override;

  void ReadFBOPixels (FBO *srcfbo, int Width, int Height, rgba_t *Dest);

  void PrepareWireframe ();
  void DrawWireframeSurface (const surface_t *surf);
  void DoneWireframe ();

  // rendering stuff
  // shoud frustum use far clipping plane?
  virtual bool UseFrustumFarClip () override;
  // setup projection matrix and viewport, clear screen, setup various OpenGL context properties
  // called from `VRenderLevelShared::SetupFrame()`
  virtual void SetupView (VRenderLevelDrawer *ARLev, const refdef_t *rd) override;
  // setup model matrix according to `viewangles` and `vieworg`
  virtual void SetupViewOrg () override;
  // setup 2D ortho rendering mode
  virtual void EndView () override;
  virtual void RenderTint (vuint32 CShift) override;

  // texture stuff
  virtual void PrecacheTexture (VTexture *Tex) override;
  virtual void PrecacheSpriteTexture (VTexture *Tex, SpriteType sptype) override;

  virtual void BeforeDrawWorldLMap () override;
  virtual void BeforeDrawWorldSV () override;

  // polygon drawing
  virtual void DrawLightmapWorld () override;

  virtual void DrawWorldAmbientPass () override;

  virtual void BeginShadowVolumesPass () override;
  virtual void BeginLightShadowVolumes (const TVec &LightPos, const float Radius, bool useZPass, bool hasScissor, const int scoords[4], const refdef_t *rd) override;
  virtual void EndLightShadowVolumes () override;
  virtual void RenderSurfaceShadowVolume (VLevel *Level, const surface_t *surf, const TVec &LightPos, float Radius) override;
  void RenderSurfaceShadowVolumeZPassIntr (VLevel *Level, const surface_t *surf, const TVec &LightPos, float Radius);

  bool AdvRenderCanSurfaceCastShadow (VLevel *Level, const surface_t *surf, const TVec &LightPos, float Radius);

  virtual void BeginLightShadowMaps (const TVec &LightPos, const float Radius) override;
  virtual void EndLightShadowMaps () override;
  virtual void SetupLightShadowMap (unsigned int facenum) override;

  //void DrawSurfaceShadowMap (const surface_t *surf);
  // may modify lists (sort)
  virtual void UploadShadowSurfaces (TArrayNC<surface_t *> &solid, TArrayNC<surface_t *> &masked) override;
  virtual void RenderShadowMaps (TArrayNC<surface_t *> &solid, TArrayNC<surface_t *> &masked) override;

  virtual void BeginLightPass (const TVec &LightPos, float Radius, float LightMin, vuint32 Color, const bool aspotLight, const TVec &aconeDir, const float aconeAngle, bool doShadow) override;
  virtual void EndLightPass () override;

  void DrawSurfaceLight (const surface_t *surf);
  virtual void RenderSolidLightSurfaces (TArrayNC<surface_t *> &slist) override;
  virtual void RenderMaskedLightSurfaces (TArrayNC<surface_t *> &slist) override;

  virtual void DrawWorldTexturesPass () override;
  virtual void DrawWorldFogPass () override;
  virtual void EndFogPass () override;

  virtual void StartSkyPolygons () override;
  virtual void EndSkyPolygons () override;
  virtual void DrawSkyPolygon (surface_t *, bool, VTexture *, float, VTexture *, float, int) override;
  virtual void DrawMaskedPolygon (surface_t *surf, float Alpha, bool Additive, bool DepthWrite, bool onlyTranslucent) override;

  //virtual void BeginTranslucentPolygonDecals () override;
  //virtual void DrawTranslucentPolygonDecals (surface_t *surf, float Alpha, bool Additive) override;

  virtual void DrawSpritePolygon (float time, const TVec *cv, VTexture *Tex,
                                  const RenderStyleInfo &ri,
                                  VTextureTranslation *Translation, int CMap,
                                  const TVec &sprnormal, float sprpdist,
                                  const TVec &saxis, const TVec &taxis, const TVec &texorg,
                                  SpriteType sptype=SP_Normal) override;

  virtual void DrawSpriteShadowMap (const TVec *cv, VTexture *Tex, const TVec &sprnormal,
                                    const TVec &saxis, const TVec &taxis, const TVec &texorg) override;

  virtual void DrawAliasModel (const TVec &origin, const TAVec &angles, const AliasModelTrans &Transform,
                               VMeshModel *Mdl, int frame, int nextframe, VTexture *Skin, VTextureTranslation *Trans,
                               int CMap, const RenderStyleInfo &ri, bool is_view_model,
                               float Inter, bool Interpolate, bool ForceDepthUse, bool AllowTransparency,
                               bool onlyDepth) override;

  virtual void BeginModelsAmbientPass () override;
  virtual void EndModelsAmbientPass () override;
  virtual void DrawAliasModelAmbient (const TVec &, const TAVec &, const AliasModelTrans &Transform,
    VMeshModel *, int, int, VTexture *, vuint32, float, float, bool,
    bool, bool) override;

  virtual void BeginModelsLightPass (const TVec &LightPos, float Radius, float LightMin, vuint32 Color, const bool aspotLight, const TVec &aconeDir, const float aconeAngle, bool doShadow) override;
  virtual void EndModelsLightPass () override;
  virtual void DrawAliasModelLight (const TVec &, const TAVec &, const AliasModelTrans &Transform,
    VMeshModel *, int, int, VTexture *, float, float, bool, bool) override;

  virtual void BeginModelShadowMaps (const TVec &LightPos, const float Radius) override;
  virtual void EndModelShadowMaps () override;
  virtual void SetupModelShadowMap (unsigned int facenum) override;
  virtual void DrawAliasModelShadowMap (const TVec &origin, const TAVec &angles,
                                        const AliasModelTrans &Transform,
                                        VMeshModel *Mdl, int frame, int nextframe,
                                        VTexture *Skin, float Alpha, float Inter,
                                        bool Interpolate, bool AllowTransparency) override;

  virtual void BeginModelsShadowsPass (TVec &LightPos, float LightRadius) override;
  virtual void EndModelsShadowsPass () override;
  virtual void DrawAliasModelShadow(const TVec &, const TAVec &, const AliasModelTrans &Transform,
    VMeshModel *, int, int, float, bool, const TVec &, float) override;

  virtual void BeginModelsTexturesPass () override;
  virtual void EndModelsTexturesPass () override;
  void DrawAliasModelTextures (const TVec &origin, const TAVec &angles,
                               const AliasModelTrans &Transform,
                               VMeshModel *Mdl, int frame, int nextframe,
                               VTexture *Skin, VTextureTranslation *Trans, int CMap,
                               const RenderStyleInfo &ri, float Inter,
                               bool Interpolate, bool ForceDepth, bool AllowTransparency) override;

  virtual void BeginModelsFogPass () override;
  virtual void EndModelsFogPass () override;
  virtual void DrawAliasModelFog(const TVec &, const TAVec &, const AliasModelTrans &Transform,
    VMeshModel *, int, int, VTexture *, vuint32, float, float, bool, bool) override;

  virtual bool StartPortal(VPortal *, bool) override;
  virtual void EndPortal(VPortal *, bool) override;

  // call this to disable stencil tests instead of doing it directly
  // this is required for portals
  void DisableStenciling ();
  void DisableScissoring ();
  void RestorePortalStenciling ();

  // particles
  virtual void StartParticles () override;
  virtual void DrawParticle (particle_t *) override;
  virtual void EndParticles () override;

  // drawing
  virtual void DrawPic (float x1, float y1, float x2, float y2,
                        float s1, float t1, float s2, float t2,
                        VTexture *Tex, VTextureTranslation *Trans, float Alpha) override;
  // recolor picture using intensity
  virtual void DrawPicRecolored (float x1, float y1, float x2, float y2,
                                 float s1, float t1, float s2, float t2,
                                 VTexture *Tex, int rgbcolor, float Alpha) override;
  virtual void DrawPicShadow (float x1, float y1, float x2, float y2,
                              float s1, float t1, float s2, float t2,
                              VTexture *Tex, float shade) override;
  virtual void FillRectWithFlat (float x1, float y1, float x2, float y2,
                                 float s1, float t1, float s2, float t2, VTexture *Tex) override;
  virtual void FillRectWithFlatRepeat (float x1, float y1, float x2, float y2,
                                       float s1, float t1, float s2, float t2, VTexture *Tex) override;
  virtual void FillRect (float x1, float y1, float x2, float y2, vuint32 color, float alpha=1.0f) override;
  virtual void DrawRect (float x1, float y1, float x2, float y2, vuint32 color, float alpha=1.0f) override;
  virtual void ShadeRect (float x1, float y1, float x2, float y2, float darkening) override;
  virtual void DrawLine (int x1, int y1, int x2, int y2, vuint32 color, float alpha=1.0f) override;
  virtual void DrawConsoleBackground (int h) override;
  virtual void DrawSpriteLump (float x1, float y1, float x2, float y2,
                               VTexture *Tex, VTextureTranslation *Translation, bool flip) override;

  virtual void DrawHex (float x0, float y0, float w, float h, vuint32 color, float alpha=1.0f) override;
  virtual void FillHex (float x0, float y0, float w, float h, vuint32 color, float alpha=1.0f) override;
  virtual void ShadeHex (float x0, float y0, float w, float h, float darkening) override;

  virtual void BeginTexturedPolys () override;
  virtual void EndTexturedPolys () override;
  virtual void DrawTexturedPoly (const texinfo_t *tinfo, TVec light, float alpha, int vcount, const TVec *verts, const SurfVertex *origverts=nullptr) override;

  // automap
  virtual void StartAutomap (bool asOverlay) override;
  virtual void DrawLineAM (float x1, float y1, vuint32 c1, float x2, float y2, vuint32 c2) override;
  virtual void EndAutomap () override;

  // advanced drawing.
  virtual bool SupportsShadowVolumeRendering () override;
  virtual bool SupportsShadowMapRendering () override;

  virtual void GetProjectionMatrix (VMatrix4 &mat) override;
  virtual void GetModelMatrix (VMatrix4 &mat) override;
  virtual void SetProjectionMatrix (const VMatrix4 &mat) override;
  virtual void SetModelMatrix (const VMatrix4 &mat) override;

  // call this before doing light scissor calculations (can be called once per scene)
  // sets `vpmats`
  // scissor setup will use those matrices (but won't modify them)
  // called in `SetupViewOrg()`, which setups model transformation matrix
  //virtual void LoadVPMatrices () override;
  // no need to do this:
  //   projection matrix and viewport set in `SetupView()`
  //   model matrix set in `SetupViewOrg()`

  // returns 0 if scissor has no sense; -1 if scissor is empty, and 1 if scissor is set
  virtual int SetupLightScissor (const TVec &org, float radius, int scoord[4], const TVec *geobbox=nullptr) override;
  virtual void ResetScissor () override;

  static inline float getAlphaThreshold () { return clampval(gl_alpha_threshold.asFloat(), 0.0f, 1.0f); }

  //virtual void GetRealWindowSize (int *rw, int *rh) override;

  virtual void DebugRenderScreenRect (int x0, int y0, int x1, int y1, vuint32 color) override;

  virtual void SetScissorEnabled (bool v) override;
  virtual bool IsScissorEnabled () override;

  // scissor is set in screen coords, with (0,0) at the top left corner
  // returns `enabled` flag
  virtual bool GetScissor (int *x, int *y, int *w, int *h) override;
  virtual void SetScissor (int x, int y, int w, int h) override;

  // call this if you modified scissor by direct calls
  // this resets scissor state to disabled and (0,0)-(0,0), but won't call any graphics API
  virtual void ForceClearScissorState () override;

private:
  vuint8 decalStcVal;
  bool decalUsedStencil;
  bool stencilBufferDirty;
  bool blendEnabled;
  bool offsetEnabled;
  float offsetFactor, offsetUnits;

  // last used shadow volume scissor
  // if new scissor is inside this one, and stencil buffer is not changed,
  // do not clear stencil buffer
  // reset in `BeginShadowVolumesPass()`
  // modified in `BeginLightShadowVolumes()`
  GLint lastSVScissor[4];
  // set in `BeginShadowVolumesPass()`
  GLint lastSVVport[4];

  // modified in `SetupLightScissor()`/`ResetScissor()`
  GLint currentSVScissor[4];

  // reset in `BeginLightShadowVolumes()`
  // set in `RenderSurfaceShadowVolume()`
  bool wasRenderedShadowSurface;

  // so we can avoid queriung it
  GLint currentViewport[4];

  enum DecalType { DT_SIMPLE, DT_LIGHTMAP, DT_ADVANCED };

  // this is required for decals
  inline void NoteStencilBufferDirty () noexcept { stencilBufferDirty = true; }
  inline bool IsStencilBufferDirty () const noexcept { return stencilBufferDirty; }
  inline void ClearStencilBuffer () noexcept { if (stencilBufferDirty) glClear(GL_STENCIL_BUFFER_BIT); stencilBufferDirty = false; decalUsedStencil = false; }

  inline bool GLIsBlendEnabled () noexcept { return blendEnabled; }
  inline void GLForceBlend () noexcept { blendEnabled = true; glEnable(GL_BLEND); }
  inline void GLEnableBlend () noexcept { if (!blendEnabled) { blendEnabled = true; glEnable(GL_BLEND); } }
  inline void GLDisableBlend () noexcept { if (blendEnabled) { blendEnabled = false; glDisable(GL_BLEND); } }
  inline void GLSetBlendEnabled (const bool v) noexcept { if (blendEnabled != v) { blendEnabled = v; if (v) glEnable(GL_BLEND); else glDisable(GL_BLEND); } }

  inline void GLSetViewport (int x, int y, int width, int height) noexcept {
    if (currentViewport[0] != x || currentViewport[1] != y || currentViewport[2] != width || currentViewport[3] != height) {
      currentViewport[0] = x;
      currentViewport[1] = y;
      currentViewport[2] = width;
      currentViewport[3] = height;
      glViewport(x, y, width, height);
    }
  }

  inline void GLGetViewport (int vp[4]) const noexcept { vp[0] = currentViewport[0]; vp[1] = currentViewport[1]; vp[2] = currentViewport[2]; vp[3] = currentViewport[3]; }

  virtual void GLEnableOffset () override;
  virtual void GLDisableOffset () override;
  // this also enables it if it was disabled
  virtual void GLPolygonOffset (const float afactor, const float aunits) override;

  virtual void ForceClearStencilBuffer () override;
  virtual void ForceMarkStencilBufferDirty () override;

  virtual void EnableBlend () override;
  virtual void DisableBlend () override;
  virtual void SetBlendEnabled (const bool v) override;

  // returns `true` if there are some decals to render
  // "prepare" and "finish" will perform no more checks!
  bool RenderSurfaceHasDecals (const surface_t *surf) const;
  // you *have* to call `RenderFinishShaderDecals()` to restore render state
  void RenderPrepareShaderDecals (const surface_t *surf);
  // returns `true` if any decals was rendered (i.e. render state was changed)
  bool RenderFinishShaderDecals (DecalType dtype, surface_t *surf, surfcache_t *cache, int cmap);

  // regular renderer building parts
  // returns `true` if we need to re-setup texture
  bool RenderSimpleSurface (bool textureChanged, surface_t *surf);
  bool RenderLMapSurface (bool textureChanged, surface_t *surf, surfcache_t *cache);

  void RestoreDepthFunc ();

  // see "r_shared.h" for `struct GlowParams`

#define VV_GLDRAWER_ACTIVATE_GLOW(shad_,gp_)  do { \
  shad_.SetGlowColorFloor(((gp_.glowCF>>16)&0xff)/255.0f, ((gp_.glowCF>>8)&0xff)/255.0f, (gp_.glowCF&0xff)/255.0f, ((gp_.glowCF>>24)&0xff)/255.0f); \
  shad_.SetGlowColorCeiling(((gp_.glowCC>>16)&0xff)/255.0f, ((gp_.glowCC>>8)&0xff)/255.0f, (gp_.glowCC&0xff)/255.0f, ((gp_.glowCC>>24)&0xff)/255.0f); \
  shad_.SetFloorZ(gp_.floorZ); \
  shad_.SetCeilingZ(gp_.ceilingZ); \
  shad_.SetFloorGlowHeight(gp_.floorGlowHeight); \
  shad_.SetCeilingGlowHeight(gp_.ceilingGlowHeight); \
} while (0)

#define VV_GLDRAWER_DEACTIVATE_GLOW(shad_)  do { \
  shad_.SetGlowColorFloor(0.0f, 0.0f, 0.0f, 0.0f); \
  shad_.SetGlowColorCeiling(0.0f, 0.0f, 0.0f, 0.0f); \
  shad_.SetFloorGlowHeight(128); \
  shad_.SetCeilingGlowHeight(128); \
} while (0)

  inline void CalcGlow (GlowParams &gp, const surface_t *surf) const noexcept {
    gp.clear();
    if (!surf->seg || !surf->subsector) return;
    bool checkFloorFlat, checkCeilingFlat;
    const sector_t *sec = surf->subsector->sector;
    // check for glowing sector floor
    if (surf->glowFloorColor && surf->glowFloorHeight > 0.0f) {
      gp.floorGlowHeight = surf->glowFloorHeight;
      gp.glowCF = surf->glowFloorColor; // always fullbright
      gp.floorZ = sec->floor.GetPointZClamped(*surf->seg->v1);
      checkFloorFlat = false;
    } else {
      checkFloorFlat = true;
    }
    // check for glowing sector ceiling
    if (surf->glowCeilingColor && surf->glowCeilingHeight > 0.0f) {
      gp.ceilingGlowHeight = surf->glowCeilingHeight;
      gp.glowCC = surf->glowCeilingColor; // always fullbright
      gp.ceilingZ = sec->ceiling.GetPointZClamped(*surf->seg->v1);
      checkCeilingFlat = false;
    } else {
      checkCeilingFlat = true;
    }
    if ((checkFloorFlat || checkCeilingFlat) && r_glow_flat) {
      // check for glowing textures
      //FIXME: check actual view height here
      if (sec /*&& !sec->heightsec*/) {
        if (checkFloorFlat && sec->floor.pic) {
          VTexture *gtex = GTextureManager(sec->floor.pic);
          if (gtex && gtex->Type != TEXTYPE_Null && gtex->glowing) {
            gp.floorGlowHeight = 128.0f;
            gp.glowCF = gtex->glowing;
            gp.floorZ = sec->floor.GetPointZClamped(*surf->seg->v1);
            if (!gtex->IsGlowFullbright()) {
              // fix light level
              //gp.glowCF = (gp.glowCF&0x00ffffffu)|(((unsigned)surf->Light)&0xff000000u);
              gp.glowCF &= 0x00ffffffu;
            } else {
              gp.glowCF |= 0xff000000u;
            }
          }
        }
        if (checkCeilingFlat && sec->ceiling.pic) {
          VTexture *gtex = GTextureManager(sec->ceiling.pic);
          if (gtex && gtex->Type != TEXTYPE_Null && gtex->glowing) {
            gp.ceilingGlowHeight = 128.0f;
            gp.glowCC = gtex->glowing;
            gp.ceilingZ = sec->ceiling.GetPointZClamped(*surf->seg->v1);
            if (!gtex->IsGlowFullbright()) {
              // fix light level
              //gp.glowCC = (gp.glowCF&0x00ffffffu)|(((unsigned)surf->Light)&0xff000000u);
              gp.glowCC &= 0x00ffffffu;
            } else {
              gp.glowCC |= 0xff000000u;
            }
          }
        }
      }
    }
  }

public:
  GLint glGetUniLoc (const char *prog, GLhandleARB pid, const char *name, bool optional=false);
  GLint glGetAttrLoc (const char *prog, GLhandleARB pid, const char *name, bool optional=false);

private:
  vuint8 *readBackTempBuf;
  int readBackTempBufSize;

public:
  struct SurfListItem {
    surface_t *surf;
    surfcache_t *cache;
  };

private:
  TArray<SurfListItem> surfList;

  inline void surfListClear () { surfList.reset(); }

  inline void surfListAppend (surface_t *surf, surfcache_t *cache=nullptr) {
    SurfListItem &si = surfList.alloc();
    si.surf = surf;
    si.cache = cache;
  }

private:
  static inline float getSurfLightLevel (const surface_t *surf) {
    if (!surf) return 0;
    if (r_glow_flat && !surf->seg && surf->subsector) {
      const sector_t *sec = surf->subsector->sector;
      //FIXME: check actual view height here
      if (sec && !sec->heightsec) {
        if (sec->floor.pic && surf->GetNormalZ() > 0.0f) {
          VTexture *gtex = GTextureManager(sec->floor.pic);
          if (gtex && gtex->Type != TEXTYPE_Null && gtex->IsGlowFullbright()) return 1.0f;
        }
        if (sec->ceiling.pic && surf->GetNormalZ() < 0.0f) {
          VTexture *gtex = GTextureManager(sec->ceiling.pic);
          if (gtex && gtex->Type != TEXTYPE_Null && gtex->IsGlowFullbright()) return 1.0f;
        }
      }
    }
    return float((surf->Light>>24)&0xff)/255.0f;
  }

  static inline void glVertex (const TVec &v) { glVertex3f(v.x, v.y, v.z); }
  static inline void glVertex4 (const TVec &v, const float w) { glVertex4f(v.x, v.y, v.z, w); }

  struct CameraFBOInfo {
  public:
    FBO fbo;
    int texnum; // camera texture number for this FBO
    int camwidth, camheight; // camera texture dimensions for this FBO
    int index; // internal index of this FBO

  public:
    VV_DISABLE_COPY(CameraFBOInfo)
    CameraFBOInfo ();
    ~CameraFBOInfo ();
  };

protected:
  // for sprite VBOs
  /*
  struct __attribute__((packed)) SpriteVertex {
    float x, y, z, s, t;
  };
  */

protected:
  vuint8 *tmpImgBuf0;
  vuint8 *tmpImgBuf1;
  int tmpImgBufSize;

  // set by `SetTexture()` and company
  // non-scaled
  float tex_iw, tex_ih; // used in shaders, and in 2D drawer
  // scaled
  float tex_iw_sc, tex_ih_sc; // this is used in 2D drawer
  // scale
  float tex_scale_x, tex_scale_y; // used in sky rendering

  int lastgamma;
  //int CurrentFade; // not used anymore

  bool hasNPOT;
  bool hasBoundsTest; // GL_EXT_depth_bounds_test

  FBO mainFBO;
  FBO mainFBOFP; // if we're using FP color textures for main FBO, this will hold it
  FBO ambLightFBO; // we'll copy ambient light texture here, to use it in decal renderer to light decals
  FBO wipeFBO; // we'll copy main FBO here to render wipe transitions
  // the scheme is like this (for overbrights):
  //   in the main playerview renderer, we'll swap main FBO and main FP FBO
  //   then we'll render the scene
  //   then we'll fix overbright (this retains FP texture)
  //   then we'll finish rendering objects and textures
  //   then we'll blit FP texture to non-FP (it is saved in main FP FBO currently)
  //   then we'll swap main FBO and main FP FBO again
  // now our main FBO is non-FP, as it was before
  // this trickery is done so posteffect shaders could use non-FP color textures (for speed)

  // bloom
  FBO bloomscratchFBO, bloomscratch2FBO, bloomeffectFBO, bloomcoloraveragingFBO;
  // view (texture) camera updates will use this to render camera views
  // as reading from rendered texture is very slow, we will flip-flop FBOs,
  // using `current` to render new camera views, and `current^1` to get previous ones
  TArray<CameraFBOInfo *> cameraFBOList;
  // current "main" fbo: <0: `mainFBO`, otherwise camera FBO
  int currMainFBO;

  FBO postSrcFBO;
  FBO postOverFBO; // must be FP is main FBO is fp
  FBO postSrcFSFBO;
  FBO tempMainFBO; // used in FS posteffects

  // tonemap
  GLuint tonemapPalLUT; // palette LUT texture
  int tonemapLastGamma;
  int tonemapMode; // 0: 64x64x64, 1: 128x128x128
  int tonemapColorAlgo;

  GLint maxTexSize;

  GLuint lmap_id[NUM_BLOCK_SURFS];
  bool atlasesGenerated;
  vuint8 atlases_updated[NUM_BLOCK_SURFS];

  GLenum ClampToEdge;
  GLint max_anisotropy; // 1: off
  bool anisotropyExists;

  bool usingFPZBuffer;

  //bool HaveDepthClamp; // moved to drawer
  bool HaveStencilWrap;
  bool HaveS3TC;

  //TArray<GLhandleARB> CreatedShaderObjects;
  TArray<VMeshModel *> UploadedModels;

  template<typename T> class VBO {
  protected:
    class VOpenGLDrawer *mOwner;
    GLuint vboId;
    int vboSize; // current VBO size, in items
    int maxElems;
    int usedElems;
    bool isStream;

  public:
    TArray<T> data;

  public:
    inline VBO () noexcept : mOwner(nullptr), vboId(0), vboSize(0), maxElems(0), usedElems(0), isStream(false), data() {}
    inline VBO (VOpenGLDrawer *aOwner, bool aStream) noexcept : mOwner(aOwner), vboId(0), vboSize(0), maxElems(0), usedElems(0), isStream(aStream), data() {}

    static inline size_t getTypeSize () noexcept { return sizeof(T); }

    inline void setOwner (VOpenGLDrawer *aOwner, bool aStream=false) noexcept {
      if (mOwner != aOwner || isStream != aStream) {
        destroy();
        mOwner = aOwner;
        isStream = aStream;
      }
    }

    VBO (VOpenGLDrawer *aOwner, int aMaxElems, bool aStream=false) noexcept
      : mOwner(aOwner)
      , vboId(0)
      , vboSize(0)
      , maxElems(aMaxElems)
      , usedElems(0)
      , isStream(aStream)
      , data()
    {
      vassert(aMaxElems >= 0);
      if (aMaxElems == 0) return;
      data.setLength(aMaxElems);
      //memset((void *)data.ptr(), 0, sizeof(T)); // nobody cares
    }

    inline ~VBO () noexcept { destroy(); }

    inline void destroy () noexcept {
      data.clear();
      if (vboId) {
        if (mOwner) mOwner->p_glDeleteBuffersARB(1, &vboId);
        vboId = 0;
      }
      maxElems = usedElems = 0;
      vboSize = 0;
    }

    inline bool isValid () const noexcept { return (mOwner && vboId != 0); }
    inline GLuint getId () const noexcept { return vboId; }

    inline int capacity () const noexcept { return maxElems; }
    inline int dataUsed () const noexcept { return usedElems; }

    // *WARNING!* does no checks!
    inline void allocReset () noexcept { usedElems = 0; }
    // *WARNING!* does no checks!
    inline T *allocPtr () noexcept { return data.ptr()+(usedElems++); }

    // this may resize `data()` (but won't destroy VBO)
    // note that you should NOT store returned pointer, it may be invalidated by the next call to `allocPtrSafe()`
    inline T *allocPtrSafe () noexcept {
      if (usedElems >= data.length()) {
        data.setLengthReserve(usedElems+1);
        maxElems = data.capacity();
      }
      return data.ptr()+(usedElems++);
    }

    // this doesn't activate VBO
    // can delete VBO, so don't keep it active!
    // this also does `allocReset()`
    void ensureDataSize (int count, int extraReserve=-1) noexcept {
      if (!mOwner) Sys_Error("VBO: trying to ensure uninitialised VBO");
      const int oldlen = maxElems;
      if (count > oldlen) {
        count += (extraReserve >= 0 ? extraReserve : 42);
        maxElems = count;
        data.setLengthNoResize(count); // don't resize
        if (vboId && vboSize < count) { mOwner->p_glDeleteBuffersARB(1, &vboId); vboId = 0; vboSize = 0; }
      }
      usedElems = 0;
    }

    inline void deactivate () const noexcept {
      if (mOwner) mOwner->p_glBindBufferARB(GL_ARRAY_BUFFER, 0);
    }

    inline void activate () const noexcept {
      if (!mOwner) Sys_Error("cannot activate uninitialised VBO");
      if (!vboId) Sys_Error("cannot activate empty VBO");
      mOwner->p_glBindBufferARB(GL_ARRAY_BUFFER, vboId);
    }

    // this activates VBO (and allocates it if necessary)
    // `buf` must not be NULL, and must not be `data.ptr()` if `count` might trigger data reallocation
    void uploadBuffer (int count, const T *buf) noexcept {
      if (count <= 0) return;
      if (!mOwner) Sys_Error("VBO: trying to upload data to uninitialised VBO");
      if (!buf) Sys_Error("VBO: trying to upload data from NULL pointer");
      // delete VBO if it is too small
      if (vboId && vboSize > 0 && vboSize < count) {
        mOwner->p_glDeleteBuffersARB(1, &vboId);
        vboId = 0;
        vboSize = 0;
      }
      // check buffer size
      if (!vboId || vboSize < count) {
        GLDRW_RESET_ERROR();
        if (vboId) {
          vassert(vboSize == 0);
        } else {
          mOwner->p_glGenBuffersARB(1, &vboId);
          GLDRW_CHECK_ERROR("VBO: creation");
        }
        if (maxElems < count) {
          data.setLength(maxElems);
          maxElems = count;
        }
        vboSize = maxElems;
        mOwner->p_glBindBufferARB(GL_ARRAY_BUFFER, vboId);
        if (vboSize == count) {
          // allocate and upload
          mOwner->p_glBufferDataARB(GL_ARRAY_BUFFER, (int)sizeof(T)*vboSize, buf, (isStream ? GL_STREAM_DRAW : GL_DYNAMIC_DRAW));
        } else {
          // allocate full buffer, upload partial data
          mOwner->p_glBufferDataARB(GL_ARRAY_BUFFER, (int)sizeof(T)*vboSize, NULL, (isStream ? GL_STREAM_DRAW : GL_DYNAMIC_DRAW));
          mOwner->p_glBufferSubDataARB(GL_ARRAY_BUFFER, 0, (int)sizeof(T)*count, buf);
        }
      } else {
        vassert(vboId);
        vassert(vboSize >= count);
        mOwner->p_glBindBufferARB(GL_ARRAY_BUFFER, vboId);
        // upload partial data
        mOwner->p_glBufferSubDataARB(GL_ARRAY_BUFFER, 0, (int)sizeof(T)*count, buf);
      }
    }

    // this activates VBO (and allocates it if necessary)
    // `usedElems` is used as counter
    void uploadData () noexcept {
      vassert(usedElems <= maxElems);
      if (maxElems == 0) {
        maxElems = 4;
        data.setLength(4);
      }
      // check buffer size
      if (vboId && vboSize > 0 && vboSize < usedElems) {
        mOwner->p_glDeleteBuffersARB(1, &vboId);
        vboId = 0;
        vboSize = 0;
      }
      // check buffer size
      if (!vboId || vboSize < usedElems) {
        GLDRW_RESET_ERROR();
        if (vboId) {
          vassert(vboSize == 0);
        } else {
          mOwner->p_glGenBuffersARB(1, &vboId);
          GLDRW_CHECK_ERROR("VBO: creation");
        }
        vassert(data.capacity() >= maxElems);
        vboSize = maxElems;
        mOwner->p_glBindBufferARB(GL_ARRAY_BUFFER, vboId);
        // allocate and upload
        mOwner->p_glBufferDataARB(GL_ARRAY_BUFFER, (int)sizeof(T)*vboSize, data.ptr(), (isStream ? GL_STREAM_DRAW : GL_DYNAMIC_DRAW));
      } else {
        vassert(vboId);
        vassert(vboSize >= usedElems);
        mOwner->p_glBindBufferARB(GL_ARRAY_BUFFER, vboId);
        // upload partial data
        mOwner->p_glBufferSubDataARB(GL_ARRAY_BUFFER, 0, (int)sizeof(T)*usedElems, data.ptr());
      }
    }

    // VBO should be activated!
    // this enables attribute
    inline void setupAttrib (GLuint attrIdx, int elemCount, ptrdiff_t byteOfs=0) noexcept {
      mOwner->p_glEnableVertexAttribArrayARB(attrIdx);
      mOwner->p_glVertexAttribPointerARB(attrIdx, elemCount, GL_FLOAT, GL_FALSE, (sizeof(T) == sizeof(float)*(unsigned)elemCount ? 0 : sizeof(T)), (void *)byteOfs);
    }

    inline void setupAttribNoEnable (GLuint attrIdx, int elemCount, ptrdiff_t byteOfs=0) noexcept {
      mOwner->p_glVertexAttribPointerARB(attrIdx, elemCount, GL_FLOAT, GL_FALSE, (sizeof(T) == sizeof(float)*(unsigned)elemCount ? 0 : sizeof(T)), (void *)byteOfs);
    }

    // VBO should be activated!
    inline void enableAttrib (GLuint attrIdx) const noexcept { mOwner->p_glEnableVertexAttribArrayARB(attrIdx); }
    inline void disableAttrib (GLuint attrIdx) const noexcept { mOwner->p_glDisableVertexAttribArrayARB(attrIdx); }
  };

  // VBO for sprite rendering
  VBO<TVec> vboSprite;

  // VBO for masked surface rendering
  VBO<SurfVertex> vboMaskedSurf;

  // for VBOs
  struct __attribute__((packed)) SkyVBOVertex {
    float x, y, z;
    float s, t;
  };
  static_assert(sizeof(SkyVBOVertex) == sizeof(float)*5, "invalid SkyVBOVertex size");

  // VBO for sky rendering (created lazily, because we don't know the proper size initially)
  VBO<SkyVBOVertex> vboSky;

  // VBO for advrender surfaces
  // used on all stages
  // order: all solid surfaces (DrawSurfListSolid), all masked surfaces (DrawSurfListMasked)
  VBO<TVec> vboAdvSurf;
  TArray<GLsizei> vboCounters; // number of indicies in each primitive
  TArray<GLint> vboStartInds; // starting indicies

  void vboAdvAppendSurface (surface_t *surf);

  // for textured surfaces, so we can keep texture switching to minimum
  struct __attribute__((packed)) SMapVBOVertex {
    float x, y, z;
    float sx, sy, sz; // saxis
    float tx, ty, tz; // taxis
    float SOffs, TOffs;
    float TexIW, TexIH;
  };

  // VBO for shadowmap surfaces (including masked)
  // for each solid surface
  VBO<TVec> vboSMapSurf;
  TArray<GLsizei> vboSMapCounters; // number of indicies in each primitive
  TArray<GLint> vboSMapStartInds; // starting indicies

  // for each textured solid surface
  VBO<SMapVBOVertex> vboSMapSurfTex;
  TArray<GLsizei> vboSMapCountersTex; // number of indicies in each primitive
  TArray<GLint> vboSMapStartIndsTex; // starting indicies

  float vboSMapTexIW, vboSMapTexIH;
  VTexture *vboSMapTex;
  int vboSMapCountIdxTex;

  inline void vboSMapResetSurfacesTex () noexcept { vboSMapTex = nullptr; vboSMapTexIW = vboSMapTexIH = 1.0f; vboSMapCountIdxTex = 0; }
  void vboSMapAppendSurfaceTex (surface_t *surf, bool flip=false);

  // console variables
  static VCvarI texture_filter;
  static VCvarI sprite_filter;
  static VCvarI model_filter;
  static VCvarI gl_texture_filter_anisotropic;
  static VCvarB clear;
  static VCvarB ext_anisotropy;
  static VCvarI multisampling_sample;
  static VCvarB gl_smooth_particles;
  static VCvarB gl_dump_vendor;
  static VCvarB gl_dump_extensions;

  // extensions
  bool CheckExtension (const char *ext);
  virtual void *GetExtFuncPtr (const char *name) = 0;

  // was used to render skies
  //void SetFade (vuint32 NewFade);

  // returns 0 if generation is disabled, and atlas is not created
  void GenerateLightmapAtlasTextures ();
  void DeleteLightmapAtlases ();

  void GeneratePaletteLUT ();

  virtual void FlushOneTexture (VTexture *tex, bool forced=false) override; // unload one texture
  virtual void FlushTextures (bool forced=false) override; // unload all textures

  void DeleteTextures ();
  void FlushTexture (VTexture *);
  void DeleteTexture (VTexture *);

protected:
  enum SetTexType {
    TT_Common, // wall, flat, sometimes pic, etc.
    TT_Brightmap,
    TT_Sprite,
    TT_SpriteBrightmap,
    TT_PSprite,
    TT_PSpriteBrightmap,
    TT_Decal,
    TT_BloodDecal,
    TT_Pic,
    TT_Model, // textures for alias models
    TT_Shadow, // sprite texture for sprite shadow
  };

  // returns `false` if non-main texture was bound
  // `ttype` is used to override releasing and cropping modes
  // "non-main" means "translated, colormapped, or shaded"
  bool SetTextureLump (SetTexType ttype, VTexture *Tex, VTextureTranslation *Translation, int CMap, vuint32 ShadeColor=0);

  void GenerateTexture (SetTexType ttype, VTexture *Tex, GLuint *pHandle, VTextureTranslation *Translation, int CMap, bool needUpdate, vuint32 ShadeColor);

  enum { UpTexNoCompress = 0, UpTexNormal = 1, UpTexHiRes = 2 };
  void UploadTexture8 (int Width, int Height, const vuint8 *Data, const rgba_t *Pal, int SourceLump, int hitype);
  void UploadTexture8A (int Width, int Height, const pala_t *Data, const rgba_t *Pal, int SourceLump, int hitype);
  void UploadTexture (int width, int height, const rgba_t *data, bool doFringeRemove, int SourceLump, int hitype);

public:
  // texture must be bound as GL_TEXTURE_2D
  // wrap: <0: clamp; 0: default; 1: repeat
  enum {
    TexWrapClamp = -1,
    TexWrapDefault = 0,
    TexWrapRepeat = 1,
  };
  void SetupTextureFiltering (VTexture *Tex, int level, int wrap=TexWrapDefault);
  void SetupShadowTextureFiltering (VTexture *Tex);

  // will not change texture filter cache
  void ForceTextureFiltering (VTexture *Tex, int level, int wrap=TexWrapDefault);

  inline void SetOrForceTextureFiltering (bool mainTexture, VTexture *Tex, int level, int wrap=TexWrapDefault) {
    if (mainTexture) SetupTextureFiltering(Tex, level, wrap); else ForceTextureFiltering(Tex, level, wrap);
  }

  // if `ShadeColor` is not zero, ignore translation, and use "shaded" mode
  // high byte of `ShadeColor` means nothing
  // returns `false` if non-main texture was bound
  bool SetCommonTexture (VTexture *Tex, int CMap, vuint32 ShadeColor=0);
  bool SetDecalTexture (VTexture *Tex, VTextureTranslation *Translation, int CMap, bool isBloodSplat); // bloodsplats will be converted to pure red
  void SetBrightmapTexture (VTexture *Tex);
  void SetSpriteBrightmapTexture (SpriteType sptype, VTexture *Tex);
  bool SetPic (VTexture *Tex, VTextureTranslation *Trans, int CMap, vuint32 ShadeColor=0);
  bool SetPicModel (VTexture *Tex, VTextureTranslation *Trans, int CMap, vuint32 ShadeColor=0);

  inline void SetSpriteTexture (SpriteType sptype, int level, VTexture *Tex, VTextureTranslation *Translation, int CMap, vuint32 ShadeColor=0) {
    SetOrForceTextureFiltering(SetTextureLump((sptype == SpriteType::SP_PSprite ? SetTexType::TT_PSprite : SetTexType::TT_Sprite), Tex, Translation, CMap, ShadeColor), Tex, level, TexWrapClamp);
  }

  inline void SetShadowTexture (VTexture *Tex) {
    SetTextureLump(SetTexType::TT_Shadow, Tex, nullptr, 0/*CMap*/, 0/*ShadeColor*/);
    SetupShadowTextureFiltering(Tex);
  }

  void DoHorizonPolygon (surface_t *surf);
  void DrawPortalArea (VPortal *Portal);

  GLhandleARB LoadShader (VGLShader *gls, const char *progname, const char *incdir, GLenum Type, VStr FileName, const TArray<VStr> &defines=TArray<VStr>());
  GLhandleARB CreateProgram (VGLShader *gls, const char *progname, GLhandleARB VertexShader, GLhandleARB FragmentShader);

  virtual void UnloadAliasModel (VMeshModel *Mdl) override;
  void UploadModel (VMeshModel *Mdl);
  void UnloadModels ();

  void SetupBlending (const RenderStyleInfo &ri);
  void RestoreBlending (const RenderStyleInfo &ri);

private: // bloom
  int bloomWidth = 0, bloomHeight = 0, bloomMipmapCount = 0;

  bool bloomColAvgValid = false;
  GLuint bloomFullSizeDownsampleFBOid = 0;
  GLuint bloomFullSizeDownsampleRBOid = 0;
  GLuint bloomColAvgGetterPBOid = 0;
  int bloomScrWdt, bloomScrHgt;
  unsigned bloomCurrentFBO = 0;

  inline void bloomResetFBOs () noexcept { bloomCurrentFBO = 0; }
  inline void bloomSwapFBOs () noexcept { bloomCurrentFBO ^= 1; }
  inline FBO *bloomGetActiveFBO () noexcept { return (bloomCurrentFBO ? &bloomscratch2FBO : &bloomscratchFBO); }
  inline FBO *bloomGetInactiveFBO () noexcept { return (bloomCurrentFBO ? &bloomscratchFBO : &bloomscratch2FBO); }

  void BloomDeinit ();
  void BloomAllocRBO (int width, int height, GLuint *RBO, GLuint *FBO);
  void BloomInitEffectTexture ();
  void BloomInitTextures ();
  void BloomDownsampleView (int ax, int ay, int awidth, int aheight);
  void BloomDarken ();
  void BloomDoGaussian ();
  void BloomDrawEffect (int ax, int ay, int awidth, int aheight);

protected:
  // automatically called by `PreparePostXXX()`
  void SetFSPosteffectMode ();

  // this will force main FBO with `SetMainFBO(true);`
  void EnsurePostSrcFBO ();

  // this will setup matrices, ensure source FBO, and copy main FBO to postsrc FBO
  void PreparePostSrcFBO ();

  void EnsurePostSrcFSFBO ();

  // this will setup matrices, ensure source FBO, and copy main FBO to postsrc FBO
  void PreparePostSrcFSFBO ();

  // should be called after `PreparePostSrcFBO()`
  void RenderPostSrcFullscreenQuad ();

  // this will deactivate texture 0 and shaders
  void FinishPostSrcFBO ();

  void PostSrcSaveMatrices ();
  void PostSrcRestoreMatrices ();

  friend class PostSrcMatrixSaver;

public:
  virtual void Posteffect_Bloom (int ax, int ay, int awidth, int aheight) override;
  virtual void Posteffect_Tonemap (int ax, int ay, int awidth, int aheight, bool restoreMatrices) override;
  virtual void Posteffect_ColorMap (int cmap, int ax, int ay, int awidth, int aheight) override;
  virtual void Posteffect_Underwater (float time, int ax, int ay, int awidth, int aheight, bool restoreMatrices) override;
  virtual void Posteffect_CAS (float coeff, int ax, int ay, int awidth, int aheight, bool restoreMatrices) override;

  virtual void PrepareMainFBO () override;
  virtual void RestoreMainFBO () override;

  // contrary to the name, this must be called at the very beginning of the rendering pass, right after setting the matices
  virtual void PrepareForPosteffects () override;
  // call this after finishing all posteffects, to restore main FBO in the proper state
  // this is required because posteffects can attach non-FP texture to FBO
  virtual void FinishPosteffects () override;

  // the following posteffects should be called after the whole screen was rendered!
  virtual void Posteffect_ColorBlind (int mode) override;
  virtual void Posteffect_ColorMatrix (const float[16]) override;

  virtual void ApplyFullscreenPosteffects () override;
  virtual void FinishFullscreenPosteffects () override;

  virtual void LevelRendererCreated (VRenderLevelPublic *Renderer) override;
  virtual void LevelRendererDestroyed () override;

public:
  virtual void SetMainFBO (bool forced=false) override;

  virtual void ClearCameraFBOs () override;
  virtual int GetCameraFBO (int texnum, int width, int height) override; // returns index or -1; (re)creates FBO if necessary
  virtual int FindCameraFBO (int texnum) override; // returns index or -1
  virtual void SetCameraFBO (int cfboindex) override;
  virtual GLuint GetCameraFBOTextureId (int cfboindex) override; // returns 0 if cfboindex is invalid

  // this copies main FBO to wipe FBO, so we can run wipe shader
  virtual void PrepareWipe () override;
  // render wipe from wipe to main FBO
  // should be called after `StartUpdate()`
  // and (possibly) rendering something to the main FBO
  // time is in seconds, from zero to...
  // returns `false` if wipe is complete
  // -1 means "show saved wipe screen"
  virtual bool RenderWipe (float time) override;

  void DestroyCameraFBOList ();

  void ActivateMainFBO ();
  FBO *GetMainFBO ();

public:
  // calculate sky texture U/V (S/T)
  // texture must be selected
  inline float CalcSkyTexCoordS (const TVec vert, const texinfo_t *tex, const float offs) const noexcept {
    return (vert.dot(tex->saxis*tex_scale_x)+(tex->soffs-offs)*tex_scale_x)*tex_iw;
  }

  inline void CalcSkyTexCoordS2 (float *outs1, float *outs2, const TVec vert, const texinfo_t *tex, const float offs1, const float offs2) const noexcept {
    const float dp = vert.dot(tex->saxis*tex_scale_x);
    *outs1 = (dp+(tex->soffs-offs1)*tex_scale_x)*tex_iw;
    *outs2 = (dp+(tex->soffs-offs2)*tex_scale_x)*tex_iw;
  }

  inline float CalcSkyTexCoordT (const TVec vert, const texinfo_t *tex) const noexcept {
    return (vert.dot(tex->taxis*tex_scale_y)+tex->toffs*tex_scale_y)*tex_ih;
  }

public:
  // will be set to dummy functions if no debug API available, so it safe to use
  bool glDebugEnabled;
  GLint glMaxDebugLabelLen;
  VV_GLOBJECTLABEL p_glObjectLabel;
  VV_GLOBJECTPTRLABEL p_glObjectPtrLabel;
  VV_GLGETOBJECTLABEL p_glGetObjectLabel;
  VV_GLGETOBJECTPTRLABEL p_glGetObjectPtrLabel;

  void p_glObjectLabelVA (GLenum identifier, GLuint name, const char *fmt, ...) __attribute__((format(printf, 4, 5)));
  void p_glDebugLogf (const char *fmt, ...) __attribute__((format(printf, 2, 3)));

public:
  #define VV_GLIMPORTS
  #define VGLAPIPTR(x,optional)  x##_t p_##x
  #include "gl_imports.h"
  #undef VGLAPIPTR
  #undef VV_GLIMPORTS

  inline void SelectTexture (int level) { p_glActiveTextureARB(GLenum(GL_TEXTURE0_ARB+level)); }

  static inline void SetColor (vuint32 c) {
    glColor4ub((vuint8)((c>>16)&255), (vuint8)((c>>8)&255), (vuint8)(c&255), (vuint8)((c>>24)&255));
  }

  static const char *glTypeName (GLenum type) {
    switch (type) {
      case /*GL_BYTE*/ 0x1400: return "byte";
      case /*GL_UNSIGNED_BYTE*/ 0x1401: return "ubyte";
      case /*GL_SHORT*/ 0x1402: return "short";
      case /*GL_UNSIGNED_SHORT*/ 0x1403: return "ushort";
      case /*GL_INT*/ 0x1404: return "int";
      case /*GL_UNSIGNED_INT*/ 0x1405: return "uint";
      case /*GL_FLOAT*/ 0x1406: return "float";
      case /*GL_2_BYTES*/ 0x1407: return "byte2";
      case /*GL_3_BYTES*/ 0x1408: return "byte3";
      case /*GL_4_BYTES*/ 0x1409: return "byte4";
      case /*GL_DOUBLE*/ 0x140A: return "double";
      case /*GL_FLOAT_VEC2*/ 0x8B50: return "vec2";
      case /*GL_FLOAT_VEC3*/ 0x8B51: return "vec3";
      case /*GL_FLOAT_VEC4*/ 0x8B52: return "vec4";
      case /*GL_INT_VEC2*/ 0x8B53: return "ivec2";
      case /*GL_INT_VEC3*/ 0x8B54: return "ivec3";
      case /*GL_INT_VEC4*/ 0x8B55: return "ivec4";
      case /*GL_BOOL*/ 0x8B56: return "bool";
      case /*GL_BOOL_VEC2*/ 0x8B57: return "bvec2";
      case /*GL_BOOL_VEC3*/ 0x8B58: return "bvec3";
      case /*GL_BOOL_VEC4*/ 0x8B59: return "bvec4";
      case /*GL_FLOAT_MAT2*/ 0x8B5A: return "mat2";
      case /*GL_FLOAT_MAT3*/ 0x8B5B: return "mat3";
      case /*GL_FLOAT_MAT4*/ 0x8B5C: return "mat4";
      case /*GL_SAMPLER_1D*/ 0x8B5D: return "sampler1D";
      case /*GL_SAMPLER_2D*/ 0x8B5E: return "sampler2D";
      case /*GL_SAMPLER_3D*/ 0x8B5F: return "sampler3D";
      case /*GL_SAMPLER_CUBE*/ 0x8B60: return "samplerCube";
      case /*GL_SAMPLER_1D_SHADOW*/ 0x8B61: return "sampler1D_shadow";
      case /*GL_SAMPLER_2D_SHADOW*/ 0x8B62: return "sampler2D_shadow";
    }
    return "<unknown>";
  }
};


/*
float advLightGetMaxBias (const unsigned int shadowmapPOT) noexcept;
float advLightGetMinBias () noexcept;
float advLightGetMulBias () noexcept;
*/

#endif
