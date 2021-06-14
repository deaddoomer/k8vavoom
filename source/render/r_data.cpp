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
//**
//**  Preparation of data for rendering, generation of lookups.
//**
//**************************************************************************
#include "../gamedefs.h"
#include "../psim/p_levelinfo.h"
#include "../decorate/vc_decorate.h"
#include "../dehacked/vc_dehacked.h"
#include "../ntvalueioex.h"  /* VCheckedStream */
#include "r_local.h"


extern VCvarB dbg_show_missing_classes;
static VCvarI r_emulate_vga_palette("__r_emulate_vga_palette", "3", "VGA palette emulation mode (0: off).", /*CVAR_Archive*/CVAR_PreInit);
static VCvarI r_color_distance_algo("__r_color_distance_algo", "1", "What algorithm use to calculate color distance?\n  0: standard\n  1: advanced.", /*CVAR_Archive*/CVAR_PreInit);
// there is no sense to store this in config, because config is loaded after brightmaps
static VCvarB x_brightmaps_ignore_iwad("x_brightmaps_ignore_iwad", false, "Ignore \"iwad\" option when *loading* brightmaps?", CVAR_PreInit);

static VCvarB r_glow_ignore_iwad("r_glow_ignore_iwad", false, "Ignore 'iwad' option in glow definitions?", /*CVAR_Archive|*/CVAR_PreInit);


static int cli_WarnBrightmaps = 0;
static int cli_DumpBrightmaps = 0;
static int cli_SprStrict = 0;
static int cli_SprDehTransfer = 0;

/*static*/ bool cliRegister_rdata_args =
  VParsedArgs::RegisterFlagSet("-Wbrightmap", "!show warnings about brightmaps", &cli_WarnBrightmaps) &&
  VParsedArgs::RegisterFlagSet("-sprstrict", "!strict sprite name checking", &cli_SprStrict) &&
  VParsedArgs::RegisterFlagSet("-dump-brightmaps", "!debug brightmap loading", &cli_DumpBrightmaps) &&
  VParsedArgs::RegisterFlagSet("-deh-transfer", "!transfer sprite effects for dehacked sprite replacements", &cli_SprDehTransfer);


struct VTempSpriteEffectDef {
  VStr Sprite;
  VStr Light;
  VStr Part;
};

struct VTempClassEffects {
  VStr ClassName;
  VStr StaticLight;
  TArray<VTempSpriteEffectDef>  SpriteEffects;
};


// main palette
//
rgba_t r_palette[256];
vuint8 r_black_color;
vuint8 r_white_color;

vuint8 r_rgbtable[VAVOOM_COLOR_COMPONENT_MAX*VAVOOM_COLOR_COMPONENT_MAX*VAVOOM_COLOR_COMPONENT_MAX+4];

// variables used to look up
// and range check thing_t sprites patches
//spritedef_t sprites[MAX_SPRITE_MODELS];
TArray<spritedef_t> sprites;

VTextureTranslation **TranslationTables;
int NumTranslationTables;
VTextureTranslation IceTranslation;
TArray<VTextureTranslation *> DecorateTranslations;
TArray<VTextureTranslation *> BloodTranslations;
TMap<VStrCI, int> NamedTranslations;

// key: color&0xffffff, value: index in BloodTranslations
static TMapNC<int, int> BloodTransMap;

// they basicly work the same as translations
VTextureTranslation ColorMaps[CM_Max];

// temporary variables for sprite installing
enum { MAX_SPR_TEMP = 30 };
static spriteframe_t sprtemp[MAX_SPR_TEMP];
static int maxframe;
static char spritenamebuf[32];
static const char *spritename;

static TArray<VLightEffectDef> GLightEffectDefs;
static TArray<VParticleEffectDef> GParticleEffectDefs;

static TMap<VStrCI, int> GLightEffectDefsMap;
static TMap<VStrCI, int> GParticleEffectDefsMap;

static VCvarB spr_report_missing_rotations("spr_report_missing_rotations", false, "Report missing sprite rotations?");
static VCvarB spr_report_missing_patches("spr_report_missing_patches", false, "Report missing sprite patches?");


//==========================================================================
//
//  R_ProcessPalette
//
//  remaps color 0 to the nearest color, emulates VGA if necessary
//  if `forceOpacity` is set, colors [1..255] will be forced to full opacity
//
//  returns index of new color 0
//
//==========================================================================
int R_ProcessPalette (rgba_t pal[256], bool forceOpacity) {
  const int emulateVGA = r_emulate_vga_palette.asInt();
  const bool newDistAlgo = (r_color_distance_algo.asInt() > 0);
  int new0color = 0;
  int best_dist_black = 0x7fffffff;
  int c0r = 0, c0g = 0, c0b = 0;
  for (unsigned i = 0; i < 256; ++i) {
    if (forceOpacity) pal[i].a = 255;
    switch (emulateVGA) {
      case 1:
        // clear two lower bits, to emulate VGA palette
        // do it this way: if either bit 0 or bit 1 is zero, clear two last bits
        // this is so solid colors like white will retain their "whiteness"
        if ((pal[i].r&0x03) != 3) pal[i].r &= 0xfc;
        if ((pal[i].g&0x03) != 3) pal[i].g &= 0xfc;
        if ((pal[i].b&0x03) != 3) pal[i].b &= 0xfc;
        break;
      case 2:
        // if either of lower two bits is set, set them both
        if (pal[i].r&0x03) pal[i].r |= 0x03;
        if (pal[i].g&0x03) pal[i].g |= 0x03;
        if (pal[i].b&0x03) pal[i].b |= 0x03;
        break;
      case 3:
        // remap
        pal[i].r = clampToByte((pal[i].r>>2)*255/63);
        pal[i].g = clampToByte((pal[i].g>>2)*255/63);
        pal[i].b = clampToByte((pal[i].b>>2)*255/63);
        break;
    }
    if (i == 0) {
      //k8: force color 0 to transparent black (it doesn't matter, but anyway)
      //GCon->Logf(NAME_Debug, "color #0 is (%02x_%02x_%02x)", pal[0].r, pal[0].g, pal[0].b);
      c0r = pal[i].r;
      c0g = pal[i].g;
      c0b = pal[i].b;
      if (pal[i].a != 255) {
        c0r = c0r*pal[i].a/255;
        c0g = c0g*pal[i].a/255;
        c0b = c0b*pal[i].a/255;
      }
      pal[i].r = pal[i].g = pal[i].b = 0;
      pal[i].a = 0;
    } else if (best_dist_black) {
      // remap color 0
      int pr = pal[i].r;
      int pg = pal[i].g;
      int pb = pal[i].b;
      if (pal[i].a != 255) {
        pr = pr*pal[i].a/255;
        pg = pg*pal[i].a/255;
        pb = pb*pal[i].a/255;
      }
      int dist;
      if (newDistAlgo) {
        dist = rgbDistanceSquared(pr, pg, pb, c0r, c0g, c0b);
      } else {
        dist = (pr-c0r)*(pr-c0r)+(pg-c0g)*(pg-c0g)+(pb-c0b)*(pb-c0b);
      }
      if (dist < best_dist_black) {
        new0color = (int)i;
        best_dist_black = dist;
      }
    }
  }
  return new0color;
}


//==========================================================================
//
//  InitPalette
//
//==========================================================================
static void InitPalette () {
  if (W_CheckNumForName(NAME_playpal) == -1) {
    Sys_Error("Palette lump not found. Did you forgot to specify path to IWAD file?");
  }
  // We use color 0 as transparent color, so we must find an alternate
  // index for black color. In Doom, Heretic and Strife there is another
  // black color, in Hexen it's almost black.
  // I think that originaly Doom uses color 255 as transparent color,
  // but utilites created by others uses the alternate black color and
  // these graphics can contain pixels of color 255.
  // Heretic and Hexen also uses color 255 as transparent, even more - in
  // colormaps it's maped to color 0. Posibly this can cause problems
  // with modified graphics.
  // Strife uses color 0 as transparent. I already had problems with fact
  // that color 255 is normal color, now there shouldn't be any problems.
  //
  // k8: color 0 is not guaranteed to be black; map it to the actual palette color instead
  VStream *lumpstream = W_CreateLumpReaderName(NAME_playpal);
  VCheckedStream Strm(lumpstream, true); // load to memory
  // load it
  rgba_t *pal = r_palette;
  for (unsigned i = 0; i < 256; ++i) {
    Strm << pal[i].r << pal[i].g << pal[i].b;
  }
  // find white color
  // preprocess, remap color 0
  r_black_color = R_ProcessPalette(pal);
  // find white color
  const bool newDistAlgo = (r_color_distance_algo.asInt() > 0);
  int best_dist_white = (newDistAlgo ? 0x7fffffff : -0x7fffffff);
  for (unsigned i = 1; i < 256; ++i) {
    if (newDistAlgo) {
      const int dist = rgbDistanceSquared(pal[i].r, pal[i].g, pal[i].b, 255, 255, 255);
      if (dist < best_dist_white) {
        r_white_color = (int)i;
        best_dist_white = dist;
      }
    } else {
      const int dist = pal[i].r*pal[i].r+pal[i].g*pal[i].g+pal[i].b*pal[i].b;
      if (dist > best_dist_white) {
        r_white_color = (int)i;
        best_dist_white = dist;
      }
    }
  }
  //GCon->Logf("black=%d:(%02x_%02x_%02x); while=%d:(%02x_%02x_%02x)", r_black_color, pal[r_black_color].r, pal[r_black_color].g, pal[r_black_color].b,
  //  r_white_color, pal[r_white_color].r, pal[r_white_color].g, pal[r_white_color].b);
}


