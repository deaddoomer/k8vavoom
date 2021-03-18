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


static VCvarB dbg_pobj_disable("dbg_pobj_disable", false, "Disable most polyobject operations?", CVAR_PreInit);


//==========================================================================
//
//  pobjAddSubsector
//
//  `*udata` is `polyobj_t`
//
//==========================================================================
static bool pobjAddSubsector (VLevel *level, subsector_t *sub, void *udata) {
  polyobj_t *po = (polyobj_t *)udata;
  po->AddSubsector(sub);
  //GCon->Logf(NAME_Debug, "linking pobj #%d (%g,%g)-(%g,%g) to subsector #%d", po->tag, po->bbox2d[BOX2D_MINX], po->bbox2d[BOX2D_MINY], po->bbox2d[BOX2D_MAXX], po->bbox2d[BOX2D_MAXY], (int)(ptrdiff_t)(sub-&level->Subsectors[0]));
  return true; // continue checking
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

  CheckBSPB2DBox(po->bbox2d, &pobjAddSubsector, po);
}


//==========================================================================
//
//  polyobjpart_t::Free
//
//==========================================================================
void polyobjpart_t::Free () noexcept {
  delete[] segs;
  delete[] verts;
  segs = nullptr;
  verts = nullptr;
  count = amount = 0;
}


//==========================================================================
//
//  polyobjpart_t::allocSeg
//
//==========================================================================
seg_t *polyobjpart_t::allocSeg (const seg_t *oldseg) noexcept {
  if (count == amount) {
    amount += 64; // arbitrary number
    segs = (seg_t *)Z_Realloc(segs, sizeof(seg_t)*amount);
    verts = (TVec *)Z_Realloc(verts, sizeof(TVec)*(amount*2));
    for (unsigned f = 0; f < (unsigned)count; ++f) {
      segs[f].v1 = &verts[f*2u+0];
      segs[f].v2 = &verts[f*2u+1];
    }
  }
  seg_t *res = &segs[count];
  memset((void *)res, 0, sizeof(*res));
  if (oldseg) *res = *oldseg;
  res->v1 = &verts[count*2+0];
  res->v2 = &verts[count*2+1];
  if (oldseg) {
    *res->v1 = *oldseg->v1;
    *res->v2 = *oldseg->v2;
  }
  ++count;
  return res;
}


