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
//**  Copyright (C) 2018-2022 Ketmar Dark
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
#ifdef VAVOOM_CUSTOM_SPECIAL_SDL
# include <SDL.h>
#else
# include <SDL2/SDL.h>
#endif
#include "gamedefs.h"
#include "drawer.h"
#include "input.h"
#include "touch.h"
#include "widgets/ui.h"
#include "psim/p_player.h"
#include "client/client.h"
#include "filesys/files.h"

//#define SDL_MOUSE_CAPTURE_DEBUG


// k8: joysticks have 16 buttons; no, really, you don't need more

#define VSDL_JINIT  (SDL_INIT_JOYSTICK|SDL_INIT_GAMECONTROLLER)


static int cli_NoMouse = 0;
static int cli_NoJoy = 0;
static int cli_NoCotl = 0;

/*static*/ bool cliRegister_input_args =
  VParsedArgs::RegisterFlagSet("-nomouse", "Disable mouse controls", &cli_NoMouse) &&
  VParsedArgs::RegisterFlagSet("-nojoystick", "Disable joysticks", &cli_NoJoy) &&
  VParsedArgs::RegisterFlagReset("-usejoystick", "Enable joysticks", &cli_NoJoy) &&
  VParsedArgs::RegisterFlagSet("-nocontroller", "Disable controllers", &cli_NoCotl) &&
  VParsedArgs::RegisterFlagReset("-usecontroller", "Enable controllers", &cli_NoCotl) &&
  VParsedArgs::RegisterAlias("-nojoy", "-nojoystick") &&
  VParsedArgs::RegisterAlias("-joy", "-usejoystick") &&
  VParsedArgs::RegisterAlias("-noctrl", "-nocontroller") &&
  VParsedArgs::RegisterAlias("-ctrl", "-usecontroller");


// ////////////////////////////////////////////////////////////////////////// //
class VSdlInputDevice : public VInputDevice {
public:
  VSdlInputDevice ();
  ~VSdlInputDevice ();

  virtual void ReadInput () override;
  virtual void RegrabMouse () override; // called by UI when mouse cursor is turned off

  virtual void SetClipboardText (VStr text) override;
  virtual bool HasClipboardText () override;
  virtual VStr GetClipboardText () override;

private:
  bool mouse; // is mouse enabled?
  bool winactive; // is current window active?
  bool firsttime; // when we just grabbed the mouse, we should ignore the first mouse event
  bool uiwasactive; // was UI active last time we polled the input?
  bool uimouselast; // was mouse "logically grabbed" last time we polled the input?
  bool curHidden; // is real mouse cursor hidden?

  bool currDoGrab; // should we grab the mouse? (cached "m_grab" value)
  bool currRelative; // should we use relative mode? (cached "m_relative" value)
  bool relativeFailed; // did setting relative mode failed at least once?

  float hlRemX;
  float hlRemY;

  int mouseLastX;
  int mouseLastY;

  vuint32 curmodflags;

  double timeKbdAllow;
  bool kbdSuspended; // if set to `true`, wait until `timeKbdAllow` to resume keyboard processing
  bool hyperDown;

  SDL_Joystick *joystick;
  SDL_GameController *controller;
  SDL_Haptic *haptic;
  SDL_JoystickID jid;
  bool joystick_started;
  bool joystick_controller;
  bool has_haptic;
  int joy_num_buttons;
  int joy_x[2];
  int joy_y[2];
  uint32_t joy_newb; // bitmask
  int joy_oldx[2];
  int joy_oldy[2];
  uint32_t joy_oldb; // bitmask
  uint32_t ctl_trigger; // bitmask
  int joynum; // also, controller number too

  // deletes stream; it is ok to pass `nullptr`
  void LoadControllerMappings (VStream *st);

  void StartupJoystick ();
  void ShutdownJoystick (bool closesubsys);
  void PostJoystick ();

  void HideRealMouse ();
  void ShowRealMouse ();

  void DoUnpress ();
  void CtlTriggerButton (int idx, bool down);

  // must be called after `StartupJoystick()`
  void OpenJoystick (int jnum);

  void OwnMouse ();
  void DisownMouse ();

public:
  bool bGotCloseRequest; // used in `CheckForEscape()`

  bool CheckForEscape ();
};


//==========================================================================
//
//  GNetCheckForUserAbortCB
//
//==========================================================================
static bool GNetCheckForUserAbortCB (void *udata) {
  VSdlInputDevice *drv = (VSdlInputDevice *)udata;
  return drv->CheckForEscape();
}


// ////////////////////////////////////////////////////////////////////////// //
VCvarB ui_freemouse("ui_freemouse", false, "Don't pass mouse movement to the camera. Used in various debug modes.", CVAR_Hidden|CVAR_NoShadow);
VCvarB ui_want_mouse_at_zero("ui_want_mouse_at_zero", false, "Move real mouse cursor to (0,0) when UI activated?", CVAR_Archive|CVAR_NoShadow);
VCvarB ui_mouse_forced("ui_mouse_forced", false, "Forge-grab mouse for UI?", CVAR_Hidden|CVAR_NoShadow);
static VCvarB ui_mouse("ui_mouse", false, "Allow using mouse in UI?", CVAR_Archive|CVAR_NoShadow);
static VCvarB ui_active("ui_active", false, "Is UI active (used to stop mouse warping if \"ui_mouse\" is false)?", CVAR_Hidden|CVAR_NoShadow);
static VCvarB ui_control_waiting("ui_control_waiting", false, "Waiting for new control key (pass mouse buttons)?", CVAR_Hidden|CVAR_NoShadow);

static VCvarB m_dbg_cursor("m_dbg_cursor", false, "Do not hide (true) mouse cursor on startup?", CVAR_PreInit|CVAR_Hidden|CVAR_NoShadow);
static VCvarB m_dbg_motion("m_dbg_motion", false, "Dump motion events?", CVAR_PreInit|CVAR_Hidden|CVAR_NoShadow);

static VCvarB m_grab("m_grab", true, "Grab mouse?", CVAR_Archive|CVAR_NoShadow);
static VCvarB m_relative("m_relative", true, "Use relative mouse motion events?", CVAR_Archive|CVAR_NoShadow);

static VCvarI ms_rel_mode("ms_rel_mode", "1", "Relative acceleration mode (0: none; 1: hl-like).", CVAR_Archive|CVAR_NoShadow);

static VCvarF ms_rel_exponent("ms_rel_exponent", "1", "Relative acceletation distance exponent.", CVAR_Archive|CVAR_NoShadow);
static VCvarF ms_rel_senscap("ms_rel_senscap", "2.7", "Relative acceletation sensitivity cap.", CVAR_Archive|CVAR_NoShadow);
static VCvarF ms_rel_sensitivity("ms_rel_sensitivity", "0.8", "Relative acceletation sensitivity.", CVAR_Archive|CVAR_NoShadow);
static VCvarF ms_rel_sensscale("ms_rel_sensscale", "0.2", "Relative acceletation sensitivity scale.", CVAR_Archive|CVAR_NoShadow);

static VCvarB ms_rel_squaredist("ms_rel_squaredist", false, "Use squared distance for relative acceletation?", CVAR_Archive|CVAR_NoShadow);
static VCvarB ms_rel_expweird("ms_rel_expweird", false, "Use different exponent formula for relative acceleration?", CVAR_Archive|CVAR_NoShadow);

