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
  } else if (line->dir.y == 0.0f) {
    line->slopetype = ST_HORIZONTAL;
  } else {
    line->slopetype = (line->dir.y*line->dir.x >= 0.0f ? ST_POSITIVE : ST_NEGATIVE);
  }

  line->SetPointDirXY(*line->v1, line->dir);
  line->ndir = line->dir.normalised2D();

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
    seg->offset = seg->v1->DistanceTo2D(*vv);
  }
  seg->length = seg->v2->DistanceTo2D(*seg->v1);
  if (!isFiniteF(seg->length)) seg->length = 0.0f; // just in case
}


//==========================================================================
//
//  VLevel::CalcSeg
//
//==========================================================================
void VLevel::CalcSeg (seg_t *seg) {
  seg->Set2Points(*seg->v1, *seg->v2);
  bool valid = (isFiniteF(seg->length) && seg->length >= 0.0001f);
  if (valid) {
    if (seg->v1->x == seg->v2->x) {
      // vertical
      if (seg->v1->y == seg->v2->y) {
        valid = false;
      } else {
        seg->dir = TVec(0.0f, (seg->v1->y < seg->v2->y ? 1.0f : -1.0f), 0.0f);
      }
    } else if (seg->v1->y == seg->v2->y) {
      // horizontal
      seg->dir = TVec((seg->v1->x < seg->v2->x ? 1.0f : -1.0f), 0.0f, 0.0f);
    } else {
      seg->dir = ((*seg->v2)-(*seg->v1)).normalised2D();
    }
    if (!seg->dir.isValid() || seg->dir.isZero2D()) valid = false;
  }
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

    seg->dir = TVec(1.0f, 0.0f, 0.0f); // arbitrary
    seg->flags |= SF_ZEROLEN;
    if (!isFiniteF(seg->offset)) seg->offset = 0.0f;
    seg->length = 0.0001f;
    // setup fake seg's plane params
    seg->normal = TVec(1.0f, 0.0f, 0.0f);
    seg->dist = 0.0f;
    seg->dir = TVec(1.0f, 0.0f, 0.0f); // arbitrary
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
//  will not modify `offset`, `drawsegs`, etc.
//
//==========================================================================
bool VLevel::ClipPObjSegToSub (const subsector_t *sub, seg_t *seg) noexcept {
  if (!sub || !seg) return false;
  if (sub->numlines < 3) return false; // oops, cannot clip

  TVec *v1 = seg->v1;
  TVec *v2 = seg->v2;

  if (v1->x == v2->x && v1->y == v2->y) return false; // too short, ignore it

  float lensq = seg->length*seg->length;
  bool updateLen = false;

  seg_t *curseg = &Segs[sub->firstline];
  for (int f = sub->numlines; f--; ++curseg) {
    // determine sides for each point
    const float dot1 = curseg->PointDistance(*v1);
    const float dot2 = curseg->PointDistance(*v2);
    // if both dots are outside, the whole seg is outside of subsector
    if (dot1 <= 0.0f && dot2 <= 0.0f) return false;
    // if both dots are inside (or coplanar), ignore this plane
    if (dot1 >= 0.0f && dot2 >= 0.0f) continue;
    // need to be split
    if (dot1 < 0.0f) {
      vassert(dot2 >= 0.0f);
      // move x
           if (curseg->normal.x == +1.0f) v1->x = curseg->dist;
      else if (curseg->normal.x == -1.0f) v1->x = -curseg->dist;
      else v1->x += dot1/(dot1-dot2);
      // move y
           if (curseg->normal.y == +1.0f) v1->y = curseg->dist;
      else if (curseg->normal.y == -1.0f) v1->y = -curseg->dist;
      else v1->y += dot1/(dot1-dot2);
    } else if (dot2 < 0.0f) {
      vassert(dot1 >= 0.0f);
      // move x
           if (curseg->normal.x == +1.0f) v2->x = curseg->dist;
      else if (curseg->normal.x == -1.0f) v2->x = -curseg->dist;
      else v2->x += dot2/(dot2-dot1);
      // move y
           if (curseg->normal.y == +1.0f) v2->y = curseg->dist;
      else if (curseg->normal.y == -1.0f) v2->y = -curseg->dist;
      else v2->y += dot2/(dot2-dot1);
    } else {
      Sys_Error("ClipPObjSegToSub: oops!");
    }
    const float dx = v2->x-v1->x;
    const float dy = v2->y-v1->y;
    lensq = dx*dx+dy*dy;
    if (lensq <= 0.001f*0.001f) return false; // too short
    updateLen = true;
  }

  if (updateLen) seg->length = sqrtf(lensq);
  return true;
}
