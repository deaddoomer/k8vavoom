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

//#define VV_DBG_VERBOSE_SLIDE

//#define VV_USE_NEWEST_SLIDE
#define VV_USE_Q3_SLIDE_CODE


// ////////////////////////////////////////////////////////////////////////// //
static VCvarB gm_use_new_slide_code("gm_use_new_slide_code", true, "Use new sliding code (experimental)?", CVAR_Archive);
static VCvarB gm_use_experimental_slide("gm_use_experimental_slide", false, "Use new VERY experimental sliding code?", CVAR_Archive);



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

    #ifdef VV_DBG_VERBOSE_SLIDE
    if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: SlidePathTraverse: best=%g; frac=%g; line #%d; flags=0x%08x; blocked=%d", GetClass()->GetName(), BestSlideFrac, in.frac, (int)(ptrdiff_t)(li-&XLevel->Lines[0]), li->flags, (int)IsBlocked);
    #endif

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
//  VEntity::SlideMoveOldest (vanilla-like)
//
//  The momx / momy move is bad, so try to slide along a wall.
//  Find the first line hit, move flush to it, and slide along it.
//  This is a kludgy mess.
//
//  `noPickups` means "don't activate specials" too.
//
//==========================================================================
void VEntity::SlideMoveOldest (float StepVelScale, bool noPickups) {
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
      const bool movedY = (YMove != 0.0f ? TryMove(tmtrace, TVec(Origin.x, Origin.y+YMove, Origin.z), true, noPickups) : false);
      if (!movedY) TryMove(tmtrace, TVec(Origin.x+XMove, Origin.y, Origin.z), true, noPickups);
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
      const bool movedY = (YMove != 0.0f ? TryMove(tmtrace, TVec(Origin.x, Origin.y+YMove, Origin.z), true, noPickups) : false);
      if (!movedY) TryMove(tmtrace, TVec(Origin.x+XMove, Origin.y, Origin.z), true, noPickups);
      return;
    }

    // fudge a bit to make sure it doesn't hit
    const float origSlide = BestSlideFrac; // we'll need it later
    BestSlideFrac -= 0.03125f;
    if (BestSlideFrac > 0.0f) {
      const float newx = XMove*BestSlideFrac;
      const float newy = YMove*BestSlideFrac;

      if (!TryMove(tmtrace, TVec(Origin.x+newx, Origin.y+newy, Origin.z), true, noPickups)) {
        const bool movedY = (YMove != 0.0f ? TryMove(tmtrace, TVec(Origin.x, Origin.y+YMove, Origin.z), true, noPickups) : false);
        if (!movedY) TryMove(tmtrace, TVec(Origin.x+XMove, Origin.y, Origin.z), true, noPickups);
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


#if 0
#define SLIDE_MOVE_FINALTRY(dodiag)  do { \
  if (!dodiag || !TryMove(tmtrace, TVec(Origin.x+dir.x, Origin.y+dir.y, Origin.z), true, noPickups)) { \
    bool moved = false; \
    TVec ndir(dir); \
    for (int ddg = 0; ddg < 2; ++ddg) { \
      ndir *= 0.4f; \
      if (TryMove(tmtrace, TVec(Origin.x+ndir.x, Origin.y+ndir.y, Origin.z), true, noPickups)) { moved = true; break; } \
    } \
    if (!moved) { \
      for (int ddg = 0; ddg < 2; ++ddg) { \
        if (dir.y != 0.0f && TryMove(tmtrace, TVec(Origin.x, Origin.y+dir.y, Origin.z), true, noPickups)) { moved = true; break; } \
        if (dir.x != 0.0f && TryMove(tmtrace, TVec(Origin.x+dir.x, Origin.y, Origin.z), true, noPickups)) { moved = true; break; } \
        dir *= 0.4f; \
      } \
    } \
    if (!moved) Velocity.x = Velocity.y = 0.0f; \
  } \
} while (0)

#else

#define SLIDE_MOVE_FINALTRY(dodiag)  do { \
  if (!dodiag || !TryMove(tmtrace, TVec(Origin.x+dir.x, Origin.y+dir.y, Origin.z), true, noPickups)) { \
    if (dir.y == 0.0f || !TryMove(tmtrace, TVec(Origin.x, Origin.y+dir.y, Origin.z), true, noPickups)) { \
      if (dir.x == 0.0f || !TryMove(tmtrace, TVec(Origin.x+dir.x, Origin.y, Origin.z), true, noPickups)) { \
        if (dir.x || dir.y) Velocity.x = Velocity.y = 0.0f; \
      } \
    } \
  } \
} while (0)
#endif


