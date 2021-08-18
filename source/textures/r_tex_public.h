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
#ifndef VAVOOM_TEXTURE_PUBLIC_HEADER
#define VAVOOM_TEXTURE_PUBLIC_HEADER


// texture use types
enum {
  TEXTYPE_Any,
  TEXTYPE_WallPatch,
  TEXTYPE_Wall,
  TEXTYPE_Flat,
  TEXTYPE_Overload,
  TEXTYPE_Sprite,
  TEXTYPE_SkyMap,
  TEXTYPE_Skin,
  TEXTYPE_Pic,
  TEXTYPE_Autopage,
  TEXTYPE_Null,
  TEXTYPE_FontChar,
  //
  TEXTYPE_MAX,
};


// texture data formats
enum {
  TEXFMT_8,    // paletised texture in main palette
  TEXFMT_8Pal, // paletised texture with custom palette
  TEXFMT_RGBA, // truecolor texture
};


struct __attribute__((__packed__)) rgb_t {
  vuint8 r, g, b;
  rgb_t () : r(0), g(0), b(0) {}
  rgb_t (vuint8 ar, vuint8 ag, vuint8 ab) : r(ar), g(ag), b(ab) {}
};
static_assert(sizeof(rgb_t) == 3, "invalid rgb_t size");

struct __attribute__((__packed__)) rgba_t {
  vuint8 r, g, b, a;
  rgba_t () : r(0), g(0), b(0), a(0) {}
  rgba_t (vuint8 ar, vuint8 ag, vuint8 ab, vuint8 aa=255) : r(ar), g(ag), b(ab), a(aa) {}
  static inline rgba_t Transparent () { return rgba_t(0, 0, 0, 0); }
};
static_assert(sizeof(rgba_t) == 4, "invalid rgba_t size");

struct __attribute__((__packed__)) pala_t {
  vuint8 idx, a;
  pala_t () : idx(0), a(0) {}
  pala_t (vuint8 aidx, vuint8 aa=255) : idx(aidx), a(aa) {}
};
static_assert(sizeof(pala_t) == 2, "invalid pala_t size");


struct picinfo_t {
  vint32 width;
  vint32 height;
  vint32 xoffset;
  vint32 yoffset;
  float xscale;
  float yscale;
  vint32 widthNonScaled;
  vint32 heightNonScaled;
  vint32 xoffsetNonScaled;
  vint32 yoffsetNonScaled;
};


struct VAnimDoorDef {
  vint32 Texture;
  VName OpenSound;
  VName CloseSound;
  vint32 *Frames;
  vint32 NumFrames;
};


struct TSwitchFrame {
  VTextureID Texture;
  /*vint16*/int BaseTime;
  /*vint16*/int RandomRange;
};


struct TSwitch {
  VTextureID Tex;
  /*vint16*/int PairIndex;
  /*vint16*/int Sound;
  TSwitchFrame *Frames;
  /*vint16*/int NumFrames;
  bool Quest;
  int InternalIndex;

  inline TSwitch () noexcept : Tex(0), PairIndex(0), Sound(0), Frames(nullptr), NumFrames(0), Quest(false), InternalIndex(0) {}
  inline ~TSwitch () noexcept { if (Frames) { delete[] Frames; Frames = nullptr; } }
};


// ////////////////////////////////////////////////////////////////////////// //
// [0..255]; a=0 is fully transparent, a=255 is fully opaque
struct VColorRGBA {
  vint32 r, g, b, a;
};


// ////////////////////////////////////////////////////////////////////////// //
class VTextureTranslation {
public:
  vuint8 Table[256];
  rgba_t Palette[256];

  vuint32 Crc;
  int nextInCache;

  // used to detect changed player translations
  vuint8 TranslStart;
  vuint8 TranslEnd;
  vint32 Color; // used for blood translations too
  bool isBloodTranslation;

  // used to replicate translation tables in more efficient way
  struct VTransCmd {
    // 0: MapToRange (only Start/End matters)
    // 1: MapToColors (everything matters)
    // 2: MapDesaturated (RGBs are scaled from [0..2] range)
    // 3: MapBlended (only R1,G1,B1 matters)
    // 4: MapTinted (R2 is amount in percents, G2 and B2 doesn't matter)
    vuint8 Type;
    vuint8 Start;
    vuint8 End;
    vuint8 R1;
    vuint8 G1;
    vuint8 B1;
    vuint8 R2;
    vuint8 G2;
    vuint8 B2;
  };
  TArray<VTransCmd> Commands;

public:
  VTextureTranslation ();

