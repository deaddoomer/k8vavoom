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
//**  Copyright (C) 2018-2023 Ketmar Dark
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
#include "r_tex.h"


//==========================================================================
//
//  VWarpTexture::VWarpTexture
//
//==========================================================================
VWarpTexture::VWarpTexture (VTexture *ASrcTex, float aspeed)
  : VTexture()
  , SrcTex(ASrcTex)
  , GenTime(0)
  , Speed(aspeed)
  , WarpXScale(1.0f)
  , WarpYScale(1.0f)
  , XSin1(nullptr)
  , XSin2(nullptr)
  , YSin1(nullptr)
  , YSin2(nullptr)
{
  Width = SrcTex->GetWidth();
  Height = SrcTex->GetHeight();
  SOffset = SrcTex->SOffset;
  TOffset = SrcTex->TOffset;
  SScale = SrcTex->SScale;
  TScale = SrcTex->TScale;
  WarpType = 1;
  if (Speed < 1) Speed = 1; else if (Speed > 16) Speed = 16;
  mFormat = mOrigFormat = (SrcTex ? SrcTex->Format : TEXFMT_RGBA);
}


//==========================================================================
//
//  VWarpTexture::~VWarpTexture
//
//==========================================================================
VWarpTexture::~VWarpTexture () {
  if (Pixels) {
    delete[] Pixels;
    Pixels = nullptr;
  }
  if (SrcTex) {
    delete SrcTex;
    SrcTex = nullptr;
  }
  if (XSin1) {
    delete[] XSin1;
    XSin1 = nullptr;
  }
  if (XSin2) {
    delete[] XSin2;
    XSin2 = nullptr;
  }
  if (YSin1) {
    delete[] YSin1;
    YSin1 = nullptr;
  }
  if (YSin2) {
    delete[] YSin2;
    YSin2 = nullptr;
  }
}


//==========================================================================
//
//  VWarpTexture::IsDynamicTexture
//
//==========================================================================
bool VWarpTexture::IsDynamicTexture () const noexcept {
  return true;
}


//==========================================================================
//
//  VWarpTexture::SetFrontSkyLayer
//
//==========================================================================
void VWarpTexture::SetFrontSkyLayer () {
  SrcTex->SetFrontSkyLayer();
}


//==========================================================================
//
//  VWarpTexture::CheckModified
//
//  returns 0 if not, positive if only data need to be updated, or
//  negative to recreate texture
//
//==========================================================================
int VWarpTexture::CheckModified () {
  return (GenTime != GTextureManager.Time*Speed);
}


//==========================================================================
//
//  VWarpTexture::GetPixels
//
//==========================================================================
vuint8 *VWarpTexture::GetPixels () {
  if (Pixels && GenTime == GTextureManager.Time*Speed) return Pixels;

  const vuint8 *SrcPixels = SrcTex->GetPixels();
  mFormat = mOrigFormat = SrcTex->Format;
  transFlags = SrcTex->transFlags;

  GenTime = GTextureManager.Time*Speed;
  Pixels8BitValid = false;
  Pixels8BitAValid = false;

  if (!XSin1) {
    XSin1 = new float[Width];
    YSin1 = new float[Height];
  }

  // precalculate sine values
  for (int x = 0; x < Width; ++x) {
    XSin1[x] = msin(GenTime*44+x/WarpXScale*5.625f+95.625f)*8*WarpYScale+8*WarpYScale*Height;
  }
  for (int y = 0; y < Height; ++y) {
    YSin1[y] = msin(GenTime*50+y/WarpYScale*5.625f)*8*WarpXScale+8*WarpXScale*Width;
  }

  if (mFormat == TEXFMT_8 || mFormat == TEXFMT_8Pal) {
    if (!Pixels) Pixels = new vuint8[Width*Height];
    vuint8 *Dst = Pixels;
    for (int y = 0; y < Height; ++y) {
      for (int x = 0; x < Width; ++x) {
        if (!(*Dst++ = SrcPixels[(((int)YSin1[y]+x)%Width)+(((int)XSin1[x]+y)%Height)*Width])) transFlags |= FlagTransparent;
        else transFlags |= FlagHasSolidPixel;
      }
    }
  } else {
    if (!Pixels) Pixels = new vuint8[Width*Height*4];
    vuint32 *Dst = (vuint32 *)Pixels;
    for (int y = 0; y < Height; ++y) {
      for (int x = 0; x < Width; ++x, ++Dst) {
        *Dst = ((vuint32 *)SrcPixels)[(((int)YSin1[y]+x)%Width)+(((int)XSin1[x]+y)%Height)*Width];
        const vuint8 a8 = (((*Dst)>>24)&0xffu);
        if (a8 != 0xffu) transFlags |= (a8 ? FlagTranslucent : FlagTransparent);
        else transFlags |= FlagHasSolidPixel;
      }
    }
  }

  return Pixels;
}


