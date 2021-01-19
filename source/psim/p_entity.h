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
//
//                ENTITY DATA
//
// NOTES: VEntity
//
// mobj_ts are used to tell the refresh where to draw an image, tell the
// world simulation when objects are contacted, and tell the sound driver
// how to position a sound.
//
// The refresh uses the next and prev links to follow lists of things in
// sectors as they are being drawn. The sprite, frame, and angle elements
// determine which patch_t is used to draw the sprite if it is visible.
// The sprite and frame values are allmost allways set from VState
// structures. The statescr.exe utility generates the states.h and states.c
// files that contain the sprite/frame numbers from the statescr.txt source
// file. The xyz origin point represents a point at the bottom middle of the
// sprite (between the feet of a biped). This is the default origin position
// for patch_ts grabbed with lumpy.exe. A walking creature will have its z
// equal to the floor it is standing on.
//
// The sound code uses the x,y, and subsector fields to do stereo
// positioning of any sound effited by the VEntity.
//
// The play simulation uses the blocklinks, x,y,z, radius, height to
// determine when mobj_ts are touching each other, touching lines in the map,
// or hit by trace lines (gunshots, lines of sight, etc). The VEntity->flags
// element has various bit flags used by the simulation.
//
// Every VEntity is linked into a single sector based on its origin
// coordinates. The subsector_t is found with R_PointInSubsector(x,y), and
// the sector_t can be found with subsector->sector. The sector links are
// only used by the rendering code, the play simulation does not care about
// them at all.
//
// Any VEntity that needs to be acted upon by something else in the play
// world (block movement, be shot, etc) will also need to be linked into the
// blockmap. If the thing has the MF_NOBLOCK flag set, it will not use the
// block links. It can still interact with other things, but only as the
// instigator (missiles will run into other things, but nothing can run into
// a missile). Each block in the grid is 128*128 units, and knows about every
// line_t that it contains a piece of, and every interactable VEntity that has
// its origin contained.
//
// A valid VEntity is a VEntity that has the proper subsector_t filled in for
// its xy coordinates and is linked into the sector from which the subsector
// was made, or has the MF_NOSECTOR flag set (the subsector_t needs to be
// valid even if MF_NOSECTOR is set), and is linked into a blockmap block or
// has the MF_NOBLOCKMAP flag set. Links should only be modified by the
// P_[Un]SetThingPosition() functions. Do not change the MF_NO? flags while
// a thing is valid.
//
// Any questions?
//
// k8: yep. why only one blockmap cell? thing should be put in all cells it
//     touches.
//
//==========================================================================

struct tmtrace_t;

#ifdef CLIENT
extern VCvarB r_interpolate_thing_angles_models;
extern VCvarB r_interpolate_thing_angles_sprites;
#endif
extern VCvarB sv_decoration_block_projectiles;

struct VDropOffLineInfo {
  line_t *line;
  vint32 side; // is front dropoff?
  // side -> dir:
  //  0: line->normal
  //  1: -line->normal
};


enum {
  STYLE_None, // do not draw
  STYLE_Normal, // normal; just copy the image to the screen
  STYLE_Fuzzy, // draw silhouette using "fuzz" effect
  STYLE_SoulTrans, // draw translucent with amount in r_transsouls
  STYLE_OptFuzzy, // draw as fuzzy or translucent, based on user preference
  STYLE_Stencil, // solid color
  STYLE_Translucent, // draw translucent
  STYLE_Add, // draw additive
  STYLE_Shaded, // implemented
  STYLE_TranslucentStencil, // implemented
  STYLE_Shadow, // implemented
  STYLE_Subtract, // not implemented
  STYLE_AddStencil, // solid color, additive
  STYLE_AddShaded, // implemented
  // special style for sprites only
  STYLE_Dark = 64,
};

// color tralslation types
enum {
  TRANSL_None, // no translation
  TRANSL_Standard, // game's standard translations
  TRANSL_Player, // per-player translations
  TRANSL_Level, // ACS translations
  TRANSL_BodyQueue, // translations of dead players
  TRANSL_Decorate, // translations defined in DECORATE
  TRANSL_Blood, // blood translations, for blood color

  TRANSL_Max,

  TRANSL_TYPE_SHIFT = 16,
};


struct VDropItemInfo {
  VClass *Type;
  VName TypeName;
  vint32 Amount;
  float Chance;
};

struct VDamageFactor {
  VName DamageType;
  float Factor;
  enum {
    DF_REPLACE = 1u<<0,
    DF_NOARMOR = 1u<<1,
  };
  vuint32 Flags;
};

struct VPainChanceInfo {
  VName DamageType;
  float Chance;
};

struct VDamageColorType {
  VName Type;
  vint32 Color;
  float Intensity;
};


// ////////////////////////////////////////////////////////////////////////// //
//WARNING: sync this with VC code!
#define PHYS_MAXMOVE  (1050.0f*30.0f)  /* this is rougly equal to 900 in Vanilla speed */


class VEntity : public VThinker {
  DECLARE_CLASS(VEntity, VThinker, 0)
  NO_DEFAULT_CONSTRUCTOR(VEntity)

  // info for drawing: position
  TVec Origin;

