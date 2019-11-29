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
//**  OpenGL driver, main module
//**
//**************************************************************************
#include <limits.h>
#include <float.h>
#include <stdarg.h>

#include "gl_local.h"
#include "../r_local.h" /* for VRenderLevelShared */


// ////////////////////////////////////////////////////////////////////////// //
extern VCvarB r_bloom;

VCvarB gl_pic_filtering("gl_pic_filtering", false, "Filter interface pictures.", CVAR_Archive);
VCvarB gl_font_filtering("gl_font_filtering", false, "Filter 2D interface.", CVAR_Archive);
VCvarF gl_maxdist("gl_maxdist", "8192", "Max view distance (too big values will cause z-buffer issues).", CVAR_Archive);

static VCvarB gl_enable_fp_zbuffer("gl_enable_fp_zbuffer", false, "Enable using of floating-point depth buffer for OpenGL3+?", CVAR_Archive|CVAR_PreInit);
static VCvarB gl_enable_reverse_z("gl_enable_reverse_z", true, "Allow using \"reverse z\" trick?", CVAR_Archive|CVAR_PreInit);
static VCvarB gl_enable_clip_control("gl_enable_clip_control", true, "Allow using `glClipControl()`?", CVAR_Archive|CVAR_PreInit);
static VCvarB gl_dbg_force_reverse_z("gl_dbg_force_reverse_z", false, "Force-enable reverse z when fp depth buffer is not available.", CVAR_PreInit);
static VCvarB gl_dbg_ignore_gpu_blacklist("gl_dbg_ignore_gpu_blacklist", false, "Ignore GPU blacklist, and don't turn off features?", CVAR_PreInit);
static VCvarB gl_dbg_force_gpu_blacklisting("gl_dbg_force_gpu_blacklisting", false, "Force GPU to be blacklisted.", CVAR_PreInit);
static VCvarB gl_dbg_disable_depth_clamp("gl_dbg_disable_depth_clamp", false, "Disable depth clamping.", CVAR_PreInit);

VCvarI VOpenGLDrawer::texture_filter("gl_texture_filter", "0", "Texture filtering mode.", CVAR_Archive);
VCvarI VOpenGLDrawer::sprite_filter("gl_sprite_filter", "0", "Sprite filtering mode.", CVAR_Archive);
VCvarI VOpenGLDrawer::model_filter("gl_model_filter", "0", "Model filtering mode.", CVAR_Archive);
VCvarI VOpenGLDrawer::gl_texture_filter_anisotropic("gl_texture_filter_anisotropic", "1", "Texture anisotropic filtering (<=1 is off).", CVAR_Archive);
VCvarB VOpenGLDrawer::clear("gl_clear", true, "Clear screen before rendering new frame?", CVAR_Archive);
VCvarB VOpenGLDrawer::ext_anisotropy("gl_ext_anisotropy", true, "Use OpenGL anisotropy extension (if present)?", CVAR_Archive|CVAR_PreInit);
VCvarI VOpenGLDrawer::multisampling_sample("gl_multisampling_sample", "1", "Multisampling mode.", CVAR_Archive);
VCvarB VOpenGLDrawer::gl_smooth_particles("gl_smooth_particles", false, "Draw smooth particles?", CVAR_Archive);

VCvarB VOpenGLDrawer::gl_dump_vendor("gl_dump_vendor", false, "Dump OpenGL vendor?", CVAR_PreInit);
VCvarB VOpenGLDrawer::gl_dump_extensions("gl_dump_extensions", false, "Dump available OpenGL extensions?", CVAR_PreInit);

// was 0.333
VCvarF gl_alpha_threshold("gl_alpha_threshold", "0.01", "Alpha threshold (less than this will not be drawn).", CVAR_Archive);

static VCvarI gl_max_anisotropy("gl_max_anisotropy", "1", "Maximum anisotropy level (r/o).", CVAR_Rom);
static VCvarB gl_is_shitty_gpu("gl_is_shitty_gpu", true, "Is shitty GPU detected (r/o)?", CVAR_Rom);

VCvarB gl_enable_depth_bounds("gl_enable_depth_bounds", true, "Use depth bounds extension if found?", CVAR_Archive);

VCvarB gl_sort_textures("gl_sort_textures", true, "Sort surfaces by their textures (slightly faster on huge levels)?", CVAR_Archive|CVAR_PreInit);

VCvarB r_decals_wall_masked("r_decals_wall_masked", true, "Render decals on masked walls?", CVAR_Archive);
VCvarB r_decals_wall_alpha("r_decals_wall_alpha", true, "Render decals on translucent walls?", CVAR_Archive);

VCvarB gl_decal_debug_nostencil("gl_decal_debug_nostencil", false, "Don't touch this!", 0);
VCvarB gl_decal_debug_noalpha("gl_decal_debug_noalpha", false, "Don't touch this!", 0);
VCvarB gl_decal_dump_max("gl_decal_dump_max", false, "Don't touch this!", 0);
VCvarB gl_decal_reset_max("gl_decal_reset_max", false, "Don't touch this!", 0);

VCvarB gl_dbg_adv_render_surface_textures("gl_dbg_adv_render_surface_textures", true, "Render surface textures in advanced renderer?", CVAR_PreInit);
VCvarB gl_dbg_adv_render_surface_fog("gl_dbg_adv_render_surface_fog", true, "Render surface fog in advanced renderer?", CVAR_PreInit);
// this makes shadows glitch for some reason with fp z-buffer (investigate!)
VCvarB gl_dbg_adv_render_offset_shadow_volume("gl_dbg_adv_render_offset_shadow_volume", false, "Offset shadow volumes?", CVAR_PreInit);
VCvarB gl_dbg_adv_render_never_offset_shadow_volume("gl_dbg_adv_render_never_offset_shadow_volume", false, "Never offseting shadow volumes?", CVAR_Archive|CVAR_PreInit);

VCvarB gl_dbg_render_stack_portal_bounds("gl_dbg_render_stack_portal_bounds", false, "Render sector stack portal bounds.", 0);

VCvarB gl_use_stencil_quad_clear("gl_use_stencil_quad_clear", false, "Draw quad to clear stencil buffer instead of 'glClear'?", CVAR_Archive|CVAR_PreInit);

// 1: normal; 2: 1-skewed
VCvarI gl_dbg_use_zpass("gl_dbg_use_zpass", "0", "DO NOT USE!", CVAR_PreInit);

VCvarB gl_dbg_advlight_debug("gl_dbg_advlight_debug", false, "Draw non-fading lights?", CVAR_PreInit);
VCvarS gl_dbg_advlight_color("gl_dbg_advlight_color", "0xff7f7f", "Color for debug lights (only dec/hex).", CVAR_PreInit);

VCvarB gl_dbg_wireframe("gl_dbg_wireframe", false, "Render wireframe level?", CVAR_PreInit);

VCvarB gl_dbg_fbo_blit_with_texture("gl_dbg_fbo_blit_with_texture", false, "Always blit FBOs using texture mapping?", CVAR_PreInit);

VCvarB r_brightmaps("r_brightmaps", true, "Allow brightmaps?", CVAR_Archive);
VCvarB r_brightmaps_sprite("r_brightmaps_sprite", true, "Allow sprite brightmaps?", CVAR_Archive);
VCvarB r_brightmaps_additive("r_brightmaps_additive", true, "Are brightmaps additive, or max?", CVAR_Archive);
VCvarB r_brightmaps_filter("r_brightmaps_filter", false, "Do bilinear filtering on brightmaps?", CVAR_Archive);

VCvarB r_glow_flat("r_glow_flat", true, "Allow glowing flats?", CVAR_Archive);

VCvarB gl_lmap_allow_partial_updates("gl_lmap_allow_partial_updates", true, "Allow partial updates of lightmap atlases (this is usually faster than full updates)?", CVAR_Archive);


//==========================================================================
//
//  MSA
//
//==========================================================================
/*
static __attribute__((sentinel)) TArray<VStr> MSA (const char *first, ...) {
  TArray<VStr> res;
  res.append(VStr(first));
  va_list ap;
  va_start(ap, first);
  for (;;) {
    const char *str = va_arg(ap, const char *);
    if (!str) break;
    res.append(VStr(str));
  }
  return res;
}
*/


//==========================================================================
//
//  CheckVendorString
//
//  both strings should be lower-cased
//  `vs` is what we got from OpenGL
//  `fuckedName` is what we are looking for
//
//==========================================================================
static __attribute__((unused)) bool CheckVendorString (VStr vs, const char *fuckedName) {
  if (vs.length() == 0) return false;
  if (!fuckedName || !fuckedName[0]) return false;
  const int fnlen = (int)strlen(fuckedName);
  //GCon->Logf(NAME_Init, "VENDOR: <%s>", *vs);
  while (vs.length()) {
    auto idx = vs.indexOf(fuckedName);
    if (idx < 0) break;
    bool startok = (idx == 0 || !VStr::isAlphaAscii(vs[idx-1]));
    bool endok = (idx+fnlen >= vs.length() || !VStr::isAlphaAscii(vs[idx+fnlen]));
    if (startok && endok) return true;
    vs.chopLeft(idx+fnlen);
    //GCon->Logf(NAME_Init, "  XXX: <%s>", *vs);
  }
  return false;
}


//==========================================================================
//
//  VOpenGLDrawer::glGetUniLoc
//
//==========================================================================
GLint VOpenGLDrawer::glGetUniLoc (const char *prog, GLhandleARB pid, const char *name, bool optional) {
  vassert(name);
  if (!pid) Sys_Error("shader program '%s' not loaded", prog);
  (void)glGetError(); // reset error flag
  GLint res = p_glGetUniformLocationARB(pid, name);
  //if (glGetError() != 0 || res == -1) Sys_Error("shader program '%s' has no uniform '%s'", prog, name);
  if (optional) {
    if (glGetError() != 0 || res == -1) res = -1;
  } else {
    if (glGetError() != 0 || res == -1) GCon->Logf(NAME_Error, "shader program '%s' has no uniform '%s'", prog, name);
  }
  return res;
}


//==========================================================================
//
//  VOpenGLDrawer::glGetAttrLoc
//
//==========================================================================
GLint VOpenGLDrawer::glGetAttrLoc (const char *prog, GLhandleARB pid, const char *name, bool optional) {
  vassert(name);
  if (!pid) Sys_Error("shader program '%s' not loaded", prog);
  (void)glGetError(); // reset error flag
  GLint res = p_glGetAttribLocationARB(pid, name);
  //if (glGetError() != 0 || res == -1) Sys_Error("shader program '%s' has no attribute '%s'", prog, name);
  if (optional) {
    if (glGetError() != 0 || res == -1) res = -1;
  } else {
    if (glGetError() != 0 || res == -1) GCon->Logf(NAME_Error, "shader program '%s' has no attribute '%s'", prog, name);
  }
  return res;
}



