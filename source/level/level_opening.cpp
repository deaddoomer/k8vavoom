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

//#define VV_DUMP_OPENING_CREATION

//k8: this should be enough for everyone, lol
#define MAX_OPENINGS  (16384)
static opening_t openings[MAX_OPENINGS];

// used in `BuildSectorOpenings()`
// sorry for this global
static TArray<opening_t> solids;

// used in `FindGapFloorCeiling()`
static TArray<opening_t> oplist_gfc;

// used in `LineOpenings()`
static TArray<opening_t> op0list_lop;
static TArray<opening_t> op1list_lop;



//==========================================================================
//
//  VLevel::DumpOpening
//
//==========================================================================
void VLevel::DumpOpening (const opening_t *op) noexcept {
  GCon->Logf(NAME_Debug, "  %p: floor=%g (%g,%g,%g:%g); ceil=%g (%g,%g,%g:%g); lowfloor=%g; range=%g",
    op,
    op->bottom, op->efloor.GetNormal().x, op->efloor.GetNormal().y, op->efloor.GetNormal().z, op->efloor.GetDist(),
    op->top, op->eceiling.GetNormal().x, op->eceiling.GetNormal().y, op->eceiling.GetNormal().z, op->eceiling.GetDist(),
    op->lowfloor, op->range);
}


//==========================================================================
//
//  VLevel::DumpOpPlanes
//
//==========================================================================
void VLevel::DumpOpPlanes (const TArray<opening_t> &list) noexcept {
  GCon->Logf(NAME_Debug, " ::: count=%d :::", list.length());
  for (int f = 0; f < list.length(); ++f) DumpOpening(&list[f]);
}


//==========================================================================
//
//  VLevel::DumpRegion
//
//==========================================================================
void VLevel::DumpRegion (const sec_region_t *inregion, bool extendedflags) noexcept {
  if (!inregion) { GCon->Log(NAME_Debug, "  <nullregion>"); return; }

  VStr flg;
  if (extendedflags) {
    const bool baseReg = (inregion->regflags&sec_region_t::RF_BaseRegion);
    if (baseReg) flg += ",BASE"; else flg += ",NON-BASE";
    if (inregion->regflags&sec_region_t::RF_SaneRegion) flg += ",SANE";
    if (inregion->regflags&sec_region_t::RF_NonSolid) flg += ",NONSOLID"; else { if (!baseReg) flg += ",SOLID"; }
    if (inregion->regflags&sec_region_t::RF_OnlyVisual) flg += ",VISUAL";
    if (inregion->regflags&sec_region_t::RF_SkipFloorSurf) flg += ",NOFLOOR";
    if (inregion->regflags&sec_region_t::RF_SkipCeilSurf) flg += ",NOCEIL";
    if (!flg.isEmpty()) {
      flg += "]";
      flg.getMutableCStr()[0] = '[';
    }
  }

  GCon->Logf(NAME_Debug, "  %p: floor=(%g,%g,%g:%g):%g; (%g : %g), flags=0x%04x; ceil=(%g,%g,%g:%g):%g; (%g : %g), flags=0x%04x; eline=%d; rflags=0x%02x%s%s",
    inregion,
    inregion->efloor.GetNormal().x,
    inregion->efloor.GetNormal().y,
    inregion->efloor.GetNormal().z,
    inregion->efloor.GetDist(), inregion->efloor.GetRealDist(),
    inregion->efloor.splane->minz, inregion->efloor.splane->maxz,
    inregion->efloor.splane->flags,
    inregion->eceiling.GetNormal().x,
    inregion->eceiling.GetNormal().y,
    inregion->eceiling.GetNormal().z,
    inregion->eceiling.GetDist(), inregion->eceiling.GetRealDist(),
    inregion->eceiling.splane->minz, inregion->eceiling.splane->maxz,
    inregion->eceiling.splane->flags,
    (inregion->extraline ? 1 : 0),
    inregion->regflags, (flg.isEmpty() ? "" : " "), *flg);
}


