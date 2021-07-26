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
#include "../../psim/p_decal.h"  /* decal_t */


// main work is done by `VLevel->CleanupDecals()`
extern VCvarI gl_bigdecal_limit;
extern VCvarI gl_smalldecal_limit;


// decal kind
enum { DWALL, DFLOOR, DCEILING };


//==========================================================================
//
//  GetSurfDecalKind
//
//==========================================================================
static inline int GetSurfDecalKind (const surface_t *surf) noexcept {
  if (surf->plane.normal.z == 0.0f) return DWALL;
  const subregion_t *sr = surf->sreg; // owning subsector region (can be `nullptr` for walls)
  if (!sr || (sr->secregion->regflags&sec_region_t::RF_BaseRegion)) {
    return (surf->plane.normal.z < 0.0f ? DCEILING : DFLOOR);
  } else {
    return (surf->plane.normal.z > 0.0f ? DCEILING : DFLOOR);
  }
}


//==========================================================================
//
//  VOpenGLDrawer::RenderPrepareShaderDecals
//
//  returns `true` if there are some decals to render
//
//  "prepare" and "finish" will perform no more checks!
//
//==========================================================================
bool VOpenGLDrawer::RenderSurfaceHasDecals (const surface_t *surf) const {
  if (!surf) return false; // just in case
  if (!r_decals) return false;
  if (RendLev->/*PortalDepth*/PortalUsingStencil) return false; //FIXME: not yet

  // check texture (just in case)
  const texinfo_t *tex = surf->texinfo;
  if (!tex || !tex->Tex || tex->noDecals || tex->Tex->Type == TEXTYPE_Null) return false;

  switch (GetSurfDecalKind(surf)) {
    case DWALL:
      if (!r_decals_wall) return false;
      if (!surf->seg || !surf->seg->decalhead) return false; // nothing to do
      if (!surf->seg->linedef) return false; // check for miniseg, just in case
      break;
    case DFLOOR:
      if (!r_decals_flat) return false;
      if (!surf->sreg || !RendLev->GetSRegFloorDecalHead(surf->sreg)) return false; // nothing to do
      break;
    case DCEILING:
      if (!r_decals_flat) return false;
      if (!surf->sreg || !RendLev->GetSRegCeilingDecalHead(surf->sreg)) return false; // nothing to do
      break;
    default: return false; // just in case
  }

  if (gl_decal_debug_nostencil) return false; // debug

  return true;
}


//==========================================================================
//
//  VOpenGLDrawer::RenderPrepareShaderDecals
//
//  returns `true` if there are some decals to render
//
//  note that if it returned `true`, you *have* to call
//  `RenderFinishShaderDecals()` to restore render state
//
//==========================================================================
void VOpenGLDrawer::RenderPrepareShaderDecals (const surface_t *surf) {
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

#define ROTVEC(v_)  do { \
  const float xc = (v_).x-dc->worldx; \
  const float yc = (v_).y-dc->worldy; \
  const float nx = xc*c-yc*s; \
  const float ny = yc*c+xc*s; \
  (v_).x = nx+dc->worldx; \
  (v_).y = ny+dc->worldy; \
} while (0)


#define CALC_LMODE_VARS  \
  const float globVis = R_CalcGlobVis(); \
  const int lightMode = (surf->Fade == FADE_LIGHT ? r_light_mode.asInt() : 0); \
  const bool fogAllowed = (surf->Fade != FADE_LIGHT || r_light_mode.asInt() <= 0); \
  /*const float llev = (dc->flags&decal_t::Fullbright ? 1.0f : getSurfLightLevel(surf));*/ \
  const float llev = getSurfLightLevel(surf); \
  GlowParams gp; \
  CalcGlow(gp, surf);


