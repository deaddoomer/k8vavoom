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
#include "p_entity.h"
#include "p_levelinfo.h"
#include "p_decal.h"
#ifdef CLIENT
# include "../drawer.h"
#endif


static VCvarB dbg_pobj_unstuck_verbose("dbg_pobj_unstuck_verbose", false, "Verbose 3d pobj unstuck code?", CVAR_PreInit|CVAR_Hidden|CVAR_NoShadow);


enum {
  AFF_VERT   = 1u<<0,
  AFF_MOVE   = 1u<<1,
  AFF_STUCK  = 1u<<2,
  //
  AFF_NOTHING_ZERO = 0u,
};

struct SavedEntityData {
  VEntity *mobj;
  TVec Origin;
  TVec LastMoveOrigin;
  unsigned aflags; // AFF_xxx flags, used in movement
  TVec spot; // used in rotator
};

static TArray<SavedEntityData> poAffectedEnitities;


// ////////////////////////////////////////////////////////////////////////// //
struct UnstuckInfo {
  TVec uv; // unstuck vector
  TVec unorm; // unstuck normal
  bool blockedByOther;
  bool normFlipped;
  const line_t *line;
};


//==========================================================================
//
//  entityDataCompareZ
//
//==========================================================================
static int entityDataCompareZ (const void *a, const void *b, void *) noexcept {
  if (a == b) return 0;
  VEntity *ea = ((const SavedEntityData *)a)->mobj;
  VEntity *eb = ((const SavedEntityData *)b)->mobj;
  if (ea->Origin.z < eb->Origin.z) return -1;
  if (ea->Origin.z > eb->Origin.z) return +1;
  return 0;
}


//==========================================================================
//
//  CalcMapUnstuckVector
//
//  collect walls we may stuck in
//
//  returns `false` if stuck, and cannot unstuck
//
//==========================================================================
static void CalcMapUnstuckVector (TArray<UnstuckInfo> &uvlist, VEntity *mobj) {
  // ignore object collisions for now
  uvlist.reset();
  float mybbox[4];

  mobj->Create2DBox(mybbox);

  // collect 1s walls
  {
    VLevel *XLevel = mobj->XLevel;
    DeclareMakeBlockMapCoordsBBox2D(mybbox, xl, yl, xh, yh);
    for (int bx = xl; bx <= xh; ++bx) {
      for (int by = yl; by <= yh; ++by) {
        for (auto &&it : XLevel->allBlockLines(bx, by)) {
          line_t *ld = it.line();
          if (!mobj->IsRealBlockingLine(ld)) continue;
          int side = 0;
          if (ld->backsector && (ld->flags&ML_TWOSIDED)) {
            // two-sided line, check if we can step up
            bool doAdd = false;
            for (unsigned ff = 0; ff < 2; ++ff) {
              const sector_t *sec = (ff ? ld->backsector : ld->frontsector);
              vassert(sec);
              const float fz = sec->floor.GetPointZClamped(mobj->Origin);
              const float zdiff = fz-mobj->Origin.z;
              if (zdiff > 0.0f && zdiff > mobj->MaxStepHeight) {
                doAdd = true;
                break;
              }
              const float cz = sec->ceiling.GetPointZClamped(mobj->Origin);
              if (mobj->Origin.z+mobj->Height > cz) {
                doAdd = true;
                break;
              }
            }
            if (!doAdd) continue;
            side = ld->PointOnSide(mobj->Origin);
          }
          UnstuckInfo &nfo = uvlist.alloc();
          nfo.uv = TVec(0.0f, 0.0f); // unused
          nfo.unorm = (side ? -ld->normal : ld->normal);
          nfo.blockedByOther = false; // unused
          nfo.normFlipped = side;
          nfo.line = ld;
        }
      }
    }
  }
}


//==========================================================================
//
//  CalcPolyUnstuckVector
//
//  calculate horizontal "unstuck vector" (delta to move)
//  for the given 3d polyobject
//
//  returns `false` if stuck, and cannot unstuck
//
//  returns `true` if not stuck, or can unstuck
//  in this case, check `uvlist.length()`, if it is zero, then we aren't stuck
//
//  this is wrong, but is still better than stucking
//
//==========================================================================
static bool CalcPolyUnstuckVector (TArray<UnstuckInfo> &uvlist, polyobj_t *po, VEntity *mobj) {
  uvlist.resetNoDtor();
  //if (!po || !mobj || !po->posector) return false;

  const float rad = mobj->GetMoveRadius();
  if (rad <= 0.0f || mobj->Height <= 0.0f) return true; // just in case
  const float radext = rad+0.2f;

  const TVec mobjOrigOrigin = mobj->Origin;
  const TVec orig2d(mobjOrigOrigin.x, mobjOrigOrigin.y);
  float bbox2d[4];
  mobj->Create2DBox(bbox2d);
  bbox2d[BOX2D_TOP] = orig2d.y+rad;
  bbox2d[BOX2D_BOTTOM] = orig2d.y-rad;
  bbox2d[BOX2D_RIGHT] = orig2d.x+rad;
  bbox2d[BOX2D_LEFT] = orig2d.x-rad;

  // if no "valid" sides to unstuck found, but has some "invalid" ones, try "invalid" sides
  bool wasIntersect = false;
  const float mobjz0 = mobj->Origin.z;
  //const float mobjz1 = mobjz0+mobj->Height;
  const float ptopz = po->poceiling.minz;

  const float crmult[4][2] = {
    { -1.0f, -1.0f }, // bottom-left
    { +1.0f, -1.0f }, // bottom-right
    { -1.0f, +1.0f }, // top-left
    { +1.0f, +1.0f }, // top-right
  };

  const char *crnames[4] = { "bottom-left", "bottom-right", "top-left", "top-right" };

  for (auto &&lit : po->LineFirst()) {
    const line_t *ld = lit.line();

    // if we are above the polyobject, check for blocking top texture
    if (!mobj->Check3DPObjLineBlocked(po, ld)) continue;
    wasIntersect = true;

    const float orgsdist = ld->PointDistance(orig2d);

    // if this is blocking toptex, we need to check side
    // otherwise, we cannot be "inside", and should move out of the front side of the wall
    bool badSide = false;
    if ((ld->flags&ML_CLIP_MIDTEX) != 0 && mobjz0 >= ptopz) {
      badSide = (orgsdist < 0.0f);
    }

    if (dbg_pobj_unstuck_verbose.asBool()) {
      GCon->Logf(NAME_Debug, "mobj '%s': going to unstuck from pobj %d, line #%d, orgsdist=%g; badSide=%d",
        mobj->GetClass()->GetName(), po->tag, (int)(ptrdiff_t)(ld-&mobj->XLevel->Lines[0]), orgsdist, (int)badSide);
    }

    // check 4 corners, find the shortest "unstuck" distance
    bool foundVector = false;
    for (unsigned cridx = 0u; cridx < 4u; ++cridx) {
      TVec corner = orig2d;
      corner += TVec(radext*crmult[cridx][0], radext*crmult[cridx][1], 0.0f);
      float csdist = ld->PointDistance(corner);
      TVec uv;

      if (!badSide) {
        if (csdist > 0.0f) continue;
        csdist -= 0.01f;
        uv = ld->normal*(-csdist);
      } else {
        // 3d midtex
        if (csdist < 0.0f) continue;
        csdist += 0.01f;
      }
      uv = ld->normal*(-csdist);

      // check if we'll stuck in some other pobj line
      bool stuckOther = false;
      {
        mobj->Origin = mobjOrigOrigin+uv;
        for (auto &&lxx : po->LineFirst()) {
          const line_t *lnx = lxx.line();
          if (lnx == ld) continue;
          if (!mobj->Check3DPObjLineBlocked(po, lnx)) continue;
          if (dbg_pobj_unstuck_verbose.asBool()) {
            GCon->Logf(NAME_Debug, "mobj '%s': going to unstuck from pobj %d, line #%d, corner %u (%s); stuck in line #%d",
              mobj->GetClass()->GetName(), po->tag, (int)(ptrdiff_t)(ld-&mobj->XLevel->Lines[0]), cridx, crnames[cridx],
              (int)(ptrdiff_t)(lnx-&mobj->XLevel->Lines[0]));
          }
          stuckOther = true;
          break;
        }
        mobj->Origin = mobjOrigOrigin;
      }

      if (dbg_pobj_unstuck_verbose.asBool()) {
        GCon->Logf(NAME_Debug, "mobj '%s': going to unstuck from pobj %d, line #%d, corner %u (%s); orig2d=(%g,%g); rad=%g (%g); cridx=%u; csdist=%g; uvec=(%g,%g); ulen=%g; unorm=(%g,%g)",
          mobj->GetClass()->GetName(), po->tag, (int)(ptrdiff_t)(ld-&mobj->XLevel->Lines[0]), cridx, crnames[cridx],
          orig2d.x, orig2d.y, rad, radext, cridx, csdist, uv.x, uv.y, uv.length2D(),
          ld->normal.x*(badSide ? -1.0f : 1.0f), ld->normal.y*(badSide ? -1.0f : 1.0f));
      }

      // append to the list
      {
        UnstuckInfo &nfo = uvlist.alloc();
        nfo.uv = uv;
        nfo.unorm = (badSide ? -ld->normal : ld->normal); // pobj sides points to outside
        nfo.blockedByOther = stuckOther;
        nfo.normFlipped = badSide;
        nfo.line = (foundVector ? nullptr : ld);
      }
      foundVector = true;
    }

    if (!foundVector) return false; // oops
  }

  if (wasIntersect) return (uvlist.length() > 0);
  return true;
}


