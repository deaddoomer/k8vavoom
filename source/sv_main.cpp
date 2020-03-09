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
//**  Copyright (C) 2018-2020 Ketmar Dark
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
#include "gamedefs.h"
#include "net/network.h"
#include "sv_local.h"
#include "cl_local.h"
#ifdef CLIENT
# include "drawer.h"
#endif

#ifndef CLIENT
// this is for BDW mod, to have tracers
VCvarB r_models("r_models", true, "Allow 3d models?", 0/*CVAR_Archive*/);
#endif


// arbitrary number
#define INITIAL_TICK_DELAY  (2)

bool sv_skipOneTitlemap = false;
int serverStartRenderFramesTic = -1;

/*
static double FrameTime = 1.0f/35.0f;
// round a little bit up to prevent "slow motion"
*(vuint64 *)&FrameTime += 1;
*/
static constexpr double FrameTime = 0x1.d41d41d41d41ep-6; // same as above
double SV_GetFrameTimeConstant () { return FrameTime; }

/*
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
int main () {
  double FrameTime = 1.0/35.0;
  printf("%a %.21g 0x%08llx\n", FrameTime, FrameTime, *(const uint64_t *)&FrameTime);
  // round a little bit up to prevent "slow motion"
  *(uint64_t *)&FrameTime += 1;
  printf("%a %.21g 0x%08llx\n", FrameTime, FrameTime, *(const uint64_t *)&FrameTime);
  *(uint64_t *)&FrameTime += 1;
  printf("%a %.21g 0x%08llx\n", FrameTime, FrameTime, *(const uint64_t *)&FrameTime);
  printf("---\n");
  FrameTime = 0x1.d41d41d41d41dp-6;
  printf("%a %.21g 0x%08llx\n", FrameTime, FrameTime, *(const uint64_t *)&FrameTime);
  FrameTime = 0x1.d41d41d41d41ep-6;
  printf("%a %.21g 0x%08llx\n", FrameTime, FrameTime, *(const uint64_t *)&FrameTime);
  return 0;
}
*/

static double ServerLastKeepAliveTime = 0.0;


static int cli_SVDumpDoomEd = 0;
static int cli_SVDumpScriptId = 0;
static int cli_SVShowExecTimes = 0;
static int cli_SVNoTitleMap = 0;

/*static*/ bool cliRegister_svmain_args =
  VParsedArgs::RegisterFlagSet("-dbg-dump-doomed", "!dump doomed numbers", &cli_SVDumpDoomEd) &&
  VParsedArgs::RegisterFlagSet("-dbg-dump-scriptid", "!dump scriptid numbers", &cli_SVDumpScriptId) &&
  VParsedArgs::RegisterFlagSet("-show-exec-times", "!show some developer info", &cli_SVShowExecTimes) &&
  VParsedArgs::RegisterFlagSet("-notitlemap", "Do not load and run TITLEMAP", &cli_SVNoTitleMap);


static void G_DoReborn (int playernum, bool cheatReborn);
static void G_DoCompleted (bool ignoreNoExit);

extern VCvarB dbg_vm_disable_thinkers;

static VCvarB dbg_skipframe_player_tick("dbg_skipframe_player_tick", true, "Run player ticks on skipped frames?", CVAR_PreInit);
static VCvarB dbg_skipframe_player_block_move("dbg_skipframe_player_block_move", false, "Keep moving on skipped player frames (this is wrong)?", CVAR_PreInit);
static VCvarB dbg_report_orphan_weapons("dbg_report_orphan_weapons", false, "Report orphan weapon assign?", CVAR_Archive|CVAR_PreInit);

VCvarB real_time("real_time", true, "Run server in real time?");

static VCvarB sv_ignore_nojump("sv_ignore_nojump", false, "Ignore \"nojump\" flag in MAPINFO?", /*CVAR_ServerInfo|CVAR_Latch|*/CVAR_PreInit);
static VCvarB sv_ignore_nocrouch("sv_ignore_nocrouch", false, "Ignore \"nocrouch\" flag in MAPINFO?", /*CVAR_ServerInfo|CVAR_Latch|*/CVAR_PreInit);
/*static*/ VCvarB sv_ignore_nomlook("sv_ignore_nomlook", false, "Ignore \"nofreelook\" flag in MAPINFO?", /*CVAR_ServerInfo|CVAR_Latch|*/CVAR_PreInit);
static VCvarB sv_ignore_reset_health("sv_ignore_reset_health", false, "Ignore \"resethealth\" flag in MAPINFO?", /*CVAR_ServerInfo|CVAR_Latch|*/CVAR_PreInit);
static VCvarB sv_ignore_reset_inventory("sv_ignore_reset_inventory", false, "Ignore \"resetinventory\" flag in MAPINFO?", /*CVAR_ServerInfo|CVAR_Latch|*/CVAR_PreInit);
static VCvarB sv_ignore_reset_items("sv_ignore_reset_items", false, "Ignore \"resetitems\" flag in MAPINFO?", /*CVAR_ServerInfo|CVAR_Latch|*/CVAR_PreInit);

static VCvarB sv_force_pistol_start("sv_force_pistol_start", false, "Start each new map with default weapons?", /*CVAR_ServerInfo|CVAR_Latch|*/CVAR_PreInit);
static VCvarB sv_force_health_reset("sv_force_health_reset", false, "Start each new map with default health?", /*CVAR_ServerInfo|CVAR_Latch|*/CVAR_PreInit);

static VCvarB sv_transporters_absolute("sv_transporters_absolute", true, "Use absolute movement instead of velocity for Boom transporters?", /*CVAR_ServerInfo|CVAR_Latch|*/CVAR_Archive|CVAR_PreInit);

static VCvarB mod_allow_server_cvars("mod_allow_server_cvars", false, "Allow server cvars from CVARINFO?", CVAR_Archive|CVAR_PreInit);

static VCvarB cl_disable_crouch("cl_disable_crouch", false, "Disable crouching?", CVAR_Archive);
static VCvarB cl_disable_jump("cl_disable_jump", false, "Disable jumping?", CVAR_Archive);
static VCvarB cl_disable_mlook("cl_disable_mlook", false, "Disable mouselook?", CVAR_Archive);

extern VCvarI host_max_skip_frames;
extern VCvarB NoExit;

static VCvarB __dbg_cl_always_allow_pause("__dbg_cl_always_allow_pause", false, "Allow pausing in network games?", CVAR_PreInit);


server_t sv;
server_static_t svs;

// increment every time a check is made
int validcount = 1;
// for sector height cache
int validcountSZCache = 1;

bool sv_loading = false;
bool sv_map_travel = false;
int sv_load_num_players = 0;
bool run_open_scripts = false;

VBasePlayer *GPlayersBase[MAXPLAYERS];
vuint8 deathmatch = 0; // only if started as net death
int TimerGame = 0;
VLevelInfo *GLevelInfo = nullptr;
int LeavePosition = 0;
bool completed = false;
VNetContext *GDemoRecordingContext = nullptr;

static int RebornPosition = 0; // position indicator for cooperative net-play reborn

static bool mapteleport_issued = false;
static int mapteleport_flags = 0;
static int mapteleport_skill = -1;

static VCvarI TimeLimit("TimeLimit", "0", "TimeLimit mode?", 0/*CVAR_PreInit*/);
VCvarB NoExit("NoExit", false, "Disable exiting in deathmatch?", 0/*CVAR_PreInit*/);
static VCvarI DeathMatch("DeathMatch", "0", "DeathMatch mode.", CVAR_ServerInfo);
VCvarB NoMonsters("NoMonsters", false, "NoMonsters mode?", 0/*CVAR_PreInit*/);
VCvarI Skill("Skill", "3", "Skill level.", 0/*CVAR_PreInit*/);
VCvarB sv_cheats("sv_cheats", false, "Allow cheats in network game?", /*CVAR_ServerInfo|CVAR_Latch|*/CVAR_PreInit);
static VCvarB sv_barrel_respawn("sv_barrel_respawn", false, "Respawn barrels in network game?", CVAR_Archive|/*CVAR_ServerInfo|CVAR_Latch|*/CVAR_PreInit);
static VCvarB sv_pushable_barrels("sv_pushable_barrels", true, "Pushable barrels?", CVAR_Archive|/*CVAR_ServerInfo|CVAR_Latch|*/CVAR_PreInit);
VCvarB sv_decoration_block_projectiles("sv_decoration_block_projectiles", false, "Should decoration things block projectiles?", CVAR_Archive|/*CVAR_ServerInfo|CVAR_Latch|*/CVAR_PreInit);
static VCvarB split_frame("split_frame", true, "Splitframe mode?", CVAR_Archive|CVAR_PreInit);
static VCvarI sv_maxmove("sv_maxmove", "400", "Maximum allowed network movement.", CVAR_Archive);
static VCvarF master_heartbeat_time("master_heartbeat_time", "3", "Master server heartbit interval, seconds.", CVAR_Archive|CVAR_PreInit);

static VServerNetContext *ServerNetContext = nullptr;

static double LastMasterUpdate = 0.0;

static bool cliMapStartFound = false;


struct VCVFSSaver {
  void *userdata;
  VStream *(*dgOpenFile) (VStr filename, void *userdata);

  VCVFSSaver () : userdata(VMemberBase::userdata), dgOpenFile(VMemberBase::dgOpenFile) {}

  ~VCVFSSaver () {
    VMemberBase::userdata = userdata;
    VMemberBase::dgOpenFile = dgOpenFile;
  }
};


static int vcmodCurrFileLump = -1;

static VStream *vcmodOpenFile (VStr filename, void *userdata) {
  /*
  for (int flump = W_IterateFile(-1, filename); flump >= 0; flump = W_IterateFile(flump, filename)) {
    if (vcmodCurrFile >= 0 && (vcmodCurrFile != W_LumpFile(flump))) continue;
    //fprintf(stderr, "VC: found <%s> for <%s>\n", *W_FullLumpName(flump), *filename);
    return W_CreateLumpReaderNum(flump);
  }
  */
  int lmp = W_CheckNumForFileNameInSameFileOrLower(vcmodCurrFileLump, filename);
  if (lmp >= 0) return W_CreateLumpReaderNum(lmp);
  //fprintf(stderr, "VC: NOT found <%s>\n", *filename);
  return nullptr;
}


