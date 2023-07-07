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
//**  Copyright (C) 2023 Ketmar Dark
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
#ifndef VCCMOD_SDLGL_HEADER_FILE
#define VCCMOD_SDLGL_HEADER_FILE

#include "../../../libs/imago/imago.h"
#include "../../vcc_run.h"

#if defined (VCCRUN_HAS_SDL) && defined(VCCRUN_HAS_OPENGL)
#ifdef VAVOOM_CUSTOM_SPECIAL_SDL
# include <SDL.h>
#else
# include <SDL2/SDL.h>
#endif
#ifdef USE_GLAD
# include "glad.h"
#else
# include <GL/gl.h>
# include <GL/glext.h>
#endif


// ////////////////////////////////////////////////////////////////////////// //
struct event_t;
class VFont;
class VOpenGLTexture;


// ////////////////////////////////////////////////////////////////////////// //
class VGLVideo : public VObject {
  DECLARE_ABSTRACT_CLASS(VGLVideo, VObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VGLVideo)

public:
  // (srcColor * <srcFactor>) <op> (dstColor * <dstFactor>)
  // If you want alpha blending use <srcFactor> of SRC_ALPHA and <dstFactor> of ONE_MINUS_SRC_ALPHA:
  //   (srcColor * srcAlpha) + (dstColor * (1-srcAlpha))
  enum {
    BlendNone, // disabled
    BlendNormal, // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    BlendBlend, // glBlendFunc(GL_SRC_ALPHA, GL_ONE)
    BlendFilter, // glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR)
    BlendInvert, // glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO)
    BlendParticle, // glBlendFunc(GL_DST_COLOR, GL_ZERO)
    BlendHighlight, // glBlendFunc(GL_DST_COLOR, GL_ONE);
    BlendDstMulDstAlpha, // glBlendFunc(GL_ZERO, GL_DST_ALPHA);
    InvModulate, // glBlendFunc(GL_ZERO, GL_SRC_COLOR)
    Premult, // glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    //
    BlendMax,
  };

  enum {
    BlendFunc_Add,
    BlendFunc_Sub,
    BlendFunc_SubRev,
    BlendFunc_Min,
    BlendFunc_Max,
  };

  enum {
    ZFunc_Never,
    ZFunc_Always,
    ZFunc_Equal,
    ZFunc_NotEqual,
    ZFunc_Less, // default
    ZFunc_LessEqual,
    ZFunc_Greater,
    ZFunc_GreaterEqual,
    ZFunc_Max,
  };

  enum {
    STC_Keep,
    STC_Zero,
    STC_Replace,
    STC_Incr,
    STC_IncrWrap,
    STC_Decr,
    STC_DecrWrap,
    STC_Invert,
  };

  enum {
    STC_Never,
    STC_Less,
    STC_LEqual,
    STC_Greater,
    STC_GEqual,
    STC_Equal,
    STC_NotEqual,
    STC_Always,
  };

  enum {
    CMask_Red   = 0x01,
    CMask_Green = 0x02,
    CMask_Blue  = 0x04,
    CMask_Alpha = 0x08,
  };

private:
  static bool mInited;
  static bool mPreInitWasDone;
  static int stencilBits;
  static int mWidth, mHeight;
  static bool doGLSwap;
  static bool doRefresh;
  static int quitSignal;
  static bool hasNPOT; // NPOT texture support
  static VMethod *onDrawVC;
  static VMethod *onEventVC;
  static VMethod *onNewFrameVC;
  static VMethod *onSwappedVC;

  static int currFrameTime;
  static uint64_t nextFrameTime;
  static int mBlendMode;
  static int mBlendFunc;

  static vuint32 colorARGB; // a==0: opaque
  static VFont *currFont;
  static bool smoothLine;
  static bool directMode;
  static bool depthTest;
  static bool depthWrite;
  static bool stencilEnabled;
  static int depthFunc;
  static int currZ;
  static int swapInterval;
  static bool texFiltering;
  static int colorMask;
  static int alphaTestFunc;
  static float alphaFuncVal;
  static bool in3dmode;
  friend class VOpenGLTexture;

  static float glPolyOfsFactor;
  static float glPolyOfsUnit;

  static bool dbgDumpOpenGLInfo;

public:
  static float currZFloat;

  static void forceGLTexFilter () noexcept;

private:
  static void forceColorMask () noexcept;

  static void forceAlphaFunc () noexcept;
  static void forceBlendFunc () noexcept;

  static inline bool getTexFiltering () noexcept { return texFiltering; }

  static inline void setTexFiltering (bool filterit) noexcept { texFiltering = filterit; }

