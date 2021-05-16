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

// main work is done by `VLevel->CleanupDecals()`
extern VCvarI gl_bigdecal_limit;
extern VCvarI gl_smalldecal_limit;


//==========================================================================
//
//  VOpenGLDrawer::RenderPrepareShaderDecals
//
//==========================================================================
void VOpenGLDrawer::RenderPrepareShaderDecals (surface_t *surf) {
  if (!r_decals) return;
  if (RendLev->/*PortalDepth*/PortalUsingStencil) return; //FIXME: not yet

  if (!surf->seg || !surf->seg->decalhead) return; // nothing to do

  if (gl_decal_debug_nostencil) return; // debug

  if (!decalUsedStencil) decalStcVal = (IsStencilBufferDirty() ? 255 : 0);

  if (++decalStcVal == 0) {
    // it wrapped, so clear stencil buffer
    ClearStencilBuffer();
    decalStcVal = 1;
  }
  glEnable(GL_STENCIL_TEST);
  glStencilFunc(GL_ALWAYS, decalStcVal, 0xff);
  glStencilOp(GL_KEEP, GL_KEEP, /*GL_INCR*/GL_REPLACE);
  decalUsedStencil = true;
  NoteStencilBufferDirty(); // it will be dirtied
}


static int maxrdcount = 0; // sorry for this global, it is for debugging

