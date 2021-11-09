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
#include "../host.h"  /* host_systime64_msec */
#include "../screen.h"
#include "../drawer.h"
#include "../text.h"
#include "ui.h"

extern VCvarB gl_pic_filtering;
extern VCvarB gl_font_filtering;


static VCvarF ui_msgxbox_wrap_trigger("ui_msgxbox_wrap_trigger", "0.9", "Maximum width (1 means whole screen) before message box will start wrapping; <=0 means \"don't\".", CVAR_Archive|CVAR_NoShadow);
static VCvarF ui_msgxbox_wrap_width("ui_msgxbox_wrap_width", "0.7", "Width (1 means whole screen) for message box wrapping; <=0 means \"don't\".", CVAR_Archive|CVAR_NoShadow);

static TMapNC<VName, bool> reportedMissingFonts;

TMapNC<VWidget *, bool> VWidget::AllWidgets;
static TArray<VWidget *> DyingWidgets;


IMPLEMENT_CLASS(V, Widget);


//==========================================================================
//
//  VWidget::PostCtor
//
//  this is called after defaults were blit
//
//==========================================================================
void VWidget::PostCtor () {
  //GCon->Logf(NAME_Debug, "created widget %s:%u:%p", GetClass()->GetName(), GetUniqueId(), (void *)this);
  AllWidgets.put(this, true);
  Super::PostCtor();
}


//==========================================================================
//
//  VWidget::CreateNewWidget
//
//==========================================================================
VWidget *VWidget::CreateNewWidget (VClass *AClass, VWidget *AParent) {
  VWidget *W = (VWidget *)StaticSpawnWithReplace(AClass);
  W->OfsX = W->OfsY = 0;
  W->Init(AParent);
  return W;
}


//==========================================================================
//
//  VWidget::CleanupWidgets
//
//  called by root widget in responder
//
//==========================================================================
void VWidget::CleanupWidgets () {
  if (GRoot && GRoot->IsDelayedDestroy()) {
    if (!GRoot->IsDestroyed()) GRoot->ConditionalDestroy();
    GRoot = nullptr;
  }

  // collect all orphans and dead widgets
  DyingWidgets.resetNoDtor();
  for (auto &&it : AllWidgets.first()) {
    VWidget *w = it.getKey();
    if (w == GRoot) continue;
    //if (!w->ParentWidget && !w->IsGoingToDie()) w->MarkDead();
    if (!w->IsDestroyed() && w->IsDelayedDestroy()) DyingWidgets.append(w);
  }

  if (DyingWidgets.length() == 0) return;
  //GCon->Logf(NAME_Debug, "=== found %d dead widgets ===", DyingWidgets.length());

  // destroy all dead widgets
  for (auto &&it : DyingWidgets) {
    VWidget *w = it;
    if (!AllWidgets.has(w)) {
      //for (auto &&ww : AllWidgets.first()) GCon->Logf("  %p : %p : %d", w, ww.getKey(), (int)(w == ww.getKey()));
      // already destroyed
      //GCon->Logf(NAME_Debug, "(0)skipping already destroyed widget %s:%u:%p : %d", w->GetClass()->GetName(), w->GetUniqueId(), (void *)w, (AllWidgets.get(w) ? 1 : 0));
      continue;
    }
    if (w->IsDestroyed()) {
      //GCon->Logf(NAME_Debug, "(1)skipping already destroyed widget %s:%u:%p", w->GetClass()->GetName(), w->GetUniqueId(), (void *)w);
      continue;
    }
    vassert(it->IsDelayedDestroy());
    for (;;) {
      if (!w->ParentWidget) break;
      if (w->ParentWidget == GRoot) break;
      if (w->ParentWidget->IsDestroyed()) break;
      if (!w->ParentWidget->IsDelayedDestroy()) break;
      w = w->ParentWidget;
    }
    vassert(!w->IsDestroyed());
    vassert(w->IsDelayedDestroy());
    //GCon->Logf(NAME_Debug, "going to destroy widget %s:%u:%p", w->GetClass()->GetName(), w->GetUniqueId(), (void *)w);
    w->ConditionalDestroy();
  }
}


//==========================================================================
//
//  VWidget::MarkChildrenDead
//
//==========================================================================
void VWidget::MarkChildrenDead () {
  for (auto &&w : fromFirstChild()) w->MarkDead();
}


//==========================================================================
//
//  VWidget::MarkDead
//
//==========================================================================
void VWidget::MarkDead () {
  MarkChildrenDead();
  if (ParentWidget && IsChildAdded()) ParentWidget->RemoveChild(this);
  SetDelayedDestroy();
}


//==========================================================================
//
//  VWidget::Close
//
//  don't delete it, GC will do
//
//==========================================================================
void VWidget::Close () {
  MarkDead();
}


//==========================================================================
//
//  VWidget::Init
//
//==========================================================================
void VWidget::Init (VWidget *AParent) {
  // set default values
  SetFont(SmallFont);
  SetTextAlign(hleft, vtop);
  ParentWidget = AParent;
  if (ParentWidget) ParentWidget->AddChild(this);
  ClipTree();
  OnCreate();
}


//==========================================================================
//
//  VWidget::Destroy
//
//==========================================================================
void VWidget::Destroy () {
  //GCon->Logf(NAME_Debug, "destroying widget %s:%u:%p", GetClass()->GetName(), GetUniqueId(), (void *)this);
  AllWidgets.del(this);
  MarkChildrenDead();
  if (ParentWidget && IsChildAdded()) ParentWidget->RemoveChild(this);
  OnDestroy();
  DestroyAllChildren();
  Super::Destroy();
}


//==========================================================================
//
//  VWidget::AddChild
//
//==========================================================================
void VWidget::AddChild (VWidget *NewChild) {
  if (!NewChild || NewChild->IsGoingToDie()) return;
  if (IsGoingToDie()) return;
  if (NewChild == this) Sys_Error("VWidget::AddChild: trying to add `this` to `this`");
  if (!NewChild->ParentWidget) Sys_Error("VWidget::AddChild: trying to adopt a child without any official papers");
  if (NewChild->ParentWidget != this) Sys_Error("VWidget::AddChild: trying to adopt an alien child");
  if (NewChild->IsChildAdded()) { GCon->Log(NAME_Error, "VWidget::AddChild: adopting already adopted child"); return; }

  // link as last widget
  NewChild->PrevWidget = LastChildWidget;
  NewChild->NextWidget = nullptr;
  if (LastChildWidget) LastChildWidget->NextWidget = NewChild; else FirstChildWidget = NewChild;
  LastChildWidget = NewChild;

  // raise it (this fixes normal widgets when there are "on top" ones)
  NewChild->Raise();

  //GCon->Logf(NAME_Debug, "NewChild:%s:%u", NewChild->GetClass()->GetName(), NewChild->GetUniqueId());

  OnChildAdded(NewChild);
  if (NewChild->CanBeFocused()) {
    if (!CurrentFocusChild || NewChild->IsCloseOnBlurFlag()) SetCurrentFocusChild(NewChild);
  }
  if (NewChild->IsCloseOnBlurFlag() && CurrentFocusChild != NewChild) NewChild->Close(); // just in case
}


//==========================================================================
//
//  VWidget::RemoveChild
//
//==========================================================================
void VWidget::RemoveChild (VWidget *InChild) {
  if (!InChild) return;
  if (!InChild->IsChildAdded()) { /*GCon->Log(NAME_Error, "VWidget::RemoveChild: trying to orphan already orphaned child");*/ return; }
  if (InChild->ParentWidget != this) Sys_Error("VWidget::RemoveChild: trying to orphan an alien child");
  //if (InChild->IsCloseOnBlurFlag()) GCon->Logf(NAME_Debug, "VWidget::RemoveChild: removing %u (focus=%u)", InChild->GetUniqueId(), (CurrentFocusChild ? CurrentFocusChild->GetUniqueId() : 0));
  //if (InChild->IsCloseOnBlurFlag()) GCon->Logf(NAME_Debug, "VWidget::RemoveChild: removed %u (focus=%u)", InChild->GetUniqueId(), (CurrentFocusChild ? CurrentFocusChild->GetUniqueId() : 0));
  // remove "close on blur", because we're going to close it anyway
  //const bool oldCOB = InChild->IsCloseOnBlurFlag();
  InChild->SetCloseOnBlurFlag(false);
  // remove from parent list
  if (InChild->PrevWidget) {
    InChild->PrevWidget->NextWidget = InChild->NextWidget;
  } else {
    FirstChildWidget = InChild->NextWidget;
  }
  if (InChild->NextWidget) {
    InChild->NextWidget->PrevWidget = InChild->PrevWidget;
  } else {
    LastChildWidget = InChild->PrevWidget;
  }
  // fix focus
  if (CurrentFocusChild == InChild) FindNewFocus();
  //GCon->Logf(NAME_Debug, "%u: OnClose(); parent=%u", InChild->GetUniqueId(), GetUniqueId());
  InChild->OnClose();
  // mark as removed
  InChild->PrevWidget = nullptr;
  InChild->NextWidget = nullptr;
  InChild->ParentWidget = nullptr;
  OnChildRemoved(InChild);
}


//==========================================================================
//
//  VWidget::DestroyAllChildren
//
//==========================================================================
void VWidget::DestroyAllChildren () {
  while (FirstChildWidget) FirstChildWidget->ConditionalDestroy();
}


//==========================================================================
//
//  VWidget::GetRootWidget
//
//==========================================================================
VRootWidget *VWidget::GetRootWidget () noexcept {
  VWidget *W = this;
  while (W->ParentWidget) W = W->ParentWidget;
  return (VRootWidget *)W;
}


//==========================================================================
//
//  VWidget::FindFirstOnTopChild
//
//==========================================================================
VWidget *VWidget::FindFirstOnTopChild () noexcept {
  // start from the last widget, because it is likely to be on top
  for (auto &&w : fromLastChild()) {
    if (!w->IsOnTopFlag()) return w->NextWidget;
  }
  return nullptr;
}


//==========================================================================
//
//  VWidget::FindLastNormalChild
//
//==========================================================================
VWidget *VWidget::FindLastNormalChild () noexcept {
  // start from the last widget, because it is likely to be on top
  for (auto &&w : fromLastChild()) {
    if (!w->IsOnTopFlag()) return w;
  }
  return nullptr;
}


//==========================================================================
//
//  VWidget::UnlinkFromParent
//
//==========================================================================
void VWidget::UnlinkFromParent () noexcept {
  if (!ParentWidget || !IsChildAdded()) return;
  // unlink from current location
  if (PrevWidget) PrevWidget->NextWidget = NextWidget; else ParentWidget->FirstChildWidget = NextWidget;
  if (NextWidget) NextWidget->PrevWidget = PrevWidget; else ParentWidget->LastChildWidget = PrevWidget;
  PrevWidget = NextWidget = nullptr;
}


//==========================================================================
//
//  VWidget::LinkToParentBefore
//
//  if `w` is null, link as first
//
//==========================================================================
void VWidget::LinkToParentBefore (VWidget *w) noexcept {
  if (!ParentWidget || w == this) return;
  if (!w) {
    if (ParentWidget->FirstChildWidget == this) return; // already there
    // unlink from current location
    UnlinkFromParent();
    vassert(!PrevWidget);
    vassert(!NextWidget);
    // link to bottom (i.e. as first child)
    PrevWidget = nullptr;
    NextWidget = ParentWidget->FirstChildWidget;
    ParentWidget->FirstChildWidget->PrevWidget = this;
    ParentWidget->FirstChildWidget = this;
  } else {
    if (w->PrevWidget == this) return; // already there
    // unlink from current location
    UnlinkFromParent();
    vassert(!PrevWidget);
    vassert(!NextWidget);
    // link before `w`
    PrevWidget = w->PrevWidget;
    NextWidget = w;
    w->PrevWidget = this;
    if (PrevWidget) PrevWidget->NextWidget = this; else ParentWidget->FirstChildWidget = this;
  }
}


//==========================================================================
//
//  VWidget::LinkToParentAfter
//
//  if `w` is null, link as last
//
//==========================================================================
void VWidget::LinkToParentAfter (VWidget *w) noexcept {
  if (!ParentWidget || w == this) return;
  if (!w) {
    //GCon->Logf(NAME_Debug, "LinkToParentAfter:(nullptr):%u (%d)", GetUniqueId(), (int)(ParentWidget->LastChildWidget == this));
    if (ParentWidget->LastChildWidget == this) return; // already there
    // unlink from current location
    UnlinkFromParent();
    vassert(!PrevWidget);
    vassert(!NextWidget);
    // link to top (i.e. as last child)
    PrevWidget = ParentWidget->LastChildWidget;
    NextWidget = nullptr;
    ParentWidget->LastChildWidget->NextWidget = this;
    ParentWidget->LastChildWidget = this;
  } else {
    if (w->NextWidget == this) return; // already there
    // unlink from current location
    UnlinkFromParent();
    vassert(!PrevWidget);
    vassert(!NextWidget);
    // link after `w`
    PrevWidget = w;
    NextWidget = w->NextWidget;
    w->NextWidget = this;
    if (NextWidget) NextWidget->PrevWidget = this; else ParentWidget->LastChildWidget = this;
  }
}


//==========================================================================
//
//  VWidget::Lower
//
//==========================================================================
void VWidget::Lower () {
  if (!ParentWidget) { /*GCon->Log(NAME_Error, "Can't lower root window");*/ return; }
  LinkToParentBefore(IsOnTopFlag() ? ParentWidget->FindFirstOnTopChild() : nullptr);
}


