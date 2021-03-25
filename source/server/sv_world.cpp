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
//**  Movement/collision utility functions, as used by function in
//**  p_map.c. BLOCKMAP Iterator functions, and some PIT_* functions to use
//**  for iteration.
//**
//**************************************************************************
#include "../gamedefs.h"
#include "sv_local.h"


//k8: this should be enough for everyone, lol
#define MAX_OPENINGS  (16384)
static opening_t openings[MAX_OPENINGS];

//#define VV_DUMP_OPENING_CREATION


//==========================================================================
//
//  GetLevelObject
//
//  have to do this, because this function can be called
//  both in server and in client
//
//==========================================================================
static inline VVA_CHECKRESULT VLevel *GetLevelObject () noexcept {
  return (GClLevel ? GClLevel : GLevel);
}


//==========================================================================
//
//  DumpRegion
//
//==========================================================================
static VVA_OKUNUSED void DumpRegion (const sec_region_t *inregion) {
  GCon->Logf(NAME_Debug, "  %p: floor=(%g,%g,%g:%g); (%g : %g), flags=0x%04x; ceil=(%g,%g,%g:%g); (%g : %g), flags=0x%04x; eline=%d; rflags=0x%02x",
    inregion,
    inregion->efloor.GetNormal().x,
    inregion->efloor.GetNormal().y,
    inregion->efloor.GetNormal().z,
    inregion->efloor.GetDist(),
    inregion->efloor.splane->minz, inregion->efloor.splane->maxz,
    inregion->efloor.splane->flags,
    inregion->eceiling.GetNormal().x,
    inregion->eceiling.GetNormal().y,
    inregion->eceiling.GetNormal().z,
    inregion->eceiling.GetDist(),
    inregion->eceiling.splane->minz, inregion->eceiling.splane->maxz,
    inregion->eceiling.splane->flags,
    (inregion->extraline ? 1 : 0),
    inregion->regflags);
}


//==========================================================================
//
//  DumpOpening
//
//==========================================================================
static VVA_OKUNUSED void DumpOpening (const opening_t *op) {
  GCon->Logf(NAME_Debug, "  %p: floor=%g (%g,%g,%g:%g); ceil=%g (%g,%g,%g:%g); lowfloor=%g; range=%g",
    op,
    op->bottom, op->efloor.GetNormal().x, op->efloor.GetNormal().y, op->efloor.GetNormal().z, op->efloor.GetDist(),
    op->top, op->eceiling.GetNormal().x, op->eceiling.GetNormal().y, op->eceiling.GetNormal().z, op->eceiling.GetDist(),
    op->lowfloor, op->range);
}


//==========================================================================
//
//  DumpOpPlanes
//
//==========================================================================
static VVA_OKUNUSED void DumpOpPlanes (TArray<opening_t> &list) {
  GCon->Logf(NAME_Debug, " ::: count=%d :::", list.length());
  for (int f = 0; f < list.length(); ++f) DumpOpening(&list[f]);
}


static TArray<opening_t> oplist_sop;

//==========================================================================
//
//  SV_SectorOpenings
//
//  used in surface creator
//
//==========================================================================
opening_t *SV_SectorOpenings (sector_t *sector, bool skipNonSolid) {
  vassert(sector);
  oplist_sop.resetNoDtor();
  GetLevelObject()->BuildSectorOpenings(nullptr, oplist_sop, sector, TVec::ZeroVector, 0/*nbflags*/, true/*linkList*/, false/*usePoint*/, skipNonSolid, true/*forSurface*/);
  //vassert(oplist_sop.length() > 0);
  if (oplist_sop.length() == 0) {
    //k8: why, it is ok to have zero openings (it seems)
    //    i've seen this in Hurt.wad, around (7856 -2146)
    //    just take the armor, and wait until the pool will start filling with blood
    //    this seems to be a bug, but meh... at least there is no reason to crash.
    #ifdef CLIENT
    //GCon->Logf(NAME_Warning, "SV_SectorOpenings: zero openings for sector #%d", (int)(ptrdiff_t)(sector-&GClLevel->Sectors[0]));
    #endif
    return nullptr;
  }
  return oplist_sop.ptr();
}


