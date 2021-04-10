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
#ifdef ANDROID
#ifdef VAVOOM_CUSTOM_SPECIAL_SDL
# include <SDL.h>
#else
# include <SDL2/SDL.h>
#endif
#include "gamedefs.h"
#include "drawer.h"
#include "touch.h"

#define ID_UNDEF (-666)

#define X_ROT_SENSITIVITY 1.0
#define Y_ROT_SENSITIVITY 1.0
#define X_ROT_THRESHOLD 0.01
#define Y_ROT_THRESHOLD 0.01

#define X_STEP_THRESHOLD 0.05
#define Y_STEP_THRESHOLD 0.05

#define STEP_FINGER 0
#define ROT_FINGER 1
#define MAX_FINGERS 2

static struct vfinger {
  int id;                 // system finger id
  float x, y;             // [0..1]
  float dx, dy;           // [-1..+1]
  float start_x, start_y; // [0..1]
  float vx, vy;           // [-1..+1]
} fingers[MAX_FINGERS] = {
  { ID_UNDEF, 0, 0 },
  { ID_UNDEF, 0, 0 },
};

#define MAX_BUTTONS 9

static struct vbutton {
  float x, y, r;
  const char *press;
  const char *release;
  vint32 key;
  int finger;
} buttons[MAX_BUTTONS] = {
  { 0.85, 0.5, 0.08, "+Attack", "-Attack", K_ENTER, ID_UNDEF },
  { 1.0, 1.0, 0.1, "+Use", "-Use", 0, ID_UNDEF },
  { 0.8, 1.0, 0.1, "+Crouch", "-Crouch", 0, ID_UNDEF },
  { 0.6, 1.0, 0.1, "+Jump", "-Jump", 0, ID_UNDEF },
  { 0.6, 0.0, 0.1, "impulse 17", "", 0, ID_UNDEF }, // prev weapon
  { 0.8, 0.0, 0.1, "impulse 18", "", 0, ID_UNDEF }, // next weapon
  { 1.0, 0.0, 0.1, "ToggleAlwaysRun", "", 0, ID_UNDEF },
  { 0.0, 1.0, 0.1, "SetMenu Main", "", K_ESCAPE, ID_UNDEF },
  { 0.0, 0.0, 0.1, "ToggleConsole", "", 0, ID_UNDEF },
};

static VCvarB touch_enable ("touch_enable", true, "show and handle touch screen controls (when available)", CVAR_Archive);

static void draw_pad (int f, float x_threshold, float y_threshold) {
  if (fingers[f].id != ID_UNDEF) {
    int scr_w = Drawer->getWidth();
    int scr_h = Drawer->getHeight();
    float sx = fingers[f].start_x * scr_w;
    float sy = fingers[f].start_y * scr_h;
    float tx = x_threshold * scr_w;
    float ty = y_threshold * scr_h;
    Drawer->DrawRect(sx - tx, sy - ty, sx + tx, sy + ty, 0xff8f0f00, 1.0);
    tx /= 4;
    ty /= 4;
    float x = fingers[f].x * scr_w;
    float y = fingers[f].y * scr_h;
    Drawer->FillRect(sx - tx, sy - ty, sx + tx, sy + ty, 0xffff0f00, 0.5);
    Drawer->FillRect(x - tx, y - ty, x + tx, y + ty, 0xffff0f3f, 0.5);
  }
}

void Touch_Draw (void) {
  if (!touch_enable || SDL_GetNumTouchDevices() <= 0 || SDL_IsTextInputActive()) {
    return;
  }

  int scr_w = Drawer->getWidth();
  int scr_h = Drawer->getHeight();
  for (int i = 0; i < MAX_BUTTONS; i++) {
    int x = buttons[i].x * scr_w;
    int y = buttons[i].y * scr_h;
    int tx = buttons[i].r * scr_w;
    int ty = buttons[i].r * scr_h;
    Drawer->DrawRect(x - tx, y - ty, x + tx, y + ty, 0xff00ff00, 0.8);
  }
  draw_pad(STEP_FINGER, X_STEP_THRESHOLD, Y_STEP_THRESHOLD);
  draw_pad(ROT_FINGER, X_ROT_THRESHOLD, Y_ROT_THRESHOLD);
}