//==========================================================================
//
//  VWidget::Raise
//
//==========================================================================
void VWidget::Raise () {
  if (!ParentWidget) { /*GCon->Log(NAME_Error, "Can't raise root window");*/ return; }
  //GCon->Logf(NAME_Debug, "raising %u (ontop=%d)", GetUniqueId(), (int)IsOnTopFlag());
  LinkToParentAfter(IsOnTopFlag() ? nullptr : ParentWidget->FindLastNormalChild());
}


//==========================================================================
//
//  VWidget::MoveBefore
//
//==========================================================================
void VWidget::MoveBefore (VWidget *Other) {
  if (!Other || Other == this) return;
  if (ParentWidget != Other->ParentWidget) { GCon->Log(NAME_Error, "Must have the same parent widget"); return; }

  if (IsOnTopFlag() != Other->IsOnTopFlag()) {
    if (IsOnTopFlag()) {
      // self is "on top", other is "normal": just lower it
      Lower();
    } else {
      // other is "on top", self is "normal": just raise it
      Raise();
    }
    return;
  }

  // link in new position
  LinkToParentBefore(Other);
}


//==========================================================================
//
//  VWidget::MoveAfter
//
//==========================================================================
void VWidget::MoveAfter (VWidget *Other) {
  if (!Other || Other == this) return;
  if (ParentWidget != Other->ParentWidget) { GCon->Log(NAME_Error, "Must have the same parent widget"); return; }

  if (IsOnTopFlag() != Other->IsOnTopFlag()) {
    if (IsOnTopFlag()) {
      // self is "on top", other is "normal": just lower it
      Lower();
    } else {
      // other is "on top", self is "normal": just raise it
      Raise();
    }
    return;
  }

  // link in new position
  LinkToParentAfter(Other);
}


//==========================================================================
//
//  VWidget::SetOnTop
//
//==========================================================================
void VWidget::SetOnTop (bool v) noexcept {
  if (v == IsOnTopFlag()) return;
  if (v) WidgetFlags |= WF_OnTop; else WidgetFlags &= ~WF_OnTop;
  if (!IsChildAdded()) return; // nothing to do yet
  Raise(); // this fixes position
}


//==========================================================================
//
//  VWidget::SetCloseOnBlur
//
//==========================================================================
void VWidget::SetCloseOnBlur (bool v) noexcept {
  if (v == IsCloseOnBlurFlag()) return;
  if (v) WidgetFlags |= WF_CloseOnBlur; else WidgetFlags &= ~WF_CloseOnBlur;
  if (!v || !IsChildAdded()) return; // nothing to do
  // if "close on blur" and not focused, close it
  if (v && !IsFocused()) Close();
}


//==========================================================================
//
//  VWidget::ClipTree
//
//==========================================================================
void VWidget::ClipTree () noexcept {
  // set up clipping rectangle
  if (ParentWidget) {
    // clipping rectangle is relative to the parent widget
    ClipRect.OriginX = ParentWidget->ClipRect.OriginX+ParentWidget->ClipRect.ScaleX*(PosX+ParentWidget->OfsX);
    ClipRect.OriginY = ParentWidget->ClipRect.OriginY+ParentWidget->ClipRect.ScaleY*(PosY+ParentWidget->OfsY);
    ClipRect.ScaleX = ParentWidget->ClipRect.ScaleX*SizeScaleX;
    ClipRect.ScaleY = ParentWidget->ClipRect.ScaleY*SizeScaleY;
    ClipRect.ClipX1 = ClipRect.OriginX;
    ClipRect.ClipY1 = ClipRect.OriginY;
    ClipRect.ClipX2 = ClipRect.OriginX+ClipRect.ScaleX*SizeWidth;
    ClipRect.ClipY2 = ClipRect.OriginY+ClipRect.ScaleY*SizeHeight;

    // clip against the parent widget's clipping rectangle
    if (ClipRect.ClipX1 < ParentWidget->ClipRect.ClipX1) ClipRect.ClipX1 = ParentWidget->ClipRect.ClipX1;
    if (ClipRect.ClipY1 < ParentWidget->ClipRect.ClipY1) ClipRect.ClipY1 = ParentWidget->ClipRect.ClipY1;
    if (ClipRect.ClipX2 > ParentWidget->ClipRect.ClipX2) ClipRect.ClipX2 = ParentWidget->ClipRect.ClipX2;
    if (ClipRect.ClipY2 > ParentWidget->ClipRect.ClipY2) ClipRect.ClipY2 = ParentWidget->ClipRect.ClipY2;
  } else {
    // this is the root widget
    ClipRect.OriginX = PosX;
    ClipRect.OriginY = PosY;
    ClipRect.ScaleX = SizeScaleX;
    ClipRect.ScaleY = SizeScaleY;
    ClipRect.ClipX1 = ClipRect.OriginX;
    ClipRect.ClipY1 = ClipRect.OriginY;
    ClipRect.ClipX2 = ClipRect.OriginX+ClipRect.ScaleX*SizeWidth;
    ClipRect.ClipY2 = ClipRect.OriginY+ClipRect.ScaleY*SizeHeight;
  }

  // set up clipping rectangles in child widgets
  for (auto &&W : fromFirstChild()) W->ClipTree();
}


//==========================================================================
//
//  VWidget::SetConfiguration
//
//  returns `true` if something was changed
//
//==========================================================================
bool VWidget::SetConfiguration (int NewX, int NewY, int NewWidth, int HewHeight, float NewScaleX, float NewScaleY, bool Forced) {
  if (!Forced && PosX == NewX && PosY == NewY &&
      SizeWidth == NewWidth && SizeHeight == HewHeight &&
      SizeScaleX == NewScaleX && SizeScaleY == NewScaleY)
  {
    return false;
  }
  PosX = NewX;
  PosY = NewY;
  //OfsX = OfsY = 0;
  SizeWidth = NewWidth;
  SizeHeight = HewHeight;
  SizeScaleX = NewScaleX;
  SizeScaleY = NewScaleY;
  ClipTree();
  OnConfigurationChanged();
  return true;
}


//==========================================================================
//
//  VWidget::SetVisibility
//
//==========================================================================
void VWidget::SetVisibility (bool NewVisibility) {
  if (IsGoingToDie()) return;
  if (IsVisibleFlag() != NewVisibility) {
    if (NewVisibility) {
      WidgetFlags |= WF_IsVisible;
      if (IsChildAdded() && ParentWidget) {
        // re-raise it, why not
        if (IsOnTopFlag()) {
          if (PrevWidget && !PrevWidget->IsOnTopFlag()) Raise();
        } else {
          if (PrevWidget && PrevWidget->IsOnTopFlag()) Raise();
        }
        if (!ParentWidget->CurrentFocusChild) ParentWidget->SetCurrentFocusChild(this);
      }
    } else {
      WidgetFlags &= ~WF_IsVisible;
      if (ParentWidget && ParentWidget->CurrentFocusChild == this) ParentWidget->FindNewFocus();
    }
    OnVisibilityChanged(NewVisibility);
  }
}


//==========================================================================
//
//  VWidget::SetEnabled
//
//==========================================================================
void VWidget::SetEnabled (bool NewEnabled) {
  if (IsGoingToDie()) return;
  if (IsEnabledFlag() != NewEnabled) {
    if (NewEnabled) {
      WidgetFlags |= WF_IsEnabled;
      if (IsChildAdded() && ParentWidget && !ParentWidget->CurrentFocusChild) ParentWidget->SetCurrentFocusChild(this);
    } else {
      WidgetFlags &= ~WF_IsEnabled;
      if (ParentWidget && ParentWidget->CurrentFocusChild == this) ParentWidget->FindNewFocus();
    }
    OnEnableChanged(NewEnabled);
  }
}


//==========================================================================
//
//  VWidget::SetFocusable
//
//==========================================================================
void VWidget::SetFocusable (bool NewFocusable) {
  if (IsGoingToDie()) return;
  if (IsFocusableFlag() != NewFocusable) {
    if (NewFocusable) {
      WidgetFlags |= WF_IsFocusable;
      if (IsChildAdded() && ParentWidget && !ParentWidget->CurrentFocusChild) ParentWidget->SetCurrentFocusChild(this);
    } else {
      WidgetFlags &= ~WF_IsFocusable;
      if (IsChildAdded() && ParentWidget && ParentWidget->CurrentFocusChild == this) ParentWidget->FindNewFocus();
    }
    OnFocusableChanged(NewFocusable);
  }
}


//==========================================================================
//
//  VWidget::SetCurrentFocusChild
//
//==========================================================================
void VWidget::SetCurrentFocusChild (VWidget *NewFocus) {
  // check if it's already focused
  if (CurrentFocusChild == NewFocus) {
    if (!CurrentFocusChild) return;
    if (!CurrentFocusChild->IsGoingToDie()) return;
    FindNewFocus();
    return;
  }

  // make sure it's visible, enabled and focusable
  if (NewFocus) {
    if (NewFocus->IsGoingToDie()) return;
    if (!NewFocus->IsChildAdded()) return; // if it is not added, get out of here
    if (!NewFocus->CanBeFocused()) return;
  }

  // if we have a focused child, send focus lost event
  VWidget *fcc = (CurrentFocusChild && CurrentFocusChild->IsCloseOnBlurFlag() ? CurrentFocusChild : nullptr);
  if (CurrentFocusChild) CurrentFocusChild->OnFocusLost();

  if (NewFocus) {
    // make it the current focus
    if (!NewFocus->IsGoingToDie()) {
      CurrentFocusChild = NewFocus;
      CurrentFocusChild->OnFocusReceived();
    } else {
      FindNewFocus();
    }
  } else {
    vassert(!NewFocus);
    CurrentFocusChild = nullptr;
  }

  if (fcc && fcc != CurrentFocusChild) fcc->Close();
}


//==========================================================================
//
//  VWidget::FindNewFocus
//
//==========================================================================
void VWidget::FindNewFocus () {
  if (CurrentFocusChild) {
    for (auto &&W : WidgetIterator(CurrentFocusChild->NextWidget, true)) {
      if (W->CanBeFocused()) {
        SetCurrentFocusChild(W);
        return;
      }
    }

    for (auto &&W : WidgetIterator(CurrentFocusChild->PrevWidget, false)) {
      if (W->CanBeFocused()) {
        SetCurrentFocusChild(W);
        return;
      }
    }
  } else {
    for (auto &&W : fromFirstChild()) {
      if (W->CanBeFocused()) {
        SetCurrentFocusChild(W);
        return;
      }
    }
  }

  SetCurrentFocusChild(nullptr);
}


//==========================================================================
//
//  VWidget::GetWidgetAt
//
//==========================================================================
VWidget *VWidget::GetWidgetAt (float X, float Y, bool allowDisabled) noexcept {
  if (!allowDisabled && !IsEnabled(false)) return nullptr; // don't perform recursive check
  for (auto &&W : fromLastChild()) {
    if (!IsVisibleFlag()) continue;
    if (W->IsGoingToDie()) continue;
    if (X >= W->ClipRect.ClipX1 && X < W->ClipRect.ClipX2 &&
        Y >= W->ClipRect.ClipY1 && Y < W->ClipRect.ClipY2)
    {
      // this can return `nullptr` for disabled widgets
      VWidget *res = W->GetWidgetAt(X, Y, allowDisabled);
      if (res) return res;
    }
  }
  return this;
}


//==========================================================================
//
//  VWidget::SetupScissor
//
//==========================================================================
bool VWidget::SetupScissor () {
  if (IsUseScissor()) {
    Drawer->SetScissor((int)ClipRect.ClipX1, (int)ClipRect.ClipY1, (int)(ClipRect.ClipX2-ClipRect.ClipX1+1), (int)(ClipRect.ClipY2-ClipRect.ClipY1+1));
    Drawer->SetScissorEnabled(true);
    return true;
  }
  if (!ParentWidget) return false;
  return ParentWidget->SetupScissor();
}


//==========================================================================
//
//  VWidget::DrawTree
//
//==========================================================================
void VWidget::DrawTree () {
  if (IsGoingToDie()) return;
  if (!IsVisibleFlag() || !ClipRect.HasArea()) return; // not visible or clipped away

  // main draw event for this widget
  bool scissorSet = SetupScissor();
  //Drawer->SetScissor(0, 0, 10, 10);
  //Drawer->SetScissorEnabled(true);

  OnDraw();

  // draw chid widgets
  for (auto &&c : fromFirstChild()) {
    if (c->IsGoingToDie()) continue;
    if (SetupScissor()) scissorSet = true;
    c->DrawTree();
  }

  // do any drawing after child wigets have been drawn
  if (SetupScissor()) scissorSet = true;
  OnPostDraw();

  if (scissorSet) Drawer->SetScissorEnabled(false);
  Drawer->SetScissorEnabled(false);
}


//==========================================================================
//
//  VWidget::TickTree
//
//==========================================================================
void VWidget::TickTree (float DeltaTime) {
  if (IsGoingToDie()) return;
  if (IsTickEnabledFlag()) Tick(DeltaTime);
  for (auto &&c : fromFirstChild()) {
    if (c->IsGoingToDie()) continue;
    c->TickTree(DeltaTime);
  }
}


//==========================================================================
//
//  VWidget::ToDrawerCoords
//
//==========================================================================
void VWidget::ToDrawerCoords (float &x, float &y) const noexcept {
  x = ClipRect.ScaleX*x+ClipRect.OriginX;
  y = ClipRect.ScaleY*y+ClipRect.OriginY;
}


