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
//**  Copyright (C) 2018-2019 Ketmar Dark
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
#include <limits.h>
#include <float.h>

#include "gamedefs.h"
#include "r_local.h"

// with this defined, it glitches
// you can see it on map01, for example, by firing imp fireball
//#define VV_LADV_CLIPCHECK_REGIONS_LIGHT
//#define VV_LADV_CLIPCHECK_REGIONS_SHADOW

#define VV_LADV_STRANGE_REGION_SORTING


extern VCvarB r_darken;
extern VCvarB r_allow_subtractive_lights;
extern VCvarB r_draw_mobjs;
extern VCvarB r_model_shadows;
extern VCvarB r_draw_pobj;
extern VCvarB r_chasecam;
extern VCvarB r_glow_flat;
extern VCvarB clip_use_1d_clipper;
extern VCvarB r_disable_world_update;

extern VCvarB gl_dbg_wireframe;

static VCvarB clip_adv_regions_shadow("clip_adv_regions_shadow", false, "Clip (1D) shadow regions?", CVAR_PreInit);
static VCvarB clip_adv_regions_light("clip_adv_regions_light", false, "Clip (1D) light regions?", CVAR_PreInit);

static VCvarB r_shadowvol_use_pofs("r_shadowvol_use_pofs", true, "Use PolygonOffset for shadow volumes to reduce some flickering (WARNING: BUGGY!)?", CVAR_Archive);
static VCvarF r_shadowvol_pofs("r_shadowvol_pofs", "20", "DEBUG");
static VCvarF r_shadowvol_pslope("r_shadowvol_pslope", "-0.2", "DEBUG");


/*
  possible shadow volume optimisation:
  for things like pillars (i.e. sectors that has solid walls facing outwards),
  we can construct a shadow silhouette. that is, all connected light-facing
  walls can be replaced with one. this will render much less sides.
  that is, we can effectively turn our pillar into cube.

  we can prolly use sector lines to render this (not segs). i thinkg that it
  will be better to use sector lines to render shadow volumes in any case,
  'cause we don't really need to render one line in several segments.
  that is, one line split by BSP gives us two surfaces, which adds two
  unnecessary polygons to shadow volume. by using linedefs instead, we can
  avoid this. there is no need to create texture coordinates and surfaces
  at all: we can easily calculate all required vertices.

  note that we cannot do the same with floors and ceilings: they can have
  arbitrary shape.

  actually, we can solve this in another way. note that we need to extrude
  only silhouette edges. so we can collect surfaces from connected subsectors
  into "shape", and run silhouette extraction on it. this way we can extrude
  onyl a silhouette. and we can avoid rendering caps if our camera is not
  lie inside any shadow volume (and volume is not clipped by near plane).

  we can easily determine if the camera is inside a volume by checking if
  it lies inside any extruded subsector. as subsectors are convex, this is a
  simple set of point-vs-plane checks. first check if light and camera are
  on a different sides of a surface, and do costly checks only if they are.

  if the camera is outside of shadow volume, we can use faster z-pass method.

  if a light is behing a camera, we can move back frustum plane, so it will
  contain light origin, and clip everything behind it. the same can be done
  for all other frustum planes. or we can build full light-vs-frustum clip.
*/


// ////////////////////////////////////////////////////////////////////////// //
// private data definitions
// ////////////////////////////////////////////////////////////////////////// //

//VCvarI r_max_model_lights("r_max_model_lights", "32", "Maximum lights that can affect one model when we aren't using model shadows.", CVAR_Archive);
VCvarI r_max_model_shadows("r_max_model_shadows", "16", "Maximum number of shadows one model can cast.", CVAR_Archive);

VCvarI r_max_lights("r_max_lights", "64", "Maximum lights.", CVAR_Archive);
static VCvarI r_max_light_segs_all("r_max_light_segs_all", "-1", "Maximum light segments for all lights.", CVAR_Archive);
static VCvarI r_max_light_segs_one("r_max_light_segs_one", "-1", "Maximum light segments for one light.", CVAR_Archive);
// was 128, but with scissored light, there is no sense to limit it anymore
static VCvarI r_max_shadow_segs_all("r_max_shadow_segs_all", "-1", "Maximum shadow segments for all lights.", CVAR_Archive);
static VCvarI r_max_shadow_segs_one("r_max_shadow_segs_one", "-1", "Maximum shadow segments for one light.", CVAR_Archive);

VCvarF r_light_filter_static_coeff("r_light_filter_static_coeff", "0.2", "How close static lights should be to be filtered out?\n(0.1-0.3 is usually ok).", CVAR_Archive);
VCvarB r_allow_static_light_filter("r_allow_static_light_filter", true, "Allow filtering of static lights?", CVAR_Archive);
VCvarI r_static_light_filter_mode("r_static_light_filter_mode", "0", "Filter only decorations(0), or all lights(1)?", CVAR_Archive);

VCvarB dbg_adv_light_notrace_mark("dbg_adv_light_notrace_mark", false, "Mark notrace lights red?", CVAR_PreInit);

//static VCvarB r_advlight_opt_trace("r_advlight_opt_trace", true, "Try to skip shadow volumes when a light can cast no shadow.", CVAR_Archive|CVAR_PreInit);
static VCvarB r_advlight_opt_scissor("r_advlight_opt_scissor", true, "Use scissor rectangle to limit light overdraws.", CVAR_Archive);
// this is wrong for now
static VCvarB r_advlight_opt_frustum_full("r_advlight_opt_frustum_full", false, "Optimise 'light is in frustum' case.", CVAR_Archive);
static VCvarB r_advlight_opt_frustum_back("r_advlight_opt_frustum_back", false, "Optimise 'light is in frustum' case.", CVAR_Archive);

