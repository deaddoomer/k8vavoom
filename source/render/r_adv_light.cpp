//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id: r_light.cpp 4220 2010-04-24 15:24:35Z dj_jl $
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "gamedefs.h"
#include "r_local.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

extern VCvarB       r_darken;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static subsector_t *r_sub;
static sec_region_t *r_region;
VCvarF          r_lights_radius("r_lights_radius", "2048", "Maximum light radius.", CVAR_Archive);
VCvarF          r_lights_radius_sight_check("r_lights_radius_sight_check", "1024", "Maximum light radius.", CVAR_Archive);
VCvarI          r_max_model_lights("r_max_model_lights", "32", "Maximum model lights.", CVAR_Archive);
VCvarI          r_max_model_shadows("r_max_model_shadows", "2", "Maximum model shadows.", CVAR_Archive);
VCvarI          r_max_lights("r_max_lights", "64", "Maximum lights.", CVAR_Archive);
VCvarI          r_max_shadows("r_max_shadows", "64", "Maximum shadows.", CVAR_Archive);

VCvarI          r_hashlight_static_div("r_hashlight_static_div", "8", "Divisor for static light spatial hashing.", CVAR_Archive);
VCvarI          r_hashlight_dynamic_div("r_hashlight_dynamic_div", "8", "Divisor for dynamic light spatial hashing.", CVAR_Archive);

// CODE --------------------------------------------------------------------

//==========================================================================
//
//  VAdvancedRenderLevel::PushDlights
//
//==========================================================================

void VAdvancedRenderLevel::PushDlights()
{
  r_dlightframecount = 1;
}

//==========================================================================
//
//  VAdvancedRenderLevel::LightPoint
//
//==========================================================================

vuint32 VAdvancedRenderLevel::LightPoint(const TVec &p)
{
  guard(VAdvancedRenderLevel::LightPoint);
  subsector_t   *sub;
  subregion_t   *reg;
  float     l=0, lr=0, lg=0, lb=0, add;
  int       leafnum;
  linetrace_t   Trace;

  if (FixedLight)
  {
    return FixedLight | (FixedLight << 8) | (FixedLight << 16) | (FixedLight << 24);
  }

  sub = Level->PointInSubsector(p);
  reg = sub->regions;
  if (reg)
  {
    while (reg->next)
    {
      float d = DotProduct(p, reg->floor->secplane->normal) - reg->floor->secplane->dist;

      if (d >= 0.0)
      {
        break;
      }

      reg = reg->next;
    }

    //  Region's base light
    l = reg->secregion->params->lightlevel + ExtraLight;
    if (r_darken)
    {
      l = light_remap[MIN(255, (int)l)];
    }
    l = MIN(255, l);
    int SecLightColour = reg->secregion->params->LightColour;
    lr = ((SecLightColour >> 16) & 255) * l / 255.0;
    lg = ((SecLightColour >> 8) & 255) * l / 255.0;
    lb = (SecLightColour & 255) * l / 255.0;
  }

  //  Add static lights
  if (r_static_lights)
  {
    vuint8 *dyn_facevis = Level->LeafPVS(sub);
    for (int i = 0; i < Lights.Num(); i++)
    {
      if (!Lights[i].radius)
      {
        continue;
      }

      // Check potential visibility
      if (!(dyn_facevis[Lights[i].leafnum >> 3] & (1 << (Lights[i].leafnum & 7))))
      {
        continue;
      }

      add = Lights[i].radius - Length(p - Lights[i].origin);
      if (add > 0)
      {
        l += add;
        lr += add * ((Lights[i].colour >> 16) & 255) / 255.0;
        lg += add * ((Lights[i].colour >> 8) & 255) / 255.0;
        lb += add * (Lights[i].colour & 255) / 255.0;
      }
    }
  }

  //  Add dynamic lights
  if (r_dynamic)
  {
    vuint8 *dyn_facevis = Level->LeafPVS(sub);
    for (int i = 0; i < MAX_DLIGHTS; i++)
    {
      if (!DLights[i].radius || DLights[i].die < Level->Time)
        continue;

      leafnum = Level->PointInSubsector(DLights[i].origin) -
        Level->Subsectors;

      // Check potential visibility
      if (!(dyn_facevis[leafnum >> 3] & (1 << (leafnum & 7))))
      {
        continue;
      }

      add = (DLights[i].radius - DLights[i].minlight) - Length(p - DLights[i].origin);
      if (add > 0)
      {
        l += add;
        lr += add * ((DLights[i].colour >> 16) & 255) / 255.0;
        lg += add * ((DLights[i].colour >> 8) & 255) / 255.0;
        lb += add * (DLights[i].colour & 255) / 255.0;
      }
    }
  }

  if (l > 255)
    l = 255;
  if (lr > 255)
    lr = 255;
  if (lg > 255)
    lg = 255;
  if (lb > 255)
    lb = 255;

  return ((int)l << 24) | ((int)lr << 16) | ((int)lg << 8) | ((int)lb);
  unguard;
}

