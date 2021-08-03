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
#include "p_entity.h"
#include "p_levelinfo.h"
#include "p_decal.h"
#ifdef CLIENT
# include "../drawer.h"
#endif

//#define VV_POBJ_DEBUG_ROTATION_UNTSTUCK

// polyobj line start special
#define PO_LINE_START     (1)
#define PO_LINE_EXPLICIT  (5)


static VCvarB dbg_pobj_disable("dbg_pobj_disable", false, "Disable most polyobject operations?", CVAR_PreInit);
static VCvarB dbg_pobj_verbose("dbg_pobj_verbose", false, "Verbose polyobject spawner?", CVAR_PreInit);

static VCvarB dbg_pobj_unstuck_verbose("__dbg_pobj_unstuck_verbose", false, "Verbose 3d pobj unstuck code?", CVAR_PreInit);


enum {
  // this thing's height is z offset from the *inner* sector floor
  PO_SPAWN_3D_TYPE = 9369,
};


static TMapNC<VEntity *, bool> poEntityMap;
static TMapNC<VEntity *, bool> poEntityMapToMove;
//static TMapNC<VEntity *, TVec> poEntityMapFloat;
static TMapNC<sector_t *, bool> poSectorMap;
//static TArray<sector_t *> poEntityArray;


enum {
  AFF_VERT  = 1u<<0,
  AFF_MOVE  = 1u<<1,
  AFF_STUCK = 1u<<2,
  //
  AFF_NOTHING_ZERO = 0u,
};

