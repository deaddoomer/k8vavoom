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
#include "../gamedefs.h"
#include "../textures/r_tex.h"
#include "ui.h"

#define VAVOOM_NAME_FONT_TEXTURES

#define VAVOOM_CON_FONT_PATH  "k8vavoom/fonts/consolefont.fnt"


struct VColTranslationDef {
  rgba_t From;
  rgba_t To;
  int LumFrom;
  int LumTo;
};

struct VTextColorDef {
  TArray<VColTranslationDef> Translations;
  TArray<VColTranslationDef> ConsoleTranslations;
  rgba_t FlatColor;
  int Index;
};

struct VColTransMap {
  VName Name;
  int Index;
};


// like regular font, but initialised using explicit list of patches
class VSpecialFont : public VFont {
public:
  VSpecialFont (VName AName, const TArray<int> &CharIndexes, const TArray<VName> &CharLumps, const bool *NoTranslate, int ASpaceWidth);
};

// font in bitmap format
class VFontBitmapBase : public VFont {
protected:
  TArray<VTexture *> atlaslist;
  TArray<VTexture *> tratlaslist;

protected:
  // for FONx
  VTexAtlas8bit *currAtlas;
  int nextToUpdate;
  int asize; // atlas size

protected:
  VTexAtlas8bit *allocAtlas (rgba_t *APalette, int awdt=1024, int ahgt=1024);
  VTexAtlas8bit *allocTranslated (rgba_t *APalette, VTexAtlas8bit *currAtlas);
  void CreateTranslatedAtlases (VTexAtlas8bit *currAtlas, int nextToUpdate);

  // for FONx
  void CalculateAtlasSize (int TotalDataSize) noexcept;
  void ReadFONxChar (VStream &Strm, VName AName, int LumpNum, int Chr, int CharWidth, int CharHeight);
  void DoneReadingFONx ();

public:
  VFontBitmapBase ();
  virtual ~VFontBitmapBase () override;
};

// font in FON1 format
class VFon1Font : public VFontBitmapBase {
public:
  VFon1Font (VName, int);
};

// font in FON2 format
class VFon2Font : public VFontBitmapBase {
public:
  VFon2Font (VName, int);
};

// font consisting of a single texture for character 'A'
class VSingleTextureFont : public VFont {
public:
  VSingleTextureFont (VName, int);
  bool IsSingleTextureFont () const noexcept override;
};

// texture class for regular font characters
class VFontChar : public VTexture {
private:
  VTexture *BaseTex;
  rgba_t *Palette;

public:
  VFontChar (VTexture *ATex, rgba_t *APalette);
  virtual ~VFontChar () override;
  virtual vuint8 *GetPixels () override;
  virtual rgba_t *GetPalette () override;
  virtual VTexture *GetHighResolutionTexture () override;
};


// ////////////////////////////////////////////////////////////////////////// //
#include "ui_pcf_parse.cpp"


// ////////////////////////////////////////////////////////////////////////// //
VFont *SmallFont = nullptr;
VFont *ConFont = nullptr;
VFont *VFont::Fonts = nullptr;
TMap<VStrCI, VFont *> VFont::FontMap;

static TArray<VTextColorDef> TextColors;
static TArray<VColTransMap> TextColorLookup;
#ifdef VAVOOM_NAME_FONT_TEXTURES
static int vfontcharTxCount = 0;
#endif

static_assert(NUM_TEXT_COLORS >= 26, "we should have at least 26 user text colors!");


//==========================================================================
//
//  GenFontTextureName
//
//==========================================================================
static inline VName GenFontTextureName () noexcept {
  #ifdef VAVOOM_NAME_FONT_TEXTURES
  return VName(va("\x7f_fontchar2_%d ", vfontcharTxCount++));
  #else
  return NAME_None;
  #endif
}


//==========================================================================
//
//  safeFormat
//
//  should have only one format for decimal
//
//==========================================================================
static bool safeFormat (VStr fmt) noexcept {
  bool seenFormat = false;
  int prcpos = fmt.indexOf('%');
  while (prcpos >= 0) {
    if (prcpos+1 >= fmt.length()) return false;
    if (fmt[prcpos+1] == '%') { prcpos = fmt.indexOf('%', prcpos+2); continue; }
    if (seenFormat) return false;
    seenFormat = true;
    ++prcpos;
    if (fmt[prcpos] == '-' || fmt[prcpos] == '+') ++prcpos;
    while (prcpos < fmt.length() && VStr::digitInBase(fmt[prcpos], 10) >= 0) ++prcpos;
    if (prcpos >= fmt.length()) return false;
    const char fc = fmt[prcpos++];
    if (fc != 'x' && fc != 'X' && fc != 'd' && fc != 'i') return false;
    if (prcpos+1 >= fmt.length()) break;
    prcpos = fmt.indexOf('%', prcpos);
  }
  return true;
}


//==========================================================================
//
//  UnpackFONx
//
//==========================================================================
static vuint8 *UnpackFONx (VStream &Strm, int Count, const int MaxCol) {
  if (Count <= 0) return nullptr;
  vuint8 *Pixels = new vuint8[Count];
  vuint8 *pDst = Pixels;
  vuint8 maxcol = clampToByte(MaxCol);
  do {
    vint32 Code = Streamer<vint8>(Strm);
    if (Code >= 0) {
      Count -= Code+1;
      while (Code-- >= 0) {
        *pDst = Streamer<vuint8>(Strm);
        *pDst = min2(*pDst, maxcol);
        ++pDst;
      }
    } else if (Code != -128) {
      Code = 1-Code;
      Count -= Code;
      vuint8 Val = Streamer<vuint8>(Strm);
      Val = min2(Val, maxcol);
      while (Code-- > 0) *pDst++ = Val;
    }
  } while (Count > 0);

  if (Strm.IsError()) { delete[] Pixels; return nullptr; }
  return Pixels;
}



//**************************************************************************
//
// VFontBitmapBase
//
//**************************************************************************

//==========================================================================
//
//  VFontBitmapBase::VFontBitmapBase
//
//==========================================================================
VFontBitmapBase::VFontBitmapBase () {
  currAtlas = nullptr;
  nextToUpdate = 0;
  asize = 0;
}


//==========================================================================
//
//  VFontBitmapBase::~VFontBitmapBase
//
//==========================================================================
VFontBitmapBase::~VFontBitmapBase () {
  //for (auto &&a : atlaslist) delete a;
  /*
  for (auto &&cc : Chars) cc.Textures = nullptr; //FIXME: memleak!
  Chars.Clear();
  */
}


//==========================================================================
//
//  VFontBitmapBase::allocAtlas
//
//==========================================================================
VTexAtlas8bit *VFontBitmapBase::allocAtlas (rgba_t *APalette, int awdt, int ahgt) {
  VTexAtlas8bit *currAtlas = new VTexAtlas8bit(GenFontTextureName(), APalette, awdt, ahgt);
  atlaslist.append(currAtlas);
  // currently all render drivers expect all textures to be registered in texture manager
  GTextureManager.AddTexture(currAtlas);
  return currAtlas;
}


//==========================================================================
//
//  VFontBitmapBase::allocTranslated
//
//==========================================================================
VTexAtlas8bit *VFontBitmapBase::allocTranslated (rgba_t *APalette, VTexAtlas8bit *currAtlas) {
  VTexAtlas8bit *res = currAtlas->Clone(GenFontTextureName(), APalette);
  tratlaslist.append(res);
  // currently all render drivers expect all textures to be registered in texture manager
  GTextureManager.AddTexture(res);
  return res;
}


//==========================================================================
//
//  VFontBitmapBase::CreateTranslatedAtlases
//
//  create translated atlases, from `nextToUpdate` FChar
//
//==========================================================================
void VFontBitmapBase::CreateTranslatedAtlases (VTexAtlas8bit *currAtlas, int nextToUpdate) {
  if (nextToUpdate >= Chars.length()) return;
  (void)currAtlas->GetMaxIntensity(); // cache it
  for (int f = nextToUpdate; f < Chars.length(); ++f) {
    Chars[f].Textures = new VTexture*[TextColors.length()];
  }
  for (int j = 0; j < TextColors.length(); ++j) {
    //VTexture *tt = (j == CR_UNTRANSLATED ? currAtlas : allocTranslated(Translation+j*256, currAtlas));
    VTexture *tt = allocTranslated(Translation+j*256, currAtlas);
    for (int f = nextToUpdate; f < Chars.length(); ++f) Chars[f].Textures[j] = tt;
  }
}


