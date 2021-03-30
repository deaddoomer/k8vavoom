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
//**  BSP traversal, handling of LineSegs for rendering.
//**
//**************************************************************************
#include <limits.h>
#include <float.h>

#include "../gamedefs.h"
#include "r_local.h"

#define HORIZON_SURF_SIZE  (sizeof(surface_t)+sizeof(SurfVertex)*3)

static VCvarB r_skybox_clip_hack("r_skybox_clip_hack", false, "Relax clipping for skyboxes/portals? Most of the time this is not needed; might be useful for complex stacked sectors.", CVAR_Archive);

VCvarB r_draw_pobj("r_draw_pobj", true, "Render polyobjects?", CVAR_PreInit);
static VCvarI r_maxmiror_depth("r_maxmiror_depth", "1", "Maximum allowed mirrors.", CVAR_Archive);
VCvarI r_max_portal_depth("r_max_portal_depth", "1", "Maximum allowed portal depth (-1: infinite)", CVAR_Archive);
VCvarI r_max_portal_depth_override("r_max_portal_depth_override", "-1", "Maximum allowed portal depth override for map fixer (-1: not active)", 0);
static VCvarB r_allow_horizons("r_allow_horizons", true, "Allow horizon portal rendering?", CVAR_Archive);
static VCvarB r_allow_mirrors("r_allow_mirrors", true, "Allow mirror portal rendering?", CVAR_Archive);
static VCvarB r_allow_floor_mirrors("r_allow_floor_mirrors", false, "Allow floor/ceiling mirror portal rendering?", CVAR_Archive);
static VCvarB r_allow_stacked_sectors("r_allow_stacked_sectors", true, "Allow non-mirror portal rendering (SLOW)?", CVAR_Archive);

static VCvarB r_enable_sky_portals("r_enable_sky_portals", true, "Enable rendering of sky portals.", 0/*CVAR_Archive*/);

static VCvarB dbg_max_portal_depth_warning("dbg_max_portal_depth_warning", false, "Show maximum allowed portal depth warning?", 0/*CVAR_Archive*/);

static VCvarB r_ordered_subregions("r_ordered_subregions", false, "Order subregions in renderer?", CVAR_Archive);

VCvarB r_draw_queue_warnings("r_draw_queue_warnings", false, "Show 'queued twice' and other warnings?", CVAR_PreInit);

//static VCvarB dbg_dump_portal_list("dbg_dump_portal_list", false, "Dump portal list before rendering?", 0/*CVAR_Archive*/);

VCvarB r_disable_world_update("r_disable_world_update", false, "Disable world updates.", 0/*CVAR_Archive*/);

static VCvarB r_dbg_always_draw_flats("r_dbg_always_draw_flats", true, "Draw flat surfaces even if region is not visible (this is pobj hack)?", 0/*CVAR_Archive*/);
//static VCvarB r_draw_adjacent_subsector_things("r_draw_adjacent_subsector_things", true, "Draw things subsectors adjacent to visible subsectors (can fix disappearing things)?", CVAR_Archive);

extern VCvarB r_decals;
extern VCvarB clip_frustum;
extern VCvarB clip_frustum_bsp;
//extern VCvarB clip_frustum_bsp_segs;
//extern VCvarB clip_frustum_mirror;
extern VCvarB clip_use_1d_clipper;
// for portals
extern VCvarB clip_height;
extern VCvarB clip_midsolid;

// to clear portals
/*
static bool oldMirrors = true;
static bool oldHorizons = true;
static int oldMaxMirrors = -666;
static int oldPortalDepth = -666;
*/

double dbgCheckVisTime = 0;


//==========================================================================
//
//  ClipSegToSomething
//
//  k8: i don't know what is this, and it is unused anyway ;-)
//
//==========================================================================
static VVA_OKUNUSED inline void ClipSegToSomething (TVec &v1, TVec &v2, const TVec vieworg) {
  const TVec r1 = vieworg-v1;
  const TVec r2 = vieworg-v2;
  const float D1 = DotProduct(Normalise(CrossProduct(r1, r2)), vieworg);
  const float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), vieworg);
  // there might be a better method of doing this, but this one works for now...
       if (D1 > 0.0f && D2 < 0.0f) v2 += ((v2-v1)*D1)/(D1-D2);
  else if (D2 > 0.0f && D1 < 0.0f) v1 += ((v2-v1)*D1)/(D2-D1);
}


//==========================================================================
//
//  GetMaxPortalDepth
//
//==========================================================================
static inline int GetMaxPortalDepth () {
  int res = r_max_portal_depth_override.asInt();
  if (res >= 0) return res;
  return r_max_portal_depth.asInt();
}


//==========================================================================
//
//  IsStackedSectorPlane
//
//==========================================================================
/*
static inline bool IsStackedSectorPlane (const sec_plane_t &plane) {
  VEntity *SkyBox = plane.SkyBox;
  if (!SkyBox) return;
  // prevent recursion
  //if (SkyBox && SkyBox->IsPortalDirty()) SkyBox = nullptr;

  return (SkyBox && SkyBox->GetSkyBoxAlways());
}
*/


//==========================================================================
//
//  VRenderLevelShared::PrepareWorldRender
//
//==========================================================================
void VRenderLevelShared::PrepareWorldRender (const refdef_t *rd, const VViewClipper *Range) {
  if (dbgCheckVisTime > 0) GCon->Logf(NAME_Debug, "dbgCheckVisTime: %g", dbgCheckVisTime);
  ClearQueues();
  MarkLeaves(); // this sets `r_viewleaf`
  //if (dbg_dlight_vis_check_messages) GCon->Logf(NAME_Debug, "*** PrepareWorldRender ***");
  dbgCheckVisTime = 0;
  // this is done in `VRenderLevelShared::RenderPlayerView()`
  //FIXME: but should be done here if `r_viewleaf` was changed
  //if (!MirrorLevel && !r_disable_world_update) UpdateFakeSectors();
}


//==========================================================================
//
//  VRenderLevelShared::ChooseFlatSurfaces
//
//==========================================================================
void VRenderLevelShared::ChooseFlatSurfaces (sec_surface_t *&f0, sec_surface_t *&f1, sec_surface_t *flat0, sec_surface_t *flat1) {
  if (!flat0 || !flat1) {
    if (flat0) {
      vassert(!flat1);
      f0 = flat0;
      f1 = nullptr;
    } else if (flat1) {
      vassert(!flat0);
      f0 = flat1;
      f1 = nullptr;
    } else {
      vassert(!flat0);
      vassert(!flat1);
      f0 = f1 = nullptr;
    }
    return;
  }

  // check if flat1 is the same as flat0
  if (flat0->esecplane.splane == flat1->esecplane.splane) {
    f0 = flat1; // flat1 is fake, use it, because it may have different texture
    f1 = nullptr;
    return;
  }

  // not the same, check if on the same height
  if (flat0->esecplane.GetNormal() == flat1->esecplane.GetNormal() &&
      flat0->esecplane.GetDist() == flat1->esecplane.GetDist())
  {
    f0 = flat1; // flat1 is fake, use it, because it may have different texture
    f1 = nullptr;
    return;
  }

  // render both
  f0 = flat0;
  f1 = flat1;
}


