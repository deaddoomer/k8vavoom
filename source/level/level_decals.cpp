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
#include "../gamedefs.h"

//#define VAVOOM_DECALS_DEBUG_REPLACE_PICTURE
//#define VAVOOM_DECALS_DEBUG

#ifdef VAVOOM_DECALS_DEBUG
# define VDC_DLOG  GCon->Logf
#else
# define VDC_DLOG(...)  do {} while (0)
#endif


extern VCvarB r_decals;
extern VCvarB r_decals_flat;
extern VCvarB r_decals_wall;

static VCvarB r_decal_switch_special("r_decal_switch_special", true, "Make decals more translucent on switch textures?", CVAR_Archive);
static VCvarF r_decal_switch_blood_alpha("r_decal_switch_blood_alpha", "0.36", "Force this transparency for blood decals on switches.", CVAR_Archive);
static VCvarF r_decal_switch_boot_alpha("r_decal_switch_boot_alpha", "0.36", "Force this transparency for boot decals on switches.", CVAR_Archive);
static VCvarF r_decal_switch_other_alpha("r_decal_switch_other_alpha", "0.66", "Force this transparency for decals on switches.", CVAR_Archive);

// make renderer life somewhat easier by not allowing alot of decals
// main work is done by `VLevel->CleanupSegDecals()`
VCvarI gl_bigdecal_limit("gl_bigdecal_limit", "16", "Limit for big decals on one seg (usually produced by gore mod).", /*CVAR_PreInit|*/CVAR_Archive);
VCvarI gl_smalldecal_limit("gl_smalldecal_limit", "64", "Limit for small decals on one seg (usually produced by shots).", /*CVAR_PreInit|*/CVAR_Archive);

VCvarI gl_flatdecal_limit("gl_flatdecal_limit", "16", "Limit for overlapping decals on floor/ceiling.", /*CVAR_PreInit|*/CVAR_Archive);


// sorry for this static
// it is used in decal cleanup code
static TArray<decal_t *> dc2kill;


//==========================================================================
//
//  VLevel::CalcDecalAlpha
//
//==========================================================================
float VLevel::CalcDecalAlpha (const VDecalDef *dec, const float alpha) noexcept {
  vassert(dec);
  float dcalpha = dec->alpha.value;
  if (alpha >= 0.0f) {
    //GCon->Logf(NAME_Debug, "decal '%s': orig alpha=%g (forced alpha=%g)", *dec->name, dcalpha, alpha);
    if (alpha < 1000.0f) dcalpha = alpha; else dcalpha *= alpha-1000.0f;
    //GCon->Logf(NAME_Debug, "decal '%s': final alpha=%g", *dec->name, dcalpha);
  }
  if (dcalpha < 0.004f) return 0.0f;
  return min2(1.0f, dcalpha);
}


//==========================================================================
//
//  VLevel::CalcSwitchDecalAlpha
//
//  switches should be more visible, so make decals almost transparent
//
//==========================================================================
float VLevel::CalcSwitchDecalAlpha (const VDecalDef *dec, const float ovralpha) {
  if (!r_decal_switch_special.asBool()) return ovralpha;
  vassert(dec);
  const float alpha = CalcDecalAlpha(dec, ovralpha);
  if (alpha <= 0.0f) return alpha;
  // blood?
  if (dec->bloodSplat) return clampval(min2(alpha, r_decal_switch_blood_alpha.asFloat()), 0.0f, 1.0f);
  if (dec->bootPrint) return clampval(min2(alpha, r_decal_switch_boot_alpha.asFloat()), 0.0f, 1.0f);
  // others
  return clampval(min2(alpha, r_decal_switch_other_alpha.asFloat()), 0.0f, 1.0f);
}


//==========================================================================
//
//  VLevel::IncLineTouchCounter
//
//==========================================================================
void VLevel::IncLineTouchCounter () noexcept {
  if (dcLineTouchMark.length() == NumLines && dcSegTouchMark.length() == NumSegs) {
    if (++dcLineTouchCounter != MAX_VINT32) return;
  } else {
    dcLineTouchMark.setLength(NumLines);
    dcSegTouchMark.setLength(NumSegs);
  }
  for (auto &&v : dcLineTouchMark) v = 0;
  for (auto &&v : dcSegTouchMark) v = 0;
  dcLineTouchCounter = 1;
}


