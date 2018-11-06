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
//**
//**  THINKERS
//**
//**  All thinkers should be allocated by Z_Malloc so they can be operated
//**  on uniformly. The actual structures will vary in size, but the first
//**  element must be VThinker.
//**
//**************************************************************************
#include "gamedefs.h"
#include "net/network.h"
#include "sv_local.h"


int VThinker::FIndex_Tick;

IMPLEMENT_CLASS(V, Thinker)


//==========================================================================
//
//  VThinker::VThinker
//
//==========================================================================
VThinker::VThinker () {
}


//==========================================================================
//
//  VThinker::Destroy
//
//==========================================================================
void VThinker::Destroy () {
  guard(VThinker::Destroy);
  // close any thinker channels
  if (XLevel) XLevel->NetContext->ThinkerDestroyed(this);
  Super::Destroy();
  unguard;
}


//==========================================================================
//
//  VThinker::Serialise
//
//==========================================================================
void VThinker::Serialise (VStream &Strm) {
  guard(VThinker::Serialise);
  Super::Serialise(Strm);
  if (Strm.IsLoading()) XLevel->AddThinker(this);
  unguard;
}


//==========================================================================
//
//  VThinker::Tick
//
//==========================================================================
void VThinker::Tick (float DeltaTime) {
  guard(VThinker::Tick);
  P_PASS_SELF;
  P_PASS_FLOAT(DeltaTime);
  EV_RET_VOID_IDX(FIndex_Tick);
  unguard;
}


//==========================================================================
//
//  VThinker::DestroyThinker
//
//==========================================================================
void VThinker::DestroyThinker () {
  guard(VThinker::DestroyThinker);
  SetFlags(_OF_DelayedDestroy);
  unguard;
}


//==========================================================================
//
//  VThinker::AddedToLevel
//
//==========================================================================
void VThinker::AddedToLevel () {
}


//==========================================================================
//
//  VThinker::RemovedFromLevel
//
//==========================================================================
void VThinker::RemovedFromLevel () {
}


//==========================================================================
//
//  VThinker::StartSound
//
//==========================================================================
void VThinker::StartSound (const TVec &origin, vint32 origin_id,
  vint32 sound_id, vint32 channel, float volume, float Attenuation,
  bool Loop, bool Local)
{
  guard(VThinker::StartSound);
  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (!Level->Game->Players[i]) continue;
    if (!(Level->Game->Players[i]->PlayerFlags & VBasePlayer::PF_Spawned)) continue;
    Level->Game->Players[i]->eventClientStartSound(sound_id, origin, (Local ? -666 : origin_id), channel, volume, Attenuation, Loop);
  }
  unguard;
}


//==========================================================================
//
//  VThinker::StopSound
//
//==========================================================================
void VThinker::StopSound (vint32 origin_id, vint32 channel) {
  guard(VThinker::StopSound);
  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (!Level->Game->Players[i]) continue;
    if (!(Level->Game->Players[i]->PlayerFlags&VBasePlayer::PF_Spawned)) continue;
    Level->Game->Players[i]->eventClientStopSound(origin_id, channel);
  }
  unguard;
}


//==========================================================================
//
//  VThinker::StartSoundSequence
//
//==========================================================================
void VThinker::StartSoundSequence (const TVec &Origin, vint32 OriginId, VName Name, vint32 ModeNum) {
  guard(VThinker::StartSoundSequence);
  // remove any existing sequences of this origin
  for (int i = 0; i < XLevel->ActiveSequences.length(); ++i) {
    if (XLevel->ActiveSequences[i].OriginId == OriginId) {
      XLevel->ActiveSequences.RemoveIndex(i);
      --i;
    }
  }

  VSndSeqInfo &Seq = XLevel->ActiveSequences.Alloc();
  Seq.Name = Name;
  Seq.OriginId = OriginId;
  Seq.Origin = Origin;
  Seq.ModeNum = ModeNum;

  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (!Level->Game->Players[i]) continue;
    if (!(Level->Game->Players[i]->PlayerFlags&VBasePlayer::PF_Spawned)) continue;
    Level->Game->Players[i]->eventClientStartSequence(Origin, OriginId, Name, ModeNum);
  }
  unguard;
}


