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
#ifndef VAVOOM_UIWIDGETS_FONT_HEADER
#define VAVOOM_UIWIDGETS_FONT_HEADER


struct VSplitLine {
  VStr Text;
  vint32 Width;
};


// base class for fonts
class VFont {
public:
  // x1 and y1 should be greater than x0 and y0
  struct CharRect {
    float x0, y0, x1, y1;
    float xofs, yofs;

    inline CharRect () noexcept : x0(0.0f), y0(0.0f), x1(0.0f), y1(0.0f), xofs(0.0f), yofs(0.0f) {}
    inline CharRect (const VTexAtlas8bit::FRect &fr, const float axofs=0.0f, const float ayofs=0.0f) noexcept : x0(fr.x0), y0(fr.y0), x1(fr.x1), y1(fr.y1), xofs(axofs), yofs(ayofs) {}

    inline void clear () noexcept { x0 = y0 = x1 = y1 = xofs = yofs = 0.0f; }
    inline bool isValid () const noexcept { return (x0 < x1 && y0 < y1); }

    inline float getWidth () const noexcept { return x1-x0; }

    inline void setFull () noexcept { x0 = y0 = xofs = yofs = 0.0f; x1 = y1 = 1.0f; }

    static inline CharRect FullRect () noexcept { CharRect res; res.setFull(); return res; }
  };

protected:
  struct FFontChar {
    int Char;
    int TexNum;
    CharRect Rect;
    VTexture *BaseTex;
    VTexture **Textures;
    int Width, Height;

    //inline int GetWidth (const int defval=0) const noexcept { return (BaseTex ? (int)(BaseTex->GetScaledWidthF()*Rect.getWidth()) : defval); }
  };

  VName Name;
  VFont *Next;

  // font characters
  TArray<FFontChar> Chars;
  TMapNC<vint32, vint32> CharMap; // key: code; value: index in `Chars`
  // fast lookup for ASCII characters
  int AsciiChars[128];
  // range of available characters
  int FirstChar;
  int LastChar;

  // width of the space character
  int SpaceWidth;
  // height of the font
  int FontHeight;
  // additional distance betweeen characters
  int Kerning;

  rgba_t *Translation;

protected:
  static VFont *Fonts;
  static TMap<VStrCI, VFont *> FontMap;

protected:
  // set font name, and register it as known font
  static void RegisterFont (VFont *font, VName aname);

protected:
  void BuildTranslations (const bool *ColorsUsed, rgba_t *Pal, bool ConsoleTrans, bool Rescale);
  void BuildCharMap ();
  int FindChar (int);

protected:
  static void ParseTextColors ();
  static void ParseFontDefs ();
  static void MarkUsedColors (VTexture *Tex, bool *Used);

public:
  VFont ();
  VFont (VName AName, VStr FormatStr, int First, int Count, int StartIndex, int ASpaceWidth);
  virtual ~VFont ();

  inline VName GetFontName () const noexcept { return Name; }

  virtual bool IsSingleTextureFont () const noexcept;

  // can return `nullptr` (pWidth is space width in this case)
  // `pWidth` can be `nullptr`
  // color is color translation (CR_XXX); -1 means "untranslated"
  VTexture *GetChar (int Chr, CharRect *rect, int *pWidth, int Color=-1);
  int GetCharWidth (int Chr);

  // doesn't properly process newlines (but ignores color escapes)
  // this properly processes Utf8 chars
  int StringWidth (VStr String);

  // properly processes newlines and color escapes
  int TextWidth (VStr String);
  int TextHeight (VStr String);

  int SplitText (VStr, TArray<VSplitLine>&, int, bool trimRight=true);
  VStr SplitTextWithNewlines (VStr Text, int MaxWidth, bool trimRight=true);

  inline int GetSpaceWidth () const noexcept { return SpaceWidth; }
  inline int GetHeight () const noexcept { return FontHeight; }
  inline int GetKerning () const noexcept { return Kerning; }

  static void StaticInit ();
  static void StaticShutdown ();
  static VFont *FindFont (VName AName);
  static VFont *FindFont (VStr AName);
  static VFont *GetFont (VStr AName, VStr LumpName);
  static VFont *GetFont (VStr AName);
  // returns color translation index, or negative value for RGB color (use `&0xffffff` to get RGB)
  static int ParseColorEscape (const char *&, int, int, VStr *escstr=nullptr);
  // returns color translation index, or negative value for RGB color (use `&0xffffff` to get RGB)
  static int FindTextColor (const char *Name, int defval=CR_UNTRANSLATED);

  // generates lump name from font name
  // names in the format of 'name:path' will be split on ':'
  // (and `aname` will be modified)
  // otherwiser, returns `aname`
  static VStr GetPathFromFontName (VStr aname);
  static VStr TrimPathFromFontName (VStr aname);

private:
  static VFont *FindAndLoadFontFromLumpIdx (VStr AName, int LumpIdx);
};


#endif
