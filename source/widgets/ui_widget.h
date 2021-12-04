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
#ifndef VAVOOM_UIWIDGETS_WIDGET_HEADER
#define VAVOOM_UIWIDGETS_WIDGET_HEADER

#include "../text.h"


struct VClipRect {
  // origin of the widget, in absolute coordinates
  float OriginX;
  float OriginY;

  // accumulative scale
  float ScaleX;
  float ScaleY;

  // clipping rectangle, in absolute coordinates
  float ClipX1;
  float ClipY1;
  float ClipX2;
  float ClipY2;

  inline bool HasArea () const noexcept { return (ClipX1 < ClipX2 && ClipY1 < ClipY2); }
};


struct VMouseDownInfo {
  float time; // mouse down time (0 means "isn't down")
  vint32 x, y; // where it was "downed" (global coords)
  float localx, localy; // local to widget
};


class VWidget : public VObject {
  DECLARE_CLASS(VWidget, VObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VWidget)

  friend class VRootWidget;

protected:
  static TMapNC<VWidget *, bool> AllWidgets;

protected:
  // parent container widget
  VWidget *ParentWidget;
  // linked list of child widgets
  VWidget *FirstChildWidget;
  VWidget *LastChildWidget;
  // links in the linked list of widgets
  VWidget *PrevWidget;
  VWidget *NextWidget;

  // position of the widget in the parent widget
  vint32 PosX;
  vint32 PosY;
  // offset for children
  vint32 OfsX;
  vint32 OfsY;
  // size of the child area of the widget
  vint32 SizeWidth;
  vint32 SizeHeight;
  // scaling of the widget
  float SizeScaleX;
  float SizeScaleY;

  VClipRect ClipRect;

  // currently focused child widget
  VWidget *CurrentFocusChild;

  VFont *Font;

  // text alignements
  vuint8 HAlign;
  vuint8 VAlign;

  // text cursor
  vint32 LastX;
  vint32 LastY;

  // Booleans
  enum {
    WF_IsVisible    = 1u<<0, // is this widget visible?
    WF_TickEnabled  = 1u<<1, // a flag that enables or disables Tick event
    WF_IsEnabled    = 1u<<2, // is this widget enabled and can receive input
    WF_IsFocusable  = 1u<<3, // can this widget be focused?
    WF_CloseOnBlur  = 1u<<4, // should this widget be closed if it loses focus?
    WF_Modal        = 1u<<5, // is this widget "modal" (i.e. can't lose focus)? (not implemented yet)
    WF_OnTop        = 1u<<6, // should this widget be "always on top"?
    WF_TextShadowed = 1u<<7, // shadowed text
    WF_UseScissor   = 1u<<8, // use scissor in rendering?
    WF_WantMouse    = 1u<<9, // set if this widget want mouse input (mouse will be stolen unconditionally)
  };
  vuint32 WidgetFlags;

  // mouse down time (0 means "isn't down")
  VMouseDownInfo MouseDownState[9]; // K_MOUSE1..K_MOUSE9

  vint32 CursorChar;

  //VObjectDelegate FocusLost;
  //VObjectDelegate FocusReceived;

protected:
  void AddChild (VWidget *NewChild);
  void RemoveChild (VWidget *InChild);

  void ClipTree () noexcept;
  void DrawTree ();
  void TickTree (float DeltaTime);

  // ignores `CurrentFocusChild`, selects next focusable or `nullptr`
  void FindNewFocus ();

  // can return `none` if coords are out of any widget, including this one
  VWidget *GetWidgetAt (float X, float Y, bool allowDisabled=false) noexcept;

  bool TransferAndClipLine (float &X1, float &Y1, float &X2, float &Y2) const noexcept;

  // translate screen and texture coordinates
  bool TransferAndClipRect (float &X1, float &Y1, float &X2, float &Y2, float &S1, float &T1, float &S2, float &T2) const noexcept;
  // translate screen coordinates
  bool TransferAndClipRect (float &X1, float &Y1, float &X2, float &Y2) const noexcept;

