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
#include "../render/r_public.h"
#include "../server/sv_local.h"
#include "../sound/sound.h"
#include "p_entity.h"
#include "p_levelinfo.h"
#include "p_player.h"
#include "../utils/qs_data.h"

//#define VV_SETSTATE_DEBUG

#ifdef VV_SETSTATE_DEBUG
# define VSLOGF(...)  GCon->Logf(NAME_Debug, __VA_ARGS__)
#else
# define VSLOGF(...)  (void)0
#endif


// roughly smaller than lowest fixed point 16.16 (it is more like 0.0000152587890625)
#define PHYS_MIN_SPEED  (0.000016f*2.0f)


IMPLEMENT_CLASS(V, Entity);


// ////////////////////////////////////////////////////////////////////////// //
static VCvarB _decorate_dont_warn_about_invalid_labels("_decorate_dont_warn_about_invalid_labels", false, "Don't do this!", CVAR_Archive|CVAR_PreInit);
static VCvarB dbg_disable_state_advance("dbg_disable_state_advance", false, "Disable states processing (for debug)?", CVAR_PreInit);

static VCvarB dbg_emulate_broken_gozzo_gotos("dbg_emulate_broken_gozzo_gotos", false, "Emulate (partially) broken GZDoom decorate gotos to missing labels?", CVAR_Archive);

static VCvarB vm_optimise_statics("vm_optimise_statics", true, "Try to detect some static things, and don't run physics for them? (DO NOT USE, IT IS GLITCHY!)", CVAR_Archive);

extern VCvarB dbg_vm_show_tick_stats;


// ////////////////////////////////////////////////////////////////////////// //
VClass *VEntity::clsInventory = nullptr;

static VClass *classSectorThinker = nullptr;
static VField *fldNextAffector = nullptr;

static VClass *classScroller = nullptr;
static VField *fldCarryScrollX = nullptr;
static VField *fldCarryScrollY = nullptr;
static VField *fldVDX = nullptr;
static VField *fldVDY = nullptr;
static VField *fldbAccel = nullptr;

static VClass *classEntityEx = nullptr;
static VField *fldbWindThrust = nullptr;
static VField *fldLastScrollOrig = nullptr;
static VField *fldbNoTimeFreeze = nullptr;
static VField *fldbInChase = nullptr;
static VField *fldbInFloat = nullptr;
static VField *fldbSkullFly = nullptr;
static VField *fldbTouchy = nullptr;
static VField *fldbDormant = nullptr;

static VField *fldiBloodColor = nullptr;
static VField *fldiBloodTranslation = nullptr;

static VClass *classActor = nullptr;


// ////////////////////////////////////////////////////////////////////////// //
struct SavedVObjectPtr {
public:
  VObject **ptr;
  VObject *saved;
public:
  VV_DISABLE_COPY(SavedVObjectPtr)
  inline SavedVObjectPtr (VObject **aptr) noexcept : ptr(aptr), saved(*aptr) {}
  inline ~SavedVObjectPtr () noexcept { *ptr = saved; }
};


struct PCSaver {
public:
  VStateCall **ptr;
  VStateCall *PrevCall;
public:
  VV_DISABLE_COPY(PCSaver)
  inline PCSaver (VStateCall **aptr) noexcept : ptr(aptr), PrevCall(nullptr) { if (aptr) PrevCall = *aptr; }
  inline ~PCSaver () noexcept { if (ptr) *ptr = PrevCall; ptr = nullptr; }
};


// ////////////////////////////////////////////////////////////////////////// //
struct SetStateGuard {
public:
  VEntity *ent;
public:
  VV_DISABLE_COPY(SetStateGuard)
  // constructor increases invocation count
  inline SetStateGuard (VEntity *aent) noexcept : ent(aent) { aent->setStateWatchCat = 0; }
  inline ~SetStateGuard () noexcept { ent->setStateWatchCat = 0; }
};



//==========================================================================
//
//  VEntity::EntityStaticInit
//
//==========================================================================
void VEntity::EntityStaticInit () {
  // ah, 'cmon, we are Doom engine, stop dreaming about being something else
  // any missing field/class is a fatality
  // this also allows to skip checks for "do we have this field"?

  // prepare classes
  classEntityEx = FindClassChecked("EntityEx");
  classActor = FindClassChecked("Actor");
  classSectorThinker = FindClassChecked("SectorThinker");
  classScroller = FindClassChecked("Scroller");
  clsInventory = FindClassChecked("Inventory");

  // prepare fields

  // `SectorThinker`
  fldNextAffector = FindTypedField(classSectorThinker, "NextAffector", TYPE_Reference, classSectorThinker);

  // `Scroller`
  fldCarryScrollX = FindTypedField(classScroller, "CarryScrollX", TYPE_Float);
  fldCarryScrollY = FindTypedField(classScroller, "CarryScrollY", TYPE_Float);
  fldVDX = FindTypedField(classScroller, "VDX", TYPE_Float);
  fldVDY = FindTypedField(classScroller, "VDY", TYPE_Float);
  fldbAccel = FindTypedField(classScroller, "bAccel", TYPE_Bool);

  // `EntityEx`
  fldbInChase = FindTypedField(classEntityEx, "bInChase", TYPE_Bool);
  fldbNoTimeFreeze = FindTypedField(classEntityEx, "bNoTimeFreeze", TYPE_Bool);
  fldbWindThrust = FindTypedField(classEntityEx, "bWindThrust", TYPE_Bool);
  fldLastScrollOrig = FindTypedField(classEntityEx, "lastScrollCheckOrigin", TYPE_Vector);
  fldbInFloat = FindTypedField(classEntityEx, "bInFloat", TYPE_Bool);
  fldbSkullFly = FindTypedField(classEntityEx, "bSkullFly", TYPE_Bool);
  fldbTouchy = FindTypedField(classEntityEx, "bTouchy", TYPE_Bool);
  fldbDormant = FindTypedField(classEntityEx, "bDormant", TYPE_Bool);
  // blood
  fldiBloodColor = FindTypedField(classEntityEx, "BloodColor", TYPE_Int);
  fldiBloodTranslation = FindTypedField(classEntityEx, "BloodTranslation", TYPE_Int);

  // `Actor` -- no fields yet
}


//==========================================================================
//
//  VEntity::PostCtor
//
//==========================================================================
void VEntity::PostCtor () {
  FlagsEx &= ~(EFEX_IsEntityEx|EFEX_IsActor);
  if (classEntityEx && GetClass()->IsChildOf(classEntityEx)) FlagsEx |= EFEX_IsEntityEx;
  if (classActor && GetClass()->IsChildOf(classActor)) FlagsEx |= EFEX_IsActor; else
  Super::PostCtor();
}