//==========================================================================
//
//  checkCrushMObj
//
//  crush mobj; returns `false` if still blocked
//
//==========================================================================
static bool checkCrushMObj (polyobj_t *po, VEntity *mobj, bool vertical, TVec thrustDir=TVec(0.0f, 0.0f)) {
  if (!mobj || mobj->IsGoingToDie()) return true; // not blocked
  // for solids, `PolyThrustMobj()` will do the job, otherwise call `PolyCrushMobj()`
  if (mobj->EntityFlags&VEntity::EF_Solid) {
    // `PolyThrustMobj()` returns `false` if mobj was killed
    // vertical with 3d pobj means "crush instantly"
    if (mobj->Level->eventPolyThrustMobj(mobj, thrustDir, po, vertical)) return false; // blocked
  } else {
    // non-solid object can't stop us
    mobj->Level->eventPolyCrushMobj(mobj, po);
  }
  // just in case: blocked if not crushed
  return !(mobj->PObjNeedPositionCheck() && mobj->Health > 0 && mobj->Height > 0.0f && mobj->GetMoveRadius() > 0.0f);
}


//==========================================================================
//
//  unstuckVectorCompare
//
//==========================================================================
static int unstuckVectorCompare (const void *aa, const void *bb, void */*udata*/) {
  if (aa == bb) return 0;
  const UnstuckInfo *a = (const UnstuckInfo *)aa;
  const UnstuckInfo *b = (const UnstuckInfo *)bb;
  // sort by length
  const float alensq = a->uv.length2DSquared();
  const float blensq = b->uv.length2DSquared();
  if (alensq < blensq) return -1;
  if (alensq > blensq) return +1;
  return 0;
}


