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
//**  Copyright (C) 2018-2022 Ketmar Dark
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
#include "../server/sv_local.h"
#include "../psim/p_decal.h"


static VCvarB gm_terrain_bootprints("gm_terrain_bootprints", true, "Allow terain bootprints?", CVAR_Archive);
static VCvarB gm_decal_bootprints("gm_decal_bootprints", true, "Allow decal bootprints for Vanilla Doom?", CVAR_Archive);
static VCvarB gm_optional_bootprints("gm_optional_bootprints", true, "Allow optional terrain bootprints (mostly used for vanilla liquids)?", CVAR_Archive);


//==========================================================================
//
//  VLevel::CheckBootPrints
//
//  check what kind of bootprint decal is required at `org`
//  returns `false` if none (params are undefined)
//
//  `sub` can be `nullptr`
//
//==========================================================================
bool VLevel::CheckBootPrints (TVec org, subsector_t *sub, VBootPrintDecalParams &params) {
  params.Animator = NAME_None;
  params.Translation = 0;
  params.Shade = -1;
  params.Alpha = 1.0f;
  params.MarkTime = 0.0f;

  if (!sub) sub = PointInSubsector(org);

  // find out sector region (and its index)
  int eregidx = 0;
  const bool pobj3d = sub->isInnerPObj();
  const sec_region_t *ourreg = nullptr;
  for (const sec_region_t *reg = sub->sector->eregions; reg; reg = reg->next, ++eregidx) {
    const float fz = (eregidx || pobj3d ? reg->eceiling.GetPointZClamped(org) : reg->efloor.GetPointZClamped(org));
    if (fabsf(fz-org.z) <= 0.1f) {
      //FIXME: we may have several overlaping 3d floors, and decals for each one of them
      //       ignore this for now
      ourreg = reg;
      break;
    }
  }
  if (!ourreg) return false; // nothing to do here

  if (subsectorDecalList && gm_decal_bootprints.asBool()) {
    // check if we are inside any blood decal
    constexpr float shrinkRatio = 0.84f;
    float dcbb2d[4];
    for (decal_t *dc = subsectorDecalList[(unsigned)(ptrdiff_t)(sub-&Subsectors[0])].tail; dc; dc = dc->subprev) {
      if (dc->boottime <= 0.0f) continue;
      if (!dc->isFloor()) continue;
      if (dc->eregindex != eregidx) continue;
      if ((dc->bootshade&0xff000000) == 0xed000000) continue; // oops -- this is impossible for now
      // check coords
      ShrinkBBox2D(dcbb2d, dc->bbox2d, shrinkRatio);
      if (!IsPointInside2DBBox(org.x, org.y, dcbb2d)) continue;
      // it seems that we found it
      params.Translation = (dc->boottranslation >= 0 ? dc->boottranslation : dc->translation);
      params.Shade = (dc->bootshade != -2 ? dc->bootshade : dc->shadeclr);
      params.Alpha = clampval((dc->bootalpha >= 0.0f ? dc->bootalpha : dc->alpha), 0.0f, 1.0f);
      params.Animator = dc->bootanimator;
      params.MarkTime = dc->boottime;
      if (params.MarkTime < 0.0f) params.MarkTime = 0.0f;
      /*
      GCon->Logf(NAME_Debug, "bootprint from floor decal '%s': trans=%d; shade=0x%08x; alpha=%g; anim=%s; time=%g",
        (dc->proto ? *dc->proto->name : "<noproto>"), params.Translation, params.Shade, params.Alpha, *params.Animator, params.MarkTime);
      */
      return true;
    }
  }

  // no blood decal, try flat/terrain
  if (gm_terrain_bootprints.asBool()) {
    sec_plane_t *splane = (eregidx || pobj3d ? ourreg->eceiling.splane : ourreg->efloor.splane);
    if (splane) {
      VTerrainBootprint *bp = SV_TerrainBootprint(splane->pic);
      if (bp && (!bp->isOptional() || gm_optional_bootprints.asBool())) {
        bp->genValues();
        if (bp->ShadeColor == -2) return false; // oops
        params.Translation = bp->Translation;
        params.Shade = bp->ShadeColor;
        // special value: low byte is maxval for average color
        if ((bp->ShadeColor&0xff000000) == 0xed000000) {
          VTexture *ftx = GTextureManager(splane->pic); // get animated texture
          if (!ftx || ftx->Type == TEXTYPE_Null || ftx->Width < 1 || ftx->Height < 1) {
            params.Translation = 0;
            params.Shade = -1;
            return false;
          }
          rgb_t avc = ftx->GetAverageColor(bp->ShadeColor&0xff);
          params.Shade = (avc.r<<16)|(avc.g<<8)|avc.b;
        }
        params.Animator = bp->Animator;
        params.Alpha = (bp->AlphaValue >= 0.0f ? min2(bp->AlphaValue, 1.0f) : FRandomBetween(0.82f, 0.96f));
        params.MarkTime = FRandomBetween(bp->TimeMin, bp->TimeMax);
        if (params.MarkTime < 0.0f) params.MarkTime = 0.0f;
        return true;
      }
    }
  }

  // oops
  return false;
}


