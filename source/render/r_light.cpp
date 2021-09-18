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
#include "../psim/p_player.h"
#include "../client/client.h"
#include "r_local.h"


// ////////////////////////////////////////////////////////////////////////// //
VCvarI r_light_mode("r_light_mode", "1", "Lighting mode (0:Vavoom; 1:Dark; 2:DarkBanded)?", CVAR_Archive);
VCvarF r_light_globvis("r_light_globvis", "8", "Dark lighting mode visibility.", CVAR_Archive);

VCvarB r_darken("r_darken", true, "Darken level to better match original Doom?", CVAR_Archive);
VCvarI r_ambient_min("r_ambient_min", "0", "Minimal ambient light.", CVAR_Archive);
VCvarB r_allow_ambient("r_allow_ambient", true, "Allow ambient lights?", CVAR_Archive);
VCvarB r_dynamic_lights("r_dynamic_lights", true, "Allow dynamic lights?", CVAR_Archive);
VCvarB r_dynamic_clip("r_dynamic_clip", true, "Clip dynamic lights?", CVAR_Archive);
VCvarB r_static_lights("r_static_lights", true, "Allow static lights?", CVAR_Archive);
VCvarB r_light_opt_shadow("r_light_opt_shadow", false, "Check if light can potentially cast a shadow.", CVAR_Archive);
VCvarF r_light_filter_dynamic_coeff("r_light_filter_dynamic_coeff", "0.2", "How close dynamic lights should be to be filtered out?\n(0.2-0.4 is usually ok).", CVAR_Archive);
VCvarB r_allow_dynamic_light_filter("r_allow_dynamic_light_filter", true, "Allow filtering of dynamic lights?", CVAR_Archive);

// currently affects only advanced renderer
VCvarI r_light_shadow_min_proj_dimension("r_light_shadow_min_proj_dimension", "112", "Do not render shadows for lights smaller than this screen size.", CVAR_Archive);

VCvarB r_shadowmaps("r_shadowmaps", false, "Use shadowmaps instead of shadow volumes?", /*CVAR_PreInit|*/CVAR_Archive);

static VCvarB r_dynamic_light_better_vis_check("r_dynamic_light_better_vis_check", true, "Do better (but slower) dynlight visibility checking on spawn?", CVAR_Archive);

extern VCvarB r_glow_flat;
extern VCvarB r_lmap_recalc_moved_static;

static VCvarB r_lmap_texture_check_static("r_lmap_texture_check_static", true, "Check textures of two-sided lines in lightmap tracer?", CVAR_Archive);
static VCvarB r_lmap_texture_check_dynamic("r_lmap_texture_check_dynamic", true, "Check textures of two-sided lines in lightmap tracer?", CVAR_Archive);
static VCvarI r_lmap_texture_check_radius_dynamic("r_lmap_texture_check_radius_dynamic", "300", "Disable texture check for dynamic lights with radius lower than this.", CVAR_Archive);


static TFrustum frustumDLight;
static TFrustumParam fpDLight;


#define RL_CLEAR_DLIGHT(_dl)  do { \
  (_dl)->radius = 0; \
  (_dl)->flags = 0; \
  /* lights with lightid aren't added to the ownership map, because we may have many of them for one owner */ \
  /* note that they will not be deleted when their owner is going away */ \
  if ((_dl)->ownerUId && !(_dl)->lightid) dlowners.del((_dl)->ownerUId); \
  (_dl)->lightid = 0; \
  (_dl)->ownerUId = 0; \
} while (0)


static VClass *eexCls = nullptr;
static VField *mflFld = nullptr;
#if 0
static VClass *invCls = nullptr;
static VField *invFld = nullptr;
#endif


//==========================================================================
//
//  VRenderLevelShared::IsInInventory
//
//==========================================================================
bool VRenderLevelShared::IsInInventory (VEntity *owner, vuint32 invUId) const {
  if (!owner || !invUId) return false;
  if (owner->ServerUId == invUId) return true;

  if (!eexCls) {
    eexCls = VThinker::FindClassChecked("EntityEx");
    mflFld = VThinker::FindTypedField(eexCls, "bMeIsMuzzleFlash", TYPE_Bool);
    #if 0
    invCls = VThinker::FindClassChecked("Inventory");
    invFld = VThinker::FindTypedField(eexCls, "Inventory", TYPE_Reference, invCls);
    #endif
  }

  VEntity *ee = Level->GetEntityBySUId(invUId);
  //if (!ee->IsA(invCls)) return false;
  return (ee && (ee->Owner == owner || mflFld->GetBool(ee)));

  #if 0
  // loop over inventory
  for (VEntity *inv = (VEntity *)invFld->GetObjectValue(owner); inv; inv = (VEntity *)invFld->GetObjectValue(inv)) {
    //GCon->Logf(NAME_Debug, "owner:%s(%u); invUId=%u; inv=%s(%u) with uid=%u", owner->GetClass()->GetName(), owner->GetUniqueId(), invUId, inv->GetClass()->GetName(), inv->GetUniqueId(), inv->ServerUId);
    if (inv->ServerUId == invUId && inv->Owner == owner) return true;
  }

  return false;
  #endif
}


//==========================================================================
//
//  calcLightPoint
//
//  raise a point to 1/2 of the height
//
//==========================================================================
static inline TVec calcLightPoint (const TVec &pt, float height) noexcept {
  return TVec(pt.x, pt.y, pt.z+max2(0.0f, height)*0.5f);
}


//==========================================================================
//
//  VRenderLevelShared::CalcScreenLightDimensions
//
//  use drawer's vieworg, so it can be called only when rendering a scene
//  it's not exact!
//  returns `false` if the light is invisible (or too small, with radius < 8)
//  in this case, `w`, and `h` are zeroes
//  both `w` and `h` can be `nullptr`
//
//==========================================================================
bool VRenderLevelShared::CalcScreenLightDimensions (const TVec &LightPos, const float LightRadius, int *w, int *h) noexcept {
  if (w) *w = 0;
  if (h) *h = 0;
  if (!Drawer || !isFiniteF(LightRadius) || LightRadius < 8.0f) return false;
  // just in case
  if (!Drawer->vpmats.vport.isValid()) return false;

  // transform into world coords
  TVec inworld = Drawer->vpmats.toWorld(LightPos);

  //GCon->Logf(NAME_Debug, "LightPos=(%g,%g,%g); LightRadius=%g; wpos=(%g,%g,%g)", LightPos.x, LightPos.y, LightPos.z, LightRadius, inworld.x, inworld.y, inworld.z);

  // the thing that should not be (completely behind)
  if (inworld.z-LightRadius > -1.0f) return false;

  // create light bbox
  float bbox[6];
  bbox[0+0] = inworld.x-LightRadius;
  bbox[0+1] = inworld.y-LightRadius;
  bbox[0+2] = inworld.z-LightRadius;

  bbox[3+0] = inworld.x+LightRadius;
  bbox[3+1] = inworld.y+LightRadius;
  bbox[3+2] = min2(-1.0f, inworld.z+LightRadius); // clamp to znear

  const int scrx0 = Drawer->vpmats.vport.x0;
  const int scry0 = Drawer->vpmats.vport.y0;
  const int scrx1 = Drawer->vpmats.vport.getX1();
  const int scry1 = Drawer->vpmats.vport.getY1();

  int minx = scrx1+64, miny = scry1+64;
  int maxx = -(scrx0-64), maxy = -(scry0-64);

  // transform points, get min and max
  for (unsigned f = 0; f < MAX_BBOX3D_CORNERS; ++f) {
    int winx, winy;
    Drawer->vpmats.project(GetBBox3DCorner(f, bbox), &winx, &winy);

    if (minx > winx) minx = winx;
    if (miny > winy) miny = winy;
    if (maxx < winx) maxx = winx;
    if (maxy < winy) maxy = winy;
  }

  if (minx > scrx1 || miny > scry1 || maxx < scrx0 || maxy < scry0) return false;

  minx = midval(scrx0, minx, scrx1);
  miny = midval(scry0, miny, scry1);
  maxx = midval(scrx0, maxx, scrx1);
  maxy = midval(scry0, maxy, scry1);

  //GCon->Logf("  LightRadius=%f; (%d,%d)-(%d,%d)", LightRadius, minx, miny, maxx, maxy);
  const int wdt = maxx-minx+1;
  const int hgt = maxy-miny+1;
  //GCon->Logf("  LightRadius=%f; (%dx%d)", LightRadius, wdt, hgt);

  if (wdt < 1 || hgt < 1) return false;

  if (w) *w = wdt;
  if (h) *h = hgt;

  return true;
}


