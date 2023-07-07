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
//**  Copyright (C) 2018-2023 Ketmar Dark
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
#include "../host.h"  /* host_frametime */
#include "p_entity.h"
#include "p_levelinfo.h"
#include "p_world.h"
#include "p_player.h"


// ////////////////////////////////////////////////////////////////////////// //
// used in `UpdateVelocity()`
//WARNING! keep this in sync with VC code! (not anymore)
// roughly smaller than lowest fixed point 16.16 (it is more like 0.0000152587890625)
#define SMALLEST_NONZERO_VEL  (0.000016f*35.0f)


// ////////////////////////////////////////////////////////////////////////// //
// this may create stuck monsters on Arch-Vile revival and such, but meh...
// i REALLY HATE hanging corpses
static VCvarB gm_smaller_corpses("gm_smaller_corpses", false, "Make corpses smaller, so they will not hang on ledges?", CVAR_Archive|CVAR_NoShadow);
static VCvarF gm_corpse_radius_mult("gm_corpse_radius_mult", "0.4", "Corpse radius multiplier for 'smaller corpese' mode.", CVAR_Archive|CVAR_NoShadow);
#ifdef CLIENT
VCvarB r_interpolate_thing_movement("r_interpolate_thing_movement", true, "Interpolate mobj movement?", CVAR_Archive|CVAR_NoShadow);
VCvarB r_interpolate_thing_angles_models("r_interpolate_thing_angles_models", true, "Interpolate mobj rotation for 3D models?", CVAR_Archive|CVAR_NoShadow);
VCvarB r_interpolate_thing_angles_models_monsters("r_interpolate_thing_angles_models_monsters", false, "Interpolate mobj rotation for monster 3D models?", CVAR_Archive|CVAR_NoShadow);
VCvarB r_interpolate_thing_angles_sprites("r_interpolate_thing_angles_sprites", false, "Interpolate mobj rotation for sprites?", CVAR_Archive|CVAR_NoShadow);
#endif


// ////////////////////////////////////////////////////////////////////////// //
// searches though the surrounding mapblocks for monsters/players
// distance is in MAPBLOCKUNITS
class VRoughBlockSearchIterator : public VScriptIterator {
private:
  VEntity *Self;
  int Distance;
  VEntity *Ent;
  VEntity **EntPtr;

  int StartX;
  int StartY;
  int Count;
  int CurrentEdge;
  int BlockIndex;
  int FirstStop;
  int SecondStop;
  int ThirdStop;
  int FinalStop;

public:
  VRoughBlockSearchIterator (VEntity *, int, VEntity **);
  virtual bool GetNext () override;
};


//==========================================================================
//
//  VRoughBlockSearchIterator
//
//==========================================================================
VRoughBlockSearchIterator::VRoughBlockSearchIterator (VEntity *ASelf, int ADistance, VEntity **AEntPtr)
  : Self(ASelf)
  , Distance(ADistance)
  , Ent(nullptr)
  , EntPtr(AEntPtr)
  , Count(1)
  , CurrentEdge(-1)
{
  StartX = MapBlock(Self->Origin.x-Self->XLevel->BlockMapOrgX);
  StartY = MapBlock(Self->Origin.y-Self->XLevel->BlockMapOrgY);

  // start with current block
  if (StartX >= 0 && StartX < Self->XLevel->BlockMapWidth &&
      StartY >= 0 && StartY < Self->XLevel->BlockMapHeight)
  {
    Ent = Self->XLevel->BlockLinks[StartY*Self->XLevel->BlockMapWidth+StartX];
  }
}


