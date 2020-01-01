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
//
//  Terrain types
//
//**************************************************************************
#include "gamedefs.h"
#include "sv_local.h"


struct VSplashInfo {
  VName Name; // always lowercased

  VClass *SmallClass;
  float SmallClip;
  VName SmallSound;

  VClass *BaseClass;
  VClass *ChunkClass;
  float ChunkXVelMul;
  float ChunkYVelMul;
  float ChunkZVelMul;
  float ChunkBaseZVel;
  VName Sound;
  enum {
    F_NoAlert = 0x00000001,
  };
  vuint32 Flags;
};


struct VTerrainInfo {
  VName Name; // always lowercased
  VName Splash;
  enum {
    F_Liquid          = 0x00000001,
    F_AllowProtection = 0x00000002,
  };
  vuint32 Flags;
  float FootClip;
  vint32 DamageTimeMask;
  vint32 DamageAmount;
  VName DamageType;
  float Friction;
  float MoveFactor;
  float StepVolume;
  float WalkingStepTime;
  float RunningStepTime;
  VName LeftStepSounds;
  VName RightStepSounds;
};


struct VTerrainType {
  int Pic;
  VName TypeName;
  // it is safe to use pointer here, becasuse it is set after all
  // terrain data is loaded, and the pointer is immutable
  VTerrainInfo *Info;
};


// terrain types
static TArray<VSplashInfo> SplashInfos;
static TArray<VTerrainInfo> TerrainInfos;
static TArray<VTerrainType> TerrainTypes;

static TMapNC<VName, int> SplashMap; // key: lowercased name; value: index in `SplashInfos`
static TMapNC<VName, int> TerrainMap; // key: lowercased name; value: index in `TerrainInfos`
static TMapNC<int, int> TerrainTypeMap; // key: pic number; value: index in `TerrainTypes`


//==========================================================================
//
//  GetSplashInfo
//
//==========================================================================
static VSplashInfo *GetSplashInfo (const char *Name) {
  if (!Name || !Name[0]) Name = "solid"; // default one
  VName loname = VName(Name, VName::FindLower);
  if (loname == NAME_None) return nullptr;
  auto spp = SplashMap.find(loname);
  return (spp ? &SplashInfos[*spp] : nullptr);
  /*
  for (int i = 0; i < SplashInfos.Num(); ++i) {
    if (VStr::strEquCI(*SplashInfos[i].Name, *Name)) return &SplashInfos[i];
  }
  return nullptr;
  */
}


//==========================================================================
//
//  GetTerrainInfo
//
//==========================================================================
static VTerrainInfo *GetTerrainInfo (const char *Name) {
  if (!Name || !Name[0]) Name = "solid"; // default one
  VName loname = VName(Name, VName::FindLower);
  if (loname == NAME_None) return nullptr;
  auto spp = TerrainMap.find(loname);
  return (spp ? &TerrainInfos[*spp] : nullptr);
  /*
  for (int i = 0; i < TerrainInfos.Num(); ++i) {
    if (VStr::strEquCI(*TerrainInfos[i].Name, *Name)) return &TerrainInfos[i];
  }
  return nullptr;
  */
}


