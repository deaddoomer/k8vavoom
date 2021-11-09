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
// REMEMBER! 3d bounding boxes are used to perform frustum culling in renderer.
// without this culling, looking almost vertically up or down will try to render
// the whole 360 degrees (because beam clipper works this way).
// DO NOT REMOVE THIS CODE!
//**************************************************************************
#include "../gamedefs.h"
#include "../render/r_public.h"  /* R_IsAnySkyFlatPlane */


static VCvarB r_bsp_bbox_sky_maxheight("r_bsp_bbox_sky_maxheight", false, "If `true`, use maximum map height for sky bboxes.", CVAR_Archive|CVAR_NoShadow);
static VCvarF r_bsp_bbox_sky_addheight("r_bsp_bbox_sky_addheight", "0.0", "Add this to sky sector height if 'sky maxheigh' is off (DEBUG).", CVAR_Archive|CVAR_NoShadow);
static int lastSkyHeightFlag = -1; // unknown yet


//==========================================================================
//
//  VLevel::CalcSkyHeight
//
//==========================================================================
float VLevel::CalcSkyHeight () const {
  if (NumSectors == 0) return 0.0f; // just in case
  // calculate sky height
  float skyheight = -99999.0f;
  for (unsigned i = 0; i < (unsigned)NumSectors; ++i) {
    if (Sectors[i].ceiling.pic == skyflatnum &&
        Sectors[i].ceiling.maxz > skyheight)
    {
      skyheight = Sectors[i].ceiling.maxz;
    }
  }
  if (skyheight < -32768.0f) skyheight = -32768.0f;
  // make it a bit higher to avoid clipping of the sprites
  skyheight += 8*1024;
  return skyheight;
}