//==========================================================================
//
//  VLevel::AddAnimatedDecal
//
//==========================================================================
void VLevel::AddAnimatedDecal (decal_t *dc) {
  if (!dc || !dc->animator) return;
  vassert(!dc->prevanimated);
  vassert(!dc->nextanimated);
  vassert(decanimlist != dc);
  if (decanimlist) decanimlist->prevanimated = dc;
  dc->nextanimated = decanimlist;
  decanimlist = dc;
}


//==========================================================================
//
//  VLevel::RemoveDecalAnimator
//
//  this will also delete animator
//  safe to call on decals without an animator
//
//==========================================================================
void VLevel::RemoveDecalAnimator (decal_t *dc) {
  if (!dc || !dc->animator) return;
  vassert(dc->prevanimated || dc->nextanimated || decanimlist == dc);
  if (dc->prevanimated) dc->prevanimated->nextanimated = dc->nextanimated; else decanimlist = dc->nextanimated;
  if (dc->nextanimated) dc->nextanimated->prevanimated = dc->prevanimated;
  delete dc->animator;
  dc->animator = nullptr;
  dc->prevanimated = dc->nextanimated = nullptr;
}


//==========================================================================
//
//  VLevel::AllocSegDecal
//
//==========================================================================
decal_t *VLevel::AllocSegDecal (seg_t *seg, VDecalDef *dec, float alpha, VDecalAnim *animator) {
  vassert(seg);
  vassert(dec);
  //alpha = clampval((alpha >= 0.0f ? alpha : dec->alpha.value), 0.0f, 1.0f);
  alpha = CalcDecalAlpha(dec, alpha);
  decal_t *decal = new decal_t;
  memset((void *)decal, 0, sizeof(decal_t));
  //decal->dectype = dec->name;
  decal->proto = dec;
  decal->texture = dec->texid;
  decal->shadeclr = decal->origshadeclr = dec->shadeclr;
  //decal->translation = translation;
  //decal->orgz = decal->curz = orgz;
  //decal->xdist = lineofs;
  decal->angle = AngleMod(dec->angleWall.value);
  decal->ofsX = decal->ofsY = 0.0f;
  decal->scaleX = decal->origScaleX = dec->scaleX.value;
  decal->scaleY = decal->origScaleY = dec->scaleY.value;
  decal->alpha = decal->origAlpha = alpha;
  decal->flags =
    (dec->fullbright ? decal_t::Fullbright : 0u)|
    (dec->fuzzy ? decal_t::Fuzzy : 0u)|
    (dec->bloodSplat ? decal_t::BloodSplat : 0u)|
    (dec->bootPrint ? decal_t::BootPrint : 0u)|
    (dec->additive ? decal_t::Additive : 0u);

  decal->boottime = 0.0f; //dec->boottime.value;
  decal->bootanimator = NAME_None; //dec->bootanimator;
  decal->bootshade = -2;
  decal->boottranslation = -1;
  decal->bootalpha = -1.0f;

  decal->animator = (animator ? animator : dec->animator);
  //if (decal->animator) GCon->Logf(NAME_Debug, "setting animator for '%s': <%s> (%s : %d)", *dec->name, *decal->animator->name, decal->animator->getTypeName(), (int)decal->animator->isEmpty());
  if (decal->animator && decal->animator->isEmpty()) decal->animator = nullptr;
  if (decal->animator) decal->animator = decal->animator->clone();
  //if (decal->animator) GCon->Logf(NAME_Debug, "set animator for '%s': <%s> (%s : %d)", *dec->name, *decal->animator->name, decal->animator->getTypeName(), (int)decal->animator->isEmpty());
  seg->appendDecal(decal);
  if (decal->animator) AddAnimatedDecal(decal);

  return decal;
}


//==========================================================================
//
//  decalAreaCompare
//
//==========================================================================
static int decalAreaCompare (const void *aa, const void *bb, void *udata) {
  if (aa == bb) return 0;
  const decal_t *adc = *(const decal_t **)aa;
  const decal_t *bdc = *(const decal_t **)bb;
  const float areaA = BBox2DArea(adc->bbox2d);
  const float areaB = BBox2DArea(bdc->bbox2d);
  // if one decal is more than two times smaller than the other, prefer smaller decal
  if (areaA <= areaB*0.5f) return -1;
  if (areaB <= areaA*0.5f) return +1;
  // if the size is roughly the same, prefer animated
  if (!!adc->animator != !!bdc->animator) {
    const float prcsz = min2(areaA, areaB)/max2(areaA, areaB);
    if (prcsz >= 0.88f) return (adc->animator ? -1 : +1);
  }
  // prefer decal that will disappear anyway
  if (adc->animator || bdc->animator) {
    const bool aWillDie = (adc->animator ? adc->animator->calcWillDisappear() : false);
    const bool bWillDie = (bdc->animator ? bdc->animator->calcWillDisappear() : false);
    if (aWillDie != bWillDie) return (aWillDie ? -1 : +1);
  }
  // normal area comparison
  const float adiff = areaA-areaB;
  if (adiff < 0.0f) return -1;
  if (adiff > 0.0f) return +1;
  return 0;
}


