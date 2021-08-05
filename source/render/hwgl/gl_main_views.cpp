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
#include "../../psim/p_player.h"
#include "../../client/client.h"


VCvarI dbg_shadowmaps("dbg_shadowmaps", "0", "Show shadowmap cubemap?", CVAR_PreInit);

static VCvarI acc_colorswap_mode("acc_colorswap_mode", "0", "Colorblind color swap mode: 0=RGB; 1=GBR; 2=GRB; 3=BRG; 4=BGR; 5=RBG", CVAR_Archive);
static VCvarI acc_colormatrix_mode("acc_colormatrix_mode", "0", "Colormatrix colorblind emulation mode.", CVAR_Archive);

extern VCvarB gl_shadowmap_preclear;
extern VCvarB r_dbg_proj_aspect;


//==========================================================================
//
//  VOpenGLDrawer::Setup2D
//
//==========================================================================
void VOpenGLDrawer::Setup2D () {
  GLSetViewport(0, 0, getWidth(), getHeight());

  //glMatrixMode(GL_PROJECTION);
  //glLoadIdentity();
  SetOrthoProjection(0, getWidth(), getHeight(), 0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  //glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_ALPHA_TEST);
  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDisable(GL_SCISSOR_TEST);
  GLForceDepthWrite();
  GLForceBlend();
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  //if (HaveDepthClamp) glDisable(GL_DEPTH_CLAMP);
}


//==========================================================================
//
//  VOpenGLDrawer::StartUpdate
//
//==========================================================================
void VOpenGLDrawer::StartUpdate () {
  //glFinish();
  //VRenderLevelShared::ResetPortalPool(); // moved to `VRenderLevelShared::RenderPlayerView()`

  ActivateMainFBO();

  glBindTexture(GL_TEXTURE_2D, 0);
  //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  // turn off anisotropy
  //glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT), 1); // 1 is minimum, i.e. "off"

  if (usegamma != lastgamma) {
    FlushTextures(true); // forced
    lastgamma = usegamma;
  }

  Setup2D();
}


// matrices are from here: http://web.archive.org/web/20091001043530/http://www.colorjack.com/labs/colormatrix/
static const float KnownColorMatrices[8][4*3] = {
  // Protanopia [1]
  {0.567f, 0.433f,   0.0f, 0.0f,
   0.558f, 0.442f,   0.0f, 0.0f,
     0.0f, 0.242f, 0.758f, 0.0f},
  // Protanomaly [2]
  {0.817f, 0.183f,   0.0f, 0.0f,
   0.333f, 0.667f,   0.0f, 0.0f,
     0.0f, 0.125f, 0.875f, 0.0f},
  // Deuteranopia [3]
  {0.625f, 0.375f,   0.0f, 0.0f,
     0.7f,   0.3f,   0.0f, 0.0f,
     0.0f,   0.3f,   0.7f, 0.0f},
  // Deuteranomaly [4]
  {  0.8f,   0.2f,   0.0f, 0.0f,
   0.258f, 0.742f,   0.0f, 0.0f,
     0.0f, 0.142f, 0.858f, 0.0f},
  // Tritanopia [5]
  { 0.95f,  0.05f,   0.0f, 0.0f,
     0.0f, 0.433f, 0.567f, 0.0f,
     0.0f, 0.475f, 0.525f, 0.0f},
  // Tritanomaly [6]
  {0.967f, 0.033f,   0.0f, 0.0f,
     0.0f, 0.733f, 0.267f, 0.0f,
     0.0f, 0.183f, 0.817f, 0.0f},
  // Achromatopsia [7]
  {0.299f, 0.587f, 0.114f, 0.0f,
   0.299f, 0.587f, 0.114f, 0.0f,
   0.299f, 0.587f, 0.114f, 0.0f},
  // Achromatomaly [8]
  {0.618f, 0.320f, 0.062f, 0.0f,
   0.163f, 0.775f, 0.062f, 0.0f,
   0.163f, 0.320f, 0.516f, 0.0f},
};


//==========================================================================
//
//  VOpenGLDrawer::ApplyFullscreenPosteffects
//
//==========================================================================
void VOpenGLDrawer::ApplyFullscreenPosteffects () {
  const int cmat = acc_colormatrix_mode.asInt();
  const int mode = acc_colorswap_mode.asInt();

  // need to do something?
  if ((cmat < 1 || cmat > 8) && (mode < 1 || mode > 5)) {
    if (tempMainFBO.isValid()) tempMainFBO.destroy();
    return;
  }

  FBO *mfbo = GetMainFBO();

  // ensure temp FBO
  if (!tempMainFBO.isValid() ||
       tempMainFBO.getWidth() != mfbo->getWidth() ||
       tempMainFBO.getHeight() != mfbo->getHeight() ||
       tempMainFBO.isColorFloat() != mfbo->isColorFloat())
  {
    tempMainFBO.destroy();
  }

  if (!tempMainFBO.isValid()) {
    //tempMainFBO.createDepthStencil(this, mfbo->getWidth(), mfbo->getHeight(), mfbo->isColorFloat());
    tempMainFBO.createTextureOnly(this, mfbo->getWidth(), mfbo->getHeight(), mfbo->isColorFloat());
    p_glObjectLabelVA(GL_FRAMEBUFFER, tempMainFBO.getFBOid(), "FS-Posteffects Temporary Main FBO");
  }

  mfbo->blitTo(&tempMainFBO, 0, 0, mfbo->getWidth(), mfbo->getHeight(), 0, 0, tempMainFBO.getWidth(), tempMainFBO.getHeight(), GL_NEAREST);

  mfbo->SwapWith(&tempMainFBO);
  currentActiveFBO = nullptr;
  mfbo->activate();

  Posteffect_ColorBlind(mode);
  if (!(cmat < 1 || cmat > 8)) Posteffect_ColorMatrix(KnownColorMatrices[cmat-1]);
}


//==========================================================================
//
//  FinishFullscreenPosteffects
//
//==========================================================================
void VOpenGLDrawer::FinishFullscreenPosteffects () {
  if (tempMainFBO.isValid()) {
    FBO *mfbo = GetMainFBO();
    mfbo->SwapWith(&tempMainFBO);
    currentActiveFBO = nullptr;
    mfbo->activate();
  }
}


//==========================================================================
//
//  GetInverseCBMode
//
//==========================================================================
/*
static int GetInverseCBMode () noexcept {
  int mode = acc_colorswap_mode.asInt();
  switch (mode) {
    case 1: return 3; // 120 -> 201: gbr
    case 2: return 2; // 102 -> 102: grb
    case 3: return 1; // 201 -> 120: brg
    case 4: return 4; // 210 -> 210: bgr
    case 5: return 5; // 021 -> 021: rbg
  }
  return 0;
}
*/




//==========================================================================
//
//  VOpenGLDrawer::FinishUpdate
//
//==========================================================================
void VOpenGLDrawer::FinishUpdate () {
  //mainFBO.blitToScreen();
  ApplyFullscreenPosteffects();
  GetMainFBO()->blitToScreen();
  FinishFullscreenPosteffects();
  if (gl_shadowmap_preclear) ClearAllShadowMaps();
  glBindTexture(GL_TEXTURE_2D, 0);
  SetMainFBO(true); // forced
  glBindTexture(GL_TEXTURE_2D, 0);
  SetOrthoProjection(0, getWidth(), getHeight(), 0);
  //ActivateMainFBO();
  //glFlush();
}


//==========================================================================
//
//  VOpenGLDrawer::SetupView
//
//==========================================================================
void VOpenGLDrawer::SetupView (VRenderLevelDrawer *ARLev, const refdef_t *rd) {
  RendLev = ARLev;

  GLSetViewport(rd->x, getHeight()-rd->height-rd->y, rd->width, rd->height);
  vpmats.vport.setOrigin(rd->x, getHeight()-rd->height-rd->y);
  vpmats.vport.setSize(rd->width, rd->height);
  /*
  {
    GLint vport[4];
    glGetIntegerv(GL_VIEWPORT, vport);
    //vpmats.vport.setOrigin(vport[0], vport[1]);
    //vpmats.vport.setSize(vport[2], vport[3]);
    GCon->Logf(NAME_Debug, "VP: (%d,%d);(%d,%d) -- (%d,%d);(%d,%d)", vpmats.vport.x0, vpmats.vport.y0, vpmats.vport.width, vpmats.vport.height, vport[0], vport[1], vport[2], vport[3]);
  }
  */

  glBindTexture(GL_TEXTURE_2D, 0);

  if (!CanUseRevZ()) {
    // normal
    glClearDepth(1.0f);
    glDepthFunc(GL_LEQUAL);
  } else {
    // reversed
    glClearDepth(0.0f);
    glDepthFunc(GL_GEQUAL);
  }
  //RestoreDepthFunc();

  CalcProjectionMatrix(vpmats.projMat, /*ARLev,*/ rd);
  glMatrixMode(GL_PROJECTION);
  glLoadMatrixf(vpmats.projMat[0]);

  //vpmats.projMat = ProjMat;
  vpmats.modelMat.SetIdentity();

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  //glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

  glEnable(GL_CULL_FACE);
  glCullFace(GL_FRONT);

  glEnable(GL_DEPTH_TEST);
  GLForceDepthWrite();
  GLForceBlend();
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_ALPHA_TEST);
  //if (RendLev && RendLev->IsShadowVolumeRenderer() && HaveDepthClamp) glEnable(GL_DEPTH_CLAMP);
  //k8: there is no reason to not do it
  //if (HaveDepthClamp) glEnable(GL_DEPTH_CLAMP);

  glEnable(GL_TEXTURE_2D);
  glDisable(GL_STENCIL_TEST);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  glDisable(GL_SCISSOR_TEST);
  currentSVScissor[SCS_MINX] = currentSVScissor[SCS_MINY] = 0;
  currentSVScissor[SCS_MAXX] = currentSVScissor[SCS_MAXY] = 32000;

  glColor4f(1.0f, 1.0f, 1.0f, 1.0f); // why not
  glClear(GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT|(rd->drawworld && !rd->DrawCamera && clear ? GL_COLOR_BUFFER_BIT : 0));
  stencilBufferDirty = false;
  decalUsedStencil = false;

  /*
  if (!rd->DrawCamera && rd->drawworld && rd->width != ScreenWidth) {
    // draws the border around the view for different size windows
    R_DrawViewBorder();
  }
  */
}