//static float length (float x0, float y0, float x1, float y1) {
//  return sqrtf(powf(x1 - x0, 2) + powf(y1 - y0, 2));
//}

static float signum (float x) {
  return x < 0 ? -1 : x > 0 ? +1 : 0;
}

static void press_pad (/*int pad,*/ int key, bool down) {
  if (MN_Active()) {
    GInput->PostKeyEvent(key, down ? 1 : 0, 0);
  } else {
    const char *c = down ? "+" : "-";
//    if (pad == STEP_FINGER) {
      switch (key) {
        case K_UPARROW: GCmdBuf << c << "Forward\n"; break;
        case K_DOWNARROW: GCmdBuf << c << "Backward\n"; break;
        case K_LEFTARROW: GCmdBuf << c << "MoveLeft\n"; break;
        case K_RIGHTARROW: GCmdBuf << c << "MoveRight\n"; break;
      }
//    } else if (pad == ROT_FINGER) {
//      switch (key) {
//        case K_UPARROW: GCmdBuf << c << "LookUp\n"; break;
//        case K_DOWNARROW: GCmdBuf << c << "LookDown\n"; break;
//        case K_LEFTARROW: GCmdBuf << c << "LookLeft\n"; break;
//        case K_RIGHTARROW: GCmdBuf << c << "LookRight\n"; break;
//      }
//    }
  }
}

static void PostMouseEvent (float dx, float dy) {
  if (!MN_Active()) {
    event_t ev;
    ev.clear();
    ev.type = ev_mouse;
    ev.modflags = 0;
    ev.dx = X_ROT_SENSITIVITY * Drawer->getWidth() * dx;
    ev.dy = Y_ROT_SENSITIVITY * Drawer->getHeight() * -dy;
    VObject::PostEvent(ev);
  }
}

void Touch_Update (void) {
  if (!touch_enable || SDL_GetNumTouchDevices() <= 0 || SDL_IsTextInputActive()) {
    return;
  }

  if (fingers[ROT_FINGER].id != ID_UNDEF) {
    float dx = fingers[ROT_FINGER].x - fingers[ROT_FINGER].start_x;
    float dy = fingers[ROT_FINGER].y - fingers[ROT_FINGER].start_y;
    dx = fabs(dx) >= X_ROT_THRESHOLD ? dx - signum(dx) * X_ROT_THRESHOLD : 0;
    dy = fabs(dy) >= X_ROT_THRESHOLD ? dy - signum(dy) * X_ROT_THRESHOLD : 0;
    if (dx != 0 || dy != 0) {
      PostMouseEvent(dx, dy);
    }
  }
  if (fingers[STEP_FINGER].id != ID_UNDEF) {
    float vx = fingers[STEP_FINGER].x - fingers[STEP_FINGER].start_x;
    float vy = fingers[STEP_FINGER].y - fingers[STEP_FINGER].start_y;
    vx = fabs(vx) >= X_STEP_THRESHOLD ? vx - signum(vx) * X_STEP_THRESHOLD : 0;
    vy = fabs(vy) >= X_STEP_THRESHOLD ? vy - signum(vy) * X_STEP_THRESHOLD : 0;
    vx = signum(vx);
    vy = signum(vy);
    if (vx != fingers[STEP_FINGER].vx) {
      if (fingers[STEP_FINGER].vx < 0) {
        press_pad(K_LEFTARROW, false);
      } else if (fingers[STEP_FINGER].vx > 0) {
        press_pad(K_RIGHTARROW, false);
      }
      if (vx < 0) {
        press_pad(K_LEFTARROW, true);
      } else if (vx > 0) {
        press_pad(K_RIGHTARROW, true);
      }
    }
    if (vy != fingers[STEP_FINGER].vy) {
      if (fingers[STEP_FINGER].vy < 0) {
        press_pad(K_UPARROW, false);
      } else if (fingers[STEP_FINGER].vy > 0) {
        press_pad(K_DOWNARROW, false);
      }
      if (vy < 0) {
        press_pad(K_UPARROW, true);
      } else if (vy > 0) {
        press_pad(K_DOWNARROW, true);
      }
    }
    fingers[STEP_FINGER].vx = vx;
    fingers[STEP_FINGER].vy = vy;
  }
}

