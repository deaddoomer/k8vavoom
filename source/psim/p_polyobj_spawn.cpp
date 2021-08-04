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
#ifdef CLIENT
# include "../drawer.h"
#endif


static VCvarB dbg_pobj_verbose_spawn("dbg_pobj_verbose_spawn", false, "Verbose polyobject spawner?", CVAR_PreInit);


// polyobj line start special
#define PO_LINE_START     (1)
#define PO_LINE_EXPLICIT  (5)


enum {
  // this thing's height is z offset from the *inner* sector floor
  PO_SPAWN_3D_TYPE = 9369,
};


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
  // some idiots are using tag 0 for polyobject
  if (/*tag == 0 ||*/ tag == 0x7fffffff) Host_Error("the map tried to spawn polyobject with invalid tag: %d", tag);
  if (tag == 0) GCon->Log(NAME_Error, "using tag 0 for polyobject is THE VIOLATION OF THE SPECS. please, don't release broken maps.");

  const bool want3DPobj = (thing && thing->type == PO_SPAWN_3D_TYPE);

  const int index = NumPolyObjs++;

  if (dbg_pobj_verbose_spawn.asBool()) GCon->Logf(NAME_Debug, "SpawnPolyobj: tag=%d; idx=%d; thing=%d", tag, index, (thing ? (int)(ptrdiff_t)(thing-&Things[0]) : -1));

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

  // arg1 is flags
  if (want3DPobj) {
    if (thing->args[0]&polyobj_t::PF_Crush) po->PolyFlags |= polyobj_t::PF_Crush; else po->PolyFlags &= ~polyobj_t::PF_Crush;
    if (thing->args[0]&polyobj_t::PF_HurtOnTouch) po->PolyFlags |= polyobj_t::PF_HurtOnTouch; else po->PolyFlags &= ~polyobj_t::PF_HurtOnTouch;
    if (thing->args[0]&polyobj_t::PF_NoCarry) po->PolyFlags |= polyobj_t::PF_NoCarry; else po->PolyFlags &= ~polyobj_t::PF_NoCarry;
    if (thing->args[0]&polyobj_t::PF_NoAngleChange) po->PolyFlags |= polyobj_t::PF_NoAngleChange; else po->PolyFlags &= ~polyobj_t::PF_NoAngleChange;
    if (thing->args[0]&polyobj_t::PF_SideCrush) po->PolyFlags |= polyobj_t::PF_SideCrush; else po->PolyFlags &= ~polyobj_t::PF_SideCrush;
    // zero tolerance to errors!
    for (int f = 1; f < 5; ++f) {
      if (thing->args[f] != 0) {
        Host_Error("SpawnPolyobj: spawner for 3d polyobject #%d has non-zero argument #%d", tag, f+1);
      }
    }
  }

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
        const char *msg = va("ignored extra `Polyobj_StartLine` special for polyobject with tag #%d at line #%d (the first is at line #%d)", tag, (int)(ptrdiff_t)(line-&Lines[0]), (int)(ptrdiff_t)(lstart-&Lines[0]));
        if (want3DPobj) Host_Error("%s", msg);
        GCon->Log(NAME_Error, msg);
        continue;
      }
      if (explines.length()) {
        const char *msg = va("ignored `Polyobj_StartLine` special for polyobject with tag #%d at line #%d (due to previous `Polyobj_ExplicitLine` special)", tag, (int)(ptrdiff_t)(line-&Lines[0]));
        if (want3DPobj) Host_Error("%s", msg);
        GCon->Log(NAME_Error, msg);
        continue;
      }
      lstart = line;
    } else if (line->special == PO_LINE_EXPLICIT && line->arg1 == tag) {
      if (lstart) {
        const char *msg = va("ignored `Polyobj_ExplicitLine` special for polyobject with tag #%d at line #%d (due to previous `Polyobj_StartLine` special)", tag, (int)(ptrdiff_t)(lstart-&Lines[0]));
        if (want3DPobj) Host_Error("%s", msg);
        GCon->Log(NAME_Error, msg);
        continue;
      }
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
    bool goodOrientation = true;
    int vxidx = 1;
    if (!want3DPobj && ld->vxCount(1)) {
      line_t *tl = ld->vxLine(1, 0);
      if (tl->vxCount(1) && tl->vxLine(1, 0) == ld) {
        GCon->Logf(NAME_Error, "fucked up polyobject with tag #%d (line #%d has wrong orientation)", tag, (int)(ptrdiff_t)(ld-&Lines[0]));
        vxidx = 0;
        goodOrientation = false;
        //HACK, wrong, but meh
        if (ld->backsector) {
          sector_t *tsec = ld->frontsector;
          ld->frontsector = ld->backsector;
          ld->backsector = tsec;
        }
      }
    }
    for (;;) {
      //GCon->Logf(NAME_Debug, "pobj #%d line #%d; vxidx=%d", tag, (int)(ptrdiff_t)(ld-&Lines[0]), vxidx);
      // take first line and go
      if (ld->vxCount(vxidx) == 0) Host_Error("missing line for implicit polyobject with tag #%d (at line #%d)", tag, (int)(ptrdiff_t)(ld-&Lines[0]));
      ld = ld->vxLine(vxidx, 0);
      if (ld->validcount == validcount) Host_Error("found loop in implicit polyobject with tag #%d (at line #%d)", tag, (int)(ptrdiff_t)(ld-&Lines[0]));
      ld->validcount = validcount;
      explines.append(ld);
      if (goodOrientation) {
        if (ld->v2 == lstart->v1) break;
      } else {
        if (ld->v2 == lstart->v2) break;
      }
      vxidx = 1;
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
    if (linetag && dbg_pobj_verbose_spawn.asBool()) {
      GCon->Logf(NAME_Debug, "pobj #%d line #%d, setting tag %d (%d)", po->tag, (int)(ptrdiff_t)(ld-&Lines[0]), linetag, (int)ld->IsTagEqual(linetag));
    }
    if (linetag && !ld->IsTagEqual(linetag)) {
      if (ld->lineTag == 0) ld->lineTag = linetag; else ld->moreTags.append(linetag);
    }
    ld->pobject = po;
    po->lines[po->numlines++] = ld;
  }
  vassert(po->numlines == explines.length());

  // now check if this is a valid 3d pobj
  bool valid3d = (want3DPobj ? IsGood3DPolyobj(po) : false);
  if (want3DPobj && !valid3d) Host_Error("trying to spawn 3d pobj #%d, which is not a valid 3d pobj", po->tag);

  if (valid3d) Validate3DPolyobj(po); else ValidateNormalPolyobj(po);

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

  sector_t *secinner = (valid3d ? po->lines[0]->backsector : nullptr);

  // collect minisegs for 3d pobjs
  if (valid3d) {
    // back sector must be the "inner" one
    vassert(secinner);
    for (subsector_t *sub = secinner->subsectors; sub; sub = sub->seclink) {
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
    vassert(secinner);
    po->posector = secinner;
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
    po->startSpot.z = height; // z offset from the inner sector floor (used in `TranslatePolyobjToStartSpot()`)
  }

  // find first seg with texture
  const sector_t *refsec = nullptr;
  /*
  const seg_t *xseg = nullptr;
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
  */

  if (valid3d) {
    refsec = po->posector;
    LevelFlags |= LF_Has3DPolyObjects;
  } else {
    //if (xseg) refsec = xseg->linedef->frontsector;
    po->refsector = PointInSubsector(TVec(x, y, 0.0f))->sector; // so we'll be able to fix polyobject height
    refsec = po->refsector;
    po->refsectorOldFloorHeight = refsec->floor.GetRealDist();
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
    po->pofloor.minz = po->pofloor.maxz = po->pofloor.TexZ = fdist;

    // build ceiling
    po->poceiling = refsec->ceiling;
    //po->poceiling.pic = 0;
    //if (po->poceiling.isCeiling()) po->poceiling.FlipInPlace();
    const float cdist = po->poceiling.GetPointZClamped(*po->lines[0]->v1);
    //GCon->Logf(NAME_Debug, "000: pobj #%d: ceiling=(%g,%g,%g:%g):%g (%g:%g)", po->tag, po->poceiling.normal.x, po->poceiling.normal.y, po->poceiling.normal.z, po->poceiling.dist, cdist, po->poceiling.minz, po->poceiling.maxz);
    // ceiling normal points up
    po->poceiling.normal = TVec(0.0f, 0.0f, 1.0f);
    po->poceiling.dist = cdist;
    //GCon->Logf(NAME_Debug, "001: pobj #%d: ceiling=(%g,%g,%g:%g):%g", po->tag, po->poceiling.normal.x, po->poceiling.normal.y, po->poceiling.normal.z, po->poceiling.dist, po->poceiling.GetPointZ(TVec(avg.x, avg.y, 0.0f)));
    po->poceiling.minz = po->poceiling.maxz = po->poceiling.TexZ = cdist;

    // fix 3d polyobject height
    if (valid3d) {
      // determine real height using midtex
      // use first polyobject line texture
      const line_t *ld = po->lines[0];
      vassert(ld->sidenum[0] >= 0);
      const side_t *sidedef = &Sides[ld->sidenum[0]];
      // clamp with texture height if it is not wrapped
      if (((ld->flags&ML_WRAP_MIDTEX)|(sidedef->Flags&SDF_WRAPMIDTEX)) == 0) {
        VTexture *MTex = GTextureManager(sidedef->MidTexture);
        vassert(MTex && MTex->Type != TEXTYPE_Null);
        const float texh = MTex->GetScaledHeightF()/sidedef->Mid.ScaleY;
        vassert(isFiniteF(texh) && texh >= 1.0f);
        float z0, z1;
        if (ld->flags&ML_DONTPEGBOTTOM) {
          // bottom of texture at bottom
          z1 = po->pofloor.TexZ+texh;
        } else {
          // top of texture at top
          z1 = po->poceiling.TexZ;
        }
        z0 = z1-texh;
        // clamp
        z0 = clampval(z0, po->pofloor.TexZ, po->poceiling.TexZ);
        z1 = clampval(z1, po->pofloor.TexZ, po->poceiling.TexZ);
        if (z0 > z1) Host_Error("invalid 3d pobj #%d height (by midtex)", po->tag);
        // fix floor and ceiling planes
        po->pofloor.minz = po->pofloor.maxz = po->pofloor.TexZ = z0;
        po->pofloor.dist = -po->pofloor.maxz;
        po->poceiling.minz = po->poceiling.maxz = po->poceiling.TexZ = z1;
        po->poceiling.dist = po->poceiling.maxz;
      }
      // and check floor/ceiling textures too (i should find a better place for this!)
      VTexture *FTex = GTextureManager(refsec->floor.pic);
      if (!FTex || FTex->Type == TEXTYPE_Null || FTex->isSeeThrough()) Host_Error("3d pobj #%d has invalid floor texture", po->tag);
      VTexture *CTex = GTextureManager(refsec->ceiling.pic);
      if (!CTex || CTex->Type == TEXTYPE_Null || CTex->isSeeThrough()) Host_Error("3d pobj #%d has invalid ceiling texture", po->tag);
    } else {
      // for normal polyobjects, the texture is simply wrapped
    }
  }

  if (dbg_pobj_verbose_spawn.asBool()) {
    GCon->Logf(NAME_Debug, "SPAWNED %s pobj #%d: floor=(%g,%g,%g:%g) %g:%g; ceiling=(%g,%g,%g:%g) %g:%g; lines=%d; segs=%d; height=%g; refsec=%d", (valid3d ? "3D" : "2D"), po->tag,
      po->pofloor.normal.x, po->pofloor.normal.y, po->pofloor.normal.z, po->pofloor.dist, po->pofloor.minz, po->pofloor.maxz,
      po->poceiling.normal.x, po->poceiling.normal.y, po->poceiling.normal.z, po->poceiling.dist, po->poceiling.minz, po->poceiling.maxz,
      po->numlines, po->numsegs, po->poceiling.maxz-po->pofloor.minz,
      (refsec ? (int)(ptrdiff_t)(refsec-&Sectors[0]) : -1));
  }
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
      TVec cp;
      if (po->posector) {
        cp = CalcPolyobjCenter2D(po);
        //if (cp.isZero2D()) Host_Error("cannot calculate centroid for pobj #%d", po->tag);
      } else {
        vassert(po->numlines && po->numsegs);
        #if 0
        cp = TVec(0.0f, 0.0f);
        for (auto &&it : po->LineFirst()) {
          cp.x += it.line()->v1->x;
          cp.y += it.line()->v1->y;
        }
        cp /= (float)po->numlines;
        #else
        cp = CalcPolyobjCenter2D(po);
        #endif
      }

      PolyAnchorPoint_t anchor;
      memset((void *)&anchor, 0, sizeof(anchor));
      anchor.x = cp.x;
      anchor.y = cp.y;
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
    const bool is3d = pofirst->posector;
    while (po0) {
      po0 = po0->polink;
      if (dostep) po1 = po1->polink;
      if (po0) {
        if (po0 == po1) Host_Error("pobj #%d chain has link loop", pofirst->tag);
        if (!!(po0->posector) != is3d) Host_Error("do not link 3d and non-3d polyobjects (chain starts at pobj #%d, pobj #%d broke the rule)", pofirst->tag, po0->tag);
      }
      dostep = !dostep;
    }
  }

  InitPolyBlockMap();
}