VCvarB r_advlight_opt_optimise_scissor("r_advlight_opt_optimise_scissor", true, "Optimise scissor with lit geometry bounds.", CVAR_Archive);


// ////////////////////////////////////////////////////////////////////////// //
// code
// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  VRenderLevelShadowVolume::RefilterStaticLights
//
//==========================================================================
void VRenderLevelShadowVolume::RefilterStaticLights () {
  staticLightsFiltered = true;

  float coeff = r_light_filter_static_coeff;

  int llen = Lights.length();
  int actlights = 0;
  for (int currlidx = 0; currlidx < llen; ++currlidx) {
    light_t &cl = Lights[currlidx];
    if (coeff > 0) {
      cl.active = (cl.radius > 6); // arbitrary limit
    } else {
      cl.active = true;
    }
    if (cl.active) ++actlights;
  }
  if (actlights < 6) return; // arbitrary limit

  if (!r_allow_static_light_filter) return; // no filtering
  if (coeff <= 0) return; // no filtering
  if (coeff > 8) coeff = 8;

  const bool onlyDecor = (r_static_light_filter_mode.asInt() == 0);

  for (int currlidx = 0; currlidx < llen; ++currlidx) {
    light_t &cl = Lights[currlidx];
    if (!cl.active) continue; // already filtered out
    if (onlyDecor && !cl.owner) continue;
    // remove nearby lights with radius less than ours (or ourself if we'll hit bigger light)
    float radsq = (cl.radius*cl.radius)*coeff;
    for (int nlidx = currlidx+1; nlidx < llen; ++nlidx) {
      light_t &nl = Lights[nlidx];
      if (!nl.active) continue; // already filtered out
      if (onlyDecor && !nl.owner) continue;
      const float distsq = length2DSquared(cl.origin-nl.origin);
      if (distsq >= radsq) continue;

      // check potential visibility
      /*
      subsector_t *sub = Level->PointInSubsector(nl.origin);
      const vuint8 *dyn_facevis = Level->LeafPVS(sub);
      if (!(dyn_facevis[nl.leafnum>>3]&(1<<(nl.leafnum&7)))) continue;
      */

      // if we cannot trace a line between two lights, they are prolly divided by a wall or a flat
      linetrace_t Trace;
      if (!Level->TraceLine(Trace, nl.origin, cl.origin, SPF_NOBLOCKSIGHT)) continue;

      if (nl.radius <= cl.radius) {
        // deactivate nl
        nl.active = false;
      } else /*if (nl.radius > cl.radius)*/ {
        // deactivate cl
        cl.active = false;
        // there is no sense to continue
        break;
      }
    }
  }

  actlights = 0;
  for (int currlidx = 0; currlidx < llen; ++currlidx) {
    light_t &cl = Lights[currlidx];
    if (cl.active) {
      ++actlights;
    } else {
      //if (cl.owner) GCon->Logf(NAME_Debug, "ADVR: filtered static light from `%s`; org=(%g,%g,%g); radius=%g", cl.owner->GetClass()->GetName(), cl.origin.x, cl.origin.y, cl.origin.z, cl.radius);
    }
  }

  GCon->Logf("ADVRENDERER: filtered %d static lights out of %d (%d left)", llen-actlights, llen, actlights);
}


//==========================================================================
//
//  VRenderLevelShadowVolume::LightPointAmbient
//
//==========================================================================
vuint32 VRenderLevelShadowVolume::LightPointAmbient (const TVec &p, float radius, const subsector_t *psub) {
  if (FixedLight) return FixedLight|(FixedLight<<8)|(FixedLight<<16)|(FixedLight<<24);

  const subsector_t *sub = (psub ? psub : Level->PointInSubsector(p));
  float l = 0.0f, lr = 0.0f, lg = 0.0f, lb = 0.0f;
  CalculateSubAmbient(l, lr, lg, lb, sub, p, radius, nullptr);

  return
    (((vuint32)clampToByte((int)l))<<24)|
    (((vuint32)clampToByte((int)lr))<<16)|
    (((vuint32)clampToByte((int)lg))<<8)|
    ((vuint32)clampToByte((int)lb));
}


//==========================================================================
//
//  VRenderLevelShadowVolume::BuildLightMap
//
//==========================================================================
void VRenderLevelShadowVolume::BuildLightMap (surface_t *surf) {
}


