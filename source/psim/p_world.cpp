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
#include "p_entity.h"
#include "p_world.h"

//#define VV_DEBUG_TRAVERSER



//==========================================================================
//
//  VBlockPObjIterator::VBlockPObjIterator
//
//==========================================================================
VBlockPObjIterator::VBlockPObjIterator (VLevel *ALevel, int x, int y, polyobj_t **APolyPtr, bool all)
  : Level(ALevel)
  , PolyPtr(APolyPtr)
  , PolyLink(nullptr)
  , returnAll(all)
{
  if (x < 0 || x >= Level->BlockMapWidth || y < 0 || y >= Level->BlockMapHeight) return; // off the map

  const int offset = y*Level->BlockMapWidth+x;
  PolyLink = Level->PolyBlockMap[offset];
}


//==========================================================================
//
//  VBlockPObjIterator::GetNext
//
//==========================================================================
bool VBlockPObjIterator::GetNext () {
  for (; PolyLink; PolyLink = PolyLink->next) {
    polyobj_t *po = PolyLink->polyobj;
    if (!po) continue;
    if (!po->Is3D() && !returnAll) continue;
    if (po->validcount == validcount) continue;
    po->validcount = validcount;
    *PolyPtr = po;
    return true;
  }
  return false;
}



//==========================================================================
//
//  VRadiusThingsIterator::VRadiusThingsIterator
//
//==========================================================================
VRadiusThingsIterator::VRadiusThingsIterator (VThinker *ASelf, VEntity **AEntPtr, TVec Org, float Radius)
  : Self(ASelf)
  , EntPtr(AEntPtr)
  , Ent(nullptr)
{
  if (Radius < 0.0f) Radius = 0.0f;
  // cache some variables
  const float bmOrgX = Self->XLevel->BlockMapOrgX;
  const float bmOrgY = Self->XLevel->BlockMapOrgY;
  const int bmWidth = Self->XLevel->BlockMapWidth;
  const int bmHeight = Self->XLevel->BlockMapHeight;
  // calculate blockmap rectangle
  xl = MapBlock(Org.x-Radius-bmOrgX/*-MAXRADIUS*/)-1;
  xh = MapBlock(Org.x+Radius-bmOrgX/*+MAXRADIUS*/)+1;
  yl = MapBlock(Org.y-Radius-bmOrgY/*-MAXRADIUS*/)-1;
  yh = MapBlock(Org.y+Radius-bmOrgY/*+MAXRADIUS*/)+1;
  if (xh < 0 || yh < 0 || xl >= bmWidth || yl >= bmHeight) {
    // nothing to do
    // set the vars so `GetNext()` will return `false`
    xl = xh = yl = yh = x = y = 0;
    Ent = nullptr;
  } else {
    // clip rect
    if (xl < 0) xl = 0;
    if (yl < 0) yl = 0;
    if (xh >= bmWidth) { xh = bmWidth-1; vassert(xh >= 0); }
    if (yh >= bmHeight) { yh = bmHeight-1; vassert(yh >= 0); }
    vassert(xl <= xh);
    vassert(yl <= yh);
    // prepare iteration
    x = xl;
    y = yl;
    Ent = Self->XLevel->BlockLinks[y*bmWidth+x];
    while (Ent && Ent->IsGoingToDie()) Ent = Ent->BlockMapNext;
  }
}


//==========================================================================
//
//  VRadiusThingsIterator::GetNext
//
//==========================================================================
bool VRadiusThingsIterator::GetNext () {
  for (;;) {
    while (Ent && Ent->IsGoingToDie()) Ent = Ent->BlockMapNext;

    if (Ent) {
      *EntPtr = Ent;
      Ent = Ent->BlockMapNext;
      return true;
    }

    ++y;
    if (y > yh) {
      ++x;
      y = yl;
      if (x > xh) {
        // clear it, why not
        *EntPtr = nullptr;
        return false;
      }
    }

    // it cannot get out of bounds, no need to perform any checks here
    Ent = Self->XLevel->BlockLinks[y*Self->XLevel->BlockMapWidth+x];
  }
}


