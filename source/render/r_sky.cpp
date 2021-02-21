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
//**  Sky rendering. The DOOM sky is a texture map like any wall, wrapping
//**  around. A 1024 columns equal 360 degrees. The default sky map is 256
//**  columns and repeats 4 times on a 320 screen
//**
//**************************************************************************
#include "../gamedefs.h"
#include "r_local.h"

#define RADIUS  (128.0f)


struct skyboxinfo_t {
  struct skyboxsurf_t {
    int texture;
  };

  VName Name;
  skyboxsurf_t surfs[6];
  bool isgozzo;
  bool fliptop;

  // convert gozzo index to proper Vavoom index
  static inline int GozzoToVavoom (int idx) noexcept {
    if (idx >= 0 && idx <= 3) return (idx+3)&3;
    return idx;
  }
};


static TArray<skyboxinfo_t> skyboxinfo;

static VCvarB r_skyboxes("r_skyboxes", true, "Allow skyboxes?", CVAR_Archive);


//==========================================================================
//
//  ParseSkyBoxesScript
//
//==========================================================================
static void ParseSkyBoxesScript (VScriptParser *sc) {
  while (!sc->AtEnd()) {
    skyboxinfo_t &info = skyboxinfo.Alloc();
    memset((void *)&info, 0, sizeof(info));
    sc->ExpectString();
    info.Name = *sc->String;
    sc->Expect("{");
    for (int i = 0; i < 6; ++i) {
      sc->Expect("{");
      sc->Expect("map");
      sc->ExpectString();
      info.surfs[i].texture = GTextureManager.AddFileTexture(VName(*sc->String), TEXTYPE_SkyMap);
      sc->Expect("}");
    }
    sc->Expect("}");
  }
  delete sc;
}


//==========================================================================
//
//  R_ParseGLDefSkyBoxesScript
//
//  https://zdoom.org/wiki/GLDEFS#Skybox_definitions
//  TODO: fliptop
//
//==========================================================================
void R_ParseGLDefSkyBoxesScript (VScriptParser *sc) {
  skyboxinfo_t &info = skyboxinfo.Alloc();
  memset((void *)&info, 0, sizeof(info));
  info.isgozzo = true;
  info.fliptop = false;
  sc->ExpectString();
  info.Name = *sc->String;
  //GCon->Logf("MSG: found gz skybox '%s'", *info.Name);
  if (sc->Check("fliptop")) {
    //GCon->Logf(NAME_Warning, "gz skybox '%s' requires fliptop, which is not supported by k8vavoom yet", *info.Name);
    info.fliptop = true;
  }
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
    // short; copy top and bottom
    txnames[4] = txnames[1];
    txnames[5] = txnames[2];
    // fill other
    for (int f = 1; f < 4; ++f) txnames[f] = txnames[0];
  }
  // create skybox textures
  for (int f = 0; f < 6; ++f) {
    const int tidx = skyboxinfo_t::GozzoToVavoom(f);
    info.surfs[tidx].texture = GTextureManager.AddFileTexture(VName(*txnames[f]), TEXTYPE_SkyMap);
  }
}


//==========================================================================
//
//  R_HasNamedSkybox
//
//==========================================================================
VName R_HasNamedSkybox (VStr aname) {
  if (aname.isEmpty()) return NAME_None;
  for (auto &&sb : skyboxinfo) {
    if (aname.strEquCI(*sb.Name)) return sb.Name;
  }
  return NAME_None;
}


//==========================================================================
//
//  R_InitSkyBoxes
//
//==========================================================================
void R_InitSkyBoxes () {
  VName skb = VName("skybox", VName::FindLower);
  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    if (W_LumpName(Lump) == NAME_skyboxes) {
      GCon->Logf(NAME_Init, "parsing skybox script '%s'", *W_FullLumpName(Lump));
      ParseSkyBoxesScript(new VScriptParser(W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump)));
    } else  if (skb != NAME_None && W_LumpName(Lump) == skb) {
      GCon->Logf(NAME_Init, "parsing gz skybox script '%s'", *W_FullLumpName(Lump));
      VScriptParser *sc = new VScriptParser(W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump));
      for (;;) {
        if (!sc->GetString()) break;
        sc->UnGet();
        sc->Expect("skybox");
        R_ParseGLDefSkyBoxesScript(sc); // as gozzo
      }
      delete sc;
    }
  }
}