  static void realizeZFunc () noexcept;
  static void realiseGLColor () noexcept;

  // returns `true` if drawing will have any effect
  static bool setupBlending () noexcept;

private:
  static void initMethods ();
  static void onNewFrame ();
  static void onDraw ();
  static void onSwapped ();
  static void onEvent (event_t &evt);

public:
  static bool canInit () noexcept;
  static bool hasOpenGL () noexcept;
  static bool isInitialized () noexcept;
  static int getWidth () noexcept;
  static int getHeight () noexcept;

  static void fuckfucksdl () noexcept;

  static void getMousePosition (int *mx, int *my);

  static bool open (VStr winname, int width, int height, int fullscreen);
  static void close () noexcept;

  static void clear (int rgb=0) noexcept;
  static void clearDepth (float val=1.0f) noexcept;
  static void clearStencil (int val=0) noexcept;
  static void clearColor (int rgb=0) noexcept;

  static void dispatchEvents ();
  static void runEventLoop ();

  static void setFont (VName fontname) noexcept;

  static inline void setColor (vuint32 clr) { if (colorARGB != clr) { colorARGB = clr; realiseGLColor(); } }
  static inline vuint32 getColor () { return colorARGB; }

  static inline int getBlendMode () { return mBlendMode; }
  static inline void setBlendMode (int v) { if (v >= BlendNormal && v <= BlendMax && v != mBlendMode) { mBlendMode = v; setupBlending(); } }

  static inline bool isFullyOpaque () { return ((colorARGB&0xff000000) == 0); }
  static inline bool isFullyTransparent () { return ((colorARGB&0xff000000) == 0xff000000); }

  static void drawTextAt (int x, int y, VStr text);
  // if clr high byte is ignored; 0 means "default color"
  static void drawTextAtTexture (VOpenGLTexture *tx, int x, int y, VStr text, const vuint32 clr=0);

  // returns timer id or 0
  // if id <= 0, creates new unique timer id
  // if interval is < 1, returns with error and won't create timer
  static int CreateTimerWithId (int id, int intervalms, bool oneshot=false) noexcept;
  static bool DeleteTimer (int id) noexcept; // `true`: deleted, `false`: no such timer
  static bool IsTimerExists (int id) noexcept;
  static bool IsTimerOneShot (int id) noexcept;
  static int GetTimerInterval (int id) noexcept; // 0: no such timer
  // returns success flag; won't do anything if interval is < 1
  static bool SetTimerInterval (int id, int intervalms) noexcept;

  static void sendPing () noexcept;
  //static void postSocketEvent (int code, int sockid, int data) noexcept;

  // called to notify `VCC_WaitEvent()` that some new event was posted
  static void VCC_PingEventLoop ();

  static void VCC_WaitEvents ();

  // process received event, called by the event loop
  static void VCC_ProcessEvent (event_t &ev, void *udata);

  static int getFrameTime () noexcept;
  static void setFrameTime (int newft) noexcept;

  // static
  //DECLARE_FUNCTION(performPreInit)
  DECLARE_FUNCTION(canInit)
  DECLARE_FUNCTION(hasOpenGL)
  DECLARE_FUNCTION(isInitialized)
  DECLARE_FUNCTION(screenWidth)
  DECLARE_FUNCTION(screenHeight)

  DECLARE_FUNCTION(getRealWindowSize)

  DECLARE_FUNCTION(isMouseCursorVisible)
  DECLARE_FUNCTION(hideMouseCursor)
  DECLARE_FUNCTION(showMouseCursor)

  DECLARE_FUNCTION(get_frameTime)
  DECLARE_FUNCTION(set_frameTime)

  DECLARE_FUNCTION(openScreen)
  DECLARE_FUNCTION(closeScreen)

  DECLARE_FUNCTION(setScale)

  DECLARE_FUNCTION(runEventLoop)

  DECLARE_FUNCTION(requestRefresh)
  DECLARE_FUNCTION(requestQuit)
  DECLARE_FUNCTION(resetQuitRequest)

  DECLARE_FUNCTION(forceSwap)

  DECLARE_FUNCTION(glHasExtension)

  DECLARE_FUNCTION(get_dbgDumpOpenGLInfo)
  DECLARE_FUNCTION(set_dbgDumpOpenGLInfo)

  DECLARE_FUNCTION(get_glHasNPOT)

  DECLARE_FUNCTION(get_directMode)
  DECLARE_FUNCTION(set_directMode)

  DECLARE_FUNCTION(get_depthTest)
  DECLARE_FUNCTION(set_depthTest)

