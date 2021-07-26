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
//**  Preparation of data for rendering, generation of lookups, caching,
//**  retrieval by name.
//**
//**  Graphics.
//**
//**  DOOM graphics for walls and sprites is stored in vertical runs of
//**  opaque pixels (posts). A column is composed of zero or more posts, a
//**  patch or sprite is composed of zero or more columns.
//**
//**  Texture definition.
//**
//**  Each texture is composed of one or more patches, with patches being
//**  lumps stored in the WAD. The lumps are referenced by number, and
//**  patched into the rectangular texture space using origin and possibly
//**  other attributes.
//**
//**  Texture definition.
//**
//**  A DOOM wall texture is a list of patches which are to be combined in
//**  a predefined order.
//**
//**  A single patch from a texture definition, basically a rectangular
//**  area within the texture rectangle.
//**
//**  A maptexturedef_t describes a rectangular texture, which is composed
//**  of one or more mappatch_t structures that arrange graphic patches.
//**
//**  MAPTEXTURE_T CACHING
//**
//**  When a texture is first needed, it counts the number of composite
//**  columns required in the texture and allocates space for a column
//**  directory and any new columns. The directory will simply point inside
//**  other patches if there is only one patch in a given column, but any
//**  columns with multiple patches will have new column_ts generated.
//**
//**************************************************************************
#include "../gamedefs.h"
#include "../utils/ntvalueioex.h"  /* VCheckedStream */
#include "../filesys/files.h"
#include "r_tex.h"


// ////////////////////////////////////////////////////////////////////////// //
//  Texture manager
// ////////////////////////////////////////////////////////////////////////// //
bool GTextureCropMessages = true;
VTextureManager GTextureManager;
VName VTextureManager::DummyTextureName = NAME_None;


static int cli_WipeWallPatches = 0;
static int cli_AddTextureDump = 0;
static int cli_WarnDuplicateTextures = 0;
static int cli_DumpTextures = 0;
static int cli_SkipSprOfs = 0;

/*static*/ bool cliRegister_txloader_args =
  VParsedArgs::RegisterFlagSet("-wipe-wall-patches", "!do not use", &cli_WipeWallPatches) &&
  VParsedArgs::RegisterFlagSet("-dev-add-texture-dump", "!do not use", &cli_AddTextureDump) &&
  VParsedArgs::RegisterFlagSet("-Wduplicate-textures", "!warn about dumplicate textures", &cli_WarnDuplicateTextures) &&
  VParsedArgs::RegisterFlagSet("-dbg-dump-textures", "!do not use", &cli_DumpTextures) &&
  VParsedArgs::RegisterFlagSet("-ignore-sprofs", "ignore 'sprofs' sprite offset lumps", &cli_SkipSprOfs);

static int hitexNewCount = 0;
static int hitexReplaceCount = 0;
static double hitexLastMessageTime = -1;

static TMapNC<VName, bool> numForNameWarned;
static TMapNC<VName, bool> numForNameWarnedMapTextures;

static TMap<VStrCI, int> txFullNameHash; // value: texture index

// sorry for this hack!
extern void SV_UpdateSkyFlat ();


// ////////////////////////////////////////////////////////////////////////// //
// Flats data
// ////////////////////////////////////////////////////////////////////////// //
int skyflatnum = -2; // sky mapping
int screenBackTexNum; // background filler for unused screen parts and status bar

VCvarB r_hirestex("r_hirestex", true, "Allow high-resolution texture replacements?", /*CVAR_Archive|*/CVAR_PreInit);
VCvarB r_showinfo("r_showinfo", false, "Show some info about loaded textures?", CVAR_Archive);

static VCvarB r_reupload_textures("r_reupload_textures", false, "Reupload textures to GPU when new map is loaded?", CVAR_Archive);

static VCvarB r_debug_fullpath_textures("r_debug_fullpath_textures", false, "Show some debug messages for fullpath textures?", CVAR_PreInit);


// ////////////////////////////////////////////////////////////////////////// //
static TMapNC<VName, bool> patchesWarned;

// used only once in `VTextureManager::Init()`
// we have to force-load textures after adding textures lump, so
// texture numbering for animations won't break
static TMapNC<VName, bool> numberedNamesMap;
static TArray<VName> numberedNamesList;

static TArray<int> textureDefLumps;
static bool textureDefLumpsCollected = false;


//==========================================================================
//
//  CollectTextureDefLumps
//
//==========================================================================
static void CollectTextureDefLumps () {
  if (textureDefLumpsCollected) return;
  textureDefLumpsCollected = true;
  for (auto &&it : WadNSIterator(WADNS_Global)) {
    if (it.getName() == NAME_hirestex || it.getName() == NAME_textures) textureDefLumps.append(it.lump);
  }
}


//==========================================================================
//
//  ClearTextureDefLumps
//
//==========================================================================
static void ClearTextureDefLumps () {
  textureDefLumpsCollected = false;
  textureDefLumps.clear();
}


//==========================================================================
//
//  ClearNumberedNames
//
//==========================================================================
static void ClearNumberedNames () {
  numberedNamesMap.clear();
  numberedNamesList.clear();
}


//==========================================================================
//
//  CheckAddNumberedName
//
//==========================================================================
static inline void CheckAddNumberedName (VName PatchName) {
  const char *txname = *PatchName;
  int namelen = VStr::length(txname);
  // this is how it is done in `AddMissingNumberedTextures()`
  if (namelen && namelen <= 8 && txname[namelen-1] == '1') {
    if (!numberedNamesMap.put(PatchName, true)) numberedNamesList.append(PatchName);
  }
}


//==========================================================================
//
//  warnMissingTexture
//
//==========================================================================
static void warnMissingTexture (VName Name, bool silent) {
  if (Name == NAME_None) return; // just in case
  VName xxn = Name.GetLower();
  if (!patchesWarned.put(xxn, true)) {
    // new value
    if (!silent) GCon->Logf(NAME_Warning,"Texture: texture \"%s\" not found", *Name);
  }
}


//==========================================================================
//
//  isSeenMissingTexture
//
//==========================================================================
static bool isSeenMissingTexture (VName Name) {
  if (Name == NAME_None) return true; // just in case
  VName xxn = Name.GetLowerNoCreate();
  if (xxn == NAME_None) return false;
  return patchesWarned.has(xxn);
}


//==========================================================================
//
//  VTextureManager::VTextureManager
//
//==========================================================================
VTextureManager::VTextureManager ()
  : inMapTextures(0)
  , DefaultTexture(-1)
  , Time(0)
{
  for (unsigned i = 0; i < HASH_SIZE; ++i) TextureHash[i] = -1;
  SkyFlatName = NAME_None;
}


//==========================================================================
//
//  VTextureManager::Init
//
//==========================================================================
void VTextureManager::Init () {
  vassert(inMapTextures == 0);
  r_hirestex.SetReadOnly(true); // it cannot be changed in-game

  // add a dummy texture
  AddTexture(new VDummyTexture);

  // initialise wall textures
  GCon->Log(NAME_Init, "initializing wall textures");
  AddTextures();

  // initialise flats
  GCon->Log(NAME_Init, "initializing flat textures");
  AddGroup(TEXTYPE_Flat, WADNS_Flats);

  // initialise overloaded textures
  GCon->Log(NAME_Init, "initializing overloaded textures");
  AddGroup(TEXTYPE_Overload, WADNS_NewTextures);

  // initialise sprites
  GCon->Log(NAME_Init, "initializing sprite textures");
  AddGroup(TEXTYPE_Sprite, WADNS_Sprites);

  // initialise hires textures
  GCon->Log(NAME_Init, "initializing advanced textures");
  AddTextureTextLumps(); // only normal for now

  // force-load numbered textures
  if (numberedNamesList.length()) {
    GCon->Logf(NAME_Init, "making finishing touches (%d)", numberedNamesList.length());
    AddMissingNumberedTextures();
  }

  //GCon->Log(NAME_Init, "initializing hires textures");
  R_InitHiResTextures();

  // we don't need numbered names anymore
  ClearNumberedNames();
  ClearTextureDefLumps();

  // find default texture
  DefaultTexture = CheckNumForName("-noflat-", TEXTYPE_Overload, false);
  if (DefaultTexture == -1) Sys_Error("Default texture -noflat- not found");

  // find sky flat number
  R_UpdateSkyFlatNum(true); // forced

  screenBackTexNum = GTextureManager.AddFileTextureChecked("graphics/screenback/screenback_base.png", TEXTYPE_Pic);

  LoadSpriteOffsets();
}


//==========================================================================
//
//  VTextureManager::Shutdown
//
//==========================================================================
void VTextureManager::Shutdown () {
  for (int i = 0; i < Textures.length(); ++i) { delete Textures[i]; Textures[i] = nullptr; }
  for (int i = 0; i < MapTextures.length(); ++i) { delete MapTextures[i]; MapTextures[i] = nullptr; }
  Textures.clear();
  MapTextures.clear();
}


//==========================================================================
//
//  VTextureManager::Shutdown
//
//==========================================================================
void VTextureManager::DumpHashStats (EName logName) {
  int maxBucketLen = 0;
  int usedBuckets = 0;
  for (unsigned bidx = 0; bidx < HASH_SIZE; ++bidx) {
    if (TextureHash[bidx] < 0) continue;
    ++usedBuckets;
    int blen = 0;
    for (int i = TextureHash[bidx]; i >= 0; i = getTxByIndex(i)->HashNext) ++blen;
    if (maxBucketLen < blen) maxBucketLen = blen;
  }
  GCon->Logf(logName, "TextureManager: maximum %d textures in bucket, used %d out of %d buckets", maxBucketLen, usedBuckets, HASH_SIZE-1);
}


//==========================================================================
//
//  VTextureManager::rehashTextures
//
//==========================================================================
void VTextureManager::rehashTextures () {
  for (unsigned i = 0; i < HASH_SIZE; ++i) TextureHash[i] = -1;
  txFullNameHash.reset();
  if (Textures.length()) {
    vassert(Textures[0]->Name == NAME_None);
    vassert(Textures[0]->Type == TEXTYPE_Null);
    for (int f = 1; f < Textures.length(); ++f) if (Textures[f]) AddToHash(f);
  }
  for (int f = 0; f < MapTextures.length(); ++f) if (MapTextures[f]) AddToHash(FirstMapTextureIndex+f);
}


//==========================================================================
//
//  VTextureManager::WipeWallPatches
//
//==========================================================================
void VTextureManager::WipeWallPatches () {
  if (cli_WipeWallPatches > 0) {
    int count = 0;
    for (int f = 0; f < Textures.length(); ++f) {
      VTexture *tx = Textures[f];
      if (!tx || tx->Type != TEXTYPE_WallPatch || tx->Name != NAME_None) continue;
      tx->Name = NAME_None;
      tx->Type = TEXTYPE_Null;
      ++count;
    }
    if (count) {
      rehashTextures();
      GCon->Logf("WipeWallPatches: %d textures wiped", count);
      R_UpdateSkyFlatNum(); // just in case
    }
  }
}


//==========================================================================
//
//  VTextureManager::ResetMapTextures
//
//==========================================================================
void VTextureManager::ResetMapTextures () {
  numForNameWarnedMapTextures.reset();

  if (MapTextures.length() == 0) {
#ifdef CLIENT
    if (r_reupload_textures && Drawer) {
      GCon->Logf("Unloading textures from GPU");
      Drawer->FlushTextures(true); // forced
      //rehashTextures();
    }
#endif
    return;
  }

  vassert(MapTextures.length() != 0);

  GCon->Logf(NAME_Dev, "*** *** MapTextures.length()=%d *** ***", MapTextures.length());
#ifdef CLIENT
  if (Drawer && r_reupload_textures) Drawer->FlushTextures(true); // forced
#endif
  for (int f = MapTextures.length()-1; f >= 0; --f) {
    if (developer) {
      if (MapTextures[f]) GCon->Logf(NAME_Dev, "removing map texture #%d (%s)", f, *MapTextures[f]->Name);
    }
#ifdef CLIENT
    if (Drawer && !r_reupload_textures && MapTextures[f]) Drawer->FlushOneTexture(MapTextures[f], true); // forced
#endif
    delete MapTextures[f];
    MapTextures[f] = nullptr;
  }
  GCon->Logf("TextureManager: %d map textures removed", MapTextures.length());
  MapTextures.setLength(0, false); // don't resize
  rehashTextures();
  R_UpdateSkyFlatNum(); // just in case
}


