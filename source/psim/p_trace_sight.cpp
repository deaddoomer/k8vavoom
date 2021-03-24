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
#include "p_trace_internal.h"


//**************************************************************************
//
// blockmap tracing
//
//**************************************************************************
static InterceptionList intercepts;


// ////////////////////////////////////////////////////////////////////////// //
struct SightTraceInfo {
  // the following should be set
  TVec Start;
  TVec End;
  const subsector_t *StartSubSector;
  const subsector_t *EndSubSector;
  VLevel *Level;
  unsigned LineBlockMask;
  vuint32 PlaneNoBlockFlags;
  // the following are working vars, and should not be set
  TVec Delta;
  TPlane Plane;
  bool Hit1S; // `true` means "hit one-sided wall"
  TVec LineStart;
  TVec LineEnd;

  inline SightTraceInfo () noexcept
    : LineBlockMask(0)
    , PlaneNoBlockFlags(0)
    , Delta(0.0f, 0.0f, 0.0f)
    , Hit1S(false)
    , LineStart(0.0f, 0.0f, 0.0f)
    , LineEnd(0.0f, 0.0f, 0.0f)
  {
    Plane.normal = TVec(0.0f, 0.0f, 1.0f);
    Plane.dist = 0.0f;
  }

  inline void setup (VLevel *alevel, const TVec &org, const TVec &dest, const subsector_t *sstart, const subsector_t *send) {
    Level = alevel;
    Start = org;
    End = dest;
    // use buggy vanilla algo here, because this is what used for world linking
    StartSubSector = (sstart ?: alevel->PointInSubsector_Buggy(org));
    EndSubSector = (send ?: alevel->PointInSubsector_Buggy(dest));
  }
};


//==========================================================================
//
//  SightCheckPlanes
//
//  returns `true` if no hit was detected
//  sets `trace.LineEnd` if hit was detected
//
//==========================================================================
static bool SightCheckPlanes (SightTraceInfo &trace, sector_t *sector, const bool ignoreSectorBounds) {
  if (!sector || sector->isAnyPObj()) return true;

  PlaneHitInfo phi(trace.LineStart, trace.LineEnd);

  unsigned flagmask = trace.PlaneNoBlockFlags;
  const bool checkFakeFloors = !(flagmask&SPF_IGNORE_FAKE_FLOORS);
  const bool checkSectorBounds = (!ignoreSectorBounds && !(flagmask&SPF_IGNORE_BASE_REGION));
  flagmask &= SPF_FLAG_MASK;

  if (checkFakeFloors) {
    // make fake floors and ceilings block view
    sector_t *hs = sector->heightsec;
    if (hs) {
      phi.update(hs->floor);
      phi.update(hs->ceiling);
    }
  }

  if (checkSectorBounds) {
    // check base sector planes
    phi.update(sector->floor);
    phi.update(sector->ceiling);
  }

  for (sec_region_t *reg = sector->eregions->next; reg; reg = reg->next) {
    if (reg->regflags&(sec_region_t::RF_BaseRegion|sec_region_t::RF_OnlyVisual|sec_region_t::RF_NonSolid)) continue;
    if ((reg->efloor.splane->flags&flagmask) == 0) {
      phi.update(reg->efloor);
    }
    if ((reg->eceiling.splane->flags&flagmask) == 0) {
      phi.update(reg->eceiling);
    }
  }

  if (phi.wasHit) trace.LineEnd = phi.getHitPoint();
  return !phi.wasHit;
}


//==========================================================================
//
//  SightCheckRegions
//
//==========================================================================
static bool SightCheckRegions (const sector_t *sec, const TVec point, const unsigned flagmask) {
  for (sec_region_t *reg = sec->eregions->next; reg; reg = reg->next) {
    if (reg->regflags&(sec_region_t::RF_BaseRegion|sec_region_t::RF_OnlyVisual|sec_region_t::RF_NonSolid)) continue;
    if (((reg->efloor.splane->flags|reg->eceiling.splane->flags)&flagmask) != 0) continue; // bad flags
    // get opening points
    const float fz = reg->efloor.GetPointZClamped(point);
    const float cz = reg->eceiling.GetPointZClamped(point);
    if (fz >= cz) continue; // ignore paper-thin regions
    // if we are inside it, we cannot pass
    if (point.z >= fz && point.z <= cz) return false;
  }
  return true;
}