//==========================================================================
//
//  VEntity::SerialiseOther
//
//==========================================================================
void VEntity::SerialiseOther (VStream &Strm) {
  Super::SerialiseOther(Strm);
  if (Strm.IsLoading()) {
    FlagsEx &= ~(EFEX_IsEntityEx|EFEX_IsActor);
    if (classEntityEx && GetClass()->IsChildOf(classEntityEx)) FlagsEx |= EFEX_IsEntityEx;
    if (classActor && GetClass()->IsChildOf(classActor)) FlagsEx |= EFEX_IsActor;
    //CHECKME: k8: is this right? voodoo dolls?
    if (EntityFlags&EF_IsPlayer) Player->MO = this;
    SubSector = nullptr; // must mark as not linked
    // proper checks for player and monsters
    if (IsPlayerOrMonster()) {
      LinkToWorld(true); // proper floor check
    } else {
      // more complex code
      //FIXME: special processing for `bSpecial`? (those are pickups)
      // first use "proper" check to get FloorZ
      LinkToWorld(true);
      // now see if FloorZ is equal to our Z
      if (FloorZ != Origin.z) {
        // try non-proper check
        LinkToWorld();
        if (FloorZ != Origin.z) {
          // both seems to be wrong, revert to "proper" again
          LinkToWorld(true);
        }
      }
    }
    // dedicated server cannot load games anyway
#ifdef CLIENT
    // create and fix blood translation for actor if necessary
    int bcolor = fldiBloodColor->GetInt(this);
    if (bcolor) {
      int btrans = fldiBloodTranslation->GetInt(this);
      if (btrans) {
        auto trans = R_GetCachedTranslation(btrans, XLevel);
        if (!trans || trans->isBloodTranslation) btrans = 0; // recreate it
      }
      if (!btrans) {
        btrans = R_GetBloodTranslation(bcolor, true/*allowAdd*/);
        fldiBloodTranslation->SetInt(this, btrans);
      }
    }
#endif
  }
}


//==========================================================================
//
//  VEntity::DestroyThinker
//
//==========================================================================
void VEntity::DestroyThinker () {
  if (!IsGoingToDie()) {
    if (Role == ROLE_Authority) {
      // stop any playing sound
      StopSound(0);
      eventDestroyed();
    }

    // unlink from sector and block lists
    // but we may be inside a state action (or VC code), so keep `Sector` and `SubSector` alive
    subsector_t *oldSubSector = SubSector;
    sector_t *oldSector = Sector;
    UnlinkFromWorld();
    if (XLevel) XLevel->DelSectorList();
    if (TID && Level) RemoveFromTIDList();
    TID = 0; // just in case
    // those could be still used, restore
    // note that it is still safe to call `UnlinkFromWorld()` on such "half-linked" object
    // but to play safe, set some guard flags
    EntityFlags |= EF_NoSector|EF_NoBlockmap;
    SubSector = oldSubSector;
    Sector = oldSector;

    Super::DestroyThinker();
  }
}


//==========================================================================
//
//  VEntity::AddedToLevel
//
//==========================================================================
void VEntity::AddedToLevel () {
  VThinker::AddedToLevel();
  if (!XLevel->NextSoundOriginID) XLevel->NextSoundOriginID = 1;
  SoundOriginID = XLevel->NextSoundOriginID+(SNDORG_Entity<<24);
  XLevel->NextSoundOriginID = (XLevel->NextSoundOriginID+1)&0x00ffffff;
  XLevel->RegisterEntitySoundID(this, SoundOriginID);
}


//==========================================================================
//
//  VEntity::RemovedFromLevel
//
//==========================================================================
void VEntity::RemovedFromLevel () {
  if (XLevel && SoundOriginID) {
    XLevel->RemoveEntitySoundID(SoundOriginID);
    SoundOriginID = 0;
  }
  VThinker::RemovedFromLevel();
}


//==========================================================================
//
//  VEntity::NeedPhysics
//
//==========================================================================
bool VEntity::NeedPhysics (const float deltaTime) {
  if (IsPlayer()) return true;
  if (Owner) return true; // inventory

  // if it moved, play safe and perform physics code
  // this may still miss sector enter/exit effects (fuck!)
  // changed 2d position may mean that something tried to move this thing without using velocities
  // it is better to play safe here
  if (PrevTickOrigin.x != Origin.x || PrevTickOrigin.y != Origin.y) return true;

  const unsigned eflags = EntityFlags;
  const unsigned eflagsex = FlagsEx;

  // if this is not `EntityEx`, then something is wrong! do not try to optimise it
  if (!(eflagsex&EFEX_IsEntityEx)) return true;

  // from now on this is `EntityEx`, for sure

  //if (WaterLevel != 0) return true; // i don't think that we need to check this
  // 2d velocity always needs physics
  if (Velocity.x != 0.0f || Velocity.y != 0.0f) return true;

  if (eflags&(EF_Fly|EF_Blasted)) return true; // blasted has special logic even when 2d velocity is zero
  // `bSkullFly` and `bTouchy` has special logic when 2d velocity is zero
  if (fldbSkullFly->GetBool(this)) return true; // "slammed skull" logic in physics changes skull state
  if (fldbTouchy->GetBool(this)) return true; // "touchy" arms a mine
  // `bInFloat` has some more special logic i am too lazy to untangle
  if (fldbInFloat->GetBool(this)) return true;
  // activation condition for special code for floaters
  if ((eflags&(EF_Corpse|EF_Float)) == EF_Float && Target && !Target->IsGoingToDie() && !fldbDormant->GetBool(this)) return true;
  // windthrust should be done in physics code too
  if (fldbWindThrust->GetBool(this)) return true;

  const float hgt = max2(0.0f, Height);
  const float oz = Origin.z;

  // check floor/ceiling stickers
  if (eflagsex&(EFEX_StickToFloor|EFEX_StickToCeiling)) {
    if (eflagsex&EFEX_StickToFloor) {
      return (oz != FloorZ);
    } else {
      return (oz+hgt != CeilingZ);
    }
  }

  // if entity is inside a floor or a ceiling, it needs full physics
  // this is because `StepMove()` or such may move entity over the stairs, for example, but the real thing is done in `ZMovement()`
  // do this only for monsters and missiles -- assume that pickups, barrels, decorations, etc. won't walk by themselves ;-)
  if ((eflags&EF_Missile)|(eflagsex&EFEX_Monster)) {
    if (oz < FloorZ || oz+hgt > CeilingZ) return true;
  }

  // check vertical velocity
  if (!(eflags&EF_NoGravity)) {
    // going up, or not standing on a floor?
    if (Velocity.z > 0.0f || oz != FloorZ) return true;
  } else {
    // no gravity, check for any vertical velocity
    if (Velocity.z != 0.0f) return true;
  }

  // check for scrollers
  if (classScroller->InstanceCountWithSub && (eflags&(EF_NoSector|EF_ColideWithWorld)) == EF_ColideWithWorld &&
      Sector && fldLastScrollOrig->GetVec(this) != Origin)
  {
    // do as much as we can here (it is *MUCH* faster than VM code)
    // fallback to VM checks only if they have a chance to succeed
    for (msecnode_t *mnode = TouchingSectorList; mnode; mnode = mnode->TNext) {
      sector_t *sec = mnode->Sector;
      VThinker *th = sec->AffectorData;
      if (!th) continue;
      if (th->GetClass()->IsChildOf(classSectorThinker)) {
        // check if we have any transporters
        //TODO: add another velocity-affecting classes here if they will ever emerge
        bool needCheck = false;
        while (th) {
          if (th->GetClass()->IsChildOf(classScroller)) {
            if (fldCarryScrollX->GetFloat(th) != 0.0f || fldCarryScrollY->GetFloat(th) != 0.0f ||
                (fldbAccel->GetBool(th) && (fldVDX->GetFloat(th) != 0.0f || fldVDX->GetFloat(th) != 0.0f)))
            {
              needCheck = true;
              break;
            }
          }
          th = (VThinker *)fldNextAffector->GetObjectValue(th);
        }
        if (needCheck) {
          if (eventPhysicsCheckScroller()) return true;
          break;
        }
      } else {
        // affector is not a sector thinker: this should not happen, but let's play safe
        if (eventPhysicsCheckScroller()) return true;
        break;
      }
    }
  }

  // check for state change
  if (!(eflagsex&EFEX_AllowSimpleTick) && StateTime >= 0.0f && StateTime-deltaTime <= 0.0f) {
    if (HasStateMethodIfAdvanced(deltaTime)) {
      // has state method call, need physics (due to possible movement)
      //if (VStr::strEqu(GetClass()->GetName(), "LostSoul")) GCon->Logf(NAME_Debug, "%s:%u: fallback to full physics -- %s", GetClass()->GetName(), GetUniqueId(), (State ? *State->Loc.toStringNoCol() : "<none>"));
      return true;
    }
  }

  return false;
}


