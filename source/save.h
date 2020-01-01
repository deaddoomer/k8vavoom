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
//**  Copyright (C) 2018-2020 Ketmar Dark
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

//extern void SV_SaveGame (int Slot, VStr Description, bool checkpoint, bool isAutosave);
extern void SV_MapTeleport (VName MapName, int flags=0, int newskill=-1);
//extern void SV_LoadGame (int Slot);
extern void SV_InitBaseSlot ();
//extern void SV_UpdateRebornSlot ();
//extern bool SV_LoadQuicksaveSlot ();
//extern void SV_SaveGameToReborn ();
extern bool SV_GetSaveString (int Slot, VStr &Desc);
extern void SV_GetSaveDateString (int Slot, VStr &datestr);
extern void SV_AutoSaveOnLevelExit ();
extern void SV_AutoSave (bool checkpoint);


//extern bool sv_autoenter_checkpoints;
enum LastLoadedMapType {
  LMT_Unknown, // nothing was loaded yet
  LMT_E1M1,
  LMT_MAP01,
  LMT_OtherFirstD1, // first map, but not from standard iwad
  LMT_OtherFirstD2, // first map, but not from standard iwad
  LMT_Other, // cannot detect map, stop detection
};

extern LastLoadedMapType mapLoaded;