//==========================================================================
//
//  InitRgbTable
//
//==========================================================================
static void InitRgbTable () {
  VStr rtblsize = VStr::size2human((unsigned)sizeof(r_rgbtable));
  if (developer) GCon->Logf(NAME_Dev, "building color translation table (%d bits, %d items per color, %s)", VAVOOM_COLOR_COMPONENT_BITS, VAVOOM_COLOR_COMPONENT_MAX, *rtblsize);
  memset(r_rgbtable, 0, sizeof(r_rgbtable));

  const int algo = r_color_distance_algo.asInt();

  double *ungamma = (double *)Z_Calloc(256*4*sizeof(double));
  for (unsigned i = 0; i < 256; ++i) {
    ungamma[i*4+0] = r_palette[i].r;
    ungamma[i*4+1] = r_palette[i].g;
    ungamma[i*4+2] = r_palette[i].b;
  }

  for (int ir = 0; ir < VAVOOM_COLOR_COMPONENT_MAX; ++ir) {
    for (int ig = 0; ig < VAVOOM_COLOR_COMPONENT_MAX; ++ig) {
      for (int ib = 0; ib < VAVOOM_COLOR_COMPONENT_MAX; ++ib) {
        const int r = (int)(ir*255.0f/((float)(VAVOOM_COLOR_COMPONENT_MAX-1))/*+0.5f*/);
        const int g = (int)(ig*255.0f/((float)(VAVOOM_COLOR_COMPONENT_MAX-1))/*+0.5f*/);
        const int b = (int)(ib*255.0f/((float)(VAVOOM_COLOR_COMPONENT_MAX-1))/*+0.5f*/);
        //GCon->Logf(NAME_Debug, "r=%d; b=%d; g=%d", r, g, b);
        int best_color = -1;
        int best_dist = 0x7fffffff;
        double best_dist_dbl = DBL_MAX;
        const double ur = r;
        const double ug = g;
        const double ub = b;
        for (unsigned i = 1; i < 256; ++i) {
          if (algo == 1) {
            const vint32 dist = rgbDistanceSquared(r_palette[i].r, r_palette[i].g, r_palette[i].b, r, g, b);
            if (best_color < 0 || dist < best_dist) {
              best_color = (int)i;
              best_dist = dist;
              if (!dist) break;
            }
          } else {
            #if 0
            dist = (r_palette[i].r-r)*(r_palette[i].r-r)+
                   (r_palette[i].g-g)*(r_palette[i].g-g)+
                   (r_palette[i].b-b)*(r_palette[i].b-b);
            #else
            const double dr = ungamma[i*4+0]-ur;
            const double dg = ungamma[i*4+1]-ug;
            const double db = ungamma[i*4+2]-ub;
            const double dist = dr*dr+dg*dg+db*db;
            if (best_color < 0 || dist < best_dist_dbl) {
              best_color = (int)i;
              best_dist_dbl = dist;
              if (dist == 0.0f) break;
            }
            #endif
          }
        }
        vassert(best_color > 0 && best_color <= 255);
        r_rgbtable[ir*VAVOOM_COLOR_COMPONENT_MAX*VAVOOM_COLOR_COMPONENT_MAX+ig*VAVOOM_COLOR_COMPONENT_MAX+ib] = best_color;
      }
    }
  }

  Z_Free(ungamma);
}


//==========================================================================
//
//  InitTranslationTables
//
//==========================================================================
static void InitTranslationTables () {
  GCon->Log(NAME_Init, "building texture translations tables");
  {
    int tlump = W_GetNumForName(NAME_translat);
    GCon->Logf(NAME_Init, "using translation table from '%s'", *W_FullLumpName(tlump));
    VStream *lumpstream = W_CreateLumpReaderName(NAME_translat);
    VCheckedStream Strm(lumpstream, true); // load to memory
    NumTranslationTables = Strm.TotalSize()/256;
    TranslationTables = new VTextureTranslation*[NumTranslationTables];
    for (int j = 0; j < NumTranslationTables; ++j) {
      VTextureTranslation *Trans = new VTextureTranslation;
      TranslationTables[j] = Trans;
      Strm.Serialise(Trans->Table, 256);
      // make sure that 0 always maps to 0
      Trans->Table[0] = 0;
      Trans->Palette[0] = r_palette[0];
      for (int i = 1; i < 256; ++i) {
        // make sure that normal colors doesn't map to color 0
        if (Trans->Table[i] == 0) Trans->Table[i] = r_black_color;
        Trans->Palette[i] = r_palette[Trans->Table[i]];
      }
    }
  }

  // calculate ice translation
  IceTranslation.Table[0] = 0;
  IceTranslation.Palette[0] = r_palette[0];
  for (int i = 1; i < 256; ++i) {
    const vuint8 r = clampToByte(int(r_palette[i].r*0.5f+64*0.5f));
    const vuint8 g = clampToByte(int(r_palette[i].g*0.5f+64*0.5f));
    const vuint8 b = clampToByte(int(r_palette[i].b*0.5f+255*0.5f));
    IceTranslation.Palette[i].r = r;
    IceTranslation.Palette[i].g = g;
    IceTranslation.Palette[i].b = b;
    IceTranslation.Palette[i].a = 255;
    IceTranslation.Table[i] = R_LookupRGB(r, g, b);
  }
}


//==========================================================================
//
//  InitColorMaps
//
//==========================================================================
static void InitColorMaps () {
  GCon->Log(NAME_Init, "building colormaps");

  // calculate inverse colormap
  VTextureTranslation *T = &ColorMaps[CM_Inverse];
  T->Table[0] = 0;
  T->Palette[0] = r_palette[0];
  for (int i = 1; i < 256; ++i) {
    const int Gray = clampToByte((r_palette[i].r*77+r_palette[i].g*143+r_palette[i].b*37)>>8);
    const int Val = 255-Gray;
    T->Palette[i].r = Val;
    T->Palette[i].g = Val;
    T->Palette[i].b = Val;
    T->Palette[i].a = 255;
    T->Table[i] = R_LookupRGB(Val, Val, Val);
  }

  // calculate gold colormap
  T = &ColorMaps[CM_Gold];
  T->Table[0] = 0;
  T->Palette[0] = r_palette[0];
  for (int i = 1; i < 256; ++i) {
    const int Gray = clampToByte((r_palette[i].r*77+r_palette[i].g*143+r_palette[i].b*37)>>8);
    T->Palette[i].r = min2(255, Gray+Gray/2);
    T->Palette[i].g = Gray;
    T->Palette[i].b = 0;
    T->Palette[i].a = 255;
    T->Table[i] = R_LookupRGB(T->Palette[i].r, T->Palette[i].g, T->Palette[i].b);
  }

  // calculate red colormap
  T = &ColorMaps[CM_Red];
  T->Table[0] = 0;
  T->Palette[0] = r_palette[0];
  for (int i = 1; i < 256; ++i) {
    const int Gray = clampToByte((r_palette[i].r*77+r_palette[i].g*143+r_palette[i].b*37)>>8);
    T->Palette[i].r = min2(255, Gray+Gray/2);
    T->Palette[i].g = 0;
    T->Palette[i].b = 0;
    T->Palette[i].a = 255;
    T->Table[i] = R_LookupRGB(T->Palette[i].r, T->Palette[i].g, T->Palette[i].b);
  }

  // calculate "berserk red" colormap
  T = &ColorMaps[CM_BeRed];
  T->Table[0] = 0;
  T->Palette[0] = r_palette[0];
  for (int i = 1; i < 256; ++i) {
    const int Gray = clampToByte((r_palette[i].r*77+r_palette[i].g*143+r_palette[i].b*37)>>8);
    T->Palette[i].r = Gray; //min2(255, Gray+Gray/2);
    T->Palette[i].g = Gray/8;
    T->Palette[i].b = Gray/8;
    T->Palette[i].a = 255;
    T->Table[i] = R_LookupRGB(T->Palette[i].r, T->Palette[i].g, T->Palette[i].b);
  }

  // calculate green colormap
  T = &ColorMaps[CM_Green];
  T->Table[0] = 0;
  T->Palette[0] = r_palette[0];
  for (int i = 1; i < 256; ++i) {
    const int Gray = clampToByte((r_palette[i].r*77+r_palette[i].g*143+r_palette[i].b*37)>>8);
    T->Palette[i].r = 0;
    T->Palette[i].g = min2(255, Gray+Gray/2);
    T->Palette[i].b = 0;
    T->Palette[i].a = 255;
    T->Table[i] = R_LookupRGB(T->Palette[i].r, T->Palette[i].g, T->Palette[i].b);
  }

  // calculate blue colormap
  T = &ColorMaps[CM_Blue];
  T->Table[0] = 0;
  T->Palette[0] = r_palette[0];
  for (int i = 1; i < 256; ++i) {
    const int Gray = clampToByte((r_palette[i].r*77+r_palette[i].g*143+r_palette[i].b*37)>>8);
    T->Palette[i].r = Gray/8;
    T->Palette[i].g = Gray/8;
    T->Palette[i].b = min2(255, Gray+Gray/2);
    T->Palette[i].a = 255;
    T->Table[i] = R_LookupRGB(T->Palette[i].r, T->Palette[i].g, T->Palette[i].b);
  }

  // calculate monochrome colormap
  T = &ColorMaps[CM_Mono];
  T->Table[0] = 0;
  T->Palette[0] = r_palette[0];
  for (int i = 1; i < 256; ++i) {
    const int Gray = clampToByte((r_palette[i].r*77+r_palette[i].g*143+r_palette[i].b*37)>>8);
    T->Palette[i].r = min2(255, Gray+Gray/2);
    T->Palette[i].g = min2(255, Gray+Gray/2);
    T->Palette[i].b = Gray;
    T->Palette[i].a = 255;
    T->Table[i] = R_LookupRGB(T->Palette[i].r, T->Palette[i].g, T->Palette[i].b);
  }
}


//==========================================================================
//
//  InstallSpriteLump
//
//  local function for R_InitSprites
//
//==========================================================================
static void InstallSpriteLump (int lumpnr, int frame, char Rot, bool flipped) {
  int rotation;

  //GCon->Logf(NAME_Init, "!!INSTALL_SPRITE_LUMP: <%s> (lumpnr=%d; frame=%d; Rot=%c; flipped=%d)", *GTextureManager[lumpnr]->Name, lumpnr, frame, Rot, (flipped ? 1 : 0));

       if (Rot >= '0' && Rot <= '9') rotation = Rot-'0';
  else if (Rot >= 'a') rotation = Rot-'a'+10;
  else if (Rot >= 'A') rotation = Rot-'A'+10;
  else rotation = 17;

  VTexture *Tex = GTextureManager[lumpnr];
  if ((vuint32)frame >= 30 || (vuint32)rotation > 16) {
    //Sys_Error("InstallSpriteLump: Bad frame characters in lump '%s'", *Tex->Name);
    GCon->Logf(NAME_Error, "InstallSpriteLump: Bad frame characters in lump '%s'", *Tex->Name);
    if ((vuint32)frame < MAX_SPR_TEMP) {
      for (int r = 0; r < 16; ++r) {
        sprtemp[frame].lump[r] = -1;
        sprtemp[frame].flip[r] = false;
      }
    }
    return;
  }

  if (frame > maxframe) maxframe = frame;

  if (rotation == 0) {
    // the lump should be used for all rotations
    sprtemp[frame].rotate = 0;
    for (int r = 0; r < 16; ++r) {
      sprtemp[frame].lump[r] = lumpnr;
      sprtemp[frame].flip[r] = flipped;
    }
    return;
  }

       if (rotation <= 8) rotation = (rotation-1)*2;
  else rotation = (rotation-9)*2+1;

  // the lump is only used for one rotation
  if (sprtemp[frame].rotate == 0) {
    for (int r = 0; r < 16; r++) {
      sprtemp[frame].lump[r] = -1;
      sprtemp[frame].flip[r] = false;
    }
  }

  sprtemp[frame].rotate = 1;
  sprtemp[frame].lump[rotation] = lumpnr;
  sprtemp[frame].flip[rotation] = flipped;
}


