//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
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
#include "../gamedefs.h"
#include "../server/sv_local.h"
#include "p_decal.h"
#include "p_decal_private.h"
#include "../utils/ntvalueioex.h"


// ////////////////////////////////////////////////////////////////////////// //
VCvarB r_decals("r_decals", true, "Enable decal spawning, processing and rendering?", CVAR_Archive|CVAR_NoShadow);
VCvarB r_decals_flat("r_decals_flat", true, "Enable decal spawning on floors/ceilings?", CVAR_Archive|CVAR_NoShadow);
VCvarB r_decals_wall("r_decals_wall", true, "Enable decal spawning on walls?", CVAR_Archive|CVAR_NoShadow);

static int cli_DebugDecals = 0;

/*static*/ bool cliRegister_decal_args =
  VParsedArgs::RegisterFlagSet("-debug-decals", "!show some debug info for decals", &cli_DebugDecals);


// ////////////////////////////////////////////////////////////////////////// //
// all names are lowercased
static TMapNC<VName, bool> optionalDecals;
static TMapNC<VName, bool> optionalDecalGroups;

VDecalAnim *DummyDecalAnimator = nullptr;

static bool GlobalDisableOverride = false;
static TMap<VStrCI, bool> NoOverrideNames;



//==========================================================================
//
//  IsNoOverrideName
//
//==========================================================================
static inline bool IsNoOverrideName (VStr name) noexcept {
  return NoOverrideNames.has(name);
}


//==========================================================================
//
//  AddNoOverrideName
//
//==========================================================================
static void AddNoOverrideName (VStr name) noexcept {
  if (!name.isEmpty()) NoOverrideNames.put(name, true);
}


//==========================================================================
//
//  addOptionalDecal
//
//==========================================================================
static void addOptionalDecal (VName name) noexcept {
  if (name == NAME_None) return;
  VName n(*name, VName::AddLower);
  optionalDecals.put(n, true);
}


//==========================================================================
//
//  addOptionalDecalGroup
//
//==========================================================================
static void addOptionalDecalGroup (VName name) noexcept {
  if (name == NAME_None) return;
  VName n(*name, VName::AddLower);
  optionalDecalGroups.put(n, true);
}


//==========================================================================
//
//  isOptionalDecal
//
//==========================================================================
static bool isOptionalDecal (VName name) noexcept {
  if (name == NAME_None) return true;
  VName n(*name, VName::AddLower);
  return optionalDecals.has(n);
}


//==========================================================================
//
//  isOptionalDecalGroup
//
//==========================================================================
static bool isOptionalDecalGroup (VName name) noexcept {
  if (name == NAME_None) return true;
  VName n(*name, VName::AddLower);
  return optionalDecalGroups.has(n);
}


// ////////////////////////////////////////////////////////////////////////// //
TMapNC<VName, VDecalDef *> VDecalDef::decalNameMap; // all names are lowercased
TMapNC<VName, VDecalGroup *> VDecalGroup::decalNameMap; // all names are lowercased

VDecalDef *VDecalDef::listHead = nullptr;
VDecalAnim *VDecalAnim::listHead = nullptr;
VDecalGroup *VDecalGroup::listHead = nullptr;


//==========================================================================
//
//  parseHexRGB
//
//==========================================================================
static bool parseHexRGB (VStr str, int *clr) noexcept {
  vuint32 ppc = M_ParseColor(*str);
  if (clr) *clr = ppc&0xffffff;
  return true;
}


#define ROTVEC(v_)  do { \
  const float xc = (v_).x-worldx; \
  const float yc = (v_).y-worldy; \
  const float nx = xc*c-yc*s; \
  const float ny = yc*c+xc*s; \
  (v_).x = nx+worldx; \
  (v_).y = ny+worldy; \
} while (0)

//==========================================================================
//
//  CalculateTextureBBox
//
//  calculates scaled and rotated texture bounding box in 2d space
//  scales are NOT inverted ones!
//
//  returns vertical spread height
//  zero means "no texture found"
//
//==========================================================================
float CalculateTextureBBox (float bbox2d[4], int texid, const float worldx, const float worldy, const float angle, const float scaleX, const float scaleY) noexcept {
  if (texid <= 0 || scaleX <= 0.0f || scaleY <= 0.0f) {
    memset((void *)&bbox2d[0], 0, sizeof(float)*4);
    return 0.0f;
  }

  VTexture *dtex = GTextureManager[texid];
  if (!dtex || dtex->Type == TEXTYPE_Null || dtex->GetWidth() < 1 || dtex->GetHeight() < 1) {
    memset((void *)&bbox2d[0], 0, sizeof(float)*4);
    return 0.0f;
  }

  const float twdt = dtex->GetScaledWidthF()*scaleX;
  const float thgt = dtex->GetScaledHeightF()*scaleY;

  if (twdt < 1.0f || thgt < 1.0f) {
    memset((void *)&bbox2d[0], 0, sizeof(float)*4);
    return 0.0f;
  }

  const float height = max2(2.0f, min2(twdt, thgt)*0.4f);

  const float txofs = dtex->GetScaledSOffsetF()*scaleX;
  const float tyofs = dtex->GetScaledTOffsetF()*scaleY;

  const TVec v1(worldx, worldy);
  // left-bottom
  TVec qv0 = v1+TVec(-txofs, tyofs);
  // right-bottom
  TVec qv1 = qv0+TVec(twdt, 0.0f);
  // left-top
  TVec qv2 = qv0-TVec(0.0f, thgt);
  // right-top
  TVec qv3 = qv1-TVec(0.0f, thgt);

  // now rotate it
  if (angle != 0.0f) {
    float s, c;
    msincos(AngleMod(angle), &s, &c);
    ROTVEC(qv0);
    ROTVEC(qv1);
    ROTVEC(qv2);
    ROTVEC(qv3);
  }

  bbox2d[BOX2D_MINX] = min2(min2(min2(qv0.x, qv1.x), qv2.x), qv3.x);
  bbox2d[BOX2D_MAXX] = max2(max2(max2(qv0.x, qv1.x), qv2.x), qv3.x);
  bbox2d[BOX2D_MINY] = min2(min2(min2(qv0.y, qv1.y), qv2.y), qv3.y);
  bbox2d[BOX2D_MAXY] = max2(max2(max2(qv0.y, qv1.y), qv2.y), qv3.y);

  return height;
}

#undef ROTVEC


//**************************************************************************
//
// DecalFloatVal
//
//**************************************************************************

//==========================================================================
//
//  DecalFloatVal::clone
//
//==========================================================================
DecalFloatVal DecalFloatVal::clone () noexcept {
  DecalFloatVal res(E_NoInit);
  res.type = type;
  res.rndMin = rndMin;
  res.rndMax = rndMax;
  //res.value = (type == T_Random ? FRandomBetween(rndMin, rndMax) : value);
  res.value = value;
  return res;
}


//==========================================================================
//
//  DecalFloatVal::genValue
//
//==========================================================================
void DecalFloatVal::genValue (float defval) noexcept {
  switch (type) {
    case T_Fixed:
      break;
    case T_Random:
      value = FRandomBetween(rndMin, rndMax);
      break;
    case T_Undefined:
      value = defval;
      break;
  }
}


//==========================================================================
//
//  DecalFloatVal::getMinValue
//
//==========================================================================
float DecalFloatVal::getMinValue () const noexcept {
  return (type == T_Random ? rndMin : value);
}


//==========================================================================
//
//  DecalFloatVal::getMaxValue
//
//==========================================================================
float DecalFloatVal::getMaxValue () const noexcept {
  return (type == T_Random ? rndMax : value);
}


//==========================================================================
//
//  DecalFloatVal::doIO
//
//==========================================================================
void DecalFloatVal::doIO (VStr prefix, VStream &strm, VNTValueIOEx &vio) {
  vint32 rndflag = (type == T_Random ? 1 : 0);
  vint32 tt = type;
  vio.io(VName(*(prefix+"_randomized")), rndflag);
  vio.io(VName(*(prefix+"_type")), tt);
  vio.io(VName(*(prefix+"_value")), value);
  vio.io(VName(*(prefix+"_rndmin")), rndMin);
  vio.io(VName(*(prefix+"_rndmax")), rndMax);
  if (strm.IsLoading()) type = (rndflag ? T_Random : tt);
}



//**************************************************************************
//
// VDecalDef
//
//**************************************************************************

//==========================================================================
//
//  VDecalDef::addToList
//
//==========================================================================
void VDecalDef::addToList (VDecalDef *dc) noexcept {
  if (!dc) return;
  if (dc->name == NAME_None) { delete dc; return; }
  // remove old definitions
  VDecalDef::removeFromList(VDecalDef::find(dc->name), true); // delete
  VDecalGroup::removeFromList(VDecalGroup::find(dc->name), true); // delete
  // insert new one
  dc->next = listHead;
  listHead = dc;
  // and replace name in hash
  VName xn = dc->name.GetLower();
  decalNameMap.put(xn, dc);
}


