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
#include "../gamedefs.h"
#include "../psim/p_entity.h"
#include "../psim/p_levelinfo.h"
#include "../psim/p_player.h"
#ifdef CLIENT
# include "../client/client.h"
# include "../client/cl_local.h"
#endif
#include "../psim/p_decal.h"
#include "../decorate/vc_decorate.h"


extern VCvarB r_decals;
extern VCvarB k8gore_enabled;
extern VCvarI k8gore_enabled_override;

VCvarB dbg_world_think_vm_time("dbg_world_think_vm_time", false, "Show time taken by VM thinkers (for debug)?", CVAR_Archive|CVAR_NoShadow);
VCvarB dbg_world_think_decal_time("dbg_world_think_decal_time", false, "Show time taken by decal thinkers (for debug)?", CVAR_Archive|CVAR_NoShadow);
VCvarB dbg_vm_disable_thinkers("dbg_vm_disable_thinkers", false, "Disable VM thinkers (for debug)?", CVAR_PreInit|CVAR_NoShadow);
VCvarB dbg_vm_enable_secthink("dbg_vm_enable_secthink", true, "Enable sector thinkers when VM thinkers are disabled (for debug)?", CVAR_PreInit|CVAR_NoShadow);
VCvarB dbg_vm_disable_specials("dbg_vm_disable_specials", false, "Disable updating specials (for debug)?", CVAR_PreInit|CVAR_NoShadow);
VCvarB dbg_vm_show_tick_stats("dbg_vm_show_tick_stats", false, "Show some debug tick statistics?", CVAR_PreInit|CVAR_NoShadow);

static VCvarB dbg_limiter_counters("dbg_limiter_counters", false, "Show limiter counters?", CVAR_PreInit|CVAR_NoShadow);
static VCvarB dbg_limiter_remove_messages("dbg_limiter_remove_messages", false, "Show limiter remove messages?", CVAR_PreInit|CVAR_NoShadow);

static VCvarI gm_corpse_limit("gm_corpse_limit", "-1", "Limit number of corpses per map (-1: no limit)?", CVAR_Archive);

double worldThinkTimeVM = -1.0;
double worldThinkTimeDecal = -1.0;

static TArray<VEntity *> corpseQueue;

int dbgEntityTickTotal = 0;
int dbgEntityTickSimple = 0;
int dbgEntityTickNoTick = 0;

static VClass *clsBlood = nullptr;
static VClass *clsBloodSplatter = nullptr;
static VClass *clsBloodAxe = nullptr;

static VClass *clsGoreBlood = nullptr;
static VClass *clsGoreBloodSplatter = nullptr;
static VClass *clsGoreBloodAxe = nullptr;

bool VLevel::canReplaceBlood = false;
VName VLevel::goreBloodDecalSplat = NAME_None;
VName VLevel::goreBloodDecalSmear = NAME_None;
VName VLevel::goreBloodDecalSplatRadius = NAME_None;
VName VLevel::goreBloodDecalSmearRadius = NAME_None;


//==========================================================================
//
//  isGoreEnabled
//
//==========================================================================
static VVA_ALWAYS_INLINE bool isGoreEnabled () {
  const int ov = k8gore_enabled_override.asInt();
  if (ov < 0) return false;
  if (ov > 0) return true;
  return k8gore_enabled.asBool();
}


//==========================================================================
//
//  VLevel::LevelStaticInit
//
//==========================================================================
void VLevel::LevelStaticInit () {
  clsBlood = VClass::FindClass("Blood");
  clsBloodSplatter = VClass::FindClass("BloodSplatter");
  clsBloodAxe = VClass::FindClass("AxeBlood");

  clsGoreBlood = VClass::FindClass("K8Gore_Blood");
  clsGoreBloodSplatter = VClass::FindClass("K8Gore_BloodSplatter");
  if (!clsGoreBloodSplatter) clsGoreBloodSplatter = clsGoreBlood;
  clsGoreBloodAxe = VClass::FindClass("K8Gore_AxeBlood");
  if (!clsGoreBloodAxe) clsGoreBloodAxe = clsGoreBlood;

  if (clsBlood && !clsBlood->IsChildOf(VEntity::StaticClass())) {
    GCon->Logf(NAME_Error, "`Blood` class is not an entity!");
    clsBlood = nullptr;
  }

  if (clsBloodSplatter && !clsBloodSplatter->IsChildOf(VEntity::StaticClass())) {
    GCon->Logf(NAME_Error, "`BloodSplatter` class is not an entity!");
    clsBloodSplatter = nullptr;
  }

  if (clsBloodAxe && !clsBloodAxe->IsChildOf(VEntity::StaticClass())) {
    GCon->Logf(NAME_Error, "`AxeBlood` class is not an entity!");
    clsBloodSplatter = nullptr;
  }

  if (clsGoreBlood && !clsGoreBlood->IsChildOf(VEntity::StaticClass())) {
    GCon->Logf(NAME_Error, "Gore `Blood` class is not an entity!");
    clsGoreBlood = nullptr;
  }

  if (clsGoreBloodSplatter && !clsGoreBloodSplatter->IsChildOf(VEntity::StaticClass())) {
    GCon->Logf(NAME_Error, "Gore `BloodSplatter` class is not an entity!");
    clsGoreBloodSplatter = nullptr;
  }

  if (clsGoreBloodAxe && !clsGoreBloodAxe->IsChildOf(VEntity::StaticClass())) {
    GCon->Logf(NAME_Error, "Gore `AxeBlood` class is not an entity!");
    clsGoreBloodSplatter = nullptr;
  }

  canReplaceBlood =
    (clsBlood || clsBloodSplatter || clsBloodAxe) &&
    (clsGoreBlood || clsGoreBloodSplatter || clsGoreBloodAxe);

  if (canReplaceBlood) {
    goreBloodDecalSplat = VName("K8GDC_BloodSplat");
    goreBloodDecalSmear = VName("K8GDC_BloodSmear");
    goreBloodDecalSplatRadius = VName("K8GDC_BloodSplatRadius");
    goreBloodDecalSmearRadius = VName("K8GDC_BloodSmearRadius");
  }
}