//==========================================================================
//
//  VAdvancedRenderLevel::LightPointAmbient
//
//==========================================================================

vuint32 VAdvancedRenderLevel::LightPointAmbient(const TVec &p)
{
  guard(VAdvancedRenderLevel::LightPointAmbient);
  subsector_t   *sub;
  subregion_t   *reg;
  float     l, lr, lg, lb;

  if (FixedLight)
  {
    return FixedLight | (FixedLight << 8) | (FixedLight << 16) | (FixedLight << 24);
  }

  sub = Level->PointInSubsector(p);
  reg = sub->regions;
  while (reg->next)
  {
    float d = DotProduct(p, reg->floor->secplane->normal) - reg->floor->secplane->dist;

    if (d >= 0.0)
    {
      break;
    }

    reg = reg->next;
  }

  //  Region's base light
  l = reg->secregion->params->lightlevel + ExtraLight;
  if (r_darken)
  {
    l = light_remap[MIN(255, (int)l)];
  }
  l = MIN(255, l);
  int SecLightColour = reg->secregion->params->LightColour;
  lr = ((SecLightColour >> 16) & 255) * l / 255.0;
  lg = ((SecLightColour >> 8) & 255) * l / 255.0;
  lb = (SecLightColour & 255) * l / 255.0;

  return ((int)l << 24) | ((int)lr << 16) | ((int)lg << 8) | ((int)lb);
  unguard;
}

//==========================================================================
//
//  VAdvancedRenderLevel::BuildLightMap
//
//==========================================================================

bool VAdvancedRenderLevel::BuildLightMap(surface_t *surf, int shift)
{
  return true;
}

//==========================================================================
//
//  VAdvancedRenderLevel::BuildLightVis
//
//==========================================================================