//==========================================================================
//
//  VDecalDef::removeFromList
//
//==========================================================================
void VDecalDef::removeFromList (VDecalDef *dc, bool deleteIt) noexcept {
  if (!dc) return;
  VDecalDef *prev = nullptr;
  VDecalDef *cur = listHead;
  while (cur && cur != dc) { prev = cur; cur = cur->next; }
  // remove it from list, if found
  if (cur) {
    if (prev) prev->next = cur->next; else listHead = cur->next;
  }
  // remove from hash
  VName xn = dc->name.GetLowerNoCreate();
  if (xn != NAME_None) decalNameMap.del(xn);
  // kill it
  if (deleteIt) delete dc;
}


//==========================================================================
//
//  VDecalDef::find
//
//==========================================================================
VDecalDef *VDecalDef::find (const char *aname) noexcept {
  if (!aname || !aname[0]) return nullptr;
  VName xn = VName(aname, VName::FindLower);
  if (xn == NAME_None) return nullptr;
  auto dpp = decalNameMap.get(xn);
  return (dpp ? *dpp : nullptr);
}


//==========================================================================
//
//  VDecalDef::find
//
//==========================================================================
VDecalDef *VDecalDef::find (VStr aname) noexcept {
  if (aname.isEmpty()) return nullptr;
  VName xn = VName(*aname, VName::FindLower);
  if (xn == NAME_None) return nullptr;
  auto dpp = decalNameMap.get(xn);
  return (dpp ? *dpp : nullptr);
}


//==========================================================================
//
//  VDecalDef::find
//
//==========================================================================
VDecalDef *VDecalDef::find (VName aname) noexcept {
  if (aname == NAME_None) return nullptr;
  VName xn = aname.GetLowerNoCreate();
  if (xn == NAME_None) return nullptr;
  auto dpp = decalNameMap.get(xn);
  return (dpp ? *dpp : nullptr);
  /*
  for (auto it = listHead; it; it = it->next) {
    if (it->name == aname) return it;
  }
  for (auto it = listHead; it; it = it->next) {
    if (VStr::ICmp(*it->name, *aname) == 0) return it;
  }
  return nullptr;
  */
}


//==========================================================================
//
//  VDecalDef::findById
//
//  this is used on initial map spawn only, no need to make it faster
//
//==========================================================================
VDecalDef *VDecalDef::findById (int id) noexcept {
  if (id < 0) return nullptr;
  for (auto it = listHead; it; it = it->next) {
    if (it->id == id) return it;
  }
  return nullptr;
}


//==========================================================================
//
//  VDecalDef::hasDecal
//
//==========================================================================
bool VDecalDef::hasDecal (VName aname) noexcept {
  if (VDecalDef::find(aname)) return true;
  if (VDecalGroup::find(aname)) return true;
  return false;
}


//==========================================================================
//
//  VDecalDef::getDecal
//
//==========================================================================
VDecalDef *VDecalDef::getDecal (const char *aname) noexcept {
  if (!aname || !aname[0]) return nullptr;
  VName xn = VName(aname, VName::FindLower);
  if (xn == NAME_None) return nullptr;
  return getDecal(xn);
}


//==========================================================================
//
//  VDecalDef::getDecal
//
//==========================================================================
VDecalDef *VDecalDef::getDecal (VStr aname) noexcept {
  if (aname.isEmpty()) return nullptr;
  VName xn = VName(*aname, VName::FindLower);
  if (xn == NAME_None) return nullptr;
  return getDecal(xn);
}


//==========================================================================
//
//  VDecalDef::getDecal
//
//==========================================================================
VDecalDef *VDecalDef::getDecal (VName aname) noexcept {
  aname = aname.GetLowerNoCreate();
  if (aname == NAME_None) return nullptr;
  VDecalDef *dc = VDecalDef::find(aname);
  if (!dc) {
    // try group
    VDecalGroup *gp = VDecalGroup::find(aname);
    if (!gp) return nullptr;
    dc = gp->chooseDecal();
  }
  if (dc) dc->genValues();
  return dc;
}


//==========================================================================
//
//  VDecalDef::getDecalById
//
//==========================================================================
VDecalDef *VDecalDef::getDecalById (int id) noexcept {
  return VDecalDef::findById(id);
}


//==========================================================================
//
//  VDecalDef::~VDecalDef
//
//==========================================================================
VDecalDef::~VDecalDef () noexcept {
  removeFromList(this, false); // don't delete, just remove
}


//==========================================================================
//
//  VDecalDef::genValues
//
//==========================================================================
void VDecalDef::genValues () noexcept {
  if (useCommonScale) {
    commonScale.genValue(1.0f);
    scaleX.value = scaleY.value = commonScale.value;
  } else {
    scaleX.genValue(1.0f);
    scaleY.genValue(1.0f);
  }

  switch (scaleSpecial) {
    case ScaleX_Is_Y_Multiply: scaleX.value = scaleY.value*scaleMultiply; break;
    case ScaleY_Is_X_Multiply: scaleY.value = scaleX.value*scaleMultiply; break;
    default: break;
  }

  alpha.genValue(1.0f);

       if (flipX == FlipRandom) flipXValue = GenRandomBool();
  else if (flipX == FlipAlways) flipXValue = true;
  else flipXValue = false;
  // fuck you, gshitcc
       if (flipY == FlipRandom) flipYValue = GenRandomBool();
  else if (flipY == FlipAlways) flipYValue = true;
  else flipYValue = false;

  angleWall.genValue(0.0f);
  angleWall.value = AngleMod(angleWall.value);
  angleFlat.genValue(0.0f);
  angleFlat.value = AngleMod(angleFlat.value);

  if (animator) animator->genValues();

  if (!bootprint && bootprintname != NAME_None) {
    bootprint = SV_FindBootprintByName(*bootprintname);
    if (!bootprint) {
      bootprintname = NAME_None;
    } else {
      bootanimator = bootprint->Animator;
      bootshade = (bootprint->ShadeColor >= 0 ? bootprint->ShadeColor : -2);
      if ((bootprint->ShadeColor&0xff000000) == 0xed000000) bootshade = bootprint->ShadeColor; // required
      boottranslation = -2; //bootprint->Translation;
      //bootalpha = bootprint->Alpha;
      boottime = DecalFloatVal(bootprint->TimeMin, bootprint->TimeMax);
    }
  }

  if (bootprintname == NAME_None) {
    bootanimator = NAME_None;
    bootshade = -2;
    boottranslation = -2;
    bootalpha = -1.0f;
    bootprint = nullptr;
    boottime.value = 0.0f;
  } else {
    bootprint->genValues();
    bootalpha = bootprint->AlphaValue;
    boottime.genValue(0.0f);
  }
}


//==========================================================================
//
//  VDecalDef::CalculateBBox
//
//  calculate decal bbox, return spreading height
//
//==========================================================================
void VDecalDef::CalculateBBox (const float worldx, const float worldy, const float angle) noexcept {
  float sx, sy;
  genMaxScales(&sx, &sy);
  spheight = CalculateTextureBBox(bbox2d, texid, worldx, worldy, angle, sx, sy);
}


//==========================================================================
//
//  VDecalDef::fixup
//
//==========================================================================
void VDecalDef::fixup () {
  if (animname == NAME_None) return;
  animator = VDecalAnim::find(animname);
  if (!animator) {
    if (animoptional) {
      GCon->Logf(NAME_Init, "decal '%s': ignored optional animator '%s'.", *name, *animname);
    } else {
      GCon->Logf(NAME_Warning, "decal '%s': animator '%s' not found!", *name, *animname);
    }
  } else {
    if (animator->isEmpty()) {
      animator = nullptr;
      GCon->Logf(NAME_Init, "decal '%s': removed empty animator '%s'.", *name, *animname);
    }
  }
}


//==========================================================================
//
//  VDecalDef::parseNumOrRandom
//
//==========================================================================
void VDecalDef::parseNumOrRandom (VScriptParser *sc, DecalFloatVal *value, bool withSign) {
  if (sc->Check("random")) {
    // `random min max`
    value->type = DecalFloatVal::T_Random;
    if (withSign) sc->ExpectFloatWithSign(); else sc->ExpectFloat();
    value->rndMin = sc->Float;
    if (withSign) sc->ExpectFloatWithSign(); else sc->ExpectFloat();
    value->rndMax = sc->Float;
    if (value->rndMin > value->rndMax) { const float tmp = value->rndMin; value->rndMin = value->rndMax; value->rndMax = tmp; }
    if (value->rndMin == value->rndMax) {
      value->value = value->rndMin;
      value->type = DecalFloatVal::T_Fixed;
    } else {
      value->value = FRandomBetween(value->rndMin, value->rndMax); // just in case
    }
  } else {
    // normal
    value->type = DecalFloatVal::T_Fixed;
    if (withSign) sc->ExpectFloatWithSign(); else sc->ExpectFloat();
    value->value = sc->Float;
    value->rndMin = value->rndMax = value->value; // just in case
  }
}


