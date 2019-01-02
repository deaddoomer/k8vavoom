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
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
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
struct cptrace_t;

enum {
  STYLE_None, // do not draw
  STYLE_Normal, // normal; just copy the image to the screen
  STYLE_Fuzzy, // draw silhouette using "fuzz" effect
  STYLE_SoulTrans, // draw translucent with amount in r_transsouls
  STYLE_OptFuzzy, // draw as fuzzy or translucent, based on user preference
  STYLE_Translucent = 64, // draw translucent
  STYLE_Add, // draw additive
  STYLE_Stencil, // solid color
  STYLE_AddStencil, // solid color, additive
};

// colour tralslation types
enum {
  TRANSL_None, // no translation
  TRANSL_Standard, // game's standard translations
  TRANSL_Player, // per-player translations
  TRANSL_Level, // ACS translations
  TRANSL_BodyQueue, // translations of dead players
  TRANSL_Decorate, // translations defined in DECORATE
  TRANSL_Blood, // blood translations, for blood colour

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
};

struct VPainChanceInfo {
  VName DamageType;
  float Chance;
};


// ////////////////////////////////////////////////////////////////////////// //
class VEntity : public VThinker {
  DECLARE_CLASS(VEntity, VThinker, 0)
  NO_DEFAULT_CONSTRUCTOR(VEntity)

  // info for drawing: position
  TVec Origin;

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
  int NumTouchingLights;

  vuint8 RenderStyle;
  float Alpha;
  int Translation;

  int StencilColour;

  float FloorClip; // value to use for floor clipping

  // scaling
  float ScaleX;
  float ScaleY;

  subsector_t *SubSector;
  sector_t *Sector;

  // interaction info, by BLOCKMAP
  // links in blocks (if needed)
  //TODO: think should occupy all blockmap cells it touches
  //      i should fix the code which does "blockmap marching", so
  //      it won't return one thing several times
  VEntity *BlockMapNext;
  VEntity *BlockMapPrev;

  // links in sector (if needed)
  VEntity *SNext;
  VEntity *SPrev;

  msecnode_t *TouchingSectorList;

  // the closest interval over all contacted sectors
  float FloorZ;
  float CeilingZ;
  float DropOffZ;

  // closest floor and ceiling, source of floorz and ceilingz
  sec_plane_t *Floor;
  sec_plane_t *Ceiling;

  // if == validcount, already checked
  int ValidCount;

  // flags
  enum {
    EF_Solid      = 0x00000001, // blocks
    EF_NoSector   = 0x00000002, // don't use the sector links (invisible but touchable)
    EF_NoBlockmap = 0x00000004, // don't use the blocklinks (inert but displayable)
    EF_IsPlayer   = 0x00000008, // player or player-bot
    EF_FixedModel = 0x00000010, // internal renderer flag
    EF_NoGravity  = 0x00000020, // don't apply gravity every tic
    EF_PassMobj   = 0x00000040, // enable z block checking; if on, this flag will allow the mobj to pass over/under other mobjs
    EF_ColideWithThings  = 0x00000080,
    EF_ColideWithWorld   = 0x00000100,
    EF_CheckLineBlocking = 0x00000200,
    EF_CheckLineBlockMonsters = 0x00000400,
    EF_DropOff           = 0x00000800, // allow jumps from high places
    EF_Float             = 0x00001000, // allow moves to any height, no gravity
    EF_Fly               = 0x00002000, // fly mode is active
    EF_Blasted           = 0x00004000, // missile will pass through ghosts
    EF_CantLeaveFloorpic = 0x00008000, // stay within a certain floor type
    EF_FloorClip         = 0x00010000, // if feet are allowed to be clipped
    EF_IgnoreCeilingStep = 0x00020000, // continue walk without lowering itself
    EF_IgnoreFloorStep   = 0x00040000, // continue walk ignoring floor height changes
    EF_AvoidingDropoff   = 0x00080000, // used to move monsters away from dropoffs
    EF_OnMobj            = 0x00100000, // mobj is resting on top of another mobj
    EF_Corpse            = 0x00200000, // don't stop moving halfway off a step
    EF_FullBright        = 0x00400000, // make current state full bright
    EF_Invisible         = 0x00800000, // don't draw this actor
    EF_Missile           = 0x01000000, // don't hit same species, explode on block
    EF_DontOverlap       = 0x02000000, // prevent some things from overlapping.
    EF_KillOnUnarchive   = 0x04000000, // remove this entity on loading game
    EF_ActLikeBridge     = 0x08000000, // always allow objects to pass.
    EF_NoDropOff         = 0x10000000, // can't drop off under any circumstances
    EF_Bright            = 0x20000000, // always render full bright
    EF_CanJump           = 0x40000000, // this entity can jump to high places
    EF_StepMissile       = 0x80000000, // missile can "walk" up steps
  };
  vuint32 EntityFlags;