void VAdvancedRenderLevel::BuildLightVis(int bspnum, float *bbox)
{
  guard(VAdvancedRenderLevel::BuildLightVis);
  if (LightClip.ClipIsFull())
  {
    return;
  }

  if (!LightClip.ClipIsBBoxVisible(bbox, true, CurrLightPos, CurrLightRadius))
  {
    return;
  }

  if (bspnum == -1)
  {
    int SubNum = 0;
    subsector_t *Sub = &Level->Subsectors[SubNum];
    if (!Sub->sector->linecount)
    {
      //  Skip sectors containing original polyobjs
      return;
    }

    if (!LightClip.ClipCheckSubsector(Sub, true, CurrLightPos, CurrLightRadius))
    {
      return;
    }

    LightVis[SubNum >> 3] |= 1 << (SubNum & 7);
    LightClip.ClipAddSubsectorSegs(Sub, true, nullptr, CurrLightPos, CurrLightRadius);
    return;
  }

  // Found a subsector?
  if (!(bspnum & NF_SUBSECTOR))
  {
    node_t *bsp = &Level->Nodes[bspnum];

    // Decide which side the view point is on.
    float Dist = DotProduct(CurrLightPos, bsp->normal) - bsp->dist;
    if (Dist > CurrLightRadius)
    {
      //  Light is completely on front side.
      BuildLightVis(bsp->children[0], bsp->bbox[0]);
    }
    else if (Dist < -CurrLightRadius)
    {
      //  Light is completely on back side.
      BuildLightVis(bsp->children[1], bsp->bbox[1]);
    }
    else
    {
      int side = bsp->PointOnSide(CurrLightPos);

      // Recursively divide front space.
      BuildLightVis(bsp->children[side], bsp->bbox[side]);

      // Possibly divide back space.
      if (!LightClip.ClipIsBBoxVisible(bsp->bbox[side ^ 1], true, CurrLightPos, CurrLightRadius))
      {
        return;
      }

      BuildLightVis(bsp->children[side ^ 1], bsp->bbox[side ^ 1]);
    }
    return;
  }

  int SubNum = bspnum & (~NF_SUBSECTOR);
  subsector_t *Sub = &Level->Subsectors[SubNum];
  if (!Sub->sector->linecount)
  {
    //  Skip sectors containing original polyobjs
    return;
  }

  if (!LightClip.ClipCheckSubsector(Sub, true, CurrLightPos, CurrLightRadius))
  {
    return;
  }

  LightVis[SubNum >> 3] |= 1 << (SubNum & 7);
  LightClip.ClipAddSubsectorSegs(Sub, true, nullptr, CurrLightPos, CurrLightRadius);
  unguard;
}

//==========================================================================
//
//  VAdvancedRenderLevel::DrawShadowSurfaces
//
//==========================================================================

void VAdvancedRenderLevel::DrawShadowSurfaces(surface_t *InSurfs, texinfo_t *texinfo,
  bool CheckSkyBoxAlways, bool LightCanCross)
{
  guard(VAdvancedRenderLevel::DrawShadowSurfaces);
  surface_t *surfs = InSurfs;
  if (!surfs)
  {
    return;
  }

  if (texinfo->Tex->Type == TEXTYPE_Null)
  {
    return;
  }

  if (texinfo->Alpha < 1.0)
  {
    return;
  }

  do
  {
    Drawer->RenderSurfaceShadowVolume(surfs, CurrLightPos, CurrLightRadius, LightCanCross);
    surfs = surfs->next;
  } while (surfs);
  unguard;
}

//==========================================================================
//
//  VAdvancedRenderLevel::RenderShadowLine
//
//  Clips the given segment and adds any visible pieces to the line list.
//
//==========================================================================

