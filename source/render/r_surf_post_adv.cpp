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
//**  Copyright (C) 2018-2022 Ketmar Dark
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
#include "r_local.h"


//==========================================================================
//
//  VRenderLevelShadowVolume::InitSurfs
//
//==========================================================================
void VRenderLevelShadowVolume::InitSurfs (bool /*recalcStaticLightmaps*/, surface_t *surfs,
                                          texinfo_t *texinfo, const TPlane *plane,
                                          subsector_t *sub, seg_t *seg, subregion_t *sreg)
{
  if (!texinfo && !plane) return;
  for (; surfs; surfs = surfs->next) {
    if (texinfo) surfs->texinfo = texinfo;
    if (plane) surfs->plane = *plane;
    surfs->SetFixCracks();
    surfs->subsector = sub;
    surfs->seg = seg;
    surfs->sreg = sreg;
  }
}


//==========================================================================
//
//  VRenderLevelShadowVolume::SubdivideFace
//
//  this is used to subdivide flags (floors and ceilings)
//  axes are two lightmap axes
//
//==========================================================================
surface_t *VRenderLevelShadowVolume::SubdivideFace (surface_t *surf, subregion_t *sreg, sec_surface_t *ssf, const TVec &axis, const TVec *nextaxis, const TPlane *plane, bool doSubdivisions) {
  (void)sreg; (void)ssf; (void)axis; (void)nextaxis; (void)plane; (void)doSubdivisions;
  // always create centroid for complex surfaces
  // this is required to avoid omiting some triangles in renderer (which can cause t-junctions)
  vassert(!surf->next);

  subsector_t *sub = surf->subsector;
  vassert(sub);

  if (surf->count < 3 || sub->isOriginalPObj()) return surf; // nobody cares

  if (surf->count > 4 && !surf->isCentroidCreated()) {
    // make room
    surf = EnsureSurfacePoints(surf, surf->count+2, surf, nullptr);
    // insert centroid
    surf->AddCentroid();
  }

  return surf;
}


//==========================================================================
//
//  VRenderLevelShadowVolume::SubdivideSeg
//
//==========================================================================
surface_t *VRenderLevelShadowVolume::SubdivideSeg (surface_t *surf, const TVec &axis, const TVec *nextaxis, seg_t *seg) {
  (void)axis; (void)nextaxis; (void)seg;
  // advanced renderer can draw whole surface
  return surf;
}


//==========================================================================
//
//  VRenderLevelShadowVolume::PreRender
//
//==========================================================================
void VRenderLevelShadowVolume::PreRender () {
  inWorldCreation = true;
  RegisterAllThinkers();
  CreateWorldSurfaces();
  inWorldCreation = false;
}
