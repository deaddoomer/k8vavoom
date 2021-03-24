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

//FIXME: this is almost the same code as sight checking
//FIXME: factor out common parts


//**************************************************************************
//
// blockmap light tracing
//
//**************************************************************************
static inline __attribute__((const)) float TextureSScale (const VTexture *pic) { return pic->SScale; }
static inline __attribute__((const)) float TextureTScale (const VTexture *pic) { return pic->TScale; }
static inline __attribute__((const)) float TextureOffsetSScale (const VTexture *pic) { return (pic->bWorldPanning ? pic->SScale : 1.0f); }
static inline __attribute__((const)) float TextureOffsetTScale (const VTexture *pic) { return (pic->bWorldPanning ? pic->TScale : 1.0f); }
static inline __attribute__((const)) float DivByScale (float v, float scale) { return (scale > 0 ? v/scale : v); }


// sadly, we have to collect lines, because sector plane checks depends on this
static intercept_t *intercepts = nullptr;
static unsigned interAllocated = 0;
static unsigned interUsed = 0;


//==========================================================================
//
//  ResetIntercepts
//
//==========================================================================
static void ResetIntercepts () {
  interUsed = 0;
}


//==========================================================================
//
//  EnsureFreeIntercept
//
//==========================================================================
static inline void EnsureFreeIntercept () {
  if (interAllocated <= interUsed) {
    unsigned oldAlloc = interAllocated;
    interAllocated = ((interUsed+4)|0xfffu)+1;
    intercepts = (intercept_t *)Z_Realloc(intercepts, interAllocated*sizeof(intercept_t));
    if (oldAlloc) GCon->Logf(NAME_Debug, "more interceptions allocated; interUsed=%u; allocated=%u (old=%u)", interUsed, interAllocated, oldAlloc);
  }
}


// ////////////////////////////////////////////////////////////////////////// //
struct LightTraceInfo {
  // the following should be set
  TVec Start;
  TVec End;
  const subsector_t *StartSubSector;
  const subsector_t *EndSubSector;
  VLevel *Level;
  bool textureCheck;
  // the following are working vars, and should not be set
  TVec Delta; // (End-Start)
  TPlane Plane;
  TVec LineStart;
  TVec LineEnd;

  inline void setup (VLevel *alevel, const TVec &org, const TVec &dest, const subsector_t *sstart, const subsector_t *send) {
    Level = alevel;
    Start = org;
    End = dest;
    StartSubSector = (sstart ?: alevel->PointInSubsector(org));
    EndSubSector = (send ?: alevel->PointInSubsector(dest));
  }
};


//==========================================================================
//
//  LightCheckPlanes
//
//  returns `true` if no hit was detected
//  sets `trace.LineEnd` if hit was detected
//
//==========================================================================
static bool LightCheckPlanes (LightTraceInfo &trace, sector_t *sector) {
  if (!sector || sector->isAnyPObj()) return true;

  PlaneHitInfo phi(trace.LineStart, trace.LineEnd);

  // make fake floors and ceilings block view
  sector_t *hs = sector->heightsec;
  if (hs) {
    if (GTextureManager.IsSightBlocking(hs->floor.pic)) phi.update(hs->floor);
    if (GTextureManager.IsSightBlocking(hs->ceiling.pic)) phi.update(hs->ceiling);
  }

  phi.update(sector->floor);
  phi.update(sector->ceiling);

  for (sec_region_t *reg = sector->eregions->next; reg; reg = reg->next) {
    if (reg->regflags&(sec_region_t::RF_BaseRegion|sec_region_t::RF_OnlyVisual|sec_region_t::RF_NonSolid)) continue;
    if ((reg->efloor.splane->flags&sec_region_t::RF_SkipFloorSurf) == 0) {
      if (GTextureManager.IsSightBlocking(reg->efloor.splane->pic)) {
        phi.update(reg->efloor);
      }
    }
    if ((reg->efloor.splane->flags&sec_region_t::RF_SkipCeilSurf) == 0) {
      if (GTextureManager.IsSightBlocking(reg->eceiling.splane->pic)) {
        phi.update(reg->eceiling);
      }
    }
  }

  if (phi.wasHit) trace.LineEnd = phi.getHitPoint();
  return !phi.wasHit;
}


