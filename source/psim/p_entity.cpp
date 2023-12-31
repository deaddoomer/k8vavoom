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
#include "../gamedefs.h"
#include "../render/r_public.h"
#include "../server/sv_local.h"
#include "../sound/sound.h"
#include "p_entity.h"
#include "p_levelinfo.h"
#include "p_player.h"
#include "../utils/qs_data.h"

//#define VV_SETSTATE_DEBUG

//#define VV_DEBUG_CSL_VERBOSE

#ifdef VV_SETSTATE_DEBUG
# define VSLOGF(...)  GCon->Logf(NAME_Debug, __VA_ARGS__)
#else
# define VSLOGF(...)  do {} while (0)
#endif


// roughly smaller than lowest fixed point 16.16 (it is more like 0.0000152587890625)
#define PHYS_MIN_SPEED  (0.000016f*2.0f)


IMPLEMENT_CLASS(V, Entity);


// ////////////////////////////////////////////////////////////////////////// //
//static VCvarB _decorate_dont_warn_about_invalid_labels("decorate_dont_warn_about_invalid_labels", false, "Don't do this!", CVAR_Archive|CVAR_PreInit|CVAR_Hidden|CVAR_NoShadow);
static VCvarB dbg_disable_state_advance("dbg_disable_state_advance", false, "Disable states processing (for debug)?", CVAR_PreInit|CVAR_NoShadow);

static VCvarB dbg_emulate_broken_gozzo_gotos("dbg_emulate_broken_gozzo_gotos", false, "Emulate (partially) broken GZDoom decorate gotos to missing labels?", CVAR_Archive|CVAR_NoShadow);

static VCvarB vm_optimise_statics("vm_optimise_statics", true, "Try to detect some static things, and don't run physics for them? This greatly speed ups the game for huge number of entities.", CVAR_Archive|CVAR_NoShadow);

extern VCvarB dbg_vm_show_tick_stats;

static VCvarB gm_slide_bodies("gm_slide_bodies", true, "Slide bodies hanging from ledges and such?", CVAR_Archive);
static VCvarB gm_slide_drop_items("gm_slide_drop_items", true, "Slide dropped items hanging from ledges and such?", CVAR_Archive);
static VCvarF gm_slide_time_limit("gm_slide_time_limit", "60", "Slide time limit, in seconds?", CVAR_Archive);
static VCvarB gm_corpse_slidemove("gm_corpse_slidemove", true, "Should corpses use sliding movement?", CVAR_Archive);

// k8gore cvars
VCvarB k8gore_enabled("k8GoreOpt_Enabled", true, "Enable extra blood and gore?", CVAR_Archive);
VCvarI k8gore_enabled_override("k8GoreOpt_Enabled_Override", "0", "-1: disable; 0: default; 1: enable.", CVAR_Hidden|CVAR_NoShadow);
VCvarI k8gore_enabled_override_decal("k8GoreOpt_Enabled_Override_Decals", "0", "-1: disable; 0: default; 1: enable.", CVAR_Hidden|CVAR_NoShadow);

static VCvarB k8gore_force_colored_blood("k8GoreOpt_ForceColoredBlood", true, "Force colored blood for Bruisers and Cacos?", CVAR_Archive);
static VCvarS k8gore_bruiser_blood_color("k8GoreOpt_BloodColorBruiser", "00 40 00", "Blood color for Hell Knight and Baron of Hell.", CVAR_Archive);
static VCvarS k8gore_cacodemon_blood_color("k8GoreOpt_BloodColorCacodemon", "00 00 40", "Blood color for Cacodemon.", CVAR_Archive);

static ColorCV BruiserBloodColor(&k8gore_bruiser_blood_color, nullptr, true); // allow "no color"
static ColorCV CacoBloodColor(&k8gore_cacodemon_blood_color, nullptr, true); // allow "no color"

static VCvarB k8GoreOpt_CeilBlood("k8GoreOpt_CeilBlood", true, "Spawn blood splats on ceiling?", CVAR_Archive);
static VCvarB k8GoreOpt_CeilBloodDrip("k8GoreOpt_CeilBloodDrip", true, "Should ceiling blood spots drip some blood?", CVAR_Archive);
static VCvarB k8GoreOpt_FloorBlood("k8GoreOpt_FloorBlood", true, "Spawn blood splats on floor?", CVAR_Archive);
static VCvarB k8GoreOpt_FlatDecals("k8GoreOpt_FlatDecals", true, "Use decals instead of sprites for floor/ceiling blood?", CVAR_Archive);
static VCvarB k8GoreOpt_ExtraFlatDecals("k8GoreOpt_ExtraFlatDecals", true, "Add more floor/ceiling decals?", CVAR_Archive);
static VCvarI k8GoreOpt_BloodAmount("k8GoreOpt_BloodAmount", "3", "Blood amount: [0..3] (some, normal, a lot, bloodbath).", CVAR_Archive);
static VCvarI k8GoreOpt_MaxBloodEntities("k8GoreOpt_MaxBloodEntities", "3500", "Limit number of spawned blood actors. It is recommended to keep this under 4000.", CVAR_Archive);
static VCvarI k8GoreOpt_MaxBloodSplatEntities("k8GoreOpt_MaxBloodSplatEntities", "2200", "Limit number of spawned blood splats.", CVAR_Archive);
static VCvarI k8GoreOpt_MaxCeilBloodSplatEntities("k8GoreOpt_MaxCeilBloodSplatEntities", "250", "Limit number of spawned ceiling blood splats sprites.", CVAR_Archive);
static VCvarI k8GoreOpt_MaxTransientBloodEntities("k8GoreOpt_MaxTransientBloodEntities", "620", "Limit number of spawned transient (temporary) blood actors.", CVAR_Archive);
static VCvarB k8GoreOpt_BloodDripSound("k8GoreOpt_BloodDripSound", true, "Play small splashes when flying/dropping blood landed?", CVAR_Archive);
static VCvarB k8GoreOpt_HeadshotSound("k8GoreOpt_HeadshotSound", false, "Play special sound on headshot kill?", CVAR_Archive);


// ////////////////////////////////////////////////////////////////////////// //
VClass *VEntity::clsInventory = nullptr;

static VClass *clsBaronOfHell = nullptr;
static VClass *clsHellKnight = nullptr;
static VClass *clsCacodemon = nullptr;
static VClass *clsBlood = nullptr;

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
static VField *fldcBloodType = nullptr;

static VClass *classActor = nullptr;


// ////////////////////////////////////////////////////////////////////////// //
struct SavedVObjectPtr {
public:
  VObject **ptr;
  VObject *saved;
public:
  VV_DISABLE_COPY(SavedVObjectPtr)
  VVA_FORCEINLINE SavedVObjectPtr (VObject **aptr) noexcept : ptr(aptr), saved(*aptr) {}
  VVA_FORCEINLINE ~SavedVObjectPtr () noexcept { *ptr = saved; }
};