//==========================================================================
//
//  VFontBitmapBase::CalculateAtlasSize
//
//==========================================================================
void VFontBitmapBase::CalculateAtlasSize (int TotalDataSize) noexcept {
  asize = 2;
  if (TotalDataSize <= 0) return;
  while (asize*asize < TotalDataSize) {
    if (asize == 1024) break;
    asize *= 2;
  }
}


//==========================================================================
//
//  VFontBitmapBase::DoneReadingFONx
//
//==========================================================================
void VFontBitmapBase::DoneReadingFONx () {
  if (!currAtlas) return;
  CreateTranslatedAtlases(currAtlas, nextToUpdate);
  currAtlas = nullptr;
}


//==========================================================================
//
//  VFontBitmapBase::ReadFONxChar
//
//==========================================================================
void VFontBitmapBase::ReadFONxChar (VStream &Strm, VName AName, int LumpNum, int Chr, int CharWidth, int CharHeight) {
  if (CharWidth < 1 || CharHeight < 1) return;

  if (!currAtlas) {
    vassert(asize);
    vassert(nextToUpdate == 0);
    vassert(Chars.length() == 0);
    currAtlas = allocAtlas(Translation, asize, asize);
  }

  VTexAtlas8bit::Rect rc;

  // allocate char in atlas
  for (;;) {
    rc = currAtlas->allocSubImage(CharWidth+2, CharHeight+2);
    if (rc.isValid()) break;
    CreateTranslatedAtlases(currAtlas, nextToUpdate);
    nextToUpdate = Chars.length();
    currAtlas = allocAtlas(Translation, asize, asize);
  }

  //GCon->Logf(NAME_Debug, "char #%d (%dx%d): rect=(%d,%d)-(%dx%d)", Chars.length(), SpaceWidth, FontHeight, rc.x, rc.y, rc.w, rc.h);

  // load char data
  vuint8 *cdata = UnpackFONx(Strm, CharWidth*CharHeight, 255);
  if (!cdata) Sys_Error("error loading font '%s' from '%s'", *AName, *W_FullLumpName(LumpNum));
  const vuint8 *csrc = cdata;
  for (int cy = 0; cy < CharHeight; ++cy) {
    for (int cx = 0; cx < CharWidth; ++cx) {
      currAtlas->setPixel(rc, cx+1, cy+1, *csrc++);
    }
  }
  delete[] cdata;

  FFontChar &FChar = Chars.Alloc();
  if (Chr >= 0 && Chr < 128) AsciiChars[Chr] = Chars.length()-1;
  FChar.Char = Chr;
  FChar.TexNum = -1;
  FChar.BaseTex = currAtlas;
  rc.shrinkBy(1);
  FChar.Rect = CharRect(currAtlas->convertRect(rc));
  //GCon->Logf(NAME_Debug, "  frc=(%g,%g)-(%g,%g)", FChar.Rect.x0, FChar.Rect.y0, FChar.Rect.x1, FChar.Rect.y1);
  FChar.Width = CharWidth;
  FChar.Height = CharHeight;
}



//**************************************************************************
//
// VFont Statics
//
//**************************************************************************

//==========================================================================
//
//  VFont::RegisterFont
//
//  set font name, and register it as known font
//
//==========================================================================
void VFont::RegisterFont (VFont *font, VName aname) {
  if (!font) return;

  font->Name = aname;
  if (aname == NAME_None) { font->Next = nullptr; return; }

  // register in list
  font->Name = aname;
  font->Next = Fonts;
  Fonts = font;

  // add to font map
  if (FontMap.has(VStrCI(aname))) {
    GCon->Logf(NAME_Init, "replacing font '%s'", *aname);
    // remove old font from the list, just for fun
    //FIXME: memory leak!
    VFont *prev = nullptr;
    VFont *curr = Fonts;
    while (curr) {
      if (curr != font && VStr::strEquCI(*curr->Name, *aname)) break;
      prev = curr;
      curr = curr->Next;
    }
    if (!curr) {
      GCon->Log(NAME_Error, "internal error: old font not found (this is harmless, but still a bug!)");
    } else {
      if (prev) prev->Next = curr->Next; else Fonts = curr->Next;
      curr->Next = nullptr;
      delete curr;
    }
  } else {
    GCon->Logf(NAME_Init, "registering font '%s'", *aname);
  }
  FontMap.put(VStrCI(aname), font);
}


//==========================================================================
//
//  VFont::StaticInit
//
//==========================================================================
void VFont::StaticInit () {
  ParseTextColors();

  // load custom fonts (they can override standard fonts)
  ParseFontDefs();

  // initialise standard fonts
  GCon->Log(NAME_Init, "creading default fonts");

  // small font
  //GCon->Logf(NAME_Debug, "'%s' : '%s' : %d : '%s' : '%s'", *VName(NAME_smallfont), *VStrCI(NAME_smallfont), (int)FontMap.has(VStrCI(NAME_smallfont)), *VStrCI(69), *VStrCI(666u));
  if (!FontMap.has(VStrCI(NAME_smallfont))) {
    for (VFont *f = Fonts; f; f = f->Next) GCon->Logf(NAME_Debug, "%p: '%s'", f, *f->Name);
    //GCon->Logf(NAME_Debug, "'%s' : '%s' : %d", *VName(NAME_smallfont), *VStrCI(NAME_smallfont), (int)FontMap.has(VStrCI(NAME_smallfont)));
    if (W_CheckNumForName(NAME_fonta_s) >= 0) {
      GCon->Log(NAME_Init, "  SmallFont: from 'fonta' lumps");
      SmallFont = new VFont(NAME_smallfont, "fonta%02d", 33, 95, 1, -666);
    } else {
      GCon->Log(NAME_Init, "  SmallFont: from 'stcfn' lumps");
      SmallFont = new VFont(NAME_smallfont, "stcfn%03d", 33, 95, 33, -666);
    }

    if (SmallFont->GetFontName() == NAME_None) {
      delete SmallFont;
      GCon->Log(NAME_Init, "  SmallFont: cannot create it, using ConsoleFont instead");
      SmallFont = GetFont(VStr(VName(NAME_smallfont)), VStr(VAVOOM_CON_FONT_PATH));
      if (!SmallFont) Sys_Error("cannot create console font");
    }
  }

  // strife's second small font
  if (!FontMap.has(VStrCI(NAME_smallfont2))) {
    if (W_CheckNumForName(NAME_stbfn033) >= 0) {
      GCon->Log(NAME_Init, "  Strife secondary small font");
      new VFont(NAME_smallfont2, "stbfn%03d", 33, 95, 33, -666);
    }
  }

  // big font
  if (!FontMap.has(VStrCI(NAME_bigfont))) {
    bool haveBigFont = false;
    if (W_CheckNumForName(NAME_bigfont) >= 0) {
      GCon->Log(NAME_Init, "  BigFont: from FON lump");
      if (!GetFont(VStr(VName(NAME_bigfont)), VStr(VName(NAME_bigfont)))) {
        GCon->Log(NAME_Init, "  BigFont: cannot create");
      } else {
        haveBigFont = true;
      }
    }
    if (!haveBigFont) {
      GCon->Log(NAME_Init, "  BigFont: from 'fontb' lumps");
      auto fnt = new VFont(NAME_bigfont, "fontb%02d", 33, 95, 1, -666);
      if (fnt->GetFontName() == NAME_None) {
        GCon->Log(NAME_Init, "  BigFont: cannot create");
      } else {
        haveBigFont = true;
      }
    }
    if (!haveBigFont && W_CheckNumForName("dbigfont") >= 0) {
      GCon->Log(NAME_Init, "  BigFont: from FON lump");
      if (!GetFont(VStr(VName(NAME_bigfont)), VStr("dbigfont"))) {
        GCon->Log(NAME_Init, "  BigFont: cannot create");
      } else {
        haveBigFont = true;
      }
    }
    if (!haveBigFont) {
      GCon->Log(NAME_Init, "  BigFont: from console font");
      if (!GetFont(VStr(VName(NAME_bigfont)), VStr(VAVOOM_CON_FONT_PATH))) Sys_Error("cannot create console font");
    }
  }

  // console font
  if (!FontMap.has(VStrCI(NAME_consolefont))) {
    GCon->Log(NAME_Init, "  ConsoleFont: from FON lump");
    ConFont = GetFont(VStr(VName(NAME_consolefont)), /*NAME_confont*/VStr(VAVOOM_CON_FONT_PATH));
    if (!ConFont) Sys_Error("cannot create console font");
  }
}


