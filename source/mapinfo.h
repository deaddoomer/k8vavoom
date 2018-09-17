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

enum
{
  MAPINFOF_DoubleSky          = 0x00000001, // parallax sky: sky2 behind sky1
  MAPINFOF_Lightning          = 0x00000002, // Use of lightning on the level flashes from sky1 to sky2
  MAPINFOF_Map07Special       = 0x00000004,
  MAPINFOF_BaronSpecial       = 0x00000008,
  MAPINFOF_CyberDemonSpecial      = 0x00000010,
  MAPINFOF_SpiderMastermindSpecial  = 0x00000020,
  MAPINFOF_MinotaurSpecial      = 0x00000040,
  MAPINFOF_DSparilSpecial       = 0x00000080,
  MAPINFOF_IronLichSpecial      = 0x00000100,
  MAPINFOF_SpecialActionOpenDoor    = 0x00000200,
  MAPINFOF_SpecialActionLowerFloor  = 0x00000400,
  MAPINFOF_SpecialActionKillMonsters  = 0x00000800,
  MAPINFOF_NoIntermission       = 0x00001000,
  MAPINFOF_AllowMonsterTelefrags    = 0x00002000,
  MAPINFOF_NoAllies         = 0x00004000,
  MAPINFOF_DeathSlideShow       = 0x00008000,
  MAPINFOF_ForceNoSkyStretch      = 0x00010000,
  MAPINFOF_LookupName         = 0x00020000,
  MAPINFOF_FallingDamage        = 0x00040000,
  MAPINFOF_OldFallingDamage     = 0x00080000,
  MAPINFOF_StrifeFallingDamage    = 0x00100000,
  MAPINFOF_MonsterFallingDamage   = 0x00200000,
  MAPINFOF_NoFreelook         = 0x00400000,
  MAPINFOF_NoJump           = 0x00800000,
  MAPINFOF_NoAutoSndSeq       = 0x01000000,
  MAPINFOF_ActivateOwnSpecial     = 0x02000000,
  MAPINFOF_MissilesActivateImpact   = 0x04000000,
  MAPINFOF_FilterStarts       = 0x08000000,
  MAPINFOF_InfiniteFlightPowerup    = 0x10000000,
  MAPINFOF_ClipMidTex         = 0x20000000,
  MAPINFOF_WrapMidTex         = 0x40000000,
  MAPINFOF_KeepFullInventory      = 0x80000000,

  MAPINFOF2_CompatShortTex      = 0x00000001,
  MAPINFOF2_CompatStairs        = 0x00000002,
  MAPINFOF2_CompatLimitPain     = 0x00000004,
  MAPINFOF2_CompatNoPassOver      = 0x00000008,
  MAPINFOF2_CompatNoTossDrops     = 0x00000010,
  MAPINFOF2_CompatUseBlocking     = 0x00000020,
  MAPINFOF2_CompatNoDoorLight     = 0x00000040,
  MAPINFOF2_CompatRavenScroll     = 0x00000080,
  MAPINFOF2_CompatSoundTarget     = 0x00000100,
  MAPINFOF2_CompatDehHealth     = 0x00000200,
  MAPINFOF2_CompatTrace       = 0x00000400,
  MAPINFOF2_CompatDropOff       = 0x00000800,
  MAPINFOF2_CompatBoomScroll      = 0x00001000,
  MAPINFOF2_CompatInvisibility    = 0x00002000,
  MAPINFOF2_LaxMonsterActivation    = 0x00004000,
  MAPINFOF2_HaveMonsterActivation   = 0x00008000,
};

struct VMapSpecialAction
{
  VName   TypeName;
  vint32    Special;
  vint32    Args[5];
};