  void Clear ();
  void CalcCrc ();
  void Serialise (VStream &Strm);
  void BuildPlayerTrans (int Start, int End, int Col);
  void MapToRange (int AStart, int AEnd, int ASrcStart, int ASrcEnd);
  void MapToColors (int AStart, int AEnd, int AR1, int AG1, int AB1, int AR2, int AG2, int AB2);
  void MapDesaturated (int AStart, int AEnd, float rs, float gs, float bs, float re, float ge, float be);
  void MapBlended (int AStart, int AEnd, int R, int G, int B);
  void MapTinted (int AStart, int AEnd, int R, int G, int B, int Amount);
  void BuildBloodTrans (int Col);
  void AddTransString (VStr Str);

  // not supported over the network yet
  void MapToPalette (const VColorRGBA newpal[768]);

  inline const vuint8 *GetTable () const noexcept { return Table; }
  inline const rgba_t *GetPalette () const noexcept { return Palette; }
};


// ////////////////////////////////////////////////////////////////////////// //
class VTexture {
public:
  int Type; // -1: in `ReleasePixels()`; used to avoid circular loops

protected:
  inline bool InReleasingPixels () const noexcept { return (Type == -1); }

  struct ReleasePixelsLock {
  public:
    VTexture *tex;
    int oldType;
  public:
    VV_DISABLE_COPY(ReleasePixelsLock)
    inline ReleasePixelsLock (VTexture *atex) noexcept : tex(atex), oldType(atex->Type) { atex->Type = -1; }
    inline ~ReleasePixelsLock () noexcept { if (tex) tex->Type = oldType; tex = nullptr; }
  };

protected:
  int mFormat; // never use this directly!
  // original format; `mFormat` can be changed by `ConvertPixelsToRGBA()`, and
  // we need to restore it in `ReleasePixels()`
  int mOrigFormat;

public:
  VName Name;
  int Width;
  int Height;
  int SOffset;
  int TOffset;
  bool bNoRemap0;
  bool bWorldPanning;
  bool bIsCameraTexture;
  bool bForcedSpriteOffset; // do not try to guess sprite offset, if this is set
  int SOffsetFix; // set only if `bForcedSpriteOffset` is true
  int TOffsetFix; // set only if `bForcedSpriteOffset` is true
  vuint8 WarpType;
  float SScale; // horizontal scaling; divide by this to get "view size"
  float TScale; // vertical scaling; divide by this to get "view size"
  int TextureTranslation; // animation
  int HashNext;
  int SourceLump;
  // real picture dimensions (w/o transparent part), in pixels; MIN_VINT32 means "not calculated yet"
  int RealX0;
  int RealY0;
  // this is from (0,0)
  int RealHeight;
  int RealWidth;

  VTexture *Brightmap;
  VTexture *BrightmapBase; // set for brightmap textures

  bool noDecals;
  bool staticNoDecals;
  bool animNoDecals;
  bool animated; // used to select "no decals" flag
  bool needFBO;
  bool nofullbright; // valid for all textures; forces "no fullbright" for sprite brightmaps (ONLY!)
  vuint32 glowing; // is this a glowing texture? (has any meaning only for floors and ceilings; 0: none; if high byte is 0xff, it is fullbright)
  bool noHires; // hires texture tried and not found
  bool hiresRepTex; // set for hires replacements

  vuint32 avgcolor; // if high byte is 0: unknown yet
  float maxintensity; // non-finite: unknown yet

  enum {
    FlagTransparent     = 0x01u, // does texture have any non-solid pixels? set in `GetPixels()`
    FlagTranslucent     = 0x02u, // does texture have some non-integral alpha pixels? set in `GetPixels()`
    FlagHasSolidPixel   = 0x04u, // set if texture has at least one solid pixel (set in `GetPixels()`)
    TransValueSolid     = 0x00u, // this MUST be zero!
    TransValueUnknown   = 0xffu,
  };
  vuint32 transFlags; // default is `TransValueUnknown`
  //bool transparent; // `true` if texture has any non-solid pixels; set in `GetPixels()`
  //bool translucent; // `true` if texture has some non-integral alpha pixels; set in `GetPixels()`
  //   <0: not set
  //    0: nearest, no mipmaps
  //    1: nearest mipmap
  //    2: linear nearest
  //    3: bilinear
  //    4: trilinear
  // bit 7: wrapping (asmodel): set if repeat
  // bits 8-15: (last anisotropy level)-1
  int lastTextureFiltering;

  inline bool filteringAsModel () const noexcept { return (lastTextureFiltering >= 0 && (lastTextureFiltering&0x80) != 0); }

