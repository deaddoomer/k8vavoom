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
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
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
#include "gamedefs.h"
#include "r_local.h"


extern VCvarF r_lights_radius;
extern VCvarI r_max_lights;
//extern VCvarB r_disable_world_update;

static VCvarB r_advlight_sort_static("r_advlight_sort_static", true, "Sort visible static lights, so nearby lights will be rendered first?", CVAR_Archive|CVAR_PreInit);
static VCvarB r_advlight_sort_dynamic("r_advlight_sort_dynamic", true, "Sort visible dynamic lights, so nearby lights will be rendered first?", CVAR_Archive|CVAR_PreInit);
static VCvarB r_advlight_flood_check("r_advlight_flood_check", true, "Check light visibility with floodfill before trying to render it?", CVAR_Archive|CVAR_PreInit);

static VCvarB dbg_adv_show_light_count("dbg_adv_show_light_count", false, "Show number of rendered lights?", CVAR_PreInit);
static VCvarB dbg_adv_show_light_seg_info("dbg_adv_show_light_seg_info", false, "Show totals of rendered light/shadow segments?", CVAR_PreInit);

static VCvarI dbg_adv_force_static_lights_radius("dbg_adv_force_static_lights_radius", "0", "Force static light radius.", CVAR_PreInit);
static VCvarI dbg_adv_force_dynamic_lights_radius("dbg_adv_force_dynamic_lights_radius", "0", "Force dynamic light radius.", CVAR_PreInit);


struct StLightInfo {
  VRenderLevelShared::light_t *stlight; // light
  float distSq; // distance
};

struct DynLightInfo {
  dlight_t *l; // light
  float distSq; // distance
};


extern "C" {
  static int stLightCompare (const void *aa, const void *bb, void *udata) {
    if (aa == bb) return 0;
    const StLightInfo *a = (const StLightInfo *)aa;
    const StLightInfo *b = (const StLightInfo *)bb;
    //TODO: consider radius too?
    if (a->distSq < b->distSq) return -1;
    if (a->distSq > b->distSq) return 1;
    return 0;
  }

  static int dynLightCompare (const void *aa, const void *bb, void *udata) {
    if (aa == bb) return 0;
    const DynLightInfo *a = (const DynLightInfo *)aa;
    const DynLightInfo *b = (const DynLightInfo *)bb;
    //TODO: consider radius too?
    if (a->distSq < b->distSq) return -1;
    if (a->distSq > b->distSq) return 1;
    return 0;
  }
}


//==========================================================================
//
//  VAdvancedRenderLevel::VAdvancedRenderLevel
//
//==========================================================================
VAdvancedRenderLevel::VAdvancedRenderLevel (VLevel *ALevel)
  : VRenderLevelShared(ALevel)
{
  NeedsInfiniteFarClip = true;
  mIsAdvancedRenderer = true;
}


