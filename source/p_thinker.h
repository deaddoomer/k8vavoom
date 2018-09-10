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
  ROLE_None,
  ROLE_DumbProxy,
  ROLE_Authority
};

//  Type of the sound origin, used for origin IDs when playing sounds.
enum
{
  SNDORG_Entity,
  SNDORG_Sector,
  SNDORG_PolyObj,
};

//
//  VThinker
//
//  Doubly linked list of actors and other special elements of a level.
//
class VThinker : public VGameObject
{
  DECLARE_CLASS(VThinker, VGameObject, 0)

  VLevel *XLevel;   //  Level object.
  VLevelInfo *Level;    //  Level info object.

  VThinker *Prev;
  VThinker *Next;

  vuint8      Role;
  vuint8      RemoteRole;

  enum
  {
    TF_AlwaysRelevant     = 0x00000001,
    TF_NetInitial       = 0x00000002,
    TF_NetOwner         = 0x00000004,
  };
  vuint32     ThinkerFlags;

  static int    FIndex_Tick;

  VThinker();

  //  VObject interface
  virtual void Destroy() override;
  virtual void Serialise(VStream&) override;

  //  VThinker interface
  virtual void Tick(float);
  virtual void DestroyThinker();
  virtual void AddedToLevel();
  virtual void RemovedFromLevel();

  void StartSound(const TVec&, vint32, vint32, vint32, float, float, bool, bool=false);
  void StopSound(vint32, vint32);
  void StartSoundSequence(const TVec&, vint32, VName, vint32);
  void AddSoundSequenceChoice(vint32, VName);
  void StopSoundSequence(vint32);

  void BroadcastPrint(const char*);
  void BroadcastPrintf(const char*, ...);
  void BroadcastCentrePrint(const char*);
  void BroadcastCentrePrintf(const char*, ...);

  DECLARE_FUNCTION(Spawn)
  DECLARE_FUNCTION(Destroy)

  //  Print functions
  DECLARE_FUNCTION(bprint)

  DECLARE_FUNCTION(AllocDlight)
  DECLARE_FUNCTION(NewParticle)
  DECLARE_FUNCTION(GetAmbientSound)

  //  Iterators
  DECLARE_FUNCTION(AllThinkers)
  DECLARE_FUNCTION(AllActivePlayers)
  DECLARE_FUNCTION(PathTraverse)
  DECLARE_FUNCTION(RadiusThings)

  void eventClientTick(float DeltaTime)
  {
    P_PASS_SELF;
    P_PASS_FLOAT(DeltaTime);
    EV_RET_VOID(NAME_ClientTick);
  }
};

template <class T> class TThinkerIterator
{
private:
  VThinker *Th;
  void GetNext()
  {
    while (Th && (!Th->IsA(T::StaticClass()) ||
      (Th->GetFlags() & _OF_DelayedDestroy)))
    {
      Th = Th->Next;
    }
  }
public:
  TThinkerIterator(const VLevel *Level)
  {
    Th = Level->ThinkerHead;
    GetNext();
  }
  operator bool()
  {
    return Th != nullptr;
  }
  void operator ++()
  {
    if (Th)
    {
      Th = Th->Next;
      GetNext();
    }
  }
  T *operator ->()
  {
    return (T*)Th;
  }
  T *operator *()
  {
    return (T*)Th;
  }
};
