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
//**  Copyright (C) 2018-2023 Ketmar Dark
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
// not done yet
#define VV_SHADER_COMPILING_PROGRESS

#include <limits.h>
#include <float.h>
#include <stdarg.h>

#include "../../screen.h"
#include "gl_local.h"

#ifdef VV_SHADER_COMPILING_PROGRESS
#include "splashfont.inc"
#endif

#ifndef GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX
# define GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX          0x9047
#endif
#ifndef GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX
# define GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX    0x9048
#endif
#ifndef GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX
# define GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX  0x9049
#endif
#ifndef GPU_MEMORY_INFO_EVICTION_COUNT_NVX
# define GPU_MEMORY_INFO_EVICTION_COUNT_NVX            0x904A
#endif
#ifndef GPU_MEMORY_INFO_EVICTED_MEMORY_NVX
# define GPU_MEMORY_INFO_EVICTED_MEMORY_NVX            0x904B
#endif


// ////////////////////////////////////////////////////////////////////////// //
extern VCvarB r_bloom;

static VCvarB gl_crippled_gpu("gl_crippled_gpu", false, "who cares.", CVAR_Rom|CVAR_Hidden|CVAR_NoShadow);

VCvarB gl_tonemap_pal_hires("gl_tonemap_pal_hires", true, "Use 128x128x128 color cube instead of 64x64x64?", CVAR_Archive|CVAR_NoShadow);
VCvarI gl_tonemap_pal_algo("gl_tonemap_pal_algo", "1", "Tonemap color distance algorithm (0-1; 1 is the best one)", CVAR_Archive|CVAR_NoShadow);

static VCvarB gl_can_bloom("gl_can_bloom", false, "who cares.", CVAR_Rom|CVAR_Hidden|CVAR_NoShadow);
static VCvarB gl_can_hires_tonemap("gl_can_hires_tonemap", false, "who cares.", CVAR_Rom|CVAR_Hidden|CVAR_NoShadow);
static VCvarB gl_can_shadowmaps("gl_can_shadowmaps", false, "who cares.", CVAR_Rom|CVAR_Hidden|CVAR_NoShadow);
static VCvarB gl_can_shadowvols("gl_can_shadowvols", false, "who cares.", CVAR_Rom|CVAR_Hidden|CVAR_NoShadow);
VCvarB gl_can_cas_filter("gl_can_cas_filter", false, "who cares.", CVAR_Rom|CVAR_Hidden|CVAR_NoShadow);
static VCvarB gl_can_clipcontrol("gl_can_clipcontrol", false, "who cares.", CVAR_Rom|CVAR_Hidden|CVAR_NoShadow);

VCvarB gl_pic_filtering("gl_pic_filtering", false, "Filter interface pictures.", CVAR_Archive|CVAR_NoShadow);
VCvarB gl_font_filtering("gl_font_filtering", false, "Filter 2D interface.", CVAR_Archive|CVAR_NoShadow);

VCvarB gl_enable_clip_control("gl_enable_clip_control", true, "Allow using `glClipControl()`?", CVAR_Archive|CVAR_PreInit|CVAR_NoShadow);
static VCvarB gl_enable_reverse_z("gl_enable_reverse_z", true, "Allow using \"reverse z\" trick?", CVAR_Archive|CVAR_PreInit|CVAR_NoShadow);
static VCvarB gl_dbg_force_reverse_z("gl_dbg_force_reverse_z", false, "Force-enable reverse z when fp depth buffer is not available.", CVAR_PreInit|CVAR_NoShadow);
static VCvarB gl_dbg_ignore_gpu_blacklist("gl_dbg_ignore_gpu_blacklist", false, "Ignore GPU blacklist, and don't turn off features?", CVAR_PreInit|CVAR_NoShadow);
static VCvarB gl_dbg_force_gpu_blacklisting("gl_dbg_force_gpu_blacklisting", false, "Force GPU to be blacklisted.", CVAR_PreInit|CVAR_NoShadow);
static VCvarB gl_dbg_disable_depth_clamp("gl_dbg_disable_depth_clamp", false, "Disable depth clamping.", CVAR_PreInit|CVAR_NoShadow);

static VCvarB gl_disable_primitive_restart("gl_disable_primitive_restart", false, "Disable triangle fan rendering with primitive restart?", CVAR_Archive|CVAR_PreInit|CVAR_NoShadow);
static VCvarB gl_can_use_primitive_restart("gl_can_use_primitive_restart", false, "who cares.", CVAR_Rom|CVAR_Hidden|CVAR_NoShadow);

VCvarB gl_letterbox("gl_letterbox", true, "Use letterbox for scaled FS mode?", CVAR_Archive|CVAR_NoShadow);
VCvarI gl_letterbox_filter("gl_letterbox_filter", "0", "Image filtering for letterbox mode (0:nearest; 1:linear).", CVAR_Archive|CVAR_NoShadow);
VCvarS gl_letterbox_color("gl_letterbox_color", "00 00 00", "Letterbox color", CVAR_Archive|CVAR_NoShadow);
VCvarF gl_letterbox_scale("gl_letterbox_scale", "1", "Letterbox scaling factor in range (0..1].", CVAR_Archive|CVAR_NoShadow);

VCvarI VOpenGLDrawer::texture_filter("gl_texture_filter", "0", "Texture filtering mode.", CVAR_Archive|CVAR_NoShadow);
VCvarI VOpenGLDrawer::sprite_filter("gl_sprite_filter", "0", "Sprite filtering mode.", CVAR_Archive|CVAR_NoShadow);
VCvarI VOpenGLDrawer::model_filter("gl_model_filter", "0", "Model filtering mode.", CVAR_Archive|CVAR_NoShadow);
VCvarI VOpenGLDrawer::gl_texture_filter_anisotropic("gl_texture_filter_anisotropic", "0", "Texture anisotropic filtering (<=1 is off).", CVAR_Archive|CVAR_NoShadow);
VCvarB VOpenGLDrawer::clear("gl_clear", true, "Clear screen before rendering new frame?", CVAR_Archive|CVAR_NoShadow);
VCvarB VOpenGLDrawer::ext_anisotropy("gl_ext_anisotropy", true, "Use OpenGL anisotropy extension (if present)?", CVAR_Archive|CVAR_PreInit|CVAR_NoShadow);
VCvarI VOpenGLDrawer::multisampling_sample("gl_multisampling_sample", "1", "Multisampling mode.", CVAR_Archive|CVAR_NoShadow);
VCvarB VOpenGLDrawer::gl_smooth_particles("gl_smooth_particles", false, "Draw smooth particles?", CVAR_Archive|CVAR_NoShadow);

VCvarB VOpenGLDrawer::gl_dump_vendor("gl_dump_vendor", false, "Dump OpenGL vendor?", CVAR_PreInit|CVAR_NoShadow);
VCvarB VOpenGLDrawer::gl_dump_extensions("gl_dump_extensions", false, "Dump available OpenGL extensions?", CVAR_PreInit|CVAR_NoShadow);

// was 0.333
VCvarF gl_alpha_threshold("gl_alpha_threshold", "0.01", "Alpha threshold (less than this will not be drawn).", CVAR_Archive|CVAR_NoShadow);

static VCvarI gl_max_anisotropy("gl_max_anisotropy", "1", "Maximum anisotropy level (r/o).", CVAR_Rom|CVAR_NoShadow);
static VCvarB gl_is_shitty_gpu("gl_is_shitty_gpu", true, "Is shitty GPU detected (r/o)?", CVAR_Rom|CVAR_NoShadow);

VCvarB gl_enable_depth_bounds("gl_enable_depth_bounds", true, "Use depth bounds extension if found?", CVAR_Archive|CVAR_NoShadow);

VCvarB gl_sort_textures("gl_sort_textures", true, "Sort surfaces by their textures (slightly faster on huge levels; affects only lightmapped renderer)?", CVAR_Archive|CVAR_PreInit|CVAR_NoShadow);

VCvarB r_decals_wall_masked("r_decals_wall_masked", true, "Render decals on masked walls?", CVAR_Archive|CVAR_NoShadow);
VCvarB r_decals_wall_alpha("r_decals_wall_alpha", true, "Render decals on translucent walls?", CVAR_Archive|CVAR_NoShadow);

