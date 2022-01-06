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
//**  Copyright (C) 2018-2022 Ketmar Dark
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
#ifndef IMAGO_HEADER_FILE
#define IMAGO_HEADER_FILE

#include "../core/core.h"


class VImage {
public:
  // see https://www.compuphase.com/cmetric.htm
  static inline VVA_OKUNUSED int32_t rgbDistanceSquared (
      const uint8_t r0, const uint8_t g0, const uint8_t b0,
      const uint8_t r1, const uint8_t g1, const uint8_t b1) noexcept
  {
    const int32_t rmean = ((int32_t)r0+(int32_t)r1)/2;
    const int32_t r = (int32_t)r0-(int32_t)r1;
    const int32_t g = (int32_t)g0-(int32_t)g1;
    const int32_t b = (int32_t)b0-(int32_t)b1;
    return (((512+rmean)*r*r)/256)+4*g*g+(((767-rmean)*b*b)/256);
  }

  // texture data formats
  enum ImageType {
    IT_Pal,  // paletised
    IT_RGBA, // truecolor
  };

  struct __attribute__((aligned(1), packed)) RGB;
  struct __attribute__((aligned(1), packed)) RGBA;

  struct __attribute__((aligned(1), packed)) RGB {
    vuint8 r, g, b;
    inline RGB () noexcept : r(0), g(0), b(0) {}
    inline RGB (const vuint8 ar, const vuint8 ag, const vuint8 ab) noexcept : r(ar), g(ag), b(ab) {}
    inline RGB (const vuint32 rgb) noexcept : r((rgb>>16)&0xffu), g((rgb>>8)&0xffu), b(rgb&0xffu) {}
    inline RGB (const RGB &c) noexcept : r(c.r), g(c.g), b(c.b) {}
    inline RGB (const RGBA &c) noexcept : r(c.r), g(c.g), b(c.b) {}
    inline bool operator == (const RGB &c) const noexcept { return (r == c.r && g == c.g && b == c.b); }
    inline bool operator == (const RGBA &c) const noexcept { return (c.a == 255 && r == c.r && g == c.g && b == c.b); }
    //inline int distSq (const RGB &c) const noexcept { return (r-c.r)*(r-c.r)+(g-c.g)*(g-c.g)+(b-c.b)*(b-c.b); }
    //inline int distSq (const RGBA &c) const noexcept { return (r-c.r)*(r-c.r)+(g-c.g)*(g-c.g)+(b-c.b)*(b-c.b)+(255-c.a)*(255-c.a); }
    inline int distSq (const RGBA &c) const noexcept { return rgbDistanceSquared(r, g, b, c.r*c.a/255, c.g*c.a/255, c.b*c.a/255); }
    inline int distSq (const RGB &c) const noexcept { return rgbDistanceSquared(r, g, b, c.r, c.g, c.b); }
    inline bool isTransparent () const noexcept { return false; }
    inline bool isOpaque () const noexcept { return true; }
    // ignores alpha
    inline bool sameColor (const RGB &c) const noexcept { return (r == c.r && g == c.g && b == c.b); }
    inline bool sameColor (const RGBA &c) const noexcept { return (r == c.r && g == c.g && b == c.b); }
    // ignores alpha
    inline void setColor (const RGB &c) noexcept { r = c.r; g = c.g; b = c.b; }
    inline void setColor (const RGBA &c) noexcept { r = c.r; g = c.g; b = c.b; }
  };

  struct __attribute__((aligned(1), packed)) RGBA {
    vuint8  r, g, b, a;
    inline RGBA () noexcept : r(0), g(0), b(0), a(0) {}
    inline RGBA (const vuint8 ar, const vuint8 ag, const vuint8 ab, const vuint8 aa=255) noexcept : r(ar), g(ag), b(ab), a(aa) {}
    inline RGBA (const vuint32 argb) noexcept : r((argb>>16)&0xffu), g((argb>>8)&0xffu), b(argb&0xffu), a((argb>>24)&0xffu) {}
    inline RGBA (const RGB &c) noexcept : r(c.r), g(c.g), b(c.b), a(255) {}
    inline RGBA (const RGBA &c) noexcept : r(c.r), g(c.g), b(c.b), a(c.a) {}
    inline bool operator == (const RGB &c) const noexcept { return (a == 255 && r == c.r && g == c.g && b == c.b); }
    inline bool operator == (const RGBA &c) const noexcept { return (a == c.a && r == c.r && g == c.g && b == c.b); }
    //inline int distSq (const RGB &c) const noexcept { return (r-c.r)*(r-c.r)+(g-c.g)*(g-c.g)+(b-c.b)*(b-c.b)+(255-a)*(255-a); }
    //inline int distSq (const RGBA &c) const noexcept { return (r-c.r)*(r-c.r)+(g-c.g)*(g-c.g)+(b-c.b)*(b-c.b)+(a-c.a)*(a-c.a); }
    inline int distSq (const RGBA &c) const noexcept { return rgbDistanceSquared(r*a/255, g*a/255, b*a/255, c.r*c.a/255, c.g*c.a/255, c.b*c.a/255); }
    inline int distSq (const RGB &c) const noexcept { return rgbDistanceSquared(r*a/255, g*a/255, b*a/255, c.r, c.g, c.b); }
    inline bool isTransparent () const noexcept { return (a == 0); }
    inline bool isOpaque () const noexcept { return (a == 255); }
    // ignores alpha
    inline bool sameColor (const RGB &c) const noexcept { return (r == c.r && g == c.g && b == c.b); }
    inline bool sameColor (const RGBA &c) const noexcept { return (r == c.r && g == c.g && b == c.b); }
    // ignores alpha
    inline void setColor (const RGB &c) noexcept { r = c.r; g = c.g; b = c.b; }
    inline void setColor (const RGBA &c) noexcept { r = c.r; g = c.g; b = c.b; }
  };

protected:
  ImageType mFormat;
  int mWidth, mHeight;
  VStr mName;
  VStr mType;