//==========================================================================
//
//  G_LoadVCMods
//
//  loading mods, take list from modlistfile
//  load user-specified Vavoom C script files
//
//==========================================================================
void G_LoadVCMods (VName modlistfile, const char *modtypestr) {
  if (modlistfile == NAME_None) return;
  if (!modtypestr && !modtypestr[0]) modtypestr = "common";
  VCVFSSaver saver;
  VMemberBase::dgOpenFile = &vcmodOpenFile;
  for (int ScLump = W_IterateNS(-1, WADNS_Global); ScLump >= 0; ScLump = W_IterateNS(ScLump, WADNS_Global)) {
    if (W_LumpName(ScLump) != modlistfile) continue;
    //vcmodCurrFile = W_LumpFile(ScLump);
    vcmodCurrFileLump = ScLump;
    VScriptParser *sc = new VScriptParser(W_FullLumpName(ScLump), W_CreateLumpReaderNum(ScLump));
    GCon->Logf(NAME_Init, "parsing Vavoom C mod list from '%s'...", *W_FullLumpName(ScLump));
    while (!sc->AtEnd()) {
      sc->ExpectString();
      //fprintf(stderr, "  <%s>\n", *sc->String.quote());
      while (sc->String.length() && (vuint8)sc->String[0] <= ' ') sc->String.chopLeft(1);
      while (sc->String.length() && (vuint8)sc->String[sc->String.length()-1] <= ' ') sc->String.chopRight(1);
      if (sc->String.length() == 0 || sc->String[0] == '#' || sc->String[0] == ';') continue;
      GCon->Logf(NAME_Init, "loading %s Vavoom C mod '%s'...", modtypestr, *sc->String);
      VMemberBase::StaticLoadPackage(VName(*sc->String), TLocation());
    }
    delete sc;
  }
}


//==========================================================================
//
//  SV_ReplaceCustomDamageFactors
//
//==========================================================================
void SV_ReplaceCustomDamageFactors () {
  if (!GGameInfo) return;
  // put custom damage factors into gameinfo object
  if (CustomDamageFactors.length()) {
    GCon->Logf(NAME_Init, "setting up %d custom damage factor%s", CustomDamageFactors.length(), (CustomDamageFactors.length() != 1 ? "s" : ""));
    VField *F = GGameInfo->GetClass()->FindFieldChecked("CustomDamageFactors");
    TArray<VDamageFactor> &dflist = *(TArray<VDamageFactor> *)F->GetFieldPtr(GGameInfo);
    dflist.resize(CustomDamageFactors.length()); // do not overallocate
    for (auto &&it : CustomDamageFactors) {
      VDamageFactor &newdf = dflist.alloc();
      newdf = it;
    }
    // this can be called from mapinfo parser, so don't clear it here
    //CustomDamageFactors.clear(); // we don't need them anymore
  }
}


//==========================================================================
//
//  SV_GetModListHash
//
//==========================================================================
vuint32 SV_GetModListHash () {
  VStr modlist;
  // get list of loaded modules
  auto wadlist = FL_GetWadPk3List();
  for (auto &&wadname : wadlist) {
    modlist += wadname;
    modlist += "\n";
  }
  //GCon->Logf(NAME_Debug, "modlist:\n%s", *modlist);
#if 0
  // get list hash
  vuint8 sha512[SHA512_DIGEST_SIZE];
  sha512_buf(sha512, *modlist, (size_t)modlist.length());
  // convert to hex
  VStr shahex = VStr::buf2hex(sha512, SHA512_DIGEST_SIZE);
#else
  vuint32 xxhashval = XXHash32::hash(*modlist, (vint32)modlist.length(), (vuint32)wadlist.length());
  //VStr shahex = VStr::buf2hex(&xxhashval, 4);
  return xxhashval;
#endif
}


//==========================================================================
//
//  SV_Init
//
//==========================================================================
void SV_Init () {
  svs.max_clients = 1;

  VMemberBase::StaticLoadPackage(NAME_game, TLocation());
  // load user-specified Vavoom C script files
  G_LoadVCMods("loadvcs", "server");
  // this emits code for all `PackagesToEmit()`
  VPackage::StaticEmitPackages();

  GGameInfo = (VGameInfo *)VObject::StaticSpawnWithReplace(VClass::FindClass("MainGameInfo"));
  GCon->Logf(NAME_Init, "Spawned game info object of class '%s'", *GGameInfo->GetClass()->GetFullName());
  GGameInfo->eventInit();

  ProcessDecorateScripts();
  ProcessDecalDefs();
  ProcessDehackedFiles();
  VPackage::StaticEmitPackages(); // just in case

  GGameInfo->eventPostDecorateInit();

  if (cli_SVDumpDoomEd > 0) VClass::StaticDumpMObjInfo();
  if (cli_SVDumpScriptId > 0) VClass::StaticDumpScriptIds();

  GCon->Logf(NAME_Init, "registering %d sprites...", VClass::GetSpriteCount());
  for (int i = 0; i < VClass::GetSpriteCount(); ++i) R_InstallSprite(*VClass::GetSpriteNameAt(i), i);
  R_InstallSpriteComplete(); // why not?

  ServerNetContext = new VServerNetContext();

  VClass *PlayerClass = VClass::FindClass("Player");
  for (int i = 0; i < MAXPLAYERS; ++i) {
    GPlayersBase[i] = (VBasePlayer *)VObject::StaticSpawnWithReplace(PlayerClass);
    if (developer) GCon->Logf(NAME_Dev, "spawned base player object for player #%d, with actual class <%s>", i, *GPlayersBase[i]->GetClass()->GetFullName());
  }

  GGameInfo->validcount = &validcount;
  GGameInfo->skyflatnum = skyflatnum; // this should be fixed after mapinfo parsing

  SV_ReplaceCustomDamageFactors();

  P_InitSwitchList();
  P_InitTerrainTypes();
  InitLockDefs();

  VMemberBase::StaticCompilerShutdown();
  CompilerReportMemory();
}


//==========================================================================
//
//  SV_ResetPlayers
//
//  call after texture manager updated a flat
//
//==========================================================================
void SV_UpdateSkyFlat () {
  if (GGameInfo) GGameInfo->skyflatnum = skyflatnum;
}


//==========================================================================
//
//  SV_ResetPlayers
//
//==========================================================================
void SV_ResetPlayers () {
  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (GPlayersBase[i]) {
      GPlayersBase[i]->ResetButtons();
      GPlayersBase[i]->eventResetToDefaults();
    }
  }
}


//==========================================================================
//
//  SV_ResetPlayerButtons
//
//==========================================================================
void SV_ResetPlayerButtons () {
  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (GPlayersBase[i]) GPlayersBase[i]->ResetButtons();
  }
}


//==========================================================================
//
//  SV_SendLoadedEvent
//
//==========================================================================
void SV_SendLoadedEvent () {
  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (GPlayersBase[i]) GPlayersBase[i]->eventOnSaveLoaded();
  }
}


//==========================================================================
//
//  SV_SendBeforeSaveEvent
//
//==========================================================================
void SV_SendBeforeSaveEvent (bool isAutosave, bool isCheckpoint) {
  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (GPlayersBase[i]) GPlayersBase[i]->eventOnBeforeSave(isAutosave, isCheckpoint);
  }
}


//==========================================================================
//
//  SV_SendAfterSaveEvent
//
//==========================================================================
void SV_SendAfterSaveEvent (bool isAutosave, bool isCheckpoint) {
  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (GPlayersBase[i]) GPlayersBase[i]->eventOnAfterSave(isAutosave, isCheckpoint);
  }
}


//==========================================================================
//
//  P_InitThinkers
//
//==========================================================================
void P_InitThinkers () {
}


//==========================================================================
//
//  SV_Shutdown
//
//==========================================================================
void SV_Shutdown () {
  if (GGameInfo) {
    SV_ShutdownGame();
    GGameInfo->ConditionalDestroy();
  }
  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (GPlayersBase[i]) {
      delete GPlayersBase[i]->Net;
      GPlayersBase[i]->Net = nullptr;
      GPlayersBase[i]->ConditionalDestroy();
    }
  }

  P_FreeTerrainTypes();
  ShutdownLockDefs();
  svs.serverinfo.Clean();

  delete ServerNetContext;
  ServerNetContext = nullptr;
}


//==========================================================================
//
//  SV_Clear
//
//==========================================================================
void SV_Clear () {
  if (GLevel) {
    for (int i = 0; i < svs.max_clients; ++i) {
      VBasePlayer *Player = GGameInfo->Players[i];
      if (!Player) continue;
      if (Player->Net) Player->Net->ResetLevel();
    }

    if (GDemoRecordingContext) {
      for (int f = 0; f < GDemoRecordingContext->ClientConnections.length(); ++f) {
        //GDemoRecordingContext->ClientConnections[f]->Driver->SetNetTime();
        GDemoRecordingContext->ClientConnections[f]->ResetLevel();
      }
    }

    GLevel->ConditionalDestroy();
    GLevel = nullptr;
    VObject::CollectGarbage();
  }
  memset(&sv, 0, sizeof(sv));
  #ifdef CLIENT
  // clear drawer level
  if (Drawer) Drawer->RendLev = nullptr;
  // make sure all sounds are stopped
  GAudio->StopAllSound();
  #endif
}


//==========================================================================
//
//  SV_NetworkHeartbeat
//
//==========================================================================
void SV_NetworkHeartbeat (bool forced) {
  if (!ServerNetContext) return;
  if (GGameInfo->NetMode != NM_DedicatedServer && GGameInfo->NetMode != NM_ListenServer) return; // no need if server is local
  #ifdef CLIENT
  if (cls.demorecording || cls.demoplayback) return;
  #endif
  double currTime = Sys_Time();
  if (currTime < 0) currTime = 0;
  if (ServerLastKeepAliveTime > currTime) ServerLastKeepAliveTime = currTime; // wtf?!
  if (!forced && currTime-ServerLastKeepAliveTime < 1.0/60.0) return;
  ServerLastKeepAliveTime = currTime;
  ServerNetContext->KeepaliveTick();
}


