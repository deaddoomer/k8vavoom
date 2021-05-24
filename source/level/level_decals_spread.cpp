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

#define VV_FLAT_DECAL_USE_FLOODFILL

/*
  i think i should use portal traversing code here instead of BSP bbox checks.
  this way i can block spreading with closed doors, for example, and other
  "closed" things.
 */


//#ifdef VV_FLAT_DECAL_USE_FLOODFILL
// "subsector touched" marks for portal spread
TArray<int> VLevel::dcSubTouchMark;
TMapNC<polyobj_t *, bool> VLevel::dcPobjTouched;
int VLevel::dcSubTouchCounter = 0;
bool VLevel::dcPobjTouchedReset = false;
//#endif


// ////////////////////////////////////////////////////////////////////////// //
struct DInfo {
  //float bbox2d[4];
  TVec org;
  float range;
  float spheight; // spread height
  VDecalDef *dec;
  int translation;
  VLevel *Level;
  unsigned orflags;
  float angle;
  float alpha; // >0: override
};


//==========================================================================
//
//  decal_t::calculateBBox
//
//==========================================================================
void decal_t::calculateBBox () noexcept {
  return (void)CalculateTextureBBox(bbox2d, texture.id, worldx, worldy, angle, scaleX, scaleY);
}


//==========================================================================
//
//  VLevel::IncSubTouchCounter
//
//==========================================================================
void VLevel::IncSubTouchCounter () noexcept {
  dcPobjTouchedReset = false;
  if (dcSubTouchMark.length() == NumSubsectors) {
    if (++dcSubTouchCounter != MAX_VINT32) return;
  } else {
    dcSubTouchMark.setLength(NumSubsectors);
  }
  for (auto &&v : dcSubTouchMark) v = 0;
  dcSubTouchCounter = 1;
}


//==========================================================================
//
//  IsGoodFlatTexture
//
//==========================================================================
static bool IsGoodFlatTexture (const int texid) noexcept {
  if (texid <= 0 || texid == skyflatnum) return false;
  VTexture *tex = GTextureManager[texid];
  if (!tex || tex->Type == TEXTYPE_Null) return false;
  if (tex->animated) return false;
  if (tex->GetWidth() < 1 || tex->GetHeight() < 1) return false;
  return true;
}


//==========================================================================
//
//  IsTransparentFlatTexture
//
//==========================================================================
static bool IsTransparentFlatTexture (const int texid) noexcept {
  if (texid <= 0) return false;
  VTexture *tex = GTextureManager[texid];
  return (tex && tex->isTransparent());
}


// ////////////////////////////////////////////////////////////////////////// //
enum {
  PutAtFloor   = 1u<<0,
  PutAtCeiling = 1u<<1,
};


