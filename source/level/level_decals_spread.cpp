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

/*
  i think i should use portal traversing code here instead of BSP bbox checks.
  this way i can block spreading with closed doors, for example, and other
  "closed" things.
 */


// ////////////////////////////////////////////////////////////////////////// //
struct DInfo {
  float bbox2d[4];
  TVec org;
  float range;
  float spheight; // spread height
  VDecalDef *dec;
  int translation;
};


//==========================================================================
//
//  CalculateDecalBBox
//
//  returns vertical spread height
//  zero means "no texture found"
//
//==========================================================================
static float CalculateDecalBBox (float bbox2d[4], VDecalDef *dec, const float worldx, const float worldy) noexcept {
  const int dcTexId = dec->texid; // "0" means "no texture found"
  if (dcTexId <= 0) {
    memset((void *)&bbox2d[0], 0, sizeof(float)*4);
    return 0.0f;
  }

  VTexture *dtex = GTextureManager[dcTexId];

  // decal scale is not inverted
  const float dscaleX = dec->scaleX.value;
  const float dscaleY = dec->scaleY.value;

  // use origScale to get the original starting point
  const float txofs = dtex->GetScaledSOffsetF()*dscaleX;
  const float tyofs = dtex->GetScaledTOffsetF()*dscaleY;

  const float twdt = dtex->GetScaledWidthF()*dscaleX;
  const float thgt = dtex->GetScaledHeightF()*dscaleY;

  if (twdt < 1.0f || thgt < 1.0f) {
    memset((void *)&bbox2d[0], 0, sizeof(float)*4);
    return 0.0f;
  }

  const float height = max2(2.0f, min2(twdt, thgt)*0.4f);

  const TVec v1(worldx, worldy);
  // left-bottom
  const TVec qv0 = v1+TVec(-txofs, tyofs);
  // right-bottom
  const TVec qv1 = qv0+TVec(twdt, 0.0f);
  // left-top
  const TVec qv2 = qv0-TVec(0.0f, thgt);
  // right-top
  const TVec qv3 = qv1-TVec(0.0f, thgt);

  bbox2d[BOX2D_MINX] = min2(min2(min2(qv0.x, qv1.x), qv2.x), qv3.x);
  bbox2d[BOX2D_MAXX] = max2(max2(max2(qv0.x, qv1.x), qv2.x), qv3.x);
  bbox2d[BOX2D_MINY] = min2(min2(min2(qv0.y, qv1.y), qv2.y), qv3.y);
  bbox2d[BOX2D_MAXY] = max2(max2(max2(qv0.y, qv1.y), qv2.y), qv3.y);

  return height;
}


//==========================================================================
//
//  IsGoodFlatTexture
//
//==========================================================================
static bool IsGoodFlatTexture (int texid) noexcept {
  if (texid <= 0 || texid == skyflatnum) return false;
  VTexture *tex = GTextureManager[texid];
  if (!tex || tex->Type == TEXTYPE_Null) return false;
  if (tex->animated) return false;
  if (tex->GetWidth() < 1 || tex->GetHeight() < 1) return false;
  return true;
}


//==========================================================================
//
//  AddDecalToSubsector
//
//  `*udata` is `DInfo`
//
//==========================================================================
static bool AddDecalToSubsector (VLevel *Level, subsector_t *sub, void *udata) {
  const DInfo *nfo = (const DInfo *)udata;
  if (!Level->IsBBox2DTouchingSubsector(sub, nfo->bbox2d)) {
    return true; // continue checking
  }

  // check sector regions, and add decals
  int eregidx = 0;
  for (sec_region_t *reg = sub->sector->eregions; reg; reg = reg->next, ++eregidx) {
    // base region?
    if (reg->regflags&sec_region_t::RF_BaseRegion) {
      // check floor
      if (IsGoodFlatTexture(reg->efloor.splane->pic)) {
        const float fz = reg->efloor.GetPointZClamped(nfo->org);
        if (nfo->org.z+2.0f >= fz && nfo->org.z-fz < nfo->range+nfo->spheight) {
          // do it
          Level->NewFlatDecal(true/*asfloor*/, sub, eregidx, nfo->org.x, nfo->org.y, nfo->dec, nfo->translation);
        }
      }
      // check ceiling
      if (IsGoodFlatTexture(reg->eceiling.splane->pic)) {
        const float cz = reg->eceiling.GetPointZClamped(nfo->org);
        if (nfo->org.z-2.0f <= cz && cz-nfo->org.z < nfo->range+nfo->spheight) {
          // do it
          Level->NewFlatDecal(false/*asceiling*/, sub, eregidx, nfo->org.x, nfo->org.y, nfo->dec, nfo->translation);
        }
      }
    } else {
      // 3d floor region; here, floor and ceiling directions are inverted
      if (reg->regflags&(sec_region_t::RF_NonSolid|sec_region_t::RF_OnlyVisual)) continue;
      // check floor
      if (!(reg->regflags&sec_region_t::RF_SkipFloorSurf) && IsGoodFlatTexture(reg->efloor.splane->pic)) {
        const float fz = reg->efloor.GetPointZClamped(nfo->org);
        if (nfo->org.z-2.0f <= fz && fz-nfo->org.z < nfo->range+nfo->spheight) {
          // do it
          Level->NewFlatDecal(true/*asfloor*/, sub, eregidx, nfo->org.x, nfo->org.y, nfo->dec, nfo->translation);
        }
      }
      // check ceiling
      if (!(reg->regflags&sec_region_t::RF_SkipFloorSurf) && IsGoodFlatTexture(reg->eceiling.splane->pic)) {
        const float cz = reg->eceiling.GetPointZClamped(nfo->org);
        if (nfo->org.z+2.0f >= cz && nfo->org.z-cz < nfo->range+nfo->spheight) {
          // do it
          Level->NewFlatDecal(false/*asceiling*/, sub, eregidx, nfo->org.x, nfo->org.y, nfo->dec, nfo->translation);
        }
      }
    }
  }

  return true; // continue checking
}


//==========================================================================
//
//  VLevel::SpreadFlatDecalEx
//
//==========================================================================
void VLevel::SpreadFlatDecalEx (const TVec org, float range, VDecalDef *dec, int level, int translation) {
  if (level > 16) {
    GCon->Logf(NAME_Warning, "too many lower decals '%s'", *dec->name);
    return;
  }

  if (dec->lowername != NAME_None) {
    if (dec->lowername != NAME_None && VStr::strEquCI(*dec->lowername, "none")) {
      VDecalDef *dcl = VDecalDef::getDecal(dec->lowername);
      if (dcl) SpreadFlatDecalEx(org, range, dcl, level+1, translation);
    }
  }

  int tex = dec->texid;
  VTexture *DTex = GTextureManager[tex];
  if (!DTex || DTex->Type == TEXTYPE_Null || DTex->GetWidth() < 1 || DTex->GetHeight() < 1) return;

  DInfo nfo;
  nfo.spheight = CalculateDecalBBox(nfo.bbox2d, dec, org.x, org.y);
  if (nfo.spheight == 0.0f) return; // just in case
  nfo.org = org;
  nfo.range = range;
  nfo.dec = dec;
  nfo.translation = translation;

  CheckBSPB2DBox(nfo.bbox2d, &AddDecalToSubsector, &nfo);
}