//==========================================================================
//
//  VRenderLevelShadowVolume::DrawShadowSurfaces
//
//==========================================================================
void VRenderLevelShadowVolume::DrawShadowSurfaces (surface_t *InSurfs, texinfo_t *texinfo,
                                               VEntity *SkyBox, bool CheckSkyBoxAlways, int LightCanCross)
{
  if (!InSurfs) return;

  if (!texinfo || texinfo->Tex->Type == TEXTYPE_Null) return;
  if (texinfo->Alpha < 1.0f || texinfo->Additive) return;
  if (LightCanCross > 0 && texinfo->Tex->isSeeThrough()) return; // has holes, don't bother

  if (SkyBox && (SkyBox->EntityFlags&VEntity::EF_FixedModel)) SkyBox = nullptr;

  if (texinfo->Tex == GTextureManager.getIgnoreAnim(skyflatnum) ||
      (CheckSkyBoxAlways && (SkyBox && SkyBox->eventSkyBoxGetAlways())))
  {
    return;
  }

  // ignore everything that is placed behind player's back
  // we shouldn't have many of those, so check them in the loop below
  // but do this only if the light is behind a player
  bool checkFrustum;
  if (!r_advlight_opt_frustum_back) {
    checkFrustum = false;
  } else {
    checkFrustum = view_frustum.checkSphere(CurrLightPos, CurrLightRadius, TFrustum::NearBit);
  }

  // TODO: if light is behing a camera, we can move back frustum plane, so it will
  //       contain light origin, and clip everything behind it. the same can be done
  //       for all other frustum planes.

  for (surface_t *surf = InSurfs; surf; surf = surf->next) {
    if (surf->count < 3) continue; // just in case

    // for two-sided walls, we want to leave only one surface, otherwise z-fighting will occur
    // also, don't bother with it at all if texture has holes
    if (LightCanCross < 0) {
      // horizon
      // k8: can horizont surfaces block light? i think they shouldn't
      //if (!surf->IsVisible(vieworg)) return; // viewer is in back side or on plane
      continue;
    }

    // floor or ceiling? ignore masked
    if (surf->GetNormalZ()) {
      VTexture *tex = surf->texinfo->Tex;
      if (!tex || tex->Type == TEXTYPE_Null) continue;
      if (surf->texinfo->Alpha < 1.0f || surf->texinfo->Additive) continue;
      if (tex->isSeeThrough()) continue; // this is masked texture
    }

    // leave only surface that light can see (it shouldn't matter for texturing which one we'll use)
    const float dist = DotProduct(CurrLightPos, surf->GetNormal())-surf->GetDist();
    // k8: use `<=` and `>=` for radius checks, 'cause why not?
    //     light completely fades away at that distance
    if (dist <= 0.0f || dist >= CurrLightRadius) return; // light is too far away

    if (checkFrustum) {
      if (!view_frustum.checkVerts(surf->verts, (unsigned)surf->count, TFrustum::NearBit)) continue;
    }

    Drawer->RenderSurfaceShadowVolume(surf, CurrLightPos, CurrLightRadius);
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderShadowLine
//
//  Clips the given segment and adds any visible pieces to the line list.
//
//==========================================================================
void VRenderLevelShadowVolume::RenderShadowLine (subsector_t *sub, sec_region_t *secregion, drawseg_t *dseg) {
  seg_t *seg = dseg->seg;
  if (!seg->linedef) return; // miniseg

  // note that we don't want to filter out shadows that are behind
  // but we are want to filter out surfaces that cannot possibly block light
  // (i.e. back-surfaces with respect to light origin)
  const float dist = DotProduct(CurrLightPos, seg->normal)-seg->dist;
  //if (dist < -CurrLightRadius || dist > CurrLightRadius) return; // light is too far away
  //if (fabsf(dist) >= CurrLightRadius) return;
  if (dist <= 0.0f || dist >= CurrLightRadius) return;

  /*
  {
    TVec v1 = *seg->v1;
    TVec v2 = *seg->v2;
    const TVec r1 = CurrLightPos-v1;
    const TVec r2 = CurrLightPos-v2;
    const TVec n1 = Normalise(CrossProduct(r1, r2));
    const TVec n2 = Normalise(CrossProduct(r2, r1));
    const float d1 = DotProduct(n1, CurrLightPos);
    const float d2 = DotProduct(n2, CurrLightPos);
    if (fabsf(n1.z) < 0.001f || fabsf(n2.z) < 0.001f) {
      GCon->Logf(NAME_Debug, "LINE #%d: n1=(%g,%g,%g); n2=(%g,%g,%g); d1=%g; d2=%g", (int)(ptrdiff_t)(seg->linedef-&Level->Lines[0]), n1.x, n1.y, n1.z, n2.x, n2.y, n2.z, d1, d2);
      return;
    }
  }
  */

/*
    k8: i don't know what Janis wanted to accomplish with this, but it actually
        makes clipping WORSE due to limited precision
  // clip sectors that are behind rendered segs
  TVec v1 = *seg->v1;
  TVec v2 = *seg->v2;
  const TVec r1 = CurrLightPos-v1;
  const TVec r2 = CurrLightPos-v2;
  const float D1 = DotProduct(Normalise(CrossProduct(r1, r2)), CurrLightPos);
  const float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), CurrLightPos);

  // there might be a better method of doing this, but this one works for now...
       if (D1 > CurrLightRadius && D2 < -CurrLightRadius) v2 += (v2-v1)*D1/(D1-D2);
  else if (D2 > CurrLightRadius && D1 < -CurrLightRadius) v1 += (v1-v2)*D2/(D2-D1);

  if (!LightClip.IsRangeVisible(v2, v1)) return;
*/
  if (!LightClip.IsRangeVisible(*seg->v2, *seg->v1)) return;

#if 1
  // k8: this drops some segs that may leak without proper frustum culling
  // k8: this seems to be unnecessary now
  // k8: yet leave it there in the hope that it will reduce GPU overdrawing
  if (!LightClip.CheckSegFrustum(sub, seg)) return;
#endif

  //line_t *linedef = seg->linedef;
  //side_t *sidedef = seg->sidedef;

  VEntity *skybox = secregion->eceiling.splane->SkyBox;
  if (dseg->mid) DrawShadowSurfaces(dseg->mid->surfs, &dseg->mid->texinfo, skybox, false, (seg->backsector ? 1 : 0));
  if (seg->backsector) {
    // two sided line
    if (dseg->top) DrawShadowSurfaces(dseg->top->surfs, &dseg->top->texinfo, skybox, false, 0);
    //k8: horizon/sky cannot block light
    //if (dseg->topsky) DrawShadowSurfaces(dseg->topsky->surfs, &dseg->topsky->texinfo, skybox, false, -1);
    if (dseg->bot) DrawShadowSurfaces(dseg->bot->surfs, &dseg->bot->texinfo, skybox, false, 0);
    for (segpart_t *sp = dseg->extra; sp; sp = sp->next) {
      DrawShadowSurfaces(sp->surfs, &sp->texinfo, skybox, false, 0);
    }
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderShadowSecSurface
//
//  this is used for floor and ceilings
//
//==========================================================================
void VRenderLevelShadowVolume::RenderShadowSecSurface (sec_surface_t *ssurf, VEntity *SkyBox) {
  //const sec_plane_t &plane = *ssurf->secplane;
  if (!ssurf->esecplane.splane->pic) return;

  // note that we don't want to filter out shadows that are behind
  // but we are want to filter out surfaces that cannot possibly block light
  // (i.e. back-surfaces with respect to light origin)
  //const float dist = DotProduct(CurrLightPos, plane.normal)-plane.dist;
  const float dist = ssurf->PointDist(CurrLightPos);
  //if (dist < -CurrLightRadius || dist > CurrLightRadius) return; // light is too far away
  //if (fabsf(dist) >= CurrLightRadius) return;
  if (dist <= 0.0f || dist >= CurrLightRadius) return;

  DrawShadowSurfaces(ssurf->surfs, &ssurf->texinfo, SkyBox, true, 0);
}


//==========================================================================
//
//  VRenderLevelShadowVolume::AddPolyObjToLightClipper
//
//  we have to do this separately, because for now we have to add
//  invisible segs to clipper too
//  i don't yet know why
//
//==========================================================================
void VRenderLevelShadowVolume::AddPolyObjToLightClipper (VViewClipper &clip, subsector_t *sub, bool asShadow) {
  if (sub && sub->HasPObjs() && r_draw_pobj && clip_use_1d_clipper) {
    for (auto &&it : sub->PObjFirst()) {
      polyobj_t *pobj = it.value();
      seg_t **polySeg = pobj->segs;
      for (int polyCount = pobj->numsegs; polyCount--; ++polySeg) {
        seg_t *seg = (*polySeg)->drawsegs->seg;
        if (seg->linedef) {
          clip.CheckAddClipSeg(seg, nullptr/*mirror*/, asShadow);
        }
      }
    }
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderShadowPolyObj
//
//==========================================================================
void VRenderLevelShadowVolume::RenderShadowPolyObj (subsector_t *sub) {
  if (sub && sub->HasPObjs() && r_draw_pobj) {
    subregion_t *region = sub->regions;
    sec_region_t *secregion = region->secregion;
    for (auto &&it : sub->PObjFirst()) {
      polyobj_t *pobj = it.value();
      seg_t **polySeg = pobj->segs;
      for (int polyCount = pobj->numsegs; polyCount--; ++polySeg) {
        RenderShadowLine(sub, secregion, (*polySeg)->drawsegs);
      }
    }
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderShadowSubRegion
//
//  Determine floor/ceiling planes.
//  Draw one or more line segments.
//
//==========================================================================
void VRenderLevelShadowVolume::RenderShadowSubRegion (subsector_t *sub, subregion_t *region) {
  const bool nextFirst = NeedToRenderNextSubFirst(region);
  if (nextFirst) RenderShadowSubRegion(sub, region->next);

  sec_region_t *secregion = region->secregion;

  if (!clip_adv_regions_shadow || LightClip.ClipLightCheckRegion(region, sub, true)) {
    drawseg_t *ds = region->lines;
    for (int count = sub->numlines; count--; ++ds) RenderShadowLine(sub, secregion, ds);
  }

  {
    sec_surface_t *fsurf[4];
    GetFlatSetToRender(sub, region, fsurf);

    if (fsurf[0]) RenderShadowSecSurface(fsurf[0], secregion->efloor.splane->SkyBox);
    if (fsurf[1]) RenderShadowSecSurface(fsurf[1], secregion->efloor.splane->SkyBox);

    if (fsurf[2]) RenderShadowSecSurface(fsurf[2], secregion->efloor.splane->SkyBox);
    if (fsurf[3]) RenderShadowSecSurface(fsurf[3], secregion->eceiling.splane->SkyBox);
  }

  if (!nextFirst && region->next) return RenderShadowSubRegion(sub, region->next);
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderShadowSubsector
//
//==========================================================================
void VRenderLevelShadowVolume::RenderShadowSubsector (int num) {
  subsector_t *sub = &Level->Subsectors[num];

  // don't do this check for shadows
  //if (!(LightBspVis[num>>3]&(1<<(num&7))) || !(BspVis[num>>3]&(1<<(num&7)))) return;

  if (!sub->sector->linecount) return; // skip sectors containing original polyobjs

  if (LightClip.ClipLightCheckSubsector(sub, true)) {
    // if our light is in frustum, out-of-frustum subsectors are not interesting
    //FIXME: pass "need frustum check" flag to other functions
    bool needToRender = true;
    if (CurrLightInFrustum && !(BspVis[num>>3]&(1u<<(num&7)))) {
      // this subsector is invisible, check if it is in frustum
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
      needToRender = view_frustum.checkBox(bbox);
    }

    if (needToRender) {
      // update world
      if (sub->updateWorldFrame != updateWorldFrame) {
        sub->updateWorldFrame = updateWorldFrame;
        if (!r_disable_world_update) UpdateSubRegion(sub, sub->regions);
      }
      // render the polyobj in the subsector first, and add it to clipper
      // this blocks view with polydoors
      RenderShadowPolyObj(sub);
      AddPolyObjToLightClipper(LightClip, sub, true);
      RenderShadowSubRegion(sub, sub->regions);
    }
  }

  // add subsector's segs to the clipper
  // clipping against mirror is done only for vertical mirror planes
  LightClip.ClipLightAddSubsectorSegs(sub, true);
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderShadowBSPNode
//
//  Renders all subsectors below a given node, traversing subtree
//  recursively. Just call with BSP root.
//
//==========================================================================
void VRenderLevelShadowVolume::RenderShadowBSPNode (int bspnum, const float *bbox, bool LimitLights) {
  if (LimitLights) {
    if (r_max_shadow_segs_all >= 0 && AllShadowsNumber > r_max_shadow_segs_all) return;
    if (r_max_shadow_segs_one >= 0 && CurrShadowsNumber > r_max_shadow_segs_one) return;
  }

#ifdef VV_CLIPPER_FULL_CHECK
  if (LightClip.ClipIsFull()) return;
#endif

  if (!LightClip.ClipLightIsBBoxVisible(bbox)) return;

  if (bspnum == -1) {
    if (LimitLights) { ++CurrShadowsNumber; ++AllShadowsNumber; }
    return RenderShadowSubsector(0);
  }

  // found a subsector?
  if (BSPIDX_IS_NON_LEAF(bspnum)) {
    node_t *bsp = &Level->Nodes[bspnum];
    // decide which side the light is on
    const float dist = DotProduct(CurrLightPos, bsp->normal)-bsp->dist;
    if (dist >= CurrLightRadius) {
      // light is completely on front side
      return RenderShadowBSPNode(bsp->children[0], bsp->bbox[0], LimitLights);
    } else if (dist <= -CurrLightRadius) {
      // light is completely on back side
      return RenderShadowBSPNode(bsp->children[1], bsp->bbox[1], LimitLights);
    } else {
      //int side = bsp->PointOnSide(CurrLightPos);
      unsigned side = (unsigned)(dist <= 0.0f);
      // recursively divide front space
      RenderShadowBSPNode(bsp->children[side], bsp->bbox[side], LimitLights);
      // always divide back space for shadows
      side ^= 1;
      return RenderShadowBSPNode(bsp->children[side], bsp->bbox[side], LimitLights);
    }
  } else {
    if (LimitLights) { ++CurrShadowsNumber; ++AllShadowsNumber; }
    return RenderShadowSubsector(BSPIDX_LEAF_SUBSECTOR(bspnum));
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::DrawLightSurfaces
//
//==========================================================================
void VRenderLevelShadowVolume::DrawLightSurfaces (surface_t *InSurfs, texinfo_t *texinfo,
                                              VEntity *SkyBox, bool CheckSkyBoxAlways, int LightCanCross)
{
  if (!InSurfs) return;

  if (!texinfo || texinfo->Tex->Type == TEXTYPE_Null) return;
  if (texinfo->Alpha < 1.0f || texinfo->Additive) return;

  if (SkyBox && (SkyBox->EntityFlags&VEntity::EF_FixedModel)) SkyBox = nullptr;

  if (texinfo->Tex == GTextureManager.getIgnoreAnim(skyflatnum) ||
      (CheckSkyBoxAlways && (SkyBox && SkyBox->eventSkyBoxGetAlways())))
  {
    return;
  }

  for (surface_t *surf = InSurfs; surf; surf = surf->next) {
    if (surf->count < 3) continue; // just in case
    if (!surf->plvisible) continue; // viewer is in back side or on plane
    const float dist = DotProduct(CurrLightPos, surf->GetNormal())-surf->GetDist();
    if (dist <= 0.0f || dist >= CurrLightRadius) continue; // light is too far away, or surface is not lit
    Drawer->DrawSurfaceLight(surf);
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderLightLine
//
//  clips the given segment and adds any visible pieces to the line list
//
//==========================================================================
void VRenderLevelShadowVolume::RenderLightLine (sec_region_t *secregion, drawseg_t *dseg) {
  const seg_t *seg = dseg->seg;

  if (!seg->linedef) return; // miniseg

  const float dist = DotProduct(CurrLightPos, seg->normal)-seg->dist;
  //if (dist <= -CurrLightRadius || dist > CurrLightRadius) return; // light sphere is not touching a plane
  if (fabsf(dist) >= CurrLightRadius) return;

/*
    k8: i don't know what Janis wanted to accomplish with this, but it actually
        makes clipping WORSE due to limited precision
  // clip sectors that are behind rendered segs
  TVec v1 = *seg->v1;
  TVec v2 = *seg->v2;
  TVec r1 = CurrLightPos-v1;
  TVec r2 = CurrLightPos-v2;
  float D1 = DotProduct(Normalise(CrossProduct(r1, r2)), CurrLightPos);
  float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), CurrLightPos);

  // there might be a better method of doing this, but this one works for now...
       if (D1 > CurrLightRadius && D2 < -CurrLightRadius) v2 += (v2-v1)*D1/(D1-D2);
  else if (D2 > CurrLightRadius && D1 < -CurrLightRadius) v1 += (v1-v2)*D2/(D2-D1);

  if (!LightClip.IsRangeVisible(v2, v1)) return;
*/
  if (!LightClip.IsRangeVisible(*seg->v2, *seg->v1)) return;

  VEntity *skybox = secregion->eceiling.splane->SkyBox;
  if (dseg->mid) DrawLightSurfaces(dseg->mid->surfs, &dseg->mid->texinfo, skybox, false, (seg->backsector ? 1 : 0));
  if (seg->backsector) {
    // two sided line
    if (dseg->top) DrawLightSurfaces(dseg->top->surfs, &dseg->top->texinfo, skybox, false, 0);
    //k8: horizon/sky cannot block light
    //if (dseg->topsky) DrawLightSurfaces(dseg->topsky->surfs, &dseg->topsky->texinfo, skybox, false, -1);
    if (dseg->bot) DrawLightSurfaces(dseg->bot->surfs, &dseg->bot->texinfo, skybox, false, 0);
    for (segpart_t *sp = dseg->extra; sp; sp = sp->next) {
      DrawLightSurfaces(sp->surfs, &sp->texinfo, skybox, false, 0);
    }
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderLightSecSurface
//
//  this is used for floor and ceilings
//
//==========================================================================
void VRenderLevelShadowVolume::RenderLightSecSurface (sec_surface_t *ssurf, VEntity *SkyBox) {
  //const sec_plane_t &plane = *ssurf->secplane;
  if (!ssurf->esecplane.splane->pic) return;

  //const float dist = DotProduct(CurrLightPos, plane.normal)-plane.dist;
  const float dist = ssurf->PointDist(CurrLightPos);
  //if (dist <= -CurrLightRadius || dist > CurrLightRadius) return; // light is in back side or on plane
  if (fabsf(dist) >= CurrLightRadius) return;

  DrawLightSurfaces(ssurf->surfs, &ssurf->texinfo, SkyBox, true, 0);
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderLightPolyObj
//
//==========================================================================
void VRenderLevelShadowVolume::RenderLightPolyObj (subsector_t *sub) {
  if (sub && sub->HasPObjs() && r_draw_pobj) {
    subregion_t *region = sub->regions;
    sec_region_t *secregion = region->secregion;
    for (auto &&it : sub->PObjFirst()) {
      polyobj_t *pobj = it.value();
      seg_t **polySeg = pobj->segs;
      for (int polyCount = pobj->numsegs; polyCount--; ++polySeg) {
        RenderLightLine(secregion, (*polySeg)->drawsegs);
      }
    }
  }
}

//==========================================================================
//
//  VRenderLevelShadowVolume::RenderLightSubRegion
//
//  Determine floor/ceiling planes.
//  Draw one or more line segments.
//
//==========================================================================
void VRenderLevelShadowVolume::RenderLightSubRegion (subsector_t *sub, subregion_t *region) {
  const bool nextFirst = NeedToRenderNextSubFirst(region);
  if (nextFirst) RenderLightSubRegion(sub, region->next);

  sec_region_t *secregion = region->secregion;

  if (!clip_adv_regions_light || LightClip.ClipLightCheckRegion(region, sub, false)) {
    drawseg_t *ds = region->lines;
    for (int count = sub->numlines; count--; ++ds) RenderLightLine(secregion, ds);
  }

  {
    sec_surface_t *fsurf[4];
    GetFlatSetToRender(sub, region, fsurf);

    if (fsurf[0]) RenderLightSecSurface(fsurf[0], secregion->efloor.splane->SkyBox);
    if (fsurf[1]) RenderLightSecSurface(fsurf[1], secregion->efloor.splane->SkyBox);

    if (fsurf[2]) RenderLightSecSurface(fsurf[2], secregion->eceiling.splane->SkyBox);
    if (fsurf[3]) RenderLightSecSurface(fsurf[3], secregion->eceiling.splane->SkyBox);
  }

  if (!nextFirst && region->next) return RenderLightSubRegion(sub, region->next);
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderLightSubsector
//
//==========================================================================
void VRenderLevelShadowVolume::RenderLightSubsector (int num) {
  vassert(num >= 0 && num < Level->NumSubsectors);
  subsector_t *sub = &Level->Subsectors[num];

  if (!sub->sector->linecount) return; // skip sectors containing original polyobjs

  // `LightBspVis` is already an intersection, no need to check `BspVis` here
  //if (!(LightBspVis[num>>3]&(1<<(num&7))) || !(BspVis[num>>3]&(1<<(num&7)))) return;

  if (LightBspVis[(unsigned)num>>3]&(1u<<((unsigned)num&7))) {
    if (LightClip.ClipLightCheckSubsector(sub, false)) {
      // update world
      if (sub->updateWorldFrame != updateWorldFrame) {
        sub->updateWorldFrame = updateWorldFrame;
        if (!r_disable_world_update) UpdateSubRegion(sub, sub->regions);
      }
      // render the polyobj in the subsector first, and add it to clipper
      // this blocks view with polydoors
      RenderLightPolyObj(sub);
      AddPolyObjToLightClipper(LightClip, sub, false);
      RenderLightSubRegion(sub, sub->regions);
    }
  }

  // add subsector's segs to the clipper
  // clipping against mirror is done only for vertical mirror planes
  LightClip.ClipLightAddSubsectorSegs(sub, false);
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderLightBSPNode
//
//  Renders all subsectors below a given node, traversing subtree
//  recursively. Just call with BSP root.
//
//==========================================================================
void VRenderLevelShadowVolume::RenderLightBSPNode (int bspnum, const float *bbox, bool LimitLights) {
  if (LimitLights) {
     if (r_max_light_segs_all >= 0 && AllLightsNumber > r_max_light_segs_all) return;
     if (r_max_light_segs_one >= 0 && CurrLightsNumber > r_max_light_segs_one) return;
  }

#ifdef VV_CLIPPER_FULL_CHECK
  if (LightClip.ClipIsFull()) return;
#endif

  if (!LightClip.ClipLightIsBBoxVisible(bbox)) return;
  //if (!CheckSphereVsAABBIgnoreZ(bbox, CurrLightPos, CurrLightRadius)) return;

  if (bspnum == -1) {
    if (LimitLights) { ++CurrLightsNumber; ++AllLightsNumber; }
    return RenderLightSubsector(0);
  }

  // found a subsector?
  if (BSPIDX_IS_NON_LEAF(bspnum)) {
    node_t *bsp = &Level->Nodes[bspnum];
    // decide which side the light is on
    const float dist = DotProduct(CurrLightPos, bsp->normal)-bsp->dist;
    if (dist >= CurrLightRadius) {
      // light is completely on front side
      return RenderLightBSPNode(bsp->children[0], bsp->bbox[0], LimitLights);
    } else if (dist <= -CurrLightRadius) {
      // light is completely on back side
      return RenderLightBSPNode(bsp->children[1], bsp->bbox[1], LimitLights);
    } else {
      //int side = bsp->PointOnSide(CurrLightPos);
      unsigned side = (unsigned)(dist <= 0.0f);
      // recursively divide front space
      RenderLightBSPNode(bsp->children[side], bsp->bbox[side], LimitLights);
      // possibly divide back space
      side ^= 1;
      return RenderLightBSPNode(bsp->children[side], bsp->bbox[side], LimitLights);
    }
  } else {
    if (LimitLights) { ++CurrLightsNumber; ++AllLightsNumber; }
    return RenderLightSubsector(BSPIDX_LEAF_SUBSECTOR(bspnum));
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::RenderLightShadows
//
//==========================================================================
void VRenderLevelShadowVolume::RenderLightShadows (VEntity *ent, vuint32 dlflags, const refdef_t *RD,
                                               const VViewClipper *Range,
                                               TVec &Pos, float Radius, float LightMin, vuint32 Color,
                                               bool LimitLights, TVec coneDir, float coneAngle)
{
  if ((r_max_lights >= 0 && LightsRendered >= r_max_lights) || Radius <= LightMin || gl_dbg_wireframe) return;

  if (!CalcLightVis(Pos, Radius-LightMin)) return;
  CurrLightRadius = Radius; // we need full radius, not modified

  if (r_advlight_opt_optimise_scissor && !LitSurfaces && !r_models) return; // no lit surfaces, nothing to do
  if (LightVisSubs.length() == 0 && !r_models) return; // just in case

  CurrLightColor = Color;
  // if our light is in frustum, ignore any out-of-frustum polys
  if (r_advlight_opt_frustum_full) {
    CurrLightInFrustum = view_frustum.checkSphere(Pos, Radius-LightMin+4.0f);
  } else {
    CurrLightInFrustum = false; // don't do frustum optimisations
  }

  bool allowShadows = doShadows;

  // if we want model shadows, always do full rendering
  if (!allowShadows && r_draw_mobjs && r_models && r_model_shadows) allowShadows = true;

  if (dlflags&dlight_t::NoShadow) allowShadows = false;

  if (!r_allow_shadows) allowShadows = false;

  if (!allowShadows && dbg_adv_light_notrace_mark) {
    //Color = 0xffff0000U;
    Color = 0xffff00ffU; // purple; it should be very noticeable
  }

  ++LightsRendered;

  CurrShadowsNumber = 0;
  CurrLightsNumber = 0;

  //  0 if scissor is empty
  // -1 if scissor has no sense (should not be used)
  //  1 if scissor is set
  int hasScissor = 1;
  int scoord[4];
  bool checkModels = false;
  float dummyBBox[6];

  //GCon->Logf("LBB:(%f,%f,%f)-(%f,%f,%f)", LitBBox[0], LitBBox[1], LitBBox[2], LitBBox[3], LitBBox[4], LitBBox[5]);

  // setup light scissor rectangle
  if (r_advlight_opt_scissor) {
    hasScissor = Drawer->SetupLightScissor(Pos, Radius-LightMin, scoord, (r_advlight_opt_optimise_scissor ? LitBBox : nullptr));
    if (hasScissor <= 0) {
      // something is VERY wrong (-1), or scissor is empty (0)
      Drawer->ResetScissor();
      if (!hasScissor && r_advlight_opt_optimise_scissor && !r_models) return; // empty scissor
      checkModels = r_advlight_opt_optimise_scissor;
      hasScissor = 0;
      scoord[0] = scoord[1] = 0;
      scoord[2] = Drawer->getWidth();
      scoord[3] = Drawer->getHeight();
    } else {
      if (scoord[0] == 0 && scoord[1] == 0 && scoord[2] == Drawer->getWidth() && scoord[3] == Drawer->getHeight()) {
        hasScissor = 0;
      }
    }
  }

  // if there are no lit surfaces oriented away from camera, it cannot possibly be in shadow volume
  // nope, it is wrong
  //bool useZPass = !HasBackLit;
  //if (useZPass) GCon->Log("*** ZPASS");
  //useZPass = false;
  bool useZPass = false;

  /* nope
  if ((CurrLightPos-vieworg).lengthSquared() > CurrLightRadius*CurrLightRadius) {
    useZPass = true;
  }
  */

  if (r_max_light_segs_all < 0 && r_max_light_segs_one < 0) LimitLights = false;

  BuildMobjsInCurrLight(allowShadows);

  // if we want to scissor on geometry, check if any lit model is out of our light bbox.
  // stop right here! say, is there ANY reason to not limit light box with map geometry?
  // after all, most of the things that can receive light is contained inside a map.
  // still, we may miss some lighting on models from flying lights that cannot touch
  // any geometry at all. to somewhat ease this case, rebuild light box when the light
  // didn't touched anything.
  if (allowShadows && checkModels) {
    if (mobjsInCurrLight.length() == 0) return; // nothing to do, as it is guaranteed that light cannot touch map geometry
    float xbbox[6] = {0};
    /*
    xbbox[0+0] = LitBBox[0].x;
    xbbox[0+1] = LitBBox[0].y;
    xbbox[0+2] = LitBBox[0].z;
    xbbox[3+0] = LitBBox[1].x;
    xbbox[3+1] = LitBBox[1].y;
    xbbox[3+2] = LitBBox[1].z;
    */
    bool wasHit = false;
    for (auto &&ment : mobjsInCurrLight) {
      if (ment == ViewEnt && (!r_chasecam || ViewEnt != cl->MO)) continue; // don't draw camera actor
      // skip things in subsectors that are not visible
      const int SubIdx = (int)(ptrdiff_t)(ment->SubSector-Level->Subsectors);
      if (!(LightBspVis[SubIdx>>3]&(1<<(SubIdx&7)))) continue;
      if (ment->Radius < 1) continue;
      if (!HasEntityAliasModel(ment)) continue;
      // assume that it is not bigger than its radius
      float zup, zdown;
      if (ment->Height < 2) {
        //GCon->Logf("  <%s>: height=%f; radius=%f", *ment->GetClass()->GetFullName(), ment->Height, ment->Radius);
        zup = ment->Radius;
        zdown = ment->Radius;
      } else {
        zup = ment->Height;
        zdown = 0;
      }
      if (wasHit) {
        xbbox[0+0] = min2(xbbox[0+0], ment->Origin.x-ment->Radius);
        xbbox[0+1] = min2(xbbox[0+1], ment->Origin.y-ment->Radius);
        xbbox[0+2] = min2(xbbox[0+2], ment->Origin.z-zup);
        xbbox[3+0] = max2(xbbox[3+0], ment->Origin.x+ment->Radius);
        xbbox[3+1] = max2(xbbox[3+1], ment->Origin.y+ment->Radius);
        xbbox[3+2] = max2(xbbox[3+2], ment->Origin.z+zdown);
      } else {
        wasHit = true;
        xbbox[0+0] = ment->Origin.x-ment->Radius;
        xbbox[0+1] = ment->Origin.y-ment->Radius;
        xbbox[0+2] = ment->Origin.z-zup;
        xbbox[3+0] = ment->Origin.x+ment->Radius;
        xbbox[3+1] = ment->Origin.y+ment->Radius;
        xbbox[3+2] = ment->Origin.z+zdown;
      }
    }
    if (wasHit &&
        (xbbox[0+0] < LitBBox[0].x || xbbox[0+1] < LitBBox[0].y || xbbox[0+2] < LitBBox[0].z ||
         xbbox[3+0] > LitBBox[1].x || xbbox[3+1] > LitBBox[1].y || xbbox[3+2] > LitBBox[1].z))
    {
      /*
      GCon->Logf("fixing light bbox; old=(%f,%f,%f)-(%f,%f,%f); new=(%f,%f,%f)-(%f,%f,%f)",
        LitBBox[0].x, LitBBox[0].y, LitBBox[0].z,
        LitBBox[1].x, LitBBox[1].y, LitBBox[1].z,
        xbbox[0], xbbox[1], xbbox[2], xbbox[3], xbbox[4], xbbox[5]);
      */
      LitBBox[0].x = min2(xbbox[0+0], LitBBox[0].x);
      LitBBox[0].y = min2(xbbox[0+1], LitBBox[0].y);
      LitBBox[0].z = min2(xbbox[0+2], LitBBox[0].z);
      LitBBox[1].x = max2(xbbox[3+0], LitBBox[1].x);
      LitBBox[1].y = max2(xbbox[3+1], LitBBox[1].y);
      LitBBox[1].z = max2(xbbox[3+2], LitBBox[1].z);
      hasScissor = Drawer->SetupLightScissor(Pos, Radius-LightMin, scoord, LitBBox);
      if (hasScissor <= 0) {
        // something is VERY wrong (-1), or scissor is empty (0)
        Drawer->ResetScissor();
        if (!hasScissor) return; // empty scissor
        hasScissor = 0;
        scoord[0] = scoord[1] = 0;
        scoord[2] = Drawer->getWidth();
        scoord[3] = Drawer->getHeight();
      } else {
        if (scoord[0] == 0 && scoord[1] == 0 && scoord[2] == Drawer->getWidth() && scoord[3] == Drawer->getHeight()) {
          hasScissor = 0;
        }
      }
    }
  }

  // do shadow volumes
  Drawer->BeginLightShadowVolumes(CurrLightPos, CurrLightRadius, useZPass, hasScissor, scoord, coneDir, coneAngle);
  LightClip.ClearClipNodes(CurrLightPos, Level, CurrLightRadius);
  if (allowShadows) {
    if (r_shadowvol_use_pofs) {
      glEnable(GL_POLYGON_OFFSET_FILL);
      // pull forward
      if (Drawer->CanUseRevZ()) {
        glPolygonOffset(r_shadowvol_pslope, -r_shadowvol_pofs);
      } else {
        glPolygonOffset(-r_shadowvol_pslope, r_shadowvol_pofs);
      }
    }
    if (r_max_shadow_segs_all) {
      dummyBBox[0] = dummyBBox[1] = dummyBBox[2] = -99999;
      dummyBBox[3] = dummyBBox[4] = dummyBBox[5] = +99999;
      RenderShadowBSPNode(Level->NumNodes-1, dummyBBox, LimitLights);
    }
    Drawer->BeginModelsShadowsPass(CurrLightPos, CurrLightRadius);
    RenderMobjsShadow(ent, dlflags);
    if (r_shadowvol_use_pofs) {
      glDisable(GL_POLYGON_OFFSET_FILL);
      glPolygonOffset(0, 0);
    }
  }
  Drawer->EndLightShadowVolumes();

  // k8: the question is: why we are rendering surfaces instead
  //     of simply render a light circle? shadow volumes should
  //     take care of masking the area, so simply rendering a
  //     circle should do the trick.
  // k8: answering to the silly younger ketmar: because we cannot
  //     read depth info, and we need normals to calculate light
  //     intensity, and so on.

  // draw light
  Drawer->BeginLightPass(CurrLightPos, CurrLightRadius, LightMin, Color, allowShadows);
  LightClip.ClearClipNodes(CurrLightPos, Level, CurrLightRadius);
  dummyBBox[0] = dummyBBox[1] = dummyBBox[2] = -99999;
  dummyBBox[3] = dummyBBox[4] = dummyBBox[5] = +99999;
  RenderLightBSPNode(Level->NumNodes-1, dummyBBox, LimitLights);
  Drawer->BeginModelsLightPass(CurrLightPos, CurrLightRadius, LightMin, Color, coneDir, coneAngle);
  RenderMobjsLight();

  //if (hasScissor) Drawer->DebugRenderScreenRect(scoord[0], scoord[1], scoord[2], scoord[3], 0x7f007f00);

  /*if (hasScissor)*/ Drawer->ResetScissor();
}
