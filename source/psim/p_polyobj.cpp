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


enum {
  // thing height is z offset from destination sector floor
  // polyobject anchor thing z is maximum 3d pobj height (if not zero)
  PO_SPAWN_3D_TYPE = 9369,
};


static TMapNC<VEntity *, bool> poEntityMap;
static TMapNC<VEntity *, bool> poEntityMapToMove;
//static TMapNC<VEntity *, TVec> poEntityMapFloat;
static TMapNC<sector_t *, bool> poSectorMap;
//static TArray<sector_t *> poEntityArray;


struct SavedEntityData {
  VEntity *mobj;
  TVec Origin;
  polyobj_t *po; // used in rotator
};

static TArray<SavedEntityData> poAffectedEnitities;


//==========================================================================
//
//  CollectPObjTouchingThingsRough
//
//  collect all objects from sectors this polyobject may touch
//
//==========================================================================
static void CollectPObjTouchingThingsRough (polyobj_t *po) {
  //FIXME: do not use static map here
  poEntityMap.reset();
  poSectorMap.reset();
    //const int visCount = Level->nextVisitedCount();
    //if (n->Visited != visCount) n->Visited = visCount;
  for (polyobjpart_t *part = po->parts; part; part = part->nextpobj) {
    sector_t *sec = part->sub->sector;
    if (sec->isAnyPObj()) continue; // just in case
    if (!poSectorMap.put(sec, true)) {
      //GCon->Logf(NAME_Debug, "pobj #%d: checking sector #%d for entities...", po->tag, (int)(ptrdiff_t)(sec-&Sectors[0]));
      // new sector, process things
      for (msecnode_t *n = sec->TouchingThingList; n; n = n->SNext) {
        //GCon->Logf(NAME_Debug, "pobj #%d:   found entity %s(%u)", po->tag, n->Thing->GetClass()->GetName(), n->Thing->GetUniqueId());
        poEntityMap.put(n->Thing, true);
      }
    }
  }
}


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
//  VLevel::RegisterPolyObj
//
//  `poidx` is NOT a tag!
//
//==========================================================================
void VLevel::RegisterPolyObj (int poidx) noexcept {
  vassert(poidx >= 0 && poidx < NumPolyObjs);
  vassert(PolyTagMap);
  if (!PolyTagMap->has(PolyObjs[poidx]->tag)) {
    PolyTagMap->put(PolyObjs[poidx]->tag, poidx);
  } else {
    GCon->Logf(NAME_Error, "duplicate pobj tag #%d", PolyObjs[poidx]->tag);
  }
}


//==========================================================================
//
//  VLevel::GetPolyobj
//
//==========================================================================
polyobj_t *VLevel::GetPolyobj (int polyNum) noexcept {
  /*
  //FIXME: make this faster!
  for (int i = 0; i < NumPolyObjs; ++i) {
    if (PolyObjs[i]->tag == polyNum) return PolyObjs[i];
  }
  return nullptr;
  */
  auto pip = PolyTagMap->find(polyNum);
  return (pip ? PolyObjs[*pip] : nullptr);
}


//==========================================================================
//
//  VLevel::GetPolyobjMirror
//
//==========================================================================
int VLevel::GetPolyobjMirror (int poly) {
  polyobj_t *po = GetPolyobj(poly);
  return (po ? po->mirror : 0);
}


//==========================================================================
//
//  explinesCompare
//
//==========================================================================
static int explinesCompare (const void *aa, const void *bb, void *) {
  if (aa == bb) return 0; // just in case
  const line_t *a = *(const line_t **)aa;
  const line_t *b = *(const line_t **)bb;
  //GCon->Logf(NAME_Debug, "compare lines; specials=(%d,%d); order=(%d,%d)", a->special, b->special, a->arg2, b->arg2);
  return a->arg2-b->arg2;
}


