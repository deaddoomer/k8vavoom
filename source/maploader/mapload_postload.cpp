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
#include "../gamedefs.h"


VCvarB loader_create_fullsegs("loader_create_fullsegs", false, "Create full line segs for rendering? (Requires map reload on change.)", CVAR_Archive);



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
    int dside = seg->side;
    if (dside != 0 && dside != 1) Sys_Error("invalid seg #%d side (%d)", i, dside);

    if (seg->linedef) {
      line_t *ldef = seg->linedef;

      if (ldef->sidenum[dside] < 0 || ldef->sidenum[dside] >= NumSides) {
        Host_Error("seg #%d, ldef=%d: invalid sidenum %d (%d) (max sidenum is %d)\n", i, (int)(ptrdiff_t)(ldef-Lines), dside, ldef->sidenum[dside], NumSides-1);
      }

      seg->sidedef = &Sides[ldef->sidenum[dside]];
      seg->frontsector = Sides[ldef->sidenum[dside]].Sector;

      if (ldef->flags&ML_TWOSIDED) {
        if (ldef->sidenum[dside^1] < 0 || ldef->sidenum[dside^1] >= NumSides) Host_Error("another side of two-sided linedef is fucked");
        seg->backsector = Sides[ldef->sidenum[dside^1]].Sector;
      } else if (ldef->sidenum[dside^1] >= 0) {
        if (ldef->sidenum[dside^1] >= NumSides) Host_Error("another side of blocking two-sided linedef is fucked");
        seg->backsector = Sides[ldef->sidenum[dside^1]].Sector;
        // not a two-sided, so clear backsector (just in case) -- nope
        //destseg->backsector = nullptr;
      } else {
        seg->backsector = nullptr;
        ldef->flags &= ~ML_TWOSIDED; // just in case
      }
    } else {
      // minisegs should have front and back sectors set too, because why not?
      // but this will be fixed in `PostLoadSubsectors()`
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


//==========================================================================
//
//  VLevel::CreateFullLineSegs
//
//  must be called after saving cached data
//  must have enough extra segs allocated
//
//==========================================================================
void VLevel::CreateFullLineSegs () {
  for (auto &&sd : allSides()) {
    vassert(!sd.fullseg);
  }
  if (!loader_create_fullsegs.asBool()) return;

  for (auto &&ld : allLines()) {
    side_t *fside = (ld.sidenum[0] >= 0 ? &Sides[ld.sidenum[0]] : nullptr);
    side_t *bside = (ld.sidenum[1] >= 0 ? &Sides[ld.sidenum[1]] : nullptr);

    if (!ld.frontsector) fside = nullptr;
    if (!ld.backsector) bside = nullptr;
    if (!fside && !bside) continue;

    // more checks
    if (fside) {
      if (!ld.frontsector || !ld.frontsector->subsectors || ld.frontsector->subsectors->sector != ld.frontsector) {
        GCon->Logf(NAME_Warning, "invalid front side at line #%d", (int)(ptrdiff_t)(&ld-&Lines[0]));
        fside = nullptr;
      }
    }

    if (bside) {
      if (!ld.backsector || !ld.backsector->subsectors || ld.backsector->subsectors->sector != ld.backsector) {
        GCon->Logf(NAME_Warning, "invalid back side at line #%d", (int)(ptrdiff_t)(&ld-&Lines[0]));
        bside = nullptr;
      }
    }

    if (!fside && !bside) continue;

    seg_t *fseg = nullptr;
    seg_t *bseg = nullptr;
    // actually, we don't care about exact segment offset, any segment will do
    for (seg_t *seg = ld.firstseg; seg; seg = seg->lsnext) {
      //GCon->Logf(NAME_Debug, "line #%d, seg: size=%d; offset=%g", (int)(ptrdiff_t)(&ld-&Lines[0]), seg->side, seg->offset);
      if (seg->side == 0) {
        if (!fseg && seg->frontsub && seg->frontsub->sector == ld.frontsector) {
          fseg = seg;
          if (bseg) break;
        }
      } else {
        if (!bseg && seg->frontsub && seg->frontsub->sector == ld.backsector) {
          bseg = seg;
          if (fseg) break;
        }
      }
    }

    const bool nofseg = (fside && !fseg);
    const bool nobseg = (bside && !bseg);
    if (nofseg || nobseg) {
      // it is ok to not have 'em
      if (nofseg && nobseg) {
        GCon->Logf(NAME_Warning, "cannot find starting front and back segs for line #%d", (int)(ptrdiff_t)(&ld-&Lines[0]));
      } else if (nofseg) {
        GCon->Logf(NAME_Warning, "cannot find starting front seg for line #%d", (int)(ptrdiff_t)(&ld-&Lines[0]));
      } else {
        vassert(nobseg);
        GCon->Logf(NAME_Warning, "cannot find starting back seg for line #%d", (int)(ptrdiff_t)(&ld-&Lines[0]));
      }
      continue;
    }

    // find good subsectors for front and back sides
    subsector_t *fsub = nullptr;
    subsector_t *bsub = nullptr;

    if (fside) {
      for (subsector_t *sub = ld.frontsector->subsectors; sub; sub = sub->seclink) {
        if (sub->numlines >= 3) {
          vassert(sub->sector == ld.frontsector);
          fsub = sub;
        }
      }
      if (!fsub) {
        GCon->Logf(NAME_Warning, "cannot find valid front subsector for line #%d", (int)(ptrdiff_t)(&ld-&Lines[0]));
        continue;
      }
    }

    if (bside) {
      for (subsector_t *sub = ld.backsector->subsectors; sub; sub = sub->seclink) {
        if (sub->numlines >= 3) {
          vassert(sub->sector == ld.backsector);
          bsub = sub;
        }
      }
      if (!bsub) {
        GCon->Logf(NAME_Warning, "cannot find valid back subsector for line #%d", (int)(ptrdiff_t)(&ld-&Lines[0]));
        continue;
      }
    }

    seg_t *newfseg = nullptr;
    seg_t *newbseg = nullptr;

    if (fside) {
      vassert(fseg->sidedef == fside);
      vassert(fseg->linedef == &ld);
      vassert(fseg->frontsector == ld.frontsector);
      vassert(fseg->backsector == ld.backsector);
      vassert(fseg->frontsub);
      if (fseg->frontsub->sector != ld.frontsector) {
        GCon->Logf(NAME_Warning, "front seg for line #%d points to subsector #%d, and back to sector #%d instead of sector #%d",
          (int)(ptrdiff_t)(&ld-&Lines[0]),
          (fseg->frontsub ? (int)(ptrdiff_t)(fseg->frontsub-&Subsectors[0]) : -1),
          (fseg->frontsub ? (int)(ptrdiff_t)(fseg->frontsub->sector-&Sectors[0]) : -1),
          (ld.frontsector ? (int)(ptrdiff_t)(ld.frontsector-&Sectors[0]) : -1));
      }
      vassert(fseg->frontsub->sector == ld.frontsector);
      vassert(fseg->side == 0);
      vassert(fsub);

      newfseg = &Segs[NumSegs++];
      fside->fullseg = newfseg;
      *newfseg = *fseg;
      newfseg->v1 = ld.v1;
      newfseg->v2 = ld.v2;
      newfseg->drawsegs = nullptr;
      newfseg->partner = nullptr;
      newfseg->frontsub = fsub;
      vassert(!(newfseg->flags&SF_FULLSEG));
      newfseg->flags |= SF_FULLSEG;
      // link to list
      newfseg->lsnext = ld.firstseg;
      ld.firstseg = newfseg;

      CalcSegLenOfs(newfseg);
      CalcSegPlaneDir(newfseg); // this will check for zero-length segs

      fside->fullseg = newfseg;
    }

    if (bside) {
      vassert(bseg->sidedef == bside);
      vassert(bseg->linedef == &ld);
      vassert(bseg->frontsector == ld.backsector);
      vassert(bseg->backsector == ld.frontsector);
      vassert(bseg->frontsub);
      if (bseg->frontsub->sector != ld.backsector) {
        GCon->Logf(NAME_Warning, "back seg for line #%d points to subsector #%d, and back to sector #%d instead of sector #%d",
          (int)(ptrdiff_t)(&ld-&Lines[0]),
          (bseg->frontsub ? (int)(ptrdiff_t)(bseg->frontsub-&Subsectors[0]) : -1),
          (bseg->frontsub ? (int)(ptrdiff_t)(bseg->frontsub->sector-&Sectors[0]) : -1),
          (ld.backsector ? (int)(ptrdiff_t)(ld.backsector-&Sectors[0]) : -1));
      }
      vassert(bseg->frontsub->sector == ld.backsector);
      vassert(bseg->side == 1);
      vassert(bsub);

      newbseg = &Segs[NumSegs++];
      bside->fullseg = newbseg;
      *newbseg = *bseg;
      newbseg->v1 = ld.v2;
      newbseg->v2 = ld.v1;
      newbseg->drawsegs = nullptr;
      newbseg->partner = nullptr;
      newbseg->frontsub = bsub;
      vassert(!(newbseg->flags&SF_FULLSEG));
      newbseg->flags |= SF_FULLSEG;
      // link to list
      newbseg->lsnext = ld.firstseg;
      ld.firstseg = newbseg;

      CalcSegLenOfs(newbseg);
      CalcSegPlaneDir(newbseg); // this will check for zero-length segs

      bside->fullseg = newbseg;
    }

    // fix partner segs
    if (newfseg) newfseg->partner = newbseg;
    if (newbseg) newbseg->partner = newfseg;
    LevelFlags |= LF_HasFullSegs;
  }
}
