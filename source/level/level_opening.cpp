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
  op.elowfloor = op.efloor;
  op.next = nullptr;
}


//==========================================================================
//
//  VLevel::CutSolidRegion
//
//  the order is from the lowest height (lowest index) to
//  the highest height (highest index)
//
//==========================================================================
void VLevel::CutSolidRegion (TArray<opening_t> &dest, const float bottom, const float top,
                             const TSecPlaneRef &efloor, const TSecPlaneRef &eceiling)
{
  if (top < bottom) return;
  // i don't think that we need to ignore paper-thin regions ever

  // cache values
  opening_t *dop = dest.ptr();
  int dlen = dest.length();

  // check if it is not off-limits
  if (top <= dop[0].bottom) return;
  if (bottom >= dop[dlen-1].top) return;

  int cpos = 0;
  for (opening_t *cop = dop; cpos < dlen; ++cop, ++cpos) {
    // if the solid is lower than the current opening, we're done
    if (top < cop->bottom) return; // we're done here

    // if the solid is higher than the current opening, go on
    if (bottom > cop->top) continue; // cannot affect

    // out solid has intersection with the current opening

    // if the current opening is completely eaten by our solid, remove the opening, and go on
    if (bottom <= cop->bottom && top >= cop->top) {
      dest.removeAt(cpos);
      // update cache
      if ((--dlen) == 0) return;
      dop = dest.ptr();
      cop = dop+cpos;
      --cop;
      --cpos;
      continue;
    }

    // if the current opening is completely contains our solid, split the current opening, and we're done
    if (bottom >= cop->bottom && top <= cop->top) {
      vassert(bottom != cop->bottom || top != cop->top); // invariant
      if (bottom == cop->bottom) {
        // cut the bottom part with the solid ceiling
        vassert(top < cop->top);
        cop->bottom = top;
        cop->efloor = eceiling;
        // floor should point up
        if (cop->efloor.GetNormalZ() < 0.0f) cop->efloor.Flip();
        return;
      }
      if (top == cop->top) {
        // cut the top part with the solid floor
        vassert(bottom > cop->bottom);
        cop->top = bottom;
        cop->eceiling = efloor;
        // ceiling should point down
        if (cop->eceiling.GetNormalZ() > 0.0f) cop->eceiling.Flip();
        return;
      }
      // split the current opening to two parts
      opening_t tmp = *cop;
      dest.insert(cpos, tmp);
      // update cache
      cop = dest.ptr()+cpos;
      // fix the lower opening (cut it's top part with the solid floor)
      cop->top = bottom;
      cop->eceiling = efloor;
      cop->next = nullptr; // just in case
      // ceiling should point down
      if (cop->eceiling.GetNormalZ() > 0.0f) cop->eceiling.Flip();
      // fix the higher opening (cut it's bottom part with the solid ceiling)
      ++cop;
      cop->bottom = top;
      cop->efloor = eceiling;
      cop->next = nullptr; // just in case
      // floor should point up
      if (cop->efloor.GetNormalZ() < 0.0f) cop->efloor.Flip();
      return;
    }

    // one of the current opening parst should be cut, check which one
    if (bottom < cop->bottom) {
      // we need to cut the bottom part with solid ceiling
      vassert(top < cop->top);
      cop->bottom = top;
      cop->efloor = eceiling;
      // floor should point up
      if (cop->efloor.GetNormalZ() < 0.0f) cop->efloor.Flip();
    } else {
      // we need to cut the top part with solid floor
      vassert(top > cop->top);
      cop->top = bottom;
      cop->eceiling = efloor;
      // ceiling should point down
      if (cop->eceiling.GetNormalZ() > 0.0f) cop->eceiling.Flip();
    }
  }
}


//==========================================================================
//
//  VLevel::Insert3DMidtex
//
//==========================================================================
void VLevel::Cut3DMidtex (TArray<opening_t> &dest, const sector_t *sector, const line_t *linedef) {
  if (!linedef || !(linedef->flags&ML_3DMIDTEX)) return;
  // for 3dmidtex, create solid from midtex bottom to midtex top
  //   from floor to midtex bottom
  //   from midtex top to ceiling
  float fz, cz;
  if (!GetMidTexturePosition(linedef, 0, &fz, &cz)) return;
  if (fz >= cz) return; // k8: is this right?
  // cut it
  CutSolidRegion(dest, fz, cz, sector->eregions->efloor, sector->eregions->eceiling);
}


