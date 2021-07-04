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
#include "../gamedefs.h"
#include "../psim/p_entity.h"
#include "../psim/p_levelinfo.h"
#include "r_local.h"


extern VCvarI r_max_lights;
//extern VCvarB r_disable_world_update;

static VCvarB r_advlight_sort_static("r_advlight_sort_static", true, "Sort visible static lights, so nearby lights will be rendered first?", CVAR_Archive|CVAR_PreInit);
static VCvarB r_advlight_sort_dynamic("r_advlight_sort_dynamic", true, "Sort visible dynamic lights, so nearby lights will be rendered first?", CVAR_Archive|CVAR_PreInit);
// no need to do this, because light rendering will do it again anyway
// yet it seems to be slightly faster for complex maps with alot of static lights
// 'cmon, `RenderLightShadows()` builds lightvis, and checks this
static VCvarB r_advlight_flood_check("r_advlight_flood_check", false, "Check static light visibility with floodfill before trying to render it?", CVAR_Archive|CVAR_PreInit);

static VCvarB dbg_adv_show_light_count("dbg_adv_show_light_count", false, "Show number of rendered lights?", CVAR_PreInit);
static VCvarB dbg_adv_show_light_seg_info("dbg_adv_show_light_seg_info", false, "Show totals of rendered light/shadow segments?", CVAR_PreInit);

static VCvarI dbg_adv_force_static_lights_radius("dbg_adv_force_static_lights_radius", "0", "Force static light radius.", CVAR_PreInit);
static VCvarI dbg_adv_force_dynamic_lights_radius("dbg_adv_force_dynamic_lights_radius", "0", "Force dynamic light radius.", CVAR_PreInit);


struct StLightInfo {
  VRenderLevelShared::light_t *stlight; // light
  float distSq; // distance from view origin to the nearest point on the light sphere
  float zofs; // origin z offset
};

