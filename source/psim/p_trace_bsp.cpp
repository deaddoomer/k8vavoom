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
//**  Copyright (C) 2018-2023 Ketmar Dark
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
#include "p_trace_internal.h"


static VCvarB dbg_bsp_trace_strict_flats("dbg_bsp_trace_strict_flats", false, "use strict checks for flats?", /*CVAR_Archive|*/CVAR_PreInit|CVAR_NoShadow);


//**************************************************************************
//
// BSP raycasting
//
//**************************************************************************

//==========================================================================
//
//  PlaneFlagsToLineFlags
//
//==========================================================================
/*
static inline VVA_CHECKRESULT VVA_OKUNUSED unsigned PlaneFlagsToLineFlags (unsigned planeblockflags) {
  return
    ((planeblockflags&SPF_NOBLOCKING) ? ((ML_BLOCKING|ML_BLOCKEVERYTHING)|(planeblockflags&SPF_PLAYER ? ML_BLOCKPLAYERS : 0u)|(planeblockflags&SPF_MONSTER ? ML_BLOCKMONSTERS : 0u)) : 0u)|
    ((planeblockflags&SPF_NOBLOCKSIGHT) ? ML_BLOCKSIGHT : 0u)|
    ((planeblockflags&SPF_NOBLOCKSHOOT) ? ML_BLOCKHITSCAN : 0u);
  //ML_BLOCK_FLOATERS      = 0x00040000u,
  //ML_BLOCKPROJECTILE     = 0x01000000u,
}
*/


struct TBSPTrace {
public:
  VLevel *Level;
  TVec Start;
  TVec End;
  TVec Delta;
  unsigned PlaneNoBlockFlags;
  // the following will be calculated from `PlaneNoBlockFlags`
  unsigned LineBlockFlags;
  // subsector we ended in (can be arbitrary if trace doesn't end in map boundaries)
  // valid only for `TraceLine()` call (i.e. BSP trace)
  subsector_t *EndSubsector;
  // the following fields are valid only if trace was failed (i.e. we hit something)
  TPlane HitPlane; // set both for a line and for a flat
  line_t *HitLine; // can be `nullptr` if we hit a floor/ceiling
  float HitTime; // will be 1.0f if nothing was hit
  TPlane LinePlane; // vertical plane for (Start,End), used only for line checks; may be undefined
  bool wantHitInfo;
};


//==========================================================================
//
//  CheckPlanes
//
//==========================================================================
static bool CheckPlanes (TBSPTrace &trace, sector_t *sec, float tmin, float tmax) {
  TPlane hpl;
  float otime;
  if (!VLevel::CheckPassPlanes(sec, trace.Start, trace.End, trace.PlaneNoBlockFlags, nullptr, nullptr, nullptr, &hpl, &otime)) {
    if (otime <= tmin || otime >= tmax) return true;
    // hit floor or ceiling
    trace.HitPlane = hpl;
    trace.HitTime = otime;
    return false;
  }
  return true;
}