//==========================================================================
//
//  VLevel::SpawnPolyobj
//
//==========================================================================
void VLevel::SpawnPolyobj (mthing_t *thing, float x, float y, float height, int tag, bool crush, bool hurt) {
  if (/*tag == 0 ||*/ tag == 0x7fffffff) Host_Error("the map tried to spawn polyobject with invalid tag: %d", tag);

  const int index = NumPolyObjs++;

  GCon->Logf(NAME_Debug, "SpawnPolyobj: tag=%d; idx=%d; thing=%d", tag, index, (thing ? (int)(ptrdiff_t)(thing-&Things[0]) : -1));

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
  RegisterPolyObj(index);

  po->startSpot.x = x;
  po->startSpot.y = y;
  po->startSpot.z = 0;

  // find either "explicit" or "line start" special for this polyobject
  TArray<line_t *> explines; // the first item is "explicit line" if `lstart` is `nullptr`
  line_t *lstart = nullptr; // if this is set, this is "line start"

  for (auto &&itline : allLines()) {
    line_t *line = &itline;
    if (line->special == PO_LINE_START && line->arg1 == tag) {
      if (lstart) {
        Host_Error("found two `Polyobj_StartLine` specials for polyobject with tag #%d (lines #%d and #%d)", tag, (int)(ptrdiff_t)(lstart-&Lines[0]), (int)(ptrdiff_t)(line-&Lines[0]));
        //continue;
      }
      if (explines.length()) Host_Error("found both `Polyobj_StartLine` and `Polyobj_ExplicitLine` specials for polyobject with tag #%d (implicit line #%d)", tag, (int)(ptrdiff_t)(line-&Lines[0]));
      lstart = line;
    } else if (line->special == PO_LINE_EXPLICIT && line->arg1 == tag) {
      if (lstart) Host_Error("found both `Polyobj_StartLine` and `Polyobj_ExplicitLine` specials for polyobject with tag #%d (implicit line #%d)", tag, (int)(ptrdiff_t)(lstart-&Lines[0]));
      explines.append(line);
    }
  }

  if (!lstart && explines.length() == 0) Host_Error("no lines found for polyobject with tag #%d", tag);

  // sort explicit lines by order (it doesn't matter, but why not?)
  if (!lstart && explines.length() > 1) {
    timsort_r(explines.ptr(), explines.length(), sizeof(explines[0]), &explinesCompare, nullptr);
  }

  // now collect lines for "start line" type
  if (lstart) {
    vassert(explines.length() == 0);
    IncrementValidCount();
    explines.append(lstart);
    line_t *ld = lstart;
    ld->validcount = validcount;
    for (;;) {
      // take first line and go
      if (ld->vxCount(1) == 0) Host_Error("missing line for implicit polyobject with tag #%d (at line #%d)", tag, (int)(ptrdiff_t)(ld-&Lines[0]));
      ld = ld->vxLine(1, 0);
      if (ld->validcount == validcount) Host_Error("found loop in implicit polyobject with tag #%d (at line #%d)", tag, (int)(ptrdiff_t)(ld-&Lines[0]));
      ld->validcount = validcount;
      explines.append(ld);
      if (ld->v2 == lstart->v1) break;
    }
  }

  // mark pobj sectors as "empty"
  // set sector's line count to 0 to force it not to be rendered even if we do a no-clip into it
  for (auto &&ld : explines) {
    sector_t *sec = ld->frontsector;
    if (sec && sec->linecount) { sec->prevlinecount = sec->linecount; sec->linecount = 0; }
    sec = ld->backsector;
    if (sec && sec->linecount) { sec->prevlinecount = sec->linecount; sec->linecount = 0; }
  }

  // now check if this is a valid 3d pobj
  // valid 3d pobj should have properly closed contours without "orphan" lines
  // all lines should be two-sided, with the same frontsector and same backsector (and those sectors must be different)

  // check if all lines are two-sided, with valid sectors
  bool valid3d = true;
  sector_t *sfront = nullptr;
  sector_t *sback = nullptr;

  // but don't bother if we don't want to spawn 3d pobj
  if (thing && thing->type == PO_SPAWN_3D_TYPE) {
    for (auto &&ld : explines) {
      if (!(ld->flags&ML_TWOSIDED)) { valid3d = false; break; } // found one-sided line
      if (!ld->frontsector || !ld->backsector || ld->backsector == ld->frontsector) { valid3d = false; break; } // invalid sectors
      if (!sfront) { sfront = ld->frontsector; sback = ld->backsector; }
      if (ld->frontsector != sfront || ld->backsector != sback) { valid3d = false; break; } // invalid sectors
      // this seems to be a valid line
    }
    if (!sback) valid3d = false;

    if (valid3d) {
      // all lines seems to be valid, check for properly closed contours
      IncrementValidCount();
      for (auto &&ld : explines) {
        if (ld->validcount == validcount) continue; // already processed
        // walk the contour, marking all lines
        line_t *curr = ld;
        do {
          curr->validcount = validcount;
          // find v2 line
          int idx = -1;
          for (int f = 0; f < explines.length(); ++f) {
            line_t *l = explines[f];
            if (l == ld) { idx = f; break; }
            if (l->validcount == validcount) continue; // already used
            if (l->v1 == curr->v2) { idx = f; break; }
          }
          if (idx < 0) {
            // no contour continuation found, invalid pobj
            valid3d = false;
            break;
          }
          curr = explines[idx];
        } while (curr != ld);
        if (!valid3d) break;
      }
      // now check if we have any unmarked lines
      if (valid3d) {
        for (auto &&ld : explines) {
          if (ld->validcount != validcount) {
            // oops
            valid3d = false;
            break;
          }
        }
      }
    }
  } else {
    // not a 3d pobj spawn
    valid3d = false;
  }

  if (thing && thing->type == PO_SPAWN_3D_TYPE && !valid3d) Host_Error("trying to spawn 3d pobj #%d, which is not a valid 3d pobj", po->tag);

  // we know all pobj lines, so store 'em
  // also, set args
  vassert(explines.length());
  po->lines = new line_t*[explines.length()];
  po->numlines = 0;
  for (auto &&ld : explines) {
    if (ld->pobj()) Host_Error("pobj #%d includes a line referenced by another pobj #%d", po->tag, ld->pobj()->tag);
    int linetag = 0;
    if (ld->special == PO_LINE_START) {
      // implicit
      vassert(lstart);
      if (ld == lstart) {
        po->mirror = ld->arg2;
        po->seqType = ld->arg3;
        linetag = ld->arg4;
        ld->special = 0;
        ld->arg1 = 0;
      }
      //if (po->seqType < 0 || po->seqType >= SEQTYPE_NUMSEQ) po->seqType = 0;
    } else if (!lstart) {
      vassert(ld->special == PO_LINE_EXPLICIT && ld->arg1 == tag);
      // explicit
      if (!po->mirror && ld->arg3) po->mirror = ld->arg3;
      if (!po->seqType && ld->arg4) po->seqType = ld->arg4;
      //if (po->seqType < 0 || po->seqType >= SEQTYPE_NUMSEQ) po->seqType = 0;
      linetag = ld->arg5;
      // just in case
      ld->special = 0;
      ld->arg1 = 0;
      ld->arg2 = ld->arg3;
      ld->arg3 = ld->arg4;
      ld->arg4 = ld->arg5;
      ld->arg5 = 0;
    }
    // set line id
    if (linetag) GCon->Logf(NAME_Debug, "pobj #%d line #%d, setting tag %d (%d)", po->tag, (int)(ptrdiff_t)(ld-&Lines[0]), linetag, (int)ld->IsTagEqual(linetag));
    if (linetag && !ld->IsTagEqual(linetag)) {
      if (ld->lineTag == 0) ld->lineTag = linetag; else ld->moreTags.append(linetag);
    }
    ld->pobject = po;
    po->lines[po->numlines++] = ld;
  }
  vassert(po->numlines == explines.length());

  // now we can collect pobj segs
  // for 3d pobjs we will collect minisegs too
  int segcount = 0;
  // collect line segments
  for (auto &&ld : explines) {
    for (seg_t *seg = ld->firstseg; seg; seg = seg->lsnext) {
      if (!seg->pobj) {
        seg->pobj = po;
        ++segcount;
      }
    }
  }
  // collect minisegs for 3d pobjs
  if (valid3d) {
    // back sector is "inner" one
    vassert(sback);
    for (subsector_t *sub = sback->subsectors; sub; sub = sub->seclink) {
      for (int f = 0; f < sub->numlines; ++f) {
        seg_t *ss = &Segs[sub->firstline+f];
        if (!ss->pobj && !ss->linedef) {
          ss->pobj = po;
          ++segcount;
        }
      }
    }
  }
  vassert(segcount);

  // copy segs
  po->segs = new seg_t*[segcount];
  po->numsegs = 0;
  for (auto &&seg : allSegs()) {
    if (seg.pobj == po) po->segs[po->numsegs++] = &seg;
  }
  vassert(po->numsegs == segcount);

  // is this a 3d pobj?
  if (valid3d) {
    // back sector is "inner" one
    vassert(sback);
    po->posector = sback;
    po->posector->linecount = po->posector->prevlinecount; // turn it back to normal sector (it should be normal)
    po->posector->ownpobj = po;
    // set ownpobj for all subsectors
    for (subsector_t *sub = po->posector->subsectors; sub; sub = sub->seclink) {
      vassert(sub->sector == po->posector);
      sub->ownpobj = po;
    }
    // make all segs reference the inner sector
    for (auto &&it : po->SegFirst()) {
      seg_t *seg = it.seg();
      if (seg->frontsector) seg->frontsector = po->posector;
      if (seg->backsector) seg->backsector = po->posector;
    }
    // make all lines reference the inner sector
    for (auto &&it : po->LineFirst()) {
      line_t *ld = it.line();
      ld->frontsector = ld->backsector = po->posector;
    }
    //po->startSpot.z = po->pofloor.minz;
    po->startSpot.z = height; // z offset from destination sector ceiling (used in `TranslatePolyobjToStartSpot()`)
  }

  // find first seg with texture
  const seg_t *xseg = nullptr;
  const sector_t *refsec = nullptr;
  for (int f = 0; f < po->numsegs; ++f) {
    // only front sides
    if (po->segs[f]->linedef && po->segs[f]->side == 0) {
      VTexture *MTex = GTextureManager(po->segs[f]->sidedef->MidTexture);
      if (MTex && MTex->Type != TEXTYPE_Null) {
        xseg = po->segs[f];
        break;
      }
    }
  }
  if (!xseg) GCon->Logf(NAME_Warning, "trying to spawn pobj #%d without any midtextures", po->tag);

  if (valid3d) {
    refsec = po->posector;
    LevelFlags |= LF_Has3DPolyObjects;
  } else {
    if (xseg) refsec = xseg->linedef->frontsector;
    refsec = PointInSubsector(TVec(x, y, 0.0f))->sector;
  }

  if (refsec) {
    // build floor
    po->pofloor = refsec->floor;
    //po->pofloor.pic = 0;
    const float fdist = po->pofloor.GetPointZClamped(*po->lines[0]->v1);
    //GCon->Logf(NAME_Debug, "000: pobj #%d: floor=(%g,%g,%g:%g):%g (%g:%g)", po->tag, po->pofloor.normal.x, po->pofloor.normal.y, po->pofloor.normal.z, po->pofloor.dist, fdist, po->pofloor.minz, po->pofloor.maxz);
    // floor normal points down
    po->pofloor.normal = TVec(0.0f, 0.0f, -1.0f);
    po->pofloor.dist = -fdist;
    //GCon->Logf(NAME_Debug, "001: pobj #%d: floor=(%g,%g,%g:%g):%g", po->tag, po->pofloor.normal.x, po->pofloor.normal.y, po->pofloor.normal.z, po->pofloor.dist, po->pofloor.GetPointZ(TVec(avg.x, avg.y, 0.0f)));
    po->pofloor.minz = po->pofloor.maxz = fdist;

    // build ceiling
    po->poceiling = refsec->ceiling;
    //po->poceiling.pic = 0;
    //if (po->poceiling.isCeiling()) po->poceiling.flipInPlace();
    const float cdist = po->poceiling.GetPointZClamped(*po->lines[0]->v1);
    //GCon->Logf(NAME_Debug, "000: pobj #%d: ceiling=(%g,%g,%g:%g):%g (%g:%g)", po->tag, po->poceiling.normal.x, po->poceiling.normal.y, po->poceiling.normal.z, po->poceiling.dist, cdist, po->poceiling.minz, po->poceiling.maxz);
    // ceiling normal points up
    po->poceiling.normal = TVec(0.0f, 0.0f, 1.0f);
    po->poceiling.dist = cdist;
    //GCon->Logf(NAME_Debug, "001: pobj #%d: ceiling=(%g,%g,%g:%g):%g", po->tag, po->poceiling.normal.x, po->poceiling.normal.y, po->poceiling.normal.z, po->poceiling.dist, po->poceiling.GetPointZ(TVec(avg.x, avg.y, 0.0f)));
    po->poceiling.minz = po->poceiling.maxz = cdist;

    // fix polyobject height

    // determine real height using midtex
    VTexture *MTex = (xseg ? GTextureManager(xseg->sidedef->MidTexture) : nullptr);
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
      if (xseg->sidedef->Mid.RowOffset < 0) {
        z1 += (xseg->sidedef->Mid.RowOffset+texh)*(!MTex->bWorldPanning ? 1.0f : 1.0f/MTex->TScale);
      } else {
        z1 += xseg->sidedef->Mid.RowOffset*(!MTex->bWorldPanning ? 1.0f : 1.0f/MTex->TScale);
      }
      z0 = z1-texh;
      z0 = max2(z0, refsec->floor.minz);
      z1 = min2(z1, refsec->ceiling.maxz);
      if (z1 < z0) z1 = z0;
      // fix floor and ceiling planes
      po->pofloor.minz = po->pofloor.maxz = z0;
      po->pofloor.dist = -po->pofloor.maxz;
      po->poceiling.minz = po->poceiling.maxz = z1;
      po->poceiling.dist = po->poceiling.maxz;
    }
  }

  if (po->posector) {
    // raise it to destination sector height
    refsec = PointInSubsector(TVec(x, y, 0.0f))->sector;
    po->startSpot.z += refsec->floor.GetPointZ(TVec(x, y, 0.0f))-po->pofloor.minz;
  }

  GCon->Logf(NAME_Debug, "SPAWNED %s pobj #%d: floor=(%g,%g,%g:%g) %g:%g; ceiling=(%g,%g,%g:%g) %g:%g; lines=%d; segs=%d; height=%g", (valid3d ? "3D" : "2D"), po->tag,
    po->pofloor.normal.x, po->pofloor.normal.y, po->pofloor.normal.z, po->pofloor.dist, po->pofloor.minz, po->pofloor.maxz,
    po->poceiling.normal.x, po->poceiling.normal.y, po->poceiling.normal.z, po->poceiling.dist, po->poceiling.minz, po->poceiling.maxz,
    po->numlines, po->numsegs, po->poceiling.maxz-po->pofloor.minz);
}