//==========================================================================
//
//  SV_SendClientMessages
//
//==========================================================================
static void SV_SendClientMessages (bool full=true) {
  if (/*full*/true) {
    // update player replication infos
    for (int i = 0; i < svs.max_clients; ++i) {
      VBasePlayer *Player = GGameInfo->Players[i];
      if (!Player) continue;
      if (Player->IsGoingToDie()) continue;

      VPlayerReplicationInfo *RepInfo = Player->PlayerReplicationInfo;
      if (!RepInfo) continue;

      RepInfo->PlayerName = Player->PlayerName;
      RepInfo->UserInfo = Player->UserInfo;
      RepInfo->TranslStart = Player->TranslStart;
      RepInfo->TranslEnd = Player->TranslEnd;
      RepInfo->Color = Player->Color;
      RepInfo->Frags = Player->Frags;
      RepInfo->Deaths = Player->Deaths;
      RepInfo->KillCount = Player->KillCount;
      RepInfo->ItemCount = Player->ItemCount;
      RepInfo->SecretCount = Player->SecretCount;

      // update view angle if needed
      if (Player->PlayerFlags&VBasePlayer::PF_Spawned) {
        Player->WriteViewData();
      }
      if (!full && Player->Net) {
        Player->Net->GetMessages();
        // server context ticker will tick all client connections
      }
    }
  }

  ServerNetContext->Tick();
  ServerLastKeepAliveTime = Sys_Time();

  if (full && GDemoRecordingContext) {
    for (int i = 0; i < GDemoRecordingContext->ClientConnections.length(); ++i) {
      //GDemoRecordingContext->ClientConnections[i]->Driver->SetNetTime();
      GDemoRecordingContext->ClientConnections[i]->NeedsUpdate = true;
    }
    GDemoRecordingContext->Tick();
  }
}


//========================================================================
//
//  CheckForSkip
//
//  Check to see if any player hit a key
//
//========================================================================
static void CheckForSkip () {
  VBasePlayer *player;
  static bool triedToSkip = false;
  bool skip = false;

  //GCon->Log(NAME_Debug, "CheckForSkip!");

  /*
  if (GDemoRecordingContext) {
    skip = true;
  } else
  */
  {
    // if we don't have alive players, skip it forcefully
    bool hasAlivePlayer = false;
    for (int i = 0; i < MAXPLAYERS; ++i) {
      player = GGameInfo->Players[i];
      if (player) {
        if (GGameInfo->NetMode == NM_DedicatedServer || GGameInfo->NetMode == NM_ListenServer) {
          if (!player->Net) continue;
        }
        if (player->PlayerFlags&VBasePlayer::PF_Spawned) {
          hasAlivePlayer = true;
        }
        if (player->Buttons&BT_ATTACK) {
          if (!(player->PlayerFlags&VBasePlayer::PF_AttackDown)) skip = true;
          player->PlayerFlags |= VBasePlayer::PF_AttackDown;
        } else {
          player->PlayerFlags &= ~VBasePlayer::PF_AttackDown;
        }
        if (player->Buttons&BT_USE) {
          if (!(player->PlayerFlags&VBasePlayer::PF_UseDown)) skip = true;
          player->PlayerFlags |= VBasePlayer::PF_UseDown;
        } else {
          player->PlayerFlags &= ~VBasePlayer::PF_UseDown;
        }
      }
    }

    if (deathmatch && sv.intertime < 4) {
      // wait for 4 seconds before allowing a skip
      if (skip) {
        triedToSkip = true;
        skip = false;
      }
    } else {
      if (triedToSkip) {
        skip = true;
        triedToSkip = false;
      }
    }

    // no alive players, and network game? skip intermission
    if (!hasAlivePlayer && (GGameInfo->NetMode == NM_DedicatedServer || GGameInfo->NetMode == NM_ListenServer)) {
      skip = true;
      GCon->Log(NAME_Debug, "Forced Skip!");
    }
  }

  if (skip) {
    bool wasSkip = false;
    for (int i = 0; i < svs.max_clients; ++i) {
      player = GGameInfo->Players[i];
      if (!player) continue;
      if (GGameInfo->NetMode == NM_DedicatedServer || GGameInfo->NetMode == NM_ListenServer) {
        if (!player->Net) continue;
      }
      wasSkip = true;
      player->eventClientSkipIntermission();
    }
    // if no alive players, simply go to the next map
    if (!wasSkip) {
      if (VStr(GLevelInfo->NextMap).startsWithCI("EndGame")) {
        for (int ep = 0; ep < P_GetNumEpisodes(); ++ep) {
          VEpisodeDef *edef = P_GetEpisodeDef(ep);
          if (!edef) continue; // just in case
          VName map = edef->Name;
          if (map == NAME_None || !IsMapPresent(map)) {
            //GCon->Logf(NAME_Debug, "  ep=%d; map '%s' is not here!", ep, *edef->Name);
            map = edef->TeaserName;
            if (map == NAME_None || !IsMapPresent(map)) continue;
          }
          GLevelInfo->NextMap = map;
          break;
        }
      }
      GCon->Logf(NAME_Debug, "*** teleporting to the new map '%s'...", *GLevelInfo->NextMap);
      GCmdBuf << "TeleportNewMap\n";
    }
  }
}


//==========================================================================
//
//  SV_RunPlayerTick
//
//  do all necessary checks *BEFORE* calling this function
//
//==========================================================================
static void SV_RunPlayerTick (VBasePlayer *Player, bool skipFrame) {
  Player->ForwardMove = (skipFrame && dbg_skipframe_player_block_move ? 0 : Player->ClientForwardMove);
  Player->SideMove = (skipFrame && dbg_skipframe_player_block_move ? 0 : Player->ClientSideMove);
  //if (Player->ForwardMove) GCon->Logf("ffm: %f (%d)", Player->ClientForwardMove, (int)skipFrame);
  // don't move faster than maxmove
       if (Player->ForwardMove > sv_maxmove) Player->ForwardMove = sv_maxmove;
  else if (Player->ForwardMove < -sv_maxmove) Player->ForwardMove = -sv_maxmove;
       if (Player->SideMove > sv_maxmove) Player->SideMove = sv_maxmove;
  else if (Player->SideMove < -sv_maxmove) Player->SideMove = -sv_maxmove;
  // check for disabled freelook and jumping
  if (!sv_ignore_nomlook && (GLevelInfo->LevelInfoFlags&VLevelInfo::LIF_NoFreelook)) Player->ViewAngles.pitch = 0;
  if (!sv_ignore_nojump && (GLevelInfo->LevelInfoFlags&VLevelInfo::LIF_NoJump)) Player->Buttons &= ~BT_JUMP;
  if (!sv_ignore_nocrouch && (GLevelInfo->LevelInfoFlags&VLevelInfo::LIF2_NoCrouch)) Player->Buttons &= ~BT_JUMP;
  if (cl_disable_crouch) Player->Buttons &= ~BT_CROUCH;
  if (cl_disable_jump) Player->Buttons &= ~BT_JUMP;
  if (cl_disable_mlook) Player->ViewAngles.pitch = 0;
  //GCon->Logf("*** 000: PLAYER TICK(%p) ***: Buttons=0x%08x; OldButtons=0x%08x", Player, Player->Buttons, Player->OldButtons);
  Player->eventPlayerTick(host_frametime);
  //GCon->Logf("*** 001: PLAYER TICK(%p) ***: Buttons=0x%08x; OldButtons=0x%08x", Player, Player->Buttons, Player->OldButtons);
  // new logic for client buttons update
  //Player->OldButtons = Player->AcsButtons;
  //Player->OldViewAngles = Player->ViewAngles;
  // latch logic
  //if ((Player->AcsNextButtonUpdate -= host_frametime) <= 0.0f)
  if (Player->AcsNextButtonUpdate <= GLevel->TicTime) {
    //Player->AcsNextButtonUpdate = 1.0f/35.0f; // once per standard DooM frame
    //!Player->AcsNextButtonUpdate = 1.0f/36.0f; // once per standard DooM frame
    Player->AcsNextButtonUpdate = GLevel->TicTime+1;
    Player->OldButtons = Player->AcsButtons;
    // now create new acs buttons
    Player->AcsButtons = Player->AcsCurrButtonsPressed;
    Player->AcsCurrButtonsPressed = Player->AcsCurrButtons;
    // mouse movement
    Player->AcsMouseX = Player->AcsPrevMouseX;
    Player->AcsMouseY = Player->AcsPrevMouseY;
    Player->AcsPrevMouseX = 0;
    Player->AcsPrevMouseY = 0;
  }
}


//==========================================================================
//
//  SV_RunClients
//
//==========================================================================
static void SV_RunClients (bool skipFrame=false) {
  // get commands
  for (int i = 0; i < MAXPLAYERS; ++i) {
    VBasePlayer *Player = GGameInfo->Players[i];
    if (!Player) continue;

    if ((Player->PlayerFlags&VBasePlayer::PF_IsBot) &&
        !(Player->PlayerFlags&VBasePlayer::PF_Spawned))
    {
      Player->SpawnClient();
    }

    // do player reborns if needed
         if (Player->PlayerState == PST_REBORN) G_DoReborn(i, false);
    else if (Player->PlayerState == PST_CHEAT_REBORN) G_DoReborn(i, true);

    if (Player->Net) {
      // resetting update flag is totally wrong: we may miss some important updates in combined mode!
      // (because they aren't sent yet)
      // let context ticker reset it instead
      //Player->Net->NeedsUpdate = false;
      //GCon->Logf(NAME_DevNet, "player #%d: getting messages...", i);
      // actually, force updates...
      //Player->Net->NeedsUpdate = true;
      Player->Net->GetMessages();
      Player->Net->Tick();
    }

    // pause if in menu or console and at least one tic has been run
    if ((Player->PlayerFlags&VBasePlayer::PF_Spawned) && !sv.intermission && !GGameInfo->IsPaused()) {
      if (dbg_vm_disable_thinkers) {
        if (Player->PlayerFlags&VBasePlayer::PF_IsBot) continue;
      }
      SV_RunPlayerTick(Player, skipFrame);
    }
  }

  //GCon->Logf(NAME_Debug, "*** IMS: %d (demo=%p : %d)", (int)sv.intermission, GDemoRecordingContext, (int)cls.demorecording);
  if (sv.intermission) {
    #ifdef CLIENT
    if (GDemoRecordingContext) {
      if (cls.demorecording) {
        //CL_NetworkHeartbeat();
        for (int f = 0; f < GDemoRecordingContext->ClientConnections.length(); ++f) {
          //GDemoRecordingContext->ClientConnections[f]->Driver->NetTime = 0; //Sys_Time()+1000;
          //GDemoRecordingContext->ClientConnections[f]->Flush();
          GDemoRecordingContext->ClientConnections[f]->Intermission(true);
        }
      }
      /*
      for (int f = 0; f < GDemoRecordingContext->ClientConnections.length(); ++f) {
        //GDemoRecordingContext->ClientConnections[f]->Driver->NetTime = 0; //Sys_Time()+1000;
        //GDemoRecordingContext->ClientConnections[f]->Flush();
        GDemoRecordingContext->ClientConnections[f]->ResetLevel();
      }
      */
    }
    #endif
    CheckForSkip();
    sv.intertime += host_frametime;
  }

  if (!sv.intermission) {
    #ifdef CLIENT
    if (GDemoRecordingContext) {
      if (cls.demorecording) {
        for (int f = 0; f < GDemoRecordingContext->ClientConnections.length(); ++f) {
          //GDemoRecordingContext->ClientConnections[f]->Driver->NetTime = 0; //Sys_Time()+1000;
          //GDemoRecordingContext->ClientConnections[f]->Flush();
          GDemoRecordingContext->ClientConnections[f]->Intermission(false);
        }
      }
    }
    #endif
  }
}


