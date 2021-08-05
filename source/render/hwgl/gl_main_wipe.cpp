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

#include "gl_local.h"


extern VCvarI r_wipe_type;
extern VCvarF r_wipe_duration;


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
  // /*static*/ const float WipeDur = 1.0f;

  if (time < 0.0f) {
    wipeFBO.blitTo(&mainFBO, 0, 0, mainFBO.getWidth(), mainFBO.getHeight(), 0, 0, mainFBO.getWidth(), mainFBO.getHeight(), GL_NEAREST);
    return true;
  }

  float WipeDur = r_wipe_duration.asFloat();
       if (!isFiniteF(WipeDur) || WipeDur < 0.1f) WipeDur = 0.1f;
  else if (WipeDur > 15.0f) WipeDur = 15.0f;

  //GCon->Logf(NAME_Debug, "WIPE: time=%g", time);

  glPushAttrib(GL_ALL_ATTRIB_BITS);
  const bool oldBlend = blendEnabled;
  const GLint oldCurrDepthMaskState = currDepthMaskState;

  GLSetViewport(0, 0, getWidth(), getHeight());

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  //glLoadIdentity();
  SetOrthoProjection(0, getWidth(), getHeight(), 0);

  //if (HaveDepthClamp) glDisable(GL_DEPTH_CLAMP);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  //GLEnableBlend();
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_SCISSOR_TEST);
  //glDepthMask(GL_FALSE); // no z-buffer writes
  GLDisableDepthWrite();
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

  GLEnableBlend();
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // premultiplied

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, wipeFBO.getColorTid());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  switch (r_wipe_type.asInt()) {
    default:
    case 0:
      DrawWipeType0.Activate();
      DrawWipeType0.SetTexture(0);
      DrawWipeType0.SetWipeTime(time);
      DrawWipeType0.SetWipeDuration(WipeDur);
      DrawWipeType0.SetScreenSize((float)getWidth(), (float)getHeight());
      break;
    case 1:
      DrawWipeType1.Activate();
      DrawWipeType1.SetTexture(0);
      DrawWipeType1.SetWipeTime(time);
      DrawWipeType1.SetWipeDuration(WipeDur);
      DrawWipeType1.SetScreenSize((float)getWidth(), (float)getHeight());
      break;
    case 2:
      DrawWipeType2.Activate();
      DrawWipeType2.SetTexture(0);
      DrawWipeType2.SetWipeTime(time);
      DrawWipeType2.SetWipeDuration(WipeDur);
      break;
  }

  currentActiveShader->UploadChangedUniforms();

  glBegin(GL_QUADS);
    glTexCoord2f(0, 1); glVertex2f(0, 0);
    glTexCoord2f(1, 1); glVertex2f(getWidth(), 0);
    glTexCoord2f(1, 0); glVertex2f(getWidth(), getHeight());
    glTexCoord2f(0, 0); glVertex2f(0, getHeight());
  glEnd();

  //GLDisableBlend();
  glBindTexture(GL_TEXTURE_2D, 0);

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();

  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();

  glPopAttrib();
  p_glUseProgramObjectARB(0);
  currentActiveShader = nullptr;

  blendEnabled = oldBlend;
  currDepthMaskState = oldCurrDepthMaskState;

  //wipeFBO.blitTo(&mainFBO, 0, 0, mainFBO.getWidth(), mainFBO.getHeight(), 0, 0, mainFBO.getWidth(), mainFBO.getHeight(), GL_NEAREST);
  return (time <= WipeDur);
}