//==========================================================================
//
//  VPathTraverse::VPathTraverse
//
//==========================================================================
VPathTraverse::VPathTraverse (VThinker *Self, intercept_t *AInPtr, const TVec &p0, const TVec &p1, int flags, vuint32 planeflags, vuint32 lineflags)
  : Level(nullptr)
  , poolStart(-1)
  , poolEnd(-1)
  , InPtr(AInPtr)
  , Count(0)
  , Index(0)
{
  Init(Self, p0, p1, flags, planeflags, lineflags);
}


//==========================================================================
//
//  VPathTraverse::~VPathTraverse
//
//==========================================================================
VPathTraverse::~VPathTraverse () {
  if (InPtr) { memset((void *)InPtr, 0, sizeof(*InPtr)); InPtr = nullptr; } // just in case
  if (Level && poolStart >= 0) {
    Level->ReleasePathInterception(poolStart, poolEnd);
    Level = nullptr;
    poolStart = poolEnd = -1;
  }
}


//==========================================================================
//
//  VPathTraverse::Init
//
//==========================================================================
void VPathTraverse::Init (VThinker *Self, const TVec &p0, const TVec &p1, int flags, vuint32 planeflags, vuint32 lineflags) {
  VBlockMapWalker walker;

  if (flags&PT_AIMTHINGS) flags |= PT_ADDTHINGS; // sanitize it a little

  scanflags = (unsigned)flags;

  float InX1 = p0.x, InY1 = p0.y, x2 = p1.x, y2 = p1.y;

  if ((flags&(PT_ADDTHINGS|PT_ADDLINES)) == 0) {
    GCon->Logf(NAME_Warning, "%s: requested traverse without any checks", Self->GetClass()->GetName());
  }

  #ifdef VV_DEBUG_TRAVERSER
  GCon->Logf(NAME_Debug, "AddLineIntercepts: started for %s(%u) (%g,%g,%g)-(%g,%g,%g)", Self->GetClass()->GetName(), Self->GetUniqueId(), p0.x, p0.y, p0.z, p1.x, p1.y, p1.z);
  #endif

  subsector_t *ssub = nullptr;

  Level = Self->XLevel;

  // init interception pool
  poolStart = Level->StartPathInterception();

  #ifdef VV_DEBUG_TRAVERSER
  GCon->Logf(NAME_Debug, "000: %s: requested traverse (0x%04x); start=(%g,%g); end=(%g,%g)", Self->GetClass()->GetName(), (unsigned)flags, InX1, InY1, x2, y2);
  #endif
  if (walker.start(Level, InX1, InY1, x2, y2)) {
    const float x1 = InX1, y1 = InY1;

    Level->IncrementValidCount();
    if (flags&PT_ADDTHINGS) Level->EnsureProcessedBMCellsArray();

    // check if `Length()` and `SetPointDirXY()` are happy
    if (x1 == x2 && y1 == y2) { x2 += 0.002f; y2 += 0.002f; }

    trace_org = TVec(x1, y1, 0);
    trace_dest = TVec(x2, y2, 0);
    trace_delta = trace_dest-trace_org;
    trace_dir = Normalise(trace_delta);
    trace_len = (trace_delta.x || trace_delta.y ? trace_delta.length() : 0.0f);
    trace_plane.SetPointDirXY(trace_org, trace_delta);

    const bool hvtrace = (trace_len != 0.0f);

    // for hitpoint calculations
    trace_org3d = p0;
    trace_dir3d = p1-p0;
    trace_len3d = trace_dir3d.length();

    #ifdef VV_DEBUG_TRAVERSER
    GCon->Logf(NAME_Debug, "001: %s: requested traverse (0x%04x); start=(%g,%g); end=(%g,%g); dir=(%g,%g); fulldir=(%g,%g); plane=(%g,%g,%g):%g", Self->GetClass()->GetName(), (unsigned)flags, x1, y1, x2, y2,
      trace_dir.x, trace_dir.y, trace_delta.x, trace_delta.y,
      trace_plane.normal.x, trace_plane.normal.y, trace_plane.normal.z, trace_plane.dist);
    #endif

    max_frac = FLT_MAX;

    // initial 3d pobj check
    // we can optimise this, but there is no reason, because we'll need `ssub` anyway
    if ((flags&(PT_ADDLINES|PT_NOOPENS)) == PT_ADDLINES) {
      ssub = Level->PointInSubsector(p0);
      TPlane pobjPlane;
      polyobj_t *po;
      const float pobjFrac = Level->CheckPObjPlanesPoint(p0, p1, ssub, nullptr, nullptr, &pobjPlane, &po);
      if (pobjFrac >= 0.0f) {
        intercept_t &In = NewIntercept(pobjFrac);
        In.Flags = intercept_t::IF_IsAPlane;
        In.po = po;
        In.plane = pobjPlane;
        max_frac = pobjFrac;
        // mark it as "not interesting"
        po->validcount = validcount;
      }
    }

    // walk blockmap
    int mapx, mapy;
    while (walker.next(mapx, mapy)) {
      // if we have a really long blocking line, it may be hit way before
      // other things and lines will be hit (because it can be in a starting blockmap cell, for example)
      // we still have to keep scanning, otherwise we may miss some hits
      // so we'll keep scanning until there will be nothing that is nearer than `max_frac`
      // check if current blockmap cell still can produce anything interesting
      // this can overscan, but it is better to overscan than to lost some hit
      bool interestingCell = false;
      if (max_frac < 1.0f && hvtrace) {
        //FIXME: do it faster, we don't need to check all four cell corners
        float idist = max_frac*trace_len;
        idist *= idist;
        TVec xp(mapx*MAPBLOCKUNITS, mapy*MAPBLOCKUNITS);
        xp -= trace_org;
        const float deltasX[4] = { 0.0f, float(MAPBLOCKUNITS),                 0.0f, float(MAPBLOCKUNITS) };
        const float deltasY[4] = { 0.0f,                 0.0f, float(MAPBLOCKUNITS), float(MAPBLOCKUNITS) };
        for (unsigned f = 0; f < 4; ++f) {
          const TVec np(xp.x+deltasX[f], xp.y+deltasY[f]);
          if (np.length2DSquared() <= idist) {
            interestingCell = true;
            break;
          }
        }
      } else {
        interestingCell = true;
      }
      #ifdef VV_DEBUG_TRAVERSER
      GCon->Logf(NAME_Debug, "*** traverse: mapx=%d; mapy=%d; maxtile=(%d,%d); endtile=(%d,%d); currtile=(%d,%d); interesting=%d", mapx, mapy,
        walker.maxTileX, walker.maxTileY,
        walker.dda.endTileX, walker.dda.endTileY,
        walker.dda.currTileX, walker.dda.currTileY,
        (int)interestingCell
        );
      #endif
      if (flags&PT_ADDTHINGS) AddThingIntercepts(mapx, mapy);
      // have to check lines anyway, to fix `max_frac`
      AddLineIntercepts(mapx, mapy, planeflags, lineflags);
      // if the current cell was "not interesting" (i.e. further than `max_frac`), we can early exit
      if (!interestingCell) {
        #ifdef VV_DEBUG_TRAVERSER
        GCon->Logf(NAME_Debug, "*** traverse: EARLY EXIT at mapx=%d; mapy=%d", mapx, mapy);
        #endif
        break; // no need to scan further
      }
    }

    // now we have to add sector flats
    if (ssub && !(flags&PT_NOOPENS)) {
      //GCon->Logf(NAME_Debug, "AddLineIntercepts: adding planes for %s(%u)", Self->GetClass()->GetName(), Self->GetUniqueId());
      Level->ResetTempPathIntercepts();
      if (max_frac > 1.0f) max_frac = 1.0f;

      // working variables
      float hfrac;
      bool isSky;
      TVec ohp;
      TPlane oplane;

      float prfrac = 0.0f; // previous hit time
      sector_t *checksec = ssub->sector; // sector to check
      int iidx = 0;
      for (;;) {
        // find next line (if there is any)
        float flfrac = max_frac;
        while (iidx < InterceptCount()) {
          const intercept_t *it = GetIntercept(iidx);
          // it must not be polyobj line
          if (it->line && !it->line->pobj()) {
            flfrac = it->frac;
            break;
          }
          ++iidx;
        }
        // `iidx` points *at* the line
        // `prfrac` is previous line hit time
        // `flfrac` is current line hit time

        // check starting sector planes
        if (checksec && prfrac < flfrac && !Level->CheckPassPlanes(checksec, p0, p1, planeflags, &ohp, nullptr, &isSky, &oplane, &hfrac)) {
          // found hit plane, it should be less than first line frac
          //hfrac = oplane.IntersectionTime(p0, p1, &ohp);
          if (hfrac > prfrac && hfrac < flfrac) {
            intercept_t *itp = Level->AllocTempPathIntercept();
            memset((void *)itp, 0, sizeof(*itp));
            itp->frac = hfrac;
            itp->Flags = intercept_t::IF_IsAPlane|(isSky ? intercept_t::IF_IsASky : intercept_t::IF_NothingZero);
            itp->sector = ssub->sector;
            itp->plane = oplane;
            itp->hitpoint = ohp;
          }
        }

        // check next sector?
        if (iidx >= InterceptCount()) break; // nope, no more lines, last sector was already checked
        const intercept_t *it = GetIntercept(iidx);
        // check invariants
        vassert(it->line);
        vassert(!it->line->pobj());
        // if this line is blocking, there is no need to scan further
        if (it->Flags&intercept_t::IF_IsABlockingLine) break;
        checksec = (it->side ? it->line->frontsector : it->line->backsector); // not a bug!
        ++iidx; // skip this line
        prfrac = flfrac; // remember current line hit time
      }

      // add plane hits
      intercept_t *tmpit = Level->GetTempPathInterceptFirst();
      for (int n = Level->GetTempPathInterceptsCount(); n--; ++tmpit) {
        intercept_t &In = NewIntercept(tmpit->frac);
        In = *tmpit;
      }
    }
  }

  poolEnd = Level->EndPathInterception();

  Count = InterceptCount();
  Index = 0;

  // just in case, drop everything after the first blocking line or blocking plane
  if (Count) {
    const int len = Count;
    const intercept_t *it = GetIntercept(0);
    if (flags&PT_ADDLINES) {
      // collecting lines
      for (int n = 0; n < len; ++n, ++it) {
        // either flat, or a blocking line ("blocking line" flag can be set only for lines, no need to do more checks)
        if (it->Flags&(intercept_t::IF_IsAPlane|intercept_t::IF_IsABlockingLine)) {
          Count = n+1;
          break; // there is no reason to scan further
        }
      }
    } else {
      // collecting things, remove everything beyond (or equal to) max_frac (blocking line)
      for (int n = 0; n < len; ++n, ++it) {
        if (it->frac >= max_frac) {
          Count = n;
          break;
        }
      }
    }
  }

  #ifdef VV_DEBUG_TRAVERSER
  GCon->Logf(NAME_Debug, "AddLineIntercepts: DONE! got %d intercept%s for %s(%u)", Count, (Count != 1 ? "s" : ""), Self->GetClass()->GetName(), Self->GetUniqueId());
  #endif

  // just in case
  #ifdef PARANOID
  for (int f = 1; f < Count; ++f) {
    if (GetIntercept(f)->frac < GetIntercept(f-1)->frac) {
      for (int c = 0; c < Count; ++c) {
        intercept_t *it = GetIntercept(c);
        GCon->Logf(NAME_Debug, "idx=%d/%d; frac=%g; flags=0x%02x; thing=%s(%d); line=%d; side=%d; sector=%d; po=%d; plane=(%g,%g,%g:%g); hitpoint=(%g,%g,%g)",
          c, Count-1, it->frac, it->Flags,
          (it->thing ? it->thing->GetClass()->GetName() : "<none>"), (it->thing ? (int)it->thing->GetUniqueId() : -1),
          (it->line ? (int)(ptrdiff_t)(it->line-&Level->Lines[0]) : -1), it->side,
          (it->sector ? (int)(ptrdiff_t)(it->sector-&Level->Sectors[0]) : -1),
          (it->po ? it->po->tag : -1),
          it->plane.normal.x, it->plane.normal.y, it->plane.normal.z, it->plane.dist,
          it->hitpoint.x, it->hitpoint.y, it->hitpoint.z);
      }
      Sys_Error("VPathTraverse: internal sorting error at %d", f);
    }
  }
  #endif
}


