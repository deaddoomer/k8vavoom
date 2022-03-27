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
//**  Copyright (C) 2018-2022 Ketmar Dark
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
//**  VEntity collision, physics and related methods.
//**
//**************************************************************************
#include "../gamedefs.h"
#include "p_entity.h"
#include "p_levelinfo.h"
#include "p_world.h"

//#define VV_DBG_VERBOSE_REL_LINE_FC

static VCvarB mv_new_slope_code("mv_new_slope_code", true, "Use experimental slope walking code?", CVAR_Archive|CVAR_NoShadow);


//==========================================================================
//
//  VEntity::CheckRelPosition
//
// This is purely informative, nothing is modified
// (except things picked up).
//
// in:
//  a mobj_t (can be valid or invalid)
//  a position to be checked
//   (doesn't need to be related to the mobj_t->x,y)
//
// during:
//  special things are touched if MF_PICKUP
//  early out on solid lines?
//
// out:
//  newsubsec
//  floorz
//  ceilingz
//  tmdropoffz
//   the lowest point contacted
//   (monsters won't move to a dropoff)
//  speciallines[]
//  numspeciallines
//  VEntity *BlockingMobj = pointer to thing that blocked position (nullptr if not
//   blocked, or blocked by a line).
//
//==========================================================================
bool VEntity::CheckRelPosition (tmtrace_t &tmtrace, TVec Pos, bool noPickups, bool ignoreMonsters, bool ignorePlayers) {
  //if (IsPlayer()) GCon->Logf(NAME_Debug, "*** CheckRelPosition: pos=(%g,%g,%g)", Pos.x, Pos.y, Pos.z);
  memset((void *)&tmtrace, 0, sizeof(tmtrace));

  tmtrace.End = Pos;

  const float rad = GetMoveRadius();

  tmtrace.BBox[BOX2D_TOP] = Pos.y+rad;
  tmtrace.BBox[BOX2D_BOTTOM] = Pos.y-rad;
  tmtrace.BBox[BOX2D_RIGHT] = Pos.x+rad;
  tmtrace.BBox[BOX2D_LEFT] = Pos.x-rad;

  subsector_t *newsubsec = XLevel->PointInSubsector_Buggy(Pos);
  //tmtrace.CeilingLine = nullptr;

  tmtrace.setupGap(XLevel, newsubsec->sector, Height);

  XLevel->IncrementValidCount();
  tmtrace.SpecHit.resetNoDtor(); // was `Clear()`

  //GCon->Logf(NAME_Debug, "xxx: %s(%u): CheckRelPosition (vc=%d)", GetClass()->GetName(), GetUniqueId(), validcount);

  tmtrace.BlockingMobj = nullptr;
  tmtrace.StepThing = nullptr;
  VEntity *thingblocker = nullptr;

  // check things first, possibly picking things up.
  if (EntityFlags&EF_ColideWithThings) {
    // the bounding box is extended by MAXRADIUS
    // because mobj_ts are grouped into mapblocks
    // based on their origin point, and can overlap
    // into adjacent blocks by up to MAXRADIUS units.
    DeclareMakeBlockMapCoordsBBox2DMaxRadius(tmtrace.BBox, xl, yl, xh, yh);
    for (int bx = xl; bx <= xh; ++bx) {
      for (int by = yl; by <= yh; ++by) {
        for (auto &&it : XLevel->allBlockThings(bx, by)) {
          VEntity *ent = it.entity();
          if (ignoreMonsters || ignorePlayers) {
            if (ignorePlayers && ent->IsPlayer()) continue;
            if (ignoreMonsters && (ent->IsAnyMissile() || ent->IsMonster())) continue;
          }
          if (!CheckRelThing(tmtrace, ent, noPickups)) {
            // continue checking for other things in to see if we hit something
            if (!tmtrace.BlockingMobj || Level->GetNoPassOver()) {
              // slammed into something
              return false;
            } else if (!tmtrace.BlockingMobj->Player &&
                       !(EntityFlags&(VEntity::EF_Float|VEntity::EF_Missile)) &&
                       tmtrace.BlockingMobj->Origin.z+tmtrace.BlockingMobj->Height-tmtrace.End.z <= MaxStepHeight)
            {
              if (!thingblocker || tmtrace.BlockingMobj->Origin.z > thingblocker->Origin.z) thingblocker = tmtrace.BlockingMobj;
              tmtrace.BlockingMobj = nullptr;
            } else if (Player && tmtrace.End.z+Height-tmtrace.BlockingMobj->Origin.z <= MaxStepHeight) {
              if (thingblocker) {
                // something to step up on, set it as the blocker so that we don't step up
                return false;
              }
              // nothing is blocking, but this object potentially could
              // if there is something else to step on
              tmtrace.BlockingMobj = nullptr;
            } else {
              // blocking
              return false;
            }
          }
        }
      }
    }
  }

  float thingdropoffz = tmtrace.FloorZ;
  tmtrace.FloorZ = tmtrace.DropOffZ;
  tmtrace.BlockingMobj = nullptr;

  //bool gotNewValid = false;

  // check lines
  if (EntityFlags&EF_ColideWithWorld) {
    #ifdef VV_DBG_VERBOSE_REL_LINE_FC
    if (IsPlayer()) GCon->Logf(NAME_Debug, "xxx: %s(%u): checking lines...; FloorZ=%g", GetClass()->GetName(), GetUniqueId(), tmtrace.FloorZ);
    #endif
    //XLevel->IncrementValidCount();
    //gotNewValid = true;

    DeclareMakeBlockMapCoordsBBox2D(tmtrace.BBox, xl, yl, xh, yh);
    bool good = true;
    line_t *fuckhit = nullptr;
    float lastFrac = 1e7f;
    for (int bx = xl; bx <= xh; ++bx) {
      for (int by = yl; by <= yh; ++by) {
        for (auto &&it : XLevel->allBlockLines(bx, by)) {
          line_t *ld = it.line();
          //good &= CheckRelLine(tmtrace, ld);
          // if we don't want pickups, don't activate specials
          if (!CheckRelLine(tmtrace, ld, noPickups)) {
            #ifdef VV_DBG_VERBOSE_REL_LINE_FC
            if (IsPlayer()) GCon->Logf(NAME_Debug, "%s:  BLOCKED by line #%d (FloorZ=%g)", GetClass()->GetName(), (int)(ptrdiff_t)(ld-&XLevel->Lines[0]), tmtrace.FloorZ);
            #endif
            good = false;
            // find the fractional intercept point along the trace line
            const float den = ld->normal.dot(tmtrace.End-Pos);
            if (den == 0.0f) {
              fuckhit = ld;
              lastFrac = 0;
            } else {
              const float num = ld->dist-Pos.dot(ld->normal);
              const float frac = fabsf(num/den);
              if (frac < lastFrac) {
                fuckhit = ld;
                lastFrac = frac;
              }
            }
          }
          #ifdef VV_DBG_VERBOSE_REL_LINE_FC
          else {
            if (IsPlayer()) GCon->Logf(NAME_Debug, "%s:  OK line #%d (FloorZ=%g)", GetClass()->GetName(), (int)(ptrdiff_t)(ld-&XLevel->Lines[0]), tmtrace.FloorZ);
          }
          #endif
        }
      }
    }

    polyobj_t *inpobj = nullptr;
    // check if we can stand inside some polyobject
    if (XLevel->Has3DPolyObjects()) {
      //if (!gotNewValid) XLevel->IncrementValidCount();
      //GCon->Logf(NAME_Debug, "xxx: %s(%u): checking pobjs (%d)... (vc=%d)", GetClass()->GetName(), GetUniqueId(), XLevel->NumPolyObjs, validcount);
      const float z1 = tmtrace.End.z+max2(0.0f, Height);
      for (int bx = xl; bx <= xh; ++bx) {
        for (int by = yl; by <= yh; ++by) {
          polyobj_t *po;
          for (VBlockPObjIterator It(XLevel, bx, by, &po); It.GetNext(); ) {
            //GCon->Logf(NAME_Debug, "000: %s(%u): checking pobj #%d... (%g:%g) (%g:%g)", GetClass()->GetName(), GetUniqueId(), po->tag, po->pofloor.minz, po->poceiling.maxz, tmtrace.End.z, z1);
            if (po->pofloor.minz >= po->poceiling.maxz) continue;
            if (!Are2DBBoxesOverlap(po->bbox2d, tmtrace.BBox)) continue;
            //GCon->Logf(NAME_Debug, "001: %s(%u):   checking pobj #%d...", GetClass()->GetName(), GetUniqueId(), po->tag);
            if (!XLevel->IsBBox2DTouchingSector(po->GetSector(), tmtrace.BBox)) continue;
            //GCon->Logf(NAME_Debug, "002: %s(%u):   HIT pobj #%d (fz=%g; cz=%g); z0=%g; z1=%g", GetClass()->GetName(), GetUniqueId(), po->tag, tmtrace.FloorZ, tmtrace.CeilingZ, tmtrace.End.z, z1);
            const float oldFZ = tmtrace.FloorZ;
            if (Copy3DPObjFloorCeiling(tmtrace, po, tmtrace.End.z, z1)) {
              if (oldFZ != tmtrace.FloorZ) tmtrace.DropOffZ = tmtrace.FloorZ;
            }
            //GCon->Logf(NAME_Debug, "003: %s(%u):       pobj #%d (fz=%g; cz=%g); z0=%g; z1=%g; pz=(%g : %g)", GetClass()->GetName(), GetUniqueId(), po->tag, tmtrace.FloorZ, tmtrace.CeilingZ, tmtrace.End.z, z1, po->pofloor.minz, po->poceiling.maxz);
          }
        }
      }
    }

    if (!good) {
      if (fuckhit) {
        if (!tmtrace.BlockingLine) tmtrace.BlockingLine = fuckhit;
        if (!tmtrace.AnyBlockingLine) tmtrace.AnyBlockingLine = fuckhit;
      }
      return false;
    }

    if (inpobj) {
      tmtrace.BlockingMobj = nullptr;
      tmtrace.BlockingLine = nullptr;
      tmtrace.AnyBlockingLine = nullptr;
      tmtrace.PolyObj = inpobj;
      return false;
    }

    if (tmtrace.CeilingZ-tmtrace.FloorZ < Height) {
      if (fuckhit) {
        if (!tmtrace.CeilingLine && !tmtrace.FloorLine && !tmtrace.BlockingLine) {
          // this can happen when you're crouching, for example
          // `fuckhit` is not set in that case too
          GCon->Logf(NAME_Warning, "CheckRelPosition for `%s` is height-blocked, but no block line determined!", GetClass()->GetName());
          tmtrace.BlockingLine = fuckhit;
        }
        if (!tmtrace.AnyBlockingLine) tmtrace.AnyBlockingLine = fuckhit;
      }
      return false;
    }
  }

  if (tmtrace.StepThing) tmtrace.DropOffZ = thingdropoffz;

  tmtrace.BlockingMobj = thingblocker;
  if (tmtrace.BlockingMobj) return false;

  #ifdef VV_DBG_VERBOSE_REL_LINE_FC
  if (IsPlayer()) GCon->Logf(NAME_Debug, "xxx: %s(%u): VALID; FloorZ=%g", GetClass()->GetName(), GetUniqueId(), tmtrace.FloorZ);
  #endif

  return true;
}