//==========================================================================
//
//  VRoughBlockSearchIterator::GetNext
//
//==========================================================================
bool VRoughBlockSearchIterator::GetNext () {
  int BlockX, BlockY;

  for (;;) {
    while (Ent && Ent->IsGoingToDie()) Ent = Ent->BlockMapNext;

    if (Ent) {
      *EntPtr = Ent;
      Ent = Ent->BlockMapNext;
      return true;
    }

    switch (CurrentEdge) {
      case 0: // trace the first block section (along the top)
        if (BlockIndex <= FirstStop) {
          Ent = Self->XLevel->BlockLinks[BlockIndex];
          ++BlockIndex;
        } else {
          CurrentEdge = 1;
          --BlockIndex;
        }
        break;
      case 1: // trace the second block section (right edge)
        if (BlockIndex <= SecondStop) {
          Ent = Self->XLevel->BlockLinks[BlockIndex];
          BlockIndex += Self->XLevel->BlockMapWidth;
        } else {
          CurrentEdge = 2;
          BlockIndex -= Self->XLevel->BlockMapWidth;
        }
        break;
      case 2: // trace the third block section (bottom edge)
        if (BlockIndex >= ThirdStop) {
          Ent = Self->XLevel->BlockLinks[BlockIndex];
          --BlockIndex;
        } else {
          CurrentEdge = 3;
          ++BlockIndex;
        }
        break;
      case 3: // trace the final block section (left edge)
        if (BlockIndex > FinalStop) {
          Ent = Self->XLevel->BlockLinks[BlockIndex];
          BlockIndex -= Self->XLevel->BlockMapWidth;
        } else {
          CurrentEdge = -1;
        }
        break;
      default:
        if (Count > Distance) return false; // we are done
        BlockX = StartX-Count;
        BlockY = StartY-Count;

        if (BlockY < 0) {
          BlockY = 0;
        } else if (BlockY >= Self->XLevel->BlockMapHeight) {
          BlockY = Self->XLevel->BlockMapHeight-1;
        }
        if (BlockX < 0) {
          BlockX = 0;
        } else if (BlockX >= Self->XLevel->BlockMapWidth) {
          BlockX = Self->XLevel->BlockMapWidth-1;
        }
        BlockIndex = BlockY*Self->XLevel->BlockMapWidth+BlockX;
        FirstStop = StartX+Count;
        if (FirstStop < 0) { ++Count; break; }
        if (FirstStop >= Self->XLevel->BlockMapWidth) FirstStop = Self->XLevel->BlockMapWidth-1;
        SecondStop = StartY+Count;
        if (SecondStop < 0) { ++Count; break; }
        if (SecondStop >= Self->XLevel->BlockMapHeight) SecondStop = Self->XLevel->BlockMapHeight-1;
        ThirdStop = SecondStop*Self->XLevel->BlockMapWidth+BlockX;
        SecondStop = SecondStop*Self->XLevel->BlockMapWidth+FirstStop;
        FirstStop += BlockY*Self->XLevel->BlockMapWidth;
        FinalStop = BlockIndex;
        ++Count;
        CurrentEdge = 0;
        break;
    }
  }

  return false;
}



//=============================================================================
//
//  VEntity::Destroy
//
//=============================================================================
void VEntity::Destroy () {
  UnlinkFromWorld();
  if (XLevel) XLevel->DelSectorList();
  if (TID && Level) RemoveFromTIDList(); // remove from TID list
  TID = 0; // just in case
  Super::Destroy();
}


//==========================================================================
//
//  VEntity::GetDrawDelta
//
//==========================================================================
TVec VEntity::GetDrawDelta () {
#ifdef CLIENT
  // movement interpolation
  if (r_interpolate_thing_movement && (MoveFlags&MVF_JustMoved)) {
    const float ctt = XLevel->Time-LastMoveTime;
    //GCon->Logf(NAME_Debug, "%s:%u: ctt=%g; delta=(%g,%g,%g)", GetClass()->GetName(), GetUniqueId(), ctt, (Origin-LastMoveOrigin).x, (Origin-LastMoveOrigin).y, (Origin-LastMoveOrigin).z);
    if (ctt >= 0.0f && ctt < LastMoveDuration && LastMoveDuration > 0.0f) {
      TVec delta = Origin-LastMoveOrigin;
      if (!delta.isZero()) {
        delta *= ctt/LastMoveDuration;
        //GCon->Logf(NAME_Debug, "%s:%u:   ...realdelta=(%g,%g,%g)", GetClass()->GetName(), GetUniqueId(), delta.x, delta.y, delta.z);
        return (LastMoveOrigin+delta)-Origin;
      } else {
        // reset if angles are equal
        if (LastMoveAngles.yaw == Angles.yaw) MoveFlags &= ~VEntity::MVF_JustMoved;
      }
    } else {
      // unconditional reset
      MoveFlags &= ~VEntity::MVF_JustMoved;
    }
  }
#endif
  return TVec(0.0f, 0.0f, 0.0f);
}