//==========================================================================
//
//  BSPCheckLine
//
//  returns `true` if the line isn't crossed
//  returns `false` if the line blocked the ray
//
//==========================================================================
static bool BSPCheckLine (TBSPTrace &trace, line_t *line, float tmin, float tmax) {
  if (!line) return true; // ignore minisegs, they cannot block anything

  // allready checked other side?
  if (line->validcount == validcount) return true;
  line->validcount = validcount;

  // signed distances from the line points to the trace line plane
  float dot1 = trace.LinePlane.PointDistance(*line->v1);
  float dot2 = trace.LinePlane.PointDistance(*line->v2);

  // do not use multiplication to check: zero speedup, lost accuracy
  //if (dot1*dot2 >= 0) return true; // line isn't crossed
  if (dot1 < 0.0f && dot2 < 0.0f) return true; // didn't reached back side
  // if the line is parallel to the trace plane, ignore it
  if (dot1 >= 0.0f && dot2 >= 0.0f) return true; // didn't reached front side

  // signed distances from the trace points to the line plane
  dot1 = line->PointDistance(trace.Start);
  dot2 = line->PointDistance(trace.End);

  // do not use multiplication to check: zero speedup, lost accuracy
  //if (dot1*dot2 >= 0) return true; // line isn't crossed
  if (dot1 < 0.0f && dot2 < 0.0f) return true; // didn't reached back side
  // if the trace is parallel to the line plane, ignore it
  if (dot1 >= 0.0f && dot2 >= 0.0f) return true; // didn't reached front side

  const bool backside = (dot1 < 0.0f);

  polyobj_t *po = line->pobj();
  if (po && !po->Is3D()) po = nullptr;

  // crosses a two sided line
  sector_t *front = (backside ? line->backsector : line->frontsector);
  if (!front) return true; // just in case

  // intercept vector
  // no need to check if den == 0, because then planes are parallel
  // (they will never cross) or it's the same plane (also rejected)
  const float den = trace.Delta.dot(line->normal);
  if (den == 0.0f) return true; // ...but just in case
  const float num = line->dist-trace.Start.dot(line->normal);
  const float frac = num/den;

  if (frac <= 0.0f || frac >= 1.0f) return true;

  // if collision is not inside our time, unmark the line
  if (frac <= tmin || frac >= tmax) {
    line->validcount = 0;
    return true;
  }

  // if the hit is further than current hit time, do nothing
  if (frac >= trace.HitTime) return true;

  if (po) {
    const float pzbot = po->pofloor.minz;
    const float pztop = po->poceiling.maxz;

    if (trace.Start.z <= pzbot && trace.End.z <= pzbot) return true; // fully under
    if (trace.Start.z >= pztop && trace.End.z >= pztop) return true; // fully over

    const float hpz = trace.Start.z+frac*trace.Delta.z;

    // easy case: hit pobj?
    if (hpz >= pzbot && hpz <= pztop) {
      trace.HitPlane = *line;
      trace.HitLine = line;
      trace.HitTime = frac;
      return false;
    }

    // if we are entering the pobj, no need to check planes
    if (dot1 >= 0.0f) return true; // no hit

    // check polyobject planes
    TPlane pplane;
    const float ptime = VLevel::CheckPObjPassPlanes(po, trace.Start, trace.End, nullptr, nullptr, &pplane);
    if (ptime <= tmin || ptime >= frac) return true; // no hit

    // we have a winner!
    trace.HitPlane = pplane;
    trace.HitLine = nullptr;
    trace.HitTime = ptime;
    return false;
  }

  if (!CheckPlanes(trace, front, tmin, frac)) return false;

  if (!(line->flags&ML_TWOSIDED) || (line->flags&trace.LineBlockFlags)) {
    //trace.Flags |= TBSPTrace::SightEarlyOut;
  } else {
    if (!po && (line->flags&ML_TWOSIDED)) {
      // crossed a two sided line
      const TVec hitpoint = trace.Start+frac*trace.Delta;
      opening_t *open = trace.Level->LineOpenings(line, hitpoint, trace.PlaneNoBlockFlags&SPF_FLAG_MASK);
      if (dbg_bsp_trace_strict_flats) {
        while (open) {
          if (open->range > 0.0f && open->bottom < hitpoint.z && open->top > hitpoint.z) return true;
          open = open->next;
        }
      } else {
        while (open) {
          if (open->range > 0.0f && open->bottom <= hitpoint.z && open->top >= hitpoint.z) return true;
          open = open->next;
        }
      }
    }
  }

  // hit line
  trace.HitPlane = *line;
  trace.HitLine = line;
  trace.HitTime = frac;
  return false;
}


//==========================================================================
//
//  BSPCrossSubsector
//
//  Returns true if trace crosses the given subsector successfully.
//
//==========================================================================
static bool BSPCrossSubsector (TBSPTrace &trace, int num, float tmin, float tmax) {
  subsector_t *sub = &trace.Level->Subsectors[num];
  bool res = true;

  if (sub->HasPObjs()) {
    // check the polyobjects in the subsector first
    for (auto &&it : sub->PObjFirst()) {
      polyobj_t *pobj = it.pobj();
      line_t **polyLines = pobj->lines;
      for (int polyCount = pobj->numlines; polyCount--; ++polyLines) {
        if (!BSPCheckLine(trace, *polyLines, tmin, tmax)) {
          res = false;
          trace.EndSubsector = sub;
          if (!trace.wantHitInfo) return false;
        }
      }
    }
  }

  // check lines
  const seg_t *seg = &trace.Level->Segs[sub->firstline];
  for (int count = sub->numlines; count--; ++seg) {
    if (seg->linedef && !BSPCheckLine(trace, seg->linedef, tmin, tmax)) {
      res = false;
      trace.EndSubsector = sub;
      if (!trace.wantHitInfo) return false;
    }
  }

  return res;
}


