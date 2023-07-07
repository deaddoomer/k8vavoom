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
//**
//**  DOOM selection menu, options, episode etc.
//**  Sliders and icons. Kinda widget stuff.
//**
//**************************************************************************
#include "gamedefs.h"
#include "menu.h"
#ifdef CLIENT
# include "input.h"
#endif
#include "client/client.h"
#include "client/cl_local.h"
#include "widgets/ui.h"


//==========================================================================
//
//  MN_Init
//
//==========================================================================
void MN_Init () {
#ifdef SERVER
  GClGame->ClientFlags |= VClientGameBase::CF_LocalServer;
#else
  GClGame->ClientFlags &= ~VClientGameBase::CF_LocalServer;
#endif
  VRootWidget::StaticInit();
  if (GClGame) GClGame->eventRootWindowCreated();
}


//==========================================================================
//
//  MN_ActivateMenu
//
//==========================================================================
void MN_ActivateMenu () {
  // intro might call this repeatedly
  if (!MN_Active() && GClGame) GClGame->eventSetMenu("Main");
}


//==========================================================================
//
//  MN_DeactivateMenu
//
//==========================================================================
void MN_DeactivateMenu () {
  if (GClGame) {
    GClGame->eventDeactivateMenu();
    VWidget::CleanupWidgets();
  }
}


//==========================================================================
//
//  MN_CheckStartupWarning
//
//==========================================================================
void MN_CheckStartupWarning () {
  if (!GClGame) return;
  if (flWarningMessage.isEmpty()) return;
  //GCon->Logf(NAME_Debug, "***MSG***: %s", *flWarningMessage.quote());
  GClGame->eventMessageBoxShowWarning(flWarningMessage);
  flWarningMessage.clear();
}


//==========================================================================
//
//  MN_Responder
//
//==========================================================================
bool MN_Responder (event_t *ev) {
  if (!GClGame) return false;
  if (GClGame->eventMessageBoxResponder(ev)) return true;

  // show menu?
  if (!MN_Active() && ev->type == ev_keydown && !C_Active() &&
      (!cl || cls.demoplayback || GGameInfo->NetMode == NM_TitleMap))
  {
    bool doActivate = (ev->keycode < K_F1 || ev->keycode > K_F12);
    #ifdef CLIENT
    if (doActivate) {
      VStr down, up;
      GInput->GetBinding(ev->keycode, down, up);
      if (down.ICmp("ToggleConsole") == 0) doActivate = false;
    }
    #endif
    if (doActivate) {
      MN_ActivateMenu();
      return true;
    }
  }

  return GClGame->eventMenuResponder(ev);
}


//==========================================================================
//
//  MN_Active
//
//==========================================================================
bool MN_Active () {
  return (GClGame ? (GClGame->eventMenuActive() || GClGame->eventMessageBoxActive()) : false);
}


//==========================================================================
//
//  DoMenuCompletions
//
//==========================================================================
static VStr DoMenuCompletions (const TArray<VStr> &args, int aidx, int mode) {
  TArray<VStr> list;
  VStr prefix = (aidx < args.length() ? args[aidx] : VStr());
  if (aidx == 1) {
    GClGame->eventGetAllMenuNames(list, mode);
    return VCommand::AutoCompleteFromListCmd(prefix, list);
  } else {
    return VStr::EmptyString;
  }
}


//==========================================================================
//
//  COMMAND_WITH_AC SetMenu
//
//==========================================================================
COMMAND_WITH_AC(SetMenu) {
  if (GClGame) GClGame->eventSetMenu(Args.length() > 1 ? *Args[1] : "");
}


//==========================================================================
//
//  COMMAND_AC SetMenu
//
//==========================================================================
COMMAND_AC(SetMenu) {
  return DoMenuCompletions(args, aidx, 0);
}


//==========================================================================
//
//  COMMAND_WITH_AC OpenMenu
//
//==========================================================================
COMMAND_WITH_AC(OpenMenu) {
  if (GClGame) GClGame->eventSetMenu(Args.length() > 1 ? *Args[1] : "");
}


//==========================================================================
//
//  COMMAND_AC OpenMenu
//
//==========================================================================
COMMAND_AC(OpenMenu) {
  return DoMenuCompletions(args, aidx, 1);
}


//==========================================================================
//
//  COMMAND_WITH_AC OpenGZMenu
//
//==========================================================================
COMMAND_WITH_AC(OpenGZMenu) {
  if (GClGame) GClGame->eventSetMenu(Args.length() > 1 ? *Args[1] : "");
}


//==========================================================================
//
//  COMMAND_AC OpenGZMenu
//
//==========================================================================
COMMAND_AC(OpenGZMenu) {
  return DoMenuCompletions(args, aidx, -1);
}