//==========================================================================
//
//  VLevel::AddScriptThinker
//
//  won't call `Destroy()`, won't call `delete`
//
//==========================================================================
void VLevel::RemoveScriptThinker (VLevelScriptThinker *sth) {
  if (!sth) return;
  const int sclenOrig = scriptThinkers.length();
  for (int scidx = sclenOrig-1; scidx >= 0; --scidx) {
    if (scriptThinkers[scidx] == sth) {
      // remove it
      for (int c = scidx+1; c < sclenOrig; ++c) scriptThinkers[c-1] = scriptThinkers[c];
      scriptThinkers.setLength(scidx, false); // don't resize
      return;
    }
  }
}


//==========================================================================
//
//  VLevel::PromoteImmediateScriptThinker
//
//  WARNING! does no checks!
//
//==========================================================================
void VLevel::PromoteImmediateScriptThinker (VLevelScriptThinker *sth) {
  if (!sth || sth->destroyed) return;
  vassert(sth->XLevel == this);
  vassert(sth->Level == LevelInfo);
  scriptThinkers.append(sth);
  if (scriptThinkers.length() > 16384) Host_Error("too many ACS thinkers spawned");
}


//==========================================================================
//
//  VLevel::AddScriptThinker
//
//==========================================================================
void VLevel::AddScriptThinker (VLevelScriptThinker *sth, bool ImmediateRun) {
  if (!sth) return;
  vassert(!sth->XLevel);
  vassert(!sth->Level);
  sth->XLevel = this;
  sth->Level = LevelInfo;
  if (ImmediateRun) return;
  // cleanup is done in `RunScriptThinkers()`
  scriptThinkers.append(sth);
  //GCon->Logf("*** ADDED ACS: %s (%d)", *sth->DebugDumpToString(), scriptThinkers.length());
  if (scriptThinkers.length() > 16384) Host_Error("too many ACS thinkers spawned");
}


//==========================================================================
//
//  VLevel::SuspendNamedScriptThinkers
//
//==========================================================================
void VLevel::SuspendNamedScriptThinkers (VStr name, int map) {
  if (name.length() == 0) return;
  const int sclenOrig = scriptThinkers.length();
  VLevelScriptThinker **sth = scriptThinkers.ptr();
  for (int count = sclenOrig; count--; ++sth) {
    if (!sth[0] || sth[0]->destroyed) continue;
    if (name.ICmp(*sth[0]->GetName()) == 0) {
      //Acs->Suspend(sth[0]->GetNumber(), map);
      AcsSuspendScript(Acs, sth[0]->GetNumber(), map);
    }
  }
}


//==========================================================================
//
//  VLevel::TerminateNamedScriptThinkers
//
//==========================================================================
void VLevel::TerminateNamedScriptThinkers (VStr name, int map) {
  if (name.length() == 0) return;
  const int sclenOrig = scriptThinkers.length();
  VLevelScriptThinker **sth = scriptThinkers.ptr();
  for (int count = sclenOrig; count--; ++sth) {
    if (!sth[0] || sth[0]->destroyed) continue;
    if (name.ICmp(*sth[0]->GetName()) == 0) {
      //Acs->Terminate(sth[0]->GetNumber(), map);
      AcsTerminateScript(Acs, sth[0]->GetNumber(), map);
    }
  }
}


