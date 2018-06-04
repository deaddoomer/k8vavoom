//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************

class VLevelInfo : public VThinker
{
  DECLARE_CLASS(VLevelInfo, VThinker, 0)

  enum { TID_HASH_SIZE = 128 };

  VGameInfo *Game;
  VWorldInfo *World;

  VStr      LevelName;
  vint32      LevelNum;
  vuint8      Cluster;

  VName     NextMap;
  VName     SecretMap;

  vint32      ParTime;
  vint32      SuckTime;

  vint32      Sky1Texture;
  vint32      Sky2Texture;
  float     Sky1ScrollDelta;
  float     Sky2ScrollDelta;
  VName     SkyBox;

  VName     FadeTable;
  vuint32     Fade;
  vuint32     OutsideFog;

  VName     SongLump;
  vuint8      CDTrack;

  enum
  {
    LIF_DoubleSky         = 0x00000001,
    LIF_Lightning         = 0x00000002,
    LIF_Map07Special        = 0x00000004,
    LIF_BaronSpecial        = 0x00000008,
    LIF_CyberDemonSpecial     = 0x00000010,
    LIF_SpiderMastermindSpecial   = 0x00000020,
    LIF_MinotaurSpecial       = 0x00000040,
    LIF_DSparilSpecial        = 0x00000080,
    LIF_IronLichSpecial       = 0x00000100,
    LIF_SpecialActionOpenDoor   = 0x00000200,
    LIF_SpecialActionLowerFloor   = 0x00000400,
    LIF_SpecialActionKillMonsters = 0x00000800,
    LIF_NoIntermission        = 0x00001000,
    LIF_AllowMonsterTelefrags   = 0x00002000,
    LIF_NoAllies          = 0x00004000,
    LIF_DeathSlideShow        = 0x00008000,
    LIF_ForceNoSkyStretch     = 0x00010000,
    LIF_LookupName          = 0x00020000,
    LIF_FallingDamage       = 0x00040000,
    LIF_OldFallingDamage      = 0x00080000,
    LIF_StrifeFallingDamage     = 0x00100000,
    LIF_MonsterFallingDamage    = 0x00200000,
    LIF_NoFreelook          = 0x00400000,
    LIF_NoJump            = 0x00800000,
    LIF_NoAutoSndSeq        = 0x01000000,
    LIF_ActivateOwnSpecial      = 0x02000000,
    LIF_MissilesActivateImpact    = 0x04000000,
    LIF_FilterStarts        = 0x08000000,
    LIF_InfiniteFlightPowerup   = 0x10000000,
    LIF_ClipMidTex          = 0x20000000,
    LIF_WrapMidTex          = 0x40000000,
    LIF_KeepFullInventory     = 0x80000000,
  };
  vuint32     LevelInfoFlags;
  enum
  {
    LIF2_CompatShortTex       = 0x00000001,
    LIF2_CompatStairs       = 0x00000002,
    LIF2_CompatLimitPain      = 0x00000004,
    LIF2_CompatNoPassOver     = 0x00000008,
    LIF2_CompatNoTossDrops      = 0x00000010,
    LIF2_CompatUseBlocking      = 0x00000020,
    LIF2_CompatNoDoorLight      = 0x00000040,
    LIF2_CompatRavenScroll      = 0x00000080,
    LIF2_CompatSoundTarget      = 0x00000100,
    LIF2_CompatDehHealth      = 0x00000200,
    LIF2_CompatTrace        = 0x00000400,
    LIF2_CompatDropOff        = 0x00000800,
    LIF2_CompatBoomScroll     = 0x00001000,
    LIF2_CompatInvisibility     = 0x00002000,
    LIF2_LaxMonsterActivation   = 0x00004000,
    LIF2_HaveMonsterActivation    = 0x00008000,
    LIF2_ClusterHub         = 0x00010000,
    LIF2_BegunPlay          = 0x00020000,
    LIF2_Frozen           = 0x00040000,
  };
  vuint32     LevelInfoFlags2;

  int       TotalKills;
  int       TotalItems;
  int       TotalSecret;    // for intermission
  int       CurrentKills;
  int       CurrentItems;
  int       CurrentSecret;

  float     CompletitionTime; //  For intermission

  // Maintain single and multi player starting spots.
  TArray<mthing_t>  DeathmatchStarts; // Player spawn spots for deathmatch.
  TArray<mthing_t>  PlayerStarts;   // Player spawn spots.

  VEntity *TIDHash[TID_HASH_SIZE];

  float     Gravity;                // Level Gravity
  float     AirControl;
  int       Infighting;
  TArray<VMapSpecialAction> SpecialActions;

  VLevelInfo();

  void SetMapInfo(const mapInfo_t&);

