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
//** texture atlases; used for fonts
//**************************************************************************
#include "../gamedefs.h"
#include "r_tex.h"


//==========================================================================
//
//  VTexAtlas8bit::VTexAtlas8bit
//
//==========================================================================
VTexAtlas8bit::VTexAtlas8bit (VName aName, rgba_t *APalette, int aWidth, int aHeight) noexcept {
  vassert(aWidth > 0 && aHeight > 0);
  BytePixels = (vuint8 *)Z_Calloc(aWidth*aHeight);

  Type = TEXTYPE_FontChar;
  mFormat = mOrigFormat = TEXFMT_8Pal;
  Name = aName;
  Width = aWidth;
  Height = aHeight;
  SOffset = 0;
  TOffset = 0;
  SScale = 1.0f;
  TScale = 1.0f;
  Palette = APalette;

  rects.append(Rect(0, 0, aWidth, aHeight)); // one big rect

  const vuint8 tc = findTransparentColor();
  if (tc != 0) memset(BytePixels, tc, aWidth*aHeight);
}


//==========================================================================
//
//  VTexAtlas8bit::~VTexAtlas8bit
//
//==========================================================================
VTexAtlas8bit::~VTexAtlas8bit () {
  Z_Free(BytePixels);
  BytePixels = nullptr;
}


//==========================================================================
//
//  VTexAtlas8bit::Clone
//
//==========================================================================
VTexAtlas8bit *VTexAtlas8bit::Clone (VName aName, rgba_t *APalette) noexcept {
  vassert(Width > 0 && Height > 0);
  VTexAtlas8bit *res = new VTexAtlas8bit(aName, APalette, Width, Height);
  res->rects = rects;
  memcpy(res->BytePixels, BytePixels, Width*Height);
  return res;
}


//==========================================================================
//
//  VTexAtlas8bit::GetPixels
//
//==========================================================================
vuint8 *VTexAtlas8bit::GetPixels () {
  shadeColor = -1;
  return BytePixels;
}


//==========================================================================
//
//  VTexAtlas8bit::GetPalette
//
//==========================================================================
rgba_t *VTexAtlas8bit::GetPalette () {
  return Palette;
}


//==========================================================================
//
//  VTexAtlas8bit::GetHighResolutionTexture
//
//==========================================================================
VTexture *VTexAtlas8bit::GetHighResolutionTexture () {
  return nullptr;
}


//==========================================================================
//
//  VTexAtlas8bit::findTransparentColor
//
//==========================================================================
vuint8 VTexAtlas8bit::findTransparentColor () const noexcept {
  if (Palette[0].a == 0) return 0;
  vint32 maxdist = MAX_VINT32;
  vuint8 bestc = 0;
  for (unsigned f = 0; f < 256; ++f) {
    if (Palette[f].a == 0) {
      vint32 dist = Palette[f].r*Palette[f].r+Palette[f].g*Palette[f].g+Palette[f].b*Palette[f].b;
      if (!dist) return (vuint8)f;
      if (dist < maxdist) {
        maxdist = dist;
        bestc = (vuint8)f;
      }
    }
  }
  return bestc;
}


//==========================================================================
//
//  VTexAtlas8bit::findBestFit
//
//  node id or BadRect
//
//==========================================================================
int VTexAtlas8bit::findBestFit (int w, int h) noexcept {
  vassert(w > 0 && h > 0);
  int fitW = BadRect, fitH = BadRect, biggest = BadRect;
  for (int idx = 0; idx < rects.length(); ++idx) {
    const Rect &r = rects[idx];
    if (r.w < w || r.h < h) continue; // absolutely can't fit
    if (r.w == w && r.h == h) return idx; // perfect fit
    if (r.w == w) {
      // width fit
      if (fitW == BadRect || rects[fitW].h < r.h) fitW = idx;
    } else if (r.h == h) {
      // height fit
      if (fitH == BadRect || rects[fitH].w < r.w) fitH = idx;
    } else {
      // get biggest rect
      if (biggest == BadRect || rects[biggest].area() > r.area()) biggest = idx;
    }
  }
  // both?
  if (fitW != BadRect && fitH != BadRect) return (rects[fitW].area() > rects[fitH].area() ? fitW : fitH);
  if (fitW != BadRect) return fitW;
  if (fitH != BadRect) return fitH;
  return biggest;
}


//==========================================================================
//
//  allocSubImage
//
//  returns invalid rect if there's no more room (or invalid dimensions)
//
//==========================================================================
VTexAtlas8bit::Rect VTexAtlas8bit::allocSubImage (const int iwdt, const int ihgt) noexcept {
  if (iwdt < 1 || ihgt < 1) return Rect();
  int ri = findBestFit(iwdt, ihgt);
  if (ri == BadRect) return Rect();
  Rect rc = rects[ri];
  Rect res = Rect(rc.x, rc.y, iwdt, ihgt);
  // split this rect
  if (rc.w == res.w && rc.h == res.h) {
    // best fit, simply remove this rect
    rects.removeAt(ri);
  } else {
    if (rc.w == res.w) {
      // split vertically
      rc.y += res.h;
      rc.h -= res.h;
    } else if (rc.h == res.h) {
      // split horizontally
      rc.x += res.w;
      rc.w -= res.w;
    } else {
      Rect nr = rc;
      // split in both directions (by longer edge)
      if (rc.w-res.w > rc.h-res.h) {
        // cut the right part
        nr.x += res.w;
        nr.w -= res.w;
        // cut the bottom part
        rc.y += res.h;
        rc.h -= res.h;
        rc.w = res.w;
      } else {
        // cut the bottom part
        nr.y += res.h;
        nr.h -= res.h;
        // cut the right part
        rc.x += res.w;
        rc.w -= res.w;
        rc.h = res.h;
      }
      rects.append(nr);
    }
    rects[ri] = rc;
  }
  return res;
}


//==========================================================================
//
//  VTexAtlas8bit::setPixel
//
//==========================================================================
void VTexAtlas8bit::setPixel (const Rect &rect, const int x, const int y, const vuint8 clr) noexcept {
  if (!rect.isValid()) return;
  if (x < 0 || y < 0 || x >= rect.w || y >= rect.h) return;
  BytePixels[(rect.y+y)*Width+(rect.x+x)] = clr;
}