//==========================================================================
//
//  VLevel::AddThinker
//
//==========================================================================
void VLevel::AddThinker (VThinker *Th) {
  Th->XLevel = this;
  Th->Level = LevelInfo;
  Th->Prev = ThinkerTail;
  Th->Next = nullptr;
  if (ThinkerTail) ThinkerTail->Next = Th; else ThinkerHead = Th;
  ThinkerTail = Th;
  // register suid
  if (Th->ServerUId && Th->IsA(VEntity::StaticClass())) suid2ent->put(Th->ServerUId, (VEntity *)Th);
  // notify thinker that is was just added to a level
  Th->AddedToLevel();
}


//==========================================================================
//
//  VLevel::RemoveThinker
//
//==========================================================================
void VLevel::RemoveThinker (VThinker *Th) {
  if (Th) {
    // notify that thinker is being removed from level
    Th->RemovedFromLevel();
    // unregister suid
    if (Th->ServerUId) suid2ent->del(Th->ServerUId);
    // remove from thinker list
    if (Th == ThinkerHead) ThinkerHead = Th->Next; else Th->Prev->Next = Th->Next;
    if (Th == ThinkerTail) ThinkerTail = Th->Prev; else Th->Next->Prev = Th->Prev;
  }
}


//==========================================================================
//
//  VLevel::DestroyAllThinkers
//
//==========================================================================
void VLevel::DestroyAllThinkers () {
  // destroy scripts
  for (int scidx = scriptThinkers.length()-1; scidx >= 0; --scidx) if (scriptThinkers[scidx]) scriptThinkers[scidx]->Destroy();
  for (int scidx = scriptThinkers.length()-1; scidx >= 0; --scidx) delete scriptThinkers[scidx];
  scriptThinkers.clear();

  // clear suid map
  suid2ent->clear();

  // destroy VC thinkers
  VThinker *Th = ThinkerHead;
  while (Th) {
    VThinker *c = Th;
    Th = c->Next;
    if (!c->IsGoingToDie()) c->DestroyThinker();
  }
  Th = ThinkerHead;
  while (Th) {
    VThinker *Next = Th->Next;
    Th->ConditionalDestroy();
    Th = Next;
  }
  ThinkerHead = nullptr;
  ThinkerTail = nullptr;
}


//==========================================================================
//
//  VLevel::UpdateThinkersLevelInfo
//
//==========================================================================
void VLevel::UpdateThinkersLevelInfo () {
  if (!ThinkerHead) return;
  int count = 0;
  for (VThinker *th = ThinkerHead; th; th = th->Next) {
    if (!th->Level) {
      ++count;
      th->Level = LevelInfo;
    } else {
      vassert(th->Level == LevelInfo);
    }
  }
  GCon->Logf(NAME_Debug, "VLevel::UpdateThinkersLevelInfo: %d thinkers updated", count);
  #ifdef CLIENT
  //vassert(!LevelInfo->Game || LevelInfo->Game == GClGame->Game);
  if (LevelInfo->Game) GCon->Logf(NAME_Debug, "*** LevelInfo->Game: %u", LevelInfo->Game->GetUniqueId());
  GCon->Logf(NAME_Debug, "*** GGameInfo: %u", GGameInfo->GetUniqueId());
  vassert(!LevelInfo->Game || LevelInfo->Game == GGameInfo);
  //GLevelInfo->Game = GGameInfo;
  //GLevelInfo->World = GGameInfo->WorldInfo;
  LevelInfo->Game = GClGame->Game;
  if (!LevelInfo->World) {
    GCon->Log(NAME_Debug, "*** setting World");
    LevelInfo->World = GGameInfo->WorldInfo;
  }
  #endif
}


//==========================================================================
//
//  VLevel::RunScriptThinkers
//
//==========================================================================
void VLevel::RunScriptThinkers (float DeltaTime) {
  if (DeltaTime <= 0.0f) return;
  // run script thinkers
  // do not run newly spawned scripts on this frame, though
  //const int sclenOrig = scriptThinkers.length();
  int firstEmpty = -1;
  // don't cache number of thinkers, as scripts may add new thinkers
  for (int scidx = 0; scidx < scriptThinkers.length()/*sclenOrig*/; ++scidx) {
    VLevelScriptThinker *sth = scriptThinkers[scidx];
    if (!sth) {
      if (firstEmpty < 0) firstEmpty = scidx;
      continue;
    }
    if (sth->destroyed) {
      //GCon->Logf("(0)DEAD ACS at slot #%d", scidx);
      if (firstEmpty < 0) firstEmpty = scidx;
      delete sth;
      scriptThinkers[scidx] = nullptr;
      continue;
    }
    //GCon->Logf("ACS #%d RUNNING: %s", scidx, *sth->DebugDumpToString());
    sth->Tick(DeltaTime);
    if (sth->destroyed) {
      //GCon->Logf("(1)DEAD ACS at slot #%d", scidx);
      if (firstEmpty < 0) firstEmpty = scidx;
      delete sth;
      scriptThinkers[scidx] = nullptr;
      continue;
    }
  }
  // remove dead thinkers
  if (firstEmpty >= 0) {
    const int sclen = scriptThinkers.length();
    int currIdx = firstEmpty+1;
    while (currIdx < sclen) {
      VLevelScriptThinker *sth = scriptThinkers[currIdx];
      if (sth) {
        // alive
        vassert(firstEmpty < currIdx);
        scriptThinkers[firstEmpty] = sth;
        scriptThinkers[currIdx] = nullptr;
        // find next empty slot
        ++firstEmpty;
        while (firstEmpty < sclen && scriptThinkers[firstEmpty]) ++firstEmpty;
      } else {
        // dead, do nothing
      }
      ++currIdx;
    }
    //GCon->Logf("  SHRINKING ACS from %d to %d", sclen, firstEmpty);
    scriptThinkers.setLength(firstEmpty, false); // don't resize
  }
}


