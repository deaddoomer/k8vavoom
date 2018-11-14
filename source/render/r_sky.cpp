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
//**  Sky rendering. The DOOM sky is a texture map like any wall, wrapping
//**  around. A 1024 columns equal 360 degrees. The default sky map is 256
//**  columns and repeats 4 times on a 320 screen
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "gamedefs.h"
#include "r_local.h"

// MACROS ------------------------------------------------------------------

#define RADIUS    128.0

// TYPES -------------------------------------------------------------------

struct skyboxinfo_t
{
  struct skyboxsurf_t
  {
    int     texture;
  };

  VName     Name;
  skyboxsurf_t  surfs[6];
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static TArray<skyboxinfo_t>   skyboxinfo;

static VCvarB   r_skyboxes("r_skyboxes", true, "Allow skyboxes?", CVAR_Archive);

// CODE --------------------------------------------------------------------

//==========================================================================
//
//  ParseSkyBoxesScript
//
//==========================================================================

static void ParseSkyBoxesScript(VScriptParser *sc)
{
  guard(ParseSkyBoxesScript);
  while (!sc->AtEnd())
  {
    skyboxinfo_t &info = skyboxinfo.Alloc();
    memset((void *)&info, 0, sizeof(info));

    sc->ExpectString();
    info.Name = *sc->String;
    sc->Expect("{");
    for (int i = 0; i < 6; i++)
    {
      sc->Expect("{");
      sc->Expect("map");
      sc->ExpectString();
      info.surfs[i].texture = GTextureManager.AddFileTexture(
        VName(*sc->String), TEXTYPE_SkyMap);
      sc->Expect("}");
    }
    sc->Expect("}");
  }
  delete sc;
  sc = nullptr;
  unguard;
}


//==========================================================================
//
//  ParseMapDefSkyBoxesScript
//
//  TODO: https://zdoom.org/wiki/GLDEFS#Skybox_definitions
//  TODO: "fliptop"
//
//==========================================================================
void R_ParseMapDefSkyBoxesScript (VScriptParser *sc) {
  skyboxinfo_t &info = skyboxinfo.Alloc();
  memset((void *)&info, 0, sizeof(info));
  sc->ExpectString();
  info.Name = *sc->String;
  GCon->Logf("MSG: found gz skybox '%s'", *info.Name);
  if (sc->Check("fliptop")) GCon->Logf("MSG: gz skybox '%s' require fliptop, which is not supported by VaVoom yet", *info.Name);
  VStr txnames[6];
  int txcount = 0;
  bool gotSq = false;
  sc->Expect("{");
  while (txcount < 6) {
    if (sc->Check("}")) { gotSq = true; break; }
    sc->ExpectString();
    txnames[txcount++] = sc->String;
  }
  if (!gotSq) sc->Expect("}");
  if (txcount != 3 && txcount != 6) sc->Error(va("Invalid skybox '%s' (%d textures found)", *info.Name, txcount));
  if (txcount == 3) {
    for (int i = 0; i < 4; ++i) info.surfs[i].texture = GTextureManager.AddFileTexture(VName(*txnames[0]), TEXTYPE_SkyMap);
    info.surfs[4].texture = GTextureManager.AddFileTexture(VName(*txnames[1]), TEXTYPE_SkyMap);
    info.surfs[5].texture = GTextureManager.AddFileTexture(VName(*txnames[2]), TEXTYPE_SkyMap);
  } else {
    // full
    for (int i = 0; i < 6; ++i) info.surfs[i].texture = GTextureManager.AddFileTexture(VName(*txnames[i]), TEXTYPE_SkyMap);
  }
}


//==========================================================================
//
//  R_HasNamedSkybox
//
//==========================================================================
VName R_HasNamedSkybox (const VStr &aname) {
  if (aname.length() == 0) return NAME_None;
  for (int f = 0; f < skyboxinfo.length(); ++f) {
    if (aname.ICmp(*skyboxinfo[f].Name) == 0) return skyboxinfo[f].Name;
  }
  return NAME_None;
}


//==========================================================================
//
//  R_InitSkyBoxes
//
//==========================================================================

void R_InitSkyBoxes()
{
  guard(R_InitSkyBoxes);
  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0;
    Lump = W_IterateNS(Lump, WADNS_Global))
  {
    if (W_LumpName(Lump) == NAME_skyboxes)
    {
      ParseSkyBoxesScript(new VScriptParser(/*"skyboxes"*/W_FullLumpName(Lump),
        W_CreateLumpReaderNum(Lump)));
    }
  }
  //  Optionally parse script file.
  /*
  if (fl_devmode && FL_FileExists("scripts/skyboxes.txt"))
  {
    ParseSkyBoxesScript(new VScriptParser("scripts/skyboxes.txt",
      FL_OpenFileRead("scripts/skyboxes.txt")));
  }
  */
  unguard;
}


