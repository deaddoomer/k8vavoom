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
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
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
#include "gamedefs.h"
#include "network.h"
#include "cl_local.h"
#include "sv_local.h"


// ////////////////////////////////////////////////////////////////////////// //
enum {
  CMD_Side,
  CMD_Sector,
  CMD_PolyObj,
  CMD_StaticLight,
  CMD_NewLevel,
  CMD_PreRender,
  CMD_Line,
  CMD_CamTex,
  CMD_LevelTrans,
  CMD_BodyQueueTrans,

  CMD_MAX
};


//==========================================================================
//
//  VLevelChannel::VLevelChannel
//
//==========================================================================
VLevelChannel::VLevelChannel (VNetConnection *AConnection, vint32 AIndex, vuint8 AOpenedLocally)
  : VChannel(AConnection, CHANNEL_Player, AIndex, AOpenedLocally)
  , Level(nullptr)
  , Lines(nullptr)
  , Sides(nullptr)
  , Sectors(nullptr)
{
}

//==========================================================================
//
//  VLevelChannel::~VLevelChannel
//
//==========================================================================
VLevelChannel::~VLevelChannel () {
  SetLevel(nullptr);
}


//==========================================================================
//
//  VLevelChannel::SetLevel
//
//==========================================================================
void VLevelChannel::SetLevel (VLevel *ALevel) {
  if (Level) {
    delete[] Lines;
    delete[] Sides;
    delete[] Sectors;
    delete[] PolyObjs;
    Lines = nullptr;
    Sides = nullptr;
    Sectors = nullptr;
    PolyObjs = nullptr;
    CameraTextures.Clear();
    Translations.Clear();
    BodyQueueTrans.Clear();
  }

  Level = ALevel;

  if (Level) {
    Lines = new rep_line_t[Level->NumLines];
    memcpy(Lines, Level->BaseLines, sizeof(rep_line_t)*Level->NumLines);
    Sides = new rep_side_t[Level->NumSides];
    memcpy(Sides, Level->BaseSides, sizeof(rep_side_t)*Level->NumSides);
    Sectors = new rep_sector_t[Level->NumSectors];
    memcpy(Sectors, Level->BaseSectors, sizeof(rep_sector_t)*Level->NumSectors);
    PolyObjs = new rep_polyobj_t[Level->NumPolyObjs];
    memcpy(PolyObjs, Level->BasePolyObjs, sizeof(rep_polyobj_t)*Level->NumPolyObjs);
  }
}


//==========================================================================
//
//  VLevelChannel::SendNewLevel
//
//==========================================================================
void VLevelChannel::SendNewLevel () {
  {
    VMessageOut Msg(this);
    Msg.bReliable = true;
    Msg.WriteInt(CMD_NewLevel/*, CMD_MAX*/);
    VStr MapName = *Level->MapName;
    check(!Msg.IsLoading());
    Msg << svs.serverinfo << MapName;
    Msg.WriteInt(svs.max_clients/*, MAXPLAYERS+1*/);
    Msg.WriteInt(deathmatch/*, 256*/);
    SendMessage(&Msg);
  }

  SendStaticLights();

  {
    VMessageOut Msg(this);
    Msg.bReliable = true;
    Msg.WriteInt(CMD_PreRender/*, CMD_MAX*/);
    SendMessage(&Msg);
  }
}