  enum {
    FilterMatch = 0x00u,
    FilterWrongLevel = 0x01u,
    FilterWrongAniso = 0x02u,
    FilterWrongModel = 0x04u,
    FilterWrongAll = FilterWrongLevel|FilterWrongAniso|FilterWrongModel,
  };

  // should NOT be called with invalid arguments!
  inline unsigned checkFiltering (int level, int aniso, bool asModel) const noexcept {
    //if (level < 0) level = 0; else if (level > 4) level = 4;
    if (lastTextureFiltering < 0) return FilterWrongAll;
    unsigned res = FilterMatch;
    if (!!(lastTextureFiltering&0x80) != asModel) res |= FilterWrongModel;
    if ((lastTextureFiltering&0x7f) != level) res |= FilterWrongLevel;
    if (((lastTextureFiltering>>8)&0xff) != aniso-1) res |= FilterWrongAniso;
    return res;
  }

  // should NOT be called with invalid arguments!
  inline void setFiltering (int level, int aniso, bool asModel) noexcept {
    lastTextureFiltering = (level&0x7f)|(((aniso-1)&0xff)<<8)|(asModel ? 0x80 : 0);
  }

  // max filtering mode is never used
  inline void invalidadeFiltering () noexcept { if (lastTextureFiltering >= 0) lastTextureFiltering |= 0x7f; }

  // used in `GenerateTexture()`, everything except repeat mode is invalid
  inline void initFilteringAsModelOnly (bool asModel) noexcept { lastTextureFiltering = (asModel ? 0x80 : 0)|0xff7f; }

  inline bool isWrapRepeat () const noexcept {
    return (Type == TEXTYPE_Wall || Type == TEXTYPE_Flat || Type == TEXTYPE_Overload || Type == TEXTYPE_WallPatch);
  }

  vuint32 lastUpdateFrame;

  // driver data
  struct VTransData {
    vuint32 Handle;
    VTextureTranslation *Trans;
    int ColorMap;
    // if non-zero, this is shade color
    vuint32 ShadeColor;
    vint32 lastUseTime; // this is "game time", from `GLevel->TicTime`

    inline VTransData () noexcept { memset((void *)this, 0, sizeof(*this)); }

    // call to wipe it -- not "properly clear", but wipe completely
    //inline void wipe () noexcept { memset((void *)this, 0, sizeof(*this)); }
  };

  vuint32 DriverHandle;
  TArray<VTransData> DriverTranslated;

  double gcLastUsedTime; // 0: don't GC; otherwise we are in GC list
  VTexture *gcPrev;
  VTexture *gcNext;

  // started from the highest `gcLastUsedTime`
  static VTexture *gcHead;
  static VTexture *gcTail;

public:
  // `time` must never be less than the previous used time
  void MarkGCUsed (const double time) noexcept;
  void MarkGCUnused () noexcept;

  // free unused texture pixels
  static void GCStep (const double currtime);

public:
  void ResetTranslations ();
  void ClearTranslations ();

  void ClearTranslationAt (int idx);
  // returns new index; also, marks it as "recently used"
  int MoveTranslationToTop (int idx);

  // -1 if not available
  static inline vint32 GetTranslationCTime () noexcept {
    return (GLevel ? GLevel->TicTime : GClLevel ? GClLevel->TicTime : -1);
  }

  inline bool IsGlowFullbright () const noexcept { return ((glowing&0xff000000u) == 0xff000000u); }

  // `true` means "do not compress"
  virtual bool IsDynamicTexture () const noexcept;
  // `true` means "immediately unload, if not dynamic"
  // note that huge hires textures considered as huge
  virtual bool IsHugeTexture () const noexcept;

  virtual bool IsMultipatch () const noexcept;

protected:
  vuint8 *Pixels; // most textures has some kind of pixel data, so declare it here
  vuint8 *Pixels8Bit;
  pala_t *Pixels8BitA;
  VTexture *HiResTexture;
  bool Pixels8BitValid;
  bool Pixels8BitAValid;
  int shadeColor;
  int shadeColorSaved; // `ConvertPixelsToShaded()` saves `shadeColor` here, so we can restore it after `ReleasePixels()`
  // we need those offsets to correctly apply patches in multipatch texture builder
  bool alreadyCropped;
  int croppedOfsX, croppedOfsY;
  int precropWidth, precropHeight; // so we will be able to restore it in `ReleasePixels()`

public:
  inline bool HasPixels () const noexcept { return (Pixels || Pixels8Bit || Pixels8BitA); }

