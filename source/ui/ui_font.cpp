//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************
#include "gamedefs.h"
#include "textures/r_tex.h"
#include "ui.h"


struct VColTranslationDef {
  rgba_t From;
  rgba_t To;
  int LumFrom;
  int LumTo;
};

struct VTextColourDef {
  TArray<VColTranslationDef> Translations;
  TArray<VColTranslationDef> ConsoleTranslations;
  rgba_t FlatColour;
  int Index;
};

struct VColTransMap {
  VName Name;
  int Index;
};


// like regular font, but initialised using explicit list of patches
class VSpecialFont : public VFont {
public:
  VSpecialFont (VName, const TArray<int> &, const TArray<VName> &, const bool *);
};

// font in FON1 format
class VFon1Font : public VFont {
public:
  VFon1Font (VName, int);
};

// font in FON2 format
class VFon2Font : public VFont {
public:
  VFon2Font (VName, int);
};

// font consisting of a single texture for character 'A'
class VSingleTextureFont : public VFont {
public:
  VSingleTextureFont (VName, int);
};

// texture class for regular font characters
class VFontChar : public VTexture {
private:
  VTexture *BaseTex;
  rgba_t *Palette;

public:
  VFontChar (VTexture*, rgba_t*);
  virtual ~VFontChar () override;
  virtual vuint8 *GetPixels () override;
  virtual rgba_t *GetPalette () override;
  virtual void Unload () override;
  virtual VTexture *GetHighResolutionTexture () override;
};

// texture class for FON1 font characters
class VFontChar2 : public VTexture {
private:
  int LumpNum;
  int FilePos;
  vuint8 *Pixels;
  rgba_t *Palette;
  int MaxCol;

public:
  VFontChar2 (int, int, int, int, rgba_t *, int);
  virtual ~VFontChar2 () override;
  virtual vuint8 *GetPixels () override;
  virtual rgba_t *GetPalette () override;
  virtual void Unload () override;
};


// ////////////////////////////////////////////////////////////////////////// //
VFont *SmallFont;
VFont *ConFont;
VFont *VFont::Fonts;

static TArray<VTextColourDef> TextColours;
static TArray<VColTransMap> TextColourLookup;


//==========================================================================
//
//  VFont::StaticInit
//
//==========================================================================
void VFont::StaticInit () {
  guard(VFont::StaticInit);
  ParseTextColours();

  // initialise standard fonts

  // small font
  if (W_CheckNumForName(NAME_fonta_s) >= 0) {
    SmallFont = new VFont(NAME_smallfont, "fonta%02d", 33, 95, 1);
  } else {
    SmallFont = new VFont(NAME_smallfont, "stcfn%03d", 33, 95, 33);
  }
  // strife's second small font
  if (W_CheckNumForName(NAME_stbfn033) >= 0) {
    new VFont(NAME_smallfont2, "stbfn%03d", 33, 95, 33);
  }
  // big font
  if (W_CheckNumForName(NAME_bigfont) >= 0) {
    GetFont(NAME_bigfont, NAME_bigfont);
  } else {
    new VFont(NAME_bigfont, "fontb%02d", 33, 95, 1);
  }
  // console font
  ConFont = GetFont(NAME_consolefont, NAME_confont);

  // load custom fonts
  ParseFontDefs();
  unguard;
}


//==========================================================================
//
//  VFont::StaticShutdown
//
//==========================================================================
void VFont::StaticShutdown () {
  guard(VFont::StaticShutdown);
  VFont *F = Fonts;
  while (F) {
    VFont *Next = F->Next;
    delete F;
    F = Next;
  }
  Fonts = nullptr;
  TextColours.Clear();
  TextColourLookup.Clear();
  unguard;
}


