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
//**  VEntity collision, physics and related methods.
//**
//**************************************************************************
#include "../gamedefs.h"
#include "p_entity.h"
#include "p_world.h"


// ////////////////////////////////////////////////////////////////////////// //
static VCvarB dbg_slide_code("dbg_slide_code", false, "Debug slide code?", CVAR_PreInit|CVAR_Hidden);
static VCvarI gm_slide_code("gm_slide_code", "2", "Which slide code to use (0:vanilla; 1:new; 2:q3-like)?", CVAR_Archive);
static VCvarB gm_q3slide_use_timeleft("gm_q3slide_use_timeleft", false, "Use 'timeleft' method for Q3 sliding code (gives wrong velocities and bumpiness)?", CVAR_Archive);



//**************************************************************************
//
//  SLIDE MOVE
//
//  Allows the player to slide along any angled walls.
//
//**************************************************************************

//==========================================================================
//
//  VEntity::TraceToWallSmall2D
//
//  this should be called in physics, when `TryMove()` failed
//  returns new origin near the wall
//  do not use big `vdelta` here, the tracer is VERY ineffective!
//
//==========================================================================
TVec VEntity::TraceToWallSmall2D (TVec org, TVec vdelta) {
  vdelta.z = 0.0f;
  const float rad = GetMoveRadius();
  const float hgt = max2(0.0f, Height);
  const float bbox2d[4] = {
    max2(org.y, org.y+vdelta.y)+rad, // BOX2D_MAXY
    min2(org.y, org.y+vdelta.y)-rad, // BOX2D_MINY
    min2(org.x, org.x+vdelta.x)-rad, // BOX2D_MINX
    max2(org.x, org.x+vdelta.x)+rad, // BOX2D_MAXX
  };
  float bestHitTime = FLT_MAX;
  bool bestHitBevel = false;
  DeclareMakeBlockMapCoordsBBox2D(bbox2d, xl, yl, xh, yh);
  XLevel->IncrementValidCount();
  for (int bx = xl; bx <= xh; ++bx) {
    for (int by = yl; by <= yh; ++by) {
      for (auto &&it : XLevel->allBlockLines(bx, by)) {
        line_t *li = it.line();
        const bool oneSided = (!(li->flags&ML_TWOSIDED) || !li->backsector);
        bool isBlocking = (oneSided || IsBlockingLine(li));
        if (!isBlocking) continue;

        VLevel::CD_HitType hitType;
        int hitplanenum = -1;
        TVec contactPoint;
        float htime = VLevel::SweepLinedefAABB(li, org, org+vdelta, TVec(-rad, -rad, 0), TVec(rad, rad, Height), nullptr, &contactPoint, &hitType, &hitplanenum);
        if (htime >= 1.0f) continue; // didn't hit
        if (htime < 0.0f) htime = 0.0f;
        if (htime > bestHitTime) continue; // don't care, too far
        if (htime == bestHitTime && (hitplanenum > 1 || !bestHitBevel)) continue;

        // check opening on two-sided line
        if (!oneSided) {
          opening_t *open = XLevel->LineOpenings(li, contactPoint, SPF_NOBLOCKING, !(EntityFlags&EF_Missile)); // missiles ignores 3dmidtex
          open = VLevel::FindOpening(open, org.z, org.z+hgt);

          float stepHeight = MaxStepHeight;
          if (EntityFlags&EF_IgnoreFloorStep) {
            stepHeight = 0.0f;
          } else {
                 if ((EntityFlags&EF_CanJump) && Health > 0) stepHeight = 48.0f;
            else if ((EntityFlags&(EF_Missile|EF_StepMissile)) == EF_Missile) stepHeight = 0.0f;
          }

          if (open && open->range >= hgt && // fits
              open->top-org.z >= hgt && // mobj is not too high
              open->bottom-org.z <= stepHeight) // not too big a step up
          {
            // doesn't block
            if (org.z >= open->bottom) continue;
            // check to make sure there's nothing in the way for the step up
            TVec norg = org;
            norg.z = open->bottom;
            if (!TestMobjZ(norg)) continue;
          }
        }

        // yes, this is new blocker
        bestHitTime = htime;
        bestHitBevel = (hitplanenum > 1);
      }
    }
  }

  if (bestHitTime >= 1.0f) return org+vdelta;
  return org+bestHitTime*vdelta;
}