VCvarB gl_decal_debug_nostencil("gl_decal_debug_nostencil", false, "Don't touch this!", CVAR_NoShadow);
VCvarB gl_decal_debug_noalpha("gl_decal_debug_noalpha", false, "Don't touch this!", CVAR_NoShadow);
VCvarB gl_decal_dump_max("gl_decal_dump_max", false, "Don't touch this!", CVAR_NoShadow);
VCvarB gl_decal_reset_max("gl_decal_reset_max", false, "Don't touch this!", CVAR_NoShadow);

VCvarB gl_dbg_adv_render_surface_textures("gl_dbg_adv_render_surface_textures", true, "Render surface textures in advanced renderer?", CVAR_PreInit|CVAR_NoShadow);
VCvarB gl_dbg_adv_render_surface_fog("gl_dbg_adv_render_surface_fog", true, "Render surface fog in advanced renderer?", CVAR_PreInit|CVAR_NoShadow);

VCvarB gl_dbg_render_stack_portal_bounds("gl_dbg_render_stack_portal_bounds", false, "Render sector stack portal bounds.", CVAR_NoShadow);

VCvarB gl_use_stencil_quad_clear("gl_use_stencil_quad_clear", false, "Draw quad to clear stencil buffer instead of 'glClear'?", CVAR_Archive|CVAR_PreInit|CVAR_NoShadow);

// 1: normal; 2: 1-skewed
VCvarI gl_dbg_use_zpass("gl_dbg_use_zpass", "0", "DO NOT USE!", CVAR_PreInit|CVAR_NoShadow);

//VCvarB gl_dbg_advlight_debug("gl_dbg_advlight_debug", false, "Draw non-fading lights?", CVAR_PreInit|CVAR_NoShadow);
//VCvarS gl_dbg_advlight_color("gl_dbg_advlight_color", "0xff7f7f", "Color for debug lights (only dec/hex).", CVAR_PreInit|CVAR_NoShadow);

VCvarB gl_dbg_wireframe("gl_dbg_wireframe", false, "Render wireframe level?", CVAR_PreInit|CVAR_NoShadow);

#ifdef GL4ES_HACKS
# define FBO_WITH_TEXTURE_DEFAULT  true
#else
# define FBO_WITH_TEXTURE_DEFAULT  false
#endif
VCvarB gl_dbg_fbo_blit_with_texture("gl_dbg_fbo_blit_with_texture", FBO_WITH_TEXTURE_DEFAULT, "Always blit FBOs using texture mapping?", CVAR_PreInit|CVAR_NoShadow);

VCvarB r_brightmaps("r_brightmaps", true, "Allow brightmaps?", CVAR_Archive);
VCvarB r_brightmaps_sprite("r_brightmaps_sprite", true, "Allow sprite brightmaps?", CVAR_Archive);
VCvarB r_brightmaps_additive("r_brightmaps_additive", true, "Are brightmaps additive, or max?", CVAR_Archive);
VCvarB r_brightmaps_filter("r_brightmaps_filter", false, "Do bilinear filtering on brightmaps?", CVAR_Archive);

VCvarB r_glow_flat("r_glow_flat", true, "Allow glowing flats?", CVAR_Archive);

VCvarB gl_lmap_allow_partial_updates("gl_lmap_allow_partial_updates", true, "Allow partial updates of lightmap atlases (this is usually faster than full updates)?", CVAR_Archive|CVAR_NoShadow);

VCvarI gl_release_ram_textures_mode("gl_release_ram_textures_mode", "2", "When the engine should release RAM (non-GPU) texture storage (0:never; 1:after map unload; 2:immediately)?", CVAR_Archive|CVAR_NoShadow);
VCvarI gl_release_ram_textures_mode_sprite("gl_release_ram_textures_mode_sprite", "2", "When the engine should release RAM (non-GPU) texture storage (0:never; 1:after map unload; 2:immediately)?", CVAR_Archive|CVAR_NoShadow);
VCvarB gl_crop_psprites("gl_crop_psprites", true, "Crop psprite textures?", CVAR_Archive|CVAR_NoShadow);
VCvarB gl_crop_sprites("gl_crop_sprites", false, "Crop all sprite textures?", CVAR_Archive|CVAR_NoShadow);

// 0: 128
// 1: 256
// 2: 512
// 3: 1024
VCvarI gl_shadowmap_size("gl_shadowmap_size", "0", "Shadowmap size (0:128; 1:256; 2:512; 3:1024).", CVAR_PreInit|CVAR_Archive|CVAR_NoShadow);
VCvarB gl_shadowmap_precision("gl_shadowmap_precision", false, "Allow higher shadowmap precision for bigger lights?", CVAR_PreInit|CVAR_Archive|CVAR_NoShadow);
VCvarB gl_shadowmap_preclear("gl_shadowmap_preclear", true, "Clear shadowmaps after frame blit?", CVAR_PreInit|CVAR_Archive|CVAR_NoShadow);
// this seems to work slightly slower
VCvarB gl_shadowmap_more_cubes("gl_shadowmap_more_cubes", false, "Use all available shadowmap cubes, and clear them in advanve?", CVAR_PreInit|CVAR_Archive|CVAR_NoShadow);


static VCvarB gl_s3tc_present("gl_s3tc_present", false, "Use S3TC texture compression, if supported?", CVAR_Rom|CVAR_Hidden|CVAR_NoShadow);

VCvarB gl_use_srgb("gl_use_srgb", false, "Use sRGB FBO and textures? (DEBUG, DO NOT USE!)", /*CVAR_Archive|*/CVAR_PreInit|CVAR_Hidden|CVAR_NoShadow);


// ////////////////////////////////////////////////////////////////////////// //
#ifdef VV_SHADER_COMPILING_PROGRESS

#define PROG_BUF_WIDTH   (512)
#define PROG_BUF_HEIGHT  (64)

static uint8_t *tempProgBuffer = nullptr;
static GLuint shadMsgTexture = 0;
static double shadMsgStartTime;
static bool shadMsgActive = false;

#ifdef VV_SHADOWCUBE_DEPTH_TEXTURE
bool VOpenGLDrawer::ShadowCubeMapData::useDefaultDepth = false;
#endif


//==========================================================================
//
//  progBufClear
//
//==========================================================================
static void progBufClear () {
  if (!tempProgBuffer) tempProgBuffer = (uint8_t *)Z_Malloc(PROG_BUF_WIDTH*PROG_BUF_HEIGHT*4);
  //memset(tempProgBuffer, 0, PROG_BUF_WIDTH*PROG_BUF_HEIGHT*4);
  uint32_t *da = (uint32_t *)tempProgBuffer;
  for (unsigned f = 0; f < PROG_BUF_WIDTH*PROG_BUF_HEIGHT; ++f, ++da) *da = 0xff000000;
}


//==========================================================================
//
//  progBufPutCharAt
//
//==========================================================================
static void progBufPutCharAt (int x0, int y0, char ch) {
  if (!tempProgBuffer) return;
  /*
  const unsigned colors[3] = { 0xff7f00u, 0xdf5f00u, 0xbf3f00u };
  const unsigned conColor = colors[lnum%3];
  int r = (conColor>>16)&0xff;
  int g = (conColor>>8)&0xff;
  int b = conColor&0xff;
  const int rr = r, gg = g, bb = b;
  */
  const unsigned color = 0xff007fff;
  for (int y = CONFONT_HEIGHT-1; y >= 0; --y) {
    if (y0+y < 0 || y0+y >= PROG_BUF_HEIGHT) break;
    vuint16 v = glConFont10[(ch&0xff)*10+y];
    uint32_t *da = (uint32_t *)(tempProgBuffer+(PROG_BUF_WIDTH*(y0+y))*4+x0*4);
    for (int x = 0; x < CONFONT_WIDTH; ++x, ++da) {
      if (x0+x < 0) continue;
      if (x0+x >= PROG_BUF_WIDTH) break;
      if (v&0x8000) {
        *da = color;
      } else {
        *da = 0xff000000;
      }
      v <<= 1;
    }
  }
}


//==========================================================================
//
//  progBufPutTextAt
//
//==========================================================================
static int progBufPutTextAt (int x0, int y0, const char *s) {
  if (!s || !s[0] || !tempProgBuffer) return 0;
  int wdt = 0;
  while (*s) {
    progBufPutCharAt(x0, y0, *s++);
    x0 += CONFONT_WIDTH;
    wdt += CONFONT_WIDTH;
  }
  return wdt;
}
#endif