//==========================================================================
//
//  DoUnstuckByAverage
//
//  returns `false` if can't
//
//==========================================================================
static bool DoUnstuckByAverage (TArray<UnstuckInfo> &uvlist, VEntity *mobj) {
  tmtrace_t tmtrace;
  const TVec origOrigin = mobj->Origin;

  // try to unstick by average vector
  TVec nsum(0.0f, 0.0f, 0.0f);
  for (auto &&nfo : uvlist) if (nfo.line) nsum += nfo.unorm;
  nsum.z = 0.0f; // just in case
  if (nsum.isValid() && !nsum.isZero2D()) {
    nsum = nsum.normalise();
    // good unstuck normal
    #if 0
    // calculate distance to move away
    float mybbox[4];
    mobj->Create2DBox(mybbox);
    float ndist = 0.0f;
    for (auto &&nfo : uvlist) {
      if (!nfo.line) continue;
      const bool blocked = mobj->Check3DPObjLineBlocked(nfo.line->pobj(), nfo.line);
      if (!blocked) continue;
      const TVec p0 = nfo.line->get2DBBoxRejectPoint(mybbox);
      const TVec p1 = nfo.line->get2DBBoxAcceptPoint(mybbox);
      const float tm0 = nfo.line->LineIntersectTime(p0, p0+nsum);
      float tm1 = nfo.line->LineIntersectTime(p1, p1+nsum);
      float tm;
      if (!isFiniteF(tm0)) {
        if (!isFiniteF(tm1)) continue;
        tm = fabsf(tm1);
      } else {
        tm = fabsf(tm0);
        if (isFiniteF(tm1)) {
          tm1 = fabsf(tm1);
          if (tm < tm1) tm = tm1;
        }
      }
      if (tm > ndist) ndist = tm;
    }
    if (dbg_pobj_unstuck_verbose.asBool()) {
      GCon->Logf(NAME_Debug, "+++ %s: trying by normal (%g,%g,%g); ndist=%g", mobj->GetClass()->GetName(), nsum.x, nsum.y, nsum.z, ndist);
    }
    // try calculated distance
    if (ndist > 0.0f) {
      ndist += 0.02f;
      mobj->Origin = origOrigin+nsum*ndist;
      bool ok = true;
      for (auto &&nfo : uvlist) {
        if (nfo.line) {
          ok = !mobj->Check3DPObjLineBlocked(nfo.line->pobj(), nfo.line);
          if (!ok) break;
        }
      }
      if (ok) {
        ok = mobj->CheckRelPosition(tmtrace, mobj->Origin, /*noPickups*/true, /*ignoreMonsters*/true, /*ignorePlayers*/true);
      }
      if (ok) return true;
    }
    #endif

    // try to move by 1/3 of radius
    const float maxdist = mobj->GetMoveRadius()/3.0f;
    TVec goodPos(0.0f, 0.0f, 0.0f);
    bool goodPosFound = false;
    if (maxdist > 0.0f) {
      //FIXME: use binary search for now
      float maxlow = 0.0f;
      float maxhigh = maxdist;
      if (dbg_pobj_unstuck_verbose.asBool()) {
        GCon->Logf(NAME_Debug, "%s: trying by normal (%g,%g,%g); maxdist=%g", mobj->GetClass()->GetName(), nsum.x, nsum.y, nsum.z, maxhigh);
      }
      while (maxlow < maxhigh && maxhigh-maxlow > 0.001f) {
        float maxmid = (maxlow+maxhigh)*0.5f;
        mobj->Origin = origOrigin+nsum*maxmid;
        bool ok = true;
        for (auto &&nfo : uvlist) {
          if (nfo.line) {
            ok = !mobj->Check3DPObjLineBlocked(nfo.line->pobj(), nfo.line);
            if (!ok) break;
          }
        }
        if (ok) {
          ok = mobj->CheckRelPosition(tmtrace, mobj->Origin, /*noPickups*/true, /*ignoreMonsters*/true, /*ignorePlayers*/true);
        }
        if (dbg_pobj_unstuck_verbose.asBool()) {
          GCon->Logf(NAME_Debug, "  ok=%d; maxmid=%g; dist=%g", (int)ok, maxmid, (origOrigin-mobj->Origin).length2D());
        }
        if (ok) {
          // not blocked
          const float sqdist = (origOrigin-mobj->Origin).length2DSquared();
          goodPosFound = true;
          goodPos = mobj->Origin;
          if (sqdist <= 0.1f*0.1f) break;
          // shrink
          maxhigh = maxmid;
        } else {
          // blocked
          maxlow = maxmid;
        }
      }
      if (goodPosFound) {
        if (dbg_pobj_unstuck_verbose.asBool()) {
          GCon->Logf(NAME_Debug, "%s: found by normal (%g,%g,%g); dist=%g", mobj->GetClass()->GetName(), nsum.x, nsum.y, nsum.z, (origOrigin-goodPos).length2D());
        }
        return true;
      }
    }
  }

  // try non-average
  // sort unstuck vectors by distance
  smsort_r(uvlist.ptr(), uvlist.length(), sizeof(UnstuckInfo), &unstuckVectorCompare, nullptr);

  // try each unstuck vector
  //bool wasAtLeastOneGood = false;
  for (auto &&nfo : uvlist) {
    if (nfo.blockedByOther) continue; // bad line
    const TVec uv = nfo.uv;
    vassert(uv.isValid());
    if (uv.isZero2D()) continue; // just in case
    //wasAtLeastOneGood = true;
    if (dbg_pobj_unstuck_verbose.asBool()) {
      GCon->Logf(NAME_Debug, "mobj '%s' unstuck move: (%g,%g,%g)", mobj->GetClass()->GetName(), uv.x, uv.y, uv.z);
    }
    // need to move
    //TODO: move any touching objects too
    mobj->Origin = origOrigin+uv;
    const bool ok = mobj->CheckRelPosition(tmtrace, mobj->Origin, /*noPickups*/true, /*ignoreMonsters*/true, /*ignorePlayers*/true);
    if (ok) return true;
  }

  // restore
  mobj->Origin = origOrigin;
  return false;
}


//==========================================================================
//
//  UnstuckFromRotatedPObj
//
//  returns `false` if movement was blocked
//
//==========================================================================
static bool UnstuckFromRotatedPObj (VLevel *Level, polyobj_t *pofirst, bool skipLink) noexcept {
  if (pofirst && !pofirst->posector) pofirst = nullptr; // don't do this for non-3d pobjs

  // ok, it's not blocked; now try to unstuck
  if (!pofirst) return true; // ok to move

  if (poAffectedEnitities.length() == 0) return true; // still ok to move

  tmtrace_t tmtrace;
  TArray<UnstuckInfo> uvlist;

  // check each affected entity against all pobj lines
  for (auto &&edata : poAffectedEnitities) {
    // if it already was stuck, ignore it
    if (edata.aflags&AFF_STUCK) {
      if (dbg_pobj_unstuck_verbose.asBool()) {
        GCon->Logf(NAME_Debug, "mobj '%s' already stuck", edata.mobj->GetClass()->GetName());
      }
      continue;
    }
    VEntity *mobj = edata.mobj;
    if (!mobj || !mobj->PObjNeedPositionCheck()) continue;

    bool doFinalCheck = true;
    // try to unstuck 3 times
    for (int trycount = 3; trycount > 0; --trycount) {
      doFinalCheck = true;
      bool wasMove = false;
      for (polyobj_t *po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
        if (!po || !po->posector) continue;
        // reject only polyobjects that are above us
        // (otherwise we may miss blockig top texture)
        if (mobj->Origin.z+max2(0.0f, mobj->Height) <= po->pofloor.minz) continue;
        // if pobj has no top-blocking textures, it can be skipped if we're above it
        if ((po->PolyFlags&polyobj_t::PF_HasTopBlocking) == 0) {
          if (mobj->Origin.z+max2(0.0f, mobj->Height) <= po->pofloor.minz) continue;
        }

        const bool canUnstuck = CalcPolyUnstuckVector(uvlist, po, mobj);
        if (dbg_pobj_unstuck_verbose.asBool()) {
          GCon->Logf(NAME_Debug, "**** mobj %s:%u, pobj %d, triesleft=%d: canUnstuck=%d; uvlist.length=%d", mobj->GetClass()->GetName(), mobj->GetUniqueId(), po->tag, trycount, (int)canUnstuck, uvlist.length());
        }

        if (!canUnstuck) {
          // oops, blocked, and can't unstuck, crush
          if (!checkCrushMObj(po, mobj, true)) return false; // blocked
          wasMove = false; // get away, we're done with this mobj
          break;
        }

        if (uvlist.length() == 0) continue; // not stuck in this pobj

        if (!DoUnstuckByAverage(uvlist, mobj)) {
          // totally stuck
          if (!checkCrushMObj(po, mobj, true)) return false; // blocked
          wasMove = false; // get away, we're done with this mobj
          break;
        }

        // moved
        wasMove = true;
        edata.aflags |= AFF_MOVE;
      } // polyobject link loop

      if (!wasMove) {
        // wasn't moved, so no need to check for "still stuck"
        doFinalCheck = false;
        break;
      }
    } // unstuck try loop

    // check if we stuck in any map walls
    if (!mobj->IsGoingToDie() && mobj->PObjNeedPositionCheck()) {
      CalcMapUnstuckVector(uvlist, mobj);
      if (uvlist.length()) {
        if (!DoUnstuckByAverage(uvlist, mobj)) return false; // blocked
      }
    }

    // if we got here, it means that either unstucking is failed, or we're ok
    // if `doFinalCheck` flag is set, we should be stuck
    // check it again, just to be sure
    if (doFinalCheck) {
      for (polyobj_t *po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
        // reject only polyobjects that are above us
        // (otherwise we may miss blockig top texture)
        if (mobj->Origin.z+max2(0.0f, mobj->Height) <= po->pofloor.minz) continue;
        // if pobj has no top-blocking textures, it can be skipped if we're above it
        if ((po->PolyFlags&polyobj_t::PF_HasTopBlocking) == 0) {
          if (mobj->Origin.z+max2(0.0f, mobj->Height) <= po->pofloor.minz) continue;
        }
        const bool canUnstuck = CalcPolyUnstuckVector(uvlist, po, mobj);
        // if we can't find unstuck direction, or found at least one, it means that we're stuck
        if (!canUnstuck || uvlist.length() > 0) {
          if (dbg_pobj_unstuck_verbose.asBool()) {
            GCon->Logf(NAME_Debug, "mobj '%s' STILL BLOCKED", edata.mobj->GetClass()->GetName());
          }
          // still blocked
          if (!checkCrushMObj(po, mobj, true)) return false; // blocked
        }
      }
    }
  }

  // ok to move
  return true;
}