static VCvarF ctl_deadzone_leftstick_x("ctl_deadzone_leftstick_x", "0.08", "Dead zone for left stick (horizontal motion) -- [0..1].", CVAR_Archive|CVAR_NoShadow);
static VCvarF ctl_deadzone_leftstick_y("ctl_deadzone_leftstick_y", "0.08", "Dead zone for left stick (vertical motion) -- [0..1].", CVAR_Archive|CVAR_NoShadow);
static VCvarF ctl_deadzone_rightstick_x("ctl_deadzone_rightstick_x", "0.08", "Dead zone for right stick (horizontal motion) -- [0..1].", CVAR_Archive|CVAR_NoShadow);
static VCvarF ctl_deadzone_rightstick_y("ctl_deadzone_rightstick_y", "0.08", "Dead zone for right stick (vertical motion) -- [0..1].", CVAR_Archive|CVAR_NoShadow);
static VCvarF ctl_trigger_left_edge("ctl_trigger_left_edge", "0.8", "Minimal level for registering trigger A -- [0..1].", CVAR_Archive|CVAR_NoShadow);
static VCvarF ctl_trigger_right_edge("ctl_trigger_right_edge", "0.8", "Minimal level for registering trigger B -- [0..1].", CVAR_Archive|CVAR_NoShadow);

static inline bool IsUIMouse () noexcept { return (ui_mouse.asBool() || ui_mouse_forced.asBool()); }

#ifdef VAVOOM_K8_DEVELOPER
# define IN_HYPER_BLOCK_DEFAULT      true
# define IN_FOCUSGAIN_DELAY_DEFAULT  "0.05"
#else
# define IN_HYPER_BLOCK_DEFAULT      false
# define IN_FOCUSGAIN_DELAY_DEFAULT  "0"
#endif
static VCvarF in_focusgain_delay("in_focusgain_delay", IN_FOCUSGAIN_DELAY_DEFAULT, "Delay before resume keyboard processing after focus gain (seconds).", CVAR_Archive|CVAR_NoShadow);
static VCvarB in_hyper_block("in_hyper_block", IN_HYPER_BLOCK_DEFAULT, "Block keyboard input when `Hyper` is pressed.", CVAR_Archive|CVAR_NoShadow);

//extern VCvarB screen_fsmode;
extern VCvarB gl_current_screen_fsmode;


// ////////////////////////////////////////////////////////////////////////// //
static int sdl2TranslateKey (SDL_Scancode scan) {
  if (scan >= SDL_SCANCODE_A && scan <= SDL_SCANCODE_Z) return (int)(scan-SDL_SCANCODE_A+'a');
  if (scan >= SDL_SCANCODE_1 && scan <= SDL_SCANCODE_9) return (int)(scan-SDL_SCANCODE_1+'1');

  switch (scan) {
    case SDL_SCANCODE_0: return '0';
    case SDL_SCANCODE_SPACE: return ' ';
    case SDL_SCANCODE_MINUS: return '-';
    case SDL_SCANCODE_EQUALS: return '=';
    case SDL_SCANCODE_LEFTBRACKET: return '[';
    case SDL_SCANCODE_RIGHTBRACKET: return ']';
    case SDL_SCANCODE_BACKSLASH: return '\\';
    case SDL_SCANCODE_SEMICOLON: return ';';
    case SDL_SCANCODE_APOSTROPHE: return '\'';
    case SDL_SCANCODE_COMMA: return ',';
    case SDL_SCANCODE_PERIOD: return '.';
    case SDL_SCANCODE_SLASH: return '/';

    case SDL_SCANCODE_UP: return K_UPARROW;
    case SDL_SCANCODE_LEFT: return K_LEFTARROW;
    case SDL_SCANCODE_RIGHT: return K_RIGHTARROW;
    case SDL_SCANCODE_DOWN: return K_DOWNARROW;
    case SDL_SCANCODE_INSERT: return K_INSERT;
    case SDL_SCANCODE_DELETE: return K_DELETE;
    case SDL_SCANCODE_HOME: return K_HOME;
    case SDL_SCANCODE_END: return K_END;
    case SDL_SCANCODE_PAGEUP: return K_PAGEUP;
    case SDL_SCANCODE_PAGEDOWN: return K_PAGEDOWN;

    case SDL_SCANCODE_KP_0: return K_PAD0;
    case SDL_SCANCODE_KP_1: return K_PAD1;
    case SDL_SCANCODE_KP_2: return K_PAD2;
    case SDL_SCANCODE_KP_3: return K_PAD3;
    case SDL_SCANCODE_KP_4: return K_PAD4;
    case SDL_SCANCODE_KP_5: return K_PAD5;
    case SDL_SCANCODE_KP_6: return K_PAD6;
    case SDL_SCANCODE_KP_7: return K_PAD7;
    case SDL_SCANCODE_KP_8: return K_PAD8;
    case SDL_SCANCODE_KP_9: return K_PAD9;

    case SDL_SCANCODE_NUMLOCKCLEAR: return K_NUMLOCK;
    case SDL_SCANCODE_KP_DIVIDE: return K_PADDIVIDE;
    case SDL_SCANCODE_KP_MULTIPLY: return K_PADMULTIPLE;
    case SDL_SCANCODE_KP_MINUS: return K_PADMINUS;
    case SDL_SCANCODE_KP_PLUS: return K_PADPLUS;
    case SDL_SCANCODE_KP_ENTER: return K_PADENTER;
    case SDL_SCANCODE_KP_PERIOD: return K_PADDOT;

    case SDL_SCANCODE_ESCAPE: return K_ESCAPE;
    case SDL_SCANCODE_RETURN: return K_ENTER;
    case SDL_SCANCODE_TAB: return K_TAB;
    case SDL_SCANCODE_BACKSPACE: return K_BACKSPACE;

    case SDL_SCANCODE_GRAVE: return K_BACKQUOTE;
    case SDL_SCANCODE_CAPSLOCK: return K_CAPSLOCK;

    case SDL_SCANCODE_F1: return K_F1;
    case SDL_SCANCODE_F2: return K_F2;
    case SDL_SCANCODE_F3: return K_F3;
    case SDL_SCANCODE_F4: return K_F4;
    case SDL_SCANCODE_F5: return K_F5;
    case SDL_SCANCODE_F6: return K_F6;
    case SDL_SCANCODE_F7: return K_F7;
    case SDL_SCANCODE_F8: return K_F8;
    case SDL_SCANCODE_F9: return K_F9;
    case SDL_SCANCODE_F10: return K_F10;
    case SDL_SCANCODE_F11: return K_F11;
    case SDL_SCANCODE_F12: return K_F12;

    case SDL_SCANCODE_LSHIFT: return K_LSHIFT;
    case SDL_SCANCODE_RSHIFT: return K_RSHIFT;
    case SDL_SCANCODE_LCTRL: return K_LCTRL;
    case SDL_SCANCODE_RCTRL: return K_RCTRL;
    case SDL_SCANCODE_LALT: return K_LALT;
    case SDL_SCANCODE_RALT: return K_RALT;

    case SDL_SCANCODE_LGUI: return K_LWIN;
    case SDL_SCANCODE_RGUI: return K_RWIN;
    case SDL_SCANCODE_MENU: return K_MENU;

    case SDL_SCANCODE_PRINTSCREEN: return K_PRINTSCRN;
    case SDL_SCANCODE_SCROLLLOCK: return K_SCROLLLOCK;
    case SDL_SCANCODE_PAUSE: return K_PAUSE;

    default:
      //if (scan >= ' ' && scan < 127) return (vuint8)scan;
      break;
  }

  return 0;
}


