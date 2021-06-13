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

TArray<VLevel::DecalLineInfo> VLevel::connectedLines;

TMapNC<VName, bool> VLevel::baddecals;
TArray<int> VLevel::dcLineTouchMark;
TArray<int> VLevel::dcSegTouchMark;
int VLevel::dcLineTouchCounter = 0;


//==========================================================================
//
//  lif2str
//
//==========================================================================
static VVA_OKUNUSED const char *lif2str (int flags) noexcept {
  static char buf[128];
  char *pp = buf;
  *pp++ = '<';
  if (flags&ML_TWOSIDED) *pp++ = '2';
  if (flags&ML_DONTPEGTOP) *pp++ = 'T';
  if (flags&ML_DONTPEGBOTTOM) *pp++ = 'B';
  *pp++ = '>';
  *pp = 0;
  return buf;
}


//==========================================================================
//
//  calcDecalSide
//
//==========================================================================
#if 0
static int calcDecalSide (const line_t *li, const sector_t *fsec, const sector_t *bsec, const line_t *nline, int origside, int lvnum) {
  // find out correct side
  if (nline->frontsector == fsec || nline->backsector == bsec) return 0;
  if (nline->backsector == fsec || nline->frontsector == bsec) return 1;
  // try to find out the side by using decal spawn point, and current side direction
  // move back a little
  TVec xdir = li->normal;
  if (origside) xdir = -xdir;
  //TVec norg = ((*li->v1)+(*li->v2))*0.5f; // line center
  TVec norg = (lvnum == 0 ? *li->v1 : *li->v2);
  norg += xdir*1024.0f;
  int nside = nline->PointOnSide(norg);
  VDC_DLOG(NAME_Debug, "  (0)nline=%d, detected side %d", (int)(ptrdiff_t)(nline-GLevel->Lines), nside);
  if (li->sidenum[nside] < 0) {
    nside ^= 1;
    if (li->sidenum[nside] < 0) return -1; // wuta?
  }
  VDC_DLOG(NAME_Debug, "  (0)nline=%d, choosen side %d", (int)(ptrdiff_t)(nline-GLevel->Lines), nside);
  /*
  VDC_DLOG(NAME_Debug, "  nline=%d, cannot detect side", (int)(ptrdiff_t)(nline-GLevel->Lines));
  //nside = origside;
  continue;
  */
  return nside;
}
#endif


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
  alpha = clampval((alpha >= 0.0f ? alpha : dec->alpha.value), 0.0f, 1.0f);
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
  decal->addAlpha = dec->addAlpha.value;
  decal->flags =
    (dec->fullbright ? decal_t::Fullbright : 0u)|
    (dec->fuzzy ? decal_t::Fuzzy : 0u)|
    (dec->bloodSplat ? decal_t::BloodSplat : 0u)|
    (dec->bootPrint ? decal_t::BootPrint : 0u);
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


// sorry for those statics
static TArray<decal_t *> dc2kill;


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
//  CheckLeakToSector
//
//  is there a way reach `destsec` from `fsec`?
//
//==========================================================================
static bool CheckLeakToSector (VLevel *Level, const sector_t *fsec, const sector_t *destsec) noexcept {
  if (!fsec || !destsec) return false;
  if (fsec == destsec) return true;
  line_t *const *lnp = fsec->lines;
  for (int f = fsec->linecount; f--; ++lnp) {
    const line_t *ln = *lnp;
    if ((ln->flags&ML_TWOSIDED) == 0) continue;
    int checkside;
         if (ln->frontsector == fsec && ln->backsector == destsec) checkside = 0;
    else if (ln->frontsector == destsec && ln->backsector == fsec) checkside = 1;
    else continue;
    if (ln->sidenum[checkside] < 0) continue; // just in case
    for (seg_t *seg = ln->firstseg; seg; seg = seg->lsnext) {
      if (seg->side != checkside) continue; // partner segs will be processed with normal segs
      if (seg->flags&SF_ZEROLEN) continue; // invalid seg
      if (!VViewClipper::IsSegAClosedSomething(Level, nullptr, seg)) return true;
    }
  }
  return false;
}


//==========================================================================
//
//  IsWallSpreadAllowed
//
//  moving to another sector, check if we have any open line into it
//  all prerequisites should be checked by the caller
//
//==========================================================================
static bool IsWallSpreadAllowed (VLevel *Level, const sector_t *fsec,
                                 const line_t *li, const int vxidx,
                                 const line_t *nline, const int nside) noexcept
{
  // this is wrong: we may not have proper connections (see MAP01 entrance)
  // for now, let decals leak over one-sided lines
  if (nline->sidenum[nside] < 0) return false;
  const sector_t *others = (nside ? nline->backsector : nline->frontsector);
  if (!others) return false;
  if (others == fsec) return true;
  for (int ni = 0; ni < li->vxCount(vxidx); ++ni) {
    const line_t *nb = li->vxLine(vxidx, ni);
    if (nb == nline || nb == li || !(nb->flags&ML_TWOSIDED)) continue;
    // one side should be our front sector, other side should be other sector
    int checkside;
         if (nb->frontsector == fsec && nb->backsector == others) checkside = 0;
    else if (nb->frontsector == others && nb->backsector == fsec) checkside = 1;
    else {
      if (nb->frontsector == fsec) {
        if (CheckLeakToSector(Level, nb->backsector, others)) return true;
      } else if (nb->backsector == fsec) {
        if (CheckLeakToSector(Level, nb->frontsector, others)) return true;
      }
      continue;
    }
    if (nb->sidenum[checkside] < 0) continue; // just in case
    // check if it is opened
    for (seg_t *seg = nb->firstseg; seg; seg = seg->lsnext) {
      if (seg->side != checkside) continue; // partner segs will be processed with normal segs
      if (seg->flags&SF_ZEROLEN) continue; // invalid seg
      if (!VViewClipper::IsSegAClosedSomething(Level, nullptr, seg)) return true;
      // no need to check other segs, the result will be the same
      break;
    }
  }
  return false;
}


