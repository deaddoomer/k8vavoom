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
//**
//**  Status bar code.
//**
//**************************************************************************
#include "gamedefs.h"
#include "host.h"  /* host_frametime */
#include "sbar.h"
#include "screen.h"
#include "client/client.h"
#include "client/cl_local.h"
#include "automap.h"
#include "psim/p_player.h"
#include "render/r_public.h"


enum {
  SB_VIEW_NORMAL,
  SB_VIEW_AUTOMAP,
  SB_VIEW_FULLSCREEN
};


extern VCvarI screen_size;

int sb_height = 32;

// colors
static VCvarS sb_color_ammo1("sb_color_ammo1", "default", "StatusBar ammo number color.", CVAR_Archive);
static VCvarS sb_color_ammo2("sb_color_ammo2", "default", "StatusBar ammo number color.", CVAR_Archive);
static VCvarS sb_color_armor("sb_color_armor", "default", "StatusBar armor number color.", CVAR_Archive);
static VCvarS sb_color_health("sb_color_health", "default", "StatusBar health number color.", CVAR_Archive);
static VCvarS sb_color_healthaccum("sb_color_healthaccum", "default", "StatusBar health accumulator number color.", CVAR_Archive);
static VCvarS sb_color_frags("sb_color_frags", "default", "StatusBar frags number color.", CVAR_Archive);

static VCvarS sb_color_smallammo_full("sb_color_smallammo_full", "default", "StatusBar FS small ammo number color (full).", CVAR_Archive);
static VCvarS sb_color_smallammo_normal("sb_color_smallammo_normal", "default", "StatusBar FS small ammo number color (normal).", CVAR_Archive);
static VCvarS sb_color_smallammo_lower("sb_color_smallammo_lower", "default", "StatusBar FS small ammo number color (lower).", CVAR_Archive);
static VCvarS sb_color_smallammo_low("sb_color_smallammo_low", "default", "StatusBar FS small ammo number color (low).", CVAR_Archive);
static VCvarS sb_color_smallammo_verylow("sb_color_smallammo_verylow", "default", "StatusBar FS small ammo number color (very low/empty).", CVAR_Archive);

static ColorCV sbColorAmmo1(&sb_color_ammo1, nullptr, true); // allow "no color"
static ColorCV sbColorAmmo2(&sb_color_ammo2, nullptr, true); // allow "no color"
static ColorCV sbColorArmor(&sb_color_armor, nullptr, true); // allow "no color"
static ColorCV sbColorHealth(&sb_color_health, nullptr, true); // allow "no color"
static ColorCV sbColorHealthAccum(&sb_color_healthaccum, nullptr, true); // allow "no color"
static ColorCV sbColorFrags(&sb_color_frags, nullptr, true); // allow "no color"

static ColorCV sbColorSmallAmmoFull(&sb_color_smallammo_full, nullptr, true); // allow "no color"
static ColorCV sbColorSmallAmmoNormal(&sb_color_smallammo_normal, nullptr, true); // allow "no color"
static ColorCV sbColorSmallAmmoLower(&sb_color_smallammo_lower, nullptr, true); // allow "no color"
static ColorCV sbColorSmallAmmoLow(&sb_color_smallammo_low, nullptr, true); // allow "no color"
static ColorCV sbColorSmallAmmoVeryLow(&sb_color_smallammo_verylow, nullptr, true); // allow "no color"


//==========================================================================
//
//  SB_RealHeight
//
//==========================================================================
int SB_RealHeight () {
  //return (int)(ScreenHeight/640.0f*sb_height*R_GetAspectRatio());
  return (int)(ScreenHeight/480.0f*sb_height*R_GetAspectRatio());
}


//==========================================================================
//
//  SB_Init
//
//==========================================================================
void SB_Init () {
  sb_height = GClGame->sb_height;
}


//==========================================================================
//
//  SB_Ticker
//
//==========================================================================
void SB_Ticker () {
  if (cl && cls.signon && cl->MO) GClGame->eventStatusBarUpdateWidgets(host_frametime);
}


//==========================================================================
//
//  SB_Responder
//
//==========================================================================
bool SB_Responder (event_t *) {
  return false;
}


//==========================================================================
//
//  SB_Drawer
//
//==========================================================================
void SB_Drawer () {
  // update widget visibility
  if (AM_IsFullscreen() && screen_size >= 11) return;
  if (!GClLevel) return;
  if (!CL_NeedAutomapUpdates()) return; // camera is not on the player
  GClGame->eventStatusBarDrawer(AM_IsFullscreen() && screen_size < 11 ?
      SB_VIEW_AUTOMAP :
      GClLevel->Renderer->refdef.height == ScreenHeight ? SB_VIEW_FULLSCREEN : SB_VIEW_NORMAL);
}


//==========================================================================
//
//  SB_Start
//
//==========================================================================
void SB_Start () {
  GClGame->eventStatusBarStartMap();
}



//==========================================================================
//
//  VC API
//
//==========================================================================
//WARNING! keep in sync with VC code!
enum {
  SBTC_Ammo1 = 0,
  SBTC_Ammo2,
  SBTC_Armor,
  SBTC_Health,
  SBTC_HealthAccum,
  SBTC_Frags,
  // smaller ammo counters
  SmallAmmoFull,
  SmallAmmoNormal,
  SmallAmmoLower,
  SmallAmmoLow,
  SmallAmmoVeryLow,
};


//==========================================================================
//
//  retDefColor
//
//==========================================================================
static inline int retDefColor (const vuint32 clr, const int def) noexcept {
  return (clr ? (int)clr : (int)def);
}


//native static final int SB_GetTextColor (int type);
IMPLEMENT_FREE_FUNCTION(VObject, SB_GetTextColor) {
  int type;
  vobjGetParam(type);
  switch (type) {
    case SBTC_Ammo1: RET_INT(sbColorAmmo1.getColor()); break;
    case SBTC_Ammo2: RET_INT(sbColorAmmo2.getColor()); break;
    case SBTC_Armor: RET_INT(sbColorArmor.getColor()); break;
    case SBTC_Health: RET_INT(sbColorHealth.getColor()); break;
    case SBTC_HealthAccum: RET_INT(sbColorHealthAccum.getColor()); break;
    case SBTC_Frags: RET_INT(sbColorFrags.getColor()); break;
    case SmallAmmoFull: RET_INT(retDefColor(sbColorSmallAmmoFull.getColor(), CR_WHITE)); break;
    case SmallAmmoNormal: RET_INT(retDefColor(sbColorSmallAmmoNormal.getColor(), CR_GREEN)); break;
    case SmallAmmoLower: RET_INT(retDefColor(sbColorSmallAmmoLower.getColor(), CR_ORANGE)); break;
    case SmallAmmoLow: RET_INT(retDefColor(sbColorSmallAmmoLow.getColor(), CR_YELLOW)); break;
    case SmallAmmoVeryLow: RET_INT(retDefColor(sbColorSmallAmmoVeryLow.getColor(), CR_RED)); break;
    default: RET_INT(0);
  }
}
