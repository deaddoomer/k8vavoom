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
#include "../server/sv_local.h"

//static VCvarI dbg_thing_traverser("dbg_thing_traverser", "0", "Thing checking in interceptor: 0=best; 1=Vavoom, better; 2=Vavoom, original", CVAR_PreInit);

// 0: best
// 1: Vavoom, better
// 2: Vavoom, original
#define VV_THING_TRAVERSER  0


// ////////////////////////////////////////////////////////////////////////// //
#define EQUAL_EPSILON (1.0f/65536.0f)

// used in new thing coldet
struct divline_t {
  float x, y;
  float dx, dy;
};


//==========================================================================
//
//  pointOnDLineSide
//
//  returns 0 (front/on) or 1 (back)
//
//==========================================================================
static inline int pointOnDLineSide (const float x, const float y, const divline_t &line) {
  return ((y-line.y)*line.dx+(line.x-x)*line.dy > EQUAL_EPSILON);
}


//==========================================================================
//
//  interceptVector
//
//  returns the fractional intercept point along the first divline
//
//==========================================================================
static float interceptVector (const divline_t &v2, const divline_t &v1) {
  const float v1x = v1.x;
  const float v1y = v1.y;
  const float v1dx = v1.dx;
  const float v1dy = v1.dy;
  const float v2x = v2.x;
  const float v2y = v2.y;
  const float v2dx = v2.dx;
  const float v2dy = v2.dy;
  const float den = v1dy*v2dx-v1dx*v2dy;
  if (den == 0) return 0; // parallel
  const float num = (v1x-v2x)*v1dy+(v2y-v1y)*v1dx;
  return num/den;
}


//==========================================================================
//
//  VBlockLinesIterator::VBlockLinesIterator
//
//==========================================================================
VBlockLinesIterator::VBlockLinesIterator (VLevel *ALevel, int x, int y, line_t **ALinePtr, unsigned pobjMode)
  : Level(ALevel)
  , LinePtr(ALinePtr)
  , PolyLink(nullptr)
  , PolySegIdx(0)
  , List(nullptr)
{
  if (x < 0 || x >= Level->BlockMapWidth || y < 0 || y >= Level->BlockMapHeight) return; // off the map

  int offset = y*Level->BlockMapWidth+x;
  if (pobjMode&POBJ_POBJ) PolyLink = Level->PolyBlockMap[offset];

  offset = *(Level->BlockMap+offset);
  if (pobjMode&POBJ_LINES) List = Level->BlockMapLump+offset+1;
}


//==========================================================================
//
//  VBlockLinesIterator::GetNext
//
//==========================================================================
bool VBlockLinesIterator::GetNext () {
  // check polyobj blockmap
  while (PolyLink) {
    polyobj_t *po = PolyLink->polyobj;
    if (po) {
      while (PolySegIdx < po->numlines) {
        line_t *linedef = po->lines[PolySegIdx++];
        if (linedef->validcount == validcount) continue;
        linedef->validcount = validcount;
        *LinePtr = linedef;
        return true;
      }
    }
    PolySegIdx = 0;
    PolyLink = PolyLink->next;
  }

  if (List) {
    while (*List != -1) {
      #ifdef PARANOID
      if (*List < 0 || *List >= Level->NumLines) Host_Error("Broken blockmap - line %d", *List);
      #endif
      line_t *Line = &Level->Lines[*List];
      ++List;

      if (Line->validcount == validcount) continue; // line has already been checked

      Line->validcount = validcount;
      *LinePtr = Line;
      return true;
    }
    List = nullptr; // just in case
  }

  return false;
}



