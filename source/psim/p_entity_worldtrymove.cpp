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
#include "p_levelinfo.h"
#include "p_player.h"

//#define VV_DBG_VERBOSE_TRYMOVE


#ifdef VV_DBG_VERBOSE_TRYMOVE
# define TMDbgF(...)  GCon->Logf(NAME_Debug, __VA_ARGS__)
#else
# define TMDbgF(...)  do {} while (0)
#endif



//==========================================================================
//
//  VEntity::PushLine
//
//==========================================================================
void VEntity::PushLine (const tmtrace_t &tmtrace, bool checkOnly) {
  if (checkOnly || GGameInfo->NetMode == NM_Client) return;
  if (EntityFlags&EF_ColideWithWorld) {
    if (EntityFlags&EF_Blasted) eventBlastedHitLine();
    int NumSpecHitTemp = tmtrace.SpecHit.length();
    while (NumSpecHitTemp > 0) {
      --NumSpecHitTemp;
      // see if the line was crossed
      line_t *ld = tmtrace.SpecHit[NumSpecHitTemp];
      int side = ld->PointOnSide(Origin);
      eventCheckForPushSpecial(ld, side);
    }
  }
}



//**************************************************************************
//
//  MOVEMENT CLIPPING
//
//**************************************************************************

//==========================================================================
//
//  VEntity::TryMove
//
//  attempt to move to a new position, crossing special lines.
//  returns `false` if move is blocked.
//
//==========================================================================
bool VEntity::TryMove (tmtrace_t &tmtrace, TVec newPos, bool AllowDropOff, bool checkOnly, bool noPickups) {
  bool check;
  TVec oldorg(0, 0, 0);
  line_t *ld;
  sector_t *OldSec = Sector;

  if (IsGoingToDie() || !Sector) {
    memset((void *)&tmtrace, 0, sizeof(tmtrace));
    return false; // just in case, dead object is immovable
  }

  const bool isClient = (GGameInfo->NetMode == NM_Client);

  bool skipEffects = (checkOnly || noPickups);

  TMDbgF("%s: *** trying to move from (%g,%g,%g) to (%g,%g,%g); Height=%g; Radius=%g", GetClass()->GetName(), Origin.x, Origin.y, Origin.z, newPos.x, newPos.y, newPos.z, Height, GetMoveRadius());
  check = CheckRelPosition(tmtrace, newPos, skipEffects);
  tmtrace.TraceFlags &= ~tmtrace_t::TF_FloatOk;
  //if (IsPlayer()) GCon->Logf(NAME_Debug, "trying to move from (%g,%g,%g) to (%g,%g,%g); check=%d", Origin.x, Origin.y, Origin.z, newPos.x, newPos.y, newPos.z, (int)check);
  TMDbgF("%s: trying to move from (%g,%g,%g) to (%g,%g,%g); check=%d; tmt.FloorZ=%g; tmt.DropOffZ=%g; tmt.CeilingZ=%g", GetClass()->GetName(), Origin.x, Origin.y, Origin.z, newPos.x, newPos.y, newPos.z, (int)check, tmtrace.FloorZ, tmtrace.DropOffZ, tmtrace.CeilingZ);

  if (isClient) skipEffects = true;

  if (!check) {
    // cannot fit into destination point
    VEntity *O = tmtrace.BlockingMobj;
    TMDbgF("%s:   HIT: entity=%s, line #%d", GetClass()->GetName(), (O ? O->GetClass()->GetName() : "<none>"), (tmtrace.BlockingLine ? (int)(ptrdiff_t)(tmtrace.BlockingLine-&XLevel->Lines[0]) : -1));
    if (!O) {
      // can't step up or doesn't fit
      PushLine(tmtrace, skipEffects);
      return false;
    }

    const float ohgt = GetBlockingHeightFor(O);
    if (!(EntityFlags&EF_IsPlayer) ||
        (O->EntityFlags&EF_IsPlayer) ||
        O->Origin.z+ohgt-Origin.z > MaxStepHeight ||
        O->CeilingZ-(O->Origin.z+ohgt) < Height ||
        tmtrace.CeilingZ-(O->Origin.z+ohgt) < Height)
    {
      // can't step up or doesn't fit
      PushLine(tmtrace, skipEffects);
      return false;
    }

    if (!(EntityFlags&EF_PassMobj) || Level->GetNoPassOver()) {
      // can't go over
      return false;
    }
  }

  // check passed

  if (EntityFlags&EF_ColideWithWorld) {
    if (tmtrace.CeilingZ-tmtrace.FloorZ < Height) {
      // doesn't fit
      PushLine(tmtrace, skipEffects);
      TMDbgF("%s:   DOESN'T FIT(0)!", GetClass()->GetName());
      if (!tmtrace.BlockingLine && tmtrace.FloorLine) tmtrace.BlockingLine = tmtrace.FloorLine;
      return false;
    }

    tmtrace.TraceFlags |= tmtrace_t::TF_FloatOk;

    if (tmtrace.CeilingZ-Origin.z < Height && !(EntityFlags&EF_Fly) && !(EntityFlags&EF_IgnoreCeilingStep)) {
      // mobj must lower itself to fit
      PushLine(tmtrace, skipEffects);
      TMDbgF("%s:   DOESN'T FIT(1)! ZBox=(%g,%g); ceilz=%g", GetClass()->GetName(), Origin.z, Origin.z+Height, tmtrace.CeilingZ);
      if (!tmtrace.BlockingLine && tmtrace.CeilingLine) tmtrace.BlockingLine = tmtrace.CeilingLine;
      return false;
    }

    if (EntityFlags&EF_Fly) {
      // when flying, slide up or down blocking lines until the actor is not blocked
      if (Origin.z+Height > tmtrace.CeilingZ) {
        // if sliding down, make sure we don't have another object below
        if (!checkOnly && !isClient) {
          if (/*(!tmtrace.BlockingMobj || !tmtrace.BlockingMobj->CheckOnmobj() ||
               (tmtrace.BlockingMobj->CheckOnmobj() && tmtrace.BlockingMobj->CheckOnmobj() != this)) &&
              (!CheckOnmobj() || (CheckOnmobj() && CheckOnmobj() != tmtrace.BlockingMobj))*/
              CheckBlockingMobj(tmtrace.BlockingMobj))
          {
            Velocity.z = -8.0f*35.0f;
          }
        }
        PushLine(tmtrace, skipEffects);
        return false;
      } else if (Origin.z < tmtrace.FloorZ && tmtrace.FloorZ-tmtrace.DropOffZ > MaxStepHeight) {
        // check to make sure there's nothing in the way for the step up
        if (!checkOnly && !isClient) {
          if (/*(!tmtrace.BlockingMobj || !tmtrace.BlockingMobj->CheckOnmobj() ||
               (tmtrace.BlockingMobj->CheckOnmobj() && tmtrace.BlockingMobj->CheckOnmobj() != this)) &&
              (!CheckOnmobj() || (CheckOnmobj() && CheckOnmobj() != tmtrace.BlockingMobj))*/
              CheckBlockingMobj(tmtrace.BlockingMobj))
          {
            Velocity.z = 8.0f*35.0f;
          }
        }
        PushLine(tmtrace, skipEffects);
        return false;
      }
    }

    if (!(EntityFlags&EF_IgnoreFloorStep)) {
      if (tmtrace.FloorZ-Origin.z > MaxStepHeight) {
        // too big a step up
        if ((EntityFlags&EF_CanJump) && Health > 0) {
          // check to make sure there's nothing in the way for the step up
          if (!Velocity.z || tmtrace.FloorZ-Origin.z > 48.0f ||
              (tmtrace.BlockingMobj && tmtrace.BlockingMobj->CheckOnmobj()) ||
              TestMobjZ(TVec(newPos.x, newPos.y, tmtrace.FloorZ)))
          {
            PushLine(tmtrace, skipEffects);
            TMDbgF("%s:   FLOORSTEP(0)!", GetClass()->GetName());
            return false;
          }
        } else {
          PushLine(tmtrace, skipEffects);
          TMDbgF("%s:   FLOORSTEP(1)!", GetClass()->GetName());
          return false;
        }
      }

      if ((EntityFlags&EF_Missile) && !(EntityFlags&EF_StepMissile) && tmtrace.FloorZ > Origin.z) {
        PushLine(tmtrace, skipEffects);
        TMDbgF("%s:   FLOORX(0)! z=%g; fl=%g; spechits=%d; Origin.z=%g to %g; FloorZ=%g", GetClass()->GetName(), Origin.z, tmtrace.FloorZ, tmtrace.SpecHit.length(), Origin.z, Origin.z+Height, tmtrace.FloorZ);
        return false;
      }

      if (Origin.z < tmtrace.FloorZ) {
        if (EntityFlags&EF_StepMissile) {
          Origin.z = tmtrace.FloorZ;
          // if moving down, cancel vertical component of velocity
          if (Velocity.z < 0) Velocity.z = 0.0f;
        }
        // check to make sure there's nothing in the way for the step up
        if (TestMobjZ(TVec(newPos.x, newPos.y, tmtrace.FloorZ))) {
          PushLine(tmtrace, skipEffects);
          TMDbgF("%s:   OBJZ(0)!", GetClass()->GetName());
          return false;
        }
      }
    }

    // killough 3/15/98: Allow certain objects to drop off
    if ((!AllowDropOff && !(EntityFlags&EF_DropOff) &&
        !(EntityFlags&EF_Float) && !(EntityFlags&EF_Missile)) ||
        (EntityFlags&EF_NoDropOff))
    {
      if (!(EntityFlags&EF_AvoidingDropoff)) {
        float floorz = tmtrace.FloorZ;
        // [RH] If the thing is standing on something, use its current z as the floorz.
        // this is so that it does not walk off of things onto a drop off.
        if (EntityFlags&EF_OnMobj) floorz = max2(Origin.z, tmtrace.FloorZ);

        if ((floorz-tmtrace.DropOffZ > MaxDropoffHeight) && !(EntityFlags&EF_Blasted)) {
          // can't move over a dropoff unless it's been blasted
          TMDbgF("%s:   DROPOFF(0)!", GetClass()->GetName());
          return false;
        }
      } else {
        // special logic to move a monster off a dropoff
        // this intentionally does not check for standing on things
        // k8: ...and due to this, the monster can stuck into another monster. what a great idea!
        if (FloorZ-tmtrace.FloorZ > MaxDropoffHeight || DropOffZ-tmtrace.DropOffZ > MaxDropoffHeight) {
          TMDbgF("%s:   DROPOFF(1)!", GetClass()->GetName());
          return false;
        }
        // check to make sure there's nothing in the way
        if (!CheckBlockingMobj(tmtrace.BlockingMobj)) {
          return false;
        }
      }
    }

    if ((EntityFlags&EF_CantLeaveFloorpic) &&
        (tmtrace.EFloor.splane->pic != EFloor.splane->pic || tmtrace.FloorZ != Origin.z))
    {
      // must stay within a sector of a certain floor type
      TMDbgF("%s:   SECTORSTAY(0)!", GetClass()->GetName());
      return false;
    }
  } // (EntityFlags&EF_ColideWithWorld)

  bool OldAboveFakeFloor = false;
  bool OldAboveFakeCeiling = false;
  if (Sector->heightsec) {
    float EyeZ = (Player ? Player->ViewOrg.z : Origin.z+Height*0.5f);
    OldAboveFakeFloor = (EyeZ > Sector->heightsec->floor.GetPointZClamped(Origin));
    OldAboveFakeCeiling = (EyeZ > Sector->heightsec->ceiling.GetPointZClamped(Origin));
  }

  if (checkOnly) return true;

  TMDbgF("%s:   ***OK-TO-MOVE", GetClass()->GetName());

  // the move is ok, so link the thing into its new position
  UnlinkFromWorld();

  oldorg = Origin;
  Origin = newPos;

  LinkToWorld();

  FloorZ = tmtrace.FloorZ;
  CeilingZ = tmtrace.CeilingZ;
  DropOffZ = tmtrace.DropOffZ;
  CopyTraceFloor(&tmtrace, false);
  CopyTraceCeiling(&tmtrace, false);

  if (EntityFlags&EF_FloorClip) {
    eventHandleFloorclip();
  } else {
    FloorClip = 0.0f;
  }

  if (!noPickups && !isClient) {
    // if any special lines were hit, do the effect
    if (EntityFlags&EF_ColideWithWorld) {
      while (tmtrace.SpecHit.length() > 0) {
        // see if the line was crossed
        ld = tmtrace.SpecHit[tmtrace.SpecHit.length()-1];
        tmtrace.SpecHit.SetNum(tmtrace.SpecHit.length()-1, false);
        // some moron once placed the entity *EXACTLY* on the fuckin' linedef! what a brilliant idea!
        // this will *NEVER* be fixed, it is a genuine map bug (it's impossible to fix it with our freestep engine anyway)
        // let's try use "front inclusive" here; it won't solve all cases, but *may* solve the one above
        const int oldside = ld->PointOnSideFri(oldorg);
        const int side = ld->PointOnSideFri(Origin);
        if (side != oldside) {
          if (ld->special) eventCrossSpecialLine(ld, oldside);
        }
      }
    }

    // do additional check here to avoid calling progs
    if ((OldSec->heightsec && Sector->heightsec && Sector->ActionList) ||
        (OldSec != Sector && (OldSec->ActionList || Sector->ActionList)))
    {
      eventCheckForSectorActions(OldSec, OldAboveFakeFloor, OldAboveFakeCeiling);
    }
  }

  return true;
}



//==========================================================================
//
//  Script natives
//
//==========================================================================
IMPLEMENT_FUNCTION(VEntity, TryMove) {
  tmtrace_t tmtrace;
  TVec Pos;
  bool AllowDropOff;
  VOptParamBool checkOnly(false);
  vobjGetParamSelf(Pos, AllowDropOff, checkOnly);
  RET_BOOL(Self->TryMove(tmtrace, Pos, AllowDropOff, checkOnly));
}

IMPLEMENT_FUNCTION(VEntity, TryMoveEx) {
  tmtrace_t tmp;
  tmtrace_t *tmtrace = nullptr;
  TVec Pos;
  bool AllowDropOff;
  VOptParamBool checkOnly(false);
  vobjGetParamSelf(tmtrace, Pos, AllowDropOff, checkOnly);
  if (!tmtrace) tmtrace = &tmp;
  RET_BOOL(Self->TryMove(*tmtrace, Pos, AllowDropOff, checkOnly));
}