static int find_finger (int id) {
  for (int i = 0; i < MAX_FINGERS; i++) {
    if (fingers[i].id == id) {
      return i;
    }
  }
  return -1;
}

static bool point_in_circle (float x, float y, float cx, float cy, float cr) {
  return powf(x - cx, 2) + powf(y - cy, 2) < powf(cr, 2);
}

static void press_button (int finger, float x, float y, bool down) {
  for (int i = 0; i < MAX_BUTTONS; i++) {
    if (point_in_circle(x, y, buttons[i].x, buttons[i].y, buttons[i].r)) {
      if (down) {
        if (buttons[i].finger == ID_UNDEF) {
          if (MN_Active()) {
            if (buttons[i].key != 0) {
              GInput->PostKeyEvent(buttons[i].key, 1, 0);
            }
          } else {
            GCmdBuf << buttons[i].press << "\n";
          }
          buttons[i].finger = finger;
        }
      } else {
        if (buttons[i].finger == finger) {
          if (MN_Active()) {
            if (buttons[i].key != 0) {
              GInput->PostKeyEvent(buttons[i].key, 0, 0);
            }
          } else {
            GCmdBuf << buttons[i].release << "\n";
          }
          buttons[i].finger = ID_UNDEF;
        }
      }
    }
  }
}

// x, y IN [0.0..1.0]
// dx, dy IN [-1.0..1.0]
// pressure IN [0.0..1.0]
void Touch_Event (int type, int id, float x, float y, float dx, float dy, float pressure) {
  if (!touch_enable || SDL_GetNumTouchDevices() <= 0 || SDL_IsTextInputActive()) {
    return;
  }

  int f;
  switch (type) {
    case TouchDown:
      press_button(id, x, y, true);
      f = x < 0.5 ? STEP_FINGER : ROT_FINGER;
      if (fingers[f].id == ID_UNDEF) {
        fingers[f].id = id;
        fingers[f].x = x;
        fingers[f].y = y;
        fingers[f].dx = dx;
        fingers[f].dy = dy;
        fingers[f].start_x = x;
        fingers[f].start_y = y;
      }
      break;
    case TouchMotion:
      f = find_finger(id);
      if (f >= 0) {
        fingers[f].x = x;
        fingers[f].y = y;
        fingers[f].dx = dx;
        fingers[f].dy = dy;
      }
      break;
    case TouchUp:
      press_button(id, x, y, false);
      f = find_finger(id);
      if (f >= 0) {
        press_button(id, fingers[f].start_x, fingers[f].start_y, false);
        fingers[f].id = ID_UNDEF;
        if (f == STEP_FINGER) {
          if (fingers[STEP_FINGER].vx < 0) {
            press_pad(K_LEFTARROW, false);
          } else if (fingers[STEP_FINGER].vx > 0) {
            press_pad(K_RIGHTARROW, false);
          }
          if (fingers[STEP_FINGER].vy < 0) {
            press_pad(K_UPARROW, false);
          } else if (fingers[STEP_FINGER].vy > 0) {
            press_pad(K_DOWNARROW, false);
          }
        } else if (f == ROT_FINGER) {
          // nothing
        }
      }
      break;
  }
}
#endif