//==========================================================================
//
//  VWidget::ToDrawerCoords
//
//==========================================================================
void VWidget::ToDrawerCoords (int &x, int &y) const noexcept {
  x = ClipRect.ScaleX*x+ClipRect.OriginX;
  y = ClipRect.ScaleY*y+ClipRect.OriginY;
}


//==========================================================================
//
//  VWidget::ToDrawerX
//
//==========================================================================
int VWidget::ToDrawerX (int x) const noexcept {
  return (int)(ClipRect.ScaleX*x+ClipRect.OriginX);
}


//==========================================================================
//
//  VWidget::ToDrawerY
//
//==========================================================================
int VWidget::ToDrawerY (int y) const noexcept {
  return (int)(ClipRect.ScaleY*y+ClipRect.OriginY);
}


//==========================================================================
//
//  VWidget::FromDrawerX
//
//==========================================================================
int VWidget::FromDrawerX (int x) const noexcept {
  return (int)((x-ClipRect.OriginX)/ClipRect.ScaleX);
}


//==========================================================================
//
//  VWidget::FromDrawerY
//
//==========================================================================
int VWidget::FromDrawerY (int y) const noexcept {
  return (int)((y-ClipRect.OriginY)/ClipRect.ScaleY);
}


//==========================================================================
//
//  VWidget::TransferAndClipRect
//
//==========================================================================
bool VWidget::TransferAndClipRect (float &X1, float &Y1, float &X2, float &Y2,
  float &S1, float &T1, float &S2, float &T2) const noexcept
{
  X1 = ClipRect.ScaleX*X1+ClipRect.OriginX;
  Y1 = ClipRect.ScaleY*Y1+ClipRect.OriginY;
  X2 = ClipRect.ScaleX*X2+ClipRect.OriginX;
  Y2 = ClipRect.ScaleY*Y2+ClipRect.OriginY;
  if (X1 < ClipRect.ClipX1) {
    S1 = S1+(X1-ClipRect.ClipX1)/(X1-X2)*(S2-S1);
    X1 = ClipRect.ClipX1;
  }
  if (X2 > ClipRect.ClipX2) {
    S2 = S2+(X2-ClipRect.ClipX2)/(X1-X2)*(S2-S1);
    X2 = ClipRect.ClipX2;
  }
  if (Y1 < ClipRect.ClipY1) {
    T1 = T1+(Y1-ClipRect.ClipY1)/(Y1-Y2)*(T2-T1);
    Y1 = ClipRect.ClipY1;
  }
  if (Y2 > ClipRect.ClipY2) {
    T2 = T2+(Y2-ClipRect.ClipY2)/(Y1-Y2)*(T2-T1);
    Y2 = ClipRect.ClipY2;
  }
  return (X1 < X2 && Y1 < Y2);
}


//==========================================================================
//
//  VWidget::TransferAndClipRect
//
//==========================================================================
bool VWidget::TransferAndClipRect (float &X1, float &Y1, float &X2, float &Y2) const noexcept {
  X1 = ClipRect.ScaleX*X1+ClipRect.OriginX;
  Y1 = ClipRect.ScaleY*Y1+ClipRect.OriginY;
  X2 = ClipRect.ScaleX*X2+ClipRect.OriginX;
  Y2 = ClipRect.ScaleY*Y2+ClipRect.OriginY;
  if (X1 < ClipRect.ClipX1) X1 = ClipRect.ClipX1;
  if (X2 > ClipRect.ClipX2) X2 = ClipRect.ClipX2;
  if (Y1 < ClipRect.ClipY1) Y1 = ClipRect.ClipY1;
  if (Y2 > ClipRect.ClipY2) Y2 = ClipRect.ClipY2;
  return (X1 < X2 && Y1 < Y2);
}


//==========================================================================
//
//  VWidget::TransferAndClipLine
//
//  Automap clipping of lines.
//
//  Based on Cohen-Sutherland clipping algorithm but with a slightly faster
//  reject and precalculated slopes. If the speed is needed, use a hash
//  algorithm to handle the common cases.
//
//==========================================================================
#define DOOUTCODE(oc,mx,my) \
  (oc) = 0; \
       if ((my) < 0.0f) (oc) |= TOP; \
  else if ((my) >= f_h) (oc) |= BOTTOM; \
  /* fuck you, gshitcc */ \
       if ((mx) < 0.0f) (oc) |= LEFT; \
  else if ((mx) >= f_w) (oc) |= RIGHT;

bool VWidget::TransferAndClipLine (float &X1, float &Y1, float &X2, float &Y2) const noexcept {
  enum {
    LEFT   = 1u,
    RIGHT  = 2u,
    BOTTOM = 4u,
    TOP    = 8u,
  };

  X1 = ClipRect.ScaleX*X1+ClipRect.OriginX;
  Y1 = ClipRect.ScaleY*Y1+ClipRect.OriginY;
  X2 = ClipRect.ScaleX*X2+ClipRect.OriginX;
  Y2 = ClipRect.ScaleY*Y2+ClipRect.OriginY;

  unsigned outcode1 = 0u;
  unsigned outcode2 = 0u;

  // do trivial rejects and outcodes
  if (Y1 > ClipRect.ClipY2) outcode1 = TOP; else if (Y1 < ClipRect.ClipY1) outcode1 = BOTTOM;
  if (Y2 > ClipRect.ClipY2) outcode2 = TOP; else if (Y2 < ClipRect.ClipY1) outcode2 = BOTTOM;
  if (outcode1&outcode2) return false; // trivially outside

  if (X1 < ClipRect.ClipX1) outcode1 |= LEFT; else if (X1 > ClipRect.ClipX2) outcode1 |= RIGHT;
  if (X2 < ClipRect.ClipX1) outcode2 |= LEFT; else if (X2 > ClipRect.ClipX2) outcode2 |= RIGHT;
  if (outcode1&outcode2) return false; // trivially outside

  // transform to 0-based coords
  X1 -= ClipRect.ClipX1;
  Y1 -= ClipRect.ClipY1;
  X2 -= ClipRect.ClipX1;
  Y2 -= ClipRect.ClipY1;

  const float f_w = ClipRect.ClipX2-ClipRect.ClipX1;
  const float f_h = ClipRect.ClipY2-ClipRect.ClipY1;

  DOOUTCODE(outcode1, X1, Y1);
  DOOUTCODE(outcode2, X2, Y2);

  if (outcode1&outcode2) return false;

  float tmpx = 0.0f, tmpy = 0.0f;
  while (outcode1|outcode2) {
    // may be partially inside box
    // find an outside point
    const unsigned outside = (outcode1 ?: outcode2);

    // clip to each side
    if (outside&TOP) {
      const float dy = Y1-Y2;
      const float dx = X2-X1;
      tmpx = X1+(dx*(Y1))/dy;
      tmpy = 0.0f;
    } else if (outside&BOTTOM) {
      const float dy = Y1-Y2;
      const float dx = X2-X1;
      tmpx = X1+(dx*(Y1-f_h))/dy;
      tmpy = f_h-1.0f;
    } else if (outside&RIGHT) {
      const float dy = Y2-Y1;
      const float dx = X2-X1;
      tmpy = Y1+(dy*(f_w-1-X1))/dx;
      tmpx = f_w-1.0f;
    } else if (outside&LEFT) {
      const float dy = Y2-Y1;
      const float dx = X2-X1;
      tmpy = Y1+(dy*(-X1))/dx;
      tmpx = 0.0f;
    }

    if (outside == outcode1) {
      X1 = tmpx;
      Y1 = tmpy;
      DOOUTCODE(outcode1, X1, Y1);
    } else {
      X2 = tmpx;
      Y2 = tmpy;
      DOOUTCODE(outcode2, X2, Y2);
    }

    if (outcode1&outcode2) return false; // trivially outside
  }

  // transform back to normal coords
  X1 += ClipRect.ClipX1;
  Y1 += ClipRect.ClipY1;
  X2 += ClipRect.ClipX1;
  Y2 += ClipRect.ClipY1;

  return true;
}
#undef DOOUTCODE


//==========================================================================
//
//  VWidget::DrawPic
//
//==========================================================================
void VWidget::DrawPic (int X, int Y, int Handle, float Alpha, int Trans) {
  DrawPic(X, Y, GTextureManager(Handle), Alpha, Trans);
}


//==========================================================================
//
//  VWidget::DrawPic
//
//==========================================================================
void VWidget::DrawPicScaled (int X, int Y, int Handle, float scaleX, float scaleY, float Alpha, int Trans) {
  DrawPicScaled(X, Y, GTextureManager(Handle), scaleX, scaleY, Alpha, Trans);
}


//==========================================================================
//
//  VWidget::DrawPic
//
//==========================================================================
void VWidget::DrawPicScaled (int X, int Y, VTexture *Tex, float scaleX, float scaleY, float Alpha, int Trans) {
  if (!Tex || Alpha < 0.004f || Tex->Type == TEXTYPE_Null) return;

  X -= (int)((float)Tex->GetScaledSOffsetI()*scaleX);
  Y -= (int)((float)Tex->GetScaledTOffsetI()*scaleY);
  float X1 = X;
  float Y1 = Y;
  float X2 = (int)(X+Tex->GetScaledWidthI()*scaleX);
  float Y2 = (int)(Y+Tex->GetScaledHeightI()*scaleY);
  float S1 = 0;
  float T1 = 0;
  float S2 = Tex->GetWidth();
  float T2 = Tex->GetHeight();
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    Drawer->DrawPic(X1, Y1, X2, Y2, S1, T1, S2, T2, Tex, R_GetCachedTranslation(Trans, nullptr), Alpha);
  }
}


//==========================================================================
//
//  VWidget::DrawPicScaledIgnoreOffset
//
//==========================================================================
void VWidget::DrawPicScaledIgnoreOffset (int X, int Y, int Handle, float scaleX, float scaleY, float Alpha, int Trans) {
  if (Alpha < 0.004f) return;
  VTexture *Tex = GTextureManager(Handle);
  if (!Tex || Tex->Type == TEXTYPE_Null) return;

  float X1 = X;
  float Y1 = Y;
  float X2 = (int)(X+Tex->GetScaledWidthI()*scaleX);
  float Y2 = (int)(Y+Tex->GetScaledHeightI()*scaleY);
  float S1 = 0;
  float T1 = 0;
  float S2 = Tex->GetWidth();
  float T2 = Tex->GetHeight();
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    Drawer->DrawPic(X1, Y1, X2, Y2, S1, T1, S2, T2, Tex, R_GetCachedTranslation(Trans, nullptr), Alpha);
  }
}


//==========================================================================
//
//  VWidget::DrawPic
//
//==========================================================================
void VWidget::DrawPic (int X, int Y, VTexture *Tex, float Alpha, int Trans) {
  if (!Tex || Alpha < 0.004f || Tex->Type == TEXTYPE_Null) return;

  X -= Tex->GetScaledSOffsetI();
  Y -= Tex->GetScaledTOffsetI();
  float X1 = X;
  float Y1 = Y;
  float X2 = X+Tex->GetScaledWidthI();
  float Y2 = Y+Tex->GetScaledHeightI();
  float S1 = 0;
  float T1 = 0;
  float S2 = Tex->GetWidth();
  float T2 = Tex->GetHeight();
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    Drawer->DrawPic(X1, Y1, X2, Y2, S1, T1, S2, T2, Tex, R_GetCachedTranslation(Trans, nullptr), Alpha);
  }
}


//==========================================================================
//
//  VWidget::DrawPicPart
//
//==========================================================================
void VWidget::DrawPicPart (float x, float y, float pwdt, float phgt, int handle, float alpha) {
  if (handle <= 0 || alpha < 0.004f || pwdt <= 0.0f || phgt <= 0.0f || !isFiniteF(pwdt) || !isFiniteF(phgt)) return;
  VTexture *Tex = GTextureManager(handle);
  if (!Tex || Tex->Type == TEXTYPE_Null) return;
  x -= Tex->GetScaledSOffsetF();
  y -= Tex->GetScaledTOffsetF();
  float X1 = x;
  float Y1 = y;
  float X2 = x+Tex->GetScaledWidthF()*pwdt;
  float Y2 = y+Tex->GetScaledHeightF()*phgt;
  float S1 = 0;
  float T1 = 0;
  float S2 = Tex->GetWidth()*pwdt;
  float T2 = Tex->GetHeight()*phgt;
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    Drawer->DrawPic(X1, Y1, X2, Y2, S1, T1, S2, T2, Tex, nullptr, alpha);
  }
}


//==========================================================================
//
//  VWidget::DrawPicPartEx
//
//==========================================================================
void VWidget::DrawPicPartEx (float x, float y, float tx0, float ty0, float tx1, float ty1, int handle, float alpha) {
  if (handle <= 0 || alpha < 0.004f /*|| tx1 <= tx0 || ty1 <= ty0*/) return;
  VTexture *Tex = GTextureManager(handle);
  if (!Tex || Tex->Type == TEXTYPE_Null) return;
  x -= Tex->GetScaledSOffsetF();
  y -= Tex->GetScaledTOffsetF();

  const float tws = Tex->GetScaledWidthF();
  const float ths = Tex->GetScaledHeightF();
  if (tws < 1.0f || ths < 1.0f) return;

  const float tw = Tex->GetWidth();
  const float th = Tex->GetHeight();
  if (tw < 1.0f || th < 1.0f) return;

  float X1 = x+tws*tx0;
  float Y1 = y+ths*ty0;
  float X2 = x+tws*tx1;
  float Y2 = y+ths*ty1;
  float S1 = tw*tx0;
  float T1 = th*ty0;
  float S2 = tw*tx1;
  float T2 = th*ty1;
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    Drawer->DrawPic(X1, Y1, X2, Y2, S1, T1, S2, T2, Tex, nullptr, alpha);
  }
}