//==========================================================================
//
//  VLevel::TranslatePolyobjToStartSpot
//
//==========================================================================
void VLevel::TranslatePolyobjToStartSpot (PolyAnchorPoint_t *anchor) {
  vassert(anchor);

  float originX = anchor->x, originY = anchor->y;
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
  if (dbg_pobj_verbose_spawn.asBool()) GCon->Logf(NAME_Debug, "pobj #%d has %d vertices", po->tag, segptnum);

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

  const float startHeight = po->startSpot.z; // this is set to anchor height in spawner
  po->startSpot.z = 0.0f; // this is actually offset from "default z point"

  const mthing_t *anchorthing = (anchor->thingidx >= 0 && anchor->thingidx < NumThings ? &Things[anchor->thingidx] : nullptr);

  float deltaZ = 0.0f;

  // clamp 3d pobj height if necessary
  if (po->posector) {
    // 3d polyobject

    if (dbg_pobj_verbose_spawn.asBool()) {
      GCon->Logf(NAME_Debug, "000: pobj #%d: floor=(%g,%g,%g:%g) %g:%g; ceiling=(%g,%g,%g:%g) %g:%g  deltaZ=%g", po->tag,
        po->pofloor.normal.x, po->pofloor.normal.y, po->pofloor.normal.z, po->pofloor.dist, po->pofloor.minz, po->pofloor.maxz,
        po->poceiling.normal.x, po->poceiling.normal.y, po->poceiling.normal.z, po->poceiling.dist, po->poceiling.minz, po->poceiling.maxz,
        startHeight);
    }

    // move vertically
    deltaZ = startHeight;

    #if 0
    // clamp height
    // nope, not this
    if (anchorthing && anchorthing->height != 0.0f) {
      const float hgt = po->poceiling.maxz-po->pofloor.minz;
      const bool fromCeiling = (anchorthing->height < 0.0f);
      const float newhgt = fabsf(anchorthing->height);
      if (hgt > newhgt) {
        // "don't peg top" flag is abused to limit the height from the ceiling
        if (fromCeiling) {
          // height is from the ceiling
          if (dbg_pobj_verbose_spawn.asBool()) GCon->Logf(NAME_Debug, "pobj #%d: height=%g; newheight=%g (from the ceiling)", po->tag, hgt, newhgt);
          // fix floor (move it up), move texture z
          const float dz = hgt-newhgt;
          po->pofloor.minz += dz;
          po->pofloor.maxz += dz;
          po->pofloor.dist += dz;
          po->pofloor.TexZ += dz;
        } else {
          // height is from the floor
          if (dbg_pobj_verbose_spawn.asBool()) GCon->Logf(NAME_Debug, "pobj #%d: height=%g; newheight=%g (from the floor)", po->tag, hgt, newhgt);
          // fix ceiling (move it down), move texture z
          const float dz = newhgt-hgt;
          po->poceiling.minz += dz;
          po->poceiling.maxz += dz;
          po->poceiling.dist += dz;
          po->poceiling.TexZ += dz;
        }
      }
    }
    #else
    // still do nothing (and reject non-zero height, to allow future expansion)
    if (anchorthing && anchorthing->height != 0.0f) Host_Error("3d pobj #%d anchor thing has non-zero height", po->tag);
    /*
    // anchor thing height offsets from the *destination* sector
    if (anchorthing && anchorthing->height != 0.0f) {
      const bool fromCeiling = (anchorthing->height < 0.0f);
      const float newhgt = fabsf(anchorthing->height);
      sector_t *dsec = PointInSubsector(sspot2d)->sector;
      if (dsec != po->posector) {
        if (fromCeiling) {
          deltaZ += dsec->ceiling.TexZ-po->poceiling.TexZ; // move up to the ceiling
          deltaZ -= newhgt; // and down
        } else {
          deltaZ += dsec->floor.TexZ-po->pofloor.TexZ; // move down to the floor
          deltaZ += newhgt; // and up
        }
      } else {
        GCon->Logf(NAME_Error, "anchor height offset is ignored due to invalid destination sector");
      }
    }
    */
    #endif

    // check if all middle textures either wrapped, or high enough
    if (po->pofloor.minz > po->poceiling.minz) Host_Error("invalid 3d pobj #%d height", po->tag);
    if (po->pofloor.minz < po->poceiling.minz) {
      for (auto &&it : po->LineFirst()) {
        line_t *ld = it.line();
        vassert(ld->sidenum[0] >= 0);
        const side_t *sidedef = &Sides[ld->sidenum[0]];
        // clamp with texture height if it is not wrapped
        if (((ld->flags&ML_WRAP_MIDTEX)|(sidedef->Flags&SDF_WRAPMIDTEX)) == 0) {
          VTexture *MTex = GTextureManager(sidedef->MidTexture);
          vassert(MTex && MTex->Type != TEXTYPE_Null);
          const float texh = MTex->GetScaledHeightF()/sidedef->Mid.ScaleY;
          if (!isFiniteF(texh) || texh < 1.0f) Host_Error("3d pobj #%d has empty midtex at line #%d", po->tag, (int)(ptrdiff_t)(ld-&Lines[0]));
          float z0, z1;
          if (ld->flags&ML_DONTPEGBOTTOM) {
            // bottom of texture at bottom
            z1 = po->pofloor.TexZ+texh;
          } else {
            // top of texture at top
            z1 = po->poceiling.TexZ;
          }
          z0 = z1-texh;
          if (z0 > po->pofloor.TexZ || z1 < po->poceiling.TexZ) Host_Error("3d pobj #%d has too smal midtex at line #%d", po->tag, (int)(ptrdiff_t)(ld-&Lines[0]));
        }
      }
    }
  }

  if (dbg_pobj_verbose_spawn.asBool()) {
    GCon->Logf(NAME_Debug, "pobj #%d: height=%g; spot=(%g,%g,%g); dz=%g", po->tag, po->poceiling.maxz-po->pofloor.minz, po->startSpot.x, po->startSpot.y, po->startSpot.z, deltaZ);

    GCon->Logf(NAME_Debug, "001: pobj #%d: floor=(%g,%g,%g:%g) %g:%g; ceiling=(%g,%g,%g:%g) %g:%g", po->tag,
      po->pofloor.normal.x, po->pofloor.normal.y, po->pofloor.normal.z, po->pofloor.dist, po->pofloor.minz, po->pofloor.maxz,
      po->poceiling.normal.x, po->poceiling.normal.y, po->poceiling.normal.z, po->poceiling.dist, po->poceiling.minz, po->poceiling.maxz);
  }

  // offset flat textures
  if (po->posector && (deltaX || deltaY)) {
    if (po->posector->floor.pic != skyflatnum) {
      VTexture *FTex = GTextureManager(po->posector->floor.pic);
      if (FTex && FTex->Type != TEXTYPE_Null) {
        const float w = FTex->GetScaledWidthF();
        const float h = FTex->GetScaledHeightF();
        if (w > 0.0f) po->pofloor.xoffs += fmodf(po->startSpot.x+deltaX, w);
        if (h > 0.0f) po->pofloor.yoffs -= fmodf(po->startSpot.y+deltaY, h);
      }
    }

    if (po->posector->floor.pic != skyflatnum) {
      VTexture *CTex = GTextureManager(po->posector->ceiling.pic);
      if (CTex && CTex->Type != TEXTYPE_Null) {
        const float w = CTex->GetScaledWidthF();
        const float h = CTex->GetScaledHeightF();
        //GCon->Logf(NAME_Debug, "*** pobj #%d: x=%g; y=%g; xmod=%g; ymod=%g; spot=(%g, %g) (%g, %g)", po->tag, x, y, fmodf(x, CTex->Width), fmodf(y, CTex->Height), po->startSpot.x, po->startSpot.y, fmodf(po->startSpot.x, CTex->Width), fmodf(po->startSpot.y, CTex->Height));
        if (w > 0.0f) po->poceiling.xoffs += fmodf(po->startSpot.x+deltaX, w);
        if (h > 0.0f) po->poceiling.yoffs -= fmodf(po->startSpot.y+deltaY, h);
      }
    }
  }

  OffsetPolyobjFlats(po, deltaZ, true);

  UpdatePolySegs(po);

  if (dbg_pobj_verbose_spawn.asBool()) {
    GCon->Logf(NAME_Debug, "translated %s pobj #%d: floor=(%g,%g,%g:%g) %g:%g; ceiling=(%g,%g,%g:%g) %g:%g", (po->posector ? "3d" : "2d"), po->tag,
      po->pofloor.normal.x, po->pofloor.normal.y, po->pofloor.normal.z, po->pofloor.dist, po->pofloor.minz, po->pofloor.maxz,
      po->poceiling.normal.x, po->poceiling.normal.y, po->poceiling.normal.z, po->poceiling.dist, po->poceiling.minz, po->poceiling.maxz);
  }

  // `InitPolyBlockMap()` will call `LinkPolyobj()`, which will calcilate the bounding box
  // no need to notify renderer yet (or update subsector list), `InitPolyBlockMap()` will do it for us

  #ifdef CLIENT
  if (Renderer) Renderer->InvalidatePObjLMaps(po);
  #endif
}