  // for smooth movement
  TVec LastMoveOrigin;
  TAVec LastMoveAngles;
  float LastMoveTime; // lifetime, or fadeout time for `EFEX_NoTickGrav`; if >0: lifetime
  float LastMoveDuration;
  enum {
    MVF_JustMoved = 1u<<0,
  };
  vuint32 MoveFlags;

  // momentums, used to update position
  TVec Velocity;

  TAVec Angles; // orientation

  VState *State;
  vint32 DispSpriteFrame; // high 7 bits is frame
  VName DispSpriteName;
  float StateTime; // state tic counter

  // more drawing info
  vuint8 SpriteType; // how to draw sprite
  VName FixedSpriteName;
  VStr FixedModelName;
  vuint8 ModelVersion;
  vint32 NumRenderedShadows;

  vuint8 RenderStyle;
  float Alpha;
  vint32 Translation;

  vint32 StencilColor;

  float FloorClip; // value to use for floor clipping

  // scaling
  float ScaleX;
  float ScaleY;

  subsector_t *SubSector;
  sector_t *Sector;
  sector_t *LastSector; // transient

  // interaction info, by BLOCKMAP
  // links in blocks (if needed)
  //TODO: think should occupy all blockmap cells it touches
  //      i should fix the code which does "blockmap marching", so
  //      it won't return one thing several times
  VEntity *BlockMapNext;
  VEntity *BlockMapPrev;
  vuint32 BlockMapCell; // blockmap cell index+1, so it will be independent of coords (0 means none)

  // links in sector (if needed)
  VEntity *SNext;
  VEntity *SPrev;

  msecnode_t *TouchingSectorList;

  // the closest interval over all contacted sectors
  float FloorZ;
  float CeilingZ;
  float DropOffZ;

  // closest floor and ceiling, source of floorz and ceilingz
  /*
  enum {
    FC_FlipFloor = 1u<<0,
    FC_FlipCeiling = 1u<<1,
  };
  vuint32 fcflags;
  */
  TSecPlaneRef EFloor;
  TSecPlaneRef ECeiling;

  // if == validcount, already checked
  //vint32 ValidCount;

  // flags
  enum {
    EF_Solid                  = 1u<<0u, // blocks
    EF_NoSector               = 1u<<1u, // don't use the sector links (invisible but touchable)
    EF_NoBlockmap             = 1u<<2u, // don't use the blocklinks (inert but displayable)
    EF_IsPlayer               = 1u<<3u, // player or player-bot
    EF_FixedModel             = 1u<<4u, // internal renderer flag
    EF_NoGravity              = 1u<<5u, // don't apply gravity every tic
    EF_PassMobj               = 1u<<6u, // enable z block checking; if on, this flag will allow the mobj to pass over/under other mobjs
    EF_ColideWithThings       = 1u<<7u,
    EF_ColideWithWorld        = 1u<<8u,
    EF_CheckLineBlocking      = 1u<<9u,
    EF_CheckLineBlockMonsters = 1u<<10u,
    EF_DropOff                = 1u<<11u, // allow jumps from high places
    EF_Float                  = 1u<<12u, // allow moves to any height, no gravity
    EF_Fly                    = 1u<<13u, // fly mode is active
    EF_Blasted                = 1u<<14u, // missile will pass through ghosts
    EF_CantLeaveFloorpic      = 1u<<15u, // stay within a certain floor type
    EF_FloorClip              = 1u<<16u, // if feet are allowed to be clipped
    EF_IgnoreCeilingStep      = 1u<<17u, // continue walk without lowering itself
    EF_IgnoreFloorStep        = 1u<<18u, // continue walk ignoring floor height changes
    EF_AvoidingDropoff        = 1u<<19u, // used to move monsters away from dropoffs
    EF_OnMobj                 = 1u<<20u, // mobj is resting on top of another mobj
    EF_Corpse                 = 1u<<21u, // don't stop moving halfway off a step
    EF_FullBright             = 1u<<22u, // make current state full bright
    EF_Invisible              = 1u<<23u, // don't draw this actor
    EF_Missile                = 1u<<24u, // don't hit same species, explode on block
    EF_DontOverlap            = 1u<<25u, // prevent some things from overlapping.
    EF_KillOnUnarchive        = 1u<<26u, // remove this entity on loading game
    EF_ActLikeBridge          = 1u<<27u, // always allow objects to pass.
    EF_NoDropOff              = 1u<<28u, // can't drop off under any circumstances
    EF_Bright                 = 1u<<29u, // always render full bright
    EF_CanJump                = 1u<<30u, // this entity can jump to high places
    EF_StepMissile            = 1u<<31u, // missile can "walk" up steps

    // portal entities (skyboxes, etc.) don't have "fixed model", so
    // reuse this flag to prevent infinite portal recursion.
    // the renderer will set this flag before calling `RenderScene()`, and
    // will reset it afterwards
    EF_PortalDirty = EF_FixedModel,
  };
  vuint32 EntityFlags;

  vint32 Health;
  vint32 InitialHealth;