//==========================================================================
//
//  VSdlInputDevice::VSdlInputDevice
//
//==========================================================================
VSdlInputDevice::VSdlInputDevice ()
  : mouse(false)
  , winactive(false)
  , firsttime(true)
  , uiwasactive(false)
  , uimouselast(false)
  , curHidden(false)
  , currDoGrab(false)
  , currRelative(false)
  , relativeFailed(false)
  , hlRemX(0.0f)
  , hlRemY(0.0f)
  , mouseLastX(0)
  , mouseLastY(0)
  , curmodflags(0)
  , timeKbdAllow(0)
  , kbdSuspended(false)
  , hyperDown(false)
  , joystick(nullptr)
  , controller(nullptr)
  , haptic(nullptr)
  , jid(0)
  , joystick_started(false)
  , joystick_controller(false)
  , has_haptic(false)
  , joy_num_buttons(0)
  , bGotCloseRequest(false)
{
  memset(&joy_x[0], 0, 2*sizeof(joy_x[0]));
  memset(&joy_y[0], 0, 2*sizeof(joy_y[0]));
  memset(&joy_oldx[0], 0, 2*sizeof(joy_oldx[0]));
  memset(&joy_oldy[0], 0, 2*sizeof(joy_oldy[0]));
  joy_newb = joy_oldb = 0;
  ctl_trigger = 0;
  joynum = -1;
  // mouse and keyboard are setup using SDL's video interface
  mouse = true;
  if (cli_NoMouse) {
    SDL_EventState(SDL_MOUSEMOTION,     SDL_IGNORE);
    SDL_EventState(SDL_MOUSEBUTTONDOWN, SDL_IGNORE);
    SDL_EventState(SDL_MOUSEBUTTONUP,   SDL_IGNORE);
    mouse = false;
  } else {
    if (Drawer) {
      OwnMouse();
    } else {
      // do it later
      mouseLastX = mouseLastY = 0;
      currDoGrab = !m_grab.asBool();
      currRelative = !m_relative.asBool();
    }
  }

  // always off
  HideRealMouse();

  // initialise joystick
  StartupJoystick();

  CL_SetNetAbortCallback(&GNetCheckForUserAbortCB, (void *)this);
}


//==========================================================================
//
//  VSdlInputDevice::~VSdlInputDevice
//
//==========================================================================
VSdlInputDevice::~VSdlInputDevice () {
  CL_SetNetAbortCallback(nullptr, nullptr);
  //SDL_SetRelativeMouseMode(SDL_FALSE);
  SDL_ShowCursor(1); // on
  ShutdownJoystick(true);
}


//==========================================================================
//
//  VSdlInputDevice::OwnMouse
//
//==========================================================================
void VSdlInputDevice::OwnMouse () {
  if (Drawer) {
    currDoGrab = m_grab.asBool();
    if (currDoGrab) {
      if (SDL_CaptureMouse(SDL_TRUE) != 0) {
        #ifdef SDL_MOUSE_CAPTURE_DEBUG
        GCon->Log(NAME_Debug, "SDL: cannot capture mouse.");
        #endif
      } else {
        #ifdef SDL_MOUSE_CAPTURE_DEBUG
        GCon->Log(NAME_Debug, "SDL: mouse captured.");
        #endif
      }
    }
    currRelative = (!relativeFailed && m_relative.asBool());
    if (currRelative) {
      if (SDL_SetRelativeMouseMode(SDL_TRUE) != 0) {
        GCon->Log(NAME_Warning, "SDL: cannot switch mouse to relative mode.");
        currRelative = false;
        relativeFailed = true;
      } else {
        #ifdef SDL_MOUSE_CAPTURE_DEBUG
        GCon->Log(NAME_Debug, "SDL: switched mouse to relative mode.");
        #endif
      }
    }
  }
  // we don't need relative mouse motion in non-relative mode
  SDL_EventState(SDL_MOUSEMOTION, (currRelative ? SDL_ENABLE : SDL_IGNORE));
  firsttime = true;
  if (!currRelative && Drawer) {
    Drawer->WarpMouseToWindowCenter();
    Drawer->GetMousePosition(&mouseLastX, &mouseLastY);
  }
}


//==========================================================================
//
//  VSdlInputDevice::DisownMouse
//
//==========================================================================
void VSdlInputDevice::DisownMouse () {
  // it is ok to release everything, why not
  SDL_SetRelativeMouseMode(SDL_FALSE);
  SDL_CaptureMouse(SDL_FALSE);
  SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
  firsttime = true;
  currDoGrab = false;
  currRelative = false;
}


//==========================================================================
//
//  VSdlInputDevice::DoUnpress
//
//==========================================================================
void VSdlInputDevice::DoUnpress () {
  memset(&joy_x[0], 0, 2*sizeof(joy_x[0]));
  memset(&joy_y[0], 0, 2*sizeof(joy_y[0]));
  memset(&joy_oldx[0], 0, 2*sizeof(joy_oldx[0]));
  memset(&joy_oldy[0], 0, 2*sizeof(joy_oldy[0]));
  joy_newb = joy_oldb = 0;
  ctl_trigger = 0;
  VInputPublic::UnpressAll();
  curmodflags = 0; // just in case
}


//==========================================================================
//
//  VSdlInputDevice::CtlTriggerButton
//
//==========================================================================
void VSdlInputDevice::CtlTriggerButton (int idx, bool down) {
  if (idx < 0 || idx > 1) return;
  const uint32_t mask = 1u<<idx;
  if (down) {
    // pressed
    if ((ctl_trigger&mask) == 0) {
      ctl_trigger |= mask;
      GInput->PostKeyEvent(K_BUTTON_TRIGGER_LEFT+idx, true, curmodflags);
    }
  } else {
    // released
    if ((ctl_trigger&mask) != 0) {
      ctl_trigger &= ~mask;
      GInput->PostKeyEvent(K_BUTTON_TRIGGER_LEFT+idx, false, curmodflags);
    }
  }
}


//==========================================================================
//
//  VSdlInputDevice::HideRealMouse
//
//==========================================================================
void VSdlInputDevice::HideRealMouse () {
  if (!curHidden) {
    // real mouse cursor is visible
    if (m_dbg_cursor || !mouse) return;
    curHidden = true;
    SDL_ShowCursor(0);
  } else {
    // real mouse cursor is invisible
    if (m_dbg_cursor || !mouse) {
      curHidden = false;
      SDL_ShowCursor(1);
    }
  }
}


//==========================================================================
//
//  VSdlInputDevice::ShowRealMouse
//
//==========================================================================
void VSdlInputDevice::ShowRealMouse () {
  if (curHidden) {
    // real mouse cursor is invisible
    curHidden = false;
    SDL_ShowCursor(1);
  }
}


//==========================================================================
//
//  VSdlInputDevice::RegrabMouse
//
//  Called by UI when mouse cursor is turned off.
//
//==========================================================================
void VSdlInputDevice::RegrabMouse () {
  //FIXME: ignore winactive here, 'cause when mouse is off-window, it may be `false`
  if (mouse) {
    firsttime = true;
    if (Drawer) {
      if (relativeFailed || !m_relative.asBool()) {
        Drawer->WarpMouseToWindowCenter();
        Drawer->GetMousePosition(&mouseLastX, &mouseLastY);
      }
    } else {
      SDL_GetMouseState(&mouseLastX, &mouseLastY);
    }
  }
}


//==========================================================================
//
//  VSdlInputDevice::SetClipboardText
//
//==========================================================================
void VSdlInputDevice::SetClipboardText (VStr text) {
  if (text.length() && !text.IsValidUtf8()) {
    VStr s2 = text.Latin1ToUtf8();
    SDL_SetClipboardText(s2.getCStr());
  } else {
    SDL_SetClipboardText(text.getCStr());
  }
}