//==========================================================================
//
//  hasSliderAnimator
//
//==========================================================================
static inline bool hasSliderAnimator (const VDecalDef *dec, const VDecalAnim *anim) noexcept {
  if (!dec) return false;
  if (!anim) anim = dec->animator;
  if (!anim) return false;
  return (anim->hasTypeId(TDecAnimSlider));
}


//==========================================================================
//
//  CalcSwitchDecalAlpha
//
//  switches should be more visible, so make decals almost transparent
//
//==========================================================================
static float CalcSwitchDecalAlpha (const VDecalDef *dec, const float ovralpha) {
  if (!r_decal_switch_special.asBool()) return ovralpha;
  vassert(dec);
  float alpha = clampval(ovralpha >= 0.0f ? ovralpha : dec->alpha.value, 0.0f, 1.0f);
  if (alpha <= 0.0f) return alpha;
  // blood?
  if (dec->bloodSplat) return clampval(min2(alpha, r_decal_switch_blood_alpha.asFloat()), 0.0f, 1.0f);
  if (dec->bootPrint) return clampval(min2(alpha, r_decal_switch_boot_alpha.asFloat()), 0.0f, 1.0f);
  // others
  return clampval(min2(alpha, r_decal_switch_other_alpha.asFloat()), 0.0f, 1.0f);
}


