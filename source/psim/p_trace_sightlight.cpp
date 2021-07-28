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
#include "p_trace_internal.h"


//**************************************************************************
//
//  blockmap light/sight tracing
//
//  this basically does ray/convex intersection tests.
//
//  because our convex are guaranteed to be... well, convex, we can cheat
//  and avoid "point in poly" checks.
//
//  the basic algorithm is like this:
//
//  first, we collecting all ray-line intersections into sorted array.
//  it is important to have this array sorted.
//
//  second, we walk this array, and check each "front side" sector planes.
//  "front side" here means "sector at the front of the line looking from
//  the starting point".
//
//  because our list is sorted, we can check if ray part from the previous
//  line hitpoint to the current line hitpoint intersects any sector plane.
//  it is guaranteed that this ray part is fully inside the sector we're
//  checking.
//
//  the same thing is done for 3d pobjs, but here we should check the ray
//  part between the previous 3d pobj line and the current one.
//
//  one exception: if our starting and ending points are inside the same 3d
//  pobj, we may hit no pobj lines; this can be optimised, but currently
//  i am simply doing the full check.
//
//**************************************************************************
static InterceptionList intercepts;


// ////////////////////////////////////////////////////////////////////////// //
struct SightTraceInfo {
  // the following should be set
  TVec Start;
  TVec End;
  const subsector_t *StartSubSector;
  const subsector_t *EndSubSector;
  VLevel *Level;
  unsigned LineBlockMask;
  unsigned PlaneNoBlockFlags;
  bool lightCheck; // sight or light?
  bool flatTextureCheck;
  bool wallTextureCheck;
  bool collectIntercepts; // set if we'll do "better sight" checks
  bool wasBlocked; // for `collectIntercepts`
  // the following are working vars, and should not be set
  bool Hit1S; // `true` means "hit one-sided wall"
  TVec Delta; // End-Start
  TPlane Plane; // vertical plane for Start..End line

  SightTraceInfo () = delete;

  inline SightTraceInfo (VLevel *alevel, const TVec &org, const TVec &dest, const subsector_t *sstart, const subsector_t *send) noexcept {
    memset((void *)this, 0, sizeof(SightTraceInfo));
    Level = alevel;
    Start = org;
    End = dest;
    #if 0
    // use buggy vanilla algo here, because this is what used for world linking
    StartSubSector = (sstart ?: alevel->PointInSubsector_Buggy(org));
    EndSubSector = (send ?: alevel->PointInSubsector_Buggy(dest));
    #else
    // nope, use correct code here
    StartSubSector = (sstart ?: alevel->PointInSubsector(org));
    EndSubSector = (send ?: alevel->PointInSubsector(dest));
    #endif
  }
};


//==========================================================================
//
//  SightPassRegionPlaneTexture
//
//  usually, `End` is light origin
//
//  returns `true` if no hit was detected
//
//==========================================================================
static bool SightPassRegionPlaneTexture (SightTraceInfo &trace, const sec_region_t *reg, const TSecPlaneRef &spl, const float frac) {
  if (!trace.lightCheck || !trace.flatTextureCheck) return false; // blocked

  //const TSecPlaneRef &spl = (checkFloor ? reg->efloor : reg->eceiling);
  if ((spl.splane->flags&SPF_ADDITIVE) || spl.splane->Alpha < 1.0f) return true; // not blocked
  if (GTextureManager.IsSightBlocking(spl.splane->pic)) return false; // blocked

  VTexture *FTex = GTextureManager(spl.splane->pic);
  if (!FTex || FTex->Type == TEXTYPE_Null) return true; // not blocked

  const int thgt = FTex->GetHeight();
  if (thgt < 1) return true; // just in case, not blocked
  const int twdt = FTex->GetWidth();
  if (twdt < 1) return true; // just in case, not blocked

  //TODO: rotated textures
  //FIXME: call render function for this!

  TVec saxis, taxis;

  if (fabsf(spl.splane->normal.z) > 0.1f) {
    float s, c;
    msincos(spl.splane->BaseAngle-spl.splane->Angle, &s, &c);
    //k8: do we really need to rotate lightmap axes? i don't think so
    saxis = TVec(c,  s)*(FTex->TextureSScale()*spl.splane->XScale);
    taxis = TVec(s, -c)*(FTex->TextureTScale()*spl.splane->YScale);
  } else {
    const TVec taxisLM = TVec(0.0f, 0.0f, -1.0f);
    const TVec saxisLM = Normalise(CrossProduct(spl.GetNormal(), taxisLM));
    taxis = taxisLM*(FTex->TextureTScale()*spl.splane->YScale);
    saxis = saxisLM*(FTex->TextureSScale()*spl.splane->XScale);
  }

  //k8: i think that this is not completely right
  if (spl.splane->flipFlags&SPF_FLIP_X) saxis = -saxis;
  if (spl.splane->flipFlags&SPF_FLIP_Y) taxis = -taxis;

  // /*bool offsChanged = */SurfRecalcFlatOffset(ssurf, spl, Tex);

  //k8: should flat texture offsets be scaled? seems that wall texture offsets aren't.
  float soffs = (spl.splane->xoffs+spl.splane->BaseXOffs)*(FTex->TextureSScale()*spl.splane->XScale);
  float toffs = (spl.splane->yoffs+spl.splane->BaseYOffs)*(FTex->TextureTScale()*spl.splane->YScale);

  //k8: i think that this is not completely right
  if (spl.splane->flipFlags&SPF_FLIP_X) soffs = -soffs;
  if (spl.splane->flipFlags&SPF_FLIP_Y) toffs = -toffs;

  const float cx = spl.splane->PObjCX;
  const float cy = spl.splane->PObjCY;
  if (cx || cy) {
    TVec p(cx, cy);
    soffs -= DotProduct(saxis, p);
    toffs -= DotProduct(taxis, p);
  }

  const TVec hitPoint = trace.Start+trace.Delta*frac;

  int texelT = (int)(DotProduct(hitPoint, taxis)+toffs)%thgt;
  if (texelT < 0) texelT += thgt;

  int texelS = (int)(DotProduct(hitPoint, saxis)+soffs)%twdt;
  if (texelS < 0) texelS += twdt;

  // i don't know why i shouldn't reverse Y here
  auto pix = FTex->getPixel(texelS, texelT);
  //GCon->Logf(NAME_Debug, "TEX %s: z=%g; ts=(%d,%d); a=%u", *MTex->Name, lend.z, texelS, texelT, pix.a);
  return (pix.a < 169); // not blocked if the pixel is transparent enough
}


