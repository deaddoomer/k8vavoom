//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
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

// HEADER FILES ------------------------------------------------------------

#include "gamedefs.h"
#include "r_local.h"
#include "sv_local.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

enum
{
  SPR_VP_PARALLEL_UPRIGHT,
  SPR_FACING_UPRIGHT,
  SPR_VP_PARALLEL,
  SPR_ORIENTED,
  SPR_VP_PARALLEL_ORIENTED,
  SPR_VP_PARALLEL_UPRIGHT_ORIENTED,
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern VCvarB   r_chasecam;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

VCvarB      r_draw_mobjs("r_draw_mobjs", true, "Draw mobjs?", CVAR_Archive);
VCvarB      r_draw_psprites("r_draw_psprites", true, "Draw psprites?", CVAR_Archive);
VCvarB      r_models("r_models", true, "Allow models?", CVAR_Archive);
VCvarB      r_hide_models("r_hide_models", false, "Hide models?", CVAR_Archive);
VCvarB      r_view_models("r_view_models", true, "View models?", CVAR_Archive);
VCvarB      r_model_shadows("r_model_shadows", false, "Draw model shadows in advanced renderer?", CVAR_Archive);
VCvarB      r_model_light("r_model_light", true, "Draw model light in advanced renderer?", CVAR_Archive);
VCvarB      r_sort_sprites("r_sort_sprites", false, "Sprite sorting.");
VCvarB      r_fix_sprite_offsets("r_fix_sprite_offsets", true, "Fix sprite offsets?", CVAR_Archive);
VCvarI      r_sprite_fix_delta("r_sprite_fix_delta", "-3", "Sprite offset amount.", CVAR_Archive);
VCvarB      r_drawfuzz("r_drawfuzz", false, "Draw fuzz effect?", CVAR_Archive);
VCvarF      transsouls("transsouls", "1.0", "Translucent Lost Souls?", CVAR_Archive);
VCvarI      crosshair("crosshair", "2", "Crosshair type (0-2).", CVAR_Archive);
VCvarF      crosshair_alpha("crosshair_alpha", "0.6", "Crosshair opacity.", CVAR_Archive);

static VCvarI r_crosshair_yofs("r_crosshair_yofs", "0", "Crosshair y offset (>0: down).", CVAR_Archive);

// CODE --------------------------------------------------------------------

//==========================================================================
//
//  VRenderLevelShared::DrawTranslucentPoly
//
//==========================================================================

void VRenderLevelShared::DrawTranslucentPoly(surface_t *surf, TVec *sv,
  int count, int lump, float Alpha, bool Additive, int translation,
  bool type, vuint32 light, vuint32 Fade, const TVec &normal, float pdist,
  const TVec &saxis, const TVec &taxis, const TVec &texorg)
{
  guard(VRenderLevelShared::DrawTranslucentPoly);
  int i;

  TVec mid(0, 0, 0);
  for (i = 0; i < count; i++)
  {
    mid += sv[i];
  }
  mid /= count;
  float dist = fabs(DotProduct(mid - vieworg, viewforward));
//  float dist = Length(mid - vieworg);
  int found = -1;
  float best_dist = -1;
  for (i = 0; i < MAX_TRANS_SPRITES; i++)
  {
    trans_sprite_t &spr = trans_sprites[i];
    if (!spr.Alpha)
    {
      if (type)
        memcpy(spr.Verts, sv, sizeof(TVec) * 4);
      spr.dist = dist;
      spr.lump = lump;
      spr.normal = normal;
      spr.pdist = pdist;
      spr.saxis = saxis;
      spr.taxis = taxis;
      spr.texorg = texorg;
      spr.surf = surf;
      spr.Alpha = Alpha;
      spr.Additive = Additive;
      spr.translation = translation;
      spr.type = type;
      spr.light = light;
      spr.Fade = Fade;
      return;
    }
    if (spr.dist > best_dist)
    {
      found = i;
      best_dist = spr.dist;
    }
  }
  if (best_dist > dist)
  {
    //  All slots are full, draw and replace a far away sprite
    trans_sprite_t &spr = trans_sprites[found];
    if (spr.type == 2)
    {
      DrawEntityModel(spr.Ent, spr.light, spr.Fade, spr.Alpha,
        spr.Additive, spr.TimeFrac, RPASS_Normal);
    }
    else if (spr.type)
    {
      Drawer->DrawSpritePolygon(spr.Verts, GTextureManager[spr.lump],
        spr.Alpha, spr.Additive, GetTranslation(spr.translation),
        ColourMap, spr.light, spr.Fade, spr.normal, spr.pdist,
        spr.saxis, spr.taxis, spr.texorg);
    }
    else
    {
      check(spr.surf);
      Drawer->DrawMaskedPolygon(spr.surf, spr.Alpha, spr.Additive);
    }
    if (type)
      memcpy(spr.Verts, sv, sizeof(TVec) * 4);
    spr.dist = dist;
    spr.lump = lump;
    spr.normal = normal;
    spr.pdist = pdist;
    spr.saxis = saxis;
    spr.taxis = taxis;
    spr.texorg = texorg;
    spr.surf = surf;
    spr.Alpha = Alpha;
    spr.Additive = Additive;
    spr.translation = translation;
    spr.type = type;
    spr.light = light;
    spr.Fade = Fade;
    return;
  }

  //  All slots are full and are nearer to current sprite so draw it
  if (type)
  {
    Drawer->DrawSpritePolygon(sv, GTextureManager[lump], Alpha,
      Additive, GetTranslation(translation), ColourMap, light, Fade,
      normal, pdist, saxis, taxis, texorg);
  }
  else
  {
    check(surf);
    Drawer->DrawMaskedPolygon(surf, Alpha, Additive);
  }
  unguard;
}

//==========================================================================
//
//  VRenderLevelShared::RenderSprite
//
//==========================================================================

void VRenderLevelShared::RenderSprite(VEntity *thing, vuint32 light,
  vuint32 Fade, float Alpha, bool Additive)
{
  guard(VRenderLevelShared::RenderSprite);
  int spr_type = thing->SpriteType;

  TVec sprorigin = thing->Origin;
  sprorigin.z -= thing->FloorClip;
  TVec sprforward;
  TVec sprright;
  TVec sprup;

  float   dot;
  TVec    tvec;
  float   sr;
  float   cr;

  switch (spr_type)
  {
  case SPR_VP_PARALLEL_UPRIGHT:
    //  Generate the sprite's axes, with sprup straight up in worldspace,
    // and sprright parallel to the viewplane. This will not work if the
    // view direction is very close to straight up or down, because the
    // cross product will be between two nearly parallel vectors and
    // starts to approach an undefined state, so we don't draw if the two
    // vectors are less than 1 degree apart
    dot = viewforward.z;  //  same as DotProduct(viewforward, sprup)
                // because sprup is 0, 0, 1
    if ((dot > 0.999848) || (dot < -0.999848))  // cos(1 degree) = 0.999848
      return;

    sprup = TVec(0, 0, 1);
    //  CrossProduct(sprup, viewforward)
    sprright = Normalise(TVec(viewforward.y, -viewforward.x, 0));
    //  CrossProduct(sprright, sprup)
    sprforward = TVec(-sprright.y, sprright.x, 0);
    break;

  case SPR_FACING_UPRIGHT:
    //  Generate the sprite's axes, with sprup straight up in worldspace,
    // and sprright perpendicular to sprorigin. This will not work if the
    // view direction is very close to straight up or down, because the
    // cross product will be between two nearly parallel vectors and
    // starts to approach an undefined state, so we don't draw if the two
    // vectors are less than 1 degree apart
    tvec = Normalise(sprorigin - vieworg);
    dot = tvec.z; //  same as DotProduct (tvec, sprup) because
            // sprup is 0, 0, 1
    if ((dot > 0.999848) || (dot < -0.999848))  // cos(1 degree) = 0.999848
      return;
    sprup = TVec(0, 0, 1);
    //  CrossProduct(sprup, -sprorigin)
    sprright = Normalise(TVec(tvec.y, -tvec.x, 0));
    //  CrossProduct(sprright, sprup)
    sprforward = TVec(-sprright.y, sprright.x, 0);
    break;

  case SPR_VP_PARALLEL:
    //  Generate the sprite's axes, completely parallel to the viewplane.
    // There are no problem situations, because the sprite is always in
    // the same position relative to the viewer
    sprup = viewup;
    sprright = viewright;
    sprforward = viewforward;
    break;

  case SPR_ORIENTED:
    //  Generate the sprite's axes, according to the sprite's world
    // orientation
    AngleVectors(thing->Angles, sprforward, sprright, sprup);
    break;

  case SPR_VP_PARALLEL_ORIENTED:
    //  Generate the sprite's axes, parallel to the viewplane, but
    // rotated in that plane around the centre according to the sprite
    // entity's roll angle. So sprforward stays the same, but sprright
    // and sprup rotate
    sr = msin(thing->Angles.roll);
    cr = mcos(thing->Angles.roll);

    sprforward = viewforward;
    sprright = TVec(viewright.x * cr + viewup.x * sr, viewright.y * cr +
      viewup.y * sr, viewright.z * cr + viewup.z * sr);
    sprup = TVec(viewright.x * -sr + viewup.x * cr, viewright.y * -sr +
      viewup.y * cr, viewright.z * -sr + viewup.z * cr);
    break;

  case SPR_VP_PARALLEL_UPRIGHT_ORIENTED:
    //  Generate the sprite's axes, with sprup straight up in worldspace,
    // and sprright parallel to the viewplane and then rotated in that
    // plane around the centre according to the sprite entity's roll
    // angle. So sprforward stays the same, but sprright and sprup rotate
    // This will not work if the view direction is very close to straight
    // up or down, because the cross product will be between two nearly
    // parallel vectors and starts to approach an undefined state, so we
    // don't draw if the two vectors are less than 1 degree apart
    dot = viewforward.z;  //  same as DotProduct(viewforward, sprup)
                // because sprup is 0, 0, 1
    if ((dot > 0.999848) || (dot < -0.999848))  // cos(1 degree) = 0.999848
      return;

    sr = msin(thing->Angles.roll);
    cr = mcos(thing->Angles.roll);

    //  CrossProduct(TVec(0, 0, 1), viewforward)
    tvec = Normalise(TVec(viewforward.y, -viewforward.x, 0));
    //  CrossProduct(tvec, TVec(0, 0, 1))
    sprforward = TVec(-tvec.y, tvec.x, 0);
    //  Rotate
    sprright = TVec(tvec.x * cr, tvec.y * cr, tvec.z * cr + sr);
    sprup = TVec(tvec.x * -sr, tvec.y * -sr, tvec.z * -sr + cr);
    break;

  default:
    Sys_Error("RenderSprite: Bad sprite type %d", spr_type);
  }

  spritedef_t *sprdef;
  spriteframe_t *sprframe;

  VState *DispState = (thing->EntityFlags & VEntity::EF_UseDispState) ?
    thing->DispState : thing->State;
  int SpriteIndex = DispState->SpriteIndex;
  if (thing->FixedSpriteName != NAME_None)
  {
    SpriteIndex = VClass::FindSprite(thing->FixedSpriteName);
  }

  // decide which patch to use for sprite relative to player
  if ((unsigned)SpriteIndex >= MAX_SPRITE_MODELS)
  {
#ifdef PARANOID
    GCon->Logf(NAME_Dev, "Invalid sprite number %d", SpriteIndex);
#endif
    return;
  }
  sprdef = &sprites[SpriteIndex];
  if ((DispState->Frame & VState::FF_FRAMEMASK) >= sprdef->numframes)
  {
#ifdef PARANOID
    GCon->Logf(NAME_Dev, "Invalid sprite frame %d : %d",
      SpriteIndex, DispState->Frame);
#endif
    return;
  }
  sprframe = &sprdef->spriteframes[DispState->Frame & VState::FF_FRAMEMASK];

  int     lump;
  bool    flip;

  if (sprframe->rotate)
  {
    // choose a different rotation based on player view
    //FIXME must use sprforward here?
    float ang = matan(sprorigin.y - vieworg.y,
      sprorigin.x - vieworg.x);
    if (sprframe->lump[0] == sprframe->lump[1])
    {
      ang = AngleMod(ang - thing->Angles.yaw + 180.0 + 45.0 / 2.0);
    }
    else
    {
      ang = matan(sprforward.y + viewforward.y,
        sprforward.x + viewforward.x);
      ang = AngleMod(ang - thing->Angles.yaw + 180.0 + 45.0 / 4.0);
    }
    vuint32 rot = (vuint32)(ang * 16.0 / 360.0) & 15;
    lump = sprframe->lump[rot];
    flip = sprframe->flip[rot];
  }
  else
  {
    // use single rotation for all views
    lump = sprframe->lump[0];
    flip = sprframe->flip[0];
  }
  if (lump <= 0)
  {
#ifdef PARANOID
    GCon->Logf(NAME_Dev, "Sprite frame %d : %d, not present",
      SpriteIndex, DispState->Frame);
#endif
    // Sprite lump is not present
    return;
  }
  VTexture *Tex = GTextureManager[lump];
  int TexWidth = Tex->GetWidth();
  int TexHeight = Tex->GetHeight();
  int TexSOffset = Tex->SOffset;
  int TexTOffset = Tex->TOffset;

  TVec  sv[4];

  TVec start = -TexSOffset * sprright * thing->ScaleX;
  TVec end = (TexWidth - TexSOffset) * sprright * thing->ScaleX;

  if (r_fix_sprite_offsets && TexTOffset < TexHeight &&
    2 * TexTOffset + r_sprite_fix_delta >= TexHeight)
  {
    TexTOffset = TexHeight;
  }
  TVec topdelta = TexTOffset * sprup * thing->ScaleY;
  TVec botdelta = (TexTOffset - TexHeight) * sprup * thing->ScaleY;

  sv[0] = sprorigin + start + botdelta;
  sv[1] = sprorigin + start + topdelta;
  sv[2] = sprorigin + end + topdelta;
  sv[3] = sprorigin + end + botdelta;

  if (Alpha < 1.0 || Additive || r_sort_sprites)
  {
    DrawTranslucentPoly(nullptr, sv, 4, lump, Alpha, Additive,
      thing->Translation, true, light, Fade, -sprforward, DotProduct(
      sprorigin, -sprforward), (flip ? -sprright : sprright) /
      thing->ScaleX, -sprup / thing->ScaleY, flip ? sv[2] : sv[1]);
  }
  else
  {
    Drawer->DrawSpritePolygon(sv, GTextureManager[lump], Alpha,
      Additive, GetTranslation(thing->Translation), ColourMap, light,
      Fade, -sprforward, DotProduct(sprorigin, -sprforward),
      (flip ? -sprright : sprright) / thing->ScaleX,
      -sprup / thing->ScaleY, flip ? sv[2] : sv[1]);
  }
  unguard;
}

//==========================================================================
//
//  VRenderLevelShared::RenderTranslucentAliasModel
//
//==========================================================================

void VRenderLevelShared::RenderTranslucentAliasModel(VEntity *mobj,
  vuint32 light, vuint32 Fade, float Alpha, bool Additive, float TimeFrac)
{
  guard(VRenderLevelShared::RenderTranslucentAliasModel);
  int i;

  float dist = fabs(DotProduct(mobj->Origin - vieworg, viewforward));
  int found = -1;
  float best_dist = -1;
  for (i = 0; i < MAX_TRANS_SPRITES; i++)
  {
    trans_sprite_t &spr = trans_sprites[i];
    if (!spr.Alpha)
    {
      spr.Ent = mobj;
      spr.light = light;
      spr.Fade = Fade;
      spr.Alpha = Alpha;
      spr.Additive = Additive;
      spr.dist = dist;
      spr.type = 2;
      spr.TimeFrac = TimeFrac;
      return;
    }
    if (spr.dist > best_dist)
    {
      found = i;
      best_dist = spr.dist;
    }
  }
  if (best_dist > dist)
  {
    //  All slots are full, draw and replace a far away sprite
    trans_sprite_t &spr = trans_sprites[found];
    if (spr.type == 2)
    {
      DrawEntityModel(spr.Ent, spr.light, spr.Fade, spr.Alpha,
        spr.Additive, spr.TimeFrac, RPASS_Normal);
    }
    else if (spr.type)
    {
      Drawer->DrawSpritePolygon(spr.Verts, GTextureManager[spr.lump],
        spr.Alpha, spr.Additive, GetTranslation(spr.translation),
        ColourMap, spr.light, spr.Fade, spr.normal, spr.pdist,
        spr.saxis, spr.taxis, spr.texorg);
    }
    else
    {
      check(spr.surf);
      Drawer->DrawMaskedPolygon(spr.surf, spr.Alpha, spr.Additive);
    }
    spr.Ent = mobj;
    spr.light = light;
    spr.Fade = Fade;
    spr.Alpha = Alpha;
    spr.Additive = Additive;
    spr.dist = dist;
    spr.type = 2;
    spr.TimeFrac = TimeFrac;
    return;
  }
  DrawEntityModel(mobj, light, Fade, Alpha, Additive, TimeFrac, RPASS_Normal);
  unguard;
}

//==========================================================================
//
//  VRenderLevelShared::RenderAliasModel
//
//==========================================================================

bool VRenderLevelShared::RenderAliasModel(VEntity *mobj, vuint32 light,
  vuint32 Fade, float Alpha, bool Additive, ERenderPass Pass)
{
  guard(VRenderLevelShared::RenderAliasModel);
  if (!r_models)
  {
    return false;
  }

  float TimeFrac = 0;
  if (mobj->State->Time > 0)
  {
    TimeFrac = 1.0 - (mobj->StateTime / mobj->State->Time);
    TimeFrac = MID(0.0, TimeFrac, 1.0);
  }

  //  Draw it
  if (Alpha < 1.0 || Additive)
  {
    if (!CheckAliasModelFrame(mobj, TimeFrac))
    {
      return false;
    }
    RenderTranslucentAliasModel(mobj, light, Fade, Alpha, Additive,
      TimeFrac);
    return true;
  }
  else
  {
    return DrawEntityModel(mobj, light, Fade, 1.0, false, TimeFrac,
      Pass);
  }
  unguard;
}

//==========================================================================
//
//  VRenderLevelShared::RenderThing
//
//==========================================================================

void VRenderLevelShared::RenderThing(VEntity *mobj, ERenderPass Pass)
{
  guard(VRenderLevelShared::RenderThing);
  if (mobj == ViewEnt && (!r_chasecam || ViewEnt != cl->MO))
  {
    //  Don't draw camera actor.
    return;
  }

  if ((mobj->EntityFlags & VEntity::EF_NoSector) ||
    (mobj->EntityFlags & VEntity::EF_Invisible))
  {
    return;
  }

  if (!mobj->State)
  {
    return;
  }

  //  Skip things in subsectors that are not visible.
  int SubIdx = mobj->SubSector - Level->Subsectors;
  if (!(BspVis[SubIdx >> 3] & (1 << (SubIdx & 7))))
  {
    return;
  }

  int RendStyle = mobj->RenderStyle;
  float Alpha = mobj->Alpha;
  bool Additive = false;

  if (RendStyle == STYLE_SoulTrans)
  {
    RendStyle = STYLE_Translucent;
    Alpha = transsouls;
  }
  else if (RendStyle == STYLE_OptFuzzy)
  {
    RendStyle = r_drawfuzz ? STYLE_Fuzzy : STYLE_Translucent;
  }

  switch (RendStyle)
  {
  case STYLE_None:
    return;

  case STYLE_Normal:
    Alpha = 1.0;
    break;

  case STYLE_Fuzzy:
    Alpha = FUZZY_ALPHA;
    break;

  case STYLE_Add:
    Additive = true;
    break;
  }
  Alpha = MID(0.0, Alpha, 1.0);

  if (!Alpha)
  {
    // Never make a vissprite when MF2_DONTDRAW is flagged.
    return;
  }

  //  Setup lighting
  vuint32 light;
  if (RendStyle == STYLE_Fuzzy)
  {
    light = 0;
  }
  else if ((mobj->State->Frame & VState::FF_FULLBRIGHT) ||
    (mobj->EntityFlags & (VEntity::EF_FullBright | VEntity::EF_Bright)))
  {
    light = 0xffffffff;
  }
  else
  {
    light = LightPoint(mobj->Origin, mobj);
  }
  vuint32 Fade = GetFade(SV_PointInRegion(mobj->Sector, mobj->Origin));

  //  Try to draw a model. If it's a script and it doesn't
  // specify model for this frame, draw sprite instead.
  if (!RenderAliasModel(mobj, light, Fade, Alpha, Additive, Pass))
  {
    RenderSprite(mobj, light, Fade, Alpha, Additive);
  }
  unguard;
}

//==========================================================================
//
//  VRenderLevelShared::RenderMobjs
//
//==========================================================================

void VRenderLevelShared::RenderMobjs(ERenderPass Pass)
{
  guard(VRenderLevelShared::RenderMobjs);
  if (!r_draw_mobjs)
  {
    return;
  }

  for (TThinkerIterator<VEntity> Ent(Level); Ent; ++Ent)
  {
    RenderThing(*Ent, Pass);
  }
  unguard;
}

//==========================================================================
//
//  VRenderLevelShared::DrawTranslucentPolys
//
//==========================================================================

void VRenderLevelShared::DrawTranslucentPolys()
{
  guard(VRenderLevelShared::DrawTranslucentPolys);
  int i, found;
  do
  {
    found = -1;
    float best_dist = -1;
    for (i = 0; i < MAX_TRANS_SPRITES; i++)
    {
      trans_sprite_t &spr = trans_sprites[i];
      if (!spr.Alpha)
      {
        continue;
      }
      if (spr.dist > best_dist)
      {
        found = i;
        best_dist = spr.dist;
      }
    }
    if (found != -1)
    {
      trans_sprite_t &spr = trans_sprites[found];
      if (spr.type == 2)
      {
        DrawEntityModel(spr.Ent, spr.light, spr.Fade, spr.Alpha,
          spr.Additive, spr.TimeFrac, RPASS_Normal);
      }
      else if (spr.type)
      {
        Drawer->DrawSpritePolygon(spr.Verts, GTextureManager[spr.lump],
          spr.Alpha, spr.Additive, GetTranslation(spr.translation),
          ColourMap, spr.light, spr.Fade, spr.normal, spr.pdist,
          spr.saxis, spr.taxis, spr.texorg);
      }
      else
      {
        check(spr.surf);
        Drawer->DrawMaskedPolygon(spr.surf, spr.Alpha, spr.Additive);
      }
      spr.Alpha = 0;
    }
  } while (found != -1);
  unguard;
}

//==========================================================================
//
//  VRenderLevelShared::RenderPSprite
//
//==========================================================================

void VRenderLevelShared::RenderPSprite(VViewState *VSt, float PSP_DIST,
  vuint32 light, vuint32 Fade, float Alpha, bool Additive)
{
  guard(VRenderLevelShared::RenderPSprite);
  spritedef_t *sprdef;
  spriteframe_t *sprframe;
  int         lump;
  bool        flip;

  // decide which patch to use
  if ((vuint32)VSt->State->SpriteIndex >= MAX_SPRITE_MODELS)
  {
#ifdef PARANOID
    GCon->Logf("R_ProjectSprite: invalid sprite number %d",
      VSt->State->SpriteIndex);
#endif
    return;
  }
  sprdef = &sprites[VSt->State->SpriteIndex];
  if ((VSt->State->Frame & VState::FF_FRAMEMASK) >= sprdef->numframes)
  {
#ifdef PARANOID
    GCon->Logf("R_ProjectSprite: invalid sprite frame %d : %d",
      VSt->State->SpriteIndex, VSt->State->Frame);
#endif
    return;
  }
  sprframe = &sprdef->spriteframes[VSt->State->Frame & VState::FF_FRAMEMASK];

  lump = sprframe->lump[0];
  flip = sprframe->flip[0];
  VTexture *Tex = GTextureManager[lump];

  int TexWidth = Tex->GetWidth();
  int TexHeight = Tex->GetHeight();
  int TexSOffset = Tex->SOffset;
  int TexTOffset = Tex->TOffset;

  TVec  dv[4];

  float PSP_DISTI = 1.0 / PSP_DIST;
  TVec sprorigin = vieworg + PSP_DIST * viewforward;

  float sprx = 160.0 - VSt->SX + TexSOffset;
  float spry = 100.0 - VSt->SY + TexTOffset;

  spry -= cl->PSpriteSY;

  //  1 / 160 = 0.00625
  TVec start = sprorigin - (sprx * PSP_DIST * 0.00625) * viewright;
  TVec end = start + (TexWidth * PSP_DIST * 0.00625) * viewright;

  //  1 / 160.0 * 120 / 100 = 0.0075
  TVec topdelta = (spry * PSP_DIST * 0.0075) * viewup;
  TVec botdelta = topdelta - (TexHeight * PSP_DIST * 0.0075) * viewup;
  if (aspect_ratio > 1)
  {
    topdelta *= 100.0 / 120.0;
    botdelta *= 100.0 / 120.0;
  }

  dv[0] = start + botdelta;
  dv[1] = start + topdelta;
  dv[2] = end + topdelta;
  dv[3] = end + botdelta;

  TVec saxis;
  TVec taxis;
  TVec texorg;
  if (flip)
  {
    saxis = -(viewright * 160 * PSP_DISTI);
    texorg = dv[2];
  }
  else
  {
    saxis = viewright * 160 * PSP_DISTI;
    texorg = dv[1];
  }
  if (aspect_ratio == 0)
  {
    taxis = -(viewup * 160 * PSP_DISTI);
  }
  else if (aspect_ratio == 1)
  {
    taxis = -(viewup * 100 * 4 / 3 * PSP_DISTI);
  }
  else if (aspect_ratio == 2)
  {
    taxis = -(viewup * 100 * 16 / 9 * PSP_DISTI);
  }
  else if (aspect_ratio > 2)
  {
    taxis = -(viewup * 100 * 16 / 10 * PSP_DISTI);
  }

  Drawer->DrawSpritePolygon(dv, GTextureManager[lump], Alpha, Additive,
    0, ColourMap, light, Fade, -viewforward,
    DotProduct(dv[0], -viewforward), saxis, taxis, texorg);
  unguard;
}

//==========================================================================
//
//  VRenderLevelShared::RenderViewModel
//
//==========================================================================

bool VRenderLevelShared::RenderViewModel(VViewState *VSt, vuint32 light,
  vuint32 Fade, float Alpha, bool Additive)
{
  guard(VRenderLevelShared::RenderViewModel);
  if (!r_view_models)
  {
    return false;
  }

  TVec origin = vieworg + (VSt->SX - 1.0) * viewright / 8.0 -
    (VSt->SY - 32.0) * viewup / 6.0;

  float TimeFrac = 0;
  if (VSt->State->Time > 0)
  {
    TimeFrac = 1.0 - (VSt->StateTime / VSt->State->Time);
    TimeFrac = MID(0.0, TimeFrac, 1.0);
  }

  bool Interpolate;
  // Check if we want to interpolate model frames
  if (!r_interpolate_frames)
  {
    Interpolate = false;
  }
  else
  {
    Interpolate = true;
  }
  return DrawAliasModel(origin, cl->ViewAngles, 1.0, 1.0, VSt->State,
    VSt->State->NextState ? VSt->State->NextState : VSt->State, nullptr,
    0, light, Fade, Alpha, Additive, true, TimeFrac, Interpolate,
    RPASS_Normal);
  unguard;
}

//==========================================================================
//
//  VRenderLevelShared::DrawPlayerSprites
//
//==========================================================================

void VRenderLevelShared::DrawPlayerSprites()
{
  guard(VRenderLevelShared::DrawPlayerSprites);
  if (!r_draw_psprites || r_chasecam)
  {
    return;
  }

  int RendStyle = STYLE_Normal;
  float Alpha = 1.0;
  bool Additive = false;

  cl->MO->eventGetViewEntRenderParams(Alpha, RendStyle);

  if (RendStyle == STYLE_SoulTrans)
  {
    RendStyle = STYLE_Translucent;
    Alpha = transsouls;
  }
  else if (RendStyle == STYLE_OptFuzzy)
  {
    RendStyle = r_drawfuzz ? STYLE_Fuzzy : STYLE_Translucent;
  }

  switch (RendStyle)
  {
  case STYLE_None:
    return;

  case STYLE_Normal:
    Alpha = 1.0;
    break;

  case STYLE_Fuzzy:
    Alpha = FUZZY_ALPHA;
    break;

  case STYLE_Add:
    Additive = true;
    break;
  }
  Alpha = MID(0.0, Alpha, 1.0);

  // add all active psprites
  for (int i = 0; i < NUMPSPRITES; i++)
  {
    if (!cl->ViewStates[i].State)
    {
      continue;
    }

    vuint32 light;
    if (RendStyle == STYLE_Fuzzy)
    {
      light = 0;
    }
    else if (cl->ViewStates[i].State->Frame & VState::FF_FULLBRIGHT)
    {
      light = 0xffffffff;
    }
    else
    {
      light = LightPoint(vieworg, cl->MO);
    }
    vuint32 Fade = GetFade(SV_PointInRegion(r_viewleaf->sector, cl->ViewOrg));

    if (!RenderViewModel(&cl->ViewStates[i], light, Fade, Alpha,
      Additive))
    {
      RenderPSprite(&cl->ViewStates[i], 3 - i, light, Fade, Alpha,
        Additive);
    }
  }
  unguard;
}

//==========================================================================
//
//  VRenderLevelShared::DrawCrosshair
//
//==========================================================================

void VRenderLevelShared::DrawCrosshair()
{
  guard(VRenderLevelShared::DrawCrosshair);
  if (crosshair)
  {
    if (crosshair_alpha < 0.0)  crosshair_alpha = 0.0;
    if (crosshair_alpha > 1.0)  crosshair_alpha = 1.0;

    int     cy;
    if (screenblocks < 11)
      cy = (480 - sb_height) / 2;
    else
      cy = 240;
    cy += r_crosshair_yofs;
    int handle = GTextureManager.AddPatch(VName(va("CROSHAI%i",
      (int)crosshair), VName::AddLower8), TEXTYPE_Pic);
    R_DrawPic(320, cy, handle, crosshair_alpha);
  }
  unguard;
}

//==========================================================================
//
//  R_DrawSpritePatch
//
//==========================================================================

void R_DrawSpritePatch(int x, int y, int sprite, int frame, int rot,
  int TranslStart, int TranslEnd, int Colour)
{
  guard(R_DrawSpritePatch);
  bool      flip;
  int       lump;

  spriteframe_t *sprframe = &sprites[sprite].spriteframes[frame & VState::FF_FRAMEMASK];
  flip = sprframe->flip[rot];
  lump = sprframe->lump[rot];
  VTexture *Tex = GTextureManager[lump];

  Tex->GetWidth();

  float x1 = x - Tex->SOffset;
  float y1 = y - Tex->TOffset;
  float x2 = x1 + Tex->GetWidth();
  float y2 = y1 + Tex->GetHeight();

  x1 *= fScaleX;
  y1 *= fScaleY;
  x2 *= fScaleX;
  y2 *= fScaleY;

  Drawer->DrawSpriteLump(x1, y1, x2, y2, Tex, R_GetCachedTranslation(
    R_SetMenuPlayerTrans(TranslStart, TranslEnd, Colour), nullptr), flip);
  unguard;
}
