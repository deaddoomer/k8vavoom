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
//**  LineOfSight/Visibility checks, uses REJECT Lookup Table.
//**
//**  This uses specialized forms of the maputils routines for optimized
//**  performance
//**
//**************************************************************************
#include "../gamedefs.h"
#include "p_entity.h"


static VCvarB compat_better_sight("compat_better_sight", true, "Check more points in LOS calculations?", CVAR_Archive);
static VCvarB dbg_disable_cansee("dbg_disable_cansee", false, "Disable CanSee processing (for debug)?", CVAR_PreInit|CVAR_NoShadow);

//k8: for some reason, sight checks ignores base sector region
//    i don't think that this is a right thing to do, so i removed that


//==========================================================================
//
//  VEntity::CanSeeEx
//
//  LineOfSight/Visibility checks, uses REJECT Lookup Table. This uses
//  specialised forms of the maputils routines for optimized performance
//  Returns true if a straight line between t1 and t2 is unobstructed.
//
//==========================================================================
bool VEntity::CanSee (VEntity *Other, bool forShooting, bool alwaysBetter) {
  return CanSeeEx(Other, (forShooting ? CSE_ForShooting : CSE_NothingZero)|(alwaysBetter ? CSE_AlwaysBetter : CSE_NothingZero)|CSE_CheckBaseRegion);
}


//==========================================================================
//
//  VEntity::CanSeeEx
//
//  LineOfSight/Visibility checks, uses REJECT Lookup Table. This uses
//  specialised forms of the maputils routines for optimized performance
//  Returns true if a straight line between t1 and t2 is unobstructed.
//
//==========================================================================
bool VEntity::CanSeeEx (VEntity *Other, unsigned flags) {
  if (dbg_disable_cansee) return false;

  if (Other == this) return true; // it can see itself (obviously)

  // if we have no base sector for any object, it cannot see each other
  if (!Other || !Other->BaseSector || !BaseSector) return false;
  if (IsGoingToDie() || Other->IsGoingToDie()) return false;

  // first check for trivial rejection
  if (XLevel->IsRejectedVis(BaseSector, Other->BaseSector)) return false; // can't possibly be connected

  // killough 11/98: shortcut for melee situations
  // same subsector? obviously visible
  // this is not true for base sectors, though
  if (SubSector == Other->SubSector) {
    // if we have some 3d pobjs or 3d floors at this subsector, do not early exit
    if (!SubSector->Has3DPObjs() && !SubSector->sector->HasAnyExtraFloors()) return true;
    // check for the same polyobject
    if (SubSector->isInnerPObj()) {
      // two entities can have some 3d pobj subsector only if they're on or inside a pobj
      //const float pz1 = SubSector->sector->ownpobj->poceiling.maxz;
      //if (Origin.z == pz1 || Other.Origin.z == pz1) return true;
      return true;
    }
  }

  bool forShooting = !!(flags&CSE_ForShooting);
  bool alwaysBetter = !!(flags&CSE_AlwaysBetter);

  if (alwaysBetter) forShooting = false;

  bool cbs = (!forShooting && (alwaysBetter || compat_better_sight));

  if (cbs && !alwaysBetter) {
    // turn off "better sight" if it is not forced, and neither entity is monster
    //cbs = (IsPlayerOrMonster() && Other->IsPlayerOrMonster());
    cbs = (IsMonster() || Other->IsMonster());
    //if (!cbs) GCon->Logf(NAME_Debug, "%s: better sight forced to 'OFF', checking sight to '%s' (not a player, not a monster)", GetClass()->GetName(), Other->GetClass()->GetName());
  }

  // if too far, don't do "better sight" (it doesn't worth it anyway)
  if (cbs) {
    const float distSq = (Origin-Other->Origin).length2DSquared();
    cbs = (distSq < 680.0*680.0); // arbitrary number
    //if (!cbs) GCon->Logf(NAME_Debug, "%s: better sight forced to 'OFF', checking sight to '%s' (dist=%g)", GetClass()->GetName(), Other->GetClass()->GetName(), sqrtf(distSq));
  }

  TVec dirF, dirR;
  if (cbs) {
    //dirR = YawVectorRight(Angles.yaw);
    TVec dirU;
    TAVec ang;
    ang.yaw = Angles.yaw;
    ang.pitch = 0.0f;
    ang.roll = 0.0f;
    AngleVectors(ang, dirF, dirR, dirU);
  } else {
    dirF = dirR = TVec::ZeroVector;
  }
  //if (forShooting) dirR = TVec::ZeroVector; // just in case, lol
  return XLevel->CastCanSee(BaseSubSector, Origin, Height, dirF, dirR, Other->Origin, Other->GetMoveRadius(), Other->Height,
                            !(flags&CSE_CheckBaseRegion)/*skip base region*/, Other->BaseSubSector, /*alwaysBetter*/cbs,
                            !!(flags&CSE_IgnoreBlockAll), !!(flags&CSE_IgnoreFakeFloors));
}



//==========================================================================
//
//  Script natives
//
//==========================================================================
IMPLEMENT_FUNCTION(VEntity, CanSee) {
  VEntity *Other;
  VOptParamBool disableBetterSight(false);
  vobjGetParamSelf(Other, disableBetterSight);
  //if (!Self) { VObject::VMDumpCallStack(); Sys_Error("empty `self`!"); }
  RET_BOOL(Self->CanSee(Other, disableBetterSight));
}

IMPLEMENT_FUNCTION(VEntity, CanSeeAdv) {
  VEntity *Other;
  vobjGetParamSelf(Other);
  //if (!Self) { VObject::VMDumpCallStack(); Sys_Error("empty `self`!"); }
  RET_BOOL(Self->CanSee(Other, false, true));
}

IMPLEMENT_FUNCTION(VEntity, CanShoot) {
  VEntity *Other;
  vobjGetParamSelf(Other);
  //if (!Self) { VObject::VMDumpCallStack(); Sys_Error("empty `self`!"); }
  RET_BOOL(Self->CanShoot(Other));
}
