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
//  VLevel::CalcLine
//
//==========================================================================
void VLevel::CalcLine (line_t *line) {
  // calc line's slopetype
  line->dir = (*line->v2)-(*line->v1);
  line->dir.z = 0.0f;

  if (line->dir.x == 0.0f) {
    line->slopetype = ST_VERTICAL;
    if (line->dir.y == 0.0f) line->dir.x = 1.0f; // fix invalid dir
  } else if (line->dir.y == 0.0f) {
    line->slopetype = ST_HORIZONTAL;
  } else {
    line->slopetype = (line->dir.y*line->dir.x >= 0.0f ? ST_POSITIVE : ST_NEGATIVE);
  }

  line->SetPointDirXY(*line->v1, line->dir);
  line->ndir = line->dir.normalise2D();

  // calc line's bounding box
  line->bbox2d[BOX2D_LEFT] = min2(line->v1->x, line->v2->x);
  line->bbox2d[BOX2D_RIGHT] = max2(line->v1->x, line->v2->x);
  line->bbox2d[BOX2D_BOTTOM] = min2(line->v1->y, line->v2->y);
  line->bbox2d[BOX2D_TOP] = max2(line->v1->y, line->v2->y);

  CalcLineCDPlanes(line);
}


//==========================================================================
//
//  VLevel::CalcSegLenOfs
//
//  only length and offset
//
//==========================================================================
void VLevel::CalcSegLenOfs (seg_t *seg) {
  const line_t *ldef = seg->linedef;
  if (ldef) {
    const TVec *vv = (seg->side ? ldef->v2 : ldef->v1);
    seg->offset = seg->v1->distanceTo2D(*vv);
  } else {
    seg->offset = 0.0f; // just in case
  }
  seg->length = seg->v2->distanceTo2D(*seg->v1);
  if (!isFiniteF(seg->length)) seg->length = 0.0f; // just in case
}


//==========================================================================
//
//  VLevel::CalcSegPlaneDir
//
//==========================================================================
void VLevel::CalcSegPlaneDir (seg_t *seg) {
  bool valid = (isFiniteF(seg->length) && seg->length >= 0.0001f);

  // use line plane, if possible
  const line_t *line = seg->linedef;
  if (line) {
    seg->normal = line->normal;
    seg->dist = line->dist;
    seg->ndir = line->ndir;
    if (seg->side == 1) {
      seg->FlipInPlace();
      seg->ndir = -seg->ndir;
    }
  } else if (valid) {
    seg->Set2Points(*seg->v1, *seg->v2);
    if (seg->v1->x == seg->v2->x) {
      // vertical
      if (seg->v1->y == seg->v2->y) {
        valid = false;
      } else {
        seg->ndir = TVec(0.0f, (seg->v1->y < seg->v2->y ? 1.0f : -1.0f), 0.0f);
      }
    } else if (seg->v1->y == seg->v2->y) {
      // horizontal
      seg->ndir = TVec((seg->v1->x < seg->v2->x ? 1.0f : -1.0f), 0.0f, 0.0f);
    } else {
      // sloped
      seg->ndir = ((*seg->v2)-(*seg->v1)).normalise2D();
    }
  }

  if (valid && (!seg->ndir.isValid() || seg->ndir.isZero2D())) valid = false;

  if (!valid) {
    GCon->Logf(NAME_Warning, "ZERO-LENGTH %sseg #%d (flags: 0x%04x)!", (seg->linedef ? "" : "mini"), (int)(ptrdiff_t)(seg-Segs), (unsigned)seg->flags);
    GCon->Logf(NAME_Warning, "  verts: (%g,%g,%g)-(%g,%g,%g)", seg->v1->x, seg->v1->y, seg->v1->z, seg->v2->x, seg->v2->y, seg->v2->z);
    GCon->Logf(NAME_Warning, "  offset: %g", seg->offset);
    GCon->Logf(NAME_Warning, "  length: %g", seg->length);
    if (seg->linedef) {
      GCon->Logf(NAME_Warning, "  linedef: %d", (int)(ptrdiff_t)(seg->linedef-Lines));
      GCon->Logf(NAME_Warning, "  sidedef: %d (side #%d)", (int)(ptrdiff_t)(seg->sidedef-Sides), seg->side);
      GCon->Logf(NAME_Warning, "  front sector: %d", (int)(ptrdiff_t)(seg->frontsector-Sectors));
      if (seg->backsector) GCon->Logf(NAME_Warning, "  back sector: %d", (int)(ptrdiff_t)(seg->backsector-Sectors));
    }
    if (seg->partner) GCon->Logf(NAME_Warning, "  partner: %d", (int)(ptrdiff_t)(seg->partner-Segs));
    if (seg->frontsub) GCon->Logf(NAME_Warning, "  frontsub: %d", (int)(ptrdiff_t)(seg->frontsub-Subsectors));

    seg->flags |= SF_ZEROLEN;
    if (!isFiniteF(seg->offset)) seg->offset = 0.0f;
    seg->length = 0.0001f;
    // setup fake seg's plane params
    seg->normal = TVec(1.0f, 0.0f, 0.0f);
    seg->dist = 0.0f;
    seg->ndir = TVec(1.0f, 0.0f, 0.0f); // arbitrary
  } else {
    seg->flags &= ~SF_ZEROLEN;
  }
}


