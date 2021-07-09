//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš, dj_jl
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
#include "r_light_adv.h"

#define VV_SMAP_PAPERTHIN_FIX


VCvarB clip_shadow("clip_shadow", true, "Use clipper to drop unnecessary shadow surfaces?", CVAR_PreInit);
VCvarB clip_advlight_regions("clip_advlight_regions", false, "Clip (1D) light regions?", CVAR_PreInit);

// this is because the other side has flipped texture, so if
// the player stands behind it, the shadow is wrong
static VCvarB r_shadowmap_flip_surfaces("r_shadowmap_flip_surfaces", true, "Flip two-sided surfaces for shadowmapping?", CVAR_Archive);


// ////////////////////////////////////////////////////////////////////////// //
enum {
  FlagAsLight = 0x01u,
  FlagAsShadow = 0x02u,
  FlagAsBoth = FlagAsLight|FlagAsShadow,
};


#define IsGeoClip()  (!CurrLightNoGeoClip)


//==========================================================================
//
//  VRenderLevelShadowVolume::CollectLightShadowSurfaces
//
//==========================================================================
void VRenderLevelShadowVolume::CollectLightShadowSurfaces (bool doShadows) {
  //CurrLightNoGeoClip = false; // debug
  LightClip.ClearClipNodes(CurrLightPos, Level, CurrLightRadius);
  LightShadowClip.ClearClipNodes(CurrLightPos, Level, CurrLightRadius);
  shadowSurfacesSolid.resetNoDtor();
  shadowSurfacesMasked.resetNoDtor();
  lightSurfacesSolid.resetNoDtor();
  lightSurfacesMasked.resetNoDtor();
  collectorForShadowMaps = (r_shadowmaps.asBool() && Drawer->CanRenderShadowMaps());
  collectorShadowType = (collectorForShadowMaps && r_shadowmap_flip_surfaces.asBool() ? VViewClipper::AsShadowMap : VViewClipper::AsShadow);
  //TODO: create separate frame counter for polyobjects
  //      it is safe to reset render counters here, because they already used and not needed for anything
  Level->ResetPObjRenderCounts();
  nextRenderedLineCounter();
  if (Level->NumSubsectors < 2) {
    if (Level->NumSubsectors == 1) return CollectAdvLightSubsector(0, (doShadows ? FlagAsBoth : FlagAsLight));
  } else {
    return CollectAdvLightBSPNode(Level->NumNodes-1, nullptr, (doShadows ? FlagAsBoth : FlagAsLight));
  }
}


//==========================================================================
//
//  ClipSegToLight
//
//  this (theoretically) should clip segment to light bounds
//  tbh, i don't think that there is a real reason to do this
//
//==========================================================================
/*
static VVA_OKUNUSED inline void ClipSegToLight (TVec &v1, TVec &v2, const TVec &pos, const float radius) {
  const TVec r1 = pos-v1;
  const TVec r2 = pos-v2;
  const float d1 = DotProduct(Normalise(CrossProduct(r1, r2)), pos);
  const float d2 = DotProduct(Normalise(CrossProduct(r2, r1)), pos);
  // there might be a better method of doing this, but this one works for now...
       if (d1 > radius && d2 < -radius) v2 += (v2-v1)*d1/(d1-d2);
  else if (d2 > radius && d1 < -radius) v1 += (v1-v2)*d2/(d2-d1);
}
*/


//==========================================================================
//
//  VRenderLevelShadowVolume::AddPolyObjToLightClipper
//
//  we have to do this separately, because for now we have to add
//  invisible segs to clipper too
//  i don't yet know why
//
//==========================================================================
void VRenderLevelShadowVolume::AddPolyObjToLightClipper (VViewClipper &clip, subsector_t *sub, int asShadow) {
  if (sub && sub->HasPObjs() /*&& r_draw_pobj*/ && clip_use_1d_clipper) {
    clip.ClipLightAddPObjSegs(sub, asShadow);
  }
}


