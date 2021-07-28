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
//**  variouse light visibility methods
//**
//**************************************************************************
#include "../gamedefs.h"
#include "r_local.h"


extern VCvarB r_draw_pobj;

#define IsGeoClip()  (!CurrLightNoGeoClip)


//==========================================================================
//
//  VRenderLevelShared::CalcLightVisUnstuckLightSubsector
//
//  check if the light is too close to a wall/floor,
//  and calculate "move out" vector
//
//==========================================================================
void VRenderLevelShared::CalcLightVisUnstuckLightSubsector (const subsector_t *sub) {
  if (!sub) return;

  const float mindist = 2.5f;
  //const sector_t *sec = sub->sector;

  /*
  const float fz = sec->floor.GetPointZClamped(CurrLightUnstuckPos);
  const float cz = sec->ceiling.GetPointZClamped(CurrLightUnstuckPos);
  */

  // check walls
  const seg_t *seg = &Level->Segs[sub->firstline];
  for (int count = sub->numlines; count--; ++seg) {
    const line_t *linedef = seg->linedef;
    if (!linedef) continue; // miniseg
    if (linedef->flags&ML_TWOSIDED) continue; // don't bother with two-sided lines for now
    //const float dist = sef->PointDistance(CurrLightUnstuckPos);
    /*
    if (seg->partner) {
      if (!seg->partner->frontsub) continue;
      const sector_t *osec = seg->partner->frontsub->sector;
      if (!osec) continue;
      // if this sector height is lower than the other sector height, and the light is above other height, bottex skip
      if ((sec->floor.maxz >= osec->floor.maxz || CurrLightUnstuckPos.z >= osec->floor.maxz) && // skip bottom?
          (sec->ceiling.minz <= osec->ceiling.minz || CurrLightUnstuckPos.z <= osec->ceiling.minz)) // skip top?
      {
        // check midtex presence
        const side_t *sidedef = seg->sidedef;
        if (!sidedef) continue; // just in case
        if (sidedef->MidTexture <= 0) continue;
      }
    }
    */
    const float dist = seg->PointDistance(CurrLightUnstuckPos);
    if (dist > 0.0f && dist < mindist) CurrLightUnstuckPos += seg->normal*(mindist-dist); // move away
  }
  // now check floors
  for (const subregion_t *region = sub->regions; region; region = region->next) {
    if (region->floorplane.splane) {
      const float dist = region->floorplane.PointDistance(CurrLightUnstuckPos);
      if (dist > 0.0f && dist < mindist) CurrLightUnstuckPos += region->floorplane.GetNormal()*(mindist-dist); // move away
    }
    if (region->ceilplane.splane) {
      const float dist = region->ceilplane.PointDistance(CurrLightUnstuckPos);
      if (dist > 0.0f && dist < mindist) CurrLightUnstuckPos += region->floorplane.GetNormal()*(mindist-dist); // move away
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::CalcLightVisCheckSubsector
//
//==========================================================================
void VRenderLevelShared::CalcLightVisCheckSubsector (const unsigned subidx) {
  subsector_t *sub = &Level->Subsectors[subidx];
  if (sub->isAnyPObj()) return;

  if (!LightClip.ClipLightCheckSubsector(sub, false)) {
    if (!IsGeoClip()) return;
    return LightClip.ClipLightAddSubsectorSegs(sub, false);
  }

  // surface hits are checked in bbox updater
  //if (LitCalcBBox) LightSubs.append((int)subidx);
  //const vuint8 bvbit = (vuint8)(1u<<((unsigned)(subidx)&7));
  //const unsigned sid8 = (unsigned)(subidx)>>3;
  //LightVis[sid8] |= bvbit;
  LightVis[subidx] = LightFrameNum;
  if (IsBspVis((int)subidx)) {
    //LightBspVis[sid8] |= bvbit;
    LightBspVis[subidx] = LightFrameNum;
    HasLightIntersection = true;
    //if (LitCalcBBox) LightVisSubs.append((int)subidx);
    if (LitCalcBBox) {
      for (const subregion_t *region = sub->regions; region; region = region->next) {
        sec_region_t *curreg = region->secregion;
        if (curreg->regflags&sec_region_t::RF_BaseRegion) {
          if (sub->HasPObjs() && r_draw_pobj.asBool()) {
            //TODO: this is excessive, check each pobj only once!
            for (auto &&it : sub->PObjFirst()) {
              polyobj_t *pobj = it.pobj();
              for (auto &&sit : pobj->SegFirst()) {
                const seg_t *pseg = sit.seg();
                if (pseg->linedef && pseg->drawsegs) UpdateBBoxWithLine(LitBBox, curreg->eceiling.splane->SkyBox, pseg->drawsegs);
              }
            }
          }
          if (sub->numlines > 0) {
            const seg_t *seg = &Level->Segs[sub->firstline];
            for (int j = sub->numlines; j--; ++seg) {
              if (!seg->linedef || !seg->drawsegs) continue; /* miniseg has no drawsegs/segparts */
              UpdateBBoxWithLine(LitBBox, curreg->eceiling.splane->SkyBox, seg->drawsegs);
            }
          }
        }
        if (region->fakefloor) UpdateBBoxWithSurface(LitBBox, region->fakefloor->surfs, &region->fakefloor->texinfo, curreg->efloor.splane->SkyBox, true);
        if (region->realfloor) UpdateBBoxWithSurface(LitBBox, region->realfloor->surfs, &region->realfloor->texinfo, curreg->efloor.splane->SkyBox, true);
        if (region->fakeceil) UpdateBBoxWithSurface(LitBBox, region->fakeceil->surfs, &region->fakeceil->texinfo, curreg->eceiling.splane->SkyBox, true);
        if (region->realceil) UpdateBBoxWithSurface(LitBBox, region->realceil->surfs, &region->realceil->texinfo, curreg->eceiling.splane->SkyBox, true);
      }
    }
  }

  if (CurrLightBit) {
    if (sub->dlightframe != currDLightFrame) {
      sub->dlightbits = CurrLightBit;
      sub->dlightframe = currDLightFrame;
    } else {
      sub->dlightbits |= CurrLightBit;
    }
  }

  if (IsGeoClip()) LightClip.ClipLightAddSubsectorSegs(sub, false);

  if (CurrLightCalcUnstuck) return CalcLightVisUnstuckLightSubsector(sub);
}


//==========================================================================
//
//  VRenderLevelShared::CalcLightVisCheckNode
//
//==========================================================================
void VRenderLevelShared::CalcLightVisCheckNode (int bspnum, const float *bbox, const float *lightbbox) {
 tailcall:
  if (!Are3DBBoxesOverlapIn2D(bbox, lightbbox)) return;

 tailcall_no_overlap:
#ifdef VV_CLIPPER_FULL_CHECK
  if (LightClip.ClipIsFull()) return;
#endif

  if (!LightClip.ClipLightIsBBoxVisible(bbox)) return;

  // mirror clip
  if (Drawer->MirrorClip && !Drawer->MirrorPlane.checkBox3D(bbox)) return;

  // found a subsector?
  if (BSPIDX_IS_NON_LEAF(bspnum)) {
    const node_t *bsp = &Level->Nodes[bspnum];
    // decide which side the view point is on
    const float dist = bsp->PointDistance(CurrLightPos);
    if (dist >= CurrLightRadius) {
      // light is completely on the front side
      //return CalcLightVisCheckNode(bsp->children[0], bsp->bbox[0], lightbbox);
      bspnum = bsp->children[0];
      bbox = bsp->bbox[0];
      goto tailcall;
    } else if (dist <= -CurrLightRadius) {
      // light is completely on the back side
      //return CalcLightVisCheckNode(bsp->children[1], bsp->bbox[1], lightbbox);
      bspnum = bsp->children[1];
      bbox = bsp->bbox[1];
      goto tailcall;
    } else {
      // as we are doing geometry culling, we should use the correct order here
      const unsigned side = (unsigned)(dist <= 0.0f);
      // check front side
      if (Are3DBBoxesOverlapIn2D(bsp->bbox[side], lightbbox)) {
        // is back side touched too?
        if (Are3DBBoxesOverlapIn2D(bsp->bbox[1u^side], lightbbox)) {
          // check both, front first
          CalcLightVisCheckNode(bsp->children[side], bsp->bbox[side], lightbbox);
          bspnum = bsp->children[1u^side];
          bbox = bsp->bbox[1u^side];
          goto tailcall_no_overlap;
        } else {
          // only front
          bspnum = bsp->children[side];
          bbox = bsp->bbox[side];
          goto tailcall_no_overlap;
        }
      } else {
        bspnum = bsp->children[1u^side];
        bbox = bsp->bbox[1u^side];
        goto tailcall;
      }
    }
  } else {
    const unsigned subidx = (unsigned)(BSPIDX_LEAF_SUBSECTOR(bspnum));
    return CalcLightVisCheckSubsector(subidx);
  }
}


//==========================================================================
//
//  VRenderLevelShared::CheckValidLightPosRough
//
//==========================================================================
bool VRenderLevelShared::CheckValidLightPosRough (TVec &lorg, const sector_t *sec) {
  if (!sec) return true;
  if (sec->floor.normal.z == 1.0f && sec->ceiling.normal.z == -1.0f) {
    // normal sector
    if (sec->floor.minz >= sec->ceiling.maxz) return false; // oops, it is closed
    const float lz = lorg.z;
    float lzdiff = lz-sec->floor.minz;
    if (lzdiff < 0) return false; // stuck
    if (lzdiff == 0) lorg.z += 2; // why not?
    lzdiff = lz-sec->ceiling.minz;
    if (lzdiff > 0) return false; // stuck
    if (lzdiff == 0) lorg.z -= 2; // why not?
  } else {
    // sloped sector
    const float lz = lorg.z;
    const float lfz = sec->floor.GetPointZClamped(lorg);
    const float lcz = sec->ceiling.GetPointZClamped(lorg);
    if (lfz >= lcz) return false; // closed
    float lzdiff = lz-lfz;
    if (lzdiff < 0) return false; // stuck
    if (lzdiff == 0) lorg.z += 2; // why not?
    lzdiff = lz-lcz;
    if (lzdiff > 0) return false; // stuck
    if (lzdiff == 0) lorg.z -= 2; // why not?
  }
  return true;
}


//==========================================================================
//
//  VRenderLevelShared::CalcLightVis
//
//  sets `CurrLightPos` and `CurrLightRadius`, and other lvis fields
//  returns `false` if the light is invisible
//
//  TODO: clip invisible geometry for spotlights
//
//==========================================================================
bool VRenderLevelShared::CalcLightVis (const TVec &org, const float radius, const TVec &aconeDir, const float aconeAngle, int dlnum) {
  if (CurrLightCalcUnstuck) CurrLightUnstuckPos = org;

  //if (dlnum >= 0) dlinfo[dlnum].touchedSubs.reset();
  if (radius < 2.0f) return false;

  //bool skipShadowCheck = !r_light_opt_shadow;

  doShadows = (radius >= 8.0f && IsGeoClip());

  setupCurrentLight(org, radius, aconeDir, aconeAngle);
  //CurrLightPos = org;
  //CurrLightRadius = radius;
  CurrLightBit = (dlnum >= 0 ? 1u<<dlnum : 0u);

  /*LightSubs.reset();*/ // all affected subsectors
  /*LightVisSubs.reset();*/ // visible affected subsectors
  LitSurfaceHit = false;
  //HasBackLit = false;

  LitBBox[0] = TVec(+FLT_MAX, +FLT_MAX, +FLT_MAX);
  LitBBox[1] = TVec(-FLT_MAX, -FLT_MAX, -FLT_MAX);

  const float dummybbox[6] = { -99999.0f, -99999.0f, -99999.0f, 99999.0f, 99999.0f, 99999.0f };

  // create light bounding box
  const float lightbbox[6] = {
    org.x-radius,
    org.y-radius,
    -99999.0f, // doesn't matter
    org.x+radius,
    org.y+radius,
    +99999.0f, // doesn't matter
  };

  // build vis data for light
  IncLightFrameNum();
  LightClip.ClearClipNodes(CurrLightPos, Level, CurrLightRadius);
  HasLightIntersection = false;
  if (Level->NumSubsectors < 2) {
    if (Level->NumSubsectors == 1) CalcLightVisCheckSubsector(0);
  } else {
    CalcLightVisCheckNode(Level->NumNodes-1, dummybbox, lightbbox);
  }
  return HasLightIntersection;
}


//==========================================================================
//
//  VRenderLevelShared::RadiusCastRay
//
//  usually, `dest` is light origin
//
//==========================================================================
bool VRenderLevelShared::RadiusCastRay (bool textureCheck, const subsector_t *subsector, const TVec &org, const subsector_t *destsubsector, const TVec &dest, float /*radius*/) {
#if 0
  // BSP tracing
  float dsq = length2DSquared(org-dest);
  if (dsq <= 1) return true;
  linetrace_t Trace;
  bool canHit = !!Level->TraceLine(Trace, org, dest, SPF_NOBLOCKSIGHT);
  if (canHit) return true;
  if (!advanced || radius < 12) return false;
  // check some more rays
  if (r_lights_cast_many_rays) {
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        if ((dy|dx) == 0) continue;
        TVec np = org;
        np.x += radius*(0.73f*dx);
        np.y += radius*(0.73f*dy);
        canHit = !!Level->TraceLine(Trace, np, dest, SPF_NOBLOCKSIGHT);
        if (canHit) return true;
      }
    }
  } else {
    // check only "head" and "feet"
    TVec np = org;
    np.y += radius*0.73f;
    if (Level->TraceLine(Trace, np, dest, SPF_NOBLOCKSIGHT)) return true;
    np = org;
    np.y -= radius*0.73f;
    if (Level->TraceLine(Trace, np, dest, SPF_NOBLOCKSIGHT)) return true;
  }
  return false;
#else
  // blockmap tracing
  return Level->CastLightRay(textureCheck, subsector, org, dest, destsubsector);
#endif
}
