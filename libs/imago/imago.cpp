//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**  Copyright (C) 2018-2023 Ketmar Dark
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; version 3
//**  of the License ONLY.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************
#include "imago.h"


// ////////////////////////////////////////////////////////////////////////// //
extern VImage *imagoLoadPNG (VStream *strm);
extern VImage *imagoLoadTGA (VStream *strm);
extern VImage *imagoLoadPCX (VStream *strm);
extern VImage *imagoLoadSTB (VStream *strm);


// ////////////////////////////////////////////////////////////////////////// //
struct ImagoLoader {
  VImageLoaderFn ldr;
  VStr ext; // without dot
  VStr desc;
  int prio;
  ImagoLoader *next;
};

static ImagoLoader *loaders = nullptr;


//==========================================================================
//
//  ImagoRegisterLoader
//
//  `ext` may, or may not include dot
//
//==========================================================================
void ImagoRegisterLoader (const char *fmtext, const char *fmtdesc, VImageLoaderFn ldr, int prio) noexcept {
  VStr ext, desc;

  if (!ldr) return;

  if (fmtext && fmtext[0]) {
    if (fmtext[0] == '.' && fmtext[1] == 0) {
    } else {
      if (fmtext[0] == '.') ext = VStr(fmtext+1); else ext = VStr(fmtext);
    }
  }

  desc = VStr(fmtdesc);
  ImagoLoader *prev = nullptr, *cur = loaders;
  while (cur && prio <= cur->prio) {
    prev = cur;
    cur = cur->next;
  }

  auto it = new ImagoLoader;
  it->ldr = ldr;
  it->ext = ext;
  it->desc = desc;
  it->prio = prio;
  it->next = cur;

  if (prev) prev->next = it; else loaders = it;
}


//==========================================================================
//
//  VImage::VImage
//
//==========================================================================
VImage::VImage (ImageType atype, int awidth, int aheight) noexcept {
  if (awidth < 1 || aheight < 1) awidth = aheight = 0;

  mFormat = atype;
  mWidth = awidth;
  mHeight = aheight;
  mName = VStr();

  if (awidth && aheight) {
    mPixels = new vuint8[awidth*aheight*(atype == IT_RGBA ? 4 : 1)];
  } else {
    mPixels = nullptr;
  }
  memset((void *)mPalette, 0, sizeof(mPalette));
  mPalUsed = 0;
}


//==========================================================================
//
//  VImage::~VImage
//
//==========================================================================
VImage::~VImage () noexcept {
  delete mPixels;
  mPixels = nullptr;
}


//==========================================================================
//
//  VImage::appendColor
//
//==========================================================================
vuint8 VImage::appendColor (const VImage::RGBA &col) noexcept {
  int bestn = 0, bestdist = 0x7fffffff;
  for (int f = 0; f < mPalUsed; ++f) {
    if (mPalette[f] == col) return (vuint8)f;
    int idst = mPalette[f].distSq(col);
    if (idst < bestdist) { bestn = f; bestdist = idst; }
  }
  if (mPalUsed < 256) {
    mPalette[mPalUsed++] = col;
    return (vuint8)(mPalUsed-1);
  }
  return (vuint8)bestn;
}


//==========================================================================
//
//  VImage::getPixel
//
//==========================================================================
VImage::RGBA VImage::getPixel (const int x, const int y) const noexcept {
  if (x < 0 || y < 0 || x >= mWidth || y >= mHeight || !mPixels) return VImage::RGBA(0, 0, 0, 0);

  const vuint8 *data = mPixels;

  int pitch = 0;
  switch (mFormat) {
    case IT_Pal: pitch = 1; break;
    case IT_RGBA: pitch = 4; break;
    default: return VImage::RGBA(0, 0, 0, 0);
  }

  data += y*(mWidth*pitch)+x*pitch;
  switch (mFormat) {
    case IT_Pal: return mPalette[*data];
    case IT_RGBA: return *((const VImage::RGBA *)data);
  }

  return VImage::RGBA(0, 0, 0, 0);
}


//==========================================================================
//
//  VImage::setPixel
//
//==========================================================================
void VImage::setPixel (const int x, const int y, const RGBA &col) noexcept {
  if (x < 0 || y < 0 || x >= mWidth || y >= mHeight || !mPixels) return;

  vuint8 *data = mPixels;

  int pitch = 0;
  switch (mFormat) {
    case IT_Pal: pitch = 1; break;
    case IT_RGBA: pitch = 4; break;
    default: return;
  }

  data += y*(mWidth*pitch)+x*pitch;
  switch (mFormat) {
    case IT_Pal: *data = appendColor(col); break;
    case IT_RGBA: *(VImage::RGBA *)data = col; break;
  }
}


//==========================================================================
//
//  VImage::setBytePixel
//
//  has any sense only for paletted images
//
//==========================================================================
void VImage::setBytePixel (const int x, const int y, vuint8 b) noexcept {
  if (x < 0 || y < 0 || x >= mWidth || y >= mHeight || !mPixels || mFormat != IT_Pal) return;

  vuint8 *data = mPixels+y*mWidth+x;
  *data = b;
}


//==========================================================================
//
//  VImage::checkerFill
//
//==========================================================================
void VImage::checkerFill () noexcept {
  if (!mPixels || width < 1 || height < 1) return;
  for (int y = 0; y < mHeight; ++y) {
    for (int x = 0; x < mWidth; ++x) {
      RGBA col = (((x/8)^(y/8))&1 ? RGBA(255, 255, 255, 255) : RGBA(0, 255, 255, 255));
      setPixel(x, y, col);
    }
  }
}


