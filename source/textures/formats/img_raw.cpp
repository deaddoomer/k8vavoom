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
#include "../../gamedefs.h"
#include "../r_tex.h"


//==========================================================================
//
//  VRawPicTexture::Create
//
//==========================================================================
VTexture *VRawPicTexture::Create (VStream &Strm, int LumpNum) {
  if (Strm.TotalSize() != 64000) return nullptr; // wrong size

  // do an extra check to see if it's a valid patch
  vint16 Width;
  vint16 Height;
  vint16 SOffset;
  vint16 TOffset;

  Strm.Seek(0);
  Strm << Width << Height << SOffset << TOffset;
  if (Width > 0 && Height > 0 && Width <= 2048 && Height < 510) {
    // dimensions seem to be valid; check column directory to see if it's valid
    // we expect at least one column to start exactly right after the directory
    bool GapAtStart = true;
    bool IsValid = true;
    vint32 *Offsets = new vint32[Width];
    for (int i = 0; i < Width; ++i) {
      Strm << Offsets[i];
      if (Offsets[i] == 8+Width*4) {
        GapAtStart = false;
      } else if (Offsets[i] < 8+Width*4 || Offsets[i] >= Strm.TotalSize()) {
        IsValid = false;
        break;
      }
    }
    if (IsValid && !GapAtStart) {
      // offsets seem to be valid
      delete[] Offsets;
      return nullptr;
    }
    delete[] Offsets;
  }

  return new VRawPicTexture(LumpNum, -1);
}


//==========================================================================
//
//  VRawPicTexture::VRawPicTexture
//
//==========================================================================
VRawPicTexture::VRawPicTexture (int ALumpNum, int APalLumpNum)
  : VTexture()
  , PalLumpNum(APalLumpNum)
  , Palette(nullptr)
{
  SourceLump = ALumpNum;
  Type = TEXTYPE_Pic;
  Name = W_LumpName(SourceLump);
  Width = 320;
  Height = 200;
  mFormat = mOrigFormat = (PalLumpNum >= 0 ? TEXFMT_8Pal : TEXFMT_8);
}


//==========================================================================
//
//  VRawPicTexture::~VRawPicTexture
//
//==========================================================================
VRawPicTexture::~VRawPicTexture () {
  if (Pixels) {
    delete[] Pixels;
    Pixels = nullptr;
  }
  if (Palette) {
    delete[] Palette;
    Palette = nullptr;
  }
}


//==========================================================================
//
//  VRawPicTexture::GetPixels
//
//==========================================================================
vuint8 *VRawPicTexture::GetPixels () {
  // if already got pixels, then just return them
  if (Pixels) return Pixels;
  transFlags = TransValueSolid; // anyway

  Pixels = new vuint8[64000];

  // set up palette
  int black;
  if (PalLumpNum < 0) {
    black = r_black_color;
  } else {
    // load palette and remap color 0
    Palette = new rgba_t[256];
    VCheckedStream PStrm(PalLumpNum);
    for (unsigned i = 0; i < 256; ++i) {
      PStrm << Palette[i].r << Palette[i].g << Palette[i].b;
    }
    black = R_ProcessPalette(Palette, true/*forceOpacity*/);
  }

  // read data
  VCheckedStream Strm(SourceLump);
  vuint8 *dst = Pixels;
  for (int i = 0; i < 64000; ++i, ++dst) {
    Strm << *dst;
    if (!*dst) *dst = black;
  }

  ConvertPixelsToShaded();
  PrepareBrightmap();
  return Pixels;
}


//==========================================================================
//
//  VRawPicTexture::GetPalette
//
//==========================================================================
rgba_t *VRawPicTexture::GetPalette () {
  return (Palette ? Palette : r_palette);
}


//==========================================================================
//
//  VRawPicTexture::ReleasePixels
//
//==========================================================================
void VRawPicTexture::ReleasePixels () {
  VTexture::ReleasePixels();
  if (Palette) { delete[] Palette; Palette = nullptr; }
}