//==========================================================================
//
//  VWidget::DrawCharPic
//
//  if `rgbcolor` is negative, use `rgbcolor&0xffffff` as RGB
//
//==========================================================================
void VWidget::DrawCharPic (int X, int Y, VTexture *Tex, const VFont::CharRect &rect, int rgbcolor, float Alpha, bool shadowed) {
  if (!Tex || Alpha < 0.004f || Tex->Type == TEXTYPE_Null || !rect.isValid()) return;

  X -= Tex->GetScaledSOffsetI();
  Y -= Tex->GetScaledTOffsetI();
  X -= rect.xofs;
  Y -= rect.yofs;

  const float tws = Tex->GetScaledWidthF();
  const float ths = Tex->GetScaledHeightF();
  if (tws < 1.0f || ths < 1.0f) return;

  const float tw = Tex->GetWidth();
  const float th = Tex->GetHeight();
  if (tw < 1.0f || th < 1.0f) return;

  const float fwdt = (rect.x1-rect.x0)*tws;
  const float fhgt = (rect.y1-rect.y0)*ths;
  if (fwdt < 1.0f || fhgt < 1.0f) return;

  float X1 = X;
  float Y1 = Y;
  float X2 = X+fwdt;
  float Y2 = Y+fhgt;
  float S1 = tw*rect.x0;
  float T1 = th*rect.y0;
  float S2 = tw*rect.x1;
  float T2 = th*rect.y1;
  /*
  GCon->Logf(NAME_Debug,"%s:  000: x1=%g; y1=%g; x2=%g; y2=%g; s1=%g; t1=%g; s2=%g; t3=%g", *Tex->Name, X1, Y1, X2, Y2, S1, T1, S2, T2);
  GCon->Logf(NAME_Debug, "   clip: scale=(%g,%g); org=(%g,%g); clip=(%g,%g)-(%g,%g)", ClipRect.ScaleX, ClipRect.ScaleY, ClipRect.OriginX, ClipRect.OriginY,
    ClipRect.ClipX1, ClipRect.ClipY1, ClipRect.ClipX2, ClipRect.ClipY2);
  */
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    //GCon->Logf(NAME_Debug,"%s:  001: x1=%g; y1=%g; x2=%g; y2=%g; s1=%g; t1=%g; s2=%g; t3=%g", *Tex->Name, X1, Y1, X2, Y2, S1, T1, S2, T2);
    if (shadowed) Drawer->DrawPicShadow(X1, Y1, X2, Y2, S1, T1, S2, T2, Tex, 0.625f*Alpha);
    if (rgbcolor < 0) {
      Drawer->DrawPicRecolored(X1, Y1, X2, Y2, S1, T1, S2, T2, Tex, ((unsigned)rgbcolor|0xff000000u), Alpha);
    } else {
      Drawer->DrawPic(X1, Y1, X2, Y2, S1, T1, S2, T2, Tex, nullptr, Alpha);
    }
  }
}


//==========================================================================
//
//  VWidget::DrawPicRecolored
//
//  texture coords are in [0..1]
//  high byte of color is alpha
//
//==========================================================================
void VWidget::DrawPicRecolored (float x, float y, float tx0, float ty0, float tx1, float ty1,
                                vuint32 color, int handle, float scaleX, float scaleY, bool ignoreOffset)
{
  if (handle <= 0) return;
  if (!(color&0xff000000u)) return;
  if (!isFiniteF(scaleX) || scaleX <= 0.0f) return;
  if (!isFiniteF(scaleY) || scaleY <= 0.0f) return;
  VTexture *Tex = GTextureManager(handle);
  if (!Tex || Tex->Type == TEXTYPE_Null) return;

  if (!ignoreOffset) {
    x -= Tex->GetScaledSOffsetF()*scaleX;
    y -= Tex->GetScaledTOffsetF()*scaleY;
  }

  const float tws = Tex->GetScaledWidthF()*scaleX;
  const float ths = Tex->GetScaledHeightF()*scaleY;
  if (tws < 1.0f || ths < 1.0f) return;

  const float tw = Tex->GetWidth();
  const float th = Tex->GetHeight();
  if (tw < 1.0f || th < 1.0f) return;

  float X1 = x+tws*tx0;
  float Y1 = y+ths*ty0;
  float X2 = x+tws*tx1;
  float Y2 = y+ths*ty1;
  float S1 = tw*tx0;
  float T1 = th*ty0;
  float S2 = tw*tx1;
  float T2 = th*ty1;
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    const float alpha = ((color>>24)&0xffu)/255.0f;
    Drawer->DrawPicRecolored(X1, Y1, X2, Y2, S1, T1, S2, T2, Tex, (color|0xff000000u), alpha);
  }
}


//==========================================================================
//
//  VWidget::DrawPicShadow
//
//==========================================================================
void VWidget::DrawPicShadow (float x, float y, float tx0, float ty0, float tx1, float ty1,
                             int handle, float alpha, float scaleX, float scaleY, bool ignoreOffset)
{
  if (handle <= 0) return;
  if (!isFiniteF(alpha) || alpha < 0.004f) return;
  if (!isFiniteF(scaleX) || scaleX <= 0.0f) return;
  if (!isFiniteF(scaleY) || scaleY <= 0.0f) return;
  VTexture *Tex = GTextureManager(handle);
  if (!Tex || Tex->Type == TEXTYPE_Null) return;

  if (!ignoreOffset) {
    x -= Tex->GetScaledSOffsetF()*scaleX;
    y -= Tex->GetScaledTOffsetF()*scaleY;
  }

  const float tws = Tex->GetScaledWidthF()*scaleX;
  const float ths = Tex->GetScaledHeightF()*scaleY;
  if (tws < 1.0f || ths < 1.0f) return;

  const float tw = Tex->GetWidth();
  const float th = Tex->GetHeight();
  if (tw < 1.0f || th < 1.0f) return;

  float X1 = x+tws*tx0;
  float Y1 = y+ths*ty0;
  float X2 = x+tws*tx1;
  float Y2 = y+ths*ty1;
  float S1 = tw*tx0;
  float T1 = th*ty0;
  float S2 = tw*tx1;
  float T2 = th*ty1;
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    //Drawer->DrawPic(X1, Y1, X2, Y2, S1, T1, S2, T2, Tex, nullptr, alpha);
    Drawer->DrawPicShadow(X1, Y1, X2, Y2, S1, T1, S2, T2, Tex, alpha);
  }
}


//==========================================================================
//
//  VWidget::DrawShadowedPic
//
//==========================================================================
void VWidget::DrawShadowedPic (int X, int Y, int Handle, float scaleX, float scaleY, bool ignoreOffset) {
  DrawShadowedPic(X, Y, GTextureManager(Handle), scaleX, scaleY, ignoreOffset);
}


//==========================================================================
//
//  VWidget::DrawShadowedPic
//
//==========================================================================
void VWidget::DrawShadowedPic (int X, int Y, VTexture *Tex, float scaleX, float scaleY, bool ignoreOffset) {
  if (!Tex || Tex->Type == TEXTYPE_Null || scaleX <= 0.0f || scaleY <= 0.0f || !isFiniteF(scaleX) || !isFiniteF(scaleY)) return;

  float X1 = X-(ignoreOffset ? 0.0f : Tex->GetScaledSOffsetI()*scaleX)+2;
  float Y1 = Y-(ignoreOffset ? 0.0f : Tex->GetScaledTOffsetI()*scaleY)+2;
  float X2 = X1+Tex->GetScaledWidthI()*scaleX;
  float Y2 = Y1+Tex->GetScaledHeightI()*scaleY;
  float S1 = 0;
  float T1 = 0;
  float S2 = Tex->GetWidth();
  float T2 = Tex->GetHeight();

  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    Drawer->DrawPicShadow(X1, Y1, X2, Y2, S1, T1, S2, T2, Tex, 0.625f);
  }

  //DrawPic(X, Y, Tex);
  X1 = X-(ignoreOffset ? 0.0f : Tex->GetScaledSOffsetI()*scaleX);
  Y1 = Y-(ignoreOffset ? 0.0f : Tex->GetScaledTOffsetI()*scaleY);
  X2 = X1+Tex->GetScaledWidthI()*scaleX;
  Y2 = Y1+Tex->GetScaledHeightI()*scaleY;
  S1 = 0;
  T1 = 0;
  S2 = Tex->GetWidth();
  T2 = Tex->GetHeight();

  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    Drawer->DrawPic(X1, Y1, X2, Y2, S1, T1, S2, T2, Tex, R_GetCachedTranslation(0, nullptr), 1.0f);
  }
}


//==========================================================================
//
//  VWidget::FillRectWithFlat
//
//==========================================================================
void VWidget::FillRectWithFlat (int X, int Y, int Width, int Height, VName Name) {
  if (Name == NAME_None) return;
  VTexture *tex;
  if (VStr::strEquCI(*Name, "ScreenBackPic")) {
    if (screenBackTexNum < 1) return;
    tex = GTextureManager.getIgnoreAnim(screenBackTexNum);
  } else {
    tex = GTextureManager.getIgnoreAnim(GTextureManager.NumForName(Name, TEXTYPE_Flat, true));
  }
  if (!tex || tex->Type == TEXTYPE_Null) return;
  float X1 = X;
  float Y1 = Y;
  float X2 = X+Width;
  float Y2 = Y+Height;
  float S1 = 0;
  float T1 = 0;
  float S2 = Width;
  float T2 = Height;
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    Drawer->FillRectWithFlat(X1, Y1, X2, Y2, S1, T1, S2, T2, tex);
  }
}


//==========================================================================
//
//  VWidget::FillRectWithFlatHandle
//
//==========================================================================
void VWidget::FillRectWithFlatHandle (int X, int Y, int Width, int Height, int Handle) {
  if (Handle <= 0) return;
  VTexture *tex = GTextureManager.getIgnoreAnim(Handle);
  if (!tex || tex->Type == TEXTYPE_Null) return;
  float X1 = X;
  float Y1 = Y;
  float X2 = X+Width;
  float Y2 = Y+Height;
  float S1 = 0;
  float T1 = 0;
  float S2 = Width;
  float T2 = Height;
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    Drawer->FillRectWithFlat(X1, Y1, X2, Y2, S1, T1, S2, T2, tex);
  }
}


//==========================================================================
//
//  VWidget::FillRectWithFlatRepeat
//
//==========================================================================
void VWidget::FillRectWithFlatRepeat (int X, int Y, int Width, int Height, VName Name) {
  if (Name == NAME_None) return;
  VTexture *tex;
  if (VStr::strEquCI(*Name, "ScreenBackPic")) {
    if (screenBackTexNum < 1) return;
    tex = GTextureManager.getIgnoreAnim(screenBackTexNum);
  } else {
    int nn = GTextureManager.CheckNumForName(Name, TEXTYPE_Flat, true);
    tex = (nn >= 0 ? GTextureManager.getIgnoreAnim(GTextureManager.NumForName(Name, TEXTYPE_Flat, true)) : nullptr);
  }
  if (!tex) {
    // no flat: fill rect with gray color
    FillRect(X, Y, Width, Height, 0x222222, 1.0f);
    return;
  }
  if (tex->Type == TEXTYPE_Null) return;
  float X1 = X;
  float Y1 = Y;
  float X2 = X+Width;
  float Y2 = Y+Height;
  float S1 = 0;
  float T1 = 0;
  float S2 = Width;
  float T2 = Height;
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    Drawer->FillRectWithFlatRepeat(X1, Y1, X2, Y2, S1, T1, S2, T2, tex);
  }
}


//==========================================================================
//
//  VWidget::FillRectWithFlatRepeatHandle
//
//==========================================================================
void VWidget::FillRectWithFlatRepeatHandle (int X, int Y, int Width, int Height, int Handle) {
  if (Handle <= 0) {
    // no flat: fill rect with gray color
    FillRect(X, Y, Width, Height, 0x222222, 1.0f);
  }
  VTexture *tex = GTextureManager.getIgnoreAnim(Handle);
  if (!tex || tex->Type == TEXTYPE_Null) {
    // no flat: fill rect with gray color
    FillRect(X, Y, Width, Height, 0x222222, 1.0f);
    return;
  }
  float X1 = X;
  float Y1 = Y;
  float X2 = X+Width;
  float Y2 = Y+Height;
  float S1 = 0;
  float T1 = 0;
  float S2 = Width;
  float T2 = Height;
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    Drawer->FillRectWithFlatRepeat(X1, Y1, X2, Y2, S1, T1, S2, T2, tex);
  }
}


//==========================================================================
//
//  VWidget::FillRect
//
//==========================================================================
void VWidget::FillRect (int X, int Y, int Width, int Height, int color, float alpha) {
  if (alpha < 0.004f || Width < 1 || Height < 1) return;
  float X1 = X;
  float Y1 = Y;
  float X2 = X+Width;
  float Y2 = Y+Height;
  if (TransferAndClipRect(X1, Y1, X2, Y2)) {
    Drawer->FillRect(truncf(X1), truncf(Y1), truncf(X2), truncf(Y2), color, alpha);
  }
}


//==========================================================================
//
//  VWidget::FillRectF
//
//==========================================================================
void VWidget::FillRectF (float X1, float Y1, float Width, float Height, int color, float alpha) {
  if (alpha < 0.004f || Width <= 0.0f || Height <= 0.0f) return;
  float X2 = X1+Width;
  float Y2 = Y1+Height;
  if (TransferAndClipRect(X1, Y1, X2, Y2)) {
    Drawer->FillRect((int)roundf(X1), (int)roundf(Y1), (int)roundf(X2), (int)roundf(Y2), color, alpha);
  }
}