//==========================================================================
//
//  SV_UpdateMaster
//
//==========================================================================
static void SV_UpdateMaster () {
  if (GGameInfo->NetMode >= NM_DedicatedServer &&
      (!LastMasterUpdate || host_time-LastMasterUpdate > master_heartbeat_time))
  {
    GNet->UpdateMaster();
    LastMasterUpdate = host_time;
  }
}


//==========================================================================
//
//  SV_Ticker
//
//==========================================================================
static void SV_Ticker () {
  //double saved_frametime;

  if (host_frametime <= 0) return;

  //saved_frametime = host_frametime;

  if (host_frametime < max_fps_cap_double) {
    host_framefrac += host_frametime;
    host_frametime = 0;
    return;
  }

  int scap = host_max_skip_frames;
  if (scap < 3) scap = 3;

  //exec_times = 1;
  if (!real_time) {
    // rounded a little bit up to prevent "slow motion"
    host_frametime = FrameTime; //0.02857142857142857142857142857143; //1.0 / 35.0;
  } else if (split_frame && host_frametime > FrameTime) {
    double i;
    /*double frc =*/ (void)modf(host_frametime/FrameTime, &i);
    //GCon->Logf("*** ft=%f; frt=%f; int=%f; frc=%f", host_frametime, FrameTime, i, frc);
    if (i < 1) { GCon->Logf(NAME_Error, "WTF?! i must be at least one, but it is %f", i); i = 1; }
    int exec_times = (i > 0x1fffffff ? 0x1fffffff : (int)i);
    {
      bool showExecTimes = (cli_SVShowExecTimes > 0);
      if (showExecTimes) {
        if (exec_times <= scap) GCon->Logf("exec_times=%d", exec_times); else GCon->Logf("exec_times=%d (capped to %d)", exec_times, scap);
      }
    }
    // cap
    if (exec_times > scap) {
      exec_times = scap;
      host_frametime = FrameTime*exec_times;
    }
  }

  if (sv_loading || sv.intermission) {
    GGameInfo->frametime = host_frametime;
    // do not run intermission while wiping
    SV_RunClients();
  } else {
    double saved_frametime = host_frametime;
    bool frameSkipped = false;
    bool wasPaused = false;
    bool timeLimitReached = false;
    bool runClientsCalled = false;
    // do main actions
    double frametimeleft = host_frametime;
    int lastTick = GLevel->TicTime;
    while (!sv.intermission && !completed && frametimeleft >= max_fps_cap_double) {
      if (GLevel->TicTime != lastTick) {
        lastTick = GLevel->TicTime;
        VObject::CollectGarbage();
      }
      // calculate frame time
      // do small steps, it seems to work better this way
      double currframetime = (split_frame && frametimeleft >= FrameTime*0.4 ? 1.0/35.0*0.4 : frametimeleft);
      if (currframetime > frametimeleft) currframetime = frametimeleft;
      // do it this way, because of rounding
      GGameInfo->frametime = currframetime;
      host_frametime = GGameInfo->frametime;
      if (GGameInfo->IsPaused()) {
        // no need to do anything more if the game is paused
        if (!frameSkipped) { runClientsCalled = true; SV_RunClients(); }
        wasPaused = true;
        break;
      }
      // advance player states, so weapons won't slow down on frame skip
      if (!frameSkipped || dbg_skipframe_player_tick) {
        runClientsCalled = true;
        SV_RunClients(frameSkipped); // have to make a full run, for demos/network (k8: is it really necessary?)
      }
      GLevel->TickWorld(host_frametime);
      //GCon->Logf("%d: ft=%f; ftleft=%f; Time=%f; tics=%d", (int)frameSkipped, host_frametime, oldft-GGameInfo->frametime, GLevel->Time, (int)GLevel->TicTime);
      // level timer
      if (TimerGame && TimerGame >= GLevel->TicTime) {
        TimerGame = 0;
        LeavePosition = 0;
        completed = true;
        timeLimitReached = true;
      }
      frametimeleft -= host_frametime /*currframetime*/; // next step
      frameSkipped = true;
    }
    if (!runClientsCalled) SV_RunClients(true);
    if (completed) G_DoCompleted(timeLimitReached);
    // remember fractional frame time
    host_frametime = saved_frametime;
    if (!wasPaused) {
      if (!sv.intermission && !completed && frametimeleft > 0 && frametimeleft < max_fps_cap_double) {
        host_framefrac += frametimeleft;
        host_frametime -= frametimeleft;
      }
    }
  }

  //host_frametime = saved_frametime;
}


//==========================================================================
//
//  CheckRedirects
//
//==========================================================================
static VName CheckRedirects (VName Map) {
  const mapInfo_t &Info = P_GetMapInfo(Map);
  if (Info.RedirectType == NAME_None || Info.RedirectMap == NAME_None) return Map; // no redirect for this map
  // check all players
  for (int i = 0; i < MAXPLAYERS; ++i) {
    VBasePlayer *P = GGameInfo->Players[i];
    if (!P || !(P->PlayerFlags&VBasePlayer::PF_Spawned)) continue;
    // no replacements allowed
    if (P->MO->eventCheckInventory(Info.RedirectType, false) > 0) return CheckRedirects(Info.RedirectMap);
  }
  // none of the players have required item, no redirect
  return Map;
}


//==========================================================================
//
//  G_DoCompleted
//
//==========================================================================
static void G_DoCompleted (bool ignoreNoExit) {
  completed = false;
  if (sv.intermission) return;

  if (!ignoreNoExit && NoExit /*&& deathmatch*/ && (GGameInfo->NetMode == NM_DedicatedServer || GGameInfo->NetMode == NM_ListenServer)) {
    return;
  }

  if (GGameInfo->NetMode < NM_DedicatedServer &&
      (!GGameInfo->Players[0] || !(GGameInfo->Players[0]->PlayerFlags&VBasePlayer::PF_Spawned)))
  {
    //FIXME: some ACS left from previous visit of the level
    return;
  }

  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (GGameInfo->Players[i]) GGameInfo->Players[i]->eventPlayerBeforeExitMap();
  }

#ifdef CLIENT
  SV_AutoSaveOnLevelExit();
  SCR_SignalWipeStart();
#endif

  sv.intermission = 1;
  sv.intertime = 0;
  GLevelInfo->CompletitionTime = GLevel->Time;

  GLevel->Acs->StartTypedACScripts(SCRIPT_Unloading, 0, 0, 0, nullptr, false, true);

  GLevelInfo->NextMap = CheckRedirects(GLevelInfo->NextMap);

  const mapInfo_t &old_info = P_GetMapInfo(GLevel->MapName);
  const mapInfo_t &new_info = P_GetMapInfo(GLevelInfo->NextMap);
  const VClusterDef *ClusterD = P_GetClusterDef(old_info.Cluster);
  bool HubChange = (!old_info.Cluster || !(ClusterD->Flags&CLUSTERF_Hub) || old_info.Cluster != new_info.Cluster);

  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (GGameInfo->Players[i]) {
      GGameInfo->Players[i]->eventPlayerExitMap(HubChange);
      if (deathmatch || HubChange) {
        GGameInfo->Players[i]->eventClientIntermission(GLevelInfo->NextMap);
      }
    }
  }

  if (!deathmatch && !HubChange) GCmdBuf << "TeleportNewMap\n";
}


//==========================================================================
static const char *knownFinalesList[] = {
  "EndGameBunny",
  "EndGameCast",
  "EndGameChess",
  "EndGameDemon",
  "EndGamePic1",
  "EndGamePic2",
  "EndGamePic3",
  "EndGameStrife",
  "EndGameUnderwater",
  nullptr,
};


//==========================================================================
//
//  COMMAND TestFinale
//
//==========================================================================
COMMAND_WITH_AC(TestFinale) {
  if (Args.length() != 2) return;

  if (GGameInfo->NetMode == NM_Client) return;

  CMD_FORWARD_TO_SERVER();

  // normalise finale name
  VStr fname = Args[1];
  for (const char **fin = knownFinalesList; *fin; ++fin) {
    if (fname.strEquCI(*fin)) {
      fname = VStr(*fin);
      break;
    }
  }

  if (GGameInfo->NetMode == NM_Standalone && !deathmatch) {
    for (int i = 0; i < svs.max_clients; ++i) {
      if (GGameInfo->Players[i]) {
        GGameInfo->Players[i]->eventClientFinale(fname);
      }
    }
    sv.intermission = 2;
  }

  //if (GGameInfo->NetMode == NM_Standalone) SV_UpdateRebornSlot(); // copy the base slot to the reborn slot
}


//==========================================================================
//
//  COMMAND_AC TestFinale
//
//==========================================================================
COMMAND_AC(TestFinale) {
  TArray<VStr> list;
  VStr prefix = (aidx < args.length() ? args[aidx] : VStr());
  if (aidx == 1) {
    for (const char **fin = knownFinalesList; *fin; ++fin) list.append(VStr(*fin));
    return AutoCompleteFromListCmd(prefix, list);
  } else {
    return VStr::EmptyString;
  }
}


