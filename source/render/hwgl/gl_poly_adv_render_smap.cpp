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
//**  Copyright (C) 2018-2020 Ketmar Dark
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
#include "gl_local.h"
#include "gl_poly_adv_render.h"


//==========================================================================
//
//  VOpenGLDrawer::BeginLightShadowVolumes
//
//==========================================================================
void VOpenGLDrawer::BeginLightShadowMaps (const TVec &LightPos, const float Radius, const TVec &aconeDir, const float aconeAngle) {
  glDepthMask(GL_TRUE); // due to shadow volumes pass settings
  p_glBindFramebuffer(GL_FRAMEBUFFER, cubeFBO);

  glClearDepth(0.0f);
  if (p_glClipControl) p_glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE); // actually, this is better even for "normal" cases
  glDepthRange(0.0f, 1.0f);
  glDepthFunc(GL_LESS);

  //glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  glDisable(GL_CULL_FACE);
  //glEnable(GL_CULL_FACE);
  //!glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

  SurfShadowMap.Activate();

  glGetIntegerv(GL_VIEWPORT, savedSMVPort);
  glViewport(0, 0, shadowmapSize, shadowmapSize);

  glDepthMask(GL_FALSE); // due to shadow volumes pass settings
  glDisable(GL_DEPTH_TEST);

  glDepthMask(GL_TRUE);
  glEnable(GL_DEPTH_TEST);
  //glClearDepth(1.0f);
  //glDepthFunc(GL_GREATER);

  glDisable(GL_TEXTURE_2D);
  GLDRW_RESET_ERROR();
}


//==========================================================================
//
//  VOpenGLDrawer::EndLightShadowMaps
//
//==========================================================================
void VOpenGLDrawer::EndLightShadowMaps () {
  currentActiveFBO = nullptr;
  mainFBO.activate();
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  RestoreDepthFunc();
  glClearDepth(!useReverseZ ? 1.0f : 0.0f);
  glViewport(savedSMVPort[0], savedSMVPort[1], savedSMVPort[2], savedSMVPort[3]);
  glEnable(GL_CULL_FACE);
  glDepthMask(GL_FALSE); // due to shadow volumes pass settings
  glEnable(GL_DEPTH_TEST);
  glClearDepth(0.0f);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // black background
}


//==========================================================================
//
//  matDump
//
//==========================================================================
static VVA_OKUNUSED void matDump (const VMatrix4 &mat) {
  GCon->Logf(NAME_Debug, "=======");
  for (int y = 0; y < 4; ++y) {
    VStr s;
    for (int x = 0; x < 4; ++x) {
      s += va(" %g", mat.m[y][x]);
    }
    GCon->Logf(NAME_Debug, "%s", *s);
  }
}


//==========================================================================
//
//  VOpenGLDrawer::SetupLightShadowMap
//
//==========================================================================
void VOpenGLDrawer::SetupLightShadowMap (const TVec &LightPos, const float Radius, const TVec &aconeDir, const float aconeAngle, unsigned int facenum) {
  //GCon->Logf(NAME_Debug, "--- facenum=%u ---", facenum);
  const TVec viewsCenter[6] = {
    TVec( 1,  0,  0),
    TVec(-1,  0,  0),
    TVec( 0,  1,  0),
    TVec( 0, -1,  0),
    TVec( 0,  0,  1),
    TVec( 0,  0, -1),
  };
  const TVec viewsUp[6] = {
    TVec(0, 1,  0), // or -1
    TVec(0, 1,  0), // or -1
    TVec(0, 0, -1), // or 1
    TVec(0, 0,  1), // or -1
    TVec(0, 1,  0), // or -1
    TVec(0, 1,  0), // or -1
  };
  //VMatrix4 newPrj = VMatrix4::ProjectionZeroOne(90.0f, 1.0f, 2.0f, Radius);
  //VMatrix4 newPrj = VMatrix4::ProjectionNegOne(90.0f, 1.0f, 2.0f, Radius);
  VMatrix4 newPrj = VMatrix4::Perspective(90.0f, 1.0f, 2.0f, Radius);
  //matDump(newPrj);
  //matDump(newPrj1);
  /*
  VMatrix4 lview = VMatrix4::TranslateNeg(LightPos);
  VMatrix4 aface = VMatrix4::LookAtGLM(TVec(0, 0, 0), viewsCenter[facenum], viewsUp[facenum]);
  VMatrix4 mvp = newPrj*aface*lview; // *mview
  */
  VMatrix4 lview = VMatrix4::LookAtGLM(LightPos, viewsCenter[facenum], viewsUp[facenum]);
  //VMatrix4 lview = VMatrix4::LookAtGLM(TVec(0, 0, 0), viewsCenter[facenum], viewsUp[facenum]);
  VMatrix4 mvp = newPrj*lview; //*mview
  //SurfShadowMap.SetLightPos(LightPos);

  p_glBindFramebuffer(GL_FRAMEBUFFER, cubeFBO);
  GLDRW_CHECK_ERROR("set cube FBO");

  //!p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X+facenum, cubeTexId, 0);
  //!GLDRW_CHECK_ERROR("set cube FBO face");

  p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, cubeDepthTexId, 0);
  GLDRW_CHECK_ERROR("set framebuffer depth texture");
  p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X+facenum, cubeTexId, 0);
  GLDRW_CHECK_ERROR("set cube FBO face");
  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  GLDRW_CHECK_ERROR("set cube FBO draw buffer");
  glReadBuffer(GL_NONE);
  GLDRW_CHECK_ERROR("set cube FBO read buffer");

  //glClearColor(0.0f, 1.0f, 0.0f, 0.0f); // black background
  glClearDepth(1.0f);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  GLDRW_CHECK_ERROR("clear cube FBO");

