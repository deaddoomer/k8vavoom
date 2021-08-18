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


VCvarB gl_regular_prefill_depth("gl_regular_prefill_depth", false, "Prefill depth buffer for regular renderer?", CVAR_Archive);

extern VCvarB r_lmap_overbright;
extern VCvarF r_overbright_specular;


// ////////////////////////////////////////////////////////////////////////// //
static inline int compareSurfaces (const surface_t *sa, const surface_t *sb) {
  if (sa == sb) return 0;
  const texinfo_t *ta = sa->texinfo;
  const texinfo_t *tb = sb->texinfo;
  // surfaces without textures should float up
  if (!ta->Tex) {
    return (!tb->Tex ? 0 : -1);
  } else if (!tb->Tex) {
    return 1;
  }
  // brightmapped textures comes last
  if (r_brightmaps) {
    if (ta->Tex->Brightmap) {
      if (!tb->Tex->Brightmap) return 1;
    } else if (tb->Tex->Brightmap) {
      return -1;
    }
  }
  // sort by texture id (just use texture pointer)
  if ((uintptr_t)ta->Tex < (uintptr_t)ta->Tex) return -1;
  if ((uintptr_t)tb->Tex > (uintptr_t)tb->Tex) return 1;
  // by light level/color
  if (sa->Light < sb->Light) return -1;
  if (sa->Light > sb->Light) return 1;
  // and by colormap, why not?
  return ((int)ta->ColorMap)-((int)tb->ColorMap);
}

static int surfListItemCmp (const void *a, const void *b, void * /*udata*/) {
  return compareSurfaces(((const VOpenGLDrawer::SurfListItem *)a)->surf, ((const VOpenGLDrawer::SurfListItem *)b)->surf);
}

static int drawListItemCmp (const void *a, const void *b, void * /*udata*/) {
  return compareSurfaces(*(const surface_t **)a, *(const surface_t **)b);
}


//==========================================================================
//
//  VOpenGLDrawer::StartSkyPolygons
//
//  used in r_sky
//
//==========================================================================
void VOpenGLDrawer::StartSkyPolygons () {
  //SetFade(0);
  //glDisable(GL_CULL_FACE);
}


//==========================================================================
//
//  VOpenGLDrawer::EndSkyPolygons
//
//  used in r_sky
//
//==========================================================================
void VOpenGLDrawer::EndSkyPolygons () {
  //SetFade(0); // disable fog
  //glEnable(GL_CULL_FACE);
}


//==========================================================================
//
//  VOpenGLDrawer::DrawSkyPolygon
//
//  used in r_sky
//
//==========================================================================
void VOpenGLDrawer::DrawSkyPolygon (surface_t *surf, bool bIsSkyBox, VTexture *Texture1,
                                    float offs1, VTexture *Texture2, float offs2, int CMap)
{
  int sidx[4];

  if (!surf || surf->count < 3) return;
  if (!Texture1) Texture1 = Texture2; // the thing that should not happen, but...

  //SetFade(surf->Fade);
  sidx[0] = 0;
  sidx[1] = 1;
  sidx[2] = 2;
  sidx[3] = 3;

  if (!bIsSkyBox) {
    if (surf->verts[1].z > 0) {
      sidx[1] = 0;
      sidx[2] = 3;
    } else {
      sidx[0] = 1;
      sidx[3] = 2;
    }
  }

  const texinfo_t *tex = surf->texinfo;

  if (Texture2 && Texture2->Type != TEXTYPE_Null) {
    SetCommonTexture(Texture1, CMap);
    SelectTexture(1);
    SetCommonTexture(Texture2, CMap);
    SelectTexture(0);

    SurfDSky.Activate();
    SurfDSky.SetTexture(0);
    SurfDSky.SetTexture2(1);
    SurfDSky.SetBrightness(r_sky_bright_factor);
    SurfDSky.SetFogFade(surf->Fade, 1.0f);
    SurfDSky.UploadChangedUniforms();

    //glBegin(GL_POLYGON);
    glBegin(GL_TRIANGLE_FAN);
    for (unsigned i = 0; i < (unsigned)surf->count; ++i) {
      float s1, s2;
      CalcSkyTexCoordS2(&s1, &s2, surf->verts[sidx[i]].vec(), tex, offs1, offs2);
      const float t = CalcSkyTexCoordT(surf->verts[i].vec(), tex);
      SurfDSky.SetTexCoordAttr(s1, t);
        /*
        (tex->saxis.dot(surf->verts[sidx[i]].vec())+tex->soffs-offs1)*tex_iw,
        (tex->taxis.dot(surf->verts[i].vec())+tex->toffs)*tex_ih);
        */
      SurfDSky.SetTexCoord2Attr(s2, t);
        /*
        (tex->saxis.dot(surf->verts[sidx[i]].vec())+tex->soffs-offs2)*tex_iw,
        (tex->taxis.dot(surf->verts[i].vec())+tex->toffs)*tex_ih);
        */
      //SurfDSky.UploadChangedAttrs();
      glVertex(surf->verts[i].vec());
    }
    glEnd();
  } else if (Texture1 && Texture1->Type != TEXTYPE_Null) {
    SetCommonTexture(Texture1, CMap);

    SurfSky.Activate();
    SurfSky.SetTexture(0);
    SurfSky.SetBrightness(r_sky_bright_factor);
    //SurfSky.SetTexSky(tex, offs1, 0, false);
    SurfSky.SetFogFade(surf->Fade, 1.0f);
    SurfSky.UploadChangedUniforms();

    vboSky.ensureDataSize(surf->count);

    // copy vertices to VBO
    const unsigned scount = (unsigned)surf->count;
    const SurfVertex *svt = surf->verts;
    for (unsigned i = 0; i < scount; ++i, ++svt) {
      SkyVBOVertex *vbovtx = vboSky.allocPtr();
      vbovtx->x = svt->x;
      vbovtx->y = svt->y;
      vbovtx->z = svt->z;
      vbovtx->s = CalcSkyTexCoordS(surf->verts[sidx[i]].vec(), tex, offs1);
      vbovtx->t = CalcSkyTexCoordT(svt->vec(), tex);
    }

    vboSky.uploadData();

    vboSky.setupAttrib(SurfSky.loc_Position, 3);
    vboSky.setupAttrib(SurfSky.loc_TexCoord, 2, (ptrdiff_t)(3*sizeof(float)));
    p_glDrawArrays(GL_TRIANGLE_FAN, 0, surf->count);
    vboSky.disableAttrib(SurfSky.loc_Position);
    vboSky.disableAttrib(SurfSky.loc_TexCoord);

    vboSky.deactivate();
  }
}


