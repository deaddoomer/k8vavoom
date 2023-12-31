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

#define VV_INTENSITY_FAST_GAMMA

//#define VV_VERY_VERBOSE_TEXTURE_LOADER
static VCvarB dbg_verbose_texture_loader("dbg_verbose_texture_loader", false, "WERY verbose texture loader?", CVAR_PreInit|CVAR_NoShadow);
static VCvarB dbg_verbose_texture_crop("dbg_verbose_texture_crop", false, "WERY verbose texture loader?", CVAR_PreInit|CVAR_Archive|CVAR_NoShadow);

VTexture *VTexture::gcHead = nullptr;
VTexture *VTexture::gcTail = nullptr;


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
  VCheckedStream Strm(lumpstream);
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
  , bForceNoFilter(false)
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
  , BrightmapBase(nullptr)
  , noDecals(false)
  , staticNoDecals(false)
  , animNoDecals(false)
  , animated(false)
  , needFBO(false)
  , nofullbright(false)
  , glowing(0)
  , noHires(false)
  , hiresRepTex(false)
  , avgcolor(0)
  , maxintensity(+NAN)
  , transFlags(TransValueUnknown)
  , lastTextureFiltering(-666)
  , lastUpdateFrame(0)
  , DriverHandle(0)
  , DriverTranslated()
  , gcLastUsedTime(0.0)
  , gcPrev(nullptr)
  , gcNext(nullptr)
  , Pixels(nullptr)
  , Pixels8Bit(nullptr)
  , Pixels8BitA(nullptr)
  , HiResTexture(nullptr)
  , Pixels8BitValid(false)
  , Pixels8BitAValid(false)
  , shadeColor(-1)
  , shadeColorSaved(-1)
  , alreadyCropped(false)
  , croppedOfsX(0)
  , croppedOfsY(0)
  , precropWidth(0)
  , precropHeight(0)
{
}