struct SpriteTextureIdInfo {
  int texid;
  int next; // next id of texture with this 4-letter prefix, or 0
};

static TArray<SpriteTextureIdInfo> spriteTextures;
static TMap<vuint32, int> spriteTexMap; // key: 4 bytes of a name; value: index in spriteTextures
static int spriteTexturesLengthCheck = -1;


//==========================================================================
//
//  sprprefix2u32
//
//==========================================================================
static vuint32 sprprefix2u32 (const char *name) {
  if (!name || !name[0] || !name[1] || !name[2] || !name[3]) return 0;
  vuint32 res = 0;
  for (int f = 0; f < 4; ++f, ++name) {
    vuint8 ch = (vuint8)name[0];
    if (ch >= 'A' && ch <= 'Z') ch = ch-'A'+'a';
    res = (res<<8)|ch;
  }
  return res;
}


//==========================================================================
//
//  BuildSpriteTexturesList
//
//==========================================================================
static void BuildSpriteTexturesList () {
  if (spriteTexturesLengthCheck == GTextureManager.GetNumTextures()) return;
  spriteTexturesLengthCheck = GTextureManager.GetNumTextures();
  spriteTextures.reset();
  spriteTexMap.reset();

  // scan the lumps, filling in the frames for whatever is found
  for (int l = 0; l < GTextureManager.GetNumTextures(); ++l) {
    if (GTextureManager[l]->Type == TEXTYPE_Sprite) {
      const char *lumpname = *GTextureManager[l]->Name;
      if (!lumpname[0] || !lumpname[1] || !lumpname[2] || !lumpname[3] || !lumpname[4] || !lumpname[5]) continue;
      vuint32 pfx = sprprefix2u32(lumpname);
      if (!pfx) continue;
      // create new record
      int cidx = spriteTextures.length();
      SpriteTextureIdInfo &sti = spriteTextures.alloc();
      sti.texid = l;
      sti.next = 0;
      // append to list
      auto pip = spriteTexMap.find(pfx);
      if (pip) {
        // append to the list
        int last = *pip;
        vassert(last >= 0 && last < spriteTextures.length()-1);
        while (spriteTextures[last].next) last = spriteTextures[last].next;
        vassert(last >= 0 && last < spriteTextures.length()-1);
        vassert(spriteTextures[last].next == 0);
        spriteTextures[last].next = cidx;
      } else {
        // list head
        spriteTexMap.put(pfx, cidx);
      }
    }
  }
}


//==========================================================================
//
//  R_InstallSpriteComplete
//
//==========================================================================
void R_InstallSpriteComplete () {
  spriteTexturesLengthCheck = -1;
  spriteTextures.clear();
  spriteTexMap.clear();
}


//==========================================================================
//
//  R_InstallSprite
//
//  Builds the sprite rotation matrixes to account for horizontally flipped
//  sprites. Will report an error if the lumps are inconsistant.
//
//  Sprite lump names are 4 characters for the actor, a letter for the frame,
//  and a number for the rotation. A sprite that is flippable will have an
//  additional letter/number appended. The rotation character can be 0 to
//  signify no rotations.
//
//==========================================================================
void R_InstallSprite (const char *name, int index) {
  if (index < 0) Host_Error("Invalid sprite index %d for sprite %s", index, name);
  //GCon->Logf(NAME_Debug, "!!INSTALL_SPRITE: <%s> (%d)", name, index);
  if (!name || !name[0]) {
    //Host_Error("Trying to install sprite with an empty name");
    GCon->Logf(NAME_Warning, "Trying to install sprite #%d with an empty name", index);
    return;
  }
  if (strlen(name) > 31) Host_Error("Trying to install sprite #%d with too long name (%s)", index, name);
  strcpy(spritenamebuf, name);
  for (char *sc = spritenamebuf; *sc; ++sc) {
    if (*sc >= 'A' && *sc <= 'Z') *sc = (*sc)-'A'+'a'; // poor man's tolower
  }
  spritename = spritenamebuf;
  //spritename = name;
#if 1
  memset(sprtemp, -1, sizeof(sprtemp));
#else
  for (unsigned idx = 0; idx < MAX_SPR_TEMP; ++idx) {
    sprtemp[idx].rotate = -1;
    for (unsigned c = 0; c < 16; ++c) {
      sprtemp[idx].lump[c] = -1;
      sprtemp[idx].flip[c] = false;
    }
  }
#endif
  maxframe = -1;

  while (index >= sprites.length()) {
    spritedef_t &ss = sprites.alloc();
    ss.numframes = 0;
    ss.spriteframes = nullptr;
  }
  vassert(index < sprites.length());
  sprites[index].numframes = 0;
  if (sprites[index].spriteframes) Z_Free(sprites[index].spriteframes);
  sprites[index].spriteframes = nullptr;

  // scan all the lump names for each of the names, noting the highest frame letter
  // just compare 4 characters as ints
  //int intname = *(int *)*VName(spritename, VName::AddLower8);
  const char *intname = *VName(spritename, VName::AddLower8);
  if (!intname[0] || !intname[1] || !intname[2] || !intname[3]) {
    GCon->Logf(NAME_Warning, "trying to install sprite with invalid name '%s'", intname);
    return;
  }

  // scan the lumps, filling in the frames for whatever is found
  /*
  for (int l = 0; l < GTextureManager.GetNumTextures(); ++l) {
    if (GTextureManager[l]->Type == TEXTYPE_Sprite) {
      const char *lumpname = *GTextureManager[l]->Name;
      if (!lumpname[0] || !lumpname[1] || !lumpname[2] || !lumpname[3] || !lumpname[4] || !lumpname[5]) continue;
      if (memcmp(lumpname, intname, 4) != 0) continue;
      //GCon->Logf("  !!<%s> [4]=%c; [6]=%c; [7]=%c", lumpname, lumpname[4], lumpname[6], (lumpname[6] ? lumpname[7] : 0));
      InstallSpriteLump(l, VStr::ToUpper(lumpname[4])-'A', lumpname[5], false);
      if (lumpname && strlen(lumpname) >= 6 && lumpname[6]) {
        InstallSpriteLump(l, VStr::ToUpper(lumpname[6])-'A', lumpname[7], true);
      }
    }
  }
  */
  vuint32 intpfx = sprprefix2u32(intname);
  if (!intpfx) {
    GCon->Logf(NAME_Warning, "trying to install sprite with invalid name '%s'!", intname);
    return;
  }

  BuildSpriteTexturesList();
  {
    auto pip = spriteTexMap.find(intpfx);
    if (pip) {
      int slidx = *pip;
      do {
        int l = spriteTextures[slidx].texid;
        slidx = spriteTextures[slidx].next;
        vassert(GTextureManager[l]->Type == TEXTYPE_Sprite);
        const char *lumpname = *GTextureManager[l]->Name;
        if (lumpname[0] && lumpname[1] && lumpname[2] && lumpname[3] && lumpname[4] && lumpname[5]) {
          if (memcmp(lumpname, intname, 4) == 0) {
            InstallSpriteLump(l, VStr::ToUpper(lumpname[4])-'A', lumpname[5], false);
            if (lumpname && strlen(lumpname) >= 6 && lumpname[6]) {
              InstallSpriteLump(l, VStr::ToUpper(lumpname[6])-'A', lumpname[7], true);
            }
          }
        }
      } while (slidx != 0);
    }
  }

  // check the frames that were found for completeness
  if (maxframe == -1) return;

  ++maxframe;

  //GCon->Logf(NAME_Init, "sprite '%s', maxframe=%d", intname, maxframe);

  for (int frame = 0; frame < maxframe; ++frame) {
    //fprintf(stderr, "  frame=%d; rot=%d (%u)\n", frame, (int)sprtemp[frame].rotate, *((unsigned char *)&sprtemp[frame].rotate));
    switch ((int)sprtemp[frame].rotate) {
      case -1:
        // no rotations were found for that frame at all
        if (cli_SprStrict > 0) {
          Sys_Error("R_InstallSprite: No patches found for '%s' frame '%c'", spritename, frame+'A');
        } else {
          if (spr_report_missing_patches) {
            GCon->Logf(NAME_Error, "R_InstallSprite: No patches found for '%s' frame '%c'", spritename, frame+'A');
          }
        }
        break;

      case 0:
        // only the first rotation is needed
        break;

      case 1:
        // copy missing frames for 16-angle rotation
        for (int rotation = 0; rotation < 8; ++rotation) {
          if (sprtemp[frame].lump[rotation*2+1] == -1) {
            sprtemp[frame].lump[rotation*2+1] = sprtemp[frame].lump[rotation*2];
            sprtemp[frame].flip[rotation*2+1] = sprtemp[frame].flip[rotation*2];
          }
          if (sprtemp[frame].lump[rotation*2] == -1) {
            sprtemp[frame].lump[rotation*2] = sprtemp[frame].lump[rotation*2+1];
            sprtemp[frame].flip[rotation*2] = sprtemp[frame].flip[rotation*2+1];
          }
        }
        // must have all 8 frames
        for (int rotation = 0; rotation < 8; ++rotation) {
          if (sprtemp[frame].lump[rotation] == -1) {
            if (cli_SprStrict > 0) {
              Sys_Error("R_InstallSprite: Sprite '%s' frame '%c' is missing rotations", spritename, frame+'A');
            } else {
              if (spr_report_missing_rotations) {
                GCon->Logf(NAME_Error, "R_InstallSprite: Sprite '%s' frame '%c' is missing rotations", spritename, frame+'A');
              }
            }
          }
        }
        break;
    }
  }

  // allocate space for the frames present and copy sprtemp to it
  sprites[index].numframes = maxframe;
  sprites[index].spriteframes = (spriteframe_t *)Z_Malloc(maxframe*sizeof(spriteframe_t));
  if (maxframe) memcpy(sprites[index].spriteframes, sprtemp, maxframe*sizeof(spriteframe_t));
}


//==========================================================================
//
//  FreeSpriteData
//
//==========================================================================
static void FreeSpriteData () {
  for (auto &&ss : sprites) if (ss.spriteframes) Z_Free(ss.spriteframes);
  sprites.clear();
}


//==========================================================================
//
//  R_AreSpritesPresent
//
//==========================================================================
bool R_AreSpritesPresent (int Index) {
  return (Index >= 0 && Index < sprites.length() && sprites.ptr()[Index].numframes > 0);
}


