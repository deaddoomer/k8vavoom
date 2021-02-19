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

// polyobj line start special
#define PO_LINE_START     (1)
#define PO_LINE_EXPLICIT  (5)


//==========================================================================
//
//  polyobj_t::IsLinkedToSubsector
//
//==========================================================================
bool polyobj_t::IsLinkedToSubsector (subsector_t *asub) {
  if (!asub) return false;
  for (const pobjsubnode_t *node = asub->polysubnode; node; node = node->snodenext) {
    if (node->pobj == this) return true;
  }
  return false;
}


//==========================================================================
//
//  polyobj_t::LinkToSubsector
//
//==========================================================================
void polyobj_t::LinkToSubsector (subsector_t *asub) {
  // check if this polyobj already linked to the given subsector
  vassert(asub);
  for (const pobjsubnode_t *node = asub->polysubnode; node; node = node->snodenext) {
    if (node->pobj == this) return; // nothing to do
  }
  // ok, not linked, link it
  pobjsubnode_t *node = (pobjsubnode_t *)Z_Calloc(sizeof(pobjsubnode_t)); //FIXME: use pool allocator
  node->sub = asub;
  node->pobj = this;
  // link to subsector
  if (asub->polysubnode) asub->polysubnode->snodeprev = node;
  node->snodenext = asub->polysubnode;
  asub->polysubnode = node;
  // link pobj
  if (polynode) polynode->pnodeprev = node;
  node->pnodenext = polynode;
  polynode = node;
  // remember original sector (we'll need it to calculate pobj height)
  if (!originalSector) originalSector = asub->sector;
}


//==========================================================================
//
//  polyobj_t::UnlinkFromSubsector
//
//==========================================================================
void polyobj_t::UnlinkFromSubsector (subsector_t *asub) {
  if (!asub) return;
  for (pobjsubnode_t *node = asub->polysubnode; node; node = node->snodenext) {
    if (node->pobj == this) {
      // found it, unlink from subsector node list
      if (node->snodeprev) node->snodeprev->snodenext = node->snodenext; else node->sub->polysubnode = node->snodenext;
      if (node->snodenext) node->snodenext->snodeprev = node->snodeprev;
      // unlink from this pobj node list
      if (node->pnodeprev) node->pnodeprev->pnodenext = node->pnodenext; else polynode = node->pnodenext;
      if (node->pnodenext) node->pnodenext->pnodeprev = node->pnodeprev;
      Z_Free(node); //FIXME: use pool allocator
      return;
    }
  }
}


//==========================================================================
//
//  polyobj_t::UnlinkFromAllSubsectors
//
//==========================================================================
void polyobj_t::UnlinkFromAllSubsectors () {
  while (polynode) UnlinkFromSubsector(polynode->sub);
}


