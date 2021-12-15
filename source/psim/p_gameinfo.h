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
#ifndef VAVOOM_PSIM_GAMEINFO_HEADER
#define VAVOOM_PSIM_GAMEINFO_HEADER


// network mode
enum {
  NM_None, // not running a game
  NM_TitleMap, // playing a titlemap
  NM_Standalone, // standalone single player game
  NM_DedicatedServer, // dedicated server, no local client
  NM_ListenServer, // server with local client
  NM_Client, // client only, no local server
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


// ////////////////////////////////////////////////////////////////////////// //
/*
struct cptrace_t {
  TVec Pos;
  float BBox[4];
  float FloorZ;
  float CeilingZ;
  float DropOffZ;
  sec_plane_t *EFloor;
  sec_plane_t *ECeiling;
};
*/

struct tmtrace_t {
  VEntity *StepThing; // not for cptrace_t
  TVec End; // for cptrace_t, this is `Pos`
  float BBox[4]; // valid for cptrace_t
  float FloorZ; // valid for cptrace_t
  float CeilingZ; // valid for cptrace_t
  float DropOffZ; // valid for cptrace_t

  // WARNING! keep in sync with VEntity fcflags
  /*
  enum {
    FC_FlipFloor = 1u<<0,
    FC_FlipCeiling = 1u<<1,
  };
  vuint32 fcflags; // valid for cptrace_t
  */
  TSecPlaneRef EFloor; // valid for cptrace_t
  TSecPlaneRef ECeiling; // valid for cptrace_t

  enum {
    TF_FloatOk = 0x01u, // if true, move would be ok if within tmtrace.FloorZ - tmtrace.CeilingZ
  };
  vuint32 TraceFlags;

  // keep track of the line that lowers the ceiling,
  // so missiles don't explode against sky hack walls
  line_t *CeilingLine;
  line_t *FloorLine;
  // also keep track of the blocking line, for checking
  // against doortracks
  line_t *BlockingLine; // only lines without backsector

  // keep track of special lines as they are hit,
  // but don't process them until the move is proven valid
  TArrayNC<line_t *> SpecHit;

  VEntity *BlockingMobj;
  // any blocking line (including passable two-sided!); only has any sense if trace returned `false`
  // note that this is really *any* line, not necessarily first or last crossed!
  line_t *AnyBlockingLine;

  // polyobject we are standing on, valid for cptrace_t
  polyobj_t *PolyObj;

  // from cptrace_t
  //TVec Pos; // valid for cptrace_t

  inline void setupGap (VLevel *XLevel, sector_t *sector, float Height) {
    XLevel->FindGapFloorCeiling(sector, End, Height, EFloor, ECeiling);
    FloorZ = DropOffZ = EFloor.GetPointZClamped(End);
    CeilingZ = ECeiling.GetPointZClamped(End);
  }

  /*
  inline void CopyRegFloor (sec_region_t *r, const TVec *Origin) {
    EFloor = r->efloor;
    if (Origin) FloorZ = EFloor.GetPointZClamped(*Origin);
  }

  inline void CopyRegCeiling (sec_region_t *r, const TVec *Origin) {
    ECeiling = r->eceiling;
    if (Origin) CeilingZ = ECeiling.GetPointZClamped(*Origin);
  }
  */

  inline void CopyOpenFloor (opening_t *o, bool setz=true) noexcept {
    EFloor = o->efloor;
    if (setz) FloorZ = o->bottom;
  }

  inline void CopyOpenCeiling (opening_t *o, bool setz=true) noexcept {
    ECeiling = o->eceiling;
    if (setz) CeilingZ = o->top;
  }
};


// ////////////////////////////////////////////////////////////////////////// //
class VGameInfo : public VGameObject {
  DECLARE_CLASS(VGameInfo, VGameObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VGameInfo)

  // client flags
  enum {
    CLF_RUN_DISABLED    = 1u<<0,
    CLF_MLOOK_DISABLED  = 1u<<1,
    CLF_CROUCH_DISABLED = 1u<<2,
    CLF_JUMP_DISABLED   = 1u<<3,
  };

  VName AcsHelper;
  VName GenericConScript;

  vuint8 NetMode;
  vuint8 deathmatch;
  vuint8 respawn;
  vuint8 nomonsters;
  vuint8 fastparm; // 0:normal; 1:fast; 2:slow
  // the following flags are valid only for `NM_Client`
  vuint32 clientFlags; // see `CLF_XXX`

  //vint32 *validcount;
  vint32 skyflatnum;

  VWorldInfo *WorldInfo;

  VBasePlayer *Players[MAXPLAYERS]; // bookkeeping on players (state)

  vint32 RebornPosition;