//==========================================================================
//
//  VOpenGLDrawer::DoHorizonPolygon
//
//==========================================================================
void VOpenGLDrawer::DoHorizonPolygon (surface_t *surf) {
  if (surf->count < 3) return;

  const float Dist = 4096.0f;
  TVec v[4];
  if (surf->HorizonPlane->normal.z > 0.0f) {
    v[0] = surf->verts[0].vec();
    v[3] = surf->verts[3].vec();
    const TVec HDir = -surf->GetNormal();

    const TVec Dir1 = (vieworg-surf->verts[1].vec()).normalise();
    const TVec Dir2 = (vieworg-surf->verts[2].vec()).normalise();
    float Mul1 = 1.0f/HDir.dot(Dir1);
    v[1] = surf->verts[1].vec()+Dir1*Mul1*Dist;
    const float Mul2 = 1.0f/HDir.dot(Dir2);
    v[2] = surf->verts[2].vec()+Dir2*Mul2*Dist;
    if (v[1].z < v[0].z) {
      v[1] = surf->verts[1].vec()+Dir1*Mul1*Dist*(surf->verts[1].z-surf->verts[0].z)/(surf->verts[1].z-v[1].z);
      v[2] = surf->verts[2].vec()+Dir2*Mul2*Dist*(surf->verts[2].z-surf->verts[3].z)/(surf->verts[2].z-v[2].z);
    }
  } else {
    v[1] = surf->verts[1].vec();
    v[2] = surf->verts[2].vec();
    const TVec HDir = -surf->GetNormal();

    const TVec Dir1 = (vieworg-surf->verts[0].vec()).normalise();
    const TVec Dir2 = (vieworg-surf->verts[3].vec()).normalise();
    const float Mul1 = 1.0f/HDir.dot(Dir1);
    v[0] = surf->verts[0].vec()+Dir1*Mul1*Dist;
    const float Mul2 = 1.0f/HDir.dot(Dir2);
    v[3] = surf->verts[3].vec()+Dir2*Mul2*Dist;
    if (v[1].z < v[0].z) {
      v[0] = surf->verts[0].vec()+Dir1*Mul1*Dist*(surf->verts[1].z-surf->verts[0].z)/(v[0].z-surf->verts[0].z);
      v[3] = surf->verts[3].vec()+Dir2*Mul2*Dist*(surf->verts[2].z-surf->verts[3].z)/(v[3].z-surf->verts[3].z);
    }
  }

  texinfo_t *Tex = surf->texinfo;
  SetCommonTexture(Tex->Tex, Tex->ColorMap);

  SurfSimple.Activate();
  SurfSimple.SetTexture(0);
  //SurfSimple_Locs.storeFogType();
  SurfSimple.SetTex(Tex);
  VV_GLDRAWER_DEACTIVATE_GLOW(SurfSimple);

  const float globVis = R_CalcGlobVis();
  const bool fogAllowed = (surf->Fade != FADE_LIGHT || r_light_mode.asInt() <= 0);
  const int lightMode = (surf->Fade == FADE_LIGHT ? r_light_mode.asInt() : 0);
  const float lev = getSurfLightLevel(surf);
  //SurfSimple.SetLight(((surf->Light>>16)&255)*lev/255.0f, ((surf->Light>>8)&255)*lev/255.0f, (surf->Light&255)*lev/255.0f, 1.0f);
  SurfSimple.SetLightGlobVis(globVis);
  SurfSimple.SetLightMode(lightMode);
  SurfSimple.SetLight(((surf->Light>>16)&255)/255.0f, ((surf->Light>>8)&255)/255.0f, (surf->Light&255)/255.0f, lev);
  SurfSimple.SetFogFade((fogAllowed ? surf->Fade : 0), 1.0f);

  // draw it
  /*
  GLint oldDepthMask;
  glGetIntegerv(GL_DEPTH_WRITEMASK, &oldDepthMask);
  glDepthMask(GL_FALSE); // no z-buffer writes
  */
  PushDepthMask();
  GLDisableDepthWrite();

  //glBegin(GL_POLYGON);
  SurfSimple.UploadChangedUniforms();
  glBegin(GL_TRIANGLE_FAN);
    for (unsigned i = 0; i < 4; ++i) glVertex(v[i]);
  glEnd();
  //glDepthMask(GL_TRUE); // allow z-buffer writes
  //glDepthMask(oldDepthMask);
  PopDepthMask();

  // write to the depth buffer
  SurfZBuf.Activate();
  SurfZBuf.UploadChangedUniforms();
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  //glBegin(GL_POLYGON);
  glBegin(GL_TRIANGLE_FAN);
    for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i].vec());
  glEnd();
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}