//==========================================================================
//
//  VLevel::CheckFloorDecalDamage
//
//  `sub` can be `nullptr`
//  `dgself` and `dgfunc` must be valid
//
//==========================================================================
void VLevel::CheckFloorDecalDamage (bool isPlayer, TVec org, subsector_t *sub, VObject *dgself, VMethod *dgfunc) {
  if (!subsectorDecalList) return;

  if (!sub) sub = PointInSubsector(org);

  // find out sector region (and its index)
  int eregidx = 0;
  const bool pobj3d = sub->isInnerPObj();
  const sec_region_t *ourreg = nullptr;
  for (const sec_region_t *reg = sub->sector->eregions; reg; reg = reg->next, ++eregidx) {
    const float fz = (eregidx || pobj3d ? reg->eceiling.GetPointZClamped(org) : reg->efloor.GetPointZClamped(org));
    if (fabsf(fz-org.z) <= 0.1f) {
      //FIXME: we may have several overlaping 3d floors, and decals for each one of them
      //       ignore this for now
      ourreg = reg;
      break;
    }
  }
  if (!ourreg) return; // nothing to do here

  // check if we are inside any blood decal
  constexpr float shrinkRatio = 0.84f;
  float dcbb2d[4];
  for (decal_t *dc = subsectorDecalList[(unsigned)(ptrdiff_t)(sub-&Subsectors[0])].tail; dc; dc = dc->subprev) {
    VDecalDef *dec = dc->proto;
    if (!dec || !dec->hasFloorDamage) continue;
    if (!dc->isFloor()) continue;
    if (dc->eregindex != eregidx) continue;
    if (isPlayer) {
      dec->floorDamagePlayer.genValue(0.0f);
      //GCon->Logf(NAME_Debug, "FLOOR DAMAGE from '%s': isPlayer=%d; dmg=%g", *dec->name, (int)isPlayer, dec->floorDamagePlayer.value);
      if (dec->floorDamagePlayer.value < 0.5f) continue;
    } else {
      dec->floorDamageMonsters.genValue(0.0f);
      //GCon->Logf(NAME_Debug, "FLOOR DAMAGE from '%s': isPlayer=%d; dmg=%g", *dec->name, (int)isPlayer, dec->floorDamageMonsters.value);
      if (dec->floorDamageMonsters.value < 0.5f) continue;
    }
    // check tick
    dec->floorDamageTick.genValue(0.0f);
    const int ticks = (int)dec->floorDamageTick.value;
    if (ticks < 1) continue;
    if (TicTime%ticks) continue;
    // gen damage
    dec->floorDamage.genValue(0.0f);
    const int dmg = (int)dec->floorDamage.value;
    if (dmg < 1) continue;
    // check coords
    ShrinkBBox2D(dcbb2d, dc->bbox2d, shrinkRatio);
    if (!IsPointInside2DBBox(org.x, org.y, dcbb2d)) continue;
    dec->floorDamageSuitLeak.genValue(5.0f);
    //GCon->Logf(NAME_Debug, "FLOOR DAMAGE from '%s': dmg=%d", *dec->name, dmg);
    // it seems that we found it
    // void delegate (int damage, name damageType, int suitLeak) dg
    if (!dgfunc->IsStatic()) P_PASS_REF(dgself);
    P_PASS_INT(dmg);
    P_PASS_NAME(dec->floorDamageType);
    P_PASS_INT((int)dec->floorDamageSuitLeak.value);
    VObject::ExecuteFunction(dgfunc);
    return; // only the top one can affect us
  }
}