#ifdef CLIENT
//==========================================================================
//
//  CheckSkyboxNumForName
//
//==========================================================================
static int CheckSkyboxNumForName (VName Name) {
  if (Name == NAME_None) return -1;
  for (int num = skyboxinfo.length()-1; num >= 0; --num) {
    if (VStr::strEquCI(*skyboxinfo[num].Name, *Name)) return num;
  }
  return -1;
}


//==========================================================================
//
//  R_FreeSkyboxData
//
//==========================================================================
void R_FreeSkyboxData () {
  skyboxinfo.clear();
}



//==========================================================================
//
//  VSky::InitOldSky
//
//==========================================================================
void VSky::InitOldSky (int Sky1Texture, int Sky2Texture,
                       float Sky1ScrollDelta, float Sky2ScrollDelta,
                       bool DoubleSky, bool ForceNoSkyStretch, bool Flip)
{
  memset((void *)sky, 0, sizeof(sky));
  bIsSkyBox = false;

  int skyheight = GTextureManager[Sky1Texture]->GetScaledHeight();
  if (ForceNoSkyStretch) skyheight = 256;
  //GCon->Logf(NAME_Debug, "VSky::InitOldSky: tex=%d; height=%d; scaledheight=%d; stretch=%d", Sky1Texture, GTextureManager[Sky1Texture]->GetHeight(), GTextureManager[Sky1Texture]->GetScaledHeight(), (int)ForceNoSkyStretch);

  const float skytop = (skyheight <= 128 ? 95.0f : 190.0f);
  //const float skybot = skytop-skyheight;
  const int skyh = (int)skytop;

  for (int j = 0; j < VDIVS; ++j) {
    const float va0 = 90.0f-j*(180.0f/VDIVS);
    float vsina0, vcosa0;
    msincos(va0, &vsina0, &vcosa0);

    const float va1 = 90.0f-(j+1)*(180.0f/VDIVS);
    float vsina1, vcosa1;
    msincos(va1, &vsina1, &vcosa1);

    //const float theight = skytop;
    //const float bheight = skybot;
    const float theight = vsina0*RADIUS;
    const float bheight = vsina1*RADIUS;
    //const float tradius = RADIUS;
    //const float vradius = RADIUS;
    const float tradius = vcosa0*RADIUS;
    const float vradius = vcosa1*RADIUS;
    for (int i = 0; i < HDIVS; ++i) {
      sky_t &s = sky[j*HDIVS+i];
      const float a0 = 45-i*(360.0f/HDIVS);
      float sina0, cosa0;
      msincos(a0, &sina0, &cosa0);

      const float a1 = 45-(i+1)*(360.0f/HDIVS);
      float sina1, cosa1;
      msincos(a1, &sina1, &cosa1);

      SurfVertex *surfverts = &s.surf.verts[0]; //k8: cache it, and silence compiler warnings
      surfverts[0].setVec(cosa0*vradius, sina0*vradius, bheight);
      surfverts[1].setVec(cosa0*tradius, sina0*tradius, theight);
      surfverts[2].setVec(cosa1*tradius, sina1*tradius, theight);
      surfverts[3].setVec(cosa1*vradius, sina1*vradius, bheight);

      TVec hdir = (j < VDIVS/2 ? surfverts[3].vec()-surfverts[0].vec() : surfverts[2].vec()-surfverts[1].vec());
      TVec vdir = surfverts[0].vec()-surfverts[1].vec();
      TVec normal = Normalise(CrossProduct(vdir, hdir));
      s.plane.Set(normal, DotProduct(surfverts[1].vec(), normal));

      s.texinfo.saxis = hdir*(1024/HDIVS/DotProduct(hdir, hdir));
      const float tk = skyh/RADIUS;
      s.texinfo.taxis = TVec(0, 0, -tk);
      s.texinfo.soffs = -DotProduct(s.surf.verts[j < VDIVS/2 ? 0 : 1].vec(), s.texinfo.saxis);
      s.texinfo.toffs = skyh;

      s.columnOffset1 = s.columnOffset2 = -i*(1024/HDIVS)+128;

      float mins, maxs;

      if (Flip) {
        s.texinfo.saxis = -s.texinfo.saxis;
        s.texinfo.soffs = -s.texinfo.soffs;
        s.columnOffset1 = -s.columnOffset1;
        s.columnOffset2 = -s.columnOffset2;

        mins = DotProduct(surfverts[j < VDIVS/2 ? 3 : 2].vec(), s.texinfo.saxis)+s.texinfo.soffs;
        maxs = DotProduct(surfverts[j < VDIVS/2 ? 0 : 1].vec(), s.texinfo.saxis)+s.texinfo.soffs;
      } else {
        mins = DotProduct(surfverts[j < VDIVS/2 ? 0 : 1].vec(), s.texinfo.saxis)+s.texinfo.soffs;
        maxs = DotProduct(surfverts[j < VDIVS/2 ? 3 : 2].vec(), s.texinfo.saxis)+s.texinfo.soffs;
      }

      int bmins = (int)floorf(mins/16.0f);
      int bmaxs = (int)ceilf(maxs/16.0f);
      s.surf.texturemins[0] = bmins*16;
      s.surf.extents[0] = (bmaxs-bmins)*16;
      //s.surf.extents[0] = 256;
      mins = DotProduct(surfverts[1].vec(), s.texinfo.taxis)+s.texinfo.toffs;
      maxs = DotProduct(surfverts[0].vec(), s.texinfo.taxis)+s.texinfo.toffs;

      bmins = (int)floorf(mins/16.0f);
      bmaxs = (int)ceilf(maxs/16.0f);
      s.surf.texturemins[1] = bmins*16;
      s.surf.extents[1] = (bmaxs-bmins)*16;
      //s.surf.extents[1] = skyh;
    }
  }

  NumSkySurfs = VDIVS*HDIVS;

  for (int j = 0; j < NumSkySurfs; ++j) {
    if (DoubleSky) {
      sky[j].texture1 = Sky2Texture;
      sky[j].texture2 = Sky1Texture;
      sky[j].scrollDelta1 = Sky2ScrollDelta;
      sky[j].scrollDelta2 = Sky1ScrollDelta;
    } else {
      sky[j].texture1 = Sky1Texture;
      sky[j].scrollDelta1 = Sky1ScrollDelta;
    }
    sky[j].surf.plane = sky[j].plane;
    sky[j].surf.texinfo = &sky[j].texinfo;
    sky[j].surf.count = 4;
    sky[j].surf.Light = 0xffffffff;
  }

  // precache textures
  Drawer->PrecacheTexture(GTextureManager[Sky1Texture]);
  Drawer->PrecacheTexture(GTextureManager[Sky2Texture]);
}