//==========================================================================
//
//  VBlockPObjIterator::VBlockPObjIterator
//
//==========================================================================
VBlockPObjIterator::VBlockPObjIterator (VLevel *ALevel, int x, int y, polyobj_t **APolyPtr)
  : Level(ALevel)
  , PolyPtr(APolyPtr)
  , PolyLink(nullptr)
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
  while (PolyLink) {
    polyobj_t *po = PolyLink->polyobj;
    PolyLink = PolyLink->next;
    if (po && po->validcount != validcount) {
      po->validcount = validcount;
      *PolyPtr = po;
      return true;
    }
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
VPathTraverse::VPathTraverse (VThinker *Self, intercept_t **AInPtr, const TVec &p0, const TVec &p1, int flags, vuint32 planeflags, vuint32 lineflags)
  : Count(0)
  , In(nullptr)
  , InPtr(AInPtr)
{
  Init(Self, p0, p1, flags, planeflags, lineflags);
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

  //GCon->Logf(NAME_Debug, "AddLineIntercepts: started for %s(%u) (%g,%g,%g)-(%g,%g,%g)", Self->GetClass()->GetName(), Self->GetUniqueId(), p0.x, p0.y, p0.z, p1.x, p1.y, p1.z);

  subsector_t *ssub = nullptr;

  Level = Self->XLevel;

  //GCon->Logf(NAME_Debug, "000: %s: requested traverse (0x%04x); start=(%g,%g); end=(%g,%g)", Self->GetClass()->GetName(), (unsigned)flags, InX1, InY1, x2, y2);
  if (walker.start(Level, InX1, InY1, x2, y2)) {
    const float x1 = InX1, y1 = InY1;

    Level->IncrementValidCount();
    if (flags&PT_ADDTHINGS) Level->EnsureProcessedBMCellsArray();

    // check if `Length()` and `SetPointDirXY()` are happy
    if (x1 == x2 && y1 == y2) { x2 += 0.002f; y2 += 0.002f; }

    //GCon->Logf(NAME_Debug, "001: %s: requested traverse (0x%04x); start=(%g,%g); end=(%g,%g)", Self->GetClass()->GetName(), (unsigned)flags, x1, y1, x2, y2);

    trace_org = TVec(x1, y1, 0);
    trace_dest = TVec(x2, y2, 0);
    trace_delta = trace_dest-trace_org;
    trace_dir = Normalise(trace_delta);
    trace_len = (trace_delta.x || trace_delta.y ? trace_delta.length() : 0.0f);
    trace_plane.SetPointDirXY(trace_org, trace_delta);

    // for hitpoint calculations
    trace_org3d = p0;
    trace_dir3d = p1-p0;
    trace_len3d = trace_dir3d.length();

    max_frac = FLT_MAX;

    // initial 3d pobj check
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
      if (flags&PT_ADDTHINGS) AddThingIntercepts(Self, mapx, mapy);
      if (AddLineIntercepts(Self, mapx, mapy, planeflags, lineflags)) break;
    }

    // now we have to add sector flats
    if (ssub && !(flags&PT_NOOPENS)) {
      //GCon->Logf(NAME_Debug, "AddLineIntercepts: adding planes for %s(%u)", Self->GetClass()->GetName(), Self->GetUniqueId());

      // find first line
      int iidx = 0;
      intercept_t *it = nullptr;
      while (iidx < Intercepts.length()) {
        it = Intercepts.ptr()+iidx;
        if (it->line) {
          if (!it->line->pobj()) break; // it must not be polyobj line
        }
        ++iidx;
      }

      TArray<intercept_t> itmp;

      // check starting sector planes
      {
        float pfrac;
        bool isSky;
        TVec ohp;
        TPlane oplane;
        TVec end = p0+trace_dir3d*min2(1.0f, max_frac);
        //GCon->Logf(NAME_Debug, "AddLineIntercepts: base planes for %s(%u); max_frac=%g", Self->GetClass()->GetName(), Self->GetUniqueId(), min2(1.0f, max_frac));
        if (!Level->CheckPassPlanes(ssub->sector, p0, end, planeflags, &ohp, nullptr, &isSky, &oplane, &pfrac)) {
          // found hit plane, it should be less than first line frac
          pfrac = oplane.IntersectionTime(p0, p1, &ohp);
          if (pfrac >= 0.0f && (iidx >= Intercepts.length() || pfrac < Intercepts.ptr()[iidx].frac)) {
            intercept_t &itp = itmp.alloc();
            memset((void *)&itp, 0, sizeof(itp));
            itp.frac = pfrac;
            itp.Flags = intercept_t::IF_IsAPlane|(isSky ? intercept_t::IF_IsASky : 0u);
            itp.sector = ssub->sector;
            itp.plane = oplane;
            itp.hitpoint = ohp;
          }
        }
      }

      TVec lineStart = p0;
      while (iidx < Intercepts.length()) {
        it = Intercepts.ptr()+iidx;
        line_t *ld = it->line;
        vassert(ld);
        const TVec hitPoint = it->hitpoint;
        // check line planes
        sector_t *sec = (ld->PointOnSide(lineStart) ? ld->backsector : ld->frontsector);
        if (sec && sec != ssub->sector) {
          // check sector planes
          float pfrac;
          bool isSky;
          TVec ohp;
          TPlane oplane;
          if (!Level->CheckPassPlanes(sec, lineStart, hitPoint, planeflags, nullptr, nullptr, &isSky, &oplane, &pfrac)) {
            // found hit plane, recalc frac
            pfrac = oplane.IntersectionTime(p0, p1, &ohp);
            if (pfrac >= 0.0f && pfrac < max_frac) {
              intercept_t &itp = itmp.alloc();
              memset((void *)&itp, 0, sizeof(itp));
              itp.frac = pfrac;
              itp.Flags = intercept_t::IF_IsAPlane|(isSky ? intercept_t::IF_IsASky : 0u);
              itp.sector = sec;
              itp.plane = oplane;
              itp.hitpoint = ohp;
            }
          }
        }
        // next line
        if (it->Flags&intercept_t::IF_IsABlockingLine) break;
        ++iidx;
        lineStart = hitPoint;
        while (iidx < Intercepts.length()) {
          it = Intercepts.ptr()+iidx;
          if (it->line) {
            if (!it->line->pobj()) break; // it must not be polyobj line
          }
          ++iidx;
        }
      }

      // add plane hits
      //GCon->Logf(NAME_Debug, "AddLineIntercepts: %d plane%s for %s(%u)", itmp.length(), (itmp.length() != 1 ? "s" : ""), Self->GetClass()->GetName(), Self->GetUniqueId());
      for (auto &&itp : itmp) {
        intercept_t &In = NewIntercept(itp.frac);
        In = itp;
      }
    }
  }

  Count = Intercepts.Num();
  In = Intercepts.Ptr();

  // just in case, drop everything after the first blocking line or blocking plane
  if ((flags&PT_ADDLINES) && Count) {
    const int len = Count;
    intercept_t *it = Intercepts.ptr();
    for (int n = 0; n < len; ++n, ++it) {
      if ((it->Flags&(intercept_t::IF_IsALine|intercept_t::IF_IsABlockingLine)) == (intercept_t::IF_IsALine|intercept_t::IF_IsABlockingLine)) {
        Count = n+1;
        break; // there is no reason to scan further
      }
      if (it->Flags&intercept_t::IF_IsAPlane) {
        Count = n+1;
        break; // there is no reason to scan further
      }
    }
  }

  //GCon->Logf(NAME_Debug, "AddLineIntercepts: DONE! got %d intercept%s for %s(%u)", Count, (Count != 1 ? "s" : ""), Self->GetClass()->GetName(), Self->GetUniqueId());

  // just in case
  #ifdef PARANOID
  for (int f = 1; f < Count; ++f) if (In[f].frac < In[f-1].frac) Sys_Error("VPathTraverse: internal sorting error");
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
  int pos = Intercepts.length();
  if (pos == 0 || Intercepts[pos-1].frac <= frac) {
    // no need to bubble, just append it
    intercept_t &xit = Intercepts.Alloc();
    memset((void *)&xit, 0, sizeof(xit));
    xit.frac = frac;
    xit.hitpoint = trace_org3d+trace_dir3d*frac;
    return xit;
  }
  // bubble
  while (pos > 0 && Intercepts[pos-1].frac > frac) --pos;
  // insert
  intercept_t it;
  memset((void *)&it, 0, sizeof(it));
  it.frac = frac;
  it.hitpoint = trace_org3d+trace_dir3d*frac;
  Intercepts.insert(pos, it);
  return Intercepts[pos];
}