//==========================================================================
//
//  SightCanPassOpening
//
//  ignore 3d midtex here (for now)
//
//==========================================================================
static bool SightCanPassOpening (const line_t *linedef, const TVec point, const unsigned flagmask) {
  if (linedef->sidenum[1] == -1 || !linedef->backsector) return false; // single sided line

  const sector_t *fsec = linedef->frontsector;
  const sector_t *bsec = linedef->backsector;

  if (!fsec || !bsec) return false;

  // check base region first
  const float ffz = fsec->floor.GetPointZClamped(point);
  const float fcz = fsec->ceiling.GetPointZClamped(point);
  const float bfz = bsec->floor.GetPointZClamped(point);
  const float bcz = bsec->ceiling.GetPointZClamped(point);

  const float pfz = max2(ffz, bfz); // highest floor
  const float pcz = min2(fcz, bcz); // lowest ceiling

  // closed sector?
  if (pfz >= pcz) return false;

  if (point.z <= pfz || point.z >= pcz) return false;

  // fast algo for two sectors without 3d floors
  if (!linedef->frontsector->Has3DFloors() &&
      !linedef->backsector->Has3DFloors())
  {
    // no 3d regions, we're ok
    return true;
  }

  // has 3d floors at least on one side, do full-featured search

  // front sector
  if (!SightCheckRegions(fsec, point, flagmask)) return false;
  // back sector
  if (!SightCheckRegions(bsec, point, flagmask)) return false;

  // done
  return true;
}


//==========================================================================
//
//  SightCheckLineHit
//
//  returns `false` if blocked
//
//==========================================================================
static bool SightCheckLineHit (SightTraceInfo &trace, const line_t *line, const float frac) {
  TVec hitpoint = trace.Start+frac*trace.Delta;
  trace.LineEnd = hitpoint;

  // check for 3d pobj
  polyobj_t *po = line->pobj();
  if (po && !po->Is3D()) po = nullptr;
  if (po) {
    // this must be 2-sided line, no need to check
    // check easy cases first (totally above or below)
    const float pz0 = po->pofloor.minz;
    const float pz1 = po->poceiling.maxz;

    //GCon->Logf(NAME_Debug, "pobj #%d, line #%d, plane check; pz=(%g,%g); lz=(%g,%g)", po->tag, (int)(ptrdiff_t)(line-&trace.Level->Lines[0]), pz0, pz1, trace.LineStart.z, hitpoint.z);
    //GCon->Logf(NAME_Debug, "  frac=%g; ls=(%g,%g,%g); hp=(%g,%g,%g)", frac, trace.LineStart.x, trace.LineStart.y, trace.LineStart.z, hitpoint.x, hitpoint.y, hitpoint.z);

    if (trace.LineStart.z > pz0 && hitpoint.z < pz1) {
      // inside pobj, cannot see anything (because it is solid)
      //GCon->Logf(NAME_Debug, "!!!! BLOCK");
      trace.Hit1S = true;
      return false;
    }

    // below?
    if (trace.LineStart.z <= pz0 && hitpoint.z <= pz0) return true; // cannot hit
    //GCon->Logf(NAME_Debug, "!!!! 000");
    // above?
    if (trace.LineStart.z >= pz1 && hitpoint.z >= pz1) return true; // cannot hit
    //GCon->Logf(NAME_Debug, "!!!! 001");

    // possible hit
    PlaneHitInfo phi(trace.LineStart, hitpoint);
    // implement proper blocking (so we could have transparent polyobjects)
    //unsigned flagmask = trace.PlaneNoBlockFlags;
    //flagmask &= SPF_FLAG_MASK;

    phi.update(po->pofloor);
    phi.update(po->poceiling);

    trace.LineStart = trace.LineEnd;

    if (phi.wasHit) {
      trace.LineEnd = phi.getHitPoint();
      return false;
    }

    // no hit
    return true;
  }

  //GCon->Logf(NAME_Debug, "linehit: line #%d; frac=%g; ls=(%g,%g,%g); hp=(%g,%g,%g)", (int)(ptrdiff_t)(line-&trace.Level->Lines[0]), frac, trace.LineStart.x, trace.LineStart.y, trace.LineStart.z, hitpoint.x, hitpoint.y, hitpoint.z);

  // normal line (not a 3d pobj part)
  const int s1 = line->PointOnSide2(trace.Start);
  sector_t *front = (s1 == 0 || s1 == 2 ? line->frontsector : line->backsector);
  if (!SightCheckPlanes(trace, front, (front == trace.StartSubSector->sector))) return false;

  trace.LineStart = trace.LineEnd;

  if (line->flags&ML_TWOSIDED) {
    // crosses a two sided line
    if (SightCanPassOpening(line, hitpoint, trace.PlaneNoBlockFlags&SPF_FLAG_MASK)) return true;
  } else {
    trace.Hit1S = true;
  }

  return false; // stop
}


