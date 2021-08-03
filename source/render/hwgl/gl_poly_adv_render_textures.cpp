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
#include "gl_local.h"
#include "gl_poly_adv_render.h"


//==========================================================================
//
//  VOpenGLDrawer::DrawWorldTexturesPass
//
//  this renders textured level with ambient lighting applied
//  this is for advanced renderer only
//  depth mask should be off
//
//==========================================================================
void VOpenGLDrawer::DrawWorldTexturesPass () {
  if (gl_dbg_wireframe) return;

  if (r_decals && RendLev->/*PortalDepth*/PortalUsingStencil == 0) {
    // stop stenciling now
    glDisable(GL_STENCIL_TEST);
    //glDepthMask(GL_FALSE); // no z-buffer writes
    GLDisableDepthWrite();
    glEnable(GL_TEXTURE_2D);
    //p_glBlendEquation(GL_FUNC_ADD);

    auto mfbo = GetMainFBO();

    // allocate ambient light FBO object if necessary
    if (!ambLightFBO.isValid() ||
        ambLightFBO.getWidth() != mfbo->getWidth() ||
        ambLightFBO.getHeight() != mfbo->getHeight() ||
        ambLightFBO.getGLType() != mfbo->getGLType())
    {
      ambLightFBO.destroy(); // it is safe to destroy non-valid FBOs
      ambLightFBO.createTextureOnly(this, mfbo->getWidth(), mfbo->getHeight(), mfbo->isColorFloat());
      p_glObjectLabelVA(GL_FRAMEBUFFER, ambLightFBO.getFBOid(), "Ambient light FBO");
    }

    // copy ambient light texture to FBO, so we can use it to light decals
    mfbo->blitTo(&ambLightFBO, 0, 0, mfbo->getWidth(), mfbo->getHeight(), 0, 0, ambLightFBO.getWidth(), ambLightFBO.getHeight(), GL_NEAREST);
    mfbo->activate();
  }

  //glDepthMask(GL_FALSE); // no z-buffer writes
  GLDisableDepthWrite();
  glEnable(GL_TEXTURE_2D);
  // turn off scissoring only if we aren't rendering portal contents
  DisableStenciling();
  DisableScissoring();
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  GLDisableOffset();
  glEnable(GL_CULL_FACE);
  RestoreDepthFunc();


  glBlendFunc(GL_DST_COLOR, GL_ZERO);
  GLEnableBlend();

  if (!gl_dbg_adv_render_surface_textures) return;

  VRenderLevelDrawer::DrawLists &dls = RendLev->GetCurrentDLS();
  if (dls.DrawSurfListSolid.length() == 0 && dls.DrawSurfListMasked.length() == 0) return;

  ShadowsTextureMasked.Activate();
  ShadowsTextureMasked.SetTexture(0);

  ShadowsTexture.Activate();
  ShadowsTexture.SetTexture(0);

  //GLDisableBlend();

  // sort surfaces with solid textures, because here we need them fully sorted
  //timsort_r(dls.DrawSurfListSolid.ptr(), dls.DrawSurfListSolid.length(), sizeof(surface_t *), &glAdvRenderDrawListItemCmpByShaderTexture, nullptr);

  // sort all surfaces by texture only, it is faster this way
  // this also sorts by fade, so we can avoid resorting in fog pass
  timsort_r(dls.DrawSurfListMasked.ptr(), dls.DrawSurfListMasked.length(), sizeof(surface_t *), &glAdvRenderDrawListItemCmpByTextureAndFade, nullptr);
  timsort_r(dls.DrawSurfListSolid.ptr(), dls.DrawSurfListSolid.length(), sizeof(surface_t *), &glAdvRenderDrawListItemCmpByTextureAndFade, nullptr);

  texinfo_t lastTexinfo;
  lastTexinfo.initLastUsed();

  bool lastCullFace = true;
  glEnable(GL_CULL_FACE);

  // activate VBO
  vboAdvSurf.activate();
  GLuint attribPosition = 0; /* shut up, gcc! */

  int vboCountIdx = 0; // element (counter) index

  //WARNING! don't forget to flush VBO on each shader uniform change! this includes glow changes (glow values aren't cached yet)

  const bool decalsEnabled = (r_decals.asBool() && RendLev->PortalUsingStencil == 0);

  // normal
  if (dls.DrawSurfListSolid.length() != 0) {
    lastTexinfo.resetLastUsed();
    ShadowsTexture.Activate();
    attribPosition = ShadowsAmbient.loc_Position;
    vboAdvSurf.enableAttrib(attribPosition);

    for (auto &&surf : dls.DrawSurfListSolid) {
      SADV_CHECK_TEXTURE(ShadowsTexture);

      const bool doDecals = (decalsEnabled && RenderSurfaceHasDecals(surf));

      // fill stencil buffer for decals
      if (doDecals) {
        SADV_FLUSH_VBO();
        RenderPrepareShaderDecals(surf);
      }

      SADV_DO_RENDER();

      if (doDecals) {
        SADV_FLUSH_VBO();
        vboAdvSurf.disableAttrib(attribPosition);
        vboAdvSurf.deactivate();
        if (RenderFinishShaderDecals(DT_ADVANCED, surf, nullptr, currTexinfo->ColorMap)) {
          ShadowsTexture.Activate();
          glBlendFunc(GL_DST_COLOR, GL_ZERO);
          //GLEnableBlend();
          lastTexinfo.resetLastUsed(); // resetup texture
          lastCullFace = true; // changed by decal renderer
        }
        vboAdvSurf.activate();
        vboAdvSurf.enableAttrib(attribPosition);
      }
    }

    SADV_FLUSH_VBO();
    vboAdvSurf.disableAttrib(attribPosition);
  }

  // masked
  if (dls.DrawSurfListMasked.length() != 0) {
    lastTexinfo.resetLastUsed();
    ShadowsTextureMasked.Activate();
    attribPosition = ShadowsTextureMasked.loc_Position;
    vboAdvSurf.enableAttrib(attribPosition);

    for (auto &&surf : dls.DrawSurfListMasked) {
      SADV_CHECK_TEXTURE(ShadowsTextureMasked);

      const bool doDecals = (decalsEnabled && RenderSurfaceHasDecals(surf));

      // fill stencil buffer for decals
      if (doDecals) {
        SADV_FLUSH_VBO();
        RenderPrepareShaderDecals(surf);
      }

      SADV_DO_RENDER();

      if (doDecals) {
        SADV_FLUSH_VBO();
        vboAdvSurf.disableAttrib(attribPosition);
        vboAdvSurf.deactivate();
        if (RenderFinishShaderDecals(DT_ADVANCED, surf, nullptr, currTexinfo->ColorMap)) {
          ShadowsTextureMasked.Activate();
          glBlendFunc(GL_DST_COLOR, GL_ZERO);
          //GLEnableBlend();
          lastTexinfo.resetLastUsed(); // resetup texture
          lastCullFace = true; // changed by decal renderer
        }
        vboAdvSurf.activate();
        vboAdvSurf.enableAttrib(attribPosition);
      }
    }

    SADV_FLUSH_VBO();
    vboAdvSurf.disableAttrib(attribPosition);
  }

  vboAdvSurf.deactivate();
}
