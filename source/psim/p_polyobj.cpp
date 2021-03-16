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
//  VLevel::PutBSPPObjBBox
//
//==========================================================================
void VLevel::PutBSPPObjBBox (int bspnum, polyobj_t *po) noexcept {
 tailcall:
  // found a subsector?
  if (BSPIDX_IS_NON_LEAF(bspnum)) {
    // nope
    const node_t *bsp = &Nodes[bspnum];
    // the order doesn't matter here, because we aren't doing any culling
    if (Are3DAnd2DBBoxesOverlap(bsp->bbox[0], po->bbox2d)) {
      if (Are3DAnd2DBBoxesOverlap(bsp->bbox[1], po->bbox2d)) {
        PutBSPPObjBBox(bsp->children[1], po);
      }
      bspnum = bsp->children[0];
      goto tailcall;
    } else if (Are3DAnd2DBBoxesOverlap(bsp->bbox[1], po->bbox2d)) {
      bspnum = bsp->children[1];
      goto tailcall;
    }
  } else {
    // check subsector
    const unsigned subidx = BSPIDX_LEAF_SUBSECTOR(bspnum);
    //GCon->Logf(NAME_Debug, "linking pobj #%d to subsector #%d (sector #%d)", po->tag, (int)subidx, (int)(ptrdiff_t)(Subsectors[subidx].sector-&Sectors[0]));
    po->AddSubsector(&Subsectors[subidx]);
  }
}


//==========================================================================
//
//  VLevel::PutPObjInSubsectors
//
//==========================================================================
void VLevel::PutPObjInSubsectors (polyobj_t *po) noexcept {
  if (!po) return;

  //TODO: make this faster by calculating overlapping rectangles or something
  po->RemoveAllSubsectors();

  if (NumSubsectors < 2) {
    if (NumSubsectors == 1) po->AddSubsector(&Subsectors[0]);
    return;
  }
  vassert(NumNodes > 0);

  //GCon->Logf(NAME_Debug, "linking pobj #%d (%g,%g)-(%g,%g)", po->tag, po->bbox2d[BOX2D_MINX], po->bbox2d[BOX2D_MINY], po->bbox2d[BOX2D_MAXX], po->bbox2d[BOX2D_MAXY]);
  return PutBSPPObjBBox(NumNodes-1, po);
}


//==========================================================================
//
//  polyobjpart_t::Free
//
//==========================================================================
void polyobjpart_t::Free () noexcept {
  delete[] segs;
  segs = nullptr;
  count = amount = 0;
}


//==========================================================================
//
//  polyobjpart_t::allocSeg
//
//==========================================================================
seg_t *polyobjpart_t::allocSeg () noexcept {
  if (count == amount) {
    amount += 64; // arbitrary number
    segs = (seg_t *)Z_Realloc(segs, sizeof(seg_t)*amount);
  }
  seg_t *res = &segs[count++];
  memset((void *)res, 0, sizeof(*res));
  return res;
}


//==========================================================================
//
//  polyobj_t::Free
//
//==========================================================================
void polyobj_t::Free () {
  delete[] segs;
  delete[] lines;
  delete[] originalPts;
  delete[] prevPts;
  polyobjpart_t *part = parts;
  while (part) {
    polyobjpart_t *c = part;
    part = part->nextpobj;
    c->Free();
    delete c;
  }
  part = freeparts;
  while (part) {
    polyobjpart_t *c = part;
    part = part->nextpobj;
    c->Free();
    delete c;
  }
}


//==========================================================================
//
//  polyobj_t::RemoveAllSubsectors
//
//==========================================================================
void polyobj_t::RemoveAllSubsectors () {
  while (parts) {
    polyobjpart_t *part = parts;
    parts = part->nextpobj;
    // remove it
    subsector_t *sub = part->sub;
    polyobjpart_t *prev = nullptr;
    polyobjpart_t *curr = sub->polyparts;
    while (curr && curr != part) { prev = curr; curr = curr->nextsub; }
    if (curr) {
      if (prev) prev->nextsub = part->nextsub; else sub->polyparts = part->nextsub;
    }
    // put to free parts pool
    part->reset();
    part->sub = nullptr;
    part->nextsub = nullptr;
    part->nextpobj = freeparts;
    freeparts = part;
  }
}