// SurfaceType
enum {
  SurfTypeOneSided, // one-sided wall
  SurfTypeTop, // two-sided wall, top
  SurfTypeBottom, // two-sided wall, bottom
  SurfTypeFlat, // flat (base floor or ceiling, including slopes)
  SurfTypeFlatEx, // flat (extra floor or ceiling, including slopes)
  // the following should be checked for flipping
  SurfTypeMiddle, // two-sided wall, middle
  SurfTypePaperFlatEx, // flat (extra floor or ceiling, including slopes), paper-thin
};


//==========================================================================
//
//  VRenderLevelShadowVolume::CollectAdvLightSurfaces
//
//==========================================================================
void VRenderLevelShadowVolume::CollectAdvLightSurfaces (const seg_t *origseg, surface_t *InSurfs, texinfo_t *texinfo,
                                                        VEntity *SkyBox, bool CheckSkyBoxAlways, int SurfaceType,
                                                        unsigned int ssflag, const bool distInFront, bool flipAllowed)
{
  if (!InSurfs) return;
  if (!(ssflag&FlagAsBoth)) return;

  if (!texinfo || !texinfo->Tex || texinfo->Tex->Type == TEXTYPE_Null) return;
  if (texinfo->Alpha < 1.0f || texinfo->Additive) return;
  // allow "semi-translucent" textures
  if (texinfo->Tex->isTranslucent() && (!r_lit_semi_translucent.asBool() || !texinfo->Tex->isSemiTranslucent())) return;

  if (SkyBox && SkyBox->IsPortalDirty()) SkyBox = nullptr;

  if (texinfo->Tex == GTextureManager.getIgnoreAnim(skyflatnum) ||
      (CheckSkyBoxAlways && (SkyBox && SkyBox->GetSkyBoxAlways())))
  {
    return;
  }

  const float surfPointDistView = InSurfs->PointDistance(Drawer->vieworg);
  const bool surfInFrontView = (surfPointDistView > 0.0f);

  const bool smaps = collectorForShadowMaps;

  const bool transTex = texinfo->Tex->/*isTransparent*/isSeeThrough();
  const bool seeTroughTex = (!smaps && /*texinfo->Tex->isSeeThrough()*/transTex);

  const seg_t *pseg = (origseg ? origseg->partner : nullptr);

  const bool canFlipSeg = flipAllowed && origseg && transTex &&
    (ssflag&FlagAsShadow) && smaps && r_shadowmap_flip_surfaces.asBool() &&
    SurfaceType == SurfTypeMiddle &&
    (pseg && pseg->drawsegs && pseg->drawsegs->mid && pseg->drawsegs->mid->surfs);

  /*const*/ bool doflip = flipAllowed &&
    (ssflag&FlagAsShadow) && smaps && r_shadowmap_flip_surfaces.asBool() &&
    SurfaceType >= SurfTypeMiddle && distInFront != surfInFrontView;

  if (ssflag&FlagAsShadow) {
    if (canFlipSeg) {
      // if we can flip the seg, assume that it is a see-through midtex
      // in this case we will render both front and back sides of the line
      // `distInFront` is for the light
      // `surfPointDistView` is for the camera origin
      // we are rendering from the light origin
      // as we can flip, render only the seg that the camera (not the light!) can see
      if (!surfInFrontView) return;
      doflip = true; // force queue (the code below will decide if it need to be flipped)
    } else {
      if (!doflip && !distInFront) return; // no flipping, and the surface is not facing the light
    }
  } else {
    if (!distInFront) return;
  }
  //if (fabsf(dist) >= CurrLightRadius) continue; // was for light
  //vassert(fabsf(dist) < CurrLightRadius);

  const bool isLightVisible = ((ssflag&FlagAsLight) && distInFront && InSurfs->IsVisibleFor(Drawer->vieworg));
  const bool paperFloor = (SurfaceType == SurfTypePaperFlatEx && InSurfs->plane.normal.z >= 0.0f);
  const bool dropPaperThinFloor = (paperFloor && /*surfPointDistView <= 0.0f &&*/ surfPointDistView < -0.1f);

  for (surface_t *surf = InSurfs; surf; surf = surf->next) {
    if (surf->count < 3) continue; // just in case
    if (!isSurfaceInSpotlight(surf)) continue;

    // check transdoor hacks
    //if (surf->drawflags&surface_t::TF_TOPHACK) continue;

    // light
    if (ssflag&FlagAsLight) {
      // `IsPlVisible()` is used here to reject some unlit surfaces
      // it is not reliable (properly set only for surfaces visible at this frame)
      // but it doesn't matter, because in the worst case we'll only get some overdraw (trivially rejected by GPU)
      if (isLightVisible && surf->IsPlVisible()) {
        // viewer is in front
        if (transTex) lightSurfacesMasked.append(surf); else lightSurfacesSolid.append(surf);
      }
    }

    // shadow
    if (ssflag&FlagAsShadow) {
      if (!smaps && (!distInFront || seeTroughTex)) continue; // this is masked texture, shadow volumes cannot process it
      if (transTex) {
        // we need to flip it if the player is behind it
        // this is not fully right, because it is better to check partner seg here, for example
        // but not for now; let map authors care about setting proper textures on 2-sided walls instead
        vassert(smaps);
        if (doflip) {
          // this is for flats: when the camera is almost on a flat, it's shadow disappears
          // this is because we cannot see neither up, nor down surface
          // in this case, leave down one
          if (paperFloor) {
            if (dropPaperThinFloor) continue;
            /*
            const float sdist = surfPointDistView;
            if (sdist <= 0.0f) {
              // paper-thin surface, ceiling: leave it if it is almost invisible
              if (sdist < -0.1f) continue;
            }
            */
          }
          // as we're rendering from the light origin, use `distInFront` to decide if we need to flip
          if (!distInFront) {
            //if (!isGood2Flip(surf, SurfaceType)) continue;
            if (SurfaceType == SurfTypeMiddle) {
              const seg_t *seg = surf->seg;
              const line_t *line = seg->linedef;
              if (!line || !(line->flags&ML_TWOSIDED)) continue;
              surf->drawflags |= surface_t::DF_SMAP_FLIP;
            } else if (SurfaceType >= SurfTypeFlatEx) {
              surf->drawflags |= surface_t::DF_SMAP_FLIP;
            } else {
              continue;
            }
          } else {
            surf->drawflags &= ~surface_t::DF_SMAP_FLIP;
          }
        } else {
          if (!distInFront) continue; // light cannot see it
          surf->drawflags &= ~surface_t::DF_SMAP_FLIP;
        }
        if (IsGeoClip()) shadowSurfacesMasked.append(surf);
      } else {
        // solid surfaces are one-sided
        if (distInFront && IsGeoClip()) shadowSurfacesSolid.append(surf);
      }
    }
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::CollectAdvLightLine
//
//  clips the given segment and adds any visible pieces to the line list
//
//==========================================================================
void VRenderLevelShadowVolume::CollectAdvLightLine (subsector_t *sub, sec_region_t *secregion, drawseg_t *dseg, unsigned int ssflag) {
  if (!dseg) return; // just in case
  const seg_t *seg = dseg->seg;
  if (!seg) return; // just in case
  if (seg->flags&SF_FULLSEG) Sys_Error("CollectAdvLightLine: fullsegs should not end up here!"); // it should not came here
  const line_t *linedef = seg->linedef;
  if (!linedef) return; // miniseg

  #if 1
  // render (queue) translucent lines by segs (for sorter)
  if (createdFullSegs && r_dbg_use_fullsegs.asBool() && /*!linedef->pobj() &&*/ (linedef->exFlags&ML_EX_NON_TRANSLUCENT)) {
    side_t *side = seg->sidedef;
    if (side->fullseg && side->fullseg->drawsegs) {
      if (side->rendercount == renderedLineCounter) return; // already rendered
      side->rendercount = renderedLineCounter;
      seg = side->fullseg;
      dseg = seg->drawsegs;
    }
  }
  #endif

  const bool goodTwoSided = (seg->backsector && (linedef->flags&ML_TWOSIDED));
  //const bool baseReg = (secregion->regflags&sec_region_t::RF_BaseRegion);

  const float dist = seg->PointDistance(CurrLightPos);
  const bool distInFront = (dist > 0.0f);
  //if (dist <= -CurrLightRadius || dist > CurrLightRadius) return; // light sphere is not touching a plane
  // we cannot flip one-sided walls
  if ((!collectorForShadowMaps || !goodTwoSided) && !distInFront) return;
  if (fabsf(dist) >= CurrLightRadius) return; // was for light

  // check this, because why not?
  // this may give nothing for shadowmaps (or even some small slowdown), but may win a little speed for shadow volumes
  if (!isCircleTouchingLine(CurrLightPos, CurrLightRadius*CurrLightRadius, *seg->v1, *seg->v2)) return;

  //k8: here we can call `ClipSegToLight()`, but i see no reasons to do so
  if (ssflag&FlagAsLight) {
    if (!distInFront || !LightClip.IsRangeVisible(*seg->v2, *seg->v1)) {
      if ((ssflag &= ~FlagAsLight) == 0) return;
    }
  }
  if (ssflag&FlagAsShadow) {
    if (collectorForShadowMaps && goodTwoSided && r_shadowmap_flip_surfaces.asBool()) {
      // alow any two-sided line for shadowmaps
    } else {
      // here `dist` should be positive, but check it anyway (for now)
      //const bool isVis = (dist > 0.0f ? LightShadowClip.IsRangeVisible(*seg->v2, *seg->v1) : LightShadowClip.IsRangeVisible(*seg->v1, *seg->v2));
      if (!distInFront || !LightShadowClip.IsRangeVisible(*seg->v2, *seg->v1)) {
        if ((ssflag &= ~FlagAsShadow) == 0) return;
      }
    }
  }

  #ifdef VV_CHECK_1S_CAST_SHADOW
  if ((ssflag&FlagAsShadow) && !seg->backsector && !CheckCan1SCastShadow(seg->linedef)) {
    //return;
    if ((ssflag &= ~FlagAsShadow) == 0) return;
  }
  #endif

  // k8: this drops some segs that may leak without proper frustum culling
  // k8: this seems to be unnecessary now
  // k8: yet leave it there in the hope that it will reduce GPU overdrawing
  // k8: no more
  /*
  if (!LightClip.CheckSegFrustum(sub, seg)) return;
  */

  VEntity *skybox = secregion->eceiling.splane->SkyBox;
  if (dseg->mid) CollectAdvLightSurfaces(seg, dseg->mid->surfs, &dseg->mid->texinfo, skybox, false, (goodTwoSided ? SurfTypeMiddle : SurfTypeOneSided), ssflag, distInFront, true);
  if (seg->backsector) {
    // two sided line
    if (dseg->top) CollectAdvLightSurfaces(seg, dseg->top->surfs, &dseg->top->texinfo, skybox, false, (goodTwoSided ? SurfTypeTop : SurfTypeOneSided), ssflag, distInFront, false);
    //k8: horizon/sky cannot block light, and cannot receive light
    //if (dseg->topsky) CollectAdvLightSurfaces(seg, dseg->topsky->surfs, &dseg->topsky->texinfo, skybox, false, SurfTypeOneSided, ssflag, distInFront, false);
    if (dseg->bot) CollectAdvLightSurfaces(seg, dseg->bot->surfs, &dseg->bot->texinfo, skybox, false, (goodTwoSided ? SurfTypeBottom : SurfTypeOneSided), ssflag, distInFront, false);
    for (segpart_t *sp = dseg->extra; sp; sp = sp->next) {
      CollectAdvLightSurfaces(seg, sp->surfs, &sp->texinfo, skybox, false, (goodTwoSided ? SurfTypeMiddle : SurfTypeOneSided), ssflag, distInFront, false); //FIXME: allow fliping
    }
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::CollectAdvLightSecSurface
//
//  this is used for floor and ceilings
//
//==========================================================================
void VRenderLevelShadowVolume::CollectAdvLightSecSurface (sec_region_t *secregion, sec_surface_t *ssurf, VEntity *SkyBox, unsigned int ssflag, const bool paperThin) {
  //const sec_plane_t &plane = *ssurf->secplane;
  if (!ssurf->esecplane.splane->pic) return;

  const float dist = ssurf->PointDistance(CurrLightPos);
  const bool distInFront = (dist > 0.0f);
  //if (dist <= -CurrLightRadius || dist > CurrLightRadius) return; // light is in back side or on plane
  if ((!collectorForShadowMaps || !paperThin) && !distInFront) return;
  if (fabsf(dist) >= CurrLightRadius) return; // was for light
  if ((ssflag&FlagAsShadow) == 0 && !distInFront) return;

  int stype;
  if (secregion->regflags&sec_region_t::RF_BaseRegion) {
    stype = SurfTypeFlat;
  } else {
    stype = (paperThin ? SurfTypePaperFlatEx : SurfTypeFlatEx);
  }

  CollectAdvLightSurfaces(nullptr, ssurf->surfs, &ssurf->texinfo, SkyBox, true, stype, ssflag, distInFront, true);
}


//==========================================================================
//
//  VRenderLevelShadowVolume::CollectAdvLightPolyObj
//
//==========================================================================
void VRenderLevelShadowVolume::CollectAdvLightPolyObj (subsector_t *sub, unsigned int ssflag) {
  if (sub && sub->HasPObjs() && r_draw_pobj) {
    const bool doUpdates = !r_disable_world_update.asBool();
    subregion_t *region = sub->regions;
    sec_region_t *secregion = region->secregion;
    for (auto &&it : sub->PObjFirst()) {
      polyobj_t *pobj = it.pobj();
      if (pobj->rendercount != BspVisFrame) {
        pobj->rendercount = BspVisFrame; // mark as rendered
        TSecPlaneRef po_floor, po_ceiling;
        po_floor.set(&pobj->pofloor, false);
        po_ceiling.set(&pobj->poceiling, false);
        if (doUpdates && pobj->updateWorldFrame != updateWorldFrame) {
          pobj->updateWorldFrame = updateWorldFrame;
          UpdatePObj(pobj);
        }
        for (auto &&sit : pobj->SegFirst()) {
          const seg_t *seg = sit.seg();
          if (seg->drawsegs && !(seg->flags&SF_FULLSEG)) CollectAdvLightLine(sub, secregion, seg->drawsegs, ssflag);
        }
        if (pobj->Is3D()) {
          for (subsector_t *posub = pobj->GetSector()->subsectors; posub; posub = posub->seclink) {
            CollectAdvLightSubRegion(posub, ssflag);
          }
        }
      }
    }
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::CollectAdvLightSubRegion
//
//  Determine floor/ceiling planes.
//  Draw one or more line segments.
//
//==========================================================================
void VRenderLevelShadowVolume::CollectAdvLightSubRegion (subsector_t *sub, unsigned int ssflag) {
  for (subregion_t *region = sub->regions; region; region = region->next) {
    //TSecPlaneRef r_floor = region->floorplane;
    //TSecPlaneRef r_ceiling = region->ceilplane;

    sec_region_t *secregion = region->secregion;

    if ((secregion->regflags&sec_region_t::RF_BaseRegion) && sub->numlines > 0 && !sub->isInnerPObj()) {
      const seg_t *seg = &Level->Segs[sub->firstline];
      for (int j = sub->numlines; j--; ++seg) {
        if (!seg->linedef || !seg->drawsegs) continue; // miniseg has no drawsegs/segparts
        CollectAdvLightLine(sub, secregion, seg->drawsegs, ssflag);
      }
    }

    sec_surface_t *fsurf[4];
    GetFlatSetToRender(sub, region, fsurf);
    //bool skipFloor = false;
    //bool skipCeiling = false;
    unsigned int floorFlag = FlagAsBoth;
    unsigned int ceilingFlag = FlagAsBoth;

    // skip sectors with height transfer for now
    if ((ssflag&FlagAsShadow) && (region->secregion->regflags&sec_region_t::RF_BaseRegion) && !sub->sector->heightsec) {
      unsigned disableflag = CheckShadowingFlats(sub);
      if (disableflag&FlatSectorShadowInfo::NoFloor) {
        //GCon->Logf(NAME_Debug, "dropping floor for sector #%d", (int)(ptrdiff_t)(sub->sector-&Level->Sectors[0]));
        //fsurf[0] = fsurf[1] = nullptr;
        //skipFloor = true;
        floorFlag = FlagAsLight;
      }
      if (disableflag&FlatSectorShadowInfo::NoCeiling) {
        //GCon->Logf(NAME_Debug, "dropping ceiling for sector #%d", (int)(ptrdiff_t)(sub->sector-&Level->Sectors[0]));
        //fsurf[2] = fsurf[3] = nullptr;
        //skipCeiling = true;
        ceilingFlag = FlagAsLight;
      }
    }

    // paper-thin surface shadow may disappear; workaround it
    #ifdef VV_SMAP_PAPERTHIN_FIX
    bool paperThin = false;
    if (((ssflag&floorFlag)&FlagAsShadow) && fsurf[0] && fsurf[2]) {
      if (secregion->efloor.GetRealDist() == secregion->eceiling.GetRealDist()) {
        paperThin = true;
      }
    }
    #else
    # define paperThin false
    #endif

    if (fsurf[0]) CollectAdvLightSecSurface(secregion, fsurf[0], secregion->efloor.splane->SkyBox, ssflag&floorFlag, paperThin);
    if (fsurf[1]) CollectAdvLightSecSurface(secregion, fsurf[1], secregion->efloor.splane->SkyBox, ssflag&floorFlag, paperThin);

    if (fsurf[2]) CollectAdvLightSecSurface(secregion, fsurf[2], secregion->eceiling.splane->SkyBox, ssflag&ceilingFlag, paperThin);
    if (fsurf[3]) CollectAdvLightSecSurface(secregion, fsurf[3], secregion->eceiling.splane->SkyBox, ssflag&ceilingFlag, paperThin);
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::CollectAdvLightSubsector
//
//==========================================================================
void VRenderLevelShadowVolume::CollectAdvLightSubsector (int num, unsigned int ssflag) {
  vassert(num >= 0 && num < Level->NumSubsectors);
  subsector_t *sub = &Level->Subsectors[num];
  if (sub->isAnyPObj()) return;

  // `LightBspVis` is already an intersection, no need to check `BspVisData` here
  //if (!IsSubsectorLitBspVis(num) || !IsBspVis(num)) return;

  if ((ssflag&FlagAsLight) && !LightClip.ClipLightCheckSubsector(sub, VViewClipper::AsLight)) {
    if ((ssflag &= ~FlagAsLight) == 0) return;
  }
  if ((ssflag&FlagAsShadow) && !LightShadowClip.ClipLightCheckSubsector(sub, collectorShadowType)) {
    if ((ssflag &= ~FlagAsShadow) == 0) return;
  }

  if (ssflag) {
    if ((ssflag&FlagAsLight) && !IsSubsectorLitBspVis(num)) {
      if ((ssflag &= ~FlagAsLight) == 0) return;
    }

    // update world
    if (sub->updateWorldFrame != updateWorldFrame) {
      sub->updateWorldFrame = updateWorldFrame;
      if (!r_disable_world_update) UpdateSubRegions(sub);
    }

    // if our light is in frustum, out-of-frustum subsectors are not interesting
    //FIXME: pass "need frustum check" flag to other functions
    if ((ssflag&FlagAsShadow) && CurrLightInFrustum && !IsBspVis(num)) {
      // this subsector is invisible, check if it is in frustum (this was originally done for shadow)
      float bbox[6];
      // min
      bbox[0] = sub->bbox2d[BOX2D_LEFT];
      bbox[1] = sub->bbox2d[BOX2D_BOTTOM];
      bbox[2] = sub->sector->floor.minz;
      // max
      bbox[3] = sub->bbox2d[BOX2D_RIGHT];
      bbox[4] = sub->bbox2d[BOX2D_TOP];
      bbox[5] = sub->sector->ceiling.maxz;
      FixBBoxZ(bbox);
      if (!Drawer->viewfrustum.checkBox(bbox)) {
        if (clip_shadow && IsGeoClip()) LightShadowClip.ClipLightAddSubsectorSegs(sub, collectorShadowType);
        if ((ssflag &= ~FlagAsShadow) == 0) return;
      }
    }

    // render the polyobj in the subsector first, and add it to clipper
    // this blocks view with polydoors
    if (ssflag) {
      CollectAdvLightPolyObj(sub, ssflag);
      if (IsGeoClip()) AddPolyObjToLightClipper(LightClip, sub, VViewClipper::AsLight);
      if (clip_shadow && IsGeoClip()) AddPolyObjToLightClipper(LightShadowClip, sub, collectorShadowType);
      CollectAdvLightSubRegion(sub, ssflag);
      // add subsector's segs to the clipper
      // clipping against mirror is done only for vertical mirror planes
      if ((ssflag&FlagAsLight) && IsGeoClip()) LightClip.ClipLightAddSubsectorSegs(sub, VViewClipper::AsLight);
      if ((ssflag&FlagAsShadow) && clip_shadow && IsGeoClip()) LightShadowClip.ClipLightAddSubsectorSegs(sub, collectorShadowType);
    }
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::CollectAdvLightBSPNode
//
//  Renders all subsectors below a given node, traversing subtree
//  recursively. Just call with BSP root.
//
//==========================================================================
void VRenderLevelShadowVolume::CollectAdvLightBSPNode (int bspnum, const float *bbox, unsigned int ssflag) {
 tailcall:
#ifdef VV_CLIPPER_FULL_CHECK
  if ((ssflag&FlagAsLight) && LightClip.ClipIsFull()) {
    if ((ssflag &= ~FlagAsLight) == 0) return;
  }
  if ((ssflag&FlagAsShadow) && LightShadowClip.ClipIsFull()) {
    if ((ssflag &= ~FlagAsShadow) == 0) return;
  }
#endif

  if (bbox) {
    // mirror clip
    if (Drawer->MirrorClip && !Drawer->MirrorPlane.checkBox3D(bbox)) return;
    // clipper clip
    if ((ssflag&FlagAsLight) && !LightClip.ClipLightIsBBoxVisible(bbox)) {
      if ((ssflag &= ~FlagAsLight) == 0) return;
    }
    if ((ssflag&FlagAsShadow) && !LightShadowClip.ClipLightIsBBoxVisible(bbox)) {
      if ((ssflag &= ~FlagAsShadow) == 0) return;
    }
  }
  //if (bbox && !CheckSphereVsAABBIgnoreZ(bbox, CurrLightPos, CurrLightRadius)) return;

  // no need to
  //if (bspnum == -1) return CollectAdvLightSubsector(0, ssflag);

  // found a subsector?
  if (BSPIDX_IS_NON_LEAF(bspnum)) {
    node_t *bsp = &Level->Nodes[bspnum];
    // decide which side the light is on
    const float dist = bsp->PointDistance(CurrLightPos);
    if (dist >= CurrLightRadius) {
      // light is completely on front side
      //return CollectAdvLightBSPNode(bsp->children[0], bsp->bbox[0], ssflag);
      bspnum = bsp->children[0];
      bbox = bsp->bbox[0];
      goto tailcall;
    } else if (dist <= -CurrLightRadius) {
      // light is completely on back side
      //return CollectAdvLightBSPNode(bsp->children[1], bsp->bbox[1], ssflag);
      bspnum = bsp->children[1];
      bbox = bsp->bbox[1];
      goto tailcall;
    } else {
      //int side = bsp->PointOnSide(CurrLightPos);
      unsigned side = (unsigned)(dist <= 0.0f);
      // recursively divide front space
      CollectAdvLightBSPNode(bsp->children[side], bsp->bbox[side], ssflag);
      // possibly divide back space
      side ^= 1u;
      //return CollectAdvLightBSPNode(bsp->children[side], bsp->bbox[side], ssflag);
      bspnum = bsp->children[side];
      bbox = bsp->bbox[side];
      goto tailcall;
    }
  } else {
    return CollectAdvLightSubsector(BSPIDX_LEAF_SUBSECTOR(bspnum), ssflag);
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderShadowSurfaceList
//
//  this is used only for shadow volumes
//
//==========================================================================
void VRenderLevelShadowVolume::RenderShadowSurfaceList () {
  // non-solid surfaces cannot cast shadows with shadow volumes
  for (auto &&surf : shadowSurfacesSolid) {
    Drawer->RenderSurfaceShadowVolume(Level, surf, CurrLightPos, CurrLightRadius);
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderLightSurfaceList
//
//==========================================================================
void VRenderLevelShadowVolume::RenderLightSurfaceList () {
  Drawer->RenderSolidLightSurfaces(lightSurfacesSolid);
  Drawer->RenderMaskedLightSurfaces(lightSurfacesMasked);
}