//==========================================================================
//
//  VWidget::ShadeRect
//
//==========================================================================
void VWidget::ShadeRect (int X, int Y, int Width, int Height, float Shade) {
  if (Width < 1 || Height < 1 || Shade <= 0.0f) return;
  float X1 = X;
  float Y1 = Y;
  float X2 = X+Width;
  float Y2 = Y+Height;
  if (TransferAndClipRect(X1, Y1, X2, Y2)) {
    Drawer->ShadeRect(truncf(X1), truncf(Y1), truncf(X2), truncf(Y2), Shade);
  }
}


//==========================================================================
//
//  VWidget::ShadeRectF
//
//==========================================================================
void VWidget::ShadeRectF (float X1, float Y1, float Width, float Height, float Shade) {
  if (Width <= 0.0f || Height <= 0.0f || Shade <= 0.0f) return;
  float X2 = X1+Width;
  float Y2 = Y1+Height;
  if (TransferAndClipRect(X1, Y1, X2, Y2)) {
    Drawer->ShadeRect((int)roundf(X1), (int)roundf(Y1), (int)roundf(X2), (int)roundf(Y2), Shade);
  }
}


//==========================================================================
//
//  VWidget::DrawRect
//
//==========================================================================
void VWidget::DrawRect (int X, int Y, int Width, int Height, int color, float alpha) {
  if (Width < 1 || Height < 1 || alpha < 0.004f) return;
  float X1 = X;
  float Y1 = Y;
  float X2 = X+Width;
  float Y2 = Y+Height;
  if (TransferAndClipRect(X1, Y1, X2, Y2)) {
    Drawer->DrawRect(truncf(X1), truncf(Y1), truncf(X2), truncf(Y2), color, alpha);
  }
}


//==========================================================================
//
//  VWidget::DrawRectF
//
//==========================================================================
void VWidget::DrawRectF (float X, float Y, float Width, float Height, int color, float alpha) {
  if (Width <= 0.0f || Height <= 0.0f || alpha < 0.004f) return;
  float X1 = X;
  float Y1 = Y;
  float X2 = X+Width;
  float Y2 = Y+Height;
  if (TransferAndClipRect(X1, Y1, X2, Y2)) {
    Drawer->DrawRect((int)roundf(X1), (int)roundf(Y1), (int)roundf(X2), (int)roundf(Y2), color, alpha);
  }
}


//==========================================================================
//
//  VWidget::DrawLine
//
//==========================================================================
void VWidget::DrawLine (int aX0, int aY0, int aX1, int aY1, int color, float alpha) {
  if (alpha < 0.004f) return;
  float X1 = aX0;
  float Y1 = aY0;
  float X2 = aX1;
  float Y2 = aY1;
  if (TransferAndClipLine(X1, Y1, X2, Y2)) {
    Drawer->DrawLine(truncf(X1), truncf(Y1), truncf(X2), truncf(Y2), color, alpha);
  }
}


//==========================================================================
//
//  VWidget::DrawLineF
//
//==========================================================================
void VWidget::DrawLineF (float X1, float Y1, float X2, float Y2, int color, float alpha) {
  if (alpha < 0.004f) return;
  if (TransferAndClipLine(X1, Y1, X2, Y2)) {
    Drawer->DrawLine((int)roundf(X1), (int)roundf(Y1), (int)roundf(X2), (int)roundf(Y2), color, alpha);
  }
}


//==========================================================================
//
//  VWidget::GetFont
//
//==========================================================================
VFont *VWidget::GetFont () noexcept {
  return Font;
}


//==========================================================================
//
//  VWidget::SetFont
//
//==========================================================================
void VWidget::SetFont (VFont *AFont) noexcept {
  if (!AFont) {
    AFont = VFont::GetFont(VStrCI(NAME_consolefont));
    if (!AFont) Sys_Error("VWidget::SetFont: cannot set `nullptr` font");
    //GCon->Logf(NAME_Debug, "*** '%s' ***", *AFont->GetFontName());
  }
  Font = AFont;
}


//==========================================================================
//
//  VWidget::SetFont
//
//==========================================================================
void VWidget::SetFont (VName FontName) {
  VFont *F = VFont::GetFont(VStr(FontName)); // this doesn't allocate
  if (F) {
    Font = F;
  } else {
    if (!reportedMissingFonts.put(FontName, true)) {
      GCon->Logf(NAME_Warning, "No such font '%s'", *FontName);
    }
  }
}


//==========================================================================
//
//  VWidget::SetTextAlign
//
//==========================================================================
void VWidget::SetTextAlign (halign_e NewHAlign, valign_e NewVAlign) {
  HAlign = NewHAlign;
  VAlign = NewVAlign;
}


//==========================================================================
//
//  VWidget::SetTextShadow
//
//==========================================================================
void VWidget::SetTextShadow (bool State) {
  if (State) {
    WidgetFlags |= WF_TextShadowed;
  } else {
    WidgetFlags &= ~WF_TextShadowed;
  }
}


//==========================================================================
//
//  VWidget::DrawString
//
//==========================================================================
void VWidget::DrawString (int x, int y, VStr String, int NormalColor, int BoldColor, float Alpha) {
  if (String.isEmpty() || !Font) return;

  int cx = x;
  int cy = y;
  int Kerning = Font->GetKerning();
  int Color = NormalColor;

  if (HAlign == hcenter) cx -= Font->StringWidth(String)/2;
  if (HAlign == hright) cx -= Font->StringWidth(String);

  bool oldflt = gl_pic_filtering;
  gl_pic_filtering = gl_font_filtering;
  VFont::CharRect rect;
  bool ignoreColor = Font->IsSingleTextureFont();

  for (const char *SPtr = *String; *SPtr; ) {
    int c = VStr::Utf8GetChar(SPtr);

    // check for color escape
    if (c == TEXT_COLOR_ESCAPE) {
      Color = VFont::ParseColorEscape(SPtr, NormalColor, BoldColor);
      continue;
    }

    int w;
    VTexture *Tex = Font->GetChar(c, &rect, &w, Color);
    if (Tex && rect.isValid()) {
      if (WidgetFlags&WF_TextShadowed) DrawCharPicShadowed(cx, cy, Tex, rect, (ignoreColor ? 0 : Color)); else DrawCharPic(cx, cy, Tex, rect, (ignoreColor ? 0 : Color), Alpha);
    }
    cx += w+Kerning;
  }

  gl_pic_filtering = oldflt;

  LastX = cx;
  LastY = cy;
}


//==========================================================================
//
//  VWidget::TextBounds
//
//  returns text bounds with respect to the current text align
//
//==========================================================================
void VWidget::TextBounds (int x, int y, VStr String, int *x0, int *y0, int *width, int *height, bool trimTrailNL) {
  if (x0 || y0) {
    if (x0) {
      int cx = x;
      if (HAlign == hcenter) cx -= Font->StringWidth(String)/2;
      if (HAlign == hright) cx -= Font->StringWidth(String);
      *x0 = cx;
    }

    if (y0) {
      int cy = y;
      if (VAlign == vcenter) cy -= Font->TextHeight(String)/2;
      if (VAlign == vbottom) cy -= Font->TextHeight(String);
      *y0 = cy;
    }
  }

  if (width || height) {
    if (trimTrailNL) {
      int count = 0;
      for (int pos = String.length()-1; pos >= 0 && String[pos] == '\n'; --pos) ++count;
      String.chopRight(count);
    }

    if (width) *width = Font->TextWidth(String);
    if (height) *height = Font->TextHeight(String);
  }
}


//==========================================================================
//
//  VWidget::DrawText
//
//==========================================================================
void VWidget::DrawText (int x, int y, VStr String, int NormalColor, int BoldColor, float Alpha) {
  int cx = x;
  int cy = y;

  if (VAlign == vcenter) cy -= Font->TextHeight(String)/2;
  if (VAlign == vbottom) cy -= Font->TextHeight(String);

  // need this for correct cursor position with empty strings
  LastX = cx;
  LastY = cy;

  const int len = String.length();
  int start = 0;
  while (start < len) {
    int end = start;
    while (end < len && String[end] != '\n') ++end;
    if (start < end) {
      VStr cs(String, start, end-start);
      DrawString(cx, cy, cs, NormalColor, BoldColor, Alpha);
    }
    cy += Font->GetHeight();
    start = end+1;
  }
}


//==========================================================================
//
//  VWidget::TextWidth
//
//==========================================================================
int VWidget::TextWidth (VStr s) {
  return (Font ? Font->TextWidth(s) : 0);
}


//==========================================================================
//
//  VWidget::StringWidth
//
//==========================================================================
int VWidget::StringWidth (VStr s) {
  return (Font ? Font->StringWidth(s) : 0);
}


//==========================================================================
//
//  VWidget::TextHeight
//
//==========================================================================
int VWidget::TextHeight (VStr s) {
  return (Font ? Font->TextHeight(s) : 0);
}


//==========================================================================
//
//  VWidget::FontHeight
//
//==========================================================================
int VWidget::FontHeight () {
  return (Font ? Font->GetHeight() : 0);
}


//==========================================================================
//
//  VWidget::CursorWidth
//
//==========================================================================
int VWidget::CursorWidth () {
  //return TextWidth("_");
  return (Font ? Font->GetCharWidth(CursorChar) : 8);
}


//==========================================================================
//
//  VWidget::DrawCursor
//
//==========================================================================
void VWidget::DrawCursor (int cursorColor) {
  DrawCursorAt(LastX, LastY, CursorChar, cursorColor);
}


//==========================================================================
//
//  VWidget::DrawCursorAt
//
//==========================================================================
void VWidget::DrawCursorAt (int x, int y, int cursorChar, int cursorColor) {
  if (Font && ((host_systime64_msec/250)&1)) {
    int w;
    bool oldflt = gl_pic_filtering;
    gl_pic_filtering = gl_font_filtering;
    VFont::CharRect rect;
    VTexture *Tex = Font->GetChar(cursorChar, &rect, &w, cursorColor/*CR_UNTRANSLATED*/);
    //DrawPic(x, y, Tex, rect);
    DrawCharPic(x, y, Tex, rect, cursorColor, 1.0f, false);
    gl_pic_filtering = oldflt;
  }
}


//==========================================================================
//
//  VWidget::BroadcastEvent
//
//  to self, then to children
//  ignores cancel/consume, will prevent event modification
//
//==========================================================================
void VWidget::BroadcastEvent (event_t *evt) {
  if (!evt || IsGoingToDie()) return;
  event_t evsaved = *evt;
  OnEvent(evt);
  *evt = evsaved;
  for (auto &&w : fromLastChild()) {
    if (w->IsGoingToDie()) continue;
    w->BroadcastEvent(evt);
    *evt = evsaved;
  }
}


//==========================================================================
//
//  TranslateCoords
//
//==========================================================================
static bool TranslateCoords (const VClipRect &ClipRect, float &x, float &y) {
  x = ClipRect.ScaleX*x+ClipRect.OriginX;
  y = ClipRect.ScaleY*y+ClipRect.OriginY;
  return
    x >= ClipRect.ClipX1 &&
    y >= ClipRect.ClipY1 &&
    x <= ClipRect.ClipX2 &&
    y <= ClipRect.ClipY2;
}


//==========================================================================
//
//  UntranslateCoords
//
//==========================================================================
static void UntranslateCoords (const VClipRect &ClipRect, float &x, float &y) {
  x = (x-ClipRect.OriginX)/ClipRect.ScaleX;
  y = (y-ClipRect.OriginY)/ClipRect.ScaleY;
}


//==========================================================================
//
//  VWidget::DrawHex
//
//==========================================================================
void VWidget::DrawHex (float x0, float y0, float w, float h, vuint32 color, float alpha) {
  if (alpha < 0.004f || w <= 0.0f || h <= 0.0f) return;
  w *= ClipRect.ScaleX;
  h *= ClipRect.ScaleY;
  TranslateCoords(ClipRect, x0, y0);
  Drawer->DrawHex(x0, y0, w, h, color, alpha);
}


//==========================================================================
//
//  VWidget::FillHex
//
//==========================================================================
void VWidget::FillHex (float x0, float y0, float w, float h, vuint32 color, float alpha) {
  if (alpha < 0.004f || w <= 0.0f || h <= 0.0f) return;
  w *= ClipRect.ScaleX;
  h *= ClipRect.ScaleY;
  TranslateCoords(ClipRect, x0, y0);
  Drawer->FillHex(x0, y0, w, h, color, alpha);
}


//==========================================================================
//
//  VWidget::ShadeHex
//
//==========================================================================
void VWidget::ShadeHex (float x0, float y0, float w, float h, float darkening) {
  if (darkening <= 0.0f || w <= 0.0f || h <= 0.0f) return;
  w *= ClipRect.ScaleX;
  h *= ClipRect.ScaleY;
  TranslateCoords(ClipRect, x0, y0);
  Drawer->ShadeHex(x0, y0, w, h, darkening);
}


//==========================================================================
//
//  VWidget::CalcHexHeight
//
//==========================================================================
float VWidget::CalcHexHeight (float h) {
  if (h <= 0.0f) return 0.0f;
  return VDrawer::CalcRealHexHeight(h*ClipRect.ScaleY)/ClipRect.ScaleY;
}