//==========================================================================
//
//  VImage::setPalette
//
//==========================================================================
void VImage::setPalette (const RGBA *pal, int colnum) noexcept {
  memset((void *)mPalette, 0, sizeof(mPalette));
  if (pal) {
    if (colnum > 256) colnum = 256;
    if (colnum > 0 && pal) memcpy((void *)mPalette, pal, sizeof(RGBA)*colnum);
    mPalUsed = colnum;
  } else {
    mPalUsed = 0;
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// this one came from GZDoom
//
//==========================================================================
//#define CHKPIX(ofs) (l1[(ofs)*4+MSB] == 255 ? ((((vuint32 *)l1)[0] = ((vuint32*)l1)[ofs]&SOME_MASK), trans = true ) : false)
#define CHKPIX(ofs) (l1[ofs].isOpaque() ? (l1[0] = RGBA(l1[ofs].r, l1[ofs].g, l1[ofs].b, 0), trans = true) : false)
#define CHKPIXV(ofs) ((void)CHKPIX(ofs))

//==========================================================================
//
//  VImage::smoothEdges
//
//==========================================================================
void VImage::smoothEdges () noexcept {
  if (mWidth <= 1 || mHeight <= 1 || mFormat != IT_RGBA) return;

  RGBA *l1 = (VImage::RGBA *)mPixels;
  // if I set this to false here the code won't detect textures that only contain transparent pixels
  bool trans = l1->isTransparent();

  if (l1->isTransparent() && !CHKPIX(1)) {
    CHKPIX(mWidth);
  }
  ++l1;

  for (int x = 1; x < mWidth-1; ++x, ++l1) {
    if (l1->isTransparent() && !CHKPIX(-1) && !CHKPIX(1)) {
      CHKPIXV(mWidth);
    }
  }
  if (l1->isTransparent() && !CHKPIX(-1)) {
    CHKPIXV(mWidth);
  }
  ++l1;

  for (int y = 1; y < mHeight-1; ++y) {
    if (l1->isTransparent() && !CHKPIX(-mWidth) && !CHKPIX(1)) {
      CHKPIXV(mWidth);
    }
    ++l1;

    for (int x = 1; x < mWidth-1; ++x, ++l1) {
      if (l1->isTransparent() && !CHKPIX(-mWidth) && !CHKPIX(-1) && !CHKPIX(1) && !CHKPIX(-mWidth-1) && !CHKPIX(-mWidth+1) && !CHKPIX(mWidth-1) && !CHKPIX(mWidth+1)) {
        CHKPIXV(mWidth);
      }
    }
    if (l1->isTransparent() && !CHKPIX(-mWidth) && !CHKPIX(-1)) {
      CHKPIXV(mWidth);
    }
    ++l1;
  }

  if (l1->isTransparent() && !CHKPIX(-mWidth)) {
    CHKPIXV(1);
  }
  ++l1;

  for (int x = 1; x < mWidth-1; ++x, ++l1) {
    if (l1->isTransparent() && !CHKPIX(-mWidth) && !CHKPIX(-1)) {
      CHKPIXV(1);
    }
  }

  if (l1->isTransparent() && !CHKPIX(-mWidth)) {
    CHKPIXV(-1);
  }
}


//==========================================================================
//
//  VImage::loadFrom
//
//  load image from stream; return `nullptr` on error
//
//==========================================================================
VImage *VImage::loadFrom (VStream *strm, VStr name) {
  if (!strm) return nullptr;
  for (ImagoLoader *it = loaders; it; it = it->next) {
    strm->Seek(0);
    //fprintf(stderr, ":: trying <%s>\n", *it->ext);
    VImage *res = it->ldr(strm);
    if (res) {
      res->setName(name);
      res->setType(it->ext);
      return res;
    }
  }
  return nullptr;
}


//==========================================================================
//
//  VImage::saveAsPNG
//
//==========================================================================
bool VImage::saveAsPNG (VStream *strm) {
  if (!strm) return false;
  TArray<vuint8> tmprgb;
  const vuint8 *picdata;
  if (isTrueColor) {
    picdata = mPixels;
  } else {
    tmprgb.setLength(width*height*4);
    vuint8 *dest = (vuint8 *)tmprgb.ptr();
    picdata = (const vuint8 *)dest;
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < height; ++x) {
        auto clr = getPixel(x, y);
        *dest++ = clr.r;
        *dest++ = clr.g;
        *dest++ = clr.b;
        *dest++ = clr.a;
      }
    }
  }

  ESSType ptp = SS_RGBA;

  PalEntry pal[256];
  memset((void *)pal, 0, sizeof(pal));

  return M_CreatePNG(strm, picdata, pal, ptp, width, height, width*4, 1.0f);
}


// ////////////////////////////////////////////////////////////////////////// //
class VLoaderReg {
public:
  VLoaderReg (int n) {
    ImagoRegisterLoader("png", "Portable Network Graphics", &imagoLoadPNG);
    ImagoRegisterLoader("pcx", "PCX image", &imagoLoadPCX, 500);
    ImagoRegisterLoader("tga", "Targa Image", &imagoLoadTGA, 600);
    ImagoRegisterLoader("stb", "STB_Image image", &imagoLoadSTB, 600);
  }
};


__attribute__((used)) VLoaderReg imago_loader_registrator_variable_(666);
