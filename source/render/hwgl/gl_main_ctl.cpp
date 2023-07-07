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
#include <limits.h>
#include <float.h>
#include <stdarg.h>

#include "gl_local.h"


//==========================================================================
//
//  VOpenGLDrawer::p_glObjectLabelVA
//
//==========================================================================
__attribute__((format(printf, 4, 5))) void VOpenGLDrawer::p_glObjectLabelVA (GLenum identifier, GLuint name, const char *fmt, ...) {
  if (!glDebugEnabled || !glMaxDebugLabelLen) return;
  if (!fmt || !fmt[0]) {
    p_glObjectLabel(identifier, name, 0, "");
  } else {
    va_list ap;
    va_start(ap, fmt);
    char *res = vavarg(fmt, ap);
    va_end(ap);
    //if (!res[0]) return;
    size_t slen = strlen(res);
    if (slen > (unsigned)glMaxDebugLabelLen) slen = glMaxDebugLabelLen;
    p_glObjectLabel(identifier, name, (GLsizei)slen, res);
  }
}


//==========================================================================
//
//  VOpenGLDrawer::p_glDebugLogf
//
//==========================================================================
__attribute__((format(printf, 2, 3))) void VOpenGLDrawer::p_glDebugLogf (const char *fmt, ...) {
  if (!fmt || !fmt[0] || !glDebugEnabled || !glMaxDebugLabelLen) return;
  va_list ap;
  va_start(ap, fmt);
  char *res = vavarg(fmt, ap);
  va_end(ap);
  if (!res[0]) return;
  GCon->Logf(NAME_DebugGL, "USER: %s", res);
}


//==========================================================================
//
//  VOpenGLDrawer::CheckExtension
//
//==========================================================================
bool VOpenGLDrawer::CheckExtension (const char *ext) {
  if (!ext || !ext[0]) return false;
  TArray<VStr> Exts;
  VStr((char *)glGetString(GL_EXTENSIONS)).Split(' ', Exts);
  for (int i = 0; i < Exts.length(); ++i) if (Exts[i] == ext) return true;
  return false;
}


//==========================================================================
//
//  VOpenGLDrawer::SupportsShadowVolumeRendering
//
//==========================================================================
bool VOpenGLDrawer::SupportsShadowVolumeRendering () {
  return (HaveStencilWrap && /*p_glStencilFuncSeparate*/p_glStencilOpSeparate && HaveDepthClamp);
}


//==========================================================================
//
//  VOpenGLDrawer::SupportsShadowMapRendering
//
//==========================================================================
bool VOpenGLDrawer::SupportsShadowMapRendering () {
  return CanRenderShadowMaps();
}


