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
//**
//**  OpenGL driver, main module
//**
//**************************************************************************
#include <limits.h>
#include <float.h>
#include <stdarg.h>

#include "../../screen.h"
#include "gl_local.h"


extern VCvarB gl_tonemap_pal_hires;
extern VCvarI gl_tonemap_pal_algo;
extern VCvarB gl_can_cas_filter;

extern VCvarB r_adv_overbright;
extern VCvarF r_overbright_specular;


// ////////////////////////////////////////////////////////////////////////// //
class PostSrcMatrixSaver {
public:
  VOpenGLDrawer *self;
public:
  VV_DISABLE_COPY(PostSrcMatrixSaver)

  inline PostSrcMatrixSaver (VOpenGLDrawer *aself, bool doSave) noexcept {
    if (doSave) {
      self = aself;
      aself->PostSrcSaveMatrices();
    } else {
      self = nullptr;
    }
  }

  inline ~PostSrcMatrixSaver () noexcept {
    if (self) { self->PostSrcRestoreMatrices(); self = nullptr; }
  }
};


//==========================================================================
//
//  VOpenGLDrawer::EnsurePostSrcFBO
//
//  this will force main FBO with `SetMainFBO(true);`
//  it also copies main FBO to `postSrcFBO`
//  make sure to save matrices if necessary, because this will destroy them
//
//==========================================================================
void VOpenGLDrawer::EnsurePostSrcFBO () {
  // enforce main FBO
  SetMainFBO(true); // forced
  auto mfbo = GetMainFBO();

  // check dimensions for already created FBO
  if (postSrcFBO.isValid()) {
    if (postSrcFBO.getWidth() == mfbo->getWidth() && postSrcFBO.getHeight() == mfbo->getHeight()) {
      return;
    }
    // destroy it, and recreate
    postSrcFBO.destroy();
  }

  // create postsrc FBO
  postSrcFBO.createTextureOnly(this, mfbo->getWidth(), mfbo->getHeight());
  p_glObjectLabelVA(GL_FRAMEBUFFER, postSrcFBO.getFBOid(), "Posteffects Texture Source FBO");
}


//==========================================================================
//
//  VOpenGLDrawer::PreparePostSrcFBO
//
//  this will setup matrices, ensure source FBO, and
//  copy main FBO to postsrc FBO
//
//==========================================================================
void VOpenGLDrawer::PreparePostSrcFBO () {
  EnsurePostSrcFBO();
  auto mfbo = GetMainFBO();

  #if 0
  // copy main FBO to postprocess source FBO, so we can read it
  mfbo->blitTo(&postSrcFBO, 0, 0, mfbo->getWidth(), mfbo->getHeight(), 0, 0, postSrcFBO.getWidth(), postSrcFBO.getHeight(), GL_NEAREST);
  #else
  if (!postSrcFBO.swapColorTextures(mfbo)) {
    GCon->Logf(NAME_Debug, "FBO texture swapping failed, use blitting");
    mfbo->blitTo(&postSrcFBO, 0, 0, mfbo->getWidth(), mfbo->getHeight(), 0, 0, postSrcFBO.getWidth(), postSrcFBO.getHeight(), GL_NEAREST);
  }
  #endif
  mfbo->activate();

  SetOrthoProjection(0.0f, 1.0f, 0.0f, 1.0f);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  GLDisableBlend();
  GLDisableDepthWrite();
  glEnable(GL_TEXTURE_2D);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  // source texture
  SelectTexture(0);
  glBindTexture(GL_TEXTURE_2D, postSrcFBO.getColorTid());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  /*
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  */
}