//==========================================================================
//
//  polyobjpart_t::allocInitedSeg
//
//==========================================================================
void polyobjpart_t::CreateClipSegs (VLevel *level) {
  reset();
  flags |= CLIPSEGS_CREATED;
  // create clip segs for each polyobject part
  for (auto &&it : pobj->SegFirst()) {
    // clip pobj seg to subsector
    const seg_t *seg = it.seg();
    if (!seg->linedef) continue;
    seg_t newseg = *seg;
    TVec sv1 = *seg->v1;
    TVec sv2 = *seg->v2;
    newseg.v1 = &sv1;
    newseg.v2 = &sv2;
    if (!level->ClipPObjSegToSub(sub, &newseg)) continue; // out of this subsector
    (void)allocSeg(&newseg);
  }
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
  delete[] segPts;
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
        if (seg->frontsector) seg->frontsector->linecount = 0;
        if (seg->backsector) seg->backsector->linecount = 0; // mark inner polyobject sectors too
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
//  VLevel::SpawnPolyobj
//
//==========================================================================
void VLevel::SpawnPolyobj (float x, float y, int tag, bool crush, bool hurt) {
  int index = NumPolyObjs++;

  polyobj_t *po = new polyobj_t;
  memset((void *)po, 0, sizeof(polyobj_t));
  po->index = index;
  po->tag = tag;

  po->pofloor.XScale = po->pofloor.YScale = 1.0f;
  po->pofloor.TexZ = -99999.0f;
  po->poceiling.XScale = po->poceiling.YScale = 1.0f;
  po->poceiling.TexZ = -99999.0f;

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
      if (seg.frontsector) seg.frontsector->linecount = 0;
      if (seg.backsector) seg.backsector->linecount = 0; // mark inner polyobject sectors too
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
      if (seg.backsector) seg.backsector->linecount = 0; // mark inner polyobject sectors too
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

  // collect 2-sided polyobject subsectors
  // line backside should point to polyobject sector
  sector_t *posec = nullptr;
  for (int f = 0; f < po->numlines; ++f) {
    const line_t *ld = po->lines[f];
    if (!(ld->flags&ML_TWOSIDED)) return; // invalid polyobject
    if (!ld->backsector) return;
    if (posec && posec != ld->backsector) return;
    posec = ld->backsector;
  }
  if (!posec) return;
  if (posec->ownpobj && posec->ownpobj != po) return; // oopsie
  posec->ownpobj = po;

  TArray<seg_t *> moresegs;

  GCon->Logf(NAME_Debug, "pobj #%d (%d) sector #%d", po->tag, tag, (int)(ptrdiff_t)(posec-&Sectors[0]));
  for (subsector_t *sub = posec->subsectors; sub; sub = sub->seclink) {
    vassert(sub->sector == posec);
    sub->ownpobj = po;
    GCon->Logf(NAME_Debug, "  subsector #%d", (int)(ptrdiff_t)(sub-&Subsectors[0]));
    // collect minisegs
    for (int f = 0; f < sub->numlines; ++f) {
      seg_t *ss = &Segs[sub->firstline+f];
      bool found = false;
      for (int c = 0; c < po->numsegs; ++c) if (po->segs[c] == ss) { found = true; break; }
      if (!found) for (int c = 0; c < moresegs.length(); ++c) if (moresegs[c] == ss) { found = true; break; }
      if (!found) {
        GCon->Logf(NAME_Debug, "    seg #%d is not collected (%d)", (int)(ptrdiff_t)(ss-&Segs[0]), (ss->linedef ? (int)(ptrdiff_t)(ss->linedef-&Lines[0]) : -1));
        moresegs.append(ss);
      }
    }
  }

  po->posector = posec;
  if (moresegs.length()) {
    // apppend minisegs
    seg_t **ns = new seg_t*[po->numsegs+moresegs.length()];
    if (po->numsegs) memcpy((void *)ns, po->segs, sizeof(po->segs[0])*po->numsegs);
    for (int f = 0; f < moresegs.length(); ++f) {
      seg_t *ss = moresegs[f];
      ss->pobj = po;
      ns[po->numsegs+f] = ss;
    }
    po->numsegs += moresegs.length();
    delete[] po->segs;
    po->segs = ns;
  }
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
//  called from `LoadMap()` after spawning the world
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
  //polyobj_t *po = GetPolyobj(tag);
  polyobj_t *po = nullptr;
  for (int i = 0; i < NumPolyObjs; ++i) {
    if (PolyObjs[i]->tag == tag) {
      if (!PolyObjs[i]->originalPts) {
        po = PolyObjs[i];
        break;
      } else {
        if (!po) po = PolyObjs[i];
      }
    }
  }

  if (!po) Host_Error("Unable to match polyobj tag: %d", tag); // didn't match the tag with a polyobj tag
  if (po->segs == nullptr) Host_Error("Anchor point located without a StartSpot point: %d", tag);
  vassert(po->numsegs);

  if (po->originalPts) {
    GCon->Logf(NAME_Error, "polyobject with tag #%d is translated to more than one starting point!", po->tag);
    return; // oopsie
  }

  const int maxpts = po->numsegs+po->numlines;
  po->originalPts = new TVec[maxpts];
  po->segPts = new TVec*[maxpts];

  const float deltaX = originX-po->startSpot.x;
  const float deltaY = originY-po->startSpot.y;

  seg_t **tempSeg = po->segs;
  TVec *origPt = po->originalPts;
  TVec avg(0, 0, 0); // used to find a polyobj's center, and hence the subsector
  int segptnum = 0;

  for (int i = 0; i < po->numsegs; ++i, ++tempSeg) {
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
    *origPt++ = *(*tempSeg)->v1-po->startSpot;
    po->segPts[segptnum++] = (*tempSeg)->v1;
  }
  vassert(segptnum <= po->numsegs);
  avg.x /= segptnum;
  avg.y /= segptnum;

  // now add linedef vertices (if they are missing)
  for (int i = 0; i < po->numlines; ++i) {
    line_t *ln = po->lines[i];
    for (int f = 0; f < 2; ++f) {
      TVec *v = (f ? ln->v2 : ln->v2);
      bool found = false;
      for (int c = 0; c < segptnum; ++c) {
        if (po->segPts[c] == v) {
          found = true;
          break;
        }
      }
      if (!found) {
        // the original Pts are based off the startSpot Pt
        v->x -= deltaX;
        v->y -= deltaY;
        *origPt++ = (*v)-po->startSpot;
        po->segPts[segptnum++] = v;
      }
    }
  }
  vassert(segptnum <= po->numsegs+po->numlines);
  po->originalPtsCount = segptnum;
  po->segPtsCount = segptnum;

  // set initial floor and ceiling
  {
    // do not set pics to nothing, let polyobjects have floors and ceilings, why not?
    // flip them, because polyobject flats are like 3d floor flats
    sector_t *sec = (po->posector ? po->posector : PointInSubsector(avg)->sector); // bugfixed algo

    // build floor
    po->pofloor = sec->floor;
    //po->pofloor.pic = 0;
    //if (po->pofloor.isFloor()) po->pofloor.flipInPlace();
    //const float fdist = -po->pofloor.PointDistance(TVec(avg.x, avg.y, 0.0f));
    const float fdist = po->pofloor.GetPointZ(TVec(avg.x, avg.y, 0.0f));
    //GCon->Logf(NAME_Debug, "000: pobj #%d: floor=(%g,%g,%g:%g):%g (%g:%g)", po->tag, po->pofloor.normal.x, po->pofloor.normal.y, po->pofloor.normal.z, po->pofloor.dist, fdist, po->pofloor.minz, po->pofloor.maxz);
    // floor normal points down
    po->pofloor.normal = TVec(0.0f, 0.0f, -1.0f);
    po->pofloor.dist = -fdist;
    //GCon->Logf(NAME_Debug, "001: pobj #%d: floor=(%g,%g,%g:%g):%g", po->tag, po->pofloor.normal.x, po->pofloor.normal.y, po->pofloor.normal.z, po->pofloor.dist, po->pofloor.GetPointZ(TVec(avg.x, avg.y, 0.0f)));
    po->pofloor.minz = po->pofloor.maxz = fdist;

    // build ceiling
    po->poceiling = sec->ceiling;
    //po->poceiling.pic = 0;
    //if (po->poceiling.isCeiling()) po->poceiling.flipInPlace();
    const float cdist = po->poceiling.GetPointZ(TVec(avg.x, avg.y, 0.0f));
    //GCon->Logf(NAME_Debug, "000: pobj #%d: ceiling=(%g,%g,%g:%g):%g (%g:%g)", po->tag, po->poceiling.normal.x, po->poceiling.normal.y, po->poceiling.normal.z, po->poceiling.dist, cdist, po->poceiling.minz, po->poceiling.maxz);
    // ceiling normal points up
    po->poceiling.normal = TVec(0.0f, 0.0f, 1.0f);
    po->poceiling.dist = cdist;
    //GCon->Logf(NAME_Debug, "001: pobj #%d: ceiling=(%g,%g,%g:%g):%g", po->tag, po->poceiling.normal.x, po->poceiling.normal.y, po->poceiling.normal.z, po->poceiling.dist, po->poceiling.GetPointZ(TVec(avg.x, avg.y, 0.0f)));
    po->poceiling.minz = po->poceiling.maxz = cdist;

    // fix polyobject height
    //FIXME: polyobject segs should not be minisegs!
    if (po->numlines && (po->segs[0]->linedef->flags&(ML_TWOSIDED|ML_WRAP_MIDTEX)) == ML_TWOSIDED && po->pofloor.maxz < po->poceiling.minz) {
      const seg_t *seg = po->segs[0];
      //auto midTexType = GTextureManager.GetTextureType(seg->sidedef->MidTexture);

      // determine real height using midtex
      //const sector_t *sec = seg->backsector; //(!seg->side ? ldef->backsector : ldef->frontsector);
      VTexture *MTex = GTextureManager(seg->sidedef->MidTexture);
      if (MTex && MTex->Type != TEXTYPE_Null) {
        // here we should check if midtex covers the whole height, as it is not tiled vertically (if not wrapped)
        const float texh = MTex->GetScaledHeight();
        float z0, z1;
        if (po->segs[0]->linedef->flags&ML_DONTPEGBOTTOM) {
          // bottom of texture at bottom
          // top of texture at top
          z1 = po->pofloor.TexZ+texh;
        } else {
          // top of texture at top
          z1 = po->poceiling.TexZ;
        }
        //k8: dunno why
        if (seg->sidedef->Mid.RowOffset < 0) {
          z1 += (seg->sidedef->Mid.RowOffset+texh)*(!MTex->bWorldPanning ? 1.0f : 1.0f/MTex->TScale);
        } else {
          z1 += seg->sidedef->Mid.RowOffset*(!MTex->bWorldPanning ? 1.0f : 1.0f/MTex->TScale);
        }
        z0 = z1-texh;
        z0 = max2(z0, sec->floor.minz);
        z1 = min2(z1, sec->ceiling.maxz);
        if (z1 < z0) z1 = z0;
        // fix floor and ceiling planes
        po->pofloor.minz = po->pofloor.maxz = z0;
        po->pofloor.dist = -po->pofloor.maxz;
        po->poceiling.minz = po->poceiling.maxz = z1;
        po->poceiling.dist = po->poceiling.maxz;
      }

      //GCon->Logf(NAME_Debug, "002: pobj #%d: floor=(%g,%g,%g:%g):%g", po->tag, po->pofloor.normal.x, po->pofloor.normal.y, po->pofloor.normal.z, po->pofloor.dist, po->pofloor.GetPointZ(TVec(avg.x, avg.y, 0.0f)));
      //GCon->Logf(NAME_Debug, "002: pobj #%d: ceiling=(%g,%g,%g:%g):%g", po->tag, po->poceiling.normal.x, po->poceiling.normal.y, po->poceiling.normal.z, po->poceiling.dist, po->poceiling.GetPointZ(TVec(avg.x, avg.y, 0.0f)));

    }

    sec = po->posector;
    if (sec) {
      sec->floor.normal = po->pofloor.normal;
      sec->floor.dist = po->pofloor.dist;
      sec->ceiling.normal = po->poceiling.normal;
      sec->ceiling.dist = po->poceiling.dist;
    }
  }

  float pobbox[4];
  pobbox[BOX2D_MINX] = pobbox[BOX2D_MINY] = +99999.0f;
  pobbox[BOX2D_MAXX] = pobbox[BOX2D_MAXY] = -99999.0f;
  TVec * const *pp = po->segPts;
  for (int f = segptnum; f--; ++pp) {
    pobbox[BOX2D_MINX] = min2(pobbox[BOX2D_MINX], (*pp)->x);
    pobbox[BOX2D_MINY] = min2(pobbox[BOX2D_MINY], (*pp)->y);
    pobbox[BOX2D_MAXX] = max2(pobbox[BOX2D_MAXX], (*pp)->x);
    pobbox[BOX2D_MAXY] = max2(pobbox[BOX2D_MAXY], (*pp)->y);
  }
  memcpy(po->bbox2d, pobbox, sizeof(po->bbox2d));

  UpdatePolySegs(po);

  // no need to notify renderer yet (or update subsector list), polyobject spawner will do it all
  /*
  PutPObjInSubsectors(po);
  // notify renderer that this polyobject is moved
  //if (Renderer) Renderer->PObjModified(po);
  */
}


//==========================================================================
//
//  VLevel::UpdatePolySegs
//
//==========================================================================
void VLevel::UpdatePolySegs (polyobj_t *po) {
  // recalc lines's slope type, bounding box, normal and dist
  for (auto &&it : po->LineFirst()) CalcLine(it.line());
  // recalc seg's normal and dist
  for (auto &&it : po->SegFirst()) CalcSeg(it.seg());
  // update region heights
  sector_t *sec = po->posector;
  if (sec) {
    sec->floor.normal = po->pofloor.normal;
    sec->floor.dist = po->pofloor.dist;
    sec->ceiling.normal = po->poceiling.normal;
    sec->ceiling.dist = po->poceiling.dist;
    // update subsector bounding boxes
    for (subsector_t *sub = sec->subsectors; sub; sub = sub->seclink) {
      if (sub->numlines < 1) continue;
      sub->bbox2d[BOX2D_MINX] = sub->bbox2d[BOX2D_MINY] = +FLT_MAX;
      sub->bbox2d[BOX2D_MAXX] = sub->bbox2d[BOX2D_MAXY] = -FLT_MAX;
      const seg_t *seg = &Segs[sub->firstline];
      for (int f = sub->numlines; f--; ++seg) {
        // v1
        sub->bbox2d[BOX2D_MINX] = min2(sub->bbox2d[BOX2D_MINX], seg->v1->x);
        sub->bbox2d[BOX2D_MINY] = min2(sub->bbox2d[BOX2D_MINY], seg->v1->y);
        sub->bbox2d[BOX2D_MAXX] = max2(sub->bbox2d[BOX2D_MAXX], seg->v1->x);
        sub->bbox2d[BOX2D_MAXY] = max2(sub->bbox2d[BOX2D_MAXY], seg->v1->y);
        // v2
        sub->bbox2d[BOX2D_MINX] = min2(sub->bbox2d[BOX2D_MINX], seg->v2->x);
        sub->bbox2d[BOX2D_MINY] = min2(sub->bbox2d[BOX2D_MINY], seg->v2->y);
        sub->bbox2d[BOX2D_MAXX] = max2(sub->bbox2d[BOX2D_MAXX], seg->v2->x);
        sub->bbox2d[BOX2D_MAXY] = max2(sub->bbox2d[BOX2D_MAXY], seg->v2->y);
      }
    }
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
  float pobbox[4];
  pobbox[BOX2D_MINX] = pobbox[BOX2D_MINY] = +99999.0f;
  pobbox[BOX2D_MAXX] = pobbox[BOX2D_MAXY] = -99999.0f;
  TVec * const *pp = po->segPts;
  for (int f = po->segPtsCount; f--; ++pp) {
    pobbox[BOX2D_MINX] = min2(pobbox[BOX2D_MINX], (*pp)->x);
    pobbox[BOX2D_MINY] = min2(pobbox[BOX2D_MINY], (*pp)->y);
    pobbox[BOX2D_MAXX] = max2(pobbox[BOX2D_MAXX], (*pp)->x);
    pobbox[BOX2D_MAXY] = max2(pobbox[BOX2D_MAXY], (*pp)->y);
  }
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
  polyobj_t *po = GetPolyobj(num);
  if (!po) Sys_Error("Invalid polyobj number: %d", num);

  if (IsForServer()) UnLinkPolyobj(po);

  EnsurePolyPrevPts(po->segPtsCount);

  const TVec delta(po->startSpot.x+x, po->startSpot.y+y, 0.0f);

  const TVec *origPts = po->originalPts;
  TVec *prevPts = polyPrevPts;
  TVec **vptr = po->segPts;
  for (int f = po->segPtsCount; f--; ++vptr, ++origPts, ++prevPts) {
    *prevPts = **vptr;
    **vptr = (*origPts)+delta;
  }
  UpdatePolySegs(po);

  const bool blocked = (!forced && IsForServer() ? PolyCheckMobjBlocked(po) : false);

  if (blocked) {
    // restore points
    prevPts = polyPrevPts;
    vptr = po->segPts;
    for (int f = po->segPtsCount; f--; ++vptr, ++prevPts) **vptr = *prevPts;
    UpdatePolySegs(po);
    LinkPolyobj(po); // it is always for server
    return false;
  }

  po->startSpot.x += x;
  po->startSpot.y += y;
  if (IsForServer()) LinkPolyobj(po);

  // notify renderer that this polyobject is moved
  if (Renderer) Renderer->PObjModified(po);

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
  const float an = AngleMod(po->angle+angle);
  float msinAn, mcosAn;
  msincos(an, &msinAn, &mcosAn);

  if (IsForServer()) UnLinkPolyobj(po);

  EnsurePolyPrevPts(po->segPtsCount);

  const float ssx = po->startSpot.x;
  const float ssy = po->startSpot.y;
  const TVec *origPts = po->originalPts;
  TVec *prevPts = polyPrevPts;
  TVec **vptr = po->segPts;
  for (int f = po->segPtsCount; f--; ++vptr, ++prevPts, ++origPts) {
    // save the previous point
    *prevPts = **vptr;
    // get the original X and Y values
    const float tr_x = origPts->x;
    const float tr_y = origPts->y;
    // calculate the new X and Y values
    (*vptr)->x = (tr_x*mcosAn-tr_y*msinAn)+ssx;
    (*vptr)->y = (tr_y*mcosAn+tr_x*msinAn)+ssy;
  }
  UpdatePolySegs(po);

  const bool blocked = (IsForServer() ? PolyCheckMobjBlocked(po) : false);

  // if we are blocked then restore the previous points
  if (blocked) {
    // restore points
    prevPts = polyPrevPts;
    vptr = po->segPts;
    for (int f = po->segPtsCount; f--; ++vptr, ++prevPts) **vptr = *prevPts;
    UpdatePolySegs(po);
    LinkPolyobj(po); // it is always for server
    return false;
  }

  //po->angle = AngleMod(po->angle+angle);
  po->angle = an;
  if (IsForServer()) LinkPolyobj(po);

  // notify renderer that this polyobject is moved
  if (Renderer) Renderer->PObjModified(po);

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
            if (mobj->Origin.z >= po->poceiling.maxz || mobj->Origin.z+max2(0.0f, mobj->Height) <= po->pofloor.minz) continue;

            //TODO: crush corpses!

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