#ifdef CLIENT
//==========================================================================
//
//  CheckSkyboxNumForName
//
//==========================================================================

static int CheckSkyboxNumForName(VName Name)
{
  guard(CheckSkyboxNumForName);
  for (int num = skyboxinfo.Num() - 1; num >= 0; num--)
  {
    if (skyboxinfo[num].Name == Name)
    {
      return num;
    }
  }
  return -1;
  unguard;
}

//==========================================================================
//
//  R_FreeSkyboxData
//
//==========================================================================

void R_FreeSkyboxData()
{
  guard(R_FreeSkyboxData);
  skyboxinfo.Clear();
  unguard;
}


//==========================================================================
//
//  VSky::InitOldSky
//
//==========================================================================

void VSky::InitOldSky(int Sky1Texture, int Sky2Texture, float Sky1ScrollDelta,
  float Sky2ScrollDelta, bool DoubleSky, bool ForceNoSkyStretch, bool Flip)
{
  guard(VSky::InitOldSky);
  memset((void *)sky, 0, sizeof(sky));
  bIsSkyBox = false;

  int skyheight = GTextureManager[Sky1Texture]->GetHeight();
  if (ForceNoSkyStretch)
  {
    skyheight = 256;
  }
  float skytop;
//  float skybot;
  int j;

  if (skyheight <= 128)
  {
    skytop = 95;
  }
  else
  {
    skytop = 190;
  }
//  skybot = skytop - skyheight;
  int skyh = (int)skytop;

  for (j = 0; j < VDIVS; j++)
  {
    float va0 = 90.0 - j * (180.0 / VDIVS);
    float va1 = 90.0 - (j + 1) * (180.0 / VDIVS);
    float vsina0 = msin(va0);
    float vcosa0 = mcos(va0);
    float vsina1 = msin(va1);
    float vcosa1 = mcos(va1);

//    float theight = skytop;
//    float bheight = skybot;
    float theight = vsina0 * RADIUS;
    float bheight = vsina1 * RADIUS;
//    float tradius = RADIUS;
//    float vradius = RADIUS;
    float tradius = vcosa0 * RADIUS;
    float vradius = vcosa1 * RADIUS;
    for (int i = 0; i < HDIVS; i++)
    {
      sky_t &s = sky[j * HDIVS + i];
      float a0 = 45 - i * (360.0 / HDIVS);
      float a1 = 45 - (i + 1) * (360.0 / HDIVS);
      float sina0 = msin(a0);
      float cosa0 = mcos(a0);
      float sina1 = msin(a1);
      float cosa1 = mcos(a1);

      TVec *surfverts = &s.surf.verts[0]; //k8: cache it, and silence compiler warnings
      surfverts[0] = TVec(cosa0 * vradius, sina0 * vradius, bheight);
      surfverts[1] = TVec(cosa0 * tradius, sina0 * tradius, theight);
      surfverts[2] = TVec(cosa1 * tradius, sina1 * tradius, theight);
      surfverts[3] = TVec(cosa1 * vradius, sina1 * vradius, bheight);

      TVec hdir = j < VDIVS / 2 ? surfverts[3] - surfverts[0] :
        surfverts[2] - surfverts[1];
      TVec vdir = surfverts[0] - surfverts[1];
      TVec normal = Normalise(CrossProduct(vdir, hdir));
      s.plane.Set(normal, DotProduct(surfverts[1], normal));

      s.texinfo.saxis = hdir * (1024 / HDIVS / DotProduct(hdir, hdir));
      float tk = skyh / RADIUS;
      s.texinfo.taxis = TVec(0, 0, -tk);
      s.texinfo.soffs = -DotProduct(s.surf.verts[j < VDIVS / 2 ? 0 : 1],
        s.texinfo.saxis);
      s.texinfo.toffs = skyh;

      s.columnOffset1 = s.columnOffset2 = -i * (1024 / HDIVS) + 128;

      float mins;
      float maxs;

      if (Flip)
      {
        s.texinfo.saxis = -s.texinfo.saxis;
        s.texinfo.soffs = -s.texinfo.soffs;
        s.columnOffset1 = -s.columnOffset1;
        s.columnOffset2 = -s.columnOffset2;

        mins = DotProduct(surfverts[j < VDIVS / 2 ? 3 : 2],
          s.texinfo.saxis) + s.texinfo.soffs;
        maxs = DotProduct(surfverts[j < VDIVS / 2 ? 0 : 1],
          s.texinfo.saxis) + s.texinfo.soffs;
      }
      else
      {
        mins = DotProduct(surfverts[j < VDIVS / 2 ? 0 : 1],
          s.texinfo.saxis) + s.texinfo.soffs;
        maxs = DotProduct(surfverts[j < VDIVS / 2 ? 3 : 2],
          s.texinfo.saxis) + s.texinfo.soffs;
      }

      int bmins = (int)floor(mins / 16);
      int bmaxs = (int)ceil(maxs / 16);
      s.surf.texturemins[0] = bmins * 16;
      s.surf.extents[0] = (bmaxs - bmins) * 16;
      //s.surf.extents[0] = 256;
      mins = DotProduct(surfverts[1], s.texinfo.taxis) + s.texinfo.toffs;
      maxs = DotProduct(surfverts[0], s.texinfo.taxis) + s.texinfo.toffs;
      bmins = (int)floor(mins / 16);
      bmaxs = (int)ceil(maxs / 16);
      s.surf.texturemins[1] = bmins * 16;
      s.surf.extents[1] = (bmaxs - bmins) * 16;
      //s.surf.extents[1] = skyh;
    }
  }

  NumSkySurfs = VDIVS * HDIVS;

  for (j = 0; j < NumSkySurfs; j++)
  {
    if (DoubleSky)
    {
      sky[j].texture1 = Sky2Texture;
      sky[j].texture2 = Sky1Texture;
      sky[j].scrollDelta1 = Sky2ScrollDelta;
      sky[j].scrollDelta2 = Sky1ScrollDelta;
    }
    else
    {
      sky[j].texture1 = Sky1Texture;
      sky[j].scrollDelta1 = Sky1ScrollDelta;
    }
    sky[j].surf.plane = &sky[j].plane;
    sky[j].surf.texinfo = &sky[j].texinfo;
    sky[j].surf.count = 4;
    sky[j].surf.Light = 0xffffffff;
  }

  //  Precache textures
  Drawer->PrecacheTexture(GTextureManager[Sky1Texture]);
  Drawer->PrecacheTexture(GTextureManager[Sky2Texture]);
  unguard;
}

