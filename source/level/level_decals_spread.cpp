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


#ifdef VV_FLAT_DECAL_USE_FLOODFILL
// "subsector touched" marks for portal spread
static TArray<int> dcSubTouchMark;
static TMapNC<polyobj_t *, bool> dcPobjTouched;
static int dcSubTouchCounter = 0;
static bool dcPobjTouchedReset = false;
#endif


// ////////////////////////////////////////////////////////////////////////// //
struct DInfo {
  float bbox2d[4];
  TVec org;
  float range;
  float spheight; // spread height
  VDecalDef *dec;
  int translation;
  VLevel *Level;
};


#ifdef VV_FLAT_DECAL_USE_FLOODFILL
//==========================================================================
//
//  IncSubTouchCounter
//
//==========================================================================
static void IncSubTouchCounter (VLevel *Level) noexcept {
  dcPobjTouchedReset = false;
  if (dcSubTouchMark.length() != Level->NumSubsectors) {
    dcSubTouchMark.setLength(Level->NumSubsectors);
    for (auto &&v : dcSubTouchMark) v = 0;
    dcSubTouchCounter = 1;
    return;
  }
  if (++dcSubTouchCounter != MAX_VINT32) return;
  for (auto &&v : dcSubTouchMark) v = 0;
  dcSubTouchCounter = 1;
}


//==========================================================================
//
//  IsSubTouched
//
//  `IncSubTouchCounter()` should be already called
//
//  doesn't mark the subsector
//
//==========================================================================
static inline bool IsSubTouched (VLevel *Level, const subsector_t *sub) noexcept {
  return (sub ? (dcSubTouchMark.ptr()[(unsigned)(ptrdiff_t)(sub-&Level->Subsectors[0])] == dcSubTouchCounter) : true);
}