//==========================================================================
//
//  SightPlaneNeedCheck
//
//  returns `true` if plane need to be checked
//
//==========================================================================
static VVA_ALWAYS_INLINE bool SightPlaneNeedCheck (SightTraceInfo &trace, const TSecPlaneRef &spl) noexcept {
  if (!trace.lightCheck) return true; // always check 3d floors
  // light checks
  /*if (trace.lightCheck && trace.flatTextureCheck)*/ {
    if ((spl.splane->flags&SPF_ADDITIVE) || spl.splane->Alpha < 1.0f) return false; // cannot block light
  }
  // only solid textures can block light
  return GTextureManager.IsSightBlocking(spl.splane->pic);
}


//==========================================================================
//
//  CheckPlaneFlags
//
//  returns `true` if plane need to be checked
//
//==========================================================================
static VVA_ALWAYS_INLINE bool CheckPlaneFlags (SightTraceInfo &trace, const TSecPlaneRef &spl, unsigned regflags, unsigned flagmask) noexcept {
  if (!trace.lightCheck) {
    // 3d floor sight check
    unsigned pflags = spl.splane->flags;
    // for non-solid regions, invert flags
    if (regflags&sec_region_t::RF_NonSolid) pflags ^= (SPF_NOBLOCKING|SPF_NOBLOCKSIGHT|SPF_NOBLOCKSHOOT);
    return !(pflags&flagmask);
  } else {
    // light check: skip checking non-solid regions
    return !(regflags&sec_region_t::RF_NonSolid);
  }
}


// helper: check plane hit time
#define CheckPlaneHitInTime(pl_)  do { \
  const float ft = (pl_).IntersectionTime(p0, p1); \
  if (ft > frac0 && ft < frac1) return false; \
} while (0)


//==========================================================================
//
//  SightPassPlanes
//
//  returns `true` if no hit was detected
//
//==========================================================================
static bool SightPassPlanes (SightTraceInfo &trace, sector_t *sector, const float frac0, const float frac1) {
  // sanity checks, just in case
  if (!sector || sector->isAnyPObj()) return true;
  if (frac0 >= frac1) return true;

  //if (sector->eregions->next) GCon->Logf(NAME_Debug, "checking planes of sector #%d", (int)(ptrdiff_t)(sector-&trace.Level->Sectors[0]));

  const TVec p0 = trace.Start;
  const TVec p1 = trace.End;

  const bool ignoreSectorBounds = (!trace.lightCheck && sector == trace.StartSubSector->sector);

  unsigned flagmask = trace.PlaneNoBlockFlags;
  const bool checkFakeFloors = !(flagmask&SPF_IGNORE_FAKE_FLOORS);
  const bool checkSectorBounds = (!ignoreSectorBounds && !(flagmask&SPF_IGNORE_BASE_REGION));
  const bool textureCheck = trace.flatTextureCheck;
  flagmask &= SPF_FLAG_MASK;

  if (checkFakeFloors) {
    // make fake floors and ceilings block view
    sector_t *hs = sector->heightsec;
    if (hs) {
      if (!textureCheck || GTextureManager.IsSightBlocking(hs->floor.pic)) {
        CheckPlaneHitInTime(hs->floor);
      }
      if (!textureCheck || GTextureManager.IsSightBlocking(hs->ceiling.pic)) {
        CheckPlaneHitInTime(hs->ceiling);
      }
    }
  }

  if (checkSectorBounds) {
    // check base sector planes
    CheckPlaneHitInTime(sector->floor);
    CheckPlaneHitInTime(sector->ceiling);
  }

  //if (sector->eregions->next) GCon->Logf(NAME_Debug, "checking 3d regions of sector #%d", (int)(ptrdiff_t)(sector-&trace.Level->Sectors[0]));

  for (const sec_region_t *reg = sector->eregions->next; reg; reg = reg->next) {
    if (reg->regflags&(sec_region_t::RF_BaseRegion|sec_region_t::RF_OnlyVisual/*|sec_region_t::RF_NonSolid*/)) continue;
    if ((reg->regflags&sec_region_t::RF_SkipFloorSurf) == 0) {
      //GCon->Logf(NAME_Debug, "check region of sector #%d (floor)", (int)(ptrdiff_t)(sector-&trace.Level->Sectors[0]));
      if (CheckPlaneFlags(trace, reg->efloor, reg->regflags, flagmask)) {
        //GCon->Log(NAME_Debug, "  ...mask ok");
        if (SightPlaneNeedCheck(trace, reg->efloor)) {
          //GCon->Log(NAME_Debug, "  ...need check");
          const float ft = reg->efloor.IntersectionTime(p0, p1);
          if (ft > frac0 && ft < frac1 && !SightPassRegionPlaneTexture(trace, reg, reg->efloor, ft)) return false;
        }
      }
    }
    if ((reg->regflags&sec_region_t::RF_SkipCeilSurf) == 0) {
      //GCon->Logf(NAME_Debug, "check region of sector #%d (ceiling)", (int)(ptrdiff_t)(sector-&trace.Level->Sectors[0]));
      if (CheckPlaneFlags(trace, reg->eceiling, reg->regflags, flagmask)) {
        //GCon->Log(NAME_Debug, "  ...mask ok");
        if (SightPlaneNeedCheck(trace, reg->eceiling)) {
          //GCon->Log(NAME_Debug, "  ...need check");
          const float ft = reg->eceiling.IntersectionTime(p0, p1);
          if (ft > frac0 && ft < frac1 && !SightPassRegionPlaneTexture(trace, reg, reg->eceiling, ft)) return false;
        }
      }
    }
  }

  return true;
}