//==========================================================================
//
//  VOpenGLDrawer::BeforeDrawWorldLMap
//
//  populate VBO with world surfaces
//
//==========================================================================
void VOpenGLDrawer::BeforeDrawWorldLMap () {
}


//==========================================================================
//
//  VOpenGLDrawer::RenderSimpleSurface
//
//  returns `true` if we need to re-setup texture
//
//==========================================================================
bool VOpenGLDrawer::RenderSimpleSurface (bool textureChanged, surface_t *surf) {
  const texinfo_t *tex = surf->texinfo;

  bool doBrightmap = (r_brightmaps && tex->Tex->Brightmap);

  GlowParams gp;
  CalcGlow(gp, surf);

  /*
  if ((surf->drawflags&surface_t::DF_MASKED) != 0) {
    GCon->Logf(NAME_Debug, "tex:%p:%s: saxis=(%g,%g,%g); taxis=(%g,%g,%g); saxisLM=(%g,%g,%g); taxisLM=(%g,%g,%g); min=(%d,%d); ext=(%d,%d)", tex, *tex->Tex->Name, tex->saxis.x, tex->saxis.y, tex->saxis.z, tex->taxis.x, tex->taxis.y, tex->taxis.z, tex->saxisLM.x, tex->saxisLM.y, tex->saxisLM.z, tex->taxisLM.x, tex->taxisLM.y, tex->taxisLM.z, surf->texturemins[0], surf->texturemins[1], surf->extents[0], surf->extents[1]);
  }
  */

  if (textureChanged) {
    if (doBrightmap) {
      SurfSimpleBrightmap.Activate();
      SurfSimpleBrightmap.SetBrightMapAdditive(r_brightmaps_additive ? 1.0f : 0.0f);
      SurfSimpleBrightmap.SetTexture(0);
      SurfSimpleBrightmap.SetTextureBM(1);
      SelectTexture(1);
      SetBrightmapTexture(tex->Tex->Brightmap);
      SelectTexture(0);
      SetCommonTexture(tex->Tex, tex->ColorMap);
      SurfSimpleBrightmap.SetTex(tex);
    } else {
      SetCommonTexture(tex->Tex, tex->ColorMap);
      if ((surf->drawflags&surface_t::DF_MASKED) == 0) {
        SurfSimple.Activate();
        SurfSimple.SetTex(tex);
      } else {
        SurfSimpleMasked.Activate();
        SurfSimpleMasked.SetTex(tex);
      }
    }
  }

  if (surf->count < 3) return false;

  const float globVis = R_CalcGlobVis();
  const int lightMode = (surf->Fade == FADE_LIGHT ? r_light_mode.asInt() : 0);
  const bool fogAllowed = (surf->Fade != FADE_LIGHT || r_light_mode.asInt() <= 0);

  float lev = getSurfLightLevel(surf);
  if (doBrightmap) {
    //SurfSimpleBrightmap.SetLight(((surf->Light>>16)&255)*lev/255.0f, ((surf->Light>>8)&255)*lev/255.0f, (surf->Light&255)*lev/255.0f, 1.0f);
    SurfSimpleBrightmap.SetLightGlobVis(globVis);
    SurfSimpleBrightmap.SetLightMode(lightMode);
    SurfSimpleBrightmap.SetLight(((surf->Light>>16)&255)/255.0f, ((surf->Light>>8)&255)/255.0f, (surf->Light&255)/255.0f, lev);
    SurfSimpleBrightmap.SetFogFade((fogAllowed ? surf->Fade : 0), 1.0f);
    if (gp.isActive()) {
      VV_GLDRAWER_ACTIVATE_GLOW(SurfSimpleBrightmap, gp);
    } else {
      VV_GLDRAWER_DEACTIVATE_GLOW(SurfSimpleBrightmap);
    }
  } else {
    if ((surf->drawflags&surface_t::DF_MASKED) == 0) {
      //SurfSimple.SetLight(((surf->Light>>16)&255)*lev/255.0f, ((surf->Light>>8)&255)*lev/255.0f, (surf->Light&255)*lev/255.0f, 1.0f);
      SurfSimple.SetLightGlobVis(globVis);
      SurfSimple.SetLightMode(lightMode);
      SurfSimple.SetLight(((surf->Light>>16)&255)/255.0f, ((surf->Light>>8)&255)/255.0f, (surf->Light&255)/255.0f, lev);
      SurfSimple.SetFogFade((fogAllowed ? surf->Fade : 0), 1.0f);
      if (gp.isActive()) {
        VV_GLDRAWER_ACTIVATE_GLOW(SurfSimple, gp);
      } else {
        VV_GLDRAWER_DEACTIVATE_GLOW(SurfSimple);
      }
    } else {
      //SurfSimpleMasked.SetLight(((surf->Light>>16)&255)*lev/255.0f, ((surf->Light>>8)&255)*lev/255.0f, (surf->Light&255)*lev/255.0f, 1.0f);
      SurfSimpleMasked.SetLightGlobVis(globVis);
      SurfSimpleMasked.SetLightMode(lightMode);
      SurfSimpleMasked.SetLight(((surf->Light>>16)&255)/255.0f, ((surf->Light>>8)&255)/255.0f, (surf->Light&255)/255.0f, lev);
      SurfSimpleMasked.SetFogFade((fogAllowed ? surf->Fade : 0), 1.0f);
      if (gp.isActive()) {
        VV_GLDRAWER_ACTIVATE_GLOW(SurfSimpleMasked, gp);
      } else {
        VV_GLDRAWER_DEACTIVATE_GLOW(SurfSimpleMasked);
      }
    }
  }

  const bool doDecals = RenderSurfaceHasDecals(surf);

  // fill stencil buffer for decals
  if (doDecals) RenderPrepareShaderDecals(surf);

  //glBegin(GL_POLYGON);
  currentActiveShader->UploadChangedUniforms();
  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);
  glBegin(GL_TRIANGLE_FAN);
    for (int i = 0; i < surf->count; ++i) glVertex(surf->verts[i].vec());
  glEnd();
  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glEnable(GL_CULL_FACE);

  // draw decals
  if (doDecals) {
    if (RenderFinishShaderDecals(DT_SIMPLE, surf, nullptr, tex->ColorMap)) {
      //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
      //glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // decal renderer is using this too
      if (doBrightmap) {
        SurfSimpleBrightmap.Activate();
      } else {
        if ((surf->drawflags&surface_t::DF_MASKED) == 0) SurfSimple.Activate(); else SurfSimpleMasked.Activate();
      }
      return true;
    }
  }

  return false;
}