//==========================================================================
//
//  VAdvancedRenderLevel::~VAdvancedRenderLevel
//
//==========================================================================
VAdvancedRenderLevel::~VAdvancedRenderLevel () {
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderTranslucentWallsDecals
//
//==========================================================================
void VAdvancedRenderLevel::RenderTranslucentWallsDecals () {
  if (traspFirst >= traspUsed) return;
  trans_sprite_t *twi = &trans_sprites[traspFirst];
  for (int f = traspFirst; f < traspUsed; ++f, ++twi) {
    if (twi->type) continue; // not a wall
    if (twi->Alpha >= 1.0f) continue; // not a translucent
    check(twi->surf);
    Drawer->DrawTranslucentPolygonDecals(twi->surf, twi->Alpha, twi->Additive);
  }
}


//==========================================================================
//
//  VAdvancedRenderLevel::RenderScene
//
//==========================================================================
void VAdvancedRenderLevel::RenderScene (const refdef_t *RD, const VViewClipper *Range) {
  if (!Drawer->SupportsAdvancedRendering()) Host_Error("Advanced rendering not supported by graphics card");

  r_viewleaf = Level->PointInSubsector(vieworg);

  TransformFrustum();

  Drawer->SetupViewOrg();

#if 0
  {
    VMatrix4 model, proj;
    Drawer->GetModelMatrix(model);
    Drawer->GetProjectionMatrix(proj);

    VMatrix4 comb;
    comb.ModelProjectCombine(model, proj);

    TPlane planes[6];
    //VMatrix4::ExtractFrustum(model, proj, planes);
    comb.ExtractFrustum(planes);


    GCon->Log("=== FRUSTUM ===");
    for (unsigned f = 0; f < 6; ++f) {
      const float len = planes[f].normal.length();
      planes[f].normal /= len;
      planes[f].dist /= len;
      //planes[f].Normalise();
      GCon->Logf("  GL plane #%u: (%9f,%9f,%9f) : %f", f, planes[f].normal.x, planes[f].normal.y, planes[f].normal.z, planes[f].dist);
      GCon->Logf("  MY plane #%u: (%9f,%9f,%9f) : %f", f, view_frustum.planes[f].normal.x, view_frustum.planes[f].normal.y, view_frustum.planes[f].normal.z, view_frustum.planes[f].dist);
    }

    // we aren't interested in far plane
    for (unsigned f = 0; f < 4; ++f) {
      view_frustum.planes[f] = planes[f];
      view_frustum.planes[f].clipflag = 1U<<f;
    }
    // near plane for reverse z is "far"
    if (Drawer->CanUseRevZ()) {
      view_frustum.planes[TFrustum::Back] = planes[5];
    } else {
      view_frustum.planes[TFrustum::Back] = planes[4];
    }
    view_frustum.planes[TFrustum::Back].clipflag = TFrustum::BackBit;
    view_frustum.planeCount = 5;
    //check(view_frustum.planes[4].PointOnSide(vieworg));
  }
#endif

  //ClearQueues(); // moved to `RenderWorld()`
  //MarkLeaves();
  //if (!MirrorLevel && !r_disable_world_update) UpdateWorld(RD, Range);

  RenderWorld(RD, Range);
  BuildVisibleObjectsList();

  RenderMobjsAmbient();

  //GCon->Log("***************** RenderScene *****************");
  //FIXME: mirrors can use stencils, and advlight too...
  if (!MirrorLevel) {
    Drawer->BeginShadowVolumesPass();

    linetrace_t Trace;
    TVec Delta;

    CurrLightsNumber = 0;
    CurrShadowsNumber = 0;
    AllLightsNumber = 0;
    AllShadowsNumber = 0;

    const float rlightraduisSq = (r_lights_radius < 1 ? 2048*2048 : r_lights_radius*r_lights_radius);
    //const bool hasPVS = Level->HasPVS();

    static TFrustum frustum;
    static TFrustumParam fp;

    TPlane backPlane;
    backPlane.SetPointNormal3D(vieworg, viewforward);

    LightsRendered = 0;

    if (!FixedLight && r_static_lights && r_max_lights != 0) {
      if (!staticLightsFiltered) RefilterStaticLights();

      // sort lights by distance to player, so faraway lights won't disable nearby ones
      static TArray<StLightInfo> visstatlights;
      if (visstatlights.length() < Lights.length()) visstatlights.setLength(Lights.length());
      unsigned visstatlightCount = 0;

      light_t *stlight = Lights.ptr();
      for (int i = Lights.length(); i--; ++stlight) {
        //if (!Lights[i].radius) continue;
        if (!stlight->active || stlight->radius < 8) continue;

        // don't do lights that are too far away
        Delta = stlight->origin-vieworg;
        const float distSq = Delta.lengthSquared();

        // if the light is behind a view, drop it if it is further than light radius
        if (distSq >= stlight->radius*stlight->radius) {
          if (distSq > rlightraduisSq || backPlane.PointOnSide(stlight->origin)) continue; // too far away
          if (fp.needUpdate(vieworg, viewangles)) {
            fp.setup(vieworg, viewangles, viewforward, viewright, viewup);
            frustum.setup(clip_base, fp, false); //true, r_lights_radius);
          }
          if (!frustum.checkSphere(stlight->origin, stlight->radius)) {
            // out of frustum
            continue;
          }
        }

        if (r_advlight_flood_check && !CheckBSPVisibility(stlight->origin, stlight->radius)) {
          //GCon->Logf("STATIC DROP: visibility check");
          continue;
        }

        StLightInfo &sli = visstatlights[visstatlightCount++];
        sli.stlight = stlight;
        sli.distSq = distSq;
      }

      // sort lights, so nearby ones will be rendered first
      if (visstatlightCount > 0) {
        if (r_advlight_sort_static) {
          timsort_r(visstatlights.ptr(), visstatlightCount, sizeof(StLightInfo), &stLightCompare, nullptr);
        }
        for (const StLightInfo *sli = visstatlights.ptr(); visstatlightCount--; ++sli) {
          VEntity *own = (sli->stlight->owner && sli->stlight->owner->IsA(VEntity::StaticClass()) ? sli->stlight->owner : nullptr);
          vuint32 flags = (own && R_ModelNoSelfShadow(own->GetClass()->Name) ? dlight_t::NoSelfShadow : 0);
          //if (own) GCon->Logf("STLOWN: %s", *own->GetClass()->GetFullName());
          RenderLightShadows(own, flags, RD, Range, sli->stlight->origin, (dbg_adv_force_static_lights_radius > 0 ? dbg_adv_force_static_lights_radius : sli->stlight->radius), 0.0f, sli->stlight->color, true);
        }
      }
    }

    int rlStatic = LightsRendered;

    if (!FixedLight && r_dynamic_lights && r_max_lights != 0) {
      static TArray<DynLightInfo> visdynlights;
      if (visdynlights.length() < MAX_DLIGHTS) visdynlights.setLength(MAX_DLIGHTS);
      unsigned visdynlightCount = 0;

      dlight_t *l = DLights;
      for (int i = MAX_DLIGHTS; i--; ++l) {
        if (l->radius < l->minlight+8 || l->die < Level->Time) continue;

        // don't do lights that are too far away
        Delta = l->origin-vieworg;
        const float distSq = Delta.lengthSquared();

        // if the light is behind a view, drop it if it is further than light radius
        if (distSq >= l->radius*l->radius) {
          if (distSq > rlightraduisSq || backPlane.PointOnSide(l->origin)) continue; // too far away
          if (fp.needUpdate(vieworg, viewangles)) {
            fp.setup(vieworg, viewangles, viewforward, viewright, viewup);
            frustum.setup(clip_base, fp, false); //true, r_lights_radius);
          }
          if (!frustum.checkSphere(l->origin, l->radius)) {
            // out of frustum
            continue;
          }
        }

        DynLightInfo &dli = visdynlights[visdynlightCount++];
        dli.l = l;
        dli.distSq = distSq;
      }

      // sort lights, so nearby ones will be rendered first
      if (visdynlightCount > 0) {
        if (r_advlight_sort_dynamic) {
          timsort_r(visdynlights.ptr(), visdynlightCount, sizeof(DynLightInfo), &dynLightCompare, nullptr);
        }
        for (const DynLightInfo *dli = visdynlights.ptr(); visdynlightCount--; ++dli) {
          VEntity *own = (dli->l->Owner && dli->l->Owner->IsA(VEntity::StaticClass()) ? (VEntity *)dli->l->Owner : nullptr);
          if (own && R_ModelNoSelfShadow(own->GetClass()->Name)) dli->l->flags |= dlight_t::NoSelfShadow;
          RenderLightShadows(own, dli->l->flags, RD, Range, dli->l->origin, (dbg_adv_force_dynamic_lights_radius > 0 ? dbg_adv_force_dynamic_lights_radius : dli->l->radius), dli->l->minlight, dli->l->color, true, dli->l->coneDirection, dli->l->coneAngle);
        }
      }
    }

    if (dbg_adv_show_light_count) {
      GCon->Logf("total lights per frame: %d (%d static, %d dynamic)", LightsRendered, rlStatic, LightsRendered-rlStatic);
    }

    if (dbg_adv_show_light_seg_info) {
      GCon->Logf("rendered %d shadow segs, and %d light segs", AllShadowsNumber, AllLightsNumber);
    }
  }

  Drawer->DrawWorldTexturesPass();
  RenderMobjsTextures();
  RenderTranslucentWallsDecals();

  Drawer->DrawWorldFogPass();
  RenderMobjsFog();
  Drawer->EndFogPass();

  RenderMobjs(RPASS_NonShadow);

  DrawParticles();

  DrawTranslucentPolys();

  RenderPortals();
}