//==========================================================================
//
//  VEntity::SlideMoveNew (current)
//
//  The momx / momy move is bad, so try to slide along a wall.
//  Find the first line hit, move flush to it, and slide along it.
//
//  `noPickups` means "don't activate specials" too.
//
//==========================================================================
void VEntity::SlideMoveNew (float StepVelScale, bool noPickups) {
  tmtrace_t tmtrace;

  Velocity.z = 0.0f;

  TVec dir(Velocity*StepVelScale); // we want to arrive there
  if (dir.x == 0.0f && dir.y == 0.0f) return; // just in case

  const float rad = GetMoveRadius();
  const float hgt = max2(0.0f, Height);

  float stepHeight = MaxStepHeight;
  if (EntityFlags&EF_IgnoreFloorStep) {
    stepHeight = 0.0f;
  } else {
         if ((EntityFlags&EF_CanJump) && Health > 0) stepHeight = 48.0f;
    else if ((EntityFlags&(EF_Missile|EF_StepMissile)) == EF_Missile) stepHeight = 0.0f;
  }

  int hitcount = 0;
  do {
    if (++hitcount == 3) {
      // don't loop forever
      SLIDE_MOVE_FINALTRY(false); // just in case, no first diagonal
      return;
    }

    float BestSlideFrac = 666.0f; //1.00001f; -- arbitrary big number
    TVec BestSlideNormal(0.0f, 0.0f, 0.0f);

    // find line to slide
    {
      const bool slideDiag = (dir.x && dir.y);
      bool prevClipToZero = false;
      bool lastHitWasGood = false;
      DeclareMakeBlockMapCoords(Origin.x, Origin.y, rad, xl, yl, xh, yh);
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
            const float htime = VLevel::SweepLinedefAABB(li, Origin, Origin+dir, TVec(-rad, -rad, 0), TVec(rad, rad, hgt), nullptr, &contactPoint, &hitType, &hitplanenum);
            if (htime < 0.0f || htime >= 1.0f) continue; // didn't hit
            if (htime > BestSlideFrac) continue; // don't care, too far away
            // totally ignore beveling planes
            if (hitplanenum > 1) continue;

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
                  TVec CheckOrg = Origin;
                  CheckOrg.z = open->bottom;
                  if (!TestMobjZ(CheckOrg)) continue; // not blocked
                } else {
                  continue; // not blocked
                }
              }
            }

            TVec hitnormal = li->normal;
            if ((li->flags&ML_TWOSIDED) && li->backsector && li->PointOnSide(Origin)) hitnormal = -hitnormal;

            // current hit is a good one, replace bad one
            if (!lastHitWasGood && hitplanenum < 2 && BestSlideFrac == htime) {
              // nothing to do here
            } else {
              // prefer walls with non-zero clipped velocity
              if (prevClipToZero && slideDiag && hitplanenum < 2 && htime == BestSlideFrac) {
                const TVec cvel = ClipVelocity(dir, hitnormal);
                if (cvel.isZero2D()) continue;
                // try this hitpoint, maybe it will not clip the velocity to zero
              }
            }

            BestSlideFrac = htime;
            BestSlideNormal = hitnormal;
            lastHitWasGood = (hitplanenum < 2);
            if (slideDiag) prevClipToZero = !ClipVelocity(dir, hitnormal).isZero2D();
          }
        }
      }
    }

    // move up to the wall
    if (BestSlideFrac >= 1.0f) {
      // the move must have hit the middle, so stairstep
      SLIDE_MOVE_FINALTRY(true); // just in case, try diagonal too
      return;
    }

    // fudge a bit to make sure it doesn't hit
    const float slidefrac = BestSlideFrac-0.03125f;
    if (slidefrac > 0.0f) {
      const float newx = dir.x*slidefrac;
      const float newy = dir.y*slidefrac;
      if (!TryMove(tmtrace, TVec(Origin.x+newx, Origin.y+newy, Origin.z), true, noPickups)) {
        dir.x = newx;
        dir.y = newy;
        SLIDE_MOVE_FINALTRY(false); // no first diagonal
        return;
      }
    }

    // now continue along the wall
    // first calculate remainder
    BestSlideFrac = 1.0f-BestSlideFrac;

    if (BestSlideFrac > 1.0f) BestSlideFrac = 1.0f;
    if (BestSlideFrac <= 0.0f) return; // just in case

    // clip the moves
    // k8: don't multiply z, 'cause it makes jumping against a wall unpredictably hard
    Velocity.x *= BestSlideFrac;
    Velocity.y *= BestSlideFrac;
    Velocity = ClipVelocity(Velocity, BestSlideNormal);
    Velocity.z = 0.0f; // just in case

    dir.x = Velocity.x*StepVelScale;
    dir.y = Velocity.y*StepVelScale;
  } while (!TryMove(tmtrace, TVec(Origin.x+dir.x, Origin.y+dir.y, Origin.z), true, noPickups));
}


