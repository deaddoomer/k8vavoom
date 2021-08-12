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


// ////////////////////////////////////////////////////////////////////////// //
static VCvarB gm_use_new_slide_code("gm_use_new_slide_code", true, "Use new sliding code (experimental)?", CVAR_Archive);



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
//  VEntity::ClipVelocity
//
//  Slide off of the impacting object
//
//==========================================================================
TVec VEntity::ClipVelocity (const TVec &in, const TVec &normal, float overbounce) {
  return in-normal*(in.dot(normal)*overbounce);
}


//==========================================================================
//
//  VEntity::SlidePathTraverse
//
//==========================================================================
void VEntity::SlidePathTraverse (float &BestSlideFrac, line_t *&BestSlideLine, float x, float y, float StepVelScale) {
  TVec SlideOrg(x, y, Origin.z);
  TVec SlideDir = Velocity*StepVelScale;
  const float hgt = max2(0.0f, Height);
  if (gm_use_new_slide_code.asBool()) {
    // new slide code
    const float rad = GetMoveRadius();
    float stepHeight = MaxStepHeight;
    if (EntityFlags&EF_IgnoreFloorStep) {
      stepHeight = 0.0f;
    } else {
           if ((EntityFlags&EF_CanJump) && Health > 0) stepHeight = 48.0f;
      else if ((EntityFlags&(EF_Missile|EF_StepMissile)) == EF_Missile) stepHeight = 0.0f;
    }
    const bool slideDiag = (SlideDir.x && SlideDir.y);
    bool prevClipToZero = false;
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
          const float htime = VLevel::SweepLinedefAABB(li, SlideOrg, SlideOrg+SlideDir, TVec(-rad, -rad, 0), TVec(rad, rad, hgt), nullptr, &contactPoint, &hitType, &hitplanenum);
          #if 0
            if (IsPlayer()) {
              GCon->Logf(NAME_Debug, "line=%d; org=(%f,%f); dir=(%f,%f); bestfrac=%f; htime=%f; pnum=%d",
                (int)(ptrdiff_t)(li-&XLevel->Lines[0]), SlideOrg.x, SlideOrg.y, SlideDir.x, SlideDir.y,
                BestSlideFrac, htime, hitplanenum);
            }
          #endif
          if (htime < 0.0f || htime >= 1.0f) continue; // didn't hit
          if (htime > BestSlideFrac) continue; // don't care, too far away
          #if 0
          if (htime <= 0.001f && hitplanenum > 1) continue; // ignore beveling planes for "stuck hits"
          #else
          // totally ignore beveling planes
          if (hitplanenum > 1) continue;
          #endif

          // prefer walls with non-zero clipped velocity
          if (prevClipToZero && slideDiag && htime == BestSlideFrac) {
            const TVec cvel = ClipVelocity(SlideDir, li->normal, 1.0f);
            if (cvel.isZero2D()) continue;
            // try this hitpoint, maybe it will not clip the velocity to zero
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
                TVec CheckOrg = Origin;
                CheckOrg.z = open->bottom;
                if (!TestMobjZ(CheckOrg)) continue; // not blocked
              } else {
                continue; // not blocked
              }
            }
          }

          BestSlideFrac = htime;
          BestSlideLine = li;
          if (slideDiag) prevClipToZero = !ClipVelocity(SlideDir, li->normal, 1.0f).isZero2D();
        }
      }
    }
  } else {
    // old slide code
    intercept_t in;
    for (VPathTraverse It(this, &in, SlideOrg, SlideOrg+TVec(SlideDir.x, SlideDir.y, 0.0f), PT_ADDLINES|PT_NOOPENS|PT_RAILINGS); It.GetNext(); ) {
      if (!(in.Flags&intercept_t::IF_IsALine)) { continue; /*Host_Error("PTR_SlideTraverse: not a line?");*/ }

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
  float leadx, leady;
  float trailx, traily;
  int hitcount = 0;
  tmtrace_t tmtrace;
  memset((void *)&tmtrace, 0, sizeof(tmtrace)); // valgrind: AnyBlockingLine

  float XMove = Velocity.x*StepVelScale;
  float YMove = Velocity.y*StepVelScale;
  if (XMove == 0.0f && YMove == 0.0f) return; // just in case

  #ifdef VV_DBG_VERBOSE_SLIDE
  if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: === SlideMove; move=(%g,%g) pos=(%g,%g,%g) ===", GetClass()->GetName(), XMove, YMove, Origin.x, Origin.y, Origin.z);
  #endif

  const float rad = GetMoveRadius();

  float prevVelX = XMove;
  float prevVelY = YMove;

  do {
    if (++hitcount == 3) {
      // don't loop forever
      const bool movedY = (YMove != 0.0f ? TryMove(tmtrace, TVec(Origin.x, Origin.y+YMove, Origin.z), true) : false);
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

    float BestSlideFrac = 1.00001f;
    line_t *BestSlideLine = nullptr;

    #ifdef VV_DBG_VERBOSE_SLIDE
    if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: SlideMove: hitcount=%d; fracleft=%g; move=(%g,%g) (%g,%g) (%d:%d)", GetClass()->GetName(), hitcount, BestSlideFrac, XMove, YMove, prevVelX, prevVelY, (XMove == 0.0f), (YMove == 0.0f));
    #endif

    if (gm_use_new_slide_code.asBool()) {
      SlidePathTraverse(BestSlideFrac, BestSlideLine, Origin.x, Origin.y, StepVelScale);
      #ifdef VV_DBG_VERBOSE_SLIDE
      if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: SlideMove: hitcount=%d; frac=%g; line #%d; pos=(%g,%g)", GetClass()->GetName(), hitcount, BestSlideFrac, (int)(ptrdiff_t)(BestSlideLine-&XLevel->Lines[0]), leadx, leady);
      #endif
    } else {
      // old slide code
      SlidePathTraverse(BestSlideFrac, BestSlideLine, leadx, leady, StepVelScale);
      #ifdef VV_DBG_VERBOSE_SLIDE
      if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: SlideMove: hitcount=%d; frac=%g; line #%d; pos=(%g,%g)", GetClass()->GetName(), hitcount, BestSlideFrac, (int)(ptrdiff_t)(BestSlideLine-&XLevel->Lines[0]), leadx, leady);
      #endif
      if (BestSlideFrac != 0.0f) SlidePathTraverse(BestSlideFrac, BestSlideLine, trailx, leady, StepVelScale);
      #ifdef VV_DBG_VERBOSE_SLIDE
      if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: SlideMove: hitcount=%d; frac=%g; line #%d; pos=(%g,%g)", GetClass()->GetName(), hitcount, BestSlideFrac, (int)(ptrdiff_t)(BestSlideLine-&XLevel->Lines[0]), trailx, leady);
      #endif
      if (BestSlideFrac != 0.0f) SlidePathTraverse(BestSlideFrac, BestSlideLine, leadx, traily, StepVelScale);
      #ifdef VV_DBG_VERBOSE_SLIDE
      if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: SlideMove: hitcount=%d; frac=%g; line #%d; pos=(%g,%g)", GetClass()->GetName(), hitcount, BestSlideFrac, (int)(ptrdiff_t)(BestSlideLine-&XLevel->Lines[0]), leadx, traily);
      #endif
    }

    // move up to the wall
    if (BestSlideFrac == 1.00001f) {
      // the move must have hit the middle, so stairstep
      const bool movedY = (YMove != 0.0f ? TryMove(tmtrace, TVec(Origin.x, Origin.y+YMove, Origin.z), true) : false);
      #ifdef VV_DBG_VERBOSE_SLIDE
      bool movedX = false;
      if (!movedY) movedX = (XMove != 0.0f ? TryMove(tmtrace, TVec(Origin.x+XMove, Origin.y, Origin.z), true) : false);
      if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: SlideMove: hitcount=%d; stairstep! (no line found!); movedx=%d; movedy=%d", GetClass()->GetName(), hitcount, (int)movedX, (int)movedY);
      #else
      if (!movedY) TryMove(tmtrace, TVec(Origin.x+XMove, Origin.y, Origin.z), true);
      #endif
      return;
    }

    #ifdef VV_DBG_VERBOSE_SLIDE
    if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: SlideMove: hitcount=%d; frac=%g; line #%d", GetClass()->GetName(), hitcount, BestSlideFrac, (int)(ptrdiff_t)(BestSlideLine-&XLevel->Lines[0]));
    #endif

    // fudge a bit to make sure it doesn't hit
    const float origSlide = BestSlideFrac; // we'll need it later
    BestSlideFrac -= 0.03125f;
    if (BestSlideFrac > 0.0f) {
      const float newx = XMove*BestSlideFrac;
      const float newy = YMove*BestSlideFrac;

      if (!TryMove(tmtrace, TVec(Origin.x+newx, Origin.y+newy, Origin.z), true)) {
        const bool movedY = (YMove != 0.0f ? TryMove(tmtrace, TVec(Origin.x, Origin.y+YMove, Origin.z), true) : false);
        if (!movedY) TryMove(tmtrace, TVec(Origin.x+XMove, Origin.y, Origin.z), true);
        return;
      }
    }

    // now continue along the wall
    // first calculate remainder
    BestSlideFrac = 1.0f-origSlide;
    #ifdef VV_DBG_VERBOSE_SLIDE
    if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: SlideMove: hitcount=%d; fracleft=%g; vel=(%g,%g)", GetClass()->GetName(), hitcount, BestSlideFrac, Velocity.x, Velocity.y);
    #endif

    if (BestSlideFrac > 1.0f) BestSlideFrac = 1.0f;
    if (BestSlideFrac <= 0.0f) return;

    // clip the moves
    // k8: don't multiply z, 'cause it makes jumping against a wall unpredictably hard
    Velocity.x *= BestSlideFrac;
    Velocity.y *= BestSlideFrac;
    #ifdef VV_DBG_VERBOSE_SLIDE
    if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: SlideMove: hitcount=%d;   velclip0=(%g,%g)", GetClass()->GetName(), hitcount, Velocity.x, Velocity.y);
    #endif
    Velocity = ClipVelocity(Velocity, BestSlideLine->normal, 1.0f);
    #ifdef VV_DBG_VERBOSE_SLIDE
    if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: SlideMove: hitcount=%d;   velclip1=(%g,%g)", GetClass()->GetName(), hitcount, Velocity.x, Velocity.y);
    #endif
    //Velocity = ClipVelocity(Velocity*BestSlideFrac, BestSlideLine->normal, 1.0f);
    XMove = Velocity.x*StepVelScale;
    YMove = Velocity.y*StepVelScale;
    #ifdef VV_DBG_VERBOSE_SLIDE
    if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: SlideMove: hitcount=%d;   step=(%g,%g) : %g", GetClass()->GetName(), hitcount, XMove, YMove, StepVelScale);
    #endif
  } while (!TryMove(tmtrace, TVec(Origin.x+XMove, Origin.y+YMove, Origin.z), true));
}


//==========================================================================
//
//  VEntity::SlideMove
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