  int Health;
  int InitialHealth;

  // for movement checking
  float Radius;
  float Height;

  // additional info record for player avatars only
  // only valid if type == MT_PLAYER
  VBasePlayer *Player;

  int TID; // thing identifier
  VEntity *TIDHashNext;
  VEntity *TIDHashPrev;

  int Special; // special
  int Args[5]; // special arguments

  int SoundOriginID;

  // params
  float Mass;
  float MaxStepHeight;
  float MaxDropoffHeight;
  float Gravity;

  // water
  vuint8 WaterLevel;
  vuint8 WaterType;

  // for player sounds
  VName SoundClass;
  VName SoundGender;

  // owner entity of inventory item
  VEntity *Owner;

  VName DecalName;

protected:
  //VEntity () : SoundClass(E_NoInit), SoundGender(E_NoInit), DecalName(E_NoInit) {}

public:
  static int FIndex_OnMapSpawn;
  static int FIndex_BeginPlay;
  static int FIndex_Destroyed;
  static int FIndex_Touch;
  static int FIndex_BlastedHitLine;
  static int FIndex_CheckForPushSpecial;
  static int FIndex_ApplyFriction;
  static int FIndex_HandleFloorclip;
  static int FIndex_CrossSpecialLine;
  static int FIndex_SectorChanged;
  static int FIndex_RoughCheckThing;
  static int FIndex_GiveInventory;
  static int FIndex_TakeInventory;
  static int FIndex_CheckInventory;
  static int FIndex_GetSigilPieces;
  static int FIndex_MoveThing;
  static int FIndex_GetStateTime;

public:
  static void InitFuncIndexes ();

  // VObject interface
  virtual void Destroy () override;
  virtual void Serialise (VStream &) override;

  // VThinker interface
  virtual void DestroyThinker () override;
  virtual void AddedToLevel () override;

  void SetTID (int);
  void InsertIntoTIDList (int);
  void RemoveFromTIDList ();

  inline VEntity *GetTopOwner () {
    VEntity *Ret = this;
    while (Ret->Owner) Ret = Ret->Owner;
    return Ret;
  }