//==========================================================================
//
//  VTextureManager::AddTexture
//
//==========================================================================
int VTextureManager::AddTexture (VTexture *Tex) {
  if (!Tex) return -1;

  if (Tex->Name == NAME_None) {
    if (Textures.length()) {
      R_DumpTextures();
      Sys_Error("internal error: tried to add dummy texture in inappropriate moment");
    }
  } else {
    //FIXME: real dummy name should be checked only for wall textures, i guess
    // it is safe to use `IsDummyTextureName()` here, as `NAME_None` already checked
    if (IsDummyTextureName(Tex->Name)) {
      // ignore this texture
      // this is memory leak, but i don't care -- broken mods cannot work right anyway
      GCon->Logf(NAME_Error, "tried to add dummy texture with name '%s' (%s) as a normal one (from lump '%s')", *VStr(Tex->Name).toUpperCase(), VTexture::TexTypeToStr(Tex->Type), *W_FullLumpName(Tex->SourceLump));
      return 0; // "no texture"
    }
  }

  int devTexDump = (cli_AddTextureDump > 0);

  // also, replace existing texture with similar name, if we aren't in "map-local" mode
  if (!inMapTextures) {
    VName loTName = Tex->Name.GetLowerNoCreate();
    if (loTName != NAME_None && Tex->Name != NAME_None && (*Tex->Name)[0] != 0x7f) {
      int repidx = -1;
      // loop, no shrinking allowed
      for (auto it = firstWithName(loTName, false); !it.empty(); it.next()) {
        if (it.isMapTexture()) continue; // skip map textures
        VTexture *tx = it.tex();
        if (tx->Name != loTName) continue;
        if (tx->Type != Tex->Type) continue;
        repidx = it.index();
        break;
      }
      if (repidx > 0) {
        vassert(repidx > 0 && repidx < FirstMapTextureIndex);
        int warnReplace = (cli_WarnDuplicateTextures > 0 ? 1 : 0);
        if (warnReplace > 0 || developer) GCon->Logf(NAME_Warning, "replacing duplicate texture '%s' with new one (id=%d)", *Tex->Name, repidx);
        ReplaceTexture(repidx, Tex);
        return repidx;
      }
    }
    if (developer && devTexDump) GCon->Logf(NAME_Dev, "***NEW TEXTURE #%d: <%s> (%s)", Textures.length(), *Tex->Name, VTexture::TexTypeToStr(Tex->Type));
    Textures.Append(Tex);
    Tex->TextureTranslation = Textures.length()-1;
    AddToHash(Textures.length()-1);
    return Textures.length()-1;
  } else {
    if (developer && devTexDump) GCon->Logf(NAME_Dev, "***MAP-TEXTURE #%d: <%s> (%s)", MapTextures.length(), *Tex->Name, VTexture::TexTypeToStr(Tex->Type));
    MapTextures.Append(Tex);
    Tex->TextureTranslation = FirstMapTextureIndex+MapTextures.length()-1;
    AddToHash(FirstMapTextureIndex+MapTextures.length()-1);
    return FirstMapTextureIndex+MapTextures.length()-1;
  }
}


//==========================================================================
//
//  VTextureManager::AddFullNameTexture
//
//==========================================================================
int VTextureManager::AddFullNameTexture (VTexture *Tex, bool asMapTexture) {
  if (!Tex) return -1;
  if (!inMapTextures && !asMapTexture) {
    Textures.Append(Tex);
    Tex->TextureTranslation = Textures.length()-1;
    AddToHash(Textures.length()-1);
    return Textures.length()-1;
  } else {
    MapTextures.Append(Tex);
    Tex->TextureTranslation = FirstMapTextureIndex+MapTextures.length()-1;
    AddToHash(FirstMapTextureIndex+MapTextures.length()-1);
    return FirstMapTextureIndex+MapTextures.length()-1;
  }
}


//==========================================================================
//
//  VTextureManager::ReplaceTexture
//
//==========================================================================
void VTextureManager::ReplaceTexture (int Index, VTexture *NewTex) {
  vassert(Index >= 0);
  vassert((Index < FirstMapTextureIndex && Index < Textures.length()) || (Index >= FirstMapTextureIndex && Index-FirstMapTextureIndex < MapTextures.length()));
  vassert(NewTex);
  //VTexture *OldTex = Textures[Index];
  VTexture *OldTex = getTxByIndex(Index);
  if (OldTex == NewTex) return;
  NewTex->Name = OldTex->Name;
  NewTex->Type = OldTex->Type;
  NewTex->TextureTranslation = OldTex->TextureTranslation;
  NewTex->HashNext = OldTex->HashNext;

  //Textures[Index] = NewTex;
  if (Index < FirstMapTextureIndex) {
    Textures[Index] = NewTex;
  } else {
    MapTextures[Index-FirstMapTextureIndex] = NewTex;
    if (skyflatnum > -2 && IsSkyTextureName(NewTex->Name)) R_UpdateSkyFlatNum();
  }
  //FIXME: delete OldTex?

  // if it comes from a wad, don't add it to fullname hash
  if (NewTex->SourceLump >= 0 && W_IsPakFile(W_LumpFile(NewTex->SourceLump))) {
    txFullNameHash.put(W_RealLumpName(NewTex->SourceLump), Index);
  }
}


//==========================================================================
//
//  VTextureManager::AddToHash
//
//==========================================================================
void VTextureManager::AddToHash (int Index) {
  VTexture *tx = getTxByIndex(Index);
  if (!tx) return; // why not?
  tx->HashNext = -1;

  const bool addShortName = (tx->Name != NAME_None && (*tx->Name)[0] != 0x7f);
  if (addShortName) {
    VName tname = tx->Name.GetLower();
    tx->Name = tname; // force lower-cased name for texture
    const unsigned HashIndex = TextureBucket(GetTypeHash(tname));
    if (Index < FirstMapTextureIndex) {
      Textures[Index]->HashNext = TextureHash[HashIndex];
    } else {
      MapTextures[Index-FirstMapTextureIndex]->HashNext = TextureHash[HashIndex];
    }
    TextureHash[HashIndex] = Index;
  }

  // if it comes from a wad, don't add it to fullname hash
  if (tx->SourceLump >= 0 && (!addShortName || W_IsPakFile(W_LumpFile(tx->SourceLump)))) {
    txFullNameHash.put(W_RealLumpName(tx->SourceLump), Index);
  }

  if (addShortName && skyflatnum > -2 && IsSkyTextureName(tx->Name)) R_UpdateSkyFlatNum();
}


//==========================================================================
//
//  VTextureManager::firstWithStr
//
//==========================================================================
VTextureManager::Iter VTextureManager::firstWithStr (VStr s) {
  if (s.isEmpty()) return Iter();
  VName n = VName(*s, VName::FindLower);
  if (n == NAME_None && s.length() > 8) n = VName(*s, VName::FindLower8);
  if (n == NAME_None) return Iter();
  return firstWithName(n);
}


//==========================================================================
//
//  VTextureManager::CheckNumForName
//
//  Check whether texture is available. Filter out NoTexture indicator.
//
//==========================================================================
int VTextureManager::CheckNumForName (VName Name, int Type, bool bOverload) {
  if ((unsigned)Type >= (unsigned)TEXTYPE_MAX) return -1; // oops

  Name = Name.GetLowerNoCreate();
  //if (Name == NAME_None) return -1;
  if (IsDummyTextureName(Name)) return 0;

  if (strchr(*Name, '/')) {
    int res = FindFullyNamedTexture(VStr(Name), nullptr, Type, bOverload, /*silent*/true);
    if (res >= 0) return res;
  }

  //k8: sorry, this code is a mess, i know it

  // if we didn't found a type we want, and there's no single texture with the given name,
  // search it again, with another set of checks
  bool secondary = false;

doitagain:
  int seenOther = -1; // other type lump
  int seenType = -1; // type for `seenOther`
  int seenOne = -1; // one of unknown type (reset if there are several textures with the same name)
  int seenOneType = -1; // type for `seenOne` (this can be set if we've seen several textures with the same name, set to first seen)

  // textures are added to hashtable in reverse order, so "first" is actually last added
  for (auto it = firstWithName(Name); !it.empty(); it.next()) {
    VTexture *ctex = it.tex();
    if (Type == TEXTYPE_Any || ctex->Type == Type || (bOverload && ctex->Type == TEXTYPE_Overload)) {
      if (secondary) {
        // secondary check
        switch (ctex->Type) {
          case TEXTYPE_WallPatch:
          case TEXTYPE_Overload:
          case TEXTYPE_Skin:
          case TEXTYPE_Autopage:
          case TEXTYPE_Null:
          case TEXTYPE_FontChar:
            continue;
        }
      }
      if (ctex->Type == TEXTYPE_Null) {
        return 0;
      }
      return it.index();
    } else if (Type == TEXTYPE_WallPatch && ctex->Type != TEXTYPE_Null) {
      bool repl = false;
      switch (ctex->Type) {
        case TEXTYPE_Wall: repl = (seenType < 0 || seenType == TEXTYPE_Sprite || seenType == TEXTYPE_Flat); break;
        case TEXTYPE_Flat: repl = (seenType < 0 || seenType == TEXTYPE_Sprite); break;
        case TEXTYPE_Sprite: repl = (seenType < 0); break;
        case TEXTYPE_Pic: repl = (seenType < 0 || seenType == TEXTYPE_Sprite || seenType == TEXTYPE_Flat || seenType == TEXTYPE_Wall); break;
      }
      if (repl) {
        seenOther = it.index();
        seenType = ctex->Type;
      }
    } else {
      switch (ctex->Type) {
        case TEXTYPE_WallPatch:
        case TEXTYPE_Overload:
        case TEXTYPE_Skin:
        case TEXTYPE_Autopage:
        case TEXTYPE_Null:
        case TEXTYPE_FontChar:
          break;
        default:
          if (seenOneType < 0) {
            seenOneType = ctex->Type;
            seenOne = (seenOneType != TEXTYPE_Null ? it.index() : 0);
          } else {
            seenOne = -1;
          }
          break;
      }
    }
  }

  if (seenOther >= 0) {
    // no exact type match, but found good other texture
    return seenOther;
  }

  if (!secondary && Type != TEXTYPE_Any) {
    if (seenOne >= 0) {
      // seen exactly one texture with a different type, return it, and hope for the best
      return seenOne;
    }
    switch (Type) {
      case TEXTYPE_Wall:
      case TEXTYPE_Flat:
      case TEXTYPE_Pic:
        // try secondary (relaxed) search for the given types
        secondary = true;
        Type = TEXTYPE_Any;
        goto doitagain;
    }
  }

  return -1;
}


//==========================================================================
//
//  VTextureManager::FindPatchByName
//
//  used in multipatch texture builder
//
//==========================================================================
int VTextureManager::FindPatchByName (VName Name) {
  return CheckNumForName(Name, TEXTYPE_WallPatch, true);
}


//==========================================================================
//
//  VTextureManager::FindWallByName
//
//  used to find wall texture (but can return non-wall)
//
//==========================================================================
int VTextureManager::FindWallByName (VName Name, bool bOverload) {
  Name = Name.GetLowerNoCreate();
  //if (Name == NAME_None) return -1;
  if (IsDummyTextureName(Name)) return 0;

  int seenOther = -1;
  int seenType = -1;

  for (int trynum = 0; trynum < 2; ++trynum) {
    if (trynum == 1) {
      if (VStr::length(*Name) < 8) return -1;
      Name = Name.GetLower8NoCreate();
      if (Name == NAME_None) return -1;
    }

    for (auto it = firstWithName(Name); !it.empty(); (void)it.next()) {
      VTexture *ctex = it.tex();
      if (ctex->Type == TEXTYPE_Wall || (bOverload && ctex->Type == TEXTYPE_Overload)) {
        if (ctex->Type == TEXTYPE_Null) return 0;
        return it.index();
      } else if (ctex->Type != TEXTYPE_Null) {
        bool repl = false;
        switch (ctex->Type) {
          case TEXTYPE_Flat: repl = (seenType < 0 || seenType == TEXTYPE_Sprite); break;
          case TEXTYPE_Sprite: repl = (seenType < 0); break;
          case TEXTYPE_Pic: repl = (seenType < 0 || seenType == TEXTYPE_Sprite || seenType == TEXTYPE_Flat); break;
        }
        if (repl) {
          seenOther = it.index();
          seenType = ctex->Type;
        }
      }
    }

    if (seenOther >= 0) return seenOther;
  }

  return -1;
}


