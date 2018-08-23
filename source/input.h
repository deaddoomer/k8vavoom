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
