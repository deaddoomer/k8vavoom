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
#ifdef HAVE_FOSSIL_COMMIT_HASH
# include "fossilversion.h"
#endif
#include "gamedefs.h"
#include "psim/p_player.h"
#include "psim/p_entity.h"
#include "server/server.h"
#include "host.h"
#ifdef CLIENT
# include "screen.h"
# include "client/cl_local.h"
# include "widgets/ui.h"
# include "input.h"
# include "video.h"
# include "text.h"
# include "automap.h"
# include "client/client.h"
# include "menu.h"
# include "sbar.h"
# include "chat.h"
#endif
#include "net/network.h"
#include "decorate/vc_decorate.h"
#include "language.h"
#include "mapinfo.h"
#include "filesys/files.h"
#include "sound/sound.h"

// we need it to init some data even in the server
#include "render/r_public.h"

#include <time.h>
#ifndef WIN32
# include <fcntl.h>
# include <unistd.h>
# include <sys/stat.h>
# include <sys/types.h>
#endif

#include "cvar.h"

#define VV_USE_U64_SYSTIME_FOR_FRAME_TIMES


extern int fsys_warp_n0;
extern int fsys_warp_n1;
extern VStr fsys_warp_cmd;


static int cli_SetDeveloper = -1;
static int cli_SetDeveloperDefine = -1;
static int cli_AsmDump = -1;
static int cli_DumpAllVars = -1;
int cli_ShowEndText = 0;

/*static*/ bool cliRegister_host_args =
  VParsedArgs::RegisterFlagSet("-developer", "!do not use if you don't know what it is (and you certainly don't know)", &cli_SetDeveloper) &&
  VParsedArgs::RegisterFlagReset("-vc-no-k8-developer", "!", &cli_SetDeveloperDefine) &&
  VParsedArgs::RegisterFlagSet("-vc-k8-developer", "!", &cli_SetDeveloperDefine) &&
  VParsedArgs::RegisterFlagSet("-endtext", "!enable end text (disabled by default)", &cli_ShowEndText) &&
  VParsedArgs::RegisterFlagSet("-vc-dump-asm", "!dump generated IR code", &cli_AsmDump) &&
  VParsedArgs::RegisterFlagSet("-con-dump-all-vars", "!dump all console variables", &cli_DumpAllVars);

const char *cli_LogFileName = nullptr;

/*static*/ bool cliRegister_con_args =
  VParsedArgs::RegisterStringOption("-logfile", "specify log file name", &cli_LogFileName) &&
  VParsedArgs::RegisterAlias("-log-file", "-logfile") &&
  VParsedArgs::RegisterAlias("--log-file", "-logfile");



// state updates, number of tics/second
#define TICRATE  (35)


class EndGame : public VavoomError {
public:
  explicit EndGame (const char *txt) : VavoomError(txt) {}
};


void Host_Quit ();


VCvarB developer("developer", false, "Developer (debug) mode?", CVAR_PreInit|CVAR_NoShadow/*|CVAR_Archive*/);

#ifdef VAVOOM_K8_DEVELOPER
# define CVAR_K8_DEV_VALUE  true
#else
# define CVAR_K8_DEV_VALUE  false
#endif
#if defined(_WIN32) || defined(__SWITCH__)
# undef CVAR_K8_DEV_VALUE
# define CVAR_K8_DEV_VALUE  false
#endif
VCvarB k8vavoom_developer_version("k8vavoom_developer_version", CVAR_K8_DEV_VALUE, "Don't even think about this.", CVAR_Rom|CVAR_Hidden|CVAR_NoShadow);


static double hostLastGCTime = 0.0;

float host_frametime = 0.0f; // time delta for the current frame
double host_systime = 0.0; // current `Sys_Time()`; used for consistency, updated in `FilterTime()`
uint64_t host_systime64_usec = 0; // monotonic time, in microseconds
int host_framecount = 0; // used in demo playback

#ifdef VV_USE_U64_SYSTIME_FOR_FRAME_TIMES
static uint64_t host_prevsystime64_usec = 0; // monotonic time, in microseconds
#else
static double last_time = 0.0; // last time `FilterTime()` was returned `true`
#endif

bool host_initialised = false;
bool host_request_exit = false;
bool host_gdb_mode = false;


#if defined(_WIN32) || defined(__SWITCH__)
# define VV_CVAR_RELEASE_MODE  true
#else
# define VV_CVAR_RELEASE_MODE  false
#endif
VCvarB game_release_mode("release_mode", VV_CVAR_RELEASE_MODE, "Affects some default settings.", CVAR_Rom|CVAR_Hidden|CVAR_NoShadow);


// for chex quest support
//VCvarI game_override_mode("game_override", 0, "Override game type for DooM game.", CVAR_Rom|CVAR_Hidden);

static VCvarF dbg_frametime("dbg_frametime", "0", "If greater or equal to 0.004, this is what be used instead of one Doom tic; DEBUG CVAR, DON'T USE!", CVAR_PreInit|CVAR_NoShadow);
//k8: this was `3`; why 3? looks like arbitrary number
VCvarI host_max_skip_frames("dbg_host_max_skip_frames", "12", "Process no more than this number of full frames if frame rate is too slow; DEBUG CVAR, DON'T USE!", CVAR_PreInit|CVAR_NoShadow);
static VCvarB host_show_skip_limit("dbg_host_show_skip_limit", false, "Show skipframe limit hits? (DEBUG CVAR, DON'T USE!)", CVAR_PreInit|CVAR_NoShadow);
static VCvarB host_show_skip_frames("dbg_host_show_skip_frames", false, "Show skipframe hits? (DEBUG CVAR, DON'T USE!)", CVAR_PreInit|CVAR_NoShadow);

static VCvarF host_gc_timeout("host_gc_timeout", "0.5", "Timeout in seconds between garbage collections.", CVAR_Archive|CVAR_NoShadow);