  void eventOnMapSpawn (mthing_t *mthing) {
    P_PASS_SELF;
    P_PASS_PTR(mthing);
    EV_RET_VOID_IDX(FIndex_OnMapSpawn);
  }
  void eventBeginPlay () {
    P_PASS_SELF;
    EV_RET_VOID_IDX(FIndex_BeginPlay);
  }
  void eventDestroyed () {
    P_PASS_SELF;
    EV_RET_VOID_IDX(FIndex_Destroyed);
  }
  bool eventTouch (VEntity *Other) {
    P_PASS_SELF;
    P_PASS_REF(Other);
    EV_RET_BOOL_IDX(FIndex_Touch);
  }
  void eventCheckForPushSpecial (line_t *line, int side) {
    P_PASS_SELF;
    P_PASS_PTR(line);
    P_PASS_INT(side);
    EV_RET_VOID_IDX(FIndex_CheckForPushSpecial);
  }
  void eventBlastedHitLine () {
    P_PASS_SELF;
    EV_RET_VOID_IDX(FIndex_BlastedHitLine);
  }
  void eventApplyFriction () {
    P_PASS_SELF;
    EV_RET_VOID_IDX(FIndex_ApplyFriction);
  }
  void eventHandleFloorclip () {
    P_PASS_SELF;
    EV_RET_VOID_IDX(FIndex_HandleFloorclip);
  }
  void eventCrossSpecialLine (line_t *ld, int side) {
    P_PASS_SELF;
    P_PASS_PTR(ld);
    P_PASS_INT(side);
    EV_RET_VOID_IDX(FIndex_CrossSpecialLine);
  }
  bool eventSectorChanged (int CrushChange) {
    P_PASS_SELF;
    P_PASS_INT(CrushChange);
    EV_RET_BOOL_IDX(FIndex_SectorChanged);
  }
  void eventClearInventory () {
    P_PASS_SELF;
    EV_RET_VOID(NAME_ClearInventory);
  }
  void eventGiveInventory (VName ItemName, int Amount) {
    P_PASS_SELF;
    P_PASS_NAME(ItemName);
    P_PASS_INT(Amount);
    EV_RET_VOID_IDX(FIndex_GiveInventory);
  }
  void eventTakeInventory (VName ItemName, int Amount) {
    P_PASS_SELF;
    P_PASS_NAME(ItemName);
    P_PASS_INT(Amount);
    EV_RET_VOID_IDX(FIndex_TakeInventory);
  }
  int eventCheckInventory (VName ItemName) {
    P_PASS_SELF;
    P_PASS_NAME(ItemName);
    EV_RET_INT_IDX(FIndex_CheckInventory);
  }
  int eventUseInventoryName (VName ItemName) {
    P_PASS_SELF;
    P_PASS_NAME(ItemName);
    EV_RET_INT(NAME_UseInventoryName);
  }
  int eventGetSigilPieces () {
    P_PASS_SELF;
    EV_RET_INT_IDX(FIndex_GetSigilPieces);
  }
  int eventGetArmorPoints () {
    P_PASS_SELF;
    EV_RET_INT(NAME_GetArmorPoints);
  }
  int eventCheckNamedWeapon (VName Name) {
    P_PASS_SELF;
    P_PASS_NAME(Name);
    EV_RET_INT(NAME_CheckNamedWeapon);
  }
  int eventSetNamedWeapon (VName Name) {
    P_PASS_SELF;
    P_PASS_NAME(Name);
    EV_RET_INT (NAME_SetNamedWeapon);
  }
  int eventGetAmmoCapacity (VName Name) {
    P_PASS_SELF;
    P_PASS_NAME(Name);
    EV_RET_INT(NAME_GetAmmoCapacity);
  }
  void eventSetAmmoCapacity (VName Name, int Amount) {
    P_PASS_SELF;
    P_PASS_NAME(Name);
    P_PASS_INT(Amount);
    EV_RET_VOID(NAME_SetAmmoCapacity);
  }
  bool eventMoveThing (TVec Pos, bool Fog) {
    P_PASS_SELF;
    P_PASS_VEC(Pos);
    P_PASS_BOOL(Fog);
    EV_RET_BOOL_IDX(FIndex_MoveThing);
  }
  float eventGetStateTime (VState *State, float StateTime) {
    P_PASS_SELF;
    P_PASS_PTR(State);
    P_PASS_FLOAT(StateTime);
    EV_RET_FLOAT_IDX(FIndex_GetStateTime);
  }
  void eventSetActorProperty (int Prop, int IntVal, VStr StrVal) {
    P_PASS_SELF;
    P_PASS_INT(Prop);
    P_PASS_INT(IntVal);
    P_PASS_STR(StrVal);
    EV_RET_VOID(NAME_SetActorProperty);
  }
  int eventGetActorProperty (int Prop) {
    P_PASS_SELF;
    P_PASS_INT(Prop);
    EV_RET_INT(NAME_GetActorProperty);
  }
  void eventCheckForSectorActions (sector_t *OldSec, bool OldAboveFakeFloor, bool OldAboveFakeCeiling) {
    P_PASS_SELF;
    P_PASS_PTR(OldSec);
    P_PASS_BOOL(OldAboveFakeFloor);
    P_PASS_BOOL(OldAboveFakeCeiling);
    EV_RET_VOID(NAME_CheckForSectorActions);
  }
  bool eventSkyBoxGetAlways () {
    P_PASS_SELF;
    EV_RET_BOOL(NAME_SkyBoxGetAlways);
  }
  VEntity *eventSkyBoxGetMate () {
    P_PASS_SELF;
    EV_RET_REF(VEntity, NAME_SkyBoxGetMate);
  }
  float eventSkyBoxGetPlaneAlpha () {
    P_PASS_SELF;
    EV_RET_FLOAT(NAME_SkyBoxGetPlaneAlpha);
  }
  void eventCalcFakeZMovement (TVec &Ret, float DeltaTime) {
    P_PASS_SELF;
    P_PASS_PTR(&Ret);
    P_PASS_FLOAT(DeltaTime);
    EV_RET_VOID(NAME_CalcFakeZMovement);
  }
  int eventClassifyActor () {
    P_PASS_SELF;
    EV_RET_INT(NAME_ClassifyActor);
  }
  int eventMorphActor (VName PlayerClass, VName MonsterClass, float Duration,
    int Style, VName MorphFlash, VName UnmorphFlash)
  {
    P_PASS_SELF;
    P_PASS_NAME(PlayerClass);
    P_PASS_NAME(MonsterClass);
    P_PASS_FLOAT(Duration);
    P_PASS_INT(Style);
    P_PASS_NAME(MorphFlash);
    P_PASS_NAME(UnmorphFlash);
    EV_RET_INT(NAME_MorphActor);
  }
  int eventUnmorphActor (VEntity *Activator, int Force) {
    P_PASS_SELF;
    P_PASS_REF(Activator);
    P_PASS_INT(Force);
    EV_RET_INT(NAME_UnmorphActor);
  }
  void eventGetViewEntRenderParams (float &OutAlpha, int &OutRenderStyle) {
    P_PASS_SELF;
    P_PASS_PTR(&OutAlpha);
    P_PASS_PTR(&OutRenderStyle);
    EV_RET_VOID(NAME_GetViewEntRenderParams);
  }