//==========================================================================
//
//  VSdlInputDevice::HasClipboardText
//
//==========================================================================
bool VSdlInputDevice::HasClipboardText () {
  return !!SDL_HasClipboardText();
}


//==========================================================================
//
//  VSdlInputDevice::GetClipboardText
//
//==========================================================================
VStr VSdlInputDevice::GetClipboardText () {
  char *text = SDL_GetClipboardText();
  if (!text || !text[0]) return VStr::EmptyString;
  for (char *p = text; *p; ++p) {
    const char ch = *p;
         if ((unsigned)(ch&0xff) <= 0 || (unsigned)(ch&0xff) > 127) *p = '?';
    else if (ch == '\t' || ch == '\n') *p = ' ';
    else if (ch > 0 && ch < 32) *p = ' ';
  }
  VStr str(text);
  SDL_free(text);
  return str;
}


//==========================================================================
//
//  calcStickTriggerValue
//
//==========================================================================
static int calcStickTriggerValue (const int axis, int value) noexcept {
  if (value == 0) return 0; // just in case
  value = clampval(value, -32767, 32767);
  float dead = 0.0f;
  switch (axis) {
    case SDL_CONTROLLER_AXIS_LEFTX: dead = ctl_deadzone_leftstick_x.asFloat(); break;
    case SDL_CONTROLLER_AXIS_LEFTY: dead = ctl_deadzone_leftstick_y.asFloat(); break;
    case SDL_CONTROLLER_AXIS_RIGHTX: dead = ctl_deadzone_rightstick_x.asFloat(); break;
    case SDL_CONTROLLER_AXIS_RIGHTY: dead = ctl_deadzone_rightstick_y.asFloat(); break;
    case SDL_CONTROLLER_AXIS_TRIGGERLEFT: dead = ctl_trigger_left_edge.asFloat(); break;
    case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: dead = ctl_trigger_right_edge.asFloat(); break;
    default: return 0;
  }
  if (dead >= 1.0f) return 0;
  dead = min2(0.0f, dead);
  const int minval = (int)(32767*dead);
  if (abs(value) < minval) return 0;
  return clampval(value*127/32767, -127, 127);
}