//==========================================================================
//
//  VEntity::Tick
//
//==========================================================================
void VEntity::Tick (float deltaTime) {
  ++dbgEntityTickTotal;
  // advance it here, why not
  // may be moved down later if some VC code will start using it
  DataGameTime = XLevel->Time+deltaTime;
  // skip ticker?
  const unsigned eflagsex = FlagsEx;
  if (eflagsex&EFEX_NoTickGrav) {
    ++dbgEntityTickNoTick;
    PrevTickOrigin = Origin; // it is not used in notick code
    #ifdef CLIENT
    //GCon->Logf(NAME_Debug, "*** %s ***", GetClass()->GetName());
    #endif
    // stick to floor or ceiling?
    if (SubSector) {
      if (eflagsex&(EFEX_StickToFloor|EFEX_StickToCeiling)) {
        tmtrace_t tmtrace;
        CheckRelPositionPoint(tmtrace, Origin);
        if (eflagsex&EFEX_StickToFloor) {
          Origin.z = tmtrace.FloorZ;
        } else {
          #ifdef CLIENT
          //const float oldz = Origin.z;
          #endif
          //Origin.z = SV_GetHighestSolidPointZ(SubSector->sector, Origin, false)-Height; // don't ignore 3d floors
          Origin.z = tmtrace.CeilingZ;
          #ifdef CLIENT
          //GCon->Logf(NAME_Debug, "*** %s ***: stick to ceiling; oldz=%g; newz=%g", GetClass()->GetName(), oldz, Origin.z);
          #endif
        }
      } else if (!(EntityFlags&EF_NoGravity)) {
        // it is always at floor level
        tmtrace_t tmtrace;
        CheckRelPositionPoint(tmtrace, Origin);
        #ifdef CLIENT
        //const float oldz = Origin.z;
        #endif
        Origin.z = tmtrace.FloorZ;
        #ifdef CLIENT
        //GCon->Logf(NAME_Debug, "*** %s ***: down to earth; oldz=%g; newz=%g", GetClass()->GetName(), oldz, Origin.z);
        #endif
      }
    }
    // in MP games, there is no `GLevelInfo`
    if (GLevelInfo && GLevelInfo->LevelInfoFlags2&VLevelInfo::LIF2_Frozen) return;
    if (eflagsex&EFEX_NoTickGravLT) {
      #ifdef CLIENT
      //GCon->Logf(NAME_Debug, "  : %s lifetime (lmt=%g)", GetClass()->GetName(), LastMoveTime-deltaTime);
      #endif
      //GCon->Logf(NAME_Debug, "*** %s ***", GetClass()->GetName());
      // perform lifetime logic
      // LastMoveTime is time before the next step
      // PlaneAlpha is fadeout after the time expires:
      // if PlaneAlpha is:
      //   <=0: die immediately
      //    >0: fadeout step time
      // it fades out by 0.016 per step
      LastMoveTime -= deltaTime;
      while (LastMoveTime <= 0) {
        // die now
        if (PlaneAlpha <= 0) {
          //GCon->Logf(NAME_Debug, "%s: DIED!", GetClass()->GetName());
          DestroyThinker();
          return;
        }
        LastMoveTime += PlaneAlpha;
        Alpha -= 0.016;
        // did it faded out completely?
        if (Alpha <= 0.002f) {
          //GCon->Logf(NAME_Debug, "%s: FADED!", GetClass()->GetName());
          DestroyThinker();
          return;
        }
        if (RenderStyle == STYLE_Normal) RenderStyle = STYLE_Translucent;
      }
    }
    return;
  }

  // allow optimiser in netplay servers too, because why not?
  const bool doSimplifiedTick =
    GGameInfo->NetMode != NM_Client &&
    !(eflagsex&EFEX_AlwaysTick) &&
    vm_optimise_statics.asBool() &&
    !NeedPhysics(deltaTime);

  PrevTickOrigin = Origin;

  // reset 'in chase' (we can do it before ticker instead of after it, it doesn't matter)
  if (eflagsex&EFEX_IsEntityEx) fldbInChase->SetBool(this, false);

  // `Mass` is clamped in `OnMapSpawn()`, and we should take care of it in VC code
  // clamp velocity (just in case)
  if (!doSimplifiedTick) {
    if (dbg_vm_show_tick_stats.asBool()) {
      GCon->Logf(NAME_Debug, "%s: cannot simplify tick; vel=(%g,%g,%g); z=%g; floorz=%g; statetime=%g (%g); Owner=%p; justmoved=%d; vcheck=%d; floorcheck=%d; stcheck=%d; interdiff=(%g,%g,%g)",
      GetClass()->GetName(), Velocity.x, Velocity.y, Velocity.z, Origin.z, FloorZ, StateTime, StateTime-deltaTime, Owner,
      (int)((MoveFlags&MVF_JustMoved) != 0), (int)(!(fabsf(Velocity.x) > 0.000016f*4.0f || fabsf(Velocity.x) > 0.000016f*4.0f)),
      (int)(!(Velocity.z > 0.0f || Origin.z != FloorZ)),
      (int)(StateTime < 0.0f || StateTime-deltaTime > 0.0f),
      (LastMoveOrigin-Origin).x, (LastMoveOrigin-Origin).y, (LastMoveOrigin-Origin).z);
    }
    Velocity.clampScaleInPlace(PHYS_MAXMOVE);
    // call normal ticker
    VThinker::Tick(deltaTime);
  } else {
    ++dbgEntityTickSimple;
    // in MP games, there is no `GLevelInfo`
    if (GLevelInfo && GLevelInfo->LevelInfoFlags2&VLevelInfo::LIF2_Frozen) {
      const bool noFreeze = ((eflagsex&EFEX_IsEntityEx) && fldbNoTimeFreeze->GetBool(this));
      if (!noFreeze) return;
    }
    const VState *oldState = State;
    eventSimplifiedTick(deltaTime);
    // advance state, if necessary (but don't do it if the state was changed)
    if (oldState == State && StateTime >= 0.0) {
      if (StateTime-deltaTime > 0.0f) {
        StateTime -= deltaTime;
      } else {
        // for some reason this doesn't work right for floating mobjs (like caco), and maybe for some others
        // i really hope that this is safe to call for states without method calls
        TAVec lastAngles = Angles;
        TVec lastOrg = Origin;
        MoveFlags |= MVF_JustMoved; // teleports and force origin changes will reset it
        if (AdvanceState(deltaTime)) {
          // possibly moved, set new interpolation state
          // if mobj is not moved, the engine will take care of it
          LastMoveOrigin = lastOrg;
          LastMoveAngles = lastAngles;
          LastMoveTime = XLevel->Time;
          LastMoveDuration = StateTime;
        }
        //if (VStr::strEqu(GetClass()->GetName(), "LostSoul")) GCon->Logf(NAME_Debug, "%s:%u:   new simplified state -- %s", GetClass()->GetName(), GetUniqueId(), (State ? *State->Loc.toStringNoCol() : "<none>"));
      }
    }
  }
}