//==========================================================================
//
//  VFont::StaticShutdown
//
//==========================================================================
void VFont::StaticShutdown () {
  VFont *F = Fonts;
  while (F) {
    VFont *Next = F->Next;
    delete F;
    F = Next;
  }
  Fonts = nullptr;
  FontMap.clear();
  TextColors.Clear();
  TextColorLookup.Clear();
}


//==========================================================================
//
//  VFont::ParseTextColors
//
//==========================================================================
void VFont::ParseTextColors () {
  TArray<VTextColorDef> TempDefs;
  TArray<VColTransMap> TempColors;
  for (auto &&it : WadNSNameIterator(NAME_textcolo, WADNS_Global)) {
    const int Lump = it.lump;
    VScriptParser sc(*W_LumpName(Lump), W_CreateLumpReaderNum(Lump));
    while (!sc.AtEnd()) {
      VTextColorDef &Col = TempDefs.Alloc();
      Col.FlatColor.r = 0;
      Col.FlatColor.g = 0;
      Col.FlatColor.b = 0;
      Col.FlatColor.a = 255;
      Col.Index = -1;

      TArray<VName> Names;
      VColTranslationDef TDef;
      TDef.LumFrom = -1;
      TDef.LumTo = -1;

      // name for this color
      sc.ExpectString();
      Names.Append(*sc.String.ToLower());
      // additional names
      while (!sc.Check("{")) {
        if (Names[0] == "untranslated") sc.Error("Color \"untranslated\" cannot have any other names");
        sc.ExpectString();
        Names.Append(*sc.String.ToLower());
      }

      int TranslationMode = 0;
      while (!sc.Check("}")) {
        if (sc.Check("Console:")) {
          if (TranslationMode == 1) sc.Error("Only one console text color definition allowed");
          TranslationMode = 1;
          TDef.LumFrom = -1;
          TDef.LumTo = -1;
        } else if (sc.Check("Flat:")) {
          sc.ExpectString();
          vuint32 C = M_ParseColor(*sc.String);
          Col.FlatColor.r = (C >> 16) & 255;
          Col.FlatColor.g = (C >> 8) & 255;
          Col.FlatColor.b = C & 255;
          Col.FlatColor.a = 255;
        } else {
          // from color
          sc.ExpectString();
          vuint32 C = M_ParseColor(*sc.String);
          TDef.From.r = (C >> 16) & 255;
          TDef.From.g = (C >> 8) & 255;
          TDef.From.b = C & 255;
          TDef.From.a = 255;

          // to color
          sc.ExpectString();
          C = M_ParseColor(*sc.String);
          TDef.To.r = (C >> 16) & 255;
          TDef.To.g = (C >> 8) & 255;
          TDef.To.b = C & 255;
          TDef.To.a = 255;

          if (sc.CheckNumber()) {
            // optional luminosity ranges
            if (TDef.LumFrom == -1 && sc.Number != 0) sc.Error("First color range must start at position 0");
            if (sc.Number < 0 || sc.Number > 256) sc.Error("Color index must be in range from 0 to 256");
            if (sc.Number <= TDef.LumTo) sc.Error("Color range must start after end of previous range");
            TDef.LumFrom = sc.Number;

            sc.ExpectNumber();
            if (sc.Number < 0 || sc.Number > 256) sc.Error("Color index must be in range from 0 to 256");
            if (sc.Number <= TDef.LumFrom) sc.Error("Ending color index must be greater than start index");
            TDef.LumTo = sc.Number;
          } else {
            // set default luminosity range
            TDef.LumFrom = 0;
            TDef.LumTo = 256;
          }

               if (TranslationMode == 0) Col.Translations.Append(TDef);
          else if (TranslationMode == 1) Col.ConsoleTranslations.Append(TDef);
        }
      }

      if (Names[0] == "untranslated") {
        if (Col.Translations.length() != 0 || Col.ConsoleTranslations.length() != 0) {
          sc.Error("The \"untranslated\" color must be left undefined");
        }
      } else {
        if (Col.Translations.length() == 0) sc.Error("There must be at least one normal range for a color");
        if (Col.ConsoleTranslations.length() == 0) {
          // if console color translation is not defined, make it white
          TDef.From.r = 0;
          TDef.From.g = 0;
          TDef.From.b = 0;
          TDef.From.a = 255;
          TDef.To.r = 255;
          TDef.To.g = 255;
          TDef.To.b = 255;
          TDef.To.a = 255;
          TDef.LumFrom = 0;
          TDef.LumTo = 256;
          Col.ConsoleTranslations.Append(TDef);
        }
      }

      // add all names to the list of colors
      for (int i = 0; i < Names.length(); ++i) {
        // check for redefined colors
        int CIdx;
        for (CIdx = 0; CIdx < TempColors.length(); ++CIdx) {
          if (TempColors[CIdx].Name == Names[i]) {
            TempColors[CIdx].Index = TempDefs.length()-1;
            break;
          }
        }
        if (CIdx == TempColors.length()) {
          VColTransMap &CMap = TempColors.Alloc();
          CMap.Name = Names[i];
          CMap.Index = TempDefs.length()-1;
        }
      }
    }
  }

  // put color definitions in it's final location
  for (int i = 0; i < TempColors.length(); ++i) {
    VColTransMap &TmpCol = TempColors[i];
    VTextColorDef &TmpDef = TempDefs[TmpCol.Index];
    if (TmpDef.Index == -1) {
      TmpDef.Index = TextColors.length();
      TextColors.Append(TmpDef);
    }
    VColTransMap &Col = TextColorLookup.Alloc();
    Col.Name = TmpCol.Name;
    Col.Index = TmpDef.Index;
  }

  // make sure all built-in colors are defined
  vassert(TextColors.length() >= NUM_TEXT_COLORS);
}