//==========================================================================
//
//  VThinker::AddSoundSequenceChoice
//
//==========================================================================
void VThinker::AddSoundSequenceChoice (int origin_id, VName Choice) {
  guard(VThinker::AddSoundSequenceChoice);
  // remove it from server's sequences list
  for (int i = 0; i < XLevel->ActiveSequences.length(); ++i) {
    if (XLevel->ActiveSequences[i].OriginId == origin_id) {
      XLevel->ActiveSequences[i].Choices.Append(Choice);
    }
  }

  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (!Level->Game->Players[i]) continue;
    if (!(Level->Game->Players[i]->PlayerFlags&VBasePlayer::PF_Spawned)) continue;
    Level->Game->Players[i]->eventClientAddSequenceChoice(origin_id, Choice);
  }
  unguard;
}


//==========================================================================
//
//  VThinker::StopSoundSequence
//
//==========================================================================
void VThinker::StopSoundSequence (int origin_id) {
  guard(VThinker::StopSoundSequence);
  // remove it from server's sequences list
  for (int i = 0; i < XLevel->ActiveSequences.length(); ++i) {
    if (XLevel->ActiveSequences[i].OriginId == origin_id) {
      XLevel->ActiveSequences.RemoveIndex(i);
      --i;
    }
  }

  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (!Level->Game->Players[i]) continue;
    if (!(Level->Game->Players[i]->PlayerFlags&VBasePlayer::PF_Spawned)) continue;
    Level->Game->Players[i]->eventClientStopSequence(origin_id);
  }
  unguard;
}


//==========================================================================
//
//  VThinker::BroadcastPrint
//
//==========================================================================
void VThinker::BroadcastPrint (const char *s) {
  guard(VThinker::BroadcastPrint);
  for (int i = 0; i < svs.max_clients; ++i) {
    if (Level->Game->Players[i]) Level->Game->Players[i]->eventClientPrint(s);
  }
  unguard;
}


//==========================================================================
//
//  VThinker::BroadcastPrintf
//
//==========================================================================
__attribute__((format(printf,2,3))) void VThinker::BroadcastPrintf (const char *s, ...) {
  guard(VThinker::BroadcastPrintf);
  va_list v;
  static char buf[4096];

  va_start(v, s);
  vsnprintf(buf, sizeof(buf), s, v);
  va_end(v);

  BroadcastPrint(buf);
  unguard;
}


//==========================================================================
//
//  VThinker::BroadcastCentrePrint
//
//==========================================================================
void VThinker::BroadcastCentrePrint (const char *s) {
  guard(VThinker::BroadcastCentrePrint);
  for (int i = 0; i < svs.max_clients; ++i) {
    if (Level->Game->Players[i]) Level->Game->Players[i]->eventClientCentrePrint(s);
  }
  unguard;
}


//==========================================================================
//
//  VThinker::BroadcastCentrePrintf
//
//==========================================================================
__attribute__((format(printf,2,3))) void VThinker::BroadcastCentrePrintf (const char *s, ...) {
  guard(VThinker::BroadcastCentrePrintf);
  va_list v;
  static char buf[4096];

  va_start(v, s);
  vsnprintf(buf, sizeof(buf), s, v);
  va_end(v);

  BroadcastCentrePrint(buf);
  unguard;
}


//==========================================================================
//
//  Script iterators
//
//==========================================================================
class VScriptThinkerIterator : public VScriptIterator {
private:
  VThinker *Self;
  VClass *Class;
  VThinker **Out;
  VThinker *Current;

public:
  VScriptThinkerIterator(VThinker *ASelf, VClass *AClass, VThinker **AOut)
    : Self(ASelf)
    , Class(AClass)
    , Out(AOut)
    , Current(nullptr)
  {}

  virtual bool GetNext () override {
    if (!Current) {
      Current = Self->XLevel->ThinkerHead;
    } else {
      Current = Current->Next;
    }
    *Out = nullptr;
    while (Current) {
      if (Current->IsA(Class) && !(Current->GetFlags()&_OF_DelayedDestroy)) {
        *Out = Current;
        break;
      }
      Current = Current->Next;
    }
    return !!*Out;
  }
};