//==========================================================================
//
//  VOpenGLDrawer::InitShaderProgress
//
//==========================================================================
void VOpenGLDrawer::InitShaderProgress () {
#ifdef VV_SHADER_COMPILING_PROGRESS
  vassert(shadMsgTexture == 0);

  shadMsgStartTime = Sys_Time();
  shadMsgActive = false;

  p_glBindFramebuffer(GL_FRAMEBUFFER, 0);

  GLSetViewport(0, 0, ScreenWidth, ScreenHeight);
  SetOrthoProjection(0, ScreenWidth, ScreenHeight, 0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glDisable(GL_ALPHA_TEST);
  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  //if (HaveDepthClamp) glDisable(GL_DEPTH_CLAMP);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_BLEND);
  GLDRW_CHECK_ERROR("progress preparation");

  glEnable(GL_TEXTURE_2D);
  glGenTextures(1, &shadMsgTexture);
  glBindTexture(GL_TEXTURE_2D, shadMsgTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
  GLDRW_CHECK_ERROR("create progress texture");

  //glClearColor(1.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  progBufClear();

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,  PROG_BUF_WIDTH, PROG_BUF_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, tempProgBuffer);
  GLDRW_CHECK_ERROR("upload texture image");
#endif
}


//==========================================================================
//
//  VOpenGLDrawer::DoneShaderProgress
//
//==========================================================================
void VOpenGLDrawer::DoneShaderProgress () {
#ifdef VV_SHADER_COMPILING_PROGRESS
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glBindTexture(GL_TEXTURE_2D, 0);
  glDeleteTextures(1, &shadMsgTexture);
  shadMsgTexture = 0;
  ReactivateCurrentFBO();
  /*
  GCon->Logf(NAME_Debug, "waiting... (%dx%d, %u)", ScreenWidth, ScreenHeight, shadMsgTexture);
  double stt = Sys_Time();
  while (Sys_Time()-stt < 2.0) Sys_YieldMicro(100);
  */
#endif
}


//==========================================================================
//
//  VOpenGLDrawer::ShowShaderProgress
//
//==========================================================================
void VOpenGLDrawer::ShowShaderProgress (int curr, int total) {
#ifdef VV_SHADER_COMPILING_PROGRESS
  const double ctt = Sys_Time();
  if (!shadMsgActive) {
    if (ctt-shadMsgStartTime < 0.666) return;
    shadMsgActive = true;
  } else {
    // update once per 100 msec
    if (ctt-shadMsgStartTime < 100.0/100.0) return;
  }

  if (total < 1) total = 1;
  if (curr < 0) curr = 0;
  if (curr > total) curr = total;

  int wdt;
  if (curr < total) {
    wdt = progBufPutTextAt(0, 0, va("compiling shaders [%d/%d]", curr, total));
  } else {
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    progBufClear();
    wdt = progBufPutTextAt(0, 0, "done compiling shaders");
  }
  glTexSubImage2D(GL_TEXTURE_2D, 0,  0, 0, PROG_BUF_WIDTH, PROG_BUF_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, tempProgBuffer);

  glEnable(GL_TEXTURE_2D);
  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

  const int texOfs = (ScreenWidth-wdt)/2;
  glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2i(texOfs, 0);
    glTexCoord2f(1.0f, 0.0f); glVertex2i(texOfs+PROG_BUF_WIDTH, 0);
    glTexCoord2f(1.0f, 1.0f); glVertex2i(texOfs+PROG_BUF_WIDTH, PROG_BUF_HEIGHT);
    glTexCoord2f(0.0f, 1.0f); glVertex2i(texOfs, PROG_BUF_HEIGHT);
  glEnd();

  const int pbarHeight = 14;
  const int pbarWidth = ScreenWidth-12;
  const int pbarXOfs = (ScreenWidth-pbarWidth)/2;
  const int pbarYOfs = CONFONT_HEIGHT+4;

  glColor4f(1.0f, 0.5f, 0.0f, 1.0f);
  glDisable(GL_TEXTURE_2D);

  glBegin(GL_QUADS);
    glVertex2i(pbarXOfs-2, pbarYOfs);
    glVertex2i(pbarXOfs+pbarWidth+2, pbarYOfs);
    glVertex2i(pbarXOfs+pbarWidth+2, pbarYOfs+pbarHeight);
    glVertex2i(pbarXOfs-2, pbarYOfs+pbarHeight);
  glEnd();

  glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
  glBegin(GL_QUADS);
    glVertex2i(pbarXOfs-1, pbarYOfs+1);
    glVertex2i(pbarXOfs+pbarWidth+1, pbarYOfs+1);
    glVertex2i(pbarXOfs+pbarWidth+1, pbarYOfs+pbarHeight-1);
    glVertex2i(pbarXOfs-1, pbarYOfs+pbarHeight-1);
  glEnd();

  const int bwdt = pbarWidth*curr/total;
  glColor4f(0.9f, 0.4f, 0.0f, 1.0f);
  glBegin(GL_QUADS);
    glVertex2i(pbarXOfs, pbarYOfs+2);
    glVertex2i(pbarXOfs+bwdt, pbarYOfs+2);
    glVertex2i(pbarXOfs+bwdt, pbarYOfs+pbarHeight-2);
    glVertex2i(pbarXOfs, pbarYOfs+pbarHeight-2);
  glEnd();

  Update(false);

  shadMsgStartTime = ctt;
#endif
}


//==========================================================================
//
//  getShadowmapPOT
//
//==========================================================================
static unsigned int getShadowmapPOT () noexcept {
  int ss = gl_shadowmap_size.asInt()+1;
  if (ss < 0) ss = 0; else if (ss > 4) ss = 4;
  return (unsigned int)ss;
}


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
static VVA_OKUNUSED bool CheckVendorString (VStr vs, const char *fuckedName) {
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
  , gpuMemEvictCountPrev(0)
  , gpuMemEvictMemPrev(0)
  , surfList()
  , mainFBO()
  , mainFBOFP()
  , ambLightFBO()
  , wipeFBO()
  , bloomscratchFBO()
  , bloomscratch2FBO()
  , bloomeffectFBO()
  , bloomcoloraveragingFBO()
  , cameraFBOList()
  , currMainFBO(-1)
  , postSrcFBO()
  , postOverFBO()
  , postSrcFSFBO()
  , tempMainFBO()
  , tonemapPalLUT(0)
  , tonemapLastGamma(-1)
  , tonemapMode(0)
  , tonemapColorAlgo(0)
{
  glVerMajor = glVerMinor = 0;

  currentActiveShader = nullptr;
  lastgamma = 0;
  //CurrentFade = 0;

  useReverseZ = false;
  hasNPOT = false;
  hasBoundsTest = false;
  canIntoBloomFX = false;

  tmpImgBuf0 = nullptr;
  tmpImgBuf1 = nullptr;
  tmpImgBufSize = 0;

  readBackTempBuf = nullptr;
  readBackTempBufSize = 0;

  decalUsedStencil = false;
  decalStcVal = 255; // next value for stencil buffer (clear on the first use, and clear on each wrap)
  stencilBufferDirty = true; // just in case
  isShittyGPU = false; // not yet
  shittyGPUCheckDone = false; // just in case
  atlasesGenerated = false;
  currentActiveFBO = nullptr;

  blendEnabled = false;

  offsetEnabled = false;
  offsetFactor = offsetUnits = 0;

  //cameraFBO[0].mOwner = nullptr;
  //cameraFBO[1].mOwner = nullptr;

  scissorEnabled = false;
  scissorX = scissorY = 0;
  scissorW = scissorH = 0;

  depthMaskSP = 0;
  currDepthMaskState = 0;

  shadowmapPOT = getShadowmapPOT();
  shadowmapSize = 64<<shadowmapPOT;

  //memset((void *)shadowCube, 0, sizeof(shadowCube));
  for (unsigned int f = 0; f < MaxShadowCubes; ++f) shadowCube[f].setAllDirty();
  smapCurrent = 0;

  memset(currentViewport, 0, sizeof(currentViewport));
  currentViewport[0] = -666;
}


//==========================================================================
//
//  VOpenGLDrawer::~VOpenGLDrawer
//
//==========================================================================
VOpenGLDrawer::~VOpenGLDrawer () {
  if (mInitialized) DeinitResolution();
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
//  VOpenGLDrawer::DestroyShadowCube
//
//==========================================================================
void VOpenGLDrawer::DestroyShadowCube () {
  UnloadShadowMapShaders();
  ShadowCubeMapData::useDefaultDepth = false;
  for (unsigned cubeidx = 0; cubeidx < MaxShadowCubes; ++cubeidx) {
    shadowCube[cubeidx].destroyCube();
  }
  smapCurrent = 0;
}


//==========================================================================
//
//  VOpenGLDrawer::CreateShadowCube
//
//==========================================================================
void VOpenGLDrawer::CreateShadowCube () {
  DeactivateShader();
  DestroyShadowCube();
  if (!canRenderShadowmaps) return;

  shadowmapPOT = getShadowmapPOT();
  shadowmapSize = 64<<shadowmapPOT;

  for (unsigned cubeidx = 0; cubeidx < MaxShadowCubes; ++cubeidx) {
    ShadowCubeMap &cube = *&shadowCube[cubeidx];
    cube.destroyCube();
    cube.createCube(this, shadowmapPOT, (cubeidx >= MaxShadowCubes/2 && gl_shadowmap_precision.asBool())/*float32*/);
    if (!cube.isValid()) Sys_Error("OpenGL: cannot create shadow cube %u", cubeidx);
    GCon->Logf(NAME_Init, "OpenGL: created cubemap %u, fbo %u; shadowmap size: %ux%u", cube.cubeTexId, cube.cubeFBO, shadowmapSize, shadowmapSize);
  }

  if (CheckExtension("GL_ARB_seamless_cube_map")) {
    GCon->Log(NAME_Init, "OpenGL: enabling seamless cubemaps.");
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
  }

  ReactivateCurrentFBO();
}


//==========================================================================
//
//  VOpenGLDrawer::EnsureShadowMapCube
//
//==========================================================================
void VOpenGLDrawer::EnsureShadowMapCube () {
  if (!shadowCube[0].isValid()) CreateShadowCube();
}


static double *ungammaNormal = nullptr;

// 128x128x128
static uint8_t *pallutAlgo0 = nullptr;
static uint8_t *pallutAlgo1 = nullptr;


//==========================================================================
//
//  PrepareUngamma
//
//==========================================================================
static void PrepareUngamma () noexcept {
  if (ungammaNormal) return;
  GCon->Logf(NAME_Init, "OpenGL: creating palette tonemap LUT tables...");
  // calculate "ungamma" table
  ungammaNormal = (double *)Z_Calloc(256*4*sizeof(double));
  for (unsigned i = 0; i < 256; ++i) {
    // normal
    ungammaNormal[i*4+0] = r_palette[i].r;
    ungammaNormal[i*4+1] = r_palette[i].g;
    ungammaNormal[i*4+2] = r_palette[i].b;
  }
}


//==========================================================================
//
//  TonemapPalFindColor
//
//  0: simple distance
//  1: rgbDistanceSquared
//
//==========================================================================
static int TonemapPalFindColor (const int algo, const vuint8 r, const vuint8 g, const vuint8 b) noexcept {
  if (algo) {
    if (pallutAlgo1) return pallutAlgo1[((r>>1)*128+(g>>1))*128+(b>>1)];
  } else {
    if (pallutAlgo0) return pallutAlgo0[((r>>1)*128+(g>>1))*128+(b>>1)];
  }

  PrepareUngamma();
  GCon->Logf(NAME_Init, "OpenGL: creating palette tonemap LUT translation (algo #%d)...", algo);

  const double *ungamma = ungammaNormal;

  // no lut, calculate it
  vuint8 *lut = (vuint8 *)Z_Malloc(128*128*128);

  for (int rr = 0; rr < 128; ++rr) {
    for (int gg = 0; gg < 128; ++gg) {
      for (int bb = 0; bb < 128; ++bb) {
        int best_color = -1;
        int best_dist = 0x7fffffff;
        double best_dist_dbl = DBL_MAX;
        const double ur = rr*255.0/127.0;
        const double ug = gg*255.0/127.0;
        const double ub = bb*255.0/127.0;
        for (unsigned i = 1; i < 256; ++i) {
          if (algo) {
            const vint32 dist = rgbDistanceSquared(r_palette[i].r, r_palette[i].g, r_palette[i].b, rr<<1, gg<<1, bb<<1);
            if (best_color < 0 || dist < best_dist) {
              best_color = (int)i;
              best_dist = dist;
              if (!dist) break;
            }
          } else {
            const double dr = ungamma[i*4+0]-ur;
            const double dg = ungamma[i*4+1]-ug;
            const double db = ungamma[i*4+2]-ub;
            const double dist = dr*dr+dg*dg+db*db;
            if (best_color < 0 || dist < best_dist_dbl) {
              best_color = (int)i;
              best_dist_dbl = dist;
              if (dist == 0.0f) break;
            }
          }
        }
        vassert(best_color >= 0);
        lut[(rr*128+gg)*128+bb] = (vuint8)best_color;
      }
    }
  }

  if (algo) pallutAlgo1 = lut; else pallutAlgo0 = lut;

  return TonemapPalFindColor(algo, r, g, b);
}


//==========================================================================
//
//  VOpenGLDrawer::GeneratePaletteLUT
//
//==========================================================================
void VOpenGLDrawer::GeneratePaletteLUT () {
  if (tonemapPalLUT) {
    glBindTexture(GL_TEXTURE_2D, 0); // just in case
    glDeleteTextures(1, &tonemapPalLUT);
    tonemapPalLUT = 0;
    GLDRW_CHECK_ERROR("destroy tonemapPalLUT");
  }

  if (!gl_can_hires_tonemap) tonemapMode = 0; // oops

  tonemapColorAlgo = gl_tonemap_pal_algo.asInt();
  // 0: simple distance
  // 1: rgbDistanceSquared
  const int algo = tonemapColorAlgo;

  rgb_t *tdata;
  if (tonemapMode == 0) {
    GCon->Logf(NAME_Init, "OpenGL: creating palette tonemap LUT...");
    tdata = (rgb_t *)Z_Calloc(512*512*sizeof(rgb_t));
    rgb_t *c = tdata;
    //const vuint8 *gt = getGammaTable(usegamma); //gammatable[usegamma];
    tonemapLastGamma = usegamma;
    for (int r = 0; r < 64; ++r) {
      for (int g = 0; g < 64; ++g) {
        for (int b = 0; b < 64; ++b, ++c) {
          //*c = R_LookupRGB((r<<2)|(r>>4), (g<<2)|(g>>4), (b<<2)|(b>>4));
          const vuint8 r8 = clampToByte(r*255/63);
          const vuint8 g8 = clampToByte(g*255/63);
          const vuint8 b8 = clampToByte(b*255/63);
          const int cidx = TonemapPalFindColor(algo, r8, g8, b8); //R_LookupRGB(r8, g8, b8)
          c->r = r_palette[cidx].r;
          c->g = r_palette[cidx].g;
          c->b = r_palette[cidx].b;
          //c->a = 255;

          //c->r = gt[c->r];
          //c->g = gt[c->g];
          //c->b = gt[c->b];
        }
      }
    }
  } else {
    GCon->Logf(NAME_Init, "OpenGL: creating hires palette tonemap LUT...");
    tdata = (rgb_t *)Z_Calloc(2048*2048*sizeof(rgb_t));
    //const vuint8 *gt = getGammaTable(usegamma); //gammatable[usegamma];
    tonemapLastGamma = usegamma;
    for (int r = 0; r < 128; ++r) {
      for (int g = 0; g < 128; ++g) {
        for (int b = 0; b < 128; ++b) {
          rgb_t *c = &tdata[(r*128+g)*128+b];
          //*c = R_LookupRGB((r<<2)|(r>>4), (g<<2)|(g>>4), (b<<2)|(b>>4));
          const vuint8 r8 = clampToByte(r*255/127);
          const vuint8 g8 = clampToByte(g*255/127);
          const vuint8 b8 = clampToByte(b*255/127);
          const int cidx = TonemapPalFindColor(algo, r8, g8, b8); //R_LookupRGB(r8, g8, b8)
          //*c = r_palette[cidx];
          c->r = r_palette[cidx].r;
          c->g = r_palette[cidx].g;
          c->b = r_palette[cidx].b;
          //c->a = 255;

          //c->r = gt[c->r];
          //c->g = gt[c->g];
          //c->b = gt[c->b];
        }
      }
    }
  }

  glGenTextures(1, &tonemapPalLUT);
  GLDRW_CHECK_ERROR("create tonemapPalLUT");
  glBindTexture(GL_TEXTURE_2D, tonemapPalLUT);
  p_glObjectLabelVA(GL_TEXTURE, tonemapPalLUT, "Palette Tonemap LUT Texture");
  GLDRW_CHECK_ERROR("bind tonemapPalLUT");
  glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, /*GL_CLAMP_TO_EDGE*/ClampToEdge);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, /*GL_CLAMP_TO_EDGE*/ClampToEdge);
  GLDRW_CHECK_ERROR("set tonemapPalLUT options");
  //glTexSubImage2D(GL_TEXTURE_2D, 0,  0, 0, 512, 512, GL_RGBA, GL_UNSIGNED_BYTE, (const void *)tdata);
  if (tonemapMode == 0) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8/*A*/, 512, 512, 0, GL_RGB/*A*/, GL_UNSIGNED_BYTE, (const void *)tdata);
  } else {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8/*A*/, 2048, 2048, 0, GL_RGB/*A*/, GL_UNSIGNED_BYTE, (const void *)tdata);
  }
  GLDRW_CHECK_ERROR("upload tonemapPalLUT");
  glBindTexture(GL_TEXTURE_2D, 0);

  Z_Free(tdata);
}