//==========================================================================
//
//  VSky::InitSkyBox
//
//==========================================================================
void VSky::InitSkyBox (VName Name1, VName Name2) {
  int num = CheckSkyboxNumForName(Name1);
  if (num == -1) Host_Error("No skybox '%s'", *Name1);
  skyboxinfo_t &s1info = skyboxinfo[num];

  if (Name2 != NAME_None) {
    num = CheckSkyboxNumForName(Name2);
    if (num == -1) Host_Error("No skybox '%s'", *Name2);
  }
  //skyboxinfo_t &s2info = skyboxinfo[num];

  memset((void *)sky, 0, sizeof(sky));

  sky[0].plane.Set(TVec(-1, 0, 0), -128);
  sky[0].texinfo.saxis = TVec(0, -1.0f, 0);
  sky[0].texinfo.taxis = TVec(0, 0, -1.0f);
  sky[0].texinfo.soffs = 128;
  sky[0].texinfo.toffs = 128;

  sky[0].surf.verts[0].setVec(128, 128, -128);
  sky[0].surf.verts[1].setVec(128, 128, 128);
  sky[0].surf.verts[2].setVec(128, -128, 128);
  sky[0].surf.verts[3].setVec(128, -128, -128);

  sky[1].plane.Set(TVec(0, 1, 0), -128);
  sky[1].texinfo.saxis = TVec(-1.0f, 0, 0);
  sky[1].texinfo.taxis = TVec(0, 0, -1.0f);
  sky[1].texinfo.soffs = 128;
  sky[1].texinfo.toffs = 128;

  sky[1].surf.verts[0].setVec(128, -128, -128);
  sky[1].surf.verts[1].setVec(128, -128, 128);
  sky[1].surf.verts[2].setVec(-128, -128, 128);
  sky[1].surf.verts[3].setVec(-128, -128, -128);

  sky[2].plane.Set(TVec(1, 0, 0), -128);
  sky[2].texinfo.saxis = TVec(0, 1.0f, 0);
  sky[2].texinfo.taxis = TVec(0, 0, -1.0f);
  sky[2].texinfo.soffs = 128;
  sky[2].texinfo.toffs = 128;

  sky[2].surf.verts[0].setVec(-128, -128, -128);
  sky[2].surf.verts[1].setVec(-128, -128, 128);
  sky[2].surf.verts[2].setVec(-128, 128, 128);
  sky[2].surf.verts[3].setVec(-128, 128, -128);

  sky[3].plane.Set(TVec(0, -1, 0), -128);
  sky[3].texinfo.saxis = TVec(1.0f, 0, 0);
  sky[3].texinfo.taxis = TVec(0, 0, -1.0f);
  sky[3].texinfo.soffs = 128;
  sky[3].texinfo.toffs = 128;

  sky[3].surf.verts[0].setVec(-128, 128, -128);
  sky[3].surf.verts[1].setVec(-128, 128, 128);
  sky[3].surf.verts[2].setVec(128, 128, 128);
  sky[3].surf.verts[3].setVec(128, 128, -128);

  sky[4].plane.Set(TVec(0, 0, -1), -128);
  sky[4].texinfo.saxis = TVec(0, -1.0f, 0);
  sky[4].texinfo.taxis = TVec(1.0f, 0, 0);
  sky[4].texinfo.soffs = 128;
  sky[4].texinfo.toffs = 128;

  if (s1info.isgozzo && !s1info.fliptop) {
    //FIXME: fliptop is wrong here!
    sky[4].texinfo.taxis = TVec(0, -1.0f, 0);
    sky[4].texinfo.saxis = TVec(1.0f, 0, 0);
  }

  sky[4].surf.verts[0].setVec(+128.0f, +128.0f, 128);
  sky[4].surf.verts[1].setVec(-128.0f, +128.0f, 128);
  sky[4].surf.verts[2].setVec(-128.0f, -128.0f, 128);
  sky[4].surf.verts[3].setVec(+128.0f, -128.0f, 128);

  sky[5].plane.Set(TVec(0, 0, 1), -128);
  sky[5].texinfo.saxis = TVec(0, -1.0f, 0);
  sky[5].texinfo.taxis = TVec(-1.0f, 0, 0);
  sky[5].texinfo.soffs = 128;
  sky[5].texinfo.toffs = 128;

  if (s1info.isgozzo) {
    sky[5].texinfo.taxis = TVec(0, -1.0f, 0);
    sky[5].texinfo.saxis = -TVec(-1.0f, 0, 0);
  }

  sky[5].surf.verts[0].setVec(128, 128, -128);
  sky[5].surf.verts[1].setVec(128, -128, -128);
  sky[5].surf.verts[2].setVec(-128, -128, -128);
  sky[5].surf.verts[3].setVec(-128, 128, -128);

  NumSkySurfs = 6;
  for (int j = 0; j < 6; ++j) {
    sky[j].texture1 = s1info.surfs[j].texture;
    sky[j].surf.plane = sky[j].plane;
    sky[j].surf.texinfo = &sky[j].texinfo;
    sky[j].surf.count = 4;
    sky[j].surf.Light = 0xffffffff;

    // it is better to have checkers than to segfault
    if (!GTextureManager[sky[j].texture1]) sky[j].texture1 = GTextureManager.DefaultTexture;

    // precache texture
    Drawer->PrecacheTexture(GTextureManager[sky[j].texture1]);

    VTexture *STex = GTextureManager[sky[j].texture1];

    sky[j].surf.extents[0] = STex->GetWidth();
    sky[j].surf.extents[1] = STex->GetHeight();
    sky[j].texinfo.saxis *= STex->GetWidth()/256.0f;
    sky[j].texinfo.soffs *= STex->GetWidth()/256.0f;
    sky[j].texinfo.taxis *= STex->GetHeight()/256.0f;
    sky[j].texinfo.toffs *= STex->GetHeight()/256.0f;
  }
  bIsSkyBox = true;
}