//==========================================================================
//
//  VEntity::SlidePathTraverseOld
//
//  used only for the oldest slide code!
//
//==========================================================================
void VEntity::SlidePathTraverseOld (float &BestSlideFrac, line_t *&BestSlideLine, float x, float y, float StepVelScale) {
  TVec SlideOrg(x, y, Origin.z);
  TVec SlideDir = Velocity*StepVelScale;
  const float hgt = max2(0.0f, Height);
  // old slide code
  intercept_t in;
  for (VPathTraverse It(this, &in, SlideOrg, SlideOrg+TVec(SlideDir.x, SlideDir.y, 0.0f), PT_ADDLINES|PT_NOOPENS|PT_RAILINGS); It.GetNext(); ) {
    if (!(in.Flags&intercept_t::IF_IsALine)) { continue; /*Host_Error("PTR_SlideTraverse: not a line?");*/ }
    // just in case
    if (in.frac >= 1.0f) continue; // no hit

    line_t *li = in.line;

    bool IsBlocked = false;
    if (!(li->flags&ML_TWOSIDED) || !li->backsector) {
      if (li->PointOnSide(Origin)) continue; // don't hit the back side
      IsBlocked = true;
    } else if (li->flags&(ML_BLOCKING|ML_BLOCKEVERYTHING)) {
      IsBlocked = true;
    } else if ((EntityFlags&EF_IsPlayer) && (li->flags&ML_BLOCKPLAYERS)) {
      IsBlocked = true;
    } else if ((EntityFlags&EF_CheckLineBlockMonsters) && (li->flags&ML_BLOCKMONSTERS)) {
      IsBlocked = true;
    }

    /*
    if (dbg_slide_code.asBool()) {
      if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: SlidePathTraverse: best=%g; frac=%g; line #%d; flags=0x%08x; blocked=%d", GetClass()->GetName(), BestSlideFrac, in.frac, (int)(ptrdiff_t)(li-&XLevel->Lines[0]), li->flags, (int)IsBlocked);
    }
    */

    if (!IsBlocked) {
      // set openrange, opentop, openbottom
      TVec hpoint = SlideOrg+in.frac*SlideDir;
      opening_t *open = XLevel->LineOpenings(li, hpoint, SPF_NOBLOCKING, true/*do3dmidtex*/); //!(EntityFlags&EF_Missile)); // missiles ignores 3dmidtex
      open = VLevel::FindOpening(open, Origin.z, Origin.z+hgt);

      if (open && open->range >= hgt && // fits
          open->top-Origin.z >= hgt && // mobj is not too high
          open->bottom-Origin.z <= MaxStepHeight) // not too big a step up
      {
        // this line doesn't block movement
        if (Origin.z < open->bottom) {
          // check to make sure there's nothing in the way for the step up
          TVec CheckOrg = Origin;
          CheckOrg.z = open->bottom;
          if (!TestMobjZ(CheckOrg)) continue;
        } else {
          continue;
        }
      }
    }

    // the line blocks movement, see if it is closer than best so far
    if (in.frac < BestSlideFrac) {
      BestSlideFrac = in.frac;
      BestSlideLine = li;
    }

    break;  // stop
  }
}