//==========================================================================
//
//  VLevel::IterFindPolySegs
//
//  Passing nullptr for segList will cause IterFindPolySegs to count the number
//  of segs in the polyobj
//
//==========================================================================
void VLevel::IterFindPolySegs (const TVec &From, seg_t **segList,
                               int &PolySegCount, const TVec &PolyStart)
{
  if (From == PolyStart) return; // reached starting vertex
  if (NumSegs == 0) Host_Error("no segs (wtf?!)");
/*
  GCon->Logf("IterFindPolySegs: from=(%f,%f,%f); count=%d", From.x, From.y, From.z, PolySegCount);
  for (int i = 0; i < NumSegs; ++i) {
    if (!Segs[i].linedef) continue; // skip minisegs
    if (*Segs[i].v1 == From) {
      if (!segList) {
        // count segs
        ++PolySegCount;
      } else {
        // add to the list
        *segList++ = &Segs[i];
        // set sector's line count to 0 to force it not to be
        // rendered even if we do a no-clip into it
        // -- FB -- I'm disabling this behavior
        // k8: and i am enabling it again
        Segs[i].frontsector->linecount = 0;
      }
      GCon->Logf("IterFindPolySegs: from=(%f,%f,%f); count=%d; recurse with %d", From.x, From.y, From.z, PolySegCount, i);
      return IterFindPolySegs(*Segs[i].v2, segList, PolySegCount, PolyStart);
    }
  }
*/
#if 0
  TArray<vuint8> touched;
  touched.setLength(NumSegs);
  memset(touched.ptr(), 0, touched.length());
  TArray<TVec> vlist;
  int vlpos = 0;
  vlist.append(From);
  while (vlpos < vlist.length()) {
    TVec v0 = vlist[vlpos++];
    bool found = false;
    seg_t *seg = Segs;
    vuint8 *tptr = touched.ptr();
    for (int i = NumSegs; i--; ++seg, ++tptr) {
      if (!seg->linedef) continue; // skip minisegs
      if (*tptr) continue;
      if (*seg->v1 == v0) {
        found = true;
        *tptr = 1;
        if (!segList) {
          // count segs
          ++PolySegCount;
        } else {
          // add to the list
          *segList++ = seg;
          // set sector's line count to 0 to force it not to be
          // rendered even if we do a no-clip into it
          // -- FB -- I'm disabling this behavior
          // k8: and i am enabling it again
          seg->frontsector->linecount = 0;
        }
        if (*seg->v2 != PolyStart) {
          bool gotV2 = false;
          for (int f = 0; f < vlist.length(); ++f) if (vlist[f] == *seg->v2) { gotV2 = true; break; }
          if (!gotV2) vlist.append(*seg->v2);
        }
      }
    }
    if (!found) Host_Error("Non-closed Polyobj located.");
  }
#else
  struct SegV1Info {
    seg_t *seg;
    SegV1Info *next;
  };

  // first, build map with all segs, keyed by v1
  TMapNC<TVec, SegV1Info *> smap;
  TArray<SegV1Info> sinfo;
  sinfo.setLength(NumSegs);
  int sinfoCount = 0;
  {
    seg_t *seg = Segs;
    for (int i = NumSegs; i--; ++seg) {
      if (!seg->frontsector) continue;
      if (!seg->linedef) continue; // skip minisegs
      SegV1Info *si = &sinfo[sinfoCount++];
      si->seg = seg;
      si->next = nullptr;
      auto mp = smap.find(*seg->v1);
      if (mp) {
        SegV1Info *l = *mp;
        while (l->next) l = l->next;
        l->next = si;
      } else {
        smap.put(*seg->v1, si);
      }
    }
  }
  // now collect segs
  TMapNC<TVec, bool> vseen;
  vseen.put(PolyStart, true);
  TArray<TVec> vlist;
  int vlpos = 0;
  vlist.append(From);
  while (vlpos < vlist.length()) {
    TVec v0 = vlist[vlpos++];
    auto mp = smap.find(v0);
    if (!mp) {
      //Host_Error("Non-closed Polyobj located (vlpos=%d; vlcount=%d).", vlpos, vlist.length());
      continue;
    }
    SegV1Info *si = *mp;
    smap.del(v0);
    for (; si; si = si->next) {
      seg_t *seg = si->seg;
      if (!segList) {
        // count segs
        ++PolySegCount;
      } else {
        // add to the list
        *segList++ = seg;
        // set sector's line count to 0 to force it not to be
        // rendered even if we do a no-clip into it
        // -- FB -- I'm disabling this behavior
        // k8: and i am enabling it again
        seg->frontsector->linecount = 0;
      }
      v0 = *seg->v2;
      if (!vseen.put(v0, true)) {
        // new vertex
        vlist.append(v0);
      }
    }
  }
#endif
}