//==========================================================================
//
//  VFont::ParseFontDefs
//
//==========================================================================
void VFont::ParseFontDefs () {
#define CHECK_TYPE(Id) if (FontType == Id) \
  sc.Error(va("Invalid combination of properties in font '%s'", *FontName))

  for (auto &&it : WadNSNameIterator(NAME_fontdefs, WADNS_Global)) {
    const int Lump = it.lump;
    VScriptParser sc(*W_LumpName(Lump), W_CreateLumpReaderNum(Lump));
    GCon->Logf(NAME_Init, "parsing font definition '%s'", *W_FullLumpName(Lump));
    while (!sc.AtEnd()) {
      // name of the font
      sc.ExpectString();
      VName FontName(*sc.String, VName::AddLower);
      sc.Expect("{");

      int FontType = 0;
      VStr Template;
      int First = 33;
      int Count = 223;
      int Start = 33;
      int SpaceWidth = -666;
      TArray<int> CharIndexes;
      TArray<VName> CharLumps;
      bool NoTranslate[256];
      memset(NoTranslate, 0, sizeof(NoTranslate));

      while (!sc.Check("}")) {
        if (sc.Check("template")) {
          CHECK_TYPE(2);
          sc.ExpectString();
          Template = sc.String;
          FontType = 1;
          //GCon->Logf(NAME_Debug, "*** TPL: <%s>", *Template);
        } else if (sc.Check("first")) {
          CHECK_TYPE(2);
          sc.ExpectNumber();
          First = sc.Number;
          FontType = 1;
        } else if (sc.Check("count")) {
          CHECK_TYPE(2);
          sc.ExpectNumber();
          Count = sc.Number;
          FontType = 1;
        } else if (sc.Check("base")) {
          CHECK_TYPE(2);
          sc.ExpectNumber();
          Start = sc.Number;
          FontType = 1;
        } else if (sc.Check("notranslate") || sc.Check("notranslation")) {
          CHECK_TYPE(1);
          while (sc.CheckNumber() && !sc.Crossed) {
            if (sc.Number >= 0 && sc.Number < 256) NoTranslate[sc.Number] = true;
          }
          FontType = 2;
        } else if (sc.Check("cursor")) {
          sc.Message("fontdef 'CURSOR' property is not supported");
          sc.ExpectString();
        } else if (sc.Check("spacewidth")) {
          sc.ExpectNumber();
          SpaceWidth = sc.Number;
        } else {
          CHECK_TYPE(1);
          sc.ExpectLoneChar();
          const char *CPtr = *sc.String;
          int CharIdx = VStr::Utf8GetChar(CPtr);
          sc.ExpectString();
          VName LumpName(*sc.String, VName::AddLower8);
          if (W_CheckNumForName(LumpName, WADNS_Graphics) >= 0) {
            CharIndexes.Append(CharIdx);
            CharLumps.Append(LumpName);
          } else {
            GCon->Logf(NAME_Error, "font '%s': char #%d not found (%s)", *FontName, CharIdx, *LumpName);
          }
          FontType = 2;
        }
      }

      if (FontName != NAME_None) {
        if (FontType == 1) {
          GCon->Logf(NAME_Init, "creating font '%s'...", *FontName);
          auto fnt = new VFont(FontName, Template, First, Count, Start, SpaceWidth);
          if (fnt->GetFontName() == NAME_None) {
            GCon->Logf(NAME_Error, "FONT: cannot create font '%s'!", *FontName);
            delete fnt;
          }
        } else if (FontType == 2) {
          if (CharIndexes.length()) {
            GCon->Logf(NAME_Init, "creating special font '%s'", *FontName);
            new VSpecialFont(FontName, CharIndexes, CharLumps, NoTranslate, SpaceWidth);
          }
        } else {
          sc.Error("Font has no attributes");
        }
      }
    }
  }
#undef CHECK_TYPE
}


//==========================================================================
//
//  VFont::GetPathFromFontName
//
//==========================================================================
VStr VFont::GetPathFromFontName (VStr aname) {
  int cp = aname.indexOf(':');
  if (cp < 0) return aname;
  VStr res(*aname+cp+1);
  res = res.xstrip();
  if (res.isEmpty()) return aname;
  return res;
}


//==========================================================================
//
//  VFont::TrimPathFromFontName
//
//==========================================================================
VStr VFont::TrimPathFromFontName (VStr aname) {
  int cp = aname.indexOf(':');
  if (cp < 0) return aname;
  VStr res(*aname, cp);
  res = res.xstrip();
  if (res.isEmpty()) return aname;
  return res;
}


//==========================================================================
//
//  VFont::FindFont
//
//==========================================================================
VFont *VFont::FindFont (VStr AName) {
  #if 0
  for (VFont *F = Fonts; F; F = F->Next) {
    if (F->Name == AName || VStr::strEquCI(F->Name, *AName)) return F;
  }
  #else
  if (!AName.isEmpty()) {
    VStr n = TrimPathFromFontName(AName);
    auto fpp = FontMap.get(n);
    if (fpp) return *fpp;
  }
  #endif
  return nullptr;
}


//==========================================================================
//
//  VFont::FindFont
//
//==========================================================================
VFont *VFont::FindFont (VName AName) {
  if (AName != NAME_None) return FindFont(VStr(AName)); // this doesn't allocate
  return nullptr;
}


//==========================================================================
//
//  VFont::FindAndLoadFontFromLumpIdx
//
//==========================================================================
VFont *VFont::FindAndLoadFontFromLumpIdx (VStr AName, int LumpIdx) {
  if (LumpIdx < 0 || AName.isEmpty()) return nullptr;
  // read header
  char Hdr[4];
  { // so the stream will be destroyed automatically
    VStream *lumpstream = W_CreateLumpReaderNum(LumpIdx);
    VCheckedStream Strm(lumpstream);
    if (Strm.TotalSize() < 8) return nullptr;
    Strm.Serialise(Hdr, 4);
  }
  if (Hdr[0] == 'F' && Hdr[1] == 'O' && Hdr[2] == 'N') {
    //GCon->Logf(NAME_Debug, "FONT%c: <%s>", Hdr[3], *AName);
    if (Hdr[3] == '1') return new VFon1Font(VName(*AName, VName::AddLower), LumpIdx);
    if (Hdr[3] == '2') return new VFon2Font(VName(*AName, VName::AddLower), LumpIdx);
  }
  return nullptr;
}


//==========================================================================
//
//  VFont::GetFont
//
//==========================================================================
VFont *VFont::GetFont (VStr AName, VStr LumpName) {
  //GCon->Logf(NAME_Debug, "VFont::GetFont: name=<%s>; lump=<%s>", *AName, *LumpName);

  VFont *F = FindFont(AName);
  if (F) return F;

  if (AName.isEmpty()) return nullptr;

  if (LumpName.isEmpty()) return nullptr;

  VName ln(*LumpName, VName::FindLower);

  if (ln == NAME_None) {
    // try file path (there is no reason to try a lump, we don't have one)
    int flump = W_CheckNumForFileName(LumpName);
    return FindAndLoadFontFromLumpIdx(AName, flump);
  }

  // check for wad lump
  const int Lump = W_CheckNumForName(ln);
  F = FindAndLoadFontFromLumpIdx(AName, Lump);
  if (F) return F;

  // try a texture
  int TexNum = GTextureManager.CheckNumForName(ln, TEXTYPE_Any);
  if (TexNum <= 0) TexNum = GTextureManager.AddPatch(ln, TEXTYPE_Pic);
  if (TexNum > 0) return new VSingleTextureFont(VName(*AName, VName::AddLower), TexNum);

  return nullptr;
}


//==========================================================================
//
//  VFont::GetFont
//
//==========================================================================
VFont *VFont::GetFont (VStr AName) {
  VStr LumpName = GetPathFromFontName(AName);
  AName = TrimPathFromFontName(AName);
  return GetFont(AName, LumpName);
}



//**************************************************************************
//
// VFont
//
//**************************************************************************

//==========================================================================
//
//  VFont::VFont
//
//==========================================================================
VFont::VFont () : Name(NAME_None) {
}