static VCvarB randomclass("RandomClass", false, "Random player class?", 0); // checkparm of -randclass
VCvarB respawnparm("RespawnMonsters", false, "Respawn monsters?", 0/*|CVAR_PreInit*/); // checkparm of -respawn
VCvarI fastparm("g_fast_monsters", "0", "Fast(1), slow(2), normal(0) monsters?", 0/*|CVAR_PreInit*/); // checkparm of -fast

static VCvarI g_fastmon_override("g_fast_monsters_override", "0", "Fast(1), slow(2), normal(0) monsters?", CVAR_PreInit|CVAR_Archive);
static VCvarI g_respawn_override("g_monsters_respawn_override", "0", "Override monster respawn (time in seconds).", CVAR_PreInit|CVAR_Archive);
static VCvarI g_spawn_filter_override("g_skill_spawn_filter_override", "0", "Override spawn flags.", CVAR_PreInit);
static VCvarI g_spawn_multi_override("g_skill_spawn_multi_override", "0", "Override \"spawn multi\" flag.", CVAR_PreInit);
static VCvarI g_spawn_limit_override("g_skill_respawn_limit_override", "0", "Override spawn limit.", CVAR_PreInit|CVAR_Archive);
static VCvarF g_ammo_factor_override("g_skill_ammo_factor_override", "0", "Override ammo multiplier.", CVAR_PreInit|CVAR_Archive);
static VCvarF g_damage_factor_override("g_skill_damage_factor_override", "0", "Override damage multiplier.", CVAR_PreInit|CVAR_Archive);
static VCvarF g_aggressiveness_override("g_skill_aggressiveness_override", "0", "Override monster aggresiveness.", CVAR_PreInit|CVAR_Archive);
static VCvarI g_switch_range_check_override("g_switch_range_check_override", "0", "Override switch range checking (0: default; 1: never; 2: always).", CVAR_PreInit|CVAR_Archive);

static VCvarI g_spawnmulti_mask("g_spawnmulti_mask", "0", "0:coop+dm; 1:dm; 2:coop; 3::coop+dm", CVAR_PreInit|CVAR_Archive);


static VCvarB show_time("dbg_show_times", false, "Show some debug times?", CVAR_PreInit|CVAR_NoShadow);

static VCvarB cfg_saving_allowed("cfg_saving_allowed", true, "Is config saving allowed?", CVAR_PreInit|CVAR_NoShadow);

static char CurrentLanguage[4];
static VCvarS Language("language", "en", "Game language.", /*CVAR_Archive|CVAR_PreInit|*/CVAR_Rom|CVAR_NoShadow);

static VCvarB cl_cap_framerate("cl_cap_framerate", true, "Cap framerate for non-networking games?", CVAR_Archive|CVAR_NoShadow);
static VCvarI cl_framerate("cl_framerate", "70", "Framerate cap for client rendering.", CVAR_Archive|CVAR_NoShadow);
static VCvarI sv_framerate("sv_framerate", "70", "Framerate cap for dedicated server.", CVAR_Archive|CVAR_NoShadow);

// this is hack for my GPU
#ifdef CLIENT
static double clientBadNetTimoutReleaseTime = 0.0;
static VCvarI cl_framerate_net_timeout("cl_framerate_net_timeout", "28", "If we have a dangerous client timeout, slow down rendering for a while.", CVAR_NoShadow/*|CVAR_Archive*/);
#endif


#include "dedlog.cpp"


//==========================================================================
//
//  StreamHostError
//
//==========================================================================
static __attribute__((noreturn)) void StreamHostError (const char *msg) {
  if (!msg || !msg[0]) msg = "stream error";
  Host_Error("%s", msg);
}


//==========================================================================
//
//  Host_InitStreamCallbacks
//
//==========================================================================
void Host_InitStreamCallbacks () {
  VCheckedStreamAbortFnDefault = &StreamHostError;
}


struct ECounterInfo {
  VClass *cc;
  int count;
};


//==========================================================================
//
//  cmpCounterInfo
//
//==========================================================================
static VVA_OKUNUSED int cmpCounterInfo (const void *aa, const void *bb, void *) {
  const ECounterInfo *a = (const ECounterInfo *)aa;
  const ECounterInfo *b = (const ECounterInfo *)bb;
  return b->count-a->count;
}


//==========================================================================
//
//  CountAllEntities
//
//==========================================================================
static VVA_OKUNUSED void CountAllEntities () {
  TMapNC<VClass *, int> cmap;
  const int ocount = VObject::GetObjectsCount();
  for (int f = 0; f < ocount; ++f) {
    VObject *o = VObject::GetIndexObject(f);
    if (!o) continue;
    VClass *cc = o->GetClass();
    auto xp = cmap.get(cc);
    if (xp) {
      ++(*xp);
    } else {
      cmap.put(cc, 1);
    }
  }
  TArray<ECounterInfo> olist;
  for (auto &&it : cmap.first()) {
    ECounterInfo &nfo = olist.alloc();
    nfo.cc = it.getKey();
    nfo.count = it.getValue();
  }
  smsort_r(olist.ptr(), olist.length(), sizeof(ECounterInfo), &cmpCounterInfo, nullptr);
  GCon->Logf(NAME_Debug, "=== %c object types ===", olist.length());
  for (auto &&it : olist) GCon->Logf("  %5d: %s", it.count, it.cc->GetName());
}