//==========================================================================
//
//  VRenderLevelShared::GetFlatSetToRender
//
//  this chooses from zero to four subsector floor/ceiling surfaces
//  this is required to properly render Boom transfer heights
//  first two elements are floors, second two elements are ceilings
//  any element can be `nullptr`
//
//==========================================================================
void VRenderLevelShared::GetFlatSetToRender (subsector_t *sub, subregion_t *region, sec_surface_t *surfs[4]) {
  // if "clip fake planes" is not set, don't render real floor/ceiling if they're clipped away
  bool realFloorClipped = false;
  bool realCeilingClipped = false;

  // sector_t::SF_ClipFakePlanes: improved texture control
  //   the real floor and ceiling will be drawn with the real sector's flats
  //   the fake floor and ceiling (from either side) will be drawn with the control sector's flats
  //   the real floor and ceiling will be drawn even when in the middle part, allowing lifts into and out of deep water to render correctly (not possible in Boom)
  //   this flag does not work properly with sloped floors, and, if flag 2 is not set, with sloped ceilings either

  const sector_t *hs = (sub->sector ? sub->sector->heightsec : nullptr);
  if (hs && !(hs->SectorFlags&sector_t::SF_ClipFakePlanes)) {
    // check for clipped real floor and ceiling
    if (sub->sector->floor.minz > hs->floor.minz) realFloorClipped = true;
    if (sub->sector->ceiling.maxz < hs->ceiling.maxz) realCeilingClipped = true;
  }

  sec_surface_t *realfloor = (region->flags&subregion_t::SRF_ZEROSKY_FLOOR_HACK ? nullptr : region->realfloor);

  if (!realFloorClipped) {
    ChooseFlatSurfaces(surfs[0], surfs[1], realfloor, region->fakefloor);
  } else {
    surfs[0] = region->fakefloor;
    surfs[1] = nullptr;
  }

  if (!realCeilingClipped) {
    ChooseFlatSurfaces(surfs[2], surfs[3], region->realceil, region->fakeceil);
  } else {
    surfs[2] = region->fakeceil;
    surfs[3] = nullptr;
  }
}


//==========================================================================
//
//  VRenderLevelShared::SurfPrepareForRender
//
//  checks if surface is not queued twice, sets various flags
//  returns `false` if the surface should not be queued
//
//==========================================================================
bool VRenderLevelShared::SurfPrepareForRender (surface_t *surf) {
  if (!surf || !surf->subsector || surf->count < 3) return false;

  if (!surf->texinfo) {
    GCon->Logf(NAME_Error, "surface for seg #%d, line #%d (side %d), subsector #%d, sector #%d has no texinfo!",
      (surf->seg ? (int)(ptrdiff_t)(surf->seg-&Level->Segs[0]) : -1),
      (surf->seg && surf->seg->linedef ? (int)(ptrdiff_t)(surf->seg->linedef-&Level->Lines[0]) : -1),
      (surf->seg ? surf->seg->side : -1),
      (surf->subsector ? (int)(ptrdiff_t)(surf->subsector-&Level->Subsectors[0]) : -1),
      (surf->subsector ? (int)(ptrdiff_t)(surf->subsector->sector-&Level->Sectors[0]) : -1));
    return false;
  }

  VTexture *tex = surf->texinfo->Tex;
  /*k8: usually there is no need to do this
  if (tex && !tex->bIsCameraTexture) {
    VTexture *hitex = tex->GetHighResolutionTexture();
    if (hitex) {
      //GCon->Logf(NAME_Debug, "tex '%s' -> hi '%s'", *tex->Name, *hitex->Name);
      tex = hitex;
    }
  }
  */
  if (!tex || tex->Type == TEXTYPE_Null) return false;

  if (surf->queueframe == currQueueFrame) {
    if (r_draw_queue_warnings) {
      if (surf->seg) {
        //abort();
        GCon->Logf(NAME_Warning, "sector %d, subsector %d, seg %d surface queued for rendering twice",
          (int)(ptrdiff_t)(surf->subsector->sector-Level->Sectors),
          (int)(ptrdiff_t)(surf->subsector-Level->Subsectors),
          (int)(ptrdiff_t)(surf->seg-Level->Segs));
      } else {
        GCon->Logf(NAME_Warning, "sector %d, subsector %d surface queued for rendering twice",
          (int)(ptrdiff_t)(surf->subsector->sector-Level->Sectors),
          (int)(ptrdiff_t)(surf->subsector-Level->Subsectors));
      }
    }
    return false;
  }

  surf->queueframe = currQueueFrame;
  surf->SetPlVisible(surf->IsVisibleFor(Drawer->vieworg));

  // alpha: 1.0 is masked wall, 1.1 is solid wall
  if (surf->texinfo->Alpha < 1.0f || surf->texinfo->Additive ||
      ((surf->typeFlags&(surface_t::TF_MIDDLE|surface_t::TF_TOPHACK)) != 0 && tex->isTransparent()) ||
      tex->isTranslucent())
  {
    surf->drawflags |= surface_t::DF_MASKED;
  } else {
    surf->drawflags &= ~surface_t::DF_MASKED;
    // this is for transparent floors
    if (tex->isTransparent() && (surf->typeFlags&(surface_t::TF_FLOOR|surface_t::TF_CEILING)) &&
        surf->subsector && surf->subsector->sector->HasAnyExtraFloors())
    {
      surf->drawflags |= surface_t::DF_MASKED;
    }
  }

  //if (surf->drawflags&surface_t::DF_NO_FACE_CULL) surf->SetPlVisible(true);
  // this is done in `IsVisibleFor()`
  //if (surf->drawflags&surface_t::DF_NO_FACE_CULL) surf->drawflags |= surface_t::DF_PL_VISIBLE;

  // this is done in `IsVisibleFor()`
  //if (surf->drawflags&surface_t::DF_MIRROR) surf->drawflags |= surface_t::DF_PL_VISIBLE;

  return true;
}


//==========================================================================
//
//  VRenderLevelShared::QueueSimpleSurf
//
//  `SurfPrepareForRender()` should be done by the caller
//  `IsPlVisible()` should be checked by the caller
//
//==========================================================================
void VRenderLevelShared::QueueSimpleSurf (surface_t *surf) {
  if ((surf->drawflags&surface_t::DF_MASKED) == 0) {
    GetCurrentDLS().DrawSurfListSolid.append(surf);
  } else {
    GetCurrentDLS().DrawSurfListMasked.append(surf);
  }
}


//==========================================================================
//
//  VRenderLevelShared::QueueSkyPortal
//
//==========================================================================
void VRenderLevelShared::QueueSkyPortal (surface_t *surf) {
  if (!SurfPrepareForRender(surf)) return;
  if (!surf->IsPlVisible()) return;
  GetCurrentDLS().DrawSkyList.append(surf);
}


//==========================================================================
//
//  VRenderLevelShared::QueueHorizonPortal
//
//==========================================================================
void VRenderLevelShared::QueueHorizonPortal (surface_t *surf) {
  if (!SurfPrepareForRender(surf)) return;
  if (!surf->IsPlVisible()) return;
  GetCurrentDLS().DrawHorizonList.append(surf);
}


