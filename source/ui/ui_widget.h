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

struct VClipRect
{
  float   OriginX;  //  Origin of the widget, in absolute coordinates.
  float   OriginY;

  float   ScaleX;   //  Accomulative scale.
  float   ScaleY;

  float   ClipX1;   //  Clipping rectangle, in absolute coordinates.
  float   ClipY1;
  float   ClipX2;
  float   ClipY2;

  bool HasArea() const
  {
    return ClipX1 < ClipX2 && ClipY1 < ClipY2;
  }
};

class VWidget : public VObject
{
  DECLARE_CLASS(VWidget, VObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VWidget)

private:
  //  Parent container widget.
  VWidget*      ParentWidget;
  //  Linked list of child widgets.
  VWidget*      FirstChildWidget;
  VWidget*      LastChildWidget;
  //  Links in the linked list of widgets.
  VWidget*      PrevWidget;
  VWidget*      NextWidget;

  //  Position of the widget in the parent widget.
  int         PosX;
  int         PosY;
  //  Offset for children.
  int         OfsX;
  int         OfsY;
  //  Size of the child area of the widget.
  int         SizeWidth;
  int         SizeHeight;
  //  Scaling of the widget.
  float       SizeScaleX;
  float       SizeScaleY;

  VClipRect     ClipRect;

  //  Currently focused child widget.
  VWidget*      CurrentFocusChild;

  VFont*        Font;

  //  Text alignements
  vuint8        HAlign;
  vuint8        VAlign;

  //  Text cursor
  int         LastX;
  int         LastY;

  // Booleans
  enum
  {
    //  Is this widget visible?
    WF_IsVisible    = 0x0001,
    //  A flag that enables or disables Tick event.
    WF_TickEnabled    = 0x0002,
    //  Is this widget enabled and can receive input.
    WF_IsEnabled    = 0x0004,
    //  Can this widget be focused?
    WF_IsFocusable    = 0x0008,
    //  Mouse button state for click events.
    WF_LMouseDown   = 0x0010,
    WF_MMouseDown   = 0x0020,
    WF_RMouseDown   = 0x0040,
    //  Shadowed text
    WF_TextShadowed   = 0x0080,
  };
  vuint32       WidgetFlags;

  VObjectDelegate   FocusLost;
  VObjectDelegate   FocusReceived;

  VObjectDelegate   KeyDown;
  VObjectDelegate   KeyUp;

  void AddChild(VWidget*);
  void RemoveChild(VWidget*);

  void ClipTree();
  void DrawTree();
  void TickTree(float DeltaTime);

  void FindNewFocus();

  VWidget* GetWidgetAt(float, float);

  bool TransferAndClipRect(float&, float&, float&, float&, float&, float&,
    float&, float&) const;
  void DrawString(int, int, const VStr&, int, int, float);

  friend class VRootWidget;

public:
  //  Destroys all child widgets.
  virtual void Init(VWidget*);
  void Destroy();
  void DestroyAllChildren();

  VRootWidget* GetRootWidget();

  //  Methods to move widget on top or bottom.
  void Lower();
  void Raise();
  void MoveBefore(VWidget*);
  void MoveAfter(VWidget*);

  //  Methods to set position, size and scale.
  void SetPos(int NewX, int NewY)
  {
    SetConfiguration(NewX, NewY, SizeWidth, SizeHeight, SizeScaleX,
      SizeScaleY);
  }
  void SetX(int NewX)
  {
    SetPos(NewX, PosY);
  }
  void SetY(int NewY)
  {
    SetPos(PosX, NewY);
  }
  void SetOfsX(int NewX)
  {
    if (OfsX != NewX) { OfsX = NewX; SetConfiguration(PosX, PosY, SizeWidth, SizeHeight, SizeScaleX, SizeScaleY); }
  }
  void SetOfsY(int NewY)
  {
    if (OfsY != NewY) { OfsY = NewY; SetConfiguration(PosX, PosY, SizeWidth, SizeHeight, SizeScaleX, SizeScaleY); }
  }
  void SetSize(int NewWidth, int NewHeight)
  {
    SetConfiguration(PosX, PosY, NewWidth, NewHeight, SizeScaleX,
      SizeScaleY);
  }
  void SetWidth(int NewWidth)
  {
    SetSize(NewWidth, SizeHeight);
  }
  void SetHeight(int NewHeight)
  {
    SetSize(SizeWidth, NewHeight);
  }
  void SetScale(float NewScaleX, float NewScaleY)
  {
    SetConfiguration(PosX, PosY, SizeWidth, SizeHeight, NewScaleX, NewScaleY);
  }
  void SetConfiguration(int, int, int, int, float = 1.0, float = 1.0);