//==========================================================================
//
//  VSky::Init
//
//==========================================================================
void VSky::Init (int Sky1Texture, int Sky2Texture,
                 float Sky1ScrollDelta, float Sky2ScrollDelta,
                 bool DoubleSky, bool ForceNoSkyStretch, bool Flip, bool Lightning)
{
  int Num1 = -1;
  int Num2 = -1;
  // it is better to have checkers than to segfault
  if (!GTextureManager[Sky1Texture]) Sky1Texture = GTextureManager.DefaultTexture;
  if (Sky2Texture >= 0 && !GTextureManager[Sky2Texture]) Sky2Texture = GTextureManager.DefaultTexture;
  VName Name1(NAME_None);
  VName Name2(NAME_None);
  // check if we want to replace old sky with a skybox
  // we can't do this if level is using double sky or it's scrolling
  //GCon->Logf("VSky::Init: t1=%d; t2=%d; double=%s; s1scroll=%f", Sky1Texture, Sky2Texture, (DoubleSky ? "tan" : "ona"), Sky1ScrollDelta);
  if (r_skyboxes && !DoubleSky && !Sky1ScrollDelta) {
    Name1 = GTextureManager[Sky1Texture]->Name;
    Name2 = (Lightning ? GTextureManager[Sky2Texture]->Name : Name1);
    Num1 = CheckSkyboxNumForName(Name1);
    Num2 = CheckSkyboxNumForName(Name2);
    //GCon->Logf("VSky::Init: name1='%s'; name2='%s'; num1=%d; num2=%d", *Name1, *Name2, Num1, Num2);
  }
  if (Num1 != -1 && Num2 != -1) {
    GCon->Logf("VSky:Init: creating skybox (%s:%s)", *Name1, *Name2);
    InitSkyBox(Name1, Name2);
  } else {
    InitOldSky(Sky1Texture, Sky2Texture, Sky1ScrollDelta, Sky2ScrollDelta, DoubleSky, ForceNoSkyStretch, Flip);
  }
  SideTex = Sky1Texture;
  SideFlip = Flip;
}