//==========================================================================
//
//  COMMAND TeleportNewMap
//
//  TeleportNewMap [mapname leaveposition] | [forced]
//
//==========================================================================
COMMAND_WITH_AC(TeleportNewMap) {
  int flags = 0;

  // dumb clients must forward this
  if (GGameInfo->NetMode == NM_None) return;

  CMD_FORWARD_TO_SERVER();

  if (Args.Num() == 3) {
    GLevelInfo->NextMap = VName(*Args[1], VName::AddLower8);
    LeavePosition = VStr::atoi(*Args[2]);
  } else if (sv.intermission != 1) {
    if (Args.length() != 2 || !Args[1].startsWithCI("force")) return;
    flags = CHANGELEVEL_REMOVEKEYS;
  }

  if (!deathmatch) {
    if (VStr(GLevelInfo->NextMap).startsWithCI("EndGame")) {
      for (int i = 0; i < svs.max_clients; ++i) {
        if (GGameInfo->Players[i]) GGameInfo->Players[i]->eventClientFinale(*GLevelInfo->NextMap);
      }
      sv.intermission = 2;
      return;
    }
  }

#ifdef CLIENT
  Draw_TeleportIcon();
#endif

  RebornPosition = LeavePosition;
  GGameInfo->RebornPosition = RebornPosition;
  mapteleport_issued = true;
  mapteleport_flags = flags;
  mapteleport_skill = -1;
  //if (GGameInfo->NetMode == NM_Standalone) SV_UpdateRebornSlot(); // copy the base slot to the reborn slot
}

COMMAND_AC_SIMPLE_LIST(TeleportNewMap, "forced")


// ////////////////////////////////////////////////////////////////////////// //
struct TeleportMapExFlag {
  const char *name;
  int value;
  bool reset;
};

static TeleportMapExFlag TMEFlags[] = {
  { "KeepFacing", CHANGELEVEL_KEEPFACING, false },
  { "KeepKeys", CHANGELEVEL_REMOVEKEYS, true },
  { "ResetInventory", CHANGELEVEL_RESETINVENTORY, false },
  { "NoMonsters", CHANGELEVEL_NOMONSTERS, false },
  //{ "ChangeSkill", CHANGELEVEL_CHANGESKILL, false },
  { "NoIntermission", CHANGELEVEL_NOINTERMISSION, false },
  { "ResetHealth", CHANGELEVEL_RESETHEALTH, false },
  { "PreraiseWeapon", CHANGELEVEL_PRERAISEWEAPON, false },
  { nullptr, 0, false },
};


//==========================================================================
//
//  COMMAND ACS_TeleportNewMap
//
//  mapname posidx flags [skill]
//
//==========================================================================
COMMAND(ACS_TeleportNewMap) {
  if (Args.length() != 5) {
    GCon->Logf("ACS_TeleportNewMap mapname posidx flags skill");
    return;
  }

  // dumb clients must forward this
  if (GGameInfo->NetMode == NM_None /*|| GGameInfo->NetMode == NM_Client*/) return;

  CMD_FORWARD_TO_SERVER();

  int posidx = 0;
  if (!Args[2].convertInt(&posidx)) {
    GCon->Logf(NAME_Warning, "ACS_TeleportNewMap: invalid position index '%s'", *Args[2]);
    posidx = 0;
  }

  int flags = 0;
  if (!Args[3].convertInt(&flags)) {
    GCon->Logf(NAME_Warning, "ACS_TeleportNewMap: invalid flags value '%s'", *Args[3]);
    flags = 0;
  }
  flags |= CHANGELEVEL_REMOVEKEYS;

  int skill = -1;
  if (!Args[4].convertInt(&flags)) {
    GCon->Logf(NAME_Warning, "ACS_TeleportNewMap: invalid skill value '%s'", *Args[4]);
    skill = -1;
  }

  GCon->Logf(NAME_Debug, "ACS level teleport: map=<%s>; flags=0x%04x, skill=%d", *Args[1], (unsigned)flags, skill);

  VName mname = VName(*Args[1], VName::FindLower8);
  if (mname == NAME_None) {
    if (!Args[1].startsWithCI("EndGame")) {
      GCon->Logf(NAME_Warning, "ACS_TeleportNewMap: unknown map name '%s'", *Args[1]);
      return;
    }
    mname = VName(*Args[1]);
  }
  GLevelInfo->NextMap = mname;

  if (!deathmatch) {
    if (VStr(GLevelInfo->NextMap).startsWithCI("EndGame")) {
      for (int i = 0; i < svs.max_clients; ++i) {
        if (GGameInfo->Players[i]) GGameInfo->Players[i]->eventClientFinale(*GLevelInfo->NextMap);
      }
      sv.intermission = 2;
      return;
    }
  }

#ifdef CLIENT
  Draw_TeleportIcon();
#endif

  RebornPosition = posidx;
  GGameInfo->RebornPosition = RebornPosition;
  mapteleport_issued = true; // this will actually change the map
  mapteleport_flags = flags;
  mapteleport_skill = skill;
  //if (GGameInfo->NetMode == NM_Standalone) SV_UpdateRebornSlot(); // copy the base slot to the reborn slot
}


//==========================================================================
//
//  COMMAND TeleportNewMapEx
//
//  mapname posidx flags [skill]
//
//==========================================================================
COMMAND_WITH_AC(TeleportNewMapEx) {
  if (Args.length() < 2) {
    GCon->Logf("TeleportNewMapEx mapname|+ [posidx [flags [skill]]]");
    return;
  }

  if (GGameInfo->NetMode == NM_None || GGameInfo->NetMode == NM_Client) return;

  CMD_FORWARD_TO_SERVER();

  int posidx = 0;
  if (Args.length() > 2) {
    if (!Args[2].convertInt(&posidx)) {
      GCon->Logf(NAME_Warning, "TeleportNewMapEx: invalid position index '%s'", *Args[2]);
      return;
    }
  }

  int flags = CHANGELEVEL_REMOVEKEYS, skill = -1;
  for (int f = 3; f < Args.length(); ++f) {
    bool found = false;
    for (const TeleportMapExFlag *tff = TMEFlags; tff->name; ++tff) {
      if (Args[f].strEquCI(tff->name)) {
        found = true;
        if (tff->reset) flags &= ~tff->value; else flags |= tff->value;
        break;
      }
    }
    // if not found, try numeric conversion for skill
    if (found) continue;
    // try skill names too
    int skn = M_SkillFromName(*Args[f]);
    if (skn >= 0) {
      skill = skn;
      flags |= CHANGELEVEL_CHANGESKILL;
      continue;
    }
    GCon->Logf(NAME_Warning, "TeleportNewMapEx: invalid flag '%s'", *Args[f]);
    return;
  }

  //GCon->Logf(NAME_Debug, "TeleportNewMapEx: name=<%s>; posidx=%d; flags=0x%04x; skill=%d", *Args[1], posidx, flags, skill);

  if (Args[1] == "+" || Args[1] == "*") {
    // use default next map; i.e. do nothing
  } else {
    VName mname = VName(*Args[1], VName::FindLower8);
    if (mname == NAME_None) {
      if (!Args[1].startsWithCI("EndGame")) {
        GCon->Logf(NAME_Warning, "TeleportNewMapEx: unknown map name '%s'", *Args[1]);
        return;
      }
      mname = VName(*Args[1]);
    }
    GLevelInfo->NextMap = mname;
  }

  if (!deathmatch) {
    if (VStr(GLevelInfo->NextMap).startsWithCI("EndGame")) {
      for (int i = 0; i < svs.max_clients; ++i) {
        if (GGameInfo->Players[i]) GGameInfo->Players[i]->eventClientFinale(*GLevelInfo->NextMap);
      }
      sv.intermission = 2;
      return;
    }
  }

#ifdef CLIENT
  Draw_TeleportIcon();
#endif

  RebornPosition = posidx;
  GGameInfo->RebornPosition = RebornPosition;
  mapteleport_issued = true; // this will actually change the map
  mapteleport_flags = flags;
  mapteleport_skill = skill;
  //if (GGameInfo->NetMode == NM_Standalone) SV_UpdateRebornSlot(); // copy the base slot to the reborn slot
}


//==========================================================================
//
//  COMMAND_AC TeleportNewMapEx
//
//==========================================================================
COMMAND_AC(TeleportNewMapEx) {
  VStr prefix = (aidx < args.length() ? args[aidx] : VStr());
  if (aidx == 1) {
    int mapcount = P_GetNumMaps();
    TArray<VStr> list;
    list.resize(mapcount);
    for (int f = 0; f < mapcount; ++f) {
      VName mlump = P_GetMapLumpName(f);
      if (mlump != NAME_None) list.append(*mlump);
    }
    if (list.length()) return AutoCompleteFromListCmd(prefix, list);
  } else if (aidx >= 3) {
    // flags
    TArray<VStr> list;
    for (const TeleportMapExFlag *tff = TMEFlags; tff->name; ++tff) {
      // check for duplicate
      bool found = false;
      for (int f = 1; f < args.length(); ++f) if (args[f].strEquCI(tff->name)) { found = true; break; }
      if (!found) list.append(tff->name);
    }
    if (list.length()) return AutoCompleteFromListCmd(prefix, list);
  }
  return VStr::EmptyString;
}


//==========================================================================
//
//  G_DoReborn
//
//==========================================================================
static void G_DoReborn (int playernum, bool cheatReborn) {
  if (!GGameInfo->Players[playernum] ||
      !(GGameInfo->Players[playernum]->PlayerFlags&VBasePlayer::PF_Spawned))
  {
    return;
  }
  if (GGameInfo->NetMode == NM_Standalone && !cheatReborn) {
    GCmdBuf << "Restart\n";
    GGameInfo->Players[playernum]->PlayerState = PST_LIVE;
  } else {
    GGameInfo->Players[playernum]->eventNetGameReborn();
  }
}


