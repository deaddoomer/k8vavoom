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

struct particle_t;
struct dlight_t;

//  Texture use types.
enum
{
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
};

//  Texture data formats.
enum
{
  TEXFMT_8,   //  Paletised texture in main palette.
  TEXFMT_8Pal,  //  Paletised texture with custom palette.
  TEXFMT_RGBA,  //  Truecolour texture.
};

struct rgb_t
{
  byte  r;
  byte  g;
  byte  b;
  rgb_t () : r(0), g(0), b(0) {}
  rgb_t (byte ar, byte ag, byte ab) : r(ar), g(ag), b(ab) {}
};

struct rgba_t
{
  byte  r;
  byte  g;
  byte  b;
  byte  a;

  rgba_t () : r(0), g(0), b(0), a(0) {}
  rgba_t (byte ar, byte ag, byte ab, byte aa=255) : r(ar), g(ag), b(ab), a(aa) {}
};

struct picinfo_t
{
  vint32    width;
  vint32    height;
  vint32    xoffset;
  vint32    yoffset;
};

struct TSwitchFrame
{
  vint16    Texture;
  vint16    BaseTime;
  vint16    RandomRange;
};

struct TSwitch
{
  vint16      Tex;
  vint16      PairIndex;
  vint16      Sound;
  vint16      NumFrames;
  TSwitchFrame* Frames;
  bool      Quest;

  TSwitch()
  : Frames(nullptr)
  {}
  ~TSwitch()
  {
    if (Frames)
    {
      delete[] Frames;
      Frames = nullptr;
    }
  }
};

struct VAnimDoorDef
{
  vint32    Texture;
  VName   OpenSound;
  VName   CloseSound;
  vint32    NumFrames;
  vint32*   Frames;
};

class VRenderLevelPublic : public VInterface
{
public:
  virtual void PreRender() = 0;
  virtual void SegMoved(seg_t*) = 0;
  virtual void SetupFakeFloors(sector_t*) = 0;
  virtual void RenderPlayerView() = 0;

  virtual void AddStaticLight(const TVec&, float, vuint32) = 0;
  virtual dlight_t* AllocDlight(VThinker*) = 0;
  virtual void DecayLights(float) = 0;

  virtual particle_t* NewParticle() = 0;
};

class VTextureTranslation
{
public:
  vuint8    Table[256];
  rgba_t    Palette[256];

  vuint16   Crc;

  //  Used to detect changed player translations.
  vuint8    TranslStart;
  vuint8    TranslEnd;
  vint32    Colour;

  //  Used to replicate translation tables in more efficient way.
  struct VTransCmd
  {
    vuint8  Type;
    vuint8  Start;
    vuint8  End;
    vuint8  R1;
    vuint8  G1;
    vuint8  B1;
    vuint8  R2;
    vuint8  G2;
    vuint8  B2;
  };
  TArray<VTransCmd> Commands;

  VTextureTranslation();
  void Clear();
  void CalcCrc();
  void Serialise(VStream&);
  void BuildPlayerTrans(int, int, int);
  void MapToRange(int, int, int, int);
  void MapToColours(int, int, int, int, int, int, int, int);
  void BuildBloodTrans(int);
  void AddTransString(const VStr&);

  const vuint8* GetTable() const
  {
    return Table;
  }
  const rgba_t* GetPalette() const
  {
    return Palette;
  }
};


class VTexture {
public:
  int Type;
  int Format;
  VName Name;
  int Width;
  int Height;
  int SOffset;
  int TOffset;
  bool bNoRemap0;
  bool bWorldPanning;
  bool bIsCameraTexture;
  vuint8 WarpType;
  float SScale; // scaling
  float TScale;
  int TextureTranslation; // animation
  int HashNext;
  int SourceLump;

  bool noDecals;

  // driver data
  struct VTransData {
    union {
      vuint32 Handle;
      void *Data;
    };
    VTextureTranslation *Trans;
    int ColourMap;
  };

  union {
    vuint32 DriverHandle;
    void *DriverData;
  };
  TArray<VTransData> DriverTranslated;

protected:
  vuint8 *Pixels8Bit;
  VTexture *HiResTexture;
  bool Pixels8BitValid;

protected:
  static void checkerFill8 (vuint8 *dest, int width, int height);
  static void checkerFillRGB (vuint8 *dest, int width, int height);
  static void checkerFillRGBA (vuint8 *dest, int width, int height);