//==========================================================================
//
//  VDecalDef::parse
//
//  name is not parsed yet
//
//==========================================================================
bool VDecalDef::parse (VScriptParser *sc) {
  sc->SetCMode(false);
  sc->ExpectString();
  if (sc->String.Length() == 0) { sc->Error("invalid decal name"); return false; }
  name = VName(*sc->String);
  if (sc->CheckNumber()) id = sc->Number; // this is decal id

  if (sc->Check("optional")) addOptionalDecal(name);

  bool wasBloodSet = false;

  sc->Expect("{");

  VName pic = NAME_None;
  texid = -1;

  while (!sc->AtEnd()) {
    if (sc->Check("}")) {
      if (!wasBloodSet && VStr::globmatch(*name, "*blood*", false/*casesens*/)) bloodSplat = true;
      //if (!wasBootSet && VStr::globmatch(*name, "*boot*", false/*casesens*/)) bootPrint = true;

      // load texture (and shade it if necessary)
      if (pic == NAME_None) {
        if (!isOptionalDecal(name)) GCon->Logf(NAME_Warning, "decal '%s' has no pic defined", *name);
        return true;
      }
      //texid = GTextureManager./*AddPatch*/CheckNumForNameAndForce(pic, TEXTYPE_Pic, false, true);
      /*
      if (shadeclr != -1) {
        texid = GTextureManager.AddPatchShaded(pic, TEXTYPE_Pic, shadeclr, true);
        if (texid < 0 && VStr::length(*pic) > 8) {
          // try short version
          VStr pn = VStr(pic);
          VName pp = *pn.left(8);
          texid = GTextureManager.AddPatchShaded(pp, TEXTYPE_Pic, shadeclr, true);
        }
        //GCon->Logf(NAME_Init, "SHADED DECAL: texture is '%s', shade is 0x%08x", *pic, (vuint32)shadeclr);
      } else {
        texid = GTextureManager.AddPatch(pic, TEXTYPE_Pic, true);
        if (texid < 0 && VStr::length(*pic) > 8) {
          // try short version
          VStr pn = VStr(pic);
          VName pp = *pn.left(8);
          texid = GTextureManager.AddPatch(pp, TEXTYPE_Pic, true);
        }
      }
      */

      texid = GTextureManager.AddPatch(pic, TEXTYPE_Pic, true);
      if (texid < 0 && VStr::length(*pic) > 8) {
        // try short version
        VStr pn = VStr(pic);
        VName pp = *pn.left(8);
        texid = GTextureManager.AddPatch(pp, TEXTYPE_Pic, true);
      }

      if (texid < 0) {
        if (!isOptionalDecal(name)) GCon->Logf(NAME_Warning, "decal '%s' has no pic '%s'", *name, *pic);
        return true;
      }

      // has floor damage?
      hasFloorDamage =
        floorDamageTick.getMaxValue() >= 1.0f &&
        floorDamage.getMaxValue() >= 1.0f &&
        (floorDamageMonsters.getMaxValue() >= 1.0f || floorDamagePlayer.getMaxValue() >= 1.0f);

      return true;
    }

    if (sc->Check("pic")) {
      sc->ExpectName();
      pic = sc->Name;
      continue;
    }

    if (sc->Check("shade")) {
      sc->ExpectString();
      if (sc->String.ICmp("BloodDefault") == 0) {
        if (!parseHexRGB("88 00 00", &shadeclr)) { sc->Error("invalid color"); return false; }
      } else {
        if (!parseHexRGB(sc->String, &shadeclr)) { sc->Error("invalid color"); return false; }
      }
      continue;
    }

    if (sc->Check("x-scale")) {
      if (sc->Check("multiply")) {
        sc->Expect("y-scale");
        sc->ExpectFloat();
        scaleSpecial = ScaleX_Is_Y_Multiply;
        scaleMultiply = sc->Float;
      } else {
        parseNumOrRandom(sc, &scaleX);
      }
      continue;
    }

    if (sc->Check("y-scale")) {
      if (sc->Check("multiply")) {
        sc->Expect("x-scale");
        sc->ExpectFloat();
        scaleSpecial = ScaleY_Is_X_Multiply;
        scaleMultiply = sc->Float;
      } else {
        parseNumOrRandom(sc, &scaleY);
      }
      continue;
    }

    if (sc->Check("scale")) { useCommonScale = true; parseNumOrRandom(sc, &commonScale); continue; }

    if (sc->Check("flipx")) { flipX = FlipAlways; continue; }
    if (sc->Check("flipy")) { flipY = FlipAlways; continue; }

    if (sc->Check("randomflipx")) { flipX = FlipRandom; continue; }
    if (sc->Check("randomflipy")) { flipY = FlipRandom; continue; }

    if (sc->Check("wallangle")) { parseNumOrRandom(sc, &angleWall); continue; }
    if (sc->Check("flatangle")) { parseNumOrRandom(sc, &angleFlat); continue; }

    if (sc->Check("solid")) { alpha = DecalFloatVal(1.0f); additive = false; continue; }

    if (sc->Check("translucent")) { parseNumOrRandom(sc, &alpha); additive = false; continue; }
    if (sc->Check("add")) { parseNumOrRandom(sc, &alpha); additive = true; continue; }

    if (sc->Check("fuzzy")) { fuzzy = true; continue; }
    if (sc->Check("fullbright")) { fullbright = true; continue; }

    if (sc->Check("lowerdecal")) {
      sc->ExpectString();
      if (sc->String.Length() == 0) { sc->Message("invalid lower decal name"); return false; }
      lowername = VName(*sc->String);
      if (VStr::strEquCI(*lowername, "None")) lowername = NAME_None;
      continue;
    }

    // k8vavoom decal options
    if (sc->Check("k8vavoom")) {
      sc->Expect("{");
      while (!sc->Check("}")) {
        // as we cannot inherit decals, there's no need in inverse flags
        if (sc->Check("nowall")) { noWall = true; continue; }
        if (sc->Check("noflat")) { noFlat = true; continue; }

        if (sc->Check("typebloodsplat")) { bloodSplat = true; wasBloodSet = true; continue; }
        if (sc->Check("typenotbloodsplat")) { bloodSplat = false; wasBloodSet = true; continue; }

        if (sc->Check("typebootprint")) { bootPrint = true; /*wasBootSet = true;*/ continue; }
        if (sc->Check("typenotbootprint")) { bootPrint = false; /*wasBootSet = true;*/ continue; }

        if (sc->Check("bootprint")) {
          sc->ExpectString();
          if (sc->String.Length() == 0) { sc->Message("invalid bootprint name"); return false; }
          bootprintname = VName(*sc->String);
          if (VStr::strEquCI(*bootprintname, "None")) bootprintname = NAME_None;
          continue;
        }

        if (sc->Check("wallangle")) { parseNumOrRandom(sc, &angleWall); continue; }
        if (sc->Check("flatangle")) { parseNumOrRandom(sc, &angleFlat); continue; }

        if (sc->Check("floordamage")) { parseNumOrRandom(sc, &floorDamage); continue; }
        if (sc->Check("floordamagetick")) { parseNumOrRandom(sc, &floorDamageTick); continue; }
        if (sc->Check("floordamageplayer")) { parseNumOrRandom(sc, &floorDamagePlayer); continue; }
        if (sc->Check("floordamagemonsters")) { parseNumOrRandom(sc, &floorDamageMonsters); continue; }
        if (sc->Check("floordamagesuitleak")) { parseNumOrRandom(sc, &floorDamageSuitLeak); continue; }
        if (sc->Check("floordamagetype")) { sc->ExpectString(); floorDamageType = VName(*sc->String); continue; }

        sc->Error(va("unknown k8vavoom decal keyword '%s'", *sc->String));
      }
      continue;
    }

    if (sc->Check("animator")) { animoptional = false; sc->ExpectString(); animname = VName(*sc->String); continue; }
    if (sc->Check("optionalanimator")) { animoptional = true; sc->ExpectString(); animname = VName(*sc->String); continue; }

    sc->Error(va("unknown decal keyword '%s'", *sc->String));
    break;
  }

  return false;
}


//==========================================================================
//
//  VDecalDef::genMaxScale
//
//  used to generate maximum scale for animators
//  must be called after attaching animator (obviously)
//
//==========================================================================
void VDecalDef::genMaxScales (float *sx, float *sy) const noexcept {
  *sx = scaleX.value;
  *sy = scaleY.value;
  if (animator) animator->calcMaxScales(sx, sy);
}



//**************************************************************************
//
// VDecalGroup
//
//**************************************************************************

//==========================================================================
//
//  VDecalGroup::addToList
//
//==========================================================================
void VDecalGroup::addToList (VDecalGroup *dg) noexcept {
  if (!dg) return;
  if (dg->name == NAME_None) { delete dg; return; }
  // remove old definitions
  VDecalDef::removeFromList(VDecalDef::find(dg->name), true); // delete
  VDecalGroup::removeFromList(VDecalGroup::find(dg->name), true); // delete
  //GCon->Logf("new group: '%s' (%d items)", *dg->name, dg->nameList.length());
  // insert new one
  dg->next = listHead;
  listHead = dg;
  // and replace name in hash
  VName xn = dg->name.GetLower();
  decalNameMap.put(xn, dg);
}


