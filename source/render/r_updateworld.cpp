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
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
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
//**  Update world sectors
//**
//**************************************************************************
#include "gamedefs.h"
#include "r_local.h"


VCvarB w_update_clip_bsp("w_update_clip_bsp", true, "Perform BSP clipping on world updates?", CVAR_PreInit/*|CVAR_Archive*/);
VCvarB w_update_clip_region("w_update_clip_region", true, "Perform region clipping on world updates?", CVAR_PreInit/*|CVAR_Archive*/);
VCvarB w_update_in_renderer("w_update_in_renderer", true, "Perform world sector updates in renderer?", CVAR_PreInit/*|CVAR_Archive*/);


//==========================================================================
//
//  VRenderLevelShared::UpdateSubsector
//
//==========================================================================
void VRenderLevelShared::UpdateSubsector (int num, float *bbox) {
  subsector_t *sub = &Level->Subsectors[num];
  if (sub->updateWorldFrame == updateWorldFrame) return;
  sub->updateWorldFrame = updateWorldFrame;

  if (updateWorldCheckVisFrame && Level->HasPVS() && sub->VisFrame != currVisFrame) return;

  if (!sub->sector->linecount) return; // skip sectors containing original polyobjs

  if (w_update_clip_bsp && !ViewClip.ClipCheckSubsector(sub)) return;

  /*
  bbox[2] = sub->sector->floor.minz;
  if (IsSky(&sub->sector->ceiling)) {
    bbox[5] = skyheight;
  } else {
    bbox[5] = sub->sector->ceiling.maxz;
  }
  FixBBoxZ(bbox);
  */

  //FIXME: this will calculate sector height too many times
  Level->CalcSectorBoundingHeight(sub->sector, &bbox[2], &bbox[5]);
  if (IsSky(&sub->sector->ceiling)) bbox[5] = skyheight;
  FixBBoxZ(bbox);

  UpdateSubRegion(sub, sub->regions/*, ClipSegs:true*/);

  ViewClip.ClipAddSubsectorSegs(sub);
}


//==========================================================================
//
//  VRenderLevelShared::UpdateBSPNode
//
//==========================================================================
void VRenderLevelShared::UpdateBSPNode (int bspnum, float *bbox) {
  if (ViewClip.ClipIsFull()) return;

  if (w_update_clip_bsp && !ViewClip.ClipIsBBoxVisible(bbox)) return;

  if (bspnum == -1) {
    UpdateSubsector(0, bbox);
    return;
  }

  // found a subsector?
  if (!(bspnum&NF_SUBSECTOR)) {
    // nope, this is a normal node
    node_t *bsp = &Level->Nodes[bspnum];

    if (updateWorldCheckVisFrame && Level->HasPVS() && bsp->VisFrame != currVisFrame) return;

    // decide which side the view point is on
    unsigned side = bsp->PointOnSide(vieworg);
    UpdateBSPNode(bsp->children[side], bsp->bbox[side]);
    bbox[2] = MIN(bsp->bbox[0][2], bsp->bbox[1][2]);
    bbox[5] = MAX(bsp->bbox[0][5], bsp->bbox[1][5]);
    side ^= 1;
    if (w_update_clip_bsp && !ViewClip.ClipIsBBoxVisible(bsp->bbox[side])) return;
    UpdateBSPNode(bsp->children[side], bsp->bbox[side]);
  } else {
    // leaf node (subsector)
    UpdateSubsector(bspnum&(~NF_SUBSECTOR), bbox);
  }
}


//==========================================================================
//
//  VRenderLevelShared::UpdateWorld
//
//==========================================================================
void VRenderLevelShared::UpdateWorld (const refdef_t *rd, const VViewClipper *Range) {
  float dummy_bbox[6] = { -99999, -99999, -99999, 99999, 99999, 99999 };

  // update fake sectors
  /*
  for (int i = 0; i < Level->NumSectors; ++i) {
    sector_t *sec = &Level->Sectors[i];
         if (sec->deepref) UpdateDeepWater(sec);
    else if (sec->heightsec && !(sec->heightsec->SectorFlags&sector_t::SF_IgnoreHeightSec)) UpdateFakeFlats(sec);
    else if (sec->othersecFloor || sec->othersecCeiling) UpdateFloodBug(sec);
  }
  */
  {
    const vint32 *fksip = Level->FakeFCSectors.ptr();
    for (int i = Level->FakeFCSectors.length(); i--; ++fksip) {
      sector_t *sec = &Level->Sectors[*fksip];
           if (sec->deepref) UpdateDeepWater(sec);
      else if (sec->heightsec && !(sec->heightsec->SectorFlags&sector_t::SF_IgnoreHeightSec)) UpdateFakeFlats(sec);
      else if (sec->othersecFloor || sec->othersecCeiling) UpdateFloodBug(sec);
    }
  }

  if (!w_update_in_renderer) {
    ViewClip.ClearClipNodes(vieworg, Level);
    ViewClip.ClipInitFrustumRange(viewangles, viewforward, viewright, viewup, rd->fovx, rd->fovy);
    if (Range) ViewClip.ClipToRanges(*Range); // range contains a valid range, so we must clip away holes in it

    UpdateBSPNode(Level->NumNodes-1, dummy_bbox); // head node is the last node output
  }
}
