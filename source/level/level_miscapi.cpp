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
//  P_SectorClosestPoint
//
//  Given a point (x,y), returns the point (ox,oy) on the sector's defining
//  lines that is nearest to (x,y).
//  Ignores `z`.
//
//==========================================================================
TVec P_SectorClosestPoint (const sector_t *sec, const TVec in, line_t **resline) {
  if (!sec || sec->isAnyPObj()) {
    if (resline) *resline = nullptr;
    return in;
  }

  double x = in.x, y = in.y;
  double bestdist = /*HUGE_VAL*/1e200;
  double bestx = x, besty = y;
  line_t *bestline = nullptr;

  line_t *line = sec->lines[0];
  for (int f = sec->linecount; f--; ++line) {
    const TVec *v1 = line->v1;
    const TVec *v2 = line->v2;
    double a = v2->x-v1->x;
    double b = v2->y-v1->y;
    const double den = a*a+b*b;
    double ix, iy, dist;

    if (fabs(den) <= 0.01) {
      // this line is actually a point!
      ix = v1->x;
      iy = v1->y;
    } else {
      double num = (x-v1->x)*a+(y-v1->y)*b;
      double u = num/den;
      if (u <= 0.0) {
        ix = v1->x;
        iy = v1->y;
      } else if (u >= 1.0) {
        ix = v2->x;
        iy = v2->y;
      } else {
        ix = v1->x+u*a;
        iy = v1->y+u*b;
      }
    }
    a = ix-x;
    b = iy-y;
    dist = a*a+b*b;
    if (dist < bestdist) {
      bestdist = dist;
      bestx = ix;
      besty = iy;
      bestline = line;
    }
  }

  if (resline) *resline = bestline;
  return TVec((float)bestx, (float)besty, in.z);
}


//==========================================================================
//
//  P_BoxOnLineSide
//
//  considers the line to be infinite
//  returns side 0 or 1, -1 if box crosses the line
//
//==========================================================================
int P_BoxOnLineSide (const float *tmbox, const line_t *ld) {
  int p1 = 0;
  int p2 = 0;

  switch (ld->slopetype) {
    case ST_HORIZONTAL:
      p1 = (tmbox[BOX2D_TOP] > ld->v1->y);
      p2 = (tmbox[BOX2D_BOTTOM] > ld->v1->y);
      if (ld->dir.x < 0.0f) { p1 ^= 1; p2 ^= 1; }
      break;
    case ST_VERTICAL:
      p1 = (tmbox[BOX2D_RIGHT] < ld->v1->x);
      p2 = (tmbox[BOX2D_LEFT] < ld->v1->x);
      if (ld->dir.y < 0.0f) { p1 ^= 1; p2 ^= 1; }
      break;
    case ST_POSITIVE:
      p1 = ld->PointOnSide(TVec(tmbox[BOX2D_LEFT], tmbox[BOX2D_TOP], 0.0f));
      p2 = ld->PointOnSide(TVec(tmbox[BOX2D_RIGHT], tmbox[BOX2D_BOTTOM], 0.0f));
      break;
    case ST_NEGATIVE:
      p1 = ld->PointOnSide(TVec(tmbox[BOX2D_RIGHT], tmbox[BOX2D_TOP], 0.0f));
      p2 = ld->PointOnSide(TVec(tmbox[BOX2D_LEFT], tmbox[BOX2D_BOTTOM], 0.0f));
      break;
  }

  return (p1 == p2 ? p1 : -1);
}


//============================================================================
//
//  P_GetMidTexturePosition
//
//  retrieves top and bottom of the current line's mid texture
//
//============================================================================
bool P_GetMidTexturePosition (const line_t *linedef, int sideno, float *ptextop, float *ptexbot) {
  if (sideno < 0 || sideno > 1 || !linedef || linedef->sidenum[0] == -1 || linedef->sidenum[1] == -1) {
    if (ptextop) *ptextop = 0;
    if (ptexbot) *ptexbot = 0;
    return false;
  }

  // have to do this, because this function can be called both in server and in client
  VLevel *level = (GClLevel ? GClLevel : GLevel);

  const side_t *sidedef = &level->Sides[linedef->sidenum[sideno]];
  if (sidedef->MidTexture <= 0) {
    if (ptextop) *ptextop = 0;
    if (ptexbot) *ptexbot = 0;
    return false;
  }

  VTexture *MTex = GTextureManager(sidedef->MidTexture);
  if (!MTex) {
    if (ptextop) *ptextop = 0;
    if (ptexbot) *ptexbot = 0;
    return false;
  }
  //FTexture * tex= TexMan(texnum);
  //if (!tex) return false;

  const sector_t *sec = (sideno ? linedef->backsector : linedef->frontsector);

  //FIXME: use sector regions instead?
  //       wtf are sector regions at all?!

  const float mheight = MTex->GetScaledHeight();

  float toffs;
  if (linedef->flags&ML_DONTPEGBOTTOM) {
    // bottom of texture at bottom
    toffs = sec->floor.TexZ+mheight;
  } else if (linedef->flags&ML_DONTPEGTOP) {
    // top of texture at top of top region
    toffs = sec->ceiling.TexZ;
  } else {
    // top of texture at top
    toffs = sec->ceiling.TexZ;
  }
  toffs *= MTex->TScale;
  toffs += sidedef->Mid.RowOffset*(MTex->bWorldPanning ? MTex->TScale : 1.0f);

  if (ptextop) *ptextop = toffs;
  if (ptexbot) *ptexbot = toffs-mheight;

  /*
  float totalscale = fabsf(sidedef->GetTextureYScale(side_t::mid)) * tex->GetScaleY();
  float y_offset = sidedef->GetTextureYOffset(side_t::mid);
  float textureheight = tex->GetHeight() / totalscale;
  if (totalscale != 1. && !tex->bWorldPanning) y_offset /= totalscale;

  if (linedef->flags & ML_DONTPEGBOTTOM) {
    *ptexbot = y_offset+max2(linedef->frontsector->GetPlaneTexZ(sector_t::floor), linedef->backsector->GetPlaneTexZ(sector_t::floor));
    *ptextop = *ptexbot+textureheight;
  } else {
    *ptextop = y_offset+min2(linedef->frontsector->GetPlaneTexZ(sector_t::ceiling), linedef->backsector->GetPlaneTexZ(sector_t::ceiling));
    *ptexbot = *ptextop-textureheight;
  }
  */

  return true;
}


//==========================================================================
//
//  VLevel::IsPointInSubsector2D
//
//==========================================================================
bool VLevel::IsPointInSubsector2D (const subsector_t *sub, TVec in) const noexcept {
  if (!sub || sub->numlines < 3) return false;
  // check bounding box first
  if (in.x < sub->bbox2d[BOX2D_MINX] || in.y < sub->bbox2d[BOX2D_MINY] ||
      in.x > sub->bbox2d[BOX2D_MAXX] || in.y > sub->bbox2d[BOX2D_MAXY])
  {
    return false;
  }
  // the point should not be on a back side of any seg
  const seg_t *seg = &Segs[sub->firstline];
  for (int count = sub->numlines; count--; ++seg) {
    if (seg->PointOnSide2(in) == 1) return false;
  }
  // it is inside
  return true;
}


//==========================================================================
//
//  VLevel::IsPointInSector
//
//==========================================================================
bool VLevel::IsPointInSector2D (const sector_t *sec, TVec in) const noexcept {
  if (!sec) return false;
  for (const subsector_t *sub = sec->subsectors; sub; sub = sub->seclink) {
    if (IsPointInSubsector2D(sub, in)) return true;
  }
  // it is outside
  return false;
}