//==========================================================================
//
//  VEntity::SlidePathTraverseNew
//
//  returns hit time -- [0..1]
//  1 means that we couldn't find any suitable wall
//
//==========================================================================
float VEntity::SlidePathTraverseNew (const TVec dir, TVec *BestSlideNormal, line_t **BestSlideLine, bool tryBiggerBlockmap) {
  const float rad = GetMoveRadius();
  const float hgt = max2(0.0f, Height);

  float stepHeight = MaxStepHeight;
  if (EntityFlags&EF_IgnoreFloorStep) {
    stepHeight = 0.0f;
  } else {
         if ((EntityFlags&EF_CanJump) && Health > 0) stepHeight = 48.0f;
    else if ((EntityFlags&(EF_Missile|EF_StepMissile)) == EF_Missile) stepHeight = 0.0f;
  }

  float BestSlideFrac = 1.0f;
  const bool slideDiag = (dir.x && dir.y);
  bool prevClipToZero = false;
  bool lastHitWasGood = false;
  const TVec end(Origin+dir);

  //DeclareMakeBlockMapCoords(Origin.x, Origin.y, rad, xl, yl, xh, yh);
  const int bmdelta = (tryBiggerBlockmap ? 1 : 0);
  const int xl = MapBlock(Origin.x-rad-XLevel->BlockMapOrgX)-bmdelta;
  const int xh = MapBlock(Origin.x+rad-XLevel->BlockMapOrgX)+bmdelta;
  const int yl = MapBlock(Origin.y-rad-XLevel->BlockMapOrgY)-bmdelta;
  const int yh = MapBlock(Origin.y+rad-XLevel->BlockMapOrgY)+bmdelta;

  const bool debugSlide = dbg_slide_code.asBool();

  XLevel->IncrementValidCount();
  for (int bx = xl; bx <= xh; ++bx) {
    for (int by = yl; by <= yh; ++by) {
      for (auto &&it : XLevel->allBlockLines(bx, by)) {
        line_t *li = it.line();
        bool IsBlocked = false;
        if (!(li->flags&ML_TWOSIDED) || !li->backsector) {
          if (li->PointOnSide(Origin)) continue; // don't hit the back side
          IsBlocked = true;
        } else if (li->flags&(ML_BLOCKING|ML_BLOCKEVERYTHING)) {
          IsBlocked = true;
        } else if ((EntityFlags&EF_IsPlayer) && (li->flags&ML_BLOCKPLAYERS)) {
          IsBlocked = true;
        } else if ((EntityFlags&EF_CheckLineBlockMonsters) && (li->flags&ML_BLOCKMONSTERS)) {
          IsBlocked = true;
        } else {
          IsBlocked = IsBlockingLine(li);
        }

        VLevel::CD_HitType hitType;
        int hitplanenum = -1;
        TVec contactPoint;
        const float htime = VLevel::SweepLinedefAABB(li, Origin, end, TVec(-rad, -rad, 0.0f), TVec(rad, rad, hgt), nullptr, &contactPoint, &hitType, &hitplanenum);
        if (htime < 0.0f || htime >= 1.0f) continue; // didn't hit
        if (htime > BestSlideFrac) continue; // don't care, too far away
        // totally ignore beveling planes
        //if (hitplanenum > 1) continue;

        // if we already have a good wall, don't replace it with a bad wall
        if (hitplanenum > 1 && BestSlideFrac == htime) {
          if (debugSlide) {
            if (IsPlayer()) GLog.Logf(NAME_Debug, "  SIDEHIT! hpl=%d; type=%d", hitplanenum, hitType);
          }
          continue;
        }

        if (!IsBlocked) {
          // set openrange, opentop, openbottom
          opening_t *open = XLevel->LineOpenings(li, contactPoint, SPF_NOBLOCKING, !(EntityFlags&EF_Missile)); // missiles ignores 3dmidtex
          open = VLevel::FindOpening(open, Origin.z, Origin.z+hgt);

          if (open && open->range >= hgt && // fits
              open->top-Origin.z >= hgt && // mobj is not too high
              open->bottom-Origin.z <= stepHeight) // not too big a step up
          {
            // this line doesn't block movement
            if (Origin.z < open->bottom) {
              // check to make sure there's nothing in the way for the step up
              const TVec CheckOrg(Origin.x, Origin.y, open->bottom);
              if (!TestMobjZ(CheckOrg)) continue; // not blocked
            } else {
              continue; // not blocked
            }
          }
        }

        TVec hitnormal = li->normal;
        if ((li->flags&ML_TWOSIDED) && li->backsector && li->PointOnSide(Origin)) hitnormal = -hitnormal;

        // prefer walls with non-zero clipped velocity
        if (prevClipToZero && slideDiag && lastHitWasGood && hitplanenum < 2 && htime == BestSlideFrac) {
          const TVec cvel = ClipVelocity(dir, hitnormal);
          if (cvel.isZero2D()) {
            if (debugSlide) {
              if (IsPlayer()) GLog.Logf(NAME_Debug, "  CVZ-SIDEHIT! hpl=%d; type=%d", hitplanenum, hitType);
            }
            continue;
          }
          // try this hitpoint, maybe it will not clip the velocity to zero
        }

        if (debugSlide) {
          if (IsPlayer()) {
            const TVec cvel = ClipVelocity(dir, hitnormal);
            GLog.Logf(NAME_Debug, "  prevhittime=%g; newhittime=%g; newhitnormal=(%g,%g); newhitpnum=%d; newdir=(%g,%g)", BestSlideFrac, htime, hitnormal.x, hitnormal.y, hitplanenum, cvel.x, cvel.y);
          }
        }
        BestSlideFrac = htime;
        if (BestSlideNormal) *BestSlideNormal = hitnormal;
        if (BestSlideLine) *BestSlideLine = li;
        lastHitWasGood = (hitplanenum < 2);
        if (slideDiag) prevClipToZero = !ClipVelocity(dir, hitnormal).isZero2D();
      }
    }
  }

  return BestSlideFrac;
}