//==========================================================================
//
//  VSky::InitSkyBox
//
//==========================================================================

void VSky::InitSkyBox(VName Name1, VName Name2)
{
  guard(VSky::InitSkyBox);
  int num = CheckSkyboxNumForName(Name1);
  if (num == -1)
  {
    Host_Error("No such skybox %s", *Name1);
  }
  skyboxinfo_t &s1info = skyboxinfo[num];
  if (Name2 != NAME_None)
  {
    num = CheckSkyboxNumForName(Name2);
    if (num == -1)
    {
      Host_Error("No such skybox %s", *Name2);
    }
  }
//  skyboxinfo_t &s2info = skyboxinfo[num];

  memset((void *)sky, 0, sizeof(sky));

  sky[0].surf.verts[0] = TVec(128, 128, -128);
  sky[0].surf.verts[1] = TVec(128, 128, 128);
  sky[0].surf.verts[2] = TVec(128, -128, 128);
  sky[0].surf.verts[3] = TVec(128, -128, -128);

  sky[1].surf.verts[0] = TVec(128, -128, -128);
  sky[1].surf.verts[1] = TVec(128, -128, 128);
  sky[1].surf.verts[2] = TVec(-128, -128, 128);
  sky[1].surf.verts[3] = TVec(-128, -128, -128);

  sky[2].surf.verts[0] = TVec(-128, -128, -128);
  sky[2].surf.verts[1] = TVec(-128, -128, 128);
  sky[2].surf.verts[2] = TVec(-128, 128, 128);
  sky[2].surf.verts[3] = TVec(-128, 128, -128);

  sky[3].surf.verts[0] = TVec(-128, 128, -128);
  sky[3].surf.verts[1] = TVec(-128, 128, 128);
  sky[3].surf.verts[2] = TVec(128, 128, 128);
  sky[3].surf.verts[3] = TVec(128, 128, -128);

  sky[4].surf.verts[0] = TVec(128.0, 128.0, 128);
  sky[4].surf.verts[1] = TVec(-128.0, 128.0, 128);
  sky[4].surf.verts[2] = TVec(-128.0, -128.0, 128);
  sky[4].surf.verts[3] = TVec(128.0, -128.0, 128);

  sky[5].surf.verts[0] = TVec(128, 128, -128);
  sky[5].surf.verts[1] = TVec(128, -128, -128);
  sky[5].surf.verts[2] = TVec(-128, -128, -128);
  sky[5].surf.verts[3] = TVec(-128, 128, -128);

  sky[0].plane.Set(TVec(-1, 0, 0), -128);
  sky[0].texinfo.saxis = TVec(0, -1.0, 0);
  sky[0].texinfo.taxis = TVec(0, 0, -1.0);
  sky[0].texinfo.soffs = 128;
  sky[0].texinfo.toffs = 128;

  sky[1].plane.Set(TVec(0, 1, 0), -128);
  sky[1].texinfo.saxis = TVec(-1.0, 0, 0);
  sky[1].texinfo.taxis = TVec(0, 0, -1.0);
  sky[1].texinfo.soffs = 128;
  sky[1].texinfo.toffs = 128;

  sky[2].plane.Set(TVec(1, 0, 0), -128);
  sky[2].texinfo.saxis = TVec(0, 1.0, 0);
  sky[2].texinfo.taxis = TVec(0, 0, -1.0);
  sky[2].texinfo.soffs = 128;
  sky[2].texinfo.toffs = 128;

  sky[3].plane.Set(TVec(0, -1, 0), -128);
  sky[3].texinfo.saxis = TVec(1.0, 0, 0);
  sky[3].texinfo.taxis = TVec(0, 0, -1.0);
  sky[3].texinfo.soffs = 128;
  sky[3].texinfo.toffs = 128;

  sky[4].plane.Set(TVec(0, 0, -1), -128);
  sky[4].texinfo.saxis = TVec(0, -1.0, 0);
  sky[4].texinfo.taxis = TVec(1.0, 0, 0);
  sky[4].texinfo.soffs = 128;
  sky[4].texinfo.toffs = 128;

  sky[5].plane.Set(TVec(0, 0, 1), -128);
  sky[5].texinfo.saxis = TVec(0, -1.0, 0);
  sky[5].texinfo.taxis = TVec(-1.0, 0, 0);
  sky[5].texinfo.soffs = 128;
  sky[5].texinfo.toffs = 128;

  NumSkySurfs = 6;
  for (int j = 0; j < 6; j++)
  {
    sky[j].texture1 = s1info.surfs[j].texture;
    sky[j].surf.plane = &sky[j].plane;
    sky[j].surf.texinfo = &sky[j].texinfo;
    sky[j].surf.count = 4;
    sky[j].surf.Light = 0xffffffff;

    //  Precache texture
    Drawer->PrecacheTexture(GTextureManager[sky[j].texture1]);

    VTexture *STex = GTextureManager[sky[j].texture1];

    sky[j].surf.extents[0] = STex->GetWidth();
    sky[j].surf.extents[1] = STex->GetHeight();
    sky[j].texinfo.saxis *= STex->GetWidth() / 256.0;
    sky[j].texinfo.soffs *= STex->GetWidth() / 256.0;
    sky[j].texinfo.taxis *= STex->GetHeight() / 256.0;
    sky[j].texinfo.toffs *= STex->GetHeight() / 256.0;
  }
  bIsSkyBox = true;
  unguard;
}

