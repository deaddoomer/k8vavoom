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
#include "../gamedefs.h"
#include "r_local.h"


extern vuint32 gf_dynlights_processed;
extern vuint32 gf_dynlights_traced;

static VCvarB r_reg_disable_world("r_reg_disable_world", false, "Disable rendering of world (regular renderer).", CVAR_NoShadow/*|CVAR_Archive*/);
static VCvarB r_reg_disable_portals("r_reg_disable_portals", false, "Disable rendering of portals (regular renderer).", CVAR_NoShadow/*|CVAR_Archive*/);
static VCvarB dbg_show_dlight_trace_info("dbg_show_dlight_trace_info", false, "Show number of properly traced dynlights per frame.", CVAR_NoShadow/*|CVAR_Archive*/);


//**************************************************************************
//
// VLMapCache
//
//**************************************************************************

//==========================================================================
//
//  VLMapCache::resetBlock
//
//==========================================================================
void VLMapCache::resetBlock (Item *block) noexcept {
  if (block->surf) {
    //GCon->Logf(NAME_Debug, "cleared surface cache pointer for surface %p", block->surf);
    block->surf->CacheSurf = nullptr;
    block->surf = nullptr;
  }
}


//==========================================================================
//
//  VLMapCache::clearCacheInfo
//
//==========================================================================
void VLMapCache::clearCacheInfo () noexcept {
}


//==========================================================================
//
//  VLMapCache::releaseAtlas
//
//==========================================================================
void VLMapCache::releaseAtlas (vuint32 aid) noexcept {
  if (aid < NUM_BLOCK_SURFS && renderer) renderer->releaseAtlas(aid);
}


//==========================================================================
//
//  VLMapCache::allocAtlas
//
//==========================================================================
VLMapCache::AtlasInfo VLMapCache::allocAtlas (vuint32 aid, int minwidth, int minheight) noexcept {
  AtlasInfo res;
  if (minwidth > BLOCK_WIDTH || minheight > BLOCK_HEIGHT) {
    GCon->Logf(NAME_Error, "requesting new atlas WITH INVALID SIZE! id=%u; minsize=(%d,%d)", aid, minwidth, minheight);
  } else {
    if (dbg_show_lightmap_cache_messages) GCon->Logf(NAME_Debug, "LIGHTMAP: requesting new atlas; id=%u; minsize=(%d,%d)", aid, minwidth, minheight);
    if (aid < NUM_BLOCK_SURFS) {
      res.width = BLOCK_WIDTH;
      res.height = BLOCK_HEIGHT;
      if (renderer) renderer->allocAtlas(aid);
    }
  }
  return res;
}


//**************************************************************************
//
// VRenderLevelLightmap
//
//**************************************************************************

//==========================================================================
//
//  VRenderLevelLightmap::VRenderLevelLightmap
//
//==========================================================================
VRenderLevelLightmap::VRenderLevelLightmap (VLevel *ALevel)
  : VRenderLevelShared(ALevel)
  , c_subdivides(0)
  , c_seg_div(0)
  //, cacheframecount(0)
  //, freeblocks(nullptr)
  , lmcache()
  , nukeLightmapsOnNextFrame(false)
  , invalidateRelight(false)
  , lastLMapStaticRecalcFrame(0)
  , lmapStaticRecalcTimeLeft(-1)
{
  mIsShadowVolumeRenderer = false;
  lmcache.renderer = this;

  float mt = clampval(r_fade_mult_regular.asFloat(), 0.0f, 16.0f);
  if (mt <= 0.0f) mt = 1.0f;
  VDrawer::LightFadeMult = mt;

  initLightChain();

  FlushCaches();
}


//==========================================================================
//
//  VRenderLevelLightmap::releaseAtlas
//
//==========================================================================
void VRenderLevelLightmap::releaseAtlas (vuint32 aid) noexcept {
  vassert(aid < NUM_BLOCK_SURFS);
  block_dirty[aid].addDirty(0, 0, BLOCK_WIDTH, BLOCK_HEIGHT);
}


