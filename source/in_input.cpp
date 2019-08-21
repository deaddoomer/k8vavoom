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
//**
//**  Event handling
//**
//**  Events are asynchronous inputs generally generated by the game user.
//**  Events can be discarded if no responder claims them
//**
//**************************************************************************
#include "gamedefs.h"
#include "cl_local.h"
#include "ui/ui.h"
#include "neoui/neoui.h"


static VCvarB allow_vanilla_cheats("allow_vanilla_cheats", true, "Allow vanilla keyboard cheat codes?", CVAR_Archive);

extern void UnpressAllButtons ();


// ////////////////////////////////////////////////////////////////////////// //
// input subsystem, handles all input events
class VInput : public VInputPublic {
public:
  VInput ();
  virtual ~VInput () override;

  // system device related functions
  virtual void Init () override;
  virtual void Shutdown () override;

  // input event handling
  virtual void ProcessEvents () override;
  virtual int ReadKey () override;

  // handling of key bindings
  virtual void ClearBindings () override;
  virtual void GetBindingKeys (VStr Binding, int &Key1, int &Key2, int strifemode=0) override;
  virtual void GetBinding (int KeyNum, VStr &Down, VStr &Up) override;
  virtual void SetBinding (int KeyNum, VStr Down, VStr Up, bool Save=true, int strifemode=0) override;
  virtual void WriteBindings (VStream *st) override;

  virtual int TranslateKey (int ch) override;

  virtual int KeyNumForName (VStr Name) override;
  virtual VStr KeyNameForNum (int KeyNr) override;

  virtual void RegrabMouse () override; // called by UI when mouse cursor is turned off

  virtual void SetClipboardText (VStr text) override;
  virtual bool HasClipboardText () override;
  virtual VStr GetClipboardText () override;

private:
  VInputDevice *Device;

  int strifeMode = -666;

  inline const bool IsStrife () {
    if (strifeMode == -666) strifeMode = (game_name.asStr().ICmp("strife") == 0 ? 1 : 0);
    return strifeMode;
  }

  bool lastWasGameBinding = false;

  /*
  struct Binding {
    //VStr cmdDown;
    //VStr cmdUp;
    VStr cmdDownStrife;
    VStr cmdUpStrife;
    VStr cmdDownNonStrife;
    VStr cmdUpNonStrife;
    bool save;

    Binding () : cmdDownStrife(), cmdUpStrife(), cmdDownNonStrife(), cmdUpNonStrife(), save(false) {}

    bool IsEmpty () const {
      return
        cmdDownStrife.isEmpty() ||
        cmdUpStrife.isEmpty() ||
        cmdDownNonStrife.isEmpty() ||
        cmdUpNonStrife.isEmpty();
    }

    //bool IsStrifeOnly () const { return (IsEmpty() && (!cmdDownStrife.isEmpty() || !cmdUpStrife.isEmpty())); }
    //bool IsNonStrifeOnly () const { return (IsEmpty() && (!cmdDownNonStrife.isEmpty() || !cmdUpNonStrife.isEmpty())); }
  };
  Binding KeyBindings[256];
  */

  struct Binding {
    VStr cmdDown;
    VStr cmdUp;
    Binding () : cmdDown(), cmdUp() {}
    inline bool IsEmpty () const { return (cmdDown.isEmpty() && cmdUp.isEmpty()); }
    inline void Clear () { cmdDown.clear(); cmdUp.clear(); }
  };

  Binding KeyBindingsAll[256];
  Binding KeyBindingsStrife[256];
  Binding KeyBindingsNonStrife[256];
  bool KeyBindingsSave[256];

  VStr getBinding (bool down, int idx);

  static const char ShiftXForm[128];
};

VInputPublic *GInput;