//==========================================================================
//
//  VRenderLevelShared::CalcBBox3DScreenPosition
//
//  does very primitive Z clipping
//  returns `false` if fully out of screen
//
//==========================================================================
bool VRenderLevelShared::CalcBBox3DScreenPosition (const float bbox3d[6], int *x0, int *y0, int *x1, int *y1) noexcept {
  // just in case
  if (!Drawer->vpmats.vport.isValid()) return false;

  TVec vbbox[8]; // transformed bbox points
  bool seenOkZ = false;
  // transform into world coords
  for (unsigned f = 0; f < MAX_BBOX3D_CORNERS; ++f) {
    vbbox[f] = Drawer->vpmats.toWorld(GetBBox3DCorner(f, bbox3d));
    if (vbbox[f].z > -1.0f) vbbox[f].z = -1.0f; else seenOkZ = true;
  }
  if (!seenOkZ) return false;

  const int scrx0 = Drawer->vpmats.vport.x0;
  const int scry0 = Drawer->vpmats.vport.y0;
  const int scrx1 = Drawer->vpmats.vport.getX1();
  const int scry1 = Drawer->vpmats.vport.getY1();

  int minx = scrx1+64, miny = scry1+64;
  int maxx = -(scrx0-64), maxy = -(scry0-64);

  // transform points, get min and max
  for (unsigned f = 0; f < 8; ++f) {
    int winx, winy;
    Drawer->vpmats.project(vbbox[f], &winx, &winy);

    if (minx > winx) minx = winx;
    if (miny > winy) miny = winy;
    if (maxx < winx) maxx = winx;
    if (maxy < winy) maxy = winy;
  }

  if (minx > scrx1 || miny > scry1 || maxx < scrx0 || maxy < scry0) return false;

  minx = midval(scrx0, minx, scrx1);
  miny = midval(scry0, miny, scry1);
  maxx = midval(scrx0, maxx, scrx1);
  maxy = midval(scry0, maxy, scry1);

  *x0 = minx;
  *y0 = miny;
  *x1 = maxx;
  *y1 = maxy;
  return true;
}


//==========================================================================
//
//  VRenderLevelShared::RefilterStaticLights
//
//==========================================================================
void VRenderLevelShared::RefilterStaticLights () {
}


//==========================================================================
//
//  VRenderLevelShared::ResetStaticLights
//
//==========================================================================
void VRenderLevelShared::ResetStaticLights () {
  StOwners.reset();
  Lights.reset();
  //GCon->Log(NAME_Debug, "VRenderLevelShared::ResetStaticLights");
  // clear touching subs
  for (auto &&ssl : SubStaticLights) {
    ssl.touchedStatic.reset();
    ssl.invalidateFrame = 0;
  }
}


//==========================================================================
//
//  VRenderLevelShared::AddStaticLightRGB
//
//==========================================================================
void VRenderLevelShared::AddStaticLightRGB (vuint32 OwnerUId, const VLightParams &lpar, const vuint32 flags) {
  staticLightsFiltered = false;
  light_t &L = Lights.Alloc();
  L.origin = lpar.Origin;
  L.radius = lpar.Radius;
  L.color = lpar.Color;
  //L.dynowner = nullptr;
  L.ownerUId = OwnerUId;
  L.leafnum = (int)(ptrdiff_t)(Level->PointInSubsector(lpar.Origin)-Level->Subsectors);
  L.active = true;
  L.coneDirection = lpar.coneDirection;
  L.coneAngle = lpar.coneAngle;
  L.levelSector = lpar.LevelSector;
  L.levelScale = lpar.LevelScale;
  L.flags = flags;
  if (lpar.LevelSector) {
    L.sectorLightLevel = lpar.LevelSector->params.lightlevel;
    const float intensity = clampval(L.sectorLightLevel*(fabsf(lpar.LevelScale)*0.125f), 0.0f, 255.0f);
    L.radius = VLevelInfo::GZSizeToRadius(intensity, (lpar.LevelScale < 0.0f), 2.0f);
  }
  if (OwnerUId) {
    auto osp = StOwners.get(OwnerUId);
    if (osp) Lights[*osp].ownerUId = 0;
    StOwners.put(OwnerUId, Lights.length()-1);
  }
  CalcStaticLightTouchingSubs(Lights.length()-1, L);
  //GCon->Logf(NAME_Debug, "VRenderLevelShared::AddStaticLightRGB: count=%d", Lights.length());
}


//==========================================================================
//
//  VRenderLevelShared::MoveStaticLightByOwner
//
//==========================================================================
void VRenderLevelShared::MoveStaticLightByOwner (vuint32 OwnerUId, const TVec &origin) {
  if (!OwnerUId) return;
  auto stp = StOwners.get(OwnerUId);
  if (!stp) return;
  light_t &sl = Lights[*stp];
  if (fabsf(sl.origin.x-origin.x) <= 4.0f &&
      fabsf(sl.origin.y-origin.y) <= 4.0f &&
      fabsf(sl.origin.z-origin.z) <= 4.0f)
  {
    return;
  }
  //GCon->Logf(NAME_Debug, "moving static light #%d (owner uid=%u)", *stp, sl.ownerUId);
  //if (sl.origin == origin) return;
  if (sl.active && r_lmap_recalc_moved_static) InvalidateStaticLightmaps(sl.origin, sl.radius, false, (sl.flags&dlight_t::NoGeoClip));
  sl.origin = origin;
  sl.leafnum = (int)(ptrdiff_t)(Level->PointInSubsector(sl.origin)-Level->Subsectors);
  CalcStaticLightTouchingSubs(*stp, sl);
  if (sl.active && r_lmap_recalc_moved_static) InvalidateStaticLightmaps(sl.origin, sl.radius, true, (sl.flags&dlight_t::NoGeoClip));
}


//==========================================================================
//
//  VRenderLevelShared::RemoveStaticLightByIndex
//
//==========================================================================
void VRenderLevelShared::RemoveStaticLightByIndex (int slidx) {
  if (slidx < 0 || slidx >= Lights.length()) return;
  light_t *sl = Lights.ptr()+slidx;
  //GCon->Logf(NAME_Debug, "removing static light #%d (owner uid=%u)", slidx, sl->ownerUId);
  if (sl->active) {
    if (r_lmap_recalc_moved_static) InvalidateStaticLightmaps(sl->origin, sl->radius, false, (sl->flags&dlight_t::NoGeoClip));
    sl->active = false;
  }
  if (sl->ownerUId) {
    StOwners.del(sl->ownerUId);
    sl->ownerUId = 0;
    CalcStaticLightTouchingSubs(slidx, *sl);
  }
}


//==========================================================================
//
//  VRenderLevelShared::RemoveStaticLightByOwner
//
//==========================================================================
void VRenderLevelShared::RemoveStaticLightByOwner (vuint32 OwnerUId) {
  if (!OwnerUId) return;
  auto stp = StOwners.get(OwnerUId);
  if (!stp) return;
  //GCon->Logf(NAME_Debug, "removing static light with owner uid %u (%d)", OwnerUId, *stp);
  RemoveStaticLightByIndex(*stp);
}


//==========================================================================
//
//  VRenderLevelShared::InvalidateStaticLightmaps
//
//==========================================================================
void VRenderLevelShared::InvalidateStaticLightmaps (const TVec &/*org*/, float /*radius*/, bool /*relight*/, bool /*noGeoClip*/) {
}