//==========================================================================
//
//  VOpenGLDrawer::RenderLMapSurface
//
//  returns `true` if we need to re-setup texture
//
//==========================================================================
bool VOpenGLDrawer::RenderLMapSurface (bool textureChanged, surface_t *surf, surfcache_t *cache) {
  texinfo_t *tex = surf->texinfo;

  bool doBrightmap = (r_brightmaps && tex->Tex->Brightmap);

  GlowParams gp;
  CalcGlow(gp, surf);

  float spec = (r_lmap_overbright.asBool() ? r_overbright_specular.asFloat() : 0.0f);
  if (!isFiniteF(spec)) spec = 0.1f;
  spec = clampval(spec, 0.0f, 16.0f);

  if (textureChanged) {
    if (doBrightmap) {
      SurfLightmapBrightmapOverbright.Activate();
      SurfLightmapBrightmapOverbright.SetBrightMapAdditive(r_brightmaps_additive ? 1.0f : 0.0f);
      SurfLightmapBrightmapOverbright.SetTexture(0);
      SurfLightmapBrightmapOverbright.SetLightMap(1);
      SurfLightmapBrightmapOverbright.SetSpecular(spec);
      SurfLightmapBrightmapOverbright.SetTextureBM(2);
      SelectTexture(2);
      SetBrightmapTexture(tex->Tex->Brightmap);
      SelectTexture(0);
      SetCommonTexture(tex->Tex, tex->ColorMap);
      SurfLightmapBrightmapOverbright.SetTex(tex);
    } else {
      SetCommonTexture(tex->Tex, tex->ColorMap);
      if ((surf->drawflags&surface_t::DF_MASKED) == 0) {
        SurfLightmapOverbright.Activate();
        SurfLightmapOverbright.SetTex(tex);
        SurfLightmapOverbright.SetSpecular(spec);
      } else {
        SurfLightmapMaskedOverbright.Activate();
        SurfLightmapMaskedOverbright.SetTex(tex);
        SurfLightmapMaskedOverbright.SetSpecular(spec);
      }
    }
  }

  if (surf->count < 3) return false;

  float lev = getSurfLightLevel(surf);
  float fullBright = 0.0f;
  if (r_glow_flat && !surf->seg && tex->Tex->IsGlowFullbright()) {
    if (surf->subsector && surf->subsector->sector && !surf->subsector->sector->heightsec) {
      lev = 1.0f;
      fullBright = 1.0f;
    }
  }

  /*
  if ((surf->drawflags&surface_t::DF_MASKED) != 0) {
    GCon->Logf(NAME_Debug, "LMtex:%p:%s: saxis=(%g,%g,%g); taxis=(%g,%g,%g); saxisLM=(%g,%g,%g); taxisLM=(%g,%g,%g); min=(%d,%d); ext=(%d,%d)", tex, *tex->Tex->Name, tex->saxis.x, tex->saxis.y, tex->saxis.z, tex->taxis.x, tex->taxis.y, tex->taxis.z, tex->saxisLM.x, tex->saxisLM.y, tex->saxisLM.z, tex->taxisLM.x, tex->taxisLM.y, tex->taxisLM.z, surf->texturemins[0], surf->texturemins[1], surf->extents[0], surf->extents[1]);
  }
  */

  const float globVis = R_CalcGlobVis();
  const int lightMode = (surf->Fade == FADE_LIGHT ? r_light_mode.asInt() : 0);
  const bool fogAllowed = (surf->Fade != FADE_LIGHT || r_light_mode.asInt() <= 0);

  if (doBrightmap) {
    SurfLightmapBrightmapOverbright.SetFullBright(fullBright);
    SurfLightmapBrightmapOverbright.SetLMap(surf, tex, cache);
    //SurfLightmapBrightmapOverbright.SetLight(((surf->Light>>16)&255)*lev/255.0f, ((surf->Light>>8)&255)*lev/255.0f, (surf->Light&255)*lev/255.0f, 1.0f);
    SurfLightmapBrightmapOverbright.SetLightGlobVis(globVis);
    SurfLightmapBrightmapOverbright.SetLightMode(lightMode);
    SurfLightmapBrightmapOverbright.SetLight(((surf->Light>>16)&255)/255.0f, ((surf->Light>>8)&255)/255.0f, (surf->Light&255)/255.0f, lev);
    SurfLightmapBrightmapOverbright.SetFogFade((fogAllowed ? surf->Fade : 0), 1.0f);
    if (gp.isActive()) {
      VV_GLDRAWER_ACTIVATE_GLOW(SurfLightmapBrightmapOverbright, gp);
    } else {
      VV_GLDRAWER_DEACTIVATE_GLOW(SurfLightmapBrightmapOverbright);
    }
  } else {
    if ((surf->drawflags&surface_t::DF_MASKED) == 0) {
      SurfLightmapOverbright.SetFullBright(fullBright);
      SurfLightmapOverbright.SetLMap(surf, tex, cache);
      //SurfLightmapOverbright.SetLight(((surf->Light>>16)&255)*lev/255.0f, ((surf->Light>>8)&255)*lev/255.0f, (surf->Light&255)*lev/255.0f, 1.0f);
      SurfLightmapOverbright.SetLightGlobVis(globVis);
      SurfLightmapOverbright.SetLightMode(lightMode);
      SurfLightmapOverbright.SetLight(((surf->Light>>16)&255)/255.0f, ((surf->Light>>8)&255)/255.0f, (surf->Light&255)/255.0f, lev);
      SurfLightmapOverbright.SetFogFade((fogAllowed ? surf->Fade : 0), 1.0f);
      if (gp.isActive()) {
        VV_GLDRAWER_ACTIVATE_GLOW(SurfLightmapOverbright, gp);
      } else {
        VV_GLDRAWER_DEACTIVATE_GLOW(SurfLightmapOverbright);
      }
    } else {
      SurfLightmapMaskedOverbright.SetFullBright(fullBright);
      SurfLightmapMaskedOverbright.SetLMap(surf, tex, cache);
      //SurfLightmapMaskedOverbright.SetLight(((surf->Light>>16)&255)*lev/255.0f, ((surf->Light>>8)&255)*lev/255.0f, (surf->Light&255)*lev/255.0f, 1.0f);
      SurfLightmapMaskedOverbright.SetLightGlobVis(globVis);
      SurfLightmapMaskedOverbright.SetLightMode(lightMode);
      SurfLightmapMaskedOverbright.SetLight(((surf->Light>>16)&255)/255.0f, ((surf->Light>>8)&255)/255.0f, (surf->Light&255)/255.0f, lev);
      SurfLightmapMaskedOverbright.SetFogFade((fogAllowed ? surf->Fade : 0), 1.0f);
      if (gp.isActive()) {
        VV_GLDRAWER_ACTIVATE_GLOW(SurfLightmapMaskedOverbright, gp);
      } else {
        VV_GLDRAWER_DEACTIVATE_GLOW(SurfLightmapMaskedOverbright);
      }
    }
  }

  const bool doDecals = RenderSurfaceHasDecals(surf);

  // fill stencil buffer for decals
  if (doDecals) RenderPrepareShaderDecals(surf);

  //glBegin(GL_POLYGON);
  currentActiveShader->UploadChangedUniforms();
  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glDisable(GL_CULL_FACE);
  glBegin(GL_TRIANGLE_FAN);
    for (int i = 0; i < surf->count; ++i) glVertex(surf->verts[i].vec());
  glEnd();
  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) glEnable(GL_CULL_FACE);

  // draw decals
  if (doDecals) {
    if (RenderFinishShaderDecals(DT_LIGHTMAP, surf, cache, tex->ColorMap)) {
      //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
      //glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // decal renderer is using this too
      if (doBrightmap) {
        SurfLightmapBrightmapOverbright.Activate();
      } else {
        if ((surf->drawflags&surface_t::DF_MASKED) == 0) {
          SurfLightmapOverbright.Activate();
        } else {
          SurfLightmapMaskedOverbright.Activate();
        }
      }
      return true;
    }
  }

  return false;
}