#undef CheckPlaneHitInTime


//==========================================================================
//
//  SightPassRegions
//
//  returns `true` if no hit was detected
//
//==========================================================================
static bool SightPassRegions (const sector_t *sec, const TVec point, const unsigned flagmask) {
  for (const sec_region_t *reg = sec->eregions->next; reg; reg = reg->next) {
    if (reg->regflags&(sec_region_t::RF_BaseRegion|sec_region_t::RF_OnlyVisual|sec_region_t::RF_NonSolid)) continue;
    if (((reg->efloor.splane->flags|reg->eceiling.splane->flags)&flagmask) != 0) continue; // bad flags
    // get opening points
    const float fz = reg->efloor.GetPointZClamped(point);
    const float cz = reg->eceiling.GetPointZClamped(point);
    if (fz >= cz) continue; // ignore paper-thin regions
    // if we are inside it, we cannot pass
    if (point.z >= fz && point.z <= cz) return false;
  }
  return true;
}


//==========================================================================
//
//  SightCanPassOpening
//
//  ignore 3d midtex here (for now)
//
//  3d pobj lines will never come here
//
//  returns `true` if no hit was detected
//
//==========================================================================
static bool SightCanPassOpening (const line_t *linedef, const TVec point, const int side, const unsigned flagmask) {
  const sector_t *fsec = linedef->frontsector;
  const sector_t *bsec = linedef->backsector;

  if (!fsec || !bsec) return false;

  // check base region first
  const float ffz = fsec->floor.GetPointZClamped(point);
  const float fcz = fsec->ceiling.GetPointZClamped(point);
  const float bfz = bsec->floor.GetPointZClamped(point);
  const float bcz = bsec->ceiling.GetPointZClamped(point);

  const float pfz = max2(ffz, bfz); // highest floor
  const float pcz = min2(fcz, bcz); // lowest ceiling

  // closed sector?
  // TODO: LIGHT: check for transparent doors here
  if (pfz >= pcz) return false;

  if (point.z < pfz || point.z > pcz) return false;

  // fast algo for two sectors without 3d floors
  if (!fsec->Has3DFloors() && !bsec->Has3DFloors()) {
    // no 3d regions, we're ok
    return true;
  }

  // has 3d floors at least on one side, do full-featured search

  // front sector
  if (!SightPassRegions(fsec, point, flagmask)) return false;
  // back sector
  if (!SightPassRegions(bsec, point, flagmask)) return false;

  // done
  return true;
}