//==========================================================================
//
//  InitNamedTranslations
//
//==========================================================================
static void InitNamedTranslations () {
  for (auto &&it : WadNSNameIterator("trnslate", WADNS_Global)) {
    GCon->Logf(NAME_Init, "parsing named translation table '%s'", *it.getFullName());
    auto sc = new VScriptParser(it.getFullName(), W_CreateLumpReaderNum(it.lump));
    while (sc->GetString()) {
      VStr name = sc->String;
      if (name.isEmpty()) Sys_Error("%s: empty translation name", *sc->GetLoc().toStringNoCol());
      sc->Expect("=");
      R_ParseDecorateTranslation(sc, /*(GameFilter&GAME_Strife ? 7 : 3)*/3, name);
    }
    delete sc;
  }
}


//==========================================================================
//
//  R_InitData
//
//==========================================================================
void R_InitData () {
  // load palette
  InitPalette();
  // calculate RGB table
  InitRgbTable();
  // init standard translation tables
  InitTranslationTables();
  // init color maps
  InitColorMaps();
  // init named translations
  InitNamedTranslations();
}


//==========================================================================
//
//  R_ShutdownData
//
//==========================================================================
void R_ShutdownData () {
  if (TranslationTables) {
    for (int i = 0; i < NumTranslationTables; ++i) {
      delete TranslationTables[i];
      TranslationTables[i] = nullptr;
    }
    delete[] TranslationTables;
    TranslationTables = nullptr;
  }

  for (int i = 0; i < DecorateTranslations.length(); ++i) {
    delete DecorateTranslations[i];
    DecorateTranslations[i] = nullptr;
  }
  DecorateTranslations.Clear();

  FreeSpriteData();

  GLightEffectDefs.Clear();
  GLightEffectDefsMap.clear();
  GParticleEffectDefs.Clear();
  GParticleEffectDefsMap.clear();
}


//==========================================================================
//
//  R_ParseDecorateTranslation
//
//==========================================================================
int R_ParseDecorateTranslation (VScriptParser *sc, int GameMax, VStr trname) {
  // first check for standard translation
  if (sc->CheckNumber()) {
    if (sc->Number < 0 || sc->Number >= max2(NumTranslationTables, GameMax)) {
      //sc->Error(va("Translation must be in range [0, %d]", max2(NumTranslationTables, GameMax)-1));
      GCon->Logf(NAME_Warning, "%s: Translation must be in range [0, %d]", *sc->GetLoc().toStringNoCol(), max2(NumTranslationTables, GameMax)-1);
      sc->Number = 2; // red
    }
    return (TRANSL_Standard<<TRANSL_TYPE_SHIFT)+sc->Number;
  }

  // check for special ice translation
  if (sc->Check("Ice")) return (TRANSL_Standard<<TRANSL_TYPE_SHIFT)+7;

  VTextureTranslation *Tr = new VTextureTranslation;
  do {
    sc->ExpectString();
    Tr->AddTransString(sc->String);
  } while (sc->Check(","));

  // see if we already have this translation
  for (int i = 0; i < DecorateTranslations.length(); ++i) {
    if (DecorateTranslations[i]->Crc != Tr->Crc) continue;
    if (memcmp(DecorateTranslations[i]->Palette, Tr->Palette, sizeof(Tr->Palette))) continue;
    // found a match
    delete Tr;
    Tr = nullptr;
    return (TRANSL_Decorate<<TRANSL_TYPE_SHIFT)+i;
  }

  // add it
  if (DecorateTranslations.length() >= MAX_DECORATE_TRANSLATIONS) {
    sc->Error("Too many translations in DECORATE scripts");
  }
  DecorateTranslations.Append(Tr);
  int res = (TRANSL_Decorate<<TRANSL_TYPE_SHIFT)+DecorateTranslations.length()-1;
  if (!trname.isEmpty()) NamedTranslations.put(trname, res);
  return res;
}


//==========================================================================
//
//  isGoodTranslationArgs
//
//==========================================================================
static inline bool isGoodTranslationArgs (int AStart, int AEnd) noexcept {
  return !(AEnd < AStart || AEnd < 0 || AStart > 255 || !GLevel);
}


//==========================================================================
//
//  appendLevelTranslation
//
//  returns 0 if translation cannot be created
//
//==========================================================================
static int appendLevelTranslation (VTextureTranslation *tr) {
  // see if we already have this translation
  for (int i = 0; i < GLevel->Translations.length(); ++i) {
    if (GLevel->Translations[i]->Crc != tr->Crc) continue;
    if (memcmp(GLevel->Translations[i]->Palette, tr->Palette, sizeof(tr->Palette))) continue;
    // found a match
    delete tr;
    return (TRANSL_Level<<TRANSL_TYPE_SHIFT)+i;
  }

  if (GLevel->Translations.length() >= MAX_LEVEL_TRANSLATIONS) {
    delete tr;
    return 0;
  }

  int idx = GLevel->Translations.length();
  GLevel->Translations.append(tr);
  return (TRANSL_Level<<TRANSL_TYPE_SHIFT)+idx;
}


//==========================================================================
//
//  R_CreateDesaturatedTranslation
//
//  returns 0 if translation cannot be created
//
//==========================================================================
int R_CreateDesaturatedTranslation (int AStart, int AEnd, float rs, float gs, float bs, float re, float ge, float be) {
  if (!isGoodTranslationArgs(AStart, AEnd)) return 0;
  VTextureTranslation *tr = new VTextureTranslation;
  tr->MapDesaturated(AStart, AEnd, rs, gs, bs, re, ge, be);
  return appendLevelTranslation(tr);
}


//==========================================================================
//
//  R_CreateBlendedTranslation
//
//  returns 0 if translation cannot be created
//
//==========================================================================
int R_CreateBlendedTranslation (int AStart, int AEnd, int r, int g, int b) {
  if (!isGoodTranslationArgs(AStart, AEnd)) return 0;
  VTextureTranslation *tr = new VTextureTranslation;
  tr->MapBlended(AStart, AEnd, r, g, b);
  return appendLevelTranslation(tr);
}


//==========================================================================
//
//  R_CreateTintedTranslation
//
//  returns 0 if translation cannot be created
//
//==========================================================================
int R_CreateTintedTranslation (int AStart, int AEnd, int r, int g, int b, int amount) {
  if (!isGoodTranslationArgs(AStart, AEnd) || amount <= 0) return 0;
  VTextureTranslation *tr = new VTextureTranslation;
  tr->MapTinted(AStart, AEnd, r, g, b, amount);
  return appendLevelTranslation(tr);
}


//==========================================================================
//
//  R_GetGamePalette
//
//==========================================================================
void R_GetGamePalette (VColorRGBA newpal[256]) {
  if (!newpal) return;
  for (unsigned int f = 0; f < 256; ++f) {
    newpal[f].r = r_palette[f].r;
    newpal[f].g = r_palette[f].g;
    newpal[f].b = r_palette[f].b;
    newpal[f].a = r_palette[f].a;
  }
}


//==========================================================================
//
//  R_GetTranslatedPalette
//
//==========================================================================
void R_GetTranslatedPalette (int transnum, VColorRGBA newpal[256]) {
  if (!newpal) return;
#ifdef CLIENT
  VTextureTranslation *tr = R_GetCachedTranslation(transnum, GLevel);
  if (!tr) {
    R_GetGamePalette(newpal);
  } else {
    for (unsigned int f = 0; f < 256; ++f) {
      newpal[f].r = tr->Palette[f].r;
      newpal[f].g = tr->Palette[f].g;
      newpal[f].b = tr->Palette[f].b;
      newpal[f].a = tr->Palette[f].a;
    }
    // just in case
    newpal[0].r = newpal[0].g = newpal[0].b = newpal[0].a = 0;
  }
#else
  // FIXME: do we need any translations on server?
  //        anyway, API doesn't guarantee that translation request will succeed,
  //        so this result is valid too
  R_GetGamePalette(newpal);
#endif
}


//==========================================================================
//
//  R_CreateColorTranslation
//
//  returns 0 if translation cannot be created
//  color 0 *WILL* be translated too!
//
//==========================================================================
int R_CreateColorTranslation (const VColorRGBA newpal[256]) {
  if (!newpal) return 0;
  VTextureTranslation *tr = new VTextureTranslation;
  tr->MapToPalette(newpal);
  return appendLevelTranslation(tr);
}


//==========================================================================
//
//  R_FindTranslationByName
//
//==========================================================================
int R_FindTranslationByName (VStr trname) {
  if (trname.isEmpty()) return 0;
  auto tp = NamedTranslations.find(trname);
  return (tp ? *tp : 0);
}


//==========================================================================
//
//  R_GetBloodTranslation
//
//==========================================================================
int R_GetBloodTranslation (int Col, bool allowAdd) {
  // check for duplicate blood translation
  auto ppi = BloodTransMap.find(Col&0xffffff);
  if (ppi) return *ppi;
  /*
  // check for duplicate blood translation
  for (int i = 0; i < BloodTranslations.length(); ++i) {
    if (BloodTranslations[i]->Color == (Col&0xffffff)) {
      return (TRANSL_Blood<<TRANSL_TYPE_SHIFT)+i;
    }
  }
  */

  if (!allowAdd) return 0;

  // check if we have enough room
  if (BloodTranslations.length() >= MAX_BLOOD_TRANSLATIONS) {
    static bool warnPrinted = false;
    if (!warnPrinted) {
      warnPrinted = true;
      GCon->Logf(NAME_Warning, "Too many blood translation colors!");
    }
    return 0;
  }

  // create new translation
  VTextureTranslation *Tr = new VTextureTranslation;
  Tr->BuildBloodTrans(Col);

  const int idx = (TRANSL_Blood<<TRANSL_TYPE_SHIFT)+BloodTranslations.length();
  // append
  BloodTranslations.Append(Tr);
  // register in map
  BloodTransMap.put(Col&0xffffff, idx);

  // done
  return idx;
}


//==========================================================================
//
//  R_FindLightEffect
//
//==========================================================================
VLightEffectDef *R_FindLightEffect (VStr Name) {
  if (Name.length()) {
    /*
    for (int i = 0; i < GLightEffectDefs.length(); ++i) {
      if (Name.ICmp(*GLightEffectDefs[i].Name) == 0) return &GLightEffectDefs[i];
    }
    */
    auto ldip = GLightEffectDefsMap.find(Name);
    if (ldip) return &GLightEffectDefs[*ldip];
  }
  return nullptr;
}


//==========================================================================
//
//  FindParticleEffect
//
//==========================================================================
static VParticleEffectDef *FindParticleEffect (VStr Name) {
  if (Name.length()) {
    /*
    for (int i = 0; i < GParticleEffectDefs.length(); ++i) {
      if (Name.ICmp(*GParticleEffectDefs[i].Name) == 0) return &GParticleEffectDefs[i];
    }
    */
    auto pdip = GParticleEffectDefsMap.find(Name);
    if (pdip) return &GParticleEffectDefs[*pdip];
  }
  return nullptr;
}


