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
//**  Copyright (C) 2018-2020 Ketmar Dark
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

//#define VV_VERY_VERBOSE_TEXTURE_LOADER
static VCvarB dbg_verbose_texture_loader("dbg_verbose_texture_loader", false, "WERY verbose texture loader?", CVAR_PreInit);


// ////////////////////////////////////////////////////////////////////////// //
typedef VTexture *(*VTexCreateFunc) (VStream &, int);


//==========================================================================
//
//  VTexture::CreateTexture
//
//==========================================================================
VTexture *VTexture::CreateTexture (int Type, int LumpNum, bool setName) {
  static const struct {
    VTexCreateFunc Create;
    int Type;
    const char *fmtname;
  } TexTable[] = {
    { VImgzTexture::Create,     TEXTYPE_Any, "imgz" },
    { VPngTexture::Create,      TEXTYPE_Any, "png" },
    { VJpegTexture::Create,     TEXTYPE_Any, "jpg" },
    //{ VSTBTexture::Create,      TEXTYPE_Any, "STB" }, // this loads jpegs and bmps
    { VPcxTexture::Create,      TEXTYPE_Any, "pcx" },
    { VTgaTexture::Create,      TEXTYPE_Any, "tga" },
    //{ VFlat64Texture::Create,     TEXTYPE_Flat, "flat" },
    { VRawPicTexture::Create,   TEXTYPE_Pic, "rawpic" },
    { VPatchTexture::Create,    TEXTYPE_Any, "patch" },
    { VFlatTexture::Create,     TEXTYPE_Flat, "flat" },
    { VAutopageTexture::Create, TEXTYPE_Autopage, "autopage" },
  };

  if (LumpNum < 0) return nullptr;
  VStream *lumpstream = W_CreateLumpReaderNum(LumpNum);
  if (lumpstream->TotalSize() < 1) return nullptr; // just in case
  VCheckedStream Strm(lumpstream, true); // load to memory
  bool doSeek = false;

  int ffcount = 0;
  if (dbg_verbose_texture_loader) GLog.Logf(NAME_Debug, "*** TRYING TO LOAD TEXTURE '%s'", *W_FullLumpName(LumpNum));
  for (size_t i = 0; i < ARRAY_COUNT(TexTable); ++i) {
    if (Type == TEXTYPE_Any || TexTable[i].Type == Type || TexTable[i].Type == TEXTYPE_Any) {
      if (doSeek) Strm.Seek(0); else doSeek = true;
      VTexture *Tex = TexTable[i].Create(Strm, LumpNum);
      if (Tex) {
        if (dbg_verbose_texture_loader) GLog.Logf(NAME_Debug, "***    LOADED TEXTURE '%s' (%s)", *W_FullLumpName(LumpNum), TexTable[i].fmtname);
        if (setName) {
          if (Tex->Name == NAME_None) {
            Tex->Name = W_LumpName(LumpNum);
            if (Tex->Name == NAME_None) Tex->Name = VName(*W_RealLumpName(LumpNum));
          }
        }
        Tex->Type = Type;
        return Tex;
      } else {
        ++ffcount;
      }
    }
  }

  if (dbg_verbose_texture_loader) GLog.Logf(NAME_Debug, "*** FAILED TO LOAD TEXTURE '%s' (%d attempts made)", *W_FullLumpName(LumpNum), ffcount);
  return nullptr;
}


//==========================================================================
//
//  VTexture::VTexture
//
//==========================================================================
VTexture::VTexture ()
  : Type(TEXTYPE_Any)
  , mFormat(TEXFMT_8)
  , mOrigFormat(TEXFMT_8)
  , Name(NAME_None)
  , Width(0)
  , Height(0)
  , SOffset(0)
  , TOffset(0)
  , bNoRemap0(false)
  , bWorldPanning(false)
  , bIsCameraTexture(false)
  , bForcedSpriteOffset(false)
  , SOffsetFix(0)
  , TOffsetFix(0)
  , WarpType(0)
  , SScale(1)
  , TScale(1)
  , TextureTranslation(0)
  , HashNext(-1)
  , SourceLump(-1)
  , RealX0(MIN_VINT32)
  , RealY0(MIN_VINT32)
  , RealHeight(MIN_VINT32)
  , RealWidth(MIN_VINT32)
  , Brightmap(nullptr)
  , noDecals(false)
  , staticNoDecals(false)
  , animNoDecals(false)
  , animated(false)
  , needFBO(false)
  , nofullbright(false)
  , glowing(0)
  , noHires(false)
  , transFlags(TransValueUnknown)
  , lastUpdateFrame(0)
  , DriverHandle(0)
  , DriverTranslated()
  , Pixels(nullptr)
  , Pixels8Bit(nullptr)
  , Pixels8BitA(nullptr)
  , HiResTexture(nullptr)
  , Pixels8BitValid(false)
  , Pixels8BitAValid(false)
  , shadeColor(-1)
{
}


//==========================================================================
//
//  VTexture::~VTexture
//
//==========================================================================
VTexture::~VTexture () {
  ReleasePixels();
  // in case it is not from a pak
  if (Pixels) { delete[] Pixels; Pixels = nullptr; }
  if (Pixels8Bit) { delete[] Pixels8Bit; Pixels8Bit = nullptr; }
  if (Pixels8BitA) { delete[] Pixels8BitA; Pixels8BitA = nullptr; }
  {
    ReleasePixelsLock rlock(this);
    if (HiResTexture) HiResTexture->ReleasePixels();
    if (Brightmap) Brightmap->ReleasePixels();
  }
  delete HiResTexture;
  HiResTexture = nullptr;
  delete Brightmap;
  Brightmap = nullptr;
}