//==========================================================================
//
//  Host_CollectGarbage
//
//==========================================================================
// this does GC rougly twice per second (unless forced)
void Host_CollectGarbage (bool forced, bool resetUniqueId) {
  const double ctt = Sys_Time();
  if (!forced) {
    float tout = host_gc_timeout.asFloat();
    if (!isFiniteF(tout) || tout < 0.0f) tout = 0.0f; else if (tout > 13.0f) tout = 13.0f; // arbitrary limit
    if (tout > 0.0f && ctt-hostLastGCTime < (double)tout) {
      //GCon->Logf(NAME_Debug, "*** gc timeout: %g", (double)tout-(ctt-hostLastGCTime));
      return; // nothing to do yet
    }
  }
  //GCon->Logf(NAME_Debug, "*** GC! ***");
  hostLastGCTime = ctt;
  VObject::CollectGarbage();
  //CountAllEntities();

  // reset unique id if we're not in a game (titlemap is "in a game") too
  if (!resetUniqueId) {
    // if we have `GGameInfo`, it means that we have server mode
    if (GGameInfo) {
      if (GGameInfo->NetMode != NM_None) return;
    }
    #ifdef CLIENT
    // for client: if we have a client, we're in game
    else {
      if (cl) return;
    }
    #endif
  }
  {
    const vuint32 olduid = VObject::GetCurrentUniqueId();
    VObject::MinimiseUniqueId();
    const vuint32 newuid = VObject::GetCurrentUniqueId();
    vassert(newuid <= olduid);
    if (resetUniqueId && newuid < olduid) {
      GCon->Logf(NAME_Debug, "new unique id is %u (shrunk by %u); don't worry, this is normal garbage collection cycle.", newuid, (unsigned)(olduid-newuid));
    }
  }
}


//==========================================================================
//
//  Host_Init
//
//==========================================================================
void Host_Init () {
  #ifndef WIN32
  { // fuck off, WSL
    int pfd = open("/proc/version", O_RDONLY);
    if (pfd >= 0) {
      VStr pvstr;
      while (pvstr.length() < 1024*1024) {
        char ch = 0;
        if (read(pfd, &ch, 1) != 1) break;
        if (!ch) ch = ' ';
        if (ch >= 'A' && ch <= 'Z') ch = ch-'A'+'a';
        pvstr += ch;
      }
      close(pfd);
      //fprintf(stderr, "<%s>\n", *pvstr);
      if (pvstr.indexOf("microsoft") >= 0) abort();
    }
  }
  #endif

  (void)Sys_Time(); // this initializes timer

  if (cli_SetDeveloper > 0) developer = true;

  #ifdef CLIENT
  C_Init(); // init console
  #endif
  DD_SetupLog();

  /*
  {
    VStr cfgdir = FL_GetConfigDir();
    OpenDebugFile(va("%s/debug.txt", *cfgdir));
  }
  */

  // seed the random-number generator
  RandomInit();

  // init subsystems
  VName::StaticInit();
  VObject::StaticInit();

  Cvars_Init();
  VCommand::Init();

  VObject::cliShowPackageLoading = true;

  GCon->Log(NAME_Init, "---------------------------------------------------------------");
  GCon->Log(NAME_Init, "k8vavoom Game Engine, (c) 1999-2023 Vavoom and k8vavoom devs");
  GCon->Logf(NAME_Init, "project started by Janis Legzdinsh                | %s", __DATE__);
  GCon->Log(NAME_Init, "also starring Francisco Ortega, and others (k8:drop me a note!)");
  GCon->Log(NAME_Init, "Ketmar Dark: improvements, bugfixes, new bugs, segfaults, etc.");
  GCon->Log(NAME_Init, "id0: support, testing, motivation for 88% of features.");
  GCon->Log(NAME_Init, "---------------------------------------------------------------");
  GCon->Log(NAME_Init, "project site: http://ketmar.no-ip.org/fossil/k8vavoom/");
  GCon->Log(NAME_Init, " Ketmar Dark: ketmar@ketmar.no-ip.org");
  GCon->Log(NAME_Init, "         id0: vasily.boytsov@proton.me");
  GCon->Log(NAME_Init, "---------------------------------------------------------------");
  GCon->Log(NAME_Init, "REMEMBER: BY USING FOSS SOFTWARE, YOU ARE SUPPORTING COMMUNISM!");
  GCon->Log(NAME_Init, "---------------------------------------------------------------");
  #if defined(FOSSIL_COMMIT_HASH)
  GCon->Log(NAME_Init, "Fossil commit hash: " FOSSIL_COMMIT_HASH);
  #endif
  GCon->Logf(NAME_Init, "Memory allocator: %s", Z_GetAllocatorType());

  //{ GCon->Logf(NAME_Init, "HOST:::ARGC=%d", GArgs.Count()); for (int f = 0; f < GArgs.Count(); ++f) GCon->Logf(NAME_Init, "  #%d: <%s>", f, GArgs[f]); }
  if (cli_SetDeveloperDefine < 0) {
    #ifdef VAVOOM_K8_DEVELOPER
    cli_SetDeveloperDefine = 1;
    #else
    cli_SetDeveloperDefine = 0;
    #endif
  }

  if (Sys_TimeMinPeriodMS()) GCon->Logf(NAME_Init, "timeBeginPeriod minimum: %d", Sys_TimeMinPeriodMS());
  if (Sys_TimeMaxPeriodMS()) GCon->Logf(NAME_Init, "timeBeginPeriod maximum: %d", Sys_TimeMaxPeriodMS());

  if (cli_AsmDump > 0) VMemberBase::doAsmDump = true;

  if (cli_SetDeveloperDefine > 0) VMemberBase::StaticAddDefine("K8_DEVELOPER");

  #ifdef SERVER
  VMemberBase::StaticAddDefine("SERVER");
  #endif
  #ifdef CLIENT
  VMemberBase::StaticAddDefine("CLIENT");
  #else
  // allow unimplemented builtins for dedicated server (temp. band-aid)
  VObject::engineAllowNotImplementedBuiltins = true;
  VObject::cliShowUndefinedBuiltins = false;
  #endif

  #ifdef CLIENT
  V_Init(true); // moved here, so we can show a splash screen
  C_SplashActive(true);
  #endif

  FL_ProcessPreInits();

  FL_Init();

  VObject::PR_Init();

  GLanguage.LoadStrings("en");
  strcpy(CurrentLanguage, "en");

  GSoundManager = new VSoundManager;
  GSoundManager->Init();
  R_InitData();
  R_InitTexture();
  //TODO: only for clients?
  //done in `R_InitTexture`
  //R_InitHiResTextures();

  GNet = VNetworkPublic::Create();
  GNet->Init();

  ReadLineSpecialInfos();

  #ifdef SERVER
  // this creates GameInfo, and then loads DECORATE
  SV_LoadMods();
  #endif
  #ifdef CLIENT
  CL_LoadMods();
  #endif

  // now compile loaded VavoomC code
  SV_CompileScripts();

  // "compile only"
  if (cli_CompileAndExit) {
    Z_Exit(0);
  }

  #ifdef SERVER
  SV_Init();
  #endif
  #ifdef CLIENT
  CL_Init();
  #endif


  VThinker::ThinkerStaticInit();
  VEntity::EntityStaticInit();
  VLevel::LevelStaticInit();

#ifdef CLIENT
  GInput = VInputPublic::Create();
  GInput->Init();
  GAudio = VAudioPublic::Create();
  if (GAudio) GAudio->Init();

  SCR_Init();
  CT_Init();
  //V_Init(); // moved to the top, so we can show a splash screen

  R_Init();

  T_Init();

  MN_Init();
  AM_Init();
  SB_Init();
#endif

#ifdef SERVER
  // normally, this is done in `R_Init()`, but server has no video
  // still, we need skybox info for server too (scripts and such)
  R_InitSkyBoxes();
#endif

  //VCommand::ProcessKeyConf();

  R_ParseEffectDefs();

  InitMapInfo(); // this also loads keyconf files
  P_SetupMapinfoPlayerClasses();
  GGameInfo->eventPostPlayerClassInit();

  //GCon->Logf(NAME_Debug, "*** WARP: n0=%d; n1=%d; cmd=<%s>", fsys_warp_n0, fsys_warp_n1, *fsys_warp_cmd);

#ifndef CLIENT
  bool wasWarp = false;

  if (GGameInfo->NetMode == NM_None) {
    //GCmdBuf << "MaxPlayers 4\n";
    VCommand::cliPreCmds += "MaxPlayers 4\n";
  }
#endif

  if (fsys_warp_n0 >= 0 && fsys_warp_n1 < 0) {
    VName maplump = P_TranslateMapEx(fsys_warp_n0);
    if (maplump != NAME_None) {
      Host_CLIMapStartFound();
      GCon->Logf("WARP: %d translated to '%s'", fsys_warp_n0, *maplump);
      fsys_warp_n0 = -1;
      VStr mcmd = "map ";
      mcmd += *maplump;
      mcmd += "\n";
      //GCmdBuf.Insert(mcmd);
      VCommand::cliPreCmds += mcmd;
      fsys_warp_cmd = VStr();
#ifndef CLIENT
      wasWarp = true;
#endif
    }
  }

  if (/*fsys_warp_n0 >= 0 &&*/ fsys_warp_cmd.length()) {
    //GCmdBuf.Insert(fsys_warp_cmd);
    Host_CLIMapStartFound();
    VCommand::cliPreCmds += fsys_warp_cmd;
    if (!fsys_warp_cmd.endsWith("\n")) VCommand::cliPreCmds += '\n';
#ifndef CLIENT
    wasWarp = true;
#endif
  }

  GCmdBuf.Exec();

#ifdef CLIENT
  //FIXME
  //R_InitHiResTextures(); // init only hires replacements
  // here, so voxel optimisation options could be loaded from config first
  R_InitAliasModels();
#endif

#ifndef CLIENT
  if (GGameInfo->NetMode == NM_None && !wasWarp) {
    Host_CLIMapStartFound();
    //GCmdBuf << "MaxPlayers 4\n";
    //GCmdBuf << "Map " << *P_TranslateMap(1) << "\n";
    VCommand::cliPreCmds += "Map \"";
    VCommand::cliPreCmds += VStr(*P_TranslateMap(1)).quote();
    VCommand::cliPreCmds += "\"\n";
  }
#endif

  FL_BuildRequiredWads();

  host_initialised = true;
  VCvar::HostInitComplete();
}