//==========================================================================
//
//  PutDecalToSubsectorRegion
//
//==========================================================================
static unsigned PutDecalToSubsectorRegion (const DInfo *nfo, subsector_t *sub, sec_region_t *reg, const int eregidx) {
  const float orgz = nfo->org.z;
  const float xhgt = nfo->range+nfo->spheight;
  unsigned res = 0u;
  // base region?
  if ((reg->regflags&sec_region_t::RF_BaseRegion)) {
    // check floor
    if (IsGoodFlatTexture(reg->efloor.splane->pic)) {
      const float fz = reg->efloor.GetPointZClamped(nfo->org);
      if (sub->isInnerPObj()) {
        // 3d polyobject
        if (orgz-2.0f <= fz && fz-orgz < xhgt) {
          // do it (for some reason it should be inverted here)
          nfo->Level->NewFlatDecal(false/*asceiling*/, sub, eregidx, nfo->org.x, nfo->org.y, nfo->dec, nfo->translation, nfo->orflags, nfo->angle, nfo->alpha);
          res |= PutAtCeiling;
        }
      } else if (fz > orgz-xhgt && fz < orgz+xhgt) {
        // do it
        nfo->Level->NewFlatDecal(true/*asfloor*/, sub, eregidx, nfo->org.x, nfo->org.y, nfo->dec, nfo->translation, nfo->orflags, nfo->angle, nfo->alpha);
        res |= PutAtFloor;
      }
    }
    // check ceiling
    if (IsGoodFlatTexture(reg->eceiling.splane->pic)) {
      const float cz = reg->eceiling.GetPointZClamped(nfo->org);
      if (sub->isInnerPObj()) {
        // 3d polyobject
        if (orgz+2.0f >= cz && orgz-cz < xhgt) {
          // do it (for some reason it should be inverted here)
          nfo->Level->NewFlatDecal(true/*asfloor*/, sub, eregidx, nfo->org.x, nfo->org.y, nfo->dec, nfo->translation, nfo->orflags, nfo->angle, nfo->alpha);
          res |= PutAtFloor;
        }
      } else if (cz > orgz-xhgt && cz < orgz+xhgt) {
        // do it
        nfo->Level->NewFlatDecal(false/*asceiling*/, sub, eregidx, nfo->org.x, nfo->org.y, nfo->dec, nfo->translation, nfo->orflags, nfo->angle, nfo->alpha);
        res |= PutAtCeiling;
      }
    }
  } else {
    // 3d floor region; here, floor and ceiling directions are inverted
    // we may come here with base region for 3d polyobject
    if (reg->regflags&(sec_region_t::RF_NonSolid|sec_region_t::RF_OnlyVisual)) return 0u;
    // check floor
    if (!(reg->regflags&sec_region_t::RF_SkipFloorSurf) && IsGoodFlatTexture(reg->efloor.splane->pic)) {
      const float fz = reg->efloor.GetPointZClamped(nfo->org);
      if ((orgz-2.0f <= fz || IsTransparentFlatTexture(reg->efloor.splane->pic)) && fz-orgz < xhgt) {
        // do it
        nfo->Level->NewFlatDecal(true/*asfloor*/, sub, eregidx, nfo->org.x, nfo->org.y, nfo->dec, nfo->translation, nfo->orflags, nfo->angle, nfo->alpha);
        res |= PutAtFloor;
      }
    }
    // check ceiling
    if (!(reg->regflags&sec_region_t::RF_SkipFloorSurf) && IsGoodFlatTexture(reg->eceiling.splane->pic)) {
      const float cz = reg->eceiling.GetPointZClamped(nfo->org);
      if ((orgz+2.0f >= cz || IsTransparentFlatTexture(reg->eceiling.splane->pic)) && orgz-cz < xhgt) {
        // do it
        nfo->Level->NewFlatDecal(false/*asceiling*/, sub, eregidx, nfo->org.x, nfo->org.y, nfo->dec, nfo->translation, nfo->orflags, nfo->angle, nfo->alpha);
        res |= PutAtCeiling;
      }
    }
  }
  return res;
}


//==========================================================================
//
//  PutDecalToSubsector
//
//  does no "box touches subsector" checks
//
//==========================================================================
static void PutDecalToSubsector (const DInfo *nfo, subsector_t *sub) {
  // check sector regions, and add decals
  int eregidx = 0;
  for (sec_region_t *reg = sub->sector->eregions; reg; reg = reg->next, ++eregidx) {
    (void)PutDecalToSubsectorRegion(nfo, sub, reg, eregidx);
  }
}


#ifndef VV_FLAT_DECAL_USE_FLOODFILL
//==========================================================================
//
//  AddDecalToSubsector
//
//  `*udata` is `DInfo`
//
//==========================================================================
static bool AddDecalToSubsector (VLevel *Level, subsector_t *sub, void *udata) {
  const DInfo *nfo = (const DInfo *)udata;
  if (Level->IsBBox2DTouchingSubsector(sub, nfo->dec->bbox2d)) {
    PutDecalToSubsector(nfo, sub);
  }
  return true; // continue checking
}

#else

