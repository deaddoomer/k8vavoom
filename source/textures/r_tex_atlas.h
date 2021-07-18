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
#ifndef VAVOOM_TEXTURE_ATLAS_PUBLIC_HEADER
#define VAVOOM_TEXTURE_ATLAS_PUBLIC_HEADER


class VTexAtlas8bit : public VTexture {
public:
  struct Rect {
  public:
    int x, y, w, h;

    inline Rect (ENoInit) noexcept {}
    inline Rect () noexcept : x(0), y(0), w(0), h(0) {}
    inline Rect (int ax, int ay, int aw, int ah) noexcept : x(ax), y(ay), w(aw), h(ah) {}

    inline bool isValid () const noexcept { return (x >= 0 && y >= 0 && w > 0 && h > 0); }
    inline int area () const noexcept { return w*h; }

    inline void shrinkBy (const int delta) noexcept { x += delta; y += delta; w -= delta*2; h -= delta*2; }
  };

  struct FRect {
  public:
    float x0, y0, x1, y1;

    inline FRect (ENoInit) noexcept {}
    inline FRect () noexcept : x0(0.0f), y0(0.0f), x1(0.0f), y1(0.0f) {}
    inline FRect (const Rect &r, const int w, const int h) noexcept : x0(0.0f), y0(0.0f), x1(0.0f), y1(0.0f) {
      if (r.isValid() && w > 0 && h > 0) {
        x0 = (float)r.x/(float)w;
        y0 = (float)r.y/(float)h;
        x1 = (float)(r.x+r.w)/(float)w;
        y1 = (float)(r.y+r.h)/(float)h;
      }
    }

    inline bool isValid () const noexcept { return (x0 >= 0.0f && y0 >= 0.0f && x1 <= 1.0f && y1 <= 1.0f && x1 > x0 && y1 > y0); }
  };

private:
  enum { BadRect = -1 };

private:
  // node id or BadRect
  int findBestFit (int w, int h) noexcept;

  vuint8 findTransparentColor () const noexcept;

private:
  vuint8 *BytePixels;
  rgba_t *Palette;
  TArray<Rect> rects;

public:
  // will own `aBytePixels`
  VTexAtlas8bit (VName aName, rgba_t *APalette, int aWidth=1024, int aHeight=1024) noexcept;
  virtual ~VTexAtlas8bit () override;
  virtual vuint8 *GetPixels () override;
  virtual rgba_t *GetPalette () override;
  virtual VTexture *GetHighResolutionTexture () override;

  VTexAtlas8bit *Clone (VName aName, rgba_t *APalette) noexcept;

  // returns invalid rect if there's no more room (or invalid dimensions)
  Rect allocSubImage (const int iwdt, const int ihgt) noexcept;

  void setPixel (const Rect &rect, const int x, const int y, const vuint8 clr) noexcept;

  inline FRect convertRect (const Rect &rect) noexcept { return FRect(rect, Width, Height); }
};


#endif