//==========================================================================
//
//  Host_GetConsoleCommands
//
//  Add them exactly as if they had been typed at the console
//
//==========================================================================
static void Host_GetConsoleCommands () {
  char *cmd;

#ifdef CLIENT
  if (GGameInfo->NetMode != NM_DedicatedServer) return;
#endif

  for (cmd = Sys_ConsoleInput(); cmd; cmd = Sys_ConsoleInput()) {
    GCmdBuf << cmd << "\n";
  }
}


//==========================================================================
//
//  Host_ResetSkipFrames
//
//  call this after saving/loading/map loading, so we won't
//  unnecessarily skip frames
//
//==========================================================================
void Host_ResetSkipFrames () {
  host_systime = Sys_Time_ExU(&host_systime64_usec);
  #ifdef VV_USE_U64_SYSTIME_FOR_FRAME_TIMES
  host_prevsystime64_usec = host_systime64_usec;
  #else
  last_time = host_systime;
  #endif
  host_frametime = 0.0f;
  //GCon->Logf("*** Host_ResetSkipFrames; ctime=%f", last_time);
}


//==========================================================================
//
//  Host_IsDangerousTimeout
//
//  check for dangerous network timeout
//
//==========================================================================
bool Host_IsDangerousTimeout () {
  #ifdef CLIENT
  if (!cl || !cl->Net || (GGameInfo->NetMode != NM_Client && GGameInfo->NetMode != NM_ListenServer)) {
    clientBadNetTimoutReleaseTime = 0;
  } else if (CL_IsDangerousTimeout()) {
    clientBadNetTimoutReleaseTime = host_systime+15; // slowdown it for 15 seconds
  }
  return (clientBadNetTimoutReleaseTime > 0 && clientBadNetTimoutReleaseTime > host_systime);
  #else
  return false;
  #endif
}