//==========================================================================
//
//  VSky::Draw
//
//==========================================================================
void VSky::Draw (int ColorMap) {
  if (NumSkySurfs > 0) {
    Drawer->StartSkyPolygons();
    for (int i = 0; i < NumSkySurfs; ++i) {
      Drawer->DrawSkyPolygon(&sky[i].surf, bIsSkyBox,
                             GTextureManager(sky[i].texture1), sky[i].columnOffset1,
                             GTextureManager(sky[i].texture2), sky[i].columnOffset2,
                             ColorMap);
    }
    Drawer->EndSkyPolygons();
  }
}


//==========================================================================
//
//  VRenderLevelShared::InitSky
//
//  Called at level load.
//
//==========================================================================
void VRenderLevelShared::InitSky () {
  if (CurrentSky1Texture == Level->LevelInfo->Sky1Texture &&
      CurrentSky2Texture == Level->LevelInfo->Sky2Texture &&
      CurrentDoubleSky == !!(Level->LevelInfo->LevelInfoFlags&VLevelInfo::LIF_DoubleSky) &&
      CurrentLightning == !!(Level->LevelInfo->LevelInfoFlags&VLevelInfo::LIF_Lightning))
  {
    return;
  }

  CurrentSky1Texture = Level->LevelInfo->Sky1Texture;
  CurrentSky2Texture = Level->LevelInfo->Sky2Texture;
  CurrentDoubleSky = !!(Level->LevelInfo->LevelInfoFlags&VLevelInfo::LIF_DoubleSky);
  CurrentLightning = !!(Level->LevelInfo->LevelInfoFlags&VLevelInfo::LIF_Lightning);

  if (Level->LevelInfo->SkyBox != NAME_None) {
    //GCon->Logf(NAME_Debug, "InitSky:skybox");
    BaseSky.InitSkyBox(Level->LevelInfo->SkyBox, NAME_None);
  } else {
    //GCon->Logf(NAME_Debug, "InitSky:normsky; skt=(%d:%d) <%s>", CurrentSky1Texture, CurrentSky2Texture, *GTextureManager[CurrentSky1Texture]->Name);
    BaseSky.Init(CurrentSky1Texture, CurrentSky2Texture,
                 Level->LevelInfo->Sky1ScrollDelta,
                 Level->LevelInfo->Sky2ScrollDelta, CurrentDoubleSky,
                 !!(Level->LevelInfo->LevelInfoFlags&VLevelInfo::LIF_ForceNoSkyStretch),
                 true, CurrentLightning);
  }
}