//==========================================================================
//
//  VLevel::TickDecals
//
//  this should be called in `CL_UpdateMobjs()`
//
//==========================================================================
void VLevel::TickDecals (float DeltaTime) {
  if (!r_decals || !decanimlist) { worldThinkTimeDecal = -1; return; }
  if (DeltaTime <= 0.0f) return;
  double stimed = (dbg_world_think_decal_time ? -Sys_Time() : 0.0);
  // run decal thinkers
  decal_t *dc = decanimlist;
  while (dc) {
    bool removeIt = true;
    if (dc->animator) removeIt = !dc->animator->animate(dc, DeltaTime);
    decal_t *c = dc;
    dc = dc->nextanimated;
    if (removeIt) RemoveDecalAnimator(c);
  }
  worldThinkTimeDecal = (dbg_world_think_decal_time ? Sys_Time()+stimed : -1);
}


//==========================================================================
//
//  cmpLimInstance
//
//==========================================================================
static VVA_OKUNUSED int cmpLimInstance (const void *aa, const void *bb, void *) {
  if (aa == bb) return 0;
  const VThinker *a = (const VThinker *)aa;
  const VThinker *b = (const VThinker *)bb;
  const float sts = a->SpawnTime-b->SpawnTime;
  return (sts < 0 ? -1 : sts > 0 ? 1 : 0);
}