//==========================================================================
//
//  VTexture::~VTexture
//
//==========================================================================
VTexture::~VTexture () {
  MarkGCUnused();
  BrightmapBase = nullptr; // anyway
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
//  VTexture::MarkGCUsed
//
//  `time` must never be less than the previous used time
//
//==========================================================================
void VTexture::MarkGCUsed (const double time) noexcept {
  if (time == 0.0 || SourceLump < 0 || !HasPixels()) return MarkGCUnused();
  if (gcHead && gcHead->gcLastUsedTime > time) Sys_Error("invalid time value in `VTexture::MarkGCUsed()`");
  if (gcHead != this) {
    // remove from the list
    MarkGCUnused();
    // append to the top
    gcNext = gcHead;
    if (gcHead) gcHead->gcPrev = this; else gcTail = this;
    gcNext = gcHead;
    gcHead = this;
  }
  gcLastUsedTime = time;
  //GCon->Logf(NAME_Debug, "VTexture::MarkGCUsed: added texture '%s'", *W_FullLumpName(SourceLump));
}


//==========================================================================
//
//  VTexture::MarkGCUnused
//
//==========================================================================
void VTexture::MarkGCUnused () noexcept {
  if (gcLastUsedTime == 0.0) {
    assert(!gcPrev);
    assert(!gcNext);
    return;
  }
  if (gcPrev) gcPrev->gcNext = gcNext; else gcHead = gcHead->gcNext;
  if (gcNext) gcNext->gcPrev = gcPrev; else gcTail = gcTail->gcPrev;
  gcPrev = gcNext = nullptr;
  gcLastUsedTime = 0.0;
  //GCon->Logf(NAME_Debug, "VTexture::MarkGCUnused: released texture '%s'", *W_FullLumpName(SourceLump));
}


//==========================================================================
//
//  VTexture::GCStep
//
//  free unused texture pixels
//
//==========================================================================
void VTexture::GCStep (const double currtime) {
  if (currtime <= 0.0) return;
  while (gcTail) {
    VTexture *tx = gcTail;
    const double diff = currtime-tx->gcLastUsedTime;
    if (diff < 3.0) break;
    GCon->Logf(NAME_Debug, "VTexture::GCStep: freeing pixels of texture '%s'", *W_FullLumpName(tx->SourceLump));
    tx->ReleasePixels();
    tx->MarkGCUnused();
  }
  #if 0
  if (gcHead) {
    GCon->Log(NAME_Debug, "=========== VTexture::GCStep list ===========");
    for (VTexture *tx = gcHead; tx; tx = tx->gcNext) {
      GCon->Logf(NAME_Debug, "VTexture::GCStep: delta=%g; texture '%s'", currtime-tx->gcLastUsedTime, *W_FullLumpName(tx->SourceLump));
    }
  }
  #endif
}


//==========================================================================
//
//  VTexture::IsDynamicTexture
//
//==========================================================================
bool VTexture::IsDynamicTexture () const noexcept {
  return false;
}


//==========================================================================
//
//  VTexture::IsDynamicTexture
//
//==========================================================================
bool VTexture::IsMultipatch () const noexcept {
  return false;
}


//==========================================================================
//
//  VTexture::IsHugeTexture
//
//==========================================================================
bool VTexture::IsHugeTexture () const noexcept {
  return (hiresRepTex && (Width >= 256 || Height >= 256));
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
//  returns 0 if not, positive if only data need to be updated, or
//  negative to recreate texture
//
//==========================================================================
int VTexture::CheckModified () {
  return 0;
}


//==========================================================================
//
//  VTexture::PixelsReleased
//
//  it's not enough to check `Pixels`; use this instead
//
//==========================================================================
bool VTexture::PixelsReleased () const noexcept {
  return (!Pixels && !Pixels8Bit && !Pixels8BitA);
}


//==========================================================================
//
//  VTexture::ReleasePixels
//
//  this is called for textures with `SrcLump < 0`
//
//==========================================================================
bool VTexture::IsSpecialReleasePixelsAllowed () {
  return false;
}


//==========================================================================
//
//  VTexture::ReleasePixels
//
//==========================================================================
void VTexture::ReleasePixels () {
  if (SourceLump < 0) {
    if (!IsSpecialReleasePixelsAllowed()) return; // this texture cannot be reloaded
  }
  //GCon->Logf(NAME_Debug, "VTexture::ReleasePixels: '%s' (%d: %s)", *Name, SourceLump, *W_FullLumpName(SourceLump));
  if (InReleasingPixels()) return; // already released
  // safeguard
  if (PixelsReleased()) {
    // just in case
    mFormat = mOrigFormat; // undo `ConvertPixelsToRGBA()`
    return;
  }
  MarkGCUnused();
  ReleasePixelsLock rlock(this);
  if (Pixels) { delete[] Pixels; Pixels = nullptr; }
  if (Pixels8Bit) { delete[] Pixels8Bit; Pixels8Bit = nullptr; }
  if (Pixels8BitA) { delete[] Pixels8BitA; Pixels8BitA = nullptr; }
  Pixels8BitValid = Pixels8BitAValid = false;
  mFormat = mOrigFormat; // undo `ConvertPixelsToRGBA()`
  // restore offset
  if (alreadyCropped) {
    SOffset += croppedOfsX;
    TOffset += croppedOfsY;
    Width = precropWidth;
    Height = precropHeight;
  }
  alreadyCropped = false;
  croppedOfsX = croppedOfsY = 0;
  if (Brightmap) Brightmap->ReleasePixels();
}


//==========================================================================
//
//  VTexture::GetPixels8
//
//==========================================================================
vuint8 *VTexture::GetPixels8 () {
  // if already have converted version, then just return it
  //GCon->Logf(NAME_Debug, "VTexture::GetPixels8: '%s' (%d: %s) : %p", *Name, SourceLump, *W_FullLumpName(SourceLump), Pixels8Bit);
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
    for (int i = 0; i < NumPixels; ++i, ++pSrc, ++pDst) {
      const vuint8 pv = *pSrc;
      if (!pv) transFlags |= FlagTransparent; else transFlags |= FlagHasSolidPixel;
      *pDst = Remap[pv];
    }
    Pixels8BitValid = true;
    return Pixels8Bit;
  } else if (Format == TEXFMT_RGBA) {
    transFlags = TransValueSolid; // for now
    int NumPixels = Width*Height;
    if (!Pixels8Bit) Pixels8Bit = new vuint8[NumPixels];
    const rgba_t *pSrc = (rgba_t *)pixdata;
    vuint8 *pDst = Pixels8Bit;
    for (int i = 0; i < NumPixels; ++i, ++pSrc, ++pDst) {
      if (pSrc->a < 128) {
        *pDst = 0;
        transFlags |= FlagTransparent;
        //if (pSrc->a) transFlags |= FlagTranslucent;
      } else {
        *pDst = R_LookupRGB(pSrc->r, pSrc->g, pSrc->b);
        transFlags |= FlagHasSolidPixel;
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
  //GCon->Logf(NAME_Debug, "VTexture::GetPixels8A: '%s' (%d: %s) : %p", *Name, SourceLump, *W_FullLumpName(SourceLump), Pixels8BitA);
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
    const vuint8 *pSrc = (const vuint8 *)pixdata;
    for (int i = 0; i < NumPixels; ++i, ++pSrc, ++pDst) {
      const vuint8 pv = *pSrc;
      if (!pv) transFlags |= FlagTransparent; else transFlags |= FlagHasSolidPixel;
      pDst->idx = remap[pv];
      pDst->a = (pv ? 255 : 0);
    }
  } else if (Format == TEXFMT_RGBA) {
    transFlags = TransValueSolid; // for now
    //GCon->Logf("*** remapping 32-bit '%s' to 8A (%dx%d)", *Name, Width, Height);
    const rgba_t *pSrc = (const rgba_t *)pixdata;
    for (int i = 0; i < NumPixels; ++i, ++pSrc, ++pDst) {
      pDst->idx = R_LookupRGB(pSrc->r, pSrc->g, pSrc->b);
      pDst->a = pSrc->a;
      if (pSrc->a != 255) transFlags |= (pSrc->a ? FlagTranslucent : FlagTransparent);
      else transFlags |= FlagHasSolidPixel;
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

  // ignore nameless and special font textures
  if (Name == NAME_None) return nullptr;
  const char *namestr = *Name;
  if (namestr[0] == '\x7f') return nullptr;

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
  int LumpNum = W_FindLumpByFileNameWithExts(VStr("hirestex/")+DirName+"/"+namestr, Exts);
  if (LumpNum >= 0) {
    // create new high-resolution texture
    HiResTexture = CreateTexture(Type, LumpNum);
    if (HiResTexture) {
      HiResTexture->hiresRepTex = true;
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
//  normalize 8-bit palette, remap color 0
//
//==========================================================================
void VTexture::FixupPalette (rgba_t Palette[256], bool forceOpacity, bool transparent0) {
  if (!Palette) return;
  const int black = R_ProcessPalette(Palette, forceOpacity);
  if (Width < 1 || Height < 1) return;
  vassert(Pixels);
  uint8_t *pix = (uint8_t *)Pixels;
  for (int i = Width*Height; i--; ++pix) {
    if (!transparent0 && *pix == 0) {
      *pix = black;
      continue;
    }
    if (Palette[*pix].a == 0) {
      *pix = 0;
    } else if (*pix == 0) {
      *pix = black;
    }
  }
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
  ResetTranslations();
  DriverTranslated.clear();
}


//==========================================================================
//
//  VTexture::ClearTranslationAt
//
//==========================================================================
void VTexture::ClearTranslationAt (int /*idx*/) {
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
  if (shadeColorSaved != -1) { shadeColor = shadeColorSaved; shadeColorSaved = -1; }
  if (shadeColor == -1) return; // nothing to do
  const int sc = shadeColor;
  shadeColorSaved = sc;
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
    const float intensity = colorIntensityGamma2Float(pic->r, pic->g, pic->b);
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
      const vuint8 ci = colorIntensityGamma2(src->r, src->g, src->b);
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
      dest->a = colorIntensityGamma2(src->r, src->g, src->b);
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
//  maxout==1: mult by 1.5
//  maxout==2: mult by 2
//  maxout==3: mult by 2.5
//  maxout==4: mult by 3
//  maxout==5: mult by 3.5
//  maxout==6: mult by 4
//
//==========================================================================
rgb_t VTexture::GetAverageColor (vuint32 maxout) {
  if (Width < 1 || Height < 1) return rgb_t(0, 0, 0);

  unsigned int r = 0, g = 0, b = 0;
  if (avgcolor) {
    r = (avgcolor>>16)&0xff;
    g = (avgcolor>>8)&0xff;
    b = avgcolor&0xff;
  } else {
    #if 0
    (void)GetPixels();
    ConvertPixelsToRGBA();

    const rgba_t *pic = (const rgba_t *)Pixels;
    for (int f = Width*Height; f--; ++pic) {
      r += pic->r;
      g += pic->g;
      b += pic->b;
    }
    #else
    (void)GetPixels();
    for (int y = 0; y < Height; ++y) {
      for (int x = 0; x < Width; ++x) {
        rgba_t c = getPixel(x, y);
        if (c.a == 0) continue;
        r += (unsigned)c.r*(unsigned)c.a/255u;
        g += (unsigned)c.g*(unsigned)c.a/255u;
        b += (unsigned)c.b*(unsigned)c.a/255u;
      }
    }
    #endif

    r /= (unsigned)(Width*Height);
    g /= (unsigned)(Width*Height);
    b /= (unsigned)(Width*Height);

    avgcolor = 0xff000000|(clampToByte(r)<<16)|(clampToByte(g)<<8)|clampToByte(b);
  }

  unsigned int maxv = max3(r, g, b);

  if (maxv && maxout) {
    if (maxout >= 1 && maxout <= 6) {
      float mt = 1.0f;
      switch (maxout) {
        case 1: mt = 1.5f; break;
        case 2: mt = 2.0f; break;
        case 3: mt = 2.5f; break;
        case 4: mt = 3.0f; break;
        case 5: mt = 3.5f; break;
        case 6: mt = 4.0f; break;
      }
      r = (unsigned)((float)r*mt);
      g = (unsigned)((float)g*mt);
      b = (unsigned)((float)b*mt);
    } else {
      r = scaleUInt(r, maxout, maxv);
      g = scaleUInt(g, maxout, maxv);
      b = scaleUInt(b, maxout, maxv);
    }
  }

  return rgb_t(clampToByte(r), clampToByte(g), clampToByte(b));
}


//==========================================================================
//
//  VTexture::GetMaxIntensity
//
//  [0..1]
//
//==========================================================================
float VTexture::GetMaxIntensity () {
  if (!isFiniteF(maxintensity)) {
    maxintensity = 0.0f;
    (void)GetPixels();
    for (int y = 0; y < Height; ++y) {
      for (int x = 0; x < Width; ++x) {
        rgba_t c = getPixel(x, y);
        if (c.a == 0) continue;
        // recolor shader formula
        #ifdef VV_INTENSITY_FAST_GAMMA
        const float r = (float)c.r/255.0f;
        const float g = (float)c.g/255.0f;
        const float b = (float)c.b/255.0f;
        const float lcr = r*r;
        const float lcg = g*g;
        const float lcb = b*b;
        const float i = 0.212655f*lcr+0.715158f*lcg+0.072187f*lcb;
        #else
        const float i = c.r*0.2989f+c.g*0.5870f+c.b*0.1140f;
        #endif
        if (i > maxintensity) maxintensity = i;
      }
    }
    #ifdef VV_INTENSITY_FAST_GAMMA
    //if (maxintensity > 0.0f) maxintensity = min2(sqrtf(maxintensity), 1.0f);
    if (maxintensity > 0.0f) maxintensity = sqrtf(maxintensity);
    #else
    maxintensity = clampval(maxintensity/255.0f, 0.0f, 1.0f);
    #endif
    ReleasePixels(); // we don't need it anymore
    //GCon->Logf(NAME_Debug, "Texture '%s': maxintensity=%f", *Name, maxintensity);
  }
  return maxintensity;
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
  delete[] Pixels;
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
  if (shadeColor == shade || shadeColorSaved == shade) return;
  // remember shading
  if (shadeColor != -1) ReleasePixels();
  shadeColor = shade;
  shadeColorSaved = -1;
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
//  VTexture::PrepareBrightmap
//
//  load brightmap texture pixels, crop/resize it if necessary
//  called from `GetPixels()`
//
//==========================================================================
void VTexture::PrepareBrightmap () {
  vassert(Pixels);
  VTexture *base = BrightmapBase;
  if (!BrightmapBase || base == this) return;
  if (base->Width == Width && base->Height == Height && alreadyCropped == base->alreadyCropped) return; // nothing to do here
  //GCon->Logf(NAME_Debug, "***PrepareBrightmap '%s' (for '%s')", *W_FullLumpName(SourceLump), *W_FullLumpName(base->SourceLump));
  if (base->alreadyCropped) {
    ResizeCanvas(base->precropWidth, base->precropHeight);
    CropCanvasTo(base->croppedOfsX, base->croppedOfsY, base->croppedOfsX+base->Width-1, base->croppedOfsY+base->Height-1);
    alreadyCropped = true;
    precropWidth = base->precropWidth;
    precropHeight = base->precropHeight;
    croppedOfsX = base->croppedOfsX;
    croppedOfsY = base->croppedOfsY;
    SOffset -= croppedOfsX;
    TOffset -= croppedOfsY;
  } else {
    // easy deal
    ResizeCanvas(base->Width, base->Height);
  }
}


//==========================================================================
//
//  VTexture::CropTextureTo
//
//  should be called ONLY on uncropped textures!
//  rectangle is inclusive, and must not be empty
//  used by `CropTexture()`
//
//==========================================================================
void VTexture::CropCanvasTo (int x0, int y0, int x1, int y1) {
  vassert(x0 <= x1);
  vassert(y0 <= y1);
  vassert(Pixels);

  // release 8-bit pixel data (it should be recreated)
  if (Pixels8Bit) { delete[] Pixels8Bit; Pixels8Bit = nullptr; }
  if (Pixels8BitA) { delete[] Pixels8BitA; Pixels8BitA = nullptr; }
  Pixels8BitValid = Pixels8BitAValid = false;

  int newwdt = x1-x0+1;
  int newhgt = y1-y0+1;
  vassert(newwdt > 0);
  vassert(newhgt > 0);

  ConvertPixelsToRGBA();

  rgba_t *newpic = new rgba_t[newwdt*newhgt];
  memset((void *)newpic, 0, newwdt*newhgt*sizeof(rgba_t));
  rgba_t *dest = newpic;
  const rgba_t *oldpix = (const rgba_t *)Pixels;
  for (int y = 0; y < newhgt; ++y) {
    for (int x = 0; x < newwdt; ++x) {
      const int ox = x0+x;
      const int oy = y0+y;
      if (ox >= 0 && oy >= 0 && ox < Width && oy < Height) {
        dest[y*newwdt+x] = oldpix[oy*Width+ox];
      }
    }
  }
  delete[] Pixels;
  Pixels = (vuint8 *)newpic;
  Width = newwdt;
  Height = newhgt;

  RealX0 = RealY0 = RealHeight = RealWidth = MIN_VINT32;
}


//==========================================================================
//
//  VTexture::CropTexture
//
//==========================================================================
void VTexture::CropTexture () {
  if (alreadyCropped) return;
  if (Type == TEXTYPE_Null) return;
  if (Width < 1 || Height < 1) return;

  //ReleasePixels();
  (void)GetPixels();

  int x0, y0;
  int x1, y1;

  if (IsMultipatch()) {
    VMultiPatchTexture *mtx = (VMultiPatchTexture *)this;
    x0 = mtx->xmin;
    y0 = mtx->ymin;
    x1 = mtx->xmax;
    y1 = mtx->ymax;
  } else {
    x0 = y0 = +0x7fffff0;
    x1 = y1 = -0x7fffff0;

    if (Format == TEXFMT_RGBA) {
      const rgba_t *pic = (const rgba_t *)Pixels;
      for (int y = 0; y < Height; ++y) {
        for (int x = 0; x < Width; ++x, ++pic) {
          if (pic->a) {
            x0 = min2(x0, x);
            y0 = min2(y0, y);
            x1 = max2(x1, x);
            y1 = max2(y1, y);
          }
        }
      }
    } else {
      const rgba_t *pal = (mFormat == TEXFMT_8Pal ? GetPalette() : r_palette);
      const vuint8 *pic = Pixels;
      if (!pal) pal = r_palette;
      for (int y = 0; y < Height; ++y) {
        for (int x = 0; x < Width; ++x, ++pic) {
          if (*pic && pal[*pic].a) {
            x0 = min2(x0, x);
            y0 = min2(y0, y);
            x1 = max2(x1, x);
            y1 = max2(y1, y);
          }
        }
      }
    }

    // leave one-pixel border
    x0 = max2(0, x0-1);
    y0 = max2(0, y0-1);
    x1 = min2(Width-1, x1+1);
    y1 = min2(Height-1, y1+1);
  }

  const int neww = x1-x0+1;
  const int newh = y1-y0+1;

  // crop it
  if ((x0 == 0 && y0 == 0 && x1 == Width-1 && y1 == Height-1) || neww < 1 || newh < 1) {
    // don't do it again
    precropWidth = Width;
    precropHeight = Height;
    alreadyCropped = true;
    croppedOfsX = 0;
    croppedOfsY = 0;
    // fix brightmap
    if (Brightmap) {
      Brightmap->precropWidth = Width;
      Brightmap->precropHeight = Height;
      Brightmap->alreadyCropped = true;
      Brightmap->croppedOfsX = 0;
      Brightmap->croppedOfsY = 0;
    }
    return;
  }

  // release 8-bit pixel data (it should be recreated)
  if (Pixels8Bit) { delete[] Pixels8Bit; Pixels8Bit = nullptr; }
  if (Pixels8BitA) { delete[] Pixels8BitA; Pixels8BitA = nullptr; }
  Pixels8BitValid = Pixels8BitAValid = false;

  // reload brightmap data
  if (Brightmap) {
    if (Brightmap->Pixels8Bit) { delete[] Brightmap->Pixels8Bit; Brightmap->Pixels8Bit = nullptr; }
    if (Brightmap->Pixels8BitA) { delete[] Brightmap->Pixels8BitA; Brightmap->Pixels8BitA = nullptr; }
    Brightmap->Pixels8BitValid = Brightmap->Pixels8BitAValid = false;
    (void)Brightmap->GetPixels(); // this will properly crop/resize brightmap to the size of this texture
    vassert(!Brightmap->alreadyCropped);
  }

  if (GTextureCropMessages && dbg_verbose_texture_crop.asBool()) {
    GCon->Logf(NAME_Debug, "%s (%s): crop from (0,0)-(%d,%d) to (%d,%d)-(%d,%d) -- %dx%d -> %dx%d", *Name, *W_FullLumpName(SourceLump), Width-1, Height-1, x0, y0, x1, y1, Width, Height, neww, newh);
  }

  if (Format == TEXFMT_RGBA) {
    const rgba_t *pic = (const rgba_t *)Pixels;
    rgba_t *newpic = new rgba_t[neww*newh];
    rgba_t *dest = newpic;
    for (int y = y0; y <= y1; ++y) {
      for (int x = x0; x <= x1; ++x) {
        *dest++ = pic[y*Width+x];
      }
    }
    delete[] Pixels;
    Pixels = (vuint8 *)newpic;
  } else {
    const vuint8 *pic = Pixels;
    vuint8 *newpic = new vuint8[neww*newh];
    vuint8 *dest = newpic;
    for (int y = y0; y <= y1; ++y) {
      for (int x = x0; x <= x1; ++x) {
        *dest++ = pic[y*Width+x];
      }
    }
    delete[] Pixels;
    Pixels = (vuint8 *)newpic;
  }

  // save original size
  precropWidth = Width;
  precropHeight = Height;
  alreadyCropped = true;
  croppedOfsX = x0;
  croppedOfsY = y0;
  SOffset -= x0;
  TOffset -= y0;
  Width = neww;
  Height = newh;

  RealX0 = RealY0 = RealHeight = RealWidth = MIN_VINT32;

  // crop brightmap data
  if (Brightmap) {
    vassert(!Brightmap->alreadyCropped);
    vassert(Brightmap->Width == precropWidth);
    vassert(Brightmap->Height == precropHeight);
    //GCon->Logf(NAME_Debug, "***cropping brightmap '%s' (for '%s')", *W_FullLumpName(Brightmap->SourceLump), *W_FullLumpName(SourceLump));
    Brightmap->precropWidth = Brightmap->Width;
    Brightmap->precropHeight = Brightmap->Height;
    Brightmap->alreadyCropped = true;
    Brightmap->CropCanvasTo(x0, y0, x1, y1);
    Brightmap->croppedOfsX = x0;
    Brightmap->croppedOfsY = y0;
    Brightmap->SOffset -= x0;
    Brightmap->TOffset -= y0;
  }
}


//==========================================================================
//
//  VTexture::ConvertXHairPixels
//
//==========================================================================
void VTexture::ConvertXHairPixels () {
  if (Width < 1 || Height < 1) return;
  if (isTranslucent()) return;

  (void)GetPixels();
  ConvertPixelsToRGBA();

  // it should be grayscale; use red channel as alpha
  rgba_t *pic = (rgba_t *)Pixels;
  for (int f = Width*Height; f--; ++pic) {
    pic->a = pic->r;
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