//==========================================================================
//
//  VOpenGLDrawer::VGLShader::Setup
//
//==========================================================================
void VOpenGLDrawer::VGLShader::MainSetup (VOpenGLDrawer *aowner, const char *aprogname, const char *aincdir, const char *avssrcfile, const char *afssrcfile) {
  next = nullptr;
  owner = aowner;
  progname = aprogname;
  incdir = aincdir;
  vssrcfile = avssrcfile;
  fssrcfile = afssrcfile;
  prog = -1;
  owner->registerShader(this);
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShader::Activate
//
//==========================================================================
void VOpenGLDrawer::VGLShader::Activate () {
  owner->p_glUseProgramObjectARB(prog);
  owner->currentActiveShader = this;
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShader::Deactivate
//
//==========================================================================
void VOpenGLDrawer::VGLShader::Deactivate () {
  owner->p_glUseProgramObjectARB(0);
  owner->currentActiveShader = nullptr;
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShader::Compile
//
//==========================================================================
void VOpenGLDrawer::VGLShader::Compile () {
  if (developer) GCon->Logf(NAME_Dev, "compiling shader '%s'...", progname);
  GLhandleARB VertexShader = owner->LoadShader(progname, incdir, GL_VERTEX_SHADER_ARB, vssrcfile, defines);
  GLhandleARB FragmentShader = owner->LoadShader(progname, incdir, GL_FRAGMENT_SHADER_ARB, fssrcfile, defines);
  prog = owner->CreateProgram(progname, VertexShader, FragmentShader);
  LoadUniforms();
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShader::Unload
//
//==========================================================================
void VOpenGLDrawer::VGLShader::Unload () {
  if (developer) GCon->Logf(NAME_Dev, "unloading shader '%s'...", progname);
  // actual program object will be destroyed elsewhere
  prog = 0;
  UnloadUniforms();
  if (owner && owner->currentActiveShader == this) owner->currentActiveShader = nullptr;
}


#include "gl_shaddef.ci"


//**************************************************************************
//
//  VOpenGLDrawer::CameraFBOInfo
//
//**************************************************************************

//==========================================================================
//
//  CameraFBOInfo::CameraFBOInfo
//
//==========================================================================
VOpenGLDrawer::CameraFBOInfo::CameraFBOInfo ()
  : fbo()
  , texnum(-1)
  , camwidth(1)
  , camheight(1)
  , index(-1)
{}


//==========================================================================
//
//  CameraFBOInfo::~CameraFBOInfo
//
//==========================================================================
VOpenGLDrawer::CameraFBOInfo::~CameraFBOInfo () {
  texnum = -1;
  camwidth = camheight = 1;
  index = -1;
}


//**************************************************************************
//
//  VOpenGLDrawer
//
//**************************************************************************

//==========================================================================
//
//  VOpenGLDrawer::VOpenGLDrawer
//
//==========================================================================
VOpenGLDrawer::VOpenGLDrawer ()
  : VDrawer()
  , shaderHead(nullptr)
  , surfList()
  , mainFBO()
  , ambLightFBO()
  , wipeFBO()
  , cameraFBOList()
  , currMainFBO(-1)
{
  currentActiveShader = nullptr;
  lastgamma = 0;
  CurrentFade = 0;

  MaxTextureUnits = 1;
  useReverseZ = false;
  hasNPOT = false;
  hasBoundsTest = false;

  tmpImgBuf0 = nullptr;
  tmpImgBuf1 = nullptr;
  tmpImgBufSize = 0;

  readBackTempBuf = nullptr;
  readBackTempBufSize = 0;

  decalUsedStencil = false;
  decalStcVal = 255; // next value for stencil buffer (clear on the first use, and clear on each wrap)
  stencilBufferDirty = true; // just in case
  isShittyGPU = true; // let's play safe
  shittyGPUCheckDone = false; // just in case
  atlasesGenerated = false;
  currentActiveFBO = nullptr;
  blendEnabled = false;
  //cameraFBO[0].mOwner = nullptr;
  //cameraFBO[1].mOwner = nullptr;
}


//==========================================================================
//
//  VOpenGLDrawer::~VOpenGLDrawer
//
//==========================================================================
VOpenGLDrawer::~VOpenGLDrawer () {
  currentActiveFBO = nullptr;
  surfList.clear();
  if (tmpImgBuf0) { Z_Free(tmpImgBuf0); tmpImgBuf0 = nullptr; }
  if (tmpImgBuf1) { Z_Free(tmpImgBuf1); tmpImgBuf1 = nullptr; }
  tmpImgBufSize = 0;
  if (readBackTempBuf) { Z_Free(readBackTempBuf); readBackTempBuf = nullptr; }
  readBackTempBufSize = 0;
}


//==========================================================================
//
//  VOpenGLDrawer::DestroyCameraFBOList
//
//==========================================================================
void VOpenGLDrawer::DestroyCameraFBOList () {
  for (auto &&cf : cameraFBOList) { delete cf; cf = nullptr; }
  cameraFBOList.clear();
}


//==========================================================================
//
//  VOpenGLDrawer::registerShader
//
//==========================================================================
void VOpenGLDrawer::registerShader (VGLShader *shader) {
  if (developer) GCon->Logf(NAME_Dev, "registering shader '%s'...", shader->progname);
  shader->owner = this;
  shader->next = shaderHead;
  shaderHead = shader;
}


//==========================================================================
//
//  VOpenGLDrawer::CompileShaders
//
//==========================================================================
void VOpenGLDrawer::CompileShaders () {
  for (VGLShader *shad = shaderHead; shad; shad = shad->next) shad->Compile();
}


void VOpenGLDrawer::DestroyShaders () {
  for (VGLShader *shad = shaderHead; shad; shad = shad->next) shad->Unload();
  shaderHead = nullptr;
}


//==========================================================================
//
//  VOpenGLDrawer::RestoreDepthFunc
//
//==========================================================================
void VOpenGLDrawer::RestoreDepthFunc () {
  glDepthFunc(!CanUseRevZ() ? GL_LEQUAL : GL_GEQUAL);
}


//==========================================================================
//
//  VOpenGLDrawer::RestoreDepthFunc
//
//==========================================================================
void VOpenGLDrawer::SetupTextureFiltering (int level) {
  // for anisotropy, we require trilinear filtering
  if (anisotropyExists) {
    /*
    if (gl_texture_filter_anisotropic > 1) {
      // turn on trilinear filtering
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      // setup anisotropy level
      glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT),
        (gl_texture_filter_anisotropic > max_anisotropy ? max_anisotropy : gl_texture_filter_anisotropic)
      );
      return;
    }
    // we have anisotropy, but it is turned off
    //glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT), 1); // 1 is minimum, i.e. "off"
    */
    // but newer OpenGL versions allows anisotropy filtering even for "nearest" mode,
    // so setup it in any case
    glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT),
      (gl_texture_filter_anisotropic > max_anisotropy ? max_anisotropy : gl_texture_filter_anisotropic)
    );
  }
  int mipfilter, maxfilter;
  // setup filtering
  switch (level) {
    case 1: // nearest mipmap
      maxfilter = GL_NEAREST;
      mipfilter = GL_NEAREST_MIPMAP_NEAREST;
      break;
    case 2: // linear nearest
      maxfilter = GL_LINEAR;
      mipfilter = GL_LINEAR_MIPMAP_NEAREST;
      break;
    case 3: // bilinear
      maxfilter = GL_LINEAR;
      mipfilter = GL_LINEAR;
      break;
    case 4: // trilinear
      maxfilter = GL_LINEAR;
      mipfilter = GL_LINEAR_MIPMAP_LINEAR;
      break;
    default: // nearest, no mipmaps
      maxfilter = GL_NEAREST;
      mipfilter = GL_NEAREST;
      break;
  }
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipfilter);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, maxfilter);
}


//==========================================================================
//
//  VOpenGLDrawer::ReactivateCurrentFBO
//
//==========================================================================
void VOpenGLDrawer::ReactivateCurrentFBO () {
  if (currentActiveFBO) {
    glBindFramebuffer(GL_FRAMEBUFFER, currentActiveFBO->getFBOid());
    ScrWdt = currentActiveFBO->getWidth();
    ScrHgt = currentActiveFBO->getHeight();
  } else {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    ScrWdt = ScreenWidth;
    ScrHgt = ScreenHeight;
  }
}


//==========================================================================
//
//  VOpenGLDrawer::DeinitResolution
//
//==========================================================================
void VOpenGLDrawer::DeinitResolution () {
  if (currentActiveFBO != nullptr) {
    currentActiveFBO = nullptr;
    ReactivateCurrentFBO();
  }
  // unload shaders
  DestroyShaders();
  // delete all created shader objects
  for (int i = CreatedShaderObjects.length()-1; i >= 0; --i) {
    p_glDeleteObjectARB(CreatedShaderObjects[i]);
  }
  CreatedShaderObjects.Clear();
  // destroy FBOs
  //DestroyCameraFBOList();
  for (auto &&cf : cameraFBOList) cf->fbo.destroy();
  mainFBO.destroy();
  //secondFBO.destroy();
  ambLightFBO.destroy();
  wipeFBO.destroy();
  BloomDeinit();
  DeleteLightmapAtlases();
}


#define gl_(x)   p_##x = x##_t(GetExtFuncPtr(#x)); if (!p_##x) Sys_Error("OpenGL: `%s()` not found!", ""#x);
#define glc_(x)  ({ p_##x = x##_t(GetExtFuncPtr(#x)); !!p_##x; })
#define glg_(x)  p_##x = x##_t(GetExtFuncPtr(#x)); glAPISuccessFlag = glAPISuccessFlag && (!!p_##x)


