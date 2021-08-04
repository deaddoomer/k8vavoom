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


static VCvarB dbg_pobj_unstuck_verbose("__dbg_pobj_unstuck_verbose", false, "Verbose 3d pobj unstuck code?", CVAR_PreInit);


enum {
  AFF_VERT  = 1u<<0,
  AFF_MOVE  = 1u<<1,
  AFF_STUCK = 1u<<2,
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
  bool fromWrongSide;
  bool blockedByOther;
};



//==========================================================================
//
//  check3DPObjLineBlocked
//
//==========================================================================
static VVA_ALWAYS_INLINE bool check3DPObjLineBlocked (const polyobj_t *po, VEntity *mobj, const line_t *ld) {
  const float mobjz0 = mobj->Origin.z;
  const float ptopz = po->poceiling.minz;
  if (mobjz0 >= ptopz) {
    if ((ld->flags&ML_CLIP_MIDTEX) == 0) return false;
    if (!mobj->IsBlocking3DPobjLineTop(ld)) return false;
    if (!mobj->LineIntersects(ld)) return false;
    // side doesn't matter, as it is guaranteed that both sides have the texture with the same height
    const side_t *tside = &mobj->XLevel->Sides[ld->sidenum[0]];
    if (tside->TopTexture <= 0) return false; // wtf?!
    VTexture *ttex = GTextureManager(tside->TopTexture);
    if (!ttex || ttex->Type == TEXTYPE_Null) return false; // wtf?!
    const float texh = ttex->GetScaledHeightF()/tside->Top.ScaleY;
    if (mobjz0 >= ptopz+texh) return false; // didn't hit top texture
  } else {
    if (!mobj->IsBlockingLine(ld)) return false;
    if (!mobj->LineIntersects(ld)) return false;
  }
  return true;
}


//==========================================================================
//
//  checkPObjLineBlocked
//
//==========================================================================
static VVA_ALWAYS_INLINE bool checkPObjLineBlocked (const polyobj_t *po, VEntity *mobj, const line_t *ld) {
  if (po->posector) {
    if (mobj->Origin.z >= po->poceiling.maxz) return false; // fully above
    if (mobj->Origin.z+max2(0.0f, mobj->Height) <= po->pofloor.minz) return false; // fully below
    return check3DPObjLineBlocked(po, mobj, ld);
  } else {
    // check for non-3d pobj with midtex
    if ((ld->flags&(ML_TWOSIDED|ML_3DMIDTEX)) == (ML_TWOSIDED|ML_3DMIDTEX)) {
      if (!mobj->LineIntersects(ld)) return false;
      // use front side
      const int side = 0; //ld->PointOnSide(mobj->Origin);
      float pz0 = 0.0f, pz1 = 0.0f;
      if (!mobj->XLevel->GetMidTexturePosition(ld, side, &pz0, &pz1)) return false; // no middle texture
      if (mobj->Origin.z >= pz1 || mobj->Origin.z+max2(0.0f, mobj->Height) <= pz0) return false; // no collision
      //ldblock = true;
    } else {
      if (!mobj->IsBlockingLine(ld)) return false;
      if (!mobj->LineIntersects(ld)) return false;
    }
  }
  return true;
}


