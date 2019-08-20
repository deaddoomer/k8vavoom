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
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
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
class VRootWidget : public VWidget {
  DECLARE_CLASS(VRootWidget, VWidget, 0)
  NO_DEFAULT_CONSTRUCTOR(VRootWidget)

private:
  enum {
    // true if mouse cursor is currently enabled
    RWF_MouseEnabled = 0x0001,
  };
  vuint32 RootFlags;

  // current mouse cursor position
  vint32 MouseX;
  vint32 MouseY;

  // current mouse cursor graphic
  vint32 MouseCursorPic;

  void MouseMoveEvent (int, int);
  bool MouseButtonEvent (int, bool);

public:
  void Init ();
  virtual void Init (VWidget*) override { Sys_Error("Root canot have a parent"); }

  void DrawWidgets ();
  void TickWidgets (float DeltaTime);
  bool Responder (event_t *Event);

  void SetMouse (bool MouseOn);

  void RefreshScale ();

  static void StaticInit ();

  DECLARE_FUNCTION(SetMouse)
};

extern VRootWidget *GRoot;