//==========================================================================
//
//  VEntity::GetInterpolatedDrawAngles
//
//==========================================================================
TAVec VEntity::GetInterpolatedDrawAngles () {
#ifdef CLIENT
  // movement interpolation
  if (MoveFlags&MVF_JustMoved) {
    const float ctt = XLevel->Time-LastMoveTime;
    if (ctt >= 0.0f && ctt < LastMoveDuration && LastMoveDuration > 0.0f) {
      float deltaYaw = AngleDiff(AngleMod(LastMoveAngles.yaw), AngleMod(Angles.yaw));
      if (deltaYaw) {
        deltaYaw *= ctt/LastMoveDuration;
        return TAVec(Angles.pitch, AngleMod(LastMoveAngles.yaw+deltaYaw), Angles.roll);
      } else {
        // reset if origins are equal
        if ((Origin-LastMoveOrigin).isZero()) MoveFlags &= ~VEntity::MVF_JustMoved;
      }
    } else {
      // unconditional reset
      MoveFlags &= ~VEntity::MVF_JustMoved;
    }
  }
#endif
  return Angles;
}


//==========================================================================
//
//  VEntity::GetDrawOrigin
//
//==========================================================================
TVec VEntity::GetDrawOrigin () {
  TVec sprorigin = Origin+GetDrawDelta();
  sprorigin.z -= FloorClip;
  // do floating bob here, so the dlight will not move
#ifdef CLIENT
  // perform bobbing
  if (FlagsEx&EFEX_FloatBob) {
    //float FloatBobPhase; // in seconds; <0 means "choose at random"; should be converted to ticks; amp is [0..63]
    //float FloatBobStrength;
    if (FloatBobPhase < 0) FloatBobPhase = FRandom()*256.0f/35.0f; // just in case
    const float amp = FloatBobStrength*8.0f;
    const float phase = fmodf(FloatBobPhase*35.0, 64.0f);
    const float angle = phase*360.0f/64.0f;
    #if 0
    float zofs = msin(angle)*amp;
    if (Sector) {
           if (Origin.z+zofs < FloorZ && Origin.z >= FloorZ) zofs = Origin.z-FloorZ;
      else if (Origin.z+zofs+Height > CeilingZ && Origin.z+Height <= CeilingZ) zofs = CeilingZ-Height-Origin.z;
    }
    res.z += zofs;
    #else
    sprorigin.z += msin(angle)*amp;
    #endif
  }
#endif
  return sprorigin;
}


//==========================================================================
//
//  VEntity::GetMoveRadius
//
//  corpses could have smaller radius
//
//  TODO: return real radius in nightmare?
//
//==========================================================================
float VEntity::GetMoveRadius () const noexcept {
  if ((EntityFlags&(EF_Corpse|EF_Missile|EF_Invisible|EF_ActLikeBridge)) == EF_Corpse && (FlagsEx&EFEX_Monster)) {
    // monster corpse
    if (gm_smaller_corpses.asBool()) {
      const float rmult = gm_corpse_radius_mult.asFloat();
      if (isFiniteF(rmult) && rmult > 0.0f && rmult < 1.0f) return Radius*rmult;
    }
  } else if (EntityFlags&EF_IsPlayer) {
    if (Radius > 0.0f && Player && Player->MO == this) {
      return Radius+0.2f; // so the player won't fit into the exact-sized passages
    }
  }
  return max2(0.0f, Radius);
}


//==========================================================================
//
//  VEntity::Copy3DPObjFloorCeiling
//
//  used to fix floor and ceiling info with polyobject
//
//  returns `false` if stuck inside
//
//==========================================================================
bool VEntity::Copy3DPObjFloorCeiling (polyobj_t *po, TSecPlaneRef &EFloor, float &FloorZ, float &DropOffZ,
                                      TSecPlaneRef &ECeiling, float &CeilingZ, polyobj_t *&PolyObj,
                                      const float z0, const float z1)
{
  const float pz0 = po->pofloor.minz;
  const float pz1 = po->poceiling.maxz;

  if (pz0 >= pz1) return true; // paper-thin or invalid polyobject

  bool fixFloor = false, fixCeiling = false, res = true;

  // check relative position
  if (z0 == pz1) {
    // standing on, fix floor
    fixFloor = true;
    if (!PolyObj || PolyObj->tag > po->tag) PolyObj = po;
  } else if (z1 > pz1) {
    // head is above, fix floor
    fixFloor = true;
    // fix current pobj if there is an intersection
    if (z0 < pz1 && (!PolyObj || (PolyObj->tag > po->tag && PolyObj->poceiling.maxz != z0))) PolyObj = po;
  } else if (z0 < pz0) {
    // feet are below, fix ceiling
    fixCeiling = true;
  } else {
    // fully inside, still fix ceiling
    // arbitrary height: for very small objects, pull 'em up
    //if (z1-z0 <= 2.0f) fixCeiling = true; else fixFloor = true;
    // if feet are below half of pobj height, fix ceiling, else fix floor
    if (z0 < pz0+(pz1-pz0)*0.5f) fixCeiling = true; else fixFloor = true;
    res = false;
    if (!PolyObj || (PolyObj->tag > po->tag && PolyObj->poceiling.maxz != z0)) PolyObj = po;
  }

  if (fixFloor && FloorZ <= pz1) {
    if (FloorZ < pz1) DropOffZ = FloorZ; // fix dropoff
    FloorZ = pz1;
    EFloor.set(&po->poceiling, false);
  }
  if (fixCeiling && CeilingZ >= pz0) {
    CeilingZ = pz0;
    ECeiling.set(&po->pofloor, false);
  }

  return res;
}