  //  Visibility methods.
  void SetVisibility(bool);
  void Show()
  {
    SetVisibility(true);
  }
  void Hide()
  {
    SetVisibility(false);
  }
  bool IsVisible(bool bRecurse = true) const
  {
    if (bRecurse)
    {
      const VWidget* pParent = this;
      while (pParent)
      {
        if (!(pParent->WidgetFlags & WF_IsVisible))
        {
          break;
        }
        pParent = pParent->ParentWidget;
      }
      return (pParent ? false : true);
    }
    else
    {
      return !!(WidgetFlags & WF_IsVisible);
    }
  }

  //  Enable state methods
  void SetEnabled(bool);
  void Enable()
  {
    SetEnabled(true);
  }
  void Disable()
  {
    SetEnabled(false);
  }
  bool IsEnabled(bool bRecurse = true) const
  {
    if (bRecurse)
    {
      const VWidget* pParent = this;
      while (pParent)
      {
        if (!(pParent->WidgetFlags & WF_IsEnabled))
        {
          break;
        }
        pParent = pParent->ParentWidget;
      }
      return (pParent ? false : true);
    }
    else
    {
      return !!(WidgetFlags & WF_IsEnabled);
    }
  }

  //  Focusable state methods.
  void SetFocusable(bool);
  bool IsFocusable() const
  {
    return !!(WidgetFlags & WF_IsFocusable);
  }

  //  Focus methods.
  void SetCurrentFocusChild(VWidget*);
  VWidget* GetCurrentFocus() const
  {
    return CurrentFocusChild;
  }
  bool IsFocus(bool Recurse = true) const;
  void SetFocus();

  void DrawPic(int, int, int, float = 1.0, int = 0);
  void DrawPic(int, int, VTexture*, float = 1.0, int = 0);
  void DrawShadowedPic(int, int, int);
  void DrawShadowedPic(int, int, VTexture*);
  void FillRectWithFlat(int, int, int, int, VName);
  void ShadeRect(int, int, int, int, float);

  void SetFont(VFont*);
  void SetFont(VName);
  void SetTextAlign(halign_e, valign_e);
  void SetTextShadow(bool);
  void DrawText(int, int, const VStr&, int, int, float);
  void DrawCursor();
  void DrawCursorAt(int, int);

  static VWidget *CreateNewWidget(VClass*, VWidget*);

  //  Events.
  void OnCreate()
  {
    P_PASS_SELF;
    EV_RET_VOID(NAME_OnCreate);
  }
  void OnDestroy()
  {
    P_PASS_SELF;
    EV_RET_VOID(NAME_OnDestroy);
  }
  virtual void OnChildAdded(VWidget* Child)
  {
    P_PASS_SELF;
    P_PASS_REF(Child);
    EV_RET_VOID(NAME_OnChildAdded);
  }
  virtual void OnChildRemoved(VWidget* Child)
  {
    P_PASS_SELF;
    P_PASS_REF(Child);
    EV_RET_VOID(NAME_OnChildRemoved);
  }
  virtual void OnConfigurationChanged()
  {
    P_PASS_SELF;
    EV_RET_VOID(NAME_OnConfigurationChanged);
  }
  virtual void OnVisibilityChanged(bool NewVisibility)
  {
    P_PASS_SELF;
    P_PASS_BOOL(NewVisibility);
    EV_RET_VOID(NAME_OnVisibilityChanged);
  }
  virtual void OnEnableChanged(bool bNewEnable)
  {
    P_PASS_SELF;
    P_PASS_BOOL(bNewEnable);
    EV_RET_VOID(NAME_OnEnableChanged);
  }
  virtual void OnFocusableChanged(bool bNewFocusable)
  {
    P_PASS_SELF;
    P_PASS_BOOL(bNewFocusable);
    EV_RET_VOID(NAME_OnFocusableChanged);
  }
  virtual void OnFocusReceived()
  {
    P_PASS_SELF;
    EV_RET_VOID(NAME_OnFocusReceived);
  }
  virtual void OnFocusLost()
  {
    P_PASS_SELF;
    EV_RET_VOID(NAME_OnFocusLost);
  }
  virtual void OnDraw()
  {
    P_PASS_SELF;
    EV_RET_VOID(NAME_OnDraw);
  }
  virtual void OnPostDraw()
  {
    P_PASS_SELF;
    EV_RET_VOID(NAME_OnPostDraw);
  }
  virtual void Tick(float DeltaTime)
  {
    P_PASS_SELF;
    P_PASS_FLOAT(DeltaTime);
    EV_RET_VOID(NAME_Tick);
  }
  virtual bool OnKeyDown(int Key)
  {
    P_PASS_SELF;
    P_PASS_INT(Key);
    EV_RET_BOOL(NAME_OnKeyDown);
  }
  virtual bool OnKeyUp(int Key)
  {
    P_PASS_SELF;
    P_PASS_INT(Key);
    EV_RET_BOOL(NAME_OnKeyUp);
  }
  virtual bool OnMouseMove(int OldX, int OldY, int NewX, int NewY)
  {
    P_PASS_SELF;
    P_PASS_INT(OldX);
    P_PASS_INT(OldY);
    P_PASS_INT(NewX);
    P_PASS_INT(NewY);
    EV_RET_BOOL(NAME_OnMouseMove);
  }
  virtual void OnMouseEnter()
  {
    P_PASS_SELF;
    EV_RET_VOID(NAME_OnMouseEnter);
  }
  virtual void OnMouseLeave()
  {
    P_PASS_SELF;
    EV_RET_VOID(NAME_OnMouseEnter);
  }
  virtual bool OnMouseDown(int X, int Y, int Button)
  {
    P_PASS_SELF;
    P_PASS_INT(X);
    P_PASS_INT(Y);
    P_PASS_INT(Button);
    EV_RET_BOOL(NAME_OnMouseDown);
  }
  virtual bool OnMouseUp(int X, int Y, int Button)
  {
    P_PASS_SELF;
    P_PASS_INT(X);
    P_PASS_INT(Y);
    P_PASS_INT(Button);
    EV_RET_BOOL(NAME_OnMouseUp);
  }
  virtual void OnMouseClick(int X, int Y)
  {
    P_PASS_SELF;
    P_PASS_INT(X);
    P_PASS_INT(Y);
    EV_RET_VOID(NAME_OnMouseClick);
  }
  virtual void OnMMouseClick(int X, int Y)
  {
    P_PASS_SELF;
    P_PASS_INT(X);
    P_PASS_INT(Y);
    EV_RET_VOID(NAME_OnMMouseClick);
  }
  virtual void OnRMouseClick(int X, int Y)
  {
    P_PASS_SELF;
    P_PASS_INT(X);
    P_PASS_INT(Y);
    EV_RET_VOID(NAME_OnRMouseClick);
  }