//==========================================================================
//
//  VTexture::SetFrontSkyLayer
//
//==========================================================================
void VTexture::SetFrontSkyLayer () {
  bNoRemap0 = true;
}


//==========================================================================
//
//  VTexture::CheckModified
//
//==========================================================================
bool VTexture::CheckModified () {
  return false;
}


//==========================================================================
//
//  VTexture::ReleasePixels
//
//==========================================================================
void VTexture::ReleasePixels () {
  //if (shadeColor != -1) return; // cannot release shaded texture
  if (SourceLump < 0) return; // this texture cannot be reloaded
  //GCon->Logf(NAME_Debug, "VTexture::ReleasePixels: '%s' (%d: %s)", *Name, SourceLump, *W_FullLumpName(SourceLump));
  if (InReleasingPixels()) return; // already released
  ReleasePixelsLock rlock(this);
  if (Pixels) { delete[] Pixels; Pixels = nullptr; }
  if (Pixels8Bit) { delete[] Pixels8Bit; Pixels8Bit = nullptr; }
  if (Pixels8BitA) { delete[] Pixels8BitA; Pixels8BitA = nullptr; }
  Pixels8BitValid = Pixels8BitAValid = false;
  mFormat = mOrigFormat; // undo `ConvertPixelsToRGBA()`
  if (Brightmap) Brightmap->ReleasePixels();
}


//==========================================================================
//
//  VTexture::GetPixels8
//
//==========================================================================
vuint8 *VTexture::GetPixels8 () {
  // if already have converted version, then just return it
  if (Pixels8Bit && Pixels8BitValid) return Pixels8Bit;
  vuint8 *pixdata = GetPixels();
  if (Format == TEXFMT_8Pal) {
    transFlags = TransValueSolid; // for now
    // remap to game palette
    int NumPixels = Width*Height;
    rgba_t *Pal = GetPalette();
    vuint8 Remap[256];
    Remap[0] = 0;
    //for (int i = 1; i < 256; ++i) Remap[i] = r_rgbtable[((Pal[i].r<<7)&0x7c00)+((Pal[i].g<<2)&0x3e0)+((Pal[i].b>>3)&0x1f)];
    for (int i = 1; i < 256; ++i) Remap[i] = R_LookupRGB(Pal[i].r, Pal[i].g, Pal[i].b);
    if (!Pixels8Bit) Pixels8Bit = new vuint8[NumPixels];
    const vuint8 *pSrc = pixdata;
    vuint8 *pDst = Pixels8Bit;
    vuint8 ccollector = 0;
    for (int i = 0; i < NumPixels; ++i, ++pSrc, ++pDst) {
      const vuint8 pv = *pSrc;
      ccollector |= pv;
      *pDst = Remap[pv];
    }
    if (ccollector) transFlags |= FlagTransparent;
    Pixels8BitValid = true;
    return Pixels8Bit;
  } else if (Format == TEXFMT_RGBA) {
    transFlags = TransValueSolid; // for now
    int NumPixels = Width*Height;
    if (!Pixels8Bit) Pixels8Bit = new vuint8[NumPixels];
    const rgba_t *pSrc = (rgba_t *)pixdata;
    vuint8 *pDst = Pixels8Bit;
    for (int i = 0; i < NumPixels; ++i, ++pSrc, ++pDst) {
      if (pSrc->a != 255) {
        *pDst = 0;
        transFlags |= FlagTransparent;
        if (pSrc->a) transFlags |= FlagTranslucent;
      } else {
        *pDst = R_LookupRGB(pSrc->r, pSrc->g, pSrc->b);
      }
    }
    Pixels8BitValid = true;
    return Pixels8Bit;
  }
  vassert(Format == TEXFMT_8);
  return pixdata;
}