//==========================================================================
//
//  VFont::ParseTextColours
//
//==========================================================================
void VFont::ParseTextColours () {
  guard(VFont::ParseTextColours);
  TArray<VTextColourDef> TempDefs;
  TArray<VColTransMap> TempColours;
  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    if (W_LumpName(Lump) != NAME_textcolo) continue;
    VScriptParser sc(*W_LumpName(Lump), W_CreateLumpReaderNum(Lump));
    while (!sc.AtEnd()) {
      VTextColourDef &Col = TempDefs.Alloc();
      Col.FlatColour.r = 0;
      Col.FlatColour.g = 0;
      Col.FlatColour.b = 0;
      Col.FlatColour.a = 255;
      Col.Index = -1;

      TArray<VName> Names;
      VColTranslationDef TDef;
      TDef.LumFrom = -1;
      TDef.LumTo = -1;

      // name for this colour
      sc.ExpectString();
      Names.Append(*sc.String.ToLower());
      // additional names
      while (!sc.Check("{")) {
        if (Names[0] == "untranslated") sc.Error("Colour \"untranslated\" cannot have any other names");
        sc.ExpectString();
        Names.Append(*sc.String.ToLower());
      }

      int TranslationMode = 0;
      while (!sc.Check("}")) {
        if (sc.Check("Console:")) {
          if (TranslationMode == 1) sc.Error("Only one console text colour definition allowed");
          TranslationMode = 1;
          TDef.LumFrom = -1;
          TDef.LumTo = -1;
        } else if (sc.Check("Flat:")) {
          sc.ExpectString();
          vuint32 C = M_ParseColour(sc.String);
          Col.FlatColour.r = (C >> 16) & 255;
          Col.FlatColour.g = (C >> 8) & 255;
          Col.FlatColour.b = C & 255;
          Col.FlatColour.a = 255;
        } else {
          // from colour
          sc.ExpectString();
          vuint32 C = M_ParseColour(sc.String);
          TDef.From.r = (C >> 16) & 255;
          TDef.From.g = (C >> 8) & 255;
          TDef.From.b = C & 255;
          TDef.From.a = 255;

          // to colour
          sc.ExpectString();
          C = M_ParseColour(sc.String);
          TDef.To.r = (C >> 16) & 255;
          TDef.To.g = (C >> 8) & 255;
          TDef.To.b = C & 255;
          TDef.To.a = 255;

          if (sc.CheckNumber()) {
            // optional luminosity ranges
            if (TDef.LumFrom == -1 && sc.Number != 0) sc.Error("First colour range must start at position 0");
            if (sc.Number < 0 || sc.Number > 256) sc.Error("Colour index must be in range from 0 to 256");
            if (sc.Number <= TDef.LumTo) sc.Error("Colour range must start after end of previous range");
            TDef.LumFrom = sc.Number;

            sc.ExpectNumber();
            if (sc.Number < 0 || sc.Number > 256) sc.Error("Colour index must be in range from 0 to 256");
            if (sc.Number <= TDef.LumFrom) sc.Error("Ending colour index must be greater than start index");
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
        if (Col.Translations.Num() != 0 || Col.ConsoleTranslations.Num() != 0) {
          sc.Error("The \"untranslated\" colour must be left undefined");
        }
      } else {
        if (Col.Translations.Num() == 0) sc.Error("There must be at least one normal range for a colour");
        if (Col.ConsoleTranslations.Num() == 0) {
          // if console colour translation is not defined, make it white
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

      // add all names to the list of colours
      for (int i = 0; i < Names.Num(); ++i) {
        // check for redefined colours
        int CIdx;
        for (CIdx = 0; CIdx < TempColours.Num(); ++CIdx) {
          if (TempColours[CIdx].Name == Names[i]) {
            TempColours[CIdx].Index = TempDefs.Num()-1;
            break;
          }
        }
        if (CIdx == TempColours.Num()) {
          VColTransMap &CMap = TempColours.Alloc();
          CMap.Name = Names[i];
          CMap.Index = TempDefs.Num()-1;
        }
      }
    }
  }

  // put colour definitions in it's final location
  for (int i = 0; i < TempColours.Num(); ++i) {
    VColTransMap &TmpCol = TempColours[i];
    VTextColourDef &TmpDef = TempDefs[TmpCol.Index];
    if (TmpDef.Index == -1) {
      TmpDef.Index = TextColours.Num();
      TextColours.Append(TmpDef);
    }
    VColTransMap &Col = TextColourLookup.Alloc();
    Col.Name = TmpCol.Name;
    Col.Index = TmpDef.Index;
  }

  // make sure all built-in colours are defined
  check(TextColours.Num() >= NUM_TEXT_COLOURS);
  unguard;
}


//==========================================================================
//
//  VFont::ParseFontDefs
//
//==========================================================================
void VFont::ParseFontDefs () {
#define CHECK_TYPE(Id) if (FontType == Id) \
  sc.Error(va("Invalid combination of properties in font '%s'", *FontName))

  guard(VFont::ParseFontDefs);
  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    if (W_LumpName(Lump) != NAME_fontdefs) continue;
    VScriptParser sc(*W_LumpName(Lump), W_CreateLumpReaderNum(Lump));
    while (!sc.AtEnd()) {
      // name of the font
      sc.ExpectString();
      VName FontName = *sc.String.ToLower();
      sc.Expect("{");

      int FontType = 0;
      VStr Template;
      int First = 33;
      int Count = 223;
      int Start = 33;
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
        } else if (sc.Check("notranslate")) {
          CHECK_TYPE(1);
          while (sc.CheckNumber() && !sc.Crossed) {
            if (sc.Number >= 0 && sc.Number < 256) NoTranslate[sc.Number] = true;
          }
          FontType = 2;
        } else {
          CHECK_TYPE(1);
          sc.ExpectString();
          const char *CPtr = *sc.String;
          int CharIdx = VStr::GetChar(CPtr);
          sc.ExpectString();
          VName LumpName(*sc.String, VName::AddLower8);
          if (W_CheckNumForName(LumpName, WADNS_Graphics) >= 0) {
            CharIndexes.Append(CharIdx);
            CharLumps.Append(LumpName);
          }
          FontType = 2;
        }
      }
      if (FontType == 1) {
        new VFont(FontName, Template, First, Count, Start);
      } else if (FontType == 2) {
        if (CharIndexes.Num()) {
          new VSpecialFont(FontName, CharIndexes, CharLumps, NoTranslate);
        }
      } else {
        sc.Error("Font has no attributes");
      }
    }
  }
  unguard;

#undef CHECK_TYPE
}