//==========================================================================
//
//  VLevelChannel::Update
//
//==========================================================================
void VLevelChannel::Update () {
  VMessageOut Msg(this);
  Msg.bReliable = true;

  for (int i = 0; i < Level->NumLines; ++i) {
    line_t *Line = &Level->Lines[i];
    //if (!Connection->SecCheckFatPVS(Line->sector)) continue;

    rep_line_t *RepLine = &Lines[i];
    if (Line->alpha == RepLine->alpha) continue;

    Msg.WriteInt(CMD_Line/*, CMD_MAX*/);
    Msg.WriteInt(i/*, Level->NumLines*/);
    Msg.WriteBit(Line->alpha != RepLine->alpha);
    if (Line->alpha != RepLine->alpha) {
      Msg << Line->alpha;
      Msg.WriteBit(!!(Line->flags&ML_ADDITIVE));
      RepLine->alpha = Line->alpha;
    }
  }

  for (int i = 0; i < Level->NumSides; ++i) {
    side_t *Side = &Level->Sides[i];
    if (!Connection->SecCheckFatPVS(Side->Sector)) continue;

    rep_side_t *RepSide = &Sides[i];
    if (Side->TopTexture == RepSide->TopTexture &&
        Side->BottomTexture == RepSide->BottomTexture &&
        Side->MidTexture == RepSide->MidTexture &&
        Side->TopTextureOffset == RepSide->TopTextureOffset &&
        Side->BotTextureOffset == RepSide->BotTextureOffset &&
        Side->MidTextureOffset == RepSide->MidTextureOffset &&
        Side->TopRowOffset == RepSide->TopRowOffset &&
        Side->BotRowOffset == RepSide->BotRowOffset &&
        Side->MidRowOffset == RepSide->MidRowOffset &&
        Side->Flags == RepSide->Flags &&
        Side->Light == RepSide->Light)
    {
      continue;
    }

    Msg.WriteInt(CMD_Side/*, CMD_MAX*/);
    Msg.WriteInt(i/*, Level->NumSides*/);
    Msg.WriteBit(Side->TopTexture != RepSide->TopTexture);
    if (Side->TopTexture != RepSide->TopTexture) {
      Msg.WriteInt(Side->TopTexture/*, MAX_VUINT16*/);
      RepSide->TopTexture = Side->TopTexture;
    }
    Msg.WriteBit(Side->BottomTexture != RepSide->BottomTexture);
    if (Side->BottomTexture != RepSide->BottomTexture) {
      Msg.WriteInt(Side->BottomTexture/*, MAX_VUINT16*/);
      RepSide->BottomTexture = Side->BottomTexture;
    }
    Msg.WriteBit(Side->MidTexture != RepSide->MidTexture);
    if (Side->MidTexture != RepSide->MidTexture) {
      Msg.WriteInt(Side->MidTexture/*, MAX_VUINT16*/);
      RepSide->MidTexture = Side->MidTexture;
    }
    Msg.WriteBit(Side->TopTextureOffset != RepSide->TopTextureOffset);
    if (Side->TopTextureOffset != RepSide->TopTextureOffset) {
      Msg << Side->TopTextureOffset;
      RepSide->TopTextureOffset = Side->TopTextureOffset;
    }
    Msg.WriteBit(Side->BotTextureOffset != RepSide->BotTextureOffset);
    if (Side->BotTextureOffset != RepSide->BotTextureOffset) {
      Msg << Side->BotTextureOffset;
      RepSide->BotTextureOffset = Side->BotTextureOffset;
    }
    Msg.WriteBit(Side->MidTextureOffset != RepSide->MidTextureOffset);
    if (Side->MidTextureOffset != RepSide->MidTextureOffset) {
      Msg << Side->MidTextureOffset;
      RepSide->MidTextureOffset = Side->MidTextureOffset;
    }
    Msg.WriteBit(Side->TopRowOffset != RepSide->TopRowOffset);
    if (Side->TopRowOffset != RepSide->TopRowOffset) {
      Msg << Side->TopRowOffset;
      RepSide->TopRowOffset = Side->TopRowOffset;
    }
    Msg.WriteBit(Side->BotRowOffset != RepSide->BotRowOffset);
    if (Side->BotRowOffset != RepSide->BotRowOffset) {
      Msg << Side->BotRowOffset;
      RepSide->BotRowOffset = Side->BotRowOffset;
    }
    Msg.WriteBit(Side->MidRowOffset != RepSide->MidRowOffset);
    if (Side->MidRowOffset != RepSide->MidRowOffset) {
      Msg << Side->MidRowOffset;
      RepSide->MidRowOffset = Side->MidRowOffset;
    }
    Msg.WriteBit(Side->Flags != RepSide->Flags);
    if (Side->Flags != RepSide->Flags) {
      Msg.WriteInt(Side->Flags/*, 0x000f*/);
      RepSide->Flags = Side->Flags;
    }
    Msg.WriteBit(Side->Light != RepSide->Light);
    if (Side->Light != RepSide->Light) {
      Msg << Side->Light;
      RepSide->Light = Side->Light;
    }
  }

  for (int i = 0; i < Level->NumSectors; ++i) {
    sector_t *Sec = &Level->Sectors[i];
    if (!Connection->SecCheckFatPVS(Sec) &&
        !(Sec->SectorFlags&sector_t::SF_ExtrafloorSource) &&
        !(Sec->SectorFlags&sector_t::SF_TransferSource))
    {
      continue;
    }

    VEntity *FloorSkyBox = Sec->floor.SkyBox;
    if (FloorSkyBox && !Connection->ObjMap->CanSerialiseObject(FloorSkyBox)) FloorSkyBox = nullptr;

    VEntity *CeilSkyBox = Sec->ceiling.SkyBox;
    if (CeilSkyBox && !Connection->ObjMap->CanSerialiseObject(CeilSkyBox)) CeilSkyBox = nullptr;

    rep_sector_t *RepSec = &Sectors[i];
    bool FloorChanged = RepSec->floor_dist != Sec->floor.dist ||
      mround(RepSec->floor_xoffs) != mround(Sec->floor.xoffs) ||
      mround(RepSec->floor_yoffs) != mround(Sec->floor.yoffs) ||
      RepSec->floor_XScale != Sec->floor.XScale ||
      RepSec->floor_YScale != Sec->floor.YScale ||
      mround(RepSec->floor_Angle) != mround(Sec->floor.Angle) ||
      mround(RepSec->floor_BaseAngle) != mround(Sec->floor.BaseAngle) ||
      mround(RepSec->floor_BaseYOffs) != mround(Sec->floor.BaseYOffs) ||
      RepSec->floor_SkyBox != FloorSkyBox;
    bool CeilChanged = RepSec->ceil_dist != Sec->ceiling.dist ||
      mround(RepSec->ceil_xoffs) != mround(Sec->ceiling.xoffs) ||
      mround(RepSec->ceil_yoffs) != mround(Sec->ceiling.yoffs) ||
      RepSec->ceil_XScale != Sec->ceiling.XScale ||
      RepSec->ceil_YScale != Sec->ceiling.YScale ||
      mround(RepSec->ceil_Angle) != mround(Sec->ceiling.Angle) ||
      mround(RepSec->ceil_BaseAngle) != mround(Sec->ceiling.BaseAngle) ||
      mround(RepSec->ceil_BaseYOffs) != mround(Sec->ceiling.BaseYOffs) ||
      RepSec->ceil_SkyBox != CeilSkyBox;
    bool LightChanged = abs(RepSec->lightlevel-Sec->params.lightlevel) >= 4;
    bool FadeChanged = RepSec->Fade != Sec->params.Fade;
    bool SkyChanged = RepSec->Sky != Sec->Sky;
    bool MirrorChanged = RepSec->floor_MirrorAlpha != Sec->floor.MirrorAlpha ||
      RepSec->ceil_MirrorAlpha != Sec->ceiling.MirrorAlpha;
    if (RepSec->floor_pic == Sec->floor.pic &&
        RepSec->ceil_pic == Sec->ceiling.pic &&
        !FloorChanged && !CeilChanged && !LightChanged && !FadeChanged &&
        !SkyChanged && !MirrorChanged)
    {
      continue;
    }

    Msg.WriteInt(CMD_Sector/*, CMD_MAX*/);
    Msg.WriteInt(i/*, Level->NumSectors*/);
    Msg.WriteBit(RepSec->floor_pic != Sec->floor.pic);
    if (RepSec->floor_pic != Sec->floor.pic) Msg.WriteInt(Sec->floor.pic/*, MAX_VUINT16*/);
    Msg.WriteBit(RepSec->ceil_pic != Sec->ceiling.pic);
    if (RepSec->ceil_pic != Sec->ceiling.pic) Msg.WriteInt(Sec->ceiling.pic/*, MAX_VUINT16*/);
    Msg.WriteBit(FloorChanged);
    if (FloorChanged) {
      Msg.WriteBit(RepSec->floor_dist != Sec->floor.dist);
      if (RepSec->floor_dist != Sec->floor.dist) {
        Msg << Sec->floor.dist;
        Msg << Sec->floor.TexZ;
      }
      Msg.WriteBit(mround(RepSec->floor_xoffs) != mround(Sec->floor.xoffs));
      if (mround(RepSec->floor_xoffs) != mround(Sec->floor.xoffs)) Msg.WriteInt(mround(Sec->floor.xoffs)&63/*, 64*/);
      Msg.WriteBit(mround(RepSec->floor_yoffs) != mround(Sec->floor.yoffs));
      if (mround(RepSec->floor_yoffs) != mround(Sec->floor.yoffs)) Msg.WriteInt(mround(Sec->floor.yoffs)&63/*, 64*/);
      Msg.WriteBit(RepSec->floor_XScale != Sec->floor.XScale);
      if (RepSec->floor_XScale != Sec->floor.XScale) Msg << Sec->floor.XScale;
      Msg.WriteBit(RepSec->floor_YScale != Sec->floor.YScale);
      if (RepSec->floor_YScale != Sec->floor.YScale) Msg << Sec->floor.YScale;
      Msg.WriteBit(mround(RepSec->floor_Angle) != mround(Sec->floor.Angle));
      if (mround(RepSec->floor_Angle) != mround(Sec->floor.Angle)) Msg.WriteInt((int)AngleMod(Sec->floor.Angle)/*, 360*/);
      Msg.WriteBit(mround(RepSec->floor_BaseAngle) != mround(Sec->floor.BaseAngle));
      if (mround(RepSec->floor_BaseAngle) != mround(Sec->floor.BaseAngle)) Msg.WriteInt((int)AngleMod(Sec->floor.BaseAngle)/*, 360*/);
      Msg.WriteBit(mround(RepSec->floor_BaseYOffs) != mround(Sec->floor.BaseYOffs));
      if (mround(RepSec->floor_BaseYOffs) != mround(Sec->floor.BaseYOffs)) Msg.WriteInt(mround(Sec->floor.BaseYOffs)&63/*, 64*/);
      Msg.WriteBit(RepSec->floor_SkyBox != FloorSkyBox);
      if (RepSec->floor_SkyBox != FloorSkyBox) Msg << FloorSkyBox;
    }
    Msg.WriteBit(CeilChanged);
    if (CeilChanged) {
      Msg.WriteBit(RepSec->ceil_dist != Sec->ceiling.dist);
      if (RepSec->ceil_dist != Sec->ceiling.dist) {
        Msg << Sec->ceiling.dist;
        Msg << Sec->ceiling.TexZ;
      }
      Msg.WriteBit(mround(RepSec->ceil_xoffs) != mround(Sec->ceiling.xoffs));
      if (mround(RepSec->ceil_xoffs) != mround(Sec->ceiling.xoffs)) Msg.WriteInt(mround(Sec->ceiling.xoffs)&63/*, 64*/);
      Msg.WriteBit(mround(RepSec->ceil_yoffs) != mround(Sec->ceiling.yoffs));
      if (mround(RepSec->ceil_yoffs) != mround(Sec->ceiling.yoffs)) Msg.WriteInt(mround(Sec->ceiling.yoffs)&63/*, 64*/);
      Msg.WriteBit(RepSec->ceil_XScale != Sec->ceiling.XScale);
      if (RepSec->ceil_XScale != Sec->ceiling.XScale) Msg << Sec->ceiling.XScale;
      Msg.WriteBit(RepSec->ceil_YScale != Sec->ceiling.YScale);
      if (RepSec->ceil_YScale != Sec->ceiling.YScale) Msg << Sec->ceiling.YScale;
      Msg.WriteBit(mround(RepSec->ceil_Angle) != mround(Sec->ceiling.Angle));
      if (mround(RepSec->ceil_Angle) != mround(Sec->ceiling.Angle)) Msg.WriteInt((int)AngleMod(Sec->ceiling.Angle)/*, 360*/);
      Msg.WriteBit(mround(RepSec->ceil_BaseAngle) != mround(Sec->ceiling.BaseAngle));
      if (mround(RepSec->ceil_BaseAngle) != mround(Sec->ceiling.BaseAngle)) Msg.WriteInt((int)AngleMod(Sec->ceiling.BaseAngle)/*, 360*/);
      Msg.WriteBit(mround(RepSec->ceil_BaseYOffs) != mround(Sec->ceiling.BaseYOffs));
      if (mround(RepSec->ceil_BaseYOffs) != mround(Sec->ceiling.BaseYOffs)) Msg.WriteInt(mround(Sec->ceiling.BaseYOffs)&63/*, 64*/);
      Msg.WriteBit(RepSec->ceil_SkyBox != CeilSkyBox);
      if (RepSec->ceil_SkyBox != CeilSkyBox) Msg << CeilSkyBox;
    }
    Msg.WriteBit(LightChanged);
    if (LightChanged) Msg.WriteInt(Sec->params.lightlevel>>2/*, 256*/);
    Msg.WriteBit(FadeChanged);
    if (FadeChanged) Msg << Sec->params.Fade;
    Msg.WriteBit(SkyChanged);
    if (SkyChanged) Msg.WriteInt(Sec->Sky/*, MAX_VUINT16*/);
    Msg.WriteBit(MirrorChanged);
    if (MirrorChanged) {
      Msg << Sec->floor.MirrorAlpha;
      Msg << Sec->ceiling.MirrorAlpha;
    }

    RepSec->floor_pic = Sec->floor.pic;
    RepSec->floor_dist = Sec->floor.dist;
    RepSec->floor_xoffs = Sec->floor.xoffs;
    RepSec->floor_yoffs = Sec->floor.yoffs;
    RepSec->floor_XScale = Sec->floor.XScale;
    RepSec->floor_YScale = Sec->floor.YScale;
    RepSec->floor_Angle = Sec->floor.Angle;
    RepSec->floor_BaseAngle = Sec->floor.BaseAngle;
    RepSec->floor_BaseYOffs = Sec->floor.BaseYOffs;
    RepSec->floor_SkyBox = FloorSkyBox;
    RepSec->floor_MirrorAlpha = Sec->floor.MirrorAlpha;
    RepSec->ceil_pic = Sec->ceiling.pic;
    RepSec->ceil_dist = Sec->ceiling.dist;
    RepSec->ceil_xoffs = Sec->ceiling.xoffs;
    RepSec->ceil_yoffs = Sec->ceiling.yoffs;
    RepSec->ceil_XScale = Sec->ceiling.XScale;
    RepSec->ceil_YScale = Sec->ceiling.YScale;
    RepSec->ceil_Angle = Sec->ceiling.Angle;
    RepSec->ceil_BaseAngle = Sec->ceiling.BaseAngle;
    RepSec->ceil_BaseYOffs = Sec->ceiling.BaseYOffs;
    RepSec->ceil_SkyBox = CeilSkyBox;
    RepSec->ceil_MirrorAlpha = Sec->ceiling.MirrorAlpha;
    RepSec->lightlevel = Sec->params.lightlevel;
    RepSec->Fade = Sec->params.Fade;
  }

  for (int i = 0; i < Level->NumPolyObjs; ++i) {
    polyobj_t *Po = &Level->PolyObjs[i];
    if (!Connection->CheckFatPVS(Po->subsector)) continue;

    rep_polyobj_t *RepPo = &PolyObjs[i];
    if (RepPo->startSpot.x == Po->startSpot.x &&
        RepPo->startSpot.y == Po->startSpot.y &&
        RepPo->angle == Po->angle)
    {
      continue;
    }

    Msg.WriteInt(CMD_PolyObj/*, CMD_MAX*/);
    Msg.WriteInt(i/*, Level->NumPolyObjs*/);
    Msg.WriteBit(RepPo->startSpot.x != Po->startSpot.x);
    if (RepPo->startSpot.x != Po->startSpot.x) Msg << Po->startSpot.x;
    Msg.WriteBit(RepPo->startSpot.y != Po->startSpot.y);
    if (RepPo->startSpot.y != Po->startSpot.y) Msg << Po->startSpot.y;
    Msg.WriteBit(RepPo->angle != Po->angle);
    if (RepPo->angle != Po->angle) Msg << Po->angle;

    RepPo->startSpot = Po->startSpot;
    RepPo->angle = Po->angle;
  }

  for (int i = 0; i < Level->CameraTextures.Num(); ++i) {
    // grow replication array if needed
    if (CameraTextures.Num() == i) {
      VCameraTextureInfo &C = CameraTextures.Alloc();
      C.Camera = nullptr;
      C.TexNum = -1;
      C.FOV = 0;
    }

    VCameraTextureInfo &Cam = Level->CameraTextures[i];
    VCameraTextureInfo &RepCam = CameraTextures[i];
    VEntity *CamEnt = Cam.Camera;
    if (CamEnt && !Connection->ObjMap->CanSerialiseObject(CamEnt)) CamEnt = nullptr;
    if (CamEnt == RepCam.Camera && Cam.TexNum == RepCam.TexNum && Cam.FOV == RepCam.FOV) continue;

    // send message
    Msg.WriteInt(CMD_CamTex/*, CMD_MAX*/);
    Msg.WriteInt(i/*, 0xff*/);
    Connection->ObjMap->SerialiseObject(Msg, *(VObject**)&CamEnt);
    Msg.WriteInt(Cam.TexNum/*, 0xffff*/);
    Msg.WriteInt(Cam.FOV/*, 360*/);

    // update replication info
    RepCam.Camera = CamEnt;
    RepCam.TexNum = Cam.TexNum;
    RepCam.FOV = Cam.FOV;
  }

  for (int i = 0; i < Level->Translations.Num(); ++i) {
    // grow replication array if needed
    if (Translations.Num() == i) Translations.Alloc();
    if (!Level->Translations[i]) continue;
    VTextureTranslation *Tr = Level->Translations[i];
    TArray<VTextureTranslation::VTransCmd> &Rep = Translations[i];
    bool Eq = (Tr->Commands.Num() == Rep.Num());
    if (Eq) {
      for (int j = 0; j < Rep.Num(); ++j) {
        if (memcmp(&Tr->Commands[j], &Rep[j], sizeof(Rep[j]))) {
          Eq = false;
          break;
        }
      }
    }
    if (Eq) continue;

    // send message
    Msg.WriteInt(CMD_LevelTrans/*, CMD_MAX*/);
    Msg.WriteInt(i/*, MAX_LEVEL_TRANSLATIONS*/);
    Msg.WriteInt(Tr->Commands.Num()/*, 0xff*/);
    Rep.SetNum(Tr->Commands.Num());
    for (int j = 0; j < Tr->Commands.Num(); ++j) {
      VTextureTranslation::VTransCmd &C = Tr->Commands[j];
      Msg.WriteInt(C.Type/*, 2*/);
           if (C.Type == 0) Msg << C.Start << C.End << C.R1 << C.R2;
      else if (C.Type == 1) Msg << C.Start << C.End << C.R1 << C.G1 << C.B1 << C.R2 << C.G2 << C.B2;
      Rep[j] = C;
    }
  }

  for (int i = 0; i < Level->BodyQueueTrans.Num(); ++i) {
    // grow replication array if needed
    if (BodyQueueTrans.Num() == i) BodyQueueTrans.Alloc().TranslStart = 0;
    if (!Level->BodyQueueTrans[i]) continue;
    VTextureTranslation *Tr = Level->BodyQueueTrans[i];
    if (!Tr->TranslStart) continue;
    VBodyQueueTrInfo &Rep = BodyQueueTrans[i];
    if (Tr->TranslStart == Rep.TranslStart && Tr->TranslEnd == Rep.TranslEnd && Tr->Colour == Rep.Colour) continue;

    // send message
    Msg.WriteInt(CMD_BodyQueueTrans/*, CMD_MAX*/);
    Msg.WriteInt(i/*, MAX_BODY_QUEUE_TRANSLATIONS*/);
    Msg << Tr->TranslStart << Tr->TranslEnd;
    Msg.WriteInt(Tr->Colour/*, 0x00ffffff*/);
    Rep.TranslStart = Tr->TranslStart;
    Rep.TranslEnd = Tr->TranslEnd;
    Rep.Colour = Tr->Colour;
  }

  if (Msg.GetNumBits()) SendMessage(&Msg);
}


