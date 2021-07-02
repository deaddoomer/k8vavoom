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
#include "../../utils/ntvalueioex.h"  /* VCheckedStream */
#include "../r_tex.h"


static VCvarB tex_strict_multipatch("tex_strict_multipatch", true, "Be strict in multipatch texture building?", CVAR_Archive|CVAR_PreInit);


//==========================================================================
//
//  VPatchTexture::Create
//
//==========================================================================
VTexture *VPatchTexture::Create (VStream &Strm, int LumpNum) {
  if (Strm.TotalSize() < 13) return nullptr; // lump is too small

  Strm.Seek(0);
  int Width = Streamer<vint16>(Strm);
  int Height = Streamer<vint16>(Strm);
  int SOffset = Streamer<vint16>(Strm);
  int TOffset = Streamer<vint16>(Strm);

  if (Width < 1 || Height < 1 || Width > 2048 || Height > 2048) return nullptr; // not valid dimensions
  if (Strm.TotalSize() < Width*4+8) return nullptr; // file has no space for offsets table

  vint32 *Offsets = new vint32[Width];
  for (int i = 0; i < Width; ++i) Strm << Offsets[i];
  // make sure that all offsets are valid and that at least one is right at the end of offsets table
  bool GapAtStart = true;
  for (int i = 0; i < Width; ++i) {
    if (Offsets[i] == Width*4+8) {
      GapAtStart = false;
    } else if (Offsets[i] < Width*4+8 || Offsets[i] >= Strm.TotalSize()) {
      delete[] Offsets;
      return nullptr;
    }
  }
  delete[] Offsets;
  if (GapAtStart) return nullptr;

  return new VPatchTexture(LumpNum, Width, Height, SOffset, TOffset);
}


//==========================================================================
//
//  VPatchTexture::VPatchTexture
//
//==========================================================================
VPatchTexture::VPatchTexture (int ALumpNum, int AWidth, int AHeight, int ASOffset, int ATOffset)
  : VTexture()
{
  SourceLump = ALumpNum;
  Name = W_LumpName(SourceLump);
  mFormat = mOrigFormat = TEXFMT_8;
  Width = AWidth;
  Height = AHeight;
  SOffset = ASOffset;
  TOffset = ATOffset;
}


//==========================================================================
//
//  VPatchTexture::~VPatchTexture
//
//==========================================================================
VPatchTexture::~VPatchTexture () {
  if (Pixels) {
    delete[] Pixels;
    Pixels = nullptr;
  }
}