//==========================================================================
//
//  VLevel::InsertOpening
//
//  insert new opening, maintaing order and non-intersection invariants
//  it is faster to insert openings from bottom to top,
//  but it is not a requerement
//
//  note that this does joining logic for "solid" openings
//
//  in op: efloor, bottom, eceiling, top -- should be set and valid
//
//==========================================================================
void VLevel::InsertOpening (TArray<opening_t> &dest, const opening_t &op) {
  if (op.top < op.bottom) return; // shrinked to invalid size
  int dlen = dest.length();
  if (dlen == 0) {
    dest.append(op);
    return;
  }
  opening_t *ops = dest.ptr(); // to avoid range checks
  // find region that contains our floor
  const float opfz = op.bottom;
  // check if current is higher than the last
  if (opfz > ops[dlen-1].top) {
    // append and exit
    dest.append(op);
    return;
  }
  // starts where last ends
  if (opfz == ops[dlen-1].top) {
    // grow last and exit
    ops[dlen-1].top = op.top;
    ops[dlen-1].eceiling = op.eceiling;
    return;
  }
  /*
    7 -- max array index
    5
    3 -- here for 2 and 3
    1 -- min array index (0)
  */
  // find a place and insert
  // then join regions
  int cpos = dlen;
  while (cpos > 0 && opfz <= ops[cpos-1].bottom) --cpos;
  // now, we can safely insert it into cpos
  // if op floor is the same as cpos ceiling, join
  if (cpos < dlen && opfz == ops[cpos].bottom) {
    // join (but check if new region is bigger first)
    if (op.top <= ops[cpos].top) return; // completely inside
    ops[cpos].eceiling = op.eceiling;
    ops[cpos].top = op.top;
  } else {
    // check if new bottom is inside a previous region
    if (cpos > 0 && opfz <= ops[cpos-1].top) {
      // yes, join with previous region
      --cpos;
      ops[cpos].eceiling = op.eceiling;
      ops[cpos].top = op.top;
    } else {
      // no, insert, and let the following loop take care of joins
      dest.insert(cpos, op);
      ops = dest.ptr();
      ++dlen;
    }
  }
  // now check if `cpos` region intersects with upper regions, and perform joins
  int npos = cpos+1;
  while (npos < dlen) {
    // below?
    if (ops[cpos].top < ops[npos].bottom) break; // done
    // npos is completely inside?
    if (ops[cpos].top >= ops[npos].top) {
      // completely inside: eat it and continue
      dest.removeAt(npos);
      ops = dest.ptr();
      --dlen;
      continue;
    }
    // join cpos (floor) and npos (ceiling)
    ops[cpos].eceiling = ops[npos].eceiling;
    ops[cpos].top = ops[npos].top;
    dest.removeAt(npos);
    // done
    break;
  }
}


//==========================================================================
//
//  VLevel::GetBaseSectorOpening
//
//==========================================================================
void VLevel::GetBaseSectorOpening (opening_t &op, sector_t *sector, const TVec point, bool usePoint) {
  op.efloor = sector->eregions->efloor;
  op.eceiling = sector->eregions->eceiling;
  if (usePoint) {
    // we cannot go out of sector heights
    op.bottom = op.efloor.GetPointZClamped(point);
    op.top = op.eceiling.GetPointZClamped(point);
  } else {
    op.bottom = op.efloor.splane->minz;
    op.top = op.eceiling.splane->maxz;
  }
  op.range = op.top-op.bottom;
  op.lowfloor = op.bottom;
  op.highceiling = op.top;
  op.elowfloor = op.efloor;
  op.ehighceiling = op.eceiling;
  op.next = nullptr;
}


//==========================================================================
//
//  VLevel::Insert3DMidtex
//
//==========================================================================
void VLevel::Insert3DMidtex (TArray<opening_t> &dest, const sector_t *sector, const line_t *linedef) {
  if (!(linedef->flags&ML_3DMIDTEX)) return;
  // for 3dmidtex, create solid from midtex bottom to midtex top
  //   from floor to midtex bottom
  //   from midtex top to ceiling
  float cz, fz;
  if (!GetMidTexturePosition(linedef, 0, &cz, &fz)) return;
  if (fz > cz) return; // k8: is this right?
  opening_t op;
  op.efloor = sector->eregions->efloor;
  op.bottom = fz;
  op.eceiling = sector->eregions->eceiling;
  op.top = cz;
  // flip floor and ceiling if they are pointing outside a region
  // i.e. they should point *inside*
  // we will use this fact to create correct "empty" openings
  if (op.efloor.GetNormalZ() < 0.0f) op.efloor.Flip();
  if (op.eceiling.GetNormalZ() > 0.0f) op.eceiling.Flip();
  // inserter will join regions
  InsertOpening(dest, op);
}