//==========================================================================
//
//  VLevelChannel::SendStaticLights
//
//==========================================================================
void VLevelChannel::SendStaticLights () {
  for (int i = 0; i < Level->NumStaticLights; ++i) {
    rep_light_t &L = Level->StaticLights[i];
    VMessageOut Msg(this);
    Msg.bReliable = true;
    Msg.WriteInt(CMD_StaticLight/*, CMD_MAX*/);
    Msg << L.Origin << L.Radius << L.Colour;
    SendMessage(&Msg);
  }
}


//==========================================================================
//
//  VLevelChannel::ParsePacket
//
//==========================================================================
void VLevelChannel::ParsePacket (VMessageIn &Msg) {
  while (!Msg.AtEnd()) {
    int Cmd = Msg.ReadInt(/*CMD_MAX*/);
    switch (Cmd) {
      case CMD_Side:
        {
          side_t *Side = &Level->Sides[Msg.ReadInt(/*Level->NumSides*/)];
          if (Msg.ReadBit()) Side->TopTexture = Msg.ReadInt(/*MAX_VUINT16*/);
          if (Msg.ReadBit()) Side->BottomTexture = Msg.ReadInt(/*MAX_VUINT16*/);
          if (Msg.ReadBit()) Side->MidTexture = Msg.ReadInt(/*MAX_VUINT16*/);
          if (Msg.ReadBit()) Msg << Side->TopTextureOffset;
          if (Msg.ReadBit()) Msg << Side->BotTextureOffset;
          if (Msg.ReadBit()) Msg << Side->MidTextureOffset;
          if (Msg.ReadBit()) Msg << Side->TopRowOffset;
          if (Msg.ReadBit()) Msg << Side->BotRowOffset;
          if (Msg.ReadBit()) Msg << Side->MidRowOffset;
          if (Msg.ReadBit()) Side->Flags = Msg.ReadInt(/*0x000f*/);
          if (Msg.ReadBit()) Msg << Side->Light;
        }
        break;
      case CMD_Sector:
        {
          sector_t *Sec = &Level->Sectors[Msg.ReadInt(/*Level->NumSectors*/)];
          float PrevFloorDist = Sec->floor.dist;
          float PrevCeilDist = Sec->ceiling.dist;
          if (Msg.ReadBit()) Sec->floor.pic = Msg.ReadInt(/*MAX_VUINT16*/);
          if (Msg.ReadBit()) Sec->ceiling.pic = Msg.ReadInt(/*MAX_VUINT16*/);
          if (Msg.ReadBit()) {
            if (Msg.ReadBit()) {
              Msg << Sec->floor.dist;
              Msg << Sec->floor.TexZ;
            }
            if (Msg.ReadBit()) Sec->floor.xoffs = Msg.ReadInt(/*64*/);
            if (Msg.ReadBit()) Sec->floor.yoffs = Msg.ReadInt(/*64*/);
            if (Msg.ReadBit()) Msg << Sec->floor.XScale;
            if (Msg.ReadBit()) Msg << Sec->floor.YScale;
            if (Msg.ReadBit()) Sec->floor.Angle = Msg.ReadInt(/*360*/);
            if (Msg.ReadBit()) Sec->floor.BaseAngle = Msg.ReadInt(/*360*/);
            if (Msg.ReadBit()) Sec->floor.BaseYOffs = Msg.ReadInt(/*64*/);
            if (Msg.ReadBit()) Msg << Sec->floor.SkyBox;
          }
          if (Msg.ReadBit()) {
            if (Msg.ReadBit()) {
              Msg << Sec->ceiling.dist;
              Msg << Sec->ceiling.TexZ;
            }
            if (Msg.ReadBit()) Sec->ceiling.xoffs = Msg.ReadInt(/*64*/);
            if (Msg.ReadBit()) Sec->ceiling.yoffs = Msg.ReadInt(/*64*/);
            if (Msg.ReadBit()) Msg << Sec->ceiling.XScale;
            if (Msg.ReadBit()) Msg << Sec->ceiling.YScale;
            if (Msg.ReadBit()) Sec->ceiling.Angle = Msg.ReadInt(/*360*/);
            if (Msg.ReadBit()) Sec->ceiling.BaseAngle = Msg.ReadInt(/*360*/);
            if (Msg.ReadBit()) Sec->ceiling.BaseYOffs = Msg.ReadInt(/*64*/);
            if (Msg.ReadBit()) Msg << Sec->ceiling.SkyBox;
          }
          if (Msg.ReadBit()) Sec->params.lightlevel = Msg.ReadInt(/*256*/)<<2;
          if (Msg.ReadBit()) Msg << Sec->params.Fade;
          if (Msg.ReadBit()) Sec->Sky = Msg.ReadInt(/*MAX_VUINT16*/);
          if (Msg.ReadBit()) {
            Msg << Sec->floor.MirrorAlpha;
            Msg << Sec->ceiling.MirrorAlpha;
          }
          if (PrevFloorDist != Sec->floor.dist || PrevCeilDist != Sec->ceiling.dist) CalcSecMinMaxs(Sec);
        }
        break;
      case CMD_PolyObj:
        {
          polyobj_t *Po = &Level->PolyObjs[Msg.ReadInt(/*Level->NumPolyObjs*/)];
          TVec Pos = Po->startSpot;
          if (Msg.ReadBit()) Msg << Pos.x;
          if (Msg.ReadBit()) Msg << Pos.y;
          if (Pos != Po->startSpot) Level->MovePolyobj(Po->tag, Pos.x-Po->startSpot.x, Pos.y-Po->startSpot.y);
          if (Msg.ReadBit()) {
            float a;
            Msg << a;
            Level->RotatePolyobj(Po->tag, a-Po->angle);
          }
        }
        break;
      case CMD_StaticLight:
        {
          TVec Origin;
          float Radius;
          vuint32 Colour;
          Msg << Origin << Radius << Colour;
          Level->RenderData->AddStaticLight(Origin, Radius, Colour);
        }
        break;
      case CMD_NewLevel:
#ifdef CLIENT
        CL_ParseServerInfo(Msg);
#endif
        break;
      case CMD_PreRender:
        Level->RenderData->PreRender();
#ifdef CLIENT
        if (cls.signon) Host_Error("Client_Spawn command already sent");
        if (!UserInfoSent) {
          cl->eventServerSetUserInfo(cls.userinfo);
          UserInfoSent = true;
        }
#endif
        Connection->SendCommand("PreSpawn\n");
        GCmdBuf << "HideConsole\n";
        break;
      case CMD_Line:
        {
          line_t *Line = &Level->Lines[Msg.ReadInt(/*Level->NumLines*/)];
          if (Msg.ReadBit()) {
            Msg << Line->alpha;
            if (Msg.ReadBit()) {
              Line->flags |= ML_ADDITIVE;
            } else {
              Line->flags &= ~ML_ADDITIVE;
            }
          }
        }
        break;
      case CMD_CamTex:
        {
          int i = Msg.ReadInt(/*0xff*/);
          while (Level->CameraTextures.Num() <= i) {
            VCameraTextureInfo &C = Level->CameraTextures.Alloc();
            C.Camera = nullptr;
            C.TexNum = -1;
            C.FOV = 0;
          }
          VCameraTextureInfo &Cam = Level->CameraTextures[i];
          Connection->ObjMap->SerialiseObject(Msg, *(VObject **)&Cam.Camera);
          Cam.TexNum = Msg.ReadInt(/*0xffff*/);
          Cam.FOV = Msg.ReadInt(/*360*/);
        }
        break;
      case CMD_LevelTrans:
        {
          int i = Msg.ReadInt(/*MAX_LEVEL_TRANSLATIONS*/);
          while (Level->Translations.Num() <= i) Level->Translations.Append(nullptr);
          VTextureTranslation *Tr = Level->Translations[i];
          if (!Tr) {
            Tr = new VTextureTranslation;
            Level->Translations[i] = Tr;
          }
          Tr->Clear();
          int Count = Msg.ReadInt(/*0xff*/);
          for (int j = 0; j < Count; ++j) {
            vuint8 Type = Msg.ReadInt(/*2*/);
            if (Type == 0) {
              vuint8 Start;
              vuint8 End;
              vuint8 SrcStart;
              vuint8 SrcEnd;
              Msg << Start << End << SrcStart << SrcEnd;
              Tr->MapToRange(Start, End, SrcStart, SrcEnd);
            } else if (Type == 1) {
              vuint8 Start;
              vuint8 End;
              vuint8 R1;
              vuint8 G1;
              vuint8 B1;
              vuint8 R2;
              vuint8 G2;
              vuint8 B2;
              Msg << Start << End << R1 << G1 << B1 << R2 << G2 << B2;
              Tr->MapToColours(Start, End, R1, G1, B1, R2, G2, B2);
            }
          }
        }
        break;
      case CMD_BodyQueueTrans:
        {
          int i = Msg.ReadInt(/*MAX_BODY_QUEUE_TRANSLATIONS*/);
          while (Level->BodyQueueTrans.Num() <= i) Level->BodyQueueTrans.Append(nullptr);
          VTextureTranslation *Tr = Level->BodyQueueTrans[i];
          if (!Tr) {
            Tr = new VTextureTranslation;
            Level->BodyQueueTrans[i] = Tr;
          }
          Tr->Clear();
          vuint8 TrStart;
          vuint8 TrEnd;
          Msg << TrStart << TrEnd;
          vint32 Col = Msg.ReadInt(/*0x00ffffff*/);
          Tr->BuildPlayerTrans(TrStart, TrEnd, Col);
        }
        break;
      default:
        Sys_Error("Invalid level update command %d", Cmd);
    }
  }
}