  // extra flags
  enum {
    EFEX_Monster          = 1u<<0u, // is this a monster?
    EFEX_Rendered         = 1u<<1u, // was this thing rendered? (unused, not set ever)
    EFEX_NetDetach        = 1u<<2u, // server is using this flag for some replication conditions
    EFEX_NoInteraction    = 1u<<3u, // moved from EntityEx
    EFEX_NoTickGrav       = 1u<<4u, // do not call `Tick()` (but process gravity)
    EFEX_PseudoCorpse     = 1u<<5u, // for sprite fixer
    EFEX_FloatBob         = 1u<<6u, // use float bobbing z movement
    EFEX_NoTickGravLT     = 1u<<7u, // if `EFEX_NoTickGrav` set, perform lifetime logic
    EFEX_StickToFloor     = 1u<<8u,
    EFEX_StickToCeiling   = 1u<<9u,
    EFEX_DetachFromServer = 1u<<10u, // set this flag to detach the entity from the server in network game
    EFEX_CorpseFlipped    = 1u<<11u, // this flag is set when monster died, and need its corpse flipped
    EFEX_AlwaysTick       = 1u<<12u, // cannot optimise physics for this entity
  };
  vuint32 FlagsEx;

  // 0: none
  // 1: switches object to "k8vavoomInternalNoTickGrav" when it enters idle state (the one with negative duration)
  // 2: switches object to "NoInteraction" when it enters idle state (the one with negative duration)
  // both "no interaction" state changes removes object from blockmap
  // checked in `AdvanceState()`
  vuint8 NoTickGravOnIdleType;

  // for movement checking
  float Radius;
  float Height;
  float VanillaHeight;

  // additional info record for player avatars only
  // only valid if type == MT_PLAYER
  VBasePlayer *Player;

  vint32 TID; // thing identifier
  VEntity *TIDHashNext;
  VEntity *TIDHashPrev;

  vint32 Special; // special
  vint32 Args[5]; // special arguments

  vint32 SoundOriginID;

  // params
  float Mass;
  float MaxStepHeight;
  float MaxDropoffHeight;
  float Gravity;
  float Friction;

  // water
  vuint8 WaterLevel;
  vuint8 WaterType;

  vint32 Score;
  vint32 Accuracy;
  vint32 Stamina;

  // variables, so it may be customised
  float WaterSinkFactor;
  float WaterSinkSpeed;

  float FloatBobPhase; // in seconds; <0 means "choose at random"; should be converted to ticks; amp is [0..63]
  float FloatBobStrength;

  // for player sounds
  VName SoundClass;
  VName SoundGender;

  // owner entity of inventory item
  VEntity *Owner;

  VName DecalName;

  // set this field in `BeginPlay()` to override `VThinker::SpawnThinker()` result
  // this is required for `RandomSpawner`
  VEntity *BeginPlayResult;

  // `SetState()` guard
  vuint32 setStateWatchCat; // watchcat
  VState *setStateNewState; // if we are inside of `SetState()`, just set this, and get out; cannot be `nullptr`

  // moved here from `SkyViewpoint`, so i don't have to call any VM methods to get it
  enum {
    SF_SkyBoxAlways = 1u<<0,
  };
  vuint32 SkyBoxFlags;
  //bool bAlways;
  /*SkyViewpoint*/VEntity *Mate;
  // also used in `EFEX_NoTickGrav`:
  //   <=0: die immediately
  //    >0: fadeout step time
  float PlaneAlpha;

  // for renderer; if <= 0, use `Radius` instead
  float RenderRadius;

  VEntity *Target; // moved from `EntityEx`, and it is `EntityEx`
  VEntity *Tracer; // moved from `EntityEx`, and it is `EntityEx`
  VEntity *Master; // moved from `EntityEx`, and it is `EntityEx`

  // for networking; will be automatically replicated by the server when `Owner` is replicated
  vuint32 OwnerSUId;
  // for networking; will be automatically replicated by the server when `Tracer` is replicated
  vuint32 TracerSUId;
  // for networking; will be automatically replicated by the server when `Target` is replicated
  vuint32 TargetSUId;
  // for networking; will be automatically replicated by the server when `Master` is replicated
  vuint32 MasterSUId;

  // used in networking; see VavoomC code for explanations
  float DataGameTime;

  // used in network code; set to `Ent->XLevel->Time` when we got a new origin
  float NetLastOrgUpdateTime;

protected:
  //VEntity () : SoundClass(E_NoInit), SoundGender(E_NoInit), DecalName(E_NoInit) {}

  void CopyTraceFloor (tmtrace_t *tr, bool setz=true);
  void CopyTraceCeiling (tmtrace_t *tr, bool setz=true);

  void CopyRegFloor (sec_region_t *reg, bool setz=true);
  void CopyRegCeiling (sec_region_t *reg, bool setz=true);

public:
  VVA_CHECKRESULT inline float GetFloorNormalZ () const noexcept { return EFloor.GetNormalZ(); }
  VVA_CHECKRESULT inline float GetCeilingNormalZ () const noexcept { return ECeiling.GetNormalZ(); }

  VVA_CHECKRESULT inline float GetRenderRadius () const noexcept { return max2(Height, (RenderRadius > 0.0f ? RenderRadius : Radius)); }

  VVA_CHECKRESULT inline bool IsPortalDirty () const noexcept { return (EntityFlags&EF_PortalDirty); }
  inline void SetPortalDirty (bool v) noexcept { if (v) EntityFlags |= EF_PortalDirty; else EntityFlags &= ~EF_PortalDirty; }

public:
  // i am so happy that crap-plus-plus doesn't have try/finally. thanks for nothing, you useless shit.
  struct AutoPortalDirty {
    VEntity *e;