//==========================================================================
//
//  VLevel::BuildSectorOpenings
//
//  this function doesn't like regions with floors that has different flags
//
//==========================================================================
void VLevel::BuildSectorOpenings (const line_t *xldef, TArray<opening_t> &dest, sector_t *sector, const TVec point,
                                  unsigned NoBlockFlags, bool linkList, bool usePoint, bool skipNonSolid, bool forSurface)
{
  dest.resetNoDtor();
  // if this sector has no 3d floors, we don't need to do any extra work
  if (!sector->Has3DFloors() /*|| !(sector->SectorFlags&sector_t::SF_Has3DMidTex)*/ && (!xldef || !(xldef->flags&ML_3DMIDTEX))) {
    opening_t &op = dest.alloc();
    GetBaseSectorOpening(op, sector, point, usePoint);
    return;
  }
  //if (thisIs3DMidTex) Insert3DMidtex(op1list, linedef->backsector, linedef);

  /* build list of closed regions (it doesn't matter if region is non-solid, as long as it satisfies flag mask).
     that list has all intersecting regions joined.
     then cut those solids from main empty region.
   */
  solids.resetNoDtor();
  opening_t cop;
  cop.lowfloor = 0.0f; // for now
  cop.highceiling = 0.0f; // for now
  // skip base region for now
  for (const sec_region_t *reg = sector->eregions; reg; reg = reg->next) {
    if (reg->regflags&(sec_region_t::RF_BaseRegion|sec_region_t::RF_OnlyVisual)) continue; // base, or pure visual region, ignore it
    if (skipNonSolid && (reg->regflags&sec_region_t::RF_NonSolid)) continue;
    if (((reg->efloor.splane->flags|reg->eceiling.splane->flags)&NoBlockFlags) != 0) continue; // bad flags
    // hack: 3d floor with sky texture seems to be transparent in renderer
    if (forSurface && reg->extraline) {
      const side_t *sidedef = &Sides[reg->extraline->sidenum[0]];
      if (sidedef->MidTexture == skyflatnum) continue;
    }
    // border points
    float fz = reg->efloor.splane->minz;
    float cz = reg->eceiling.splane->maxz;
    if (usePoint) {
      fz = reg->efloor.GetPointZClamped(point);
      cz = reg->eceiling.GetPointZClamped(point);
    }
    // k8: just in case
    //if (fz > cz && (reg->efloor.isSlope() || reg->eceiling.isSlope())) { float tmp = fz; fz = cz; cz = tmp; }
    if (fz > cz) fz = cz;
    cop.efloor = reg->efloor;
    cop.bottom = fz;
    cop.eceiling = reg->eceiling;
    cop.top = cz;
    // flip floor and ceiling if they are pointing outside a region
    // i.e. they should point *inside*
    // we will use this fact to create correct "empty" openings
    if (cop.efloor.GetNormalZ() < 0.0f) cop.efloor.Flip();
    if (cop.eceiling.GetNormalZ() > 0.0f) cop.eceiling.Flip();
    // inserter will join regions
    InsertOpening(solids, cop);
  }
  // add 3dmidtex, if there are any
  if (!forSurface) {
    if (xldef && (xldef->flags&ML_3DMIDTEX)) {
      Insert3DMidtex(solids, sector, xldef);
    } else if (usePoint && sector->SectorFlags&sector_t::SF_Has3DMidTex) {
      //FIXME: we need to know a radius here
      /*
      GCon->Logf("!!!!!");
      line_t *const *lparr = sector->lines;
      for (int lct = sector->linecount; lct--; ++lparr) {
        const line_t *ldf = *lparr;
        Insert3DMidtex(solids, sector, ldf);
      }
      */
    }
    //if (thisIs3DMidTex) Insert3DMidtex(op1list, linedef->backsector, linedef);
  }

  // if we have no openings, or openings are out of bounds, just use base sector region
  float secfz, seccz;
  if (usePoint) {
    secfz = sector->floor.GetPointZClamped(point);
    seccz = sector->ceiling.GetPointZClamped(point);
  } else {
    secfz = sector->floor.minz;
    seccz = sector->ceiling.maxz;
  }

  if (solids.length() == 0 || solids[solids.length()-1].top <= secfz || solids[0].bottom >= seccz) {
    opening_t &op = dest.alloc();
    GetBaseSectorOpening(op, sector, point, usePoint);
    return;
  }
  /* now we have to cut out all solid regions from base one
     this can be done in a simple loop, because all solids are non-intersecting
     paper-thin regions should be cutted too, as those can be real floors/ceilings

     the algorithm is simple:
       get sector floor as current floor.
       for each solid:
         if current floor if lower than solid floor:
           insert opening from current floor to solid floor
         set current floor to solid ceiling
     take care that "emptyness" floor and ceiling are pointing inside
   */
  TSecPlaneRef currfloor;
  currfloor.set(&sector->floor, false);
  float currfloorz = secfz;
  vassert(currfloor.isFloor());
  // HACK: if the whole sector is taken by some region, return sector opening
  //       this is required to proper 3d-floor backside creation
  //       alas, `hadNonSolid` hack is required to get rid of "nano-water walls"
  opening_t *cs = solids.ptr();
  if (!usePoint /*&& !hadNonSolid*/ && solids.length() == 1) {
    if (cs[0].bottom <= sector->floor.minz && cs[0].top >= sector->ceiling.maxz) {
      opening_t &op = dest.alloc();
      GetBaseSectorOpening(op, sector, point, usePoint);
      return;
    }
  }
  // main loop
  for (int scount = solids.length(); scount--; ++cs) {
    if (cs->bottom >= seccz) break; // out of base sector bounds, we are done here
    if (cs->top < currfloorz) continue; // below base sector bounds, nothing to do yet
    if (currfloorz <= cs->bottom) {
      // insert opening from current floor to solid floor
      cop.efloor = currfloor;
      cop.bottom = currfloorz;
      cop.eceiling = cs->efloor;
      cop.top = cs->bottom;
      cop.range = cop.top-cop.bottom;
      // flip ceiling if necessary, so it will point inside
      if (!cop.eceiling.isCeiling()) cop.eceiling.Flip();
      dest.append(cop);
    }
    // set current floor to solid ceiling (and flip, if necessary)
    currfloor = cs->eceiling;
    currfloorz = cs->top;
    if (!currfloor.isFloor()) currfloor.Flip();
  }
  // add top cap (to sector base ceiling) if necessary
  if (currfloorz <= seccz) {
    cop.efloor = currfloor;
    cop.bottom = currfloorz;
    cop.eceiling.set(&sector->ceiling, false);
    cop.top = seccz;
    cop.range = cop.top-cop.bottom;
    dest.append(cop);
  }
  // if we aren't using point, join openings with paper-thin edges
  if (!usePoint) {
    cs = dest.ptr();
    int dlen = dest.length();
    int dpos = 0;
    while (dpos < dlen-1) {
      if (cs[dpos].top == cs[dpos+1].bottom) {
        // join
        cs[dpos].top = cs[dpos+1].top;
        cs[dpos].eceiling = cs[dpos+1].eceiling;
        cs[dpos].range = cs[dpos].top-cs[dpos].bottom;
        dest.removeAt(dpos+1);
        cs = dest.ptr();
        --dlen;
      } else {
        // skip
        ++dpos;
      }
    }
  }
  // link list, if necessary
  if (linkList && dest.length()) {
    cs = dest.ptr();
    for (int f = dest.length()-1; f > 0; --f) cs[f-1].next = &cs[f];
    cs[dest.length()-1].next = nullptr;
  }
}