  float eventFindActivePowerupTime (VName pname) {
    P_PASS_SELF;
    P_PASS_NAME(pname);
    EV_RET_FLOAT(VName("FindActivePowerupTime"));
  }

  // EntityEx PickActor (optional TVec Origin, TVec dir, float distance, optional int actorMask, optional int wallMask) {
  VEntity *eventPickActor (bool specified_orig, TVec orig, TVec dir, float dist, bool specified_actmask, int actmask, bool specified_wallmask, int wallmask) {
    P_PASS_SELF;
    if (!specified_orig) orig = TVec(0, 0, 0);
    P_PASS_VEC(orig);
    P_PASS_BOOL(specified_orig);
    P_PASS_VEC(dir);
    P_PASS_FLOAT(dist);
    if (!specified_actmask) actmask = 0;
    P_PASS_INT(actmask);
    P_PASS_BOOL(specified_actmask);
    if (!specified_wallmask) wallmask = 0;
    P_PASS_INT(wallmask);
    P_PASS_BOOL(specified_wallmask);
    EV_RET_REF(VEntity, VName("PickActor"));
  }

  VEntity *eventDoAAPtr (int aaptr) {
    P_PASS_SELF;
    P_PASS_INT(aaptr);
    EV_RET_REF(VEntity, VName("eventDoAAPtr"));
  }

  VEntity *eventFindTargetForACS () {
    P_PASS_SELF;
    EV_RET_REF(VEntity, VName("eventFindTargetForACS"));
  }

  bool eventSetPointerForACS (int assign_slot, int tid, int aptr, int flags) {
    P_PASS_SELF;
    P_PASS_INT(assign_slot);
    P_PASS_INT(tid);
    P_PASS_INT(aptr);
    P_PASS_INT(flags);
    EV_RET_BOOL(VName("eventSetPointerForACS"));
  }