//==========================================================================
//
//  VFont::FindFont
//
//==========================================================================
VFont *VFont::FindFont (VName AName) {
  guard(VFont::FindFont);
  for (VFont *F = Fonts; F; F = F->Next) if (F->Name == AName) return F;
  return nullptr;
  unguard;
}


//==========================================================================
//
//  VFont::GetFont
//
//==========================================================================
VFont *VFont::GetFont (VName AName, VName LumpName) {
  guard(VFont::GetFont);
  VFont *F = FindFont(AName);
  if (F) return F;

  // check for wad lump
  int Lump = W_CheckNumForName(LumpName);
  if (Lump >= 0) {
    // read header
    VStream *Strm = W_CreateLumpReaderNum(Lump);
    char Hdr[4];
    Strm->Serialise(Hdr, 4);
    delete Strm;
    if (Hdr[0] == 'F' && Hdr[1] == 'O' && Hdr[2] == 'N') {
      if (Hdr[3] == '1') return new VFon1Font(AName, Lump);
      if (Hdr[3] == '2') return new VFon2Font(AName, Lump);
    }
  }

  int TexNum = GTextureManager.CheckNumForName(LumpName, TEXTYPE_Any);
  if (TexNum <= 0) TexNum = GTextureManager.AddPatch(LumpName, TEXTYPE_Pic);
  if (TexNum > 0) return new VSingleTextureFont(AName, TexNum);

  return nullptr;
  unguard;
}


//==========================================================================
//
//  VFont::VFont
//
//==========================================================================
VFont::VFont () {
}


//==========================================================================
//
//  VFont::VFont
//
//==========================================================================
VFont::VFont (VName AName, const VStr &FormatStr, int First, int Count, int StartIndex) {
  guard(VFont::VFont);
  Name = AName;
  Next = Fonts;
  Fonts = this;

  for (int i = 0; i < 128; ++i) AsciiChars[i] = -1;
  FirstChar = -1;
  LastChar = -1;
  FontHeight = 0;
  Kerning = 0;
  Translation = nullptr;
  bool ColoursUsed[256];
  memset(ColoursUsed, 0, sizeof(ColoursUsed));

  for (int i = 0; i < Count; ++i) {
    int Char = i+First;
    char Buffer[10];
    snprintf(Buffer, sizeof(Buffer), *FormatStr, i+StartIndex);
    VName LumpName(Buffer, VName::AddLower8);
    int Lump = W_CheckNumForName(LumpName, WADNS_Graphics);

    // in Doom, stcfn121 is actually a '|' and not 'y' and many wad
    // authors provide it as such, so put it in correct location
    if (LumpName == "stcfn121" &&
        (W_CheckNumForName("stcfn120", WADNS_Graphics) == -1 ||
         W_CheckNumForName("stcfn122", WADNS_Graphics) == -1))
    {
      Char = '|';
    }

    if (Lump >= 0) {
      VTexture *Tex = GTextureManager[GTextureManager.AddPatch(LumpName, TEXTYPE_Pic)];
      FFontChar &FChar = Chars.Alloc();
      FChar.Char = Char;
      FChar.TexNum = -1;
      FChar.BaseTex = Tex;
      if (Char < 128) AsciiChars[Char] = Chars.Num()-1;

      // calculate height of font character and adjust font height as needed
      int Height = Tex->GetScaledHeight();
      int TOffs = Tex->GetScaledTOffset();
      Height += abs(TOffs);
      if (FontHeight < Height) FontHeight = Height;

      // update first and last characters
      if (FirstChar == -1) FirstChar = Char;
      LastChar = Char;

      // mark colours that are used by this texture
      MarkUsedColours(Tex, ColoursUsed);
    }
  }

  // set up width of a space character as half width of N character
  // or 4 if character N has no graphic for it
  int NIdx = FindChar('N');
  if (NIdx >= 0) {
    SpaceWidth = (Chars[NIdx].BaseTex->GetScaledWidth()+1)/2;
    if (SpaceWidth < 1) SpaceWidth = 1; //k8: just in case
  } else {
    SpaceWidth = 4;
  }

  BuildTranslations(ColoursUsed, r_palette, false, true);

  // create texture objects for all different colours
  for (int i = 0; i < Chars.Num(); ++i) {
    Chars[i].Textures = new VTexture*[TextColours.Num()];
    for (int j = 0; j < TextColours.Num(); ++j) {
      Chars[i].Textures[j] = new VFontChar(Chars[i].BaseTex, Translation+j*256);
      // currently all render drivers expect all textures to be registered in texture manager
      GTextureManager.AddTexture(Chars[i].Textures[j]);
    }
  }
  unguard;
}