//==========================================================================
//
//  VLevel::CleanupSegDecals
//
//  permament decals are simply ignored
//
//==========================================================================
void VLevel::CleanupSegDecals (seg_t *seg) {
  if (!seg) return;
  const int bigLimit = gl_bigdecal_limit.asInt();
  const int smallLimit = gl_smalldecal_limit.asInt();
  if (bigLimit < 1 && smallLimit < 1) return; // nothing to do

  dc2kill.resetNoDtor();

  // count decals
  int bigDecalCount = 0, smallDecalCount = 0;
  decal_t *dc = seg->decalhead;
  while (dc) {
    decal_t *cdc = dc;
    dc = dc->next;

    int dcTexId = cdc->texture;
    auto dtex = GTextureManager[dcTexId];
    if (!dtex || dtex->Width < 1 || dtex->Height < 1) {
      // remove this decal (just in case)
      DestroyDecal(cdc);
      continue;
    }

    const int twdt = (int)(dtex->GetScaledWidthF()*cdc->scaleX);
    const int thgt = (int)(dtex->GetScaledHeightF()*cdc->scaleY);

    if (!cdc->animator && (twdt < 1 || thgt < 1 || cdc->alpha < 0.004f)) {
      // remove this decal (just in case)
      DestroyDecal(cdc);
      continue;
    }

    if (cdc->isPermanent()) continue;

    if (twdt >= BigDecalWidth || thgt >= BigDecalHeight) ++bigDecalCount; else ++smallDecalCount;
    dc2kill.append(cdc);
  }

  int toKillBig = (bigLimit > 0 ? bigDecalCount-bigLimit : 0);
  int toKillSmall = (smallLimit > 0 ? smallDecalCount-smallLimit : 0);
  if (toKillBig <= 0 && toKillSmall <= 0) return;

  if (toKillBig < 0) toKillBig = 0;
  if (toKillSmall < 0) toKillSmall = 0;

  // hack: if we have to remove big decals, limit small decals too
  if (toKillBig > 0 && smallDecalCount > bigLimit) {
    int biglim = bigLimit+bigLimit/4;
    //if (biglim < 1) biglim = 1;
    int tks = smallDecalCount-biglim;
    if (tks > toKillSmall) {
      //GCon->Logf(NAME_Debug, "force-kill %d small decals instead of %d", tks, toKillSmall);
      toKillSmall = tks;
    }
  }

  //if (toKillBig) GCon->Logf(NAME_Debug, "have to kill %d big decals...", toKillBig);
  //if (toKillSmall) GCon->Logf(NAME_Debug, "have to kill %d small decals...", toKillSmall);

  // prefer smaller and animated decals
  int dcCount = toKillBig+toKillSmall;

  // sort by preference
  timsort_r(dc2kill.ptr(), dc2kill.length(), sizeof(decal_t *), &decalAreaCompare, nullptr);
  for (decal_t *dcdie : dc2kill) {
    if (dcCount) {
      --dcCount;
      //GCon->Logf(NAME_Debug, "killing wall %sdecal '%s' with area %g", (dcdie->animator ? "animated " : ""), *GTextureManager[dcdie->texture]->Name, BBox2DArea(dcdie->bbox2d));
      DestroyDecal(dcdie);
    }
  }
}


//==========================================================================
//
//  VLevel::DestroyFlatDecal
//
//  this will also destroy decal and its animator!
//
//==========================================================================
void VLevel::DestroyFlatDecal (decal_t *dc) {
  vassert(dc);
  vassert(dc->isFloor() || dc->isCeiling());
  vassert(dc->sub);
  vassert(NumSubsectors > 0);
  vassert(subsectorDecalList);
  // remove from renderer
  if (dc->sreg && Renderer) Renderer->RemoveFlatDecal(dc);
  const int sidx = (int)(ptrdiff_t)(dc->sub-&Subsectors[0]);
  if (sidx < 0 || sidx >= NumSubsectors) return;
  // remove from subsector list
  VDecalList *lst = &subsectorDecalList[sidx];
  DLListRemoveEx(dc, lst->head, lst->tail, subprev, subnext);
  // remove from global list
  DLListRemove(dc, subdecalhead, subdecaltail);
  // remove animator
  RemoveDecalAnimator(dc);
  // and kill decal
  delete dc;
}