//==========================================================================
//
//  VLevel::PointContents
//
//==========================================================================
int VLevel::PointContents (sector_t *sector, const TVec &p, bool dbgDump) {
  if (!sector) return 0;

  //dbgDump = true;
  if (sector->heightsec && (sector->heightsec->SectorFlags&sector_t::SF_UnderWater) &&
      p.z <= sector->heightsec->floor.GetPointZClamped(p))
  {
    if (dbgDump) GCon->Log(NAME_Debug, "SVP: case 0");
    return CONTENTS_BOOMWATER;
  }

  if (sector->SectorFlags&sector_t::SF_UnderWater) {
    if (dbgDump) GCon->Log(NAME_Debug, "SVP: case 1");
    return CONTENTS_BOOMWATER;
  }

  const sec_region_t *best = sector->eregions;

  if (sector->Has3DFloors()) {
    const float secfz = sector->floor.GetPointZClamped(p);
    const float seccz = sector->ceiling.GetPointZClamped(p);
    // out of sector's empty space?
    if (p.z < secfz || p.z > seccz) {
      if (dbgDump) GCon->Log(NAME_Debug, "SVP: case 2");
      return best->params->contents;
    }

    // ignore solid regions, as we cannot be inside them legally
    float bestDist = 999999.0f; // minimum distance to region floor
    for (const sec_region_t *reg = sector->eregions->next; reg; reg = reg->next) {
      if (reg->regflags&(sec_region_t::RF_OnlyVisual|sec_region_t::RF_BaseRegion)) continue;
      // "sane" regions may represent Vavoom water
      if ((reg->regflags&(sec_region_t::RF_SaneRegion|sec_region_t::RF_NonSolid)) == sec_region_t::RF_SaneRegion) {
        if (dbgDump) { GCon->Logf(NAME_Debug, "SVP: sane region"); DumpRegion(reg); }
        if (!reg->params || (int)reg->params->contents <= 0) continue;
        if (dbgDump) { GCon->Logf(NAME_Debug, "SVP: sane non-solid region"); DumpRegion(reg); }
        if (dbgDump) { GCon->Logf(NAME_Debug, "   botz=%g; topz=%g", reg->efloor.GetPointZ(p), reg->eceiling.GetPointZ(p)); }
        // it may be paper-thin
        const float topz = reg->eceiling.GetPointZ(p);
        const float botz = reg->efloor.GetPointZ(p);
        const float minz = min2(botz, topz);
        // everything beneath it is a water
        if (p.z <= minz) {
          best = reg;
          break;
        }
      } else if (!(reg->regflags&sec_region_t::RF_NonSolid)) {
        continue;
      }
      // non-solid
      if (dbgDump) { GCon->Logf(NAME_Debug, "SVP: checking region"); DumpRegion(reg); }
      const float rtopz = reg->eceiling.GetPointZ(p);
      const float rbotz = reg->efloor.GetPointZ(p);
      // ignore paper-thin regions
      if (rtopz <= rbotz) continue; // invalid, or paper-thin, ignore
      // check if point is inside, and for best ceiling dist
      if (p.z >= rbotz && p.z < rtopz) {
        const float fdist = rtopz-p.z;
        if (dbgDump) GCon->Logf(NAME_Debug, "SVP: non-solid check: bestDist=%g; fdist=%g; p.z=%g; botz=%g; topz=%g", bestDist, fdist, p.z, rbotz, rtopz);
        if (fdist < bestDist) {
          if (dbgDump) GCon->Log(NAME_Debug, "SVP:   NON-SOLID HIT!");
          bestDist = fdist;
          best = reg;
          //wasHit = true;
        }
      } else {
        if (dbgDump) GCon->Logf(NAME_Debug, "SVP: non-solid SKIP: bestDist=%g; cdist=%g; p.z=%g; botz=%g; topz=%g", bestDist, rtopz-p.z, p.z, rbotz, rtopz);
      }
    }
  }

  if (dbgDump) { GCon->Logf(NAME_Debug, "SVP: best region"); DumpRegion(best); }
  return best->params->contents;
}