//==========================================================================
//
//  VTextureManager::FindFlatByName
//
//  used to find flat texture (but can return non-flat)
//
//==========================================================================
int VTextureManager::FindFlatByName (VName Name, bool bOverload) {
  Name = Name.GetLowerNoCreate();
  //if (Name == NAME_None) return -1;
  if (IsDummyTextureName(Name)) return 0;

  int seenOther = -1;
  int seenType = -1;

  for (int trynum = 0; trynum < 2; ++trynum) {
    if (trynum == 1) {
      if (VStr::length(*Name) < 8) return -1;
      Name = Name.GetLower8NoCreate();
      if (Name == NAME_None) return -1;
    }

    for (auto it = firstWithName(Name); !it.empty(); (void)it.next()) {
      VTexture *ctex = it.tex();
      if (ctex->Type == TEXTYPE_Flat || (bOverload && ctex->Type == TEXTYPE_Overload)) {
        if (ctex->Type == TEXTYPE_Null) return 0;
        return it.index();
      } else if (ctex->Type != TEXTYPE_Null) {
        bool repl = false;
        switch (ctex->Type) {
          case TEXTYPE_Wall: repl = (seenType < 0 || seenType == TEXTYPE_Sprite); break;
          case TEXTYPE_Sprite: repl = (seenType < 0); break;
          case TEXTYPE_Pic: repl = (seenType < 0 || seenType == TEXTYPE_Sprite || seenType == TEXTYPE_Wall); break;
        }
        if (repl) {
          seenOther = it.index();
          seenType = ctex->Type;
        }
      }
    }

    if (seenOther >= 0) return seenOther;
  }

  return -1;
}


//==========================================================================
//
//  VTextureManager::NumForName
//
//  Calls R_CheckTextureNumForName, aborts with error message.
//
//==========================================================================
int VTextureManager::NumForName (VName Name, int Type, bool bOverload, bool bAllowLoadAsMapTexture) {
  //if (Name == NAME_None) return 0;
  if (IsDummyTextureName(Name)) return 0; // was -1
  int i = CheckNumForName(Name, Type, bOverload);
  if (i == -1) {
    if (bAllowLoadAsMapTexture) {
      if (!numForNameWarnedMapTextures.has(Name)) {
        auto lock = LockMapLocalTextures();
        i = AddFileTextureChecked(Name, Type);
        if (i == -1) i = AddFileTextureChecked(Name.GetLower(), Type);
        if (i != -1) return i;
        numForNameWarnedMapTextures.put(Name, true);
      }
    }
    if (!numForNameWarned.put(Name, true)) {
      GCon->Logf(NAME_Warning, "VTextureManager::NumForName: '%s' not found (type:%d; over:%d)", *Name, (int)Type, (int)bOverload);
    }
    i = DefaultTexture;
  }
  return i;
}


//==========================================================================
//
//  VTextureManager::FindTextureByLumpNum
//
//==========================================================================
int VTextureManager::FindTextureByLumpNum (int LumpNum) {
  for (int i = 0; i < MapTextures.length(); ++i) {
    if (MapTextures[i]->SourceLump == LumpNum) return i+FirstMapTextureIndex;
  }
  for (int i = 0; i < Textures.length(); ++i) {
    if (Textures[i]->SourceLump == LumpNum) return i;
  }
  return -1;
}


//==========================================================================
//
//  VTextureManager::GetTextureName
//
//==========================================================================
VName VTextureManager::GetTextureName (int TexNum) {
  VTexture *tx = getTxByIndex(TexNum);
  return (tx ? tx->Name : NAME_None);
}


//==========================================================================
//
//  VTextureManager::TextureWidth
//
//==========================================================================
float VTextureManager::TextureWidth (int TexNum) {
  VTexture *tx = getTxByIndex(TexNum);
  return (tx ? tx->GetWidth()/tx->SScale : 0);
}


//==========================================================================
//
//  VTextureManager::TextureHeight
//
//==========================================================================
float VTextureManager::TextureHeight (int TexNum) {
  VTexture *tx = getTxByIndex(TexNum);
  return (tx ? tx->GetHeight()/tx->TScale : 0);
}


//==========================================================================
//
//  VTextureManager::SetFrontSkyLayer
//
//==========================================================================
void VTextureManager::SetFrontSkyLayer (int tex) {
  VTexture *tx = getTxByIndex(tex);
  if (tx) tx->SetFrontSkyLayer();
}


//==========================================================================
//
//  VTextureManager::GetTextureInfo
//
//==========================================================================
void VTextureManager::GetTextureInfo (int TexNum, picinfo_t *info) {
  VTexture *Tex = getTxByIndex(TexNum);
  if (Tex) {
    info->width = Tex->GetScaledWidthI();
    info->height = Tex->GetScaledHeightI();
    info->xoffset = Tex->GetScaledSOffsetI();
    info->yoffset = Tex->GetScaledTOffsetI();
    info->xscale = Tex->SScale;
    info->yscale = Tex->TScale;
    info->widthNonScaled = Tex->GetWidth();
    info->heightNonScaled = Tex->GetHeight();
    info->xoffsetNonScaled = Tex->SOffset;
    info->yoffsetNonScaled = Tex->TOffset;
  } else {
    memset((void *)info, 0, sizeof(*info));
    info->xscale = info->yscale = 1.0f;
  }
}


//==========================================================================
//
//  findAndLoadTexture
//
//  FIXME: make this faster!
//
//==========================================================================
static int findAndLoadTexture (VTextureManager &txman, VName Name, int Type, EWadNamespace NS) {
  //if (Name == NAME_None) return -1;
  if (VTextureManager::IsDummyTextureName(Name)) return 0;
  VName PatchName = Name.GetLower8();

  // need to collect 'em to go in backwards order
  TArray<int> fulllist; // full names
  TArray<int> shortlist; // short names
  VName fullLoName = Name.GetLower(); // this creates lo-cased name, if necessary
  for (int LNum = W_IterateNS(-1, NS); LNum >= 0; LNum = W_IterateNS(LNum, NS)) {
    //GCon->Logf(NAME_Debug, "FOR '%s': #%d is '%s' ns=%d; (%s)", *Name, LNum, *W_LumpName(LNum), (int)NS, *W_FullLumpName(LNum));
    VName lmpname = W_LumpName(LNum);
    if (lmpname == NAME_None) continue; // just in case
    if (lmpname.IsLower()) {
           if (lmpname == fullLoName) fulllist.append(LNum);
      else if (lmpname == PatchName) shortlist.append(LNum);
    } else {
           if (VStr::ICmp(*lmpname, *Name) == 0) fulllist.append(LNum);
      else if (VStr::ICmp(*lmpname, *PatchName) == 0) shortlist.append(LNum);
    }
  }

  // now go with first list (full name)
  for (int f = fulllist.length()-1; f >= 0; --f) {
    int LNum = fulllist[f];
    VTexture *tex = VTexture::CreateTexture(Type, LNum);
    if (!tex) continue;
    int res = txman.AddTexture(tex);
    // if lump name is not identical to short, add with short name too
    if (VStr::ICmp(*W_LumpName(LNum), *PatchName) != 0) {
      tex = VTexture::CreateTexture(Type, LNum);
      tex->Name = PatchName;
      txman.AddTexture(tex);
    }
    return res;
  }

  // and with second list (short name)
  for (int f = shortlist.length()-1; f >= 0; --f) {
    int LNum = shortlist[f];
    VTexture *tex = VTexture::CreateTexture(Type, LNum);
    if (!tex) continue;
    int res = txman.AddTexture(tex);
    // if lump name is not identical to long, add with long name too
    if (VStr::ICmp(*W_LumpName(LNum), *Name) != 0) {
      tex = VTexture::CreateTexture(Type, LNum);
      tex->Name = Name.GetLower();
      txman.AddTexture(tex);
    }
    return res;
  }

  return -1;
}


//==========================================================================
//
//  findAndLoadTextureShaded
//
//  FIXME: make this faster!
//
//==========================================================================
/*
static int findAndLoadTextureShaded (VTextureManager &txman, VName Name, VName shName, int Type, EWadNamespace NS, int shade) {
  //if (Name == NAME_None) return -1;
  if (VTextureManager::IsDummyTextureName(Name)) return 0;
  VName PatchName = Name.GetLower8();

  // need to collect 'em to go in backwards order
  TArray<int> fulllist; // full names
  TArray<int> shortlist; // short names
  VName fullLoName = Name.GetLower(); // this creates lo-cased name, if necessary
  for (int LNum = W_IterateNS(-1, NS); LNum >= 0; LNum = W_IterateNS(LNum, NS)) {
    //GCon->Logf("FOR '%s': #%d is '%s'", *Name, LNum, *W_LumpName(LNum));
    VName lmpname = W_LumpName(LNum);
    if (lmpname == NAME_None) continue; // just in case
    if (lmpname.IsLower()) {
           if (lmpname == fullLoName) fulllist.append(LNum);
      else if (lmpname == PatchName) shortlist.append(LNum);
    } else {
           if (VStr::ICmp(*lmpname, *Name) == 0) fulllist.append(LNum);
      else if (VStr::ICmp(*lmpname, *PatchName) == 0) shortlist.append(LNum);
    }
  }

  // now go with first list (full name)
  for (int f = fulllist.length()-1; f >= 0; --f) {
    int LNum = fulllist[f];
    VTexture *tex = VTexture::CreateTexture(Type, LNum);
    if (!tex) continue;
    tex->Name = shName;
    tex->Shade(shade);
    return txman.AddTexture(tex);
  }

  // and with second list (short name)
  for (int f = shortlist.length()-1; f >= 0; --f) {
    int LNum = shortlist[f];
    VTexture *tex = VTexture::CreateTexture(Type, LNum);
    if (!tex) continue;
    tex->Name = shName;
    tex->Shade(shade);
    return txman.AddTexture(tex);
  }

  return -1;
}
*/


//==========================================================================
//
//  MoveNSUp
//
//==========================================================================
static void MoveNSUp (EWadNamespace nslist[], EWadNamespace NS) {
  unsigned idx = 0;
  while (nslist[idx] != WADNS_ZipSpecial && nslist[idx] != NS) ++idx;
  if (idx == 0 || nslist[idx] == WADNS_ZipSpecial) return; // nothing to do
  memmove(&nslist[1], &nslist[0], idx*sizeof(nslist[0]));
  nslist[0] = NS;
}


//==========================================================================
//
//  FixTextureNSList
//
//==========================================================================
static void FixTextureNSList (EWadNamespace nslist[], int Type) {
  switch (Type) {
    case TEXTYPE_Flat: MoveNSUp(nslist, WADNS_Flats); break;
    case TEXTYPE_Overload: MoveNSUp(nslist, WADNS_Graphics); break;
    case TEXTYPE_Sprite: MoveNSUp(nslist, WADNS_Sprites); break;
    case TEXTYPE_Pic: MoveNSUp(nslist, WADNS_Graphics); break;
    case TEXTYPE_SkyMap: MoveNSUp(nslist, WADNS_Graphics); break;
    case TEXTYPE_Skin: MoveNSUp(nslist, WADNS_Graphics); break;
    case TEXTYPE_Autopage: MoveNSUp(nslist, WADNS_Graphics); break;
    case TEXTYPE_FontChar: MoveNSUp(nslist, WADNS_Graphics); break;
    default: break;
  }
}


#define CREATE_TEXTURE_NS_LIST(typevar_)  \
  EWadNamespace nslist[] = { \
    WADNS_Patches, \
    WADNS_Graphics, \
    WADNS_Sprites, \
    WADNS_Flats, \
    WADNS_Global, \
    /* end marker */ \
    WADNS_ZipSpecial, \
  }; \
  FixTextureNSList(nslist, typevar_);


//==========================================================================
//
//  VTextureManager::AddPatch
//
//==========================================================================
int VTextureManager::AddPatch (VName Name, int Type, bool Silent, bool asXHair) {
  if (IsDummyTextureName(Name)) return 0;

  // check if it's already registered
  int i = CheckNumForName(Name, Type);
  if (i >= 0) return i;

  // do not try to load already seen missing texture
  if (isSeenMissingTexture(Name)) return -1; // alas

  CREATE_TEXTURE_NS_LIST(Type)

  for (unsigned nsidx = 0; nslist[nsidx] != WADNS_ZipSpecial; ++nsidx) {
    int tidx = findAndLoadTexture(*this, Name, Type, nslist[nsidx]);
    if (tidx >= 0) {
      //GCon->Logf(NAME_Debug, "AddPatch: '%s' of '%s' found! (%d) (%s)", *Name, VTexture::TexTypeToStr(Type), tidx, *W_FullLumpName(getIgnoreAnim(tidx)->SourceLump));
      if (asXHair) {
        VTexture *tex = getIgnoreAnim(tidx);
        if (tex) tex->ConvertXHairPixels();
      }
      return tidx;
    }
  }

  warnMissingTexture(Name, Silent);
  return -1;
}