//==========================================================================
//
//  CalcPolyUnstuckVectorNew
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
static bool CalcPolyUnstuckVector (TArray<UnstuckInfo> &uvlist, VLevel *Level, polyobj_t *po, VEntity *mobj) {
  uvlist.resetNoDtor();
  //if (!po || !mobj || !po->posector) return false;

  const float rad = mobj->Radius;
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

  int mobjIsInside = 0; // didn't checked yet; -1: outside; 1: inside

  // if no "valid" sides to unstuck found, but has some "invalid" ones, try "invalid" sides
  bool wasIntersect = false;
  const float mobjz0 = mobj->Origin.z;
  //const float mobjz1 = mobjz0+mobj->Height;
  const float ptopz = po->poceiling.minz;
  const bool checkTopTex = (mobjz0 >= ptopz);

  const float crmult[4][2] = {
    { -1.0f, -1.0f }, // bottom-left
    { +1.0f, -1.0f }, // bottom-right
    { -1.0f, +1.0f }, // top-left
    { +1.0f, +1.0f }, // top-right
  };

  for (auto &&lit : po->LineFirst()) {
    const line_t *ld = lit.line();

    // if we are above the polyobject, check for blocking top texture
    if (!check3DPObjLineBlocked(po, mobj, ld)) continue;
    wasIntersect = true;

    // check if we're inside (we need this to determine the right line side)
    //FIXME: this may be wrong for huge angles, because after rotation the object could move from outside to inside, or vice versa
    if (!mobjIsInside) {
      mobjIsInside = (Level->Is2DPointInside3DPolyObj(po, mobjOrigOrigin.x, mobjOrigOrigin.y) ? 1 : -1);
    }

    const float orgsdist = ld->PointDistance(orig2d);

    if (dbg_pobj_unstuck_verbose.asBool()) {
      GCon->Logf(NAME_Debug, "mobj '%s': going to unstuck from pobj %d, line #%d, orgsdist=%g; checkTopTex=%d; inside=%d",
        mobj->GetClass()->GetName(), po->tag, (int)(ptrdiff_t)(ld-&Level->Lines[0]), orgsdist, (int)checkTopTex, mobjIsInside);
    }

    const bool badSide = (mobjIsInside > 0 ? (checkTopTex ? (orgsdist > 0.0f) : (orgsdist < 0.0f)) : (orgsdist < 0.0f));

    // check 4 corners, find the shortest "unstuck" distance
    bool foundVector = false;
    for (unsigned cridx = 0u; cridx < 4u; ++cridx) {
      TVec corner = orig2d;
      corner += TVec(radext*crmult[cridx][0], radext*crmult[cridx][1], 0.0f);
      const float csdist = ld->PointDistance(corner);

      if (checkTopTex) {
        if (csdist <= 0.0f) continue;
      } else {
        if (csdist >= 0.0f) continue;
      }
      const TVec uv = ld->normal*(-csdist);

      // check if we'll stuck in some other pobj line
      bool stuckOther = false;
      {
        mobj->Origin = mobjOrigOrigin+uv;
        for (auto &&lxx : po->LineFirst()) {
          const line_t *lnx = lxx.line();
          if (lnx == ld) continue;
          if (!check3DPObjLineBlocked(po, mobj, lnx)) continue;
          if (dbg_pobj_unstuck_verbose.asBool()) {
            GCon->Logf(NAME_Debug, "mobj '%s': going to unstuck from pobj %d, line #%d, corner %u; stuck in line #%d",
              mobj->GetClass()->GetName(), po->tag, (int)(ptrdiff_t)(ld-&Level->Lines[0]), cridx, (int)(ptrdiff_t)(lnx-&Level->Lines[0]));
          }
          stuckOther = true;
          break;
        }
        mobj->Origin = mobjOrigOrigin;
      }

      if (dbg_pobj_unstuck_verbose.asBool()) {
        GCon->Logf(NAME_Debug, "mobj '%s': going to unstuck from pobj %d, line #%d; orig2d=(%g,%g); rad=%g (%g); cridx=%u; csdist=%g; uvec=(%g,%g); ulen=%g",
          mobj->GetClass()->GetName(), po->tag, (int)(ptrdiff_t)(ld-&Level->Lines[0]),
          orig2d.x, orig2d.y, rad, radext, cridx, csdist, uv.x, uv.y, uv.length2D());
      }

      // append to the list
      foundVector = true;
      {
        UnstuckInfo &nfo = uvlist.alloc();
        nfo.uv = uv;
        nfo.unorm = ld->normal; // pobj sides points to outside
        nfo.fromWrongSide = badSide;
        nfo.blockedByOther = stuckOther;
      }
    }

    if (!foundVector) return false; // oops
  }

  if (wasIntersect) return (uvlist.length() > 0);
  return true;
}