//==========================================================================
//
//  VTexture::GetPixels8A
//
//==========================================================================
pala_t *VTexture::GetPixels8A () {
  // if already have converted version, then just return it
  if (Pixels8BitA && Pixels8BitAValid) {
    //GCon->Logf("***** ALREADY 8A: '%s", *Name);
    return Pixels8BitA;
  }

  const vuint8 *pixdata = GetPixels();

  int NumPixels = Width*Height;
  if (!Pixels8BitA) Pixels8BitA = new pala_t[NumPixels];
  pala_t *pDst = Pixels8BitA;

  if (Format == TEXFMT_8Pal || Format == TEXFMT_8) {
    vassert(Format == mFormat);
    transFlags = TransValueSolid; // for now
    // remap to game palette
    //GCon->Logf("*** remapping paletted '%s' to 8A (%dx%d:%d) (%d)", *Name, Width, Height, NumPixels, mFormat);
    vuint8 remap[256];
    if (Format == TEXFMT_8Pal) {
      // own palette, remap
      remap[0] = 0;
      const rgba_t *pal = GetPalette();
      if (pal) {
        //for (int i = 1; i < 256; ++i) remap[i] = r_rgbtable[((pal[i].r<<7)&0x7c00)+((pal[i].g<<2)&0x3e0)+((pal[i].b>>3)&0x1f)];
        for (int i = 1; i < 256; ++i) remap[i] = R_LookupRGB(pal[i].r, pal[i].g, pal[i].b);
      } else {
        for (int i = 0; i < 256; ++i) remap[i] = i;
      }
    } else {
      // game palette, no remap
      for (int i = 0; i < 256; ++i) remap[i] = i;
    }
    vuint8 ccollector = 0;
    const vuint8 *pSrc = (const vuint8 *)pixdata;
    for (int i = 0; i < NumPixels; ++i, ++pSrc, ++pDst) {
      const vuint8 pv = *pSrc;
      ccollector |= pv;
      pDst->idx = remap[pv];
      pDst->a = (pv ? 255 : 0);
    }
    if (ccollector) transFlags |= FlagTransparent;
  } else if (Format == TEXFMT_RGBA) {
    transFlags = TransValueSolid; // for now
    //GCon->Logf("*** remapping 32-bit '%s' to 8A (%dx%d)", *Name, Width, Height);
    const rgba_t *pSrc = (const rgba_t *)pixdata;
    for (int i = 0; i < NumPixels; ++i, ++pSrc, ++pDst) {
      pDst->idx = R_LookupRGB(pSrc->r, pSrc->g, pSrc->b);
      pDst->a = pSrc->a;
      if (pSrc->a != 255) {
        transFlags |= FlagTransparent;
        if (pSrc->a) transFlags |= FlagTranslucent;
      }
    }
  } else {
    Sys_Error("invalid texture format in `VTexture::GetPixels8A()`");
  }

  Pixels8BitAValid = true;
  return Pixels8BitA;
}


//==========================================================================
//
//  VTexture::GetPalette
//
//==========================================================================
rgba_t *VTexture::GetPalette () {
  return r_palette;
}


//==========================================================================
//
//  VTexture::GetHighResolutionTexture
//
//  Return high-resolution version of this texture, or self if it doesn't
//  exist.
//
//==========================================================================
VTexture *VTexture::GetHighResolutionTexture () {
#ifdef CLIENT
  // if high resolution texture is already created, then just return it
  if (HiResTexture) return HiResTexture;

  if (noHires) return nullptr;
  noHires = true;

  if (!r_hirestex) return nullptr;

  // determine directory name depending on type
  const char *DirName;
  switch (Type) {
    case TEXTYPE_Wall: DirName = "walls"; break;
    case TEXTYPE_Flat: DirName = "flats"; break;
    case TEXTYPE_Overload: DirName = "textures"; break;
    case TEXTYPE_Sprite: DirName = "sprites"; break;
    case TEXTYPE_Pic: case TEXTYPE_Autopage: DirName = "graphics"; break;
    default: return nullptr;
  }

  // try to find it
  /*static*/ const char *Exts[] = { "png", "jpg", "tga", nullptr };
  int LumpNum = W_FindLumpByFileNameWithExts(VStr("hirestex/")+DirName+"/"+*Name, Exts);
  if (LumpNum >= 0) {
    // create new high-resolution texture
    HiResTexture = CreateTexture(Type, LumpNum);
    if (HiResTexture) {
      HiResTexture->Name = Name;
      return HiResTexture;
    }
  }
#endif
  // no hi-res texture found
  return nullptr;
}


//==========================================================================
//
//  VTexture::FixupPalette
//
//==========================================================================
void VTexture::FixupPalette (rgba_t *Palette) {
  if (Width < 1 || Height < 1) return;
  vassert(Pixels);
  // find black color for remaping
  int black = 0;
  int best_dist = 0x10000;
  for (int i = 1; i < 256; ++i) {
    int dist = Palette[i].r*Palette[i].r+Palette[i].g*Palette[i].g+Palette[i].b*Palette[i].b;
    if (dist < best_dist && Palette[i].a == 255) {
      black = i;
      best_dist = dist;
    }
  }
  for (int i = 0; i < Width*Height; ++i) {
    if (Palette[Pixels[i]].a == 0) {
      Pixels[i] = 0;
    } else if (!Pixels[i]) {
      Pixels[i] = black;
    }
  }
  Palette[0].r = 0;
  Palette[0].g = 0;
  Palette[0].b = 0;
  Palette[0].a = 0;
}


//==========================================================================
//
//  VTexture::ResetTranslations
//
//==========================================================================
void VTexture::ResetTranslations () {
  DriverTranslated.resetNoDtor();
}


//==========================================================================
//
//  VTexture::ClearTranslations
//
//==========================================================================
void VTexture::ClearTranslations () {
  DriverTranslated.resetNoDtor();
  DriverTranslated.clear();
}


//==========================================================================
//
//  VTexture::ClearTranslationAt
//
//==========================================================================
void VTexture::ClearTranslationAt (int idx) {
  // currently, there's nothing to do
}


//==========================================================================
//
//  VTexture::MoveTranslationToTop
//
//  returns new index; also, marks it as "recently used"
//
//==========================================================================
int VTexture::MoveTranslationToTop (int idx) {
  if (idx < 0 || idx >= DriverTranslated.length()) return idx;
  vint32 ctime = GetTranslationCTime();
  if (ctime < 0) return idx; // nothing to do here
  VTransData *td = DriverTranslated.ptr()+idx; // sorry
  // don't bother swapping translations, it doesn't really matter
  td->lastUseTime = ctime;
  return idx;
}