//==========================================================================
//
//  VSky::Init
//
//==========================================================================

void VSky::Init(int Sky1Texture, int Sky2Texture, float Sky1ScrollDelta,
  float Sky2ScrollDelta, bool DoubleSky, bool ForceNoSkyStretch,
  bool Flip, bool Lightning)
{
  guard(VSky::Init);
  int Num1 = -1;
  int Num2 = -1;
  VName Name1(NAME_None);
  VName Name2(NAME_None);
  //  Check if we want to replace old sky with a skybox. We can't do
  // this if level is using double sky or it's scrolling.
  //GCon->Logf("VSky::Init: t1=%d; t2=%d; double=%s; s1scroll=%f", Sky1Texture, Sky2Texture, (DoubleSky ? "tan" : "ona"), Sky1ScrollDelta);
  if (r_skyboxes && !DoubleSky && !Sky1ScrollDelta)
  {
    Name1 = GTextureManager[Sky1Texture]->Name;
    Name2 = Lightning ? GTextureManager[Sky2Texture]->Name : Name1;
    Num1 = CheckSkyboxNumForName(Name1);
    Num2 = CheckSkyboxNumForName(Name2);
    //GCon->Logf("VSky::Init: name1='%s'; name2='%s'; num1=%d; num2=%d", *Name1, *Name2, Num1, Num2);
  }
  if (Num1 != -1 && Num2 != -1 /*&& Name1 != "-noflat-" && Name2 != "-noflat"*/)
  {
    GCon->Logf("VSky:Init: creating skybox (%s:%s)", *Name1, *Name2);
    InitSkyBox(Name1, Name2);
  }
  else
  {
    InitOldSky(Sky1Texture, Sky2Texture, Sky1ScrollDelta, Sky2ScrollDelta,
      DoubleSky, ForceNoSkyStretch, Flip);
  }
  SideTex = Sky1Texture;
  SideFlip = Flip;
  unguard;
}