//==========================================================================
//
//  VLevel::Add3DPolyobjLink
//
//==========================================================================
void VLevel::Add3DPolyobjLink (mthing_t *thing, int srcpid, int destpid) {
  if (!srcpid || !destpid) return;
  const int idx = NumPolyLinks3D++;

  VPolyLink3D *nl = new VPolyLink3D[NumPolyLinks3D];
  for (int f = 0; f < NumPolyLinks3D-1; ++f) nl[f] = PolyLinks3D[f];
  delete[] PolyLinks3D;
  PolyLinks3D = nl;

  nl[idx].srcpid = srcpid;
  nl[idx].destpid = destpid;
}


//==========================================================================
//
//  VLevel::AddPolyAnchorPoint
//
//==========================================================================
void VLevel::AddPolyAnchorPoint (mthing_t *thing, float x, float y, float height, int tag) {
  if (/*tag == 0 ||*/ tag == 0x7fffffff) { GCon->Logf(NAME_Error, "ignored anchor point invalid tag: %d", tag); return; }

  ++NumPolyAnchorPoints;

  PolyAnchorPoint_t *Temp = PolyAnchorPoints;
  PolyAnchorPoints = new PolyAnchorPoint_t[NumPolyAnchorPoints];
  if (Temp) {
    for (int i = 0; i < NumPolyAnchorPoints-1; ++i) PolyAnchorPoints[i] = Temp[i];
    delete[] Temp;
    Temp = nullptr;
  }

  PolyAnchorPoint_t *A = &PolyAnchorPoints[NumPolyAnchorPoints-1];
  A->x = x;
  A->y = y;
  A->height = height;
  A->tag = tag;
  A->thingidx = (thing ? (int)(ptrdiff_t)(thing-&Things[0]) : -1);
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
    TranslatePolyobjToStartSpot(&PolyAnchorPoints[i]);
  }

  // check for a startspot without an anchor point
  for (auto &&po : allPolyobjects()) {
    if (!po->originalPts) {
      GCon->Logf(NAME_Warning, "pobj #%d has no anchor point, using centroid", po->tag);
      vassert(po->numlines && po->numsegs);
      float x = 0.0f;
      float y = 0.0f;
      for (auto &&it : po->LineFirst()) {
        x += it.line()->v1->x;
        y += it.line()->v1->y;
      }
      x /= (float)po->numlines;
      y /= (float)po->numlines;

      PolyAnchorPoint_t anchor;
      memset((void *)&anchor, 0, sizeof(anchor));
      anchor.x = x;
      anchor.y = y;
      anchor.height = 0.0f;
      anchor.tag = po->tag;
      anchor.thingidx = -1; // no thing
      TranslatePolyobjToStartSpot(&anchor);
    }
  }

  // now assign polyobject links
  for (int f = 0; f < NumPolyLinks3D; ++f) {
    const int src = PolyLinks3D[f].srcpid;
    const int dest = PolyLinks3D[f].destpid;
    if (!src || !dest) continue;
    if (src == dest) Host_Error("pobj #%d is linked to itself", src);
    polyobj_t *psrc = GetPolyobj(src);
    if (!psrc) Host_Error("there is no pobj #%d for link source", src);
    polyobj_t *pdest = GetPolyobj(dest);
    if (!pdest) Host_Error("there is no pobj #%d for link destination", dest);
    if (psrc == pdest) Host_Error("pobj #%d is linked to itself", src);
    if (psrc->polink) Host_Error("pobj #%d is already linked to pobj #%d", src, psrc->polink->tag);
    psrc->polink = pdest;
  }

  // check for loops
  for (auto &&pofirst : allPolyobjects()) {
    polyobj_t *po0 = pofirst;
    polyobj_t *po1 = pofirst;
    bool dostep = false;
    while (po0) {
      po0 = po0->polink;
      if (dostep) po1 = po1->polink;
      if (po0 && po0 == po1) Host_Error("pobj #%d has link loop", pofirst->tag);
      dostep = !dostep;
    }
  }

  InitPolyBlockMap();
}


