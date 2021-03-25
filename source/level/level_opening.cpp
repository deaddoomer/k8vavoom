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


// used in `BuildSectorOpenings()`
// sorry for this global
static TArray<opening_t> solids;


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
  if (!P_GetMidTexturePosition(linedef, 0, &cz, &fz)) return;
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