  vuint8 *mPixels;
  RGBA mPalette[256];
  int mPalUsed; // number of used colors in palette

public:
  inline ImageType getFormat () const noexcept { return mFormat; }
  inline int getWidth () const noexcept { return mWidth; }
  inline int getHeight () const noexcept { return mHeight; }

  inline bool getIsTrueColor () const noexcept { return (mFormat == IT_RGBA); }
  inline bool getHasPalette () const noexcept { return (mPalUsed > 0); }
  inline int getPalUsed () const noexcept { return mPalUsed; }
  inline const RGBA *getPalette () const noexcept { return mPalette; }
  inline const vuint8 *getPixels () const noexcept { return mPixels; }
  inline vuint8 *getWriteablePixels () const noexcept { return mPixels; }

public:
  inline VStr getName () const noexcept { return mName; }
  inline void setName (VStr aname) noexcept { mName = aname; }
  inline VStr getType () const noexcept { return mType; }
  inline void setType (VStr aname) noexcept { mType = aname; }

public:
  PropertyRO<ImageType, VImage> format {this, &VImage::getFormat};
  PropertyRO<int, VImage> width {this, &VImage::getWidth};
  PropertyRO<int, VImage> height {this, &VImage::getHeight};
  //Property<const VStr &, VImage> name {this, &VImage::getName, &VImage::setName};
  //Property<VStr, VImage> type {this, &VImage::getType, &VImage::setType};
  PropertyRO<bool, VImage> isTrueColor {this, &VImage::getIsTrueColor};
  PropertyRO<bool, VImage> hasPalette {this, &VImage::getHasPalette}; // note that non-tc images can still be palette-less
  PropertyRO<int, VImage> palUsed {this, &VImage::getPalUsed};
  PropertyRO<const RGBA *, VImage> palette {this, &VImage::getPalette};
  PropertyRO<const vuint8 *, VImage> pixels {this, &VImage::getPixels};
  PropertyRO<vuint8 *, VImage> writeablePixels {this, &VImage::getWriteablePixels};

private:
  vuint8 appendColor (const RGBA &col) noexcept;

public:
  VImage (const VImage &) = delete; // no copies
  VImage &operator = (const VImage &) = delete; // no copies

  VImage (ImageType atype, int awidth, int aheight) noexcept; // pixels aren't initialized
  ~VImage () noexcept;

  // load image from stream; return `nullptr` on error
  static VImage *loadFrom (VStream *strm, VStr name=VStr::EmptyString);

  bool saveAsPNG (VStream *strm);

  void checkerFill () noexcept;

  void setPalette (const RGBA *pal, int colnum) noexcept;

  RGBA getPixel (const int x, const int y) const noexcept;
  void setPixel (const int x, const int y, const RGBA &col) noexcept;

  // has any sense only for paletted images
  void setBytePixel (const int x, const int y, const vuint8 b) noexcept;

  inline unsigned getPitch () const noexcept { return (mFormat == IT_RGBA ? 4u : 1u); }

  // filters edges in RGBA images
  void smoothEdges () noexcept;
};


// ////////////////////////////////////////////////////////////////////////// //
typedef VImage *(*VImageLoaderFn) (VStream *);

enum { ImagoDefaultPriority = 1000 };

// `ext` may, or may not include dot
// loaders with higher priority will be tried first
void ImagoRegisterLoader (const char *fmtext, const char *fmtdesc, VImageLoaderFn ldr, int prio=1000) noexcept;


#endif