  //void eventLineAttackACS (TVec dir, float distance, int LADamage, name pufftype, name damagetype, int flags, int pufftid)
  void eventLineAttackACS (TVec dir, float distance, int damage, VName pufftype, VName damagetype, int flags, int pufftid) {
    P_PASS_SELF;
    P_PASS_VEC(dir);
    P_PASS_FLOAT(distance);
    P_PASS_INT(damage);
    P_PASS_NAME(pufftype);
    P_PASS_NAME(damagetype);
    P_PASS_INT(flags);
    P_PASS_INT(pufftid);
    EV_RET_VOID(VName("eventLineAttackACS"));
  }

  void callSectorChanged (int crush) {
    static int mtindex = -666;
    if (mtindex < 0) mtindex = StaticClass()->GetMethodIndex(VName("SectorChanged"));
    P_PASS_SELF;
    P_PASS_INT(crush);
    EV_RET_VOID_IDX(mtindex);
  }

  //static final bool decoDoCheckFlag (string flagname, Entity tgt)
  /*
  bool eventCheckFlag (const VStr &flagname) {
    if (flagname.length() == 0) return false;
    P_PASS_STR(flagname);
    P_PASS_PTR(this);
    EV_RET_BOOL(NAME_decoDoCheckFlag);
  }
  */

  //static final bool decoDoSetFlag (string flagname, Entity tgt, bool newvalue)
  /*
  bool eventSetFlag (const VStr &flagname, bool newvalue) {
    if (flagname.length() == 0) return false;
    P_PASS_STR(flagname);
    P_PASS_PTR(this);
    P_PASS_BOOL(newvalue);
    EV_RET_BOOL(NAME_decoDoSetFlag);
  }
  */

  bool SetState (VState *);
  void SetInitialState (VState *);
  bool AdvanceState (float);
  VState *FindState (VName, VName = NAME_None, bool = false);
  VState *FindStateEx (const VStr &, bool);
  bool HasSpecialStates (VName);
  void GetStateEffects (TArray<VLightEffectDef *> &, TArray<VParticleEffectDef *> &) const;
  bool CallStateChain (VEntity *, VState *);

  bool CheckWater ();
  bool CheckPosition (TVec);
  bool CheckRelPosition (tmtrace_t &, TVec);
  bool TryMove (tmtrace_t &, TVec, bool);
  VEntity *TestMobjZ (const TVec &);
  void SlideMove (float);
  void BounceWall (float, float);
  void UpdateVelocity ();
  TVec FakeZMovement ();
  VEntity *CheckOnmobj ();
  bool CheckSides (TVec);
  void CheckDropOff (float &, float &);

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

private:
  // world iterator callbacks
  bool CheckThing (cptrace_t &, VEntity *);
  bool CheckLine (cptrace_t &, line_t *);
  bool CheckRelThing (tmtrace_t &, VEntity *);
  bool CheckRelLine (tmtrace_t &, line_t *);
  void BlockedByLine (line_t *);
  void PushLine (const tmtrace_t &tmtrace);
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
  bool SetDecorateFlag (const VStr &, bool); // true: flag was found and set
  bool GetDecorateFlag (const VStr &);

public:
  void LinkToWorld ();
  void UnlinkFromWorld ();
  bool CanSee (VEntity *);

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
  DECLARE_FUNCTION(CheckDropOff)
  DECLARE_FUNCTION(CheckPosition)
  DECLARE_FUNCTION(CheckRelPosition)
  DECLARE_FUNCTION(TryMove)
  DECLARE_FUNCTION(TryMoveEx)
  DECLARE_FUNCTION(TestMobjZ)
  DECLARE_FUNCTION(SlideMove)
  DECLARE_FUNCTION(BounceWall)
  DECLARE_FUNCTION(UpdateVelocity)
  DECLARE_FUNCTION(CheckOnmobj)
  DECLARE_FUNCTION(LinkToWorld)
  DECLARE_FUNCTION(UnlinkFromWorld)
  DECLARE_FUNCTION(CanSee)
  DECLARE_FUNCTION(RoughBlockSearch)
  DECLARE_FUNCTION(SetDecorateFlag)
  DECLARE_FUNCTION(GetDecorateFlag)
};