//==========================================================================
//
//  SightPass2SLine
//
//  `iidx` is index in interceptions array
//
//  returns `true` if no hit was detected
//
//==========================================================================
static bool SightPass2SLine (SightTraceInfo &trace, int iidx, const line_t *line, const int side, const float frac) {
  // check for 3d pobj
  polyobj_t *po = line->pobj();

  //GCon->Logf(NAME_Debug, "CHECKING line #%d side #%d (pobj #%d)...", (int)(ptrdiff_t)(line-&trace.Level->Lines[0]), side, (po ? po->tag : -1));

  if (po) {
    if (!po->Is3D()) {
      // 2d polyobject
      if ((line->flags&(ML_TWOSIDED|ML_3DMIDTEX)) == (ML_TWOSIDED|ML_3DMIDTEX)) return true; //FIXME: check texture here
      // 2d polyobject blocks the whole sector
      return false;
    }

    // check easy cases first (totally above or below)
    const float pz0 = po->pofloor.minz;
    const float pz1 = po->poceiling.maxz;
    const float hpz = trace.Start.z+trace.Delta.z*frac;

    // hit pobj line?
    if (hpz >= pz0 && hpz <= pz1) {
      // this line cannot be considered 1s, tho (otherwise it will block additional checks)
      //trace.Hit1S = true;
      return false; // blocked
    }

    // fully below?
    if (trace.Start.z <= pz0 && hpz <= pz0) return true; // cannot hit, not blocked
    // check blocksight
    if (trace.collectIntercepts && ((line->flags&trace.LineBlockMask)&ML_BLOCKSIGHT) && hpz > pz1) {
      const side_t *fsd = &trace.Level->Sides[line->sidenum[0]];
      if (fsd->TopTexture > 0) {
        VTexture *TTex = GTextureManager(fsd->TopTexture);
        if (TTex && TTex->Type != TEXTYPE_Null) {
          const float texh = TTex->GetScaledHeightF()/fsd->Top.ScaleY;
          if (hpz < pz1+texh) {
            // this line cannot be considered 1s, tho (otherwise it will block additional checks)
            //trace.Hit1S = true;
            return false; // blocked
          }
        }
      }
    }
    // fully above?
    if (trace.Start.z >= pz1 && hpz >= pz1) return true; // cannot hit, not blocked

    if (side == 0) {
      // if we're entering a 3d pobj, no need to do more checks
      // but if we have no 3d pobj exit point, perform a check
      // (because it means that we stopped inside the pobj)
      const intercept_t *it = intercepts.list+iidx;
      for (++iidx; iidx < (int)intercepts.count(); ++iidx, ++it) {
        const line_t *exitld = it->line;
        if (exitld && exitld->pobj() == po && it->side == 1) {
          //GCon->Logf(NAME_Debug, "pobj #%d line #%d will be checked on exit", po->tag, (int)(ptrdiff_t)(line-&trace.Level->Lines[0]));
          // will be checked on exit
          return true; // not blocked
        }
      }
      // no exit point, perform planes check
      const float pfc = VLevel::CheckPObjPassPlanes(po, trace.Start, trace.End, nullptr, nullptr, nullptr);
      //GCon->Logf(NAME_Debug, "pobj #%d single enter line #%d, frac=%g; pfc=%g; hit=%d", po->tag, (int)(ptrdiff_t)(line-&trace.Level->Lines[0]), frac, pfc, (int)(pfc > frac && pfc < 1.0f));
      //GCon->Logf(NAME_Debug, "  pobjz: %g,%g; sz=%g; ez=%g", pz0, pz1, trace.Start.z, trace.End.z);
      return !(pfc > frac && pfc < 1.0f);
    } else {
      // we're exiting 3d pobj, need to check pobj planes
      // to do this, we have to find a previous 3d pobj line
      // if plane hit is inside previous frac, and current frac, we hit pobj flat
      // if there is no previous 3d pobj plane, assume that previous frac is 0.0
      const intercept_t *ilist = intercepts.list;
      float prevfrac = 0.0f;
      while (--iidx >= 0) {
        line_t *prevld = ilist[iidx].line;
        if (prevld && prevld->pobj() == po && ilist[iidx].side == 0) {
          // i found her!
          // 3d pobj sector must be valid and closed (i.e. should not contain any extra lines or missing sides)
          prevfrac = ilist[iidx].frac;
          break;
        }
      }

      // implement proper blocking (so we could have transparent polyobjects)
      //unsigned flagmask = trace.PlaneNoBlockFlags;
      //flagmask &= SPF_FLAG_MASK;

      // use full line to get a correct frac
      const float pfc = VLevel::CheckPObjPassPlanes(po, trace.Start, trace.End, nullptr, nullptr, nullptr);
      //GCon->Logf(NAME_Debug, "pobj #%d enter line #%d, exit line #%d, prevfrac=%g; frac=%g; pfc=%g; hit=%d", po->tag, enterline, (int)(ptrdiff_t)(line-&trace.Level->Lines[0]), prevfrac, frac, pfc, (int)(pfc > prevfrac && pfc < frac));
      //GCon->Logf(NAME_Debug, "  pobjz: %g,%g; sz=%g; ez=%g", pz0, pz1, trace.Start.z, trace.End.z);
      return !(pfc > prevfrac && pfc < frac);
    }
  }

  const TVec lend = trace.Start+trace.Delta*frac;
  if (!SightCanPassOpening(line, lend, side, trace.PlaneNoBlockFlags&SPF_FLAG_MASK)) return false; // blocked

  // if not light, we're done here (no need to check texture)
  if (!trace.lightCheck) return true; // not blocked

  // light checks
  // usually, `End` is light origin

  // just in case
  if (!line->frontsector || !line->backsector) return false; // looks like invalid line, so we'd better block

  if (!trace.wallTextureCheck) return true; // not blocked
  if (line->alpha < 1.0f || (line->flags&ML_ADDITIVE)) return true; // not blocked

  // check texture
  //const int sidenum = line->PointOnSide2(trace.Start);
  //if (sidenum == 2 || line->sidenum[sidenum] == -1) return true; // on a line, not blocked
  const int sidenum = side;
  if (line->sidenum[sidenum] == -1) return true; // on a line, not blocked

  const side_t *sidedef = &trace.Level->Sides[line->sidenum[sidenum]];
  VTexture *MTex = GTextureManager(sidedef->MidTexture);
  if (!MTex || MTex->Type == TEXTYPE_Null) return true; // not blocked

  const bool wrapped = ((line->flags&ML_WRAP_MIDTEX)|(sidedef->Flags&SDF_WRAPMIDTEX));
  if (wrapped && !MTex->isSeeThrough()) return false; // the texture is solid, so there are no holes possible

  const int thgt = MTex->GetHeight();
  if (thgt < 1) return true; // just in case, not blocked
  const int twdt = MTex->GetWidth();
  if (twdt < 1) return true; // just in case, not blocked

  //TODO: rotated textures

  // we may need it (or not, but meh)
  const float texh = MTex->GetScaledHeightF()/sidedef->Mid.ScaleY;

  // t axis
  const bool yflip = (sidedef->Mid.Flags&STP_FLIP_Y);
  const TVec taxis = TVec(0.0f, 0.0f, (yflip ? +1.0f : -1.0f))*MTex->TextureTScale()*sidedef->Mid.ScaleY;

  float zOrg; // texture bottom
  if (line->flags&ML_DONTPEGBOTTOM) {
    // bottom of texture at bottom
    zOrg = max2(line->frontsector->floor.TexZ, line->backsector->floor.TexZ);
  } else {
    // top of texture at top
    zOrg = min2(line->frontsector->ceiling.TexZ, line->backsector->ceiling.TexZ)-texh;
  }

  float toffs = 0.0f;
  float yofs = sidedef->Mid.RowOffset;
  if (yofs != 0.0f) {
    if (yflip) yofs = -yofs;
    if (wrapped) {
      // it is wrapped, so just slide it
      toffs = yofs*MTex->TextureOffsetTScale()/sidedef->Mid.ScaleY;
    } else {
      // move origin up/down, as this texture is not wrapped
      zOrg += yofs/(MTex->TextureTScale()/MTex->TextureOffsetTScale()*sidedef->Mid.ScaleY);
      // offset is done by origin, so we don't need to offset texture
      //toffs = 0.0f;
    }
  }
  if (!wrapped) {
    // check if the texture is hit
    if (lend.z < zOrg || lend.z >= zOrg+texh) return true; // not blocked (because texture wasn't hit)
    if (!MTex->isSeeThrough()) return false; // this can happen for non-wrapped midtex; blocked
  }
  if (!yflip) {
    toffs += zOrg*MTex->TextureTScale()*sidedef->Mid.ScaleY;
  } else {
    toffs += (zOrg+texh)*MTex->TextureTScale()*sidedef->Mid.ScaleY;
  }

  int texelT = (int)(DotProduct(lend, taxis)+toffs)%thgt;
  if (texelT < 0) texelT += thgt;

  // s axis
  const bool xflip = (sidedef->Mid.Flags&STP_FLIP_X);
  const bool xbroken = (sidedef->Mid.Flags&STP_BROKEN_FLIP_X);

  const TVec saxis = (sidenum == (int)!xflip ? -line->ndir : line->ndir)*MTex->TextureSScale()*sidedef->Mid.ScaleX;

  const TVec *v = (sidenum == (int)(!xflip || xbroken) ? line->v2 : line->v1);
  float soffs = -DotProduct(*v, saxis);
  float xoffs = sidedef->Mid.TextureOffset;
  if (xoffs != 0.0f) {
    if (xflip) xoffs = -xoffs;
    soffs += xoffs*MTex->TextureOffsetSScale()/sidedef->Mid.ScaleX;
  }

  int texelS = (int)(DotProduct(lend, saxis)+soffs)%twdt;
  if (texelS < 0) texelS += twdt;

  // i don't know why i shouldn't reverse Y here
  auto pix = MTex->getPixel(texelS, texelT);
  //GCon->Logf(NAME_Debug, "TEX %s: z=%g; ts=(%d,%d); a=%u", *MTex->Name, lend.z, texelS, texelT, pix.a);
  return (pix.a < 169); // not blocked if the pixel is transparent enough
}