//==========================================================================
//
//  VOpenGLDrawer::GLDisableDepthWriteSlow
//
//==========================================================================
void VOpenGLDrawer::GLDisableDepthWriteSlow () noexcept {
  GLDisableDepthWrite();
}


//==========================================================================
//
//  VOpenGLDrawer::PushDepthMaskSlow
//
//==========================================================================
void VOpenGLDrawer::PushDepthMaskSlow () noexcept {
  PushDepthMask();
}


//==========================================================================
//
//  VOpenGLDrawer::PopDepthMaskSlow
//
//==========================================================================
void VOpenGLDrawer::PopDepthMaskSlow () noexcept {
  PopDepthMask();
}


//==========================================================================
//
//  VOpenGLDrawer::GLDisableDepthTestSlow
//
//==========================================================================
void VOpenGLDrawer::GLDisableDepthTestSlow () noexcept {
  glDisable(GL_DEPTH_TEST);
}


//==========================================================================
//
//  VOpenGLDrawer::GLEnableDepthTestSlow
//
//==========================================================================
void VOpenGLDrawer::GLEnableDepthTestSlow () noexcept {
  glEnable(GL_DEPTH_TEST);
}


//==========================================================================
//
//  VOpenGLDrawer::InitGPUMemoryUse
//
//==========================================================================
void VOpenGLDrawer::InitGPUMemoryUse () {
  GLint vals[4]; // i may want to add AMD support later
  memset(vals, 0, sizeof(vals));

  gpuMemEvictMemPrev = 0;
  gpuMemEvictCountPrev = 0;

  GLDRW_RESET_ERROR();
  glGetIntegerv(GPU_MEMORY_INFO_EVICTION_COUNT_NVX, vals);
  if (glGetError() != 0) return; // no such enum, nothing to do anymore
  gpuMemEvictCountPrev = vals[0];

  glGetIntegerv(GPU_MEMORY_INFO_EVICTED_MEMORY_NVX, vals);
  if (glGetError() != 0) return; // no such enum, nothing to do anymore
  gpuMemEvictMemPrev = vals[0];
}


