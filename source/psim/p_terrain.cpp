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
#include "p_entity.h"


static VCvarB gm_optional_terrain("gm_optional_terrain", true, "Allow liquid sounds for Vanilla Doom?", CVAR_Archive);


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
static TMap<VStrCI, VTerrainBootprint *> TerrainBootprintNameMap;
static VName DefaultTerrainName;
static VStr DefaultTerrainNameStr;
static int DefaultTerrainIndex;

static bool GlobalDisableOverride = false;
static TMap<VStrCI, bool> NoOverrideSplash;
static TMap<VStrCI, bool> NoOverrideTerrain;
static TMap<VStrCI, bool> NoOverrideBootprint;


//==========================================================================
//
//  IsSplashNoOverride
//
//==========================================================================
static inline bool IsSplashNoOverride (VStr name) noexcept {
  return NoOverrideSplash.has(name);
}


//==========================================================================
//
//  IsTerrainNoOverride
//
//==========================================================================
static inline bool IsTerrainNoOverride (VStr name) noexcept {
  return NoOverrideTerrain.has(name);
}


//==========================================================================
//
//  IsBootprintNoOverride
//
//==========================================================================
static inline bool IsBootprintNoOverride (VStr name) noexcept {
  return NoOverrideBootprint.has(name);
}


//==========================================================================
//
//  AddSplashNoOverride
//
//==========================================================================
static inline void AddSplashNoOverride (VStr name) noexcept {
  if (!name.isEmpty()) NoOverrideSplash.put(name, true);
}


//==========================================================================
//
//  AddTerrainNoOverride
//
//==========================================================================
static inline void AddTerrainNoOverride (VStr name) noexcept {
  if (!name.isEmpty()) NoOverrideTerrain.put(name, true);
}