    AutoPortalDirty () = delete;
    AutoPortalDirty (const AutoPortalDirty &) = delete;
    AutoPortalDirty &operator = (const AutoPortalDirty &) = delete;

    inline AutoPortalDirty (VEntity *ae) noexcept : e(ae) {
      if (e) {
        vassert(!e->IsPortalDirty());
        e->SetPortalDirty(true);
      }
    }

    inline ~AutoPortalDirty () noexcept {
      if (e) {
        vassert(e->IsPortalDirty());
        e->SetPortalDirty(false);
      }
    }
  };

public:
  // call this *AFTER* all decorate code was compiled!
  static void EntityStaticInit ();

public:
  // VObject interface
  virtual void Destroy () override;
  virtual void SerialiseOther (VStream &) override;

  // VThinker interface
  virtual void DestroyThinker () override;
  virtual void AddedToLevel () override;
  virtual void Tick (float deltaTime) override;

  inline bool IsRenderable () const noexcept {
    return
      State && !IsGoingToDie() &&
      !(EntityFlags&(VEntity::EF_NoSector|VEntity::EF_Invisible)) &&
      SubSector; // just in case
  }

  void SetTID (int);
  void InsertIntoTIDList (int);
  void RemoveFromTIDList ();

  TVec GetDrawOrigin ();
  // this returns only interpolated delta (from current origin to previous origin!)
  // used in dynamic light rendering
  TVec GetDrawDelta ();
  // return interpolated angles
  TAVec GetInterpolatedDrawAngles ();
  inline TAVec GetModelDrawAngles () {
    #ifdef CLIENT
      return (r_interpolate_thing_angles_models && (MoveFlags&MVF_JustMoved) ? GetInterpolatedDrawAngles() : Angles);
    #else
      return Angles;
    #endif
  }
  inline TAVec GetSpriteDrawAngles () {
    #ifdef CLIENT
      return (r_interpolate_thing_angles_sprites && (MoveFlags&MVF_JustMoved) ? GetInterpolatedDrawAngles() : Angles);
    #else
      return Angles;
    #endif
  }

  inline VEntity *GetTopOwner () {
    VEntity *Ret = this;
    while (Ret->Owner) Ret = Ret->Owner;
    return Ret;
  }

  sector_t *GetTouchedFloorSectorEx (sector_t **swimmable);

  // cannot return `nullptr`
  VTerrainInfo *GetActorTerrain ();

  /*
  bool eventSkyBoxGetAlways () { static VMethodProxy method("SkyBoxGetAlways"); vobjPutParamSelf(); VMT_RET_BOOL(method); }
  VEntity *eventSkyBoxGetMate () { static VMethodProxy method("SkyBoxGetMate"); vobjPutParamSelf(); VMT_RET_REF(VEntity, method); }
  float eventSkyBoxGetPlaneAlpha () { static VMethodProxy method("SkyBoxGetPlaneAlpha"); vobjPutParamSelf(); VMT_RET_FLOAT(method); }
  */
  inline bool GetSkyBoxAlways () const noexcept { return !!(SkyBoxFlags&SF_SkyBoxAlways); }
  inline VEntity *GetSkyBoxMate () const noexcept { return Mate; }
  inline float GetSkyBoxPlaneAlpha () const noexcept { return PlaneAlpha; }

