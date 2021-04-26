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


//============================================================================
//
//  VLevel::GetMidTexturePosition
//
//  returns top and bottom of the current line's mid texture
//  should be used ONLY for 3dmidtex non-wrapping lines
//  returns `false` if there is no midtex (or it is empty)
//
//============================================================================
bool VLevel::GetMidTexturePosition (const line_t *linedef, int sideno, float *ptextop, float *ptexbot) {
  if (sideno < 0 || sideno > 1 || !linedef || linedef->sidenum[0] == -1 || linedef->sidenum[1] == -1) {
    if (ptextop) *ptextop = 0.0f;
    if (ptexbot) *ptexbot = 0.0f;
    return false;
  }

  const side_t *sidedef = &Sides[linedef->sidenum[sideno]];
  if (!sidedef || GTextureManager.IsEmptyTexture(sidedef->MidTexture)) {
    if (ptextop) *ptextop = 0.0f;
    if (ptexbot) *ptexbot = 0.0f;
    return false;
  }

  VTexture *MTex = GTextureManager(sidedef->MidTexture);
  // just in case
  if (!MTex || MTex->Type == TEXTYPE_Null) {
    if (ptextop) *ptextop = 0.0f;
    if (ptexbot) *ptexbot = 0.0f;
    return false;
  }

  const sector_t *fsec = (sideno ? linedef->backsector : linedef->frontsector);
  // just in case
  if (!fsec) {
    if (ptextop) *ptextop = 0.0f;
    if (ptexbot) *ptexbot = 0.0f;
    return false;
  }

  const sector_t *bsec = (sideno ? linedef->frontsector : linedef->backsector);

  const float scale = sidedef->Mid.ScaleY;
  const float texh = MTex->GetScaledHeight()/(scale > 0.0f ? scale : 1.0f);
  // just in case
  if (texh < 1.0f) {
    if (ptextop) *ptextop = 0.0f;
    if (ptexbot) *ptexbot = 0.0f;
    return false;
  }

  float zOrg; // texture bottom
  if (linedef->flags&ML_DONTPEGBOTTOM) {
    // bottom of texture at bottom
    zOrg = (bsec ? max2(fsec->floor.TexZ, bsec->floor.TexZ) : fsec->floor.TexZ);
  } else {
    // top of texture at top
    zOrg = (bsec ? min2(fsec->ceiling.TexZ, bsec->ceiling.TexZ) : fsec->ceiling.TexZ)-texh;
  }

  zOrg *= MTex->TScale;
  zOrg += sidedef->Mid.RowOffset*MTex->TextureOffsetTScale()/sidedef->Mid.ScaleX;

  if (ptexbot) *ptexbot = zOrg;
  if (ptextop) *ptextop = zOrg+texh;

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



//**************************************************************************
//
//  VavoomC API
//
//**************************************************************************

// native final bool GetMidTexturePosition (const line_t *line, int sideno, out float ptextop, out float ptexbot);
IMPLEMENT_FUNCTION(VLevel, GetMidTexturePosition) {
  line_t *ld;
  int sideno;
  float *ptextop;
  float *ptexbot;
  vobjGetParamSelf(ld, sideno, ptextop, ptexbot);
  RET_BOOL(Self->GetMidTexturePosition(ld, sideno, ptextop, ptexbot));
}
