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
//**  DOOM selection menu, options, episode etc.
//**  Sliders and icons. Kinda widget stuff.
//**
//**************************************************************************
#include "gamedefs.h"
#include "cl_local.h"
#include "ui/ui.h"


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
  GClGame->eventRootWindowCreated();
}


//==========================================================================
//
//  MN_ActivateMenu
//
//==========================================================================
void MN_ActivateMenu () {
  // intro might call this repeatedly
  if (!MN_Active()) GClGame->eventSetMenu("Main");
}


//==========================================================================
//
//  MN_DeactivateMenu
//
//==========================================================================
void MN_DeactivateMenu () {
  GClGame->eventDeactivateMenu();
}


//==========================================================================
//
//  MN_Responder
//
//==========================================================================
bool MN_Responder (event_t *ev) {
  if (GClGame->eventMessageBoxResponder(ev)) return true;

  // show menu?
  if (!MN_Active() && ev->type == ev_keydown && !C_Active() &&
      (!cl || cls.demoplayback || GGameInfo->NetMode == NM_TitleMap))
  {
    bool doActivate = (ev->data1 < K_F1 || ev->data1 > K_F12);
    if (doActivate) {
      VStr down, up;
      GInput->GetBinding(ev->data1, down, up);
      if (down.ICmp("ToggleConsole") == 0) doActivate = false;
    }
    if (doActivate) {
      MN_ActivateMenu();
      return true;
    }
  }

  return GClGame->eventMenuResponder(ev);
}


//==========================================================================
//
//  MN_Drawer
//
//==========================================================================
void MN_Drawer () {
  GClGame->eventMessageBoxDrawer();
}


//==========================================================================
//
//  MN_Active
//
//==========================================================================
bool MN_Active () {
  return (GClGame->eventMenuActive() || GClGame->eventMessageBoxActive());
}


//==========================================================================
//
//  COMMAND SetMenu
//
//==========================================================================
COMMAND(SetMenu) {
  GClGame->eventSetMenu(Args.Num() > 1 ? *Args[1] : "");
}