  inline bool isCropped () const noexcept { return alreadyCropped; }
  inline int CropOffsetX () const noexcept { return croppedOfsX; }
  inline int CropOffsetY () const noexcept { return croppedOfsY; }

public:
  static void checkerFill8 (vuint8 *dest, int width, int height);
  static void checkerFillRGB (vuint8 *dest, int width, int height, int alpha=-1); // alpha <0 means 3-byte RGB texture
  static inline void checkerFillRGBA (vuint8 *dest, int width, int height) { checkerFillRGB(dest, width, height, 255); }

  // `dest` points at column, `x` is used only to build checker
  static void checkerFillColumn8 (vuint8 *dest, int x, int pitch, int height);

  static const char *TexTypeToStr (int ttype) {
    switch (ttype) {
      case TEXTYPE_Any: return "any";
      case TEXTYPE_WallPatch: return "patch";
      case TEXTYPE_Wall: return "wall";
      case TEXTYPE_Flat: return "flat";
      case TEXTYPE_Overload: return "overload";
      case TEXTYPE_Sprite: return "sprite";
      case TEXTYPE_SkyMap: return "sky";
      case TEXTYPE_Skin: return "skin";
      case TEXTYPE_Pic: return "pic";
      case TEXTYPE_Autopage: return "autopage";
      case TEXTYPE_Null: return "null";
      case TEXTYPE_FontChar: return "fontchar";
    }
    return "unknown";
  }

protected:
  // this should be called after `Pixels` were converted to RGBA
  void shadePixelsRGBA (int shadeColor);
  void stencilPixelsRGBA (int shadeColor);

  // uses `Format` to convert, so it must be valid
  // `Pixels` must be set
  // will delete old `Pixels` if necessary
  void ConvertPixelsToRGBA ();

  // uses `Format` to convert, so it must be valid
  // `Pixels` must be set
  // will delete old `Pixels` if necessary
  void ConvertPixelsToShaded ();

  void CalcRealDimensions ();

  // should be called ONLY on uncropped textures!
  // rectangle is inclusive, and must not be empty
  // used by `CropTexture()`
  void CropCanvasTo (int x0, int y0, int x1, int y1);

  // load brightmap texture pixels, crop/resize it if necessary
  // called from `GetPixels()`
  void PrepareBrightmap ();

public:
  static void FilterFringe (rgba_t *pic, int wdt, int hgt);
  static void PremultiplyImage (rgba_t *pic, int wdt, int hgt);

  // use `153` to calculate glow color
  rgb_t GetAverageColor (vuint32 maxout);

  // [0..1]
  float GetMaxIntensity ();

  void ResizeCanvas (int newwdt, int newhgt);

  void CropTexture (); // this also crops brightmap texture

  void ConvertXHairPixels ();

public:
  //k8: please note that due to my sloppy coding, real format checking should be preceded by `GetPixels()`
  inline int GetFormat () const noexcept { return (shadeColor == -1 ? mFormat : TEXFMT_RGBA); }
  PropertyRO<int, VTexture> Format {this, &VTexture::GetFormat};

  inline int GetRealX0 () { if (RealX0 == MIN_VINT32) CalcRealDimensions(); return RealX0; }
  inline int GetRealY0 () { if (RealY0 == MIN_VINT32) CalcRealDimensions(); return RealY0; }
  inline int GetRealHeight () { if (RealHeight == MIN_VINT32) CalcRealDimensions(); return RealHeight; }
  inline int GetRealWidth () { if (RealWidth == MIN_VINT32) CalcRealDimensions(); return RealWidth; }

public:
  VTexture ();
  virtual ~VTexture ();

  // this won't add texture to texture hash
  static VTexture *CreateTexture (int Type, int LumpNum, bool setName=true);

  // WARNING! this converts texture to RGBA!
  // DO NOT USE! DEBUG ONLY!
  void WriteToPNG (VStream *strm);

  inline __attribute__((const)) int GetWidth () const noexcept { return Width; }
  inline __attribute__((const)) int GetHeight () const noexcept { return Height; }

  inline __attribute__((const)) int GetScaledWidthI () const noexcept { return max2(1, (int)((float)Width/SScale)); }
  inline __attribute__((const)) int GetScaledHeightI () const noexcept { return max2(1, (int)((float)Height/TScale)); }

  inline __attribute__((const)) float GetScaledWidthF () const noexcept { return max2(0.001f, (float)Width/SScale); }
  inline __attribute__((const)) float GetScaledHeightF () const noexcept { return max2(0.001f, (float)Height/TScale); }