//==========================================================================
//
//  COMMAND dgl_ForceAtlasUpdate
//
//==========================================================================
static bool forceAtlasUpdate = 0;
COMMAND(dgl_ForceAtlasUpdate) {
  if (!forceAtlasUpdate) {
    forceAtlasUpdate = 1;
    GCon->Logf("forcing lightmap atlas update.");
  }
}


//==========================================================================
//
//  VOpenGLDrawer::PrepareWireframe
//
//==========================================================================
void VOpenGLDrawer::PrepareWireframe () {
  DrawAutomap.Activate();
  DrawAutomap.UploadChangedUniforms();
  GLEnableBlend();
  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

  SelectTexture(1);
  glBindTexture(GL_TEXTURE_2D, 0);
  SelectTexture(0);
}


//==========================================================================
//
//  VOpenGLDrawer::DrawWireframeSurface
//
//==========================================================================
void VOpenGLDrawer::DrawWireframeSurface (const surface_t *surf) {
  if (!surf || surf->count < 3) return;
  const TVec sv0 = surf->verts[0].vec();
  for (int i = 1; i < surf->count; ++i) {
    glBegin(GL_LINE_LOOP);
    glVertex(sv0);
    glVertex(surf->verts[i].vec());
    glVertex(surf->verts[(i+1)%surf->count].vec());
    glEnd();
  }
  /*
  glPointSize(3.0f);
  glBegin(GL_POINTS);
  for (int i = 0; i < surf->count; ++i) glVertex(surf->verts[i].vec());
  glEnd();
  glPointSize(1.0f);
  */
}


