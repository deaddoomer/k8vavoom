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
//  VEntity::ClipVelocity
//
//  Slide off of the impacting object
//
//==========================================================================
TVec VEntity::ClipVelocity (const TVec &in, const TVec &normal, float overbounce) {
  return in-normal*(DotProduct(in, normal)*overbounce);
}


//==========================================================================
//
//  VEntity::SlidePathTraverse
//
//==========================================================================
void VEntity::SlidePathTraverse (float &BestSlideFrac, line_t *&BestSlideLine, float x, float y, float StepVelScale) {
  TVec SlideOrg(x, y, Origin.z);
  TVec SlideDir = Velocity*StepVelScale;
  if (gm_use_new_slide_code.asBool()) {
    const float rad = GetMoveRadius();
    const bool slideDiag = (SlideDir.x && SlideDir.y);
    bool prevClipToZero = false;
    // new slide code
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
          }

          VLevel::CD_HitType hitType;
          int hitplanenum = -1;
          TVec contactPoint;
          const float fdist = VLevel::SweepLinedefAABB(li, SlideOrg, SlideOrg+SlideDir, TVec(-rad, -rad, 0), TVec(rad, rad, Height), nullptr, &contactPoint, &hitType, &hitplanenum);
          // ignore cap planes
          if (hitplanenum < 0 || hitplanenum > 1 || fdist < 0.0f || fdist >= 1.0f) continue;

          if (!IsBlocked) {
            const TVec hpoint = contactPoint; //SlideOrg+fdist*SlideDir;
            // set openrange, opentop, openbottom
            opening_t *open = XLevel->LineOpenings(li, hpoint, SPF_NOBLOCKING, true); //!(EntityFlags&EF_Missile)); // missiles ignores 3dmidtex
            open = VLevel::FindOpening(open, Origin.z, Origin.z+Height);
            if (open && open->range >= Height && // fits
                open->top-Origin.z >= Height && // mobj is not too high
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

          if (fdist < BestSlideFrac) {
            #ifdef VV_DBG_VERBOSE_SLIDE
            if (IsPlayer()) {
              const TVec cvel = ClipVelocity(SlideDir, li->normal, 1.0f);
              GCon->Logf(NAME_Debug, "%s: SlidePathTraverse: NEW line #%d; oldfrac=%g; newfrac=%g; norm=(%g,%g); sldir=(%g,%g); cvel=(%g,%g); ht=%d", GetClass()->GetName(), (int)(ptrdiff_t)(li-&XLevel->Lines[0]), BestSlideFrac, fdist, li->normal.x, li->normal.y, SlideDir.x, SlideDir.y, cvel.x, cvel.y, (int)hitType);
            }
            #endif
            BestSlideFrac = fdist;
            BestSlideLine = li;
            if (slideDiag) {
              const TVec cvel = ClipVelocity(SlideDir, li->normal, 1.0f);
              prevClipToZero = !(cvel.x && cvel.y);
            }
          } else if (prevClipToZero && slideDiag && fdist == BestSlideFrac) {
            // choose line that doesn't clip one of the sliding dirs to zero
            const TVec cvel = ClipVelocity(SlideDir, li->normal, 1.0f);
            if (cvel.x && cvel.y) {
              #ifdef VV_DBG_VERBOSE_SLIDE
              if (IsPlayer()) {
                GCon->Logf(NAME_Debug, "%s: SlidePathTraverse: NEW NON-ZERO line #%d; oldfrac=%g; newfrac=%g; norm=(%g,%g); sldir=(%g,%g); cvel=(%g,%g); ht=%d", GetClass()->GetName(), (int)(ptrdiff_t)(li-&XLevel->Lines[0]), BestSlideFrac, fdist, li->normal.x, li->normal.y, SlideDir.x, SlideDir.y, cvel.x, cvel.y, (int)hitType);
              }
              #endif
              BestSlideFrac = fdist;
              BestSlideLine = li;
              prevClipToZero = false;
            }
            #ifdef VV_DBG_VERBOSE_SLIDE
            else {
              if (IsPlayer()) {
                GCon->Logf(NAME_Debug, "%s: SlidePathTraverse: SKIP ZERO line #%d; oldfrac=%g; newfrac=%g; norm=(%g,%g); sldir=(%g,%g); cvel=(%g,%g); ht=%d", GetClass()->GetName(), (int)(ptrdiff_t)(li-&XLevel->Lines[0]), BestSlideFrac, fdist, li->normal.x, li->normal.y, SlideDir.x, SlideDir.y, cvel.x, cvel.y, (int)hitType);
              }
            }
            #endif
          }
          #ifdef VV_DBG_VERBOSE_SLIDE
          else {
            if (IsPlayer()) {
              const TVec cvel = ClipVelocity(SlideDir, li->normal, 1.0f);
              GCon->Logf(NAME_Debug, "%s: SlidePathTraverse: SKIP line #%d; oldfrac=%g; newfrac=%g; norm=(%g,%g); sldir=(%g,%g); cvel=(%g,%g); ht=%d", GetClass()->GetName(), (int)(ptrdiff_t)(li-&XLevel->Lines[0]), BestSlideFrac, fdist, li->normal.x, li->normal.y, SlideDir.x, SlideDir.y, cvel.x, cvel.y, (int)hitType);
            }
          }
          #endif
        }
      }
    }
  } else {
    // old slide code
    intercept_t in;
    for (VPathTraverse It(this, &in, SlideOrg, SlideOrg+TVec(SlideDir.x, SlideDir.y, 0.0f), PT_ADDLINES|PT_NOOPENS); It.GetNext(); ) {
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
        opening_t *open = XLevel->LineOpenings(li, hpoint, SPF_NOBLOCKING, true); //!(EntityFlags&EF_Missile)); // missiles ignores 3dmidtex
        open = VLevel::FindOpening(open, Origin.z, Origin.z+Height);

        if (open && open->range >= Height && // fits
            open->top-Origin.z >= Height && // mobj is not too high
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
    if (XLevel->TraceLine(ltr, org, end, 0/*SPF_NOBLOCKSIGHT*/)) return end; // no hit
    // hit something, move slightly forward
    TVec mdelta = ltr.LineEnd-org;
    //const float wantdist = velo.length();
    const float movedist = mdelta.length();
    if (movedist > 2.0f) {
      //GCon->Logf("*** hit! (%g,%g,%g)", ltr.HitPlaneNormal.x, ltr.HitPlaneNormal.y, ltr.HitPlaneNormal.z);
      if (ltr.HitPlaneNormal.z) {
        // floor
        //GCon->Logf("floor hit! (%g,%g,%g)", ltr.HitPlaneNormal.x, ltr.HitPlaneNormal.y, ltr.HitPlaneNormal.z);
        ltr.LineEnd += ltr.HitPlaneNormal*2;
      } else {
        ltr.LineEnd -= mdelta.normalised()*2;
      }
    }
    return ltr.LineEnd;
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
