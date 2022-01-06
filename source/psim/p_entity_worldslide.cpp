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
#include "p_world.h"

#define VV_NO_DENORMAL_VELOCITIES


// ////////////////////////////////////////////////////////////////////////// //
static VCvarB dbg_slide_code("dbg_slide_code", false, "Debug slide code?", CVAR_PreInit|CVAR_Hidden|CVAR_NoShadow);
static VCvarI gm_slide_code("gm_slide_code", "0", "Which slide code to use (0:vanilla; 1:new; 2:q3-like)?", CVAR_Archive|CVAR_NoShadow);
static VCvarB gm_slide_vanilla_newsel("gm_slide_vanilla_newsel", true, "Use same velocity-based selector in vanilla sliding code?", CVAR_Archive|CVAR_Hidden|CVAR_NoShadow);
static VCvarB gm_q3slide_use_timeleft("gm_q3slide_use_timeleft", false, "Use 'timeleft' method for Q3 sliding code (gives wrong velocities and bumpiness)?", CVAR_Archive|CVAR_Hidden|CVAR_NoShadow);
static VCvarB gm_slide_vanilla_finish("gm_slide_vanilla_finish", false, "Finish stuck slide with vanilla algo?", CVAR_Archive|CVAR_Hidden|CVAR_NoShadow);



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
//  VEntity::IsBlockedSlideLine
//
//==========================================================================
int VEntity::IsBlockedSlideLine (const line_t *line) const noexcept {
  if (!line) return SlideBlockSkip; // just in case
  if (!(line->flags&ML_TWOSIDED) || !line->backsector) {
    // one-sided line
    return (line->PointOnSide(Origin) ? SlideBlockSkip : SlideBlockBlocked);
  }
  // two-sided line
  if (line->flags&(ML_BLOCKING|ML_BLOCKEVERYTHING)) return SlideBlockBlocked;
  if ((EntityFlags&EF_IsPlayer) && (line->flags&ML_BLOCKPLAYERS)) return SlideBlockBlocked;
  if ((EntityFlags&EF_CheckLineBlockMonsters) && (line->flags&ML_BLOCKMONSTERS)) return SlideBlockBlocked;
  return (IsBlockingLine(line) ? SlideBlockBlocked : SlideBlockNotBlocked);
}


//==========================================================================
//
//  VEntity::IsBlockedSlide2SLine
//
//==========================================================================
bool VEntity::IsBlockedSlide2SLine (const line_t *line, const TVec hitpoint) noexcept {
  vassert(line);
  vassert((line->flags&ML_TWOSIDED) && line->backsector);

  const float hgt = Height;
  if (hgt <= 0.0f) return false;
  const float oz = Origin.z;

  // set openrange, opentop, openbottom
  opening_t *open = XLevel->LineOpenings(line, hitpoint, SPF_NOBLOCKING, !(EntityFlags&EF_Missile)/*do3dmidtex*/); // missiles ignores 3dmidtex
  open = VLevel::FindOpening(open, oz, oz+hgt);

  float stepHeight = MaxStepHeight;
  if (EntityFlags&EF_IgnoreFloorStep) {
    stepHeight = 0.0f;
  } else {
         if ((EntityFlags&EF_CanJump) && Health > 0) stepHeight = 48.0f;
    else if ((EntityFlags&(EF_Missile|EF_StepMissile)) == EF_Missile) stepHeight = 0.0f;
  }

  if (open && open->range >= hgt && // fits
      open->top-oz >= hgt && // mobj is not too high
      open->bottom-oz <= stepHeight) // not too big for a step up
  {
    // this line doesn't block movement... but do some more checks
    if (oz >= open->bottom) return false; // not blocked
    // check to make sure there's nothing in the way for the step up
    const TVec CheckOrg(Origin.x, Origin.y, open->bottom);
    return TestMobjZ(CheckOrg);
  }

  // blocked
  return true;
}