//==========================================================================
//
//  VOpenGLDrawer::UseFrustumFarClip
//
//==========================================================================
bool VOpenGLDrawer::UseFrustumFarClip () {
  if (CanUseRevZ()) return false;
  if (RendLev && !HaveDepthClamp && RendLev->IsShadowVolumeRenderer() &&
      !RendLev->IsShadowMapRenderer()) return false;
  return true;
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
//  VOpenGLDrawer::SetProjectionMatrix
//
//==========================================================================
void VOpenGLDrawer::SetProjectionMatrix (const VMatrix4 &mat) {
  glMatrixMode(GL_PROJECTION);
  glLoadMatrixf(mat[0]);
  glMatrixMode(GL_MODELVIEW);
}


//==========================================================================
//
//  VOpenGLDrawer::SetModelMatrix
//
//==========================================================================
void VOpenGLDrawer::SetModelMatrix (const VMatrix4 &mat) {
  glMatrixMode(GL_MODELVIEW);
  glLoadMatrixf(mat[0]);
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
/*
void VOpenGLDrawer::LoadVPMatrices () {
  glGetFloatv(GL_PROJECTION_MATRIX, vpmats.projMat[0]);
  glGetFloatv(GL_MODELVIEW_MATRIX, vpmats.modelMat[0]);
  GLint vport[4];
  glGetIntegerv(GL_VIEWPORT, vport);
  vpmats.vport.setOrigin(vport[0], vport[1]);
  vpmats.vport.setSize(vport[2], vport[3]);
}
*/


//==========================================================================
//
//  VOpenGLDrawer::PushDepthMask
//
//==========================================================================
/*
void VOpenGLDrawer::PushDepthMask () {
  if (depthMaskSP >= MaxDepthMaskStack) Sys_Error("OpenGL: depth mask stack overflow");
  glGetIntegerv(GL_DEPTH_WRITEMASK, &depthMaskStack[depthMaskSP]);
  ++depthMaskSP;
}
*/


//==========================================================================
//
//  VOpenGLDrawer::PopDepthMask
//
//==========================================================================
/*
void VOpenGLDrawer::PopDepthMask () {
  if (depthMaskSP == 0) Sys_Error("OpenGL: depth mask stack underflow");
  glDepthMask(depthMaskStack[--depthMaskSP]);
}
*/


//==========================================================================
//
//  VOpenGLDrawer::RestoreDepthFunc
//
//==========================================================================
void VOpenGLDrawer::RestoreDepthFunc () {
  glDepthFunc(!CanUseRevZ() ? GL_LEQUAL : GL_GEQUAL);
}


static const GLint GLLevelMinFilter[5] = {
  GL_NEAREST, // 0: nearest, no mipmaps
  GL_NEAREST_MIPMAP_NEAREST, // 1: nearest mipmap
  GL_LINEAR_MIPMAP_NEAREST, // 2: linear nearest
  GL_LINEAR, // 3: bilinear
  GL_LINEAR_MIPMAP_LINEAR, // 4: trilinear
};

static const GLint GLLevelMagFilter[5] = {
  GL_NEAREST, // 0: nearest, no mipmaps
  GL_NEAREST, // 1: nearest mipmap
  GL_LINEAR, // 2: linear nearest
  GL_LINEAR, // 3: bilinear
  GL_LINEAR, // 4: trilinear
};


//==========================================================================
//
//  VOpenGLDrawer::RestoreDepthFunc
//
//==========================================================================
static inline void SetGLFilteringMode (int level) noexcept {
  level = clampval(level, 0, 4);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GLLevelMinFilter[(unsigned)level]);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GLLevelMagFilter[(unsigned)level]);
}


//==========================================================================
//
//  VOpenGLDrawer::SetupShadowTextureFiltering
//
//==========================================================================
void VOpenGLDrawer::SetupShadowTextureFiltering (VTexture *Tex) {
  if (!Tex || Tex->Type == TEXTYPE_Null) return;
  const unsigned ck = Tex->checkFiltering(0, 1, false)&~(VTexture::FilterWrongModel/*|VTexture::FilterWrongAniso*/); // `asModel` doesn't matter here
  if (ck) {
    if (ck&VTexture::FilterWrongLevel) {
      SetGLFilteringMode(0);
    }
    if ((ck&VTexture::FilterWrongAniso) && anisotropyExists) {
      glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT), 1.0f);
    }
    Tex->setFiltering(0, 1, Tex->filteringAsModel());
  }
}


//==========================================================================
//
//  VOpenGLDrawer::ForceTextureFiltering
//
//==========================================================================
void VOpenGLDrawer::ForceTextureFiltering (VTexture *Tex, int level, int wrap) {
  if (!Tex || Tex->Type == TEXTYPE_Null) return;

  // wrapping
  const GLint rep = (wrap < 0 ? ClampToEdge : wrap > 0 ? GL_REPEAT : (Tex->isWrapRepeat() ? GL_REPEAT : ClampToEdge));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, rep);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, rep);

  if (Tex->bForceNoFilter) {
    if (anisotropyExists) glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT), (GLfloat)1.0f);
    SetGLFilteringMode(0);
  } else {
    // anisotropy
    if (anisotropyExists) {
      int aniso = (level != -666 ? gl_texture_filter_anisotropic.asInt() : 1);
      aniso = clampval(aniso, 1, max_anisotropy); // `max_anisotropy` clamped to 255
      glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT), (GLfloat)aniso);
    }

    // setup filtering
    SetGLFilteringMode(level);
  }

  // this is called when "non-main" (i.e. translated, colormapped, or shaded) texture was bound
  // do not record new filtering mode, 'cause it affects only the main texture, not secondaries
  //Tex->setFiltering(level, aniso, (rep == GL_REPEAT));
}