  void DrawString (int x, int y, VStr String, int NormalColor, int BoldColor, float Alpha);

  void MarkDead ();
  void MarkChildrenDead ();

  inline bool CanBeFocused () const noexcept {
    return (!IsGoingToDie() && ((WidgetFlags&(WF_IsVisible|WF_IsEnabled|WF_IsFocusable)) == (WF_IsVisible|WF_IsEnabled|WF_IsFocusable)));
  }

  inline bool IsVisibleFlag () const noexcept { return (WidgetFlags&WF_IsVisible); }
  inline bool IsTickEnabledFlag () const noexcept { return (WidgetFlags&WF_TickEnabled); }
  inline bool IsEnabledFlag () const noexcept { return (WidgetFlags&WF_IsEnabled); }
  inline bool IsFocusableFlag () const noexcept { return (WidgetFlags&WF_IsFocusable); }
  inline bool IsCloseOnBlurFlag () const noexcept { return (WidgetFlags&WF_CloseOnBlur); }
  inline bool IsModalFlag () const noexcept { return (WidgetFlags&WF_Modal); }
  inline bool IsOnTopFlag () const noexcept { return (WidgetFlags&WF_OnTop); }
  inline bool IsUseScissor () const noexcept { return (WidgetFlags&WF_UseScissor); }
  inline bool IsWantMouse () const noexcept { return (WidgetFlags&WF_WantMouse); }

  inline void SetCloseOnBlurFlag (bool v) noexcept { if (v) WidgetFlags |= WF_CloseOnBlur; else WidgetFlags &= ~WF_CloseOnBlur; }

  inline bool IsChildAdded () const noexcept {
    if (!ParentWidget) return false;
    if (PrevWidget || NextWidget) return true;
    // check if parent widget list points to us
    if (ParentWidget->FirstChildWidget != ParentWidget->LastChildWidget) return false;
    return (ParentWidget->FirstChildWidget == this);
  }

  inline void ResetMouseDowns () noexcept { for (unsigned f = 0; f < 9; ++f) MouseDownState[f].time = 0; }

  // used to calculate local widget coords
  inline float ScaledXToLocal (const float v) const noexcept { return (v-ClipRect.OriginX)/ClipRect.ScaleX; }
  inline float ScaledYToLocal (const float v) const noexcept { return (v-ClipRect.OriginY)/ClipRect.ScaleY; }

  inline float LocalXToRoot (const float v) const noexcept { return (v+ClipRect.OriginX)*ClipRect.ScaleX; }
  inline float LocalYToRoot (const float v) const noexcept { return (v+ClipRect.OriginY)*ClipRect.ScaleY; }

  VWidget *FindFirstOnTopChild () noexcept;
  VWidget *FindLastNormalChild () noexcept;

  inline bool HasOnTopChildren () const noexcept { return (LastChildWidget && LastChildWidget->IsOnTopFlag()); }

  void UnlinkFromParent () noexcept;
  // if `w` is null, link as first
  void LinkToParentBefore (VWidget *w) noexcept;
  // if `w` is null, link as last
  void LinkToParentAfter (VWidget *w) noexcept;

  bool SetupScissor ();

protected:
  // if `rgbcolor` is negative, use `rgbcolor&0xffffff` as RGB
  void DrawCharPic (int X, int Y, VTexture *Tex, const VFont::CharRect &rect, int rgbcolor, float Alpha=1.0f, bool shadowed=false);
  inline void DrawCharPicShadowed (int X, int Y, VTexture *Tex, const VFont::CharRect &rect, int rgbcolor) { DrawCharPic(X, Y, Tex, rect, rgbcolor, 1.0f, true); }

  // to self, then to children
  // ignores cancel/consume, will prevent event modification
  void BroadcastEvent (event_t *evt);

public: // iterators
  // the iterators will survive widget deletion in called method
  struct WidgetIterator {
  public:
    VWidget *curr;
    VWidget *next;
    bool fwd;