//==========================================================================
//
//  VEntity::CopyTraceFloor
//
//==========================================================================
void VEntity::CopyTraceFloor (tmtrace_t *tr, bool setz) {
  EFloor = tr->EFloor;
  if (setz) FloorZ = tr->FloorZ;
}


//==========================================================================
//
//  VEntity::CopyTraceCeiling
//
//==========================================================================
void VEntity::CopyTraceCeiling (tmtrace_t *tr, bool setz) {
  ECeiling = tr->ECeiling;
  if (setz) CeilingZ = tr->CeilingZ;
}


//==========================================================================
//
//  VEntity::CopyRegFloor
//
//==========================================================================
void VEntity::CopyRegFloor (sec_region_t *r) {
  EFloor = r->efloor;
  FloorZ = EFloor.GetPointZClamped(Origin);
}


//==========================================================================
//
//  VEntity::CopyRegCeiling
//
//==========================================================================
void VEntity::CopyRegCeiling (sec_region_t *r) {
  ECeiling = r->eceiling;
  CeilingZ = ECeiling.GetPointZClamped(Origin);
}


//==========================================================================
//
//  VEntity::IsInPolyObj
//
//==========================================================================
bool VEntity::IsInPolyObj (polyobj_t *po) {
  if (!po) return false;
  if (!po->Is3D()) return false;
  if (po->pofloor.minz >= po->poceiling.maxz) return false;
  float tmbbox[4];
  const float rad = GetMoveRadius();
  Create2DBBox(tmbbox, Origin, rad);
  if (!Are2DBBoxesOverlap(po->bbox2d, tmbbox)) return false;
  if (!XLevel->IsBBox2DTouchingSector(po->GetSector(), tmbbox)) return false;
  const float pz0 = po->pofloor.minz;
  const float pz1 = po->poceiling.maxz;
  const float z0 = Origin.z;
  const float z1 = z0+max2(0.0f, Height);
  return
    (z0 > pz0 && z0 < pz1) ||
    (z0 <= pz0 && z1 >= pz0);
}


//==========================================================================
//
//  VEntity::IsBlockingLine
//
//==========================================================================
bool VEntity::IsBlockingLine (const line_t *ld) const noexcept {
  if (ld) {
    const unsigned lflags = ld->flags;
    // one-sided line is always blocking
    if (lflags&ML_TWOSIDED) {
      if (lflags&ML_RAILING) return false;
      if (lflags&ML_BLOCKEVERYTHING) return true; // explicitly blocking everything
      const unsigned eflags = EntityFlags;
      if ((eflags&EF_Missile) && (lflags&GetMissileLineBlockFlag())) return true; // blocks projectile or hitscan-projectile?
      if ((eflags&EF_CheckLineBlocking) && (lflags&ML_BLOCKING)) return true; // explicitly blocking everything
      if ((eflags&EF_CheckLineBlockMonsters) && (lflags&ML_BLOCKMONSTERS)) return true; // block monsters only
      if ((eflags&EF_IsPlayer) && (lflags&ML_BLOCKPLAYERS)) return true; // block players only
      if ((eflags&EF_Float) && (lflags&ML_BLOCK_FLOATERS)) return true; // block floaters only
    } else {
      return true;
    }
  }
  return false;
}