//==========================================================================
//
//  VSdlInputDevice::ReadInput
//
//  Reads input from the input devices.
//
//==========================================================================
void VSdlInputDevice::ReadInput () {
  SDL_Event ev;
  event_t vev;
  int normal_value;
  int mouseCurrX = mouseLastX, mouseCurrY = mouseLastY;
  int rel_x = 0, rel_y = 0;
  bool firstDump = true;
  //bool wasMouseButtons = false; // if `true`, prepend mouse movement

  //SDL_PumpEvents();
  while (SDL_PollEvent(&ev)) {
    vev.clear();
    vev.modflags = curmodflags;
    switch (ev.type) {
      case SDL_KEYDOWN:
      case SDL_KEYUP:
        // "hyper down" flag
        switch (ev.key.keysym.scancode) {
          case SDL_SCANCODE_LGUI:
          case SDL_SCANCODE_RGUI:
            hyperDown = (ev.key.state == SDL_PRESSED);
            break;
          default: break;
        }
        // "kbd suspended" check
        if (kbdSuspended) {
          if (timeKbdAllow > Sys_Time()) break;
          kbdSuspended = false;
          if (ev.key.state != SDL_PRESSED) break; // just in case
        }
        // translate key, and post keyboard event
        if (!hyperDown || !in_hyper_block) {
          int kk = sdl2TranslateKey(ev.key.keysym.scancode);
          if (kk > 0) {
            //GCon->Logf(NAME_Debug, "***KEY%s; kk=%d", (ev.key.state == SDL_PRESSED ? "DOWN" : "UP"), kk);
            GInput->PostKeyEvent(kk, (ev.key.state == SDL_PRESSED ? 1 : 0), vev.modflags);
          }
        }
        // now fix the flags
        switch (ev.key.keysym.sym) {
          case SDLK_LSHIFT: if (ev.type == SDL_KEYDOWN) curmodflags |= bShiftLeft; else curmodflags &= ~bShiftLeft; break;
          case SDLK_RSHIFT: if (ev.type == SDL_KEYDOWN) curmodflags |= bShiftRight; else curmodflags &= ~bShiftRight; break;
          case SDLK_LCTRL: if (ev.type == SDL_KEYDOWN) curmodflags |= bCtrlLeft; else curmodflags &= ~bCtrlLeft; break;
          case SDLK_RCTRL: if (ev.type == SDL_KEYDOWN) curmodflags |= bCtrlRight; else curmodflags &= ~bCtrlRight; break;
          case SDLK_LALT: if (ev.type == SDL_KEYDOWN) curmodflags |= bAltLeft; else curmodflags &= ~bAltLeft; break;
          case SDLK_RALT: if (ev.type == SDL_KEYDOWN) curmodflags |= bAltRight; else curmodflags &= ~bAltRight; break;
          case SDLK_LGUI: if (ev.type == SDL_KEYDOWN) curmodflags |= bHyper; else curmodflags &= ~bHyper; break;
          case SDLK_RGUI: if (ev.type == SDL_KEYDOWN) curmodflags |= bHyper; else curmodflags &= ~bHyper; break;
          default: break;
        }
        if (curmodflags&(bShiftLeft|bShiftRight)) curmodflags |= bShift; else curmodflags &= ~bShift;
        if (curmodflags&(bCtrlLeft|bCtrlRight)) curmodflags |= bCtrl; else curmodflags &= ~bCtrl;
        if (curmodflags&(bAltLeft|bAltRight)) curmodflags |= bAlt; else curmodflags &= ~bAlt;
        break;
      case SDL_MOUSEMOTION:
        if (currRelative && winactive) {
          if (!firsttime) {
            // motion values
            int dx = +ev.motion.xrel;
            int dy = -ev.motion.yrel;
            if (ms_rel_mode.asInt() > 0) {
              float mx = (float)dx;
              float my = (float)dy;
              float effsens = 1.0f;

              float movedist = mx*mx+my*my;
              if (!ms_rel_squaredist.asBool()) movedist = sqrtf(movedist);

              if (movedist > 0.0f) {
                float dexp = ms_rel_exponent.asFloat();
                if (!isFiniteF(dexp)) dexp = 0.0f;
                if (dexp > 4.0f) dexp = 4.0f;
                if (dexp > 0.0f) {
                  if (ms_rel_expweird.asBool()) {
                    const float expaccel = max2(0.0f, (dexp-1.0f)*0.5f);
                    effsens = powf(movedist, expaccel)*ms_rel_sensitivity.asFloat();
                  } else {
                    effsens = powf(movedist, dexp)*ms_rel_sensscale.asFloat()+ms_rel_sensitivity.asFloat();
                  }
                }
              }

              float finsens = effsens;
              if (ms_rel_senscap.asFloat() > 0.0f && finsens > ms_rel_senscap.asFloat()) finsens = ms_rel_senscap.asFloat();

              if (m_dbg_motion.asBool()) {
                if (firstDump) { firstDump = false; GCon->Log(NAME_Debug, "=== SDLMOTION ==="); }
                GCon->Logf(NAME_Debug, "SDLMOTION: dx=%d; dy=%d; movedist=%g; finsens=%g (%g); sqdist=%d; weirdexp=%d; mx=%d; my=%d", dx, dy, movedist, finsens, effsens, (int)ms_rel_squaredist.asBool(), (int)ms_rel_expweird.asBool(), (int)floorf(mx*finsens+hlRemX), (int)floorf(my*finsens+hlRemY));
              }

              mx *= finsens;
              my *= finsens;

              // hold the remainder and pass it into the next round
              mx += hlRemX;
              my += hlRemY;

              hlRemX = mx-floorf(mx);
              hlRemY = my-floorf(my);

              rel_x += (int)floorf(mx);
              rel_y += (int)floorf(my);
            } else {
              // no acceleration
              rel_x += dx;
              rel_y += dy;
            }
            // temp, for button events
            if (Drawer) {
              mouseCurrX = clampval(mouseLastX+rel_x, 0, Drawer->getWidth()-1);
              mouseCurrY = clampval(mouseLastY+rel_y, 0, Drawer->getHeight()-1);
            }
          } else {
            firsttime = false;
          }
        }
        break;
      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP:
        vev.type = (ev.button.state == SDL_PRESSED ? ev_keydown : ev_keyup);
             if (ev.button.button == SDL_BUTTON_LEFT) vev.keycode = K_MOUSE1;
        else if (ev.button.button == SDL_BUTTON_RIGHT) vev.keycode = K_MOUSE2;
        else if (ev.button.button == SDL_BUTTON_MIDDLE) vev.keycode = K_MOUSE3;
        else if (ev.button.button == SDL_BUTTON_X1) vev.keycode = K_MOUSE4;
        else if (ev.button.button == SDL_BUTTON_X2) vev.keycode = K_MOUSE5;
        else break;
        if (IsUIMouse() || !ui_active || ui_control_waiting || ui_freemouse) {
          if (currRelative) {
            vev.x = mouseCurrX;
            vev.y = mouseCurrY;
          } else {
            if (Drawer) Drawer->GetMousePosition(&vev.x, &vev.y);
          }
          VObject::PostEvent(vev);
          //wasMouseButtons = true;
        }
        // now fix the flags
             if (ev.button.button == SDL_BUTTON_LEFT) { if (ev.button.state == SDL_PRESSED) curmodflags |= bLMB; else curmodflags &= ~bLMB; }
        else if (ev.button.button == SDL_BUTTON_RIGHT) { if (ev.button.state == SDL_PRESSED) curmodflags |= bRMB; else curmodflags &= ~bRMB; }
        else if (ev.button.button == SDL_BUTTON_MIDDLE) { if (ev.button.state == SDL_PRESSED) curmodflags |= bMMB; else curmodflags &= ~bMMB; }
        break;
      case SDL_MOUSEWHEEL:
        vev.type = ev_keydown;
             if (ev.wheel.y > 0) vev.keycode = K_MWHEELUP;
        else if (ev.wheel.y < 0) vev.keycode = K_MWHEELDOWN;
        else break;
        if (IsUIMouse() || !ui_active || ui_control_waiting || ui_freemouse) {
          if (currRelative) {
            vev.x = mouseCurrX;
            vev.y = mouseCurrY;
          } else {
            if (Drawer) Drawer->GetMousePosition(&vev.x, &vev.y);
          }
          VObject::PostEvent(vev);
          //wasMouseButtons = true;
        }
        break;
      // joysticks
      case SDL_JOYAXISMOTION:
        if (joystick) {
          //GCon->Logf(NAME_Debug, "JOY AXIS %d: motion=%d", ev.jaxis.axis, ev.jaxis.value);
          normal_value = clampval(ev.jaxis.value*127/32767, -127, 127);
               if (ev.jaxis.axis == 0) joy_x[0] = normal_value;
          else if (ev.jaxis.axis == 1) joy_y[0] = normal_value;
        }
        break;
      case SDL_JOYBALLMOTION:
        if (joystick) {
          //GCon->Logf(NAME_Debug, "JOY BALL");
        }
        break;
      case SDL_JOYHATMOTION:
        if (joystick) {
          //GCon->Logf(NAME_Debug, "JOY HAT");
        }
        break;
      case SDL_JOYBUTTONDOWN:
        if (joystick) {
          //GCon->Logf(NAME_Debug, "JOY BUTTON %d", ev.jbutton.button);
          if (/*ev.jbutton.button >= 0 &&*/ (unsigned)ev.jbutton.button <= 15) {
            joy_newb |= 1u<<ev.jbutton.button;
          }
        }
        break;
      case SDL_JOYBUTTONUP:
        if (joystick) {
          if (/*ev.jbutton.button >= 0 &&*/ (unsigned)ev.jbutton.button <= 15) {
            joy_newb &= ~(1u<<ev.jbutton.button);
          }
        }
        break;
      // controllers
      case SDL_CONTROLLERAXISMOTION:
        if (controller && ev.caxis.which == jid) {
          normal_value = calcStickTriggerValue(ev.caxis.axis, ev.caxis.value);
          #if 0
          const char *axisName = "<unknown>";
          switch (ev.caxis.axis) {
            case SDL_CONTROLLER_AXIS_LEFTX: axisName = "LEFTX"; break;
            case SDL_CONTROLLER_AXIS_LEFTY: axisName = "LEFTY"; break;
            case SDL_CONTROLLER_AXIS_RIGHTX: axisName = "RIGHTX"; break;
            case SDL_CONTROLLER_AXIS_RIGHTY: axisName = "RIGHTY"; break;
            case SDL_CONTROLLER_AXIS_TRIGGERLEFT: axisName = "TRIGLEFT"; break;
            case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: axisName = "TRIGRIGHT"; break;
            default: break;
          }
          GCon->Logf(NAME_Debug, "CTL AXIS '%s' (%d): motion=%d; nval=%d", axisName, ev.caxis.axis, ev.caxis.value, normal_value);
          #endif
          switch (ev.caxis.axis) {
            case SDL_CONTROLLER_AXIS_LEFTX: joy_x[0] = normal_value; break;
            case SDL_CONTROLLER_AXIS_LEFTY: joy_y[0] = normal_value; break;
            case SDL_CONTROLLER_AXIS_RIGHTX: joy_x[1] = normal_value; break;
            case SDL_CONTROLLER_AXIS_RIGHTY: joy_y[1] = normal_value; break;
            //HACK: consider both triggers as buttons for now
            case SDL_CONTROLLER_AXIS_TRIGGERLEFT: CtlTriggerButton(0, (normal_value != 0)); break;
            case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: CtlTriggerButton(1, (normal_value != 0)); break;
          }
        }
        break;
      case SDL_CONTROLLERBUTTONDOWN:
      case SDL_CONTROLLERBUTTONUP:
        if (controller && ev.cbutton.which == jid) {
          #if 0
          const char *buttName = "<unknown>";
          switch (ev.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_A: buttName = "K_BUTTON_A"; break;
            case SDL_CONTROLLER_BUTTON_B: buttName = "K_BUTTON_B"; break;
            case SDL_CONTROLLER_BUTTON_X: buttName = "K_BUTTON_X"; break;
            case SDL_CONTROLLER_BUTTON_Y: buttName = "K_BUTTON_Y"; break;
            case SDL_CONTROLLER_BUTTON_BACK: buttName = "K_BUTTON_BACK"; break;
            case SDL_CONTROLLER_BUTTON_GUIDE: buttName = "K_BUTTON_GUIDE"; break;
            case SDL_CONTROLLER_BUTTON_START: buttName = "K_BUTTON_START"; break;
            case SDL_CONTROLLER_BUTTON_LEFTSTICK: buttName = "K_BUTTON_LEFTSTICK"; break;
            case SDL_CONTROLLER_BUTTON_RIGHTSTICK: buttName = "K_BUTTON_RIGHTSTICK"; break;
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: buttName = "K_BUTTON_LEFTSHOULDER"; break;
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: buttName = "K_BUTTON_RIGHTSHOULDER"; break;
            case SDL_CONTROLLER_BUTTON_DPAD_UP: buttName = "K_BUTTON_DPAD_UP"; break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN: buttName = "K_BUTTON_DPAD_DOWN"; break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT: buttName = "K_BUTTON_DPAD_LEFT"; break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: buttName = "K_BUTTON_DPAD_RIGHT"; break;
            default: break;
          }
          GCon->Logf(NAME_Debug, "CTL %d BUTTON %s: '%s' (%d)", ev.cbutton.which, (ev.cbutton.state == SDL_PRESSED ? "DOWN" : "UP"), buttName, ev.cbutton.button);
          #endif
          switch (ev.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_A: GInput->PostKeyEvent(K_BUTTON_A, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
            case SDL_CONTROLLER_BUTTON_B: GInput->PostKeyEvent(K_BUTTON_B, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
            case SDL_CONTROLLER_BUTTON_X: GInput->PostKeyEvent(K_BUTTON_X, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
            case SDL_CONTROLLER_BUTTON_Y: GInput->PostKeyEvent(K_BUTTON_Y, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
            case SDL_CONTROLLER_BUTTON_BACK: GInput->PostKeyEvent(K_BUTTON_BACK, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
            case SDL_CONTROLLER_BUTTON_GUIDE: GInput->PostKeyEvent(K_BUTTON_GUIDE, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
            case SDL_CONTROLLER_BUTTON_START: GInput->PostKeyEvent(K_BUTTON_START, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
            case SDL_CONTROLLER_BUTTON_LEFTSTICK: GInput->PostKeyEvent(K_BUTTON_LEFTSTICK, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
            case SDL_CONTROLLER_BUTTON_RIGHTSTICK: GInput->PostKeyEvent(K_BUTTON_RIGHTSTICK, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: GInput->PostKeyEvent(K_BUTTON_LEFTSHOULDER, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: GInput->PostKeyEvent(K_BUTTON_RIGHTSHOULDER, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
            case SDL_CONTROLLER_BUTTON_DPAD_UP: GInput->PostKeyEvent(K_BUTTON_DPAD_UP, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN: GInput->PostKeyEvent(K_BUTTON_DPAD_DOWN, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT: GInput->PostKeyEvent(K_BUTTON_DPAD_LEFT, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: GInput->PostKeyEvent(K_BUTTON_DPAD_RIGHT, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
            default: break;
          }
        }
        break;
      case SDL_CONTROLLERDEVICEADDED:
        //GCon->Logf(NAME_Debug, "CTL %d added", ev.cdevice.which);
        if (SDL_NumJoysticks() == 1 || !(joystick && !controller)) {
          GCon->Logf(NAME_Debug, "controller attached");
          OpenJoystick(0);
        }
        break;
      case SDL_CONTROLLERDEVICEREMOVED:
        //GCon->Logf(NAME_Debug, "CTL %d removed", ev.cdevice.which);
        if (joystick_controller && ev.cdevice.which == jid) {
          ShutdownJoystick(false);
          GCon->Logf(NAME_Debug, "controller detached");
        }
        break;
      /*k8: i don't know what to do here
      case SDL_CONTROLLERDEVICEREMAPPED:
        GCon->Logf(NAME_Debug, "CTL %d remapped", ev.cdevice.which);
        if (joystick_controller && ev.cdevice.which == jid) {
        }
        break;
      */
#ifdef ANDROID
      case SDL_FINGERMOTION:
        Touch_Event(TouchMotion, ev.tfinger.fingerId, ev.tfinger.x, ev.tfinger.y, ev.tfinger.dx, ev.tfinger.dy, ev.tfinger.pressure);
        break;
      case SDL_FINGERDOWN:
        Touch_Event(TouchDown, ev.tfinger.fingerId, ev.tfinger.x, ev.tfinger.y, ev.tfinger.dx, ev.tfinger.dy, ev.tfinger.pressure);
        break;
      case SDL_FINGERUP:
        Touch_Event(TouchUp, ev.tfinger.fingerId, ev.tfinger.x, ev.tfinger.y, ev.tfinger.dx, ev.tfinger.dy, ev.tfinger.pressure);
        break;
#endif
      // window/other
      case SDL_WINDOWEVENT:
        switch (ev.window.event) {
          case SDL_WINDOWEVENT_FOCUS_GAINED:
            //GCon->Logf(NAME_Debug, "***FOCUS GAIN; wa=%d; first=%d", (int)winactive, (int)firsttime);
            DoUnpress();
            vev.modflags = 0;
            if (!winactive && mouse) {
              if (Drawer) {
                if (!ui_freemouse && (!ui_active || IsUIMouse())) Drawer->WarpMouseToWindowCenter();
                //SDL_GetMouseState(&mouseLastX, &mouseLastY);
                if (relativeFailed || !m_relative.asBool()) Drawer->GetMousePosition(&mouseLastX, &mouseLastY);
              }
            }
            winactive = true;
            OwnMouse();
            if (cl) cl->ClearInput();
            if (in_focusgain_delay.asFloat() > 0) {
              kbdSuspended = true;
              timeKbdAllow = Sys_Time()+in_focusgain_delay.asFloat();
              //GCon->Log(NAME_Debug, "***FOCUS GAIN: KBDSUSPEND!");
            }
            hyperDown = false;
            vev.type = ev_winfocus;
            vev.focused = 1;
            VObject::PostEvent(vev);
            break;
          case SDL_WINDOWEVENT_FOCUS_LOST:
            //fprintf(stderr, "***FOCUS LOST; first=%d; drawer=%p\n", (int)firsttime, Drawer);
            DoUnpress();
            vev.modflags = 0;
            winactive = false;
            DisownMouse();
            if (cl) cl->ClearInput();
            kbdSuspended = false;
            hyperDown = false;
            vev.type = ev_winfocus;
            vev.focused = 0;
            VObject::PostEvent(vev);
            break;
          //case SDL_WINDOWEVENT_TAKE_FOCUS: Drawer->SDL_SetWindowInputFocus();
          case SDL_WINDOWEVENT_RESIZED:
            // this seems to be called on videomode change; but this is not reliable
            //GCon->Logf("SDL: resized to %dx%d", ev.window.data1, ev.window.data2);
            break;
          case SDL_WINDOWEVENT_SIZE_CHANGED:
            //GCon->Logf("SDL: size changed to %dx%d", ev.window.data1, ev.window.data2);
            break;
        }
        break;
      case SDL_QUIT:
        bGotCloseRequest = true;
        GCmdBuf << "Quit\n";
        break;
      default:
        break;
    }
  }

  // read mouse separately
  if (mouse && winactive && Drawer) {
    #if 1
    // not a typo
    if (rel_x|rel_y) {
      mouseCurrX = mouseLastX = clampval(mouseLastX+rel_x, 0, Drawer->getWidth()-1);
      mouseCurrY = mouseLastY = clampval(mouseLastY+rel_y, 0, Drawer->getHeight()-1);
    }
    #endif

    bool currMouseInUI = (ui_active.asBool() || ui_freemouse.asBool());
    bool currMouseGrabbed = (ui_freemouse.asBool() ? false : IsUIMouse());
    if (!currMouseInUI) {
      if ((!relativeFailed && currRelative != m_relative.asBool()) ||
          currDoGrab != m_grab.asBool())
      {
        #ifdef SDL_MOUSE_CAPTURE_DEBUG
        GCon->Logf(NAME_Debug, "SDL: mouse mode changed, recapturing...");
        #endif
        DisownMouse();
        OwnMouse();
      }
    }
    //SDL_GetMouseState(&mouseCurrX, &mouseCurrY);
    if (!currRelative) Drawer->GetMousePosition(&mouseCurrX, &mouseCurrY);
    // check for UI activity changes
    if (currMouseInUI != uiwasactive) {
      // UI activity changed
      uiwasactive = currMouseInUI;
      uimouselast = currMouseGrabbed;
      if (!uiwasactive) {
        // ui deactivated
        OwnMouse();
        HideRealMouse();
      } else {
        // ui activted
        DisownMouse();
        if (!uimouselast) {
          if (curHidden && ui_want_mouse_at_zero) SDL_WarpMouseGlobal(0, 0);
          ShowRealMouse();
        }
      }
    }
    // check for "allow mouse in UI" changes
    if (currMouseGrabbed != uimouselast) {
      // "allow mouse in UI" changed
      if (uiwasactive) {
        if (gl_current_screen_fsmode == 0) {
          if (currMouseGrabbed) HideRealMouse(); else ShowRealMouse();
        } else {
          HideRealMouse();
        }
        if (GRoot) GRoot->SetMouse(currMouseGrabbed);
      }
      uimouselast = currMouseGrabbed;
    }
    // hide real mouse in fullscreen mode, show in windowed mode (if necessary)
    if (gl_current_screen_fsmode != 0 && !curHidden) HideRealMouse();
    if (gl_current_screen_fsmode == 0 && curHidden && currMouseInUI && !currMouseGrabbed) ShowRealMouse();
    // generate events
    if (!currMouseInUI || currMouseGrabbed) {
      if (currRelative) {
        // not a typo
        if (rel_x|rel_y) {
          #if 0
          if (true/*wasMouseButtons*/) {
            GCon->Log(NAME_Debug, "=== BEFORE EVENTS ===");
            for (int eidx = 0; eidx < VObject::CountQueuedEvents(); ++eidx) {
              if (!VObject::PeekEvent(eidx, &vev)) break;
              GCon->Logf(NAME_Debug, "  %2d: type=%d; data1=%d; data2=%d; data3=%d; data4=%d", eidx, vev.type, vev.data1, vev.data2, vev.data3, vev.data4);
            }
          }
          #endif
          vev.clear();
          vev.modflags = curmodflags;
          vev.type = ev_mouse;
          vev.dx = rel_x;
          vev.dy = rel_y;
          if (true/*wasMouseButtons*/) VObject::InsertEvent(vev); else VObject::PostEvent(vev);
          #if 0
          if (true/*wasMouseButtons*/) {
            GCon->Log(NAME_Debug, "--- AFTER EVENTS ---");
            for (int eidx = 0; eidx < VObject::CountQueuedEvents(); ++eidx) {
              if (!VObject::PeekEvent(eidx, &vev)) break;
              GCon->Logf(NAME_Debug, "  %2d: type=%d; data1=%d; data2=%d; data3=%d; data4=%d", eidx, vev.type, vev.data1, vev.data2, vev.data3, vev.data4);
            }
            GCon->Log(NAME_Debug, "--------------------");
          }
          #endif

          vev.clear();
          vev.modflags = curmodflags;
          vev.type = ev_uimouse;
          vev.x = mouseCurrX;
          vev.y = mouseCurrY;
          VObject::PostEvent(vev);
        }
      } else {
        // non-relative
        Drawer->WarpMouseToWindowCenter();
        int dx = mouseCurrX-mouseLastX;
        int dy = mouseLastY-mouseCurrY;
        if (dx || dy) {
          if (!firsttime) {
            vev.clear();
            vev.modflags = curmodflags;
            vev.type = ev_mouse;
            vev.dx = dx;
            vev.dy = dy;
            if (true/*wasMouseButtons*/) VObject::InsertEvent(vev); else VObject::PostEvent(vev);

            vev.clear();
            vev.modflags = curmodflags;
            vev.type = ev_uimouse;
            vev.x = mouseCurrX;
            vev.y = mouseCurrY;
            VObject::PostEvent(vev);
          }
          firsttime = false;
          Drawer->GetMousePosition(&mouseLastX, &mouseLastY);
        }
      }
    } else {
      mouseLastX = mouseCurrX;
      mouseLastY = mouseCurrY;
    }
  }

  PostJoystick();

#ifdef ANDROID
  Touch_Update();
#endif

}


//**************************************************************************
//**
//**  JOYSTICK
//**
//**************************************************************************

//==========================================================================
//
//  VSdlInputDevice::ShutdownJoystick
//
//==========================================================================
void VSdlInputDevice::ShutdownJoystick (bool closesubsys) {
  if (haptic) { SDL_HapticClose(haptic); haptic = nullptr; }
  if (joystick) { SDL_JoystickClose(joystick); joystick = nullptr; }
  if (controller) { SDL_GameControllerClose(controller); controller = nullptr; }
  if (closesubsys) {
    if (has_haptic) { SDL_QuitSubSystem(SDL_INIT_HAPTIC); has_haptic = false; }
    if (joystick_started) { SDL_QuitSubSystem(VSDL_JINIT); joystick_started = false; }
  }
  joy_num_buttons = 0;
  joystick_controller = false;
  joynum = -1;
  jid = 0;
  memset(&joy_x[0], 0, 2*sizeof(joy_x[0]));
  memset(&joy_y[0], 0, 2*sizeof(joy_y[0]));
  memset(&joy_oldx[0], 0, 2*sizeof(joy_oldx[0]));
  memset(&joy_oldy[0], 0, 2*sizeof(joy_oldy[0]));
  if (closesubsys) {
    SDL_JoystickEventState(SDL_IGNORE);
    SDL_GameControllerEventState(SDL_IGNORE);
  }
}


//==========================================================================
//
//  VSdlInputDevice::LoadControllerMappings
//
//  deletes stream; it is ok to pass `nullptr`
//
//==========================================================================
void VSdlInputDevice::LoadControllerMappings (VStream *st) {
  if (!st) return;

  VStr stname = st->GetName();
  int size = st->TotalSize();
  if (size <= 0 || size > 1024*1024*32) {
    if (size) GCon->Logf(NAME_Error, "cannot load controller mappings from '%s' (bad file size)", *stname);
    VStream::Destroy(st);
    return;
  }

  char *buf = new char[(unsigned)(size+1)];
  st->Serialise(buf, size);
  const bool wasErr = st->IsError();
  VStream::Destroy(st);
  if (wasErr) {
    GCon->Logf(NAME_Error, "cannot read controller mappings from '%s'", *stname);
    delete[] buf;
    return;
  }

  SDL_RWops *rwo = SDL_RWFromConstMem(buf, size);
  if (rwo) {
    const int count = SDL_GameControllerAddMappingsFromRW(rwo, 1); // free rwo
    if (count < 0) {
      GCon->Logf(NAME_Error, "cannot read controller mappings from '%s'", *stname);
    } else if (count > 0) {
      GCon->Logf(NAME_Init, "read %d controller mapping%s from '%s'", count, (count != 1 ? "s" : ""), *stname);
    }
  } else {
    GCon->Logf(NAME_Error, "cannot read controller mappings from '%s' (rwo)", *stname);
  }

  delete[] buf;
}


//==========================================================================
//
//  VSdlInputDevice::StartupJoystick
//
//  Initialises joystick
//
//==========================================================================
void VSdlInputDevice::OpenJoystick (int jnum) {
  if (!joystick_started) return;

  ShutdownJoystick(false);

  int joycount = SDL_NumJoysticks();
  if (joycount < 0) {
    GCon->Log(NAME_Init, "SDL: joystick/controller initialisation failed (cannot get number of joysticks)");
    ShutdownJoystick(true);
    return;
  }

  if (jnum >= joycount) jnum = joycount-1;
  if (jnum < 0) jnum = 0;
  joynum = jnum;

  if (joycount == 0) return;

  if (SDL_IsGameController(joynum)) {
    if (cli_NoCotl) {
      GCon->Log(NAME_Init, "SDL: skipping controller initialisation due to user request");
      ShutdownJoystick(false);
      return;
    }
    joystick_controller = true;
    joy_num_buttons = 0;
    controller = SDL_GameControllerOpen(joynum);
    if (!controller) {
      GCon->Logf(NAME_Init, "SDL: cannot initialise controller #%d", joynum);
      ShutdownJoystick(false);
      return;
    }
    GCon->Logf(NAME_Init, "SDL: joystick is a controller (%s)", SDL_GameControllerNameForIndex(joynum));
    SDL_Joystick *cj = SDL_GameControllerGetJoystick(controller);
    if (!cj) {
      GCon->Log(NAME_Init, "SDL: controller initialisation failed (cannot get joystick info)");
      ShutdownJoystick(true);
      return;
    }
    jid = SDL_JoystickInstanceID(cj);
    if (has_haptic) {
      haptic = SDL_HapticOpenFromJoystick(cj);
      if (haptic) {
        GCon->Logf(NAME_Init, "SDL: found haptic support for controller");
      } else {
        GCon->Logf(NAME_Init, "SDL: cannot open haptic interface");
      }
    }
  } else {
    if (cli_NoJoy) {
      GCon->Log(NAME_Init, "SDL: skipping joystick initialisation due to user request");
      ShutdownJoystick(false);
      return;
    }
    joystick = SDL_JoystickOpen(joynum);
    if (!joystick) {
      GCon->Logf(NAME_Init, "SDL: cannot initialise joystick #%d", joynum);
      ShutdownJoystick(false);
      return;
    }
    jid = SDL_JoystickInstanceID(joystick);
    joy_num_buttons = SDL_JoystickNumButtons(joystick);
    GCon->Logf(NAME_Init, "SDL: found joystick with %d buttons", joy_num_buttons);
  }

  SDL_JoystickEventState(SDL_ENABLE);
  SDL_GameControllerEventState(SDL_ENABLE);
}


//==========================================================================
//
//  VSdlInputDevice::StartupJoystick
//
//  Initialises joystick
//
//==========================================================================
void VSdlInputDevice::StartupJoystick () {
  ShutdownJoystick(true);

  if (cli_NoJoy && cli_NoCotl) {
    //GCon->Log(NAME_Init, "SDL: skipping joystick/controller initialisation due to user request");
    return;
  }

  // load user controller mappings
  VStream *rcstrm = FL_OpenFileReadInCfgDir("gamecontrollerdb.txt");
  if (rcstrm) {
    GCon->Log(NAME_Init, "SDL: loading user-defined controller map");
    LoadControllerMappings(rcstrm);
  } else {
    // no user mappings, load built-in mappings
    rcstrm = FL_OpenFileReadBaseOnly("gamecontrollerdb.txt");
    GCon->Log(NAME_Init, "SDL: loading built-in controller map");
    LoadControllerMappings(rcstrm);
  }

  //FIXME: change this!
  int jnum = 0;
  for (int jparm = GArgs.CheckParmFrom("-joy", -1, true); jparm; jparm = GArgs.CheckParmFrom("-joy", jparm, true)) {
    const char *jarg = GArgs[jparm];
    //GCon->Logf("***%d:<%s>", jparm, jarg);
    vassert(jarg); // just in case
    jarg += 4;
    if (!jarg[0]) continue;
    int n = -1;
    if (!VStr::convertInt(jarg, &n)) continue;
    if (n < 0) continue;
    jnum = n;
  }
  GCon->Logf(NAME_Init, "SDL: will try to use joystick #%d", jnum);

  if (SDL_InitSubSystem(VSDL_JINIT) < 0) {
    GCon->Log(NAME_Init, "SDL: joystick/controller initialisation failed");
    cli_NoJoy = 1;
    cli_NoCotl = 1;
    return;
  }
  joystick_started = true;

  if (cli_NoCotl) {
    has_haptic = false;
  } else if (SDL_InitSubSystem(SDL_INIT_HAPTIC) < 0) {
    has_haptic = false;
    GCon->Log(NAME_Init, "SDL: cannot init haptic subsystem, force feedback disabled");
  } else {
    has_haptic = true;
    GCon->Log(NAME_Init, "SDL: force feedback enabled");
  }

  int joycount = SDL_NumJoysticks();
  if (joycount < 0) {
    GCon->Log(NAME_Init, "SDL: joystick/controller initialisation failed (cannot get number of joysticks)");
    ShutdownJoystick(true);
    cli_NoJoy = 1;
    cli_NoCotl = 1;
    return;
  }

  SDL_JoystickEventState(SDL_ENABLE);
  SDL_GameControllerEventState(SDL_ENABLE);

  if (joycount == 0) {
    GCon->Log(NAME_Init, "SDL: no joysticks found");
    return;
  }

  GCon->Logf(NAME_Init, "SDL: %d joystick%s found", joycount, (joycount == 1 ? "" : "s"));

  if (joycount == 1 || (jnum >= 0 && jnum < joycount)) OpenJoystick(jnum);
}


//==========================================================================
//
//  VSdlInputDevice::PostJoystick
//
//==========================================================================
void VSdlInputDevice::PostJoystick () {
  if (!joystick_started || !(joystick || controller)) return;

  for (unsigned jn = 0; jn < 2; ++jn) {
    if (joy_oldx[jn] != joy_x[jn] || joy_oldy[jn] != joy_y[jn]) {
      //GCon->Logf(NAME_Debug, "joystick: %d (old:%d), %d (old:%d)", joy_x, joy_oldx, joy_y, joy_oldy);
      event_t event;
      event.clear();
      event.type = ev_joystick;
      event.dx = joy_x[jn];
      event.dy = joy_y[jn];
      event.joyidx = (int)jn;
      event.modflags = curmodflags;
      VObject::PostEvent(event);

      joy_oldx[jn] = joy_x[jn];
      joy_oldy[jn] = joy_y[jn];
    }
  }

  if (joystick) {
    const int nb = min2(joy_num_buttons, 15);
    for (int i = 0; i < nb; ++i) {
      if ((joy_newb&(1u<<i)) != (joy_oldb&(1u<<i))) {
        const bool pressed = !!(joy_newb&(1u<<i));
        GInput->PostKeyEvent(K_JOY1+i, pressed, curmodflags);
      }
      joy_oldb = joy_newb;
    }
  }
}


//==========================================================================
//
//  VSdlInputDevice::CheckForEscape
//
//==========================================================================
bool VSdlInputDevice::CheckForEscape () {
  bool res = bGotCloseRequest;
  SDL_Event ev;

  VInputPublic::UnpressAll();

  //SDL_PumpEvents();
  while (!bGotCloseRequest && SDL_PollEvent(&ev)) {
    switch (ev.type) {
      //case SDL_KEYDOWN:
      case SDL_KEYUP:
        switch (ev.key.keysym.sym) {
          case SDLK_ESCAPE: res = true; break;
          default: break;
        }
        break;
      case SDL_QUIT:
        bGotCloseRequest = true;
        res = true;
        GCmdBuf << "Quit\n";
        break;
      default:
        break;
    }
  }

  return res;
}


//**************************************************************************
//**
//**    INPUT
//**
//**************************************************************************

//==========================================================================
//
//  VInputDevice::CreateDevice
//
//==========================================================================
VInputDevice *VInputDevice::CreateDevice () {
  return new VSdlInputDevice();
}
