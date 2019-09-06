//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
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
#include "net/network.h"
#include "sv_local.h"


// ////////////////////////////////////////////////////////////////////////// //
VCvarB r_decals_enabled("r_decals_enabled", true, "Enable decal spawning, processing and rendering?", CVAR_Archive);

static int cli_DebugDecals = 0;

/*static*/ bool cliRegister_decal_args =
  VParsedArgs::RegisterFlagSet("-debug-decals", "!show some debug info for decals", &cli_DebugDecals);


// ////////////////////////////////////////////////////////////////////////// //
// all names are lowercased
static TMapNC<VName, bool> optionalDecals;
static TMapNC<VName, bool> optionalDecalGroups;


static void addOptionalDecal (VName name) {
  if (name == NAME_None) return;
  VName n(*name, VName::AddLower);
  optionalDecals.put(n, true);
}

static void addOptionalDecalGroup (VName name) {
  if (name == NAME_None) return;
  VName n(*name, VName::AddLower);
  optionalDecalGroups.put(n, true);
}


static bool isOptionalDecal (VName name) {
  if (name == NAME_None) return true;
  VName n(*name, VName::AddLower);
  return optionalDecals.has(n);
}


static bool isOptionalDecalGroup (VName name) {
  if (name == NAME_None) return true;
  VName n(*name, VName::AddLower);
  return optionalDecalGroups.has(n);
}


// ////////////////////////////////////////////////////////////////////////// //
VDecalDef *VDecalDef::listHead = nullptr;
VDecalAnim *VDecalAnim::listHead = nullptr;
VDecalGroup *VDecalGroup::listHead = nullptr;


// ////////////////////////////////////////////////////////////////////////// //
static bool parseHexRGB (VStr str, int *clr) {
  vuint32 ppc = M_ParseColor(*str);
  if (clr) *clr = ppc&0xffffff;
  return true;
}


// ////////////////////////////////////////////////////////////////////////// //
DecalFloatVal DecalFloatVal::clone () {
  DecalFloatVal res;
  res.value = (rnd ? RandomBetween(rndMin, rndMax) : value);
  return res;
}


void DecalFloatVal::genValue () {
  if (rnd) value = RandomBetween(rndMin, rndMax);
}


void DecalFloatVal::doIO (VStr prefix, VStream &strm, VNTValueIOEx &vio) {
  vint32 rndflag = (rnd ? 1 : 0);
  vio.io(VName(*(prefix+"_randomized")), rndflag);
  vio.io(VName(*(prefix+"_value")), value);
  vio.io(VName(*(prefix+"_rndmin")), rndMin);
  vio.io(VName(*(prefix+"_rndmax")), rndMax);
  if (strm.IsLoading()) rnd = !!rndflag;
}


// ////////////////////////////////////////////////////////////////////////// //
void VDecalDef::addToList (VDecalDef *dc) {
  if (!dc) return;
  if (dc->name == NAME_None) { delete dc; return; }
  // remove old definitions
  //FIXME: memory leak
  VDecalDef::removeFromList(VDecalDef::find(dc->name));
  VDecalGroup::removeFromList(VDecalGroup::find(dc->name));
  // insert new one
  dc->next = listHead;
  listHead = dc;
}


void VDecalDef::removeFromList (VDecalDef *dc) {
  VDecalDef *prev = nullptr;
  VDecalDef *cur = listHead;
  while (cur && cur != dc) { prev = cur; cur = cur->next; }
  // remove it from list, if found
  if (cur) {
    if (prev) prev->next = cur->next; else listHead = cur->next;
  }
}


VDecalDef *VDecalDef::find (VStr aname) {
  VName xn = VName(*aname, VName::Find);
  if (xn == NAME_None) return nullptr;
  return find(xn);
}

VDecalDef *VDecalDef::find (VName aname) {
  for (auto it = listHead; it; it = it->next) {
    if (it->name == aname) return it;
  }
  for (auto it = listHead; it; it = it->next) {
    if (VStr::ICmp(*it->name, *aname) == 0) return it;
  }
  return nullptr;
}