static TArray<opening_t> oplist_sop2;


//==========================================================================
//
//  SV_SectorOpenings2
//
//  used in surface creator
//
//==========================================================================
opening_t *SV_SectorOpenings2 (sector_t *sector, bool skipNonSolid) {
  /*
  vassert(sector);
  oplist_sop2.resetNoDtor();
  GetLevelObject()->BuildSectorOpenings(nullptr, oplist_sop2, sector, TVec::ZeroVector, 0, false/ *linkList* /, false/ *usePoint* /, skipNonSolid);
  vassert(oplist_sop2.length() > 0);
  if (oplist_sop2.length() > MAX_OPENINGS) Host_Error("too many sector openings");
  opening_t *dest = openings;
  opening_t *src = oplist_sop2.ptr();
  for (int count = oplist_sop2.length(); count--; ++dest, ++src) {
    *dest = *src;
    dest->next = dest+1;
  }
  openings[oplist_sop2.length()-1].next = nullptr;
  return openings;
  */
  vassert(sector);
  oplist_sop2.resetNoDtor();
  GetLevelObject()->BuildSectorOpenings(nullptr, oplist_sop2, sector, TVec::ZeroVector, 0, true/*linkList*/, false/*usePoint*/, skipNonSolid, true/*forSurface*/);
  vassert(oplist_sop2.length() > 0);
  return oplist_sop2.ptr();
}


static TArray<opening_t> op0list_lop;
static TArray<opening_t> op1list_lop;