extern "C" {
  static int cmpPobjSegs (const void *aa, const void *bb, void *linedefBase) {
    if (aa == bb) return 0;
    const seg_t *sega = *(const seg_t **)aa;
    const seg_t *segb = *(const seg_t **)bb;
    const int seqA = sega->linedef->arg2;
    const int seqB = segb->linedef->arg2;
    if (seqA < seqB) return -1;
    if (seqA > seqB) return 1;
    // sort by linedef order
    if (sega->linedef == segb->linedef) return 0;
    const ptrdiff_t lnumA = (ptrdiff_t)(sega->linedef-((const line_t *)linedefBase));
    const ptrdiff_t lnumB = (ptrdiff_t)(segb->linedef-((const line_t *)linedefBase));
    if (lnumA < lnumB) return -1;
    if (lnumA > lnumB) return 1;
    return 0;
  }
}


//==========================================================================
//
//  VLevel::SpawnPolyobj
//
//==========================================================================
void VLevel::SpawnPolyobj (float x, float y, int tag, bool crush, bool hurt) {
  int index = NumPolyObjs++;

  polyobj_t *po = new polyobj_t;
  memset((void *)po, 0, sizeof(polyobj_t));
  po->index = index;
  po->tag = tag;

  if (crush) po->PolyFlags |= polyobj_t::PF_Crush; else po->PolyFlags &= ~polyobj_t::PF_Crush;
  if (hurt) po->PolyFlags |= polyobj_t::PF_HurtOnTouch; else po->PolyFlags &= ~polyobj_t::PF_HurtOnTouch;

  // realloc polyobj array
  //FIXME: do this better; but don't use `TArray`, because `ClearReferences()` will walk it
  {
    polyobj_t **Temp = PolyObjs;
    PolyObjs = new polyobj_t*[NumPolyObjs];
    if (Temp) {
      for (int i = 0; i < NumPolyObjs-1; ++i) PolyObjs[i] = Temp[i];
      delete[] Temp;
      Temp = nullptr;
    }
    //memset((void *)(&PolyObjs[index]), 0, sizeof(polyobj_t));
    PolyObjs[index] = po;
  }

  po->startSpot.x = x;
  po->startSpot.y = y;

  TArray<seg_t *> psegs; // collect explicit pobj segs here
  for (auto &&seg : allSegs()) {
    if (!seg.linedef) continue;
    if (seg.linedef->special == PO_LINE_START && seg.linedef->arg1 == tag) {
      psegs.clear(); // we don't need 'em anymore
      seg.linedef->special = 0;
      seg.linedef->arg1 = 0;
      int PolySegCount = 1;
      TVec PolyStart = *seg.v1;
      IterFindPolySegs(*seg.v2, nullptr, PolySegCount, PolyStart);

      po->numsegs = PolySegCount;
      po->segs = new seg_t*[PolySegCount];
      *(po->segs) = &seg; // insert the first seg
      // set sector's line count to 0 to force it not to be
      // rendered even if we do a no-clip into it
      // -- FB -- I'm disabling this behavior
      // k8: and i am enabling it again
      seg.frontsector->linecount = 0;
      IterFindPolySegs(*seg.v2, po->segs+1, PolySegCount, PolyStart);
      po->seqType = seg.linedef->arg3;
      //if (po->seqType < 0 || po->seqType >= SEQTYPE_NUMSEQ) po->seqType = 0;
      break;
    } else if (seg.linedef->special == PO_LINE_EXPLICIT && seg.linedef->arg1 == tag) {
      // collect explicit segs
      psegs.append(&seg);
      // set sector's line count to 0 to force it not to be
      // rendered even if we do a no-clip into it
      // -- FB -- I'm disabling this behavior
      // k8: and i am enabling it again
      if (seg.frontsector) seg.frontsector->linecount = 0;
    }
  }

  if (!po->segs) {
    // didn't found a polyobj through PO_LINE_START, build from explicit lines
    if (psegs.length()) {
      // sort segs (why not?)
      timsort_r(psegs.ptr(), psegs.length(), sizeof(seg_t *), &cmpPobjSegs, (void *)&Lines[0]);
      po->numsegs = psegs.length();
      po->segs = new seg_t*[po->numsegs];
      bool seqSet = false;
      for (int i = 0; i < po->numsegs; ++i) {
        po->segs[i] = psegs[i];
        if (!seqSet && psegs[i]->linedef->arg4) {
          seqSet = true;
          po->seqType = psegs[i]->linedef->arg4;
        }
      }
      //po->seqType = (*po->segs)->linedef->arg4;
      // next, change the polyobjs first line to point to a mirror if it exists
      po->segs[0]->linedef->arg2 = po->segs[0]->linedef->arg3;
    }
  }

  // set seg pobj owner
  for (int c = 0; c < po->numsegs; ++c) po->segs[c]->pobj = po;
}