// key shifting
const char VInput::ShiftXForm[128] = {
  0,
  1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
  11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
  21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
  31,
  ' ', '!', '"', '#', '$', '%', '&',
  '"', // shift-'
  '(', ')', '*', '+',
  '<', // shift-,
  '_', // shift--
  '>', // shift-.
  '?', // shift-/
  ')', // shift-0
  '!', // shift-1
  '@', // shift-2
  '#', // shift-3
  '$', // shift-4
  '%', // shift-5
  '^', // shift-6
  '&', // shift-7
  '*', // shift-8
  '(', // shift-9
  ':',
  ':', // shift-;
  '<',
  '+', // shift-=
  '>', '?', '@',
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
  'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  '{', // shift-[
  '|', // shift-backslash - OH MY GOD DOES WATCOM SUCK
  '}', // shift-]
  '"', '_',
  '\'', // shift-`
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
  'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  '{', '|', '}', '~', 127
};


TArray<VInputPublic::CheatCode> VInputPublic::kbcheats;
char VInputPublic::currkbcheat[VInputPublic::MAX_KBCHEAT_LENGTH+1];


//==========================================================================
//
//  VInputPublic::Create
//
//==========================================================================
VInputPublic *VInputPublic::Create () {
  return new VInput();
}


//==========================================================================
//
//  VInputPublic::KBCheatClearAll
//
//==========================================================================
void VInputPublic::KBCheatClearAll () {
  kbcheats.clear();
}


//==========================================================================
//
//  VInputPublic::KBCheatAppend
//
//==========================================================================
void VInputPublic::KBCheatAppend (VStr keys, VStr concmd) {
  if (keys.length() == 0) return;
  for (int f = 0; f < kbcheats.length(); ++f) {
    if (kbcheats[f].keys.ICmp(keys) == 0) {
      if (concmd.length() == 0) {
        kbcheats.removeAt(f);
      } else {
        kbcheats[f].concmd = concmd;
      }
      return;
    }
  }
  // reset cheat
  currkbcheat[0] = 0;
  CheatCode &cc = kbcheats.alloc();
  cc.keys = keys;
  cc.concmd = concmd;
  //GCon->Logf("added cheat code '%s' (command: '%s')", *keys, *concmd);
}


//==========================================================================
//
//  VInputPublic::KBCheatProcessor
//
//==========================================================================
bool VInputPublic::KBCheatProcessor (event_t *ev) {
  if (!allow_vanilla_cheats) { currkbcheat[0] = 0; return false; }
  if (ev->type != ev_keydown) return false;
  if (ev->data1 < ' ' || ev->data1 >= 127) { currkbcheat[0] = 0; return false; }
  int slen = (int)strlen(currkbcheat);
  if (slen >= MAX_KBCHEAT_LENGTH) { currkbcheat[0] = 0; return false; }
  currkbcheat[slen] = (char)ev->data1;
  currkbcheat[slen+1] = 0;
  ++slen;
  int clen = kbcheats.length();
  //GCon->Logf("C:<%s> (clen=%d)", currkbcheat, clen);
  char digits[MAX_KBCHEAT_LENGTH+1];
  for (int f = 0; f <= MAX_KBCHEAT_LENGTH; ++f) digits[f] = '0';
  int digcount = 0;
  bool wasCheat = false;
  for (int cnum = 0; cnum < clen; ++cnum) {
    CheatCode &cc = kbcheats[cnum];
    //GCon->Logf("  check00:<%s> (with <%s>)", *cc.keys, currkbcheat);
    if (cc.keys.length() < slen) continue; // too short
    bool ok = true;
    for (int f = 0; f < slen; ++f) {
      char c1 = currkbcheat[f];
      if (c1 >= 'A' && c1 <= 'Z') c1 = c1-'A'+'a';
      char c2 = cc.keys[f];
      if (c2 >= 'A' && c2 <= 'Z') c2 = c2-'A'+'a';
      if (c1 == c2) continue;
      if (c2 == '#' && (c1 >= '0' && c1 <= '9')) {
        digits[digcount++] = c1;
        continue;
      }
      ok = false;
      break;
    }
    //GCon->Logf("  check01:<%s> (with <%s>): ok=%d", *cc.keys, currkbcheat, (ok ? 1 : 0));
    if (!ok) continue;
    if (cc.keys.length() == slen) {
      // cheat complete
      VStr concmd;
      if (digcount > 0) {
        // preprocess console command
        int dignum = 0;
        for (int f = 0; f < cc.concmd.length(); ++f) {
          if (cc.concmd[f] == '#') {
            if (dignum < digcount) {
              concmd += digits[dignum++];
            } else {
              concmd += '0';
            }
          } else {
            concmd += cc.concmd[f];
          }
        }
      } else {
        concmd = cc.concmd;
      }
      if (concmd.length()) {
        if (concmd[concmd.length()-1] != '\n') concmd += "\n";
        GCmdBuf << concmd;
      }
      // reset cheat
      currkbcheat[0] = 0;
      return true;
    }
    wasCheat = true;
  }
  // nothing was found, reset
  if (!wasCheat) currkbcheat[0] = 0;
  return false;
}


//==========================================================================
//
//  VInputPublic::UnpressAll
//
//==========================================================================
void VInputPublic::UnpressAll () {
  UnpressAllButtons();
}


//==========================================================================
//
//  VInput::VInput
//
//==========================================================================
VInput::VInput () : Device(0) {
  memset(KeyBindingsSave, 0, sizeof(KeyBindingsSave));
  memset((void *)&KeyBindingsAll[0], 0, sizeof(KeyBindingsAll[0]));
  memset((void *)&KeyBindingsStrife[0], 0, sizeof(KeyBindingsStrife[0]));
  memset((void *)&KeyBindingsNonStrife[0], 0, sizeof(KeyBindingsNonStrife[0]));
}


//==========================================================================
//
//  VInput::~VInput
//
//==========================================================================
VInput::~VInput () {
  Shutdown();
}


//==========================================================================
//
//  VInput::ClearBindings
//
//==========================================================================
void VInput::ClearBindings () {
  for (int f = 0; f < 256; ++f) {
    KeyBindingsSave[f] = false;
    KeyBindingsAll[f].Clear();
    KeyBindingsStrife[f].Clear();
    KeyBindingsNonStrife[f].Clear();
  }
}


//==========================================================================
//
//  VInput::getBinding
//
//==========================================================================
VStr VInput::getBinding (bool down, int idx) {
  if (idx < 1 || idx > 255) return VStr::EmptyString;
  // for all games
  if (IsStrife()) {
    if (!KeyBindingsStrife[idx].IsEmpty()) {
      return (down ? KeyBindingsStrife[idx].cmdDown : KeyBindingsStrife[idx].cmdUp);
    }
  } else {
    if (!KeyBindingsNonStrife[idx].IsEmpty()) {
      return (down ? KeyBindingsNonStrife[idx].cmdDown : KeyBindingsNonStrife[idx].cmdUp);
    }
  }
  return (down ? KeyBindingsAll[idx].cmdDown : KeyBindingsAll[idx].cmdUp);
}


//==========================================================================
//
//  VInput::Init
//
//==========================================================================
void VInput::Init () {
  Device = VInputDevice::CreateDevice();
}


//==========================================================================
//
//  VInput::Shutdown
//
//==========================================================================
void VInput::Shutdown () {
  if (Device) {
    delete Device;
    Device = nullptr;
  }
}


//==========================================================================
//
//  VInputPublic::PostKeyEvent
//
//  Called by the I/O functions when a key or button is pressed or released
//
//==========================================================================
bool VInputPublic::PostKeyEvent (int key, int press, vuint32 modflags) {
  if (!key) return true; // always succeed
  event_t ev;
  memset((void *)&ev, 0, sizeof(event_t));
  ev.type = (press ? ev_keydown : ev_keyup);
  ev.data1 = key;
  ev.modflags = modflags;
  return VObject::PostEvent(ev);
}


//==========================================================================
//
//  VInput::ProcessEvents
//
//  Send all the Events of the given timestamp down the responder chain
//
//==========================================================================
void VInput::ProcessEvents () {
  bool reachedBinding = false;
  bool wasEvent = false;
  Device->ReadInput();
  for (int count = VObject::CountQueuedEvents(); count > 0; --count) {
    event_t ev;
    if (!VObject::GetEvent(&ev)) break;
    wasEvent = true;

    // shift key state
    if (ev.data1 == K_RSHIFT) { if (ev.type == ev_keydown) ShiftDown |= 1; else ShiftDown &= ~1; }
    if (ev.data1 == K_LSHIFT) { if (ev.type == ev_keydown) ShiftDown |= 2; else ShiftDown &= ~2; }
    // ctrl key state
    if (ev.data1 == K_RCTRL) { if (ev.type == ev_keydown) CtrlDown |= 1; else CtrlDown &= ~1; }
    if (ev.data1 == K_LCTRL) { if (ev.type == ev_keydown) CtrlDown |= 2; else CtrlDown &= ~2; }
    // alt key state
    if (ev.data1 == K_RALT) { if (ev.type == ev_keydown) AltDown |= 1; else AltDown &= ~1; }
    if (ev.data1 == K_LALT) { if (ev.type == ev_keydown) AltDown |= 2; else AltDown &= ~2; }

    if (C_Responder(&ev)) continue; // console
    if (NUI_Responder(&ev)) continue; // new UI
    if (CT_Responder(&ev)) continue; // chat
    if (MN_Responder(&ev)) continue; // menu
    if (GRoot->Responder(&ev)) continue; // root widget

    //k8: this hack prevents "keyup" to be propagated when console is active
    //    this should be in console responder, but...
    //if (C_Active() && (ev.type == ev_keydown || ev.type == ev_keyup)) continue;
    // actually, when console is active, it eats everything
    if (C_Active()) continue;

    reachedBinding = true;
    if (!lastWasGameBinding) {
      //GCon->Log("unpressing(0)...");
      lastWasGameBinding = true;
      UnpressAll();
    }

    if (cl && !GClGame->InIntermission()) {
      if (KBCheatProcessor(&ev)) continue; // cheatcode typed
      if (SB_Responder(&ev)) continue; // status window ate it
      if (AM_Responder(&ev)) continue; // automap ate it
    }

    if (F_Responder(&ev)) continue; // finale

    // key bindings
    if ((ev.type == ev_keydown || ev.type == ev_keyup) && (ev.data1 > 0 && ev.data1 < 256)) {
      //VStr kb;
      //if (isAllowed(ev.data1&0xff)) kb = (ev.type == ev_keydown ? KeyBindingsDown[ev.data1&0xff] : KeyBindingsUp[ev.data1&0xff]);
      VStr kb = getBinding((ev.type == ev_keydown), ev.data1&0xff);
      //GCon->Logf("KEY %s is %s; action is '%s'", *GInput->KeyNameForNum(ev.data1&0xff), (ev.type == ev_keydown ? "down" : "up"), *kb);
      if (kb.IsNotEmpty()) {
        if (kb[0] == '+' || kb[0] == '-') {
          // button commands add keynum as a parm
          if (kb.length() > 1) GCmdBuf << kb << " " << VStr(ev.data1) << "\n";
        } else {
          GCmdBuf << kb << "\n";
        }
        continue;
      }
    }

    if (CL_Responder(&ev)) continue;
  }

  if (wasEvent && !reachedBinding) {
    // something ate all the keys, so unpress buttons
    if (lastWasGameBinding) {
      //GCon->Log("unpressing(1)...");
      lastWasGameBinding = false;
      UnpressAll();
    }
  }
}


//==========================================================================
//
//  VInput::ReadKey
//
//==========================================================================
int VInput::ReadKey () {
  int ret = 0;
  do {
    Device->ReadInput();
    event_t ev;
    while (!ret && VObject::GetEvent(&ev)) {
      if (ev.type == ev_keydown) ret = ev.data1;
    }
  } while (!ret);
  return ret;
}


//==========================================================================
//
//  VInput::GetBindingKeys
//
//==========================================================================
void VInput::GetBindingKeys (VStr bindStr, int &Key1, int &Key2, int strifemode) {
  Key1 = -1;
  Key2 = -1;
  if (bindStr.isEmpty()) return;
  for (int i = 1; i < 256; ++i) {
    int kf = -1;
    if (strifemode < 0) {
      if (!KeyBindingsNonStrife[i].IsEmpty() && KeyBindingsNonStrife[i].cmdDown.ICmp(bindStr) == 0) kf = i;
    } else if (strifemode > 0) {
      if (!KeyBindingsStrife[i].IsEmpty() && KeyBindingsStrife[i].cmdDown.ICmp(bindStr) == 0) kf = i;
    } else {
      if (!KeyBindingsAll[i].IsEmpty() && KeyBindingsAll[i].cmdDown.ICmp(bindStr) == 0) kf = i;
    }
    if (kf > 0) {
      if (Key1 != -1) { Key2 = kf; return; }
      Key1 = kf;
    }
  }
}


//==========================================================================
//
//  VInput::GetBinding
//
//==========================================================================
void VInput::GetBinding (int KeyNum, VStr &Down, VStr &Up) {
  //Down = KeyBindingsDown[KeyNum];
  //Up = KeyBindingsUp[KeyNum];
  Down = getBinding(true, KeyNum);
  Up = getBinding(false, KeyNum);
}


//==========================================================================
//
//  VInput::SetBinding
//
//==========================================================================
void VInput::SetBinding (int KeyNum, VStr Down, VStr Up, bool Save, int strifemode) {
  if (KeyNum < 1 || KeyNum > 255) return;
  if (Down.IsEmpty() && Up.IsEmpty() && !KeyBindingsSave[KeyNum]) return;
  KeyBindingsSave[KeyNum] = Save;
  if (strifemode == 0) {
    KeyBindingsAll[KeyNum].cmdDown = Down;
    KeyBindingsAll[KeyNum].cmdUp = Up;
  } else if (strifemode < 0) {
    KeyBindingsNonStrife[KeyNum].cmdDown = Down;
    KeyBindingsNonStrife[KeyNum].cmdUp = Up;
  } else {
    KeyBindingsStrife[KeyNum].cmdDown = Down;
    KeyBindingsStrife[KeyNum].cmdUp = Up;
  }
}


//==========================================================================
//
//  VInput::WriteBindings
//
//  Writes lines containing "bind key value"
//
//==========================================================================
void VInput::WriteBindings (VStream *st) {
  st->writef("UnbindAll\n");
  for (int i = 1; i < 256; ++i) {
    if (!KeyBindingsSave[i]) continue;
    if (!KeyBindingsAll[i].IsEmpty()) st->writef("bind \"%s\" \"%s\" \"%s\"\n", *KeyNameForNum(i).quote(), *KeyBindingsAll[i].cmdDown.quote(), *KeyBindingsAll[i].cmdUp.quote());
    if (!KeyBindingsStrife[i].IsEmpty()) st->writef("bind strife \"%s\" \"%s\" \"%s\"\n", *KeyNameForNum(i).quote(), *KeyBindingsStrife[i].cmdDown.quote(), *KeyBindingsStrife[i].cmdUp.quote());
    if (!KeyBindingsNonStrife[i].IsEmpty()) st->writef("bind notstrife \"%s\" \"%s\" \"%s\"\n", *KeyNameForNum(i).quote(), *KeyBindingsNonStrife[i].cmdDown.quote(), *KeyBindingsNonStrife[i].cmdUp.quote());
  }
}


//==========================================================================
//
//  VInput::TranslateKey
//
//==========================================================================
int VInput::TranslateKey (int ch) {
  switch (ch) {
    case K_PADDIVIDE: return '/';
    case K_PADMULTIPLE: return '*';
    case K_PADMINUS: return '-';
    case K_PADPLUS: return '+';
    case K_PADDOT: return '.';
  }
  if (ch <= 0 || ch > 127) return ch;
  return (ShiftDown ? ShiftXForm[ch] : ch);
}


//==========================================================================
//
//  VInput::KeyNumForName
//
//  Searches in key names for given name
//  return key code
//
//==========================================================================
int VInput::KeyNumForName (VStr Name) {
  if (Name.IsEmpty()) return -1;
  int res = VObject::VKeyFromName(Name);
  if (res <= 0) res = -1;
  return res;
}


//==========================================================================
//
//  VInput::KeyNameForNum
//
//  Writes into given string key name
//
//==========================================================================
VStr VInput::KeyNameForNum (int KeyNr) {
       if (KeyNr == ' ') return "SPACE";
  else if (KeyNr == K_ESCAPE) return VStr("ESCAPE");
  else if (KeyNr == K_ENTER) return VStr("ENTER");
  else if (KeyNr == K_TAB) return VStr("TAB");
  else if (KeyNr == K_BACKSPACE) return VStr("BACKSPACE");
  else return VObject::NameFromVKey(KeyNr);
}


//==========================================================================
//
//  VInput::RegrabMouse
//
//  Called by UI when mouse cursor is turned off
//
//==========================================================================
void VInput::RegrabMouse () {
  if (Device) Device->RegrabMouse();
}


//==========================================================================
//
//  VInput::SetClipboardText
//
//==========================================================================
void VInput::SetClipboardText (VStr text) {
  if (Device) Device->SetClipboardText(text);
}


//==========================================================================
//
//  VInput::HasClipboardText
//
//==========================================================================
bool VInput::HasClipboardText () {
  return (Device ? Device->HasClipboardText() : false);
}


//==========================================================================
//
//  VInput::GetClipboardText
//
//==========================================================================
VStr VInput::GetClipboardText () {
  return (Device ? Device->GetClipboardText() : VStr::EmptyString);
}


//==========================================================================
//
//  COMMAND Unbind
//
//==========================================================================
COMMAND(Unbind) {
  int stidx = 1;
  int strifeFlag = 0;

  if (Args.length() > 1) {
         if (Args[1].ICmp("strife") == 0) { strifeFlag = 1; ++stidx; }
    else if (Args[1].ICmp("notstrife") == 0) { strifeFlag = -1; ++stidx; }
    else if (Args[1].ICmp("all") == 0) { strifeFlag = 0; ++stidx; }
  }

  if (Args.length() != stidx+1) {
    GCon->Log("unbind <key> : remove commands from a key");
    return;
  }

  int b = GInput->KeyNumForName(Args[stidx]);
  if (b == -1) {
    GCon->Logf("\"%s\" isn't a valid key", *Args[stidx].quote());
    return;
  }

  GInput->SetBinding(b, VStr(), VStr(), true, strifeFlag);
}


//==========================================================================
//
//  COMMAND UnbindAll
//
//==========================================================================
COMMAND(UnbindAll) {
  GInput->ClearBindings();
}


//==========================================================================
//
//  COMMAND Bind
//
//==========================================================================
static void bindCommon (const TArray<VStr> &Args, bool ParsingKeyConf) {
  const int argc = Args.length();

  if (argc != 2 && argc != 3 && argc != 4 && argc != 5) {
    GCon->Logf("%s [strife|nostrife|all] <key> [down_command] [up_command]: attach a command to a key", *Args[0]);
    return;
  }

  int strifeFlag = 0;
  int stidx = 1;

       if (Args[stidx].ICmp("strife") == 0) { strifeFlag = 1; ++stidx; }
  else if (Args[stidx].ICmp("notstrife") == 0) { strifeFlag = -1; ++stidx; }
  else if (Args[stidx].ICmp("all") == 0) { strifeFlag = 0; ++stidx; }

  int alen = argc-stidx;

  if (alen < 1) {
    GCon->Logf("%s [strife|nostrife|all] <key> [down_command] [up_command]: attach a command to a key", *Args[0]);
    return;
  }

  VStr kname = Args[stidx];

  if (kname.length() == 0) {
    GCon->Logf("%s: key name?", *Args[0]);
    return;
  }

  int b = GInput->KeyNumForName(kname);
  if (b == -1) {
    GCon->Logf(NAME_Error, "\"%s\" isn't a valid key", *kname.quote());
    return;
  }

  if (alen == 1) {
    VStr Down, Up;
    GInput->GetBinding(b, Down, Up);
    if (Down.IsNotEmpty() || Up.IsNotEmpty()) {
      if (Up.IsNotEmpty()) {
        GCon->Logf("%s \"%s\" \"%s\" \"%s\"", *Args[0], *kname.quote(), *Down.quote(), *Up.quote());
      } else {
        GCon->Logf("%s \"%s\" \"%s\" \"\"", *Args[0], *kname.quote(), *Down.quote());
      }
    } else {
      GCon->Logf("\"%s\" is not bound", *kname.quote());
    }
  } else {
    GInput->SetBinding(b, Args[stidx+1], (alen > 2 ? Args[stidx+2] : VStr()), !ParsingKeyConf, strifeFlag);
  }
}


//==========================================================================
//
//  COMMAND Bind
//
//==========================================================================
COMMAND(Bind) {
  bindCommon(Args, ParsingKeyConf);
}


//==========================================================================
//
//  COMMAND DefaultBind
//
//==========================================================================
COMMAND(DefaultBind) {
  bindCommon(Args, ParsingKeyConf);
}
