//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš, dj_jl
//**  Copyright (C) 2018-2023 Ketmar Dark
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
#include "r_light_adv.h"

extern VCvarI r_light_shadow_min_proj_dimension;

static VCvarI r_dynlight_minimum("r_dynlight_minimum", "6", "Render at least this number of dynamic lights, regardless of total limit.", CVAR_Archive|CVAR_NoShadow);

// this is wrong for now
static VCvarB r_advlight_opt_frustum_full("r_advlight_opt_frustum_full", false, "Optimise 'light is in frustum' case.", CVAR_Archive|CVAR_NoShadow);
static VCvarB r_advlight_opt_frustum_back("r_advlight_opt_frustum_back", false, "Optimise 'light is in frustum' case.", CVAR_Archive|CVAR_NoShadow);

static VCvarB r_advlight_opt_scissor("r_advlight_opt_scissor", true, "Use scissor rectangle to limit light overdraws.", CVAR_Archive|CVAR_NoShadow);

static VCvarB r_shadowvol_use_pofs("r_shadowvol_use_pofs", true, "Use PolygonOffset for shadow volumes to reduce some flickering (WARNING: BUGGY!)?", CVAR_Archive|CVAR_NoShadow);
static VCvarF r_shadowvol_pofs("r_shadowvol_pofs", "20", "DEBUG", CVAR_NoShadow);
static VCvarF r_shadowvol_pslope("r_shadowvol_pslope", "-0.2", "DEBUG", CVAR_NoShadow);

VCvarB r_shadowmap_fix_light_dist("r_shadowmap_fix_light_dist", false, "Move lights slightly away from surfaces?", /*CVAR_PreInit|*/CVAR_Archive|CVAR_NoShadow);
VCvarI r_shadowmap_sprshadows("r_shadowmap_sprshadows", "2", "Render shadows from sprites (0:none;1:non-rotational;2:all)?", /*CVAR_PreInit|*/CVAR_Archive);

static VCvarF r_shadowmap_quality_distance("r_shadowmap_quality_distance", "512", "If the camera is further than this (from the light sphere), no shadowmap bluring will be done.", CVAR_Archive);
static VCvarI r_shadowmap_small_light_divisor("r_shadowmap_small_light_divisor", "20", "If visible light rectangle is smaller than view width divided by this, no shadowmap bluring will be done.", CVAR_Archive);


extern VCvarI gl_shadowmap_blur;

