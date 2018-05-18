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
//**
//**  Preparation of data for rendering, generation of lookups.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "gamedefs.h"
#include "r_local.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

struct VTempSpriteEffectDef
{
  VStr              Sprite;
  VStr              Light;
  VStr              Part;
};

struct VTempClassEffects
{
  VStr              ClassName;
  VStr              StaticLight;
  TArray<VTempSpriteEffectDef>  SpriteEffects;
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

//
//  Main palette
//
rgba_t                r_palette[256];
vuint8                r_black_colour;

vuint8                r_rgbtable[32 * 32 * 32 + 4];

//  variables used to look up
// and range check thing_t sprites patches
spritedef_t             sprites[MAX_SPRITE_MODELS];

VTextureTranslation**       TranslationTables;
int                 NumTranslationTables;
VTextureTranslation         IceTranslation;
TArray<VTextureTranslation*>    DecorateTranslations;
TArray<VTextureTranslation*>    BloodTranslations;

//  They basicly work the same as translations.
VTextureTranslation         ColourMaps[CM_Max];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

//  Temporary variables for sprite installing
static spriteframe_t        sprtemp[30];
static int              maxframe;
static const char*          spritename;

static TArray<VLightEffectDef>    GLightEffectDefs;
static TArray<VParticleEffectDef> GParticleEffectDefs;

// CODE --------------------------------------------------------------------

//==========================================================================
//
//  InitPalette
//
//==========================================================================

static void InitPalette()
{
  guard(InitPalette);
  //  We use colour 0 as transparent colour, so we must find an alternate
  // index for black colour. In Doom, Heretic and Strife there is another
  // black colour, in Hexen it's almost black.
  //  I think that originaly Doom uses colour 255 as transparent colour,
  // but utilites created by others uses the alternate black colour and
  // these graphics can contain pixels of colour 255.
  //  Heretic and Hexen also uses colour 255 as transparent, even more - in
  // colourmaps it's maped to colour 0. Posibly this can cause problems
  // with modified graphics.
  //  Strife uses colour 0 as transparent. I already had problems with fact
  // that colour 255 is normal colour, now there shouldn't be any problems.
  VStream* Strm = W_CreateLumpReaderName(NAME_playpal);
  check(Strm);
  rgba_t* pal = r_palette;
  int best_dist = 0x10000;
  for (int i = 0; i < 256; i++)
  {
    *Strm << pal[i].r
      << pal[i].g
      << pal[i].b;
    if (i == 0)
    {
      pal[i].a = 0;
    }
    else
    {
      pal[i].a = 255;
      int dist = pal[i].r * pal[i].r + pal[i].g * pal[i].g +
        pal[i].b * pal[i].b;
      if (dist < best_dist)
      {
        r_black_colour = i;
        best_dist = dist;
      }
    }
  }
  delete Strm;
  Strm = NULL;
  unguard;
}

//==========================================================================
//
//  InitRgbTable
//
//==========================================================================

static void InitRgbTable()
{
  guard(InitRgbTable);
  for (int ir = 0; ir < 32; ir++)
  {
    for (int ig = 0; ig < 32; ig++)
    {
      for (int ib = 0; ib < 32; ib++)
      {
        int r = (int)(ir * 255.0 / 31.0 + 0.5);
        int g = (int)(ig * 255.0 / 31.0 + 0.5);
        int b = (int)(ib * 255.0 / 31.0 + 0.5);
        int best_colour = 0;
        int best_dist = 0x1000000;
        for (int i = 1; i < 256; i++)
        {
          int dist = (r_palette[i].r - r) * (r_palette[i].r - r) +
            (r_palette[i].g - g) * (r_palette[i].g - g) +
            (r_palette[i].b - b) * (r_palette[i].b - b);
          if (dist < best_dist)
          {
            best_colour = i;
            best_dist = dist;
            if (!dist)
              break;
          }
        }
        r_rgbtable[(ir << 10) + (ig << 5) + ib] = best_colour;
      }
    }
  }
  r_rgbtable[32 * 32 * 32] = 0;
  unguard;
}

//==========================================================================
//
//  InitTranslationTables
//
//==========================================================================

static void InitTranslationTables()
{
  guard(InitTranslationTables);
  VStream* Strm = W_CreateLumpReaderName(NAME_translat);
  NumTranslationTables = Strm->TotalSize() / 256;
  TranslationTables = new VTextureTranslation*[NumTranslationTables];
  for (int j = 0; j < NumTranslationTables; j++)
  {
    VTextureTranslation* Trans = new VTextureTranslation;
    TranslationTables[j] = Trans;
    Strm->Serialise(Trans->Table, 256);
    //  Make sure that 0 always maps to 0.
    Trans->Table[0] = 0;
    Trans->Palette[0] = r_palette[0];
    for (int i = 1; i < 256; i++)
    {
      //  Make sure that normal colours doesn't map to colour 0.
      if (Trans->Table[i] == 0)
      {
        Trans->Table[i] = r_black_colour;
      }
      Trans->Palette[i] = r_palette[Trans->Table[i]];
    }
  }
  delete Strm;
  Strm = NULL;

  //  Calculate ice translation.
  IceTranslation.Table[0] = 0;
  IceTranslation.Palette[0] = r_palette[0];
  for (int i = 1; i < 256; i++)
  {
    int r = int(r_palette[i].r * 0.5 + 64 * 0.5);
    int g = int(r_palette[i].g * 0.5 + 64 * 0.5);
    int b = int(r_palette[i].b * 0.5 + 255 * 0.5);
    IceTranslation.Palette[i].r = r;
    IceTranslation.Palette[i].g = g;
    IceTranslation.Palette[i].b = b;
    IceTranslation.Palette[i].a = 255;
    IceTranslation.Table[i] = R_LookupRGB(r, g, b);
  }
  unguard;
}

//==========================================================================
//
//  InitColourMaps
//
//==========================================================================

static void InitColourMaps()
{
  guard(InitColourMaps);
  //  Calculate inverse colourmap.
  VTextureTranslation* T = &ColourMaps[CM_Inverse];
  T->Table[0] = 0;
  T->Palette[0] = r_palette[0];
  for (int i = 1; i < 256; i++)
  {
    int Gray = (r_palette[i].r * 77 + r_palette[i].g * 143 +
      r_palette[i].b * 37) >> 8;
    int Val = 255 - Gray;
    T->Palette[i].r = Val;
    T->Palette[i].g = Val;
    T->Palette[i].b = Val;
    T->Palette[i].a = 255;
    T->Table[i] = R_LookupRGB(Val, Val, Val);
  }

  //  Calculate gold colourmap.
  T = &ColourMaps[CM_Gold];
  T->Table[0] = 0;
  T->Palette[0] = r_palette[0];
  for (int i = 1; i < 256; i++)
  {
    int Gray = (r_palette[i].r * 77 + r_palette[i].g * 143 +
      r_palette[i].b * 37) >> 8;
    T->Palette[i].r = MIN(255, Gray + Gray / 2);
    T->Palette[i].g = Gray;
    T->Palette[i].b = 0;
    T->Palette[i].a = 255;
    T->Table[i] = R_LookupRGB(T->Palette[i].r, T->Palette[i].g,
      T->Palette[i].b);
  }

  //  Calculate red colourmap.
  T = &ColourMaps[CM_Red];
  T->Table[0] = 0;
  T->Palette[0] = r_palette[0];
  for (int i = 1; i < 256; i++)
  {
    int Gray = (r_palette[i].r * 77 + r_palette[i].g * 143 +
      r_palette[i].b * 37) >> 8;
    T->Palette[i].r = MIN(255, Gray + Gray / 2);
    T->Palette[i].g = 0;
    T->Palette[i].b = 0;
    T->Palette[i].a = 255;
    T->Table[i] = R_LookupRGB(T->Palette[i].r, T->Palette[i].g,
      T->Palette[i].b);
  }

  //  Calculate green colourmap.
  T = &ColourMaps[CM_Green];
  T->Table[0] = 0;
  T->Palette[0] = r_palette[0];
  for (int i = 1; i < 256; i++)
  {
    int Gray = (r_palette[i].r * 77 + r_palette[i].g * 143 +
      r_palette[i].b * 37) >> 8;
    T->Palette[i].r = MIN(255, Gray + Gray / 2);
    T->Palette[i].g = MIN(255, Gray + Gray / 2);
    T->Palette[i].b = Gray;
    T->Palette[i].a = 255;
    T->Table[i] = R_LookupRGB(T->Palette[i].r, T->Palette[i].g,
      T->Palette[i].b);
  }
  unguard;
}

//==========================================================================
//
//  InstallSpriteLump
//
//  Local function for R_InitSprites.
//
//==========================================================================

static void InstallSpriteLump(int lumpnr, int frame, char Rot, bool flipped)
{
  guard(InstallSpriteLump);
  int     r;
  int     rotation;

  if (Rot >= '0' && Rot <= '9')
  {
    rotation = Rot - '0';
  }
  else if (Rot >= 'a')
  {
    rotation = Rot - 'a' + 10;
  }
  else
  {
    rotation = 17;
  }

  VTexture* Tex = GTextureManager[lumpnr];
  if ((vuint32)frame >= 30 || (vuint32)rotation > 16)
  {
    Sys_Error("InstallSpriteLump: Bad frame characters in lump %s",
      *Tex->Name);
  }

  if (frame > maxframe)
    maxframe = frame;

  if (rotation == 0)
  {
    // the lump should be used for all rotations
    sprtemp[frame].rotate = false;
    for (r = 0; r < 16; r++)
    {
      sprtemp[frame].lump[r] = lumpnr;
      sprtemp[frame].flip[r] = flipped;
    }
    return;
  }

  if (rotation <= 8)
  {
    rotation = (rotation - 1) * 2;
  }
  else
  {
    rotation = (rotation - 9) * 2 + 1;
  }

  // the lump is only used for one rotation
  if (sprtemp[frame].rotate == false)
  {
    for (r = 0; r < 16; r++)
    {
      sprtemp[frame].lump[r] = -1;
      sprtemp[frame].flip[r] = false;
    }
  }

  sprtemp[frame].rotate = true;
  sprtemp[frame].lump[rotation] = lumpnr;
  sprtemp[frame].flip[rotation] = flipped;
  unguard;
}

//==========================================================================
//
//  R_InstallSprite
//
//  Builds the sprite rotation matrixes to account for horizontally flipped
// sprites. Will report an error if the lumps are inconsistant.
//  Sprite lump names are 4 characters for the actor, a letter for the frame,
// and a number for the rotation. A sprite that is flippable will have an
// additional letter/number appended. The rotation character can be 0 to
// signify no rotations.
//
//==========================================================================

void R_InstallSprite(const char *name, int index)
{
  guard(R_InstallSprite);
  if ((vuint32)index >= MAX_SPRITE_MODELS)
  {
    Host_Error("Invalid sprite index %d for sprite %s", index, name);
  }
  spritename = name;
  memset(sprtemp, -1, sizeof(sprtemp));
  maxframe = -1;

  // scan all the lump names for each of the names,
  //  noting the highest frame letter.
  // Just compare 4 characters as ints
  int intname = *(int*)*VName(spritename, VName::AddLower8);

  // scan the lumps, filling in the frames for whatever is found
  for (int l = 0; l < GTextureManager.GetNumTextures(); l++)
  {
    if (GTextureManager[l]->Type == TEXTYPE_Sprite)
    {
      const char* lumpname = *GTextureManager[l]->Name;
      if (*(int*)lumpname == intname)
      {
        InstallSpriteLump(l, VStr::ToUpper(lumpname[4]) - 'A',
          lumpname[5], false);

        if (lumpname[6])
        {
          InstallSpriteLump(l, VStr::ToUpper(lumpname[6]) - 'A',
            lumpname[7], true);
        }
      }
    }
  }

  // check the frames that were found for completeness
  if (maxframe == -1)
  {
    sprites[index].numframes = 0;
    return;
  }

  maxframe++;

  for (int frame = 0; frame < maxframe; frame++)
  {
    switch ((int)sprtemp[frame].rotate)
    {
    case -1:
      // no rotations were found for that frame at all
      Sys_Error("R_InstallSprite: No patches found "
          "for %s frame %c", spritename, frame + 'A');
      break;

    case 0:
      // only the first rotation is needed
      break;

    case 1:
      //  Copy missing frames for 16-angle rotation.
      for (int rotation = 0; rotation < 8; rotation++)
      {
        if (sprtemp[frame].lump[rotation * 2 + 1] == -1)
        {
          sprtemp[frame].lump[rotation * 2 + 1] =
            sprtemp[frame].lump[rotation * 2];
          sprtemp[frame].flip[rotation * 2 + 1] =
            sprtemp[frame].flip[rotation * 2];
        }
        if (sprtemp[frame].lump[rotation * 2] == -1)
        {
          sprtemp[frame].lump[rotation * 2] =
            sprtemp[frame].lump[rotation * 2 + 1];
          sprtemp[frame].flip[rotation * 2] =
            sprtemp[frame].flip[rotation * 2 + 1];
        }
      }
      // must have all 8 frames
      for (int rotation = 0; rotation < 8; rotation++)
      {
        if (sprtemp[frame].lump[rotation] == -1)
        {
          if (GArgs.CheckParm("-sprloose")) {
            GCon->Logf("R_InstallSprite: Sprite %s frame %c is missing rotations", spritename, frame+'A');
          } else {
            Sys_Error("R_InstallSprite: Sprite %s frame %c is missing rotations", spritename, frame+'A');
          }
        }
      }
      break;
    }
  }

  if (sprites[index].spriteframes)
  {
    Z_Free(sprites[index].spriteframes);
    sprites[index].spriteframes = NULL;
  }
  // allocate space for the frames present and copy sprtemp to it
  sprites[index].numframes = maxframe;
  sprites[index].spriteframes = (spriteframe_t*)
    Z_Malloc(maxframe * sizeof(spriteframe_t));
  memcpy(sprites[index].spriteframes, sprtemp, maxframe * sizeof(spriteframe_t));
  unguard;
}

//==========================================================================
//
//  FreeSpriteData
//
//==========================================================================

static void FreeSpriteData()
{
  guard(FreeSpriteData);
  for (int i = 0; i < MAX_SPRITE_MODELS; i++)
  {
    if (sprites[i].spriteframes)
    {
      Z_Free(sprites[i].spriteframes);
    }
  }
  unguard;
}

//==========================================================================
//
//  R_AreSpritesPresent
//
//==========================================================================

bool R_AreSpritesPresent(int Index)
{
  guardSlow(R_AreSpritesPresent);
  return sprites[Index].numframes > 0;
  unguardSlow;
}

//==========================================================================
//
//  R_InitData
//
//==========================================================================

void R_InitData()
{
  guard(R_InitData);
  //  Load palette.
  InitPalette();

  //  Calculate RGB table.
  InitRgbTable();

  //  Init standard translation tables.
  InitTranslationTables();

  //  Init colour maps.
  InitColourMaps();
  unguard;
}

//==========================================================================
//
//  R_ShutdownData
//
//==========================================================================

void R_ShutdownData()
{
  guard(R_ShutdownData);
  if (TranslationTables)
  {
    for (int i = 0; i < NumTranslationTables; i++)
    {
      delete TranslationTables[i];
      TranslationTables[i] = NULL;
    }
    delete[] TranslationTables;
    TranslationTables = NULL;
  }

  for (int i = 0; i < DecorateTranslations.Num(); i++)
  {
    delete DecorateTranslations[i];
    DecorateTranslations[i] = NULL;
  }
  DecorateTranslations.Clear();

  FreeSpriteData();

  GLightEffectDefs.Clear();
  GParticleEffectDefs.Clear();
  unguard;
}

//==========================================================================
//
//  VTextureTranslation::VTextureTranslation
//
//==========================================================================

VTextureTranslation::VTextureTranslation()
: Crc(0)
, TranslStart(0)
, TranslEnd(0)
, Colour(0)
{
  Clear();
}

//==========================================================================
//
//  VTextureTranslation::Clear
//
//==========================================================================

void VTextureTranslation::Clear()
{
  guard(VTextureTranslation::Clear);
  for (int i = 0; i < 256; i++)
  {
    Table[i] = i;
    Palette[i] = r_palette[i];
  }
  Commands.Clear();
  CalcCrc();
  unguard;
}

//==========================================================================
//
//  VTextureTranslation::CalcCrc
//
//==========================================================================

void VTextureTranslation::CalcCrc()
{
  guard(VTextureTranslation::CalcCrc);
  TCRC Work;
  Work.Init();
  for (int i = 1; i < 256; i++)
  {
    Work + Palette[i].r;
    Work + Palette[i].g;
    Work + Palette[i].b;
  }
  Crc = Work;
  unguard;
}

//==========================================================================
//
//  VTextureTranslation::Serialise
//
//==========================================================================

void VTextureTranslation::Serialise(VStream& Strm)
{
  guard(VTextureTranslation::Serialise);
  Strm.Serialise(Table, 256);
  Strm.Serialise(Palette, sizeof(Palette));
  Strm << Crc
    << TranslStart
    << TranslEnd
    << Colour;
  int CmdsSize = Commands.Num();
  Strm << STRM_INDEX(CmdsSize);
  if (Strm.IsLoading())
  {
    Commands.SetNum(CmdsSize);
  }
  for (int i = 0; i < CmdsSize; i++)
  {
    VTransCmd& C = Commands[i];
    Strm << C.Type << C.Start << C.End << C.R1 << C.G1 << C.B1 <<
      C.R2 << C.G2 << C.B2;
  }
  unguard;
}

//==========================================================================
//
//  VTextureTranslation::BuildPlayerTrans
//
//==========================================================================

void VTextureTranslation::BuildPlayerTrans(int Start, int End, int Col)
{
  guard(VTextureTranslation::BuildPlayerTrans);
  int Count = End - Start + 1;
  vuint8 r = (Col >> 16) & 255;
  vuint8 g = (Col >> 8) & 255;
  vuint8 b = Col & 255;
  vuint8 h, s, v;
  M_RgbToHsv(r, g, b, h, s, v);
  for (int i = 0; i < Count; i++)
  {
    int Idx = Start + i;
    vuint8 TmpH, TmpS, TmpV;
    M_RgbToHsv(Palette[Idx].r, Palette[Idx].g,Palette[Idx].b, TmpH, TmpS, TmpV);
    M_HsvToRgb(h, s, v * TmpV / 255, Palette[Idx].r,
      Palette[Idx].g, Palette[Idx].b);
    Table[Idx] = R_LookupRGB(Palette[Idx].r, Palette[Idx].g,
      Palette[Idx].b);
  }
  CalcCrc();
  TranslStart = Start;
  TranslEnd = End;
  Colour = Col;
  unguard;
}

//==========================================================================
//
//  VTextureTranslation::MapToRange
//
//==========================================================================

void VTextureTranslation::MapToRange(int AStart, int AEnd, int ASrcStart,
  int ASrcEnd)
{
  guard(VTextureTranslation::MapToRange);
  int Start;
  int End;
  int SrcStart;
  int SrcEnd;
  //  Swap range if necesary.
  if (AStart > AEnd)
  {
    Start = AEnd;
    End = AStart;
    SrcStart = ASrcEnd;
    SrcEnd = ASrcStart;
  }
  else
  {
    Start = AStart;
    End = AEnd;
    SrcStart = ASrcStart;
    SrcEnd = ASrcEnd;
  }
  //  Check for single colour change.
  if (Start == End)
  {
    Table[Start] = SrcStart;
    Palette[Start] = r_palette[SrcStart];
    return;
  }
  float CurCol = SrcStart;
  float ColStep = (float(SrcEnd) - float(SrcStart)) /
    (float(End) - float(Start));
  for (int i = Start; i < End; i++, CurCol += ColStep)
  {
    Table[i] = int(CurCol);
    Palette[i] = r_palette[Table[i]];
  }
  VTransCmd& C = Commands.Alloc();
  C.Type = 0;
  C.Start = Start;
  C.End = End;
  C.R1 = SrcStart;
  C.R2 = SrcEnd;
  CalcCrc();
  unguard;
}

//==========================================================================
//
//  VTextureTranslation::MapToColours
//
//==========================================================================

void VTextureTranslation::MapToColours(int AStart, int AEnd, int AR1, int AG1,
  int AB1, int AR2, int AG2, int AB2)
{
  guard(VTextureTranslation::MapToColours);
  int Start;
  int End;
  int R1;
  int G1;
  int B1;
  int R2;
  int G2;
  int B2;
  //  Swap range if necesary.
  if (AStart > AEnd)
  {
    Start = AEnd;
    End = AStart;
    R1 = AR2;
    G1 = AG2;
    B1 = AB2;
    R2 = AR1;
    G2 = AG1;
    B2 = AB1;
  }
  else
  {
    Start = AStart;
    End = AEnd;
    R1 = AR1;
    G1 = AG1;
    B1 = AB1;
    R2 = AR2;
    G2 = AG2;
    B2 = AB2;
  }
  //  Check for single colour change.
  if (Start == End)
  {
    Palette[Start].r = R1;
    Palette[Start].g = G1;
    Palette[Start].b = B1;
    Table[Start] = R_LookupRGB(R1, G1, B1);
    return;
  }
  float CurR = R1;
  float CurG = G1;
  float CurB = B1;
  float RStep = (float(R2) - float(R1)) / (float(End) - float(Start));
  float GStep = (float(G2) - float(G1)) / (float(End) - float(Start));
  float BStep = (float(B2) - float(B1)) / (float(End) - float(Start));
  for (int i = Start; i < End; i++, CurR += RStep, CurG += GStep,
    CurB += BStep)
  {
    Palette[i].r = int(CurR);
    Palette[i].g = int(CurG);
    Palette[i].b = int(CurB);
    Table[i] = R_LookupRGB(Palette[i].r, Palette[i].g, Palette[i].b);
  }
  VTransCmd& C = Commands.Alloc();
  C.Type = 1;
  C.Start = Start;
  C.End = End;
  C.R1 = R1;
  C.G1 = G1;
  C.B1 = B1;
  C.R2 = R2;
  C.G2 = G2;
  C.B2 = B2;
  CalcCrc();
  unguard;
}

//==========================================================================
//
//  VTextureTranslation::BuildBloodTrans
//
//==========================================================================

void VTextureTranslation::BuildBloodTrans(int Col)
{
  guard(VTextureTranslation::BuildBloodTrans);
  vuint8 r = (Col >> 16) & 255;
  vuint8 g = (Col >> 8) & 255;
  vuint8 b = Col & 255;
  //  Don't remap colour 0.
  for (int i = 1; i < 256; i++)
  {
    int Bright = MAX(MAX(r_palette[i].r, r_palette[i].g), r_palette[i].b);
    Palette[i].r = r * Bright / 255;
    Palette[i].g = g * Bright / 255;
    Palette[i].b = b * Bright / 255;
    Table[i] = R_LookupRGB(Palette[i].r, Palette[i].g, Palette[i].b);
  }
  CalcCrc();
  Colour = Col;
  unguard;
}

//==========================================================================
//
//  CheckChar
//
//==========================================================================

static bool CheckChar(char*& pStr, char Chr)
{
  //  Skip whitespace
  while (*pStr && *pStr <= ' ')
  {
    pStr++;
  }
  if (*pStr != Chr)
  {
    return false;
  }
  pStr++;
  return true;
}

//==========================================================================
//
//  VTextureTranslation::AddTransString
//
//==========================================================================

void VTextureTranslation::AddTransString(const VStr& Str)
{
  guard(VTextureTranslation::AddTransString);
  char* pStr = const_cast<char*>(*Str);

  //  Parse start and end of the range
  int Start = strtol(pStr, &pStr, 10);
  if (!CheckChar(pStr, ':'))
  {
    return;
  }
  int End = strtol(pStr, &pStr, 10);
  if (!CheckChar(pStr, '='))
  {
    return;
  }
  if (!CheckChar(pStr, '['))
  {
    int SrcStart = strtol(pStr, &pStr, 10);
    if (!CheckChar(pStr, ':'))
    {
      return;
    }
    int SrcEnd = strtol(pStr, &pStr, 10);
    MapToRange(Start, End, SrcStart, SrcEnd);
  }
  else
  {
    int R1 = strtol(pStr, &pStr, 10);
    if (!CheckChar(pStr, ','))
    {
      return;
    }
    int G1 = strtol(pStr, &pStr, 10);
    if (!CheckChar(pStr, ','))
    {
      return;
    }
    int B1 = strtol(pStr, &pStr, 10);
    if (!CheckChar(pStr, ']'))
    {
      return;
    }
    if (!CheckChar(pStr, ':'))
    {
      return;
    }
    if (!CheckChar(pStr, '['))
    {
      return;
    }
    int R2 = strtol(pStr, &pStr, 10);
    if (!CheckChar(pStr, ','))
    {
      return;
    }
    int G2 = strtol(pStr, &pStr, 10);
    if (!CheckChar(pStr, ','))
    {
      return;
    }
    int B2 = strtol(pStr, &pStr, 10);
    if (!CheckChar(pStr, ']'))
    {
      return;
    }
    MapToColours(Start, End, R1, G1, B1, R2, G2, B2);
  }
  unguard;
}

//==========================================================================
//
//  R_ParseDecorateTranslation
//
//==========================================================================

int R_ParseDecorateTranslation(VScriptParser* sc, int GameMax)
{
  guard(R_ParseDecorateTranslation);
  //  First check for standard translation.
  if (sc->CheckNumber())
  {
    if (sc->Number < 0 || sc->Number >= MAX(NumTranslationTables, GameMax))
    {
      sc->Error(va("Translation must be in range [0, %d]",
        MAX(NumTranslationTables, GameMax) - 1));
    }
    return (TRANSL_Standard << TRANSL_TYPE_SHIFT) + sc->Number;
  }

  //  Check for special ice translation
  if (sc->Check("Ice"))
  {
    return (TRANSL_Standard << TRANSL_TYPE_SHIFT) + 7;
  }

  VTextureTranslation* Tr = new VTextureTranslation;

  do
  {
    sc->ExpectString();
    Tr->AddTransString(sc->String);
  }
  while (sc->Check(","));

  //  See if we already have this translation.
  for (int i = 0; i < DecorateTranslations.Num(); i++)
  {
    if (DecorateTranslations[i]->Crc != Tr->Crc)
    {
      continue;
    }
    if (memcmp(DecorateTranslations[i]->Palette, Tr->Palette,
      sizeof(Tr->Palette)))
    {
      continue;
    }
    //  Found a match.
    delete Tr;
    Tr = NULL;
    return (TRANSL_Decorate << TRANSL_TYPE_SHIFT) + i;
  }

  //  Add it.
  if (DecorateTranslations.Num() >= MAX_DECORATE_TRANSLATIONS)
  {
    sc->Error("Too many translations in DECORATE scripts");
  }
  DecorateTranslations.Append(Tr);
  return (TRANSL_Decorate << TRANSL_TYPE_SHIFT) +
    DecorateTranslations.Num() - 1;
  unguard;
}

//==========================================================================
//
//  R_GetBloodTranslation
//
//==========================================================================

int R_GetBloodTranslation(int Col)
{
  guard(R_GetBloodTranslation);
  //  Check for duplicate blood translation.
  for (int i = 0; i < BloodTranslations.Num(); i++)
  {
    if (BloodTranslations[i]->Colour == Col)
    {
      return (TRANSL_Blood << TRANSL_TYPE_SHIFT) + i;
    }
  }

  //  Create new translation.
  VTextureTranslation* Tr = new VTextureTranslation;
  Tr->BuildBloodTrans(Col);

  //  Add it.
  if (BloodTranslations.Num() >= MAX_BLOOD_TRANSLATIONS)
  {
    Sys_Error("Too many blood colours in DECORATE scripts");
  }
  BloodTranslations.Append(Tr);
  return (TRANSL_Blood << TRANSL_TYPE_SHIFT) +
    BloodTranslations.Num() - 1;
  unguard;
}

//==========================================================================
//
//  FindLightEffect
//
//==========================================================================

static VLightEffectDef* FindLightEffect(const VStr& Name)
{
  guard(FindLightEffect);
  for (int i = 0; i < GLightEffectDefs.Num(); i++)
  {
    if (GLightEffectDefs[i].Name == *Name)
    {
      return &GLightEffectDefs[i];
    }
  }
  return NULL;
  unguard;
}

//==========================================================================
//
//  ParseLightDef
//
//==========================================================================

static void ParseLightDef(VScriptParser* sc, int LightType)
{
  guard(ParseLightDef);
  //  Get name, find it in the list or add it if it's not there yet.
  sc->ExpectString();
  VLightEffectDef* L = FindLightEffect(sc->String);
  if (!L)
  {
    L = &GLightEffectDefs.Alloc();
  }

  //  Set default values.
  L->Name = *sc->String.ToLower();
  L->Type = LightType;
  L->Colour = 0xffffffff;
  L->Radius = 0.0;
  L->Radius2 = 0.0;
  L->MinLight = 0.0;
  L->Offset = TVec(0, 0, 0);
  L->Chance = 0.0;
  L->Interval = 0.0;
  L->Scale = 0.0;

  //  Parse light def.
  sc->Expect("{");
  while (!sc->Check("}"))
  {
    if (sc->Check("colour"))
    {
      sc->ExpectFloat();
      float r = MID(0, sc->Float, 1);
      sc->ExpectFloat();
      float g = MID(0, sc->Float, 1);
      sc->ExpectFloat();
      float b = MID(0, sc->Float, 1);
      L->Colour = ((int)(r * 255) << 16) | ((int)(g * 255) << 8) |
        (int)(b * 255) | 0xff000000;
    }
    else if (sc->Check("radius"))
    {
      sc->ExpectFloat();
      L->Radius = sc->Float;
    }
    else if (sc->Check("radius2"))
    {
      sc->ExpectFloat();
      L->Radius2 = sc->Float;
    }
    else if (sc->Check("minlight"))
    {
      sc->ExpectFloat();
      L->MinLight = sc->Float;
    }
    else if (sc->Check("offset"))
    {
      sc->ExpectFloat();
      L->Offset.x = sc->Float;
      sc->ExpectFloat();
      L->Offset.y = sc->Float;
      sc->ExpectFloat();
      L->Offset.z = sc->Float;
    }
    else
    {
      sc->Error("Bad point light parameter");
    }
  }
  unguard;
}

//==========================================================================
//
//  IntensityToRadius
//
//==========================================================================

float IntensityToRadius(float Val)
{
  if (Val <= 20.0)
  {
    return Val * 4.5;
  }
  if (Val <= 30.0)
  {
    return Val * 3.6;
  }
  if (Val <= 40.0)
  {
    return Val * 3.3;
  }
  if (Val <= 60.0)
  {
    return Val * 2.8;
  }
  return Val * 2.5;
}

//==========================================================================
//
//  ParseGZLightDef
//
//==========================================================================

static void ParseGZLightDef(VScriptParser* sc, int LightType)
{
  guard(ParseGZLightDef);
  //  Get name, find it in the list or add it if it's not there yet.
  sc->ExpectString();
  VLightEffectDef* L = FindLightEffect(sc->String);
  if (!L)
  {
    L = &GLightEffectDefs.Alloc();
  }

  //  Set default values.
  L->Name = *sc->String.ToLower();
  L->Type = LightType;
  L->Colour = 0xffffffff;
  L->Radius = 0.0;
  L->Radius2 = 0.0;
  L->MinLight = 0.0;
  L->Offset = TVec(0, 0, 0);
  L->Chance = 0.0;
  L->Interval = 0.0;
  L->Scale = 0.0;

  //  Parse light def.
  sc->Expect("{");
  while (!sc->Check("}"))
  {
    if (sc->Check("color"))
    {
      sc->ExpectFloat();
      float r = MID(0, sc->Float, 1);
      sc->ExpectFloat();
      float g = MID(0, sc->Float, 1);
      sc->ExpectFloat();
      float b = MID(0, sc->Float, 1);
      L->Colour = ((int)(r * 255) << 16) | ((int)(g * 255) << 8) |
        (int)(b * 255) | 0xff000000;
    }
    else if (sc->Check("size"))
    {
      sc->ExpectNumber();
      L->Radius = IntensityToRadius(float(sc->Number));
    }
    else if (sc->Check("secondarySize"))
    {
      sc->ExpectNumber();
      L->Radius2 = IntensityToRadius(float(sc->Number));
    }
    else if (sc->Check("offset"))
    {
      // GZDoom manages Z offset as Y offset...
      sc->ExpectNumber();
      L->Offset.x = float(sc->Number);
      sc->ExpectNumber();
      L->Offset.z = float(sc->Number);
      sc->ExpectNumber();
      L->Offset.y = float(sc->Number);
    }
    else if (sc->Check("subtractive"))
    {
      sc->ExpectNumber();
      sc->Message("Subtractive lights not supported.");
    }
    else if (sc->Check("chance"))
    {
      sc->ExpectFloat();
      L->Chance = sc->Float;
    }
    else if (sc->Check("scale"))
    {
      sc->ExpectFloat();
      L->Scale = sc->Float;
    }
    else if (sc->Check("interval"))
    {
      sc->ExpectFloat();
      L->Interval = sc->Float;
    }
    else if (sc->Check("additive"))
    {
      sc->ExpectNumber();
      sc->Message("Additive parameter not supported.");
    }
    else if (sc->Check("halo"))
    {
      sc->ExpectNumber();
      sc->Message("Halo parameter not supported.");
    }
    else if (sc->Check("dontlightself"))
    {
      sc->ExpectNumber();
      sc->Message("DontLightSelf parameter not supported.");
    }
    else
    {
      sc->Error("Bad point light parameter");
    }
  }
  unguard;
}

//==========================================================================
//
//  FindParticleEffect
//
//==========================================================================

static VParticleEffectDef* FindParticleEffect(const VStr& Name)
{
  guard(FindParticleEffect);
  for (int i = 0; i < GParticleEffectDefs.Num(); i++)
  {
    if (GParticleEffectDefs[i].Name == *Name)
    {
      return &GParticleEffectDefs[i];
    }
  }
  return NULL;
  unguard;
}

//==========================================================================
//
//  ParseParticleEffect
//
//==========================================================================

static void ParseParticleEffect(VScriptParser* sc)
{
  guard(ParseParticleEffect);
  //  Get name, find it in the list or add it if it's not there yet.
  sc->ExpectString();
  VParticleEffectDef* P = FindParticleEffect(sc->String);
  if (!P)
  {
    P = &GParticleEffectDefs.Alloc();
  }

  //  Set default values.
  P->Name = *sc->String.ToLower();
  P->Type = 0;
  P->Type2 = 0;
  P->Colour = 0xffffffff;
  P->Offset = TVec(0, 0, 0);
  P->Count = 0;
  P->OrgRnd = 0;
  P->Velocity = TVec(0, 0, 0);
  P->VelRnd = 0;
  P->Accel = 0;
  P->Grav = 0;
  P->Duration = 0;
  P->Ramp = 0;

  //  Parse light def.
  sc->Expect("{");
  while (!sc->Check("}"))
  {
    if (sc->Check("type"))
    {
      if (sc->Check("static"))
      {
        P->Type = 0;
      }
      else if (sc->Check("explode"))
      {
        P->Type = 1;
      }
      else if (sc->Check("explode2"))
      {
        P->Type = 2;
      }
      else
      {
        sc->Error("Bad type");
      }
    }
    else if (sc->Check("type2"))
    {
      if (sc->Check("static"))
      {
        P->Type2 = 0;
      }
      else if (sc->Check("explode"))
      {
        P->Type2 = 1;
      }
      else if (sc->Check("explode2"))
      {
        P->Type2 = 2;
      }
      else
      {
        sc->Error("Bad type");
      }
    }
    else if (sc->Check("colour"))
    {
      sc->ExpectFloat();
      float r = MID(0, sc->Float, 1);
      sc->ExpectFloat();
      float g = MID(0, sc->Float, 1);
      sc->ExpectFloat();
      float b = MID(0, sc->Float, 1);
      P->Colour = ((int)(r * 255) << 16) | ((int)(g * 255) << 8) |
        (int)(b * 255) | 0xff000000;
    }
    else if (sc->Check("offset"))
    {
      sc->ExpectFloat();
      P->Offset.x = sc->Float;
      sc->ExpectFloat();
      P->Offset.y = sc->Float;
      sc->ExpectFloat();
      P->Offset.z = sc->Float;
    }
    else if (sc->Check("count"))
    {
      sc->ExpectNumber();
      P->Count = sc->Number;
    }
    else if (sc->Check("originrandom"))
    {
      sc->ExpectFloat();
      P->OrgRnd = sc->Float;
    }
    else if (sc->Check("velocity"))
    {
      sc->ExpectFloat();
      P->Velocity.x = sc->Float;
      sc->ExpectFloat();
      P->Velocity.y = sc->Float;
      sc->ExpectFloat();
      P->Velocity.z = sc->Float;
    }
    else if (sc->Check("velocityrandom"))
    {
      sc->ExpectFloat();
      P->VelRnd = sc->Float;
    }
    else if (sc->Check("acceleration"))
    {
      sc->ExpectFloat();
      P->Accel = sc->Float;
    }
    else if (sc->Check("gravity"))
    {
      sc->ExpectFloat();
      P->Grav = sc->Float;
    }
    else if (sc->Check("duration"))
    {
      sc->ExpectFloat();
      P->Duration = sc->Float;
    }
    else if (sc->Check("ramp"))
    {
      sc->ExpectFloat();
      P->Ramp = sc->Float;
    }
    else
    {
      sc->Error("Bad point light parameter");
    }
  }
  unguard;
}

//==========================================================================
//
//  ParseClassEffects
//
//==========================================================================

static void ParseClassEffects(VScriptParser* sc,
  TArray<VTempClassEffects>& ClassDefs)
{
  guard(ParseClassEffects);
  //  Get name, find it in the list or add it if it's not there yet.
  sc->ExpectString();
  VTempClassEffects* C = NULL;
  for (int i = 0; i < ClassDefs.Num(); i++)
  {
    if (ClassDefs[i].ClassName == sc->String)
    {
      C = &ClassDefs[i];
      break;
    }
  }
  if (!C)
  {
    C = &ClassDefs.Alloc();
  }

  //  Set defaults.
  C->ClassName = sc->String;
  C->StaticLight.Clean();
  C->SpriteEffects.Clear();

  //  Parse
  sc->Expect("{");
  while (!sc->Check("}"))
  {
    if (sc->Check("frame"))
    {
      sc->ExpectString();
      VTempSpriteEffectDef& S = C->SpriteEffects.Alloc();
      S.Sprite = sc->String;
      sc->Expect("{");
      while (!sc->Check("}"))
      {
        if (sc->Check("light"))
        {
          sc->ExpectString();
          S.Light = sc->String.ToLower();
        }
        else if (sc->Check("particles"))
        {
          sc->ExpectString();
          S.Part = sc->String.ToLower();
        }
        else
        {
          sc->Error("Bad frame parameter");
        }
      }
    }
    else if (sc->Check("static_light"))
    {
      sc->ExpectString();
      C->StaticLight = sc->String.ToLower();
    }
    else
    {
      sc->Error("Bad class parameter");
    }
  }
  unguard;
}

//==========================================================================
//
//  ParseEffectDefs
//
//==========================================================================

static void ParseEffectDefs(VScriptParser* sc,
  TArray<VTempClassEffects>& ClassDefs)
{
  guard(ParseEffectDefs);
  while (!sc->AtEnd())
  {
    if (sc->Check("#include"))
    {
      sc->ExpectString();
      int Lump = W_CheckNumForFileName(sc->String);
      //  Check WAD lump only if it's no longer than 8 characters and
      // has no path separator.
      if (Lump < 0 && sc->String.Length() <= 8 &&
        sc->String.IndexOf('/') < 0)
      {
        Lump = W_CheckNumForName(VName(*sc->String, VName::AddLower8));
      }
      if (Lump < 0)
      {
        sc->Error(va("Lump %s not found", *sc->String));
      }
      ParseEffectDefs(new VScriptParser(sc->String,
        W_CreateLumpReaderNum(Lump)), ClassDefs);
      continue;
    }
    else if (sc->Check("pointlight"))
    {
      ParseLightDef(sc, DLTYPE_Point);
    }
    else if (sc->Check("muzzleflashlight"))
    {
      ParseLightDef(sc, DLTYPE_MuzzleFlash);
    }
    else if (sc->Check("particleeffect"))
    {
      ParseParticleEffect(sc);
    }
    else if (sc->Check("class"))
    {
      ParseClassEffects(sc, ClassDefs);
    }
    else if (sc->Check("clear"))
    {
      ClassDefs.Clear();
    }
    else if (sc->Check("pwad"))
    {
      sc->Message("Tried to parse a WAD file, aborting parsing...");
      break;
    }
    else
    {
      sc->Error(va("Unknown command %s", *sc->String));
    }
  }
  delete sc;
  sc = NULL;
  unguard;
}

//==========================================================================
//
//  ParseGZDoomEffectDefs
//
//==========================================================================

static void ParseGZDoomEffectDefs(VScriptParser* sc,
  TArray<VTempClassEffects>& ClassDefs)
{
  guard(ParseEffectDefs);
  while (!sc->AtEnd())
  {
    if (sc->Check("#include"))
    {
      sc->ExpectString();
      int Lump = W_CheckNumForFileName(sc->String);
      //  Check WAD lump only if it's no longer than 8 characters and
      // has no path separator.
      if (Lump < 0 && sc->String.Length() <= 8 &&
        sc->String.IndexOf('/') < 0)
      {
        Lump = W_CheckNumForName(VName(*sc->String, VName::AddLower8));
      }
      if (Lump < 0)
      {
        sc->Error(va("Lump %s not found", *sc->String));
      }
      ParseGZDoomEffectDefs(new VScriptParser(sc->String,
        W_CreateLumpReaderNum(Lump)), ClassDefs);
      continue;
    }
    else if (sc->Check("pointlight"))
    {
      ParseGZLightDef(sc, DLTYPE_Point);
    }
    else if (sc->Check("pulselight"))
    {
      ParseGZLightDef(sc, DLTYPE_Pulse);
    }
    else if (sc->Check("flickerlight"))
    {
      ParseGZLightDef(sc, DLTYPE_Flicker);
    }
    else if (sc->Check("flickerlight2"))
    {
      ParseGZLightDef(sc, DLTYPE_FlickerRandom);
    }
    else if (sc->Check("sectorlight"))
    {
      ParseGZLightDef(sc, DLTYPE_Sector);
    }
    else if (sc->Check("object"))
    {
      ParseClassEffects(sc, ClassDefs);
    }
    else if (sc->Check("skybox"))
    {
      sc->GetString();
      sc->Expect("{");
      while (!sc->Check("}"))
      {
        sc->GetString();
      }
      sc->Message("Skybox parsing isn't implemented yet.");
    }
    else if (sc->Check("brightmap"))
    {
      sc->Message("Brightmaps are not supported.");
    }
    else if (sc->Check("glow"))
    {
      sc->Message("Glowing textures aren't supported yet.");
    }
    else if (sc->Check("hardwareshader"))
    {
      sc->Message("Shaders are not supported");
    }
    else if (sc->Check("pwad"))
    {
      sc->Message("Tried to parse a WAD file, aborting parsing...");
      break;
    }
    else
    {
      sc->Error(va("Unknown command %s", *sc->String));
    }
  }
  delete sc;
  sc = NULL;
  unguard;
}

//==========================================================================
//
//  SetClassFieldInt
//
//==========================================================================

static void SetClassFieldInt(VClass* Class, const char* FieldName,
  int Value, int Idx = 0)
{
  guard(SetClassFieldInt);
  VField* F = Class->FindFieldChecked(FieldName);
  vint32* Ptr = (vint32*)(Class->Defaults + F->Ofs);
  Ptr[Idx] = Value;
  unguard;
}

//==========================================================================
//
//  SetClassFieldBool
//
//==========================================================================

static void SetClassFieldBool(VClass* Class, const char* FieldName, int Value)
{
  guard(SetClassFieldBool);
  VField* F = Class->FindFieldChecked(FieldName);
  vuint32* Ptr = (vuint32*)(Class->Defaults + F->Ofs);
  if (Value)
    *Ptr |= F->Type.BitMask;
  else
    *Ptr &= ~F->Type.BitMask;
  unguard;
}

//==========================================================================
//
//  SetClassFieldFloat
//
//==========================================================================

static void SetClassFieldFloat(VClass* Class, const char* FieldName,
  float Value)
{
  guard(SetClassFieldFloat);
  VField* F = Class->FindFieldChecked(FieldName);
  float* Ptr = (float*)(Class->Defaults + F->Ofs);
  *Ptr = Value;
  unguard;
}

//==========================================================================
//
//  SetClassFieldVec
//
//==========================================================================

static void SetClassFieldVec(VClass* Class, const char* FieldName,
  const TVec& Value)
{
  guard(SetClassFieldVec);
  VField* F = Class->FindFieldChecked(FieldName);
  TVec* Ptr = (TVec*)(Class->Defaults + F->Ofs);
  *Ptr = Value;
  unguard;
}

//==========================================================================
//
//  R_ParseEffectDefs
//
//==========================================================================

void R_ParseEffectDefs()
{
  guard(R_ParseEffectDefs);
  GCon->Log(NAME_Init, "Parsing effect defs");

  TArray<VTempClassEffects> ClassDefs;

  //  Parse VFXDEFS, GLDEFS, etc. scripts.
  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0;
    Lump = W_IterateNS(Lump, WADNS_Global))
  {
    if (W_LumpName(Lump) == NAME_vfxdefs)
    {
      ParseEffectDefs(new VScriptParser(*W_LumpName(Lump),
        W_CreateLumpReaderNum(Lump)), ClassDefs);
    }
    if (W_LumpName(Lump) == NAME_gldefs ||
      W_LumpName(Lump) == NAME_doomdefs || W_LumpName(Lump) == NAME_hticdefs ||
      W_LumpName(Lump) == NAME_hexndefs || W_LumpName(Lump) == NAME_strfdefs)
    {
      ParseGZDoomEffectDefs(new VScriptParser(*W_LumpName(Lump),
        W_CreateLumpReaderNum(Lump)), ClassDefs);
    }
  }