//==========================================================================
//
//  VWidget::IsPointInsideHex
//
//==========================================================================
bool VWidget::IsPointInsideHex (float x, float y, float x0, float y0, float w, float h) noexcept {
  if (w <= 0.0f || h <= 0.0f) return false;
  w *= ClipRect.ScaleX;
  h *= ClipRect.ScaleY;
  TranslateCoords(ClipRect, x0, y0);
  TranslateCoords(ClipRect, x, y);
  return VDrawer::IsPointInsideHex(x, y, x0, y0, w, h);
}


// ////////////////////////////////////////////////////////////////////////// //
struct HexHive {
public:
  int diameter;

public:
  inline HexHive (int radius) noexcept : diameter((radius < 1 ? 1 : radius)*2) {}

  inline int getMinCY () const noexcept { return -diameter; }
  inline int getMaxCY () const noexcept { return diameter; }
  inline int getCYStep () const noexcept { return 2; }

  inline int calcRowSize (const int cy) const noexcept { return diameter-abs(cy/2); }

  inline int getMinCX (const int cy) const noexcept { return -calcRowSize(cy); }
  inline int getMaxCX (const int cy) const noexcept { return calcRowSize(cy); }
  inline int getCXStep () const noexcept { return 2; }

  inline bool isValidCoordsInternal (const int cx, const int cy) const noexcept {
    if (cy < -diameter || cy > diameter) return false;
    const int rsz = calcRowSize(cy);
    return (cx >= -rsz && cx <= rsz);
  }

  inline bool isValidCoordsPublic (int x, int y) const noexcept {
    xyToInternal(x, y);
    return isValidCoordsInternal(x, y);
  }

  inline int xToPublic (const int cx, const int cy) const noexcept { return (cx+calcRowSize(cy))/2; }
  inline int yToPublic (const int /*cx*/, const int cy) const noexcept { return (cy+diameter)/2; }
  inline int yToPublic (const int cy) const noexcept { return (cy+diameter)/2; }
  inline void xyToPublic (int &cx, int &cy) const noexcept { const int sx = cx; const int sy = cy; cx = xToPublic(sx, sy); cy = yToPublic(sx, sy); }

  inline int xToInternal (const int x, const int y) const noexcept { return x*2-calcRowSize(y*2-diameter); }
  inline int yToInternal (const int /*x*/, const int y) const noexcept { return y*2-diameter; }
  inline int yToInternal (const int y) const noexcept { return y*2-diameter; }
  inline void xyToInternal (int &x, int &y) const noexcept { const int sx = x; const int sy = y; x = xToInternal(sx, sy); y = yToInternal(sx, sy); }

  inline void clampXY (int &x, int &y) const noexcept {
    // clamp y
    if (y < 0) y = 0; else if (y > diameter) y = diameter;
    // clamp x
    if (x < 0) x = 0;
    x = xToInternal(x, y);
    const int cy = yToInternal(y);
    if (x < getMinCX(cy)) x = getMinCX(cy); else if (x > getMaxCX(cy)) x = getMaxCX(cy);
    x = xToPublic(x, cy);
  }

  inline void first (int &cx, int &cy) const noexcept {
    cy = getMinCY();
    cx = getMinCX(cy);
  }

  // returns `false` when iteration is complete (and coords are undefined)
  inline bool next (int &cx, int &cy) noexcept {
    // just in case
    if (cy < getMinCY()) {
      first(cx, cy);
      return true;
    }
    if (cy > getMaxCY()) return false;
    // walk horizontally
    cx += getCYStep();
    if (cx <= getMaxCX(cy)) return true;
    // next vertical line
    cy += getCYStep();
    if (cy > getMaxCY()) return false;
    cx = getMinCX(cy);
    return true;
  }

  inline void move (int &x, int &y, int dx, int dy) {
    // move vertically
    if (dy) {
      // don't even ask me
      dy = (dy < 0 ? -1 : 1);
      y += dy;
      if (y*2 < diameter) {
        if (dy < 0) {
          x -= abs(y*2-diameter)/2%2;
        } else {
          x += 1-abs(y*2-diameter)/2%2;
        }
      } else if (y*2 > diameter) {
        if (dy < 0) {
          x += 1-abs(y*2-diameter)/2%2;
        } else {
          x -= abs(y*2-diameter)/2%2;
        }
      } else {
        x += 1;
      }
    }
    // move horizontally
    if (dx) x += (dx < 0 ? -1 : 1);
    // clamp
    clampXY(x, y);
  }
};


// ////////////////////////////////////////////////////////////////////////// //
struct HexHiveWalker {
public:
  VWidget *w;
  HexHive hive;
  float cellW, cellH;
  float scrX0, scrY0;
  float scrW, scrH, realScrH;
  int cx, cy;

public:
  HexHiveWalker (VWidget *aw, float x0, float y0, int radius, float acellW, float acellH) noexcept : w(aw), hive(radius) {
    if (acellW < 1.0f) acellW = 1.0f;
    if (acellH < 1.0f) acellH = 1.0f;
    cellW = acellW;
    cellH = acellH;
    scrW = cellW*w->GetClipRect().ScaleX;
    scrH = cellH*w->GetClipRect().ScaleY;
    // calculate real hex height
    realScrH = VDrawer::CalcRealHexHeight(scrH);
    // calculate real starting coords
    TranslateCoords(w->GetClipRect(), x0, y0);
    scrX0 = x0+scrW*radius;
    scrY0 = y0+realScrH*radius;
    // setup starting coords
    hive.first(cx, cy);
  }

  // returns widget coords
  inline float calcDimX () const noexcept { return cellW*(hive.diameter+1); }
  inline float calcDimY () const noexcept { return VDrawer::CalcRealHexHeight(cellH)*(hive.diameter+1)+cellH/3.0f*2.0f/3.0f; }

  // returns `false` when iteration is complete
  inline bool next () noexcept { return hive.next(cx, cy); }

  inline float getScreenX () const noexcept { return scrX0+(float)cx*0.5f*scrW; }
  inline float getScreenY () const noexcept { return scrY0+(float)cy*0.5f*realScrH; }

  inline float getCellScrW () const noexcept { return scrW; }
  inline float getCellScrH () const noexcept { return scrH; }

  inline int getPublicX () const noexcept { return hive.xToPublic(cx, cy); }
  inline int getPublicY () const noexcept { return hive.yToPublic(cx, cy); }

  // the following operates on "public" coords
  inline float getScreenXAt (int hpx, int hpy) const noexcept { return scrX0+(float)hive.xToInternal(hpx, hpy)*0.5f*scrW; }
  inline float getScreenYAt (int hpx, int hpy) const noexcept { return scrY0+(float)hive.yToInternal(hpx, hpy)*0.5f*realScrH; }

  inline bool isScreenPointInsideCurrentHex (float px, float py) {
    return VDrawer::IsPointInsideHex(px, py, getScreenX(), getScreenY(), getCellScrW(), getCellScrH());
  }
};


//==========================================================================
//
//  VWidget::CalcHexColorPatternDims
//
//==========================================================================
void VWidget::CalcHexColorPatternDims (float *w, float *h, int radius, float cellW, float cellH) {
  if (!w && !h) return;
  HexHiveWalker wk(this, 0, 0, radius, cellW, cellH);
  if (w) *w = wk.calcDimX();
  if (h) *h = wk.calcDimY();
}


//==========================================================================
//
//  VWidget::IsValidHexColorPatternCoords
//
//  returns `true` if the given "internal" hex pattern
//  coordinates are valid
//
//==========================================================================
bool VWidget::IsValidHexColorPatternCoords (int hpx, int hpy, int radius) {
  HexHive hive(radius);
  return hive.isValidCoordsPublic(hpx, hpy);
}


//==========================================================================
//
//  VWidget::CalcHexColorPatternCoords
//
//  calcs "internal" hex pattern coordinates,
//  returns `false` if passed coords are outside any hex
//
//==========================================================================
bool VWidget::CalcHexColorPatternCoords (int *hpx, int *hpy, float px, float py, float x0, float y0, int radius, float cellW, float cellH) {
  HexHiveWalker wk(this, x0, y0, radius, cellW, cellH);
  TranslateCoords(ClipRect, px, py);
  do {
    if (wk.isScreenPointInsideCurrentHex(px, py)) {
      if (hpx) *hpx = wk.getPublicX();
      if (hpy) *hpy = wk.getPublicY();
      return true;
    }
  } while (wk.next());
  if (hpx) *hpx = -1;
  if (hpy) *hpy = -1;
  return false;
}


//==========================================================================
//
//  VWidget::MoveHexCoords
//
//  cannot move diagonally yet
//
//==========================================================================
void VWidget::MoveHexCoords (int &hpx, int &hpy, int dx, int dy, int radius) {
  HexHive hive(radius);
  hive.move(hpx, hpy, dx, dy);
}


//==========================================================================
//
//  VWidget::CalcHexColorPatternHexCoordsAt
//
//  calcs coordinates of the individual hex;
//  returns `false` if the given "internal" hp coordinates are not valid
//  those coords can be used in `*Hex()` methods
//
//==========================================================================
bool VWidget::CalcHexColorPatternHexCoordsAt (float *hx, float *hy, int hpx, int hpy, float x0, float y0, int radius, float cellW, float cellH) {
  HexHiveWalker wk(this, x0, y0, radius, cellW, cellH);
  float x = wk.getScreenXAt(hpx, hpy);
  float y = wk.getScreenYAt(hpx, hpy);
  UntranslateCoords(ClipRect, x, y);
  if (hx) *hx = x;
  if (hy) *hy = y;
  return wk.hive.isValidCoordsPublic(hpx, hpy);
}


//==========================================================================
//
//  CalcHexHSByCoords
//
//==========================================================================
static void CalcHexHSByCoords (int x, int y, int diameter, float *h, float *s) {
  if (h) *h = AngleMod(360.0f*(0.5f-0.5f*atan2f(y, -x)/(float)(M_PI)));
  if (s) *s = clampval(sqrtf(x*x+y*y)/(float)diameter, 0.0f, 1.0f);
}


//==========================================================================
//
//  CalcHexColorByCoords
//
//==========================================================================
static vuint32 CalcHexColorByCoords (int x, int y, int diameter) {
  float h, s;
  CalcHexHSByCoords(x, y, diameter, &h, &s);
  const float v = 1.0f;
  float r, g, b;
  M_HsvToRgb(h, s, v, r, g, b);
  return PackRGBf(r, g, b);
}


//==========================================================================
//
//  VWidget::GetHexColorPatternColorAt
//
//  returns opaque color at the given "internal" hex pattern
//  coordinates (with high byte set), or 0
//
//==========================================================================
int VWidget::GetHexColorPatternColorAt (int hpx, int hpy, int radius) {
  HexHive hive(radius);
  //GCon->Logf(NAME_Debug, "hpos=(%d,%d); valid=%d; rad=%d; hx=%d; hy=%d; rsz=%d", hpx, hpy, (int)wk.isValidHexCoords(hpx, hpy), radius, wk.hpx2hx(hpx, hpy), wk.hpy2hy(hpx, hpy), wk.calcRowSize(wk.hpy2hy(hpx, hpy)));
  if (!hive.isValidCoordsPublic(hpx, hpy)) return 0;
  hive.xyToInternal(hpx, hpy);
  return CalcHexColorByCoords(hpx, hpy, hive.diameter);
}


//==========================================================================
//
//  VWidget::DrawHexColorPattern
//
//==========================================================================
void VWidget::DrawHexColorPattern (float x0, float y0, int radius, float cellW, float cellH) {
  HexHiveWalker wk(this, x0, y0, radius, cellW, cellH);
  do {
    const vuint32 clr = CalcHexColorByCoords(wk.cx, wk.cy, wk.hive.diameter);
    Drawer->FillHex(wk.getScreenX(), wk.getScreenY(), wk.getCellScrW(), wk.getCellScrH(), clr);
  } while (wk.next());
}


//==========================================================================
//
//  VWidget::FindHexColorCoords
//
//  ignores `v`
//
//==========================================================================
bool VWidget::FindHexColorCoords (int *hpx, int *hpy, int radius, float h, float s) {
  if (radius < 1) radius = 1;

  h = AngleMod(h);
  s = clampval(s, 0.0f, 1.0f);

  float fr, fg, fb;
  M_HsvToRgb(h, s, 1.0f, fr, fg, fb);

  int bestX = 0, bestY = 0;
  float bestDist = -1;
  bool directHit = false;

  int cx, cy;
  HexHive hive(radius);
  hive.first(cx, cy);

  do {
    float ch, cs;
    float cr, cg, cb;
    CalcHexHSByCoords(cx, cy, hive.diameter, &ch, &cs);
    M_HsvToRgb(ch, cs, 1.0f, cr, cg, cb);
    float dist = (fr-cr)*(fr-cr)+(fg-cg)*(fg-cg)+(fb-cb)*(fb-cb);
    if (bestDist < 0 || dist < bestDist) {
      directHit = (dist == 0);
      bestDist = dist;
      bestX = hive.xToPublic(cx, cy);
      bestY = hive.yToPublic(cx, cy);
      if (directHit) break;
    }
  } while (hive.next(cx, cy));

  if (hpx) *hpx = bestX;
  if (hpy) *hpy = bestY;
  return directHit;
}


//==========================================================================
//
//  Natives
//
//==========================================================================
IMPLEMENT_FUNCTION(VWidget, NewChild) {
  VClass *ChildClass;
  vobjGetParamSelf(ChildClass);
  RET_REF(CreateNewWidget(ChildClass, Self));
}

IMPLEMENT_FUNCTION(VWidget, Destroy) {
  vobjGetParamSelf();
  //k8: don't delete it, GC will do
  if (Self) Self->Close();
}