//==========================================================================
//
//  VRenderLevelShared::AnimateSky
//
//==========================================================================
void VRenderLevelShared::AnimateSky (float frametime) {
  InitSky();
  if (!(Level->LevelInfo->LevelInfoFlags2&VLevelInfo::LIF2_Frozen)) {
    // update sky column offsets
    for (int i = 0; i < BaseSky.NumSkySurfs; ++i) {
      BaseSky.sky[i].columnOffset1 += BaseSky.sky[i].scrollDelta1*frametime;
      BaseSky.sky[i].columnOffset2 += BaseSky.sky[i].scrollDelta2*frametime;
    }
  }
}
#endif


//==========================================================================
//
//  R_IsAnySkyFlatPlane
//
//==========================================================================
bool R_IsAnySkyFlatPlane (const sec_plane_t *SPlane) {
  return (SPlane->pic == skyflatnum || (SPlane->SkyBox && SPlane->SkyBox->GetSkyBoxAlways()));
}


//==========================================================================
//
//  R_IsSkyFlatPlane
//
//==========================================================================
bool R_IsSkyFlatPlane (const sec_plane_t *SPlane) {
  return (SPlane->pic == skyflatnum);
}


//==========================================================================
//
//  R_IsStackedSectorPlane
//
//==========================================================================
bool R_IsStackedSectorPlane (const sec_plane_t *SPlane) {
  return (SPlane->SkyBox && SPlane->SkyBox->GetSkyBoxAlways());
}


//==========================================================================
//
//  R_IsStrictlySkyFlatPlane
//
//==========================================================================
bool R_IsStrictlySkyFlatPlane (const sec_plane_t *SPlane) {
  return (SPlane->pic == skyflatnum && (!SPlane->SkyBox || !SPlane->SkyBox->GetSkyBoxAlways()));
}