//==========================================================================
//
//  VOpenGLDrawer::ReportGPUMemoryUse
//
//  may do nothing
//  called in level precaching, and in level uncaching
//  `infomsg` is used to identify various call sites
//  it may be shown before reports
//
//==========================================================================
void VOpenGLDrawer::ReportGPUMemoryUse (const char *infomsg) {
  GLint vals[4]; // i may want to add AMD support later
  int memAvail, memEvict, countEvict;

  memset(vals, 0, sizeof(vals));

  GLDRW_RESET_ERROR();
  glGetIntegerv(GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, vals);
  if (glGetError() != 0) return; // no such enum, nothing to do anymore
  memAvail = vals[0];

  glGetIntegerv(GPU_MEMORY_INFO_EVICTION_COUNT_NVX, vals);
  if (glGetError() != 0) return; // no such enum, nothing to do anymore
  countEvict = vals[0];

  glGetIntegerv(GPU_MEMORY_INFO_EVICTED_MEMORY_NVX, vals);
  if (glGetError() != 0) return; // no such enum, nothing to do anymore
  memEvict = vals[0];

  if (memEvict < gpuMemEvictMemPrev) gpuMemEvictMemPrev = memEvict;
  if (countEvict < gpuMemEvictCountPrev) gpuMemEvictCountPrev = countEvict;

  if (memAvail != 0) {
    if (infomsg && infomsg[0]) GCon->Logf("OpenGL: ===== %s =====", infomsg);
    GCon->Logf("OpenGL: %sGPU VRAM available: %s MB", (infomsg && infomsg[0] ? "  " : ""), comatoze((vuint32)memAvail/1024U));
    if (countEvict > gpuMemEvictCountPrev) {
      GCon->Logf("OpenGL:   eviction count since last report: %s", comatoze((vuint32)(countEvict-gpuMemEvictCountPrev)));
    }
    if (memEvict > gpuMemEvictMemPrev) {
      GCon->Logf("OpenGL:   evicted memory since last report: %s MB", comatoze((vuint32)(memEvict-gpuMemEvictMemPrev)/1024U));
    }
  }

  gpuMemEvictMemPrev = memEvict;
  gpuMemEvictCountPrev = countEvict;
}


//==========================================================================
//
//  VOpenGLDrawer::DeinitResolution
//
//==========================================================================
void VOpenGLDrawer::DeinitResolution () {
  DestroyShadowCube();
  // delete VBOs
  vboSprite.destroy();
  vboSky.destroy();
  vboMaskedSurf.destroy();
  vboAdvSurf.destroy();
  vboSMapSurf.destroy();
  vboSMapSurfTex.destroy();
  // FBOs
  if (currentActiveFBO != nullptr) {
    currentActiveFBO = nullptr;
    ReactivateCurrentFBO();
  }
  // unload shaders
  DestroyShaders();
  // delete all created shader objects
  /*
  for (int i = CreatedShaderObjects.length()-1; i >= 0; --i) {
    p_glDeleteObjectARB(CreatedShaderObjects[i]);
  }
  CreatedShaderObjects.Clear();
  */
  // destroy FBOs
  DestroyCameraFBOList();
  mainFBO.destroy();
  mainFBOFP.destroy();
  ambLightFBO.destroy();
  wipeFBO.destroy();
  BloomDeinit();
  DeleteLightmapAtlases();
  depthMaskSP = 0;

  if (tonemapPalLUT) {
    glBindTexture(GL_TEXTURE_2D, 0); // just in case
    glDeleteTextures(1, &tonemapPalLUT);
    tonemapPalLUT = 0;
  }

  postSrcFBO.destroy();
  postOverFBO.destroy();
  postSrcFSFBO.destroy();
  tempMainFBO.destroy();

  memset(currentViewport, 0, sizeof(currentViewport));
  currentViewport[0] = -666;
}


