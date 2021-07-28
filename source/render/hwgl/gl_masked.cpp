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


//==========================================================================
//
//  VOpenGLDrawer::DrawMaskedPolygon
//
//  used only to render translucent walls
//
//==========================================================================
void VOpenGLDrawer::DrawMaskedPolygon (surface_t *surf, float Alpha, bool Additive, bool DepthWrite, bool onlyTranslucent) {
  if (!surf->IsPlVisible()) return; // viewer is in back side or on plane
  if (surf->count < 3 || Alpha < 0.004f) return;

  texinfo_t *tex = surf->texinfo;
  if (!tex->Tex || tex->Tex->Type == TEXTYPE_Null) return;
  if (Alpha > 1.0f) Alpha = 1.0f; // just in case

  if (onlyTranslucent && (Additive || Alpha < 1.0f)) onlyTranslucent = false; // just in case

  GlowParams gp;
  CalcGlow(gp, surf);

  SetCommonTexture(tex->Tex, tex->ColorMap);

  const bool doBrightmap = (r_brightmaps && tex->Tex->Brightmap);
  const bool isAlphaTrans = (Alpha < 1.0f || tex->Tex->isTranslucent());
  //k8: non-translucent walls should not end here, so there is no need to recalc/check lightmaps
  const float lightLevel = getSurfLightLevel(surf);
  const bool zbufferWriteDisabled = (!DepthWrite || Additive || Alpha < 1.0f); // translucent things should not modify z-buffer

  const float globVis = R_CalcGlobVis();
  const int lightMode = (surf->Fade == FADE_LIGHT ? r_light_mode.asInt() : 0);
  const bool lightWithFog = (surf->Fade != FADE_LIGHT || r_light_mode.asInt() <= 0);

  bool doDecals = RenderSurfaceHasDecals(surf);

  if (doDecals) {
    if (Additive || isAlphaTrans) {
      if (!r_decals_wall_alpha) doDecals = false;
    } else {
      if (!r_decals_wall_masked) doDecals = false;
    }
  }

  GLuint attribPosition;

  if (doBrightmap) {
    SurfMaskedPolyBrightmapGlow.Activate();
    SurfMaskedPolyBrightmapGlow.SetBrightMapAdditive(r_brightmaps_additive ? 1.0f : 0.0f);
    SurfMaskedPolyBrightmapGlow.SetTexture(0);
    SurfMaskedPolyBrightmapGlow.SetTextureBM(1);
    SelectTexture(1);
    SetBrightmapTexture(tex->Tex->Brightmap);
    SelectTexture(0);
    if (gp.isActive()) {
      VV_GLDRAWER_ACTIVATE_GLOW(SurfMaskedPolyBrightmapGlow, gp);
    } else {
      VV_GLDRAWER_DEACTIVATE_GLOW(SurfMaskedPolyBrightmapGlow);
    }
    //SurfMaskedPolyBrightmapGlow.SetAlphaRef(Additive || isAlphaTrans ? getAlphaThreshold() : 0.666f);
    // this should be a different shader, but meh
    SurfMaskedPolyBrightmapGlow.SetAlphaRef(onlyTranslucent ? -1.0f : (Additive || isAlphaTrans ? getAlphaThreshold() : 0.666f));

    SurfMaskedPolyBrightmapGlow.SetLightGlobVis(globVis);
    SurfMaskedPolyBrightmapGlow.SetLightMode(lightMode);
    SurfMaskedPolyBrightmapGlow.SetAlpha(Alpha);

    SurfMaskedPolyBrightmapGlow.SetLight(
      ((surf->Light>>16)&255)*255.0f,
      ((surf->Light>>8)&255)*255.0f,
      (surf->Light&255)*255.0f, lightLevel);
    /*
      SurfMaskedPolyBrightmapGlow.SetLight(
        ((surf->Light>>16)&255)*lightLevel/255.0f,
        ((surf->Light>>8)&255)*lightLevel/255.0f,
        (surf->Light&255)*lightLevel/255.0f, -1.0f);
    */
    SurfMaskedPolyBrightmapGlow.SetFogFade((lightWithFog ? surf->Fade : 0), Alpha);
    SurfMaskedPolyBrightmapGlow.SetSpriteTex(TVec(tex->soffs, tex->toffs), tex->saxis, tex->taxis, tex_iw, tex_ih);
    attribPosition = SurfMaskedPolyBrightmapGlow.loc_Position;
  } else {
    SurfMaskedPolyGlow.Activate();
    SurfMaskedPolyGlow.SetTexture(0);
    //SurfMaskedPolyGlow.SetFogType();
    if (gp.isActive()) {
      VV_GLDRAWER_ACTIVATE_GLOW(SurfMaskedPolyGlow, gp);
    } else {
      VV_GLDRAWER_DEACTIVATE_GLOW(SurfMaskedPolyGlow);
    }
    //SurfMaskedPolyGlow.SetAlphaRef(Additive || isAlphaTrans ? getAlphaThreshold() : 0.666f);
    // this should be a different shader, but meh
    SurfMaskedPolyGlow.SetAlphaRef(onlyTranslucent ? -1.0f : (Additive || isAlphaTrans ? getAlphaThreshold() : 0.666f));

    SurfMaskedPolyGlow.SetLightGlobVis(globVis);
    SurfMaskedPolyGlow.SetLightMode(lightMode);
    SurfMaskedPolyGlow.SetAlpha(Alpha);
    SurfMaskedPolyGlow.SetLight(
      ((surf->Light>>16)&255)*255.0f,
      ((surf->Light>>8)&255)*255.0f,
      (surf->Light&255)*255.0f, lightLevel);
    /*
      SurfMaskedPolyGlow.SetLight(
        ((surf->Light>>16)&255)*lightLevel/255.0f,
        ((surf->Light>>8)&255)*lightLevel/255.0f,
        (surf->Light&255)*lightLevel/255.0f, -1.0f);
    */
    SurfMaskedPolyGlow.SetFogFade((lightWithFog ? surf->Fade : 0), Alpha);
    SurfMaskedPolyGlow.SetSpriteTex(TVec(tex->soffs, tex->toffs), tex->saxis, tex->taxis, tex_iw, tex_ih);
    attribPosition = SurfMaskedPolyGlow.loc_Position;
  }

  if (Additive) glBlendFunc(GL_ONE, GL_ONE); // our source rgb is already premultiplied

  if (zbufferWriteDisabled) {
    PushDepthMask();
    //glDepthMask(GL_FALSE); // no z-buffer writes
    GLDisableDepthWrite();
  }

  // fill stencil buffer for decals
  if (doDecals) RenderPrepareShaderDecals(surf);

  currentActiveShader->UploadChangedUniforms();
  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);

  // this also activates VBO
  vboMaskedSurf.uploadBuffer(surf->count, surf->verts);

  vboMaskedSurf.setupAttrib(attribPosition, 3);
  p_glDrawArrays(GL_TRIANGLE_FAN, 0, surf->count);
  vboMaskedSurf.disableAttrib(attribPosition);
  vboMaskedSurf.deactivate();

  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glEnable(GL_CULL_FACE);

  // draw decals
  if (doDecals) {
    if (RenderFinishShaderDecals(DT_SIMPLE, surf, nullptr, tex->ColorMap)) {
      Additive = true; // restore blending
    }
  }

  if (zbufferWriteDisabled) PopDepthMask();
  if (Additive) glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  if (doBrightmap) {
    SelectTexture(1);
    glBindTexture(GL_TEXTURE_2D, 0);
    SelectTexture(0);
  }
}