//==========================================================================
//
//  FindGapFloorCeiling
//
//  find region for thing, and return best floor/ceiling
//  `p.z` is bottom
//
//  this is used mostly in sector movement, so we should try hard to stay
//  inside our current gap, so we won't teleport up/down from lifts, and
//  from crushers
//
//  this ignores 3d pobjs yet
//
//  used in simplified `VEntity::LinkToWorld()`, and
//  in `VEntity::CheckPosition()` (along with `VEntity::CheckDropOff()`)
//
//==========================================================================
void VLevel::FindGapFloorCeiling (sector_t *sector, const TVec point, float height, TSecPlaneRef &floor, TSecPlaneRef &ceiling, bool debugDump) {
  /*
  if (debugDump) {
    GCon->Logf("=== ALL OPENINGS: sector %p ===", sector);
    for (const sec_region_t *reg = sector->eregions; reg; reg = reg->next) VLevel::DumpRegion(reg);
  }
  */

  if (!sector->Has3DFloors()) {
    // only one region, yay
    //FIXME: this is wrong, because we may have 3dmidtex
    floor = sector->eregions->efloor;
    ceiling = sector->eregions->eceiling;
    return;
  }

  if (height < 0.0f) height = 0.0f;

  /* for multiple regions, we have some hard work to do.
     as region sorting is not guaranteed, we will force-sort regions.
     alas.
   */
  oplist_gfc.resetNoDtor();

  BuildSectorOpenings(nullptr, oplist_gfc, sector, point, SPF_NOBLOCKING, true/*linkList*/, true/*usePoint*/);
  if (oplist_gfc.length() == 0) {
    // something is very wrong here, so use sector boundaries
    floor = sector->eregions->efloor;
    ceiling = sector->eregions->eceiling;
    return;
  }

  if (debugDump) { GCon->Logf(NAME_Debug, "=== ALL OPENINGS (z=%g; height=%g) ===", point.z, height); DumpOpPlanes(oplist_gfc); }

#if 1
  opening_t *opres = FindRelOpening(oplist_gfc.ptr(), point.z, point.z+height);
  if (opres) {
    if (debugDump) { GCon->Logf(NAME_Debug, " found result"); DumpOpening(opres); }
    floor = opres->efloor;
    ceiling = opres->eceiling;
  } else {
    if (debugDump) GCon->Logf(NAME_Debug, " NO result");
    floor = sector->eregions->efloor;
    ceiling = sector->eregions->eceiling;
  }
#else
  // now find best-fit region:
  //
  //  1. if the thing fits in one of the gaps without moving vertically,
  //     then choose that gap (one with less distance to the floor).
  //
  //  2. if there is only *one* gap which the thing could fit in, then
  //     choose that gap.
  //
  //  3. if there is multiple gaps which the thing could fit in, choose
  //     the gap whose floor is closest to the thing's current Z.
  //
  //  4. if there is no gaps which the thing could fit in, do the same.

  // one the thing can possibly fit
  const opening_t *bestGap = nullptr;
  float bestGapDist = 999999.0f;

  const opening_t *op = oplist_gfc.ptr();
  for (int opleft = oplist_gfc.length(); opleft--; ++op) {
    const float fz = op->bottom;
    const float cz = op->top;
    if (point.z >= fz && point.z <= cz) {
      // no need to move vertically
      if (debugDump) { GCon->Logf(NAME_Debug, " best fit"); DumpOpening(op); }
      return op;
    } else {
      const float fdist = fabsf(point.z-fz); // we don't care about sign here
      if (!bestGap || fdist < bestGapDist) {
        if (debugDump) { GCon->Logf(NAME_Debug, " gap fit"); DumpOpening(op); }
        bestGap = op;
        bestGapDist = fdist;
        //if (fdist == 0.0f) break; // there is no reason to look further
      } else {
        if (debugDump) { GCon->Logf(NAME_Debug, " REJECTED gap fit"); DumpOpening(op); }
      }
    }
  }

  if (bestFit) {
    if (debugDump) { GCon->Logf(NAME_Debug, " best result"); DumpOpening(bestFit); }
    floor = bestFit->efloor;
    ceiling = bestFit->eceiling;
  } else if (bestGap) {
    if (debugDump) { GCon->Logf(NAME_Debug, " gap result"); DumpOpening(bestGap); }
    floor = bestGap->efloor;
    ceiling = bestGap->eceiling;
  } else {
    // just fit into sector
    if (debugDump) { GCon->Logf(NAME_Debug, " no result"); }
    floor = sector->eregions->efloor;
    ceiling = sector->eregions->eceiling;
  }
#endif
}


//==========================================================================
//
//  VLevel::GetSectorGapCoords
//
//  this is only used in VavoomC code, for `BloodAddZClamped()`
//
//==========================================================================
void VLevel::GetSectorGapCoords (sector_t *sector, const TVec point, float &floorz, float &ceilz) {
  if (!sector) { floorz = 0.0f; ceilz = 0.0f; return; }
  if (!sector->Has3DFloors()) {
    floorz = sector->floor.GetPointZClamped(point);
    ceilz = sector->ceiling.GetPointZClamped(point);
    return;
  }
  TSecPlaneRef f, c;
  FindGapFloorCeiling(sector, point, 0, f, c);
  floorz = f.GetPointZClamped(point);
  ceilz = c.GetPointZClamped(point);
}