//==========================================================================
//
//  VTextureManager::AddPatchLump
//
//==========================================================================
int VTextureManager::AddPatchLump (int LumpNum, VName Name, int Type, bool Silent) {
  vassert(Name != NAME_None);

  // check if it's already registered
  int i = CheckNumForName(Name, Type);
  if (i >= 0) return i;

  if (LumpNum >= 0) {
    VTexture *tex = VTexture::CreateTexture(Type, LumpNum);
    if (tex) {
      tex->Name = Name;
      AddTexture(tex);
      int tidx = CheckNumForName(Name, Type);
      vassert(tidx > 0);
      return tidx;
    }
  }

  // do not try to load already seen missing texture
  if (isSeenMissingTexture(Name)) return -1; // alas

  warnMissingTexture(Name, Silent);
  return -1;
}


//==========================================================================
//
//  VTextureManager::AddRawWithPal
//
//  Adds a raw image with custom palette lump. It's here to support
//  Heretic's episode 2 finale pic.
//
//==========================================================================
int VTextureManager::AddRawWithPal (VName Name, VName PalName) {
  if (IsDummyTextureName(Name)) abort();
  //TODO
  int LumpNum = W_CheckNumForName(Name, WADNS_Graphics);
  if (LumpNum < 0) LumpNum = W_CheckNumForName(Name, WADNS_Sprites);
  if (LumpNum < 0) LumpNum = W_CheckNumForName(Name, WADNS_Global);
  if (LumpNum < 0) LumpNum = W_CheckNumForFileName(VStr(Name));
  if (LumpNum < 0) {
    GCon->Logf(NAME_Warning, "VTextureManager::AddRawWithPal: \"%s\" not found", *Name);
    return -1;
  }
  // check if lump's size to see if it really is a raw image; if not, load it as regular image
  if (W_LumpLength(LumpNum) != 64000) {
    GCon->Logf(NAME_Warning, "VTextureManager::AddRawWithPal: \"%s\" doesn't appear to be a raw image", *Name);
    return AddPatch(Name, TEXTYPE_Pic);
  }

  int i = CheckNumForName(Name, TEXTYPE_Pic);
  if (i >= 0) return i;

  return AddTexture(new VRawPicTexture(LumpNum, W_GetNumForName(PalName)));
}


//==========================================================================
//
//  tryHardToFindTheImageLump
//
//==========================================================================
static int tryHardToFindTheImageLump (VStr filename) {
  if (filename.isEmpty()) return -1;
  int i = W_CheckNumForFileName(filename);
  if (i >= 0) return i;
  /*static*/ const char *exts[] = {
    ".png",
    ".tga",
    ".pcx",
    ".jpg",
    ".jpeg",
    nullptr,
  };
  VStr base = filename.stripExtension();
  for (const char **eptr = exts; *eptr; ++eptr) {
    VStr nn = base+(*eptr);
    i = W_CheckNumForFileName(nn);
    if (i >= 0) {
      GCon->Logf(NAME_Warning, "found image '%s' instead of requested '%s'", *nn, *filename);
      return i;
    }
  }
  return -1;
}


//==========================================================================
//
//  VTextureManager::AddFileTextureChecked
//
//  returns -1 if no file texture found
//
//==========================================================================
int VTextureManager::AddFileTextureChecked (VName Name, int Type, VName forceName) {
  if (IsDummyTextureName(Name)) return 0;

  int i = CheckNumForName(Name, Type);
  if (i >= 0) return i;

  VStr fname = VStr(Name);

  for (int trynum = 0; trynum <= 4; ++trynum) {
    if (Type == TEXTYPE_SkyMap) {
      switch (trynum) {
        case 0: fname = "textures/skybox/"+VStr(Name); break;
        case 1: fname = "textures/skyboxes/"+VStr(Name); break;
        case 2: fname = "textures/skies/"+VStr(Name); break;
        case 3: fname = "textures/"+VStr(Name); break;
        case 4: fname = VStr(Name); break;
        default: abort(); // just in case
      }
    } else {
      if (trynum) break;
    }

    i = tryHardToFindTheImageLump(*fname);
    if (i >= 0) {
      VTexture *Tex = VTexture::CreateTexture(Type, i);
      if (Tex) {
        if (developer) GCon->Logf(NAME_Dev, "******************** %s : %s ********************", *fname, *Tex->Name);
        Tex->Name = (forceName != NAME_None ? forceName : Name);
        return AddTexture(Tex);
      }
    }
  }

  return CheckNumForNameAndForce(Name, Type, true/*bOverload*/, true/*silent*/);
}


//==========================================================================
//
//  VTextureManager::AddFileTexture
//
//==========================================================================
int VTextureManager::AddFileTexture (VName Name, int Type) {
  if (IsDummyTextureName(Name)) return 0;
  int i = AddFileTextureChecked(Name, Type);
  if (i == -1) {
    GCon->Logf(NAME_Warning, "Couldn't create texture \"%s\".", *Name);
    return DefaultTexture;
  }
  return i;
}


//==========================================================================
//
//  VTextureManager::AddFileTextureShaded
//
//  shade==-1: don't shade
//
//==========================================================================
int VTextureManager::AddFileTextureShaded (VName Name, int Type, int shade) {
  if (shade == -1) return AddFileTexture(Name, Type);

  if (IsDummyTextureName(Name)) return 0;

  VName shName = VName(va("%s %08x", *Name, (vuint32)shade));

  int i = CheckNumForName(shName, Type);
  if (i >= 0) return i;

  i = tryHardToFindTheImageLump(*Name);
  if (i >= 0) {
    VTexture *Tex = VTexture::CreateTexture(Type, i);
    if (Tex) {
      Tex->Name = shName;
      Tex->Shade(shade);
      const int res = AddTexture(Tex);
      //GCon->Logf("TEXMAN: loaded shaded texture '%s' (named '%s'; id=%d)", *Name, *shName, res);
      return res;
    }
  }

  GCon->Logf(NAME_Warning, "Couldn't create shaded texture \"%s\".", *Name);
  return DefaultTexture;
}


//==========================================================================
//
//  VTextureManager::AddPatchShaded
//
//==========================================================================
/*
int VTextureManager::AddPatchShaded (VName Name, int Type, int shade, bool Silent) {
  if (shade == -1) return AddPatch(Name, Type, Silent);

  if (IsDummyTextureName(Name)) return 0;

  VName shName = VName(va("%s %08x", *Name, (vuint32)shade));

  // check if it's already registered
  int i = CheckNumForName(shName, Type);
  if (i >= 0) return i;

  // do not try to load already seen missing texture
  if (isSeenMissingTexture(Name)) return -1; // alas

  CREATE_TEXTURE_NS_LIST(Type)

  for (unsigned nsidx = 0; nslist[nsidx] != WADNS_ZipSpecial; ++nsidx) {
    int tidx = findAndLoadTextureShaded(*this, Name, shName, Type, nslist[nsidx], shade);
    if (tidx >= 0) {
      //GCon->Logf(NAME_Warning, "AddPatchShaded: '%s' of '%s' found! (%d)", *shName, VTexture::TexTypeToStr(Type), tidx);
      return tidx;
    }
  }

  warnMissingTexture(Name, Silent);
  return -1;
}
*/


//==========================================================================
//
//  VTextureManager::AddPatchShadedById
//
//  used in decal cloner
//
//==========================================================================
/*
int VTextureManager::AddPatchShadedById (int texid, int shade) {
  if (shade == -1) return texid;
  if (texid <= 0) return texid;
  if (texid >= FirstMapTextureIndex) return -1; //FIXME
  VTexture *tx = getIgnoreAnim(texid);
  if (!tx) return -1;
  if (tx->Type == TEXTYPE_Null) return 0;
  VName shName;
  if (tx->Name == NAME_None) {
    shName = VName(va(" (%d) %08x", texid, (vuint32)shade));
  } else {
    shName = VName(va("%s %08x", *tx->Name, (vuint32)shade));
  }
  // check if it's already registered
  int i = CheckNumForName(shName, tx->Type);
  if (i >= 0) return i;

  if (tx->SourceLump < 0) return -1; // alas

  VTexture *newtx = VTexture::CreateTexture(tx->Type, tx->SourceLump, false/ *setName* /);
  if (!newtx) return -1;

  newtx->Name = shName;
  newtx->Shade(shade);
  return AddTexture(newtx);
}
*/


//==========================================================================
//
//  VTextureManager::CheckNumForNameAndForce
//
//  find or force-load texture
//
//==========================================================================
int VTextureManager::CheckNumForNameAndForce (VName Name, int Type, bool bOverload, bool silent) {
  int tidx = CheckNumForName(Name, Type, bOverload);
  if (tidx >= 0) return tidx;
  // do not try to load already seen missing texture
  if (isSeenMissingTexture(Name)) return -1; // alas

  CREATE_TEXTURE_NS_LIST(Type)

  for (unsigned nsidx = 0; nslist[nsidx] != WADNS_ZipSpecial; ++nsidx) {
    tidx = findAndLoadTexture(*this, Name, Type, nslist[nsidx]);
    if (tidx >= 0) {
      /*
      tidx = CheckNumForName(Name, Type, bOverload);
      if (developer && tidx <= 0) {
        GCon->Logf(NAME_Dev, "CheckNumForNameAndForce: OOPS for '%s'; type=%d; overload=%d", *Name, Type, (int)bOverload);
        const unsigned HashIndex = TextureBucket(GetTypeHash(Name));
        for (int i = TextureHash[HashIndex]; i >= 0; i = getTxByIndex(i)->HashNext) {
          VTexture *tx = getTxByIndex(i);
          if (!tx) abort();
          GCon->Logf(NAME_Dev, "  %d: name='%s'; type=%d", i, *tx->Name, tx->Type);
        }
      }
      vassert(tidx > 0);
      */
      return tidx;
    }
  }

  // alas
  //if (!silent) GCon->Logf(NAME_Warning, "Textures: missing texture \"%s\"", *Name);
  warnMissingTexture(Name, silent);
  return -1;
}


//==========================================================================
//
//  VTextureManager::FindOrLoadFullyNamedTexture
//
//  this can find/load both textures without path (lump-named), and textures with full path
//  it also can return normalized texture name in `normname` (it can be `nullptr` too)
//  returns -1 if not found/cannot load
//
//==========================================================================
int VTextureManager::FindOrLoadFullyNamedTexture (VStr txname, VName *normname, int Type, bool bOverload, bool silent, bool allowLoad, bool forceMapTexture) {
  if (normname) *normname = NAME_None;
  if (txname.isEmpty()) return -1;

  txname = txname.fixSlashes().toLowerCase();

  // check for full path
  if (txname.indexOf('/') < 0) {
    // no path, try lump search
    VStr noext = txname.stripExtension();
    VName loname = VName(*noext, VName::AddLower);
    int res =
      allowLoad ?
        CheckNumForNameAndForce(loname, Type, bOverload, silent) :
        CheckNumForName(loname, Type, bOverload);
    if (res >= 0) {
      if (normname) *normname = loname;
    }
    return res;
  }

  // full path search
  {
    auto tpp = txFullNameHash.find(txname);
    if (tpp) {
      if (r_debug_fullpath_textures) GCon->Logf(NAME_Debug, "found texture '%s' by hash (%s : %s)", *txname, *W_RealLumpName(getTxByIndex(*tpp)->SourceLump), *getTxByIndex(*tpp)->Name);
      if (normname) *normname = VName(*txname);
      return *tpp;
    }
  }

  // not found, try to load it
  {
    int lump = tryHardToFindTheImageLump(txname);
    if (lump >= 0) {
      // try existing texture first (because we may got it with a different extension)
      VStr realtxname = W_RealLumpName(lump);
      auto tpp = txFullNameHash.find(realtxname);
      if (tpp) {
        if (r_debug_fullpath_textures) GCon->Logf(NAME_Debug, "found texture '%s' by hash (%s : %s)", *txname, *W_RealLumpName(getTxByIndex(*tpp)->SourceLump), *getTxByIndex(*tpp)->Name);
        if (normname) *normname = VName(*txname);
        return *tpp;
      }
      // load new texture
      if (allowLoad) {
        VTexture *tx = VTexture::CreateTexture(Type, lump);
        if (tx) {
          if (r_debug_fullpath_textures) GCon->Logf(NAME_Debug, "loaded texture with long name \"%s\"", *txname.quote());
          tx->Name = NAME_None; // don't register in "short names" hash
          const int res = AddFullNameTexture(tx, forceMapTexture);
          if (normname) *normname = VName(*txname);
          return res;
        }
      }
    }
  }

  // alas
  warnMissingTexture(VName(*txname), silent);

  return -1;
}