//==========================================================================
//
//  VOpenGLDrawer::InitResolution
//
//==========================================================================
void VOpenGLDrawer::InitResolution () {
  if (currentActiveFBO != nullptr) {
    currentActiveFBO = nullptr;
    ReactivateCurrentFBO();
  }

  ShadowCubeMapData::useDefaultDepth = false;

  const int calcWidth = ScreenWidth;
  const int calcHeight = ScreenHeight;

  GCon->Logf(NAME_Init, "Setting up new resolution: %dx%d (%dx%d)", RealScreenWidth, RealScreenHeight, calcWidth, calcHeight);

  bool isCrippledGPU = false;
  bool setCrippledGPU = false; // just report it, but don't turn off the features

  if (gl_dump_vendor) {
    GCon->Logf(NAME_Init, "GL_VENDOR: %s", glGetString(GL_VENDOR));
    GCon->Logf(NAME_Init, "GL_RENDERER: %s", glGetString(GL_RENDERER));
    GCon->Logf(NAME_Init, "GL_VERSION: %s", glGetString(GL_VERSION));
  } else {
    VStr ven((char *)glGetString(GL_VENDOR));
    VStr ren((char *)glGetString(GL_RENDERER));
    VStr ver((char *)glGetString(GL_VERSION));
    GCon->Logf(NAME_Init, "OpenGL: %s / %s / %s", *ven, *ren, *ver);
    if (ren.globMatchCI("*rtx*3060*") ||
        (ren.globMatchCI("*rtx*3*0*0*") && ven.globMatchCI("NVIDIA") &&
         (ven.globMatchCI("LHR") || ver.globMatchCI("LHR"))))
    {
      // oh... ok, if somebody wants to eat shit that shitvidia fed them... it's possible to recompile the code anyway
      #if 0
      isCrippledGPU = true;
      GCon->Log(NAME_Error, "OpenGL: this videocard is severely crippled. turning off advanced effects.");
      #else
      setCrippledGPU = true;
      GCon->Log(NAME_Error, "OpenGL: This videocard is severely crippled. It may or may not work properly. Continue on your own risk.");
      GCon->Log(NAME_Error, "OpenGL: Go on on your own risk, i cannot give any guarantees.");
      #endif
    }
  }

  if (gl_dump_extensions) {
    GCon->Log(NAME_Init, "GL_EXTENSIONS:");
    TArray<VStr> Exts;
    VStr((char *)glGetString(GL_EXTENSIONS)).Split(' ', Exts);
    for (int i = 0; i < Exts.length(); ++i) GCon->Log(NAME_Init, VStr("- ")+Exts[i]);
  }

  memset(currentViewport, 0, sizeof(currentViewport));
  currentViewport[0] = -666;

#ifdef GL4ES_NO_CONSTRUCTOR
  // fake version for GL4ES
  GLint major = 2, minor = 1;
#else
  GLint major = 30000, minor = 30000;
  glGetIntegerv(GL_MAJOR_VERSION, &major);
  glGetIntegerv(GL_MINOR_VERSION, &minor);
  if (major > 8 || minor > 16 || major < 1 || minor < 0) {
    GCon->Log(NAME_Error, "OpenGL: your GPU drivers are absolutely broken.");
    if (major == 30000 && minor == 30000) {
      GCon->Logf(NAME_Error, "OpenGL: reported OpenGL version is NOTHING, which is a nonsense.");
    } else {
      GCon->Logf(NAME_Error, "OpenGL: reported OpenGL version is v%d.%d, which is a nonsense.", major, minor);
    }
    GCon->Log(NAME_Error, "OpenGL: expect crashes and visual glitches (if the engine will run at all).");
    major = minor = 2;
    if (!isShittyGPU) {
      isShittyGPU = true;
      if (gl_dbg_ignore_gpu_blacklist) {
        GCon->Log(NAME_Init, "User command is to ignore blacklist; I shall obey!");
        isShittyGPU = false;
      }
    }
    shittyGPUCheckDone = true;
  } else {
    GCon->Logf(NAME_Init, "OpenGL v%d.%d found", major, minor);
  }
#endif
  if (isCrippledGPU) {
    major = minor = 2;
    shittyGPUCheckDone = true;
    isShittyGPU = true;
  }
  gl_crippled_gpu = (isCrippledGPU || setCrippledGPU);

  glVerMajor = major;
  glVerMinor = minor;
  shadowmapPOT = getShadowmapPOT();
  shadowmapSize = 64<<shadowmapPOT;

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
  gl_can_hires_tonemap = (maxTexSize >= 2048);
  if (!gl_can_hires_tonemap) gl_tonemap_pal_hires = false; // turn it off

  hasNPOT = CheckExtension("GL_ARB_texture_non_power_of_two") || CheckExtension("GL_OES_texture_npot");
  hasBoundsTest = CheckExtension("GL_EXT_depth_bounds_test");

  useReverseZ = false;
  DepthZeroOne = false;
  canRenderShadowmaps = false;
  p_glClipControl = nullptr;
  gl_can_clipcontrol = false;

  if (!isCrippledGPU) {
    if ((major > 4 || (major == 4 && minor >= 5)) || CheckExtension("GL_ARB_clip_control")) {
      p_glClipControl = glClipControl_t(GetExtFuncPtr("glClipControl"));
    }
    gl_can_clipcontrol = !!p_glClipControl;
    if (p_glClipControl) {
      if (gl_enable_clip_control) {
        GCon->Log(NAME_Init, "OpenGL: `glClipControl()` found");
      } else {
        p_glClipControl = nullptr;
        GCon->Log(NAME_Init, "OpenGL: `glClipControl()` found, but disabled by user; i shall obey");
      }
    }
  }


  /*
  if (!CheckExtension("GL_ARB_multitexture")) {
    Sys_Error("OpenGL FATAL: Multitexture extensions not found.");
  } else {
    GLint tmp = 0;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &tmp);
    GCon->Logf(NAME_Init, "Max texture samplers: %d", tmp);
  }
  */

  // check for shader extensions
  if (CheckExtension("GL_ARB_shader_objects") && CheckExtension("GL_ARB_shading_language_100") &&
      CheckExtension("GL_ARB_vertex_shader") && CheckExtension("GL_ARB_fragment_shader"))
  {
    GLint tmp;
    GCon->Logf(NAME_Init, "Shading language version: %s", glGetString(GL_SHADING_LANGUAGE_VERSION_ARB));
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS_ARB, &tmp);
    GCon->Logf(NAME_Init, "Max texture image units: %d", tmp);
    if (tmp < 4) Sys_Error("OpenGL: your GPU must support at least 4 texture samplers, but it has only %d", tmp);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS_ARB, &tmp);
    GCon->Logf(NAME_Init, "Max vertex attribs: %d", tmp);
    #ifndef GL4ES_HACKS
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB, &tmp);
    GCon->Logf(NAME_Init, "Max vertex uniform components: %d", tmp);
    glGetIntegerv(GL_MAX_VARYING_FLOATS_ARB, &tmp);
    GCon->Logf(NAME_Init, "Max varying floats: %d", tmp);
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS_ARB, &tmp);
    GCon->Logf(NAME_Init, "Max fragment uniform components: %d", tmp);
    #endif
  } else {
    Sys_Error("OpenGL FATAL: no shader support");
  }

  if (!CheckExtension("GL_ARB_vertex_buffer_object")) Sys_Error("OpenGL FATAL: VBO not found.");
  if (!CheckExtension("GL_EXT_draw_range_elements")) Sys_Error("OpenGL FATAL: GL_EXT_draw_range_elements not found");

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
  float max_aniso = 1.0f;
  if (!isCrippledGPU && ext_anisotropy && CheckExtension("GL_EXT_texture_filter_anisotropic")) {
    glGetFloatv(GLenum(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT), &max_aniso);
    if (max_aniso < 1.0f) max_aniso = 1.0f;
    GCon->Logf(NAME_Init, "Max anisotropy: %g", (double)max_aniso);
  }
  max_anisotropy = clampval((int)max_aniso, 1, 255);
  gl_max_anisotropy = max_anisotropy;
  anisotropyExists = (max_anisotropy > 1);
  if (gl_texture_filter_anisotropic.asInt() == 0) {
    int defani = clampval(max_anisotropy/2, 1, max_anisotropy);
    if (defani > 8) defani = 8;
    if (max_anisotropy > 1 && max_anisotropy < 4) defani = 2;
    gl_texture_filter_anisotropic.Set(defani);
    if (max_anisotropy > 1) GCon->Logf(NAME_Init, "Set default anisotropy filtering level to %d", defani);
  }

  // clamp to edge extension
  if (CheckExtension("GL_SGIS_texture_edge_clamp") || CheckExtension("GL_EXT_texture_edge_clamp")) {
    GCon->Log(NAME_Init, "Clamp to edge extension found.");
    ClampToEdge = GL_CLAMP_TO_EDGE_SGIS;
  } else {
    ClampToEdge = GL_CLAMP;
  }

  glClipControl_t savedClipControl = p_glClipControl;
#ifdef GL4ES_NO_CONSTRUCTOR
  #define VV_GLIMPORTS
  #define VV_GLIMPORTS_PROC
  #define VGLAPIPTR(x,required)  do { \
    p_##x = &x; \
  } while(0)
  #include "gl_imports.h"
  #undef VGLAPIPTR
  #undef VV_GLIMPORTS_PROC
  #undef VV_GLIMPORTS