IMPLEMENT_FUNCTION(VWidget, DestroyAllChildren) {
  vobjGetParamSelf();
  if (Self) Self->MarkChildrenDead();
}

IMPLEMENT_FUNCTION(VWidget, GetRootWidget) {
  vobjGetParamSelf();
  RET_REF(Self ? Self->GetRootWidget() : nullptr);
}

IMPLEMENT_FUNCTION(VWidget, Lower) {
  vobjGetParamSelf();
  if (Self) Self->Lower();
}

IMPLEMENT_FUNCTION(VWidget, Raise) {
  vobjGetParamSelf();
  if (Self) Self->Raise();
}

IMPLEMENT_FUNCTION(VWidget, MoveBefore) {
  VWidget *Other;
  vobjGetParamSelf(Other);
  if (Self) Self->MoveBefore(Other);
}

IMPLEMENT_FUNCTION(VWidget, MoveAfter) {
  VWidget *Other;
  vobjGetParamSelf(Other);
  if (Self) Self->MoveAfter(Other);
}

IMPLEMENT_FUNCTION(VWidget, SetPos) {
  int NewX, NewY;
  vobjGetParamSelf(NewX, NewY);
  if (Self) Self->SetPos(NewX, NewY);
}

IMPLEMENT_FUNCTION(VWidget, SetX) {
  int NewX;
  vobjGetParamSelf(NewX);
  if (Self) Self->SetX(NewX);
}

IMPLEMENT_FUNCTION(VWidget, SetY) {
  int NewY;
  vobjGetParamSelf(NewY);
  if (Self) Self->SetY(NewY);
}

IMPLEMENT_FUNCTION(VWidget, SetOfsX) {
  int NewX;
  vobjGetParamSelf(NewX);
  if (Self) Self->SetOfsX(NewX);
}

IMPLEMENT_FUNCTION(VWidget, SetOfsY) {
  int NewY;
  vobjGetParamSelf(NewY);
  if (Self) Self->SetOfsY(NewY);
}

IMPLEMENT_FUNCTION(VWidget, SetOffset) {
  int NewX, NewY;
  vobjGetParamSelf(NewX, NewY);
  if (Self) Self->SetOffset(NewX, NewY);
}

IMPLEMENT_FUNCTION(VWidget, SetSize) {
  int NewWidth, NewHeight;
  vobjGetParamSelf(NewWidth, NewHeight);
  if (Self) Self->SetSize(NewWidth, NewHeight);
}

IMPLEMENT_FUNCTION(VWidget, SetWidth) {
  int NewWidth;
  vobjGetParamSelf(NewWidth);
  if (Self) Self->SetWidth(NewWidth);
}

IMPLEMENT_FUNCTION(VWidget, SetHeight) {
  int NewHeight;
  vobjGetParamSelf(NewHeight);
  if (Self) Self->SetHeight(NewHeight);
}

IMPLEMENT_FUNCTION(VWidget, SetScale) {
  float NewScaleX, NewScaleY;
  vobjGetParamSelf(NewScaleX, NewScaleY);
  if (Self) Self->SetScale(NewScaleX, NewScaleY);
}

IMPLEMENT_FUNCTION(VWidget, SetConfiguration) {
  int NewX, NewY, NewWidth, NewHeight;
  VOptParamFloat NewScaleX(1.0f);
  VOptParamFloat NewScaleY(1.0f);
  vobjGetParamSelf(NewX, NewY, NewWidth, NewHeight, NewScaleX, NewScaleY);
  if (Self) {
    if (!NewScaleX.specified) NewScaleX = Self->SizeScaleX;
    if (!NewScaleY.specified) NewScaleY = Self->SizeScaleY;
  }
  RET_BOOL(Self ? Self->SetConfiguration(NewX, NewY, NewWidth, NewHeight, NewScaleX, NewScaleY) : false);
}

IMPLEMENT_FUNCTION(VWidget, SetVisibility) {
  bool bNewVisibility;
  vobjGetParamSelf(bNewVisibility);
  if (Self) Self->SetVisibility(bNewVisibility);
}

IMPLEMENT_FUNCTION(VWidget, Show) {
  vobjGetParamSelf();
  if (Self) Self->Show();
}

IMPLEMENT_FUNCTION(VWidget, Hide) {
  vobjGetParamSelf();
  if (Self) Self->Hide();
}

IMPLEMENT_FUNCTION(VWidget, IsVisible) {
  VOptParamBool Recurse(true);
  vobjGetParamSelf(Recurse);
  RET_BOOL(Self ? Self->IsVisible(Recurse) : false);
}

IMPLEMENT_FUNCTION(VWidget, SetEnabled) {
  bool bNewEnabled;
  vobjGetParamSelf(bNewEnabled);
  if (Self) Self->SetEnabled(bNewEnabled);
}

IMPLEMENT_FUNCTION(VWidget, Enable) {
  vobjGetParamSelf();
  if (Self) Self->Enable();
}

IMPLEMENT_FUNCTION(VWidget, Disable) {
  vobjGetParamSelf();
  if (Self) Self->Disable();
}

IMPLEMENT_FUNCTION(VWidget, IsEnabled) {
  VOptParamBool Recurse(true);
  vobjGetParamSelf(Recurse);
  RET_BOOL(Self ? Self->IsEnabled(Recurse) : false);
}

IMPLEMENT_FUNCTION(VWidget, SetOnTop) {
  bool bNewOnTop;
  vobjGetParamSelf(bNewOnTop);
  if (Self) Self->SetOnTop(bNewOnTop);
}

IMPLEMENT_FUNCTION(VWidget, IsOnTop) {
  vobjGetParamSelf();
  RET_BOOL(Self ? Self->IsOnTop() : false);
}

IMPLEMENT_FUNCTION(VWidget, SetCloseOnBlur) {
  bool bNewCloseOnBlur;
  vobjGetParamSelf(bNewCloseOnBlur);
  if (Self) Self->SetCloseOnBlur(bNewCloseOnBlur);
}

IMPLEMENT_FUNCTION(VWidget, IsCloseOnBlur) {
  vobjGetParamSelf();
  RET_BOOL(Self ? Self->IsCloseOnBlur() : false);
}

IMPLEMENT_FUNCTION(VWidget, IsModal) {
  vobjGetParamSelf();
  RET_BOOL(Self ? Self->IsModal() : false);
}

IMPLEMENT_FUNCTION(VWidget, SetFocusable) {
  bool bNewFocusable;
  vobjGetParamSelf(bNewFocusable);
  if (Self) Self->SetFocusable(bNewFocusable);
}

IMPLEMENT_FUNCTION(VWidget, IsFocusable) {
  vobjGetParamSelf();
  RET_BOOL(Self ? Self->IsFocusable() : false);
}

IMPLEMENT_FUNCTION(VWidget, SetCurrentFocusChild) {
  VWidget *NewFocus;
  vobjGetParamSelf(NewFocus);
  if (Self) Self->SetCurrentFocusChild(NewFocus);
}

IMPLEMENT_FUNCTION(VWidget, GetCurrentFocus) {
  vobjGetParamSelf();
  RET_REF(Self ? Self->GetCurrentFocus() : nullptr);
}

IMPLEMENT_FUNCTION(VWidget, IsFocused) {
  VOptParamBool Recurse(true);
  vobjGetParamSelf(Recurse);
  RET_BOOL(Self ? Self->IsFocused(Recurse) : false);
}

IMPLEMENT_FUNCTION(VWidget, SetFocus) {
  VOptParamBool onlyInParent(false);
  vobjGetParamSelf(onlyInParent);
  if (Self) Self->SetFocus(onlyInParent);
}


//native final void DrawPicPart (float x, float y, float pwdt, float phgt, int handle, optional float alpha);
IMPLEMENT_FUNCTION(VWidget, DrawPicPart) {
  float x, y, pwdt, phgt;
  int handle;
  VOptParamFloat alpha(1.0f);
  vobjGetParamSelf(x, y, pwdt, phgt, handle, alpha);
  if (Self) Self->DrawPicPart(x, y, pwdt, phgt, handle, alpha);
}

//native final void DrawPicPartEx (float x, float y, float tx0, float ty0, float tx1, float ty1, int handle, optional float alpha);
IMPLEMENT_FUNCTION(VWidget, DrawPicPartEx) {
  float x, y, tx0, ty0, tx1, ty1;
  int handle;
  VOptParamFloat alpha(1.0f);
  vobjGetParamSelf(x, y, tx0, ty0, tx1, ty1, handle, alpha);
  if (Self) Self->DrawPicPartEx(x, y, tx0, ty0, tx1, ty1, handle, alpha);
}

//native final void DrawPicRecoloredEx (float x, float y, float tx0, float ty0, float tx1, float ty1, vuint32 color, int handle,
//                                      optional float scaleX/*=1.0*/, optional float scaleY/*=1.0*/, optional bool ignoreOffset/*=false*/);
IMPLEMENT_FUNCTION(VWidget, DrawPicRecoloredEx) {
  float x, y, tx0, ty0, tx1, ty1;
  vuint32 color;
  int handle;
  VOptParamFloat scaleX(1.0f);
  VOptParamFloat scaleY(1.0f);
  VOptParamBool ignoreOffset(false);
  vobjGetParamSelf(x, y, tx0, ty0, tx1, ty1, color, handle, scaleX, scaleY, ignoreOffset);
  if (Self) Self->DrawPicRecolored(x, y, tx0, ty0, tx1, ty1, color, handle, scaleX, scaleY, ignoreOffset);
}

//native final void DrawPicShadowEx (float x, float y, float tx0, float ty0, float tx1, float ty1, int handle,
//                                   optional float alpha/*=0.625f*/,
//                                   optional float scaleX/*=1.0*/, optional float scaleY/*=1.0*/, optional bool ignoreOffset/*=false*/);
IMPLEMENT_FUNCTION(VWidget, DrawPicShadowEx) {
  float x, y, tx0, ty0, tx1, ty1;
  int handle;
  VOptParamFloat alpha(0.625f);
  VOptParamFloat scaleX(1.0f);
  VOptParamFloat scaleY(1.0f);
  VOptParamBool ignoreOffset(false);
  vobjGetParamSelf(x, y, tx0, ty0, tx1, ty1, handle, alpha, scaleX, scaleY, ignoreOffset);
  if (Self) Self->DrawPicShadow(x, y, tx0, ty0, tx1, ty1, handle, alpha, scaleX, scaleY, ignoreOffset);
}

IMPLEMENT_FUNCTION(VWidget, DrawPicScaledIgnoreOffset) {
  int X, Y, Handle;
  VOptParamFloat scaleX(1.0f);
  VOptParamFloat scaleY(1.0f);
  VOptParamFloat Alpha(1.0f);
  VOptParamInt Translation(0);
  vobjGetParamSelf(X, Y, Handle, scaleX, scaleY, Alpha, Translation);
  if (Self) Self->DrawPicScaledIgnoreOffset(X, Y, Handle, scaleX, scaleY, Alpha, Translation);
}

IMPLEMENT_FUNCTION(VWidget, DrawPic) {
  int X, Y, Handle;
  VOptParamFloat Alpha(1.0f);
  VOptParamInt Translation(0);
  vobjGetParamSelf(X, Y, Handle, Alpha, Translation);
  if (Self) Self->DrawPic(X, Y, Handle, Alpha, Translation);
}

IMPLEMENT_FUNCTION(VWidget, DrawPicScaled) {
  int X, Y, Handle;
  float scaleX, scaleY;
  VOptParamFloat Alpha(1.0f);
  VOptParamInt Translation(0);
  vobjGetParamSelf(X, Y, Handle, scaleX, scaleY, Alpha, Translation);
  if (Self) Self->DrawPicScaled(X, Y, Handle, scaleX, scaleY, Alpha, Translation);
}

IMPLEMENT_FUNCTION(VWidget, DrawShadowedPic) {
  int X, Y, Handle;
  VOptParamFloat scaleX(1.0f);
  VOptParamFloat scaleY(1.0f);
  VOptParamBool ignoreOffset(false);
  vobjGetParamSelf(X, Y, Handle, scaleX, scaleY, ignoreOffset);
  if (Self) Self->DrawShadowedPic(X, Y, Handle, scaleX, scaleY, ignoreOffset);
}

IMPLEMENT_FUNCTION(VWidget, FillRectWithFlat) {
  int X, Y, Width, Height;
  VName Name;
  vobjGetParamSelf(X, Y, Width, Height, Name);
  if (Self) Self->FillRectWithFlat(X, Y, Width, Height, Name);
}

IMPLEMENT_FUNCTION(VWidget, FillRectWithFlatHandle) {
  int X, Y, Width, Height, Handle;
  vobjGetParamSelf(X, Y, Width, Height, Handle);
  if (Self) Self->FillRectWithFlatHandle(X, Y, Width, Height, Handle);
}

IMPLEMENT_FUNCTION(VWidget, FillRectWithFlatRepeat) {
  int X, Y, Width, Height;
  VName Name;
  vobjGetParamSelf(X, Y, Width, Height, Name);
  if (Self) Self->FillRectWithFlatRepeat(X, Y, Width, Height, Name);
}

IMPLEMENT_FUNCTION(VWidget, FillRectWithFlatRepeatHandle) {
  int X, Y, Width, Height, Handle;
  vobjGetParamSelf(X, Y, Width, Height, Handle);
  if (Self) Self->FillRectWithFlatRepeatHandle(X, Y, Width, Height, Handle);
}