//==========================================================================
//
//  ParseTerrainScript
//
//==========================================================================
static void ParseTerrainScript (VScriptParser *sc) {
  GCon->Logf(NAME_Init, "parsing terrain script '%s'...", *sc->GetScriptName());
  bool insideIf = false;
  while (!sc->AtEnd()) {
    auto loc = sc->GetLoc();
    if (sc->Check("splash")) {
      sc->ExpectString();
      if (sc->String.isEmpty()) sc->String = "none";
      VSplashInfo *SInfo = GetSplashInfo(*sc->String);
      if (!SInfo) {
        //!GCon->Logf(NAME_Init, "%s: new splash '%s'", *sc->GetLoc().toStringNoCol(), *sc->String);
        VName nn = VName(*sc->String, VName::AddLower);
        SInfo = &SplashInfos.Alloc();
        SInfo->Name = nn;
        SplashMap.put(nn, SplashInfos.length()-1);
      }
      SInfo->SmallClass = nullptr;
      SInfo->SmallClip = 0;
      SInfo->SmallSound = NAME_None;
      SInfo->BaseClass = nullptr;
      SInfo->ChunkClass = nullptr;
      SInfo->ChunkXVelMul = 0;
      SInfo->ChunkYVelMul = 0;
      SInfo->ChunkZVelMul = 0;
      SInfo->ChunkBaseZVel = 0;
      SInfo->Sound = NAME_None;
      SInfo->Flags = 0;
      sc->Expect("{");
      while (!sc->Check("}")) {
        if (sc->Check("smallclass")) {
          sc->ExpectString();
          SInfo->SmallClass = VClass::FindClass(*sc->String);
        } else if (sc->Check("smallclip")) {
          sc->ExpectFloat();
          SInfo->SmallClip = sc->Float;
        } else if (sc->Check("smallsound")) {
          sc->ExpectString();
          SInfo->SmallSound = *sc->String;
        } else if (sc->Check("baseclass")) {
          sc->ExpectString();
          SInfo->BaseClass = VClass::FindClass(*sc->String);
        } else if (sc->Check("chunkclass")) {
          sc->ExpectString();
          SInfo->ChunkClass = VClass::FindClass(*sc->String);
        } else if (sc->Check("chunkxvelshift")) {
          sc->ExpectNumber();
          SInfo->ChunkXVelMul = sc->Number < 0 ? 0.0f : float((1<<sc->Number)/256);
        } else if (sc->Check("chunkyvelshift")) {
          sc->ExpectNumber();
          SInfo->ChunkYVelMul = sc->Number < 0 ? 0.0f : float((1<<sc->Number)/256);
        } else if (sc->Check("chunkzvelshift")) {
          sc->ExpectNumber();
          SInfo->ChunkZVelMul = sc->Number < 0 ? 0.0f : float((1<<sc->Number)/256);
        } else if (sc->Check("chunkbasezvel")) {
          sc->ExpectFloat();
          SInfo->ChunkBaseZVel = sc->Float;
        } else if (sc->Check("sound")) {
          sc->ExpectString();
          SInfo->Sound = *sc->String;
        } else if (sc->Check("noalert")) {
          SInfo->Flags |= VSplashInfo::F_NoAlert;
        } else {
          sc->Error(va("Unknown command (%s)", *sc->String));
        }
      }
    } else if (sc->Check("terrain")) {
      sc->ExpectString();
      if (sc->String.isEmpty()) sc->String = "none";
      VTerrainInfo *TInfo = GetTerrainInfo(*sc->String);
      if (!TInfo) {
        //!GCon->Logf(NAME_Init, "%s: new terrain '%s'", *sc->GetLoc().toStringNoCol(), *sc->String);
        VName nn = VName(*sc->String, VName::AddLower);
        TInfo = &TerrainInfos.Alloc();
        TInfo->Name = nn;
        TerrainMap.put(nn, TerrainInfos.length()-1);
      }
      TInfo->Splash = NAME_None;
      TInfo->Flags = 0;
      TInfo->FootClip = 0;
      TInfo->DamageTimeMask = 0;
      TInfo->DamageAmount = 0;
      TInfo->DamageType = NAME_None;
      TInfo->Friction = 0.0f;
      sc->Expect("{");
      while (!sc->Check("}")) {
        if (sc->Check("splash")) {
          sc->ExpectString();
          TInfo->Splash = *sc->String;
        } else if (sc->Check("liquid")) {
          TInfo->Flags |= VTerrainInfo::F_Liquid;
        } else if (sc->Check("footclip")) {
          sc->ExpectFloat();
          TInfo->FootClip = sc->Float;
        } else if (sc->Check("damagetimemask")) {
          sc->ExpectNumber();
          TInfo->DamageTimeMask = sc->Number;
        } else if (sc->Check("damageamount")) {
          sc->ExpectNumber();
          TInfo->DamageAmount = sc->Number;
        } else if (sc->Check("damagetype")) {
          sc->ExpectString();
          TInfo->DamageType = *sc->String;
        } else if (sc->Check("friction")) {
          sc->ExpectFloat();
          int friction, movefactor;

          // same calculations as in Sector_SetFriction special
          // a friction of 1.0 is equivalent to ORIG_FRICTION

          friction = (int)(0x1EB8*(sc->Float*100))/0x80+0xD001;
          friction = midval(0, friction, 0x10000);

          if (friction > 0xe800) {
            // ice
            movefactor = ((0x10092-friction)*1024)/4352+568;
          } else {
            movefactor = ((friction-0xDB34)*(0xA))/0x80;
          }

          if (movefactor < 32) movefactor = 32;

          TInfo->Friction = (1.0f-(float)friction/(float)0x10000)*35.0f;
          TInfo->MoveFactor = float(movefactor)/float(0x10000);
        } else if (sc->Check("stepvolume")) {
          sc->ExpectFloat();
          TInfo->StepVolume = sc->Float;
        } else if (sc->Check("walkingsteptime")) {
          sc->ExpectFloat();
          TInfo->WalkingStepTime = sc->Float;
        } else if (sc->Check("runningsteptime")) {
          sc->ExpectFloat();
          TInfo->RunningStepTime = sc->Float;
        } else if (sc->Check("leftstepsounds")) {
          sc->ExpectString();
          TInfo->LeftStepSounds = *sc->String;
        } else if (sc->Check("rightstepsounds")) {
          sc->ExpectString();
          TInfo->RightStepSounds = *sc->String;
        } else if (sc->Check("allowprotection")) {
          TInfo->Flags |= VTerrainInfo::F_AllowProtection;
        } else {
          sc->Error(va("Unknown terrain command (%s)", *sc->String));
        }
      }
    } else if (sc->Check("floor")) {
      sc->Check("optional"); // ignore it
      sc->ExpectName8Warn();
      VName floorname = sc->Name8;
      int Pic = GTextureManager.CheckNumForName(floorname, TEXTYPE_Flat, false);
      sc->ExpectString();
      if (sc->String.isEmpty()) sc->String = "none";
      auto pp = TerrainTypeMap.find(Pic);
      if (pp) {
        // replace old one
        TerrainTypes[*pp].TypeName = VName(*sc->String, VName::AddLower);
      } else {
        //!GCon->Logf(NAME_Init, "%s: new terrain floor '%s' of type '%s' (pic=%d)", *sc->GetLoc().toStringNoCol(), *floorname, *sc->String, Pic);
        VTerrainType &T = TerrainTypes.Alloc();
        T.Pic = Pic;
        T.TypeName = VName(*sc->String, VName::AddLower);
        TerrainTypeMap.put(Pic, TerrainTypes.length()-1);
      }
      /*
      bool Found = false;
      for (int i = 0; i < TerrainTypes.Num(); ++i) {
        if (TerrainTypes[i].Pic == Pic) {
          TerrainTypes[i].TypeName = *sc->String;
          Found = true;
          break;
        }
      }
      if (!Found) {
        VTerrainType &T = TerrainTypes.Alloc();
        T.Pic = Pic;
        T.TypeName = *sc->String;
      }
      */
    } else if (sc->Check("endif")) {
      if (insideIf) {
        insideIf = false;
      } else {
        GCon->Logf(NAME_Warning, "%s: stray `endif` in terrain script", *loc.toStringNoCol());
      }
    } else if (sc->Check("ifdoom") || sc->Check("ifheretic") ||
               sc->Check("ifhexen") || sc->Check("ifstrife"))
    {
      if (insideIf) {
        sc->Error(va("nested conditionals are not allowed (%s)", *sc->String));
      }
      //GCon->Logf(NAME_Warning, "%s: k8vavoom doesn't support conditional game commands in terrain script", *loc.toStringNoCol());
      VStr gmname = VStr(*sc->String+2);
      if (game_name.asStr().startsWithCI(gmname)) {
        insideIf = true;
        GCon->Logf(NAME_Init, "%s: processing conditional section '%s' in terrain script", *loc.toStringNoCol(), *gmname);
      } else {
        // skip lines until we hit `endif`
        GCon->Logf(NAME_Init, "%s: skipping conditional section '%s' in terrain script", *loc.toStringNoCol(), *gmname);
        while (sc->GetString()) {
          if (sc->Crossed) {
            if (sc->String.strEqu("endif")) {
              //GCon->Logf(NAME_Init, "******************** FOUND ENDIF!");
              break;
            }
          }
        }
      }
    } else {
      sc->Error(va("Unknown command (%s)", *sc->String));
    }
  }
  delete sc;
}