  // `dest` points at column, `x` is used only to build checker
  static void checkerFillColumn8 (vuint8 *dest, int x, int pitch, int height);

public:
  VTexture ();
  virtual ~VTexture ();

  static VTexture *CreateTexture (int, int);

  int GetWidth () const { return Width; }
  int GetHeight () const { return Height; }

  int GetScaledWidth () const { return (int)(Width / SScale); }
  int GetScaledHeight () const { return (int)(Height / TScale); }

  int GetScaledSOffset () const { return (int)(SOffset / SScale); }
  int GetScaledTOffset () const { return (int)(TOffset / TScale); }

  // get texture pixel; will call `GetPixels()`
  rgba_t getPixel (int x, int y);

  virtual void SetFrontSkyLayer ();
  virtual bool CheckModified ();
  virtual vuint8 *GetPixels () = 0;
  vuint8 *GetPixels8 ();
  virtual rgba_t *GetPalette ();
  virtual void Unload () = 0;
  virtual VTexture *GetHighResolutionTexture ();
  VTransData *FindDriverTrans (VTextureTranslation *, int);

  static void AdjustGamma (rgba_t *, int);
  static void SmoothEdges (vuint8 *, int, int, vuint8 *);
  static void ResampleTexture (int, int, const vuint8 *, int, int, vuint8 *, int);
  static void MipMap (int, int, vuint8 *);

protected:
  void FixupPalette (vuint8 *Pixels, rgba_t *Palette);
};


class VTextureManager
{
public:
  vint32        DefaultTexture;
  float       Time; //  Time value for warp textures

  VTextureManager();
  void Init();
  void Shutdown();
  int AddTexture(VTexture* Tex);
  void ReplaceTexture(int Index, VTexture* Tex);
  int CheckNumForName(VName Name, int Type, bool bOverload = false,
    bool bCheckAny = false);
  int NumForName(VName Name, int Type, bool bOverload = false,
    bool bCheckAny = false);
  int FindTextureByLumpNum(int);
  VName GetTextureName(int TexNum);
  float TextureWidth(int TexNum);
  float TextureHeight(int TexNum);
  int TextureAnimation(int InTex);
  void SetFrontSkyLayer(int tex);
  void GetTextureInfo(int TexNum, picinfo_t* info);
  int AddPatch(VName Name, int Type, bool Silent = false);
  int AddRawWithPal(VName Name, VName PalName);
  int AddFileTexture(VName Name, int Type);

  //  Get unanimated texture
  VTexture* operator[](int TexNum)
  {
    if ((vuint32)TexNum >= (vuint32)Textures.Num())
    {
      return nullptr;
    }
    return Textures[TexNum];
  }

  //  Get animated texture
  VTexture* operator()(int TexNum)
  {
    if ((vuint32)TexNum >= (vuint32)Textures.Num())
    {
      return nullptr;
    }
    return Textures[TextureAnimation(TexNum)];
  }

  int GetNumTextures() const
  {
    return Textures.Num();
  }

private:
  enum { HASH_SIZE = 1024 };

  TArray<VTexture*> Textures;
  int         TextureHash[HASH_SIZE];

  void AddToHash(int Index);
  void RemoveFromHash(int Index);
  void AddTextures();
  void AddTexturesLump(int, int, int, bool);
  void AddGroup(int, EWadNamespace);
  void AddHiResTextures();
};

// r_data
void R_InitData();
void R_ShutdownData();
void R_InstallSprite(const char*, int);
bool R_AreSpritesPresent(int);
int R_ParseDecorateTranslation(VScriptParser*, int);
int R_GetBloodTranslation(int);
void R_ParseEffectDefs();

// r_main
void R_Init(); // Called by startup code.
void R_Start(VLevel*);
void R_SetViewSize(int blocks);
void R_RenderPlayerView();
VTextureTranslation* R_GetCachedTranslation(int, VLevel*);

// r_tex
void R_InitTexture();
void R_ShutdownTexture();
VAnimDoorDef* R_FindAnimDoor(vint32);
void R_AnimateSurfaces();

// r_things
void R_DrawSpritePatch(int, int, int, int, int, int = 0, int = 0, int = 0);
void R_InitSprites();

//  2D graphics
void R_DrawPic(int x, int y, int handle, float Aplha = 1.0);

extern VTextureManager  GTextureManager;

extern int        validcount;

extern int        skyflatnum;

//  Switches
extern TArray<TSwitch*> Switches;