  // the two following methods are used in 2D renderer, and for decals
  inline __attribute__((const)) int GetScaledSOffsetI () const noexcept { return (int)(SOffset/SScale); }
  inline __attribute__((const)) int GetScaledTOffsetI () const noexcept { return (int)(TOffset/TScale); }

  inline __attribute__((const)) float GetScaledSOffsetF () const noexcept { return SOffset/SScale; }
  inline __attribute__((const)) float GetScaledTOffsetF () const noexcept { return TOffset/TScale; }

  inline __attribute__((const)) float TextureSScale () const noexcept { return SScale; }
  inline __attribute__((const)) float TextureTScale () const noexcept { return TScale; }
  // texture offsets are applied after performing dot product on texture axes, so they are in texels
  // with "world panning" they should be scaled accordingly to texture scale to be in "world coordinates"
  inline __attribute__((const)) float TextureOffsetSScale () const noexcept { return (bWorldPanning ? SScale : 1.0f); }
  inline __attribute__((const)) float TextureOffsetTScale () const noexcept { return (bWorldPanning ? TScale : 1.0f); }

  // get texture pixel; will call `GetPixels()`
  rgba_t getPixel (int x, int y);

  inline bool isTransparent () {
    if (transFlags == TransValueUnknown) (void)GetPixels(); // this will set the flag
    return !!(transFlags&FlagTransparent);
  }

  inline bool isTranslucent () {
    if (transFlags == TransValueUnknown) (void)GetPixels(); // this will set the flag
    return !!(transFlags&FlagTranslucent);
  }

  // has both translucent and solid pixels
  inline bool isSemiTranslucent () {
    if (transFlags == TransValueUnknown) (void)GetPixels(); // this will set the flag
    return ((transFlags&(FlagTranslucent|FlagHasSolidPixel)) == (FlagTranslucent|FlagHasSolidPixel));
  }

  inline bool isSeeThrough () {
    if (transFlags == TransValueUnknown) (void)GetPixels(); // this will set the flag
    return !!(transFlags&(FlagTransparent|FlagTranslucent));
  }

  /*
  inline void ResetTransparentFlag () noexcept { if (transFlags != TransValueUnknown) transFlags &= ~FlagTransparent; }
  inline void ResetTranslucentFlag () noexcept { if (transFlags != TransValueUnknown) transFlags &= ~FlagTranslucent; }
  inline void ResetHasSolidPixelFlag () noexcept { if (transFlags != TransValueUnknown) transFlags &= ~FlagHasSolidPixel; }

  // no need to check for `TransValueUnknown` here, as setting any flag will not modify it
  inline void SetTransparentFlag () noexcept { transFlags |= FlagTransparent; }
  inline void SetTranslucentFlag () noexcept { transFlags |= FlagTranslucent; }
  inline void SetHasSolidPixelFlag () noexcept { transFlags |= FlagHasSolidPixel; }
  */

  virtual void SetFrontSkyLayer ();
  virtual bool CheckModified ();
  virtual void Shade (int shade); // should be called before any `GetPixels()` call!

  // this can be called to release texture memory
  // the texture will be re-read on next `GetPixels()`
  // can be used to release hires texture memory
  virtual void ReleasePixels ();
  // it's not enough to check `Pixels`; use this instead
  // this is because camera textures, for example, never frees pixels
  virtual bool PixelsReleased () const noexcept;

  virtual vuint8 *GetPixels () = 0;
  vuint8 *GetPixels8 ();
  pala_t *GetPixels8A ();
  virtual rgba_t *GetPalette ();
  virtual VTexture *GetHighResolutionTexture ();

  // this returns temporary data, which should be freed with `FreeShadedPixels()`
  // WARNING: next call to `CreateShadedPixels()` may invalidate the pointer!
  //          that means that you MUST call `FreeShadedPixels()` before trying to
  //          get another shaded pixels
  // `palette` can be `nullptr` if no translation needed
  rgba_t *CreateShadedPixels (vuint32 shadeColor, const rgba_t *palette);
  void FreeShadedPixels (rgba_t *shadedPixels);

  VTransData *FindDriverTrans (VTextureTranslation *TransTab, int CMap, bool markUsed);
  VTransData *FindDriverShaded (vuint32 ShadeColor, int CMap, bool markUsed);