  void SectorStartSound(const sector_t*, int, int, float, float);
  void SectorStopSound(const sector_t*, int);
  void SectorStartSequence(const sector_t*, VName, int);
  void SectorStopSequence(const sector_t*);
  void PolyobjStartSequence(const polyobj_t*, VName, int);
  void PolyobjStopSequence(const polyobj_t*);

  void ExitLevel(int Position);
  void SecretExitLevel(int Position);
  void Completed(int Map, int Position, int SaveAngle);

  bool ChangeSwitchTexture(int, bool, VName, bool&);
  bool StartButton(int, vuint8, int, VName, bool);

  VEntity *FindMobjFromTID(int, VEntity*);

  void ChangeMusic(VName);

  VStr GetLevelName() const
  {
    return LevelInfoFlags & LIF_LookupName ? GLanguage[*LevelName] : LevelName;
  }

  //  Static lights
  DECLARE_FUNCTION(AddStaticLight)
  DECLARE_FUNCTION(AddStaticLightRGB)

  //  Sound sequences
  DECLARE_FUNCTION(SectorStartSequence)
  DECLARE_FUNCTION(SectorStopSequence)
  DECLARE_FUNCTION(PolyobjStartSequence)
  DECLARE_FUNCTION(PolyobjStopSequence)

  //  Exiting the level
  DECLARE_FUNCTION(ExitLevel)
  DECLARE_FUNCTION(SecretExitLevel)
  DECLARE_FUNCTION(Completed)

  //  Special thinker utilites
  DECLARE_FUNCTION(ChangeSwitchTexture)
  DECLARE_FUNCTION(FindMobjFromTID)
  DECLARE_FUNCTION(AutoSave)

  DECLARE_FUNCTION(ChangeMusic)