    WidgetIterator () = delete;

    inline WidgetIterator (const WidgetIterator &w) noexcept { curr = w.curr; next = w.next; fwd = w.fwd; }

    inline WidgetIterator (VWidget *first, bool afwd) noexcept
      : curr(first)
      , next(first ? (afwd ? first->NextWidget : first->PrevWidget) : nullptr)
      , fwd(afwd)
    {}

    inline void operator = (const WidgetIterator &w) noexcept { curr = w.curr; next = w.next; fwd = w.fwd; }

    inline WidgetIterator begin () noexcept { return WidgetIterator(*this); }
    inline WidgetIterator end () noexcept { return WidgetIterator(nullptr, fwd); }
    inline bool operator == (const WidgetIterator &b) const noexcept { return (curr == b.curr && fwd == b.fwd); }
    inline bool operator != (const WidgetIterator &b) const noexcept { return (curr != b.curr || fwd != b.fwd); }
    inline VWidget *operator * () const noexcept { return curr; } /* required for iterator */
    inline void operator ++ () noexcept { curr = next; next = (curr ? (fwd ? curr->NextWidget : curr->PrevWidget) : nullptr); } /* this is enough for iterator */
  };

  WidgetIterator fromFirstChild () const noexcept { return WidgetIterator(FirstChildWidget, true); }
  WidgetIterator fromLastChild () const noexcept { return WidgetIterator(LastChildWidget, false); }

public:
  // called by root widget in responder
  // DO NOT CALL MANUALLY!
  static void CleanupWidgets ();


  virtual void PostCtor () override; // this is called after defaults were blit

  // destroys all child widgets
  virtual void Init (VWidget *);
  virtual void Destroy () override;

  void DestroyAllChildren ();

  VRootWidget *GetRootWidget () noexcept;

  inline const VClipRect &GetClipRect () const noexcept { return ClipRect; }

  void ToDrawerCoords (float &x, float &y) const noexcept;
  void ToDrawerCoords (int &x, int &y) const noexcept;

  int ToDrawerX (int x) const noexcept;
  int ToDrawerY (int y) const noexcept;

  int FromDrawerX (int x) const noexcept;
  int FromDrawerY (int y) const noexcept;

  // call this to close widget instead of destroying it
  void Close ();

  // methods to move widget on top or bottom
  void Lower ();
  void Raise ();
  void MoveBefore (VWidget *Other);
  void MoveAfter (VWidget *Other);

  // methods to set position, size and scale
  inline void SetPos (int NewX, int NewY) { SetConfiguration(NewX, NewY, SizeWidth, SizeHeight, SizeScaleX, SizeScaleY); }
  inline void SetX (int NewX) { SetPos(NewX, PosY); }
  inline void SetY (int NewY) { SetPos(PosX, NewY); }
  inline void SetOfsX (int NewX) { if (OfsX != NewX) { OfsX = NewX; SetConfiguration(PosX, PosY, SizeWidth, SizeHeight, SizeScaleX, SizeScaleY, true); } }
  inline void SetOfsY (int NewY) { if (OfsY != NewY) { OfsY = NewY; SetConfiguration(PosX, PosY, SizeWidth, SizeHeight, SizeScaleX, SizeScaleY, true); } }
  inline void SetOffset (int NewX, int NewY) { if (OfsX != NewX || OfsY != NewY) { OfsX = NewX; OfsY = NewY; SetConfiguration(PosX, PosY, SizeWidth, SizeHeight, SizeScaleX, SizeScaleY, true); } }
  inline void SetSize (int NewWidth, int NewHeight) { SetConfiguration(PosX, PosY, NewWidth, NewHeight, SizeScaleX, SizeScaleY); }
  inline void SetWidth (int NewWidth) { SetSize(NewWidth, SizeHeight); }
  inline void SetHeight (int NewHeight) { SetSize(SizeWidth, NewHeight); }
  inline void SetScale (float NewScaleX, float NewScaleY) { SetConfiguration(PosX, PosY, SizeWidth, SizeHeight, NewScaleX, NewScaleY); }
  // returns `true` if something was changed
  bool SetConfiguration (int NewX, int NewY, int NewWidth, int HewHeight, float NewScaleX=1.0f, float NewScaleY=1.0f, bool Forced=false);