//==========================================================================
//
//  VRenderLevelShared::CommonQueueSurface
//
//  main dispatcher (it calls other queue methods)
//
//==========================================================================
void VRenderLevelShared::CommonQueueSurface (surface_t *surf, SFCType type) {
  if (PortalLevel == 0) {
    world_surf_t &s = WorldSurfs.alloc();
    s.Surf = surf;
    s.Type = type;
  } else {
    switch (type) {
      case SFCType::SFCT_World: QueueWorldSurface(surf); break;
      case SFCType::SFCT_Sky: QueueSkyPortal(surf); break;
      case SFCType::SFCT_Horizon: QueueHorizonPortal(surf); break;
      default: Sys_Error("internal renderer error: unknown surface type %d", (int)type);
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::DrawSurfaces
//
//==========================================================================
void VRenderLevelShared::DrawSurfaces (subsector_t *sub, sec_region_t *secregion, seg_t *seg,
  surface_t *InSurfs, texinfo_t *texinfo, VEntity *SkyBox, int LightSourceSector, int SideLight,
  bool AbsSideLight, bool CheckSkyBoxAlways)
{
  surface_t *surf = InSurfs; // this actually a list
  if (!surf) return;

  if (!texinfo->Tex || texinfo->Tex->Type == TEXTYPE_Null) return;

  // check mirror clipping plane for non-walls
  // FIXME: dunno if this is really necessary
  if (Drawer->MirrorClip && Drawer->MirrorPlane.normal.z) {
    for (const surface_t *ss = surf; ss; ss = ss->next) {
      if (ss->count < 3) continue;
      for (int f = 0; f < ss->count; ++f) {
        if (Drawer->MirrorPlane.PointOnSide2(ss->verts[f].vec())) return;
      }
    }
  }

  enum {
    SFT_Wall,
    SFT_Floor,
    SFT_Ceiling,
  };

  const int surfaceType =
    surf->plane.normal.z == 0.0f ? SFT_Wall :
    (surf->plane.normal.z > 0.0f ? SFT_Floor : SFT_Ceiling);

  vuint32 lightColor;

  bool complexLight = false; // do we need to set wall surfaces to proper light level from 3d floors?

  // calculate lighting for floors and ceilings
  //TODO: do this in 3d floor setup
  sec_params_t *LightParams;
  if (LightSourceSector < 0 || LightSourceSector >= Level->NumSectors) {
    sector_t *pobjsector = (seg && seg->pobj ? seg->pobj->GetSector() : nullptr);
    if (pobjsector) {
      // use original 3d polyobject sector for lighting
      LightParams = pobjsector->eregions->params;
      lightColor = LightParams->LightColor;
    } else {
      LightParams = secregion->params;
      lightColor = LightParams->LightColor;
      if (sub->sector->Has3DFloors()) {
        // if this is top flat of insane 3d floor, its light level should be taken from the upper region (or main sector, if this region is the last one)
        //??? should we ignore visual regions here? (sec_region_t::RF_OnlyVisual)
        if (surfaceType == SFT_Floor) {
          if ((secregion->extraline && (secregion->regflags&(sec_region_t::RF_NonSolid|sec_region_t::RF_SaneRegion|sec_region_t::RF_BaseRegion)) == 0) ||
              (secregion->regflags&sec_region_t::RF_BaseRegion))
          {
            sec_region_t *nreg = GetHigherRegion(sub->sector, secregion);
            if (nreg->params) {
              LightParams = nreg->params;
              lightColor = LightParams->LightColor;
            }
            //lightColor = 0xff00ff00;
            //if (!(secregion->regflags&sec_region_t::RF_BaseRegion)) return;
          }
        } else if (surfaceType == SFT_Wall && seg && seg->frontsector == sub->sector && !FixedLight && !AbsSideLight) {
          // if we don't have regions that may affect light, don't do it
          for (sec_region_t *reg = sub->sector->eregions->next; reg; reg = reg->next) {
            if ((reg->regflags&(sec_region_t::RF_NonSolid|sec_region_t::RF_OnlyVisual|sec_region_t::RF_SaneRegion|sec_region_t::RF_BaseRegion)) == 0) {
              complexLight = true;
              break;
            }
          }
        }
      }
    }
  } else {
    //GCon->Logf(NAME_Debug, "sector #%d, lightsrc=%d", (int)(ptrdiff_t)(sub->sector-&Level->Sectors[0]), LightSourceSector);
    LightParams = &Level->Sectors[LightSourceSector].params;
    lightColor = LightParams->LightColor;
  }

  int lLev = (AbsSideLight ? 0 : LightParams->lightlevel)+SideLight;

  float glowFloorHeight = 0;
  float glowCeilingHeight = 0;
  vuint32 glowFloorColor = 0;
  vuint32 glowCeilingColor = 0;

  // floor or ceiling lights
  // this rely on the fact that flat surfaces will never mix with other surfaces
  //TODO: this should also interpolate wall lighting
  switch (surfaceType) {
    case SFT_Wall:
      glowFloorHeight = LightParams->glowFloorHeight;
      glowCeilingHeight = LightParams->glowCeilingHeight;
      glowFloorColor = LightParams->glowFloor;
      glowCeilingColor = LightParams->glowCeiling;
      break;
    case SFT_Floor:
      if (LightParams->lightFCFlags&sec_params_t::LFC_FloorLight_Abs) lLev = LightParams->lightFloor; else lLev += LightParams->lightFloor;
      break;
    case SFT_Ceiling:
      if (LightParams->lightFCFlags&sec_params_t::LFC_CeilingLight_Abs) lLev = LightParams->lightCeiling; else lLev += LightParams->lightCeiling;
      break;
  }

  lLev = R_GetLightLevel(FixedLight, lLev+ExtraLight);
  vuint32 Fade = GetFade(secregion, (surfaceType != SFT_Wall));

  /*
  if (surfaceType == SFT_Floor || surfaceType == SFT_Ceiling) {
    GCon->Logf(NAME_Debug, "reg.flags=0x%08x; fade=0x%08x (0x%08x)", secregion->regflags, Fade, secregion->params->Fade);
    Fade = FADE_LIGHT;
  }
  */

  // sky/skybox/stacked sector rendering

  // prevent recursion
  if (SkyBox) {
    if (SkyBox->IsPortalDirty() || (CurrPortal && CurrPortal->IsSky())) SkyBox = nullptr;
  }

  const bool IsStack = (SkyBox && SkyBox->GetSkyBoxAlways());

  //k8: i hope that the parens are right here
  if (texinfo->Tex == GTextureManager[skyflatnum] || (IsStack && CheckSkyBoxAlways)) {
    VSky *Sky = nullptr;
    if (!SkyBox && (sub->sector->Sky&SKY_FROM_SIDE) != 0) {
      //GCon->Logf(NAME_Debug, "SKYSIDE!!!");
      int Tex;
      bool Flip;
      if ((vuint32)sub->sector->Sky == SKY_FROM_SIDE) {
        Tex = Level->LevelInfo->Sky2Texture;
        Flip = true;
      } else {
        side_t *Side = &Level->Sides[(sub->sector->Sky&(SKY_FROM_SIDE-1))-1];
        Tex = Side->TopTexture;
        //GCon->Logf(NAME_Debug, "SKYSIDE: %s", *GTextureManager[Tex]->Name);
        Flip = !!Level->Lines[Side->LineNum].arg3;
      }

      VTexture *sk2tex = GTextureManager[Tex];
      if (sk2tex && sk2tex->Type != TEXTYPE_Null) {
        for (auto &&sks : SideSkies) if (sks->SideTex == Tex && sks->SideFlip == Flip) { Sky = sks; break; }
        if (!Sky) {
          Sky = new VSky;
          Sky->Init(Tex, Tex, 0, 0, false, !!(Level->LevelInfo->LevelInfoFlags&VLevelInfo::LIF_ForceNoSkyStretch), Flip, false);
          SideSkies.Append(Sky);
        }
      }
    }

    if (!Sky && !SkyBox) {
      //if (CurrPortal && CurrPortal->IsSky()) GCon->Logf(NAME_Debug, "NEW IN SKYPORTAL");
      InitSky();
      Sky = &BaseSky;
    }

    VPortal *Portal = nullptr;
    if (SkyBox) {
      // check if we already have any portal with this skybox/stacked sector
      for (auto &&pp: Portals) if (pp && pp->MatchSkyBox(SkyBox)) { Portal = pp; break; }
      // nope?
      if (!Portal) {
        // no, no such portal yet, create a new one
        if (IsStack) {
          // advrender cannot into stacked sectors yet
          if (r_allow_stacked_sectors) Portal = new VSectorStackPortal(this, SkyBox);
        } else {
          if (r_enable_sky_portals) Portal = new VSkyBoxPortal(this, SkyBox);
        }
        if (Portal) Portals.Append(Portal);
      }
    } else if (r_enable_sky_portals) {
      // check if we already have any portal with this sky
      for (auto &&pp : Portals) if (pp && pp->MatchSky(Sky)) { Portal = pp; break; }
      // nope?
      if (!Portal) {
        // no, no such portal yet, create a new one
        //if (CurrPortal && CurrPortal->IsSky()) GCon->Logf(NAME_Debug, "NEW NORMAL SKY IN SKY PORTAL");
        Portal = new VSkyPortal(this, Sky);
        Portals.Append(Portal);
      }
    }

    // if we have a portal to add, put its surfaces into render/portal surfaces list
    if (Portal) {
      //GCon->Log("----");
      //const bool doRenderSurf = (surf ? IsStack && CheckSkyBoxAlways && SkyBox->GetSkyBoxPlaneAlpha() : false);
      // stacked sectors are queued for rendering immediately
      // k8: this can cause "double surface queue" error (if stacked sector is used more than once)
      //     also, for some reason surfaces are added both to portal, and to renderer; wtf?!
      //     it seems that this is done for things like mirrors or such; dunno yet
      bool doRenderSurf = (surf ? IsStack && CheckSkyBoxAlways : false);
      float alpha = 0.0f;
      //k8: wtf is this?
      if (doRenderSurf) {
        alpha = SkyBox->GetSkyBoxPlaneAlpha();
        if (alpha <= 0.0f || alpha >= 1.0f) doRenderSurf = false;
        //if (!plane.splane->pic) return;
      }
      //GCon->Logf(NAME_Debug, "PORTAL(%d): IsStack=%d; doRenderSurf=%d", Portals.length(), (int)IsStack, (int)doRenderSurf);
      for (; surf; surf = surf->next) {
        //k8: do we need to set this here? or portal will do this for us?
        surf->texinfo = texinfo; //k8:should we force texinfo here?
        surf->Light = (lLev<<24)|lightColor;
        surf->Fade = Fade;
        surf->dlightframe = sub->dlightframe;
        surf->dlightbits = sub->dlightbits;
        surf->glowFloorHeight = glowFloorHeight;
        surf->glowCeilingHeight = glowCeilingHeight;
        surf->glowFloorColor = glowFloorColor;
        surf->glowCeilingColor = glowCeilingColor;
        Portal->Surfs.Append(surf);
        if (!doRenderSurf) continue;
        if (surf->queueframe == currQueueFrame) continue;
        //GCon->Logf("  SURF!");
        if (!SurfPrepareForRender(surf)) continue;
        if (!surf->IsPlVisible()) continue;
        //GCon->Logf(NAME_Debug, "  SURF! norm=(%g,%g,%g); alpha=%g", surfs->plane.normal.x, surfs->plane.normal.y, surfs->plane.normal.z, SkyBox->GetSkyBoxPlaneAlpha());
        //surfs->drawflags |= surface_t::DF_MASKED;
        RenderStyleInfo ri;
        ri.alpha = alpha;
        ri.translucency = RenderStyleInfo::Translucent;
        ri.fade = Fade;
        QueueTranslucentSurface(surf, ri);
      }
      return;
    } else {
      if (!IsStack || !CheckSkyBoxAlways) return;
    }
  } // done skybox rendering

  vuint32 sflight = (lLev<<24)|lightColor;

  /* 3d floor flat debug code
  if (surfaceType == SFT_Floor) {
    if ((secregion->extraline && (secregion->regflags&(sec_region_t::RF_NonSolid|sec_region_t::RF_SaneRegion|sec_region_t::RF_BaseRegion)) == 0)) {
      texinfo->Alpha = 0.4f;
    }
  }
  */

  // note that masked surfaces (i.e. textures with binary transparency) processed by the normal renderers.
  // only alpha-blended and additive surfaces must be rendered in a separate pass.
  const bool isCommon = (texinfo->Alpha >= 1.0f && !texinfo->Additive && !texinfo->Tex->isTranslucent());

  sec_region_t *lastWallRegion = sub->sector->eregions; // default region for walls

  for (; surf; surf = surf->next) {
    // calculate proper light level for wall parts
    if (complexLight && surf->count >= 3) {
      //FIXME: make this faster!
      float mz = FLT_MAX;
      const SurfVertex *sv = &surf->verts[0];
      for (int f = surf->count; f--; ++sv) mz = min2(mz, sv->z);
      sec_region_t *nreg = GetHigherRegionAtZ(sub->sector, mz-0.1f);
      if (nreg != lastWallRegion) {
        // `AbsSideLight` and `FixedLight` are definitely `false` here
        LightParams = nreg->params;
        lightColor = LightParams->LightColor;
        lLev = LightParams->lightlevel+SideLight;
        lLev = R_GetLightLevel(0, lLev+ExtraLight);
        //Fade = GetFade(secregion, (surfaceType != SFT_Wall));
        sflight = (lLev<<24)|lightColor;
        lastWallRegion = nreg;
      }
    }

    surf->texinfo = texinfo; //k8:should we force texinfo here?
    surf->Light = sflight;
    surf->Fade = Fade;
    surf->dlightframe = sub->dlightframe;
    surf->dlightbits = sub->dlightbits;
    surf->glowFloorHeight = glowFloorHeight;
    surf->glowCeilingHeight = glowCeilingHeight;
    surf->glowFloorColor = glowFloorColor;
    surf->glowCeilingColor = glowCeilingColor;

    if (isCommon) {
      CommonQueueSurface(surf, SFCType::SFCT_World);
    } else if (surf->queueframe != currQueueFrame) {
      //surf->queueframe = currQueueFrame;
      //surf->SetPlVisible(surf->IsVisibleFor(Drawer->vieworg));
      if (!SurfPrepareForRender(surf)) continue;
      if (!surf->IsPlVisible()) continue;
      RenderStyleInfo ri;
      ri.alpha = texinfo->Alpha;
      ri.translucency = (texinfo->Additive ? RenderStyleInfo::Additive : RenderStyleInfo::Translucent);
      ri.fade = Fade;
      QueueTranslucentSurface(surf, ri);
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::RenderHorizon
//
//==========================================================================
void VRenderLevelShared::RenderHorizon (subsector_t *sub, sec_region_t *secregion, subregion_t *subregion, drawseg_t *dseg) {
  if (!dseg) return; // just in case
  seg_t *seg = dseg->seg;
  if (!seg) return; // just in case

  if (!dseg->HorizonTop) {
    dseg->HorizonTop = (surface_t *)Z_Malloc(HORIZON_SURF_SIZE);
    dseg->HorizonBot = (surface_t *)Z_Malloc(HORIZON_SURF_SIZE);
    memset((void *)dseg->HorizonTop, 0, HORIZON_SURF_SIZE);
    memset((void *)dseg->HorizonBot, 0, HORIZON_SURF_SIZE);
  }

  // horizon is not supported in sectors with slopes, so just use TexZ
  float TopZ = secregion->eceiling.splane->TexZ;
  float BotZ = secregion->efloor.splane->TexZ;
  float HorizonZ = Drawer->vieworg.z;

  // handle top part
  if (TopZ > HorizonZ) {
    sec_surface_t *Ceil = (subregion->fakeceil ? subregion->fakeceil : subregion->realceil);
    if (Ceil) {
      // calculate light and fade
      sec_params_t *LightParams = Ceil->esecplane.splane->LightSourceSector != -1 ?
        &Level->Sectors[Ceil->esecplane.splane->LightSourceSector].params :
        secregion->params;
      int lLev = R_GetLightLevel(FixedLight, LightParams->lightlevel+ExtraLight);
      vuint32 Fade = GetFade(secregion);

      surface_t *Surf = dseg->HorizonTop;
      Surf->plane = *(TPlane *)(dseg->seg);
      Surf->texinfo = &Ceil->texinfo;
      Surf->HorizonPlane = Ceil->esecplane.splane; //FIXME: 3dfloor
      Surf->Light = (lLev<<24)|LightParams->LightColor;
      Surf->Fade = Fade;
      Surf->count = 4;
      SurfVertex *svs = &Surf->verts[0];
      svs[0].setVec(*seg->v1); svs[0].z = max2(BotZ, HorizonZ);
      svs[1].setVec(*seg->v1); svs[1].z = TopZ;
      svs[2].setVec(*seg->v2); svs[2].z = TopZ;
      svs[3].setVec(*seg->v2); svs[3].z = max2(BotZ, HorizonZ);
      if (Ceil->esecplane.splane->pic == skyflatnum) {
        // if it's a sky, render it as a regular sky surface
        DrawSurfaces(sub, secregion, nullptr, Surf, &Ceil->texinfo, secregion->eceiling.splane->SkyBox, -1,
          seg->sidedef->Light, !!(seg->sidedef->Flags&SDF_ABSLIGHT),
          false);
      } else {
        CommonQueueSurface(Surf, SFCType::SFCT_Horizon);
      }
    }
  }

  // handle bottom part
  if (BotZ < HorizonZ) {
    sec_surface_t *Floor = (subregion->fakefloor ? subregion->fakefloor : subregion->realfloor);
    if (Floor) {
      // calculate light and fade
      sec_params_t *LightParams = Floor->esecplane.splane->LightSourceSector != -1 ?
        &Level->Sectors[Floor->esecplane.splane->LightSourceSector].params :
        secregion->params;
      int lLev = R_GetLightLevel(FixedLight, LightParams->lightlevel+ExtraLight);
      vuint32 Fade = GetFade(secregion);

      surface_t *Surf = dseg->HorizonBot;
      Surf->plane = *(TPlane *)(dseg->seg);
      Surf->texinfo = &Floor->texinfo;
      Surf->HorizonPlane = Floor->esecplane.splane; //FIXME: 3dfloor
      Surf->Light = (lLev<<24)|LightParams->LightColor;
      Surf->Fade = Fade;
      Surf->count = 4;
      SurfVertex *svs = &Surf->verts[0];
      svs[0].setVec(*seg->v1); svs[0].z = BotZ;
      svs[1].setVec(*seg->v1); svs[1].z = min2(TopZ, HorizonZ);
      svs[2].setVec(*seg->v2); svs[2].z = min2(TopZ, HorizonZ);
      svs[3].setVec(*seg->v2); svs[3].z = BotZ;
      if (Floor->esecplane.splane->pic == skyflatnum) {
        // if it's a sky, render it as a regular sky surface
        DrawSurfaces(sub, secregion, nullptr, Surf, &Floor->texinfo, secregion->efloor.splane->SkyBox, -1,
          seg->sidedef->Light, !!(seg->sidedef->Flags&SDF_ABSLIGHT),
          false);
      } else {
        CommonQueueSurface(Surf, SFCType::SFCT_Horizon);
      }
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::RenderMirror
//
//==========================================================================
void VRenderLevelShared::RenderMirror (subsector_t *sub, sec_region_t *secregion, drawseg_t *dseg) {
  if (!dseg) return; // just in case
  seg_t *seg = dseg->seg;
  if (!seg) return; // just in case
  if (MirrorLevel < r_maxmiror_depth && r_allow_mirrors) {
    if (!dseg->mid) return;

    VPortal *Portal = nullptr;
    for (auto &&pp : Portals) {
      if (pp && pp->MatchMirror(seg)) {
        Portal = pp;
        break;
      }
    }
    if (!Portal) {
      Portal = new VMirrorPortal(this, seg);
      Portals.Append(Portal);
    }

    for (surface_t *surf = dseg->mid->surfs; surf; surf = surf->next) Portal->Surfs.Append(surf);
  } else {
    if (dseg->mid) {
      DrawSurfaces(sub, secregion, seg, dseg->mid->surfs, &dseg->mid->texinfo,
                   secregion->eceiling.splane->SkyBox, -1, seg->sidedef->Light,
                   !!(seg->sidedef->Flags&SDF_ABSLIGHT), false);
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::RenderLine
//
//  Clips the given segment and adds any visible pieces to the line list.
//
//==========================================================================
void VRenderLevelShared::RenderLine (subsector_t *sub, sec_region_t *secregion, subregion_t *subregion, drawseg_t *dseg) {
  if (!dseg) return; // just in case
  seg_t *seg = dseg->seg;
  if (!seg) return; // just in case
  line_t *linedef = seg->linedef;

  // mirror clip
  if (Drawer->MirrorClip && (Drawer->MirrorPlane.PointOnSide2(*seg->v1) || Drawer->MirrorPlane.PointOnSide2(*seg->v2))) return;

  if (!linedef) {
    // miniseg
    // check with clipper?
    #if 0
    if (!(sub->miscFlags&subsector_t::SSMF_Rendered) && sub->numlines) {
      // check if have any non-miniseg segs
      const *sseg = &Level->Segs[sub->firstline];
      for (int f = sub->numlines; f--; ++sseg) {
        linedef = sseg->linedef;
        if (linedef && (linedef->flags&ML_DONTDRAW) == 0) return; // no need to mark it, normal line rendering should do it
      }
      // no non-minisegs, mark this subsector as visible
      //FIXME: this can be wrong for hidden sectors, and some other obscure cases
      sub->miscFlags |= subsector_t::SSMF_Rendered;
      seg->flags |= SF_MAPPED;
    }
    #else
    sub->miscFlags |= subsector_t::SSMF_Rendered;
    seg->flags |= SF_MAPPED;
    #endif
    return;
  }

  #if 1
  // render (queue) translucent lines by segs (for sorter)
  if (r_dbg_use_fullsegs.asBool() && !linedef->pobj() && IsShadowVolumeRenderer() && (linedef->exFlags&ML_EX_NON_TRANSLUCENT)) {
    side_t *side = (seg->side == 0 ? linedef->frontside : linedef->backside);
    //vassert(side);
    if (side->fullseg) {
      if (side->rendercount == renderedLineCounter) return; // already rendered
      side->rendercount = renderedLineCounter;
      seg = side->fullseg;
      dseg = seg->drawsegs;
    }
  }
  #endif

  if (seg->PointOnSide(Drawer->vieworg)) {
    // viewer is in back side or on plane
    // gozzo 3d floors should be rendered regardless of orientation
    segpart_t *sp = dseg->extra;
    if (sp && sp->texinfo.Tex && (sp->texinfo.Alpha < 1.0f || sp->texinfo.Additive || sp->texinfo.Tex->isTranslucent())) {
      // mark subsector as rendered (nope)
      //sub->miscFlags |= subsector_t::SSMF_Rendered;
      side_t *sidedef = seg->sidedef;
      //GCon->Logf("00: extra for seg #%d (line #%d)", (int)(ptrdiff_t)(seg-Level->Segs), (int)(ptrdiff_t)(linedef-Level->Lines));
      for (; sp; sp = sp->next) {
        DrawSurfaces(sub, secregion, seg, sp->surfs, &sp->texinfo, secregion->eceiling.splane->SkyBox,
          -1, sidedef->Light, !!(sidedef->Flags&SDF_ABSLIGHT), false);
        //GCon->Logf("  extra for seg #%d (%p)", (int)(ptrdiff_t)(seg-Level->Segs), sp);
      }
      //GCon->Logf("01: extra for seg #%d", (int)(ptrdiff_t)(seg-Level->Segs));
    }
    return;
  }

  #if 0
  if (MirrorClipSegs && clip_frustum && clip_frustum_mirror && /*clip_frustum_bsp &&*/ Drawer->viewfrustum.planes[TFrustum::Forward].isValid()) {
    // clip away segs that are behind mirror
    if (Drawer->viewfrustum.planes[TFrustum::Forward].PointOnSide(*seg->v1) && Drawer->viewfrustum.planes[TFrustum::Forward].PointOnSide(*seg->v2)) return; // behind mirror
  }
  #endif

  /*
    k8: i don't know what Janis wanted to accomplish with this, but it actually
        makes clipping WORSE due to limited precision

        clip sectors that are behind rendered segs
  if (seg->backsector) {
    // just apply this to sectors without slopes
    if (seg->frontsector->floor.normal.z == 1.0f && seg->backsector->floor.normal.z == 1.0f &&
        seg->frontsector->ceiling.normal.z == -1.0f && seg->backsector->ceiling.normal.z == -1.0f)
    {
      TVec v1 = *seg->v1;
      TVec v2 = *seg->v2;
      ClipSegToSomething(v1, v2, Drawer->vieworg);
      if (!ViewClip.IsRangeVisible(v2, v1)) return;
    }
  } else {
    TVec v1 = *seg->v1;
    TVec v2 = *seg->v2;
    ClipSegToSomething(v1, v2, Drawer->vieworg);
    if (!ViewClip.IsRangeVisible(v2, v1)) return;
  }
  */

  if (!ViewClip.IsRangeVisible(*seg->v2, *seg->v1)) return;

  // k8: this drops some segs that may leak without proper frustum culling
  // k8: this seems to be unnecessary now
  // k8: reenabled, because why not?
  // k8: no more
  /*
  if (clip_frustum_bsp_segs && !ViewClip.CheckSegFrustum(sub, seg)) return;
  */

  // automap
  // mark only autolines that allowed to be seen on the automap
  if ((linedef->flags&ML_DONTDRAW) == 0) {
    if ((linedef->flags&ML_MAPPED) == 0) {
      // this line is at least partially mapped; let automap drawer do the rest
      linedef->exFlags |= ML_EX_PARTIALLY_MAPPED|ML_EX_CHECK_MAPPED;
      automapUpdateSeen = true;
    }
    if ((seg->flags&SF_MAPPED) == 0) {
      linedef->exFlags |= ML_EX_PARTIALLY_MAPPED|ML_EX_CHECK_MAPPED;
      seg->flags |= SF_MAPPED;
      automapUpdateSeen = true;
      // mark subsector as rendered (but only if linedef is allowed to be seen on the automap)
      if ((linedef->flags&ML_DONTDRAW) == 0) sub->miscFlags |= subsector_t::SSMF_Rendered;
    }
  }

  side_t *sidedef = seg->sidedef;

  VEntity *SkyBox = secregion->eceiling.splane->SkyBox;
  if (!seg->backsector) {
    // single sided line
    if (seg->linedef->special == LNSPEC_LineHorizon) {
      if (r_allow_horizons) RenderHorizon(sub, secregion, subregion, dseg);
    } else if (seg->linedef->special == LNSPEC_LineMirror) {
      RenderMirror(sub, secregion, dseg);
    } else {
      if (dseg->mid) DrawSurfaces(sub, secregion, seg, dseg->mid->surfs, &dseg->mid->texinfo, SkyBox, -1, sidedef->Light, !!(sidedef->Flags&SDF_ABSLIGHT), false);
    }
    if (dseg->topsky) DrawSurfaces(sub, secregion, nullptr, dseg->topsky->surfs, &dseg->topsky->texinfo, SkyBox, -1, sidedef->Light, !!(sidedef->Flags&SDF_ABSLIGHT), false);
  } else {
    // two sided line
    if (dseg->top) DrawSurfaces(sub, secregion, seg, dseg->top->surfs, &dseg->top->texinfo, SkyBox, -1, sidedef->Light, !!(sidedef->Flags&SDF_ABSLIGHT), false);
    if (dseg->topsky) DrawSurfaces(sub, secregion, nullptr, dseg->topsky->surfs, &dseg->topsky->texinfo, SkyBox, -1, sidedef->Light, !!(sidedef->Flags&SDF_ABSLIGHT), false);
    if (dseg->bot) DrawSurfaces(sub, secregion, seg, dseg->bot->surfs, &dseg->bot->texinfo, SkyBox, -1, sidedef->Light, !!(sidedef->Flags&SDF_ABSLIGHT), false);
    if (dseg->mid) DrawSurfaces(sub, secregion, seg, dseg->mid->surfs, &dseg->mid->texinfo, SkyBox, -1, sidedef->Light, !!(sidedef->Flags&SDF_ABSLIGHT), false);
    for (segpart_t *sp = dseg->extra; sp; sp = sp->next) {
      DrawSurfaces(sub, secregion, seg, sp->surfs, &sp->texinfo, SkyBox, -1, sidedef->Light, !!(sidedef->Flags&SDF_ABSLIGHT), false);
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::RenderSecSurface
//
//==========================================================================
void VRenderLevelShared::RenderSecSurface (subsector_t *sub, sec_region_t *secregion, sec_surface_t *ssurf, VEntity *SkyBox) {
  if (!ssurf) return;
  TSecPlaneRef plane(ssurf->esecplane);

  //if (!plane.splane->pic) return;

  // floor/ceiling mirrors
  // check if the surface is visible, to avoid doing excessive work
  // for some yet unknown (for me) reason this culls out alot of mirror planes. wtf?!
  if (plane.splane->MirrorAlpha < 1.0f /*&& ssurf->surfs && ssurf->surfs->IsVisibleFor(Drawer->vieworg)*/) {
    // mirror
    if (r_allow_mirrors && r_allow_floor_mirrors && MirrorLevel < r_maxmiror_depth) {
      TPlane rpl = plane.GetPlane();

      VPortal *Portal = nullptr;
      for (auto &&pp : Portals) {
        if (pp && pp->MatchMirror(&rpl)) {
          Portal = pp;
          break;
        }
      }
      if (!Portal) {
        Portal = new VMirrorPortal(this, &rpl);
        Portals.Append(Portal);
      }

      //FIXME: this flag should be reset if `MirrorAlpha` was changed!
      for (surface_t *surfs = ssurf->surfs; surfs; surfs = surfs->next) {
        //surfs->drawflags |= surface_t::DF_NO_FACE_CULL;
        surfs->drawflags |= surface_t::DF_MIRROR;
        Portal->Surfs.Append(surfs);
      }

      if (plane.splane->MirrorAlpha <= 0.0f) return;
      ssurf->texinfo.Alpha = min2(plane.splane->MirrorAlpha, 1.0f);
    } else {
      // this is NOT right!
      if (plane.splane->pic) {
        ssurf->texinfo.Alpha = 1.0f;
        //GCon->Logf("MALPHA=%f", plane.splane->MirrorAlpha);
        // darken it a little to simulate a mirror
        sec_params_t *oldRegionLightParams = secregion->params;
        sec_params_t newLight = (plane.splane->LightSourceSector >= 0 ? Level->Sectors[plane.splane->LightSourceSector].params : *oldRegionLightParams);
        newLight.lightlevel = (int)((float)newLight.lightlevel*plane.splane->MirrorAlpha);
        secregion->params = &newLight;
        // take light from `secregion->params`
        DrawSurfaces(sub, secregion, nullptr, ssurf->surfs, &ssurf->texinfo, SkyBox, -1, 0, false, true);
        // and resore rregion
        secregion->params = oldRegionLightParams;
      }
      return;
    }
  }

  if (!plane.splane->pic) return;
  //if (plane.splane->isCeiling()) GCon->Logf(NAME_Debug, "SFCTID: %d (sector #%d)", (int)plane.splane->pic, (int)(ptrdiff_t)(sub->sector-&Level->Sectors[0]));
  DrawSurfaces(sub, secregion, nullptr, ssurf->surfs, &ssurf->texinfo, SkyBox, plane.splane->LightSourceSector, 0, false, true);
}


//==========================================================================
//
//  VRenderLevelShared::NeedToRenderNextSubFirst
//
//  this orders subregions to avoid overdraw (partially)
//
//==========================================================================
bool VRenderLevelShared::NeedToRenderNextSubFirst (const subregion_t *region) noexcept {
  if (!region->next || !r_ordered_subregions) return false;
  const sec_surface_t *floor = region->fakefloor;
  if (floor) {
    const float d = floor->PointDistance(Drawer->vieworg);
    if (d <= 0.0f) return true;
  }
  floor = region->realfloor;
  if (floor) {
    const float d = floor->PointDistance(Drawer->vieworg);
    if (d <= 0.0f) return true;
  }
  return false;
}


//==========================================================================
//
//  VRenderLevelShared::AddPolyObjToClipper
//
//  we have to do this separately, because pobj shape is arbitrary
//
//==========================================================================
void VRenderLevelShared::AddPolyObjToClipper (VViewClipper &clip, subsector_t *sub) {
  if (sub && sub->HasPObjs() /*&& r_draw_pobj*/ && clip_use_1d_clipper) {
    clip.ClipAddPObjSegs(sub, (Drawer->MirrorClip ? &Drawer->MirrorPlane : nullptr));
    //clip.ClipAddPObjSegs(pobj, sub, (MirrorClipSegs && Drawer->viewfrustum.planes[TFrustum::Forward].isValid() ? &Drawer->viewfrustum.planes[TFrustum::Forward] : nullptr));
  }
}


//==========================================================================
//
//  VRenderLevelShared::RenderPolyObj
//
//  render all polyobjects in the subsector
//
//==========================================================================
void VRenderLevelShared::RenderPolyObj (subsector_t *sub) {
  if (sub && sub->HasPObjs() && r_draw_pobj) {
    const bool doUpdates = !r_disable_world_update.asBool();
    subregion_t *region = sub->regions;
    sec_region_t *secregion = region->secregion;
    for (auto &&it : sub->PObjFirst()) {
      polyobj_t *pobj = it.pobj();
      if (pobj->rendercount != BspVisFrame) {
        pobj->rendercount = BspVisFrame; // mark as rendered
        TSecPlaneRef po_floor, po_ceiling;
        po_floor.set(&pobj->pofloor, false);
        po_ceiling.set(&pobj->poceiling, false);
        const bool doSegUpdates = (doUpdates && pobj->updateWorldFrame != updateWorldFrame);
        pobj->updateWorldFrame = updateWorldFrame;
        for (auto &&sit : pobj->SegFirst()) {
          const seg_t *seg = sit.seg();
          if (seg->linedef && seg->drawsegs) {
            if (doSegUpdates) UpdateDrawSeg(sub, seg->drawsegs, po_floor, po_ceiling);
            RenderLine(sub, secregion, region, seg->drawsegs);
          }
        }
        // render flats for 3d pobjs
        if (pobj->Is3D()) {
          // mark this sector as rendered (thing visibility check needs this info)
          const int secnum = (int)(ptrdiff_t)(pobj->GetSector()-&Level->Sectors[0]);
          MarkBspVisSector(secnum);
          markSectorRendered(secnum);
          for (subsector_t *posub = pobj->GetSector()->subsectors; posub; posub = posub->seclink) {
            // update pobj
            if (doUpdates && posub->updateWorldFrame != updateWorldFrame) {
              posub->updateWorldFrame = updateWorldFrame;
              UpdateSubRegions(posub);
            }
            //GCon->Logf(NAME_Debug, "pobj #%d: sub #%d (%p)", pobj->tag, (int)(ptrdiff_t)(posub-&Level->Subsectors[0]), posub->regions);
            RenderSubRegion(posub, posub->regions);
          }
        }
      }
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::RenderSubRegion
//
//  Determine floor/ceiling planes.
//  Draw one or more line segments.
//
//==========================================================================
void VRenderLevelShared::RenderSubRegion (subsector_t *sub, subregion_t *region) {
  sec_region_t *secregion = region->secregion;

  if ((secregion->regflags&sec_region_t::RF_BaseRegion) && sub->numlines > 0 && !sub->isInnerPObj()) {
    const seg_t *seg = &Level->Segs[sub->firstline];
    for (int j = sub->numlines; j--; ++seg) {
      if (!seg->linedef || !seg->drawsegs) continue; // miniseg has no drawsegs/segparts
      RenderLine(sub, secregion, region, seg->drawsegs);
    }
  }

  const bool nextFirst = NeedToRenderNextSubFirst(region);
  if (nextFirst) RenderSubRegion(sub, region->next);

  {
    sec_surface_t *fsurf[4];
    GetFlatSetToRender(sub, region, fsurf);

    if (fsurf[0]) RenderSecSurface(sub, secregion, fsurf[0], secregion->efloor.splane->SkyBox);
    if (fsurf[1]) RenderSecSurface(sub, secregion, fsurf[1], secregion->efloor.splane->SkyBox);

    if (fsurf[2]) RenderSecSurface(sub, secregion, fsurf[2], secregion->eceiling.splane->SkyBox);
    if (fsurf[3]) RenderSecSurface(sub, secregion, fsurf[3], secregion->eceiling.splane->SkyBox);
  }

  if (!nextFirst && region->next) return RenderSubRegion(sub, region->next);
}


//==========================================================================
//
//  VRenderLevelShared::RenderSubsector
//
//==========================================================================
void VRenderLevelShared::RenderSubsector (int num, bool onlyClip) {
  subsector_t *sub = &Level->Subsectors[num];
  if (sub->isAnyPObj()) return;

  // this should not be necessary, because BSP node rejection does it for us
  /*
  if (Drawer->MirrorClip) {
    float sbb[6];
    Level->GetSubsectorBBox(sub, sbb);
    if (!Drawer->MirrorPlane.checkBox(sbb)) return;
  }
  */

  // render it if we're not in "only clip" mode
  if (!onlyClip) {
    // if we already hit this subsector somehow, do nothing
    if (sub->VisFrame == currVisFrame) return;

    // is this subsector potentially visible?
    if (ViewClip.ClipCheckSubsector(sub)) {
      if (sub->parent) sub->parent->visframe = currVisFrame; // check is here for one-sector degenerate maps
      sub->VisFrame = currVisFrame;

      // mark this subsector as rendered
      MarkBspVis(num);

      // mark this sector as rendered (thing visibility check needs this info)
      const int secnum = (int)(ptrdiff_t)(sub->sector-&Level->Sectors[0]);
      MarkBspVisSector(secnum);

      markSectorRendered(secnum);

      // update world
      if (sub->updateWorldFrame != updateWorldFrame) {
        sub->updateWorldFrame = updateWorldFrame;
        if (!r_disable_world_update) UpdateSubRegions(sub);
      }

      // update static light info
      SubStaticLigtInfo *sli = &SubStaticLights.ptr()[num];
      if (sli->touchedStatic.length()) {
        for (auto it : sli->touchedStatic.first()) {
          const int slidx = it.getKey();
          if (slidx >= 0 && slidx < Lights.length()) {
            Lights.ptr()[slidx].dlightframe = currDLightFrame;
          }
        }
      }

      // render the polyobj in the subsector first, and add it to clipper
      // this blocks view with polydoors
      //FIXME: (it is broken now, because we need to dynamically split pobj with BSP)
      RenderPolyObj(sub);
      AddPolyObjToClipper(ViewClip, sub);
      RenderSubRegion(sub, sub->regions);
    }
  }

  // add subsector's segs to the clipper
  // clipping against mirror is done only for vertical mirror planes
  if (clip_use_1d_clipper) {
    //ViewClip.ClipAddSubsectorSegs(sub, (MirrorClipSegs && Drawer->viewfrustum.planes[TFrustum::Forward].isValid() ? &Drawer->viewfrustum.planes[TFrustum::Forward] : nullptr));
    ViewClip.ClipAddSubsectorSegs(sub, (Drawer->MirrorClip ? &Drawer->MirrorPlane : nullptr));
  }
}


//==========================================================================
//
//  VRenderLevelShared::RenderBSPNode
//
//  Renders all subsectors below a given node, traversing subtree
//  recursively. Just call with BSP root.
//
//==========================================================================
void VRenderLevelShared::RenderBSPNode (int bspnum, const float bbox[6], unsigned clipflags, bool onlyClip) {
 tailcall:
#ifdef VV_CLIPPER_FULL_CHECK
  if (ViewClip.ClipIsFull()) return;
#endif

  if (!ViewClip.ClipIsBBoxVisible(bbox)) return;

  // mirror clip
  if (Drawer->MirrorClip && !Drawer->MirrorPlane.checkBox(bbox)) return;

  if (!onlyClip) {
    // cull the clipping planes if not trivial accept
    if (clipflags && clip_frustum && clip_frustum_bsp) {
      const TClipPlane *cp = &Drawer->viewfrustum.planes[0];
      for (unsigned i = Drawer->viewfrustum.planeCount; i--; ++cp) {
        if (!(clipflags&cp->clipflag)) continue; // don't need to clip against it
        //k8: this check is always true, because view origin is outside of frustum (oops)
        //if (cp->PointOnSide(Drawer->vieworg)) continue; // viewer is in back side or on plane (k8: why check this?)
        auto crs = cp->checkBoxEx(bbox);
        if (crs == 1) clipflags ^= cp->clipflag; // if it is on a front side of this plane, don't bother checking with it anymore
        else if (crs == 0) {
          // it is enough to hit at least one "outside" to be completely outside
          // add this box to clipper, why not
          // k8: nope; this glitches
          //if (clip_use_1d_clipper) ViewClip.ClipAddBBox(bbox);
          if (!clip_use_1d_clipper) return;
          if (cp->clipflag&TFrustum::FarBit) return; // clipped with forward plane
          onlyClip = true;
          break;
          //return;
        }
      }
    }
  }

  // found a subsector?
  if (BSPIDX_IS_NON_LEAF(bspnum)) {
    // nope
    const node_t *bsp = &Level->Nodes[bspnum];
    //if (bsp->visframe == currVisFrame) return; // if we're exactly on a splitting plane, this can happen
    // decide which side the view point is on
    unsigned side = (unsigned)bsp->PointOnSide(Drawer->vieworg);
    // recursively divide front space (towards the viewer)
    RenderBSPNode(bsp->children[side], bsp->bbox[side], clipflags, onlyClip);
    // recursively divide back space (away from the viewer)
    side ^= 1;
    //return RenderBSPNode(bsp->children[side], bsp->bbox[side], clipflags, onlyClip);
    bspnum = bsp->children[side];
    bbox = bsp->bbox[side];
    goto tailcall;
  } else {
    // if we have a skybox there, turn off advanced clipping, or stacked sector may glitch
    const sector_t *sec = Level->Subsectors[BSPIDX_LEAF_SUBSECTOR(bspnum)].sector;
    if (sec->isAnyPObj()) return;
    if (r_skybox_clip_hack.asBool() && (sec->floor.SkyBox || sec->ceiling.SkyBox || MirrorLevel || PortalLevel)) {
      // this is for kdizd z1m3, for example
      const bool old_clip_height = clip_height.asBool();
      const bool old_clip_midsolid = clip_midsolid.asBool();
      //const bool old_clip_frustum_bsp = clip_frustum_bsp.asBool();
      //const bool old_clip_frustum_bsp_segs = clip_frustum_bsp_segs.asBool();
      clip_height = false;
      clip_midsolid = false;
      //clip_frustum_bsp = false;
      //clip_frustum_bsp_segs = false;
      RenderSubsector(BSPIDX_LEAF_SUBSECTOR(bspnum), onlyClip);
      clip_height = old_clip_height;
      clip_midsolid = old_clip_midsolid;
      //clip_frustum_bsp = old_clip_frustum_bsp;
      //clip_frustum_bsp_segs = old_clip_frustum_bsp_segs;
    } else {
      return RenderSubsector(BSPIDX_LEAF_SUBSECTOR(bspnum), onlyClip);
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::RenderBSPTree
//
//  Renders all subsectors below a given node, traversing subtree
//  recursively from BSP root.
//
//==========================================================================
void VRenderLevelShared::RenderBSPTree () {
  nextRenderedLineCounter();
  if (Level->NumSubsectors > 1) {
    vassert(Level->NumNodes > 0);
    /*static*/ const float bbox[6] = { -99999.0f, -99999.0f, -99999.0f, 99999.0f, 99999.0f, 99999.0f };
    unsigned clipflags = 0;
    const TClipPlane *cp = &Drawer->viewfrustum.planes[0];
    for (unsigned i = Drawer->viewfrustum.planeCount; i--; ++cp) clipflags |= cp->clipflag;
    return RenderBSPNode(Level->NumNodes-1, bbox, clipflags /*(Drawer->MirrorClip ? 0x3f : 0x1f)*/, false);
  } else if (Level->NumSubsectors > 0) {
    return RenderSubsector(0, false);
  }
}


//==========================================================================
//
//  VRenderLevelShared::RenderBspWorld
//
//==========================================================================
void VRenderLevelShared::RenderBspWorld (const refdef_t *rd, const VViewClipper *Range) {
  ViewClip.ClearClipNodes(Drawer->vieworg, Level);
  if (Range) {
    ViewClip.ClipResetFrustumPlanes();
    ViewClip.SetFrustum(Range->GetFrustum());
    ViewClip.ClipToRanges(*Range); // range contains a valid range, so we must clip away holes in it
  } else {
    ViewClip.ClipInitFrustumRange(Drawer->viewangles, Drawer->viewforward, Drawer->viewright, Drawer->viewup, rd->fovx, rd->fovy);
  }

  IncrementBspVis();

  if (PortalLevel == 0) {
    if (WorldSurfs.NumAllocated() < 32768) WorldSurfs.Resize(32768);
  }

  /*
  MirrorClipSegs = (Drawer->MirrorClip && !Drawer->viewfrustum.planes[TFrustum::Forward].normal.z);

  if (!clip_frustum_mirror) {
    MirrorClipSegs = false;
    Drawer->viewfrustum.planes[TFrustum::Forward].clipflag = 0;
  }
  */

  // mark the leaf we're in as seen
  if (r_viewleaf) r_viewleaf->miscFlags |= subsector_t::SSMF_Rendered;

  RenderBSPTree();

  // draw the most complex sky portal behind the scene first, without the need to use stencil buffer
  // most of the time this is the only sky portal, so we can get away without rendering portals at all
  if (PortalLevel == 0) {
    VPortal *BestSky = nullptr;
    int BestSkyIndex = -1;
    for (auto &&it : Portals.itemsIdx()) {
      VPortal *pp = it.value();
      if (pp && pp->IsSky() && (!BestSky || BestSky->Surfs.length() < pp->Surfs.length())) {
        BestSky = pp;
        BestSkyIndex = it.index();
      }
    }

    // if we found a sky, render it, and remove it from portal list
    if (BestSky) {
      PortalLevel = 1;
      BestSky->Draw(false);
      delete BestSky;
      BestSky = nullptr;
      Portals.RemoveIndex(BestSkyIndex);
      PortalLevel = 0;
    }

    // queue all collected world surfaces
    // k8: tell me again, why we couldn't do that in-place?
    for (auto &&wsurf : WorldSurfs) {
      switch (wsurf.Type) {
        case SFCType::SFCT_World: QueueWorldSurface(wsurf.Surf); break;
        case SFCType::SFCT_Sky: QueueSkyPortal(wsurf.Surf); break;
        case SFCType::SFCT_Horizon: QueueHorizonPortal(wsurf.Surf); break;
        default: Sys_Error("invalid queued 0-level world surface type %d", (int)wsurf.Type);
      }
    }
    WorldSurfs.resetNoDtor();
  }
}


//==========================================================================
//
//  VRenderLevelShared::RenderPortals
//
//==========================================================================
void VRenderLevelShared::RenderPortals () {
  //GCon->Logf(NAME_Debug, "VRenderLevelShared::RenderPortals: PortalLevel=%d; CurrPortal=%s; pcount=%d", PortalLevel, (CurrPortal ? (CurrPortal->IsSky() ? "sky" : "non-sky") : "none"), Portals.length());

  ++PortalLevel;

  // do not use foreach iterator over portals here ('cause number of portals may change)
  const int maxpdepth = GetMaxPortalDepth();
  if (maxpdepth < 0 || PortalLevel <= maxpdepth) {
    //FIXME: disable decals for portals
    //       i should rewrite decal rendering, so we can skip stencil buffer
    //       (or emulate stencil buffer with texture and shaders)
    const bool oldDecalsEnabled = r_decals;
    r_decals = false;
    //bool oldShadows = r_shadows;
    //if (/*Portal->stackedSector &&*/ IsShadowVolumeRenderer()) r_shadows = false;
    for (int pnum = 0; pnum < Portals.length(); ++pnum) {
      VPortal *pp = Portals[pnum];
      if (pp && pp->Level == PortalLevel) {
        bool allowDraw = true;
        if (pp->IsMirror()) {
          allowDraw = r_allow_mirrors.asBool();
        } else if (pp->IsStack()) {
          //!if (pp->stackedSector && IsShadowVolumeRenderer()) continue;
          allowDraw = r_allow_stacked_sectors.asBool();
        }
        if (allowDraw) pp->Draw(true);
      }
    }
    r_decals = oldDecalsEnabled;
    //r_shadows = oldShadows;
  } else {
    // if we are in sky portal, render nested sky portals
    // actually, always render sky portals
    if (true /*CurrPortal && CurrPortal->IsSkyBox()*/) {
      for (int pnum = 0; pnum < Portals.length(); ++pnum) {
        VPortal *pp = Portals[pnum];
        if (pp && pp->Level == PortalLevel && pp->IsSky() && !pp->IsSkyBox()) {
          pp->Draw(true);
        }
      }
    }
    if (dbg_max_portal_depth_warning) GCon->Logf(NAME_Warning, "portal level too deep (%d)", PortalLevel);
  }

  for (auto &&pp : Portals) {
    if (pp && pp->Level == PortalLevel) {
      delete pp;
      pp = nullptr;
    }
  }

  --PortalLevel;
  if (PortalLevel == 0) Portals.resetNoDtor();
}