//==========================================================================
//
//  NET_SendToAll
//
//==========================================================================
int NET_SendToAll (int blocktime) {
  double start;
  int count = 0;
  bool state1[MAXPLAYERS];
  bool state2[MAXPLAYERS];

  for (int i = 0; i < svs.max_clients; ++i) {
    VBasePlayer *Player = GGameInfo->Players[i];
    if (Player && Player->Net) {
      if (Player->Net->IsLocalConnection()) {
        state1[i] = false;
        state2[i] = true;
        continue;
      }
      ++count;
      state1[i] = false;
      state2[i] = false;
    } else {
      state1[i] = true;
      state2[i] = true;
    }
  }

  start = Sys_Time();
  while (count) {
    count = 0;
    for (int i = 0; i < svs.max_clients; ++i) {
      VBasePlayer *Player = GGameInfo->Players[i];
      if (!Player) continue;

      if (!state1[i]) {
        state1[i] = true;
        //Player->Net->ForceAllowSendForServer();
        Player->Net->GetGeneralChannel()->Close();
        ++count;
        continue;
      }

      if (!state2[i]) {
        if (Player->Net->State == NETCON_Closed) {
          state2[i] = true;
        } else {
          //Player->Net->ForceAllowSendForServer();
          Player->Net->GetMessages();
          Player->Net->Tick();
        }
        ++count;
        continue;
      }
    }
    if ((Sys_Time()-start) > blocktime) break;
  }

  return count;
}


//==========================================================================
//
//  SV_SendServerInfoToClients
//
//==========================================================================
void SV_SendServerInfoToClients () {
  for (int i = 0; i < svs.max_clients; ++i) {
    VBasePlayer *Player = GGameInfo->Players[i];
    if (!Player) continue;
    Player->Level = GLevelInfo;
    if (Player->Net) {
      Player->Net->LoadedNewLevel();
      Player->Net->SendServerInfo();
    } else {
      //GCon->Logf("SERVER: player #%d has no network connection", i);
    }
  }

  if (GDemoRecordingContext) {
    for (int f = 0; f < GDemoRecordingContext->ClientConnections.length(); ++f) {
      //GDemoRecordingContext->ClientConnections[f]->Driver->SetNetTime();
      GDemoRecordingContext->ClientConnections[f]->LoadedNewLevel();
      GDemoRecordingContext->ClientConnections[f]->SendServerInfo();
    }
  }
}


//==========================================================================
//
//  SV_SpawnServer
//
//==========================================================================
void SV_SpawnServer (const char *mapname, bool spawn_thinkers, bool titlemap) {
  if (GSoundManager) GSoundManager->CleanupSounds();

  GCon->Log("===============================================");
  GCon->Logf("Spawning %sserver with map \"%s\"%s...", (titlemap ? "titlemap " : ""), mapname, (spawn_thinkers ? "" : " (without thinkers)"));

  GGameInfo->Flags &= ~VGameInfo::GIF_Paused;
  mapteleport_issued = false;
  mapteleport_flags = 0;
  mapteleport_skill = -1;
  run_open_scripts = spawn_thinkers;
  serverStartRenderFramesTic = -1;

  cliMapStartFound = false;

  if (GGameInfo->NetMode != NM_None) {
    //fprintf(stderr, "SV_SpawnServer!!!\n");
    // level change
    for (int i = 0; i < MAXPLAYERS; ++i) {
      if (!GGameInfo->Players[i]) continue;

      GGameInfo->Players[i]->KillCount = 0;
      GGameInfo->Players[i]->SecretCount = 0;
      GGameInfo->Players[i]->ItemCount = 0;

      GGameInfo->Players[i]->PlayerFlags &= ~VBasePlayer::PF_Spawned;
      GGameInfo->Players[i]->MO = nullptr;
      GGameInfo->Players[i]->Frags = 0;
      GGameInfo->Players[i]->Deaths = 0;
      if (GGameInfo->Players[i]->PlayerState == PST_DEAD) GGameInfo->Players[i]->PlayerState = PST_REBORN;
    }
  } else {
    // new game
    deathmatch = DeathMatch;
    GGameInfo->deathmatch = deathmatch;

    P_InitThinkers();

#ifdef CLIENT
    GGameInfo->NetMode = (titlemap ? NM_TitleMap : svs.max_clients == 1 ? NM_Standalone : NM_ListenServer);
#else
    GGameInfo->NetMode = NM_DedicatedServer;
#endif

    GGameInfo->WorldInfo = GGameInfo->eventCreateWorldInfo();

    GGameInfo->WorldInfo->SetSkill(Skill);
    GGameInfo->eventInitNewGame(GGameInfo->WorldInfo->GameSkill);
  }

  SV_Clear();
  //GCon->Log("*** UNLATCH ***");
  VCvar::Unlatch();

  // load it
  SV_LoadLevel(VName(mapname, VName::AddLower8));
  GLevel->NetContext = ServerNetContext;
  GLevel->WorldInfo = GGameInfo->WorldInfo;

#ifdef CLIENT
  if (GGameInfo->NetMode <= NM_Standalone) SCR_SignalWipeStart();
#endif

  const mapInfo_t &info = P_GetMapInfo(GLevel->MapName);

  if (spawn_thinkers) {
    // create level info
    GLevelInfo = (VLevelInfo *)GLevel->SpawnThinker(GGameInfo->LevelInfoClass);
    if (!GLevelInfo) Sys_Error("SV_SpawnServer: cannot spawn LevelInfo");
    if (!GLevelInfo->GetClass()->IsChildOf(GGameInfo->LevelInfoClass)) Sys_Error("SV_SpawnServer: spawned LevelInfo is of invalid class `%s`", GLevelInfo->GetClass()->GetName());
    GLevelInfo->Level = GLevelInfo;
    GLevelInfo->Game = GGameInfo;
    GLevelInfo->World = GGameInfo->WorldInfo;
    GLevel->LevelInfo = GLevelInfo;
    GLevelInfo->SetMapInfo(info);

    // spawn things
    for (int i = 0; i < GLevel->NumThings; ++i) GLevelInfo->eventSpawnMapThing(&GLevel->Things[i]);
    if (deathmatch && GLevelInfo->DeathmatchStarts.length() < 4) Host_Error("Level needs more deathmatch start spots");
  }

  if (deathmatch) {
    TimerGame = TimeLimit*35*60;
  } else {
    TimerGame = 0;
  }

  // set up world state
  Host_ResetSkipFrames();

  if (!spawn_thinkers) {
    // usually loading a save
    SV_SendServerInfoToClients(); // anyway
    if (GLevel->scriptThinkers.length() || AcsHasScripts(GLevel->Acs)) {
      serverStartRenderFramesTic = GLevel->TicTime+INITIAL_TICK_DELAY;
      GCon->Log("---");
      if (AcsHasScripts(GLevel->Acs)) GCon->Log("Found some ACS scripts");
      if (GLevel->scriptThinkers.length()) GCon->Logf("ACS thinkers: %d", GLevel->scriptThinkers.length());
    }
  } else {
    // P_SpawnSpecials
    // after the map has been loaded, scan for specials that spawn thinkers
    GLevelInfo->eventSpawnSpecials();

    SV_SendServerInfoToClients();

    // call BeginPlay events
    for (TThinkerIterator<VEntity> Ent(GLevel); Ent; ++Ent) Ent->eventBeginPlay();
    GLevelInfo->LevelInfoFlags2 |= VLevelInfo::LIF2_BegunPlay;

    Host_ResetSkipFrames();

    if (GGameInfo->NetMode != NM_TitleMap && GGameInfo->NetMode != NM_Standalone) {
      GLevel->TickWorld(FrameTime);
      GLevel->TickWorld(FrameTime);
      // start open scripts
      GLevel->Acs->StartTypedACScripts(SCRIPT_Open, 0, 0, 0, nullptr, false, false);
    }

    // delay rendering if we have ACS scripts
    if (GLevel->scriptThinkers.length() || AcsHasScripts(GLevel->Acs)) {
      serverStartRenderFramesTic = GLevel->TicTime+INITIAL_TICK_DELAY;
      GCon->Log("---");
      if (AcsHasScripts(GLevel->Acs)) GCon->Log("Found some ACS scripts");
      if (GLevel->scriptThinkers.length()) GCon->Logf("ACS thinkers: %d", GLevel->scriptThinkers.length());
    }
  }

  GCon->Logf("Spawned server for \"%s\".", mapname);
}


//==========================================================================
//
//  COMMAND PreSpawn
//
//==========================================================================
COMMAND(PreSpawn) {
  if (Source == SRC_Command) {
    GCon->Log("PreSpawn is not valid from console");
    return;
  }

  // allow server to send updates
  Player->Net->NeedsUpdate = true;

  // make sure level info is spawned on server side, since there could be some RPCs that depend on it
  VThinkerChannel *Chan = Player->Net->ThinkerChannels.FindPtr(GLevelInfo);
  if (!Chan) {
    Chan = (VThinkerChannel *)Player->Net->CreateChannel(CHANNEL_Thinker, -1, true); // local channel
    if (!Chan) Sys_Error("cannot allocate `LevelInfo` channel, this should NOT happen!");
    GCon->Logf(NAME_DevNet, "%s: created channel for `GLevelInfo`", *Player->Net->GetAddress());
    Chan->SetThinker(GLevelInfo);
  } else {
    GCon->Logf(NAME_DevNet, "%s: have existing channel for `GLevelInfo` (why?!)", *Chan->GetDebugName());
  }
  Chan->Update();
}


//==========================================================================
//
//  COMMAND Spawn
//
//==========================================================================
COMMAND(Client_Spawn) {
  if (Source == SRC_Command) {
    GCon->Log("Client_Spawn is not valid from console");
    return;
  }
  Player->SpawnClient();
}


//==========================================================================
//
//  SV_DropClient
//
//==========================================================================
void SV_DropClient (VBasePlayer *Player, bool crash) {
  if (!crash) {
    if (GLevel && GLevel->Acs) {
      GLevel->Acs->StartTypedACScripts(SCRIPT_Disconnect, SV_GetPlayerNum(Player), 0, 0, nullptr, true, false);
    }
    if (Player->PlayerFlags&VBasePlayer::PF_Spawned) Player->eventDisconnectClient();
  } else {
    // this is sudden network disconnect, kill player mobile (if any)
    // if we won't do this, player mobile will be left in the game, and will become Voodoo Doll
    if ((Player->PlayerFlags&VBasePlayer::PF_Spawned) && Player->MO) {
      GCon->Logf(NAME_DevNet, "killing player #%d mobile", SV_GetPlayerNum(Player));
      Player->eventDisconnectClient();
    }
  }

  Player->PlayerFlags &= ~VBasePlayer::PF_Active;
  GGameInfo->Players[SV_GetPlayerNum(Player)] = nullptr;
  Player->PlayerFlags &= ~VBasePlayer::PF_Spawned;

  if (Player->PlayerReplicationInfo) Player->PlayerReplicationInfo->DestroyThinker();

  if (Player->Net) {
    GCon->Log(NAME_DevNet, "deleting player connection");
    delete Player->Net;
    Player->Net = nullptr;
  }

  --svs.num_connected;
  Player->UserInfo = VStr();
}