//==========================================================================
//
//  VOpenGLDrawer::SetupViewOrg
//
//==========================================================================
void VOpenGLDrawer::SetupViewOrg () {
  glMatrixMode(GL_MODELVIEW);
  //glLoadIdentity();

  if (r_dbg_proj_aspect.asBool()) {
    CalcModelMatrix(vpmats.modelMat, vieworg, viewangles, MirrorClip);
  } else {
    //CalcModelMatrix(vpmats.modelMat, TVec(vieworg.x, vieworg.y, vieworg.z*PixelAspect), viewangles, MirrorClip);
    vpmats.modelMat.SetIdentity();
    vpmats.modelMat *= VMatrix4::RotateX(-90.0f); //glRotatef(-90, 1, 0, 0);
    vpmats.modelMat *= VMatrix4::RotateZ(90.0f); //glRotatef(90, 0, 0, 1);
    if (MirrorFlip) vpmats.modelMat *= VMatrix4::Scale(TVec(1, -1, 1)); //glScalef(1, -1, 1);
    vpmats.modelMat *= VMatrix4::RotateX(-viewangles.roll); //glRotatef(-viewangles.roll, 1, 0, 0);
    vpmats.modelMat *= VMatrix4::RotateY(-viewangles.pitch*PixelAspect); //glRotatef(-viewangles.pitch, 0, 1, 0);
    vpmats.modelMat *= VMatrix4::RotateZ(-viewangles.yaw); //glRotatef(-viewangles.yaw, 0, 0, 1);
    vpmats.modelMat *= VMatrix4::Translate(-TVec(vieworg.x, vieworg.y, vieworg.z*PixelAspect)); //glTranslatef(-vieworg.x, -vieworg.y, -vieworg.z);

    vpmats.modelMat *= VMatrix4::Scale(TVec(1.0f, 1.0f, PixelAspect));
  }

  glLoadMatrixf(vpmats.modelMat[0]);

  glCullFace(MirrorClip ? GL_BACK : GL_FRONT);

  //LoadVPMatrices();
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

#if 0
  if (r_shadowmaps.asBool() && CanRenderShadowMaps() && (dbg_shadowmaps.asInt()&0x01) != 0) {
    // right
    // left
    // top
    // bottom
    // back
    // front
    //   front, back
    //   left, right
    //   top, bottom

    T_SetFont(ConFont);
    T_SetAlign(hcenter, vtop);
    const char *cbnames[6] = { "POS-X", "NEG-X", "POS-Y", "NEG-Y", "POS-Z", "NEG-Z" };

    //const float ssize = shadowmapSize;
    const float ssize = 128.0f;
    const float fyofs = T_ToDrawerY(T_FontHeight()+2);
    const float ysize = (ssize+T_ToDrawerY(T_FontHeight()+4));
    GLDisableBlend();
    for (unsigned int face = 0; face < 6; ++face) {
      float xofs = (face%2)*(ssize+4);
      float yofs = (face/2%3)*ysize+fyofs*2;
      glDisable(GL_TEXTURE_2D);
      glEnable(GL_TEXTURE_2D);
      glEnable(GL_TEXTURE_CUBE_MAP);
      glBindTexture(GL_TEXTURE_CUBE_MAP, shadowCube[0].cubeTexId);
      DbgShadowMap.Activate();
      DbgShadowMap.SetTexture(0);
      DbgShadowMap.SetCubeFace(face+0.5f);
      DbgShadowMap.UploadChangedUniforms();
      glBegin(GL_QUADS);
        glTexCoord2f(0, 1); glVertex2f(xofs+0    , yofs+0);
        glTexCoord2f(1, 1); glVertex2f(xofs+ssize, yofs+0);
        glTexCoord2f(1, 0); glVertex2f(xofs+ssize, yofs+ssize);
        glTexCoord2f(0, 0); glVertex2f(xofs+0    , yofs+ssize);
      glEnd();
      glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
      glDisable(GL_TEXTURE_CUBE_MAP);
      glEnable(GL_TEXTURE_2D);
    }
    GLEnableBlend();

    for (unsigned int face = 0; face < 6; ++face) {
      int xpos = T_FromDrawerX((int)((face%2)*(ssize+4)+(ssize+4)/2));
      int ypos = T_FromDrawerY((int)((face/2%3)*ysize))+T_FontHeight()+2+1;
      T_DrawText(xpos, ypos, cbnames[face], CR_DARKBROWN);
    }
  }
#endif
}