  static void AdjustGamma (rgba_t *, int); // for non-premultiplied
  static void SmoothEdges (vuint8 *, int, int); // for non-premultiplied
  static void ResampleTexture (int, int, const vuint8 *, int, int, vuint8 *, int); // for non-premultiplied
  static void MipMap (int, int, vuint8 *); // for non-premultiplied
  static void PremultiplyRGBAInPlace (void *databuff, int w, int h);
  static void PremultiplyRGBA (void *dest, const void *src, int w, int h);

protected:
  // normalize 8-bit palette, remap color 0
  // if `forceOpacity` is set, colors [1..255] will be forced to full opacity
  // if `transparent0` is set, color 0 will be transparent, otherwise it will be remapped
  void FixupPalette (rgba_t Palette[256], bool forceOpacity, bool transparent0);
};


// ////////////////////////////////////////////////////////////////////////// //
class VTextureManager {
public:
  struct WallPatchInfo {
    int index;
    VName name;
    VTexture *tx; // can be null
  };

private:
  enum { HASH_SIZE = 4096 };
  enum { FirstMapTextureIndex = 1000000 };

  TArray<VTexture *> Textures;
  TArray<VTexture *> MapTextures;
  int TextureHash[HASH_SIZE];

  int inMapTextures; // >0: loading map textures

  static inline VVA_CHECKRESULT uint16_t TextureBucket (uint32_t hash) noexcept { return (uint16_t)(foldHash32to16(hash)&(HASH_SIZE-1)); }

public:
  struct MTLock {
  friend class VTextureManager;
  private:
    VTextureManager *tm;
    MTLock (VTextureManager *atm) : tm(atm) { ++tm->inMapTextures; }
  public:
    MTLock (const MTLock &alock) : tm(alock.tm) { ++tm->inMapTextures; }
    ~MTLock () { if (tm) --tm->inMapTextures; tm = nullptr; }
    void operator = (const MTLock &alock) {
      if (&alock == this) return;
      if (alock.tm != tm) {
        if (tm) --tm->inMapTextures;
        tm = alock.tm;
      }
      if (tm) ++tm->inMapTextures;
    }
  };

private:
  void rehashTextures ();

  inline VTexture *getTxByIndex (int idx) const noexcept {
    if (idx < FirstMapTextureIndex) {
      return ((vuint32)idx < (vuint32)Textures.length() ? Textures[idx] : nullptr);
    } else {
      idx -= FirstMapTextureIndex;
      return ((vuint32)idx < (vuint32)MapTextures.length() ? MapTextures[idx] : nullptr);
    }
  }

public:
  vint32 DefaultTexture;
  float Time; // time value for warp textures
  //WARNING! as we have only one global texture manager object, it is safe to make this static
  static VName DummyTextureName; // this must be taken from `TEXTUREn` lump, because it can be anything

public:
  static inline bool IsDummyTextureName (VName n) noexcept { return (n == NAME_None || n == NAME_Minus_Sign || n == DummyTextureName); }

  struct Iter {
  private:
    VTextureManager *tman;
    int idx;
    VName name;
    bool allowShrink;

  private:
    inline bool restart () {
      // check for empty texture name
      if (name == NAME_None) { idx = -1; return false; }
      // check for "NoTexture" marker
      if (VTextureManager::IsDummyTextureName(name)) { idx = 0; return true; }
      // has texture manager object?
      if (!tman) { idx = -1; return false; }
      // hashmap search
      VName loname = name.GetLowerNoCreate();
      if (loname == NAME_None) { idx = -1; return false; }
      name = loname;
      int cidx = tman->TextureHash[TextureBucket(GetTypeHash(name))];
      while (cidx >= 0) {
        VTexture *ctex = tman->getTxByIndex(cidx);
        if (ctex->Name == name) { idx = cidx; return true; }
        cidx = ctex->HashNext;
      }
      idx = -1;
      return false;
    }

  public:
    Iter () : tman(nullptr), idx(-1), name(NAME_None), allowShrink(false) {}
    Iter (VTextureManager *atman, VName aname, bool aAllowShrink=true) : tman(atman), idx(-1), name(aname), allowShrink(aAllowShrink) { restart(); }

    inline bool empty () const { return (idx < 0); }
    inline bool isMapTexture () const { return (tman && idx >= tman->Textures.length()); }
    inline bool next () {
      if (idx < 0) return false;
      for (;;) {
        idx = tman->getTxByIndex(idx)->HashNext;
        if (idx < 0) break;
        if (tman->getTxByIndex(idx)->Name == name) return true;
      }
      // here we can restart iteration with shrinked name, if it is longer than 8 chars
      if (allowShrink && VStr::length(*name) > 8) {
        allowShrink = false;
        name = name.GetLower8NoCreate();
        if (name != NAME_None) return restart();
      }
      return false;
    }
    inline int index () const { return idx; }
    inline VTexture *tex () const { return tman->getTxByIndex(idx); }
  };