//==========================================================================
//
// P_InitTerrainTypes
//
//==========================================================================
void P_InitTerrainTypes () {
  // create default terrain
  VTerrainInfo &DefT = TerrainInfos.Alloc();
  VName nn = VName("Solid", VName::AddLower);
  TerrainMap.put(nn, TerrainInfos.length()-1);
  DefT.Name = nn;
  DefT.Splash = NAME_None;
  DefT.Flags = 0;
  DefT.FootClip = 0;
  DefT.DamageTimeMask = 0;
  DefT.DamageAmount = 0;
  DefT.DamageType = NAME_None;
  DefT.Friction = 0.0f;

  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    if (W_LumpName(Lump) == NAME_terrain) {
      ParseTerrainScript(new VScriptParser(W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump)));
    }
  }

  // setup terrain type pointers
  for (auto &&ttinf : TerrainTypes) {
    ttinf.Info = GetTerrainInfo(*ttinf.TypeName);
    if (!ttinf.Info) {
      GCon->Logf(NAME_Warning, "unknown terrain type '%s' for texture '%s'", *ttinf.TypeName, (ttinf.Pic >= 0 ? *GTextureManager[ttinf.Pic]->Name : "<notexture>"));
      ttinf.Info = &TerrainInfos[0]; // default one
    } else {
      //GCon->Logf(NAME_Warning, "set terrain type '%s' (%s) for texture '%s'", *ttinf.TypeName, *ttinf.Info->Name, (ttinf.Pic >= 0 ? *GTextureManager[ttinf.Pic]->Name : "<notexture>"));
    }
  }
}


//==========================================================================
//
//  SV_TerrainType
//
//==========================================================================
VTerrainInfo *SV_TerrainType (int pic) {
  /*
  for (int i = 0; i < TerrainTypes.Num(); ++i) {
    if (TerrainTypes[i].Pic == pic) return TerrainTypes[i].Info;
  }
  */
  auto pp = TerrainTypeMap.find(pic);
  return (pp ? TerrainTypes[*pp].Info : &TerrainInfos[0]);
}


//==========================================================================
//
// P_FreeTerrainTypes
//
//==========================================================================
void P_FreeTerrainTypes () {
  SplashInfos.Clear();
  TerrainInfos.Clear();
  TerrainTypes.Clear();
  SplashMap.clear();
  TerrainMap.clear();
  TerrainTypeMap.clear();
}


// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_FREE_FUNCTION(VObject, GetSplashInfo) {
  P_GET_NAME(Name);
  if (Name == NAME_None) RET_PTR(nullptr); else RET_PTR(GetSplashInfo(*Name));
}

IMPLEMENT_FREE_FUNCTION(VObject, GetTerrainInfo) {
  P_GET_NAME(Name);
  if (Name == NAME_None) RET_PTR(nullptr); else RET_PTR(GetTerrainInfo(*Name));
}