//==========================================================================
//
//  VEntity::SlidePathTraverseOld
//
//  used only for the oldest slide code!
//
//==========================================================================
void VEntity::SlidePathTraverseOld (float &BestSlideFrac, TVec &BestSlideNormal, line_t *&BestSlideLine, float x, float y, float deltaTime) {
  #ifdef VV_NO_DENORMAL_VELOCITIES
  const TVec SlideDir = (Velocity.xy()*deltaTime).noNanInfDenormal2D();
  if (SlideDir.isZero2D()) return;
  #else
  const TVec SlideDir = Velocity.xy()*deltaTime;
  if (!SlideDir.toBool2D()) return;
  #endif
  const TVec SlideOrg(x, y, Origin.z);
  const bool usecos = gm_slide_vanilla_newsel.asBool();

  intercept_t in;
  float bestVelCos = -INFINITY;

  for (VPathTraverse It(this, &in, SlideOrg, SlideOrg+SlideDir, PT_ADDLINES|PT_NOOPENS|PT_RAILINGS); It.GetNext(); ) {
    if (!(in.Flags&intercept_t::IF_IsALine)) continue;
    if (in.frac >= 1.0f) continue; // just in case
    if (in.frac > BestSlideFrac) break; // hits are sorted by hittime, there's no reason to go further

    line_t *li = in.line;

    const int IsBlocked = IsBlockedSlideLine(li);
    if (IsBlocked == SlideBlockSkip) continue;

    /*
    if (dbg_slide_code.asBool()) {
      if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: SlidePathTraverse: best=%g; frac=%g; line #%d; flags=0x%08x; blocked=%d", GetClass()->GetName(), BestSlideFrac, in.frac, (int)(ptrdiff_t)(li-&XLevel->Lines[0]), li->flags, (int)IsBlocked);
    }
    */

    if (IsBlocked == SlideBlockNotBlocked) {
      if (!IsBlockedSlide2SLine(li, SlideOrg+in.frac*SlideDir)) continue;
    }

    const TVec hitnormal = ((li->flags&ML_TWOSIDED) && li->backsector && li->PointOnSide(Origin) ? -li->normal : li->normal);
    const TVec cvel = ClipVelocity(SlideDir, hitnormal);
    // dot product for two normalized vectors is cosine between them
    // cos(0) is 1, cos(180) is -1
    const float velcos = SlideDir.dot2D(cvel);

    if (in.frac == BestSlideFrac) {
      if (!usecos) continue;
      // our cosine is multiplied by both direction magnitudes; `SlideDir` magnitude is constant, though
      // prefer velocity that goes to the same dir
      if (velcos < 0.0f || velcos < bestVelCos) continue;
    }

    // the line blocks movement, and it is closer than best so far
    BestSlideFrac = in.frac;
    BestSlideLine = li;
    BestSlideNormal = hitnormal;
    bestVelCos = velcos;

    // hits are sorted by hittime, there's no reason to go further...
    // ...unless we're using new selector
    if (!usecos) break;
  }
}