//==========================================================================
//
//  polyobj_t::AddSubsector
//
//  no need to check for duplicates here, there won't be any (yet)
//
//==========================================================================
void polyobj_t::AddSubsector (subsector_t *sub) {
  vassert(sub);
  polyobjpart_t *pp = freeparts;
  if (pp) {
    freeparts = pp->nextpobj;
  } else {
    pp = new polyobjpart_t;
    memset((void *)pp, 0, sizeof(*pp));
  }
  // set owner
  pp->pobj = this;
  // add part to our list
  pp->nextpobj = parts;
  parts = pp;
  // set subsector
  pp->sub = sub;
  // add part to subsector list
  pp->nextsub = sub->polyparts;
  sub->polyparts = pp;
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
}


//==========================================================================
//
//  cmpPobjSegs
//
//==========================================================================
/*
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
*/


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

  po->floor.XScale = po->floor.YScale = 1.0f;
  po->floor.TexZ = -99999.0f;
  po->ceiling.XScale = po->ceiling.YScale = 1.0f;
  po->ceiling.TexZ = -99999.0f;

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
    if (!seg.linedef) continue; //FIXME: we may need later to render floors, tho
    if (seg.linedef->special == PO_LINE_START && seg.linedef->arg1 == tag) {
      psegs.clear(); // we don't need 'em anymore
      seg.linedef->special = 0;
      seg.linedef->arg1 = 0;
      int PolySegCount = 1;
      TVec PolyStart = *seg.v1;
      IterFindPolySegs(*seg.v2, nullptr, PolySegCount, PolyStart);

      po->numsegs = PolySegCount;
      po->segs = new seg_t*[PolySegCount+1];
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
      // sort segs (why not?) (why do?)
      //timsort_r(psegs.ptr(), psegs.length(), sizeof(seg_t *), &cmpPobjSegs, (void *)&Lines[0]);
      po->numsegs = psegs.length();
      po->segs = new seg_t*[po->numsegs+1];
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

  // now collect polyobj lines (we'll need them to build clipped segs)
  int lcount = 0;
  TMapNC<line_t *, bool> lmap;
  for (int c = 0; c < po->numsegs; ++c) {
    line_t *ln = po->segs[c]->linedef;
    vassert(ln);
    if (!lmap.put(ln, true)) ++lcount;
  }

  po->lines = new line_t*[lcount+1];
  lmap.reset();
  lcount = 0;
  for (int c = 0; c < po->numsegs; ++c) {
    line_t *ln = po->segs[c]->linedef;
    vassert(ln);
    if (!lmap.put(ln, true)) {
      po->lines[lcount++] = ln;
      if (!ln->pobject) {
        ln->pobject = po;
      } else {
        if (ln->pobject != po) {
          GCon->Logf(NAME_Error, "invalid pobj tagged %d firstseg (line #%d, old tag #%d)", po->tag, (int)(ptrdiff_t)(ln-&Lines[0]), ln->pobject->tag);
          ln->pobject = po;
        }
      }
    }
  }
  po->numlines = lcount;
}


//==========================================================================
//
//  VLevel::ResetPObjRenderCounts
//
//  called from renderer
//
//==========================================================================
void VLevel::ResetPObjRenderCounts () noexcept {
  for (int i = 0; i < NumPolyObjs; ++i) PolyObjs[i]->rendercount = 0;
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
      Host_Error("StartSpot located without an Anchor point: %d", PolyObjs[i]->tag);
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

  float pobbox[4];
  pobbox[BOX2D_MINX] = pobbox[BOX2D_MINY] = +99999.0f;
  pobbox[BOX2D_MAXX] = pobbox[BOX2D_MAXY] = -99999.0f;

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
    pobbox[BOX2D_MINX] = min2(pobbox[BOX2D_MINX], (*tempSeg)->v1->x);
    pobbox[BOX2D_MINY] = min2(pobbox[BOX2D_MINY], (*tempSeg)->v1->y);
    pobbox[BOX2D_MAXX] = max2(pobbox[BOX2D_MAXX], (*tempSeg)->v1->x);
    pobbox[BOX2D_MAXY] = max2(pobbox[BOX2D_MAXY], (*tempSeg)->v1->y);
  }
  avg.x /= po->numsegs;
  avg.y /= po->numsegs;

  memcpy(po->bbox2d, pobbox, sizeof(po->bbox2d));

  // set initial floor and ceiling
  if (po->floor.TexZ < -90000.0f) {
    // do not set pics to nothing, let polyobjects have floors and ceilings, why not?
    subsector_t *sub = PointInSubsector(avg); // bugfixed algo
    po->floor = sub->sector->floor;
    //po->floor.pic = 0;
    if (!po->floor.isFloor()) po->floor.flipInPlace();
    po->ceiling = sub->sector->ceiling;
    //po->ceiling.pic = 0;
    if (!po->ceiling.isCeiling()) po->ceiling.flipInPlace();
  } else {
    GCon->Logf(NAME_Error, "double-spawned polyobject with tag %d (DON'T DO THIS!)", po->tag);
  }

  UpdatePolySegs(po);
  PutPObjInSubsectors(po);
}