//==========================================================================
//
//  NewLightEffect
//
//==========================================================================
static VLightEffectDef *NewLightEffect (VStr Name, int LightType) {
  VLightEffectDef *L = R_FindLightEffect(Name);
  if (!L) {
    L = &GLightEffectDefs.Alloc();
    L->Name = VName(*Name, VName::AddLower);
    GLightEffectDefsMap.put(Name, GLightEffectDefs.length()-1);
    L->setDefaultValues(LightType);
  }
  return L;
}


//==========================================================================
//
//  NewLightAlias
//
//==========================================================================
static void NewLightAlias (const VLightEffectDef *lt, VStr newname) {
  if (!lt || newname.length() == 0) return;
  auto ldip = GLightEffectDefsMap.find(*lt->Name);
  vassert(ldip);
  GLightEffectDefsMap.put(newname, *ldip);
}


//==========================================================================
//
//  NewParticleEffect
//
//==========================================================================
static VParticleEffectDef *NewParticleEffect (VStr Name) {
  VParticleEffectDef *P = FindParticleEffect(Name);
  if (!P) {
    P = &GParticleEffectDefs.Alloc();
    P->Name = VName(*Name, VName::AddLower);
    GParticleEffectDefsMap.put(Name, GParticleEffectDefs.length()-1);
    P->setDefaultValues();
  }
  return P;
}


//==========================================================================
//
//  NewParticleAlias
//
//==========================================================================
static void NewParticleAlias (const VParticleEffectDef *pt, VStr newname) {
  if (!pt || newname.length() == 0) return;
  auto pdip = GParticleEffectDefsMap.find(*pt->Name);
  vassert(pdip);
  GParticleEffectDefsMap.put(newname, *pdip);
}


//==========================================================================
//
//  ParseLightDef
//
//==========================================================================
static void ParseLightDef (VScriptParser *sc, int LightType) {
  const bool extend = sc->Check("extend");
  sc->ExpectString();
  if (extend && !R_FindLightEffect(sc->String)) sc->Error(va("Cannot extend unknown light '%s'", *sc->String));
  VLightEffectDef *L = NewLightEffect(sc->String, LightType);
  if (!extend) L->setDefaultValues(LightType);
  // parse light def
  sc->Expect("{");
  while (!sc->Check("}")) {
    if (sc->Check("colour") || sc->Check("color")) {
      sc->ExpectFloat();
      float r = midval(0.0f, (float)sc->Float, 1.0f);
      sc->ExpectFloat();
      float g = midval(0.0f, (float)sc->Float, 1.0f);
      sc->ExpectFloat();
      float b = midval(0.0f, (float)sc->Float, 1.0f);
      L->Color = ((int)(r*255)<<16)|((int)(g*255)<<8)|(int)(b*255)|0xff000000;
    } else if (sc->Check("radius")) {
      sc->ExpectFloat();
      L->Radius = sc->Float;
    } else if (sc->Check("radius2")) {
      sc->ExpectFloat();
      L->Radius2 = sc->Float;
    } else if (sc->Check("minlight")) {
      sc->ExpectFloat();
      L->MinLight = sc->Float;
    } else if (sc->Check("noselfshadow")) {
      L->SetNoSelfShadow(true);
    } else if (sc->Check("noshadow")) {
      L->SetNoShadow(true);
    } else if (sc->Check("offset")) {
      sc->ExpectFloat();
      L->Offset.x = sc->Float;
      sc->ExpectFloat();
      L->Offset.y = sc->Float;
      sc->ExpectFloat();
      L->Offset.z = sc->Float;
    } else if (sc->Check("coneangle")) {
      sc->ExpectFloat();
      L->ConeAngle = clampval(sc->Float, 0.0f, 360.0f);
    } else if (sc->Check("conedir")) {
      sc->ExpectFloat();
      L->ConeDir.x = sc->Float;
      sc->ExpectFloat();
      L->ConeDir.y = sc->Float;
      sc->ExpectFloat();
      L->ConeDir.z = sc->Float;
      L->ConeDir = L->ConeDir.Normalised();
      if (!L->ConeDir.isValid()) L->ConeDir = TVec(0, 0, 0);
    } else if (sc->Check("alias")) {
      sc->ExpectString();
      NewLightAlias(L, sc->String);
    } else {
      sc->Error(va("Bad point light parameter (%s)", *sc->String));
    }
  }

  if (L->Type&DLTYPE_Spot) {
    //GCon->Logf(NAME_Debug, "dir=(%g,%g,%g); angle=%g", L->ConeDir.x, L->ConeDir.y, L->ConeDir.z, L->ConeAngle);
    if (L->ConeAngle < 1 || L->ConeAngle >= 360 || L->ConeDir.isZero()) {
      L->Type &= ~DLTYPE_Spot;
    }
  }
}


//==========================================================================
//
//  ParseGZLightDef
//
//==========================================================================
static void ParseGZLightDef (VScriptParser *sc, int LightType, float lightsizefactor) {
  sc->ExpectString();
  VLightEffectDef *L = NewLightEffect(sc->String, LightType);
  L->setDefaultValues(LightType);
  L->SetNoSelfShadow(true); // by default
  bool attenuated = false;
  // parse light def
  sc->Expect("{");
  while (!sc->Check("}")) {
    if (sc->Check("color") || sc->Check("colour")) {
      sc->ExpectFloat();
      float r = midval(0.0f, (float)sc->Float, 1.0f);
      sc->ExpectFloat();
      float g = midval(0.0f, (float)sc->Float, 1.0f);
      sc->ExpectFloat();
      float b = midval(0.0f, (float)sc->Float, 1.0f);
      L->Color = ((int)(r*255)<<16)|((int)(g*255)<<8)|(int)(b*255)|0xff000000;
    } else if (sc->Check("size")) {
      sc->ExpectFloat();
      L->Radius = sc->Float;
    } else if (sc->Check("secondarySize")) {
      sc->ExpectFloat();
      L->Radius2 = sc->Float;
    } else if (sc->Check("offset")) {
      // GZDoom manages Z offset as Y offset
      sc->ExpectFloat();
      L->Offset.x = sc->Float;
      sc->ExpectFloat();
      L->Offset.z = sc->Float;
      sc->ExpectFloat();
      L->Offset.y = sc->Float;
    } else if (sc->Check("subtractive")) {
      sc->ExpectNumber();
      sc->Message(va("Subtractive light ('%s') is not supported yet.", *L->Name));
    } else if (sc->Check("chance")) {
      sc->ExpectFloat();
      // negative means "special processing"
      L->Chance = -(clampval((float)sc->Float, 0.0f, 1.0f)+1.0f);
    } else if (sc->Check("scale")) {
      sc->ExpectFloat();
      L->Scale = sc->Float;
    } else if (sc->Check("interval")) {
      sc->ExpectFloat();
      L->Interval = sc->Float*35.0f;
    } else if (sc->Check("additive")) {
      sc->ExpectNumber();
      sc->Message(va("Additive light ('%s') parameter not supported yet.", *L->Name));
    } else if (sc->Check("halo")) {
      sc->ExpectNumber();
      sc->Message(va("Halo light ('%s') parameter not supported.", *L->Name));
    } else if (sc->Check("dontlightself")) {
      sc->ExpectNumber();
      sc->Message(va("DontLightSelf light ('%s') parameter not supported.", *L->Name));
    } else if (sc->Check("attenuate")) {
      sc->ExpectNumber();
      if (sc->Number) {
        attenuated = true;
      } else {
        attenuated = false;
        sc->Message(va("Non-attenuated light ('%s') will be attenuated anyway.", *L->Name));
      }
    } else if (sc->Check("noselfshadow")) {
      // k8vavoom extension
      L->SetNoSelfShadow(true);
    } else if (sc->Check("selfshadow")) {
      // k8vavoom extension
      L->SetNoSelfShadow(false);
    } else if (sc->Check("noshadow")) {
      // k8vavoom extension
      L->SetNoShadow(true);
    } else if (sc->Check("alias")) {
      // k8vavoom extension
      sc->ExpectString();
      NewLightAlias(L, sc->String);
    } else {
      sc->Error(va("Bad gz light ('%s') parameter (%s)", *L->Name, *sc->String));
    }
  }

  //k8: it seems to work this way...
  if (attenuated) {
    L->Radius *= lightsizefactor;
    L->Radius2 *= lightsizefactor;
  } else {
    //k8: i absolutely don't know what i am doing here, and why
    // bump intensity
    float r = ((L->Color>>16)&0xff)/255.0f;
    float g = ((L->Color>>8)&0xff)/255.0f;
    float b = (L->Color&0xff)/255.0f;
    #if 0
    float h, s, l;
    M_RgbToHsl(r, g, b, h, s, l);
    l = clampval(l*2.2f, 0.0f, 1.0f);
    //GCon->Logf("OLD(%s): (%g,%g,%g)", *L->Name, r, g, b);
    M_HslToRgb(h, s, l, r, g, b);
    //GCon->Logf("NEW(%s): (%g,%g,%g)", *L->Name, r, g, b);
    #else
    float h, s, v;
    M_RgbToHsv(r, g, b, h, s, v);
    v = clampval(v*1.5f, 0.0f, 1.0f);
    //GCon->Logf("OLD(%s): (%g,%g,%g)", *L->Name, r, g, b);
    M_HsvToRgb(h, s, v, r, g, b);
    //GCon->Logf("NEW(%s): (%g,%g,%g)", *L->Name, r, g, b);
    #endif
    L->Color = ((int)(r*255)<<16)|((int)(g*255)<<8)|(int)(b*255)|0xff000000;
  }

  L->Radius = VLevelInfo::GZSizeToRadius(L->Radius, attenuated);
  L->Radius2 = VLevelInfo::GZSizeToRadius(L->Radius2, attenuated);
}