//==========================================================================
//
//  VOpenGLDrawer::InitResolution
//
//==========================================================================
void VOpenGLDrawer::InitResolution () {
  bool glAPISuccessFlag;

  if (currentActiveFBO != nullptr) {
    currentActiveFBO = nullptr;
    ReactivateCurrentFBO();
  }

  GCon->Logf(NAME_Init, "Setting up new resolution: %dx%d", ScreenWidth, ScreenHeight);

  if (gl_dump_vendor) {
    GCon->Logf(NAME_Init, "GL_VENDOR: %s", glGetString(GL_VENDOR));
    GCon->Logf(NAME_Init, "GL_RENDERER: %s", glGetString(GL_RENDERER));
    GCon->Logf(NAME_Init, "GL_VERSION: %s", glGetString(GL_VERSION));
  }

  if (gl_dump_extensions) {
    GCon->Log(NAME_Init, "GL_EXTENSIONS:");
    TArray<VStr> Exts;
    VStr((char *)glGetString(GL_EXTENSIONS)).Split(' ', Exts);
    for (int i = 0; i < Exts.Num(); ++i) GCon->Log(NAME_Init, VStr("- ")+Exts[i]);
  }

  if (!shittyGPUCheckDone) {
    shittyGPUCheckDone = true;
    const char *vcstr = (const char *)glGetString(GL_VENDOR);
    VStr vs = VStr(vcstr).toLowerCase();
    isShittyGPU = CheckVendorString(vs, "intel");
    if (isShittyGPU) {
      GCon->Log(NAME_Init, "Sorry, but your GPU seems to be in my glitchy list; turning off some advanced features");
      GCon->Logf(NAME_Init, "GPU Vendor: %s", vcstr);
      if (gl_dbg_ignore_gpu_blacklist) {
        GCon->Log(NAME_Init, "User command is to ignore blacklist; I shall obey!");
        isShittyGPU = false;
      }
    }
  }

  if (!isShittyGPU && gl_dbg_force_gpu_blacklisting) {
    GCon->Log(NAME_Init, "User command is to blacklist GPU; I shall obey!");
    isShittyGPU = true;
  }

  gl_is_shitty_gpu = isShittyGPU;

  // check the maximum texture size
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
  GCon->Logf(NAME_Init, "Maximum texture size: %d", maxTexSize);
  if (maxTexSize < 1024) maxTexSize = 1024; // 'cmon!

  hasNPOT = CheckExtension("GL_ARB_texture_non_power_of_two") || CheckExtension("GL_OES_texture_npot");
  hasBoundsTest = CheckExtension("GL_EXT_depth_bounds_test");

  useReverseZ = false;
  GLint major, minor;
  glGetIntegerv(GL_MAJOR_VERSION, &major);
  glGetIntegerv(GL_MINOR_VERSION, &minor);
  GCon->Logf(NAME_Init, "OpenGL v%d.%d found", major, minor);

  p_glClipControl = nullptr;
  if ((major > 4 || (major == 4 && minor >= 5)) || CheckExtension("GL_ARB_clip_control")) {
    p_glClipControl = glClipControl_t(GetExtFuncPtr("glClipControl"));
  }
  if (p_glClipControl) {
    if (gl_enable_clip_control) {
      GCon->Logf(NAME_Init, "OpenGL: `glClipControl()` found");
    } else {
      p_glClipControl = nullptr;
      GCon->Logf(NAME_Init, "OpenGL: `glClipControl()` found, but disabled by user; i shall obey");
    }
  }

  p_glBlitFramebuffer = glBlitFramebuffer_t(GetExtFuncPtr("glBlitFramebuffer"));
  if (p_glBlitFramebuffer) GCon->Logf(NAME_Init, "OpenGL: `glBlitFramebuffer()` found");

  if (!isShittyGPU && p_glClipControl) {
    // normal GPUs
    useReverseZ = true;
    if (!gl_enable_reverse_z) {
      GCon->Logf(NAME_Init, "OpenGL: oops, user disabled reverse z, i shall obey");
      useReverseZ = false;
    }
  } else {
    GCon->Logf(NAME_Init, "OpenGL: reverse z is turned off for your GPU");
    useReverseZ = false;
  }

  // check multi-texture extensions
  if (!CheckExtension("GL_ARB_multitexture")) {
    Sys_Error("OpenGL FATAL: Multitexture extensions not found.");
  } else {
    //gl_(glMultiTexCoord2fARB);
    gl_(glActiveTextureARB);

    GCon->Log(NAME_Init, "Multitexture extensions found.");
    GLint tmp;
    glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &tmp);
    GCon->Logf(NAME_Init, "Max texture units: %d", tmp);
    if (tmp > 1) MaxTextureUnits = tmp;
  }

  // check main stencil buffer
  // this is purely informative, as we are using FBO to render things anyway
  /*
  {
    GLint stencilBits = 0;
    glGetIntegerv(GL_STENCIL_BITS, &stencilBits);
    GCon->Logf(NAME_Init, "Main stencil buffer depth: %d", stencilBits);
  }
  */

  // anisotropy extension
  max_anisotropy = 1.0f;
  if (ext_anisotropy && CheckExtension("GL_EXT_texture_filter_anisotropic")) {
    glGetFloatv(GLenum(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT), &max_anisotropy);
    if (max_anisotropy < 1) max_anisotropy = 1;
    GCon->Logf(NAME_Init, "Max anisotropy: %g", (double)max_anisotropy);
  }
  gl_max_anisotropy = (int)max_anisotropy;
  anisotropyExists = (gl_max_anisotropy > 1);

  // clamp to edge extension
  if (CheckExtension("GL_SGIS_texture_edge_clamp") || CheckExtension("GL_EXT_texture_edge_clamp")) {
    GCon->Log(NAME_Init, "Clamp to edge extension found.");
    ClampToEdge = GL_CLAMP_TO_EDGE_SGIS;
  } else {
    ClampToEdge = GL_CLAMP;
  }

  // check for shader extensions
  if (CheckExtension("GL_ARB_shader_objects") && CheckExtension("GL_ARB_shading_language_100") &&
      CheckExtension("GL_ARB_vertex_shader") && CheckExtension("GL_ARB_fragment_shader"))
  {
    gl_(glDeleteObjectARB);
    gl_(glGetHandleARB);
    gl_(glDetachObjectARB);
    gl_(glCreateShaderObjectARB);
    gl_(glShaderSourceARB);
    gl_(glCompileShaderARB);
    gl_(glCreateProgramObjectARB);
    gl_(glAttachObjectARB);
    gl_(glLinkProgramARB);
    gl_(glUseProgramObjectARB);
    gl_(glValidateProgramARB);
    gl_(glUniform1fARB);
    gl_(glUniform2fARB);
    gl_(glUniform3fARB);
    gl_(glUniform4fARB);
    gl_(glUniform1iARB);
    gl_(glUniform2iARB);
    gl_(glUniform3iARB);
    gl_(glUniform4iARB);
    gl_(glUniform1fvARB);
    gl_(glUniform2fvARB);
    gl_(glUniform3fvARB);
    gl_(glUniform4fvARB);
    gl_(glUniform1ivARB);
    gl_(glUniform2ivARB);
    gl_(glUniform3ivARB);
    gl_(glUniform4ivARB);
    gl_(glUniformMatrix2fvARB);
    gl_(glUniformMatrix3fvARB);
    gl_(glUniformMatrix4fvARB);
    gl_(glGetObjectParameterfvARB);
    gl_(glGetObjectParameterivARB);
    gl_(glGetInfoLogARB);
    gl_(glGetAttachedObjectsARB);
    gl_(glGetUniformLocationARB);
    gl_(glGetActiveUniformARB);
    gl_(glGetUniformfvARB);
    gl_(glGetUniformivARB);
    gl_(glGetShaderSourceARB);

    gl_(glVertexAttrib1dARB);
    gl_(glVertexAttrib1dvARB);
    gl_(glVertexAttrib1fARB);
    gl_(glVertexAttrib1fvARB);
    gl_(glVertexAttrib1sARB);
    gl_(glVertexAttrib1svARB);
    gl_(glVertexAttrib2dARB);
    gl_(glVertexAttrib2dvARB);
    gl_(glVertexAttrib2fARB);
    gl_(glVertexAttrib2fvARB);
    gl_(glVertexAttrib2sARB);
    gl_(glVertexAttrib2svARB);
    gl_(glVertexAttrib3dARB);
    gl_(glVertexAttrib3dvARB);
    gl_(glVertexAttrib3fARB);
    gl_(glVertexAttrib3fvARB);
    gl_(glVertexAttrib3sARB);
    gl_(glVertexAttrib3svARB);
    gl_(glVertexAttrib4NbvARB);
    gl_(glVertexAttrib4NivARB);
    gl_(glVertexAttrib4NsvARB);
    gl_(glVertexAttrib4NubARB);
    gl_(glVertexAttrib4NubvARB);
    gl_(glVertexAttrib4NuivARB);
    gl_(glVertexAttrib4NusvARB);
    gl_(glVertexAttrib4bvARB);
    gl_(glVertexAttrib4dARB);
    gl_(glVertexAttrib4dvARB);
    gl_(glVertexAttrib4fARB);
    gl_(glVertexAttrib4fvARB);
    gl_(glVertexAttrib4ivARB);
    gl_(glVertexAttrib4sARB);
    gl_(glVertexAttrib4svARB);
    gl_(glVertexAttrib4ubvARB);
    gl_(glVertexAttrib4uivARB);
    gl_(glVertexAttrib4usvARB);
    gl_(glVertexAttribPointerARB);
    gl_(glEnableVertexAttribArrayARB);
    gl_(glDisableVertexAttribArrayARB);
    gl_(glBindAttribLocationARB);
    gl_(glGetActiveAttribARB);
    gl_(glGetAttribLocationARB);
    gl_(glGetVertexAttribdvARB);
    gl_(glGetVertexAttribfvARB);
    gl_(glGetVertexAttribivARB);
    gl_(glGetVertexAttribPointervARB);

    gl_(glGetProgramiv);
    //gl_(glGetPointerv);

    gl_(glBlendFuncSeparate);

    if (hasBoundsTest) {
      if (!glc_(glDepthBoundsEXT)) {
        hasBoundsTest = false;
        GCon->Logf(NAME_Init, "OpenGL: GL_EXT_depth_bounds_test found, but no `glDepthBoundsEXT()` exported");
      }
    }

    GLint tmp;
    GCon->Logf(NAME_Init, "Shading language version: %s", glGetString(GL_SHADING_LANGUAGE_VERSION_ARB));
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS_ARB, &tmp);
    GCon->Logf(NAME_Init, "Max texture image units: %d", tmp);
    if (tmp > 1) MaxTextureUnits = tmp; // this is number of texture *samplers*, but it is ok for our shaders case
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB, &tmp);
    GCon->Logf(NAME_Init, "Max vertex uniform components: %d", tmp);
    glGetIntegerv(GL_MAX_VARYING_FLOATS_ARB, &tmp);
    GCon->Logf(NAME_Init, "Max varying floats: %d", tmp);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS_ARB, &tmp);
    GCon->Logf(NAME_Init, "Max vertex attribs: %d", tmp);
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS_ARB, &tmp);
    GCon->Logf(NAME_Init, "Max fragment uniform components: %d", tmp);
  } else {
    Sys_Error("OpenGL FATAL: no shader support");
  }

  {
    if (glc_(glStencilFuncSeparate) && glc_(glStencilOpSeparate)) {
      GCon->Log(NAME_Init, "Found OpenGL 2.0 separate stencil methods");
    } else if (CheckExtension("GL_ATI_separate_stencil")) {
      GCon->Log(NAME_Init, "Found GL_ATI_separate_stencil...");
      p_glStencilFuncSeparate = glStencilFuncSeparate_t(GetExtFuncPtr("glStencilFuncSeparateATI"));
      p_glStencilOpSeparate = glStencilOpSeparate_t(GetExtFuncPtr("glStencilOpSeparateATI"));
      if (p_glStencilFuncSeparate && p_glStencilOpSeparate) {
        GCon->Log(NAME_Init, "Separate stencil extensions found");
      } else {
        p_glStencilFuncSeparate = nullptr;
        p_glStencilOpSeparate = nullptr;
      }
    } else {
      GCon->Log(NAME_Init, "No separate stencil methods found");
      p_glStencilFuncSeparate = nullptr;
      p_glStencilOpSeparate = nullptr;
    }
  }

  if (!gl_dbg_disable_depth_clamp && CheckExtension("GL_ARB_depth_clamp")) {
    GCon->Log(NAME_Init, "Found GL_ARB_depth_clamp...");
    HaveDepthClamp = true;
  } else if (!gl_dbg_disable_depth_clamp && CheckExtension("GL_NV_depth_clamp")) {
    GCon->Log(NAME_Init, "Found GL_NV_depth_clamp...");
    HaveDepthClamp = true;
  } else {
    GCon->Log(NAME_Init, "Symbol not found, depth clamp extensions disabled.");
    HaveDepthClamp = false;
  }

  if (CheckExtension("GL_EXT_stencil_wrap")) {
    GCon->Log(NAME_Init, "Found GL_EXT_stencil_wrap...");
    HaveStencilWrap = true;
  } else {
    GCon->Log(NAME_Init, "Symbol not found, stencil wrap extensions disabled.");
    HaveStencilWrap = false;
  }

  if (!CheckExtension("GL_ARB_vertex_buffer_object")) {
    Sys_Error("OpenGL FATAL: VBO not found.");
  } else {
    gl_(glBindBufferARB);
    gl_(glDeleteBuffersARB);
    gl_(glGenBuffersARB);
    gl_(glIsBufferARB);
    gl_(glBufferDataARB);
    gl_(glBufferSubDataARB);
    gl_(glGetBufferSubDataARB);
    gl_(glMapBufferARB);
    gl_(glUnmapBufferARB);
    gl_(glGetBufferParameterivARB);
    gl_(glGetBufferPointervARB);
  }

  if (!CheckExtension("GL_EXT_draw_range_elements")) {
    Sys_Error("OpenGL FATAL: GL_EXT_draw_range_elements not found");
  } else {
    gl_(glDrawRangeElementsEXT);
  }

  // API required for bloom fx
  glAPISuccessFlag = true;
  glg_(glDeleteRenderbuffers);
  glg_(glGenRenderbuffersEXT);
  glg_(glRenderbufferStorageEXT);
  glg_(glBindRenderbufferEXT);
  glg_(glGenFramebuffersEXT);
  glg_(glBindFramebufferEXT);
  glg_(glFramebufferRenderbufferEXT);
  glg_(glGenerateMipmapEXT);

  if (!glAPISuccessFlag || !p_glBlitFramebuffer) {
    GCon->Logf(NAME_Init, "OpenGL: bloom postprocessing effect disabled due to missing API");
    r_bloom = false;
  }

  if (hasBoundsTest) GCon->Logf(NAME_Init, "Found GL_EXT_depth_bounds_test...");


  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glPixelStorei(GL_PACK_ROW_LENGTH, 0);
  glPixelStorei(GL_PACK_IMAGE_HEIGHT, 0);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);


  glFramebufferTexture2D = (glFramebufferTexture2DFn)GetExtFuncPtr("glFramebufferTexture2D");
  glDeleteFramebuffers = (glDeleteFramebuffersFn)GetExtFuncPtr("glDeleteFramebuffers");
  glGenFramebuffers = (glGenFramebuffersFn)GetExtFuncPtr("glGenFramebuffers");
  glCheckFramebufferStatus = (glCheckFramebufferStatusFn)GetExtFuncPtr("glCheckFramebufferStatus");
  glBindFramebuffer = (glBindFramebufferFn)GetExtFuncPtr("glBindFramebuffer");

  if (!glFramebufferTexture2D || !glDeleteFramebuffers || !glGenFramebuffers ||
      !glCheckFramebufferStatus || !glBindFramebuffer)
  {
    Sys_Error("OpenGL FBO API not found");
  }

  //GCon->Logf("********* %d : %d *********", ScreenWidth, ScreenHeight);

  // allocate main FBO object
  mainFBO.createDepthStencil(this, ScreenWidth, ScreenHeight);
  GCon->Logf(NAME_Init, "OpenGL: reverse z is %s", (useReverseZ ? "enabled" : "disabled"));

  mainFBO.activate();
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // black background
  glClearDepth(!useReverseZ ? 1.0f : 0.0f);
  if (p_glClipControl) p_glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE); // actually, this is better even for "normal" cases
  RestoreDepthFunc();
  glDepthRange(0.0f, 1.0f);

  glClearStencil(0);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
  stencilBufferDirty = false;

  // recreate camera FBOs
  for (auto &&cf : cameraFBOList) {
    cf->fbo.createDepthStencil(this, cf->camwidth, cf->camheight); // create depthstencil
    cf->fbo.activate();
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // black background
    glClearDepth(!useReverseZ ? 1.0f : 0.0f);
    if (p_glClipControl) p_glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE); // actually, this is better even for "normal" cases
    RestoreDepthFunc();
    glDepthRange(0.0f, 1.0f);

    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
  }

  // allocate ambient light FBO object
  ambLightFBO.createTextureOnly(this, ScreenWidth, ScreenHeight);

  // allocate wipe FBO object
  wipeFBO.createTextureOnly(this, ScreenWidth, ScreenHeight);

  mainFBO.activate();

  // init some defaults
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // Black Background
  glClearDepth(!useReverseZ ? 1.0f : 0.0f);
  if (p_glClipControl) p_glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE); // actually, this is better even for "normal" cases
  RestoreDepthFunc();
  glDepthRange(0.0f, 1.0f);

  glClearStencil(0);

  glClear(GL_COLOR_BUFFER_BIT);
  Update();
  glClear(GL_COLOR_BUFFER_BIT);

  glEnable(GL_TEXTURE_2D);
  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

  vassert(!atlasesGenerated);
  GenerateLightmapAtlasTextures();

  //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glAlphaFunc(GL_GREATER, getAlphaThreshold());
  glShadeModel(GL_FLAT);

  glDisable(GL_POLYGON_SMOOTH);

  // shaders
  shaderHead = nullptr; // just in case

  LoadAllShaders();
  CompileShaders();

  if (glGetError() != 0) Sys_Error("OpenGL initialization error");

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  blendEnabled = true;

  mInitialized = true;

  currMainFBO = -1;

  currentActiveFBO = nullptr;
  ReactivateCurrentFBO();

  callICB(VCB_InitResolution);
}