//==========================================================================
//
//  SV_LineOpenings
//
//  sets opentop and openbottom to the window through a two sided line
//
//==========================================================================
opening_t *SV_LineOpenings (const line_t *linedef, const TVec point, unsigned NoBlockFlags, bool do3dmidtex, bool usePoint) {
  if (linedef->sidenum[1] == -1 || !linedef->backsector) return nullptr; // single sided line

  NoBlockFlags &= (SPF_MAX_FLAG-1u);

  // fast algo for two sectors without 3d floors
  if (!linedef->frontsector->Has3DFloors() &&
      !linedef->backsector->Has3DFloors() &&
      //!thisIs3DMidTex &&
      !((linedef->frontsector->SectorFlags|linedef->backsector->SectorFlags)&sector_t::SF_Has3DMidTex))
  {
    opening_t fop, bop;
    GetLevelObject()->GetBaseSectorOpening(fop, linedef->frontsector, point, usePoint);
    GetLevelObject()->GetBaseSectorOpening(bop, linedef->backsector, point, usePoint);
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

  GetLevelObject()->BuildSectorOpenings(linedef, op0list_lop, linedef->frontsector, point, NoBlockFlags, false/*linkList*/, usePoint);
  if (op0list_lop.length() == 0) {
    // just in case: no front sector openings
    return nullptr;
  }

  GetLevelObject()->BuildSectorOpenings(linedef, op1list_lop, linedef->backsector, point, NoBlockFlags, false/*linkList*/, usePoint);
  if (op1list_lop.length() == 0) {
    // just in case: no back sector openings
    return nullptr;
  }

#ifdef VV_DUMP_OPENING_CREATION
  GCon->Logf(NAME_Debug, "*** line: %p (0x%02x) ***", linedef, NoBlockFlags);
  GCon->Log(NAME_Debug, "::: before :::"); DumpOpPlanes(op0list_lop); DumpOpPlanes(op1list_lop);
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
      DumpOpening(op0);
      DumpOpening(op1);
#endif
      ++op0; --op0left;
      continue;
    }
    // if op1 is below op0, skip op1
    if (op1->top < op0->bottom) {
#ifdef VV_DUMP_OPENING_CREATION
      GCon->Log(NAME_Debug, " +++ SKIP op1 (dump: op0, op1) +++");
      DumpOpening(op0);
      DumpOpening(op1);
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
    DumpOpening(op0);
    DumpOpening(op1);
    DumpOpening(dest);
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
    DumpOpening(xop);
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
//  SV_FindOpening
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
opening_t *SV_FindOpening (opening_t *InGaps, float z1, float z2) {
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


//==========================================================================
//
//  SV_FindRelOpening
//
//  used in sector movement, so it tries hard to not leave current opening
//
//==========================================================================
opening_t *SV_FindRelOpening (opening_t *InGaps, float z1, float z2) {
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


static TArray<opening_t> oplist_gfc;


//==========================================================================
//
//  SV_FindGapFloorCeiling
//
//  find region for thing, and return best floor/ceiling
//  `p.z` is bottom
//
//  this is used mostly in sector movement, so we should try hard to stay
//  inside our current gap, so we won't teleport up/down from lifts, and
//  from crushers
//
//==========================================================================
void SV_FindGapFloorCeiling (sector_t *sector, const TVec point, float height, TSecPlaneRef &floor, TSecPlaneRef &ceiling, bool debugDump) {
  /*
  if (debugDump) {
    GCon->Logf("=== ALL OPENINGS: sector %p ===", sector);
    for (const sec_region_t *reg = sector->eregions; reg; reg = reg->next) DumpRegion(reg);
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

  GetLevelObject()->BuildSectorOpenings(nullptr, oplist_gfc, sector, point, SPF_NOBLOCKING, true/*linkList*/, true/*usePoint*/);
  if (oplist_gfc.length() == 0) {
    // something is very wrong here, so use sector boundaries
    floor = sector->eregions->efloor;
    ceiling = sector->eregions->eceiling;
    return;
  }

  if (debugDump) { GCon->Logf(NAME_Debug, "=== ALL OPENINGS (z=%g; height=%g) ===", point.z, height); DumpOpPlanes(oplist_gfc); }

#if 1
  opening_t *opres = SV_FindRelOpening(oplist_gfc.ptr(), point.z, point.z+height);
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
//  SV_GetSectorGapCoords
//
//==========================================================================
void SV_GetSectorGapCoords (sector_t *sector, const TVec point, float &floorz, float &ceilz) {
  if (!sector) { floorz = 0.0f; ceilz = 0.0f; return; }
  if (!sector->Has3DFloors()) {
    floorz = sector->floor.GetPointZClamped(point);
    ceilz = sector->ceiling.GetPointZClamped(point);
    return;
  }
  TSecPlaneRef f, c;
  SV_FindGapFloorCeiling(sector, point, 0, f, c);
  floorz = f.GetPointZClamped(point);
  ceilz = c.GetPointZClamped(point);
}


//==========================================================================
//
//  SV_PointContents
//
//==========================================================================
int SV_PointContents (sector_t *sector, const TVec &p, bool dbgDump) {
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
//  SV_GetNextRegion
//
//  the one that is higher
//  valid only if `srcreg` is solid and insane
//
//==========================================================================
sec_region_t *SV_GetNextRegion (sector_t *sector, sec_region_t *srcreg) {
  vassert(sector);
  if (!srcreg || !sector->eregions->next) return sector->eregions;
  // get distance to ceiling
  // we want the best sector that is higher
  const float srcrtopz = srcreg->eceiling.GetRealDist();
  float bestdist = 99999.0f;
  sec_region_t *bestreg = sector->eregions;
  for (sec_region_t *reg = sector->eregions->next; reg; reg = reg->next) {
    if (reg == srcreg || (reg->regflags&(sec_region_t::RF_NonSolid|sec_region_t::RF_OnlyVisual|sec_region_t::RF_SaneRegion|sec_region_t::RF_BaseRegion)) != 0) continue;
    const float rtopz = reg->eceiling.GetRealDist();
    const float rbotz = reg->efloor.GetRealDist();
    // ignore paper-thin regions
    if (rtopz <= rbotz) continue; // invalid, or paper-thin, ignore
    const float dist = srcrtopz-rbotz;
    if (dist <= 0.0f) continue; // too low
    if (dist < bestdist) {
      bestdist = dist;
      bestreg = reg;
    }
  }
  return bestreg;
}


//==========================================================================
//
//  SV_GetLowestSolidPointZ
//
//  FIXME: this ignores 3d pobjs!
//
//==========================================================================
float SV_GetLowestSolidPointZ (sector_t *sector, const TVec &point, bool ignore3dFloors) {
  if (!sector) return 0.0f; // idc
  if (ignore3dFloors || !sector->Has3DFloors()) return sector->floor.GetPointZClamped(point); // cannot be lower than this
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
}


//==========================================================================
//
//  SV_GetHighestSolidPointZ
//
//==========================================================================
/*
float SV_GetHighestSolidPointZ (sector_t *sector, const TVec &point, bool ignore3dFloors) {
  if (!sector) return 0.0f; // idc
  if (ignore3dFloors || !sector->Has3DFloors()) return sector->ceiling.GetPointZClamped(point); // cannot be higher than this
  // find best solid 3d floor
  vassert(sector->eregions);
  float bestz = sector->ceiling.GetPointZClamped(point);
  float bestdist = fabsf(bestz-point.z);
  for (sec_region_t *reg = sector->eregions; reg; reg = reg->next) {
    if ((reg->regflags&(sec_region_t::RF_NonSolid|sec_region_t::RF_OnlyVisual|sec_region_t::RF_BaseRegion)) != 0) continue;
    // ceiling is at the top
    const float fz = reg->efloor.GetPointZClamped(point);
    const float dist = fabsf(fz-point.z);
    if (dist < bestdist) {
      bestz = fz;
      bestdist = dist;
    }
  }
  return bestz;
}
*/


//**************************************************************************
//
//  SECTOR HEIGHT CHANGING
//
//  After modifying a sectors floor or ceiling height, call this routine to
//  adjust the positions of all things that touch the sector.
//  If anything doesn't fit anymore, true will be returned. If crunch is
//  true, they will take damage as they are being crushed.
//  If Crunch is false, you should set the sector height back the way it was
//  and call P_ChangeSector again to undo the changes.
//
//**************************************************************************

//==========================================================================
//
//  VLevel::ChangeOneSectorInternal
//
//  resets all caches and such
//  used to "fake open" doors and move lifts in bot pathfinder
//
//  doesn't move things (so you *HAVE* to restore sector heights)
//
//==========================================================================
void VLevel::ChangeOneSectorInternal (sector_t *sector) {
  if (!sector) return;
  CalcSecMinMaxs(sector);
}


//==========================================================================
//
//  VLevel::nextVisitedCount
//
//==========================================================================
vint32 VLevel::nextVisitedCount () {
  if (tsVisitedCount == MAX_VINT32) {
    tsVisitedCount = 1;
    for (auto &&sector : allSectors()) {
      for (msecnode_t *n = sector.TouchingThingList; n; n = n->SNext) n->Visited = 0;
    }
  } else {
    ++tsVisitedCount;
  }
  return tsVisitedCount;
}

IMPLEMENT_FUNCTION(VLevel, GetNextVisitedCount) {
  vobjGetParamSelf();
  RET_INT(Self->nextVisitedCount());
}


//==========================================================================
//
//  VLevel::ChangeSectorInternal
//
//  jff 3/19/98 added to just check monsters on the periphery of a moving
//  sector instead of all in bounding box of the sector. Both more accurate
//  and faster.
//
//  `-1` for `crunch` means "ignore stuck mobj"
//
//  returns `true` if movement was blocked
//
//==========================================================================
bool VLevel::ChangeSectorInternal (sector_t *sector, int crunch) {
  vassert(sector);
  const int secnum = (int)(ptrdiff_t)(sector-Sectors);
  if ((csTouched[secnum]&0x7fffffffU) == csTouchCount) return !!(csTouched[secnum]&0x80000000U);
  csTouched[secnum] = csTouchCount;

  if (sector->isOriginalPObj()) return false; // just in case

  // do not recalc bounds for inner 3d pobj sectors
  if (!sector->isInnerPObj()) CalcSecMinMaxs(sector);

  bool ret = false;

  // killough 4/4/98: scan list front-to-back until empty or exhausted,
  // restarting from beginning after each thing is processed. Avoids
  // crashes, and is sure to examine all things in the sector, and only
  // the things which are in the sector, until a steady-state is reached.
  // Things can arbitrarily be inserted and removed and it won't mess up.
  //
  // killough 4/7/98: simplified to avoid using complicated counter
  //
  // ketmar: mostly rewritten, and reintroduced the counter

  // mark all things invalid
  //for (msecnode_t *n = sector->TouchingThingList; n; n = n->SNext) n->Visited = 0;

  if (sector->TouchingThingList) {
    const int visCount = nextVisitedCount();
    msecnode_t *n = nullptr;
    do {
      // go through list
      for (n = sector->TouchingThingList; n; n = n->SNext) {
        if (n->Visited != visCount) {
          // unprocessed thing found, mark thing as processed
          n->Visited = visCount;
          if (n->Thing->IsGoingToDie()) continue;
          // process it
          //const TVec oldOrg = n->Thing->Origin;
          //GCon->Logf("CHECKING Thing '%s'(%u) hit (zpos:%g) (sector #%d)", *n->Thing->GetClass()->GetFullName(), n->Thing->GetUniqueId(), n->Thing->Origin.z, (int)(ptrdiff_t)(sector-&Sectors[0]));
          if (!n->Thing->eventSectorChanged(crunch)) {
            // doesn't fit, keep checking (crush other things)
            //GCon->Logf("Thing '%s'(%u) hit (old: %g; new: %g) (sector #%d)", *n->Thing->GetClass()->GetFullName(), n->Thing->GetUniqueId(), oldOrg.z, n->Thing->Origin.z, (int)(ptrdiff_t)(sector-&Sectors[0]));
            if (ret) csTouched[secnum] |= 0x80000000U;
            ret = true;
          }
          // exit and start over
          break;
        }
      }
    } while (n); // repeat from scratch until all things left are marked valid
  }

  // this checks the case when 3d control sector moved (3d lifts)
  for (auto it = IterControlLinks(sector); !it.isEmpty(); it.next()) {
    //GCon->Logf("*** src=%d; dest=%d", secnum, it.getDestSectorIndex());
    bool r2 = ChangeSectorInternal(it.getDestSector(), crunch);
    if (r2) { ret = true; csTouched[secnum] |= 0x80000000U; }
  }

  return ret;
}


//==========================================================================
//
//  VLevel::ChangeSector
//
//  `-1` for `crunch` means "ignore stuck mobj"
//
//  returns `true` if movement was blocked
//
//==========================================================================
bool VLevel::ChangeSector (sector_t *sector, int crunch) {
  if (!sector || NumSectors == 0) return false; // just in case
  if (sector->isOriginalPObj()) return false; // this can be done accidentally, and it doesn't do anything anyway
  if (!csTouched) {
    csTouched = (vuint32 *)Z_Calloc(NumSectors*sizeof(csTouched[0]));
    csTouchCount = 0;
  }
  if (++csTouchCount == 0x80000000U) {
    memset(csTouched, 0, NumSectors*sizeof(csTouched[0]));
    csTouchCount = 1;
  }
  IncrementSZValidCount();
  return ChangeSectorInternal(sector, crunch);
}