//==========================================================================
//
//  VTexture::FindDriverTrans
//
//==========================================================================
VTexture::VTransData *VTexture::FindDriverTrans (VTextureTranslation *TransTab, int CMap, bool markUsed) {
  for (auto &&it : DriverTranslated) {
    if (it.Trans == TransTab && it.ColorMap == CMap) {
      if (markUsed) {
        const vint32 ctime = GetTranslationCTime();
        if (ctime >= 0) it.lastUseTime = ctime;
      }
      return &it;
    }
  }
  return nullptr;
}


//==========================================================================
//
//  VTexture::FindDriverShaded
//
//==========================================================================
VTexture::VTransData *VTexture::FindDriverShaded (vuint32 ShadeColor, int CMap, bool markUsed) {
  ShadeColor |= 0xff000000u;
  for (auto &&it : DriverTranslated) {
    if (it.ShadeColor == ShadeColor && it.ColorMap == CMap) {
      if (markUsed) {
        const vint32 ctime = GetTranslationCTime();
        if (ctime >= 0) it.lastUseTime = ctime;
      }
      return &it;
    }
  }
  return nullptr;
}


//==========================================================================
//
//  VTexture::PremultiplyRGBAInPlace
//
//==========================================================================
void VTexture::PremultiplyRGBAInPlace (void *databuff, int w, int h) {
  if (w < 1 || h < 1) return;
  vuint8 *data = (vuint8 *)databuff;
  // premultiply original image
  for (int count = w*h; count > 0; --count, data += 4) {
    int a = data[3];
    if (a == 0) {
      data[0] = data[1] = data[2] = 0;
    } else if (a != 255) {
      data[0] = data[0]*a/255;
      data[1] = data[1]*a/255;
      data[2] = data[2]*a/255;
    }
  }
}


//==========================================================================
//
//  VTexture::PremultiplyRGBA
//
//==========================================================================
void VTexture::PremultiplyRGBA (void *dest, const void *src, int w, int h) {
  if (w < 1 || h < 1) return;
  const vuint8 *s = (const vuint8 *)src;
  vuint8 *d = (vuint8 *)dest;
  // premultiply image
  for (int count = w*h; count > 0; --count, s += 4, d += 4) {
    int a = s[3];
    if (a == 0) {
      *(vuint32 *)d = 0;
    } else if (a == 255) {
      *(vuint32 *)d = *(const vuint32 *)s;
    } else {
      d[0] = s[0]*a/255;
      d[1] = s[1]*a/255;
      d[2] = s[2]*a/255;
      d[3] = a;
    }
  }
}


//==========================================================================
//
//  VTexture::AdjustGamma
//
//  non-premultiplied
//
//==========================================================================
void VTexture::AdjustGamma (rgba_t *data, int size) {
#ifdef CLIENT
  const vuint8 *gt = getGammaTable(usegamma); //gammatable[usegamma];
  for (int i = 0; i < size; ++i) {
    data[i].r = gt[data[i].r];
    data[i].g = gt[data[i].g];
    data[i].b = gt[data[i].b];
  }
#endif
}


//==========================================================================
//
//  VTexture::SmoothEdges
//
//  This one comes directly from GZDoom
//
//  non-premultiplied
//
//==========================================================================
#define CHKPIX(ofs) (l1[(ofs)*4+MSB] == 255 ? (( ((vuint32 *)l1)[0] = ((vuint32 *)l1)[ofs]&SOME_MASK), trans = true ) : false)

void VTexture::SmoothEdges (vuint8 *buffer, int w, int h) {
  int x, y;
  // why the fuck you would use 0 on big endian here?
  int MSB = 3;
  vuint32 SOME_MASK = (GBigEndian ? 0xffffff00 : 0x00ffffff);

  // if I set this to false here the code won't detect textures that only contain transparent pixels
  bool trans = (buffer[MSB] == 0);
  vuint8 *l1;

  if (h <= 1 || w <= 1) return; // makes (a) no sense and (b) doesn't work with this code!
  l1 = buffer;

  if (l1[MSB] == 0 && !CHKPIX(1)) {
    CHKPIX(w);
  }
  l1 += 4;

  for (x = 1; x < w-1; ++x, l1 += 4) {
    if (l1[MSB] == 0 && !CHKPIX(-1) && !CHKPIX(1)) {
      CHKPIX(w);
    }
  }
  if (l1[MSB] == 0 && !CHKPIX(-1)) {
    CHKPIX(w);
  }
  l1 += 4;

  for (y = 1; y < h-1; ++y) {
    if (l1[MSB] == 0 && !CHKPIX(-w) && !CHKPIX(1)) {
      CHKPIX(w);
    }
    l1 += 4;

    for (x = 1; x < w-1; ++x, l1 += 4) {
      if (l1[MSB] == 0 && !CHKPIX(-w) && !CHKPIX(-1) && !CHKPIX(1) && !CHKPIX(-w-1) && !CHKPIX(-w+1) && !CHKPIX(w-1) && !CHKPIX(w+1)) {
        CHKPIX(w);
      }
    }
    if (l1[MSB] == 0 && !CHKPIX(-w) && !CHKPIX(-1)) {
      CHKPIX(w);
    }
    l1 += 4;
  }

  if (l1[MSB] == 0 && !CHKPIX(-w)) {
    CHKPIX(1);
  }
  l1 += 4;
  for (x = 1; x < w-1; ++x, l1 += 4) {
    if (l1[MSB] == 0 && !CHKPIX(-w) && !CHKPIX(-1)) {
      CHKPIX(1);
    }
  }
  if (l1[MSB] == 0 && !CHKPIX(-w)) {
    CHKPIX(-1);
  }
}