IMPLEMENT_FUNCTION(VWidget, FillRect) {
  int X, Y, Width, Height;
  vuint32 color;
  VOptParamFloat alpha(1.0f);
  vobjGetParamSelf(X, Y, Width, Height, color, alpha);
  if (Self) Self->FillRect(X, Y, Width, Height, color, alpha);
}

IMPLEMENT_FUNCTION(VWidget, DrawRect) {
  int X, Y, Width, Height;
  vuint32 color;
  VOptParamFloat alpha(1.0f);
  vobjGetParamSelf(X, Y, Width, Height, color, alpha);
  if (Self) Self->DrawRect(X, Y, Width, Height, color, alpha);
}

IMPLEMENT_FUNCTION(VWidget, ShadeRect) {
  int X, Y, Width, Height;
  float Shade;
  vobjGetParamSelf(X, Y, Width, Height, Shade);
  if (Self) Self->ShadeRect(X, Y, Width, Height, Shade);
}

IMPLEMENT_FUNCTION(VWidget, DrawLine) {
  int X1, Y1, X2, Y2;
  vuint32 color;
  VOptParamFloat alpha(1.0f);
  vobjGetParamSelf(X1, Y1, X2, Y2, color, alpha);
  if (Self) Self->DrawLine(X1, Y1, X2, Y2, color, alpha);
}

IMPLEMENT_FUNCTION(VWidget, GetFont) {
  vobjGetParamSelf();
  if (Self) {
    VFont *font = Self->GetFont();
    if (font) { RET_NAME(font->GetFontName()); return; }
  }
  RET_NAME(NAME_None);
}

IMPLEMENT_FUNCTION(VWidget, SetFont) {
  VName FontName;
  vobjGetParamSelf(FontName);
  if (Self) Self->SetFont(FontName);
}

IMPLEMENT_FUNCTION(VWidget, SetTextAlign) {
  int halign, valign;
  vobjGetParamSelf(halign, valign);
  if (Self) Self->SetTextAlign((halign_e)halign, (valign_e)valign);
}

IMPLEMENT_FUNCTION(VWidget, SetTextShadow) {
  bool State;
  vobjGetParamSelf(State);
  if (Self) Self->SetTextShadow(State);
}

// native final void TextBounds (int x, int y, string text, out int x0, out int y0, out int width, out int height, optional bool trimTrailNL);
IMPLEMENT_FUNCTION(VWidget, TextBounds) {
  int x, y;
  VStr text;
  int *x0, *y0, *width, *height;
  VOptParamBool trimTrailNL(true);
  vobjGetParamSelf(x, y, text, x0, y0, width, height, trimTrailNL);
  if (Self) {
    Self->TextBounds(x, y, text, x0, y0, width, height, trimTrailNL);
  } else {
    if (x0) *x0 = x;
    if (y0) *y0 = y;
    if (width) *width = 0;
    if (height) *height = 0;
  }
}

IMPLEMENT_FUNCTION(VWidget, TextWidth) {
  VStr text;
  vobjGetParamSelf(text);
  RET_INT(Self ? Self->TextWidth(text) : 0);
}

IMPLEMENT_FUNCTION(VWidget, StringWidth) {
  VStr text;
  vobjGetParamSelf(text);
  RET_INT(Self ? Self->TextWidth(text) : 0);
}

IMPLEMENT_FUNCTION(VWidget, TextHeight) {
  VStr text;
  vobjGetParamSelf(text);
  RET_INT(Self ? Self->TextHeight(text) : 0);
}

IMPLEMENT_FUNCTION(VWidget, FontHeight) {
  vobjGetParamSelf();
  RET_INT(Self ? Self->FontHeight() : 0);
}

IMPLEMENT_FUNCTION(VWidget, SplitText) {
  VStr Text;
  TArray<VSplitLine> *Lines;
  int MaxWidth;
  VOptParamBool trimRight(true);
  vobjGetParamSelf(Text, Lines, MaxWidth, trimRight);
  RET_INT(Self ? Self->Font->SplitText(Text, *Lines, MaxWidth, trimRight) : 0);
}

IMPLEMENT_FUNCTION(VWidget, SplitTextWithNewlines) {
  VStr Text;
  int MaxWidth;
  VOptParamBool trimRight(true);
  vobjGetParamSelf(Text, MaxWidth, trimRight);
  RET_STR(Self ? Self->Font->SplitTextWithNewlines(Text, MaxWidth, trimRight) : VStr::EmptyString);
}

IMPLEMENT_FUNCTION(VWidget, DrawText) {
  int X, Y;
  VStr String;
  VOptParamInt Color(CR_UNTRANSLATED);
  VOptParamInt BoldColor(CR_UNTRANSLATED);
  VOptParamFloat Alpha(1.0f);
  vobjGetParamSelf(X, Y, String, Color, BoldColor, Alpha);
  if (Self) Self->DrawText(X, Y, String, Color, BoldColor, Alpha);
}

IMPLEMENT_FUNCTION(VWidget, CursorWidth) {
  vobjGetParamSelf();
  RET_INT(Self ? Self->CursorWidth() : 0);
}

IMPLEMENT_FUNCTION(VWidget, DrawCursor) {
  VOptParamInt Color(CR_UNTRANSLATED);
  vobjGetParamSelf(Color);
  if (Self) Self->DrawCursor(Color);
}

IMPLEMENT_FUNCTION(VWidget, DrawCursorAt) {
  int cx, cy;
  VOptParamInt Color(CR_UNTRANSLATED);
  vobjGetParamSelf(cx, cy, Color);
  if (Self) Self->DrawCursorAt(cx, cy, Color);
}

IMPLEMENT_FUNCTION(VWidget, SetCursorPos) {
  int cx, cy;
  vobjGetParamSelf(cx, cy);
  Self->SetCursorPos(cx, cy);
}

IMPLEMENT_FUNCTION(VWidget, get_CursorX) {
  vobjGetParamSelf();
  RET_INT(Self ? Self->GetCursorX() : 0);
}

IMPLEMENT_FUNCTION(VWidget, get_CursorY) {
  vobjGetParamSelf();
  RET_INT(Self ? Self->GetCursorY() : 0);
}

IMPLEMENT_FUNCTION(VWidget, set_CursorX) {
  int v;
  vobjGetParamSelf(v);
  if (Self) Self->SetCursorX(v);
}

IMPLEMENT_FUNCTION(VWidget, set_CursorY) {
  int v;
  vobjGetParamSelf(v);
  if (Self) Self->SetCursorY(v);
}

IMPLEMENT_FUNCTION(VWidget, FindTextColor) {
  VStr Name;
  vobjGetParam(Name);
  RET_INT(VFont::FindTextColor(*Name));
}

IMPLEMENT_FUNCTION(VWidget, TranslateXY) {
  float *px, *py;
  vobjGetParamSelf(px, py);
  if (Self) {
    if (px) *px = (Self->ClipRect.ScaleX*(*px)+Self->ClipRect.OriginX)/fScaleX;
    if (py) *py = (Self->ClipRect.ScaleY*(*py)+Self->ClipRect.OriginY)/fScaleY;
  } else {
    if (px) *px = 0;
    if (py) *py = 0;
  }
}


// native final Widget GetWidgetAt (float x, float y, optional bool allowDisabled/*=false*/);
IMPLEMENT_FUNCTION(VWidget, GetWidgetAt) {
  float x, y;
  VOptParamBool allowDisabled(false);
  vobjGetParamSelf(x, y, allowDisabled);
  RET_REF(Self ? Self->GetWidgetAt(x, y, allowDisabled) : nullptr);
}


// native final float CalcHexHeight (float h);
IMPLEMENT_FUNCTION(VWidget, CalcHexHeight) {
  float h;
  vobjGetParamSelf(h);
  RET_FLOAT(Self ? Self->CalcHexHeight(h) : 0.0f);
}

// native final void DrawHex (float x0, float y0, float w, float h, int color, optional float alpha/*=1.0f*/);
IMPLEMENT_FUNCTION(VWidget, DrawHex) {
  float x0, y0, w, h;
  vuint32 color;
  VOptParamFloat alpha(1.0f);
  vobjGetParamSelf(x0, y0, w, h, color, alpha);
  if (Self) Self->DrawHex(x0, y0, w, h, color, alpha);
}

// native final void FillHex (float x0, float y0, float w, float h, int color, optional float alpha/*=1.0f*/);
IMPLEMENT_FUNCTION(VWidget, FillHex) {
  float x0, y0, w, h;
  vuint32 color;
  VOptParamFloat alpha(1.0f);
  vobjGetParamSelf(x0, y0, w, h, color, alpha);
  if (Self) Self->FillHex(x0, y0, w, h, color, alpha);
}

// native final void ShadeHex (float x0, float y0, float w, float h, float darkening);
IMPLEMENT_FUNCTION(VWidget, ShadeHex) {
  float x0, y0, w, h, darkening;
  vobjGetParamSelf(x0, y0, w, h, darkening);
  if (Self) Self->ShadeHex(x0, y0, w, h, darkening);
}


// native final bool IsPointInsideHex (float x, float y, float x0, float y0, float w, float h);
IMPLEMENT_FUNCTION(VWidget, IsPointInsideHex) {
  float x, y, x0, y0, w, h;
  vobjGetParamSelf(x, y, x0, y0, w, h);
  RET_BOOL(Self ? Self->IsPointInsideHex(x, y, x0, y0, w, h) : false);
}


// native final void DrawHexColorPattern (float x0, float y0, int radius, float cellW, float cellH);
IMPLEMENT_FUNCTION(VWidget, DrawHexColorPattern) {
  float x0, y0;
  int radius;
  float cellW, cellH;
  vobjGetParamSelf(x0, y0, radius, cellW, cellH);
  if (Self) Self->DrawHexColorPattern(x0, y0, radius, cellW, cellH);
}


// native final float CalcHexColorPatternWidth (int radius, float cellW, float cellH);
IMPLEMENT_FUNCTION(VWidget, CalcHexColorPatternWidth) {
  int radius;
  float cellW, cellH;
  vobjGetParamSelf(radius, cellW, cellH);
  if (Self) {
    float w;
    Self->CalcHexColorPatternDims(&w, nullptr, radius, cellW, cellH);
    RET_FLOAT(w);
  } else {
    RET_FLOAT(0.0f);
  }
}

// native final float CalcHexColorPatternHeight (int radius, float cellW, float cellH);
IMPLEMENT_FUNCTION(VWidget, CalcHexColorPatternHeight) {
  int radius;
  float cellW, cellH;
  vobjGetParamSelf(radius, cellW, cellH);
  if (Self) {
    float h;
    Self->CalcHexColorPatternDims(nullptr, &h, radius, cellW, cellH);
    RET_FLOAT(h);
  } else {
    RET_FLOAT(0.0f);
  }
}

// native final bool IsValidHexColorPatternCoords (int hpx, int hpy, int radius);
IMPLEMENT_FUNCTION(VWidget, IsValidHexColorPatternCoords) {
  int hpx, hpy, radius;
  vobjGetParamSelf(hpx, hpy, radius);
  RET_BOOL(Self ? Self->IsValidHexColorPatternCoords(hpx, hpy, radius) : false);
}

// native final bool CalcHexColorPatternCoords (out int hpx, out int hpy, float x, float y, float x0, float y0, int radius, float cellW, float cellH);
IMPLEMENT_FUNCTION(VWidget, CalcHexColorPatternCoords) {
  int *hpx;
  int *hpy;
  float x, y, x0, y0;
  int radius;
  float cellW, cellH;
  vobjGetParamSelf(hpx, hpy, x, y, x0, y0, radius, cellW, cellH);
  RET_BOOL(Self ? Self->CalcHexColorPatternCoords(hpx, hpy, x, y, x0, y0, radius, cellW, cellH) : false);
}

// native final bool CalcHexColorPatternHexCoordsAt (out float hx, out float hy, int hpx, int hpy, float x0, float y0, int radius, float cellW, float cellH);
IMPLEMENT_FUNCTION(VWidget, CalcHexColorPatternHexCoordsAt) {
  float *hx;
  float *hy;
  int hpx, hpy;
  float x0, y0;
  int radius;
  float cellW, cellH;
  vobjGetParamSelf(hx, hy, hpx, hpy, x0, y0, radius, cellW, cellH);
  RET_BOOL(Self ? Self->CalcHexColorPatternHexCoordsAt(hx, hy, hpx, hpy, x0, y0, radius, cellW, cellH) : false);
}

// native final int GetHexColorPatternColorAt (int hpx, int hpy, int radius);
IMPLEMENT_FUNCTION(VWidget, GetHexColorPatternColorAt) {
  int hpx, hpy, radius;
  vobjGetParamSelf(hpx, hpy, radius);
  RET_INT(Self ? Self->GetHexColorPatternColorAt(hpx, hpy, radius) : 0);
}

// native final bool FindHexColorCoords (out int hpx, out int hpy, int radius, float h, float s);
IMPLEMENT_FUNCTION(VWidget, FindHexColorCoords) {
  int *hpx;
  int *hpy;
  int radius;
  float h, s;
  vobjGetParamSelf(hpx, hpy, radius, h, s);
  RET_BOOL(Self ? Self->FindHexColorCoords(hpx, hpy, radius, h, s) : false);
}

// native final void MoveHexCoords (ref int hpx, ref int hpy, int dx, int dy, int radius);
IMPLEMENT_FUNCTION(VWidget, MoveHexCoords) {
  int *hpx;
  int *hpy;
  int dx, dy, radius;
  vobjGetParamSelf(hpx, hpy, dx, dy, radius);
  if (Self) Self->MoveHexCoords(*hpx, *hpy, dx, dy, radius);
}

