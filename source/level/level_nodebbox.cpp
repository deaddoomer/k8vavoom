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
// REMEMBER! 3d bounding boxes are used to perform frustum culling in renderer.
// without this culling, looking almost vertically up or down will try to render
// the whole 360 degrees (because beam clipper works this way).
// DO NOT REMOVE THIS CODE!
//**************************************************************************
#include "../gamedefs.h"
#include "../render/r_public.h"  /* R_IsAnySkyFlatPlane */


static VCvarB r_bsp_bbox_sky_maxheight("r_bsp_bbox_sky_maxheight", false, "If `true`, use maximum map height for sky bboxes.", CVAR_Archive|CVAR_NoShadow);
static VCvarF r_bsp_bbox_sky_addheight("r_bsp_bbox_sky_addheight", "0.0", "Add this to sky sector height if 'sky maxheigh' is off (DEBUG).", CVAR_Archive|CVAR_NoShadow);

static VCvarI r_dbg_subbbox_use_bsp("r_dbg_subbbox_use_bsp", "2", "Use BSP boinding box for subsectors if it is higher/lower (DEBUG). [1: only for slopes; 2: for all]", CVAR_Archive|CVAR_NoShadow);


static int lastSkyHeightFlag = -1; // unknown yet
static int lastSubBBoxBSP = -1; // unknown yet


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
  minswap2(minz, maxz);

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
      /*
      if (bsec->floor.minz < bsec->ceiling.maxz) {
        zmin = bsec->floor.minz;
        zmax = bsec->ceiling.maxz;
      } else {
        zmin = bsec->ceiling.maxz;
        zmax = bsec->floor.minz;
      }
      */
      if (FASI(bsec->floor.minz) == FASI(bsec->floor.maxz) &&
          FASI(bsec->ceiling.minz) == FASI(bsec->ceiling.maxz))
      {
        // flat sector
        if (bsec->floor.minz < bsec->ceiling.maxz) {
          zmin = bsec->floor.minz;
          zmax = bsec->ceiling.maxz;
        } else {
          zmin = bsec->ceiling.maxz;
          zmax = bsec->floor.minz;
        }
      } else {
        // sloped sector
        zmin = bsec->floor.minz;
        if (zmin > bsec->ceiling.minz) zmin = bsec->ceiling.minz;
        zmax = bsec->ceiling.maxz;
        if (zmax < bsec->floor.maxz) zmax = bsec->floor.maxz;
        minswap2(zmin, zmax); // just in case
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
    float currMinZ = node->bbox[childnum][BOX3D_MINZ];
    float currMaxZ = node->bbox[childnum][BOX3D_MAXZ];
    if (currMinZ <= minz && currMaxZ >= maxz) continue;
    // fix bounding boxes
    currMinZ = min2(currMinZ, minz);
    currMaxZ = max2(currMaxZ, maxz);
    minswap2(currMinZ, currMaxZ); // just in case
    node->bbox[childnum][BOX3D_MINZ] = currMinZ;
    node->bbox[childnum][BOX3D_MAXZ] = currMaxZ;
    // fix parent nodes
    for (; node->parent; node = node->parent) {
      node_t *pnode = node->parent;
           if (pnode->children[0] == node->index) childnum = 0;
      else if (pnode->children[1] == node->index) childnum = 1;
      else Sys_Error("invalid BSP tree");
      const float parCMinZ = pnode->bbox[childnum][BOX3D_MINZ];
      const float parCMaxZ = pnode->bbox[childnum][BOX3D_MAXZ];
      if (parCMinZ <= currMinZ && parCMaxZ >= currMaxZ) continue; // we're done here
      pnode->bbox[childnum][BOX3D_MINZ] = min2(parCMinZ, currMinZ);
      pnode->bbox[childnum][BOX3D_MAXZ] = max2(parCMaxZ, currMaxZ);
      FixBBoxZ(pnode->bbox[childnum]);
      currMinZ = min2(min2(parCMinZ, pnode->bbox[childnum^1][BOX3D_MINZ]), currMinZ);
      currMaxZ = max2(max2(parCMaxZ, pnode->bbox[childnum^1][BOX3D_MAXZ]), currMaxZ);
      minswap2(currMinZ, currMaxZ); // just in case
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
  bbox[BOX3D_MINX] = sub->bbox2d[BOX2D_LEFT];
  bbox[BOX3D_MINY] = sub->bbox2d[BOX2D_BOTTOM];
  // max
  bbox[BOX3D_MAXX] = sub->bbox2d[BOX2D_RIGHT];
  bbox[BOX3D_MAXY] = sub->bbox2d[BOX2D_TOP];

  sector_t *sector = sub->sector;
  if (sector->ZExtentsCacheId != validcountSZCache) UpdateSectorHeightCache(sector);

  // update with BSP bounding box here, it should be more correct, and
  // should fix some  clipper problems with slopes
  // this can cause some overdraw, but it is way better than missing walls, i believe
  const int bxbb = r_dbg_subbbox_use_bsp.asInt();
  if (bxbb <= 0) return;
  if (bxbb > 1 ||
      ((FASI(sector->floor.minz)^FASI(sector->floor.maxz))|
       (FASI(sector->ceiling.minz)^FASI(sector->ceiling.maxz))))
  {
    const node_t *node = sub->parent;
    if (node) {
      const float *nbb = node->bbox[sub->parentChild];
      if (bxbb > 1) {
        bbox[BOX3D_MINZ] = min2(sector->LastMinZ, nbb[BOX3D_MINZ]);
        bbox[BOX3D_MAXZ] = max2(sector->LastMaxZ, nbb[BOX3D_MAXZ]);
      } else {
        bbox[BOX3D_MINZ] = (FASI(sector->floor.minz) != FASI(sector->floor.maxz) ? min2(sector->LastMinZ, nbb[BOX3D_MINZ]) : sector->LastMinZ);
        bbox[BOX3D_MAXZ] = (FASI(sector->ceiling.minz) != FASI(sector->ceiling.maxz) ? max2(sector->LastMaxZ, nbb[BOX3D_MAXZ]) : sector->LastMaxZ);
      }
    }
  } else {
    bbox[BOX3D_MINZ] = sector->LastMinZ;
    bbox[BOX3D_MAXZ] = sector->LastMaxZ;
  }
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
      minswap2(minzf, maxzf); // just in case
      sector->floor.minz = minzf;
      sector->floor.maxz = maxzf;
      if (fixTexZ) sector->floor.TexZ = sector->floor.minz;
    }
    if (slopedFC&SlopedCeiling) {
      minswap2(minzc, maxzc); // just in case
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
  lastSkyHeightFlag = (r_bsp_bbox_sky_maxheight.asBool() ? 1 : 0);
  lastSubBBoxBSP = r_dbg_subbbox_use_bsp.asInt();
  if (NumSectors == 0 || NumSubsectors == 0) return; // just in case
  const float skyheight = CalcSkyHeight();
  for (auto &&node : allNodes()) {
    // special values
    node.bbox[0][BOX3D_MINZ] = -99999.0f;
    node.bbox[0][BOX3D_MAXZ] = +99999.0f;
    node.bbox[1][BOX3D_MINZ] = -99999.0f;
    node.bbox[1][BOX3D_MAXZ] = +99999.0f;
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
  const int nbbh = (r_bsp_bbox_sky_maxheight.asBool() ? 1 : 0);
  const int sbbx = r_dbg_subbbox_use_bsp.asInt();
  if (lastSkyHeightFlag != nbbh || lastSubBBoxBSP != sbbx) {
    double stime = -Sys_Time();
    RecalcWorldBBoxes();
    stime += Sys_Time();
    const int msecs = (int)(stime*1000);
    GCon->Logf("Recalculated world BSP bounding boxes in %d msecs", msecs);
  }
}