//==========================================================================
//
//  VLevel::GetPolyobj
//
//==========================================================================
polyobj_t *VLevel::GetPolyobj (int polyNum) noexcept {
  //FIXME: make this faster!
  for (int i = 0; i < NumPolyObjs; ++i) {
    if (PolyObjs[i]->tag == polyNum) return PolyObjs[i];
  }
  return nullptr;
}


//==========================================================================
//
//  VLevel::AddPolyAnchorPoint
//
//==========================================================================
void VLevel::AddPolyAnchorPoint (float x, float y, int tag) {
  ++NumPolyAnchorPoints;
  PolyAnchorPoint_t *Temp = PolyAnchorPoints;
  PolyAnchorPoints = new PolyAnchorPoint_t[NumPolyAnchorPoints];
  if (Temp) {
    for (int i = 0; i < NumPolyAnchorPoints-1; ++i) PolyAnchorPoints[i] = Temp[i];
    delete[] Temp;
    Temp = nullptr;
  }

  PolyAnchorPoint_t &A = PolyAnchorPoints[NumPolyAnchorPoints-1];
  A.x = x;
  A.y = y;
  A.tag = tag;
}


//==========================================================================
//
//  VLevel::InitPolyobjs
//
//==========================================================================
void VLevel::InitPolyobjs () {
  for (int i = 0; i < NumPolyAnchorPoints; ++i) {
    TranslatePolyobjToStartSpot(PolyAnchorPoints[i].x, PolyAnchorPoints[i].y, PolyAnchorPoints[i].tag);
  }

  // check for a startspot without an anchor point
  for (int i = 0; i < NumPolyObjs; ++i) {
    if (!PolyObjs[i]->originalPts) {
      Sys_Error("StartSpot located without an Anchor point: %d", PolyObjs[i]->tag);
    }
  }

  InitPolyBlockMap();
}


//==========================================================================
//
//  VLevel::TranslatePolyobjToStartSpot
//
//==========================================================================
void VLevel::TranslatePolyobjToStartSpot (float originX, float originY, int tag) {
  polyobj_t *po = GetPolyobj(tag);
  if (!po) Host_Error("Unable to match polyobj tag: %d", tag); // didn't match the tag with a polyobj tag
  if (po->segs == nullptr) Host_Error("Anchor point located without a StartSpot point: %d", tag);
  vassert(po->numsegs);
  po->originalPts = new TVec[po->numsegs];
  po->prevPts = new TVec[po->numsegs];
  float deltaX = originX-po->startSpot.x;
  float deltaY = originY-po->startSpot.y;

  seg_t **tempSeg = po->segs;
  TVec *tempPt = po->originalPts;
  TVec avg(0, 0, 0); // used to find a polyobj's center, and hence the subsector

  for (int i = 0; i < po->numsegs; ++i, ++tempSeg, ++tempPt) {
    seg_t **veryTempSeg = po->segs;
    for (; veryTempSeg != tempSeg; ++veryTempSeg) {
      if ((*veryTempSeg)->v1 == (*tempSeg)->v1) break;
    }
    if (veryTempSeg == tempSeg) {
      // the point hasn't been translated yet
      (*tempSeg)->v1->x -= deltaX;
      (*tempSeg)->v1->y -= deltaY;
    }
    avg.x += (*tempSeg)->v1->x;
    avg.y += (*tempSeg)->v1->y;
    // the original Pts are based off the startSpot Pt, and are unique to each seg, not each linedef
    *tempPt = *(*tempSeg)->v1-po->startSpot;
  }
  avg.x /= po->numsegs;
  avg.y /= po->numsegs;

  subsector_t *sub = PointInSubsector(avg); // bugfixed algo
  po->UnlinkFromSubsector(sub); // just in case
  po->LinkToSubsector(sub);
  UpdatePolySegs(po);
}