//==========================================================================
//
//  VTextureManager::AddTextures
//
//  Initialises the texture list with the textures from the textures lump
//
//==========================================================================
void VTextureManager::AddTextures () {
  int NamesFile = -1;
  int LumpTex1 = -1;
  int LumpTex2 = -1;
  int FirstTex;

  TArray<WallPatchInfo> patchtexlookup;
  // for each PNAMES lump load TEXTURE1 and TEXTURE2 from the same wad
  int lastPNameFile = -1; // fuck you, slade!
  /*
  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    if (W_LumpName(Lump) != NAME_pnames) continue;
    NamesFile = W_LumpFile(Lump);
    if (lastPNameFile == NamesFile) continue;
    lastPNameFile = NamesFile;
    LoadPNames(Lump, patchtexlookup);
    LumpTex1 = W_CheckLastNumForNameInFile(NAME_texture1, NamesFile);
    LumpTex2 = W_CheckLastNumForNameInFile(NAME_texture2, NamesFile);
    FirstTex = Textures.length();
    AddTexturesLump(patchtexlookup, LumpTex1, FirstTex, true);
    AddTexturesLump(patchtexlookup, LumpTex2, FirstTex, false);
  }
  */
  int lastPNameLump = -1;
  int pnFileReported = -1;
  for (auto &&it : WadNSNameIterator(NAME_pnames, WADNS_Global)) {
    if (it.getFile() == lastPNameFile) {
      vassert(lastPNameLump != -1);
      if (pnFileReported != lastPNameFile) {
        pnFileReported = lastPNameFile;
        GCon->Logf(NAME_Warning, "duplicate file \"PNAMES\" in archive \"%s\".", *W_FullPakNameForLump(lastPNameLump));
        GCon->Log(NAME_Warning, "THIS IS FUCKIN' WRONG. DO NOT USE BROKEN TOOLS TO CREATE PK3/WAD FILES!");
      }
      continue;
    }
    lastPNameLump = it.lump;
    lastPNameFile = it.getFile();
    NamesFile = lastPNameFile;
    int plump = W_CheckLastNumForNameInFile(NAME_pnames, NamesFile);
    vassert(plump >= 0);
    LoadPNames(plump, patchtexlookup);
    // find "texture1"
    LumpTex1 = W_CheckLastNumForNameInFile(NAME_texture1, NamesFile);
    if (LumpTex1 >= 0) {
      int LT1First = W_CheckFirstNumForNameInFile(NAME_texture1, NamesFile);
      if (LT1First >= 0 && LumpTex1 != LT1First) {
        GCon->Logf(NAME_Warning, "duplicate file \"TEXTURE1\" in archive \"%s\".", *W_FullPakNameForLump(it.lump));
        GCon->Log(NAME_Warning, "THIS IS FUCKIN' WRONG. DO NOT USE BROKEN TOOLS TO CREATE PK3/WAD FILES!");
        //LumpTex1 = LT1First;
      }
    }
    // find "texture2"
    LumpTex2 = W_CheckLastNumForNameInFile(NAME_texture2, NamesFile);
    if (LumpTex2 >= 0) {
      int LT2First = W_CheckFirstNumForNameInFile(NAME_texture2, NamesFile);
      if (LT2First >= 0 && LumpTex2 != LT2First) {
        GCon->Logf(NAME_Warning, "duplicate file \"TEXTURE2\" in archive \"%s\".", *W_FullPakNameForLump(it.lump));
        GCon->Log(NAME_Warning, "THIS IS FUCKIN' WRONG. DO NOT USE BROKEN TOOLS TO CREATE PK3/WAD FILES!");
        //LumpTex2 = LT2First;
      }
    }
    // load textures
    FirstTex = Textures.length();
    AddTexturesLump(patchtexlookup, LumpTex1, FirstTex, true);
    AddTexturesLump(patchtexlookup, LumpTex2, FirstTex, false);
  }

  // if last TEXTURE1 or TEXTURE2 are in a wad without a PNAMES, they must be loaded too
  int LastTex1 = W_CheckNumForName(NAME_texture1);
  int LastTex2 = W_CheckNumForName(NAME_texture2);
  if (LastTex1 >= 0 && (LastTex1 == LumpTex1 || W_LumpFile(LastTex1) <= NamesFile)) LastTex1 = -1;
  if (LastTex2 >= 0 && (LastTex2 == LumpTex2 || W_LumpFile(LastTex2) <= NamesFile)) LastTex2 = -1;
  if (LastTex1 != -1 || LastTex2 != -1) {
    FirstTex = Textures.length();
    LoadPNames(W_GetNumForName(NAME_pnames), patchtexlookup);
    AddTexturesLump(patchtexlookup, LastTex1, FirstTex, true);
    AddTexturesLump(patchtexlookup, LastTex2, FirstTex, false);
  }
}


//==========================================================================
//
//  VTextureManager::LoadPNames
//
//  load the patch names from pnames.lmp
//
//==========================================================================
void VTextureManager::LoadPNames (int NamesLump, TArray<WallPatchInfo> &patchtexlookup) {
  patchtexlookup.clear();
  if (NamesLump < 0) return;
  int pncount = 0;
  int NamesFile = W_LumpFile(NamesLump);
  VStr pkname = W_FullPakNameForLump(NamesLump);
  while (NamesLump >= 0 && W_LumpFile(NamesLump) == NamesFile) {
    if (W_LumpName(NamesLump) != NAME_pnames) {
      // next one
      NamesLump = W_IterateNS(NamesLump, WADNS_Global);
      continue;
    }
    if (pncount++ == 1) {
      GCon->Logf(NAME_Warning, "duplicate file \"PNAMES\" in archive \"%s\".", *W_FullPakNameForLump(NamesLump));
      GCon->Log(NAME_Warning, "THIS IS FUCKIN' WRONG. DO NOT USE BROKEN TOOLS TO CREATE PK3/WAD FILES!");
    }
    VCheckedStream Strm(NamesLump, true); // use Sys_Error
    if (developer) GCon->Logf(NAME_Dev, "READING '%s' 0x%08x (%d)", *W_FullLumpName(NamesLump), NamesLump, Strm.TotalSize());
    vint32 nummappatches = Streamer<vint32>(Strm);
    if (nummappatches < 0 || nummappatches > 1024*1024) Sys_Error("%s: invalid number of patches in pnames (%d)", *W_FullLumpName(NamesLump), nummappatches);
    //VTexture **patchtexlookup = new VTexture *[nummappatches];
    GCon->Logf(NAME_Init, "loading pnames from '%s' (%d patches)", *W_FullLumpName(NamesLump), nummappatches);
    TimedReportInit report("  ", nummappatches);

    for (int i = 0; i < nummappatches; ++i, report.step()) {
      // read patch name
      char TmpName[12];
      Strm.Serialise(TmpName, 8);
      //if (Strm.IsError()) Sys_Error("%s: error reading PNAMES", *W_FullLumpName(NamesLump));
      TmpName[8] = 0;

      bool warned = false;
      for (int cc = 0; TmpName[cc]; ++cc) {
        if ((vuint8)TmpName[cc] < 32 || (vuint8)TmpName[cc] >= 127) {
          if (!warned) {
            warned = true;
            vuint8 nc = (vuint8)TmpName[cc]&0x7f;
            if (nc < 32 || nc >= 127) {
              Sys_Error("%s: record #%d, name is <%s> (0x%08x)", *W_FullLumpName(NamesLump), i, TmpName, (unsigned)(Strm.Tell()-8));
            } else {
              GCon->Logf("%s: record #%d, name is <%s> (0x%08x)", *W_FullLumpName(NamesLump), i, TmpName, (unsigned)(Strm.Tell()-8));
            }
            TmpName[cc] = nc;
          }
        }
      }

      //if (developer) GCon->Logf(NAME_Dev, "PNAMES entry #%d is '%s'", i, TmpName);
      if (!TmpName[0]) {
        GCon->Logf(NAME_Warning, "PNAMES entry #%d is empty!", i);
        WallPatchInfo &wpi = patchtexlookup.alloc();
        wpi.index = patchtexlookup.length()-1;
        wpi.name = NAME_None;
        wpi.tx = nullptr;
        continue;
      }

      //GCon->Logf(NAME_Init, "  [%d/%d]: <%s>", i+1, nummappatches, TmpName);
      VName PatchName(TmpName, VName::AddLower8);
      CheckAddNumberedName(PatchName);

      WallPatchInfo &wpi = patchtexlookup.alloc();
      wpi.index = patchtexlookup.length()-1;
      wpi.name = PatchName;

      // check if it's already has been added
      /*
      int PIdx = CheckNumForName(PatchName, TEXTYPE_WallPatch, false);
      if (PIdx >= 0) {
        //patchtexlookup[i] = Textures[PIdx];
        wpi.tx = Textures[PIdx];
        vassert(wpi.tx);
        if (developer) GCon->Logf(NAME_Dev, "PNAMES(%s): found texture patch '%s' (%d/%d)", *W_FullLumpName(NamesLump), *PatchName, i, nummappatches-1);
        continue;
      }
      */

      bool isFlat = false;
      // get wad lump number
      int LNum = W_CheckNumForName(PatchName, WADNS_Patches);
      // sprites, flats, etc. also can be used as patches
      if (LNum < 0) { LNum = W_CheckNumForName(PatchName, WADNS_Flats); if (LNum >= 0) isFlat = true; } // eh, why not?
      if (LNum < 0) LNum = W_CheckNumForName(PatchName, WADNS_NewTextures);
      if (LNum < 0) LNum = W_CheckNumForName(PatchName, WADNS_Sprites);
      if (LNum < 0) LNum = W_CheckNumForName(PatchName, WADNS_Global); // just in case

      // add it to textures
      if (LNum < 0) {
        wpi.tx = nullptr;
        //patchtexlookup[i] = nullptr;
        if (game_options.warnPNames || !W_IsIWADLump(NamesLump)) {
          GCon->Logf(NAME_Warning, "PNAMES(%s): cannot find texture patch '%s' (%d/%d)", *W_FullLumpName(NamesLump), *PatchName, i, nummappatches-1);
        }
      } else {
        //patchtexlookup[i] = VTexture::CreateTexture(TEXTYPE_WallPatch, LNum);
        //if (patchtexlookup[i]) AddTexture(patchtexlookup[i]);
        wpi.tx = VTexture::CreateTexture((isFlat ? TEXTYPE_Flat : TEXTYPE_WallPatch), LNum);
        if (!wpi.tx) GCon->Logf(NAME_Warning, "%s: loading patch '%s' (%d/%d) failed", *W_FullLumpName(NamesLump), *PatchName, i, nummappatches-1);
        if (wpi.tx) {
          vassert(wpi.tx->SourceLump == LNum);
          AddTexture(wpi.tx);
        }
      }
    }
    report.finalReport();
    // next one
    NamesLump = W_IterateNS(NamesLump, WADNS_Global);
  }


  if (developer) {
    for (int f = 0; f < patchtexlookup.length(); ++f) {
      vassert(patchtexlookup[f].index == f);
      VTexture *tx = patchtexlookup[f].tx;
      GCon->Logf(NAME_Dev, "%s:PNAME (%d/%d): name=%s; tx=%d; txname=%s (%s : %s)", *pkname, f, patchtexlookup.length()-1, *patchtexlookup[f].name, (tx ? 1 : 0), (tx ? *tx->Name : "----"),
        (tx && tx->SourceLump >= 0 ? *W_FullLumpName(tx->SourceLump) : "<?>"), (tx ? VTexture::TexTypeToStr(tx->Type) : "(none)"));
    }
  }
}


//==========================================================================
//
//  VTextureManager::AddMissingNumberedTextures
//
//  Initialises the texture list with the textures from the textures lump
//
//==========================================================================
void VTextureManager::AddMissingNumberedTextures () {
  //k8: force-load numbered textures
  for (auto &&it : numberedNamesList) {
    const char *txname = *it;
    int namelen = VStr::length(txname);
    if (namelen > 8) continue; // too long
    if (namelen && txname[namelen-1] == '1') {
      char nbuf[130];
      strcpy(nbuf, txname);
      for (int c = 2; c < 10; ++c) {
        nbuf[namelen-1] = '0'+c;
        VName PatchName(nbuf, VName::FindLower8);
        // if the name is empty, it means that we don't have such lump, so there is no reason to check anything
        if (PatchName == NAME_None) continue;
        if (CheckNumForName(PatchName, TEXTYPE_WallPatch, false) < 0) {
          int tid = CheckNumForNameAndForce(PatchName, TEXTYPE_WallPatch, true, true);
          if (tid > 0) GCon->Logf(NAME_Init, "Textures: force-loaded numbered texture '%s'", nbuf);
        } else {
          if (developer) GCon->Logf(NAME_Dev, "Textures: already loaded numbered texture '%s'", nbuf);
        }
      }
    }
  }
}