//==========================================================================
//
//  VFont::VFont
//
//==========================================================================
VFont::VFont (VName AName, VStr FormatStr, int First, int Count, int StartIndex, int ASpaceWidth) {
  for (unsigned i = 0; i < 128; ++i) AsciiChars[i] = -1;
  FirstChar = -1;
  LastChar = -1;
  FontHeight = 0;
  Kerning = 0;
  Translation = nullptr;
  bool ColorsUsed[256];
  memset(ColorsUsed, 0, sizeof(ColorsUsed));

  if (!safeFormat(FormatStr)) Sys_Error("font format string \"%s\" is not safe!", *FormatStr.quote());
  //GCon->Logf(NAME_Debug, "**** format \"%s\" is SAFE!", *FormatStr.quote());

  const bool allowCFonLower = FormatStr.startsWithCI("stcfn");

  bool wasAnyChar = false;

  //GCon->Logf(NAME_Debug, "creating font of type 1; Template=<%s>; First=%d; Count=%d; Start=%d", *FormatStr, First, Count, StartIndex);
  for (int i = 0; i < Count; ++i) {
    int Char = i+First;
    char Buffer[10];
    //FIXME: replace this with safe variant
    int prc = snprintf(Buffer, sizeof(Buffer), *FormatStr, i+StartIndex);
    if (!Buffer[0] || prc < 1 || prc > 9) continue;
    VName LumpName(Buffer, VName::AddLower8);

    int Lump = W_CheckNumForName(LumpName, WADNS_Graphics);
    // our UI cannot support hires fonts without lores templates, and i will not implement that
    /*
    if (Lump < 0) Lump = W_CheckNumForName(LumpName, WADNS_NewTextures);
    if (Lump < 0) Lump = W_CheckNumForName(LumpName, WADNS_HiResTextures);
    if (Lump < 0) Lump = W_CheckNumForName(LumpName, WADNS_HiResTexturesDDay);
    //if (Lump < 0) = W_CheckNumForName(LumpName, WADNS_HiResFlatsDDay);
    */
    VTexture *knownTexture = nullptr;
    {
      int tid = GTextureManager.CheckNumForName(LumpName, TEXTYPE_Pic, true);
      if (tid > 0) {
        knownTexture = GTextureManager[tid];
        //GCon->Logf(NAME_Debug, "*** CHAR %d: lump is <%s>; texture id is %d (%s)", Char, *LumpName, tid, *W_FullLumpName(knownTexture->SourceLump));
      }
    }

    // try lowercased ("cfontXXX")
    bool iscfont = false;
    if (!knownTexture && Lump < 0 && allowCFonLower && i >= 'a' && i <= 'z') {
      char tb[64];
      snprintf(tb, sizeof(tb), "%s", *LumpName);
      if (strlen(tb) > 5) {
        tb[0] = 'c';
        tb[1] = 'f';
        tb[2] = 'o';
        tb[3] = 'n';
        tb[4] = 't';
        VName ln2(tb, VName::FindLower8);
        if (ln2 != NAME_None) {
          Lump = W_CheckNumForName(ln2, WADNS_Graphics);
          if (Lump >= 0) LumpName = ln2;
          iscfont = true;
        }
      }
    }

    //GCon->Logf(NAME_Debug, "  char %d (%c): lump=%d (%s)", Char, (Char >= 32 && Char < 127 ? Char : '?'), Lump, (Lump >= 0 ? *W_FullLumpName(Lump) : "<oops>"));

    // in Doom, stcfn121 is actually a '|' and not 'y' and many wad
    // authors provide it as such, so put it in correct location
    if (!iscfont && LumpName == "stcfn121" &&
        (W_CheckNumForName("stcfn120", WADNS_Graphics) == -1 ||
         W_CheckNumForName("stcfn122", WADNS_Graphics) == -1))
    {
      Char = '|';
    }

    if (Lump >= 0 || knownTexture) {
      VTexture *Tex = (knownTexture ? knownTexture : GTextureManager[GTextureManager.AddPatchLump(Lump, LumpName, TEXTYPE_Pic)]);
      if (Tex) {
        FFontChar &FChar = Chars.Alloc();
        FChar.Char = Char;
        FChar.TexNum = -1;
        FChar.BaseTex = Tex;
        FChar.Rect.setFull();
        if (Char < 128) AsciiChars[Char] = Chars.length()-1;

        // calculate height of font character and adjust font height as needed
        int Height = Tex->GetScaledHeightI();
        FChar.Width = Tex->GetScaledWidthI();
        FChar.Height = Height;

        int TOffs = Tex->GetScaledTOffsetI();
        Height += abs(TOffs);
        if (FontHeight < Height) FontHeight = Height;


        // update first and last characters
        if (FirstChar == -1 || FirstChar > Char) FirstChar = Char;
        if (LastChar == -1 || LastChar < Char) LastChar = Char;

        // mark colors that are used by this texture
        MarkUsedColors(Tex, ColorsUsed);

        (void)Tex->GetMaxIntensity(); // cache it

        //if (developer) GCon->Logf(NAME_Dev, "loaded char lump '%s' as '%s' (%d) for char %d of font '%s'", *W_FullLumpName(Lump), *LumpName, Lump, Char, *AName);
        wasAnyChar = true;
      } else {
        GCon->Logf(NAME_Warning, "cannot load char lump for char %d of font '%s'", Char, *AName);
      }
    }
  }

  // if we have no char textures, don't register this font
  // this way we will have standard fonts visible of override failed
  if (!wasAnyChar) {
    AName = NAME_None;
    return;
  }

  RegisterFont(this, AName);

  // set up width of a space character as half width of N character
  // or 4 if character N has no graphic for it
  if (ASpaceWidth < 0) {
    int NIdx = FindChar('N');
    if (NIdx >= 0) {
      SpaceWidth = max2(1, (Chars[NIdx].Width+1)/2);
    } else {
      SpaceWidth = 4;
    }
  } else {
    SpaceWidth = ASpaceWidth;
  }

  BuildTranslations(ColorsUsed, r_palette, false, true);

  // create texture objects for all different colors
  for (auto &&cc : Chars) {
    cc.Textures = new VTexture*[TextColors.length()];
    for (int j = 0; j < TextColors.length(); ++j) {
      cc.Textures[j] = new VFontChar(cc.BaseTex, Translation+j*256);
      // currently all render drivers expect all textures to be registered in texture manager
      GTextureManager.AddTexture(cc.Textures[j]);
    }
  }
}


//==========================================================================
//
//  VFont::~VFont
//
//==========================================================================
VFont::~VFont () {
  for (auto &&cc : Chars) {
    if (cc.Textures) {
      delete[] cc.Textures;
      cc.Textures = nullptr;
    }
  }
  Chars.Clear();
  if (Translation) {
    delete[] Translation;
    Translation = nullptr;
  }
}


//==========================================================================
//
//  VFont::IsSingleTextureFont
//
//==========================================================================
bool VFont::IsSingleTextureFont () const noexcept {
  return false;
}


//==========================================================================
//
//  VFont::BuildTranslations
//
//==========================================================================
void VFont::BuildTranslations (const bool *ColorsUsed, rgba_t *Pal, bool ConsoleTrans, bool Rescale) {
  // calculate luminosity for all colors and find minimal and maximal values for used colors
  float Luminosity[256];
  float MinLum = 1000000.0f;
  float MaxLum = 0.0f;
  for (int i = 1; i < 256; ++i) {
    Luminosity[i] = Pal[i].r*0.299f+Pal[i].g*0.587f+Pal[i].b*0.114f;
    if (ColorsUsed[i]) {
      if (MinLum > Luminosity[i]) MinLum = Luminosity[i];
      if (MaxLum < Luminosity[i]) MaxLum = Luminosity[i];
    }
  }

  // create gradual luminosity values
  float Scale = 1.0f/(Rescale ? MaxLum-MinLum : 255.0f);
  for (int i = 1; i < 256; ++i) {
    Luminosity[i] = (Luminosity[i]-MinLum)*Scale;
    Luminosity[i] = midval(0.0f, Luminosity[i], 1.0f);
  }

  Translation = new rgba_t[256*TextColors.length()];
  for (int ColIdx = 0; ColIdx < TextColors.length(); ++ColIdx) {
    rgba_t *pOut = Translation+ColIdx*256;
    const TArray<VColTranslationDef> &TList = (ConsoleTrans ? TextColors[ColIdx].ConsoleTranslations : TextColors[ColIdx].Translations);
    if (ColIdx == CR_UNTRANSLATED || !TList.length()) {
      memcpy(pOut, Pal, 4*256);
      continue;
    }

    pOut[0] = Pal[0];
    for (int i = 1; i < 256; ++i) {
      int ILum = (int)(Luminosity[i]*256);
      int TDefIdx = 0;
      while (TDefIdx < TList.length()-1 && ILum > TList[TDefIdx].LumTo) ++TDefIdx;
      const VColTranslationDef &TDef = TList[TDefIdx];

      // linearly interpolate between colors
      float v = ((float)(ILum-TDef.LumFrom)/(float)(TDef.LumTo-TDef.LumFrom));
      int r = (int)((1.0f-v)*TDef.From.r+v*TDef.To.r);
      int g = (int)((1.0f-v)*TDef.From.g+v*TDef.To.g);
      int b = (int)((1.0f-v)*TDef.From.b+v*TDef.To.b);
      pOut[i].r = clampToByte(r);
      pOut[i].g = clampToByte(g);
      pOut[i].b = clampToByte(b);
      pOut[i].a = 255;
    }
  }
}