//==========================================================================
//
//  VEntity::IsBlocking3DPobjLineTop
//
//==========================================================================
bool VEntity::IsBlocking3DPobjLineTop (const line_t *ld) const noexcept {
  if (ld) {
    const unsigned lflags = ld->flags;
    // one-sided line is always blocking
    if (lflags&ML_TWOSIDED) {
      if (lflags&ML_BLOCKEVERYTHING) return true; // explicitly blocking everything
      if (IsPlayer() && (lflags&ML_BLOCKPLAYERS)) return true;
      if (IsMonster() && (lflags&ML_BLOCKMONSTERS)) return true;
      if (IsAnyMissile() && (ld->flags&GetMissileLineBlockFlag())) return true;
      if ((EntityFlags&EF_Float) && (lflags&ML_BLOCK_FLOATERS)) return true; // block floaters only
    }
  }
  return false;
}


//==========================================================================
//
//  VEntity::BlockedByLine
//
//==========================================================================
void VEntity::BlockedByLine (line_t *ld) {
  if (EntityFlags&EF_Blasted) eventBlastedHitLine();
  if (ld->special) eventCheckForPushSpecial(ld, 0);
}


//==========================================================================
//
//  VEntity::CheckBlockingMobj
//
//  returns `false` if blocked
//
//==========================================================================
bool VEntity::CheckBlockingMobj (VEntity *blockmobj) {
  VEntity *bmee = (blockmobj ? blockmobj->CheckOnmobj() : nullptr);
  VEntity *myee = CheckOnmobj();
  return
    (!blockmobj || !bmee || bmee != this) &&
    (!myee || myee != blockmobj);
}


//=============================================================================
//
//  VEntity::UpdateVelocity
//
//  called from entity `Physics()`
//
//=============================================================================
void VEntity::UpdateVelocity (float DeltaTime, bool allowSlopeFriction) {
  // clamp minimum velocity (because why not?)
  if (fabsf(Velocity.x) < SMALLEST_NONZERO_VEL) Velocity.x = 0.0f;
  if (fabsf(Velocity.y) < SMALLEST_NONZERO_VEL) Velocity.y = 0.0f;

  if (!Sector || (EntityFlags&EF_NoSector)) return; // it is still called for each entity

  /*
  if (Origin.z <= FloorZ && !Velocity && !bCountKill && !bIsPlayer) {
    // no gravity for non-moving things on ground to prevent static objects from sliding on slopes
    return;
  }
  */

  const float fnormz = EFloor.GetNormalZ();

  // apply slope friction
  if (allowSlopeFriction && Origin.z <= FloorZ && fnormz != 1.0f && (EntityFlags&(EF_Fly|EF_Missile)) == 0 &&
      fabsf(Sector->floor.maxz-Sector->floor.minz) > MaxStepHeight)
  {
    if (IsPlayer() && Player && Player->IsNoclipActive()) {
      // do nothing
    } else if (true /*fnormz <= 0.7f*/) {
      TVec Vel = EFloor.GetNormal();
      const float dot = Velocity.dot(Vel);
      if (dot < 0.0f) {
        /*
        if (IsPlayer()) {
          GCon->Logf(NAME_Debug, "%s: slopeZ=%g; dot=%g; Velocity=(%g,%g,%g); Vel=(%g,%g,%g); Vel*dot=(%g,%g,%g)",
            GetClass()->GetName(), fnormz, dot, Velocity.x, Velocity.y, Velocity.z, Vel.x, Vel.y, Vel.z, Vel.x*dot, Vel.y*dot, Vel.z*dot);
        }
        */
        //TVec Vel = dot*EFloor.spGetNormal();
        //if (bIsPlayer) printdebug("%C: Velocity=%s; Vel=%s; dot=%s; Vel*dot=%s (%s); dt=%s", self, Velocity.xy, Vel, dot, Vel.xy*dot, Vel.xy*(dot*DeltaTime), DeltaTime);
        //Vel *= dot*35.0f*DeltaTime;
        if (fnormz <= 0.7f) {
          Vel *= dot*1.75f; // k8: i pulled this out of my ass
        } else {
          Vel *= dot*DeltaTime;
        }
        Vel.z = 0;
        //print("mht=%s; hgt=%s", MaxStepHeight, Sector.floor.maxz-Sector.floor.minz);
        /*
        print("vv: Vel=%s; dot=%s; norm=%s", Vel, dot, EFloor.spGetNormal());
        print("  : z=%s; fminz=%s; fmaxz=%s", Origin.z, Sector.floor.minz, Sector.floor.maxz);
        print("  : velocity: %s -- %s", Velocity, Velocity-Vel.xy);
        */
        Velocity -= Vel;
      }
    }
  }

  if (EntityFlags&EF_NoGravity) return;

  const float dz = Origin.z-FloorZ;

  // don't add gravity if standing on a slope with normal.z > 0.7 (aprox 45 degrees)
  if (dz > 0.6f || fnormz <= 0.7f) {
    //if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: *** dfz=%g; normz=%g", GetClass()->GetName(), dz, EFloor.GetNormalZ());
    if (WaterLevel < 2) {
      Velocity.z -= Gravity*Level->Gravity*Sector->Gravity*DeltaTime;
    } else if (!IsPlayer() || Health <= 0) {
      // water gravity
      Velocity.z -= Gravity*Level->Gravity*Sector->Gravity/10.0f*DeltaTime;
      float startvelz = Velocity.z;
      float sinkspeed = -WaterSinkSpeed/(IsRealCorpse() ? 3.0f : 1.0f);
      if (Velocity.z < sinkspeed) {
        Velocity.z = (startvelz < sinkspeed ? startvelz : sinkspeed);
      } else {
        Velocity.z = startvelz+(Velocity.z-startvelz)*WaterSinkFactor;
      }
    } else if (dz > 0.0f && Velocity.z == 0.0f) {
      // snap to the floor
      Velocity.z = dz;
      //if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: *** SNAP! dfz=%g; normz=%g", GetClass()->GetName(), dz, EFloor.GetNormalZ());
    }
  }
}