//==========================================================================
//
//  VDecalGroup::removeFromList
//
//==========================================================================
void VDecalGroup::removeFromList (VDecalGroup *dg, bool deleteIt) noexcept {
  if (!dg) return;
  VDecalGroup *prev = nullptr;
  VDecalGroup *cur = listHead;
  while (cur && cur != dg) { prev = cur; cur = cur->next; }
  // remove it from list, if found
  if (cur) {
    if (prev) prev->next = cur->next; else listHead = cur->next;
  }
  // remove from hash
  VName xn = dg->name.GetLowerNoCreate();
  if (xn != NAME_None) decalNameMap.del(xn);
  // kill it
  if (deleteIt) delete dg;
}


//==========================================================================
//
//  VDecalGroup::find
//
//==========================================================================
VDecalGroup *VDecalGroup::find (const char *aname) noexcept {
  if (!aname || !aname[0]) return nullptr;
  VName xn = VName(aname, VName::FindLower);
  if (xn == NAME_None) return nullptr;
  auto dpp = decalNameMap.get(xn);
  return (dpp ? *dpp : nullptr);
}


//==========================================================================
//
//  VDecalGroup::find
//
//==========================================================================
VDecalGroup *VDecalGroup::find (VStr aname) noexcept {
  if (aname.isEmpty()) return nullptr;
  VName xn = VName(*aname, VName::FindLower);
  if (xn == NAME_None) return nullptr;
  auto dpp = decalNameMap.get(xn);
  return (dpp ? *dpp : nullptr);
}


//==========================================================================
//
//  VDecalGroup::find
//
//==========================================================================
VDecalGroup *VDecalGroup::find (VName aname) noexcept {
  if (aname == NAME_None) return nullptr;
  VName xn = aname.GetLowerNoCreate();
  if (xn == NAME_None) return nullptr;
  auto dpp = decalNameMap.get(xn);
  return (dpp ? *dpp : nullptr);
  /*
  for (auto it = listHead; it; it = it->next) {
    if (it->name == aname) return it;
  }
  for (auto it = listHead; it; it = it->next) {
    if (VStr::ICmp(*it->name, *aname) == 0) return it;
  }
  return nullptr;
  */
}


//==========================================================================
//
//  VDecalGroup::fixup
//
//==========================================================================
void VDecalGroup::fixup () {
  //GCon->Logf("fixing decal group '%s'", *name);
  for (int f = 0; f < nameList.length(); ++f) {
    auto it = VDecalDef::find(nameList[f].name);
    if (it) {
      auto li = new ListItem(it, nullptr);
      //GCon->Logf("  adding decal '%s' (%u)", *it->name, (unsigned)nameList[f].weight);
      list.AddEntry(li, nameList[f].weight);
      continue;
    }
    auto itg = VDecalGroup::find(nameList[f].name);
    if (itg) {
      auto li = new ListItem(nullptr, itg);
      //GCon->Logf("  adding group '%s' (%u)", *itg->name, (unsigned)nameList[f].weight);
      list.AddEntry(li, nameList[f].weight);
      continue;
    }
    if (!isOptionalDecalGroup(name) && !isOptionalDecal(nameList[f].name)) {
      GCon->Logf(NAME_Warning, "decalgroup '%s' contains unknown decal '%s'!", *name, *nameList[f].name);
    }
  }
}


//==========================================================================
//
//  VDecalGroup::chooseDecal
//
//==========================================================================
VDecalDef *VDecalGroup::chooseDecal (int reclevel) noexcept {
  if (reclevel > 64) return nullptr; // too deep
  auto li = list.PickEntry();
  if (li) {
    if (li->dd) return li->dd;
    if (li->dg) return li->dg->chooseDecal(reclevel+1);
  }
  return nullptr;
}


//==========================================================================
//
//  VDecalGroup::parse
//
//  name is not parsed yet
//
//==========================================================================
bool VDecalGroup::parse (VScriptParser *sc) {
  sc->SetCMode(false);
  sc->ExpectString();
  if (sc->String.Length() == 0) { sc->Error("invalid decal group name"); return false; }
  name = VName(*sc->String);

  if (sc->Check("optional")) addOptionalDecalGroup(name);

  sc->Expect("{");

  while (!sc->AtEnd()) {
    if (sc->Check("}")) return true;

    sc->ExpectString();
    if (sc->String.Length() == 0) { sc->Error("invalid decal name in group"); return false; }
    VName dn = VName(*sc->String);

    sc->ExpectNumber();
    if (sc->Number > 65535) sc->Number = 65535;
    nameList.Append(NameListItem(dn, sc->Number));
  }

  return false;
}



//**************************************************************************
//
// VDecalAnim
//
// base decal animator class
//
//**************************************************************************

//==========================================================================
//
//  VDecalAnim::addToList
//
//==========================================================================
void VDecalAnim::addToList (VDecalAnim *anim) noexcept {
  if (!anim) return;
  if (anim->name == NAME_None) { delete anim; return; }
  // remove old definition
  for (auto it = listHead, prev = (VDecalAnim *)nullptr; it; prev = it, it = it->next) {
    if (it->name == anim->name) {
      if (prev) prev->next = it->next; else listHead = it->next;
      delete it;
      break;
    }
  }
  // insert new one
  anim->next = listHead;
  listHead = anim;
}


//==========================================================================
//
//  VDecalAnim::removeFromList
//
//==========================================================================
void VDecalAnim::removeFromList (VDecalAnim *anim, bool deleteIt) noexcept {
  if (!anim) return;
  VDecalAnim *prev = nullptr;
  VDecalAnim *cur = listHead;
  while (cur && cur != anim) { prev = cur; cur = cur->next; }
  // remove it from list, if found
  if (cur) {
    if (prev) prev->next = cur->next; else listHead = cur->next;
  }
  // kill it
  if (deleteIt) delete anim;
}


//==========================================================================
//
//  VDecalAnim::find
//
//==========================================================================
VDecalAnim *VDecalAnim::find (const char *aname) noexcept {
  if (!aname || !aname[0]) return nullptr;
  for (auto it = listHead; it; it = it->next) {
    if (VStr::strEquCI(*it->name, aname)) return it;
  }
  return nullptr;
}


//==========================================================================
//
//  VDecalAnim::find
//
//==========================================================================
VDecalAnim *VDecalAnim::find (VStr aname) noexcept {
  if (aname.isEmpty()) return nullptr;
  return find(*aname);
}


//==========================================================================
//
//  VDecalAnim::find
//
//==========================================================================
VDecalAnim *VDecalAnim::find (VName aname) noexcept {
  if (aname == NAME_None) return nullptr;
  return find(*aname);
}


//==========================================================================
//
//  VDecalAnim::GetAnimatorByName
//
//  used by decal spawners
//
//==========================================================================
VDecalAnim *VDecalAnim::GetAnimatorByName (VName animator) noexcept {
  if (animator == NAME_None) return nullptr;
  if (VStr::strEquCI(*animator, "none")) return DummyDecalAnimator;
  VDecalAnim *res = VDecalAnim::find(animator);
  if (!res) res = DummyDecalAnimator; // oops
  return res;
}


//==========================================================================
//
//  VDecalAnim::~VDecalAnim
//
//==========================================================================
VDecalAnim::~VDecalAnim () {
  removeFromList(this, false); // don't remove
}


//==========================================================================
//
//  VDecalAnim::getTypeId
//
//==========================================================================
vuint8 VDecalAnim::getTypeId () const noexcept {
  return VDecalAnim::TypeId;
}


//==========================================================================
//
//  VDecalAnim::getCount
//
//==========================================================================
int VDecalAnim::getCount () const noexcept {
  return 1;
}


//==========================================================================
//
//  VDecalAnim::getAt
//
//==========================================================================
VDecalAnim *VDecalAnim::getAt (int idx) noexcept {
  return (idx == 0 ? this : nullptr);
}


//==========================================================================
//
//  VDecalAnim::hasTypeId
//
//==========================================================================
bool VDecalAnim::hasTypeId (vuint8 tid, int /*depth*/) const noexcept {
  return (getTypeId() == tid);
}


//==========================================================================
//
//  VDecalAnim::fixup
//
//==========================================================================
void VDecalAnim::fixup () {
}


//==========================================================================
//
//  VDecalAnim::calcWillDisappear
//
//==========================================================================
bool VDecalAnim::calcWillDisappear () const noexcept {
  return false;
}


//==========================================================================
//
//  VDecalAnim::calcMaxScales
//
//==========================================================================
void VDecalAnim::calcMaxScales (float * /*sx*/, float * /*sy*/) const noexcept {
  // nothing to do here
}