void VAdvancedRenderLevel::RenderShadowLine(drawseg_t *dseg)
{
  guard(VAdvancedRenderLevel::RenderShadowLine);
  seg_t *line = dseg->seg;

  if (!line->linedef)
  {
    //  Miniseg
    return;
  }

  // Clip sectors that are behind rendered segs
  TVec v1 = *line->v1;
  TVec v2 = *line->v2;
  TVec r1 = CurrLightPos - v1;
  TVec r2 = CurrLightPos - v2;
  float D1 = DotProduct(Normalise(CrossProduct(r1, r2)), CurrLightPos);
  float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), CurrLightPos);

  // There might be a better method of doing this, but
  // this one works for now...
  if (D1 > CurrLightRadius && D2 < -CurrLightRadius)
  {
    v2 += ((v2 - v1) * D1 / (D1 - D2));
  }
  else if (D2 > CurrLightRadius && D1 < -CurrLightRadius)
  {
    v1 += ((v1 - v2) * D2 / (D2 - D1));
  }

  if (!LightClip.IsRangeVisible(LightClip.PointToClipAngle(v2),
    LightClip.PointToClipAngle(v1)))
  {
    return;
  }

    // NOTE: We don't want to filter out shadows that are behind...
  float dist = DotProduct(CurrLightPos, line->normal) - line->dist;
  if (dist < -CurrLightRadius || dist > CurrLightRadius)
  {
    //  Light is too far away
    return;
  }

  //line_t *linedef = line->linedef;
  //side_t *sidedef = line->sidedef;

  if (!line->backsector)
  {
    // single sided line
    DrawShadowSurfaces(dseg->mid->surfs, &dseg->mid->texinfo, false, false);
    DrawShadowSurfaces(dseg->topsky->surfs, &dseg->topsky->texinfo, false, false);
  }
  else
  {
    // two sided line
    DrawShadowSurfaces(dseg->top->surfs, &dseg->top->texinfo, false, false);
    DrawShadowSurfaces(dseg->topsky->surfs, &dseg->topsky->texinfo, false, true);
    DrawShadowSurfaces(dseg->bot->surfs, &dseg->bot->texinfo, false, false);
    DrawShadowSurfaces(dseg->mid->surfs, &dseg->mid->texinfo, false, true);

    for (segpart_t *sp = dseg->extra; sp; sp = sp->next)
    {
      DrawShadowSurfaces(sp->surfs, &sp->texinfo, false, false);
    }
  }
  unguard;
}

//==========================================================================
//
//  VAdvancedRenderLevel::RenderShadowSecSurface
//
//==========================================================================

void VAdvancedRenderLevel::RenderShadowSecSurface(sec_surface_t *ssurf, VEntity *SkyBox)
{
  guard(VAdvancedRenderLevel::RenderShadowSecSurface);
  sec_plane_t &plane = *ssurf->secplane;

  if (!plane.pic)
  {
    return;
  }

  // NOTE: We don't want to filter out shadows that are behind
  float dist = DotProduct(CurrLightPos, plane.normal) - plane.dist;
  if (dist < -CurrLightRadius || dist > CurrLightRadius)
  {
    //  Light is too far away
    return;
  }

  DrawShadowSurfaces(ssurf->surfs, &ssurf->texinfo, true, false);
  unguard;
}

//==========================================================================
//
//  VAdvancedRenderLevel::RenderShadowSubRegion
//
//  Determine floor/ceiling planes.
//  Draw one or more line segments.
//
//==========================================================================

void VAdvancedRenderLevel::RenderShadowSubRegion(subregion_t *region)
{
  guard(VAdvancedRenderLevel::RenderShadowSubRegion);
  int       count;
  int       polyCount;
  seg_t **polySeg;
  float     d;

  d = DotProduct(CurrLightPos, region->floor->secplane->normal) -
    region->floor->secplane->dist;
  if (region->next && d <= -CurrLightRadius)
  {
    if (!LightClip.ClipCheckRegion(region->next, r_sub, true, CurrLightPos, CurrLightRadius))
    {
      return;
    }
    RenderShadowSubRegion(region->next);
  }

  r_region = region->secregion;

  if (r_sub->poly)
  {
    //  Render the polyobj in the subsector first
    polyCount = r_sub->poly->numsegs;
    polySeg = r_sub->poly->segs;
    while (polyCount--)
    {
      RenderShadowLine((*polySeg)->drawsegs);
      polySeg++;
    }
  }

  count = r_sub->numlines;
  drawseg_t *ds = region->lines;
  while (count--)
  {
    RenderShadowLine(ds);
    ds++;
  }

  RenderShadowSecSurface(region->floor, r_region->floor->SkyBox);
  RenderShadowSecSurface(region->ceil, r_region->ceiling->SkyBox);

  if (region->next && d > CurrLightRadius)
  {
    if (!LightClip.ClipCheckRegion(region->next, r_sub, true, CurrLightPos, CurrLightRadius))
    {
      return;
    }
    RenderShadowSubRegion(region->next);
  }
  unguard;
}