//==========================================================================
//
//  SightCheckLine
//
//  return `true` if line is not crossed or put into intercept list
//  return `false` to stop checking due to blocking
//
//==========================================================================
static bool SightCheckLine (SightTraceInfo &trace, line_t *ld) {
  if (ld->validcount == validcount) return true;

  ld->validcount = validcount;

  // signed distances from the line points to the trace line plane
  const float ldot1 = trace.Plane.PointDistance(*ld->v1);
  const float ldot2 = trace.Plane.PointDistance(*ld->v2);

  // do not use multiplication to check: zero speedup, lost accuracy
  //if (dot1*dot2 >= 0) return true; // line isn't crossed
  if (ldot1 < 0.0f && ldot2 < 0.0f) return true; // didn't reached back side
  // if the line is parallel to the trace plane, ignore it
  if (ldot1 >= 0.0f && ldot2 >= 0.0f) return true; // didn't reached front side

  // signed distances from the trace points to the line plane
  const float dot1 = ld->PointDistance(trace.Start);
  const float dot2 = ld->PointDistance(trace.End);

  // if starting point is on a line, ignore this line
  if (fabsf(dot1) <= 0.1f) return true;

  // do not use multiplication to check: zero speedup, lost accuracy
  //if (dot1*dot2 >= 0) return true; // line isn't crossed
  if (dot1 < 0.0f && dot2 < 0.0f) return true; // didn't reached back side
  // if the trace is parallel to the line plane, ignore it
  if (dot1 >= 0.0f && dot2 >= 0.0f) return true; // didn't reached front side

  // if we hit an "early exit" line, don't bother doing anything more, the sight is blocked
  // yet this is not true for 3d pobj lines -- they have height!
  polyobj_t *po = ld->pobj();
  if (po && !po->Is3D()) po = nullptr;
  if (!ld->backsector || !(ld->flags&ML_TWOSIDED) || (!po && (ld->flags&trace.LineBlockMask))) {
    // note that we hit 1s line
    trace.Hit1S = true;
    return false;
  }

  // signed distance
  const float den = DotProduct(ld->normal, trace.Delta);
  if (fabsf(den) < 0.00001f) return true; // wtf?!
  const float num = ld->dist-DotProduct(trace.Start, ld->normal);
  const float frac = num/den;

  // store the line for later intersection testing
  intercept_t *icept = intercepts.insert(frac);
  icept->Flags = intercept_t::IF_IsALine; // just in case
  icept->line = ld;

  return true; // continue scanning
}


//==========================================================================
//
//  SightBlockLinesIterator
//
//==========================================================================
static bool SightBlockLinesIterator (SightTraceInfo &trace, int x, int y) {
  // it should never happen, but...
  //if (x < 0 || y < 0 || x >= trace.Level->BlockMapWidth || y >= trace.Level->BlockMapHeight) return false;

  int offset = y*trace.Level->BlockMapWidth+x;

  for (polyblock_t *polyLink = trace.Level->PolyBlockMap[offset]; polyLink; polyLink = polyLink->next) {
    polyobj_t *pobj = polyLink->polyobj;
    if (pobj && pobj->validcount != validcount) {
      pobj->validcount = validcount;
      for (auto &&sit : pobj->LineFirst()) {
        line_t *ld = sit.line();
        if (ld->validcount != validcount) {
          if (!SightCheckLine(trace, ld)) return false;
        }
      }
    }
  }

  offset = *(trace.Level->BlockMap+offset);

  for (const vint32 *list = trace.Level->BlockMapLump+offset+1; *list != -1; ++list) {
    if (!SightCheckLine(trace, &trace.Level->Lines[*list])) return false;
  }

  return true; // everything was checked
}


