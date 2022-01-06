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
//**
//**  visibility by BSP
//**
//**************************************************************************
#include "../gamedefs.h"
#include "r_local.h"


extern VCvarB dbg_vischeck_time;


static TVec cboxPos;
static float cboxRadius;
static float cboxCheckBox[6];


//==========================================================================
//
//  VRenderLevelShared::CheckBSPVisibilityBoxSub
//
//==========================================================================
bool VRenderLevelShared::CheckBSPVisibilityBoxSub (int bspnum) noexcept {
  //if (bspnum == -1) return true;
 tailcall:
  // found a subsector?
  if (BSPIDX_IS_NON_LEAF(bspnum)) {
    // nope
    const node_t *bsp = &Level->Nodes[bspnum];
    // k8: this seems to be marginally slower than simple bbox check
    // k8: checking bbox before recurse into one node speeds it up
    // k8: checking bbox in two-node recursion doesn't do anything sensible (obviously)
    // decide which side the light is on
    const float dist = bsp->PointDistance(cboxPos);
    if (dist >= cboxRadius) {
      // light is completely on front side
      if (!Are3DBBoxesOverlapIn2D(bsp->bbox[0], cboxCheckBox)) return false;
      //return CheckBSPVisibilityBoxSub(bsp->children[0]);
      bspnum = bsp->children[0];
      goto tailcall;
    } else if (dist <= -cboxRadius) {
      // light is completely on back side
      if (!Are3DBBoxesOverlapIn2D(bsp->bbox[1], cboxCheckBox)) return false;
      //return CheckBSPVisibilityBoxSub(bsp->children[1]);
      bspnum = bsp->children[1];
      goto tailcall;
    } else {
      /* the order doesn't matter here, because we aren't doing any culling
      const unsigned side = (unsigned)(dist <= 0.0f);
      // recursively divide front space
      if (CheckBSPVisibilityBoxSub(bsp->children[side])) return true;
      // recursively divide back space
      bspnum = bsp->children[1u^side];
      goto tailcall;
      */
      if (Are3DBBoxesOverlapIn2D(bsp->bbox[0], cboxCheckBox)) {
        if (Are3DBBoxesOverlapIn2D(bsp->bbox[1], cboxCheckBox)) {
          if (CheckBSPVisibilityBoxSub(bsp->children[1])) return true;
        }
        bspnum = bsp->children[0];
        goto tailcall;
      } else if (Are3DBBoxesOverlapIn2D(bsp->bbox[1], cboxCheckBox)) {
        bspnum = bsp->children[1];
        goto tailcall;
      }
    }
  } else {
    // check subsector
    const unsigned subidx = BSPIDX_LEAF_SUBSECTOR(bspnum);
    if (IsBspVis((int)subidx)) return true;
  }
  return false;
}


//==========================================================================
//
//  VRenderLevelShared::CheckBSPVisibilityBox
//
//==========================================================================
bool VRenderLevelShared::CheckBSPVisibilityBox (const TVec &org, float radius, const subsector_t *sub) noexcept {
  if (!Level) return false; // just in case
  if (r_vis_check_flood) return CheckBSPFloodVisibility(org, radius, sub);

  if (sub) {
    const unsigned subidx = (unsigned)(ptrdiff_t)(sub-Level->Subsectors);
    // rendered means "visible"
    if (IsBspVis((int)subidx)) return true;
  }

  if (Level->NumSubsectors < 2) return true; // anyway
  vassert(Level->NumNodes > 0);

  // create light bounding box
  cboxCheckBox[0] = org.x-radius;
  cboxCheckBox[1] = org.y-radius;
  cboxCheckBox[2] = -99999.9f; // doesn't matter
  cboxCheckBox[3] = org.x+radius;
  cboxCheckBox[4] = org.y+radius;
  cboxCheckBox[5] = +99999.9f; // doesn't matter

  cboxPos = org;
  cboxRadius = radius;

  if (!dbg_vischeck_time) {
    return CheckBSPVisibilityBoxSub(Level->NumNodes-1);
  } else {
    const double stt = -Sys_Time_CPU();
    bool res = CheckBSPVisibilityBoxSub(Level->NumNodes-1);
    dbgCheckVisTime += stt+Sys_Time_CPU();
    return res;
  }
}