//==========================================================================
//
//  VEntity::SetTID
//
//==========================================================================
void VEntity::SetTID (int tid) {
  RemoveFromTIDList();
  if (tid && Role == ROLE_Authority && !IsGoingToDie()) InsertIntoTIDList(tid);
}


//==========================================================================
//
//  VEntity::InsertIntoTIDList
//
//==========================================================================
void VEntity::InsertIntoTIDList (int tid) {
  vassert(tid != 0);
  vassert(TID == 0);
  vassert(TIDHashNext == nullptr);
  vassert(TIDHashPrev == nullptr);
  TID = tid;
  const int HashIndex = tid&(VLevelInfo::TID_HASH_SIZE-1);
  TIDHashPrev = nullptr;
  TIDHashNext = Level->TIDHash[HashIndex];
  if (TIDHashNext) TIDHashNext->TIDHashPrev = this;
  Level->TIDHash[HashIndex] = this;
}


//==========================================================================
//
//  VEntity::RemoveFromTIDList
//
//==========================================================================
void VEntity::RemoveFromTIDList () {
  if (!TID) return; // no TID, which means it's not in the cache
  if (TIDHashNext) TIDHashNext->TIDHashPrev = TIDHashPrev;
  if (TIDHashPrev) {
    TIDHashPrev->TIDHashNext = TIDHashNext;
  } else {
    const int HashIndex = TID&(VLevelInfo::TID_HASH_SIZE-1);
    vassert(Level->TIDHash[HashIndex] == this);
    Level->TIDHash[HashIndex] = TIDHashNext;
  }
  TIDHashNext = TIDHashPrev = nullptr;
  TID = 0;
}


//==========================================================================
//
//  VEntity::SetState
//
//  returns true if the actor is still present
//
//==========================================================================
bool VEntity::SetState (VState *InState) {
  if (!InState || IsGoingToDie()) {
    if (developer) GCon->Logf(NAME_Dev, "   (00):%s: dead (0x%04x) before state actions, state is %s", *GetClass()->GetFullName(), GetFlags(), (InState ? *InState->Loc.toStringNoCol() : "<none>"));
    State = nullptr;
    StateTime = -1;
    DispSpriteFrame = 0;
    DispSpriteName = NAME_None;
    //GCon->Logf(NAME_Debug, "*** 000: STATE DYING THINKER %u: %s", GetUniqueId(), GetClass()->GetName());
    if (!IsGoingToDie()) DestroyThinker();
    return false;
  }

  // the only way we can arrive here is via decorate call
  if (setStateWatchCat) {
    vassert(InState);
    VSLOGF("%s: recursive(%d) SetState, %s to %s", GetClass()->GetName(), setStateWatchCat, (State ? *State->Loc.toStringNoCol() : "<none>"), *InState->Loc.toStringNoCol());
    setStateNewState = InState;
    return true;
  }

  {
    SetStateGuard guard(this);
    ++validcountState;

    VState *st = InState;
    do {
      if (!st) { State = nullptr; break; }

      if (++setStateWatchCat > 512 /*|| st->validcount == validcountState*/) {
        //k8: FIXME! what to do here?
        GCon->Logf(NAME_Error, "WatchCat interrupted `%s::SetState()` at '%s' (%s)!", *GetClass()->GetFullName(), *st->Loc.toStringNoCol(), (st->validcount == validcountState ? "loop" : "timeout"));
        //StateTime = 13.0f;
        break;
      }
      st->validcount = validcountState;

      VSLOGF("%s: loop SetState(%d), %s to %s", GetClass()->GetName(), setStateWatchCat, (State ? *State->Loc.toStringNoCol() : "<none>"), *st->Loc.toStringNoCol());

      // remember current sprite and frame
      UpdateDispFrameFrom(st);

      State = st;
      StateTime = eventGetStateTime(st, st->Time);
      EntityFlags &= ~EF_FullBright;
      //GCon->Logf("%s: loop SetState(%d), time %g, %s", GetClass()->GetName(), setStateWatchCat, StateTime, *State->Loc.toStringNoCol());

      // modified handling
      // call action functions when the state is set
      if (st->Function && Role == ROLE_Authority) {
        XLevel->CallingState = State;
        {
          SavedVObjectPtr svp(&_stateRouteSelf);
          _stateRouteSelf = nullptr;
          setStateNewState = nullptr;
          ExecuteFunctionNoArgs(this, st->Function); //k8: allow VMT lookups (k8:why?)
          if (IsGoingToDie()) {
            State = nullptr;
            break;
          }
          if (setStateNewState) {
            // recursive invocation set a new state
            st = setStateNewState;
            StateTime = 0.0f;
            continue;
          }
        }
      }

      st = State->NextState;
    } while (!StateTime);
    VSLOGF("%s: SetState(%d), done with %s", GetClass()->GetName(), setStateWatchCat, (State ? *State->Loc.toStringNoCol() : "<none>"));
  }
  vassert(setStateWatchCat == 0);

  if (!State || IsGoingToDie()) {
    //GCon->Logf(NAME_Debug, "*** 001: STATE DYING THINKER %u: %s from %s", GetUniqueId(), GetClass()->GetName(), (InState ? *InState->Loc.toStringNoCol() : "<none>"));
    DispSpriteFrame = 0;
    DispSpriteName = NAME_None;
    StateTime = -1;
    if (!IsGoingToDie()) DestroyThinker();
    return false;
  }

  return true;
}