//==========================================================================
//
//  VOpenGLDrawer::DrawSpritePolygon
//
//  WARNING! it may modify `cv`!
//
//==========================================================================
void VOpenGLDrawer::DrawSpritePolygon (float time, const TVec *cv, VTexture *Tex,
                                       const RenderStyleInfo &ri,
                                       VTextureTranslation *Translation, int CMap,
                                       const TVec &/*sprnormal*/, float /*sprpdist*/,
                                       const TVec &saxis, const TVec &taxis, const TVec &texorg,
                                       SpriteType sptype)
{
  if (!Tex || Tex->Type == TEXTYPE_Null) return; // just in case

  enum ShaderType {
    ShaderMasked,
    ShaderMaskedBM,
    ShaderStencil,
    ShaderFakeShadow,
    ShaderFuzzy,
  };

  // ignore translucent textures here: some idiots are trying to make "smoothed" sprites in this manner
  bool resetDepthMask = (ri.alpha < 1.0f || ri.isAdditive()); // `true` means "depth write disabled"
  ShaderType shadtype = ShaderMasked;
  if (ri.isStenciled()) {
    shadtype = (ri.stencilColor&0x00ffffffu ? ShaderStencil : ShaderFakeShadow);
    if (ri.translucency != RenderStyleInfo::Normal && ri.translucency != RenderStyleInfo::Translucent) resetDepthMask = true;
  } else {
    if (r_brightmaps && r_brightmaps_sprite && Tex->Brightmap) shadtype = ShaderMaskedBM;
    if (ri.isTranslucent()) resetDepthMask = true;
    if (ri.translucency == RenderStyleInfo::Fuzzy) shadtype = ShaderFuzzy;
  }

  //const float globVis = R_CalcGlobVis();

  const bool trans = (ri.translucency || ri.alpha < 1.0f || Tex->isTranslucent());
  SetSpriteTexture(sptype, sprite_filter, Tex, Translation, CMap, (ri.isShaded() ? ri.stencilColor : 0u));

  const bool fogAllowed = (ri.fade != FADE_LIGHT || r_light_mode.asInt() <= 0);
  //const float lightMode = (surf->Fade == FADE_LIGHT ? (float)r_light_mode.asInt() : 0.0f);

  GLuint attribPosition;
  switch (shadtype) {
    case ShaderMasked:
      SurfMasked.Activate();
      SurfMasked.SetTexture(0);
      /*
      SurfMasked.SetLightGlobVis(globVis);
      SurfMasked.SetLightMode(lightMode);
      SurfMasked.SetAlpha(ri.alpha);
      SurfMasked.SetLight(
        ((ri.light>>16)&255)/255.0f,
        ((ri.light>>8)&255)/255.0f,
        (ri.light&255)/255.0f, ((ri.light>>24)&0xff)/255.0f);
      */
      SurfMasked.SetLight(
        ((ri.light>>16)&255)/255.0f,
        ((ri.light>>8)&255)/255.0f,
        (ri.light&255)/255.0f, ri.alpha);
      SurfMasked.SetFogFade((fogAllowed ? ri.fade : 0), ri.alpha);
      SurfMasked.SetAlphaRef(trans ? getAlphaThreshold() : 0.666f);
      SurfMasked.SetSpriteTex(texorg, saxis, taxis, tex_iw, tex_ih);
      attribPosition = SurfMasked.loc_Position;
      break;
    case ShaderMaskedBM:
      SurfMaskedBrightmap.Activate();
      SurfMaskedBrightmap.SetBrightMapAdditive(r_brightmaps_additive ? 1.0f : 0.0f);
      SurfMaskedBrightmap.SetTexture(0);
      SurfMaskedBrightmap.SetTextureBM(1);
      SelectTexture(1);
      SetSpriteBrightmapTexture(sptype, Tex->Brightmap);
      SelectTexture(0);
      /*
      SurfMaskedBrightmap.SetLightGlobVis(globVis);
      SurfMaskedBrightmap.SetLightMode(lightMode);
      SurfMaskedBrightmap.SetAlpha(ri.alpha);
      SurfMaskedBrightmap.SetLight(
        ((ri.light>>16)&255)/255.0f,
        ((ri.light>>8)&255)/255.0f,
        (ri.light&255)/255.0f, ((ri.light>>24)&0xff)/255.0f);
      */
      SurfMaskedBrightmap.SetLight(
        ((ri.light>>16)&255)/255.0f,
        ((ri.light>>8)&255)/255.0f,
        (ri.light&255)/255.0f, ri.alpha);
      SurfMaskedBrightmap.SetFogFade((fogAllowed ? ri.fade : 0), ri.alpha);
      SurfMaskedBrightmap.SetAlphaRef(trans ? getAlphaThreshold() : 0.666f);
      SurfMaskedBrightmap.SetSpriteTex(texorg, saxis, taxis, tex_iw, tex_ih);
      attribPosition = SurfMaskedBrightmap.loc_Position;
      break;
    case ShaderStencil:
      SurfMaskedStencil.Activate();
      SurfMaskedStencil.SetTexture(0);
      SurfMaskedStencil.SetStencilColor(
        ((ri.stencilColor>>16)&255)/255.0f,
        ((ri.stencilColor>>8)&255)/255.0f,
        (ri.stencilColor&255)/255.0f);
      /*
      SurfMaskedStencil.SetLightGlobVis(globVis);
      SurfMaskedStencil.SetLightMode(lightMode);
      SurfMaskedStencil.SetAlpha(ri.alpha);
      SurfMaskedStencil.SetLight(
        ((ri.light>>16)&255)/255.0f,
        ((ri.light>>8)&255)/255.0f,
        (ri.light&255)/255.0f, ((ri.light>>24)&0xff)/255.0f);
      */
      SurfMaskedStencil.SetLight(
        ((ri.light>>16)&255)/255.0f,
        ((ri.light>>8)&255)/255.0f,
        (ri.light&255)/255.0f, ri.alpha);
      SurfMaskedStencil.SetFogFade((fogAllowed ? ri.fade : 0), ri.alpha);
      SurfMaskedStencil.SetAlphaRef(trans ? getAlphaThreshold() : 0.666f);
      SurfMaskedStencil.SetSpriteTex(texorg, saxis, taxis, tex_iw, tex_ih);
      attribPosition = SurfMaskedStencil.loc_Position;
      break;
    case ShaderFakeShadow:
      SurfMaskedFakeShadow.Activate();
      SurfMaskedFakeShadow.SetTexture(0);
      /*
      SurfMaskedFakeShadow.SetLightGlobVis(globVis);
      SurfMaskedFakeShadow.SetLightMode(lightMode);
      SurfMaskedFakeShadow.SetAlpha(ri.alpha);
      SurfMaskedFakeShadow.SetLight(
        ((ri.light>>16)&255)/255.0f,
        ((ri.light>>8)&255)/255.0f,
        (ri.light&255)/255.0f, ((ri.light>>24)&0xff)/255.0f);
      */
      SurfMaskedFakeShadow.SetLight(
        ((ri.light>>16)&255)/255.0f,
        ((ri.light>>8)&255)/255.0f,
        (ri.light&255)/255.0f, ri.alpha);
      SurfMaskedFakeShadow.SetFogFade((fogAllowed ? ri.fade : 0), ri.alpha);
      SurfMaskedFakeShadow.SetAlphaRef(trans ? getAlphaThreshold() : 0.666f);
      SurfMaskedFakeShadow.SetSpriteTex(texorg, saxis, taxis, tex_iw, tex_ih);
      attribPosition = SurfMaskedFakeShadow.loc_Position;
      break;
    case ShaderFuzzy:
      SurfMaskedFuzzy.Activate();
      SurfMaskedFuzzy.SetTexture(0);
      /*
      SurfMaskedFuzzy.SetLight(
        ((ri.light>>16)&255)/255.0f,
        ((ri.light>>8)&255)/255.0f,
        (ri.light&255)/255.0f, ri.alpha);
      */
      SurfMaskedFuzzy.SetTime(time);
      //FIXME: lighting!
      SurfMaskedFuzzy.SetFogFade((fogAllowed ? ri.fade : 0), ri.alpha);
      SurfMaskedFuzzy.SetAlphaRef(trans ? getAlphaThreshold() : 0.666f);
      SurfMaskedFuzzy.SetSpriteTex(texorg, saxis, taxis, tex_iw, tex_ih);
      attribPosition = SurfMaskedFuzzy.loc_Position;
      break;
    default: Sys_Error("ketmar forgot some shader type in `VOpenGLDrawer::DrawSpritePolygon()`");
  }
  currentActiveShader->UploadChangedUniforms();

  // setup hangups
  if (ri.flags) {
    // no z-buffer?
    if (ri.flags&RenderStyleInfo::FlagNoDepthWrite) resetDepthMask = true;
    // offset?
    if (ri.flags&RenderStyleInfo::FlagOffset) {
      const float updir = (!CanUseRevZ() ? -1.0f : 1.0f);
      GLPolygonOffset(updir, updir);
    }
    // no cull?
    if (ri.flags&RenderStyleInfo::FlagNoCull) glDisable(GL_CULL_FACE);
  }

  // translucent things should not modify z-buffer
  // no
  /*
  if (!resetDepthMask && trans) {
    resetDepthMask = true;
  }
  */

  SetupBlending(ri);

  if (resetDepthMask) {
    PushDepthMask();
    //glDepthMask(GL_FALSE); // no z-buffer writes
    GLDisableDepthWrite();
  }

  // this also activates VBO
  vboSprite.uploadBuffer(4, cv);

  vboSprite.setupAttrib(attribPosition, 3);
  #if 0
  const vuint8 indices[6] = { 0, 1, 2,  0, 2, 3 };
  p_glDrawRangeElements(GL_TRIANGLES, 0, 5, 6, GL_UNSIGNED_BYTE, indices);
  #else
  p_glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
  #endif
  vboSprite.disableAttrib(attribPosition);
  vboSprite.deactivate();

  if (resetDepthMask) PopDepthMask();
  if (ri.flags&RenderStyleInfo::FlagOffset) GLDisableOffset();
  if (ri.flags&RenderStyleInfo::FlagNoCull) glEnable(GL_CULL_FACE);
  RestoreBlending(ri);

  if (shadtype == ShaderMaskedBM) {
    SelectTexture(1);
    glBindTexture(GL_TEXTURE_2D, 0);
    SelectTexture(0);
  }
}