//==========================================================================
//
//  VLevel::GetSectorFloorPointZ
//
//  FIXME: this ignores 3d pobjs and 3d floors!
//
//  this is only used in VavoomC code, for `EV_SilentLineTeleport()`
//  as silent teleport isn't passing a valid z coord here, there is no need
//  to check 3d floors yet
//
//==========================================================================
float VLevel::GetSectorFloorPointZ (sector_t *sector, const TVec &point) {
  if (!sector) return 0.0f; // idc
  return sector->floor.GetPointZClamped(point); // cannot be lower than this
  #if 0
  if (!sector->Has3DFloors()) return sector->floor.GetPointZClamped(point); // cannot be lower than this
  // find best solid 3d ceiling
  vassert(sector->eregions);
  float bestz = sector->floor.GetPointZClamped(point);
  float bestdist = fabsf(bestz-point.z);
  for (sec_region_t *reg = sector->eregions; reg; reg = reg->next) {
    if ((reg->regflags&(sec_region_t::RF_NonSolid|sec_region_t::RF_OnlyVisual|sec_region_t::RF_BaseRegion)) != 0) continue;
    // ceiling is at the top
    const float cz = reg->eceiling.GetPointZClamped(point);
    const float dist = fabsf(cz-point.z);
    if (dist < bestdist) {
      bestz = cz;
      bestdist = dist;
    }
  }
  return bestz;
  #endif
}


//==========================================================================
//
//  VLevel::LineOpenings
//
//  sets opentop and openbottom to the window through a two sided line
//
//==========================================================================
opening_t *VLevel::LineOpenings (const line_t *linedef, const TVec point, unsigned NoBlockFlags, bool do3dmidtex, bool usePoint) {
  if (linedef->sidenum[1] == -1 || !linedef->backsector) return nullptr; // single sided line

  NoBlockFlags &= (SPF_MAX_FLAG-1u);

  // fast algo for two sectors without 3d floors
  if (!linedef->frontsector->Has3DFloors() &&
      !linedef->backsector->Has3DFloors() &&
      //!thisIs3DMidTex &&
      !((linedef->frontsector->SectorFlags|linedef->backsector->SectorFlags)&sector_t::SF_Has3DMidTex))
  {
    opening_t fop, bop;
    GetBaseSectorOpening(fop, linedef->frontsector, point, usePoint);
    GetBaseSectorOpening(bop, linedef->backsector, point, usePoint);
    // no intersection?
    if (fop.top <= bop.bottom || bop.top <= fop.bottom ||
        fop.bottom >= bop.top || bop.bottom >= fop.top)
    {
      return nullptr;
    }
    // create opening
    opening_t *dop = &openings[0];
    // setup floor
    if (fop.bottom == bop.bottom) {
      // prefer non-sloped sector
      if (fop.efloor.isSlope() && !bop.efloor.isSlope()) {
        dop->efloor = bop.efloor;
        dop->bottom = bop.bottom;
        dop->lowfloor = fop.bottom;
        dop->elowfloor = fop.efloor;
      } else {
        dop->efloor = fop.efloor;
        dop->bottom = fop.bottom;
        dop->lowfloor = bop.bottom;
        dop->elowfloor = bop.efloor;
      }
    } else if (fop.bottom > bop.bottom) {
      dop->efloor = fop.efloor;
      dop->bottom = fop.bottom;
      dop->lowfloor = bop.bottom;
      dop->elowfloor = bop.efloor;
    } else {
      dop->efloor = bop.efloor;
      dop->bottom = bop.bottom;
      dop->lowfloor = fop.bottom;
      dop->elowfloor = fop.efloor;
    }
    // setup ceiling
    if (fop.top == bop.top) {
      // prefer non-sloped sector
      if (fop.eceiling.isSlope() && !bop.eceiling.isSlope()) {
        dop->eceiling = bop.eceiling;
        dop->top = bop.top;
        dop->highceiling = fop.top;
        dop->ehighceiling = fop.eceiling;
      } else {
        dop->eceiling = fop.eceiling;
        dop->top = fop.top;
        dop->highceiling = bop.top;
        dop->ehighceiling = bop.eceiling;
      }
    } else if (fop.top <= bop.top) {
      dop->eceiling = fop.eceiling;
      dop->top = fop.top;
      dop->highceiling = bop.top;
      dop->ehighceiling = bop.eceiling;
    } else {
      dop->eceiling = bop.eceiling;
      dop->top = bop.top;
      dop->highceiling = fop.top;
      dop->ehighceiling = fop.eceiling;
    }
    dop->range = dop->top-dop->bottom;
    dop->next = nullptr;

    return dop;
  }

  // has 3d floors at least on one side, do full-featured intersection calculation
  op0list_lop.resetNoDtor();
  op1list_lop.resetNoDtor();

  BuildSectorOpenings(linedef, op0list_lop, linedef->frontsector, point, NoBlockFlags, false/*linkList*/, usePoint);
  if (op0list_lop.length() == 0) {
    // just in case: no front sector openings
    return nullptr;
  }

  BuildSectorOpenings(linedef, op1list_lop, linedef->backsector, point, NoBlockFlags, false/*linkList*/, usePoint);
  if (op1list_lop.length() == 0) {
    // just in case: no back sector openings
    return nullptr;
  }

#ifdef VV_DUMP_OPENING_CREATION
  GCon->Logf(NAME_Debug, "*** line: %p (0x%02x) ***", linedef, NoBlockFlags);
  GCon->Log(NAME_Debug, "::: before :::"); VLevel::DumpOpPlanes(op0list_lop); VLevel::DumpOpPlanes(op1list_lop);
#endif

  /* build intersections
     both lists are valid (sorted, without intersections -- but with possible paper-thin regions)
   */
  opening_t *dest = openings;
  unsigned destcount = 0;

  const opening_t *op0 = op0list_lop.ptr();
  int op0left = op0list_lop.length();

  const opening_t *op1 = op1list_lop.ptr();
  int op1left = op1list_lop.length();

  while (op0left && op1left) {
    // if op0 is below op1, skip op0
    if (op0->top < op1->bottom) {
#ifdef VV_DUMP_OPENING_CREATION
      GCon->Log(NAME_Debug, " +++ SKIP op0 (dump: op0, op1) +++");
      VLevel::DumpOpening(op0);
      VLevel::DumpOpening(op1);
#endif
      ++op0; --op0left;
      continue;
    }
    // if op1 is below op0, skip op1
    if (op1->top < op0->bottom) {
#ifdef VV_DUMP_OPENING_CREATION
      GCon->Log(NAME_Debug, " +++ SKIP op1 (dump: op0, op1) +++");
      VLevel::DumpOpening(op0);
      VLevel::DumpOpening(op1);
#endif
      ++op1; --op1left;
      continue;
    }
    // here op0 and op1 are intersecting
    vassert(op0->bottom <= op1->top);
    vassert(op1->bottom <= op0->top);
    if (destcount >= MAX_OPENINGS) Host_Error("too many line openings");
    // floor
    if (op0->bottom >= op1->bottom) {
      dest->efloor = op0->efloor;
      dest->bottom = op0->bottom;
      dest->lowfloor = op1->bottom;
      dest->elowfloor = op1->efloor;
    } else {
      dest->efloor = op1->efloor;
      dest->bottom = op1->bottom;
      dest->lowfloor = op0->bottom;
      dest->elowfloor = op0->efloor;
    }
    // ceiling
    if (op0->top <= op1->top) {
      dest->eceiling = op0->eceiling;
      dest->top = op0->top;
      dest->highceiling = op1->top;
      dest->ehighceiling = op1->eceiling;
    } else {
      dest->eceiling = op1->eceiling;
      dest->top = op1->top;
      dest->highceiling = op0->top;
      dest->ehighceiling = op0->eceiling;
    }
    dest->range = dest->top-dest->bottom;
#ifdef VV_DUMP_OPENING_CREATION
    GCon->Log(NAME_Debug, " +++ NEW opening (dump: op0, op1, new) +++");
    VLevel::DumpOpening(op0);
    VLevel::DumpOpening(op1);
    VLevel::DumpOpening(dest);
#endif
    ++dest;
    ++destcount;
    // if both regions ends at the same height, skip both,
    // otherwise skip region with lesser top
    if (op0->top == op1->top) {
#ifdef VV_DUMP_OPENING_CREATION
      GCon->Log(NAME_Debug, " +++ SKIP BOTH +++");
#endif
      ++op0; --op0left;
      ++op1; --op1left;
    } else if (op0->top < op1->top) {
#ifdef VV_DUMP_OPENING_CREATION
      GCon->Log(NAME_Debug, " +++ SKIP OP0 +++");
#endif
      ++op0; --op0left;
    } else {
#ifdef VV_DUMP_OPENING_CREATION
      GCon->Log(NAME_Debug, " +++ SKIP OP1 +++");
#endif
      ++op1; --op1left;
    }
  }

#ifdef VV_DUMP_OPENING_CREATION
  GCon->Logf(NAME_Debug, "::: after (%u) :::", destcount);
  for (unsigned f = 0; f < destcount; ++f) {
    const opening_t *xop = &openings[f];
    VLevel::DumpOpening(xop);
  }
  GCon->Log(NAME_Debug, "-----------------------------");
#endif

  // no intersections?
  if (destcount == 0) {
    // oops
    return nullptr;
  }

  if (destcount > 1) {
    for (unsigned f = 0; f < destcount-1; ++f) {
      openings[f].next = &openings[f+1];
    }
  }
  openings[destcount-1].next = nullptr;
  return openings;
}


