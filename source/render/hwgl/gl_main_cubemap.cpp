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


//**************************************************************************
//
//  VOpenGLDrawer::ShadowCubeMapData
//
//**************************************************************************

//==========================================================================
//
//  VOpenGLDrawer::ShadowCubeMapData::createTextures
//
//==========================================================================
void VOpenGLDrawer::ShadowCubeMapData::createTextures (VOpenGLDrawer *aowner, const unsigned ashift, const bool afloat32) noexcept {
  bool recreateTextures = false;

  const int shadowmapSize = 64<<ashift;

  for (;;) {
    destroyTextures();
    mOwner = aowner;
    float32 = afloat32;
    shift = ashift;
    GLDRW_RESET_ERROR();

    #ifdef VV_SHADOWCUBE_DEPTH_TEXTURE
    glGenTextures(6, &cubeDepthTexId[0]);
    GLDRW_CHECK_ERROR("create shadowmap depth textures");
    #else
    aowner->p_glGenRenderbuffers(6, &cubeDepthRBId[0]);
    GLDRW_CHECK_ERROR("create shadowmap depth renderbuffers");
    #endif

    for (unsigned int fc = 0; fc < 6; ++fc) {
      #ifdef VV_SHADOWCUBE_DEPTH_TEXTURE
      vassert(cubeDepthTexId[fc]);
      glBindTexture(GL_TEXTURE_2D, cubeDepthTexId[fc]);
      GLDRW_CHECK_ERROR("bind shadowmap depth texture");
      aowner->p_glObjectLabelVA(GL_TEXTURE, cubeDepthTexId[fc], "ShadowCube depth texture #%u", fc);
      if (!useDefaultDepth) {
        //GCon->Logf(NAME_Debug, "OpenGL: setting up 16-bit depth texture");
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, shadowmapSize, shadowmapSize, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 0);
        if (glGetError()) {
          if (fc) recreateTextures = true;
          GCon->Logf(NAME_Warning, "OpenGL: cannot use 16-bit depth, reverting to default...");
          useDefaultDepth = true;
          glBindTexture(GL_TEXTURE_2D, 0);
          glDeleteTextures(1, &cubeDepthTexId[fc]);
          glGenTextures(1, &cubeDepthTexId[fc]);
          GLDRW_CHECK_ERROR("create shadowmap depth textures");
          glBindTexture(GL_TEXTURE_2D, cubeDepthTexId[fc]);
        }
      }
      if (ShadowCubeMap::useDefaultDepth) {
        //GCon->Logf(NAME_Debug, "OpenGL: setting up default depth texture");
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, shadowmapSize, shadowmapSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
        GLDRW_CHECK_ERROR("setup shadowmap depth textures");
      }
      /*
      GLDRW_CHECK_ERROR("initialize shadowmap depth texture");
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      GLDRW_CHECK_ERROR("set shadowmap depth texture min filter");
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      GLDRW_CHECK_ERROR("set shadowmap depth texture mag filter");
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      GLDRW_CHECK_ERROR("set shadowmap depth texture s");
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      GLDRW_CHECK_ERROR("set shadowmap depth texture t");
      */
      glBindTexture(GL_TEXTURE_2D, 0);
      GLDRW_CHECK_ERROR("unbind shadowmap depth texture");
      #else
      vassert(cubeDepthRBId[fc]);
      aowner->p_glBindRenderbuffer(GL_RENDERBUFFER, cubeDepthRBId[fc]);
      GLDRW_CHECK_ERROR("bind cubemap depth renderbuffer");
      aowner->p_glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, shadowmapSize, shadowmapSize);
      GLDRW_CHECK_ERROR("create cubemap depth renderbuffer storage");
        #ifndef GL4ES_HACKS
        // unbind the render buffer (this crashes GL4ES for some reason)
        aowner->p_glBindRenderbuffer(GL_RENDERBUFFER, 0);
        GLDRW_CHECK_ERROR("unbind cubemap depth renderbuffer");
        #endif
      #endif
    }

    glGenTextures(1, &cubeTexId);
    vassert(cubeTexId);
    GLDRW_CHECK_ERROR("create shadowmap cubemap");
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubeTexId);
    GLDRW_CHECK_ERROR("bind shadowmap cubemap");
    aowner->p_glObjectLabelVA(GL_TEXTURE, cubeTexId, "ShadowCube cubemap texture");

    /*
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    GLDRW_CHECK_ERROR("set shadowmap compare func");
    */
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    GLDRW_CHECK_ERROR("set shadowmap mag filter");
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    GLDRW_CHECK_ERROR("set shadowmap min filter");
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    GLDRW_CHECK_ERROR("set shadowmap wrap r");
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    GLDRW_CHECK_ERROR("set shadowmap wrap s");
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    GLDRW_CHECK_ERROR("set shadowmap wrap t");
    /*
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
    GLDRW_CHECK_ERROR("set shadowmap compare mode");
    */

    for (unsigned int fc = 0; fc < 6; ++fc) {
      //glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+fc, 0, GL_DEPTH_COMPONENT, shadowmapSize, shadowmapSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
      //glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+fc, 0, GL_R16F, shadowmapSize, shadowmapSize, 0, GL_RED, GL_FLOAT, 0);
      //!glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+fc, 0, GL_RGBA, shadowmapSize, shadowmapSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

      //glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+fc, 0, GL_R32F, shadowmapSize, shadowmapSize, 0, GL_RED, GL_FLOAT, 0);
      //glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+fc, 0, GL_RGB16F, shadowmapSize, shadowmapSize, 0, GL_RGB, GL_FLOAT, 0);
      if (afloat32) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+fc, 0, GL_R32F, shadowmapSize, shadowmapSize, 0, GL_RED, GL_FLOAT, 0);
      } else {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+fc, 0, GL_R16F, shadowmapSize, shadowmapSize, 0, GL_RED, GL_FLOAT, 0);
      }
      GLDRW_CHECK_ERROR("init cubemap texture");
    }

    if (!recreateTextures) break;
  }
}


