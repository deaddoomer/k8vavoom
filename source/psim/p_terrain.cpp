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
//
//  Terrain types
//
//**************************************************************************
#include "../gamedefs.h"


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
static VName DefaultTerrainName;
static VStr DefaultTerrainNameStr;
static int DefaultTerrainIndex;


//==========================================================================
//
//  GetSplashInfo
//
//==========================================================================
static VSplashInfo *GetSplashInfo (const char *Name) {
  if (!Name || !Name[0]) return nullptr;
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
  if (!Name || !Name[0]) return &TerrainInfos[DefaultTerrainIndex]; // default one
  VName loname = VName(Name, VName::FindLower);
  if (loname == NAME_None) return nullptr;
  auto spp = TerrainMap.find(loname);
  return (spp ? &TerrainInfos[*spp] : nullptr);
}


//==========================================================================
//
//  CheckTerrainKW
//
//  returns:
//    0: nope
//    1: terrain definition
//    2: default terrain definition
//
//==========================================================================
static int CheckTerrainKW (VScriptParser *sc) {
  if (sc->Check("terrain")) return 1;
  return (sc->Check("defaultterrain") ? 2 : 0);
}


//==========================================================================
//
//  ProcessFlatGlobMask
//
//==========================================================================
static void ProcessFlatGlobMask (VStr mask, const char *tername) {
  if (!tername) tername = "";
  mask = mask.xstrip();
  if (mask.isEmpty()) return;
  //GCon->Logf(NAME_Debug, "checking flatglobmask <%s> (terrain: %s)", *mask, (tername ? tername : "<none>"));
  const int tcount = GTextureManager.GetNumTextures();
  for (int pic = 1; pic < tcount; ++pic) {
    VTexture *tx = GTextureManager.getIgnoreAnim(pic);
    if (!tx) continue;
    VName tn = tx->Name;
    if (tn == NAME_None) continue;
    bool goodtx = false;
    switch (tx->Type) {
      case TEXTYPE_Any: //FIXME: dunno
      case TEXTYPE_WallPatch:
      case TEXTYPE_Wall:
      case TEXTYPE_Flat:
      case TEXTYPE_Overload:
        goodtx = true;
        break;
      default: break;
    }
    if (!goodtx) {
      //GCon->Logf(NAME_Debug, "  rejected texture '%s'", *tn);
      continue;
    }
    if (!VStr::globMatchCI(*tn, *mask)) {
      //GCon->Logf(NAME_Debug, "  skipped texture '%s'", *tn);
      continue;
    }
    //GCon->Logf(NAME_Debug, "  matched texture '%s'", *tn);
    // found texture
    if (!tername[0]) {
      //WARNING! memory leak on deletion!
      TerrainTypeMap.del(pic);
      continue;
    }
    auto pp = TerrainTypeMap.find(pic);
    if (pp) {
      // replace old one
      TerrainTypes[*pp].TypeName = VName(tername);
    } else {
      //!GCon->Logf(NAME_Init, "%s: new terrain floor '%s' of type '%s' (pic=%d)", *sc->GetLoc().toStringNoCol(), *floorname, *sc->String, Pic);
      VTerrainType &T = TerrainTypes.Alloc();
      T.Pic = pic;
      T.TypeName = VName(tername);
      TerrainTypeMap.put(pic, TerrainTypes.length()-1);
    }
  }
}