//==========================================================================
//
//  VLevel::FindRelOpening
//
//  used in sector movement, so it tries hard to not leave current opening
//
//  used in `VEntity::CheckRelPosition()`, and in `FindGapFloorCeiling()`
//
//==========================================================================
opening_t *VLevel::FindRelOpening (opening_t *InGaps, float z1, float z2) noexcept {
  // check for trivial gaps
  if (!InGaps) return nullptr;
  if (!InGaps->next) return InGaps;

  // there are 2 or more gaps; now it gets interesting :-)
  if (z2 < z1) z2 = z1;

  // as we cannot be lower or higher than the base sector, clamp values
  float gapminz = FLT_MAX;
  float gapmaxz = -FLT_MAX;
  for (opening_t *gap = InGaps; gap; gap = gap->next) {
    if (gapminz > gap->bottom) gapminz = gap->bottom;
    if (gapmaxz < gap->top) gapmaxz = gap->top;
  }

  if (z1 > gapmaxz) {
    const float hgt = z2-z1;
    z2 = gapmaxz;
    z1 = z2-hgt;
  }

  if (z1 < gapminz) {
    const float hgt = z2-z1;
    z1 = gapminz;
    z2 = z1+hgt;
  }

  opening_t *fit_closest = nullptr;
  float fit_mindist = FLT_MAX; //99999.0f;

  opening_t *nofit_closest = nullptr;
  float nofit_mindist = FLT_MAX; //99999.0f;

  const float zmid = z1+(z2-z1)*0.5f;

  opening_t *zerogap = nullptr;

  for (opening_t *gap = InGaps; gap; gap = gap->next) {
    if (gap->range <= 0.0f) {
      if (!zerogap) zerogap = gap;
      continue;
    }

    const float f = gap->bottom;
    const float c = gap->top;

    if (z1 >= f && z2 <= c) return gap; // completely fits

    // can this gap contain us?
    if (z2-z1 <= c-f) {
      // this gap is big enough to contain us
      // if this gap's floor is higher than our feet, it is not interesting
      if (f > zmid) continue;
      // choose minimal distance to floor or ceiling
      const float dist = fabsf(z1-f);
      if (dist < fit_mindist) {
        fit_mindist = dist;
        fit_closest = gap;
      }
    } else {
      // not big enough
      const float dist = fabsf(z1-f);
      if (dist < nofit_mindist) {
        nofit_mindist = dist;
        nofit_closest = gap;
      }
    }
  }

  return (fit_closest ? fit_closest : nofit_closest ? nofit_closest : zerogap ? zerogap : InGaps);
}