//==========================================================================
//
//  VTextureManager::AddTexturesLump
//
//==========================================================================
void VTextureManager::AddTexturesLump (TArray<WallPatchInfo> &patchtexlookup, int TexLump, int FirstTex, bool First) {
  if (TexLump < 0) return;

  vassert(inMapTextures == 0);
  VName tlname = W_LumpName(TexLump);
  TMapNC<VName, bool> tseen; // only the first seen texture is relevant, so ignore others

  int pncount = 0;
  int TexFile = W_LumpFile(TexLump);
  while (TexLump >= 0 && W_LumpFile(TexLump) == TexFile) {
    if (W_LumpName(TexLump) != tlname) {
      // next one
      TexLump = W_IterateNS(TexLump, WADNS_Global);
      continue;
    }
    if (pncount++ == 1) {
      GCon->Logf(NAME_Warning, "duplicate file \"%s\" in archive \"%s\".", *tlname, *W_FullPakNameForLump(TexLump));
      GCon->Log(NAME_Warning, "THIS IS *ABSOLUTELY* FUCKIN' WRONG. DO NOT USE BROKEN TOOLS TO CREATE PK3/WAD FILES!");
      break;
    }

    // load the map texture definitions from textures.lmp
    // the data is contained in one or two lumps, TEXTURE1 for shareware, plus TEXTURE2 for commercial
    VCheckedStream Strm(TexLump, true); // use Sys_Error
    vint32 NumTex = Streamer<vint32>(Strm);

    GCon->Logf(NAME_Init, "loading TEXTURES from '%s' (%d textures)", *W_FullLumpName(TexLump), NumTex);
    TimedReportInit report("  ", NumTex);

    // check the texture file format
    bool IsStrife = false;
    vint32 PrevOffset = Streamer<vint32>(Strm);
    for (int i = 0; i < NumTex-1; ++i, report.step()) {
      vint32 Offset = Streamer<vint32>(Strm);
      if (Offset-PrevOffset == 24) {
        IsStrife = true;
        GCon->Logf(NAME_Init, "Strife textures detected in lump '%s'", *W_FullLumpName(TexLump));
        break;
      }
      PrevOffset = Offset;
    }

    for (int i = 0; i < NumTex; ++i) {
      VMultiPatchTexture *Tex = new VMultiPatchTexture(Strm, i, patchtexlookup, FirstTex, IsStrife);
      // copy dimensions of the first texture to the dummy texture in case they are used, and
      // set it to be `TEXTYPE_Null`, as this is how DooM works
      if (i == 0 && First) {
        // the very first texture, it is dummy; no need to append it
        Textures[0]->Width = Tex->Width;
        Textures[0]->Height = Tex->Height;
        Tex->Type = TEXTYPE_Null;
        DummyTextureName = Tex->Name;
        GCon->Logf(NAME_Init, "dummy texture name is '%s', from the lump '%s'", *VStr(DummyTextureName).toUpperCase(), *W_FullLumpName(TexLump));
        delete Tex;
        continue;
      } else {
        // ignore empty textures
        // use `IsDummyTextureName()` here, to reject duplicate dummy textures (this should not happen, tho)
        if (IsDummyTextureName(Tex->Name)) { delete Tex; continue; }
        //if (Tex->Name == NAME_Minus_Sign) { delete Tex; continue; }
      }
      // but keep duplicate ones, because later pwads can replace some of them
      if (Tex->SourceLump < TexLump) Tex->SourceLump = TexLump;
      if (tseen.has(Tex->Name)) {
        if (developer) GCon->Logf(NAME_Dev, "textures lump '%s': replacing texture '%s'", *W_FullLumpName(TexLump), *Tex->Name);
        AddTexture(Tex);
      } else {
        if (developer) GCon->Logf(NAME_Dev, "textures lump '%s': adding texture '%s'", *W_FullLumpName(TexLump), *Tex->Name);
        tseen.put(Tex->Name, true);
        AddTexture(Tex);
      }
    }
    report.finalReport();
    // next one
    TexLump = W_IterateNS(TexLump, WADNS_Global);
  }
}


//==========================================================================
//
//  VTextureManager::AddGroup
//
//==========================================================================
void VTextureManager::AddGroup (int Type, EWadNamespace Namespace) {
  int counter = 0;
  for (int Lump = W_IterateNS(-1, Namespace); Lump >= 0; Lump = W_IterateNS(Lump, Namespace)) ++counter;
  TimedReportInit report("  ", "textures added", counter);
  for (int Lump = W_IterateNS(-1, Namespace); Lump >= 0; Lump = W_IterateNS(Lump, Namespace)) {
    // to avoid duplicates, add only the last one
    if (W_GetNumForName(W_LumpName(Lump), Namespace) == Lump) {
      //GCon->Logf("VTextureManager::AddGroup(%d:%d): loading lump '%s'", Type, Namespace, *W_FullLumpName(Lump));
      report.stepCounter2();
      AddTexture(VTexture::CreateTexture(Type, Lump));
    } else {
      //GCon->Logf(NAME_Dev, "VTextureManager::AddGroup(%d:%d): skipped lump '%s'", Type, Namespace, *W_FullLumpName(Lump));
    }
    report.step();
  }
  report.finalReport();
}


//==========================================================================
//
//  VTextureManager::FindHiResToReplace
//
//  this tries to add `TEXTYPE_Pic` patch if `Type` not found
//  returns -1 on error; never returns 0 (0 is "not found")
//
//==========================================================================
int VTextureManager::FindHiResToReplace (VName Name, int Type, bool bOverload) {
  if (IsDummyTextureName(Name)) return -1;
  // find texture to replace
  int OldIdx = CheckNumForName(Name, Type, bOverload);
  if (OldIdx > 0) return OldIdx;
  OldIdx = CheckNumForName(Name, TEXTYPE_Pic, bOverload);
  if (OldIdx > 0) return OldIdx;
  // try to load picture patch
  OldIdx = AddPatch(Name, TEXTYPE_Pic, true/*silent*/);
  return (OldIdx <= 0 ? -1 : OldIdx);
}


//==========================================================================
//
//  VTextureManager::ReplaceTextureWithHiRes
//
//  this can also append a new texture if `OldIndex` is < 0
//  it can do `delete NewTex` too
//
//==========================================================================
void VTextureManager::ReplaceTextureWithHiRes (int OldIndex, VTexture *NewTex, int oldWidth, int oldHeight) {
  vassert(inMapTextures == 0);
  if (!NewTex) return; // just in case

  if (NewTex->GetWidth() < 1 || NewTex->GetHeight() < 1 || NewTex->GetWidth() > 8192 || NewTex->GetHeight() > 8192) {
    // oops
    GCon->Logf(NAME_Error, "hires texture '%s' has invalid dimensions (%dx%d)", *NewTex->Name, NewTex->GetWidth(), NewTex->GetHeight());
    delete NewTex;
    return;
  }

  if (OldIndex == NewTex->SourceLump) {
    delete NewTex;
    return;
  }

  if (OldIndex < 0) {
    // add it as a new texture
    //NewTex->Type = TEXTYPE_Overload; //k8: nope, it is new
    AddTexture(NewTex);
    ++hitexNewCount;
    //GCon->Logf(NAME_Debug, "*** new hires texture '%s' (%s)", *NewTex->Name, *W_FullLumpName(NewTex->SourceLump));
  } else {
    // replace existing texture by adjusting scale and offsets
    VTexture *OldTex = Textures[OldIndex];
    bool doSOffset = false, doTOffset = false;
    if (oldWidth < 0) { oldWidth = OldTex->GetScaledWidthI(); doSOffset = true; }
    if (oldHeight < 0) { oldHeight = OldTex->GetScaledHeightI(); doTOffset = true; }
    //NewTex->Type = OldTex->Type;
    //k8: it is overriden by the original texture type
    //    i don't even know what `overload` meant to do, and it seems to work this way
    //    FIXME: investigate this later
    //if (OldTex->SScale != 1.0f || OldTex->TScale != 1.0f) GCon->Logf(NAME_Debug, "OLD: %s: oldsize=(%d,%d); oldscale=(%g,%g); newsize=(%d,%d); newscale=(%g,%g)", *OldTex->Name, OldTex->GetWidth(), OldTex->GetHeight(), OldTex->SScale, OldTex->TScale, NewTex->GetWidth(), NewTex->GetHeight(), NewTex->SScale, NewTex->TScale);
    NewTex->hiresRepTex = true;
    NewTex->Type = TEXTYPE_Overload;
    //GCon->Logf(NAME_Debug, "*** REPLACE0 <%s> (%d)", *OldTex->Name, OldIndex);
    NewTex->Name = OldTex->Name;
    NewTex->TextureTranslation = OldTex->TextureTranslation; //k8: do we need this at all?
    NewTex->bWorldPanning = true; //FIXME: this assumes that the original texture scaling is 1
    NewTex->SScale = NewTex->GetWidth()/max2(1, oldWidth);
    NewTex->TScale = NewTex->GetHeight()/max2(1, oldHeight);
    if (!isFiniteF(NewTex->SScale) || NewTex->SScale <= 0.0f) NewTex->SScale = 1.0f; // just in case
    if (!isFiniteF(NewTex->TScale) || NewTex->TScale <= 0.0f) NewTex->TScale = 1.0f; // just in case
    if (doSOffset) NewTex->SOffset = (int)floorf(OldTex->GetScaledSOffsetF()*NewTex->SScale);
    if (doTOffset) NewTex->TOffset = (int)floorf(OldTex->GetScaledTOffsetF()*NewTex->TScale);
    //if (OldTex->SScale != 1.0f || OldTex->TScale != 1.0f) GCon->Logf(NAME_Debug, "NEW: %s: oldsize=(%d,%d); oldscale=(%g,%g); newsize=(%d,%d); newscale=(%g,%g)", *OldTex->Name, OldTex->GetWidth(), OldTex->GetHeight(), OldTex->SScale, OldTex->TScale, NewTex->GetWidth(), NewTex->GetHeight(), NewTex->SScale, NewTex->TScale);
    NewTex->Type = OldTex->Type; // oops, type is forced here
    NewTex->TextureTranslation = OldTex->TextureTranslation;
    NewTex->HashNext = OldTex->HashNext;
    Textures[OldIndex] = NewTex;
    // force non-translucency for lightmapping
    // no need to do this anymore, solid texture texels should be processed as usual
    //if (NewTex->isTranslucent() && !OldTex->isTranslucent()) NewTex->ResetTranslucentFlag();
    // we may not need it for a long time
    //GCon->Logf(NAME_Debug, "*** releasing %s", *OldTex->Name);
    OldTex->ReleasePixels();
    //GCon->Logf(NAME_Debug, "*** released %s", *OldTex->Name);
    // k8: don't delete old texture, it can be referenced out there
    //delete OldTex;
    //OldTex = nullptr;
    ++hitexReplaceCount;
  }
  // we may not need it for a long time
  NewTex->ReleasePixels();

  const double ctt = Sys_Time();
  if (hitexLastMessageTime < 0 || ctt-hitexLastMessageTime >= 1.5) {
    if (hitexLastMessageTime < 0) {
      GCon->Log(NAME_Init, "loading hires textures");
    } else {
      GCon->Logf(NAME_Init, "   %d hires textures processed", hitexNewCount+hitexReplaceCount);
    }
    hitexLastMessageTime = ctt;
  }
}


//==========================================================================
//
//  VTextureManager::AddHiResTexturesFromNS
//
//==========================================================================
void VTextureManager::AddHiResTexturesFromNS (EWadNamespace NS, int ttype, bool &messaged) {
  for (auto &&it : WadNSIterator(NS)) {
    int lump = it.lump;
    VName Name = W_LumpName(lump);
    // to avoid duplicates, add only the last one
    if (W_GetNumForName(Name, NS) != lump) continue;

    // find texture to replace
    int OldIdx = FindHiResToReplace(Name, ttype);
    if (OldIdx > 0 && !r_hirestex) continue; // replacement is disabled

    //if (!messaged) { messaged = true; GCon->Log(NAME_Init, "adding hires texture replacements"); }

    // create new texture
    VTexture *NewTex = VTexture::CreateTexture(TEXTYPE_Any, lump);
    if (!NewTex) continue;
    if (NewTex->Type == TEXTYPE_Any) NewTex->Type = ttype;

    ReplaceTextureWithHiRes(OldIdx, NewTex);
  }
}


