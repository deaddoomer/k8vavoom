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

// input device class, handles actual reading of the input
class VInputDevice : public VInterface {
public:
  // VInputDevice interface
  virtual void ReadInput() = 0;
  virtual void RegrabMouse () = 0; // called by UI when mouse cursor is turned off

  virtual void SetClipboardText (VStr text) = 0;
  virtual bool HasClipboardText () = 0;
  virtual VStr GetClipboardText () = 0;

  // implemented in corresponding system module
  static VInputDevice *CreateDevice ();
};


// input subsystem, handles all input events
class VInputPublic : public VInterface {
public:
  enum { MAX_KBCHEAT_LENGTH = 128 };

  struct CheatCode {
    VStr keys;
    VStr concmd;
  };

public:
  int ShiftDown;
  int CtrlDown;
  int AltDown;
  static TArray<CheatCode> kbcheats;
  static char currkbcheat[MAX_KBCHEAT_LENGTH+1];

  VInputPublic () : ShiftDown(0), CtrlDown(0), AltDown(0) { currkbcheat[0] = 0; }

  // system device related functions
  virtual void Init () = 0;
  virtual void Shutdown () = 0;

  // input event handling
  bool PostKeyEvent (int key, int press, vuint32 modflags);
  virtual void ProcessEvents () = 0;
  virtual int ReadKey () = 0;

  // handling of key bindings
  virtual void ClearBindings () = 0; // but not automap
  virtual void ClearAutomapBindings () = 0;
  // `isActive`: bit 0 for key1, bit 1 for key2
  virtual void GetBindingKeys (VStr Binding, int &Key1, int &Key2, VStr modSection, int strifemode, int *isActive) = 0;
  virtual void GetBindingKeysEx (VStr Binding, TArray<int> &keylist, VStr modSection, int strifemode) = 0;
  virtual void GetDefaultModBindingKeys (VStr Binding, TArray<int> &keylist, VStr modSection) = 0;
  virtual void GetBinding (int KeyNum, VStr &Down, VStr &Up) = 0; // for current game mode
  virtual void SetBinding (int KeyNum, VStr Down, VStr Up, VStr modSection, int strifemode, bool allowOverride=true) = 0;
  virtual void WriteBindings (VStream *st) = 0;
  virtual void AddActiveMod (VStr modSection) = 0;
  virtual void SetAutomapActive (bool active) = 0;

  // returns translated ASCII char, or unchanged keycode
  // it is safe to call this with any keycode
  // note that only ASCII chars are supported
  virtual int TranslateKey (int keycode) = 0;

  virtual int KeyNumForName (VStr Name) = 0;
  virtual VStr KeyNameForNum (int KeyNr) = 0;

  virtual void RegrabMouse () = 0; // called by UI when mouse cursor is turned off

  virtual void SetClipboardText (VStr text) = 0;
  virtual bool HasClipboardText () = 0;
  virtual VStr GetClipboardText () = 0;

  static void KBCheatClearAll ();
  static void KBCheatAppend (VStr keys, VStr concmd);
  static bool KBCheatProcessor (event_t *ev);

  static void UnpressAll ();

  virtual void UnpressAllInternal () = 0;

  static VInputPublic *Create ();
};


// global input handler
extern VInputPublic *GInput;