//==========================================================================
//
//  VEntity::SetInitialState
//
//  Returns true if the actor is still present.
//
//==========================================================================
void VEntity::SetInitialState (VState *InState) {
  State = InState;
  if (InState) {
    UpdateDispFrameFrom(InState);
    StateTime = eventGetStateTime(InState, InState->Time);
    if (StateTime > 0.0f) StateTime += 0.0002f; //k8: delay it slightly, so spawner may do its business
    // first state can be a goto; follow it
    if (DispSpriteName == NAME_None && InState->NextState && StateTime <= 0.0f) {
      UpdateDispFrameFrom(InState->NextState);
      //GCon->Logf(NAME_Debug, "SetInitialState: jumpfix for `%s`", GetClass()->GetName());
    }
  } else {
    DispSpriteFrame = 0;
    DispSpriteName = NAME_None;
    StateTime = -1.0f;
  }
  if (NoTickGravOnIdleType && (!State || StateTime < 0.0f)) PerformOnIdle();
}


//==========================================================================
//
//  VEntity::PerformOnIdle
//
//==========================================================================
void VEntity::PerformOnIdle () {
  //GCon->Logf(NAME_Debug, "*** %s: idle; ntit=%d", GetClass()->GetName(), NoTickGravOnIdleType);
  if (NoTickGravOnIdleType) {
    // remove from blockmap
    if (!(EntityFlags&EF_NoBlockmap)) {
      UnlinkFromWorld();
      EntityFlags |= EF_NoBlockmap;
      LinkToWorld(true); // ...and link back again
    }
    switch (NoTickGravOnIdleType) {
      case 1: // switches object to "k8vavoomInternalNoTickGrav" when it enters idle state (the one with negative duration)
        FlagsEx |= EFEX_NoTickGrav;
        //GCon->Logf(NAME_Debug, "*** %s becomes notick", GetClass()->GetName());
        break;
      case 2: // switches object to "NoInteraction" when it enters idle state (the one with negative duration)
        FlagsEx |= EFEX_NoInteraction;
        //GCon->Logf(NAME_Debug, "*** %s becomes nointeraction", GetClass()->GetName());
        break;
      //default: NoTickGravOnIdleType = 0; break; // just in case
    }
  }
}


//==========================================================================
//
//  VEntity::HasStateMethodIfAdvanced
//
//==========================================================================
bool VEntity::HasStateMethodIfAdvanced (float deltaTime) const noexcept {
  const VState *state = State;
  if (!state || deltaTime <= 0.0f) return false;
  float stime = StateTime;
  if (stime < 0.0f) return false;
  stime -= deltaTime;
  if (stime > 0.0f) return false;
  const VState *prevstate = state; // to detect loops
  bool doSecondMove = false;
  for (;;) {
    state = state->NextState;
    if (!state) return false;
    if (state->Function) return true;
    float stdur;
    if (state->TicType == VState::TicKind::TCK_Random) {
      stdur = max2(0, min2(state->Arg1, state->Arg2))/35.0f;
    } else {
      stdur = state->Time;
      if (stdur < 0.0f) return false;
    }
    if (prevstate) {
      if (doSecondMove) prevstate = prevstate->NextState;
      if (prevstate == state) return true; // endless loop detected, play safe
      doSecondMove = !doSecondMove;
    }
    stime += state->Time;
    if (stime > 0.0f) return false;
  }
  return false;
}


//==========================================================================
//
//  VEntity::AdvanceState
//
//  returns `false` if entity fried itself
//
//==========================================================================
bool VEntity::AdvanceState (float deltaTime) {
  if (dbg_disable_state_advance) return true;
  if (deltaTime < 0.0f) return true; // allow zero delta time to process zero-duration states
  if (!State) return true;

  //if (VStr::strEquCI(GetClass()->GetName(), "DeadDoomImp")) GCon->Logf(NAME_Debug, "%s: ADVANCE(000): state=%s", GetClass()->GetName(), (State ? *State->Loc.toStringNoCol() : "<none>"));

  if (StateTime >= 0.0f) {
    //const bool dbg = isDebugDumpEnt(this);
    //if (dbg) GCon->Logf(NAME_Debug, "%u:%s:%s: StateTime=%g (%g) (nst=%g); delta=%g (%g)", GetUniqueId(), GetClass()->GetName(), *State->Loc.toStringShort(), StateTime, StateTime*35.0f, StateTime-deltaTime, deltaTime, deltaTime*35.0f);
    // we can came here with zero-duration states; if we'll subtract delta time in such case, we'll overshoot for the whole frame
    if (StateTime > 0.0f) {
      // normal states
      StateTime -= deltaTime;
      // loop here, just in case current state duration is less than our delta time
      while (StateTime <= 0.0f) {
        const float tleft = StateTime; // "overjump time"
        if (!SetState(State->NextState)) return false; // freed itself
        if (!State) break; // just in case
        if (StateTime < 0.0f) { StateTime = -1.0f; break; } // force `-1` here just in case
        if (StateTime <= 0.0f) break; // zero should not end up here, but WatchCat can cause this
        // this somewhat compensates freestep instability (at least revenant missiles are more stable on a short term)
        //if (dbg) GCon->Logf(NAME_Debug, "%u:%s:%s:     tleft=%g; StateTime=%g (%g); rest=%g", GetUniqueId(), GetClass()->GetName(), *State->Loc.toStringShort(), tleft, StateTime, StateTime*35.0f, StateTime+tleft);
        StateTime += tleft;
      }
    } else {
      // zero-duration state; advance, and delay next non-zero state a little
      if (!SetState(State->NextState)) return false; // freed itself
      if (State) {
        //vassert(StateTime != 0.0f); // WatchCat can cause zero duration
        if (StateTime > 0.0f) StateTime += 0.0002f; // delay it slightly, so spawner may do its business
      }
    }
    if (NoTickGravOnIdleType && (!State || StateTime < 0.0f)) PerformOnIdle();
    //if (dbg && State) GCon->Logf(NAME_Debug, "%u:%s:%s:     END; StateTime=%g (%g); delta=%g (%g)", GetUniqueId(), GetClass()->GetName(), *State->Loc.toStringShort(), StateTime, StateTime*35.0f, deltaTime, deltaTime*35.0f);
  } else if (NoTickGravOnIdleType) {
    vassert(StateTime < 0.0f);
    PerformOnIdle();
  }
  /*
  if (State) {
    if (VStr::strEquCI(GetClass()->GetName(), "DeadDoomImp")) GCon->Logf(NAME_Debug, "%s: state=%s", GetClass()->GetName(), *State->Loc.toStringNoCol());
    UpdateDispFrameFrom(State);
  }
  */
  return true;
}