//**************************************************************************
//
//  TEST ON MOBJ
//
//**************************************************************************

//=============================================================================
//
//  TestMobjZ
//
//  Checks if the new Z position is legal
//  returns blocking thing
//
//=============================================================================
bool VEntity::TestMobjZ (const TVec &TryOrg, VEntity **hitent) {
  if (hitent) *hitent = nullptr;

  const float rad = GetMoveRadius();

  // can't hit things, or not solid?
  if ((EntityFlags&(EF_ColideWithThings|EF_Solid)) == (EF_ColideWithThings|EF_Solid)) {
    // the bounding box is extended by MAXRADIUS because mobj_ts are grouped
    // into mapblocks based on their origin point, and can overlap into adjacent
    // blocks by up to MAXRADIUS units
    DeclareMakeBlockMapCoordsMaxRadius(TryOrg.x, TryOrg.y, rad, xl, yl, xh, yh);
    // xl->xh, yl->yh determine the mapblock set to search
    for (int bx = xl; bx <= xh; ++bx) {
      for (int by = yl; by <= yh; ++by) {
        for (auto &&it : XLevel->allBlockThings(bx, by)) {
          VEntity *Other = it.entity();
          if (Other == this) continue; // don't clip against self
          //if (OwnerSUId && Other->ServerUId == OwnerSUId) continue;
          //k8: can't hit corpse
          if ((Other->EntityFlags&(EF_ColideWithThings|EF_Solid|EF_Corpse)) != (EF_ColideWithThings|EF_Solid)) continue; // can't hit things, or not solid
          const float ohgt = GetBlockingHeightFor(Other);
          if (TryOrg.z > Other->Origin.z+ohgt) continue; // over thing
          if (TryOrg.z+Height < Other->Origin.z) continue; // under thing
          const float blockdist = Other->GetMoveRadius()+rad;
          if (fabsf(Other->Origin.x-TryOrg.x) >= blockdist ||
              fabsf(Other->Origin.y-TryOrg.y) >= blockdist)
          {
            // didn't hit thing
            continue;
          }
          if (hitent) *hitent = Other;
          return true;
        }
      }
    }
  }

  //FIXME: this should return some "fake entity", because some callers rely on in
  if (XLevel->Has3DPolyObjects() && (EntityFlags&EF_ColideWithWorld)) {
    float bbox2d[4];
    bbox2d[BOX2D_TOP] = TryOrg.y+rad;
    bbox2d[BOX2D_BOTTOM] = TryOrg.y-rad;
    bbox2d[BOX2D_RIGHT] = TryOrg.x+rad;
    bbox2d[BOX2D_LEFT] = TryOrg.x-rad;
    DeclareMakeBlockMapCoordsBBox2D(bbox2d, xl, yl, xh, yh);
    // need new validcount
    XLevel->IncrementValidCount();
    const float z1 = TryOrg.z+max2(0.0f, Height);
    for (int bx = xl; bx <= xh; ++bx) {
      for (int by = yl; by <= yh; ++by) {
        polyobj_t *po;
        for (VBlockPObjIterator It(XLevel, bx, by, &po); It.GetNext(); ) {
          //GCon->Logf(NAME_Debug, "x00: %s(%u): checking pobj #%d... (%g:%g) (%g:%g)", GetClass()->GetName(), GetUniqueId(), po->tag, po->pofloor.minz, po->poceiling.maxz, TryOrg.z, z1);
          if (po->pofloor.minz < po->poceiling.maxz && (z1 <= po->pofloor.minz || TryOrg.z >= po->poceiling.maxz)) {
            // outside of polyobject
            continue;
          }
          if (!Are2DBBoxesOverlap(po->bbox2d, bbox2d)) continue;
          //GCon->Logf(NAME_Debug, "x01: %s(%u): checking pobj #%d...", GetClass()->GetName(), GetUniqueId(), po->tag);
          if (!XLevel->IsBBox2DTouchingSector(po->GetSector(), bbox2d)) continue;
          //if (!XLevel->IsPointInsideSector2D(po->GetSector(), TryOrg.x, TryOrg.y)) continue;
          //GCon->Logf(NAME_Debug, "x02: %s(%u): checking pobj #%d...", GetClass()->GetName(), GetUniqueId(), po->tag);
          //GCon->Logf(NAME_Debug, "x02: %s(%u): HIT pobj #%d... (%g:%g) (%g:%g)", GetClass()->GetName(), GetUniqueId(), po->tag, po->pofloor.minz, po->poceiling.maxz, TryOrg.z, z1);
          // hit pobj
          if (hitent) *hitent = nullptr;
          return true;
        }
      }
    }
  }

  return false;
}