//==========================================================================
//
//  VLevel::UpdatePolySegs
//
//==========================================================================
void VLevel::UpdatePolySegs (polyobj_t *po) {
  //IncrementValidCount();
  line_t **lineList = po->lines;
  for (int count = po->numlines; count; --count, ++lineList) {
    CalcLine(*lineList);
  }
  // recalc lines
  seg_t **segList = po->segs;
  for (int count = po->numsegs; count; --count, ++segList) {
    /*
    if ((*segList)->linedef->validcount != validcount) {
      // recalc lines's slope type, bounding box, normal and dist
      CalcLine((*segList)->linedef);
      (*segList)->linedef->validcount = validcount;
    }
    */
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
  /* k8 notes
     relink polyobject to the new subsector.
     this somewhat eases rendering glitches until i'll write the proper BSP splitter for pobjs.
     it is not the best way, but at least something.

     note that if it moves to the sector with a different height, the renderer
     adjusts it to the height of the new sector.

     to avoid this, i had to introduce new field to segs, which links back to the
     pobj. and i had to store the initial floor and ceiling planes in the pobj, so
     renderer can use them instead of the corresponding sector planes.
  */

  // calculate the polyobj bbox
  /*
  float rightX = (*tempSeg)->v1->x;
  float leftX = (*tempSeg)->v1->x;
  float topY = (*tempSeg)->v1->y;
  float bottomY = (*tempSeg)->v1->y;
  */
  float pobbox[4];
  pobbox[BOX2D_MINX] = pobbox[BOX2D_MINY] = +99999.0f;
  pobbox[BOX2D_MAXX] = pobbox[BOX2D_MAXY] = -99999.0f;
  seg_t * const* tempSeg = po->segs;
  for (int i = 0; i < po->numsegs; ++i, ++tempSeg) {
    pobbox[BOX2D_MINX] = min2(pobbox[BOX2D_MINX], (*tempSeg)->v1->x);
    pobbox[BOX2D_MINY] = min2(pobbox[BOX2D_MINY], (*tempSeg)->v1->y);
    pobbox[BOX2D_MAXX] = max2(pobbox[BOX2D_MAXX], (*tempSeg)->v1->x);
    pobbox[BOX2D_MAXY] = max2(pobbox[BOX2D_MAXY], (*tempSeg)->v1->y);
  }

  // just in case
  memcpy(po->bbox2d, pobbox, sizeof(po->bbox2d));

  PutPObjInSubsectors(po);

  const int xbboxRight = MapBlock(pobbox[BOX2D_MAXX]-BlockMapOrgX);
  const int xbboxLeft = MapBlock(pobbox[BOX2D_MINX]-BlockMapOrgX);
  const int xbboxTop = MapBlock(pobbox[BOX2D_MAXY]-BlockMapOrgY);
  const int xbboxBottom = MapBlock(pobbox[BOX2D_MINY]-BlockMapOrgY);

  const int bmxsize = BlockMapWidth;
  const int bmysize = BlockMapHeight*bmxsize;
  const int bmyend = xbboxTop*bmxsize;

  po->bmbbox[BOX2D_RIGHT] = xbboxRight;
  po->bmbbox[BOX2D_LEFT] = xbboxLeft;
  po->bmbbox[BOX2D_TOP] = xbboxTop;
  po->bmbbox[BOX2D_BOTTOM] = xbboxBottom;

  // add the polyobj to each blockmap section
  for (int by = xbboxBottom*bmxsize; by <= bmyend; by += bmxsize) {
    if (by < 0 || by >= bmysize) continue;
    for (int bx = xbboxLeft; bx <= xbboxRight; ++bx) {
      if (bx >= 0 && bx < bmxsize) {
        polyblock_t **link = &PolyBlockMap[by+bx];
        if (!(*link)) {
          // create a new link at the current block cell
          *link = new polyblock_t;
          (*link)->next = nullptr;
          (*link)->prev = nullptr;
          (*link)->polyobj = po;
          continue;
        }

        polyblock_t *tempLink = *link;
        while (tempLink->next != nullptr && tempLink->polyobj != nullptr) tempLink = tempLink->next;
        if (tempLink->polyobj == nullptr) {
          tempLink->polyobj = po;
        } else {
          tempLink->next = new polyblock_t;
          tempLink->next->next = nullptr;
          tempLink->next->prev = tempLink;
          tempLink->next->polyobj = po;
        }
      }
    }
  }
}


//==========================================================================
//
//  VLevel::UnLinkPolyobj
//
//==========================================================================
void VLevel::UnLinkPolyobj (polyobj_t *po) {
  const int xbboxRight = po->bmbbox[BOX2D_RIGHT];
  const int xbboxLeft = po->bmbbox[BOX2D_LEFT];
  const int xbboxTop = po->bmbbox[BOX2D_TOP];
  const int xbboxBottom = po->bmbbox[BOX2D_BOTTOM];

  const int bmxsize = BlockMapWidth;
  const int bmysize = BlockMapHeight*bmxsize;
  const int bmyend = xbboxTop*bmxsize;

  // remove the polyobj from each blockmap section
  for (int by = xbboxBottom*bmxsize; by <= bmyend; by += bmxsize) {
    if (by < 0 || by >= bmysize) continue;
    for (int bx = xbboxLeft; bx <= xbboxRight; ++bx) {
      if (bx >= 0 && bx < bmxsize) {
        polyblock_t *link = PolyBlockMap[by+bx];
        while (link != nullptr && link->polyobj != po) link = link->next;
        if (link) link->polyobj = nullptr;
      }
    }
  }

  po->RemoveAllSubsectors();
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

  if (!forced && IsForServer()) blocked = PolyCheckMobjBlocked(po);

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
  if (IsForServer()) blocked = PolyCheckMobjBlocked(po);

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
//  VLevel::PolyCheckMobjLineBlocking
//
//==========================================================================
bool VLevel::PolyCheckMobjLineBlocking (const line_t *ld, polyobj_t *po) {
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

  for (int by = bottom*BlockMapWidth; by <= top*BlockMapWidth; by += BlockMapWidth) {
    for (int bx = left; bx <= right; ++bx) {
      for (VEntity *mobj = BlockLinks[by+bx]; mobj; mobj = mobj->BlockMapNext) {
        if (mobj->IsGoingToDie()) continue;
        if (mobj->EntityFlags&VEntity::EF_ColideWithWorld) {
          if (mobj->EntityFlags&(VEntity::EF_Solid|VEntity::EF_Corpse)) {
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

            // check mobj height (pobj floor and ceiling shouldn't be sloped here)
            if (mobj->Origin.z > -po->ceiling.dist || mobj->Origin.z+max2(0.0f, mobj->Height) < po->floor.dist) continue;

            if (!mobj->IsBlockingLine(ld)) continue;

            if (P_BoxOnLineSide(tmbbox, ld) != -1) continue;

            if (mobj->EntityFlags&VEntity::EF_Solid) {
              mobj->Level->eventPolyThrustMobj(mobj, /*seg*/ld->normal, po);
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
//  VLevel::PolyCheckMobjBlocked
//
//==========================================================================
bool VLevel::PolyCheckMobjBlocked (polyobj_t *po) {
  if (!po || po->numlines == 0) return false;
  bool blocked = false;
  /*
  seg_t **segList = po->segs;
  for (int count = po->numsegs; count; --count, ++segList) {
    if (PolyCheckMobjBlocking((*segList)->linedef, po)) blocked = true; //k8: break here?
  }
  */
  line_t **lineList = po->lines;
  for (int count = po->numlines; count; --count, ++lineList) {
    if (PolyCheckMobjLineBlocking(*lineList, po)) blocked = true; //k8: break here?
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
