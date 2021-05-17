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

extern VCvarI gl_flatdecal_limit;


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
  float dcz;
  float height;
  const sec_region_t *sr;
};


//==========================================================================
//
//  DupDecal
//
//  this will copy animator, and will add the decal to animated list
//
//==========================================================================
static decal_t *DupDecal (VLevel *Level, const decal_t *dc) noexcept {
  vassert(dc);
  decal_t *dcnew = new decal_t;
  memcpy((void *)dcnew, (const void *)dc, sizeof(decal_t));
  // clear pointers
  dcnew->prev = dcnew->next = nullptr;
  dcnew->animator = nullptr;
  dcnew->prevanimated = dcnew->nextanimated = nullptr;
  dcnew->secprev = dcnew->secnext = nullptr;
  // copy animator
  if (dc->animator) {
    dcnew->animator = dc->animator->clone();
    Level->AddAnimatedDecal(dcnew);
  }
  return dcnew;
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
  if (!Level->IsBBox2DTouchingSubsector(sub, nfo->bb2d)) {
    //GCon->Logf(NAME_Debug, "  skip subsector #%d (sector #%d)", (int)(ptrdiff_t)(sub-&Level->Subsectors[0]), (int)(ptrdiff_t)(sub->sector-&Level->Sectors[0]));
    return true; // continue checking
  }
  //GCon->Logf(NAME_Debug, "  ADD subsector #%d (sector #%d)", (int)(ptrdiff_t)(sub-&Level->Subsectors[0]), (int)(ptrdiff_t)(sub->sector-&Level->Sectors[0]));
  const decal_t *origdc = nfo->origdc;
  // find region for decal
  subregion_t *dreg = nullptr;
  // check if we're inside an original sector
  if (sub->sector == origdc->slidesec) {
    // yes, get region by its index
    for (subregion_t *reg = sub->regions; reg; reg = reg->next) {
      if (reg->secregion == nfo->sr) {
        dreg = reg;
        break;
      }
    }
  } else {
    // some other sector, check bounds, and find region by z coord
    const float dcz = nfo->dcz;
    const TVec p(origdc->worldx, origdc->worldy);
    const bool isFloorDc = (origdc->eregindex ? origdc->isFloor() : origdc->isCeiling()); // as if it is not in base subregion
    // check base floor
    if (dcz <= sub->sector->floor.minz-nfo->height) return true; // continue checking
    // check base ceiling
    if (dcz >= sub->sector->ceiling.maxz+nfo->height) return true; // continue checking
    // first subregion is always base sector subregion
    subregion_t *reg = sub->regions;
    float bestdist = fabsf(dcz-(!isFloorDc ? reg->floorplane.GetPointZClamped(p) : reg->ceilplane.GetPointZClamped(p))); // for base subregion, f/c flag is inverted
    if (bestdist <= nfo->height) dreg = reg;
    for (reg = reg->next; reg; reg = reg->next) {
      const sec_region_t *sr = reg->secregion;
      if (sr->regflags&(sec_region_t::RF_NonSolid|sec_region_t::RF_OnlyVisual)) continue;
      float rdist;
      if (isFloorDc) {
        if (sr->regflags&sec_region_t::RF_SkipFloorSurf) continue;
        const float rz = reg->floorplane.GetPointZClamped(p);
        rdist = dcz+0.2f-rz;
      } else {
        if (sr->regflags&sec_region_t::RF_SkipCeilSurf) continue;
        const float rz = reg->ceilplane.GetPointZClamped(p);
        rdist = rz-(dcz-0.2f);
      }
      if (rdist >= 0.0f && rdist < bestdist && fabsf(rdist-dcz) <= nfo->height) {
        dreg = reg;
        if (rdist == 0.0f) break;
      }
    }
  }
  if (!dreg) return true; // continue checking
  // duplicate base decal, and add it to its region
  decal_t *dcnew = DupDecal(Level, origdc); // this will add decal to animations list
  bool atFloor;
  if (dreg->secregion->regflags&sec_region_t::RF_BaseRegion) {
    // base region
    atFloor = (origdc->eregindex ? origdc->isCeiling() : origdc->isFloor());
  } else {
    atFloor = (origdc->eregindex ? origdc->isFloor() : origdc->isCeiling());
  }

  // check texture
  const int texid = (atFloor ? dreg->floorplane.splane : dreg->ceilplane.splane)->pic.id;
  if (texid <= 0 || texid == skyflatnum) return true; // continue checking
  VTexture *tex = GTextureManager[texid];
  if (!tex || tex->Type == TEXTYPE_Null || tex->animated) return true; // continue checking

  // fix decal surface type
  dcnew->dcsurf = (atFloor ? decal_t::Floor : decal_t::Ceiling);
  // and append it
  dreg->appendDecal(dcnew);

  // decal limiter
  const int dclimit = gl_flatdecal_limit.asInt();
  if (dclimit > 0) {
    int dcCount = 0;
    decal_t *cdc = (atFloor ? dreg->floordecaltail : dreg->ceildecaltail);
    while (cdc) {
      decal_t *c = cdc;
      cdc = cdc->prev;
      if (Are2DBBoxesOverlap(dcnew->bbox2d, c->bbox2d)) {
        if (++dcCount > dclimit) {
          Level->RemoveDecalAnimator(c);
          dreg->removeDecal(c);
          delete c;
        }
      }
    }
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
  vassert(origdc->isFloor() || origdc->isCeiling());
  vassert(origdc->eregindex >= 0);

  DInfo nfo;
  nfo.origdc = origdc;
  nfo.dcz = 0.0f;
  nfo.sr = nullptr;

  // calculate decal Z position
  int ridx = origdc->eregindex;
  for (const sec_region_t *sr = origdc->slidesec->eregions; sr; sr = sr->next, --ridx) {
    if (ridx == 0) {
      nfo.sr = sr;
      const TVec p(origdc->worldx, origdc->worldy);
      nfo.dcz = (origdc->isFloor() ? sr->efloor.GetPointZClamped(p) : sr->eceiling.GetPointZClamped(p));
      break;
    }
  }
  vassert(nfo.sr);

  //TODO: rotated decals

  const int dcTexId = origdc->texture; // "0" means "no texture found"
  if (dcTexId <= 0) return;

  /*
  VTexture *dtex = GTextureManager[dcTexId];

  // decal scale is not inverted
  const float dscaleX = origdc->scaleX;
  const float dscaleY = origdc->scaleY;

  // use origScale to get the original starting point
  const float txofs = dtex->GetScaledSOffsetF()*dscaleX;
  const float tyofs = dtex->GetScaledTOffsetF()*origdc->origScaleY;

  const float twdt = dtex->GetScaledWidthF()*dscaleX;
  const float thgt = dtex->GetScaledHeightF()*dscaleY;

  //nfo.height = max2(2.0f, min2(twdt, thgt)*0.4f);
  //nfo.dcz = origdc->orgz;
  nfo.height = max2(2.0f, origdc->height);

  const TVec v1(origdc->worldx, origdc->worldy);
  // left-bottom
  const TVec qv0 = v1+TVec(-txofs, tyofs);
  // right-bottom
  const TVec qv1 = qv0+TVec(twdt, 0.0f);
  // left-top
  const TVec qv2 = qv0-TVec(0.0f, thgt);
  // right-top
  const TVec qv3 = qv1-TVec(0.0f, thgt);

  nfo.bb2d[BOX2D_MINX] = min2(min2(min2(qv0.x, qv1.x), qv2.x), qv3.x);
  nfo.bb2d[BOX2D_MAXX] = max2(max2(max2(qv0.x, qv1.x), qv2.x), qv3.x);
  nfo.bb2d[BOX2D_MINY] = min2(min2(min2(qv0.y, qv1.y), qv2.y), qv3.y);
  nfo.bb2d[BOX2D_MAXY] = max2(max2(max2(qv0.y, qv1.y), qv2.y), qv3.y);
  */

  memcpy((void *)&nfo.bb2d[0], (void *)&origdc->bbox2d[0], sizeof(nfo.bb2d));
  nfo.height = origdc->height;

  //GCon->Logf(NAME_Debug, "decal tex %d: sector #%d; box: (%g,%g)-(%g,%g)", origdc->texture.id, (int)(ptrdiff_t)(origdc->slidesec-&Level->Sectors[0]), nfo.bb2d[BOX2D_MINX], nfo.bb2d[BOX2D_MINY], nfo.bb2d[BOX2D_MAXX], nfo.bb2d[BOX2D_MAXY]);

  Level->CheckBSPB2DBox(nfo.bb2d, &AddDecalToSubsector, &nfo);
}