//==========================================================================
//
//  VEntity::SlideMoveVanilla
//
//  The momx / momy move is bad, so try to slide along a wall.
//  Find the first line hit, move flush to it, and slide along it.
//  This is a kludgy mess.
//
//  `noPickups` means "don't activate specials" too.
//
//==========================================================================
void VEntity::SlideMoveVanilla (float StepVelScale, bool noPickups) {
  tmtrace_t tmtrace;
  memset((void *)&tmtrace, 0, sizeof(tmtrace)); // valgrind: AnyBlockingLine

  float XMove = Velocity.x*StepVelScale;
  float YMove = Velocity.y*StepVelScale;
  if (XMove == 0.0f && YMove == 0.0f) return; // just in case

  const float rad = GetMoveRadius();

  // old slide
  float leadx, leady;
  float trailx, traily;

  float prevVelX = XMove;
  float prevVelY = YMove;

  int hitcount = 0;
  do {
    if (++hitcount == 3) {
      // don't loop forever
      bool movedY = (YMove != 0.0f ? TryMove(tmtrace, TVec(Origin.x, Origin.y+YMove, Origin.z), true, noPickups) : false);
      if (!movedY && XMove != 0.0f) movedY = TryMove(tmtrace, TVec(Origin.x+XMove, Origin.y, Origin.z), true, noPickups);
      if (!movedY) Velocity.x = Velocity.y = 0.0f;
      return;
    }

    if (XMove != 0.0f) prevVelX = XMove;
    if (YMove != 0.0f) prevVelY = YMove;

    // trace along the three leading corners
    if (/*XMove*/prevVelX > 0.0f) {
      leadx = Origin.x+rad;
      trailx = Origin.x-rad;
    } else {
      leadx = Origin.x-rad;
      trailx = Origin.x+rad;
    }

    if (/*Velocity.y*/prevVelY > 0.0f) {
      leady = Origin.y+rad;
      traily = Origin.y-rad;
    } else {
      leady = Origin.y-rad;
      traily = Origin.y+rad;
    }

    float BestSlideFrac = 666.0f; //1.00001f; -- arbitrary big number
    line_t *BestSlideLine = nullptr;

    SlidePathTraverseOld(BestSlideFrac, BestSlideLine, leadx, leady, StepVelScale);
    if (BestSlideFrac != 0.0f) SlidePathTraverseOld(BestSlideFrac, BestSlideLine, trailx, leady, StepVelScale);
    if (BestSlideFrac != 0.0f) SlidePathTraverseOld(BestSlideFrac, BestSlideLine, leadx, traily, StepVelScale);

    // move up to the wall
    if (BestSlideFrac >= 1.0f /*== 1.00001f*/) {
      // the move must have hit the middle, so stairstep
      bool movedY = (YMove != 0.0f ? TryMove(tmtrace, TVec(Origin.x, Origin.y+YMove, Origin.z), true, noPickups) : false);
      if (!movedY && XMove != 0.0f) movedY = TryMove(tmtrace, TVec(Origin.x+XMove, Origin.y, Origin.z), true, noPickups);
      if (!movedY) Velocity.x = Velocity.y = 0.0f;
      return;
    }

    // fudge a bit to make sure it doesn't hit
    const float origSlide = BestSlideFrac; // we'll need it later
    BestSlideFrac -= 0.03125f;
    if (BestSlideFrac > 0.0f) {
      const float newx = XMove*BestSlideFrac;
      const float newy = YMove*BestSlideFrac;
      if (!TryMove(tmtrace, TVec(Origin.x+newx, Origin.y+newy, Origin.z), true, noPickups)) {
        bool movedY = (YMove != 0.0f ? TryMove(tmtrace, TVec(Origin.x, Origin.y+YMove, Origin.z), true, noPickups) : false);
        if (!movedY && XMove != 0.0f) movedY = TryMove(tmtrace, TVec(Origin.x+XMove, Origin.y, Origin.z), true, noPickups);
        if (!movedY) Velocity.x = Velocity.y = 0.0f;
        return;
      }
    }

    // now continue along the wall
    // first calculate remainder
    BestSlideFrac = 1.0f-origSlide;

    if (BestSlideFrac > 1.0f) BestSlideFrac = 1.0f;
    if (BestSlideFrac <= 0.0f) return;

    // clip the moves
    // k8: don't multiply z, 'cause it makes jumping against a wall unpredictably hard
    Velocity.x *= BestSlideFrac;
    Velocity.y *= BestSlideFrac;
    Velocity = ClipVelocity(Velocity, BestSlideLine->normal);
    //Velocity = ClipVelocity(Velocity*BestSlideFrac, BestSlideLine->normal);
    XMove = Velocity.x*StepVelScale;
    YMove = Velocity.y*StepVelScale;
  } while (!TryMove(tmtrace, TVec(Origin.x+XMove, Origin.y+YMove, Origin.z), true, noPickups));
}


#define SLIDE_MOVE_FINALTRY()  do { \
  if (dir.y == 0.0f || !TryMove(tmtrace, TVec(Origin.x, Origin.y+dir.y, Origin.z), true, noPickups)) { \
    if (dir.x == 0.0f || !TryMove(tmtrace, TVec(Origin.x+dir.x, Origin.y, Origin.z), true, noPickups)) { \
      if (dir.x || dir.y) Velocity.x = Velocity.y = 0.0f; \
    } \
  } \
} while (0)