#ifdef VV_USE_U64_SYSTIME_FOR_FRAME_TIMES
//==========================================================================
//
//  CalcFrameMicro
//
//==========================================================================
static VVA_FORCEINLINE unsigned CalcFrameMicro (const unsigned fps) noexcept {
  return 1000000u/fps;
}
#endif


//==========================================================================
//
//  FilterTime
//
//  Returns false if the time is too short to run a frame
//
//==========================================================================
static bool FilterTime () {
  const double curr_time = Sys_Time_ExU(&host_systime64_usec);
  // update it here, it is used as a substitute for `Sys_Time()` in demo and other code
  host_systime = curr_time;

  #ifdef VV_USE_U64_SYSTIME_FOR_FRAME_TIMES
  if (!host_prevsystime64_usec) host_prevsystime64_usec = host_systime64_usec;
  // start of U64 ticker
  unsigned usDelta = (unsigned)(host_systime64_usec-host_prevsystime64_usec);
  if (usDelta < 4000u) return false; // no more than 250 frames per second
  if (usDelta > 10000000U) usDelta = 10000000U; // 10 seconds

  float timeDelta;
  if (dbg_frametime < max_fps_cap_float) {
    #ifdef CLIENT
    // check for dangerous network timeout
    if (Host_IsDangerousTimeout()) {
      const unsigned capfr = (unsigned)clampval(cl_framerate_net_timeout.asInt(), 0, 42);
      if (capfr > 0u) {
        if (usDelta < CalcFrameMicro(capfr)) return false; // framerate is too high
      }
    } else {
      // cap client fps
      unsigned ftime;
      if (GGameInfo->NetMode <= NM_Standalone) {
        // local game
        if (cl_cap_framerate.asBool()) {
          const unsigned frate = (unsigned)clampval(cl_framerate.asInt(), 1, 250);
          ftime = CalcFrameMicro(frate);
        } else {
          ftime = 4000u/2u; // no more than 250^w 500 frames per second
        }
      } else {
        // network game
        const unsigned frate = (unsigned)clampval(sv_framerate.asInt(), 5, 70);
        ftime = CalcFrameMicro(frate);
      }
      //GCon->Logf("*** FilterTime; lasttime=%g; ctime=%g; time=%g; ftime=%g; cfr=%g", last_time, curr_time, time, ftime, 1.0/(double)ftime);
      if (usDelta < ftime) return false; // framerate is too high
    }
    #else
    // dedicated server
    const unsigned frate = (unsigned)clampval(sv_framerate.asInt(), 5, 70);
    const unsigned ftime = CalcFrameMicro(frate);
    if (usDelta < ftime) return false; // framerate is too high
    #endif
    timeDelta = (float)usDelta/1000000.0f;
  } else {
    // force ticker time per Doom tick
    if (usDelta < 28571u) return false; // one Doom tick is ~28.571428571429 milliseconds
    timeDelta = dbg_frametime;
  }
  // end of U64 ticker
  #else
  // start of floating ticker
  // add fractional frame time left from previous tick, so we won't miss it
  // this gives higher framerate around `cl_framerate 39`, but the game feels much better
  float timeDelta = (float)(curr_time-last_time);

  if (dbg_frametime < max_fps_cap_float) {
    // freestep mode
    #ifdef CLIENT
    // check for dangerous network timeout
    if (Host_IsDangerousTimeout()) {
      int capfr = min2(42, cl_framerate_net_timeout.asInt());
      if (capfr > 0) {
        if (timeDelta < 1.0f/(float)capfr) return false; // framerate is too high
      }
    } else {
      // cap client fps
      float ftime;
      if (GGameInfo->NetMode <= NM_Standalone) {
        // local game
        if (cl_cap_framerate.asBool()) {
          int cfr = cl_framerate.asInt();
          if (cfr < 1 || cfr > 250) cfr = 140;
          ftime = 1.0f/(float)cfr;
        } else {
          ftime = max_fps_cap_float;
        }
      } else {
        // network game
        const int sfr = clampval(sv_framerate.asInt(), 5, 70);
        ftime = 1.0f/(float)sfr;
      }
      //GCon->Logf("*** FilterTime; lasttime=%g; ctime=%g; time=%g; ftime=%g; cfr=%g", last_time, curr_time, time, ftime, 1.0/(double)ftime);
      if (timeDelta < ftime) return false; // framerate is too high
    }
    #else
    // dedicated server
    const int sfr = clampval(sv_framerate.asInt(), 5, 70);
    if (timeDelta < 1.0f/(float)sfr) return false; // framerate is too high
    //GCon->Logf(NAME_Debug, "headless tick! %g", timeDelta*1000);
    #endif
  } else {
    // force ticker time per Doom tick
    if (timeDelta < SV_GetFrameTimeConstant()) return false;
    timeDelta = dbg_frametime;
  }
  // end of floating ticker
  #endif

  // here we can advance our "last success call" mark
  #ifdef VV_USE_U64_SYSTIME_FOR_FRAME_TIMES
  host_prevsystime64_usec = host_systime64_usec;
  #else
  last_time = curr_time;
  #endif

  //GCon->Logf(NAME_Debug, "timeDelta=%g", timeDelta);
  host_frametime = timeDelta;

  const int ticlimit = clampval(host_max_skip_frames.asInt(), 3, 256);

  if (dbg_frametime > 0.0f) {
    host_frametime = dbg_frametime;
  } else {
    // don't allow really long or short frames
    const float tld = SV_GetFrameTimeConstant()*(float)(ticlimit);
    if (host_frametime > tld) {
      if (developer) GCon->Logf(NAME_Dev, "*** FRAME TOO LONG: %f (capped to %f)", host_frametime, tld);
      host_frametime = tld;
    }
  }
  if (host_frametime < max_fps_cap_float) host_frametime = max_fps_cap_float; // just in case

  if (host_show_skip_limit || host_show_skip_frames) {
    // freestep mode, check if we want to skip too many frames, and show a warning
    const int ftics = (int)(timeDelta*TICRATE);
    if (ftics > ticlimit) {
           if (host_show_skip_limit) GCon->Logf(NAME_Warning, "want to skip %d tics, but only %d allowed", ftics, ticlimit);
      else if (developer) GCon->Logf(NAME_Dev, "want to skip %d tics, but only %d allowed", ftics, ticlimit);
    }
    if (ftics > 1 && host_show_skip_frames) GCon->Logf(NAME_Warning, "processing %d ticframes", ftics);
  }

  return true;
}


