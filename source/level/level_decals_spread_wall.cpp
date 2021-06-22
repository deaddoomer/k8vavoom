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
#include "../psim/p_decal.h"
#include "../psim/p_levelinfo.h"
#include "beamclip.h"

//#define VAVOOM_DECALS_DEBUG

#ifdef VAVOOM_DECALS_DEBUG
# define VDC_DLOG  GCon->Logf
#else
# define VDC_DLOG(...)  do {} while (0)
#endif


enum {
  LNSPEC_TeleportNewMap = 74,
  LNSPEC_TeleportEndGame = 75,
  LNSPEC_ExitNormal = 243,
  LNSPEC_ExitSecret = 244,
};


extern VCvarB r_decals;
extern VCvarB r_decals_wall;

TArray<VLevel::DecalLineInfo> VLevel::connectedLines;


//==========================================================================
//
//  IsSpecialLine
//
//==========================================================================
static inline bool IsSpecialLine (const line_t *line, const int sidenum, const int texid) noexcept {
  // has special?
  if (!line || !line->special || texid <= 0) return false;
  //GCon->Logf("linespc=%d; spac=0x%04x; texid=%d (%d)", line->special, (unsigned)line->SpacFlags, texid, (int)VLevelInfo::IsSwitchTexture(texid));
  // can use?
  if ((line->SpacFlags&(SPAC_Use|SPAC_Impact|SPAC_Push|SPAC_UseThrough)) == 0) return false;
  // backside?
  //if (sidenum == 1 && (line->SpacFlags&SPAC_UseBack) == 0) return false;
  switch (line->special) {
    case LNSPEC_TeleportNewMap:
    case LNSPEC_TeleportEndGame:
    case LNSPEC_ExitNormal:
    case LNSPEC_ExitSecret:
      // always
      return true;
  }
  // check for the switch
  return VLevelInfo::IsSwitchTexture(texid);
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
//  VLevel::PutDecalAtLine
//
//  `flips` will be bitwise-ored with decal flags
//
//==========================================================================
void VLevel::PutDecalAtLine (const TVec &aorg, float lineofs, VDecalDef *dec, int side, line_t *li,
                             DecalParams &params, bool skipMarkCheck)
{
  // don't process linedef twice
  if (!skipMarkCheck) {
    if (IsLineTouched(li)) return;
    MarkLineTouched(li);
  }

  // for offset
  VTexture *dtex = GTextureManager[dec->texid];
  //if (!dtex || dtex->Type == TEXTYPE_Null) return;

  //const float twdt = dtex->GetScaledWidthF()*dec->scaleX.value;
  //const float thgt = dtex->GetScaledHeightF()*dec->scaleY.value;

  //GCon->Logf(NAME_Debug, "decal '%s' at line #%d: size=(%g,%g) : (%g,%g)", *dec->name, (int)(ptrdiff_t)(li-&Lines[0]), twdt, thgt, dec->bbWidth(), dec->bbHeight());

  const float twdt = dec->bbWidth();
  const float thgt = dec->bbHeight();

  if (twdt < 1.0f || thgt < 1.0f) return;

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

  TVec org = aorg;

  #if 0
  // if our side has no textures, but the other side has some, switch sides
  if (sidedef && fsec && bsec &&
      (sidedef->MidTexture <= 0 || GTextureManager(sidedef->MidTexture)->Type == TEXTYPE_Null) &&
      (sidedef->TopTexture <= 0 || GTextureManager(sidedef->TopTexture)->Type == TEXTYPE_Null) &&
      (sidedef->BottomTexture <= 0 || GTextureManager(sidedef->BottomTexture)->Type == TEXTYPE_Null))
  {
    sector_t *tmps = fsec;
    fsec = bsec;
    bsec =tmps;
    side ^= 1;
    sidedef = (li->sidenum[side] >= 0 ? &Sides[li->sidenum[side]] : nullptr);
    //params.orflags ^= decal_t::FlipX;
    //const float llen = ((*li->v2)-(*li->v1)).length2D();
    //lineofs = llen-lineofs;
    float ang = AngleMod(isFiniteF(params.angle) ? params.angle : dec->angleWall.value);
    if (ang != 0.0f) {
      //ang = AngleMod(-ang);
      //params.angle = ang;
      float s, c;
      msincos(ang, &s, &c);
      //taxis = TVec(s*seg->dir.x, s*seg->dir.y, -c);
      //saxis = Normalise(CrossProduct(seg->normal, taxis));
      //org.z -= c;
      GCon->Logf(NAME_Debug, "decal '%s' at line #%d: size=(%g,%g); s=%g; c=%g", *dec->name, (int)(ptrdiff_t)(li-&Lines[0]), twdt, thgt, s, c);
      //org.x += 4;
      //org.z += 1;
      //lineofs += twdt*s;
      lineofs += 8;
    }
  }
  #endif

  // side 1 will have decals flipped, so flip them to make it look right
  const unsigned xflipside = (side ? decal_t::FlipX : 0u);

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

  const float orgz = org.z;

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
            const float swalpha = (IsSpecialLine(reg->extraline, 0, extraside->MidTexture) ? CalcSwitchDecalAlpha(dec, params.alpha) : params.alpha);
            VDC_DLOG(NAME_Debug, " HIT solid region: fz=%g; cz=%g; orgz=%g; dcy0=%g; dcy1=%g", fz, cz, orgz, dcy0, dcy1);
            // create decal
            decal_t *decal = AllocSegDecal(seg, dec, swalpha, params.animator);
            if (isFiniteF(params.angle)) decal->angle = AngleMod(params.angle);
            decal->translation = params.translation;
            if (params.shadeclr != -2) decal->shadeclr = decal->origshadeclr = params.shadeclr;
            decal->orgz = decal->curz = orgz;
            decal->xdist = lineofs;
            // setup misc flags
            decal->flags |= (params.orflags|stvflag)^xflipside;
            decal->flags |= /*disabledTextures*/0u;
            // slide with 3d floor floor
            decal->slidesec = sec3d;
            decal->flags |= decal_t::SlideFloor;
            decal->curz -= decal->slidesec->floor.TexZ;
            decal->calculateBBox();
            if (doParnter) {
              // create decal on partner seg
              decal_t *dcp = AllocSegDecal(seg->partner, dec, swalpha, params.animator);
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
          } else if (!hasSliderAnimator(dec, params.animator) && dcy0 >= max2(ffloorZ, bfloorZ)) {
            // if decal bottom is higher than highest floor, consider toptex invisible
            // (but don't do this for animated decals -- this may be sliding blood)
            allowBotTex = false;
          }
        } else {
          // one-sided: see the last coment above
          if (!hasSliderAnimator(dec, params.animator) && dcy0 >= ffloorZ) allowBotTex = false;
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
          onSwitch = IsSpecialLine(li, side, seg->sidedef->MidTexture);
        } else {
          if (allowTopTex && allowBotTex) {
            // both top and bottom
            if (orgz < max2(ffloorZ, bfloorZ)) {
              // bottom texture
              if ((li->flags&ML_DONTPEGBOTTOM) == 0) slideWithFloor = true;
              onSwitch = IsSpecialLine(li, side, seg->sidedef->BottomTexture);
            } else if (orgz > min2(fceilingZ, bceilingZ)) {
              // top texture
              if ((li->flags&ML_DONTPEGTOP) == 0) slideWithCeiling = true;
              onSwitch = IsSpecialLine(li, side, seg->sidedef->TopTexture);
            }
          } else if (allowBotTex) {
            // only bottom texture
            if ((li->flags&ML_DONTPEGBOTTOM) == 0) slideWithFloor = true;
            onSwitch = IsSpecialLine(li, side, seg->sidedef->BottomTexture);
          } else if (allowTopTex) {
            // only top texture
            if ((li->flags&ML_DONTPEGTOP) == 0) slideWithCeiling = true;
            onSwitch = IsSpecialLine(li, side, seg->sidedef->TopTexture);
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
          onSwitch = IsSpecialLine(li, side, seg->sidedef->MidTexture);
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

      const float swalpha = (onSwitch ? CalcSwitchDecalAlpha(dec, params.alpha) : params.alpha);

      VDC_DLOG(NAME_Debug, "  decaling seg #%d; offset=%g; length=%g; onSwitch=%d; swalpha=%g", (int)(ptrdiff_t)(seg-Segs), seg->offset, seg->length, (int)onSwitch, swalpha);

      // create decal
      decal_t *decal = AllocSegDecal(seg, dec, swalpha, params.animator);
      if (isFiniteF(params.angle)) decal->angle = AngleMod(params.angle);
      decal->translation = params.translation;
      if (params.shadeclr != -2) decal->shadeclr = decal->origshadeclr = params.shadeclr;
      decal->orgz = decal->curz = orgz;
      decal->xdist = lineofs;
      // setup misc flags
      decal->flags |= (params.orflags|stvflag)^xflipside;
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
        decal_t *dcp = AllocSegDecal(seg->partner, dec, swalpha, params.animator);
        dcp->angle = decal->angle;
        dcp->translation = decal->translation;
        if (params.shadeclr != -2) dcp->shadeclr = dcp->origshadeclr = params.shadeclr;
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
        PutDecalAtLine(org, nofs, dec, dlc->nside, dlc->nline, params, true); // skip mark check
      } else {
        // reversed continuation
        const float nofs = -dcx0-twdt+txofs;
        VDC_DLOG(NAME_Debug, ":::v1b: %d (nside=%d; argside=%d; dstxofs=%g; dcx=(%g : %g); twdt=%g; nofs=%g)", (int)(ptrdiff_t)(dlc->nline-&Lines[0]), dlc->nside, (dlc->nline->frontsector == fsec ? 0 : 1), dstxofs, dcx0, dcx1, twdt, nofs);
        PutDecalAtLine(org, nofs, dec, dlc->nside, dlc->nline, params, true); // skip mark check
      }
    } else {
      if (!dlc->isbackside) {
        const float nofs = dstxofs-linelen;
        VDC_DLOG(NAME_Debug, ":::v2f: %d (nside=%d; argside=%d; dstxofs=%g; dcx=(%g : %g); twdt=%g; nofs=%g)", (int)(ptrdiff_t)(dlc->nline-&Lines[0]), dlc->nside, (dlc->nline->frontsector == fsec ? 0 : 1), dstxofs, dcx0, dcx1, twdt, nofs);
        PutDecalAtLine(org, nofs, dec, dlc->nside, dlc->nline, params, true); // skip mark check
      } else {
        const float nofs = dlc->nline->dir.length2D()-(dcx1-linelen)+txofs;
        VDC_DLOG(NAME_Debug, ":::v2b: %d (nside=%d; argside=%d; dstxofs=%g; dcx=(%g : %g); twdt=%g; nofs=%g)", (int)(ptrdiff_t)(dlc->nline-&Lines[0]), dlc->nside, (dlc->nline->frontsector == fsec ? 0 : 1), dstxofs, dcx0, dcx1, twdt, nofs);
        PutDecalAtLine(org, nofs, dec, dlc->nside, dlc->nline, params, true); // skip mark check
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
void VLevel::AddOneDecal (int level, TVec org, VDecalDef *dec, int side, line_t *li, DecalParams &params) {
  if (!dec || !li) return;

  if (dec->noWall) return;

  if (level > 16) {
    GCon->Logf(NAME_Warning, "too many lower decals '%s'", *dec->name);
    return;
  }

  VDC_DLOG(NAME_Debug, "DECAL: line #%d; side #%d", (int)(ptrdiff_t)(li-&Lines[0]), side);

  // calculate it here, so lower decals will have the same angle too
  params.angle = AngleMod(isFiniteF(params.angle) ? params.angle : dec->angleWall.value);

  if (dec->lowername != NAME_None) {
    //GCon->Logf(NAME_Debug, "adding lower decal '%s' for decal '%s' (level %d)", *dec->lowername, *dec->name, level);
    AddDecal(org, dec->lowername, side, li, level+1, params);
  }

  // generate decal values
  //dec->angleWall = 0.0f; // cannot rotate wall decals yet

  int tex = dec->texid;
  VTexture *dtex = GTextureManager[tex];
  if (!dtex || dtex->Type == TEXTYPE_Null || dtex->GetWidth() < 1 || dtex->GetHeight() < 1) {
    // no decal gfx, nothing to do
    if (!baddecals.put(dec->name, true)) GCon->Logf(NAME_Warning, "Decal '%s' has no pic", *dec->name);
    return;
  }

  // actually, we should check animator here, but meh...
  if (!dec->animator && !params.animator && dec->alpha.value < 0.004f) {
    if (!baddecals.put(dec->name, true)) GCon->Logf(NAME_Warning, "Decal '%s' has zero alpha", *dec->name);
    return;
  }

  dec->CalculateBBox(org.x, org.y, params.angle);
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
  params.orflags |=
    (dec->flipXValue ? decal_t::FlipX : 0u)|
    (dec->flipYValue ? decal_t::FlipY : 0u)|
    (params.forcePermanent ? decal_t::Permanent : 0u);
  if (params.forceFlipX) params.orflags ^= decal_t::FlipX;

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
  VDC_DLOG(NAME_Debug, "  linelen=%g; dist=%g; lineofs=%g; flip=0x%04x", (v2-v1).length2D(), dist, lineofs, (unsigned)(params.orflags&(decal_t::FlipX|decal_t::FlipY)));

  connectedLines.resetNoDtor();
  PutDecalAtLine(org, lineofs, dec, side, li, params, false); // don't skip mark check
}