struct SavedEntityData {
  VEntity *mobj;
  TVec Origin;
  TVec LastMoveOrigin;
  unsigned aflags; // AFF_xxx flags, used in movement
  TVec spot; // used in rotator
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
  #ifdef CLIENT
  if (level->Renderer) level->Renderer->InvalidateStaticLightmapsSubs(sub);
  #endif
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
  Z_Free(segs);
  Z_Free(verts);
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
void polyobjpart_t::CreateClipSegs (VLevel *Level) {
  reset();
  flags |= CLIPSEGS_CREATED;
  const bool is3d = pobj->Is3D();
  // create clip segs for each polyobject part
  // we can use full lines here, because they will be clipped to destination subsector
  for (auto &&it : pobj->LineFirst()) {
    // clip pobj seg to subsector
    line_t *ld = it.line();
    side_t *sd = nullptr;

    //GCon->Logf(NAME_Debug, "trying pobj #%d, line #%d to subsector of sector #%d", pobj->tag, (int)(ptrdiff_t)(ld-&Level->Lines[0]), (int)(ptrdiff_t)(sub->sector-&Level->Sectors[0]));

    seg_t newseg;
    memset((void *)&newseg, 0, sizeof(newseg));

    TVec sv1 = *ld->v1;
    TVec sv2 = *ld->v2;

    // check for valid line, and setup vertices
    if (is3d) {
      if (ld->sidenum[0] >= 0 && ld->frontsector == pobj->posector) {
        newseg.side = 0;
        sd = &Level->Sides[ld->sidenum[0]];
        newseg.v1 = &sv1;
        newseg.v2 = &sv2;
      } else if (ld->sidenum[1] >= 0 && ld->backsector == pobj->posector) {
        newseg.side = 1;
        sd = &Level->Sides[ld->sidenum[1]];
        newseg.v1 = &sv2;
        newseg.v2 = &sv1;
      }
    } else {
      // normal pobj
      if ((ld->flags&ML_TWOSIDED) != 0) continue; // never clip with 2-sided pobj walls (for now)
      if (ld->sidenum[0] < 0) continue;
      newseg.side = 0;
      sd = &Level->Sides[ld->sidenum[0]];
      newseg.v1 = &sv1;
      newseg.v2 = &sv2;
    }
    if (!sd) continue; // just in case

    newseg.sidedef = sd;
    newseg.linedef = ld;
    newseg.frontsector = sub->sector; // just in case
    newseg.frontsub = sub;
    newseg.pobj = pobj;

    //GCon->Logf(NAME_Debug, "...clipping pobj #%d, line #%d to subsector of sector #%d", pobj->tag, (int)(ptrdiff_t)(ld-&Level->Lines[0]), (int)(ptrdiff_t)(sub->sector-&Level->Sectors[0]));

    if (!Level->ClipPObjSegToSub(sub, &newseg)) continue; // out of this subsector

    //GCon->Logf(NAME_Debug, "...ADDING clipped line of pobj #%d, line #%d to subsector of sector #%d", pobj->tag, (int)(ptrdiff_t)(ld-&Level->Lines[0]), (int)(ptrdiff_t)(sub->sector-&Level->Sectors[0]));
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
    part->reset(); // remove clipsegs
    part->sub = nullptr;
    part->nextsub = nullptr;
    part->nextpobj = freeparts;
    freeparts = part;
    #ifdef CLIENT
    if (GClLevel && GClLevel->Renderer) GClLevel->Renderer->InvalidateStaticLightmapsSubs(sub);
    #endif
  }
}


//==========================================================================
//
//  polyobj_t::ResetClipSegs
//
//==========================================================================
void polyobj_t::ResetClipSegs () {
  for (polyobjpart_t *part = parts; part; part = part->nextpobj) {
    part->reset();
  }
}


//==========================================================================
//
//  subsector_t::ResetClipSegs
//
//==========================================================================
void subsector_t::ResetClipSegs () {
  for (polyobjpart_t *part = polyparts; part; part = part->nextsub) {
    part->reset();
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
//  VLevel::ValidateNormalPolyobj
//
//  note that polyobject segs are not collected yet, only lines
//
//==========================================================================
void VLevel::ValidateNormalPolyobj (polyobj_t *po) {
  if (po->numlines < 3) {
    GCon->Logf(NAME_Error, "pobj #%d has less than 3 lines (%d)", po->tag, po->numlines);
  }
  vassert(po->numlines);
  sector_t *sfront = po->lines[0]->frontsector;
  sector_t *sback = po->lines[0]->backsector;
  for (auto &&it : po->LineFirst()) {
    line_t *ld = it.line();
    if (ld->frontsector != sfront || ld->backsector != sback) {
      Host_Error("invalid pobj #%d configuration -- bad line #%d (invalid sectors)", po->tag, (int)(ptrdiff_t)(ld-&Lines[0]));
    }
  }
}


#define PO3DALLOWEDLFAGS  ( \
  ML_BLOCKING| \
  ML_BLOCKMONSTERS| \
  ML_TWOSIDED| \
  /*ML_DONTPEGTOP|*/ \
  ML_DONTPEGBOTTOM| \
  /*ML_SOUNDBLOCK|*/ \
  ML_DONTDRAW| \
  ML_MAPPED| \
  ML_REPEAT_SPECIAL| \
  ML_ADDITIVE| \
  ML_MONSTERSCANACTIVATE| \
  ML_BLOCKPLAYERS| \
  ML_BLOCKEVERYTHING| \
  ML_BLOCK_FLOATERS| \
  ML_CLIP_MIDTEX| \
  ML_WRAP_MIDTEX| \
  ML_CHECKSWITCHRANGE| \
  ML_FIRSTSIDEONLY| \
  ML_BLOCKPROJECTILE| \
  ML_BLOCKUSE| \
  ML_BLOCKSIGHT| \
  ML_BLOCKHITSCAN| \
  ML_NODECAL| \
  0u)


//==========================================================================
//
//  VLevel::FixPolyobjCachedFlags
//
//  called from loader
//
//==========================================================================
void VLevel::FixPolyobjCachedFlags (polyobj_t *po) {
  if (!po) return;
  po->PolyFlags &= ~polyobj_t::PF_HasTopBlocking;
  if (!po->posector) return; // non-3d pobjs has no cached flags
  for (auto &&it : po->LineFirst()) {
    line_t *ld = it.line();
    if (ld->flags&ML_CLIP_MIDTEX) {
      po->PolyFlags |= polyobj_t::PF_HasTopBlocking;
      break;
    }
  }
}


//==========================================================================
//
//  VLevel::Validate3DPolyobj
//
//  note that polyobject segs are not collected yet, only lines
//
//==========================================================================
void VLevel::Validate3DPolyobj (polyobj_t *po) {
  vassert(po->numlines >= 3);

  po->PolyFlags &= ~polyobj_t::PF_HasTopBlocking;
  sector_t *sfront = po->lines[0]->frontsector;
  sector_t *sback = po->lines[0]->backsector;
  for (auto &&it : po->LineFirst()) {
    line_t *ld = it.line();
    if (ld->frontsector != sfront || ld->backsector != sback) {
      Host_Error("invalid 3d pobj #%d configuration -- bad line #%d (invalid sectors)", po->tag, (int)(ptrdiff_t)(ld-&Lines[0]));
    }
    // allow some other flags
    /*
         if (ld->flags&ML_BLOCKEVERYTHING) ld->flags |= ML_BLOCKING;
    else if (ld->flags&(ML_BLOCKMONSTERS|ML_BLOCKPLAYERS)) ld->flags |= ML_BLOCKING;
    */
    // check flags
    if (!(ld->flags&ML_BLOCKING)) {
      Host_Error("3d pobj #%d line #%d should have \"impassable\" flag!", po->tag, (int)(ptrdiff_t)(ld-&Lines[0]));
      //ld->flags |= ML_BLOCKING;
    }
    // remove flags we are not interested in
    //ld->flags &= ~(ML_BLOCKEVERYTHING|ML_BLOCKMONSTERS|ML_BLOCKPLAYERS);
    // check for some extra flags
    if (ld->flags&~PO3DALLOWEDLFAGS) Host_Error("3d pobj #%d line #%d have some forbidden flags set!", po->tag, (int)(ptrdiff_t)(ld-&Lines[0]));
    //if (linedef->alpha != 1.0f) Host_Error("3d pobj #%d line #%d cannot be alpha-blended!", po->tag, (int)(ptrdiff_t)(ld-&Lines[0])); //nope, it is used for top/bottom textures
    // validate midtex (only the front side)
    if (ld->sidenum[0] < 0) Host_Error("3d pobj #%d line #%d has no front side!", po->tag, (int)(ptrdiff_t)(ld-&Lines[0]));
    //if (ld->sidenum[1] < 0) Host_Error("3d pobj #%d line #%d has no back side!", po->tag, (int)(ptrdiff_t)(ld-&Lines[0]));
    const side_t *sidedef = &Sides[ld->sidenum[0]];
    VTexture *MTex = GTextureManager(sidedef->MidTexture);
    if (!MTex || MTex->Type == TEXTYPE_Null) Host_Error("3d pobj #%d line #%d has invalid front midtex!", po->tag, (int)(ptrdiff_t)(ld-&Lines[0]));
    if (MTex->GetScaledWidthF()/sidedef->Mid.ScaleY < 1.0f) Host_Error("3d pobj #%d line #%d has invalid front midtex width!", po->tag, (int)(ptrdiff_t)(ld-&Lines[0]));
    if (MTex->GetScaledHeightF()/sidedef->Mid.ScaleY < 1.0f) Host_Error("3d pobj #%d line #%d has invalid front midtex height!", po->tag, (int)(ptrdiff_t)(ld-&Lines[0]));
    // it should not be translucent or masked
    if (MTex->isSeeThrough()) Host_Error("3d pobj #%d line #%d has transparent/translucent midtex!", po->tag, (int)(ptrdiff_t)(ld-&Lines[0]));
    // check sides
    const side_t *s0 = (ld->sidenum[0] < 0 ? nullptr : &Sides[ld->sidenum[0]]);
    const side_t *s1 = (ld->sidenum[1] < 0 ? nullptr : &Sides[ld->sidenum[1]]);
    //if (!s0) Host_Error("invalid 3d pobj #%d configuration -- bad line #%d (no front side)", po->tag, (int)(ptrdiff_t)(ld-&Lines[0]));
    //if (GTextureManager.IsEmptyTexture(s0->MidTexture)) Host_Error("invalid 3d pobj #%d configuration -- bad line #%d (empty midtex)", po->tag, (int)(ptrdiff_t)(ld-&Lines[0]));
    // top textures must be the same if they are impassable
    if (ld->flags&ML_CLIP_MIDTEX) {
      // top texture is blocking
      if (GTextureManager.IsEmptyTexture(s0->TopTexture)) Host_Error("invalid 3d pobj #%d configuration -- bad line #%d (toptex is blocking and missing)", po->tag, (int)(ptrdiff_t)(ld-&Lines[0]));
      if (!s1 || GTextureManager.IsEmptyTexture(s1->TopTexture)) Host_Error("invalid 3d pobj #%d configuration -- bad line #%d (inner toptex is blocking and missing)", po->tag, (int)(ptrdiff_t)(ld-&Lines[0]));
      // check texture heights
      VTexture *TTex0 = GTextureManager(s0->TopTexture);
      if (!TTex0) TTex0 = GTextureManager[GTextureManager.DefaultTexture];
      VTexture *TTex1 = GTextureManager(s1->TopTexture);
      if (!TTex1) TTex1 = GTextureManager[GTextureManager.DefaultTexture];
      const float texh0 = TTex0->GetScaledHeightF()/s0->Top.ScaleY;
      const float texh1 = TTex1->GetScaledHeightF()/s1->Top.ScaleY;
      if (texh0 != texh1) Host_Error("invalid 3d pobj #%d configuration -- bad line #%d (blocking toptex has different sizes)", po->tag, (int)(ptrdiff_t)(ld-&Lines[0]));
      po->PolyFlags |= polyobj_t::PF_HasTopBlocking;
    }
  }
}


//==========================================================================
//
//  VLevel::IsGood3DPolyobj
//
//  valid 3d pobj should have properly closed contours without "orphan" lines
//  all lines should be two-sided, with the same frontsector and same
//  backsector (and those sectors must be different)
//
//  note that polyobject segs are not collected yet, only lines
//
//==========================================================================
bool VLevel::IsGood3DPolyobj (polyobj_t *po) {
  if (po->numlines < 3) return false; // no wai
  //po->lines = new line_t*[explines.length()];
  //po->numlines = 0;

  // check if all lines are two-sided, with valid sectors
  sector_t *sfront = nullptr;
  sector_t *sback = nullptr;

  // but don't bother if we don't want to spawn 3d pobj
  for (auto &&it : po->LineFirst()) {
    line_t *ld = it.line();
    if (!(ld->flags&ML_TWOSIDED)) return false; // found one-sided line
    if (!ld->frontsector || !ld->backsector || ld->backsector == ld->frontsector) return false; // invalid sectors
    if (!sfront) { sfront = ld->frontsector; sback = ld->backsector; }
    if (ld->frontsector != sfront || ld->backsector != sback) return false; // invalid sectors
    // this seems to be a valid line
  }
  // should have both sectors
  if (!sback || sfront == sback) return false;

  // all lines seems to be valid, check for properly closed contours
  IncrementValidCount();
  for (auto &&it : po->LineFirst()) {
    line_t *ld = it.line();
    if (ld->validcount == validcount) continue; // already processed
    // walk the contour, marking all lines
    line_t *curr = ld;
    // first line should be marked as used last
    int count = 0;
    do {
      // find v2 line
      line_t *next = nullptr;
      for (auto &&it2 : po->LineFirst()) {
        line_t *l2 = it2.line();
        if (l2 == curr) continue; // just in case
        if (l2->validcount == validcount) continue; // already used
        if (l2->v1 == curr->v2) {
          if (next) return false; // two connected lines
          next = l2;
          break;
        }
      }
      if (!next) return false; // no contour continuation found, invalid pobj
      ++count;
      curr = next;
      curr->validcount = validcount;
    } while (curr != ld);
    if (count < 3) return false;
  }

  // now check if we have any unmarked lines
  for (auto &&it : po->LineFirst()) {
    line_t *ld = it.line();
    if (ld->validcount != validcount) return false; // oops
  }

  return true;
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

  if (dbg_pobj_verbose.asBool()) GCon->Logf(NAME_Debug, "SpawnPolyobj: tag=%d; idx=%d; thing=%d", tag, index, (thing ? (int)(ptrdiff_t)(thing-&Things[0]) : -1));

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
    if (linetag && dbg_pobj_verbose.asBool()) {
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

  if (dbg_pobj_verbose.asBool()) {
    GCon->Logf(NAME_Debug, "SPAWNED %s pobj #%d: floor=(%g,%g,%g:%g) %g:%g; ceiling=(%g,%g,%g:%g) %g:%g; lines=%d; segs=%d; height=%g; refsec=%d", (valid3d ? "3D" : "2D"), po->tag,
      po->pofloor.normal.x, po->pofloor.normal.y, po->pofloor.normal.z, po->pofloor.dist, po->pofloor.minz, po->pofloor.maxz,
      po->poceiling.normal.x, po->poceiling.normal.y, po->poceiling.normal.z, po->poceiling.dist, po->poceiling.minz, po->poceiling.maxz,
      po->numlines, po->numsegs, po->poceiling.maxz-po->pofloor.minz,
      (refsec ? (int)(ptrdiff_t)(refsec-&Sectors[0]) : -1));
  }
}


//==========================================================================
//
//  VLevel::Add3DPolyobjLink
//
//==========================================================================
void VLevel::Add3DPolyobjLink (mthing_t * /*thing*/, int srcpid, int destpid) {
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
//  VLevel::CalcPolyobjCenter2D
//
//==========================================================================
TVec VLevel::CalcPolyobjCenter2D (polyobj_t *po) noexcept {
  #if 1
  // find bounding box, and use it's center
  if (po->numlines == 0) Sys_Error("pobj #%d has no lines (internal engine error)", po->tag);
  float xmin = +FLT_MAX;
  float ymin = +FLT_MAX;
  float xmax = -FLT_MAX;
  float ymax = -FLT_MAX;
  for (auto &&it : po->LineFirst()) {
    for (unsigned f = 0; f < 2; ++f) {
      const TVec v = *(f ? it.line()->v2 : it.line()->v1);
      xmin = min2(xmin, v.x);
      ymin = min2(ymin, v.y);
      xmax = max2(xmax, v.x);
      ymax = max2(ymax, v.y);
    }
  }
  return TVec((xmin+xmax)*0.5f, (ymin+ymax)*0.5f);
  #else
  // ssectors can contain split points on the same line
  // if i'll simply sum all vertices, our center could be not right
  // so i will use only "turning points" (as it should be)
  int count = 0;
  TVec center = TVec(0.0f, 0.0f);
  TVec pdir(0.0f, 0.0f);
  TVec lastv1(0.0f, 0.0f);
  for (auto &&it : po->LineFirst()) {
    line_t *ld = it.line();
    if (!count) {
      // first one
      lastv1 = *ld->v1;
      pdir = ((*ld->v2)-lastv1);
      if (pdir.length2DSquared() >= 1.0f) {
        pdir = pdir.normalised();
        center += lastv1;
        count = 1;
      }
    } else {
      // are we making a turn here?
      const TVec xdir = ((*ld->v2)-lastv1).normalised();
      if (fabsf(xdir.dot(pdir)) < 1.0f-0.0001f) {
        // looks like we did made a turn
        // we have a new point
        // remember it
        lastv1 = *ld->v1;
        // add it to the sum
        center += lastv1;
        ++count;
        // and remember new direction
        pdir = ((*ld->v2)-lastv1).normalised();
      }
    }
  }
  // if we have less than three points, something's gone wrong...
  if (count < 3) return TVec::ZeroVector;
  center = center/(float)count;
  return center;
  #endif
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

  if (!forceRecreation && fabsf(z) != 0.0f) forceRecreation = true;

  if (!forceRecreation &&
      (FASI(po->pofloor.PObjCX) != FASI(po->startSpot.x) ||
       FASI(po->pofloor.PObjCY) != FASI(po->startSpot.y) ||
       FASI(po->poceiling.PObjCX) != FASI(po->startSpot.x) ||
       FASI(po->poceiling.PObjCY) != FASI(po->startSpot.y)))
  {
    forceRecreation = true;
  }
  po->pofloor.PObjCX = po->poceiling.PObjCX = po->startSpot.x;
  po->pofloor.PObjCY = po->poceiling.PObjCY = po->startSpot.y;

  // fix sector
  sec->floor = po->pofloor;
  sec->ceiling = po->poceiling;

  if (forceRecreation) {
    for (subsector_t *sub = sec->subsectors; sub; sub = sub->seclink) {
      for (subregion_t *reg = sub->regions; reg; reg = reg->next) {
        reg->ForceRecreation();
      }
    }
  }
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
  for (auto &&it : po->SegFirst()) {
    CalcSegPlaneDir(it.seg());
    // invalidate decals
    // we can check seg vertices in cache checks, but meh
    for (decal_t *dc = it.seg()->decalhead; dc; dc = dc->next) dc->invalidateCache();
  }
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
  if (dbg_pobj_verbose.asBool()) GCon->Logf(NAME_Debug, "pobj #%d has %d vertices", po->tag, segptnum);

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

    if (dbg_pobj_verbose.asBool()) {
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
          if (dbg_pobj_verbose.asBool()) GCon->Logf(NAME_Debug, "pobj #%d: height=%g; newheight=%g (from the ceiling)", po->tag, hgt, newhgt);
          // fix floor (move it up), move texture z
          const float dz = hgt-newhgt;
          po->pofloor.minz += dz;
          po->pofloor.maxz += dz;
          po->pofloor.dist += dz;
          po->pofloor.TexZ += dz;
        } else {
          // height is from the floor
          if (dbg_pobj_verbose.asBool()) GCon->Logf(NAME_Debug, "pobj #%d: height=%g; newheight=%g (from the floor)", po->tag, hgt, newhgt);
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

  if (dbg_pobj_verbose.asBool()) {
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

  if (dbg_pobj_verbose.asBool()) {
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
    // update sector sound origin
    po->posector->soundorg = TVec((pobbox[BOX2D_RIGHT]+pobbox[BOX2D_LEFT])*0.5f, (pobbox[BOX2D_TOP]+pobbox[BOX2D_BOTTOM])*0.5f, 0.0f);
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
  //memcpy(po->bbox2d, pobbox, sizeof(po->bbox2d));
  CopyBBox2D(po->bbox2d, pobbox);

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
      VEntity *mobj = it.key();
      //GCon->Logf(NAME_Debug, "pobj #%d: relinking entity %s(%u)", po->tag, mobj->GetClass()->GetName(), mobj->GetUniqueId());
      mobj->LinkToWorld(); // relink it
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


struct UnstuckInfo {
  TVec uv; // unstuck vector
  TVec unorm; // unstuck normal
  bool fromWrongSide;
  bool blockedByOther;
};



//==========================================================================
//
//  check3DPObjLineBlocked
//
//==========================================================================
static VVA_ALWAYS_INLINE bool check3DPObjLineBlocked (const polyobj_t *po, VEntity *mobj, const line_t *ld) {
  const float mobjz0 = mobj->Origin.z;
  const float ptopz = po->poceiling.minz;
  if (mobjz0 >= ptopz) {
    if ((ld->flags&ML_CLIP_MIDTEX) == 0) return false;
    if (!mobj->IsBlocking3DPobjLineTop(ld)) return false;
    if (!mobj->LineIntersects(ld)) return false;
    // side doesn't matter, as it is guaranteed that both sides have the texture with the same height
    const side_t *tside = &mobj->XLevel->Sides[ld->sidenum[0]];
    if (tside->TopTexture <= 0) return false; // wtf?!
    VTexture *ttex = GTextureManager(tside->TopTexture);
    if (!ttex || ttex->Type == TEXTYPE_Null) return false; // wtf?!
    const float texh = ttex->GetScaledHeightF()/tside->Top.ScaleY;
    if (mobjz0 >= ptopz+texh) return false; // didn't hit top texture
  } else {
    if (!mobj->IsBlockingLine(ld)) return false;
    if (!mobj->LineIntersects(ld)) return false;
  }
  return true;
}


//==========================================================================
//
//  checkPObjLineBlocked
//
//==========================================================================
static VVA_ALWAYS_INLINE bool checkPObjLineBlocked (const polyobj_t *po, VEntity *mobj, const line_t *ld) {
  if (po->posector) {
    if (mobj->Origin.z >= po->poceiling.maxz) return false; // fully above
    if (mobj->Origin.z+max2(0.0f, mobj->Height) <= po->pofloor.minz) return false; // fully below
    return check3DPObjLineBlocked(po, mobj, ld);
  } else {
    // check for non-3d pobj with midtex
    if ((ld->flags&(ML_TWOSIDED|ML_3DMIDTEX)) == (ML_TWOSIDED|ML_3DMIDTEX)) {
      if (!mobj->LineIntersects(ld)) return false;
      // use front side
      const int side = 0; //ld->PointOnSide(mobj->Origin);
      float pz0 = 0.0f, pz1 = 0.0f;
      if (!mobj->XLevel->GetMidTexturePosition(ld, side, &pz0, &pz1)) return false; // no middle texture
      if (mobj->Origin.z >= pz1 || mobj->Origin.z+max2(0.0f, mobj->Height) <= pz0) return false; // no collision
      //ldblock = true;
    } else {
      if (!mobj->IsBlockingLine(ld)) return false;
      if (!mobj->LineIntersects(ld)) return false;
    }
  }
  return true;
}


//==========================================================================
//
//  CalcPolyUnstuckVectorNew
//
//  calculate horizontal "unstuck vector" (delta to move)
//  for the given 3d polyobject
//
//  returns `false` if stuck, and cannot unstuck
//
//  returns `true` if not stuck, or can unstuck
//  in this case, check `uvlist.length()`, if it is zero, then we aren't stuck
//
//  this is wrong, but is still better than stucking
//
//==========================================================================
static bool CalcPolyUnstuckVector (TArray<UnstuckInfo> &uvlist, VLevel *Level, polyobj_t *po, VEntity *mobj) {
  uvlist.resetNoDtor();
  //if (!po || !mobj || !po->posector) return false;

  const float rad = mobj->Radius;
  if (rad <= 0.0f || mobj->Height <= 0.0f) return true; // just in case
  const float radext = rad+0.2f;

  const TVec mobjOrigOrigin = mobj->Origin;
  const TVec orig2d(mobjOrigOrigin.x, mobjOrigOrigin.y);
  float bbox2d[4];
  mobj->Create2DBox(bbox2d);
  bbox2d[BOX2D_TOP] = orig2d.y+rad;
  bbox2d[BOX2D_BOTTOM] = orig2d.y-rad;
  bbox2d[BOX2D_RIGHT] = orig2d.x+rad;
  bbox2d[BOX2D_LEFT] = orig2d.x-rad;

  int mobjIsInside = 0; // didn't checked yet; -1: outside; 1: inside

  // if no "valid" sides to unstuck found, but has some "invalid" ones, try "invalid" sides
  bool wasIntersect = false;
  const float mobjz0 = mobj->Origin.z;
  //const float mobjz1 = mobjz0+mobj->Height;
  const float ptopz = po->poceiling.minz;
  const bool checkTopTex = (mobjz0 >= ptopz);

  const float crmult[4][2] = {
    { -1.0f, -1.0f }, // bottom-left
    { +1.0f, -1.0f }, // bottom-right
    { -1.0f, +1.0f }, // top-left
    { +1.0f, +1.0f }, // top-right
  };

  for (auto &&lit : po->LineFirst()) {
    const line_t *ld = lit.line();

    // if we are above the polyobject, check for blocking top texture
    if (!check3DPObjLineBlocked(po, mobj, ld)) continue;
    /*
    if (checkTopTex) {
      bool hitit = false;
      do {
        if ((ld->flags&ML_CLIP_MIDTEX) == 0) break;
        if (!mobj->IsBlocking3DPobjLineTop(ld)) break;
        if (!mobj->LineIntersects(ld)) break;
        // side doesn't matter, as it is guaranteed that both sides have the texture with the same height
        const side_t *tside = &Level->Sides[ld->sidenum[0]];
        if (tside->TopTexture <= 0) break; // wtf?!
        VTexture *ttex = GTextureManager(tside->TopTexture);
        if (!ttex || ttex->Type == TEXTYPE_Null) break; // wtf?!
        const float texh = ttex->GetScaledHeightF()/tside->Top.ScaleY;
        if (mobjz0 >= ptopz+texh) break; // didn't hit top texture
        hitit = true;
      } while (0);
      if (!hitit) continue;
    } else {
      if (!mobj->IsBlockingLine(ld)) continue;
      if (!mobj->LineIntersects(ld)) continue;
    }
    */
    wasIntersect = true;

    // check if we're inside (we need this to determine the right line side)
    //FIXME: this may be wrong for huge angles, because after rotation the object could move from outside to inside, or vice versa
    if (!mobjIsInside) {
      mobjIsInside = (Level->Is2DPointInside3DPolyObj(po, mobjOrigOrigin.x, mobjOrigOrigin.y) ? 1 : -1);
    }

    const float orgsdist = ld->PointDistance(orig2d);

    if (dbg_pobj_unstuck_verbose.asBool()) {
      GCon->Logf(NAME_Debug, "mobj '%s': going to unstuck from pobj %d, line #%d, orgsdist=%g; checkTopTex=%d; inside=%d",
        mobj->GetClass()->GetName(), po->tag, (int)(ptrdiff_t)(ld-&Level->Lines[0]), orgsdist, (int)checkTopTex, mobjIsInside);
    }

    const bool badSide = (mobjIsInside > 0 ? (checkTopTex ? (orgsdist > 0.0f) : (orgsdist < 0.0f)) : (orgsdist < 0.0f));

    // check 4 corners, find the shortest "unstuck" distance
    bool foundVector = false;
    for (unsigned cridx = 0u; cridx < 4u; ++cridx) {
      TVec corner = orig2d;
      corner += TVec(radext*crmult[cridx][0], radext*crmult[cridx][1], 0.0f);
      const float csdist = ld->PointDistance(corner);

      if (checkTopTex) {
        if (csdist <= 0.0f) continue;
      } else {
        if (csdist >= 0.0f) continue;
      }
      const TVec uv = ld->normal*(-csdist);

      // check if we'll stuck in some other pobj line
      bool stuckOther = false;
      {
        mobj->Origin = mobjOrigOrigin+uv;
        for (auto &&lxx : po->LineFirst()) {
          const line_t *lnx = lxx.line();
          if (lnx == ld) continue;
          //if (!mobj->IsBlockingLine(lnx)) continue;
          //if (!mobj->LineIntersects(lnx)) continue;

          // if we are above the polyobject, check for blocking top texture
          /*
          if (checkTopTex) {
            bool hitit = false;
            do {
              if ((lnx->flags&ML_CLIP_MIDTEX) == 0) break;
              if (!mobj->IsBlocking3DPobjLineTop(lnx)) break;
              if (!mobj->LineIntersects(lnx)) break;
              // side doesn't matter, as it is guaranteed that both sides have the texture with the same height
              const side_t *tside = &Level->Sides[lnx->sidenum[0]];
              if (tside->TopTexture <= 0) break; // wtf?!
              VTexture *ttex = GTextureManager(tside->TopTexture);
              if (!ttex || ttex->Type == TEXTYPE_Null) break; // wtf?!
              const float texh = ttex->GetScaledHeightF()/tside->Top.ScaleY;
              if (mobjz0 >= ptopz+texh) break; // didn't hit top texture
              hitit = true;
            } while (0);
            if (!hitit) continue;
          } else {
            if (!mobj->IsBlockingLine(lnx)) continue;
            if (!mobj->LineIntersects(lnx)) continue;
          }
          */
          if (!check3DPObjLineBlocked(po, mobj, lnx)) continue;

          if (dbg_pobj_unstuck_verbose.asBool()) {
            GCon->Logf(NAME_Debug, "mobj '%s': going to unstuck from pobj %d, line #%d, corner %u; stuck in line #%d",
              mobj->GetClass()->GetName(), po->tag, (int)(ptrdiff_t)(ld-&Level->Lines[0]), cridx, (int)(ptrdiff_t)(lnx-&Level->Lines[0]));
          }
          stuckOther = true;
          break;
        }
        mobj->Origin = mobjOrigOrigin;
        //if (stuckOther) continue; // alas: NOPE
      }

      if (dbg_pobj_unstuck_verbose.asBool()) {
        GCon->Logf(NAME_Debug, "mobj '%s': going to unstuck from pobj %d, line #%d; orig2d=(%g,%g); rad=%g (%g); cridx=%u; csdist=%g; uvec=(%g,%g); ulen=%g",
          mobj->GetClass()->GetName(), po->tag, (int)(ptrdiff_t)(ld-&Level->Lines[0]),
          orig2d.x, orig2d.y, rad, radext, cridx, csdist, uv.x, uv.y, uv.length2D());
      }

      // append to the list
      foundVector = true;
      {
        UnstuckInfo &nfo = uvlist.alloc();
        nfo.uv = uv;
        nfo.unorm = ld->normal; // pobj sides points to outside
        nfo.fromWrongSide = badSide;
        nfo.blockedByOther = stuckOther;
      }
    }

    if (!foundVector) return false; // oops
  }

  if (wasIntersect) return (uvlist.length() > 0);
  return true;
}


//==========================================================================
//
//  NeedPositionCheck
//
//==========================================================================
static inline bool NeedPositionCheck (VEntity *mobj) noexcept {
  if (mobj->IsGoingToDie()) return false; // just in case
  if ((mobj->FlagsEx&(VEntity::EFEX_NoInteraction|VEntity::EFEX_NoTickGrav)) == VEntity::EFEX_NoInteraction) return false;
  if (mobj->Height < 1.0f || mobj->Radius < 1.0f) return false;
  return
    (mobj->EntityFlags&(
      VEntity::EF_Solid|
      VEntity::EF_NoSector|
      VEntity::EF_NoBlockmap|
      VEntity::EF_ColideWithWorld|
      VEntity::EF_Blasted|
      VEntity::EF_Corpse|
      VEntity::EF_Invisible)) == (VEntity::EF_Solid|VEntity::EF_ColideWithWorld);
}


//==========================================================================
//
//  unstuckVectorCompare
//
//==========================================================================
static int unstuckVectorCompare (const void *aa, const void *bb, void */*udata*/) {
  if (aa == bb) return 0;
  const UnstuckInfo *a = (const UnstuckInfo *)aa;
  const UnstuckInfo *b = (const UnstuckInfo *)bb;
  // "wrong sides" should be tried last
  if (a->fromWrongSide != b->fromWrongSide) {
    return (a->fromWrongSide ? -1 : +1);
  }
  // sort by length
  const float alensq = a->uv.length2DSquared();
  const float blensq = b->uv.length2DSquared();
  if (alensq < blensq) return -1;
  if (alensq > blensq) return +1;
  return 0;
}


//==========================================================================
//
//  UnstuckFromRotatedPObj
//
//  returns `false` if movement was blocked
//
//==========================================================================
static bool UnstuckFromRotatedPObj (VLevel *Level, polyobj_t *pofirst, bool skipLink) noexcept {
  if (pofirst && !pofirst->posector) pofirst = nullptr; // don't do this for non-3d pobjs

  // ok, it's not blocked; now try to unstuck
  if (!pofirst) return true; // ok to move

  if (poAffectedEnitities.length() == 0) return true; // still ok to move

  tmtrace_t tmtrace;
  TArray<UnstuckInfo> uvlist;

  // check each affected entity against all pobj lines
  for (auto &&edata : poAffectedEnitities) {
    // if it already was stuck, ignore it
    if (edata.aflags&AFF_STUCK) {
      if (dbg_pobj_unstuck_verbose.asBool()) {
        GCon->Logf(NAME_Debug, "mobj '%s' already stuck", edata.mobj->GetClass()->GetName());
      }
      continue;
    }
    VEntity *mobj = edata.mobj;
    if (!mobj || !NeedPositionCheck(mobj)) continue;

    bool doFinalCheck = true;
    // try to unstuck 3 times
    for (int trycount = 3; trycount > 0; --trycount) {
      doFinalCheck = true;
      bool wasMove = false;

      for (polyobj_t *po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
        if (!po || !po->posector) continue;
        // reject only polyobjects that are above us
        // (otherwise we may miss blockig top texture)
        if (mobj->Origin.z+max2(0.0f, mobj->Height) <= po->pofloor.minz) continue;
        // if pobj has no top-blocking textures, it can be skipped if we're above it
        if ((po->PolyFlags&polyobj_t::PF_HasTopBlocking) == 0) {
          if (mobj->Origin.z+max2(0.0f, mobj->Height) <= po->pofloor.minz) continue;
        }

        const bool canUnstuck = CalcPolyUnstuckVector(uvlist, Level, po, mobj);
        if (dbg_pobj_unstuck_verbose.asBool()) {
          GCon->Logf(NAME_Debug, "mobj %s:%u, pobj %d, triesleft=%d: canUnstuck=%d; uvlist.length=%d", mobj->GetClass()->GetName(), mobj->GetUniqueId(), po->tag, trycount, (int)canUnstuck, uvlist.length());
        }

        if (!canUnstuck) {
          // oops, blocked, and can't unstuck, crush
          if (mobj->EntityFlags&VEntity::EF_Solid) {
            if (mobj->Level->eventPolyThrustMobj(mobj, TVec(0.0f, 0.0f), po, true/*vertical*/)) return false; // blocked
          } else {
            mobj->Level->eventPolyCrushMobj(mobj, po);
            if (NeedPositionCheck(mobj) && mobj->Health > 0) return false; // blocked if not crushed
          }
          wasMove = false; // get away, we're done with this mobj
          break;
        }

        if (uvlist.length() == 0) continue; // not stuck in this pobj

        const TVec origOrigin = mobj->Origin;
        bool foundGoodUnstuck = false;

        // try to unstick by average vector
        bool tryNonAverage = true;
        {
          TVec nsum(0.0f, 0.0f, 0.0f);
          for (auto &&nfo : uvlist) {
            TVec n = nfo.unorm;
            if (nfo.fromWrongSide) n = -n;
            nsum += n;
          }
          nsum.z = 0.0f; // just in case
          nsum = nsum.normalised();
          if (!nsum.isZero2D()) {
            // good unstuck normal
            // try to move by 1/3 of radius
            const float maxdist = mobj->Radius/3.0f;
            TVec goodPos(0.0f, 0.0f, 0.0f);
            bool goodPosFound = false;
            if (maxdist > 0.0f) {
              //FIXME: use binary search for now
              float maxlow = 0.0f;
              float maxhigh = maxdist;
              #if 0
              GCon->Logf(NAME_Debug, "%s: trying by normal (%g,%g,%g); maxdist=%g", mobj->GetClass()->GetName(), nsum.x, nsum.y, nsum.z, maxhigh);
              #endif
              while (maxlow < maxhigh && maxhigh-maxlow > 0.001f) {
                float maxmid = (maxlow+maxhigh)*0.5f;
                mobj->Origin = origOrigin+nsum*maxmid;
                const bool ok = mobj->CheckRelPosition(tmtrace, mobj->Origin, /*noPickups*/true, /*ignoreMonsters*/true, /*ignorePlayers*/true);
                #if 0
                GCon->Logf(NAME_Debug, "  ok=%d; maxmid=%g; dist=%g", (int)ok, maxmid, (origOrigin-mobj->Origin).length2D());
                #endif
                if (ok) {
                  // not blocked
                  maxhigh = maxmid;
                  goodPosFound = true;
                  goodPos = mobj->Origin;
                  //edata.aflags |= AFF_MOVE;
                  //wasMove = true;
                  //foundGoodUnstuck = true;
                  const float sqdist = (origOrigin-mobj->Origin).length2DSquared();
                  // restore
                  mobj->Origin = origOrigin;
                  if (sqdist <= 0.1f*0.1f) break;
                } else {
                  // blocked
                  maxlow = maxmid;
                }
              }
              if (goodPosFound) {
                #if 0
                GCon->Logf(NAME_Debug, "%s: found by normal (%g,%g,%g); dist=%g", mobj->GetClass()->GetName(), nsum.x, nsum.y, nsum.z, (origOrigin-goodPos).length2D());
                #endif
                mobj->Origin = goodPos;
                edata.aflags |= AFF_MOVE;
                wasMove = true;
                foundGoodUnstuck = true;
                tryNonAverage = false;
              }
            }
          }
        }

        if (tryNonAverage) {
          // sort unstuck vectors by distance
          timsort_r(uvlist.ptr(), uvlist.length(), sizeof(UnstuckInfo), &unstuckVectorCompare, nullptr);

          // try each unstuck vector
          //bool wasAtLeastOneGood = false;
          for (auto &&nfo : uvlist) {
            if (nfo.blockedByOther) continue; // bad line
            const TVec uv = nfo.uv;
            vassert(uv.isValid());
            if (uv.isZero2D()) continue; // just in case
            //wasAtLeastOneGood = true;
            if (dbg_pobj_unstuck_verbose.asBool()) {
              GCon->Logf(NAME_Debug, "mobj '%s' unstuck move: (%g,%g,%g)", edata.mobj->GetClass()->GetName(), uv.x, uv.y, uv.z);
            }
            // need to move
            //TODO: move any touching objects too
            mobj->Origin = origOrigin+uv;
            const bool ok = mobj->CheckRelPosition(tmtrace, mobj->Origin, /*noPickups*/true, /*ignoreMonsters*/true, /*ignorePlayers*/true);
            if (ok) {
              // not blocked, check other linked polyobjects
              edata.aflags |= AFF_MOVE;
              wasMove = true;
              foundGoodUnstuck = true;
              break;
            }
            // restore
            mobj->Origin = origOrigin;
          }
        } // tryNonAverage

        // if we can't find good unstucking move, crush
        if (!foundGoodUnstuck) {
          // totally stuck
          if (mobj->EntityFlags&VEntity::EF_Solid) {
            if (mobj->Level->eventPolyThrustMobj(mobj, TVec(0.0f, 0.0f), po, true/*vertical*/)) return false; // blocked
          } else {
            mobj->Level->eventPolyCrushMobj(mobj, po);
            if (NeedPositionCheck(mobj) && mobj->Health > 0) return false; // blocked if not crushed
          }
          wasMove = false; // get away, we're done with this mobj
          break;
        }
      } // polyobject link loop

      if (!wasMove) {
        // wasn't moved, so no need to check for "still stuck"
        doFinalCheck = false;
        break;
      }
    } // unstuck try loop

    // if we got here, it means that either unstucking is failed, or we're ok
    // if `doFinalCheck` flag is set, we should be stuck
    // check it again, just to be sure
    if (doFinalCheck) {
      for (polyobj_t *po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
        // reject only polyobjects that are above us
        // (otherwise we may miss blockig top texture)
        if (mobj->Origin.z+max2(0.0f, mobj->Height) <= po->pofloor.minz) continue;
        // if pobj has no top-blocking textures, it can be skipped if we're above it
        if ((po->PolyFlags&polyobj_t::PF_HasTopBlocking) == 0) {
          if (mobj->Origin.z+max2(0.0f, mobj->Height) <= po->pofloor.minz) continue;
        }
        const bool canUnstuck = CalcPolyUnstuckVector(uvlist, Level, po, mobj);
        // if we can't find unstuck direction, or found at least one, it means that we're stuck
        if (!canUnstuck || uvlist.length() > 0) {
          if (dbg_pobj_unstuck_verbose.asBool()) {
            GCon->Logf(NAME_Debug, "mobj '%s' STILL BLOCKED", edata.mobj->GetClass()->GetName());
          }
          // still blocked
          if (mobj->EntityFlags&VEntity::EF_Solid) {
            if (mobj->Level->eventPolyThrustMobj(mobj, TVec(0.0f, 0.0f), po, true/*vertical*/)) return false; // blocked
          } else {
            mobj->Level->eventPolyCrushMobj(mobj, po);
            if (NeedPositionCheck(mobj) && mobj->Health > 0) return false; // blocked if not crushed
          }
        }
      }
    }
  }

  // ok to move
  return true;
}


//==========================================================================
//
//  CheckAffectedMObjPositions
//
//  returns `true` if movement was blocked
//
//==========================================================================
static bool CheckAffectedMObjPositions (const polyobj_t *pofirst) noexcept {
  if (poAffectedEnitities.length() == 0) return false;
  (void)pofirst;

  tmtrace_t tmtrace;
  for (auto &&edata : poAffectedEnitities) {
    if (!edata.aflags) continue;
    VEntity *mobj = edata.mobj;
    if (!NeedPositionCheck(mobj)) continue;
    // temporarily disable collision with things
    //FIXME: reenable it when i'll write stacking code
    //!mobj->EntityFlags &= ~VEntity::EF_ColideWithThings;
    bool ok = mobj->CheckRelPosition(tmtrace, mobj->Origin, /*noPickups*/true, /*ignoreMonsters*/true, /*ignorePlayers*/true);
    //!mobj->EntityFlags = oldFlags; // restore flags
    if (!ok) {
      // abort horizontal movement
      if (edata.aflags&AFF_MOVE) {
        if (dbg_pobj_unstuck_verbose.asBool()) {
          GCon->Logf(NAME_Debug, "pobj #%d: HCHECK(0) for %s(%u)", pofirst->tag, mobj->GetClass()->GetName(), mobj->GetUniqueId());
        }
        TVec oo = edata.Origin;
        oo.z = mobj->Origin.z;
        ok = mobj->CheckRelPosition(tmtrace, oo, /*noPickups*/true, /*ignoreMonsters*/true, /*ignorePlayers*/true);
        if (ok) {
          // undo horizontal movement, and allow normal pobj check to crash the object
          if (dbg_pobj_unstuck_verbose.asBool()) {
            GCon->Logf(NAME_Debug, "pobj #%d: HABORT(0) for %s(%u)", pofirst->tag, mobj->GetClass()->GetName(), mobj->GetUniqueId());
          }
          mobj->LastMoveOrigin.z += mobj->Origin.z-oo.z;
          mobj->Origin = oo;
          // link it back, so pobj checker will do its work
          edata.aflags = 0;
          mobj->LinkToWorld();
          continue;
        }
      }
      // alas, blocked
      if (dbg_pobj_unstuck_verbose.asBool()) {
        GCon->Logf(NAME_Debug, "pobj #%d: blocked(0) by %s(%u)", pofirst->tag, mobj->GetClass()->GetName(), mobj->GetUniqueId());
      }
      return true;
    }
    // check ceiling
    if (mobj->Origin.z+max2(0.0f, mobj->Height) > tmtrace.CeilingZ) {
      // alas, height block
      if (dbg_pobj_unstuck_verbose.asBool()) {
        GCon->Logf(NAME_Debug, "pobj #%d: blocked(1) by %s(%u)", pofirst->tag, mobj->GetClass()->GetName(), mobj->GetUniqueId());
      }
      return true;
    }
    // fix z if we can step up
    const float zdiff = tmtrace.FloorZ-mobj->Origin.z;
    //GCon->Logf(NAME_Debug, "%s(%u): zdiff=%g; z=%g; FloorZ=%g; tr.FloorZ=%g", mobj->GetClass()->GetName(), mobj->GetUniqueId(), zdiff, mobj->Origin.z, mobj->FloorZ, tmtrace.FloorZ);
    if (zdiff <= 0.0f) continue;
    if (zdiff <= mobj->MaxStepHeight) {
      // ok, we can step up
      // let physics engine fix it
      if (!mobj->IsPlayer()) mobj->Origin.z = tmtrace.FloorZ;
      continue;
    }
    // cannot step up, rollback horizontal movement
    if (edata.aflags&AFF_MOVE) {
      //GCon->Logf(NAME_Debug, "pobj #%d: HCHECK(1) for %s(%u)", pofirst->tag, mobj->GetClass()->GetName(), mobj->GetUniqueId());
      TVec oo = edata.Origin;
      oo.z = mobj->Origin.z;
      ok = mobj->CheckRelPosition(tmtrace, oo, /*noPickups*/true, /*ignoreMonsters*/true, /*ignorePlayers*/true);
      if (ok) {
        // undo horizontal movement, and allow normal pobj check to crash the object
        //GCon->Logf(NAME_Debug, "pobj #%d: HABORT(1) for %s(%u)", pofirst->tag, mobj->GetClass()->GetName(), mobj->GetUniqueId());
        mobj->LastMoveOrigin.z += mobj->Origin.z-oo.z;
        mobj->Origin = oo;
        // link it back, so pobj checker will do its work
        edata.aflags = 0;
        mobj->LinkToWorld();
        continue;
      }
    }
    // alas, blocked
    if (dbg_pobj_unstuck_verbose.asBool()) {
      GCon->Logf(NAME_Debug, "pobj #%d: blocked(2) by %s(%u)", pofirst->tag, mobj->GetClass()->GetName(), mobj->GetUniqueId());
    }
    return true;
  }

  // done, no blocks
  return false;
}


//==========================================================================
//
//  VLevel::MovePolyobj
//
//==========================================================================
bool VLevel::MovePolyobj (int num, float x, float y, float z, unsigned flags) {
  const bool forcedMove = (flags&POFLAG_FORCED); // forced move is like teleport, it will not affect objects
  const bool skipLink = (flags&POFLAG_NOLINK);

  polyobj_t *po = GetPolyobj(num);
  if (!po) { GCon->Logf(NAME_Error, "MovePolyobj: Invalid pobj #%d", num); return false; }

  if (!po->posector) z = 0.0f;

  polyobj_t *pofirst = po;

  const bool verticalMove = (!forcedMove && z != 0.0f);
  const bool horizMove = (!forcedMove && (x != 0.0f || y != 0.0f));
  const bool docarry = (!forcedMove && horizMove && pofirst->posector && !(pofirst->PolyFlags&polyobj_t::PF_NoCarry));
  bool flatsSaved = false;

  // collect `poAffectedEnitities`, and save planes
  // but do this only if we have to
  // only for 3d pobjs that either can carry objects, or moving vertically
  // (because vertical move always pushing objects)
  poAffectedEnitities.resetNoDtor();
  if (!forcedMove && pofirst->posector && (docarry || verticalMove)) {
    flatsSaved = true;
    IncrementValidCount();
    const int visCount = validcount;
    //GCon->Logf(NAME_Debug, "=== pobj #%d ===", pofirst->tag);
    for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
      po->savedFloor = po->pofloor;
      po->savedCeiling = po->poceiling;
      for (msecnode_t *n = po->posector->TouchingThingList; n; n = n->SNext) {
        VEntity *mobj = n->Thing;
        if (mobj->ValidCount == visCount) continue;
        mobj->ValidCount = visCount;
        if (mobj->IsGoingToDie()) continue;
        // check flags
        if ((mobj->EntityFlags&VEntity::EF_ColideWithWorld) == 0) continue;
        if ((mobj->FlagsEx&(VEntity::EFEX_NoInteraction|VEntity::EFEX_NoTickGrav)) == VEntity::EFEX_NoInteraction) continue;
        //FIXME: ignore everything that cannot possibly be affected?
        //GCon->Logf(NAME_Debug, "  %s(%u): z=%g (poz1=%g); sector=%p; basesector=%p", mobj->GetClass()->GetName(), mobj->GetUniqueId(), mobj->Origin.z, pofirst->poceiling.maxz, mobj->Sector, mobj->BaseSector);
        SavedEntityData &edata = poAffectedEnitities.alloc();
        edata.mobj = mobj;
        edata.Origin = mobj->Origin;
        edata.LastMoveOrigin = mobj->LastMoveOrigin;
        edata.aflags = 0;
      }
    }

    // setup "affected" flags, calculate new z
    const bool verticalMoveUp = (verticalMove && z > 0.0f);
    for (auto &&edata : poAffectedEnitities) {
      VEntity *mobj = edata.mobj;
      for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
        const float oldz = mobj->Origin.z;
        const float pofz = po->poceiling.maxz;
        // fix "affected" flag
        // vertical move goes up, and the platform goes through the object?
        if (verticalMove && oldz > pofz && oldz < pofz+z) {
          // yeah
          if (verticalMoveUp) {
            edata.aflags = AFF_VERT|(docarry ? AFF_MOVE : AFF_NOTHING_ZERO);
            mobj->Origin.z = pofz+z;
          }
        } else if (oldz == pofz) {
          // the object is standing on the pobj ceiling (i.e. floor ;-)
          // always carry, and always move with the pobj, even if it is flying
          edata.aflags = AFF_VERT|(docarry ? AFF_MOVE : AFF_NOTHING_ZERO);
          mobj->Origin.z = pofz+z;
        }
      }
    }

    // now unlink all affected objects, because we'll do "move and check" later
    // also, set move objects horizontally, because why not
    const TVec xymove(x, y);
    for (auto &&edata : poAffectedEnitities) {
      VEntity *mobj = edata.mobj;
      if (edata.aflags) {
        mobj->UnlinkFromWorld();
        if (edata.aflags&AFF_MOVE) mobj->Origin += xymove;
      }
    }
  }

  const bool dovmove = (z != 0.0f); // cannot use `verticalMove` here
  // unlink and move all linked polyobjects
  for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
    UnlinkPolyobj(po);
    const TVec delta(po->startSpot.x+x, po->startSpot.y+y);
    const float an = AngleMod(po->angle);
    float s, c;
    msincos(an, &s, &c);
    // save previous points, move horizontally
    const TVec *origPts = po->originalPts;
    TVec *prevPts = po->prevPts;
    TVec **vptr = po->segPts;
    for (int f = po->segPtsCount; f--; ++vptr, ++origPts, ++prevPts) {
      *prevPts = **vptr;
      //**vptr = (*origPts)+delta;
      TVec np(*origPts);
      // get the original X and Y values
      const float tr_x = np.x;
      const float tr_y = np.y;
      // calculate the new X and Y values
      np.x = (tr_x*c-tr_y*s)+delta.x;
      np.y = (tr_y*c+tr_x*s)+delta.y;
      **vptr = np;
    }
    UpdatePolySegs(po);
    if (dovmove) OffsetPolyobjFlats(po, z);
  }

  bool blocked = false;

  // now check if movement is blocked by any affected object
  // we have to do it after we unlinked all pobjs
  if (!forcedMove && pofirst->posector) {
    blocked = CheckAffectedMObjPositions(pofirst);
    if (dbg_pobj_unstuck_verbose.asBool()) {
      GCon->Logf(NAME_Debug, "pobj #%d: MOVEMENT: blocked=%d", pofirst->tag, (int)blocked);
    }
  }

  if (!forcedMove && !blocked) {
    for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
      blocked = PolyCheckMobjBlocked(po, false);
      if (dbg_pobj_unstuck_verbose.asBool()) {
        GCon->Logf(NAME_Debug, "pobj #%d: MOVEMENT(1): blocked=%d", po->tag, (int)blocked);
      }
      if (blocked) break; // process all?
    }
  }

  bool linked = false;

  // if not blocked, relink polyobject temporarily, and check vertical hits
  if (!blocked && !forcedMove && pofirst->posector) {
    for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
      LinkPolyobj(po);
    }
    linked = true;
    // check height-blocking objects
    for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
      const float pz0 = po->pofloor.minz;
      const float pz1 = po->poceiling.maxz;
      for (msecnode_t *n = po->posector->TouchingThingList; n; n = n->SNext) {
        VEntity *mobj = n->Thing;
        if (mobj->IsGoingToDie()) continue;
        // check flags
        if (!NeedPositionCheck(mobj)) continue;
        //if (mobj->Height <= 0.0f || mobj->Radius <= 0.0f) continue; // done in `NeedPositionCheck()`
        const float mz0 = mobj->Origin.z;
        const float mz1 = mz0+mobj->Height;
        if (mz1 <= pz0) continue;
        if (mz0 >= pz1) continue;
        // vertical intersection, blocked movement
        if (mobj->EntityFlags&VEntity::EF_Solid) {
          //GCon->Logf(NAME_Debug, "pobj #%d hit %s(%u)", po->tag, mobj->GetClass()->GetName(), mobj->GetUniqueId());
          if (mobj->Level->eventPolyThrustMobj(mobj, TVec(0.0f, 0.0f), po, true/*vertical*/)) blocked = true;
        } else {
          mobj->Level->eventPolyCrushMobj(mobj, po);
        }
        if (blocked) break;
      }
      if (blocked) break;
    }
    // if blocked, unlink back
    if (blocked) {
      for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
        UnlinkPolyobj(po);
      }
      linked = false;
    }
  }

  // restore position if blocked
  if (blocked) {
    vassert(!linked);
    // undo polyobject movement, and link them back
    for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
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
    // restore and relink all mobjs back
    for (auto &&edata : poAffectedEnitities) {
      VEntity *mobj = edata.mobj;
      if (!mobj->IsGoingToDie() && edata.aflags) {
        mobj->LastMoveOrigin = edata.LastMoveOrigin;
        mobj->Origin = edata.Origin;
        mobj->LinkToWorld();
      }
    }
    return false;
  }

  // succesfull move, fix startspot
  for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
    po->startSpot.x += x;
    po->startSpot.y += y;
    po->startSpot.z += z;
    OffsetPolyobjFlats(po, 0.0f, true);
    if (!linked) LinkPolyobj(po);
    // move decals
    if (!forcedMove && po->Is3D() && subsectorDecalList) {
      for (subsector_t *posub = po->GetSector()->subsectors; posub; posub = posub->seclink) {
        const unsigned psnum = (unsigned)(ptrdiff_t)(posub-&Subsectors[0]);
        VDecalList *lst = &subsectorDecalList[psnum];
        for (decal_t *dc = lst->head; dc; dc = dc->subnext) {
          dc->worldx += x;
          dc->worldy += y;
        }
      }
    }
  }