//==========================================================================
//
//  CheckAffectedMObjPositions
//
//  returns `true` if movement was blocked
//
//==========================================================================
static bool CheckAffectedMObjPositions (const polyobj_t *pofirst) noexcept {
  if (poAffectedEnitities.length() == 0) return false;
  (void)pofirst;

  tmtrace_t tmtrace;
  for (auto &&edata : poAffectedEnitities) {
    if (!edata.aflags) continue;
    VEntity *mobj = edata.mobj;
    if (!mobj->PObjNeedPositionCheck()) continue;
    // all effected objects were unlinked from the world at this stage
    bool ok = mobj->CheckRelPosition(tmtrace, mobj->Origin, /*noPickups*/true, /*ignoreMonsters*/true, /*ignorePlayers*/true);
    if (!ok) {
      // blocked, abort horizontal movement, and try again
      //FIXME: this can break stacking and such
      #if 0
      if (edata.aflags&AFF_MOVE) {
        if (dbg_pobj_unstuck_verbose.asBool()) {
          GCon->Logf(NAME_Debug, "pobj #%d: HCHECK(0) for %s(%u)", pofirst->tag, mobj->GetClass()->GetName(), mobj->GetUniqueId());
        }
        TVec oo = edata.Origin;
        oo.z = mobj->Origin.z;
        ok = mobj->CheckRelPosition(tmtrace, oo, /*noPickups*/true, /*ignoreMonsters*/true, /*ignorePlayers*/true);
        if (ok) {
          // undo horizontal movement, and allow normal pobj check to crash the object
          if (dbg_pobj_unstuck_verbose.asBool()) {
            GCon->Logf(NAME_Debug, "pobj #%d: HABORT(0) for %s(%u)", pofirst->tag, mobj->GetClass()->GetName(), mobj->GetUniqueId());
          }
          mobj->LastMoveOrigin.z += mobj->Origin.z-oo.z;
          mobj->Origin = oo;
          // link it back, so pobj checker will do its work
          edata.aflags = 0;
          mobj->LinkToWorld();
          continue;
        }
      }
      #endif
      // alas, blocked
      if (dbg_pobj_unstuck_verbose.asBool()) {
        GCon->Logf(NAME_Debug, "pobj #%d: blocked(0) by %s(%u)", pofirst->tag, mobj->GetClass()->GetName(), mobj->GetUniqueId());
      }
      return true;
    }
    //FIXME: process 3d floors
    // check ceiling
    if (mobj->Origin.z+mobj->Height > tmtrace.CeilingZ) {
      // alas, height block
      if (dbg_pobj_unstuck_verbose.asBool()) {
        GCon->Logf(NAME_Debug, "pobj #%d: blocked(1) by %s(%u)", pofirst->tag, mobj->GetClass()->GetName(), mobj->GetUniqueId());
      }
      return true;
    }
    // fix z if we can step up
    const float zdiff = tmtrace.FloorZ-mobj->Origin.z;
    //GCon->Logf(NAME_Debug, "%s(%u): zdiff=%g; z=%g; FloorZ=%g; tr.FloorZ=%g", mobj->GetClass()->GetName(), mobj->GetUniqueId(), zdiff, mobj->Origin.z, mobj->FloorZ, tmtrace.FloorZ);
    if (zdiff <= 0.0f) continue;
    if (zdiff <= mobj->MaxStepHeight) {
      // ok, we can step up
      // let physics engine fix it
      if (!mobj->IsPlayer()) mobj->Origin.z = tmtrace.FloorZ;
      continue;
    }
    // cannot step up, rollback horizontal movement
    #if 0
    if (edata.aflags&AFF_MOVE) {
      //GCon->Logf(NAME_Debug, "pobj #%d: HCHECK(1) for %s(%u)", pofirst->tag, mobj->GetClass()->GetName(), mobj->GetUniqueId());
      TVec oo = edata.Origin;
      oo.z = mobj->Origin.z;
      ok = mobj->CheckRelPosition(tmtrace, oo, /*noPickups*/true, /*ignoreMonsters*/true, /*ignorePlayers*/true);
      if (ok) {
        // undo horizontal movement, and allow normal pobj check to crash the object
        //GCon->Logf(NAME_Debug, "pobj #%d: HABORT(1) for %s(%u)", pofirst->tag, mobj->GetClass()->GetName(), mobj->GetUniqueId());
        mobj->LastMoveOrigin.z += mobj->Origin.z-oo.z;
        mobj->Origin = oo;
        // link it back, so pobj checker will do its work
        edata.aflags = 0;
        mobj->LinkToWorld();
        continue;
      }
    }
    #endif
    // alas, blocked
    if (dbg_pobj_unstuck_verbose.asBool()) {
      GCon->Logf(NAME_Debug, "pobj #%d: blocked(2) by %s(%u)", pofirst->tag, mobj->GetClass()->GetName(), mobj->GetUniqueId());
    }
    return true;
  }

  // done, no blocks
  return false;
}


//==========================================================================
//
//  VLevel::PolyCheckMobjLineBlocking
//
//==========================================================================
bool VLevel::PolyCheckMobjLineBlocking (const line_t *ld, polyobj_t *po, bool /*rotation*/) {
  // check one extra block, as usual
  int top = MapBlock(ld->bbox2d[BOX2D_TOP]-BlockMapOrgY)+1;
  int bottom = MapBlock(ld->bbox2d[BOX2D_BOTTOM]-BlockMapOrgY)-1;
  int left = MapBlock(ld->bbox2d[BOX2D_LEFT]-BlockMapOrgX)-1;
  int right = MapBlock(ld->bbox2d[BOX2D_RIGHT]-BlockMapOrgX)+1;

  if (top < 0 || right < 0 || bottom >= BlockMapHeight || left >= BlockMapWidth) return false;

  if (bottom < 0) bottom = 0;
  if (top >= BlockMapHeight) top = BlockMapHeight-1;
  if (left < 0) left = 0;
  if (right >= BlockMapWidth) right = BlockMapWidth-1;

  bool blocked = false;

  for (int by = bottom*BlockMapWidth; by <= top*BlockMapWidth; by += BlockMapWidth) {
    for (int bx = left; bx <= right; ++bx) {
      for (VEntity *mobj = BlockLinks[by+bx]; mobj; mobj = mobj->BlockMapNext) {
        if (mobj->IsGoingToDie()) continue;
        if (!mobj->PObjNeedPositionCheck()) continue;
        if (!mobj->CheckPObjLineBlocked(po, ld)) continue;

        if (dbg_pobj_unstuck_verbose.asBool()) {
          GCon->Logf(NAME_Debug, "mobj '%s': blocked by pobj %d, line #%d", mobj->GetClass()->GetName(), po->tag, (int)(ptrdiff_t)(ld-&mobj->XLevel->Lines[0]));
        }
        if (!checkCrushMObj(po, mobj, false, ld->normal)) {
          blocked = true;
          break;
        }
      }
    }
  }

  return blocked;
}