//==========================================================================
//
//  VEntity::CheckRelThing
//
//  returns `false` when blocked
//
//==========================================================================
bool VEntity::CheckRelThing (tmtrace_t &tmtrace, VEntity *Other, bool noPickups) {
  // don't clip against self
  if (Other == this) return true;
  //if (OwnerSUId && Other && Other->ServerUId == OwnerSUId) return true;

  // can't hit thing
  if (!(Other->EntityFlags&EF_ColideWithThings)) return true;

  const float rad = GetMoveRadius();
  const float otherrad = Other->GetMoveRadius();

  const float blockdist = otherrad+rad;

  if (fabsf(Other->Origin.x-tmtrace.End.x) >= blockdist ||
      fabsf(Other->Origin.y-tmtrace.End.y) >= blockdist)
  {
    // didn't hit it
    return true;
  }

  // can't walk on corpses (but can touch them)
  const bool isOtherCorpse = !!(Other->EntityFlags&EF_Corpse);

  // /*!(Level->LevelInfoFlags2&VLevelInfo::LIF2_CompatNoPassOver) && !compat_nopassover*/!Level->GetNoPassOver()

  if (!isOtherCorpse) {
    //tmtrace.BlockingMobj = Other;

    // check bridges
    if (!Level->GetNoPassOver() &&
        !(EntityFlags&(EF_Float|EF_Missile|EF_NoGravity)) &&
        (Other->EntityFlags&(EF_Solid|EF_ActLikeBridge)) == (EF_Solid|EF_ActLikeBridge))
    {
      // allow actors to walk on other actors as well as floors
      if (fabsf(Other->Origin.x-tmtrace.End.x) < otherrad ||
          fabsf(Other->Origin.y-tmtrace.End.y) < otherrad)
      {
        const float ohgt = GetBlockingHeightFor(Other);
        if (Other->Origin.z+ohgt >= tmtrace.FloorZ &&
            Other->Origin.z+ohgt <= tmtrace.End.z+MaxStepHeight)
        {
          //tmtrace.BlockingMobj = Other;
          tmtrace.StepThing = Other;
          tmtrace.FloorZ = Other->Origin.z+ohgt;
        }
      }
    }
  }

  //if (!(tmtrace.Thing->EntityFlags & VEntity::EF_NoPassMobj) || Actor(Other).bSpecial)
  if ((EntityFlags&EF_Missile) ||
      (((EntityFlags&EF_PassMobj)|(Other->EntityFlags&EF_ActLikeBridge)) && !Level->GetNoPassOver()))
  {
    // prevent some objects from overlapping
    if (!isOtherCorpse && ((EntityFlags&Other->EntityFlags)&EF_DontOverlap)) {
      tmtrace.BlockingMobj = Other;
      return false;
    }
    // check if a mobj passed over/under another object
    if (tmtrace.End.z >= Other->Origin.z+GetBlockingHeightFor(Other)) return true; // overhead
    if (tmtrace.End.z+Height <= Other->Origin.z) return true; // underneath
  }

  if (!eventTouch(Other, noPickups)) {
    // just in case
    tmtrace.BlockingMobj = Other;
    return false;
  }

  return true;
}