  //  Script natives
  DECLARE_FUNCTION(NewChild)
  DECLARE_FUNCTION(Destroy)
  DECLARE_FUNCTION(DestroyAllChildren)

  DECLARE_FUNCTION(GetRootWidget)

  DECLARE_FUNCTION(Raise)
  DECLARE_FUNCTION(Lower)
  DECLARE_FUNCTION(MoveBefore)
  DECLARE_FUNCTION(MoveAfter)

  DECLARE_FUNCTION(SetPos)
  DECLARE_FUNCTION(SetX)
  DECLARE_FUNCTION(SetY)
  DECLARE_FUNCTION(SetOfsX)
  DECLARE_FUNCTION(SetOfsY)
  DECLARE_FUNCTION(SetSize)
  DECLARE_FUNCTION(SetWidth)
  DECLARE_FUNCTION(SetHeight)
  DECLARE_FUNCTION(SetScale)
  DECLARE_FUNCTION(SetConfiguration)

  DECLARE_FUNCTION(SetVisibility)
  DECLARE_FUNCTION(Show)
  DECLARE_FUNCTION(Hide)
  DECLARE_FUNCTION(IsVisible)

  DECLARE_FUNCTION(SetEnabled)
  DECLARE_FUNCTION(Enable)
  DECLARE_FUNCTION(Disable)
  DECLARE_FUNCTION(IsEnabled)

  DECLARE_FUNCTION(SetFocusable)
  DECLARE_FUNCTION(IsFocusable)

  DECLARE_FUNCTION(SetCurrentFocusChild)
  DECLARE_FUNCTION(GetCurrentFocus)
  DECLARE_FUNCTION(IsFocus)
  DECLARE_FUNCTION(SetFocus)

  DECLARE_FUNCTION(DrawPic)
  DECLARE_FUNCTION(DrawShadowedPic)
  DECLARE_FUNCTION(FillRectWithFlat)
  DECLARE_FUNCTION(ShadeRect)

  DECLARE_FUNCTION(SetFont)
  DECLARE_FUNCTION(SetTextAlign)
  DECLARE_FUNCTION(SetTextShadow)
  DECLARE_FUNCTION(TextWidth)
  DECLARE_FUNCTION(TextHeight)
  DECLARE_FUNCTION(SplitText)
  DECLARE_FUNCTION(SplitTextWithNewlines)
  DECLARE_FUNCTION(DrawText)
  DECLARE_FUNCTION(DrawCursor)
  DECLARE_FUNCTION(FindTextColour)
};