#undef gl_
#undef glc_
#undef glg_


//==========================================================================
//
//  VOpenGLDrawer::CheckExtension
//
//==========================================================================
bool VOpenGLDrawer::CheckExtension (const char *ext) {
  if (!ext || !ext[0]) return false;
  TArray<VStr> Exts;
  VStr((char*)glGetString(GL_EXTENSIONS)).Split(' ', Exts);
  for (int i = 0; i < Exts.Num(); ++i) if (Exts[i] == ext) return true;
  return false;
}


//==========================================================================
//
//  VOpenGLDrawer::SupportsShadowVolumeRendering
//
//==========================================================================
bool VOpenGLDrawer::SupportsShadowVolumeRendering () {
  return (HaveStencilWrap && p_glStencilFuncSeparate && HaveDepthClamp);
}


//==========================================================================
//
//  VOpenGLDrawer::Setup2D
//
//==========================================================================
void VOpenGLDrawer::Setup2D () {
  glViewport(0, 0, getWidth(), getHeight());

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, getWidth(), getHeight(), 0, -666, 666);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  //GLDisableBlend();
  GLEnableBlend();
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  if (HaveDepthClamp) glDisable(GL_DEPTH_CLAMP);
}


//==========================================================================
//
//  VOpenGLDrawer::StartUpdate
//
//==========================================================================
void VOpenGLDrawer::StartUpdate () {
  //glFinish();
  VRenderLevelShared::ResetPortalPool();

  ActivateMainFBO();

  glBindTexture(GL_TEXTURE_2D, 0);
  //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  // turn off anisotropy
  //glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT), 1); // 1 is minimum, i.e. "off"

  if (usegamma != lastgamma) {
    FlushTextures(true); // forced
    lastgamma = usegamma;
  }

  Setup2D();
}


//==========================================================================
//
//  VOpenGLDrawer::FinishUpdate
//
//==========================================================================
void VOpenGLDrawer::ClearScreen (unsigned clearFlags) {
  GLuint flags = 0;
  if (clearFlags&CLEAR_COLOR) flags |= GL_COLOR_BUFFER_BIT;
  if (clearFlags&CLEAR_DEPTH) flags |= GL_DEPTH_BUFFER_BIT;
  if (clearFlags&CLEAR_STENCIL) flags |= GL_STENCIL_BUFFER_BIT;
  if (flags) glClear(flags);
}


//==========================================================================
//
//  VOpenGLDrawer::FinishUpdate
//
//==========================================================================
void VOpenGLDrawer::FinishUpdate () {
  //mainFBO.blitToScreen();
  GetMainFBO()->blitToScreen();
  glBindTexture(GL_TEXTURE_2D, 0);
  SetMainFBO(true); // forced
  glBindTexture(GL_TEXTURE_2D, 0);
  glOrtho(0, getWidth(), getHeight(), 0, -666, 666);
  //ActivateMainFBO();
  //glFlush();
}


//==========================================================================
//
//  VOpenGLDrawer::GetProjectionMatrix
//
//==========================================================================
void VOpenGLDrawer::GetProjectionMatrix (VMatrix4 &mat) {
  glGetFloatv(GL_PROJECTION_MATRIX, mat[0]);
}


//==========================================================================
//
//  VOpenGLDrawer::GetModelMatrix
//
//==========================================================================
void VOpenGLDrawer::GetModelMatrix (VMatrix4 &mat) {
  glGetFloatv(GL_MODELVIEW_MATRIX, mat[0]);
}


//==========================================================================
//
//  VOpenGLDrawer::LoadVPMatrices
//
//  call this before doing light scissor calculations (can be called once per scene)
//  sets `projMat` and `modelMat`
//  scissor setup will use those matrices (but won't modify them)
//
//==========================================================================
void VOpenGLDrawer::LoadVPMatrices () {
  glGetFloatv(GL_PROJECTION_MATRIX, vpmats.projMat[0]);
  glGetFloatv(GL_MODELVIEW_MATRIX, vpmats.modelMat[0]);
  GLint vport[4];
  glGetIntegerv(GL_VIEWPORT, vport);
  vpmats.vport.setOrigin(vport[0], vport[1]);
  vpmats.vport.setSize(vport[2], vport[3]);
}