//=============================================================================
//
//  VEntity::FakeZMovement
//
//  Fake the zmovement so that we can check if a move is legal
//
//=============================================================================
TVec VEntity::FakeZMovement () {
  TVec Ret = TVec(0, 0, 0);
  eventCalcFakeZMovement(Ret, host_frametime);
  // clip movement
  if (Ret.z <= FloorZ) Ret.z = FloorZ; // hit the floor
  if (Ret.z+Height > CeilingZ) Ret.z = CeilingZ-Height; // hit the ceiling
  return Ret;
}


//=============================================================================
//
//  VEntity::CheckOnmobj
//
//  Checks if an object is above another object
//
//=============================================================================
VEntity *VEntity::CheckOnmobj () {
  // can't hit things, or not solid? (check it here to save one VM invocation)
  if ((EntityFlags&(EF_ColideWithThings|EF_Solid)) != (EF_ColideWithThings|EF_Solid)) return nullptr;
  VEntity *ee = nullptr;
  if (TestMobjZ(FakeZMovement(), &ee)) return ee;
  return nullptr;
}


//==========================================================================
//
//  VEntity::CheckSides
//
//  This routine checks for Lost Souls trying to be spawned    // phares
//  across 1-sided lines, impassible lines, or "monsters can't //   |
//  cross" lines. Draw an imaginary line between the PE        //   V
//  and the new Lost Soul spawn spot. If that line crosses
//  a 'blocking' line, then disallow the spawn. Only search
//  lines in the blocks of the blockmap where the bounding box
//  of the trajectory line resides. Then check bounding box
//  of the trajectory vs. the bounding box of each blocking
//  line to see if the trajectory and the blocking line cross.
//  Then check the PE and LS to see if they're on different
//  sides of the blocking line. If so, return true, otherwise
//  false.
//
//==========================================================================
bool VEntity::CheckSides (TVec lsPos) {
  // here is the bounding box of the trajectory
  float tmbbox[4];
  tmbbox[BOX2D_LEFT] = min2(Origin.x, lsPos.x);
  tmbbox[BOX2D_RIGHT] = max2(Origin.x, lsPos.x);
  tmbbox[BOX2D_TOP] = max2(Origin.y, lsPos.y);
  tmbbox[BOX2D_BOTTOM] = min2(Origin.y, lsPos.y);

  // determine which blocks to look in for blocking lines
  DeclareMakeBlockMapCoordsBBox2D(tmbbox, xl, yl, xh, yh);

  //k8: is this right?
  int projblk = (EntityFlags&VEntity::EF_Missile ? GetMissileLineBlockFlag() : 0);

  // xl->xh, yl->yh determine the mapblock set to search
  //++validcount; // prevents checking same line twice
  XLevel->IncrementValidCount();
  for (int bx = xl; bx <= xh; ++bx) {
    for (int by = yl; by <= yh; ++by) {
      for (auto &&it : XLevel->allBlockLines(bx, by)) {
        line_t *ld = it.line();
        // Checks to see if a PE->LS trajectory line crosses a blocking
        // line. Returns false if it does.
        //
        // tmbbox holds the bounding box of the trajectory. If that box
        // does not touch the bounding box of the line in question,
        // then the trajectory is not blocked. If the PE is on one side
        // of the line and the LS is on the other side, then the
        // trajectory is blocked.
        //
        // Currently this assumes an infinite line, which is not quite
        // correct. A more correct solution would be to check for an
        // intersection of the trajectory and the line, but that takes
        // longer and probably really isn't worth the effort.

        if (ld->flags&(ML_BLOCKING|ML_BLOCKMONSTERS|ML_BLOCKEVERYTHING|projblk)) {
          if (tmbbox[BOX2D_LEFT] <= ld->bbox2d[BOX2D_RIGHT] &&
              tmbbox[BOX2D_RIGHT] >= ld->bbox2d[BOX2D_LEFT] &&
              tmbbox[BOX2D_TOP] >= ld->bbox2d[BOX2D_BOTTOM] &&
              tmbbox[BOX2D_BOTTOM] <= ld->bbox2d[BOX2D_TOP])
          {
            if (ld->PointOnSide(Origin) != ld->PointOnSide(lsPos)) return true; // line blocks trajectory
          }
        }

        // line doesn't block trajectory
      }
    }
  }

  return false;
}