//==========================================================================
//
//  VFont::BuildCharMap
//
//==========================================================================
void VFont::BuildCharMap () {
  if (CharMap.length() || !Chars.length()) return;
  CharMap.clear();
  for (auto &&it : Chars.itemsIdx()) {
    CharMap.put(it.value().Char, it.index());
  }
}


//==========================================================================
//
//  VFont::FindChar
//
//==========================================================================
int VFont::FindChar (int Chr) {
  // check if character is outside of available character range
  if (Chr < FirstChar || Chr > LastChar) return -1;
  // fast look-up for ASCII characters
  if (Chr >= 0 && Chr < 128) return AsciiChars[Chr];
  BuildCharMap();
  auto pp = CharMap.get(Chr);
  return (pp ? *pp : -1);
}


//==========================================================================
//
//  VFont::GetChar
//
//==========================================================================
VTexture *VFont::GetChar (int Chr, CharRect *rect, int *pWidth, int Color) {
  int Idx = FindChar(Chr);
  if (Idx < 0) {
    // try upper-case letter
    Chr = VStr::ToUpper(Chr);
    Idx = FindChar(Chr);
    if (Idx < 0) {
      if (pWidth) *pWidth = SpaceWidth;
      if (rect) rect->clear();
      return nullptr;
    }
  }

  if (Color < 0 || Color >= TextColors.length()) Color = CR_UNTRANSLATED;
  VTexture *Tex = Chars[Idx].Textures ? Chars[Idx].Textures[Color] :
    Chars[Idx].TexNum > 0 ? GTextureManager(Chars[Idx].TexNum) :
    Chars[Idx].BaseTex;
  // need to do it here, so renderer could get the right dimensions
  VTexture *HiTex = (Tex ? Tex->GetHighResolutionTexture() : nullptr);
  if (HiTex) Tex = HiTex;
  if (pWidth) *pWidth = Chars[Idx].Width;
  if (rect) *rect = Chars[Idx].Rect;
  return Tex;
}


//==========================================================================
//
//  VFont::GetCharWidth
//
//==========================================================================
int VFont::GetCharWidth (int Chr) {
  int Idx = FindChar(Chr);
  if (Idx < 0) {
    // try upper-case letter
    Chr = VStr::ToUpper(Chr);
    Idx = FindChar(Chr);
    if (Idx < 0) return SpaceWidth;
  }
  return Chars[Idx].Width;
}


//==========================================================================
//
//  VFont::MarkUsedColors
//
//==========================================================================
void VFont::MarkUsedColors (VTexture *Tex, bool *Used) {
  if (!Tex /*|| Tex->Format == TEXFMT_RGBA*/) return; // wtf?
  const vuint8 *Pixels = Tex->GetPixels8();
  int Count = Tex->GetWidth()*Tex->GetHeight();
  for (int i = 0; i < Count; ++i) Used[Pixels[i]] = true;
}


//==========================================================================
//
//  VFont::ParseColorEscape
//
//  assumes that pColor points to the character right after the color
//  escape character.
//
//  if `escstr` is not `nullptr`, put escate into it (including escape char)
//
//  returns negative for RGB (see `VWidget::DrawCharPic()`)
//
//==========================================================================
int VFont::ParseColorEscape (const char *&pColor, int NormalColor, int BoldColor, VStr *escstr) {
  if (!pColor || !pColor[0]) {
    if (escstr) escstr->clear();
    return CR_UNDEFINED;
  }

  if (escstr) (*escstr) = VStr((char)TEXT_COLOR_ESCAPE);

  int Col = *pColor++;

  if (Col >= 'a' && Col <= 'z') Col = Col-'a'+'A';

  if (Col == '[') {
    // named colors
    char nbuf[32];
    size_t nbpos = 0;
    for (;;) {
      const char ch = *pColor++;
      if (!ch) { --pColor; break; }
      if (ch == ']') break;
      if (nbpos < sizeof(nbuf)-1) nbuf[nbpos++] = ch;
    }
    nbuf[nbpos] = 0;
    Col = FindTextColor(nbuf, CR_UNDEFINED);
    if (escstr) {
      if (Col != CR_UNDEFINED) {
        (*escstr) += '[';
        (*escstr) += VStr(nbuf);
        (*escstr) += ']';
      } else {
        escstr->clear();
      }
    }
  } else if (Col >= 'A' && Col <= 'Z' /*Col < 'A'+NUM_TEXT_COLORS*/) {
    if (escstr) (*escstr) += (char)Col;
    Col -= 'A'; // standard colors
  } else if (Col == '-') {
    if (escstr) (*escstr) += '-';
    Col = NormalColor; // normal color
  } else if (Col == '+') {
    if (escstr) (*escstr) += '+';
    Col = BoldColor; // bold color
  } else {
    --pColor;
    Col = CR_UNDEFINED;
    if (escstr) escstr->clear();
  }

  return Col;
}


//==========================================================================
//
//  VFont::FindTextColor
//
//==========================================================================
int VFont::FindTextColor (const char *Name, int defval) {
  if (!Name || !Name[0]) return defval;
  // "normal" color name
  if (Name[1] && (Name[0] == '!' || Name[0] == '#')) {
    if (Name[0] == '!') ++Name;
    vuint32 cc = M_ParseColor(Name, true/*retZeroIfInvalid*/);
    if (cc) { vassert(cc&0xff000000u); return (int)cc; }
    return defval; // translation name cannot start with '!' or '#'
  }
  // translation names
  for (auto &&it : TextColorLookup) {
    if (VStr::strEquCI(*it.Name, Name)) return it.Index;
  }
  return defval;
}


//==========================================================================
//
//  VFont::StringWidth
//
//==========================================================================
int VFont::StringWidth (VStr String) {
  int w = 0;
  if (!String.isEmpty()) {
    const char *SPtr = *String;
    for (;;) {
      const int c = VStr::Utf8GetChar(SPtr);
      if (!c) break;
      // check for color escape
      if (c == TEXT_COLOR_ESCAPE) {
        ParseColorEscape(SPtr, CR_UNDEFINED, CR_UNDEFINED);
      } else {
        w += GetCharWidth(c)+GetKerning();
      }
    }
  }
  return w;
}


//==========================================================================
//
//  VFont::TextWidth
//
//==========================================================================
int VFont::TextWidth (VStr String) {
  const int len = String.length();
  if (len == 0) return 0;
  int res = 0, pos = 0, start = 0;
  for (;;) {
    if (pos >= len || String[pos] == '\n') {
      int curw = StringWidth(VStr(String, start, pos-start));
      if (res < curw) res = curw;
      if (++pos >= len) break;
      start = pos;
    } else {
      ++pos;
    }
  }
  return res;
}


//==========================================================================
//
//  VFont::TextHeight
//
//==========================================================================
int VFont::TextHeight (VStr String) {
  int h = FontHeight;
  const int len = String.length();
  if (len > 0) {
    const char *s = *String;
    for (int f = len; f > 0; --f, ++s) if (*s == '\n') h += FontHeight;
  }
  return h;
}