#undef CHKPIX


//==========================================================================
//
//  VTexture::ResampleTexture
//
//  Resizes texture.
//  This is a simplified version of gluScaleImage from sources of MESA 3.0
//  non-premultiplied
//
//==========================================================================
void VTexture::ResampleTexture (int widthin, int heightin, const vuint8 *datain, int widthout, int heightout, vuint8 *dataout, int sampling_type) {
  int i, j, k;
  float sx, sy;

  if (widthout > 1) {
    sx = float(widthin-1)/float(widthout-1);
  } else {
    sx = float(widthin-1);
  }
  if (heightout > 1) {
    sy = float(heightin-1)/float(heightout-1);
  } else {
    sy = float(heightin-1);
  }

  if (sampling_type == 1) {
    // use point sample
    for (i = 0; i < heightout; ++i) {
      int ii = int(i*sy);
      for (j = 0; j < widthout; ++j) {
        int jj = int(j*sx);

        const vuint8 *src = datain+(ii*widthin+jj)*4;
        vuint8 *dst = dataout+(i*widthout+j)*4;

        for (k = 0; k < 4; ++k) *dst++ = *src++;
      }
    }
  } else {
    // use weighted sample
    if (sx <= 1.0f && sy <= 1.0f) {
      // magnify both width and height: use weighted sample of 4 pixels
      int i0, i1, j0, j1;
      float alpha, beta;
      const vuint8 *src00, *src01, *src10, *src11;
      float s1, s2;
      vuint8 *dst;

      for (i = 0; i < heightout; ++i) {
        i0 = int(i*sy);
        i1 = i0+1;
        if (i1 >= heightin) i1 = heightin-1;
        alpha = i*sy-i0;
        for (j = 0; j < widthout; ++j) {
          j0 = int(j*sx);
          j1 = j0+1;
          if (j1 >= widthin) j1 = widthin-1;
          beta = j*sx-j0;

          // compute weighted average of pixels in rect (i0,j0)-(i1,j1)
          src00 = datain+(i0*widthin+j0)*4;
          src01 = datain+(i0*widthin+j1)*4;
          src10 = datain+(i1*widthin+j0)*4;
          src11 = datain+(i1*widthin+j1)*4;

          dst = dataout+(i*widthout+j)*4;

          for (k = 0; k < 4; ++k) {
            s1 = *src00++ *(1.0f-beta)+ *src01++ *beta;
            s2 = *src10++ *(1.0f-beta)+ *src11++ *beta;
            *dst++ = vuint8(s1*(1.0f-alpha)+s2*alpha);
          }
        }
      }
    } else {
      // shrink width and/or height: use an unweighted box filter
      int i0, i1;
      int j0, j1;
      int ii, jj;
      int sum;
      vuint8 *dst;

      for (i = 0; i < heightout; ++i) {
        i0 = int(i*sy);
        i1 = i0+1;
        if (i1 >= heightin) i1 = heightin-1;
        for (j = 0; j < widthout; ++j) {
          j0 = int(j*sx);
          j1 = j0+1;
          if (j1 >= widthin) j1 = widthin-1;

          dst = dataout+(i*widthout+j)*4;

          // compute average of pixels in the rectangle (i0,j0)-(i1,j1)
          for (k = 0; k < 4; ++k) {
            sum = 0;
            for (ii = i0; ii <= i1; ++ii) {
              for (jj = j0; jj <= j1; ++jj) {
                sum += *(datain+(ii*widthin+jj)*4+k);
              }
            }
            sum /= (j1-j0+1)*(i1-i0+1);
            *dst++ = vuint8(sum);
          }
        }
      }
    }
  }
}


//==========================================================================
//
//  VTexture::MipMap
//
//  Scales image down for next mipmap level, operates in place
//  non-premultiplied
//
//==========================================================================
void VTexture::MipMap (int width, int height, vuint8 *InIn) {
  vuint8 *in = InIn;
  int i, j;
  vuint8 *out = in;

  if (width == 1 || height == 1) {
    // special case when only one dimension is scaled
    int total = width*height/2;
    for (i = 0; i < total; ++i, in += 8, out += 4) {
      out[0] = vuint8((in[0]+in[4])>>1);
      out[1] = vuint8((in[1]+in[5])>>1);
      out[2] = vuint8((in[2]+in[6])>>1);
      out[3] = vuint8((in[3]+in[7])>>1);
    }
    return;
  }

  // scale down in both dimensions
  width <<= 2;
  height >>= 1;
  for (i = 0; i < height; ++i, in += width) {
    for (j = 0; j < width; j += 8, in += 8, out += 4) {
      out[0] = vuint8((in[0]+in[4]+in[width+0]+in[width+4])>>2);
      out[1] = vuint8((in[1]+in[5]+in[width+1]+in[width+5])>>2);
      out[2] = vuint8((in[2]+in[6]+in[width+2]+in[width+6])>>2);
      out[3] = vuint8((in[3]+in[7]+in[width+3]+in[width+7])>>2);
    }
  }
}