//==========================================================================
//
//  VPatchTexture::GetPixels
//
//==========================================================================
vuint8 *VPatchTexture::GetPixels () {
  if (Pixels) return Pixels; // if already got pixels, then just return them
  transFlags = TransValueSolid; // for now

  VCheckedStream Strm(SourceLump);
  const int stsize = Strm.TotalSize();

  // make sure header is present
  if (stsize < 8) {
    GCon->Logf(NAME_Warning, "Patch \"%s\" is too small", *Name);
    Width = 1;
    Height = 1;
    SOffset = 0;
    TOffset = 0;
    Pixels = new vuint8[1];
    Pixels[0] = 0;
    ConvertPixelsToShaded();
    PrepareBrightmap();
    transFlags |= FlagTransparent;
    return Pixels;
  }

  // read header
  Width = Streamer<vint16>(Strm);
  Height = Streamer<vint16>(Strm);
  SOffset = Streamer<vint16>(Strm);
  TOffset = Streamer<vint16>(Strm);

  if (Width < 1 || Height < 1) {
    GCon->Logf(NAME_Error, "Patch \"%s\" has invalid size: width=%d; height=%d", *Name, Width, Height);
    Width = 1;
    Height = 1;
    checkerFill8(Pixels, Width, Height);
    ConvertPixelsToShaded();
    PrepareBrightmap();
    return Pixels;
  }

  //if (VStr::strEquCI(*Name, "fsky1t")) GCon->Logf(NAME_Debug, "%s: %dx%d; offset: (%d,%d); fofs=0x%08x", *Name, Width, Height, SOffset, TOffset, (unsigned)(8+Width*4));

  // allocate image data
  Pixels = new vuint8[Width*Height];
  memset(Pixels, 0, Width*Height);

  // make sure all column offsets are there
  if (stsize < 8+Width*4) {
    GCon->Logf(NAME_Warning, "Patch \"%s\" is too small", *Name);
    checkerFill8(Pixels, Width, Height);
    ConvertPixelsToShaded();
    PrepareBrightmap();
    return Pixels;
  }

  // read data
  for (int x = 0; x < Width; ++x) {
    // get offset of the column
    Strm.Seek(8+x*4);
    vint32 Offset = Streamer<vint32>(Strm);
    if (Offset < 8+Width*4 || Offset > stsize-1) {
      GCon->Logf(NAME_Warning, "Bad offset for column %d in patch \"%s\"", x, *Name);
      //checkerFillColumn8(Pixels+x, x, Width, Height);
      if (tex_strict_multipatch) checkerFill8(Pixels, Width, Height);
      continue;
    }
    Strm.Seek(Offset);
    //if (VStr::strEquCI(*Name, "fsky1t")) GCon->Logf(NAME_Debug, "%s:  x=%d; fofs=0x%08x", *Name, x, (unsigned)Offset);

    // step through the posts in a column
    int top = -1; // DeepSea tall patches support
    bool warnTooLong = false;
    bool dontDraw = false;
    vuint8 TopDelta;
    Strm << TopDelta;
    while (TopDelta != 0xff) {
      // make sure length is there
      if (stsize-Strm.Tell() < 2) {
        GCon->Logf(NAME_Warning, "Broken column %d in patch \"%s\" (length)", x, *Name);
        //checkerFillColumn8(Pixels+x, x, Width, Height);
        if (tex_strict_multipatch) checkerFill8(Pixels, Width, Height);
        x = Width;
        break;
      }

      // calculate top offset
      if (TopDelta <= top) top += TopDelta; else top = TopDelta;

      // read column length and skip unused byte
      vuint8 ByteLen;
      Strm << ByteLen;
      Streamer<vuint8>(Strm); // garbage byte
      // patch of exactly 256 pixels hight, and with zero offset can have 256 bytes of data
      // otherwise it is zero bytes of data
      // shit
      int Len = ByteLen;
      if (!ByteLen && Height == 256 && TopDelta == 0) Len = 256;
      //if (VStr::strEquCI(*Name, "fsky1t")) GCon->Logf(NAME_Debug, "%s:   x=%d; Len=%d; top=%d; end=%d", *Name, x, Len, top, top+Len);

      // make sure column doesn't go out of the bounds of the image
      if (top+Len > Height) {
        if (!warnTooLong) {
          GCon->Logf(NAME_Warning, "Column %d too long in patch \"%s\"", x, *Name);
          warnTooLong = true;
        }
        //checkerFillColumn8(Pixels+x, x, Width, Height);
        if (tex_strict_multipatch) {
          checkerFill8(Pixels, Width, Height);
          x = Width;
          break;
        }
        dontDraw = true;
      }

      // make sure all post data is there
      if (stsize-Strm.Tell() < Len) {
        GCon->Logf(NAME_Warning, "Broken column %d in patch \"%s\" (missing %d bytes)", x, *Name, stsize-Strm.Tell()-Len);
        //checkerFillColumn8(Pixels+x, x, Width, Height);
        if (tex_strict_multipatch) {
          checkerFill8(Pixels, Width, Height);
          x = Width;
          break;
        }
        Len = stsize-Strm.Tell();
        if (Len <= 0) {
          x = Width;
          break;
        }
      }

      // read post, convert color 0 to black if needed
      int count = Len;
      vuint8 *dest = Pixels+top*Width+x;
      int currY = top;
      while (count--) {
        vuint8 b = 0;
        Strm << b;
        if (!dontDraw && currY >= 0 && currY < Height) {
          *dest = b;
          if (!dest[0] && !bNoRemap0) *dest = r_black_color;
        }
        ++currY;
        dest += Width;
      }

      // make sure unused byte and next post's top offset is there
      if (stsize-Strm.Tell() < 2) {
        if (tex_strict_multipatch) {
          GCon->Logf(NAME_Warning, "Broken column %d in patch \"%s\" (missing end marker)", x, *Name);
          //checkerFillColumn8(Pixels+x, x, Width, Height);
          checkerFill8(Pixels, Width, Height);
        }
        x = Width;
        break;
      }

      // skip unused byte and get top offset of the next post
      Streamer<vuint8>(Strm);
      Strm << TopDelta;
    }
  }

  if (Width > 0 && Height > 0) {
    const vuint8 *s = Pixels;
    for (int count = Width*Height; count--; ++s) {
      if ((transFlags |= (s[0] == 0 ? FlagTransparent : FlagHasSolidPixel)) == (FlagTransparent|FlagHasSolidPixel)) break;
    }
  }

  // cache average color for small images
  if (transFlags == TransValueSolid && Width > 0 && Height > 0 && Width <= 512 && Height <= 512) (void)GetAverageColor(0);

  ConvertPixelsToShaded();
  PrepareBrightmap();
  return Pixels;
}