//==========================================================================
//
//  CrossBSPNode
//
//  Returns true if trace crosses the given node successfully.
//
//==========================================================================
static bool CrossBSPNode (TBSPTrace &trace, int bspnum, float tmin, float tmax) {
  while (BSPIDX_IS_NON_LEAF(bspnum)) {
    const node_t *node = &trace.Level->Nodes[bspnum];
    //if (!node.containsBox2D(tr.swbbmin, tr.swbbmax)) return;

    // decide which side the start point is on
    int side = node->PointOnSide2(trace.Start);

    // cross the starting side
    if (!CrossBSPNode(trace, node->children[side&1], tmin, tmax)) return false;

    if (side != 2 && side == node->PointOnSide2(trace.End)) return true;

    // cross the ending side
    bspnum = node->children[(side&1)^1];
  }
  return BSPCrossSubsector(trace, BSPIDX_LEAF_SUBSECTOR(bspnum), tmin, tmax);

#if 0
  if (BSPIDX_IS_NON_LEAF(bspnum)) {
    const node_t *node = &trace.Level->Nodes[bspnum];
    // decide which side the start point is on
    #if 0
    const float denom = node->normal.dot(trace.Delta);
    const float dist = node->dist-node->normal.dot(trace.Start);
    unsigned nearIndex = (unsigned)(dist > 0.0f);
    // if denom is zero, ray runs parallel to the plane
    // in this case, just fall through to visit the near side (the one p lies on)
    if (denom != 0.0f) {
      const float t = dist/denom; // intersection time
      if (t > 0.0f && t <= tmax) {
        if (t >= tmin) {
          // visit near side first
          return
            !CrossBSPNode(trace, node->children[nearIndex], tmin, t)) ||
            // then visit the far side
            CrossBSPNode(trace, node->children[1u^nearIndex], t, tmax);
        } else {
          // 0 <= t < tmin, visit far side
          //nearIndex ^= 1u;
          return CrossBSPNode(trace, node->children[1u^nearIndex], t, tmax);
        }
      }
    }
    return CrossBSPNode(trace, node->children[nearIndex], tmin, tmax);

    #else

    /*
    const TVec dif = p1-p0;
    const float dv = normal.dot(dif);
    const float t = (fabsf(dv) > eps ? (dist-normal.dot(p0))/dv : 0.0f);
    return p0+(dif*t);
    */
    unsigned side = (unsigned)(trace.Start.dot(node->normal)-node->dist < 0.0f);
    if (!CrossBSPNode(trace, node->children[side], tmin, tmax)) return false;
    // then visit the far side
    return CrossBSPNode(trace, node->children[1u^side], tmin, tmax);
    #endif
    /*
    // if bit 1 is set (i.e. `(side&2) != 0`), the point lies on the plane
    const int side = node->PointOnSide2(trace.Start);
    // cross the starting side
    if (!CrossBSPNode(trace, node->children[side&1])) {
      // if on the plane, check other side
      if (side&2) (void)CrossBSPNode(trace, node->children[(side&1)^1]);
      // definitely blocked
      return false;
    }
    // the partition plane is crossed here
    // if not on the plane, and endpoint is on the same side, there's nothing more to do
    if (!(side&2) && side == node->PointOnSide2(trace.End)) return true; // the line doesn't touch the other side
    // cross the ending side
    return CrossBSPNode(trace, node->children[(side&1)^1]);
    */
  } else {
    return BSPCrossSubsector(trace, BSPIDX_LEAF_SUBSECTOR(bspnum), tmin, tmax);
  }
#endif
}


//==========================================================================
//
//  CheckStartingPObj
//
//  check if starting point is inside any 3d pobj
//  we have to do this to check pobj planes first
//
//  returns `true` if no obstacle was hit
//  sets `trace.LineEnd` if something was hit (and returns `false`)
//
//==========================================================================
static bool CheckStartingPObj (TBSPTrace &trace) {
  TVec hp;
  TPlane hpl;
  const float frc = trace.Level->CheckPObjPlanesPoint(trace.Start, trace.End, nullptr, &hp, nullptr, &hpl);
  if (frc < 0.0f || frc >= 1.0f) return true;
  trace.EndSubsector = trace.Level->PointInSubsector(trace.Start);
  trace.HitPlane = hpl;
  trace.HitTime = frc;
  return false;
}


//==========================================================================
//
//  FillLineTrace
//
//==========================================================================
static void FillLineTrace (linetrace_t &trace, const TBSPTrace &btr) noexcept {
  trace.Start = btr.Start;
  trace.End = btr.End;
  trace.EndSubsector = btr.EndSubsector;
  trace.HitPlane = btr.HitPlane;
  trace.HitLine = btr.HitLine;
  trace.HitTime = btr.HitTime;
}