//==========================================================================
//
//  VLevel::PutDecalAtLine
//
//  `flips` will be bitwise-ored with decal flags
//
//==========================================================================
void VLevel::PutDecalAtLine (const TVec &org, float lineofs, VDecalDef *dec, int side, line_t *li,
                             unsigned flips, int translation, int shadeclr, float alpha, VDecalAnim *animator,
                             float angle, bool skipMarkCheck)
{
  // don't process linedef twice
  if (!skipMarkCheck) {
    if (IsLineTouched(li)) return;
    MarkLineTouched(li);
  }

  const float orgz = org.z;

  VTexture *dtex = GTextureManager[dec->texid];
  //if (!dtex || dtex->Type == TEXTYPE_Null) return;

  //GCon->Logf(NAME_Debug, "decal '%s' at linedef %d", *GTextureManager[tex]->Name, (int)(ptrdiff_t)(li-Lines));

  const float twdt = dtex->GetScaledWidthF()*dec->scaleX.value;
  const float thgt = dtex->GetScaledHeightF()*dec->scaleY.value;

  sector_t *fsec;
  sector_t *bsec;
  if (side == 0) {
    // front side
    fsec = li->frontsector;
    bsec = li->backsector;
  } else {
    // back side
    fsec = li->backsector;
    bsec = li->frontsector;
  }

  if (fsec && bsec && fsec == bsec) bsec = nullptr; // self-referenced lines

  // flip side if we somehow ended on the wrong side of one-sided line
  if (!fsec) {
    side ^= 1;
    fsec = bsec;
    if (!bsec) {
      GCon->Logf(NAME_Debug, "oops; something went wrong in decal code!");
      return;
    }
  }

  // offset is always from `v1`
  const unsigned stvflag = 0u; //(side ? decal_t::FromV2 : 0u);

  const side_t *sidedef = (li->sidenum[side] >= 0 ? &Sides[li->sidenum[side]] : nullptr);
  const TVec &v1 = *li->v1;
  const TVec &v2 = *li->v2;
  const float linelen = (v2-v1).length2D();

  // decal flipping won't change decal offset
  const float txofs = dtex->GetScaledSOffsetF()*dec->scaleX.value;

  // calculate left and right texture bounds on the line
  const float dcx0 = lineofs-txofs;
  const float dcx1 = dcx0+twdt;

  // check if decal is in line bounds
  if (dcx1 <= 0.0f || dcx0 >= linelen) {
    // out of bounds
    VDC_DLOG(NAME_Debug, "*** OOB: Decal '%s' at line #%d (side %d; fs=%d; bs=%d): linelen=%g; dcx0=%g; dcx1=%g", *dec->name, (int)(ptrdiff_t)(li-Lines), side, (int)(ptrdiff_t)(fsec-Sectors), (bsec ? (int)(ptrdiff_t)(bsec-Sectors) : -1), linelen, dcx0, dcx1);
  }

  // calculate top and bottom texture bounds
  // decal flipping won't change decal offset
  const float tyofs = dtex->GetScaledTOffsetF()*dec->scaleY.value;
  const float dcy1 = orgz+dec->scaleY.value+tyofs;
  const float dcy0 = dcy1-thgt;

  VDC_DLOG(NAME_Debug, "Decal '%s' at line #%d (side %d; fs=%d; bs=%d): linelen=%g; o0=%g; o1=%g (ofsorig=%g; txofs=%g; tyofs=%g; tw=%g; th=%g)", *dec->name, (int)(ptrdiff_t)(li-Lines), side, (int)(ptrdiff_t)(fsec-Sectors), (bsec ? (int)(ptrdiff_t)(bsec-Sectors) : -1), linelen, dcx0, dcx1, lineofs, txofs, tyofs, twdt, thgt);

  const TVec linepos = v1+li->ndir*lineofs;

  const float ffloorZ = fsec->floor.GetPointZClamped(linepos);
  const float fceilingZ = fsec->ceiling.GetPointZClamped(linepos);

  const float bfloorZ = (bsec ? bsec->floor.GetPointZClamped(linepos) : ffloorZ);
  const float bceilingZ = (bsec ? bsec->ceiling.GetPointZClamped(linepos) : fceilingZ);

  if (sidedef && ((li->flags&(ML_NODECAL|ML_ADDITIVE))|(sidedef->Flags&SDF_NODECAL)) == 0) {
    // find segs for this decal (there may be several segs)
    // for two-sided lines, put decal on segs for both sectors
    for (seg_t *seg = li->firstseg; seg; seg = seg->lsnext) {
      if (seg->side != side) continue; // partner segs will be processed with normal segs
      if (IsSegTouched(seg)) continue;
      MarkSegTouched(seg);
      const bool doParnter = !IsSegTouched(seg->partner);
      MarkSegTouched(seg->partner);

      if (!seg->linedef) continue; // ignore minisegs (just in case)
      //if (seg->frontsub->isOriginalPObj()) continue; // ignore original polyobj sectors (just in case) -- nope, we want decals on polyobjects too
      if (seg->flags&SF_ZEROLEN) continue; // invalid seg
      vassert(seg->linedef == li);

      VDC_DLOG(NAME_Debug, "  checking seg #%d; offset=%g; length=%g (ldef=%d; segside=%d)", (int)(ptrdiff_t)(seg-Segs), seg->offset, seg->length, (int)(ptrdiff_t)(seg->linedef-&Lines[0]), seg->side);

      sector_t *sec3d = seg->backsector;
      const bool has3dregs = (sec3d && sec3d->eregions->next);

      // check for 3d floors
      // for some reason, "oob" segs should still be checked if they has 3d floors, so do this before bounds checking
      //FIXME: reverse and document this!
      if (has3dregs) {
        //if (s3dsnum == 1 && seg->backsector == seg->frontsector) continue;
        VDC_DLOG(NAME_Debug, "*** this seg has 3d floor regions (secnum=%d)!", (int)(ptrdiff_t)(sec3d-&Sectors[0]));
        // find solid region to put decal onto
        for (sec_region_t *reg = sec3d->eregions->next; reg; reg = reg->next) {
          if (!reg->extraline) continue; // no need to create extra side
          if (reg->regflags&(sec_region_t::RF_NonSolid|sec_region_t::RF_BaseRegion|sec_region_t::RF_OnlyVisual)) continue;
          const side_t *extraside = &Sides[reg->extraline->sidenum[0]];
          // hack: 3d floor with sky texture seems to be transparent in renderer
          if (extraside->MidTexture == skyflatnum) continue;
          const float fz = reg->efloor.GetPointZClamped(linepos);
          const float cz = reg->eceiling.GetPointZClamped(linepos);
          if (dcy0 < cz && dcy1 > fz) {
            const float swalpha = (reg->extraline->special && VLevelInfo::IsSwitchTexture(extraside->MidTexture) ? CalcSwitchDecalAlpha(dec, alpha) : alpha);
            VDC_DLOG(NAME_Debug, " HIT solid region: fz=%g; cz=%g; orgz=%g; dcy0=%g; dcy1=%g", fz, cz, orgz, dcy0, dcy1);
            // create decal
            decal_t *decal = AllocSegDecal(seg, dec, swalpha, animator);
            if (isFiniteF(angle)) decal->angle = AngleMod(angle);
            decal->translation = translation;
            if (shadeclr != -2) decal->shadeclr = decal->origshadeclr = shadeclr;
            decal->orgz = decal->curz = orgz;
            decal->xdist = lineofs;
            // setup misc flags
            decal->flags |= flips|stvflag;
            decal->flags |= /*disabledTextures*/0u;
            // slide with 3d floor floor
            decal->slidesec = sec3d;
            decal->flags |= decal_t::SlideFloor;
            decal->curz -= decal->slidesec->floor.TexZ;
            decal->calculateBBox();
            if (doParnter) {
              // create decal on partner seg
              decal_t *dcp = AllocSegDecal(seg->partner, dec, swalpha, animator);
              dcp->angle = decal->angle;
              dcp->translation = decal->translation;
              dcp->shadeclr = dcp->origshadeclr = decal->shadeclr;
              dcp->orgz = decal->orgz;
              dcp->curz = decal->curz;
              dcp->xdist = decal->xdist;
              dcp->flags = decal->flags/*^decal_t::FromV2*/^decal_t::FlipX;
              dcp->slidesec = decal->slidesec;
              dcp->calculateBBox();
              if (!dcp->isPermanent()) CleanupSegDecals(seg->partner);
            }
            if (!decal->isPermanent()) CleanupSegDecals(seg);
          } else {
            VDC_DLOG(NAME_Debug, " SKIP solid region: fz=%g; cz=%g; orgz=%g; dcy0=%g; dcy1=%g", fz, cz, orgz, dcy0, dcy1);
          }
        }
      }

      // check if decal is in seg bounds
      if (seg->side == 0) {
        // seg running forward
        if (dcx1 <= seg->offset || dcx0 >= seg->offset+seg->length) {
          // out of bounds
          VDC_DLOG(NAME_Debug, "   OOB(side0)! dcx0=%g; dcx1=%g; sofs0=%g; sofs1=%g", dcx0, dcx1, seg->offset, seg->offset+seg->length);
          continue;
        }
        VDC_DLOG(NAME_Debug, "   HIT(side0)! dcx0=%g; dcx1=%g; sofs0=%g; sofs1=%g", dcx0, dcx1, seg->offset, seg->offset+seg->length);
      } else {
        // seg running backward
        const float sofs = linelen-seg->offset-seg->length;
        if (dcx1 <= sofs || dcx0 >= sofs+seg->length) {
          // out of bounds
          VDC_DLOG(NAME_Debug, "   OOB(side1)! dcx0=%g; dcx1=%g; sofs0=%g; sofs1=%g", dcx0, dcx1, sofs, sofs+seg->length);
          continue;
        }
        VDC_DLOG(NAME_Debug, "   HIT(side1)! dcx0=%g; dcx1=%g; sofs0=%g; sofs1=%g", dcx0, dcx1, sofs, sofs+seg->length);
      }

      const side_t *sb = seg->sidedef;

      // check if decal is allowed on this side
      if (sb->MidTexture == skyflatnum) continue; // never on the sky

      bool slideWithFloor = false;
      bool slideWithCeiling = false;
      sector_t *slidesec = nullptr;
      bool hasMidTex = true;
      bool onSwitch = false;

      if (sb->MidTexture <= 0 || GTextureManager(sb->MidTexture)->Type == TEXTYPE_Null) {
        hasMidTex = false;
      }

      // check if we have top/bottom textures
      bool allowTopTex = (sb->TopTexture > 0 && sb->TopTexture != skyflatnum);
      bool allowBotTex = (sb->BottomTexture > 0 && sb->BottomTexture != skyflatnum);
      if (allowTopTex) {
        VTexture *xtx = GTextureManager(sb->TopTexture);
        allowTopTex = (xtx && xtx->Type != TEXTYPE_Null && !xtx->noDecals);
      }
      if (allowBotTex) {
        VTexture *xtx = GTextureManager(sb->BottomTexture);
        allowBotTex = (xtx && xtx->Type != TEXTYPE_Null && !xtx->noDecals);
      }

      // can we hit toptex?
      if (allowTopTex) {
        if (fsec && bsec) {
          // if there is no ceiling height difference, toptex cannot be visible
          if (fsec->ceiling.minz == bsec->ceiling.minz &&
              fsec->ceiling.maxz == bsec->ceiling.maxz)
          {
            allowTopTex = false;
          } else if (fsec->ceiling.minz <= bsec->ceiling.minz) {
            // if front ceiling is lower than back ceiling, toptex cannot be visible
            allowTopTex = false;
          } else if (dcy1 <= min2(fceilingZ, bceilingZ)) {
            // if decal top is lower than lowest ceiling, consider toptex invisible
            // (i assume that we won't have animators sliding up)
            allowTopTex = false;
          }
        } else {
          // one-sided: see the last coment above
          if (dcy1 <= fceilingZ) allowTopTex = false;
        }
      }

      // can we hit bottex?
      if (allowBotTex) {
        if (fsec && bsec) {
          // if there is no floor height difference, bottex cannot be visible
          if (fsec->floor.minz == bsec->floor.minz &&
              fsec->floor.maxz == bsec->floor.maxz)
          {
            allowBotTex = false;
          } else if (fsec->floor.maxz >= bsec->floor.maxz) {
            // if front floor is higher than back floor, bottex cannot be visible
            allowBotTex = false;
          } else if (!hasSliderAnimator(dec, animator) && dcy0 >= max2(ffloorZ, bfloorZ)) {
            // if decal bottom is higher than highest floor, consider toptex invisible
            // (but don't do this for animated decals -- this may be sliding blood)
            allowBotTex = false;
          }
        } else {
          // one-sided: see the last coment above
          if (!hasSliderAnimator(dec, animator) && dcy0 >= ffloorZ) allowBotTex = false;
        }
      }

      // if no textures were hit, don't bother
      if (!hasMidTex && !allowTopTex && !allowBotTex) continue;

      vuint32 disabledTextures = 0;
      //FIXME: animators are often used to slide blood decals down
      //       until i'll implement proper bounding for animated decals,
      //       just allow bottom textures here
      //  later: don't do it yet, cropping sliding blood is ugly, but acceptable
      /*if (!dec->animator)*/ {
        if (!allowBotTex || min2(dcy0, dcy1) >= max2(ffloorZ, bfloorZ)) disabledTextures |= decal_t::NoBotTex;
      }
      if (!allowTopTex || max2(dcy0, dcy1) <= min2(fceilingZ, bceilingZ)) disabledTextures |= decal_t::NoTopTex;
      if (!hasMidTex) {
        disabledTextures |= decal_t::NoMidTex;
      } else {
        //if (min2(dcy0, dcy1) >= max2(ffloorZ, bfloorZ) || max2(dcy0, dcy1) <= min2(fceilingZ, bceilingZ))
        if (dcy1 > ffloorZ && dcy0 < fceilingZ)
        {
          // touching midtex
        } else {
          disabledTextures |= decal_t::NoMidTex;
        }
      }

      if (fsec && bsec) {
        VDC_DLOG(NAME_Debug, "  2s: orgz=%g; front=(%g,%g); back=(%g,%g)", orgz, ffloorZ, fceilingZ, bfloorZ, bceilingZ);
        if (hasMidTex && orgz >= max2(ffloorZ, bfloorZ) && orgz <= min2(fceilingZ, bceilingZ)) {
          // midtexture
               if (li->flags&ML_DONTPEGBOTTOM) slideWithFloor = true;
          else if (li->flags&ML_DONTPEGTOP) slideWithCeiling = true;
          else slideWithCeiling = true;
          onSwitch = (li->special && VLevelInfo::IsSwitchTexture(seg->sidedef->TopTexture));
        } else {
          if (allowTopTex && allowBotTex) {
            // both top and bottom
            if (orgz < max2(ffloorZ, bfloorZ)) {
              // bottom texture
              if ((li->flags&ML_DONTPEGBOTTOM) == 0) slideWithFloor = true;
              onSwitch = (li->special && VLevelInfo::IsSwitchTexture(seg->sidedef->BottomTexture));
            } else if (orgz > min2(fceilingZ, bceilingZ)) {
              // top texture
              if ((li->flags&ML_DONTPEGTOP) == 0) slideWithCeiling = true;
              onSwitch = (li->special && VLevelInfo::IsSwitchTexture(seg->sidedef->TopTexture));
            }
          } else if (allowBotTex) {
            // only bottom texture
            if ((li->flags&ML_DONTPEGBOTTOM) == 0) slideWithFloor = true;
            onSwitch = (li->special && VLevelInfo::IsSwitchTexture(seg->sidedef->BottomTexture));
          } else if (allowTopTex) {
            // only top texture
            if ((li->flags&ML_DONTPEGTOP) == 0) slideWithCeiling = true;
            onSwitch = (li->special && VLevelInfo::IsSwitchTexture(seg->sidedef->TopTexture));
          }
          VDC_DLOG(NAME_Debug, "  2s: front=(%g,%g); back=(%g,%g); sc=%d; sf=%d", ffloorZ, fceilingZ, bfloorZ, bceilingZ, (int)slideWithFloor, (int)slideWithCeiling);
        }

        // door hack
        /*
        if (!slideWithFloor && !slideWithCeiling) {
          if (ffloorZ == fceilingZ || bfloorZ == bceilingZ) {
            slideWithCeiling = (bfloorZ == ffloorZ);
            slideWithFloor = !slideWithCeiling;
            slidesec = (ffloorZ == fceilingZ ? fsec : bsec);
            //GCon->Logf(NAME_Debug, "DOOR HACK: front=(%g,%g); back=(%g,%g); sc=%d; sf=%d", ffloorZ, fceilingZ, bfloorZ, bceilingZ, (int)slideWithFloor, (int)slideWithCeiling);
          }
        }
        */
      } else {
        VDC_DLOG(NAME_Debug, "  1s: orgz=%g; front=(%g,%g); hasMidTex=%d; dcy=(%g : %g); gap=(%g : %g)", orgz, ffloorZ, fceilingZ, (int)hasMidTex, dcy0, dcy0, ffloorZ, fceilingZ);
        // one-sided
        if (hasMidTex && dcy1 > ffloorZ && dcy0 < fceilingZ) {
          // midtexture
               if (li->flags&ML_DONTPEGBOTTOM) slideWithFloor = true;
          else if (li->flags&ML_DONTPEGTOP) slideWithCeiling = true;
          else slideWithCeiling = true;
          VDC_DLOG(NAME_Debug, "   one-sided midtex: pegbot=%d; pegtop=%d; fslide=%d; cslide=%d", (int)(!!(li->flags&ML_DONTPEGBOTTOM)), (int)(!!(li->flags&ML_DONTPEGTOP)), (int)slideWithFloor, (int)slideWithCeiling);
          disabledTextures = decal_t::NoBotTex|decal_t::NoTopTex;
          onSwitch = (li->special && VLevelInfo::IsSwitchTexture(seg->sidedef->TopTexture));
        } else {
          /*
          if (allowTopTex && allowBotTex) {
            // both top and bottom
            if (orgz < ffloorZ) {
              // bottom texture
              if ((li->flags&ML_DONTPEGBOTTOM) == 0) slideWithFloor = true;
            } else if (orgz > fceilingZ) {
              // top texture
              if ((li->flags&ML_DONTPEGTOP) == 0) slideWithCeiling = true;
            }
          } else if (allowBotTex) {
            // only bottom texture
            if ((li->flags&ML_DONTPEGBOTTOM) == 0) slideWithFloor = true;
          } else if (allowTopTex) {
            // only top texture
            if ((li->flags&ML_DONTPEGTOP) == 0) slideWithCeiling = true;
          }
          */
          continue;
        }
        if (slideWithFloor || slideWithCeiling) slidesec = fsec;
        VDC_DLOG(NAME_Debug, "  1s: front=(%g,%g); sc=%d; sf=%d", ffloorZ, fceilingZ, (int)slideWithFloor, (int)slideWithCeiling);
      }

      VDC_DLOG(NAME_Debug, "  decaling seg #%d; offset=%g; length=%g", (int)(ptrdiff_t)(seg-Segs), seg->offset, seg->length);

      const float swalpha = (onSwitch ? CalcSwitchDecalAlpha(dec, alpha) : alpha);

      // create decal
      decal_t *decal = AllocSegDecal(seg, dec, swalpha, animator);
      if (isFiniteF(angle)) decal->angle = AngleMod(angle);
      decal->translation = translation;
      if (shadeclr != -2) decal->shadeclr = decal->origshadeclr = shadeclr;
      decal->orgz = decal->curz = orgz;
      decal->xdist = lineofs;
      // setup misc flags
      decal->flags |= flips|stvflag;
      decal->flags |= disabledTextures;
      decal->calculateBBox();

      // setup curz and pegs
      if (slideWithFloor) {
        decal->slidesec = (slidesec ? slidesec : bsec);
        if (decal->slidesec) {
          decal->flags |= decal_t::SlideFloor;
          decal->curz -= decal->slidesec->floor.TexZ;
          VDC_DLOG(NAME_Debug, "  floor slide; sec=%d", (int)(ptrdiff_t)(decal->slidesec-Sectors));
        }
      } else if (slideWithCeiling) {
        decal->slidesec = (slidesec ? slidesec : bsec);
        if (decal->slidesec) {
          decal->flags |= decal_t::SlideCeil;
          decal->curz -= decal->slidesec->ceiling.TexZ;
          VDC_DLOG(NAME_Debug, "  ceil slide; sec=%d", (int)(ptrdiff_t)(decal->slidesec-Sectors));
        }
      }
      //if (side != seg->side) decal->flags ^= decal_t::FlipX;

      if (doParnter) {
        // create decal on partner seg
        decal_t *dcp = AllocSegDecal(seg->partner, dec, swalpha, animator);
        dcp->angle = decal->angle;
        dcp->translation = translation;
        if (shadeclr != -2) dcp->shadeclr = dcp->origshadeclr = shadeclr;
        dcp->orgz = decal->orgz;
        dcp->curz = decal->curz;
        dcp->xdist = lineofs;
        // setup misc flags
        dcp->flags = decal->flags/*^decal_t::FromV2*/^decal_t::FlipX;
        //if (min2(dcy0, dcy1) >= max2(ffloorZ, bfloorZ) || max2(dcy0, dcy1) <= min2(fceilingZ, bceilingZ))
        if (dcy1 > bfloorZ && dcy0 < bceilingZ) {
          // touching midtex
          dcp->flags &= ~decal_t::NoMidTex;
        } else {
          dcp->flags |= decal_t::NoMidTex;
        }
        dcp->slidesec = decal->slidesec;
        dcp->calculateBBox();
        if (!dcp->isPermanent()) CleanupSegDecals(seg->partner);
      }
      if (!decal->isPermanent()) CleanupSegDecals(seg);
    }
  }


  // if our decal is not completely at linedef, spread it to adjacent linedefs
  const float dstxofs = dcx0+txofs;
  const int clnum = connectedLines.length();

  if (dcx0 < 0.0f) {
    // to the left
    VDC_DLOG(NAME_Debug, "Decal '%s' at line #%d: going to the left; ofs=%g; side=%d", *dec->name, (int)(ptrdiff_t)(li-Lines), dcx0, side);
    line_t **ngb = li->v1lines;
    for (int ngbCount = li->v1linesCount; ngbCount--; ++ngb) {
      line_t *nline = *ngb;
      if (IsLineTouched(nline)) continue;
      // determine correct decal side
      const int nside = (nline->v2 != li->v1); // see below
      // spread check: do not go to another sector through closed lines
      if (!IsWallSpreadAllowed(this, fsec, li, 0, nline, nside)) continue;
      MarkLineTouched(nline);
      DecalLineInfo &dlc = connectedLines.alloc();
      dlc.nline = nline;
      dlc.isv1 = true;
      dlc.nside = nside;
      dlc.isbackside = (nside == 1);
      /*
      // determine correct decal side
      if (nline->v2 == li->v1) {
        // nline has the same orientation, so the same side
        dlc.nside = 0;
        dlc.isbackside = false;
      } else {
        // nline is differently oriented, flip side
        dlc.nside = 1;
        dlc.isbackside = true;
      }
      */
    }
  }

  if (dcx1 > linelen) {
    // to the right
    VDC_DLOG(NAME_Debug, "Decal '%s' at line #%d: going to the right; left=%g; side=%d", *dec->name, (int)(ptrdiff_t)(li-Lines), dcx1-linelen, side);
    line_t **ngb = li->v2lines;
    for (int ngbCount = li->v2linesCount; ngbCount--; ++ngb) {
      line_t *nline = *ngb;
      if (IsLineTouched(nline)) continue;
      const int nside = (li->v2 != nline->v1); // see below
      if (!IsWallSpreadAllowed(this, fsec, li, 1, nline, nside)) continue;
      MarkLineTouched(nline);
      DecalLineInfo &dlc = connectedLines.alloc();
      dlc.nline = nline;
      dlc.isv1 = false;
      dlc.nside = nside;
      dlc.isbackside = (nside == 1);
      /*
      // determine correct decal side
      if (li->v2 == nline->v1) {
        // nline has the same orientation, so the same side
        dlc.nside = 0;
        dlc.isbackside = false;
      } else {
        // nline is differently oriented, flip side
        dlc.nside = 1;
        dlc.isbackside = true;
      }
      */
    }
  }

  // put decals
  // cannot use iterator here, because `connectedLines()` may grow
  const int clend = connectedLines.length();
  for (int cc = clnum; cc < clend; ++cc) {
    DecalLineInfo *dlc = connectedLines.ptr()+cc;
    if (dlc->isv1) {
      if (!dlc->isbackside) {
        // normal continuation
        // decal offset is dcx0, and it is negative
        // we need to fix it for `v1`
        const float nofs = dlc->nline->dir.length2D()+dstxofs;
        VDC_DLOG(NAME_Debug, ":::v1f: %d (nside=%d; argside=%d; dstxofs=%g; dcx=(%g : %g); twdt=%g; nofs=%g)", (int)(ptrdiff_t)(dlc->nline-&Lines[0]), dlc->nside, (dlc->nline->frontsector == fsec ? 0 : 1), dstxofs, dcx0, dcx1, twdt, nofs);
        PutDecalAtLine(org, nofs, dec, dlc->nside, dlc->nline, flips, translation, shadeclr, alpha, animator, angle, true); // skip mark check
      } else {
        // reversed continuation
        const float nofs = -dcx0-twdt+txofs;
        VDC_DLOG(NAME_Debug, ":::v1b: %d (nside=%d; argside=%d; dstxofs=%g; dcx=(%g : %g); twdt=%g; nofs=%g)", (int)(ptrdiff_t)(dlc->nline-&Lines[0]), dlc->nside, (dlc->nline->frontsector == fsec ? 0 : 1), dstxofs, dcx0, dcx1, twdt, nofs);
        PutDecalAtLine(org, nofs, dec, dlc->nside, dlc->nline, flips/*^decal_t::FlipX*/, translation, alpha, shadeclr, animator, angle, true); // skip mark check
      }
    } else {
      if (!dlc->isbackside) {
        const float nofs = dstxofs-linelen;
        VDC_DLOG(NAME_Debug, ":::v2f: %d (nside=%d; argside=%d; dstxofs=%g; dcx=(%g : %g); twdt=%g; nofs=%g)", (int)(ptrdiff_t)(dlc->nline-&Lines[0]), dlc->nside, (dlc->nline->frontsector == fsec ? 0 : 1), dstxofs, dcx0, dcx1, twdt, nofs);
        PutDecalAtLine(org, nofs, dec, dlc->nside, dlc->nline, flips, translation, shadeclr, alpha, animator, angle, true); // skip mark check
      } else {
        const float nofs = dlc->nline->dir.length2D()-(dcx1-linelen)+txofs;
        VDC_DLOG(NAME_Debug, ":::v2b: %d (nside=%d; argside=%d; dstxofs=%g; dcx=(%g : %g); twdt=%g; nofs=%g)", (int)(ptrdiff_t)(dlc->nline-&Lines[0]), dlc->nside, (dlc->nline->frontsector == fsec ? 0 : 1), dstxofs, dcx0, dcx1, twdt, nofs);
        PutDecalAtLine(org, nofs, dec, dlc->nside, dlc->nline, flips/*^decal_t::FlipX*/, translation, shadeclr, alpha, animator, angle, true); // skip mark check
      }
    }
  }

  connectedLines.setLength(clnum, false); // don't realloc
}