//==========================================================================
//
//  VPathTraverse::NewIntercept
//
//  insert new intercept
//  this is faster than sorting, as most intercepts are already sorted
//
//==========================================================================
intercept_t &VPathTraverse::NewIntercept (const float frac) {
  intercept_t *it;
  int pos = InterceptCount();
  if (pos == 0 || GetIntercept(pos-1)->frac <= frac) {
    // no need to bubble, just append it
    it = Level->AllocPathIntercept();
  } else {
    // bubble
    while (pos > 0 && GetIntercept(pos-1)->frac > frac) --pos;
    // insert
    // allocate new
    (void)Level->AllocPathIntercept();
    // move down
    it = GetIntercept(pos);
    memmove((void *)(it+1), (void *)it, (InterceptCount()-pos-1)*sizeof(intercept_t));
    // setup
  }
  memset((void *)it, 0, sizeof(*it));
  it->frac = frac;
  it->hitpoint = trace_org3d+trace_dir3d*frac;
  return *it;
}


//==========================================================================
//
//  VPathTraverse::AddLineIntercepts
//
//  looks for lines in the given block that intercept the given trace to add
//  to the intercepts list.
//  a line is crossed if its endpoints are on opposite sides of the trace.
//
//  note that we cannot check planes here, because we need list of lines
//  sorted by frac.
//  with sorted lines, sector/pobj plane frac must be between two lines to
//  register a hit.
//
//==========================================================================
void VPathTraverse::AddLineIntercepts (int mapx, int mapy, vuint32 planeflags, vuint32 lineflags) {
  const bool doopening = !(scanflags&PT_NOOPENS);
  const bool doadd = (scanflags&PT_ADDLINES);
  const bool dorailings = (scanflags&PT_RAILINGS);
  for (auto &&it : Level->allBlockLines(mapx, mapy)) {
    line_t *ld = it.line();
    const float dot1 = trace_plane.PointDistance(*ld->v1);
    const float dot2 = trace_plane.PointDistance(*ld->v2);

    // do not use multiplication to check: zero speedup, lost accuracy
    //if (dot1*dot2 >= 0) continue; // line isn't crossed
    if (dot1 < 0.0f && dot2 < 0.0f) continue; // didn't reached back side
    // if the trace is parallel to the line plane, ignore it
    if (dot1 >= 0.0f && dot2 >= 0.0f) continue; // didn't reached front side

    // hit the line

    // find the fractional intercept point along the trace line
    const float den = DotProduct(ld->normal, trace_delta);
    if (den == 0.0f) continue;

    const float num = ld->dist-DotProduct(trace_org, ld->normal);
    const float frac = num/den;
    if (frac < 0.0f || frac > 1.0f) continue; // behind source or beyond end point

    #ifdef VV_DEBUG_TRAVERSER
    GCon->Logf(NAME_Debug, "000: pathtrace: line #%d; frac=%g; max=%g; flags=0x%08x", (int)(ptrdiff_t)(ld-&Level->Lines[0]), frac, max_frac, ld->flags);
    #endif

    if (frac > max_frac) {
      // this line is too far away
      continue;
    }

    bool blockFlag;
    bool isSky = false;
    // ignore 3d pobj lines we cannot possibly hit
    // also, if such line was hit, it is blocking, for sure
    polyobj_t *po = ld->pobj();
    if (/*doopening &&*/ po && po->Is3D()) {
      // 3d polyobject
      const float hpz = trace_org3d.z+trace_dir3d.z*frac;
      // check if hitpoint is under or above a pobj
      if (hpz < po->pofloor.minz || hpz > po->poceiling.maxz) {
        // check hitscan blocking flags (only front side matters for now)
        const unsigned lbflag = ld->flags|(ld->flags&ML_BLOCKEVERYTHING ? ML_BLOCKPROJECTILE : ML_NOTHING_ZERO);
        if (doopening && ((lbflag&lineflags)&(ML_BLOCKHITSCAN|ML_BLOCKPROJECTILE)) && ld->sidenum[0] >= 0 && hpz > po->poceiling.maxz) {
          const side_t *fsd = &Level->Sides[ld->sidenum[0]];
          if (fsd->TopTexture > 0) {
            VTexture *TTex = GTextureManager(fsd->TopTexture);
            if (TTex && TTex->Type != TEXTYPE_Null) {
              const float texh = TTex->GetScaledHeightF()/fsd->Top.ScaleY;
              if (hpz < po->poceiling.maxz+texh) goto xxdone; // sorry
            }
          }
        }
        // non-blocking pobj line: check polyobject planes
        // if this polyobject plane was already added to list, it is not interesting anymore
        //
        // possible optimisation: check this after finishing with line list
        // this way we'll have sorted lines, and we can skip checking "point-in-sector" (sometimes),
        // because if pobj plane hit time is bigger than the time of the next line, there's no hit
        // yet this is complicated by 2-sided non-blocking lines, so maybe we should check hit time
        // against two lines of the same polyobject (there *MUST* be two of them for enter/exit)
        if (po->validcount != validcount) {
          // check if we can hit pobj vertically
          const float tz0 = trace_org3d.z;
          const float tz1 = trace_org3d.z+trace_dir3d.z;
          const float tminz = min2(tz0, tz1);
          const float tmaxz = max2(tz0, tz1);
          if (tmaxz <= po->pofloor.minz || tminz >= po->poceiling.maxz) continue;
          // hit is possible, check pobj planes
          TVec php;
          TPlane pplane;
          const float pfrc = Level->CheckPObjPassPlanes(po, trace_org3d, trace_org3d+trace_dir3d, &php, nullptr, &pplane);
          if (pfrc <= 0.0f || pfrc >= max_frac) continue; // cannot hit
          // check if hitpoint is still inside pobj
          if (!IsPointInside2DBBox(php.x, php.y, po->bbox2d)) continue;
          if (!Level->IsPointInsideSector2D(po->GetSector(), php.x, php.y)) continue;
          // yep; add polyobject plane to intercept list
          intercept_t &itp = NewIntercept(pfrc);
          itp.Flags = intercept_t::IF_IsAPlane;
          itp.po = po;
          itp.plane = pplane;
          // and mark this polyobject as "not interesting"
          po->validcount = validcount;
        }
        continue;
      }
     xxdone:
      blockFlag = doopening;
    } else {
      // in "compat" mode, lines of self-referenced sectors are ignored
      if (!po && ld->frontsector == ld->backsector && (scanflags&PT_COMPAT)) continue;

      if (ld->flags&ML_TWOSIDED) {
        // flag-blocking line?
        blockFlag = !!(ld->flags&lineflags);
        if (doopening && !blockFlag && !po) {
          // want opening scan, not blocked by flags check, and not a polyobject
          // there is no reason to check openings for polyobjects, such openings are not valid anyway
          // (proper opening check will be done with a proper two-sided sector line)
          const TVec hitPoint = trace_org3d+trace_dir3d*frac;
          const float hpz = hitPoint.z;
          opening_t *open = Level->LineOpenings(ld, hitPoint, planeflags, false/*do3dmidtex*/);
          while (open) {
            if (open->range > 0.0f && hpz >= open->bottom && hpz <= open->top) break; // shot continues
            open = open->next;
          }
          if (open && dorailings && (ld->flags&ML_RAILING)) {
            //GCon->Logf(NAME_Debug, "...railing bump: from %g to %g (valid=%d; hpok=%d)", open->bottom, open->bottom+32.0f, (int)(open->bottom+32.0f < open->top), (int)(hpz >= open->bottom+32.0f && hpz <= open->top));
            open->bottom += 32.0f;
            open->range -= 32.0f;
            if (open->range <= 0.0f || hpz < open->bottom || hpz > open->top) open = nullptr;
          }
          blockFlag = !open; // block if no opening was found
        }
        // sky hack?
        if (blockFlag && doopening && !po &&
            ld->frontsector->ceiling.pic == skyflatnum &&
            ld->backsector->ceiling.pic == skyflatnum)
        {
          // check position (and texture?)
          const TVec hitPoint = trace_org3d+trace_dir3d*frac; // we can cache this, but meh, sky hacks are not that frequent
          const float cz = min2(ld->frontsector->ceiling.GetPointZClamped(hitPoint), ld->backsector->ceiling.GetPointZClamped(hitPoint));
          if (hitPoint.z >= cz) isSky = true; // top texture
        }
      } else {
        // one-sided line always blocks
        blockFlag = true;
      }
    }

    if (blockFlag) {
      max_frac = frac; // we cannot travel further anyway
    }

    if (doadd) {
      if (!isSky && ld->special == LNSPEC_LineHorizon) isSky = true;
      intercept_t &In = NewIntercept(frac);
      In.Flags = intercept_t::IF_IsALine|(blockFlag ? intercept_t::IF_IsABlockingLine : intercept_t::IF_NothingZero)|(isSky ? intercept_t::IF_IsASky : intercept_t::IF_NothingZero);
      In.line = ld;
      In.side = ld->PointOnSide2(trace_org3d);
      if (In.side == 2) {
        In.side = ld->PointOnSide2(trace_org3d-trace_dir*128.0f);
        if (In.side == 2) In.side = 0; // just in case
      }
      #ifdef VV_DEBUG_TRAVERSER
      GCon->Logf(NAME_Debug, "001: pathtrace: line #%d; frac=%g; max=%g; start=(%g,%g,%g); hit=(%g,%g,%g); blockFlag=%d", (int)(ptrdiff_t)(ld-&Level->Lines[0]), frac, max_frac, trace_org3d.x, trace_org3d.y, trace_org3d.z, In.hitpoint.x, In.hitpoint.y, In.hitpoint.z, (int)blockFlag);
      #endif
      // set line sector
      if (po) {
        //FIXME: this code is a mess!
             if (po->Is3D()) In.sector = po->GetSector(); // it's ok for puff checks
        else if (ld->frontsector) In.sector = ld->frontsector;
        else In.sector = ld->backsector;
      } else {
        In.sector = (In.side ? ld->backsector : ld->frontsector);
      }
      //GCon->Logf(NAME_Debug, "002: pathtrace: line #%d; frac=%g; max=%g", (int)(ptrdiff_t)(In.line-&Level->Lines[0]), In.frac, max_frac);
    }
  }
}