//==========================================================================
//
//  VOpenGLDrawer::SetupLightScissor
//
//  returns:
//   0 if scissor is empty
//  -1 if scissor has no sense (should not be used)
//   1 if scissor is set
//
//==========================================================================
int VOpenGLDrawer::SetupLightScissor (const TVec &org, float radius, int scoord[4], const TVec *geobbox) {
  int tmpscoord[4];

  if (!scoord) scoord = tmpscoord;

  if (radius < 4) {
    scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
    currentSVScissor[SCS_MINX] = currentSVScissor[SCS_MINY] = currentSVScissor[SCS_MAXX] = currentSVScissor[SCS_MAXY] = 0;
    glScissor(0, 0, 0, 0);
    return 0;
  }

  // just in case
  if (!vpmats.vport.isValid()) {
    scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
    currentSVScissor[SCS_MINX] = currentSVScissor[SCS_MINY] = currentSVScissor[SCS_MAXX] = currentSVScissor[SCS_MAXY] = 0;
    glDisable(GL_SCISSOR_TEST);
    return 0;
  }

  // transform into world coords
  TVec inworld = vpmats.toWorld(org);

  // the thing that should not be (completely behind)
  if (inworld.z-radius > -1.0f) {
    scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
    currentSVScissor[SCS_MINX] = currentSVScissor[SCS_MINY] = currentSVScissor[SCS_MAXX] = currentSVScissor[SCS_MAXY] = 0;
    glDisable(GL_SCISSOR_TEST);
    return 0;
  }

  // create light bbox
  float bbox[6];
  bbox[0+0] = inworld.x-radius;
  bbox[0+1] = inworld.y-radius;
  bbox[0+2] = inworld.z-radius;

  bbox[3+0] = inworld.x+radius;
  bbox[3+1] = inworld.y+radius;
  bbox[3+2] = min2(-1.0f, inworld.z+radius); // clamp to znear

  // clamp it with geometry bbox, if there is any
  if (geobbox) {
    float gbb[6];
    gbb[0] = geobbox[0].x;
    gbb[1] = geobbox[0].y;
    gbb[2] = geobbox[0].z;
    gbb[3] = geobbox[1].x;
    gbb[4] = geobbox[1].y;
    gbb[5] = geobbox[1].z;
    float trbb[6] = { FLT_MAX, FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX };
    for (unsigned f = 0; f < 8; ++f) {
      const TVec vtx = vpmats.toWorld(TVec(gbb[BBoxVertexIndex[f][0]], gbb[BBoxVertexIndex[f][1]], gbb[BBoxVertexIndex[f][2]]));
      trbb[0] = min2(trbb[0], vtx.x);
      trbb[1] = min2(trbb[1], vtx.y);
      trbb[2] = min2(trbb[2], vtx.z);
      trbb[3] = max2(trbb[3], vtx.x);
      trbb[4] = max2(trbb[4], vtx.y);
      trbb[5] = max2(trbb[5], vtx.z);
    }

    if (trbb[0] >= trbb[3+0] || trbb[1] >= trbb[3+1] || trbb[2] >= trbb[3+2]) {
      scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
      currentSVScissor[SCS_MINX] = currentSVScissor[SCS_MINY] = currentSVScissor[SCS_MAXX] = currentSVScissor[SCS_MAXY] = 0;
      glDisable(GL_SCISSOR_TEST);
      return 0;
    }

    trbb[2] = min2(-1.0f, trbb[2]);
    trbb[5] = min2(-1.0f, trbb[5]);

    /*
    if (trbb[0] > bbox[0] || trbb[1] > bbox[1] || trbb[2] > bbox[2] ||
        trbb[3] < bbox[3] || trbb[4] < bbox[4] || trbb[5] < bbox[5])
    {
      GCon->Logf("GEOCLAMP: (%f,%f,%f)-(%f,%f,%f) : (%f,%f,%f)-(%f,%f,%f)", bbox[0], bbox[1], bbox[2], bbox[3], bbox[4], bbox[5], trbb[0], trbb[1], trbb[2], trbb[3], trbb[4], trbb[5]);
    }
    */

    bbox[0] = max2(bbox[0], trbb[0]);
    bbox[1] = max2(bbox[1], trbb[1]);
    bbox[2] = max2(bbox[2], trbb[2]);
    bbox[3] = min2(bbox[3], trbb[3]);
    bbox[4] = min2(bbox[4], trbb[4]);
    bbox[5] = min2(bbox[5], trbb[5]);
    if (bbox[0] >= bbox[3+0] || bbox[1] >= bbox[3+1] || bbox[2] >= bbox[3+2]) {
      scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
      currentSVScissor[SCS_MINX] = currentSVScissor[SCS_MINY] = currentSVScissor[SCS_MAXX] = currentSVScissor[SCS_MAXY] = 0;
      glDisable(GL_SCISSOR_TEST);
      return 0;
    }

    /*
    const TVec bc0 = vpmats.toWorld(geobbox[0]);
    const TVec bc1 = vpmats.toWorld(geobbox[1]);
    const TVec bmin = TVec(min2(bc0.x, bc1.x), min2(bc0.y, bc1.y), min2(-1.0f, min2(bc0.z, bc1.z)));
    const TVec bmax = TVec(max2(bc0.x, bc1.x), max2(bc0.y, bc1.y), min2(-1.0f, max2(bc0.z, bc1.z)));
    if (bmin.x > bbox[0] || bmin.y > bbox[1] || bmin.z > bbox[2] ||
        bmax.x < bbox[3] || bmax.y < bbox[4] || bmax.z < bbox[5])
    {
      GCon->Logf("GEOCLAMP: (%f,%f,%f)-(%f,%f,%f) : (%f,%f,%f)-(%f,%f,%f)", bbox[0], bbox[1], bbox[2], bbox[3], bbox[4], bbox[5], bmin.x, bmin.y, bmin.z, bmax.x, bmax.y, bmax.z);
    }
    bbox[0] = max2(bbox[0], bmin.x);
    bbox[1] = max2(bbox[1], bmin.y);
    bbox[2] = max2(bbox[2], bmin.z);
    bbox[3] = min2(bbox[3], bmax.x);
    bbox[4] = min2(bbox[4], bmax.y);
    bbox[5] = min2(bbox[5], bmax.z);
    if (bbox[0] >= bbox[3+0] || bbox[1] >= bbox[3+1] || bbox[2] >= bbox[3+2]) {
      scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
      currentSVScissor[SCS_MINX] = currentSVScissor[SCS_MINY] = currentSVScissor[SCS_MAXX] = currentSVScissor[SCS_MAXY] = 0;
      glDisable(GL_SCISSOR_TEST);
      return 0;
    }
    */
  }

  // setup depth bounds
  if (hasBoundsTest && gl_enable_depth_bounds) {
    const bool zeroZ = (gl_enable_clip_control && p_glClipControl);
    const bool revZ = CanUseRevZ();

    //const float ofsz0 = min2(-1.0f, inworld.z+radius);
    //const float ofsz1 = inworld.z-radius;
    const float ofsz0 = bbox[5];
    const float ofsz1 = bbox[2];
    vassert(ofsz1 <= -1.0f);

    float pjwz0 = -1.0f/ofsz0;
    float pjwz1 = -1.0f/ofsz1;

    // for reverse z, projz is always 1, so we can simply use pjw
    if (!revZ) {
      pjwz0 *= vpmats.projMat.Transform2OnlyZ(TVec(inworld.x, inworld.y, ofsz0));
      pjwz1 *= vpmats.projMat.Transform2OnlyZ(TVec(inworld.x, inworld.y, ofsz1));
    }

    // transformation for [-1..1] z range
    if (!zeroZ) {
      pjwz0 = (1.0f+pjwz0)*0.5f;
      pjwz1 = (1.0f+pjwz1)*0.5f;
    }

    if (revZ) {
      p_glDepthBoundsEXT(pjwz1, pjwz0);
    } else {
      p_glDepthBoundsEXT(pjwz0, pjwz1);
    }
    glEnable(GL_DEPTH_BOUNDS_TEST_EXT);
  }

  const int scrx0 = vpmats.vport.x0;
  const int scry0 = vpmats.vport.y0;
  const int scrx1 = vpmats.vport.getX1();
  const int scry1 = vpmats.vport.getY1();

  int minx = scrx1+64, miny = scry1+64;
  int maxx = -(scrx0-64), maxy = -(scry0-64);

  // transform points, get min and max
  for (unsigned f = 0; f < 8; ++f) {
    int winx, winy;
    vpmats.project(TVec(bbox[BBoxVertexIndex[f][0]], bbox[BBoxVertexIndex[f][1]], bbox[BBoxVertexIndex[f][2]]), &winx, &winy);

    if (minx > winx) minx = winx;
    if (miny > winy) miny = winy;
    if (maxx < winx) maxx = winx;
    if (maxy < winy) maxy = winy;
  }

  if (minx > scrx1 || miny > scry1 || maxx < scrx0 || maxy < scry0) {
    scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
    currentSVScissor[SCS_MINX] = currentSVScissor[SCS_MINY] = currentSVScissor[SCS_MAXX] = currentSVScissor[SCS_MAXY] = 0;
    glDisable(GL_SCISSOR_TEST);
    if (hasBoundsTest && gl_enable_depth_bounds) glDisable(GL_DEPTH_BOUNDS_TEST_EXT);
    return 0;
  }

  minx = midval(scrx0, minx, scrx1);
  miny = midval(scry0, miny, scry1);
  maxx = midval(scrx0, maxx, scrx1);
  maxy = midval(scry0, maxy, scry1);

  //GCon->Logf("  radius=%f; (%d,%d)-(%d,%d)", radius, minx, miny, maxx, maxy);
  const int wdt = maxx-minx+1;
  const int hgt = maxy-miny+1;

  // drop very small lights, why not?
  if (wdt <= 4 || hgt <= 4) {
    scoord[0] = scoord[1] = scoord[2] = scoord[3] = 0;
    currentSVScissor[SCS_MINX] = currentSVScissor[SCS_MINY] = currentSVScissor[SCS_MAXX] = currentSVScissor[SCS_MAXY] = 0;
    glDisable(GL_SCISSOR_TEST);
    if (hasBoundsTest && gl_enable_depth_bounds) glDisable(GL_DEPTH_BOUNDS_TEST_EXT);
    return 0;
  }

  glEnable(GL_SCISSOR_TEST);
  glScissor(minx, miny, wdt, hgt);
  scoord[0] = minx;
  scoord[1] = miny;
  scoord[2] = maxx;
  scoord[3] = maxy;
  currentSVScissor[SCS_MINX] = scoord[0];
  currentSVScissor[SCS_MINY] = scoord[1];
  currentSVScissor[SCS_MAXX] = scoord[2];
  currentSVScissor[SCS_MAXY] = scoord[3];

  return 1;
}


//==========================================================================
//
//  VOpenGLDrawer::ResetScissor
//
//==========================================================================
void VOpenGLDrawer::ResetScissor () {
  glScissor(0, 0, getWidth(), getHeight());
  glDisable(GL_SCISSOR_TEST);
  if (hasBoundsTest) glDisable(GL_DEPTH_BOUNDS_TEST_EXT);
  currentSVScissor[SCS_MINX] = currentSVScissor[SCS_MINY] = 0;
  currentSVScissor[SCS_MAXX] = currentSVScissor[SCS_MAXY] = 32000;
}


//==========================================================================
//
//  VOpenGLDrawer::UseFrustumFarClip
//
//==========================================================================
bool VOpenGLDrawer::UseFrustumFarClip () {
  if (CanUseRevZ()) return false;
  if (RendLev && RendLev->NeedsInfiniteFarClip && !HaveDepthClamp) return false;
  return true;
}


//==========================================================================
//
//  VOpenGLDrawer::SetMainFBO
//
//==========================================================================
void VOpenGLDrawer::SetMainFBO (bool forced) {
  if (forced || currMainFBO != -1) {
    currMainFBO = -1;
    if (forced) currentActiveFBO = nullptr;
    mainFBO.activate();
    stencilBufferDirty = true;
  }
}


//==========================================================================
//
//  VOpenGLDrawer::ClearCameraFBOs
//
//==========================================================================
void VOpenGLDrawer::ClearCameraFBOs () {
  if (cameraFBOList.length()) GCon->Logf(NAME_Debug, "deleting #%d camera FBO%s", cameraFBOList.length(), (cameraFBOList.length() != 1 ? "s" : ""));
  DestroyCameraFBOList();
}


//==========================================================================
//
//  VOpenGLDrawer::GetCameraFBO
//
//  returns index or -1; (re)creates FBO if necessary
//
//==========================================================================
int VOpenGLDrawer::GetCameraFBO (int texnum, int width, int height) {
  if (width < 1) width = 1;
  if (height < 1) height = 1;

  int cfidx = cameraFBOList.length();

  for (auto &&cf : cameraFBOList) {
    if (cf->texnum == texnum) {
      if (cf->camwidth == width && cf->camheight == height) return cf->index; // nothing to do
      // recreate
      GCon->Logf(NAME_Debug, "recreating camera FBO #%d for texture #%d (old size is %dx%d, new size is %dx%d)", cf->index, texnum, cf->camwidth, cf->camheight, width, height);
      cfidx = cf->index;
      break;
    }
  }

  if (cfidx < cameraFBOList.length()) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    cameraFBOList[cfidx]->fbo.destroy();
  }

  if (cfidx >= cameraFBOList.length()) {
    cfidx = cameraFBOList.length();
    GCon->Logf(NAME_Debug, "creating new camera FBO #%d for texture #%d (new size is %dx%d)", cfidx, texnum, width, height);
    CameraFBOInfo *cin = new CameraFBOInfo();
    cin->index = cfidx;
    cameraFBOList.append(cin);
    vassert(cameraFBOList.length()-1 == cfidx);
  }

  CameraFBOInfo *ci = cameraFBOList[cfidx];
  vassert(ci->index == cfidx);
  ci->texnum = texnum;
  ci->camwidth = width;
  ci->camheight = height;
  ci->fbo.createDepthStencil(this, width, height);

  glBindFramebuffer(GL_FRAMEBUFFER, ci->fbo.getFBOid());
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // black background
  glClearDepth(!useReverseZ ? 1.0f : 0.0f);
  if (p_glClipControl) p_glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE); // actually, this is better even for "normal" cases
  RestoreDepthFunc();
  glDepthRange(0.0f, 1.0f);

  glClearStencil(0);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

  ReactivateCurrentFBO();

  //GCon->Logf(NAME_Debug, "  FBO #%d tid=%u", cfidx, ci->fbo.getColorTid());

  return cfidx;
}


//==========================================================================
//
//  VOpenGLDrawer::FindCameraFBO
//
//  returns index or -1
//
//==========================================================================
int VOpenGLDrawer::FindCameraFBO (int texnum) {
  if (texnum < 0) return -1;
  for (auto &&cf : cameraFBOList) if (cf->texnum == texnum) return cf->index;
  return -1;
}


//==========================================================================
//
//  VOpenGLDrawer::SetCameraFBO
//
//==========================================================================
void VOpenGLDrawer::SetCameraFBO (int cfboindex) {
  if (cfboindex < 0 || cfboindex >= cameraFBOList.length()) return;
  if (currMainFBO != cfboindex) {
    currMainFBO = cfboindex;
    cameraFBOList[cfboindex]->fbo.activate();
    stencilBufferDirty = true;
  }
}


//==========================================================================
//
//  VOpenGLDrawer::GetCameraFBOTextureId
//
//  returns 0 if cfboindex is invalid
//
//==========================================================================
GLuint VOpenGLDrawer::GetCameraFBOTextureId (int cfboindex) {
  if (cfboindex < 0 || cfboindex >= cameraFBOList.length()) return 0;
  return cameraFBOList[cfboindex]->fbo.getColorTid();
  //glBindTexture(GL_TEXTURE_2D, cameraFBOList[cfboindex]->fbo.getColorTid());
  //glBindTexture(GL_TEXTURE_2D, 10);
}


//==========================================================================
//
//  VOpenGLDrawer::ActivateMainFBO
//
//==========================================================================
void VOpenGLDrawer::ActivateMainFBO () {
  if (currMainFBO < 0) {
    mainFBO.activate();
  } else {
    cameraFBOList[currMainFBO]->fbo.activate();
  }
}


//==========================================================================
//
//  VOpenGLDrawer::GetMainFBO
//
//==========================================================================
VOpenGLDrawer::FBO *VOpenGLDrawer::GetMainFBO () {
  return (currMainFBO < 0 ? &mainFBO : &cameraFBOList[currMainFBO]->fbo);
}