//==========================================================================
//
//  VLevel::KillAllSubsectorDecals
//
//==========================================================================
void VLevel::KillAllSubsectorDecals () {
  while (subdecalhead) DestroyFlatDecal(subdecalhead);
  vassert(!subdecalhead);
  vassert(!subdecaltail);
  if (subsectorDecalList) {
    delete[] subsectorDecalList;
    subsectorDecalList = nullptr;
  }
}


//==========================================================================
//
//  VLevel::AppendDecalToSubsectorList
//
//  permament decals are simply ignored by the limiter
//
//==========================================================================
void VLevel::AppendDecalToSubsectorList (decal_t *dc) {
  if (!dc) return;

  if (dc->animator) AddAnimatedDecal(dc);
  dc->calculateBBox();

  if (NumSubsectors == 0/*just in case*/) {
    RemoveDecalAnimator(dc);
    delete dc;
    return;
  }

  vassert(dc->sub);
  vassert(dc->isFloor() || dc->isCeiling());
  vassert(!dc->prev);
  vassert(!dc->next);
  vassert(!dc->subprev);
  vassert(!dc->subnext);
  vassert(!dc->sregprev);
  vassert(!dc->sregnext);
  vassert(!dc->seg);
  vassert(!dc->sreg);
  vassert(!dc->slidesec);
  vassert(dc->eregindex >= 0);

  // append to global sector decal list
  DLListAppend(dc, subdecalhead, subdecaltail);

  // append to list of decals in the given sector
  if (!subsectorDecalList) {
    subsectorDecalList = new VDecalList[NumSubsectors];
    for (int f = 0; f < NumSubsectors; ++f) subsectorDecalList[f].head = subsectorDecalList[f].tail = nullptr;
  }

  VDecalList *lst = &subsectorDecalList[(unsigned)(ptrdiff_t)(dc->sub-&Subsectors[0])];
  DLListAppendEx(dc, lst->head, lst->tail, subprev, subnext);

  if (Renderer) Renderer->AppendFlatDecal(dc);

  // check subsector decals limit
  const int dclimit = gl_flatdecal_limit.asInt();
  if (dclimit > 0 && !dc->isPermanent()) {
    // prefer decals with animators (they are usually transient blood or something like that)
    dc2kill.resetNoDtor();
    constexpr float shrinkRatio = 0.8f;
    float mybbox[4];
    ShrinkBBox2D(mybbox, dc->bbox2d, shrinkRatio);
    float curbbox[4];
    int dcCount = 0;
    decal_t *prdc = lst->head;
    while (prdc != dc) {
      decal_t *curdc = prdc;
      prdc = prdc->subnext;
      if (curdc->eregindex != dc->eregindex) continue;
      if (curdc->dcsurf != dc->dcsurf) continue;
      if (curdc->isPermanent()) continue;
      ShrinkBBox2D(curbbox, curdc->bbox2d, shrinkRatio);
      if (Are2DBBoxesOverlap(mybbox, curbbox)) {
        ++dcCount;
        dc2kill.append(curdc);
      }
    }
    // do we need to remove some decals?
    if (dcCount > dclimit) {
      // prefer smallest decals
      //GCon->Logf(NAME_Debug, "%d flat decals to remove (%d total)...", dcCount-dclimit, dcCount);
      dcCount -= dclimit; // decals left to remove
      // sort by preference
      timsort_r(dc2kill.ptr(), dc2kill.length(), sizeof(decal_t *), &decalAreaCompare, nullptr);
      for (decal_t *dcdie : dc2kill) {
        if (dcCount) {
          --dcCount;
          //GCon->Logf(NAME_Debug, "killing flat %sdecal '%s' with area %g", (dcdie->animator ? "animated " : ""), *GTextureManager[dcdie->texture]->Name, BBox2DArea(dcdie->bbox2d));
          DestroyFlatDecal(dcdie);
        }
      }
    }
  }
}


//==========================================================================
//
//  VLevel::DestroyDecal
//
//  this will also destroy decal and its animator!
//
//==========================================================================
void VLevel::DestroyDecal (decal_t *dc) {
  if (!dc) return;
  if (dc->isWall()) {
    RemoveDecalAnimator(dc);
    dc->seg->removeDecal(dc);
    delete dc;
  } else {
    return DestroyFlatDecal(dc);
  }
}