//==========================================================================
//
//  SightPassSectorPlanes
//
//  `iidx` is index in interceptions array
//
//  returns `true` if no hit was detected
//
//==========================================================================
static bool SightPassSectorPlanes (SightTraceInfo &trace, int iidx, const line_t *line, const int side, const float prevfrac, const float frac) {
  if ((line->flags&ML_TWOSIDED) == 0 || !line->frontsector || !line->backsector) return false; // stop
  // polyobjects correctly processed in `SightPass2SLine()`
  // their sectors are not "valid" for checker, so don't check sector planes
  if (!line->pobj()) {
    sector_t *fsec = (side ? line->backsector : line->frontsector);
    if (!SightPassPlanes(trace, fsec, prevfrac, frac)) return false;
  }
  if (!SightPass2SLine(trace, iidx, line, side, frac)) return false;
  return true;
}


//==========================================================================
//
//  SightPassLine
//
//  return `true` if line is not crossed or put into intercept list
//  return `false` to stop checking due to blocking
//
//  `trace.Delta` must be correct
//
//  may set `trace.Hit1S`
//
//==========================================================================
static bool SightPassLine (SightTraceInfo &trace, line_t *ld) {
  if (ld->validcount == validcount) return true;

  ld->validcount = validcount;

  // signed distances from the line points to the trace line plane
  const float ldot1 = trace.Plane.PointDistance(*ld->v1);
  const float ldot2 = trace.Plane.PointDistance(*ld->v2);

  // do not use multiplication to check: zero speedup, lost accuracy
  //if (dot1*dot2 >= 0) return true; // line isn't crossed
  if (ldot1 < 0.0f && ldot2 < 0.0f) return true; // didn't reached back side
  // if the line is parallel to the trace plane, ignore it
  if (ldot1 >= 0.0f && ldot2 >= 0.0f) return true; // didn't reached front side

  // signed distances from the trace points to the line plane
  const float dot1 = ld->PointDistance(trace.Start);
  const float dot2 = ld->PointDistance(trace.End);

  // if starting point is on a line, ignore this line
  if (fabsf(dot1) <= 0.1f) return true;
  // light only: if ending point is on a line, ignore this line
  if (trace.lightCheck && fabsf(dot2) <= 0.1f) return true;

  // do not use multiplication to check: zero speedup, lost accuracy
  //if (dot1*dot2 >= 0) return true; // line isn't crossed
  if (dot1 < 0.0f && dot2 < 0.0f) return true; // didn't reached back side
  // if the trace is parallel to the line plane, ignore it
  if (dot1 >= 0.0f && dot2 >= 0.0f) return true; // didn't reached front side

  polyobj_t *po = ld->pobj();
  if (po && !po->Is3D()) po = nullptr;

  if (!ld->backsector || !(ld->flags&ML_TWOSIDED) ||
      (!po && (ld->flags&trace.LineBlockMask)))
  {
    // note that we hit 1s line
    trace.Hit1S = true;
    return false;
  }

  // signed distance
  const float den = DotProduct(ld->normal, trace.Delta);
  if (den == 0.0f) return true; // wtf?!
  const float num = ld->dist-DotProduct(trace.Start, ld->normal);
  const float frac = num/den;

  // just in case
  if (!isFiniteF(frac) || frac <= 0.0f || frac >= 1.0f) return true;

  // polyobject; check blocking
  if (po) {
    const float pz0 = po->pofloor.minz;
    const float pz1 = po->poceiling.minz;
    const float hpz = trace.Start.z+trace.Delta.z*frac;
    if (hpz >= pz0 && hpz <= pz1) {
      trace.wasBlocked = true;
      if (!trace.collectIntercepts) {
        trace.Hit1S = true; // this blocks "better sight" too
        return false; // blocked
      }
    }
    // check blocksight
    if (((ld->flags&trace.LineBlockMask)&ML_BLOCKSIGHT) && hpz > pz1) {
      //GCon->Logf(NAME_Debug, "LBS:000: #%d", (int)(ptrdiff_t)(ld-&trace.Level->Lines[0]));
      const side_t *fsd = &trace.Level->Sides[ld->sidenum[0]];
      if (fsd->TopTexture > 0) {
        VTexture *TTex = GTextureManager(fsd->TopTexture);
        if (TTex && TTex->Type != TEXTYPE_Null) {
          const float texh = TTex->GetScaledHeightF()/fsd->Top.ScaleY;
          //GCon->Logf(NAME_Debug, "LBS:001: #%d -- hpz=%g; pz1=%g; pz1+th=%g; inside=%d", (int)(ptrdiff_t)(ld-&trace.Level->Lines[0]), hpz, pz1, pz1+texh, (int)(hpz < pz1+texh));
          if (hpz < pz1+texh) {
            trace.wasBlocked = true;
            if (!trace.collectIntercepts) {
              trace.Hit1S = true; // this blocks "better sight" too
              return false; // blocked
            }
          }
        }
      }
    }
  }

  // store the line for later intersection testing
  intercept_t *icept = intercepts.insert(frac);
  icept->Flags = intercept_t::IF_IsALine; // just in case
  icept->line = ld;
  icept->side = (dot1 < 0.0f); // was <=

  return true; // continue scanning
}


