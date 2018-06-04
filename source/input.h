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

//  Keys and buttons
enum
{
  K_UPARROW = 0x80,
  K_LEFTARROW,
  K_RIGHTARROW,
  K_DOWNARROW,
  K_INSERT,
  K_DELETE,
  K_HOME,
  K_END,
  K_PAGEUP,
  K_PAGEDOWN,

  K_PAD0,
  K_PAD1,
  K_PAD2,
  K_PAD3,
  K_PAD4,
  K_PAD5,
  K_PAD6,
  K_PAD7,
  K_PAD8,
  K_PAD9,

  K_NUMLOCK,
  K_PADDIVIDE,
  K_PADMULTIPLE,
  K_PADMINUS,
  K_PADPLUS,
  K_PADENTER,
  K_PADDOT,

  K_ESCAPE,
  K_ENTER,
  K_TAB,
  K_BACKSPACE,
  K_CAPSLOCK,

  K_F1,
  K_F2,
  K_F3,
  K_F4,
  K_F5,
  K_F6,
  K_F7,
  K_F8,
  K_F9,
  K_F10,
  K_F11,
  K_F12,

  K_LSHIFT,
  K_RSHIFT,
  K_LCTRL,
  K_RCTRL,
  K_LALT,
  K_RALT,

  K_LWIN,
  K_RWIN,
  K_MENU,

  K_PRINTSCRN,
  K_SCROLLLOCK,
  K_PAUSE,

  K_ABNT_C1,
  K_YEN,
  K_KANA,
  K_CONVERT,
  K_NOCONVERT,
  K_AT,
  K_CIRCUMFLEX,
  K_COLON2,
  K_KANJI,

  K_MOUSE1,
  K_MOUSE2,
  K_MOUSE3,

  K_MOUSED1,
  K_MOUSED2,
  K_MOUSED3,

  K_MWHEELUP,
  K_MWHEELDOWN,

  K_JOY1,
  K_JOY2,
  K_JOY3,
  K_JOY4,
  K_JOY5,
  K_JOY6,
  K_JOY7,
  K_JOY8,
  K_JOY9,
  K_JOY10,
  K_JOY11,
  K_JOY12,
  K_JOY13,
  K_JOY14,
  K_JOY15,
  K_JOY16,

  KEY_COUNT,
  SCANCODECOUNT = KEY_COUNT - 0x80
};

//  Input event types.
enum evtype_t
{
  ev_keydown,
  ev_keyup,
  ev_mouse,
  ev_joystick
};

//  Event structure.
struct event_t
{
  evtype_t  type;   // event type
  int     data1;    // keys / mouse/joystick buttons
  int     data2;    // mouse/joystick x move
  int     data3;    // mouse/joystick y move
};

//
//  Input device class, handles actual reading of the input.
//
class VInputDevice : public VInterface
{
public:
  //  VInputDevice interface.
  virtual void ReadInput() = 0;
  virtual void RegrabMouse () = 0; // called by UI when mouse cursor is turned off

  //  Implemented in corresponding system module.
  static VInputDevice *CreateDevice();
};

//
//  Input subsystem, handles all input events.
//
class VInputPublic : public VInterface
{
public:
  int       ShiftDown;
  int       CtrlDown;
  int       AltDown;

  VInputPublic()
  : ShiftDown(0)
  , CtrlDown(0)
  , AltDown(0)
  {}

  //  System device related functions.
  virtual void Init() = 0;
  virtual void Shutdown() = 0;

  //  Input event handling.
  virtual void PostEvent(event_t*) = 0;
  virtual void KeyEvent(int, int) = 0;
  virtual void ProcessEvents() = 0;
  virtual int ReadKey() = 0;

  //  Handling of key bindings.
  virtual void GetBindingKeys(const VStr&, int&, int&) = 0;
  virtual void GetBinding(int, VStr&, VStr&) = 0;
  virtual void SetBinding(int, const VStr&, const VStr&, bool = true) = 0;
  virtual void WriteBindings(FILE*) = 0;

  virtual int TranslateKey(int) = 0;

  virtual int KeyNumForName(const VStr &Name) = 0;
  virtual VStr KeyNameForNum(int KeyNr) = 0;

  virtual void RegrabMouse () = 0; // called by UI when mouse cursor is turned off

  static VInputPublic *Create();
};

//  Global input handler.
extern VInputPublic *GInput;