//==========================================================================
//
//  LightCheckRegions
//
//==========================================================================
static bool LightCheckRegions (const sector_t *sec, const TVec point) {
  for (sec_region_t *reg = sec->eregions->next; reg; reg = reg->next) {
    if (reg->regflags&(sec_region_t::RF_BaseRegion|sec_region_t::RF_OnlyVisual|sec_region_t::RF_NonSolid)) continue;
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
//  LightCanPassOpening
//
//  ignore 3d midtex here (for now)
//
//==========================================================================
static bool LightCanPassOpening (const line_t *linedef, const TVec point) {
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
  // TODO: check for transparent doors here
  if (pfz >= pcz) return false;

  if (point.z <= pfz || point.z >= pcz) return false;

  // fast algo for two sectors without 3d floors
  if (!fsec->Has3DFloors() && !bsec->Has3DFloors()) {
    // no 3d regions, we're ok
    return true;
  }

  // has 3d floors at least on one side, do full-featured search

  // front sector
  if (!LightCheckRegions(fsec, point)) return false;
  // back sector
  if (!LightCheckRegions(bsec, point)) return false;

  // done
  return true;
}


//==========================================================================
//
//  LightCheck2SLinePass
//
//==========================================================================
static bool LightCheck2SLinePass (LightTraceInfo &trace, const line_t *line, const TVec hitpoint) {
  vassert(line->flags&ML_TWOSIDED);

  // check for 3d pobj
  polyobj_t *po = line->pobj();
  if (po && !po->Is3D()) po = nullptr;
  if (po) {
    // this must be 2-sided line, no need to check
    // check easy cases first (totally above or below)
    const float pz0 = po->pofloor.minz;
    const float pz1 = po->poceiling.maxz;

    if (trace.LineStart.z > pz0 && hitpoint.z < pz1) {
      // inside pobj, cannot see anything (because it is solid)
      return false;
    }

    // below?
    if (trace.LineStart.z <= pz0 && hitpoint.z <= pz0) return true; // cannot hit
    // above?
    if (trace.LineStart.z >= pz1 && hitpoint.z >= pz1) return true; // cannot hit

    // possible hit
    PlaneHitInfo phi(trace.LineStart, hitpoint);

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

  //TVec hitpoint = trace.Start+frac*trace.Delta;

  if (!LightCanPassOpening(line, hitpoint)) return false;

  if (line->alpha < 1.0f || (line->flags&ML_ADDITIVE)) return true;
  if (!trace.textureCheck) return true;

  // check texture
  int sidenum = line->PointOnSide2(trace.Start);
  if (sidenum == 2 || line->sidenum[sidenum] == -1) return true; // on a line

  const side_t *sidedef = &trace.Level->Sides[line->sidenum[sidenum]];
  VTexture *MTex = GTextureManager(sidedef->MidTexture);
  if (!MTex || MTex->Type == TEXTYPE_Null) return true;

  const bool wrapped = ((line->flags&ML_WRAP_MIDTEX)|(sidedef->Flags&SDF_WRAPMIDTEX));
  if (wrapped && !MTex->isSeeThrough()) return true;

  const TVec taxis = TVec(0, 0, -1)*(TextureTScale(MTex)*sidedef->Mid.ScaleY);
  float toffs;

  float z_org; // texture top
  if (line->flags&ML_DONTPEGBOTTOM) {
    // bottom of texture at bottom
    const float texh = DivByScale(MTex->GetScaledHeight(), sidedef->Mid.ScaleY);
    z_org = max2(line->frontsector->floor.TexZ, line->backsector->floor.TexZ)+texh;
  } else {
    // top of texture at top
    z_org = min2(line->frontsector->ceiling.TexZ, line->backsector->ceiling.TexZ);
  }

  if (wrapped) {
    // it is wrapped, so just slide it
    toffs = sidedef->Mid.RowOffset*TextureOffsetTScale(MTex);
  } else {
    // move origin up/down, as this texture is not wrapped
    z_org += sidedef->Mid.RowOffset*DivByScale(TextureOffsetTScale(MTex), sidedef->Mid.ScaleY);
    // offset is done by origin, so we don't need to offset texture
    toffs = 0.0f;
  }
  toffs += z_org*(TextureTScale(MTex)*sidedef->Mid.ScaleY);

  const int texelT = (int)(DotProduct(hitpoint, taxis)+toffs); // /MTex->GetHeight();
  // check for wrapping
  if (!wrapped && (texelT < 0 || texelT >= MTex->GetHeight())) return true;
  if (!MTex->isSeeThrough()) return true;

  const TVec saxis = line->ndir*(TextureSScale(MTex)*sidedef->Mid.ScaleX);
  const float soffs = -DotProduct(*line->v1, saxis)+sidedef->Mid.TextureOffset*TextureOffsetSScale(MTex);

  const float texelS = (int)(DotProduct(hitpoint, saxis)+soffs)%MTex->GetWidth();

  auto pix = MTex->getPixel(texelS, texelT);
  return (pix.a < 128);
}


//==========================================================================
//
//  LightCheckSectorPlanesPass
//
//  returns `false` if blocked
//
//==========================================================================
static bool LightCheckSectorPlanesPass (LightTraceInfo &trace, const line_t *line, const float frac) {
  const int s1 = line->PointOnSide2(trace.Start);
  TVec hitpoint = trace.Start+frac*trace.Delta;
  sector_t *front = (s1 == 0 || s1 == 2 ? line->frontsector : line->backsector);
  trace.LineEnd = hitpoint;
  if (!LightCheckPlanes(trace, front)) return false;
  trace.LineStart = trace.LineEnd;

  if ((line->flags&ML_TWOSIDED) == 0) return false; // stop

  return LightCheck2SLinePass(trace, line, hitpoint);
}


//==========================================================================
//
//  LightCheckLine
//
//  return `true` if line is not crossed or put into intercept list
//  return `false` to stop checking due to blocking
//
//==========================================================================
static bool LightCheckLine (LightTraceInfo &trace, line_t *ld) {
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
  // if ending point is on a line, ignore this line
  if (fabsf(dot2) <= 0.1f) return true;

  // do not use multiplication to check: zero speedup, lost accuracy
  //if (dot1*dot2 >= 0) return true; // line isn't crossed
  if (dot1 < 0.0f && dot2 < 0.0f) return true; // didn't reached back side
  // if the trace is parallel to the line plane, ignore it
  if (dot1 >= 0.0f && dot2 >= 0.0f) return true; // didn't reached front side

  // try to early out the check
  if (!ld->backsector || !(ld->flags&ML_TWOSIDED)) {
    return false; // stop checking
  }

  // signed distance
  const float den = DotProduct(ld->normal, trace.Delta);
  if (fabsf(den) < 0.00001f) return true; // wtf?!
  const float num = ld->dist-DotProduct(trace.Start, ld->normal);
  const float frac = num/den;

  // store the line for later flat plane intersection testing
  intercept_t *icept;

  EnsureFreeIntercept();
  // find place to put our new record
  // this is usually faster than sorting records, as we are traversing blockmap
  // more-or-less in order
  if (interUsed > 0) {
    unsigned ipos = interUsed;
    while (ipos > 0 && frac < intercepts[ipos-1].frac) --ipos;
    // here we should insert at `ipos` position
    if (ipos == interUsed) {
      // as last
      icept = &intercepts[interUsed++];
    } else {
      // make room
      memmove(intercepts+ipos+1, intercepts+ipos, (interUsed-ipos)*sizeof(intercepts[0]));
      ++interUsed;
      icept = &intercepts[ipos];
    }
  } else {
    icept = &intercepts[interUsed++];
  }

  icept->line = ld;
  icept->frac = frac;

  return true; // continue scanning
}


//==========================================================================
//
//  LightBlockLinesIterator
//
//==========================================================================
static bool LightBlockLinesIterator (LightTraceInfo &trace, int x, int y) {
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
          if (!LightCheckLine(trace, sit.line())) return false;
        }
      }
    }
  }

  offset = *(trace.Level->BlockMap+offset);

  for (const vint32 *list = trace.Level->BlockMapLump+offset+1; *list != -1; ++list) {
    if (!LightCheckLine(trace, &trace.Level->Lines[*list])) return false;
  }

  return true; // everything was checked
}