//==========================================================================
//
//  VEntity::FindState
//
//==========================================================================
VState *VEntity::FindState (VName StateName, VName SubLabel, bool Exact) {
  VStateLabel *Lbl = GetClass()->FindStateLabel(StateName, SubLabel, Exact);
  //k8: there's no need to manually resolve compound labels (like "A.B"), `VClass::FindStateLabel()` will do it for us
  //if (Lbl) GCon->Logf("VEntity::FindState(%s): found '%s' (%s : %s)", GetClass()->GetName(), *StateName, *Lbl->Name, *Lbl->State->Loc.toStringNoCol());
  return (Lbl ? Lbl->State : nullptr);
}


//==========================================================================
//
//  VEntity::FindStateEx
//
//==========================================================================
VState *VEntity::FindStateEx (VStr StateName, bool Exact) {
  TArray<VName> Names;
  VMemberBase::StaticSplitStateLabel(StateName, Names);
  VStateLabel *Lbl = GetClass()->FindStateLabel(Names, Exact);
  return (Lbl ? Lbl->State : nullptr);
}


//==========================================================================
//
//  VEntity::HasSpecialStates
//
//==========================================================================
bool VEntity::HasSpecialStates (VName StateName) {
  VStateLabel *Lbl = GetClass()->FindStateLabel(StateName);
  return (Lbl != nullptr && Lbl->SubLabels.length() > 0);
}


//==========================================================================
//
//  VEntity::GetStateEffects
//
//==========================================================================
void VEntity::GetStateEffects (TArray<VLightEffectDef *> &Lights, TArray<VParticleEffectDef *> &Part) const {
  // clear arrays
  Lights.resetNoDtor();
  Part.resetNoDtor();

  // check for valid state
  if (!State) return;

  // init state light effect
  if (!State->LightInited) {
    State->LightInited = true;
    State->LightDef = nullptr;
    if (State->LightName.length()) State->LightDef = R_FindLightEffect(State->LightName);
  }
  if (State->LightDef) Lights.Append(State->LightDef);

  // add class sprite effects
  for (auto &&it : GetClass()->SpriteEffects) {
    if (it.SpriteIndex != State->SpriteIndex) continue;
    if (it.Frame != -1 && it.Frame != (State->Frame&VState::FF_FRAMEMASK)) continue;
    if (it.LightDef) Lights.Append(it.LightDef);
    if (it.PartDef) Part.Append(it.PartDef);
  }
}


//==========================================================================
//
//  VEntity::HasAnyLightEffects
//
//==========================================================================
bool VEntity::HasAnyLightEffects () const {
  // check for valid state
  if (!State) return false;

  // init state light effect
  if (!State->LightInited) {
    State->LightInited = true;
    State->LightDef = nullptr;
    if (State->LightName.length()) State->LightDef = R_FindLightEffect(State->LightName);
  }
  if (State->LightDef) return true;

  // add class sprite effects
  for (auto &&it : GetClass()->SpriteEffects) {
    if (it.SpriteIndex != State->SpriteIndex) continue;
    if (it.Frame != -1 && it.Frame != (State->Frame&VState::FF_FRAMEMASK)) continue;
    if (it.LightDef) return true;
  }

  return false;
}


//==========================================================================
//
//  VEntity::CallStateChain
//
//==========================================================================
bool VEntity::CallStateChain (VEntity *Actor, VState *AState) {
  if (!Actor) return false;

  // set up state call structure
  //if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: CHAIN (Actor=%s); actorstate=%s; itstate=%s", *GetClass()->GetFullName(), *Actor->GetClass()->GetFullName(), (Actor->State ? *Actor->State->Loc.toStringNoCol() : "<none>"), (AState ? *AState->Loc.toStringNoCol() : "<none>"));
  PCSaver saver(&XLevel->StateCall);
  VStateCall Call;
  Call.Item = this;
  Call.State = AState;
  Call.Result = 1;
  XLevel->StateCall = &Call;

  int RunAway = 0;
  VState *S = AState;
  vuint8 res = 0;
  //const bool dbg = (isDebugDumpEnt(this) || isDebugDumpEnt(Actor));
  //if (dbg && S) GCon->Logf(NAME_Debug, "*** %u:%s(%s):%s: CallStateChain ENTER", GetUniqueId(), GetClass()->GetName(), Actor->GetClass()->GetName(), *S->Loc.toStringShort());
  while (S) {
    // check for infinite loops
    if (++RunAway > 512) {
      GCon->Logf(NAME_Warning, "entity '%s' state chain interrupted by WatchCat!", *Actor->GetClass()->GetFullName());
      GCon->Logf(NAME_Warning, "... state: '%s'", *S->Loc.toStringNoCol());
      res = 0; // watchcat break, oops
      break;
    }

    //if (dbg) GCon->Logf(NAME_Debug, "*** %u:%s(%s):%s:   calling state", GetUniqueId(), GetClass()->GetName(), Actor->GetClass()->GetName(), *S->Loc.toStringShort());
    Call.State = S;
    // call action function
    if (S->Function) {
      XLevel->CallingState = S;
      // assume success by default
      Call.Result = 1;
      ExecuteFunctionNoArgs(Actor, S->Function); //k8: allow VMT lookups (k8:why?)
      // at least one success means overal success (do it later)
      //res |= Call.Result;
    } else {
      Call.Result = 0; // don't modify
    }

    if (Call.State == S) {
      // abort immediately if next state loops to itself
      // in this case the overal result is always false
      if (S->NextState == S) { res = 0; break; }
      // advance to the next state
      S = S->NextState;
      // at least one success means overal success
      res |= Call.Result;
    } else {
      // there was a state jump, result should not be modified
      S = Call.State;
    }
  }

  //if (dbg) GCon->Logf(NAME_Debug, "*** %u:%s(%s):%s:  CallStateChain EXIT", GetUniqueId(), GetClass()->GetName(), Actor->GetClass()->GetName(), (S ? *S->Loc.toStringShort() : "<none>"));
  return !!res;
}


//==========================================================================
//
//  VEntity::StartSound
//
//==========================================================================
void VEntity::StartSound (VName Sound, vint32 Channel, float Volume, float Attenuation, bool Loop, bool Local) {
  if (!Sector) return;
  if (Sector->SectorFlags&sector_t::SF_Silent) return;
  //if (IsPlayer()) GCon->Logf(NAME_Debug, "player sound '%s' (sound class '%s', gender '%s')", *Sound, *SoundClass, *SoundGender);
  Super::StartSound(Origin, SoundOriginID,
    GSoundManager->ResolveEntitySound(SoundClass, SoundGender, Sound),
    Channel, Volume, Attenuation, Loop, Local);
}