//==========================================================================
//
//  VDecalAnim::isFader
//
//==========================================================================
bool VDecalAnim::isFader () const noexcept {
  return false;
}


//==========================================================================
//
//  VDecalAnim::isStretcher
//
//==========================================================================
bool VDecalAnim::isStretcher () const noexcept {
  return false;
}


//==========================================================================
//
//  VDecalAnim::isSlider
//
//==========================================================================
bool VDecalAnim::isSlider () const noexcept {
  return false;
}


//==========================================================================
//
//  VDecalAnim::isColorChanger
//
//==========================================================================
bool VDecalAnim::isColorChanger () const noexcept {
  return false;
}


//==========================================================================
//
//  VDecalAnim::isCombiner
//
//==========================================================================
bool VDecalAnim::isCombiner () const noexcept {
  return false;
}



//**************************************************************************
//
// VDecalAnimFader
//
//**************************************************************************

//==========================================================================
//
//  VDecalAnimFader::~VDecalAnimFader
//
//==========================================================================
VDecalAnimFader::~VDecalAnimFader () {
}


//==========================================================================
//
//  VDecalAnimFader::getTypeId
//
//==========================================================================
vuint8 VDecalAnimFader::getTypeId () const noexcept {
  return VDecalAnimFader::TypeId;
}

//==========================================================================
//
//  VDecalAnimFader::getTypeName
//
//==========================================================================
const char *VDecalAnimFader::getTypeName () const noexcept {
  return "Fader";
}


//==========================================================================
//
//  VDecalAnimFader::clone
//
//==========================================================================
VDecalAnim *VDecalAnimFader::clone (bool forced) {
  if (!forced && isEmpty()) return nullptr;
  VDecalAnimFader *res = new VDecalAnimFader(E_NoInit);
  res->copyBaseFrom(this);
  res->startTime = startTime.clone();
  res->actionTime = actionTime.clone();
  return res;
}


//==========================================================================
//
//  VDecalAnimFader::doIO
//
//==========================================================================
void VDecalAnimFader::doIO (VStream &strm, VNTValueIOEx &vio) {
  vio.io(VName("time_passed"), timePassed);
  startTime.doIO("start_time", strm, vio);
  actionTime.doIO("action_time", strm, vio);
}


//==========================================================================
//
//  VDecalAnimFader::genValues
//
//==========================================================================
void VDecalAnimFader::genValues () noexcept {
  startTime.genValue(0.0f);
  actionTime.genValue(0.0f);
}


//==========================================================================
//
//  VDecalAnimFader::animate
//
//==========================================================================
bool VDecalAnimFader::animate (decal_t *decal, float timeDelta) {
  if (isEmpty()) return false; // just in case
  //GCon->Logf(NAME_Debug, "VDecalAnimFader(%p:%s):000: origAlpha=%g; alpha=%g; tp=%g; stt=%g", decal, *decal->proto->name, decal->origAlpha, decal->alpha, timePassed, startTime.value);
  if (decal->origAlpha <= 0.0f || decal->alpha <= 0.0f) return false;
  timePassed += timeDelta;
  if (timePassed < startTime.value) return true; // not yet
  if (timePassed >= startTime.value+actionTime.value || actionTime.value <= 0.0f) {
    //GCon->Logf("decal %p completely faded away", decal);
    decal->alpha = 0.0f;
    return false;
  }
  float dtx = timePassed-startTime.value;
  float aleft = decal->origAlpha;
  decal->alpha = aleft-aleft*dtx/actionTime.value;
  //GCon->Logf(NAME_Debug, "VDecalAnimFader(%p):001: origAlpha=%g; alpha=%g; dtx=%g", decal, decal->origAlpha, decal->alpha, dtx/actionTime.value);
  //GCon->Logf("decal %p: dtx=%f; origA=%f; a=%f", decal, dtx, decal->origAlpha, decal->alpha);
  return (decal->alpha > 0.0f);
}


//==========================================================================
//
//  VDecalAnimFader::calcWillDisappear
//
//==========================================================================
bool VDecalAnimFader::calcWillDisappear () const noexcept {
  return true; // fader will eventually destroy decal
}


//==========================================================================
//
//  VDecalAnimFader::isFader
//
//==========================================================================
bool VDecalAnimFader::isFader () const noexcept {
  return true;
}


//==========================================================================
//
//  VDecalAnimFader::parse
//
//==========================================================================
bool VDecalAnimFader::parse (VScriptParser *sc) {
  empty = true;
  sc->SetCMode(true);
  sc->ExpectString();
  if (sc->String.Length() == 0) { sc->Error("invalid decal fader name"); return false; }
  name = VName(*sc->String);
  sc->Expect("{");
  while (!sc->AtEnd()) {
    if (sc->Check("}")) return true;
    if (sc->Check("decaystart")) { empty = false; VDecalDef::parseNumOrRandom(sc, &startTime); continue; }
    if (sc->Check("decaytime")) { empty = false; VDecalDef::parseNumOrRandom(sc, &actionTime); continue; }
    sc->Error(va("unknown decal keyword '%s'", *sc->String));
    break;
  }
  return false;
}



//**************************************************************************
//
// VDecalAnimStretcher
//
//**************************************************************************

//==========================================================================
//
//  VDecalAnimStretcher::~VDecalAnimStretcher
//
//==========================================================================
VDecalAnimStretcher::~VDecalAnimStretcher () {
}


//==========================================================================
//
//  VDecalAnimStretcher::getTypeId
//
//==========================================================================
vuint8 VDecalAnimStretcher::getTypeId () const noexcept {
  return VDecalAnimStretcher::TypeId;
}


//==========================================================================
//
//  VDecalAnimStretcher::getTypeName
//
//==========================================================================
const char *VDecalAnimStretcher::getTypeName () const noexcept {
  return "Stretcher";
}


//==========================================================================
//
//  VDecalAnimStretcher::clone
//
//==========================================================================
VDecalAnim *VDecalAnimStretcher::clone (bool forced) {
  if (!forced && isEmpty()) return nullptr;
  VDecalAnimStretcher *res = new VDecalAnimStretcher(E_NoInit);
  res->copyBaseFrom(this);
  res->goalX = goalX.clone();
  res->goalY = goalY.clone();
  res->startTime = startTime.clone();
  res->actionTime = actionTime.clone();
  return res;
}


//==========================================================================
//
//  VDecalAnimStretcher::doIO
//
//==========================================================================
void VDecalAnimStretcher::doIO (VStream &strm, VNTValueIOEx &vio) {
  vio.io(VName("time_passed"), timePassed);
  startTime.doIO("start_time", strm, vio);
  actionTime.doIO("action_time", strm, vio);
  goalX.doIO("goal_x", strm, vio);
  goalY.doIO("goal_y", strm, vio);
}


//==========================================================================
//
//  VDecalAnimStretcher::genValues
//
//==========================================================================
void VDecalAnimStretcher::genValues () noexcept {
  goalX.genValue(0.0f);
  goalY.genValue(0.0f);
  startTime.genValue(0.0f);
  actionTime.genValue(0.0f);
}


//==========================================================================
//
//  VDecalAnimStretcher::animate
//
//==========================================================================
bool VDecalAnimStretcher::animate (decal_t *decal, float timeDelta) {
  if (isEmpty()) return false; // just in case
  if (decal->origScaleX <= 0.0f || decal->origScaleY <= 0.0f) { decal->alpha = 0.0f; return false; }
  if (decal->scaleX <= 0.0f || decal->scaleY <= 0.0f) { decal->alpha = 0.0f; return false; }
  timePassed += timeDelta;
  if (timePassed < startTime.value) return true; // not yet
  if (timePassed >= startTime.value+actionTime.value || actionTime.value <= 0.0f) {
    if (goalX.isDefined()) decal->scaleX = goalX.value;
    if (goalY.isDefined()) decal->scaleY = goalY.value;
    if (decal->scaleX <= 0.0f || decal->scaleY <= 0.0f) { decal->alpha = 0.0f; return false; }
    return false;
  }
  float dtx = timePassed-startTime.value;
  if (goalX.isDefined()) {
    float aleft = goalX.value-decal->origScaleX;
    if ((decal->scaleX = decal->origScaleX+aleft*dtx/actionTime.value) <= 0.0f) { decal->alpha = 0.0f; return false; }
  }
  if (goalY.isDefined()) {
    float aleft = goalY.value-decal->origScaleY;
    if ((decal->scaleY = decal->origScaleY+aleft*dtx/actionTime.value) <= 0.0f) { decal->alpha = 0.0f; return false; }
  }
  return true;
}


//==========================================================================
//
//  VDecalAnimStretcher::calcWillDisappear
//
//==========================================================================
bool VDecalAnimStretcher::calcWillDisappear () const noexcept {
  if (goalX.isDefined() && goalX.value <= 0.0f) return true;
  if (goalY.isDefined() && goalY.value <= 0.0f) return true;
  return false;
}