//==========================================================================
//
//  VPathTraverse::RemoveInterceptsAfter
//
//==========================================================================
void VPathTraverse::RemoveInterceptsAfter (const float frac) {
  int len = Intercepts.length();
  while (len > 0 && Intercepts[len-1].frac >= frac) --len;
  if (len != Intercepts.length()) Intercepts.setLength(len, false); // don't resize
}


//==========================================================================
//
//  VPathTraverse::AddLineIntercepts
//
//  looks for lines in the given block that intercept the given trace to add
//  to the intercepts list.
//  a line is crossed if its endpoints are on opposite sides of the trace.
//  returns `true` if some blocking line was hit (so we can stop scanning).
//
//  note that we cannot check planes here, because we need list of lines
//  sorted by frac.
//  with sorted lines, sector/pobj plane frac must be between two lines to
//  register a hit.
//
//==========================================================================
bool VPathTraverse::AddLineIntercepts (VThinker *Self, int mapx, int mapy, vuint32 planeflags, vuint32 lineflags) {
  line_t *ld;
  bool wasBlocking = false;
  const bool doopening = !(scanflags&PT_NOOPENS);
  const bool doadd = (scanflags&PT_ADDLINES);
  for (VBlockLinesIterator It(Level, mapx, mapy, &ld); It.GetNext(); ) {
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

    //GCon->Logf(NAME_Debug, "000: pathtrace: line #%d; frac=%g; max=%g", (int)(ptrdiff_t)(ld-&Level->Lines[0]), frac, max_frac);

    if (frac > max_frac) {
      //if (!(ld->flags&ML_TWOSIDED)) wasBlocking = true;
      wasBlocking = true; // this line is too far away, no need to scan other blockmap cells
      continue;
    }

    bool blockFlag;
    bool isSky = false;
    // ignore 3d pobj lines we cannot possibly hit
    // also, if such line was hit, it is blocking, for sure
    polyobj_t *po = ld->pobj();
    if (po) {
      if (!doopening && po->posector) continue;
      const float hpz = trace_org3d.z+trace_dir3d.z*frac;
      // check if hitpoint is under or above a pobj
      if (hpz < po->pofloor.minz || hpz > po->poceiling.maxz) {
        // non-blocking pobj line: check polyobject planes
        // if this polyobject plane was already added to list, it is not interesting anymore
        if (po->posector && po->validcount != validcount) {
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
          if (pfrc < 0.0f || pfrc >= max_frac) continue; // cannot hit
          // check if hitpoint is still inside pobj
          if (!IsPointInside2DBBox(php.x, php.y, po->bbox2d)) continue;
          if (!Level->IsPointInsideSector2D(po->posector, php.x, php.y)) continue;
          // yep, it can; add polyobject plane to intercept list
          intercept_t &itp = NewIntercept(pfrc);
          itp.Flags = intercept_t::IF_IsAPlane;
          itp.po = po;
          itp.plane = pplane;
          // and mark this polyobject as "not interesting"
          po->validcount = validcount;
        }
        continue;
      }
      blockFlag = true;
    } else {
      // in "compat" mode, lines of self-referenced sectors are ignored
      if (ld->frontsector == ld->backsector && (scanflags&PT_COMPAT)) continue;

      if (ld->flags&ML_TWOSIDED) {
        if (doopening) {
          // flag-blocking line
          if (ld->flags&lineflags) {
            blockFlag = true;
            // sky hack?
            if (ld->frontsector->ceiling.pic == skyflatnum &&
                ld->backsector->ceiling.pic == skyflatnum)
            {
              const TVec hitPoint = trace_org3d+trace_dir3d*frac;
              const float hpz = hitPoint.z;
              opening_t *open = SV_LineOpenings(ld, hitPoint, planeflags, /*do3dmidtex*/false);
              if (open && hpz > open->top) isSky = true;
            }
          } else {
            // crosses a two sided line, check openings
            const TVec hitPoint = trace_org3d+trace_dir3d*frac;
            const float hpz = hitPoint.z; //trace_org3d.z+trace_dir3d.z*frac;
            opening_t *open = SV_LineOpenings(ld, hitPoint, planeflags, /*do3dmidtex*/false);
            if (open &&
                ld->frontsector->ceiling.pic == skyflatnum &&
                ld->backsector->ceiling.pic == skyflatnum &&
                hpz > open->top)
            {
              // it's a sky hack wall
              isSky = true;
            }
            while (open) {
              if (open->range > 0.0f && hpz >= open->bottom && hpz <= open->top) {
                /*if (!(ld->flags&lineflags))*/ break; // shot continues
              }
              open = open->next;
            }
            blockFlag = !open;
          }
        } else {
          // no opening scan
          blockFlag = false; // 2-sided line doesn't block
        }
      } else {
        // one-sided line always blocks
        blockFlag = true;
      }
    }

    if (blockFlag) {
      RemoveInterceptsAfter(frac); // remove all intercepts we are not interested in
      max_frac = frac;
      wasBlocking = true;
    }

    if (doadd) {
      if (!isSky && ld->special == LNSPEC_LineHorizon) isSky = true;
      //GCon->Logf(NAME_Debug, "001: pathtrace: line #%d; frac=%g; max=%g", (int)(ptrdiff_t)(ld-&Level->Lines[0]), frac, max_frac);
      intercept_t &In = NewIntercept(frac);
      In.Flags = intercept_t::IF_IsALine|(blockFlag ? intercept_t::IF_IsABlockingLine : 0u)|(isSky ? intercept_t::IF_IsASky : 0u);
      In.line = ld;
      In.side = ld->PointOnSide(trace_org3d);
      // set line sector
      if (po) {
        //FIXME: this code is a mess!
             if (po->posector) In.sector = po->posector; // it's ok for puff checks
        else if (ld->frontsector) In.sector = ld->frontsector;
        else In.sector = ld->backsector;
      } else {
        In.sector = (In.side ? ld->backsector : ld->frontsector);
      }
      //GCon->Logf(NAME_Debug, "002: pathtrace: line #%d; frac=%g; max=%g", (int)(ptrdiff_t)(In.line-&Level->Lines[0]), In.frac, max_frac);
    }
  }

  return wasBlocking;
}


