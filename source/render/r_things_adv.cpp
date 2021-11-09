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
//**  Refresh of things, i.e. objects represented by sprites.
//**
//**  Sprite rotation 0 is facing the viewer, rotation 1 is one angle turn
//**  CLOCKWISE around the axis. This is not the same as the angle, which
//**  increases counter clockwise (protractor). There was a lot of stuff
//**  grabbed wrong, so I changed it...
//**
//**************************************************************************
#include "../gamedefs.h"
#include "../psim/p_entity.h"
#include "../psim/p_player.h"
#include "../client/client.h"
#include "r_local.h"


extern VCvarB r_chasecam;
extern VCvarB r_draw_mobjs;
extern VCvarB r_model_shadows;
extern VCvarB r_camera_player_shadows;
extern VCvarB r_model_light;
extern VCvarI r_max_model_shadows;
extern VCvarI r_shadowmap_sprshadows_player;

extern VCvarI r_fix_sprite_offsets;
extern VCvarB r_fix_sprite_offsets_missiles;
extern VCvarB r_fix_sprite_offsets_smart_corpses;
extern VCvarI r_sprite_fix_delta;
extern VCvarB r_use_real_sprite_offset;
extern VCvarB r_use_sprofs_lump;

extern VCvarB gl_crop_sprites;

static VCvarB r_dbg_advthing_dump_actlist("r_dbg_advthing_dump_actlist", false, "Dump built list of active/affected things in advrender?", CVAR_NoShadow);
static VCvarB r_dbg_advthing_dump_ambient("r_dbg_advthing_dump_ambient", false, "Dump rendered ambient things?", CVAR_NoShadow);
static VCvarB r_dbg_advthing_dump_textures("r_dbg_advthing_dump_textures", false, "Dump rendered textured things?", CVAR_NoShadow);

static VCvarB r_dbg_advthing_draw_ambient("r_dbg_advthing_draw_ambient", true, "Draw ambient light for things?", CVAR_NoShadow);
static VCvarB r_dbg_advthing_draw_texture("r_dbg_advthing_draw_texture", true, "Draw textures for things?", CVAR_NoShadow);
static VCvarB r_dbg_advthing_draw_light("r_dbg_advthing_draw_light", true, "Draw textures for things?", CVAR_NoShadow);
static VCvarB r_dbg_advthing_draw_shadow("r_dbg_advthing_draw_shadow", true, "Draw textures for things?", CVAR_NoShadow);
static VCvarB r_dbg_advthing_draw_fog("r_dbg_advthing_draw_fog", true, "Draw fog for things?", CVAR_NoShadow);

VCvarB r_model_advshadow_all("r_model_advshadow_all", false, "Light all alias models, not only those that are in blockmap (slower)?", CVAR_Archive|CVAR_NoShadow);

//static VCvarB r_shadowmap_spr_alias_models("r_shadowmap_spr_alias_models", false, "Render shadows from alias models (based on sprite frame)?", CVAR_Archive|CVAR_NoShadow);

static VCvarB r_shadowmap_spr_monsters("r_shadowmap_spr_monsters", true, "Render fake sprite shadows for monsters?", CVAR_Archive);
static VCvarB r_shadowmap_spr_corpses("r_shadowmap_spr_corpses", true, "Render fake sprite shadows for corpses?", CVAR_Archive);
static VCvarB r_shadowmap_spr_missiles("r_shadowmap_spr_missiles", true, "Render fake sprite shadows for projectiles?", CVAR_Archive);
static VCvarB r_shadowmap_spr_pickups("r_shadowmap_spr_pickups", true, "Render fake sprite shadows for pickups?", CVAR_Archive);
static VCvarB r_shadowmap_spr_decorations("r_shadowmap_spr_decorations", true, "Render fake sprite shadows for decorations?", CVAR_Archive);
static VCvarB r_shadowmap_spr_players("r_shadowmap_spr_players", true, "Render fake sprite shadows for players?", CVAR_Archive);


