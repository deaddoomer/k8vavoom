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

static VCvarS sb_color_small_ammo("sb_color_small_ammo", "default", "StatusBar small ammo color.", CVAR_Archive);
static VCvarS sb_color_small_ammomax("sb_color_small_ammomax", "default", "StatusBar small max ammo color.", CVAR_Archive);
static VCvarS sb_color_itemamount("sb_color_itemamount", "default", "StatusBar item amount color.", CVAR_Archive);

static VCvarS sb_color_weaponammo_full("sb_color_weaponammo_full", "default", "StatusBar FS small ammo number color (full).", CVAR_Archive);
static VCvarS sb_color_weaponammo_normal("sb_color_weaponammo_normal", "default", "StatusBar FS small ammo number color (normal).", CVAR_Archive);
static VCvarS sb_color_weaponammo_lower("sb_color_weaponammo_lower", "default", "StatusBar FS small ammo number color (lower).", CVAR_Archive);
static VCvarS sb_color_weaponammo_low("sb_color_weaponammo_low", "default", "StatusBar FS small ammo number color (low).", CVAR_Archive);
static VCvarS sb_color_weaponammo_verylow("sb_color_weaponammo_verylow", "default", "StatusBar FS small ammo number color (very low/empty).", CVAR_Archive);

static VCvarS sb_color_weapon_name("sb_color_weapon_name", "default", "StatusBar FS weapon name color.", CVAR_Archive);

static VCvarS sb_color_automap_mapname("sb_color_automap_mapname", "default", "Automap stats: map name color.", CVAR_Archive);
static VCvarS sb_color_automap_mapcluster("sb_color_automap_mapcluster", "default", "Automap stats: map cluster info color.", CVAR_Archive);
static VCvarS sb_color_automap_kills("sb_color_automap_kills", "default", "Automap stats: number of kills color.", CVAR_Archive);
static VCvarS sb_color_automap_items("sb_color_automap_items", "default", "Automap stats: number of items color.", CVAR_Archive);
static VCvarS sb_color_automap_secrets("sb_color_automap_secrets", "default", "Automap stats: number of secrets color.", CVAR_Archive);
static VCvarS sb_color_automap_totaltime("sb_color_automap_totaltime", "default", "Automap stats: total playing time color.", CVAR_Archive);


static ColorCV sbColorAmmo1(&sb_color_ammo1, nullptr, true); // allow "no color"
static ColorCV sbColorAmmo2(&sb_color_ammo2, nullptr, true); // allow "no color"
static ColorCV sbColorArmor(&sb_color_armor, nullptr, true); // allow "no color"
static ColorCV sbColorHealth(&sb_color_health, nullptr, true); // allow "no color"
static ColorCV sbColorHealthAccum(&sb_color_healthaccum, nullptr, true); // allow "no color"
static ColorCV sbColorFrags(&sb_color_frags, nullptr, true); // allow "no color"

static ColorCV sbColorSmallAmmo(&sb_color_small_ammo, nullptr, true); // allow "no color"
static ColorCV sbColorSmallAmmoMax(&sb_color_small_ammomax, nullptr, true); // allow "no color"
static ColorCV sbColorItemAmount(&sb_color_itemamount, nullptr, true); // allow "no color"

static ColorCV sbColorWeaponAmmoFull(&sb_color_weaponammo_full, nullptr, true); // allow "no color"
static ColorCV sbColorWeaponAmmoNormal(&sb_color_weaponammo_normal, nullptr, true); // allow "no color"
static ColorCV sbColorWeaponAmmoLower(&sb_color_weaponammo_lower, nullptr, true); // allow "no color"
static ColorCV sbColorWeaponAmmoLow(&sb_color_weaponammo_low, nullptr, true); // allow "no color"
static ColorCV sbColorWeaponAmmoVeryLow(&sb_color_weaponammo_verylow, nullptr, true); // allow "no color"

static ColorCV sbColorWeaponName(&sb_color_weapon_name, nullptr, true); // allow "no color"