//==========================================================================
//
//  SightPassBlockLines
//
//  `trace.Delta` must be correct
//
//  returns `true` if no hit was detected
//  may set `trace.Hit1S`
//
//==========================================================================
static bool SightPassBlockLines (SightTraceInfo &trace, int x, int y) {
  // it should never happen, but...
  //if (x < 0 || y < 0 || x >= trace.Level->BlockMapWidth || y >= trace.Level->BlockMapHeight) return false;

  const int offset = y*trace.Level->BlockMapWidth+x;

  for (polyblock_t *polyLink = trace.Level->PolyBlockMap[offset]; polyLink; polyLink = polyLink->next) {
    polyobj_t *pobj = polyLink->polyobj;
    if (pobj && pobj->validcount != validcount) {
      pobj->validcount = validcount;
      for (auto &&sit : pobj->LineFirst()) {
        line_t *ld = sit.line();
        if (ld->validcount != validcount) {
          if (!SightPassLine(trace, ld)) return false;
        }
      }
    }
  }

  for (const vint32 *list = trace.Level->BlockMapLump+(*(trace.Level->BlockMap+offset))+1; *list != -1; ++list) {
    if (!SightPassLine(trace, &trace.Level->Lines[*list])) return false;
  }

  return true; // everything was checked
}


//==========================================================================
//
//  SightPassIntercepts
//
//  `trace.Delta` must be correct
//
//  returns `true` if no hit was detected
//
//==========================================================================
static bool SightPassIntercepts (SightTraceInfo &trace) {
  float prevfrac = 0.0f;
  if (!intercepts.isEmpty()) {
    // go through in order
    const intercept_t *scan = intercepts.list;
    int iidx = 0;
    for (unsigned i = intercepts.count(); i--; ++scan, ++iidx) {
      if (!SightPassSectorPlanes(trace, iidx, scan->line, scan->side, prevfrac, scan->frac)) return false; // don't bother going further
      // ignore polyobject lines
      if (!scan->line->pobj()) prevfrac = scan->frac;
    }
  }
  return SightPassPlanes(trace, trace.EndSubSector->sector, prevfrac, 1.0f);
}


//==========================================================================
//
//  SightPassStartingPObj
//
//  check if starting point is inside any 3d pobj
//  we have to do this to check pobj planes first
//
//  returns `true` if no obstacle was hit
//  returns `false` if blocked
//
//==========================================================================
static bool SightPassStartingPObj (SightTraceInfo &trace) {
  // check if both start point and end point are in the same 3d pobj
  if (!trace.Level->Has3DPolyObjects()) return true;

  const TVec p0 = trace.Start;
  const TVec p1 = trace.End;

  for (auto &&it : trace.StartSubSector->PObjFirst()) {
    polyobj_t *po = it.pobj();
    if (!po->Is3D()) continue;

    if (!IsPointInside2DBBox(p0.x, p0.y, po->bbox2d)) continue;
    if (!IsPointInside2DBBox(p1.x, p1.y, po->bbox2d)) continue;

    // check planes first, because it is usually faster than "point in sector" check
    TVec hp;
    const float frc = VLevel::CheckPObjPassPlanes(po, p0, p1, &hp, nullptr, nullptr);
    if (frc <= 0.0f || frc >= 1.0f) continue;

    // check if starting point is inside this pobj
    if (!trace.Level->IsPointInsideSector2D(po->GetSector(), p0.x, p0.y)) continue;

    // check if hitpoint is still inside this pobj
    if (!IsPointInside2DBBox(hp.x, hp.y, po->bbox2d)) continue;
    if (!trace.Level->IsPointInsideSector2D(po->GetSector(), hp.x, hp.y)) continue;

    //GCon->Logf(NAME_Debug, "POBJ #%d HIT!", po->tag);
    // pobj flat hit, no need to trace further
    return false;
  }

  //GCon->Logf(NAME_Debug, "*** NO POBJ HIT!");
  // no starting hit
  return true;
}