#else
  #define VV_GLIMPORTS
  #define VGLAPIPTR(x,required)  do { \
    p_##x = x##_t(GetExtFuncPtr(#x)); \
    if (!p_##x /*|| strstr(#x, "Framebuffer")*/) { \
      p_##x = nullptr; \
      VStr extfn(#x); \
      extfn += "EXT"; \
      /*extfn += "ARB";*/ \
      GCon->Logf(NAME_Init, "OpenGL: trying `%s` instead of `%s`", *extfn, #x); \
      p_##x = x##_t(GetExtFuncPtr(*extfn)); \
      if (p_##x) GCon->Logf(NAME_Init, "OpenGL: ...found `%s`.", *extfn); \
    } \
    if (required && !p_##x) Sys_Error("OpenGL: `%s()` not found!", ""#x); \
  } while (0)
  #include "gl_imports.h"
  #undef VGLAPIPTR
  #undef VV_GLIMPORTS
#endif
  p_glClipControl = savedClipControl;

  if (p_glBlitFramebuffer) GCon->Log(NAME_Init, "OpenGL: `glBlitFramebuffer()` found");

  if (!isShittyGPU && !isCrippledGPU && p_glClipControl) {
    // normal GPUs
    useReverseZ = true;
    if (!gl_enable_reverse_z) {
      GCon->Log(NAME_Init, "OpenGL: oops, user disabled reverse z, i shall obey");
      useReverseZ = false;
    }
  } else {
    GCon->Log(NAME_Init, "OpenGL: reverse z is turned off for your GPU");
    useReverseZ = false;
  }

  if (isCrippledGPU) { p_glDepthBounds = nullptr; hasBoundsTest = false; }

  if (hasBoundsTest && !p_glDepthBounds) {
    hasBoundsTest = false;
    GCon->Log(NAME_Init, "OpenGL: GL_EXT_depth_bounds_test found, but no `glDepthBounds()` exported");
  }

  if (isCrippledGPU) p_glStencilOpSeparate = nullptr;
  if (/*p_glStencilFuncSeparate &&*/ p_glStencilOpSeparate) {
    GCon->Log(NAME_Init, "Found OpenGL 2.0 separate stencil methods");
  } else if (CheckExtension("GL_ATI_separate_stencil")) {
    //p_glStencilFuncSeparate = glStencilFuncSeparate_t(GetExtFuncPtr("glStencilFuncSeparateATI"));
    p_glStencilOpSeparate = glStencilOpSeparate_t(GetExtFuncPtr("glStencilOpSeparateATI"));
    if (/*p_glStencilFuncSeparate &&*/ p_glStencilOpSeparate) {
      GCon->Log(NAME_Init, "Found GL_ATI_separate_stencil");
    } else {
      GCon->Log(NAME_Init, "No separate stencil methods found");
      //p_glStencilFuncSeparate = nullptr;
      p_glStencilOpSeparate = nullptr;
    }
  } else {
    GCon->Log(NAME_Init, "No separate stencil methods found");
    //p_glStencilFuncSeparate = nullptr;
    p_glStencilOpSeparate = nullptr;
  }

  if (!gl_dbg_disable_depth_clamp && !isCrippledGPU && CheckExtension("GL_ARB_depth_clamp")) {
    GCon->Log(NAME_Init, "Found GL_ARB_depth_clamp");
    HaveDepthClamp = true;
  } else if (!gl_dbg_disable_depth_clamp && !isCrippledGPU && CheckExtension("GL_NV_depth_clamp")) {
    GCon->Log(NAME_Init, "Found GL_NV_depth_clamp");
    HaveDepthClamp = true;
  } else {
    GCon->Log(NAME_Init, "Symbol not found, depth clamp extensions disabled.");
    HaveDepthClamp = false;
  }

  if (!isCrippledGPU && CheckExtension("GL_EXT_stencil_wrap")) {
    GCon->Log(NAME_Init, "Found GL_EXT_stencil_wrap");
    HaveStencilWrap = true;
  } else {
    GCon->Log(NAME_Init, "Symbol not found, stencil wrap extensions disabled.");
    HaveStencilWrap = false;
  }

  if (!isCrippledGPU && CheckExtension("GL_EXT_texture_compression_s3tc")) {
    GCon->Log(NAME_Init, "S3TC (texture compression) is supported");
    HaveS3TC = true;
  } else {
    GCon->Log(NAME_Init, "Symbol not found, stencil wrap extensions disabled.");
    HaveS3TC = false;
  }
  gl_s3tc_present = HaveS3TC;

  if (!HaveStencilWrap) GCon->Log(NAME_Init, "*** no stencil wrap --> no shadow volumes");
  if (!HaveDepthClamp) GCon->Log(NAME_Init, "*** no depth clamp --> no shadow volumes");
  //if (!p_glStencilFuncSeparate) GCon->Log(NAME_Init, "*** no separate stencil ops --> no shadow volumes");
  if (!p_glStencilOpSeparate) GCon->Log(NAME_Init, "*** no separate stencil ops --> no shadow volumes");

  if (!p_glGenerateMipmap || gl_dbg_fbo_blit_with_texture) {
    GCon->Log(NAME_Init, "OpenGL: bloom postprocessing effect disabled due to missing API");
    r_bloom = false;
    canIntoBloomFX = false;
  } else {
    canIntoBloomFX = true;
  }
  gl_can_bloom = canIntoBloomFX;

  if (hasBoundsTest) GCon->Log(NAME_Init, "Found GL_EXT_depth_bounds_test");

  DepthZeroOne = !!p_glClipControl;

  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glPixelStorei(GL_PACK_ROW_LENGTH, 0);
  glPixelStorei(GL_PACK_IMAGE_HEIGHT, 0);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);

  if (gl_use_srgb.asBool()) glEnable(GL_FRAMEBUFFER_SRGB); else glDisable(GL_FRAMEBUFFER_SRGB);
  GLDRW_RESET_ERROR();

  // allocate main FBO object
  mainFBO.createDepthStencil(this, calcWidth, calcHeight);
  mainFBO.scrScaled = (RealScreenWidth != ScreenWidth || RealScreenHeight != ScreenHeight);
  GCon->Logf(NAME_Init, "OpenGL: reverse z is %s", (useReverseZ ? "enabled" : "disabled"));
  p_glObjectLabelVA(GL_FRAMEBUFFER, mainFBO.getFBOid(), "Main FBO");

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

  // ambient light FBO object will be allocated on demand
  //ambLightFBO.createTextureOnly(this, calcWidth, calcHeight);
  //p_glObjectLabelVA(GL_FRAMEBUFFER, ambLightFBO.getFBOid(), "Ambient light FBO");

  // allocate wipe FBO object
  wipeFBO.createTextureOnly(this, calcWidth, calcHeight);
  p_glObjectLabelVA(GL_FRAMEBUFFER, wipeFBO.getFBOid(), "Wipe FBO");

  // `colormapSrcFBO` will be allocated on demand

  //if (major >= 3) canRenderShadowmaps = true;
  canRenderShadowmaps = !isCrippledGPU; //true;

  // check extensions
  if (canRenderShadowmaps) {
    if (!CheckExtension("GL_ARB_texture_cube_map") && !CheckExtension("GL_EXT_texture_cube_map")) {
      canRenderShadowmaps = false;
      GCon->Log(NAME_Init, "OpenGL: no cubemap support, shadowmaps disabled.");
    }
  }

  if (canRenderShadowmaps) {
    if (!CheckExtension("GL_ARB_texture_float")) {
      canRenderShadowmaps = false;
      GCon->Log(NAME_Init, "OpenGL: no floating point textures, shadowmaps disabled.");
    }
  }

  gl_can_shadowmaps = canRenderShadowmaps;
  gl_can_shadowvols = SupportsShadowVolumeRendering();

  mainFBO.activate();

  // create VBO for sprites
  vassert(!vboSprite.isValid());
  vassert(!vboSky.isValid());
  vassert(!vboMaskedSurf.isValid());
  vassert(!vboAdvSurf.isValid());
  vassert(!vboSMapSurf.isValid());
  vassert(!vboSMapSurfTex.isValid());

  vboSprite.setOwner(this);
  vboSprite.ensureDataSize(4); // sprite is always a quad, so we can allocate it right here

  vboSky.setOwner(this);
  vboMaskedSurf.setOwner(this);
  vboAdvSurf.setOwner(this, true); // streaming
  vboSMapSurf.setOwner(this, true); // streaming
  vboSMapSurfTex.setOwner(this, true); // streaming

  // init some defaults
  glBindTexture(GL_TEXTURE_2D, 0);
  p_glBindFramebuffer(GL_FRAMEBUFFER, 0);

  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClearDepth(!useReverseZ ? 1.0f : 0.0f);
  if (p_glClipControl) p_glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE); // actually, this is better even for "normal" cases
  RestoreDepthFunc();
  glDepthRange(0.0f, 1.0f);

  glClearStencil(0);

  glClear(GL_COLOR_BUFFER_BIT);
  Update(false); // only swap
  glClear(GL_COLOR_BUFFER_BIT);

  glEnable(GL_TEXTURE_2D);
  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
  glHint(GL_GENERATE_MIPMAP_HINT, GL_FASTEST); // this seems to affect only `glGenerateMipmap()`

  vassert(!atlasesGenerated);
  GenerateLightmapAtlasTextures();

  //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  //glAlphaFunc(GL_GREATER, getAlphaThreshold());
  glShadeModel(GL_FLAT); // we're doing our own lighting anyway
  glDisable(GL_ALPHA_TEST);
  //glDisable(GL_BLEND);
  glDisable(GL_DITHER);
  glDisable(GL_FOG);
  glDisable(GL_LIGHTING);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  SelectTexture(0);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);

  glDepthMask(GL_FALSE); // no z-buffer writes
  currDepthMaskState = 0;

  #ifndef GL4ES_HACKS
  glDisable(GL_POLYGON_SMOOTH);
  glDisable(GL_MULTISAMPLE);
  glDisable(GL_LINE_SMOOTH);
  #endif

  // this is used in model renderer
  if (gl_disable_primitive_restart) p_glPrimitiveRestartIndex = nullptr;
  if (p_glPrimitiveRestartIndex) {
    glEnable(GL_PRIMITIVE_RESTART);
    #ifdef VAVOOM_GLMODEL_32BIT_VIDX
    p_glPrimitiveRestartIndex(6553500);
    #else
    p_glPrimitiveRestartIndex(65535);
    #endif
    GCon->Log(NAME_Init, "OpenGL: `glPrimitiveRestartIndex()` found.");
    gl_can_use_primitive_restart = true;
  } else {
    GCon->Log(NAME_Init, "OpenGL: no `glPrimitiveRestartIndex()` found, optimised voxel rendering is disabled.");
    gl_can_use_primitive_restart = false;
  }

  GLDRW_RESET_ERROR();

  // shaders
  shaderHead = nullptr; // just in case

  GCon->Log(NAME_Init, "OpenGL: loading shaders");
  LoadAllShaders();
  LoadShadowmapShaders();

  p_glUseProgramObjectARB(0);
  currentActiveShader = nullptr;