//==========================================================================
//
//  VAdvancedRenderLevel::RenderShadowSubsector
//
//==========================================================================

void VAdvancedRenderLevel::RenderShadowSubsector(int num)
{
  guard(VAdvancedRenderLevel::RenderShadowSubsector);
  subsector_t *Sub = &Level->Subsectors[num];
  r_sub = Sub;

  // Don't do this check for shadows
  /*if (!(LightBspVis[num >> 3] & (1 << (num & 7))) ||
    !(BspVis[num >> 3] & (1 << (num & 7))))
  {
    return;
  }*/

  if (!Sub->sector->linecount)
  {
    //  Skip sectors containing original polyobjs
    return;
  }

  if (!LightClip.ClipCheckSubsector(Sub, true, CurrLightPos, CurrLightRadius))
  {
    return;
  }

  RenderShadowSubRegion(Sub->regions);

  //  Add subsector's segs to the clipper. Clipping against mirror
  // is done only for vertical mirror planes.
  LightClip.ClipAddSubsectorSegs(Sub, true, nullptr, CurrLightPos, CurrLightRadius);
  unguard;
}

//==========================================================================
//
//  VAdvancedRenderLevel::RenderShadowBSPNode
//
//  Renders all subsectors below a given node, traversing subtree
// recursively. Just call with BSP root.
//
//==========================================================================

void VAdvancedRenderLevel::RenderShadowBSPNode(int bspnum, float *bbox, bool LimitLights)
{
  guard(VAdvancedRenderLevel::RenderShadowBSPNode);
  if (LimitLights && CurrShadowsNumber > r_max_shadows)
  {
    return;
  }

  if (LightClip.ClipIsFull())
  {
    return;
  }

  if (!LightClip.ClipIsBBoxVisible(bbox, true, CurrLightPos, CurrLightRadius))
  {
    return;
  }

  if (bspnum == -1)
  {
    RenderShadowSubsector(0);
    if (LimitLights)
    {
      CurrShadowsNumber += 1;
    }
    return;
  }

  // Found a subsector?
  if (!(bspnum & NF_SUBSECTOR))
  {
    node_t *bsp = &Level->Nodes[bspnum];

    // Decide which side the light is on.
    float Dist = DotProduct(CurrLightPos, bsp->normal) - bsp->dist;
    if (Dist > CurrLightRadius)
    {
      //  Light is completely on front side.
      RenderShadowBSPNode(bsp->children[0], bsp->bbox[0], LimitLights ? true : false);
    }
    else if (Dist < -CurrLightRadius)
    {
      //  Light is completely on back side.
      RenderShadowBSPNode(bsp->children[1], bsp->bbox[1], LimitLights ? true : false);
    }
    else
    {
      int side = bsp->PointOnSide(CurrLightPos);

      // Recursively divide front space.
      RenderShadowBSPNode(bsp->children[side], bsp->bbox[side], false);

      // Always divide back space for shadows
      RenderShadowBSPNode(bsp->children[side ^ 1], bsp->bbox[side ^ 1], false);
    }

    if (LimitLights)
    {
      CurrShadowsNumber += 1;
    }
    return;
  }

  RenderShadowSubsector(bspnum & (~NF_SUBSECTOR));

  /*if (LimitLights)
  {
    CurrShadowsNumber += 1;
  }*/
  unguard;
}

//==========================================================================
//
//  VAdvancedRenderLevel::DrawLightSurfaces
//
//==========================================================================