  void eventSpawnSpecials()
  {
    P_PASS_SELF;
    EV_RET_VOID(NAME_SpawnSpecials);
  }
  void eventUpdateSpecials()
  {
    P_PASS_SELF;
    EV_RET_VOID(NAME_UpdateSpecials);
  }
  void eventAfterUnarchiveThinkers()
  {
    P_PASS_SELF;
    EV_RET_VOID(NAME_AfterUnarchiveThinkers);
  }
  void eventPolyThrustMobj(VEntity *A, TVec thrustDir, polyobj_t *po)
  {
    P_PASS_SELF;
    P_PASS_REF(A);
    P_PASS_VEC(thrustDir);
    P_PASS_PTR(po);
    EV_RET_VOID(NAME_PolyThrustMobj);
  }
  void eventPolyCrushMobj(VEntity *A, polyobj_t *po)
  {
    P_PASS_SELF;
    P_PASS_REF(A);
    P_PASS_PTR(po);
    EV_RET_VOID(NAME_PolyCrushMobj);
  }
  bool eventTagBusy(int tag)
  {
    P_PASS_SELF;
    P_PASS_INT(tag);
    EV_RET_BOOL(NAME_TagBusy);
  }
  bool eventPolyBusy(int polyobj)
  {
    P_PASS_SELF;
    P_PASS_INT(polyobj);
    EV_RET_BOOL(NAME_PolyBusy);
  }
  int eventThingCount(int type, VName TypeName, int tid, int SectorTag)
  {
    P_PASS_SELF;
    P_PASS_INT(type);
    P_PASS_NAME(TypeName);
    P_PASS_INT(tid);
    P_PASS_INT(SectorTag);
    EV_RET_INT(NAME_ThingCount);
  }
  int eventExecuteActionSpecial(int Special, int Arg1, int Arg2, int Arg3,
    int Arg4, int Arg5, line_t *Line, int Side, VEntity *A)
  {
    P_PASS_SELF;
    P_PASS_INT(Special);
    P_PASS_INT(Arg1);
    P_PASS_INT(Arg2);
    P_PASS_INT(Arg3);
    P_PASS_INT(Arg4);
    P_PASS_INT(Arg5);
    P_PASS_PTR(Line);
    P_PASS_INT(Side);
    P_PASS_REF(A);
    EV_RET_INT(NAME_ExecuteActionSpecial);
  }
  int eventEV_ThingProjectile(int tid, int type, int angle, int speed,
    int vspeed, int gravity, int newtid, VName TypeName, VEntity *Activator)
  {
    P_PASS_SELF;
    P_PASS_INT(tid);
    P_PASS_INT(type);
    P_PASS_INT(angle);
    P_PASS_INT(speed);
    P_PASS_INT(vspeed);
    P_PASS_INT(gravity);
    P_PASS_INT(newtid);
    P_PASS_NAME(TypeName);
    P_PASS_REF(Activator);
    EV_RET_INT(NAME_EV_ThingProjectile);
  }
  void eventStartPlaneWatcher(VEntity *it, line_t *line, int lineSide,
    bool ceiling, int tag, int height, int special, int arg1, int arg2,
    int arg3, int arg4, int arg5)
  {
    P_PASS_SELF;
    P_PASS_REF(it);
    P_PASS_PTR(line);
    P_PASS_INT(lineSide);
    P_PASS_BOOL(ceiling);
    P_PASS_INT(tag);
    P_PASS_INT(height);
    P_PASS_INT(special);
    P_PASS_INT(arg1);
    P_PASS_INT(arg2);
    P_PASS_INT(arg3);
    P_PASS_INT(arg4);
    P_PASS_INT(arg5);
    EV_RET_VOID(NAME_StartPlaneWatcher);
  }
  void eventSpawnMapThing(mthing_t *mthing)
  {
    P_PASS_SELF;
    P_PASS_PTR(mthing);
    EV_RET_VOID(NAME_SpawnMapThing);
  }
  void eventUpdateParticle(particle_t *p, float DeltaTime)
  {
    P_PASS_SELF;
    P_PASS_PTR(p);
    P_PASS_FLOAT(DeltaTime);
    EV_RET_VOID(NAME_UpdateParticle);
  }
  int eventAcsSpawnThing(VName Name, TVec Org, int Tid, float Angle)
  {
    P_PASS_SELF;
    P_PASS_NAME(Name);
    P_PASS_VEC(Org);
    P_PASS_INT(Tid);
    P_PASS_FLOAT(Angle);
    EV_RET_INT(NAME_AcsSpawnThing);
  }
  int eventAcsSpawnSpot(VName Name, int SpotTid, int Tid, float Angle)
  {
    P_PASS_SELF;
    P_PASS_NAME(Name);
    P_PASS_INT(SpotTid);
    P_PASS_INT(Tid);
    P_PASS_FLOAT(Angle);
    EV_RET_INT(NAME_AcsSpawnSpot);
  }
  int eventAcsSpawnSpotFacing(VName Name, int SpotTid, int Tid)
  {
    P_PASS_SELF;
    P_PASS_NAME(Name);
    P_PASS_INT(SpotTid);
    P_PASS_INT(Tid);
    EV_RET_INT(NAME_AcsSpawnSpotFacing);
  }
  void eventSectorDamage(int Tag, int Amount, VName DamageType,
    VName ProtectionType, int Flags)
  {
    P_PASS_SELF;
    P_PASS_INT(Tag);
    P_PASS_INT(Amount);
    P_PASS_NAME(DamageType);
    P_PASS_NAME(ProtectionType);
    P_PASS_INT(Flags);
    EV_RET_VOID(NAME_SectorDamage);
  }
  int eventDoThingDamage(int Tid, int Amount, VName DmgType, VEntity *Activator)
  {
    P_PASS_SELF;
    P_PASS_INT(Tid);
    P_PASS_INT(Amount);
    P_PASS_NAME(DmgType);
    P_PASS_REF(Activator);
    EV_RET_INT(NAME_DoThingDamage);
  }
  void eventSetMarineWeapon(int Tid, int Weapon, VEntity *Activator)
  {
    P_PASS_SELF;
    P_PASS_INT(Tid);
    P_PASS_INT(Weapon);
    P_PASS_REF(Activator);
    EV_RET_VOID(NAME_SetMarineWeapon);
  }
  void eventSetMarineSprite(int Tid, VName SrcClass, VEntity *Activator)
  {
    P_PASS_SELF;
    P_PASS_INT(Tid);
    P_PASS_NAME(SrcClass);
    P_PASS_REF(Activator);
    EV_RET_VOID(NAME_SetMarineSprite);
  }
  void eventAcsFadeRange(float BlendR1, float BlendG1, float BlendB1,
    float BlendA1, float BlendR2, float BlendG2, float BlendB2,
    float BlendA2, float Duration, VEntity *Activator)
  {
    P_PASS_SELF;
    P_PASS_FLOAT(BlendR1);
    P_PASS_FLOAT(BlendG1);
    P_PASS_FLOAT(BlendB1);
    P_PASS_FLOAT(BlendA1);
    P_PASS_FLOAT(BlendR2);
    P_PASS_FLOAT(BlendG2);
    P_PASS_FLOAT(BlendB2);
    P_PASS_FLOAT(BlendA2);
    P_PASS_FLOAT(Duration);
    P_PASS_REF(Activator);
    EV_RET_VOID(NAME_AcsFadeRange);
  }
  void eventAcsCancelFade(VEntity *Activator)
  {
    P_PASS_SELF;
    P_PASS_REF(Activator);
    EV_RET_VOID(NAME_AcsCancelFade);
  }
};