//==========================================================================
//
//  VLevel::ClipPObjSegToSub
//
//  returns `false` if seg is out of subsector
//
//  WARNING! this WILL MODIFY `seg->v1` and `seg->v2`!
//  will not modify `drawsegs`, etc.
//
//  if polyobject seg cannot obscure the sector, it won't be created
//  also, no need to create segs with see-through textures
//
//==========================================================================
bool VLevel::ClipPObjSegToSub (const subsector_t *sub, seg_t *seg) noexcept {
  if (!sub || !seg || !seg->sidedef || !seg->linedef) return false; // do not clip with minisegs too
  if (sub->numlines < 3) return false; // oops, cannot clip

  const sector_t *fsec = sub->sector;
  if (!fsec) return false;

  const float minz = fsec->floor.minz;
  const float maxz = fsec->ceiling.maxz;
  if (minz >= maxz) return false; // closed something, don't bother

  // check texture
  if (GTextureManager.GetTextureType(seg->sidedef->MidTexture) != VTextureManager::TCT_SOLID) return false;

  // do not clip with translucent lines
  if (seg->linedef->alpha < 1.0f || (seg->linedef->flags&ML_ADDITIVE)) return false;

  polyobj_t *po = seg->pobj;
  vassert(po);

  const float pominz = po->pofloor.minz;
  const float pomaxz = po->poceiling.maxz;
  if (pominz > minz || pomaxz < maxz) return false;

  TVec *v1 = seg->v1;
  TVec *v2 = seg->v2;

  if (v1->x == v2->x && v1->y == v2->y) return false; // too short, ignore it

  // check if the seg is outside of subsector bounding box
  if (min2(v1->x, v2->x) > sub->bbox2d[BOX2D_MAXX]) return false;
  if (min2(v1->y, v2->y) > sub->bbox2d[BOX2D_MAXY]) return false;
  if (max2(v1->x, v2->x) < sub->bbox2d[BOX2D_MINX]) return false;
  if (max2(v1->y, v2->y) < sub->bbox2d[BOX2D_MINY]) return false;

  float lensq = seg->length*seg->length;

  seg_t *curseg = &Segs[sub->firstline];
  for (int f = sub->numlines; f--; ++curseg) {
    // determine sides for each point
    const float dot1 = curseg->PointDistance(*v1);
    const float dot2 = curseg->PointDistance(*v2);
    // if both dots are outside, the whole seg is outside of subsector
    if (dot1 < -0.01f && dot2 < -0.01f) return false;
    // if both dots are inside (or coplanar), ignore this plane
    if (dot1 >= 0.0f && dot2 >= 0.0f) continue;
    // need to be split
    if (dot1 < -0.01f) {
      vassert(dot2 >= -0.01f);
      // if one of normal components is |1|, the other is inevitably `0`
      const float idist = dot1/(dot1-max2(dot2, 0.0f));
      v1->x = (fabsf(curseg->normal.x) != 1.0f ? v1->x+(v2->x-v1->x)*idist : (curseg->normal.x > 0.0f ? curseg->dist : -curseg->dist));
      v1->y = (fabsf(curseg->normal.y) != 1.0f ? v1->y+(v2->y-v1->y)*idist : (curseg->normal.y > 0.0f ? curseg->dist : -curseg->dist));
    } else if (dot2 < -0.01f) {
      vassert(dot1 >= -0.01f);
      // if one of normal components is |1|, the other is inevitably `0`
      const float idist = dot2/(dot2-max2(dot1, 0.0f));
      v2->x = (fabsf(curseg->normal.x) != 1.0f ? v2->x+(v1->x-v2->x)*idist : (curseg->normal.x > 0.0f ? curseg->dist : -curseg->dist));
      v2->y = (fabsf(curseg->normal.y) != 1.0f ? v2->y+(v1->y-v2->y)*idist : (curseg->normal.y > 0.0f ? curseg->dist : -curseg->dist));
    } else {
      // this can happen for very small distances
      //Sys_Error("ClipPObjSegToSub: oops!");
      continue;
    }
    const float dx = v2->x-v1->x;
    const float dy = v2->y-v1->y;
    lensq = dx*dx+dy*dy;
    if (lensq <= 0.001f*0.001f) return false; // too short
  }

  CalcSegLenOfs(seg);
  CalcSegPlaneDir(seg);
  return true;
}