  DECLARE_FUNCTION(get_depthWrite)
  DECLARE_FUNCTION(set_depthWrite)

  DECLARE_FUNCTION(get_depthFunc)
  DECLARE_FUNCTION(set_depthFunc)

  DECLARE_FUNCTION(get_currZFloat)
  DECLARE_FUNCTION(get_currZ)
  DECLARE_FUNCTION(set_currZ)

  DECLARE_FUNCTION(get_scissorEnabled)
  DECLARE_FUNCTION(set_scissorEnabled)
  DECLARE_FUNCTION(getScissor)
  DECLARE_FUNCTION(setScissor)
  DECLARE_FUNCTION(copyScissor)

  enum {
    GLMatrixMode_ModelView,
    GLMatrixMode_Projection,
  };
  DECLARE_FUNCTION(set_glMatrixMode)

  DECLARE_FUNCTION(glPushMatrix)
  DECLARE_FUNCTION(glPopMatrix)
  DECLARE_FUNCTION(glLoadIdentity)
  DECLARE_FUNCTION(glScale)
  DECLARE_FUNCTION(glTranslate)
  DECLARE_FUNCTION(glRotate)
  DECLARE_FUNCTION(glGetMatrix)
  DECLARE_FUNCTION(glSetMatrix)

  DECLARE_FUNCTION(glSetup2D)
  DECLARE_FUNCTION(glSetup3D)

  enum {
    GLFaceCull_None,
    GLFaceCull_Front,
    GLFaceCull_Back,
  };
  DECLARE_FUNCTION(set_glCullFace)

  DECLARE_FUNCTION(glSetTexture)
  DECLARE_FUNCTION(glSetColor)

  enum {
    GLBegin_Points,
    GLBegin_Lines,
    GLBegin_LineStrip,
    GLBegin_LineLoop,
    GLBegin_Triangles,
    GLBegin_TriangleStrip,
    GLBegin_TriangleFan,
    GLBegin_Quads,
    GLBegin_QuadStrip,
    GLBegin_Polygon,
  };

  DECLARE_FUNCTION(glBegin)
  DECLARE_FUNCTION(glEnd)
  DECLARE_FUNCTION(glVertex)

  enum {
    FM_None,
    FM_Linear,
    FM_Exp,
    FM_Exp2,
  };

  DECLARE_FUNCTION(set_glFogMode)
  DECLARE_FUNCTION(set_glFogDensity)
  DECLARE_FUNCTION(set_glFogStart)
  DECLARE_FUNCTION(set_glFogEnd)
  DECLARE_FUNCTION(glSetFogColor)

  DECLARE_FUNCTION(get_glPolygonOffsetFactor)
  DECLARE_FUNCTION(get_glPolygonOffsetUnits)
  DECLARE_FUNCTION(glPolygonOffset)

  DECLARE_FUNCTION(clearScreen)
  DECLARE_FUNCTION(clearDepth)
  DECLARE_FUNCTION(clearStencil)
  DECLARE_FUNCTION(clearColor)

  DECLARE_FUNCTION(get_stencil)
  DECLARE_FUNCTION(set_stencil)
  DECLARE_FUNCTION(stencilOp)
  DECLARE_FUNCTION(stencilFunc)

  DECLARE_FUNCTION(get_alphaTestFunc)
  DECLARE_FUNCTION(set_alphaTestFunc)
  DECLARE_FUNCTION(get_alphaTestVal)
  DECLARE_FUNCTION(set_alphaTestVal)

  DECLARE_FUNCTION(get_realStencilBits)
  DECLARE_FUNCTION(get_framebufferHasAlpha)

  DECLARE_FUNCTION(get_smoothLine)
  DECLARE_FUNCTION(set_smoothLine)

  DECLARE_FUNCTION(get_colorARGB) // aarrggbb
  DECLARE_FUNCTION(set_colorARGB) // aarrggbb

  DECLARE_FUNCTION(get_blendMode)
  DECLARE_FUNCTION(set_blendMode)

  DECLARE_FUNCTION(get_blendFunc)
  DECLARE_FUNCTION(set_blendFunc)

  DECLARE_FUNCTION(get_colorMask)
  DECLARE_FUNCTION(set_colorMask)

  DECLARE_FUNCTION(get_textureFiltering)
  DECLARE_FUNCTION(set_textureFiltering)

  DECLARE_FUNCTION(get_swapInterval)
  DECLARE_FUNCTION(set_swapInterval)

  DECLARE_FUNCTION(get_fontName)
  DECLARE_FUNCTION(set_fontName)