//==========================================================================
//
//  SV_ShutdownGame
//
//  This only happens at the end of a game, not between levels
//  This is also called on Host_Error, so it shouldn't cause any errors
//
//==========================================================================
void SV_ShutdownGame () {
  if (GGameInfo->NetMode == NM_None) return;

  #ifdef CLIENT
  if (GGameInfo->Flags&VGameInfo::GIF_Paused) {
    GGameInfo->Flags &= ~VGameInfo::GIF_Paused;
    GAudio->ResumeSound();
  }

  // stop sounds (especially looping!)
  GAudio->StopAllSound();

  if (cls.demorecording) CL_StopRecording();

  // clear drawer level
  if (Drawer) Drawer->RendLev = nullptr;
  #endif

  if (GGameInfo->NetMode == NM_Client) {
    #ifdef CLIENT
    if (cls.demoplayback) GClGame->eventDemoPlaybackStopped();

    // sends a disconnect message to the server
    if (!cls.demoplayback) {
      GCon->Log(NAME_Dev, "Sending clc_disconnect");
      if (cl->Net) {
        if (cl->Net->GetGeneralChannel()) cl->Net->GetGeneralChannel()->Close();
        cl->Net->Flush();
      }
    }

    delete cl->Net;
    cl->Net = nullptr;
    cl->ConditionalDestroy();

    if (GClLevel) {
      delete GClLevel;
      GClLevel = nullptr;
    }
    #endif
  } else {
    sv_loading = false;
    sv_map_travel = false;

    // make sure all the clients know we're disconnecting
    int count = NET_SendToAll(5);
    if (count) GCon->Logf("Shutdown server failed for %d clients", count);

    for (int i = 0; i < svs.max_clients; ++i) {
      if (GGameInfo->Players[i]) SV_DropClient(GGameInfo->Players[i], false);
    }

    // clear structures
    if (GLevel) {
      delete GLevel;
      GLevel = nullptr;
    }
    if (GGameInfo->WorldInfo) {
      delete GGameInfo->WorldInfo;
      GGameInfo->WorldInfo = nullptr;
    }
    for (int i = 0; i < MAXPLAYERS; ++i) {
      // save net pointer
      VNetConnection *OldNet = GPlayersBase[i]->Net;
      GPlayersBase[i]->GetClass()->DestructObject(GPlayersBase[i]);
      memset((vuint8 *)GPlayersBase[i]+sizeof(VObject), 0, GPlayersBase[i]->GetClass()->ClassSize-sizeof(VObject));
      // restore pointer
      GPlayersBase[i]->Net = OldNet;
    }
    memset(GGameInfo->Players, 0, sizeof(GGameInfo->Players));
    memset(&sv, 0, sizeof(sv));

    // tell master server that this server is gone
    if (GGameInfo->NetMode >= NM_DedicatedServer) {
      GNet->QuitMaster();
      LastMasterUpdate = 0;
    }
  }

  #ifdef CLIENT
  GClLevel = nullptr;
  cl = nullptr;
  cls.clearForDisconnect(); // this resets demo playback flag too

  if (GGameInfo->NetMode != NM_DedicatedServer) GClGame->eventDisconnected();
  #endif

  SV_InitBaseSlot();

  GGameInfo->NetMode = NM_None;
}


#ifdef CLIENT
//==========================================================================
//
//  COMMAND Restart
//
//==========================================================================
COMMAND(Restart) {
  //fprintf(stderr, "*****RESTART!\n");
  if (GGameInfo->NetMode != NM_Standalone) return;
  //if (!SV_LoadQuicksaveSlot())
  {
    // reload the level from scratch
    SV_SpawnServer(*GLevel->MapName, true/*spawn thinkers*/);
    if (GGameInfo->NetMode != NM_DedicatedServer) CL_SetupLocalPlayer();
  }
}
#endif


//==========================================================================
//
//  COMMAND Pause
//
//==========================================================================
COMMAND(Pause) {
  if (GGameInfo->NetMode == NM_None || GGameInfo->NetMode == NM_Client) return;

  CMD_FORWARD_TO_SERVER();

  if (!__dbg_cl_always_allow_pause) {
    if (GGameInfo->NetMode != NM_Standalone || svs.max_clients > 1) {
      if (Player) Player->Printf("%s", "You cannot pause in network game, sorry.");
      return;
    }
  }

  GGameInfo->Flags ^= VGameInfo::GIF_Paused;
  for (int i = 0; i < svs.max_clients; ++i) {
    if (GGameInfo->Players[i]) {
      GGameInfo->Players[i]->eventClientPause(!!(GGameInfo->Flags&VGameInfo::GIF_Paused));
    }
  }
}


//==========================================================================
//
//  COMMAND Stats
//
//==========================================================================
COMMAND(Stats) {
  CMD_FORWARD_TO_SERVER();
  if (Player) {
    Player->Printf("Kills: %d of %d", Player->KillCount, GLevelInfo->TotalKills);
    Player->Printf("Items: %d of %d", Player->ItemCount, GLevelInfo->TotalItems);
    Player->Printf("Secrets: %d of %d", Player->SecretCount, GLevelInfo->TotalSecret);
  }
}


//==========================================================================
//
//  SV_ConnectClient
//
//  initialises a client_t for a new net connection
//  this will only be called once for a player each game, not once
//  for each level change
//
//==========================================================================
void SV_ConnectClient (VBasePlayer *player) {
  if (player->Net) {
    GCon->Logf(NAME_DevNet, "Client %s connected", *player->Net->GetAddress());
    ServerNetContext->ClientConnections.Append(player->Net);
    player->Net->NeedsUpdate = false; // we're just connected, no need to send world updates yet
  }

  GGameInfo->Players[SV_GetPlayerNum(player)] = player;
  player->ClientNum = SV_GetPlayerNum(player);
  player->PlayerFlags |= VBasePlayer::PF_Active;

  player->PlayerFlags &= ~VBasePlayer::PF_Spawned;
  player->Level = GLevelInfo;
  if (!sv_loading) {
    //GCon->Logf(NAME_Debug, "player #%d: reborn!", SV_GetPlayerNum(player));
    player->MO = nullptr;
    player->PlayerState = PST_REBORN;
    player->eventPutClientIntoServer();
  }
  player->Frags = 0;
  player->Deaths = 0;

  player->PlayerReplicationInfo = (VPlayerReplicationInfo *)GLevel->SpawnThinker(GGameInfo->PlayerReplicationInfoClass);
  player->PlayerReplicationInfo->Player = player;
  player->PlayerReplicationInfo->PlayerNum = SV_GetPlayerNum(player);
}


//==========================================================================
//
//  SV_CheckForNewClients
//
//  check for new connections
//
//  returns `false` if we have no active connections
//
//==========================================================================
static bool SV_CheckForNewClients () {
  for (;;) {
    VSocketPublic *sock = GNet->CheckNewConnections();
    if (!sock) break;

    //GCon->Logf(NAME_DevNet, "SV_CheckForNewClients: got client at %s", *sock->Address);

    // init a new client structure
    int i;
    for (i = 0; i < svs.max_clients; ++i) if (!GGameInfo->Players[i]) break;
    if (i == svs.max_clients) Sys_Error("Host_CheckForNewClients: no free clients");

    VBasePlayer *Player = GPlayersBase[i];
    Player->Net = new VNetConnection(sock, ServerNetContext, Player);
    Player->Net->ObjMap->SetupClassLookup();
    Player->Net->GetPlayerChannel()->SetPlayer(Player);
    Player->Net->CreateChannel(CHANNEL_ObjectMap, -1, true); // local channel
    SV_ConnectClient(Player);
    ++svs.num_connected;
  }

  #ifdef CLIENT
  return true; // always
  #else
  return (svs.num_connected > 0);
  #endif
}


//==========================================================================
//
//  SV_ConnectBot
//
//==========================================================================
void SV_ConnectBot (const char *name) {
  int i;

  if (GGameInfo->NetMode == NM_None || GGameInfo->NetMode == NM_Client) {
    GCon->Log("Game is not running");
    return;
  }

  if (svs.num_connected >= svs.max_clients) {
    GCon->Log("Server is full");
    return;
  }

  // init a new client structure
  for (i = 0; i < svs.max_clients; ++i) if (!GGameInfo->Players[i]) break;
  if (i == svs.max_clients) Sys_Error("SV_ConnectBot: no free clients");

  VBasePlayer *Player = GPlayersBase[i];
  Player->PlayerFlags |= VBasePlayer::PF_IsBot;
  Player->PlayerName = name;
  SV_ConnectClient(Player);
  ++svs.num_connected;
  Player->SetUserInfo(Player->UserInfo);
  Player->SpawnClient();
}


//==========================================================================
//
//  COMMAND AddBot
//
//==========================================================================
COMMAND(AddBot) {
  SV_ConnectBot(Args.Num() > 1 ? *Args[1] : "");
}


//==========================================================================
//
//  Map
//
//==========================================================================
COMMAND_WITH_AC(Map) {
  VStr mapname;

  if (Args.Num() != 2) {
    GCon->Log("map <mapname> : change level");
    return;
  }
  mapname = Args[1];

  SV_ShutdownGame();

  // default the player start spot group to 0
  RebornPosition = 0;
  GGameInfo->RebornPosition = RebornPosition;

       if ((int)Skill < 0) Skill = 0;
  else if ((int)Skill >= P_GetNumSkills()) Skill = P_GetNumSkills()-1;

  SV_ResetPlayers();
  SV_SpawnServer(*mapname, true/*spawn thinkers*/);

  if (mapLoaded == LMT_Unknown) {
    if (GLevel->MapName == "e1m1") {
      if (GLevel->MapHashMD5 == "b49f7a6c519757d390d52667db7d8793") {
        mapLoaded = LastLoadedMapType::LMT_E1M1;
      } else {
        mapLoaded = LastLoadedMapType::LMT_OtherFirstD1;
      }
    } else if (GLevel->MapName == "map01") {
      if (GLevel->MapHashMD5 == "3c9902e376cca1e9c3be8763bdc21df5") {
        mapLoaded = LastLoadedMapType::LMT_MAP01;
      } else {
        mapLoaded = LastLoadedMapType::LMT_OtherFirstD2;
      }
    } else {
      mapLoaded = LastLoadedMapType::LMT_Other;
    }
  } else {
    mapLoaded = LastLoadedMapType::LMT_Other;
  }

#ifdef CLIENT
  if (GGameInfo->NetMode != NM_DedicatedServer) CL_SetupLocalPlayer();
#endif
}