//==========================================================================
//
//  VTexture::getPixel
//
//==========================================================================
rgba_t VTexture::getPixel (int x, int y) {
  rgba_t col = rgba_t(0, 0, 0, 0);
  if (x < 0 || y < 0 || x >= Width || y >= Height) return col;

  const vuint8 *data = (const vuint8 *)GetPixels();
  if (!data) return col;

  int pitch = 0;
  switch (Format) {
    case TEXFMT_8: case TEXFMT_8Pal: pitch = 1; break;
    case TEXFMT_RGBA: pitch = 4; break;
    default: return col;
  }

  data += y*(Width*pitch)+x*pitch;
  switch (Format) {
    case TEXFMT_8: col = r_palette[*data]; break;
    case TEXFMT_8Pal: col = (GetPalette() ? GetPalette()[*data] : r_palette[*data]); break;
    case TEXFMT_RGBA: col = *((const rgba_t *)data); break;
    default: return col;
  }

  return col;
}


//==========================================================================
//
//  VTexture::CalcRealDimensions
//
//==========================================================================
void VTexture::CalcRealDimensions () {
  RealX0 = RealY0 = 0;
  RealHeight = Height;
  RealWidth = Width;
  // x0
  while (RealX0 < Width) {
    bool found = false;
    for (int y = 0; y < Height; ++y) if (getPixel(RealX0, y).a != 0) { found = true; break; }
    if (found) break;
    ++RealX0;
  }
  // y0
  while (RealY0 < Height) {
    bool found = false;
    for (int x = 0; x < Width; ++x) if (getPixel(x, RealY0).a != 0) { found = true; break; }
    if (found) break;
    ++RealY0;
  }
  // width
  while (RealWidth > 0) {
    bool found = false;
    for (int y = 0; y < Height; ++y) if (getPixel(RealWidth-1, y).a != 0) { found = true; break; }
    if (found) break;
    --RealWidth;
  }
  // height
  while (RealHeight > 0) {
    bool found = false;
    for (int x = 0; x < Width; ++x) if (getPixel(x, RealHeight-1).a != 0) { found = true; break; }
    if (found) break;
    --RealHeight;
  }
}


//==========================================================================
//
//  VTexture::ConvertPixelsToRGBA
//
//==========================================================================
void VTexture::ConvertPixelsToRGBA () {
  if (Width > 0 && Height > 0 && mFormat != TEXFMT_RGBA) {
    vassert(Pixels);
    vassert(mFormat == TEXFMT_8 || mFormat == TEXFMT_8Pal);
    rgba_t *newpic = new rgba_t[Width*Height];
    rgba_t *dest = newpic;
    const rgba_t *pal = (mFormat == TEXFMT_8Pal ? GetPalette() : r_palette);
    const vuint8 *pic = Pixels;
    if (!pal) pal = r_palette;
    for (int f = Width*Height; f > 0; --f, ++pic, ++dest) {
      *dest = pal[*pic];
      dest->a = (*pic ? 255 : 0); // just in case
    }
    delete[] Pixels;
    Pixels = (vuint8 *)newpic;
    if (Pixels8BitValid) { delete Pixels8Bit; Pixels8Bit = nullptr; Pixels8BitValid = false; }
    if (Pixels8BitAValid) { delete Pixels8BitA; Pixels8BitA = nullptr; Pixels8BitAValid = false; }
  }
  mFormat = TEXFMT_RGBA;
}


//==========================================================================
//
//  VTexture::ConvertPixelsToShaded
//
//==========================================================================
void VTexture::ConvertPixelsToShaded () {
  if (shadeColor == -1) return; // nothing to do
  int sc = shadeColor;
  shadeColor = -1; // no more conversions, and return original format info
  ConvertPixelsToRGBA();
  if (sc >= 0) shadePixelsRGBA(sc); else stencilPixelsRGBA(sc&0xffffff);
}


//==========================================================================
//
//  VTexture::shadePixelsRGBA
//
//  use image as alpha-map
//
//==========================================================================
void VTexture::shadePixelsRGBA (int shadeColor) {
  if (Width < 1 || Height < 1) return;
  vassert(Pixels);
  vassert(shadeColor >= 0);
  vassert(mFormat == TEXFMT_RGBA);
  const vuint8 shadeR = (shadeColor>>16)&0xff;
  const vuint8 shadeG = (shadeColor>>8)&0xff;
  const vuint8 shadeB = (shadeColor)&0xff;
  rgba_t *pic = (rgba_t *)Pixels;
  for (int f = Width*Height; f--; ++pic) {
    // use red as intensity
    vuint8 intensity = pic->r;
    /*k8: nope
    // use any non-zero color as intensity
    if (!intensity) {
           if (pic->g) intensity = pic->g;
      else if (pic->b) intensity = pic->b;
    }
    */
    pic->r = shadeR;
    pic->g = shadeG;
    pic->b = shadeB;
    pic->a = intensity;
  }
}