//==========================================================================
//
//  VOpenGLDrawer::DoneWireframe
//
//==========================================================================
void VOpenGLDrawer::DoneWireframe () {
}


//==========================================================================
//
//  VOpenGLDrawer::DrawLightmapWorld
//
//  lightmapped rendering
//
//==========================================================================
void VOpenGLDrawer::DrawLightmapWorld () {
  texinfo_t lastTexinfo;
  lastTexinfo.initLastUsed();

  VRenderLevelDrawer::DrawLists &dls = RendLev->GetCurrentDLS();

  if (gl_dbg_wireframe) {
    PrepareWireframe();
    for (auto &&surf : dls.DrawSurfListSolid) DrawWireframeSurface(surf);
    for (auto &&surf : dls.DrawSurfListMasked) DrawWireframeSurface(surf);

    for (vuint32 lcbn = RendLev->GetLightChainHead(); lcbn; lcbn = RendLev->GetLightChainNext(lcbn)) {
      const vuint32 lb = lcbn-1;
      vassert(lb < NUM_BLOCK_SURFS);
      for (surfcache_t *cache = RendLev->GetLightChainFirst(lb); cache; cache = cache->chain) {
        surface_t *surf = cache->surf;
        if (!surf->IsPlVisible()) continue; // viewer is in back side or on plane
        DrawWireframeSurface(surf);
      }
    }

    DoneWireframe();
    return;
  }


  // first draw horizons
  {
    surface_t **surfptr = dls.DrawHorizonList.ptr();
    for (int count = dls.DrawHorizonList.length(); count--; ++surfptr) {
      surface_t *surf = *surfptr;
      if (!surf->IsPlVisible()) continue; // viewer is in back side or on plane
      DoHorizonPolygon(surf);
    }
  }

  // for sky areas we just write to the depth buffer to prevent drawing polygons behind the sky
  if (dls.DrawSkyList.length()) {
    SurfZBuf.Activate();
    currentActiveShader->UploadChangedUniforms();
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    surface_t **surfptr = dls.DrawSkyList.ptr();
    for (int count = dls.DrawSkyList.length(); count--; ++surfptr) {
      surface_t *surf = *surfptr;
      if (!surf->IsPlVisible()) continue; // viewer is in back side or on plane
      if (surf->count < 3) continue;
      glBegin(GL_TRIANGLE_FAN);
        for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i].vec());
      glEnd();
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  }

  const bool useDepthPreFill = gl_regular_prefill_depth.asBool();

  // render surfaces to depth buffer to avoid overdraw later
  if (useDepthPreFill) {
    #if 1
    SurfZBuf.Activate();
    currentActiveShader->UploadChangedUniforms();
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    //glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
    // solid
    for (auto &&surf : dls.DrawSurfListSolid) {
      if (!surf->IsPlVisible()) continue; // viewer is in back side or on plane
      if (surf->drawflags&surface_t::DF_MASKED) continue; // later
      const texinfo_t *currTexinfo = surf->texinfo;
      if (!currTexinfo || currTexinfo->isEmptyTexture()) continue; // just in case
      glBegin(GL_TRIANGLE_FAN);
        for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i].vec());
      glEnd();
    }
    // collect masked
    surfListClear();
    for (auto &&surf : dls.DrawSurfListMasked) {
      if (!surf->IsPlVisible()) continue; // viewer is in back side or on plane
      if (!(surf->drawflags&surface_t::DF_MASKED)) continue; // not here
      const texinfo_t *currTexinfo = surf->texinfo;
      if (!currTexinfo || currTexinfo->isEmptyTexture()) continue; // just in case
      surfListAppend(surf);
      /*
      glBegin(GL_TRIANGLE_FAN);
        for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i].vec());
      glEnd();
      */
    }
    // lightmapped (and collect masked)
    for (vuint32 lcbn = RendLev->GetLightChainHead(); lcbn; lcbn = RendLev->GetLightChainNext(lcbn)) {
      const vuint32 lb = lcbn-1;
      vassert(lb < NUM_BLOCK_SURFS);
      for (surfcache_t *cache = RendLev->GetLightChainFirst(lb); cache; cache = cache->chain) {
        surface_t *surf = cache->surf;
        if (!surf->IsPlVisible()) continue; // viewer is in back side or on plane
        const texinfo_t *currTexinfo = surf->texinfo;
        if (!currTexinfo || currTexinfo->isEmptyTexture()) continue; // just in case
        if (surf->drawflags&surface_t::DF_MASKED) {
          surfListAppend(surf);
        } else {
          glBegin(GL_TRIANGLE_FAN);
            for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i].vec());
          glEnd();
        }
      }
    }
    glEnable(GL_TEXTURE_2D);
    // render masked textures
    if (surfList.length() > 0) {
      SurfZBufMasked.Activate();
      SurfZBufMasked.SetTexture(0);
      currentActiveShader->UploadChangedUniforms();
      timsort_r(surfList.ptr(), surfList.length(), sizeof(SurfListItem), &surfListItemCmp, nullptr);
      lastTexinfo.resetLastUsed();
      for (auto &&sli : surfList) {
        surface_t *surf = sli.surf;
        const texinfo_t *currTexinfo = surf->texinfo;
        if (!currTexinfo || currTexinfo->isEmptyTexture()) continue; // just in case
        const bool textureChanded = lastTexinfo.needChange(*currTexinfo, updateFrame);
        if (textureChanded) {
          //glBindTexture(GL_TEXTURE_2D, 0);
          SetCommonTexture(currTexinfo->Tex, currTexinfo->ColorMap);
          SurfZBufMasked.SetTex(currTexinfo);
          currentActiveShader->UploadChangedUniforms();
          lastTexinfo.updateLastUsed(*currTexinfo);
        }
        glBegin(GL_TRIANGLE_FAN);
          for (unsigned i = 0; i < (unsigned)surf->count; ++i) glVertex(surf->verts[i].vec());
        glEnd();
      }
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    #endif
    //glDepthMask(GL_FALSE); // no z-buffer writes
    GLDisableDepthWrite();
    glDepthFunc(GL_EQUAL);
  }


  // draw surfaces without lightmaps
  if (dls.DrawSurfListSolid.length() != 0 || dls.DrawSurfListMasked.length() != 0) {
    // sort by texture, to minimise texture switches
    if (gl_sort_textures) {
      timsort_r(dls.DrawSurfListSolid.ptr(), dls.DrawSurfListSolid.length(), sizeof(surface_t *), &drawListItemCmp, nullptr);
      timsort_r(dls.DrawSurfListMasked.ptr(), dls.DrawSurfListMasked.length(), sizeof(surface_t *), &drawListItemCmp, nullptr);
    }

    SurfSimpleMasked.Activate();
    SurfSimpleMasked.SetTexture(0);
    VV_GLDRAWER_DEACTIVATE_GLOW(SurfSimpleMasked);
    //SurfSimple_Locs.storeFogType();
    SurfSimpleMasked.UploadChangedUniforms();

    SurfSimple.Activate();
    SurfSimple.SetTexture(0);
    VV_GLDRAWER_DEACTIVATE_GLOW(SurfSimple);
    //SurfSimple_Locs.storeFogType();
    SurfSimple.UploadChangedUniforms();

    // normal
    lastTexinfo.resetLastUsed();
    for (auto &&surf : dls.DrawSurfListSolid) {
      if (!surf->IsPlVisible()) continue; // viewer is in back side or on plane
      const texinfo_t *currTexinfo = surf->texinfo;
      if (!currTexinfo || currTexinfo->isEmptyTexture()) continue; // just in case
      if (surf->drawflags&surface_t::DF_MASKED) continue; // later
      const bool textureChanded = lastTexinfo.needChange(*currTexinfo, updateFrame);
      if (textureChanded) lastTexinfo.updateLastUsed(*currTexinfo);
      if (RenderSimpleSurface(textureChanded, surf)) lastTexinfo.resetLastUsed();
    }

    // masked
    lastTexinfo.resetLastUsed();
    if (dls.DrawSurfListMasked.length()) {
      for (auto &&surf : dls.DrawSurfListMasked) {
        if (!surf->IsPlVisible()) continue; // viewer is in back side or on plane
        const texinfo_t *currTexinfo = surf->texinfo;
        if (!currTexinfo || currTexinfo->isEmptyTexture()) continue; // just in case
        if (!(surf->drawflags&surface_t::DF_MASKED)) continue; // not here
        const bool textureChanded = lastTexinfo.needChange(*currTexinfo, updateFrame);
        if (textureChanded) lastTexinfo.updateLastUsed(*currTexinfo);
        if (RenderSimpleSurface(textureChanded, surf)) lastTexinfo.resetLastUsed();
      }
    }
  }

  // draw surfaces with lightmaps
  {
    //unsigned lmc = 0;
    float spec = (r_lmap_overbright.asBool() ? r_overbright_specular.asFloat() : 0.0f);
    if (!isFiniteF(spec)) spec = 0.1f;
    spec = clampval(spec, 0.0f, 16.0f);

    SurfLightmapMaskedOverbright.Activate();
    SurfLightmapMaskedOverbright.SetTexture(0);
    SurfLightmapMaskedOverbright.SetLightMap(1);
    SurfLightmapMaskedOverbright.SetSpecular(spec);
    //SurfLightmap_Locs.storeFogType();
    VV_GLDRAWER_DEACTIVATE_GLOW(SurfLightmapMaskedOverbright);
    SurfLightmapMaskedOverbright.UploadChangedUniforms();

    //unsigned lmc = 0;
    SurfLightmapOverbright.Activate();
    SurfLightmapOverbright.SetTexture(0);
    SurfLightmapOverbright.SetLightMap(1);
    SurfLightmapOverbright.SetSpecular(spec);
    //SurfLightmap_Locs.storeFogType();
    VV_GLDRAWER_DEACTIVATE_GLOW(SurfLightmapOverbright);
    SurfLightmapOverbright.UploadChangedUniforms();

    lastTexinfo.resetLastUsed();
    if (forceAtlasUpdate) {
      forceAtlasUpdate = 0;
      memset(atlases_updated, 0, sizeof(atlases_updated));
    }

    //GCon->Logf(NAME_Debug, "GL: ************ (%u)", RendLev->GetLightChainHead());
    for (vuint32 lcbn = RendLev->GetLightChainHead(); lcbn; lcbn = RendLev->GetLightChainNext(lcbn)) {
      const vuint32 lb = lcbn-1;
      vassert(lb < NUM_BLOCK_SURFS);
      VDirtyArea &blockDirty = RendLev->GetLightBlockDirtyArea(lb);
      // is full atlas update required?
      if ((atlases_updated[lb]&0x01u) == 0) {
        atlases_updated[lb] |= 0x01u;
        blockDirty.addDirty(0, 0, BLOCK_WIDTH, BLOCK_HEIGHT);
      } else {
        /*
        if (blockDirty.isValid()) {
          GCon->Logf(NAME_Debug, "GL: *** lightmap atlas #%u (%d,%d,%d,%d)", lb, blockDirty.x0, blockDirty.y0, blockDirty.x1, blockDirty.y1);
        }
        */
      }
      //++lmc;
      //GCon->Logf(NAME_Debug, "GL: *** lightmap atlas #%u (%u)", lb, lmap_id[lb]);

      SelectTexture(1);
      glBindTexture(GL_TEXTURE_2D, lmap_id[lb]);

      // check for lightmap update
      if (blockDirty.isValid()) {
        //GCon->Logf(NAME_Debug, "GL: updated lightmap atlas #%u", lb);
        //RendLev->SetLightBlockChanged(lb, false);
        //glTexImage2D(GL_TEXTURE_2D, 0, 4, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, RendLev->GetLightBlock(lb));
        //RendLev->SetLightAddBlockChanged(lb, true);
        const int x0 = blockDirty.x0;
        const int y0 = blockDirty.y0;
        const int wdt = blockDirty.getWidth();
        const int hgt = blockDirty.getHeight();
        if (/*(wdt >= BLOCK_WIDTH && hgt >= BLOCK_HEIGHT) ||*/ !gl_lmap_allow_partial_updates) {
          // this still can be slower than `glTexSubImage2D()`
          glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, RendLev->GetLightBlock(lb));
        } else {
          glPixelStorei(GL_UNPACK_ROW_LENGTH, BLOCK_WIDTH);
          glTexSubImage2D(GL_TEXTURE_2D, 0,
            x0, y0, wdt, hgt,
            GL_RGBA, GL_UNSIGNED_BYTE, RendLev->GetLightBlock(lb)+y0*BLOCK_WIDTH+x0);
          glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        }
        blockDirty.clear();
      }

      SelectTexture(0);

      if (!gl_sort_textures) {
        for (surfcache_t *cache = RendLev->GetLightChainFirst(lb); cache; cache = cache->chain) {
          surface_t *surf = cache->surf;
          if (!surf->IsPlVisible()) continue; // viewer is in back side or on plane
          const texinfo_t *currTexinfo = surf->texinfo;
          if (!currTexinfo || currTexinfo->isEmptyTexture()) continue; // just in case
          const bool textureChanded = lastTexinfo.needChange(*currTexinfo, updateFrame);
          if (textureChanded) lastTexinfo.updateLastUsed(*currTexinfo);
          if (RenderLMapSurface(textureChanded, surf, cache)) lastTexinfo.resetLastUsed();
        }
      } else {
        surfListClear();
        for (surfcache_t *cache = RendLev->GetLightChainFirst(lb); cache; cache = cache->chain) {
          surface_t *surf = cache->surf;
          if (!surf->IsPlVisible()) continue; // viewer is in back side or on plane
          surfListAppend(surf, cache);
        }
        if (surfList.length() > 0) {
          timsort_r(surfList.ptr(), surfList.length(), sizeof(SurfListItem), &surfListItemCmp, nullptr);
          for (auto &&sli : surfList) {
            surface_t *surf = sli.surf;
            const texinfo_t *currTexinfo = surf->texinfo;
            if (!currTexinfo || currTexinfo->isEmptyTexture()) continue; // just in case
            const bool textureChanded = lastTexinfo.needChange(*currTexinfo, updateFrame);
            if (textureChanded) lastTexinfo.updateLastUsed(*currTexinfo);
            if (RenderLMapSurface(textureChanded, surf, sli.cache)) lastTexinfo.resetLastUsed();
            //if (RenderSimpleSurface(textureChanded, surf)) lastTexinfo.resetLastUsed();
          }
        }
      }
    }
    //if (lmc) GCon->Logf(NAME_Debug, "rendered %u lightmap chains out of %u lightmap block surfs", lmc, (unsigned)NUM_BLOCK_SURFS);
  }

  if (useDepthPreFill) {
    //glDepthMask(GL_TRUE); // allow z-buffer writes
    GLEnableDepthWrite();
    RestoreDepthFunc();
  }
}