//==========================================================================
//
//  LightTraverseIntercepts
//
//  Returns true if the traverser function returns true for all lines
//
//==========================================================================
static bool LightTraverseIntercepts (LightTraceInfo &trace) {
  int count = (int)interUsed;

  if (count > 0) {
    // go through in order
    intercept_t *scan = intercepts;
    for (int i = count; i--; ++scan) {
      if (!LightCheckSectorPlanesPass(trace, scan->line, scan->frac)) return false; // don't bother going further
    }
  }

  trace.LineEnd = trace.End;
  return LightCheckPlanes(trace, trace.EndSubSector->sector);
}


//==========================================================================
//
//  LightCheckStartingPObj
//
//  check if starting point is inside any 3d pobj
//  we have to do this to check pobj planes first
//
//  returns `true` if no obstacle was hit
//  sets `trace.LineEnd` if something was hit (and returns `false`)
//
//==========================================================================
static bool LightCheckStartingPObj (LightTraceInfo &trace) {
  // if starting point is inside a 3d pobj, check pobj plane hits
  // if no hit, or enter time is more than first interception frac, ignore
  // otherwise check if hit point is still inside a pobj
  for (auto &&it : trace.StartSubSector->PObjFirst()) {
    polyobj_t *po = it.pobj();
    if (!po->Is3D()) continue;
    if (!IsPointInside2DBBox(trace.Start.x, trace.Start.y, po->bbox2d)) continue;
    if (!trace.Level->IsPointInsideSector2D(po->GetSector(), trace.Start.x, trace.Start.y)) continue;
    // starting point is inside 3d pobj inner sector, check pobj planes

    PlaneHitInfo phi(trace.Start, trace.End);
    // implement proper blocking (so we could have transparent polyobjects)
    //unsigned flagmask = trace.PlaneNoBlockFlags;
    //flagmask &= SPF_FLAG_MASK;

    phi.update(po->pofloor);
    phi.update(po->poceiling);

    if (!phi.wasHit) continue; // no pobj plane hit

    // check if hitpoint is still inside this pobj
    const TVec hp = phi.getHitPoint();
    if (!IsPointInside2DBBox(hp.x, hp.y, po->bbox2d)) continue;
    if (!trace.Level->IsPointInsideSector2D(po->GetSector(), trace.Start.x, trace.Start.y)) continue;

    // yep, got it; we don't care about "best hit" here, only hit presence matters
    trace.LineEnd = hp;
    return false;
  }

  // no starting hit
  return true;
}