//==========================================================================
//
//  VEntity::SlidePathTraverseNew
//
//  returns hit time -- [0..1]
//  1 means that we couldn't find any suitable wall
//  -1 means "initially stuck"
//
//==========================================================================
float VEntity::SlidePathTraverseNew (const TVec dir, TVec *BestSlideNormal, line_t **BestSlideLine, bool tryBiggerBlockmap) {
  const float rad = GetMoveRadius(); //+0.1f; // make it slightly bigger, to compensate `TryMove()` checks
  const float hgt = max2(0.0f, Height);

  /*
  float stepHeight = MaxStepHeight;
  if (EntityFlags&EF_IgnoreFloorStep) {
    stepHeight = 0.0f;
  } else {
         if ((EntityFlags&EF_CanJump) && Health > 0) stepHeight = 48.0f;
    else if ((EntityFlags&(EF_Missile|EF_StepMissile)) == EF_Missile) stepHeight = 0.0f;
  }
  */

  const TVec end(Origin+dir);

  float BestSlideFrac = 1.0f;
  float bestVelCos = -INFINITY;

  //const TVec normdir(dir.normalise2D());

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

        const int IsBlocked = IsBlockedSlideLine(li);
        if (IsBlocked == SlideBlockSkip) continue;

        VLevel::CD_HitType hitType;
        int hitplanenum = -1;
        TVec contactPoint;
        float htime = VLevel::SweepLinedefAABB(li, Origin, end, TVec(-rad, -rad, 0.0f), TVec(rad, rad, hgt), nullptr, &contactPoint, &hitType, &hitplanenum);
        if (/*htime < 0.0f ||*/ htime >= 1.0f) {
          // didn't hit
          /*
          if (debugSlide) {
            if (IsPlayer()) GLog.Logf(NAME_Debug, "  DIDNOT: frc=%g (best=%g); hpl=%d; type=%d; line #%d", htime, BestSlideFrac, hitplanenum, hitType, (int)(ptrdiff_t)(li-&XLevel->Lines[0]));
          }
          */
          continue;
        }
        if (htime > BestSlideFrac) {
          // don't care, too far away
          if (debugSlide) {
            if (IsPlayer()) GLog.Logf(NAME_Debug, "  FARAWAY: frc=%g (best=%g); hpl=%d; type=%d; line #%d", htime, BestSlideFrac, hitplanenum, hitType, (int)(ptrdiff_t)(li-&XLevel->Lines[0]));
          }
          continue;
        }
        // totally ignore beveling planes
        //if (hitplanenum > 1) continue;

        // we CAN stuck without stucking!
        if (htime < 0.0f) {
          //htime = 0.0f;
          //contactPoint = Origin;
          if (debugSlide) {
            if (IsPlayer()) GLog.Logf(NAME_Debug, "  CHECK-STUCK: line #%d", (int)(ptrdiff_t)(li-&XLevel->Lines[0]));
          }
          return -1.0f;
        }

        // if we already have a good wall, don't replace it with a bad wall
        if (hitplanenum > 1 && BestSlideFrac == htime) {
          if (debugSlide) {
            if (IsPlayer()) GLog.Logf(NAME_Debug, "  SIDEHIT! hpl=%d; type=%d; line #%d", hitplanenum, hitType, (int)(ptrdiff_t)(li-&XLevel->Lines[0]));
          }
          continue;
        }

        if (IsBlocked == SlideBlockNotBlocked) {
          if (!IsBlockedSlide2SLine(li, contactPoint)) {
            if (debugSlide) {
              if (IsPlayer()) GLog.Logf(NAME_Debug, "  2S-SKIP: frc=%g (best=%g); hpl=%d; type=%d; line #%d", htime, BestSlideFrac, hitplanenum, hitType, (int)(ptrdiff_t)(li-&XLevel->Lines[0]));
            }
            continue;
          }
        }

        const TVec hitnormal = ((li->flags&ML_TWOSIDED) && li->backsector && li->PointOnSide(Origin) ? -li->normal : li->normal);
        const TVec cvel = ClipVelocity(dir, hitnormal);
        // dot product for two normalized vectors is cosine between them
        // cos(0) is 1, cos(180) is -1
        const float velcos = dir.dot2D(cvel);
        //const float velcos = normdir.dot2D(cvel.normalise2D());

        if (htime == BestSlideFrac) {
          // our cosine is multiplied by both direction magnitudes; `SlideDir` magnitude is constant, though
          // prefer velocity that goes to the same dir
          if (velcos < 0.0f || velcos < bestVelCos) {
            if (debugSlide) {
              if (IsPlayer()) GLog.Logf(NAME_Debug, "  COSSKIP: frc=%g (best=%g); hpl=%d; type=%d; velcos=%g (best=%g), line #%d", htime, BestSlideFrac, hitplanenum, hitType, velcos, bestVelCos, (int)(ptrdiff_t)(li-&XLevel->Lines[0]));
            }
            continue;
          }
        }

        if (debugSlide) {
          if (IsPlayer()) {
            GLog.Logf(NAME_Debug, "  prevhittime=%g; newhittime=%g; newhitnormal=(%g,%g); newhitpnum=%d; newdir=(%g,%g); line #%d", BestSlideFrac, htime, hitnormal.x, hitnormal.y, hitplanenum, cvel.x, cvel.y,
              (int)(ptrdiff_t)(li-&XLevel->Lines[0]));
          }
        }

        BestSlideFrac = htime;
        if (BestSlideNormal) *BestSlideNormal = hitnormal;
        if (BestSlideLine) *BestSlideLine = li;
        //lastHitWasGood = (hitplanenum < 2);
        bestVelCos = velcos;
      }
    }
  }

  return BestSlideFrac;
}