//==========================================================================
//
//  ParseParticleEffect
//
//==========================================================================
static void ParseParticleEffect (VScriptParser *sc) {
  const bool extend = sc->Check("extend");
  sc->ExpectString();
  if (extend && !FindParticleEffect(sc->String)) sc->Error(va("Cannot extend unknown particle effect '%s'", *sc->String));
  VParticleEffectDef *P = NewParticleEffect(sc->String);
  if (!extend) P->setDefaultValues();
  // parse particle def
  sc->Expect("{");
  while (!sc->Check("}")) {
    if (sc->Check("type")) {
           if (sc->Check("static")) P->Type = 0;
      else if (sc->Check("explode")) P->Type = 1;
      else if (sc->Check("explode2")) P->Type = 2;
      else sc->Error("Bad type");
    } else if (sc->Check("type2")) {
           if (sc->Check("static")) P->Type2 = 0;
      else if (sc->Check("explode")) P->Type2 = 1;
      else if (sc->Check("explode2")) P->Type2 = 2;
      else sc->Error("Bad type");
    } else if (sc->Check("colour") || sc->Check("color")) {
      sc->ExpectFloat();
      float r = midval(0.0f, (float)sc->Float, 1.0f);
      sc->ExpectFloat();
      float g = midval(0.0f, (float)sc->Float, 1.0f);
      sc->ExpectFloat();
      float b = midval(0.0f, (float)sc->Float, 1.0f);
      P->Color = ((int)(r*255)<<16)|((int)(g*255)<<8)|(int)(b*255)|0xff000000;
    } else if (sc->Check("offset")) {
      sc->ExpectFloat();
      P->Offset.x = sc->Float;
      sc->ExpectFloat();
      P->Offset.y = sc->Float;
      sc->ExpectFloat();
      P->Offset.z = sc->Float;
    } else if (sc->Check("count")) {
      sc->ExpectNumber();
      P->Count = sc->Number;
    } else if (sc->Check("originrandom")) {
      sc->ExpectFloat();
      P->OrgRnd = sc->Float;
    } else if (sc->Check("velocity")) {
      sc->ExpectFloat();
      P->Velocity.x = sc->Float;
      sc->ExpectFloat();
      P->Velocity.y = sc->Float;
      sc->ExpectFloat();
      P->Velocity.z = sc->Float;
    } else if (sc->Check("velocityrandom")) {
      sc->ExpectFloat();
      P->VelRnd = sc->Float;
    } else if (sc->Check("acceleration")) {
      sc->ExpectFloat();
      P->Accel = sc->Float;
    } else if (sc->Check("gravity")) {
      sc->ExpectFloat();
      P->Grav = sc->Float;
    } else if (sc->Check("duration")) {
      sc->ExpectFloat();
      P->Duration = sc->Float;
    } else if (sc->Check("ramp")) {
      sc->ExpectFloat();
      P->Ramp = sc->Float;
    } else if (sc->Check("alias")) {
      sc->ExpectString();
      NewParticleAlias(P, sc->String);
    } else {
      sc->Error(va("Bad particle effect parameter (%s)", *sc->String));
    }
  }
}


//==========================================================================
//
//  ParseClassEffects
//
//==========================================================================
static void ParseClassEffects (VScriptParser *sc, TArray<VTempClassEffects> &ClassDefs) {
  // get name, find it in the list or add it if it's not there yet
  sc->ExpectString();
  VTempClassEffects *C = nullptr;
  for (int i = 0; i < ClassDefs.length(); ++i) {
    if (ClassDefs[i].ClassName.ICmp(sc->String) == 0) {
      C = &ClassDefs[i];
      break;
    }
  }
  if (!C) C = &ClassDefs.Alloc();

  // set defaults
  C->ClassName = sc->String;
  C->StaticLight.Clean();
  C->SpriteEffects.Clear();

  // parse
  sc->Expect("{");
  while (!sc->Check("}")) {
    if (sc->Check("frame")) {
      sc->ExpectString();
      VTempSpriteEffectDef &S = C->SpriteEffects.Alloc();
      S.Sprite = sc->String;
      sc->Expect("{");
      while (!sc->Check("}")) {
        if (sc->Check("light")) {
          sc->ExpectString();
          S.Light = sc->String.ToLower();
        } else if (sc->Check("particles")) {
          sc->ExpectString();
          S.Part = sc->String.ToLower();
        } else {
          sc->Error("Bad frame parameter");
        }
      }
    } else if (sc->Check("static_light")) {
      sc->ExpectString();
      C->StaticLight = sc->String.ToLower();
    } else {
      sc->Error("Bad class parameter");
    }
  }
}


//==========================================================================
//
//  ParseEffectDefs
//
//==========================================================================
static void ParseEffectDefs (VScriptParser *sc, TArray<VTempClassEffects> &ClassDefs) {
  if (developer) GCon->Logf(NAME_Dev, "parsing k8vavoom effect definitions '%s'", *sc->GetScriptName());
  while (!sc->AtEnd()) {
    if (sc->Check("#include")) {
      sc->ExpectString();
      int Lump = W_CheckNumForFileName(sc->String);
      // check WAD lump only if it's no longer than 8 characters and has no path separator
      if (Lump < 0 && sc->String.Length() <= 8 && sc->String.IndexOf('/') < 0) {
        Lump = W_CheckNumForName(VName(*sc->String, VName::AddLower8));
      }
      if (Lump < 0) sc->Error(va("Lump '%s' not found", *sc->String));
      ParseEffectDefs(new VScriptParser(W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump)), ClassDefs);
      continue;
    }
    else if (sc->Check("pointlight")) ParseLightDef(sc, DLTYPE_Point);
    else if (sc->Check("spotlight")) ParseLightDef(sc, DLTYPE_Point|DLTYPE_Spot);
    else if (sc->Check("muzzleflashlight")) ParseLightDef(sc, DLTYPE_MuzzleFlash);
    else if (sc->Check("particleeffect")) ParseParticleEffect(sc);
    else if (sc->Check("class")) ParseClassEffects(sc, ClassDefs);
    else if (sc->Check("clear")) ClassDefs.Clear();
    else sc->Error(va("Unknown command (%s)", *sc->String));
  }
  delete sc;
}


//==========================================================================
//
//  ParseBrightmap
//
//==========================================================================
static void ParseBrightmap (int SrcLump, VScriptParser *sc) {
  int ttype = TEXTYPE_Any;
       if (sc->Check("flat")) ttype = TEXTYPE_Flat;
  else if (sc->Check("sprite")) ttype = TEXTYPE_Sprite;
  else if (sc->Check("texture")) ttype = TEXTYPE_Wall;
  else sc->Error("unknown brightmap type");
  VName img;
#ifdef CLIENT
  bool imgIsPath = false;
#endif
  if (sc->ExpectName8WarnOrFilePath()) {
    img = sc->Name8;
  } else {
#ifdef CLIENT
    imgIsPath = true;
#endif
    img = VName(*sc->String, VName::AddLower);
  }
  VStr bmap;
  bool iwad = false;
  bool thiswad = false;
  bool nofb = false;
  sc->Expect("{");
  while (!sc->Check("}")) {
    if (sc->Check("map")) {
      if (!sc->GetString()) sc->Error(va("brightmap image name expected for image '%s'", *img));
      if (sc->String.isEmpty()) sc->Error(va("empty brightmap image for image '%s'", *img));
      if (!bmap.isEmpty()) GCon->Logf(NAME_Warning, "duplicate brightmap image for image '%s'", *img);
      bmap = sc->String.fixSlashes();
    } else if (sc->Check("iwad")) {
      iwad = true;
    } else if (sc->Check("thiswad")) {
      thiswad = true;
    } else if (sc->Check("disablefullbright")) {
      nofb = true;
    } else {
      //sc->Error(va("Unknown command (%s)", *sc->String));
      sc->ExpectString();
      sc->Message(va("Unknown command (%s) for image '%s'", *sc->String, *img));
    }
  }
  // there is no need to load brightmap textures for server
#ifdef CLIENT
  if (!VTextureManager::IsDummyTextureName(img) && !bmap.isEmpty()) {
    const bool doWarn = (cli_WAll > 0 || cli_WarnBrightmaps > 0);
    const bool doLoadDump = (cli_DumpBrightmaps > 0);

    VTexture *basetex = nullptr;
    if (!imgIsPath) {
      basetex = GTextureManager.GetExistingTextureByName(VStr(img), ttype);
    } else {
      // this may happen for brightmaps that is using UDMF map-local textures
      // they should be loaded on-demand, but meh... let's do it this way now
      int tnum = GTextureManager.AddFileTexture(img, /*TEXTYPE_Skin*/TEXTYPE_Wall); // wtf? why it is a skin?!
      if (tnum > 0) basetex = GTextureManager[tnum];
      //if (basetex) GCon->Logf(NAME_Debug, "force-loaded texture '%s' (%s) for brightmap '%s'", *img, *W_FullLumpName(basetex->SourceLump), *bmap);
    }

    if (!basetex) {
      if (doWarn) GCon->Logf(NAME_Warning, "texture '%s' not found, cannot attach brightmap", *img);
      return;
    }

    if (x_brightmaps_ignore_iwad) iwad = false;

    if (iwad && !W_IsIWADLump(basetex->SourceLump)) {
      // oops
      if (doWarn) GCon->Logf(NAME_Warning, "IWAD SKIP! '%s' (%s[%d]; '%s')", *img, *W_FullLumpName(basetex->SourceLump), basetex->SourceLump, *basetex->Name);
      return;
    }
    //if (iwad && doWarn) GCon->Logf(NAME_Warning, "IWAD PASS! '%s'", *img);

    basetex->nofullbright = nofb;
    delete basetex->Brightmap; // it is safe to remove it, as each brightmap texture is unique
    basetex->Brightmap = nullptr;

    int lmp = W_CheckNumForFileName(bmap);
    if (lmp < 0) {
      /*static*/ const EWadNamespace lookns[] = {
        WADNS_Graphics,
        WADNS_Sprites,
        WADNS_Flats,
        WADNS_NewTextures,
        WADNS_HiResTextures,
        WADNS_HiResTexturesDDay,
        WADNS_HiResFlatsDDay,
        WADNS_Patches,
        WADNS_Global,
        // end marker
        WADNS_ZipSpecial,
      };

      // this can be ordinary texture lump, try hard to find it
      if (bmap.indexOf('/') < 0 && bmap.indexOf('.') < 0) {
        for (unsigned nsidx = 0; lookns[nsidx] != WADNS_ZipSpecial; ++nsidx) {
          for (int Lump = W_IterateNS(-1, lookns[nsidx]); Lump >= 0; Lump = W_IterateNS(Lump, lookns[nsidx])) {
            if (W_LumpFile(Lump) > W_LumpFile(SrcLump)) break;
            if (Lump <= lmp) continue;
            if (bmap.ICmp(*W_LumpName(Lump)) == 0) lmp = Lump;
          }
        }
        //if (lmp >= 0) GCon->Logf(NAME_Warning, "std. brightmap texture '%s' is '%s'", *bmap, *W_FullLumpName(lmp));
      }
      if (lmp < 0) GCon->Logf(NAME_Warning, "brightmap texture '%s' not found", *bmap);
      return;
    }

    if (thiswad && W_LumpFile(lmp) != W_LumpFile(basetex->SourceLump)) return;

    VTexture *bm = VTexture::CreateTexture(TEXTYPE_Any, lmp, false); // don't set name
    if (!bm) {
      GCon->Logf(NAME_Warning, "cannot load brightmap texture '%s'", *bmap);
      return;
    }
    bm->nofullbright = nofb; // just in case
    bm->Name = VName(*(W_RealLumpName(lmp).ExtractFileBaseName().StripExtension()), VName::AddLower);

    if (bm->GetWidth() != basetex->GetWidth() || bm->GetHeight() != basetex->GetHeight()) {
      if (doWarn) {
        GCon->Logf(NAME_Warning, "texture '%s' has dimensions (%d,%d), but brightmap '%s' has dimensions (%d,%d)",
          *img, basetex->GetWidth(), basetex->GetHeight(), *bmap, bm->GetWidth(), bm->GetHeight());
      }
      bm->ResizeCanvas(basetex->GetWidth(), basetex->GetHeight());
      vassert(bm->GetWidth() == basetex->GetWidth());
      vassert(bm->GetHeight() == basetex->GetHeight());
    }

    basetex->Brightmap = bm;
    bm->BrightmapBase = basetex;
    if (doLoadDump) GCon->Logf(NAME_Warning, "texture '%s' got brightmap '%s' (%p : %p) (lump %d:%s)", *basetex->Name, *bm->Name, basetex, bm, lmp, *W_FullLumpName(lmp));
  }
#else
  (void)img;
  (void)iwad;
  (void)thiswad;
  (void)nofb;
  (void)ttype;
#endif
}