struct IntVarSaver {
  VCvarI *cvar;
  int origValue;
  inline IntVarSaver (VCvarI &acvar) noexcept : cvar(&acvar), origValue(acvar.asInt()) {}
  inline ~IntVarSaver () noexcept { if (cvar) { cvar->Set(origValue); cvar = nullptr; } }
  VV_DISABLE_COPY(IntVarSaver)
};


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderLightShadows
//
//  WARNING! may modify `Pos`
//
//==========================================================================
void VRenderLevelShadowVolume::RenderLightShadows (VEntity *ent, vuint32 dlflags, const refdef_t *RD,
                                                   const VViewClipper *Range,
                                                   TVec &Pos, float Radius, float LightMin, vuint32 Color,
                                                   TVec coneDir, float coneAngle, bool forceRender)
{
  (void)RD; (void)Range;
  if (Radius <= LightMin || gl_dbg_wireframe) return;

  //if (PortalDepth > 0) { GCon->Logf(NAME_Debug, "skipped portal lighting (depth=%d)", PortalDepth); return; }

  if (!forceRender) {
    if (r_max_lights >= 0 && LightsRendered >= r_max_lights) {
      // check limits
      if (!DynamicLights || (r_dynlight_minimum >= 0 && DynLightsRendered >= r_dynlight_minimum)) return;
    }
  }

  const bool useShadowMaps = (r_shadowmaps.asBool() && Drawer->CanRenderShadowMaps());

  //if (useShadowMaps && IsShadowsEnabled() && !(dlflags&dlight_t::NoShadow)) Drawer->PrepareShadowMaps(Radius);

  //TODO: we can reuse collected surfaces in next passes
  LitCalcBBox = true;
  CurrLightNoGeoClip = (dlflags&dlight_t::NoGeoClip);
  CurrLightCalcUnstuck = (useShadowMaps && r_shadowmap_fix_light_dist);
  if (!CalcLightVis(Pos, Radius-LightMin, coneDir, coneAngle)) return;

  if (!LitSurfaceHit /*&& !r_models*/) return; // no lit surfaces/subsectors, and no need to light models, so nothing to do

  // if our light is in frustum, ignore any out-of-frustum polys
  CurrLightInFrustum = (r_advlight_opt_frustum_full && Drawer->viewfrustum.checkSphere(Pos, Radius /*-LightMin+4.0f*/));
  CurrLightInFront = (r_advlight_opt_frustum_back && Drawer->viewfrustum.checkSphere(Pos, Radius, TFrustum::NearBit));

  bool allowShadows = doShadows;

  // if we want model shadows, always do full rendering
  //FIXME: we shoud check if we have any model that can cast shadow instead
  //FIXME: also, models can be enabled, but we may not have any models loaded
  //FIXME: for now, `doShadows` only set to false for very small lights (radius<8), so it doesn't matter
  if (!allowShadows && r_draw_mobjs && r_models && r_model_shadows) allowShadows = true;

  if (dlflags&dlight_t::NoShadow) allowShadows = false;

  if (!IsShadowsEnabled()) allowShadows = false;

  // set if the projected light rectangle is less than width/20 pixels
  bool smallLight = false;

  // check distance
  if (allowShadows) {
    const int smallDivisor = r_shadowmap_small_light_divisor.asInt();
    const int smallDim = (smallDivisor > 0 ? Drawer->getWidth()/smallDivisor : 0);
    int prjdim = r_light_shadow_min_proj_dimension.asInt()*Drawer->getWidth()/1024;
    if (prjdim >= 9) {
      //const float ldist = (Drawer->vieworg-Pos).lengthSquared();
      int lw, lh;
      if (!CalcScreenLightDimensions(Pos, Radius, &lw, &lh)) return; // this light is invisible
      smallLight = (smallDim > 0 && lw <= smallDim && lh < smallDim);
      if (lw < 16 || lh < 10) {
        allowShadows = false;
      } else {
        const float distsq = (Drawer->vieworg-Pos).lengthSquared();
             if (distsq >= 4200.0f*4200.0f) prjdim *= 3;
        else if (distsq >= 3500.0f*3500.0f) prjdim *= 2;
        else if (distsq >= 2900.0f*2900.0f) prjdim += prjdim/2;
        if (distsq >= 3100.0f*3100.0f) {
          if (lw < prjdim || lh < prjdim) allowShadows = false;
        } else {
          if (lw < prjdim && lh < prjdim) allowShadows = false;
        }
      }
      //GCon->Logf(NAME_Debug, "light at (%g,%g,%g); radius=%g; size=(%dx%d); shadows=%s; dist=%g", Pos.x, Pos.y, Pos.z, Radius, lw, lh, (allowShadows ? "tan" : "ona"), Drawer->vieworg.distanceTo(Pos));
    } else if (smallDim > 0) {
      int lw, lh;
      if (!CalcScreenLightDimensions(Pos, Radius, &lw, &lh)) return; // this light is invisible
      smallLight = (smallDim > 0 && lw <= smallDim && lh < smallDim);
    }
  }

  // if we don't need shadows, and no visible subsectors were hit, we have nothing to do here
  if (!allowShadows && (!HasLightIntersection || !LitSurfaceHit)) return;

  if (!allowShadows && dbg_adv_light_notrace_mark) {
    //Color = 0xffff0000U;
    Color = 0xffff00ffU; // purple; it should be very noticeable
  }

  ++LightsRendered;
  if (DynamicLights) ++DynLightsRendered;

  CurrLightRadius = Radius; // we need full radius, not modified
  CurrLightColor = Color;

  const bool optimiseScissor = r_advlight_opt_scissor.asBool();

  //  0 if scissor is empty
  // -1 if scissor has no sense (should not be used)
  //  1 if scissor is set
  int hasScissor = 1;
  int scoord[4];
  bool checkModels = false;

  //GCon->Logf("LBB:(%f,%f,%f)-(%f,%f,%f)", LitBBox[0], LitBBox[1], LitBBox[2], LitBBox[3], LitBBox[4], LitBBox[5]);

  if (!useShadowMaps && allowShadows) {
    // setup light scissor rectangle
    if (optimiseScissor) {
      hasScissor = Drawer->SetupLightScissor(Pos, Radius-LightMin, scoord, (r_advlight_opt_optimise_scissor ? LitBBox : nullptr));
      if (hasScissor <= 0) {
        // something is VERY wrong (-1), or scissor is empty (0)
        Drawer->ResetScissor();
        if (!hasScissor && r_advlight_opt_optimise_scissor && !r_models) return; // empty scissor
        checkModels = r_advlight_opt_optimise_scissor;
        hasScissor = 0;
        scoord[0] = scoord[1] = 0;
        scoord[2] = Drawer->getWidth();
        scoord[3] = Drawer->getHeight();
      } else {
        if (scoord[0] == 0 && scoord[1] == 0 && scoord[2] == Drawer->getWidth() && scoord[3] == Drawer->getHeight()) {
          hasScissor = 0;
        }
      }
    }
  } else {
    Drawer->ResetScissor();
    hasScissor = 0;
    scoord[0] = scoord[1] = 0;
    scoord[2] = Drawer->getWidth();
    scoord[3] = Drawer->getHeight();
  }

  // if there are no lit surfaces oriented away from camera, it cannot possibly be in shadow volume
  // nope, it is wrong
  //bool useZPass = !HasBackLit;
  //if (useZPass) GCon->Log("*** ZPASS");
  //useZPass = false;
  bool useZPass = false;

  /* nope
  if ((CurrLightPos-Drawer->vieworg).lengthSquared() > CurrLightRadius*CurrLightRadius) {
    useZPass = true;
  }
  */

  // one-sided sprites can cast shadows, because why not?
  // this way, we may cast shadow from most decorations
  BuildMobjsInCurrLight(allowShadows, (useShadowMaps && allowShadows));

  // if we want to scissor on geometry, check if any lit model is out of our light bbox.
  // stop right here! say, is there ANY reason to not limit light box with map geometry?
  // after all, most of the things that can receive light is contained inside a map.
  // still, we may miss some lighting on models from flying lights that cannot touch
  // any geometry at all. to somewhat ease this case, rebuild light box when the light
  // didn't touched anything.
  if (optimiseScissor && allowShadows && checkModels && !useShadowMaps) {
    if (mobjsInCurrLightModels.length() == 0) return; // nothing to do, as it is guaranteed that light cannot touch map geometry
    float xbbox[6] = {0};
    /*
    xbbox[0+0] = LitBBox[0].x;
    xbbox[0+1] = LitBBox[0].y;
    xbbox[0+2] = LitBBox[0].z;
    xbbox[3+0] = LitBBox[1].x;
    xbbox[3+1] = LitBBox[1].y;
    xbbox[3+2] = LitBBox[1].z;
    */
    bool wasHit = false;
    for (auto &&ment : mobjsInCurrLightModels) {
      if (ment == ViewEnt && (!r_chasecam || ViewEnt != cl->MO)) continue; // don't draw camera actor
      // skip things in subsectors that are not visible
      const int SubIdx = (int)(ptrdiff_t)(ment->SubSector-Level->Subsectors);
      if (!IsSubsectorLitBspVis(SubIdx)) continue;
      const float eradius = ment->GetRenderRadius();
      if (eradius < 1) continue;
      if (!HasEntityAliasModel(ment)) continue;
      // assume that it is not bigger than its radius
      float zup, zdown;
      if (ment->Height < 2) {
        //GCon->Logf("  <%s>: height=%f; radius=%f", *ment->GetClass()->GetFullName(), ment->Height, ment->Radius);
        zup = eradius;
        zdown = eradius;
      } else {
        zup = ment->Height;
        zdown = 0;
      }
      if (wasHit) {
        xbbox[0+0] = min2(xbbox[0+0], ment->Origin.x-eradius);
        xbbox[0+1] = min2(xbbox[0+1], ment->Origin.y-eradius);
        xbbox[0+2] = min2(xbbox[0+2], ment->Origin.z-zup);
        xbbox[3+0] = max2(xbbox[3+0], ment->Origin.x+eradius);
        xbbox[3+1] = max2(xbbox[3+1], ment->Origin.y+eradius);
        xbbox[3+2] = max2(xbbox[3+2], ment->Origin.z+zdown);
      } else {
        wasHit = true;
        xbbox[0+0] = ment->Origin.x-eradius;
        xbbox[0+1] = ment->Origin.y-eradius;
        xbbox[0+2] = ment->Origin.z-zup;
        xbbox[3+0] = ment->Origin.x+eradius;
        xbbox[3+1] = ment->Origin.y+eradius;
        xbbox[3+2] = ment->Origin.z+zdown;
      }
    }
    if (wasHit &&
        (xbbox[0+0] < LitBBox[0].x || xbbox[0+1] < LitBBox[0].y || xbbox[0+2] < LitBBox[0].z ||
         xbbox[3+0] > LitBBox[1].x || xbbox[3+1] > LitBBox[1].y || xbbox[3+2] > LitBBox[1].z))
    {
      /*
      GCon->Logf("fixing light bbox; old=(%f,%f,%f)-(%f,%f,%f); new=(%f,%f,%f)-(%f,%f,%f)",
        LitBBox[0].x, LitBBox[0].y, LitBBox[0].z,
        LitBBox[1].x, LitBBox[1].y, LitBBox[1].z,
        xbbox[0], xbbox[1], xbbox[2], xbbox[3], xbbox[4], xbbox[5]);
      */
      LitBBox[0].x = min2(xbbox[0+0], LitBBox[0].x);
      LitBBox[0].y = min2(xbbox[0+1], LitBBox[0].y);
      LitBBox[0].z = min2(xbbox[0+2], LitBBox[0].z);
      LitBBox[1].x = max2(xbbox[3+0], LitBBox[1].x);
      LitBBox[1].y = max2(xbbox[3+1], LitBBox[1].y);
      LitBBox[1].z = max2(xbbox[3+2], LitBBox[1].z);
      hasScissor = Drawer->SetupLightScissor(Pos, Radius-LightMin, scoord, LitBBox);
      if (hasScissor <= 0) {
        // something is VERY wrong (-1), or scissor is empty (0)
        Drawer->ResetScissor();
        if (!hasScissor) return; // empty scissor
        hasScissor = 0;
        scoord[0] = scoord[1] = 0;
        scoord[2] = Drawer->getWidth();
        scoord[3] = Drawer->getHeight();
      } else {
        if (scoord[0] == 0 && scoord[1] == 0 && scoord[2] == Drawer->getWidth() && scoord[3] == Drawer->getHeight()) {
          hasScissor = 0;
        }
      }
    }
  }

  CollectLightShadowSurfaces(allowShadows);

  // used in flats checker
  (void)fsecCounterGen();

  if (allowShadows) {
    const bool doModels = (r_models.asBool() && r_model_shadows.asBool());
    // need shadows
    if (useShadowMaps) {
      // shadowmaps
      if (allowShadows && CurrLightCalcUnstuck) {
        if (CurrLightUnstuckPos != CurrLightPos) {
          #if 0
          const TVec move = CurrLightUnstuckPos-CurrLightPos;
          GCon->Logf(NAME_Debug, "light at pos (%g,%g,%g) moved by (%g,%g,%g) (radius=%g)", CurrLightPos.x, CurrLightPos.y, CurrLightPos.z, move.x, move.y, move.z, CurrLightRadius);
          #endif
          CurrLightPos = CurrLightUnstuckPos;
        }
      }
      Drawer->BeginLightShadowMaps(CurrLightPos, CurrLightRadius);
      Drawer->UploadShadowSurfaces(shadowSurfacesSolid, shadowSurfacesMasked);
      const int spShad = r_shadowmap_sprshadows.asInt();
      if (doModels) Drawer->BeginModelShadowMaps(CurrLightPos, CurrLightRadius);
      for (unsigned fc = 0; fc < 6; ++fc) {
        Drawer->SetupLightShadowMap(fc);
        Drawer->RenderShadowMaps(shadowSurfacesSolid, shadowSurfacesMasked);
        // force monster shadows as sprites?
        if (spShad > 0) {
          RenderMobjSpriteShadowMap(ent, fc, spShad, dlflags);
        }
        if (doModels) {
          Drawer->SetupModelShadowMap(fc);
          RenderMobjsShadowMap(ent, fc, dlflags);
        }
      }
      if (doModels) Drawer->EndModelShadowMaps();
      Drawer->EndLightShadowMaps();
    } else {
      // shadow volumes
      Drawer->BeginLightShadowVolumes(CurrLightPos, CurrLightRadius, useZPass, hasScissor, scoord, &refdef);
      if (r_shadowvol_use_pofs) Drawer->GLPolygonOffsetEx(r_shadowvol_pslope, -r_shadowvol_pofs); // pull forward
      RenderShadowSurfaceList();
      if (doModels) {
        Drawer->BeginModelsShadowsPass(CurrLightPos, CurrLightRadius);
        RenderMobjsShadow(ent, dlflags);
        Drawer->EndModelsShadowsPass();
      }
      if (r_shadowvol_use_pofs) Drawer->GLDisableOffset();
      Drawer->EndLightShadowVolumes();
    }
  }

  // k8: the question is: why we are rendering surfaces instead
  //     of simply render a light circle? shadow volumes should
  //     take care of masking the area, so simply rendering a
  //     circle should do the trick.
  // k8: answering to the silly younger ketmar: because we cannot
  //     read depth info, and we need normals to calculate light
  //     intensity, and so on.

  IntVarSaver blursv(gl_shadowmap_blur);
  if (allowShadows && useShadowMaps) {
    if (smallLight) {
      // don't do any blur for visually small lights
      gl_shadowmap_blur = 0;
    } else {
      float ldd = r_shadowmap_quality_distance.asFloat();
      if (ldd > 0.0f) {
        ldd += CurrLightRadius; // so distance will be to the light sphere edge
        if ((CurrLightPos-Drawer->vieworg).lengthSquared() >= ldd*ldd) {
          gl_shadowmap_blur = 0;
        }
      }
    }
  }

  // draw light
  Drawer->BeginLightPass(CurrLightPos, CurrLightRadius, LightMin, Color, CurrLightSpot, CurrLightConeDir, CurrLightConeAngle, allowShadows);
  RenderLightSurfaceList();
  Drawer->EndLightPass();

  Drawer->BeginModelsLightPass(CurrLightPos, CurrLightRadius, LightMin, Color, CurrLightSpot, CurrLightConeDir, CurrLightConeAngle, allowShadows);
  RenderMobjsLight(ent, dlflags);
  Drawer->EndModelsLightPass();

  //if (hasScissor) Drawer->DebugRenderScreenRect(scoord[0], scoord[1], scoord[2], scoord[3], 0x7f007f00);

  /*if (hasScissor)*/ Drawer->ResetScissor();
}