//==========================================================================
//
//  CalcYieldTime
//
//==========================================================================
static VVA_OKUNUSED unsigned CalcYieldTime () {
  #ifdef CLIENT
    #ifdef VV_USE_U64_SYSTIME_FOR_FRAME_TIMES
    if (!host_systime64_usec || !host_prevsystime64_usec) return 0;
    if (host_systime64_usec == host_prevsystime64_usec) return 0;
    unsigned ftime;
    if (GGameInfo->NetMode > NM_Standalone) return 100; // network game
    // local game
    if (cl_cap_framerate.asBool()) {
      const unsigned frate = (unsigned)clampval(cl_framerate.asInt(), 1, 250);
      ftime = CalcFrameMicro(frate);
    } else {
      ftime = 4000u/2u; // no more than 250^w 500 frames per second
    }
    const uint64_t nft = host_systime64_usec+(uint64_t)(ftime*1000U); // next frame time (mircoseconds)
    uint64_t ctt = Sys_Time_Micro(); // current time in microseconds
    if (ctt >= nft) return 0; // oops, right now
    ctt = nft-ctt;
    //if (ctt < 100) return 0; // don't wait
    //ctt -= 100; // just in case
    if (ctt > 3000) ctt = 3000; // no more than 3 msec
    return (unsigned)ctt;
    #else
    return 100;
    #endif
  #else
    // dedicated server
    return 100;
  #endif
}


//==========================================================================
//
//  Host_UpdateLanguage
//
//==========================================================================
static void Host_UpdateLanguage () {
  if (!Language.IsModified()) return;

  VStr NewLang = VStr((const char *)Language).ToLower();
  if (NewLang.Length() != 2 && NewLang.Length() != 3) {
    GCon->Log("Language identifier must be 2 or 3 characters long");
    Language = CurrentLanguage;
    return;
  }

  if (Language == CurrentLanguage) return;

  GLanguage.LoadStrings(*NewLang);
  VStr::Cpy(CurrentLanguage, *NewLang);
}


static double lastNetFrameTime = 0;

//==========================================================================
//
//  Host_Frame
//
//  Runs all active servers
//
//==========================================================================
void Host_Frame () {
  static double time1 = 0;
  static double time2 = 0;
  static double time3 = 0;
  int pass1, pass2, pass3;

  if (!lastNetFrameTime) lastNetFrameTime = Sys_Time();

  try {
    // decide the simulation time
    if (!FilterTime()) {
      // don't run frames too fast
      // but still process network activity, we may want to re-send packets and such
      if (lastNetFrameTime > host_systime) lastNetFrameTime = host_systime; // just in case
      if (GGameInfo->NetMode <= NM_Standalone) {
        // not a network game
        #ifdef CLIENT
        // process client
        CL_NetInterframe(); // this does all necessary checks
        #endif
        // don't do it too often, tho
        const unsigned yt = CalcYieldTime();
        //fprintf(stderr, "***SLEEP MICROSECS: %u\n", yt);
        if (yt) {
          #ifdef WIN32
          Sys_YieldMicro(0);
          #else
          Sys_YieldMicro(yt);
          #endif
        }
      } else {
        //const double ctt = Sys_Time();
        const double dtt = (GGameInfo->NetMode >= NM_DedicatedServer ? 1.0/200.0 : 1.0/70.0);
        if (host_systime-lastNetFrameTime >= dtt) {
          // perform network activity
          lastNetFrameTime = host_systime;
          #ifdef CLIENT
          // process client
          CL_NetInterframe(); // this does all necessary checks
          #endif
          #ifdef SERVER
          // if we're running a server (either dedicated, or combined, and we are in game), process server bookkeeping
          SV_ServerInterframeBK(); // this does all necessary checks
          #endif
        } else {
          // don't do it too often, tho
          #ifdef WIN32
          Sys_YieldMicro(50); // sleep for 0.05 milliseconds
          #else
          const unsigned yt = CalcYieldTime();
          //fprintf(stderr, "***SLEEP MICROSECS: %u\n", yt);
          if (yt) Sys_YieldMicro(yt);
          #endif
        }
      }
      return;
    }

    lastNetFrameTime = host_systime;

    if (GSoundManager) GSoundManager->Process();

    Host_UpdateLanguage();

    #ifdef CLIENT
    // get new key, mice and joystick events
    GInput->ProcessEvents();
    #endif

    // check for commands typed to the host
    Host_GetConsoleCommands();

    // process console commands
    GCmdBuf.Exec();
    if (host_request_exit) Host_Quit();

    bool incFrame = false;

    GNet->Poll();

    // if we perfomed load/save, frame time will be near zero, so do nothing
    if (host_frametime >= max_fps_cap_float) {
      incFrame = true;

      #ifdef CLIENT
      CL_SendMove(); // this also ticks network
      #endif

      #ifdef SERVER
      if (GGameInfo->NetMode != NM_None && GGameInfo->NetMode != NM_Client) {
        // server operations
        SV_ServerFrame();
      }
      # ifndef CLIENT
      else {
        // still check for rcon commands
        GNet->CheckNewConnections(true); // rcon only
      }
      # endif
      #endif

      //host_time += host_frametime;

      #ifdef CLIENT
      // fetch results from server
      CL_ReadFromServer(host_frametime);

      // update user interface
      GRoot->TickWidgets(host_frametime);
      #endif
    }

    #ifdef CLIENT
    // update video
    if (show_time) time1 = Sys_Time();
    SCR_Update();
    if (show_time) time2 = Sys_Time();

    // update audio
    GAudio->UpdateSounds();

    if (cls.signon) CL_DecayLights();
    #endif

    Host_CollectGarbage();

    if (show_time) {
      pass1 = (int)((time1-time3)*1000);
      time3 = Sys_Time();
      pass2 = (int)((time2-time1)*1000);
      pass3 = (int)((time3-time2)*1000);
      GCon->Logf("%d tot | %d server | %d gfx | %d snd", pass1+pass2+pass3, pass1, pass2, pass3);
    }

    if (incFrame) {
      ++host_framecount;
    } else {
      if (developer) GCon->Log(NAME_Dev, "Frame delayed due to lengthy operation (this is perfectly ok)");
    }
  } catch (RecoverableError &e) {
    //GCon->Logf(NAME_Error, "Host Error: \034[RedError]%s\034[Untranslated]", e.message);
    GCon->Logf(NAME_Error, "Host Error: %s", e.message);
    /* color test
    GCon->Logf(NAME_Warning, "W:Host Error: %s", e.message);
    GCon->Logf(NAME_Init, "I:Host Error: %s", e.message);
    GCon->Logf(NAME_Debug, "D:Host Error: %s", e.message);
    GCon->Logf(NAME_DevNet, "N:Host Error: %s", e.message);
    GCon->Logf("Host Error: %s", e.message);
    */

    // reset progs virtual machine
    VObject::PR_OnAbort();
    // make sure, that we use primary wad files
    //k8: no need to, we'll do this in "mapload"
    //W_CloseAuxiliary();

    #ifdef CLIENT
    if (GGameInfo->NetMode == NM_DedicatedServer) {
      SV_ShutdownGame();
      Sys_Error("Host_Error: %s\n", e.message); // dedicated servers exit
    }

    SV_ShutdownGame();
    GClGame->eventOnHostError();
    C_StartFull();
    #else
    SV_ShutdownGame();
    Sys_Error("Host_Error: %s\n", e.message); // dedicated servers exit
    #endif
  } catch (EndGame &e) {
    GCon->Logf(NAME_Dev, "Host_EndGame: %s", e.message);

    // reset progs virtual machine
    VObject::PR_OnAbort();
    // make sure, that we use primary wad files
    //k8: no need to, we'll do this in "mapload"
    //W_CloseAuxiliary();

    #ifdef CLIENT
    if (GGameInfo->NetMode == NM_DedicatedServer) {
      SV_ShutdownGame();
      Sys_Error("Host_EndGame: %s\n", e.message); // dedicated servers exit
    }

    SV_ShutdownGame();
    GClGame->eventOnHostEndGame();
    #else
    SV_ShutdownGame();
    Sys_Error("Host_EndGame: %s\n", e.message); // dedicated servers exit
    #endif
  }
}