//==========================================================================
//
//  VRenderLevelLightmap::allocAtlas
//
//==========================================================================
void VRenderLevelLightmap::allocAtlas (vuint32 aid) noexcept {
  vassert(aid < NUM_BLOCK_SURFS);
  block_dirty[aid].addDirty(0, 0, BLOCK_WIDTH, BLOCK_HEIGHT);
}


//==========================================================================
//
//  VRenderLevelLightmap::ClearQueues
//
//  clears render queues
//
//==========================================================================
void VRenderLevelLightmap::ClearQueues () {
  VRenderLevelShared::ClearQueues();
  LMSurfList.reset();
  advanceCacheFrame();
}


//==========================================================================
//
//  VRenderLevelLightmap::advanceCacheFrame
//
//==========================================================================
void VRenderLevelLightmap::advanceCacheFrame () {
  light_chain_head = 0;
  if (++lmcache.cacheframecount == 0) {
    lmcache.cacheframecount = 1;
    memset(light_chain_used, 0, sizeof(light_chain_used));
    //memset(add_chain_used, 0, sizeof(add_chain_used));
    //blockpool.resetFrames();
    lmcache.zeroFrames();
  }
  //NukeLightmapCache();
}


//==========================================================================
//
//  VRenderLevelLightmap::initLightChain
//
//==========================================================================
void VRenderLevelLightmap::initLightChain () {
  memset(light_block, 0, sizeof(light_block));
  memset(light_chain, 0, sizeof(light_chain));
  memset(light_chain_used, 0, sizeof(light_chain_used));
  light_chain_head = 0;
  // force updating of all lightmaps
  for (unsigned f = 0; f < NUM_BLOCK_SURFS; ++f) {
    block_dirty[f].clear();
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::chainLightmap
//
//==========================================================================
void VRenderLevelLightmap::chainLightmap (surfcache_t *cache) {
  vassert(cache);
  const vuint32 bnum = ((VLMapCache::Item *)cache)->atlasid;
  vassert(bnum < NUM_BLOCK_SURFS);
  if (light_chain_used[bnum].lastframe != lmcache.cacheframecount) {
    // first time, put into list
    light_chain_used[bnum].lastframe = lmcache.cacheframecount;
    light_chain_used[bnum].next = light_chain_head;
    light_chain_head = bnum+1;
    light_chain[bnum] = nullptr;
  }
  cache->chain = light_chain[bnum];
  light_chain[bnum] = cache;
  ((VLMapCache::Item *)cache)->lastframe = lmcache.cacheframecount;
}


//==========================================================================
//
//  VRenderLevelLightmap::GetLightChainHead
//
//  lightmap chain iterator (used in renderer)
//  block number+1 or 0
//
//==========================================================================
vuint32 VRenderLevelLightmap::GetLightChainHead () {
  return light_chain_head;
}


//==========================================================================
//
//  VRenderLevelLightmap::GetLightChainNext
//
//  block number+1 or 0
//
//==========================================================================
vuint32 VRenderLevelLightmap::GetLightChainNext (vuint32 bnum) {
  if (bnum--) {
    vassert(bnum < NUM_BLOCK_SURFS);
    return light_chain_used[bnum].next;
  }
  return 0;
}


//==========================================================================
//
//  VRenderLevelLightmap::GetLightBlockDirtyArea
//
//==========================================================================
VDirtyArea &VRenderLevelLightmap::GetLightBlockDirtyArea (vuint32 bnum) {
  vassert(bnum < NUM_BLOCK_SURFS);
  return block_dirty[bnum];
}


//==========================================================================
//
//  VRenderLevelLightmap::GetLightBlock
//
//==========================================================================
rgba_t *VRenderLevelLightmap::GetLightBlock (vuint32 bnum) {
  vassert(bnum < NUM_BLOCK_SURFS);
  return light_block[bnum];
}


//==========================================================================
//
//  VRenderLevelLightmap::GetLightChainFirst
//
//==========================================================================
surfcache_t *VRenderLevelLightmap::GetLightChainFirst (vuint32 bnum) {
  vassert(bnum < NUM_BLOCK_SURFS);
  return light_chain[bnum];
}


//==========================================================================
//
//  VRenderLevelLightmap::RenderScene
//
//==========================================================================
void VRenderLevelLightmap::RenderScene (const refdef_t *RD, const VViewClipper *Range) {
  //r_viewleaf = Level->PointInSubsector(Drawer->vieworg); // moved to `PrepareWorldRender()`

  TransformFrustum();
  Drawer->SetupViewOrg();

  MiniStopTimer profRenderScene("RenderScene", IsAnyProfRActive());
  if (profRenderScene.isEnabled()) GCon->Log(NAME_Debug, "=== RenderScene ===");

  //ClearQueues(); // moved to `PrepareWorldRender()`
  //MarkLeaves(); // moved to `PrepareWorldRender()`
  //if (!MirrorLevel && !r_disable_world_update) UpdateFakeSectors(); // moved to `PrepareWorldRender()`

  // we will collect rendered sectors, so we could collect things from them
  nextRenderedSectorsCounter();

  gf_dynlights_processed = 0;
  gf_dynlights_traced = 0;

  RenderCollectSurfaces(RD, Range);

  if (!r_reg_disable_world) {
    //GCon->Logf("vfz: %f", Drawer->viewforward.z);
    MiniStopTimer profBeforeDraw("BeforeDrawWorldLMap", prof_r_bsp_world_render.asBool());
    Drawer->BeforeDrawWorldLMap();
    profBeforeDraw.stopAndReport();

    MiniStopTimer profDrawLMap("DrawLightmapWorld", prof_r_bsp_world_render.asBool());
    Drawer->DrawLightmapWorld();
    profDrawLMap.stopAndReport();

    /*
    if (r_lmap_overbright.asBool() && Drawer->IsMainFBOFloat() && !Drawer->IsCameraFBO()) {
      Drawer->PostprocessOvebright();
    }
    */
  }

  //if (!r_reg_disable_portals) RenderPortals(); //k8: it was here before, why?
  if (dbg_show_dlight_trace_info) GCon->Logf("DYNLIGHT: %u total, %u traced", gf_dynlights_processed, gf_dynlights_traced);

  BuildVisibleObjectsList(false); // no shadows
  MiniStopTimer profDrawMObj("RenderMobjs", prof_r_bsp_mobj_render.asBool());
  RenderMobjs(RPASS_Normal);
  profDrawMObj.stopAndReport();

  // draw light bulbs
  if (r_dbg_lightbulbs_static) {
    TAVec langles(0, 0, 0);
    const float zofs = r_dbg_lightbulbs_zofs_static.asFloat();
    light_t *stlight = Lights.ptr();
    for (int i = Lights.length(); i--; ++stlight) {
      //if (!Lights[i].radius) continue;
      if (!stlight->active || stlight->radius < 8) continue;
      TVec lorg = stlight->origin;
      lorg.z += zofs;
      R_DrawLightBulb(lorg, langles, stlight->color, RPASS_Normal, false/*shadowvol*/);
    }
  }

  if (r_dbg_lightbulbs_dynamic) {
    TAVec langles(0, 0, 0);
    const float zofs = r_dbg_lightbulbs_zofs_dynamic.asFloat();
    dlight_t *l = DLights;
    for (int i = MAX_DLIGHTS; i--; ++l) {
      if (l->radius < l->minlight+8 || l->die < Level->Time) continue;
      TVec lorg = l->origin;
      lorg.z += zofs;
      R_DrawLightBulb(lorg, langles, l->color, RPASS_Normal, false/*shadowvol*/);
    }
  }

  DrawParticles();

  RenderPortals();

  MiniStopTimer profDrawTransSpr("DrawTranslucentPolys", prof_r_bsp_world_render.asBool());
  DrawTranslucentPolys();
  profDrawTransSpr.stopAndReport();

  profRenderScene.stopAndReport();
  if (profRenderScene.isEnabled()) GCon->Log(NAME_Debug, "===:::=== RenderScene ===:::===");
}
