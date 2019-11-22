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
//**  Copyright (C) 2018-2019 Ketmar Dark
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
#include "cl_local.h"
#include "sv_local.h"
#include "neoui/neoui.h"


IMPLEMENT_CLASS(V, GameInfo)

VGameInfo *GGameInfo;


//==========================================================================
//
//  VGameInfo::VGameInfo
//
//==========================================================================
/*
VGameInfo::VGameInfo ()
  : AcsHelper(E_NoInit)
  , GenericConScript(E_NoInit)
  , PlayerClasses(E_NoInit)
{
}
*/


//==========================================================================
//
//  VGameInfo::IsWipeAllowed
//
//==========================================================================
bool VGameInfo::IsWipeAllowed () {
#ifdef CLIENT
  return (NetMode == NM_Standalone);
#else
  return false;
#endif
}


//==========================================================================
//
//  VGameInfo::IsInWipe
//
//==========================================================================
bool VGameInfo::IsInWipe () {
#ifdef CLIENT
  // in single player pause game if in menu or console
  if (GLevel && serverStartRenderFramesTic > 0 && GLevel->TicTime < serverStartRenderFramesTic) return false;
  return (clWipeTimer >= 0.0f);
#else
  return false;
#endif
}


IMPLEMENT_FUNCTION(VGameInfo, get_isInWipe) {
  P_GET_SELF;
  RET_BOOL(Self ? Self->IsInWipe() : false);
}


//==========================================================================
//
//  VGameInfo::IsPaused
//
//==========================================================================
bool VGameInfo::IsPaused () {
  if (NetMode <= NM_TitleMap) return false;
#ifdef CLIENT
  // in single player pause game if in menu or console
  return (Flags&GIF_Paused) || IsInWipe() || (NetMode == NM_Standalone && (MN_Active() || C_Active() || NUI_IsPaused()));
#else
  return !!(Flags&GIF_Paused);
#endif
}


IMPLEMENT_FUNCTION(VGameInfo, get_isPaused) {
  P_GET_SELF;
  RET_BOOL(Self ? Self->IsPaused() : false);
}


//==========================================================================
//
//  COMMAND ClearPlayerClasses
//
//==========================================================================
COMMAND(ClearPlayerClasses) {
  if (!ParsingKeyConf) return;
  GGameInfo->PlayerClasses.Clear();
}


//==========================================================================
//
//  COMMAND AddPlayerClass
//
//==========================================================================
COMMAND(AddPlayerClass) {
  if (!ParsingKeyConf) return;

  if (Args.Num() < 2) {
    GCon->Logf(NAME_Warning, "AddPlayerClass: Player class name missing");
    return;
  }

  VClass *Class = VClass::FindClassNoCase(*Args[1]);
  if (!Class) {
    GCon->Logf(NAME_Warning, "AddPlayerClass: No such class `%s`", *Args[1]);
    return;
  }

  VClass *PPClass = VClass::FindClass("PlayerPawn");
  if (!PPClass) {
    GCon->Logf(NAME_Warning, "AddPlayerClass: Can't find PlayerPawn class");
    return;
  }

  if (!Class->IsChildOf(PPClass)) {
    GCon->Logf(NAME_Warning, "AddPlayerClass: '%s' is not a player pawn class", *Args[1]);
    return;
  }

  GGameInfo->PlayerClasses.Append(Class);
}


//==========================================================================
//
//  COMMAND WeaponSection
//
//==========================================================================
COMMAND(WeaponSection) {
  if (!ParsingKeyConf) return;
  GGameInfo->eventCmdWeaponSection(Args.Num() > 1 ? Args[1] : "");
}


//==========================================================================
//
//  COMMAND SetSlot
//
//==========================================================================
COMMAND(SetSlot) {
  if (!ParsingKeyConf) return;
  GGameInfo->eventCmdSetSlot(&Args, true); // as keyconf
}


//==========================================================================
//
//  COMMAND AddSlotDefault
//
//==========================================================================
COMMAND(AddSlotDefault) {
  if (!ParsingKeyConf) return;
  GGameInfo->eventCmdAddSlotDefault(&Args, true); // as keyconf
}