//==========================================================================
//
//  VTexture::stencilPixelsRGBA
//
//==========================================================================
void VTexture::stencilPixelsRGBA (int shadeColor) {
  if (Width < 1 || Height < 1) return;
  vassert(Pixels);
  vassert(shadeColor >= 0);
  vassert(mFormat == TEXFMT_RGBA);
  const float shadeR = (shadeColor>>16)&0xff;
  const float shadeG = (shadeColor>>8)&0xff;
  const float shadeB = (shadeColor)&0xff;
  rgba_t *pic = (rgba_t *)Pixels;
  for (int f = Width*Height; f > 0; --f, ++pic) {
    float intensity = colorIntensity(pic->r, pic->g, pic->b)/255.0f;
    pic->r = clampToByte(intensity*shadeR);
    pic->g = clampToByte(intensity*shadeG);
    pic->b = clampToByte(intensity*shadeB);
  }
}


//==========================================================================
//
//  VTexture::CreateShadedPixels
//
//==========================================================================
rgba_t *VTexture::CreateShadedPixels (vuint32 shadeColor, const rgba_t *palette) {
  rgba_t *res = (rgba_t *)Z_Malloc(Width*Height*sizeof(rgba_t));
  rgba_t *dest = res;

  const vuint8 shadeR = (shadeColor>>16)&0xff;
  const vuint8 shadeG = (shadeColor>>8)&0xff;
  const vuint8 shadeB = (shadeColor)&0xff;

  (void)GetPixels(); // this updates warp and other textures
  // create shaded data
  if (Format == TEXFMT_8 || Format == TEXFMT_8Pal) {
    //GLog.Logf(NAME_Debug, "*** FMT8(%s): 0x%08x", *W_FullLumpName(SourceLump), shadeColor);
    const vuint8 *src = (const vuint8 *)Pixels;
    for (int f = Width*Height; f--; ++dest, ++src) {
      dest->r = shadeR;
      dest->g = shadeG;
      dest->b = shadeB;
      dest->a = *src;
    }
  } else {
    vassert(Format == TEXFMT_RGBA);
    //GLog.Logf(NAME_Debug, "*** FMT32(%s): 0x%08x", *W_FullLumpName(SourceLump), shadeColor);
    const rgba_t *src = (const rgba_t *)Pixels;
    for (int f = Width*Height; f--; ++dest, ++src) {
      #if 1
      const vuint8 ci = colorIntensity(src->r, src->g, src->b);
      dest->r = shadeR;
      dest->g = shadeG;
      dest->b = shadeB;
      dest->a = ci;
      #elif 0
      const vuint8 ci = src->r;
      const float intensity = (int)ci/255.0f;
      dest->r = clampToByte(intensity*shadeR);
      dest->g = clampToByte(intensity*shadeG);
      dest->b = clampToByte(intensity*shadeB);
      dest->a = ci;
      #else
      dest->r = clampToByte((shadeR/255.0f)*(src->r/255.0f)*255.0f);
      dest->g = clampToByte((shadeR/255.0f)*(src->g/255.0f)*255.0f);
      dest->b = clampToByte((shadeR/255.0f)*(src->b/255.0f)*255.0f);
      dest->a = colorIntensity(src->r, src->g, src->b);
      #endif
    }
  }

  // remap to another palette?
  if (palette) {
    dest = res;
    for (int f = Width*Height; f--; ++dest) {
      const vuint8 pidx = R_LookupRGB(dest->r, dest->g, dest->b);
      dest->r = palette[pidx].r;
      dest->g = palette[pidx].g;
      dest->b = palette[pidx].b;
    }
  }

  return res;
}


//==========================================================================
//
//  VTexture::FreeShadedPixels
//
//==========================================================================
void VTexture::FreeShadedPixels (rgba_t *shadedPixels) {
  if (shadedPixels) Z_Free(shadedPixels);
}


//==========================================================================
//
//  VTexture::GetAverageColor
//
//==========================================================================
rgb_t VTexture::GetAverageColor (vuint32 maxout) {
  if (Width < 1 || Height < 1) return rgb_t(255, 255, 255);

  unsigned int r = 0, g = 0, b = 0;
  (void)GetPixels();
  ConvertPixelsToRGBA();

  const rgba_t *pic = (const rgba_t *)Pixels;
  for (int f = Width*Height; f--; ++pic) {
    r += pic->r;
    g += pic->g;
    b += pic->b;
  }

  r /= (unsigned)(Width*Height);
  g /= (unsigned)(Width*Height);
  b /= (unsigned)(Width*Height);

  unsigned int maxv = max3(r, g, b);

  if (maxv && maxout) {
    r = scaleUInt(r, maxout, maxv);
    g = scaleUInt(g, maxout, maxv);
    b = scaleUInt(b, maxout, maxv);
  }

  return rgb_t(clampToByte(r), clampToByte(g), clampToByte(b));
}


//==========================================================================
//
//  VTexture::ResizeCanvas
//
//==========================================================================
void VTexture::ResizeCanvas (int newwdt, int newhgt) {
  if (newwdt < 1 || newhgt < 1) return;
  if (newwdt == Width && newhgt == Height) return;
  (void)GetPixels();
  ConvertPixelsToRGBA();
  rgba_t *newpic = new rgba_t[newwdt*newhgt];
  memset((void *)newpic, 0, newwdt*newhgt*sizeof(rgba_t));
  rgba_t *dest = newpic;
  const rgba_t *oldpix = (const rgba_t *)Pixels;
  for (int y = 0; y < newhgt; ++y) {
    for (int x = 0; x < newwdt; ++x) {
      if (x < Width && y < Height) {
        dest[y*newwdt+x] = oldpix[y*Width+x];
      }
    }
  }
  delete [] Pixels;
  Pixels = (vuint8 *)newpic;
  Width = newwdt;
  Height = newhgt;
}