//==========================================================================
//
//  VLevel::CheckBSPB2DBoxNode
//
//==========================================================================
bool VLevel::CheckBSPB2DBoxNode (int bspnum, const float bbox2d[4], bool (*cb) (VLevel *level, subsector_t *sub, void *udata), void *udata) noexcept {
 tailcall:
  // found a subsector?
  if (BSPIDX_IS_NON_LEAF(bspnum)) {
    // nope
    const node_t *bsp = &Nodes[bspnum];
    const int side = bsp->checkBox2DEx(bbox2d);
    switch (side) {
      case TPlane::OUTSIDE: // on a back side
        bspnum = bsp->children[1];
        goto tailcall;
      case TPlane::INSIDE: // on a front side
        bspnum = bsp->children[0];
        goto tailcall;
      default: // check both sides
        // the order doesn't matter here, because we aren't doing any culling
        if (Are3DAnd2DBBoxesOverlap(bsp->bbox[0], bbox2d)) {
          if (Are3DAnd2DBBoxesOverlap(bsp->bbox[1], bbox2d)) {
            if (!CheckBSPB2DBoxNode(bsp->children[1], bbox2d, cb, udata)) return false;
          }
          bspnum = bsp->children[0];
          goto tailcall;
        } else if (Are3DAnd2DBBoxesOverlap(bsp->bbox[1], bbox2d)) {
          bspnum = bsp->children[1];
          goto tailcall;
        }
    }
  } else {
    // check subsector
    subsector_t *sub = &Subsectors[BSPIDX_LEAF_SUBSECTOR(bspnum)];
    // do not call with original polyobject sectors
    if (!sub->isAnyPObj()) return cb(this, sub, udata);
  }
  return true; // keep checking
}


//==========================================================================
//
//  VLevel::CheckBSPB2DBox
//
//==========================================================================
void VLevel::CheckBSPB2DBox (const float bbox2d[4], bool (*cb) (VLevel *level, subsector_t *sub, void *udata), void *udata) noexcept {
  if (!cb) return;

  if (NumSubsectors < 2) {
    if (NumSubsectors == 1) cb(this, &Subsectors[0], udata);
    return;
  }
  vassert(NumNodes > 0);

  //GCon->Logf(NAME_Debug, "linking pobj #%d (%g,%g)-(%g,%g)", po->tag, po->bbox2d[BOX2D_MINX], po->bbox2d[BOX2D_MINY], po->bbox2d[BOX2D_MAXX], po->bbox2d[BOX2D_MAXY]);
  return (void)CheckBSPB2DBoxNode(NumNodes-1, bbox2d, cb, udata);
}