static ColorCV sbColorAutomapMapName(&sb_color_automap_mapname, nullptr, true); // allow "no color"
static ColorCV sbColorAutomapMapCluster(&sb_color_automap_mapcluster, nullptr, true); // allow "no color"
static ColorCV sbColorAutomapKills(&sb_color_automap_kills, nullptr, true); // allow "no color"
static ColorCV sbColorAutomapItems(&sb_color_automap_items, nullptr, true); // allow "no color"
static ColorCV sbColorAutomapSecrets(&sb_color_automap_secrets, nullptr, true); // allow "no color"
static ColorCV sbColorAutomapTotalTime(&sb_color_automap_totaltime, nullptr, true); // allow "no color"


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
//  retDefColorU
//
//==========================================================================
static inline vuint32 retDefColorU (const vuint32 clr, const vuint32 def) noexcept {
  return (clr ? clr : def);
}


//==========================================================================
//
//  SB_GetTextColor
//
//==========================================================================
vuint32 SB_GetTextColor (int type, vuint32 defval) {
  if (defval == 0xdeadf00dU) defval = CR_UNTRANSLATED;
  switch (type) {
    case SBTC_Ammo1: return retDefColorU(sbColorAmmo1.getColor(), defval);
    case SBTC_Ammo2: return retDefColorU(sbColorAmmo2.getColor(), defval);
    case SBTC_Armor: return retDefColorU(sbColorArmor.getColor(), defval);
    case SBTC_Health: return retDefColorU(sbColorHealth.getColor(), defval);
    case SBTC_HealthAccum: return retDefColorU(sbColorHealthAccum.getColor(), defval);
    case SBTC_Frags: return retDefColorU(sbColorFrags.getColor(), defval);
    case SBTC_SmallAmmo: return retDefColorU(sbColorSmallAmmo.getColor(), defval);
    case SBTC_SmallMaxAmmo: return retDefColorU(sbColorSmallAmmoMax.getColor(), defval);
    case SBTC_ItemAmount: return retDefColorU(sbColorItemAmount.getColor(), defval);
    case SBTC_WeaponAmmoFull: return retDefColorU(sbColorWeaponAmmoFull.getColor(), CR_WHITE);
    case SBTC_WeaponAmmoNormal: return retDefColorU(sbColorWeaponAmmoNormal.getColor(), CR_GREEN);
    case SBTC_WeaponAmmoLower: return retDefColorU(sbColorWeaponAmmoLower.getColor(), CR_ORANGE);
    case SBTC_WeaponAmmoLow: return retDefColorU(sbColorWeaponAmmoLow.getColor(), CR_YELLOW);
    case SBTC_WeaponAmmoVeryLow: return retDefColorU(sbColorWeaponAmmoVeryLow.getColor(), CR_RED);
    case SBTC_WeaponName: return retDefColorU(sbColorWeaponName.getColor(), CR_GOLD);
    case SBTC_AutomapMapName: return retDefColorU(sbColorAutomapMapName.getColor(), defval);
    case SBTC_AutomapMapCluster: return retDefColorU(sbColorAutomapMapCluster.getColor(), defval);
    case SBTC_AutomapKills: return retDefColorU(sbColorAutomapKills.getColor(), CR_RED);
    case SBTC_AutomapItems: return retDefColorU(sbColorAutomapItems.getColor(), CR_GREEN);
    case SBTC_AutomapSecrets: return retDefColorU(sbColorAutomapSecrets.getColor(), CR_GOLD);
    case SBTS_AutomapGameTotalTime: return retDefColorU(sbColorAutomapTotalTime.getColor(), defval);
    default: break;
  }
  return CR_UNTRANSLATED;
}



//==========================================================================
//
//  VC API
//
//==========================================================================


//==========================================================================
//
//  retDefColor
//
//==========================================================================
static inline int retDefColor (const vuint32 clr, const int def) noexcept {
  return (clr ? (int)clr : (int)def);
}


//native static final int SB_GetTextColor (int type, optional int defval);
IMPLEMENT_FREE_FUNCTION(VObject, SB_GetTextColor) {
  int type;
  VOptParamInt defval(CR_UNTRANSLATED);
  vobjGetParam(type, defval);
  RET_INT((int)SB_GetTextColor(type, (unsigned)defval.value));
}