//==========================================================================
//
//  VLevel::PolyCheckMobjBlocked
//
//==========================================================================
bool VLevel::PolyCheckMobjBlocked (polyobj_t *po, bool rotation) {
  if (!po || po->numlines == 0) return false;
  bool blocked = false;
  line_t **lineList = po->lines;
  for (int count = po->numlines; count; --count, ++lineList) {
    if (PolyCheckMobjLineBlocking(*lineList, po, rotation)) blocked = true; //k8: break here?
  }
  return blocked;
}


//==========================================================================
//
//  ProcessStackingAffectedDownFrom
//
//  FIXME:this should be optimized!
//
//==========================================================================
static void ProcessStackingAffectedDownFrom (VEntity *mobj, int idx) {
  for (; idx >= 0; --idx) {
    VEntity *other = poAffectedEnitities.ptr()[idx].mobj; // no need to do range checking
    if (!mobj->CollisionWithOther(other)) continue;
    // move other down
    float ndz = (other->Origin.z+other->Height)-mobj->Origin.z;
    vassert(ndz > 0.0f);
    other->Origin.z -= ndz;
    poAffectedEnitities.ptr()[idx].aflags |= AFF_VERT;
    ProcessStackingAffectedDownFrom(other, idx-1);
  }
}


//==========================================================================
//
//  ProcessStackingAffectedUpFrom
//
//  FIXME:this should be optimized!
//
//==========================================================================
static void ProcessStackingAffectedUpFrom (VEntity *mobj, int idx, const unsigned addflg) {
  const int len = poAffectedEnitities.length();
  for (; idx < len; ++idx) {
    VEntity *other = poAffectedEnitities.ptr()[idx].mobj; // no need to do range checking
    if (other->Origin.z >= mobj->Origin.z+mobj->Height) break; // no more
    if (!mobj->CollisionWithOther(other)) continue;
    // move other up
    float ndz = (mobj->Origin.z+mobj->Height)-other->Origin.z;
    vassert(ndz > 0.0f);
    other->Origin.z += ndz;
    poAffectedEnitities.ptr()[idx].aflags |= AFF_VERT|addflg;
    ProcessStackingAffectedUpFrom(other, idx+1, addflg);
  }
}


//==========================================================================
//
//  ProcessStackingAffected
//
//  if there was some vertical movement, check for stacking
//  this can be done faster, but currently i'm ok with this code
//  this doesn't check vertical bounds
//
//  `poAffectedEnitities` should contain the list for checking
//
//==========================================================================
static void ProcessStackingAffected () {
  // sort by z position
  smsort_r(poAffectedEnitities.ptr(), poAffectedEnitities.length(), sizeof(SavedEntityData), &entityDataCompareZ, nullptr);
  const int len = poAffectedEnitities.length();
  for (int eidx = 0; eidx < len; ++eidx) {
    SavedEntityData &edata = poAffectedEnitities.ptr()[eidx]; // no need to do range checking
    VEntity *mobj = edata.mobj;
    if ((edata.aflags&AFF_VERT) == 0) continue; // not moved
    if ((mobj->EntityFlags&(VEntity::EF_ColideWithThings|VEntity::EF_Solid)) != (VEntity::EF_ColideWithThings|VEntity::EF_Solid)) continue; // cannot collide with things
    if (mobj->GetMoveRadius() <= 0.0f || mobj->Height <= 0.0f) continue; // cannot collide with things
    // z delta
    const float dz = mobj->Origin.z-edata.Origin.z;
    if (dz == 0.0f) continue; // just in case, for horizontal movement
    if (dz < 0.0f) {
      // moved down
      ProcessStackingAffectedDownFrom(mobj, eidx-1);
    } else {
      ProcessStackingAffectedUpFrom(mobj, eidx+1, (edata.aflags&AFF_MOVE));
    }
  }
}



//**************************************************************************
//
// polyobject movement code
//
//**************************************************************************