  // relink all mobjs back
  for (auto &&edata : poAffectedEnitities) {
    VEntity *mobj = edata.mobj;
    if (!mobj->IsGoingToDie() && edata.aflags) {
      mobj->LastMoveOrigin += mobj->Origin-edata.Origin;
      mobj->LinkToWorld();
    }
  }

  // notify renderer that this polyobject was moved
  #ifdef CLIENT
  if (Renderer) {
    for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
      Renderer->PObjModified(po);
      Renderer->InvalidatePObjLMaps(po);
    }
  }
  #endif

  return true;
}


//==========================================================================
//
//  VLevel::RotatePolyobj
//
//==========================================================================
bool VLevel::RotatePolyobj (int num, float angle, unsigned flags) {
  const bool forcedMove = (flags&POFLAG_FORCED);
  const bool skipLink = (flags&POFLAG_NOLINK);
  const bool indRot = (flags&POFLAG_INDROT);

  // get the polyobject
  polyobj_t *po = GetPolyobj(num);
  if (!po) { GCon->Logf(NAME_Error, "RotatePolyobj: Invalid pobj #%d", num); return false; }

  polyobj_t *pofirst = po;

  const bool docarry = (!forcedMove && pofirst->posector && !(po->PolyFlags&polyobj_t::PF_NoCarry));
  const bool doangle = (!forcedMove && pofirst->posector && !(po->PolyFlags&polyobj_t::PF_NoAngleChange));
  const bool flatsSaved = (!forcedMove && pofirst->posector);

  float s, c;

  // collect objects we need to move/rotate
  poAffectedEnitities.resetNoDtor();
  if (!forcedMove && pofirst->posector && (docarry || doangle)) {
    IncrementValidCount();
    const int visCount = validcount;
    for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
      const float pz1 = po->poceiling.maxz;
      for (msecnode_t *n = po->posector->TouchingThingList; n; n = n->SNext) {
        VEntity *mobj = n->Thing;
        if (mobj->ValidCount == visCount) continue;
        mobj->ValidCount = visCount;
        if (mobj->IsGoingToDie()) continue;
        if (dbg_pobj_unstuck_verbose.asBool()) {
          GCon->Logf(NAME_Debug, "ROTATION: pobj #%d: checking mobj '%s' (z=%g; pz1=%g)", po->tag, mobj->GetClass()->GetName(), mobj->Origin.z, pz1);
        }
        // check flags
        if ((mobj->EntityFlags&VEntity::EF_ColideWithWorld) == 0) continue;
        if ((mobj->FlagsEx&(VEntity::EFEX_NoInteraction|VEntity::EFEX_NoTickGrav)) == VEntity::EFEX_NoInteraction) continue;
        if (mobj->Origin.z != pz1) {
          mobj->ValidCount = 0; // it may be affected by another pobj
          continue; // just in case
        }
        SavedEntityData &edata = poAffectedEnitities.alloc();
        edata.mobj = mobj;
        edata.Origin = mobj->Origin;
        edata.LastMoveOrigin = mobj->LastMoveOrigin;
        // we need to remember rotation point
        //FIXME: this will glitch with objects standing on some multipart platforms
        edata.spot = (indRot ? po->startSpot : pofirst->startSpot); // mobj->Sector->ownpobj
        edata.aflags = 0;
        if (dbg_pobj_unstuck_verbose.asBool()) {
          GCon->Logf(NAME_Debug, "ROTATION: pobj #%d: mobj '%s' is affected", po->tag, mobj->GetClass()->GetName());
        }
        // check if it is initially stuck
        #if 1
        if (NeedPositionCheck(mobj)) {
          for (polyobj_t *xpp = pofirst; xpp; xpp = (skipLink ? nullptr : xpp->polink)) {
            if (mobj->Origin.z >= po->poceiling.maxz || mobj->Origin.z+max2(0.0f, mobj->Height) <= po->pofloor.minz) {
              // do nothing
            } else {
              for (auto &&lxx : xpp->LineFirst()) {
                const line_t *ld = lxx.line();
                if (!mobj->IsBlockingLine(ld)) continue;
                if (!mobj->LineIntersects(ld)) continue;
                edata.aflags |= AFF_STUCK;
                break;
              }
            }
            if (edata.aflags&AFF_STUCK) break;
          }
        }
        #endif
      }
    }

    // now unlink all affected objects, because we'll do "move and check" later
    // also, rotate the objects
    msincos(AngleMod(angle), &s, &c);
    for (auto &&edata : poAffectedEnitities) {
      VEntity *mobj = edata.mobj;
      mobj->UnlinkFromWorld();
      if (docarry) {
        const float ssx = edata.spot.x;
        const float ssy = edata.spot.y;
        // rotate around polyobject spot point
        const float xc = mobj->Origin.x-ssx;
        const float yc = mobj->Origin.y-ssy;
        //GCon->Logf(NAME_Debug, "%s(%u): oldrelpos=(%g,%g)", mobj->GetClass()->GetName(), mobj->GetUniqueId(), xc, yc);
        // calculate the new X and Y values
        const float nx = (xc*c-yc*s);
        const float ny = (yc*c+xc*s);
        //GCon->Logf(NAME_Debug, "%s(%u): newrelpos=(%g,%g)", mobj->GetClass()->GetName(), mobj->GetUniqueId(), nx, ny);
        const float dx = (nx+ssx)-mobj->Origin.x;
        const float dy = (ny+ssy)-mobj->Origin.y;
        if (dx != 0.0f || dy != 0.0f) {
          mobj->Origin.x += dx;
          mobj->Origin.y += dy;
          edata.aflags |= AFF_MOVE;
        }
      }
    }
  }

  // rotate all polyobjects
  for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
    /*if (IsForServer())*/ UnlinkPolyobj(po);
    po->origStartSpot = po->startSpot;
    if (!indRot && po != pofirst) {
      // rotate this object's starting spot around the main object starting spot
      msincos(angle, &s, &c);
      const float sp_x = po->startSpot.x-pofirst->startSpot.x;
      const float sp_y = po->startSpot.y-pofirst->startSpot.y;
      // calculate the new X and Y values
      const float nx = (sp_x*c-sp_y*s)+pofirst->startSpot.x;
      const float ny = (sp_y*c+sp_x*s)+pofirst->startSpot.y;
      po->startSpot.x = nx;
      po->startSpot.y = ny;
    }
    const float ssx = po->startSpot.x;
    const float ssy = po->startSpot.y;
    const float an = AngleMod(po->angle+angle);
    msincos(an, &s, &c);
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
      float tr_x = origPts->x;
      float tr_y = origPts->y;
      /*
      if (!indRot && po != pofirst) {
        tr_x += po->startSpot.x-ssx;
        tr_y += po->startSpot.y-ssy;
      }
      */
      // calculate the new X and Y values
      (*vptr)->x = (tr_x*c-tr_y*s)+ssx;
      (*vptr)->y = (tr_y*c+tr_x*s)+ssy;
    }
    UpdatePolySegs(po);
  }

  bool blocked = false;

  if (!forcedMove) {
    // now check if movement is blocked by any affected object
    // we have to do it after we unlinked all pobjs
    if (!forcedMove && pofirst->posector) {
      blocked = CheckAffectedMObjPositions(pofirst);
      if (dbg_pobj_unstuck_verbose.asBool()) {
        GCon->Logf(NAME_Debug, "pobj #%d: ROTATION: blocked=%d", pofirst->tag, (int)blocked);
      }
      if (!blocked) blocked = !UnstuckFromRotatedPObj(this, pofirst, skipLink);
    }
    if (!blocked) {
      for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
        blocked = PolyCheckMobjBlocked(po, true);
        if (dbg_pobj_unstuck_verbose.asBool()) {
          GCon->Logf(NAME_Debug, "pobj #%d: ROTATION(1): blocked=%d", po->tag, (int)blocked);
        }
        if (blocked) break; // process all?
      }
    }
  }

  // if we are blocked then restore the previous points
  if (blocked) {
    // restore points
    for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
      if (flatsSaved) {
        po->pofloor = po->savedFloor;
        po->poceiling = po->savedCeiling;
      }
      po->startSpot = po->origStartSpot;
      TVec *prevPts = po->prevPts;
      TVec **vptr = po->segPts;
      for (int f = po->segPtsCount; f--; ++vptr, ++prevPts) **vptr = *prevPts;
      UpdatePolySegs(po);
      LinkPolyobj(po);
    }
    // restore and relink all mobjs back
    for (auto &&edata : poAffectedEnitities) {
      VEntity *mobj = edata.mobj;
      if (!mobj->IsGoingToDie()) {
        mobj->LastMoveOrigin = edata.LastMoveOrigin;
        mobj->Origin = edata.Origin;
        mobj->LinkToWorld();
      }
    }
    return false;
  }

  // not blocked, fix angles and floors, rotate decals
  // also, fix starting spots for non-independent rotation
  for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
    /*
    // fix start spots
    if (!indRot && po != pofirst) {
      // rotate this object's starting spot around the main object starting spot
      msincos(angle, &s, &c);
      const float tr_x = po->startSpot.x-pofirst->startSpot.x;
      const float tr_y = po->startSpot.y-pofirst->startSpot.y;
      // calculate the new X and Y values
      const float nx = (tr_x*c-tr_y*s)+pofirst->startSpot.x;
      const float ny = (tr_y*c+tr_x*s)+pofirst->startSpot.y;
      po->startSpot.x = nx;
      po->startSpot.y = ny;
    }
    */
    po->angle = AngleMod(po->angle+angle);
    if (flatsSaved) {
      po->pofloor.BaseAngle = AngleMod(po->pofloor.BaseAngle+angle);
      po->poceiling.BaseAngle = AngleMod(po->poceiling.BaseAngle+angle);
      OffsetPolyobjFlats(po, 0.0f, true);
    }
    LinkPolyobj(po);
    // move decals
    if (!forcedMove && po->Is3D() && subsectorDecalList) {
      for (subsector_t *posub = po->GetSector()->subsectors; posub; posub = posub->seclink) {
        const unsigned psnum = (unsigned)(ptrdiff_t)(posub-&Subsectors[0]);
        VDecalList *lst = &subsectorDecalList[psnum];
        if (!lst->head) continue;
        msincos(AngleMod(angle), &s, &c);
        const float ssx = (indRot ? po->startSpot.x : pofirst->startSpot.x);
        const float ssy = (indRot ? po->startSpot.y : pofirst->startSpot.y);
        for (decal_t *dc = lst->head; dc; dc = dc->subnext) {
          const float xc = dc->worldx-ssx;
          const float yc = dc->worldy-ssy;
          const float nx = (xc*c-yc*s);
          const float ny = (yc*c+xc*s);
          dc->worldx = nx+ssx;
          dc->worldy = ny+ssy;
          dc->angle = AngleMod(dc->angle+angle);
        }
      }
    }
  }

  // relink things
  if (!forcedMove && poAffectedEnitities.length()) {
    for (auto &&edata : poAffectedEnitities) {
      VEntity *mobj = edata.mobj;
      if (mobj->IsGoingToDie()) continue;
      if (doangle) mobj->Level->eventPolyRotateMobj(mobj, angle);
      mobj->LastMoveOrigin += mobj->Origin-edata.Origin;
      mobj->LinkToWorld();
    }
  }

  // notify renderer that this polyobject was moved
  #ifdef CLIENT
  if (Renderer) {
    for (po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
      Renderer->PObjModified(po);
      Renderer->InvalidatePObjLMaps(po);
    }
  }
  #endif

  return true;
}