//==========================================================================
//
//  VEntity::CheckRelLine
//
//  Adjusts tmtrace.FloorZ and tmtrace.CeilingZ as lines are contacted
//
//  returns `false` if blocked
//
//==========================================================================
bool VEntity::CheckRelLine (tmtrace_t &tmtrace, line_t *ld, bool skipSpecials) {
  if (GGameInfo->NetMode == NM_Client) skipSpecials = true;

  //if (IsPlayer()) GCon->Logf(NAME_Debug, "  trying line: %d", (int)(ptrdiff_t)(ld-&XLevel->Lines[0]));
  if (!ld->Box2DHit(tmtrace.BBox)) return true; // no intersection

  // a line has been hit

  // The moving thing's destination position will cross the given line.
  // If this should not be allowed, return false.
  // If the line is special, keep track of it to process later if the move is proven ok.
  // NOTE: specials are NOT sorted by order, so two special lines that are only 8 pixels apart
  //       could be crossed in either order.

  // k8: this code is fuckin' mess. why some lines are processed immediately, and
  //     other lines are pushed to be processed later? what the fuck is going on here?!
  //     it seems that the original intent was to immediately process blocking lines,
  //     but push non-blocking lines. wtf?!

  if (!ld->backsector || !(ld->flags&ML_TWOSIDED)) {
    // one sided line
    if (!skipSpecials) BlockedByLine(ld);
    // mark the line as blocking line
    tmtrace.BlockingLine = ld;
    return false;
  }

  const float hgt = max2(0.0f, Height);
  //TVec hitPoint = tmtrace.End-(tmtrace.End.dot(ld->normal)-ld->dist)*ld->normal;
  const TVec hitPoint = tmtrace.End-(ld->PointDistance(tmtrace.End)*ld->normal);

  // check polyobject
  polyobj_t *po = ld->pobj();
  if (po) {
    //if (IsPlayer()) GCon->Logf(NAME_Debug, "pobj #%d line #%d, blocking=%d", po->tag, (int)(ptrdiff_t)(ld-&XLevel->Lines[0]), (int)IsBlockingLine(ld));
    if (po->validcount == validcount) return true; // already checked and stuck, ignore it
    // non-3d polyobjects
    if (!po->Is3D()) {
      po->validcount = validcount; // do not check if we are inside of it, we definitely are
      // check for non-3d pobj with midtex
      if (ld->flags&ML_3DMIDTEX) {
        // use front side
        const int side = 0; //ld->PointOnSide(tmtrace.End);
        float pz0 = 0.0f, pz1 = 0.0f;
        if (!XLevel->GetMidTexturePosition(ld, side, &pz0, &pz1)) return true; // no middle texture, so no collision
        if (hitPoint.z >= pz1 || hitPoint.z+hgt <= pz0) return true; // no collision
      } else {
        if (!IsBlockingLine(ld)) return true;
      }
      // blocking non-3d polyobject line
      if (!skipSpecials) BlockedByLine(ld);
      tmtrace.BlockingLine = ld;
      return false;
    }
    vassert(po->Is3D()); // invariant
    // check top texture, if it blocking
    if (ld->flags&ML_CLIP_MIDTEX) {
      //bool doBlock = (IsPlayer() || IsMonster());
      //if (!doBlock && IsMissile() && (ld->flags&ML_BLOCKPROJECTILE)) doBlock = true;
      const bool doBlock = IsBlocking3DPobjLineTop(ld);
      if (doBlock) {
        // check if we are above the pobj (at least partially)
        const float ptopz = po->poceiling.minz;
        if (/*hitPoint.z >= ptopz ||*/ hitPoint.z+hgt > ptopz) {
          // side doesn't matter, as it is guaranteed that both sides have the texture with the same height
          const side_t *tside = &XLevel->Sides[ld->sidenum[0]];
          if (tside->TopTexture > 0) {
            VTexture *ttex = GTextureManager(tside->TopTexture);
            if (ttex && ttex->Type != TEXTYPE_Null) {
              const float texh = ttex->GetScaledHeightF()/tside->Top.ScaleY;
              if (hitPoint.z < ptopz+texh && hitPoint.z+hgt > ptopz) {
                // blocked by top texture
                po->validcount = validcount; // do not check if we are inside of it, we definitely are
                if (!skipSpecials) BlockedByLine(ld);
                tmtrace.BlockingLine = ld;
                //TODO: set proper FloorZ, CeilingZ, and DropOffZ!
                if (FloorZ < ptopz) DropOffZ = FloorZ; // fix dropoff
                FloorZ = ptopz;
                EFloor.set(&po->poceiling, false);
                return false;
              }
            }
          }
        }
      }
    }
    //if (!IsBlockingLine(ld)) return true; // always blocking
    if (!Copy3DPObjFloorCeiling(tmtrace, po, hitPoint.z, hitPoint.z+max2(0.0f, Height))) {
      // stuck inside, blocked
      po->validcount = validcount; // do not check if we are inside of it, we definitely are
      if (!skipSpecials) BlockedByLine(ld);
      tmtrace.BlockingLine = ld;
      return false;
    }
    return true;
  }

  if (IsBlockingLine(ld)) {
    if (!skipSpecials) BlockedByLine(ld);
    tmtrace.BlockingLine = ld;
    return false;
  }

  // set openrange, opentop, openbottom
  opening_t *open = XLevel->LineOpenings(ld, hitPoint, SPF_NOBLOCKING, !(EntityFlags&EF_Missile)/*do3dmidtex*/); // missiles ignores 3dmidtex

  /*
  if (IsPlayer()) {
    GCon->Logf(NAME_Debug, "line #%d: end=(%g,%g,%g); hp=(%g,%g,%g)",
               (int)(ptrdiff_t)(ld-&XLevel->Lines[0]),
               tmtrace.End.x, tmtrace.End.y, tmtrace.End.z,
               hitPoint.x, hitPoint.y, hitPoint.z);
  }
  */

  #ifdef VV_DBG_VERBOSE_REL_LINE_FC
  if (IsPlayer()) {
    GCon->Logf(NAME_Debug, "  checking line: %d; sz=%g; ez=%g; hgt=%g; traceFZ=%g; traceCZ=%g", (int)(ptrdiff_t)(ld-&XLevel->Lines[0]), tmtrace.End.z, tmtrace.End.z+hgt, hgt, tmtrace.FloorZ, tmtrace.CeilingZ);
    for (opening_t *op = open; op; op = op->next) {
      GCon->Logf(NAME_Debug, "   %p: bot=%g; top=%g; range=%g; lowfloor=%g; fnormz=%g", op, op->bottom, op->top, op->range, op->lowfloor, op->efloor.GetNormalZSafe());
    }
  }
  #endif

  open = VLevel::FindRelOpening(open, tmtrace.End.z, tmtrace.End.z+hgt);
  #ifdef VV_DBG_VERBOSE_REL_LINE_FC
  if (IsPlayer()) GCon->Logf(NAME_Debug, "  open=%p; railing=%d", open, (int)!!(ld->flags&ML_RAILING));
  #endif
  // process railings
  if (open && (ld->flags&ML_RAILING)) {
    open->bottom += 32.0f;
    open->range -= 32.0f;
    if (open->range <= 0.0f) {
      open = nullptr;
    } else {
      const float z0 = tmtrace.End.z;
      const float z1 = z0+hgt;
      if (z1 < open->bottom || z0 > open->top) open = nullptr;
    }
  }
  //if (ld->flags&ML_RAILING) tmtrace.FloorZ += 32.0f;

  if (open) {
    // adjust floor / ceiling heights
    if ((open->eceiling.splane->flags&SPF_NOBLOCKING) == 0) {
      bool replaceIt;
      if (open->eceiling.GetNormalZ() != -1.0f) {
        // slope; use epsilon
        replaceIt = (tmtrace.CeilingZ-open->top > 0.1f);
      } else {
        replaceIt = (tmtrace.CeilingZ > open->top || (open->top == tmtrace.CeilingZ && tmtrace.ECeiling.isSlope()));
      }
      if (replaceIt) {
        /*if (!skipSpecials || open->top >= Origin.z+hgt)*/ {
          #ifdef VV_DBG_VERBOSE_REL_LINE_FC
          if (IsPlayer()) GCon->Logf(NAME_Debug, "    copy ceiling; hgt=%g; z+hgt=%g; top=%g; curcz-top=%g", hgt, Origin.z+hgt, open->top, tmtrace.CeilingZ-open->top);
          #endif
          tmtrace.CopyOpenCeiling(open);
          tmtrace.CeilingLine = ld;
        }
      }
    }

    if ((open->efloor.splane->flags&SPF_NOBLOCKING) == 0) {
      bool replaceIt;
      if (open->efloor.GetNormalZ() != 1.0f) {
        // slope; use epsilon
        replaceIt = (open->bottom-tmtrace.FloorZ > 0.1f);
        #ifdef VV_DBG_VERBOSE_REL_LINE_FC
        if (IsPlayer()) GCon->Logf(NAME_Debug, "    !floorcheck; slopez=%g; open->bottom=%g; tmtrace.FloorZ=%g; >=%d (%d)", open->efloor.GetNormalZ(), open->bottom, tmtrace.FloorZ, (int)(open->bottom > tmtrace.FloorZ), (int)replaceIt);
        #endif
        // this is required for proper slope processing
        if (replaceIt && mv_new_slope_code.asBool()) {
          if (open->bottom-tmtrace.FloorZ <= MaxStepHeight) replaceIt = false;
        }
      } else {
        replaceIt = (open->bottom > tmtrace.FloorZ || (open->bottom == tmtrace.FloorZ && tmtrace.EFloor.isSlope()));
        #ifdef VV_DBG_VERBOSE_REL_LINE_FC
        if (IsPlayer()) GCon->Logf(NAME_Debug, "    !floorcheck; open->bottom=%g; tmtrace.FloorZ=%g; >=%d (%d)", open->bottom, tmtrace.FloorZ, (int)(open->bottom > tmtrace.FloorZ), (int)replaceIt);
        #endif
      }
      if (replaceIt) {
        /*if (!skipSpecials || open->bottom <= Origin.z)*/ {
          #ifdef VV_DBG_VERBOSE_REL_LINE_FC
          if (IsPlayer()) GCon->Logf(NAME_Debug, "    copy floor; z=%g; bot=%g; curfz-bot=%g", Origin.z, open->bottom, tmtrace.FloorZ-open->bottom);
          #endif
          tmtrace.CopyOpenFloor(open);
          tmtrace.FloorLine = ld;
        }
      }
    } else {
      #ifdef VV_DBG_VERBOSE_REL_LINE_FC
      if (IsPlayer()) GCon->Logf(NAME_Debug, "...skip floor");
      #endif
    }

    if (open->lowfloor < tmtrace.DropOffZ) tmtrace.DropOffZ = open->lowfloor;

    //if (ld->flags&ML_RAILING) tmtrace.FloorZ += 32.0f;
  } else {
    // no opening
    tmtrace.CeilingZ = tmtrace.FloorZ;
    //k8: oops; it seems that we have to return `false` here
    //    but only if this is not a special line, otherwise monsters cannot open doors
    if (!ld->special) {
      //GCon->Logf(NAME_Debug, "BLK: %s (hgt=%g); line=%d", GetClass()->GetName(), hgt, (int)(ptrdiff_t)(ld-&XLevel->Lines[0]));
      if (!skipSpecials) BlockedByLine(ld);
      tmtrace.BlockingLine = ld;
      return false;
    } else {
      // this is special line, don't block movement (but remember this line as blocking!)
      tmtrace.BlockingLine = ld;
    }
  }

  // if contacted a special line, add it to the list
  if (!skipSpecials && ld->special) tmtrace.SpecHit.Append(ld);

  return true;
}