#define SLIDEOLD_MOVE_FINALTRY()  do { \
  bool moved = (isU32NonZeroF(YMove) ? TryMove(tmtrace, TVec(Origin.x, Origin.y+YMove, Origin.z), true, noPickups) : false); \
  if (!moved && isU32NonZeroF(XMove)) moved = TryMove(tmtrace, TVec(Origin.x+XMove, Origin.y, Origin.z), true, noPickups); \
  if (!moved) Velocity.x = Velocity.y = 0.0f; \
} while (0)


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
void VEntity::SlideMoveVanilla (float deltaTime, bool noPickups) {
  tmtrace_t tmtrace;
  memset((void *)&tmtrace, 0, sizeof(tmtrace)); // valgrind: AnyBlockingLine

  float XMove = Velocity.x*deltaTime;
  float YMove = Velocity.y*deltaTime;

  const float rad = GetMoveRadius();

  // old slide
  float leadx, leady;
  float trailx, traily;

  float prevVelX = XMove;
  float prevVelY = YMove;

  int hitcount = 0;
  do {
    #ifdef VV_NO_DENORMAL_VELOCITIES
    XMove = zeroNanInfDenormalsF(XMove);
    YMove = zeroNanInfDenormalsF(YMove);
    #endif
    if (!(isU32NonZeroF(XMove)|isU32NonZeroF(YMove))) return; // just in case

    // this was `3`, but why not `4`?
    if (++hitcount == 4) {
      // don't loop forever
      SLIDEOLD_MOVE_FINALTRY();
      return;
    }

    if (isU32NonZeroF(XMove)) prevVelX = XMove;
    if (isU32NonZeroF(YMove)) prevVelY = YMove;

    // trace along the three leading corners
    if (isPositiveF(prevVelX) > 0.0f) {
      leadx = Origin.x+rad;
      trailx = Origin.x-rad;
    } else {
      leadx = Origin.x-rad;
      trailx = Origin.x+rad;
    }

    if (isPositiveF(prevVelY) > 0.0f) {
      leady = Origin.y+rad;
      traily = Origin.y-rad;
    } else {
      leady = Origin.y-rad;
      traily = Origin.y+rad;
    }

    float BestSlideFrac = 666.0f; // arbitrary big number
    line_t *BestSlideLine = nullptr;
    TVec BestSlideNormal(0.0f, 0.0f);

    SlidePathTraverseOld(BestSlideFrac, BestSlideNormal, BestSlideLine, leadx, leady, deltaTime);
    if (isU32NonZeroF(BestSlideFrac)) SlidePathTraverseOld(BestSlideFrac, BestSlideNormal, BestSlideLine, trailx, leady, deltaTime);
    if (isU32NonZeroF(BestSlideFrac)) SlidePathTraverseOld(BestSlideFrac, BestSlideNormal, BestSlideLine, leadx, traily, deltaTime);

    // move up to the wall
    if (BestSlideFrac >= 1.0f) {
      // the move must have hit the middle, so stairstep
      if (TryMove(tmtrace, TVec(Origin.x+XMove, Origin.y+YMove, Origin.z), true, noPickups)) return;
      SLIDEOLD_MOVE_FINALTRY();
      return;
    }

    // fudge a bit to make sure it doesn't hit
    const float newSlideFrac = BestSlideFrac-0.03125f;
    if (newSlideFrac > 0.0f) {
      const float newx = XMove*newSlideFrac;
      const float newy = YMove*newSlideFrac;
      if (!TryMove(tmtrace, TVec(Origin.x+newx, Origin.y+newy, Origin.z), true, noPickups)) {
        XMove = newx;
        YMove = newy;
        SLIDEOLD_MOVE_FINALTRY();
        return;
      }
    }

    // now continue along the wall
    // first calculate remainder
    BestSlideFrac = 1.0f-BestSlideFrac;

    //if (BestSlideFrac > 1.0f) BestSlideFrac = 1.0f; // just in case
    if (BestSlideFrac <= 0.0f) return; // just in case too

    // clip the moves
    Velocity *= BestSlideFrac;
    Velocity = ClipVelocity(Velocity, BestSlideNormal);
    Velocity.fix2DNanInfDenormalsInPlace(); // just in case, also resets `z`

    XMove = Velocity.x*deltaTime;
    YMove = Velocity.y*deltaTime;
  } while (!TryMove(tmtrace, TVec(Origin.x+XMove, Origin.y+YMove, Origin.z), true, noPickups));
}

#undef SLIDEOLD_MOVE_FINALTRY