//==========================================================================
//
//  VOpenGLDrawer::ShadowCubeMapData::destroyTextures
//
//==========================================================================
void VOpenGLDrawer::ShadowCubeMapData::destroyTextures () noexcept {
  VOpenGLDrawer *aowner = mOwner;
  mOwner = nullptr;
  if (cubeTexId) {
    vassert(aowner);

    glDeleteTextures(1, &cubeTexId);
    #ifdef VV_SHADOWCUBE_DEPTH_TEXTURE
    glDeleteTextures(6, &cubeDepthTexId[0]);
    #else
    aowner->p_glDeleteRenderbuffers(6, &cubeDepthRBId[0]);
    #endif

    cubeTexId = 0u;
    shift = 0u;
    float32 = false;
    #ifdef VV_SHADOWCUBE_DEPTH_TEXTURE
    memset((void *)&cubeDepthTexId[0], 0, sizeof(cubeDepthTexId));
    #else
    memset((void *)&cubeDepthRBId[0], 0, sizeof(cubeDepthRBId));
    #endif
  }
}



//**************************************************************************
//
//  VOpenGLDrawer::ShadowCubeMap
//
//**************************************************************************

//==========================================================================
//
//  VOpenGLDrawer::ShadowCubeMap::createCube
//
//==========================================================================
void VOpenGLDrawer::ShadowCubeMap::createCube (VOpenGLDrawer *aowner, const unsigned ashift, const bool afloat32) noexcept {
  destroyCube();
  createTextures(aowner, ashift, afloat32);
  if (!isValid()) { destroyCube(); return; }

  // create cubemap for shadowmapping
  aowner->p_glGenFramebuffers(1, &cubeFBO);
  GLDRW_CHECK_ERROR("create shadowmap FBO");
  vassert(cubeFBO);
  aowner->p_glBindFramebuffer(GL_FRAMEBUFFER, cubeFBO);
  GLDRW_CHECK_ERROR("bind shadowmap FBO");
  aowner->p_glObjectLabelVA(GL_FRAMEBUFFER, cubeFBO, "Shadowmap FBO");

  #ifdef VV_SHADOWCUBE_DEPTH_TEXTURE
  aowner->p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, cubeDepthTexId[0], 0);
  GLDRW_CHECK_ERROR("set shadowmap FBO depth texture");
  #else
  aowner->p_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, cubeDepthRBId[0]);
  GLDRW_CHECK_ERROR("set shadowmap FBO depth renderbuffer");
  #endif

  for (unsigned int fc = 0; fc < 6; ++fc) {
    aowner->p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X+fc, cubeTexId, 0);
    GLDRW_CHECK_ERROR("set shadowmap FBO face texture");
  }

  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  GLDRW_CHECK_ERROR("set cube FBO draw buffer");
  glReadBuffer(GL_NONE);
  GLDRW_CHECK_ERROR("set cube FBO read buffer");

  if (aowner->p_glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) Sys_Error("OpenGL: cannot initialise shadowmap FBO");
  aowner->p_glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


//==========================================================================
//
//  VOpenGLDrawer::ShadowCubeMap::destroyCube
//
//==========================================================================
void VOpenGLDrawer::ShadowCubeMap::destroyCube () noexcept {
  VOpenGLDrawer *aowner = mOwner;
  if (cubeFBO) {
    vassert(aowner);
    aowner->p_glBindFramebuffer(GL_FRAMEBUFFER, cubeFBO);
    //aowner->p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
    aowner->p_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
    aowner->p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    aowner->p_glBindFramebuffer(GL_FRAMEBUFFER, 0);
    aowner->p_glDeleteFramebuffers(1, &cubeFBO);
    cubeFBO = 0u;
  }
  destroyTextures();
  setAllDirty();
  smapCurrentFace = 0u;
}