//==========================================================================
//
//  DecalFloodFill
//
//==========================================================================
static void DecalFloodFill (const DInfo *nfo, subsector_t *sub) {
  if (nfo->Level->IsSubTouched(sub)) return;
  nfo->Level->MarkSubTouched(sub);
  if (sub->numlines < 3) return;
  if (!nfo->Level->IsBBox2DTouchingSubsector(sub, nfo->dec->bbox2d)) return;
  //GCon->Logf(NAME_Debug, "flat decal '%s' to sub #%d (sector #%d)", *nfo->dec->name, (int)(ptrdiff_t)(sub-&nfo->Level->Subsectors[0]), (int)(ptrdiff_t)(sub->sector-&nfo->Level->Sectors[0]));
  // put decals
  PutDecalToSubsector(nfo, sub);

  // process 3d polyobjects
  if (sub->HasPObjs()) {
    for (auto &&it : sub->PObjFirst()) {
      polyobj_t *pobj = it.pobj();
      if (!pobj->Is3D()) continue;
      if (!nfo->Level->dcPobjTouchedReset) {
        nfo->Level->dcPobjTouchedReset = true;
        nfo->Level->dcPobjTouched.reset();
      }
      if (nfo->Level->dcPobjTouched.put(pobj, true)) continue; // already processed
      for (subsector_t *posub = pobj->GetSector()->subsectors; posub; posub = posub->seclink) {
        if (!nfo->Level->IsSubTouched(posub)) {
          nfo->Level->MarkSubTouched(posub);
          if (posub->numlines >= 3) {
            if (nfo->Level->IsBBox2DTouchingSubsector(posub, nfo->dec->bbox2d)) {
              // put decals
              PutDecalToSubsector(nfo, posub);
            }
          }
        }
      }
    }
  }

  // if subsector is fully inside our rect, we should spread over all segs
  bool spreadAllSegs = false;
  if (sub->bbox2d[BOX2D_MINX] >= nfo->dec->bbox2d[BOX2D_MINX] &&
      sub->bbox2d[BOX2D_MINY] >= nfo->dec->bbox2d[BOX2D_MINY] &&
      sub->bbox2d[BOX2D_MAXX] <= nfo->dec->bbox2d[BOX2D_MAXX] &&
      sub->bbox2d[BOX2D_MAXY] <= nfo->dec->bbox2d[BOX2D_MAXY])
  {
    spreadAllSegs = true;
  }
  // check subsector segs
  const seg_t *seg = &nfo->Level->Segs[sub->firstline];
  for (int f = sub->numlines; f--; ++seg) {
    if (!seg->frontsector || !seg->backsector || !seg->partner) continue;
    if (nfo->Level->IsSubTouched(seg->partner->frontsub)) continue; // already processed
    // check if we should spread over this seg
    if (!spreadAllSegs) {
      // our box should intersect with the seg
      // check reject point
      if (seg->PointDistance(seg->get2DBBoxRejectPoint(nfo->dec->bbox2d)) <= 0.0f) continue; // entire box on a back side
      // check accept point
      // if accept point on another side (or on plane), assume intersection
      if (seg->PointDistance(seg->get2DBBoxAcceptPoint(nfo->dec->bbox2d)) >= 0.0f) continue;
    }
    // ok, we must spread over this seg; check if it is a closed something
    const line_t *line = seg->linedef;
    if (line && !line->pobj() && seg->frontsector != seg->backsector) {
      if (VViewClipper::IsSegAClosedSomething(nfo->Level, nullptr, seg)) continue; // oops, it is closed
      const float orgz = nfo->org.z;
      const float xhgt = nfo->range+nfo->spheight;
      // check if it is blocked by high floor
      if (seg->backsector->floor.minz > seg->frontsector->floor.maxz) {
        // back sector floor is higher
        if (orgz+xhgt <= seg->backsector->floor.minz) continue;
      }
      // check if it is blocked by low ceiling
      if (seg->frontsector->ceiling.maxz < seg->backsector->ceiling.minz) {
        // front sector ceiling is lower
        if (orgz-xhgt >= seg->frontsector->ceiling.maxz) continue;
      }
    }
    // recurse
    DecalFloodFill(nfo, seg->partner->frontsub);
  }
}
#endif