//==========================================================================
//
//  VTexture::FilterFringe
//
//==========================================================================
void VTexture::FilterFringe (rgba_t *pic, int wdt, int hgt) {
#define GETPIX(dest,_sx,_sy)  do { \
  int sx = (_sx), sy = (_sy); \
  if (sx >= 0 && sy >= 0 && sx < wdt && sy < hgt) { \
    (dest) = pic[sy*wdt+sx]; \
  } else { \
    (dest) = rgba_t(0, 0, 0, 0); \
  } \
} while (0)

  if (!pic || wdt < 1 || hgt < 1) return;
  for (int y = 0; y < hgt; ++y) {
    for (int x = 0; x < wdt; ++x) {
      rgba_t *dp = &pic[y*wdt+x];
      if (dp->a != 0) continue;
      if (dp->r != 0 || dp->g != 0 || dp->b != 0) continue;
      int r = 0, g = 0, b = 0, cnt = 0;
      rgba_t px;
      for (int dy = -1; dy < 2; ++dy) {
        for (int dx = -1; dx < 2; ++dx) {
          GETPIX(px, x+dx, y+dy);
          if (px.a != 0)
          //if (px.r != 0 || px.g != 0 || px.b != 0)
          {
            r += px.r;
            g += px.g;
            b += px.b;
            ++cnt;
          }
        }
      }
      if (cnt > 0) {
        dp->r = clampToByte(r/cnt);
        dp->g = clampToByte(g/cnt);
        dp->b = clampToByte(b/cnt);
      }
    }
  }

#undef GETPIX
}


//==========================================================================
//
//  VTexture::PremultiplyImage
//
//==========================================================================
void VTexture::PremultiplyImage (rgba_t *pic, int wdt, int hgt) {
  if (!pic || wdt < 1 || hgt < 1) return;
  for (int ofs = wdt*hgt; ofs--; ++pic) {
    pic->r = clampToByte((int)((float)pic->r*(pic->a/255.0f)));
    pic->g = clampToByte((int)((float)pic->g*(pic->a/255.0f)));
    pic->b = clampToByte((int)((float)pic->b*(pic->a/255.0f)));
  }
}


//==========================================================================
//
//  VTexture::Shade
//
//==========================================================================
void VTexture::Shade (int shade) {
  if (shadeColor == shade) return;
  vassert(shadeColor == -1);
  // remember shading
  shadeColor = shade;
}


//==========================================================================
//
//  VTexture::checkerFill8
//
//==========================================================================
void VTexture::checkerFill8 (vuint8 *dest, int width, int height) {
  if (!dest || width < 1 || height < 1) return;
  vuint8 bc = R_LookupRGB(0, 0, 192);
  vuint8 wc = R_LookupRGB(200, 96, 0);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      *dest++ = (((x/8)^(y/8))&1 ? wc : bc);
    }
  }
}


//==========================================================================
//
//  VTexture::checkerFillRGB
//
//  alpha <0 means 3-byte RGB texture
//
//==========================================================================
void VTexture::checkerFillRGB (vuint8 *dest, int width, int height, int alpha) {
  if (!dest || width < 1 || height < 1) return;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      if (((x/8)^(y/8))&1) {
        *dest++ = 200;
        *dest++ = 96;
        *dest++ = 0;
      } else {
        *dest++ = 0;
        *dest++ = 0;
        *dest++ = 192;
      }
      if (alpha >= 0) *dest++ = clampToByte(alpha);
    }
  }
}


//==========================================================================
//
//  VTexture::checkerFillColumn8
//
//  `dest` points at column, `x` is used only to build checker
//
//==========================================================================
void VTexture::checkerFillColumn8 (vuint8 *dest, int x, int pitch, int height) {
  if (!dest || pitch < 1 || height < 1) return;
  for (int y = 0; y < height; ++y) {
    *dest = (((x/8)^(y/8))&1 ? r_white_color : r_black_color);
    dest += pitch;
  }
}


//==========================================================================
//
//  VTexture::WriteToPNGFile
//
//  `dest` points at column, `x` is used only to build checker
//
//==========================================================================
void VTexture::WriteToPNG (VStream *strm) {
  if (!strm) return;
  (void)GetPixels();
  ConvertPixelsToRGBA();
  if (!M_CreatePNG(strm, (const vuint8 *)GetPixels(), /*pal*/nullptr, SS_RGBA, Width, Height, Width*4, 1.0f)) {
    GCon->Log(NAME_Error, "Error writing png");
  }
}



//==========================================================================
//
//  VDummyTexture::VDummyTexture
//
//==========================================================================
VDummyTexture::VDummyTexture () {
  Type = TEXTYPE_Null;
  mFormat = mOrigFormat = TEXFMT_8;
}


//==========================================================================
//
//  VDummyTexture::GetPixels
//
//==========================================================================
vuint8 *VDummyTexture::GetPixels () {
  transFlags = TransValueSolid; // anyway
  return nullptr;
}