//==========================================================================
//
//  VLevel::UpdateSectorHeightCache
//
//  some sectors (like doors) has floor and ceiling on the same level, so
//  we have to look at neighbour sector to get height.
//  note that if neighbour sector is closed door too, we can safely use
//  our zero height, as camera cannot see through top/bottom textures.
//
//==========================================================================
void VLevel::UpdateSectorHeightCache (sector_t *sector) {
  if (!sector || sector->ZExtentsCacheId == validcountSZCache) return;

  sector->ZExtentsCacheId = validcountSZCache;

  float minz = sector->floor.minz;
  float maxz = sector->ceiling.maxz;
  if (minz > maxz) { const float tmp = minz; minz = maxz; maxz = tmp; }

  const sector_t *hs = sector->heightsec;
  if (hs && !(hs->SectorFlags&sector_t::SF_IgnoreHeightSec)) {
    minz = min2(minz, min2(hs->floor.minz, hs->ceiling.maxz));
    maxz = max2(maxz, max2(hs->floor.minz, hs->ceiling.maxz));
  }

  if (!sector->isAnyPObj()) {
    sector_t *const *nbslist = sector->nbsecs;
    for (int nbc = sector->nbseccount; nbc--; ++nbslist) {
      const sector_t *bsec = *nbslist;
      if (bsec->isAnyPObj()) continue; // pobj sectors should not come here, but just in case...
      float zmin, zmax;
      if (bsec->floor.minz < bsec->ceiling.maxz) {
        zmin = bsec->floor.minz;
        zmax = bsec->ceiling.maxz;
      } else {
        zmin = bsec->ceiling.maxz;
        zmax = bsec->floor.minz;
      }
      const sector_t *bhs = bsec->heightsec;
      if (bhs && !(bhs->SectorFlags&sector_t::SF_IgnoreHeightSec)) {
        // rare case, allow more comparisons
        zmin = min2(zmin, min2(bhs->floor.minz, bhs->ceiling.maxz));
        zmax = max2(zmax, max2(bhs->floor.minz, bhs->ceiling.maxz));
      }
      minz = min2(minz, zmin);
      maxz = max2(maxz, zmax);
    }
  } else {
    // pobj sector; it won't be directly rendered anyway
    //minz = maxz = 0.0f;
  }

  vassert(minz <= maxz);
  sector->LastMinZ = minz;
  sector->LastMaxZ = maxz;

  // update BSP
  for (subsector_t *sub = sector->subsectors; sub; sub = sub->seclink) {
    node_t *node = sub->parent;
    //GCon->Logf("  sub %d; pc=%d; nodeparent=%d; next=%d", (int)(ptrdiff_t)(sub-Subsectors), sub->parentChild, (int)(ptrdiff_t)(node-Nodes), (sub->seclink ? (int)(ptrdiff_t)(sub->seclink-Subsectors) : -1));
    if (!node) continue;
    int childnum = sub->parentChild;
    if (node->bbox[childnum][2] <= minz && node->bbox[childnum][5] >= maxz) continue;
    // fix bounding boxes
    float currMinZ = min2(node->bbox[childnum][2], minz);
    float currMaxZ = max2(node->bbox[childnum][5], maxz);
    if (currMinZ > currMaxZ) { float tmp = currMinZ; currMinZ = currMaxZ; currMaxZ = tmp; } // just in case
    node->bbox[childnum][2] = currMinZ;
    node->bbox[childnum][5] = currMaxZ;
    for (; node->parent; node = node->parent) {
      node_t *pnode = node->parent;
           if (pnode->children[0] == node->index) childnum = 0;
      else if (pnode->children[1] == node->index) childnum = 1;
      else Sys_Error("invalid BSP tree");
      const float parCMinZ = pnode->bbox[childnum][2];
      const float parCMaxZ = pnode->bbox[childnum][5];
      if (parCMinZ <= currMinZ && parCMaxZ >= currMaxZ) continue; // we're done here
      pnode->bbox[childnum][2] = min2(parCMinZ, currMinZ);
      pnode->bbox[childnum][5] = max2(parCMaxZ, currMaxZ);
      FixBBoxZ(pnode->bbox[childnum]);
      currMinZ = min2(min2(parCMinZ, pnode->bbox[childnum^1][2]), currMinZ);
      currMaxZ = max2(max2(parCMaxZ, pnode->bbox[childnum^1][5]), currMaxZ);
      if (currMinZ > currMaxZ) { float tmp = currMinZ; currMinZ = currMaxZ; currMaxZ = tmp; } // just in case
    }
  }
}


//==========================================================================
//
//  VLevel::GetSubsectorBBox
//
//==========================================================================
void VLevel::GetSubsectorBBox (subsector_t *sub, float bbox[6]) {
  // min
  bbox[0+0] = sub->bbox2d[BOX2D_LEFT];
  bbox[0+1] = sub->bbox2d[BOX2D_BOTTOM];
  // max
  bbox[3+0] = sub->bbox2d[BOX2D_RIGHT];
  bbox[3+1] = sub->bbox2d[BOX2D_TOP];

  sector_t *sector = sub->sector;
  if (sector->ZExtentsCacheId != validcountSZCache) UpdateSectorHeightCache(sector);
  bbox[0+2] = sector->LastMinZ;
  bbox[3+2] = sector->LastMaxZ;
  //FixBBoxZ(bbox); // no need to do this, minz/maxz *MUST* be correct here
}