//==========================================================================
//
//  SightTraverseIntercepts
//
//  Returns true if the traverser function returns true for all lines
//
//==========================================================================
static bool SightTraverseIntercepts (SightTraceInfo &trace) {
  if (!intercepts.isEmpty()) {
    // go through in order
    const intercept_t *scan = intercepts.list;
    for (unsigned i = intercepts.count(); i--; ++scan) {
      if (!SightCheckLineHit(trace, scan->line, scan->frac)) return false; // don't bother going further
    }
  }
  trace.LineEnd = trace.End;
  return SightCheckPlanes(trace, trace.EndSubSector->sector, (trace.EndSubSector->sector == trace.StartSubSector->sector));
}


//==========================================================================
//
//  SightCheckStartingPObj
//
//  check if starting point is inside any 3d pobj
//  we have to do this to check pobj planes first
//
//  returns `true` if no obstacle was hit
//  sets `trace.LineEnd` if something was hit (and returns `false`)
//
//==========================================================================
static bool SightCheckStartingPObj (SightTraceInfo &trace) {
  TVec hp;
  const float frac = trace.Level->CheckPObjPlanesPoint(trace.Start, trace.End, trace.StartSubSector, &hp);
  if (frac >= 0.0f) {
    // yep, got it; we don't care about "best hit" here, only hit presence matters
    trace.LineEnd = hp;
    return false;
  }
  // no starting hit
  return true;
}


//==========================================================================
//
//  SightPathTraverse
//
//  traces a sight ray from `trace.Start` to `trace.End`, possibly
//  collecting intercepts
//
//  `trace.StartSubSector` and `trace.EndSubSector` must be set
//
//  returns `true` if no obstacle was hit
//  sets `trace.LineEnd` if something was hit
//
//==========================================================================
static bool SightPathTraverse (SightTraceInfo &trace) {
  VBlockMapWalker walker;

  intercepts.reset();

  trace.LineStart = trace.Start;
  trace.Delta = trace.End-trace.Start;
  trace.Hit1S = false;

  if (fabsf(trace.Delta.x) <= 1.0f && fabsf(trace.Delta.y) <= 1.0f) {
    // vertical trace; check starting sector planes and get out
    //FIXME: this is wrong for 3d pobjs!
    trace.Delta.x = trace.Delta.y = 0; // to simplify further checks
    trace.LineEnd = trace.End;
    // point cannot hit anything!
    if (fabsf(trace.Delta.z) <= 1.0f) {
      trace.Hit1S = true;
      trace.Delta.z = 0;
      return false;
    }
    if (!SightCheckStartingPObj(trace)) return false;
    return SightCheckPlanes(trace, trace.StartSubSector->sector, true);
  }

  if (!SightCheckStartingPObj(trace)) return false;

  if (walker.start(trace.Level, trace.Start.x, trace.Start.y, trace.End.x, trace.End.y)) {
    trace.Plane.SetPointDirXY(trace.Start, trace.Delta);
    trace.Level->IncrementValidCount();
    int mapx, mapy;
    while (walker.next(mapx, mapy)) {
      if (!SightBlockLinesIterator(trace, mapx, mapy)) {
        trace.Hit1S = true;
        return false; // early out
      }
    }
    // couldn't early out, so go through the sorted list
    return SightTraverseIntercepts(trace);
  }

  // out of map, see nothing
  return false;
}


//==========================================================================
//
//  SightPathTraverse2
//
//  rechecks intercepts with different ending z value
//
//==========================================================================
static bool SightPathTraverse2 (SightTraceInfo &trace) {
  trace.Delta = trace.End-trace.Start;
  trace.LineStart = trace.Start;
  if (fabsf(trace.Delta.x) <= 1.0f && fabsf(trace.Delta.y) <= 1.0f) {
    // vertical trace; check starting sector planes and get out
    //FIXME: this is wrong for 3d pobjs!
    trace.Delta.x = trace.Delta.y = 0; // to simplify further checks
    trace.LineEnd = trace.End;
    // point cannot hit anything!
    if (fabsf(trace.Delta.z) <= 1.0f) {
      trace.Hit1S = true;
      trace.Delta.z = 0;
      return false;
    }
    if (!SightCheckStartingPObj(trace)) return false;
    return SightCheckPlanes(trace, trace.StartSubSector->sector, true);
  }
  if (!SightCheckStartingPObj(trace)) return false;
  return SightTraverseIntercepts(trace);
}