//==========================================================================
//
//  VEntity::SlideMoveSweep
//
//  The momx / momy move is bad, so try to slide along a wall.
//  Find the first line hit, move flush to it, and slide along it.
//
//  `noPickups` means "don't activate specials" too.
//
//==========================================================================
void VEntity::SlideMoveSweep (float StepVelScale, bool noPickups) {
  tmtrace_t tmtrace;

  Velocity.z = 0.0f;

  TVec dir(Velocity*StepVelScale); // we want to arrive there
  if (dir.x == 0.0f && dir.y == 0.0f) return; // just in case

  const bool debugSlide = dbg_slide_code.asBool();

  if (debugSlide) {
    if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: === SlideMove; move=(%g,%g) pos=(%g,%g,%g) ===", GetClass()->GetName(), dir.x, dir.y, Origin.x, Origin.y, Origin.z);
  }

  int hitcount = 0;
  do {
    if (++hitcount == 4) {
      // don't loop forever
      if (debugSlide) {
        if (IsPlayer()) GCon->Logf(NAME_Debug, " STUCK! hitcount=%d; dir=(%g,%g)", hitcount, dir.x, dir.y);
      }
      //SLIDE_MOVE_FINALTRY();
      //return;
      return SlideMoveVanilla(StepVelScale, noPickups);
    }

    if (debugSlide) {
      if (IsPlayer()) GCon->Logf(NAME_Debug, " hitcount=%d; dir=(%g,%g)", hitcount, dir.x, dir.y);
    }

    // find line to slide
    TVec BestSlideNormal(0.0f, 0.0f, 0.0f);
    float BestSlideFrac = SlidePathTraverseNew(dir, &BestSlideNormal);

    // move up to the wall
    if (BestSlideFrac >= 1.0f) {
      // the move must have hit the middle, so stairstep
      if (debugSlide) {
        if (IsPlayer()) GCon->Logf(NAME_Debug, "  DONE! BestSlideFrac=%g", BestSlideFrac);
      }
      if (TryMove(tmtrace, TVec(Origin.x+dir.x, Origin.y+dir.y, Origin.z), true, noPickups)) return;
      // if we can't move, try better search
      if (debugSlide) {
        if (IsPlayer()) GCon->Logf(NAME_Debug, "    CANNOT MOVE!");
      }
      // this can happen because our sweeping function is not exactly the same as `TryMove()`
      // so swept box can hit nothing, yet `TryMove()` could find a collision
      // in this case we will make our direction longer to find that pesky wall to slide
      TVec ndir(dir);
      ndir += dir.normalised2D()*8.0f;
      BestSlideFrac = SlidePathTraverseNew(ndir, &BestSlideNormal, nullptr, true);
      if (BestSlideFrac >= 1.0f) {
        // still no wall, give up
        if (debugSlide) {
          if (IsPlayer()) GCon->Logf(NAME_Debug, "    ...and giving up.");
        }
        //SLIDE_MOVE_FINALTRY();
        //return;
        // just in case, it may unstuck sometimes
        return SlideMoveVanilla(StepVelScale, noPickups);
      }
      // this slide time is not right, zero it
      BestSlideFrac = 0.0f;
    }

    if (BestSlideFrac > 0.0f) {
      if (!TryMove(tmtrace, TVec(Origin.x+dir.x, Origin.y+dir.y, Origin.z), true, noPickups)) {
        // fudge a bit to make sure it doesn't hit
        const float slidefrac = BestSlideFrac-0.03125f;
        if (slidefrac > 0.0f) {
          const float newx = dir.x*slidefrac;
          const float newy = dir.y*slidefrac;
          if (!TryMove(tmtrace, TVec(Origin.x+newx, Origin.y+newy, Origin.z), true, noPickups)) {
            if (debugSlide) {
              if (IsPlayer()) GLog.Logf(NAME_Debug, "  CAN'T SLIDE; newdir=(%g,%g)", newx, newy);
            }
            //dir.x = newx;
            //dir.y = newy;
            //SLIDE_MOVE_FINALTRY();
            //return;
            return SlideMoveVanilla(StepVelScale, noPickups);
          }
        }
      }
    }

    // now continue along the wall
    // first calculate remainder
    BestSlideFrac = 1.0f-BestSlideFrac;
    if (debugSlide) {
      if (IsPlayer()) GLog.Logf(NAME_Debug, "  fracleft=%g; vel=(%g,%g)", BestSlideFrac, Velocity.x, Velocity.y);
    }

    //if (BestSlideFrac > 1.0f) BestSlideFrac = 1.0f;
    if (BestSlideFrac <= 0.0f) return; // fully moved (this cannot happen, but...)

    // clip the moves
    // k8: don't multiply z, 'cause it makes jumping against a wall unpredictably hard
    Velocity.x *= BestSlideFrac;
    Velocity.y *= BestSlideFrac;
    Velocity = ClipVelocity(Velocity, BestSlideNormal);
    Velocity.z = 0.0f; // just in case

    dir.x = Velocity.x*StepVelScale;
    dir.y = Velocity.y*StepVelScale;

    if (debugSlide) {
      if (IsPlayer()) GLog.Logf(NAME_Debug, "  newvel=(%g,%g)", Velocity.x, Velocity.y);
    }
  } while (!TryMove(tmtrace, TVec(Origin.x+dir.x, Origin.y+dir.y, Origin.z), true, noPickups));
}

