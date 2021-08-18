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
#include "p_decal.h"
#ifdef CLIENT
# include "../drawer.h"
#endif


enum {
  PO_3D_LINK_SEQ = 9367,
  PO_3D_LINK = 9368,
};


TArray<VEntity *> VLevel::poRoughEntityList; // moved to VLevel as static


//==========================================================================
//
//  CollectPObjTouchingThingsRoughBlockmap
//
//  collect all objects from blockmap cells this polyobject may touch
//  puts it in `entList`
//
//  pobj bounding box must be valid
//
//==========================================================================
static VVA_OKUNUSED void CollectTouchingThingsRoughBlockmap (TArray<VEntity *> &entList, VLevel *XLevel, const float bbox2d[4], bool clearList=true) {
  if (clearList) {
    entList.reset();
    // do it anyway, because we may rely on the fact that `validcount` is incremented
    XLevel->IncrementValidCount();
  }

  DeclareMakeBlockMapCoordsBBox2DMaxRadius(bbox2d, cleft, cbottom, cright, ctop);

  const int bwdt = XLevel->BlockMapWidth;
  if (ctop < 0 || cright < 0 || cbottom >= XLevel->BlockMapHeight || cleft >= bwdt) return;
  const int bhgt = XLevel->BlockMapHeight;

  const int bottom = max2(cbottom, 0);
  const int top = min2(ctop, bhgt-1);
  const int left = max2(cleft, 0);
  const int right = min2(cright, bwdt-1);
  const int visCount = validcount;

  const int ey = top*bwdt;
  for (int by = bottom*bwdt; by <= ey; by += bwdt) {
    for (int bx = left; bx <= right; ++bx) {
      for (VEntity *mobj = XLevel->BlockLinks[by+bx]; mobj; mobj = mobj->BlockMapNext) {
        if (mobj->IsGoingToDie()) continue;
        if (mobj->ValidCount == visCount) continue;
        mobj->ValidCount = visCount;
        entList.append(mobj);
      }
    }
  }
}


//==========================================================================
//
//  CollectPObjTouchingThingsRoughSectors
//
//  collect all objects from sectors this polyobject may touch
//  puts it in `entList`
//
//  this is done by checking each sector this pobj possibly touches,
//  so it should be done after calling `PutPObjInSubsectors()`
//
//  this also checks pobj bounding box, to skip mobjs that cannot be touched
//
//==========================================================================
static VVA_OKUNUSED void CollectPObjTouchingThingsRoughSectors (TArray<VEntity *> &entList, VLevel *XLevel, polyobj_t *po, bool clearList=true) {
  if (clearList) {
    entList.reset();
    // do it anyway, because we may rely on the fact that `validcount` is incremented
    XLevel->IncrementValidCount();
  }

  const int visCount = validcount;

  // touching sectors
  for (polyobjpart_t *part = po->parts; part; part = part->nextpobj) {
    sector_t *sec = part->sub->sector;
    if (sec->isAnyPObj()) continue; // just in case
    if (sec->validcount == visCount) continue;
    sec->validcount = visCount;
    // new sector, process things
    for (msecnode_t *n = sec->TouchingThingList; n; n = n->SNext) {
      VEntity *mobj = n->Thing;
      if (mobj->IsGoingToDie()) continue;
      if (mobj->ValidCount == visCount) continue;
      mobj->ValidCount = visCount;
      // check bounding box
      if (!IsAABBInside2DBBox(mobj->Origin.x, mobj->Origin.y, mobj->GetMoveRadius(), po->bbox2d)) continue;
      entList.append(mobj);
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
  auto pip = PolyTagMap->get(polyNum);
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
//  VLevel::InitPolyBlockMap
//
//==========================================================================
void VLevel::InitPolyBlockMap () {
  PolyBlockMap = new polyblock_t*[BlockMapWidth*BlockMapHeight];
  memset((void *)PolyBlockMap, 0, sizeof(polyblock_t *)*BlockMapWidth*BlockMapHeight);
  for (int i = 0; i < NumPolyObjs; ++i) LinkPolyobj(PolyObjs[i], true);
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
//  VLevel::LinkPolyobj
//
//==========================================================================
void VLevel::LinkPolyobj (polyobj_t *po, bool relinkMObjs) {
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
  if (relinkMObjs && po->posector) {
    CollectPObjTouchingThingsRoughSectors(poRoughEntityList, this, po);
    // relink all things, so they can get pobj info right
    for (VEntity *mobj : poRoughEntityList) {
      //GCon->Logf(NAME_Debug, "pobj #%d: relinking entity %s(%u)", po->tag, mobj->GetClass()->GetName(), mobj->GetUniqueId());
      mobj->LinkToWorld(); // relink it
    }
  }
}


//==========================================================================
//
//  VLevel::Link3DPolyobjMObjs
//
//==========================================================================
void VLevel::Link3DPolyobjMObjs (polyobj_t *pofirst, bool skipLink) {
  bool first = true;
  for (polyobj_t *po = pofirst; po; po = (skipLink ? nullptr : po->polink)) {
    if (!po->posector) continue;
    CollectPObjTouchingThingsRoughSectors(poRoughEntityList, this, po, first);
    first = false;
  }
  for (VEntity *mobj : poRoughEntityList) mobj->LinkToWorld(); // relink
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
//  VLevel::Add3DPolyobjLink
//
//==========================================================================
void VLevel::Add3DPolyobjLink (mthing_t *thing, int srcpid, int destpid) {
  if (!thing) Host_Error("Add3DPolyobjLink(): called without a thing");
  vassert(thing);

  bool isSeq = false;
       if (thing->type == PO_3D_LINK) isSeq = false;
  else if (thing->type == PO_3D_LINK_SEQ) isSeq = true;
  else Host_Error("Add3DPolyobjLink(): called with invalid thing");

  if (!isSeq) {
    // normal link
    if (!srcpid || !destpid) return;
  } else {
    // sequence link
    if (srcpid == destpid) return;
  }

  const int idx = NumPolyLinks3D++;

  VPolyLink3D *nl = new VPolyLink3D[NumPolyLinks3D];
  for (int f = 0; f < NumPolyLinks3D-1; ++f) nl[f] = PolyLinks3D[f];
  delete[] PolyLinks3D;
  PolyLinks3D = nl;

  nl[idx].srcpid = srcpid;
  nl[idx].destpid = destpid;
  nl[idx].flags = (isSeq ? VPolyLink3D::Flag_Sequence : VPolyLink3D::Flag_NothingZero);
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
        pdir = pdir.normalise();
        center += lastv1;
        count = 1;
      }
    } else {
      // are we making a turn here?
      const TVec xdir = ((*ld->v2)-lastv1).normalise();
      if (fabsf(xdir.dot(pdir)) < 1.0f-0.0001f) {
        // looks like we did made a turn
        // we have a new point
        // remember it
        lastv1 = *ld->v1;
        // add it to the sum
        center += lastv1;
        ++count;
        // and remember new direction
        pdir = ((*ld->v2)-lastv1).normalise();
      }
    }
  }
  // if we have less than three points, something's gone wrong...
  if (count < 3) return TVec::ZeroVector;
  center = center/(float)count;
  return center;
  #endif
}