//==========================================================================
//
//  VOpenGLDrawer::SetupTextureFiltering
//
//==========================================================================
void VOpenGLDrawer::SetupTextureFiltering (VTexture *Tex, int level, int wrap) {
  if (!Tex) return;

  // anisotropy (calculate it here, we need to check and set it later)
  int aniso;
  if (Tex->bForceNoFilter) {
    level = 0;
    aniso = 1;
  } else if (level == -666) {
    aniso = 1;
    level = 0;
  } else {
    aniso = clampval(gl_texture_filter_anisotropic.asInt(), 1, max_anisotropy); // `max_anisotropy` clamped to 255
    //aniso = clampval(aniso, 1, 255);
    // why not?
    level = clampval(level, 0, 4);
  }

  // repeat mode
  const GLint rep = (wrap < 0 ? ClampToEdge : wrap > 0 ? GL_REPEAT : (Tex->isWrapRepeat() ? GL_REPEAT : ClampToEdge));

  // check for changes
  const unsigned ck = Tex->checkFiltering(level, aniso, (rep == GL_REPEAT));

  // wrapping
  if (ck&VTexture::FilterWrongModel) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, rep);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, rep);
  }

  // anisotropy
  if ((ck&VTexture::FilterWrongAniso) && anisotropyExists) {
    glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT), (GLfloat)aniso);
  }

  // filtering
  if (ck&VTexture::FilterWrongLevel) {
    SetGLFilteringMode(level);
  }

  if (ck) Tex->setFiltering(level, aniso, (rep == GL_REPEAT));
}


//==========================================================================
//
//  VOpenGLDrawer::SetupBlending
//
//==========================================================================
void VOpenGLDrawer::SetupBlending (const RenderStyleInfo &ri) {
  switch (ri.translucency) {
    case RenderStyleInfo::Translucent: // normal translucency
    case RenderStyleInfo::Shaded: // normal translucency
    case RenderStyleInfo::Fuzzy: // normal translucency
      //glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      break;
    case RenderStyleInfo::Additive: // additive translucency
    case RenderStyleInfo::AddShaded: // additive translucency
      glBlendFunc(GL_ONE, GL_ONE); // our source rgb is already premultiplied
      break;
    case RenderStyleInfo::DarkTrans: // translucent-dark (k8vavoom special)
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      break;
    case RenderStyleInfo::Subtractive: // subtractive translucency
      //glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      glBlendFunc(GL_ONE, GL_ONE); // dunno, looks like it
      if (p_glBlendEquationSeparate) {
        //glBlendEquationSeparate(GL_FUNC_SUBTRACT, GL_FUNC_ADD);
        p_glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_ADD);
      } else {
        // at least something
        p_glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
      }
      break;
    default: // normal
      /*
      if (trans) {
        //restoreBlend = true; // default blending
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      }
      */
      break;
  }
}


//==========================================================================
//
//  VOpenGLDrawer::RestoreBlending
//
//==========================================================================
void VOpenGLDrawer::RestoreBlending (const RenderStyleInfo &ri) {
  switch (ri.translucency) {
    case RenderStyleInfo::Additive: // additive translucency
    case RenderStyleInfo::AddShaded: // additive translucency
    case RenderStyleInfo::DarkTrans: // translucent-dark (k8vavoom special)
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      break;
    case RenderStyleInfo::Subtractive: // subtractive translucency
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      p_glBlendEquation(GL_FUNC_ADD);
      break;
    default:
      break;
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

  ApplyFullscreenPosteffects(); // so screenshots will look ok

  glBindTexture(GL_TEXTURE_2D, GetMainFBO()->getColorTid());
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  void *dst = Z_Malloc(GetMainFBO()->getWidth()*GetMainFBO()->getHeight()*3);
  #if 0
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, dst);
  #else
  float *tmp = (float *)Z_Malloc(GetMainFBO()->getWidth()*GetMainFBO()->getHeight()*sizeof(float)*3);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_FLOAT, tmp);
  vuint8 *d = (vuint8 *)dst;
  float *s = tmp;
  for (int f = GetMainFBO()->getWidth()*GetMainFBO()->getHeight()*3; f > 0; --f) {
    float v = *s++;
    v = clampval(v, 0.0f, 1.0f);
    *d++ = clampToByte((int)(v*255.0f));
  }
  Z_Free(tmp);
  #endif
  glBindTexture(GL_TEXTURE_2D, oldbindtex);

  // restore main FBO
  FinishFullscreenPosteffects();

  *bpp = 24;
  *bot2top = true;
  return dst;
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
//  used only to render skies
//
//==========================================================================
/*
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
    //glHint(GL_FOG_HINT, GL_DONT_CARE);
    glHint(GL_FOG_HINT, GL_NICEST);
    glEnable(GL_FOG);
  } else {
    glDisable(GL_FOG);
  }
  CurrentFade = NewFade;
}
*/