//**************************************************************************
//
// VavoomC API
//
//**************************************************************************

//native final void AddDecal (TVec org, name dectype, int side, line_t *li, optional int translation,
//                            optional int shadeclr, optional float alpha, optional name animator,
//                            optional bool permanent, optional float angle, optional bool forceFlipX);
IMPLEMENT_FUNCTION(VLevel, AddDecal) {
  TVec org;
  VName dectype;
  int side;
  line_t *li;
  VOptParamInt translation(0);
  VOptParamInt shadeclr(-2);
  VOptParamFloat alpha(-2.0f);
  VOptParamName animator(NAME_None);
  VOptParamBool permanent(false);
  VOptParamFloat angle(INFINITY);
  VOptParamBool forceFlipX(false);
  vobjGetParamSelf(org, dectype, side, li, translation, shadeclr, alpha, animator, permanent, angle, forceFlipX);
  //if (!angle.specified) angle.value = INFINITY;
  DecalParams params;
  params.translation = translation.value;
  params.shadeclr = shadeclr.value;
  params.alpha = alpha.value;
  params.animator = VDecalAnim::GetAnimatorByName(animator.value);
  params.angle = angle.value;
  params.forceFlipX = forceFlipX.value;
  params.forcePermanent = permanent.value;
  Self->AddDecal(org, dectype, side, li, 0, params);
}

//native final void AddDecalById (TVec org, int id, int side, line_t *li, optional int translation,
//                                optional int shadeclr, optional float alpha, optional name animator);
IMPLEMENT_FUNCTION(VLevel, AddDecalById) {
  TVec org;
  int id;
  int side;
  line_t *li;
  VOptParamInt translation(0);
  VOptParamInt shadeclr(-2);
  VOptParamFloat alpha(-2.0f);
  VOptParamName animator(NAME_None);
  vobjGetParamSelf(org, id, side, li, translation, shadeclr, alpha, animator);
  DecalParams params;
  params.translation = translation.value;
  params.shadeclr = shadeclr.value;
  params.alpha = alpha.value;
  params.animator = VDecalAnim::GetAnimatorByName(animator.value);
  params.angle = INFINITY;
  params.forceFlipX = false;
  params.forcePermanent = true;
  Self->AddDecalById(org, id, side, li, 0, params);
}


//native final void AddFlatDecal (TVec org, name dectype, float range, optional int translation, optional int shadeclr, optional float alpha,
//                                optional name animator, optional float angle, optional bool forceFlipX, optional bool permanent);
IMPLEMENT_FUNCTION(VLevel, AddFlatDecal) {
  TVec org;
  VName dectype;
  float range;
  VOptParamInt translation(0);
  VOptParamInt shadeclr(-2);
  VOptParamFloat alpha(-2.0f);
  VOptParamName animator(NAME_None);
  VOptParamFloat angle(INFINITY);
  VOptParamBool forceFlipX(false);
  VOptParamBool permanent(false);
  vobjGetParamSelf(org, dectype, range, translation, shadeclr, alpha, animator, angle, forceFlipX, permanent);
  //if (!angle.specified) angle = INFINITY;
  DecalParams params;
  params.translation = translation.value;
  params.shadeclr = shadeclr.value;
  params.alpha = alpha.value;
  params.animator = VDecalAnim::GetAnimatorByName(animator.value);
  params.angle = angle.value;
  params.forceFlipX = forceFlipX.value;
  params.forcePermanent = permanent.value;
  Self->AddFlatDecal(org, dectype, range, params);
}


// check what kind of bootprint decal is required at `org`
// returns `false` if none (params are undefined)
// native /*final*/ bool CheckBootPrints (TVec org, subsector_t *sub, out VBootPrintDecalParams params);
IMPLEMENT_FUNCTION(VLevel, CheckBootPrints) {
  TVec org;
  subsector_t *sub;
  VBootPrintDecalParams *params;
  vobjGetParamSelf(org, sub, params);
  RET_BOOL(Self->CheckBootPrints(org, sub, *params));
}


COMMAND(RemoveAllDecals) {
  VLevel *lvl = (GLevel ? GLevel : GClLevel);
  if (!lvl) {
    GCon->Log(NAME_Error, "no level loaded");
    return;
  }
  lvl->KillAllMapDecals();
  GCon->Log("removed all decals");
}