//==========================================================================
//
//  VDecalAnimStretcher::calcMaxScales
//
//==========================================================================
void VDecalAnimStretcher::calcMaxScales (float *sx, float *sy) const noexcept {
  if (goalX.isDefined()) *sx = max2(*sx, goalX.value);
  if (goalY.isDefined()) *sy = max2(*sy, goalY.value);
}


//==========================================================================
//
//  VDecalAnimStretcher::isStretcher
//
//==========================================================================
bool VDecalAnimStretcher::isStretcher () const noexcept {
  return true;
}


//==========================================================================
//
//  VDecalAnimStretcher::parse
//
//==========================================================================
bool VDecalAnimStretcher::parse (VScriptParser *sc) {
  empty = true;
  sc->SetCMode(true);
  sc->ExpectString();
  if (sc->String.Length() == 0) { sc->Error("invalid decal fader name"); return false; }
  name = VName(*sc->String);
  sc->Expect("{");
  while (!sc->AtEnd()) {
    if (sc->Check("}")) return true;
    if (sc->Check("goalx")) { VDecalDef::parseNumOrRandom(sc, &goalX); continue; }
    if (sc->Check("goaly")) { VDecalDef::parseNumOrRandom(sc, &goalY); continue; }
    if (sc->Check("stretchstart")) { empty = false; VDecalDef::parseNumOrRandom(sc, &startTime); continue; }
    if (sc->Check("stretchtime")) { empty = false; VDecalDef::parseNumOrRandom(sc, &actionTime); continue; }
    sc->Error(va("unknown decal keyword '%s'", *sc->String));
    break;
  }
  return false;
}



//**************************************************************************
//
// VDecalAnimSlider
//
//**************************************************************************

//==========================================================================
//
//  VDecalAnimSlider::~VDecalAnimSlider
//
//==========================================================================
VDecalAnimSlider::~VDecalAnimSlider () {
}


//==========================================================================
//
//  VDecalAnimSlider::getTypeId
//
//==========================================================================
vuint8 VDecalAnimSlider::getTypeId () const noexcept {
  return VDecalAnimSlider::TypeId;
}


//==========================================================================
//
//  VDecalAnimSlider::getTypeName
//
//==========================================================================
const char *VDecalAnimSlider::getTypeName () const noexcept {
  return "Slider";
}


//==========================================================================
//
//  VDecalAnimSlider::clone
//
//==========================================================================
VDecalAnim *VDecalAnimSlider::clone (bool forced) {
  if (!forced && isEmpty()) return nullptr;
  VDecalAnimSlider *res = new VDecalAnimSlider(E_NoInit);
  res->copyBaseFrom(this);
  res->distX = distX.clone();
  res->distY = distY.clone();
  res->startTime = startTime.clone();
  res->actionTime = actionTime.clone();
  res->k8reversey = k8reversey;
  return res;
}


//==========================================================================
//
//  VDecalAnimSlider::doIO
//
//==========================================================================
void VDecalAnimSlider::doIO (VStream &strm, VNTValueIOEx &vio) {
  vio.io(VName("time_passed"), timePassed);
  startTime.doIO("start_time", strm, vio);
  actionTime.doIO("action_time", strm, vio);
  distX.doIO("dist_x", strm, vio);
  distY.doIO("dist_y", strm, vio);
  vint32 revy = (k8reversey ? 1 : 0);
  vio.io(VName("reverse_y"), revy);
  if (strm.IsLoading()) k8reversey = !!revy;
}


//==========================================================================
//
//  VDecalAnimSlider::genValues
//
//==========================================================================
void VDecalAnimSlider::genValues () noexcept {
  distX.genValue(0.0f);
  distY.genValue(0.0f);
  startTime.genValue(0.0f);
  actionTime.genValue(0.0f);
}


//==========================================================================
//
//  VDecalAnimSlider::animate
//
//==========================================================================
bool VDecalAnimSlider::animate (decal_t *decal, float timeDelta) {
  if (isEmpty()) return false; // just in case
  timePassed += timeDelta;
  if (timePassed < startTime.value) return true; // not yet
  if (timePassed >= startTime.value+actionTime.value || actionTime.value <= 0.0f) {
    if (distX.isDefined()) decal->ofsX = distX.value;
    if (distY.isDefined()) decal->ofsY = distY.value*(k8reversey ? 1.0f : -1.0f);
    return false;
  }
  float dtx = timePassed-startTime.value;
  if (distX.isDefined()) decal->ofsX = distX.value*dtx/actionTime.value;
  if (distY.isDefined()) decal->ofsY = (distY.value*dtx/actionTime.value)*(k8reversey ? 1.0f : -1.0f);
  return true;
}


//==========================================================================
//
//  VDecalAnimSlider::isSlider
//
//==========================================================================
bool VDecalAnimSlider::isSlider () const noexcept {
  return true;
}


//==========================================================================
//
//  VDecalAnimSlider::parse
//
//==========================================================================
bool VDecalAnimSlider::parse (VScriptParser *sc) {
  empty = true;
  sc->SetCMode(true);
  sc->ExpectString();
  if (sc->String.Length() == 0) { sc->Error("invalid decal fader name"); return false; }
  name = VName(*sc->String);
  k8reversey = false;
  sc->Expect("{");
  while (!sc->AtEnd()) {
    if (sc->Check("}")) return true;
    if (sc->Check("distx")) { VDecalDef::parseNumOrRandom(sc, &distX, true); continue; }
    if (sc->Check("disty")) { VDecalDef::parseNumOrRandom(sc, &distY, true); continue; }
    if (sc->Check("slidestart")) { empty = false; VDecalDef::parseNumOrRandom(sc, &startTime); continue; }
    if (sc->Check("slidetime")) { empty = false; VDecalDef::parseNumOrRandom(sc, &actionTime); continue; }
    if (sc->Check("k8reversey")) { k8reversey = true; continue; }
    sc->Error(va("unknown decal keyword '%s'", *sc->String));
    break;
  }
  return false;
}



//**************************************************************************
//
// VDecalAnimColorChanger
//
//**************************************************************************

//==========================================================================
//
//  VDecalAnimColorChanger::~VDecalAnimColorChanger
//
//==========================================================================
VDecalAnimColorChanger::~VDecalAnimColorChanger () {
}


//==========================================================================
//
//  VDecalAnimColorChanger::getTypeId
//
//==========================================================================
vuint8 VDecalAnimColorChanger::getTypeId () const noexcept {
  return VDecalAnimColorChanger::TypeId;
}


//==========================================================================
//
//  VDecalAnimColorChanger::getTypeName
//
//==========================================================================
const char *VDecalAnimColorChanger::getTypeName () const noexcept {
  return "ColorChanger";
}


//==========================================================================
//
//  VDecalAnimColorChanger::clone
//
//==========================================================================
VDecalAnim *VDecalAnimColorChanger::clone (bool forced) {
  if (!forced && isEmpty()) return nullptr;
  VDecalAnimColorChanger *res = new VDecalAnimColorChanger(E_NoInit);
  res->copyBaseFrom(this);
  res->dest[0] = dest[0];
  res->dest[1] = dest[1];
  res->dest[2] = dest[2];
  res->startTime = startTime.clone();
  res->actionTime = actionTime.clone();
  return res;
}


//==========================================================================
//
//  VDecalAnimColorChanger::doIO
//
//==========================================================================
void VDecalAnimColorChanger::doIO (VStream &strm, VNTValueIOEx &vio) {
  vio.io(VName("time_passed"), timePassed);
  startTime.doIO("start_time", strm, vio);
  actionTime.doIO("action_time", strm, vio);
  vio.io(VName("dest_color_r"), dest[0]);
  vio.io(VName("dest_color_g"), dest[1]);
  vio.io(VName("dest_color_b"), dest[2]);
}


//==========================================================================
//
//  VDecalAnimColorChanger::genValues
//
//==========================================================================
void VDecalAnimColorChanger::genValues () noexcept {
  startTime.genValue(0.0f);
  actionTime.genValue(0.0f);
}


//==========================================================================
//
//  VDecalAnimColorChanger::animate
//
//==========================================================================
bool VDecalAnimColorChanger::animate (decal_t *decal, float timeDelta) {
  if (isEmpty()) return false; // just in case
  timePassed += timeDelta;
  if (timePassed < startTime.value) return true; // not yet
  if (timePassed >= startTime.value+actionTime.value || actionTime.value <= 0.0f) {
    //GCon->Logf("decal %p completely faded away", decal);
    decal->shadeclr = PackRGBf(dest[0], dest[1], dest[2])&0xffffff;
    return false;
  }
  if (decal->origshadeclr == -1) decal->origshadeclr = 0x7f7f7f; // this cannot be done yet, so choose arbitrary starting color
  float orig[3];
  UnpackRGBf(decal->origshadeclr, orig[0], orig[1], orig[2]);
  if (orig[0] == dest[0] && orig[1] == dest[1] && orig[2] == dest[2]) return false;
  bool done = true;
  const float dtx = (timePassed-startTime.value)/actionTime.value;
  for (unsigned f = 0; f < 3; ++f) {
    const float left = dest[f]-orig[f];
    float newv = dest[f]+left*dtx;
    if (fabsf(newv-dest[f]) < 0.004f) newv = dest[f]; else done = false;
    orig[f] = newv;
  }
  decal->shadeclr = PackRGBf(orig[0], orig[1], orig[2])&0xffffff;
  return !done;
}