  inline Iter firstWithName (VName n, bool allowShrink=true) { return Iter(this, n, allowShrink); }
  Iter firstWithStr (VStr s);

public:
  VName SkyFlatName;
  VVA_CHECKRESULT bool IsSkyTextureName (VName n) noexcept;
  void SetSkyFlatName (VStr flatname) noexcept;

protected:
  int AddFullNameTexture (VTexture *Tex, bool asMapTexture);

public:
  VTextureManager ();

  void Init ();
  void Shutdown ();

  void DumpHashStats (EName logName=NAME_Log);

  // unload all map-local textures
  void ResetMapTextures ();

  // call this before possible loading of map-local texture
  inline MTLock LockMapLocalTextures () { return MTLock(this); }

  int AddTexture (VTexture *Tex);
  void ReplaceTexture (int Index, VTexture *Tex);
  int CheckNumForName (VName Name, int Type, bool bOverload=false);
  int FindPatchByName (VName Name); // used in multipatch texture builder
  int FindWallByName (VName Name, bool bOverload=true); // used to find wall texture (but can return non-wall)
  int FindFlatByName (VName Name, bool bOverload=true); // used to find flat texture (but can return non-flat)
  int NumForName (VName Name, int Type, bool bOverload=false, bool bAllowLoadAsMapTexture=false);
  int FindTextureByLumpNum (int);
  VName GetTextureName (int TexNum);
  float TextureWidth (int TexNum);
  float TextureHeight (int TexNum);
  void SetFrontSkyLayer (int tex);
  void GetTextureInfo (int TexNum, picinfo_t *info);
  int AddPatch (VName Name, int Type, bool Silent=false, bool asXHair=false);
  //int AddPatchShaded (VName Name, int Type, int shade, bool Silent=false); // shade==-1: don't shade
  //int AddPatchShadedById (int texid, int shade); // shade==-1: don't shade
  int AddPatchLump (int LumpNum, VName Name, int Type, bool Silent=false);
  int AddRawWithPal (VName Name, VName PalName);
  int AddFileTexture (VName Name, int Type);
  int AddFileTextureShaded (VName Name, int Type, int shade); // shade==-1: don't shade; used only for alias model skins
  int AddFileTextureChecked (VName Name, int Type, VName forceName=NAME_None); // returns -1 if no texture found
  // try to force-load texture
  int CheckNumForNameAndForce (VName Name, int Type, bool bOverload, bool silent);

  // this can find/load both textures without path (lump-named), and textures with full path
  // it also can return normalized texture name in `normname` (it can be `nullptr` too)
  // returns -1 if not found/cannot load
  int FindOrLoadFullyNamedTexture (VStr txname, VName *normname, int Type, bool bOverload, bool silent, bool allowLoad=true, bool forceMapTexture=false);
  int FindOrLoadFullyNamedTextureAsMapTexture (VStr txname, VName *normname, int Type, bool bOverload, bool silent=false) { return FindOrLoadFullyNamedTexture(txname, normname, Type, bOverload, silent, true, true); }
  inline int FindFullyNamedTexture (VStr txname, VName *normname, int Type, bool bOverload, bool silent=false) { return FindOrLoadFullyNamedTexture(txname, normname, Type, bOverload, silent, false); }

  inline bool IsMapLocalTexture (int TexNum) const noexcept { return (TexNum >= FirstMapTextureIndex); }

  inline bool IsEmptyTexture (int TexNum) const noexcept {
    if (TexNum <= 0) return true;
    VTexture *tx = getIgnoreAnim(TexNum);
    return (!tx || tx->Type == TEXTYPE_Null);
  }

  inline bool IsSeeThrough (int TexNum) const noexcept {
    if (TexNum <= 0) return true;
    VTexture *tx = getIgnoreAnim(TexNum);
    return (!tx || tx->Type == TEXTYPE_Null || tx->isSeeThrough());
  }

  inline bool IsSightBlocking (int TexNum) const noexcept { return !IsSeeThrough(TexNum); }

  enum TexCheckType {
    TCT_EMPTY,
    TCT_SEE_TRHOUGH,
    TCT_SOLID,
  };

  inline TexCheckType GetTextureType (int TexNum) const noexcept {
    if (TexNum <= 0) return TCT_EMPTY;
    VTexture *tx = getIgnoreAnim(TexNum);
    return (!tx || tx->Type == TEXTYPE_Null ? TCT_EMPTY : (tx->isSeeThrough() ? TCT_SEE_TRHOUGH : TCT_SOLID));
  }