  inline float GetScaleX () const noexcept { return SizeScaleX; }
  inline float GetScaleY () const noexcept { return SizeScaleY; }

  inline int GetWidth () const noexcept { return SizeWidth; }
  inline int GetHeight () const noexcept { return SizeHeight; }

  inline int GetCursorChar () const noexcept { return CursorChar; }
  inline void SetCursorChar (int ch) noexcept { if (ch < 0) ch = '_'; CursorChar = ch; }

  inline bool IsWantMouseInput (bool bRecurse=true) const noexcept {
    if (IsGoingToDie()) return false;
    if (IsWantMouse()) return true;
    if (bRecurse) {
      for (auto &&w : fromLastChild()) {
        if (w->IsWantMouseInput(true)) return true;
      }
    }
    return false;
  }

  // visibility methods
  void SetVisibility (bool NewVisibility);
  inline void Show () { SetVisibility(true); }
  inline void Hide () { SetVisibility(false); }

  inline bool IsVisible (bool bRecurse=true) const noexcept {
    if (bRecurse) {
      const VWidget *pParent = this;
      while (pParent) {
        if (!(pParent->WidgetFlags&WF_IsVisible)) break;
        pParent = pParent->ParentWidget;
      }
      return (pParent ? false : true);
    } else {
      return !!(WidgetFlags&WF_IsVisible);
    }
  }

  // enable state methods
  void SetEnabled (bool NewEnabled);
  inline void Enable () { SetEnabled(true); }
  inline void Disable () { SetEnabled(false); }

  inline bool IsEnabled (bool bRecurse=true) const noexcept {
    if (bRecurse) {
      const VWidget *pParent = this;
      while (pParent) {
        if (!(pParent->WidgetFlags&WF_IsEnabled)) break;
        pParent = pParent->ParentWidget;
      }
      return (pParent ? false : true);
    } else {
      return !!(WidgetFlags&WF_IsEnabled);
    }
  }

  // focusable state methods
  void SetFocusable (bool NewFocusable);
  inline bool IsFocusable () const { return !!(WidgetFlags&WF_IsFocusable); }

  // this returns "deepest" focused child
  VWidget *FindFocused () noexcept {
    VWidget *w = this;
    while (w && w->CurrentFocusChild) w = w->CurrentFocusChild;
    return w;
  }

  // other focus methods
  void SetCurrentFocusChild (VWidget *NewFocus);
  inline VWidget *GetCurrentFocus () const { return CurrentFocusChild; }

  inline void SetFocus (bool onlyInParent=false) {
    if (!ParentWidget) return;
    if (onlyInParent) {
      ParentWidget->SetCurrentFocusChild(this);
    } else {
      for (VWidget *w = this; w->ParentWidget; w = w->ParentWidget) {
        w->ParentWidget->SetCurrentFocusChild(w);
      }
    }
  }

  inline bool IsFocused (bool Recurse=true) const noexcept {
    // root is always focused
    if (!ParentWidget) return true;
    if (Recurse) {
      const VWidget *W = this;
      while (W->ParentWidget && W->ParentWidget->CurrentFocusChild == W) W = W->ParentWidget;
      return !W->ParentWidget;
    } else {
      return (ParentWidget->CurrentFocusChild == this);
    }
  }

  void SetOnTop (bool v) noexcept;
  inline bool IsOnTop () noexcept { return IsOnTopFlag(); }

  void SetCloseOnBlur (bool v) noexcept;
  inline bool IsCloseOnBlur () noexcept { return IsCloseOnBlurFlag(); }

  inline bool IsModal () noexcept { return IsModalFlag(); }

  void DrawPicScaledIgnoreOffset (int X, int Y, int Handle, float scaleX=1.0f, float scaleY=1.0f, float Alpha=1.0f, int Trans=0);