//==========================================================================
//
//  VEntity::StartLocalSound
//
//==========================================================================
void VEntity::StartLocalSound (VName Sound, vint32 Channel, float Volume, float Attenuation) {
  if (Sector->SectorFlags&sector_t::SF_Silent) return;
  if (Player) {
    Player->eventClientStartSound(
      GSoundManager->ResolveEntitySound(SoundClass, SoundGender, Sound),
      TVec(0, 0, 0), /*0*/-666, Channel, Volume, Attenuation, false);
  }
}


//==========================================================================
//
//  VEntity::StopSound
//
//==========================================================================
void VEntity::StopSound (vint32 channel) {
  Super::StopSound(SoundOriginID, channel);
}


//==========================================================================
//
//  VEntity::StartSoundSequence
//
//==========================================================================
void VEntity::StartSoundSequence (VName Name, vint32 ModeNum) {
  if (Sector->SectorFlags&sector_t::SF_Silent) return;
  Super::StartSoundSequence(Origin, SoundOriginID, Name, ModeNum);
}


//==========================================================================
//
//  VEntity::AddSoundSequenceChoice
//
//==========================================================================
void VEntity::AddSoundSequenceChoice (VName Choice) {
  if (Sector->SectorFlags&sector_t::SF_Silent) return;
  Super::AddSoundSequenceChoice(SoundOriginID, Choice);
}


//==========================================================================
//
//  VEntity::StopSoundSequence
//
//==========================================================================
void VEntity::StopSoundSequence () {
  Super::StopSoundSequence(SoundOriginID);
}


//==========================================================================
//
//  VEntity::GetTouchedFloorSectorEx
//
//  used for 3d floors
//  can return `nullptr`
//  `orgsector` can be used to avoid BSP search (nullptr allowed)
//
//==========================================================================
sector_t *VEntity::GetTouchedFloorSectorEx (sector_t **swimmable) {
  if (swimmable) *swimmable = nullptr;
  if (!Sector) return nullptr;
  const float orgz = Origin.z;
  //if (Origin.z != FloorZ) return nullptr;
  if (!Sector->Has3DFloors()) {
    if (Sector->floor.GetPointZClamped(Origin) != orgz) return nullptr;
    return Sector;
  }
  sector_t *bestNonSolid = nullptr;
  sector_t *bestSolid = nullptr;
  float bestNSDist = 99999.0f;
  // check 3d floors
  for (sec_region_t *reg = Sector->eregions; reg; reg = reg->next) {
    if ((reg->regflags&(sec_region_t::RF_OnlyVisual|sec_region_t::RF_BaseRegion)) != 0) continue;
    if (!reg->extraline) continue;
    if (!reg->extraline->frontsector) continue;
    const float rtopz = reg->eceiling.GetPointZClamped(Origin);
    const float rbotz = reg->efloor.GetPointZClamped(Origin);
    // ignore paper-thin regions
    if (rtopz <= rbotz) continue; // invalid, or paper-thin, ignore
    if (reg->regflags&sec_region_t::RF_NonSolid) {
      // swimmable sector
      if (orgz > rbotz && orgz < rtopz) {
        // inside, check for best distance
        const float bdist = orgz-rbotz;
        const float tdist = rtopz-orgz;
        if (bdist < bestNSDist) {
          bestNSDist = bdist;
          bestNonSolid = reg->extraline->frontsector;
        } else if (tdist < bestNSDist) {
          bestNSDist = tdist;
          bestNonSolid = reg->extraline->frontsector;
        }
      }
    } else {
      // solid sector, check floor
      if (rtopz == orgz) {
        //return reg->extraline->frontsector;
        if (!bestSolid) bestSolid = reg->extraline->frontsector;
      }
    }
  }
  if (swimmable) {
    *swimmable = bestNonSolid;
  } else {
    // prefer swimmable
    if (bestNonSolid) return bestNonSolid;
  }
  return bestSolid;
}


//==========================================================================
//
//  VEntity::GetActorTerrain
//
//  cannot return `nullptr`
//
//==========================================================================
VTerrainInfo *VEntity::GetActorTerrain () {
  return SV_TerrainType((Sector ? EFloor.splane->pic.id : -1), IsPlayer());
}


//==========================================================================
//
//  Script natives
//
//==========================================================================
IMPLEMENT_FUNCTION(VEntity, GetActorTerrain) {
  vobjGetParamSelf();
  RET_PTR((Self ? Self->GetActorTerrain() : SV_TerrainType(-1, false)));
}

IMPLEMENT_FUNCTION(VEntity, GetTouchedFloorSector) {
  vobjGetParamSelf();
  RET_PTR((Self ? Self->GetTouchedFloorSectorEx(nullptr) : nullptr));
}

IMPLEMENT_FUNCTION(VEntity, GetTouchedFloorSectorEx) {
  sector_t **swimmable;
  vobjGetParamSelf(swimmable);
  RET_PTR((Self ? Self->GetTouchedFloorSectorEx(swimmable) : nullptr));
}

IMPLEMENT_FUNCTION(VEntity, SetTID) {
  P_GET_INT(tid);
  P_GET_SELF;
  Self->SetTID(tid);
}

IMPLEMENT_FUNCTION(VEntity, SetState) {
  P_GET_PTR(VState, state);
  P_GET_SELF;
  RET_BOOL(Self->SetState(state));
}

IMPLEMENT_FUNCTION(VEntity, SetInitialState) {
  P_GET_PTR(VState, state);
  P_GET_SELF;
  Self->SetInitialState(state);
}

IMPLEMENT_FUNCTION(VEntity, AdvanceState) {
  P_GET_FLOAT(deltaTime);
  P_GET_SELF;
  RET_BOOL(Self->AdvanceState(deltaTime));
}

IMPLEMENT_FUNCTION(VEntity, FindState) {
  P_GET_BOOL_OPT(Exact, false);
  P_GET_NAME_OPT(SubLabel, NAME_None);
  P_GET_NAME(StateName);
  P_GET_SELF;
  RET_PTR(Self->FindState(StateName, SubLabel, Exact));
}

IMPLEMENT_FUNCTION(VEntity, FindStateEx) {
  P_GET_BOOL_OPT(Exact, false);
  P_GET_STR(StateName);
  P_GET_SELF;
  RET_PTR(Self->FindStateEx(StateName, Exact));
}

IMPLEMENT_FUNCTION(VEntity, HasSpecialStates) {
  P_GET_NAME(StateName);
  P_GET_SELF;
  RET_BOOL(Self->HasSpecialStates(StateName));
}

IMPLEMENT_FUNCTION(VEntity, GetStateEffects) {
  P_GET_PTR(TArray<VParticleEffectDef *>, Part);
  P_GET_PTR(TArray<VLightEffectDef *>, Lights);
  P_GET_SELF;
  Self->GetStateEffects(*Lights, *Part);
}