//==========================================================================
//
//  VLevel::UpdatePolySegs
//
//==========================================================================
void VLevel::UpdatePolySegs (polyobj_t *po) {
  IncrementValidCount();
  seg_t **segList = po->segs;
  for (int count = po->numsegs; count; --count, ++segList) {
    if ((*segList)->linedef->validcount != validcount) {
      // recalc lines's slope type, bounding box, normal and dist
      CalcLine((*segList)->linedef);
      (*segList)->linedef->validcount = validcount;
    }
    // recalc seg's normal and dist
    CalcSeg(*segList);
    if (Renderer) Renderer->SegMoved(*segList);
  }
}


//==========================================================================
//
//  VLevel::InitPolyBlockMap
//
//==========================================================================
void VLevel::InitPolyBlockMap () {
  PolyBlockMap = new polyblock_t*[BlockMapWidth*BlockMapHeight];
  memset((void *)PolyBlockMap, 0, sizeof(polyblock_t *)*BlockMapWidth*BlockMapHeight);
  for (int i = 0; i < NumPolyObjs; ++i) LinkPolyobj(PolyObjs[i]);
}


//==========================================================================
//
//  VLevel::LinkPolyobj
//
//==========================================================================
void VLevel::LinkPolyobj (polyobj_t *po) {
  // calculate the polyobj bbox
  seg_t **tempSeg = po->segs;
  float rightX = (*tempSeg)->v1->x;
  float leftX = (*tempSeg)->v1->x;
  float topY = (*tempSeg)->v1->y;
  float bottomY = (*tempSeg)->v1->y;

  /* k8 notes
     relink polyobject to the new subsector.
     this somewhat eases rendering glitches until i'll write the proper BSP splitter for pobjs.
     it is not the best way, but at least something.

     note that if it moves to the sector with a different height, the renderer
     adjusts it to the height of the new sector.

     to avoid this, i had to introduce new field to segs, which links back to the
     pobj. and i had to store the initial floor and ceiling planes in the pobj, so
     renderer can use them instead of the corresponding sector planes.

     actually, not the original. on relinking, we have to scan all touching sectors,
     and get min floor and max ceiling.
  */
  TVec avg(0, 0, 0); // used to find a polyobj's center, and hence subsector
  for (int i = 0; i < po->numsegs; ++i, ++tempSeg) {
    if ((*tempSeg)->v1->x > rightX) rightX = (*tempSeg)->v1->x;
    if ((*tempSeg)->v1->x < leftX) leftX = (*tempSeg)->v1->x;
    if ((*tempSeg)->v1->y > topY) topY = (*tempSeg)->v1->y;
    if ((*tempSeg)->v1->y < bottomY) bottomY = (*tempSeg)->v1->y;
    avg.x += (*tempSeg)->v1->x;
    avg.y += (*tempSeg)->v1->y;
  }
  avg.x /= po->numsegs;
  avg.y /= po->numsegs;

  // for now, polyobj can be linked only to one subsector
  // this will be changed later
  subsector_t *sub = PointInSubsector(avg); // bugfixed algo
  if (sub) {
    if (!po->IsLinkedToSubsector(sub)) po->LinkToSubsector(sub);
  } else {
    po->UnlinkFromAllSubsectors();
  }

  po->bbox2d[BOX2D_RIGHT] = MapBlock(rightX-BlockMapOrgX);
  po->bbox2d[BOX2D_LEFT] = MapBlock(leftX-BlockMapOrgX);
  po->bbox2d[BOX2D_TOP] = MapBlock(topY-BlockMapOrgY);
  po->bbox2d[BOX2D_BOTTOM] = MapBlock(bottomY-BlockMapOrgY);
  // add the polyobj to each blockmap section
  for (int j = po->bbox2d[BOX2D_BOTTOM]*BlockMapWidth; j <= po->bbox2d[BOX2D_TOP]*BlockMapWidth; j += BlockMapWidth) {
    for (int i = po->bbox2d[BOX2D_LEFT]; i <= po->bbox2d[BOX2D_RIGHT]; ++i) {
      if (i >= 0 && i < BlockMapWidth && j >= 0 && j < BlockMapHeight*BlockMapWidth) {
        polyblock_t **link = &PolyBlockMap[j+i];
        if (!(*link)) {
          // create a new link at the current block cell
          *link = new polyblock_t;
          (*link)->next = nullptr;
          (*link)->prev = nullptr;
          (*link)->polyobj = po;
          continue;
        }

        polyblock_t *tempLink = *link;
        while (tempLink->next != nullptr && tempLink->polyobj != nullptr) {
          tempLink = tempLink->next;
        }
        if (tempLink->polyobj == nullptr) {
          tempLink->polyobj = po;
          continue;
        } else {
          tempLink->next = new polyblock_t;
          tempLink->next->next = nullptr;
          tempLink->next->prev = tempLink;
          tempLink->next->polyobj = po;
        }
      }
      // else, don't link the polyobj, since it's off the map
    }
  }
}