//==========================================================================
//
//  CalcProperThingHit
//
//==========================================================================
static bool CalcProperThingHit (TVec &hitPoint, VEntity *th, const TVec &shootOrigin, const TVec &dir, const float distance) {
  const float rad = th->Radius;
  if (rad <= 0.0f) return false;
  if (hitPoint.z > th->Origin.z+max2(0.0f, th->Height)) {
    // trace enters above actor
    if (dir.z >= 0.0f) return false; // going up: can't hit
    // does it hit the top of the actor?
    const float dist = (th->Origin.z+th->Height-shootOrigin.z)/dir.z;
    if (dist > distance) return false;
    //float frac = dist/distance;
    hitPoint = shootOrigin+dir*dist;
    // calculated coordinate is outside the actor's bounding box
    if (fabsf(hitPoint.x-th->Origin.x) > rad ||
        fabsf(hitPoint.y-th->Origin.y) > rad)
    {
      return false;
    }
  } else if (hitPoint.z < th->Origin.z) {
    // trace enters below actor
    if (dir.z <= 0.0f) return false; // going down: can't hit
    // does it hit the bottom of the actor?
    const float dist = (th->Origin.z-shootOrigin.z)/dir.z;
    if (dist > distance) return false;
    //frac = dist/distance;
    hitPoint = shootOrigin+dir*dist;
    // calculated coordinate is outside the actor's bounding box
    if (fabsf(hitPoint.x-th->Origin.x) > rad ||
        fabsf(hitPoint.y-th->Origin.y) > rad)
    {
      return false;
    }
  }
  return true;
}