//==========================================================================
//
//  VWarpTexture::GetPalette
//
//==========================================================================
rgba_t *VWarpTexture::GetPalette () {
  return SrcTex->GetPalette();
}


//==========================================================================
//
//  VWarpTexture::GetHighResolutionTexture
//
//==========================================================================
VTexture *VWarpTexture::GetHighResolutionTexture () {
  //if (!r_hirestex) return nullptr;
  // if high resolution texture is already created, then just return it
  if (HiResTexture) return HiResTexture;

  VTexture *SrcTex = VTexture::GetHighResolutionTexture();
  if (!SrcTex) return nullptr;

  VWarpTexture *NewTex;
  if (WarpType == 1) NewTex = new VWarpTexture(SrcTex); else NewTex = new VWarp2Texture(SrcTex);
  NewTex->Name = Name;
  NewTex->Type = Type;
  NewTex->WarpXScale = NewTex->GetWidth()/GetWidth();
  NewTex->WarpYScale = NewTex->GetHeight()/GetHeight();
  HiResTexture = NewTex;
  return HiResTexture;
}


//==========================================================================
//
//  VWarp2Texture::VWarp2Texture
//
//==========================================================================
VWarp2Texture::VWarp2Texture (VTexture *ASrcTex, float aspeed)
  : VWarpTexture(ASrcTex, aspeed)
{
  WarpType = 2;
}


//==========================================================================
//
//  VWarp2Texture::GetPixels
//
//==========================================================================
vuint8 *VWarp2Texture::GetPixels () {
  if (Pixels && GenTime == GTextureManager.Time*Speed) return Pixels;
  Pixels8BitValid = false;
  Pixels8BitAValid = false;

  const vuint8 *SrcPixels = SrcTex->GetPixels();
  mFormat = mOrigFormat = SrcTex->Format;
  transFlags = SrcTex->transFlags;

  GenTime = GTextureManager.Time*Speed;

  if (!XSin1) {
    XSin1 = new float[Height];
    XSin2 = new float[Width];
    YSin1 = new float[Height];
    YSin2 = new float[Width];
  }

  // precalculate sine values
  for (int y = 0; y < Height; ++y) {
    XSin1[y] = msin(y/WarpYScale*5.625f+GenTime*313.895f+39.55f)*2*WarpXScale;
    YSin1[y] = y+(2*Height+msin(y/WarpYScale*5.625f+GenTime*118.337f+30.76f)*2)*WarpYScale;
  }
  for (int x = 0; x < Width; ++x) {
    XSin2[x] = x+(2*Width+msin(x/WarpXScale*11.25f+GenTime*251.116f+13.18f)*2)*WarpXScale;
    YSin2[x] = msin(x/WarpXScale*11.25f+GenTime*251.116f+52.73f)*2*WarpYScale;
  }

  if (mFormat == TEXFMT_8 || mFormat == TEXFMT_8Pal) {
    if (!Pixels) Pixels = new vuint8[Width*Height];
    vuint8 *dest = Pixels;
    for (int y = 0; y < Height; ++y) {
      for (int x = 0; x < Width; ++x) {
        *dest++ = SrcPixels[((int)(XSin1[y]+XSin2[x])%Width)+((int)(YSin1[y]+YSin2[x])%Height)*Width];
      }
    }
  } else {
    if (!Pixels) Pixels = new vuint8[Width*Height*4];
    vuint32 *dest = (vuint32 *)Pixels;
    for (int y = 0; y < Height; ++y) {
      for (int x = 0; x < Width; ++x) {
        int Idx = ((int)(XSin1[y]+XSin2[x])%Width)*4+((int)(YSin1[y]+YSin2[x])%Height)*Width*4;
        *dest++ = *(vuint32 *)(SrcPixels+Idx);
      }
    }
  }

  return Pixels;
}


//==========================================================================
//
//  VWarpTexture::ReleasePixels
//
//==========================================================================
void VWarpTexture::ReleasePixels () {
  if (InReleasingPixels()) return; // already released
  if (PixelsReleased()) return; // safeguard
  VTexture::ReleasePixels();
  ReleasePixelsLock rlock(this);
  if (SrcTex) SrcTex->ReleasePixels();
}