//==========================================================================
//
//  VEntity::CheckRelPositionPoint
//
//==========================================================================
bool VEntity::CheckRelPositionPoint (tmtrace_t &tmtrace, TVec Pos) {
  memset((void *)&tmtrace, 0, sizeof(tmtrace));

  tmtrace.End = Pos;

  const subsector_t *sub = XLevel->PointInSubsector_Buggy(Pos);
  sector_t *sec = sub->sector;

  const float ffz = sec->floor.GetPointZClamped(Pos);
  const float fcz = sec->ceiling.GetPointZClamped(Pos);

  tmtrace.EFloor.set(&sec->floor, false);
  tmtrace.FloorZ = tmtrace.DropOffZ = ffz;
  tmtrace.ECeiling.set(&sec->ceiling, false);
  tmtrace.CeilingZ = fcz;

  // below or above?
  if (Pos.z < ffz || Pos.z > fcz) return false;

  // closed sector?
  if (ffz >= fcz) return false;

  // on the floor, or on the ceiling?
  if (Pos.z == ffz || Pos.z == fcz) return true;

  bool res = true;

  // check regions
  if (sec->Has3DFloors()) {
    for (sec_region_t *reg = sec->eregions->next; reg; reg = reg->next) {
      if (reg->regflags&(sec_region_t::RF_BaseRegion|sec_region_t::RF_OnlyVisual|sec_region_t::RF_NonSolid)) continue;
      //if (((reg->efloor.splane->flags|reg->eceiling.splane->flags)&flagmask) != 0) continue; // bad flags
      // get opening points
      const float fz = reg->efloor.GetPointZClamped(Pos);
      const float cz = reg->eceiling.GetPointZClamped(Pos);
      if (fz >= cz) continue; // ignore paper-thin regions
      if (Pos.z <= cz) {
        // below, fix ceiling
        if (cz < tmtrace.CeilingZ) {
          tmtrace.ECeiling = reg->eceiling;
          tmtrace.CeilingZ = cz;
        }
        continue;
      }
      if (Pos.z >= fz) {
        // above, fix floor
        if (fz > tmtrace.FloorZ) {
          tmtrace.EFloor = reg->efloor;
          tmtrace.FloorZ = fz;
        }
        continue;
      }
      // inside, ignore it
      res = false; // blocked
    }
  }

  // check polyobjects
  if (XLevel->CanHave3DPolyObjAt2DPoint(Pos.x, Pos.y)) {
    for (auto &&it : sub->PObjFirst()) {
      polyobj_t *po = it.pobj();
      if (!po->Is3D()) continue;
      if (!IsPointInside2DBBox(Pos.x, Pos.y, po->bbox2d)) continue;
      const float pz0 = po->pofloor.minz;
      const float pz1 = po->poceiling.maxz;
      if (pz0 >= pz1) continue; // paper-thin
      if (Pos.z > pz0 && Pos.z < pz1) { res = false; continue; } // inside, nothing to do
      // fix floor and ceiling
      if (Pos.z <= pz0) {
        // below, fix ceiling
        if (pz0 < tmtrace.CeilingZ) {
          if (XLevel->IsPointInsideSector2D(po->GetSector(), Pos.x, Pos.y)) {
            tmtrace.ECeiling.set(&po->pofloor, false);
            tmtrace.CeilingZ = pz0;
          }
        }
        continue;
      }
      if (Pos.z >= pz1) {
        // above, fix floor
        if (pz1 > tmtrace.FloorZ) {
          if (XLevel->IsPointInsideSector2D(po->GetSector(), Pos.x, Pos.y)) {
            tmtrace.EFloor.set(&po->poceiling, false);
            tmtrace.FloorZ = pz1;
          }
        }
        continue;
      }
    }
  }

  return res;
}



