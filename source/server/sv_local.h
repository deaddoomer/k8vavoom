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
#ifndef SV_LOCAL_HEADER
#define SV_LOCAL_HEADER

#define MAXHEALTH        (100)
#define DEFAULT_GRAVITY  (1225.0f)

//#define REBORN_DESCRIPTION    "TEMP GAME"

class VMessageOut;

extern VLevelInfo *GLevelInfo;


// ////////////////////////////////////////////////////////////////////////// //
// flags for SV_MapTeleport
enum {
  CHANGELEVEL_KEEPFACING      = 0x00000001,
  CHANGELEVEL_RESETINVENTORY  = 0x00000002,
  CHANGELEVEL_NOMONSTERS      = 0x00000004,
  CHANGELEVEL_CHANGESKILL     = 0x00000008,
  CHANGELEVEL_NOINTERMISSION  = 0x00000010,
  CHANGELEVEL_RESETHEALTH     = 0x00000020,
  CHANGELEVEL_PRERAISEWEAPON  = 0x00000040,
  // k8
  CHANGELEVEL_REMOVEKEYS      = 0x00100000,
};


//==========================================================================
//
//  sv_acs
//
//  Action code scripts
//
//==========================================================================

// script types
enum {
  SCRIPT_Closed      = 0,
  SCRIPT_Open        = 1,
  SCRIPT_Respawn     = 2,
  SCRIPT_Death       = 3,
  SCRIPT_Enter       = 4,
  SCRIPT_Pickup      = 5, // not implemented
  SCRIPT_BlueReturn  = 6, // not implemented
  SCRIPT_RedReturn   = 7, // not implemented
  SCRIPT_WhiteReturn = 8, // not implemented
  SCRIPT_Lightning   = 12,
  SCRIPT_Unloading   = 13,
  SCRIPT_Disconnect  = 14,
  SCRIPT_Return      = 15,
  SCRIPT_Event       = 16, // not implemented
  SCRIPT_Kill        = 17,
  SCRIPT_Reopen      = 18, // not implemented
};


class VAcs;
class VAcsObject;
struct VAcsInfo;


// ////////////////////////////////////////////////////////////////////////// //
class VAcsLevel {
private:
  TMap<VStr, int> stringMapByStr;
  TArray<VStr> stringList;
  TMapNC<int, bool> unknownScripts;

private:
  bool AddToACSStore (int Type, VName Map, int Number, int Arg1, int Arg2, int Arg3, int Arg4, VEntity *Activator);

public:
  VLevel *XLevel;

  TArray<VAcsObject *> LoadedObjects;
  //TArray<int> LoadedStaticObjects; // index in `LoadedObjects`

public:
  VAcsLevel (VLevel *ALevel);
  ~VAcsLevel ();

  VAcsObject *LoadObject (int Lump);
  VAcsInfo *FindScript (int Number, VAcsObject *&Object);
  VAcsInfo *FindScriptByName (int Number, VAcsObject *&Object);
  int FindScriptNumberByName (VStr aname, VAcsObject *&Object);
  VAcsInfo *FindScriptByNameStr (VStr aname, VAcsObject *&Object);
  VStr GetString (int Index);
  VName GetNameLowerCase (int Index);
  VAcsObject *GetObject (int Index);
  void StartTypedACScripts (int Type, int Arg1, int Arg2, int Arg3,
                            VEntity *Activator, bool Always, bool RunNow);
  void Serialise (VStream &Strm);
  void CheckAcsStore ();
  bool Start (int Number, int MapNum, int Arg1, int Arg2, int Arg3, int Arg4,
              VEntity *Activator, line_t *Line, int Side, bool Always,
              bool WantResult, bool Net = false, int *realres=nullptr);
  bool Terminate (int Number, int MapNum);
  bool Suspend (int Number, int MapNum);
  VAcs *SpawnScript (VAcsInfo *Info, VAcsObject *Object, VEntity *Activator,
                     line_t *Line, int Side, int Arg1, int Arg2, int Arg3, int Arg4,
                     bool Always, bool Delayed, bool ImmediateRun);

  VStr GetNewString (int idx);
  VName GetNewLowerName (int idx);
  int PutNewString (VStr str);

public: // debug
  static VStr GenScriptName (int Number);
};


// ////////////////////////////////////////////////////////////////////////// //
class VAcsGrowingArray {
private:
  TMapNC<int, int> values; // index, value

public:
  VAcsGrowingArray () : values() {}

  inline void clear () { values.clear(); }

  inline void SetElemVal (int index, int value) { values.put(index, value); }
  inline int GetElemVal (int index) const { auto vp = values.get(index); return (vp ? *vp : 0); }