//==========================================================================
//
//  VTextureManager::AddHiResTextures
//
//==========================================================================
void VTextureManager::AddHiResTextures () {
  vassert(inMapTextures == 0);
  bool messaged = false;
  // GZDoom
  AddHiResTexturesFromNS(WADNS_HiResTextures, TEXTYPE_Wall, messaged);
  // jdoom/doomsday textures
  AddHiResTexturesFromNS(WADNS_HiResTexturesDDay, TEXTYPE_Wall, messaged);
  // jdoom/doomsday flats
  AddHiResTexturesFromNS(WADNS_HiResFlatsDDay, TEXTYPE_Flat, messaged);
}


//==========================================================================
//
//  VTextureManager::ParseTextureTextLump
//
//==========================================================================
void VTextureManager::ParseTextureTextLump (int lump, bool asHiRes) {
  GCon->Logf(NAME_Init, "parsing %stextures script \"%s\"", (asHiRes ? "hires " : ""), *W_FullLumpName(lump));
  VScriptParser *sc = VScriptParser::NewWithLump(lump);
  while (!sc->AtEnd()) {
    if (sc->Check("remap")) {
      // new (possibly hires) texture
      int Type = TEXTYPE_Any;
      bool Overload = false;

      if (sc->Check("wall")) {
        Type = TEXTYPE_Wall;
        Overload = true;
      } else if (sc->Check("flat")) {
        Type = TEXTYPE_Flat;
        Overload = true;
      } else if (sc->Check("sprite")) {
        Type = TEXTYPE_Sprite;
      }

      // old texture name
      sc->ExpectName8Warn();
      if (!asHiRes) {
        // skip new texture name
        sc->ExpectName8Warn();
        continue;
      }

      int OldIdx = FindHiResToReplace(sc->Name8, Type, Overload);
      if (OldIdx < 0) {
        // nothing to remap (no old texture)
        VName oldname = sc->Name8;
        // skip new texture name
        sc->ExpectName8Warn();
        GCon->Logf(NAME_Warning, "cannot remap old texture '%s' to new texture '%s' (old texture not found)", *oldname, *sc->Name8);
        continue;
      }

      // new texture name
      sc->ExpectName8Warn();
      if (!r_hirestex) continue; // replacement is disabled

      int LumpIdx = W_CheckNumForName(sc->Name8, WADNS_Graphics);
      if (LumpIdx <= 0) continue;

      // create new texture
      VTexture *NewTex = VTexture::CreateTexture(TEXTYPE_Any, LumpIdx);
      if (!NewTex) continue;

      ReplaceTextureWithHiRes(OldIdx, NewTex);
      continue;
    }

    if (sc->Check("define")) {
      // lump name
      sc->ExpectName();
      VName Name = sc->Name;
      int LumpIdx = W_CheckNumForName(sc->Name, WADNS_Graphics);
      if (LumpIdx < 0 && sc->String.length() > 8) {
        VName Name8 = VName(*sc->String, VName::AddLower8);
        LumpIdx = W_CheckNumForName(Name8, WADNS_Graphics);
        //FIXME: fix name?
        if (LumpIdx >= 0) {
          if (asHiRes) GCon->Logf(NAME_Warning, "Texture: found short texture '%s' for long texture '%s'", *Name8, *Name);
          Name = Name8;
        }
      }
      sc->Check("force32bit");

      // dimensions
      sc->ExpectNumber();
      int Width = sc->Number;
      sc->ExpectNumber();
      int Height = sc->Number;
      if (LumpIdx < 0) continue;
      if (Width < 1 || Height < 1 || Width > 4096 || Height > 4096) {
        GCon->Logf(NAME_Error, "invalid replacement texture '%s' dimensions (%dx%d)", *W_FullLumpName(LumpIdx), Width, Height);
        continue;
      }

      int OldIdx = FindHiResToReplace(Name, TEXTYPE_Overload, false);
      if (OldIdx >= 0) {
        // replacing old texture
        //if (IsEmptyTexture(OldIdx)) continue; // there is no reason to replace "AASHITTY" and such (this is checked in `FindHiResToReplace()`
        // don't do it if hires textures are disabled, or we're loading normal textures
        if (!asHiRes) continue; // loading normal textures
        if (!r_hirestex) continue; // hires texture replacements are disabled
      }
      //if (!r_hirestex && OldIdx < 0) continue; // don't replace

      // create new texture
      VTexture *NewTex = VTexture::CreateTexture(TEXTYPE_Overload, LumpIdx);
      if (!NewTex) continue;
      NewTex->Name = Name.GetLower();

      ReplaceTextureWithHiRes(OldIdx, NewTex, Width, Height);
      continue;
    }

    int ttype = TEXTYPE_Null;
    const VTextLocation loc = sc->GetLoc();

         if (sc->Check("walltexture")) ttype = TEXTYPE_Wall;
    else if (sc->Check("flat")) ttype = TEXTYPE_Flat;
    else if (sc->Check("texture")) ttype = TEXTYPE_Overload;
    else if (sc->Check("sprite")) ttype = TEXTYPE_Sprite;
    else if (sc->Check("graphic")) ttype = TEXTYPE_Pic;
    else sc->Error(va("Bad texture command: '%s'", *sc->String));

    //GCon->Logf(NAME_Debug, "%s: ***ttype=%s", *loc.toStringNoCol(), VTexture::TexTypeToStr(ttype));
    if (asHiRes) { sc->SkipBracketed(false); continue; }

    VTexture *Tex = new VMultiPatchTexture(sc, ttype);
    if (!Tex) {
      GCon->Logf(NAME_Warning, "%s: cannot add texture with type '%s'", *loc.toStringNoCol(), VTexture::TexTypeToStr(ttype));
      continue;
    }

    if (Textures.length() && IsDummyTextureName(Tex->Name)) {
      GCon->Logf(NAME_Error, "tried to add dummy texture with name '%s' (%s) as a normal one (from lump '%s')", *VStr(Tex->Name).toUpperCase(), VTexture::TexTypeToStr(Tex->Type), *W_FullLumpName(Tex->SourceLump));
      delete Tex;
      continue;
    }

    AddTexture(Tex);
  }
  delete sc;
}


//==========================================================================
//
//  VTextureManager::AddTextureTextLumps
//
//==========================================================================
void VTextureManager::AddTextureTextLumps () {
  vassert(inMapTextures == 0);
  CollectTextureDefLumps();
  for (auto &&lump : textureDefLumps) ParseTextureTextLump(lump, false);
}


//==========================================================================
//
//  VTextureManager::AddHiResTextureTextLumps
//
//==========================================================================
void VTextureManager::AddHiResTextureTextLumps () {
  vassert(inMapTextures == 0);
  CollectTextureDefLumps();
  for (auto &&lump : textureDefLumps) ParseTextureTextLump(lump, true); // (re)parse for hires
}


//==========================================================================
//
//  R_DumpTextures
//
//==========================================================================
void R_DumpTextures () {
  GCon->Log("=========================");
  GCon->Logf("texture count: %d", GTextureManager.Textures.length());
  for (int f = 0; f < GTextureManager.Textures.length(); ++f) {
    VTexture *tx = GTextureManager.Textures[f];
    if (!tx) { GCon->Logf(" %d: <EMPTY>", f); continue; }
    GCon->Logf("  %d: name='%s'; size=(%dx%d); srclump=%d", f, *tx->Name, tx->Width, tx->Height, tx->SourceLump);
  }
  GCon->Logf(" map-local texture count: %d", GTextureManager.MapTextures.length());
  for (int f = 0; f < GTextureManager.MapTextures.length(); ++f) {
    VTexture *tx = GTextureManager.MapTextures[f];
    if (!tx) { GCon->Logf(" %d: <EMPTY>", f+VTextureManager::FirstMapTextureIndex); continue; }
    GCon->Logf("  %d: name='%s'; size=(%dx%d); srclump=%d", f+VTextureManager::FirstMapTextureIndex, *tx->Name, tx->Width, tx->Height, tx->SourceLump);
  }

  GCon->Log("=========================");
  for (unsigned hb = 0; hb < VTextureManager::HASH_SIZE; ++hb) {
    GCon->Logf(" hash bucket %05u", hb);
    for (int i = GTextureManager.TextureHash[hb]; i >= 0; i = GTextureManager.getTxByIndex(i)->HashNext) {
      VTexture *tx = GTextureManager.getTxByIndex(i);
      if (!tx) abort();
      GCon->Logf(NAME_Dev, "  %05d:%u: name='%s'; type=%d", hb, i, *tx->Name, tx->Type);
    }
  }
}


//==========================================================================
//
//  LoadIcon
//
//==========================================================================
static void LoadIcon (VName icnName, VStr icnPath) {
  if (W_CheckNumForName(icnName) >= 0) {
    GTextureManager.AddPatch(icnName, TEXTYPE_Pic, true);
    return;
  }
  if (icnPath.length()) {
    GTextureManager.AddFileTextureChecked(*icnPath, TEXTYPE_Pic, icnName);
  }
}


int gtxRight = -1;
int gtxLeft = -1;
int gtxTop = -1;
int gtxBottom = -1;
int gtxFront = -1;
int gtxBack = -1;

//==========================================================================
//
//  R_InitTexture
//
//==========================================================================
void R_InitTexture () {
  GTextureManager.Init();
  R_InitFTAnims(); // init flat and texture animations
  GTextureManager.WipeWallPatches();
  vassert(GTextureManager.MapTextures.length() == 0);
  /*
  if (W_CheckNumForName(NAME_teleicon) >= 0) GTextureManager.AddPatch(NAME_teleicon, TEXTYPE_Pic, true);
  if (W_CheckNumForName(NAME_saveicon) >= 0) GTextureManager.AddPatch(NAME_saveicon, TEXTYPE_Pic, true);
  if (W_CheckNumForName(NAME_loadicon) >= 0) GTextureManager.AddPatch(NAME_loadicon, TEXTYPE_Pic, true);
  */
  LoadIcon(NAME_teleicon, VStr());
  LoadIcon(NAME_saveicon, "graphics/k8vavoom_special/k8vavoom_save.png");
  LoadIcon(NAME_loadicon, "graphics/k8vavoom_special/k8vavoom_load.png");
  if (developer) GTextureManager.DumpHashStats(NAME_Dev);

  /*
  GCon->Logf(NAME_Debug, "right=%d", (gtxLeft = GTextureManager.AddFileTextureChecked("textures/right.png", TEXTYPE_Pic)));
  GCon->Logf(NAME_Debug, "left=%d", (gtxRight = GTextureManager.AddFileTextureChecked("textures/left.png", TEXTYPE_Pic)));
  GCon->Logf(NAME_Debug, "top=%d", (gtxTop = GTextureManager.AddFileTextureChecked("textures/top.png", TEXTYPE_Pic)));
  GCon->Logf(NAME_Debug, "bottom=%d", (gtxBottom = GTextureManager.AddFileTextureChecked("textures/bottom.png", TEXTYPE_Pic)));
  GCon->Logf(NAME_Debug, "back=%d", (gtxBack = GTextureManager.AddFileTextureChecked("textures/back.png", TEXTYPE_Pic)));
  GCon->Logf(NAME_Debug, "front=%d", (gtxFront = GTextureManager.AddFileTextureChecked("textures/front.png", TEXTYPE_Pic)));
  */

  if (cli_DumpTextures > 0) R_DumpTextures();
}


//==========================================================================
//
//  R_InitHiResTextures
//
//==========================================================================
void R_InitHiResTextures () {
  // initialise hires textures
  hitexReplaceCount = hitexNewCount = 0;
  GTextureManager.AddHiResTextures();
  GTextureManager.AddHiResTextureTextLumps();
  if (hitexReplaceCount) GCon->Logf(NAME_Init, "replaced %d texture%s with hires one%s", hitexReplaceCount, (hitexReplaceCount == 1 ? "" : "s"), (hitexReplaceCount == 1 ? "" : "s"));
  if (hitexNewCount) GCon->Logf(NAME_Init, "added %d new hires texture%s", hitexNewCount, (hitexNewCount == 1 ? "" : "s"));
}