//==========================================================================
//
//  NeedPositionCheck
//
//==========================================================================
static inline bool NeedPositionCheck (VEntity *mobj) noexcept {
  if (mobj->IsGoingToDie()) return false; // just in case
  if ((mobj->FlagsEx&(VEntity::EFEX_NoInteraction|VEntity::EFEX_NoTickGrav)) == VEntity::EFEX_NoInteraction) return false;
  if (mobj->Height < 1.0f || mobj->Radius < 1.0f) return false;
  return
    (mobj->EntityFlags&(
      VEntity::EF_Solid|
      VEntity::EF_NoSector|
      VEntity::EF_NoBlockmap|
      VEntity::EF_ColideWithWorld|
      VEntity::EF_Blasted|
      VEntity::EF_Corpse|
      VEntity::EF_Invisible)) == (VEntity::EF_Solid|VEntity::EF_ColideWithWorld);
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
  // "wrong sides" should be tried last
  if (a->fromWrongSide != b->fromWrongSide) {
    return (a->fromWrongSide ? -1 : +1);
  }
  // sort by length
  const float alensq = a->uv.length2DSquared();
  const float blensq = b->uv.length2DSquared();
  if (alensq < blensq) return -1;
  if (alensq > blensq) return +1;
  return 0;
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
    if (!mobj || !NeedPositionCheck(mobj)) continue;

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

        const bool canUnstuck = CalcPolyUnstuckVector(uvlist, Level, po, mobj);
        if (dbg_pobj_unstuck_verbose.asBool()) {
          GCon->Logf(NAME_Debug, "mobj %s:%u, pobj %d, triesleft=%d: canUnstuck=%d; uvlist.length=%d", mobj->GetClass()->GetName(), mobj->GetUniqueId(), po->tag, trycount, (int)canUnstuck, uvlist.length());
        }

        if (!canUnstuck) {
          // oops, blocked, and can't unstuck, crush
          if (mobj->EntityFlags&VEntity::EF_Solid) {
            if (mobj->Level->eventPolyThrustMobj(mobj, TVec(0.0f, 0.0f), po, true/*vertical*/)) return false; // blocked
          } else {
            mobj->Level->eventPolyCrushMobj(mobj, po);
            if (NeedPositionCheck(mobj) && mobj->Health > 0) return false; // blocked if not crushed
          }
          wasMove = false; // get away, we're done with this mobj
          break;
        }

        if (uvlist.length() == 0) continue; // not stuck in this pobj

        const TVec origOrigin = mobj->Origin;
        bool foundGoodUnstuck = false;

        // try to unstick by average vector
        bool tryNonAverage = true;
        {
          TVec nsum(0.0f, 0.0f, 0.0f);
          for (auto &&nfo : uvlist) {
            TVec n = nfo.unorm;
            if (nfo.fromWrongSide) n = -n;
            nsum += n;
          }
          nsum.z = 0.0f; // just in case
          nsum = nsum.normalised();
          if (!nsum.isZero2D()) {
            // good unstuck normal
            // try to move by 1/3 of radius
            const float maxdist = mobj->Radius/3.0f;
            TVec goodPos(0.0f, 0.0f, 0.0f);
            bool goodPosFound = false;
            if (maxdist > 0.0f) {
              //FIXME: use binary search for now
              float maxlow = 0.0f;
              float maxhigh = maxdist;
              #if 0
              GCon->Logf(NAME_Debug, "%s: trying by normal (%g,%g,%g); maxdist=%g", mobj->GetClass()->GetName(), nsum.x, nsum.y, nsum.z, maxhigh);
              #endif
              while (maxlow < maxhigh && maxhigh-maxlow > 0.001f) {
                float maxmid = (maxlow+maxhigh)*0.5f;
                mobj->Origin = origOrigin+nsum*maxmid;
                const bool ok = mobj->CheckRelPosition(tmtrace, mobj->Origin, /*noPickups*/true, /*ignoreMonsters*/true, /*ignorePlayers*/true);
                #if 0
                GCon->Logf(NAME_Debug, "  ok=%d; maxmid=%g; dist=%g", (int)ok, maxmid, (origOrigin-mobj->Origin).length2D());
                #endif
                if (ok) {
                  // not blocked
                  maxhigh = maxmid;
                  goodPosFound = true;
                  goodPos = mobj->Origin;
                  //edata.aflags |= AFF_MOVE;
                  //wasMove = true;
                  //foundGoodUnstuck = true;
                  const float sqdist = (origOrigin-mobj->Origin).length2DSquared();
                  // restore
                  mobj->Origin = origOrigin;
                  if (sqdist <= 0.1f*0.1f) break;
                } else {
                  // blocked
                  maxlow = maxmid;
                }
              }
              if (goodPosFound) {
                #if 0
                GCon->Logf(NAME_Debug, "%s: found by normal (%g,%g,%g); dist=%g", mobj->GetClass()->GetName(), nsum.x, nsum.y, nsum.z, (origOrigin-goodPos).length2D());
                #endif
                mobj->Origin = goodPos;
                edata.aflags |= AFF_MOVE;
                wasMove = true;
                foundGoodUnstuck = true;
                tryNonAverage = false;
              }
            }
          }
        }

        if (tryNonAverage) {
          // sort unstuck vectors by distance
          timsort_r(uvlist.ptr(), uvlist.length(), sizeof(UnstuckInfo), &unstuckVectorCompare, nullptr);

          // try each unstuck vector
          //bool wasAtLeastOneGood = false;
          for (auto &&nfo : uvlist) {
            if (nfo.blockedByOther) continue; // bad line
            const TVec uv = nfo.uv;
            vassert(uv.isValid());
            if (uv.isZero2D()) continue; // just in case
            //wasAtLeastOneGood = true;
            if (dbg_pobj_unstuck_verbose.asBool()) {
              GCon->Logf(NAME_Debug, "mobj '%s' unstuck move: (%g,%g,%g)", edata.mobj->GetClass()->GetName(), uv.x, uv.y, uv.z);
            }
            // need to move
            //TODO: move any touching objects too
            mobj->Origin = origOrigin+uv;
            const bool ok = mobj->CheckRelPosition(tmtrace, mobj->Origin, /*noPickups*/true, /*ignoreMonsters*/true, /*ignorePlayers*/true);
            if (ok) {
              // not blocked, check other linked polyobjects
              edata.aflags |= AFF_MOVE;
              wasMove = true;
              foundGoodUnstuck = true;
              break;
            }
            // restore
            mobj->Origin = origOrigin;
          }
        } // tryNonAverage

        // if we can't find good unstucking move, crush
        if (!foundGoodUnstuck) {
          // totally stuck
          if (mobj->EntityFlags&VEntity::EF_Solid) {
            if (mobj->Level->eventPolyThrustMobj(mobj, TVec(0.0f, 0.0f), po, true/*vertical*/)) return false; // blocked
          } else {
            mobj->Level->eventPolyCrushMobj(mobj, po);
            if (NeedPositionCheck(mobj) && mobj->Health > 0) return false; // blocked if not crushed
          }
          wasMove = false; // get away, we're done with this mobj
          break;
        }
      } // polyobject link loop

      if (!wasMove) {
        // wasn't moved, so no need to check for "still stuck"
        doFinalCheck = false;
        break;
      }
    } // unstuck try loop

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
        const bool canUnstuck = CalcPolyUnstuckVector(uvlist, Level, po, mobj);
        // if we can't find unstuck direction, or found at least one, it means that we're stuck
        if (!canUnstuck || uvlist.length() > 0) {
          if (dbg_pobj_unstuck_verbose.asBool()) {
            GCon->Logf(NAME_Debug, "mobj '%s' STILL BLOCKED", edata.mobj->GetClass()->GetName());
          }
          // still blocked
          if (mobj->EntityFlags&VEntity::EF_Solid) {
            if (mobj->Level->eventPolyThrustMobj(mobj, TVec(0.0f, 0.0f), po, true/*vertical*/)) return false; // blocked
          } else {
            mobj->Level->eventPolyCrushMobj(mobj, po);
            if (NeedPositionCheck(mobj) && mobj->Health > 0) return false; // blocked if not crushed
          }
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
    if (!NeedPositionCheck(mobj)) continue;
    // temporarily disable collision with things
    //FIXME: reenable it when i'll write stacking code
    //!mobj->EntityFlags &= ~VEntity::EF_ColideWithThings;
    bool ok = mobj->CheckRelPosition(tmtrace, mobj->Origin, /*noPickups*/true, /*ignoreMonsters*/true, /*ignorePlayers*/true);
    //!mobj->EntityFlags = oldFlags; // restore flags
    if (!ok) {
      // abort horizontal movement
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
      // alas, blocked
      if (dbg_pobj_unstuck_verbose.asBool()) {
        GCon->Logf(NAME_Debug, "pobj #%d: blocked(0) by %s(%u)", pofirst->tag, mobj->GetClass()->GetName(), mobj->GetUniqueId());
      }
      return true;
    }
    // check ceiling
    if (mobj->Origin.z+max2(0.0f, mobj->Height) > tmtrace.CeilingZ) {
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
        if (!NeedPositionCheck(mobj)) continue;

        if (!checkPObjLineBlocked(po, mobj, ld)) continue;

        if (dbg_pobj_unstuck_verbose.asBool()) {
          GCon->Logf(NAME_Debug, "mobj '%s': blocked by pobj %d, line #%d", mobj->GetClass()->GetName(), po->tag, (int)(ptrdiff_t)(ld-&mobj->XLevel->Lines[0]));
        }

        //TODO: crush corpses!
        //TODO: crush objects with platforms

        if (mobj->EntityFlags&VEntity::EF_Solid) {
          //GCon->Logf(NAME_Debug, "pobj #%d hit %s(%u)", po->tag, mobj->GetClass()->GetName(), mobj->GetUniqueId());
          if (mobj->Level->eventPolyThrustMobj(mobj, ld->normal, po, false/*non-vertical*/)) blocked = true;
        } else {
          mobj->Level->eventPolyCrushMobj(mobj, po);
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
  poAffectedEnitities.resetNoDtor();
  if (!forcedMove && pofirst->posector && (docarry || verticalMove)) {
    flatsSaved = true;
    IncrementValidCount();
    const int visCount = validcount;
    //GCon->Logf(NAME_Debug, "=== pobj #%d ===", pofirst->tag);
    for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
      po->savedFloor = po->pofloor;
      po->savedCeiling = po->poceiling;
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
    const bool verticalMoveUp = (verticalMove && z > 0.0f);
    for (auto &&edata : poAffectedEnitities) {
      VEntity *mobj = edata.mobj;
      for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
        const float oldz = mobj->Origin.z;
        const float pofz = po->poceiling.maxz;
        // fix "affected" flag
        // vertical move goes up, and the platform goes through the object?
        if (verticalMove && oldz > pofz && oldz < pofz+z) {
          // yeah
          if (verticalMoveUp) {
            edata.aflags = AFF_VERT|(docarry ? AFF_MOVE : AFF_NOTHING_ZERO);
            mobj->Origin.z = pofz+z;
          }
        } else if (oldz == pofz) {
          // the object is standing on the pobj ceiling (i.e. floor ;-)
          // always carry, and always move with the pobj, even if it is flying
          edata.aflags = AFF_VERT|(docarry ? AFF_MOVE : AFF_NOTHING_ZERO);
          mobj->Origin.z = pofz+z;
        }
      }
    }

    // now unlink all affected objects, because we'll do "move and check" later
    // also, set move objects horizontally, because why not
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
    if (dovmove) OffsetPolyobjFlats(po, z);
  }

  bool blocked = false;

  // now check if movement is blocked by any affected object
  // we have to do it after we unlinked all pobjs
  if (!forcedMove && pofirst->posector) {
    blocked = CheckAffectedMObjPositions(pofirst);
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
    for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
      LinkPolyobj(po);
    }
    linked = true;
    // check height-blocking objects
    for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
      const float pz0 = po->pofloor.minz;
      const float pz1 = po->poceiling.maxz;
      for (msecnode_t *n = po->posector->TouchingThingList; n; n = n->SNext) {
        VEntity *mobj = n->Thing;
        if (mobj->IsGoingToDie()) continue;
        // check flags
        if (!NeedPositionCheck(mobj)) continue;
        //if (mobj->Height <= 0.0f || mobj->Radius <= 0.0f) continue; // done in `NeedPositionCheck()`
        const float mz0 = mobj->Origin.z;
        const float mz1 = mz0+mobj->Height;
        if (mz1 <= pz0) continue;
        if (mz0 >= pz1) continue;
        // vertical intersection, blocked movement
        if (mobj->EntityFlags&VEntity::EF_Solid) {
          //GCon->Logf(NAME_Debug, "pobj #%d hit %s(%u)", po->tag, mobj->GetClass()->GetName(), mobj->GetUniqueId());
          if (mobj->Level->eventPolyThrustMobj(mobj, TVec(0.0f, 0.0f), po, true/*vertical*/)) blocked = true;
        } else {
          mobj->Level->eventPolyCrushMobj(mobj, po);
        }
        if (blocked) break;
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
      LinkPolyobj(po);
    }
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
    if (!linked) LinkPolyobj(po);
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
      const float pz1 = po->poceiling.maxz;
      for (msecnode_t *n = po->posector->TouchingThingList; n; n = n->SNext) {
        VEntity *mobj = n->Thing;
        if (mobj->ValidCount == visCount) continue;
        mobj->ValidCount = visCount;
        if (mobj->IsGoingToDie()) continue;
        if (dbg_pobj_unstuck_verbose.asBool()) {
          GCon->Logf(NAME_Debug, "ROTATION: pobj #%d: checking mobj '%s' (z=%g; pz1=%g)", po->tag, mobj->GetClass()->GetName(), mobj->Origin.z, pz1);
        }
        // check flags
        if ((mobj->EntityFlags&VEntity::EF_ColideWithWorld) == 0) continue;
        if ((mobj->FlagsEx&(VEntity::EFEX_NoInteraction|VEntity::EFEX_NoTickGrav)) == VEntity::EFEX_NoInteraction) continue;
        if (mobj->Origin.z != pz1) {
          mobj->ValidCount = 0; // it may be affected by another pobj
          continue; // just in case
        }
        SavedEntityData &edata = poAffectedEnitities.alloc();
        edata.mobj = mobj;
        edata.Origin = mobj->Origin;
        edata.LastMoveOrigin = mobj->LastMoveOrigin;
        // we need to remember rotation point
        //FIXME: this will glitch with objects standing on some multipart platforms
        edata.spot = (indRot ? po->startSpot : pofirst->startSpot); // mobj->Sector->ownpobj
        edata.aflags = 0;
        if (dbg_pobj_unstuck_verbose.asBool()) {
          GCon->Logf(NAME_Debug, "ROTATION: pobj #%d: mobj '%s' is affected", po->tag, mobj->GetClass()->GetName());
        }
        // check if it is initially stuck
        #if 1
        if (NeedPositionCheck(mobj)) {
          for (polyobj_t *xpp = pofirst; xpp; xpp = (skipLink ? nullptr : xpp->polink)) {
            if (mobj->Origin.z >= po->poceiling.maxz || mobj->Origin.z+max2(0.0f, mobj->Height) <= po->pofloor.minz) {
              // do nothing
            } else {
              for (auto &&lxx : xpp->LineFirst()) {
                const line_t *ld = lxx.line();
                if (!mobj->IsBlockingLine(ld)) continue;
                if (!mobj->LineIntersects(ld)) continue;
                edata.aflags |= AFF_STUCK;
                break;
              }
            }
            if (edata.aflags&AFF_STUCK) break;
          }
        }
        #endif
      }
    }

    // now unlink all affected objects, because we'll do "move and check" later
    // also, rotate the objects
    msincos(AngleMod(angle), &s, &c);
    for (auto &&edata : poAffectedEnitities) {
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
          edata.aflags |= AFF_MOVE;
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
      blocked = CheckAffectedMObjPositions(pofirst);
      if (dbg_pobj_unstuck_verbose.asBool()) {
        GCon->Logf(NAME_Debug, "pobj #%d: ROTATION: blocked=%d", pofirst->tag, (int)blocked);
      }
      if (!blocked) blocked = !UnstuckFromRotatedPObj(this, pofirst, skipLink);
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
      LinkPolyobj(po);
    }
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
    /*
    // fix start spots
    if (!indRot && po != pofirst) {
      // rotate this object's starting spot around the main object starting spot
      msincos(angle, &s, &c);
      const float tr_x = po->startSpot.x-pofirst->startSpot.x;
      const float tr_y = po->startSpot.y-pofirst->startSpot.y;
      // calculate the new X and Y values
      const float nx = (tr_x*c-tr_y*s)+pofirst->startSpot.x;
      const float ny = (tr_y*c+tr_x*s)+pofirst->startSpot.y;
      po->startSpot.x = nx;
      po->startSpot.y = ny;
    }
    */
    po->angle = AngleMod(po->angle+angle);
    if (flatsSaved) {
      po->pofloor.BaseAngle = AngleMod(po->pofloor.BaseAngle+angle);
      po->poceiling.BaseAngle = AngleMod(po->poceiling.BaseAngle+angle);
      OffsetPolyobjFlats(po, 0.0f, true);
    }
    LinkPolyobj(po);
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

  // relink things
  if (!forcedMove && poAffectedEnitities.length()) {
    for (auto &&edata : poAffectedEnitities) {
      VEntity *mobj = edata.mobj;
      if (mobj->IsGoingToDie()) continue;
      if (doangle) mobj->Level->eventPolyRotateMobj(mobj, angle);
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