#undef SLIDE_MOVE_FINALTRY


//==========================================================================
//
//  VEntity::SlideMoveQ3Like
//
//  The momx / momy move is bad, so try to slide along a wall.
//  Find the first line hit, move flush to it, and slide along it.
//
//  `noPickups` means "don't activate specials" too.
//
//==========================================================================
void VEntity::SlideMoveQ3Like (float StepVelScale, bool noPickups) {
  tmtrace_t tmtrace;

  const bool debugSlide = dbg_slide_code.asBool();

  if (debugSlide) {
    if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: === SlideMove; move=(%g,%g) pos=(%g,%g,%g) ===", GetClass()->GetName(), Velocity.x*StepVelScale, Velocity.y*StepVelScale, Origin.x, Origin.y, Origin.z);
  }

  Velocity.z = 0.0f;

  int i;
  TVec planes[/*MAX_CLIP_PLANES*/5];
  int numplanes = 0;

  const bool useTimeLeft = dbg_slide_code.asBool();

  int trynum = 0;
  for (;;) {
    TVec dir(Velocity);
    dir.z = 0.0f;
    dir *= StepVelScale; // we want to arrive here

    // just in case
    if (TryMove(tmtrace, TVec(Origin+dir), true, noPickups)) return;

    if (++trynum == 4) {
      // don't loop forever
      if (debugSlide) {
        if (IsPlayer()) GCon->Logf(NAME_Debug, " STUCK! hitcount=%d; dir=(%g,%g)", trynum, dir.x, dir.y);
      }
      //Velocity.x = Velocity.y = 0.0f;
      //return;
      return SlideMoveVanilla(StepVelScale, noPickups);
    }

    if (debugSlide) {
      if (IsPlayer()) GCon->Logf(NAME_Debug, " trynum=%d; dir=(%g,%g)", trynum, dir.x, dir.y);
    }

    // find line to slide
    TVec BestSlideNormal(0.0f, 0.0f, 0.0f);
    float BestSlideFrac = SlidePathTraverseNew(dir, &BestSlideNormal);

    if (BestSlideFrac >= 1.0f) {
      // the move must have hit the middle, so stairstep
      if (debugSlide) {
        if (IsPlayer()) GCon->Logf(NAME_Debug, "  DONE! BestSlideFrac=%g", BestSlideFrac);
      }
      if (TryMove(tmtrace, TVec(Origin.x+dir.x, Origin.y+dir.y, Origin.z), true, noPickups)) return;
      // if we can't move, try better search
      if (debugSlide) {
        if (IsPlayer()) GCon->Logf(NAME_Debug, "    CANNOT MOVE!");
      }
      // this can happen because our sweeping function is not exactly the same as `TryMove()`
      // so swept box can hit nothing, yet `TryMove()` could find a collision
      // in this case we will make our direction longer to find that pesky wall to slide
      TVec ndir(dir);
      ndir += dir.normalised2D()*8.0f;
      BestSlideFrac = SlidePathTraverseNew(ndir, &BestSlideNormal, nullptr, true);
      if (BestSlideFrac >= 1.0f) {
        // still no wall, give up
        if (debugSlide) {
          if (IsPlayer()) GCon->Logf(NAME_Debug, "    ...and giving up.");
        }
        // just in case, it may unstuck sometimes
        //Velocity.x = Velocity.y = 0.0f;
        //return;
        return SlideMoveVanilla(StepVelScale, noPickups);
      }
      // this slide time is not right, zero it
      BestSlideFrac = 0.0f;
    }

    if (BestSlideFrac > 0.0f) {
      if (!TryMove(tmtrace, Origin+dir, true, noPickups)) {
        // make hittime slightly smaller, to avoid walls stucking
        const float slidetime = BestSlideFrac-0.03125f;
        if (slidetime > 0.0f) {
          const float newx = dir.x*slidetime;
          const float newy = dir.y*slidetime;
          if (!TryMove(tmtrace, TVec(Origin.x+newx, Origin.y+newy, Origin.z), true, noPickups)) {
            if (debugSlide) {
              if (IsPlayer()) GLog.Logf(NAME_Debug, "  CAN'T SLIDE AT ALL!");
            }
            //Velocity.x = Velocity.y = 0.0f;
            //return;
            return SlideMoveVanilla(StepVelScale, noPickups);
          }
        }
      }
    }

    // fix timeleft
    if (useTimeLeft) {
      StepVelScale -= StepVelScale*BestSlideFrac;
      if (debugSlide) {
        if (IsPlayer()) GLog.Logf(NAME_Debug, "  timeleft=%g; vel=(%g,%g)", StepVelScale, Velocity.x, Velocity.y);
      }
      if (StepVelScale <= 0.0f) return; // fully moved (this cannot happen, but...)
    } else {
      BestSlideFrac = 1.0f-BestSlideFrac;
      if (debugSlide) {
        if (IsPlayer()) GLog.Logf(NAME_Debug, "  fracleft=%g; vel=(%g,%g)", BestSlideFrac, Velocity.x, Velocity.y);
      }
      if (BestSlideFrac <= 0.0f) return; // fully moved (this cannot happen, but...)

      // clip the moves
      // k8: don't multiply z, 'cause it makes jumping against a wall unpredictably hard
      Velocity.x *= BestSlideFrac;
      Velocity.y *= BestSlideFrac;
    }

    // if this is the same plane we hit before, nudge velocity out along it,
    // which fixes some epsilon issues with non-axial planes
    for (i = 0; i < numplanes; ++i) {
      if (BestSlideNormal.dot(planes[i]) > 0.99f) {
        Velocity += BestSlideNormal;
        break;
      }
    }
    if (i < numplanes) continue; // already seen it
    planes[numplanes++] = BestSlideNormal;

    // modify velocity so it parallels all of the clip planes

    // find a plane that it enters
    for (i = 0; i < numplanes; ++i) {
      float into = Velocity.dot(planes[i]);
      if (into >= 0.1f) {
        // move doesn't interact with the plane
        if (debugSlide) {
          if (IsPlayer()) GLog.Logf(NAME_Debug, "   skipped %d (into=%g)", i, into);
        }
        continue;
      }

      TVec clipVelocity = ClipVelocity(Velocity, planes[i]);

      if (debugSlide) {
        if (IsPlayer()) GLog.Logf(NAME_Debug, "   vel=(%g,%g); cvel=(%g,%g)", Velocity.x, Velocity.y, clipVelocity.x, clipVelocity.y);
      }

      // see if there is a second plane that the new move enters
      for (int j = 0; j < numplanes; ++j) {
        if (j == i) continue;
        if (clipVelocity.dot(planes[j]) >= 0.1f) continue; // move doesn't interact with the plane

        // try clipping the move to the plane
        clipVelocity = ClipVelocity(clipVelocity, planes[j]);

        // see if it goes back into the first clip plane
        if (clipVelocity.dot(planes[i]) >= 0.0f) continue;

        // see if there is a third plane the new move enters
        for (int k = 0; k < numplanes; ++k) {
          if (k == i || k == j) continue;
          if (clipVelocity.dot(planes[k]) >= 0.1f) continue; // move doesn't interact with the plane
          // stop dead at a tripple plane interaction
          if (debugSlide) {
            if (IsPlayer()) GLog.Logf(NAME_Debug, "   TRIPLEPLANE!");
          }
          Velocity.x = Velocity.y = 0.0f;
          return;
        }
      }

      // if we have fixed all interactions, try another move
      Velocity = clipVelocity;
    }

    // clamp velocity
    //Velocity -= BestSlideNormal*Velocity.dot(BestSlideNormal);

    if (debugSlide) {
      if (IsPlayer()) GLog.Logf(NAME_Debug, "  newvel=(%g,%g)", Velocity.x, Velocity.y);
    }
  }
}