//==========================================================================
//
//  R_ShutdownTexture
//
//==========================================================================
void R_ShutdownTexture () {
  R_ShutdownFTAnims();
  // shut down texture manager
  GTextureManager.Shutdown();
}


//==========================================================================
//
//  VTextureManager::FillNameAutocompletion
//
//  to use in `ExportTexture` command
//
//==========================================================================
void VTextureManager::FillNameAutocompletion (VStr prefix, TArray<VStr> &list) {
  if (prefix.length() == 0) {
    GCon->Logf("\034KThere are about %d textures, be more specific, please!", Textures.length()+MapTextures.length()-1);
    return;
  }
  for (int f = 1; f < Textures.length(); ++f) {
    if (Textures[f]->Name == NAME_None) continue;
    if (VStr::startsWithNoCase(*Textures[f]->Name, *prefix)) list.append(VStr(Textures[f]->Name));
  }
  for (int f = 0; f < MapTextures.length(); ++f) {
    if (MapTextures[f]->Name == NAME_None) continue;
    if (VStr::startsWithNoCase(*MapTextures[f]->Name, *prefix)) list.append(VStr(MapTextures[f]->Name));
  }
}


//==========================================================================
//
//  VTextureManager::GetExistingTextureByName
//
//  to use in `ExportTexture` command
//
//==========================================================================
VTexture *VTextureManager::GetExistingTextureByName (VStr txname, int type) {
  VName nn = VName(*txname, VName::FindLower);
  if (nn == NAME_None) return nullptr;

  int idx = CheckNumForName(nn, type, true/*overloads*/);
  if (idx >= 0) return this->operator()(idx);
  return nullptr;
}


//==========================================================================
//
//  VTextureManager::LoadSpriteOffsets
//
//  based on GZDoom code
//
//==========================================================================
void VTextureManager::LoadSpriteOffsets () {
  TMapNC<vuint32, bool> donotprocess;

  if (cli_SkipSprOfs > 0) return;

  // check for replaced sprites
  for (auto &&tex : Textures) {
    if (tex->SourceLump < 0) continue;
    if (W_IsIWADLump(tex->SourceLump)) continue;
    const char *n = *tex->Name;
    if (VStr::length(n) < 4) continue; // just in case
    vuint32 sprid;
    memcpy(&sprid, n, 4);
    donotprocess.put(sprid, true);
  }

  for (auto &&it : WadNSNameIterator(VName("sprofs"), WADNS_Global)) {
    int fixed = 0, skipped = 0;
    VScriptParser *sc = VScriptParser::NewWithLump(it.lump);
    GCon->Logf(NAME_Init, "loading SPROFS lump (%s)", *W_FullLumpName(it.lump));
    sc->SetEscape(false);
    sc->SetCMode(true);
    while (sc->GetString()) {
      int x, y;
      bool iwadonly = false;
      bool forced = false;
      int tid = -1;
      VStr textureName = sc->String.toLowerCase();
      if (!textureName.isEmpty() && textureName.length() <= 8) {
        tid = CheckNumForName(VName(*textureName), TEXTYPE_Sprite, false);
      }
      sc->Expect(",");
      sc->ExpectNumberWithSign();
      x = sc->Number;
      sc->Expect(",");
      sc->ExpectNumberWithSign();
      y = sc->Number;
      if (sc->Check(",")) {
             if (sc->Check("iwad")) iwadonly = true;
        else if (sc->Check("iwadforced")) forced = iwadonly = true;
        else { sc->ExpectString(); sc->Error(va("unexpected flag '%s'", *sc->String)); }
      }
      //GCon->Logf(NAME_Debug, " tid=%d (%d,%d) <%s>", tid, x, y, *textureName);
      if (tid > 0) {
        VTexture *tex = getIgnoreAnim(tid);
        if (!tex) { ++skipped; continue; }
        // we only want to change texture offsets for sprites in the IWAD or the file this lump originated from
        if (tex->SourceLump < 0) { ++skipped; continue; }
        // check for the same wad file
        if (it.getFile() != W_LumpFile(tex->SourceLump)) {
          if (!iwadonly) { ++skipped; continue; }
          // reject non-iwad textures
          if (!W_IsIWADLump(tex->SourceLump)) { ++skipped; continue; }
        }
        if (!forced) {
          const char *txname = *tex->Name;
          if (VStr::length(txname) >= 4) {
            vuint32 sprid;
            memcpy(&sprid, txname, 4);
            if (donotprocess.has(sprid)) { ++skipped; continue; } // do not alter sprites that only get partially replaced
          }
        }
        //GCon->Logf(NAME_Debug, "  fixed sprite '%s': old is (%d,%d); new is (%d,%d)", txname, tex->SOffset, tex->TOffset, x, y);
        tex->SOffsetFix = x;
        tex->TOffsetFix = y;
        tex->bForcedSpriteOffset = true;
        ++fixed;
      } else {
        ++skipped;
      }
    }
    delete sc;
    if (fixed || skipped) GCon->Logf(NAME_Init, "  fixed: %d; skipped: %d", fixed, skipped);
  }
}


//==========================================================================
//
//  VTextureManager::IsSkyTextureName
//
//==========================================================================
VVA_CHECKRESULT bool VTextureManager::IsSkyTextureName (VName n) noexcept {
  if (SkyFlatName != NAME_None) return (n == SkyFlatName);
  return
    n == NAME_f_sky ||
    n == NAME_f_sky001 ||
    n == NAME_f_sky1;
}


//==========================================================================
//
//  VTextureManager::SetSkyFlatName
//
//==========================================================================
void VTextureManager::SetSkyFlatName (VStr flatname) noexcept {
  if (flatname.isEmpty()) {
    SkyFlatName = NAME_None;
  } else {
    SkyFlatName = VName(*flatname, VName::AddLower8);
  }
  R_UpdateSkyFlatNum(true); // force update
  if (skyflatnum < 0) {
    // create dummy texture to use as a sky (for invalid sky textures)
    VName skyname = (SkyFlatName != NAME_None ? SkyFlatName : VName("f_sky1"));
    GCon->Logf(NAME_Warning, "cannot load sky texture '%s', creating a dummy one", *skyname);
    skyflatnum = AddFileTextureChecked("graphics/k8vavoom_special/k8vavoom_dummysky.png", TEXTYPE_Flat, skyname);
    if (skyflatnum < 0) Sys_Error("cannot load 'graphics/k8vavoom_special/k8vavoom_dummysky.png'");
    R_UpdateSkyFlatNum(true); // force update
    vassert(skyflatnum >= 0);
  }
}



//==========================================================================
//
//  R_UpdateSkyFlatNum
//
//  update sky flat number
//
//==========================================================================
void R_UpdateSkyFlatNum (bool force) {
  const int oldskf = skyflatnum;
  if (force || oldskf >= -1) {
    if (GTextureManager.SkyFlatName != NAME_None) {
      skyflatnum = GTextureManager.CheckNumForName(GTextureManager.SkyFlatName, TEXTYPE_Flat, true);
    } else {
      skyflatnum = GTextureManager.CheckNumForName(NAME_f_sky, TEXTYPE_Flat, true);
      if (skyflatnum < 0) skyflatnum = GTextureManager.CheckNumForName(NAME_f_sky001, TEXTYPE_Flat, true);
      if (skyflatnum < 0) skyflatnum = GTextureManager.CheckNumForName(NAME_f_sky1, TEXTYPE_Flat, true);
    }

    if (skyflatnum < 0) {
      if (oldskf >= 0) GCon->Logf(NAME_Warning, "OOPS! i lost the sky!");
      skyflatnum = -1; // wtf?!
    } else if (oldskf != skyflatnum && oldskf != -2) {
      //GCon->Logf(NAME_Debug, "*** NEW SKY FLAT: %d <%s>", skyflatnum, *W_FullLumpName(GTextureManager[skyflatnum]->SourceLump));
    }
    // sorry for this hack!
    SV_UpdateSkyFlat();
  }
}


// ////////////////////////////////////////////////////////////////////////// //
extern "C" {
  static int sortCmpVStrCI (const void *a, const void *b, void *udata) {
    if (a == b) return 0;
    VStr *sa = (VStr *)a;
    VStr *sb = (VStr *)b;
    return sa->ICmp(*sb);
  }
}


//==========================================================================
//
//  ExportTexture
//
//==========================================================================
COMMAND_WITH_AC(ExportTexture) {
  if (Args.length() < 2 || Args.length() > 3) {
    GCon->Log(
      "usage: ExportTexture name [type]\n"
      "where \"type\" is one of:\n"
      "  any\n"
      "  patch\n"
      "  wall\n"
      "  flat\n"
      "  sprite\n"
      "  sky\n"
      "  skin\n"
      "  pic\n"
      "  autopage\n"
      "  fontchar"
      "");
    return;
  }

  int ttype = TEXTYPE_Any;
  if (Args.length() == 3) {
    VStr tstr = Args[2];
         if (tstr.ICmp("any") == 0) ttype = TEXTYPE_Any;
    else if (tstr.ICmp("patch") == 0) ttype = TEXTYPE_WallPatch;
    else if (tstr.ICmp("wall") == 0) ttype = TEXTYPE_Wall;
    else if (tstr.ICmp("flat") == 0) ttype = TEXTYPE_Flat;
    else if (tstr.ICmp("sprite") == 0) ttype = TEXTYPE_Sprite;
    else if (tstr.ICmp("sky") == 0) ttype = TEXTYPE_SkyMap;
    else if (tstr.ICmp("skin") == 0) ttype = TEXTYPE_Skin;
    else if (tstr.ICmp("pic") == 0) ttype = TEXTYPE_Pic;
    else if (tstr.ICmp("autopage") == 0) ttype = TEXTYPE_Autopage;
    else if (tstr.ICmp("fontchar") == 0) ttype = TEXTYPE_FontChar;
    else { GCon->Logf("unknown texture type '%s'", *tstr); return; }
  }

  VTexture *tx = GTextureManager.GetExistingTextureByName(Args[1], ttype);
  if (!tx) {
    GCon->Logf(NAME_Error, "Texture '%s' not found!", *Args[1]);
    return;
  }

  // find a file name to save it to
  VStr fname = va("%s.png", *tx->Name);
  if (!FL_IsSafeDiskFileName(fname)) {
    GCon->Logf(NAME_Error, "unsafe file name '%s'", *fname);
    return;
  }

  auto strm = FL_OpenFileWrite(fname, true); // as full name
  if (!strm) {
    GCon->Logf(NAME_Error, "cannot create file '%s'", *fname);
    return;
  }

  tx->WriteToPNG(strm);
  VStream::Destroy(strm);

  GCon->Logf("Exported texture '%s' of type '%s' to '%s'", *tx->Name, VTexture::TexTypeToStr(tx->Type), *fname);
  GCon->Logf("Texture info: %dx%d; scale:(%g,%g); offset:(%d,%d)", tx->Width, tx->Height, tx->SScale, tx->TScale, tx->SOffset, tx->TOffset);
  GCon->Logf("Texture transparency: transparent=%s; translucent=%s; seethrough=%s; semitranslucent=%s", (tx->isTransparent() ? "tan" : "ona"), (tx->isTranslucent() ? "tan" : "ona"), (tx->isSeeThrough() ? "tan" : "ona"), (tx->isSemiTranslucent() ? "tan" : "ona"));
}


//==========================================================================
//
//  ExportTexture_AC
//
//==========================================================================
COMMAND_AC(ExportTexture) {
  //if (aidx != 1) return VStr::EmptyString;
  TArray<VStr> list;
  VStr prefix = (aidx < args.length() ? args[aidx] : VStr());
  if (aidx == 1) {
    GTextureManager.FillNameAutocompletion(prefix, list);
    if (!list.length()) return VStr::EmptyString;
    // sort
    timsort_r(list.ptr(), list.length(), sizeof(VStr), &sortCmpVStrCI, nullptr);
    // remove possible duplicates
    int pos = 1;
    while (pos < list.length()) {
      if (list[pos].ICmp(list[pos-1]) == 0) {
        list.removeAt(pos);
      } else {
        ++pos;
      }
    }
    return AutoCompleteFromListCmd(prefix, list);
  } else if (aidx == 2) {
    // type
    list.append(VStr("any"));
    list.append(VStr("autopage"));
    list.append(VStr("flat"));
    list.append(VStr("fontchar"));
    list.append(VStr("patch"));
    list.append(VStr("pic"));
    list.append(VStr("skin"));
    list.append(VStr("sky"));
    list.append(VStr("sprite"));
    list.append(VStr("wall"));
    return AutoCompleteFromListCmd(prefix, list);
  } else {
    return VStr::EmptyString;
  }
}