//==========================================================================
//
//  VLevel::UnLinkPolyobj
//
//==========================================================================
void VLevel::UnLinkPolyobj (polyobj_t *po) {
  // remove the polyobj from each blockmap section
  for (int j = po->bbox2d[BOX2D_BOTTOM]; j <= po->bbox2d[BOX2D_TOP]; ++j) {
    int index = j*BlockMapWidth;
    for (int i = po->bbox2d[BOX2D_LEFT]; i <= po->bbox2d[BOX2D_RIGHT]; ++i) {
      if (i >= 0 && i < BlockMapWidth && j >= 0 && j < BlockMapHeight) {
        polyblock_t *link = PolyBlockMap[index+i];
        while (link != nullptr && link->polyobj != po) {
          link = link->next;
        }
        if (link == nullptr) {
          // polyobj not located in the link cell
          continue;
        }
        link->polyobj = nullptr;
      }
    }
  }
}


//==========================================================================
//
//  VLevel::GetPolyobjMirror
//
//==========================================================================
int VLevel::GetPolyobjMirror (int poly) {
  //FIXME: make this faster!
  polyobj_t *po = GetPolyobj(poly);
  return (po ? po->segs[0]->linedef->arg2 : 0);
}


//==========================================================================
//
//  VLevel::MovePolyobj
//
//==========================================================================
bool VLevel::MovePolyobj (int num, float x, float y, bool forced) {
  int count;
  seg_t **segList;
  seg_t **veryTempSeg;
  polyobj_t *po;
  TVec *prevPts;
  bool blocked;

  po = GetPolyobj(num);
  if (!po) Sys_Error("Invalid polyobj number: %d", num);

  if (IsForServer()) UnLinkPolyobj(po);

  segList = po->segs;
  prevPts = po->prevPts;
  blocked = false;

  for (count = po->numsegs; count; --count, ++segList, ++prevPts) {
    for (veryTempSeg = po->segs; veryTempSeg != segList; ++veryTempSeg) {
      if ((*veryTempSeg)->v1 == (*segList)->v1) break;
    }
    if (veryTempSeg == segList) {
      (*segList)->v1->x += x;
      (*segList)->v1->y += y;
    }
    if (IsForServer()) {
      // previous points are unique for each seg
      (*prevPts).x += x;
      (*prevPts).y += y;
    }
  }

  UpdatePolySegs(po);

  if (!forced && IsForServer()) {
    segList = po->segs;
    for (count = po->numsegs; count; --count, ++segList) {
      if (PolyCheckMobjBlocking(*segList, po)) blocked = true; //k8: break here?
    }
  }

  if (blocked) {
    count = po->numsegs;
    segList = po->segs;
    prevPts = po->prevPts;
    while (count--) {
      for (veryTempSeg = po->segs; veryTempSeg != segList; ++veryTempSeg) {
        if ((*veryTempSeg)->v1 == (*segList)->v1) break;
      }
      if (veryTempSeg == segList) {
        (*segList)->v1->x -= x;
        (*segList)->v1->y -= y;
      }
      (*prevPts).x -= x;
      (*prevPts).y -= y;
      ++segList;
      ++prevPts;
    }
    UpdatePolySegs(po);
    LinkPolyobj(po); // it is always for server
    return false;
  }

  po->startSpot.x += x;
  po->startSpot.y += y;
  if (IsForServer()) LinkPolyobj(po);

  return true;
}