//==========================================================================
//
//  Script natives
//
//==========================================================================

// native final bool CheckRelPosition (out tmtrace_t tmtrace, TVec Pos, optional bool noPickups/*=false*/, optional bool ignoreMonsters, optional bool ignorePlayers);
IMPLEMENT_FUNCTION(VEntity, CheckRelPosition) {
  tmtrace_t tmp;
  tmtrace_t *tmtrace = nullptr;
  TVec Pos;
  VOptParamBool noPickups(false);
  VOptParamBool ignoreMonsters(false);
  VOptParamBool ignorePlayers(false);
  vobjGetParamSelf(tmtrace, Pos, noPickups, ignoreMonsters, ignorePlayers);
  if (!tmtrace) tmtrace = &tmp;
  RET_BOOL(Self->CheckRelPosition(*tmtrace, Pos, noPickups, ignoreMonsters, ignorePlayers));
}

// native final bool CheckRelPositionPoint (out tmtrace_t tmtrace, TVec Pos);
IMPLEMENT_FUNCTION(VEntity, CheckRelPositionPoint) {
  tmtrace_t tmp;
  tmtrace_t *tmtrace = nullptr;
  TVec Pos;
  vobjGetParamSelf(tmtrace, Pos);
  if (!tmtrace) tmtrace = &tmp;
  RET_BOOL(Self->CheckRelPositionPoint(*tmtrace, Pos));
}