//==========================================================================
//
//  VLevel::AddOneDecal
//
//==========================================================================
void VLevel::AddOneDecal (int level, TVec org, VDecalDef *dec, int side, line_t *li,
                          int translation, int shadeclr, float alpha, VDecalAnim *animator,
                          bool permanent, float angle, bool forceFlipX)
{
  if (!dec || !li) return;

  if (dec->noWall) return;

  if (level > 16) {
    GCon->Logf(NAME_Warning, "too many lower decals '%s'", *dec->name);
    return;
  }

  VDC_DLOG(NAME_Debug, "DECAL: line #%d; side #%d", (int)(ptrdiff_t)(li-&Lines[0]), side);

  if (dec->lowername != NAME_None) {
    //GCon->Logf(NAME_Debug, "adding lower decal '%s' for decal '%s' (level %d)", *dec->lowername, *dec->name, level);
    AddDecal(org, dec->lowername, side, li, level+1, translation, shadeclr, alpha, animator, permanent, angle, forceFlipX);
  }

  // generate decal values
  dec->angleWall = 0.0f; // cannot rotate wall decals yet

  int tex = dec->texid;
  VTexture *dtex = GTextureManager[tex];
  if (!dtex || dtex->Type == TEXTYPE_Null || dtex->GetWidth() < 1 || dtex->GetHeight() < 1) {
    // no decal gfx, nothing to do
    if (!baddecals.put(dec->name, true)) GCon->Logf(NAME_Warning, "Decal '%s' has no pic", *dec->name);
    return;
  }

  // actually, we should check animator here, but meh...
  if (!dec->animator && !animator && dec->alpha.value < 0.004f) {
    if (!baddecals.put(dec->name, true)) GCon->Logf(NAME_Warning, "Decal '%s' has zero alpha", *dec->name);
    return;
  }

  dec->CalculateWallBBox(org.x, org.y);
  //GCon->Logf(NAME_Debug, "decal '%s': scale=(%g:%g)", *dec->name, dec->scaleX.value, dec->scaleY.value);

  if (dec->spheight == 0.0f || dec->bbWidth() < 1.0f || dec->bbHeight() < 1.0f) {
    if (!baddecals.put(dec->name, true)) GCon->Logf(NAME_Warning, "Decal '%s' has zero size", *dec->name);
    return;
  }

  //FIXME: change spreading code!
  if (dec->scaleX.value <= 0.0f || dec->scaleY.value <= 0.0f) {
    if (!baddecals.put(dec->name, true)) GCon->Logf(NAME_Warning, "Decal '%s' has invalid scale", *dec->name);
    return;
  }

  const float twdt = dtex->GetScaledWidthF()*dec->scaleX.value;
  const float thgt = dtex->GetScaledHeightF()*dec->scaleY.value;

  //FIXME: change spreading code!
  if (twdt < 1.0f || thgt < 1.0f) {
    // too small
  }

  //GCon->Logf(NAME_Debug, "Decal '%s', texture '%s'", *dec->name, *dtex->Name);

  IncLineTouchCounter();

  // setup flips
  unsigned flips =
    (dec->flipXValue ? decal_t::FlipX : 0u)|
    (dec->flipYValue ? decal_t::FlipY : 0u)|
    (permanent ? decal_t::Permanent : 0u);
  if (forceFlipX) flips ^= decal_t::FlipX;

  // calculate offset from line start
  const TVec &v1 = *li->v1;
  const TVec &v2 = *li->v2;

  const float dx = v2.x-v1.x;
  const float dy = v2.y-v1.y;
  float dist; // distance from wall start
       if (fabsf(dx) > fabsf(dy)) dist = (org.x-v1.x)/dx;
  else if (dy != 0.0f) dist = (org.y-v1.y)/dy;
  else dist = 0.0f;

  const float lineofs = dist*(v2-v1).length2D();
  VDC_DLOG(NAME_Debug, "linelen=%g; dist=%g; lineofs=%g", (v2-v1).length2D(), dist, lineofs);

  connectedLines.resetNoDtor();
  PutDecalAtLine(org, lineofs, dec, side, li, flips, translation, shadeclr, alpha, animator, angle, false); // don't skip mark check
}