//==========================================================================
//
//  VLevel::FindOpening
//
//  Find the best gap that the thing could fit in, given a certain Z
//  position (z1 is foot, z2 is head).  Assuming at least two gaps exist,
//  the best gap is chosen as follows:
//
//  1. if the thing fits in one of the gaps without moving vertically,
//     then choose that gap.
//
//  2. if there is only *one* gap which the thing could fit in, then
//     choose that gap.
//
//  3. if there is multiple gaps which the thing could fit in, choose
//     the gap whose floor is closest to the thing's current Z.
//
//  4. if there is no gaps which the thing could fit in, do the same.
//
//  Can return `nullptr`
//
//==========================================================================
opening_t *VLevel::FindOpening (opening_t *InGaps, float z1, float z2) {
  // check for trivial gaps
  if (!InGaps) return nullptr;
  if (!InGaps->next) return (InGaps->range > 0.0f ? InGaps : nullptr);

  int fit_num = 0;
  opening_t *fit_last = nullptr;

  opening_t *fit_closest = nullptr;
  float fit_mindist = 99999.0f;

  opening_t *nofit_closest = nullptr;
  float nofit_mindist = 99999.0f;

  // there are 2 or more gaps; now it gets interesting :-)
  for (opening_t *gap = InGaps; gap; gap = gap->next) {
    if (gap->range <= 0.0f) continue;
    const float f = gap->bottom;
    const float c = gap->top;

    if (z1 >= f && z2 <= c) return gap; // [1]

    const float dist = fabsf(z1-f);

    if (z2-z1 <= c-f) {
      // [2]
      ++fit_num;
      fit_last = gap;
      if (dist < fit_mindist) {
        // [3]
        fit_mindist = dist;
        fit_closest = gap;
      }
    } else {
      if (dist < nofit_mindist) {
        // [4]
        nofit_mindist = dist;
        nofit_closest = gap;
      }
    }
  }

  if (fit_num == 1) return fit_last;
  if (fit_num > 1) return fit_closest;
  return nofit_closest;
}



//**************************************************************************
//
//  VavoomC API
//
//**************************************************************************

//native final void GetSectorGapCoords (const GameObject::sector_t *sector, const ref TVec point, out float floorz, out float ceilz);
IMPLEMENT_FUNCTION(VLevel, GetSectorGapCoords) {
  sector_t *sector;
  TVec *point;
  float *floorz;
  float *ceilz;
  vobjGetParamSelf(sector, point, floorz, ceilz);
  Self->GetSectorGapCoords(sector, *point, *floorz, *ceilz);
}


//native final float GetSectorFloorPointZ (const sector_t *sector, const ref TVec point);
IMPLEMENT_FUNCTION(VLevel, GetSectorFloorPointZ) {
  sector_t *sector;
  TVec *point;
  vobjGetParamSelf(sector, point);
  RET_FLOAT(Self->GetSectorFloorPointZ(sector, *point));
}

//native static final GameObject::opening_t *FindOpening (const GameObject::opening_t *gaps, float z1, float z2);
IMPLEMENT_FUNCTION(VLevel, FindOpening) {
  opening_t *gaps;
  float z1, z2;
  vobjGetParam(gaps, z1, z2);
  RET_PTR(VLevel::FindOpening(gaps, z1, z2));
}

//native final GameObject::opening_t *LineOpenings (GameObject::line_t *linedef, const ref TVec point, optional int blockmask, optional bool do3dmidtex);
IMPLEMENT_FUNCTION(VLevel, LineOpenings) {
  line_t *linedef;
  TVec *point;
  VOptParamInt blockmask(SPF_NOBLOCKING);
  VOptParamBool do3dmidtex(false);
  vobjGetParamSelf(linedef, point, blockmask, do3dmidtex);
  RET_PTR(Self->LineOpenings(linedef, *point, blockmask, do3dmidtex));
}