  void Serialise (VStream &Strm);
};

static inline __attribute((unused)) VStream &operator << (VStream &strm, VAcsGrowingArray &arr) { arr.Serialise(strm); return strm; }


// ////////////////////////////////////////////////////////////////////////// //
struct VAcsStore {
  enum {
    Start,
    StartAlways,
    Terminate,
    Suspend
  };

  VName Map; // target map
  vuint8 Type; // type of action
  vint8 PlayerNum; // player who executes this script
  vint32 Script; // script number on target map
  vint32 Args[4]; // arguments

  void Serialise (VStream &Strm);
};

static inline __attribute((unused)) VStream &operator << (VStream &Strm, VAcsStore &Store) { Store.Serialise(Strm); return Strm; }


class VAcsGlobal {
private:
  enum {
    MAX_ACS_WORLD_VARS  = 256,
    MAX_ACS_GLOBAL_VARS = 64,
  };

  VAcsGrowingArray WorldVars;
  VAcsGrowingArray GlobalVars;
  VAcsGrowingArray WorldArrays[MAX_ACS_WORLD_VARS];
  VAcsGrowingArray GlobalArrays[MAX_ACS_GLOBAL_VARS];

public:
  TArray<VAcsStore> Store;

public:
  VAcsGlobal ();

  void Serialise (VStream &Strm);

  VStr GetGlobalVarStr (VAcsLevel *level, int index) const;

  int GetGlobalVarInt (int index) const;
  float GetGlobalVarFloat (int index) const;
  void SetGlobalVarInt (int index, int value);
  void SetGlobalVarFloat (int index, float value);

  int GetWorldVarInt (int index) const;
  float GetWorldVarFloat (int index) const;
  void SetWorldVarInt (int index, int value);
  void SetWorldVarFloat (int index, float value);

  int GetGlobalArrayInt (int aidx, int index) const;
  float GetGlobalArrayFloat (int aidx, int index) const;
  void SetGlobalArrayInt (int aidx, int index, int value);
  void SetGlobalArrayFloat (int aidx, int index, float value);

  int GetWorldArrayInt (int aidx, int index) const;
  float GetWorldArrayFloat (int aidx, int index) const;
  void SetWorldArrayInt (int aidx, int index, int value);
  void SetWorldArrayFloat (int aidx, int index, float value);
};


//==========================================================================
//
//  sv_switch
//
//  Switches
//
//==========================================================================
void P_InitSwitchList ();

struct VTerrainInfo;
void P_InitTerrainTypes ();
VTerrainInfo *SV_TerrainType (int pic, bool asPlayer);
VTerrainBootprint *SV_TerrainBootprint (int pic);
VTerrainBootprint *SV_FindBootprintByName (const char *name);
VTerrainInfo *SV_GetDefaultTerrain ();
void P_FreeTerrainTypes ();


//==========================================================================
//
//  sv_main
//
//==========================================================================
void SV_ReadMove ();

void Draw_TeleportIcon ();

void SV_DropClient (VBasePlayer *Player, bool crash);
void SV_SpawnServer (const char *mapname, bool spawn_thinkers, bool titlemap=false);
void SV_SendServerInfoToClients ();

// call after texture manager updated a flat
void SV_UpdateSkyFlat ();

extern int LeavePosition;
extern bool completed;


//==========================================================================
//
//  ????
//
//==========================================================================

void G_TeleportNewMap (int map, int position);
void G_WorldDone ();
//void G_PlayerReborn (int player);
void G_StartNewInit ();

bool G_CheckWantExitText ();
bool G_CheckFinale ();
bool G_StartClientFinale ();


extern VBasePlayer *GPlayersBase[MAXPLAYERS]; // Bookkeeping on players state

extern bool sv_loading;
extern bool sv_map_travel;
extern int sv_load_num_players;
extern bool run_open_scripts;


//==========================================================================
//
//  inlines
//
//==========================================================================
static inline VVA_OKUNUSED int SV_GetPlayerNum (VBasePlayer *player) {
  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (player == GPlayersBase[i]) return i;
  }
  return 0;
}

static inline VVA_OKUNUSED VBasePlayer *SV_GetPlayerByNum (int pidx) {
  if (pidx < 0 || pidx >= MAXPLAYERS) return nullptr;
  return GPlayersBase[pidx];
}


//==========================================================================
//
//  GetLevelObject
//
//  have to do this, because this function can be called
//  both in server and in client
//
//==========================================================================
/*
static inline VVA_OKUNUSED VVA_CHECKRESULT VLevel *GetLevelObject () noexcept {
  return (GClLevel ? GClLevel : GLevel);
}
*/


#endif