#if 0
  VMatrix4 prjOrtho = VMatrix4::Ortho(0, shadowmapSize, 0, shadowmapSize, -666.0f, 666.0f);
  SurfShadowMapClear.Activate();
  SurfShadowMapClear.SetLightMPV(prjOrtho);
  SurfShadowMapClear.UploadChangedUniforms();

  glDepthMask(GL_TRUE);
  glDisable(GL_DEPTH_TEST);
  glBegin(GL_QUADS);
    glVertex2f(-shadowmapSize,  shadowmapSize);
    glVertex2f( shadowmapSize,  shadowmapSize);
    glVertex2f( shadowmapSize, -shadowmapSize);
    glVertex2f(-shadowmapSize, -shadowmapSize);
  glEnd();
#endif

  glEnable(GL_DEPTH_TEST);
  //glDisable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  //glDepthFunc(GL_GREATER);
  SurfShadowMap.Activate();
  SurfShadowMap.SetLightMPV(mvp);
  SurfShadowMap.UploadChangedUniforms();
  GLDRW_CHECK_ERROR("update cube FBO shader");
}


//==========================================================================
//
//  VOpenGLDrawer::RenderSurfaceShadowMap
//
//==========================================================================
void VOpenGLDrawer::RenderSurfaceShadowMap (const surface_t *surf, const TVec &LightPos, float Radius) {
  if (gl_dbg_wireframe) return;
  if (surf->count < 3) return; // just in case
  //return;

  //if (gl_smart_reject_shadows && !AdvRenderCanSurfaceCastShadow(surf, LightPos, Radius)) return;

  const unsigned vcount = (unsigned)surf->count;
  const SurfVertex *sverts = surf->verts;
  const SurfVertex *v = sverts;

  currentActiveShader->UploadChangedUniforms();
  //currentActiveShader->UploadChangedAttrs();

  //GCon->Logf(NAME_Debug, "  sfc: 0x%08x", (unsigned)(uintptr_t)surf);

  #if 1
  //glBegin(GL_POLYGON);
  glBegin(GL_TRIANGLE_FAN);
    for (unsigned i = 0; i < vcount; ++i, ++v) glVertex(v->vec());
  glEnd();
  #else
  const float size = 68.0f;
  // top
  glBegin(GL_QUADS);
    glVertex3f(-size, -size, -size);
    glVertex3f( size, -size, -size);
    glVertex3f( size,  size, -size);
    glVertex3f(-size,  size, -size);
  glEnd();
  // bottom
  glBegin(GL_QUADS);
    glVertex3f(-size, -size, size);
    glVertex3f( size, -size, size);
    glVertex3f( size,  size, size);
    glVertex3f(-size,  size, size);
  glEnd();
  // left
  glBegin(GL_QUADS);
    glVertex3f(-size, -size, -size);
    glVertex3f(-size,  size, -size);
    glVertex3f(-size,  size,  size);
    glVertex3f(-size, -size,  size);
  glEnd();
  // right
  glBegin(GL_QUADS);
    glVertex3f(size, -size, -size);
    glVertex3f(size,  size, -size);
    glVertex3f(size,  size,  size);
    glVertex3f(size, -size,  size);
  glEnd();
  // rear
  glBegin(GL_QUADS);
    glVertex3f(-size, -size, -size);
    glVertex3f( size, -size, -size);
    glVertex3f( size, -size,  size);
    glVertex3f(-size, -size,  size);
  glEnd();
  // front
  glBegin(GL_QUADS);
    glVertex3f(-size, size, -size);
    glVertex3f( size, size, -size);
    glVertex3f( size, size,  size);
    glVertex3f(-size, size,  size);
  glEnd();
  #endif
}