//==========================================================================
//
//  MarkSubTouched
//
//  `IncSubTouchCounter()` should be already called
//
//==========================================================================
static inline void MarkSubTouched (VLevel *Level, const subsector_t *sub) noexcept {
  if (sub) dcSubTouchMark.ptr()[(unsigned)(ptrdiff_t)(sub-&Level->Subsectors[0])] = dcSubTouchCounter;
}
#endif


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
          nfo->Level->NewFlatDecal(false/*asceiling*/, sub, eregidx, nfo->org.x, nfo->org.y, nfo->dec, nfo->translation);
          res |= PutAtCeiling;
        }
      } else if (fz > orgz-xhgt && fz < orgz+xhgt) {
        // do it
        nfo->Level->NewFlatDecal(true/*asfloor*/, sub, eregidx, nfo->org.x, nfo->org.y, nfo->dec, nfo->translation);
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
          nfo->Level->NewFlatDecal(true/*asfloor*/, sub, eregidx, nfo->org.x, nfo->org.y, nfo->dec, nfo->translation);
          res |= PutAtFloor;
        }
      } else if (cz > orgz-xhgt && cz < orgz+xhgt) {
        // do it
        nfo->Level->NewFlatDecal(false/*asceiling*/, sub, eregidx, nfo->org.x, nfo->org.y, nfo->dec, nfo->translation);
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
      if (orgz-2.0f <= fz && fz-orgz < xhgt) {
        // do it
        nfo->Level->NewFlatDecal(true/*asfloor*/, sub, eregidx, nfo->org.x, nfo->org.y, nfo->dec, nfo->translation);
        res |= PutAtFloor;
      }
    }
    // check ceiling
    if (!(reg->regflags&sec_region_t::RF_SkipFloorSurf) && IsGoodFlatTexture(reg->eceiling.splane->pic)) {
      const float cz = reg->eceiling.GetPointZClamped(nfo->org);
      if (orgz+2.0f >= cz && orgz-cz < xhgt) {
        // do it
        nfo->Level->NewFlatDecal(false/*asceiling*/, sub, eregidx, nfo->org.x, nfo->org.y, nfo->dec, nfo->translation);
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
  if (Level->IsBBox2DTouchingSubsector(sub, nfo->bbox2d)) {
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
  if (IsSubTouched(nfo->Level, sub)) return;
  MarkSubTouched(nfo->Level, sub);
  if (sub->numlines < 3) return;
  if (!nfo->Level->IsBBox2DTouchingSubsector(sub, nfo->bbox2d)) return;
  // put decals
  PutDecalToSubsector(nfo, sub);

  // process 3d polyobjects
  if (sub->HasPObjs()) {
    for (auto &&it : sub->PObjFirst()) {
      polyobj_t *pobj = it.pobj();
      if (!pobj->Is3D()) continue;
      if (!dcPobjTouchedReset) {
        dcPobjTouchedReset = true;
        dcPobjTouched.reset();
      }
      if (dcPobjTouched.put(pobj, true)) continue; // already processed
      for (subsector_t *posub = pobj->GetSector()->subsectors; posub; posub = posub->seclink) {
        if (!IsSubTouched(nfo->Level, posub)) {
          MarkSubTouched(nfo->Level, posub);
          if (posub->numlines >= 3) {
            if (nfo->Level->IsBBox2DTouchingSubsector(posub, nfo->bbox2d)) {
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
  if (sub->bbox2d[BOX2D_MINX] >= nfo->bbox2d[BOX2D_MINX] &&
      sub->bbox2d[BOX2D_MINY] >= nfo->bbox2d[BOX2D_MINY] &&
      sub->bbox2d[BOX2D_MAXX] <= nfo->bbox2d[BOX2D_MAXX] &&
      sub->bbox2d[BOX2D_MAXY] <= nfo->bbox2d[BOX2D_MAXY])
  {
    spreadAllSegs = true;
  }
  // check subsector segs
  const seg_t *seg = &nfo->Level->Segs[sub->firstline];
  for (int f = sub->numlines; f--; ++seg) {
    if (!seg->frontsector || !seg->backsector || !seg->partner) continue;
    if (IsSubTouched(nfo->Level, seg->partner->frontsub)) continue; // already processed
    // check if we should spread over this seg
    if (!spreadAllSegs) {
      // our box should intersect with the seg
      // check reject point
      if (seg->PointDistance(seg->get2DBBoxRejectPoint(nfo->bbox2d)) <= 0.0f) continue; // entire box on a back side
      // check accept point
      // if accept point on another side (or on plane), assume intersection
      if (seg->PointDistance(seg->get2DBBoxAcceptPoint(nfo->bbox2d)) >= 0.0f) continue;
    }
    // ok, we must spread over this seg; check if it is a closed something
    const line_t *line = seg->linedef;
    if (line && !line->pobj() && seg->frontsector != seg->backsector) {
      if (VViewClipper::IsSegAClosedSomething(nfo->Level, nullptr, seg)) continue; // oops, it is closed
      const float orgz = nfo->org.z;
      const float xhgt = nfo->range+nfo->spheight;
      // check if it is blocked by high floor
      if (seg->backsector->floor.maxz > seg->frontsector->floor.maxz) {
        // back sector floor is higher
        if (orgz+xhgt <= seg->backsector->floor.maxz) continue;
      }
      // check if it is blocked by low ceiling
      if (seg->frontsector->ceiling.minz < seg->backsector->ceiling.minz) {
        // front sector ceiling is lower
        if (orgz-xhgt >= seg->frontsector->ceiling.minz) continue;
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
void VLevel::SpreadFlatDecalEx (const TVec org, float range, VDecalDef *dec, int level, int translation) {
  if (level > 16) {
    GCon->Logf(NAME_Warning, "too many lower decals '%s'", *dec->name);
    return;
  }

  if (dec->lowername != NAME_None && !VStr::strEquCI(*dec->lowername, "none")) {
    VDecalDef *dcl = VDecalDef::getDecal(dec->lowername);
    if (dcl) SpreadFlatDecalEx(org, range, dcl, level+1, translation);
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
  nfo.Level = this;

#ifndef VV_FLAT_DECAL_USE_FLOODFILL
  CheckBSPB2DBox(nfo.bbox2d, &AddDecalToSubsector, &nfo);
#else
  // floodfill spread
  subsector_t *sub = PointInSubsector(org);
  /*
  if (sub->sector->isOriginalPObj()) return; //wtf?!
  if (sub->sector->isInnerPObj() && !sub->sector->ownpobj->Is3D()) return; //wtf?!
  */
  if (sub->sector->isAnyPObj()) return; // the thing that should not be
  IncSubTouchCounter(this);
  DecalFloodFill(&nfo, sub);
#endif
}