#ifdef VV_SHADER_COMPILING_PROGRESS
  InitShaderProgress();
#endif

  GCon->Log(NAME_Init, "OpenGL: compiling shaders");
  CompileShaders(major, minor, canRenderShadowmaps);

#ifdef VV_SHADER_COMPILING_PROGRESS
  DoneShaderProgress();
#endif

  GLDRW_CHECK_ERROR("finish OpenGL initialization");

  InitGPUMemoryUse();

  gl_can_cas_filter = CasFX.CheckOpenGLVersion(glVerMajor, glVerMinor, false);

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  blendEnabled = true;

  glDisable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(0, 0);
  offsetEnabled = false;
  offsetFactor = offsetUnits = 0;

  mInitialized = true;

  currMainFBO = -1;
  currentActiveFBO = nullptr;
  ReactivateCurrentFBO();
  SetMainFBO(true); // forced

  callICB(VCB_InitResolution);

  /*
  if (canIntoBloomFX && r_bloom) Posteffect_Bloom(0, 0, calcWidth, calcHeight);
  currentActiveFBO = nullptr;
  ReactivateCurrentFBO();
  */
  GCon->Log(NAME_Init, "OpenGL: finished initialization.");
}

#undef gl_
#undef glc_
#undef glg_


//==========================================================================
//
//  VOpenGLDrawer::RecreateFBOs
//
//  recreate FBOs with fp/int formats (alpha is not guaranteed)
//  used for overbright
//
//==========================================================================
bool VOpenGLDrawer::RecreateFBOs (bool wantFP) {
  vassert(mainFBO.isValid());
  //vassert(ambLightFBO.isValid());

  const int mwdt = mainFBO.getWidth();
  const int mhgt = mainFBO.getHeight();
  vassert(mwdt > 0);
  vassert(mhgt > 0);

  FBO *currFBO = currentActiveFBO;

  // if we don't want FP color textures, it is very easy
  if (!wantFP) {
    // destroy main FP FBO, it is enough to tell the code what to do
    if (mainFBOFP.isValid()) {
      mainFBOFP.destroy();
      // also destroy ambient light FBO, it will be recreated if necessary
      ambLightFBO.destroy();
      // reactivate old FBO
      if (currFBO == &mainFBOFP) currFBO = nullptr;
      currentActiveFBO = currFBO;
      ReactivateCurrentFBO();
    }
    return true;
  }

  // here we definitely want FP color textures
  vassert(wantFP);

  // if we already have main FP FBO, there's nothing to do here
  if (mainFBOFP.isValid()) {
    vassert(mainFBOFP.isColorFloat());
    return true;
  }

  // allocate main FP FBO
  //const bool scrScaled = mainFBO.scrScaled;
  // we don't need depth/stencil, main FP FBO is just a texture holder
  //mainFBOFP.createDepthStencil(this, mwdt, mhgt, wantFP);
  mainFBOFP.createTextureOnly(this, mwdt, mhgt, wantFP);
  if (!mainFBOFP.isColorFloat()) {
    // oops, cannot do that
    mainFBOFP.destroy();
    return false;
  }
  //mainFBOFP.scrScaled = scrScaled;
  p_glObjectLabelVA(GL_FRAMEBUFFER, mainFBO.getFBOid(), "Main FBO");

  // ambient light FBO object will be allocated on demand, destroy the old one, because why not?
  ambLightFBO.destroy();

  currentActiveFBO = currFBO;
  ReactivateCurrentFBO();

  GCon->Log(NAME_Debug, "created FP FBO");

  return true;
}


//==========================================================================
//
//  VOpenGLDrawer::PrepareMainFBO
//
//==========================================================================
void VOpenGLDrawer::PrepareMainFBO () {
  // check invariants
  vassert(!mainFBO.isColorFloat());
  vassert(!mainFBOFP.isValid() || mainFBOFP.isColorFloat());

  // if we have main FP FBO, then use it for rendering
  if (mainFBOFP.isValid()) mainFBOFP.swapColorTextures(&mainFBO);
}


//==========================================================================
//
//  VOpenGLDrawer::RestoreMainFBO
//
//==========================================================================
void VOpenGLDrawer::RestoreMainFBO () {
  // switch to non-FP texture
  if (mainFBOFP.isValid()) {
    // check invariants
    vassert(mainFBO.isColorFloat());
    vassert(!mainFBOFP.isColorFloat());
    // swap textures, and blit FP texture to the normal one
    mainFBOFP.swapColorTextures(&mainFBO);
    mainFBOFP.blitTo(&mainFBO, 0, 0, mainFBOFP.getWidth(), mainFBOFP.getHeight(), 0, 0, mainFBO.getWidth(), mainFBO.getHeight(), GL_NEAREST);
  }
}


//==========================================================================
//
//  VOpenGLDrawer::IsMainFBOFloat
//
//==========================================================================
bool VOpenGLDrawer::IsMainFBOFloat () {
  return mainFBO.isColorFloat();
}


//==========================================================================
//
//  VOpenGLDrawer::IsCameraFBO
//
//==========================================================================
bool VOpenGLDrawer::IsCameraFBO () {
  return (currMainFBO >= 0);
}


//==========================================================================
//
//  VOpenGLDrawer::SetupTranslucentPass
//
//==========================================================================
void VOpenGLDrawer::SetupTranslucentPass () {
  //glDepthMask(GL_TRUE); // allow z-buffer writes
  GLEnableDepthWrite();
  RestoreDepthFunc();
}