//==========================================================================
//
//  VSky::Draw
//
//==========================================================================

void VSky::Draw(int ColourMap)
{
  guard(VSky::Draw);
  for (int i = 0; i < NumSkySurfs; i++)
  {
    Drawer->DrawSkyPolygon(&sky[i].surf, bIsSkyBox,
      GTextureManager(sky[i].texture1), sky[i].columnOffset1,
      GTextureManager(sky[i].texture2), sky[i].columnOffset2,
      ColourMap);
  }
  unguard;
}

//==========================================================================
//
//  VRenderLevelShared::InitSky
//
//  Called at level load.
//
//==========================================================================

void VRenderLevelShared::InitSky()
{
  guard(VRenderLevelShared::InitSky);
  if (CurrentSky1Texture == Level->LevelInfo->Sky1Texture &&
    CurrentSky2Texture == Level->LevelInfo->Sky2Texture &&
    CurrentDoubleSky == !!(Level->LevelInfo->LevelInfoFlags & VLevelInfo::LIF_DoubleSky) &&
    CurrentLightning == !!(Level->LevelInfo->LevelInfoFlags & VLevelInfo::LIF_Lightning))
  {
    return;
  }
  CurrentSky1Texture = Level->LevelInfo->Sky1Texture;
  CurrentSky2Texture = Level->LevelInfo->Sky2Texture;
  CurrentDoubleSky = !!(Level->LevelInfo->LevelInfoFlags & VLevelInfo::LIF_DoubleSky);
  CurrentLightning = !!(Level->LevelInfo->LevelInfoFlags & VLevelInfo::LIF_Lightning);

  if (Level->LevelInfo->SkyBox != NAME_None)
  {
    BaseSky.InitSkyBox(Level->LevelInfo->SkyBox, NAME_None);
  }
  else
  {
    BaseSky.Init(CurrentSky1Texture, CurrentSky2Texture,
      Level->LevelInfo->Sky1ScrollDelta,
      Level->LevelInfo->Sky2ScrollDelta, CurrentDoubleSky,
      !!(Level->LevelInfo->LevelInfoFlags &
      VLevelInfo::LIF_ForceNoSkyStretch), true, CurrentLightning);
  }
  unguard;
}

//==========================================================================
//
//  VRenderLevelShared::AnimateSky
//
//==========================================================================

void VRenderLevelShared::AnimateSky(float frametime)
{
  guard(VRenderLevelShared::AnimateSky);
  InitSky();

  if (!(Level->LevelInfo->LevelInfoFlags2 & VLevelInfo::LIF2_Frozen))
  {
    //  Update sky column offsets
    for (int i = 0; i < BaseSky.NumSkySurfs; i++)
    {
      BaseSky.sky[i].columnOffset1 += BaseSky.sky[i].scrollDelta1 * frametime;
      BaseSky.sky[i].columnOffset2 += BaseSky.sky[i].scrollDelta2 * frametime;
    }
  }
  unguard;
}
#endif