IMPLEMENT_FUNCTION(VEntity, HasAnyLightEffects) {
  P_GET_SELF;
  RET_BOOL(Self->HasAnyLightEffects());
}

IMPLEMENT_FUNCTION(VEntity, CallStateChain) {
  P_GET_PTR(VState, State);
  P_GET_REF(VEntity, Actor);
  P_GET_SELF;
  RET_BOOL(Self->CallStateChain(Actor, State));
}

//native final void PlaySound (name SoundName, int Channel, optional float Volume,
//                             optional float Atenuation, optional bool Loop, optional bool Local);
IMPLEMENT_FUNCTION(VEntity, PlaySound) {
  VName SoundName;
  int Channel;
  VOptParamFloat Volume(1.0f);
  VOptParamFloat Attenuation(1.0f);
  VOptParamBool Loop(false);
  VOptParamBool Local(false);
  vobjGetParamSelf(SoundName, Channel, Volume, Attenuation, Loop, Local);
  if (Channel&256) Loop = true; // sorry for this magic number
  Channel &= 7; // other bits are flags
  Self->StartSound(SoundName, Channel, Volume, Attenuation, Loop, Local);
}

IMPLEMENT_FUNCTION(VEntity, StopSound) {
  P_GET_INT(Channel);
  P_GET_SELF;
  Self->StopSound(Channel);
}

IMPLEMENT_FUNCTION(VEntity, AreSoundsEquivalent) {
  P_GET_NAME(Sound2);
  P_GET_NAME(Sound1);
  P_GET_SELF;
  RET_BOOL(GSoundManager->ResolveEntitySound(Self->SoundClass,
    Self->SoundGender, Sound1) == GSoundManager->ResolveEntitySound(
    Self->SoundClass, Self->SoundGender, Sound2));
}

IMPLEMENT_FUNCTION(VEntity, IsSoundPresent) {
  P_GET_NAME(Sound);
  P_GET_SELF;
  RET_BOOL(GSoundManager->IsSoundPresent(Self->SoundClass, Self->SoundGender, Sound));
}

IMPLEMENT_FUNCTION(VEntity, StartSoundSequence) {
  P_GET_INT(ModeNum);
  P_GET_NAME(Name);
  P_GET_SELF;
  Self->StartSoundSequence(Name, ModeNum);
}

IMPLEMENT_FUNCTION(VEntity, AddSoundSequenceChoice) {
  P_GET_NAME(Choice);
  P_GET_SELF;
  Self->AddSoundSequenceChoice(Choice);
}

IMPLEMENT_FUNCTION(VEntity, StopSoundSequence) {
  P_GET_SELF;
  Self->StopSoundSequence();
}

IMPLEMENT_FUNCTION(VEntity, SetDecorateFlag) {
  P_GET_BOOL(Value);
  P_GET_STR(Name);
  P_GET_SELF;
  Self->SetDecorateFlag(Name, Value);
}


IMPLEMENT_FUNCTION(VEntity, GetDecorateFlag) {
  P_GET_STR(Name);
  P_GET_SELF;
  RET_BOOL(Self->GetDecorateFlag(Name));
}

// native final int CalcLight (optional int flags/*=0*/);
IMPLEMENT_FUNCTION(VEntity, CalcLight) {
  VOptParamInt flags(0);
  vobjGetParamSelf(flags);
  RET_INT(Self->XLevel->CalcEntityLight(Self, (unsigned)flags.value));
}


// native final void QS_PutInt (name fieldname, int value);
IMPLEMENT_FUNCTION(VEntity, QS_PutInt) {
  P_GET_INT(value);
  P_GET_STR(name);
  P_GET_SELF;
  QS_PutValue(QSValue::CreateInt(Self, name, value));
}

// native final void QS_PutName (name fieldname, name value);
IMPLEMENT_FUNCTION(VEntity, QS_PutName) {
  P_GET_NAME(value);
  P_GET_STR(name);
  P_GET_SELF;
  QS_PutValue(QSValue::CreateName(Self, name, value));
}

// native final void QS_PutStr (name fieldname, string value);
IMPLEMENT_FUNCTION(VEntity, QS_PutStr) {
  P_GET_STR(value);
  P_GET_STR(name);
  P_GET_SELF;
  QS_PutValue(QSValue::CreateStr(Self, name, value));
}

// native final void QS_PutFloat (name fieldname, float value);
IMPLEMENT_FUNCTION(VEntity, QS_PutFloat) {
  P_GET_FLOAT(value);
  P_GET_STR(name);
  P_GET_SELF;
  QS_PutValue(QSValue::CreateFloat(Self, name, value));
}


// native final int QS_GetInt (name fieldname, optional int defvalue);
IMPLEMENT_FUNCTION(VEntity, QS_GetInt) {
  P_GET_INT_OPT(value, 0);
  P_GET_STR(name);
  P_GET_SELF;
  QSValue ret = QS_GetValue(Self, name);
  if (ret.type != QSType::QST_Int) {
    if (!specified_value) Host_Error("value '%s' not found for '%s'", *name, Self->GetClass()->GetName());
    ret.ival = value;
  }
  RET_INT(ret.ival);
}

// native final name QS_GetName (name fieldname, optional name defvalue);
IMPLEMENT_FUNCTION(VEntity, QS_GetName) {
  P_GET_NAME_OPT(value, NAME_None);
  P_GET_STR(name);
  P_GET_SELF;
  QSValue ret = QS_GetValue(Self, name);
  if (ret.type != QSType::QST_Name) {
    if (!specified_value) Host_Error("value '%s' not found for '%s'", *name, Self->GetClass()->GetName());
    ret.nval = value;
  }
  RET_NAME(ret.nval);
}

// native final string QS_GetStr (name fieldname, optional string defvalue);
IMPLEMENT_FUNCTION(VEntity, QS_GetStr) {
  P_GET_STR_OPT(value, VStr::EmptyString);
  P_GET_STR(name);
  P_GET_SELF;
  QSValue ret = QS_GetValue(Self, name);
  if (ret.type != QSType::QST_Str) {
    if (!specified_value) Host_Error("value '%s' not found for '%s'", *name, Self->GetClass()->GetName());
    ret.sval = value;
  }
  RET_STR(ret.sval);
}

// native final float QS_GetFloat (name fieldname, optional float defvalue);
IMPLEMENT_FUNCTION(VEntity, QS_GetFloat) {
  P_GET_FLOAT_OPT(value, 0.0f);
  P_GET_STR(name);
  P_GET_SELF;
  QSValue ret = QS_GetValue(Self, name);
  if (ret.type != QSType::QST_Float) {
    if (!specified_value) Host_Error("value '%s' not found for '%s'", *name, Self->GetClass()->GetName());
    ret.fval = value;
  }
  RET_FLOAT(ret.fval);
}