  void DrawPic (int X, int Y, int Handle, float Alpha=1.0f, int Trans=0);
  void DrawPicScaled (int X, int Y, int Handle, float scaleX, float scaleY, float Alpha=1.0f, int Trans=0);
  void DrawPicScaled (int X, int Y, VTexture *Tex, float scaleX, float scaleY, float Alpha=1.0f, int Trans=0);
  void DrawPic (int X, int Y, VTexture *Tex, float Alpha=1.0f, int Trans=0);

  void DrawPicPart (float x, float y, float pwdt, float phgt, int handle, float alpha);
  // texture coords are in [0..1]
  void DrawPicPartEx (float x, float y, float tx0, float ty0, float tx1, float ty1, int handle, float alpha);

  // texture coords are in [0..1]
  // high byte of color is alpha
  void DrawPicRecolored (float x, float y, float tx0, float ty0, float tx1, float ty1, vuint32 color, int handle, float scaleX=1.0f, float scaleY=1.0f, bool ignoreOffset=false);
  void DrawPicShadow (float x, float y, float tx0, float ty0, float tx1, float ty1, int handle, float alpha=0.625f, float scaleX=1.0f, float scaleY=1.0f, bool ignoreOffset=false);

  void DrawShadowedPic (int X, int Y, int Handle, float scaleX=1.0f, float scaleY=1.0f, bool ignoreOffset=false);
  void DrawShadowedPic (int X, int Y, VTexture *Tex, float scaleX=1.0f, float scaleY=1.0f, bool ignoreOffset=false);

  void FillRectWithFlat (int X, int Y, int Width, int Height, VName Name);
  void FillRectWithFlatHandle (int X, int Y, int Width, int Height, int Handle);
  void FillRectWithFlatRepeat (int X, int Y, int Width, int Height, VName Name);
  void FillRectWithFlatRepeatHandle (int X, int Y, int Width, int Height, int Handle);
  void FillRect (int X, int Y, int Width, int Height, int color, float alpha);
  void FillRectF (float X1, float Y1, float Width, float Height, int color, float alpha);
  void ShadeRect (int X, int Y, int Width, int Height, float Shade);
  void ShadeRectF (float X1, float Y1, float Width, float Height, float Shade);
  void DrawRect (int X, int Y, int Width, int Height, int color, float alpha);
  void DrawRectF (float X, float Y, float Width, float Height, int color, float alpha);
  void DrawLine (int aX0, int aY0, int aX1, int aY1, int color, float alpha);
  void DrawLineF (float X1, float Y1, float X2, float Y2, int color, float alpha);

  VFont *GetFont () noexcept;
  void SetFont (VFont *AFont) noexcept;
  void SetFont (VName);
  void SetTextAlign (halign_e, valign_e);
  void SetTextShadow (bool);
  void DrawText (int x, int y, VStr String, int NormalColor, int BoldColor, float Alpha);
  int TextWidth (VStr);
  int TextHeight (VStr);
  int StringWidth (VStr);
  int FontHeight ();
  int CursorWidth ();
  void DrawCursor (int cursorColor=CR_UNTRANSLATED);
  void DrawCursorAt (int x, int y, int cursorChar, int cursorColor=CR_UNTRANSLATED);

  float CalcHexHeight (float h);
  void DrawHex (float x0, float y0, float w, float h, vuint32 color, float alpha=1.0f);
  void FillHex (float x0, float y0, float w, float h, vuint32 color, float alpha=1.0f);
  void ShadeHex (float x0, float y0, float w, float h, float darkening);

  bool IsPointInsideHex (float x, float y, float x0, float y0, float w, float h) noexcept;

