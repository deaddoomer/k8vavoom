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
#include "../server/sv_local.h"


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
      //FIXME: we may several overlaping 3d floors, and decals for each one of them
      //       ignore this for now
      ourreg = reg;
      break;
    }
  }
  if (!ourreg) return false; // nothing to do here

  if (subsectorDecalList) {
    // check if we are inside any blood decal
    constexpr float shrinkRatio = 0.84f;
    float dcbb2d[4];
    for (decal_t *dc = subsectorDecalList[(unsigned)(ptrdiff_t)(sub-&Subsectors[0])].tail; dc; dc = dc->subprev) {
      if (dc->boottime <= 0.0f) continue;
      if (!dc->isFloor()) continue;
      if (dc->eregindex != eregidx) continue;
      // check coords
      ShrinkBBox2D(dcbb2d, dc->bbox2d, shrinkRatio);
      if (!IsPointInside2DBBox(org.x, org.y, dcbb2d)) continue;
      #if 0
      /*if (dc->eregindex != 0)*/ {
        const bool pobj3d = sub->isInnerPObj();
        // 3d floor decal, check Z coord
        bool zok = false;
        int eregidx = 0;
        for (const sec_region_t *reg = sub->sector->eregions; reg; reg = reg->next, ++eregidx) {
          if (eregidx == dc->eregindex) {
            const float fz = (eregidx || pobj3d ? reg->eceiling.GetPointZClamped(org) : reg->efloor.GetPointZClamped(org));
            if (fabsf(fz-org.z) <= 0.1f) {
              zok = true;
              //org.z = fz;
            }
            break;
          }
        }
        if (!zok) continue;
      }
      #endif
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
  #if 0
  {
    const bool pobj3d = sub->isInnerPObj();
    int eregidx = 0;
    for (const sec_region_t *reg = sub->sector->eregions; reg; reg = reg->next, ++eregidx) {
      const float fz = (eregidx || pobj3d ? reg->eceiling.GetPointZClamped(org) : reg->efloor.GetPointZClamped(org));
      if (fabsf(fz-org.z) <= 0.1f) {
        // region found, detect terrain
        sec_plane_t *splane = (eregidx || pobj3d ? reg->eceiling.splane : reg->efloor.splane);
        if (splane) {
          VTerrainBootprint *bp = SV_TerrainBootprint(splane->pic);
          if (bp) {
            bp->genValues();
            params.Translation = bp->Translation;
            params.Shade = bp->ShadeColor;
            params.Animator = bp->Animator;
            params.Alpha = (bp->AlphaValue >= 0.0f ? min2(bp->AlphaValue, 1.0f) : RandomBetween(0.82f, 0.96f));
            params.MarkTime = RandomBetween(bp->TimeMin, bp->TimeMax);
            if (params.MarkTime < 0.0f) params.MarkTime = 0.0f;
            return true;
          }
        }
      }
    }
  }
  #else
  {
    sec_plane_t *splane = (eregidx || pobj3d ? ourreg->eceiling.splane : ourreg->efloor.splane);
    if (splane) {
      VTerrainBootprint *bp = SV_TerrainBootprint(splane->pic);
      if (bp) {
        bp->genValues();
        params.Translation = bp->Translation;
        params.Shade = bp->ShadeColor;
        params.Animator = bp->Animator;
        params.Alpha = (bp->AlphaValue >= 0.0f ? min2(bp->AlphaValue, 1.0f) : RandomBetween(0.82f, 0.96f));
        params.MarkTime = RandomBetween(bp->TimeMin, bp->TimeMax);
        if (params.MarkTime < 0.0f) params.MarkTime = 0.0f;
        return true;
      }
    }
  }
  #endif

  // oops
  return false;
}