struct PCSaver {
public:
  VStateCall **ptr;
  VStateCall *PrevCall;
public:
  VV_DISABLE_COPY(PCSaver)
  VVA_FORCEINLINE PCSaver (VStateCall **aptr) noexcept : ptr(aptr), PrevCall(nullptr) { if (aptr) PrevCall = *aptr; }
  VVA_FORCEINLINE ~PCSaver () noexcept { if (ptr) *ptr = PrevCall; ptr = nullptr; }
};


// ////////////////////////////////////////////////////////////////////////// //
struct SetStateGuard {
public:
  VEntity *ent;
public:
  VV_DISABLE_COPY(SetStateGuard)
  // constructor increases invocation count
  VVA_FORCEINLINE SetStateGuard (VEntity *aent) noexcept : ent(aent) { aent->setStateWatchCat = 0; }
  VVA_FORCEINLINE ~SetStateGuard () noexcept { ent->setStateWatchCat = 0; }
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

  // they may be absent, it is ok
  clsBaronOfHell = VClass::FindClass("BaronOfHell");
  clsHellKnight = VClass::FindClass("HellKnight");
  clsCacodemon = VClass::FindClass("Cacodemon");
  clsBlood = VClass::FindClass("Blood");

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
  fldcBloodType = FindTypedField(classEntityEx, "BloodType", TYPE_Class);

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
//  VEntity::CheckForceColoredBlood
//
//  called from `VLevel::SpawnThinker()`
//
//==========================================================================
void VEntity::CheckForceColoredBlood () {
  if (!k8gore_force_colored_blood.asBool()) return;
  if (!IsMonster()) return;
  if (!clsBlood) return; // just in case

  // check for the existing blood translation
  int bcolor = fldiBloodColor->GetInt(this);
  if (bcolor) return; // already translated

  VClass *bt = (VClass *)fldcBloodType->GetObjectValue(this);
  if (bt && bt != clsBlood) return; // not a default blood class, do nothing

  // check class
  int ctrans = 0;
  for (const VClass *c = GetClass(); c; c = c->ParentClass) {
    if (c == classActor || c == classEntityEx) return;
    if (c == clsHellKnight || c == clsBaronOfHell) {
      ctrans = BruiserBloodColor.getColor();
      if (!ctrans) return; // "no color"
      ctrans |= 0xff000000; // just in case
      break;
    }
    if (c == clsCacodemon) {
      ctrans = CacoBloodColor.getColor();
      if (!ctrans) return; // "no color"
      ctrans |= 0xff000000; // just in case
      break;
    }
  }
  if (!ctrans) return;

  fldiBloodColor->SetInt(this, ctrans);
  int btrans = R_GetBloodTranslation(ctrans, true/*allowAdd*/);
  fldiBloodTranslation->SetInt(this, btrans);
  //ee.BloodSplatterType = K8Gore_Blood_SplatterReplacer;
}


#if 0
#define EXTRA_DATA_SIGNATURE  "EntityExtra0"

//==========================================================================
//
//  VEntity::LoadExtraData
//
//  returns `false` if no extra data was found (stream need to be restored)
//
//==========================================================================
void VEntity::LoadExtraData (VStream &Strm) {
  //k8: i am dumb, so i had to write this fuckery to detect if we have extra data
  if (Strm.TotalSize()-opos <= 3*8) return false; // there cannot be any extra data here
  vuint32 sign0 = 0, sign1 = 0, sign2 = 0;
  Strm << sign0 << sign1 << sign2;
  if (Strm.IsError()) return false; // oops, it's dead anyway
  // check hashes
  if (!sign0) return; // it cannot be zero
  const char *esign = EXTRA_DATA_SIGNATURE;
  const size_t esignelen = strlen(esign);
  // check first hash
  XXH32_state_t xx32;
  XXH32_reset(&xx32, sign0);
  XXH32_update(&xx32, esign, esignelen);
  if (XXH32_digest(&xx32) != sign1) return false; // invalid first hash
  // first hash is ok, try second
  if (bjHashBuf(esign, esignelen, sign1) != sign2) return false; // invalid second hash
  GCon->Logf(NAME_Debug, "%s: EXTRA DATA!", GetClass()->GetName());
  return true;
}


//==========================================================================
//
//  VEntity::SaveExtraData
//
//==========================================================================
void VEntity::SaveExtraData (VStream &Strm) {
  vuint32 sign0, sign1, sign2;
  sign0 = joaatHashBuf((uint32_t)(unitptr_t)this, 4);
  sign0 += !sign0;
  // first hash
  XXH32_state_t xx32;
  XXH32_reset(&xx32, sign0);
  XXH32_update(&xx32, esign, esignelen);
  sign1 = XXH32_digest(&xx32);
  // second hash
  sign2 = bjHashBuf(esign, esignelen, sign1);
  // write them
  Strm << sign0 << sign1 << sign2;
  // write extra data
}
#endif


//==========================================================================
//
//  VEntity::SerialiseOther
//
//==========================================================================
void VEntity::SerialiseOther (VStream &Strm) {
  Super::SerialiseOther(Strm);
  if (Strm.IsLoading()) {
    // loading
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
    // restore sprite (because sprite index can change)
    {
      int sidx = 1;
      if (DispSpriteName != NAME_None) sidx = VClass::FindAddSprite(DispSpriteName);
      DispSpriteFrame = (DispSpriteFrame&~0x00ffffff)|(sidx&0x00ffffff);
    }
    // restore "gore mod" flag
    if (VStr::startsWith(GetClass()->GetName(), "K8Gore_")) {
      FlagsEx |= EFEX_GoreModEntity;
    } else {
      FlagsEx &= ~EFEX_GoreModEntity;
    }
    #ifdef EXTRA_DATA_SIGNATURE
    not used
    // has other data?
    auto opos = Strm.Tell();
    if (!LoadExtraData(Strm)) {
      // no extra data, restore stream
      Strm.Seek(opos);
    }
    #endif
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
  } else {
    // saving
    #ifdef EXTRA_DATA_SIGNATURE
    not used
    SaveExtraData();
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
  // FIXME: this won't work right in network games: server has it's own sound ids, and
  // those ids are used to send sound events. this may replace some valid entity, and
  // then the server will send sound id correction; so, old valid id will be orphaned.
  // this is not fatal, but may produce sound anomalies.
  if (!XLevel->NextSoundOriginID) XLevel->NextSoundOriginID = 1;
  SoundOriginID = XLevel->NextSoundOriginID+(SNDORG_Entity<<24);
  XLevel->NextSoundOriginID = (XLevel->NextSoundOriginID+1)&0x00ffffff;
  XLevel->RegisterEntitySoundID(this, SoundOriginID);
  //GCon->Logf(NAME_Debug, "VEntity(%s)::AddedToLevel: uid=%u; ptr=%p; sid=%d", GetClass()->GetName(), GetUniqueId(), (void*)this, SoundOriginID);
}


//==========================================================================
//
//  VEntity::RemovedFromLevel
//
//==========================================================================
void VEntity::RemovedFromLevel () {
  //GCon->Logf(NAME_Debug, "VEntity(%s)::RemovedFromLevel: uid=%u; ptr=%p; sid=%d", GetClass()->GetName(), GetUniqueId(), (void*)this, SoundOriginID);
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


// ////////////////////////////////////////////////////////////////////////// //
// corpse sliding debug code
// sorry for globals!
//

// collected points
static TArrayNC<TVec> stxPoints;
// final convex hull
static TArrayNC<TVec> stxHullOrig;
static TArrayNC<TVec> stxHull;
// planes for each hull edge
// always contains the plane for [$-1] to [0] edge
static TArrayNC<TPlane> stxHullPlanes;
// kept static to avoid needless reallocations
static TPlane::ClipWorkData stxWk;


//==========================================================================
//
//  PtSide
//
//  this is a magnitude of the cross product of two 2D vectors (yes, it sounds funny)
//  it is also a determinant of two vectors
//  and it is proprotional to (signed) triangle area
//
//==========================================================================
static VVA_OKUNUSED VVA_FORCEINLINE float PtSide (const TVec &a, const TVec &b, const TVec &p) {
  return (b.x - a.x)*(p.y - a.y) - (b.y - a.y)*(p.x - a.x);
}


//==========================================================================
//
//  bchTVecCompare
//
//  used to sort points for monotone chain algo
//
//==========================================================================
static int bchTVecCompare (const void *aa, const void *bb, void *udata) noexcept {
  const TVec &a = *(const TVec *)aa;
  const TVec &b = *(const TVec *)bb;
  const float diffx = a.x - b.x;
  if (diffx != 0.0f) return (diffx < 0.0f ? -1 : +1);
  const float diffy = a.y - b.y;
  return (diffy < 0.0f ? -1 : diffy > 0.0f ? +1 : 0);
}


//==========================================================================
//
//  buildConvexHull
//
//  uses Andrew's Monotone Chain algo
//  faster and more stable that gift wrapping
//
//==========================================================================
static VVA_OKUNUSED void buildConvexHull () {
  const float hullEps = 0.01f;

  const int ptlen = stxPoints.length();
  int k = 0, t;

  stxHull.resetNoDtor();
  stxHullOrig.resetNoDtor();

  // first, sort points
  smsort_r(stxPoints.ptr(), (size_t)ptlen, sizeof(stxPoints[0]), &bchTVecCompare, nullptr);

  // build lower hull
  for (int f = 0; f < ptlen; f += 1) {
    while (k >= 2 && PtSide(stxHull[k-2], stxHull[k-1], stxPoints[f]) >= -hullEps) {
      stxHull.drop(); k -= 1;
    }
    stxHull.Append(stxPoints[f]); k += 1;
  }

  // build upper hull
  t = k + 1;
  for (int f = ptlen - 1; f > 0; f -= 1) {
    while (k >= t && PtSide(stxHull[k-2], stxHull[k-1], stxPoints[f-1]) >= -hullEps) {
      stxHull.drop(); k -= 1;
    }
    stxHull.Append(stxPoints[f-1]); k += 1;
  }

  if (k) stxHull.drop();
}


//==========================================================================
//
//  PointInHull
//
//==========================================================================
static VVA_OKUNUSED bool PointInHull (const TVec morg) {
  bool inside = false;
  const int hlen = stxHull.length();
  if (hlen > 2) {
    int c = hlen - 1;
    for (int f = 0; f < hlen; c = f, f += 1) {
      // c is prev
      const TVec pf = stxHull[c];
      const TVec pc = stxHull[f];
      if (((pf.y > morg.y) != (pc.y > morg.y)) &&
          (morg.x < (pc.x-pf.x) * (morg.y-pf.y) / (pc.y-pf.y) + pf.x))
      {
        inside = !inside;
      }
    }
  }
  return inside;
}


//==========================================================================
//
//  CheckGoodStanding
//
//  check all regions, return `true` if at least one region contains
//
//==========================================================================
static bool CheckGoodStanding (VEntity *mobj, sector_t *sector, TVec morg) {
  if (sector) {
    if (sector->floor.GetPointZClamped(morg) == morg.z) return true;
    else if (sector->Has3DFloors()) {
      for (const sec_region_t *reg = sector->eregions->next; reg; reg = reg->next) {
        if (reg->regflags&(sec_region_t::RF_BaseRegion|sec_region_t::RF_OnlyVisual|sec_region_t::RF_NonSolid)) continue;
        // get opening points
        const float fz = reg->efloor.GetPointZClamped(morg);
        const float cz = reg->eceiling.GetPointZClamped(morg);
        if (fz >= cz) continue; // ignore paper-thin regions
        #if 0
        GCon->Logf(NAME_Debug, "%s(%u): fz=%g; cz=%g; z=%g; hit=%d",
                   mobj->GetClass()->GetName(), mobj->GetUniqueId(),
                   fz, cz, morg.z, (cz == morg.z));
        #endif
        // we are standing on 3d floor ceilings
        if (cz == morg.z) return true;
      }
    }
  }
  return false;
}


//==========================================================================
//
//  checkStationary
//
//  as we know, the body "lies stable" when convex hull created from all
//  contact points contains body's center of mass. so let's collect all
//  points from touching subsectors, and do exactly that check.
//
//  previous version of the code was clipping each seg against the mobj
//  bounding box, and sorted out duplicate points. this was required
//  because convex hull creation algo was unable to cope with duplicates,
//  and because i wrote it that way.
//
//  as we know from our geometry classes, clipping convex poly with another
//  convex poly gives us a convex poly. so we can simply collect all points,
//  build convex hull, and then clip that hull with mobj bbox. it will be
//  faster this way. don't bother applying Akl–Toussaint heuristic, the
//  price of quadrilateral checing doesn't worth the efforts.
//
//  also, instead of using "point in poly" check (several, actually), we
//  can calculate planes for convex hull edges, and use "box in convex"
//  check. as we're actually checking if small box is in hull, this will
//  give us a small speedup.
//
//==========================================================================
static VVA_OKUNUSED bool checkStationary (VEntity *mobj) {
  float mybbox[4];
  VLevel *XLevel = mobj->XLevel;
  TVec morg = mobj->Origin;
  const float rad = mobj->GetMoveRadius();
  if (rad < 2.0f) return true; // why bother?

  mobj->Create2DBox(mybbox);

  // there is no need to reset "inner rect"
  stxPoints.resetNoDtor();
  // just in case
  stxHull.resetNoDtor();

  // optimisation: if we hit only one subsector, don't bother with
  // hull building: subsectors are always convex
  int ssCount = 0;

  //GCon->Log(NAME_Debug, ":::::::::::::::::::::");
  // check all touching sectors
  for (msecnode_t *mnode = mobj->TouchingSectorList; mnode; mnode = mnode->TNext) {
    // if this sector doesn't have any regions we could stand on, ignore it
    if (!CheckGoodStanding(mobj, mnode->Sector, morg)) continue;
    // check all subsectors
    for (subsector_t *sub = mnode->Sector->subsectors; sub; sub = sub->seclink) {
      // this is fast, because it is using subsector bounding box, and multiplies only
      if (!XLevel->IsBBox2DTouchingSubsector(sub, mybbox)) continue;
      // just append all subsector points
      const seg_t *seg = &XLevel->Segs[sub->firstline];
      for (int f = sub->numlines; f--; ++seg) {
        stxPoints.append(TVec(seg->v1->x, seg->v1->y));
      }
      ssCount += 1;
    }
  }

  // we're done collecting points.
  // don't do anything if we don't have enough points to build even a triangle.
  // assume "stationary body" in this case, because we
  // don't have a hull to calculate a sliding vector.
  if (stxPoints.length() < 3) return true;

  // build convex hull
  if (ssCount > 1) {
    buildConvexHull();
  } else {
    // just use original subsector points
    // for maps with moderate/low details, this is often the case
    const int slen = stxPoints.length();
    for (int f = 0; f < slen; f += 1) stxHull.Append(stxPoints[f]);
  }

  // now clip the hull
  int fc = stxHull.length();

  stxHullOrig.resetNoDtor();
  for (int f = 0; f < fc; f += 1) stxHullOrig.Append(stxHull[f]);

  TPlane clipPlane;
  const int boxCorners[4][4] = {
    // left edge
    { BOX2D_MINX, BOX2D_MINY, BOX2D_MINX, BOX2D_MAXY },
    // top edge
    { BOX2D_MINX, BOX2D_MAXY, BOX2D_MAXX, BOX2D_MAXY },
    // right edge
    { BOX2D_MAXX, BOX2D_MAXY, BOX2D_MAXX, BOX2D_MINY },
    // bottom edge
    { BOX2D_MAXX, BOX2D_MINY, BOX2D_MINX, BOX2D_MINY },
  };
  for (int edge = 0; fc >= 3 && edge < 4; edge += 1) {
    clipPlane.Set2Points(TVec(mybbox[boxCorners[edge][0]], mybbox[boxCorners[edge][1]]),
                         TVec(mybbox[boxCorners[edge][2]], mybbox[boxCorners[edge][3]]));
    #if 0
    GCon->Logf(NAME_Debug, "edge: #%d", edge);
    GCon->Logf(NAME_Debug, "  box: (%g,%g)-(%g, %g)",
               mybbox[boxCorners[edge][0]], mybbox[boxCorners[edge][1]],
               mybbox[boxCorners[edge][2]], mybbox[boxCorners[edge][3]]);
    GCon->Logf(NAME_Debug, "  hull: %d points", fc);
    for (int f = 0; f < fc; f += 1) {
      GCon->Logf(NAME_Debug, "    #%d: (%g, %g, %g)", f,
                 stxHull[f].x, stxHull[f].y, stxHull[f].z);
    }
    #endif
    // always should have room for one more point, it is enough for convex polys
    // but let's play safe, and reserve more (it is WAY more than needed, but meh)
    const int newsz = fc + 128;
    stxHull.setLengthNoResize(newsz);
    clipPlane.ClipPolyFront(stxWk, stxHull.ptr(), fc, stxHull.ptr(), &fc);
    vassert(fc <= newsz);
  }

  // don't do anything if we don't have enough points to build even a triangle.
  // assume "stationary body" in this case, because we
  // don't have a hull to calculate a sliding vector.
  if (fc < 3) {
    stxHull.resetNoDtor();
    return true;
  }
  stxHull.setLengthNoResize(fc);

  // calculate planes for hull edges
  // we need plane normals to point inside a hull for "box in poly" checks.
  stxHullPlanes.setLengthNoResize(fc);
  for (int f = 0; f < fc - 1; f += 1) {
    //stxHullPlanes[f].Set2Points(stxHull[f + 1], stxHull[f]);
    stxHullPlanes[f].Set2Points(stxHull[f], stxHull[f + 1]);
  }
  //stxHullPlanes[fc - 1].Set2Points(stxHull[0], stxHull[fc - 1]);
  stxHullPlanes[fc - 1].Set2Points(stxHull[fc - 1], stxHull[0]);

  // check if a small box arount the center of the mass is inside a hull
  const float nofs = (rad > 6.0f ? 2.0f : 1.0f);
  mybbox[BOX2D_MINX] = morg.x - nofs;
  mybbox[BOX2D_MINY] = morg.y - nofs;
  mybbox[BOX2D_MAXX] = morg.x + nofs;
  mybbox[BOX2D_MAXY] = morg.y + nofs;

  bool isInside = true;
  for (int f = 0; isInside && f < fc; f += 1) {
    const TPlane &pl = stxHullPlanes[f];
    if (pl.normal.dot2D(pl.get2DBBoxAcceptPoint(mybbox)) < pl.dist) {
      isInside = false;
    }
  }

  #if 0
  GCon->Logf(NAME_Debug, "=== pts:%d; hull:%d; inside=%d ===", stxPoints.length(), stxHull.length(), inside);
  #endif
  return isInside;
}


//==========================================================================
//
//  CalculateSlideUnitVector
//
//  move by the vector from the center of hull to origin
//
//==========================================================================
static VVA_OKUNUSED TVec CalculateSlideUnitVector (VEntity *mobj) {
  TVec snorm, hc;

  const int hlen = stxHull.length();
  vassert(hlen >= 3);

  // calculate hull centroid: average of all hull vertices
  // our hull has no collinear points, so it is ok

  hc = TVec(0.0f, 0.0f, 0.0f);
  for (int f = 0; f < hlen; f += 1) hc += stxHull[f];
  hc /= hlen;

  // vector from hc to morg is our new direction
  snorm = mobj->Origin - hc;
  snorm.z = 0.0f;
  if (!snorm.isZero2D()) snorm.normalise2DInPlace();

  return snorm;
}


//==========================================================================
//
//  CorpseIdle
//
//==========================================================================
static VVA_FORCEINLINE void CorpseIdle (VEntity *mobj) {
  mobj->cslFlags &= ~(VEntity::CSL_CorpseSliding|VEntity::CSL_CorpseSlidingSlope);
  mobj->cslCheckDelay = -1.0f;
  mobj->cslStartTime = 0.2f;
  //mobj->cslLastSub = nullptr;
}


//==========================================================================
//
//  VEntity::CorpseSlide
//
//  "corpse check" should be already done
//
//  `cslCheckDelay` is zero or negative here
//
//==========================================================================
void VEntity::CorpseSlide (float deltaTime) {
  tmtrace_t tm;
  TVec snorm;

  // if on pobj, do nothing (yet)
  //if (Sector != BaseSector) { CorpseIdle(this); return; }
  if (Sector->isAnyPObj()) { CorpseIdle(this); return; }

  // default timeout; nudge rougly each two ticks
  cslCheckDelay = 0.06f;

  // `checkStationary()` is fast enough to avoid other checks
  if (!checkStationary(this)) {
    // this corpse is not in a "stable" position (i.e. should slide away)
    // we have built convex hull here too
    #ifdef VV_DEBUG_CSL_VERBOSE
    CheckRelPositionPoint(tm, Origin);
    GCon->Logf("CORPSE HANGING: %s(%u): fz=%g; oz=%g", GetClass()->GetName(),
               GetUniqueId(), tm.FloorZ, Origin.z);
    #endif

    cslFlags &= ~CSL_CorpseSlidingSlope;

    snorm = CalculateSlideUnitVector(this);

    if (!snorm.isZero2D()) {
      if (cslFlags&CSL_CorpseSliding) {
        // reset starting time when Z was changed
        if (cslLastPos.z != Origin.z) {
          cslStartTime = XLevel->Time;
        } else if ((cslFlags&CSL_CorpseWasNudged) != 0 && cslLastPos == Origin) {
          // if cannot move since last check, it means that it stuck
          if (cslStartTime > 0.0f) cslStartTime = 0.0f;
          cslStartTime -= deltaTime;
          if (cslStartTime < -2.666f) {
            CorpseIdle(this);
            return;
          }
        }
      } else {
        // remember sliding start time
        cslStartTime = XLevel->Time;
        cslFlags |= CSL_CorpseSliding;
      }
      cslLastPos = Origin;
          //cslFlags |= CSL_CorpseWasNudged;

      // if slided for too long, don't bother anymore
      const float slm = gm_slide_time_limit.asFloat();
      if (slm > 0.0f && cslStartTime > 0.0f && XLevel->Time - cslStartTime > slm) {
        CorpseIdle(this);
        #ifdef VV_DEBUG_CSL_VERBOSE
        GCon->Logf("CORPSE IDLE: %s(%u): tm=%g", GetClass()->GetName(),
                   GetUniqueId(), XLevel->Time - cslStartTime);
        #endif
      } else {
        //const float speed = 15.0f;
        const float speed = 12.0f + 6.0f*((hashU32(GetUniqueId())>>4)&0x3fu)/63.0f;
        #if defined(VV_DEBUG_CSL_VERBOSE) || 0
        GCon->Logf("CC(%u):%s: snorm:(%g,%g,%g):%g; angle:%g; xspeed:%g (%g)",
                   GetUniqueId(), GetClass()->GetName(),
                   snorm.x, snorm.y, snorm.z, snorm.length2D(),
                   AngleMod360(VectorAngleYaw(snorm)),
                   Velocity.length2D(), speed);
        #endif
        // don't change velocity each time, it doesn't look good
        if (cslFlags&CSL_CorpseWasNudged) {
          if (Velocity.length2DSquared() < 10.0f*10.0f) {
            Velocity.x = snorm.x*speed;
            Velocity.y = snorm.y*speed;
            #if defined(VV_DEBUG_CSL_VERBOSE) || 0
            GCon->Logf("  finspeed(0):%g", Velocity.length2D());
            #endif
          }
        } else if (Velocity.length2DSquared() < 3.0f*3.0f) {
          // nudged for the first time
          cslFlags |= CSL_CorpseWasNudged;
          Velocity.x = snorm.x*speed;
          Velocity.y = snorm.y*speed;
          #if defined(VV_DEBUG_CSL_VERBOSE) || 0
          GCon->Logf("  finspeed(1):%g", Velocity.length2D());
          #endif
        }
      }
    } else {
      // no slide direction; alas, freeze it (but only if it doesn't move)
      #ifdef VV_DEBUG_CSL_VERBOSE
      GCon->Logf("CC(%u):%s: NO SLIDE DIR; freezing; snorm:(%g,%g,%g)",
                 GetUniqueId(), GetClass()->GetName(),
                 snorm.x, snorm.y, snorm.z);
      #endif
      if (Velocity.length2DSquared() < 3.0f*3.0f) {
        CorpseIdle(this);
      }
    }
  } else {
    // the corpse is stable, but may still move, or lie on a slope
    #ifdef VV_DEBUG_CSL_VERBOSE
    CheckRelPositionPoint(tm, Origin);
    GCon->Logf("CORPSE STABLE: %s(%u): vel=(%g,%g,%g); diffz=%g; lz=%g; stat=%d; fn=(%g,%g,%g):%d",
               GetClass()->GetName(), GetUniqueId(),
               Velocity.x, Velocity.y, Velocity.z,
               tm.FloorZ-Origin.z, cslLastPos.z, checkStationary(this),
               tm.EFloor.GetNormal().x, tm.EFloor.GetNormal().y, tm.EFloor.GetNormal().z,
               tm.EFloor.isSlope());
    #endif
    if (Velocity.isZero2D()) {
      // it seems to stay in place, so don't bother re-checking it
      // check for slopes
      //FIXME: use `Sector` and region check instead
      CheckRelPositionPoint(tm, Origin);
      if (tm.EFloor.isSlope()) {
        #if 0
        GCon->Logf("CORPSE STABLE: %s(%u): cslFlags=0x%04XH; pdiff=(%g,%g,%g)",
                   GetClass()->GetName(), GetUniqueId(), cslFlags,
                   (cslLastPos-Origin).x, (cslLastPos-Origin).y, (cslLastPos-Origin).z);
        #endif
        if ((cslFlags&CSL_CorpseSlidingSlope) != 0 && cslLastPos == Origin) {
          // cannot move
          CorpseIdle(this);
        } else {
          const TVec fn = tm.EFloor.GetNormal();
          //const float speed = 15.0f;
          const float speed = 12.0f + 6.0f*((hashU32(GetUniqueId())>>4)&0x3fu)/63.0f;
          cslFlags |= CSL_CorpseSliding|CSL_CorpseSlidingSlope|CSL_CorpseWasNudged;
          Velocity.x = fn.x*speed;
          Velocity.y = fn.y*speed;
          #ifdef VV_DEBUG_CSL_VERBOSE
          GCon->Logf("...slope: vel=(%g,%g,%g)", Velocity.x, Velocity.y, Velocity.z);
          #endif
        }
      } else if ((cslFlags&CSL_CorpseSliding) == 0) {
        // wasn't sliding
        cslFlags &= ~(CSL_CorpseSliding|CSL_CorpseSlidingSlope);
        // "map was just loaded" hack
        if (cslStartTime == 0.0f) {
          cslStartTime = 0.2f;
          cslCheckDelay = 0.4f + 0.2f*FRandom();
        } else {
          CorpseIdle(this);
        }
      } else {
        // was sliding, and stopped
        CorpseIdle(this);
      }
      cslLastPos = Origin;
    } else {
      // still moving
      //FIXME: use `Sector` and region check instead
      CheckRelPositionPoint(tm, Origin);
      if (!tm.EFloor.isSlope()) {
        cslFlags &= ~(CSL_CorpseSliding|CSL_CorpseSlidingSlope);
      }
    }
  }
}


//==========================================================================
//
//  COMMAND_WITH_AC dbg_slide_awake_all
//
//  awake all bodies
//
//==========================================================================
COMMAND_WITH_AC(dbg_slide_awake_all) {
  bool nocorpses = false;
  bool nodrops = false;
  bool marknudged = false;

  for (int f = 1; f < Args.length(); f += 1) {
    VStr arg = Args[f];
    if (arg.strEquCI("nocorpses")) nocorpses = true;
    else if (arg.strEquCI("nodrops")) nodrops = true;
    else if (arg.strEquCI("marknudged")) marknudged = true;
  }

  VThinker *th = (GLevel ? GLevel->ThinkerHead : nullptr);
  while (th) {
    VEntity *c = Cast<VEntity>(th);
    th = th->Next;
    if (c) {
      if (!nocorpses &&
          (c->EntityFlags&(VEntity::EF_Corpse|VEntity::EF_Missile|VEntity::EF_Invisible|VEntity::EF_ActLikeBridge)) == VEntity::EF_Corpse &&
          (c->FlagsEx&VEntity::EFEX_Monster))
      {
        c->cslCheckDelay = 0.0f;
        c->cslStartTime = 0.2f;
        c->cslFlags &= ~(VEntity::CSL_CorpseSliding|VEntity::CSL_CorpseSlidingSlope|VEntity::CSL_CorpseWasNudged);
        if (marknudged) c->cslFlags |= VEntity::CSL_CorpseWasNudged;
      }
      else if (!nodrops &&
               (c->FlagsEx&(VEntity::EFEX_Special|VEntity::EFEX_Dropped)) == (VEntity::EFEX_Special|VEntity::EFEX_Dropped) &&
               (c->EntityFlags&(VEntity::EF_Solid|VEntity::EF_NoBlockmap)) == 0)
      {
        c->cslCheckDelay = 0.0f;
        c->cslStartTime = 0.2f;
        c->cslFlags &= ~(VEntity::CSL_CorpseSliding|VEntity::CSL_CorpseSlidingSlope|VEntity::CSL_CorpseWasNudged);
        if (marknudged) c->cslFlags |= VEntity::CSL_CorpseWasNudged;
      }
    }
  }
}


//==========================================================================
//
//  COMMAND_AC dbg_slide_awake_all
//
//==========================================================================
COMMAND_AC(dbg_slide_awake_all) {
  TArray<VStr> list;
  VStr prefix = (aidx < args.length() ? args[aidx] : VStr());
  list.Append("nocorpses");
  list.Append("nodrops");
  list.Append("marknudged");
  return VCommand::AutoCompleteFromListCmd(prefix, list);
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
      //GCon->Logf(NAME_Debug, "  : %s lifetime (lmt=%g; %g : %f)", GetClass()->GetName(), LastMoveTime-deltaTime, LastMoveTime, deltaTime);
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


  //HACK: slide dead bodies away if their center of mass is out of the good sector
  if (gm_slide_bodies.asBool() &&
      (EntityFlags&(EF_Corpse|EF_Missile|EF_Invisible|EF_ActLikeBridge)) == EF_Corpse &&
      (FlagsEx&EFEX_Monster))
  {
    #ifdef VV_DEBUG_CSL_VERBOSE
    GCon->Logf("CORPSE-0: %s(%u): pre: delay=%g; stm=%g; pz=%g; flg=0x%04xH",
               GetClass()->GetName(), GetUniqueId(),
               cslCheckDelay, cslStartTime, cslLastZ,
               cslFlags);
    #endif

    // if sector with the corpse was moved...
    if (FlagsEx&EFEX_SomeSectorMoved) {
      FlagsEx &= ~EFEX_SomeSectorMoved;
      // consider this corpse "fresh"
      CorpseIdle(this);
      cslCheckDelay = 0.1f + 0.2f*FRandom();
      cslStartTime = 0.2f;
    }

    // is it not idle?
    if (cslCheckDelay >= 0.0f) {
      cslCheckDelay -= deltaTime;
      if (cslCheckDelay <= 0.0f) CorpseSlide(deltaTime);
    }

    #ifdef VV_DEBUG_CSL_VERBOSE
    GCon->Logf("CORPSE-1: %s(%u): pre: delay=%g; stm=%g; pz=%g; flg=0x%04xH",
               GetClass()->GetName(), GetUniqueId(),
               cslCheckDelay, cslStartTime, cslLastZ,
               cslFlags);
    #endif
  }
  else if (gm_slide_drop_items.asBool() &&
           (FlagsEx&(EFEX_Special|EFEX_Dropped)) == (EFEX_Special|EFEX_Dropped) &&
           (EntityFlags&(EF_Solid|EF_NoBlockmap)) == 0)
  {
    #ifdef VV_DEBUG_CSL_VERBOSE
    GCon->Logf("DROP-0: %s(%u): pre: delay=%g; stm=%g; pz=%g; flg=0x%04xH",
               GetClass()->GetName(), GetUniqueId(),
               cslCheckDelay, cslStartTime, cslLastZ,
               cslFlags);
    #endif

    // if sector with the corpse was moved...
    if (FlagsEx&EFEX_SomeSectorMoved) {
      FlagsEx &= ~EFEX_SomeSectorMoved;
      // consider this corpse "fresh"
      CorpseIdle(this);
      cslCheckDelay = 0.1f + 0.2f*FRandom();
      cslStartTime = 0.2f;
    }

    // is it not idle?
    if (cslCheckDelay >= 0.0f) {
      cslCheckDelay -= deltaTime;
      if (cslCheckDelay <= 0.0f) CorpseSlide(deltaTime);
    }

    #ifdef VV_DEBUG_CSL_VERBOSE
    GCon->Logf("DROP-1: %s(%u): pre: delay=%g; stm=%g; pz=%g; flg=0x%04xH",
               GetClass()->GetName(), GetUniqueId(),
               cslCheckDelay, cslStartTime, cslLastZ,
               cslFlags);
    #endif
  } else {
    // just in case: clear sliding flags
    cslFlags = 0;
  }

  #if 0
  if (FlagsEx&EFEX_Dropped) {
    GCon->Logf("ITEN: %s(%u): flags=0x%08xH; exflags=0x%08xH",
               GetClass()->GetName(), GetUniqueId(),
               EntityFlags, FlagsEx);
  }
  #endif

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
  const int HashIndex = VLevelInfo::TIDHashBucket(tid);
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
    const int HashIndex = VLevelInfo::TIDHashBucket(TID);
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
  if (InState == VState::GetInvalidState()) InState = nullptr;

  if (!InState || IsGoingToDie()) {
    if (developer) GCon->Logf(NAME_Dev, "   (00):%s: dead (0x%04x) before state actions, state is %s", *GetClass()->GetFullName(), GetFlags(), (InState ? *InState->Loc.toStringNoCol() : "<none>"));
    State = nullptr;
    StateTime = -1.0f;
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

  if (VState::IsNoJumpState(InState)) {
    if (!State) {
      GCon->Logf(NAME_Error, "invalid continuation state in `%s::SetState()` (%s)", *GetClass()->GetFullName(), (State ? *State->Loc.toStringNoCol() : "<null>"));
      InState = nullptr;
    } else {
      InState = State->NextState;
    }
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
      if (!isFiniteF(StateTime)) {
        GCon->Logf(NAME_Error, "invalid state duration in `%s::SetState()` at '%s'!", *GetClass()->GetFullName(), *st->Loc.toStringNoCol());
        State = nullptr;
        break;
      }
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
          VObject::ExecuteFunctionNoArgs(this, st->Function); //k8: allow VMT lookups (k8:why?)
          if (IsGoingToDie()) {
            State = nullptr;
            break;
          }
          if (setStateNewState && !VState::IsNoJumpState(setStateNewState)) {
            // recursive invocation set a new state
            st = setStateNewState;
            StateTime = 0.0f;
            continue;
          }
        }
      }

      st = State->NextState;
    } while (StateTime == 0.0f);
    VSLOGF("%s: SetState(%d), done with %s", GetClass()->GetName(), setStateWatchCat, (State ? *State->Loc.toStringNoCol() : "<none>"));
  }
  vassert(setStateWatchCat == 0);

  if (!State || IsGoingToDie()) {
    if (developer) {
      GCon->Logf(NAME_Dev, "*** 001: STATE DYING THINKER %u: %s from %s", GetUniqueId(), GetClass()->GetName(), (InState ? *InState->Loc.toStringNoCol() : "<none>"));
      if (IsPlayer()) __builtin_trap();
    }
    DispSpriteFrame = 0;
    DispSpriteName = NAME_None;
    StateTime = -1.0f;
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
    if (!isFiniteF(StateTime)) {
      GCon->Logf(NAME_Error, "invalid initial state duration in `%s::SetState()` at '%s'!", *GetClass()->GetFullName(), *InState->Loc.toStringNoCol());
      StateTime = 0.0002f;
    } else {
      if (StateTime > 0.0f) StateTime += 0.0002f; //k8: delay it slightly, so spawner may do its business
    }
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
void VEntity::GetStateEffects (TArrayNC<VLightEffectDef *> &Lights, TArrayNC<VParticleEffectDef *> &Part) const {
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
  Call.WasFunCall = 0;
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
      Call.WasFunCall = 1;
      XLevel->CallingState = S;
      // assume success by default
      Call.Result = 1;
      VObject::ExecuteFunctionNoArgs(Actor, S->Function); //k8: allow VMT lookups (k8:why?)
      // at least one success means overal success (see below)
      //res |= Call.Result;
    } else {
      Call.Result = 0; // don't modify
    }

    if (Call.State == S) {
      // abort immediately if next state loops to itself
      // in this case the overal result is always false
      if (S->NextState == S) { Call.WasFunCall = 1;/*don't overwrite res*/ res = 0; break; }
      // advance to the next state
      S = S->NextState;
      // at least one success means overal success
      res |= Call.Result;
    } else {
      // there was a state jump, result should not be modified
      S = Call.State;
    }
  }

  // if there was no function calls, and we are stopped with "stop", assume success
  if (!S && !Call.WasFunCall) res = 1;

  //if (dbg) GCon->Logf(NAME_Debug, "*** %u:%s(%s):%s:  CallStateChain EXIT", GetUniqueId(), GetClass()->GetName(), Actor->GetClass()->GetName(), (S ? *S->Loc.toStringShort() : "<none>"));
  return !!res;
}


//==========================================================================
//
//  VEntity::StartSound
//
//==========================================================================
void VEntity::StartSound (VName Sound, vint32 Channel, float Volume, float Attenuation, bool Loop,
                          float Pitch)
{
  if (!Sector) return;
  if (Sector->SectorFlags&sector_t::SF_Silent) return;
  //if (IsPlayer()) GCon->Logf(NAME_Debug, "player sound '%s' (sound class '%s', gender '%s')", *Sound, *SoundClass, *SoundGender);
  Super::StartSound(Origin, SoundOriginID,
    GSoundManager->ResolveEntitySound(SoundClass, SoundGender, Sound),
    Channel, Volume, Attenuation, Loop, Pitch);
}


//==========================================================================
//
//  VEntity::StartLocalSound
//
//==========================================================================
void VEntity::StartLocalSound (VName Sound, vint32 Channel, float Volume, float Attenuation, float ForcePitch) {
  if (Sector->SectorFlags&sector_t::SF_Silent) return;
  if (Player) {
    Player->eventClientStartSound(
      GSoundManager->ResolveEntitySound(SoundClass, SoundGender, Sound),
      TVec(0, 0, 0), /*0*/-666, Channel, Volume, Attenuation, false, ForcePitch);
  }
}


//==========================================================================
//
//  VEntity::StopSound
//
//==========================================================================
void VEntity::StopSound (vint32 channel) {
  //if (IsPlayer()) GCon->Logf(NAME_Debug, "STOPSOUND: %s: chan=%d; sod=%d", GetClass()->GetName(), channel, SoundOriginID);
  if (SoundOriginID) Super::StopSound(SoundOriginID, channel);
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
//  VEntity::Check3DPObjLineBlockedInternal
//
//  used in 3d pobj collision detection
//  WARNING! this doesn't check vertical coordinates
//           if it's not required for blocking top texture!
//  all necessary checks (height and intersection) shoule be already done
//
//==========================================================================
bool VEntity::Check3DPObjLineBlockedInternal (const polyobj_t *po, const line_t *ld) const noexcept {
  const float mobjz0 = Origin.z;
  const float ptopz = po->poceiling.minz;
  if (mobjz0 >= ptopz) {
    if ((ld->flags&ML_CLIP_MIDTEX) == 0) return false;
    if (!IsBlocking3DPobjLineTop(ld)) return false;
    // side doesn't matter, as it is guaranteed that both sides have the texture with the same height
    const side_t *tside = &XLevel->Sides[ld->sidenum[0]];
    if (tside->TopTexture <= 0) return false; // wtf?!
    VTexture *ttex = GTextureManager(tside->TopTexture);
    if (!ttex || ttex->Type == TEXTYPE_Null) return false; // wtf?!
    const float texh = ttex->GetScaledHeightF()/tside->Top.ScaleY;
    if (texh <= 0.0f) return false;
    return (mobjz0 < ptopz+texh); // hit top texture?
  } else {
    return IsBlockingLine(ld);
  }
}


//==========================================================================
//
//  VEntity::Check3DPObjLineBlocked
//
//  used in 3d pobj collision detection
//  WARNING! this doesn't check vertical coordinates
//           if it's not required for blocking top texture!
//
//==========================================================================
bool VEntity::Check3DPObjLineBlocked (const polyobj_t *po, const line_t *ld) const noexcept {
  if (!po || !ld || !po->posector) return false; // just in case
  if (Height <= 0.0f || GetMoveRadius() <= 0.0f) return false; // just in case
  if (!LineIntersects(ld)) return false;
  return Check3DPObjLineBlockedInternal(po, ld);
}


//==========================================================================
//
//  VEntity::CheckPObjLineBlocked
//
//  used in pobj collision detection
//
//==========================================================================
bool VEntity::CheckPObjLineBlocked (const polyobj_t *po, const line_t *ld) const noexcept {
  if (!po || !ld) return false; // just in case
  if (Height <= 0.0f || GetMoveRadius() <= 0.0f) return false; // just in case
  if (!LineIntersects(ld)) return false;
  if (po->posector) {
    if (Origin.z >= po->poceiling.maxz) {
      // fully above, may hit top blocking texture
      if ((ld->flags&ML_CLIP_MIDTEX) == 0) return false;
    } else {
      if (Origin.z+Height <= po->pofloor.minz) return false; // fully below
    }
    return Check3DPObjLineBlockedInternal(po, ld);
  } else {
    // check for non-3d pobj with midtex
    if ((ld->flags&(ML_TWOSIDED|ML_3DMIDTEX)) != (ML_TWOSIDED|ML_3DMIDTEX)) return IsBlockingLine(ld);
    // use front side
    //const int side = 0; //ld->PointOnSide(mobj->Origin);
    float pz0 = 0.0f, pz1 = 0.0f;
    if (!XLevel->GetMidTexturePosition(ld, /*side*/0, &pz0, &pz1)) return false; // no middle texture
    return (Origin.z < pz1 && Origin.z+Height > pz0);
  }
}


//==========================================================================
//
//  VEntity::IsRealBlockingLine
//
//  this does all the necessary checks for pobjs and 3d pobjs too
//  does check for blocking 3d midtex, and for 3d floors (i.e. checks openings)
//
//  returns `true` if blocking
//
//==========================================================================
bool VEntity::IsRealBlockingLine (const line_t *ld) const noexcept {
  if (!ld) return false;
  if (Height <= 0.0f || GetMoveRadius() <= 0.0f) return false; // just in case
  if (!ld->backsector || !(ld->flags&ML_TWOSIDED)) return LineIntersects(ld); // one sided line
  // polyobject line?
  if (ld->pobj()) return CheckPObjLineBlocked(ld->pobj(), ld);
  if (!LineIntersects(ld)) return false;
  if (IsBlockingLine(ld)) return true;
  // check openings
  opening_t *open = XLevel->LineOpenings(ld, Origin, SPF_NOBLOCKING, !(EntityFlags&EF_Missile)/*do3dmidtex*/); // missiles ignores 3dmidtex
  const float z0 = Origin.z;
  const float z1 = z0+Height;
  open = VLevel::FindRelOpening(open, z0, z1);
  // process railings
  if (open && (ld->flags&ML_RAILING)) {
    open->bottom += 32.0f;
    open->range -= 32.0f;
    if (open->range <= 0.0f || z1 < open->bottom || z0 > open->top) open = nullptr;
  }
  // blocked if there is no opening
  return !!open;
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
  /*
  P_GET_BOOL_OPT(Exact, false);
  P_GET_NAME_OPT(SubLabel, NAME_None);
  P_GET_NAME(StateName);
  P_GET_SELF;
  RET_PTR(Self->FindState(StateName, SubLabel, Exact));
  */
  VName StateName;
  VOptParamName SubLabel(NAME_None);
  VOptParamBool Exact(false);
  VOptParamBool ignoreDefault(false);
  vobjGetParamSelf(StateName, SubLabel, Exact, ignoreDefault);
  VState *state = Self->FindState(StateName, SubLabel, Exact);
  if (ignoreDefault && state) {
    /* check if it's not an Actor state */
    VStateLabel *ass = classActor->FindStateLabel(StateName, SubLabel, Exact);
    if (ass && ass->State == state) state = nullptr;
  }
  RET_PTR(state);
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
  P_GET_PTR(TArrayNC<VParticleEffectDef *>, Part);
  P_GET_PTR(TArrayNC<VLightEffectDef *>, Lights);
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
//                             optional float Atenuation, optional bool Loop,
//                             optional float Pitch);
IMPLEMENT_FUNCTION(VEntity, PlaySound) {
  VName SoundName;
  int Channel;
  VOptParamFloat Volume(1.0f);
  VOptParamFloat Attenuation(1.0f);
  VOptParamBool Loop(false);
  VOptParamFloat Pitch(0.0f); // "use default"; negative means "disable forced pitch"
  vobjGetParamSelf(SoundName, Channel, Volume, Attenuation, Loop, Pitch);
  if (Channel&256) Loop = true; // sorry for this magic number
  Channel &= 7; // other bits are flags
  Self->StartSound(SoundName, Channel, Volume, Attenuation, Loop, Pitch);
}

IMPLEMENT_FUNCTION(VEntity, StopSound) {
  int Channel;
  vobjGetParamSelf(Channel);
  //GCon->Logf(NAME_Debug, "%s: STOPSOUND: chan=%d", Self->GetClass()->GetName(), Channel);
  Self->StopSound(Channel);
}

IMPLEMENT_FUNCTION(VEntity, AreSoundsEquivalent) {
  VName Sound1, Sound2;
  vobjGetParamSelf(Sound1, Sound2);
  if (Sound1 == Sound2) { RET_BOOL(true); return; }
  RET_BOOL(GSoundManager->ResolveEntitySound(Self->SoundClass,
    Self->SoundGender, Sound1) == GSoundManager->ResolveEntitySound(
    Self->SoundClass, Self->SoundGender, Sound2));
}

IMPLEMENT_FUNCTION(VEntity, IsSoundPresent) {
  VName Sound;
  vobjGetParamSelf(Sound);
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
