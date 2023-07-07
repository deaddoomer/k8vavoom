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
#ifndef VAVOOM_MAPINFO_HEADER
#define VAVOOM_MAPINFO_HEADER


struct VMapSpecialAction {
  VName TypeName; // class name
  vint32 Special; // negative means "need to translate it"
  vint32 Args[5];
};

struct VMapInfo {
  VName LumpName;
  VStr Name; // name of map
  vint32 LevelNum; // level number for action specials
  vint32 Cluster; // defines what cluster level belongs to
  vint32 WarpTrans; // actual map number in case maps are not sequential
  VName NextMap; // map to teleport to upon exit of timed deathmatch
  VName SecretMap; // map to teleport upon secret exit
  VName SongLump; // background music (MUS or MIDI)
  vint32 Sky1Texture; // default sky texture
  vint32 Sky2Texture; // alternate sky displayed in Sky2 sectors
  float Sky1ScrollDelta; // default sky texture speed
  float Sky2ScrollDelta; // alternate sky texture speed
  VName SkyBox; // sky box
  VName FadeTable; // fade table {fogmap}
  vuint32 Fade;
  vuint32 OutsideFog;
  float Gravity; // map gravity
  float AirControl; // air control in this map.
  vuint32 Flags; // for values, see `VLevelInfo::LIF_XXX`
  vuint32 Flags2; // for values, see `VLevelInfo::LIF2_XXX`
  VName EnterTitlePatch;
  VName ExitTitlePatch; // if empty, `EnterTitlePatch` will be used
  vint32 ParTime;
  vint32 SuckTime;
  vint8 HorizWallShade;
  vint8 VertWallShade;
  vint8 Infighting;
  TArray<VMapSpecialAction> SpecialActions;
  VName RedirectType;
  VName RedirectMap;
  VName ExitPic;
  VName EnterPic;
  VName InterMusic;
  vint32 MapinfoSourceLump;
  vint32 FakeContrast; // 0: default; 1: smooth; 2: disabled (even)
  // exit texts will override cluster exit texts
  VStr ExitText;
  VStr SecretExitText;
  VName InterBackdrop; // used when we have exit test, set in umapinfo parser

  VStr GetName () const;
  void dump (const char *msg=nullptr) const;
};

enum {
  CLUSTERF_Hub             = 0x01,
  CLUSTERF_EnterTextIsLump = 0x02,
  CLUSTERF_ExitTextIsLump  = 0x04,
  CLUSTERF_FinalePic       = 0x80,
  CLUSTERF_LookupEnterText = 0x10,
  CLUSTERF_LookupExitText  = 0x20,
};

struct VClusterDef {
  vint32 Cluster;
  vint32 Flags;
  VStr EnterText;
  VStr ExitText;
  VName Flat;
  VName Music;

  inline VClusterDef () noexcept
    : Cluster(0)
    , Flags(0)
    , EnterText()
    , ExitText()
    , Flat(NAME_None)
    , Music(NAME_None)
  {}

  inline void reset () noexcept {
    Cluster = 0;
    Flags = 0;
    EnterText.clear();
    ExitText.clear();
    Flat = NAME_None;
    Music = NAME_None;
  }
};

enum {
  EPISODEF_LookupText  = 0x0001,
  EPISODEF_NoSkillMenu = 0x0002,
  EPISODEF_Optional    = 0x0004,
};

struct VEpisodeDef {
  VName Name;
  VName TeaserName;
  VStr Text;
  VName PicName;
  vuint32 Flags;
  VStr Key;
  vint32 MapinfoSourceLump;
};

enum {
  SKILLF_FastMonsters  = 0x00000001,
  SKILLF_DisableCheats = 0x00000002,
  SKILLF_EasyBossBrain = 0x00000004,
  SKILLF_AutoUseHealth = 0x00000008,
  SKILLF_MenuNameIsPic = 0x00000010,
  SKILLF_MustConfirm   = 0x00000020,
  SKILLF_SlowMonsters  = 0x00000040,
  SKILLF_SpawnMulti    = 0x00000080,
};

struct VSkillPlayerClassName {
  VStr ClassName;
  VStr MenuName;
};

// both must be at least `EntityEx`, and compatible
struct VSkillMonsterReplacement {
  VClass *oldClass;
  VClass *newClass;
};

struct VSkillDef {
  VStr Name;
  float AmmoFactor;
  float DoubleAmmoFactor;
  float DamageFactor;
  float RespawnTime;
  vint32 RespawnLimit;
  float Aggressiveness;
  vint32 SpawnFilter;
  vint32 AcsReturn;
  float MonsterHealth;
  float HealthFactor;
  VStr MenuName;
  TArray<VSkillPlayerClassName> PlayerClassNames;
  VStr ConfirmationText;
  VStr Key;
  VStr TextColor;
  vuint32 Flags;
  // monster replacements for each skill
  // WARNING! currently it works only for predefined map spawns!
  TArray<VSkillMonsterReplacement> Replacements;
};


void InitMapInfo ();
void ShutdownMapInfo ();
const VMapInfo &P_GetMapInfo (VName);
VStr P_GetMapName (int);
VName P_GetMapLumpName (int);
int P_GetMapIndexByLevelNum (int);
VName P_TranslateMap (int);
VName P_TranslateMapEx (int); // returns `NAME_None` if not found
VName P_GetMapLumpNameByLevelNum (int);
void P_PutMapSongLump (int, VName);
const VClusterDef *P_GetClusterDef (int);
int P_GetNumEpisodes ();
int P_GetNumMaps ();
VMapInfo *P_GetMapInfoPtr (int mapidx);
VEpisodeDef *P_GetEpisodeDef (int);
int P_GetNumSkills ();
const VSkillDef *P_GetSkillDef (int);
void P_GetMusicLumpNames (TArray<FReplacedString> &);
void P_ReplaceMusicLumpNames (TArray<FReplacedString> &);
void P_SetParTime (VName, int);
bool IsMapPresent (VName);

void P_SetupMapinfoPlayerClasses ();


#endif