//==========================================================================
//
//  VOpenGLDrawer::RenderFinishShaderDecals
//
//  returns `true` if caller should restore vertex program and other params
//  (i.e. some actual decals were rendered)
//
//==========================================================================
bool VOpenGLDrawer::RenderFinishShaderDecals (DecalType dtype, surface_t *surf, surfcache_t *cache, int cmap) {
  if (!r_decals) return false;
  if (RendLev->/*PortalDepth*/PortalUsingStencil) return false; //FIXME: not yet

  enum { DWALL, DFLOOR, DCEILING };

  const int dkind =
    surf->plane.normal.z == 0.0f ? DWALL :
    surf->plane.normal.z < 0.0f ? DCEILING : DFLOOR;

  switch (dkind) {
    case DWALL:
      if (!surf->seg || !surf->seg->decalhead) return false; // nothing to do
      break;
    case DFLOOR:
      if (!surf->sreg || !surf->sreg->floordecalhead) return false; // nothing to do
      break;
    case DCEILING:
      if (!surf->sreg || !surf->sreg->ceildecalhead) return false; // nothing to do
      break;
    default: return false; // just in case
  }

  texinfo_t *tex = surf->texinfo;

  // setup shader
  switch (dtype) {
    case DT_SIMPLE:
      SurfDecalNoLMap.Activate();
      SurfDecalNoLMap.SetTexture(0);
      //SurfDecalNoLMap_Locs.storeFogType();
      SurfDecalNoLMap.SetFogFade(surf->Fade, 1.0f);
      {
        //const float llev = (dc->flags&decal_t::Fullbright ? 1.0f : getSurfLightLevel(surf));
        const float llev = getSurfLightLevel(surf);
        SurfDecalNoLMap.SetLight(((surf->Light>>16)&255)/255.0f, ((surf->Light>>8)&255)/255.0f, (surf->Light&255)/255.0f, llev);
      }
      break;
    case DT_LIGHTMAP:
      if (gl_regular_disable_overbright) {
        SurfDecalLMapNoOverbright.Activate();
        SurfDecalLMapNoOverbright.SetTexture(0);
        SurfDecalLMapNoOverbright.SetFogFade(surf->Fade, 1.0f);
        SurfDecalLMapNoOverbright.SetLightMap(1);
        //SurfDecalLMapNoOverbright.SetLMapOnly(tex, surf, cache);
        SurfDecalLMapNoOverbright.SetLMap(surf, tex, cache);
      } else {
        SurfDecalLMapOverbright.Activate();
        SurfDecalLMapOverbright.SetTexture(0);
        //SurfDecalLMapOverbright_Locs.storeFogType();
        SurfDecalLMapOverbright.SetFogFade(surf->Fade, 1.0f);
        SurfDecalLMapOverbright.SetLightMap(1);
        SurfDecalLMapOverbright.SetSpecularMap(2);
        //SurfDecalLMapOverbright.SetLMapOnly(tex, surf, cache);
        SurfDecalLMapOverbright.SetLMap(surf, tex, cache);
      }
      break;
    case DT_ADVANCED:
      SurfAdvDecal.Activate();
      SurfAdvDecal.SetTexture(0);
      SurfAdvDecal.SetAmbLightTexture(1);
      SurfAdvDecal.SetScreenSize((float)getWidth(), (float)getHeight());
      break;
    default:
      abort();
  }

  seg_t *seg = (dkind == DWALL ? surf->seg : nullptr);
  const line_t *line = (dkind == DWALL ? seg->linedef : nullptr);
  if (dkind == DWALL && !line) return false; // just in case

  /*
  GLint oldDepthMask;
  glGetIntegerv(GL_DEPTH_WRITEMASK, &oldDepthMask);
  glDepthMask(GL_FALSE); // no z-buffer writes
  */
  PushDepthMask();
  GLDisableDepthWrite();

  glEnable(GL_STENCIL_TEST);
  glStencilFunc(GL_EQUAL, decalStcVal, 0xff);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  //glDisable(GL_ALPHA_TEST); // just in case

  GLEnableBlend();
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  if (gl_decal_debug_nostencil) glDisable(GL_STENCIL_TEST);
  if (gl_decal_debug_noalpha) GLDisableBlend();

  int rdcount = 0;
  if (gl_decal_reset_max) { maxrdcount = 0; gl_decal_reset_max = false; }

  bool tex1set = false;
  int currTexId = -1; // don't call `SetTexture()` repeatedly
  VTextureTranslation *currTrans = nullptr;
  int lastTexTrans = 0;

  int bigDecalCount = 0;
  int smallDecalCount = 0;

  // also, clear dead decals here, 'cause why not?

  decal_t *dcl;
  switch (dkind) {
    case DWALL: dcl = seg->decalhead; break;
    case DFLOOR: dcl = surf->sreg->floordecalhead; break;
    case DCEILING: dcl = surf->sreg->ceildecalhead; break;
    default: abort(); // just in case
  }

  while (dcl) {
    decal_t *dc = dcl;
    dcl = dcl->next;

    const int dcTexId = dc->texture; // "0" means "no texture found"
    VTexture *dtex = (dcTexId > 0 ? GTextureManager[dcTexId] : nullptr);

    // decal scale is not inverted
    const float dscaleX = dc->scaleX;
    const float dscaleY = dc->scaleY;

    if (!dtex || dtex->Width < 1 || dtex->Height < 1 || dc->alpha <= 0.0f || dscaleX <= 0.0f || dscaleY <= 0.0f) {
      // remove it, if it is not animated
      if (!dc->animator) {
        if (dkind == DWALL) seg->removeDecal(dc); else surf->sreg->removeDecal(dc);
        delete dc;
      }
      continue;
    }

    if (dkind == DWALL) {
      // check decal flags
      if ((dc->flags&decal_t::NoMidTex) && (surf->typeFlags&surface_t::TF_MIDDLE)) continue;
      if ((dc->flags&decal_t::NoTopTex) && (surf->typeFlags&surface_t::TF_TOP)) continue;
      if ((dc->flags&decal_t::NoBotTex) && (surf->typeFlags&surface_t::TF_BOTTOM)) continue;
    }

    if (currTexId != dcTexId || lastTexTrans != dc->translation) {
      auto trans = R_GetCachedTranslation(dc->translation, GClLevel); //FIXME! -- k8: what i wanted to fix here?
      if (currTrans != trans) {
        currTexId = -1;
        currTrans = trans;
      }
    }

    // use origScale to get the original starting point
    const float txofs = dtex->GetScaledSOffsetF()*dscaleX;
    const float tyofs = dtex->GetScaledTOffsetF()*dc->origScaleY;

    const float twdt = dtex->GetScaledWidthF()*dscaleX;
    const float thgt = dtex->GetScaledHeightF()*dscaleY;

    if (twdt < 1.0f || thgt < 1.0f) continue;

    if (twdt >= VLevel::BigDecalWidth || thgt >= VLevel::BigDecalHeight) ++bigDecalCount; else ++smallDecalCount;

    if (currTexId != dcTexId) {
      currTexId = dcTexId;
      SetDecalTexture(dtex, currTrans, cmap); // this sets `tex_iw` and `tex_ih`
    }

    TVec v1, v2;
    TVec saxis, taxis;
    float soffs, toffs;
    float dcz;

    // decal scale is not inverted, but we need inverted scale for some calculations
    const float dscaleXInv = 1.0f/dscaleX;
    const float dscaleYInv = 1.0f/dscaleY;

    if (dkind == DWALL) {
      const float xstofs = dc->xdist-txofs+dc->ofsX;
      v1 = (*line->v1)+line->ndir*xstofs;
      v2 = v1+line->ndir*twdt;

      dcz = dc->curz+dscaleY+tyofs-dc->ofsY;
      // fix Z, if necessary
      if (dc->slidesec) {
        if (dc->flags&decal_t::SlideFloor) {
          // should slide with back floor
          dcz += dc->slidesec->floor.TexZ;
        } else if (dc->flags&decal_t::SlideCeil) {
          // should slide with back ceiling
          dcz += dc->slidesec->ceiling.TexZ;
        }
      }
      // move to bottom
      dcz -= thgt;

      // calculate texture axes
      saxis = TVec(seg->dir);
      taxis = TVec(0.0f, 0.0f, -1.0f);
      #if 0
      // this does one part of decal rotation
      // decal placement code is not ready for this yet, so it is disabled
      float s = 1.0f, c = 1.0f;
      const float angle = 0.0f;
      if (angle != 0.0f) {
        msincos(angle, &s, &c);
        taxis = TVec(s*seg->dir.x, s*seg->dir.y, -c);
        saxis = Normalise(CrossProduct(seg->normal, taxis));
      }
      #endif

      saxis *= dtex->TextureSScale()*dscaleXInv;
      taxis *= dtex->TextureTScale()*dscaleYInv;

      if (dc->flags&decal_t::FlipX) saxis = -saxis;
      if (dc->flags&decal_t::FlipY) taxis = -taxis;

      // texture offset already taken into account, so we need only offsets for bottom left corner
      //TODO: this is prolly not fully right for flipped decals
      soffs = -DotProduct(v1, saxis); // horizontal
      toffs = -DotProduct(TVec(v1.x, v1.y, dcz), taxis); // vertical
    } else {
      // floor/ceiling
      saxis = TVec(1.0f,  0.0f);
      taxis = TVec(0.0f, -1.0f);

      saxis *= dtex->TextureSScale()*dscaleXInv;
      taxis *= dtex->TextureTScale()*dscaleYInv;

      if (dc->flags&decal_t::FlipX) saxis = -saxis;
      if (dc->flags&decal_t::FlipY) taxis = -taxis;

      // texture offset already taken into account, so we need only offsets for bottom left corner
      //FIXME: this is TOTALLY wrong!
      //TODO: this is prolly not fully right for flipped decals
      v1 = TVec(dc->worldx, dc->worldy);
      v1.z = surf->plane.GetPointZ(v1);
      soffs = -DotProduct(v1, saxis); // horizontal
      toffs = -DotProduct(v1, taxis); // vertical
    }

    // setup texture
    switch (dtype) {
      case DT_SIMPLE:
        SurfDecalNoLMap.SetFullBright(dc->flags&decal_t::Fullbright ? 1.0f : 0.0f);
        SurfDecalNoLMap.SetSplatAlpha(dc->alpha);
        SurfDecalNoLMap.SetDecalTex(saxis, taxis, soffs, toffs, tex_iw, tex_ih);
        break;
      case DT_LIGHTMAP:
        if (gl_regular_disable_overbright) {
          SurfDecalLMapNoOverbright.SetFullBright(dc->flags&decal_t::Fullbright ? 1.0f : 0.0f);
          SurfDecalLMapNoOverbright.SetSplatAlpha(dc->alpha);
          SurfDecalLMapNoOverbright.SetDecalTex(saxis, taxis, soffs, toffs, tex_iw, tex_ih);
        } else {
          SurfDecalLMapOverbright.SetFullBright(dc->flags&decal_t::Fullbright ? 1.0f : 0.0f);
          SurfDecalLMapOverbright.SetSplatAlpha(dc->alpha);
          SurfDecalLMapOverbright.SetDecalTex(saxis, taxis, soffs, toffs, tex_iw, tex_ih);
        }
        break;
      case DT_ADVANCED:
        SurfAdvDecal.SetSplatAlpha(dc->alpha);
        if (!tex1set) {
          tex1set = true;
          SelectTexture(1);
          glBindTexture(GL_TEXTURE_2D, ambLightFBO.getColorTid());
          SelectTexture(0);
        }
        SurfAdvDecal.SetFullBright(dc->flags&decal_t::Fullbright ? 1.0f : 0.0f);
        SurfAdvDecal.SetDecalTex(saxis, taxis, soffs, toffs, tex_iw, tex_ih);
        break;
    }

    currentActiveShader->UploadChangedUniforms();
    glBegin(GL_QUADS);
      if (dkind == DWALL) {
        glVertex3f(v1.x, v1.y, dcz);
        glVertex3f(v1.x, v1.y, dcz+thgt);
        glVertex3f(v2.x, v2.y, dcz+thgt);
        glVertex3f(v2.x, v2.y, dcz);
      } else {
        // floor/ceiling
        //TODO: rotation
        // left-bottom
        TVec qv0 = v1-TVec(txofs, tyofs);
        qv0.z = surf->plane.GetPointZ(qv0);
        // right-bottom
        TVec qv1 = qv0+TVec(twdt, 0.0f);
        qv1.z = surf->plane.GetPointZ(qv1);
        // left-top
        TVec qv2 = qv0-TVec(0.0f, thgt);
        qv2.z = surf->plane.GetPointZ(qv2);
        // right-top
        TVec qv3 = qv1-TVec(0.0f, thgt);
        qv3.z = surf->plane.GetPointZ(qv3);
        // draw it
        glVertex3f(qv0.x, qv0.y, qv0.z);
        glVertex3f(qv2.x, qv2.y, qv2.z);
        glVertex3f(qv3.x, qv3.y, qv3.z);
        glVertex3f(qv1.x, qv1.y, qv1.z);
      }
    glEnd();

    ++rdcount;
  }

  if (rdcount > maxrdcount) {
    maxrdcount = rdcount;
    if (gl_decal_dump_max) GCon->Logf(NAME_Debug, "*** max decals on seg: %d", maxrdcount);
  }

  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  if (tex1set) {
    SelectTexture(1);
    glBindTexture(GL_TEXTURE_2D, 0);
    SelectTexture(0);
  }

  if (gl_decal_debug_noalpha) GLEnableBlend();
  glDisable(GL_STENCIL_TEST);
  //glDepthMask(oldDepthMask);
  PopDepthMask();
  GLEnableBlend();

  //if (rdcount && dbg_noblend) GCon->Logf(NAME_Debug, "DECALS: total=%d; noblend=%d", rdcount, dbg_noblend);

  //if (bigDecalCount) GCon->Logf(NAME_Debug, "bigDecalCount=%d", bigDecalCount);
  // kill some big decals
  if (GClLevel) {
    const int bigLimit = gl_bigdecal_limit.asInt();
    const int smallLimit = gl_smalldecal_limit.asInt();
    int toKillBig = (bigLimit > 0 ? bigDecalCount-bigLimit : 0);
    int toKillSmall = (smallLimit > 0 ? smallDecalCount-smallLimit : 0);
    if (toKillBig > 0 || toKillSmall > 0) {
      switch (dkind) {
        case DWALL: GClLevel->CleanupSegDecals(seg); break;
        case DFLOOR: GClLevel->CleanupSRegFloorDecals(surf->sreg); break;
        case DCEILING: GClLevel->CleanupSRegCeilingDecals(surf->sreg); break;
      }
    }
  }

  return true;
}
