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
#ifndef VAVOOM_UIWIDGETS_FONT_HEADER
#define VAVOOM_UIWIDGETS_FONT_HEADER


struct VSplitLine {
  VStr Text;
  vint32 Width;
};


// base class for fonts
class VFont {
protected:
  struct FFontChar {
    int Char;
    int TexNum;
    VTexture *BaseTex;
    VTexture **Textures;
  };

  VName Name;
  VFont *Next;

  // font characters
  TArray<FFontChar> Chars;
  TMapNC<vint32, vint32> CharMap; // key: code; value: index in `Chars`
  // fast look-up for ASCII characters
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
  ~VFont ();

  inline VName GetFontName () const noexcept { return Name; }

  // can return `nullptr` (pWidth is space width in this case)
  // `pWidth` can be `nullptr`
  // color is color translation (CR_XXX); -1 means "untranslated"
  VTexture *GetChar (int Chr, int *pWidth, int Color=-1);
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
  static int ParseColorEscape (const char *&, int, int, VStr *escstr=nullptr);
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