//==========================================================================
//
//  VOpenGLDrawer::SetupView
//
//==========================================================================
void VOpenGLDrawer::SetupView (VRenderLevelDrawer *ARLev, const refdef_t *rd) {
  RendLev = ARLev;

  if (!rd->DrawCamera && rd->drawworld && rd->width != ScreenWidth) {
    // draws the border around the view for different size windows
    R_DrawViewBorder();
  }

  glBindTexture(GL_TEXTURE_2D, 0);

  VMatrix4 ProjMat;
  if (!CanUseRevZ()) {
    // normal
    glClearDepth(1.0f);
    glDepthFunc(GL_LEQUAL);
    ProjMat.SetIdentity();
    ProjMat[0][0] = 1.0f/rd->fovx;
    ProjMat[1][1] = 1.0f/rd->fovy;
    ProjMat[2][3] = -1.0f;
    ProjMat[3][3] = 0.0f;
    if (RendLev && RendLev->NeedsInfiniteFarClip && !HaveDepthClamp) {
      ProjMat[2][2] = -1.0f;
      ProjMat[3][2] = -2.0f;
    } else {
      ProjMat[2][2] = -(gl_maxdist+1.0f)/(gl_maxdist-1.0f);
      ProjMat[3][2] = -2.0f*gl_maxdist/(gl_maxdist-1.0f);
    }
  } else {
    // reversed
    // see https://nlguillemot.wordpress.com/2016/12/07/reversed-z-in-opengl/
    glClearDepth(0.0f);
    glDepthFunc(GL_GEQUAL);
    ProjMat.SetZero();
    ProjMat[0][0] = 1.0f/rd->fovx;
    ProjMat[1][1] = 1.0f/rd->fovy;
    ProjMat[2][3] = -1.0f;
    ProjMat[3][2] = 1.0f; // zNear
  }
  //RestoreDepthFunc();

  glClear(GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT|(rd->drawworld && !rd->DrawCamera && clear ? GL_COLOR_BUFFER_BIT : 0));
  stencilBufferDirty = false;

  glViewport(rd->x, getHeight()-rd->height-rd->y, rd->width, rd->height);

  glMatrixMode(GL_PROJECTION);
  glLoadMatrixf(ProjMat[0]);

  glMatrixMode(GL_MODELVIEW);

  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

  glEnable(GL_CULL_FACE);
  glCullFace(GL_FRONT);

  glEnable(GL_DEPTH_TEST);
  //GLDisableBlend();
  GLEnableBlend();
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_ALPHA_TEST);
  if (RendLev && RendLev->NeedsInfiniteFarClip && HaveDepthClamp) glEnable(GL_DEPTH_CLAMP);
  //k8: there is no reason to not do it
  //if (HaveDepthClamp) glEnable(GL_DEPTH_CLAMP);

  glEnable(GL_TEXTURE_2D);
  glDisable(GL_STENCIL_TEST);
  glDepthMask(GL_TRUE); // allow z-buffer writes
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  glDisable(GL_SCISSOR_TEST);
  currentSVScissor[SCS_MINX] = currentSVScissor[SCS_MINY] = 0;
  currentSVScissor[SCS_MAXX] = currentSVScissor[SCS_MAXY] = 32000;
}


//==========================================================================
//
//  VOpenGLDrawer::SetupViewOrg
//
//==========================================================================
void VOpenGLDrawer::SetupViewOrg () {
  glLoadIdentity();
  glRotatef(-90, 1, 0, 0);
  glRotatef(90, 0, 0, 1);
  if (MirrorFlip) {
    glScalef(1, -1, 1);
    glCullFace(GL_BACK);
  } else {
    glCullFace(GL_FRONT);
  }
  glRotatef(-viewangles.roll, 1, 0, 0);
  glRotatef(-viewangles.pitch, 0, 1, 0);
  glRotatef(-viewangles.yaw, 0, 0, 1);
  glTranslatef(-vieworg.x, -vieworg.y, -vieworg.z);

  if (MirrorClip && view_frustum.planes[5].isValid()) {
    glEnable(GL_CLIP_PLANE0);
    const GLdouble eq[4] = {
      view_frustum.planes[5].normal.x, view_frustum.planes[5].normal.y, view_frustum.planes[5].normal.z,
      -view_frustum.planes[5].dist
    };
    glClipPlane(GL_CLIP_PLANE0, eq);
  } else {
    glDisable(GL_CLIP_PLANE0);
  }
  LoadVPMatrices();
}


//==========================================================================
//
//  VOpenGLDrawer::EndView
//
//==========================================================================
void VOpenGLDrawer::EndView (bool ignoreColorTint) {
  Setup2D();

  if (!ignoreColorTint && cl && cl->CShift) {
    DrawFixedCol.Activate();
    DrawFixedCol.SetColor(
      (float)((cl->CShift>>16)&255)/255.0f,
      (float)((cl->CShift>>8)&255)/255.0f,
      (float)(cl->CShift&255)/255.0f,
      (float)((cl->CShift>>24)&255)/255.0f);
    DrawFixedCol.UploadChangedUniforms();
    //GLEnableBlend();
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);

    glBegin(GL_QUADS);
      glVertex2f(0, 0);
      glVertex2f(getWidth(), 0);
      glVertex2f(getWidth(), getHeight());
      glVertex2f(0, getHeight());
    glEnd();

    //GLDisableBlend();
    glEnable(GL_TEXTURE_2D);
  }
}


//==========================================================================
//
//  VOpenGLDrawer::ReadScreen
//
//==========================================================================
void *VOpenGLDrawer::ReadScreen (int *bpp, bool *bot2top) {
  GLint oldbindtex = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldbindtex);

  glBindTexture(GL_TEXTURE_2D, GetMainFBO()->getColorTid());
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  void *dst = Z_Malloc(GetMainFBO()->getWidth()*GetMainFBO()->getHeight()*3);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, dst);
  glBindTexture(GL_TEXTURE_2D, oldbindtex);

  *bpp = 24;
  *bot2top = true;
  return dst;
}


//==========================================================================
//
//  VOpenGLDrawer::ReadFBOPixels
//
//==========================================================================
void VOpenGLDrawer::ReadFBOPixels (FBO *srcfbo, int Width, int Height, rgba_t *Dest) {
  if (!srcfbo || Width < 1 || Height < 1 || !Dest) return;

  const int fbowidth = srcfbo->getWidth();
  const int fboheight = srcfbo->getHeight();

  if (fbowidth < 1 || fboheight < 1) {
    memset((void *)Dest, 0, Width*Height*sizeof(rgba_t));
    return;
  }

  if (readBackTempBufSize < fbowidth*fboheight*4) {
    readBackTempBufSize = fbowidth*fboheight*4;
    readBackTempBuf = (vuint8 *)Z_Realloc(readBackTempBuf, readBackTempBufSize);
  }

  GLint oldbindtex = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldbindtex);

  glBindTexture(GL_TEXTURE_2D, srcfbo->getColorTid());
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  rgba_t *temp = (rgba_t *)readBackTempBuf;
  vassert(temp);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, temp);
  glBindTexture(GL_TEXTURE_2D, oldbindtex);

  if (Width <= fbowidth) {
    size_t blen = Width*sizeof(rgba_t);
    for (int y = 0; y < Height; ++y) memcpy(Dest+y*Width, temp+(fboheight-y-1)*fbowidth, blen);
  } else {
    size_t blen = fbowidth*sizeof(rgba_t);
    size_t restlen = Width*sizeof(rgba_t)-blen;
    for (int y = 0; y < Height; ++y) {
      memcpy(Dest+y*Width, temp+(fboheight-y-1)*fbowidth, blen);
      memset((void *)(Dest+y*Width+fbowidth), 0, restlen);
    }
  }
}


//==========================================================================
//
//  VOpenGLDrawer::ReadBackScreen
//
//==========================================================================
void VOpenGLDrawer::ReadBackScreen (int Width, int Height, rgba_t *Dest) {
  ReadFBOPixels(GetMainFBO(), Width, Height, Dest);
}


//==========================================================================
//
//  VOpenGLDrawer::SetFade
//
//==========================================================================
void VOpenGLDrawer::SetFade (vuint32 NewFade) {
  if ((vuint32)CurrentFade == NewFade) return;

  if (NewFade) {
    //static GLenum fogMode[4] = { GL_LINEAR, GL_LINEAR, GL_EXP, GL_EXP2 };
    float fogColor[4];

    fogColor[0] = float((NewFade>>16)&255)/255.0f;
    fogColor[1] = float((NewFade>>8)&255)/255.0f;
    fogColor[2] = float(NewFade&255)/255.0f;
    fogColor[3] = float((NewFade>>24)&255)/255.0f;
    //glFogi(GL_FOG_MODE, fogMode[r_fog&3]);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogfv(GL_FOG_COLOR, fogColor);
    if (NewFade == FADE_LIGHT) {
      glFogf(GL_FOG_DENSITY, 0.3f);
      glFogf(GL_FOG_START, 1.0f);
      glFogf(GL_FOG_END, 1024.0f*r_fade_factor);
    } else {
      glFogf(GL_FOG_DENSITY, r_fog_density);
      glFogf(GL_FOG_START, r_fog_start);
      glFogf(GL_FOG_END, r_fog_end);
    }
    //glHint(GL_FOG_HINT, r_fog < 4 ? GL_DONT_CARE : GL_NICEST);
    glHint(GL_FOG_HINT, GL_DONT_CARE);
    glEnable(GL_FOG);
  } else {
    glDisable(GL_FOG);
  }
  CurrentFade = NewFade;
}


//==========================================================================
//
//  VOpenGLDrawer::DebugRenderScreenRect
//
//==========================================================================
void VOpenGLDrawer::DebugRenderScreenRect (int x0, int y0, int x1, int y1, vuint32 color) {
  glPushAttrib(/*GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_ENABLE_BIT|GL_VIEWPORT_BIT|GL_TRANSFORM_BIT*/GL_ALL_ATTRIB_BITS);
  bool oldBlend = blendEnabled;

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();

  //glColor4f(((color>>16)&0xff)/255.0f, ((color>>8)&0xff)/255.0f, (color&0xff)/255.0f, ((color>>24)&0xff)/255.0f);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  GLEnableBlend();
  //glDisable(GL_STENCIL_TEST);
  //glDisable(GL_SCISSOR_TEST);
  glDisable(GL_TEXTURE_2D);
  glDepthMask(GL_FALSE); // no z-buffer writes
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  //p_glUseProgramObjectARB(0);

  DrawFixedCol.Activate();
  DrawFixedCol.SetColor(
    (GLfloat)(((color>>16)&255)/255.0f),
    (GLfloat)(((color>>8)&255)/255.0f),
    (GLfloat)((color&255)/255.0f), ((color>>24)&0xff)/255.0f);
  DrawFixedCol.UploadChangedUniforms();

  glOrtho(0, getWidth(), getHeight(), 0, -666, 666);
  glBegin(GL_QUADS);
    glVertex2i(x0, y0);
    glVertex2i(x1, y0);
    glVertex2i(x1, y1);
    glVertex2i(x0, y1);
  glEnd();

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();

  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();

  glPopAttrib();
  p_glUseProgramObjectARB(0);
  currentActiveShader = nullptr;
  blendEnabled = oldBlend;
}


//==========================================================================
//
//  VOpenGLDrawer::ForceClearStencilBuffer
//
//==========================================================================
void VOpenGLDrawer::ForceClearStencilBuffer () {
  NoteStencilBufferDirty();
  ClearStencilBuffer();
}


//==========================================================================
//
//  VOpenGLDrawer::ForceMarkStencilBufferDirty
//
//==========================================================================
void VOpenGLDrawer::ForceMarkStencilBufferDirty () {
  NoteStencilBufferDirty();
}


//==========================================================================
//
//  VOpenGLDrawer::EnableBlend
//
//==========================================================================
void VOpenGLDrawer::EnableBlend () {
  GLEnableBlend();
}


//==========================================================================
//
//  VOpenGLDrawer::DisableBlend
//
//==========================================================================
void VOpenGLDrawer::DisableBlend () {
  GLDisableBlend();
}


//==========================================================================
//
//  VOpenGLDrawer::GLSetBlendEnabled
//
//==========================================================================
void VOpenGLDrawer::SetBlendEnabled (const bool v) {
  GLSetBlendEnabled(v);
}