//==========================================================================
//
//  VFont::SplitText
//
//==========================================================================
int VFont::SplitText (VStr Text, TArray<VSplitLine> &Lines, int MaxWidth, bool trimRight) {
  Lines.Clear();
  const char *Start = *Text;
  bool WordStart = true;
  int CurW = 0;
  VStr lineCEsc;
  VStr lastCEsc;
  for (const char *SPtr = *Text; *SPtr; ) {
    const char *PChar = SPtr;
    int c = VStr::Utf8GetChar(SPtr);

    // check for color escape
    if (c == TEXT_COLOR_ESCAPE) {
      ParseColorEscape(SPtr, CR_UNDEFINED, CR_UNDEFINED, &lastCEsc);
      //fprintf(stderr, "!!!!! <%s>\n", *VStr(SPtr).quote());
    } else if (c == '\n') {
      VSplitLine &L = Lines.Alloc();
      L.Text = lineCEsc+VStr(Text, (int)(ptrdiff_t)(Start-(*Text)), (int)(ptrdiff_t)(PChar-Start));
      L.Width = CurW;
      Start = SPtr;
      WordStart = true;
      CurW = 0;
      // newline clears all color sequences
      lineCEsc.clear();
      lastCEsc.clear();
    } else if (WordStart && c > ' ') {
      const char *SPtr2 = SPtr;
      int c2 = c;
      int NewW = CurW;
      while (c2 > ' ' || c2 == TEXT_COLOR_ESCAPE) {
        if (c2 != TEXT_COLOR_ESCAPE) NewW += GetCharWidth(c2);
        c2 = VStr::Utf8GetChar(SPtr2);
        // check for color escape
        if (c2 == TEXT_COLOR_ESCAPE) ParseColorEscape(SPtr2, CR_UNDEFINED, CR_UNDEFINED, &lastCEsc);
      }
      if (NewW > MaxWidth && PChar != Start) {
        VSplitLine &L = Lines.Alloc();
        L.Text = lineCEsc+VStr(Text, (int)(ptrdiff_t)(Start-(*Text)), (int)(ptrdiff_t)(PChar-Start));
        L.Width = CurW;
        Start = PChar;
        CurW = 0;
        lineCEsc = lastCEsc;
      }
      WordStart = false;
      CurW += GetCharWidth(c);
    } else if (c <= ' ') {
      WordStart = true;
      CurW += GetCharWidth(c);
    } else {
      CurW += GetCharWidth(c);
    }

    if (!*SPtr && Start != SPtr) {
      VSplitLine &L = Lines.Alloc();
      L.Text = lineCEsc+Start;
      L.Width = CurW;
      lineCEsc = lastCEsc;
    }
  }

  if (trimRight) {
    for (int f = 0; f < Lines.length(); ++f) {
      VStr s = Lines[f].Text;
      while (s.length() && (vuint8)s[s.length()-1] <= 32) s.chopRight(1);
      if (Lines[f].Text.length() != s.length()) {
        Lines[f].Text = s;
        Lines[f].Width = StringWidth(s);
      }
    }
  }

  return Lines.length()*FontHeight;
}


//==========================================================================
//
//  VFont::SplitTextWithNewlines
//
//==========================================================================
VStr VFont::SplitTextWithNewlines (VStr Text, int MaxWidth, bool trimRight) {
  TArray<VSplitLine> Lines;
  SplitText(Text, Lines, MaxWidth, trimRight);
  VStr Ret;
  for (int i = 0; i < Lines.length(); ++i) {
    if (i != 0) Ret += "\n";
    Ret += Lines[i].Text;
  }
  return Ret;
}



//**************************************************************************
//
// VSpecialFont
//
//**************************************************************************

//==========================================================================
//
//  VSpecialFont::VSpecialFont
//
//==========================================================================
VSpecialFont::VSpecialFont (VName AName, const TArray<int> &CharIndexes, const TArray<VName> &CharLumps, const bool *NoTranslate, int ASpaceWidth) {
  RegisterFont(this, AName);

  for (int i = 0; i < 128; ++i) AsciiChars[i] = -1;
  FirstChar = -1;
  LastChar = -1;
  FontHeight = 0;
  Kerning = 0;
  Translation = nullptr;
  bool ColorsUsed[256];
  memset(ColorsUsed, 0, sizeof(ColorsUsed));

  vassert(CharIndexes.length() == CharLumps.length());
  for (int i = 0; i < CharIndexes.length(); ++i) {
    int Char = CharIndexes[i];
    if (Char < 0) Char = (vuint8)(Char&0xff); // just in case
    VName LumpName = CharLumps[i];

    int texid = GTextureManager.AddPatch(LumpName, TEXTYPE_Pic);
    if (texid <= 0) {
      GCon->Logf(NAME_Error, "font '%s': missing patch '%s' for char #%d", *AName, *LumpName, Char);
      texid = -1;
    }

    VTexture *Tex = GTextureManager[texid];
    if (Tex) {
      FFontChar &FChar = Chars.Alloc();
      FChar.Char = Char;
      FChar.TexNum = -1;
      FChar.BaseTex = Tex;
      FChar.Rect.setFull();
      if (Char < 128) AsciiChars[Char] = Chars.length()-1;

      //GCon->Logf(NAME_Debug, "font '%s': char #%d (%c): patch '%s', texture size:%dx%d; ofs:%d,%d; scale:%g,%g", *AName, Char, (Char > 32 && Char < 127 ? (char)Char : '?'), *LumpName, Tex->GetWidth(), Tex->GetHeight(), Tex->SOffset, Tex->TOffset, Tex->SScale, Tex->TScale);

      // calculate height of font character and adjust font height as needed
      int Height = Tex->GetScaledHeightI();
      FChar.Width = Tex->GetScaledWidthI();
      FChar.Height = Height;

      int TOffs = Tex->GetScaledTOffsetI();
      Height += abs(TOffs);
      if (FontHeight < Height) FontHeight = Height;

      // update first and last characters
      if (FirstChar == -1 || FirstChar > Char) FirstChar = Char;
      if (LastChar == -1 || LastChar < Char) LastChar = Char;

      // mark colors that are used by this texture
      MarkUsedColors(Tex, ColorsUsed);

      (void)Tex->GetMaxIntensity(); // cache it
    }
  }

  // exclude non-translated colors from calculations
  for (int i = 0; i < 256; ++i) if (NoTranslate[i]) ColorsUsed[i] = false;

  // set up width of a space character as half width of N character
  // or 4 if character N has no graphic for it
  if (ASpaceWidth < 0) {
    int NIdx = FindChar('N');
    if (NIdx >= 0) {
      SpaceWidth = max2(1, (Chars[NIdx].Width+1)/2);
    } else {
      SpaceWidth = 4;
    }
  } else {
    SpaceWidth = ASpaceWidth;
  }

  BuildTranslations(ColorsUsed, r_palette, false, true);

  // map non-translated colors to their original values
  for (int i = 0; i < TextColors.length(); ++i) {
    for (int j = 0; j < 256; ++j) {
      if (NoTranslate[j]) Translation[i*256+j] = r_palette[j];
    }
  }

  // create texture objects for all different colors
  for (auto &&cc : Chars) {
    cc.Textures = new VTexture*[TextColors.length()];
    for (int j = 0; j < TextColors.length(); ++j) {
      cc.Textures[j] = new VFontChar(cc.BaseTex, Translation+j*256);
      // currently all render drivers expect all textures to be registered in texture manager
      GTextureManager.AddTexture(cc.Textures[j]);
    }
  }
}



//**************************************************************************
//
// VFon1Font
//
//**************************************************************************

//==========================================================================
//
//  VFon1Font::VFon1Font
//
//==========================================================================
VFon1Font::VFon1Font (VName AName, int LumpNum) :
  VFontBitmapBase()
{
  RegisterFont(this, AName);

  VStream *lumpstream = W_CreateLumpReaderNum(LumpNum);
  VCheckedStream Strm(lumpstream, true); // load to memory
  // skip ID
  Strm.Seek(4);
  vuint16 w;
  vuint16 h;
  Strm << w << h;
  SpaceWidth = w;
  FontHeight = h;

  if (SpaceWidth == 0 || FontHeight == 0) Sys_Error("font '%s' at '%s' has zero dimensions", *AName, *W_FullLumpName(LumpNum));
  if (SpaceWidth > 1024-2 || FontHeight > 1024-2) Sys_Error("font '%s' at '%s' has too big dimensions", *AName, *W_FullLumpName(LumpNum));

  FirstChar = 0;
  LastChar = 255;
  Kerning = 0;
  Translation = nullptr;
  for (int i = 0; i < 128; ++i) AsciiChars[i] = i;

  // mark all colors as used and construct a grayscale palette
  bool ColorsUsed[256];
  rgba_t Pal[256];
  ColorsUsed[0] = false;
  Pal[0].r = 0;
  Pal[0].g = 0;
  Pal[0].b = 0;
  Pal[0].a = 0;
  for (int i = 1; i < 256; ++i) {
    ColorsUsed[i] = true;
    Pal[i].r = (i-1)*255/254;
    Pal[i].g = Pal[i].r;
    Pal[i].b = Pal[i].r;
    Pal[i].a = 255;
  }

  BuildTranslations(ColorsUsed, Pal, true, false);

  CalculateAtlasSize(((SpaceWidth+2)*(FontHeight+2))*256);
  GCon->Logf(NAME_Debug, "atlas size for font '%s' from '%s' is %dx%d", *AName, *W_FullLumpName(LumpNum), asize, asize);

  for (int i = 0; i < 256; ++i) {
    ReadFONxChar(Strm, AName, LumpNum, i, SpaceWidth, FontHeight);
  }
  DoneReadingFONx();
}