//==========================================================================
//
//  VLevel::MovePolyobj
//
//==========================================================================
bool VLevel::MovePolyobj (int num, float x, float y, float z, unsigned flags) {
  const bool forcedMove = (flags&POFLAG_FORCED); // forced move is like teleport, it will not affect objects
  const bool skipLink = (flags&POFLAG_NOLINK);

  polyobj_t *po = GetPolyobj(num);
  if (!po) { GCon->Logf(NAME_Error, "MovePolyobj: Invalid pobj #%d", num); return false; }

  if (!po->posector) z = 0.0f;

  polyobj_t *pofirst = po;

  const bool verticalMove = (!forcedMove && z != 0.0f);
  const bool horizMove = (!forcedMove && (x != 0.0f || y != 0.0f));
  const bool docarry = (!forcedMove && horizMove && pofirst->posector && !(pofirst->PolyFlags&polyobj_t::PF_NoCarry));
  bool flatsSaved = false;

  // collect `poAffectedEnitities`, and save planes
  // but do this only if we have to
  // only for 3d pobjs that either can carry objects, or moving vertically
  // (because vertical move always pushing objects)
  // "affected entities" are entities that collides with world, can be interacted, and touching our polyobjects
  // world linker sets "touching sector" list for us, so no need to check coordinates or bounding boxes here
  poAffectedEnitities.resetNoDtor();
  if (!forcedMove && pofirst->posector && (docarry || verticalMove)) {
    flatsSaved = true;
    IncrementValidCount();
    const int visCount = validcount;
    //GCon->Logf(NAME_Debug, "=== pobj #%d ===", pofirst->tag);
    for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
      // save flats
      po->savedFloor = po->pofloor;
      po->savedCeiling = po->poceiling;
      // collect objects
      for (msecnode_t *n = po->posector->TouchingThingList; n; n = n->SNext) {
        VEntity *mobj = n->Thing;
        if (mobj->ValidCount == visCount) continue;
        mobj->ValidCount = visCount;
        if (mobj->IsGoingToDie()) continue;
        // check flags
        if ((mobj->EntityFlags&VEntity::EF_ColideWithWorld) == 0) continue;
        if ((mobj->FlagsEx&(VEntity::EFEX_NoInteraction|VEntity::EFEX_NoTickGrav)) == VEntity::EFEX_NoInteraction) continue;
        //FIXME: ignore everything that cannot possibly be affected?
        //GCon->Logf(NAME_Debug, "  %s(%u): z=%g (poz1=%g); sector=%p; basesector=%p", mobj->GetClass()->GetName(), mobj->GetUniqueId(), mobj->Origin.z, pofirst->poceiling.maxz, mobj->Sector, mobj->BaseSector);
        SavedEntityData &edata = poAffectedEnitities.alloc();
        edata.mobj = mobj;
        edata.Origin = mobj->Origin;
        edata.LastMoveOrigin = mobj->LastMoveOrigin;
        edata.aflags = 0;
      }
    }

    // setup "affected" flags, calculate new z
    //const bool verticalMoveUp = (verticalMove && z > 0.0f);
    bool wasVertMovement = false; // set only if some solid thing was moved
    for (auto &&edata : poAffectedEnitities) {
      VEntity *mobj = edata.mobj;
      // reset valid count, so we can avoid incrementing it
      mobj->ValidCount = 0;
      // check for vertical movement
      for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
        const float oldz = mobj->Origin.z;
        const float pofz = po->poceiling.maxz;
        // fix "affected" flag
        // vertical move goes up, and the platform goes through the object?
        if (verticalMove && oldz > pofz && oldz < pofz+z) {
          // yeah, carry it, but only if the platform is moving up
          // if the platform is moving down, it should be blocked, or the object should be crushed
          //FIXME: move down if possible instead of crush -- check this later
          /*if (verticalMoveUp)*/ {
            edata.aflags = AFF_VERT|(docarry ? AFF_MOVE : AFF_NOTHING_ZERO);
            mobj->Origin.z = pofz+z;
            if (!wasVertMovement) {
              wasVertMovement =
                (mobj->EntityFlags&(VEntity::EF_ColideWithThings|VEntity::EF_Solid)) == (VEntity::EF_ColideWithThings|VEntity::EF_Solid) &&
                mobj->GetMoveRadius() > 0.0f && mobj->Height > 0.0f;
            }
          }
        } else if (oldz == pofz) {
          // the object is standing on the pobj ceiling (i.e. floor ;-)
          // always carry, and always move with the pobj, even if it is flying
          edata.aflags = AFF_VERT|(docarry ? AFF_MOVE : AFF_NOTHING_ZERO);
          mobj->Origin.z = pofz+z;
          if (!wasVertMovement) {
            wasVertMovement =
              (mobj->EntityFlags&(VEntity::EF_ColideWithThings|VEntity::EF_Solid)) == (VEntity::EF_ColideWithThings|VEntity::EF_Solid) &&
              mobj->GetMoveRadius() > 0.0f && mobj->Height > 0.0f;
          }
        }
      }
    }

    // if there was some vertical movement, check for stacking
    if (wasVertMovement) ProcessStackingAffected();

    // now unlink all affected objects, because we'll do "move and check" later
    // also, move objects horizontally, because why not
    const TVec xymove(x, y);
    for (auto &&edata : poAffectedEnitities) {
      VEntity *mobj = edata.mobj;
      if (edata.aflags) {
        mobj->UnlinkFromWorld();
        if (edata.aflags&AFF_MOVE) mobj->Origin += xymove;
      }
    }
  }

  const bool dovmove = (z != 0.0f); // cannot use `verticalMove` here
  // unlink and move all linked polyobjects
  for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
    UnlinkPolyobj(po);
    const TVec delta(po->startSpot.x+x, po->startSpot.y+y);
    const float an = AngleMod(po->angle);
    float s, c;
    msincos(an, &s, &c);
    // save previous points, move horizontally
    const TVec *origPts = po->originalPts;
    TVec *prevPts = po->prevPts;
    TVec **vptr = po->segPts;
    for (int f = po->segPtsCount; f--; ++vptr, ++origPts, ++prevPts) {
      *prevPts = **vptr;
      //**vptr = (*origPts)+delta;
      TVec np(*origPts);
      // get the original X and Y values
      const float tr_x = np.x;
      const float tr_y = np.y;
      // calculate the new X and Y values
      np.x = (tr_x*c-tr_y*s)+delta.x;
      np.y = (tr_y*c+tr_x*s)+delta.y;
      **vptr = np;
    }
    UpdatePolySegs(po);
    if (dovmove) OffsetPolyobjFlats(po, z, false);
  }

  // now check if movement is blocked by any affected object
  // we have to do it after we unlinked all pobjs
  bool blocked = false;

  if (!forcedMove && pofirst->posector) {
    if (!blocked) blocked = !UnstuckFromRotatedPObj(this, pofirst, skipLink);
    if (!blocked) blocked = CheckAffectedMObjPositions(pofirst);
    if (dbg_pobj_unstuck_verbose.asBool()) {
      GCon->Logf(NAME_Debug, "pobj #%d: MOVEMENT: blocked=%d", pofirst->tag, (int)blocked);
    }
  }

  if (!forcedMove && !blocked) {
    for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
      blocked = PolyCheckMobjBlocked(po, false);
      if (dbg_pobj_unstuck_verbose.asBool()) {
        GCon->Logf(NAME_Debug, "pobj #%d: MOVEMENT(1): blocked=%d", po->tag, (int)blocked);
      }
      if (blocked) break; // process all?
    }
  }

  bool linked = false;

  // if not blocked, relink polyobject temporarily, and check vertical hits
  if (!blocked && !forcedMove && pofirst->posector) {
    for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) LinkPolyobj(po, false); // don't relink mobjs
    Link3DPolyobjMObjs(pofirst, skipLink);
    linked = true;
    // check height-blocking objects
    // note that `Link3DPolyobjMObjs()` collected all touching mobjs in `poRoughEntityList` for us
    for (VEntity *mobj : poRoughEntityList) {
      // check flags
      if (!mobj->PObjNeedPositionCheck()) continue;
      float mobjbb2d[4];
      mobj->Create2DBox(mobjbb2d);
      const float mz0 = mobj->Origin.z;
      const float mz1 = mz0+mobj->Height;
      // check all polyobjects
      for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
        //if (!IsAABBInside2DBBox(mobj->Origin.x, mobj->Origin.y, mobj->GetMoveRadius(), po->bbox2d)) continue;
        const float pz0 = po->pofloor.minz;
        if (mz1 <= pz0) continue;
        // if pobj has no top-blocking textures, it can be skipped if we're above it
        if ((po->PolyFlags&polyobj_t::PF_HasTopBlocking) == 0) {
          const float pz1 = po->poceiling.maxz;
          if (mz0 >= pz1) continue;
        }
        if (!Are2DBBoxesOverlap(mobjbb2d, po->bbox2d)) continue;
        // possible vertical intersection, check pobj lines
        bool blk = false;
        line_t **lineList = po->lines;
        for (int count = po->numlines; count; --count, ++lineList) {
          if (!mobj->CheckPObjLineBlocked(po, *lineList)) continue;
          blk = true;
          break;
        }
        if (blk) {
          if (!checkCrushMObj(po, mobj, false)) {
            blocked = true;
            break;
          }
        } else {
          const float pz1 = po->poceiling.maxz;
          if (mz0 >= pz1) continue;
          if (IsBBox2DTouching3DPolyObj(po, mobjbb2d)) {
            if (!checkCrushMObj(po, mobj, true)) {
              blocked = true;
              break;
            }
          }
        }
      }
      if (blocked) break;
    }
    // if blocked, unlink back
    if (blocked) {
      for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
        UnlinkPolyobj(po);
      }
      linked = false;
    }
  }

  // restore position if blocked
  if (blocked) {
    vassert(!linked);
    // undo polyobject movement, and link them back
    for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
      if (flatsSaved) {
        po->pofloor = po->savedFloor;
        po->poceiling = po->savedCeiling;
      }
      // restore points
      TVec *prevPts = po->prevPts;
      TVec **vptr = po->segPts;
      for (int f = po->segPtsCount; f--; ++vptr, ++prevPts) **vptr = *prevPts;
      UpdatePolySegs(po);
      OffsetPolyobjFlats(po, 0.0f, true);
      LinkPolyobj(po, false); // do not relink mobjs
    }
    // relink mobjs
    Link3DPolyobjMObjs(pofirst, skipLink);
    // restore and relink all mobjs back
    for (auto &&edata : poAffectedEnitities) {
      VEntity *mobj = edata.mobj;
      if (!mobj->IsGoingToDie() && edata.aflags) {
        mobj->LastMoveOrigin = edata.LastMoveOrigin;
        mobj->Origin = edata.Origin;
        mobj->LinkToWorld();
      }
    }
    return false;
  }

  // succesfull move, fix startspot
  for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
    po->startSpot.x += x;
    po->startSpot.y += y;
    po->startSpot.z += z;
    OffsetPolyobjFlats(po, 0.0f, true);
    if (!linked) LinkPolyobj(po, false); // do not relink mobjs
    // move decals
    if (!forcedMove && po->Is3D() && subsectorDecalList) {
      for (subsector_t *posub = po->GetSector()->subsectors; posub; posub = posub->seclink) {
        const unsigned psnum = (unsigned)(ptrdiff_t)(posub-&Subsectors[0]);
        VDecalList *lst = &subsectorDecalList[psnum];
        for (decal_t *dc = lst->head; dc; dc = dc->subnext) {
          dc->worldx += x;
          dc->worldy += y;
        }
      }
    }
  }
  // relink mobjs
  Link3DPolyobjMObjs(pofirst, skipLink);

  // relink all mobjs back
  for (auto &&edata : poAffectedEnitities) {
    VEntity *mobj = edata.mobj;
    if (!mobj->IsGoingToDie() && edata.aflags) {
      mobj->LastMoveOrigin += mobj->Origin-edata.Origin;
      mobj->LinkToWorld();
    }
  }

  // notify renderer that this polyobject was moved
  #ifdef CLIENT
  if (Renderer) {
    for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
      Renderer->PObjModified(po);
      Renderer->InvalidatePObjLMaps(po);
    }
  }
  #endif

  return true;
}