  void CalcHexColorPatternDims (float *w, float *h, int radius, float cellW, float cellH);
  // returns `true` if the given "internal" hex pattern coordinates are valid
  bool IsValidHexColorPatternCoords (int hpx, int hpy, int radius);
  // calcs "internal" hex pattern coordinates, returns `false` if passed coords are outside any hex
  bool CalcHexColorPatternCoords (int *hpx, int *hpy, float x, float y, float x0, float y0, int radius, float cellW, float cellH);
  // calcs coordinates of the individual hex; returns `false` if the given "internal" hex pattern coordinates are not valid
  // those coords can be used in `*Hex()` methods
  bool CalcHexColorPatternHexCoordsAt (float *hx, float *hy, int hpx, int hpy, float x0, float y0, int radius, float cellW, float cellH);
  // returns opaque color at the given "internal" hex pattern coordinates (with high byte set), or 0
  int GetHexColorPatternColorAt (int hpx, int hpy, int radius);
  // draws hex pattern (WARNING: no clipping!)
  void DrawHexColorPattern (float x0, float y0, int radius, float cellW, float cellH);

  // cannot move diagonally yet
  void MoveHexCoords (int &hpx, int &hpy, int dx, int dy, int radius);

  // ignores `v`
  bool FindHexColorCoords (int *hpx, int *hpy, int radius, float h, float s);

  // returns text bounds with respect to the current text align
  void TextBounds (int x, int y, VStr String, int *x0, int *y0, int *width, int *height, bool trimTrailNL=true);

  inline void SetCursorPos (int ax, int ay) noexcept { LastX = ax; LastY = ay; }
  inline void SetCursorX (int v) noexcept { LastX = v; }
  inline void SetCursorY (int v) noexcept { LastY = v; }
  inline int GetCursorX () const noexcept { return LastX; }
  inline int GetCursorY () const noexcept { return LastY; }

  static VWidget *CreateNewWidget (VClass *, VWidget *);