  //  Add effects to the classes.
  for (int i = 0; i < ClassDefs.Num(); i++)
  {
    VTempClassEffects& CD = ClassDefs[i];
    VClass* Cls = VClass::FindClass(*CD.ClassName);
    if (Cls)
    {
      // Get class replacement
      Cls = Cls->GetReplacement();
    }
    else
    {
      GCon->Logf(NAME_Init, "No such class %s", *CD.ClassName);
      continue;
    }

    if (CD.StaticLight.IsNotEmpty())
    {
      VLightEffectDef* SLight = FindLightEffect(CD.StaticLight);
      if (SLight)
      {
        SetClassFieldBool(Cls, "bStaticLight", true);
        SetClassFieldInt(Cls, "LightColour", SLight->Colour);
        SetClassFieldFloat(Cls, "LightRadius", SLight->Radius);
        SetClassFieldVec(Cls, "LightOffset", SLight->Offset);
      }
      else
      {
        GCon->Logf("Light \"%s\" not found", *CD.StaticLight);
      }
    }

    for (int j = 0; j < CD.SpriteEffects.Num(); j++)
    {
      VTempSpriteEffectDef& SprDef = CD.SpriteEffects[j];
      //  Sprite name must be either 4 or 5 chars.
      if (SprDef.Sprite.Length() != 4 && SprDef.Sprite.Length() != 5)
      {
        GCon->Logf(NAME_Init, "Bad sprite name length");
        continue;
      }

      //  Find sprite index.
      char SprName[8];
      SprName[0] = VStr::ToLower(SprDef.Sprite[0]);
      SprName[1] = VStr::ToLower(SprDef.Sprite[1]);
      SprName[2] = VStr::ToLower(SprDef.Sprite[2]);
      SprName[3] = VStr::ToLower(SprDef.Sprite[3]);
      SprName[4] = 0;
      int SprIdx = VClass::FindSprite(SprName, false);
      if (SprIdx == -1)
      {
        GCon->Logf(NAME_Init, "No such sprite %s", SprName);
        continue;
      }

      VSpriteEffect& SprFx = Cls->SpriteEffects.Alloc();
      SprFx.SpriteIndex = SprIdx;
      SprFx.Frame = SprDef.Sprite.Length() == 4 ? -1 :
        (VStr::ToUpper(SprDef.Sprite[4]) - 'A');
      SprFx.LightDef = NULL;
      if (SprDef.Light.IsNotEmpty())
      {
        SprFx.LightDef = FindLightEffect(SprDef.Light);
        if (!SprFx.LightDef)
        {
          GCon->Logf("Light \"%s\" not found", *SprDef.Light);
        }
      }
      SprFx.PartDef = NULL;
      if (SprDef.Part.IsNotEmpty())
      {
        SprFx.PartDef = FindParticleEffect(SprDef.Part);
        if (!SprFx.PartDef)
        {
          GCon->Logf("Particle effect \"%s\" not found", *SprDef.Part);
        }
      }
    }
  }
  unguard;
}