//==========================================================================
//
//  VOpenGLDrawer::RenderFinishShaderDecals
//
//  returns `true` if caller should restore vertex program and other params
//  (i.e. some actual decals were rendered)
//
// this will never be called without some decals to process
//
//==========================================================================
bool VOpenGLDrawer::RenderFinishShaderDecals (DecalType dtype, surface_t *surf, surfcache_t *cache, int cmap) {
  const texinfo_t *tex = surf->texinfo;

  // setup shader
  switch (dtype) {
    case DT_SIMPLE:
      SurfDecalNoLMap.Activate();
      SurfDecalNoLMap.SetTexture(0);
      //SurfDecalNoLMap_Locs.storeFogType();
      {
        CALC_LMODE_VARS
        SurfDecalNoLMap.SetLightGlobVis(globVis);
        SurfDecalNoLMap.SetLightMode(lightMode);
        SurfDecalNoLMap.SetLight(((surf->Light>>16)&255)/255.0f, ((surf->Light>>8)&255)/255.0f, (surf->Light&255)/255.0f, llev);
        SurfDecalNoLMap.SetFogFade((fogAllowed ? surf->Fade : 0), 1.0f);
        if (gp.isActive()) {
          VV_GLDRAWER_ACTIVATE_GLOW(SurfDecalNoLMap, gp);
        } else {
          VV_GLDRAWER_DEACTIVATE_GLOW(SurfDecalNoLMap);
        }
      }
      break;
    case DT_LIGHTMAP:
      if (gl_regular_disable_overbright) {
        SurfDecalLMapNoOverbright.Activate();
        SurfDecalLMapNoOverbright.SetTexture(0);
        {
          CALC_LMODE_VARS
          SurfDecalLMapNoOverbright.SetLightGlobVis(globVis);
          SurfDecalLMapNoOverbright.SetLightMode(lightMode);
          SurfDecalLMapNoOverbright.SetLight(((surf->Light>>16)&255)/255.0f, ((surf->Light>>8)&255)/255.0f, (surf->Light&255)/255.0f, llev);
          SurfDecalLMapNoOverbright.SetFogFade((fogAllowed ? surf->Fade : 0), 1.0f);
          if (gp.isActive()) {
            VV_GLDRAWER_ACTIVATE_GLOW(SurfDecalLMapNoOverbright, gp);
          } else {
            VV_GLDRAWER_DEACTIVATE_GLOW(SurfDecalLMapNoOverbright);
          }
        }
        SurfDecalLMapNoOverbright.SetLightMap(1);
        //SurfDecalLMapNoOverbright.SetLMapOnly(tex, surf, cache);
        SurfDecalLMapNoOverbright.SetLMap(surf, tex, cache);
      } else {
        SurfDecalLMapOverbright.Activate();
        SurfDecalLMapOverbright.SetTexture(0);
        //SurfDecalLMapOverbright_Locs.storeFogType();
        {
          CALC_LMODE_VARS
          SurfDecalLMapOverbright.SetLightGlobVis(globVis);
          SurfDecalLMapOverbright.SetLightMode(lightMode);
          SurfDecalLMapOverbright.SetLight(((surf->Light>>16)&255)/255.0f, ((surf->Light>>8)&255)/255.0f, (surf->Light&255)/255.0f, llev);
          SurfDecalLMapOverbright.SetFogFade((fogAllowed ? surf->Fade : 0), 1.0f);
          if (gp.isActive()) {
            VV_GLDRAWER_ACTIVATE_GLOW(SurfDecalLMapOverbright, gp);
          } else {
            VV_GLDRAWER_DEACTIVATE_GLOW(SurfDecalLMapOverbright);
          }
        }
        SurfDecalLMapOverbright.SetLightMap(1);
        SurfDecalLMapOverbright.SetSpecularMap(2);
        //SurfDecalLMapOverbright.SetLMapOnly(tex, surf, cache);
        SurfDecalLMapOverbright.SetLMap(surf, tex, cache);
      }
      break;
    case DT_ADVANCED:
      SurfDecalAdv.Activate();
      SurfDecalAdv.SetTexture(0);
      SurfDecalAdv.SetAmbLightTexture(1);
      SurfDecalAdv.SetScreenSize((float)getWidth(), (float)getHeight());
      break;
    default:
      abort();
  }

  const int dkind = GetSurfDecalKind(surf);
  seg_t *seg = (dkind == DWALL ? surf->seg : nullptr);
  const line_t *line = (dkind == DWALL ? seg->linedef : nullptr);

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
  //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  bool additiveMode = false;

  if (gl_decal_debug_nostencil) glDisable(GL_STENCIL_TEST);
  if (gl_decal_debug_noalpha) GLDisableBlend();

  int rdcount = 0;
  if (gl_decal_reset_max) { maxrdcount = 0; gl_decal_reset_max = false; }

  bool tex1set = false;
  int currTexId = -1; // don't call `SetTexture()` repeatedly
  VTextureTranslation *currTrans = nullptr;
  int lastTexTrans = 0;
  int currBloodTrans = -1; // special code for "blood translation" color translations

  int bigDecalCount = 0;
  int smallDecalCount = 0;

  // also, clear dead decals here, 'cause why not?

  decal_t *dcl;
  switch (dkind) {
    case DWALL: dcl = seg->decalhead; break;
    case DFLOOR: dcl = RendLev->GetSRegFloorDecalHead(surf->sreg); break;
    case DCEILING: dcl = RendLev->GetSRegCeilingDecalHead(surf->sreg); break;
    default: abort(); // just in case
  }

  // shade colors and mode
  float sr = 0.0f, sg = 0.0f, sb = 0.0f, smode = 0.0f;

  while (dcl) {
    decal_t *dc = dcl;
    dcl = (dkind == DWALL ? dcl->next : dcl->sregnext);

    const int dcTexId = dc->texture; // "0" means "no texture found"
    VTexture *dtex = (dcTexId > 0 ? GTextureManager[dcTexId] : nullptr);

    // decal scale is not inverted
    const float dscaleX = dc->scaleX;
    const float dscaleY = dc->scaleY;

    if (!dtex || dtex->Width < 1 || dtex->Height < 1 || dc->alpha <= 0.004f || dscaleX <= 0.0f || dscaleY <= 0.0f) {
      // remove it, if it is not animated
      if (!dc->animator) {
        if (GClLevel) GClLevel->DestroyDecal(dc);
      }
      continue;
    }

    if (dkind == DWALL) {
      // check decal flags
      if ((dc->flags&decal_t::NoMidTex) && (surf->typeFlags&surface_t::TF_MIDDLE)) continue;
      if ((dc->flags&decal_t::NoTopTex) && (surf->typeFlags&surface_t::TF_TOP)) continue;
      if ((dc->flags&decal_t::NoBotTex) && (surf->typeFlags&surface_t::TF_BOTTOM)) continue;
    }

    const float twdt = dtex->GetScaledWidthF()*dscaleX;
    const float thgt = dtex->GetScaledHeightF()*dscaleY;

    if (twdt < 1.0f || thgt < 1.0f) continue;

    if (lastTexTrans != dc->translation) {
      auto trans = R_GetCachedTranslation(dc->translation, GClLevel); //FIXME! -- k8: what i wanted to fix here?
      if (trans && trans->isBloodTranslation) {
        if (currTrans) {
          currTrans = nullptr;
          currTexId = -1;
        }
        currBloodTrans = trans->Color&0xffffff;
      } else {
        if (currTrans != trans) {
          currTexId = -1;
          currTrans = trans;
        }
        currBloodTrans = -1;
      }
      lastTexTrans = dc->translation;
    }

    // permament decals are ignored
    if (!dc->isPermanent()) {
      if (twdt >= VLevel::BigDecalWidth || thgt >= VLevel::BigDecalHeight) ++bigDecalCount; else ++smallDecalCount;
    }

    if (currTexId != dcTexId) {
      currTexId = dcTexId;
      SetDecalTexture(dtex, currTrans, cmap, (dc->flags&(decal_t::BloodSplat|decal_t::BootPrint))); // this sets `tex_iw` and `tex_ih`
    }

    const float pd2 =
      dc->slidesec ?
        (dc->flags&decal_t::SlideFloor ? dc->slidesec->floor.TexZ :
         dc->flags&decal_t::SlideCeil ? dc->slidesec->ceiling.TexZ :
         0.0f) : 0.0f;

    // check cache
    if (dc->needRecalc(surf->plane.dist, pd2)) {
      dc->angle = AngleMod(dc->angle);
      dc->updateCache(surf->plane.dist, pd2);
      // use origScale to get the original starting point
      const float txofs = dtex->GetScaledSOffsetF()*dscaleX;
      const float tyofs = dtex->GetScaledTOffsetF()*dc->origScaleY;
      // decal scale is not inverted, but we need inverted scale for some calculations
      const float dscaleXInv = 1.0f/dscaleX;
      const float dscaleYInv = 1.0f/dscaleY;
      // recalc
      TVec saxis, taxis;
      float soffs, toffs;
      if (dkind == DWALL) {
        //const TVec wp = (*line->v1)+line->ndir*(dc->xdist+dc->ofsX);
        const TVec &v0 = (dc->flags&decal_t::FromV2 ? (*line->v2) : (*line->v1));
        TVec ndir = line->ndir;
        if (dc->flags&decal_t::FromV2) ndir = -ndir;
        const float xstofs = dc->xdist-txofs+dc->ofsX;
        TVec v1 = v0+ndir*xstofs;
        TVec v2 = v1+ndir*twdt;

        float dcz = dc->curz+dscaleY+tyofs-dc->ofsY;
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
        //saxis = TVec(seg->dir);
        saxis = TVec(ndir);
        taxis = TVec(0.0f, 0.0f, -1.0f);
        #if 1
        // this does one part of decal rotation
        // decal placement code is not ready for this yet, so it is disabled
        //float s = 1.0f, c = 1.0f;
        const float angle = -dc->angle;
        //const float angle = 37.0f;
        if (angle != 0.0f) {
          float s, c;
          msincos(angle, &s, &c);
          taxis = TVec(s*seg->ndir.x, s*seg->ndir.y, -c);
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

        dc->v1 = TVec(v1.x, v1.y, dcz);
        dc->v2 = TVec(v1.x, v1.y, dcz+thgt);
        dc->v3 = TVec(v2.x, v2.y, dcz+thgt);
        dc->v4 = TVec(v2.x, v2.y, dcz);

        if (angle != 0.0f) {
          /*
          GCon->Logf(NAME_Debug, "000: v1=(%g,%g,%g); v2=(%g,%g,%g); v3=(%g,%g,%g); v4=(%g,%g,%g); angle=%g",
            dc->v1.x, dc->v1.y, dc->v1.z,
            dc->v2.x, dc->v2.y, dc->v2.z,
            dc->v3.x, dc->v3.y, dc->v3.z,
            dc->v4.x, dc->v4.y, dc->v4.z, angle);
          */
          #if 0
          const float xmin = min2(min2(min2(dc->v1.x, dc->v2.x), dc->v3.x), dc->v4.x);
          const float xmax = max2(max2(max2(dc->v1.x, dc->v2.x), dc->v3.x), dc->v4.x);
          const float ymin = min2(min2(min2(dc->v1.y, dc->v2.y), dc->v3.y), dc->v4.y);
          const float ymax = max2(max2(max2(dc->v1.y, dc->v2.y), dc->v3.y), dc->v4.y);
          const float zmin = min2(min2(min2(dc->v1.z, dc->v2.z), dc->v3.z), dc->v4.z);
          const float zmax = max2(max2(max2(dc->v1.z, dc->v2.z), dc->v3.z), dc->v4.z);
          TVec cc(xmin+(xmax-xmin)*0.5f, ymin+(ymax-ymin)*0.5f, zmin+(zmax-zmin)*0.5f);
          #else
          // this does rotation around hotspot (i hope)
          TVec cc = v1+ndir*txofs;
          cc.z = dcz+thgt-tyofs;
          #endif
          VRotMatrix M(surf->plane.normal, angle);
          dc->v1 = (dc->v1-cc)*M+cc;
          dc->v2 = (dc->v2-cc)*M+cc;
          dc->v3 = (dc->v3-cc)*M+cc;
          dc->v4 = (dc->v4-cc)*M+cc;
          TVec nv1 = (TVec(v1.x, v1.y, dcz)-cc)*M+cc;
          soffs = -DotProduct(nv1, saxis); // horizontal
          toffs = -DotProduct(nv1, taxis); // vertical
          /*
          GCon->Logf(NAME_Debug, "100: v1=(%g,%g,%g); v2=(%g,%g,%g); v3=(%g,%g,%g); v4=(%g,%g,%g)",
            dc->v1.x, dc->v1.y, dc->v1.z,
            dc->v2.x, dc->v2.y, dc->v2.z,
            dc->v3.x, dc->v3.y, dc->v3.z,
            dc->v4.x, dc->v4.y, dc->v4.z);
          */
        }
        //void RotatePointAroundVector (TVec &dst, const TVec &dir, const TVec &point, float degrees) noexcept;
      } else {
        // floor/ceiling
        float s, c;
        #if 1
        // this distorts the texture, but it seems good
        // sloped textures are done this way too
        if (dc->angle == 0.0f) {
          saxis = TVec(1.0f,  0.0f);
          taxis = TVec(0.0f, -1.0f);
        } else {
          msincos(dc->angle, &s, &c);
          saxis = TVec(c,  s);
          taxis = TVec(s, -c);
        }
        #else
        // and this is right, but cannot seam when goes through different slopes
        taxis = TVec(0.0f, -1.0f);
        saxis = Normalise(CrossProduct(surf->plane.normal, taxis));
        #endif

        saxis *= dtex->TextureSScale()*dscaleXInv;
        taxis *= dtex->TextureTScale()*dscaleYInv;

        if (dc->flags&decal_t::FlipX) saxis = -saxis;
        if (dc->flags&decal_t::FlipY) taxis = -taxis;

        // texture offset already taken into account, so we need only offsets for bottom left corner
        //FIXME: this is TOTALLY wrong!
        //TODO: this is prolly not fully right for flipped decals
        TVec v1 = TVec(dc->worldx, dc->worldy);
        v1.z = surf->plane.GetPointZ(v1);
        TVec ofsv = v1+TVec(-txofs, tyofs);
        if (dc->angle != 0.0f) {
          ROTVEC(ofsv);
        }
        soffs = -DotProduct(ofsv, saxis); // horizontal
        toffs = -DotProduct(ofsv, taxis); // vertical

        // floor/ceiling
        // left-bottom
        TVec qv0 = v1+TVec(-txofs, tyofs);
        // right-bottom
        TVec qv1 = qv0+TVec(twdt, 0.0f);
        // left-top
        TVec qv2 = qv0-TVec(0.0f, thgt);
        // right-top
        TVec qv3 = qv1-TVec(0.0f, thgt);

        // now rotate it
        if (dc->angle != 0.0f) {
          ROTVEC(qv0);
          ROTVEC(qv1);
          ROTVEC(qv2);
          ROTVEC(qv3);
        }

        // fix z points
        qv0.z = surf->plane.GetPointZ(qv0);
        qv1.z = surf->plane.GetPointZ(qv1);
        qv2.z = surf->plane.GetPointZ(qv2);
        qv3.z = surf->plane.GetPointZ(qv3);

        dc->v1 = qv0;
        dc->v2 = qv1;
        dc->v3 = qv2;
        dc->v4 = qv3;
      }

      dc->saxis = saxis;
      dc->taxis = taxis;
      dc->soffs = soffs;
      dc->toffs = toffs;
    }

    // use cached values
    const TVec &saxis = dc->saxis;
    const TVec &taxis = dc->taxis;
    const float &soffs = dc->soffs;
    const float &toffs = dc->toffs;

    if (!currTrans) {
      if (currBloodTrans != -1) {
        smode = 1.0f;
        sr = ((currBloodTrans>>16)&255)/255.0f;
        sg = ((currBloodTrans>>8)&255)/255.0f;
        sb = (currBloodTrans&255)/255.0f;
      } else if (dc->shadeclr != -1) {
        smode = 1.0f;
        sr = ((dc->shadeclr>>16)&255)/255.0f;
        sg = ((dc->shadeclr>>8)&255)/255.0f;
        sb = (dc->shadeclr&255)/255.0f;
      } else {
        smode = 0.0f;
      }
    } else {
      smode = 0.0f;
    }

    // setup texture
    switch (dtype) {
      case DT_SIMPLE:
        SurfDecalNoLMap.SetFullBright(dc->flags&decal_t::Fullbright ? 1.0f : 0.0f);
        SurfDecalNoLMap.SetSplatAlpha(clampval(dc->alpha, 0.0f, 1.0f));
        SurfDecalNoLMap.SetDecalTex(saxis, taxis, soffs, toffs, tex_iw, tex_ih);
        SurfDecalNoLMap.SetTexShade(sr, sg, sb);
        SurfDecalNoLMap.SetTexShadeMode(smode);
        break;
      case DT_LIGHTMAP:
        if (gl_regular_disable_overbright) {
          SurfDecalLMapNoOverbright.SetFullBright(dc->flags&decal_t::Fullbright ? 1.0f : 0.0f);
          SurfDecalLMapNoOverbright.SetSplatAlpha(clampval(dc->alpha, 0.0f, 1.0f));
          SurfDecalLMapNoOverbright.SetDecalTex(saxis, taxis, soffs, toffs, tex_iw, tex_ih);
          SurfDecalLMapNoOverbright.SetTexShade(sr, sg, sb);
          SurfDecalLMapNoOverbright.SetTexShadeMode(smode);
        } else {
          SurfDecalLMapOverbright.SetFullBright(dc->flags&decal_t::Fullbright ? 1.0f : 0.0f);
          SurfDecalLMapOverbright.SetSplatAlpha(clampval(dc->alpha, 0.0f, 1.0f));
          SurfDecalLMapOverbright.SetDecalTex(saxis, taxis, soffs, toffs, tex_iw, tex_ih);
          SurfDecalLMapOverbright.SetTexShade(sr, sg, sb);
          SurfDecalLMapOverbright.SetTexShadeMode(smode);
        }
        break;
      case DT_ADVANCED:
        SurfDecalAdv.SetSplatAlpha(clampval(dc->alpha, 0.0f, 1.0f));
        if (!tex1set) {
          tex1set = true;
          SelectTexture(1);
          glBindTexture(GL_TEXTURE_2D, ambLightFBO.getColorTid());
          SelectTexture(0);
        }
        SurfDecalAdv.SetFullBright(dc->flags&decal_t::Fullbright ? 1.0f : 0.0f);
        SurfDecalAdv.SetDecalTex(saxis, taxis, soffs, toffs, tex_iw, tex_ih);
        SurfDecalAdv.SetTexShade(sr, sg, sb);
        SurfDecalAdv.SetTexShadeMode(smode);
        break;
    }

    currentActiveShader->UploadChangedUniforms();

    const bool dcadditive = dc->isAdditive();
    if (dcadditive != additiveMode) {
      additiveMode = dcadditive;
      if (additiveMode) {
        glBlendFunc(GL_ONE, GL_ONE); // our source rgb is already premultiplied
      } else {
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      }
    }

    glBegin(GL_QUADS);
      if (dkind == DWALL) {
        glVertex(dc->v1);
        glVertex(dc->v2);
        glVertex(dc->v3);
        glVertex(dc->v4);
      } else {
        glVertex(dc->v1);
        glVertex(dc->v3);
        glVertex(dc->v4);
        glVertex(dc->v2);
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

  // restore blending mode
  if (additiveMode) glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  //if (rdcount && dbg_noblend) GCon->Logf(NAME_Debug, "DECALS: total=%d; noblend=%d", rdcount, dbg_noblend);

  //if (bigDecalCount) GCon->Logf(NAME_Debug, "bigDecalCount=%d", bigDecalCount);
  // kill some big decals
  // flat decals limiting is done in spread code
  if (GClLevel && dkind == DWALL) {
    const int bigLimit = gl_bigdecal_limit.asInt();
    const int smallLimit = gl_smalldecal_limit.asInt();
    int toKillBig = (bigLimit > 0 ? bigDecalCount-bigLimit : 0);
    int toKillSmall = (smallLimit > 0 ? smallDecalCount-smallLimit : 0);
    if (toKillBig > 0 || toKillSmall > 0) GClLevel->CleanupSegDecals(seg);
  }

  return true;
}