VDecalDef *VDecalDef::findById (int id) {
  if (id < 0) return nullptr;
  for (auto it = listHead; it; it = it->next) {
    if (it->id == id) return it;
  }
  return nullptr;
}


bool VDecalDef::hasDecal (VName aname) {
  if (VDecalDef::find(aname)) return true;
  if (VDecalGroup::find(aname)) return true;
  return false;
}


VDecalDef *VDecalDef::getDecal (VStr aname) {
  VName xn = VName(*aname, VName::Find);
  if (xn == NAME_None) return nullptr;
  return getDecal(xn);
}


VDecalDef *VDecalDef::getDecal (VName aname) {
  VDecalDef *dc = VDecalDef::find(aname);
  if (dc) return dc;
  // try group
  VDecalGroup *gp = VDecalGroup::find(aname);
  if (!gp) return nullptr;
  return gp->chooseDecal();
}


VDecalDef *VDecalDef::getDecalById (int id) {
  return VDecalDef::findById(id);
}


// ////////////////////////////////////////////////////////////////////////// //
VDecalDef::~VDecalDef () {
  removeFromList(this);
}


void VDecalDef::genValues () {
  if (useCommonScale) {
    commonScale.genValue();
    scaleX.value = scaleY.value = commonScale.value;
  } else {
    scaleX.genValue();
    scaleY.genValue();
  }

  switch (scaleSpecial) {
    case ScaleX_Is_Y_Multiply: scaleX.value = scaleY.value*scaleMultiply; break;
    case ScaleY_Is_X_Multiply: scaleY.value = scaleX.value*scaleMultiply; break;
    default: break;
  }

  alpha.genValue();
  addAlpha.genValue();
}


void VDecalDef::fixup () {
  if (animname == NAME_None) return;
  animator = VDecalAnim::find(animname);
  if (!animator) GCon->Logf(NAME_Warning, "decal '%s': animator '%s' not found!", *name, *animname);
}


void VDecalDef::parseNumOrRandom (VScriptParser *sc, DecalFloatVal *value, bool withSign) {
  if (sc->Check("random")) {
    // `random(min, max)`
    value->rnd = true;
    if (withSign) sc->ExpectFloatWithSign(); else sc->ExpectFloat();
    value->rndMin = sc->Float;
    if (withSign) sc->ExpectFloatWithSign(); else sc->ExpectFloat();
    value->rndMax = sc->Float;
    if (value->rndMin > value->rndMax) { const float tmp = value->rndMin; value->rndMin = value->rndMax; value->rndMax = tmp; }
    if (value->rndMin == value->rndMax) {
      value->value = value->rndMin;
      value->rnd = false;
    } else {
      value->value = RandomBetween(value->rndMin, value->rndMax); // just in case
    }
  } else {
    // normal
    value->rnd = false;
    if (withSign) sc->ExpectFloatWithSign(); else sc->ExpectFloat();
    value->value = sc->Float;
    value->rndMin = value->rndMax = value->value; // just in case
  }
}


