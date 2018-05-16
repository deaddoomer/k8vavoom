//**************************************************************************
//**
//**	##   ##    ##    ##   ##   ####     ####   ###     ###
//**	##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**	 ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**	 ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**	  ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**	   #    ##    ##    #      ####     ####   ##       ##
//**
//**	$Id$
//**
//**	Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**	This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**	This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "gamedefs.h"
#include "render/r_local.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
//	VLevel::AddThinker
//
//==========================================================================

void VLevel::AddThinker(VThinker* Th)
{
	guard(VLevel::AddThinker);
	Th->XLevel = this;
	Th->Level = LevelInfo;
	Th->Prev = ThinkerTail;
	Th->Next = NULL;
	if (ThinkerTail)
		ThinkerTail->Next = Th;
	else
		ThinkerHead = Th;
	ThinkerTail = Th;

	//	Notify thinker that is was just added to a level.
	Th->AddedToLevel();
	unguard;
}

//==========================================================================
//
//	VLevel::RemoveThinker
//
//==========================================================================

void VLevel::RemoveThinker(VThinker* Th)
{
	guard(VLevel::RemoveThinker);
	//	Notify that thinker is being removed from level.
	Th->RemovedFromLevel();

	if (Th == ThinkerHead)
		ThinkerHead = Th->Next;
	else
		Th->Prev->Next = Th->Next;
	if (Th == ThinkerTail)
		ThinkerTail = Th->Prev;
	else
		Th->Next->Prev = Th->Prev;
	unguard;
}

//==========================================================================
//
//	VLevel::DestroyAllThinkers
//
//==========================================================================

void VLevel::DestroyAllThinkers()
{
	guard(VLevel::DestroyAllThinkers);
	for (VThinker* Th = ThinkerHead; Th; Th = Th->Next)
	{
		if (!(Th->GetFlags() & _OF_DelayedDestroy))
		{
			Th->DestroyThinker();
		}
	}
	for (VThinker* Th = ThinkerHead; Th;)
	{
		VThinker* Next = Th->Next;
		Th->ConditionalDestroy();
		Th = Next;
	}
	ThinkerHead = NULL;
	ThinkerTail = NULL;
	unguard;
}

//==========================================================================
//
//	VLevel::TickWorld
//
//==========================================================================

void VLevel::TickWorld(float DeltaTime)
{
	guard(VLevel::TickWorld);

	// Run decal thinkers
	if (DeltaTime > 0) {
		decal_t* dc = decanimlist;
		while (dc) {
			bool removeIt = true;
			if (dc->animator) removeIt = !dc->animator->animate(dc, DeltaTime);
			decal_t* c = dc;
			dc = dc->nextanimated;
			if (removeIt) RemoveAnimatedDecal(c);
		}
	}

	// Run thinkers
	for (VThinker* Th = ThinkerHead; Th; Th = Th->Next)
	{
		if (!(Th->GetFlags() & _OF_DelayedDestroy))
		{
			Th->Tick(DeltaTime);
		}
		else
		{
			RemoveThinker(Th);
			Th->ConditionalDestroy();
		}
	}

	// Don't update specials if the level is frozen
	if (!(LevelInfo->LevelInfoFlags2 & VLevelInfo::LIF2_Frozen))
	{
		LevelInfo->eventUpdateSpecials();
	}

	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (LevelInfo->Game->Players[i] &&
			LevelInfo->Game->Players[i]->PlayerFlags & VBasePlayer::PF_Spawned)
		{
			LevelInfo->Game->Players[i]->eventSetViewPos();
		}
	}

	Time += DeltaTime;
	TicTime++;
	unguard;
}

//==========================================================================
//
//	VLevel::SpawnThinker
//
//==========================================================================

VThinker* VLevel::SpawnThinker(VClass* AClass, const TVec& AOrigin,
	const TAVec& AAngles, mthing_t* mthing, bool AllowReplace)
{
	guard(VLevel::SpawnThinker);
	check(AClass);
	VClass* Class = AllowReplace ? AClass->GetReplacement() : AClass;
	VThinker* Ret = (VThinker*)StaticSpawnObject(Class);
	AddThinker(Ret);

	if (IsForServer() && Class->IsChildOf(VEntity::StaticClass()))
	{
		((VEntity*)Ret)->Origin = AOrigin;
		((VEntity*)Ret)->Angles = AAngles;
		((VEntity*)Ret)->eventOnMapSpawn(mthing);
		if (LevelInfo->LevelInfoFlags2 & VLevelInfo::LIF2_BegunPlay)
		{
			((VEntity*)Ret)->eventBeginPlay();
		}
	}
	return Ret;
	unguard;
}