//==========================================================================
//
//  VLevel::TickWorld
//
//==========================================================================
void VLevel::TickWorld (float DeltaTime) {
  if (DeltaTime <= 0.0f) return;

  CheckAndRecalcWorldBBoxes();
  //if (pathInterceptsUsed) GCon->Logf(NAME_Debug, "unbalanced path iterators; used=%d", pathInterceptsUsed);
  ResetAllPathIntercepts();

  double stimet = 0.0;

  NextTime = Time+DeltaTime;

  eventUpdateCachedCVars();
  eventBeforeWorldTick(DeltaTime);

  dbgEntityTickTotal = dbgEntityTickSimple = dbgEntityTickNoTick = 0;

  if (dbg_world_think_vm_time) stimet = -Sys_Time();

  // setup limiter info
  // `NumberLimitedClasses` contains only base limiter classes
  for (VClass *cls : NumberLimitedClasses) {
    if (!cls->GetLimitInstancesWithSub()) {
      cls->InstanceLimitWithSub = 0;
      continue;
    }
    VCvar *cv = cls->InstanceLimitWithSubCvarPtr;
    if (!cv) {
      if (!cls->InstanceLimitWithSubCvar.isEmpty()) cv = VCvar::FindVariable(*cls->InstanceLimitWithSubCvar);
      cls->InstanceLimitWithSubCvarPtr = cv;
      if (!cv) {
        cls->InstanceLimitWithSubCvar.clear();
        cls->SetLimitInstancesWithSub(false);
        cls->InstanceLimitWithSub = 0;
        cls->InstanceLimitList.clear(); // it will never contain anything
        continue;
      }
    }
    cls->InstanceLimitWithSub = max2(0, cv->asInt());
    cls->InstanceLimitList.reset();
  }

  if (dbg_vm_show_tick_stats.asBool()) GCon->Log(NAME_Debug, "=== WORLD TICK ===");

  const int corpseLimit = gm_corpse_limit.asInt();
  corpseQueue.reset();

  // run thinkers
  #ifdef CLIENT
  // first run player mobile thinker, so we'll get camera position
  VThinker *plrmo = (cl ? cl->MO : nullptr);
  if (plrmo) {
    if (plrmo->IsGoingToDie()) {
      plrmo = nullptr;
    } else {
      plrmo->Tick(DeltaTime);
    }
  }
  #else
  // server: run player thinkers when vm thinkers are disabled
  if (dbg_vm_disable_thinkers) {
    for (int i = 0; i < MAXPLAYERS; ++i) {
      VBasePlayer *Player = GGameInfo->Players[i];
      if (!Player) continue;
      if (!(Player->PlayerFlags&VBasePlayer::PF_Spawned)) continue;
      if (Player->PlayerFlags&VBasePlayer::PF_IsBot) continue;
      VThinker *plrmo = Player->MO;
      if (plrmo && !plrmo->IsGoingToDie()) {
        //GCon->Logf("TICK: '%s'", *Player->PlayerName);
        plrmo->Tick(DeltaTime);
      }
    }
  }
  #endif

  bool shouldProcessLimiters = false;
  //GCon->Log(NAME_Debug, "========================");
  VThinker *Th = ThinkerHead;
  if (!dbg_vm_disable_thinkers) {
    while (Th) {
      VThinker *c = Th;
      Th = c->Next;
      #ifdef CLIENT
      if (c != plrmo)
      #endif
      {
        if (!c->IsGoingToDie()) c->Tick(DeltaTime);
      }
      if (c->IsGoingToDie()) {
        //GCon->Logf(NAME_Debug, "  DYING THINKER %u: %s", c->GetUniqueId(), c->GetClass()->GetName());
        // object is already dead, or dying
        if (c->IsDelayedDestroy()) RemoveThinker(c);
        // if it is just destroyed, call level notifier
        if (!c->IsDestroyed() && c->IsA(VEntity::StaticClass())) {
          //HACK! do not call it for `VLevel`, as it is empty anyway
          if (GetClass() != VLevel::StaticClass()) eventEntityDying((VEntity *)c);
        }
        c->ConditionalDestroy();
      } else {
        // collect instances for limiters
        VClass *lcls = c->GetClass();
        if (lcls->GetLimitInstancesWithSub()) {
          lcls = (lcls->InstanceLimitBaseClass ?: lcls);
          vassert(lcls);
          if (lcls->InstanceLimitWithSub > 0 && lcls->InstanceCountWithSub > lcls->InstanceLimitWithSub) {
            lcls->InstanceLimitList.append(c);
            shouldProcessLimiters = true;
            //GCon->Logf(NAME_Debug, ":ADDING:%s: count=%d; limit=%d (lcls=%s)", c->GetClass()->GetName(), lcls->InstanceCountWithSub, lcls->InstanceLimitWithSub, lcls->GetName());
          }
        }
        // collect corpses
        if (corpseLimit >= 0) {
          if (c->IsA(VEntity::StaticClass())) {
            VEntity *e = (VEntity *)c;
            // check if it is really dead, and not moving
            if (e->IsRealCorpse() && e->StateTime < 0 && fabsf(e->Velocity.x) < 0.01f && fabsf(e->Velocity.y) < 0.01f) {
              // if the corpse is on a floor, it is safe to remove it
              if (e->Origin.z <= e->FloorZ) {
                // notick corpses are fading out
                if ((e->FlagsEx&(VEntity::EFEX_NoTickGrav|VEntity::EFEX_NoTickGravLT)) != (VEntity::EFEX_NoTickGrav|VEntity::EFEX_NoTickGravLT)) {
                  corpseQueue.append(e);
                }
              }
            }
          }
        }
      }
    }
  } else {
    // thinkers are disabled
    if (dbg_vm_enable_secthink) {
      // but sector thinkers are enabled
      static VClass *SSClass = nullptr;
      if (!SSClass) {
        SSClass = VClass::FindClass("SectorThinker");
        if (!SSClass) Sys_Error("VM class 'SectorThinker' not found!");
      }
      while (Th) {
        VThinker *c = Th;
        Th = c->Next;
        if (c->IsGoingToDie()) {
          if (c->IsDelayedDestroy()) RemoveThinker(c);
          // if it is just destroyed, call level notifier
          if (!c->IsDestroyed() && c->IsA(VEntity::StaticClass())) {
            //HACK! do not call it for `VLevel`, as it is empty anyway
            if (GetClass() != VLevel::StaticClass()) eventEntityDying((VEntity *)c);
          }
          c->ConditionalDestroy();
        } else if (c->IsA(SSClass)) {
          c->Tick(DeltaTime);
        }
      }
    }
  }

  worldThinkTimeVM = (dbg_world_think_vm_time ? Sys_Time()+stimet : -1);

  RunScriptThinkers(DeltaTime);


  if (!dbg_vm_disable_specials) {
    // don't update specials if the level is frozen
    if (!(LevelInfo->LevelInfoFlags2&VLevelInfo::LIF2_Frozen)) LevelInfo->eventUpdateSpecials();
  }

  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (LevelInfo->Game->Players[i] && (LevelInfo->Game->Players[i]->PlayerFlags&VBasePlayer::PF_Spawned) != 0) {
      LevelInfo->Game->Players[i]->eventSetViewPos();
    }
  }

  // process limiters
  //FIXME: make this faster: we know exactly what classes to process
  if (shouldProcessLimiters) {
    const bool limdbg = dbg_limiter_counters.asBool();
    const bool limdbgmsg = dbg_limiter_remove_messages.asBool();
    for (VClass *cls : NumberLimitedClasses) {
      if (cls->InstanceLimitWithSub < 1) continue;
      // remove ~1/3 of instances
      int maxCount = cls->InstanceLimitWithSub-(cls->InstanceLimitWithSub/3);
      if (maxCount >= cls->InstanceLimitWithSub) maxCount -= 10;
      if (maxCount < 1) maxCount = 1;
      const int instTotal = cls->InstanceLimitList.length();
      if (instTotal <= maxCount) continue; // just in case
      // limit hit, remove furthest
      if (limdbg) GCon->Logf(NAME_Debug, "%s: limit is %d, current is %d, reducing to %d", cls->GetName(), cls->InstanceLimitWithSub, cls->InstanceLimitList.length(), maxCount);
      //FIXME: remove by proximity to the camera?
      // sort by spawn time
      // note that it is guaranteed that all objects are `VEntity` instances here (see decorate parsing code)
      const int tokill = instTotal-maxCount;
      vassert(tokill > 0);
      #if 0
      smsort_r(cls->InstanceLimitList.ptr(), instTotal, sizeof(VObject *), &cmpLimInstance, nullptr);
      #else
      // shuffle first `tokill` elements, it is much faster than sorting
      // this will effectively remove some random instances; it may not be as nice-looking as sorting by spawn time, but meh...
      for (int f = 0; f < tokill; ++f) {
        int swapidx = (int)(GenRandomU31()%(unsigned)instTotal);
        if (swapidx == f) continue;
        VObject *tmp = cls->InstanceLimitList.ptr()[f];
        cls->InstanceLimitList.ptr()[f] = cls->InstanceLimitList.ptr()[swapidx];
        cls->InstanceLimitList.ptr()[swapidx] = tmp;
      }
      #endif
      for (int f = 0; f < tokill; ++f) {
        VThinker *c = (VThinker *)cls->InstanceLimitList.ptr()[f]; // no need to perform range checking
        if (c->IsDestroyed()) {
          if (limdbgmsg) GCon->Logf(NAME_Debug, "  %s(%u): destroyed", c->GetClass()->GetName(), c->GetUniqueId());
          continue;
        }
        if (!c->IsDelayedDestroy()) {
          if (limdbgmsg) GCon->Logf(NAME_Debug, "  %s(%u): removing", c->GetClass()->GetName(), c->GetUniqueId());
          RemoveThinker(c);
          //WARNING! death notifier will not be called for this entity!
          c->ConditionalDestroy();
        }
      }
    }
  }

  // process corpse limit
  if (corpseLimit >= 0 && corpseQueue.length() > corpseLimit) {
    const int end = corpseQueue.length();
    GCon->Logf(NAME_Debug, "removing %d corpses out of %d", end-corpseLimit, end);
    // shuffle corpse queue, so we'll remove random corpses
    for (int f = 0; f < end; ++f) {
      // swap current and another random corpse
      int n = (int)(GenRandomU31()%(unsigned)end);
      VEntity *e0 = corpseQueue.ptr()[f];
      VEntity *e1 = corpseQueue.ptr()[n];
      corpseQueue.ptr()[f] = e1;
      corpseQueue.ptr()[n] = e0;
    }
    for (int f = corpseLimit; f < end; ++f) {
      VEntity *e = corpseQueue.ptr()[f];
      if (e->IsDestroyed()) continue;
      if (!e->IsDelayedDestroy()) {
        //RemoveThinker(e);
        //WARNING! death notifier will not be called for this entity!
        //e->ConditionalDestroy();
        // make the corpse notick, and fade it away
        e->FlagsEx |= VEntity::EFEX_NoTickGrav|VEntity::EFEX_NoTickGravLT;
        e->LastMoveTime = 0; // fade immediately
        e->PlaneAlpha = 0.04f; // fade step time
      }
    }
  }

  //GCon->Logf("VLevel::TickWorld: time=%f; tictime=%f; dt=%f : %f", (double)Time, (double)TicTime, DeltaTime, DeltaTime*1000.0);
  Time += DeltaTime;
  //++TicTime;
  TicTime = (int)(Time*35.0f);

  eventAfterWorldTick(DeltaTime);

  if (dbg_vm_show_tick_stats.asBool()) {
    GCon->Logf(NAME_Debug, "TICK: total=%d; simple=%d; notick=%d (full left: %d)", dbgEntityTickTotal, dbgEntityTickSimple, dbgEntityTickNoTick, dbgEntityTickTotal-(dbgEntityTickSimple+dbgEntityTickNoTick));
  }
}