void VAdvancedRenderLevel::DrawLightSurfaces(surface_t *InSurfs, texinfo_t *texinfo,
  VEntity *SkyBox, bool CheckSkyBoxAlways, bool LightCanCross)
{
  guard(VAdvancedRenderLevel::DrawLightSurfaces);
  surface_t *surfs = InSurfs;
  if (!surfs)
  {
    return;
  }

  if (texinfo->Tex->Type == TEXTYPE_Null)
  {
    return;
  }

  if (texinfo->Alpha < 1.0)
  {
    return;
  }

  if (SkyBox && (SkyBox->EntityFlags & VEntity::EF_FixedModel))
  {
    SkyBox = nullptr;
  }
  bool IsStack = SkyBox && SkyBox->eventSkyBoxGetAlways();
  if (texinfo->Tex == GTextureManager.getIgnoreAnim(skyflatnum) ||
    (IsStack && CheckSkyBoxAlways))
  {
    return;
  }

  do
  {
    Drawer->DrawSurfaceLight(surfs, CurrLightPos, CurrLightRadius, LightCanCross);
    surfs = surfs->next;
  } while (surfs);
  unguard;
}

//==========================================================================
//
//  VAdvancedRenderLevel::RenderLightLine
//
//  Clips the given segment and adds any visible pieces to the line list.
//
//==========================================================================

void VAdvancedRenderLevel::RenderLightLine(drawseg_t *dseg)
{
  guard(VAdvancedRenderLevel::RenderLightLine);
  seg_t *line = dseg->seg;

  if (!line->linedef)
  {
    //  Miniseg
    return;
  }

  // Clip sectors that are behind rendered segs
  TVec v1 = *line->v1;
  TVec v2 = *line->v2;
  TVec r1 = CurrLightPos - v1;
  TVec r2 = CurrLightPos - v2;
  float D1 = DotProduct(Normalise(CrossProduct(r1, r2)), CurrLightPos);
  float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), CurrLightPos);

  // There might be a better method of doing this, but
  // this one works for now...
  if (D1 > CurrLightRadius && D2 < -CurrLightRadius)
  {
    v2 += ((v2 - v1) * D1 / (D1 - D2));
  }
  else if (D2 > CurrLightRadius && D1 < -CurrLightRadius)
  {
    v1 += ((v1 - v2) * D2 / (D2 - D1));
  }

  if (!LightClip.IsRangeVisible(LightClip.PointToClipAngle(v2),
    LightClip.PointToClipAngle(v1)))
  {
    return;
  }

  float dist = DotProduct(CurrLightPos, line->normal) - line->dist;
  if (dist <= -CurrLightRadius || dist > CurrLightRadius)
  {
    //  Light is in back side or on plane
    return;
  }

  //line_t *linedef = line->linedef;
  //side_t *sidedef = line->sidedef;

  if (!line->backsector)
  {
    // single sided line
    DrawLightSurfaces(dseg->mid->surfs, &dseg->mid->texinfo,
      r_region->ceiling->SkyBox, false, false);
    DrawLightSurfaces(dseg->topsky->surfs, &dseg->topsky->texinfo,
      r_region->ceiling->SkyBox, false, false);
  }
  else
  {
    // two sided line
    DrawLightSurfaces(dseg->top->surfs, &dseg->top->texinfo,
      r_region->ceiling->SkyBox, false, false);
    DrawLightSurfaces(dseg->topsky->surfs, &dseg->topsky->texinfo,
      r_region->ceiling->SkyBox, false, true);
    DrawLightSurfaces(dseg->bot->surfs, &dseg->bot->texinfo,
      r_region->ceiling->SkyBox, false, false);
    DrawLightSurfaces(dseg->mid->surfs, &dseg->mid->texinfo,
      r_region->ceiling->SkyBox, false, true);

    for (segpart_t *sp = dseg->extra; sp; sp = sp->next)
    {
      DrawLightSurfaces(sp->surfs, &sp->texinfo,
        r_region->ceiling->SkyBox, false, false);
    }
  }
  unguard;
}

//==========================================================================
//
//  VAdvancedRenderLevel::RenderLightSecSurface
//
//==========================================================================

