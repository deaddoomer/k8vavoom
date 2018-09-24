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
#include "gamedefs.h"
#include "cl_local.h"
#include "ui.h"

IMPLEMENT_CLASS(V, RootWidget);

VRootWidget *GRoot;


//==========================================================================
//
//  VRootWidget::VRootWidget
//
//==========================================================================
VRootWidget::VRootWidget () {
}


//==========================================================================
//
//  VRootWidget::Init
//
//==========================================================================
void VRootWidget::Init () {
  guard(VRootWidget::Init);
  Super::Init(nullptr);
  SetSize(640, 480);

  MouseCursorPic = GTextureManager.AddPatch("mc_arrow", TEXTYPE_Pic);
  unguard;
}


//==========================================================================
//
//  VRootWidget::DrawWidgets
//
//==========================================================================
void VRootWidget::DrawWidgets () {
  guard(VRootWidget::DrawWidgets)
  DrawTree();

  if (RootFlags&RWF_MouseEnabled) DrawPic(MouseX-16, MouseY-16, MouseCursorPic);
  unguard;
}


//==========================================================================
//
//  VRootWidget::TickWidgets
//
//==========================================================================
void VRootWidget::TickWidgets (float DeltaTime) {
  guard(VRootWidget::TickWidgets);
  if (GetFlags()&_OF_Destroyed) return;
  if (SizeScaleX != fScaleX || SizeScaleY != fScaleY) {
    SizeScaleX = fScaleX;
    SizeScaleY = fScaleY;
    ClipTree();
  }
  TickTree(DeltaTime);
  unguard;
}


//==========================================================================
//
//  VRootWidget::Responder
//
//==========================================================================
bool VRootWidget::Responder (event_t *Event) {
  guard(VRootWidget::Responder);
  if (GetFlags()&_OF_Destroyed) return false;
  if (RootFlags&RWF_MouseEnabled) {
    // handle mouse movement
    if (Event->type == ev_mouse) {
      MouseMoveEvent(MouseX+Event->data2, MouseY-Event->data3);
      return true;
    }
    // handle mouse buttons
    if ((Event->type == ev_keydown || Event->type == ev_keyup) &&
        Event->data1 >= K_MOUSE1 && Event->data1 <= K_MOUSE3)
    {
      return MouseButtonEvent(Event->data1, Event->type == ev_keydown);
    }
  }

  // handle keyboard events
  if (Event->type == ev_keydown || Event->type == ev_keyup) {
    // find the top-most focused widget
    VWidget *W = CurrentFocusChild;
    while (W && W->CurrentFocusChild) W = W->CurrentFocusChild;
    // call event handlers
    while (W) {
      if (Event->type == ev_keydown) {
        if (W->OnKeyDown(Event->data1)) return true;
      } else {
        if (W->OnKeyUp(Event->data1)) return true;
      }
      W = W->ParentWidget;
    }
  }
  return false;
  unguard;
}


//==========================================================================
//
//  VRootWidget::SetMouse
//
//==========================================================================
void VRootWidget::SetMouse (bool MouseOn) {
  guard(VRootWidget::SetMouse);
  if (MouseOn) {
    RootFlags |= RWF_MouseEnabled;
  } else {
    RootFlags &= ~RWF_MouseEnabled;
  }
  if (!MouseOn && GInput) GInput->RegrabMouse();
  unguard;
}