  float frametime;

  float FloatBobOffsets[64];
  vint32 PhaseTable[64];

  VClass *LevelInfoClass;
  VClass *PlayerReplicationInfoClass;

  vint32 GameFilterFlag;

  TArray<VClass *> PlayerClasses;

  enum {
    GIF_DefaultLaxMonsterActivation = 1u<<0,
    GIF_DefaultBloodSplatter        = 1u<<1,
    GIF_Paused                      = 1u<<2,
    GIF_ForceKillScripts            = 1u<<3,
  };
  vuint32 Flags;

  TArray<VDamageFactor> CustomDamageFactors;

public:
  enum { PlrAll = 0, PlrOnlySpawned = 1 };

  struct PlayersIter {
  private:
    VGameInfo *gi;
    int plridx;
    int type;

    void nextByType () noexcept;

  public:
    inline PlayersIter (int atype, VGameInfo *agi) noexcept : gi(agi), plridx(-1), type(atype) { nextByType(); }
    inline PlayersIter (const PlayersIter &src) noexcept : gi(src.gi), plridx(src.plridx), type(src.type) {}
    inline PlayersIter (const PlayersIter &src, bool /*atEnd*/) noexcept : gi(src.gi), plridx(MAXPLAYERS), type(src.type) {}

    inline PlayersIter begin () noexcept { return PlayersIter(*this); }
    inline PlayersIter end () noexcept { return PlayersIter(*this, true); }
    inline bool operator == (const PlayersIter &b) const noexcept { return (gi == b.gi && plridx == b.plridx && type == b.type); }
    inline bool operator != (const PlayersIter &b) const noexcept { return (gi != b.gi || plridx != b.plridx || type != b.type); }
    inline PlayersIter operator * () const noexcept { return PlayersIter(*this); } /* required for iterator */
    inline void operator ++ () noexcept { if (plridx < MAXPLAYERS) nextByType(); } /* this is enough for iterator */
    // accessors
    inline int index () const noexcept { return plridx; }
    inline VBasePlayer *value () const noexcept { return gi->Players[plridx]; }
    inline VBasePlayer *player () const noexcept { return gi->Players[plridx]; }
  };

  PlayersIter playersAll () { return PlayersIter(PlrAll, this); }
  PlayersIter playersSpawned () { return PlayersIter(PlrOnlySpawned, this); }

public:
  //VGameInfo ();

  bool IsPaused (bool ignoreOpenConsole=false);
  bool IsInWipe ();
  bool IsWipeAllowed ();

  void eventInit () { static VMethodProxy method("Init"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventPostDecorateInit () { static VMethodProxy method("PostDecorateInit"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventPostPlayerClassInit () { static VMethodProxy method("PostPlayerClassInit"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventInitNewGame (int skill) { static VMethodProxy method("InitNewGame"); vobjPutParamSelf(skill); VMT_RET_VOID(method); }
  VWorldInfo *eventCreateWorldInfo () { static VMethodProxy method("CreateWorldInfo"); vobjPutParamSelf(); VMT_RET_REF(VWorldInfo, method); }
  void eventTranslateLevel (VLevel *InLevel) { static VMethodProxy method("TranslateLevel"); vobjPutParamSelf(InLevel); VMT_RET_VOID(method); }
  void eventTranslateSpecialActions (VLevel *InLevel, VLevelInfo *Level) { static VMethodProxy method("TranslateSpecialActions"); vobjPutParamSelf(InLevel, Level); VMT_RET_VOID(method); }
  void eventSpawnWorld (VLevel *InLevel) { static VMethodProxy method("SpawnWorld"); vobjPutParamSelf(InLevel); VMT_RET_VOID(method); }
  VName eventGetConScriptName (VName LevelName) { static VMethodProxy method("GetConScriptName"); vobjPutParamSelf(LevelName); VMT_RET_NAME(method); }
  void eventCmdWeaponSection (VStr Section) { static VMethodProxy method("CmdWeaponSection"); vobjPutParamSelf(Section); VMT_RET_VOID(method); }
  void eventCmdSetSlot (TArray<VStr> *Args, bool asKeyconf) { static VMethodProxy method("CmdSetSlot"); vobjPutParamSelf((void *)Args, asKeyconf); VMT_RET_VOID(method); }
  void eventCmdAddSlotDefault (TArray<VStr> *Args, bool asKeyconf) { static VMethodProxy method("CmdAddSlotDefault"); vobjPutParamSelf((void *)Args, asKeyconf); VMT_RET_VOID(method); }

  DECLARE_FUNCTION(get_isPaused)
  DECLARE_FUNCTION(get_isInWipe)
};


extern VGameInfo *GGameInfo;


#endif