//==========================================================================
//
//  VLevel::RotatePolyobj
//
//==========================================================================
bool VLevel::RotatePolyobj (int num, float angle) {
  // get the polyobject
  polyobj_t *po = GetPolyobj(num);
  if (!po) Sys_Error("Invalid polyobj number: %d", num);

  // calculate the angle
  float an = po->angle+angle;
  float msinAn, mcosAn;
  msincos(an, &msinAn, &mcosAn);

  if (IsForServer()) UnLinkPolyobj(po);

  seg_t **segList = po->segs;
  TVec *originalPts = po->originalPts;
  TVec *prevPts = po->prevPts;

  for (int count = po->numsegs; count; --count, ++segList, ++originalPts, ++prevPts) {
    if (IsForServer()) {
      // save the previous points
      prevPts->x = (*segList)->v1->x;
      prevPts->y = (*segList)->v1->y;
    }

    // get the original X and Y values
    float tr_x = originalPts->x;
    float tr_y = originalPts->y;

    // calculate the new X and Y values
    (*segList)->v1->x = (tr_x*mcosAn-tr_y*msinAn)+po->startSpot.x;
    (*segList)->v1->y = (tr_y*mcosAn+tr_x*msinAn)+po->startSpot.y;
  }
  UpdatePolySegs(po);

  bool blocked = false;
  if (IsForServer()) {
    segList = po->segs;
    for (int count = po->numsegs; count; --count, ++segList) {
      if (PolyCheckMobjBlocking(*segList, po)) blocked = true; //k8: break here?
    }
  }

  // if we are blocked then restore the previous points
  if (blocked) {
    segList = po->segs;
    prevPts = po->prevPts;
    for (int count = po->numsegs; count; --count, ++segList, ++prevPts) {
      (*segList)->v1->x = prevPts->x;
      (*segList)->v1->y = prevPts->y;
    }
    UpdatePolySegs(po);
    LinkPolyobj(po); // it is always for server
    return false;
  }

  po->angle = AngleMod(po->angle+angle);
  if (IsForServer()) LinkPolyobj(po);
  return true;
}