//==========================================================================
//
//  VOpenGLDrawer::RenderPostSrcFullscreenQuad
//
//  should be called after `PreparePostSrcFBO()`
//
//==========================================================================
void VOpenGLDrawer::RenderPostSrcFullscreenQuad () {
  auto mfbo = GetMainFBO();
  vassert(postSrcFBO.getWidth() == mfbo->getWidth());
  vassert(postSrcFBO.getHeight() == mfbo->getHeight());
  //GCon->Logf(NAME_Debug, "width=%d; height=%d", mfbo->getWidth(), mfbo->getHeight());
  glBegin(GL_QUADS);
    #if 0
    glTexCoord3f(0.0f, 0.0f, 10.0f); glVertex2i(0, 0);
    glTexCoord3f(1.0f, 0.0f, 10.0f); glVertex2i(mfbo->getWidth(), 0);
    glTexCoord3f(1.0f, 1.0f, 10.0f); glVertex2i(mfbo->getWidth(), mfbo->getHeight());
    glTexCoord3f(0.0f, 1.0f, 10.0f); glVertex2i(0, mfbo->getHeight());
    #else
    glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(1.0f, 0.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(1.0f, 1.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, 1.0f);
    #endif
  glEnd();
}


//==========================================================================
//
//  VOpenGLDrawer::FinishPostSrcFBO
//
//==========================================================================
void VOpenGLDrawer::FinishPostSrcFBO () {
  // unbind texture 0
  SelectTexture(0);
  glBindTexture(GL_TEXTURE_2D, 0);
  // and deactivate shaders (just in case)
  DeactivateShader();
}


//==========================================================================
//
//  VOpenGLDrawer::PostSrcSaveMatrices
//
//==========================================================================
void VOpenGLDrawer::PostSrcSaveMatrices () {
  glPushAttrib(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_ENABLE_BIT|GL_VIEWPORT_BIT|GL_TRANSFORM_BIT);
  glMatrixMode(GL_MODELVIEW); glPushMatrix();
  glMatrixMode(GL_PROJECTION); glPushMatrix();
}


//==========================================================================
//
//  VOpenGLDrawer::PostSrcRestoreMatrices
//
//==========================================================================
void VOpenGLDrawer::PostSrcRestoreMatrices () {
  glPopAttrib();
  glMatrixMode(GL_PROJECTION); glPopMatrix();
  glMatrixMode(GL_MODELVIEW); glPopMatrix();
}


//==========================================================================
//
//  VOpenGLDrawer::Posteffect_Tonemap
//
//==========================================================================
void VOpenGLDrawer::Posteffect_Tonemap (int ax, int ay, int awidth, int aheight, bool restoreMatrices) {
  (void)ax; (void)ay; (void)awidth; (void)aheight;
  if (!tonemapPalLUT) {
    GeneratePaletteLUT();
  } else if (tonemapLastGamma != usegamma || tonemapMode != (int)gl_tonemap_pal_hires.asBool() || tonemapColorAlgo != gl_tonemap_pal_algo.asInt()) {
    tonemapMode = (int)gl_tonemap_pal_hires.asBool();
    GeneratePaletteLUT();
  }

  PostSrcMatrixSaver matsaver(this, restoreMatrices);
  PreparePostSrcFBO();

  // LUT texture
  SelectTexture(1);
  glBindTexture(GL_TEXTURE_2D, tonemapPalLUT);

  if (tonemapMode == 0) {
    TonemapPalette64.Activate();
    TonemapPalette64.SetScreenFBO(0);
    TonemapPalette64.SetTexPalLUT(1);
  } else {
    TonemapPalette128.Activate();
    TonemapPalette128.SetScreenFBO(0);
    TonemapPalette128.SetTexPalLUT(1);
  }

  currentActiveShader->UploadChangedUniforms();

  RenderPostSrcFullscreenQuad();

  // unbind texture 1
  glBindTexture(GL_TEXTURE_2D, 0);
  SelectTexture(0);

  FinishPostSrcFBO();
}


//==========================================================================
//
//  VOpenGLDrawer::Posteffect_ColorMap
//
//==========================================================================
void VOpenGLDrawer::Posteffect_ColorMap (int cmap, int ax, int ay, int awidth, int aheight) {
  (void)ax; (void)ay; (void)awidth; (void)aheight;
  if (cmap <= CM_Default || cmap >= CM_Max) return;

  //PostSrcMatrixSaver matsaver(this, restoreMatrices);
  PreparePostSrcFBO();

  // the formula is:
  //   i = clamp(i*IntRange.z+IntRange.a, IntRange.x, IntRange.y);
  //   vec3 clr = clamp(vec3(i, i, i)*ColorMult+ColorAdd, 0.0, 1.0);
  ColormapTonemap.Activate();
  ColormapTonemap.SetScreenFBO(0);
  ColormapTonemap.SetIntRange(0.0f, 1.0f, 1.0f, 0.0f);
  ColormapTonemap.SetColorMult(1.0f, 1.0f, 1.0f);
  ColormapTonemap.SetColorAdd(0.0f, 0.0f, 0.0f);

  switch (cmap) {
    case CM_Inverse:
      //vec3(1.0-i, 1.0-i, 1.0-i)
      ColormapTonemap.SetIntRange(0.0f, 1.0f, -1.0f, 1.0f);
      break;
    case CM_Gold:
      //vec3(min(1.0, i*1.5), i, 0)
      ColormapTonemap.SetColorMult(1.5f, 1.0f, 0.0f);
      break;
    case CM_Red:
      //vec3(min(1.0, i*1.5), 0, 0)
      ColormapTonemap.SetColorMult(1.5f, 0.0f, 0.0f);
      break;
    case CM_Green:
      //vec3(0.0, min(1.0, i*1.5), 0)
      ColormapTonemap.SetColorMult(0.0f, 1.5f, 0.0f);
      break;
    case CM_Mono:
      //vec3(min(1.0, i*1.5), min(1.0, i*1.5), i)
      ColormapTonemap.SetColorMult(1.5f, 1.5f, 1.0f);
      break;
    case CM_BeRed:
      //vec3(i, i*0.125, i*0.125)
      ColormapTonemap.SetColorMult(1.0f, 0.125f, 0.125f);
      break;
    case CM_Blue:
      //vec3(i*0.125, i*0.125, i)
      ColormapTonemap.SetColorMult(0.125f, 0.125f, 1.0f);
      break;
    default:
      Sys_Error("ketmar forgot to implement shader for colormap #%d", cmap);
  }

  currentActiveShader->UploadChangedUniforms();

  RenderPostSrcFullscreenQuad();
  FinishPostSrcFBO();
}


//==========================================================================
//
//  VOpenGLDrawer::Posteffect_Underwater
//
//==========================================================================
void VOpenGLDrawer::Posteffect_Underwater (float time, int ax, int ay, int awidth, int aheight, bool restoreMatrices) {
  (void)ax; (void)ay; (void)awidth; (void)aheight;
  PostSrcMatrixSaver matsaver(this, restoreMatrices);
  PreparePostSrcFBO();

  UnderwaterFX.Activate();
  UnderwaterFX.SetScreenFBO(0);
  //UnderwaterFX.SetTextureDimensions(postSrcFBO.getWidth(), postSrcFBO.getHeight());
  UnderwaterFX.SetInputTime(time);

  currentActiveShader->UploadChangedUniforms();

  RenderPostSrcFullscreenQuad();
  FinishPostSrcFBO();
}


//==========================================================================
//
//  VOpenGLDrawer::Posteffect_CAS
//
//==========================================================================
void VOpenGLDrawer::Posteffect_CAS (float coeff, int ax, int ay, int awidth, int aheight, bool restoreMatrices) {
  (void)ax; (void)ay; (void)awidth; (void)aheight;
  if (coeff < 0.001f) return;

  //if (!CasFX.CheckOpenGLVersion(glVerMajor, glVerMinor, false)) return;
  if (!gl_can_cas_filter.asBool()) return;

  PostSrcMatrixSaver matsaver(this, restoreMatrices);
  PreparePostSrcFBO();

  CasFX.Activate();
  CasFX.SetScreenFBO(0);
  CasFX.SetSharpenIntensity(coeff);

  currentActiveShader->UploadChangedUniforms();

  RenderPostSrcFullscreenQuad();
  FinishPostSrcFBO();
}




//==========================================================================
//
//  VOpenGLDrawer::PostprocessOvebright
//
//  normalize overbrighting for fp textures
//
//==========================================================================
void VOpenGLDrawer::PostprocessOvebright () {
  //GCon->Log(NAME_Debug, "VOpenGLDrawer::PostprocessOvebright!");

  glPushAttrib(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_ENABLE_BIT|GL_VIEWPORT_BIT|GL_TRANSFORM_BIT);
  glMatrixMode(GL_MODELVIEW); glPushMatrix();
  glMatrixMode(GL_PROJECTION); glPushMatrix();

  // enforce main FBO
  SetMainFBO(true); // forced
  auto mfbo = GetMainFBO();

  // check dimensions for already created FBO
  if (postOverFBO.isValid()) {
    if (postOverFBO.getWidth() != mfbo->getWidth() || postOverFBO.getHeight() != mfbo->getHeight() ||
        postOverFBO.isColorFloat() != mfbo->isColorFloat())
    {
      postOverFBO.destroy();
    }
  }

  if (!postOverFBO.isValid()) {
    // create postsrc FBO
    postOverFBO.createTextureOnly(this, mfbo->getWidth(), mfbo->getHeight(), mfbo->isColorFloat());
    p_glObjectLabelVA(GL_FRAMEBUFFER, postOverFBO.getFBOid(), "Posteffects Texture Source FBO");
    if (postOverFBO.isColorFloat() != mfbo->isColorFloat()) {
      GCon->Logf(NAME_Warning, "cannot create FP temprorary FBO, overbright is disabled.");
      r_adv_overbright = false;
      return;
    }
  }

  if (!postOverFBO.swapColorTextures(mfbo)) {
    GCon->Logf(NAME_Debug, "FBO texture swapping failed, use blitting");
    mfbo->blitTo(&postOverFBO, 0, 0, mfbo->getWidth(), mfbo->getHeight(), 0, 0, postOverFBO.getWidth(), postOverFBO.getHeight(), GL_NEAREST);
  }
  mfbo->activate();

  SetOrthoProjection(0.0f, 1.0f, 0.0f, 1.0f);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  GLDisableBlend();
  GLDisableDepthWrite();
  glEnable(GL_TEXTURE_2D);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  // source texture
  SelectTexture(0);
  glBindTexture(GL_TEXTURE_2D, postOverFBO.getColorTid());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  PostprocessOberbright.Activate();
  float spec = r_overbright_specular.asFloat();
  if (!isFiniteF(spec)) spec = 0.1f;
  spec = clampval(spec, 0.0f, 16.0f);
  PostprocessOberbright.SetSpecular(spec);

  currentActiveShader->UploadChangedUniforms();

  vassert(postOverFBO.getWidth() == mfbo->getWidth());
  vassert(postOverFBO.getHeight() == mfbo->getHeight());
  //GCon->Logf(NAME_Debug, "width=%d; height=%d", mfbo->getWidth(), mfbo->getHeight());
  glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(1.0f, 0.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(1.0f, 1.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, 1.0f);
  glEnd();

  // unbind texture 0
  SelectTexture(0);
  glBindTexture(GL_TEXTURE_2D, 0);
  // and deactivate shaders (just in case)
  DeactivateShader();

  glPopAttrib();
  glMatrixMode(GL_PROJECTION); glPopMatrix();
  glMatrixMode(GL_MODELVIEW); glPopMatrix();
}