//==========================================================================
//
//  VLevel::TraceLine
//
//  returns `true` if the line is not blocked
//  returns `false` if the line was blocked, and sets hit normal/point
//
//==========================================================================
bool VLevel::TraceLine (linetrace_t &trace, const TVec &Start, const TVec &End, unsigned PlaneNoBlockFlags, unsigned moreLineBlockFlags) {
  TBSPTrace btr;
  btr.Level = this;
  btr.Start = Start;
  btr.End = End;
  btr.Delta = End-Start;
  btr.PlaneNoBlockFlags = PlaneNoBlockFlags;
  btr.HitLine = nullptr;
  btr.HitTime = 1.0f;
  btr.wantHitInfo = !!(PlaneNoBlockFlags&SPF_HITINFO);
  PlaneNoBlockFlags &= ~SPF_HITINFO;

  if (PlaneNoBlockFlags&SPF_NOBLOCKING) {
    moreLineBlockFlags |= ML_BLOCKING|ML_BLOCKEVERYTHING;
    if (PlaneNoBlockFlags&SPF_PLAYER) moreLineBlockFlags |= ML_BLOCKPLAYERS;
    if (PlaneNoBlockFlags&SPF_MONSTER) moreLineBlockFlags |= ML_BLOCKMONSTERS;
    //ML_BLOCK_FLOATERS      = 0x00040000u,
    //ML_BLOCKPROJECTILE     = 0x01000000u,
  }
  if (PlaneNoBlockFlags&SPF_NOBLOCKSIGHT) moreLineBlockFlags |= ML_BLOCKSIGHT;
  if (PlaneNoBlockFlags&SPF_NOBLOCKSHOOT) moreLineBlockFlags |= ML_BLOCKHITSCAN;

  btr.LineBlockFlags = moreLineBlockFlags;

  //k8: HACK!
  if (btr.Delta.x == 0.0f && btr.Delta.y == 0.0f) {
    // this is vertical trace; end subsector is known
    btr.EndSubsector = PointInSubsector(End);
    //btr.Plane.SetPointNormal3D(Start, TVec(0.0f, 0.0f, (btr.Delta.z >= 0.0f ? 1.0f : -1.0f))); // arbitrary orientation
    // point cannot hit anything!
    if (fabsf(btr.Delta.z) <= 0.0001f) {
      // always succeed
      FillLineTrace(trace, btr);
      return true;
    }
    const bool pores = CheckStartingPObj(btr);
    const bool plres = CheckPlanes(btr, btr.EndSubsector->sector, 0.0f, 1.0f);
    FillLineTrace(trace, btr);
    return (pores && plres);
  } else {
    IncrementValidCount();
    btr.LinePlane.SetPointDirXY(Start, btr.Delta);
    // the head node is the last node output
    if (NumSubsectors > 1) {
      const bool pores = CheckStartingPObj(btr);
      const bool plres = CrossBSPNode(btr, NumNodes-1, 0.0f, 1.0f);
      FillLineTrace(trace, btr);
      return (pores && plres);
    } else if (NumSubsectors == 1) {
      btr.EndSubsector = &Subsectors[0];
      const bool pores = CheckStartingPObj(btr);
      const bool plres = BSPCrossSubsector(btr, 0, 0.0f, 1.0f);
      FillLineTrace(trace, btr);
      return (pores && plres);
    }
  }
  // we should never arrive here
  abort();
}


//==========================================================================
//
//  Script natives
//
//==========================================================================
IMPLEMENT_FUNCTION(VLevel, BSPTraceLine) {
  TVec Start, End;
  TVec *HitPoint;
  TVec *HitNormal;
  VOptParamInt noBlockFlags(SPF_NOBLOCKING);
  vobjGetParamSelf(Start, End, HitPoint, HitNormal, noBlockFlags);
  linetrace_t trace;
  bool res = Self->TraceLine(trace, Start, End, noBlockFlags);
  if (!res && (noBlockFlags.value&SPF_HITINFO)) {
    if (HitPoint) *HitPoint = Start+(End-Start)*trace.HitTime;
    if (HitNormal) {
      *HitNormal = trace.HitPlane.normal;
      if (trace.HitLine && trace.HitLine->PointOnSide(Start)) *HitNormal = -trace.HitPlane.normal;
    }
  } else {
    if (HitPoint) *HitPoint = End;
    if (HitNormal) *HitNormal = TVec(0.0f, 0.0f, 1.0f); // arbitrary
  }
  RET_BOOL(res);
}

IMPLEMENT_FUNCTION(VLevel, BSPTraceLineEx) {
  linetrace_t tracetmp;
  TVec Start, End;
  linetrace_t *tracep;
  VOptParamInt noBlockFlags(SPF_NOBLOCKING);
  vobjGetParamSelf(Start, End, tracep, noBlockFlags);
  if (!tracep) tracep = &tracetmp;
  RET_BOOL(Self->TraceLine(*tracep, Start, End, noBlockFlags));
}