//==========================================================================
//
//  SightPassPath
//
//  traces a sight ray from `trace.Start` to `trace.End`, possibly
//  collecting intercepts
//
//  `trace.StartSubSector` and `trace.EndSubSector` must be set
//
//  returns `true` if no hit was detected
//  may set `trace.Hit1S`
//
//==========================================================================
static bool SightPassPath (SightTraceInfo &trace) {
  VBlockMapWalker walker;

  intercepts.reset();

  trace.Delta = trace.End-trace.Start;
  trace.Hit1S = false;
  trace.wasBlocked = false;

  if (fabsf(trace.Delta.x) <= 1.0f && fabsf(trace.Delta.y) <= 1.0f) {
    // vertical trace; check starting sector planes and get out
    //FIXME: this is wrong for 3d pobjs!
    trace.Delta.x = trace.Delta.y = 0.0f; // to simplify further checks
    // point cannot hit anything!
    if (fabsf(trace.Delta.z) <= 1.0f) {
      trace.Hit1S = true;
      trace.Delta.z = 0.0f;
      return false;
    }
    if (!SightPassStartingPObj(trace) || !SightPassPlanes(trace, trace.StartSubSector->sector, 0.0f, 1.0f)) {
      trace.Hit1S = true; // block further height checks
      return false;
    }
    return true;
  }

  if (!SightPassStartingPObj(trace)) {
    trace.Hit1S = true; // block further height checks
    return false;
  }

  if (walker.start(trace.Level, trace.Start.x, trace.Start.y, trace.End.x, trace.End.y)) {
    trace.Plane.SetPointDirXY(trace.Start, trace.Delta);
    trace.Level->IncrementValidCount();
    int mapx, mapy;
    while (walker.next(mapx, mapy)) {
      if (!SightPassBlockLines(trace, mapx, mapy)) return false;
    }
    if (trace.wasBlocked) return false;
    // couldn't early out, so go through the sorted list
    return SightPassIntercepts(trace);
  }

  // out of map, see nothing
  return false;
}


//==========================================================================
//
//  SightPassCollectedPath
//
//  rechecks intercepts with different ending z value
//
//  returns `true` if no hit was detected
//
//==========================================================================
static bool SightPassCollectedPath (SightTraceInfo &trace) {
  // do not check for 3d pobj here (it is not fully right, but meh)
  trace.Delta = trace.End-trace.Start;
  if (fabsf(trace.Delta.x) <= 1.0f && fabsf(trace.Delta.y) <= 1.0f) {
    // vertical trace; check starting sector planes and get out
    trace.Delta.x = trace.Delta.y = 0.0f; // to simplify further checks
    // point cannot hit anything!
    if (fabsf(trace.Delta.z) <= 1.0f) {
      trace.Hit1S = true;
      trace.Delta.z = 0.0f;
      return false;
    }
    if (!SightPassStartingPObj(trace) || !SightPassPlanes(trace, trace.StartSubSector->sector, 0.0f, 1.0f)) {
      trace.Hit1S = true; // block further height checks
      return false;
    }
    return true;
  }

  if (!SightPassStartingPObj(trace)) {
    trace.Hit1S = true; // block further height checks
    return false;
  }

  return SightPassIntercepts(trace);
}


//==========================================================================
//
//  isNotInsideBM
//
//  right edge is not included
//
//==========================================================================
static VVA_CHECKRESULT inline bool isNotInsideBM (const TVec &pos, const VLevel *level) {
  // horizontal check
  const int intx = (int)pos.x;
  const int intbx0 = (int)level->BlockMapOrgX;
  if (intx < intbx0 || intx >= intbx0+level->BlockMapWidth*MAPBLOCKUNITS) return true;
  // vertical checl
  const int inty = (int)pos.y;
  const int intby0 = (int)level->BlockMapOrgY;
  return (inty < intby0 || inty >= intby0+level->BlockMapHeight*MAPBLOCKUNITS);
}