  // events
  virtual void OnCreate () { static VMethodProxy method("OnCreate"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  virtual void OnDestroy () { static VMethodProxy method("OnDestroy"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  virtual void OnClose () { static VMethodProxy method("OnClose"); vobjPutParamSelf(); VMT_RET_VOID(method); }

  virtual void OnChildAdded (VWidget *Child) { static VMethodProxy method("OnChildAdded"); vobjPutParamSelf(Child); VMT_RET_VOID(method); }
  virtual void OnChildRemoved (VWidget *Child) { static VMethodProxy method("OnChildRemoved"); vobjPutParamSelf(Child); VMT_RET_VOID(method); }

  virtual void OnConfigurationChanged () { static VMethodProxy method("OnConfigurationChanged"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  virtual void OnVisibilityChanged (bool NewVisibility) { static VMethodProxy method("OnVisibilityChanged"); vobjPutParamSelf(NewVisibility); VMT_RET_VOID(method); }
  virtual void OnEnableChanged (bool bNewEnable) { static VMethodProxy method("OnEnableChanged"); vobjPutParamSelf(bNewEnable); VMT_RET_VOID(method); }
  virtual void OnFocusableChanged (bool bNewFocusable) { static VMethodProxy method("OnFocusableChanged"); vobjPutParamSelf(bNewFocusable); VMT_RET_VOID(method); }

  virtual void OnFocusReceived () { static VMethodProxy method("OnFocusReceived"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  virtual void OnFocusLost () { static VMethodProxy method("OnFocusLost"); vobjPutParamSelf(); VMT_RET_VOID(method); }

  virtual void OnDraw () { static VMethodProxy method("OnDraw"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  virtual void OnPostDraw () { static VMethodProxy method("OnPostDraw"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  virtual void Tick (float DeltaTime) { if (DeltaTime <= 0.0f) return; static VMethodProxy method("Tick"); vobjPutParamSelf(DeltaTime); VMT_RET_VOID(method); }
  virtual bool OnEvent (event_t *evt) { static VMethodProxy method("OnEvent"); if (!evt) return false; vobjPutParamSelf(evt); VMT_RET_BOOL(method); }
  virtual bool OnEventSink (event_t *evt) { static VMethodProxy method("OnEventSink"); if (!evt) return false; vobjPutParamSelf(evt); VMT_RET_BOOL(method); }

  // script natives
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
  DECLARE_FUNCTION(SetOffset)
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

  DECLARE_FUNCTION(SetOnTop)
  DECLARE_FUNCTION(IsOnTop)

  DECLARE_FUNCTION(SetCloseOnBlur)
  DECLARE_FUNCTION(IsCloseOnBlur)

  DECLARE_FUNCTION(IsModal)

  DECLARE_FUNCTION(SetFocusable)
  DECLARE_FUNCTION(IsFocusable)

  DECLARE_FUNCTION(SetCurrentFocusChild)
  DECLARE_FUNCTION(GetCurrentFocus)
  DECLARE_FUNCTION(IsFocused)
  DECLARE_FUNCTION(SetFocus)

  DECLARE_FUNCTION(DrawPicScaledIgnoreOffset)
  DECLARE_FUNCTION(DrawPic)
  DECLARE_FUNCTION(DrawPicScaled)
  DECLARE_FUNCTION(DrawShadowedPic)

  DECLARE_FUNCTION(DrawPicPart)
  DECLARE_FUNCTION(DrawPicPartEx)
  DECLARE_FUNCTION(DrawPicRecoloredEx)
  DECLARE_FUNCTION(DrawPicShadowEx)

  DECLARE_FUNCTION(FillRectWithFlat)
  DECLARE_FUNCTION(FillRectWithFlatHandle)
  DECLARE_FUNCTION(FillRectWithFlatRepeat)
  DECLARE_FUNCTION(FillRectWithFlatRepeatHandle)
  DECLARE_FUNCTION(FillRect)

  DECLARE_FUNCTION(DrawRect)
  DECLARE_FUNCTION(ShadeRect)
  DECLARE_FUNCTION(DrawLine)

  DECLARE_FUNCTION(CalcHexHeight)
  DECLARE_FUNCTION(DrawHex)
  DECLARE_FUNCTION(FillHex)
  DECLARE_FUNCTION(ShadeHex)

  DECLARE_FUNCTION(IsPointInsideHex)

  DECLARE_FUNCTION(CalcHexColorPatternWidth)
  DECLARE_FUNCTION(CalcHexColorPatternHeight)
  DECLARE_FUNCTION(IsValidHexColorPatternCoords)
  DECLARE_FUNCTION(CalcHexColorPatternCoords)
  DECLARE_FUNCTION(CalcHexColorPatternHexCoordsAt)
  DECLARE_FUNCTION(GetHexColorPatternColorAt)
  DECLARE_FUNCTION(DrawHexColorPattern)
  DECLARE_FUNCTION(FindHexColorCoords)

  DECLARE_FUNCTION(MoveHexCoords)

  DECLARE_FUNCTION(GetFont)
  DECLARE_FUNCTION(SetFont)
  DECLARE_FUNCTION(SetTextAlign)
  DECLARE_FUNCTION(SetTextShadow)
  DECLARE_FUNCTION(TextBounds)
  DECLARE_FUNCTION(TextWidth)
  DECLARE_FUNCTION(StringWidth)
  DECLARE_FUNCTION(TextHeight)
  DECLARE_FUNCTION(FontHeight)
  DECLARE_FUNCTION(SplitText)
  DECLARE_FUNCTION(SplitTextWithNewlines)
  DECLARE_FUNCTION(DrawText)
  DECLARE_FUNCTION(CursorWidth)
  DECLARE_FUNCTION(DrawCursor)
  DECLARE_FUNCTION(DrawCursorAt)
  DECLARE_FUNCTION(SetCursorPos)
  DECLARE_FUNCTION(get_CursorX)
  DECLARE_FUNCTION(get_CursorY)
  DECLARE_FUNCTION(set_CursorX)
  DECLARE_FUNCTION(set_CursorY)
  DECLARE_FUNCTION(FindTextColor)

  DECLARE_FUNCTION(TranslateXY)

  DECLARE_FUNCTION(GetWidgetAt)
};


#endif