//==========================================================================
//
//  VLevel::PolyCheckMobjBlocking
//
//==========================================================================
bool VLevel::PolyCheckMobjBlocking (seg_t *seg, polyobj_t *po) {
  const line_t *ld = seg->linedef;

  int top = MapBlock(ld->bbox2d[BOX2D_TOP]-BlockMapOrgY/*+MAXRADIUS*/)+1;
  int bottom = MapBlock(ld->bbox2d[BOX2D_BOTTOM]-BlockMapOrgY/*-MAXRADIUS*/)-1;
  int left = MapBlock(ld->bbox2d[BOX2D_LEFT]-BlockMapOrgX/*-MAXRADIUS*/)-1;
  int right = MapBlock(ld->bbox2d[BOX2D_RIGHT]-BlockMapOrgX/*+MAXRADIUS*/)+1;

  if (top < 0 || right < 0 || bottom >= BlockMapHeight || left >= BlockMapWidth) return false;

  if (bottom < 0) bottom = 0;
  if (top >= BlockMapHeight) top = BlockMapHeight-1;
  if (left < 0) left = 0;
  if (right >= BlockMapWidth) right = BlockMapWidth-1;

  bool blocked = false;

  for (int j = bottom*BlockMapWidth; j <= top*BlockMapWidth; j += BlockMapWidth) {
    for (int i = left; i <= right; ++i) {
      for (VEntity *mobj = BlockLinks[j+i]; mobj; mobj = mobj->BlockMapNext) {
        if (mobj->IsGoingToDie()) continue;
        if (mobj->EntityFlags&VEntity::EF_ColideWithWorld) {
          if (mobj->EntityFlags&(VEntity::EF_Solid|VEntity::EF_Corpse)) {
            bool isSolid = !!(mobj->EntityFlags&VEntity::EF_Solid);

            float tmbbox[4];
            tmbbox[BOX2D_TOP] = mobj->Origin.y+mobj->Radius;
            tmbbox[BOX2D_BOTTOM] = mobj->Origin.y-mobj->Radius;
            tmbbox[BOX2D_LEFT] = mobj->Origin.x-mobj->Radius;
            tmbbox[BOX2D_RIGHT] = mobj->Origin.x+mobj->Radius;

            if (tmbbox[BOX2D_RIGHT] <= ld->bbox2d[BOX2D_LEFT] ||
                tmbbox[BOX2D_LEFT] >= ld->bbox2d[BOX2D_RIGHT] ||
                tmbbox[BOX2D_TOP] <= ld->bbox2d[BOX2D_BOTTOM] ||
                tmbbox[BOX2D_BOTTOM] >= ld->bbox2d[BOX2D_TOP])
            {
              continue;
            }
            if (P_BoxOnLineSide(tmbbox, ld) != -1) continue;

            if (isSolid) {
              mobj->Level->eventPolyThrustMobj(mobj, seg->normal, po);
              blocked = true;
            } else {
              mobj->Level->eventPolyCrushMobj(mobj, po);
            }
          }
        }
      }
    }
  }

  return blocked;
}


//==========================================================================
//
//  Script polyobject methods
//
//==========================================================================
IMPLEMENT_FUNCTION(VLevel, SpawnPolyobj) {
  P_GET_BOOL(hurt);
  P_GET_BOOL(crush);
  P_GET_INT(tag);
  P_GET_FLOAT(y);
  P_GET_FLOAT(x);
  P_GET_SELF;
  Self->SpawnPolyobj(x, y, tag, crush, hurt);
}

IMPLEMENT_FUNCTION(VLevel, AddPolyAnchorPoint) {
  P_GET_INT(tag);
  P_GET_FLOAT(y);
  P_GET_FLOAT(x);
  P_GET_SELF;
  Self->AddPolyAnchorPoint(x, y, tag);
}

IMPLEMENT_FUNCTION(VLevel, GetPolyobj) {
  P_GET_INT(polyNum);
  P_GET_SELF;
  RET_PTR(Self->GetPolyobj(polyNum));
}

IMPLEMENT_FUNCTION(VLevel, GetPolyobjMirror) {
  P_GET_INT(polyNum);
  P_GET_SELF;
  RET_INT(Self->GetPolyobjMirror(polyNum));
}

IMPLEMENT_FUNCTION(VLevel, MovePolyobj) {
  P_GET_BOOL_OPT(forced, false);
  P_GET_FLOAT(y);
  P_GET_FLOAT(x);
  P_GET_INT(num);
  P_GET_SELF;
  RET_BOOL(Self->MovePolyobj(num, x, y, forced));
}

IMPLEMENT_FUNCTION(VLevel, RotatePolyobj) {
  P_GET_FLOAT(angle);
  P_GET_INT(num);
  P_GET_SELF;
  RET_BOOL(Self->RotatePolyobj(num, angle));
}