//==========================================================================
//
//  VRenderLevelShared::ClearReferences
//
//==========================================================================
void VRenderLevelShared::ClearReferences () {
  // no need to do anything here, the renderer will be notified about thinker add/remove events
  // dynlights
  /*
  dlight_t *l = DLights;
  for (unsigned i = 0; i < MAX_DLIGHTS; ++i, ++l) {
    if (l->die < Level->Time || l->radius < 1.0f) {
      RL_CLEAR_DLIGHT(l);
      continue;
    }
    if (l->Owner && l->Owner->IsRefToCleanup()) {
      RL_CLEAR_DLIGHT(l);
    }
  }
  */
}


//==========================================================================
//
//  VRenderLevelShared::MarkLights
//
//==========================================================================
/*
void VRenderLevelShared::MarkLights (dlight_t *light, vuint32 bit, int bspnum, int lleafnum) {
  if (BSPIDX_IS_LEAF(bspnum)) {
    const int num = (bspnum != -1 ? BSPIDX_LEAF_SUBSECTOR(bspnum) : 0);
    subsector_t *ss = &Level->Subsectors[num];

    if (ss->dlightframe != currDLightFrame) {
      ss->dlightbits = bit;
      ss->dlightframe = currDLightFrame;
    } else {
      ss->dlightbits |= bit;
    }
  } else {
    node_t *node = &Level->Nodes[bspnum];
    const float dist = node->PointDistance(light->origin);
    if (dist > -light->radius+light->minlight) MarkLights(light, bit, node->children[0], lleafnum);
    if (dist < light->radius-light->minlight) MarkLights(light, bit, node->children[1], lleafnum);
  }
}
*/