//==========================================================================
//
//  VPathTraverse::AddThingIntercepts
//
//==========================================================================
void VPathTraverse::AddThingIntercepts (int mapx, int mapy) {
  //if (!Self) return false;
  // proper ray-vs-aabb
  float bbox[4];
  /*static*/ const int deltas[3] = { 0, -1, 1 };
  for (unsigned dy = 0; dy < 3; ++dy) {
    for (unsigned dx = 0; dx < 3; ++dx) {
      #ifdef VV_DEBUG_TRAVERSER_BMCELL
      GCon->Logf(NAME_Debug, "BMCELL: (%d,%d)", mapx+deltas[dx], mapy+deltas[dy]);
      #endif
      if (!NeedCheckBMCell(mapx+deltas[dx], mapy+deltas[dy])) continue;
      #ifdef VV_DEBUG_TRAVERSER_BMCELL
      GCon->Logf(NAME_Debug, "BMCELL: checking (%d,%d)", mapx+deltas[dx], mapy+deltas[dy]);
      #endif
      for (auto &&it : Level->allBlockThings(mapx+deltas[dx], mapy+deltas[dy])) {
        VEntity *ent = it.entity();
        #ifdef VV_DEBUG_TRAVERSER_BMCELL
        GCon->Logf(NAME_Debug, "BMCELL: entity %s(%u) (checked=%d) Height=%g, Radius=%g; cell=(%d,%d)", ent->GetClass()->GetName(), ent->GetUniqueId(), (int)(ent->ValidCount == validcount), ent->Height, ent->Radius, mapx+deltas[dx], mapy+deltas[dy]);
        #endif
        if (ent->ValidCount == validcount) continue;
        ent->ValidCount = validcount;
        if (ent->Height <= 0.0f) continue;
        const float rad = ent->Radius;
        if (rad <= 0.0f) continue;

        // fast reject using trace plane
        if (trace_len != 0.0f) {
          Create2DBBox(bbox, ent->Origin, ent->Radius);
          if (trace_plane.checkBox2DEx(bbox) != TPlane::PARTIALLY) continue;
        }

        #ifdef VV_DEBUG_TRAVERSER
        GCon->Logf(NAME_Debug, "BMCELL: entity %s(%u) start checking... traceorg=(%g,%g,%g); thingorg=(%g,%g,%g); tracedir=(%g,%g,%g)", ent->GetClass()->GetName(), ent->GetUniqueId(),
          trace_org3d.x, trace_org3d.y, trace_org3d.z,
          ent->Origin.x, ent->Origin.y, ent->Origin.z,
          trace_dir3d.x, trace_dir3d.y, trace_dir3d.z);
        #endif

        // use standard ray-aabb intersection algorithm (instead of the crap ZDoom is using because RH cannot code)
        // if we're doing 2D checks, use 2d checks, and thing center as hitpoint
        if (scanflags&(PT_NOOPENS|PT_AIMTHINGS)) {
          const float rbfrac = RayBoxIntersection2D(ent->Origin, rad, trace_org, trace_delta);
          #ifdef VV_DEBUG_TRAVERSER
          GCon->Logf(NAME_Debug, "BMCELL: CENTER entity %s(%u) rbfrac=%g; max_frac=%g", ent->GetClass()->GetName(), ent->GetUniqueId(), rbfrac, max_frac);
          #endif
          if (rbfrac >= 0.0f && rbfrac <= max_frac) {
            // yes, use entity center as hit point
            const float dist = DotProduct((TVec(ent->Origin.x, ent->Origin.y)-trace_org), trace_dir); //dist -= sqrt(ent->radius * ent->radius - dot * dot);
            #ifdef VV_DEBUG_TRAVERSER
            GCon->Logf(NAME_Debug, "BMCELL: entity %s(%u) CENTER; dist=%g; distance=%g", ent->GetClass()->GetName(), ent->GetUniqueId(), dist, trace_len);
            #endif
            const float cffrac = (trace_len ? dist/trace_len : 0.0f);
            #ifdef VV_DEBUG_TRAVERSER
            GCon->Logf(NAME_Debug, "BMCELL: entity %s(%u) CENTER; frac=%g", ent->GetClass()->GetName(), ent->GetUniqueId(), cffrac);
            #endif
            if (cffrac < 0.0f || cffrac > max_frac) continue;
            intercept_t &In = NewIntercept(cffrac);
            In.Flags = 0;
            In.thing = ent;
          }
        } else {
          const float rbfrac = RayBoxIntersection3D(ent->Origin, rad, ent->Height, trace_org3d, trace_dir3d);
          #ifdef VV_DEBUG_TRAVERSER
          GCon->Logf(NAME_Debug, "BMCELL: entity %s(%u) rbfrac=%g; max_frac=%g", ent->GetClass()->GetName(), ent->GetUniqueId(), rbfrac, max_frac);
          #endif
          if (rbfrac >= 0.0f && rbfrac <= max_frac) {
            intercept_t &In = NewIntercept(rbfrac);
            In.Flags = 0;
            In.thing = ent;
          }
        }
      }
    }
  }
  /*
    // original
    if (!NeedCheckBMCell(mapx, mapy)) return;
    for (auto &&it : Level->allBlockThings(mapx, mapy)) {
      VEntity *ent = it.entity();
      if (ent->ValidCount == validcount) continue;
      ent->ValidCount = validcount;
      if (ent->Radius <= 0.0f || ent->Height <= 0.0f) continue;
      const float dot = trace_plane.PointDistance(ent->Origin);
      if (fabsf(dot) >= ent->Radius) continue; // thing is too far away
      const float dist = DotProduct((ent->Origin-trace_org), trace_dir); //dist -= sqrt(ent->radius * ent->radius - dot * dot);
      if (dist < 0.0f) continue; // behind source
      const float frac = (trace_len ? dist/trace_len : 0.0f);
      if (frac < 0.0f || frac > 1.0f) continue;
      AddProperThingHit(ent, frac);
    }
  */
}


//==========================================================================
//
//  VPathTraverse::GetNext
//
//==========================================================================
bool VPathTraverse::GetNext () {
  if (Index >= Count) { if (InPtr) memset((void *)InPtr, 0, sizeof(*InPtr)); InPtr = nullptr; Index = 0; Count = 0; return false; } // everything was traversed
  *InPtr = *GetIntercept(Index++);
  return true;
}