//==========================================================================
//
//  VLevel::CastCanSee
//
//  doesn't check pvs or reject
//  if better sight is allowed, `orgdirRight` and `orgdirFwd` MUST be valid!
//
//  returns `true` if no hit was detected
//
//==========================================================================
bool VLevel::CastCanSee (const subsector_t *SubSector, const TVec &org, float myheight, const TVec &orgdirFwd, const TVec &orgdirRight,
                         const TVec &dest, float radius, float height, bool skipBaseRegion, const subsector_t *DestSubSector,
                         bool allowBetterSight, bool ignoreBlockAll, bool ignoreFakeFloors)
{
  if (lengthSquared(org-dest) <= 2.0f*2.0f) return true; // arbitrary

  // if starting or ending point is out of blockmap bounds, don't bother tracing
  // we can get away with this, because nothing can see anything beyound the map extents
  if (isNotInsideBM(org, this)) return false;
  if (isNotInsideBM(dest, this)) return false;

  // it should not happen!
  if (SubSector->sector->isInnerPObj()) GCon->Logf(NAME_Error, "CastCanSee: source sector #%d is 3d pobj inner sector", (int)(ptrdiff_t)(SubSector->sector-&Sectors[0]));
  if (DestSubSector->sector->isInnerPObj()) GCon->Logf(NAME_Error, "CastCanSee: destination sector #%d is 3d pobj inner sector", (int)(ptrdiff_t)(DestSubSector->sector-&Sectors[0]));

  radius = max2(0.0f, radius);
  height = max2(0.0f, height);
  myheight = max2(0.0f, myheight);

  SightTraceInfo trace(this, org, dest, SubSector, DestSubSector);

  trace.PlaneNoBlockFlags =
    SPF_NOBLOCKSIGHT|
    (ignoreFakeFloors ? SPF_IGNORE_FAKE_FLOORS : SPF_NOTHING_ZERO)|
    (skipBaseRegion ? SPF_IGNORE_BASE_REGION : SPF_NOTHING_ZERO);

  trace.LineBlockMask =
    ML_BLOCKSIGHT|
    (ignoreBlockAll ? 0 : ML_BLOCKEVERYTHING)|
    (trace.PlaneNoBlockFlags&SPF_NOBLOCKSHOOT ? ML_BLOCKHITSCAN : ML_NOTHING_ZERO);

  //GCon->Logf(NAME_Debug, "checking sight, ssec #%d, esec #%d", (int)(ptrdiff_t)(trace.StartSubSector->sector-&trace.Level->Sectors[0]), (int)(ptrdiff_t)(trace.EndSubSector->sector-&trace.Level->Sectors[0]));

  if (trace.StartSubSector == trace.EndSubSector) {
    // same subsector, still may need to check 3d pobj/3d floors
    if (org.z-dest.z != 0.0f) {
      // vertical check
      if (!SightPassStartingPObj(trace)) return false; // oops
      if (!SightPassPlanes(trace, trace.StartSubSector->sector, 0.0f, 1.0f)) return false;
    }
    return true; // not blocked
  }

  if (!allowBetterSight || radius < 4.0f || height < 4.0f || myheight < 4.0f) {
    const TVec lookOrigin = org+TVec(0, 0, myheight*0.75f); // look from the eyes (roughly)
    trace.Start = lookOrigin;
    trace.End = dest;
    trace.End.z += height*0.85f; // roughly at the head
    const bool collectIntercepts = (height < 4.0f || trace.Delta.length2DSquared() <= 820.0f*820.0f); // arbitrary number
    trace.collectIntercepts = collectIntercepts;
    if (SightPassPath(trace)) return true;
    if (trace.Hit1S || !collectIntercepts) return false; // hit one-sided wall, or too far, no need to do other checks
    // another fast check
    trace.End = dest;
    trace.End.z += height*0.5f;
    return SightPassCollectedPath(trace);
  } else {
    const TVec lookOrigin = org+TVec(0, 0, myheight*0.86f); // look from the eyes (roughly)
    const float sidemult[3] = { 0.0f, -0.8f, 0.8f }; // side shift multiplier (by radius)
    const float ithmult = 0.92f; // destination height multiplier (0.5f is checked first)
    // check side looks
    for (unsigned myx = 0; myx < 3; ++myx) {
      // now look from eyes of t1 to some parts of t2
      trace.Start = lookOrigin+orgdirRight*(radius*sidemult[myx]);
      trace.End = dest;
      trace.collectIntercepts = true;

      //DUNNO: if our point is not in a starting sector, fix it?

      // check middle
      trace.End.z += height*0.5f;
      if (SightPassPath(trace)) return true;
      if (trace.Hit1S || intercepts.isEmpty() == 0) continue;

      // check eyes (roughly)
      trace.End = dest;
      trace.End.z += height*ithmult;
      if (SightPassCollectedPath(trace)) return true;
    }
  }

  return false;
}


//==========================================================================
//
//  VLevel::CastLightRay
//
//  doesn't check pvs or reject
//
//  usually, `dest` is light origin
//
//  returns `true` if no hit was detected
//
//==========================================================================
bool VLevel::CastLightRay (bool textureCheck, const subsector_t *startSubSector, const TVec &org, const TVec &dest, const subsector_t *endSubSector) {
  // if starting or ending point is out of blockmap bounds, don't bother tracing
  // we can get away with this, because nothing can see anything beyound the map extents
  if (isNotInsideBM(org, this)) return false;
  if (isNotInsideBM(dest, this)) return false;

  if (lengthSquared(org-dest) <= 2.0f) return true;

  SightTraceInfo trace(this, org, dest, startSubSector, endSubSector);
  trace.lightCheck = true;
  trace.flatTextureCheck = true;
  trace.wallTextureCheck = true;

  if (trace.StartSubSector == trace.EndSubSector) {
    // same subsector, still may need to check 3d pobj/3d floors
    if (org.z-dest.z != 0.0f) {
      // vertical check
      if (!SightPassStartingPObj(trace)) return false; // oops
      if (!SightPassPlanes(trace, trace.StartSubSector->sector, 0.0f, 1.0f)) return false;
    }
    return true; // not blocked
  }

  return SightPassPath(trace);
}


//==========================================================================
//
//  Script natives
//
//==========================================================================
IMPLEMENT_FUNCTION(VLevel, CastLightRay) {
  bool textureCheck;
  TVec org, dest;
  VOptParamPtr<subsector_t> startSubSector(nullptr);
  VOptParamPtr<subsector_t> endSubSector(nullptr);
  vobjGetParamSelf(textureCheck, org, dest, startSubSector, endSubSector);
  bool res = Self->CastLightRay(textureCheck, startSubSector, org, dest, endSubSector);
  RET_BOOL(res);
}