// name is not parsed yet
bool VDecalDef::parse (VScriptParser *sc) {
  sc->SetCMode(false);
  sc->ExpectString();
  if (sc->String.Length() == 0) { sc->Error("invalid decal name"); return false; }
  name = VName(*sc->String);
  if (sc->CheckNumber()) id = sc->Number; // this is decal id

  if (sc->Check("optional")) addOptionalDecal(name);

  sc->Expect("{");

  VName pic = NAME_None;
  texid = -1;
  int shadeclr = -1;

  while (!sc->AtEnd()) {
    if (sc->Check("}")) {
      // load texture (and shade it if necessary)
      if (pic == NAME_None) {
        if (!isOptionalDecal(name)) GCon->Logf(NAME_Warning, "decal '%s' has no pic defined", *name);
        return true;
      }
      //texid = GTextureManager./*AddPatch*/CheckNumForNameAndForce(pic, TEXTYPE_Pic, false, true);
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
      if (texid < 0) {
        if (!isOptionalDecal(name)) GCon->Logf(NAME_Warning, "decal '%s' has no pic '%s'", *name, *pic);
        return true;
      }
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

    if (sc->Check("solid")) { alpha = 1; continue; }

    if (sc->Check("translucent")) { parseNumOrRandom(sc, &alpha); continue; }
    if (sc->Check("add")) { parseNumOrRandom(sc, &addAlpha); continue; }

    if (sc->Check("fuzzy")) { fuzzy = true; continue; }
    if (sc->Check("fullbright")) { fullbright = true; continue; }

    if (sc->Check("lowerdecal")) {
      sc->ExpectString();
      if (sc->String.Length() == 0) { sc->Error("invalid lower decal name"); return false; }
      lowername = VName(*sc->String);
      continue;
    }

    if (sc->Check("animator")) { sc->ExpectString(); animname = VName(*sc->String); continue; }

    sc->Error(va("unknown decal keyword '%s'", *sc->String));
    break;
  }

  return false;
}


// ////////////////////////////////////////////////////////////////////////// //
void VDecalGroup::addToList (VDecalGroup *dg) {
  if (!dg) return;
  if (dg->name == NAME_None) { delete dg; return; }
  // remove old definitions
  //FIXME: memory leak
  //if (VDecalDef::find(dg->name)) GCon->Logf("replaced decal '%s'...", *dg->name);
  VDecalDef::removeFromList(VDecalDef::find(dg->name));
  //if (VDecalGroup::find(dg->name)) GCon->Logf("replaced group '%s'...", *dg->name);
  VDecalGroup::removeFromList(VDecalGroup::find(dg->name));
  //GCon->Logf("new group: '%s' (%d items)", *dg->name, dg->nameList.Num());
  // insert new one
  dg->next = listHead;
  listHead = dg;
}


void VDecalGroup::removeFromList (VDecalGroup *dg) {
  VDecalGroup *prev = nullptr;
  VDecalGroup *cur = listHead;
  while (cur && cur != dg) { prev = cur; cur = cur->next; }
  // remove it from list, if found
  if (cur) {
    if (prev) prev->next = cur->next; else listHead = cur->next;
  }
}


VDecalGroup *VDecalGroup::find (VStr aname) {
  VName xn = VName(*aname, VName::Find);
  if (xn == NAME_None) return nullptr;
  return find(xn);
}

VDecalGroup *VDecalGroup::find (VName aname) {
  for (auto it = listHead; it; it = it->next) {
    if (it->name == aname) return it;
  }
  for (auto it = listHead; it; it = it->next) {
    if (VStr::ICmp(*it->name, *aname) == 0) return it;
  }
  return nullptr;
}


// ////////////////////////////////////////////////////////////////////////// //
void VDecalGroup::fixup () {
  //GCon->Logf("fixing decal group '%s'...", *name);
  for (int f = 0; f < nameList.Num(); ++f) {
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


VDecalDef *VDecalGroup::chooseDecal (int reclevel) {
  if (reclevel > 64) return nullptr; // too deep
  auto li = list.PickEntry();
  if (li) {
    if (li->dd) return li->dd;
    if (li->dg) return li->dg->chooseDecal(reclevel+1);
  }
  return nullptr;
}


// name is not parsed yet
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


// ////////////////////////////////////////////////////////////////////////// //
void VDecalAnim::addToList (VDecalAnim *anim) {
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


void VDecalAnim::removeFromList (VDecalAnim *anim) {
  VDecalAnim *prev = nullptr;
  VDecalAnim *cur = listHead;
  while (cur && cur != anim) { prev = cur; cur = cur->next; }
  // remove it from list, if found
  if (cur) {
    if (prev) prev->next = cur->next; else listHead = cur->next;
  }
}


VDecalAnim *VDecalAnim::find (VStr aname) {
  VName xn = VName(*aname, VName::Find);
  if (xn == NAME_None) return nullptr;
  return find(xn);
}

VDecalAnim *VDecalAnim::find (VName aname) {
  for (auto it = listHead; it; it = it->next) {
    if (it->name == aname) return it;
  }
  for (auto it = listHead; it; it = it->next) {
    if (VStr::ICmp(*it->name, *aname) == 0) return it;
  }
  return nullptr;
}


// ////////////////////////////////////////////////////////////////////////// //
// base decal animator class
VDecalAnim::~VDecalAnim () {
  removeFromList(this);
}


void VDecalAnim::fixup () {
}


// ////////////////////////////////////////////////////////////////////////// //
VDecalAnimFader::~VDecalAnimFader () {
}


VDecalAnim *VDecalAnimFader::clone () {
  VDecalAnimFader *res = new VDecalAnimFader();
  res->name = name;
  res->startTime = startTime.clone();
  res->actionTime = actionTime.clone();
  res->timePassed = timePassed;
  return res;
}


void VDecalAnimFader::doIO (VStream &strm, VNTValueIOEx &vio) {
  vio.io(VName("time_passed"), timePassed);
  startTime.doIO("start_time", strm, vio);
  actionTime.doIO("action_time", strm, vio);
}


bool VDecalAnimFader::animate (decal_t *decal, float timeDelta) {
  if (decal->origAlpha <= 0 || decal->alpha <= 0) return false;
  timePassed += timeDelta;
  if (timePassed < startTime.value) return true; // not yet
  if (timePassed >= startTime.value+actionTime.value || actionTime.value <= 0) {
    //GCon->Logf("decal %p completely faded away", decal);
    decal->alpha = 0;
    return false;
  }
  float dtx = timePassed-startTime.value;
  float aleft = decal->origAlpha;
  decal->alpha = aleft-aleft*dtx/actionTime.value;
  //GCon->Logf("decal %p: dtx=%f; origA=%f; a=%f", decal, dtx, decal->origAlpha, decal->alpha);
  return (decal->alpha > 0);
}


bool VDecalAnimFader::parse (VScriptParser *sc) {
  sc->SetCMode(true);
  sc->ExpectString();
  if (sc->String.Length() == 0) { sc->Error("invalid decal fader name"); return false; }
  name = VName(*sc->String);
  sc->Expect("{");
  while (!sc->AtEnd()) {
    if (sc->Check("}")) return true;
    if (sc->Check("decaystart")) { VDecalDef::parseNumOrRandom(sc, &startTime); continue; }
    if (sc->Check("decaytime")) { VDecalDef::parseNumOrRandom(sc, &actionTime); continue; }
    sc->Error(va("unknown decal keyword '%s'", *sc->String));
    break;
  }
  return false;
}



// ////////////////////////////////////////////////////////////////////////// //
VDecalAnimStretcher::~VDecalAnimStretcher () {
}


VDecalAnim *VDecalAnimStretcher::clone () {
  VDecalAnimStretcher *res = new VDecalAnimStretcher();
  res->name = name;
  res->goalX = goalX.clone();
  res->goalY = goalY.clone();
  res->startTime = startTime.clone();
  res->actionTime = actionTime.clone();
  res->timePassed = timePassed;
  return res;
}


void VDecalAnimStretcher::doIO (VStream &strm, VNTValueIOEx &vio) {
  vio.io(VName("time_passed"), timePassed);
  startTime.doIO("start_time", strm, vio);
  actionTime.doIO("action_time", strm, vio);
  goalX.doIO("goal_x", strm, vio);
  goalY.doIO("goal_y", strm, vio);
}


bool VDecalAnimStretcher::animate (decal_t *decal, float timeDelta) {
  if (decal->origScaleX <= 0 || decal->origScaleY <= 0) { decal->alpha = 0; return false; }
  if (decal->scaleX <= 0 || decal->scaleY <= 0) { decal->alpha = 0; return false; }
  timePassed += timeDelta;
  if (timePassed < startTime.value) return true; // not yet
  if (timePassed >= startTime.value+actionTime.value || actionTime.value <= 0) {
    if ((decal->scaleX = goalX.value) <= 0) { decal->alpha = 0; return false; }
    if ((decal->scaleY = goalY.value) <= 0) { decal->alpha = 0; return false; }
    return false;
  }
  float dtx = timePassed-startTime.value;
  {
    float aleft = goalX.value-decal->origScaleX;
    if ((decal->scaleX = decal->origScaleX+aleft*dtx/actionTime.value) <= 0) { decal->alpha = 0; return false; }
  }
  {
    float aleft = goalY.value-decal->origScaleY;
    if ((decal->scaleY = decal->origScaleY+aleft*dtx/actionTime.value) <= 0) { decal->alpha = 0; return false; }
  }
  return true;
}


bool VDecalAnimStretcher::parse (VScriptParser *sc) {
  sc->SetCMode(true);
  sc->ExpectString();
  if (sc->String.Length() == 0) { sc->Error("invalid decal fader name"); return false; }
  name = VName(*sc->String);
  sc->Expect("{");
  while (!sc->AtEnd()) {
    if (sc->Check("}")) return true;
    if (sc->Check("goalx")) { VDecalDef::parseNumOrRandom(sc, &goalX); continue; }
    if (sc->Check("goaly")) { VDecalDef::parseNumOrRandom(sc, &goalY); continue; }
    if (sc->Check("stretchstart")) { VDecalDef::parseNumOrRandom(sc, &startTime); continue; }
    if (sc->Check("stretchtime")) { VDecalDef::parseNumOrRandom(sc, &actionTime); continue; }
    sc->Error(va("unknown decal keyword '%s'", *sc->String));
    break;
  }
  return false;
}


// ////////////////////////////////////////////////////////////////////////// //
VDecalAnimSlider::~VDecalAnimSlider () {
}


VDecalAnim *VDecalAnimSlider::clone () {
  VDecalAnimSlider *res = new VDecalAnimSlider();
  res->name = name;
  res->distX = distX.clone();
  res->distY = distY.clone();
  res->startTime = startTime.clone();
  res->actionTime = actionTime.clone();
  res->timePassed = timePassed;
  return res;
}


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


bool VDecalAnimSlider::animate (decal_t *decal, float timeDelta) {
  timePassed += timeDelta;
  if (timePassed < startTime.value) return true; // not yet
  if (timePassed >= startTime.value+actionTime.value || actionTime.value <= 0) {
    decal->ofsX = distX.value;
    decal->ofsY = distY.value*(k8reversey ? 1.0f : -1.0f);
    return false;
  }
  float dtx = timePassed-startTime.value;
  decal->ofsX = distX.value*dtx/actionTime.value;
  decal->ofsY = (distY.value*dtx/actionTime.value)*(k8reversey ? 1.0f : -1.0f);
  return true;
}


bool VDecalAnimSlider::parse (VScriptParser *sc) {
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
    if (sc->Check("slidestart")) { VDecalDef::parseNumOrRandom(sc, &startTime); continue; }
    if (sc->Check("slidetime")) { VDecalDef::parseNumOrRandom(sc, &actionTime); continue; }
    if (sc->Check("k8reversey")) { k8reversey = true; continue; }
    sc->Error(va("unknown decal keyword '%s'", *sc->String));
    break;
  }
  return false;
}


// ////////////////////////////////////////////////////////////////////////// //
VDecalAnimColorChanger::~VDecalAnimColorChanger () {
}


VDecalAnim *VDecalAnimColorChanger::clone () {
  VDecalAnimColorChanger *res = new VDecalAnimColorChanger();
  res->name = name;
  res->dest[0] = dest[0];
  res->dest[1] = dest[1];
  res->dest[2] = dest[2];
  res->startTime = startTime.clone();
  res->actionTime = actionTime.clone();
  res->timePassed = timePassed;
  return res;
}


void VDecalAnimColorChanger::doIO (VStream &strm, VNTValueIOEx &vio) {
  vio.io(VName("time_passed"), timePassed);
  startTime.doIO("start_time", strm, vio);
  actionTime.doIO("action_time", strm, vio);
  vio.io(VName("dest_color_r"), dest[0]);
  vio.io(VName("dest_color_g"), dest[1]);
  vio.io(VName("dest_color_b"), dest[2]);
}


bool VDecalAnimColorChanger::animate (decal_t *decal, float timeDelta) {
  // not yet, sorry
  // as we are using pre-translated textures, color changer cannot work
  // and we need pre-translated textures for working colormaps
  return true;
}


bool VDecalAnimColorChanger::parse (VScriptParser *sc) {
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
      dest[0] = ((destclr>>16)&0xff)/255.0f;
      dest[1] = ((destclr>>8)&0xff)/255.0f;
      dest[2] = (destclr&0xff)/255.0f;
      continue;
    }
    if (sc->Check("fadestart")) { VDecalDef::parseNumOrRandom(sc, &startTime); continue; }
    if (sc->Check("fadetime")) { VDecalDef::parseNumOrRandom(sc, &actionTime); continue; }

    sc->Error(va("unknown decal keyword '%s'", *sc->String));
    break;
  }

  return false;
}


// ////////////////////////////////////////////////////////////////////////// //
VDecalAnimCombiner::~VDecalAnimCombiner () {
  if (mIsCloned) {
    for (int f = 0; f < list.Num(); ++f) { delete list[f]; list[f] = nullptr; }
  }
}


void VDecalAnimCombiner::fixup () {
  for (int f = 0; f < nameList.Num(); ++f) {
    auto it = VDecalAnim::find(nameList[f]);
    if (it) list.Append(it); else GCon->Logf(NAME_Warning, "animgroup '%s' contains unknown anim '%s'!", *name, *nameList[f]);
  }
}


VDecalAnim *VDecalAnimCombiner::clone () {
  VDecalAnimCombiner *res = new VDecalAnimCombiner();
  res->name = name;
  res->mIsCloned = true;
  for (int f = 0; f < nameList.Num(); ++f) res->nameList.Append(nameList[f]);
  for (int f = 0; f < list.Num(); ++f) res->list.Append(list[f]->clone());
  res->timePassed = timePassed; // why not?
  return res;
}


void VDecalAnimCombiner::doIO (VStream &strm, VNTValueIOEx &vio) {
  /*
  Strm << timePassed;
  int len = 0;
  if (Strm.IsLoading()) {
    Strm << len;
    if (len < 0 || len > 65535) Host_Error("Level load: invalid number of animations in animcombiner");
    list.SetNum(len);
    for (int f = 0; f < list.Num(); ++f) list[f] = nullptr;
  } else {
    len = list.Num();
    Strm << len;
  }
  for (int f = 0; f < list.Num(); ++f) VDecalAnim::Serialise(Strm, list[f]);
  */
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
    list.setLength(len);
  }

  for (auto &&lit : list.itemsIdx()) {
    vio.prefix = va("combiner d%d ", lit.index())+oldpfx;
    VDecalAnim::SerialiseNested(strm, vio, lit.value());
  }

  // restore prefix
  vio.prefix = oldpfx;
}