struct mapInfo_t
{
  VName   LumpName;
  VStr    Name;     // Name of map
  vint32    LevelNum;   // Level number for action specials
  vint32    Cluster;    // Defines what cluster level belongs to
  vint32    WarpTrans;    // Actual map number in case maps are not sequential
  VName   NextMap;    // Map to teleport to upon exit of timed deathmatch
  VName   SecretMap;    // Map to teleport upon secret exit
  VName   SongLump;   // Background music (MUS or MIDI)
  vint32    Sky1Texture;  // Default sky texture
  vint32    Sky2Texture;  // Alternate sky displayed in Sky2 sectors
  float   Sky1ScrollDelta;// Default sky texture speed
  float   Sky2ScrollDelta;// Alternate sky texture speed
  VName   SkyBox;     // Sky box
  VName   FadeTable;    // Fade table {fogmap}
  vuint32   Fade;
  vuint32   OutsideFog;
  float   Gravity;    // Map gravity
  float   AirControl;   // Air control in this map.
  vuint32   Flags;
  vuint32   Flags2;
  VName   TitlePatch;
  vint32    ParTime;
  vint32    SuckTime;
  vint8   HorizWallShade;
  vint8   VertWallShade;
  vint8   Infighting;
  TArray<VMapSpecialAction> SpecialActions;
  VName   RedirectType;
  VName   RedirectMap;
  VName   ExitPic;
  VName   EnterPic;
  VName   InterMusic;

  VStr GetName() const
  {
    return Flags & MAPINFOF_LookupName ? GLanguage[*Name] : Name;
  }
};

enum
{
  CLUSTERF_Hub        = 0x01,
  CLUSTERF_EnterTextIsLump  = 0x02,
  CLUSTERF_ExitTextIsLump   = 0x04,
  CLUSTERF_FinalePic      = 0x80,
  CLUSTERF_LookupEnterText  = 0x10,
  CLUSTERF_LookupExitText   = 0x20,
};

struct VClusterDef
{
  vint32    Cluster;
  vint32    Flags;
  VStr    EnterText;
  VStr    ExitText;
  VName   Flat;
  VName   Music;
};

enum
{
  EPISODEF_LookupText   = 0x0001,
  EPISODEF_NoSkillMenu  = 0x0002,
  EPISODEF_Optional   = 0x0004,
};

struct VEpisodeDef
{
  VName   Name;
  VName   TeaserName;
  VStr    Text;
  VName   PicName;
  vuint32   Flags;
  VStr    Key;
};

enum
{
  SKILLF_FastMonsters     = 0x00000001,
  SKILLF_DisableCheats    = 0x00000002,
  SKILLF_EasyBossBrain    = 0x00000004,
  SKILLF_AutoUseHealth    = 0x00000008,
  SKILLF_MenuNameIsPic    = 0x00000010,
  SKILLF_MustConfirm      = 0x00000020,
};

struct VSkillPlayerClassName
{
  VStr    ClassName;
  VStr    MenuName;
};

struct VSkillDef
{
  VStr    Name;
  float   AmmoFactor;
  float   DoubleAmmoFactor;
  float   DamageFactor;
  float   RespawnTime;
  int     RespawnLimit;
  float   Aggressiveness;
  int     SpawnFilter;
  int     AcsReturn;
  VStr    MenuName;
  TArray<VSkillPlayerClassName> PlayerClassNames;
  VStr    ConfirmationText;
  VStr    Key;
  VStr    TextColour;
  vuint32   Flags;
};

void InitMapInfo();
void ShutdownMapInfo();
const mapInfo_t &P_GetMapInfo(VName);
VStr P_GetMapName(int);
VName P_GetMapLumpName(int);
int P_GetMapIndexByLevelNum(int);
VName P_TranslateMap(int);
VName P_GetMapLumpNameByLevelNum(int);
void P_PutMapSongLump(int, VName);
const VClusterDef *P_GetClusterDef(int);
int P_GetNumEpisodes();
int P_GetNumMaps();
mapInfo_t *P_GetMapInfoPtr(int mapidx);
VEpisodeDef *P_GetEpisodeDef(int);
int P_GetNumSkills();
const VSkillDef *P_GetSkillDef(int);
void P_GetMusicLumpNames(TArray<FReplacedString>&);
void P_ReplaceMusicLumpNames(TArray<FReplacedString>&);
void P_SetParTime(VName, int);
int P_GetCDStartTrack();
int P_GetCDEnd1Track();
int P_GetCDEnd2Track();
int P_GetCDEnd3Track();
int P_GetCDIntermissionTrack();
int P_GetCDTitleTrack();
bool IsMapPresent(VName);
