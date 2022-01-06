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

  float x = in.x, y = in.y;
  float bestdist = INFINITY;
  float bestx = x, besty = y;
  line_t *bestline = nullptr;

  line_t *line = sec->lines[0];
  for (int f = sec->linecount; f--; ++line) {
    const TVec *v1 = line->v1;
    const TVec *v2 = line->v2;
    float a = v2->x-v1->x;
    float b = v2->y-v1->y;
    const float den = a*a+b*b;

    float ix, iy;
    if (fabsf(den) <= 0.01f) {
      // this line is actually a point!
      ix = v1->x;
      iy = v1->y;
    } else {
      const float num = (x-v1->x)*a+(y-v1->y)*b;
      const float u = num/den;
      if (u <= 0.0f) {
        ix = v1->x;
        iy = v1->y;
      } else if (u >= 1.0f) {
        ix = v2->x;
        iy = v2->y;
      } else {
        ix = v1->x+u*a;
        iy = v1->y+u*b;
      }
    }
    a = ix-x;
    b = iy-y;
    const float dist = a*a+b*b;
    if (dist < bestdist) {
      bestdist = dist;
      bestx = ix;
      besty = iy;
      bestline = line;
    }
  }

  if (resline) *resline = bestline;
  return TVec(bestx, besty, in.z);
}


//============================================================================
//
//  VLevel::GetMidTexturePosition
//
//  returns top and bottom of the current line's mid texture
//  returns `false` if there is no midtex (or it is empty)
//
//============================================================================
bool VLevel::GetMidTexturePosition (const line_t *linedef, int sideno, float *ptexbot, float *ptextop) {
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
  if (!bsec) bsec = fsec; // to avoid more checks down the lane

  const float texh = MTex->GetScaledHeightF()/sidedef->Mid.ScaleY;
  // just in case
  if (texh < 1.0f) {
    if (ptextop) *ptextop = 0.0f;
    if (ptexbot) *ptexbot = 0.0f;
    return false;
  }

  const float tz0 = max2(fsec->floor.TexZ, bsec->floor.TexZ);
  const float tz1 = min2(fsec->ceiling.TexZ, bsec->ceiling.TexZ);

  // wrapped?
  if ((linedef->flags&ML_WRAP_MIDTEX)|(sidedef->Flags&SDF_WRAPMIDTEX)) {
    // wrapped texture takes the whole sector free space
    if (ptexbot) *ptexbot = tz0;
    if (ptextop) *ptextop = tz1;
    return true;
  }

  float zOrg; // texture bottom
  if (linedef->flags&ML_DONTPEGBOTTOM) {
    // bottom of texture at bottom
    zOrg = tz0;
  } else {
    // top of texture at top
    zOrg = tz1-texh;
  }

  // fix origin with vertical offset
  if (sidedef->Mid.RowOffset != 0.0f) {
    zOrg += sidedef->Mid.RowOffset/(MTex->TextureTScale()/MTex->TextureOffsetTScale()*sidedef->Mid.ScaleY);
  }

  if (ptexbot) *ptexbot = max2(tz0, zOrg);
  if (ptextop) *ptextop = min2(tz1, zOrg+texh);

  /*
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



//**************************************************************************
//
//  VavoomC API
//
//**************************************************************************

// native final bool GetMidTexturePosition (const line_t *line, int sideno, out float ptexbot, out float ptextop);
IMPLEMENT_FUNCTION(VLevel, GetMidTexturePosition) {
  line_t *ld;
  int sideno;
  float *ptextop;
  float *ptexbot;
  vobjGetParamSelf(ld, sideno, ptexbot, ptextop);
  RET_BOOL(Self->GetMidTexturePosition(ld, sideno, ptexbot, ptextop));
}