//==========================================================================
//
//  VLevel::SpawnThinker
//
//==========================================================================
VThinker *VLevel::SpawnThinker (VClass *AClass, const TVec &AOrigin,
                                const TAVec &AAngles, mthing_t *mthing, bool AllowReplace,
                                vuint32 srvUId)
{
  vassert(AClass);
  VClass *Class = (AllowReplace ? AClass->GetReplacement() : AClass);
  //if (!Class) Class = AClass;

  if (!Class) {
    VObject::VMDumpCallStack();
    GCon->Log(NAME_Error, "cannot spawn object without a class (expect engine crash soon!)");
    return nullptr;
  }

  if (BlockedSpawnSet.has(Class->Name)) {
    GCon->Logf(NAME_Debug, "blocked spawning of `%s`", Class->GetName());
    return nullptr;
  }

  // check for forced replacement
  auto repclspp = ForceReplacements.get(Class->Name);
  if (repclspp) {
    VClass *ocls = Class;
    Class = *repclspp;
    Class = Class->GetReplacement();
    if (Class != ocls) GCon->Logf(NAME_Debug, "force-replaced spawning of `%s` with `%s`", ocls->GetName(), Class->GetName());
  }

  // check for gore blood replacements
  if (canReplaceBlood && isGoreEnabled()) {
    VClass *brepl = nullptr;
    #if 0
    for (const VClass *c = AClass; c; c = c->ParentClass) {
      if (c == clsBlood) { brepl = clsGoreBlood; break; }
      if (c == clsBloodSplatter) { brepl = clsGoreBloodSplatter; break; }
      if (c == clsGoreBloodAxe) { brepl = clsGoreBloodAxe; break; }
    }
    #else
           if (AClass == clsBlood) brepl = clsGoreBlood;
      else if (AClass == clsBloodSplatter) brepl = clsGoreBloodSplatter;
      else if (AClass == clsGoreBloodAxe) brepl = clsGoreBloodAxe;
    #endif
    if (brepl) {
      Class = brepl; // it is guaranteed to be a VEntity
      if (AllowReplace) Class = Class->GetReplacement();
    }
  }

  // check for valid class
  if (!Class->IsChildOf(VThinker::StaticClass())) {
    VObject::VMDumpCallStack();
    GCon->Logf(NAME_Error, "cannot spawn non-thinker object with class `%s` (expect engine crash soon!)", Class->GetName());
    return nullptr;
  }

  // instance limiter
  if (Class->GetLimitInstancesWithSub()) {
    do {
      VClass *limClass = (Class->InstanceLimitBaseClass ?: Class);
      if (!limClass->GetLimitInstancesWithSub()) {
        Class->InstanceLimitWithSubCvar.clear();
        Class->SetLimitInstancesWithSub(false);
        Class->InstanceLimitWithSub = 0;
        Class->InstanceLimitList.clear(); // it will never contain anything
        break;
      }
      // limiting enabled, check cvar
      VCvar *cv = limClass->InstanceLimitWithSubCvarPtr;
      if (!cv) {
        if (!limClass->InstanceLimitWithSubCvar.isEmpty()) cv = VCvar::FindVariable(*limClass->InstanceLimitWithSubCvar);
        limClass->InstanceLimitWithSubCvarPtr = cv;
        if (!cv) {
          limClass->InstanceLimitWithSubCvar.clear();
          limClass->SetLimitInstancesWithSub(false);
          limClass->InstanceLimitWithSub = 0;
          limClass->InstanceLimitList.clear(); // it will never contain anything
          // just in case
          Class->InstanceLimitWithSubCvar.clear();
          Class->SetLimitInstancesWithSub(false);
          Class->InstanceLimitWithSub = 0;
          Class->InstanceLimitList.clear(); // it will never contain anything
          break;
        }
      }
      int instlimit = cv->asInt();
      if (instlimit < 1) break;
      if (instlimit >= 0x3fffffff) break; // wtf?!
      instlimit += 1; // so thinker limiter could kick in
      //WARNING! some code may expect the proper spawn here, and will crash on `nullptr` result!
      //WARNING! this must be checked and fixed everywhere. maybe.
      // `>`, so thinker limiter could kick in
      if (limClass->InstanceCountWithSub > instlimit) {
        if (dbg_limiter_remove_messages.asBool()) {
          GCon->Logf(NAME_Debug, "prevented spawning of thinker object with class `%s` (limiter: %d/%d/%d)", Class->GetName(), instlimit, limClass->InstanceCountWithSub, Class->InstanceCountWithSub);
        }
        return nullptr;
      }
    } while (0);
  }

  // spawn it
  VThinker *Ret = (VThinker *)StaticSpawnNoReplace(Class);
  if (!Ret) {
    VObject::VMDumpCallStack();
    GCon->Logf(NAME_Error, "cannot spawn thinker object with class `%s` (expect engine crash soon!)", Class->GetName());
    return nullptr;
  }

  // setup spawn time, add thinker
  Ret->SpawnTime = Time;
  Ret->ServerUId = (srvUId ? srvUId : Ret->GetUniqueId());
  AddThinker(Ret);

  /*
  #ifdef CLIENT
  GCon->Logf(NAME_Debug, "SPAWNED: <%s>; forserver=%d", Ret->GetClass()->GetName(), (int)IsForServer());
  #endif
  */

  VThinker *OverRet = nullptr;

  if (IsForServer()) {
    #ifdef CLIENT
    // if we're spawning something on the client side, mark it as detached
    // this is harmless for non-authorithy entities
    if (GGameInfo && GGameInfo->NetMode == NM_Client) Ret->ThinkerFlags |= VThinker::TF_DetachComplete;
    #endif
    if (Class->IsChildOf(VEntity::StaticClass())) {
      VEntity *e = (VEntity *)Ret;
      e->Origin = AOrigin;
      e->Angles = AAngles;
      e->eventOnMapSpawn(mthing);
      // call it anyway, some script code may rely on this
      /*if (!e->IsGoingToDie())*/ {
        if (LevelInfo->LevelInfoFlags2&VLevelInfo::LIF2_BegunPlay) {
          e->eventBeginPlay();
          if (e->BeginPlayResult) {
            // check it, just in case
            if (e->BeginPlayResult->IsA(VEntity::StaticClass())) {
              OverRet = e->BeginPlayResult;
              /*
              if (!OverRet->IsA(AClass)) {
                GCon->Logf(NAME_Error, "%s:BeginPlay() tried to override return with non-compatible class `%s`", e->GetClass()->GetName(), OverRet->GetClass()->GetName());
              }
              */
            } else {
              GCon->Logf(NAME_Error, "%s:BeginPlay() tried to override return with non-entity class `%s`", e->GetClass()->GetName(), e->BeginPlayResult->GetClass()->GetName());
            }
          }
        }
      }
      if (!e->IsGoingToDie()) {
        // force colored blood for some entities
        e->CheckForceColoredBlood();
        //HACK! do not call it for `VLevel`, as it is empty anyway
        if (GetClass() != VLevel::StaticClass()) eventEntitySpawned(e);
      }
    }
  }

  return (OverRet ? OverRet : Ret);
}