//==========================================================================
//
//  VOpenGLDrawer::DebugRenderScreenRect
//
//==========================================================================
void VOpenGLDrawer::DebugRenderScreenRect (int x0, int y0, int x1, int y1, vuint32 color) {
  glPushAttrib(/*GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_ENABLE_BIT|GL_VIEWPORT_BIT|GL_TRANSFORM_BIT*/GL_ALL_ATTRIB_BITS);
  const bool oldBlend = blendEnabled;
  const GLint oldCurrDepthMaskState = currDepthMaskState;

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
  //glDepthMask(GL_FALSE); // no z-buffer writes
  GLDisableDepthWrite();
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  //p_glUseProgramObjectARB(0);

  DrawFixedCol.Activate();
  DrawFixedCol.SetColor(
    (GLfloat)(((color>>16)&255)/255.0f),
    (GLfloat)(((color>>8)&255)/255.0f),
    (GLfloat)((color&255)/255.0f), ((color>>24)&0xff)/255.0f);
  DrawFixedCol.UploadChangedUniforms();

  SetOrthoProjection(0, getWidth(), getHeight(), 0);
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
  currDepthMaskState = oldCurrDepthMaskState;
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
//  VOpenGLDrawer::GLEnableOffset
//
//==========================================================================
void VOpenGLDrawer::GLEnableOffset () {
  if (!offsetEnabled) {
    offsetEnabled = true;
    glEnable(GL_POLYGON_OFFSET_FILL);
    //glPolygonOffset(afactor, aunits); // just in case
  }
}


//==========================================================================
//
//  VOpenGLDrawer::GLDisableOffset
//
//==========================================================================
void VOpenGLDrawer::GLDisableOffset () {
  if (offsetEnabled) {
    offsetEnabled = false;
    glDisable(GL_POLYGON_OFFSET_FILL);
    //glPolygonOffset(0, 0); // just in case
  }
}


//==========================================================================
//
//  VOpenGLDrawer::GLPolygonOffset
//
//==========================================================================
void VOpenGLDrawer::GLPolygonOffset (const float afactor, const float aunits) {
  if (afactor != offsetFactor || aunits != offsetUnits || !offsetEnabled) {
    offsetFactor = afactor;
    offsetUnits = aunits;
    glPolygonOffset(afactor, aunits);
    if (!offsetEnabled) {
      offsetEnabled = true;
      glEnable(GL_POLYGON_OFFSET_FILL);
    }
  }
}


//==========================================================================
//
//  VOpenGLDrawer::ClearScreen
//
//==========================================================================
void VOpenGLDrawer::ClearScreen (unsigned clearFlags) {
  GLuint flags = 0;
  if (clearFlags&CLEAR_COLOR) flags |= GL_COLOR_BUFFER_BIT;
  if (clearFlags&CLEAR_DEPTH) flags |= GL_DEPTH_BUFFER_BIT;
  if (clearFlags&CLEAR_STENCIL) {
    flags |= GL_STENCIL_BUFFER_BIT;
    stencilBufferDirty = false;
    decalUsedStencil = false;
  }
  if (flags) glClear(flags);
}