//==========================================================================
//
//  VLevel::AddDecal
//
//==========================================================================
void VLevel::AddDecal (TVec org, VName dectype, int side, line_t *li, int level,
                       int translation, int shadeclr, float alpha, VDecalAnim *animator, bool permanent,
                       float angle, bool forceFlipX)
{
  if (!r_decals || !r_decals_wall) return;
  if (!li || dectype == NAME_None || VStr::strEquCI(*dectype, "none")) return; // just in case

  //GCon->Logf(NAME_Debug, "%s: oorg:(%g,%g,%g); org:(%g,%g,%g); trans=%d", *dectype, org.x, org.y, org.z, li->landAlongNormal(org).x, li->landAlongNormal(org).y, li->landAlongNormal(org).z, translation);

#ifdef VAVOOM_DECALS_DEBUG_REPLACE_PICTURE
  //dectype = VName("k8TestDecal");
  dectype = VName("k8TestDecal001");
#endif
  VDecalDef *dec = VDecalDef::getDecal(dectype);
  //if (dec->animator) GCon->Logf(NAME_Debug, "   animator: <%s> (%s : %d)", *dec->animator->name, dec->animator->getTypeName(), (int)dec->animator->isEmpty());
  //if (animator) GCon->Logf(NAME_Debug, "   forced animator: <%s> (%s : %d)", *animator->name, animator->getTypeName(), (int)animator->isEmpty());
  if (dec) {
    org = li->landAlongNormal(org);
    //GCon->Logf(NAME_Debug, "DECAL '%s'; name is '%s', texid is %d; org=(%g,%g,%g)", *dectype, *dec->name, dec->texid, org.x, org.y, org.z);
    AddOneDecal(level, org, dec, side, li, translation, shadeclr, alpha, animator, permanent, angle, forceFlipX);
  } else {
    if (!baddecals.put(dectype, true)) GCon->Logf(NAME_Warning, "NO DECAL: '%s'", *dectype);
  }
}