//==========================================================================
//
//  VLevel::SpreadFlatDecalEx
//
//==========================================================================
void VLevel::SpreadFlatDecalEx (const TVec org, float range, VDecalDef *dec, int level, int translation,
                                float angle, bool angleOverride, bool forceFlipX, float alpha, float alphaOverride)
{
  if (!dec) return; // just in case

  if (dec->noFlat) return;

  if (alphaOverride && alpha <= 0.004f) return;
  alpha = min2(1.0f, alpha);

  if (level > 16) {
    GCon->Logf(NAME_Warning, "too many lower decals '%s'", *dec->name);
    return;
  }

  if (dec->lowername != NAME_None && !VStr::strEquCI(*dec->lowername, "none")) {
    VDecalDef *dcl = VDecalDef::getDecal(dec->lowername);
    if (dcl) SpreadFlatDecalEx(org, range, dcl, level+1, translation, angle, angleOverride, forceFlipX, alpha, alphaOverride);
  }

  dec->genValues();

  int tex = dec->texid;
  VTexture *dtex = GTextureManager[tex];
  if (!dtex || dtex->Type == TEXTYPE_Null || dtex->GetWidth() < 1 || dtex->GetHeight() < 1) return;

  dec->CalculateFlatBBox(org.x, org.y);

  /*
  GCon->Logf(NAME_Debug, "decal '%s': angle=%g; scale=(%g,%g); spheight=%g; bbsize=(%g,%g)", *dec->name, dec->angleFlat.value,
    dec->scaleX.value, dec->scaleY.value, dec->spheight, dec->bbWidth(), dec->bbHeight());
  */

  if (dec->spheight == 0.0f || dec->bbWidth() < 1.0f || dec->bbHeight() < 1.0f) {
    if (!baddecals.put(dec->name, true)) GCon->Logf(NAME_Warning, "Decal '%s' has zero size", *dec->name);
    return;
  }

  // setup flips
  unsigned flips = (dec->flipXValue ? decal_t::FlipX : 0u)|(dec->flipYValue ? decal_t::FlipY : 0u);
  if (forceFlipX) flips ^= decal_t::FlipX;

  DInfo nfo;
  nfo.angle = (angleOverride ? AngleMod(angle) : dec->angleFlat.value);
  nfo.spheight = dec->spheight;
  nfo.org = org;
  nfo.range = range;
  nfo.dec = dec;
  nfo.translation = translation;
  nfo.Level = this;
  nfo.orflags = flips; //|(dec->fullbright ? decal_t::Fullbright : 0u)|(dec->fuzzy ? decal_t::Fuzzy : 0u); // done in `NewFlatDecal()`
  nfo.alpha = (alphaOverride ? alpha : -1.0f);

#ifndef VV_FLAT_DECAL_USE_FLOODFILL
  CheckBSPB2DBox(nfo.dec->bbox2d, &AddDecalToSubsector, &nfo);
#else
  // floodfill spread
  subsector_t *sub = PointInSubsector(org);
  /*
  if (sub->sector->isOriginalPObj()) return; //wtf?!
  if (sub->sector->isInnerPObj() && !sub->sector->ownpobj->Is3D()) return; //wtf?!
  */
  if (sub->sector->isAnyPObj()) return; // the thing that should not be
  IncSubTouchCounter();
  DecalFloodFill(&nfo, sub);
#endif
}


//==========================================================================
//
//  VLevel::CheckBootPrints
//
//  check what kind of bootprint decal is required at `org`
//  returns `false` if none (other vars are undefined)
//  otherwise:
//  `decalName` is decal name (WARNING! DON'T RETURN 'None' for no decals, return '' (empty name)!)
//  `decalTranslation` is translation (for translated blood)
//  `markTime` is the time the marks should be left (in seconds)
//
//  `sub` can be `nullptr`
//
//==========================================================================
bool VLevel::CheckBootPrints (TVec org, subsector_t *sub, VName &decalName, int &decalTranslation, float &markTime) {
  decalName = NAME_None;
  decalTranslation = 0;
  markTime = 0.0f;

  if (!subsectorDecalList) return false;

  if (!sub) sub = PointInSubsector(org);

  // check if we are inside any blood decal
  constexpr float shrinkRatio = 0.8f;
  float dcbb2d[4];
  for (decal_t *dc = subsectorDecalList[(unsigned)(ptrdiff_t)(sub-&Subsectors[0])].tail; dc; dc = dc->subprev) {
    if (dc->bootname == NAME_None) continue;
    if (!dc->isFloor()) continue;
    // check coords
    ShrinkBBox2D(dcbb2d, dc->bbox2d, shrinkRatio);
    if (!IsPointInside2DBBox(org.x, org.y, dcbb2d)) continue;
    /*if (dc->eregindex != 0)*/ {
      // 3d floor decal, check Z coord
      bool zok = false;
      int eregidx = 0;
      for (sec_region_t *reg = sub->sector->eregions; reg; reg = reg->next, ++eregidx) {
        if (eregidx == dc->eregindex) {
          const float fz = (eregidx ? reg->eceiling.GetPointZClamped(org) : reg->efloor.GetPointZClamped(org));
          if (fabsf(fz-org.z) <= 0.1f) {
            zok = true;
            //org.z = fz;
          }
          break;
        }
      }
      if (!zok) return false;
    }
    // it seems that we found it
    decalName = dc->bootname;
    decalTranslation = dc->translation;
    markTime = dc->boottime;
    return true;
  }

  // oops
  return false;
}