//==========================================================================
//
//  SetupRenderStyle
//
//  returns `false` if object is not need to be rendered
//
//==========================================================================
static inline bool SetupRenderStyleAndTime (const VEntity *ent, RenderStyleInfo &ri, float &TimeFrac) {
  if (!VRenderLevelDrawer::CalculateRenderStyleInfo(ri, ent->RenderStyle, ent->Alpha, ent->StencilColor)) return false;

  if (ent->State->Time > 0) {
    TimeFrac = 1.0f-(ent->StateTime/ent->State->Time);
    TimeFrac = midval(0.0f, TimeFrac, 1.0f);
  } else {
    TimeFrac = 0;
  }

  return true;
}


//==========================================================================
//
//  VRenderLevelShadowVolume::BuildMobjsInCurrLight
//
//==========================================================================
void VRenderLevelShadowVolume::BuildMobjsInCurrLight (bool doShadows, bool collectSprites) {
  mobjsInCurrLightModels.resetNoDtor();
  mobjsInCurrLightSprites.resetNoDtor();

  const bool modelsAllowed = r_models.asBool();
  if (!r_draw_mobjs || (!collectSprites && !modelsAllowed)) return;

  if (modelsAllowed) {
    useInCurrLightAsLight = true;
    for (auto &&ent : visibleAliasModels) {
      //if (ent == ViewEnt && (!r_chasecam || ent != cl->MO)) continue; // don't draw camera actor
      if (!ent->IsRenderable()) continue;
      if (!IsTouchedByCurrLight(ent)) continue;
      // skip things in subsectors that are not visible by the current light
      if (!doShadows) {
        const int SubIdx = (int)(ptrdiff_t)(ent->SubSector-Level->Subsectors);
        if (!IsSubsectorLitBspVis(SubIdx)) continue;
      }
      mobjsInCurrLightModels.append(ent);
    }
  }

  if (doShadows && collectSprites) {
    RenderStyleInfo ri;
    for (auto &&ent : visibleSprites) {
      // limitation of the current sprite shadow renderer
      if (ent->SpriteType != SPR_VP_PARALLEL_UPRIGHT) continue;
      //if (ent == ViewEnt && (!r_chasecam || ent != cl->MO)) continue; // don't draw camera actor
      if (!ent->IsRenderable()) continue;
      if (!IsTouchedByCurrLight(ent)) continue;
      // skip things in subsectors that are not visible by the current light
      if (!CalculateRenderStyleInfo(ri, ent->RenderStyle, ent->Alpha, ent->StencilColor)) continue; // invisible
      if (!ri.isTranslucent()) mobjsInCurrLightSprites.append(ent);
    }
  }

#if 0
  // build new list
  // if we won't render thing shadows, don't bother trying invisible things
  if (!doShadows || !r_model_shadows) {
    // we already have a list of visible things built
    if (!modelsAllowed) return;
    useInCurrLightAsLight = true;
    const bool doDump = r_dbg_advthing_dump_actlist;
    if (doDump) GCon->Log("=== counting objects ===");
    for (auto &&ent : visibleAliasModels) {
      if (!IsTouchedByCurrLight(ent)) continue;
      if (doDump) GCon->Logf("  <%s> (%f,%f,%f)", *ent->GetClass()->GetFullName(), ent->Origin.x, ent->Origin.y, ent->Origin.z);
      mobjsInCurrLightModels.append(ent);
    }
  } else {
    // we need to render shadows, so process all things
    if (r_model_advshadow_all) {
      //TODO: collect sprires here too?
      if (!modelsAllowed) return;
      useInCurrLightAsLight = true;
      for (auto &&ent : allShadowModelObjects) {
        // skip things in subsectors that are not visible by the current light
        const int SubIdx = (int)(ptrdiff_t)(ent->SubSector-Level->Subsectors);
        if (!IsSubsectorLitVis(SubIdx)) continue;
        if (!IsTouchedByCurrLight(ent)) continue;
        //if (r_dbg_advthing_dump_actlist) GCon->Logf("  <%s> (%f,%f,%f)", *ent->GetClass()->GetFullName(), ent->Origin.x, ent->Origin.y, ent->Origin.z);
        mobjsInCurrLightModels.append(ent);
      }
    } else {
      useInCurrLightAsLight = false;
      const int xl = MapBlock(CurrLightPos.x-CurrLightRadius-Level->BlockMapOrgX/*-MAXRADIUS*/)-1;
      const int xh = MapBlock(CurrLightPos.x+CurrLightRadius-Level->BlockMapOrgX/*+MAXRADIUS*/)+1;
      const int yl = MapBlock(CurrLightPos.y-CurrLightRadius-Level->BlockMapOrgY/*-MAXRADIUS*/)-1;
      const int yh = MapBlock(CurrLightPos.y+CurrLightRadius-Level->BlockMapOrgY/*+MAXRADIUS*/)+1;
      RenderStyleInfo ri;
      for (int bx = xl; bx <= xh; ++bx) {
        for (int by = yl; by <= yh; ++by) {
          for (auto &&it : Level->allBlockThings(bx, by)) {
            VEntity *ent = it.entity();
            if (!ent->IsRenderable()) continue;
            if (ent->GetRenderRadius() < 1) continue;
            //TODO: use `RenderRadius` here to check subsectors
            // skip things in subsectors that are not visible by the current light
            const int SubIdx = (int)(ptrdiff_t)(ent->SubSector-Level->Subsectors);
            if (!IsSubsectorLitBspVis(SubIdx)) continue;
            if (!IsTouchedByCurrLight(ent)) continue;
            const bool isModel = (modelsAllowed ? HasEntityAliasModel(ent) : false);
            //if (collectSprites) GCon->Logf(NAME_Debug, "000: thing:<%s>; model=%d", ent->GetClass()->GetName(), (int)isModel);
            if (!collectSprites && !isModel) continue;
            //if (collectSprites) GCon->Logf(NAME_Debug, "001: thing:<%s>; model=%d", ent->GetClass()->GetName(), (int)isModel);
            //if (!CalculateThingAlpha(ent, RendStyle, Alpha)) continue; // invisible
            if (!CalculateRenderStyleInfo(ri, ent->RenderStyle, ent->Alpha, ent->StencilColor)) continue; // invisible
            //if (collectSprites) GCon->Logf(NAME_Debug, "002: thing:<%s>; model=%d", ent->GetClass()->GetName(), (int)isModel);
            // ignore translucent things, they cannot cast a shadow
            if (!ri.isTranslucent()) {
              //if (collectSprites) GCon->Logf(NAME_Debug, "003: thing:<%s>; model=%d", ent->GetClass()->GetName(), (int)isModel);
              if (isModel) mobjsInCurrLightModels.append(ent); else mobjsInCurrLightSprites.append(ent);
            }
          }
        }
      }
    }
  }
#endif
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderMobjsShadow
//
//==========================================================================
void VRenderLevelShadowVolume::RenderMobjsShadow (VEntity *owner, vuint32 dlflags) {
  if (!r_draw_mobjs || !r_models || !r_model_shadows) return;
  if (!r_dbg_advthing_draw_shadow) return;
  if (dlflags&(dlight_t::NoActorLight|dlight_t::NoActorShadow|dlight_t::NoShadow)) return;
  float TimeFrac;
  RenderStyleInfo ri;
  for (auto &&ent : mobjsInCurrLightModels) {
    if (ent == owner && (dlflags&dlight_t::NoSelfShadow)) continue;
    if (ent->NumRenderedShadows > r_max_model_shadows) continue; // limit maximum shadows for this Entity
    if (!IsShadowAllowedFor(ent)) continue;
    //RenderThingShadow(ent);
    if (SetupRenderStyleAndTime(ent, ri, TimeFrac)) {
      //GCon->Logf("THING SHADOW! (%s)", *ent->GetClass()->GetFullName());
      if (ri.isTranslucent()) continue;
      ri.light = 0xffffffffu;
      ri.fade = 0;
      DrawEntityModel(ent, ri, TimeFrac, RPASS_ShadowVolumes);
      //DrawEntityModel(ent, 0xffffffff, 0, ri, TimeFrac, RPASS_ShadowVolumes);
    }
    ++ent->NumRenderedShadows;
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderMobjsShadowMap
//
//==========================================================================
void VRenderLevelShadowVolume::RenderMobjsShadowMap (VEntity *owner, const unsigned int /*facenum*/, vuint32 dlflags) {
  if (!r_draw_mobjs || !r_models || !r_model_shadows) return;
  if (!r_dbg_advthing_draw_shadow) return;
  if (dlflags&(dlight_t::NoActorLight|dlight_t::NoActorShadow|dlight_t::NoShadow)) return;
  float TimeFrac;
  RenderStyleInfo ri;
  for (auto &&ent : mobjsInCurrLightModels) {
    if (ent == owner && (dlflags&dlight_t::NoSelfShadow)) continue;
    //if (facenum == 0 && ent->NumRenderedShadows > r_max_model_shadows) continue; // limit maximum shadows for this Entity
    if (!IsShadowAllowedFor(ent)) continue;
    //RenderThingShadow(ent);
    if (SetupRenderStyleAndTime(ent, ri, TimeFrac)) {
      //GCon->Logf("THING SHADOW! (%s)", *ent->GetClass()->GetFullName());
      if (ri.isTranslucent()) continue;
      bool renderShadow = true;
      const VEntity::EType tclass = ent->Classify();
      switch (tclass) {
        case VEntity::EType::ET_Unknown: renderShadow = true; break;
        case VEntity::EType::ET_Player: renderShadow = r_shadowmap_spr_players.asBool(); break;
        case VEntity::EType::ET_Missile: renderShadow = r_shadowmap_spr_missiles.asBool(); break;
        case VEntity::EType::ET_Corpse: renderShadow = r_shadowmap_spr_corpses.asBool(); break;
        case VEntity::EType::ET_Monster: renderShadow = r_shadowmap_spr_monsters.asBool(); break;
        case VEntity::EType::ET_Decoration: renderShadow = r_shadowmap_spr_decorations.asBool(); break;
        case VEntity::EType::ET_Pickup: renderShadow = r_shadowmap_spr_pickups.asBool(); break;
        default: abort();
      }
      if (!renderShadow) continue;
      ri.light = 0xffffffffu;
      ri.fade = 0;
      DrawEntityModel(ent, ri, TimeFrac, RPASS_ShadowMaps);
      //DrawEntityModel(ent, 0xffffffff, 0, ri, TimeFrac, RPASS_ShadowVolumes);
    }
    //++ent->NumRenderedShadows;
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderMobjsLight
//
//==========================================================================
void VRenderLevelShadowVolume::RenderMobjsLight (VEntity *owner, vuint32 dlflags) {
  if (!r_draw_mobjs || !r_models || !r_model_light) return;
  if (!r_dbg_advthing_draw_light) return;
  if (dlflags&dlight_t::NoActorLight) return;
  float TimeFrac;
  RenderStyleInfo ri;
  if (useInCurrLightAsLight) {
    // list is already built
    for (auto &&ent : mobjsInCurrLightModels) {
      if (ent == ViewEnt && (!r_chasecam || ent != cl->MO)) continue; // don't draw camera actor
      if (ent == owner) continue; // this is done in ambient pass
      //RenderThingLight(ent);
      if (SetupRenderStyleAndTime(ent, ri, TimeFrac)) {
        ri.light = 0xffffffffu;
        ri.fade = 0;
        DrawEntityModel(ent, ri, TimeFrac, RPASS_Light);
        //DrawEntityModel(ent, 0xffffffff, 0, ri, TimeFrac, RPASS_Light);
      }
    }
  } else {
    for (auto &&ent : visibleAliasModels) {
      if (ent == ViewEnt && (!r_chasecam || ent != cl->MO)) continue; // don't draw camera actor
      if (ent == owner) continue; // this is done in ambient pass
      // skip things in subsectors that are not visible by the current light
      const int SubIdx = (int)(ptrdiff_t)(ent->SubSector-Level->Subsectors);
      if (!IsSubsectorLitBspVis(SubIdx)) continue;
      if (!IsTouchedByCurrLight(ent)) continue;
      //RenderThingLight(ent);
      if (SetupRenderStyleAndTime(ent, ri, TimeFrac)) {
        //if (ri.isTranslucent()) continue;
        ri.light = 0xffffffffu;
        ri.fade = 0;
        DrawEntityModel(ent, ri, TimeFrac, RPASS_Light);
        //DrawEntityModel(ent, 0xffffffff, 0, Alpha, Additive, TimeFrac, RPASS_Light);
      }
    }
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderMobjsAmbient
//
//==========================================================================
void VRenderLevelShadowVolume::RenderMobjsAmbient () {
  if (!r_draw_mobjs || !r_models) return;
  if (!r_dbg_advthing_draw_ambient) return;
  const bool asAmbient = r_model_light;
  const bool doDump = r_dbg_advthing_dump_ambient.asBool();
  float TimeFrac;
  RenderStyleInfo ri;
  if (doDump) GCon->Log("=== ambient ===");
  Drawer->BeginModelsAmbientPass();
  for (auto &&ent : visibleAliasModels) {
    if (ent == ViewEnt && (!r_chasecam || ent != cl->MO)) continue; // don't draw camera actor
    if (doDump) GCon->Logf("  <%s> (%f,%f,%f)", *ent->GetClass()->GetFullName(), ent->Origin.x, ent->Origin.y, ent->Origin.z);
    //RenderThingAmbient(ent);

    if (SetupRenderStyleAndTime(ent, ri, TimeFrac)) {
      //GCon->Logf("  <%s>", *ent->GetClass()->GetFullName());
      if (ri.isTranslucent()) continue;

      SetupRIThingLighting(ent, ri, asAmbient, false/*allowBM*/);
      ri.fade = 0;

      //DrawEntityModel(ent, light, 0, Alpha, Additive, TimeFrac, RPASS_Ambient);
      DrawEntityModel(ent, ri, TimeFrac, RPASS_Ambient);
    }
  }
  Drawer->EndModelsAmbientPass();
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderMobjsTextures
//
//==========================================================================
void VRenderLevelShadowVolume::RenderMobjsTextures () {
  if (!r_draw_mobjs || !r_models) return;
  if (!r_dbg_advthing_draw_texture) return;
  const bool doDump = r_dbg_advthing_dump_textures.asBool();
  float TimeFrac;
  RenderStyleInfo ri;
  if (doDump) GCon->Log("=== textures ===");
  Drawer->BeginModelsTexturesPass();
  for (auto &&ent : visibleAliasModels) {
    if (ent == ViewEnt && (!r_chasecam || ent != cl->MO)) continue; // don't draw camera actor
    if (doDump) GCon->Logf("  <%s> (%f,%f,%f)", *ent->GetClass()->GetFullName(), ent->Origin.x, ent->Origin.y, ent->Origin.z);
    //RenderThingTextures(ent);
    if (SetupRenderStyleAndTime(ent, ri, TimeFrac)) {
      //if (ri.alpha < 1.0f) continue; // wtf?!
      if (ri.isTranslucent()) continue;
      ri.light = 0xffffffffu;
      ri.fade = 0;
      DrawEntityModel(ent, ri, TimeFrac, RPASS_Textures);
      //DrawEntityModel(ent, 0xffffffff, 0, Alpha, Additive, TimeFrac, RPASS_Textures);
    }
  }
  Drawer->EndModelsTexturesPass();
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderMobjsFog
//
//==========================================================================
void VRenderLevelShadowVolume::RenderMobjsFog () {
  if (!r_draw_mobjs || !r_models) return;
  if (!r_dbg_advthing_draw_fog) return;
  float TimeFrac;
  RenderStyleInfo ri;
  Drawer->BeginModelsFogPass();
  for (auto &&ent : visibleAliasModels) {
    if (ent == ViewEnt && (!r_chasecam || ent != cl->MO)) continue; // don't draw camera actor
    //RenderThingFog(ent);
    if (SetupRenderStyleAndTime(ent, ri, TimeFrac)) {
      if (ri.isAdditive()) continue;
      vuint32 Fade = GetFade(Level->PointRegionLight(ent->SubSector, ent->Origin));
      if (Fade) {
        ri.light = 0xffffffffu;
        ri.fade = Fade;
        DrawEntityModel(ent, ri, TimeFrac, RPASS_Fog);
        //DrawEntityModel(ent, 0xffffffff, Fade, Alpha, Additive, TimeFrac, RPASS_Fog);
      }
    }
  }
  Drawer->EndModelsFogPass();
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderMobjSpriteShadowMap
//
//==========================================================================
void VRenderLevelShadowVolume::RenderMobjSpriteShadowMap (VEntity *owner, const unsigned int facenum, int spShad, vuint32 dlflags) {
  if (dlflags&(dlight_t::NoActorLight|dlight_t::NoActorShadow|dlight_t::NoShadow)) return;
  if (spShad < 1) return;
  //const bool doPlayer = r_shadowmap_spr_player.asBool();

  for (auto &&mo : mobjsInCurrLightSprites) {
    if (mo == owner /*&& (dlflags&dlight_t::NoSelfShadow)*/) continue; // always

    bool renderShadow = true;
    const VEntity::EType tclass = mo->Classify();
    switch (tclass) {
      case VEntity::EType::ET_Unknown: renderShadow = true; break;
      case VEntity::EType::ET_Player: renderShadow = r_shadowmap_spr_players.asBool(); break;
      case VEntity::EType::ET_Missile: renderShadow = r_shadowmap_spr_missiles.asBool(); break;
      case VEntity::EType::ET_Corpse: renderShadow = r_shadowmap_spr_corpses.asBool(); break;
      case VEntity::EType::ET_Monster: renderShadow = r_shadowmap_spr_monsters.asBool(); break;
      case VEntity::EType::ET_Decoration: renderShadow = r_shadowmap_spr_decorations.asBool(); break;
      case VEntity::EType::ET_Pickup: renderShadow = r_shadowmap_spr_pickups.asBool(); break;
      default: abort();
    }
    if (!renderShadow) continue;

    RenderMobjShadowMapSprite(mo, facenum, (spShad > 1));
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderMobjShadowMapSprite
//
//==========================================================================
void VRenderLevelShadowVolume::RenderMobjShadowMapSprite (VEntity *ent, const unsigned int /*facenum*/, const bool allowRotating) {
  const int sprtype = ent->SpriteType;
  if (sprtype != SPR_VP_PARALLEL_UPRIGHT) return;

  //GCon->Logf(NAME_Debug, "r00: thing:<%s>", ent->GetClass()->GetName());

  TVec sprorigin = ent->GetDrawOrigin();

  spritedef_t *sprdef;
  spriteframe_t *sprframe;

  int SpriteIndex = ent->GetEffectiveSpriteIndex();
  int FrameIndex = ent->GetEffectiveSpriteFrame();
  if (ent->FixedSpriteName != NAME_None) SpriteIndex = VClass::FindSprite(ent->FixedSpriteName); // don't append

  if ((unsigned)SpriteIndex >= (unsigned)sprites.length()) return;

  // decide which patch to use for sprite relative to player
  sprdef = &sprites.ptr()[SpriteIndex];
  if (FrameIndex >= sprdef->numframes) return;

  sprframe = &sprdef->spriteframes[FrameIndex];

  //GCon->Logf(NAME_Debug, "r01: thing:<%s>; rotate=%d", ent->GetClass()->GetName(), (int)sprframe->rotate);
  //FIXME: precalc this
  if (!allowRotating && sprframe->rotate) {
    for (unsigned int f = 1; f < 16; ++f) if (sprframe->lump[0] != sprframe->lump[f]) return;
  }

  // use single rotation for all views
  int lump = sprframe->lump[0];
  bool flip = sprframe->flip[0];

  // always look at the light source
  if (sprframe->rotate) {
    float ang = matan(sprorigin.y-CurrLightPos.y, sprorigin.x-CurrLightPos.x);
    if (!isFiniteF(ang)) ang = matan(sprorigin.y-CurrLightPos.y, sprorigin.x-CurrLightPos.x+1);
    const float angadd = (sprframe->lump[0] == sprframe->lump[1] ? 45.0f*0.5f : 45.0f/4.0f); //k8: is this right?
    ang = AngleMod(ang-ent->GetSpriteDrawAngles().yaw+180.0f+angadd);
    const unsigned rot = (unsigned)(ang*16.0f/360.0f)&15;
    lump = sprframe->lump[rot];
    flip = sprframe->flip[rot];
  }

  if (lump <= 0) return; // sprite lump is not present

  VTexture *Tex = GTextureManager[lump];
  if (!Tex || Tex->Type == TEXTYPE_Null) return; // just in case

  // need to call cropper here, because if the texture is not cached yet, its dimensions are wrong
  if (gl_crop_sprites.asBool()) Tex->CropTexture();

  // always look at the light source
  TVec viewforward = ent->Origin-CurrLightPos;
  viewforward.z = 0;
  float len = viewforward.x*viewforward.x+viewforward.y*viewforward.y;
  if (len >= 0.0001f) {
    len = 1.0f/sqrtf(len);
    viewforward.x *= len;
    viewforward.y *= len;
  } else {
    // dunno what to do here
    viewforward = TVec(1.0f, 0.0f, 0.0f);
  }

  // Generate the sprite's axes, with sprup straight up in worldspace,
  // and sprright parallel to the viewplane. This will not work if the
  // view direction is very close to straight up or down, because the
  // cross product will be between two nearly parallel vectors and
  // starts to approach an undefined state, so we don't draw if the two
  // vectors are less than 1 degree apart
  //const float dot = viewforward.z; // same as viewforward.dot(sprup), because sprup is 0, 0, 1
  //if (fabsf(dot) > 0.999848f) return; // cos(1 degree) = 0.999848f
  const TVec sprup(0.0f, 0.0f, 1.0f);
  TVec sprright = TVec(viewforward.y, -viewforward.x, 0.0f);
  TVec sprforward = TVec(-sprright.y, sprright.x, 0.0f);

  int fixAlgo = r_fix_sprite_offsets.asInt();
  if (fixAlgo < 0 || ent->IsFloatBob()) fixAlgo = 0; // just in case

  int TexWidth = Tex->GetWidth();
  int TexHeight = Tex->GetHeight();
  int TexSOffset = (fixAlgo > 1 && Tex->bForcedSpriteOffset && r_use_sprofs_lump ? Tex->SOffsetFix : Tex->SOffset);

  float scaleX = max2(0.001f, ent->ScaleX/Tex->SScale);
  float scaleY = max2(0.001f, ent->ScaleY/Tex->TScale);

  TVec sv[4];

  TVec start = -TexSOffset*sprright*scaleX;
  TVec end = (TexWidth-TexSOffset)*sprright*scaleX;

  int TexTOffset, dummy;
  FixSpriteOffset(fixAlgo, ent, Tex, TexHeight, scaleY, TexTOffset, dummy);

  TVec topdelta = TexTOffset*sprup*scaleY;
  TVec botdelta = (TexTOffset-TexHeight)*sprup*scaleY;

  sv[0] = sprorigin+start+botdelta;
  sv[1] = sprorigin+start+topdelta;
  sv[2] = sprorigin+end+topdelta;
  sv[3] = sprorigin+end+botdelta;

  if (!isSpriteInSpotlight(sv)) return;

  /*
  sv: sprite vertices (4)
  normal: -sprforward
  saxis: (flip ? -sprright : sprright)/scaleX
  taxit: -sprup/scaleY
  texorg: (flip ? sv[2] : sv[1])

        Drawer->DrawSpritePolygon((Level ? Level->Time : 0.0f), spr.Verts, GTextureManager[spr.lump],
                                  spr.rstyle, GetTranslation(spr.translation),
                                  ColorMap, spr.normal, spr.pdist,
                                  spr.saxis, spr.taxis, spr.texorg);
  */
  //GCon->Logf(NAME_Debug, "r02: thing:<%s>", ent->GetClass()->GetName());
  Drawer->DrawSpriteShadowMap(sv, Tex, -sprforward/*normal*/, (flip ? -sprright : sprright)/scaleX/*saxis*/, -sprup/scaleY/*taxis*/, (flip ? sv[2] : sv[1])/*texorg*/);
}