//==========================================================================
//
//  isNotInsideBM
//
//  right edge is not included
//
//==========================================================================
static VVA_CHECKRESULT inline bool isNotInsideBM (const TVec &pos, const VLevel *level) {
  // horizontal check
  const int intx = (int)pos.x;
  const int intbx0 = (int)level->BlockMapOrgX;
  if (intx < intbx0 || intx >= intbx0+level->BlockMapWidth*MAPBLOCKUNITS) return true;
  // vertical checl
  const int inty = (int)pos.y;
  const int intby0 = (int)level->BlockMapOrgY;
  return (inty < intby0 || inty >= intby0+level->BlockMapHeight*MAPBLOCKUNITS);
}


//==========================================================================
//
//  VLevel::CastCanSee
//
//  doesn't check pvs or reject
//  if better sight is allowed, `orgdirRight` and `orgdirFwd` MUST be valid!
//
//==========================================================================
bool VLevel::CastCanSee (const subsector_t *SubSector, const TVec &org, float myheight, const TVec &orgdirFwd, const TVec &orgdirRight,
                         const TVec &dest, float radius, float height, bool skipBaseRegion, const subsector_t *DestSubSector,
                         bool allowBetterSight, bool ignoreBlockAll, bool ignoreFakeFloors)
{
  if (lengthSquared(org-dest) <= 2.0f*2.0f) return true; // arbitrary

  // if starting or ending point is out of blockmap bounds, don't bother tracing
  // we can get away with this, because nothing can see anything beyound the map extents
  if (isNotInsideBM(org, this)) return false;
  if (isNotInsideBM(dest, this)) return false;

  // it should not happen!
  if (SubSector->sector->isInnerPObj()) GCon->Logf(NAME_Error, "CastCanSee: source sector #%d is 3d pobj inner sector", (int)(ptrdiff_t)(SubSector->sector-&Sectors[0]));
  if (DestSubSector->sector->isInnerPObj()) GCon->Logf(NAME_Error, "CastCanSee: destination sector #%d is 3d pobj inner sector", (int)(ptrdiff_t)(DestSubSector->sector-&Sectors[0]));

  radius = max2(0.0f, radius);
  height = max2(0.0f, height);
  myheight = max2(0.0f, myheight);

  SightTraceInfo trace;
  trace.setup(this, org, dest, SubSector, DestSubSector);

  trace.PlaneNoBlockFlags =
    SPF_NOBLOCKSIGHT|
    (ignoreFakeFloors ? SPF_IGNORE_FAKE_FLOORS : 0u)|
    (skipBaseRegion ? SPF_IGNORE_BASE_REGION : 0u);

  trace.LineBlockMask =
    ML_BLOCKSIGHT|
    (ignoreBlockAll ? 0 : ML_BLOCKEVERYTHING)|
    (trace.PlaneNoBlockFlags&SPF_NOBLOCKSHOOT ? ML_BLOCKHITSCAN : 0u);

  if (!allowBetterSight || radius < 4.0f || height < 4.0f || myheight < 4.0f) {
    const TVec lookOrigin = org+TVec(0, 0, myheight*0.75f); // look from the eyes (roughly)
    trace.Start = lookOrigin;
    trace.End = dest;
    trace.End.z += height*0.85f; // roughly at the head
    const bool collectIntercepts = (trace.Delta.length2DSquared() <= 820.0f*820.0f); // arbitrary number
    if (SightPathTraverse(trace)) return true;
    if (trace.Hit1S || !collectIntercepts) return false; // hit one-sided wall, or too far, no need to do other checks
    // another fast check
    trace.End = dest;
    trace.End.z += height*0.5f;
    return SightPathTraverse2(trace);
  } else {
    const TVec lookOrigin = org+TVec(0, 0, myheight*0.86f); // look from the eyes (roughly)
    const float sidemult[3] = { 0.0f, -0.8f, 0.8f }; // side shift multiplier (by radius)
    const float ithmult = 0.92f; // destination height multiplier (0.5f is checked first)
    // check side looks
    for (unsigned myx = 0; myx < 3; ++myx) {
      // now look from eyes of t1 to some parts of t2
      trace.Start = lookOrigin+orgdirRight*(radius*sidemult[myx]);
      trace.End = dest;
      //trace.collectIntercepts = true;

      //DUNNO: if our point is not in a starting sector, fix it?

      // check middle
      trace.End.z += height*0.5f;
      if (SightPathTraverse(trace)) return true;
      if (trace.Hit1S || intercepts.isEmpty() == 0) continue;

      // check eyes (roughly)
      trace.End = dest;
      trace.End.z += height*ithmult;
      if (SightPathTraverse2(trace)) return true;
    }
  }

  return false;
}