//==========================================================================
//
//  Host_EndGame
//
//==========================================================================
void Host_EndGame (const char *message, ...) {
  va_list argptr;
  static char string[4096];

  va_start(argptr, message);
  vsnprintf(string, sizeof(string), message, argptr);
  va_end(argptr);

  throw EndGame(string);
}


//==========================================================================
//
//  Host_Error
//
//  This shuts down both the client and server
//
//==========================================================================
void Host_Error (const char *error, ...) {
  va_list argptr;
  static char string[4096];

  va_start(argptr, error);
  vsnprintf(string, sizeof(string), error, argptr);
  va_end(argptr);

  throw RecoverableError(string);
}


//==========================================================================
//
//  Version_f
//
//==========================================================================
COMMAND(Version) {
  GCon->Log("k8vavoom version " VERSION_TEXT ".");
  GCon->Log("Compiled " __DATE__ " " __TIME__ ".");
}


//==========================================================================
//
//  Host_GetConfigDir
//
//==========================================================================
VStr Host_GetConfigDir () {
  return FL_GetConfigDir();
}


#ifdef CLIENT
//==========================================================================
//
//  Host_SaveConfiguration
//
//  Saves game variables
//
//==========================================================================
void Host_SaveConfiguration () {
  if (!host_initialised) return;

  if (!cfg_saving_allowed) {
    GCon->Log("config saving disabled by the user.");
    return;
  }

  VStr cff = Host_GetConfigDir().appendPath("config.cfg");

  // create new config in memory first
  VMemoryStream cst(cff);
  cst.BeginWrite();
  cst.writef("// generated by k8vavoom\n");
  cst.writef("//\n// bindings\n//\n");
  GInput->WriteBindings(&cst);
  cst.writef("//\n// aliases\n//\n");
  VCommand::WriteAlias(&cst);
  cst.writef("\n//\n// variables\n//\n");
  VCvar::WriteVariablesToStream(&cst);

  // compare with existing config (if there is any)
  VStream *oldcst = CreateDiskStreamRead(cff);
  if (oldcst) {
    if (oldcst->TotalSize() == cst.TotalSize()) {
      TArrayNC<vuint8> olddata;
      olddata.setLength(cst.TotalSize());
      oldcst->Serialise(olddata.ptr(), olddata.length());
      if (memcmp(olddata.ptr(), cst.GetArray().ptr(), olddata.length()) == 0) {
        GCon->Logf("config at \"%s\" wasn't changed.", *cff);
        delete oldcst;
        return;
      }
    }
    delete oldcst;
  }

  FL_CreatePath(cff.extractFilePath());

  VStream *st = CreateDiskStreamWrite(cff);
  if (!st) {
    GCon->Logf(NAME_Error, "Failed to create config file \"%s\"", *cff);
    return; // can't write the file, but don't fail
  }
  st->Serialise(cst.GetArray().ptr(), cst.GetArray().length());
  if (st->Close()) {
    GCon->Logf("config written to \"%s\".", *cff);
  } else {
    GCon->Logf(NAME_Error, "cannot write config to \"%s\"!", *cff);
  }

  delete st;
}