//==========================================================================
//
//  LightPathTraverse
//
//  traces a light ray from `trace.Start` to `trace.End`
//  `trace.StartSubSector` and `trace.EndSubSector` must be set
//
//  returns `true` if no obstacle was hit
//  sets `trace.LineEnd` if something was hit
//
//==========================================================================
static bool LightPathTraverse (LightTraceInfo &trace) {
  VBlockMapWalker walker;

  ResetIntercepts();

  trace.LineStart = trace.Start;
  trace.Delta = trace.End-trace.Start;

  if (fabsf(trace.Delta.x) <= 1.0f && fabsf(trace.Delta.y) <= 1.0f) {
    // vertical trace; check starting sector planes and get out
    trace.LineEnd = trace.End;
    // point cannot hit anything!
    if (fabsf(trace.Delta.z) <= 1.0f) return false;
    if (!LightCheckStartingPObj(trace)) return false;
    return LightCheckPlanes(trace, trace.StartSubSector->sector);
  }

  if (!LightCheckStartingPObj(trace)) return false;

  if (walker.start(trace.Level, trace.Start.x, trace.Start.y, trace.End.x, trace.End.y)) {
    trace.Plane.SetPointDirXY(trace.Start, trace.Delta);
    trace.Level->IncrementValidCount();
    int mapx, mapy;
    while (walker.next(mapx, mapy)) {
      if (!LightBlockLinesIterator(trace, mapx, mapy)) return false; // hit found
    }
    // couldn't early out, so go through the sorted list
    return LightTraverseIntercepts(trace);
  }

  // out of map, see nothing
  return false;
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
//  VLevel::CastLightRay
//
//  doesn't check pvs or reject
//
//==========================================================================
bool VLevel::CastLightRay (bool textureCheck, const subsector_t *startSubSector, const TVec &org, const TVec &dest, const subsector_t *endSubSector) {
  // if starting or ending point is out of blockmap bounds, don't bother tracing
  // we can get away with this, because nothing can see anything beyound the map extents
  if (isNotInsideBM(org, this)) return false;
  if (isNotInsideBM(dest, this)) return false;

  if (lengthSquared(org-dest) <= 2.0f) return true;

  LightTraceInfo trace;
  trace.setup(this, org, dest, startSubSector, endSubSector);
  trace.textureCheck = textureCheck;
  return LightPathTraverse(trace);
}


//==========================================================================
//
//  Script natives
//
//==========================================================================
IMPLEMENT_FUNCTION(VLevel, CastLightRay) {
  bool textureCheck;
  TVec org, dest;
  VOptParamPtr<subsector_t> startSubSector(nullptr);
  VOptParamPtr<subsector_t> endSubSector(nullptr);
  vobjGetParamSelf(textureCheck, org, dest, startSubSector, endSubSector);
  bool res = Self->CastLightRay(textureCheck, startSubSector, org, dest, endSubSector);
  RET_BOOL(res);
}