//==========================================================================
//
//  Script natives
//
//==========================================================================
IMPLEMENT_FUNCTION(VEntity, CheckSides) {
  TVec lsPos;
  vobjGetParamSelf(lsPos);
  RET_BOOL(Self->CheckSides(lsPos));
}

//native final bool TestMobjZ ();
// returns `true` if not blocked
IMPLEMENT_FUNCTION(VEntity, TestMobjZ) {
  vobjGetParamSelf();
  RET_BOOL(!Self->TestMobjZ(Self->Origin)); // returns `false` if not blocked, so invert it
}


//native final bool TestMobjZEx (TVec pos, optional out Entity hitent);
// returns `true` if not blocked
IMPLEMENT_FUNCTION(VEntity, TestMobjZEx) {
  TVec pos;
  VOptParamPtr<VEntity *> hitent;
  vobjGetParamSelf(pos, hitent);
  VEntity *he = nullptr;
  const bool res = !Self->TestMobjZ(pos, &he); // returns `false` if not blocked, so invert it
  if (!res && hitent.specified) *hitent.value = he;
  RET_BOOL(res);
}

IMPLEMENT_FUNCTION(VEntity, CheckOnmobj) {
  vobjGetParamSelf();
  RET_REF(Self->CheckOnmobj());
}

IMPLEMENT_FUNCTION(VEntity, IsInPolyObj) {
  polyobj_t *po;
  vobjGetParamSelf(po);
  RET_BOOL(Self->IsInPolyObj(po));
}

IMPLEMENT_FUNCTION(VEntity, RoughBlockSearch) {
  VEntity **EntPtr;
  int Distance;
  vobjGetParamSelf(EntPtr, Distance);
  RET_PTR(new VRoughBlockSearchIterator(Self, Distance, EntPtr));
}

// native void UpdateVelocity (float DeltaTime, bool allowSlopeFriction);
IMPLEMENT_FUNCTION(VEntity, UpdateVelocity) {
  float DeltaTime;
  bool allowSlopeFriction;
  vobjGetParamSelf(DeltaTime, allowSlopeFriction);
  Self->UpdateVelocity(DeltaTime, allowSlopeFriction);
}

// native final float GetBlockingHeightFor (Entity other);
IMPLEMENT_FUNCTION(VEntity, GetBlockingHeightFor) {
  VEntity *other;
  vobjGetParamSelf(other);
  RET_FLOAT(other ? Self->GetBlockingHeightFor(other) : 0.0f);
}