bool VDecalAnimCombiner::animate (decal_t *decal, float timeDelta) {
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
    }
  }
  return res;
}


bool VDecalAnimCombiner::parse (VScriptParser *sc) {
  sc->SetCMode(true);
  sc->ExpectString();
  if (sc->String.Length() == 0) { sc->Error("invalid decal fader name"); return false; }
  name = VName(*sc->String);
  sc->Expect("{");

  while (!sc->AtEnd()) {
    if (sc->Check("}")) return true;

    sc->ExpectString();
    if (sc->String.Length() == 0) { sc->Error("invalid animation name in group"); return false; }
    VName dn = VName(*sc->String);

    nameList.Append(dn);
  }

  return false;
}


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


// main decal serialisation code
void VDecalAnim::Serialise (VStream &Strm, VDecalAnim *&aptr) {
  VNTValueIOEx vio(&Strm);
  SerialiseNested(Strm, vio, aptr);
}


// ////////////////////////////////////////////////////////////////////////// //
static void SetClassFieldName (VClass *Class, VName FieldName, VName Value) {
  VField *F = Class->FindFieldChecked(FieldName);
  F->SetNameValue((VObject *)Class->Defaults, Value);
}


// ////////////////////////////////////////////////////////////////////////// //
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
        int lmp = W_CheckNumForFileName(sc->String);
        if (lmp >= 0) {
          if (scsp >= MaxStack) {
            sc->Error(va("decal include nesting too deep"));
            error = true;
            break;
          }
          GCon->Logf(NAME_Init, "Including '%s'...", *sc->String);
          scstack[scsp++] = sc;
          sc = new VScriptParser(/**sc->String*/W_FullLumpName(lmp), W_CreateLumpReaderNum(lmp));
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
          SetClassFieldName(klass, VName("DecalName"), VName(*decname));
          VClass *k2 = klass->GetReplacee();
          if (k2 && k2 != klass) {
            if (developer) GCon->Logf(NAME_Dev, "  repclass '%s': set decal '%s'", k2->GetName(), *decname);
            SetClassFieldName(k2, VName("DecalName"), VName(*decname));
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
        if (dc->texid > 0) VDecalDef::addToList(dc); else delete dc;
        continue;
      }

      if (sc->Check("decalgroup")) {
        auto dg = new VDecalGroup();
        if ((error = !dg->parse(sc)) == true) { delete dg; break; }
        sc->SetCMode(false);
        VDecalGroup::addToList(dg);
        continue;
      }

      if (sc->Check("fader")) {
        auto ani = new VDecalAnimFader();
        if ((error = !ani->parse(sc)) == true) { delete ani; break; }
        sc->SetCMode(false);
        VDecalAnim::addToList(ani);
        continue;
      }

      if (sc->Check("stretcher")) {
        auto ani = new VDecalAnimStretcher();
        if ((error = !ani->parse(sc)) == true) { delete ani; break; }
        sc->SetCMode(false);
        VDecalAnim::addToList(ani);
        continue;
      }

      if (sc->Check("slider")) {
        auto ani = new VDecalAnimSlider();
        if ((error = !ani->parse(sc)) == true) { delete ani; break; }
        sc->SetCMode(false);
        VDecalAnim::addToList(ani);
        continue;
      }

      if (sc->Check("colorchanger")) {
        auto ani = new VDecalAnimColorChanger();
        if ((error = !ani->parse(sc)) == true) { delete ani; break; }
        sc->SetCMode(false);
        VDecalAnim::addToList(ani);
        continue;
      }

      if (sc->Check("combiner")) {
        auto ani = new VDecalAnimCombiner();
        if ((error = !ani->parse(sc)) == true) { delete ani; break; }
        sc->SetCMode(false);
        VDecalAnim::addToList(ani);
        continue;
      }

      sc->Error(va("Invalid command %s", *sc->String));
      error = true;
      break;
    }
    //GCon->Logf(NAME_Init, "DONE WITH '%s'", *sc->GetLoc().GetSource());

    if (error) {
      while (scsp > 0) { delete sc; sc = scstack[--scsp]; }
      break;
    }
    if (scsp == 0) break;
    GCon->Logf(NAME_Init, "Finished included '%s'", *sc->GetLoc().GetSource());
    delete sc;
    sc = scstack[--scsp];
  }

  delete sc;
}


// ////////////////////////////////////////////////////////////////////////// //
void ProcessDecalDefs () {
  GCon->Logf(NAME_Init, "Parsing DECAL definitions");

  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    //fprintf(stderr, "<%s>\n", *W_LumpName(Lump));
    if (W_LumpName(Lump) == NAME_decaldef) {
      GCon->Logf(NAME_Init, "Parsing decal definition script '%s'...", *W_FullLumpName(Lump));
      ParseDecalDef(new VScriptParser(W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump)));
    }
  }

  for (auto it = VDecalGroup::listHead; it; it = it->next) it->fixup();
  for (auto it = VDecalAnim::listHead; it; it = it->next) it->fixup();
  for (auto it = VDecalDef::listHead; it; it = it->next) it->fixup();

  optionalDecals.clear();
  optionalDecalGroups.clear();

  //!TLocation::ClearSourceFiles();
}