//==========================================================================
//
//  VRootWidget::MouseMoveEvent
//
//==========================================================================
void VRootWidget::MouseMoveEvent (int NewX, int NewY) {
  guard(VRootWidget::MouseMoveEvent);
  // remember old mouse coordinates
  int OldMouseX = MouseX;
  int OldMouseY = MouseY;

  // update mouse position
  MouseX = NewX;
  MouseY = NewY;

  // clip mouse coordinates against window boundaries
  if (MouseX < 0) MouseX = 0; else if (MouseX >= SizeWidth) MouseX = SizeWidth-1;
  if (MouseY < 0) MouseY = 0; else if (MouseY >= SizeHeight) MouseY = SizeHeight-1;

  // check if mouse position has changed
  if (MouseX == OldMouseX && MouseY == OldMouseY) return;

  // find widget under old position
  float ScaledOldX = OldMouseX*SizeScaleX;
  float ScaledOldY = OldMouseY*SizeScaleY;
  float ScaledNewX = MouseX*SizeScaleX;
  float ScaledNewY = MouseY*SizeScaleY;
  VWidget *OldFocus = GetWidgetAt(ScaledOldX, ScaledOldY);
  for (VWidget *W = OldFocus; W != nullptr; W = W->ParentWidget) {
    if (W->OnMouseMove(
      (int)((ScaledOldX-W->ClipRect.OriginX)/W->ClipRect.ScaleX),
      (int)((ScaledOldY-W->ClipRect.OriginY)/W->ClipRect.ScaleY),
      (int)((ScaledNewX-W->ClipRect.OriginX)/W->ClipRect.ScaleX),
      (int)((ScaledNewY-W->ClipRect.OriginY)/W->ClipRect.ScaleY)))
    {
      break;
    }
  }
  VWidget *NewFocus = GetWidgetAt(ScaledNewX, ScaledNewY);
  if (OldFocus != nullptr && OldFocus != NewFocus) {
    OldFocus->WidgetFlags &= ~(WF_LMouseDown|WF_MMouseDown|WF_RMouseDown);
    OldFocus->OnMouseLeave();
    NewFocus->OnMouseEnter();
  }
  unguard;
}


//==========================================================================
//
//  VRootWidget::MouseButtonEvent
//
//==========================================================================
bool VRootWidget::MouseButtonEvent (int Button, bool Down) {
  guard(VRootWidget::MouseButtonEvent);
  // find widget under mouse
  float ScaledX = MouseX*SizeScaleX;
  float ScaledY = MouseY*SizeScaleY;
  VWidget *Focus = GetWidgetAt(ScaledX, ScaledY);

  if (Down) {
         if (Button == K_MOUSE1) Focus->WidgetFlags |= WF_LMouseDown;
    else if (Button == K_MOUSE3) Focus->WidgetFlags |= WF_MMouseDown;
    else if (Button == K_MOUSE2) Focus->WidgetFlags |= WF_RMouseDown;
  } else {
    int LocalX = (int)((ScaledX-Focus->ClipRect.OriginX)/Focus->ClipRect.ScaleX);
    int LocalY = (int)((ScaledY-Focus->ClipRect.OriginY)/Focus->ClipRect.ScaleY);
    if (Button == K_MOUSE1 && (Focus->WidgetFlags&WF_LMouseDown)) {
      Focus->WidgetFlags &= ~WF_LMouseDown;
      Focus->OnMouseClick(LocalX, LocalY);
    }
    if (Button == K_MOUSE3 && (Focus->WidgetFlags&WF_MMouseDown)) {
      Focus->WidgetFlags &= ~WF_MMouseDown;
      Focus->OnMMouseClick(LocalX, LocalY);
    }
    if (Button == K_MOUSE2 && (Focus->WidgetFlags&WF_RMouseDown)) {
      Focus->WidgetFlags &= ~WF_RMouseDown;
      Focus->OnRMouseClick(LocalX, LocalY);
    }
  }

  for (VWidget *W = Focus; W; W = W->ParentWidget) {
    int LocalX = (int)((ScaledX-W->ClipRect.OriginX)/W->ClipRect.ScaleX);
    int LocalY = (int)((ScaledY-W->ClipRect.OriginY)/W->ClipRect.ScaleY);
    if (Down) {
      if (W->OnMouseDown(LocalX, LocalY, Button)) return true;
    } else {
      if (W->OnMouseUp(LocalX, LocalY, Button)) return true;
    }
  }
  return false;
  unguard;
}


//==========================================================================
//
//  VRootWidget::StaticInit
//
//==========================================================================
void VRootWidget::StaticInit () {
  guard(VRootWidget::StaticInit);
  GRoot = Spawn<VRootWidget>();
  GRoot->Init();
  GClGame->GRoot = GRoot;
  unguard;
}


//==========================================================================
//
//  Natives
//
//==========================================================================
IMPLEMENT_FUNCTION(VRootWidget, SetMouse) {
  P_GET_BOOL(MouseOn);
  P_GET_SELF;
  Self->SetMouse(MouseOn);
}