//==========================================================================
//
//  VOpenGLDrawer::DeactivateShader
//
//==========================================================================
void VOpenGLDrawer::DeactivateShader () {
  p_glUseProgramObjectARB(0);
  currentActiveShader = nullptr;
}


//==========================================================================
//
//  VOpenGLDrawer::PrepareWipe
//
//  this copies main FBO to wipe FBO, so we can run wipe shader
//
//==========================================================================
void VOpenGLDrawer::PrepareWipe () {
  mainFBO.blitTo(&wipeFBO, 0, 0, mainFBO.getWidth(), mainFBO.getHeight(), 0, 0, mainFBO.getWidth(), mainFBO.getHeight(), GL_NEAREST);
}


//==========================================================================
//
//  VOpenGLDrawer::RenderWipe
//
//  render wipe from wipe to main FBO
//  should be called after `StartUpdate()`
//  and (possibly) rendering something to the main FBO
//  time is in seconds, from zero to...
//  returns `false` if wipe is complete
//  -1 means "show saved wipe screen"
//
//==========================================================================
bool VOpenGLDrawer::RenderWipe (float time) {
  static const float WipeDur = 1.0f;

  if (time < 0.0f) {
    wipeFBO.blitTo(&mainFBO, 0, 0, mainFBO.getWidth(), mainFBO.getHeight(), 0, 0, mainFBO.getWidth(), mainFBO.getHeight(), GL_NEAREST);
    return true;
  }

  //GCon->Logf(NAME_Debug, "WIPE: time=%g", time);

  glPushAttrib(GL_ALL_ATTRIB_BITS);
  bool oldBlend = blendEnabled;

  glViewport(0, 0, getWidth(), getHeight());

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0, getWidth(), getHeight(), 0, -666, 666);

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  if (HaveDepthClamp) glDisable(GL_DEPTH_CLAMP);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  //GLEnableBlend();
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_SCISSOR_TEST);
  glDepthMask(GL_FALSE); // no z-buffer writes
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

  GLEnableBlend();
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // premultiplied

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, wipeFBO.getColorTid());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  DrawWipeType0.Activate();
  DrawWipeType0.SetTexture(0);
  DrawWipeType0.SetWipeTime(time);
  DrawWipeType0.SetWipeDuration(WipeDur);
  DrawWipeType0.SetScreenSize((float)getWidth(), (float)getHeight());
  DrawWipeType0.UploadChangedUniforms();

  glBegin(GL_QUADS);
    glTexCoord2f(0, 1); glVertex2f(0, 0);
    glTexCoord2f(1, 1); glVertex2f(getWidth(), 0);
    glTexCoord2f(1, 0); glVertex2f(getWidth(), getHeight());
    glTexCoord2f(0, 0); glVertex2f(0, getHeight());
  glEnd();

  //GLDisableBlend();
  glBindTexture(GL_TEXTURE_2D, 0);

  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();

  glPopAttrib();
  p_glUseProgramObjectARB(0);
  currentActiveShader = nullptr;
  blendEnabled = oldBlend;

  //wipeFBO.blitTo(&mainFBO, 0, 0, mainFBO.getWidth(), mainFBO.getHeight(), 0, 0, mainFBO.getWidth(), mainFBO.getHeight(), GL_NEAREST);
  return (time <= WipeDur);
}



//==========================================================================
//
//  readTextFile
//
//==========================================================================
static VStr readTextFile (VStr fname) {
  VStream *strm = FL_OpenFileReadBaseOnly(fname);
  if (!strm) Sys_Error("Failed to open shader '%s'", *fname);
  int size = strm->TotalSize();
  if (size == 0) return VStr();
  VStr res;
  res.setLength(size, 0);
  strm->Serialise(res.GetMutableCharPointer(0), size);
  delete strm;
  return res;
}


//==========================================================================
//
//  isEmptyLine
//
//==========================================================================
static bool isEmptyLine (VStr s) {
  int pos = 0;
  while (pos < s.length()) {
    if ((vuint8)s[pos] > ' ') return false;
    ++pos;
  }
  return true;
}


//==========================================================================
//
//  isCommentLine
//
//==========================================================================
static bool isCommentLine (VStr s) {
  if (s.length() < 2) return false;
  int pos = 0;
  while (pos+1 < s.length()) {
    if (s[pos] == '/' && s[pos+1] == '/') return true;
    if ((vuint8)s[pos] > ' ') return false;
    ++pos;
  }
  return false;
}


//==========================================================================
//
//  isVersionLine
//
//==========================================================================
static bool isVersionLine (VStr s) {
  if (s.length() < 2) return false;
  int pos = 0;
  while (pos < s.length()) {
    if ((vuint8)s[pos] == '#') {
      ++pos;
      while (pos < s.length() && (vuint8)s[pos] <= ' ') ++pos;
      if (pos >= s.length()) return false;
      if (s[pos+0] == 'v' && s[pos+1] == 'e' && s[pos+2] == 'r' && s[pos+3] == 's' &&
          s[pos+4] == 'i' && s[pos+5] == 'o' && s[pos+6] == 'n') return true;
      return false;
    }
    if ((vuint8)s[pos] > ' ') return false;
    ++pos;
  }
  return false;
}


//==========================================================================
//
//  getDirective
//
//==========================================================================
static VStr getDirective (VStr s) {
  int pos = 0;
  while (pos+1 < s.length()) {
    if (s[pos] == '$') {
      ++pos;
      while (pos < s.length() && (vuint8)s[pos] <= ' ') ++pos;
      if (pos >= s.length()) return VStr("$");
      int start = pos;
      while (pos < s.length() && (vuint8)s[pos] > ' ') ++pos;
      return s.mid(start, pos-start);
    }
    if ((vuint8)s[pos] > ' ') return VStr();
    ++pos;
  }
  return VStr();
}


//==========================================================================
//
//  getDirectiveArg
//
//==========================================================================
static VStr getDirectiveArg (VStr s) {
  int pos = 0;
  while (pos+1 < s.length()) {
    if (s[pos] == '$') {
      ++pos;
      while (pos < s.length() && (vuint8)s[pos] <= ' ') ++pos;
      if (pos >= s.length()) return VStr();
      while (pos < s.length() && (vuint8)s[pos] > ' ') ++pos;
      while (pos < s.length() && (vuint8)s[pos] <= ' ') ++pos;
      if (pos >= s.length()) return VStr();
      if (s[pos] == '"') {
        int start = ++pos;
        while (pos < s.length() && s[pos] != '"') ++pos;
        return s.mid(start, pos-start);
      } else {
        int start = pos;
        while (pos < s.length() && (vuint8)s[pos] > ' ') ++pos;
        return s.mid(start, pos-start);
      }
    }
    if ((vuint8)s[pos] > ' ') return VStr();
    ++pos;
  }
  return VStr();
}


//==========================================================================
//
//  VOpenGLDrawer::LoadShader
//
//==========================================================================
GLhandleARB VOpenGLDrawer::LoadShader (const char *progname, const char *incdircs, GLenum Type, VStr FileName, const TArray<VStr> &defines) {
  // load source file
  VStr ssrc = readTextFile(FileName);

  const char *sotype = (Type == GL_VERTEX_SHADER_ARB ? "vertex" : "fragment");

  // create shader object
  GLhandleARB Shader = p_glCreateShaderObjectARB(Type);
  if (!Shader) Sys_Error("Failed to create %s shader object for shader '%s'", sotype, progname);
  CreatedShaderObjects.Append(Shader);

  // build source text
  bool needToAddRevZ = CanUseRevZ();
  bool needToAddDefines = (needToAddRevZ || defines.length() > 0);

  VStr incdir(incdircs);
  incdir = incdir.fixSlashes();
  if (!incdir.isEmpty()) {
    incdir = incdir.appendTrailingSlash();
  } else {
    incdir = FileName.extractFilePath();
  }

  //GCon->Logf(NAME_Debug, "<%s>: incdir: <%s> (%s); file: <%s>", progname, incdircs, *incdir, *FileName);

  // process $include, add defines
  //FIXME: nested "$include", and proper directive parsing
  VStr res;
  while (ssrc.length()) {
    // find line end
    int epos = 0;
    while (epos < ssrc.length() && ssrc[epos] != '\n') ++epos;
    if (epos < ssrc.length()) ++epos; // skip "\n"
    // extract line
    VStr line = ssrc.left(epos);
    ssrc.chopLeft(epos);
    if (isEmptyLine(line)) { res += line; continue; }
    if (isCommentLine(line)) { res += line; continue; }
    // add "reverse z" define
    VStr cmd = getDirective(line);
    if (cmd.length() == 0) {
      if (needToAddDefines) {
        if (isVersionLine(line)) { res += line; continue; }
        if (needToAddRevZ) { res += "#define VAVOOM_REVERSE_Z\n"; needToAddRevZ = false; }
        // add other defines
        for (int didx = 0; didx < defines.length(); ++didx) {
          VStr def = defines[didx];
          if (def.isEmpty()) continue;
          res += "#define ";
          res += def;
          res += "\n";
        }
        needToAddDefines = false;
      }
      res += line;
      continue;
    }
    if (cmd != "include") Sys_Error("%s", va("invalid directive \"%s\" in shader '%s'", *cmd, *FileName));
    VStr fname = getDirectiveArg(line);
    if (fname.length() == 0) Sys_Error("%s", va("directive \"%s\" in shader '%s' expects file name", *cmd, *FileName));
    VStr incf = readTextFile(incdir+fname);
    if (incf.length() && incf[incf.length()-1] != '\n') incf += '\n';
    incf += ssrc;
    ssrc = incf;
  }
  //if (defines.length()) GCon->Logf("%s", *res);
  //fprintf(stderr, "================ %s ================\n%s\n=================================\n", *FileName, *res);
  //vsShaderSrc = res;

  // upload source text
  const GLcharARB *ShaderText = *res;
  p_glShaderSourceARB(Shader, 1, &ShaderText, nullptr);

  // compile it
  p_glCompileShaderARB(Shader);

  // check id it is compiled successfully
  GLint Status;
  p_glGetObjectParameterivARB(Shader, GL_OBJECT_COMPILE_STATUS_ARB, &Status);
  if (!Status) {
    GLcharARB LogText[1024];
    GLsizei LogLen;
    p_glGetInfoLogARB(Shader, sizeof(LogText)-1, &LogLen, LogText);
    LogText[LogLen] = 0;
    GCon->Logf(NAME_Error, "FAILED to compile %s shader '%s'!", sotype, progname);
    GCon->Logf(NAME_Error, "%s\n", LogText);
    GCon->Logf(NAME_Error, "====\n%s\n====", *res);
    //fprintf(stderr, "================ %s ================\n%s\n=================================\n%s\b", *FileName, *res, LogText);
    //Sys_Error("%s", va("Failed to compile shader %s: %s", *FileName, LogText));
    Sys_Error("Failed to compile %s shader %s!", sotype, progname);
  }
  return Shader;
}


//==========================================================================
//
//  VOpenGLDrawer::CreateProgram
//
//==========================================================================
GLhandleARB VOpenGLDrawer::CreateProgram (const char *progname, GLhandleARB VertexShader, GLhandleARB FragmentShader) {
  // create program object
  (void)glGetError();
  GLhandleARB Program = p_glCreateProgramObjectARB();
  if (!Program) Sys_Error("Failed to create program object");
  CreatedShaderObjects.Append(Program);

  // attach shaders
  p_glAttachObjectARB(Program, VertexShader);
  p_glAttachObjectARB(Program, FragmentShader);

  // link program
  (void)glGetError();
  p_glLinkProgramARB(Program);

  // check if it was linked successfully
  GLint Status;
  p_glGetObjectParameterivARB(Program, GL_OBJECT_LINK_STATUS_ARB, &Status);
  if (!Status) {
    GLcharARB LogText[1024];
    GLsizei LogLen;
    p_glGetInfoLogARB(Program, sizeof(LogText)-1, &LogLen, LogText);
    LogText[LogLen] = 0;
    Sys_Error("Failed to link program '%s'", LogText);
  }

  if (glGetError() != 0) Sys_Error("Failed to link program '%s' for unknown reason", progname);

  return Program;
}