void VAdvancedRenderLevel::RenderLightSecSurface(sec_surface_t *ssurf, VEntity *SkyBox)
{
  guard(VAdvancedRenderLevel::RenderLightSecSurface);
  sec_plane_t &plane = *ssurf->secplane;

  if (!plane.pic)
  {
    return;
  }

  float dist = DotProduct(CurrLightPos, plane.normal) - plane.dist;
  if (dist <= -CurrLightRadius || dist > CurrLightRadius)
  {
    //  Light is in back side or on plane
    return;
  }

  DrawLightSurfaces(ssurf->surfs, &ssurf->texinfo, SkyBox, true, false);
  unguard;
}

//==========================================================================
//
//  VAdvancedRenderLevel::RenderLightSubRegion
//
//  Determine floor/ceiling planes.
//  Draw one or more line segments.
//
//==========================================================================

void VAdvancedRenderLevel::RenderLightSubRegion(subregion_t *region)
{
  guard(VAdvancedRenderLevel::RenderLightSubRegion);
  int       count;
  int       polyCount;
  seg_t **polySeg;
  float     d;

  d = DotProduct(CurrLightPos, region->floor->secplane->normal) -
    region->floor->secplane->dist;
  if (region->next && d <= -CurrLightRadius)
  {
    if (!LightClip.ClipCheckRegion(region->next, r_sub, true, CurrLightPos, CurrLightRadius))
    {
      return;
    }
    RenderLightSubRegion(region->next);
  }

  r_region = region->secregion;

  if (r_sub->poly)
  {
    //  Render the polyobj in the subsector first
    polyCount = r_sub->poly->numsegs;
    polySeg = r_sub->poly->segs;
    while (polyCount--)
    {
      RenderLightLine((*polySeg)->drawsegs);
      polySeg++;
    }
  }

  count = r_sub->numlines;
  drawseg_t *ds = region->lines;
  while (count--)
  {
    RenderLightLine(ds);
    ds++;
  }

  RenderLightSecSurface(region->floor, r_region->floor->SkyBox);
  RenderLightSecSurface(region->ceil, r_region->ceiling->SkyBox);

  if (region->next && d > CurrLightRadius)
  {
    if (!LightClip.ClipCheckRegion(region->next, r_sub, true, CurrLightPos, CurrLightRadius))
    {
      return;
    }
    RenderLightSubRegion(region->next);
  }
  unguard;
}

//==========================================================================
//
//  VAdvancedRenderLevel::RenderLightSubsector
//
//==========================================================================

void VAdvancedRenderLevel::RenderLightSubsector(int num)
{
  guard(VAdvancedRenderLevel::RenderLightSubsector);
  subsector_t *Sub = &Level->Subsectors[num];
  r_sub = Sub;

  if (!(LightBspVis[num >> 3] & (1 << (num & 7))) ||
    !(BspVis[num >> 3] & (1 << (num & 7))))
  {
    return;
  }

  if (!Sub->sector->linecount)
  {
    //  Skip sectors containing original polyobjs
    return;
  }

  if (!LightClip.ClipCheckSubsector(Sub, true, CurrLightPos, CurrLightRadius))
  {
    return;
  }

  RenderLightSubRegion(Sub->regions);

  //  Add subsector's segs to the clipper. Clipping against mirror
  // is done only for vertical mirror planes.
  LightClip.ClipAddSubsectorSegs(Sub, true, nullptr, CurrLightPos, CurrLightRadius);
  unguard;
}

//==========================================================================
//
//  VAdvancedRenderLevel::RenderLightBSPNode
//
//  Renders all subsectors below a given node, traversing subtree
// recursively. Just call with BSP root.
//
//==========================================================================