//==========================================================================
//
//  VDecalAnimColorChanger::isColorChanger
//
//==========================================================================
bool VDecalAnimColorChanger::isColorChanger () const noexcept {
  return true;
}


//==========================================================================
//
//  VDecalAnimColorChanger::parse
//
//==========================================================================
bool VDecalAnimColorChanger::parse (VScriptParser *sc) {
  empty = true;
  sc->SetCMode(true);
  sc->ExpectString();
  if (sc->String.Length() == 0) { sc->Error("invalid decal fader name"); return false; }
  name = VName(*sc->String);
  sc->Expect("{");

  while (!sc->AtEnd()) {
    if (sc->Check("}")) return true;

    if (sc->Check("color") || sc->Check("colour")) {
      sc->ExpectString();
      int destclr = 0;
      if (!parseHexRGB(sc->String, &destclr)) { sc->Error("invalid color"); return false; }
      UnpackRGBf(destclr, dest[0], dest[1], dest[2]);
      continue;
    }
    if (sc->Check("fadestart")) { empty = false; VDecalDef::parseNumOrRandom(sc, &startTime); continue; }
    if (sc->Check("fadetime")) { empty = false; VDecalDef::parseNumOrRandom(sc, &actionTime); continue; }

    sc->Error(va("unknown decal keyword '%s'", *sc->String));
    break;
  }

  return false;
}



//**************************************************************************
//
// VDecalAnimCombiner
//
//**************************************************************************

//==========================================================================
//
//  VDecalAnimCombiner::~VDecalAnimCombiner
//
//==========================================================================
VDecalAnimCombiner::~VDecalAnimCombiner () {
  if (mIsCloned) {
    for (auto &&da : list) { delete da; da = nullptr; }
  }
  list.resetNoDtor();
}


//==========================================================================
//
//  VDecalAnimCombiner::getTypeId
//
//==========================================================================
vuint8 VDecalAnimCombiner::getTypeId () const noexcept {
  return VDecalAnimCombiner::TypeId;
}


//==========================================================================
//
//  VDecalAnimCombiner::getTypeName
//
//==========================================================================
const char *VDecalAnimCombiner::getTypeName () const noexcept {
  return "Combiner";
}


//==========================================================================
//
//  VDecalAnimCombiner::fixup
//
//==========================================================================
void VDecalAnimCombiner::fixup () {
  empty = true;
  for (VName dn : nameList) {
    auto it = VDecalAnim::find(dn);
    if (it) {
      if (!it->isEmpty()) empty = false;
      list.Append(it);
    } else {
      GCon->Logf(NAME_Warning, "animgroup '%s' contains unknown anim '%s'!", *name, *dn);
    }
  }
}


//==========================================================================
//
//  VDecalAnimCombiner::clone
//
//==========================================================================
VDecalAnim *VDecalAnimCombiner::clone (bool forced) {
  if (!forced && isEmpty()) return nullptr;
  VDecalAnimCombiner *res = new VDecalAnimCombiner(E_NoInit);
  res->copyBaseFrom(this);
  res->mIsCloned = true;
  // copy names
  res->nameList.resize(nameList.length());
  for (VName dn : nameList) res->nameList.append(dn);
  // copy animators
  res->list.resize(list.length());
  for (auto &&da : list) res->list.append(da->clone(true));
  return res;
}


//==========================================================================
//
//  VDecalAnimCombiner::doIO
//
//==========================================================================
void VDecalAnimCombiner::doIO (VStream &strm, VNTValueIOEx &vio) {
  vio.io(VName("time_passed"), timePassed);
  VStr oldpfx = vio.prefix;
  vio.prefix = "combiner h "+vio.prefix; // nested combiners will add more of this

  vint32 len = list.length();
  vio.io(VName("combiner_length"), len);
  if (len < 0 || len > 32767) {
    GCon->Logf(NAME_Warning, "Level load: invalid number of decal combiners: %d", len);
    return;
  }

  if (strm.IsLoading()) {
    // loading, alloc combiners
    mIsCloned = true; // so we'll free combiners
    list.setLength(len);
  }

  for (auto &&lit : list.itemsIdx()) {
    vio.prefix = va("combiner d%d ", lit.index())+oldpfx;
    VDecalAnim::SerialiseNested(strm, vio, lit.value());
  }

  // restore prefix
  vio.prefix = oldpfx;
}


//==========================================================================
//
//  VDecalAnimCombiner::genValues
//
//==========================================================================
void VDecalAnimCombiner::genValues () noexcept {
  for (VDecalAnim *da : list) if (da) da->genValues();
}


//==========================================================================
//
//  VDecalAnimCombiner::animate
//
//==========================================================================
bool VDecalAnimCombiner::animate (decal_t *decal, float timeDelta) {
  if (isEmpty()) return false; // just in case
  vassert(mIsCloned);
  bool res = false;
  int f = 0;
  while (f < list.length()) {
    if (list[f]->animate(decal, timeDelta)) {
      res = true;
      ++f;
    } else {
      delete list[f];
      list[f] = nullptr;
      list.removeAt(f);
      // nope; saved combiners doesn't have name list
      //nameList.removeAt(f);
    }
  }
  return res;
}


//==========================================================================
//
//  VDecalAnimCombiner::calcWillDisappear
//
//==========================================================================
bool VDecalAnimCombiner::calcWillDisappear () const noexcept {
  for (VDecalAnim *da : list) if (da && da->calcWillDisappear()) return true;
  return false;
}


//==========================================================================
//
//  VDecalAnimCombiner::calcMaxScales
//
//==========================================================================
void VDecalAnimCombiner::calcMaxScales (float *sx, float *sy) const noexcept {
  for (VDecalAnim *da : list) if (da) da->calcMaxScales(sx, sy);
}


//==========================================================================
//
//  VDecalAnimCombiner::isCombiner
//
//==========================================================================
bool VDecalAnimCombiner::isCombiner () const noexcept {
  return true;
}


//==========================================================================
//
//  VDecalAnimCombiner::getCount
//
//==========================================================================
int VDecalAnimCombiner::getCount () const noexcept {
  return list.length();
}


//==========================================================================
//
//  VDecalAnimCombiner::getAt
//
//==========================================================================
VDecalAnim *VDecalAnimCombiner::getAt (int idx) noexcept {
  if (idx >= 0 && idx < list.length()) return list[idx];
  return nullptr;
}


//==========================================================================
//
//  VDecalAnimCombiner::hasTypeId
//
//==========================================================================
bool VDecalAnimCombiner::hasTypeId (vuint8 tid, int depth) const noexcept {
  if (getTypeId() == tid) return true;
  if (depth >= 64) return false;
  for (VDecalAnim *da : list) if (da && da->hasTypeId(tid, depth+1)) return true;
  return false;
}


//==========================================================================
//
//  VDecalAnimCombiner::parse
//
//==========================================================================
bool VDecalAnimCombiner::parse (VScriptParser *sc) {
  empty = true;
  sc->SetCMode(true);
  sc->ExpectString();
  if (sc->String.Length() == 0) { sc->Error("invalid decal fader name"); return false; }
  name = VName(*sc->String);
  sc->Expect("{");

  while (!sc->AtEnd()) {
    if (sc->Check("}")) return true;

    empty = false;
    sc->ExpectString();
    if (sc->String.Length() == 0) { sc->Error("invalid animation name in group"); return false; }
    VName dn = VName(*sc->String);

    nameList.Append(dn);
  }

  return false;
}


//==========================================================================
//
//  VDecalAnim::SerialiseNested
//
//==========================================================================
void VDecalAnim::SerialiseNested (VStream &strm, VNTValueIOEx &vio, VDecalAnim *&aptr) {
  vint32 atype = 0;
  if (strm.IsLoading()) {
    vio.io(VName("animtype"), atype);
    switch (atype) {
      case 0: aptr = nullptr; return;
      case VDecalAnimFader::TypeId: aptr = new VDecalAnimFader(); break;
      case VDecalAnimStretcher::TypeId: aptr = new VDecalAnimStretcher(); break;
      case VDecalAnimSlider::TypeId: aptr = new VDecalAnimSlider(); break;
      case VDecalAnimColorChanger::TypeId: aptr = new VDecalAnimColorChanger(); break;
      case VDecalAnimCombiner::TypeId: aptr = new VDecalAnimCombiner(); break;
      default:
        GCon->Logf(NAME_Warning, "Level load: unknown decal animator type %d (this is harmless)", atype);
        return;
    }
  } else {
    // save animator
    if (aptr) atype = aptr->getTypeId();
    vio.io(VName("animtype"), atype);
    if (!aptr) return;
  }
  aptr->doIO(strm, vio);
}