//**************************************************************************
//
// VFon2Font
//
//**************************************************************************

//==========================================================================
//
//  VFon2Font::VFon2Font
//
//  4       - header
//  2       - height
//  1       - first char
//  1       - last char
//  1       - fixed width flag
//  1
//  1       - active colors
//  1       - kerning flag
//  2 (if have flag) - kerning
//
//==========================================================================
VFon2Font::VFon2Font (VName AName, int LumpNum) {
  RegisterFont(this, AName);

  VStream *lumpstream = W_CreateLumpReaderNum(LumpNum);
  VCheckedStream Strm(lumpstream, true); // load to memory
  // skip ID
  Strm.Seek(4);

  // read header
  FontHeight = Streamer<vuint16>(Strm);
  FirstChar = Streamer<vuint8>(Strm);
  LastChar = Streamer<vuint8>(Strm);
  vuint8 FixedWidthFlag;
  vuint8 RescalePal;
  vuint8 ActiveColors;
  vuint8 KerningFlag;
  Strm << FixedWidthFlag << RescalePal << ActiveColors << KerningFlag;
  Kerning = 0;
  if (KerningFlag&1) Kerning = Streamer<vint16>(Strm);

  Translation = nullptr;
  for (int i = 0; i < 128; ++i) AsciiChars[i] = -1;

  // read character widths
  int Count = LastChar-FirstChar+1;
  vuint16 *Widths = new vuint16[Count];
  int TotalWidth = 0;
  int TotalWidth2 = 0;
  int MaxWidth = 0;
  if (FixedWidthFlag) {
    Strm << Widths[0];
    for (int i = 1; i < Count; ++i) Widths[i] = Widths[0];
    TotalWidth = Widths[0]*Count;
    TotalWidth2 = (Widths[0]+2)*Count;
    MaxWidth = Widths[0];
  } else {
    for (int i = 0; i < Count; ++i) {
      Strm << Widths[i];
      TotalWidth += Widths[i];
      if (Widths[i]) TotalWidth2 += Widths[i]+2;
      if (Widths[i] > MaxWidth) MaxWidth = Widths[i];
    }
  }

       if (FirstChar <= ' ' && LastChar >= ' ') SpaceWidth = Widths[' '-FirstChar];
  else if (FirstChar <= 'N' && LastChar >= 'N') SpaceWidth = (Widths['N'-FirstChar]+1)/2;
  else SpaceWidth = TotalWidth*2/(3*Count);

  if (MaxWidth == 0 || FontHeight == 0) Sys_Error("font '%s' at '%s' has zero dimensions", *AName, *W_FullLumpName(LumpNum));
  if (MaxWidth > 1024-2 || FontHeight > 1024-2) Sys_Error("font '%s' at '%s' has too big dimensions", *AName, *W_FullLumpName(LumpNum));

  // read palette
  bool ColorsUsed[256];
  rgba_t Pal[256];
  memset(ColorsUsed, 0, sizeof(ColorsUsed));
  memset((void *)Pal, 0, sizeof(Pal));
  for (int i = 0; i <= ActiveColors; ++i) {
    ColorsUsed[i] = true;
    Strm << Pal[i].r << Pal[i].g << Pal[i].b;
    Pal[i].a = (i ? 255 : 0);
  }

  BuildTranslations(ColorsUsed, Pal, false, !!RescalePal);

  CalculateAtlasSize(TotalWidth2*(FontHeight+2));
  GCon->Logf(NAME_Debug, "atlas size for font '%s' from '%s' is %dx%d", *AName, *W_FullLumpName(LumpNum), asize, asize);

  for (int i = 0; i < Count; ++i) {
    int DataSize = Widths[i]*FontHeight;
    if (DataSize <= 0) continue;
    ReadFONxChar(Strm, AName, LumpNum, FirstChar+i, Widths[i], FontHeight);
  }
  DoneReadingFONx();

  delete[] Widths;
}



//**************************************************************************
//
// VSingleTextureFont
//
//**************************************************************************

//==========================================================================
//
//  VSingleTextureFont::VSingleTextureFont
//
//==========================================================================
VSingleTextureFont::VSingleTextureFont (VName AName, int TexNum) {
  RegisterFont(this, AName);

  VTexture *Tex = GTextureManager[TexNum];
  if (!Tex) Tex = GTextureManager[0];
  for (int i = 0; i < 128; ++i) AsciiChars[i] = -1;
  AsciiChars[(int)'A'] = 0;
  FirstChar = 'A';
  LastChar = 'A';
  SpaceWidth = Tex->GetScaledWidthI();
  FontHeight = Tex->GetScaledHeightI();
  Kerning = 0;
  Translation = nullptr;

  FFontChar &FChar = Chars.Alloc();
  FChar.Char = 'A';
  FChar.TexNum = TexNum;
  FChar.BaseTex = Tex;
  FChar.Textures = nullptr;
  FChar.Rect.setFull();

  FChar.Width = SpaceWidth;
  FChar.Height = FontHeight;

  (void)Tex->GetMaxIntensity(); // cache it
}


//==========================================================================
//
//  VSingleTextureFont::IsSingleTextureFont
//
//==========================================================================
bool VSingleTextureFont::IsSingleTextureFont () const noexcept {
  return true;
}



//**************************************************************************
//
// VFontChar
//
//**************************************************************************

//==========================================================================
//
//  VFontChar::VFontChar
//
//==========================================================================
VFontChar::VFontChar (VTexture *ATex, rgba_t *APalette)
  : BaseTex(ATex)
  , Palette(APalette)
{
  Type = TEXTYPE_FontChar;
  mFormat = mOrigFormat = TEXFMT_8Pal;
#ifdef VAVOOM_NAME_FONT_TEXTURES
  Name = VName(va("\x7f_fontchar_%d ", vfontcharTxCount++));
#else
  Name = NAME_None;
#endif
  Width = BaseTex->GetWidth();
  Height = BaseTex->GetHeight();
  SOffset = BaseTex->SOffset;
  TOffset = BaseTex->TOffset;
  SScale = fabsf(BaseTex->SScale);
  TScale = fabsf(BaseTex->TScale);
  if (!isFiniteF(SScale) || SScale <= 0.0f) SScale = 1.0f;
  if (!isFiniteF(TScale) || TScale <= 0.0f) TScale = 1.0f;
  //GCon->Logf("created font char with basetex %p (%s)", BaseTex, *BaseTex->Name);
  (void)BaseTex->GetMaxIntensity(); // cache it
}


//==========================================================================
//
//  VFontChar::~VFontChar
//
//==========================================================================
VFontChar::~VFontChar () {
}


//==========================================================================
//
//  VFontChar::GetPixels
//
//==========================================================================
vuint8 *VFontChar::GetPixels () {
  shadeColor = -1; //FIXME
  return BaseTex->GetPixels8();
}


//==========================================================================
//
//  VFontChar::GetPalette
//
//==========================================================================
rgba_t *VFontChar::GetPalette () {
  return Palette;
}


//==========================================================================
//
//  VFontChar::GetHighResolutionTexture
//
//==========================================================================
VTexture *VFontChar::GetHighResolutionTexture () {
  if (!HiResTexture) {
    #if 0
    GCon->Logf("getting hires texture for basetex %p", BaseTex);
    GCon->Logf(" basetex name: %s", *BaseTex->Name);
    GCon->Logf(" basetex hires: %p", BaseTex->GetHighResolutionTexture());
    #endif
    if (!r_hirestex) return nullptr;
    VTexture *Tex = BaseTex->GetHighResolutionTexture();
    if (Tex) HiResTexture = new VFontChar(Tex, Palette);
  }
  return HiResTexture;
}