void VAdvancedRenderLevel::RenderLightBSPNode(int bspnum, float *bbox, bool LimitLights)
{
  guard(VAdvancedRenderLevel::RenderLightBSPNode);
  if (LimitLights && CurrLightsNumber > r_max_lights)
  {
    return;
  }

  if (LightClip.ClipIsFull())
  {
    return;
  }

  if (!LightClip.ClipIsBBoxVisible(bbox, true, CurrLightPos, CurrLightRadius))
  {
    return;
  }

  if (bspnum == -1)
  {
    RenderLightSubsector(0);
    if (LimitLights)
    {
      CurrLightsNumber += 1;
    }
    return;
  }

  // Found a subsector?
  if (!(bspnum & NF_SUBSECTOR))
  {
    node_t *bsp = &Level->Nodes[bspnum];

    // Decide which side the light is on.
    float Dist = DotProduct(CurrLightPos, bsp->normal) - bsp->dist;
    if (Dist > CurrLightRadius)
    {
      //  Light is completely on front side.
      RenderLightBSPNode(bsp->children[0], bsp->bbox[0], LimitLights ? true : false);
    }
    else if (Dist < -CurrLightRadius)
    {
      //  Light is completely on back side.
      RenderLightBSPNode(bsp->children[1], bsp->bbox[1], LimitLights ? true : false);
    }
    else
    {
      int side = bsp->PointOnSide(CurrLightPos);

      // Recursively divide front space.
      RenderLightBSPNode(bsp->children[side], bsp->bbox[side], false);

      // Possibly divide back space
      if (!LightClip.ClipIsBBoxVisible(bsp->bbox[side ^ 1], true, CurrLightPos, CurrLightRadius))
      {
        if (LimitLights)
        {
          CurrLightsNumber += 1;
        }
        return;
      }
      RenderLightBSPNode(bsp->children[side ^ 1], bsp->bbox[side ^ 1], false);
    }

    if (LimitLights)
    {
      CurrLightsNumber += 1;
    }
    return;
  }

  RenderLightSubsector(bspnum & (~NF_SUBSECTOR));

  /*if (LimitLights)
  {
    CurrLightsNumber += 1;
  }*/
  unguard;
}

//==========================================================================
//
//  VAdvancedRenderLevel::RenderLightShadows
//
//==========================================================================

void VAdvancedRenderLevel::RenderLightShadows(const refdef_t *RD,
  const VViewClipper *Range, TVec &Pos, float Radius, vuint32 Colour, bool LimitLights)
{
  guard(VAdvancedRenderLevel::RenderLightShadows);
  CurrLightPos = Pos;
  CurrLightRadius = Radius;
  CurrLightColour = Colour;

  float dummy_bbox[6] = { -99999, -99999, -99999, 99999, 99999, 99999 };

  //  Build vis data for light.
  LightClip.ClearClipNodes(CurrLightPos, Level);
  memset(LightVis, 0, VisSize);
  BuildLightVis(Level->NumNodes - 1, dummy_bbox);

  //  Create combined light and view visibility.
  bool HaveIntersect = false;
  for (int i = 0; i < VisSize; i++)
  {
    LightBspVis[i] = BspVis[i] & LightVis[i];
    if (LightBspVis[i])
    {
      HaveIntersect = true;
    }
  }
  if (!HaveIntersect)
  {
    return;
  }

  ResetMobjsLightCount();

  //  Do shadow volumes.
  Drawer->BeginLightShadowVolumes();
  LightClip.ClearClipNodes(CurrLightPos, Level);
  RenderShadowBSPNode(Level->NumNodes - 1, dummy_bbox, LimitLights);
  Drawer->BeginModelsShadowsPass(CurrLightPos, CurrLightRadius);
  RenderMobjsShadow();
  Drawer->EndLightShadowVolumes();

  ResetMobjsLightCount();

  //  Draw light.
  Drawer->BeginLightPass(CurrLightPos, CurrLightRadius, Colour);
  LightClip.ClearClipNodes(CurrLightPos, Level);
  RenderLightBSPNode(Level->NumNodes - 1, dummy_bbox, LimitLights);
  Drawer->BeginModelsLightPass(CurrLightPos, CurrLightRadius, Colour);
  RenderMobjsLight();
  unguard;
}