//==========================================================================
//
//  Script iterators
//
//==========================================================================
class VScriptThinkerLevelIterator : public VScriptIterator {
private:
  VLevel *Self;
  VClass *Class;
  VThinker **Out;
  VThinker *Current;

public:
  VScriptThinkerLevelIterator (VLevel *ASelf, VClass *AClass, VThinker **AOut)
    : Self(ASelf)
    , Class(AClass)
    , Out(AOut)
    , Current(nullptr)
  {}

  virtual bool GetNext () override {
    Current = (Current ? Current->Next : Class ? Self->ThinkerHead : nullptr);
    *Out = nullptr;
    for (; Current; Current = Current->Next) {
      if (!Current->IsGoingToDie() && Current->IsA(Class)) {
        *Out = Current;
        break;
      }
    }
    return !!*Out;
  }
};


class VActivePlayersLevelIterator : public VScriptIterator {
private:
  VLevel *Self;
  VBasePlayer **Out;
  int Index;

public:
  VActivePlayersLevelIterator (VBasePlayer **AOut) : Out(AOut), Index(0) {}

  virtual bool GetNext () override {
    while (Index < MAXPLAYERS && GGameInfo) {
      VBasePlayer *P = GGameInfo->Players[Index];
      ++Index;
      if (P && (P->PlayerFlags&VBasePlayer::PF_Spawned)) {
        *Out = P;
        return true;
      }
    }
    return false;
  }
};


IMPLEMENT_FUNCTION(VLevel, AllThinkers) {
  /*
  P_GET_PTR(VThinker *, Thinker);
  P_GET_PTR(VClass, Class);
  P_GET_SELF;
  */
  VThinker **Thinker;
  VClass *Class;
  vobjGetParamSelf(Class, Thinker);
  RET_PTR(new VScriptThinkerLevelIterator(Self, Class, Thinker));
}


IMPLEMENT_FUNCTION(VLevel, AllActivePlayers) {
  /*
  P_GET_PTR(VBasePlayer *, Out);
  P_GET_SELF;
  */
  VBasePlayer **Out;
  vobjGetParam(Out);
  RET_PTR(new VActivePlayersLevelIterator(Out));
}