//==========================================================================
//
//  VPathTraverse::AddProperThingHit
//
//  calculates proper thing hit and so on
//
//==========================================================================
void VPathTraverse::AddProperThingHit (VEntity *th, const float frac) {
  if (frac >= max_frac) return;
  TVec hp = trace_org3d+trace_dir3d*frac;
  if (!(scanflags&(PT_NOOPENS|PT_AIMTHINGS))) {
    if (!CalcProperThingHit(hp, th, trace_org3d, trace_dir3d, trace_len3d)) return;
    if (th->Origin.z+max2(0.0f, th->Height) < hp.z) return; // shot over the thing
    if (th->Origin.z > hp.z) return; // shot under the thing
  }
  intercept_t &In = NewIntercept(frac);
  In.Flags = 0;
  In.thing = th;
  In.hitpoint = hp;
}


//==========================================================================
//
//  VPathTraverse::AddThingIntercepts
//
//==========================================================================
void VPathTraverse::AddThingIntercepts (VThinker *Self, int mapx, int mapy) {
  if (!Self) return;
  //const int tvt = dbg_thing_traverser.asInt();
  #if VV_THING_TRAVERSER == 0
    // best, gz
    divline_t trace;
    trace.x = trace_org.x;
    trace.y = trace_org.y;
    trace.dx = trace_delta.x;
    trace.dy = trace_delta.y;
    divline_t line;
    /*static*/ const int deltas[3] = { 0, -1, 1 };
    for (int dy = 0; dy < 3; ++dy) {
      for (int dx = 0; dx < 3; ++dx) {
        if (!NeedCheckBMCell(mapx+deltas[dx], mapy+deltas[dy])) continue;
        for (VBlockThingsIterator It(Level, mapx+deltas[dx], mapy+deltas[dy]); It; ++It) {
          if (It->Radius <= 0.0f || It->Height <= 0.0f) continue;
          //if (vptSeenThings.put(*It, true)) continue;
          // [RH] don't check a corner to corner crossection for hit
          // instead, check against the actual bounding box

          if (scanflags&(PT_NOOPENS|PT_AIMTHINGS)) {
            // center hitpoint
            const float dot = trace_plane.PointDistance(It->Origin);
            if (fabsf(dot) >= It->Radius) continue; // thing is too far away
            const float dist = DotProduct((It->Origin-trace_org), trace_dir); //dist -= sqrt(It->radius * It->radius - dot * dot);
            if (dist < 0.0f) continue; // behind source
            const float frac = (trace_len ? dist/trace_len : 0.0f);
            if (frac < 0.0f || frac > 1.0f) continue;
            intercept_t &In = NewIntercept(frac);
            In.Flags = 0;
            In.thing = *It;
            continue;
          }

          // there's probably a smarter way to determine which two sides
          // of the thing face the trace than by trying all four sides...
          int numfronts = 0;
          for (int i = 0; i < 4; ++i) {
            switch (i) {
              case 0: // top edge
                line.y = It->Origin.y+It->Radius;
                if (trace_org.y < line.y) continue;
                line.x = It->Origin.x+It->Radius;
                line.dx = -It->Radius*2.0f;
                line.dy = 0.0f;
                break;
              case 1: // right edge
                line.x = It->Origin.x+It->Radius;
                if (trace_org.x < line.x) continue;
                line.y = It->Origin.y-It->Radius;
                line.dx = 0.0f;
                line.dy = It->Radius*2.0f;
                break;
              case 2: // bottom edge
                line.y = It->Origin.y-It->Radius;
                if (trace_org.y > line.y) continue;
                line.x = It->Origin.x-It->Radius;
                line.dx = It->Radius*2.0f;
                line.dy = 0.0f;
                break;
              case 3: // left edge
                line.x = It->Origin.x-It->Radius;
                if (trace_org.x > line.x) continue;
                line.y = It->Origin.y+It->Radius;
                line.dx = 0.0f;
                line.dy = -It->Radius*2.0f;
                break;
            }
            ++numfronts;
            // check if this side is facing the trace origin
            // if it is, see if the trace crosses it
            if (pointOnDLineSide(line.x, line.y, trace) != pointOnDLineSide(line.x+line.dx, line.y+line.dy, trace)) {
              // it's a hit
              const float frac = interceptVector(trace, line);
              if (frac < 0.0f || frac > 1.0f) continue;
              AddProperThingHit(*It, frac);
              break;
            }
          }
          // if none of the sides was facing the trace, then the trace
          // must have started inside the box, so add it as an intercept
          if (numfronts == 0) {
            intercept_t &In = NewIntercept(0.0f);
            In.Flags = 0;
            In.thing = *It;
          }
        }
      }
    }
  #elif VV_THING_TRAVERSER == 1
    // better original
    for (int dy = -1; dy < 2; ++dy) {
      for (int dx = -1; dx < 2; ++dx) {
        if (!NeedCheckBMCell(mapx+dx, mapy+dy)) continue;
        for (VBlockThingsIterator It(Level, mapx+dx, mapy+dy); It; ++It) {
          if (It->Radius <= 0.0f || It->Height <= 0.0f) continue;
          const float dot = trace_plane.PointDistance(It->Origin);
          if (fabsf(dot) >= It->Radius) continue; // thing is too far away
          const float dist = DotProduct((It->Origin-trace_org), trace_dir); //dist -= sqrt(It->radius * It->radius - dot * dot);
          if (dist < 0.0f) continue; // behind source
          const float frac = (trace_len ? dist/trace_len : 0.0f);
          if (frac < 0.0f || frac > 1.0f) continue;
          //if (vptSeenThings.put(*It, true)) continue;
          AddProperThingHit(*It, frac);
        }
      }
    }
  #elif VV_THING_TRAVERSER == 2
    // original
    if (!NeedCheckBMCell(mapx, mapy)) return;
    for (VBlockThingsIterator It(Level, mapx, mapy); It; ++It) {
      if (It->Radius <= 0.0f || It->Height <= 0.0f) continue;
      const float dot = trace_plane.PointDistance(It->Origin);
      if (fabsf(dot) >= It->Radius) continue; // thing is too far away
      const float dist = DotProduct((It->Origin-trace_org), trace_dir); //dist -= sqrt(It->radius * It->radius - dot * dot);
      if (dist < 0.0f) continue; // behind source
      const float frac = (trace_len ? dist/trace_len : 0.0f);
      if (frac < 0.0f || frac > 1.0f) continue;
      AddProperThingHit(*It, frac);
    }
  #else
    #error "VV_THING_TRAVERSER is not properly set!"
  #endif
}


//==========================================================================
//
//  VPathTraverse::GetNext
//
//==========================================================================
bool VPathTraverse::GetNext () {
  if (!Count) return false; // everything was traversed
  --Count;
  //k8: it is already sorted
  *InPtr = In++;
  return true;
}