#ifdef CLIENT
//==========================================================================
//
//  ApplyGlowToExactTexture
//
//==========================================================================
static void ApplyGlowToExactTexture (VName txname, bool isWall, bool allowOtherType, bool fullbright, bool iwad, vuint32 clr=0u) {
  if (!VTextureManager::IsDummyTextureName(txname)) {
    VTexture *basetex = GTextureManager.GetExistingTextureByName(VStr(txname), (isWall ? TEXTYPE_Wall : TEXTYPE_Flat));
    if (!basetex && allowOtherType) basetex = GTextureManager.GetExistingTextureByName(VStr(txname), (!isWall ? TEXTYPE_Wall : TEXTYPE_Flat));
    if (basetex) {
      if (iwad && !W_IsIWADLump(basetex->SourceLump)) {
        // oops
        GCon->Logf(NAME_Warning, "IWAD GLOW SKIP: %s[%d]; '%s'", *W_FullLumpName(basetex->SourceLump), basetex->SourceLump, *basetex->Name);
        return;
      }
      rgb_t gclr;
      if (clr) {
        gclr.r = (clr>>16)&0xffu;
        gclr.g = (clr>>8)&0xffu;
        gclr.b = clr&0xffu;
      } else {
        gclr = basetex->GetAverageColor(153);
      }
      //GCon->Logf(NAME_Debug, "GLOW: <%s> : <%s> (%d,%d,%d)", *txname, *basetex->Name, gclr.r, gclr.g, gclr.b);
      if (gclr.r || gclr.g || gclr.b) {
        basetex->glowing = (fullbright ? 0xff000000u : 0xfe000000u)|(gclr.r<<16)|(gclr.g<<8)|gclr.b;
      } else {
        basetex->glowing = 0;
      }
      // we don't need its pixels anymore (also, undo any RGBA conversion)
      basetex->ReleasePixels();
    }
  }
}


//==========================================================================
//
//  GlowNameMaskCheck
//
//==========================================================================
static bool GlowNameMaskCheck (const char *name, const char *mask) {
  if (!mask || !mask[0]) return false;
  if (!name || !name[0]) return false;
  if (VStr::startsWithCI(name, mask)) return true;
  return VStr::globMatchCI(name, mask);
}
#endif


//==========================================================================
//
//  ApplyGlowToExactTexture
//
//==========================================================================
static void ApplyGlowToTexture (VName txname, VStr txnamefull, bool isWall, bool allowOtherType, bool fullbright, bool iwad, bool basename, vuint32 clr=0u) {
#ifdef CLIENT
  if (VTextureManager::IsDummyTextureName(txname)) return;
  if (r_glow_ignore_iwad) iwad = false;
  if (!basename) {
    //GCon->Logf(NAME_Debug, "applying glow '%s'", *txname);
    ApplyGlowToExactTexture(txname, isWall, allowOtherType, fullbright, iwad, clr);
  } else {
    const int count = GTextureManager.GetNumTextures();
    for (int f = 0; f < count; ++f) {
      VTexture *tex = GTextureManager.getIgnoreAnim(f);
      if (!tex || tex->Name == NAME_None || tex->Type == TEXTYPE_Null || tex->Width < 1 || tex->Height < 1) continue;
      if (iwad && !W_IsIWADLump(tex->SourceLump)) continue;
      if (GlowNameMaskCheck(*tex->Name, *txnamefull)) {
        //GCon->Logf(NAME_Debug, "applying glow mask '%s' to '%s'", *txnamefull, *tex->Name);
        ApplyGlowToExactTexture(tex->Name, isWall, allowOtherType, fullbright, iwad, clr);
        continue;
      }
      /*
      else {
        GCon->Logf(NAME_Debug, "skipping glow mask '%s' at '%s'", *txnamefull, *tex->Name);
      }
      */
    }
  }
#endif
}


//==========================================================================
//
//  ParseGlow
//
//==========================================================================
static void ParseGlow (VScriptParser *sc) {
  bool allFullBright = true;
  bool allIWad = false;
  bool allBaseName = false;
  // parse global options
  for (;;) {
    if (sc->Check("fullbright")) { allFullBright = true; continue; }
    if (sc->Check("nofullbright")) { allFullBright = false; continue; }
    if (sc->Check("iwad")) { allIWad = true; continue; }
    if (sc->Check("basename")) { allBaseName = true; continue; }
    break;
  }
  sc->Expect("{");
  while (!sc->Check("}")) {
    // not implemented gozzo feature (in gozzo too)
    if (sc->Check("Texture")) {
      // Texture "flat name", color[, glow height] [, fullbright]
      VName txname;
      if (allBaseName) { sc->ExpectName(); txname = sc->Name; } else { sc->ExpectName8(); txname = sc->Name8; }
      VStr txnamefull = sc->String;
      if (sc->Check(",")) {
        // ignore, we cannot support parameters yet
        sc->ExpectString();
        while (sc->Check(",")) sc->ExpectString();
        (void)sc->Check("fullbright");
      } else {
        // no parameters, apply normal glow
        bool fullbright = allFullBright;
        bool iwad = allIWad;
        bool basename = allBaseName;
        for (;;) {
          if (sc->Check("fullbright")) { fullbright = true; continue; }
          if (sc->Check("nofullbright")) { fullbright = false; continue; }
          if (sc->Check("iwad")) { iwad = true; continue; }
          if (sc->Check("basename")) { basename = true; continue; }
          break;
        }
        ApplyGlowToTexture(txname, txnamefull, true, true, fullbright, iwad, basename);
      }
      continue;
    }
    // texture list
    int ttype = -1;
         if (sc->Check("flats")) ttype = TEXTYPE_Flat;
    else if (sc->Check("walls")) ttype = TEXTYPE_Wall;
    if (ttype > 0) {
      bool fullbright = allFullBright;
      bool iwad = allIWad;
      bool basename = allBaseName;
      for (;;) {
        if (sc->Check("fullbright")) { fullbright = true; continue; }
        if (sc->Check("nofullbright")) { fullbright = false; continue; }
        if (sc->Check("iwad")) { iwad = true; continue; }
        if (sc->Check("basename")) { basename = true; continue; }
        break;
      }
      sc->Expect("{");
      VName txname;
      VStr txnamefull;
      while (!sc->Check("}")) {
        if (sc->Check(",")) continue;
        bool xfbr = fullbright;
        bool xiwad = iwad;
        vuint32 gclr = 0u;
        if (sc->Check("texture")) {
          if (basename) { sc->ExpectName(); txname = sc->Name; } else { sc->ExpectName8(); txname = sc->Name8; }
          txnamefull = sc->String;
          while (sc->Check(",")) sc->ExpectString();
               if (sc->Check("fullbright")) xfbr = true;
          else if (sc->Check("nofullbright")) xfbr = false;
        } else {
          if (basename) { sc->ExpectName(); txname = sc->Name; } else { sc->ExpectName8(); txname = sc->Name8; }
          txnamefull = sc->String;
          // check for glow color and fullbright flag
          while (sc->GetString()) {
            if (sc->Crossed) { sc->UnGet(); break; }
            if (sc->String.strEquCI("fullbright")) { xfbr = true; continue; }
            if (sc->String.strEquCI("nofullbright")) { xfbr = false; continue; }
            if (sc->String.strEquCI("iwad")) { xiwad = true; continue; }
            // try to parse as color
            vuint32 cc = M_ParseColor(*sc->String, true/*retZeroIfInvalid*/);
            if (!cc) { sc->UnGet(); break; }
            gclr = cc;
          }
          //if (gclr || xfbr) GCon->Logf(NAME_Debug, "GLOW for '%s': gclr=0x%08x; fbr=%d", *txname, gclr, (int)xfbr);
        }
        ApplyGlowToTexture(txname, txnamefull, (ttype == TEXTYPE_Wall), false, xfbr, xiwad, basename, gclr);
      }
      continue;
    }
    // something strange
    if (sc->Check("{")) {
      sc->SkipBracketed(true);
      continue;
    }
    if (!sc->GetString()) sc->Error("unexpected EOF");
  }
}


//==========================================================================
//
//  ParseIncludeCondition
//
//==========================================================================
static bool ParseIncludeCondition (VScriptParser *sc) {
  enum {
    IFANYWAD,
    IFNOTWADS,
  };

  if (!sc->Check(":")) return true;

  bool oldCMode = sc->IsCMode();
  sc->SetCMode(true);

  int cond = -1;
       if (sc->Check("ifanywad")) cond = IFANYWAD;
  else if (sc->Check("ifanywads")) cond = IFANYWAD; // common typo
  else if (sc->Check("ifnotwads")) cond = IFNOTWADS;
  else if (sc->Check("ifnotwad")) cond = IFNOTWADS; // common typo
  else sc->Error("include condition expected");

  //GCon->Logf(NAME_Debug, ": cond=%d", cond);

  int count = 0, foundcount = 0;
  sc->Expect("(");
  while (!sc->Check(")")) {
    sc->ExpectString();
    VStr wadname = sc->String;
    if (!wadname.isEmpty()) {
      // find wad
      wadname = wadname.extractFileBaseName().stripExtension();
      bool found = false;
      for (int f = 0; f < W_NextMountFileId(); ++f) {
        VStr pak = W_FullPakNameByFile(f);
        int cpos = pak.lastIndexOf(':');
        if (cpos > 0) pak.chopLeft(cpos+1);
        pak = pak.extractFileBaseName().stripExtension();
        //GCon->Logf(NAME_Debug, "<%s> : <%s> (%d)", *pak, *wadname, (int)pak.strEquCI(wadname));
        if (pak.strEquCI(wadname)) { found = true; break; }
      }
      ++count;
      if (found) ++foundcount;
    }
    if (!sc->Check(",")) { sc->Expect(")"); break; }
  }
  sc->SetCMode(oldCMode);
  //GCon->Logf(NAME_Debug, "::: count=%d; foundcount=%d", count, foundcount);
  if (count == 0) return true;

  switch (cond) {
    case IFANYWAD:
      return (foundcount > 0);
    case IFNOTWADS:
      return (foundcount == 0);
    default: Sys_Error("oops!");
  }

  return false;
}