//==========================================================================
//
//  VEntity::SlideMove
//
//  The momx / momy move is bad, so try to slide along a wall.
//  Find the first line hit, move flush to it, and slide along it.
//  This is a kludgy mess.
//
//  `noPickups` means "don't activate specials" too.
//
//  k8: TODO: switch to beveled BSP!
//
//==========================================================================
void VEntity::SlideMove (float StepVelScale, bool noPickups) {
  const float oldvelz = Velocity.z;
  const int slideType = gm_slide_code.asInt();
       if (slideType >= 2) SlideMoveQ3Like(StepVelScale, noPickups);
  else if (slideType == 1) return SlideMoveSweep(StepVelScale, noPickups);
  else SlideMoveVanilla(StepVelScale, noPickups);
  Velocity.z = oldvelz;
}


//==========================================================================
//
//  VEntity::SlideMoveCamera
//
//  this is used to move chase camera
//
//==========================================================================
TVec VEntity::SlideMoveCamera (TVec org, TVec end, float radius) {
  TVec velo = end-org;
  if (!velo.isValid() || velo.isZero()) return org;

  if (radius < 2) {
    // just trace
    linetrace_t ltr;
    if (XLevel->TraceLine(ltr, org, end, 0/*|SPF_NOBLOCKSIGHT*/|SPF_HITINFO)) return end; // no hit
    TVec hp = org+(end-org)*ltr.HitTime;
    //TVec norm = ltr.HitPlane.normal;
    //if (ltr.HitLine && ltr.HitLine->PointOnSide(org)) norm = -ltr.HitPlane.normal;
    // hit something, move slightly forward
    TVec mdelta = hp-org;
    //const float wantdist = velo.length();
    const float movedist = mdelta.length();
    if (movedist > 2.0f) {
      //GCon->Logf("*** hit! (%g,%g,%g)", ltr.HitPlaneNormal.x, ltr.HitPlaneNormal.y, ltr.HitPlaneNormal.z);
      if (ltr.HitPlane.normal.z != 0.0f) {
        // floor
        //GCon->Logf("floor hit! (%g,%g,%g)", ltr.HitPlaneNormal.x, ltr.HitPlaneNormal.y, ltr.HitPlaneNormal.z);
        hp += ltr.HitPlane.normal*2.0f;
      } else {
        hp -= mdelta.normalised()*2.0f;
      }
    }
    return hp;
  }

  // split move in multiple steps if moving too fast
  const float xmove = velo.x;
  const float ymove = velo.y;

  int Steps = 1;
  float XStep = fabsf(xmove);
  float YStep = fabsf(ymove);
  float MaxStep = radius-1.0f;

  if (XStep > MaxStep || YStep > MaxStep) {
    if (XStep > YStep) {
      Steps = int(XStep/MaxStep)+1;
    } else {
      Steps = int(YStep/MaxStep)+1;
    }
  }

  const float StepXMove = xmove/float(Steps);
  const float StepYMove = ymove/float(Steps);
  const float StepZMove = velo.z/float(Steps);

  //GCon->Logf("*** *** Steps=%d; move=(%g,%g,%g); onestep=(%g,%g,%g)", Steps, xmove, ymove, velo.z, StepXMove, StepYMove, StepZMove);
  tmtrace_t tmtrace;
  for (int step = 0; step < Steps; ++step) {
    float ptryx = org.x+StepXMove;
    float ptryy = org.y+StepYMove;
    float ptryz = org.z+StepZMove;

    TVec newPos = TVec(ptryx, ptryy, ptryz);
    bool check = CheckRelPosition(tmtrace, newPos, true);
    if (check) {
      org = newPos;
      continue;
    }

    // blocked move; trace back until we got a good position
    // this sux, but we'll do it only once per frame, so...
    float len = (newPos-org).length();
    TVec dir = (newPos-org).normalised(); // to newpos
    //GCon->Logf("*** len=%g; dir=(%g,%g,%g)", len, dir.x, dir.y, dir.z);
    float curr = 1.0f;
    while (curr <= len) {
      if (!CheckRelPosition(tmtrace, org+dir*curr, true)) {
        curr -= 1.0f;
        break;
      }
      curr += 1.0f;
    }
    if (curr > len) curr = len;
    //GCon->Logf("   final len=%g", curr);
    org += dir*curr;
    break;
  }

  // clamp to floor/ceiling
  CheckRelPosition(tmtrace, org, true);
  if (org.z < tmtrace.FloorZ+radius) org.z = tmtrace.FloorZ+radius;
  if (org.z > tmtrace.CeilingZ-radius) org.z = tmtrace.CeilingZ-radius;
  return org;
}



//==========================================================================
//
//  Script natives
//
//==========================================================================
IMPLEMENT_FUNCTION(VEntity, SlideMove) {
  float StepVelScale;
  VOptParamBool noPickups(false);
  vobjGetParamSelf(StepVelScale, noPickups);
  Self->SlideMove(StepVelScale, noPickups);
}

// native final TVec TraceToWallSmall2D (const TVec org, const TVec vdelta);
IMPLEMENT_FUNCTION(VEntity, TraceToWallSmall2D) {
  TVec org, vdelta;
  vobjGetParamSelf(org, vdelta);
  if (vdelta.isZero2D()) {
    RET_VEC(org);
  } else {
    RET_VEC(Self->TraceToWallSmall2D(org, vdelta));
  }
}