struct DynLightInfo {
  dlight_t *l; // light
  float distSq; // distance from view origin to the nearest point on the light sphere
  //float zofs; // origin z offset
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
//  VRenderLevelShadowVolume::VRenderLevelShadowVolume
//
//==========================================================================
VRenderLevelShadowVolume::VRenderLevelShadowVolume (VLevel *ALevel)
  : VRenderLevelShared(ALevel)
  , LightsRendered(0)
  , DynLightsRendered(0)
  , DynamicLights(false)
  , fsecCounter(0)
{
  mIsShadowVolumeRenderer = true;
  float mt = clampval(r_fade_mult_advanced.asFloat(), 0.0f, 16.0f);
  if (mt <= 0.0f) mt = 1.0f;
  VDrawer::LightFadeMult = mt;
}


//==========================================================================
//
//  VRenderLevelShadowVolume::~VRenderLevelShadowVolume
//
//==========================================================================
VRenderLevelShadowVolume::~VRenderLevelShadowVolume () {
}


//==========================================================================
//
//  VRenderLevelShadowVolume::IsTouchedByCurrLight
//
//==========================================================================
bool VRenderLevelShadowVolume::IsTouchedByCurrLight (const VEntity *ent) const noexcept {
  const float clr = CurrLightRadius;
  //if (clr < 2) return false; // arbitrary number
  const TVec eofs = CurrLightPos-ent->Origin;
  const float edist = ent->Radius+clr;
  if (eofs.Length2DSquared() >= edist*edist) return false;
  // if light is higher than thing height, assume that the thing is not touched
  if (eofs.z >= clr+ent->Height) return false;
  // if light is lower than the thing, assume that the thing is not touched
  if (eofs.z <= -clr) return false;
  return true;
}


static TArray<StLightInfo> visstatlights;
static TArray<DynLightInfo> visdynlights;

static unsigned visibleStaticLightCount;
static unsigned visibleDynamicLightCount;
static unsigned visdynlightCount;
static unsigned visstatlightCount;

static TFrustum advLightFrustum;
static TFrustumParam advLightFp;


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderSceneStaticLights
//
//==========================================================================
void VRenderLevelShadowVolume::RenderSceneStaticLights (const refdef_t *RD, const VViewClipper *Range) {
  if (FixedLight || !r_static_lights || r_max_lights == 0 || Lights.length() == 0) return;

  linetrace_t Trace;
  TVec Delta;

  // do not render lights further than `gl_maxdist`
  const float maxLightDist = GetLightMaxDistDef();
  const float rlightraduisSq = maxLightDist*maxLightDist;

  // no need to do this, because light rendering will do it again anyway
  const bool checkLightVis = r_advlight_flood_check.asBool();

  TPlane backPlane;
  backPlane.SetPointNormal3D(Drawer->vieworg, Drawer->viewforward);

  DynamicLights = false;

  //if (!FixedLight && r_static_lights && r_max_lights != 0) {
  if (!staticLightsFiltered) RefilterStaticLights();

  // sort lights by distance to player, so faraway lights won't disable nearby ones
  if (visstatlights.length() < Lights.length()) visstatlights.setLength(Lights.length());

  light_t *stlight = Lights.ptr();
  for (int i = Lights.length(); i--; ++stlight) {
    //if (!Lights[i].radius) continue;
    if (!stlight->active) continue;

    // update `DLTYPE_Sector`
    if (stlight->levelSector && stlight->sectorLightLevel != stlight->levelSector->params.lightlevel) {
      stlight->sectorLightLevel = stlight->levelSector->params.lightlevel;
      const float intensity = clampval(stlight->sectorLightLevel*(fabsf(stlight->levelScale)*0.125f), 0.0f, 255.0f);
      stlight->radius = VLevelInfo::GZSizeToRadius(intensity, (stlight->levelScale < 0.0f), 2.0f);
    }

    if (stlight->radius < 8.0f) continue;

    if (stlight->leafnum < 0 || stlight->leafnum >= Level->NumSubsectors) {
      stlight->leafnum = (int)(ptrdiff_t)(Level->PointInSubsector(stlight->origin)-Level->Subsectors);
    }

    // drop invisible lights without further processing
    if (stlight->dlightframe != currDLightFrame) continue;

    TVec lorg = stlight->origin;

    // don't do lights that are too far away
    Delta = lorg-Drawer->vieworg;
    const float distSq = Delta.lengthSquared();
    const float srq = stlight->radius*stlight->radius;

    // if the light is behind a view, drop it if it is further than light radius
    if (distSq >= srq) {
      if (distSq-srq > rlightraduisSq || backPlane.SphereOnSide(lorg, stlight->radius)) continue; // too far away
      if (advLightFp.needUpdate(Drawer->vieworg, Drawer->viewangles)) {
        advLightFp.setup(Drawer->vieworg, Drawer->viewangles, Drawer->viewforward, Drawer->viewright, Drawer->viewup);
        advLightFrustum.setup(clip_base, advLightFp, false); //true, maxLightDist);
      }
      if (!advLightFrustum.checkSphere(lorg, stlight->radius)) {
        // out of frustum
        continue;
      }
    }

    // drop lights inside sectors without height
    const sector_t *sec = Level->Subsectors[stlight->leafnum].sector;
    if (!CheckValidLightPosRough(lorg, sec)) continue;
    // 'cmon, `RenderLightShadows()` builds lightvis, and checks this
    if (checkLightVis && !(IsBspVisSector(stlight->leafnum) || CheckBSPVisibilityBox(lorg, stlight->radius, &Level->Subsectors[stlight->leafnum]))) continue;

    StLightInfo &sli = visstatlights[visstatlightCount++];
    sli.stlight = stlight;
    sli.distSq = distSq-srq;
    sli.zofs = lorg.z-stlight->origin.z;
  }

  // sort lights, so nearby ones will be rendered first
  if (visstatlightCount > 0) {
    visibleStaticLightCount = visstatlightCount;
    if (r_advlight_sort_static) {
      timsort_r(visstatlights.ptr(), visstatlightCount, sizeof(StLightInfo), &stLightCompare, nullptr);
    }
    //GCon->Logf(NAME_Debug, "=== %d static lights ===", visstatlightCount);
    for (const StLightInfo *sli = visstatlights.ptr(); visstatlightCount--; ++sli) {
      //VEntity *own = (sli->stlight->owner && sli->stlight->owner->IsA(VEntity::StaticClass()) ? sli->stlight->owner : nullptr);
      //VObject *ownobj = (sli->stlight->dynowner ? sli->stlight->dynowner : sli->stlight->ownerUId ? VObject::FindByUniqueId(sli->stlight->ownerUId) : nullptr);
      //VObject *ownobj = (sli->stlight->ownerUId ? VObject::FindByUniqueId(sli->stlight->ownerUId) : nullptr);
      //VObject *ownobj = (sli->stlight->ownerUId ? VObject::FindByUniqueId(sli->stlight->ownerUId) : nullptr);
      //VEntity *own = (ownobj && !ownobj->IsGoingToDie() && ownobj->IsA(VEntity::StaticClass()) ? (VEntity *)ownobj : nullptr);
      VEntity *own = nullptr;
      if (sli->stlight->ownerUId) {
        own = Level->GetEntityBySUId(sli->stlight->ownerUId);
        /*
        auto ownpp = suid2ent.find(sli->stlight->ownerUId);
        if (ownpp) own = *ownpp; //else GCon->Logf(NAME_Debug, "stlight owner with uid %u not found", sli->stlight->ownerUId);
        */
      }
      //vuint32 flags = (own && R_EntModelNoSelfShadow(own) ? dlight_t::NoSelfShadow : 0);
      vuint32 flags = sli->stlight->flags;
      if (own && R_EntModelNoSelfShadow(own)) flags |= dlight_t::NoSelfShadow;
      //if (own) GCon->Logf("STLOWN: %s", *own->GetClass()->GetFullName());
      TVec lorg = sli->stlight->origin;
      lorg.z += sli->zofs;
      RenderLightShadows(own, flags, RD, Range, lorg, (dbg_adv_force_static_lights_radius > 0 ? dbg_adv_force_static_lights_radius : sli->stlight->radius), 0.0f, sli->stlight->color, sli->stlight->coneDirection, sli->stlight->coneAngle);
    }
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderSceneDynamicLights
//
//==========================================================================
void VRenderLevelShadowVolume::RenderSceneDynamicLights (const refdef_t *RD, const VViewClipper *Range) {
  if (FixedLight || !r_dynamic_lights || r_max_lights == 0) return;

  linetrace_t Trace;
  TVec Delta;

  // do not render lights further than `gl_maxdist`
  const float maxLightDist = GetLightMaxDistDef();
  const float rlightraduisSq = maxLightDist*maxLightDist;

  TPlane backPlane;
  backPlane.SetPointNormal3D(Drawer->vieworg, Drawer->viewforward);

  //int rlStatic = LightsRendered;
  DynamicLights = true;

  //if (!FixedLight && r_dynamic_lights && r_max_lights != 0) {
  if (visdynlights.length() < MAX_DLIGHTS) visdynlights.setLength(MAX_DLIGHTS);

  dlight_t *l = DLights;
  for (int i = MAX_DLIGHTS; i--; ++l) {
    if (l->radius < l->minlight+8.0f || l->die < Level->Time) continue;

    TVec lorg = l->origin;

    // drop lights inside sectors without height
    /* it is not set here yet; why?! we should calc leafnum!
    const int leafnum = dlinfo[i].leafnum;
    GCon->Logf(NAME_Debug, "dl #%d: lfn=%d", i, leafnum);
    if (leafnum >= 0 && leafnum < Level->NumSubsectors) {
      const sector_t *sec = Level->Subsectors[leafnum].sector;
      if (!CheckValidLightPosRough(lorg, sec)) continue;
    }
    */

    // don't do lights that are too far away
    Delta = lorg-Drawer->vieworg;
    const float distSq = Delta.lengthSquared();
    const float srq = l->radius*l->radius;

    // if the light is behind a view, drop it if it is further than light radius
    if (distSq >= srq) {
      if (distSq-srq > rlightraduisSq || backPlane.SphereOnSide(lorg, l->radius)) continue; // too far away
      if (advLightFp.needUpdate(Drawer->vieworg, Drawer->viewangles)) {
        advLightFp.setup(Drawer->vieworg, Drawer->viewangles, Drawer->viewforward, Drawer->viewright, Drawer->viewup);
        advLightFrustum.setup(clip_base, advLightFp, false); //true, maxLightDist);
      }
      if (!advLightFrustum.checkSphere(lorg, l->radius)) {
        // out of frustum
        continue;
      }
    }

    DynLightInfo &dli = visdynlights[visdynlightCount++];
    dli.l = l;
    dli.distSq = distSq-srq;
    //dli.zofs = lorg.z-l->origin.z;
  }

  // sort lights, so nearby ones will be rendered first
  if (visdynlightCount > 0) {
    visibleDynamicLightCount = visdynlightCount;
    if (r_advlight_sort_dynamic) {
      timsort_r(visdynlights.ptr(), visdynlightCount, sizeof(DynLightInfo), &dynLightCompare, nullptr);
    }
    //GCon->Logf(NAME_Debug, "=== %d dynamic lights ===", visdynlightCount);
    for (const DynLightInfo *dli = visdynlights.ptr(); visdynlightCount--; ++dli) {
      //VEntity *own = (dli->l->Owner && dli->l->Owner->IsA(VEntity::StaticClass()) ? (VEntity *)dli->l->Owner : nullptr);
      VEntity *own = nullptr;
      if (dli->l->ownerUId) {
        //auto ownpp = suid2ent.find(dli->l->ownerUId);
        own = Level->GetEntityBySUId(dli->l->ownerUId);
        /*
        auto ownpp = suid2ent.find(dli->l->ownerUId);
        if (ownpp) own = *ownpp; //else GCon->Logf(NAME_Debug, "stlight owner with uid %u not found", sli->stlight->ownerUId);
        */
      }
      //if (own && R_EntModelNoSelfShadow(own)) dli->l->flags |= dlight_t::NoSelfShadow;
      vuint32 flags = dli->l->flags;
      if (own && R_EntModelNoSelfShadow(own)) flags |= dlight_t::NoSelfShadow;
      //TVec lorg = dli->l->origin;
      //lorg.z += dli->zofs;
      // always render player lights
      const bool forced = (own && own->IsPlayer());
      TVec lorg = dli->l->origin;
      RenderLightShadows(own, flags, RD, Range, lorg, (dbg_adv_force_dynamic_lights_radius > 0 ? dbg_adv_force_dynamic_lights_radius : dli->l->radius), dli->l->minlight, dli->l->color, dli->l->coneDirection, dli->l->coneAngle, forced);
    }
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderSceneLights
//
//==========================================================================
void VRenderLevelShadowVolume::RenderSceneLights (const refdef_t *RD, const VViewClipper *Range) {
  visibleStaticLightCount = 0;
  visibleDynamicLightCount = 0;
  visdynlightCount = 0;
  visstatlightCount = 0;

  //if (PortalDepth == 0 /*PortalUsingStencil != 0*/) return;

  //FIXME: portals can use stencils, and advlight too...
  // disable shadows in portals (for now)
  const bool oldForceDisableShadows = forceDisableShadows;
  if (PortalUsingStencil > 0) forceDisableShadows = true;

  //GCon->Log("***************** RenderScene *****************");
  MiniStopTimer profDrawSVol("ShadowVolumes", prof_r_bsp_world_render.asBool());
  Drawer->BeginShadowVolumesPass();

  LightsRendered = 0;
  DynLightsRendered = 0;

  RenderSceneStaticLights(RD, Range);
  RenderSceneDynamicLights(RD, Range);

  profDrawSVol.stopAndReport();

  if (dbg_adv_show_light_count) {
    GCon->Logf("total lights per frame: %d (%d static, %d dynamic)", LightsRendered, LightsRendered-DynLightsRendered, DynLightsRendered);
  }

  forceDisableShadows = oldForceDisableShadows;
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderScene
//
//==========================================================================
void VRenderLevelShadowVolume::RenderScene (const refdef_t *RD, const VViewClipper *Range) {
  if (!Drawer->SupportsShadowVolumeRendering()) {
    if (!Drawer->SupportsShadowMapRendering()) {
      Host_Error("Shadow volume rendering is not supported by your graphics card");
    }
    // force shadowmaps if shadow volumes are not supported
    if (!r_shadowmaps) {
      r_shadowmaps = true;
      GCon->Logf("Forced shadowmap renderer, because shadow volumes are not supported.");
    }
  }

  //r_viewleaf = Level->PointInSubsector(Drawer->vieworg); // moved to `PrepareWorldRender()`

  TransformFrustum();
  Drawer->SetupViewOrg();

  MiniStopTimer profRenderScene("RenderScene", IsAnyProfRActive());
  if (profRenderScene.isEnabled()) GCon->Log(NAME_Debug, "=== RenderScene ===");

  double totalRenderTime = 0.0;

  // we will collect rendered sectors, so we could collect things from them
  nextRenderedSectorsCounter();

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
      GCon->Logf("  MY plane #%u: (%9f,%9f,%9f) : %f", f, Drawer->viewfrustum.planes[f].normal.x, Drawer->viewfrustum.planes[f].normal.y, Drawer->viewfrustum.planes[f].normal.z, Drawer->viewfrustum.planes[f].dist);
    }

    // we aren't interested in far plane
    for (unsigned f = 0; f < 4; ++f) {
      Drawer->viewfrustum.planes[f] = planes[f];
      Drawer->viewfrustum.planes[f].clipflag = 1U<<f;
    }
    // near plane for reverse z is "far"
    if (Drawer->CanUseRevZ()) {
      Drawer->viewfrustum.planes[TFrustum::Back] = planes[5];
    } else {
      Drawer->viewfrustum.planes[TFrustum::Back] = planes[4];
    }
    Drawer->viewfrustum.planes[TFrustum::Back].clipflag = TFrustum::BackBit;
    Drawer->viewfrustum.planeCount = 5;
    //vassert(Drawer->viewfrustum.planes[4].PointOnSide(Drawer->vieworg));
  }
#endif

  //ClearQueues(); // moved to `PrepareWorldRender()`
  //MarkLeaves(); // moved to `PrepareWorldRender()`
  //if (!MirrorLevel && !r_disable_world_update) UpdateFakeSectors(); // moved to `PrepareWorldRender()`

  RenderCollectSurfaces(RD, Range);

  MiniStopTimer profBeforeDraw("BeforeDrawWorldSV", prof_r_bsp_world_render.asBool());
  Drawer->BeforeDrawWorldSV();
  totalRenderTime += profBeforeDraw.stopAndReport();

  MiniStopTimer profDrawAmbient("DrawWorldAmbientPass", prof_r_bsp_world_render.asBool());
  Drawer->DrawWorldAmbientPass();
  totalRenderTime += profDrawAmbient.stopAndReport();

  //RenderPortals(); //k8: it was here before, why?

  MiniStopTimer profBuildMObj("BuildVisibleObjectsList", prof_r_bsp_mobj_render.asBool());
  BuildVisibleObjectsList(IsShadowsEnabled());
  profBuildMObj.stopAndReport();

  MiniStopTimer profDrawMObjAmb("RenderMobjsAmbient", prof_r_bsp_mobj_render.asBool());
  RenderMobjsAmbient();
  profDrawMObjAmb.stopAndReport();

  RenderSceneLights(RD, Range);

  MiniStopTimer profDrawTextures("DrawWorldTexturesPass", prof_r_bsp_world_render.asBool());
  Drawer->DrawWorldTexturesPass();
  totalRenderTime += profDrawTextures.stopAndReport();

  MiniStopTimer profDrawMObjTex("RenderMobjsTextures", prof_r_bsp_mobj_render.asBool());
  RenderMobjsTextures();
  profDrawMObjTex.stopAndReport();

  MiniStopTimer profDrawFog("DrawWorldFogPass", prof_r_bsp_world_render.asBool());
  Drawer->DrawWorldFogPass();
  totalRenderTime += profDrawFog.stopAndReport();

  MiniStopTimer profDrawMObjFog("RenderMobjsFog", prof_r_bsp_mobj_render.asBool());
  RenderMobjsFog();
  profDrawMObjFog.stopAndReport();

  Drawer->EndFogPass();

  MiniStopTimer profDrawMObjNonShadow("RenderMobjsNonShadow", prof_r_bsp_mobj_render.asBool());
  RenderMobjs(RPASS_NonShadow);
  profDrawMObjNonShadow.stopAndReport();

  // render light bulbs
  // use "normal" model rendering code here (yeah)
  if (r_dbg_lightbulbs_static || r_dbg_lightbulbs_dynamic) {
    // static
    if (visibleStaticLightCount > 0 && r_dbg_lightbulbs_static) {
      visstatlightCount = visibleStaticLightCount;
      TAVec langles(0, 0, 0);
      const float zofs = r_dbg_lightbulbs_zofs_static.asFloat();
      for (const StLightInfo *sli = visstatlights.ptr(); visstatlightCount--; ++sli) {
        TVec lorg = sli->stlight->origin;
        lorg.z += sli->zofs+zofs;
        R_DrawLightBulb(lorg, langles, sli->stlight->color, RPASS_Normal, true/*shadowvol*/);
      }
    }
    // dynamic
    if (visibleDynamicLightCount > 0 && r_dbg_lightbulbs_dynamic) {
      visdynlightCount = visibleDynamicLightCount;
      TAVec langles(0, 0, 0);
      const float zofs = r_dbg_lightbulbs_zofs_dynamic.asFloat();
      for (const DynLightInfo *dli = visdynlights.ptr(); visdynlightCount--; ++dli) {
        TVec lorg = dli->l->origin;
        lorg.z += zofs;
        R_DrawLightBulb(lorg, langles, dli->l->color, RPASS_Normal, true/*shadowvol*/);
      }
    }
  }

  DrawParticles();

  RenderPortals();

  MiniStopTimer profDrawTransSpr("DrawTranslucentPolys", prof_r_bsp_world_render.asBool());
  DrawTranslucentPolys();
  totalRenderTime += profDrawTransSpr.stopAndReport();

  profRenderScene.stopAndReport();
  if (profRenderScene.isEnabled()) {
    if (totalRenderTime) GCon->Logf(NAME_Debug, "PROF: total advpasses time %g seconds (%d msecs)", ((int)totalRenderTime == 0 ? 0.0 : totalRenderTime), (int)(totalRenderTime*1000.0));
    GCon->Log(NAME_Debug, "===:::=== RenderScene ===:::===");
  }
}