#define SLIDE_MOVE_FINALTRY()  do { \
  if (gm_slide_vanilla_finish.asBool()) { \
    SlideMoveVanilla(deltaTime, noPickups); \
  } else { \
    bool moved = (isU32NonZeroF(dir.y) ? TryMove(tmtrace, TVec(Origin.x, Origin.y+dir.y, Origin.z), true, noPickups) : false); \
    if (!moved && isU32NonZeroF(dir.x)) moved = TryMove(tmtrace, TVec(Origin.x+dir.x, Origin.y, Origin.z), true, noPickups); \
    if (!moved) Velocity.x = Velocity.y = 0.0f; \
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
void VEntity::SlideMoveSweep (float deltaTime, bool noPickups) {
  tmtrace_t tmtrace;

  Velocity.z = 0.0f;

  TVec dir(Velocity*deltaTime); // we want to arrive there
  if (dir.isZero2D()) return; // just in case

  const bool debugSlide = dbg_slide_code.asBool();

  if (debugSlide) {
    if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: === SlideMove; move=(%g,%g) pos=(%g,%g,%g) ===", GetClass()->GetName(), dir.x, dir.y, Origin.x, Origin.y, Origin.z);
  }

  int hitcount = 0;
  do {
    #ifdef VV_NO_DENORMAL_VELOCITIES
    dir.fix2DNanInfDenormalsInPlace(); // just in case
    #else
    dir.z = 0.0f; // just in case
    #endif
    if (!dir.toBool2D()) return; // just in case

    if (++hitcount == 4) {
      // don't loop forever
      if (debugSlide) {
        if (IsPlayer()) GCon->Logf(NAME_Debug, " STUCK! hitcount=%d; dir=(%g,%g)", hitcount, dir.x, dir.y);
      }
      SLIDE_MOVE_FINALTRY();
      return;
    }

    if (debugSlide) {
      if (IsPlayer()) {
        const bool ok = CheckRelPosition(tmtrace, Origin, true/*noPickups*/);
        const bool mvok = TryMove(tmtrace, Origin, true, true/*noPickups*/);
        GCon->Logf(NAME_Debug, " hitcount=%d; dir=(%g,%g); checkpos=%d; mvok=%d", hitcount, dir.x, dir.y, (int)ok, (int)mvok);
      }
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
      if (TryMove(tmtrace, Origin+dir, true, noPickups)) return;
      // if we can't move, try better search
      if (debugSlide) {
        if (IsPlayer()) GCon->Logf(NAME_Debug, "    CANNOT MOVE!");
      }
      // this can happen because our sweeping function is not exactly the same as `TryMove()`
      // so swept box can hit nothing, yet `TryMove()` could find a collision
      // in this case we will make our direction longer to find that pesky wall to slide
      TVec ndir(dir);
      ndir += dir.normalise2D()*8.0f;
      BestSlideFrac = SlidePathTraverseNew(ndir, &BestSlideNormal, nullptr, true);
      if (BestSlideFrac >= 1.0f) {
        // still no wall, give up
        if (debugSlide) {
          if (IsPlayer()) GCon->Logf(NAME_Debug, "    ...and giving up.");
        }
        SLIDE_MOVE_FINALTRY();
        return;
      }
      // this slide time is not right, zero it
      BestSlideFrac = 0.0f;
    }

    // we initially stuck, wtf?!
    if (BestSlideFrac < 0.0f) {
      // try to backup a little
      if (debugSlide) {
        if (IsPlayer()) GCon->Logf(NAME_Debug, "  UNSTUCK-HACK!");
      }
      TVec ndir(dir.normalise2D());
      const TVec oldOrg = Origin;
      Origin -= ndir*2.0f;
      BestSlideFrac = SlidePathTraverseNew(dir+ndir*3.0f, &BestSlideNormal);
      Origin = oldOrg;
      if (BestSlideFrac < 0.0f) {
        // still stuck, wtf?!
        if (debugSlide) {
          if (IsPlayer()) GCon->Logf(NAME_Debug, "  STUCK-STUCK!");
        }
        SLIDE_MOVE_FINALTRY();
        return;
      }
      // this slide time is not right, zero it
      BestSlideFrac = 0.0f;
    }

    if (BestSlideFrac == 0.0f) {
      TVec cdir = ClipVelocity(dir, BestSlideNormal);
      if (debugSlide) {
        if (IsPlayer()) GLog.Logf(NAME_Debug, "  TIME0:0: newdir=(%g,%g); dir=(%g,%g); norm=(%g,%g)", cdir.x, cdir.y, dir.x, dir.y, BestSlideNormal.x, BestSlideNormal.y);
      }
      bool mvok = TryMove(tmtrace, Origin+cdir, true, noPickups);
      if (debugSlide) {
        if (IsPlayer()) GLog.Logf(NAME_Debug, "  TIME0:1: mvok=%d", (int)mvok);
      }
      if (!mvok) {
        if (isU32NonZeroF(dir.x)) mvok = TryMove(tmtrace, TVec(Origin.x+dir.x, Origin.y, Origin.z), true, noPickups);
        if (!mvok && isU32NonZeroF(dir.y)) mvok = TryMove(tmtrace, TVec(Origin.x, Origin.y+dir.y, Origin.z), true, noPickups);
      }
    }

    // just in case, try full move too
    if (BestSlideFrac > 0.0f && !TryMove(tmtrace, Origin+dir, true, noPickups)) {
      // fudge a bit to make sure it doesn't hit
      const float newSlideFrac = BestSlideFrac-0.03125f;
      if (newSlideFrac > 0.0f) {
        const TVec smdir(dir*newSlideFrac);
        if (!TryMove(tmtrace, Origin+smdir, true, noPickups)) {
          if (debugSlide) {
            if (IsPlayer()) GLog.Logf(NAME_Debug, "  CAN'T SLIDE; newdir=(%g,%g)", smdir.x, smdir.y);
          }
          dir = smdir;
          SLIDE_MOVE_FINALTRY();
          return;
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
    Velocity *= BestSlideFrac;
    Velocity = ClipVelocity(Velocity, BestSlideNormal);
    Velocity.fix2DNanInfDenormalsInPlace(); // just in case, also resets `z`

    dir = Velocity*deltaTime; // `dir.z` is zero

    if (debugSlide) {
      if (IsPlayer()) GLog.Logf(NAME_Debug, "  newvel=(%g,%g); newdir=(%g,%g)", Velocity.x, Velocity.y, dir.x, dir.y);
    }
  } while (!TryMove(tmtrace, Origin+dir, true, noPickups));
}


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
void VEntity::SlideMoveQ3Like (float deltaTime, bool noPickups) {
  tmtrace_t tmtrace;

  const bool debugSlide = dbg_slide_code.asBool();

  if (debugSlide) {
    if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: === SlideMove; move=(%g,%g) pos=(%g,%g,%g) ===", GetClass()->GetName(), Velocity.x*deltaTime, Velocity.y*deltaTime, Origin.x, Origin.y, Origin.z);
  }

  Velocity.z = 0.0f;

  TVec planes[/*MAX_CLIP_PLANES*/5];
  int numplanes = 0;

  const bool useTimeLeft = dbg_slide_code.asBool();

  int trynum = 0;
  for (;;) {
    TVec dir(Velocity);
    dir *= deltaTime; // we want to arrive here
    #ifdef VV_NO_DENORMAL_VELOCITIES
    dir.fix2DNanInfDenormalsInPlace(); // just in case
    #else
    dir.z = 0.0f; // just in case
    #endif
    if (!dir.toBool2D()) return; // just in case

    // just in case
    if (TryMove(tmtrace, Origin+dir, true, noPickups)) return;

    if (++trynum == 4) {
      // don't loop forever
      if (debugSlide) {
        if (IsPlayer()) GCon->Logf(NAME_Debug, " STUCK! hitcount=%d; dir=(%g,%g)", trynum, dir.x, dir.y);
      }
      SLIDE_MOVE_FINALTRY();
      return;
    }

    if (debugSlide) {
      if (IsPlayer()) GCon->Logf(NAME_Debug, " trynum=%d; dir=(%g,%g)", trynum, dir.x, dir.y);
    }

    // find line to slide
    TVec BestSlideNormal(0.0f, 0.0f, 0.0f);
    float BestSlideFrac = SlidePathTraverseNew(dir, &BestSlideNormal);

    if (BestSlideFrac >= 1.0f) {
      if (debugSlide) {
        if (IsPlayer()) GCon->Logf(NAME_Debug, "  CANNOT MOVE! BestSlideFrac=%g", BestSlideFrac);
      }
      //if (TryMove(tmtrace, Origin+dir, true, noPickups)) return; // it is done above
      // if we can't move, try better search
      //if (debugSlide) {
      //  if (IsPlayer()) GCon->Logf(NAME_Debug, "    CANNOT MOVE!");
      //}
      // this can happen because our sweeping function is not exactly the same as `TryMove()`
      // so swept box can hit nothing, yet `TryMove()` could find a collision
      // in this case we will make our direction longer to find that pesky wall to slide
      TVec ndir(dir);
      ndir += dir.normalise2D()*8.0f;
      BestSlideFrac = SlidePathTraverseNew(ndir, &BestSlideNormal, nullptr, true);
      if (BestSlideFrac >= 1.0f) {
        // still no wall, give up
        if (debugSlide) {
          if (IsPlayer()) GCon->Logf(NAME_Debug, "    ...and giving up.");
        }
        SLIDE_MOVE_FINALTRY();
        return;
      }
      // this slide time is not right, zero it
      BestSlideFrac = 0.0f;
    }

    // we initially stuck, wtf?!
    if (BestSlideFrac < 0.0f) {
      // try to backup a little
      if (debugSlide) {
        if (IsPlayer()) GCon->Logf(NAME_Debug, "  UNSTUCK-HACK!");
      }
      TVec ndir(dir.normalise2D());
      const TVec oldOrg = Origin;
      Origin -= ndir*2.0f;
      BestSlideFrac = SlidePathTraverseNew(dir+ndir*3.0f, &BestSlideNormal);
      Origin = oldOrg;
      if (BestSlideFrac < 0.0f) {
        // still stuck, wtf?!
        if (debugSlide) {
          if (IsPlayer()) GCon->Logf(NAME_Debug, "  STUCK-STUCK!");
        }
        SLIDE_MOVE_FINALTRY();
        return;
      }
      // this slide time is not right, zero it
      BestSlideFrac = 0.0f;
    }

    // no need to try a full move here, it is done above
    if (BestSlideFrac > 0.0f) {
      // make hittime slightly smaller, to avoid walls stucking
      const float newSlideFrac = BestSlideFrac-0.03125f;
      if (newSlideFrac > 0.0f) {
        const TVec smdir(dir*newSlideFrac);
        if (!TryMove(tmtrace, Origin+smdir, true, noPickups)) {
          if (debugSlide) {
            if (IsPlayer()) GLog.Logf(NAME_Debug, "  CAN'T SLIDE AT ALL!");
          }
          dir = smdir;
          SLIDE_MOVE_FINALTRY();
          return;
        }
      }
    }

    // fix timeleft
    if (useTimeLeft) {
      deltaTime -= deltaTime*BestSlideFrac;
      if (debugSlide) {
        if (IsPlayer()) GLog.Logf(NAME_Debug, "  timeleft=%g; vel=(%g,%g)", deltaTime, Velocity.x, Velocity.y);
      }
      if (deltaTime <= 0.0f) return; // fully moved (this cannot happen, but...)
    } else {
      BestSlideFrac = 1.0f-BestSlideFrac;
      if (debugSlide) {
        if (IsPlayer()) GLog.Logf(NAME_Debug, "  fracleft=%g; vel=(%g,%g)", BestSlideFrac, Velocity.x, Velocity.y);
      }
      if (BestSlideFrac <= 0.0f) return; // fully moved (this cannot happen, but...)

      // clip the moves
      Velocity *= BestSlideFrac;
    }

    // if this is the same plane we hit before, nudge velocity out along it,
    // which fixes some epsilon issues with non-axial planes
    {
      bool found = false;
      for (int i = 0; i < numplanes; ++i) {
        if (BestSlideNormal.dot(planes[i]) > 0.99f) {
          found = true;
          Velocity += BestSlideNormal;
          break;
        }
      }
      if (found) continue; // already seen it
      planes[numplanes++] = BestSlideNormal;
    }

    // modify velocity so it parallels all of the clip planes

    // find a plane that it enters
    for (int i = 0; i < numplanes; ++i) {
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

#undef SLIDE_MOVE_FINALTRY


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
void VEntity::SlideMove (float deltaTime, bool noPickups) {
  const float oldvelz = Velocity.z;
  const int slideType = gm_slide_code.asInt();
       if (slideType >= 2) SlideMoveQ3Like(deltaTime, noPickups);
  else if (slideType == 1) SlideMoveSweep(deltaTime, noPickups);
  else SlideMoveVanilla(deltaTime, noPickups);
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
        hp -= mdelta.normalise()*2.0f;
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
    TVec dir = (newPos-org).normalise(); // to newpos
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
  float deltaTime;
  VOptParamBool noPickups(false);
  vobjGetParamSelf(deltaTime, noPickups);
  Self->SlideMove(deltaTime, noPickups);
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