//==========================================================================
//
//  VRenderLevelShared::PushDlights
//
//==========================================================================
void VRenderLevelShared::PushDlights () {
  //???:if (GGameInfo->IsPaused() || (Level->LevelInfo->LevelInfoFlags2&VLevelInfo::LIF2_Frozen)) return;
  IncDLightFrameCount();

  if (!r_dynamic_lights) return;

  // lightvis params
  LitCalcBBox = false;
  CurrLightCalcUnstuck = (r_shadowmaps.asBool() && Drawer->CanRenderShadowMaps() && r_shadowmap_fix_light_dist);

  dlight_t *l = DLights;
  for (unsigned i = 0; i < MAX_DLIGHTS; ++i, ++l) {
    if (l->radius < 1.0f || l->die < Level->Time) {
      dlinfo[i].needTrace = 0;
      dlinfo[i].leafnum = -1;
      continue;
    }
    l->origin = l->origOrigin;
    //if (l->Owner && l->Owner->IsA(VEntity::StaticClass())) l->origin += ((VEntity *)l->Owner)->GetDrawDelta();
    if (l->ownerUId) {
      /*
      auto ownpp = suid2ent.get(l->ownerUId);
      if (ownpp) l->origin += (*ownpp)->GetDrawDelta();
      */
      VEntity *own = Level->GetEntityBySUId(l->ownerUId);
      if (own) l->origin += own->GetDrawDelta();
    }
    if (dlinfo[i].leafnum < 0) dlinfo[i].leafnum = (int)(ptrdiff_t)(Level->PointInSubsector(l->origin)-Level->Subsectors);
    //dlinfo[i].needTrace = (r_dynamic_clip && Level->NeedProperLightTraceAt(l->origin, l->radius) ? 1 : -1);
    //MarkLights(l, 1U<<i, Level->NumNodes-1, dlinfo[i].leafnum);
    //FIXME: this has one frame latency; meh for now
    LitCalcBBox = false; // we don't need any lists
    if (CalcPointLightVis(l->origin, l->radius, (int)i)) {
      dlinfo[i].needTrace = (doShadows ? 1 : -1);
    } else {
      // this one is invisible
      dlinfo[i].needTrace = 0;
      dlinfo[i].leafnum = -1;
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::AllocDlight
//
//==========================================================================
dlight_t *VRenderLevelShared::AllocDlight (VThinker *Owner, const TVec &lorg, float radius, int lightid) {
  dlight_t *dlowner = nullptr;
  dlight_t *dldying = nullptr;
  dlight_t *dlreplace = nullptr;
  dlight_t *dlbestdist = nullptr;

  if (radius <= 0.0f) radius = 0.0f; else if (radius < 2.0f) return nullptr; // ignore too small lights
  if (lightid < 0) lightid = 0;

  float bestdist = (lorg-cl->ViewOrg).lengthSquared();

  // even if filtering is disabled, filter them anyway
  const float coeff = (r_allow_dynamic_light_filter.asBool() ? clampval(r_light_filter_dynamic_coeff.asFloat(), 0.1f, 1.0f) : 0.02f);

  float radsq = (radius < 1.0f ? 64*64 : radius*radius*coeff);
  if (radsq < 32.0f*32.0f) radsq = 32.0f*32.0f;
  const float radsqhalf = radsq*0.25f;

  // if this is player's dlight, never drop it
  //TODO: check for Voodoo Dolls?
  bool isPlr = false;
  if (Owner && Owner->IsA(VEntity::StaticClass())) {
    isPlr = ((VEntity *)Owner)->IsPlayer();
  }

  int leafnum = -1;

  if (!isPlr) {
    // if the light is behind a view, drop it if it is further than the light radius
    if (bestdist >= (radius > 0.0f ? radius*radius : 64.0f*64.0f)) {
      if (fpDLight.needUpdate(cl->ViewOrg, cl->ViewAngles)) {
        fpDLight.setup(cl->ViewOrg, cl->ViewAngles);
        // this also drops too far-away lights
        frustumDLight.setup(clip_base, fpDLight, true, GetLightMaxDistDef());
      }
      if (!frustumDLight.checkSphere(lorg, (radius > 0.0f ? radius : 64.0f))) {
        //GCon->Logf("  DROPPED; radius=%f; dist=%f", radius, sqrtf(bestdist));
        return nullptr;
      }
    }
  }

  // look for any free slot (or free one if necessary)
  dlight_t *dl;
  bool skipVisCheck = false;

  // first try to find owned light to replace
  if (Owner) {
    if (lightid == 0) {
      auto idxp = dlowners.get(Owner->ServerUId);
      if (idxp) {
        dlowner = &DLights[*idxp];
        vassert(dlowner->ownerUId == Owner->ServerUId);
      }
    } else {
      //FIXME: make this faster!
      dl = DLights;
      for (int i = 0; i < MAX_DLIGHTS; ++i, ++dl) {
        if (dl->ownerUId == Owner->ServerUId && dl->lightid == lightid /*&& dl->die >= Level->Time && dl->radius > 0.0f*/) {
          dlowner = dl;
          break;
        }
      }
    }
  }

  // look for any free slot (or free one if necessary)
  if (!dlowner) {
    dl = DLights;
    for (int i = 0; i < MAX_DLIGHTS; ++i, ++dl) {
      // remove dead lights (why not?)
      if (dl->die < Level->Time) dl->radius = 0.0f;
      // unused light?
      if (dl->radius < 2.0f) {
        RL_CLEAR_DLIGHT(dl);
        if (!dldying) dldying = dl;
        continue;
      }
      // don't replace player's lights
      if (dl->flags&dlight_t::PlayerLight) continue;
      // replace furthest light
      const float dist = (dl->origin-cl->ViewOrg).lengthSquared();
      if (dist > bestdist) {
        bestdist = dist;
        dlbestdist = dl;
      }
      // check if we already have dynamic light around new origin
      if (!isPlr) {
        const float dd = (dl->origin-lorg).lengthSquared();
        if (dd <= 6.0f*6.0f) {
          if (radius > 0 && dl->radius >= radius) return nullptr;
          dlreplace = dl;
          skipVisCheck = true; // it is so near that we don't need to do any checks
          break; // stop searching, we have the perfect candidate
        } else if (dd < radsqhalf) {
          // if existing light radius is greater than a new radius, drop new light, 'cause
          // we have too much lights around one point (prolly due to several things at one place)
          if (radius > 0.0f && dl->radius >= radius) return nullptr;
          // otherwise, replace this light
          dlreplace = dl;
          skipVisCheck = true; // it is so near that we don't need to do any checks (i hope)
          // keep checking, we may find a better candidate
        }
      }
    }
  }

  if (dlowner) {
    // remove replaced light
    //if (dlreplace && dlreplace != dlowner) memset((void *)dlreplace, 0, sizeof(*dlreplace));
    vassert(dlowner->ownerUId == Owner->ServerUId);
    dl = dlowner;
  } else {
    dl = dlreplace;
    if (!dl) {
      dl = dldying;
      if (!dl) {
        dl = dlbestdist;
        if (!dl) return nullptr;
      }
    }
  }

  // floodfill visibility check
  if (!skipVisCheck && !isPlr && /*!IsShadowVolumeRenderer() &&*/ r_dynamic_light_better_vis_check) {
    if (leafnum < 0) leafnum = (int)(ptrdiff_t)(Level->PointInSubsector(lorg)-Level->Subsectors);
    if (!IsBspVisSector(leafnum) && !CheckBSPVisibilityBox(lorg, (radius > 0.0f ? radius : 64.0f), &Level->Subsectors[leafnum])) {
      //GCon->Logf("DYNAMIC DROP: visibility check");
      return nullptr;
    }
  }

  // tagged lights are not in the map
  if (dl->ownerUId && !dl->lightid) dlowners.del(dl->ownerUId);

  // clean new light, and return it
  memset((void *)dl, 0, sizeof(*dl));
  dl->ownerUId = (Owner ? Owner->ServerUId : 0);
  dl->origin = lorg;
  dl->radius = radius;
  dl->type = DLTYPE_Point;
  dl->lightid = lightid;
  if (isPlr) dl->flags |= dlight_t::PlayerLight;

  dlinfo[(ptrdiff_t)(dl-DLights)].leafnum = leafnum;

  // tagged lights are not in the map
  if (!lightid && dl->ownerUId) dlowners.put(dl->ownerUId, (vuint32)(ptrdiff_t)(dl-&DLights[0]));

  dl->origOrigin = lorg;
  if (Owner && Owner->IsA(VEntity::StaticClass())) {
    dl->origin += ((VEntity *)Owner)->GetDrawDelta();
  }

  return dl;
}


//==========================================================================
//
//  VRenderLevelShared::FindDlightById
//
//==========================================================================
dlight_t *VRenderLevelShared::FindDlightById (int lightid) {
  if (lightid <= 0) return nullptr;
  dlight_t *dl = DLights;
  for (int i = MAX_DLIGHTS; i--; ++dl) {
    if (dl->die < Level->Time || dl->radius <= 0.0f) continue;
    if (dl->lightid == lightid) return dl;
  }
  return nullptr;
}


//==========================================================================
//
//  VRenderLevelShared::DecayLights
//
//==========================================================================
void VRenderLevelShared::DecayLights (float timeDelta) {
  TFrustum frustum;
  int frustumState = 0; // <0: don't check; >1: inited
  if (!cl) frustumState = -1;
  dlight_t *dl = DLights;
  for (int i = 0; i < MAX_DLIGHTS; ++i, ++dl) {
    if (dl->radius <= 0.0f || dl->die < Level->Time) {
      RL_CLEAR_DLIGHT(dl);
      continue;
    }
    //dl->radius -= timeDelta*(dl->decay/1000.0f);
    dl->radius -= timeDelta*dl->decay;
    // remove small lights too
    if (dl->radius < 2.0f) {
      RL_CLEAR_DLIGHT(dl);
    } else {
      // check if light is out of frustum, and remove it if it is invisible
      if (frustumState == 0) {
        TClipBase cb(refdef.fovx, refdef.fovy);
        if (cb.isValid()) {
          frustum.setup(cb, TFrustumParam(cl->ViewOrg, cl->ViewAngles), true, GetLightMaxDistDef());
          frustumState = (frustum.isValid() ? 1 : -1);
        } else {
          frustumState = -1;
        }
      }
      if (frustumState > 0 && !frustum.checkSphere(dl->origin, dl->radius)) {
        RL_CLEAR_DLIGHT(dl);
      }
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::ThinkerAdded
//
//==========================================================================
void VRenderLevelShared::ThinkerAdded (VThinker * /*Owner*/) {
  //if (!Owner) return;
  //if (!Owner->IsA(VEntity::StaticClass())) return;
  //suid2ent.put(Owner->ServerUId, (VEntity *)Owner);
}


//==========================================================================
//
//  VRenderLevelShared::ThinkerDestroyed
//
//==========================================================================
void VRenderLevelShared::ThinkerDestroyed (VThinker *Owner) {
  if (!Owner) return;
  // remove dynamic light
  auto idxp = dlowners.get(Owner->ServerUId);
  if (idxp) {
    dlight_t *dl = &DLights[*idxp];
    vassert(dl->ownerUId == Owner->ServerUId);
    dl->radius = 0;
    dl->flags = 0;
    dl->ownerUId = 0;
    dlowners.del(Owner->ServerUId);
  }
  // remove static light
  auto stxp = StOwners.get(Owner->ServerUId);
  if (stxp) RemoveStaticLightByIndex(*stxp);
  //suid2ent.del(Owner->ServerUId);
}


//==========================================================================
//
//  VRenderLevelShared::FreeSurfCache
//
//==========================================================================
void VRenderLevelShared::FreeSurfCache (surfcache_t *&) {
}


//==========================================================================
//
//  VRenderLevelShared::ProcessCachedSurfaces
//
//==========================================================================
void VRenderLevelShared::ProcessCachedSurfaces () {
}


//==========================================================================
//
//  VRenderLevelShared::CheckLightPointCone
//
//==========================================================================
float VRenderLevelShared::CheckLightPointCone (VEntity *lowner, const TVec &p, const float radius, const float height, const TVec &coneOrigin, const TVec &coneDir, const float coneAngle) {
  (void)lowner;
  TPlane pl;
  pl.SetPointNormal3D(coneOrigin, coneDir);

  if ((p-coneOrigin).lengthSquared() <= 8.0f) return 1.0f;

  //if (checkSpot && dl.coneAngle > 0.0f && dl.coneAngle < 360.0f)
  if (radius <= 0.0f) {
    if (height <= 0.0f) {
      if (pl.PointOnSide(p)) return 0.0f;
      return p.calcSpotlightAttMult(coneOrigin, coneDir, coneAngle);
    } else {
      const TVec p1(p.x, p.y, p.z+height);
      if (pl.PointOnSide(p)) {
        if (pl.PointOnSide(p1)) return 0.0f;
        return p1.calcSpotlightAttMult(coneOrigin, coneDir, coneAngle);
      } else {
        const float att0 = p.calcSpotlightAttMult(coneOrigin, coneDir, coneAngle);
        if (att0 == 1.0f || pl.PointOnSide(p1)) return att0;
        const float att1 = p1.calcSpotlightAttMult(coneOrigin, coneDir, coneAngle);
        return max2(att0, att1);
      }
    }
  }

  float bbox[6];
  bbox[0+0] = p.x-radius*0.4f;
  bbox[0+1] = p.y-radius*0.4f;
  bbox[0+2] = p.z;
  bbox[3+0] = p.x+radius*0.4f;
  bbox[3+1] = p.y+radius*0.4f;
  bbox[3+2] = p.z+(height > 0.0f ? height : 0.0f);
  if (!pl.checkBox3D(bbox)) return 0.0f;
  float res = calcLightPoint(p, height).calcSpotlightAttMult(coneOrigin, coneDir, coneAngle);
  if (res == 1.0f) return res;
  for (unsigned bi = 0; bi < MAX_BBOX3D_CORNERS; ++bi) {
    const TVec vv(GetBBox3DCorner(bi, bbox));
    if (pl.PointOnSide(vv)) continue;
    const float attn = vv.calcSpotlightAttMult(coneOrigin, coneDir, coneAngle);
    if (attn > res) {
      res = attn;
      if (res == 1.0f) return 1.0f; // it can't be higher than this
    }
  }
  // check box midpoint
  {
    const TVec cv((bbox[0+0]+bbox[3+0])*0.5f, (bbox[0+1]+bbox[3+1])*0.5f, (bbox[0+2]+bbox[3+2])*0.5f);
    const float attn = cv.calcSpotlightAttMult(coneOrigin, coneDir, coneAngle);
    res = max2(res, attn);
  }
  return res;
}


//==========================================================================
//
//  getFPlaneDist
//
//==========================================================================
static inline float getFPlaneDist (const sec_surface_t *floor, const TVec &p) {
  //const float d = floor->PointDistance(p);
  if (floor) {
    const float z = floor->esecplane.GetPointZClamped(p);
    if (floor->esecplane.GetNormalZ() > 0.0f) {
      // floor
      if (p.z < z) {
        GCon->Logf(NAME_Debug, "skip floor: p.z=%g; fz=%g", p.z, z);
        return -1.0f;
      }
      GCon->Logf(NAME_Debug, "floor: p.z=%g; fz=%g; d=%g", p.z, z, p.z-z);
      return p.z-z;
    } else {
      // ceiling
      if (p.z >= z) {
        GCon->Logf(NAME_Debug, "skip ceiling: p.z=%g; cz=%g", p.z, z);
        return -1.0f;
      }
      GCon->Logf(NAME_Debug, "floor: p.z=%g; cz=%g; d=%g", p.z, z, z-p.z);
      return z-p.z;
    }
  } else {
    return -1.0f;
  }
}


//==========================================================================
//
//  VRenderLevelShared::GetNearestFloor
//
//  used to find a lightmap
//  slightly wrong (we should process ceilings too, and use nearest lmap)
//  also note that to process floors/ceilings it is better to use
//  sprite height center
//
//==========================================================================
/*
sec_surface_t *VRenderLevelShared::GetNearestFloor (const subsector_t *sub, const TVec &p) {
  if (!sub) return nullptr;
  sec_surface_t *rfloor = nullptr;
  float bestdist = 999999.0f;
  for (subregion_t *r = sub->regions; r; r = r->next) {
    sec_surface_t *floor;
    // floors
    floor = r->fakefloor;
    if (floor && floor->esecplane.GetNormalZ() > 0.0f) {
      const float z = floor->esecplane.GetPointZClamped(p);
      const float d = p.z-z;
      if (d >= 0.0f && d <= bestdist) {
        bestdist = d;
        rfloor = floor;
      }
    }
    floor = r->realfloor;
    if (floor && floor->esecplane.GetNormalZ() > 0.0f) {
      const float z = floor->esecplane.GetPointZClamped(p);
      const float d = p.z-z;
      if (d >= 0.0f && d <= bestdist) {
        bestdist = d;
        rfloor = floor;
      }
    }
    // ceilings
    floor = r->fakeceil;
    if (floor && floor->esecplane.GetNormalZ() > 0.0f) {
      const float z = floor->esecplane.GetPointZClamped(p);
      const float d = p.z-z;
      if (d >= 0.0f && d <= bestdist) {
        bestdist = d;
        rfloor = floor;
      }
    }
    floor = r->realceil;
    if (floor && floor->esecplane.GetNormalZ() > 0.0f) {
      const float z = floor->esecplane.GetPointZClamped(p);
      const float d = p.z-z;
      if (d >= 0.0f && d <= bestdist) {
        bestdist = d;
        rfloor = floor;
      }
    }
  }
  return rfloor;
}

// get lightmap
int s = (int)(p.dot(rfloor->texinfo.saxisLM));
int t = (int)(p.dot(rfloor->texinfo.taxisLM));
int ds, dt;
for (surface_t *surf = rfloor->surfs; surf; surf = surf->next) {
  if (surf->lightmap == nullptr) continue;
  if (surf->count < 3) continue; // wtf?!
  //if (s < surf->texturemins[0] || t < surf->texturemins[1]) continue;

  ds = s-surf->texturemins[0];
  dt = t-surf->texturemins[1];

  if (ds < 0 || dt < 0 || ds > surf->extents[0] || dt > surf->extents[1]) continue;

  GCon->Logf(NAME_Debug, "  lightmap hit! (%d,%d)", ds, dt);
  if (surf->lightmap_rgb) {
    //l += surf->lightmap[(ds>>4)+(dt>>4)*((surf->extents[0]>>4)+1)];
    const rgb_t *rgbtmp = &surf->lightmap_rgb[(ds>>4)+(dt>>4)*((surf->extents[0]>>4)+1)];
    GCon->Logf(NAME_Debug, "    (%d,%d,%d)", rgbtmp->r, rgbtmp->g, rgbtmp->b);
  } else {
    int ltmp = surf->lightmap[(ds>>4)+(dt>>4)*((surf->extents[0]>>4)+1)];
    GCon->Logf(NAME_Debug, "    (%d)", ltmp);
  }
  break;
}

*/


//==========================================================================
//
//  VRenderLevelShared::CalcEntityStaticLightingFromOwned
//
//==========================================================================
void VRenderLevelShared::CalcEntityStaticLightingFromOwned (VEntity *lowner, const TVec &pt, float radius, float height, float &l, float &lr, float &lg, float &lb, unsigned flags) {
  // fullight by owned lights
  if (!lowner || (flags&LP_IgnoreSelfLights)) return;
  if (!r_static_lights.asBool()) return;

  auto stpp = StOwners.get(lowner->ServerUId);
  if (!stpp) return;

  const light_t *stl = &Lights[*stpp];
  //GCon->Logf(NAME_Debug, "000: own static light for '%s' (%u): flags=0x%04x", lowner->GetClass()->GetName(), lowner->ServerUId, stl->flags);
  if (stl->flags&(dlight_t::Disabled|dlight_t::Subtractive|dlight_t::NoActorLight|dlight_t::NoSelfLight)) return; // check "no self/actor light" flags
  //GCon->Logf(NAME_Debug, "001: own static light for '%s' (%u): flags=0x%04x", lowner->GetClass()->GetName(), lowner->ServerUId, stl->flags);
  /*
  const TVec p = calcLightPoint(pt, height);
  const float distSq = (p-stl->origin).lengthSquared();
  if (distSq >= stl->radius*stl->radius) return; // too far away
  GCon->Logf(NAME_Debug, "002: own static light for '%s' (%u): flags=0x%04x; dist=%g", lowner->GetClass()->GetName(), lowner->ServerUId, stl->flags, sqrtf(distSq));
  */

  //float add = stl->radius-sqrtf(distSq);
  float add = stl->radius;
  //GCon->Logf(NAME_Debug, "003: own static light for '%s' (%u): flags=0x%04x; add=%g", lowner->GetClass()->GetName(), lowner->ServerUId, stl->flags, add);
  if (add > 1.0f) {
    if (stl->coneAngle > 0.0f && stl->coneAngle < 360.0f) {
      const float attn = CheckLightPointCone(lowner, pt, radius, height, stl->origin, stl->coneDirection, stl->coneAngle);
      add *= attn;
      if (add <= 1.0f) return;
    }
    l += add;
    lr += add*((stl->color>>16)&255)/255.0f;
    lg += add*((stl->color>>8)&255)/255.0f;
    lb += add*(stl->color&255)/255.0f;
  }
}


//==========================================================================
//
//  VRenderLevelShared::CalcEntityDynamicLightingFromOwned
//
//==========================================================================
void VRenderLevelShared::CalcEntityDynamicLightingFromOwned (VEntity *lowner, const TVec &pt, float radius, float height, float &l, float &lr, float &lg, float &lb, unsigned flags) {
  // fullight by owned lights
  if (!lowner || (flags&LP_IgnoreSelfLights)) return;
  if (!r_dynamic_lights.asBool()) return;

  auto dlpp = dlowners.get(lowner->ServerUId);
  if (!dlpp) return;

  const dlight_t &dl = DLights[*dlpp];
  if (dl.flags&(dlight_t::Disabled|dlight_t::Subtractive|dlight_t::NoActorLight|dlight_t::NoSelfLight)) return; // check "no self/actor light" flags
  const TVec p = calcLightPoint(pt, height);
  const float distSq = (p-dl.origin).lengthSquared();
  if (distSq >= dl.radius*dl.radius) return; // too far away
  float add = (dl.radius-dl.minlight)-sqrtf(distSq);
  if (add > 1.0f) {
    if (dl.coneAngle > 0.0f && dl.coneAngle < 360.0f) {
      const float attn = CheckLightPointCone(lowner, pt, radius, height, dl.origin, dl.coneDirection, dl.coneAngle);
      add *= attn;
      if (add <= 1.0f) return;
    }
    l += add;
    lr += add*((dl.color>>16)&255)/255.0f;
    lg += add*((dl.color>>8)&255)/255.0f;
    lb += add*(dl.color&255)/255.0f;
  }
}


//==========================================================================
//
//  VRenderLevelShared::CalculateDynLightSub
//
//  this is common code for light point calculation
//  pass light values from ambient pass
//
//==========================================================================
void VRenderLevelShared::CalculateDynLightSub (VEntity *lowner, float &l, float &lr, float &lg, float &lb, const subsector_t *sub, const TVec &pt, float radius, float height, unsigned flags) {
  //WARNING! if `MAX_DLIGHTS` is not 32, remove the last check
  if (r_dynamic_lights && sub->dlightframe == currDLightFrame && sub->dlightbits) {
    const TVec p = calcLightPoint(pt, height);
    const bool dynclip = r_dynamic_clip.asBool();
    const int snum = (int)(ptrdiff_t)(sub-&Level->Subsectors[0]);
    static_assert(sizeof(unsigned) >= sizeof(vuint32), "`unsigned` should be at least of `vuint32` size");
    const unsigned dlbits = (unsigned)sub->dlightbits;
    const bool texCheck = r_lmap_texture_check_dynamic.asBool();
    const float texCheckRadus = r_lmap_texture_check_radius_dynamic.asInt(); // float, to avoid conversions
    for (unsigned i = 0; i < MAX_DLIGHTS; ++i) {
      // check visibility
      if (!(dlbits&(1U<<i))) continue;
      if (!dlinfo[i].isVisible()) continue;
      // reject owned light, because they are processed by another method
      const dlight_t &dl = DLights[i];
      if (lowner && lowner->ServerUId == dl.ownerUId) continue;
      if (dl.flags&(dlight_t::Disabled|dlight_t::Subtractive|dlight_t::NoActorLight)) continue; // check "no self/actor light" flags
      // special processing for API light queries
      if ((flags&LP_IgnoreSelfLights) && lowner && IsInInventory(lowner, dl.ownerUId)) continue;
      if (dl.type&DLTYPE_Subtractive) continue;
      //if (!dl.radius || dl.die < Level->Time) continue; // this is not needed here
      const float distSq = (p-dl.origin).lengthSquared();
      if (distSq >= dl.radius*dl.radius) continue; // too far away
      float add = (dl.radius-dl.minlight)-sqrtf(distSq);
      if (add > 1.0f) {
        // owned light will light the whole object
        //const bool isowned = (lowner && lowner->ServerUId == dl.ownerUId);
        // check potential visibility
        if (dl.coneAngle > 0.0f && dl.coneAngle < 360.0f) {
          const float attn = CheckLightPointCone(lowner, pt, radius, height, dl.origin, dl.coneDirection, dl.coneAngle);
          add *= attn;
          if (add <= 1.0f) continue;
        }
        // trace light that needs shadows
        const int leafnum = dlinfo[i].leafnum;
        if (dynclip && !(dl.flags&dlight_t::NoShadow) && leafnum != snum && dlinfo[i].isNeedTrace()) {
          // trace *to* light, because this is slightly faster for closets and such
          if (!RadiusCastRay((texCheck && dl.radius > texCheckRadus), sub, p, (leafnum >= 0 ? &Level->Subsectors[leafnum] : nullptr), dl.origin, radius)) continue;
        }
        //!if (dl.type&DLTYPE_Subtractive) add = -add;
        l += add;
        lr += add*((dl.color>>16)&255)/255.0f;
        lg += add*((dl.color>>8)&255)/255.0f;
        lb += add*(dl.color&255)/255.0f;
      }
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::CalculateSubStatic
//
//  calculate subsector's light from static light sources
//  (light variables must be initialized)
//
//==========================================================================
void VRenderLevelShared::CalculateSubStatic (VEntity *lowner, float &l, float &lr, float &lg, float &lb, const subsector_t *sub, const TVec &pt, float radius, float height, unsigned flags) {
  if (flags&LP_StrictDynamic) {
    if ((flags&(LP_IgnoreDynLights|LP_IgnoreStatLights)) == (LP_IgnoreDynLights|LP_IgnoreStatLights)) return;
  } else {
    if (flags&LP_IgnoreStatLights) return;
  }
  if (r_static_lights && sub) {
    if (!staticLightsFiltered) RefilterStaticLights();
    const TVec p = calcLightPoint(pt, height);
    const bool dynclip = true; //r_dynamic_clip.asBool();
    const bool texCheck = r_lmap_texture_check_static.asBool();
    // we know for sure what static light may affect the subsector, so there is no need to check 'em all
    const int snum = (int)(ptrdiff_t)(sub-&Level->Subsectors[0]);
    SubStaticLigtInfo *subslinfo = &SubStaticLights[snum];
    for (auto it : subslinfo->touchedStatic.first()) {
      const int idx = it.getKey();
      if (idx < 0 || idx >= Lights.length()) continue;
      const light_t *stl = Lights.ptr()+idx;
      if (!stl->active) continue;
      // ignore owned lights, because they are processed in another method
      if (lowner && lowner->ServerUId == stl->ownerUId) continue;
      if (stl->flags&(dlight_t::Disabled|dlight_t::Subtractive|dlight_t::NoActorLight)) continue; // check "no self/actor light" flags
      // check for "converted"
      if (flags&LP_StrictDynamic) {
        if (flags&(stl->flags&dlight_t::LightSource ? LP_IgnoreStatLights : LP_IgnoreDynLights)) continue;
      }
      // check potential visibility
      const float distSq = (p-stl->origin).lengthSquared();
      if (distSq >= stl->radius*stl->radius) continue; // too far away
      float add = stl->radius-sqrtf(distSq);
      if (add > 1.0f) {
        if (stl->coneAngle > 0.0f && stl->coneAngle < 360.0f) {
          const float attn = CheckLightPointCone(lowner, pt, radius, height, stl->origin, stl->coneDirection, stl->coneAngle);
          add *= attn;
          if (add <= 1.0f) continue;
        }
        if (dynclip && stl->leafnum != snum) {
          // trace *to* light, because this is slightly faster for closets and such
          if (!RadiusCastRay(texCheck, sub, p, (stl->leafnum >= 0 ? &Level->Subsectors[stl->leafnum] : nullptr), stl->origin, radius)) continue;
        }
        l += add;
        lr += add*((stl->color>>16)&255)/255.0f;
        lg += add*((stl->color>>8)&255)/255.0f;
        lb += add*(stl->color&255)/255.0f;
      }
    }
  }
}


//===========================================================================
//
//  Doom-like lighting equation
//
//  level: [0..255]
//
//===========================================================================
static inline float DoomLightingEquation (float llev, float zdist) {
  //const lm = r_light_mode.asInt();
  //if (lm <= 0) return llev;

  // L is the integer llev level used in the game
  const float L = llev; //*255.0;

  const float z = max2(fabsf(zdist), 0.001f);

  const float LightGlobVis = R_CalcGlobVis();

  // more-or-less Doom light level equation
  const float vis = min2(LightGlobVis/z, 24.0f/32.0f);
  const float shade = 2.0f-(L+12.0f)/128.0f;
  float lightscale = shade-vis;

  if (r_light_mode.asInt() >= 2) lightscale = (-floorf(-lightscale*31.0f)-0.5f)/31.0f; // banded

  lightscale = clampval(lightscale, 0.0f, 31.0f/32.0f);
  // `lightscale` is the normalized colormap index (0 bright .. 1 dark)

  return 255.0f*(1.0f-lightscale);
}


//==========================================================================
//
//  VRenderLevelShared::CalculateSubAmbient
//
//  calculate subsector's ambient light
//  (light variables must be initialized)
//
//==========================================================================
void VRenderLevelShared::CalculateSubAmbient (VEntity *lowner, float &l, float &lr, float &lg, float &lb, const subsector_t *sub, const TVec &p, float radius, float height, unsigned flags) {
  (void)lowner; (void)radius;
  unsigned glowFlags = 3u; // bit 0: floor glow allowed; bit 1: ceiling glow allowed
  if (height < 0.0f) height = 0.0f;

  //const TVec p = calcLightPoint(pt, height);
  //FIXME: this is slightly wrong (and slow)
  if (/*!skipAmbient &&*/ sub->regions && !(flags&LP_IgnoreAmbLight)) {
    //sec_region_t *regbase;
    sec_region_t *reglight = Level->PointRegionLight(sub, p, &glowFlags);

    // allow glow only for bottom regions
    //FIXME: this is not right, we should calculate glow for translucent/transparent floors too!
    //glowAllowed = !!(regbase->regflags&sec_region_t::RF_BaseRegion);

    // region's base light
    l = R_GetLightLevel(0, reglight->params->lightlevel+(flags&LP_IgnoreExtraLight ? 0 : ExtraLight));

    int SecLightColor = reglight->params->LightColor;
    if (r_light_mode.asInt() <= 0 || (flags&LP_NoLightFade)) {
      lr = ((SecLightColor>>16)&255)*l/255.0f;
      lg = ((SecLightColor>>8)&255)*l/255.0f;
      lb = (SecLightColor&255)*l/255.0f;
    } else {
      // calculate light fading
      // this transforms point into camera space
      // as we are interested only in distance, it is enough to get transformed `z`
      const TVec mp = p-Drawer->vieworg;
      //TVec pp;
      //pp.x = mp.dot(Drawer->viewright);
      //pp.y = mp.dot(Drawer->viewup);
      //pp.z = mp.dot(Drawer->viewforward);
      const float ppz = mp.dot(Drawer->viewforward);
      const float nl = DoomLightingEquation(l, ppz);
      lr = ((SecLightColor>>16)&255)*nl/255.0f;
      lg = ((SecLightColor>>8)&255)*nl/255.0f;
      lb = (SecLightColor&255)*nl/255.0f;
    }

    // light from floor's lightmap
    //k8: nope, we'll use light sources to calculate this
    #if 0
    /*
    if (!IsShadowVolumeRenderer()) {
      sec_surface_t *rfloor = GetNearestFloor(sub, p);
      if (rfloor) {
        //int s = (int)(p.dot(rfloor->texinfo.saxis)+rfloor->texinfo.soffs);
        //int t = (int)(p.dot(rfloor->texinfo.taxis)+rfloor->texinfo.toffs);
        int s = (int)(p.dot(rfloor->texinfo.saxisLM));
        int t = (int)(p.dot(rfloor->texinfo.taxisLM));
        int ds, dt;
        for (surface_t *surf = rfloor->surfs; surf; surf = surf->next) {
          if (surf->lightmap == nullptr) continue;
          if (surf->count < 3) continue; // wtf?!
          //if (s < surf->texturemins[0] || t < surf->texturemins[1]) continue;

          ds = s-surf->texturemins[0];
          dt = t-surf->texturemins[1];

          if (ds < 0 || dt < 0 || ds > surf->extents[0] || dt > surf->extents[1]) continue;

          if (surf->lightmap_rgb) {
            l += surf->lightmap[(ds>>4)+(dt>>4)*((surf->extents[0]>>4)+1)];
            const rgb_t *rgbtmp = &surf->lightmap_rgb[(ds>>4)+(dt>>4)*((surf->extents[0]>>4)+1)];
            lr += rgbtmp->r;
            lg += rgbtmp->g;
            lb += rgbtmp->b;
          } else {
            int ltmp = surf->lightmap[(ds>>4)+(dt>>4)*((surf->extents[0]>>4)+1)];
            l += ltmp;
            lr += ltmp;
            lg += ltmp;
            lb += ltmp;
          }
          break;
        }
      }
    }
    */
    #endif
  }

  //TODO: glow height
  #define MIX_FLAT_GLOW  \
    if (hgt >= 0.0f && hgt < 120.0f) { \
      /* if (lr == l && lg == l && lb == l) lr = lg = lb = 255; l = 255; */ \
      /*return gtex->glowing|0xff000000u;*/ \
      /*skipAmbient = true;*/ \
      /*glowL = 255.0f;*/ \
      float glowL = (120.0f-hgt)*255.0f/120.0f; \
      float glowR = (gtex->glowing>>16)&0xff; \
      float glowG = (gtex->glowing>>8)&0xff; \
      float glowB = gtex->glowing&0xff; \
      /* mix with glow */ \
      /*glowL *= 0.8f;*/ \
      if (glowL > 1.0f) { \
        /*l *= 0.8f;*/ \
        const float llfrac = 0.8f; /*(l/255.0f)*0.8f;*/ \
        const float glfrac = (glowL/255.0f)*0.9f; \
        lr = clampval(lr*llfrac+glowR*glfrac, 0.0f, 255.0f); \
        lg = clampval(lg*llfrac+glowG*glfrac, 0.0f, 255.0f); \
        lb = clampval(lb*llfrac+glowB*glfrac, 0.0f, 255.0f); \
        l = clampval(l+glowL, 0.0f, 255.0f); \
      } \
    }


  // glowing flats
  if (glowFlags && r_glow_flat && sub->sector && !(flags&LP_IgnoreGlowLights)) {
    //const float hadd = height*(1.0f/8.0f);
    const float hadd = 6.0f;
    const sector_t *sec = sub->sector;
    // fuckin' pasta!
    if ((glowFlags&1u) && sec->floor.pic) {
      VTexture *gtex = GTextureManager(sec->floor.pic);
      if (gtex && gtex->Type != TEXTYPE_Null && gtex->glowing) {
        const float floorz = sub->sector->floor.GetPointZClamped(p);
        if (p.z >= floorz) {
          const float pz = p.z+hadd;
          const float hgt = pz-floorz;
          MIX_FLAT_GLOW
        }
      }
    }
    if ((glowFlags&2u) && sec->ceiling.pic) {
      VTexture *gtex = GTextureManager(sec->ceiling.pic);
      if (gtex && gtex->Type != TEXTYPE_Null && gtex->glowing) {
        const float ceilz = sub->sector->ceiling.GetPointZClamped(p);
        if (p.z <= ceilz) {
          const float pz = min2(ceilz-hadd, p.z+height-hadd);
          const float hgt = ceilz-pz;
          MIX_FLAT_GLOW
        }
      }
    }
  }

  #undef MIX_FLAT_GLOW
}


//==========================================================================
//
//  VRenderLevelShared::LightPoint
//
//  used to calculate lighting for objects
//  can be used without `lowner` to calculate lighting in the given point:
//  in this case, the lighting is assumed to be for non-actor
//
//==========================================================================
vuint32 VRenderLevelShared::LightPoint (VEntity *lowner, const TVec p, float radius, float height, const subsector_t *psub, unsigned flags) {
  if (FixedLight && !(flags&LP_IgnoreFixedLight)) return FixedLight|(FixedLight<<8)|(FixedLight<<16)|(FixedLight<<24);

  const subsector_t *sub = (psub ? psub : Level->PointInSubsector(p));

  float l = 0.0f, lr = 0.0f, lg = 0.0f, lb = 0.0f;

  // calculate ambient light level
  CalculateSubAmbient(lowner, l, lr, lg, lb, sub, p, radius, height, flags);

  // fullight by owned lights
  if (lowner && !(flags&LP_IgnoreSelfLights)) {
    CalcEntityStaticLightingFromOwned(lowner, p, radius, height, l, lr, lg, lb, flags);
    CalcEntityDynamicLightingFromOwned(lowner, p, radius, height, l, lr, lg, lb, flags);
  }

  // add static lights (flags are checked in the callee)
  /*if (IsShadowVolumeRenderer())*/ CalculateSubStatic(lowner, l, lr, lg, lb, sub, p, radius, height, flags);

  // add dynamic lights
  if (!(flags&LP_IgnoreDynLights)) {
    CalculateDynLightSub(lowner, l, lr, lg, lb, sub, p, radius, height, flags);
  }

  return
    (((vuint32)clampToByte((int)l))<<24)|
    (((vuint32)clampToByte((int)lr))<<16)|
    (((vuint32)clampToByte((int)lg))<<8)|
    ((vuint32)clampToByte((int)lb));
}


//==========================================================================
//
//  VRenderLevelShared::LightPointAmbient
//
//  used in advrender to calculate model lighting
//
//==========================================================================
vuint32 VRenderLevelShared::LightPointAmbient (VEntity *lowner, const TVec p, float radius, float height, const subsector_t *psub, unsigned flags) {
  if (FixedLight && !(flags&LP_IgnoreFixedLight)) return FixedLight|(FixedLight<<8)|(FixedLight<<16)|(FixedLight<<24);

  const subsector_t *sub = (psub ? psub : Level->PointInSubsector(p));
  float l = 0.0f, lr = 0.0f, lg = 0.0f, lb = 0.0f;

  CalculateSubAmbient(lowner, l, lr, lg, lb, sub, p, radius, height, flags);

  // fullight by owned lights
  if (lowner && !(flags&LP_IgnoreSelfLights)) {
    CalcEntityStaticLightingFromOwned(lowner, p, radius, height, l, lr, lg, lb, flags);
    CalcEntityDynamicLightingFromOwned(lowner, p, radius, height, l, lr, lg, lb, flags);
  }

  return
    (((vuint32)clampToByte((int)l))<<24)|
    (((vuint32)clampToByte((int)lr))<<16)|
    (((vuint32)clampToByte((int)lg))<<8)|
    ((vuint32)clampToByte((int)lb));
}

#undef RL_CLEAR_DLIGHT



//==========================================================================
//
//  VRenderLevelShared::CalcBSPNodeLMaps
//
//  FIXME:POBJ:
//
//==========================================================================
void VRenderLevelShared::CalcBSPNodeLMaps (int slindex, light_t &sl, int bspnum /*, const float *bbox*/) {
 tailcall:
  //if (!CheckSphereVsAABBIgnoreZ(bbox, sl.origin, sl.radius)) return;

  // found a subsector?
  if (BSPIDX_IS_NON_LEAF(bspnum)) {
    const node_t *bsp = &Level->Nodes[bspnum];
    // decide which side the light is on
    const float dist = bsp->PointDistance(sl.origin);
    if (dist >= sl.radius) {
      // light is completely on the front side
      //return CalcBSPNodeLMaps(slindex, sl, bsp->children[0], bsp->bbox[0]);
      bspnum = bsp->children[0];
      //!bbox = bsp->bbox[0];
      goto tailcall;
    } else if (dist <= -sl.radius) {
      // light is completely on the back side
      //return CalcBSPNodeLMaps(slindex, sl, bsp->children[1], bsp->bbox[1]);
      bspnum = bsp->children[1];
      //!bbox = bsp->bbox[1];
      goto tailcall;
    } else {
      unsigned side = (unsigned)(dist <= 0.0f);
      // recursively divide front space
      CalcBSPNodeLMaps(slindex, sl, bsp->children[side]/*, bsp->bbox[side]*/);
      // possibly divide back space
      side ^= 1;
      //return CalcBSPNodeLMaps(slindex, sl, bsp->children[side], bsp->bbox[side]);
      bspnum = bsp->children[side];
      //!bbox = bsp->bbox[side];
      goto tailcall;
    }
  } else {
    //subsector_t *sub = &Level->Subsectors[BSPIDX_LEAF_SUBSECTOR(bspnum)];
    //CalcSubsectorLMaps(slindex, sl, BSPIDX_LEAF_SUBSECTOR(bspnum));
    const int num = BSPIDX_LEAF_SUBSECTOR(bspnum);
    subsector_t *sub = &Level->Subsectors[num];
    if (sub->isOriginalPObj()) return;
    if (!CheckSphereVs2dAABB(sub->bbox2d, sl.origin, sl.radius)) return;
    // polyobj
    /*
    if (sub->HasPObjs()) {
      for (auto &&it : sub->PObjFirst()) {
        polyobj_t *pobj = it.value();
        sl.touchedPolys.append(pobj);
      }
    }
    */
    sl.touchedSubs.append(sub);
    SubStaticLights[num].touchedStatic.put(slindex, true);
  }
}


//==========================================================================
//
//  VRenderLevelShared::CalcStaticLightTouchingSubs
//
//  FIXME: make this faster!
//  FIXME:POBJ:
//
//==========================================================================
void VRenderLevelShared::CalcStaticLightTouchingSubs (int slindex, light_t &sl) {
  // remove from all subsectors
  if (SubStaticLights.length() < Level->NumSubsectors) SubStaticLights.setLength(Level->NumSubsectors);
  for (auto &&it : sl.touchedSubs) {
    const int snum = (int)(ptrdiff_t)(it-&Level->Subsectors[0]);
    SubStaticLights[snum].touchedStatic.del(slindex);
  }

  sl.touchedSubs.resetNoDtor();
  if (!sl.active || sl.radius < 2.0f) return;

  //sl.touchedPolys.reset();

  if (Level->NumSubsectors < 2) {
    if (Level->NumSubsectors == 1) {
      subsector_t *sub = &Level->Subsectors[0];
      if (!sub->isOriginalPObj()) {
        sl.touchedSubs.append(sub);
        SubStaticLights[0].touchedStatic.put(slindex, true);
      }
    }
  } else {
    //!const float bbox[6] = { -999999.0f, -999999.0f, -999999.0f, +999999.0f, +999999.0f, +999999.0f };
    CalcBSPNodeLMaps(slindex, sl, Level->NumNodes-1/*, bbox*/);
  }
}


//==========================================================================
//
//  VRenderLevelShared::InvalidateStaticLightmapsSubs
//
//==========================================================================
void VRenderLevelShared::InvalidateStaticLightmapsSubs (subsector_t * /*sub*/) {
}


//==========================================================================
//
//  VRenderLevelShared::InvalidatePObjLMaps
//
//==========================================================================
void VRenderLevelShared::InvalidatePObjLMaps (polyobj_t * /*po*/) {
}


//==========================================================================
//
//  VRenderLevelShared::CalcEntityLight
//
//  `flags` is `VDrawer::ELFlag_XXX` set
//  returns 0 for unknown
//
//==========================================================================
vuint32 VRenderLevelShared::CalcEntityLight (VEntity *lowner, unsigned dflags) {
  if (!lowner) return 0;
  const unsigned lflags =
    LP_IgnoreFixedLight|
    LP_IgnoreExtraLight|
    LP_NoLightFade|
    LP_StrictDynamic| // consider "converted" dynlights as dynamic
    (dflags&VDrawer::ELFlag_AllowSelfLights ? LP_NothingZero : LP_IgnoreSelfLights)|
    (dflags&VDrawer::ELFlag_IgnoreDynLights ? LP_IgnoreDynLights : LP_NothingZero)|
    (dflags&VDrawer::ELFlag_IgnoreStaticLights ? LP_IgnoreStatLights : LP_NothingZero)|
    (dflags&VDrawer::ELFlag_IgnoreAmbLights ? LP_IgnoreAmbLight : LP_NothingZero)|
    (dflags&VDrawer::ELFlag_IgnoreGlowLights ? LP_IgnoreGlowLights : LP_NothingZero)|
    LP_NothingZero;
  // we need real light
  //const int oldRM = r_light_mode.asInt();
  //r_light_mode = 0; // switch to real ambient lighting
  vuint32 res = LightPoint(lowner, lowner->Origin, max2(1.0f, lowner->Radius), max2(0.0f, lowner->Height), lowner->SubSector, lflags);
  //r_light_mode = oldRM; // restore lighting mode
  if (res == 0u) res = 0x01000000u;
  return res;
}