//==========================================================================
//
//  VFont::~VFont
//
//==========================================================================
VFont::~VFont () {
  for (int i = 0; i < Chars.Num(); ++i) {
    if (Chars[i].Textures) {
      delete[] Chars[i].Textures;
      Chars[i].Textures = nullptr;
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
//  VFont::BuildTranslations
//
//==========================================================================
void VFont::BuildTranslations (const bool *ColoursUsed, rgba_t *Pal, bool ConsoleTrans, bool Rescale) {
  guard(VFont::BuildTranslations);
  // calculate luminosity for all colours and find minimal and maximal values for used colours
  float Luminosity[256];
  float MinLum = 1000000.0f;
  float MaxLum = 0.0f;
  for (int i = 1; i < 256; ++i) {
    Luminosity[i] = Pal[i].r*0.299+Pal[i].g*0.587+Pal[i].b*0.114;
    if (ColoursUsed[i]) {
      if (MinLum > Luminosity[i]) MinLum = Luminosity[i];
      if (MaxLum < Luminosity[i]) MaxLum = Luminosity[i];
    }
  }
  // create gradual luminosity values
  float Scale = 1.0f/(Rescale ? MaxLum-MinLum : 255.0f);
  for (int i = 1; i < 256; ++i) {
    Luminosity[i] = (Luminosity[i]-MinLum)*Scale;
    Luminosity[i] = MID(0.0, Luminosity[i], 1.0);
  }

  Translation = new rgba_t[256*TextColours.Num()];
  for (int ColIdx = 0; ColIdx < TextColours.Num(); ++ColIdx) {
    rgba_t *pOut = Translation+ColIdx*256;
    const TArray<VColTranslationDef>& TList = ConsoleTrans ?
      TextColours[ColIdx].ConsoleTranslations :
      TextColours[ColIdx].Translations;
    if (ColIdx == CR_UNTRANSLATED || !TList.Num()) {
      memcpy(pOut, Pal, 4*256);
      continue;
    }

    pOut[0] = Pal[0];
    for (int i = 1; i < 256; ++i) {
      int ILum = (int)(Luminosity[i]*256);
      int TDefIdx = 0;
      while (TDefIdx < TList.Num()-1 && ILum > TList[TDefIdx].LumTo) ++TDefIdx;
      const VColTranslationDef &TDef = TList[TDefIdx];

      // linearly interpolate between colours
      float v = ((float)(ILum-TDef.LumFrom)/(float)(TDef.LumTo-TDef.LumFrom));
      int r = (int)((1.0-v)*TDef.From.r+v*TDef.To.r);
      int g = (int)((1.0-v)*TDef.From.g+v*TDef.To.g);
      int b = (int)((1.0-v)*TDef.From.b+v*TDef.To.b);
      pOut[i].r = MID(0, r, 255);
      pOut[i].g = MID(0, g, 255);
      pOut[i].b = MID(0, b, 255);
      pOut[i].a = 255;
    }
  }
  unguard;
}


//==========================================================================
//
//  VFont::GetChar
//
//==========================================================================
int VFont::FindChar (int Chr) const {
  // check if character is outside of available character range
  if (Chr < FirstChar || Chr > LastChar) return -1;
  // fast look-up for ASCII characters
  if (Chr < 128) return AsciiChars[Chr];
  // a slower one for unicode
  for (int i = 0; i < Chars.Num(); ++i) if (Chars[i].Char == Chr) return i;
  // oops
  return -1;
}


//==========================================================================
//
//  VFont::GetChar
//
//==========================================================================
VTexture *VFont::GetChar (int Chr, int *pWidth, int Colour) const {
  guard(VFont::GetChar);
  int Idx = FindChar(Chr);
  if (Idx < 0) {
    // try upper-case letter
    Chr = VStr::ToUpper(Chr);
    Idx = FindChar(Chr);
    if (Idx < 0) {
      *pWidth = SpaceWidth;
      return nullptr;
    }
  }

  if (Colour < 0 || Colour >= TextColours.Num()) Colour = CR_UNTRANSLATED;
  VTexture *Tex = Chars[Idx].Textures ? Chars[Idx].Textures[Colour] :
    Chars[Idx].TexNum > 0 ? GTextureManager(Chars[Idx].TexNum) :
    Chars[Idx].BaseTex;
  *pWidth = Tex->GetScaledWidth();
  return Tex;
  unguard;
}


//==========================================================================
//
//  VFont::GetCharWidth
//
//==========================================================================
int VFont::GetCharWidth (int Chr) const {
  guard(VFont::GetCharWidth);
  int Idx = FindChar(Chr);
  if (Idx < 0) {
    // try upper-case letter
    Chr = VStr::ToUpper(Chr);
    Idx = FindChar(Chr);
    if (Idx < 0) return SpaceWidth;
  }
  return Chars[Idx].BaseTex->GetScaledWidth();
  unguard;
}


//==========================================================================
//
//  VFont::MarkUsedColours
//
//==========================================================================
void VFont::MarkUsedColours (VTexture *Tex, bool *Used) {
  guard(VFont::MarkUsedColours);
  const vuint8 *Pixels = Tex->GetPixels8();
  int Count = Tex->GetWidth()*Tex->GetHeight();
  for (int i = 0; i < Count; ++i) Used[Pixels[i]] = true;
  unguard;
}


//==========================================================================
//
//  VFont::ParseColourEscape
//
//  assumes that pColour points to the character right after the colour
//  escape character
//
//==========================================================================
int VFont::ParseColourEscape (const char *&pColour, int NormalColour, int BoldColour) {
  guard(VFont::ParseColourEscape);
  const char *Chr = pColour;
  int Col = *Chr++;

       if (Col >= 'A' && Col < 'A'+NUM_TEXT_COLOURS) Col -= 'A'; // standard colors, upper case
  else if (Col >= 'a' && Col < 'a' + NUM_TEXT_COLOURS) Col -= 'a'; // standard colors, lower case
  else if (Col == '-') Col = NormalColour; // normal colour
  else if (Col == '+') Col = BoldColour; // bold colour
  else if (Col == '[') {
    // named colours
    VStr CName;
    while (*Chr && *Chr != ']') CName += *Chr++;
    if (*Chr == ']') Chr++;
    Col = FindTextColour(*CName.ToLower());
  } else {
    if (!Col) --Chr;
    Col = CR_UNDEFINED;
  }

  // set pointer after the colour definition
  pColour = Chr;
  return Col;
  unguard;
}


//==========================================================================
//
//  VFont::FindTextColour
//
//==========================================================================
int VFont::FindTextColour (VName Name) {
  guard(VFont::FindTextColour);
  for (int i = 0; i < TextColourLookup.Num(); ++i) {
    if (TextColourLookup[i].Name == Name) return TextColourLookup[i].Index;
  }
  return CR_UNTRANSLATED;
  unguard;
}


//==========================================================================
//
//  VFont::StringWidth
//
//==========================================================================
int VFont::StringWidth (const VStr &String) const {
  guard(VFont::StringWidth);
  int w = 0;
  for (const char *SPtr = *String; *SPtr; ) {
    int c = VStr::GetChar(SPtr);
    // check for colour escape
    if (c == TEXT_COLOUR_ESCAPE) {
      ParseColourEscape(SPtr, CR_UNDEFINED, CR_UNDEFINED);
      continue;
    }
    w += GetCharWidth(c)+GetKerning();
  }
  return w;
  unguard;
}


//==========================================================================
//
//  VFont::TextWidth
//
//==========================================================================
int VFont::TextWidth (const VStr &String) const {
  guard(VFont::TextWidth);
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
  unguard;
}


//==========================================================================
//
//  VFont::TextHeight
//
//==========================================================================
int VFont::TextHeight (const VStr &String) const {
  guard(VFont::TextHeight);
  int h = FontHeight;
  const int len = String.length();
  if (len > 0) {
    const char *s = *String;
    for (int f = len; f > 0; --f, ++s) if (*s == '\n') h += FontHeight;
  }
  return h;
  unguard;
}


//==========================================================================
//
//  VFont::SplitText
//
//==========================================================================
int VFont::SplitText (const VStr &Text, TArray<VSplitLine> &Lines, int MaxWidth) const {
  guard(VFont::SplitText);
  Lines.Clear();
  const char *Start = *Text;
  bool WordStart = true;
  int CurW = 0;
  for (const char *SPtr = *Text; *SPtr; ) {
    const char *PChar = SPtr;
    int c = VStr::GetChar(SPtr);

    // check for colour escape
    if (c == TEXT_COLOUR_ESCAPE) {
      ParseColourEscape(SPtr, CR_UNDEFINED, CR_UNDEFINED);
      continue;
    }

    if (c == '\n') {
      VSplitLine &L = Lines.Alloc();
      L.Text = VStr(Text, (int)(ptrdiff_t)(Start-(*Text)), (int)(ptrdiff_t)(PChar-Start));
      L.Width = CurW;
      Start = SPtr;
      WordStart = true;
      CurW = 0;
    } else if (WordStart && c > ' ') {
      const char *SPtr2 = SPtr;
      int c2 = c;
      int NewW = CurW;
      while (c2 > ' ' || c2 == TEXT_COLOUR_ESCAPE) {
        if (c2 != TEXT_COLOUR_ESCAPE) NewW += GetCharWidth(c2);
        c2 = VStr::GetChar(SPtr2);
        // check for colour escape
        if (c2 == TEXT_COLOUR_ESCAPE) ParseColourEscape(SPtr2, CR_UNDEFINED, CR_UNDEFINED);
      }
      if (NewW > MaxWidth && PChar != Start) {
        VSplitLine &L = Lines.Alloc();
        L.Text = VStr(Text, (int)(ptrdiff_t)(Start-(*Text)), (int)(ptrdiff_t)(PChar-Start));
        L.Width = CurW;
        Start = PChar;
        CurW = 0;
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
      L.Text = Start;
      L.Width = CurW;
    }
  }
  return Lines.Num()*FontHeight;
  unguard;
}


//==========================================================================
//
//  VFont::SplitTextWithNewlines
//
//==========================================================================
VStr VFont::SplitTextWithNewlines (const VStr &Text, int MaxWidth) const {
  guard(VFont::SplitTextWithNewlines);
  TArray<VSplitLine> Lines;
  SplitText(Text, Lines, MaxWidth);
  VStr Ret;
  for (int i = 0; i < Lines.Num(); ++i) {
    if (i != 0) Ret += "\n";
    Ret += Lines[i].Text;
  }
  return Ret;
  unguard;
}


// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  VSpecialFont::VSpecialFont
//
//==========================================================================
VSpecialFont::VSpecialFont (VName AName, const TArray<int>& CharIndexes, const TArray<VName>& CharLumps, const bool *NoTranslate) {
  guard(VSpecialFont::VSpecialFont);
  Name = AName;
  Next = Fonts;
  Fonts = this;

  for (int i = 0; i < 128; ++i) AsciiChars[i] = -1;
  FirstChar = -1;
  LastChar = -1;
  FontHeight = 0;
  Kerning = 0;
  Translation = nullptr;
  bool ColoursUsed[256];
  memset(ColoursUsed, 0, sizeof(ColoursUsed));

  check(CharIndexes.Num() == CharLumps.Num());
  for (int i = 0; i < CharIndexes.Num(); ++i) {
    int Char = CharIndexes[i];
    VName LumpName = CharLumps[i];

    VTexture *Tex = GTextureManager[GTextureManager.AddPatch(LumpName, TEXTYPE_Pic)];
    FFontChar &FChar = Chars.Alloc();
    FChar.Char = Char;
    FChar.TexNum = -1;
    FChar.BaseTex = Tex;
    if (Char < 128) AsciiChars[Char] = Chars.Num()-1;

    // calculate height of font character and adjust font height as needed
    int Height = Tex->GetScaledHeight();
    int TOffs = Tex->GetScaledTOffset();
    Height += abs(TOffs);
    if (FontHeight < Height) FontHeight = Height;

    // update first and last characters
    if (FirstChar == -1) FirstChar = Char;
    LastChar = Char;

    // mark colours that are used by this texture
    MarkUsedColours(Tex, ColoursUsed);
  }

  // exclude non-translated colours from calculations
  for (int i = 0; i < 256; ++i) if (NoTranslate[i]) ColoursUsed[i] = false;

  // set up width of a space character as half width of N character
  // or 4 if character N has no graphic for it
  int NIdx = FindChar('N');
  if (NIdx >= 0) {
    SpaceWidth = (Chars[NIdx].BaseTex->GetScaledWidth() + 1) / 2;
    if (SpaceWidth < 1) SpaceWidth = 1; //k8: just in case
  } else {
    SpaceWidth = 4;
  }

  BuildTranslations(ColoursUsed, r_palette, false, true);

  // map non-translated colours to their original values
  for (int i = 0; i < TextColours.Num(); ++i) {
    for (int j = 0; j < 256; ++j) {
      if (NoTranslate[j]) Translation[i*256+j] = r_palette[j];
    }
  }

  // create texture objects for all different colours
  for (int i = 0; i < Chars.Num(); ++i) {
    Chars[i].Textures = new VTexture*[TextColours.Num()];
    for (int j = 0; j < TextColours.Num(); ++j) {
      Chars[i].Textures[j] = new VFontChar(Chars[i].BaseTex, Translation+j*256);
      // currently all render drivers expect all textures to be registered in texture manager
      GTextureManager.AddTexture(Chars[i].Textures[j]);
    }
  }
  unguard;
}


// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  VFon1Font::VFon1Font
//
//==========================================================================
VFon1Font::VFon1Font (VName AName, int LumpNum) {
  guard(VFon1Font::VFon1Font);
  Name = AName;
  Next = Fonts;
  Fonts = this;

  VStream *Strm = W_CreateLumpReaderNum(LumpNum);
  // skip ID
  Strm->Seek(4);
  vuint16 w;
  vuint16 h;
  *Strm << w << h;
  SpaceWidth = w;
  FontHeight = h;

  FirstChar = 0;
  LastChar = 255;
  Kerning = 0;
  Translation = nullptr;
  for (int i = 0; i < 128; ++i) AsciiChars[i] = i;

  // mark all colours as used and construct a grayscale palette
  bool ColoursUsed[256];
  rgba_t Pal[256];
  ColoursUsed[0] = false;
  Pal[0].r = 0;
  Pal[0].g = 0;
  Pal[0].b = 0;
  Pal[0].a = 0;
  for (int i = 1; i < 256; ++i) {
    ColoursUsed[i] = true;
    Pal[i].r = (i-1)*255/254;
    Pal[i].g = Pal[i].r;
    Pal[i].b = Pal[i].r;
    Pal[i].a = 255;
  }

  BuildTranslations(ColoursUsed, Pal, true, false);

  for (int i = 0; i < 256; ++i) {
    FFontChar &FChar = Chars.Alloc();
    FChar.Char = i;
    FChar.TexNum = -1;

    // create texture objects for all different colours
    FChar.Textures = new VTexture*[TextColours.Num()];
    for (int j = 0; j < TextColours.Num(); ++j) {
      FChar.Textures[j] = new VFontChar2(LumpNum, Strm->Tell(), SpaceWidth, FontHeight, Translation+j*256, 255);
      // currently all render drivers expect all textures to be registered in texture manager
      GTextureManager.AddTexture(FChar.Textures[j]);
    }
    if (FChar.Textures[CR_UNTRANSLATED]) FChar.BaseTex = FChar.Textures[CR_UNTRANSLATED];

    // skip character data
    int Count = SpaceWidth*FontHeight;
    do {
      vint8 Code = Streamer<vint8>(*Strm);
      if (Code >= 0) {
        Count -= Code+1;
        while (Code-- >= 0) Streamer<vint8>(*Strm);
      } else if (Code != -128) {
        Count -= 1-Code;
        Streamer<vint8>(*Strm);
      }
    } while (Count > 0);
    if (Count < 0) Sys_Error("Overflow decompressing a character %d", i);
  }

  delete Strm;
  unguard;
}


// ////////////////////////////////////////////////////////////////////////// //

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
//  1       - active colours
//  1       - kerning flag
//  2 (if have flag) - kerning
//
//==========================================================================
VFon2Font::VFon2Font (VName AName, int LumpNum) {
  guard(VFon2Font::VFon2Font);
  Name = AName;
  Next = Fonts;
  Fonts = this;

  VStream *Strm = W_CreateLumpReaderNum(LumpNum);
  // skip ID
  Strm->Seek(4);

  // read header
  FontHeight = Streamer<vuint16>(*Strm);
  FirstChar = Streamer<vuint8>(*Strm);
  LastChar = Streamer<vuint8>(*Strm);
  vuint8 FixedWidthFlag;
  vuint8 RescalePal;
  vuint8 ActiveColours;
  vuint8 KerningFlag;
  *Strm << FixedWidthFlag << RescalePal << ActiveColours << KerningFlag;
  Kerning = 0;
  if (KerningFlag&1) Kerning = Streamer<vint16>(*Strm);

  Translation = nullptr;
  for (int i = 0; i < 128; ++i) AsciiChars[i] = -1;

  // read character widths
  int Count = LastChar-FirstChar+1;
  vuint16 *Widths = new vuint16[Count];
  int TotalWidth = 0;
  if (FixedWidthFlag) {
    *Strm << Widths[0];
    for (int i = 1; i < Count; ++i) Widths[i] = Widths[0];
    TotalWidth = Widths[0]*Count;
  } else {
    for (int i = 0; i < Count; ++i) {
      *Strm << Widths[i];
      TotalWidth += Widths[i];
    }
  }

       if (FirstChar <= ' ' && LastChar >= ' ') SpaceWidth = Widths[' ' - FirstChar];
  else if (FirstChar <= 'N' && LastChar >= 'N') SpaceWidth = (Widths['N'-FirstChar]+1)/2;
  else SpaceWidth = TotalWidth*2/(3*Count);

  // read palette
  bool ColoursUsed[256];
  rgba_t Pal[256];
  memset(ColoursUsed, 0, sizeof(ColoursUsed));
  memset(Pal, 0, sizeof(Pal));
  for (int i = 0; i <= ActiveColours; ++i) {
    ColoursUsed[i] = true;
    *Strm << Pal[i].r << Pal[i].g << Pal[i].b;
    Pal[i].a = (i ? 255 : 0);
  }

  BuildTranslations(ColoursUsed, Pal, false, !!RescalePal);

  for (int i = 0; i < Count; ++i) {
    int Chr = FirstChar+i;
    int DataSize = Widths[i]*FontHeight;
    if (DataSize > 0) {
      FFontChar &FChar = Chars.Alloc();
      FChar.Char = Chr;
      FChar.TexNum = -1;
      if (Chr < 128) AsciiChars[Chr] = Chars.Num()-1;

      // create texture objects for all different colours
      FChar.Textures = new VTexture*[TextColours.Num()];
      for (int j = 0; j < TextColours.Num(); ++j) {
        FChar.Textures[j] = new VFontChar2(LumpNum, Strm->Tell(), Widths[i], FontHeight, Translation+j*256, ActiveColours);
        // currently all render drivers expect all textures to be registered in texture manager
        GTextureManager.AddTexture(FChar.Textures[j]);
      }
      if (FChar.Textures[CR_UNTRANSLATED]) FChar.BaseTex = FChar.Textures[CR_UNTRANSLATED];

      // skip character data
      do {
        vint8 Code = Streamer<vint8>(*Strm);
        if (Code >= 0) {
          DataSize -= Code+1;
          while (Code-- >= 0) Streamer<vint8>(*Strm);
        } else if (Code != -128) {
          DataSize -= 1-Code;
          Streamer<vint8>(*Strm);
        }
      } while (DataSize > 0);
      if (DataSize < 0) Sys_Error("Overflow decompressing a character %d", i);
    }
  }

  delete Strm;
  delete[] Widths;
  unguard;
}


// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  VSingleTextureFont::VSingleTextureFont
//
//==========================================================================
VSingleTextureFont::VSingleTextureFont (VName AName, int TexNum) {
  guard(VSingleTextureFont::VSingleTextureFont);
  Name = AName;
  Next = Fonts;
  Fonts = this;

  VTexture *Tex = GTextureManager[TexNum];
  for (int i = 0; i < 128; ++i) AsciiChars[i] = -1;
  AsciiChars[(int)'A'] = 0;
  FirstChar = 'A';
  LastChar = 'A';
  SpaceWidth = Tex->GetScaledWidth();
  FontHeight = Tex->GetScaledHeight();
  Kerning = 0;
  Translation = nullptr;

  FFontChar &FChar = Chars.Alloc();
  FChar.Char = 'A';
  FChar.TexNum = TexNum;
  FChar.BaseTex = Tex;
  FChar.Textures = nullptr;
  unguard;
}


// ////////////////////////////////////////////////////////////////////////// //

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
  Format = TEXFMT_8Pal;
  Name = NAME_None;
  Width = BaseTex->GetWidth();
  Height = BaseTex->GetHeight();
  SOffset = BaseTex->SOffset;
  TOffset = BaseTex->TOffset;
  SScale = BaseTex->SScale;
  TScale = BaseTex->TScale;
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
  guard(VFontChar::GetPixels);
  return BaseTex->GetPixels8();
  unguard;
}


//==========================================================================
//
//  VFontChar::GetPalette
//
//==========================================================================
rgba_t *VFontChar::GetPalette () {
  guard(VFontChar::GetPalette);
  return Palette;
  unguard;
}


//==========================================================================
//
//  VFontChar::Unload
//
//==========================================================================
void VFontChar::Unload () {
  guard(VFontChar::Unload);
  BaseTex->Unload();
  unguard;
}


//==========================================================================
//
//  VFontChar::GetHighResolutionTexture
//
//==========================================================================
VTexture *VFontChar::GetHighResolutionTexture () {
  guard(VFontChar::GetHighResolutionTexture);
  if (!r_hirestex) return nullptr;
  if (!HiResTexture) {
    VTexture *Tex = BaseTex->GetHighResolutionTexture();
    if (Tex) HiResTexture = new VFontChar(Tex, Palette);
  }
  return HiResTexture;
  unguard;
}


// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  VFontChar2::VFontChar2
//
//==========================================================================
VFontChar2::VFontChar2 (int ALumpNum, int AFilePos, int CharW, int CharH, rgba_t *APalette, int AMaxCol)
  : LumpNum(ALumpNum)
  , FilePos(AFilePos)
  , Pixels(nullptr)
  , Palette(APalette)
  , MaxCol(AMaxCol)
{
  Type = TEXTYPE_FontChar;
  Format = TEXFMT_8Pal;
  Name = NAME_None;
  Width = CharW;
  Height = CharH;
}


//==========================================================================
//
//  VFontChar2::~VFontChar2
//
//==========================================================================
VFontChar2::~VFontChar2 () {
  if (Pixels) {
    delete[] Pixels;
    Pixels = nullptr;
  }
}


//==========================================================================
//
//  VFontChar2::GetPixels
//
//==========================================================================
vuint8 *VFontChar2::GetPixels () {
  guard(VFontChar2::GetPixels);
  if (Pixels) return Pixels;

  VStream *Strm = W_CreateLumpReaderNum(LumpNum);
  Strm->Seek(FilePos);

  int Count = Width*Height;
  Pixels = new vuint8[Count];
  vuint8 *pDst = Pixels;
  do {
    vint32 Code = Streamer<vint8>(*Strm);
    if (Code >= 0) {
      Count -= Code+1;
      while (Code-- >= 0) {
        *pDst = Streamer<vuint8>(*Strm);
        *pDst = MIN(*pDst, MaxCol);
        ++pDst;
      }
    } else if (Code != -128) {
      Code = 1-Code;
      Count -= Code;
      vuint8 Val = Streamer<vuint8>(*Strm);
      Val = MIN(Val, MaxCol);
      while (Code-- > 0) *pDst++ = Val;
    }
  } while (Count > 0);

  delete Strm;
  return Pixels;
  unguard;
}


//==========================================================================
//
//  VFontChar2::GetPalette
//
//==========================================================================
rgba_t *VFontChar2::GetPalette () {
  guard(VFontChar2::GetPalette);
  return Palette;
  unguard;
}


//==========================================================================
//
//  VFontChar2::Unload
//
//==========================================================================
void VFontChar2::Unload () {
  guard(VFontChar2::Unload);
  if (Pixels) {
    delete[] Pixels;
    Pixels = nullptr;
  }
  unguard;
}