//==========================================================================
//
//  VDecalAnim::Serialise
//
//  main decal serialisation code
//
//==========================================================================
void VDecalAnim::Serialise (VStream &Strm, VDecalAnim *&aptr) {
  VNTValueIOEx vio(&Strm);
  SerialiseNested(Strm, vio, aptr);
}



//**************************************************************************
//
// main parsing API
//
//**************************************************************************

//==========================================================================
//
//  ParseDecalDef
//
//==========================================================================
void ParseDecalDef (VScriptParser *sc) {
  const unsigned int MaxStack = 64;
  VScriptParser *scstack[MaxStack];
  unsigned int scsp = 0;
  bool error = false;

  sc->SetCMode(false);

  for (;;) {
    while (!sc->AtEnd()) {
      if (sc->Check("include")) {
        sc->ExpectString();
        int lmp = VScriptParser::FindIncludeLump(sc->SourceLump, sc->String);
        if (lmp >= 0) {
          if (scsp >= MaxStack) {
            sc->Error(va("decal include nesting too deep"));
            error = true;
            break;
          }
          GCon->Logf(NAME_Init, "Including '%s'...", *sc->String);
          scstack[scsp++] = sc;
          sc = VScriptParser::NewWithLump(lmp);
        } else {
          sc->Error(va("decal include '%s' not found", *sc->String));
          error = true;
          break;
        }
        continue;
      }

      //GCon->Logf("%s: \"%s\"", *sc->GetLoc().toStringNoCol(), *sc->String);
      if (sc->Check("generator")) {
        sc->ExpectString();
        VStr clsname = sc->String;
        sc->ExpectString();
        VStr decname = sc->String;
        // find class
        VClass *klass = VClass::FindClassNoCase(*clsname);
        if (klass) {
          if (developer && cli_DebugDecals > 0) GCon->Logf(NAME_Dev, "%s: class '%s': set decal '%s'", *sc->GetLoc().toStringNoCol(), klass->GetName(), *decname);
          if (IsNoOverrideName(VStr(klass->GetFieldNameValue(VName("DecalName"))))) {
            GCon->Logf(NAME_Init, "%s: skipped generator override at class '%s' due to global protection", *sc->GetLoc().toStringNoCol(), klass->GetName());
          } else {
            klass->SetFieldNameValue(VName("DecalName"), VName(*decname));
          }
          VClass *k2 = klass->GetReplacee();
          if (k2 && k2 != klass) {
            if (developer) GCon->Logf(NAME_Dev, "  repclass '%s': set decal '%s'", k2->GetName(), *decname);
            if (IsNoOverrideName(VStr(k2->GetFieldNameValue(VName("DecalName"))))) {
              GCon->Logf(NAME_Init, "%s: skipped generator override at class '%s' due to global protection", *sc->GetLoc().toStringNoCol(), k2->GetName());
            } else {
              k2->SetFieldNameValue(VName("DecalName"), VName(*decname));
            }
          }
        } else {
          GCon->Logf(NAME_Warning, "%s: ignored 'generator' definition for class '%s'", *sc->GetLoc().toStringNoCol(), *clsname);
        }
        continue;
      }

      if (sc->Check("decal")) {
        //sc->GetString();
        //GCon->Logf("%s:   DECAL \"%s\"", *sc->GetLoc().toStringNoCol(), *sc->String);
        //sc->UnGet();
        auto dc = new VDecalDef();
        if ((error = !dc->parse(sc)) == true) { delete dc; break; }
        sc->SetCMode(false);
        if (IsNoOverrideName(VStr(dc->name))) {
          GCon->Logf(NAME_Init, "%s: skipped decal override '%s' due to global protection", *sc->GetLoc().toStringNoCol(), *dc->name);
          delete dc;
          continue;
        }
        if (GlobalDisableOverride) AddNoOverrideName(VStr(dc->name));
        if (dc->texid > 0) VDecalDef::addToList(dc); else delete dc;
        continue;
      }

      if (sc->Check("decalgroup")) {
        auto dg = new VDecalGroup();
        if ((error = !dg->parse(sc)) == true) { delete dg; break; }
        sc->SetCMode(false);
        if (IsNoOverrideName(VStr(dg->name))) {
          GCon->Logf(NAME_Init, "%s: skipped decal group override '%s' due to global protection", *sc->GetLoc().toStringNoCol(), *dg->name);
          delete dg;
          continue;
        }
        if (GlobalDisableOverride) AddNoOverrideName(VStr(dg->name));
        VDecalGroup::addToList(dg);
        continue;
      }

      if (sc->Check("fader")) {
        auto ani = new VDecalAnimFader();
        if ((error = !ani->parse(sc)) == true) { delete ani; break; }
        sc->SetCMode(false);
        if (IsNoOverrideName(VStr(ani->name))) {
          GCon->Logf(NAME_Init, "%s: skipped animator override '%s' due to global protection", *sc->GetLoc().toStringNoCol(), *ani->name);
          delete ani;
          continue;
        }
        if (GlobalDisableOverride) AddNoOverrideName(VStr(ani->name));
        VDecalAnim::addToList(ani);
        continue;
      }

      if (sc->Check("stretcher")) {
        auto ani = new VDecalAnimStretcher();
        if ((error = !ani->parse(sc)) == true) { delete ani; break; }
        sc->SetCMode(false);
        if (IsNoOverrideName(VStr(ani->name))) {
          GCon->Logf(NAME_Init, "%s: skipped animator override '%s' due to global protection", *sc->GetLoc().toStringNoCol(), *ani->name);
          delete ani;
          continue;
        }
        if (GlobalDisableOverride) AddNoOverrideName(VStr(ani->name));
        VDecalAnim::addToList(ani);
        continue;
      }

      if (sc->Check("slider")) {
        auto ani = new VDecalAnimSlider();
        if ((error = !ani->parse(sc)) == true) { delete ani; break; }
        sc->SetCMode(false);
        if (IsNoOverrideName(VStr(ani->name))) {
          GCon->Logf(NAME_Init, "%s: skipped animator override '%s' due to global protection", *sc->GetLoc().toStringNoCol(), *ani->name);
          delete ani;
          continue;
        }
        if (GlobalDisableOverride) AddNoOverrideName(VStr(ani->name));
        VDecalAnim::addToList(ani);
        continue;
      }

      if (sc->Check("colorchanger")) {
        auto ani = new VDecalAnimColorChanger();
        if ((error = !ani->parse(sc)) == true) { delete ani; break; }
        sc->SetCMode(false);
        if (IsNoOverrideName(VStr(ani->name))) {
          GCon->Logf(NAME_Init, "%s: skipped animator override '%s' due to global protection", *sc->GetLoc().toStringNoCol(), *ani->name);
          delete ani;
          continue;
        }
        if (GlobalDisableOverride) AddNoOverrideName(VStr(ani->name));
        VDecalAnim::addToList(ani);
        continue;
      }

      if (sc->Check("combiner")) {
        auto ani = new VDecalAnimCombiner();
        if ((error = !ani->parse(sc)) == true) { delete ani; break; }
        sc->SetCMode(false);
        if (IsNoOverrideName(VStr(ani->name))) {
          GCon->Logf(NAME_Init, "%s: skipped animator override '%s' due to global protection", *sc->GetLoc().toStringNoCol(), *ani->name);
          delete ani;
          continue;
        }
        if (GlobalDisableOverride) AddNoOverrideName(VStr(ani->name));
        VDecalAnim::addToList(ani);
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

      sc->Error(va("Invalid command %s", *sc->String));
      error = true;
      break;
    }
    //GCon->Logf(NAME_Init, "DONE WITH '%s'", *sc->GetLoc().GetSourceFile());

    if (error) {
      while (scsp > 0) { delete sc; sc = scstack[--scsp]; }
      break;
    }
    if (scsp == 0) break;
    GCon->Logf(NAME_Init, "Finished included '%s'", *sc->GetLoc().GetSourceFile());
    delete sc;
    sc = scstack[--scsp];
  }

  delete sc;
}


//==========================================================================
//
//  ProcessDecalDefs
//
//==========================================================================
void ProcessDecalDefs () {
  GCon->Logf(NAME_Init, "Parsing DECAL definitions");

  DummyDecalAnimator = new VDecalAnimFader();

  for (auto &&it : WadNSNameIterator(NAME_decaldef, WADNS_Global)) {
    GlobalDisableOverride = false;
    const int Lump = it.lump;
    GCon->Logf(NAME_Init, "Parsing decal definition script '%s'...", *W_FullLumpName(Lump));
    ParseDecalDef(VScriptParser::NewWithLump(Lump));
  }

  for (auto it = VDecalGroup::listHead; it; it = it->next) it->fixup();
  for (auto it = VDecalAnim::listHead; it; it = it->next) it->fixup();
  for (auto it = VDecalDef::listHead; it; it = it->next) it->fixup();

  optionalDecals.clear();
  optionalDecalGroups.clear();
  NoOverrideNames.clear();
}