//**************************************************************************
//
// polyobject rotation code
//
//**************************************************************************

//==========================================================================
//
//  VLevel::RotatePolyobj
//
//==========================================================================
bool VLevel::RotatePolyobj (int num, float angle, unsigned flags) {
  const bool forcedMove = (flags&POFLAG_FORCED);
  const bool skipLink = (flags&POFLAG_NOLINK);
  const bool indRot = (flags&POFLAG_INDROT);

  // get the polyobject
  polyobj_t *po = GetPolyobj(num);
  if (!po) { GCon->Logf(NAME_Error, "RotatePolyobj: Invalid pobj #%d", num); return false; }

  polyobj_t *pofirst = po;

  const bool docarry = (!forcedMove && pofirst->posector && !(po->PolyFlags&polyobj_t::PF_NoCarry));
  const bool doangle = (!forcedMove && pofirst->posector && !(po->PolyFlags&polyobj_t::PF_NoAngleChange));
  const bool flatsSaved = (!forcedMove && pofirst->posector);

  float s, c;

  // collect objects we need to move/rotate
  poAffectedEnitities.resetNoDtor();
  if (!forcedMove && pofirst->posector && (docarry || doangle)) {
    IncrementValidCount();
    const int visCount = validcount;
    for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
      //const float pz1 = po->poceiling.maxz;
      for (msecnode_t *n = po->posector->TouchingThingList; n; n = n->SNext) {
        VEntity *mobj = n->Thing;
        if (mobj->ValidCount == visCount) continue;
        mobj->ValidCount = visCount;
        if (mobj->IsGoingToDie()) continue;
        if (dbg_pobj_unstuck_verbose.asBool()) {
          GCon->Logf(NAME_Debug, "ROTATION: pobj #%d: checking mobj '%s'", po->tag, mobj->GetClass()->GetName());
        }
        // check flags
        if ((mobj->EntityFlags&VEntity::EF_ColideWithWorld) == 0) continue;
        if ((mobj->FlagsEx&(VEntity::EFEX_NoInteraction|VEntity::EFEX_NoTickGrav)) == VEntity::EFEX_NoInteraction) continue;
        SavedEntityData &edata = poAffectedEnitities.alloc();
        edata.mobj = mobj;
        edata.Origin = mobj->Origin;
        edata.LastMoveOrigin = mobj->LastMoveOrigin;
        edata.aflags = 0;
      }
    }

    // flag objects we should carry
    for (auto &&edata : poAffectedEnitities) {
      VEntity *mobj = edata.mobj;
      for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
        // check if it is initially stuck
        #if 1
        if ((edata.aflags&AFF_STUCK) == 0 && mobj->PObjNeedPositionCheck()) {
          for (auto &&lxx : po->LineFirst()) {
            const line_t *ld = lxx.line();
            if (mobj->CheckPObjLineBlocked(po, ld)) {
              edata.aflags |= AFF_STUCK;
              break;
            }
          }
        }
        #endif
        if ((edata.aflags&AFF_MOVE) == 0) {
          const float pz1 = po->poceiling.maxz;
          if (mobj->Origin.z != pz1) continue;
          // we need to remember rotation point
          //FIXME: this will glitch with objects standing on some multipart platforms
          edata.spot = (indRot ? po->startSpot : pofirst->startSpot); // mobj->Sector->ownpobj
          edata.aflags |= AFF_MOVE;
          if (dbg_pobj_unstuck_verbose.asBool()) {
            GCon->Logf(NAME_Debug, "ROTATION: pobj #%d: mobj '%s' is affected", po->tag, mobj->GetClass()->GetName());
          }
        }
      }
    }

    // now unlink all affected objects, because we'll do "move and check" later
    // also, rotate the objects
    msincos(AngleMod(angle), &s, &c);
    for (auto &&edata : poAffectedEnitities) {
      if ((edata.aflags&AFF_MOVE) == 0) continue; // no need to move it
      VEntity *mobj = edata.mobj;
      mobj->UnlinkFromWorld();
      if (docarry) {
        const float ssx = edata.spot.x;
        const float ssy = edata.spot.y;
        // rotate around polyobject spot point
        const float xc = mobj->Origin.x-ssx;
        const float yc = mobj->Origin.y-ssy;
        //GCon->Logf(NAME_Debug, "%s(%u): oldrelpos=(%g,%g)", mobj->GetClass()->GetName(), mobj->GetUniqueId(), xc, yc);
        // calculate the new X and Y values
        const float nx = (xc*c-yc*s);
        const float ny = (yc*c+xc*s);
        //GCon->Logf(NAME_Debug, "%s(%u): newrelpos=(%g,%g)", mobj->GetClass()->GetName(), mobj->GetUniqueId(), nx, ny);
        const float dx = (nx+ssx)-mobj->Origin.x;
        const float dy = (ny+ssy)-mobj->Origin.y;
        if (dx != 0.0f || dy != 0.0f) {
          mobj->Origin.x += dx;
          mobj->Origin.y += dy;
          //edata.aflags |= AFF_MOVE;
        }
      }
    }
  }

  // rotate all polyobjects
  for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
    /*if (IsForServer())*/ UnlinkPolyobj(po);
    po->origStartSpot = po->startSpot;
    if (!indRot && po != pofirst) {
      // rotate this object's starting spot around the main object starting spot
      msincos(angle, &s, &c);
      const float sp_x = po->startSpot.x-pofirst->startSpot.x;
      const float sp_y = po->startSpot.y-pofirst->startSpot.y;
      // calculate the new X and Y values
      const float nx = (sp_x*c-sp_y*s)+pofirst->startSpot.x;
      const float ny = (sp_y*c+sp_x*s)+pofirst->startSpot.y;
      po->startSpot.x = nx;
      po->startSpot.y = ny;
    }
    const float ssx = po->startSpot.x;
    const float ssy = po->startSpot.y;
    const float an = AngleMod(po->angle+angle);
    msincos(an, &s, &c);
    const TVec *origPts = po->originalPts;
    TVec *prevPts = po->prevPts;
    TVec **vptr = po->segPts;
    for (int f = po->segPtsCount; f--; ++vptr, ++prevPts, ++origPts) {
      if (flatsSaved) {
        po->savedFloor = po->pofloor;
        po->savedCeiling = po->poceiling;
      }
      // save the previous point
      *prevPts = **vptr;
      // get the original X and Y values
      float tr_x = origPts->x;
      float tr_y = origPts->y;
      /*
      if (!indRot && po != pofirst) {
        tr_x += po->startSpot.x-ssx;
        tr_y += po->startSpot.y-ssy;
      }
      */
      // calculate the new X and Y values
      (*vptr)->x = (tr_x*c-tr_y*s)+ssx;
      (*vptr)->y = (tr_y*c+tr_x*s)+ssy;
    }
    UpdatePolySegs(po);
  }

  bool blocked = false;

  if (!forcedMove) {
    // now check if movement is blocked by any affected object
    // we have to do it after we unlinked all pobjs
    if (!forcedMove && pofirst->posector) {
      if (!blocked) blocked = !UnstuckFromRotatedPObj(this, pofirst, skipLink);
      if (!blocked) blocked = CheckAffectedMObjPositions(pofirst);
      if (dbg_pobj_unstuck_verbose.asBool()) {
        GCon->Logf(NAME_Debug, "pobj #%d: ROTATION: blocked=%d", pofirst->tag, (int)blocked);
      }
    }
    if (!blocked) {
      for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
        blocked = PolyCheckMobjBlocked(po, true);
        if (dbg_pobj_unstuck_verbose.asBool()) {
          GCon->Logf(NAME_Debug, "pobj #%d: ROTATION(1): blocked=%d", po->tag, (int)blocked);
        }
        if (blocked) break; // process all?
      }
    }
  }

  // if we are blocked then restore the previous points
  if (blocked) {
    // restore points
    for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
      if (flatsSaved) {
        po->pofloor = po->savedFloor;
        po->poceiling = po->savedCeiling;
      }
      po->startSpot = po->origStartSpot;
      TVec *prevPts = po->prevPts;
      TVec **vptr = po->segPts;
      for (int f = po->segPtsCount; f--; ++vptr, ++prevPts) **vptr = *prevPts;
      UpdatePolySegs(po);
      LinkPolyobj(po, false); // do not relink mobjs
    }
    // relink mobjs
    Link3DPolyobjMObjs(pofirst, skipLink);
    // restore and relink all mobjs back
    for (auto &&edata : poAffectedEnitities) {
      VEntity *mobj = edata.mobj;
      if (!mobj->IsGoingToDie()) {
        mobj->LastMoveOrigin = edata.LastMoveOrigin;
        mobj->Origin = edata.Origin;
        mobj->LinkToWorld();
      }
    }
    return false;
  }

  // not blocked, fix angles and floors, rotate decals
  // also, fix starting spots for non-independent rotation
  for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
    po->angle = AngleMod(po->angle+angle);
    if (flatsSaved) {
      po->pofloor.BaseAngle = AngleMod(po->pofloor.BaseAngle+angle);
      po->poceiling.BaseAngle = AngleMod(po->poceiling.BaseAngle+angle);
    } else {
      // on loading, our baseangle is not set
      //FIXME: wrong with initially rotated floors
      po->pofloor.BaseAngle = po->poceiling.BaseAngle = AngleMod(po->angle);
    }
    OffsetPolyobjFlats(po, 0.0f, true);
    LinkPolyobj(po, false);
    // move decals
    if (!forcedMove && po->Is3D() && subsectorDecalList) {
      for (subsector_t *posub = po->GetSector()->subsectors; posub; posub = posub->seclink) {
        const unsigned psnum = (unsigned)(ptrdiff_t)(posub-&Subsectors[0]);
        VDecalList *lst = &subsectorDecalList[psnum];
        if (!lst->head) continue;
        msincos(AngleMod(angle), &s, &c);
        const float ssx = (indRot ? po->startSpot.x : pofirst->startSpot.x);
        const float ssy = (indRot ? po->startSpot.y : pofirst->startSpot.y);
        for (decal_t *dc = lst->head; dc; dc = dc->subnext) {
          const float xc = dc->worldx-ssx;
          const float yc = dc->worldy-ssy;
          const float nx = (xc*c-yc*s);
          const float ny = (yc*c+xc*s);
          dc->worldx = nx+ssx;
          dc->worldy = ny+ssy;
          dc->angle = AngleMod(dc->angle+angle);
        }
      }
    }
  }
  // relink mobjs
  Link3DPolyobjMObjs(pofirst, skipLink);

  // relink things
  if (!forcedMove && poAffectedEnitities.length()) {
    for (auto &&edata : poAffectedEnitities) {
      VEntity *mobj = edata.mobj;
      if (mobj->IsGoingToDie()) continue;
      if ((edata.aflags&AFF_MOVE) != 0) {
        if (doangle) mobj->Level->eventPolyRotateMobj(mobj, angle);
      }
      mobj->LastMoveOrigin += mobj->Origin-edata.Origin;
      mobj->LinkToWorld();
    }
  }

  // notify renderer that this polyobject was moved
  #ifdef CLIENT
  if (Renderer) {
    for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
      Renderer->PObjModified(po);
      Renderer->InvalidatePObjLMaps(po);
    }
  }
  #endif

  return true;
}