//==========================================================================
//
//  COMMAND_AC Map
//
//==========================================================================
COMMAND_AC(Map) {
  VStr prefix = (aidx < args.length() ? args[aidx] : VStr());
  if (aidx == 1) {
    TArray<VStr> list;
    // prefer pwad maps
    if (fsys_PWadMaps.length()) {
      list.resize(fsys_PWadMaps.length());
      for (auto &&lmp : fsys_PWadMaps) list.append(lmp.mapname);
    } else {
      int mapcount = P_GetNumMaps();
      list.resize(mapcount);
      for (int f = 0; f < mapcount; ++f) {
        VName mlump = P_GetMapLumpName(f);
        if (mlump != NAME_None) list.append(VStr(mlump));
      }
    }
    if (list.length()) return AutoCompleteFromListCmd(prefix, list);
  }
  return VStr::EmptyString;
}


//==========================================================================
//
//  Host_CLIMapStartFound
//
//  called if CLI arguments contains some map selections command
//
//==========================================================================
void Host_CLIMapStartFound () {
  cliMapStartFound = true;
}


//==========================================================================
//
//  Host_IsCLIMapStartFound
//
//==========================================================================
bool Host_IsCLIMapStartFound () {
  return cliMapStartFound;
}


//==========================================================================
//
//  Host_StartTitleMap
//
//==========================================================================
bool Host_StartTitleMap () {
  if (cliMapStartFound) return false;

  static bool loadingTitlemap = false;

  if (cli_SVNoTitleMap > 0) return false;

  if (loadingTitlemap) {
    // it is completely fucked, ignore it
    static bool titlemapWarned = false;
    if (!titlemapWarned) {
      titlemapWarned = true;
      GCon->Log(NAME_Warning, "Your titlemap is fucked, I won't try to load it anymore.");
    }
    return false;
  }

  if (!FL_FileExists("maps/titlemap.wad") && W_CheckNumForName(NAME_titlemap) < 0) return false;

  loadingTitlemap = true;
  // default the player start spot group to 0
  RebornPosition = 0;
  GGameInfo->RebornPosition = RebornPosition;

  SV_ResetPlayers();
  SV_SpawnServer("titlemap", true/*spawn thinkers*/, true/*titlemap*/);
#ifdef CLIENT
  CL_SetupLocalPlayer();
#endif
  loadingTitlemap = false;
  return true;
}


//==========================================================================
//
//  COMMAND MaxPlayers
//
//==========================================================================
COMMAND(MaxPlayers) {
  if (Args.Num() < 2 || Args.Num() > 3) {
    GCon->Logf("maxplayers is %d", svs.max_clients);
    return;
  }

  if (GGameInfo->NetMode != NM_None && GGameInfo->NetMode != NM_Client) {
    GCon->Log("maxplayers can not be changed while a server is running.");
    return;
  }

  int n = VStr::atoi(*Args[1]);
  if (n < 1) n = 1;
  if (n > MAXPLAYERS) {
    n = MAXPLAYERS;
    GCon->Logf("maxplayers set to %d", n);
  }
  svs.max_clients = n;

  int dmMode = 2;
  if (Args.length() > 2) {
    dmMode = VStr::atoi(*Args[2]);
    if (dmMode < 0 || dmMode > 2) dmMode = 2;
  }

  if (n == 1) {
#ifdef CLIENT
    GCmdBuf << "listen 0\n";
#endif
    DeathMatch = 0;
    NoMonsters = (cli_NoMonsters > 0 ? 1 : 0);
  } else {
#ifdef CLIENT
    GCmdBuf << "listen 1\n";
#endif
    DeathMatch = dmMode;
    if (dmMode) {
      NoMonsters = 1;
    } else {
      //NoMonsters = (dmMode ? 1 : 0);
      NoMonsters = (cli_NoMonsters > 0 ? 1 : 0);
    }
  }
}


//==========================================================================
//
//  SV_ServerInterframeBK
//
//==========================================================================
void SV_ServerInterframeBK () {
  // if we're running a server (either dedicated, or combined, and we are in game), process server bookkeeping
  if (GGameInfo->NetMode == NM_DedicatedServer || GGameInfo->NetMode == NM_ListenServer) {
    SV_SendClientMessages(false); // not full
  }
}


//==========================================================================
//
//  SV_AllClientsNeedsWorldUpdate
//
//  force world update on the next tick
//
//==========================================================================
void SV_AllClientsNeedsWorldUpdate () {
  for (int i = 0; i < MAXPLAYERS; ++i) {
    VBasePlayer *Player = GGameInfo->Players[i];
    if (!Player || !Player->Net) continue;
    Player->Net->NeedsUpdate = true;
  }
}


//==========================================================================
//
//  NET_SendNetworkHeartbeat
//
//==========================================================================
void NET_SendNetworkHeartbeat (bool forced) {
#ifdef CLIENT
  CL_NetworkHeartbeat(forced);
#endif
#ifdef SERVER
  SV_NetworkHeartbeat(forced);
#endif
}


//==========================================================================
//
//  ServerFrame
//
//==========================================================================
void ServerFrame (int realtics) {
  const bool haveClients = SV_CheckForNewClients();

  // there is no need to tick if we have no active clients
  // no, really
  if (haveClients || sv_loading || sv.intermission) {
    if (real_time) {
      SV_Ticker();
    } else {
      // run the count dics
      while (realtics--) SV_Ticker();
    }
  }

  if (mapteleport_issued) SV_MapTeleport(GLevelInfo->NextMap, mapteleport_flags, mapteleport_skill);

  SV_SendClientMessages(); // full
  SV_UpdateMaster();
}


//==========================================================================
//
//  SV_FindClassFromEditorId
//
//==========================================================================
VClass *SV_FindClassFromEditorId (int Id, int GameFilter) {
  mobjinfo_t *nfo = VClass::FindMObjId(Id, GameFilter);
  if (nfo) return nfo->Class;
  return nullptr;
}


//==========================================================================
//
//  SV_FindClassFromScriptId
//
//==========================================================================
VClass *SV_FindClassFromScriptId (int Id, int GameFilter) {
  mobjinfo_t *nfo = VClass::FindScriptId(Id, GameFilter);
  if (nfo) return nfo->Class;
  return nullptr;
}


//==========================================================================
//
//  COMMAND Say
//
//==========================================================================
COMMAND(Say) {
  CMD_FORWARD_TO_SERVER();
  if (Args.Num() < 2) return;

  VStr Text;
  for (int i = 1; i < Args.length(); ++i) {
    VStr s = Args[i].xstrip();
    if (!s.isEmpty()) {
      if (!Text.isEmpty()) Text += " ";
      Text += s;
    }
  }
  Text = Text.xstrip();
  if (Text.isEmpty()) return;
  GLevelInfo->BroadcastChatPrint(Player->PlayerName, Text);
  GLevelInfo->StartSound(TVec(0, 0, 0), 0, GSoundManager->GetSoundID("misc/chat"), 0, 1.0f, 0, false);
  #ifndef CLIENT
  Text = VStr("[")+Player->PlayerName.RemoveColors().xstrip()+"]:";
  for (int i = 1; i < Args.length(); ++i) {
    Text += " ";
    Text += Args[i].RemoveColors().xstrip();
  }
  GCon->Logf(NAME_Chat, "%s", *Text);
  #endif
}


//==========================================================================
//
//  COMMAND gc_toggle_stats
//
//==========================================================================
COMMAND(gc_toggle_stats) {
  VObject::GGCMessagesAllowed = !VObject::GGCMessagesAllowed;
}


//==========================================================================
//
//  COMMAND gc_show_all_objects
//
//==========================================================================
COMMAND(gc_show_all_objects) {
  int total = VObject::GetObjectsCount();
  GCon->Log("===============");
  GCon->Logf("total array size: %d", total);
  for (int f = 0; f < total; ++f) {
    VObject *o = VObject::GetIndexObject(f);
    if (!o) continue;
         if (o->GetFlags()&_OF_Destroyed) GCon->Logf("  #%5d: %p: DESTROYED! (%s)", f, o, o->GetClass()->GetName());
    else if (o->GetFlags()&_OF_DelayedDestroy) GCon->Logf("  #%5d: %p: DELAYED! (%s)", f, o, o->GetClass()->GetName());
    else GCon->Logf("  #%5d: %p: `%s`", f, o, o->GetClass()->GetName());
  }
}


//==========================================================================
//
//  VServerNetContext::GetLevel
//
//==========================================================================
VLevel *VServerNetContext::GetLevel () {
  return GLevel;
}


//==========================================================================
//
//  COMMAND NetChanInfo
//
//==========================================================================
COMMAND(NetChanInfo) {
  if (GGameInfo->NetMode <= NM_Standalone) {
    GCon->Logf(NAME_Error, "no netchan info available without a network game!");
    return;
  }

  #ifdef CLIENT
  if (GGameInfo->NetMode == NM_Client) {
    if (!cl || !cl->Net) return;
    GCon->Logf(NAME_DevNet, "%s: client: %d open channels", *cl->Net->GetAddress(), cl->Net->OpenChannels.Length());
    return;
  }
  #endif

  if (GGameInfo->NetMode == NM_DedicatedServer || GGameInfo->NetMode == NM_ListenServer) {
    for (int i = 0; i < MAXPLAYERS; ++i) {
      VBasePlayer *plr = GGameInfo->Players[i];
      if (!plr || !plr->Net) continue;
      GCon->Logf(NAME_DevNet, "%s: player #%d: %d open channels", *plr->Net->GetAddress(), i, plr->Net->OpenChannels.Length());
    }
  }
}
