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


//==========================================================================
//
//  VLevel::LinkNode
//
//==========================================================================
void VLevel::LinkNode (int BSPNum, node_t *pParent) {
  if (BSPNum&NF_SUBSECTOR) {
    int num = (BSPNum == -1 ? 0 : BSPNum&(~NF_SUBSECTOR));
    if (num < 0 || num >= NumSubsectors) Host_Error("ss %i with numss = %i", num, NumSubsectors);
    Subsectors[num].parent = pParent;
    if (pParent) {
      Subsectors[num].parentChild = ((int)pParent->children[0] == BSPNum ? 0 : 1);
    } else {
      Subsectors[num].parentChild = -1;
    }
  } else {
    if (BSPNum < 0 || BSPNum >= NumNodes) Host_Error("bsp %i with numnodes = %i", NumNodes, NumNodes);
    node_t *bsp = &Nodes[BSPNum];
    bsp->parent = pParent;
    LinkNode(bsp->children[0], bsp);
    LinkNode(bsp->children[1], bsp);
  }
}


//==========================================================================
//
//  VLevel::PostLoadNodes
//
//==========================================================================
void VLevel::PostLoadNodes () {
  if (NumNodes > 0) LinkNode(NumNodes-1, nullptr);
}


//==========================================================================
//
//  VLevel::PostLoadSegs
//
//  vertices, side, linedef and partner should be set
//  MUST be called after `FinaliseLines()`
//
//==========================================================================
void VLevel::PostLoadSegs () {
  for (auto &&it : allSegsIdx()) {
    int i = it.index();
    seg_t *seg = it.value();

    if (seg->linedef) {
      const int dside = seg->side;
      if (dside != 0 && dside != 1) Sys_Error("invalid seg #%d side (%d)", i, dside);
      const int oside = dside^1;

      line_t *ldef = seg->linedef;

      if (ldef->sidenum[dside] < 0 || ldef->sidenum[dside] >= NumSides) {
        //Host_Error("seg #%d, ldef=%d: invalid sidenum %d (%d) (max sidenum is %d)", i, (int)(ptrdiff_t)(ldef-Lines), dside, ldef->sidenum[dside], NumSides-1);
        Host_Error("%s seg #%d of linedef #%d references to non-existing sidedef #%d (max sidenum is %d)", (dside ? "back" : "front"), i, (int)(ptrdiff_t)(ldef-Lines), ldef->sidenum[oside], NumSides-1);
      }

      seg->sidedef = &Sides[ldef->sidenum[dside]];
      seg->frontsector = Sides[ldef->sidenum[dside]].Sector;

      if (ldef->flags&ML_TWOSIDED) {
        // two-sided line
        if (ldef->sidenum[oside] < 0 || ldef->sidenum[oside] >= NumSides) {
          Host_Error("%s seg #%d of two-sided linedef #%d references to non-existing sidedef #%d (max sidenum is %d)", (oside ? "back" : "front"), i, (int)(ptrdiff_t)(ldef-Lines), ldef->sidenum[oside], NumSides-1);
        }
        seg->backsector = Sides[ldef->sidenum[oside]].Sector;
      } else {
        // one-sided line
        //NO, not this:if (dside == 1) Host_Error("one-sided linedef #%d has back side; looks like internal error in map loader!", (int)(ptrdiff_t)(ldef-Lines));
        // clear backsector
        seg->backsector = nullptr;
        // break partnership, if there is any
        if (seg->partner) {
          seg->partner->partner = nullptr;
          seg->partner->backsector = nullptr;
          seg->partner = nullptr;
        }
      }
    } else {
      // minisegs should have front and back sectors set too, because why not?
      // but this will be fixed in `PostLoadSubsectors()`
      if (seg->side != 0 && seg->side != 1) {
        GCon->Logf(NAME_Warning, "MapLoader: miniseg #%d has invalid side (%d); fixed.", i, seg->side);
        seg->side = 0;
      }
    }

    CalcSegLenOfs(seg);
    CalcSegPlaneDir(seg); // this will check for zero-length segs
  }
}


//==========================================================================
//
//  VLevel::PostLoadSubsectors
//
//==========================================================================
void VLevel::PostLoadSubsectors () {
  // this can be called with already linked sector<->subsectors, so don't assume anything
  for (auto &&sec : allSectors()) sec.subsectors = nullptr;
  for (auto &&sub : allSubsectors()) sub.seclink = nullptr;

  for (auto &&it : allSubsectorsIdx()) {
    const int idx = it.index();
    subsector_t *ss = it.value();
    if (ss->firstline < 0 || ss->firstline >= NumSegs) Host_Error("Bad seg index %d", ss->firstline);
    if (ss->numlines <= 0 || ss->firstline+ss->numlines > NumSegs) Host_Error("Bad segs range %d %d", ss->firstline, ss->numlines);

    // look up sector number for each subsector
    // (and link this subsector to its parent sector)
    ss->sector = nullptr;
    for (auto &&seg : allSubSegs(ss)) {
      if (seg.linedef) {
        vassert(seg.sidedef);
        vassert(seg.sidedef->Sector);
        ss->sector = seg.sidedef->Sector;
        ss->seclink = ss->sector->subsectors;
        ss->sector->subsectors = ss;
        break;
      }
    }
    if (!ss->sector) Host_Error("Subsector %d contains only minisegs; this is nodes builder bug!", idx);

    // calculate bounding box
    // also, setup frontsector and backsector for segs
    ss->bbox2d[BOX2D_LEFT] = ss->bbox2d[BOX2D_BOTTOM] = 999999.0f;
    ss->bbox2d[BOX2D_RIGHT] = ss->bbox2d[BOX2D_TOP] = -999999.0f;
    for (auto &&seg : allSubSegs(ss)) {
      seg.frontsub = ss;
      // for minisegs, set front and back sectors to subsector owning sector
      if (!seg.linedef) seg.frontsector = seg.backsector = ss->sector;
      // min
      ss->bbox2d[BOX2D_LEFT] = min2(ss->bbox2d[BOX2D_LEFT], min2(seg.v1->x, seg.v2->x));
      ss->bbox2d[BOX2D_BOTTOM] = min2(ss->bbox2d[BOX2D_BOTTOM], min2(seg.v1->y, seg.v2->y));
      // max
      ss->bbox2d[BOX2D_RIGHT] = max2(ss->bbox2d[BOX2D_RIGHT], max2(seg.v1->x, seg.v2->x));
      ss->bbox2d[BOX2D_TOP] = max2(ss->bbox2d[BOX2D_TOP], max2(seg.v1->y, seg.v2->y));
    }
  }

  for (auto &&it : allSegsIdx()) {
    if (it.value()->frontsub == nullptr) Host_Error("Seg %d: frontsub is not set!", it.index());
    if (it.value()->frontsector == nullptr) {
      if (it.value()->backsector == nullptr) {
        Host_Error("Seg %d: front sector is not set!", it.index());
      } else {
        GCon->Logf(NAME_Warning, "Seg %d: only back sector is set (this may be a bug, but i am not sure)", it.index());
      }
    }
  }
}
