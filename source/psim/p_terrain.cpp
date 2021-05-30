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


static VCvarB gm_vanilla_liquids("gm_vanilla_liquids", true, "Allow liquid sounds for Vanilla Doom?", CVAR_Archive);


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
static TArray<VTerrainBootprint *> TerrainBootprints;

static TMapNC<VName, int> SplashMap; // key: lowercased name; value: index in `SplashInfos`
static TMapNC<VName, int> TerrainMap; // key: lowercased name; value: index in `TerrainInfos`
static TMapNC<int, int> TerrainTypeMap; // key: pic number; value: index in `TerrainTypes`
static TMapNC<int, VTerrainBootprint *> TerrainBootprintMap; // key: pic number
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
//  FindBootprint
//
//==========================================================================
static VTerrainBootprint *FindBootprint (const char *bpname, bool allowCreation, const char *tname=nullptr) {
  if (!bpname || !bpname[0] || VStr::strEquCI(bpname, "none")) return nullptr;
  for (VTerrainBootprint *bp : TerrainBootprints) {
    if (bp->OrigName.strEquCI(bpname)) return bp;
  }
  if (!allowCreation) {
    if (tname) Sys_Error("cannot find bootprint definition named '%s' for terrain '%s'", bpname, tname);
    Sys_Error("cannot find bootprint definition named '%s'", bpname);
  }
  // create forward definition
  VTerrainBootprint *bp = new VTerrainBootprint;
  memset((void *)bp, 0, sizeof(VTerrainBootprint));
  bp->OrigName = bpname;
  TerrainBootprints.append(bp);
  return bp;
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
//  ParseTerrainSplashDef
//
//  "splash" is already eaten
//
//==========================================================================
static void ParseTerrainSplashDef (VScriptParser *sc) {
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
}


//==========================================================================
//
//  ParseTerrainTerrainDef
//
//  "terrain" is already eaten
//
//==========================================================================
static void ParseTerrainTerrainDef (VScriptParser *sc, int tkw) {
  sc->ExpectString();
  if (sc->String.isEmpty()) sc->String = "none";
  VStr TerName = sc->String;
  bool modify = false;
  VTerrainInfo *TInfo;
  if (tkw == 2) {
    // default terrain definition, remember new default terrain
    DefaultTerrainNameStr = TerName;
    DefaultTerrainName = VName(*TerName, VName::AddLower);
    // if just a name, do nothing else
    if (sc->PeekChar() != '{') return;
  } else {
    if (sc->Check("modify")) modify = true;
  }
  //GCon->Logf(NAME_Debug, "terraindef '%s'...", *TerName);
  // new terrain
  TInfo = GetTerrainInfo(*TerName);
  if (!TInfo) {
    //GCon->Logf(NAME_Debug, "%s: new terrain '%s'", *sc->GetLoc().toStringNoCol(), *TerName);
    VName nn = VName(*TerName, VName::AddLower);
    TInfo = &TerrainInfos.Alloc();
    memset((void *)TInfo, 0, sizeof(VTerrainInfo));
    TInfo->Name = nn;
    TInfo->OrigName = TerName;
    TerrainMap.put(nn, TerrainInfos.length()-1);
    modify = false; // clear it
  }
  /*
  else {
    GCon->Logf(NAME_Debug, "%s: %s terrain '%s'", *sc->GetLoc().toStringNoCol(), (modify ? "modifying" : "redefining"), *TerName);
  }
  */
  // clear
  if (!modify) {
    TInfo->Splash = NAME_None;
    TInfo->Flags = 0;
    TInfo->FootClip = 0.0f;
    TInfo->DamageTimeMask = 0;
    TInfo->DamageAmount = 0;
    TInfo->DamageType = NAME_None;
    TInfo->Friction = 0.0f;
    TInfo->MoveFactor = 1.0f; // this seems to be unused
    TInfo->StepVolume = 1.0f; // this seems to be unused
    TInfo->WalkingStepTime = 0.0f; // this seems to be unused
    TInfo->RunningStepTime = 0.0f; // this seems to be unused
    TInfo->LeftStepSounds = NAME_None;
    TInfo->RightStepSounds = NAME_None;
    TInfo->BootPrint = nullptr;
  }
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
        if (sc->Check("noprotection")) { TInfo->Flags &= ~VTerrainInfo::F_AllowProtection; continue; }
        if (sc->Check("playeronly")) { TInfo->Flags |= VTerrainInfo::F_PlayerOnly; continue; }
        if (sc->Check("everybody")) { TInfo->Flags &= ~VTerrainInfo::F_PlayerOnly; continue; }
        if (sc->Check("optout")) { TInfo->Flags |= VTerrainInfo::F_OptOut; continue; }
        if (sc->Check("nooptout")) { TInfo->Flags &= ~VTerrainInfo::F_OptOut; continue; }
        if (sc->Check("bootprint")) {
          sc->ExpectString();
          TInfo->BootPrint = FindBootprint(*sc->String, true);
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
}


//==========================================================================
//
//  ParseTerrainBootPrintDef
//
//  "bootprint" is already eaten
//
//==========================================================================
static void ParseTerrainBootPrintDef (VScriptParser *sc) {
  sc->ExpectString();
  if (sc->String.isEmpty()) sc->String = "none";
  if (sc->String.strEquCI("none")) sc->Error("invalid bootprint name");
  VTerrainBootprint *bp = FindBootprint(*sc->String, true);
  vassert(bp); // invariant
  constexpr float BOOT_TIME_MIN = 3.8f;
  constexpr float BOOT_TIME_MAX = 4.2f;
  if (sc->Check("modify")) {
    // do nothing (but init times)
    if (bp->Name == NAME_None) {
      bp->TimeMin = BOOT_TIME_MIN;
      bp->TimeMax = BOOT_TIME_MAX;
    }
  } else {
    // remove from bootprint map
    if (bp->Name != NAME_None) {
      TArray<int> rmidx;
      for (auto &&it : TerrainBootprintMap.first()) {
        if (it.value() == bp) rmidx.append(it.key());
      }
      for (int ri : rmidx) TerrainBootprintMap.del(ri);
    }
    // clear
    bp->DecalName = NAME_None;
    bp->TimeMin = BOOT_TIME_MIN;
    bp->TimeMax = BOOT_TIME_MAX;
    bp->Translation = 0;
  }
  bp->Name = VName(*bp->OrigName, VName::AddLower);
  sc->Check("{");
  while (!sc->Check("}")) {
    if (sc->AtEnd()) sc->Error(va("unfinished bootprint '%s' definition", *bp->OrigName));

    // decal name
    if (sc->Check("decal")) {
      sc->ExpectString();
      bp->DecalName = (sc->String.isEmpty() ? NAME_None : VName(*sc->String));
      continue;
    }

    // times
    if (sc->Check("time")) {
      if (sc->Check("default")) {
        bp->TimeMin = BOOT_TIME_MIN;
        bp->TimeMax = BOOT_TIME_MAX;
      } else {
        sc->ExpectFloat();
        float tmin = sc->Float;
        sc->ExpectFloat();
        float tmax = sc->Float;
        if (tmin > tmax) { const float tt = tmin; tmin = tmax; tmax = tt; }
        bp->TimeMin = tmin;
        bp->TimeMax = tmax;
      }
      continue;
    }

    // flat name glob
    if (sc->Check("flat")) {
      sc->ExpectString();
      VStr mask = sc->String.xstrip();
      // sorry for this pasta
      if (mask.isEmpty()) continue;
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
        if (!goodtx) continue;
        if (!VStr::globMatchCI(*tn, *mask)) continue;
        // found texture
        TerrainBootprintMap.put(pic, bp);
      }
    }
  }
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
      ParseTerrainSplashDef(sc);
      continue;
    }

    // terrain definition?
    if ((tkw = CheckTerrainKW(sc)) != 0) {
      ParseTerrainTerrainDef(sc, tkw);
      continue;
    }

    // bootprint definition?
    if (sc->Check("bootprint")) {
      ParseTerrainBootPrintDef(sc);
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

  // check bootprints
  bool wasErrors = false;
  for (auto &&ti : TerrainInfos) {
    if (ti.BootPrint && ti.BootPrint->Name == NAME_None) {
      wasErrors = true;
      GCon->Logf(NAME_Error, "terrain '%s' references unknown bootprint '%s'", *ti.OrigName, *ti.BootPrint->OrigName);
    }
  }
  if (wasErrors) Sys_Error("error(s) in terrain definitions");

  for (auto &&it : TerrainBootprintMap.first()) {
    VTerrainBootprint *bp = it.value();
    if (bp->Name == NAME_None) {
      wasErrors = true;
      GCon->Logf(NAME_Error, "texture '%s' referenced unknown bootprint '%s'", *GTextureManager.GetTextureName(it.key()), *bp->OrigName);
    }
  }
  if (wasErrors) Sys_Error("error(s) in bootprint definitions");
}


//==========================================================================
//
//  SV_TerrainType
//
//==========================================================================
VTerrainInfo *SV_TerrainType (int pic, bool asPlayer) {
  if (pic <= 0) return &TerrainInfos[DefaultTerrainIndex];
  auto pp = TerrainTypeMap.find(pic);
  VTerrainInfo *ter = (pp ? TerrainTypes[*pp].Info : nullptr);
  if (ter && !asPlayer && (ter->Flags&VTerrainInfo::F_PlayerOnly)) ter = nullptr;
  if (ter && (ter->Flags&VTerrainInfo::F_OptOut) && !gm_vanilla_liquids.asBool()) ter = nullptr;
  return (ter ? ter : &TerrainInfos[DefaultTerrainIndex]);
}


//==========================================================================
//
//  SV_TerrainBootprint
//
//==========================================================================
VTerrainBootprint *SV_TerrainBootprint (int pic) {
  if (pic <= 0) return nullptr;
  auto pp = TerrainBootprintMap.find(pic);
  if (pp) return *pp;
  // try terrain
  VTerrainInfo *ter = SV_TerrainType(pic, true/*asPlayer*/); // this is used ONLY for player
  if (ter) return ter->BootPrint;
  return nullptr;
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
  for (auto &&bp : TerrainBootprints) { delete bp; bp = nullptr; }
  TerrainBootprints.clear();
  TerrainBootprintMap.clear();
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
