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

extern VCvarB clip_shadow;
extern VCvarB clip_advlight_regions;


//==========================================================================
//
//  VRenderLevelLightmap::CollectRegLightSurfaces
//
//==========================================================================
void VRenderLevelLightmap::CollectRegLightSurfaces (TMapNC<surface_t *, bool> &litSurfaces) {
  LightClip.ClearClipNodes(CurrLightPos, Level, CurrLightRadius);
  //TODO: create separate frame counter for polyobjects
  //      it is safe to reset render counters here, because they already used and not needed for anything
  Level->ResetPObjRenderCounts();
  nextRenderedLineCounter();
  if (Level->NumSubsectors < 2) {
    if (Level->NumSubsectors == 1) return CheckRegLightSubsector(0, litSurfaces);
  } else {
    return CheckRegLightBSPNode(Level->NumNodes-1, nullptr, litSurfaces);
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::CheckRegLightSurfaces
//
//==========================================================================
void VRenderLevelLightmap::CheckRegLightSurfaces (const seg_t * /*origseg*/, surface_t *InSurfs, texinfo_t *texinfo,
                                                  VEntity *SkyBox, bool CheckSkyBoxAlways, TMapNC<surface_t *, bool> &litSurfaces)
{
  if (!InSurfs) return;

  if (!texinfo || !texinfo->Tex || texinfo->Tex->Type == TEXTYPE_Null) return;
  if (texinfo->Alpha < 1.0f || texinfo->Additive) return;
  // allow "semi-translucent" textures
  //if (texinfo->Tex->isTranslucent() && (!r_lit_semi_translucent.asBool() || !texinfo->Tex->isSemiTranslucent())) return;
  if (texinfo->Tex->isTranslucent()) return;

  if (SkyBox && SkyBox->IsPortalDirty()) SkyBox = nullptr;

  if (texinfo->Tex == GTextureManager.getIgnoreAnim(skyflatnum) ||
      (CheckSkyBoxAlways && (SkyBox && SkyBox->GetSkyBoxAlways())))
  {
    return;
  }

  for (surface_t *surf = InSurfs; surf; surf = surf->next) {
    if (surf->count < 3) continue;
    litSurfaces.put(surf, true);
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::CheckRegLightLine
//
//  clips the given segment and adds any visible pieces to the line list
//
//==========================================================================
void VRenderLevelLightmap::CheckRegLightLine (subsector_t * /*sub*/, sec_region_t *secregion, drawseg_t *dseg, TMapNC<surface_t *, bool> &litSurfaces) {
  if (!dseg) return; // just in case
  const seg_t *seg = dseg->seg;
  if (!seg) return; // just in case
  if (seg->flags&SF_FULLSEG) Sys_Error("CheckRegLightLine: fullsegs should not end up here!"); // it should not came here
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

  const float dist = seg->PointDistance(CurrLightPos);
  if (dist < 0.0f || dist >= CurrLightRadius) return;

  // check this, because why not?
  // this may give nothing for shadowmaps (or even some small slowdown), but may win a little speed for shadow volumes
  if (!isCircleTouchingLine(CurrLightPos, CurrLightRadius*CurrLightRadius, *seg->v1, *seg->v2)) return;

  if (!LightClip.IsRangeVisible(*seg->v2, *seg->v1)) return;

  VEntity *skybox = secregion->eceiling.splane->SkyBox;
  if (dseg->mid) CheckRegLightSurfaces(seg, dseg->mid->surfs, &dseg->mid->texinfo, skybox, false, litSurfaces);

  if (seg->backsector) {
    // two sided line
    if (dseg->top) CheckRegLightSurfaces(seg, dseg->top->surfs, &dseg->top->texinfo, skybox, false, litSurfaces);
    //k8: horizon/sky cannot block light, and cannot receive light
    //if (dseg->topsky) CheckRegLightSurfaces(seg, dseg->topsky->surfs, &dseg->topsky->texinfo, skybox, false, sfcheck);
    if (dseg->bot) CheckRegLightSurfaces(seg, dseg->bot->surfs, &dseg->bot->texinfo, skybox, false, litSurfaces);
    for (segpart_t *sp = dseg->extra; sp; sp = sp->next) {
      CheckRegLightSurfaces(seg, sp->surfs, &sp->texinfo, skybox, false, litSurfaces);
    }
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::CheckRegLightSecSurface
//
//  this is used for floor and ceilings
//
//==========================================================================
void VRenderLevelLightmap::CheckRegLightSecSurface (sec_region_t * /*secregion*/, sec_surface_t *ssurf, VEntity *SkyBox, TMapNC<surface_t *, bool> &litSurfaces) {
  //const sec_plane_t &plane = *ssurf->secplane;
  if (!ssurf->esecplane.splane->pic) return;

  const float dist = ssurf->PointDistance(CurrLightPos);
  if (dist < 0.0f || dist >= CurrLightRadius) return;

  return CheckRegLightSurfaces(nullptr, ssurf->surfs, &ssurf->texinfo, SkyBox, true, litSurfaces);
}


//==========================================================================
//
//  VRenderLevelLightmap::CheckRegLightSubRegion
//
//  Determine floor/ceiling planes.
//  Draw one or more line segments.
//
//==========================================================================
void VRenderLevelLightmap::CheckRegLightSubRegion (subsector_t *sub, TMapNC<surface_t *, bool> &litSurfaces) {
  for (subregion_t *region = sub->regions; region; region = region->next) {
    //TSecPlaneRef r_floor = region->floorplane;
    //TSecPlaneRef r_ceiling = region->ceilplane;

    sec_region_t *secregion = region->secregion;

    if ((secregion->regflags&sec_region_t::RF_BaseRegion) && sub->numlines > 0 && !sub->isInnerPObj()) {
      const seg_t *seg = &Level->Segs[sub->firstline];
      for (int j = sub->numlines; j--; ++seg) {
        if (!seg->linedef || !seg->drawsegs) continue; // miniseg has no drawsegs/segparts
        CheckRegLightLine(sub, secregion, seg->drawsegs, litSurfaces);
      }
    }

    sec_surface_t *fsurf[4];
    GetFlatSetToRender(sub, region, fsurf);

    if (fsurf[0]) CheckRegLightSecSurface(secregion, fsurf[0], secregion->efloor.splane->SkyBox, litSurfaces);
    if (fsurf[1]) CheckRegLightSecSurface(secregion, fsurf[1], secregion->efloor.splane->SkyBox, litSurfaces);

    if (fsurf[2]) CheckRegLightSecSurface(secregion, fsurf[2], secregion->eceiling.splane->SkyBox, litSurfaces);
    if (fsurf[3]) CheckRegLightSecSurface(secregion, fsurf[3], secregion->eceiling.splane->SkyBox, litSurfaces);
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::CheckRegLightSubsector
//
//==========================================================================
void VRenderLevelLightmap::CheckRegLightSubsector (int num, TMapNC<surface_t *, bool> &litSurfaces) {
  vassert(num >= 0 && num < Level->NumSubsectors);
  subsector_t *sub = &Level->Subsectors[num];
  if (sub->isAnyPObj()) return;

  // `LightBspVis` is already an intersection, no need to check `BspVisData` here
  //if (!IsSubsectorLitBspVis(num) || !IsBspVis(num)) return;

  // update world
  if (sub->updateWorldFrame != updateWorldFrame) {
    sub->updateWorldFrame = updateWorldFrame;
    if (!r_disable_world_update) UpdateSubRegions(sub);
  }

  CheckRegLightSubRegion(sub, litSurfaces);
  // add subsector's segs to the clipper
  // clipping against mirror is done only for vertical mirror planes
  LightClip.ClipLightAddSubsectorSegs(sub, VViewClipper::AsLight);
}


//==========================================================================
//
//  VRenderLevelLightmap::CheckRegLightBSPNode
//
//  Renders all subsectors below a given node, traversing subtree
//  recursively. Just call with BSP root.
//
//==========================================================================
void VRenderLevelLightmap::CheckRegLightBSPNode (int bspnum, const float *bbox, TMapNC<surface_t *, bool> &litSurfaces) {
 tailcall:
#ifdef VV_CLIPPER_FULL_CHECK
  if (LightClip.ClipIsFull()) return;
#endif

  if (bbox) {
    // mirror clip
    if (Drawer->MirrorClip && !Drawer->MirrorPlane.checkBox3D(bbox)) return;
    // clipper clip
    if (!LightClip.ClipLightIsBBoxVisible(bbox)) return;
  }
  //if (bbox && !CheckSphereVsAABBIgnoreZ(bbox, CurrLightPos, CurrLightRadius)) return;

  // no need to
  //if (bspnum == -1) return CheckRegLightSubsector(0, ssflag);

  // found a subsector?
  if (BSPIDX_IS_NON_LEAF(bspnum)) {
    node_t *bsp = &Level->Nodes[bspnum];
    // decide which side the light is on
    const float dist = bsp->PointDistance(CurrLightPos);
    if (dist >= CurrLightRadius) {
      // light is completely on front side
      //return CheckRegLightBSPNode(bsp->children[0], bsp->bbox[0], ssflag);
      bspnum = bsp->children[0];
      bbox = bsp->bbox[0];
      goto tailcall;
    } else if (dist <= -CurrLightRadius) {
      // light is completely on back side
      //return CheckRegLightBSPNode(bsp->children[1], bsp->bbox[1], ssflag);
      bspnum = bsp->children[1];
      bbox = bsp->bbox[1];
      goto tailcall;
    } else {
      //int side = bsp->PointOnSide(CurrLightPos);
      unsigned side = (unsigned)(dist <= 0.0f);
      // recursively divide front space
      CheckRegLightBSPNode(bsp->children[side], bsp->bbox[side], litSurfaces);
      // possibly divide back space
      side ^= 1u;
      //return CheckRegLightBSPNode(bsp->children[side], bsp->bbox[side], sfcheck);
      bspnum = bsp->children[side];
      bbox = bsp->bbox[side];
      goto tailcall;
    }
  } else {
    return CheckRegLightSubsector(BSPIDX_LEAF_SUBSECTOR(bspnum), litSurfaces);
  }
}