//==========================================================================
//
//  AddBootprintNoOverride
//
//==========================================================================
static inline void AddBootprintNoOverride (VStr name) noexcept {
  if (!name.isEmpty()) NoOverrideBootprint.put(name, true);
}


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
  for (int i = 0; i < SplashInfos.length(); ++i) {
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
static void ProcessFlatGlobMask (VStr mask, const char *tername, VStr slocstr) {
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
      if (GlobalDisableOverride && IsTerrainNoOverride(VStr(TerrainTypes[*pp].TypeName))) {
        GCon->Logf(NAME_Init, "%s: skipped floor '%s' override to '%s' due to 'GlobalDisableOverride'", *slocstr, *tn, tername);
      } else {
        TerrainTypes[*pp].TypeName = VName(tername);
      }
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
//  ParseTerrainSplashClassName
//
//==========================================================================
static VClass *ParseTerrainSplashClassName (VScriptParser *sc) {
  sc->ExpectString();
  if (sc->String.isEmpty() || sc->String.strEquCI("none")) return nullptr;
  VClass *cls = VClass::FindClass(*sc->String);
  if (!cls) {
    sc->Message(va("splash class '%s' not found", *sc->String));
  } else if (!cls->IsChildOf(VEntity::StaticClass())) {
    sc->Message(va("splash class '%s' is not an Entity, ignored", *sc->String));
    cls = nullptr;
  }
  return cls;
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

  if (IsSplashNoOverride(sc->String)) {
    GCon->Logf(NAME_Init, "%s: skipped splash '%s' override due to 'GlobalDisableOverride'", *sc->GetLoc().toStringNoCol(), *sc->String);
    sc->Expect("{");
    sc->SkipBracketed(true/*bracketEaten*/);
    return;
  }

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

  if (GlobalDisableOverride) AddSplashNoOverride(SInfo->OrigName);

  sc->Expect("{");
  while (!sc->Check("}")) {
    if (sc->AtEnd()) break;
    if (sc->Check("smallclass")) {
      SInfo->SmallClass = ParseTerrainSplashClassName(sc);
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
      SInfo->BaseClass = ParseTerrainSplashClassName(sc);
      continue;
    }
    if (sc->Check("chunkclass")) {
      SInfo->ChunkClass = ParseTerrainSplashClassName(sc);
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

  if (IsTerrainNoOverride(sc->String)) {
    GCon->Logf(NAME_Init, "%s: skipped terrain '%s' override due to 'GlobalDisableOverride'", *sc->GetLoc().toStringNoCol(), *sc->String);
    sc->Check("modify");
    if (sc->Check("{")) sc->SkipBracketed(true/*bracketEaten*/);
    return;
  }

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

  if (GlobalDisableOverride) AddTerrainNoOverride(TInfo->OrigName);

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
    TInfo->MoveFactor = 0.0f;
    TInfo->WalkingStepVolume = 1.0f;
    TInfo->RunningStepVolume = 1.0f;
    TInfo->CrouchingStepVolume = 1.0f;
    TInfo->WalkingStepTime = 0.0f;
    TInfo->RunningStepTime = 0.0f;
    TInfo->CrouchingStepTime = 0.0f;
    TInfo->LeftStepSound = NAME_None;
    TInfo->RightStepSound = NAME_None;
    TInfo->LandVolume = 1.0f;
    TInfo->LandSound = NAME_None;
    TInfo->SmallLandVolume = 1.0f;
    TInfo->SmallLandSound = NAME_None;
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
    if (sc->Check("damageonland")) {
      TInfo->Flags |= VTerrainInfo::F_DamageOnLand;
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
        if (sc->Check("notliqid")) { TInfo->Flags &= ~VTerrainInfo::F_Liquid; continue; }
        if (sc->Check("nodamageonland")) { TInfo->Flags &= ~VTerrainInfo::F_DamageOnLand; continue; }
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
          ProcessFlatGlobMask(sc->String, *TInfo->Name, sc->GetLoc().toStringNoCol());
          continue;
        }
        // footsteps
        if (sc->Check("walkingstepvolume")) { sc->ExpectFloat(); TInfo->WalkingStepVolume = sc->Float; continue; }
        if (sc->Check("runningstepvolume")) { sc->ExpectFloat(); TInfo->RunningStepVolume = sc->Float; continue; }
        if (sc->Check("crouchingstepvolume")) { sc->ExpectFloat(); TInfo->CrouchingStepVolume = sc->Float; continue; }
        if (sc->Check("allstepvolume")) { sc->ExpectFloat(); TInfo->WalkingStepVolume = TInfo->RunningStepVolume = TInfo->CrouchingStepVolume = sc->Float; continue; }
        if (sc->Check("walkingsteptime")) { sc->ExpectFloat(); TInfo->WalkingStepTime = sc->Float; continue; }
        if (sc->Check("runningsteptime")) { sc->ExpectFloat(); TInfo->RunningStepTime = sc->Float; continue; }
        if (sc->Check("crouchingsteptime")) { sc->ExpectFloat(); TInfo->CrouchingStepTime = sc->Float; continue; }
        if (sc->Check("allsteptime")) { sc->ExpectFloat(); TInfo->WalkingStepTime = TInfo->RunningStepTime = TInfo->CrouchingStepTime = sc->Float; continue; }
        if (sc->Check("leftstepsound")) { sc->ExpectString(); TInfo->LeftStepSound = *sc->String; continue; }
        if (sc->Check("rightstepsound")) { sc->ExpectString(); TInfo->RightStepSound = *sc->String; continue; }
        if (sc->Check("allstepsound")) { sc->ExpectString(); TInfo->LeftStepSound = TInfo->RightStepSound = *sc->String; continue; }
        // first step
        if (sc->Check("landvolume")) { sc->ExpectFloat(); TInfo->LandVolume = sc->Float; continue; }
        if (sc->Check("smalllandvolume")) { sc->ExpectFloat(); TInfo->SmallLandVolume = sc->Float; continue; }
        if (sc->Check("alllandvolume")) { sc->ExpectFloat(); TInfo->LandVolume = TInfo->SmallLandVolume = sc->Float; continue; }
        if (sc->Check("landsound")) { sc->ExpectString(); TInfo->LandSound = *sc->String; continue; }
        if (sc->Check("smalllandsound")) { sc->ExpectString(); TInfo->SmallLandSound = *sc->String; continue; }
        if (sc->Check("alllandsound")) { sc->ExpectString(); TInfo->LandSound = TInfo->SmallLandSound = *sc->String; continue; }
        sc->Error(va("Unknown k8vavoom terrain extension command (%s)", *sc->String));
      }
      continue;
    }
    sc->Error(va("Unknown terrain command (%s)", *sc->String));
  }
}


//==========================================================================
//
//  ProcessAssignBootprint
//
//==========================================================================
static void ProcessAssignBootprint (VTerrainBootprint *bp, VStr mask) {
  if (!bp) return;
  if (mask.isEmpty()) return;
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

  if (IsBootprintNoOverride(sc->String)) {
    GCon->Logf(NAME_Init, "%s: skipped bootprint '%s' override due to 'GlobalDisableOverride'", *sc->GetLoc().toStringNoCol(), *sc->String);
    sc->Check("modify");
    if (sc->Check("{")) sc->SkipBracketed(true/*bracketEaten*/);
    return;
  }

  VTerrainBootprint *bp = FindBootprint(*sc->String, true);
  vassert(bp); // invariant

  constexpr float BOOT_TIME_MIN = 3.8f;
  constexpr float BOOT_TIME_MAX = 4.2f;

  bool doclear = true;
  if (sc->Check("modify")) {
    doclear = (bp->Name == NAME_None);
  } else {
    // remove from bootprint map
    if (bp->Name != NAME_None) {
      TArray<int> rmidx;
      for (auto &&it : TerrainBootprintMap.first()) {
        if (it.value() == bp) rmidx.append(it.key());
      }
      for (int ri : rmidx) TerrainBootprintMap.del(ri);
    }
  }

  bp->Name = VName(*bp->OrigName, VName::AddLower);
  if (doclear) {
    bp->TimeMin = BOOT_TIME_MIN;
    bp->TimeMax = BOOT_TIME_MAX;
    bp->AlphaMin = bp->AlphaMax = bp->AlphaValue = 1.0f;
    bp->Translation = 0;
    bp->ShadeColor = -2;
    bp->Animator = NAME_None;
    bp->Flags = 0;
  }

  if (GlobalDisableOverride) AddBootprintNoOverride(bp->OrigName);

  sc->Check("{");
  //bool wasDecalName = false;
  while (!sc->Check("}")) {
    if (sc->AtEnd()) sc->Error(va("unfinished bootprint '%s' definition", *bp->OrigName));

    // decal name
    /*
    if (sc->Check("decal")) {
      if (wasDecalName) sc->Error("duplicate decal name");
      wasDecalName = true;
      sc->ExpectString();
      bp->DecalName = (sc->String.isEmpty() ? NAME_None : VName(*sc->String));
      continue;
    }
    */

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
      ProcessAssignBootprint(bp, mask);
      continue;
    }

    if (sc->Check("optional")) {
      bp->setOptional();
      continue;
    }

    if (sc->Check("required")) {
      bp->resetOptional();
      continue;
    }

    if (sc->Check("shade")) {
      if (sc->Check("flatpic")) {
        bp->ShadeColor = 0xed000000;
        if (sc->Check("maxout")) {
          sc->ExpectNumber();
          bp->ShadeColor |= clampToByte(sc->Number);
        } else {
          // mult by 2.0
          bp->ShadeColor |= 2;
        }
      } else if (sc->Check("fromdecal")) {
        bp->ShadeColor = -2;
      } else {
        sc->ExpectString();
        vuint32 ppc = M_ParseColor(*sc->String);
        bp->ShadeColor = ppc&0xffffff;
      }
      continue;
    }

    if (sc->Check("animator")) {
      sc->ExpectString();
      if (sc->String.isEmpty()) {
        bp->Animator = VName("none");
      } else {
        bp->Animator = VName(*sc->String);
      }
      continue;
    }

    if (sc->Check("translucent")) {
      if (sc->Check("random")) {
        // `random min max`
        sc->ExpectFloat();
        bp->AlphaMin = clampval(sc->Float, 0.0f, 1.0f);
        sc->ExpectFloat();
        bp->AlphaMax = clampval(sc->Float, 0.0f, 1.0f);
        bp->AlphaValue = RandomBetween(bp->AlphaMin, bp->AlphaMax);
      } else {
        sc->ExpectFloat();
        bp->AlphaMin = bp->AlphaMax = bp->AlphaValue = clampval(sc->Float, 0.0f, 1.0f);
      }
      continue;
    }

    if (sc->Check("solid")) {
      bp->AlphaMin = bp->AlphaMax = bp->AlphaValue = 1.0f;
      continue;
    }

    if (sc->Check("usesourcedecalalpha")) {
      bp->AlphaMin = bp->AlphaMax = bp->AlphaValue = -1.0f;
      continue;
    }

    #if 0
    // base decal
    if (sc->Check("basedecal")) {
      sc->ExpectString();
      VStr bdbname = sc->String;
      if (bdbname.isEmpty() || bdbname.strEquCI("none")) {
        sc->Expect("{");
        sc->SkipBracketed(true/*bracketEaten*/);
        continue;
      }
      if (!wasDecalName) sc->Error("decal name should be defined");
      if (bp->DecalName == NAME_None || VStr::strEquCI(*bp->DecalName, "none")) {
        sc->Expect("{");
        sc->SkipBracketed(true/*bracketEaten*/);
        continue;
      }
      if (bdbname.strEquCI(*bp->DecalName)) sc->Error("base decal name should not be equal to decal name");
      //GCon->Logf(NAME_Debug, "creating decal '%s' from base decal '%s'", *bp->DecalName, *bdbname);
      VDecalDef::CreateFromBaseDecal(sc, VName(*bdbname), bp->DecalName);
      continue;
    }
    #endif

    sc->Error(va("Unknown bootprint command (%s)", *sc->String));
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
    const VTextLocation loc = sc->GetLoc();

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
      const VStr slocstr = sc->GetLoc().toStringNoCol();
      sc->Check("optional"); // ignore it
      sc->ExpectName8Warn();
      VName floorname = sc->Name8;
      sc->ExpectString();
      VStr tername = sc->String;
      if (tername.isEmpty()) tername = "none"; //k8:???
      //GCon->Logf(NAME_Debug, "::: <%s> -- <%s>", *floorname, *tername);
      int Pic = GTextureManager.CheckNumForName(floorname, TEXTYPE_Flat, /*false*/true); // allow overloads
      if (Pic == 0) {
        GCon->Logf(NAME_Warning, "%s: cannot assign NULL floor texture '%s' for terrain '%s'", *slocstr, *floorname, *tername);
        continue;
      }
      if (Pic < 0) {
        GCon->Logf(NAME_Warning, "%s: ignored unknown floor texture '%s' for terrain '%s'", *slocstr, *floorname, *tername);
        continue;
        /*
        Pic = GTextureManager.CheckNumForName(floorname, TEXTYPE_Wall, false);
        if (Pic == 0) { GCon->Logf(NAME_Warning, "%s: cannot assign NULL wall texture '%s' for terrain '%s'", *slocstr, *floorname, *tername); continue; }
        if (Pic < 0) {
          Pic = GTextureManager.CheckNumForName(floorname, TEXTYPE_WallPatch, false);
          if (Pic == 0) { GCon->Logf(NAME_Warning, "%s: cannot assign NULL patch texture '%s' for terrain '%s'", *slocstr, *floorname, *tername); continue; }
          if (Pic < 0) { GCon->Logf(NAME_Warning, "%s: ignored unknown floor texture '%s' for terrain '%s'", *slocstr, *floorname, *tername); continue; }
          GCon->Logf(NAME_Init, "%s; assigning patch texture '%s' to terrain '%s'", *slocstr, *floorname, *tername);
        } else {
          GCon->Logf(NAME_Init, "%s; assigning wall texture '%s' to terrain '%s'", *slocstr, *floorname, *tername);
        }
        */
      } else {
        VTexture *tex = GTextureManager[Pic];
        if (!tex) {
          GCon->Logf(NAME_Warning, "%s: ignored unknown floor texture '%s' for terrain '%s'", *slocstr, *floorname, *tername);
          continue;
        }
        bool invalid = false;
        const char *stn = "wtf?";
        switch (tex->Type) {
          case TEXTYPE_WallPatch: stn = "patch"; break;
          case TEXTYPE_Wall: stn = "wall"; break;
          case TEXTYPE_Flat: stn = nullptr; break;
          case TEXTYPE_Overload: stn = nullptr/*"overload"*/; break;
          case TEXTYPE_Sprite: stn = "sprite"; invalid = true; break;
          case TEXTYPE_SkyMap: stn = "skymap"; invalid = true; break;
          case TEXTYPE_Skin: stn = "skin"; invalid = true; break;
          case TEXTYPE_Pic: stn = "pic"; invalid = true; break;
          case TEXTYPE_Autopage: stn = "autopage"; invalid = true; break;
          case TEXTYPE_Null: stn = "null"; invalid = true; break;
          case TEXTYPE_FontChar: stn = "font"; invalid = true; break;
          default: break;
        }
        if (invalid) {
          GCon->Logf(NAME_Warning, "%s: cannot assign %s floor texture '%s' for terrain '%s'", *slocstr, stn, *floorname, *tername);
          continue;
        }
        if (stn) {
          GCon->Logf(NAME_Init, "%s: assigning %s texture '%s' for terrain '%s'", *slocstr, stn, *floorname, *tername);
        }
      }
      auto pp = TerrainTypeMap.find(Pic);
      if (pp) {
        // replace old one
        if (GlobalDisableOverride && IsTerrainNoOverride(VStr(TerrainTypes[*pp].TypeName))) {
          GCon->Logf(NAME_Init, "%s: skipped floor '%s' override to '%s' due to 'GlobalDisableOverride'", *sc->GetLoc().toStringNoCol(), *floorname, *tername);
        } else {
          TerrainTypes[*pp].TypeName = VName(*tername, VName::AddLower);
        }
      } else {
        //!GCon->Logf(NAME_Init, "%s: new terrain floor '%s' of type '%s' (pic=%d)", *sc->GetLoc().toStringNoCol(), *floorname, *tername, Pic);
        VTerrainType &T = TerrainTypes.Alloc();
        T.Pic = Pic;
        T.TypeName = VName(*tername, VName::AddLower);
        TerrainTypeMap.put(Pic, TerrainTypes.length()-1);
      }
      continue;
    }

    // floor detection by globmask definition?
    if (sc->Check("floorglob")) {
      GCon->Logf(NAME_Warning, "%s: \"floorglob\" is obsolete, please, use \"assign_terrain\" instead!", *loc.toStringNoCol());
      sc->ExpectString();
      VStr mask = sc->String.xstrip();
      sc->ExpectString();
      VStr tername = sc->String;
      ProcessFlatGlobMask(mask, *tername, sc->GetLoc().toStringNoCol());
      continue;
    }

    // floor detection by globmask definition? (k8vavoom extension)
    if (sc->Check("assign_terrain")) {
      sc->ExpectString();
      VStr tername = sc->String;
      sc->Expect("{");
      while (!sc->Check("}")) {
        if (sc->AtEnd()) sc->Error(va("unexpected end of file in \"assign_terrain\" (started at %s)", *loc.toStringNoCol()));
        sc->Expect("flat");
        sc->ExpectString();
        VStr mask = sc->String.xstrip();
        ProcessFlatGlobMask(mask, *tername, sc->GetLoc().toStringNoCol());
      }
      continue;
    }

    // floor detection by globmask definition? (k8vavoom extension)
    if (sc->Check("assign_bootprint")) {
      sc->ExpectString();
      VStr tername = sc->String;
      VTerrainBootprint *bp = FindBootprint(*tername, false);
      vassert(bp); // invariant
      sc->Expect("{");
      while (!sc->Check("}")) {
        if (sc->AtEnd()) sc->Error(va("unexpected end of file in \"assign_terrain\" (started at %s)", *loc.toStringNoCol()));
        sc->Expect("flat");
        sc->ExpectString();
        VStr mask = sc->String.xstrip();
        ProcessAssignBootprint(bp, mask);
      }
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

    // k8vavoom options?
    if (sc->Check("k8vavoom")) {
      sc->Expect("{");
      while (!sc->Check("}")) {
        if (sc->AtEnd()) sc->Expect("}");
        if (sc->Check("GlobalDisableOverride")) { GlobalDisableOverride = true; continue; }
        if (sc->Check("GlobalEnableOverride")) { GlobalDisableOverride = false; continue; }
        sc->Error(va("Unknown k8vavoom global command (%s)", *sc->String));
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
  DefT.FootClip = 0.0f;
  DefT.DamageTimeMask = 0;
  DefT.DamageAmount = 0;
  DefT.DamageType = NAME_None;
  DefT.Friction = 0.0f;
  DefT.MoveFactor = 0.0f;
  DefT.WalkingStepVolume = 1.0f;
  DefT.RunningStepVolume = 1.0f;
  DefT.CrouchingStepVolume = 1.0f;
  DefT.WalkingStepTime = 0.0f;
  DefT.RunningStepTime = 0.0f;
  DefT.CrouchingStepTime = 0.0f;
  DefT.LeftStepSound = NAME_None;
  DefT.RightStepSound = NAME_None;
  DefT.LandVolume = 1.0f;
  DefT.LandSound = NAME_None;
  DefT.SmallLandVolume = 1.0f;
  DefT.SmallLandSound = NAME_None;
  DefT.BootPrint = nullptr;

  DefaultTerrainName = DefT.Name;
  DefaultTerrainNameStr = DefT.OrigName;
  DefaultTerrainIndex = 0;

  for (auto &&it : WadNSNameIterator(NAME_terrain, WADNS_Global)) {
    const int Lump = it.lump;
    GlobalDisableOverride = false;
    ParseTerrainScript(VScriptParser::NewWithLump(Lump));
  }
  for (auto &&it : WadNSNameIterator(NAME_vterrain, WADNS_Global)) {
    const int Lump = it.lump;
    GlobalDisableOverride = false;
    ParseTerrainScript(VScriptParser::NewWithLump(Lump));
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

  // build name map
  for (VTerrainBootprint *bp : TerrainBootprints) {
    if (bp->Name != NAME_None) {
      TerrainBootprintNameMap.put(VStrCI(*bp->Name), bp);
    }
  }

  NoOverrideSplash.clear();
  NoOverrideTerrain.clear();
  NoOverrideBootprint.clear();
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
  if (ter && (ter->Flags&VTerrainInfo::F_OptOut) && !gm_optional_terrain.asBool()) ter = nullptr;
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
//  SV_FindBootprintByName
//
//==========================================================================
VTerrainBootprint *SV_FindBootprintByName (const char *name) {
  if (!name || !name[0] || VStr::strEquCI(name, "none")) return nullptr;
  auto bpp = TerrainBootprintNameMap.find(name);
  return (bpp ? *bpp : nullptr);
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
  TerrainBootprintNameMap.clear();
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