class VActivePlayersIterator : public VScriptIterator {
private:
  VThinker *Self;
  VBasePlayer **Out;
  int Index;

public:
  VActivePlayersIterator(VThinker *ASelf, VBasePlayer **AOut)
    : Self(ASelf)
    , Out(AOut)
    , Index(0)
  {}

  virtual bool GetNext() override {
    while (Index < MAXPLAYERS) {
      VBasePlayer *P = Self->Level->Game->Players[Index];
      ++Index;
      if (P && (P->PlayerFlags&VBasePlayer::PF_Spawned)) {
        *Out = P;
        return true;
      }
    }
    return false;
  }
};


//==========================================================================
//
//  Script natives
//
//==========================================================================
IMPLEMENT_FUNCTION(VThinker, Spawn) {
  P_GET_BOOL_OPT(AllowReplace, true);
  P_GET_PTR_OPT(mthing_t, mthing, nullptr);
  P_GET_AVEC_OPT(AAngles, TAVec(0, 0, 0));
  P_GET_VEC_OPT(AOrigin, TVec(0, 0, 0));
  P_GET_PTR(VClass, Class);
  P_GET_SELF;
  VEntity *SelfEnt = Cast<VEntity>(Self);
  // if spawner is entity, default to it's origin and angles
  if (SelfEnt) {
    if (!specified_AOrigin) AOrigin = SelfEnt->Origin;
    if (!specified_AAngles) AAngles = SelfEnt->Angles;
  }
  if (!Class) { VObject::VMDumpCallStack(); Sys_Error("Trying to spawn `None` class"); }
  RET_REF(Self->XLevel->SpawnThinker(Class, AOrigin, AAngles, mthing, AllowReplace));
}

IMPLEMENT_FUNCTION(VThinker, Destroy) {
  P_GET_SELF;
  Self->DestroyThinker();
}

IMPLEMENT_FUNCTION(VThinker, bprint) {
  VStr Msg = PF_FormatString();
  P_GET_SELF;
  Self->BroadcastPrint(*Msg);
}

// native final dlight_t *AllocDlight(Thinker Owner, TVec origin, optional float radius);
IMPLEMENT_FUNCTION(VThinker, AllocDlight) {
  P_GET_FLOAT_OPT(radius, 0);
  P_GET_VEC(lorg);
  P_GET_REF(VThinker, Owner);
  P_GET_SELF;
  RET_PTR(Self->XLevel->RenderData->AllocDlight(Owner, lorg, radius));
}

IMPLEMENT_FUNCTION(VThinker, NewParticle) {
  P_GET_SELF;
  RET_PTR(Self->XLevel->RenderData->NewParticle());
}

IMPLEMENT_FUNCTION(VThinker, GetAmbientSound) {
  P_GET_INT(Idx);
  RET_PTR(GSoundManager->GetAmbientSound(Idx));
}

IMPLEMENT_FUNCTION(VThinker, AllThinkers) {
  P_GET_PTR(VThinker*, Thinker);
  P_GET_PTR(VClass, Class);
  P_GET_SELF;
  RET_PTR(new VScriptThinkerIterator(Self, Class, Thinker));
}

IMPLEMENT_FUNCTION(VThinker, AllActivePlayers) {
  P_GET_PTR(VBasePlayer*, Out);
  P_GET_SELF;
  RET_PTR(new VActivePlayersIterator(Self, Out));
}

IMPLEMENT_FUNCTION(VThinker, PathTraverse) {
  P_GET_INT(flags);
  P_GET_FLOAT(y2);
  P_GET_FLOAT(x2);
  P_GET_FLOAT(y1);
  P_GET_FLOAT(x1);
  P_GET_PTR(intercept_t*, In);
  P_GET_SELF;
  RET_PTR(new VPathTraverse(Self, In, x1, y1, x2, y2, flags));
}

IMPLEMENT_FUNCTION(VThinker, RadiusThings) {
  P_GET_FLOAT(Radius);
  P_GET_VEC(Org);
  P_GET_PTR(VEntity*, EntPtr);
  P_GET_SELF;
  RET_PTR(new VRadiusThingsIterator(Self, EntPtr, Org, Radius));
}