//==========================================================================
//
//  VLevel::AddDecalById
//
//==========================================================================
void VLevel::AddDecalById (TVec org, int id, int side, line_t *li, int level,
                           int translation, int shadeclr, float alpha, VDecalAnim *animator, bool permanent)
{
  if (!r_decals || !r_decals_wall) return;
  if (!li || id < 0) return; // just in case
  VDecalDef *dec = VDecalDef::getDecalById(id);
  if (dec) {
    org = li->landAlongNormal(org);
    AddOneDecal(level, org, dec, side, li, translation, shadeclr, alpha, animator, true, INFINITY, false);
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
//  VLevel::NewFlatDecal
//
//==========================================================================
void VLevel::NewFlatDecal (bool asFloor, subsector_t *sub, const int eregidx, const float wx, const float wy,
                           VDecalDef *dec, int translation, int shadeclr, float alpha, VDecalAnim *animator,
                           unsigned orflags, float angle)
{
  vassert(sub);
  vassert(eregidx >= 0);
  vassert(dec);

  float dcalpha = dec->alpha.value;
  if (alpha >= 0.0f) {
    //GCon->Logf(NAME_Debug, "decal '%s': orig alpha=%g (forced alpha=%g)", *dec->name, dcalpha, alpha);
    if (alpha < 1000.0f) dcalpha = alpha; else dcalpha *= alpha-1000.0f;
    //GCon->Logf(NAME_Debug, "decal '%s': final alpha=%g", *dec->name, dcalpha);
    if (dcalpha < 0.004f) return;
    dcalpha = min2(1.0f, dcalpha);
    //GCon->Logf(NAME_Debug, "decal '%s': final clamped alpha=%g", *dec->name, dcalpha);
  }

  decal_t *decal = new decal_t;
  memset((void *)decal, 0, sizeof(decal_t));
  //decal->dectype = dec->name;
  decal->proto = dec;
  decal->texture = dec->texid;
  decal->translation = translation;
  decal->shadeclr = decal->origshadeclr = dec->shadeclr;
  if (shadeclr != -2) decal->shadeclr = decal->origshadeclr = shadeclr;
  decal->slidesec = nullptr;
  decal->sub = sub;
  decal->eregindex = eregidx;
  decal->dcsurf = (asFloor ? decal_t::Floor : decal_t::Ceiling);
  decal->worldx = wx;
  decal->worldy = wy;
  decal->angle = AngleMod(angle);
  //decal->orgz = org.z; // doesn't matter
  //!decal->height = height;
  //decal->curz = 0.0f; // doesn't matter
  //decal->xdist = 0.0f; // doesn't matter
  //decal->ofsX = decal->ofsY = 0.0f;
  decal->scaleX = decal->origScaleX = dec->scaleX.value;
  decal->scaleY = decal->origScaleY = dec->scaleY.value;
  decal->alpha = decal->origAlpha = dcalpha;
  decal->addAlpha = dec->addAlpha.value;
  decal->flags =
    orflags|
    (dec->fullbright ? decal_t::Fullbright : 0u)|
    (dec->fuzzy ? decal_t::Fuzzy : 0u)|
    (dec->bloodSplat ? decal_t::BloodSplat : 0u)|
    (dec->bootPrint ? decal_t::BootPrint : 0u);

  decal->boottime = dec->boottime.value;
  decal->bootanimator = dec->bootanimator;
  decal->bootshade = dec->bootshade;
  decal->boottranslation = dec->boottranslation;
  decal->bootalpha = dec->bootalpha;

  decal->animator = (animator ? animator : dec->animator);
  //if (decal->animator) GCon->Logf(NAME_Debug, "anim: %s(%s) (%d)", *decal->animator->name, decal->animator->getTypeName(), (int)decal->animator->isEmpty());
  if (decal->animator && decal->animator->isEmpty()) decal->animator = nullptr;
  //decal->animator = nullptr;
  if (decal->animator) decal->animator = decal->animator->clone();

  AppendDecalToSubsectorList(decal);
}


//==========================================================================
//
//  VLevel::AddFlatDecal
//
//  z coord matters!
//  `height` is from `org.z`
//  zero height means "take from decal texture"
//
//==========================================================================
void VLevel::AddFlatDecal (TVec org, VName dectype, float range, int translation, int shadeclr, float alpha,
                           VDecalAnim *animator, float angle, bool angleOverride, bool forceFlipX)
{
  if (!r_decals || !r_decals_flat) return;
  if (dectype == NAME_None || VStr::strEquCI(*dectype, "none")) return; // just in case

  VDecalDef *dec = VDecalDef::getDecal(dectype);
  if (!dec) {
    if (!baddecals.put(dectype, true)) GCon->Logf(NAME_Warning, "NO DECAL: '%s'", *dectype);
    return;
  }

  range = max2(2.0f, fabsf(range));
  SpreadFlatDecalEx(org, range, dec, 0, translation, shadeclr, alpha, animator, angle, angleOverride, forceFlipX);
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
  VOptParamFloat angle(0.0f);
  VOptParamBool forceFlipX(false);
  vobjGetParamSelf(org, dectype, side, li, translation, shadeclr, alpha, animator, permanent, angle, forceFlipX);
  if (!angle.specified) angle.value = INFINITY;
  Self->AddDecal(org, dectype, side, li, 0, translation, shadeclr, alpha, VDecalAnim::GetAnimatorByName(animator.value), permanent, angle, forceFlipX);
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
  Self->AddDecalById(org, id, side, li, 0, translation, shadeclr, alpha, VDecalAnim::GetAnimatorByName(animator.value), true);
}


//native final void AddFlatDecal (TVec org, name dectype, float range, optional int translation, optional int shadeclr, optional float alpha,
//                                optional name animator, optional float angle, optional bool forceFlipX);
IMPLEMENT_FUNCTION(VLevel, AddFlatDecal) {
  TVec org;
  VName dectype;
  float range;
  VOptParamInt translation(0);
  VOptParamInt shadeclr(-2);
  VOptParamFloat alpha(-2.0f);
  VOptParamName animator(NAME_None);
  VOptParamFloat angle(0.0f);
  VOptParamBool forceFlipX(false);
  vobjGetParamSelf(org, dectype, range, translation, shadeclr, alpha, animator, angle, forceFlipX);
  Self->AddFlatDecal(org, dectype, range, translation, shadeclr, alpha, VDecalAnim::GetAnimatorByName(animator.value), angle, angle.specified, forceFlipX);
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