//==========================================================================
//
//  VLevel::PolyCheckMobjLineBlocking
//
//==========================================================================
bool VLevel::PolyCheckMobjLineBlocking (const line_t *ld, polyobj_t *po, bool /*rotation*/) {
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
        //if (!(mobj->EntityFlags&VEntity::EF_ColideWithWorld)) continue;
        //if (!(mobj->EntityFlags&(VEntity::EF_Solid|VEntity::EF_Corpse))) continue;
        if (!NeedPositionCheck(mobj)) continue;

        // check mobj height (pobj floor and ceiling shouldn't be sloped here)
        //FIXME: check height for 3dmitex pobj
        //bool ldblock = false;

        if (!checkPObjLineBlocked(po, mobj, ld)) continue;

        if (dbg_pobj_unstuck_verbose.asBool()) {
          GCon->Logf(NAME_Debug, "mobj '%s': blocked by pobj %d, line #%d", mobj->GetClass()->GetName(), po->tag, (int)(ptrdiff_t)(ld-&mobj->XLevel->Lines[0]));
        }

        /*
        if (po->posector) {
          if (mobj->Origin.z >= po->poceiling.maxz) continue; // fully above
          if (mobj->Origin.z+max2(0.0f, mobj->Height) <= po->pofloor.minz) continue; // fully below
          if (!check3DPObjLineBlocked(po, mobj, ld)) continue;
        } else {
          // check for non-3d pobj with midtex
          if ((ld->flags&(ML_TWOSIDED|ML_3DMIDTEX)) == (ML_TWOSIDED|ML_3DMIDTEX)) {
            if (!mobj->LineIntersects(ld)) continue;
            // use front side
            const int side = 0; //ld->PointOnSide(mobj->Origin);
            float pz0 = 0.0f, pz1 = 0.0f;
            if (!GetMidTexturePosition(ld, side, &pz0, &pz1)) continue; // no middle texture
            if (mobj->Origin.z >= pz1 || mobj->Origin.z+max2(0.0f, mobj->Height) <= pz0) continue; // no collision
            //ldblock = true;
          } else {
            if (!mobj->IsBlockingLine(ld)) continue;
            if (!mobj->LineIntersects(ld)) continue;
          }
        }
        */

        /*
        if (!ldblock) {
          if (!mobj->IsBlockingLine(ld)) continue;
          if (!mobj->LineIntersects(ld)) continue;
        }
        */

        //TODO: crush corpses!
        //TODO: crush objects with platforms

        if (mobj->EntityFlags&VEntity::EF_Solid) {
          //GCon->Logf(NAME_Debug, "pobj #%d hit %s(%u)", po->tag, mobj->GetClass()->GetName(), mobj->GetUniqueId());
          if (mobj->Level->eventPolyThrustMobj(mobj, ld->normal, po, false/*non-vertical*/)) blocked = true;
        } else {
          mobj->Level->eventPolyCrushMobj(mobj, po);
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
bool VLevel::PolyCheckMobjBlocked (polyobj_t *po, bool rotation) {
  if (!po || po->numlines == 0) return false;
  bool blocked = false;
  line_t **lineList = po->lines;
  for (int count = po->numlines; count; --count, ++lineList) {
    if (PolyCheckMobjLineBlocking(*lineList, po, rotation)) blocked = true; //k8: break here?
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

//native final bool MovePolyobj (int num, float x, float y, optional float z, optional int flags);
IMPLEMENT_FUNCTION(VLevel, MovePolyobj) {
  int tag;
  float x, y;
  VOptParamFloat z(0);
  VOptParamInt flags(0);
  vobjGetParamSelf(tag, x, y, z, flags);
  RET_BOOL(Self->MovePolyobj(tag, x, y, z, flags));
}

//native final bool RotatePolyobj (int num, float angle, optional int flags);
IMPLEMENT_FUNCTION(VLevel, RotatePolyobj) {
  int tag;
  float angle;
  VOptParamInt flags(0);
  vobjGetParamSelf(tag, angle, flags);
  RET_BOOL(Self->RotatePolyobj(tag, angle, flags));
}
