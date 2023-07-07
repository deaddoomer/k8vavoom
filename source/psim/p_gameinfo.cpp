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
#include "../gamedefs.h"
#include "../server/server.h"
#include "../menu.h"
#include "../filesys/files.h"
#ifdef CLIENT
# include "../client/client.h"
#endif
#include "p_player.h"


IMPLEMENT_CLASS(V, GameInfo)

VGameInfo *GGameInfo = nullptr;

static TArray<VClass *> savedPlayerClasses;
static bool playerClassesSaved = false;


//==========================================================================
//
//  VGameInfo::PlayersIter::nextByType
//
//==========================================================================
void VGameInfo::PlayersIter::nextByType () noexcept {
  for (++plridx; plridx < MAXPLAYERS; ++plridx) {
    if (!gi->Players[plridx]) continue;
    switch (type) {
      case PlrAll: return;
      default: // just in case
      case PlrOnlySpawned: if (gi->Players[plridx]->PlayerFlags&VBasePlayer::PF_Spawned) return; break;
    }
  }
  plridx = MAXPLAYERS;
}


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
  vobjGetParamSelf();
  RET_BOOL(Self ? Self->IsInWipe() : false);
}


//==========================================================================
//
//  VGameInfo::IsPaused
//
//==========================================================================
bool VGameInfo::IsPaused (bool ignoreOpenConsole) {
  if (NetMode <= NM_TitleMap) return false;
#ifdef CLIENT
  // BadApple.wad hack
  if (NetMode == NM_Standalone) {
    const bool isBadApple = ((GLevel && GLevel->IsBadApple()) || (GClLevel && GClLevel->IsBadApple()));
    if (isBadApple) return IsInWipe();
  }
#endif
  // should we totally ignore pause flag in server mode?
  return
    !!(Flags&GIF_Paused)
#ifdef CLIENT
    // in single player pause game if in menu or console
    || IsInWipe() ||
    (NetMode == NM_Standalone && (MN_Active() || (!ignoreOpenConsole && C_Active())))
#endif
  ;
}


IMPLEMENT_FUNCTION(VGameInfo, get_isPaused) {
  vobjGetParamSelf();
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

  if (Args.length() < 2) {
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

  if (FL_IsIgnoredPlayerClass(Args[1])) {
    GCon->Logf(NAME_Init, "keyconf player class '%s' ignored due to mod detector orders", Class->GetName());
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
  GGameInfo->eventCmdWeaponSection(Args.length() > 1 ? Args[1] : "");
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


//==========================================================================
//
//  COMMAND ForcePlayerClass
//
//==========================================================================
static void savePlayerClasses () {
  if (playerClassesSaved) return;
  playerClassesSaved = true;
  vassert(savedPlayerClasses.length() == 0);

  VClass *PPClass = VClass::FindClass("PlayerPawn");
  if (!PPClass) return;

  for (VClass *cc : GGameInfo->PlayerClasses) {
    if (!cc) continue;
    if (!cc->IsChildOf(PPClass)) continue;
    savedPlayerClasses.append(cc);
  }
}


//==========================================================================
//
//  COMMAND ForcePlayerClass
//
//==========================================================================
COMMAND_WITH_AC(ForcePlayerClass) {
  savePlayerClasses();

  if (GGameInfo->NetMode > NM_TitleMap) {
    CMD_FORWARD_TO_SERVER();
    if (GGameInfo->NetMode >= NM_Client) {
      GCon->Logf(NAME_Error, "Cannot force player class on client!");
      return;
    }
  }

  if (Args.length() < 2) {
    GCon->Logf(NAME_Warning, "ForcePlayerClass: Player class name missing");
    return;
  }

  VClass *PPClass = VClass::FindClass("PlayerPawn");
  if (!PPClass) {
    GCon->Logf(NAME_Warning, "ForcePlayerClass: Can't find PlayerPawn class");
    return;
  }

  TArray<VClass *> clist;
  for (int f = 1; f < Args.length(); ++f) {
    VStr cn = Args[f];
    if (cn.length() && cn[0] == '*') cn.chopLeft(1);
    VClass *Class = VClass::FindClassNoCase(*cn);
    if (!Class) {
      GCon->Logf(NAME_Warning, "ForcePlayerClass: No such class `%s`", *cn);
      continue;
    }
    if (!Class->IsChildOf(PPClass)) {
      GCon->Logf(NAME_Warning, "ForcePlayerClass: '%s' is not a player pawn class", *cn);
      continue;
    }
    clist.append(Class);
  }

  if (clist.length() == 0) {
    GCon->Logf(NAME_Warning, "ForcePlayerClass: no valid player classes were specified.");
    return;
  }

  GGameInfo->PlayerClasses.Clear();
  for (auto &&cc : clist) GGameInfo->PlayerClasses.Append(cc);
}


//==========================================================================
//
//  COMMAND_AC ForcePlayerClass
//
//==========================================================================
COMMAND_AC(ForcePlayerClass) {
  savePlayerClasses();

  VClass *PPClass = VClass::FindClass("PlayerPawn");
  if (!PPClass) return VStr::EmptyString;

  TArray<VClass *> clist;
  for (auto &&cc : savedPlayerClasses) {
    if (!cc) continue;
    if (!cc->IsChildOf(PPClass)) continue;
    clist.append(cc);
  }

  if (clist.length() == 0) return VStr::EmptyString;

  TArray<VStr> list;
  VStr prefix = (aidx < args.length() ? args[aidx] : VStr());
  if (aidx == 1) {
    if (prefix.length() && prefix[0] == '*') {
      VStr pfx = prefix;
      pfx.chopLeft(1);
      VClass::ForEachChildOf("PlayerPawn", [&list, &pfx](VClass *cls) {
        if (pfx.length() == 0 || VStr::startsWithCI(cls->GetName(), *pfx)) {
          list.append(VStr("*")+cls->GetName());
        }
        return FERes::FOREACH_NEXT;
      });
    } else {
      for (VClass *cn : clist) list.append(cn->GetName());
    }
    return VCommand::AutoCompleteFromListCmd(prefix, list);
  } else {
    return VStr::EmptyString;
  }
}


//==========================================================================
//
//  COMMAND PrintPlayerClasses
//
//==========================================================================
COMMAND(PrintPlayerClasses) {
  savePlayerClasses();

  if (GGameInfo->NetMode > NM_TitleMap) {
    CMD_FORWARD_TO_SERVER();
    if (GGameInfo->NetMode >= NM_Client) {
      GCon->Logf(NAME_Error, "Cannot list player classes on client!");
      return;
    }
  }

  GCon->Logf("=== %d known player class%s ===", GGameInfo->PlayerClasses.length(), (GGameInfo->PlayerClasses.length() != 1 ? "es" : ""));
  for (auto &&cc : savedPlayerClasses) GCon->Logf("  %s (%s)", cc->GetName(), *cc->Loc.toStringNoCol());
}