//==========================================================================
//
//  VLevel::OffsetPolyobjFlats
//
//==========================================================================
void VLevel::OffsetPolyobjFlats (polyobj_t *po, float z, bool forceRecreation) {
  if (!po) return;

  po->pofloor.dist -= z; // floor dist is negative
  po->pofloor.TexZ += z;
  po->pofloor.minz += z;
  po->pofloor.maxz += z;
  po->pofloor.TexZ = po->pofloor.minz;
  po->poceiling.dist += z; // ceiling dist is positive
  po->poceiling.TexZ += z;
  po->poceiling.minz += z;
  po->poceiling.maxz += z;
  po->poceiling.TexZ = po->poceiling.minz;

  sector_t *sec = po->posector;
  if (!sec) return;

  po->pofloor.PObjCX = po->poceiling.PObjCX = po->startSpot.x;
  po->pofloor.PObjCY = po->poceiling.PObjCY = po->startSpot.y;

  // fix sector
  sec->floor = po->pofloor;
  sec->ceiling = po->poceiling;

  if (forceRecreation || fabsf(z) != 0.0f) {
    for (subsector_t *sub = sec->subsectors; sub; sub = sub->seclink) {
      for (subregion_t *reg = sub->regions; reg; reg = reg->next) {
        reg->ForceRecreation();
      }
    }
  }
}