  void eventOnMapSpawn (mthing_t *mthing) { static VMethodProxy method("OnMapSpawn"); vobjPutParamSelf(mthing); VMT_RET_VOID(method); }
  void eventBeginPlay () { static VMethodProxy method("BeginPlay"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventDestroyed () { static VMethodProxy method("Destroyed"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  bool eventTouch (VEntity *Other, bool disableEffects) { static VMethodProxy method("Touch"); vobjPutParamSelf(Other, disableEffects); VMT_RET_BOOL(method); }
  void eventCheckForPushSpecial (line_t *line, int side) { static VMethodProxy method("CheckForPushSpecial"); vobjPutParamSelf(line, side); VMT_RET_VOID(method); }
  void eventBlastedHitLine () { static VMethodProxy method("BlastedHitLine"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventHandleFloorclip () { static VMethodProxy method("HandleFloorclip"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventCrossSpecialLine (line_t *ld, int side) { static VMethodProxy method("CrossSpecialLine"); vobjPutParamSelf(ld, side); VMT_RET_VOID(method); }
  bool eventSectorChanged (int CrushChange) { static VMethodProxy method("SectorChanged"); vobjPutParamSelf(CrushChange); VMT_RET_BOOL(method); }
  void eventClearInventory () { static VMethodProxy method("ClearInventory"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventGiveInventory (VName ItemName, int Amount, bool allowReplacement) { static VMethodProxy method("GiveInventory"); vobjPutParamSelf(ItemName, Amount, allowReplacement); VMT_RET_VOID(method); }
  void eventTakeInventory (VName ItemName, int Amount, bool allowReplacement) { static VMethodProxy method("TakeInventory"); vobjPutParamSelf(ItemName, Amount, allowReplacement); VMT_RET_VOID(method); }
  int eventCheckInventory (VName ItemName, bool allowReplacement, bool fromACS=false) { static VMethodProxy method("CheckInventory"); vobjPutParamSelf(ItemName, allowReplacement, fromACS); VMT_RET_INT(method); }
  int eventUseInventoryName (VName ItemName, bool allowReplacement) { static VMethodProxy method("UseInventoryName"); vobjPutParamSelf(ItemName, allowReplacement); VMT_RET_INT(method); }
  int eventGetSigilPieces () { static VMethodProxy method("GetSigilPieces"); vobjPutParamSelf(); VMT_RET_INT(method); }
  int eventGetArmorPoints () { static VMethodProxy method("GetArmorPoints"); vobjPutParamSelf(); VMT_RET_INT(method); }
  int eventCheckNamedWeapon (VName Name) { static VMethodProxy method("CheckNamedWeapon"); vobjPutParamSelf(Name); VMT_RET_INT(method); }
  int eventSetNamedWeapon (VName Name) { static VMethodProxy method("SetNamedWeapon"); vobjPutParamSelf(Name); VMT_RET_INT(method); }
  int eventGetAmmoCapacity (VName Name) { static VMethodProxy method("GetAmmoCapacity"); vobjPutParamSelf(Name); VMT_RET_INT(method); }
  void eventSetAmmoCapacity (VName Name, int Amount) { static VMethodProxy method("SetAmmoCapacity"); vobjPutParamSelf(Name, Amount); VMT_RET_VOID(method); }
  bool eventMoveThing (TVec Pos, bool Fog) { static VMethodProxy method("MoveThing"); vobjPutParamSelf(Pos, Fog); VMT_RET_BOOL(method); }
  float eventGetStateTime (VState *State, float StateTime) { static VMethodProxy method("GetStateTime"); vobjPutParamSelf(State, StateTime); VMT_RET_FLOAT(method); }
  void eventSetActorProperty (int Prop, int IntVal, VStr StrVal) { static VMethodProxy method("SetActorProperty"); vobjPutParamSelf(Prop, IntVal, StrVal); VMT_RET_VOID(method); }
  int eventGetActorProperty (int Prop) { static VMethodProxy method("GetActorProperty"); vobjPutParamSelf(Prop); VMT_RET_INT(method); }
  void eventCheckForSectorActions (sector_t *OldSec, bool OldAboveFakeFloor, bool OldAboveFakeCeiling) { static VMethodProxy method("CheckForSectorActions"); vobjPutParamSelf(OldSec, OldAboveFakeFloor, OldAboveFakeCeiling); VMT_RET_VOID(method); }
  void eventCalcFakeZMovement (TVec &Ret, float DeltaTime) { static VMethodProxy method("CalcFakeZMovement"); vobjPutParamSelf(&Ret, DeltaTime); VMT_RET_VOID(method); }
  int eventClassifyActor () { static VMethodProxy method("ClassifyActor"); vobjPutParamSelf(); VMT_RET_INT(method); }
  int eventMorphActor (VName PlayerClass, VName MonsterClass, float Duration, int Style, VName MorphFlash, VName UnmorphFlash) {
    static VMethodProxy method("MorphActor"); vobjPutParamSelf(PlayerClass, MonsterClass, Duration, Style, MorphFlash, UnmorphFlash); VMT_RET_INT(method);
  }
  int eventUnmorphActor (VEntity *Activator, int Force) { static VMethodProxy method("UnmorphActor"); vobjPutParamSelf(Activator, Force); VMT_RET_INT(method); }
  void eventGetViewEntRenderParams (float &OutAlpha, int &OutRenderStyle) { static VMethodProxy method("GetViewEntRenderParams"); vobjPutParamSelf(&OutAlpha, &OutRenderStyle); VMT_RET_VOID(method); }

  float eventFindActivePowerupTime (VName pname) { static VMethodProxy method("FindActivePowerupTime"); vobjPutParamSelf(pname); VMT_RET_FLOAT(method); }

  // bool eventDropItem (name itemName, int amount, float chance)
  bool eventDropItem (VName itemName, int amout, float chance) {
    static VMethodProxy method("eventDropItem");
    vobjPutParamSelf(itemName, amout, chance);
    VMT_RET_BOOL(method);
  }

  // bool eventACSDropInventory (name itemName)
  bool eventACSDropInventory (VName itemName) {
    static VMethodProxy method("ACSDropInventory");
    vobjPutParamSelf(itemName);
    VMT_RET_BOOL(method);
  }

  // bool ACSIsPointerEqual (int aptr0, int aptr1, Entity src1)
  bool eventACSIsPointerEqual (int aptr0, int aptr1, VEntity *src1) {
    static VMethodProxy method("ACSIsPointerEqual");
    vobjPutParamSelf(aptr0, aptr1, src1);
    VMT_RET_BOOL(method);
  }

  // bool eventCanRaise ()
  VState *eventCanRaise () {
    static VMethodProxy method("CanRaise");
    vobjPutParamSelf();
    VMT_RET_REF(VState, method);
  }

  // EntityEx PickActor (optional TVec Origin, TVec dir, float distance, optional int actorMask, optional int wallMask) {
  VEntity *eventPickActor (bool specified_orig, TVec orig, TVec dir, float dist, bool specified_actmask, int actmask, bool specified_wallmask, int wallmask) {
    static VMethodProxy method("PickActor");
    vobjPutParamSelf(
      VOptPutParamVec(orig, specified_orig),
      dir, dist,
      VOptPutParamInt(actmask, specified_actmask),
      VOptPutParamInt(wallmask, specified_wallmask)
    );
    VMT_RET_REF(VEntity, method);
  }

  //override bool do_CheckProximity (name classname, float distance, optional int count, optional int flags, optional int aptr)
  bool doCheckProximity (VName classname, float dist,
                         bool specified_count, int count,
                         bool specified_flags, int flags,
                         bool specified_aptr, int aptr)
  {
    static VMethodProxy method("do_CheckProximity");
    vobjPutParamSelf(
      classname, dist,
      VOptPutParamInt(count, specified_count),
      VOptPutParamInt(flags, specified_flags),
      VOptPutParamInt(aptr, specified_aptr)
    );
    VMT_RET_BOOL(method);
  }

  VEntity *eventDoAAPtr (int aaptr) { static VMethodProxy method("eventDoAAPtr"); vobjPutParamSelf(aaptr); VMT_RET_REF(VEntity, method); }

  VEntity *eventFindTargetForACS () { static VMethodProxy method("eventFindTargetForACS"); vobjPutParamSelf(); VMT_RET_REF(VEntity, method); }

  bool eventSetPointerForACS (int assign_slot, int tid, int aptr, int flags) {
    static VMethodProxy method("eventSetPointerForACS");
    vobjPutParamSelf(assign_slot, tid, aptr, flags);
    VMT_RET_BOOL(method);
  }

  //void eventLineAttackACS (TVec dir, float distance, int LADamage, name pufftype, name damagetype, int flags, int pufftid)
  void eventLineAttackACS (TVec dir, float distance, int damage, VName pufftype, VName damagetype, int flags, int pufftid) {
    static VMethodProxy method("eventLineAttackACS");
    vobjPutParamSelf(dir, distance, damage, pufftype, damagetype, flags, pufftid);
    VMT_RET_VOID(method);
  }

  inline int eventGetArmorPointsForType (VName atype) { static VMethodProxy method("GetArmorPointsForType"); vobjPutParamSelf(atype); VMT_RET_INT(method); }

  inline void QS_ClearEntityInventory () { static VMethodProxy method("QS_ClearEntityInventory"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  inline VEntity *QS_GetEntityInventory () { static VMethodProxy method("QS_GetEntityInventory"); vobjPutParamSelf(); VMT_RET_REF(VEntity, method); }
  inline VEntity *QS_SpawnEntityInventory (VName className) { static VMethodProxy method("QS_SpawnEntityInventory"); vobjPutParamSelf(className); VMT_RET_REF(VEntity, method); }

  void QS_Save () { static VMethodProxy method("QS_Save"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void QS_Load () { static VMethodProxy method("QS_Load"); vobjPutParamSelf(); VMT_RET_VOID(method); }

  float GetViewHeight () { static VMethodProxy method("GetViewHeight"); vobjPutParamSelf(); VMT_RET_FLOAT(method); }

  inline VEntity *ehGetInventory () { static VMethodProxy method("EngineHelperGetInventory"); vobjPutParamSelf(); VMT_RET_REF(VEntity, method); }

  void DebugDumpInventory (bool startFromSelf=false) {
    GCon->Logf(NAME_Debug, "==== INVENTORY: %s:%u (%s) ====", GetClass()->GetName(), GetUniqueId(), (IsGoingToDie() ? "dead" : "alive"));
    for (VEntity *ent = (startFromSelf ? this : ehGetInventory()); ent; ent = ent->ehGetInventory()) {
      GCon->Logf(NAME_Debug, "  %s:%u (%s:%u)", ent->GetClass()->GetName(), ent->GetUniqueId(), (ent->Owner ? ent->Owner->GetClass()->GetName() : "<none>"), (ent->Owner ? ent->Owner->GetUniqueId() : 0u));
      if (ent->IsGoingToDie()) GCon->Log(NAME_Debug, "    item is going to die!");
      if (ent->Owner && ent->Owner->IsGoingToDie()) GCon->Log(NAME_Debug, "    owner is going to die!");
    }
  }

  void eventSimplifiedTick (float deltaTime) {
    static VMethodProxy method("SimplifiedTick");
    vobjPutParamSelf(deltaTime);
    VMT_RET_VOID(method);
  }

  bool eventPhysicsCheckScroller () {
    static VMethodProxy method("PhysicsCheckScroller");
    vobjPutParamSelf();
    VMT_RET_BOOL(method);
  }

  bool NeedPhysics ();

  //bool IsMonster () { static VMethodProxy method("IsMonster"); vobjPutParamSelf(); VMT_RET_BOOL(method); }
  inline bool IsPlayer () const noexcept { return !!(EntityFlags&EF_IsPlayer); }
  inline bool IsMissile () const noexcept { return !!(EntityFlags&EF_Missile); }
  inline bool IsAnyCorpse () const noexcept { return !!((EntityFlags&EF_Corpse)|(FlagsEx&EFEX_PseudoCorpse)); }
  inline bool IsRealCorpse () const noexcept { return !!(EntityFlags&EF_Corpse); }
  inline bool IsSolid () const noexcept { return !!(EntityFlags&EF_Solid); }
  inline bool IsMonster () const noexcept { return !!(FlagsEx&EFEX_Monster); }
  inline bool IsNoInteraction () const noexcept { return !!(FlagsEx&EFEX_NoInteraction); }
  inline bool IsPlayerOrMonster () const noexcept { return !!((EntityFlags&EF_IsPlayer)|(FlagsEx&EFEX_Monster)); }
  inline bool IsPlayerOrMissileOrMonster () const noexcept { return !!((EntityFlags&(EF_IsPlayer|EF_Missile))|(FlagsEx&EFEX_Monster)); }
  // floating, flying, floatbob; used in renderer to disable sprite offset fix
  inline bool IsAnyAerial () const noexcept { return !!((EntityFlags&(EF_Float|EF_Fly))|(FlagsEx&EFEX_FloatBob)); }
  inline bool IsFloatBob () const noexcept { return !!(FlagsEx&EFEX_FloatBob); }
  inline bool IsSpriteFlipped () const noexcept { return !!(FlagsEx&EFEX_CorpseFlipped); }

  enum EType {
    ET_Unknown,
    ET_Player,
    ET_Missile,
    ET_Corpse,
    ET_Monster,
    ET_Decoration, // any solid thing that isn't previous one (except unknown)
    ET_Pickup, // any non-solid thing that isn't previous one (except unknown)
  };

  // used in renderer
  inline EType Classify () const {
    if (IsPlayer()) return EType::ET_Player;
    if (IsMissile()) return EType::ET_Missile;
    if (IsAnyCorpse()) return EType::ET_Corpse;
    if (IsMonster()) return EType::ET_Monster;
    if (IsSolid()) return EType::ET_Decoration;
    // either pickup, or unknown
    // get inventory class
    static VClass *invCls = nullptr;
    static bool invClsInited = false;
    if (!invClsInited) {
      invClsInited = true;
      invCls = VMemberBase::StaticFindClass("Inventory");
    }
    return (invCls && IsA(invCls) ? EType::ET_Pickup : EType::ET_Unknown);
  }

  bool SetState (VState *);
  void SetInitialState (VState *);
  void PerformOnIdle (); // call when state duration becomes negative
  bool AdvanceState (float);
  VState *FindState (VName StateName, VName SubLabel=NAME_None, bool Exact=false);
  VState *FindStateEx (VStr StateName, bool Exact);
  bool HasSpecialStates (VName);
  void GetStateEffects (TArray<VLightEffectDef *> &, TArray<VParticleEffectDef *> &) const;
  bool HasAnyLightEffects () const;
  bool CallStateChain (VEntity *, VState *);

  void CheckWater (); // this sets `WaterLevel` and `WaterType`
  bool CheckPosition (TVec);
  // `tmtrace` is output
  bool CheckRelPosition (tmtrace_t &tmtrace, TVec Pos, bool noPickups=false, bool ignoreMonsters=false, bool ignorePlayers=false);
  // `tmtrace` is output
  bool TryMove (tmtrace_t &tmtrace, TVec newPos, bool AllowDropOff, bool checkOnly=false, bool noPickups=false);
  VEntity *TestMobjZ (const TVec &);
  void SlideMove (float StepVelScale, bool noPickups=false);
  void BounceWall (float DeltaTime, const line_t *blockline, float overbounce, float bouncefactor);
  void UpdateVelocity (float DeltaTime, bool allowSlopeFriction);
  TVec FakeZMovement ();
  VEntity *CheckOnmobj ();
  bool CheckSides (TVec);
  bool FixMapthingPos ();
  void CheckDropOff (float &DeltaX, float &DeltaY, float baseSpeed=32.0f);
  // returns number of *added* lines (i.e. it won't clear `list`)
  // `list` can be `nullptr`
  int FindDropOffLine (TArray<VDropOffLineInfo> *list, TVec pos);

  // this is used to move chase camera
  TVec SlideMoveCamera (TVec org, TVec end, float radius);

  const int GetEffectiveSpriteIndex () const { return DispSpriteFrame&0x00ffffff; }
  const int GetEffectiveSpriteFrame () const { return ((DispSpriteFrame>>24)&VState::FF_FRAMEMASK); }

  inline VAliasModelFrameInfo getMFI () const {
    VAliasModelFrameInfo res;
    res.sprite = DispSpriteName;
    res.frame = GetEffectiveSpriteFrame();
    res.index = (State ? State->InClassIndex : -1);
    res.spriteIndex = GetEffectiveSpriteIndex();
    return res;
  }

  // gross hack!
  inline VAliasModelFrameInfo getNextMFI () {
    if (!State || !State->NextState) return getMFI();
    vint32 oldDSF = DispSpriteFrame;
    VName oldDSN = DispSpriteName;
    UpdateDispFrameFrom(State->NextState);
    VAliasModelFrameInfo res;
    res.sprite = DispSpriteName;
    res.frame = GetEffectiveSpriteFrame();
    res.index = State->NextState->InClassIndex;
    res.spriteIndex = GetEffectiveSpriteIndex();
    DispSpriteFrame = oldDSF;
    DispSpriteName = oldDSN;
    return res;
  }

  inline float GetBlockingHeightFor (const VEntity *other) const noexcept {
    if (!(EntityFlags&EF_Missile)) return other->Height;
    const float vhgt = other->VanillaHeight;
    return (vhgt > 0 || (vhgt < 0 && !sv_decoration_block_projectiles) ? fabsf(vhgt) : other->Height);
  }

private:
  // world iterator callbacks
  bool CheckThing (tmtrace_t &, VEntity *);
  bool CheckLine (tmtrace_t &, line_t *);
  bool CheckRelThing (tmtrace_t &tmtrace, VEntity *Other, bool noPickups=false);
  bool CheckRelLine (tmtrace_t &tmtrace, line_t *ld, bool skipSpecials=false);
  void BlockedByLine (line_t *);
  void PushLine (const tmtrace_t &tmtrace, bool checkOnly);
  static TVec ClipVelocity (const TVec &, const TVec &, float);
  void SlidePathTraverse (float &, line_t* &, float, float, float);

  void CreateSecNodeList ();

  inline void UpdateDispFrameFrom (const VState *st) {
    if (st) {
      if ((st->Frame&VState::FF_KEEPSPRITE) == 0 && st->SpriteIndex != 1) {
        DispSpriteFrame = (DispSpriteFrame&~0x00ffffff)|(st->SpriteIndex&0x00ffffff);
        DispSpriteName = st->SpriteName;
      }
      if ((st->Frame&VState::FF_DONTCHANGE) == 0) DispSpriteFrame = (DispSpriteFrame&0x00ffffff)|((st->Frame&VState::FF_FRAMEMASK)<<24);
    }
  }

public:
  bool SetDecorateFlag (VStr, bool); // true: flag was found and set
  bool GetDecorateFlag (VStr);

public:
  // pass -666 to force proper check (sorry for this hack)
  void LinkToWorld (int properFloorCheck=0);
  void UnlinkFromWorld ();

  enum {
    CSE_ForShooting      = 1u<<0,
    CSE_AlwaysBetter     = 1u<<1,
    CSE_IgnoreBlockAll   = 1u<<2,
    CSE_IgnoreFakeFloors = 1u<<3,
    CSE_CheckBaseRegion  = 1u<<4,
  };
  bool CanSeeEx (VEntity *Ent, unsigned flags=0);

  bool CanSee (VEntity *Ent, bool forShooting=false, bool alwaysBetter=false);
  inline bool CanShoot (VEntity *Ent) { return CanSee(Ent, true); }

  void StartSound (VName Sound, vint32 Channel, float Volume, float Attenuation, bool Loop, bool Local=false);
  void StartLocalSound (VName Sound, vint32 Channel, float Volume, float Attenuation);
  void StopSound (vint32 Channel);
  void StartSoundSequence (VName, vint32 Channel);
  void AddSoundSequenceChoice (VName);
  void StopSoundSequence ();

  DECLARE_FUNCTION(SetTID)
  DECLARE_FUNCTION(SetState)
  DECLARE_FUNCTION(SetInitialState)
  DECLARE_FUNCTION(AdvanceState)
  DECLARE_FUNCTION(FindState)
  DECLARE_FUNCTION(FindStateEx)
  DECLARE_FUNCTION(HasSpecialStates)
  DECLARE_FUNCTION(GetStateEffects)
  DECLARE_FUNCTION(HasAnyLightEffects)
  DECLARE_FUNCTION(CallStateChain)
  DECLARE_FUNCTION(PlaySound)
  DECLARE_FUNCTION(StopSound)
  DECLARE_FUNCTION(AreSoundsEquivalent)
  DECLARE_FUNCTION(IsSoundPresent)
  DECLARE_FUNCTION(StartSoundSequence)
  DECLARE_FUNCTION(AddSoundSequenceChoice)
  DECLARE_FUNCTION(StopSoundSequence)
  DECLARE_FUNCTION(CheckWater)
  DECLARE_FUNCTION(CheckSides)
  DECLARE_FUNCTION(FixMapthingPos)
  DECLARE_FUNCTION(CheckDropOff)
  DECLARE_FUNCTION(FindDropOffLine)
  DECLARE_FUNCTION(CheckPosition)
  DECLARE_FUNCTION(CheckRelPosition)
  DECLARE_FUNCTION(TryMove)
  DECLARE_FUNCTION(TryMoveEx)
  DECLARE_FUNCTION(TestMobjZ)
  DECLARE_FUNCTION(SlideMove)
  DECLARE_FUNCTION(BounceWall)
  DECLARE_FUNCTION(CheckOnmobj)
  DECLARE_FUNCTION(LinkToWorld)
  DECLARE_FUNCTION(UnlinkFromWorld)
  DECLARE_FUNCTION(CanSee)
  DECLARE_FUNCTION(CanSeeAdv)
  DECLARE_FUNCTION(CanShoot)
  DECLARE_FUNCTION(RoughBlockSearch)
  DECLARE_FUNCTION(SetDecorateFlag)
  DECLARE_FUNCTION(GetDecorateFlag)

  DECLARE_FUNCTION(QS_PutInt);
  DECLARE_FUNCTION(QS_PutName);
  DECLARE_FUNCTION(QS_PutStr);
  DECLARE_FUNCTION(QS_PutFloat);

  DECLARE_FUNCTION(QS_GetInt);
  DECLARE_FUNCTION(QS_GetName);
  DECLARE_FUNCTION(QS_GetStr);
  DECLARE_FUNCTION(QS_GetFloat);

  DECLARE_FUNCTION(UpdateVelocity);

  DECLARE_FUNCTION(GetBlockingHeightFor);

  DECLARE_FUNCTION(GetTouchedFloorSector);
  DECLARE_FUNCTION(GetTouchedFloorSectorEx);
};