  // get unanimated texture
  inline VTexture *operator [] (int TexNum) const noexcept {
    VTexture *res = getTxByIndex(TexNum);
    if (res) res->noDecals = res->staticNoDecals;
    return res;
  }

  inline VTexture *getIgnoreAnim (int TexNum) const noexcept { return getTxByIndex(TexNum); }
  inline VTexture *getMapTexIgnoreAnim (int TexNum) const noexcept { return ((vuint32)TexNum < (vuint32)MapTextures.length() ? MapTextures[TexNum] : nullptr); }

  //inline int TextureAnimation (int InTex) { return Textures[InTex]->TextureTranslation; }

  // get animated texture
  inline VTexture *operator () (int TexNum) noexcept {
    VTexture *origtex;
    if (TexNum < FirstMapTextureIndex) {
      if ((vuint32)TexNum >= (vuint32)Textures.length()) return nullptr;
      origtex = Textures[TexNum];
    } else {
      if ((vuint32)(TexNum-FirstMapTextureIndex) >= (vuint32)MapTextures.length()) return nullptr;
      origtex = MapTextures[TexNum-FirstMapTextureIndex];
    }
    if (!origtex) return nullptr;
    //FIXMEFIXMEFIXME: `origtex->TextureTranslation == -1`? whatisthat?
    if (origtex->TextureTranslation != TexNum /*&& (vuint32)origtex->TextureTranslation < (vuint32)Textures.length()*/) {
      //VTexture *res = Textures[origtex->TextureTranslation];
      VTexture *res = getTxByIndex(origtex->TextureTranslation);
      if (res) {
        if (res) res->noDecals = origtex->animNoDecals || res->staticNoDecals;
        return res;
      }
    }
    origtex->noDecals = (origtex->animated ? origtex->animNoDecals : false) || origtex->staticNoDecals;
    return origtex;
  }

  inline int GetNumTextures () const noexcept { return Textures.length(); }
  inline int GetNumMapTextures () const noexcept { return MapTextures.length(); }

  // to use in `ExportTexture` command
  void FillNameAutocompletion (VStr prefix, TArray<VStr> &list);
  VTexture *GetExistingTextureByName (VStr txname, int type=TEXTYPE_Any);

private:
  void LoadPNames (int Lump, TArray<WallPatchInfo> &patchtexlookup);
  void AddToHash (int Index);
  void AddTextures ();
  void AddMissingNumberedTextures ();
  void AddTexturesLump (TArray<WallPatchInfo> &patchtexlookup, int TexLump, int FirstTex, bool First);
  void AddGroup (int, EWadNamespace);

  void ParseTextureTextLump (int Lump, bool asHiRes);

  void AddTextureTextLumps ();

  // this tries to add `TEXTYPE_Pic` patch if `Type` not found
  // returns -1 on error; never returns 0 (0 is "not found")
  int FindHiResToReplace (VName name, int Type, bool bOverload=true);

  // this can also append a new texture if `OldIndex` is < 0
  // it can do `delete NewTex` too
  void ReplaceTextureWithHiRes (int OldIndex, VTexture *NewTex, int oldWidth=-1, int oldHeight=-1);
  void AddHiResTextureTextLumps ();

  void AddHiResTexturesFromNS (EWadNamespace NS, int ttype, bool &messaged);
  void AddHiResTextures (); // hires namespace

  void LoadSpriteOffsets ();

  void WipeWallPatches ();

  friend void R_InitTexture ();
  friend void R_DumpTextures ();

  friend void R_InitHiResTextures ();
};


// ////////////////////////////////////////////////////////////////////////// //
#include "r_tex_atlas.h"


// ////////////////////////////////////////////////////////////////////////// //
// r_tex
void R_InitTexture ();
void R_InitHiResTextures ();
void R_DumpTextures ();
void R_ShutdownTexture ();
VAnimDoorDef *R_FindAnimDoor (vint32);
#ifdef CLIENT
void R_ResetAnimatedSurfaces ();
void R_AnimateSurfaces ();
void R_CheckAnimatedTexture (int id, void (*CacheTextureCallback) (int id, void *udata), void *udata);

#endif
bool R_IsAnimatedTexture (int texid);

void R_UpdateSkyFlatNum (bool force=false);

// ////////////////////////////////////////////////////////////////////////// //
extern bool GTextureCropMessages;
extern VTextureManager GTextureManager;
extern int skyflatnum;
extern int screenBackTexNum;

// switches
extern TArray<TSwitch *> Switches;


extern int gtxRight;
extern int gtxLeft;
extern int gtxTop;
extern int gtxBottom;
extern int gtxFront;
extern int gtxBack;


#endif
