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
#include "gamedefs.h"

static VLockDef *LockDefs[256];


//==========================================================================
//
//  ParseLockDefs
//
//==========================================================================
static void ParseLockDefs (VScriptParser *sc) {
  while (!sc->AtEnd()) {
    if (sc->Check("ClearLocks")) {
      for (int i = 0; i < 256; ++i) {
        if (LockDefs[i]) {
          delete LockDefs[i];
          LockDefs[i] = nullptr;
        }
      }
      continue;
    }
    if (sc->Check("Lock")) {
      // lock number
      sc->ExpectNumber();
      int Lock = sc->Number;
      if (Lock <= 0 || Lock >= 255) sc->Error(va("Bad lock number (%d)", sc->Number));
      if (LockDefs[Lock]) {
        delete LockDefs[Lock];
        LockDefs[Lock] = nullptr;
      }
      VLockDef *LDef = new VLockDef;
      LockDefs[Lock] = LDef;
      LDef->MapColor = 0;
      LDef->LockedSound = "misc/keytry";

      // skip game specifier
      if (sc->Check("Doom") || sc->Check("Heretic") ||
          sc->Check("Hexen") || sc->Check("Strife") ||
          sc->Check("Chex"))
      {
      }

      sc->Expect("{");
      while (!sc->Check("}")) {
        if (sc->Check("Message")) {
          sc->ExpectString();
          LDef->Message = sc->String;
          continue;
        }
        if (sc->Check("RemoteMessage")) {
          sc->ExpectString();
          LDef->RemoteMessage = sc->String;
          continue;
        }
        if (sc->Check("MapColor")) {
          sc->ExpectNumber();
          int r = sc->Number;
          sc->ExpectNumber();
          int g = sc->Number;
          sc->ExpectNumber();
          int b = sc->Number;
          LDef->MapColor = 0xff000000 | (r << 16) | (g << 8) | b;
          continue;
        }
        if (sc->Check("LockedSound")) {
          sc->ExpectString();
          LDef->LockedSound = *sc->String;
          continue;
        }
        if (sc->Check("Any")) {
          sc->Expect("{");
          VLockGroup &Grp = LDef->Locks.Alloc();
          while (!sc->Check("}")) {
            sc->ExpectString();
            VClass *Cls = VClass::FindClassNoCase(*sc->String);
            if (!Cls) {
              GCon->Logf(NAME_Warning, "%s: No lockdef class '%s'", *sc->GetLoc().toStringNoCol(), *sc->String);
            } else {
              Grp.AnyKeyList.Append(Cls);
            }
          }
          continue;
        }
        sc->ExpectString();
        VClass *Cls = VClass::FindClassNoCase(*sc->String);
        if (!Cls) {
          GCon->Logf(NAME_Warning, "%s: No lockdef class '%s'", *sc->GetLoc().toStringNoCol(), *sc->String);
        } else {
          LDef->Locks.Alloc().AnyKeyList.Append(Cls);
        }
      }
      // copy message if other one is not defined
      if (LDef->Message.IsEmpty()) LDef->Message = LDef->RemoteMessage;
      if (LDef->RemoteMessage.IsEmpty()) LDef->RemoteMessage = LDef->Message;
      continue;
    }
    sc->Error(va("invalid lockdef command (%s)", *sc->String));
  }
  delete sc;
}


//==========================================================================
//
//  InitLockDefs
//
//==========================================================================
void InitLockDefs () {
  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    if (W_LumpName(Lump) == NAME_lockdefs) {
      GCon->Logf(NAME_Init, "Parsing lockdefs from '%s'", *W_FullLumpName(Lump));
      ParseLockDefs(new VScriptParser(W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump)));
    }
  }
}


//==========================================================================
//
//  ShutdownLockDefs
//
//==========================================================================
void ShutdownLockDefs () {
  for (int i = 0; i < 256; ++i) {
    if (LockDefs[i]) {
      delete LockDefs[i];
      LockDefs[i] = nullptr;
    }
  }
}


//==========================================================================
//
//  GetLockDef
//
//==========================================================================
VLockDef *GetLockDef (int Lock) {
  return (Lock < 0 || Lock > 255 ? nullptr : LockDefs[Lock]);
}