//==========================================================================
//
//  ParseGZDoomEffectDefs
//
//==========================================================================
static void ParseGZDoomEffectDefs (int SrcLump, VScriptParser *sc, TArray<VTempClassEffects> &ClassDefs) {
  // for old mods (before Apr 2018) it should be `0.667f` (see https://forum.zdoom.org/viewtopic.php?t=60280 )
  // sadly, there is no way to autodetect it, so let's use what GZDoom is using now
  float lightsizefactor = 1.0; // for attenuated lights
  if (developer) GCon->Logf(NAME_Dev, "parsing GZDoom light definitions '%s'", *sc->GetScriptName());
  while (!sc->AtEnd()) {
    if (sc->Check("#include")) {
      sc->ExpectString();
      VStr incfile = sc->String.fixSlashes();
      // parse optional condition
      bool doit = ParseIncludeCondition(sc);
      if (doit) {
        int Lump = W_CheckNumForFileName(incfile);
        // check WAD lump only if it's no longer than 8 characters and has no path separator
        if (Lump < 0 && incfile.Length() <= 8 && incfile.IndexOf('/') < 0) {
          VName nn = VName(*incfile, VName::FindLower8);
          if (nn != NAME_None) Lump = W_CheckNumForName(nn);
        }
        if (Lump < 0) sc->Error(va("Lump '%s' not found", *incfile));
        //GCon->Logf(NAME_Debug, "including '%s' (%s)", *incfile, *W_FullLumpName(Lump));
        ParseGZDoomEffectDefs(Lump, new VScriptParser(W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump)), ClassDefs);
      } else {
        GCon->Logf(NAME_Init, "gldefs include file '%s' skipped due to condition", *incfile);
      }
      continue;
    }
    else if (sc->Check("pointlight")) ParseGZLightDef(sc, DLTYPE_Point, lightsizefactor);
    else if (sc->Check("pulselight")) ParseGZLightDef(sc, DLTYPE_Pulse, lightsizefactor);
    else if (sc->Check("flickerlight")) ParseGZLightDef(sc, DLTYPE_Flicker, lightsizefactor);
    else if (sc->Check("flickerlight2")) ParseGZLightDef(sc, DLTYPE_FlickerRandom, lightsizefactor);
    else if (sc->Check("sectorlight")) ParseGZLightDef(sc, DLTYPE_Sector, lightsizefactor);
    else if (sc->Check("object")) ParseClassEffects(sc, ClassDefs);
    else if (sc->Check("skybox")) R_ParseGLDefSkyBoxesScript(sc);
    else if (sc->Check("brightmap")) ParseBrightmap(SrcLump, sc);
    else if (sc->Check("glow")) ParseGlow(sc);
    else if (sc->Check("hardwareshader")) { sc->Message("Shaders are not supported"); sc->SkipBracketed(); }
    else if (sc->Check("lightsizefactor")) { sc->ExpectFloat(); lightsizefactor = clampval((float)sc->Float, 0.0f, 4.0f); }
    else if (sc->Check("material")) {
      sc->Message("Materials are not supported");
      while (sc->GetString()) {
        if (sc->String == "}") break;
        if (sc->String == "{") { sc->SkipBracketed(true); break; }
      }
    } else { sc->Error(va("Unknown command (%s)", *sc->String)); }
  }
  delete sc;
}


//==========================================================================
//
//  R_ParseEffectDefs
//
//==========================================================================
void R_ParseEffectDefs () {
  GCon->Log(NAME_Init, "Parsing effect defs");

  TArray<VTempClassEffects> ClassDefs;

  TArray<int> gldefslist;

  // parse VFXDEFS, GLDEFS, etc. scripts
  // parse GLDefs after VFXDEFS
  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    VName lname = W_LumpName(Lump);
    if (lname == NAME_vfxdefs) {
      GCon->Logf(NAME_Init, "Parsing k8vavoom effect definitions '%s'", *W_FullLumpName(Lump));
      ParseEffectDefs(new VScriptParser(W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump)), ClassDefs);
      continue;
    }
    if (lname == NAME_gldefs ||
        lname == NAME_doomdefs || lname == NAME_hticdefs ||
        lname == NAME_hexndefs || lname == NAME_strfdefs)
    {
      gldefslist.append(Lump);
    }
  }

  for (auto &&lmpidx : gldefslist) {
    GCon->Logf(NAME_Init, "Parsing GZDoom light definitions '%s'", *W_FullLumpName(lmpidx));
    ParseGZDoomEffectDefs(lmpidx, new VScriptParser(W_FullLumpName(lmpidx), W_CreateLumpReaderNum(lmpidx)), ClassDefs);
  }

  // build known effects list, so we can properly apply replacements
  TMap<VStrCI, bool> knownClasses;
  for (auto &&cd : ClassDefs) knownClasses.put(cd.ClassName, true);

  // add effects to the classes
  for (int i = 0; i < ClassDefs.length(); ++i) {
    VTempClassEffects &CD = ClassDefs[i];
    VClass *Cls = VClass::FindClassNoCase(*CD.ClassName);
    if (Cls) {
      // get class replacement
      VClass *repl = Cls->GetReplacement();
      // check if we have separate definition for replacement class
      if (repl != Cls && !knownClasses.has(repl->GetName())) Cls = repl;
      //Cls = Cls->GetReplacement();
    } else {
      if (dbg_show_missing_classes) {
        if (CD.StaticLight.IsNotEmpty()) {
          GCon->Logf(NAME_Warning, "No such class `%s` for static light \"%s\"", *CD.ClassName, *CD.StaticLight);
        } else {
          GCon->Logf(NAME_Warning, "No such class `%s` for effect", *CD.ClassName);
        }
      }
      continue;
    }

    if (CD.StaticLight.IsNotEmpty()) {
      VLightEffectDef *SLight = R_FindLightEffect(CD.StaticLight);
      if (SLight) {
        Cls->SetFieldBool("bStaticLight", true);
        Cls->SetFieldInt("LightColor", SLight->Color);
        Cls->SetFieldFloat("LightRadius", SLight->Radius);
        Cls->SetFieldVec("LightOffset", SLight->Offset);
        if (SLight->Type&DLTYPE_Spot) {
          Cls->SetFieldFloat("LightConeAngle", SLight->ConeAngle);
          Cls->SetFieldVec("LightConeDir", SLight->ConeDir);
        } else {
          Cls->SetFieldFloat("LightConeAngle", 0);
          Cls->SetFieldVec("LightConeDir", TVec(0, 0, 0));
        }
      } else {
        GCon->Logf(NAME_Warning, "Light \"%s\" not found.", *CD.StaticLight);
      }
    }

    for (int j = 0; j < CD.SpriteEffects.length(); j++) {
      VTempSpriteEffectDef &SprDef = CD.SpriteEffects[j];
      // sprite name must be either 4 or 5 chars
      // 6-char is for '0' frames (not properly implemented yet, just a hack)
      if (SprDef.Sprite.Length() == 6) {
        if (SprDef.Sprite[5] != '0') {
          GCon->Logf(NAME_Warning, "Bad sprite name length '%s', sprite effects ignored.", *SprDef.Sprite);
          continue;
        }
        GCon->Logf(NAME_Warning, "Applying sprite effects from '%s' to all rotations.", *SprDef.Sprite);
      } else if (SprDef.Sprite.Length() != 4 && SprDef.Sprite.Length() != 5) {
        GCon->Logf(NAME_Warning, "Bad sprite name length '%s', sprite effects ignored.", *SprDef.Sprite);
        continue;
      }

      if (SprDef.Sprite.length() >= 5) {
        char ch = VStr::ToUpper(SprDef.Sprite[4]);
        if (ch < 'A' || ch-'A' > 36) {
          GCon->Logf(NAME_Warning, "Bad sprite frame in '%s', sprite effects ignored.", *SprDef.Sprite);
          continue;
        }
      }

      // find sprite index
      char SprName[8];
      SprName[0] = VStr::ToLower(SprDef.Sprite[0]);
      SprName[1] = VStr::ToLower(SprDef.Sprite[1]);
      SprName[2] = VStr::ToLower(SprDef.Sprite[2]);
      SprName[3] = VStr::ToLower(SprDef.Sprite[3]);
      SprName[4] = 0;
      int SprIdx = VClass::FindSprite(SprName, false);
      if (SprIdx == -1) {
        VName repl = (cli_SprDehTransfer ? GetDehReplacedSprite(VName(SprName, VName::FindLower)) : NAME_None);
        if (repl != NAME_None) {
          SprIdx = VClass::FindSprite(repl, false);
          if (SprIdx != -1) GCon->Logf(NAME_Debug, "Dehacked effect transfer from '%s' to '%s'", SprName, *repl);
        }
        if (SprIdx == -1) {
          GCon->Logf(NAME_Warning, "No such sprite '%s', sprite effects ignored.", SprName);
          continue;
        }
      }

      VSpriteEffect &SprFx = Cls->SpriteEffects.Alloc();
      SprFx.SpriteIndex = SprIdx;
      SprFx.Frame = (SprDef.Sprite.Length() == 4 ? -1 : VStr::ToUpper(SprDef.Sprite[4])-'A');
      SprFx.LightDef = nullptr;
      if (SprDef.Light.IsNotEmpty()) {
        SprFx.LightDef = R_FindLightEffect(SprDef.Light);
        if (!SprFx.LightDef) {
          GCon->Logf(NAME_Warning, "Light '%s' not found.", *SprDef.Light);
        }
        //if (VStr::strEquCI(SprName, "bar1")) GCon->Logf(NAME_Debug, "BAR1(%s): light=%s; color=0x%08x; radius=(%g,%g)", Cls->GetName(), *SprDef.Light, SprFx.LightDef->Color, SprFx.LightDef->Radius, SprFx.LightDef->Radius2);
      }
      SprFx.PartDef = nullptr;
      if (SprDef.Part.IsNotEmpty()) {
        SprFx.PartDef = FindParticleEffect(SprDef.Part);
        if (!SprFx.PartDef) {
          GCon->Logf(NAME_Warning, "Particle effect '%s' not found.", *SprDef.Part);
        }
      }
    }
  }
}