//==========================================================================
//
//  ParseTerrainScript
//
//==========================================================================
static void ParseTerrainScript (VScriptParser *sc) {
  GCon->Logf(NAME_Init, "parsing terrain script '%s'", *sc->GetScriptName());
  bool insideIf = false;
  int tkw;
  while (!sc->AtEnd()) {
    auto loc = sc->GetLoc();
    // splash definition?
    if (sc->Check("splash")) {
      sc->ExpectString();
      if (sc->String.isEmpty()) sc->String = "none";
      VSplashInfo *SInfo = GetSplashInfo(*sc->String);
      if (!SInfo) {
        //!GCon->Logf(NAME_Init, "%s: new splash '%s'", *sc->GetLoc().toStringNoCol(), *sc->String);
        VName nn = VName(*sc->String, VName::AddLower);
        SInfo = &SplashInfos.Alloc();
        SInfo->Name = nn;
        SInfo->OrigName = sc->String;
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
        if (sc->AtEnd()) break;
        if (sc->Check("smallclass")) {
          sc->ExpectString();
          SInfo->SmallClass = VClass::FindClass(*sc->String);
          continue;
        }
        if (sc->Check("smallclip")) {
          sc->ExpectFloat();
          SInfo->SmallClip = sc->Float;
          continue;
        }
        if (sc->Check("smallsound")) {
          sc->ExpectString();
          SInfo->SmallSound = *sc->String;
          continue;
        }
        if (sc->Check("baseclass")) {
          sc->ExpectString();
          SInfo->BaseClass = VClass::FindClass(*sc->String);
          continue;
        }
        if (sc->Check("chunkclass")) {
          sc->ExpectString();
          SInfo->ChunkClass = VClass::FindClass(*sc->String);
          continue;
        }
        if (sc->Check("chunkxvelshift")) {
          sc->ExpectNumber();
          SInfo->ChunkXVelMul = sc->Number < 0 ? 0.0f : float((1<<sc->Number)/256);
          continue;
        }
        if (sc->Check("chunkyvelshift")) {
          sc->ExpectNumber();
          SInfo->ChunkYVelMul = sc->Number < 0 ? 0.0f : float((1<<sc->Number)/256);
          continue;
        }
        if (sc->Check("chunkzvelshift")) {
          sc->ExpectNumber();
          SInfo->ChunkZVelMul = sc->Number < 0 ? 0.0f : float((1<<sc->Number)/256);
          continue;
        }
        if (sc->Check("chunkbasezvel")) {
          sc->ExpectFloat();
          SInfo->ChunkBaseZVel = sc->Float;
          continue;
        }
        if (sc->Check("sound")) {
          sc->ExpectString();
          SInfo->Sound = *sc->String;
          continue;
        }
        if (sc->Check("noalert")) {
          SInfo->Flags |= VSplashInfo::F_NoAlert;
          continue;
        }
        sc->Error(va("Unknown splash command (%s)", *sc->String));
      }
      continue;
    }

    // terrain definition?
    if ((tkw = CheckTerrainKW(sc)) != 0) {
      sc->ExpectString();
      if (sc->String.isEmpty()) sc->String = "none";
      VTerrainInfo *TInfo;
      if (tkw == 2) {
        // default terrain definition, remember new default terrain
        DefaultTerrainNameStr = sc->String;
        DefaultTerrainName = VName(*sc->String, VName::AddLower);
        // if just a name, do nothing else
        if (sc->PeekChar() != '{') continue;
      }
      //GCon->Logf(NAME_Debug, "terraindef '%s'...", *sc->String);
      // new terrain
      TInfo = GetTerrainInfo(*sc->String);
      if (!TInfo) {
        //!GCon->Logf(NAME_Init, "%s: new terrain '%s'", *sc->GetLoc().toStringNoCol(), *sc->String);
        VName nn = VName(*sc->String, VName::AddLower);
        TInfo = &TerrainInfos.Alloc();
        memset((void *)TInfo, 0, sizeof(VTerrainInfo));
        TInfo->Name = nn;
        TInfo->OrigName = sc->String;
        TerrainMap.put(nn, TerrainInfos.length()-1);
      }
      TInfo->Splash = NAME_None;
      TInfo->BootPrintDecal = NAME_None;
      TInfo->Flags = 0;
      TInfo->FootClip = 0;
      TInfo->DamageTimeMask = 0;
      TInfo->DamageAmount = 0;
      TInfo->DamageType = NAME_None;
      TInfo->Friction = 0.0f;
      constexpr float BOOT_TIME_MIN = 4.0f;
      constexpr float BOOT_TIME_MAX = 4.0f;
      TInfo->BootPrintTimeMin = BOOT_TIME_MIN;
      TInfo->BootPrintTimeMax = BOOT_TIME_MAX;
      sc->Expect("{");
      while (!sc->Check("}")) {
        if (sc->AtEnd()) break;
        //GCon->Logf(NAME_Debug, "  terraindef '%s': <%s>", *TInfo->OrigName, *sc->String);
        if (sc->Check("splash")) {
          sc->ExpectString();
          TInfo->Splash = *sc->String;
          continue;
        }
        if (sc->Check("liquid")) {
          TInfo->Flags |= VTerrainInfo::F_Liquid;
          continue;
        }
        if (sc->Check("footclip")) {
          sc->ExpectFloat();
          TInfo->FootClip = sc->Float;
          continue;
        }
        if (sc->Check("damagetimemask")) {
          sc->ExpectNumber();
          TInfo->DamageTimeMask = sc->Number;
          continue;
        }
        if (sc->Check("damageamount")) {
          sc->ExpectNumber();
          TInfo->DamageAmount = sc->Number;
          continue;
        }
        if (sc->Check("damagetype")) {
          sc->ExpectString();
          TInfo->DamageType = *sc->String;
          continue;
        }
        if (sc->Check("friction")) {
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
          continue;
        }
        if (sc->Check("stepvolume")) {
          sc->ExpectFloat();
          TInfo->StepVolume = sc->Float;
          continue;
        }
        if (sc->Check("walkingsteptime")) {
          sc->ExpectFloat();
          TInfo->WalkingStepTime = sc->Float;
          continue;
        }
        if (sc->Check("runningsteptime")) {
          sc->ExpectFloat();
          TInfo->RunningStepTime = sc->Float;
          continue;
        }
        if (sc->Check("leftstepsounds")) {
          sc->ExpectString();
          TInfo->LeftStepSounds = *sc->String;
          continue;
        }
        if (sc->Check("rightstepsounds")) {
          sc->ExpectString();
          TInfo->RightStepSounds = *sc->String;
          continue;
        }
        if (sc->Check("allowprotection")) {
          TInfo->Flags |= VTerrainInfo::F_AllowProtection;
          continue;
        }
        // k8vavoom extensions
        if (sc->Check("k8vavoom")) {
          sc->Expect("{");
          while (!sc->Check("}")) {
            if (sc->AtEnd()) break;
            if (sc->Check("bootprintdecal")) {
              sc->ExpectString();
              if (sc->String.strEquCI("none")) sc->String.clear();
              TInfo->BootPrintDecal = VName(*sc->String);
              continue;
            }
            if (sc->Check("bootprinttime")) {
              if (sc->Check("default")) {
                TInfo->BootPrintTimeMin = BOOT_TIME_MIN;
                TInfo->BootPrintTimeMax = BOOT_TIME_MAX;
              } else {
                sc->ExpectFloat();
                float tmin = sc->Float;
                sc->ExpectFloat();
                float tmax = sc->Float;
                if (tmin > tmax) { const float tt = tmin; tmin = tmax; tmax = tt; }
                TInfo->BootPrintTimeMin = tmin;
                TInfo->BootPrintTimeMax = tmax;
              }
              continue;
            }
            if (sc->Check("detectfloorflat")) { // k8vavoom extension
              sc->ExpectString();
              ProcessFlatGlobMask(sc->String, *TInfo->Name);
              continue;
            }
            sc->Error(va("Unknown k8vavoom terrain extension command (%s)", *sc->String));
          }
          continue;
        }
        sc->Error(va("Unknown terrain command (%s)", *sc->String));
      }
      continue;
    }

    // floor detection definition?
    if (sc->Check("floor")) {
      sc->Check("optional"); // ignore it
      sc->ExpectName8Warn();
      VName floorname = sc->Name8;
      int Pic = GTextureManager.CheckNumForName(floorname, TEXTYPE_Flat, false);
      if (Pic <= 0) continue;
      sc->ExpectString();
      if (sc->String.isEmpty()) sc->String = "none"; //k8:???
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
      continue;
    }

    // floor detection by globmask definition?
    if (sc->Check("floorglob")) {
      sc->ExpectString();
      VStr mask = sc->String;
      sc->ExpectString();
      VStr tername = sc->String;
      ProcessFlatGlobMask(mask, *tername);
      continue;
    }

    // endif?
    if (sc->Check("endif")) {
      if (insideIf) {
        insideIf = false;
      } else {
        GCon->Logf(NAME_Warning, "%s: stray `endif` in terrain script", *loc.toStringNoCol());
      }
      continue;
    }

    // ifdefs?
    if (sc->Check("ifdoom") || sc->Check("ifheretic") ||
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
      continue;
    }
    sc->Error(va("Unknown command (%s)", *sc->String));
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
  DefT.OrigName = "Solid";
  DefT.Splash = NAME_None;
  DefT.Flags = 0;
  DefT.FootClip = 0;
  DefT.DamageTimeMask = 0;
  DefT.DamageAmount = 0;
  DefT.DamageType = NAME_None;
  DefT.Friction = 0.0f;

  DefaultTerrainName = DefT.Name;
  DefaultTerrainNameStr = DefT.OrigName;
  DefaultTerrainIndex = 0;

  for (auto &&it : WadNSNameIterator(NAME_terrain, WADNS_Global)) {
    const int Lump = it.lump;
    ParseTerrainScript(new VScriptParser(W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump)));
  }
  GCon->Logf(NAME_Init, "got %d terrain definition%s", TerrainInfos.length(), (TerrainInfos.length() != 1 ? "s" : ""));

  // fix default terrain name (why not?)
  {
    vassert(DefaultTerrainName != NAME_None);
    auto spp = TerrainMap.find(DefaultTerrainName);
    if (!spp) {
      GCon->Logf(NAME_Warning, "unknown default terrain '%s', defaulted to '%s'", *DefaultTerrainNameStr, *TerrainInfos[0].Name);
      DefaultTerrainName = TerrainInfos[0].Name;
      DefaultTerrainNameStr = TerrainInfos[0].OrigName;
      DefaultTerrainIndex = 0;
    } else {
      GCon->Logf(NAME_Init, "default terrain is '%s' (%d)", *DefaultTerrainNameStr, *spp);
      DefaultTerrainIndex = *spp;
      vassert(DefaultTerrainIndex >= 0 && DefaultTerrainIndex < TerrainInfos.length());
    }
  }

  // setup terrain type pointers
  for (auto &&ttinf : TerrainTypes) {
    ttinf.Info = GetTerrainInfo(*ttinf.TypeName);
    if (!ttinf.Info) {
      GCon->Logf(NAME_Warning, "unknown terrain type '%s' for texture '%s'", *ttinf.TypeName, (ttinf.Pic >= 0 ? *GTextureManager[ttinf.Pic]->Name : "<notexture>"));
      ttinf.Info = &TerrainInfos[DefaultTerrainIndex]; // default one
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
  auto pp = TerrainTypeMap.find(pic);
  return (pp ? TerrainTypes[*pp].Info : &TerrainInfos[DefaultTerrainIndex]);
}


//==========================================================================
//
//  SV_GetDefaultTerrain
//
//==========================================================================
VTerrainInfo *SV_GetDefaultTerrain () {
  return &TerrainInfos[DefaultTerrainIndex];
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
  VName Name;
  vobjGetParam(Name);
  if (Name == NAME_None) RET_PTR(nullptr); else RET_PTR(GetSplashInfo(*Name));
}

IMPLEMENT_FREE_FUNCTION(VObject, GetTerrainInfo) {
  VName Name;
  vobjGetParam(Name);
  if (Name == NAME_None) RET_PTR(nullptr); else RET_PTR(GetTerrainInfo(*Name));
}