//==========================================================================
//
//  VLevel::TranslatePolyobjToStartSpot
//
//==========================================================================
void VLevel::TranslatePolyobjToStartSpot (PolyAnchorPoint_t *anchor) {
  vassert(anchor);

  float originX = anchor->x, originY = anchor->y, height = anchor->height;
  int tag = anchor->tag;

  polyobj_t *po = GetPolyobj(tag);

  if (!po) Host_Error("Unable to match polyobj tag: %d", tag); // didn't match the tag with a polyobj tag
  if (po->segs == nullptr) Host_Error("Anchor point located without a StartSpot point: %d", tag);
  vassert(po->numsegs);

  if (po->originalPts) {
    GCon->Logf(NAME_Error, "polyobject with tag #%d is translated to more than one starting point!", po->tag);
    return; // oopsie
  }

  const int maxpts = po->numsegs*2+po->numlines*2;
  po->originalPts = new TVec[maxpts];
  po->segPts = new TVec*[maxpts];

  const float deltaX = originX-po->startSpot.x;
  const float deltaY = originY-po->startSpot.y;
  const TVec sspot2d = TVec(po->startSpot.x, po->startSpot.y, 0.0f);

  //TVec *origPt = po->originalPts;
  int segptnum = 0;

  TMapNC<TVec *, bool> seenVtx;
  // seg points
  for (auto &&it : po->SegFirst()) {
    seg_t *seg = it.seg();
    if (!seenVtx.put(seg->v1, true)) {
      if (segptnum >= maxpts) Sys_Error("too many seg points for pobj #%d", po->tag);
      po->segPts[segptnum++] = seg->v1;
    }
    if (!seenVtx.put(seg->v2, true)) {
      if (segptnum >= maxpts) Sys_Error("too many seg points for pobj #%d", po->tag);
      po->segPts[segptnum++] = seg->v2;
    }
  }
  // line points, just in case
  for (auto &&it : po->LineFirst()) {
    line_t *ld = it.line();
    if (!seenVtx.put(ld->v1, true)) {
      if (segptnum >= maxpts) Sys_Error("too many line points for pobj #%d", po->tag);
      po->segPts[segptnum++] = ld->v1;
    }
    if (!seenVtx.put(ld->v2, true)) {
      if (segptnum >= maxpts) Sys_Error("too many line points for pobj #%d", po->tag);
      po->segPts[segptnum++] = ld->v2;
    }
  }
  vassert(segptnum > 0);
  po->originalPtsCount = segptnum;
  po->segPtsCount = segptnum;
  GCon->Logf(NAME_Debug, "pobj #%d has %d vertices", po->tag, segptnum);

  // save and translate points
  for (int f = 0; f < segptnum; ++f) {
    TVec *v = po->segPts[f];
    v->x -= deltaX;
    v->y -= deltaY;
    po->originalPts[f] = (*v)-sspot2d;
  }

  // allocate temporary storage
  po->prevPts = new TVec[segptnum];
  po->prevPtsCount = segptnum;

  const float startHeight = po->startSpot.z;
  //const sector_t *destsec = PointInSubsector(TVec(originX, originY, 0.0f))->sector;
  po->startSpot.z = 0.0f; // this is actually offset from "default z point" destsec->floor.GetPointZClamped(TVec(originX, originY, 0.0f));

  const mthing_t *thing = (anchor->thingidx >= 0 && anchor->thingidx < NumThings ? &Things[anchor->thingidx] : nullptr);
  //if (!thing && thing->type != PO_SPAWN_3D_TYPE) po->posector = nullptr; // not a 3d polyobject

  float deltaZ = 0.0f;

  // clamp 3d pobj height if necessary
  if (po->posector) {
    // 3d polyobject

    GCon->Logf(NAME_Debug, "000: pobj #%d: floor=(%g,%g,%g:%g) %g:%g; ceiling=(%g,%g,%g:%g) %g:%g  deltaZ=%g", po->tag,
      po->pofloor.normal.x, po->pofloor.normal.y, po->pofloor.normal.z, po->pofloor.dist, po->pofloor.minz, po->pofloor.maxz,
      po->poceiling.normal.x, po->poceiling.normal.y, po->poceiling.normal.z, po->poceiling.dist, po->poceiling.minz, po->poceiling.maxz,
      startHeight);

    // clamp height
    if (thing && thing->height > 0.0f) {
      const float hgt = po->poceiling.maxz-po->pofloor.minz;
      if (hgt > thing->height) {
        GCon->Logf(NAME_Debug, "pobj #%d: height=%g; newheight=%g", po->tag, hgt, thing->height);
        // fix ceiling, move texture z
        const float dz = height-hgt;
        po->poceiling.minz += dz;
        po->poceiling.maxz += dz;
        po->poceiling.dist += dz;
        po->poceiling.TexZ += dz;
      }
    }

    // move vertically
    deltaZ = startHeight;
  }

  GCon->Logf(NAME_Debug, "pobj #%d: height=%g; spot=(%g,%g,%g); dz=%g", po->tag, po->poceiling.maxz-po->pofloor.minz, po->startSpot.x, po->startSpot.y, po->startSpot.z, deltaZ);

  GCon->Logf(NAME_Debug, "001: pobj #%d: floor=(%g,%g,%g:%g) %g:%g; ceiling=(%g,%g,%g:%g) %g:%g", po->tag,
    po->pofloor.normal.x, po->pofloor.normal.y, po->pofloor.normal.z, po->pofloor.dist, po->pofloor.minz, po->pofloor.maxz,
    po->poceiling.normal.x, po->poceiling.normal.y, po->poceiling.normal.z, po->poceiling.dist, po->poceiling.minz, po->poceiling.maxz);

  // offset flat textures
  if (po->posector && (deltaX || deltaY)) {
    if (po->posector->floor.pic != skyflatnum) {
      VTexture *FTex = GTextureManager(po->posector->floor.pic);
      if (FTex && FTex->Type != TEXTYPE_Null) {
        const float w = FTex->GetScaledWidth();
        const float h = FTex->GetScaledHeight();
        if (w > 0.0f) po->pofloor.xoffs += fmodf(po->startSpot.x+deltaX, w);
        if (h > 0.0f) po->pofloor.yoffs -= fmodf(po->startSpot.y+deltaY, h);
      }
    }

    if (po->posector->floor.pic != skyflatnum) {
      VTexture *CTex = GTextureManager(po->posector->ceiling.pic);
      if (CTex && CTex->Type != TEXTYPE_Null) {
        const float w = CTex->GetScaledWidth();
        const float h = CTex->GetScaledHeight();
        //GCon->Logf(NAME_Debug, "*** pobj #%d: x=%g; y=%g; xmod=%g; ymod=%g; spot=(%g, %g) (%g, %g)", po->tag, x, y, fmodf(x, CTex->Width), fmodf(y, CTex->Height), po->startSpot.x, po->startSpot.y, fmodf(po->startSpot.x, CTex->Width), fmodf(po->startSpot.y, CTex->Height));
        if (w > 0.0f) po->poceiling.xoffs += fmodf(po->startSpot.x+deltaX, w);
        if (h > 0.0f) po->poceiling.yoffs -= fmodf(po->startSpot.y+deltaY, h);
      }
    }
  }

  OffsetPolyobjFlats(po, deltaZ, true);

  UpdatePolySegs(po);

  GCon->Logf(NAME_Debug, "translated %s pobj #%d: floor=(%g,%g,%g:%g) %g:%g; ceiling=(%g,%g,%g:%g) %g:%g", (po->posector ? "3d" : "2d"), po->tag,
    po->pofloor.normal.x, po->pofloor.normal.y, po->pofloor.normal.z, po->pofloor.dist, po->pofloor.minz, po->pofloor.maxz,
    po->poceiling.normal.x, po->poceiling.normal.y, po->poceiling.normal.z, po->poceiling.dist, po->poceiling.minz, po->poceiling.maxz);

  // `InitPolyBlockMap()` will call `LinkPolyobj()`, which will calcilate the bounding box
  // no need to notify renderer yet (or update subsector list), `InitPolyBlockMap()` will do it for us
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
  if (po->posector) {
    // update subsector bounding boxes
    for (subsector_t *sub = po->posector->subsectors; sub; sub = sub->seclink) {
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
    // we can use subsector bounding boxes here
    for (const subsector_t *sub = po->posector->subsectors; sub; sub = sub->seclink) {
      pobbox[BOX2D_MINX] = min2(pobbox[BOX2D_MINX], sub->bbox2d[BOX2D_MINX]);
      pobbox[BOX2D_MINY] = min2(pobbox[BOX2D_MINY], sub->bbox2d[BOX2D_MINY]);
      pobbox[BOX2D_MAXX] = max2(pobbox[BOX2D_MAXX], sub->bbox2d[BOX2D_MAXX]);
      pobbox[BOX2D_MAXY] = max2(pobbox[BOX2D_MAXY], sub->bbox2d[BOX2D_MAXY]);
    }
  } else {
    // use polyobject lines
    line_t * const *ld = po->lines;
    for (int f = po->numlines; f--; ++ld) {
      // v1
      pobbox[BOX2D_MINX] = min2(pobbox[BOX2D_MINX], (*ld)->v1->x);
      pobbox[BOX2D_MINY] = min2(pobbox[BOX2D_MINY], (*ld)->v1->y);
      pobbox[BOX2D_MAXX] = max2(pobbox[BOX2D_MAXX], (*ld)->v1->x);
      pobbox[BOX2D_MAXY] = max2(pobbox[BOX2D_MAXY], (*ld)->v1->y);
      // v2
      pobbox[BOX2D_MINX] = min2(pobbox[BOX2D_MINX], (*ld)->v2->x);
      pobbox[BOX2D_MINY] = min2(pobbox[BOX2D_MINY], (*ld)->v2->y);
      pobbox[BOX2D_MAXX] = max2(pobbox[BOX2D_MAXX], (*ld)->v2->x);
      pobbox[BOX2D_MAXY] = max2(pobbox[BOX2D_MAXY], (*ld)->v2->y);
    }
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

  // relink all mobjs this polyobject may touch (if it is 3d pobj)
  if (po->posector) {
    CollectPObjTouchingThingsRough(po);
    // relink all things, so they can get pobj info right
    for (auto &&it : poEntityMap.first()) {
      VEntity *e = it.key();
      //GCon->Logf(NAME_Debug, "pobj #%d: relinking entity %s(%u)", po->tag, e->GetClass()->GetName(), e->GetUniqueId());
      e->LinkToWorld(); // relink it
    }
  }
}


//==========================================================================
//
//  VLevel::UnlinkPolyobj
//
//==========================================================================
void VLevel::UnlinkPolyobj (polyobj_t *po) {
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
//  VLevel::MovePolyobj
//
//  `forced` is used on map loading, and we don't need to follow link chain
//
//==========================================================================
bool VLevel::MovePolyobj (int num, float x, float y, float z, bool forced) {
  polyobj_t *po = GetPolyobj(num);
  if (!po) { GCon->Logf(NAME_Error, "MovePolyobj: Invalid pobj #%d", num); return false; }

  if (!po->posector) z = 0.0f;

  polyobj_t *pofirst = po;

  const bool docarry = (pofirst->posector && !(pofirst->PolyFlags&polyobj_t::PF_NoCarry));
  bool flatsSaved = false;

  // collect `poAffectedEnitities`, and save planes
  // but do this only if we have to
  // only for 3d pobjs that either can carry objects, or moving vertically
  poAffectedEnitities.resetNoDtor();
  if (!forced && pofirst->posector && (docarry || z != 0.0f)) {
    flatsSaved = true;
    IncrementValidCount();
    const int visCount = validcount;
    //GCon->Logf(NAME_Debug, "=== pobj #%d ===", pofirst->tag);
    for (po = pofirst; po; po = po->polink) {
      po->savedFloor = po->pofloor;
      po->savedCeiling = po->poceiling;
      for (msecnode_t *n = po->posector->TouchingThingList; n; n = n->SNext) {
        VEntity *mobj = n->Thing;
        if (mobj->ValidCount == visCount) continue;
        mobj->ValidCount = visCount;
        if (mobj->IsGoingToDie()) continue;
        //GCon->Logf(NAME_Debug, "  %s(%u): z=%g (poz1=%g); sector=%p; basesector=%p", mobj->GetClass()->GetName(), mobj->GetUniqueId(), mobj->Origin.z, pofirst->poceiling.maxz, mobj->Sector, mobj->BaseSector);
        SavedEntityData &edata = poAffectedEnitities.alloc();
        edata.mobj = mobj;
        edata.Origin = mobj->Origin;
      }
    }
  } else if (forced && pofirst->posector) {
    IncrementValidCount();
    const int visCount = validcount;
    for (po = pofirst; po; po = po->polink) {
      for (msecnode_t *n = po->posector->TouchingThingList; n; n = n->SNext) {
        VEntity *mobj = n->Thing;
        if (mobj->ValidCount == visCount) continue;
        mobj->ValidCount = visCount;
        if (mobj->IsGoingToDie()) continue;
        SavedEntityData &edata = poAffectedEnitities.alloc();
        edata.mobj = mobj;
        edata.Origin = mobj->Origin;
      }
    }
  }

  // vertical movement
  // note that there is no need to relink pobj, because blockmap position will stay the same
  if (pofirst->posector && z != 0.0f) {
    // move mobjs standing on this pobj up/down with it
    if (!forced) {
      for (auto &&edata : poAffectedEnitities) {
        VEntity *mobj = edata.mobj;
        const float oldz = mobj->Origin.z;
        const bool moveup = (mobj->Velocity.z > 0.0f);
        for (po = pofirst; po; po = po->polink) {
          const float pofz = po->poceiling.maxz;
          if ((docarry && !moveup && oldz == pofz) || // standing on a platform -- should go down with it
              (oldz >= pofz && oldz < pofz+z)) // platform moving up, and goes through the mobj
          {
            // force onto platform
            mobj->Origin.z = pofz+z;
            //GCon->Logf(NAME_Debug, "pobj #%d: %s(%u):   MOVED! z=%g; FloorZ=%g; CeilingZ=%g; pofz=%g; newpofz=%g; zofs=%g", po->tag, mobj->GetClass()->GetName(), mobj->GetUniqueId(), mobj->Origin.z, mobj->FloorZ, mobj->CeilingZ, pofz, pofz+z, z);
          }
        }
      }
    }
    // move platform vertically
    for (po = pofirst; po; po = po->polink) {
      OffsetPolyobjFlats(po, z);
      if (forced) break;
    }
    // check if not blocked
    if (!forced) {
      // check movement
      //FIXME: push stacked mobjs (one atop of another) up
      bool canMove = true;
      tmtrace_t tmtrace;
      for (auto &&edata : poAffectedEnitities) {
        VEntity *mobj = edata.mobj;
        //if (!mobj->IsSolid() || !(mobj->EntityFlags&VEntity::EF_ColideWithWorld)) continue;
        const vuint32 oldFlags = mobj->EntityFlags;
        mobj->EntityFlags &= ~VEntity::EF_ColideWithThings;
        const bool ok = mobj->CheckRelPosition(tmtrace, mobj->Origin, /*noPickups*/true, /*ignoreMonsters*/true, /*ignorePlayers*/true);
        if (!ok) {
          canMove = false;
          mobj->EntityFlags = oldFlags;
          break;
        } else {
          // fix z if necessary
          if (mobj->Origin.z < tmtrace.FloorZ) {
            // hit something
            mobj->Origin.z = tmtrace.FloorZ;
          }
        }
        mobj->EntityFlags = oldFlags;
      }
      // blocked?
      if (!canMove) {
        vassert(flatsSaved);
        // restore mobjs and platforms positions
        //GCon->Logf(NAME_Debug, "pobj #%d: BLOCKED!", po->tag);
        for (po = pofirst; po; po = po->polink) {
          po->pofloor = po->savedFloor;
          po->poceiling = po->savedCeiling;
          OffsetPolyobjFlats(po, 0.0f, true);
        }
        // restore `z` of force-modified entities, otherwise they may fall through the platform
        for (auto &&edata : poAffectedEnitities) {
          VEntity *e = edata.mobj;
          if (!e->IsGoingToDie()) {
            e->Origin.z = edata.Origin.z;
            e->LinkToWorld(); // relink it, just in case
          }
        }
        return false;
      }
      // relink, and crash corpses
      VName ctp("PolyObjectGibs");
      for (auto &&edata : poAffectedEnitities) {
        VEntity *mobj = edata.mobj;
        if (mobj->IsGoingToDie()) continue;
        mobj->LinkToWorld(); // relink it
        mobj->LastMoveOrigin.z += mobj->Origin.z-edata.Origin.z;
        if (mobj->IsRealCorpse()) {
          if (mobj->Origin.z+max2(0.0f, mobj->Height) > mobj->CeilingZ) mobj->GibMe(ctp);
        }
      }
    }
  }

  bool unlinked;

  // horizontal movement
  if (forced || x || y) {
    unlinked = true;

    // unlink and move all linked polyobjects
    for (po = pofirst; po; po = po->polink) {
      UnlinkPolyobj(po);
      const TVec delta(po->startSpot.x+x, po->startSpot.y+y, 0.0f);
      // save previous points, move horizontally
      const TVec *origPts = po->originalPts;
      TVec *prevPts = po->prevPts;
      TVec **vptr = po->segPts;
      for (int f = po->segPtsCount; f--; ++vptr, ++origPts, ++prevPts) {
        *prevPts = **vptr;
        **vptr = (*origPts)+delta;
      }
      UpdatePolySegs(po);
      if (forced) break;
    }

    // check for blocked movement
    bool blocked = false;

    if (!forced) {
      for (po = pofirst; po; po = po->polink) {
        blocked = PolyCheckMobjBlocked(po);
        if (blocked) break; // process all?
      }
    }

    // restore position if blocked
    if (blocked) {
      // restore object z positions; there is no need to relink them, because pobj relinking will do it
      if (pofirst->posector) {
        for (auto &&edata : poAffectedEnitities) {
          VEntity *mobj = edata.mobj;
          if (!mobj->IsGoingToDie()) {
            mobj->LastMoveOrigin.z -= mobj->Origin.z-edata.Origin.z;
            mobj->Origin.z = edata.Origin.z;
            // and relink it
            //mobj->LinkToWorld();
          }
        }
      }
      // undo polyobject movement, and link them back
      for (po = pofirst; po; po = po->polink) {
        if (flatsSaved) {
          po->pofloor = po->savedFloor;
          po->poceiling = po->savedCeiling;
        }
        // restore points
        TVec *prevPts = po->prevPts;
        TVec **vptr = po->segPts;
        for (int f = po->segPtsCount; f--; ++vptr, ++prevPts) **vptr = *prevPts;
        UpdatePolySegs(po);
        OffsetPolyobjFlats(po, 0.0f, true);
        LinkPolyobj(po);
      }
      // relink all mobjs (just in case)
      for (auto &&edata : poAffectedEnitities) {
        VEntity *mobj = edata.mobj;
        if (!mobj->IsGoingToDie()) mobj->LinkToWorld();
      }
      return false;
    }
  } else {
    unlinked = false;
    for (po = pofirst; po; po = po->polink) {
      UpdatePolySegs(po);
      if (forced) break;
    }
  }

  for (po = pofirst; po; po = po->polink) {
    po->startSpot.x += x;
    po->startSpot.y += y;
    po->startSpot.z += z;
    OffsetPolyobjFlats(po, 0.0f, true);
    if (unlinked) LinkPolyobj(po);
    if (forced) break;
  }

  // carry things?
  if (!forced && docarry && pofirst->posector && (x || y)) {
    //for (po = pofirst; po; po = po->polink) poflags |= po->PolyFlags;
    //GCon->Logf(NAME_Debug, "=== pobj #%d ===", pofirst->tag);
    for (auto &&edata : poAffectedEnitities) {
      VEntity *e = edata.mobj;
      if (e->IsGoingToDie()) continue;
      //GCon->Logf(NAME_Debug, "  moving entity %s(%u)", e->GetClass()->GetName(), e->GetUniqueId());
      //FIXME: make this faster!
      bool needmove = false;
      vuint32 poflags = 0;
      for (po = pofirst; po; po = po->polink) {
        //GCon->Logf(NAME_Debug, "    pobj #%d: %s(%u): z=%g (poz1=%g); sector=%p; basesector=%p (onit=%d : %g : %f : %a) fz=%g", po->tag, e->GetClass()->GetName(), e->GetUniqueId(), e->Origin.z, po->poceiling.maxz, e->Sector, e->BaseSector, (int)(e->Origin.z == po->poceiling.maxz), (e->Origin.z-po->poceiling.maxz), (e->Origin.z-po->poceiling.maxz), (e->Origin.z-po->poceiling.maxz), e->FloorZ);
        if (e->Sector == po->posector || e->Origin.z == po->poceiling.maxz) {
          needmove = true;
          poflags = po->PolyFlags;
          break;
        }
      }
      if (needmove) {
        //GCon->Logf(NAME_Debug, "      moving entity %s(%u) by (%g,%g)", e->GetClass()->GetName(), e->GetUniqueId(), x, y);
        e->Level->eventPolyMoveMobjBy(e, poflags, x, y);
      }
    }
  } else if (forced) {
    // relink all mobjs (just in case)
    //GCon->Logf(NAME_Debug, "=== pobj #%d ===", pofirst->tag);
    for (auto &&edata : poAffectedEnitities) {
      VEntity *mobj = edata.mobj;
      if (!mobj->IsGoingToDie()) {
        //GCon->Logf(NAME_Debug, "  fixing entity %s(%u)", mobj->GetClass()->GetName(), mobj->GetUniqueId());
        mobj->LinkToWorld();
      }
    }
  }

  // notify renderer that this polyobject is moved
  if (Renderer) {
    for (po = pofirst; po; po = po->polink) {
      Renderer->PObjModified(po);
      if (forced) break;
    }
  }

  return true;
}


//==========================================================================
//
//  VLevel::RotatePolyobj
//
//  `forced` is used on map loading, and we don't need to follow link chain
//
//==========================================================================
bool VLevel::RotatePolyobj (int num, float angle, bool forced) {
  // get the polyobject
  polyobj_t *po = GetPolyobj(num);
  if (!po) { GCon->Logf(NAME_Error, "RotatePolyobj: Invalid pobj #%d", num); return false; }

  polyobj_t *pofirst = po;

  const bool docarry = (pofirst->posector && !(po->PolyFlags&polyobj_t::PF_NoCarry));
  const bool doangle = (pofirst->posector && !(po->PolyFlags&polyobj_t::PF_NoAngleChange));

  // calculate the angle
  const float an = AngleMod(po->angle+angle);
  float msinAn, mcosAn;
  msincos(an, &msinAn, &mcosAn);

  poAffectedEnitities.resetNoDtor();

  const bool flatsSaved = (pofirst->posector);

  // collect objects we need to move/rotate
  if (!forced && pofirst->posector && (docarry || doangle)) {
    IncrementValidCount();
    const int visCount = validcount;
    for (po = pofirst; po; po = po->polink) {
      const float pz1 = po->poceiling.maxz;
      for (msecnode_t *n = po->posector->TouchingThingList; n; n = n->SNext) {
        if (n->Visited == visCount) continue;
        // unprocessed thing found, mark thing as processed
        n->Visited = visCount;
        VEntity *mobj = n->Thing;
        if (mobj->ValidCount == visCount) continue;
        mobj->ValidCount = visCount;
        if (mobj->IsGoingToDie()) continue;
        if (mobj->Origin.z != pz1) continue; // just in case
        if (!mobj->Sector->isInnerPObj()) continue; // this should be properly set in `LinkToWorld()`
        SavedEntityData &edata = poAffectedEnitities.alloc();
        edata.mobj = mobj;
        edata.Origin = mobj->Origin;
        // need to save it, because entity relinking may change/reset it
        edata.po = mobj->Sector->ownpobj;
      }
    }
  }

  // rotate all polyobjects
  for (po = pofirst; po; po = po->polink) {
    /*if (IsForServer())*/ UnlinkPolyobj(po);
    const float ssx = po->startSpot.x;
    const float ssy = po->startSpot.y;
    const TVec *origPts = po->originalPts;
    TVec *prevPts = po->prevPts;
    TVec **vptr = po->segPts;
    for (int f = po->segPtsCount; f--; ++vptr, ++prevPts, ++origPts) {
      if (flatsSaved) {
        po->savedFloor = po->pofloor;
        po->savedCeiling = po->poceiling;
      }
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
    if (forced) break;
  }

  bool blocked = false;

  if (!forced) {
    for (po = pofirst; po; po = po->polink) {
      blocked = PolyCheckMobjBlocked(po);
      if (blocked) break; // process all?
    }
  }

  // if we are blocked then restore the previous points
  if (blocked) {
    // restore points
    for (po = pofirst; po; po = po->polink) {
      if (flatsSaved) {
        po->pofloor = po->savedFloor;
        po->poceiling = po->savedCeiling;
      }
      TVec *prevPts = po->prevPts;
      TVec **vptr = po->segPts;
      for (int f = po->segPtsCount; f--; ++vptr, ++prevPts) **vptr = *prevPts;
      UpdatePolySegs(po);
      LinkPolyobj(po);
    }
    return false;
  }

  for (po = pofirst; po; po = po->polink) {
    po->angle = an;
    if (flatsSaved) {
      po->pofloor.BaseAngle = AngleMod(po->pofloor.BaseAngle+angle);
      po->poceiling.BaseAngle = AngleMod(po->poceiling.BaseAngle+angle);
      OffsetPolyobjFlats(po, 0.0f, true);
    }
    LinkPolyobj(po);
    if (forced) break;
  }

  // move and rotate things
  if (!forced && flatsSaved && (docarry || doangle) && poAffectedEnitities.length()) {
    msincos(AngleMod(angle), &msinAn, &mcosAn);
    for (auto &&edata : poAffectedEnitities) {
      VEntity *e = edata.mobj;
      if (e->IsGoingToDie()) continue;
      if (docarry) {
        const float ssx = edata.po->startSpot.x;
        const float ssy = edata.po->startSpot.y;
        // rotate around polyobject spot point
        const float xc = e->Origin.x-ssx;
        const float yc = e->Origin.y-ssy;
        //GCon->Logf(NAME_Debug, "%s(%u): oldrelpos=(%g,%g)", e->GetClass()->GetName(), e->GetUniqueId(), xc, yc);
        // calculate the new X and Y values
        const float nx = (xc*mcosAn-yc*msinAn);
        const float ny = (yc*mcosAn+xc*msinAn);
        //GCon->Logf(NAME_Debug, "%s(%u): newrelpos=(%g,%g)", e->GetClass()->GetName(), e->GetUniqueId(), nx, ny);
        e->Level->eventPolyMoveMobjBy(e, edata.po->PolyFlags, (nx+ssx)-e->Origin.x, (ny+ssy)-e->Origin.y); // anyway
      }
      if (doangle && angle && !e->IsGoingToDie()) e->Level->eventPolyRotateMobj(e, edata.po->PolyFlags, angle);
    }
  }

  // notify renderer that this polyobject is moved
  if (Renderer) {
    for (po = pofirst; po; po = po->polink) {
      Renderer->PObjModified(po);
      if (forced) break;
    }
  }

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
        if (!(mobj->EntityFlags&VEntity::EF_ColideWithWorld)) continue;
        if (!(mobj->EntityFlags&(VEntity::EF_Solid|VEntity::EF_Corpse))) continue;

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
        //FIXME: check height for 3dmitex pobj
        if (po->posector) {
          if (mobj->Origin.z >= po->poceiling.maxz || mobj->Origin.z+max2(0.0f, mobj->Height) <= po->pofloor.minz) continue;
        }

        //TODO: crush corpses!

        if (!mobj->IsBlockingLine(ld)) continue;

        if (P_BoxOnLineSide(tmbbox, ld) != -1) continue;

        //mobj->PolyObjIgnore = po;
        if (mobj->EntityFlags&VEntity::EF_Solid) {
          //GCon->Logf(NAME_Debug, "pobj #%d hit %s(%u)", po->tag, mobj->GetClass()->GetName(), mobj->GetUniqueId());
          mobj->Level->eventPolyThrustMobj(mobj, ld->normal, po);
          blocked = true;
        } else {
          mobj->Level->eventPolyCrushMobj(mobj, po);
        }
        //mobj->PolyObjIgnore = nullptr;
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
//native final void SpawnPolyobj (mthing_t *thing, float x, float y, float height, int tag, bool crush, bool hurt);
IMPLEMENT_FUNCTION(VLevel, SpawnPolyobj) {
  mthing_t *thing;
  float x, y, height;
  int tag;
  bool crush, hurt;
  vobjGetParamSelf(thing, x, y, height, tag, crush, hurt);
  Self->SpawnPolyobj(thing, x, y, height, tag, crush, hurt);
}

//native final void AddPolyAnchorPoint (mthing_t *thing, float x, float y, float height, int tag);
IMPLEMENT_FUNCTION(VLevel, AddPolyAnchorPoint) {
  mthing_t *thing;
  float x, y, height;
  int tag;
  vobjGetParamSelf(thing, x, y, height, tag);
  Self->AddPolyAnchorPoint(thing, x, y, height, tag);
}

//native final void Add3DPolyobjLink (mthing_t *thing, int srcpid, int destpid);
IMPLEMENT_FUNCTION(VLevel, Add3DPolyobjLink) {
  mthing_t *thing;
  int srcpid, destpid;
  vobjGetParamSelf(thing, srcpid, destpid);
  Self->Add3DPolyobjLink(thing, srcpid, destpid);
}

//native final polyobj_t *GetPolyobj (int polyNum);
IMPLEMENT_FUNCTION(VLevel, GetPolyobj) {
  int tag;
  vobjGetParamSelf(tag);
  RET_PTR(Self->GetPolyobj(tag));
}

//native final int GetPolyobjMirror (int poly);
IMPLEMENT_FUNCTION(VLevel, GetPolyobjMirror) {
  int tag;
  vobjGetParamSelf(tag);
  RET_INT(Self->GetPolyobjMirror(tag));
}

//native final bool MovePolyobj (int num, float x, float y, optional float z, optional bool forced);
IMPLEMENT_FUNCTION(VLevel, MovePolyobj) {
  int tag;
  float x, y;
  VOptParamFloat z(0);
  VOptParamBool forced(false);
  vobjGetParamSelf(tag, x, y, z, forced);
  RET_BOOL(Self->MovePolyobj(tag, x, y, z, forced));
}

//native final bool RotatePolyobj (int num, float angle);
IMPLEMENT_FUNCTION(VLevel, RotatePolyobj) {
  int tag;
  float angle;
  vobjGetParamSelf(tag, angle);
  RET_BOOL(Self->RotatePolyobj(tag, angle));
}