//==========================================================================
//
//  VOpenGLDrawer::FBO::FBO
//
//==========================================================================
VOpenGLDrawer::FBO::FBO ()
  : mOwner(nullptr)
  , mFBO(0)
  , mColorTid(0)
  , mDepthStencilTid(0)
  , mHasDepthStencil(false)
  , mWidth(0)
  , mHeight(0)
{
}


//==========================================================================
//
//  VOpenGLDrawer::FBO::~FBO
//
//==========================================================================
VOpenGLDrawer::FBO::~FBO () {
  destroy();
}


//==========================================================================
//
//  VOpenGLDrawer::FBO::destroy
//
//==========================================================================
void VOpenGLDrawer::FBO::destroy () {
  if (!mOwner) return;
  mOwner->glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
  mOwner->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
  if (mHasDepthStencil) mOwner->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
  mOwner->glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDeleteTextures(1, &mColorTid);
  if (mHasDepthStencil) glDeleteTextures(1, &mDepthStencilTid);
  mOwner->glDeleteFramebuffers(1, &mFBO);
  mFBO = 0;
  mColorTid = 0;
  mDepthStencilTid = 0;
  mHasDepthStencil = false;
  mWidth = 0;
  mHeight = 0;
  if (mOwner->currentActiveFBO == this) mOwner->currentActiveFBO = nullptr;
  mOwner->ReactivateCurrentFBO();
  mOwner = nullptr;
}


//==========================================================================
//
//  VOpenGLDrawer::FBO::createInternal
//
//==========================================================================
void VOpenGLDrawer::FBO::createInternal (VOpenGLDrawer *aowner, int awidth, int aheight, bool createDepthStencil, bool mirroredRepeat) {
  destroy();
  vassert(aowner);
  vassert(awidth > 0);
  vassert(aheight > 0);
  vassert(aowner->currentActiveFBO != this);

  GLint oldbindtex = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldbindtex);

  // allocate FBO object
  aowner->glGenFramebuffers(1, &mFBO);
  if (mFBO == 0) Sys_Error("OpenGL: cannot create FBO");
  aowner->glBindFramebuffer(GL_FRAMEBUFFER, mFBO);

  // attach 2D texture to this FBO
  glGenTextures(1, &mColorTid);
  if (mColorTid == 0) Sys_Error("OpenGL: cannot create RGBA texture for FBO");
  glBindTexture(GL_TEXTURE_2D, mColorTid);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

  if (mirroredRepeat) {
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
  } else {
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, aowner->ClampToEdge);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, aowner->ClampToEdge);
  }

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (mLinearFilter ? GL_LINEAR : GL_NEAREST));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (mLinearFilter ? GL_LINEAR : GL_NEAREST));
  if (aowner->anisotropyExists) glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT), 1.0f); // 1 is minimum, i.e. "off"

  // empty texture
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, awidth, aheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  aowner->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mColorTid, 0);

  // attach stencil texture to this FBO
  if (createDepthStencil) {
    glGenTextures(1, &mDepthStencilTid);
    if (mDepthStencilTid == 0) Sys_Error("OpenGL: cannot create stencil texture for main FBO");
    glBindTexture(GL_TEXTURE_2D, mDepthStencilTid);

    GLint major, minor;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);

    GLint depthStencilFormat = GL_DEPTH24_STENCIL8;
    // there is (almost) no reason to use fp depth buffer without reverse z
    // also, reverse z is perfectly working with int24 depth buffer, see http://www.reedbeta.com/blog/depth-precision-visualized/
    if (major >= 3 && gl_enable_fp_zbuffer) {
      depthStencilFormat = GL_DEPTH32F_STENCIL8;
      GCon->Logf(NAME_Init, "OpenGL: using floating-point depth buffer");
    }

    (void)glGetError();

    glTexImage2D(GL_TEXTURE_2D, 0, depthStencilFormat, awidth, aheight, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
    if (glGetError() != 0) {
      if (depthStencilFormat == GL_DEPTH32F_STENCIL8) {
        GCon->Log(NAME_Init, "OpenGL: cannot create fp depth buffer, trying 24-bit one");
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, awidth, aheight, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
        if (glGetError() != 0) Sys_Error("OpenGL initialization error");
      } else {
        Sys_Error("OpenGL initialization error");
      }
    }
    aowner->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, mDepthStencilTid, 0);

    {
      GLenum status = aowner->glCheckFramebufferStatus(GL_FRAMEBUFFER);
      if (status != GL_FRAMEBUFFER_COMPLETE) Sys_Error("OpenGL: framebuffer creation failed");
    }
  }

  mHasDepthStencil = createDepthStencil;
  mWidth = awidth;
  mHeight = aheight;
  mOwner = aowner;

  glBindTexture(GL_TEXTURE_2D, oldbindtex);

  mOwner->ReactivateCurrentFBO();
}


//==========================================================================
//
//  VOpenGLDrawer::FBO::createTextureOnly
//
//==========================================================================
void VOpenGLDrawer::FBO::createTextureOnly (VOpenGLDrawer *aowner, int awidth, int aheight, bool mirroredRepeat) {
  createInternal(aowner, awidth, aheight, false, mirroredRepeat);
}


//==========================================================================
//
//  VOpenGLDrawer::FBO::createDepthStencil
//
//==========================================================================
void VOpenGLDrawer::FBO::createDepthStencil (VOpenGLDrawer *aowner, int awidth, int aheight, bool mirroredRepeat) {
  createInternal(aowner, awidth, aheight, true, mirroredRepeat);
}


//==========================================================================
//
//  VOpenGLDrawer::FBO::activate
//
//==========================================================================
void VOpenGLDrawer::FBO::activate () {
  if (mOwner && mOwner->currentActiveFBO != this) {
    mOwner->currentActiveFBO = this;
    mOwner->ReactivateCurrentFBO();
  }
}


//==========================================================================
//
//  VOpenGLDrawer::FBO::deactivate
//
//==========================================================================
void VOpenGLDrawer::FBO::deactivate () {
  if (mOwner && mOwner->currentActiveFBO != nullptr) {
    mOwner->currentActiveFBO = nullptr;
    mOwner->ReactivateCurrentFBO();
  }
}


//==========================================================================
//
//  VOpenGLDrawer::FBO::blitTo
//
//  this blits only color info
//
//==========================================================================
void VOpenGLDrawer::FBO::blitTo (FBO *dest, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                                 GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLenum filter)
{
  if (!mOwner || !dest || !dest->mOwner) return;

  if (mOwner->p_glBlitFramebuffer && !gl_dbg_fbo_blit_with_texture) {
    mOwner->glBindFramebuffer(GL_READ_FRAMEBUFFER, mFBO);
    mOwner->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dest->mFBO);
    mOwner->p_glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, GL_COLOR_BUFFER_BIT, filter);
    mOwner->glBindFramebuffer(GL_READ_FRAMEBUFFER, mFBO);
    mOwner->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mFBO);
  } else {
    GLint oldbindtex = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldbindtex);
    glPushAttrib(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_ENABLE_BIT|GL_VIEWPORT_BIT|GL_TRANSFORM_BIT);
    bool oldBlend = mOwner->blendEnabled;

    mOwner->glBindFramebuffer(GL_FRAMEBUFFER, dest->mFBO);
    glBindTexture(GL_TEXTURE_2D, mColorTid);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    mOwner->GLDisableBlend();
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_TEXTURE_2D);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    mOwner->p_glUseProgramObjectARB(0);
    mOwner->currentActiveShader = nullptr;

    const float mywf = (float)mWidth;
    const float myhf = (float)mHeight;

    const float mytx0 = (float)srcX0/mywf;
    const float myty0 = (float)srcY0/myhf;
    const float mytx1 = (float)srcX1/mywf;
    const float myty1 = (float)srcY1/myhf;

    glOrtho(0, dest->mWidth, dest->mHeight, 0, -666, 666);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, /*GL_LINEAR*/filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, /*GL_LINEAR*/filter);
    glBegin(GL_QUADS);
      glTexCoord2f(mytx0, myty1); glVertex2i(dstX0, dstY0);
      glTexCoord2f(mytx1, myty1); glVertex2i(dstX1, dstY0);
      glTexCoord2f(mytx1, myty0); glVertex2i(dstX1, dstY1);
      glTexCoord2f(mytx0, myty0); glVertex2i(dstX0, dstY1);
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glPopAttrib();
    glBindTexture(GL_TEXTURE_2D, oldbindtex);
    mOwner->blendEnabled = oldBlend;
  }
  mOwner->ReactivateCurrentFBO();
}


//==========================================================================
//
//  VOpenGLDrawer::FBO::blitToScreen
//
//==========================================================================
void VOpenGLDrawer::FBO::blitToScreen () {
  if (!mOwner) return;

  mOwner->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // screen FBO

  int realw, realh;
  mOwner->GetRealWindowSize(&realw, &realh);

  if (mOwner->p_glBlitFramebuffer && !gl_dbg_fbo_blit_with_texture) {
    glBindTexture(GL_TEXTURE_2D, 0);
    mOwner->glBindFramebuffer(GL_READ_FRAMEBUFFER, mFBO);
    if (realw == mWidth && realh == mHeight) {
      mOwner->p_glBlitFramebuffer(0, 0, mWidth, mHeight, 0, 0, mWidth, mHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    } else {
      mOwner->p_glBlitFramebuffer(0, 0, mWidth, mHeight, 0, 0, realw, realh, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }
  } else {
    GLint oldbindtex = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldbindtex);
    glPushAttrib(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_ENABLE_BIT|GL_VIEWPORT_BIT|GL_TRANSFORM_BIT);
    bool oldBlend = mOwner->blendEnabled;

    glViewport(0, 0, realw, realh);

    glBindTexture(GL_TEXTURE_2D, mColorTid);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrtho(0, realw, realh, 0, -666, 666);

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    mOwner->GLDisableBlend();
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_TEXTURE_2D);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    mOwner->p_glUseProgramObjectARB(0);
    mOwner->currentActiveShader = nullptr;

    if (realw == mWidth && realh == mHeight) {
      // copy texture by drawing full quad
      //glOrtho(0, mWidth, mHeight, 0, -666, 666);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f); glVertex2i(0, 0);
        glTexCoord2f(1.0f, 1.0f); glVertex2i(mWidth, 0);
        glTexCoord2f(1.0f, 0.0f); glVertex2i(mWidth, mHeight);
        glTexCoord2f(0.0f, 0.0f); glVertex2i(0, mHeight);
      glEnd();
    } else {
      //glOrtho(0, realw, realh, 0, -99999, 99999);
      //glClear(GL_COLOR_BUFFER_BIT); // just in case

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

      // scale it properly
      float scaleX = float(realw)/float(mWidth);
      float scaleY = float(realh)/float(mHeight);
      float scale = (scaleX <= scaleY ? scaleX : scaleY);
      int newWidth = (int)(mWidth*scale);
      int newHeight = (int)(mHeight*scale);
      int x0 = (realw-newWidth)/2;
      int y0 = (realh-newHeight)/2;
      int x1 = x0+newWidth;
      int y1 = y0+newHeight;
      //fprintf(stderr, "scaleX=%f; scaleY=%f; scale=%f; real=(%d,%d); screen=(%d,%d); new=(%d,%d); rect:(%d,%d)-(%d,%d)\n", scaleX, scaleY, scale, realw, realh, getWidth(), getHeight(), newWidth, newHeight, x0, y0, x1, y1);
      glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f); glVertex2i(x0, y0);
        glTexCoord2f(1.0f, 1.0f); glVertex2i(x1, y0);
        glTexCoord2f(1.0f, 0.0f); glVertex2i(x1, y1);
        glTexCoord2f(0.0f, 0.0f); glVertex2i(x0, y1);
      glEnd();
    }

    glPopAttrib();
    glBindTexture(GL_TEXTURE_2D, oldbindtex);
    mOwner->blendEnabled = oldBlend;
  }

  mOwner->ReactivateCurrentFBO();
}