//==========================================================================
//
//  VLevel::CalcSecMinMaxs
//
//==========================================================================
void VLevel::CalcSecMinMaxs (sector_t *sector, bool fixTexZ) {
  if (!sector || sector->isAnyPObj()) return; // k8: just in case

  enum {
    SlopedFloor   = 1u<<0,
    SlopedCeiling = 1u<<1,
  };

  unsigned slopedFC = 0;

  if (sector->floor.normal.z == 1.0f) {
    // horizontal floor
    sector->floor.minz = sector->floor.maxz = sector->floor.dist;
    if (fixTexZ) sector->floor.TexZ = sector->floor.minz;
  } else {
    // sloped floor
    slopedFC |= SlopedFloor;
  }

  if (sector->ceiling.normal.z == -1.0f) {
    // horizontal ceiling
    sector->ceiling.minz = sector->ceiling.maxz = -sector->ceiling.dist;
    if (fixTexZ) sector->ceiling.TexZ = sector->ceiling.minz;
  } else {
    // sloped ceiling
    slopedFC |= SlopedCeiling;
  }

  // calculate extents for sloped flats
  if (slopedFC) {
    float minzf = +99999.0f;
    float maxzf = -99999.0f;
    float minzc = +99999.0f;
    float maxzc = -99999.0f;
    line_t **llist = sector->lines;
    for (int cnt = sector->linecount; cnt--; ++llist) {
      line_t *ld = *llist;
      if (slopedFC&SlopedFloor) {
        float z = sector->floor.GetPointZ(*ld->v1);
        minzf = min2(minzf, z);
        maxzf = max2(maxzf, z);
        z = sector->floor.GetPointZ(*ld->v2);
        minzf = min2(minzf, z);
        maxzf = max2(maxzf, z);
      }
      if (slopedFC&SlopedCeiling) {
        float z = sector->ceiling.GetPointZ(*ld->v1);
        minzc = min2(minzc, z);
        maxzc = max2(maxzc, z);
        z = sector->ceiling.GetPointZ(*ld->v2);
        minzc = min2(minzc, z);
        maxzc = max2(maxzc, z);
      }
    }
    if (slopedFC&SlopedFloor) {
      if (minzf > maxzf) { const float tmp = minzf; minzf = maxzf; maxzf = tmp; } // just in case
      sector->floor.minz = minzf;
      sector->floor.maxz = maxzf;
      if (fixTexZ) sector->floor.TexZ = sector->floor.minz;
    }
    if (slopedFC&SlopedCeiling) {
      if (minzc > maxzc) { const float tmp = minzc; minzc = maxzc; maxzc = tmp; } // just in case
      sector->ceiling.minz = minzc;
      sector->ceiling.maxz = maxzc;
      if (fixTexZ) sector->ceiling.TexZ = sector->ceiling.minz;
    }
  }

  sector->ZExtentsCacheId = 0; // force update
  UpdateSectorHeightCache(sector); // this also updates BSP bounding boxes
}


//==========================================================================
//
//  VLevel::UpdateSubsectorBBox
//
//==========================================================================
void VLevel::UpdateSubsectorBBox (int num, float bbox[6], const float skyheight) {
  subsector_t *sub = &Subsectors[num];
  if (sub->isAnyPObj()) return;

  float ssbbox[6];
  GetSubsectorBBox(sub, ssbbox);
  FixBBoxZ(ssbbox);
  ssbbox[BOX3D_MINZ] = min2(ssbbox[BOX3D_MINZ], sub->sector->floor.minz);
  if (R_IsAnySkyFlatPlane(&sub->sector->ceiling)) {
    ssbbox[BOX3D_MAXZ] = max2(ssbbox[BOX3D_MAXZ], (lastSkyHeightFlag ? skyheight : sub->sector->ceiling.maxz+r_bsp_bbox_sky_addheight.asFloat()));
  } else {
    ssbbox[BOX3D_MAXZ] = max2(ssbbox[BOX3D_MAXZ], sub->sector->ceiling.maxz);
  }
  FixBBoxZ(ssbbox);

  memcpy(bbox, ssbbox, sizeof(ssbbox));
  FixBBoxZ(bbox);
}


