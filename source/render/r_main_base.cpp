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
#include "r_local.h"


//==========================================================================
//
//  VRenderLevelPublic::VRenderLevelPublic
//
//==========================================================================
VRenderLevelPublic::VRenderLevelPublic (VLevel *ALevel) noexcept
  : staticLightsFiltered(false)
  //, clip_base()
  //, refdef()
  , Level(ALevel)
{
  Drawer->MirrorFlip = false;
  Drawer->MirrorClip = false;
}


// ////////////////////////////////////////////////////////////////////////// //
struct DInfo {
  float bb2d[4];
  const decal_t *origdc;
};


//==========================================================================
//
//  AddDecalToSubsector
//
//  `*udata` is `DInfo`
//
//==========================================================================
static bool AddDecalToSubsector (VLevel *level, subsector_t *sub, void *udata) {
  const DInfo *nfo = (const DInfo *)udata;
  if (level->IsBBox2DTouchingSubsector(sub, nfo->bb2d)) {
  }
  return true; // continue checking
}


//==========================================================================
//
//  VRenderLevelPublic::SpreadDecal
//
//  will create new decals for all touching subsectors
//  `origdc` must be valid, with proper flags and region number
//  `origdc` won't be modified, owned, or destroyed
//
//==========================================================================
void VRenderLevelPublic::SpreadDecal (const decal_t *origdc) {
  if (!origdc) return;
  vassert(origdc->slidesec);
  vassert((origdc->dcsurf&decal_t::SurfTypeMask) == decal_t::Floor || (origdc->dcsurf&decal_t::SurfTypeMask) == decal_t::Ceiling);
  vassert(origdc->eregindex >= 0);
  int ridx = origdc->eregindex;
  for (const sec_region_t *sr = origdc->slidesec->eregions; sr; sr = sr->next) {
    --ridx;
    vassert(ridx >= 0);
  }

  //TODO: rotated decals

  const int dcTexId = origdc->texture; // "0" means "no texture found"
  if (dcTexId <= 0) return;
  VTexture *dtex = GTextureManager[dcTexId];

  // decal scale is not inverted
  const float dscaleX = origdc->scaleX;
  const float dscaleY = origdc->scaleY;

  // use origScale to get the original starting point
  const float txofs = dtex->GetScaledSOffsetF()*dscaleX;
  const float tyofs = dtex->GetScaledTOffsetF()*origdc->origScaleY;

  const float twdt = dtex->GetScaledWidthF()*dscaleX;
  const float thgt = dtex->GetScaledHeightF()*dscaleY;

  const TVec v1(origdc->worldx, origdc->worldy);
  // left-bottom
  const TVec qv0 = v1-TVec(txofs, tyofs);
  // right-bottom
  const TVec qv1 = qv0+TVec(twdt, 0.0f);
  // left-top
  const TVec qv2 = qv0-TVec(0.0f, thgt);
  // right-top
  const TVec qv3 = qv1-TVec(0.0f, thgt);

  DInfo nfo;
  nfo.origdc = origdc;
  nfo.bb2d[BOX2D_MINX] = min2(min2(min2(qv0.x, qv1.x), qv2.x), qv3.x);
  nfo.bb2d[BOX2D_MAXX] = max2(max2(max2(qv0.x, qv1.x), qv2.x), qv3.x);
  nfo.bb2d[BOX2D_MINY] = min2(min2(min2(qv0.y, qv1.y), qv2.y), qv3.y);
  nfo.bb2d[BOX2D_MAXY] = max2(max2(max2(qv0.y, qv1.y), qv2.y), qv3.y);

  Level->CheckBSPB2DBox(nfo.bb2d, &AddDecalToSubsector, &nfo);
}