  DECLARE_FUNCTION(loadFontDF)
  DECLARE_FUNCTION(loadFontPCF)

  DECLARE_FUNCTION(fontHeight)
  DECLARE_FUNCTION(getCharInfo)
  DECLARE_FUNCTION(charWidth)
  DECLARE_FUNCTION(spaceWidth)
  DECLARE_FUNCTION(textWidth)
  DECLARE_FUNCTION(textHeight)
  DECLARE_FUNCTION(drawTextAt)
  DECLARE_FUNCTION(drawTextAtTexture)

  DECLARE_FUNCTION(drawLine)
  DECLARE_FUNCTION(drawRect)
  DECLARE_FUNCTION(fillRect)

  DECLARE_FUNCTION(getMousePos)
};


// ////////////////////////////////////////////////////////////////////////// //
// refcounted object
class VOpenGLTexture {
private:
  mutable int rc;
  VStr mPath;

public:
  struct TexQuad {
    int x0, y0, x1, y1;
    float tx0, ty0, tx1, ty1;
  };

  inline VStr getPath () const noexcept { return mPath; }
  inline int getRC () const noexcept { return rc; }

public:
  enum {
    TT_Auto, // do analysis
    TT_Opaque, // no alpha
    TT_OneBitAlpha, // only 0 and 255 alpha
    TT_Translucent, // has alpha different from 0 and from 255
    TT_Transparent, // all pixels has alpha of 0
  };

public:
  VImage *img;
  GLuint tid; // !0: texture loaded
  unsigned type; // TT_xxx
  unsigned realType; // TT_xxx, after analysis
  bool needSmoothing; // default is true
  VOpenGLTexture *prev;
  VOpenGLTexture *next;

  inline bool getTransparent () const noexcept { return (realType == TT_Transparent); }
  inline bool getOpaque () const noexcept { return (realType == TT_Opaque); }
  inline bool get1BitAlpha () const noexcept { return (realType == TT_OneBitAlpha); }

  inline unsigned getType () const noexcept { return type; }
  inline unsigned getRealType () const noexcept { return realType; }

private:
  void registerMe () noexcept;
  void analyzeImage () noexcept;

  VOpenGLTexture (int awdt, int ahgt, bool doSmoothing=true); // dimensions must be valid!

public:
  VOpenGLTexture (VImage *aimg, VStr apath, bool doSmoothing=true);
  ~VOpenGLTexture (); // don't call this manually!

  VVA_FORCEINLINE void addRef () const noexcept { ++rc; }

  //WARNING: can delete `this`!
  VVA_FORCEINLINE void release () noexcept { if (--rc == 0) delete this; }

  void convertToTrueColor ();

  void update ();

  void reanalyzeImage () noexcept;

  static VOpenGLTexture *Load (VStr fname, bool doSmoothing=true);
  static VOpenGLTexture *CreateEmpty (VName txname, int wdt, int hgt, bool doSmoothing=true);

  inline int getWidth () const noexcept { return (img ? img->width : 0); }
  inline int getHeight () const noexcept { return (img ? img->height : 0); }

  PropertyRO<VStr, VOpenGLTexture> path {this, &VOpenGLTexture::getPath};
  PropertyRO<int, VOpenGLTexture> width {this, &VOpenGLTexture::getWidth};
  PropertyRO<int, VOpenGLTexture> height {this, &VOpenGLTexture::getHeight};

  // that is:
  //  isTransparent==true: no need to draw anything
  //  isOpaque==true: no need to do alpha-blending
  //  isOneBitAlpha==true: no need to do advanced alpha-blending

  PropertyRO<bool, VOpenGLTexture> isTransparent {this, &VOpenGLTexture::getTransparent};
  PropertyRO<bool, VOpenGLTexture> isOpaque {this, &VOpenGLTexture::getOpaque};
  // this means that the only alpha values are 255 or 0
  PropertyRO<bool, VOpenGLTexture> isOneBitAlpha {this, &VOpenGLTexture::get1BitAlpha};

  // angle is in degrees
  void blitExt (int dx0, int dy0, int dx1, int dy1, int x0, int y0, int x1, int y1, float angle) const noexcept;
  void blitAt (int dx0, int dy0, float scale=1, float angle=0) const noexcept;

  // this uses integer texture coords
  void blitExtRep (int dx0, int dy0, int dx1, int dy1, int x0, int y0, int x1, int y1) const noexcept;

  void blitWithLightmap (TexQuad *t0, VOpenGLTexture *lmap, TexQuad *t1) const noexcept;
};


// ////////////////////////////////////////////////////////////////////////// //
// VavoomC wrapper
class VGLTexture : public VObject {
  DECLARE_CLASS(VGLTexture, VObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VGLTexture)