//==========================================================================
//
//  VEntity::SlideMoveNew (q3 experiment, doesn't work yet)
//
//  The momx / momy move is bad, so try to slide along a wall.
//  Find the first line hit, move flush to it, and slide along it.
//
//  `noPickups` means "don't activate specials" too.
//
//==========================================================================
void VEntity::SlideMoveNewest (float StepVelScale, bool noPickups) {
  tmtrace_t tmtrace;

  #ifdef VV_DBG_VERBOSE_SLIDE
  if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: === SlideMove; move=(%g,%g) pos=(%g,%g,%g) ===", GetClass()->GetName(), XMove, YMove, Origin.x, Origin.y, Origin.z);
  #endif

  const float rad = GetMoveRadius();

  #ifdef VV_USE_Q3_SLIDE_CODE
  int i;
  TVec planes[/*MAX_CLIP_PLANES*/5];
  int numplanes = 0;
  #endif

  const float hgt = max2(0.0f, Height);
  float stepHeight = MaxStepHeight;
  if (EntityFlags&EF_IgnoreFloorStep) {
    stepHeight = 0.0f;
  } else {
         if ((EntityFlags&EF_CanJump) && Health > 0) stepHeight = 48.0f;
    else if ((EntityFlags&(EF_Missile|EF_StepMissile)) == EF_Missile) stepHeight = 0.0f;
  }

  //float timeleft = StepVelScale;
  for (int trynum = 0; trynum < 4; ++trynum) {
    TVec dir(Velocity);
    dir.z = 0.0f;
    dir *= StepVelScale; // we want to arrive here

    if (dir.x == 0.0f && dir.y == 0.0f) return; // just in case

    float hittime = 666.0f;
    bool hitgoodplane = false;
    TVec hitnormal(0.0f, 0.0f, 0.0f);

    // find "first hit" line
    DeclareMakeBlockMapCoords(Origin.x, Origin.y, rad, xl, yl, xh, yh);
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
          const float htime = VLevel::SweepLinedefAABB(li, Origin, Origin+dir, TVec(-rad, -rad, 0), TVec(rad, rad, hgt), nullptr, &contactPoint, &hitType, &hitplanenum);
          //if (htime < 0.0f) { hittime = -1.0f; break; } // stuck
          //if (htime >= 1.0f) continue; // didn't hit
          if (htime < 0.0f || htime >= 1.0f) continue; // didn't hit
          if (htime > hittime) continue; // don't care, too far away

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
                TVec CheckOrg = Origin;
                CheckOrg.z = open->bottom;
                if (!TestMobjZ(CheckOrg)) continue; // not blocked
              } else {
                continue; // not blocked
              }
            }
          }

          // prefer non-point hits
          if (htime < hittime || (!hitgoodplane && hitplanenum < 2 && htime == hittime)) {
            hitnormal = li->normal;
            if ((li->flags&ML_TWOSIDED) && li->backsector && li->PointOnSide(Origin)) hitnormal = -hitnormal;
            hittime = htime;
            hitgoodplane = (hitplanenum < 2);
          }
        }
      }
    } // done looking for hit line

    if (hittime >= 1.0f) {
      // the move must have hit the middle, so stairstep
      bool movedY = (dir.y != 0.0f ? TryMove(tmtrace, TVec(Origin.x, Origin.y+dir.y, Origin.z), true, noPickups) : false);
      if (!movedY && dir.x) movedY = TryMove(tmtrace, TVec(Origin.x+dir.x, Origin.y, Origin.z), true, noPickups);
      if (!movedY) Velocity.x = Velocity.y = 0.0f;
      return;
    }

    /*
    if (hittime < 0.0f) {
      // completely stuck
      Velocity.x = 0.0f;
      Velocity.y = 0.0f;
      return;
    }
    */

    // make hittime slightly smaller, to avoid stucking in the walls
    const float slidetime = hittime-0.03125f;
    if (slidetime > 0.0f) {
      const float newx = dir.x*slidetime;
      const float newy = dir.y*slidetime;

      if (!TryMove(tmtrace, TVec(Origin.x+newx, Origin.y+newy, Origin.z), true, noPickups)) {
        bool movedY = (dir.x != 0.0f ? TryMove(tmtrace, TVec(Origin.x, Origin.y+dir.x, Origin.z), true, noPickups) : false);
        if (!movedY && dir.y) movedY = TryMove(tmtrace, TVec(Origin.x+dir.y, Origin.y, Origin.z), true, noPickups);
        if (!movedY) Velocity.x = Velocity.y = 0.0f;
        return;
      }
    }

    // fix timeleft
    hittime = 1.0f-hittime;
    // clip the moves
    // k8: don't multiply z, 'cause it makes jumping against a wall unpredictably hard
    Velocity.x *= hittime;
    Velocity.y *= hittime;

    // calculate new destination point
    //end = mobj.origin+hitline.plane.normal*dir.dot(hitline.plane.normal);

    #ifdef VV_USE_Q3_SLIDE_CODE
    // if this is the same plane we hit before, nudge velocity out along it,
    // which fixes some epsilon issues with non-axial planes
    for (i = 0; i < numplanes; ++i) {
      if (hitnormal.dot(planes[i]) > 0.99f) {
        Velocity += hitnormal;
        break;
      }
    }
    if (i < numplanes) continue; // already seen it
    planes[numplanes++] = hitnormal;

    // modify velocity so it parallels all of the clip planes

    // find a plane that it enters
    for (i = 0; i < numplanes; ++i) {
      float into = Velocity.dot(planes[i]);
      if (into >= 0.1f) {
        // move doesn't interact with the plane
        #ifdef SLIDE_DEBUG
        printdebug("...skipped %s (into=%s)", i, into);
        #endif
        continue;
      }

      // see how hard we are hitting things
      //if (-into > impactSpeed) impactSpeed = -into;

      TVec clipVelocity = Velocity;
      clipVelocity -= planes[i]*clipVelocity.dot(planes[i]);

      #ifdef SLIDE_DEBUG
      printdebug("...vel=%s; cvel=%s", mobj.velocity, clipVelocity);
      #endif

      // see if there is a second plane that the new move enters
      for (int j = 0; j < numplanes; ++j) {
        if (j == i) continue;
        if (clipVelocity.dot(planes[j]) >= 0.1f) continue; // move doesn't interact with the plane

        // try clipping the move to the plane
        clipVelocity -= planes[j]*clipVelocity.dot(planes[j]);

        // see if it goes back into the first clip plane
        if (clipVelocity.dot(planes[i]) >= 0.0f) continue;

        #if 0
        // slide the original velocity along the crease
        TVec vdir = planes[i].cross(planes[j]).normalise;
        //TVec vdir = vector(0, 0, planes[i].cross2D(planes[j])).normalise;
        const float d = vdir.dot(Velocity);
        #ifdef SLIDE_DEBUG
        printdebug("....try:%s; i:%s; j:%s; vdir=%s; d=%s; cv=%s; ncv=%s", try, i, j, vdir, d, clipVelocity, vdir*d);
        #endif
        clipVelocity = vdir*d;
        #endif

        // see if there is a third plane the new move enters
        for (int k = 0; k < numplanes; ++k) {
          if (k == i || k == j) continue;
          if (clipVelocity.dot(planes[k]) >= 0.1f) continue; // move doesn't interact with the plane
          // stop dead at a tripple plane interaction
          Velocity.x = 0.0f;
          Velocity.y = 0.0f;
          return;
        }
      }

      // if we have fixed all interactions, try another move
      Velocity = clipVelocity;
    }

    #else
    // clamp velocity
    Velocity -= hitnormal*(Velocity.dot(hitnormal));
    // if velocity is near zero, it means that we cannot move anymore
    //if (fabs(vel.x) < STOPSPEED && fabs(vel.x) < STOPSPEED) break;
    #endif

    //if (timeleft <= 0.0) break;
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
       if (gm_use_experimental_slide.asBool()) SlideMoveNewest(StepVelScale, noPickups);
  else if (gm_use_new_slide_code.asBool()) return SlideMoveNew(StepVelScale, noPickups);
  else SlideMoveOldest(StepVelScale, noPickups);
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