COMMAND(SaveConfig) {
  if (!host_initialised) return;
  Host_SaveConfiguration();
}
#endif


//==========================================================================
//
//  Host_Quit
//
//==========================================================================
#ifndef _WIN32
# include <stdio.h>
# include <unistd.h>
#endif
void Host_Quit () {
  SV_ShutdownGame();
#ifdef CLIENT
  // save game configyration
  Host_SaveConfiguration();
#endif

  DD_ShutdownLog();

  // get the lump with the end text
  // if option -noendtxt is set, don't print the text
  bool GotEndText = false;
  char EndText[80*25*2];
  if (cli_ShowEndText > 0) {
    // find end text lump
    VStream *Strm = nullptr;
         if (W_CheckNumForName(NAME_endoom) >= 0) Strm = W_CreateLumpReaderName(NAME_endoom);
    else if (W_CheckNumForName(NAME_endtext) >= 0) Strm = W_CreateLumpReaderName(NAME_endtext);
    else if (W_CheckNumForName(NAME_endstrf) >= 0) Strm = W_CreateLumpReaderName(NAME_endstrf);
    // read it, if found
    if (Strm) {
      int Len = 80*25*2;
      if (Strm->TotalSize() < Len) {
        memset(EndText, 0, Len);
        Len = Strm->TotalSize();
      }
      Strm->Serialise(EndText, Len);
      VStream::Destroy(Strm);
      GotEndText = true;
    }
#ifndef _WIN32
    if (!isatty(STDOUT_FILENO)) GotEndText = false;
#endif
  }

  Sys_Quit(GotEndText ? EndText : nullptr);
}


//==========================================================================
//
//  Quit
//
//==========================================================================
COMMAND(Quit) {
  host_request_exit = true;
}


//==========================================================================
//
//  Host_Shutdown
//
//  Return to default system state
//
//==========================================================================
void Host_Shutdown () {
  static bool shutting_down = false;

  if (shutting_down) {
    GLog.Log("Recursive shutdown");
    return;
  }
  shutting_down = true;

  VMemberBase::DumpNameMaps();
  GTextureManager.DumpHashStats(NAME_Log);
  VCvar::DumpHashStats();

#define SAFE_SHUTDOWN(name, args) \
  try { /*GLog.Log("Doing "#name);*/ name args; } catch (...) { GLog.Log(#name" failed"); }

#ifdef CLIENT
  //k8:no need to do this:SAFE_SHUTDOWN(C_Shutdown, ()) // console
  if (developer) GLog.Log(NAME_Dev, "shutting down client");
  SAFE_SHUTDOWN(CL_Shutdown, ())
#endif
#ifdef SERVER
  if (developer) GLog.Log(NAME_Dev, "shutting down server");
  SAFE_SHUTDOWN(SV_Shutdown, ())
#endif
  if (GNet) {
    if (developer) GLog.Log(NAME_Dev, "shutting down network");
    SAFE_SHUTDOWN(delete GNet,)
    GNet = nullptr;
  }
#ifdef CLIENT
  if (GInput) {
    if (developer) GLog.Log(NAME_Dev, "shutting down input");
    SAFE_SHUTDOWN(delete GInput,)
    GInput = nullptr;
  }

  if (developer) GLog.Log(NAME_Dev, "shutting down video");
  SAFE_SHUTDOWN(V_Shutdown, ()) // video

  if (GAudio) {
    if (developer) GLog.Log(NAME_Dev, "shutting down audio");
    GAudio->Shutdown();
    SAFE_SHUTDOWN(delete GAudio,)
    GAudio = nullptr;
    if (developer) GLog.Log(NAME_Dev, "audio deleted");
  }
  //k8:no need to do this:SAFE_SHUTDOWN(T_Shutdown, ()) // font system
#endif
  //k8:no need to do this:SAFE_SHUTDOWN(Sys_Shutdown, ()) // nothing at all

  if (GSoundManager) {
    if (developer) GLog.Log(NAME_Dev, "shutting down sound manager");
    SAFE_SHUTDOWN(delete GSoundManager,)
    GSoundManager = nullptr;
  }

  if (cli_DumpAllVars > 0) VCvar::DumpAllVars();
  //k8:no need to do this:SAFE_SHUTDOWN(R_ShutdownTexture, ()) // texture manager
  //k8:no need to do this:SAFE_SHUTDOWN(R_ShutdownData, ()) // various game tables
  //k8:no need to do this:SAFE_SHUTDOWN(VCommand::Shutdown, ())
  //k8:no need to do this:SAFE_SHUTDOWN(VCvar::Shutdown, ())
  //k8:no need to do this:SAFE_SHUTDOWN(ShutdownMapInfo, ()) // mapinfo
  //k8:no need to do this:SAFE_SHUTDOWN(FL_Shutdown, ()) // filesystem
  //k8:no need to do this:SAFE_SHUTDOWN(W_Shutdown, ()) // wads
  //k8:no need to do this:SAFE_SHUTDOWN(GLanguage.FreeData, ())
  //k8:no need to do this:SAFE_SHUTDOWN(ShutdownDecorate, ())

  if (developer) GLog.Log(NAME_Dev, "shutting down VObject");
  SAFE_SHUTDOWN(VObject::StaticExit, ())
  //k8:no need to do this:SAFE_SHUTDOWN(VName::StaticExit, ())
  //SAFE_SHUTDOWN(Z_Shutdown, ())
  //GLog.Log("k8vavoom: shutdown complete");

#ifdef CLIENT
  if (developer) GLog.Log(NAME_Dev, "shutting down console");
  C_Shutdown(); // save log
#endif

  DD_ShutdownLog();

  // prevent shitdoze crashes
  if (developer) GLog.Log(NAME_Dev, "shutting down memory manager");
  Z_ShuttingDown();
}