  friend class VGLVideo;

public:
  static bool savingAllowed;

private:
  VOpenGLTexture *tex;
  int id;

public:
  virtual void Destroy () override;

public:
  DECLARE_FUNCTION(Destroy)
  DECLARE_FUNCTION(Load) // native final static GLTexture Load (string fname);
  DECLARE_FUNCTION(GetById); // native final static GLTexture GetById (int id);
  DECLARE_FUNCTION(width)
  DECLARE_FUNCTION(height)
  DECLARE_FUNCTION(isTransparent)
  DECLARE_FUNCTION(isOpaque)
  DECLARE_FUNCTION(isOneBitAlpha)
  DECLARE_FUNCTION(blitExt)
  DECLARE_FUNCTION(blitExtRep)
  DECLARE_FUNCTION(blitAt)

  DECLARE_FUNCTION(blitWithLightmap)

  DECLARE_FUNCTION(CreateEmpty) // native final static GLTexture CreateEmpty (int wdt, int hgt, optional name txname);
  DECLARE_FUNCTION(setPixel) // native final static void setPixel (int x, int y, int argb); // aarrggbb; a==0 is completely opaque
  DECLARE_FUNCTION(getPixel) // native final static int getPixel (int x, int y); // aarrggbb; a==0 is completely opaque
  DECLARE_FUNCTION(upload) // native final static void upload ();
  DECLARE_FUNCTION(smoothEdges) // native final void smoothEdges (); // call after manual texture building

  DECLARE_FUNCTION(get_textureType);
  DECLARE_FUNCTION(set_textureType);
  DECLARE_FUNCTION(get_textureRealType);
  DECLARE_FUNCTION(set_textureRealType);

  DECLARE_FUNCTION(get_needSmoothing);
  DECLARE_FUNCTION(set_needSmoothing);

  DECLARE_FUNCTION(saveAsPNG)

  DECLARE_FUNCTION(fillRect)
  DECLARE_FUNCTION(hline)
  DECLARE_FUNCTION(vline)

  friend class VFont;
};


// ////////////////////////////////////////////////////////////////////////// //
// base class for fonts
class VFont {
protected:
  static VFont *fontList;

public:
  struct FontChar {
    vint32 ch;
    vint32 width, height; // height may differ from font height
    //vint32 advance; // horizontal advance to print next char
    vint32 topofs; // offset from font top (i.e. y+topofs should be used to draw char)
    vint32 leftbear, rightbear; // positive means "more to the respective bbox side"
    vint32 ascent, descent; // both are positive, which actually means "offset from baseline to the respective direction"
    float tx0, ty0; // texture coordinates, [0..1)
    float tx1, ty1; // texture coordinates, [0..1) -- cached for convenience
    VOpenGLTexture *tex; // don't destroy this!

    inline FontChar () noexcept : ch(-1), width(0), height(0), tex(nullptr) {}
  };

public:
  VName name;
  VFont *next;
  VOpenGLTexture *tex;
  bool singleTexture;

  // font characters (cp1251)
  TArray<FontChar> chars;
  vint32 spaceWidth; // width of the space character
  vint32 fontHeight; // height of the font
  vint32 minWidth, maxWidth, avgWidth;

protected:
  VFont (); // this inits nothing, and intended to be used in `LoadXXX()`

public:
  //VFont (VName aname, VStr fnameIni, VStr fnameTexture);
  ~VFont ();

  static VFont *LoadDF (VName aname, VStr fnameIni, VStr fnameTexture);
  static VFont *LoadPCF (VName aname, VStr filename);

  const FontChar *getChar (vint32 ch) const noexcept;
  vint32 charWidth (vint32 ch) const noexcept;
  vint32 textWidth (VStr s) const noexcept;
  vint32 textHeight (VStr s) const noexcept;
  // will clear lines; returns maximum text width
  //int splitTextWidth (VStr text, TArray<VSplitLine> &lines, int maxWidth) const;

  inline VName getName () const noexcept { return name; }
  inline vint32 getSpaceWidth () const noexcept { return spaceWidth; }
  inline vint32 getHeight () const noexcept { return fontHeight; }
  inline vint32 getMinWidth () const noexcept { return minWidth; }
  inline vint32 getMaxWidth () const noexcept { return maxWidth; }
  inline vint32 getAvgWidth () const noexcept { return avgWidth; }
  inline const VOpenGLTexture *getTexture () const noexcept { return tex; }

public:
  static VFont *findFont (VName name) noexcept;
};


#endif

#endif