//==========================================================================
//
//  VLevel::RecalcWorldNodeBBox
//
//==========================================================================
void VLevel::RecalcWorldNodeBBox (int bspnum, float bbox[6], const float skyheight) {
  if (bspnum == -1) {
    UpdateSubsectorBBox(0, bbox, skyheight);
    return;
  }
  // found a subsector?
  if (BSPIDX_IS_NON_LEAF(bspnum)) {
    // nope, this is a normal node
    node_t *bsp = &Nodes[bspnum];
    // decide which side the view point is on
    for (unsigned side = 0; side < 2; ++side) {
      RecalcWorldNodeBBox(bsp->children[side], bsp->bbox[side], skyheight);
      FixBBoxZ(bsp->bbox[side]);
      for (unsigned f = 0; f < 3; ++f) {
        if (bbox[0+f] <= -99990.0f) bbox[0+f] = bsp->bbox[side][0+f];
        if (bbox[3+f] >= +99990.0f) bbox[3+f] = bsp->bbox[side][3+f];
        bbox[0+f] = min2(bbox[0+f], bsp->bbox[side][0+f]);
        bbox[3+f] = max2(bbox[3+f], bsp->bbox[side][3+f]);
      }
      FixBBoxZ(bbox);
    }
    //bbox[2] = min2(bsp->bbox[0][2], bsp->bbox[1][2]);
    //bbox[5] = max2(bsp->bbox[0][5], bsp->bbox[1][5]);
  } else {
    // leaf node (subsector)
    UpdateSubsectorBBox(BSPIDX_LEAF_SUBSECTOR(bspnum), bbox, skyheight);
  }
}


/*
#define ITER_CHECKER(arrname_,itname_,typename_) \
  { \
    int count = 0; \
    for (auto &&it : itname_()) { \
      vassert(it.index() == count); \
      typename_ *tp = it.value(); \
      vassert(tp == &arrname_[count]); \
      ++count; \
    } \
  }
*/

//==========================================================================
//
//  VLevel::RecalcWorldBBoxes
//
//==========================================================================
void VLevel::RecalcWorldBBoxes () {
  /*
  ITER_CHECKER(Vertexes, allVerticesIdx, TVec)
  ITER_CHECKER(Sectors, allSectorsIdx, sector_t)
  ITER_CHECKER(Sides, allSidesIdx, side_t)
  ITER_CHECKER(Lines, allLinesIdx, line_t)
  ITER_CHECKER(Segs, allSegsIdx, seg_t)
  ITER_CHECKER(Subsectors, allSubsectorsIdx, subsector_t)
  ITER_CHECKER(Nodes, allNodesIdx, node_t)
  ITER_CHECKER(Things, allThingsIdx, mthing_t)
  */
  ResetSZValidCount();
  lastSkyHeightFlag = (r_bsp_bbox_sky_maxheight ? 1 : 0);
  if (NumSectors == 0 || NumSubsectors == 0) return; // just in case
  const float skyheight = CalcSkyHeight();
  for (auto &&node : allNodes()) {
    // special values
    node.bbox[0][0+2] = -99999.0f;
    node.bbox[0][3+2] = +99999.0f;
    node.bbox[1][0+2] = -99999.0f;
    node.bbox[1][3+2] = +99999.0f;
  }
  // special values
  if (NumNodes) {
    float dummy_bbox[6] = { -99999.0f, -99999.0f, -99999.0f, 99999.0f, 99999.0f, 99999.0f };
    RecalcWorldNodeBBox(NumNodes-1, dummy_bbox, skyheight);
  }
}


//==========================================================================
//
//  VLevel::CheckAndRecalcWorldBBoxes
//
//  recalcs world bboxes if some cvars changed
//
//==========================================================================
void VLevel::CheckAndRecalcWorldBBoxes () {
  const int nbbh = (r_bsp_bbox_sky_maxheight ? 1 : 0);
  if (lastSkyHeightFlag != nbbh) {
    double stime = -Sys_Time();
    RecalcWorldBBoxes();
    stime += Sys_Time();
    const int msecs = (int)(stime*1000);
    GCon->Logf("Recalculated world BSP bounding boxes in %d msecs", msecs);
  }
}