//==========================================================================
//
//  VLevel::BuildSectorOpenings
//
//  this function doesn't like regions with floors that has different flags
//
//==========================================================================
void VLevel::BuildSectorOpenings (const line_t *xldef, TArray<opening_t> &dest, sector_t *sector, const TVec point,
                                  unsigned NoBlockFlags, unsigned bsoFlags)
{
  dest.resetNoDtor();

  const bool usePoint = (bsoFlags&BSO_UsePoint);
  const bool do3DMidtex = (bsoFlags&BSO_Do3DMid);

  opening_t &op = dest.alloc();
  GetBaseSectorOpening(op, sector, point, usePoint);

  // if this sector has no 3d floors, we don't need to do any extra work
  if (!sector->Has3DFloors() /*|| !(sector->SectorFlags&sector_t::SF_Has3DMidTex)*/ && (!do3DMidtex || !xldef || !(xldef->flags&ML_3DMIDTEX))) {
    return;
  }

  // cut out all solid regions (i.e. split openings)
  // skip base region for now

  // build list of closed regions (it doesn't matter if region is non-solid, as long as it satisfies flag mask).

  // the order is from the lowest height (lowest index) to the highest height (highest index)
  for (const sec_region_t *reg = sector->eregions; reg; reg = reg->next) {
    if (reg->regflags&(sec_region_t::RF_BaseRegion|sec_region_t::RF_OnlyVisual|sec_region_t::RF_NonSolid)) continue; // base, pure visual region, or water, ignore it
    if (((reg->efloor.splane->flags|reg->eceiling.splane->flags)&NoBlockFlags) != 0) continue; // bad flags
    // hack: 3d floor with sky texture seems to be transparent in renderer
    /*
    if (forSurface && reg->extraline) {
      const side_t *sidedef = &Sides[reg->extraline->sidenum[0]];
      if (sidedef->MidTexture == skyflatnum) continue;
    }
    */

    // get border points
    float bottom, top;
    if (usePoint) {
      bottom = reg->efloor.GetPointZClamped(point);
      top = reg->eceiling.GetPointZClamped(point);
    } else {
      bottom = reg->efloor.splane->minz;
      top = reg->eceiling.splane->maxz;
    }

    CutSolidRegion(dest, bottom, top, reg->efloor, reg->eceiling);
    if (dest.length() == 0) return; // everything is eaten, nothing to do anymore
  }

  // add 3dmidtex, if there are any
  if (do3DMidtex) {
    if (xldef && (xldef->flags&ML_3DMIDTEX)) {
      Cut3DMidtex(dest, sector, xldef);
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
  }
}


//==========================================================================
//
//  VLevel::PointContentsRegion
//
//  FIXME: use this in `PointContents()`!
//
//==========================================================================
const sec_region_t *VLevel::PointContentsRegion (const sector_t *sector, const TVec &p) {
  if (!sector) sector = PointInSubsector(p)->sector;

  if (sector->heightsec && sector->heightsec->IsUnderwater() &&
      p.z <= sector->heightsec->floor.GetPointZClamped(p))
  {
    return sector->heightsec->eregions;
  }

  const sec_region_t *best = sector->eregions;

  if (!sector->Has3DFloors() || sector->IsUnderwater()) {
    return sector->eregions;
  }

  const float secfz = sector->floor.GetPointZClamped(p);
  const float seccz = sector->ceiling.GetPointZClamped(p);

  // out of sector's empty space?
  if (p.z < secfz || p.z > seccz) {
    return best;
  }

  // ignore solid regions, as we cannot be inside them legally
  float bestDist = +INFINITY; // minimum distance to region floor
  for (const sec_region_t *reg = sector->eregions->next; reg; reg = reg->next) {
    if (reg->regflags&(sec_region_t::RF_OnlyVisual|sec_region_t::RF_BaseRegion)) continue;
    // "sane" regions may represent Vavoom water
    if ((reg->regflags&(sec_region_t::RF_SaneRegion|sec_region_t::RF_NonSolid)) == sec_region_t::RF_SaneRegion) {
      if (!reg->params || (int)reg->params->contents <= 0) continue;
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
    const float rtopz = reg->eceiling.GetPointZ(p);
    const float rbotz = reg->efloor.GetPointZ(p);
    // ignore paper-thin regions
    if (rtopz <= rbotz) continue; // invalid, or paper-thin, ignore
    // check if point is inside, and for best ceiling dist
    if (p.z >= rbotz && p.z < rtopz) {
      const float fdist = rtopz-p.z;
      if (fdist < bestDist) {
        bestDist = fdist;
        best = reg;
      }
    }
  }

  return best;
}


//==========================================================================
//
//  VLevel::PointContents
//
//==========================================================================
int VLevel::PointContents (const sector_t *sector, const TVec &p, bool dbgDump) {
  if (!sector) sector = PointInSubsector(p)->sector;

  //dbgDump = true;
  if (sector->heightsec && sector->heightsec->IsUnderwater() &&
      p.z <= sector->heightsec->floor.GetPointZClamped(p))
  {
    if (dbgDump) GCon->Log(NAME_Debug, "SVP: case 0");
    return CONTENTS_BOOMWATER;
  }

  if (sector->IsUnderwater()) {
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
    float bestDist = +INFINITY; // minimum distance to region floor
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

  BuildSectorOpenings(nullptr, oplist_gfc, sector, point, SPF_NOBLOCKING, BSO_UsePoint);
  if (oplist_gfc.length() == 0) {
    // something is very wrong here, so use sector boundaries
    floor = sector->eregions->efloor;
    ceiling = sector->eregions->eceiling;
    return;
  }

  if (debugDump) { GCon->Logf(NAME_Debug, "=== ALL OPENINGS (z=%g; height=%g) ===", point.z, height); DumpOpPlanes(oplist_gfc); }

  { // link it
    const unsigned olen = (unsigned)oplist_gfc.length();
    opening_t *xop = oplist_gfc.ptr();
    if (olen > 1) {
      for (unsigned f = 0; f < olen-1; ++f) {
        xop[f].next = &xop[f+1];
      }
    }
    xop[olen-1].next = nullptr;
  }
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
      (!do3dmidtex || !((linedef->frontsector->SectorFlags|linedef->backsector->SectorFlags)&sector_t::SF_Has3DMidTex)))
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
      } else {
        dop->eceiling = fop.eceiling;
        dop->top = fop.top;
      }
    } else if (fop.top <= bop.top) {
      dop->eceiling = fop.eceiling;
      dop->top = fop.top;
    } else {
      dop->eceiling = bop.eceiling;
      dop->top = bop.top;
    }
    dop->range = dop->top-dop->bottom;
    dop->next = nullptr;

    return dop;
  }

  // has 3d floors at least on one side, do full-featured intersection calculation
  op0list_lop.resetNoDtor();
  op1list_lop.resetNoDtor();

  BuildSectorOpenings(linedef, op0list_lop, linedef->frontsector, point, NoBlockFlags, (do3dmidtex ? BSO_Do3DMid : BSO_NothingZero)|(usePoint ? BSO_UsePoint : BSO_NothingZero));
  if (op0list_lop.length() == 0) {
    // just in case: no front sector openings
    return nullptr;
  }

  BuildSectorOpenings(linedef, op1list_lop, linedef->backsector, point, NoBlockFlags, (do3dmidtex ? BSO_Do3DMid : BSO_NothingZero)|(usePoint ? BSO_UsePoint : BSO_NothingZero));
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
    } else {
      dest->eceiling = op1->eceiling;
      dest->top = op1->top;
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
//  can return `nullptr`, or zero-height gap
//
//==========================================================================
opening_t *VLevel::FindRelOpening (opening_t *InGaps, float zbot, float ztop) noexcept {
  // check for trivial gaps
  if (!InGaps) return nullptr;
  if (!InGaps->next) return InGaps;

  if (ztop < zbot) ztop = zbot; // just in case

  // there are 2 or more gaps; now it gets interesting :-)

  // as we cannot be lower or higher than the base sector, clamp values
  float gapminz = +FLT_MAX;
  float gapmaxz = -FLT_MAX;
  for (opening_t *gap = InGaps; gap; gap = gap->next) {
    if (gapminz > gap->bottom) gapminz = gap->bottom;
    if (gapmaxz < gap->top) gapmaxz = gap->top;
  }

  if (zbot > gapmaxz) {
    const float hgt = ztop-zbot;
    ztop = gapmaxz;
    zbot = ztop-hgt;
  }

  if (zbot < gapminz) {
    const float hgt = ztop-zbot;
    zbot = gapminz;
    ztop = zbot+hgt;
  }

  const float zheight = ztop-zbot;

  opening_t *fit_closest = nullptr;
  float fit_mindist = FLT_MAX;

  opening_t *nofit_closest = nullptr;
  float nofit_mindist = FLT_MAX;

  const float zmid = zbot+zheight*0.5f;

  for (opening_t *gap = InGaps; gap; gap = gap->next) {
    if (gap->range <= 0.0f) continue;

    const float gapbot = gap->bottom;
    const float gaptop = gap->top;

    if (zbot >= gapbot && ztop <= gaptop) return gap; // completely fits

    // can this gap contain us?
    if (zheight <= gaptop-gapbot) {
      // this gap is big enough to contain us
      // if this gap's bottom is higher than our "chest", it is not interesting
      if (gapbot > zmid) continue;
      // choose minimal distance to gap bottom
      const float dist = fabsf(zbot-gapbot);
      if (dist < fit_mindist) {
        fit_mindist = dist;
        fit_closest = gap;
      }
    } else {
      // not big enough, choose minimal distance to gap bottom
      const float dist = fabsf(zbot-gapbot);
      if (dist < nofit_mindist) {
        nofit_mindist = dist;
        nofit_closest = gap;
      }
    }
  }

  return (fit_closest ? fit_closest : nofit_closest ? nofit_closest : InGaps);
}


//==========================================================================
//
//  VLevel::FindOpening
//
//  find the best gap that the thing could fit in, given a certain Z
//  position (zbot is foot, ztop is head).
//  assuming at least two gaps exist, the best gap is chosen as follows:
//
//  1. if the thing fits in one of the gaps without moving vertically,
//     then choose that gap.
//
//  2. if there is only *one* gap which the thing could fit in, then
//     choose that gap.
//
//  3. if there are multiple gaps which the thing could fit in, choose
//     the gap whose floor is closest to the thing's current Z.
//
//  4. if there are no gaps which the thing could fit in, do the same.
//
//  can return `nullptr`
//
//==========================================================================
opening_t *VLevel::FindOpening (opening_t *InGaps, const float zbot, float ztop) {
  // check for trivial gaps
  if (!InGaps) return nullptr;
  if (!InGaps->next) return (InGaps->range > 0.0f ? InGaps : nullptr);

  if (ztop < zbot) ztop = zbot; // just in case
  const float zheight = ztop-zbot;

  int fit_num = 0;
  opening_t *fit_last = nullptr;

  opening_t *fit_closest = nullptr;
  float fit_mindist = FLT_MAX;

  opening_t *nofit_closest = nullptr;
  float nofit_mindist = FLT_MAX;

  // there are 2 or more gaps; now it gets interesting :-)
  for (opening_t *gap = InGaps; gap; gap = gap->next) {
    if (gap->range <= 0.0f) continue;
    const float gapbot = gap->bottom;
    const float gaptop = gap->top;

    if (zbot >= gapbot && ztop <= gaptop) return gap; // [1]

    const float dist = fabsf(zbot-gapbot);

    if (zheight <= gaptop-gapbot) {
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
  RET_PTR(Self->LineOpenings(linedef, *point, blockmask, do3dmidtex, true/*usePoint*/));
}

//native final int PointContents (const TVec p, optional sector_t *sector);
IMPLEMENT_FUNCTION(VLevel, PointContents) {
  TVec p;
  VOptParamPtr<sector_t> sector;
  vobjGetParamSelf(p, sector);
  if (!sector.specified) sector.value = Self->PointInSubsector(p)->sector;
  RET_INT(Self->PointContents(sector, p));
}
