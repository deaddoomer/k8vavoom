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
#ifndef VAVOOM_SBAR_HEADER
#define VAVOOM_SBAR_HEADER


// status bar height at bottom of screen
//#define SB_REALHEIGHT ((int)(sb_height * fScaleY))
//#define SB_REALHEIGHT ((int)(ScreenHeight/640.0f*sb_height*R_GetAspectRatio()))


void SB_Init ();
void SB_Drawer ();
void SB_Ticker ();
bool SB_Responder (event_t *ev);
void SB_Start (); // called when the console player is spawned on each level
int SB_RealHeight ();


extern int sb_height;


//WARNING! keep in sync with VC code!
enum {
  SBTC_Ammo1 = 0,
  SBTC_Ammo2,
  SBTC_Armor,
  SBTC_Health,
  SBTC_HealthAccum,
  SBTC_Frags,
  // small numbers
  SBTC_SmallAmmo,
  SBTC_SmallMaxAmmo,
  SBTC_ItemAmount,
  // FS weapon ammo counters
  SBTC_WeaponAmmoFull,
  SBTC_WeaponAmmoNormal,
  SBTC_WeaponAmmoLower,
  SBTC_WeaponAmmoLow,
  SBTC_WeaponAmmoVeryLow,
  //
  SBTC_WeaponName,
  // automap stats
  SBTC_AutomapMapName,
  SBTC_AutomapMapCluster,
  SBTC_AutomapKills,
  SBTC_AutomapItems,
  SBTC_AutomapSecrets,
  SBTS_AutomapGameTotalTime,
};

// type is SBTC_xxx
// returns CR_UNTRANSLATED for default
vuint32 SB_GetTextColor (int type, vuint32 defval=0xdeadf00dU);

#endif
